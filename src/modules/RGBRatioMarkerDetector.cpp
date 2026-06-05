#include "modules/RGBRatioMarkerDetector.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>

void RGBRatioMarkerDetector::SetRanges(const std::vector<RGBRatioRange> &ranges)
{
    m_ranges = ranges;
}

const std::vector<RGBRatioRange> &RGBRatioMarkerDetector::GetRanges() const
{
    return m_ranges;
}

void RGBRatioMarkerDetector::SetPoolSize(int pool)
{
    m_poolSize = std::max(1, pool);
}

const cv::Mat &RGBRatioMarkerDetector::GetPooledFrame() const
{
    return m_pooledView;
}

std::vector<MarkerObservation> RGBRatioMarkerDetector::Detect()
{
    std::vector<MarkerObservation> observations;

    if (!GrabFrame())
    {
        return observations;
    }

    // Average pool: INTER_AREA box-averages each block when shrinking.
    const int pool = std::max(1, m_poolSize);
    const int pw = std::max(1, m_frame.cols / pool);
    const int ph = std::max(1, m_frame.rows / pool);
    cv::resize(m_frame, m_pooled, cv::Size(pw, ph), 0, 0, cv::INTER_AREA);
    cv::resize(m_pooled, m_pooledView, m_frame.size(), 0, 0, cv::INTER_NEAREST);

    // The visible mask is the union across all ranges, upscaled to full res for
    // parity with HSVMarkerDetector's GetLastMask().
    cv::Mat poolMask = cv::Mat::zeros(m_pooled.size(), CV_8UC1);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));

    // Area floor mirrors HSV's 500px full-res threshold, scaled to pooled grid.
    const double areaFloor = std::max(1.0, 500.0 / (static_cast<double>(pool) * pool));
    constexpr float kEps = 1e-3f;

    for (size_t ri = 0; ri < m_ranges.size(); ++ri)
    {
        const auto &range = m_ranges[ri];

        m_rangeMask = cv::Mat::zeros(m_pooled.size(), CV_8UC1);
        for (int y = 0; y < m_pooled.rows; ++y)
        {
            const cv::Vec3b *row = m_pooled.ptr<cv::Vec3b>(y);
            uchar *mrow = m_rangeMask.ptr<uchar>(y);
            for (int x = 0; x < m_pooled.cols; ++x)
            {
                // OpenCV stores BGR.
                const float b = row[x][0];
                const float g = row[x][1];
                const float r = row[x][2];
                if (std::max({b, g, r}) <= static_cast<float>(range.vMin))
                    continue;

                const float rg = r / (g + kEps);
                const float rb = r / (b + kEps);
                if (rg >= range.rgMin && rg <= range.rgMax && rb >= range.rbMin && rb <= range.rbMax)
                    mrow[x] = 255;
            }
        }

        cv::morphologyEx(m_rangeMask, m_rangeMask, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(m_rangeMask, m_rangeMask, cv::MORPH_CLOSE, kernel);
        cv::bitwise_or(poolMask, m_rangeMask, poolMask);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(m_rangeMask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        if (contours.empty())
        {
            continue;
        }

        // Keep only the largest blob per color range — single marker per color.
        const std::vector<cv::Point> *best = nullptr;
        double bestArea = areaFloor;
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

        // Centroid normalized by pooled dims keeps the [0..1] space identical to HSV.
        MarkerObservation obs;
        obs.id = static_cast<int>(ri);
        obs.centroidNorm = {static_cast<float>(m.m10 / m.m00) / static_cast<float>(pw),
                            static_cast<float>(m.m01 / m.m00) / static_cast<float>(ph)};
        // Scale area back to full-res pixels so the depth proxy matches HSV.
        obs.areaPixels = static_cast<float>(bestArea) * static_cast<float>(pool) * pool;
        cv::Rect r = cv::boundingRect(*best);
        obs.boundingBox = cv::Rect(r.x * pool, r.y * pool, r.width * pool, r.height * pool);
        observations.push_back(obs);
    }

    cv::resize(poolMask, m_mask, m_frame.size(), 0, 0, cv::INTER_NEAREST);

    return observations;
}
