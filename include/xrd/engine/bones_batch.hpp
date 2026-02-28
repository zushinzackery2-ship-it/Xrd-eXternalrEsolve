#pragma once
// Xrd-eXternalrEsolve - 骨骼批量操作
// 从 bones.hpp 拆分：单骨骼世界坐标、批量骨骼投影

#include "bones.hpp"

namespace xrd
{

// 获取单个骨骼的世界坐标
inline bool GetBoneWorldLocation(uptr meshComponent, i32 boneIndex, FVector& outPos)
{
    outPos = {};
    if (!meshComponent || boneIndex < 0)
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

    std::vector<FTransform> bones;
    if (!ReadBoneTransforms(meshComponent, bones))
    {
        return false;
    }
    if (boneIndex >= (i32)bones.size())
    {
        return false;
    }

    outPos = TransformBoneToWorld(c2w, bones[boneIndex]);
    return IsFiniteReal(outPos.X) && IsFiniteReal(outPos.Y) && IsFiniteReal(outPos.Z);
}

// 批量获取所有骨骼的世界坐标和屏幕投影
inline bool GetAllBoneWorldLocations(
    uptr meshComponent,
    const FMatrix& viewProj,
    i32 screenW, i32 screenH,
    std::vector<FVector>& outLocations,
    std::vector<FVector2D>& outScreens,
    std::vector<bool>& outValids)
{
    outLocations.clear();
    outScreens.clear();
    outValids.clear();

    FTransform c2w;
    if (!ReadComponentToWorld(meshComponent, c2w))
    {
        return false;
    }
    if (!IsLikelyValidTransform(c2w))
    {
        return false;
    }

    std::vector<FTransform> bones;
    if (!ReadBoneTransforms(meshComponent, bones))
    {
        return false;
    }

    i32 count = (i32)bones.size();
    outLocations.resize(count);
    outScreens.resize(count);
    outValids.resize(count, false);

    for (i32 i = 0; i < count; ++i)
    {
        FVector wp = TransformBoneToWorld(c2w, bones[i]);
        outLocations[i] = wp;

        if (!IsFiniteReal(wp.X) || !IsFiniteReal(wp.Y) || !IsFiniteReal(wp.Z))
        {
            continue;
        }

        // 世界坐标投影到屏幕
        FReal x = wp.X * viewProj.M[0][0] + wp.Y * viewProj.M[1][0]
                + wp.Z * viewProj.M[2][0] + viewProj.M[3][0];
        FReal y = wp.X * viewProj.M[0][1] + wp.Y * viewProj.M[1][1]
                + wp.Z * viewProj.M[2][1] + viewProj.M[3][1];
        FReal w = wp.X * viewProj.M[0][3] + wp.Y * viewProj.M[1][3]
                + wp.Z * viewProj.M[2][3] + viewProj.M[3][3];

        if (w > 0.001)
        {
            FReal invW = 1.0 / w;
            outScreens[i].X = (screenW / 2.0) + (x * invW) * (screenW / 2.0);
            outScreens[i].Y = (screenH / 2.0) - (y * invW) * (screenH / 2.0);
            outValids[i] = true;
        }
    }

    return true;
}

} // namespace xrd
