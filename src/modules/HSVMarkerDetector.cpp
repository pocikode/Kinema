#include "modules/HSVMarkerDetector.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>

void HSVMarkerDetector::SetRanges(const std::vector<HSVRange> &ranges)
{
    m_ranges = ranges;
}

const std::vector<HSVRange> &HSVMarkerDetector::GetRanges() const
{
    return m_ranges;
}

std::vector<MarkerObservation> HSVMarkerDetector::Detect()
{
    std::vector<MarkerObservation> observations;

    if (!GrabFrame())
    {
        return observations;
    }

    cv::cvtColor(m_frame, m_frameHSV, cv::COLOR_BGR2HSV);

    // The visible mask is the union across all configured ranges; useful for the UI overlay.
    m_mask = cv::Mat::zeros(m_frame.size(), CV_8UC1);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));

    for (size_t ri = 0; ri < m_ranges.size(); ++ri)
    {
        const int want = BlobCount(ri);
        if (want <= 0)
        {
            continue; // paired follower slot: resolved from its primary's range
        }
        const auto &range = m_ranges[ri];

        cv::inRange(m_frameHSV, cv::Scalar(range.hMin, range.sMin, range.vMin),
                    cv::Scalar(range.hMax, range.sMax, range.vMax), m_rangeMask);
        if (range.dualHue)
        {
            // Second hue band for wrap-around colors (red). Same S/V bounds.
            cv::inRange(m_frameHSV, cv::Scalar(range.hMin2, range.sMin, range.vMin),
                        cv::Scalar(range.hMax2, range.sMax, range.vMax), m_rangeMask2);
            cv::bitwise_or(m_rangeMask, m_rangeMask2, m_rangeMask);
        }
        cv::morphologyEx(m_rangeMask, m_rangeMask, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(m_rangeMask, m_rangeMask, cv::MORPH_CLOSE, kernel);
        cv::bitwise_or(m_mask, m_rangeMask, m_mask);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(m_rangeMask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        if (contours.empty())
        {
            continue;
        }

        // Keep the `want` largest blobs per color range (paired slots use two).
        // Area threshold keeps ambient-noise blobs (lighting speckle, shadows) out
        // of the observation stream so unbound downstream bones don't jitter.
        std::vector<std::pair<double, const std::vector<cv::Point> *>> candidates;
        for (const auto &c : contours)
        {
            double area = cv::contourArea(c);
            if (area > 500.0)
            {
                candidates.push_back({area, &c});
            }
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto &a, const auto &b) { return a.first > b.first; });
        if (candidates.size() > static_cast<size_t>(want))
        {
            candidates.resize(static_cast<size_t>(want));
        }

        for (const auto &[area, contour] : candidates)
        {
            cv::Moments m = cv::moments(*contour);
            if (m.m00 == 0.0)
            {
                continue;
            }

            MarkerObservation obs;
            obs.id = static_cast<int>(ri);
            obs.centroidNorm = {static_cast<float>(m.m10 / m.m00) / static_cast<float>(m_width),
                                static_cast<float>(m.m01 / m.m00) / static_cast<float>(m_height)};
            obs.areaPixels = static_cast<float>(area);
            obs.boundingBox = cv::boundingRect(*contour);
            observations.push_back(obs);
        }
    }

    return observations;
}
