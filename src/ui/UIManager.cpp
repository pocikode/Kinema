#include "ui/UIManager.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <opencv2/imgproc.hpp>

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

void UIManager::BuildUI(UIState &state, const MarkerResult &detectionResult, bool detectedThisFrame)
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

    if (state.showCameraFeed)
    {
        DrawCameraFeedWindow(state);
    }

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
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Camera Feed", nullptr, &state.showCameraFeed);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void UIManager::DrawMarkerConfigPanel(UIState &state, const MarkerResult &result)
{
    ImGui::SetNextWindowPos(ImVec2(10, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 280), ImGuiCond_FirstUseEver);
    ImGui::Begin("Marker Configuration");

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
        ImGui::Text("Area: %.0f px²", result.areaPixels);
    }

    ImGui::End();
}

void UIManager::DrawRecordingPanel(UIState &state)
{
    ImGui::SetNextWindowPos(ImVec2(10, 350), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Recording Control");

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
    ImGui::SetNextWindowPos(ImVec2(10, 570), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 220), ImGuiCond_FirstUseEver);
    ImGui::Begin("Export Settings");

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
    ImGui::SetNextWindowPos(ImVec2(320, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 280), ImGuiCond_FirstUseEver);
    ImGui::Begin("Camera Feed");

    if (state.cameraFeedTexture != 0)
    {
        ImGui::Image((ImTextureID)(uintptr_t)state.cameraFeedTexture,
                     ImVec2(state.feedWidth * 0.5f, state.feedHeight * 0.5f));
        ImGui::Text("Live Feed");
    }
    else
    {
        ImGui::Text("Camera not available");
    }

    ImGui::End();
}
