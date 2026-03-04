#pragma once
// Xrd-eXternalrEsolve - PhysX 数据类型与偏移定义
// 支持 PhysX 3.4（UE4 Fortnite 分支）和 Unity 版本

#include "../../core/types.hpp"
#include <vector>
#include <set>
#include <cmath>

namespace xrd
{

// ─── PhysX 几何类型枚举 ───

enum PxGeometryType : u32
{
    eSPHERE       = 0,
    ePLANE        = 1,
    eCAPSULE      = 2,
    eBOX          = 3,
    eCONVEXMESH   = 4,
    eTRIANGLEMESH = 5,
    eHEIGHTFIELD  = 6,
    eGEOM_INVALID = 0xFFFFFFFF
};

// ─── PxShapeFlags 位定义 ───

constexpr u32 eSIMULATION_SHAPE  = (1 << 0);
constexpr u32 eSCENE_QUERY_SHAPE = (1 << 1);
constexpr u32 eTRIGGER_SHAPE     = (1 << 2);

// ─── 基础数学类型 ───

struct PxVec3
{
    float x = 0, y = 0, z = 0;
};

struct PxQuat
{
    float x = 0, y = 0, z = 0, w = 1;
};

struct PxTransform
{
    PxQuat q;
    PxVec3 p;
};

// ─── 四元数 / 变换运算 ───

inline PxQuat QuatMultiply(const PxQuat& a, const PxQuat& b)
{
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    };
}

inline PxQuat QuatConjugate(const PxQuat& q)
{
    return { -q.x, -q.y, -q.z, q.w };
}

inline PxVec3 QuatRotate(const PxQuat& q, const PxVec3& v)
{
    PxQuat p = { v.x, v.y, v.z, 0 };
    PxQuat r = QuatMultiply(QuatMultiply(q, p), QuatConjugate(q));
    return { r.x, r.y, r.z };
}

inline PxTransform TransformMultiply(const PxTransform& a, const PxTransform& b)
{
    PxVec3 rotated = QuatRotate(a.q, b.p);
    return {
        QuatMultiply(a.q, b.q),
        { a.p.x + rotated.x, a.p.y + rotated.y, a.p.z + rotated.z }
    };
}

inline PxVec3 TransformPoint(const PxTransform& t, const PxVec3& v)
{
    PxVec3 r = QuatRotate(t.q, v);
    return { t.p.x + r.x, t.p.y + r.y, t.p.z + r.z };
}

// ─── PhysX 内存偏移表（可切换 UE4/Unity） ───

struct PhysXOffsets
{
    // NpPhysics
    u32 physics_scenes      = 0x08;
    u32 physics_scenesCount = 0x10;

    // NpScene actors（UE4 默认值，Unity 不同）
    u32 scene_actors      = 0x2618;
    u32 scene_actorsCount = 0x2620;

    // NpRigidActor
    u32 actor_type       = 0x08;
    u32 actor_userData   = 0x10;
    u32 actor_shapesData = 0x28;
    u32 actor_shapesCount = 0x30;
    u32 staticType  = 7;   // UE4=7, Unity=5
    u32 dynamicType = 6;

    // NpRigidStatic pose
    u32 staticPose_flag       = 0x68;
    u32 staticPose_flagBit    = 0x40;
    u32 staticPose_extPtr     = 0x70;
    u32 staticPose_extOffset  = 0xB0;
    u32 staticPose_inline     = 0x90;

    // NpRigidDynamic pose
    u32 dynPose_flagAddr      = 0x17C;
    u32 dynPose_flagBit       = 0x200;
    u32 dynPose_extPtr        = 0x70;
    u32 dynPose_extOffset     = 0xE0;
    u32 dynPose_b2wInline     = 0xB0;
    u32 dynPose_body2actor    = 0x140;

    // NpShape
    u32 shape_flagByte   = 0x38;
    u32 shape_extPtr     = 0x40;
    u32 shape_localPose  = 0x70;
    u32 shape_geomType   = 0x98;
    u32 shape_geomData   = 0x9C;

    // PxShapeFlags 位
    u32 shapeFlagsInline   = 0x90;
    u32 shapeFlags_extOff  = 0x34;
    u32 shapeFlags_flagBit = 0x40;
};

// ─── Shape 几何数据 ───

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

union GeomData
{
    BoxGeom     box;
    SphereGeom  sphere;
    CapsuleGeom capsule;
};

// ─── 解析后的 shape 数据 ───

struct PxShapeData
{
    PxGeometryType geomType = eGEOM_INVALID;
    PxTransform localPose;
    GeomData geom{};
    u32 pxFlags = 0;

    // ConvexMesh 数据（仅 eCONVEXMESH 有效）
    std::vector<PxVec3> convexVerts;
    std::vector<std::pair<u8, u8>> convexEdges;
};

// ─── 解析后的 actor 数据 ───

enum PxActorType : u32
{
    eRIGID_STATIC  = 7,
    eRIGID_DYNAMIC = 6
};

struct PxActorData
{
    uptr address = 0;
    PxActorType type = eRIGID_STATIC;
    PxTransform globalPose;
    std::vector<PxShapeData> shapes;
};

struct PxSceneData
{
    std::vector<PxActorData> staticActors;
    std::vector<PxActorData> dynamicActors;
};

} // namespace xrd
