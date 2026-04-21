#include "ui/UIManager.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <glm/mat3x3.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect/aruco_dictionary.hpp>

#include <cstring>

namespace
{
constexpr ImGuiWindowFlags kSidebarFlags = ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoResize |
                                           ImGuiWindowFlags_NoCollapse;

constexpr float kSidebarW = 320.0f;
constexpr float kMenuBarH = 20.0f;
constexpr float kStatusBarH = 24.0f;

const char *const kArucoDicts[] = {"DICT_4X4_50", "DICT_4X4_100", "DICT_5X5_50", "DICT_5X5_100", "DICT_6X6_250"};
const int kArucoDictIds[] = {cv::aruco::DICT_4X4_50, cv::aruco::DICT_4X4_100, cv::aruco::DICT_5X5_50,
                             cv::aruco::DICT_5X5_100, cv::aruco::DICT_6X6_250};
constexpr int kArucoDictCount = static_cast<int>(sizeof(kArucoDicts) / sizeof(kArucoDicts[0]));

const char *const kBindingLabels[] = {"Position", "Position + Rotation", "IK Target (2-bone)"};

// Helper: combo over available bone names writing into a char[N] field.
// Empty dropdown if the model hasn't been loaded yet.
bool BoneCombo(const char *label, char *buffer, size_t bufferSize, const std::vector<std::string> &bones)
{
    bool changed = false;
    if (ImGui::BeginCombo(label, buffer[0] ? buffer : "(none)"))
    {
        if (ImGui::Selectable("(none)", buffer[0] == 0))
        {
            buffer[0] = 0;
            changed = true;
        }
        for (const auto &bone : bones)
        {
            bool selected = std::strcmp(buffer, bone.c_str()) == 0;
            if (ImGui::Selectable(bone.c_str(), selected))
            {
                std::strncpy(buffer, bone.c_str(), bufferSize - 1);
                buffer[bufferSize - 1] = 0;
                changed = true;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}
} // namespace

int UIManager_GetArucoDictId(int index)
{
    if (index < 0 || index >= kArucoDictCount)
        return kArucoDictIds[0];
    return kArucoDictIds[index];
}

bool UIManager::Init(GLFWwindow *window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

void UIManager::Destroy()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UIManager::BuildUI(UIState &state, const std::vector<MarkerObservation> &observations, const glm::mat4 &viewMatrix)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    DrawMenuBar(state);
    DrawSidebar(state, observations);

    ImGuiIO &io = ImGui::GetIO();
    DrawStatusBar(state, io.Framerate, !observations.empty());

    DrawCameraFeedWindow(state, observations);
    DrawAxisGizmo(viewMatrix);

    ImGui::Render();
}

void UIManager::Render()
{
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UIManager::UploadCameraFeed(const cv::Mat &frame, UIState &state)
{
    if (frame.empty())
        return;

    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

    if (state.cameraFeedTexture == 0)
    {
        glGenTextures(1, &state.cameraFeedTexture);
        glBindTexture(GL_TEXTURE_2D, state.cameraFeedTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, state.cameraFeedTexture);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
    glBindTexture(GL_TEXTURE_2D, 0);

    state.feedWidth = rgb.cols;
    state.feedHeight = rgb.rows;
}

void UIManager::DrawMenuBar(UIState &state)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Save JSON"))
                state.saveJsonRequested = true;
            if (ImGui::MenuItem("Export Video"))
                state.exportVideoRequested = true;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void UIManager::DrawSidebar(UIState &state, const std::vector<MarkerObservation> &observations)
{
    ImVec2 display = ImGui::GetIO().DisplaySize;
    float sidebarH = display.y - kMenuBarH - kStatusBarH;
    if (sidebarH < 1.0f)
        sidebarH = 1.0f;

    ImGui::SetNextWindowPos(ImVec2(0, kMenuBarH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kSidebarW, sidebarH), ImGuiCond_Always);
    ImGui::Begin("Sidebar", nullptr, kSidebarFlags);

    if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
        DrawModelSection(state);

    if (ImGui::CollapsingHeader("Detector", ImGuiTreeNodeFlags_DefaultOpen))
        DrawDetectorSection(state);

    if (ImGui::CollapsingHeader("Markers", ImGuiTreeNodeFlags_DefaultOpen))
        DrawMarkersSection(state, observations);

    if (ImGui::CollapsingHeader("Recording"))
        DrawRecordingSection(state);

    if (ImGui::CollapsingHeader("Export"))
        DrawExportSection(state);

    ImGui::End();
}

void UIManager::DrawModelSection(UIState &state)
{
    ImGui::InputText("Path##model_path", state.modelPath, sizeof(state.modelPath));
    if (ImGui::Button("Load Model", ImVec2(-1, 26)))
        state.loadModelRequested = true;

    if (!state.loadedModelPath.empty())
    {
        ImGui::TextDisabled("Loaded: %s", state.loadedModelPath.c_str());
        ImGui::TextDisabled("Bones: %d", static_cast<int>(state.availableBones.size()));
    }
    ImGui::Spacing();
}

void UIManager::DrawDetectorSection(UIState &state)
{
    int kind = static_cast<int>(state.detectorKind);
    if (ImGui::RadioButton("HSV##det_hsv", &kind, 0))
    {
        state.detectorKind = DetectorKind::HSV;
        state.detectorDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("ArUco##det_aruco", &kind, 1))
    {
        state.detectorKind = DetectorKind::ArUco;
        state.detectorDirty = true;
    }

    if (state.detectorKind == DetectorKind::ArUco)
    {
        if (ImGui::Combo("Dictionary##aruco_dict", &state.arucoDictIndex, kArucoDicts, kArucoDictCount))
        {
            state.detectorDirty = true;
        }
        if (ImGui::InputFloat("Marker (m)##aruco_len", &state.arucoMarkerLengthMeters, 0.005f, 0.01f, "%.3f"))
        {
            if (state.arucoMarkerLengthMeters < 0.005f)
                state.arucoMarkerLengthMeters = 0.005f;
            state.detectorDirty = true;
        }
    }
    ImGui::Spacing();
}

void UIManager::DrawMarkersSection(UIState &state, const std::vector<MarkerObservation> &observations)
{
    if (ImGui::Button("+ Add marker"))
    {
        MarkerSlot slot;
        std::snprintf(slot.name, sizeof(slot.name), "marker_%d", static_cast<int>(state.markers.size()));
        slot.arucoTagId = static_cast<int>(state.markers.size());
        state.markers.push_back(slot);
        state.markersDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("- Remove last") && !state.markers.empty())
    {
        state.markers.pop_back();
        state.markersDirty = true;
    }

    for (size_t i = 0; i < state.markers.size(); ++i)
    {
        auto &slot = state.markers[i];
        ImGui::PushID(static_cast<int>(i));

        bool open = ImGui::TreeNodeEx((void *)i, ImGuiTreeNodeFlags_DefaultOpen, "%zu: %s", i, slot.name);
        if (open)
        {
            if (ImGui::InputText("Name##slot_name", slot.name, sizeof(slot.name)))
                state.markersDirty = true;
            if (ImGui::ColorEdit3("Color##slot_color", slot.overlayColor,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
                state.markersDirty = true;

            int observationId = (state.detectorKind == DetectorKind::HSV) ? static_cast<int>(i) : slot.arucoTagId;
            bool matched = false;
            for (const auto &o : observations)
                if (o.id == observationId)
                {
                    matched = true;
                    break;
                }
            ImGui::SameLine();
            ImGui::TextColored(matched ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                               matched ? "detected" : "—");

            if (state.detectorKind == DetectorKind::HSV)
            {
                ImGui::Text("Observation id: %d (row index)", static_cast<int>(i));
                bool changed = false;
                changed |= ImGui::SliderInt("H min##h_min", &slot.hsv.hMin, 0, 179);
                changed |= ImGui::SliderInt("H max##h_max", &slot.hsv.hMax, 0, 179);
                changed |= ImGui::SliderInt("S min##s_min", &slot.hsv.sMin, 0, 255);
                changed |= ImGui::SliderInt("S max##s_max", &slot.hsv.sMax, 0, 255);
                changed |= ImGui::SliderInt("V min##v_min", &slot.hsv.vMin, 0, 255);
                changed |= ImGui::SliderInt("V max##v_max", &slot.hsv.vMax, 0, 255);
                if (changed)
                    state.markersDirty = true;
            }
            else
            {
                if (ImGui::InputInt("Tag id##aruco_id", &slot.arucoTagId))
                    state.markersDirty = true;
            }

            int bindingIdx = static_cast<int>(slot.binding);
            if (ImGui::Combo("Binding##binding", &bindingIdx, kBindingLabels,
                             IM_ARRAYSIZE(kBindingLabels)))
            {
                slot.binding = static_cast<BindingKind>(bindingIdx);
                state.markersDirty = true;
            }

            if (slot.binding == BindingKind::IKTarget)
            {
                if (BoneCombo("IK Root##ik_root", slot.ikRootBone, sizeof(slot.ikRootBone), state.availableBones))
                    state.markersDirty = true;
                if (BoneCombo("IK Mid##ik_mid", slot.ikMidBone, sizeof(slot.ikMidBone), state.availableBones))
                    state.markersDirty = true;
                if (BoneCombo("End##ik_end", slot.boneName, sizeof(slot.boneName), state.availableBones))
                    state.markersDirty = true;
            }
            else
            {
                if (BoneCombo("Bone##bone", slot.boneName, sizeof(slot.boneName), state.availableBones))
                    state.markersDirty = true;
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::Spacing();
}

void UIManager::DrawRecordingSection(UIState &state)
{
    ImGui::Text("Status: %s", state.isRecording ? "RECORDING" : "Idle");
    ImGui::ProgressBar(state.recordingDuration / 30.0f, ImVec2(-1, 0));
    ImGui::Text("Duration: %.2f s", state.recordingDuration);
    ImGui::Text("Keyframes: %d", state.keyframeCount);

    if (!state.isRecording)
    {
        if (ImGui::Button("START RECORDING", ImVec2(-1, 32)))
            state.startRecordRequested = true;
    }
    else
    {
        if (ImGui::Button("STOP RECORDING", ImVec2(-1, 32)))
            state.stopRecordRequested = true;
    }

    ImGui::Separator();
    if (ImGui::Button("RECONSTRUCT 3D", ImVec2(-1, 28)))
        state.reconstructRequested = true;
    ImGui::Spacing();
}

void UIManager::DrawExportSection(UIState &state)
{
    ImGui::Text("Output Format");
    const char *fmts[] = {"MP4", "AVI"};
    ImGui::Combo("##format", &state.exportFmtIndex, fmts, 2);

    ImGui::SliderInt("FPS##export_fps", &state.exportFps, 1, 60);
    ImGui::InputText("Output Path##export_path", state.exportPath, sizeof(state.exportPath));

    ImGui::Separator();
    if (ImGui::Button("SAVE JSON", ImVec2(-1, 24)))
        state.saveJsonRequested = true;
    if (ImGui::Button("EXPORT VIDEO", ImVec2(-1, 24)))
        state.exportVideoRequested = true;
    ImGui::Spacing();
}

void UIManager::DrawStatusBar(UIState &state, float fps, bool detectedThisFrame)
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - kStatusBarH));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, kStatusBarH));
    ImGui::Begin("##statusbar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::Text("FPS: %.1f | %s | Markers: %s", fps, state.isRecording ? "Recording" : "Idle",
                detectedThisFrame ? "detected" : "none");

    ImGui::End();
}

void UIManager::DrawCameraFeedWindow(UIState &state, const std::vector<MarkerObservation> &observations)
{
    const float W = 340.0f;
    const float H = 260.0f;
    const float margin = 10.0f;

    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x - W - margin, display.y - H - kStatusBarH - margin), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(W, H), ImGuiCond_Always);
    ImGui::Begin("Camera Feed", nullptr, kSidebarFlags);

    if (state.cameraFeedTexture != 0)
    {
        ImVec2 imgSize(W - 20, H - 50);
        ImVec2 imgPos = ImGui::GetCursorScreenPos();
        ImGui::Image((ImTextureID)(uintptr_t)state.cameraFeedTexture, imgSize);

        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(imgPos, ImVec2(imgPos.x + imgSize.x, imgPos.y + imgSize.y), true);

        for (const auto &obs : observations)
        {
            // Match the observation back to a slot for color + label.
            const MarkerSlot *slot = nullptr;
            for (size_t i = 0; i < state.markers.size(); ++i)
            {
                int id = (state.detectorKind == DetectorKind::HSV) ? static_cast<int>(i) : state.markers[i].arucoTagId;
                if (id == obs.id)
                {
                    slot = &state.markers[i];
                    break;
                }
            }

            ImU32 col = slot ? IM_COL32(static_cast<int>(slot->overlayColor[0] * 255.0f),
                                        static_cast<int>(slot->overlayColor[1] * 255.0f),
                                        static_cast<int>(slot->overlayColor[2] * 255.0f), 255)
                             : IM_COL32(220, 220, 220, 255);

            ImVec2 center(imgPos.x + obs.centroidNorm.x * imgSize.x, imgPos.y + obs.centroidNorm.y * imgSize.y);
            dl->AddCircle(center, 10.0f, IM_COL32(0, 0, 0, 180), 16, 3.0f);
            dl->AddCircle(center, 9.0f, col, 16, 2.0f);
            dl->AddCircleFilled(center, 2.0f, col);
            if (slot)
                dl->AddText(ImVec2(center.x + 8.0f, center.y - 6.0f), col, slot->name);
        }
        dl->PopClipRect();
    }
    else
    {
        ImGui::TextWrapped("Camera not available");
    }

    ImGui::End();
}

void UIManager::DrawAxisGizmo(const glm::mat4 &view)
{
    const float radius = 40.0f;
    const float margin = 20.0f;
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImVec2 center(display.x - radius - margin, kMenuBarH + radius + margin);

    ImDrawList *dl = ImGui::GetForegroundDrawList();
    glm::mat3 rot(view);

    glm::vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    ImU32 colors[3] = {
        IM_COL32(235, 60, 60, 255),
        IM_COL32(60, 220, 60, 255),
        IM_COL32(80, 140, 255, 255),
    };
    const char *labels[3] = {"X", "Y", "Z"};

    dl->AddCircleFilled(center, radius + 6.0f, IM_COL32(20, 20, 20, 160));

    for (int i = 0; i < 3; ++i)
    {
        glm::vec3 v = rot * axes[i];
        ImVec2 tip(center.x + v.x * radius, center.y - v.y * radius);
        dl->AddLine(center, tip, colors[i], 2.5f);
        dl->AddCircleFilled(tip, 3.0f, colors[i]);
        dl->AddText(ImVec2(tip.x + 4, tip.y - 7), colors[i], labels[i]);
    }
}
