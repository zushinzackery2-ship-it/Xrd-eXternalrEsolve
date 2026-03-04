#pragma once
// Xrd-eXternalrEsolve - 骨骼系统
// 骨骼网格体读取，支持双缓冲机制（继承 UnrealResolve 算法）

#include "../core/context.hpp"
#include "../resolve/scan_bones.hpp"
#include "world.hpp"
#include <vector>
#include <cmath>
#include <cstring>

namespace xrd
{

// 读取 USceneComponent 的 ComponentToWorld 变换
inline bool ReadComponentToWorld(uptr component, FTransform& out)
{
    if (!component)
    {
        return false;
    }

    auto& off = Ctx().off;

    // 首次访问时自动扫描偏移
    if (off.ComponentToWorld_Offset == -1 && off.kAutoScanUSceneComponentComponentToWorld)
    {
        i32 detected = resolve::ScanComponentToWorldOffset(Mem(), component, off.bUseDoublePrecision);
        if (detected != -1)
        {
            off.ComponentToWorld_Offset = detected;
            std::cerr << "[xrd] ComponentToWorld 偏移: 0x"
                      << std::hex << detected << std::dec << "\n";
        }
    }

    if (off.ComponentToWorld_Offset == -1)
    {
        return false;
    }

    if (off.bUseDoublePrecision)
    {
        return GReadValue(component + off.ComponentToWorld_Offset, out);
    }
    else
    {
        // float 精度读取后转换为 double 存储
        struct FTransformF
        {
            float Rot[4];
            float Trans[3];
            float Pad0;
            float Scale[3];
            float Pad1;
        } tf{};

        if (!GReadValue(component + off.ComponentToWorld_Offset, tf))
        {
            return false;
        }

        out.Rotation    = { tf.Rot[0], tf.Rot[1], tf.Rot[2], tf.Rot[3] };
        out.Translation = { tf.Trans[0], tf.Trans[1], tf.Trans[2] };
        out.Scale3D     = { tf.Scale[0], tf.Scale[1], tf.Scale[2] };
        return true;
    }
}

// 读取 SkeletalMeshComponent 的骨骼变换数组
inline bool ReadBoneTransforms(uptr meshComponent, std::vector<FTransform>& outBones)
{
    outBones.clear();
    if (!meshComponent)
    {
        return false;
    }

    auto& off = Ctx().off;

    // 对标 UnrealResolve::ScanComponentSpaceTransformsArrayOffset
    // 搜索两个相邻的 TArray<FTransform>：指针不同但 count/max 相同（双缓冲）
    if (off.ComponentSpaceTransforms_Offset == -1)
    {
        constexpr u32 searchSize = 0x1000;
        std::vector<u8> buf(searchSize);
        if (Mem().Read(meshComponent, buf.data(), searchSize))
        {
            for (u32 offset = 0; offset + 32 <= searchSize; offset += 8)
            {
                u64 ptr1   = *reinterpret_cast<u64*>(&buf[offset]);
                i32 count1 = *reinterpret_cast<i32*>(&buf[offset + 8]);
                i32 max1   = *reinterpret_cast<i32*>(&buf[offset + 12]);

                u64 ptr2   = *reinterpret_cast<u64*>(&buf[offset + 16]);
                i32 count2 = *reinterpret_cast<i32*>(&buf[offset + 24]);
                i32 max2   = *reinterpret_cast<i32*>(&buf[offset + 28]);

                if (ptr1 != 0 && ptr2 != 0 && ptr1 != ptr2 &&
                    count1 == count2 && max1 == max2 &&
                    count1 > 0 && count1 < 1000 && max1 > 0 && max1 < 1000)
                {
                    off.ComponentSpaceTransforms_Offset = static_cast<i32>(offset);
                    std::cerr << "[xrd] ComponentSpaceTransforms 偏移: 0x"
                              << std::hex << offset << std::dec
                              << " (bones=" << count1 << ")\n";
                    break;
                }
            }
        }
    }

    if (off.ComponentSpaceTransforms_Offset == -1)
    {
        return false;
    }

    // 选择有效的双缓冲索引
    int validBuf = resolve::GetValidBoneArrayIndex(
        Mem(), meshComponent,
        off.ComponentSpaceTransforms_Offset,
        off.BoneSpaceTransforms_ActiveIndex,
        off.bUseDoublePrecision
    );

    i32 arrayOffset = off.ComponentSpaceTransforms_Offset + validBuf * 0x10;

    uptr arrData = 0;
    i32 arrCount = 0;
    if (!GReadPtr(meshComponent + arrayOffset, arrData))
    {
        return false;
    }
    if (!IsCanonicalUserPtr(arrData))
    {
        return false;
    }
    GReadI32(meshComponent + arrayOffset + 8, arrCount);
    if (arrCount <= 0 || arrCount > 500)
    {
        return false;
    }

    i32 transformSize = off.bUseDoublePrecision ? 0x60 : 0x30;

    if (off.bUseDoublePrecision)
    {
        outBones.resize(arrCount);
        std::vector<u8> rawData(arrCount * transformSize);
        if (!Mem().Read(arrData, rawData.data(), rawData.size()))
        {
            return false;
        }
        for (i32 i = 0; i < arrCount; ++i)
        {
            std::memcpy(&outBones[i], rawData.data() + i * transformSize, transformSize);
        }
    }
    else
    {
        // float 精度批量读取后逐个转换
        struct FTransformF { float data[12]; };
        std::vector<FTransformF> rawBones(arrCount);
        if (!Mem().Read(arrData, rawBones.data(), arrCount * sizeof(FTransformF)))
        {
            return false;
        }
        outBones.resize(arrCount);
        for (i32 i = 0; i < arrCount; ++i)
        {
            auto& r = rawBones[i];
            outBones[i].Rotation    = { r.data[0], r.data[1], r.data[2], r.data[3] };
            outBones[i].Translation = { r.data[4], r.data[5], r.data[6] };
            outBones[i].Scale3D     = { r.data[8], r.data[9], r.data[10] };
        }
    }

    return true;
}

// 将骨骼从组件空间变换到世界空间
// 使用四元数旋转公式: rotated = v + 2*cross(q.xyz, cross(q.xyz, v) + q.w*v)
inline FVector TransformBoneToWorld(const FTransform& c2w, const FTransform& bone)
{
    auto& q = c2w.Rotation;
    auto& v = bone.Translation;

    FReal tx = q.Y * v.Z - q.Z * v.Y + q.W * v.X;
    FReal ty = q.Z * v.X - q.X * v.Z + q.W * v.Y;
    FReal tz = q.X * v.Y - q.Y * v.X + q.W * v.Z;

    FReal rx = v.X + 2.0 * (q.Y * tz - q.Z * ty);
    FReal ry = v.Y + 2.0 * (q.Z * tx - q.X * tz);
    FReal rz = v.Z + 2.0 * (q.X * ty - q.Y * tx);

    return {
        rx * c2w.Scale3D.X + c2w.Translation.X,
        ry * c2w.Scale3D.Y + c2w.Translation.Y,
        rz * c2w.Scale3D.Z + c2w.Translation.Z
    };
}

} // namespace xrd
