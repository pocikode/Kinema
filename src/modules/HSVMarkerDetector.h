#pragma once

#include "modules/MarkerDetection.h"
#include "modules/MarkerDetector.h" // HSVRange

#include <vector>

// Per-range color blob detector. Each configured HSVRange maps to one output
// observation, tagged with that range's index (so marker identity is
// determined by the color it matched).
class HSVMarkerDetector : public IMarkerDetector
{
  public:
    void SetRanges(const std::vector<HSVRange> &ranges);
    const std::vector<HSVRange> &GetRanges() const;

    std::vector<MarkerObservation> Detect() override;

  private:
    std::vector<HSVRange> m_ranges;

    // Scratch buffers reused across Detect() calls to avoid per-frame allocations.
    cv::Mat m_frameHSV;
    cv::Mat m_rangeMask;
};
