#include <gst/gst.h>
#include <gst/video/video.h>

#include <string>

#include "Pipeline.h"

class PipelineConstructor {
public:
  void get_libcamerasrc_element(Pipeline* pipeline, std::string name, int width,
                                int height, int fps, std::string pixelFormat,
                                GstVideoOrientationMethod orientationMethod);

  /**
   * @brief Get the videotestsrc element object
   *
   * @param name Name of the element
   * @return GstElement*
   */
  static GstElement* get_videotestsrc_element(std::string name);

  /**
   * @brief Get the fakesink element object
   *
   * @param name Name of the element
   * @return GstElement*
   */
  static GstElement* get_fakesink_element(std::string name);

  /**
   * @brief Get the tee element object
   *
   * @param name Name of the element
   * @return GstElement*
   */
  static GstElement* get_tee_element(std::string name);

  /**
   * @brief Get the queue element object
   *
   * @param name Name of the element
   * @param max_buffer_size Maximum buffer size
   * @param leaky Whether to drop elements from the queue when the buffer is
   * full. `0 = no leak, 1 = downstream (drop oldest), 2 = upstream (drop
   * newest)`
   * @return GstElement*
   */
  static GstElement* get_queue_element(std::string name, int maxBufferSize,
                                       int leaky = 0);

  /**
   * @brief Get the videoflip element object
   *
   * @param name Name of the element
   * @param orientationMethod GstVideoOrientationMethod
   * @return GstElement*
   */
  static GstElement* get_videoflip_element(
      std::string name, GstVideoOrientationMethod orientationMethod);

  static GstElement* get_videoscale_element(std::string name, int method, int threads, bool addBorders, bool qos);


  /////////////// HAILO ELEMENTS ///////////////
  void get_hailo_face_detection_element(Pipeline* pipeline);
  void get_hailo_detector_element(Pipeline* pipeline);
};