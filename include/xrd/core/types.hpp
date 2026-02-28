#pragma once
// Xrd-eXternalrEsolve - 基础类型定义
// 类型别名、指针校验、FVector/FTransform 等数学结构体

// 防止 Windows.h 的 min/max 宏污染 std::min/std::max
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <limits>

namespace xrd
{

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using uptr = std::uintptr_t;

// 运行时检测后的浮点类型，用 double 存储以兼容 UE4(float) 和 UE5(double)
using FReal = double;

// 验证指针是否在用户态合法范围内
inline bool IsCanonicalUserPtr(uptr ptr)
{
    if (ptr == 0)
    {
        return false;
    }
    constexpr uptr kUserMin = 0x0000000000010000ull;
    constexpr uptr kUserMax = 0x00007FFFFFFFFFFFull;
    return ptr >= kUserMin && ptr <= kUserMax;
}

inline bool IsLikelyPtr(uptr ptr)
{
    return IsCanonicalUserPtr(ptr);
}

// 检查浮点数是否有限（非 NaN、非 Inf）
inline bool IsFiniteReal(FReal v)
{
    return std::isfinite(v);
}

// ─── 数学结构体 ───

struct FVector
{
    FReal X = 0, Y = 0, Z = 0;
};

struct FVector2D
{
    FReal X = 0, Y = 0;
};

struct FQuat
{
    FReal X = 0, Y = 0, Z = 0, W = 1;
};

struct FTransform
{
    FQuat   Rotation;
    FVector Translation;
    FReal   Pad0 = 0;
    FVector Scale3D;
    FReal   Pad1 = 0;
};

struct FMatrix
{
    FReal M[4][4] = {};
};

// 验证四元数是否接近单位长度
inline bool IsReasonableQuaternion(const FQuat& q)
{
    FReal len2 = q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W;
    return IsFiniteReal(len2) && len2 > 0.5 && len2 < 1.5;
}

// 验证 FTransform 各分量是否合理
inline bool IsLikelyValidTransform(const FTransform& t)
{
    if (!IsReasonableQuaternion(t.Rotation))
    {
        return false;
    }
    if (!IsFiniteReal(t.Translation.X) ||
        !IsFiniteReal(t.Translation.Y) ||
        !IsFiniteReal(t.Translation.Z))
    {
        return false;
    }

    auto absVal = [](FReal s) { return s < 0 ? -s : s; };

    if (absVal(t.Scale3D.X) < 0.001 || absVal(t.Scale3D.X) > 1000.0)
    {
        return false;
    }
    if (absVal(t.Scale3D.Y) < 0.001 || absVal(t.Scale3D.Y) > 1000.0)
    {
        return false;
    }
    if (absVal(t.Scale3D.Z) < 0.001 || absVal(t.Scale3D.Z) > 1000.0)
    {
        return false;
    }
    return true;
}

// UE 的 FName 紧凑表示
struct FName
{
    i32 ComparisonIndex = 0;
    i32 Number = 0;
};

} // namespace xrd
