#pragma once
#include <geni.h>
#include "modules/GlbExporter.h"
#include "modules/HSVMarkerDetector.h"
#include "modules/MarkerDetection.h"
#include "modules/MotionRecorder.h"
#include "modules/SkeletonDriver.h"
#include "modules/VideoExporter.h"
#include "ui/UIManager.h"

#include <memory>

class Application : public Geni::Application
{
  public:
    bool Init() override;
    void Update(float deltaTime) override;
    void Render() override;
    void Destroy() override;

  private:
    void SetupScene();
    void LoadRigFromState();
    void RebuildDetectorFromState();
    void RebuildBindingsFromState();
    glm::vec3 Unproject2DtoWorld(const glm::vec2 &centroidNorm, float areaPixels);

    // Modules
    std::unique_ptr<IMarkerDetector> m_detector;
    MotionRecorder m_recorder;
    VideoExporter m_exporter;
    GlbExporter m_glbExporter;
    UIManager m_uiManager;
    UIState m_uiState;
    SkeletonDriver m_skelDriver;

    // Scene objects (non-owning pointers — owned by Scene)
    Geni::Scene *m_scene = nullptr;
    Geni::GameObject *m_cameraObj = nullptr;
    Geni::GameObject *m_lightObj = nullptr;
    Geni::GameObject *m_gridObj = nullptr;
    Geni::GameObject *m_axesObj = nullptr;
    Geni::GameObject *m_riggedObj = nullptr;
    std::shared_ptr<Geni::Skeleton> m_riggedSkeleton;

    // Materials/Meshes (owning shared_ptrs)
    std::shared_ptr<Geni::Material> m_unlitMat;
    std::shared_ptr<Geni::Mesh> m_gridMesh;
    std::shared_ptr<Geni::Mesh> m_axesMesh;

    // Runtime state
    float m_fps = 0.0f;
    float m_playbackTime = 0.0f;
    bool m_playback = false;

    // Calibration for 2D→3D mapping
    float m_depthRefDist = 2.0f;
    float m_depthRefArea = 10000.0f;
};
