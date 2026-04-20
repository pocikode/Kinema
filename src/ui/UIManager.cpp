#include "ui/UIManager.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <glm/mat3x3.hpp>
#include <opencv2/imgproc.hpp>

namespace
{
constexpr ImGuiWindowFlags kSidebarFlags = ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoResize |
                                           ImGuiWindowFlags_NoCollapse;

constexpr float kSidebarW = 300.0f;
constexpr float kMenuBarH = 20.0f;
constexpr float kMarkerH = 280.0f;
constexpr float kRecordH = 200.0f;
constexpr float kExportH = 240.0f;
} // namespace

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

void UIManager::BuildUI(UIState &state, const MarkerResult &detectionResult,
                        bool detectedThisFrame, const glm::mat4 &viewMatrix)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    DrawMenuBar(state);
    DrawMarkerConfigPanel(state, detectionResult);
    DrawRecordingPanel(state);
    DrawExportPanel(state);

    ImGuiIO &io = ImGui::GetIO();
    DrawStatusBar(state, io.Framerate, detectedThisFrame);

    DrawCameraFeedWindow(state);
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

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 rgb.data);
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
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
            {
                // Will be handled by application
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void UIManager::DrawMarkerConfigPanel(UIState &state, const MarkerResult &result)
{
    ImGui::SetNextWindowPos(ImVec2(0, kMenuBarH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kSidebarW, kMarkerH), ImGuiCond_Always);
    ImGui::Begin("Marker Configuration", nullptr, kSidebarFlags);

    ImGui::Text("HSV Range");
    state.hsvDirty |= ImGui::SliderInt("Hue Min##h_min", &state.hsvRange.hMin, 0, 179);
    state.hsvDirty |= ImGui::SliderInt("Hue Max##h_max", &state.hsvRange.hMax, 0, 179);
    state.hsvDirty |= ImGui::SliderInt("Sat Min##s_min", &state.hsvRange.sMin, 0, 255);
    state.hsvDirty |= ImGui::SliderInt("Sat Max##s_max", &state.hsvRange.sMax, 0, 255);
    state.hsvDirty |= ImGui::SliderInt("Val Min##v_min", &state.hsvRange.vMin, 0, 255);
    state.hsvDirty |= ImGui::SliderInt("Val Max##v_max", &state.hsvRange.vMax, 0, 255);

    ImGui::Separator();
    ImGui::Text("Detection Stats");
    ImGui::Text("Status: %s", result.detected ? "DETECTED" : "Not found");
    if (result.detected)
    {
        ImGui::Text("Centroid: (%.3f, %.3f)", result.centroidNorm.x, result.centroidNorm.y);
        ImGui::Text("Area: %.0f px", result.areaPixels);
    }

    ImGui::End();
}

void UIManager::DrawRecordingPanel(UIState &state)
{
    ImGui::SetNextWindowPos(ImVec2(0, kMenuBarH + kMarkerH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kSidebarW, kRecordH), ImGuiCond_Always);
    ImGui::Begin("Recording Control", nullptr, kSidebarFlags);

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

    ImGui::End();
}

void UIManager::DrawExportPanel(UIState &state)
{
    ImGui::SetNextWindowPos(ImVec2(0, kMenuBarH + kMarkerH + kRecordH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kSidebarW, kExportH), ImGuiCond_Always);
    ImGui::Begin("Export Settings", nullptr, kSidebarFlags);

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

    ImGui::End();
}

void UIManager::DrawStatusBar(UIState &state, float fps, bool detectedThisFrame)
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - 24));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 24));
    ImGui::Begin("##statusbar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::Text("FPS: %.1f | %s | Marker: %s", fps, state.isRecording ? "Recording" : "Idle",
                detectedThisFrame ? "Detected" : "Not found");

    ImGui::End();
}

void UIManager::DrawCameraFeedWindow(UIState &state)
{
    const float W = 340.0f;
    const float H = 260.0f;
    const float margin = 10.0f;
    const float statusBarH = 24.0f;

    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(
        ImVec2(display.x - W - margin, display.y - H - statusBarH - margin),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(W, H), ImGuiCond_Always);
    ImGui::Begin("Camera Feed", nullptr, kSidebarFlags);

    if (state.cameraFeedTexture != 0)
    {
        ImGui::Image((ImTextureID)(uintptr_t)state.cameraFeedTexture,
                     ImVec2(W - 20, H - 50));
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
