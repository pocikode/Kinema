#include "modules/HSVMarkerDetector.h"

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
        const auto &range = m_ranges[ri];

        cv::inRange(m_frameHSV, cv::Scalar(range.hMin, range.sMin, range.vMin),
                    cv::Scalar(range.hMax, range.sMax, range.vMax), m_rangeMask);
        cv::morphologyEx(m_rangeMask, m_rangeMask, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(m_rangeMask, m_rangeMask, cv::MORPH_CLOSE, kernel);
        cv::bitwise_or(m_mask, m_rangeMask, m_mask);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(m_rangeMask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        if (contours.empty())
        {
            continue;
        }

        // Keep only the largest blob per color range — single marker per color.
        // Area threshold keeps ambient-noise blobs (lighting speckle, shadows) out
        // of the observation stream so unbound downstream bones don't jitter.
        const std::vector<cv::Point> *best = nullptr;
        double bestArea = 500.0;
        for (const auto &c : contours)
        {
            double area = cv::contourArea(c);
            if (area > bestArea)
            {
                bestArea = area;
                best = &c;
            }
        }
        if (!best)
        {
            continue;
        }

        cv::Moments m = cv::moments(*best);
        if (m.m00 == 0.0)
        {
            continue;
        }

        MarkerObservation obs;
        obs.id = static_cast<int>(ri);
        obs.centroidNorm = {static_cast<float>(m.m10 / m.m00) / static_cast<float>(m_width),
                            static_cast<float>(m.m01 / m.m00) / static_cast<float>(m_height)};
        obs.areaPixels = static_cast<float>(bestArea);
        obs.boundingBox = cv::boundingRect(*best);
        observations.push_back(obs);
    }

    return observations;
}
