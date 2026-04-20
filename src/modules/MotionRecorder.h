#pragma once
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <chrono>

struct Keyframe
{
    double timestamp;
    glm::vec3 position;
    glm::quat rotation;
};

class MotionRecorder
{
  public:
    void StartRecording();
    void StopRecording();
    bool IsRecording() const;

    void AddKeyframe(const glm::vec3 &pos, const glm::quat &rot);

    const std::vector<Keyframe> &GetKeyframes() const;
    int GetKeyframeCount() const;
    float GetDuration() const;

    bool SaveToJson(const std::string &path) const;
    bool LoadFromJson(const std::string &path);
    void Clear();

  private:
    std::vector<Keyframe> m_keyframes;
    bool m_recording = false;
    std::chrono::steady_clock::time_point m_startTime;
};
