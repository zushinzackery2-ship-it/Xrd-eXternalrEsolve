#pragma once
// Xrd-eXternalrEsolve - UE5 Chaos 物理数据类型与偏移定义
// 参考 UE5-Chaos.md

#include "../core/types.hpp"
#include "../collision/collision_types.hpp"
#include <vector>

namespace xrd
{

// ─── Chaos 偏移表 ───
// [SDK] 标记的偏移由 AutoInit 反射发现
// [引擎] 标记的偏移为引擎内部稳定布局
// [扫描] 标记的偏移需运行时扫描验证

struct ChaosOffsets
{
    // [SDK] UPrimitiveComponent::BodyInstance（反射发现）
    i32 PrimComp_BodyInstance = -1;

    // [引擎] FBodyInstance +0x08 -> TWeakObjectPtr<UBodySetup>
    i32 BodyInstance_BodySetup = 0x08;

    // [扫描] FBodyInstance 尾部私有句柄 -> FSingleParticlePhysicsProxy*
    i32 BodyInstance_PhysicsProxy = -1;

    // [SDK] UBodySetup::AggGeom（反射发现）
    i32 BodySetup_AggGeom = -1;

    // [SDK] FKAggregateGeom 成员（反射发现，引擎布局可作 fallback）
    i32 AggGeom_SphereElems  = 0x00;
    i32 AggGeom_BoxElems     = 0x10;
    i32 AggGeom_SphylElems   = 0x20;
    i32 AggGeom_ConvexElems  = 0x30;

    // [引擎] FKShapeElem 基类大小
    i32 ShapeElem_BaseSize = 0x30;

    // [SDK/引擎] FKBoxElem 布局（基于 FKShapeElem 之后）
    i32 BoxElem_Center   = 0x30;  // FVector (3x double)
    i32 BoxElem_Rotation = 0x48;  // FRotator (3x double)
    i32 BoxElem_X        = 0x60;  // float
    i32 BoxElem_Y        = 0x64;  // float
    i32 BoxElem_Z        = 0x68;  // float
    i32 BoxElem_Size     = 0x70;  // sizeof(FKBoxElem) 估算

    // [SDK/引擎] FKSphereElem 布局
    i32 SphereElem_Center = 0x30; // FVector (3x double)
    i32 SphereElem_Radius = 0x48; // float
    i32 SphereElem_Size   = 0x50; // sizeof(FKSphereElem) 估算

    // [SDK/引擎] FKSphylElem 布局（Capsule）
    i32 SphylElem_Center   = 0x30; // FVector (3x double)
    i32 SphylElem_Rotation = 0x48; // FRotator (3x double)
    i32 SphylElem_Radius   = 0x60; // float
    i32 SphylElem_Length   = 0x64; // float
    i32 SphylElem_Size     = 0x70; // sizeof(FKSphylElem) 估算

    // [SDK/引擎] FKConvexElem 布局
    i32 ConvexElem_VertexData = 0x30; // TArray<FVector>
    i32 ConvexElem_IndexData  = 0x40; // TArray<int32>
    i32 ConvexElem_Size       = 0x80; // sizeof(FKConvexElem) 估算

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

    // [引擎] FBodyInstance 反射结构大小（从 UStruct::Size 获取）
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
