#pragma once

#include <vector>
#include <gst/gst.h>

struct PipelineElement {
  GstElement* element;
  std::vector<std::vector<GstElement*>> branches = {};  // Each branch is a vector of elements
};

class Pipeline {
public:
  Pipeline() = default;
  ~Pipeline() = default;

  void addElement(const PipelineElement& element);

  GstElement* construct();

 private:
  std::vector<PipelineElement> mElements;
};