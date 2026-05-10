#pragma once

#include "modules/MarkerDetection.h"

#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace Geni
{
class Skeleton;
}

// Mapping from a marker observation to a bone. Supports Position (drag the
// bone to the unprojected marker pos) and LookAt (rotate bone toward marker).
// IK-driven limb posing is handled via separate `IKChain` entries.
struct MarkerBinding
{
    enum class Mode
    {
        Position, // set bone world position; rotation untouched
        LookAt,   // rotate bone toward marker direction; position unchanged
    };

    int markerId = -1;
    std::string boneName;
    Mode mode = Mode::Position;

    // Offset (world space) added after unprojection. Lets the user zero the rig
    // relative to the capture space without recalibrating the camera.
    glm::vec3 worldOffset{0.0f};
};

// Two-bone analytical IK: root bone's rotation is solved so that its chain
// (root → mid → effector) reaches the marker-derived target, with the mid joint
// bending toward `poleHint`. Bone lengths and bind-pose directions are sampled
// once from the skeleton's inverse-bind matrices so repeated solves don't drift.
struct IKChain
{
    int markerId = -1;         // drives the end-effector target
    std::string rootBoneName;  // e.g. "mixamorig:LeftArm" (shoulder)
    std::string midBoneName;   // e.g. "mixamorig:LeftForeArm" (elbow)
    std::string endBoneName;   // e.g. "mixamorig:LeftHand" (hand)
    glm::vec3 poleHint{0.0f, 0.0f, 1.0f}; // direction the elbow should prefer
    glm::vec3 worldOffset{0.0f};

    // Cached bind-pose geometry (filled lazily on first Apply). -1 lengths signal
    // "not yet sampled".
    mutable float upperLen = -1.0f;
    mutable float lowerLen = -1.0f;
    mutable glm::vec3 upperBindDirWorld{0.0f};   // shoulder → elbow (bind, world space)
    mutable glm::vec3 lowerBindDirWorld{0.0f};   // elbow → hand (bind, world space)
    mutable glm::vec3 rootBindWorldPos{0.0f};
    mutable glm::quat rootBindWorldRot{1, 0, 0, 0};
    mutable glm::quat midBindWorldRot{1, 0, 0, 0};
};

class SkeletonDriver
{
  public:
    using Unproject = std::function<glm::vec3(const MarkerObservation &)>;

    void SetBindings(std::vector<MarkerBinding> bindings);
    const std::vector<MarkerBinding> &GetBindings() const;

    void SetIKChains(std::vector<IKChain> chains);
    const std::vector<IKChain> &GetIKChains() const;

    // Apply bindings and IK chains to the skeleton. Missing bones or unseen markers
    // are silently skipped so the rest of the rig stays in its bind pose.
    void Apply(Geni::Skeleton &skeleton, const std::vector<MarkerObservation> &observations, const Unproject &unproject);

  private:
    std::vector<MarkerBinding> m_bindings;
    std::vector<IKChain> m_chains;
};
