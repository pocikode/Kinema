#include "modules/GlbExporter.h"

// cgltf's implementation lives in geni's GameObject.cpp, so we only need the
// writer implementation here.
#define CGLTF_WRITE_IMPLEMENTATION
#include <cgltf.h>
#include <cgltf_write.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

namespace
{
// Pad `n` up to a 4-byte boundary — glTF requires that buffer views for float
// data are 4-byte aligned.
std::size_t AlignUp4(std::size_t n)
{
    return (n + 3u) & ~std::size_t(3u);
}

// What we track per animated bone while building the blob.
struct BoneAnim
{
    int boneIndex = -1;
    cgltf_node *targetNode = nullptr;
    std::vector<float> trans; // N*3, local translation per keyframe
    std::vector<float> rots;  // N*4, local rotation per keyframe (xyzw)
    std::vector<float> scale; // N*3, local scale per keyframe
    bool hasTrans = false;
    bool hasRot = false;
    bool hasScale = false;

    // Resolved after layout: offsets into the appended binary blob.
    std::size_t transOffset = 0;
    std::size_t rotsOffset = 0;
    std::size_t scaleOffset = 0;

    // Indices into the new bufferViews/accessors arrays.
    int transView = -1, transAccessor = -1;
    int rotsView = -1, rotsAccessor = -1;
    int scaleView = -1, scaleAccessor = -1;
};
} // namespace

bool GlbExporter::Export(const std::string &srcGlbPath, const std::vector<Keyframe> &keyframes,
                         const Geni::Skeleton &skeleton, const std::string &dstGlbPath,
                         const std::string &animName)
{
    if (keyframes.empty())
    {
        std::cerr << "GLB export: no recorded keyframes." << std::endl;
        return false;
    }
    if (skeleton.GetJointCount() == 0)
    {
        std::cerr << "GLB export: skeleton has no joints." << std::endl;
        return false;
    }

    cgltf_options parseOpts{};
    cgltf_data *data = nullptr;
    if (cgltf_parse_file(&parseOpts, srcGlbPath.c_str(), &data) != cgltf_result_success)
    {
        std::cerr << "GLB export: failed to parse source: " << srcGlbPath << std::endl;
        return false;
    }
    if (cgltf_load_buffers(&parseOpts, data, srcGlbPath.c_str()) != cgltf_result_success)
    {
        std::cerr << "GLB export: failed to load buffers for: " << srcGlbPath << std::endl;
        cgltf_free(data);
        return false;
    }

    if (data->buffers_count != 1 || data->buffers[0].data == nullptr)
    {
        std::cerr << "GLB export: source must be a single-buffer GLB (got " << data->buffers_count << " buffers)."
                  << std::endl;
        cgltf_free(data);
        return false;
    }

    // Map skeleton joints → cgltf nodes (by GameObject name == cgltf_node name).
    std::vector<cgltf_node *> jointToNode(skeleton.GetJointCount(), nullptr);
    for (int b = 0; b < skeleton.GetJointCount(); ++b)
    {
        Geni::GameObject *jn = skeleton.GetJointNode(b);
        if (!jn)
            continue;
        const std::string &name = jn->GetName();
        for (cgltf_size i = 0; i < data->nodes_count; ++i)
        {
            if (data->nodes[i].name && name == data->nodes[i].name)
            {
                jointToNode[b] = &data->nodes[i];
                break;
            }
        }
    }

    // Pull per-bone pose tracks out of the keyframe map.
    const std::size_t N = keyframes.size();
    std::vector<BoneAnim> bones;
    bones.reserve(skeleton.GetJointCount());

    for (int b = 0; b < skeleton.GetJointCount(); ++b)
    {
        Geni::GameObject *jn = skeleton.GetJointNode(b);
        if (!jn || !jointToNode[b])
            continue;

        const std::string &name = jn->GetName();
        BoneAnim ba;
        ba.boneIndex = b;
        ba.targetNode = jointToNode[b];
        ba.trans.resize(N * 3);
        ba.rots.resize(N * 4);
        ba.scale.resize(N * 3);

        std::size_t found = 0;
        for (std::size_t k = 0; k < N; ++k)
        {
            auto it = keyframes[k].bonePoses.find(name);
            if (it == keyframes[k].bonePoses.end())
            {
                // No pose for this bone this frame — hold previous value (or bind pose
                // at frame 0). For simplicity, leave zero-initialized for translation
                // and scale; rotation falls back to identity.
                ba.rots[k * 4 + 3] = 1.0f; // identity quat (x,y,z,w)
                ba.scale[k * 3 + 0] = 1.0f;
                ba.scale[k * 3 + 1] = 1.0f;
                ba.scale[k * 3 + 2] = 1.0f;
                continue;
            }
            const BonePose &p = it->second;
            ba.trans[k * 3 + 0] = p.position.x;
            ba.trans[k * 3 + 1] = p.position.y;
            ba.trans[k * 3 + 2] = p.position.z;
            // glm::quat storage is (w,x,y,z); glTF wants (x,y,z,w).
            ba.rots[k * 4 + 0] = p.rotation.x;
            ba.rots[k * 4 + 1] = p.rotation.y;
            ba.rots[k * 4 + 2] = p.rotation.z;
            ba.rots[k * 4 + 3] = p.rotation.w;
            ba.scale[k * 3 + 0] = p.scale.x;
            ba.scale[k * 3 + 1] = p.scale.y;
            ba.scale[k * 3 + 2] = p.scale.z;
            ++found;
        }
        if (found == 0)
            continue; // bone never recorded; skip
        ba.hasTrans = true;
        ba.hasRot = true;
        ba.hasScale = true;
        bones.push_back(std::move(ba));
    }

    if (bones.empty())
    {
        std::cerr << "GLB export: no skeleton bones resolved against source nodes." << std::endl;
        cgltf_free(data);
        return false;
    }

    // Build the new binary blob: [times][per-bone: T, R, S]
    const std::size_t srcBinSize = data->buffers[0].size;
    std::size_t blobSize = 0;
    std::size_t timesOffset = AlignUp4(srcBinSize);
    blobSize = timesOffset + N * sizeof(float);
    for (auto &ba : bones)
    {
        blobSize = AlignUp4(blobSize);
        ba.transOffset = blobSize;
        blobSize += ba.trans.size() * sizeof(float);
        blobSize = AlignUp4(blobSize);
        ba.rotsOffset = blobSize;
        blobSize += ba.rots.size() * sizeof(float);
        blobSize = AlignUp4(blobSize);
        ba.scaleOffset = blobSize;
        blobSize += ba.scale.size() * sizeof(float);
    }

    std::vector<std::uint8_t> combinedBin(blobSize, 0);
    std::memcpy(combinedBin.data(), data->buffers[0].data, srcBinSize);

    // timestamps
    std::vector<float> times(N);
    float tMin = 0.0f, tMax = 0.0f;
    for (std::size_t k = 0; k < N; ++k)
    {
        times[k] = static_cast<float>(keyframes[k].timestamp);
        if (k == 0)
            tMin = tMax = times[k];
        else
        {
            tMin = std::min(tMin, times[k]);
            tMax = std::max(tMax, times[k]);
        }
    }
    std::memcpy(combinedBin.data() + timesOffset, times.data(), N * sizeof(float));

    for (auto &ba : bones)
    {
        std::memcpy(combinedBin.data() + ba.transOffset, ba.trans.data(), ba.trans.size() * sizeof(float));
        std::memcpy(combinedBin.data() + ba.rotsOffset, ba.rots.data(), ba.rots.size() * sizeof(float));
        std::memcpy(combinedBin.data() + ba.scaleOffset, ba.scale.data(), ba.scale.size() * sizeof(float));
    }

    // --- Grow cgltf arrays. We own the new arrays and will restore originals
    // before cgltf_free. ---
    const cgltf_size srcViewsCount = data->buffer_views_count;
    const cgltf_size srcAccCount = data->accessors_count;
    const cgltf_size srcAnimCount = data->animations_count;

    const cgltf_size newViewsPerBone = 3;
    const cgltf_size newViewsCount = srcViewsCount + 1 /*times*/ + bones.size() * newViewsPerBone;
    const cgltf_size newAccCount = srcAccCount + 1 + bones.size() * newViewsPerBone;
    const cgltf_size newAnimCount = srcAnimCount + 1;

    std::vector<cgltf_buffer_view> newViews(newViewsCount);
    std::vector<cgltf_accessor> newAccessors(newAccCount);
    std::vector<cgltf_animation> newAnimations(newAnimCount);

    // Copy source entries verbatim; they still point into cgltf-owned memory and
    // stay valid as long as `data` is alive.
    for (cgltf_size i = 0; i < srcViewsCount; ++i)
        newViews[i] = data->buffer_views[i];
    for (cgltf_size i = 0; i < srcAccCount; ++i)
        newAccessors[i] = data->accessors[i];
    for (cgltf_size i = 0; i < srcAnimCount; ++i)
        newAnimations[i] = data->animations[i];

    // After copying, existing accessors still reference the OLD data->buffer_views
    // array by pointer. Remap those pointers into our newViews[] so the writer
    // walks our storage only.
    for (cgltf_size i = 0; i < srcAccCount; ++i)
    {
        cgltf_accessor &a = newAccessors[i];
        if (a.buffer_view)
        {
            cgltf_size idx = static_cast<cgltf_size>(a.buffer_view - data->buffer_views);
            a.buffer_view = &newViews[idx];
        }
    }

    // Same remap for existing animation channels/samplers — they reference the
    // OLD accessors array. We keep the original sampler/channel arrays inside
    // each animation because they still point at source-owned storage that we
    // cannot safely reallocate; cgltf_write walks from those pointers.
    for (cgltf_size i = 0; i < srcAnimCount; ++i)
    {
        cgltf_animation &an = newAnimations[i];
        for (cgltf_size s = 0; s < an.samplers_count; ++s)
        {
            if (an.samplers[s].input)
            {
                cgltf_size idx = static_cast<cgltf_size>(an.samplers[s].input - data->accessors);
                an.samplers[s].input = &newAccessors[idx];
            }
            if (an.samplers[s].output)
            {
                cgltf_size idx = static_cast<cgltf_size>(an.samplers[s].output - data->accessors);
                an.samplers[s].output = &newAccessors[idx];
            }
        }
    }

    // Set up the new "times" buffer view + accessor.
    cgltf_size viewIdx = srcViewsCount;
    cgltf_size accIdx = srcAccCount;
    const int timesView = static_cast<int>(viewIdx);
    const int timesAccessor = static_cast<int>(accIdx);
    {
        cgltf_buffer_view &bv = newViews[viewIdx++];
        bv = {};
        bv.buffer = &data->buffers[0];
        bv.offset = timesOffset;
        bv.size = N * sizeof(float);
        bv.type = cgltf_buffer_view_type_invalid;

        cgltf_accessor &ac = newAccessors[accIdx++];
        ac = {};
        ac.component_type = cgltf_component_type_r_32f;
        ac.type = cgltf_type_scalar;
        ac.offset = 0;
        ac.count = N;
        ac.stride = sizeof(float);
        ac.buffer_view = &newViews[timesView];
        ac.has_min = true;
        ac.has_max = true;
        ac.min[0] = tMin;
        ac.max[0] = tMax;
    }

    // Per-bone T/R/S views+accessors.
    for (auto &ba : bones)
    {
        ba.transView = static_cast<int>(viewIdx);
        {
            cgltf_buffer_view &bv = newViews[viewIdx++];
            bv = {};
            bv.buffer = &data->buffers[0];
            bv.offset = ba.transOffset;
            bv.size = ba.trans.size() * sizeof(float);
            bv.type = cgltf_buffer_view_type_invalid;
        }
        ba.rotsView = static_cast<int>(viewIdx);
        {
            cgltf_buffer_view &bv = newViews[viewIdx++];
            bv = {};
            bv.buffer = &data->buffers[0];
            bv.offset = ba.rotsOffset;
            bv.size = ba.rots.size() * sizeof(float);
            bv.type = cgltf_buffer_view_type_invalid;
        }
        ba.scaleView = static_cast<int>(viewIdx);
        {
            cgltf_buffer_view &bv = newViews[viewIdx++];
            bv = {};
            bv.buffer = &data->buffers[0];
            bv.offset = ba.scaleOffset;
            bv.size = ba.scale.size() * sizeof(float);
            bv.type = cgltf_buffer_view_type_invalid;
        }

        ba.transAccessor = static_cast<int>(accIdx);
        {
            cgltf_accessor &ac = newAccessors[accIdx++];
            ac = {};
            ac.component_type = cgltf_component_type_r_32f;
            ac.type = cgltf_type_vec3;
            ac.offset = 0;
            ac.count = N;
            ac.stride = 3 * sizeof(float);
            ac.buffer_view = &newViews[ba.transView];
        }
        ba.rotsAccessor = static_cast<int>(accIdx);
        {
            cgltf_accessor &ac = newAccessors[accIdx++];
            ac = {};
            ac.component_type = cgltf_component_type_r_32f;
            ac.type = cgltf_type_vec4;
            ac.offset = 0;
            ac.count = N;
            ac.stride = 4 * sizeof(float);
            ac.buffer_view = &newViews[ba.rotsView];
        }
        ba.scaleAccessor = static_cast<int>(accIdx);
        {
            cgltf_accessor &ac = newAccessors[accIdx++];
            ac = {};
            ac.component_type = cgltf_component_type_r_32f;
            ac.type = cgltf_type_vec3;
            ac.offset = 0;
            ac.count = N;
            ac.stride = 3 * sizeof(float);
            ac.buffer_view = &newViews[ba.scaleView];
        }
    }

    // Build our new animation: 3 samplers + 3 channels per bone.
    const cgltf_size ourSamplersCount = bones.size() * 3;
    const cgltf_size ourChannelsCount = bones.size() * 3;
    std::vector<cgltf_animation_sampler> ourSamplers(ourSamplersCount);
    std::vector<cgltf_animation_channel> ourChannels(ourChannelsCount);

    std::string nameOwned = animName; // stable storage for char* the writer will emit
    cgltf_animation &ourAnim = newAnimations[srcAnimCount];
    ourAnim = {};
    ourAnim.name = nameOwned.empty() ? nullptr : nameOwned.data();
    ourAnim.samplers = ourSamplers.data();
    ourAnim.samplers_count = ourSamplersCount;
    ourAnim.channels = ourChannels.data();
    ourAnim.channels_count = ourChannelsCount;

    for (std::size_t i = 0; i < bones.size(); ++i)
    {
        auto &ba = bones[i];
        const std::size_t base = i * 3;
        // T sampler+channel
        ourSamplers[base + 0] = {};
        ourSamplers[base + 0].input = &newAccessors[timesAccessor];
        ourSamplers[base + 0].output = &newAccessors[ba.transAccessor];
        ourSamplers[base + 0].interpolation = cgltf_interpolation_type_linear;
        ourChannels[base + 0] = {};
        ourChannels[base + 0].sampler = &ourSamplers[base + 0];
        ourChannels[base + 0].target_node = ba.targetNode;
        ourChannels[base + 0].target_path = cgltf_animation_path_type_translation;
        // R
        ourSamplers[base + 1] = {};
        ourSamplers[base + 1].input = &newAccessors[timesAccessor];
        ourSamplers[base + 1].output = &newAccessors[ba.rotsAccessor];
        ourSamplers[base + 1].interpolation = cgltf_interpolation_type_linear;
        ourChannels[base + 1] = {};
        ourChannels[base + 1].sampler = &ourSamplers[base + 1];
        ourChannels[base + 1].target_node = ba.targetNode;
        ourChannels[base + 1].target_path = cgltf_animation_path_type_rotation;
        // S
        ourSamplers[base + 2] = {};
        ourSamplers[base + 2].input = &newAccessors[timesAccessor];
        ourSamplers[base + 2].output = &newAccessors[ba.scaleAccessor];
        ourSamplers[base + 2].interpolation = cgltf_interpolation_type_linear;
        ourChannels[base + 2] = {};
        ourChannels[base + 2].sampler = &ourSamplers[base + 2];
        ourChannels[base + 2].target_node = ba.targetNode;
        ourChannels[base + 2].target_path = cgltf_animation_path_type_scale;
    }

    // Save source pointers/counts we're about to overwrite; restore before free.
    cgltf_buffer_view *savedViews = data->buffer_views;
    cgltf_size savedViewsCount = data->buffer_views_count;
    cgltf_accessor *savedAccessors = data->accessors;
    cgltf_size savedAccCount = data->accessors_count;
    cgltf_animation *savedAnimations = data->animations;
    cgltf_size savedAnimCount = data->animations_count;
    void *savedBufData = data->buffers[0].data;
    cgltf_size savedBufSize = data->buffers[0].size;
    const void *savedBin = data->bin;
    cgltf_size savedBinSize = data->bin_size;

    data->buffer_views = newViews.data();
    data->buffer_views_count = newViewsCount;
    data->accessors = newAccessors.data();
    data->accessors_count = newAccCount;
    data->animations = newAnimations.data();
    data->animations_count = newAnimCount;

    // For GLB output, cgltf_write writes bin from data->bin/bin_size; also update
    // buffer 0 so JSON buffer byteLength matches the binary chunk size.
    data->buffers[0].data = combinedBin.data();
    data->buffers[0].size = combinedBin.size();
    data->bin = combinedBin.data();
    data->bin_size = combinedBin.size();

    cgltf_options writeOpts{};
    writeOpts.type = cgltf_file_type_glb;
    cgltf_result wr = cgltf_write_file(&writeOpts, dstGlbPath.c_str(), data);

    // Restore so cgltf_free only frees what it originally allocated.
    data->buffer_views = savedViews;
    data->buffer_views_count = savedViewsCount;
    data->accessors = savedAccessors;
    data->accessors_count = savedAccCount;
    data->animations = savedAnimations;
    data->animations_count = savedAnimCount;
    data->buffers[0].data = savedBufData;
    data->buffers[0].size = savedBufSize;
    data->bin = savedBin;
    data->bin_size = savedBinSize;

    cgltf_free(data);

    if (wr != cgltf_result_success)
    {
        std::cerr << "GLB export: cgltf_write_file failed (" << wr << ") for " << dstGlbPath << std::endl;
        return false;
    }

    std::cout << "GLB export: wrote " << dstGlbPath << " (" << bones.size() << " animated bones, " << N << " frames)"
              << std::endl;
    return true;
}
