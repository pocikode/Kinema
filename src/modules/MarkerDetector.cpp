#include "modules/MarkerDetector.h"
#include <opencv2/imgproc.hpp>

bool MarkerDetector::Init(int cameraIndex)
{
    m_cap.open(cameraIndex);
    if (!m_cap.isOpened())
    {
        return false;
    }

    m_width = static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_WIDTH));
    m_height = static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    return true;
}

void MarkerDetector::Destroy()
{
    if (m_cap.isOpened())
    {
        m_cap.release();
    }
}

MarkerResult MarkerDetector::Update(const HSVRange &range)
{
    MarkerResult result;

    if (!m_cap.isOpened())
    {
        return result;
    }

    m_cap >> m_frame;
    if (m_frame.empty())
    {
        return result;
    }

    cv::cvtColor(m_frame, m_frameHSV, cv::COLOR_BGR2HSV);
    cv::inRange(m_frameHSV, cv::Scalar(range.hMin, range.sMin, range.vMin),
                cv::Scalar(range.hMax, range.sMax, range.vMax), m_mask);

    // Morphological operations to reduce noise
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(m_mask, m_mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(m_mask, m_mask, cv::MORPH_CLOSE, kernel);

    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(m_mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty())
    {
        return result;
    }

    // Find largest contour with area > 100 px²
    std::vector<cv::Point> largestContour;
    double maxArea = 100.0;

    for (const auto &contour : contours)
    {
        double area = cv::contourArea(contour);
        if (area > maxArea)
        {
            maxArea = area;
            largestContour = contour;
        }
    }

    if (largestContour.empty())
    {
        return result;
    }

    // Compute centroid via moments
    cv::Moments m = cv::moments(largestContour);
    if (m.m00 == 0.0)
    {
        return result;
    }

    float cx = static_cast<float>(m.m10 / m.m00);
    float cy = static_cast<float>(m.m01 / m.m00);

    result.detected = true;
    result.centroidNorm = {cx / m_width, cy / m_height};
    result.areaPixels = static_cast<float>(maxArea);
    result.boundingBox = cv::boundingRect(largestContour);

    return result;
}

const cv::Mat &MarkerDetector::GetLastFrame() const
{
    return m_frame;
}

const cv::Mat &MarkerDetector::GetLastMask() const
{
    return m_mask;
}

bool MarkerDetector::IsOpen() const
{
    return m_cap.isOpened();
}

int MarkerDetector::GetFrameWidth() const
{
    return m_width;
}

int MarkerDetector::GetFrameHeight() const
{
    return m_height;
}
