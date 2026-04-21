#pragma once
#include "MotionRecorder.h"
#include <geni.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>

class VideoExporter
{
  public:
    // Replays `keyframes` against `skeleton` (each frame's bone poses are applied
    // before rendering). Scene is rendered via the engine's active camera.
    bool Export(const std::vector<Keyframe> &keyframes, Geni::Skeleton *skeleton, int fps, const std::string &path,
                bool mp4);

  private:
    bool SetupFBO(int width, int height);
    cv::Mat ReadFBO(int width, int height);
    void DestroyFBO();

    GLuint m_fbo = 0;
    GLuint m_colorTex = 0;
    GLuint m_depthRbo = 0;
};
