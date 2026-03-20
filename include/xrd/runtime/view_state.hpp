#pragma once
// Xrd-eXternalrEsolve - VP / 相机状态缓存
// 上层通常都需要同一套 VP 读取、相机提取与线程安全缓存，因此统一放到 xrd。

#include "../core/types.hpp"
#include <mutex>
#include <cmath>

namespace xrd
{
inline bool ExtractCameraPosFromVP(const FMatrix& vp, float outPos[3])
{
    double a00 = vp.M[0][0];
    double a01 = vp.M[1][0];
    double a02 = vp.M[2][0];
    double b0 = -vp.M[3][0];

    double a10 = vp.M[0][1];
    double a11 = vp.M[1][1];
    double a12 = vp.M[2][1];
    double b1 = -vp.M[3][1];

    double a20 = vp.M[0][3];
    double a21 = vp.M[1][3];
    double a22 = vp.M[2][3];
    double b2 = -vp.M[3][3];

    double det = a00 * (a11 * a22 - a12 * a21)
        - a01 * (a10 * a22 - a12 * a20)
        + a02 * (a10 * a21 - a11 * a20);

    if (std::abs(det) < 1e-12)
    {
        return false;
    }

    double invDet = 1.0 / det;
    outPos[0] = static_cast<float>(
        (b0 * (a11 * a22 - a12 * a21) - a01 * (b1 * a22 - a12 * b2) + a02 * (b1 * a21 - a11 * b2))
        * invDet
    );
    outPos[1] = static_cast<float>(
        (a00 * (b1 * a22 - a12 * b2) - b0 * (a10 * a22 - a12 * a20) + a02 * (a10 * b2 - b1 * a20))
        * invDet
    );
    outPos[2] = static_cast<float>(
        (a00 * (a11 * b2 - b1 * a21) - a01 * (a10 * b2 - b1 * a20) + b0 * (a10 * a21 - a11 * a20))
        * invDet
    );
    return true;
}

struct ViewStateCache
{
    mutable std::mutex mutex;
    FMatrix matrix{};
    bool valid = false;
    float cameraWorldPos[3] = {0.0f, 0.0f, 0.0f};
    bool hasCameraPos = false;

    bool Get(FMatrix& out) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!valid)
        {
            return false;
        }

        out = matrix;
        return true;
    }

    bool GetCameraPos(float outPos[3]) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!hasCameraPos)
        {
            return false;
        }

        outPos[0] = cameraWorldPos[0];
        outPos[1] = cameraWorldPos[1];
        outPos[2] = cameraWorldPos[2];
        return true;
    }

    void Reset()
    {
        std::lock_guard<std::mutex> lock(mutex);
        matrix = FMatrix{};
        valid = false;
        cameraWorldPos[0] = 0.0f;
        cameraWorldPos[1] = 0.0f;
        cameraWorldPos[2] = 0.0f;
        hasCameraPos = false;
    }
};
} // namespace xrd
