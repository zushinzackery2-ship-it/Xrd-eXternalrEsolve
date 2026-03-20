#pragma once
// Xrd-eXternalrEsolve - 骨骼偏移辅助
// ComponentSpaceTransforms 只允许在 USkinnedMeshComponent 自身成员范围内
// 通过双 TArray 特征发现，避免把子类私有数组误判成骨骼数组。

#include "../../core/context.hpp"
#include "../world/world.hpp"
#include "../objects/objects_search.hpp"
#include <vector>
#include <cmath>
#include <sstream>
#include <mutex>

namespace xrd
{
namespace detail
{
    struct StructMemberRange
    {
        i32 begin = -1;
        i32 end = -1;

        bool IsValid() const
        {
            return begin >= 0 && end > begin;
        }
    };

    inline std::recursive_mutex& BoneRuntimeResolveMutex()
    {
        static std::recursive_mutex mutex;
        return mutex;
    }

    inline bool IsOffsetInRange(i32 offset, const StructMemberRange& range)
    {
        return range.IsValid() && offset >= range.begin && offset < range.end;
    }

    inline StructMemberRange GetSkinnedMeshComponentMemberRange()
    {
        static std::mutex mutex;
        static u32 cachedPid = 0;
        static StructMemberRange cachedRange{};
        static bool resolved = false;

        std::lock_guard<std::mutex> lock(mutex);
        if (cachedPid != Ctx().pid)
        {
            cachedPid = Ctx().pid;
            cachedRange = {};
            resolved = false;
        }

        if (resolved)
        {
            return cachedRange;
        }

        resolved = true;

        uptr cls = FindClassByName("SkinnedMeshComponent");
        if (!cls)
        {
            std::cerr << "[xrd] 未找到类 SkinnedMeshComponent，无法限定骨骼偏移范围\n";
            return cachedRange;
        }

        uptr super = GetSuperStruct(cls);
        i32 begin = super ? GetStructSize(super) : 0;
        i32 end = GetStructSize(cls);

        if (begin < 0 || end <= begin)
        {
            std::cerr << "[xrd] SkinnedMeshComponent 成员范围无效: begin=0x"
                      << std::hex << begin << " end=0x" << end << std::dec << "\n";
            return cachedRange;
        }

        cachedRange.begin = begin;
        cachedRange.end = end;

        std::cerr << "[xrd] USkinnedMeshComponent 自身成员范围: [0x"
                  << std::hex << begin << ", 0x" << end << ")" << std::dec << "\n";
        return cachedRange;
    }

    inline bool ReadTransformSample(
        const IMemoryAccessor& mem,
        uptr address,
        bool isDouble,
        FTransform& out)
    {
        if (!IsCanonicalUserPtr(address))
        {
            return false;
        }

        if (isDouble)
        {
            return ReadValue(mem, address, out);
        }

        struct FTransformF
        {
            float Rot[4];
            float Trans[3];
            float Pad0;
            float Scale[3];
            float Pad1;
        } tf{};

        if (!ReadValue(mem, address, tf))
        {
            return false;
        }

        out.Rotation = { tf.Rot[0], tf.Rot[1], tf.Rot[2], tf.Rot[3] };
        out.Translation = { tf.Trans[0], tf.Trans[1], tf.Trans[2] };
        out.Scale3D = { tf.Scale[0], tf.Scale[1], tf.Scale[2] };
        return true;
    }

    inline bool ReadTransformArrayMeta(
        const IMemoryAccessor& mem,
        uptr meshComponent,
        i32 arrayOffset,
        uptr& outData,
        i32& outCount,
        i32& outMax)
    {
        outData = 0;
        outCount = 0;
        outMax = 0;

        if (!IsCanonicalUserPtr(meshComponent) || arrayOffset < 0)
        {
            return false;
        }

        if (!ReadPtr(mem, meshComponent + arrayOffset, outData))
        {
            return false;
        }
        if (!IsCanonicalUserPtr(outData))
        {
            return false;
        }

        ReadValue(mem, meshComponent + arrayOffset + 8, outCount);
        ReadValue(mem, meshComponent + arrayOffset + 12, outMax);

        if (outCount <= 0 || outCount > 500 || outMax < outCount || outMax > 500)
        {
            return false;
        }

        return true;
    }

    inline int ScoreTransformArrayOffset(
        const IMemoryAccessor& mem,
        uptr meshComponent,
        i32 arrayOffset,
        bool isDouble)
    {
        uptr arrayData = 0;
        i32 arrayCount = 0;
        i32 arrayMax = 0;
        if (!ReadTransformArrayMeta(mem, meshComponent, arrayOffset, arrayData, arrayCount, arrayMax))
        {
            return -1;
        }

        const i32 sampleCount = arrayCount < 16 ? arrayCount : 16;
        std::vector<FTransform> samples;
        samples.reserve(sampleCount);

        int validCount = 0;
        double magnitudeSum = 0.0;
        double maxDistance = 0.0;

        for (i32 index = 0; index < sampleCount; ++index)
        {
            FTransform transform{};
            if (!ReadTransformSample(
                    mem,
                    arrayData + static_cast<uptr>(index) * (isDouble ? 0x60 : 0x30),
                    isDouble,
                    transform))
            {
                continue;
            }

            if (!IsLikelyValidTransform(transform))
            {
                continue;
            }

            samples.push_back(transform);
            ++validCount;

            const double x = transform.Translation.X;
            const double y = transform.Translation.Y;
            const double z = transform.Translation.Z;
            const double magnitude = std::sqrt(x * x + y * y + z * z);
            if (std::isfinite(magnitude) && magnitude > 0.1 && magnitude < 5000.0)
            {
                magnitudeSum += magnitude;
            }
        }

        if (validCount < 2)
        {
            return -1;
        }

        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            for (std::size_t j = i + 1; j < samples.size(); ++j)
            {
                const double dx = samples[i].Translation.X - samples[j].Translation.X;
                const double dy = samples[i].Translation.Y - samples[j].Translation.Y;
                const double dz = samples[i].Translation.Z - samples[j].Translation.Z;
                const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (std::isfinite(distance) && distance > maxDistance)
                {
                    maxDistance = distance;
                }
            }
        }

        const double averageMagnitude = magnitudeSum / static_cast<double>(validCount);

        int score = validCount * 100;
        if (std::isfinite(averageMagnitude) && averageMagnitude > 1.0 && averageMagnitude < 2000.0)
        {
            score += static_cast<int>(averageMagnitude > 300.0 ? 300.0 : averageMagnitude);
        }
        if (std::isfinite(maxDistance) && maxDistance > 1.0 && maxDistance < 2000.0)
        {
            score += static_cast<int>(maxDistance > 300.0 ? 300.0 : maxDistance);
        }

        return score;
    }

    inline void AddTransformArrayCandidate(
        std::vector<std::pair<i32, int>>& candidates,
        i32 arrayOffset,
        int bonus,
        const StructMemberRange& allowedRange)
    {
        if (!IsOffsetInRange(arrayOffset, allowedRange))
        {
            return;
        }

        for (auto& candidate : candidates)
        {
            if (candidate.first == arrayOffset)
            {
                if (bonus > candidate.second)
                {
                    candidate.second = bonus;
                }
                return;
            }
        }

        candidates.emplace_back(arrayOffset, bonus);
    }

    inline i32 DiscoverComponentSpaceTransformsOffset(uptr meshComponent, bool isDouble)
    {
        const StructMemberRange allowedRange = GetSkinnedMeshComponentMemberRange();
        if (!allowedRange.IsValid())
        {
            return -1;
        }

        constexpr u32 searchSize = 0x1000;
        std::vector<std::pair<i32, int>> candidates;
        std::vector<u8> buf(searchSize);
        if (Mem().Read(meshComponent, buf.data(), searchSize))
        {
            u32 begin = static_cast<u32>(allowedRange.begin);
            u32 end = static_cast<u32>(allowedRange.end);

            if ((begin & 0x7u) != 0)
            {
                begin = (begin + 7u) & ~0x7u;
            }
            if (end > searchSize)
            {
                end = searchSize;
            }

            for (u32 offset = begin; offset + 32 <= end; offset += 8)
            {
                u64 ptr1 = *reinterpret_cast<u64*>(&buf[offset]);
                i32 count1 = *reinterpret_cast<i32*>(&buf[offset + 8]);
                i32 max1 = *reinterpret_cast<i32*>(&buf[offset + 12]);

                u64 ptr2 = *reinterpret_cast<u64*>(&buf[offset + 16]);
                i32 count2 = *reinterpret_cast<i32*>(&buf[offset + 24]);
                i32 max2 = *reinterpret_cast<i32*>(&buf[offset + 28]);

                if (ptr1 == 0 || ptr2 == 0 || ptr1 == ptr2)
                {
                    continue;
                }

                if (count1 != count2 || max1 != max2)
                {
                    continue;
                }

                if (count1 <= 0 || count1 >= 1000 || max1 <= 0 || max1 >= 1000)
                {
                    continue;
                }

                AddTransformArrayCandidate(
                    candidates,
                    static_cast<i32>(offset),
                    80,
                    allowedRange
                );
                AddTransformArrayCandidate(
                    candidates,
                    static_cast<i32>(offset + 0x10),
                    120,
                    allowedRange
                );
            }
        }

        i32 bestOffset = -1;
        int bestScore = -1;
        std::ostringstream candidateLog;
        bool hasLoggedCandidate = false;
        for (const auto& candidate : candidates)
        {
            int score = ScoreTransformArrayOffset(
                Mem(),
                meshComponent,
                candidate.first,
                isDouble
            );
            if (score < 0)
            {
                continue;
            }

            score += candidate.second;
            if (hasLoggedCandidate)
            {
                candidateLog << ' ';
            }
            candidateLog << "0x" << std::hex << candidate.first << std::dec
                         << '(' << score << ')';
            hasLoggedCandidate = true;

            if (score > bestScore)
            {
                bestScore = score;
                bestOffset = candidate.first;
            }
        }

        if (hasLoggedCandidate && bestOffset != -1)
        {
            std::cerr << "[xrd] ComponentSpaceTransforms 候选: "
                      << candidateLog.str()
                      << " -> 0x" << std::hex << bestOffset << std::dec << "\n";
        }

        return bestOffset;
    }
} // namespace detail
} // namespace xrd
