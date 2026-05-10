#define GLM_ENABLE_EXPERIMENTAL
#include "Application.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>

namespace
{
std::shared_ptr<Geni::Skeleton> FindSkeleton(Geni::GameObject *root)
{
    if (!root)
        return nullptr;
    if (auto *skin = root->GetComponent<Geni::SkinnedMeshComponent>())
    {
        return skin->GetSkeleton();
    }
    for (const auto &child : root->GetChildren())
    {
        if (auto s = FindSkeleton(child.get()))
            return s;
    }
    return nullptr;
}

void CollectBoneNames(Geni::GameObject *root, std::vector<std::string> &out)
{
    if (!root)
        return;
    out.push_back(root->GetName());
    for (const auto &child : root->GetChildren())
    {
        CollectBoneNames(child.get(), out);
    }
}

// Seed the UI with sensible default bindings so a fresh launch with the Mixamo
// smoke rig + three markers drives head + both hands immediately.
MarkerSlot MakeSlot(const char *name, const float color[3], const char *bone,
                    BindingKind kind = BindingKind::Position, const char *ikRoot = "", const char *ikMid = "")
{
    MarkerSlot slot;
    std::strncpy(slot.name, name, sizeof(slot.name) - 1);
    slot.overlayColor[0] = color[0];
    slot.overlayColor[1] = color[1];
    slot.overlayColor[2] = color[2];
    slot.overlayColor[3] = 1.0f;
    std::strncpy(slot.boneName, bone, sizeof(slot.boneName) - 1);
    slot.binding = kind;
    std::strncpy(slot.ikRootBone, ikRoot, sizeof(slot.ikRootBone) - 1);
    std::strncpy(slot.ikMidBone, ikMid, sizeof(slot.ikMidBone) - 1);
    return slot;
}
} // namespace

bool Application::Init()
{
    // Defaults: three markers (head + both hands) targeting Mixamo bones.
    const float red[3] = {1.0f, 0.35f, 0.35f};
    const float green[3] = {0.35f, 1.0f, 0.4f};
    const float blue[3] = {0.4f, 0.6f, 1.0f};

    auto head = MakeSlot("head", red, "mixamorig:Head", BindingKind::LookAt);
    auto leftHand = MakeSlot("hand_L", green, "mixamorig:LeftHand", BindingKind::IKTarget, "mixamorig:LeftArm",
                             "mixamorig:LeftForeArm");
    auto rightHand = MakeSlot("hand_R", blue, "mixamorig:RightHand", BindingKind::IKTarget, "mixamorig:RightArm",
                               "mixamorig:RightForeArm");

    // Narrow default HSV bands so ambient color noise in the webcam doesn't register
    // as a marker. Users can widen from the UI once their real markers are in frame.
    // Red wraps the hue circle, so we expose slot 0 at the high end; the user can
    // add a second range via the UI if their lighting pushes red to the low end.
    head.hsv = {160, 179, 150, 255, 80, 255};
    leftHand.hsv = {40, 80, 150, 255, 80, 255};
    rightHand.hsv = {100, 130, 150, 255, 80, 255};

    m_uiState.markers = {head, leftHand, rightHand};
    m_uiState.markersDirty = true;
    m_uiState.loadModelRequested = true;

    // Default export filename: kinema_YYYYMMDD — user can overwrite from the UI.
    {
        std::time_t t = std::time(nullptr);
        std::tm tmLocal{};
#if defined(_WIN32)
        localtime_s(&tmLocal, &t);
#else
        localtime_r(&t, &tmLocal);
#endif
        std::strftime(m_uiState.exportName, sizeof(m_uiState.exportName), "kinema_%Y%m%d", &tmLocal);
    }
    if (const char *home = std::getenv("HOME"))
    {
        std::strncpy(m_uiState.exportDir, home, sizeof(m_uiState.exportDir) - 1);
        m_uiState.exportDir[sizeof(m_uiState.exportDir) - 1] = 0;
    }

    SetupScene();
    RebuildDetectorFromState();
    m_uiManager.Init(Geni::Engine::GetInstance().GetWindow());
    return true;
}

void Application::SetupScene()
{
    auto &engine = Geni::Engine::GetInstance();
    m_scene = new Geni::Scene();
    engine.SetScene(m_scene);

    m_cameraObj = m_scene->CreateObject("Camera");
    m_cameraObj->AddComponent(new Geni::CameraComponent());
    auto orbit = new Geni::OrbitCameraComponent();
    orbit->SetTarget(glm::vec3(0, 1.0f, 0));
    orbit->SetDistance(4.0f);
    orbit->SetPitch(15.0f);
    m_cameraObj->AddComponent(orbit);
    m_scene->SetMainCamera(m_cameraObj);

    m_lightObj = m_scene->CreateObject("Light");
    m_lightObj->SetPosition(glm::vec3(5, 8, 5));
    auto light = new Geni::LightComponent();
    light->SetColor(glm::vec3(1.0f, 0.95f, 0.9f));
    m_lightObj->AddComponent(light);

    m_unlitMat = Geni::Material::Load("materials/unlit.mat");

    std::vector<glm::vec3> gridPos, gridCol;
    glm::vec3 gridColor(0.4f);
    for (int i = -10; i <= 10; ++i)
    {
        gridPos.push_back({-10.0f, 0.0f, static_cast<float>(i)});
        gridPos.push_back({10.0f, 0.0f, static_cast<float>(i)});
        gridCol.push_back(gridColor);
        gridCol.push_back(gridColor);
        gridPos.push_back({static_cast<float>(i), 0.0f, -10.0f});
        gridPos.push_back({static_cast<float>(i), 0.0f, 10.0f});
        gridCol.push_back(gridColor);
        gridCol.push_back(gridColor);
    }
    m_gridMesh = Geni::Mesh::CreateLines(gridPos, gridCol);
    m_gridObj = m_scene->CreateObject("Grid");
    m_gridObj->AddComponent(new Geni::MeshComponent(m_unlitMat, m_gridMesh));

    std::vector<glm::vec3> axesPos = {
        {0, 0, 0}, {1.5f, 0, 0}, {0, 0, 0}, {0, 1.5f, 0}, {0, 0, 0}, {0, 0, 1.5f},
    };
    std::vector<glm::vec3> axesCol = {
        {1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 1},
    };
    m_axesMesh = Geni::Mesh::CreateLines(axesPos, axesCol);
    m_axesObj = m_scene->CreateObject("Axes");
    m_axesObj->AddComponent(new Geni::MeshComponent(m_unlitMat, m_axesMesh));
}

void Application::LoadRigFromState()
{
    // Destroy any previously loaded rig before loading its replacement.
    if (m_riggedObj)
    {
        m_riggedObj->MarkForDestroy();
        m_riggedObj = nullptr;
        m_riggedSkeleton.reset();
        m_uiState.availableBones.clear();
    }

    m_riggedObj = Geni::GameObject::LoadGLTF(m_uiState.modelPath);
    if (!m_riggedObj)
    {
        m_uiState.loadedModelPath = std::string("FAILED: ") + m_uiState.modelPath;
        return;
    }

    m_riggedObj->SetPosition(glm::vec3(0.0f));
    m_riggedSkeleton = FindSkeleton(m_riggedObj);
    m_uiState.loadedModelPath = m_uiState.modelPath;
    m_uiState.availableBones.clear();
    CollectBoneNames(m_riggedObj, m_uiState.availableBones);
    m_uiState.markersDirty = true; // re-validate bindings against the new skeleton
}

void Application::RebuildDetectorFromState()
{
    if (m_detector)
        m_detector->Destroy();

    auto hsv = std::make_unique<HSVMarkerDetector>();
    m_detector = std::move(hsv);
    m_detector->Init(0);
}

void Application::RebuildBindingsFromState()
{
    std::vector<MarkerBinding> bindings;
    std::vector<IKChain> chains;

    for (size_t i = 0; i < m_uiState.markers.size(); ++i)
    {
        const auto &slot = m_uiState.markers[i];
        int observationId = static_cast<int>(i);

        if (slot.binding == BindingKind::IKTarget)
        {
            if (slot.ikRootBone[0] == 0 || slot.ikMidBone[0] == 0 || slot.boneName[0] == 0)
                continue;
            IKChain chain;
            chain.markerId = observationId;
            chain.rootBoneName = slot.ikRootBone;
            chain.midBoneName = slot.ikMidBone;
            chain.endBoneName = slot.boneName;
            chain.poleHint = glm::vec3(0.0f, -1.0f, 0.0f);
            chains.push_back(chain);
        }
        else
        {
            if (slot.boneName[0] == 0)
                continue;
            MarkerBinding b;
            b.markerId = observationId;
            b.boneName = slot.boneName;
            b.mode = (slot.binding == BindingKind::LookAt) ? MarkerBinding::Mode::LookAt
                                                           : MarkerBinding::Mode::Position;
            bindings.push_back(b);
        }
    }

    m_skelDriver.SetBindings(std::move(bindings));
    m_skelDriver.SetIKChains(std::move(chains));
}

glm::vec3 Application::Unproject2DtoWorld(const glm::vec2 &centroidNorm, float areaPixels)
{
    float worldX = (centroidNorm.x - 0.5f) * 2.0f;
    float worldY = -(centroidNorm.y - 0.5f) * 2.0f;
    float worldZ = m_depthRefDist * glm::sqrt(m_depthRefArea / (areaPixels + 1.0f));
    worldX *= (worldZ / m_depthRefDist);
    worldY *= (worldZ / m_depthRefDist);
    return glm::vec3(worldX, worldY, worldZ);
}

void Application::Update(float deltaTime)
{
    m_fps = 1.0f / (deltaTime > 0 ? deltaTime : 1e-4f);

    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)
    {
        auto &im = Geni::Engine::GetInstance().GetInputManager();
        im.SetMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT, false);
        im.SetMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT, false);
        im.SetMousePositionOld(im.GetMousePositionCurrent());
        im.SetScrollDelta(0.0f);
    }

    m_scene->Update(deltaTime);

    // React to UI-driven state changes.
    if (m_uiState.loadModelRequested)
    {
        LoadRigFromState();
        m_uiState.loadModelRequested = false;
    }
    if (m_uiState.resetModelRequested)
    {
        m_playback = false;
        m_playbackTime = 0.0f;
        if (m_riggedObj && !m_uiState.loadedModelPath.empty())
        {
            char savedPath[512];
            strncpy(savedPath, m_uiState.modelPath, sizeof(savedPath));
            strncpy(m_uiState.modelPath, m_uiState.loadedModelPath.c_str(), sizeof(m_uiState.modelPath));
            LoadRigFromState();
            strncpy(m_uiState.modelPath, savedPath, sizeof(m_uiState.modelPath));
        }
        m_uiState.resetModelRequested = false;
    }
    if (m_uiState.markersDirty)
    {
        RebuildBindingsFromState();
        m_uiState.markersDirty = false;
    }

    // Each frame, sync per-slot HSV ranges into the detector — cheap and lets the
    // slider feedback be immediate without needing an explicit "apply" press.
    if (auto *hsv = dynamic_cast<HSVMarkerDetector *>(m_detector.get()))
    {
        std::vector<HSVRange> ranges;
        ranges.reserve(m_uiState.markers.size());
        for (const auto &slot : m_uiState.markers)
            ranges.push_back(slot.hsv);
        hsv->SetRanges(ranges);
    }

    std::vector<MarkerObservation> observations;
    if (m_detector)
        observations = m_detector->Detect();

    if (m_playback && m_riggedSkeleton)
    {
        // Playback overrides live detection so the recorded motion renders untouched.
        const auto &keyframes = m_recorder.GetKeyframes();
        if (keyframes.empty())
        {
            m_playback = false;
        }
        else
        {
            m_playbackTime += deltaTime;
            size_t idx = 0;
            while (idx + 1 < keyframes.size() && keyframes[idx + 1].timestamp <= m_playbackTime)
                ++idx;
            MotionRecorder::ApplyKeyframe(keyframes[idx], *m_riggedSkeleton);
            if (m_playbackTime >= keyframes.back().timestamp)
                m_playback = false;
        }
    }
    else if (m_riggedSkeleton)
    {
        m_skelDriver.Apply(*m_riggedSkeleton, observations, [this](const MarkerObservation &o) {
            return Unproject2DtoWorld(o.centroidNorm, o.areaPixels);
        });

        if (m_recorder.IsRecording())
            m_recorder.AddSkeletonKeyframe(*m_riggedSkeleton);
    }

    m_uiState.isRecording = m_recorder.IsRecording();
    m_uiState.keyframeCount = m_recorder.GetKeyframeCount();
    m_uiState.recordingDuration = m_recorder.GetDuration();

    if (m_detector && m_detector->IsOpen())
    {
        m_uiManager.UploadCameraFeed(m_detector->GetLastFrame(), m_uiState);
    }

    auto *cam = m_cameraObj->GetComponent<Geni::CameraComponent>();
    glm::mat4 view = cam->GetViewMatrix();
    m_uiManager.BuildUI(m_uiState, observations, view);

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
        m_playbackTime = 0.0f;
        m_playback = !m_recorder.GetKeyframes().empty();
        m_uiState.reconstructRequested = false;
    }
    auto joinPath = [this](const char *ext) {
        std::string p = m_uiState.exportDir;
        if (!p.empty() && p.back() != '/' && p.back() != '\\')
            p += '/';
        return p + m_uiState.exportName + ext;
    };

    if (m_uiState.saveJsonRequested)
    {
        m_recorder.SaveToJson(joinPath(".json"));
        m_uiState.saveJsonRequested = false;
    }
    if (m_uiState.exportVideoRequested)
    {
        const char *ext = (m_uiState.exportFmtIndex == 0) ? ".mp4" : ".avi";
        m_exporter.Export(m_recorder.GetKeyframes(), m_riggedSkeleton.get(), m_uiState.exportFps, joinPath(ext),
                          m_uiState.exportFmtIndex == 0);
        m_uiState.exportVideoRequested = false;
    }
    if (m_uiState.exportGlbRequested)
    {
        if (m_riggedSkeleton && !m_uiState.loadedModelPath.empty())
        {
            // loadedModelPath is the same relative string LoadGLTF was handed; cgltf_parse_file
            // uses fopen() so we must resolve it against the assets folder ourselves. An
            // absolute right-hand operand wins, so user-entered absolute paths still work.
            std::filesystem::path src = m_uiState.loadedModelPath;
            if (src.is_relative())
                src = Geni::Engine::GetInstance().GetFileSystem().GetAssetsFolder() / src;
            m_glbExporter.Export(src.string(), m_recorder.GetKeyframes(), *m_riggedSkeleton,
                                 joinPath(".glb"));
        }
        m_uiState.exportGlbRequested = false;
    }
}

void Application::Render()
{
    m_uiManager.Render();
}

void Application::Destroy()
{
    m_uiManager.Destroy();
    if (m_detector)
    {
        m_detector->Destroy();
    }
}
