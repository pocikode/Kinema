#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <vector>

// Single sighting of a marker in the most recent camera frame.
// `id` is the slot index of the configured HSVRange that produced the blob.
struct MarkerObservation
{
    int id = -1;
    glm::vec2 centroidNorm = {0.0f, 0.0f};
    float areaPixels = 0.0f;
    cv::Rect boundingBox;
    bool predicted = false; // synthesized by the stabilizer during occlusion hold

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

    // Per range index, how many blobs to emit: 0 = skip the range entirely
    // (paired follower slot), 1 = largest only (default), 2 = two largest.
    // Indices past the end of the vector behave as 1.
    void SetBlobCounts(const std::vector<int> &counts);

    const cv::Mat &GetLastFrame() const;
    const cv::Mat &GetLastMask() const;
    bool IsOpen() const;
    int GetFrameWidth() const;
    int GetFrameHeight() const;

  protected:
    bool GrabFrame();
    int BlobCount(size_t rangeIndex) const;

    std::vector<int> m_blobCounts;

    cv::VideoCapture m_cap;
    cv::Mat m_frame;
    cv::Mat m_mask;
    int m_width = 0;
    int m_height = 0;
};
