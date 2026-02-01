#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>   // for GST_VIDEO_FLIP_METHOD_*
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <memory>

const int CAPTURE_FPS = 30;
const int CAPTURE_WIDTH = 640;
const int CAPTURE_HEIGHT = 480;

static GstElement *stream_pipeline = nullptr;
static GstElement *stream_appsrc   = nullptr;
static gboolean    enable_streaming = TRUE;  // ← toggle this

cv::CascadeClassifier face_cascade;

static inline void pushFrameToStream(const cv::Mat &frame)
{
    if (!enable_streaming || !stream_appsrc)
        return;

    const gsize size = frame.total() * frame.elemSize();
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, size, nullptr);

    gst_buffer_fill(buffer, 0, frame.data, size);

    GST_BUFFER_PTS(buffer) = gst_util_get_timestamp();
    GST_BUFFER_DURATION(buffer) =
        gst_util_uint64_scale_int(1, GST_SECOND, CAPTURE_FPS);

    gst_app_src_push_buffer(GST_APP_SRC(stream_appsrc), buffer);
}

static GstFlowReturn pullSample(GstAppSink *appsink, gpointer user_data) {
    // GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    GstSample *sample = gst_app_sink_pull_sample(appsink);
    if (!sample)
        return GST_FLOW_ERROR;

    // g_print("Pulled sample");

    // Process sample here
    GstCaps *caps = gst_sample_get_caps(sample);
    if (!caps) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;  // or ERROR, your choice
    }
    // g_print("Retrieved caps from sample");


    GstStructure *s = gst_caps_get_structure(caps, 0);
    if (!s) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }
    // g_print("Retrieved caps from sample");

    int width, height;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);
    // g_print("Retrieved dimensions from sample");

    // Pull the image data
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    // g_print("Retrieved sample buffer");

    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }
    // g_print("Mapped sample buffer");

    // OpenCV
    cv::Mat frame_ro(height, width, CV_8UC3, map.data);
    cv::Mat frame = frame_ro.clone();   // OWNED memory
    // g_print("Converted map data to frame");

    // Convert to grayscale
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    // g_print("Converted to grayscale");

    // Detect faces
    std::vector<cv::Rect> faces;
    face_cascade.detectMultiScale(
        gray,
        faces,
        1.2,        // scale factor
        5,          // min neighbors
        0,
        cv::Size(40, 40)
    );
    // g_print("Faces detected: %zu\n", faces.size());

    // Draw rectangles around the faces (Annotate)
    int i = 0;
    for (const auto &r : faces) {
        cv::rectangle(
            frame,
            r,
            cv::Scalar(0, 255, 0),
            2
        );
        i++;
    }
    pushFrameToStream(frame);

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

static GstElement *initStreamPipeline(int width, int height, int fps) {
    GstElement *pipeline = gst_pipeline_new("stream_pipeline");

    GstElement *appsrc       = gst_element_factory_make("appsrc", "stream_appsrc");
    GstElement *videoconvert = gst_element_factory_make("videoconvert", nullptr);
    GstElement *capsfilter   = gst_element_factory_make("capsfilter", nullptr);
    GstElement *encoder      = gst_element_factory_make("v4l2h264enc", nullptr);
    GstElement *parser       = gst_element_factory_make("h264parse", nullptr);
    GstElement *pay          = gst_element_factory_make("rtph264pay", nullptr);
    GstElement *udpsink      = gst_element_factory_make("udpsink", nullptr);

    if (!pipeline || !appsrc || !videoconvert || !capsfilter ||
        !encoder || !parser || !pay || !udpsink) {
        g_printerr("Failed to create stream elements\n");
        return nullptr;
    }

    /* ---------- appsrc caps (must match OpenCV frames) ---------- */
    GstCaps *src_caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "BGR",
        "width", G_TYPE_INT, CAPTURE_WIDTH,
        "height", G_TYPE_INT, CAPTURE_HEIGHT,
        "framerate", GST_TYPE_FRACTION, CAPTURE_FPS, 1,
        nullptr);

    g_object_set(appsrc,
        "caps", src_caps,
        "is-live", TRUE,
        "format", GST_FORMAT_TIME,
        "do-timestamp", TRUE,
        nullptr);
    gst_caps_unref(src_caps);

    /* ---------- Force I420 for x264enc ---------- */
    GstCaps *i420_caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "NV12",
        "width", G_TYPE_INT, CAPTURE_WIDTH,
        "height", G_TYPE_INT, CAPTURE_HEIGHT,
        "framerate", GST_TYPE_FRACTION, CAPTURE_FPS, 1,
        nullptr);

    g_object_set(capsfilter, "caps", i420_caps, nullptr);
    gst_caps_unref(i420_caps);

    /* ---------- Encoder ---------- */
    g_object_set(G_OBJECT(encoder),
        "extra-controls", "controls,video_bitrate=2000000",
        NULL);

    // g_object_set(encoder,
    //     "tune", 0x00000004,      // zerolatency
    //     "speed-preset", 1,       // ultrafast
    //     "key-int-max", fps,      // one keyframe per second
    //     "bitrate", 800,
    //     nullptr);

    /* ---------- RTP payloader ---------- */
    g_object_set(pay,
        "config-interval", 1,    // VERY IMPORTANT
        "pt", 96,
        nullptr);

    /* ---------- UDP sink ---------- */
    g_object_set(udpsink,
        "host", "192.168.1.70",  // CHANGE ME
        "port", 5000,
        "sync", FALSE,
        nullptr);

    gst_bin_add_many(GST_BIN(pipeline),
        appsrc,
        videoconvert,
        capsfilter,
        encoder,
        parser,
        pay,
        udpsink,
        nullptr);

    if (!gst_element_link_many(
            appsrc,
            videoconvert,
            capsfilter,
            encoder,
            parser,
            pay,
            udpsink,
            nullptr)) {
        g_printerr("Failed to link stream pipeline\n");
        gst_object_unref(pipeline);
        return nullptr;
    }

    stream_appsrc = appsrc;
    return pipeline;
}

static GstElement *initPipeline() {
    // Create the pipeline
    GstElement *pipeline = gst_pipeline_new("main_pipeline");
    if (!pipeline) {
        g_printerr("Failed to create pipeline.\n");
        return nullptr;
    }

    // Create elements to populate the pipeline
    GstElement *source = gst_element_factory_make("libcamerasrc", "maincamerasrc");
    GstElement *videoflip = gst_element_factory_make("videoflip", "mainvideoflip");
    GstElement *videoconvert = gst_element_factory_make("videoconvert", "mainvideoconvert");
    GstElement *capsfilter = gst_element_factory_make("capsfilter", "maincaps");
    GstElement *appsink = gst_element_factory_make("appsink", "mainappsink");
    // GstElement *fakesink = gst_element_factory_make("fakesink", "fakesink");

    if (!source || !videoflip || !videoconvert || !capsfilter || !appsink) {
        g_printerr("Failed to create elements.\n");
        gst_object_unref(pipeline);
        return nullptr;
    }

    g_object_set(videoflip, 
        "method", GST_VIDEO_ORIENTATION_180,
        nullptr);

    // Preferred format for v4l2h264enc is NV12. Keep resolution/framerate reasonable for Pi Zero 2 W.
    GstCaps *caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "BGR",
        "width", G_TYPE_INT, CAPTURE_WIDTH,
        "height", G_TYPE_INT, CAPTURE_HEIGHT,
        "framerate", GST_TYPE_FRACTION, CAPTURE_FPS, 1,
        nullptr);
    g_object_set(capsfilter, "caps", caps, nullptr);
    gst_caps_unref(caps);

    // Populate the pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, videoflip, videoconvert, capsfilter, appsink, nullptr);
    if (!gst_element_link_many(source, videoflip, videoconvert, capsfilter, appsink, nullptr)) {
        g_printerr("Failed to link main elements.\n");
        gst_object_unref(pipeline);
        return nullptr;
    }

    gst_app_sink_set_emit_signals(GST_APP_SINK(appsink), TRUE);
    gst_app_sink_set_drop(GST_APP_SINK(appsink), TRUE);
    gst_app_sink_set_max_buffers(GST_APP_SINK(appsink), 1);
    // g_signal_connect(appsink, "new-sample", G_CALLBACK(pullSample), &face_cascade);

    GstAppSinkCallbacks callbacks = {
        nullptr,
        nullptr,
        pullSample
    };
    gst_app_sink_set_callbacks(
        GST_APP_SINK(appsink),
        &callbacks,
        nullptr,
        nullptr
    );
    // g_signal_connect(appsink, "new-sample", G_CALLBACK(pullSample), nullptr);

    g_print("Created main pipeline\n");

    return pipeline;
}


int main(int argc, char **argv) {
    if (!face_cascade.load(
        "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml"
    )) {
        g_printerr("Failed to load face cascade\n");
        return -1;
    }

    // Initialize GStreamer
    gst_init(&argc, &argv);

    if (enable_streaming) {
        stream_pipeline = initStreamPipeline(CAPTURE_WIDTH, CAPTURE_HEIGHT, CAPTURE_FPS);
        if (!stream_pipeline) {
            g_printerr("Failed to init stream pipeline\n");
            return -1;
        }

        gst_element_set_state(stream_pipeline, GST_STATE_PLAYING);
        g_print("Streaming pipeline PLAYING\n");
    }

    // Create pipeline
    std::unique_ptr<GstElement, void(*)(GstElement*)> pipeline(initPipeline(),
        [](GstElement *p){ if (p) gst_object_unref(p); });

    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    if (!loop) {
        g_printerr("Failed to create GMainLoop.\n");
        return -1;
    }

    GstStateChangeReturn ret = gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set pipeline to PLAYING\n");
        g_main_loop_unref(loop);
        return -1;
    }
    g_print("Pipeline set to PLAYING\n");

    g_main_loop_run(loop);

}