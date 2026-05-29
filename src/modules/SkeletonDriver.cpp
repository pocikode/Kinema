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
// two bones are aimed using *inter-marker* directions so they live in the same
// frame as each other (the rig shoulder's absolute world height is unrelated to
// the camera/marker space, so aiming from rig-shoulder to a marker would let a
// rest-down arm read as "lifted"):
//   upper arm  = lowerArm marker - upperArm marker   (U → L)
//   forearm    = palm marker      - lowerArm marker   (L → P)
// At rest (all three markers vertically aligned) both directions point straight
// down, so the avatar arm hangs down. Bone lengths from the bind pose are
// preserved (no stretch). `haveU/haveL/haveP` let a marker drop out: a bone is
// only re-aimed when both of its endpoints are visible, otherwise it holds its
// current pose.
void SolveArmChain(Geni::GameObject *root, Geni::GameObject *mid, const IKChain &chain,
                   const glm::vec3 &rootPos, const glm::vec3 &upperMarker, bool haveU,
                   const glm::vec3 &lowerMarker, bool haveL, const glm::vec3 &palmMarker, bool haveP)
{
    if (!root || !mid)
        return;
    if (chain.upperLen < 1e-5f || chain.lowerLen < 1e-5f)
        return;

    // Upper-arm bone: aim along the upper-arm → lower-arm marker vector.
    glm::quat shoulderDelta(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 upperDirDesired = chain.upperBindDirWorld;
    if (haveU && haveL)
    {
        glm::vec3 dir = lowerMarker - upperMarker;
        if (glm::length(dir) > 1e-5f)
        {
            upperDirDesired = glm::normalize(dir);
            shoulderDelta = RotationBetween(chain.upperBindDirWorld, upperDirDesired);
            glm::quat shoulderWorldRot = shoulderDelta * chain.rootBindWorldRot;
            root->SetRotation(WorldToLocalRot(root, shoulderWorldRot));
        }
    }

    // Forearm bone: aim along the lower-arm → palm marker vector, pivoting at the
    // resolved elbow (bind-pose upper-arm length along the upper direction, so a
    // longer/shorter physical arm doesn't stretch the rig mesh).
    if (!(haveL && haveP))
        return;
    glm::vec3 lowerDir = palmMarker - lowerMarker;
    if (glm::length(lowerDir) < 1e-5f)
        return;
    glm::vec3 lowerDirDesired = glm::normalize(lowerDir);

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
        // Cheap early-out: skip the chain entirely if none of its markers are
        // visible. Per-bone visibility (which pair drives which bone) is resolved
        // in SolveArmChain.
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

        // Gather the three arm markers. Z is pinned to the shoulder plane on all
        // three so the bones articulate in the frontal plane rather than reaching
        // toward the camera on noisy depth-from-area estimates. Directions are
        // taken marker-to-marker (U→L, L→P) in SolveArmChain, so the markers share
        // a frame and a rest-down arm reads as down.
        auto fetch = [&](int id, glm::vec3 &out) -> bool {
            if (id < 0)
                return false;
            auto mIt = byId.find(id);
            if (mIt == byId.end())
                return false;
            out = unproject(*mIt->second);
            out.z = rootPos.z;
            return true;
        };

        glm::vec3 upperMarker(0.0f), lowerMarker(0.0f), palmMarker(0.0f);
        bool haveU = fetch(chain.upperArmMarkerId, upperMarker);
        bool haveL = fetch(chain.foreArmMarkerId, lowerMarker);
        bool haveP = fetch(chain.markerId, palmMarker);
        if (haveP)
            palmMarker += chain.worldOffset;

        SolveArmChain(rootBone, midBone, chain, rootPos, upperMarker, haveU, lowerMarker, haveL,
                      palmMarker, haveP);
    }
}
