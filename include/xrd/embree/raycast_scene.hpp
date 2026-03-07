#pragma once
// Xrd-eXternalrEsolve - Embree 射线遮挡查询场景（header-only）
// 消费者需要链接 embree3.lib 并包含 Embree SDK 头文件

#include "tessellation.hpp"
#include <embree3/rtcore.h>
#include <atomic>
#include <cmath>
#include <iostream>

namespace xrd
{

// Embree 射线遮挡查询场景（引擎无关）
class EmbreeScene
{
public:
    static bool Initialize()
    {
        if (s_ready)
        {
            return true;
        }

        std::cerr << "[Embree] Initializing...\n";
        s_device = rtcNewDevice(nullptr);
        if (!s_device)
        {
            std::cerr << "[Embree] FAIL: rtcNewDevice returned null\n";
            return false;
        }

        std::cerr << "[Embree] Device created OK\n";
        s_ready = true;
        return true;
    }

    static void Shutdown()
    {
        RTCScene old = s_scene.exchange(nullptr);
        if (old)
        {
            rtcReleaseScene(old);
        }
        if (s_device)
        {
            rtcReleaseDevice(s_device);
            s_device = nullptr;
        }
        s_ready = false;
        std::cerr << "[Embree] Shutdown\n";
    }

    static bool IsReady()
    {
        return s_ready;
    }

    // 从碰撞数据重建 Embree 场景
    static void RebuildScene(const CollisionScene& data)
    {
        TriMesh mesh = TessellateScene(data, true);

        unsigned numTris = (unsigned)(mesh.indices.size() / 3);
        unsigned numVerts = (unsigned)(mesh.verts.size() / 3);

        if (numTris == 0)
        {
            return;
        }

        RTCScene newScene = rtcNewScene(s_device);
        RTCGeometry geom = rtcNewGeometry(s_device, RTC_GEOMETRY_TYPE_TRIANGLE);

        float* vb = (float*)rtcSetNewGeometryBuffer(
            geom, RTC_BUFFER_TYPE_VERTEX, 0,
            RTC_FORMAT_FLOAT3, 3 * sizeof(float), numVerts);
        memcpy(vb, mesh.verts.data(), numVerts * 3 * sizeof(float));

        unsigned* ib = (unsigned*)rtcSetNewGeometryBuffer(
            geom, RTC_BUFFER_TYPE_INDEX, 0,
            RTC_FORMAT_UINT3, 3 * sizeof(unsigned), numTris);
        memcpy(ib, mesh.indices.data(), numTris * 3 * sizeof(unsigned));

        rtcCommitGeometry(geom);
        rtcAttachGeometry(newScene, geom);
        rtcReleaseGeometry(geom);
        rtcCommitScene(newScene);

        // 无锁原子替换
        RTCScene old = s_scene.exchange(newScene);
        if (old)
        {
            rtcReleaseScene(old);
        }

        std::cerr << "[Embree] Scene built: " << numVerts << " verts, "
                  << numTris << " tris\n";
    }

    // 射线遮挡查询：origin → target 之间是否有碰撞体
    static bool IsOccluded(
        float ox, float oy, float oz,
        float tx, float ty, float tz)
    {
        if (!s_ready)
        {
            return false;
        }

        RTCScene scene = s_scene.load();
        if (!scene)
        {
            return false;
        }

        float dx = tx - ox;
        float dy = ty - oy;
        float dz = tz - oz;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist < 1e-4f)
        {
            return false;
        }

        float inv = 1.0f / dist;
        dx *= inv;
        dy *= inv;
        dz *= inv;

        RTCRay ray{};
        ray.org_x = ox;
        ray.org_y = oy;
        ray.org_z = oz;
        ray.dir_x = dx;
        ray.dir_y = dy;
        ray.dir_z = dz;
        ray.tnear = 0.0f;
        ray.tfar  = dist;
        ray.mask  = 0xFFFFFFFF;
        ray.flags = 0;

        RTCIntersectContext ctx;
        rtcInitIntersectContext(&ctx);

        rtcOccluded1(scene, &ctx, &ray);

        return (ray.tfar < 0.0f);
    }

    // 沿骨骼线段多点采样遮挡比例 [0.0, 1.0]
    static float GetOcclusionRatio(
        float camX, float camY, float camZ,
        float t1x, float t1y, float t1z,
        float t2x, float t2y, float t2z,
        int sampleCount = 5)
    {
        if (!s_ready || sampleCount < 1)
        {
            return 0.0f;
        }

        int occludedCount = 0;
        for (int i = 0; i < sampleCount; ++i)
        {
            float t = (sampleCount == 1) ? 0.5f : (float)i / (float)(sampleCount - 1);
            float px = t1x + (t2x - t1x) * t;
            float py = t1y + (t2y - t1y) * t;
            float pz = t1z + (t2z - t1z) * t;

            if (IsOccluded(camX, camY, camZ, px, py, pz))
            {
                ++occludedCount;
            }
        }

        return (float)occludedCount / (float)sampleCount;
    }

private:
    // C++17 inline static 保证跨 TU 唯一
    static inline RTCDevice s_device = nullptr;
    static inline std::atomic<RTCScene> s_scene{nullptr};
    static inline bool s_ready = false;
};

} // namespace xrd
