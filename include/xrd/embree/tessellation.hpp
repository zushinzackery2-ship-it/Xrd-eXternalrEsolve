#pragma once
// Xrd-eXternalrEsolve - 碰撞几何体三角形化（引擎无关，header-only）
// 将 Box/Sphere/Capsule/ConvexMesh 转为 TriMesh 供 Embree 使用

#include "../collision/collision_types.hpp"
#include <vector>
#include <cmath>

namespace xrd
{

// 三角网格数据：顶点（x,y,z 交错）+ 三角形索引
struct TriMesh
{
    std::vector<float> verts;
    std::vector<unsigned> indices;
};

// ─── 内部工具 ───

namespace detail
{

inline void RotateByQuat(
    const Quat& q,
    float vx, float vy, float vz,
    float& ox, float& oy, float& oz)
{
    float qx = q.x, qy = q.y, qz = q.z, qw = q.w;
    float tx = 2.0f * (qy * vz - qz * vy);
    float ty = 2.0f * (qz * vx - qx * vz);
    float tz = 2.0f * (qx * vy - qy * vx);
    ox = vx + qw * tx + (qy * tz - qz * ty);
    oy = vy + qw * ty + (qz * tx - qx * tz);
    oz = vz + qw * tz + (qx * ty - qy * tx);
}

inline void TransformVertex(
    const Transform& pose,
    float lx, float ly, float lz,
    float& wx, float& wy, float& wz)
{
    RotateByQuat(pose.q, lx, ly, lz, wx, wy, wz);
    wx += pose.p.x;
    wy += pose.p.y;
    wz += pose.p.z;
}

} // namespace detail

// ─── 几何体三角形化 ───

// Box → 12 个三角形（6 面 × 2）
inline void TessellateBox(
    const Transform& pose,
    const BoxGeom& box,
    TriMesh& mesh)
{
    float hx = box.halfX, hy = box.halfY, hz = box.halfZ;

    float localVerts[8][3] =
    {
        {-hx, -hy, -hz}, { hx, -hy, -hz}, { hx,  hy, -hz}, {-hx,  hy, -hz},
        {-hx, -hy,  hz}, { hx, -hy,  hz}, { hx,  hy,  hz}, {-hx,  hy,  hz}
    };

    unsigned base = (unsigned)(mesh.verts.size() / 3);

    for (int i = 0; i < 8; ++i)
    {
        float wx, wy, wz;
        detail::TransformVertex(pose,
                                localVerts[i][0], localVerts[i][1], localVerts[i][2],
                                wx, wy, wz);
        mesh.verts.push_back(wx);
        mesh.verts.push_back(wy);
        mesh.verts.push_back(wz);
    }

    unsigned faces[12][3] =
    {
        {0,1,2}, {0,2,3},
        {4,6,5}, {4,7,6},
        {0,4,5}, {0,5,1},
        {2,6,7}, {2,7,3},
        {0,3,7}, {0,7,4},
        {1,5,6}, {1,6,2}
    };

    for (int i = 0; i < 12; ++i)
    {
        mesh.indices.push_back(base + faces[i][0]);
        mesh.indices.push_back(base + faces[i][1]);
        mesh.indices.push_back(base + faces[i][2]);
    }
}

// Sphere → 细分球体（8×6 = 96 个三角形）
inline void TessellateSphere(
    const Transform& pose,
    float radius,
    TriMesh& mesh)
{
    constexpr int SLICES = 8;
    constexpr int STACKS = 6;
    constexpr float PI = 3.14159265f;

    unsigned base = (unsigned)(mesh.verts.size() / 3);

    for (int j = 0; j <= STACKS; ++j)
    {
        float phi = PI * j / STACKS;
        float sp = sinf(phi), cp = cosf(phi);
        for (int i = 0; i <= SLICES; ++i)
        {
            float theta = 2.0f * PI * i / SLICES;
            float lx = radius * sp * cosf(theta);
            float ly = radius * sp * sinf(theta);
            float lz = radius * cp;
            float wx, wy, wz;
            detail::TransformVertex(pose, lx, ly, lz, wx, wy, wz);
            mesh.verts.push_back(wx);
            mesh.verts.push_back(wy);
            mesh.verts.push_back(wz);
        }
    }

    for (int j = 0; j < STACKS; ++j)
    {
        for (int i = 0; i < SLICES; ++i)
        {
            unsigned a = base + j * (SLICES + 1) + i;
            unsigned b = a + SLICES + 1;
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(a + 1);
            mesh.indices.push_back(a + 1);
            mesh.indices.push_back(b);
            mesh.indices.push_back(b + 1);
        }
    }
}

// Capsule → 圆柱 + 两个半球（胶囊沿 X 轴）
inline void TessellateCapsule(
    const Transform& pose,
    const CapsuleGeom& cap,
    TriMesh& mesh)
{
    constexpr int SLICES = 8;
    constexpr int HEMI_STACKS = 3;
    constexpr float PI = 3.14159265f;

    float r = cap.radius;
    float hh = cap.halfHeight;

    unsigned base = (unsigned)(mesh.verts.size() / 3);

    // 圆柱体顶点：两圈
    for (int end = 0; end < 2; ++end)
    {
        float cx = (end == 0) ? -hh : hh;
        for (int i = 0; i <= SLICES; ++i)
        {
            float theta = 2.0f * PI * i / SLICES;
            float ly = r * cosf(theta);
            float lz = r * sinf(theta);
            float wx, wy, wz;
            detail::TransformVertex(pose, cx, ly, lz, wx, wy, wz);
            mesh.verts.push_back(wx);
            mesh.verts.push_back(wy);
            mesh.verts.push_back(wz);
        }
    }

    // 圆柱侧面三角形
    for (int i = 0; i < SLICES; ++i)
    {
        unsigned a = base + i;
        unsigned b = base + (SLICES + 1) + i;
        mesh.indices.push_back(a);
        mesh.indices.push_back(b);
        mesh.indices.push_back(a + 1);
        mesh.indices.push_back(a + 1);
        mesh.indices.push_back(b);
        mesh.indices.push_back(b + 1);
    }

    // 两个半球盖
    for (int end = 0; end < 2; ++end)
    {
        float sign = (end == 0) ? -1.0f : 1.0f;
        unsigned hemiBase = (unsigned)(mesh.verts.size() / 3);

        for (int j = 0; j <= HEMI_STACKS; ++j)
        {
            float phi = (PI / 2.0f) * j / HEMI_STACKS;
            float sp = sinf(phi), cp = cosf(phi);
            for (int i = 0; i <= SLICES; ++i)
            {
                float theta = 2.0f * PI * i / SLICES;
                float lx = sign * (hh + r * sp);
                float ly = r * cp * cosf(theta);
                float lz = r * cp * sinf(theta);
                float wx, wy, wz;
                detail::TransformVertex(pose, lx, ly, lz, wx, wy, wz);
                mesh.verts.push_back(wx);
                mesh.verts.push_back(wy);
                mesh.verts.push_back(wz);
            }
        }

        for (int j = 0; j < HEMI_STACKS; ++j)
        {
            for (int i = 0; i < SLICES; ++i)
            {
                unsigned a = hemiBase + j * (SLICES + 1) + i;
                unsigned b = a + SLICES + 1;
                mesh.indices.push_back(a);
                mesh.indices.push_back(b);
                mesh.indices.push_back(a + 1);
                mesh.indices.push_back(a + 1);
                mesh.indices.push_back(b);
                mesh.indices.push_back(b + 1);
            }
        }
    }
}

// ConvexMesh → 三角形扇（每条 edge 用两端点 + 重心构成三角形）
inline void TessellateConvex(
    const Transform& pose,
    const CollisionShape& shape,
    TriMesh& mesh)
{
    if (shape.convexVerts.size() < 3)
    {
        return;
    }

    // 计算重心
    float cx = 0, cy = 0, cz = 0;
    for (auto& v : shape.convexVerts)
    {
        cx += v.x;
        cy += v.y;
        cz += v.z;
    }
    float n = (float)shape.convexVerts.size();
    cx /= n; cy /= n; cz /= n;

    unsigned base = (unsigned)(mesh.verts.size() / 3);

    for (auto& v : shape.convexVerts)
    {
        float wx, wy, wz;
        detail::TransformVertex(pose, v.x, v.y, v.z, wx, wy, wz);
        mesh.verts.push_back(wx);
        mesh.verts.push_back(wy);
        mesh.verts.push_back(wz);
    }
    // 重心顶点
    {
        float wx, wy, wz;
        detail::TransformVertex(pose, cx, cy, cz, wx, wy, wz);
        mesh.verts.push_back(wx);
        mesh.verts.push_back(wy);
        mesh.verts.push_back(wz);
    }
    unsigned centerIdx = base + (unsigned)shape.convexVerts.size();

    for (auto& edge : shape.convexEdges)
    {
        mesh.indices.push_back(base + edge.first);
        mesh.indices.push_back(base + edge.second);
        mesh.indices.push_back(centerIdx);
    }
}

// 将 CollisionScene 的所有 actor 三角形化到单个 TriMesh
inline TriMesh TessellateScene(const CollisionScene& scene, bool staticOnly = true)
{
    TriMesh mesh;

    auto processActors = [&](const std::vector<CollisionActor>& actors)
    {
        for (auto& actor : actors)
        {
            for (auto& shape : actor.shapes)
            {
                switch (shape.geomType)
                {
                case eGEOM_BOX:
                    TessellateBox(shape.worldPose, shape.box, mesh);
                    break;
                case eGEOM_SPHERE:
                    TessellateSphere(shape.worldPose, shape.sphere.radius, mesh);
                    break;
                case eGEOM_CAPSULE:
                    TessellateCapsule(shape.worldPose, shape.capsule, mesh);
                    break;
                case eGEOM_CONVEX:
                    TessellateConvex(shape.worldPose, shape, mesh);
                    break;
                default:
                    break;
                }
            }
        }
    };

    processActors(scene.staticActors);
    if (!staticOnly)
    {
        processActors(scene.dynamicActors);
    }
    return mesh;
}

} // namespace xrd
