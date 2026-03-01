#include "GstFactory.h"

#include <gst/gst.h>
#include <stdexcept>

GstElement* GstFactory::getLibcameraSrc(const std::string& name, const int autoFocusMode) {
  GstElement* elem = gst_element_factory_make("libcamerasrc", name.c_str());
  if (!elem) {
    g_printerr("GstFactory: failed to create element 'libcamerasrc' with name '%s'\n", name.c_str());
  }
  g_object_set(elem, "auto-focus-mode", autoFocusMode, nullptr);
  return elem;
}

GstElement* GstFactory::getTimeoverlay(const std::string& name) {
  GstElement* elem = gst_element_factory_make("timeoverlay", name.c_str());
  if (!elem) {
    g_printerr(
        "GstFactory: failed to create element 'timeoverlay' with name '%s'\n",
        name.c_str());
  }
  return elem;
}

GstElement* GstFactory::getCaps(const std::string& name, const std::string& format,
                                 const std::string& pixelFormat,
                                 int width, int height, int framerate,
                                 const std::string& pixelAspectRatio,
                                 const std::string& level) {
  std::string capsStr = format;

  if (!pixelFormat.empty()) {
    if (!capsStr.empty()) capsStr += ",";
    capsStr += "format=" + pixelFormat;
  }
  // if (width > 0 && height > 0 && width <= Config::MAX_WIDTH && height <= Config::MAX_HEIGHT) {
  if (width > 0 && height > 0) {
    if (!capsStr.empty()) capsStr += ",";
    capsStr += "width=" + std::to_string(width) + ",height=" + std::to_string(height);
  }
  // if (framerate > 0 && framerate <= Config::MAX_FRAMERATE) {
  if (framerate > 0) {
    if (!capsStr.empty()) capsStr += ",";
    capsStr += "framerate=" + std::to_string(framerate) + "/1";
  }
  if (!level.empty()) {
    if (!capsStr.empty()) capsStr += ",";
    capsStr += "level=" + level;
  }
  if (!pixelAspectRatio.empty()) {
    if (!capsStr.empty()) capsStr += ",";
    capsStr += "pixel-aspect-ratio=" + pixelAspectRatio;
  }

  g_print("%s caps string: '%s'\n", name.c_str(), capsStr.c_str());

  GstCaps* caps = gst_caps_from_string(capsStr.c_str());
  if (!caps) {
    g_printerr("GstFactory: failed to create GstCaps from string '%s'\n", capsStr.c_str());
    return nullptr;
  }

  GstElement* capsfilter = gst_element_factory_make("capsfilter", name.c_str());
  if (!capsfilter) {
    g_printerr(
        "GstFactory: failed to create element 'capsfilter' with name '%s'\n",
        name.c_str());
    if (caps) gst_caps_unref(caps);
    return nullptr;
  }

  g_object_set(capsfilter, "caps", caps, NULL);
  gst_caps_unref(caps);

  return capsfilter;
}

GstElement* GstFactory::getVideoconvert(const std::string& name, const int nThreads, bool qos) {
  GstElement* elem = gst_element_factory_make("videoconvert", name.c_str());
  if (!elem) g_printerr("GstFactory: failed to create element 'videoconvert' with name '%s'\n", name.c_str());
  g_object_set(elem, "n-threads", nThreads, "qos", qos, nullptr);
  return elem;
}

GstElement* GstFactory::getVideorate(const std::string& name) {
  GstElement* elem = gst_element_factory_make("videorate", name.c_str());
  if (!elem) g_printerr("GstFactory: failed to create element 'videorate' with name '%s'\n", name.c_str());
  return elem;
}

GstElement* GstFactory::getV4l2convert(const std::string& name) {
  GstElement* elem = gst_element_factory_make("videoconvert", name.c_str());
  if (!elem) g_printerr("GstFactory: failed to create element 'v4l2convert' with name '%s'\n", name.c_str());
  return elem;
}

GstElement* GstFactory::getV4l2h264Enc(const std::string& name, const int bitrate) {
  GstElement* enc = gst_element_factory_make("v4l2h264enc", name.c_str());
  GstStructure* controls = gst_structure_new("controls", "video_bitrate", G_TYPE_INT, bitrate, NULL);
  g_object_set(enc, "extra-controls", controls, NULL);
  gst_structure_free(controls);
  if (!enc) g_printerr("GstFactory: failed to create element 'v4l2h264enc' with name '%s'\n", name.c_str());
  return enc;
}

GstElement* GstFactory::getH264Parse(const std::string& name) {
  GstElement* elem = gst_element_factory_make("h264parse", name.c_str());
  if (!elem) g_printerr("GstFactory: failed to create element 'h264parse' with name '%s'\n", name.c_str());
  return elem;
}

GstElement* GstFactory::getQueue(const std::string& name, int maxSizeBuffers,
                                 int maxSizeBytes, int maxSizeTime,
                                 int leaky) {
  if (leaky < 0 || leaky > 2) {
    g_printerr(
        "Invalid leaky value '%d' for queue '%s', ust be 0 (no leak), 1 (leak "
        "on upstream), or 2 (leak on downstream)\n",
        leaky, name.c_str());
    return nullptr;
  }

  GstElement* queue = gst_element_factory_make("queue", name.c_str());
  if (!queue) {
    g_printerr("GstFactory: failed to create element 'queue' with name '%s'\n", name.c_str());
    return NULL;
  }

  g_object_set(queue, "max-size-buffers", maxSizeBuffers, NULL);
  g_object_set(queue, "max-size-bytes", maxSizeBytes, NULL);
  g_object_set(queue, "max-size-time", maxSizeTime, NULL);
  g_object_set(queue, "leaky", leaky, NULL);

  return queue;
}

GstElement* GstFactory::getSplitMuxSink(const std::string& name,
                                        const std::string& location,
                                        long maxSizeBytes, long maxSizeTime,
                                        int maxFiles,
                                        const std::string& muxer) {
  GstElement* splitmuxsink = gst_element_factory_make("splitmuxsink", name.c_str());
  if (!splitmuxsink) {
    g_printerr("GstFactory: failed to create element 'splitmuxsink' with name '%s'\n", name.c_str());
    return nullptr;
  }
  g_object_set(splitmuxsink,
               "location", location.c_str(),
               "max-size-bytes", maxSizeBytes,
               "max-size-time", maxSizeTime,
               "max-files", maxFiles,
               "muxer-factory", muxer.c_str(),
               NULL);
  return splitmuxsink;
}

GstElement* GstFactory::getTee(const std::string& name) {
  GstElement* elem = gst_element_factory_make("tee", name.c_str());
  if (!elem) {
    g_printerr(
        "GstFactory: failed to create element 'tee' with name '%s'\n",
        name.c_str());
  }
  return elem;
}

GstElement* GstFactory::getIdentity(const std::string& name) {
  GstElement* elem = gst_element_factory_make("identity", name.c_str());
  if (!elem) {
    g_printerr(
        "GstFactory: failed to create element 'identity' with name '%s'\n",
        name.c_str());
  }
  return elem;
}

GstElement* GstFactory::getFakesink(const std::string& name) {
  GstElement* elem = gst_element_factory_make("fakesink", name.c_str());
  if (!elem) g_printerr("GstFactory: failed to create element 'fakesink' with name '%s'\n", name.c_str());
  return elem;
}

GstElement* GstFactory::getValve(const std::string& name, const std::string& type) {
  GstElement* elem = gst_element_factory_make(type.c_str(), name.c_str());
  if (!elem) g_printerr("GstFactory: failed to create element '%s' with name '%s'\n", type.c_str(), name.c_str());
  return elem;
}

GstElement* GstFactory::getFpsDisplaySink(const std::string& name,
                                          const std::string& videoSink,
                                          bool sync, bool textOverlay) {
  GstElement* fpsSink = gst_element_factory_make("videoscale", name.c_str());
  if (!fpsSink) {
    g_printerr("Failed to create videoscale element\n");
    return nullptr;
  }
  g_object_set(fpsSink,
              "video-sink", videoSink.c_str(),
              "sync", sync,
              "text-overlay", textOverlay,
              nullptr);
  return fpsSink;
}

GstElement* GstFactory::getVideoFlip(const std::string& name,
                                     GstVideoOrientationMethod method) {
  GstElement* videoflip = gst_element_factory_make("videoflip", name.c_str());
  if (!videoflip) {
    g_printerr("Failed to create videoflip element\n");
    return nullptr;
  }
  g_object_set(videoflip, "method", GST_VIDEO_ORIENTATION_180, nullptr);
  return videoflip;
}

GstElement* GstFactory::getVideoScale(const std::string& name,
                                      int method, int threads, bool addBorders, bool qos) {
  GstElement* videoscale = gst_element_factory_make("videoscale", name.c_str());
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

// GstElement* GstFactory::getHailoNet(const std::string& name,
//                                     const std::string& hefPath,
//                                     int schedulingAlgorithm, int vDeviceKey) {
//   GstElement* hailonet = gst_element_factory_make("hailonet", name.c_str());
//   if (!hailonet) {
//     g_printerr("Failed to create hailonet element\n");
//     return nullptr;
//   }
//   g_object_set(hailonet,
//                "hef-path", hefPath.c_str(),
//                "scheduling-algorithm", schedulingAlgorithm,
//                "vdevice-group-id", "SHARED",
//                "force-writeable", TRUE,
//                nullptr);
//   return hailonet;
// }

GstElement* GstFactory::getHailoNet(const std::string& name,
                                    const std::string& hefPath,
                                    const std::string& vDeviceGroupId,
                                    int batchSize) {
  GstElement* hailonet = gst_element_factory_make("hailonet", name.c_str());
  if (!hailonet) {
    g_printerr("Failed to create hailonet element\n");
    return nullptr;
  }
  g_object_set(hailonet,
               "hef-path", hefPath.c_str(),
               "batch-size", batchSize,
               "vdevice-group-id", vDeviceGroupId.c_str(),
               "force-writeable", TRUE,
               nullptr);
  return hailonet;
}

GstElement* GstFactory::getHailoFilter(const std::string& name,
                                       const std::string& soPath,
                                       const std::string& funcName, bool qos, bool useGstBuffer) {
  GstElement* hailofilter = gst_element_factory_make("hailofilter", name.c_str());
  if (!hailofilter) {
    g_printerr("Failed to create hailofilter element\n");
    return nullptr;
  }

  if (!funcName.empty()) {
    g_object_set(hailofilter, "function-name", funcName.c_str(), nullptr);
  }
  g_object_set(hailofilter,
               "so-path", soPath.c_str(),
               "use-gst-buffers", useGstBuffer, 
               "qos", qos,
               nullptr);
  return hailofilter;
}

GstElement* GstFactory::getHailoMuxer(const std::string& name) {
  GstElement* hmux = gst_element_factory_make("hailomuxer", name.c_str());
  if (!hmux) {
    g_printerr("Failed to create hailomuxer element\n");
    return nullptr;
  }
  return hmux;
}

GstElement* GstFactory::getHailoCropper(const std::string& name,
                                        const std::string& soPath,
                                        const std::string& funcName) {
  GstElement* cropper = gst_element_factory_make("hailocropper", name.c_str());
  if (!cropper) {
    g_printerr("Failed to create hailocropper element\n");
    return nullptr;
  }
  g_object_set(cropper,
              "so-path", soPath.c_str(),
              "function-name", funcName.c_str(),
              "use-letterbox", TRUE,
              "resize-method", "inter-area",
              "internal-offset", TRUE,
              nullptr);
  return cropper;
}

GstElement* GstFactory::getHailoAggregator(const std::string& name) {
  GstElement* agg = gst_element_factory_make("hailoaggregator", name.c_str());
  if (!agg) {
    g_printerr("Failed to create hailoaggregator element\n");
    return nullptr;
  }
  return agg;
}

GstElement* GstFactory::getHailoTracker(const std::string& name) {
  GstElement* tracker = gst_element_factory_make("hailotracker", name.c_str());
  if (!tracker) {
    g_printerr("Failed to create hailotracker element\n");
    return nullptr;
  }
    g_object_set(tracker,
      "class-id", -1,
      "kalman-dist-thr", 0.7,
      "iou-thr", 0.8,
      "init-iou-thr", 0.9,
      "keep-new-frames", 2,
      "keep-tracked-frames", 6,
      "keep-lost-frames", 8,
      "keep-past-metadata", TRUE,
      "qos", FALSE, nullptr);
  return tracker;
}

GstElement* GstFactory::getHailoGallery(const std::string& name,
                                        const std::string& galleryFilePath,
                                        float similarityThreshold,
                                        int queueSize, int classId,
                                        bool loadLocalGallery) {
  GstElement* gallery = gst_element_factory_make("hailogallery", name.c_str());
  if (!gallery) {
    g_printerr("Failed to create hailotracker element\n");
    return nullptr;
  }
  g_object_set(gallery, 
    "gallery-file-path", galleryFilePath.c_str(), 
    "load-local-gallery", loadLocalGallery,
    "similarity-thr", similarityThreshold,
    "gallery-queue-size", queueSize,
    "class-id", classId, nullptr);
  return gallery;
}

GstElement* GstFactory::getHailoOverlay(const std::string& name,
                                        int lineThickness, int fontThickness,
                                        int landmarkPointRadius, bool qos,
                                        bool showConfidence,
                                        bool localGallery) {
  GstElement* overlay = gst_element_factory_make("hailogallery", name.c_str());
  if (!overlay) {
    g_printerr("Failed to create hailotracker element\n");
    return nullptr;
  }
  g_object_set(overlay, 
    "qos", qos, 
    "show-confidence", showConfidence,
    "local-gallery", localGallery,
    "line-thickness", lineThickness,
    "font-thickness", fontThickness, 
    "landmark-point-radius", landmarkPointRadius, nullptr);
  return overlay;
}