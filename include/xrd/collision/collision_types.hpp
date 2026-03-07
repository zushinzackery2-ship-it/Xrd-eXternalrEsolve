#pragma once
// Xrd-eXternalrEsolve - 引擎无关碰撞数据类型
// PhysX / Chaos 共用的几何与变换类型，Embree 建模仅依赖此文件

#include "../core/types.hpp"
#include <vector>
#include <cmath>

namespace xrd
{

// ─── 几何类型枚举（引擎无关） ───

enum CollisionGeomType : u32
{
    eGEOM_SPHERE       = 0,
    eGEOM_CAPSULE      = 1,
    eGEOM_BOX          = 2,
    eGEOM_CONVEX       = 3,
    eGEOM_INVALID_TYPE = 0xFFFFFFFF
};

// ─── 基础数学类型 ───

struct Vec3
{
    float x = 0, y = 0, z = 0;
};

struct Quat
{
    float x = 0, y = 0, z = 0, w = 1;
};

struct Transform
{
    Quat q;
    Vec3 p;
};

// ─── 四元数 / 变换运算 ───

inline Quat QuatMul(const Quat& a, const Quat& b)
{
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    };
}

inline Quat QuatConj(const Quat& q)
{
    return { -q.x, -q.y, -q.z, q.w };
}

inline Vec3 QuatRot(const Quat& q, const Vec3& v)
{
    Quat p = { v.x, v.y, v.z, 0 };
    Quat r = QuatMul(QuatMul(q, p), QuatConj(q));
    return { r.x, r.y, r.z };
}

inline Transform TransformMul(const Transform& a, const Transform& b)
{
    Vec3 rotated = QuatRot(a.q, b.p);
    return {
        QuatMul(a.q, b.q),
        { a.p.x + rotated.x, a.p.y + rotated.y, a.p.z + rotated.z }
    };
}

inline Vec3 TransformPt(const Transform& t, const Vec3& v)
{
    Vec3 r = QuatRot(t.q, v);
    return { t.p.x + r.x, t.p.y + r.y, t.p.z + r.z };
}

// ─── 几何数据 ───

struct BoxGeom
{
    float halfX = 0, halfY = 0, halfZ = 0;
};

struct SphereGeom
{
    float radius = 0;
};

struct CapsuleGeom
{
    float radius = 0, halfHeight = 0;
};

// ─── 解析后的 shape 数据（引擎无关） ───

struct CollisionShape
{
    CollisionGeomType geomType = eGEOM_INVALID_TYPE;
    Transform worldPose;   // 已合并 globalPose × localPose 的最终世界变换

    // 几何参数（按 geomType 选用）
    BoxGeom     box{};
    SphereGeom  sphere{};
    CapsuleGeom capsule{};

    // ConvexMesh 数据（仅 eGEOM_CONVEX 有效）
    std::vector<Vec3> convexVerts;
    std::vector<std::pair<u8, u8>> convexEdges;
};

// ─── 解析后的 actor 数据（引擎无关） ───

struct CollisionActor
{
    uptr address = 0;
    bool isDynamic = false;
    std::vector<CollisionShape> shapes;
};

// ─── 碰撞场景（引擎无关，PhysXReader / ChaosReader 均输出此类型） ───

struct CollisionScene
{
    std::vector<CollisionActor> staticActors;
    std::vector<CollisionActor> dynamicActors;
};

} // namespace xrd
