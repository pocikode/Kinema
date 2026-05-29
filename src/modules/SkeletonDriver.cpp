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

// Index observations by id so repeat lookups are O(1).
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

// Segment-driven arm solver. The shoulder joint (`rootPos`) is held fixed; the
// upper-arm bone is aimed from there toward `upperArmWorld`, and the forearm is
// aimed from the resolved elbow toward `foreAimWorld` (palm, or lower-arm
// fallback). Bone lengths from the bind pose are preserved (no stretch), so
// bending the physical elbow rotates the forearm relative to the upper arm and
// shows a visible joint bend. `haveUpper`/`haveFore` let either marker drop out
// (occlusion) while the other still drives its bone; a missing marker leaves
// that bone at its current pose.
void SolveArmChain(Geni::GameObject *root, Geni::GameObject *mid, const IKChain &chain,
                   const glm::vec3 &rootPos, const glm::vec3 &upperArmWorld, bool haveUpper,
                   const glm::vec3 &foreAimWorld, bool haveFore)
{
    if (!root || !mid)
        return;
    if (chain.upperLen < 1e-5f || chain.lowerLen < 1e-5f)
        return;

    // Upper-arm bone: aim from the fixed shoulder toward the upper-arm marker.
    glm::quat shoulderDelta(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 upperDirDesired = chain.upperBindDirWorld;
    if (haveUpper)
    {
        glm::vec3 toUpper = upperArmWorld - rootPos;
        if (glm::length(toUpper) > 1e-5f)
        {
            upperDirDesired = glm::normalize(toUpper);
            shoulderDelta = RotationBetween(chain.upperBindDirWorld, upperDirDesired);
            glm::quat shoulderWorldRot = shoulderDelta * chain.rootBindWorldRot;
            root->SetRotation(WorldToLocalRot(root, shoulderWorldRot));
        }
    }

    if (!haveFore)
        return;

    // Forearm bone: aim from the resolved elbow toward the palm/forearm marker.
    // The elbow sits at bind-pose upper-arm length along the upper direction, so
    // a longer/shorter physical arm doesn't stretch the rig mesh.
    glm::vec3 elbowPosResolved = rootPos + upperDirDesired * chain.upperLen;
    glm::vec3 toFore = foreAimWorld - elbowPosResolved;
    if (glm::length(toFore) < 1e-5f)
        return;
    glm::vec3 lowerDirDesired = glm::normalize(toFore);

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
        }
    }

    for (auto &chain : m_chains)
    {
        // Run if any of the chain's markers is visible — each bone can be driven
        // independently, so an occluded palm shouldn't freeze the upper arm.
        bool anyVisible = byId.count(chain.markerId) ||
                          (chain.upperArmMarkerId >= 0 && byId.count(chain.upperArmMarkerId)) ||
                          (chain.foreArmMarkerId >= 0 && byId.count(chain.foreArmMarkerId));
        if (!anyVisible)
            continue;

        int rootIdx = skeleton.FindJoint(chain.rootBoneName);
        int midIdx = skeleton.FindJoint(chain.midBoneName);
        int endIdx = skeleton.FindJoint(chain.endBoneName);
        if (rootIdx < 0 || midIdx < 0 || endIdx < 0)
            continue;

        PrimeIKChain(chain, skeleton, rootIdx, midIdx, endIdx);
        Geni::GameObject *rootBone = skeleton.GetJointNode(rootIdx);
        Geni::GameObject *midBone = skeleton.GetJointNode(midIdx);

        // Shoulder joint stays fixed at its current world position (bind pose /
        // parent chain). Only the upper-arm and forearm bones rotate.
        glm::vec3 rootPos = rootBone ? glm::vec3(rootBone->GetWorldTransform()[3]) : chain.rootBindWorldPos;

        // Upper-arm marker: aims the upper-arm bone. Z is pinned to the shoulder
        // plane so the arm swings sideways rather than reaching toward the camera
        // on noisy depth-from-area estimates.
        bool haveUpper = false;
        glm::vec3 upperArmWorld(0.0f);
        if (chain.upperArmMarkerId >= 0)
        {
            auto uIt = byId.find(chain.upperArmMarkerId);
            if (uIt != byId.end())
            {
                upperArmWorld = unproject(*uIt->second);
                upperArmWorld.z = rootPos.z;
                haveUpper = true;
            }
        }

        // Forearm aim: prefer the palm marker (end of the chain), fall back to the
        // lower-arm marker when the palm is occluded. Either one driving the
        // forearm independently of the upper arm is what makes the elbow bend read.
        bool haveFore = false;
        glm::vec3 foreAimWorld(0.0f);
        {
            auto pIt = byId.find(chain.markerId);
            if (pIt != byId.end())
            {
                foreAimWorld = unproject(*pIt->second) + chain.worldOffset;
                haveFore = true;
            }
            else if (chain.foreArmMarkerId >= 0)
            {
                auto fIt = byId.find(chain.foreArmMarkerId);
                if (fIt != byId.end())
                {
                    foreAimWorld = unproject(*fIt->second);
                    haveFore = true;
                }
            }
        }
        if (haveFore)
            foreAimWorld.z = rootPos.z;

        SolveArmChain(rootBone, midBone, chain, rootPos, upperArmWorld, haveUpper, foreAimWorld, haveFore);
    }
}
