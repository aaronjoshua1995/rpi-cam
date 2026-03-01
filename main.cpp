#include <gst/gst.h>
#include <gst/video/video.h>  // for GST_VIDEO_FLIP_METHOD_*

#include <memory>

#include "Pipeline.h"
#include "GstFactory.h"

const int CAPTURE_FPS = 30;
const int CAPTURE_WIDTH = 800;
const int CAPTURE_HEIGHT = 600;

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
  GstElement* source = GstFactory::getLibcameraSrc("src_0", 0);
  GstElement* caps = GstFactory::getCaps("src_caps", "video/x-raw", "YUY2", CAPTURE_WIDTH, CAPTURE_HEIGHT, CAPTURE_FPS);
  GstElement* preFlipQ = GstFactory::getQueue("pre_flip_q", 30);
  GstElement* videoflip = GstFactory::getVideoFlip("videoflip", GST_VIDEO_ORIENTATION_HORIZ);
  GstElement* hailoPreConvertQ = GstFactory::getQueue("hailo_pre_convert_q", 30);
  GstElement *videoconvert = GstFactory::getVideoconvert("videoconvert", 2, FALSE);
  // Face detection pipeline elements
  GstElement* fdHailonet = GstFactory::getHailoNet("fd_hailonet", FD_HEF_PATH, 1, VDEVICE_KEY);
  GstElement* fdPostQ = GstFactory::getQueue("fd_post_q", 30);
  GstElement* fdHailofilter = GstFactory::getHailoFilter("fd_hailofilter", FD_SO_PATH, FD_FUNC_NAME, FALSE);
  // Detector pipeline elements
  GstElement* dPreQ = GstFactory::getQueue("d_pre_q", 30);
  GstElement* dTee = GstFactory::getTee("t");
  GstElement* dHailomuxer = GstFactory::getHailoMuxer("hmux");
  GstElement* dBypassQ = GstFactory::getQueue("d_bypass_q", 30, 0);
  GstElement* dVideoscale = GstFactory::getVideoScale("d_face_videoscale", 0, 2, FALSE, FALSE);
  GstElement* dCaps = GstFactory::getCaps("d_caps", "video/x-raw", "", -1, -1, -1, "1/1");
  GstElement* dPreFdInferQ = GstFactory::getQueue("d_pre_fd_infer_q", 30, 0);
  GstElement* dPostFilterQ = GstFactory::getQueue("d_post_filter_q", 30, 0);
  // Face tracker pipeline elements
  GstElement* frPreQ = GstFactory::getQueue("fr_pre_q", 30);
  GstElement* frHailotracker = GstFactory::getHailoTracker("fr_hailotracker");
  // Cropper pipeline elements
  GstElement* crPreQ = GstFactory::getQueue("cr_pre_q", 30);
  GstElement* crHailocropper = GstFactory::getHailoCropper("fr_cropper", CR_SO_PATH, CR_FUNC_NAME);
  GstElement* crPrecrAggegrator = GstFactory::getQueue("cr_pre_agg_q", 30, 0);
  GstElement* crAggegrator = GstFactory::getHailoAggregator("fr_agg");
  GstElement* crPostcrAggegrator = GstFactory::getQueue("cr_post_agg_q", 30, 0);
  GstElement* crBypassQ = GstFactory::getQueue("cr_bypass_q", 30, 0);
  // Align pipeline elements
  GstElement* alPreQ = GstFactory::getQueue("al_pre_q", 30);
  GstElement* alHailofilter = GstFactory::getHailoFilter("al_hailofilter", FACE_ALIGN_SO_PATH, "face_align", FALSE); 
  // Recognition pipeline elements
  GstElement* rPreQ = GstFactory::getQueue("r_pre_q", 30);
  GstElement* rHailonet = GstFactory::getHailoNet("r_hailonet", FR_HEF_PATH, 1, VDEVICE_KEY);
  GstElement* rPreAggQ = GstFactory::getQueue("recognition_pre_agg_q", 30, 0);
  GstElement* rPostAggQ = GstFactory::getQueue("recognition_post_agg_q", 30, 0);
  GstElement* rHailoFilter = GstFactory::getHailoFilter("face_recognition_hailofilter", FR_SO_PATH, FR_FUNC_NAME, FALSE);
  // Gallery pipeline
  GstElement* gaPreQ = GstFactory::getQueue("ga_pre_q", 30);
  GstElement* gaHailoGallery = GstFactory::getHailoGallery("gallery", GALLERY_PATH, 0.4, 20, 1, TRUE);
  // Drawing pipeline
  GstElement* drPreQ = GstFactory::getQueue("dr_pre_q", 30);
  GstElement* drHailoOverlay = GstFactory::getHailoOverlay("hailo_overlay", 5, 2, 8, FALSE, TRUE, TRUE);
  GstElement* drPreIdentityQ = GstFactory::getQueue("dr_pre_identity_q", 30);
  GstElement* drIdentity = GstFactory::getIdentity("identity_callback");
  GstElement* drPostQ = GstFactory::getQueue("dr_post_q", 30);
  // Sink pipeline
  GstElement* sinkConvert = GstFactory::getVideoconvert("display_videoconvert", 4, FALSE);
  GstElement* sinkQ = GstFactory::getQueue("sink_q", 30, 0);
  GstElement* sink = GstFactory::getFpsDisplaySink("videosink", "xvimagesink", FALSE, FALSE);
  GstElement* fakeSink = GstFactory::getFakesink("fakeSink");

  GstElement* gstElements[] = {
      source,       caps,         preFlipQ,       videoflip,
      fdHailonet,   fdPostQ,      fdHailofilter,  dTee,
      dHailomuxer,  dBypassQ,     dVideoscale,    dCaps,
      dPreFdInferQ, dPostFilterQ, frHailotracker, crHailocropper,
      crAggegrator, crBypassQ,    alHailofilter,  rHailonet,
      rPreAggQ,     rHailoFilter, rPostAggQ,      rPreQ};
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
  std::vector<GstElement*> srcPipeline = {
      source,           caps,         preFlipQ, videoflip,
      hailoPreConvertQ, videoconvert, dPreQ};
  for (GstElement* elem : srcPipeline) {
    pe = PipelineElement();
    pe.element = elem;
    pipeline.addElement(pe);
  }

  std::vector<GstElement*> dBypassBranch = {dBypassQ, dHailomuxer};
  std::vector<GstElement*> dInferBranch = {
      dVideoscale, dCaps,         dPreFdInferQ, fdHailonet,
      fdPostQ,     fdHailofilter, dPostFilterQ, dHailomuxer};
  pe = PipelineElement();
  pe.element = dTee;
  pe.branches.push_back(dBypassBranch);
  pe.branches.push_back(dInferBranch);
  pipeline.addElement(pe);

  std::vector<GstElement*> fTrackerPipeline = {frPreQ, frHailotracker, crPreQ};
  for (GstElement* elem : fTrackerPipeline) {
    PipelineElement pe = PipelineElement();
    pe.element = elem;
    pipeline.addElement(pe);
  }

  pe = PipelineElement();
  pe.element = fakeSink;
  pipeline.addElement(pe);

  // std::vector<GstElement*> crBypassBranch = {crBypassQ, crAggegrator};
  // std::vector<GstElement*> crPipeline = {
  //   // alPreQ,
  //   // alHailofilter,
  //   rPreQ,
  //   rHailonet,
  //   rPreAggQ,
  //   rHailoFilter,
  //   rPostAggQ,
  //   crAggegrator
  // };
  // pe = PipelineElement();
  // pe.element = crHailocropper;
  // pe.branches.push_back(crBypassBranch);
  // pe.branches.push_back(crPipeline);
  // pipeline.addElement(pe);
  // std::vector<GstElement*> sinkPipeline = {gaPreQ,         fakeSink};

  // std::vector<GstElement*> sinkPipeline = {gaPreQ,         gaHailoGallery,
  //                                          drPreQ,         drHailoOverlay,
  //                                          drPreIdentityQ, drIdentity,
  //                                          drPostQ,        sinkConvert,
  //                                          sinkQ,          sink};
  // for (GstElement* elem : sinkPipeline) {
  //   PipelineElement pe = PipelineElement();
  //   pe.element = elem;
  //   pipeline.addElement(pe);
  // }

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