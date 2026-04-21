#pragma once

#include "modules/MarkerDetection.h"

#include <opencv2/objdetect/aruco_detector.hpp>

// ArUco fiducial detector. Uses OpenCV 4.7+ object-oriented detector API (in
// opencv_objdetect, no opencv_contrib dependency required).
//
// Tag id is carried through to `MarkerObservation::id`.
// Orientation is recovered via `solvePnP` using approximate intrinsics derived
// from the frame size when no explicit calibration has been provided.
class ArucoMarkerDetector : public IMarkerDetector
{
  public:
    ArucoMarkerDetector();

    void SetDictionary(int dictionaryId); // e.g. cv::aruco::DICT_4X4_50
    void SetMarkerLengthMeters(float meters);

    bool Init(int cameraIndex) override;
    std::vector<MarkerObservation> Detect() override;

  private:
    cv::aruco::ArucoDetector m_detector;
    int m_dictionaryId;
    float m_markerLengthMeters = 0.05f; // 5 cm printed tags by default

    cv::Mat m_cameraMatrix;
    cv::Mat m_distCoeffs;

    void RebuildIntrinsics();
};
