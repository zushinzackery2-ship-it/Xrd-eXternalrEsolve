#pragma once
// Xrd-eXternalrEsolve - 结构体预定义成员/函数
// 对标 Rei-Dumper 的 FVector/FVector2D/FRotator 预定义内容
// 包含 UnderlayingType、构造函数、运算符重载等

#include <string>

namespace xrd
{
namespace detail
{

// 结构体预定义 typedef（插入到成员之前）
inline std::string GetStructPredefinedTypedefs(
    const std::string& name)
{
    if (name == "Vector" || name == "Vector2D"
        || name == "Rotator")
    {
        return "\tusing UnderlayingType = float;\n\n";
    }
    return "";
}

// FVector 预定义函数（插入到成员之后）
inline std::string GetFVectorPredefinedFuncs()
{
    std::string s;
    s += "\npublic:\n";
    s += "\tconstexpr FVector(UnderlayingType X = 0, "
         "UnderlayingType Y = 0, UnderlayingType Z = 0)\n";
    s += "\t\t: X(X), Y(Y), Z(Z)\n\t{\n\t}\n";
    s += "\tconstexpr FVector(const FVector& other)\n";
    s += "\t\t: X(other.X), Y(other.Y), Z(other.Z)\n";
    s += "\t{\n\t}\n";
    s += "\tFVector& Normalize()\n\t{\n";
    s += "\t\t*this /= Magnitude();\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector& operator*=(UnderlayingType Scalar)\n\t{\n";
    s += "\t\t*this = *this * Scalar;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector& operator*=(const FVector& Other)\n\t{\n";
    s += "\t\t*this = *this * Other;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector& operator+=(const FVector& Other)\n\t{\n";
    s += "\t\t*this = *this + Other;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector& operator-=(const FVector& Other)\n\t{\n";
    s += "\t\t*this = *this - Other;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector& operator/=(UnderlayingType Scalar)\n\t{\n";
    s += "\t\t*this = *this / Scalar;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector& operator/=(const FVector& Other)\n\t{\n";
    s += "\t\t*this = *this / Other;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector& operator=(const FVector& other)\n\t{\n";
    s += "\t\tX = other.X;\n\t\tY = other.Y;\n\t\tZ = other.Z;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\n";
    s += "\tUnderlayingType Dot(const FVector& Other) const\n\t{\n";
    s += "\t\treturn (X * Other.X) + (Y * Other.Y) + (Z * Other.Z);\n\t}\n";
    s += "\tUnderlayingType GetDistanceTo(const FVector& Other) const\n\t{\n";
    s += "\t\tFVector DiffVector = Other - *this;\n\t\n";
    s += "\t\treturn DiffVector.Magnitude();\n\t}\n";
    s += "\tUnderlayingType GetDistanceToInMeters(const FVector& Other) const\n\t{\n";
    s += "\t\treturn GetDistanceTo(Other) * static_cast<UnderlayingType>(0.01);\n\t}\n";
    s += "\tFVector GetNormalized() const\n\t{\n";
    s += "\t\treturn *this / Magnitude();\n\t}\n";
    s += "\tbool IsZero() const\n\t{\n";
    s += "\t\treturn X == 0 && Y == 0 && Z == 0;\n\t}\n";
    s += "\tUnderlayingType Magnitude() const\n\t{\n";
    s += "\t\treturn std::sqrt((X * X) + (Y * Y) + (Z * Z));\n\t}\n";
    s += "\tbool operator!=(const FVector& Other) const\n\t{\n";
    s += "\t\treturn X != Other.X || Y != Other.Y || Z != Other.Z;\n\t}\n";
    s += "\tFVector operator*(UnderlayingType Scalar) const\n\t{\n";
    s += "\t\treturn { X * Scalar, Y * Scalar, Z * Scalar };\n\t}\n";
    s += "\tFVector operator*(const FVector& Other) const\n\t{\n";
    s += "\t\treturn { X * Other.X, Y * Other.Y, Z * Other.Z };\n\t}\n";
    s += "\tFVector operator+(const FVector& Other) const\n\t{\n";
    s += "\t\treturn { X + Other.X, Y + Other.Y, Z + Other.Z };\n\t}\n";
    s += "\tFVector operator-(const FVector& Other) const\n\t{\n";
    s += "\t\treturn { X - Other.X, Y - Other.Y, Z - Other.Z };\n\t}\n";
    s += "\tFVector operator/(UnderlayingType Scalar) const\n\t{\n";
    s += "\t\tif (Scalar == 0)\n\t\t\treturn *this;\n\t\n";
    s += "\t\treturn { X / Scalar, Y / Scalar, Z / Scalar };\n\t}\n";
    s += "\tFVector operator/(const FVector& Other) const\n\t{\n";
    s += "\t\tif (Other.X == 0 || Other.Y == 0 || Other.Z == 0)\n";
    s += "\t\t\treturn *this;\n\t\n";
    s += "\t\treturn { X / Other.X, Y / Other.Y, Z / Other.Z };\n\t}\n";
    s += "\tbool operator==(const FVector& Other) const\n\t{\n";
    s += "\t\treturn X == Other.X && Y == Other.Y && Z == Other.Z;\n\t}\n";
    return s;
}

// FVector2D 预定义函数
inline std::string GetFVector2DPredefinedFuncs()
{
    std::string s;
    s += "\npublic:\n";
    s += "\tconstexpr FVector2D(UnderlayingType X = 0, "
         "UnderlayingType Y = 0)\n";
    s += "\t\t: X(X), Y(Y)\n\t{\n\t}\n";
    s += "\tconstexpr FVector2D(const FVector2D& other)\n";
    s += "\t\t: X(other.X), Y(other.Y)\n\t{\n\t}\n";
    s += "\tFVector2D& Normalize()\n\t{\n";
    s += "\t\t*this /= Magnitude();\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector2D& operator*=(UnderlayingType Scalar)\n\t{\n";
    s += "\t\t*this = *this * Scalar;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector2D& operator*=(const FVector2D& Other)\n\t{\n";
    s += "\t\t*this = *this * Other;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector2D& operator+=(const FVector2D& Other)\n\t{\n";
    s += "\t\t*this = *this + Other;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector2D& operator-=(const FVector2D& Other)\n\t{\n";
    s += "\t\t*this = *this - Other;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector2D& operator/=(UnderlayingType Scalar)\n\t{\n";
    s += "\t\t*this = *this / Scalar;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector2D& operator/=(const FVector2D& Other)\n\t{\n";
    s += "\t\t*this = *this / Other;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFVector2D& operator=(const FVector2D& other)\n\t{\n";
    s += "\t\tX = other.X;\n\t\tY = other.Y;\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\n";
    s += "\tUnderlayingType Dot(const FVector2D& Other) const\n\t{\n";
    s += "\t\treturn (X * Other.X) + (Y * Other.Y);\n\t}\n";
    s += "\tUnderlayingType GetDistanceTo(const FVector2D& Other) const\n\t{\n";
    s += "\t\tFVector2D DiffVector = Other - *this;\n\t\n";
    s += "\t\treturn DiffVector.Magnitude();\n\t}\n";
    s += "\tFVector2D GetNormalized() const\n\t{\n";
    s += "\t\treturn *this / Magnitude();\n\t}\n";
    s += "\tbool IsZero() const\n\t{\n";
    s += "\t\treturn X == 0 && Y == 0;\n\t}\n";
    s += "\tUnderlayingType Magnitude() const\n\t{\n";
    s += "\t\treturn std::sqrt((X * X) + (Y * Y));\n\t}\n";
    s += "\tbool operator!=(const FVector2D& Other) const\n\t{\n";
    s += "\t\treturn X != Other.X || Y != Other.Y;\n\t}\n";
    s += "\tFVector2D operator*(UnderlayingType Scalar) const\n\t{\n";
    s += "\t\treturn { X * Scalar, Y * Scalar };\n\t}\n";
    s += "\tFVector2D operator*(const FVector2D& Other) const\n\t{\n";
    s += "\t\treturn { X * Other.X, Y * Other.Y };\n\t}\n";
    s += "\tFVector2D operator+(const FVector2D& Other) const\n\t{\n";
    s += "\t\treturn { X + Other.X, Y + Other.Y };\n\t}\n";
    s += "\tFVector2D operator-(const FVector2D& Other) const\n\t{\n";
    s += "\t\treturn { X - Other.X, Y - Other.Y };\n\t}\n";
    s += "\tFVector2D operator/(UnderlayingType Scalar) const\n\t{\n";
    s += "\t\tif (Scalar == 0)\n\t\t\treturn *this;\n\t\n";
    s += "\t\treturn { X / Scalar, Y / Scalar };\n\t}\n";
    s += "\tFVector2D operator/(const FVector2D& Other) const\n\t{\n";
    s += "\t\tif (Other.X == 0 || Other.Y == 0)\n";
    s += "\t\t\treturn *this;\n\t\n";
    s += "\t\treturn { X / Other.X, Y / Other.Y };\n\t}\n";
    s += "\tbool operator==(const FVector2D& Other) const\n\t{\n";
    s += "\t\treturn X == Other.X && Y == Other.Y;\n\t}\n";
    return s;
}

// FRotator 预定义函数
inline std::string GetFRotatorPredefinedFuncs()
{
    std::string s;
    s += "\npublic:\n";
    s += "\tstatic UnderlayingType ClampAxis(UnderlayingType Angle)\n\t{\n";
    s += "\t\tAngle = std::fmod(Angle, static_cast<UnderlayingType>(360));\n";
    s += "\t\tif (Angle < static_cast<UnderlayingType>(0))\n";
    s += "\t\t\tAngle += static_cast<UnderlayingType>(360);\n\t\n";
    s += "\t\treturn Angle;\n\t}\n";
    s += "\tstatic UnderlayingType NormalizeAxis(UnderlayingType Angle)\n\t{\n";
    s += "\t\tAngle = ClampAxis(Angle);\n";
    s += "\t\tif (Angle > static_cast<UnderlayingType>(180))\n";
    s += "\t\t\tAngle -= static_cast<UnderlayingType>(360);\n\t\n";
    s += "\t\treturn Angle;\n\t}\n";
    s += "\n";
    s += "\tFRotator& Clamp()\n\t{\n";
    s += "\t\tPitch = ClampAxis(Pitch);\n";
    s += "\t\tYaw = ClampAxis(Yaw);\n";
    s += "\t\tRoll = ClampAxis(Roll);\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tconstexpr FRotator(UnderlayingType Pitch = 0, "
         "UnderlayingType Yaw = 0, UnderlayingType Roll = 0)\n";
    s += "\t\t: Pitch(Pitch), Yaw(Yaw), Roll(Roll)\n\t{\n\t}\n";
    s += "\tconstexpr FRotator(const FRotator& other)\n";
    s += "\t\t: Pitch(other.Pitch), Yaw(other.Yaw), Roll(other.Roll)\n";
    s += "\t{\n\t}\n";
    s += "\tFRotator& Normalize()\n\t{\n";
    s += "\t\tPitch = NormalizeAxis(Pitch);\n";
    s += "\t\tYaw = NormalizeAxis(Yaw);\n";
    s += "\t\tRoll = NormalizeAxis(Roll);\n\t\n";
    s += "\t\treturn *this;\n\t}\n";
    s += "\tFRotator& operator*=(UnderlayingType Scalar)\n\t{\n";
    s += "\t\t*this = *this * Scalar;\n\t\n\t\treturn *this;\n\t}\n";
    s += "\tFRotator& operator*=(const FRotator& Other)\n\t{\n";
    s += "\t\t*this = *this * Other;\n\t\n\t\treturn *this;\n\t}\n";
    s += "\tFRotator& operator+=(const FRotator& Other)\n\t{\n";
    s += "\t\t*this = *this + Other;\n\t\n\t\treturn *this;\n\t}\n";
    s += "\tFRotator& operator-=(const FRotator& Other)\n\t{\n";
    s += "\t\t*this = *this - Other;\n\t\n\t\treturn *this;\n\t}\n";
    s += "\tFRotator& operator/=(UnderlayingType Scalar)\n\t{\n";
    s += "\t\t*this = *this / Scalar;\n\t\n\t\treturn *this;\n\t}\n";
    s += "\tFRotator& operator/=(const FRotator& Other)\n\t{\n";
    s += "\t\t*this = *this / Other;\n\t\n\t\treturn *this;\n\t}\n";
    s += "\tFRotator& operator=(const FRotator& other)\n\t{\n";
    s += "\t\tPitch = other.Pitch;\n\t\tYaw = other.Yaw;\n";
    s += "\t\tRoll = other.Roll;\n\t\n\t\treturn *this;\n\t}\n";
    s += "\n";
    s += "\tFRotator GetNormalized() const\n\t{\n";
    s += "\t\tFRotator rotator = *this;\n";
    s += "\t\trotator.Normalize();\n\t\n";
    s += "\t\treturn rotator;\n\t}\n";
    s += "\tbool IsZero() const\n\t{\n";
    s += "\t\treturn ClampAxis(Pitch) == 0 && ClampAxis(Yaw) == 0 && ClampAxis(Roll) == 0;\n\t}\n";
    s += "\tbool operator!=(const FRotator& Other) const\n\t{\n";
    s += "\t\treturn Pitch != Other.Pitch || Yaw != Other.Yaw || Roll != Other.Roll;\n\t}\n";
    s += "\tFRotator operator*(UnderlayingType Scalar) const\n\t{\n";
    s += "\t\treturn { Pitch * Scalar, Yaw * Scalar, Roll * Scalar };\n\t}\n";
    s += "\tFRotator operator*(const FRotator& Other) const\n\t{\n";
    s += "\t\treturn { Pitch * Other.Pitch, Yaw * Other.Yaw, Roll * Other.Roll };\n\t}\n";
    s += "\tFRotator operator+(const FRotator& Other) const\n\t{\n";
    s += "\t\treturn { Pitch + Other.Pitch, Yaw + Other.Yaw, Roll + Other.Roll };\n\t}\n";
    s += "\tFRotator operator-(const FRotator& Other) const\n\t{\n";
    s += "\t\treturn { Pitch - Other.Pitch, Yaw - Other.Yaw, Roll - Other.Roll };\n\t}\n";
    s += "\tFRotator operator/(UnderlayingType Scalar) const\n\t{\n";
    s += "\t\tif (Scalar == 0)\n\t\t\treturn *this;\n\t\n";
    s += "\t\treturn { Pitch / Scalar, Yaw / Scalar, Roll / Scalar };\n\t}\n";
    s += "\tFRotator operator/(const FRotator& Other) const\n\t{\n";
    s += "\t\tif (Other.Pitch == 0 || Other.Yaw == 0 || Other.Roll == 0)\n";
    s += "\t\t\treturn *this;\n\t\n";
    s += "\t\treturn { Pitch / Other.Pitch, Yaw / Other.Yaw, Roll / Other.Roll };\n\t}\n";
    s += "\tbool operator==(const FRotator& Other) const\n\t{\n";
    s += "\t\treturn Pitch == Other.Pitch && Yaw == Other.Yaw && Roll == Other.Roll;\n\t}\n";
    return s;
}

// 获取结构体预定义函数（插入到成员之后，}; 之前）
inline std::string GetStructPredefinedFunctions(
    const std::string& name)
{
    if (name == "Vector")   return GetFVectorPredefinedFuncs();
    if (name == "Vector2D") return GetFVector2DPredefinedFuncs();
    if (name == "Rotator")  return GetFRotatorPredefinedFuncs();
    return "";
}

} // namespace detail
} // namespace xrd
