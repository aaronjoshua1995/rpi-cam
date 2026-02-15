#include <gst/gst.h>
#include <gst/video/video.h>  // for GST_VIDEO_FLIP_METHOD_*

#include <memory>

#include "Pipeline.h"
#include "PipelineConstructor.h"

const int CAPTURE_FPS = 30;
const int CAPTURE_WIDTH = 640;
const int CAPTURE_HEIGHT = 480;

static GstElement* initPipeline() {
  Pipeline pipeline;
  PipelineConstructor constructor;
  g_print("Created pipe and construct\n");

  //   constructor->get_libcamerasrc_element(pipeline, "src", 640, 480, 30,
  //   "YUV2",
  //                                        GST_VIDEO_ORIENTATION_IDENTITY);
  pipeline.addElement(constructor.get_videotestsrc_element("testsrc"));
  pipeline.addElement(constructor.get_fakesink_element("fakesink"));
  g_print("Created elements\n");

  // Populate the pipeline
  GstElement* p = gst_pipeline_new("main_pipeline");
  std::vector<GstElement*> elements = pipeline.getElements();
  for (auto element : elements) {
    gst_bin_add(GST_BIN(p), element);
  }

  for (size_t i = 0; i < elements.size() - 1; ++i) {
    GstElement* src = elements[i];
    GstElement* dst = elements[i + 1];
    if (!gst_element_link(src, dst)) {
      g_printerr("Failed to link %s -> %s\n", 
                 GST_ELEMENT_NAME(src),
                 GST_ELEMENT_NAME(dst));
      // Optionally clean up and exit
      for (auto e : elements) {
        gst_object_unref(e);
      }
      break;
    }
  }

  g_print("Created main pipeline\n");
  return p;
}

int main(int argc, char** argv) {
  // Initialize GStreamer
  gst_init(&argc, &argv);

  // Create pipeline
  std::unique_ptr<GstElement, void (*)(GstElement*)> pipeline(
      initPipeline(), [](GstElement* p) {
        if (p) gst_object_unref(p);
      });

  GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
  if (!loop) {
    g_printerr("Failed to create GMainLoop.\n");
    return -1;
  }

  GstStateChangeReturn ret =
      gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set pipeline to PLAYING\n");
    g_main_loop_unref(loop);
    return -1;
  }
  g_print("Pipeline set to PLAYING\n");

  g_main_loop_run(loop);
}