#pragma once
#include <geni.h>
#include "modules/MarkerDetector.h"
#include "modules/MotionRecorder.h"
#include "modules/VideoExporter.h"
#include "ui/UIManager.h"

class Application : public Geni::Application
{
  public:
    bool Init() override;
    void Update(float deltaTime) override;
    void Render() override;
    void Destroy() override;

  private:
    void SetupScene();
    glm::vec3 Unproject2DtoWorld(const MarkerResult &result, int frameW, int frameH);

    // Modules
    MarkerDetector m_markerDetector;
    MotionRecorder m_recorder;
    VideoExporter m_exporter;
    UIManager m_uiManager;
    UIState m_uiState;

    // Scene objects (non-owning pointers — owned by Scene)
    Geni::Scene *m_scene = nullptr;
    Geni::GameObject *m_targetObj = nullptr;
    Geni::GameObject *m_cameraObj = nullptr;
    Geni::GameObject *m_lightObj = nullptr;
    Geni::GameObject *m_gridObj = nullptr;
    Geni::GameObject *m_axesObj = nullptr;

    // Materials/Meshes (owning shared_ptrs)
    std::shared_ptr<Geni::Material> m_unlitMat;
    std::shared_ptr<Geni::Mesh> m_gridMesh;
    std::shared_ptr<Geni::Mesh> m_axesMesh;

    // Runtime state
    float m_fps = 0.0f;
    int m_playFrame = 0;
    bool m_playback = false;
    MarkerResult m_lastMarkerResult;
    bool m_markerDetectedThisFrame = false;

    // Calibration for 2D→3D mapping
    float m_depthRefDist = 2.0f;
    float m_depthRefArea = 10000.0f;
};
