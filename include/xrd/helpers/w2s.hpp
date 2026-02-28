#pragma once
// Xrd-eXternalrEsolve - 世界坐标到屏幕坐标
// ViewProjection 矩阵读取和坐标变换

#include "../core/context.hpp"
#include "../engine/bones.hpp"
#include "../engine/world_actors.hpp"
#include <cmath>

namespace xrd
{

// 通过 DebugCanvasObject 链路动态读取 ViewProjection 矩阵
// 链路: [DebugCanvasObjCacheAddr] -> +0x20 -> +0x280 -> 4x4 矩阵
inline bool GetVPMatrix(FMatrix& out)
{
    out = {};
    if (!IsInited())
    {
        return false;
    }

    uptr dcoAddr = Off().DebugCanvasObjCacheAddr;
    if (dcoAddr == 0)
    {
        return false;
    }

    // [dcoAddr] -> TMap Data 数组指针
    uptr tmapData = 0;
    if (!GReadPtr(dcoAddr, tmapData) || !IsCanonicalUserPtr(tmapData))
    {
        return false;
    }

    // TMap Data + 0x20 -> Canvas 对象指针 (entry[1].Value)
    uptr canvasObj = 0;
    if (!GReadPtr(tmapData + 0x20, canvasObj) || !IsCanonicalUserPtr(canvasObj))
    {
        return false;
    }

    // Canvas 对象 + 0x280 -> ViewProjection 矩阵
    uptr matrixAddr = canvasObj + 0x280;

    if (Off().bUseDoublePrecision)
    {
        return GReadValue(matrixAddr, out);
    }
    else
    {
        struct FMatrixF { float M[4][4]; } fm{};
        if (!GReadValue(matrixAddr, fm))
        {
            return false;
        }
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                out.M[i][j] = static_cast<FReal>(fm.M[i][j]);
            }
        }
        return true;
    }
}

// 世界坐标 -> 屏幕坐标
inline bool WorldToScreen(const FVector& worldPos, i32 screenW, i32 screenH, FVector2D& outScreen)
{
    outScreen = {};

    FMatrix viewProj;
    if (!GetVPMatrix(viewProj))
    {
        return false;
    }

    FReal x = worldPos.X * viewProj.M[0][0] + worldPos.Y * viewProj.M[1][0]
            + worldPos.Z * viewProj.M[2][0] + viewProj.M[3][0];
    FReal y = worldPos.X * viewProj.M[0][1] + worldPos.Y * viewProj.M[1][1]
            + worldPos.Z * viewProj.M[2][1] + viewProj.M[3][1];
    FReal w = worldPos.X * viewProj.M[0][3] + worldPos.Y * viewProj.M[1][3]
            + worldPos.Z * viewProj.M[2][3] + viewProj.M[3][3];

    if (w <= 0.001)
    {
        return false;
    }

    FReal invW = 1.0 / w;
    outScreen.X = (screenW / 2.0) + (x * invW) * (screenW / 2.0);
    outScreen.Y = (screenH / 2.0) - (y * invW) * (screenH / 2.0);
    return true;
}

// 获取 Actor 的世界坐标
inline bool GetActorWorldPos(uptr actor, FVector& outPos)
{
    outPos = {};
    if (!actor)
    {
        return false;
    }

    // 优先读取 RootComponent
    uptr rootComp = ReadActorFieldPtr(actor, "RootComponent");
    if (!rootComp)
    {
        // 回退到 Mesh
        rootComp = ReadActorFieldPtr(actor, "Mesh");
    }
    if (!rootComp)
    {
        return false;
    }

    FTransform c2w;
    if (!ReadComponentToWorld(rootComp, c2w))
    {
        return false;
    }

    outPos = c2w.Translation;
    return IsFiniteReal(outPos.X) && IsFiniteReal(outPos.Y) && IsFiniteReal(outPos.Z);
}

} // namespace xrd
