#pragma once
#include "modules/MarkerDetection.h"
#include "modules/MarkerDetector.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>
#include <imgui.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>

enum class DetectorKind
{
    HSV,
    ArUco,
};

// How a marker slot drives its bound bone.
enum class BindingKind
{
    Position,         // bone.worldPos = unproject(marker)
    PositionRotation, // bone.worldPos + bone.worldRot (ArUco only)
    IKTarget,         // marker is end-effector; ikRoot + ikMid posed via analytical 2-bone IK
};

// One user-configured marker. For HSV, matches by slot index. For ArUco, matches by tag id.
struct MarkerSlot
{
    char name[32] = "marker";
    float overlayColor[4] = {0.2f, 1.0f, 0.4f, 1.0f};

    HSVRange hsv;
    int arucoTagId = 0;

    BindingKind binding = BindingKind::Position;
    char boneName[64] = "";   // target bone (end-effector when IKTarget)
    char ikRootBone[64] = ""; // IKTarget only
    char ikMidBone[64] = "";  // IKTarget only
};

struct UIState
{
    // Detector configuration
    DetectorKind detectorKind = DetectorKind::HSV;
    int arucoDictIndex = 0;                // index into kArucoDicts (defined in UIManager.cpp)
    float arucoMarkerLengthMeters = 0.05f; // 5 cm default
    bool detectorDirty = false;            // set when detector kind / aruco params change

    // Marker slots
    std::vector<MarkerSlot> markers;
    bool markersDirty = false; // set when any slot config changes (bindings, HSV ranges, etc.)

    // Model import
    char modelPath[512] = "models/mixamo_lowpoly/mixamo-animated-lowpoly.glb";
    std::string loadedModelPath;
    std::vector<std::string> availableBones;
    bool loadModelRequested = false;

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

    void BuildUI(UIState &state, const std::vector<MarkerObservation> &observations, const glm::mat4 &viewMatrix);
    void Render();
    void UploadCameraFeed(const cv::Mat &frame, UIState &state);

  private:
    void DrawMenuBar(UIState &state);
    void DrawSidebar(UIState &state, const std::vector<MarkerObservation> &observations);
    void DrawDetectorSection(UIState &state);
    void DrawMarkersSection(UIState &state, const std::vector<MarkerObservation> &observations);
    void DrawModelSection(UIState &state);
    void DrawRecordingSection(UIState &state);
    void DrawExportSection(UIState &state);
    void DrawStatusBar(UIState &state, float fps, bool detectedThisFrame);
    void DrawCameraFeedWindow(UIState &state, const std::vector<MarkerObservation> &observations);
    void DrawAxisGizmo(const glm::mat4 &viewMatrix);
};

struct GLFWwindow;
