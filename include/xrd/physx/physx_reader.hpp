#pragma once
// Xrd-eXternalrEsolve - PhysX 内存读取器
// 从远程进程读取 PhysX scene/actor/shape 数据

#include "physx_types.hpp"
#include "../memory/memory.hpp"

namespace xrd
{

class PhysXReader
{
public:
    PhysXReader(const IMemoryAccessor& mem, uptr physxGlobalPtr)
        : m_mem(mem), m_globalPtr(physxGlobalPtr)
    {
    }

    // 获取偏移表引用（可在外部修改以适配不同引擎版本）
    PhysXOffsets& Offsets() { return m_off; }
    const PhysXOffsets& Offsets() const { return m_off; }

    // 读取所有 scene 的 static actor 碰撞数据（仅 eSIMULATION_SHAPE）
    bool ReadStaticCollision(PxSceneData& outData) const
    {
        outData.staticActors.clear();
        outData.dynamicActors.clear();

        auto scenes = ReadSceneList();
        for (uptr scenePtr : scenes)
        {
            auto actors = ReadActorList(scenePtr);
            for (uptr actorPtr : actors)
            {
                PxActorData ad;
                if (!ReadActorData(actorPtr, ad))
                {
                    continue;
                }

                // 只保留有可渲染 shape 的 static actor
                if (ad.type != eRIGID_STATIC)
                {
                    continue;
                }

                bool hasRenderable = false;
                for (auto& s : ad.shapes)
                {
                    if (s.geomType == eBOX || s.geomType == eSPHERE ||
                        s.geomType == eCAPSULE || s.geomType == eCONVEXMESH)
                    {
                        hasRenderable = true;
                        break;
                    }
                }
                if (hasRenderable)
                {
                    outData.staticActors.push_back(std::move(ad));
                }
            }
        }
        return true;
    }

    // 读取 actor 数据（pose + shapes）
    bool ReadActorData(uptr actorAddr, PxActorData& outData) const
    {
        outData.address = actorAddr;

        // 读取 actor type
        u16 rawType = 0;
        if (!ReadValue(m_mem, actorAddr + m_off.actor_type, rawType))
        {
            return false;
        }

        if (rawType == m_off.staticType)
        {
            outData.type = eRIGID_STATIC;
            if (!ReadStaticGlobalPose(actorAddr, outData.globalPose))
            {
                return false;
            }
        }
        else if (rawType == m_off.dynamicType)
        {
            outData.type = eRIGID_DYNAMIC;
            if (!ReadDynamicGlobalPose(actorAddr, outData.globalPose))
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        // 读取 shapes
        ReadShapes(actorAddr, outData);
        return true;
    }

private:
    const IMemoryAccessor& m_mem;
    uptr m_globalPtr = 0;
    PhysXOffsets m_off;

    // ─── Scene / Actor 列表读取 ───

    std::vector<uptr> ReadSceneList() const
    {
        std::vector<uptr> result;

        uptr scenesData = 0;
        u32 scenesCount = 0;
        if (!ReadPtr(m_mem, m_globalPtr + m_off.physics_scenes, scenesData) ||
            !ReadValue(m_mem, m_globalPtr + m_off.physics_scenesCount, scenesCount))
        {
            return result;
        }

        scenesCount &= 0x7FFFFFFF;
        if (!IsCanonicalUserPtr(scenesData) || scenesCount == 0 || scenesCount > 64)
        {
            return result;
        }

        result.resize(scenesCount);
        m_mem.Read(scenesData, result.data(), scenesCount * sizeof(uptr));
        return result;
    }

    std::vector<uptr> ReadActorList(uptr scenePtr) const
    {
        std::vector<uptr> result;
        if (!IsCanonicalUserPtr(scenePtr))
        {
            return result;
        }

        uptr actorsData = 0;
        u32 actorsCount = 0;
        if (!ReadPtr(m_mem, scenePtr + m_off.scene_actors, actorsData) ||
            !ReadValue(m_mem, scenePtr + m_off.scene_actorsCount, actorsCount))
        {
            return result;
        }

        if (!IsCanonicalUserPtr(actorsData) || actorsCount == 0 || actorsCount > 100000)
        {
            return result;
        }

        result.resize(actorsCount);
        m_mem.Read(actorsData, result.data(), actorsCount * sizeof(uptr));
        return result;
    }

    // ─── Pose 读取 ───

    bool ReadStaticGlobalPose(uptr actorAddr, PxTransform& outPose) const
    {
        u8 flag = 0;
        ReadValue(m_mem, actorAddr + m_off.staticPose_flag, flag);

        if (flag & m_off.staticPose_flagBit)
        {
            // 外部存储
            uptr extPtr = 0;
            if (!ReadPtr(m_mem, actorAddr + m_off.staticPose_extPtr, extPtr) ||
                !IsCanonicalUserPtr(extPtr))
            {
                return false;
            }
            return ReadValue(m_mem, extPtr + m_off.staticPose_extOffset, outPose);
        }
        else
        {
            return ReadValue(m_mem, actorAddr + m_off.staticPose_inline, outPose);
        }
    }

    bool ReadDynamicGlobalPose(uptr actorAddr, PxTransform& outPose) const
    {
        // 读取 body2world
        PxTransform b2w{};
        u32 dynFlag = 0;
        ReadValue(m_mem, actorAddr + m_off.dynPose_flagAddr, dynFlag);

        if (dynFlag & m_off.dynPose_flagBit)
        {
            uptr extPtr = 0;
            if (!ReadPtr(m_mem, actorAddr + m_off.dynPose_extPtr, extPtr) ||
                !IsCanonicalUserPtr(extPtr))
            {
                return false;
            }
            if (!ReadValue(m_mem, extPtr + m_off.dynPose_extOffset, b2w))
            {
                return false;
            }
        }
        else
        {
            if (!ReadValue(m_mem, actorAddr + m_off.dynPose_b2wInline, b2w))
            {
                return false;
            }
        }

        // 读取 body2actor 并计算 globalPose = b2w × inverse(b2a)
        PxTransform b2a{};
        ReadValue(m_mem, actorAddr + m_off.dynPose_body2actor, b2a);

        // body2actor 为单位变换则直接用 body2world
        bool isIdentity = (std::abs(b2a.q.x) < 1e-5f && std::abs(b2a.q.y) < 1e-5f &&
                           std::abs(b2a.q.z) < 1e-5f && std::abs(b2a.q.w - 1.0f) < 1e-5f &&
                           std::abs(b2a.p.x) < 1e-5f && std::abs(b2a.p.y) < 1e-5f &&
                           std::abs(b2a.p.z) < 1e-5f);
        if (isIdentity)
        {
            outPose = b2w;
        }
        else
        {
            // inverse(b2a)
            PxQuat invQ = QuatConjugate(b2a.q);
            PxVec3 invP = QuatRotate(invQ, { -b2a.p.x, -b2a.p.y, -b2a.p.z });
            PxTransform invB2A = { invQ, invP };
            outPose = TransformMultiply(b2w, invB2A);
        }
        return true;
    }

    // ─── Shape 读取 ───

    void ReadShapes(uptr actorAddr, PxActorData& outData) const
    {
        // PtrTable：+0x28 data, +0x30 count
        uptr shapesData = 0;
        u16 shapesCount = 0;
        if (!ReadPtr(m_mem, actorAddr + m_off.actor_shapesData, shapesData) ||
            !ReadValue(m_mem, actorAddr + m_off.actor_shapesCount, shapesCount))
        {
            return;
        }
        if (shapesCount == 0 || shapesCount > 256)
        {
            return;
        }

        // PtrTable 内联优化：count==1 时 data 字段直接存指针值
        std::vector<uptr> shapeAddrs;
        if (shapesCount == 1)
        {
            if (IsCanonicalUserPtr(shapesData))
            {
                shapeAddrs.push_back(shapesData);
            }
        }
        else
        {
            if (!IsCanonicalUserPtr(shapesData))
            {
                return;
            }
            shapeAddrs.resize(shapesCount);
            m_mem.Read(shapesData, shapeAddrs.data(), shapesCount * sizeof(uptr));
        }

        outData.shapes.reserve(shapeAddrs.size());
        for (uptr sa : shapeAddrs)
        {
            if (!IsCanonicalUserPtr(sa))
            {
                continue;
            }
            PxShapeData sd;
            if (ReadShapeData(sa, sd))
            {
                // 只保留有物理碰撞的 shape（eSIMULATION_SHAPE）
                if (sd.pxFlags & eSIMULATION_SHAPE)
                {
                    outData.shapes.push_back(std::move(sd));
                }
            }
        }
    }

    bool ReadShapeData(uptr shapeAddr, PxShapeData& outData) const
    {
        // 读取 PxShapeFlags
        outData.pxFlags = ReadPxShapeFlags(shapeAddr);

        // 判断内联/外部存储
        u8 flagByte = 0;
        ReadValue(m_mem, shapeAddr + m_off.shape_flagByte, flagByte);

        uptr extPtr = 0;
        if (flagByte & 0x44)
        {
            ReadPtr(m_mem, shapeAddr + m_off.shape_extPtr, extPtr);
        }

        // localPose
        if (flagByte & 0x04)
        {
            if (!IsCanonicalUserPtr(extPtr))
            {
                return false;
            }
            ReadValue(m_mem, extPtr, outData.localPose);
        }
        else
        {
            ReadValue(m_mem, shapeAddr + m_off.shape_localPose, outData.localPose);
        }

        // geometry
        uptr geomAddr = 0;
        if (flagByte & 0x01)
        {
            if (!IsCanonicalUserPtr(extPtr))
            {
                return false;
            }
            geomAddr = extPtr + 0x38;
        }
        else
        {
            geomAddr = shapeAddr + m_off.shape_geomType;
        }

        return ReadGeometryData(geomAddr, outData);
    }

    u32 ReadPxShapeFlags(uptr shapeAddr) const
    {
        u8 flagByte = 0;
        ReadValue(m_mem, shapeAddr + m_off.shape_flagByte, flagByte);

        if (flagByte & m_off.shapeFlags_flagBit)
        {
            uptr extPtr = 0;
            if (!ReadPtr(m_mem, shapeAddr + m_off.shape_extPtr, extPtr) ||
                !IsCanonicalUserPtr(extPtr))
            {
                return 0;
            }
            u8 flags = 0;
            ReadValue(m_mem, extPtr + m_off.shapeFlags_extOff, flags);
            return flags;
        }
        else
        {
            u8 flags = 0;
            ReadValue(m_mem, shapeAddr + m_off.shapeFlagsInline, flags);
            return flags;
        }
    }

    bool ReadGeometryData(uptr geomAddr, PxShapeData& outData) const
    {
        u32 geomType = eGEOM_INVALID;
        if (!ReadValue(m_mem, geomAddr, geomType))
        {
            return false;
        }
        outData.geomType = static_cast<PxGeometryType>(geomType);

        uptr dataAddr = geomAddr + 4;
        switch (outData.geomType)
        {
            case eBOX:
                ReadValue(m_mem, dataAddr, outData.geom.box);
                break;
            case eSPHERE:
                ReadValue(m_mem, dataAddr, outData.geom.sphere);
                break;
            case eCAPSULE:
                ReadValue(m_mem, dataAddr, outData.geom.capsule);
                break;
            case eCONVEXMESH:
                return ReadConvexMeshData(dataAddr, outData);
            default:
                break;
        }
        return true;
    }

    // ─── ConvexMesh 读取 ───

    // ConvexMeshGeometry 内偏移
    static constexpr u32 kConvexGeom_MeshPtr    = 0x1C;
    // Gu::ConvexMesh 内偏移
    static constexpr u32 kConvex_Vertices       = 0x20;
    static constexpr u32 kConvex_Polygons       = 0x30;
    static constexpr u32 kConvex_NbVertices     = 0x4A;
    static constexpr u32 kConvex_NbPolygons     = 0x4B;
    static constexpr u32 kHullPolygonDataSize   = 20;

    bool ReadConvexMeshData(uptr geomDataAddr, PxShapeData& outData) const
    {
        uptr meshPtr = 0;
        if (!ReadPtr(m_mem, geomDataAddr + kConvexGeom_MeshPtr, meshPtr) ||
            !IsCanonicalUserPtr(meshPtr))
        {
            return false;
        }

        u8 nbVerts = 0;
        if (!ReadValue(m_mem, meshPtr + kConvex_NbVertices, nbVerts) || nbVerts == 0)
        {
            return false;
        }

        uptr vertsPtr = 0;
        if (!ReadPtr(m_mem, meshPtr + kConvex_Vertices, vertsPtr) ||
            !IsCanonicalUserPtr(vertsPtr))
        {
            return false;
        }

        // 一次性批量读取全部顶点（原来逐个读，现在只需 1 次 Read）
        outData.convexVerts.resize(nbVerts);
        m_mem.Read(vertsPtr, outData.convexVerts.data(), nbVerts * sizeof(PxVec3));

        // 读多边形提取边
        u8 nbPolys = 0;
        ReadValue(m_mem, meshPtr + kConvex_NbPolygons, nbPolys);

        uptr polysPtr = 0;
        ReadPtr(m_mem, meshPtr + kConvex_Polygons, polysPtr);

        if (nbPolys > 0 && IsCanonicalUserPtr(polysPtr))
        {
            // 一次性批量读取全部多边形头（原来每个多边形 2 次 Read）
            u32 polyBufSize = nbPolys * kHullPolygonDataSize;
            std::vector<u8> polyBuf(polyBufSize);
            m_mem.Read(polysPtr, polyBuf.data(), polyBufSize);

            // 计算顶点索引区总大小并一次性读取
            uptr vertRefBase = polysPtr + polyBufSize;
            u32 totalIndices = 0;
            for (u8 p = 0; p < nbPolys; ++p)
            {
                u16 vRef8 = *reinterpret_cast<u16*>(&polyBuf[p * kHullPolygonDataSize + 16]);
                u8 polyNbVerts = polyBuf[p * kHullPolygonDataSize + 18];
                if (polyNbVerts >= 3 && polyNbVerts <= 32)
                {
                    u32 end = static_cast<u32>(vRef8) + polyNbVerts;
                    if (end > totalIndices) totalIndices = end;
                }
            }

            std::vector<u8> allIndices(totalIndices);
            if (totalIndices > 0)
            {
                m_mem.Read(vertRefBase, allIndices.data(), totalIndices);
            }

            // 从已缓存的数据中提取边
            std::set<std::pair<u8, u8>> edgeSet;
            for (u8 p = 0; p < nbPolys; ++p)
            {
                u16 vRef8 = *reinterpret_cast<u16*>(&polyBuf[p * kHullPolygonDataSize + 16]);
                u8 polyNbVerts = polyBuf[p * kHullPolygonDataSize + 18];

                if (polyNbVerts < 3 || polyNbVerts > 32)
                {
                    continue;
                }
                if (static_cast<u32>(vRef8) + polyNbVerts > totalIndices)
                {
                    continue;
                }

                for (u8 v = 0; v < polyNbVerts; ++v)
                {
                    u8 a = allIndices[vRef8 + v];
                    u8 b = allIndices[vRef8 + (v + 1) % polyNbVerts];
                    if (a > b) std::swap(a, b);
                    if (a < nbVerts && b < nbVerts)
                    {
                        edgeSet.insert({ a, b });
                    }
                }
            }

            outData.convexEdges.assign(edgeSet.begin(), edgeSet.end());
        }
        return true;
    }
};

} // namespace xrd
