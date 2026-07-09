#pragma once
#include "modules/GlbExporter.h"
#include "modules/HSVMarkerDetector.h"
#include "modules/MarkerAssigner.h"
#include "modules/MarkerDetection.h"
#include "modules/MarkerStabilizer.h"
#include "modules/MotionRecorder.h"
#include "modules/RGBRatioMarkerDetector.h"
#include "modules/SkeletonDriver.h"
#include "modules/VideoExporter.h"
#include "ui/UIManager.h"
#include <geni.h>

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
    void ApplyEyedropperPick(); // sample the camera frame at the armed slot's clicked point
    glm::vec3 Unproject2DtoWorld(const glm::vec2 &centroidNorm, float areaPixels);
    // Perf debug: drain UI reset/click requests, aggregate FPS/latency while
    // sampling, and score centroid-accuracy clicks against raw observations.
    void UpdatePerfStats(float deltaTime, const std::vector<MarkerObservation> &rawObservations);

    // Modules
    std::unique_ptr<IMarkerDetector> m_detector;
    MotionRecorder m_recorder;
    VideoExporter m_exporter;
    GlbExporter m_glbExporter;
    UIManager m_uiManager;
    UIState m_uiState;
    SkeletonDriver m_skelDriver;
    MarkerAssigner m_assigner;
    MarkerStabilizer m_stabilizer;

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
    float m_cameraRetryTimer = 0.0f;

    // Perf debug accumulators — aggregated into UIState.perf* while sampling
    int m_perfFrames = 0;
    float m_perfDuration = 0.0f;
    float m_perfFpsSum = 0.0f, m_perfFpsMin = 0.0f, m_perfFpsMax = 0.0f;
    float m_perfDetectSumMs = 0.0f, m_perfDetectMaxMs = 0.0f;
    float m_perfPoseSumMs = 0.0f;

    // Centroid accuracy accumulators — aggregated into UIState.centroid*
    int m_centroidSamples = 0;
    float m_centroidErrSumPx = 0.0f, m_centroidErrMinPx = 0.0f, m_centroidErrMaxPx = 0.0f;
};
