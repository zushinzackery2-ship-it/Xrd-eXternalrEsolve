#pragma once
// Xrd-eXternalrEsolve - 骨骼偏移扫描器
// 自动检测 ComponentToWorld、ComponentSpaceTransforms 偏移
// 实现双缓冲骨骼数组选择（来自 UnrealResolve 算法）

#include "../core/context.hpp"
#include <iostream>
#include <cmath>

namespace xrd
{
namespace resolve
{

// 扫描 USceneComponent 中的 ComponentToWorld 偏移
// 通过验证 FTransform 的旋转四元数是否接近单位长度来判断
inline i32 ScanComponentToWorldOffset(
    const IMemoryAccessor& mem,
    uptr meshComponent,
    bool isDouble)
{
    if (!IsCanonicalUserPtr(meshComponent))
    {
        return -1;
    }

    // 在 0x100~0x300 范围内搜索（USceneComponent 的典型范围）
    for (i32 off = 0x100; off <= 0x300; off += 0x10)
    {
        if (isDouble)
        {
            double qx = 0, qy = 0, qz = 0, qw = 0;
            mem.Read(meshComponent + off + 0x00, &qx, 8);
            mem.Read(meshComponent + off + 0x08, &qy, 8);
            mem.Read(meshComponent + off + 0x10, &qz, 8);
            mem.Read(meshComponent + off + 0x18, &qw, 8);

            double len2 = qx * qx + qy * qy + qz * qz + qw * qw;
            if (!std::isfinite(len2) || len2 < 0.9 || len2 > 1.1)
            {
                continue;
            }

            // 验证平移分量有限
            double tx = 0, ty = 0, tz = 0;
            mem.Read(meshComponent + off + 0x20, &tx, 8);
            mem.Read(meshComponent + off + 0x28, &ty, 8);
            mem.Read(meshComponent + off + 0x30, &tz, 8);
            if (!std::isfinite(tx) || !std::isfinite(ty) || !std::isfinite(tz))
            {
                continue;
            }

            // 验证缩放分量合理
            double sx = 0, sy = 0, sz = 0;
            mem.Read(meshComponent + off + 0x40, &sx, 8);
            mem.Read(meshComponent + off + 0x48, &sy, 8);
            mem.Read(meshComponent + off + 0x50, &sz, 8);
            if (std::abs(sx) > 0.01 && std::abs(sx) < 100.0 &&
                std::abs(sy) > 0.01 && std::abs(sy) < 100.0 &&
                std::abs(sz) > 0.01 && std::abs(sz) < 100.0)
            {
                return off;
            }
        }
        else
        {
            float qx = 0, qy = 0, qz = 0, qw = 0;
            mem.Read(meshComponent + off + 0x00, &qx, 4);
            mem.Read(meshComponent + off + 0x04, &qy, 4);
            mem.Read(meshComponent + off + 0x08, &qz, 4);
            mem.Read(meshComponent + off + 0x0C, &qw, 4);

            float len2 = qx * qx + qy * qy + qz * qz + qw * qw;
            if (!std::isfinite(len2) || len2 < 0.9f || len2 > 1.1f)
            {
                continue;
            }

            float tx = 0, ty = 0, tz = 0;
            mem.Read(meshComponent + off + 0x10, &tx, 4);
            mem.Read(meshComponent + off + 0x14, &ty, 4);
            mem.Read(meshComponent + off + 0x18, &tz, 4);
            if (!std::isfinite(tx) || !std::isfinite(ty) || !std::isfinite(tz))
            {
                continue;
            }

            float sx = 0, sy = 0, sz = 0;
            mem.Read(meshComponent + off + 0x20, &sx, 4);
            mem.Read(meshComponent + off + 0x24, &sy, 4);
            mem.Read(meshComponent + off + 0x28, &sz, 4);
            if (std::abs(sx) > 0.01f && std::abs(sx) < 100.0f &&
                std::abs(sy) > 0.01f && std::abs(sy) < 100.0f &&
                std::abs(sz) > 0.01f && std::abs(sz) < 100.0f)
            {
                return off;
            }
        }
    }
    return -1;
}

// 获取有效的骨骼数组索引（双缓冲机制）
// USkeletalMeshComponent 有两个骨骼变换数组，只有一个是当前帧有效的
inline int GetValidBoneArrayIndex(
    const IMemoryAccessor& mem,
    uptr meshComponent,
    i32 boneArrayOffset,
    i32 activeIndexOffset,
    bool isDouble)
{
    if (!IsCanonicalUserPtr(meshComponent))
    {
        return 0;
    }

    // 优先使用引擎提供的活跃缓冲索引
    if (activeIndexOffset != -1)
    {
        i32 activeIdx = 0;
        ReadValue(mem, meshComponent + activeIndexOffset, activeIdx);
        return (activeIdx & 1);
    }

    // 回退方案：检查哪个缓冲区包含有效变换
    for (int bufIdx = 0; bufIdx < 2; ++bufIdx)
    {
        uptr arrayData = 0;
        i32 arrayCount = 0;
        i32 arrayOff = boneArrayOffset + bufIdx * 0x10;

        if (!ReadPtr(mem, meshComponent + arrayOff, arrayData))
        {
            continue;
        }
        if (!IsCanonicalUserPtr(arrayData))
        {
            continue;
        }

        ReadValue(mem, meshComponent + arrayOff + 8, arrayCount);
        if (arrayCount <= 0 || arrayCount > 500)
        {
            continue;
        }

        // 验证第一个骨骼变换的 W 分量
        if (isDouble)
        {
            double qw = 0;
            mem.Read(arrayData + 0x18, &qw, 8);
            if (std::isfinite(qw) && std::abs(qw) > 0.01 && std::abs(qw) <= 1.0)
            {
                return bufIdx;
            }
        }
        else
        {
            float qw = 0;
            mem.Read(arrayData + 0x0C, &qw, 4);
            if (std::isfinite(qw) && std::abs(qw) > 0.01f && std::abs(qw) <= 1.0f)
            {
                return bufIdx;
            }
        }
    }

    return 0;
}

// 对标 UnrealResolve::ScanRefSkeletonBoneInfoOffset
// 在 USkeletalMesh 对象内存中搜索连续 4+ 个有效 TArray 结构
// RefSkeleton 包含多个连续 TArray（BoneInfo, NameMapping 等）
inline i32 ScanRefSkeletonBoneInfoOffset(
    const IMemoryAccessor& mem,
    uptr skeletalMeshPtr)
{
    if (!IsCanonicalUserPtr(skeletalMeshPtr))
    {
        return -1;
    }

    constexpr u32 searchSize = 0x1000;
    std::vector<u8> buf(searchSize);
    if (!mem.Read(skeletalMeshPtr, buf.data(), searchSize))
    {
        return -1;
    }

    constexpr u32 tarraySize = 16;       // TArray: Data(8) + Count(4) + Max(4)
    constexpr u32 minConsecutive = 4;

    for (u32 offset = 0; offset + tarraySize * minConsecutive <= searchSize; offset += 8)
    {
        u32 consecutive = 0;
        u32 cur = offset;

        while (cur + tarraySize <= searchSize)
        {
            u64 ptr   = *reinterpret_cast<u64*>(&buf[cur]);
            i32 count = *reinterpret_cast<i32*>(&buf[cur + 8]);
            i32 max   = *reinterpret_cast<i32*>(&buf[cur + 12]);

            if (ptr != 0 && count > 0 && count <= 1000 &&
                max > 0 && max <= 1000 && count <= max)
            {
                ++consecutive;
                cur += tarraySize;
            }
            else
            {
                break;
            }
        }

        if (consecutive >= minConsecutive)
        {
            return static_cast<i32>(offset);
        }
    }

    return -1;
}

} // namespace resolve
} // namespace xrd
