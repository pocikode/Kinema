#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

struct HSVRange
{
    int hMin = 0;
    int hMax = 179;
    int sMin = 50;
    int sMax = 255;
    int vMin = 50;
    int vMax = 255;
};

struct MarkerResult
{
    bool detected = false;
    glm::vec2 centroidNorm = {0, 0};
    float areaPixels = 0.0f;
    cv::Rect boundingBox;
};

class MarkerDetector
{
  public:
    bool Init(int cameraIndex = 0);
    void Destroy();

    MarkerResult Update(const HSVRange &range);

    const cv::Mat &GetLastFrame() const;
    const cv::Mat &GetLastMask() const;

    bool IsOpen() const;
    int GetFrameWidth() const;
    int GetFrameHeight() const;

  private:
    cv::VideoCapture m_cap;
    cv::Mat m_frame;
    cv::Mat m_frameHSV;
    cv::Mat m_mask;
    int m_width = 0;
    int m_height = 0;
};
