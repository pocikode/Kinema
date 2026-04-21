#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <vector>

// Single sighting of a marker in the most recent camera frame.
// `id` disambiguates observations when multiple markers are tracked:
//   - HSV detector: `id` is the index of the configured HSVRange that produced the blob
//   - ArUco detector: `id` is the unique tag id decoded from the fiducial
// Orientation is only populated by detectors that can recover it (ArUco does; HSV cannot).
struct MarkerObservation
{
    int id = -1;
    glm::vec2 centroidNorm = {0.0f, 0.0f};
    float areaPixels = 0.0f;
    cv::Rect boundingBox;

    bool hasOrientation = false;
    glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};

// Camera backbone shared by every concrete detector: opens the capture device,
// pulls frames, and exposes them for UI preview. Subclasses only need to implement Detect().
class IMarkerDetector
{
  public:
    virtual ~IMarkerDetector() = default;

    virtual bool Init(int cameraIndex);
    virtual void Destroy();

    virtual std::vector<MarkerObservation> Detect() = 0;

    const cv::Mat &GetLastFrame() const;
    const cv::Mat &GetLastMask() const;
    bool IsOpen() const;
    int GetFrameWidth() const;
    int GetFrameHeight() const;

  protected:
    bool GrabFrame();

    cv::VideoCapture m_cap;
    cv::Mat m_frame;
    cv::Mat m_mask;
    int m_width = 0;
    int m_height = 0;
};
