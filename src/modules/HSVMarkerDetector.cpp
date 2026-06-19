#include "modules/HSVMarkerDetector.h"

#include <algorithm>
#include <cmath>
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

    const int n = static_cast<int>(m_ranges.size());

    // Per-range hue + saturation anchor, plus active flag. Red (dualHue) anchors
    // at the 0/180 wrap seam; every other band anchors at its center hue. The
    // saturation anchor is the band's S center.
    std::vector<float> anchor(n, 0.0f);
    std::vector<float> sAnchor(n, 0.0f);
    std::vector<unsigned char> active(n, 0);
    for (int ri = 0; ri < n; ++ri)
    {
        active[ri] = BlobCount(ri) > 0 ? 1 : 0; // 0 = paired follower slot, skipped
        const auto &r = m_ranges[ri];
        anchor[ri] = r.dualHue ? 0.0f : 0.5f * (r.hMin + r.hMax);
        sAnchor[ri] = 0.5f * (r.sMin + r.sMax);
    }

    // Per-pixel argmax: a pixel that falls inside several ranges' bands is
    // assigned to the nearest anchor in (hue, saturation). Hue distance is the
    // circular distance on the 0..179 wheel (normalized by its 90 max); a
    // weighted saturation term lets two colors that share a hue but differ in
    // vividness — e.g. vivid red vs pastel pink — separate cleanly. When every
    // slot shares the same S range the term cancels, so behavior collapses to
    // pure hue argmax (backward compatible). Overlapping bands therefore can't
    // double-claim the same blob, the main source of marker identity swaps.
    constexpr float kSatWeight = 1.0f;
    m_label.create(m_frameHSV.size(), CV_16S);
    m_label.setTo(-1);
    for (int y = 0; y < m_frameHSV.rows; ++y)
    {
        const cv::Vec3b *row = m_frameHSV.ptr<cv::Vec3b>(y);
        short *lrow = m_label.ptr<short>(y);
        for (int x = 0; x < m_frameHSV.cols; ++x)
        {
            const int h = row[x][0];
            const int s = row[x][1];
            const int v = row[x][2];
            int best = -1;
            float bestDist = 1e9f;
            for (int ri = 0; ri < n; ++ri)
            {
                if (!active[ri])
                    continue;
                const auto &r = m_ranges[ri];
                if (s < r.sMin || s > r.sMax || v < r.vMin || v > r.vMax)
                    continue;
                const bool inHue = (h >= r.hMin && h <= r.hMax) ||
                                   (r.dualHue && h >= r.hMin2 && h <= r.hMax2);
                if (!inHue)
                    continue;
                float dh = std::fabs(static_cast<float>(h) - anchor[ri]);
                dh = std::min(dh, 180.0f - dh);    // hue wheel wraps at 180
                const float dhn = dh / 90.0f;      // max circular hue distance is 90
                const float dsn =                  // saturation mismatch, normalized
                    std::fabs(static_cast<float>(s) - sAnchor[ri]) / 255.0f;
                const float d = dhn + kSatWeight * dsn;
                if (d < bestDist)
                {
                    bestDist = d;
                    best = ri;
                }
            }
            lrow[x] = static_cast<short>(best);
        }
    }

    // The visible mask is the union across all configured ranges; useful for the UI overlay.
    m_mask = cv::Mat::zeros(m_frame.size(), CV_8UC1);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));

    for (int ri = 0; ri < n; ++ri)
    {
        const int want = BlobCount(ri);
        if (want <= 0)
        {
            continue; // paired follower slot: resolved from its primary's range
        }

        cv::compare(m_label, ri, m_rangeMask, cv::CMP_EQ);
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
