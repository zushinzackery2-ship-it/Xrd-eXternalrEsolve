#pragma once
// Xrd-eXternalrEsolve - PhysX 数据类型与偏移定义
// 支持 PhysX 3.4（UE4 Fortnite 分支）和 Unity 版本
// 数学/几何类型已迁移到 collision_types.hpp，此处保留别名以向后兼容

#include "../collision/collision_types.hpp"
#include <vector>
#include <set>

namespace xrd
{

// ─── 向后兼容别名（旧代码可继续使用 PxVec3 / PxTransform 等） ───

using PxVec3      = Vec3;
using PxQuat      = Quat;
using PxTransform = Transform;

inline PxQuat QuatMultiply(const PxQuat& a, const PxQuat& b) { return QuatMul(a, b); }
inline PxQuat QuatConjugate(const PxQuat& q) { return QuatConj(q); }
inline PxVec3 QuatRotate(const PxQuat& q, const PxVec3& v) { return QuatRot(q, v); }
inline PxTransform TransformMultiply(const PxTransform& a, const PxTransform& b) { return TransformMul(a, b); }
inline PxVec3 TransformPoint(const PxTransform& t, const PxVec3& v) { return TransformPt(t, v); }

// ─── PhysX 几何类型枚举（PhysX 内部编号，与引擎无关枚举不同） ───

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

// ─── GeomData union（PhysX 读取专用） ───

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

// ─── PhysX → CollisionScene 转换 ───

inline CollisionScene ToCollisionScene(const PxSceneData& pxData)
{
    CollisionScene scene;

    auto convertActors = [](const std::vector<PxActorData>& src, std::vector<CollisionActor>& dst, bool isDynamic)
    {
        dst.reserve(src.size());
        for (auto& pa : src)
        {
            CollisionActor ca;
            ca.address = pa.address;
            ca.isDynamic = isDynamic;
            ca.shapes.reserve(pa.shapes.size());
            for (auto& ps : pa.shapes)
            {
                CollisionShape cs;
                // 合并 globalPose × localPose
                cs.worldPose = TransformMul(pa.globalPose, ps.localPose);
                switch (ps.geomType)
                {
                case eBOX:
                    cs.geomType = eGEOM_BOX;
                    cs.box = ps.geom.box;
                    break;
                case eSPHERE:
                    cs.geomType = eGEOM_SPHERE;
                    cs.sphere = ps.geom.sphere;
                    break;
                case eCAPSULE:
                    cs.geomType = eGEOM_CAPSULE;
                    cs.capsule = ps.geom.capsule;
                    break;
                case eCONVEXMESH:
                    cs.geomType = eGEOM_CONVEX;
                    cs.convexVerts.reserve(ps.convexVerts.size());
                    for (auto& v : ps.convexVerts)
                    {
                        cs.convexVerts.push_back({v.x, v.y, v.z});
                    }
                    cs.convexEdges = ps.convexEdges;
                    break;
                default:
                    continue;
                }
                ca.shapes.push_back(std::move(cs));
            }
            if (!ca.shapes.empty())
            {
                dst.push_back(std::move(ca));
            }
        }
    };

    convertActors(pxData.staticActors, scene.staticActors, false);
    convertActors(pxData.dynamicActors, scene.dynamicActors, true);
    return scene;
}

} // namespace xrd
