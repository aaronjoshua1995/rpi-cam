#include "PipelineConstructor.h"

#include <gst/video/video.h>

#include <format>
#include <stdexcept>
#include <string>
#include <gst/video/videoorientation.h>

void PipelineConstructor::get_libcamerasrc_element(Pipeline* pipeline,
    std::string name, int width, int height, int fps, std::string pixelFormat,
    GstVideoOrientationMethod orientationMethod) {
  // Create camera source and caps
  const std::string caps_name = name + "_caps";
  GstElement* source = gst_element_factory_make("libcamerasrc", name.c_str());
  GstElement* capsfilter = gst_element_factory_make("capsfilter", caps_name.c_str());
  if (!source || !capsfilter) {
    g_printerr("Failed to create libcamerasrc elements.\n");
    return;
  }

  // Create queue
  GstElement* queue = get_queue_element(name + "_queue", 30, 0);

  // Define caps
  GstCaps* caps = gst_caps_new_simple(
      "video/x-raw", "format", G_TYPE_STRING, pixelFormat.c_str(), "width",
      G_TYPE_INT, width, "height", G_TYPE_INT, height, "framerate",
      GST_TYPE_FRACTION, fps, 1, nullptr);
  g_object_set(capsfilter, "caps", caps, nullptr);
  gst_caps_unref(caps);

  // Add elements to the pipeline
  pipeline->addElement(source);
  pipeline->addElement(capsfilter);
  pipeline->addElement(queue);

  // Flip the video if needed and add to pipeline
  if (orientationMethod != GST_VIDEO_ORIENTATION_IDENTITY) {
    const std::string flip_name = name + "_videoflip";
    GstElement* videoflip = get_videoflip_element(flip_name.c_str(), orientationMethod);
    pipeline->addElement(videoflip);
  }
}

GstElement* PipelineConstructor::get_videotestsrc_element(std::string name) {
  GstElement* src = gst_element_factory_make("videotestsrc", name.c_str());
  return src;
}

GstElement* PipelineConstructor::get_fakesink_element(std::string name) {
  GstElement* sink = gst_element_factory_make("fakesink", name.c_str());
  return sink;
}

GstElement* PipelineConstructor::get_autovideosink_element(std::string name) {
  GstElement* sink = gst_element_factory_make("autovideosink", name.c_str());
  return sink;
}

GstElement* PipelineConstructor::get_waylandsink_element(std::string name) {
  GstElement* sink = gst_element_factory_make("waylandsink", name.c_str());
  return sink;
}

GstElement* PipelineConstructor::get_fpsdisplaysink_element(std::string name) {
  GstElement* sink = gst_element_factory_make("autovideosink", name.c_str());
  g_object_set(sink, "video-sink", "xvimagessink", "scheduling-algorithm", 1,
               "sync", FALSE,"text-overlay", FALSE, nullptr);
  return sink;
}

GstElement* PipelineConstructor::get_tee_element(std::string name) {
  GstElement* tee = gst_element_factory_make("tee", name.c_str());
  return tee;
}

GstElement* PipelineConstructor::get_queue_element(const std::string name,
                                                   int maxBufferSize,
                                                   int leaky) {
  if (leaky < 0 || leaky > 2) {
    throw std::invalid_argument(std::format(
        "Invalid value: Expected 0, 1 or 2 for leaky value, got {} instead",
        leaky));
  }

  GstElement* queue = gst_element_factory_make("queue", name.c_str());
  if (!queue) {
    g_printerr("Failed to create queue element\n");
    return nullptr;
  }

  g_object_set(queue, "max-size-buffers", maxBufferSize, nullptr);
  g_object_set(queue, "leaky", leaky, nullptr);

  return queue;
}

GstElement* PipelineConstructor::get_videoflip_element(
    std::string name, GstVideoOrientationMethod orientationMethod) {
  GstElement* videoflip = gst_element_factory_make("videoflip", name.c_str());
  if (!videoflip) {
    g_printerr("Failed to create videoflip element\n");
    return nullptr;
  }
  g_object_set(videoflip, "method", GST_VIDEO_ORIENTATION_180, nullptr);
  return videoflip;
}

GstElement* PipelineConstructor::get_videoscale_element(std::string name,
                                                        int method, int threads,
                                                        bool addBorders,
                                                        bool qos) {
  GstElement* videoscale = gst_element_factory_make("videoscale", "face_videoscale");
  if (!videoscale) {
    g_printerr("Failed to create videoscale element\n");
    return nullptr;
  }
  g_object_set(videoscale,
              "method", method,
              "n-threads", threads,
              "add-borders", addBorders,
              "qos", qos,
              nullptr);
  return videoscale;
}

void PipelineConstructor::get_hailo_face_detection_element(Pipeline* pipeline) {
  const std::string postProcessDir = "/usr/lib/aarch64-linux-gnu/hailo/tappas/post_processes";
  const std::string faceDetectionHEFPath = "/retinaface_mobilenet_v1.hef";
  const std::string faceDetectionSO = postProcessDir + "/libface_detection_post.so";
  const std::string faceDetectionFunctionName = "retinaface";
  int vDeviceKey = 1;

      GstElement* hailonet =
          gst_element_factory_make("hailonet", "fd_hailonet");
  GstElement* hailofilter = gst_element_factory_make("hailofilter", "fd_hailofilter");

  if (!hailonet || !hailofilter) {
    g_printerr("Failed to create face detection elements.\n");
    return;
  }

  // Create queue
  GstElement* queue = get_queue_element("fd_queue", 30, 0);

  // hailonet properties
  g_object_set(hailonet, "hef-path", faceDetectionHEFPath.c_str(),
               "scheduling-algorithm", 1, "vdevice-key", vDeviceKey,
               nullptr);

  g_object_set(hailofilter, "so-path", faceDetectionSO.c_str(), "qos", FALSE,
               "function-name", faceDetectionFunctionName.c_str(), nullptr);

  pipeline->addElement(hailonet);
  pipeline->addElement(queue);
  pipeline->addElement(hailofilter);
}

void PipelineConstructor::get_hailo_detector_element(Pipeline* pipeline) {
  GstElement* tee = get_tee_element("detector_tee");
  GstElement* hmux = gst_element_factory_make("hailomuxer", "hmux");
  GstElement* bypassQueue = get_queue_element("detector_bypass_q", 30, 0);
  GstElement* videoscale = get_videoscale_element("face_videoscale", 0, 2, FALSE, FALSE);

  GstElement* capsfilter = gst_element_factory_make("capsfilter", nullptr);
  GstCaps* caps = gst_caps_from_string("video/x-raw, pixel-aspect-ratio=1/1");
  g_object_set(capsfilter, "caps", caps, nullptr);
  gst_caps_unref(caps);

  GstElement* preInferQueue = get_queue_element("pre_face_detector_infer_q", 30, 0);
  GstElement* postFilterQueue = get_queue_element("post_filter", 30, 0);
}