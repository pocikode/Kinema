#pragma once
#include <chrono>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Geni
{
class Skeleton;
}

// Local-space pose of a single bone at a single keyframe. Local (not world) so
// playback is re-parenting-safe — rig root can move without rebaking.
struct BonePose
{
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct Keyframe
{
    double timestamp = 0.0;
    std::unordered_map<std::string, BonePose> bonePoses;
};

// Per-skeleton keyframe recorder. v2 JSON schema only: each keyframe stores a
// bone-name → local pose map so multi-bone rigs replay exactly as captured.
class MotionRecorder
{
  public:
    void StartRecording();
    void StopRecording();
    bool IsRecording() const;

    // Samples the skeleton's current local pose into a new keyframe at now - startTime.
    void AddSkeletonKeyframe(const Geni::Skeleton &skeleton);

    // Writes the keyframe's bone poses into the skeleton's joint GameObjects. Bones
    // in the keyframe but missing from the skeleton are silently skipped.
    static void ApplyKeyframe(const Keyframe &keyframe, Geni::Skeleton &skeleton);

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
