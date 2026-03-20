#pragma once
// Xrd-eXternalrEsolve - UE5 Chaos 碰撞数据读取器
// 从远程进程读取 Chaos 碰撞几何并输出到 CollisionScene

#include "chaos_types.hpp"
#include "../memory/memory.hpp"
#include "../core/context.hpp"
#include <set>
#include <unordered_set>

namespace xrd
{

class ChaosReader
{
public:
    ChaosReader(const IMemoryAccessor& mem, const UEOffsets& ueOff, const ChaosOffsets& chaosOff, uptr moduleBase = 0)
        : m_mem(mem), m_ueOff(ueOff), m_off(chaosOff), m_moduleBase(moduleBase)
    {
    }

    ChaosOffsets& Offsets() { return m_off; }
    const ChaosOffsets& Offsets() const { return m_off; }

    // 从 UWorld 的已加载 Level 集合遍历所有 Actor，读取碰撞几何
    bool ReadStaticCollision(uptr worldPtr, CollisionScene& outScene) const
    {
        outScene.staticActors.clear();
        outScene.dynamicActors.clear();

        if (!IsCanonicalUserPtr(worldPtr))
        {
            return false;
        }

        std::vector<uptr> levelPtrs;
        if (!ReadLoadedLevels(worldPtr, levelPtrs))
        {
            std::cerr << "[xrd][Chaos] 无法枚举已加载 Levels\n";
            return false;
        }

        std::unordered_set<uptr> seenActors;
        seenActors.reserve(8192);

        for (uptr levelPtr : levelPtrs)
        {
            std::vector<uptr> actorPtrs;
            if (!ReadLevelActors(levelPtr, actorPtrs))
            {
                continue;
            }

            for (uptr actorPtr : actorPtrs)
            {
                if (!IsCanonicalUserPtr(actorPtr))
                {
                    continue;
                }
                if (!seenActors.insert(actorPtr).second)
                {
                    continue;
                }
                ProcessActor(actorPtr, outScene);
            }
        }

        // 统计并输出实时诊断
        int totalShapes = 0;
        for (auto& a : outScene.staticActors)
        {
            totalShapes += (int)a.shapes.size();
        }
        if (!outScene.staticActors.empty())
        {
            auto& firstPos = outScene.staticActors[0].shapes[0].worldPose.p;
            std::cerr << "[Chaos] ReadStaticCollision: actors="
                      << outScene.staticActors.size()
                      << " shapes=" << totalShapes
                      << " first=(" << firstPos.x << "," << firstPos.y << "," << firstPos.z << ")\n";
        }
        else
        {
            std::cerr << "[Chaos] ReadStaticCollision: actors=0 shapes=0\n";
        }

        return true;
    }

private:
    const IMemoryAccessor& m_mem;
    const UEOffsets& m_ueOff;
    ChaosOffsets m_off;
    uptr m_moduleBase = 0;

    void AppendUniqueLevel(
        uptr levelPtr,
        std::unordered_set<uptr>& seenLevels,
        std::vector<uptr>& outLevels) const
    {
        if (!IsCanonicalUserPtr(levelPtr))
        {
            return;
        }

        if (seenLevels.insert(levelPtr).second)
        {
            outLevels.push_back(levelPtr);
        }
    }

    bool ReadLoadedLevels(uptr worldPtr, std::vector<uptr>& outLevels) const
    {
        outLevels.clear();
        if (!IsCanonicalUserPtr(worldPtr))
        {
            return false;
        }

        std::unordered_set<uptr> seenLevels;
        seenLevels.reserve(16);

        if (m_ueOff.UWorld_PersistentLevel != -1)
        {
            uptr persistentLevel = 0;
            if (ReadPtr(m_mem, worldPtr + m_ueOff.UWorld_PersistentLevel, persistentLevel))
            {
                AppendUniqueLevel(persistentLevel, seenLevels, outLevels);
            }
        }

        if (m_ueOff.UWorld_Levels != -1)
        {
            uptr levelsData = 0;
            i32 levelsCount = 0;
            if (ReadPtr(m_mem, worldPtr + m_ueOff.UWorld_Levels, levelsData) &&
                ReadValue(m_mem, worldPtr + m_ueOff.UWorld_Levels + 8, levelsCount) &&
                IsCanonicalUserPtr(levelsData) &&
                levelsCount > 0 &&
                levelsCount <= 4096)
            {
                std::vector<uptr> levelPtrs(static_cast<std::size_t>(levelsCount));
                if (m_mem.Read(levelsData, levelPtrs.data(), levelPtrs.size() * sizeof(uptr)))
                {
                    for (uptr levelPtr : levelPtrs)
                    {
                        AppendUniqueLevel(levelPtr, seenLevels, outLevels);
                    }
                }
            }
        }

        return !outLevels.empty();
    }

    bool ReadLevelActors(uptr levelPtr, std::vector<uptr>& outActors) const
    {
        outActors.clear();
        if (!IsCanonicalUserPtr(levelPtr) || m_ueOff.ULevel_Actors < 0)
        {
            return false;
        }

        uptr actorsData = 0;
        i32 actorsCount = 0;
        if (!ReadPtr(m_mem, levelPtr + m_ueOff.ULevel_Actors, actorsData) ||
            !ReadValue(m_mem, levelPtr + m_ueOff.ULevel_Actors + 8, actorsCount))
        {
            return false;
        }

        if (!IsCanonicalUserPtr(actorsData) || actorsCount <= 0 || actorsCount > 100000)
        {
            return false;
        }

        std::vector<uptr> rawActorPtrs(static_cast<std::size_t>(actorsCount));
        if (!m_mem.Read(actorsData, rawActorPtrs.data(), rawActorPtrs.size() * sizeof(uptr)))
        {
            return false;
        }

        outActors.reserve(rawActorPtrs.size());
        for (uptr actorPtr : rawActorPtrs)
        {
            if (IsCanonicalUserPtr(actorPtr))
            {
                outActors.push_back(actorPtr);
            }
        }

        return !outActors.empty();
    }

    // ─── Actor 处理 ───

    void ProcessActor(uptr actorPtr, CollisionScene& outScene) const
    {
        uptr rootComp = 0;

        uptr actorClass = GetObjectClass(actorPtr);
        if (!actorClass)
        {
            return;
        }

        i32 rootCompOff = GetPropertyOffsetByName(actorClass, "RootComponent");
        if (rootCompOff < 0)
        {
            return;
        }

        if (!ReadPtr(m_mem, actorPtr + rootCompOff, rootComp) || !IsCanonicalUserPtr(rootComp))
        {
            return;
        }

        if (m_off.PrimComp_BodyInstance < 0)
        {
            return;
        }

        uptr bodyInstanceAddr = rootComp + m_off.PrimComp_BodyInstance;
        CollisionActor actor;
        if (ReadBodyInstanceCollision(bodyInstanceAddr, actor) && !actor.shapes.empty())
        {
            outScene.staticActors.push_back(std::move(actor));
        }
    }

    // ─── 从 BodyInstance 读取碰撞几何 ───

    bool ReadBodyInstanceCollision(uptr bodyInstanceAddr, CollisionActor& outActor) const
    {
        if (m_off.PrimComp_BodyInstance < 0)
        {
            return false;
        }

        // FBodyInstance -> TWeakObjectPtr<UBodySetup>
        if (m_off.BodyInstance_BodySetup < 0)
        {
            return false;
        }

        i32 weakIdx = 0;
        if (!ReadValue(m_mem, bodyInstanceAddr + m_off.BodyInstance_BodySetup, weakIdx))
        {
            return false;
        }

        // 解析 TWeakObjectPtr：通过 GObjects 查找
        uptr bodySetupPtr = ResolveWeakObjectPtr(weakIdx);
        if (!IsCanonicalUserPtr(bodySetupPtr))
        {
            return false;
        }

        // UBodySetup -> AggGeom
        if (m_off.BodySetup_AggGeom < 0)
        {
            return false;
        }
        uptr aggGeomAddr = bodySetupPtr + m_off.BodySetup_AggGeom;

        // 读取世界变换（proxy 无效时跳过整个 actor，不用 identity 兜底）
        Transform worldPose;
        if (!ReadWorldPoseFromBodyInstance(bodyInstanceAddr, worldPose))
        {
            return false;
        }

        // 读取各类碰撞形状
        ReadBoxElems(aggGeomAddr, worldPose, outActor);
        ReadSphereElems(aggGeomAddr, worldPose, outActor);
        ReadSphylElems(aggGeomAddr, worldPose, outActor);
        ReadConvexElems(aggGeomAddr, worldPose, outActor);

        return !outActor.shapes.empty();
    }

    // ─── TWeakObjectPtr 解析 ───

    uptr ResolveWeakObjectPtr(i32 objectIndex) const
    {
        if (objectIndex <= 0 || m_ueOff.GObjects == 0)
        {
            return 0;
        }

        // GObjects chunked array: chunks[idx >> 16] + (idx & 0xFFFF) * FUObjectItemSize
        i32 chunkIdx = objectIndex >> 16;
        i32 withinIdx = objectIndex & 0xFFFF;

        // 读取 chunk 指针
        uptr chunksBase = 0;
        if (!ReadPtr(m_mem, m_ueOff.GObjects, chunksBase) || !IsCanonicalUserPtr(chunksBase))
        {
            return 0;
        }

        uptr chunkPtr = 0;
        if (!ReadPtr(m_mem, chunksBase + chunkIdx * sizeof(uptr), chunkPtr) ||
            !IsCanonicalUserPtr(chunkPtr))
        {
            return 0;
        }

        // 读取 FUObjectItem 中的 Object 指针
        uptr objPtr = 0;
        uptr itemAddr = chunkPtr + withinIdx * m_ueOff.FUObjectItemSize;
        if (!ReadPtr(m_mem, itemAddr, objPtr))
        {
            return 0;
        }

        return objPtr;
    }

    // ─── 世界变换读取（通过 FBodyInstance 尾部私有句柄） ───

    bool ReadWorldPoseFromBodyInstance(uptr bodyInstanceAddr, Transform& outPose) const
    {
        outPose.q = { 0, 0, 0, 1 };
        outPose.p = { 0, 0, 0 };

        if (m_off.BodyInstance_PhysicsProxy < 0)
        {
            return false;
        }

        // FBodyInstance +?? -> FSingleParticlePhysicsProxy*
        uptr proxyPtr = 0;
        if (!ReadPtr(m_mem, bodyInstanceAddr + m_off.BodyInstance_PhysicsProxy, proxyPtr) ||
            !IsCanonicalUserPtr(proxyPtr))
        {
            return false;
        }

        // 验证 proxy vtable 指向游戏模块内（匹配 Python 参考的 is_mod 校验）
        uptr proxyVtable = 0;
        if (!ReadPtr(m_mem, proxyPtr, proxyVtable))
        {
            return false;
        }
        if (m_moduleBase == 0 ||
            proxyVtable < m_moduleBase ||
            proxyVtable > m_moduleBase + 0x8000000)
        {
            return false;
        }

        // FSingleParticlePhysicsProxy +0x20 -> TGeometryParticle*
        uptr particlePtr = 0;
        if (!ReadPtr(m_mem, proxyPtr + m_off.Proxy_Particle, particlePtr) ||
            !IsCanonicalUserPtr(particlePtr))
        {
            return false;
        }

        // 读取双精度 Position + Rotation
        ChaosWorldPose pose;
        ReadValue(m_mem, particlePtr + m_off.Particle_PosX, pose.posX);
        ReadValue(m_mem, particlePtr + m_off.Particle_PosY, pose.posY);
        ReadValue(m_mem, particlePtr + m_off.Particle_PosZ, pose.posZ);
        ReadValue(m_mem, particlePtr + m_off.Particle_RotX, pose.rotX);
        ReadValue(m_mem, particlePtr + m_off.Particle_RotY, pose.rotY);
        ReadValue(m_mem, particlePtr + m_off.Particle_RotZ, pose.rotZ);
        ReadValue(m_mem, particlePtr + m_off.Particle_RotW, pose.rotW);

        // 位置范围校验（匹配 Python 参考：-1e7 < v < 1e7）
        if (pose.posX < -1e7 || pose.posX > 1e7 ||
            pose.posY < -1e7 || pose.posY > 1e7 ||
            pose.posZ < -1e7 || pose.posZ > 1e7)
        {
            return false;
        }

        // 四元数长度校验（匹配 Python 参考：0.95 < len < 1.05）
        double qlen = std::sqrt(
            pose.rotX * pose.rotX + pose.rotY * pose.rotY +
            pose.rotZ * pose.rotZ + pose.rotW * pose.rotW);
        if (qlen < 0.95 || qlen > 1.05)
        {
            // 不合法就用 identity 旋转，但位置仍然有效
            pose.rotX = 0; pose.rotY = 0; pose.rotZ = 0; pose.rotW = 1;
        }

        outPose = pose.ToTransform();
        return true;
    }

    // ─── 碰撞形状读取 ───

    void ReadBoxElems(uptr aggGeomAddr, const Transform& worldPose, CollisionActor& outActor) const
    {
        uptr data = 0;
        i32 count = 0;
        if (!ReadPtr(m_mem, aggGeomAddr + m_off.AggGeom_BoxElems, data) ||
            !ReadValue(m_mem, aggGeomAddr + m_off.AggGeom_BoxElems + 8, count))
        {
            return;
        }
        if (!IsCanonicalUserPtr(data) || count <= 0 || count > 1024)
        {
            return;
        }

        for (i32 i = 0; i < count; ++i)
        {
            uptr elemAddr = data + i * m_off.BoxElem_Size;

            // 读取双精度 Center
            double cx = 0, cy = 0, cz = 0;
            ReadValue(m_mem, elemAddr + m_off.BoxElem_Center + 0, cx);
            ReadValue(m_mem, elemAddr + m_off.BoxElem_Center + 8, cy);
            ReadValue(m_mem, elemAddr + m_off.BoxElem_Center + 16, cz);

            // 读取双精度 Rotation（欧拉角 → 四元数）
            double rPitch = 0, rYaw = 0, rRoll = 0;
            ReadValue(m_mem, elemAddr + m_off.BoxElem_Rotation + 0, rPitch);
            ReadValue(m_mem, elemAddr + m_off.BoxElem_Rotation + 8, rYaw);
            ReadValue(m_mem, elemAddr + m_off.BoxElem_Rotation + 16, rRoll);

            // 读取 half-extents
            float halfX = 0, halfY = 0, halfZ = 0;
            ReadValue(m_mem, elemAddr + m_off.BoxElem_X, halfX);
            ReadValue(m_mem, elemAddr + m_off.BoxElem_Y, halfY);
            ReadValue(m_mem, elemAddr + m_off.BoxElem_Z, halfZ);

            // 半尺寸是全尺寸的一半（UE 的 X/Y/Z 存的是全尺寸）
            halfX *= 0.5f;
            halfY *= 0.5f;
            halfZ *= 0.5f;

            CollisionShape shape;
            shape.geomType = eGEOM_BOX;
            shape.box = { halfX, halfY, halfZ };

            // 组合 worldPose × localPose(center + rotation)
            Transform localPose;
            localPose.p = { (float)cx, (float)cy, (float)cz };
            localPose.q = EulerToQuat(rPitch, rYaw, rRoll);
            shape.worldPose = TransformMul(worldPose, localPose);

            outActor.shapes.push_back(std::move(shape));
        }
    }

    void ReadSphereElems(uptr aggGeomAddr, const Transform& worldPose, CollisionActor& outActor) const
    {
        uptr data = 0;
        i32 count = 0;
        if (!ReadPtr(m_mem, aggGeomAddr + m_off.AggGeom_SphereElems, data) ||
            !ReadValue(m_mem, aggGeomAddr + m_off.AggGeom_SphereElems + 8, count))
        {
            return;
        }
        if (!IsCanonicalUserPtr(data) || count <= 0 || count > 1024)
        {
            return;
        }

        for (i32 i = 0; i < count; ++i)
        {
            uptr elemAddr = data + i * m_off.SphereElem_Size;

            double cx = 0, cy = 0, cz = 0;
            ReadValue(m_mem, elemAddr + m_off.SphereElem_Center + 0, cx);
            ReadValue(m_mem, elemAddr + m_off.SphereElem_Center + 8, cy);
            ReadValue(m_mem, elemAddr + m_off.SphereElem_Center + 16, cz);

            float radius = 0;
            ReadValue(m_mem, elemAddr + m_off.SphereElem_Radius, radius);

            CollisionShape shape;
            shape.geomType = eGEOM_SPHERE;
            shape.sphere = { radius };

            Transform localPose;
            localPose.p = { (float)cx, (float)cy, (float)cz };
            localPose.q = { 0, 0, 0, 1 };
            shape.worldPose = TransformMul(worldPose, localPose);

            outActor.shapes.push_back(std::move(shape));
        }
    }

    void ReadSphylElems(uptr aggGeomAddr, const Transform& worldPose, CollisionActor& outActor) const
    {
        uptr data = 0;
        i32 count = 0;
        if (!ReadPtr(m_mem, aggGeomAddr + m_off.AggGeom_SphylElems, data) ||
            !ReadValue(m_mem, aggGeomAddr + m_off.AggGeom_SphylElems + 8, count))
        {
            return;
        }
        if (!IsCanonicalUserPtr(data) || count <= 0 || count > 1024)
        {
            return;
        }

        for (i32 i = 0; i < count; ++i)
        {
            uptr elemAddr = data + i * m_off.SphylElem_Size;

            double cx = 0, cy = 0, cz = 0;
            ReadValue(m_mem, elemAddr + m_off.SphylElem_Center + 0, cx);
            ReadValue(m_mem, elemAddr + m_off.SphylElem_Center + 8, cy);
            ReadValue(m_mem, elemAddr + m_off.SphylElem_Center + 16, cz);

            double rPitch = 0, rYaw = 0, rRoll = 0;
            ReadValue(m_mem, elemAddr + m_off.SphylElem_Rotation + 0, rPitch);
            ReadValue(m_mem, elemAddr + m_off.SphylElem_Rotation + 8, rYaw);
            ReadValue(m_mem, elemAddr + m_off.SphylElem_Rotation + 16, rRoll);

            float radius = 0, length = 0;
            ReadValue(m_mem, elemAddr + m_off.SphylElem_Radius, radius);
            ReadValue(m_mem, elemAddr + m_off.SphylElem_Length, length);

            CollisionShape shape;
            shape.geomType = eGEOM_CAPSULE;
            shape.capsule = { radius, length * 0.5f };

            Transform localPose;
            localPose.p = { (float)cx, (float)cy, (float)cz };
            localPose.q = EulerToQuat(rPitch, rYaw, rRoll);
            shape.worldPose = TransformMul(worldPose, localPose);

            outActor.shapes.push_back(std::move(shape));
        }
    }

    void ReadConvexElems(uptr aggGeomAddr, const Transform& worldPose, CollisionActor& outActor) const
    {
        uptr data = 0;
        i32 count = 0;
        if (!ReadPtr(m_mem, aggGeomAddr + m_off.AggGeom_ConvexElems, data) ||
            !ReadValue(m_mem, aggGeomAddr + m_off.AggGeom_ConvexElems + 8, count))
        {
            return;
        }
        if (!IsCanonicalUserPtr(data) || count <= 0 || count > 256)
        {
            return;
        }

        for (i32 i = 0; i < count; ++i)
        {
            uptr elemAddr = data + i * m_off.ConvexElem_Size;

            // VertexData: TArray<FVector> (双精度时每个 FVector 24 字节)
            uptr vertsData = 0;
            i32 vertsCount = 0;
            if (!ReadPtr(m_mem, elemAddr + m_off.ConvexElem_VertexData, vertsData) ||
                !ReadValue(m_mem, elemAddr + m_off.ConvexElem_VertexData + 8, vertsCount))
            {
                continue;
            }
            if (!IsCanonicalUserPtr(vertsData) || vertsCount <= 0 || vertsCount > 4096)
            {
                continue;
            }

            // IndexData: TArray<int32>
            uptr idxData = 0;
            i32 idxCount = 0;
            ReadPtr(m_mem, elemAddr + m_off.ConvexElem_IndexData, idxData);
            ReadValue(m_mem, elemAddr + m_off.ConvexElem_IndexData + 8, idxCount);

            CollisionShape shape;
            shape.geomType = eGEOM_CONVEX;

            // 读取双精度顶点
            i32 vecSize = m_ueOff.bUseDoublePrecision ? 24 : 12;
            std::vector<u8> vertsBuf(vertsCount * vecSize);
            m_mem.Read(vertsData, vertsBuf.data(), vertsBuf.size());

            shape.convexVerts.reserve(vertsCount);
            for (i32 v = 0; v < vertsCount; ++v)
            {
                Vec3 vert;
                if (m_ueOff.bUseDoublePrecision)
                {
                    double dx, dy, dz;
                    memcpy(&dx, &vertsBuf[v * 24 + 0], 8);
                    memcpy(&dy, &vertsBuf[v * 24 + 8], 8);
                    memcpy(&dz, &vertsBuf[v * 24 + 16], 8);
                    vert = { (float)dx, (float)dy, (float)dz };
                }
                else
                {
                    float fx, fy, fz;
                    memcpy(&fx, &vertsBuf[v * 12 + 0], 4);
                    memcpy(&fy, &vertsBuf[v * 12 + 4], 4);
                    memcpy(&fz, &vertsBuf[v * 12 + 8], 4);
                    vert = { fx, fy, fz };
                }
                shape.convexVerts.push_back(vert);
            }

            // 如果有 IndexData，从中提取边
            if (IsCanonicalUserPtr(idxData) && idxCount >= 3 && idxCount <= 65536)
            {
                std::vector<i32> indices(idxCount);
                m_mem.Read(idxData, indices.data(), idxCount * sizeof(i32));

                // 三角形索引 → 边集合
                std::set<std::pair<u8, u8>> edgeSet;
                for (i32 t = 0; t + 2 < idxCount; t += 3)
                {
                    i32 a = indices[t], b = indices[t + 1], c = indices[t + 2];
                    if (a >= 0 && a < 256 && b >= 0 && b < 256 && c >= 0 && c < 256)
                    {
                        auto addEdge = [&](u8 x, u8 y)
                        {
                            if (x > y) std::swap(x, y);
                            edgeSet.insert({x, y});
                        };
                        addEdge((u8)a, (u8)b);
                        addEdge((u8)b, (u8)c);
                        addEdge((u8)c, (u8)a);
                    }
                }
                shape.convexEdges.assign(edgeSet.begin(), edgeSet.end());
            }

            shape.worldPose = worldPose;
            outActor.shapes.push_back(std::move(shape));
        }
    }

    // ─── 欧拉角（度数） → 四元数 ───

    static Quat EulerToQuat(double pitchDeg, double yawDeg, double rollDeg)
    {
        constexpr double DEG2RAD = 3.14159265358979323846 / 180.0;
        double p = pitchDeg * DEG2RAD * 0.5;
        double y = yawDeg * DEG2RAD * 0.5;
        double r = rollDeg * DEG2RAD * 0.5;

        double cp = cos(p), sp = sin(p);
        double cy = cos(y), sy = sin(y);
        double cr = cos(r), sr = sin(r);

        // UE 的欧拉角约定 (Pitch=Y, Yaw=Z, Roll=X)
        Quat q;
        q.x = (float)(cr * sp * sy - sr * cp * cy);
        q.y = (float)(-cr * sp * cy - sr * cp * sy);
        q.z = (float)(cr * cp * sy - sr * sp * cy);
        q.w = (float)(cr * cp * cy + sr * sp * sy);
        return q;
    }
};

} // namespace xrd
