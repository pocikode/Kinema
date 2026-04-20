#pragma once
#include "MotionRecorder.h"
#include <geni.h>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

class VideoExporter
{
  public:
    bool Export(const std::vector<Keyframe> &keyframes, Geni::GameObject *targetObj, int fps,
                const std::string &path, bool mp4);

  private:
    bool SetupFBO(int width, int height);
    cv::Mat ReadFBO(int width, int height);
    void DestroyFBO();

    GLuint m_fbo = 0;
    GLuint m_colorTex = 0;
    GLuint m_depthRbo = 0;
};
