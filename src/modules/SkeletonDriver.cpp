#define GLM_ENABLE_EXPERIMENTAL
#include "modules/SkeletonDriver.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <scene/GameObject.h>
#include <scene/Skeleton.h>

#include <unordered_map>

namespace
{
// Given a target world position for `bone`, compute the local position that makes
// its world end up at `targetWorld` (leaving parent's pose unchanged).
glm::vec3 WorldToLocalPos(Geni::GameObject *bone, const glm::vec3 &targetWorld)
{
    auto *parent = bone->GetParent();
    if (!parent)
    {
        return targetWorld;
    }
    glm::mat4 parentInv = glm::inverse(parent->GetWorldTransform());
    return glm::vec3(parentInv * glm::vec4(targetWorld, 1.0f));
}

// Local rotation needed so that parent.world * local == desiredWorldRot.
glm::quat WorldToLocalRot(Geni::GameObject *bone, const glm::quat &desiredWorldRot)
{
    auto *parent = bone->GetParent();
    if (!parent)
    {
        return desiredWorldRot;
    }
    glm::quat parentWorldRot = glm::quat_cast(parent->GetWorldTransform());
    return glm::inverse(parentWorldRot) * desiredWorldRot;
}

// Index observations by id so repeat lookups are O(1). If multiple observations
// share an id (shouldn't happen for ArUco; could for misconfigured HSV), last wins.
std::unordered_map<int, const MarkerObservation *> IndexById(const std::vector<MarkerObservation> &obs)
{
    std::unordered_map<int, const MarkerObservation *> map;
    map.reserve(obs.size());
    for (const auto &o : obs)
    {
        map[o.id] = &o;
    }
    return map;
}

// Build a quaternion that rotates `from` onto `to`. Handles the 180° edge case by
// picking an arbitrary axis orthogonal to `from`.
glm::quat RotationBetween(const glm::vec3 &from, const glm::vec3 &to)
{
    glm::vec3 f = glm::normalize(from);
    glm::vec3 t = glm::normalize(to);
    float cosTheta = glm::dot(f, t);
    if (cosTheta > 0.99999f)
    {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    if (cosTheta < -0.99999f)
    {
        glm::vec3 axis = glm::cross(glm::vec3(1, 0, 0), f);
        if (glm::length(axis) < 1e-4f)
        {
            axis = glm::cross(glm::vec3(0, 1, 0), f);
        }
        return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
    }
    glm::vec3 axis = glm::normalize(glm::cross(f, t));
    float angle = std::acos(glm::clamp(cosTheta, -1.0f, 1.0f));
    return glm::angleAxis(angle, axis);
}

// Lazily sample bind-pose geometry for a chain from the skeleton's inverse-bind
// matrices (bind_world = inverse(inverse_bind)). Captures lengths, world-space
// bone directions, and the bind world rotation for each joint — all constant
// across frames, so compute once and reuse.
void PrimeIKChain(IKChain &chain, const Geni::Skeleton &skeleton, int rootIdx, int midIdx, int endIdx)
{
    if (chain.upperLen > 0.0f)
        return;

    glm::mat4 rootBind = glm::inverse(skeleton.GetInverseBindMatrix(rootIdx));
    glm::mat4 midBind = glm::inverse(skeleton.GetInverseBindMatrix(midIdx));
    glm::mat4 endBind = glm::inverse(skeleton.GetInverseBindMatrix(endIdx));

    glm::vec3 rootPos(rootBind[3]);
    glm::vec3 midPos(midBind[3]);
    glm::vec3 endPos(endBind[3]);

    chain.upperLen = glm::length(midPos - rootPos);
    chain.lowerLen = glm::length(endPos - midPos);
    chain.upperBindDirWorld =
        chain.upperLen > 1e-5f ? (midPos - rootPos) / chain.upperLen : glm::vec3(0, 1, 0);
    chain.lowerBindDirWorld =
        chain.lowerLen > 1e-5f ? (endPos - midPos) / chain.lowerLen : glm::vec3(0, 1, 0);
    chain.rootBindWorldPos = rootPos;
    chain.rootBindWorldRot = glm::quat_cast(rootBind);
    chain.midBindWorldRot = glm::quat_cast(midBind);
}

// Analytical two-bone IK. Poses `root` (shoulder) and `mid` (elbow) so that the
// chain root→mid→end reaches `target` with the elbow bending toward `poleHint`.
// Uses precomputed bind geometry so it's stable across repeated frames.
void SolveTwoBoneIK(Geni::GameObject *root, Geni::GameObject *mid, const IKChain &chain, const glm::vec3 &target)
{
    if (!root || !mid)
        return;
    if (chain.upperLen < 1e-5f || chain.lowerLen < 1e-5f)
        return;

    // Shoulder world position is driven by the parent chain (hips/spine), which
    // we don't touch — so read the CURRENT root world position, not the bind one.
    glm::vec3 rootPos(root->GetWorldTransform()[3]);
    glm::vec3 toTarget = target - rootPos;
    float targetDist = glm::length(toTarget);

    float maxReach = chain.upperLen + chain.lowerLen - 1e-4f;
    float minReach = std::abs(chain.upperLen - chain.lowerLen) + 1e-4f;
    float dist = glm::clamp(targetDist, minReach, maxReach);
    if (dist < 1e-5f)
        return;

    glm::vec3 targetDir = toTarget / (targetDist > 1e-5f ? targetDist : 1.0f);

    float cosRoot = glm::clamp((chain.upperLen * chain.upperLen + dist * dist - chain.lowerLen * chain.lowerLen) /
                                   (2.0f * chain.upperLen * dist),
                               -1.0f, 1.0f);
    float cosMid = glm::clamp(
        (chain.upperLen * chain.upperLen + chain.lowerLen * chain.lowerLen - dist * dist) /
            (2.0f * chain.upperLen * chain.lowerLen),
        -1.0f, 1.0f);
    float rootAngle = std::acos(cosRoot);
    float midInteriorAngle = std::acos(cosMid);
    float elbowBend = glm::pi<float>() - midInteriorAngle;

    // Bend plane normal: perpendicular to the target direction and the pole hint.
    glm::vec3 poleDir = glm::length(chain.poleHint) > 1e-5f ? glm::normalize(chain.poleHint) : glm::vec3(0, 0, 1);
    glm::vec3 bendAxis = glm::cross(targetDir, poleDir);
    if (glm::length(bendAxis) < 1e-4f)
    {
        bendAxis = glm::cross(targetDir, glm::vec3(0, 1, 0));
        if (glm::length(bendAxis) < 1e-4f)
            bendAxis = glm::cross(targetDir, glm::vec3(1, 0, 0));
    }
    bendAxis = glm::normalize(bendAxis);

    // Desired world-space directions for the two bones.
    glm::quat shoulderTilt = glm::angleAxis(rootAngle, bendAxis);
    glm::vec3 upperDirDesired = shoulderTilt * targetDir;
    glm::vec3 elbowPos = rootPos + upperDirDesired * chain.upperLen;
    glm::vec3 lowerDirDesired = glm::normalize(target - elbowPos);
    (void)elbowBend; // encoded implicitly via elbowPos/lowerDirDesired

    // Map bind-pose bone directions onto the desired ones. Each bone's new world
    // rotation is (delta that rotates bind-dir onto desired-dir) * bind-rot; then
    // convert to the bone's parent-local rotation.
    glm::quat shoulderDelta = RotationBetween(chain.upperBindDirWorld, upperDirDesired);
    glm::quat shoulderWorldRot = shoulderDelta * chain.rootBindWorldRot;
    root->SetRotation(WorldToLocalRot(root, shoulderWorldRot));

    // For the elbow, the bind-pose lower direction was computed relative to the
    // elbow's bind world rotation; after the shoulder moves, apply the same
    // shoulderDelta to that bind direction to find where it sits now, then rotate
    // it onto the desired lower direction.
    glm::vec3 lowerDirAfterShoulder = shoulderDelta * chain.lowerBindDirWorld;
    glm::quat elbowDelta = RotationBetween(lowerDirAfterShoulder, lowerDirDesired);
    glm::quat elbowWorldRot = elbowDelta * shoulderDelta * chain.midBindWorldRot;
    mid->SetRotation(WorldToLocalRot(mid, elbowWorldRot));
}
} // namespace

void SkeletonDriver::SetBindings(std::vector<MarkerBinding> bindings)
{
    m_bindings = std::move(bindings);
}

const std::vector<MarkerBinding> &SkeletonDriver::GetBindings() const
{
    return m_bindings;
}

void SkeletonDriver::SetIKChains(std::vector<IKChain> chains)
{
    m_chains = std::move(chains);
}

const std::vector<IKChain> &SkeletonDriver::GetIKChains() const
{
    return m_chains;
}

void SkeletonDriver::Apply(Geni::Skeleton &skeleton, const std::vector<MarkerObservation> &observations,
                           const Unproject &unproject)
{
    auto byId = IndexById(observations);

    for (const auto &binding : m_bindings)
    {
        auto it = byId.find(binding.markerId);
        if (it == byId.end())
            continue;

        int jointIndex = skeleton.FindJoint(binding.boneName);
        if (jointIndex < 0)
            continue;
        Geni::GameObject *bone = skeleton.GetJointNode(jointIndex);
        if (!bone)
            continue;

        glm::vec3 targetWorld = unproject(*it->second) + binding.worldOffset;

        if (binding.mode == MarkerBinding::Mode::LookAt)
        {
            // Yaw-only rotation: rotate around world Y based on the marker's horizontal
            // position. This keeps the bone upright and only turns it left/right.
            // The angle is measured from the +Z axis toward +X in the XZ plane.
            glm::mat4 bindWorld = glm::inverse(skeleton.GetInverseBindMatrix(jointIndex));
            glm::quat boneBindRot = glm::quat_cast(bindWorld);

            float yaw = std::atan2(targetWorld.x, targetWorld.z);
            glm::quat yawDelta = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
            bone->SetRotation(WorldToLocalRot(bone, yawDelta * boneBindRot));
        }
        else
        {
            bone->SetPosition(WorldToLocalPos(bone, targetWorld));

            if (binding.mode == MarkerBinding::Mode::PositionRotation && it->second->hasOrientation)
            {
                bone->SetRotation(WorldToLocalRot(bone, it->second->orientation));
            }
        }
    }

    for (auto &chain : m_chains)
    {
        auto it = byId.find(chain.markerId);
        if (it == byId.end())
            continue;

        int rootIdx = skeleton.FindJoint(chain.rootBoneName);
        int midIdx = skeleton.FindJoint(chain.midBoneName);
        int endIdx = skeleton.FindJoint(chain.endBoneName);
        if (rootIdx < 0 || midIdx < 0 || endIdx < 0)
            continue;

        PrimeIKChain(chain, skeleton, rootIdx, midIdx, endIdx);
        glm::vec3 target = unproject(*it->second) + chain.worldOffset;
        // Constrain target to the shoulder's bind-pose Z plane so the arm extends
        // to the side rather than reaching forward toward the camera.
        target.z = chain.rootBindWorldPos.z;
        SolveTwoBoneIK(skeleton.GetJointNode(rootIdx), skeleton.GetJointNode(midIdx), chain, target);
    }
}
