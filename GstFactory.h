#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>

#include <string>

class GstFactory {
public:
  static GstElement* getLibcameraSrc(const std::string& nam, const int autoFocusMode);
  static GstElement* getTimeoverlay(const std::string& name);
  static GstElement* getCaps(const std::string& name, const std::string& format,
                             const std::string& pixelFormat = "",
                             int width = -1, int height = -1,
                             int framerate = -1, 
                             const std::string& pixelAspectRatio = "", 
                             const std::string& level = "");
  static GstElement* getVideoconvert(const std::string& name, const int nThreads = 0, bool qos = false);
  static GstElement* getVideorate(const std::string& name);
  static GstElement* getV4l2convert(const std::string& name);
  static GstElement* getV4l2h264Enc(const std::string& name, const int bitrate);
  static GstElement* getH264Parse(const std::string& name);
  static GstElement* getQueue(const std::string& name, 
                              int maxSizeBytes = 0,
                              int maxSizeBuffers = 0,
                              int maxSizeTime = 0,
                              int leaky = 0);
  static GstElement* getSplitMuxSink(const std::string& name,
                                     const std::string& location,
                                     long maxSizeBytes = 0,
                                     long maxSizeTime = 0, 
                                     int maxFiles = 0,
                                     const std::string& muxer = "mp4mux");
  static GstElement* getTee(const std::string& name);
  static GstElement* getIdentity(const std::string& name);
  static GstElement* getFakesink(const std::string& name);
  static GstElement* getValve(const std::string& name, const std::string& type);
  static GstElement* getFpsDisplaySink(const std::string& name,
                                       const std::string& videoSink = "xvimagesink",
                                       bool sync = false,
                                       bool textOverlay = false);
  static GstElement* getVideoFlip(const std::string& name, GstVideoOrientationMethod method);
  static GstElement* getVideoScale(const std::string& name,
                                  int method,
                                  int threads,
                                  bool addBorders,
                                  bool qos);
  static GstElement* getHailoNet(const std::string& name, const std::string& hefPath, int schedulingAlgorithm, int vDeviceKey);
  static GstElement* getHailoFilter(const std::string& name, const std::string& soPath, const std::string& funcName, bool qos);
  static GstElement* getHailoMuxer(const std::string& name);
  static GstElement* getHailoCropper(const std::string& name, const std::string& soPath, const std::string& funcName);
  static GstElement* getHailoAggregator(const std::string& name);
  static GstElement* getHailoTracker(const std::string& name);
  static GstElement* getHailoGallery(const std::string& name,
                              const std::string& galleryFilePath,
                              float similarityThreshold, int queueSize,
                              int classId, bool loadLocalGallery);
  static GstElement* getHailoOverlay(const std::string& name, int lineThickness,
                              int fontThickness, int landmarkPointRadius,
                              bool qos, bool showConfidence, bool localGallery);
};
