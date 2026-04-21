#include "modules/ArucoMarkerDetector.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

namespace
{
// Convert a Rodrigues rotation vector (from solvePnP) to a glm quaternion.
glm::quat RodriguesToQuat(const cv::Vec3d &rvec)
{
    cv::Mat R;
    cv::Rodrigues(rvec, R);

    // cv::Mat is row-major; glm::mat3 is column-major.
    glm::mat3 m(static_cast<float>(R.at<double>(0, 0)), static_cast<float>(R.at<double>(1, 0)),
                static_cast<float>(R.at<double>(2, 0)), static_cast<float>(R.at<double>(0, 1)),
                static_cast<float>(R.at<double>(1, 1)), static_cast<float>(R.at<double>(2, 1)),
                static_cast<float>(R.at<double>(0, 2)), static_cast<float>(R.at<double>(1, 2)),
                static_cast<float>(R.at<double>(2, 2)));
    return glm::quat_cast(m);
}
} // namespace

ArucoMarkerDetector::ArucoMarkerDetector() : m_dictionaryId(cv::aruco::DICT_4X4_50)
{
    auto dict = cv::aruco::getPredefinedDictionary(m_dictionaryId);
    cv::aruco::DetectorParameters params;
    m_detector = cv::aruco::ArucoDetector(dict, params);
}

void ArucoMarkerDetector::SetDictionary(int dictionaryId)
{
    m_dictionaryId = dictionaryId;
    auto dict = cv::aruco::getPredefinedDictionary(dictionaryId);
    cv::aruco::DetectorParameters params;
    m_detector = cv::aruco::ArucoDetector(dict, params);
}

void ArucoMarkerDetector::SetMarkerLengthMeters(float meters)
{
    m_markerLengthMeters = meters;
}

bool ArucoMarkerDetector::Init(int cameraIndex)
{
    if (!IMarkerDetector::Init(cameraIndex))
    {
        return false;
    }
    RebuildIntrinsics();
    return true;
}

void ArucoMarkerDetector::RebuildIntrinsics()
{
    // Uncalibrated approximation: assume a ~60° horizontal FOV pinhole with the
    // principal point at the image centre. Good enough for driving a demo; real
    // calibration (cv::calibrateCamera) would improve pose accuracy.
    double fx = static_cast<double>(m_width);
    double fy = static_cast<double>(m_width);
    double cx = m_width * 0.5;
    double cy = m_height * 0.5;

    m_cameraMatrix = (cv::Mat_<double>(3, 3) << fx, 0, cx, 0, fy, cy, 0, 0, 1);
    m_distCoeffs = cv::Mat::zeros(1, 5, CV_64F);
}

std::vector<MarkerObservation> ArucoMarkerDetector::Detect()
{
    std::vector<MarkerObservation> observations;

    if (!GrabFrame())
    {
        return observations;
    }

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    m_detector.detectMarkers(m_frame, corners, ids);

    if (ids.empty())
    {
        return observations;
    }

    // Pose of each tag (rvec, tvec) in camera space.
    // cv::aruco::estimatePoseSingleMarkers is deprecated in 4.7+; solvePnP per marker
    // is the recommended replacement and avoids the deprecation warning.
    const float half = m_markerLengthMeters * 0.5f;
    std::vector<cv::Point3f> objectPoints = {
        {-half, half, 0.0f}, {half, half, 0.0f}, {half, -half, 0.0f}, {-half, -half, 0.0f}};

    for (size_t i = 0; i < ids.size(); ++i)
    {
        cv::Vec3d rvec, tvec;
        cv::solvePnP(objectPoints, corners[i], m_cameraMatrix, m_distCoeffs, rvec, tvec);

        // Centroid for UI overlay / fallback depth estimation.
        cv::Point2f centroid(0.0f, 0.0f);
        for (const auto &c : corners[i])
        {
            centroid += c;
        }
        centroid *= (1.0f / 4.0f);

        MarkerObservation obs;
        obs.id = ids[i];
        obs.centroidNorm = {centroid.x / static_cast<float>(m_width), centroid.y / static_cast<float>(m_height)};
        obs.areaPixels = static_cast<float>(cv::contourArea(corners[i]));
        obs.boundingBox = cv::boundingRect(corners[i]);
        obs.hasOrientation = true;
        obs.orientation = RodriguesToQuat(rvec);
        observations.push_back(obs);
    }

    return observations;
}
