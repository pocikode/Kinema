#pragma once

#include "modules/MarkerDetection.h"
#include "modules/MarkerDetector.h" // RGBRatioRange

#include <vector>

// Chromaticity-based color blob detector. The frame is average-pooled
// (downsampled by block averaging) and each pooled pixel is classified by its
// normalized chromaticity rn = R/(R+G+B), gn = G/(R+G+B), gated on brightness
// and saturation. Each configured RGBRatioRange maps to one output observation,
// tagged with that range's index (marker identity = the color it matched).
class RGBRatioMarkerDetector : public IMarkerDetector
{
  public:
    void SetRanges(const std::vector<RGBRatioRange> &ranges);
    const std::vector<RGBRatioRange> &GetRanges() const;
    void SetPoolSize(int pool);

    std::vector<MarkerObservation> Detect() override;

    // Pooled frame upscaled back to full resolution (blocky), for UI preview.
    const cv::Mat &GetPooledFrame() const;

  private:
    std::vector<RGBRatioRange> m_ranges;
    int m_poolSize = 8;

    // Scratch buffers reused across Detect() calls to avoid per-frame allocations.
    cv::Mat m_pooled;     // average-pooled BGR (pooled resolution)
    cv::Mat m_pooledView; // m_pooled upscaled to full res for display
    cv::Mat m_label;      // per-pixel argmax range index (CV_16S, -1 = unassigned)
    cv::Mat m_rangeMask;  // single-range mask extracted from m_label (pooled res)
};
