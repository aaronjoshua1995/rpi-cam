#include <gst/gst.h>
#include <gst/video/video.h>  // for GST_VIDEO_FLIP_METHOD_*

#include <memory>

#include "Pipeline.h"
#include "GstFactory.h"

const int CAPTURE_FPS = 30;
const int CAPTURE_WIDTH = 640;
const int CAPTURE_HEIGHT = 640;

// Base directories
const std::string MODEL_DIR        = "/home/pi/rpi-cam/models";
const std::string POST_PROCESS_DIR = "/usr/lib/aarch64-linux-gnu/hailo/tappas/post_processes";
// Face detection
const std::string FD_HEF_PATH  = MODEL_DIR + "/retinaface_mobilenet_v1.hef";
const std::string FD_SO_PATH   = POST_PROCESS_DIR + "/libface_detection_post.so";
const std::string FD_FUNC_NAME = "retinaface";
// Face recognition
const std::string FR_HEF_PATH  = MODEL_DIR + "/arcface_mobilefacenet.hef";
const std::string FR_SO_PATH   = POST_PROCESS_DIR + "/libface_recognition_post.so";
const std::string FR_FUNC_NAME = "arcface_nv12";

const std::string GALLERY_PATH  = "/home/pi/work/ai/exp/face_recognition_local_gallery_rgba.json";
const std::string CR_SO_PATH    = POST_PROCESS_DIR + "/cropping_algorithms/libvms_croppers.so";
const std::string CR_FUNC_NAME  = "face_recognition";
const std::string FACE_ALIGN_SO_PATH = "/usr/local/hailo/resources/so/libvms_face_align.so";
const int VDEVICE_KEY = 1;

int main(int argc, char** argv) {
  // Initialize GStreamer
  gst_init(&argc, &argv);

  // Create pipeline elements
  GstElement* src = GstFactory::getLibcameraSrc("src_0", 0);
  GstElement* srcCaps = GstFactory::getCaps("src_caps", "video/x-raw", "YUY2", CAPTURE_WIDTH, CAPTURE_HEIGHT, CAPTURE_FPS);
  GstElement* srcScaleQ = GstFactory::getQueue("src_scale_q");
  GstElement* srcScale = GstFactory::getVideoScale("src_scale", 1, 2, TRUE, TRUE);
  GstElement* srcConvertQ = GstFactory::getQueue("src_convert_q");
  GstElement* srcConvert = GstFactory::getVideoconvert("src_convert", 3, FALSE);
  GstElement* srcConvertCaps = GstFactory::getCaps("src_convert_caps", "video/x-raw", "RGB", CAPTURE_WIDTH, CAPTURE_HEIGHT, -1, "1/1");
  GstElement* srcVideorate = GstFactory::getVideorate("src_videorate");
  GstElement* srcVideorateCaps = GstFactory::getCaps("src_videorate_caps", "video/x-raw", "", -1, -1, 30);
  GstElement* fakeSink = GstFactory::getFakesink("fakeSink");

  GstElement* gstElements[] = {
    src,
    srcCaps,
    srcScaleQ,
    srcScale,
    srcConvertQ,
    srcConvert,
    srcConvertCaps,
    srcVideorate,
    srcVideorateCaps,
  };
  for (GstElement* elem : gstElements) {
    if (!elem) {
      g_printerr(
          "GstFactory: failed to create one or more GstElements; cleaning up "
          "and "
          "exiting.\n");
      for (GstElement* ce : gstElements) {
        if (ce) gst_object_unref(ce);
      }
      return 1;
    }
  }

  Pipeline pipeline = Pipeline();
  PipelineElement pe;
  GstElement* srcPipeline[] = {
    src,
    srcCaps,
    srcScaleQ,
    srcScale,
    srcConvertQ,
    srcConvert,
    srcConvertCaps,
    srcVideorate,
    srcVideorateCaps,
  };
  for (GstElement* elem : srcPipeline) {
    pe = PipelineElement();
    pe.element = elem;
    pipeline.addElement(pe);
  }
  pe = PipelineElement();
  pe.element = fakeSink;
  pipeline.addElement(pe);

  GstElement* p = pipeline.construct();
  if (!p) {
    g_printerr("Failed to construct pipeline\n");
    return 1;
  }

  GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
  if (!loop) {
    g_printerr("Failed to create GMainLoop.\n");
    return -1;
  }

  GstStateChangeReturn ret = gst_element_set_state(p, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set pipeline to PLAYING");
    gst_object_unref(p);
    return 1;
  }
  g_print("Pipeline set to PLAYING successfully.\n");

  g_main_loop_run(loop);

  gst_element_set_state(p, GST_STATE_NULL);
  gst_object_unref(p);
  gst_deinit();
}