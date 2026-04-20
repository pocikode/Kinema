#pragma once
#include "modules/MarkerDetector.h"
#include "modules/MotionRecorder.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <string>

struct UIState
{
    // Marker config
    HSVRange hsvRange;
    bool hsvDirty = false;

    // Recording
    bool isRecording = false;
    float recordingDuration = 0.0f;
    int keyframeCount = 0;
    bool startRecordRequested = false;
    bool stopRecordRequested = false;
    bool reconstructRequested = false;

    // Export
    int exportFmtIndex = 0;
    int exportFps = 30;
    char exportPath[256] = "output";
    bool saveJsonRequested = false;
    bool exportVideoRequested = false;

    // Camera feed texture
    GLuint cameraFeedTexture = 0;
    int feedWidth = 0;
    int feedHeight = 0;
    bool showCameraFeed = false;
};

class UIManager
{
  public:
    bool Init(GLFWwindow *window);
    void Destroy();

    void BuildUI(UIState &state, const MarkerResult &detectionResult, bool detectedThisFrame);
    void Render();
    void UploadCameraFeed(const cv::Mat &frame, UIState &state);

  private:
    void DrawMenuBar(UIState &state);
    void DrawMarkerConfigPanel(UIState &state, const MarkerResult &result);
    void DrawRecordingPanel(UIState &state);
    void DrawExportPanel(UIState &state);
    void DrawStatusBar(UIState &state, float fps, bool detectedThisFrame);
    void DrawCameraFeedWindow(UIState &state);
};

struct GLFWwindow;
