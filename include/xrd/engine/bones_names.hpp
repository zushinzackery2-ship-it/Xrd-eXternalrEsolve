#pragma once
// Xrd-eXternalrEsolve - 骨骼名称读取
// 对标 UnrealResolve::ReadBoneName / ReadAllBoneNames
// 通过 SkeletalMesh 的 RefSkeleton.BoneInfo 数组读取骨骼 FName

#include "bones.hpp"
#include "../resolve/scan_property_offsets.hpp"
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <mutex>

namespace xrd
{

// FMeshBoneInfo: FName(8) + ParentIndex(4) = 12 字节
struct FMeshBoneInfo
{
    FName BoneName;
    i32   ParentIndex;
};

// 从 SkeletalMeshComponent 获取 SkeletalMesh 指针
inline uptr GetSkeletalMesh(uptr meshComponent)
{
    if (!meshComponent)
    {
        return 0;
    }

    // 读取 meshComponent 的类
    uptr cls = GetObjectClass(meshComponent);
    if (!cls)
    {
        return 0;
    }

    i32 smOff = GetPropertyOffsetByName(cls, "SkeletalMesh");
    if (smOff == -1)
    {
        // UE5.1+ 重命名为 SkeletalMeshAsset
        smOff = GetPropertyOffsetByName(cls, "SkeletalMeshAsset");
    }
    if (smOff == -1)
    {
        return 0;
    }

    uptr skelMesh = 0;
    if (!GReadPtr(meshComponent + smOff, skelMesh))
    {
        return 0;
    }
    return skelMesh;
}

// 自动定位 RefSkeletonBoneInfo 偏移并读取 BoneInfo TArray 元信息
// 返回: BoneInfo 数组的 Data 指针和 Count
inline bool ReadBoneInfoArray(
    uptr skeletalMesh,
    uptr& outDataPtr,
    i32& outCount)
{
    outDataPtr = 0;
    outCount = 0;
    if (!skeletalMesh)
    {
        return false;
    }

    auto& off = Ctx().off;

    // 首次访问时自动扫描偏移
    if (off.RefSkeletonBoneInfo_Offset == -1)
    {
        i32 detected = resolve::ScanRefSkeletonBoneInfoOffset(Mem(), skeletalMesh);
        if (detected != -1)
        {
            off.RefSkeletonBoneInfo_Offset = detected;
            std::cerr << "[xrd] RefSkeletonBoneInfo 偏移: 0x"
                      << std::hex << detected << std::dec << "\n";
        }
    }

    if (off.RefSkeletonBoneInfo_Offset == -1)
    {
        return false;
    }

    // 读取 TArray<FMeshBoneInfo> 的头部: Data(8) + Count(4) + Max(4)
    uptr dataPtr = 0;
    i32 count = 0;
    i32 max = 0;
    if (!ReadPtr(Mem(), skeletalMesh + off.RefSkeletonBoneInfo_Offset, dataPtr))
    {
        return false;
    }
    if (!IsCanonicalUserPtr(dataPtr))
    {
        return false;
    }
    ReadValue(Mem(), skeletalMesh + off.RefSkeletonBoneInfo_Offset + 8, count);
    ReadValue(Mem(), skeletalMesh + off.RefSkeletonBoneInfo_Offset + 12, max);

    if (count <= 0 || count > 500 || max < count)
    {
        return false;
    }

    outDataPtr = dataPtr;
    outCount = count;
    return true;
}

// 读取单个骨骼名称
inline std::string GetBoneNameByIndex(uptr meshComponent, i32 boneIndex)
{
    if (!meshComponent || boneIndex < 0)
    {
        return "";
    }

    uptr skelMesh = GetSkeletalMesh(meshComponent);
    if (!skelMesh)
    {
        return "";
    }

    uptr dataPtr = 0;
    i32 count = 0;
    if (!ReadBoneInfoArray(skelMesh, dataPtr, count))
    {
        return "";
    }

    if (boneIndex >= count)
    {
        return "";
    }

    // 读取 FMeshBoneInfo 中的 FName
    FName fn{};
    uptr boneInfoAddr = dataPtr + static_cast<uptr>(boneIndex) * sizeof(FMeshBoneInfo);
    if (!GReadValue(boneInfoAddr, fn))
    {
        return "";
    }

    return resolve::ResolveNameDirect(Mem(), Off(), fn.ComparisonIndex, fn.Number);
}

// 获取所有骨骼的索引和名称
inline std::vector<std::pair<i32, std::string>> GetAllBoneIndexAndNames(uptr meshComponent)
{
    std::vector<std::pair<i32, std::string>> result;

    if (!meshComponent)
    {
        return result;
    }

    uptr skelMesh = GetSkeletalMesh(meshComponent);
    if (!skelMesh)
    {
        return result;
    }

    uptr dataPtr = 0;
    i32 count = 0;
    if (!ReadBoneInfoArray(skelMesh, dataPtr, count))
    {
        return result;
    }

    // 批量读取所有 FMeshBoneInfo
    std::vector<FMeshBoneInfo> boneInfos(count);
    if (!Mem().Read(dataPtr, boneInfos.data(), count * sizeof(FMeshBoneInfo)))
    {
        return result;
    }

    result.reserve(count);
    for (i32 i = 0; i < count; ++i)
    {
        std::string name = resolve::ResolveNameDirect(
            Mem(), Off(),
            boneInfos[i].BoneName.ComparisonIndex,
            boneInfos[i].BoneName.Number);
        result.emplace_back(i, std::move(name));
    }

    return result;
}

// 通过名称查找骨骼索引，未找到返回 -1
inline i32 GetBoneIndexByName(uptr meshComponent, const std::string& boneName)
{
    auto allBones = GetAllBoneIndexAndNames(meshComponent);
    for (const auto& [idx, name] : allBones)
    {
        if (name == boneName)
        {
            return idx;
        }
    }
    return -1;
}

// GetBoneName 等价于 GetBoneNameByIndex（兼容接口）
inline std::string GetBoneName(uptr meshComponent, i32 boneIndex)
{
    return GetBoneNameByIndex(meshComponent, boneIndex);
}

// ─── 骨骼名缓存（按 SkeletalMesh 指针缓存，避免每帧重复读取） ───

namespace detail
{
    inline std::unordered_map<uptr, std::vector<std::string>>& BoneNameCache()
    {
        static std::unordered_map<uptr, std::vector<std::string>> cache;
        return cache;
    }

    inline std::mutex& BoneNameCacheMutex()
    {
        static std::mutex mtx;
        return mtx;
    }
} // namespace detail

// 获取缓存的骨骼名列表（线程安全）
// 首次调用时自动读取并缓存
inline const std::vector<std::string>* GetCachedBoneNames(uptr meshComponent)
{
    if (!meshComponent)
    {
        return nullptr;
    }

    uptr skelMesh = GetSkeletalMesh(meshComponent);
    if (!skelMesh)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(detail::BoneNameCacheMutex());
    auto& cache = detail::BoneNameCache();

    auto it = cache.find(skelMesh);
    if (it != cache.end())
    {
        return &it->second;
    }

    // 缓存未命中，读取并缓存
    uptr dataPtr = 0;
    i32 count = 0;
    if (!ReadBoneInfoArray(skelMesh, dataPtr, count))
    {
        return nullptr;
    }

    std::vector<FMeshBoneInfo> boneInfos(count);
    if (!Mem().Read(dataPtr, boneInfos.data(), count * sizeof(FMeshBoneInfo)))
    {
        return nullptr;
    }

    std::vector<std::string> names;
    names.reserve(count);
    for (i32 i = 0; i < count; ++i)
    {
        names.push_back(resolve::ResolveNameDirect(
            Mem(), Off(),
            boneInfos[i].BoneName.ComparisonIndex,
            boneInfos[i].BoneName.Number));
    }

    auto inserted = cache.emplace(skelMesh, std::move(names));
    return &inserted.first->second;
}

// 预缓存骨骼名（在 MemoryThread 中调用，避免 BoneThread 阻塞）
inline void PrecacheBoneNames(uptr meshComponent)
{
    (void)GetCachedBoneNames(meshComponent);
}

// ─── 过滤式骨骼读取：只读指定索引的骨骼世界坐标和屏幕投影 ───

inline bool GetFilteredBoneWorldLocations(
    uptr meshComponent,
    const FMatrix& viewProj,
    i32 screenW, i32 screenH,
    const std::vector<i32>& boneIndices,
    std::vector<FVector>& outLocations,
    std::vector<FVector2D>& outScreens,
    std::vector<bool>& outValids)
{
    outLocations.clear();
    outScreens.clear();
    outValids.clear();

    if (!meshComponent || boneIndices.empty())
    {
        return false;
    }

    FTransform c2w;
    if (!ReadComponentToWorld(meshComponent, c2w))
    {
        return false;
    }
    if (!IsLikelyValidTransform(c2w))
    {
        return false;
    }

    std::vector<FTransform> allBones;
    if (!ReadBoneTransforms(meshComponent, allBones))
    {
        return false;
    }

    i32 totalBones = static_cast<i32>(allBones.size());
    i32 filterCount = static_cast<i32>(boneIndices.size());
    outLocations.resize(filterCount);
    outScreens.resize(filterCount);
    outValids.resize(filterCount, false);

    for (i32 i = 0; i < filterCount; ++i)
    {
        i32 idx = boneIndices[i];
        if (idx < 0 || idx >= totalBones)
        {
            continue;
        }

        FVector wp = TransformBoneToWorld(c2w, allBones[idx]);
        outLocations[i] = wp;

        if (!IsFiniteReal(wp.X) || !IsFiniteReal(wp.Y) || !IsFiniteReal(wp.Z))
        {
            continue;
        }

        FReal x = wp.X * viewProj.M[0][0] + wp.Y * viewProj.M[1][0]
                + wp.Z * viewProj.M[2][0] + viewProj.M[3][0];
        FReal y = wp.X * viewProj.M[0][1] + wp.Y * viewProj.M[1][1]
                + wp.Z * viewProj.M[2][1] + viewProj.M[3][1];
        FReal w = wp.X * viewProj.M[0][3] + wp.Y * viewProj.M[1][3]
                + wp.Z * viewProj.M[2][3] + viewProj.M[3][3];

        if (w > FReal(0.001))
        {
            FReal invW = FReal(1.0) / w;
            outScreens[i].X = (screenW / FReal(2.0)) + (x * invW) * (screenW / FReal(2.0));
            outScreens[i].Y = (screenH / FReal(2.0)) - (y * invW) * (screenH / FReal(2.0));
            outValids[i] = true;
        }
    }

    return true;
}

} // namespace xrd
