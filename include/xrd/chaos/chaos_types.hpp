#pragma once
// Xrd-eXternalrEsolve - UE5 Chaos 物理数据类型与偏移定义
// 参考 UE5-Chaos.md

#include "../core/types.hpp"
#include "../collision/collision_types.hpp"
#include <vector>

namespace xrd
{

// ─── Chaos 偏移表 ───
// [反射] 优先从 UClass / UScriptStruct 发现，失败时退回稳定布局
// [引擎] 依赖私有物理实现的稳定布局
// [扫描] 需要运行时扫描验证

struct ChaosOffsets
{
    // [反射] UPrimitiveComponent::BodyInstance
    i32 PrimComp_BodyInstance = -1;

    // [反射] FBodyInstance::BodySetup，失败时回退稳定布局
    i32 BodyInstance_BodySetup = -1;

    // [扫描] FBodyInstance 尾部私有句柄 -> FSingleParticlePhysicsProxy*
    i32 BodyInstance_PhysicsProxy = -1;

    // [反射] UBodySetup::AggGeom
    i32 BodySetup_AggGeom = -1;

    // [反射] FKAggregateGeom 成员，失败时退回稳定布局
    i32 AggGeom_SphereElems  = -1;
    i32 AggGeom_BoxElems     = -1;
    i32 AggGeom_SphylElems   = -1;
    i32 AggGeom_ConvexElems  = -1;

    // [反射] FKShapeElem 基类大小，失败时退回稳定布局
    i32 ShapeElem_BaseSize = -1;

    // [反射] FKBoxElem 布局，失败时退回稳定布局
    i32 BoxElem_Center   = -1;
    i32 BoxElem_Rotation = -1;
    i32 BoxElem_X        = -1;
    i32 BoxElem_Y        = -1;
    i32 BoxElem_Z        = -1;
    i32 BoxElem_Size     = -1;

    // [反射] FKSphereElem 布局，失败时退回稳定布局
    i32 SphereElem_Center = -1;
    i32 SphereElem_Radius = -1;
    i32 SphereElem_Size   = -1;

    // [反射] FKSphylElem 布局（Capsule），失败时退回稳定布局
    i32 SphylElem_Center   = -1;
    i32 SphylElem_Rotation = -1;
    i32 SphylElem_Radius   = -1;
    i32 SphylElem_Length   = -1;
    i32 SphylElem_Size     = -1;

    // [反射] FKConvexElem 布局，失败时退回稳定布局
    i32 ConvexElem_VertexData = -1;
    i32 ConvexElem_IndexData  = -1;
    i32 ConvexElem_Size       = -1;

    // [引擎] FSingleParticlePhysicsProxy 布局
    i32 Proxy_Owner    = 0x08; // UObject* 回指
    i32 Proxy_Particle = 0x20; // TGeometryParticle*

    // [引擎] TGeometryParticle 布局（世界坐标，直接读）
    i32 Particle_PosX = 0x08; // double
    i32 Particle_PosY = 0x10; // double
    i32 Particle_PosZ = 0x18; // double
    i32 Particle_RotX = 0x30; // double（Python诊断确认：+0x20是索引字段，+0x30才是四元数）
    i32 Particle_RotY = 0x38; // double
    i32 Particle_RotZ = 0x40; // double
    i32 Particle_RotW = 0x48; // double

    // [扫描] UWorld -> FPhysScene_Chaos*（非反射成员，需回指扫描）
    i32 UWorld_PhysScene = -1;

    // [引擎] FPhysScene_Chaos 布局
    i32 PhysScene_OwnerWorld = 0x10; // UWorld* 回指
    i32 PhysScene_Solver     = 0x18; // FPBDRigidsSolver*

    // [引擎] FPBDRigidsSolver 布局
    i32 Solver_PhysScene = 0x10;     // FPhysScene_Chaos* 回指

    // [反射] FBodyInstance 结构大小（从 UStruct::Size 获取）
    i32 BodyInstance_StructSize = -1;
};

// ─── 从 TGeometryParticle 读到的世界变换 ───

struct ChaosWorldPose
{
    double posX = 0, posY = 0, posZ = 0;
    double rotX = 0, rotY = 0, rotZ = 0, rotW = 1;

    // 转换为引擎无关的 float Transform
    Transform ToTransform() const
    {
        Transform t;
        t.p = { (float)posX, (float)posY, (float)posZ };
        t.q = { (float)rotX, (float)rotY, (float)rotZ, (float)rotW };
        return t;
    }
};

} // namespace xrd
