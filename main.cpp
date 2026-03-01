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

static GstPadProbeReturn vectorDBCallback(GstPad* pad, GstPadProbeInfo* info,
                                      gpointer user_data) {
  // Example: check if this is a buffer probe
  if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER) {
    GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER(info);

    if (buffer) {
      g_print("Buffer PTS: %" GST_TIME_FORMAT "\n",
              GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));
    }
  }
  g_print("PROBE CALLED");

  return GST_PAD_PROBE_OK;
}

int main(int argc, char** argv) {
  // Initialize GStreamer
  gst_init(&argc, &argv);

  GstElement* fakeSink = GstFactory::getFakesink("fakeSink");

  // Create source elements
  GstElement* src = GstFactory::getLibcameraSrc("src_0", 0);
  GstElement* srcCaps = GstFactory::getCaps("src_caps", "video/x-raw", "YUY2", CAPTURE_WIDTH, CAPTURE_HEIGHT, CAPTURE_FPS);
  GstElement* srcScaleQ = GstFactory::getQueue("src_scale_q");
  GstElement* srcScale = GstFactory::getVideoScale("src_scale", 1, 2, TRUE, TRUE);
  GstElement* srcConvertQ = GstFactory::getQueue("src_convert_q");
  GstElement* srcConvert = GstFactory::getVideoconvert("src_convert", 3, FALSE);
  GstElement* srcConvertCaps = GstFactory::getCaps("src_convert_caps", "video/x-raw", "RGB", CAPTURE_WIDTH, CAPTURE_HEIGHT, -1, "1/1");
  GstElement* srcVideorate = GstFactory::getVideorate("src_videorate");
  GstElement* srcVideorateCaps = GstFactory::getCaps("src_videorate_caps", "video/x-raw", "", -1, -1, 30);
  GstElement* srcElements[] = {
      src,        srcCaps,        srcScaleQ,    srcScale,         srcConvertQ,
      srcConvert, srcConvertCaps, srcVideorate, srcVideorateCaps,
  };
  // Create detection elements
  GstElement* detectScaleQ = GstFactory::getQueue("detect_scale_q");
  GstElement* detectScale = GstFactory::getVideoScale("detect_scale", 1, 2, TRUE, FALSE);
  GstElement* detectConvertQ = GstFactory::getQueue("detect_convert_q");
  GstElement* detectConvert = GstFactory::getVideoconvert("detect_convert", 3, FALSE);
  GstElement* detectConvertCaps = GstFactory::getCaps("detect_convert_caps", "video/x-raw", "", -1, -1, -1, "1/1");
  GstElement* detectHailonetQ = GstFactory::getQueue("detect_hailonet_q");
  GstElement* detectHailonet = GstFactory::getHailoNet("detect_hailonet", FD_HEF_PATH, "SHARED", 1);
  GstElement* detectHailofilterQ = GstFactory::getQueue("detect_hailofilter_q");
  GstElement* detectHailofilter = GstFactory::getHailoFilter("detect_hailofilter", FD_SO_PATH, FD_FUNC_NAME, FALSE);
  GstElement* detectOutputQ = GstFactory::getQueue("detect_output_q");
  // Create detection wrapper elements
  GstElement* detectWrapInputQ = GstFactory::getQueue("detect_wrap_input_q");
  GstElement* detectWrapHailocropper = GstFactory::getHailoCropper("detect_wrap_crop", CR_SO_PATH, CR_FUNC_NAME);
  GstElement* detectWrapHailoagg = GstFactory::getHailoAggregator("detect_wrap_agg");
  GstElement* detectWrapBypassQ = GstFactory::getQueue("detect_wrap_bypass_q", 0, 30);
  GstElement* detectWrapOutputQ = GstFactory::getQueue("detect_wrap_output_q");
  GstElement* detectWrapElements[] = {
      detectScaleQ,       detectScale,        detectConvertQ,
      detectConvert,      detectConvertCaps,  detectHailonetQ,
      detectHailonet,     detectHailofilterQ, detectHailofilter,
      detectOutputQ,      detectWrapInputQ,   detectWrapHailocropper,
      detectWrapHailoagg, detectWrapBypassQ,  detectWrapOutputQ,
  };
  std::vector<GstElement*> detectWrapCropBranch1 = {detectWrapBypassQ,
                                                    detectWrapHailoagg};
  std::vector<GstElement*> detectWrapCropBranch2 = {
      detectScaleQ,      detectScale,     detectConvertQ,    detectConvert,
      detectConvertCaps, detectHailonetQ, detectHailonet,    detectHailofilterQ,
      detectHailofilter, detectOutputQ,   detectWrapHailoagg};
  // Create tracker elements
  GstElement* hailoTracker = GstFactory::getHailoTracker("hailo_face_tracker");
  // Create face align elements
  GstElement* alignHailofilter = GstFactory::getHailoFilter("face_align_hailofilter", FACE_ALIGN_SO_PATH, "", FALSE, TRUE);
  GstElement* alignOutputQ = GstFactory::getQueue("face_align_q");
  // Create face recognition elements
  GstElement* recogScaleQ = GstFactory::getQueue("recog_scale_q");
  GstElement* recogScale = GstFactory::getVideoScale("recog_scale", 1, 2, TRUE, FALSE);
  GstElement* recogConvertQ = GstFactory::getQueue("recog_convert_q");
  GstElement* recogConvert = GstFactory::getVideoconvert("recog_convert", 3, FALSE);
  GstElement* recogConvertCaps = GstFactory::getCaps("recog_convert_caps", "video/x-raw", "", -1, -1, -1, "1/1");
  GstElement* recogHailonetQ = GstFactory::getQueue("recog_hailonet_q");
  GstElement* recogHailonet = GstFactory::getHailoNet("recog_hailonet", FR_HEF_PATH, "SHARED", 1);
  GstElement* recogHailofilterQ = GstFactory::getQueue("recog_hailofilter_q");
  GstElement* recogHailofilter = GstFactory::getHailoFilter("recog_hailofilter", FR_SO_PATH, FR_FUNC_NAME, FALSE);
  GstElement* recogOutputQ = GstFactory::getQueue("recog_output_q");
  // Create cropper elements
  GstElement* recogWrapInputQ = GstFactory::getQueue("recog_wrap_input_q");
  GstElement* recogWrapHailocropper = GstFactory::getHailoCropper("recog_wrap_hailocropper", CR_SO_PATH, CR_FUNC_NAME);
  GstElement* recogWrapHailoagg = GstFactory::getHailoAggregator("recog_wrap_agg");
  GstElement* recogWrapBypassQ = GstFactory::getQueue("recog_wrap_bypass_q", 0, 20);
  GstElement* recogWrapOutputQ = GstFactory::getQueue("recog_wrap_output_q");
  std::vector<GstElement*> recogWrapCropBranch1 = {recogWrapBypassQ, recogWrapHailoagg};
  std::vector<GstElement*> recogWrapCropBranch2 = {
      recogScaleQ,      recogScale,     recogConvertQ,    recogConvert,
      recogConvertCaps, recogHailonetQ, recogHailonet,    recogHailofilterQ,
      recogHailofilter, recogOutputQ,   recogWrapHailoagg};
  
  GstElement* identityQueue = GstFactory::getQueue("identity_cb_q");
  GstElement* identityCallback = GstFactory::getIdentity("identity_cb");
  // Display pipeline
  GstElement* displayOverlayQ = GstFactory::getQueue("display_overlay_q");
  GstElement* displayOverlay = GstFactory::getHailoOverlay("display_overlay", 5, 2, 8, FALSE, TRUE, TRUE);
  GstElement* displayConvertQ = GstFactory::getQueue("display_convert_q");
  GstElement* displayConvert = GstFactory::getVideoconvert("display_convert", 4, FALSE);
  GstElement* displayQ = GstFactory::getQueue("display_sink_q");
  GstElement* displaySink = GstFactory::getFpsDisplaySink("display_sink", "autovideosink", FALSE, FALSE);
  GstElement* displayElements[] = {
      displayOverlayQ, displayOverlay, displayConvertQ, displayConvert, displayQ, displaySink,
  };

  // for (GstElement* elem : gstElements) {
  //   if (!elem) {
  //     g_printerr(
  //         "GstFactory: failed to create one or more GstElements; cleaning up "
  //         "and "
  //         "exiting.\n");
  //     for (GstElement* ce : gstElements) {
  //       if (ce) gst_object_unref(ce);
  //     }
  //     return 1;
  //   }
  // }

  Pipeline pipeline = Pipeline();

  PipelineElement pe;
  for (GstElement* elem : srcElements) {
    pe = PipelineElement();
    pe.element = elem;
    pipeline.addElement(pe);
  }

  // Detection
  pe = PipelineElement();
  pe.element = detectWrapInputQ;
  pipeline.addElement(pe);

  pe = PipelineElement();
  pe.element = detectWrapHailocropper;
  pe.branches.push_back(detectWrapCropBranch1);
  pe.branches.push_back(detectWrapCropBranch2);
  pipeline.addElement(pe);

  pe = PipelineElement();
  pe.element = detectWrapOutputQ;
  pipeline.addElement(pe);

  // Recognition
  pe = PipelineElement();
  pe.element = recogWrapInputQ;
  pipeline.addElement(pe);

  pe = PipelineElement();
  pe.element = recogWrapHailocropper;
  pe.branches.push_back(recogWrapCropBranch1);
  pe.branches.push_back(recogWrapCropBranch2);
  pipeline.addElement(pe);

  pe = PipelineElement();
  pe.element = recogWrapOutputQ;
  pipeline.addElement(pe);
 
  // Callback probe
  pe = PipelineElement();
  pe.element = identityQueue;
  pipeline.addElement(pe);
  pe = PipelineElement();
  pe.element = identityCallback;
  pipeline.addElement(pe);
 
  for (int i = 0; i < 5; ++i) {
    pe = PipelineElement();
    pe.element = displayElements[i];
    pipeline.addElement(pe);
  }

  // Display elements
  pe = PipelineElement();
  pe.element = fakeSink;
  pipeline.addElement(pe);

  GstElement* p = pipeline.construct();
  if (!p) {
    g_printerr("Failed to construct pipeline\n");
    return 1;
  }

  GstPad* identitySrcPad = gst_element_get_static_pad(identityCallback, "src");
  gst_pad_add_probe(identitySrcPad, GstPadProbeType::GST_PAD_PROBE_TYPE_BUFFER, vectorDBCallback, nullptr, nullptr);

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