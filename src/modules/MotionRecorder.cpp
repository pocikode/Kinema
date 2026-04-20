#include "modules/MotionRecorder.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

void MotionRecorder::StartRecording()
{
    m_keyframes.clear();
    m_recording = true;
    m_startTime = std::chrono::steady_clock::now();
}

void MotionRecorder::StopRecording()
{
    m_recording = false;
}

bool MotionRecorder::IsRecording() const
{
    return m_recording;
}

void MotionRecorder::AddKeyframe(const glm::vec3 &pos, const glm::quat &rot)
{
    if (!m_recording)
        return;

    double timestamp = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - m_startTime).count();

    m_keyframes.push_back({timestamp, pos, rot});
}

const std::vector<Keyframe> &MotionRecorder::GetKeyframes() const
{
    return m_keyframes;
}

int MotionRecorder::GetKeyframeCount() const
{
    return static_cast<int>(m_keyframes.size());
}

float MotionRecorder::GetDuration() const
{
    if (m_keyframes.empty())
        return 0.0f;
    return static_cast<float>(m_keyframes.back().timestamp);
}

bool MotionRecorder::SaveToJson(const std::string &path) const
{
    try
    {
        json j;
        j["keyframes"] = json::array();

        for (const auto &kf : m_keyframes)
        {
            json kfObj;
            kfObj["t"] = kf.timestamp;
            kfObj["px"] = kf.position.x;
            kfObj["py"] = kf.position.y;
            kfObj["pz"] = kf.position.z;
            kfObj["qw"] = kf.rotation.w;
            kfObj["qx"] = kf.rotation.x;
            kfObj["qy"] = kf.rotation.y;
            kfObj["qz"] = kf.rotation.z;
            j["keyframes"].push_back(kfObj);
        }

        std::ofstream f(path);
        f << j.dump(2);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool MotionRecorder::LoadFromJson(const std::string &path)
{
    try
    {
        std::ifstream f(path);
        if (!f.good())
            return false;

        json j;
        f >> j;

        m_keyframes.clear();
        for (const auto &kfObj : j["keyframes"])
        {
            Keyframe kf;
            kf.timestamp = kfObj.at("t").get<double>();
            kf.position.x = kfObj.at("px").get<float>();
            kf.position.y = kfObj.at("py").get<float>();
            kf.position.z = kfObj.at("pz").get<float>();
            kf.rotation.w = kfObj.at("qw").get<float>();
            kf.rotation.x = kfObj.at("qx").get<float>();
            kf.rotation.y = kfObj.at("qy").get<float>();
            kf.rotation.z = kfObj.at("qz").get<float>();
            m_keyframes.push_back(kf);
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void MotionRecorder::Clear()
{
    m_keyframes.clear();
    m_recording = false;
}
