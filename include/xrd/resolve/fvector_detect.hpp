#pragma once
// Xrd-eXternalrEsolve - FVector 精度自动检测
// 判断目标使用 UE4 (float) 还是 UE5 (double) 的 FVector

#include "../core/context.hpp"
#include <iostream>
#include <cmath>

namespace xrd
{
namespace resolve
{

// 通过读取已知 Actor 的 RootComponent 中的 FTransform 来判断精度
// 如果旋转四元数以 double 读取后接近单位长度 -> UE5
// 如果以 float 读取后接近单位长度 -> UE4
inline bool DetectDoublePrecision(
    const IMemoryAccessor& mem,
    const UEOffsets& off,
    bool& outIsDouble)
{
    outIsDouble = false;

    if (off.GWorld == 0)
    {
        std::cerr << "[xrd] GWorld 不可用，FVector 精度检测默认 float\n";
        return true;
    }

    uptr worldPtr = 0;
    if (!ReadPtr(mem, off.GWorld, worldPtr) || !IsCanonicalUserPtr(worldPtr))
    {
        return false;
    }

    // 遍历 PersistentLevel -> Actors -> RootComponent
    for (i32 levelOff = 0x28; levelOff <= 0x100; levelOff += 8)
    {
        uptr level = 0;
        if (!ReadPtr(mem, worldPtr + levelOff, level) || !IsCanonicalUserPtr(level))
        {
            continue;
        }

        for (i32 actorsOff = 0x80; actorsOff <= 0x120; actorsOff += 8)
        {
            uptr actorsData = 0;
            i32 actorsCount = 0;
            if (!ReadPtr(mem, level + actorsOff, actorsData) || !IsCanonicalUserPtr(actorsData))
            {
                continue;
            }
            ReadValue(mem, level + actorsOff + 8, actorsCount);
            if (actorsCount <= 0 || actorsCount > 100000)
            {
                continue;
            }

            for (i32 ai = 0; ai < std::min(actorsCount, 10); ++ai)
            {
                uptr actor = 0;
                if (!ReadPtr(mem, actorsData + ai * 8, actor) || !IsCanonicalUserPtr(actor))
                {
                    continue;
                }

                // 尝试不同的 RootComponent 偏移
                for (i32 rcOff = 0x130; rcOff <= 0x180; rcOff += 8)
                {
                    uptr rootComp = 0;
                    if (!ReadPtr(mem, actor + rcOff, rootComp) || !IsCanonicalUserPtr(rootComp))
                    {
                        continue;
                    }

                    // 在 ComponentToWorld 的可能范围内检测
                    for (i32 ctw = 0x100; ctw <= 0x200; ctw += 0x10)
                    {
                        // 先尝试 double
                        double qx = 0, qy = 0, qz = 0, qw = 0;
                        mem.Read(rootComp + ctw + 0x00, &qx, 8);
                        mem.Read(rootComp + ctw + 0x08, &qy, 8);
                        mem.Read(rootComp + ctw + 0x10, &qz, 8);
                        mem.Read(rootComp + ctw + 0x18, &qw, 8);

                        double dlen2 = qx * qx + qy * qy + qz * qz + qw * qw;
                        if (std::isfinite(dlen2) && dlen2 > 0.95 && dlen2 < 1.05)
                        {
                            outIsDouble = true;
                            std::cerr << "[xrd] FVector 精度: double (UE5)\n";
                            return true;
                        }

                        // 再尝试 float
                        float fqx = 0, fqy = 0, fqz = 0, fqw = 0;
                        mem.Read(rootComp + ctw + 0x00, &fqx, 4);
                        mem.Read(rootComp + ctw + 0x04, &fqy, 4);
                        mem.Read(rootComp + ctw + 0x08, &fqz, 4);
                        mem.Read(rootComp + ctw + 0x0C, &fqw, 4);

                        float flen2 = fqx * fqx + fqy * fqy + fqz * fqz + fqw * fqw;
                        if (std::isfinite(flen2) && flen2 > 0.95f && flen2 < 1.05f)
                        {
                            outIsDouble = false;
                            std::cerr << "[xrd] FVector 精度: float (UE4)\n";
                            return true;
                        }
                    }
                }
            }
        }
    }

    std::cerr << "[xrd] 无法检测 FVector 精度，默认 float\n";
    return true;
}

} // namespace resolve
} // namespace xrd
