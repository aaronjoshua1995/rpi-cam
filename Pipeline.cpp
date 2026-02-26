#include "Pipeline.h"

void Pipeline::addElement(const PipelineElement& element) {
  mElements.push_back(element);
}

GstElement* Pipeline::construct() {
  GstElement* pipeline = gst_pipeline_new("dashcam_pipeline");
  if (!pipeline) {
    g_printerr("Failed to create pipeline\n");
    return nullptr;
  }

  // Add all elements to the pipeline
  for (auto pe : mElements) {
    gst_bin_add(GST_BIN(pipeline), pe.element);
    for (const auto& branch : pe.branches) {
      for (auto branchElement : branch) {
        gst_bin_add(GST_BIN(pipeline), branchElement);
      }
    }
  }

  // Link elements in the main branch.
  for (int i = 0; i < mElements.size(); ++i) {
    PipelineElement pe = mElements[i];

    if (pe.branches.empty() && i < mElements.size() - 1) {
      // If there are no branches, link to the next element in the pipeline.
      g_print("'%s' --> '%s'\n", GST_ELEMENT_NAME(pe.element),
              GST_ELEMENT_NAME(mElements[i + 1].element));
      gst_element_link(pe.element, mElements[i + 1].element);
    } else {
      // If there are branches link the current element to the first element of each branch
      // If the last elements of the branch are the same, merge them to that elements sink pads.
      bool lastBranchElementsSame = pe.branches.size() >= 2;
      GstElement* lastElement = nullptr;
      for (int j = 0; j < pe.branches.size(); ++j) {
        const auto& branch = pe.branches[j];
        if (j == 0) {
          lastElement = branch.back();
        } else if (lastElement != branch.back()) {
          lastBranchElementsSame = FALSE; 
          break;
        }
      }

      for (int j = 0; j < pe.branches.size(); ++j) {
        const auto& branch = pe.branches[j];
        g_print("'src: %s' --> snk: '%s'\n", GST_ELEMENT_NAME(pe.element),
                GST_ELEMENT_NAME(branch.front()));
        GstPad* srcPad = gst_element_request_pad_simple(pe.element, "src_%u");
        if (!srcPad) {
          g_printerr("Failed to request srcPad\n");
          gst_object_unref(pipeline);
          return nullptr;
        }
        GstPad* sinkPad = gst_element_get_static_pad(branch.front(), "sink");
        if (!sinkPad) {
          g_printerr("Failed to get sinkPad of branch's first element\n");
          gst_element_release_request_pad(pe.element, srcPad);
          gst_object_unref(srcPad);
          gst_object_unref(pipeline);
          return nullptr;
        }
        if (gst_pad_link(srcPad, sinkPad) != GST_PAD_LINK_OK) {
          g_printerr("Failed to link tee to pre_splitmux_q\n");
          gst_element_release_request_pad(pe.element, srcPad);
          gst_object_unref(srcPad);
          gst_object_unref(sinkPad);
          gst_object_unref(pipeline);
          return nullptr;
        }
        gst_element_release_request_pad(pe.element, srcPad);
        gst_object_unref(srcPad);
        gst_object_unref(sinkPad);
        
        // Link the element to the first element in the branch
        gst_element_link(pe.element, branch.front());


        int x = lastBranchElementsSame ? 1 : 0;
        // Link elements within the branch
        for (size_t n = 0; n < branch.size() - 1 - x; ++n) {
          g_print("'%s' --> '%s'\n", GST_ELEMENT_NAME(branch[n]),
                  GST_ELEMENT_NAME(branch[n + 1]));
          if (!gst_element_link(branch[n], branch[n + 1])) {
            g_printerr("Failed to link elements within branch\n");
            gst_object_unref(pipeline);
            return nullptr;
          }
        }
      }


      if (lastBranchElementsSame) {
        // Link the second to last elements src pads to the last elements sink
        // pads
        for (const auto& branch: pe.branches) {
          auto elementToMerge = branch[branch.size() - 2];
          // Always use the last element of the first branch, as that one is added to the 
          // GST BIN, which is required to get the pads.
          auto lastElement = pe.branches[0].back();

          g_print("'src: %s' --> snk: '%s'\n", GST_ELEMENT_NAME(elementToMerge),
                  GST_ELEMENT_NAME(lastElement));

          GstPad* srcPad = gst_element_get_static_pad(elementToMerge, "src");
          if (!srcPad) {
            g_printerr("Failed to request srcPad from second-to-last branch element\n");
            gst_object_unref(pipeline);
            return nullptr;
          }
          GstPad* sinkPad = gst_element_request_pad_simple(lastElement, "sink_%u");
          if (!sinkPad) {
            g_printerr("Failed to get sinkPad of branch's last element\n");
            gst_element_release_request_pad(pe.element, srcPad);
            gst_object_unref(srcPad);
            gst_object_unref(pipeline);
            return nullptr;
          }
          if (gst_pad_link(srcPad, sinkPad) != GST_PAD_LINK_OK) {
            g_printerr("Failed to link\n");
            gst_element_release_request_pad(pe.element, srcPad);
            gst_object_unref(srcPad);
            gst_object_unref(sinkPad);
            gst_object_unref(pipeline);
            return nullptr;
          }
          gst_element_release_request_pad(pe.element, sinkPad);
          gst_object_unref(srcPad);
          gst_object_unref(sinkPad);

        }
      }
    }
        }
  return pipeline;
}