#include "modules/MarkerDetection.h"

bool IMarkerDetector::Init(int cameraIndex)
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

void IMarkerDetector::Destroy()
{
    if (m_cap.isOpened())
    {
        m_cap.release();
    }
}

bool IMarkerDetector::GrabFrame()
{
    if (!m_cap.isOpened())
    {
        return false;
    }
    m_cap >> m_frame;
    return !m_frame.empty();
}

const cv::Mat &IMarkerDetector::GetLastFrame() const
{
    return m_frame;
}

const cv::Mat &IMarkerDetector::GetLastMask() const
{
    return m_mask;
}

bool IMarkerDetector::IsOpen() const
{
    return m_cap.isOpened();
}

int IMarkerDetector::GetFrameWidth() const
{
    return m_width;
}

int IMarkerDetector::GetFrameHeight() const
{
    return m_height;
}
