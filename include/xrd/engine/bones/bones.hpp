#pragma once
// Xrd-eXternalrEsolve - 骨骼系统
// 骨骼网格体读取，ComponentSpaceTransforms 只在 USkinnedMeshComponent
// 自身成员范围内按双 TArray 特征发现，避免被子类私有数组带偏。

#include "../../core/context.hpp"
#include "../../resolve/runtime/scan_bones.hpp"
#include "../world/world.hpp"
#include "bones_offsets.hpp"
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
        std::lock_guard<std::recursive_mutex> lock(detail::BoneRuntimeResolveMutex());
        if (off.ComponentToWorld_Offset == -1)
        {
            i32 detected = -1;
            uptr cls = GetObjectClass(component);
            if (cls)
            {
                detected = GetPropertyOffsetByName(cls, "ComponentToWorld");
            }

            if (detected == -1)
            {
                detected = resolve::ScanComponentToWorldOffset(
                    Mem(),
                    component,
                    off.bUseDoublePrecision
                );
            }

            if (detected != -1)
            {
                off.ComponentToWorld_Offset = detected;
                std::cerr << "[xrd] ComponentToWorld 偏移: 0x"
                          << std::hex << detected << std::dec << "\n";
            }
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

// 读取 SkeletalMeshComponent 的 CachedComponentSpaceTransforms
inline bool ReadBoneTransforms(uptr meshComponent, std::vector<FTransform>& outBones)
{
    outBones.clear();
    if (!meshComponent)
    {
        return false;
    }

    auto& off = Ctx().off;
    const detail::StructMemberRange allowedRange = detail::GetSkinnedMeshComponentMemberRange();

    if (off.ComponentSpaceTransforms_Offset != -1
        && allowedRange.IsValid()
        && !detail::IsOffsetInRange(off.ComponentSpaceTransforms_Offset, allowedRange))
    {
        std::lock_guard<std::recursive_mutex> lock(detail::BoneRuntimeResolveMutex());
        if (off.ComponentSpaceTransforms_Offset != -1
            && !detail::IsOffsetInRange(off.ComponentSpaceTransforms_Offset, allowedRange))
        {
            std::cerr << "[xrd] ComponentSpaceTransforms 当前偏移越界: 0x"
                      << std::hex << off.ComponentSpaceTransforms_Offset
                      << " 不在 [0x" << allowedRange.begin
                      << ", 0x" << allowedRange.end << ")" << std::dec << "\n";
            off.ComponentSpaceTransforms_Offset = -1;
        }
    }

    if (off.ComponentSpaceTransforms_Offset == -1)
    {
        std::lock_guard<std::recursive_mutex> lock(detail::BoneRuntimeResolveMutex());
        if (off.ComponentSpaceTransforms_Offset == -1)
        {
            i32 detected = detail::DiscoverComponentSpaceTransformsOffset(
                meshComponent,
                off.bUseDoublePrecision
            );

            if (detected != -1)
            {
                off.ComponentSpaceTransforms_Offset = detected;
                std::cerr << "[xrd] ComponentSpaceTransforms 偏移: 0x"
                          << std::hex << detected << std::dec << "\n";
            }
        }
    }

    if (off.ComponentSpaceTransforms_Offset == -1)
    {
        return false;
    }

    auto tryReadAtOffset = [&](i32 arrayOffset) -> bool
    {
        uptr arrData = 0;
        i32 arrCount = 0;
        i32 arrMax = 0;
        if (!detail::ReadTransformArrayMeta(
                Mem(),
                meshComponent,
                arrayOffset,
                arrData,
                arrCount,
                arrMax))
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
            struct FTransformF
            {
                float data[12];
            };

            std::vector<FTransformF> rawBones(arrCount);
            if (!Mem().Read(arrData, rawBones.data(), arrCount * sizeof(FTransformF)))
            {
                return false;
            }
            outBones.resize(arrCount);
            for (i32 i = 0; i < arrCount; ++i)
            {
                auto& r = rawBones[i];
                outBones[i].Rotation = { r.data[0], r.data[1], r.data[2], r.data[3] };
                outBones[i].Translation = { r.data[4], r.data[5], r.data[6] };
                outBones[i].Scale3D = { r.data[8], r.data[9], r.data[10] };
            }
        }

        return true;
    };

    if (tryReadAtOffset(off.ComponentSpaceTransforms_Offset))
    {
        return true;
    }

    return false;
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
