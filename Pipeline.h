#ifndef PIPELINE_H
#define PIPELINE_H

#include <gst/gst.h>

#include <vector>

class Pipeline {
 public:
  Pipeline() = default;
  ~Pipeline() = default;

  // Add a GstElement to the pipeline container
  void addElement(GstElement* element);

  // Retrieve all elements (const reference to avoid copy)
  const std::vector<GstElement*>& getElements() const;

 private:
  std::vector<GstElement*> elements;
};

#endif  // PIPELINE_H