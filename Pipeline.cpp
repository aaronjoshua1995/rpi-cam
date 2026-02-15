#include "Pipeline.h"

void Pipeline::addElement(GstElement* element) {
  if (element) {
    elements.push_back(element);
  }
}

const std::vector<GstElement*>& Pipeline::getElements() const {
  return elements;
}
