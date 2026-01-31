#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <memory>

cv::CascadeClassifier face_cascade;

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
    g_print("Faces detected: %zu\n", faces.size());

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

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
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