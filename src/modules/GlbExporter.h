#pragma once
#include "MotionRecorder.h"
#include <geni.h>
#include <string>
#include <vector>

// Writes a GLB that is the source rig (mesh + materials + skin + IBMs) plus one
// additional animation assembled from the recorded keyframes. Bone target nodes
// are resolved by name against the source glTF node list, so skeleton bones and
// cgltf nodes must share names (they do when the rig was loaded via
// Geni::GameObject::LoadGLTF).
class GlbExporter
{
  public:
    // srcGlbPath  — path to the source .glb the user loaded.
    // dstGlbPath  — where to write the new GLB.
    // Returns false on any fatal error (source not a single-buffer GLB, skeleton
    // empty, no keyframes, write failed, etc.). Non-fatal issues such as bones
    // that cannot be resolved are logged to stderr and those channels skipped.
    bool Export(const std::string &srcGlbPath, const std::vector<Keyframe> &keyframes, const Geni::Skeleton &skeleton,
                const std::string &dstGlbPath, const std::string &animName = "Recorded");
};
