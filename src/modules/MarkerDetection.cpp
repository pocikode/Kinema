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
    // Lock auto white-balance so marker hue/chromaticity stays stable frame to
    // frame. Auto-WB drift silently shifts measured colors, which is a common
    // cause of dropped/swapped marker identities. Best effort: backends that
    // don't support the property (e.g. some macOS AVFoundation paths) ignore it.
    m_cap.set(cv::CAP_PROP_AUTO_WB, 0.0);
    return true;
}

void IMarkerDetector::Destroy()
{
    if (m_cap.isOpened())
    {
        m_cap.release();
    }
}

void IMarkerDetector::SetBlobCounts(const std::vector<int> &counts)
{
    m_blobCounts = counts;
}

int IMarkerDetector::BlobCount(size_t rangeIndex) const
{
    return rangeIndex < m_blobCounts.size() ? m_blobCounts[rangeIndex] : 1;
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
