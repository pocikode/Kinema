#define GLM_ENABLE_EXPERIMENTAL
#include "Application.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>

bool Application::Init()
{
    m_markerDetector.Init(0);
    SetupScene();
    m_uiManager.Init(Geni::Engine::GetInstance().GetWindow());
    return true;
}

void Application::SetupScene()
{
    auto &engine = Geni::Engine::GetInstance();
    m_scene = new Geni::Scene();
    engine.SetScene(m_scene);

    // Camera with OrbitCameraComponent
    m_cameraObj = m_scene->CreateObject("Camera");
    m_cameraObj->AddComponent(new Geni::CameraComponent());
    auto orbit = new Geni::OrbitCameraComponent();
    orbit->SetTarget(glm::vec3(0, 0.3f, 0));
    orbit->SetDistance(5.0f);
    orbit->SetPitch(25.0f);
    m_cameraObj->AddComponent(orbit);
    m_scene->SetMainCamera(m_cameraObj);

    // Light
    m_lightObj = m_scene->CreateObject("Light");
    m_lightObj->SetPosition(glm::vec3(5, 8, 5));
    auto light = new Geni::LightComponent();
    light->SetColor(glm::vec3(1.0f, 0.95f, 0.9f));
    m_lightObj->AddComponent(light);

    // Load unlit material (used for box, grid, and axes)
    m_unlitMat = Geni::Material::Load("materials/unlit.mat");

    // Target box (the object being tracked)
    auto boxMesh = Geni::Mesh::CreateBox(glm::vec3(0.3f));
    m_targetObj = m_scene->CreateObject("Target");
    m_targetObj->SetPosition(glm::vec3(0, 0.3f, 0));
    m_targetObj->AddComponent(new Geni::MeshComponent(m_unlitMat, boxMesh));

    // XZ Grid: 20×20, 1-unit spacing, ±10 units
    std::vector<glm::vec3> gridPos, gridCol;
    glm::vec3 gridColor = glm::vec3(0.4f);

    for (int i = -10; i <= 10; ++i)
    {
        // X-parallel lines (along Z)
        gridPos.push_back({-10.0f, 0.0f, static_cast<float>(i)});
        gridPos.push_back({10.0f, 0.0f, static_cast<float>(i)});
        gridCol.push_back(gridColor);
        gridCol.push_back(gridColor);

        // Z-parallel lines (along X)
        gridPos.push_back({static_cast<float>(i), 0.0f, -10.0f});
        gridPos.push_back({static_cast<float>(i), 0.0f, 10.0f});
        gridCol.push_back(gridColor);
        gridCol.push_back(gridColor);
    }

    m_gridMesh = Geni::Mesh::CreateLines(gridPos, gridCol);
    m_gridObj = m_scene->CreateObject("Grid");
    m_gridObj->AddComponent(new Geni::MeshComponent(m_unlitMat, m_gridMesh));

    // RGB Coordinate Axes (length 1.5)
    std::vector<glm::vec3> axesPos = {
        {0, 0, 0}, {1.5f, 0, 0}, // X-axis
        {0, 0, 0}, {0, 1.5f, 0}, // Y-axis
        {0, 0, 0}, {0, 0, 1.5f}  // Z-axis
    };
    std::vector<glm::vec3> axesCol = {
        {1, 0, 0}, {1, 0, 0}, // Red X
        {0, 1, 0}, {0, 1, 0}, // Green Y
        {0, 0, 1}, {0, 0, 1}  // Blue Z
    };

    m_axesMesh = Geni::Mesh::CreateLines(axesPos, axesCol);
    m_axesObj = m_scene->CreateObject("Axes");
    m_axesObj->AddComponent(new Geni::MeshComponent(m_unlitMat, m_axesMesh));
}

glm::vec3 Application::Unproject2DtoWorld(const MarkerResult &result, int frameW, int frameH)
{
    // Normalize centroid to [-1, 1] with Y flipped
    float worldX = (result.centroidNorm.x - 0.5f) * 2.0f;
    float worldY = -(result.centroidNorm.y - 0.5f) * 2.0f;

    // Estimate depth from bounding box area
    // Depth ∝ 1/sqrt(area) because projected area ∝ 1/z²
    float worldZ = m_depthRefDist * glm::sqrt(m_depthRefArea / (result.areaPixels + 1.0f));

    // Scale X and Y by perspective (depth affects field size)
    worldX *= (worldZ / m_depthRefDist);
    worldY *= (worldZ / m_depthRefDist);

    return glm::vec3(worldX, worldY, worldZ);
}

void Application::Update(float deltaTime)
{
    m_fps = 1.0f / (deltaTime > 0 ? deltaTime : 1e-4f);

    // Update scene (camera, physics, etc.)
    m_scene->Update(deltaTime);

    // Marker detection
    m_lastMarkerResult = m_markerDetector.Update(m_uiState.hsvRange);
    m_markerDetectedThisFrame = m_lastMarkerResult.detected;

    if (m_lastMarkerResult.detected)
    {
        glm::vec3 pos3d =
            Unproject2DtoWorld(m_lastMarkerResult, m_markerDetector.GetFrameWidth(), m_markerDetector.GetFrameHeight());
        m_targetObj->SetPosition(pos3d);

        // Record if active
        if (m_recorder.IsRecording())
        {
            m_recorder.AddKeyframe(pos3d, glm::quat(1, 0, 0, 0));
        }
    }

    // Update UI state from recorder
    m_uiState.isRecording = m_recorder.IsRecording();
    m_uiState.keyframeCount = m_recorder.GetKeyframeCount();
    m_uiState.recordingDuration = m_recorder.GetDuration();

    // Always upload camera feed (panel is always visible now)
    if (m_markerDetector.IsOpen())
    {
        m_uiManager.UploadCameraFeed(m_markerDetector.GetLastFrame(), m_uiState);
    }

    // Build ImGui command lists
    auto *cam = m_cameraObj->GetComponent<Geni::CameraComponent>();
    glm::mat4 view = cam->GetViewMatrix();
    m_uiManager.BuildUI(m_uiState, m_lastMarkerResult, m_markerDetectedThisFrame, view);

    // Handle button requests from UI
    if (m_uiState.startRecordRequested)
    {
        m_recorder.StartRecording();
        m_uiState.startRecordRequested = false;
    }

    if (m_uiState.stopRecordRequested)
    {
        m_recorder.StopRecording();
        m_uiState.stopRecordRequested = false;
    }

    if (m_uiState.reconstructRequested)
    {
        m_playFrame = 0;
        m_playback = true;
        m_uiState.reconstructRequested = false;
    }

    if (m_uiState.saveJsonRequested)
    {
        std::string path = std::string(m_uiState.exportPath) + ".json";
        m_recorder.SaveToJson(path);
        m_uiState.saveJsonRequested = false;
    }

    if (m_uiState.exportVideoRequested)
    {
        std::string ext = (m_uiState.exportFmtIndex == 0) ? ".mp4" : ".avi";
        std::string path = std::string(m_uiState.exportPath) + ext;
        m_exporter.Export(
            m_recorder.GetKeyframes(), m_targetObj, m_uiState.exportFps, path, m_uiState.exportFmtIndex == 0
        );
        m_uiState.exportVideoRequested = false;
    }

    // Reconstruction playback: advance through keyframes
    if (m_playback && !m_recorder.GetKeyframes().empty())
    {
        const auto &keyframes = m_recorder.GetKeyframes();
        if (m_playFrame < (int)keyframes.size())
        {
            const auto &kf = keyframes[m_playFrame];
            m_targetObj->SetPosition(kf.position);
            m_targetObj->SetRotation(kf.rotation);
            m_playFrame++;
        }
        else
        {
            m_playback = false;
        }
    }
}

void Application::Render()
{
    m_uiManager.Render();
}

void Application::Destroy()
{
    m_uiManager.Destroy();
    m_markerDetector.Destroy();
}
