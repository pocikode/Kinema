#include "modules/VideoExporter.h"
#include <GL/glew.h>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>

bool VideoExporter::SetupFBO(int width, int height)
{
    DestroyFBO();

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_colorTex);
    glBindTexture(GL_TEXTURE_2D, m_colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);

    glGenRenderbuffers(1, &m_depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return status == GL_FRAMEBUFFER_COMPLETE;
}

cv::Mat VideoExporter::ReadFBO(int width, int height)
{
    cv::Mat frame(height, width, CV_8UC3);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, frame.data);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    cv::flip(frame, frame, 0);
    return frame;
}

void VideoExporter::DestroyFBO()
{
    if (m_fbo)
    {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_colorTex)
    {
        glDeleteTextures(1, &m_colorTex);
        m_colorTex = 0;
    }
    if (m_depthRbo)
    {
        glDeleteRenderbuffers(1, &m_depthRbo);
        m_depthRbo = 0;
    }
}

bool VideoExporter::Export(const std::vector<Keyframe> &keyframes, Geni::Skeleton *skeleton,
                           int fps, const std::string &path, bool mp4)
{
    if (!skeleton || keyframes.empty())
    {
        return false;
    }

    int width = 1280;
    int height = 720;

    if (!SetupFBO(width, height))
    {
        std::cerr << "Failed to setup FBO for video export" << std::endl;
        return false;
    }

    // Create video writer
    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    if (!mp4)
    {
        fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    }

    cv::VideoWriter writer(path, fourcc, fps, {width, height});
    if (!writer.isOpened())
    {
        std::cerr << "Failed to open video writer for: " << path << std::endl;
        DestroyFBO();
        return false;
    }

    auto &engine = Geni::Engine::GetInstance();
    auto scene = engine.GetScene();
    if (!scene)
    {
        DestroyFBO();
        return false;
    }

    auto cameraObj = scene->GetMainCamera();
    auto cameraComp = cameraObj ? cameraObj->GetComponent<Geni::CameraComponent>() : nullptr;
    if (!cameraComp)
    {
        DestroyFBO();
        return false;
    }

    const float aspect = static_cast<float>(width) / static_cast<float>(height);

    // The current frame's Application::Update has already submitted live-pose draws
    // into the queue. Drain them once into the FBO (not written to file) so the
    // loop below starts with a clean queue and each exported frame reflects only
    // the applied keyframe pose.
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Geni::CameraData drainCam;
        drainCam.viewMatrix = cameraComp->GetViewMatrix();
        drainCam.projectionMatrix = cameraComp->GetProjectionMatrix(aspect);
        drainCam.position = cameraObj->GetWorldPosition();
        auto lights = scene->CollectLights();
        engine.GetRenderQueue().Draw(engine.GetGraphicsAPI(), drainCam, lights);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Render each keyframe
    for (const auto &kf : keyframes)
    {
        MotionRecorder::ApplyKeyframe(kf, *skeleton);

        // Re-run the scene update so bone GameObjects propagate their new local
        // transforms and MeshComponent / SkinnedMeshComponent submit draws with
        // the freshly built skinning palette for this keyframe.
        scene->Update(0.0f);

        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Geni::CameraData cameraData;
        cameraData.viewMatrix = cameraComp->GetViewMatrix();
        cameraData.projectionMatrix = cameraComp->GetProjectionMatrix(aspect);
        cameraData.position = cameraObj->GetWorldPosition();

        auto lights = scene->CollectLights();
        engine.GetRenderQueue().Draw(engine.GetGraphicsAPI(), cameraData, lights);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        cv::Mat frame = ReadFBO(width, height);
        writer << frame;
    }

    writer.release();
    DestroyFBO();

    // Restore the default viewport so the main window keeps rendering full-size.
    // Without this the next Engine::Run frame inherits our FBO-sized viewport and
    // the 3D scene collapses into a small corner behind the sidebar.
    glm::ivec2 fb = Geni::Engine::GetInstance().GetFramebufferSize();
    glViewport(0, 0, fb.x, fb.y);

    return true;
}
