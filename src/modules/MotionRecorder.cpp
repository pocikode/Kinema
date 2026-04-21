#include "modules/MotionRecorder.h"

#include <fstream>
#include <geni.h>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
constexpr int kSchemaVersion = 2;
}

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

void MotionRecorder::AddSkeletonKeyframe(const Geni::Skeleton &skeleton)
{
    if (!m_recording)
        return;

    Keyframe kf;
    kf.timestamp = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_startTime).count();

    const int count = skeleton.GetJointCount();
    kf.bonePoses.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        Geni::GameObject *node = skeleton.GetJointNode(i);
        if (!node)
            continue;
        BonePose pose;
        pose.position = node->GetPosition();
        pose.rotation = node->GetRotation();
        pose.scale = node->GetScale();
        kf.bonePoses.emplace(node->GetName(), pose);
    }

    m_keyframes.push_back(std::move(kf));
}

void MotionRecorder::ApplyKeyframe(const Keyframe &keyframe, Geni::Skeleton &skeleton)
{
    for (const auto &entry : keyframe.bonePoses)
    {
        int index = skeleton.FindJoint(entry.first);
        if (index < 0)
            continue;
        Geni::GameObject *node = skeleton.GetJointNode(index);
        if (!node)
            continue;
        node->SetPosition(entry.second.position);
        node->SetRotation(entry.second.rotation);
        node->SetScale(entry.second.scale);
    }
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
        j["version"] = kSchemaVersion;
        j["keyframes"] = json::array();

        for (const auto &kf : m_keyframes)
        {
            json kfObj;
            kfObj["t"] = kf.timestamp;
            json bones = json::object();
            for (const auto &entry : kf.bonePoses)
            {
                const auto &p = entry.second.position;
                const auto &q = entry.second.rotation;
                const auto &s = entry.second.scale;
                bones[entry.first] = {{"px", p.x}, {"py", p.y}, {"pz", p.z}, {"qw", q.w}, {"qx", q.x},
                                      {"qy", q.y}, {"qz", q.z}, {"sx", s.x}, {"sy", s.y}, {"sz", s.z}};
            }
            kfObj["bones"] = std::move(bones);
            j["keyframes"].push_back(std::move(kfObj));
        }

        std::ofstream f(path);
        f << j.dump(2);
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "MotionRecorder::SaveToJson failed: " << e.what() << std::endl;
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

        int version = j.value("version", 1);
        if (version != kSchemaVersion)
        {
            std::cerr << "MotionRecorder::LoadFromJson: unsupported schema version " << version << " (expected "
                      << kSchemaVersion << ")" << std::endl;
            return false;
        }

        m_keyframes.clear();
        for (const auto &kfObj : j.at("keyframes"))
        {
            Keyframe kf;
            kf.timestamp = kfObj.at("t").get<double>();
            const auto &bones = kfObj.at("bones");
            kf.bonePoses.reserve(bones.size());
            for (auto it = bones.begin(); it != bones.end(); ++it)
            {
                const auto &b = it.value();
                BonePose pose;
                pose.position = {b.at("px").get<float>(), b.at("py").get<float>(), b.at("pz").get<float>()};
                pose.rotation = {b.at("qw").get<float>(), b.at("qx").get<float>(), b.at("qy").get<float>(),
                                 b.at("qz").get<float>()};
                pose.scale = {b.at("sx").get<float>(), b.at("sy").get<float>(), b.at("sz").get<float>()};
                kf.bonePoses.emplace(it.key(), pose);
            }
            m_keyframes.push_back(std::move(kf));
        }
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "MotionRecorder::LoadFromJson failed: " << e.what() << std::endl;
        return false;
    }
}

void MotionRecorder::Clear()
{
    m_keyframes.clear();
    m_recording = false;
}
