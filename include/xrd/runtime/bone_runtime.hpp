#pragma once
// Xrd-eXternalrEsolve - 骨骼运行时重扫辅助
// 将骨骼偏移清理与探测逻辑统一收口，便于上层项目复用。

#include "../engine/bones/bones_names.hpp"

namespace xrd
{
struct BoneRuntimeProbeResult
{
    bool componentToWorldOk = false;
    bool boneTransformsOk = false;
    bool boneNamesOk = false;
};

inline void ResetBoneRuntimeState()
{
    std::lock_guard<std::recursive_mutex> lock(detail::BoneRuntimeResolveMutex());
    auto& offsets = Ctx().off;
    offsets.ComponentToWorld_Offset = -1;
    offsets.ComponentSpaceTransforms_Offset = -1;
    offsets.RefSkeletonBoneInfo_Offset = -1;
    ClearBoneCaches();
}

inline BoneRuntimeProbeResult ProbeBoneRuntime(uptr meshPtr)
{
    BoneRuntimeProbeResult result{};
    if (!meshPtr)
    {
        return result;
    }

    std::lock_guard<std::recursive_mutex> lock(detail::BoneRuntimeResolveMutex());
    PrecacheBoneNames(meshPtr);

    FTransform componentToWorld{};
    std::vector<FTransform> boneTransforms;
    const auto* boneNames = GetCachedBoneNames(meshPtr);

    result.componentToWorldOk = ReadComponentToWorld(meshPtr, componentToWorld);
    result.boneTransformsOk = ReadBoneTransforms(meshPtr, boneTransforms);
    result.boneNamesOk = (boneNames != nullptr && !boneNames->empty());
    return result;
}

inline bool RescanBoneRuntime(uptr meshPtr, BoneRuntimeProbeResult* outResult = nullptr)
{
    std::lock_guard<std::recursive_mutex> lock(detail::BoneRuntimeResolveMutex());
    auto& offsets = Ctx().off;
    offsets.ComponentToWorld_Offset = -1;
    offsets.ComponentSpaceTransforms_Offset = -1;
    offsets.RefSkeletonBoneInfo_Offset = -1;
    ClearBoneCaches();

    BoneRuntimeProbeResult result = ProbeBoneRuntime(meshPtr);
    if (outResult)
    {
        *outResult = result;
    }

    return result.componentToWorldOk && result.boneTransformsOk && result.boneNamesOk;
}
} // namespace xrd
