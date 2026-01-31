#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <memory>

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
        gst_util_uint64_scale_int(1, GST_SECOND, 30);

    gst_app_src_push_buffer(GST_APP_SRC(stream_appsrc), buffer);
}

static GstFlowReturn pullSample(GstAppSink *appsink, gpointer user_data) {
    // GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    GstSample *sample = gst_app_sink_pull_sample(appsink);
    if (!sample)
        return GST_FLOW_ERROR;

    // Process sample here
    GstCaps *caps = gst_sample_get_caps(sample);
    GstStructure *s = gst_caps_get_structure(caps, 0);

    int width, height;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);

    // Pull the image data
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;

    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    // OpenCV
    cv::Mat frame(height, width, CV_8UC3, map.data);

    // Convert to grayscale
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_RGB2GRAY);

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

    GstElement *appsrc      = gst_element_factory_make("appsrc", "stream_appsrc");
    GstElement *videoconvert= gst_element_factory_make("videoconvert", nullptr);
    GstElement *encoder     = gst_element_factory_make("x264enc", nullptr);
    GstElement *pay         = gst_element_factory_make("rtph264pay", nullptr);
    GstElement *udpsink     = gst_element_factory_make("udpsink", nullptr);

    if (!pipeline || !appsrc || !videoconvert || !encoder || !pay || !udpsink) {
        g_printerr("Failed to create stream elements\n");
        return nullptr;
    }

    // Configure appsrc
    GstCaps *caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB",
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "framerate", GST_TYPE_FRACTION, fps, 1,
        nullptr);

    g_object_set(appsrc,
        "caps", caps,
        "is-live", TRUE,
        "format", GST_FORMAT_TIME,
        "do-timestamp", TRUE,
        nullptr);
    gst_caps_unref(caps);

    g_object_set(encoder,
        "tune", 0x00000004, // zerolatency
        "speed-preset", 1,  // ultrafast
        "bitrate", 800,
        nullptr);

    g_object_set(udpsink,
        "host", "192.168.1.81",  // CHANGE ME
        "port", 5000,
        nullptr);

    gst_bin_add_many(GST_BIN(pipeline),
        appsrc, videoconvert, encoder, pay, udpsink, nullptr);

    if (!gst_element_link_many(
            appsrc, videoconvert, encoder, pay, udpsink, nullptr)) {
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
    GstElement *source = gst_element_factory_make("libcamerasrc", "camerasrc");
    GstElement *capsfilter = gst_element_factory_make("capsfilter", "maincaps");
    GstElement *appsink = gst_element_factory_make("appsink", "mainappsink");
    // GstElement *fakesink = gst_element_factory_make("fakesink", "fakesink");

    if (!source || !capsfilter || !appsink) {
        g_printerr("Failed to create elements.\n");
        gst_object_unref(pipeline);
        return nullptr;
    }

    // Preferred format for v4l2h264enc is NV12. Keep resolution/framerate reasonable for Pi Zero 2 W.
    GstCaps *caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB",
        "width", G_TYPE_INT, 640,
        "height", G_TYPE_INT, 480,
        "framerate", GST_TYPE_FRACTION, 30, 1,
        nullptr);
    g_object_set(capsfilter, "caps", caps, nullptr);
    gst_caps_unref(caps);

    // Populate the pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, capsfilter, appsink, nullptr);
    if (!gst_element_link_many(source, capsfilter, appsink, nullptr)) {
        g_printerr("Failed to link main elements.\n");
        gst_object_unref(pipeline);
        return nullptr;
    }

    gst_app_sink_set_emit_signals(GST_APP_SINK(appsink), TRUE);
    gst_app_sink_set_drop(GST_APP_SINK(appsink), TRUE);
    gst_app_sink_set_max_buffers(GST_APP_SINK(appsink), 1);
    // g_signal_connect(appsink, "new-sample", G_CALLBACK(pullSample), &face_cascade);
    g_signal_connect(appsink, "new-sample", G_CALLBACK(pullSample), nullptr);

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

    // Create pipeline
    std::unique_ptr<GstElement, void(*)(GstElement*)> pipeline(initPipeline(),
        [](GstElement *p){ if (p) gst_object_unref(p); });

    if (enable_streaming) {
        stream_pipeline = initStreamPipeline(640, 480, 30);
        if (!stream_pipeline) {
            g_printerr("Failed to init stream pipeline\n");
            return -1;
        }

        gst_element_set_state(stream_pipeline, GST_STATE_PLAYING);
        g_print("Streaming pipeline PLAYING\n");
    }

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