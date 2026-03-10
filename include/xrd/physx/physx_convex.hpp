#pragma once

#include "../memory/memory.hpp"
#include <algorithm>
#include <set>
#include <vector>

namespace xrd
{
    inline void ReadPhysXConvexTopology(
        const IMemoryAccessor& mem,
        uptr polygonsPtr,
        u8 polygonCount,
        u8 vertexCount,
        std::vector<std::pair<u8, u8>>& outEdges,
        std::vector<u8>& outTriangles)
    {
        outEdges.clear();
        outTriangles.clear();

        if (!IsCanonicalUserPtr(polygonsPtr) || polygonCount == 0 || vertexCount < 3)
        {
            return;
        }

        constexpr u32 kHullPolygonDataSize = 20;
        u32 polygonBytes = static_cast<u32>(polygonCount) * kHullPolygonDataSize;

        std::vector<u8> polygonBuffer(polygonBytes);
        if (!mem.Read(polygonsPtr, polygonBuffer.data(), polygonBytes))
        {
            return;
        }

        uptr vertexRefBase = polygonsPtr + polygonBytes;
        u32 totalIndexCount = 0;

        for (u8 polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex)
        {
            std::size_t polygonOffset =
                static_cast<std::size_t>(polygonIndex) * kHullPolygonDataSize;
            u16 vertexRefOffset = *reinterpret_cast<const u16*>(&polygonBuffer[polygonOffset + 16]);
            u8 polygonVertexCount = polygonBuffer[polygonOffset + 18];

            if (polygonVertexCount < 3 || polygonVertexCount > 32)
            {
                continue;
            }

            u32 polygonEnd = static_cast<u32>(vertexRefOffset) + polygonVertexCount;
            if (polygonEnd > totalIndexCount)
            {
                totalIndexCount = polygonEnd;
            }
        }

        if (totalIndexCount == 0)
        {
            return;
        }

        std::vector<u8> allIndices(totalIndexCount);
        if (!mem.Read(vertexRefBase, allIndices.data(), totalIndexCount))
        {
            return;
        }

        std::set<std::pair<u8, u8>> edgeSet;

        for (u8 polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex)
        {
            std::size_t polygonOffset =
                static_cast<std::size_t>(polygonIndex) * kHullPolygonDataSize;
            u16 vertexRefOffset = *reinterpret_cast<const u16*>(&polygonBuffer[polygonOffset + 16]);
            u8 polygonVertexCount = polygonBuffer[polygonOffset + 18];

            if (polygonVertexCount < 3 || polygonVertexCount > 32)
            {
                continue;
            }

            if (static_cast<u32>(vertexRefOffset) + polygonVertexCount > totalIndexCount)
            {
                continue;
            }

            u8 polygonBaseIndex = allIndices[vertexRefOffset];
            if (polygonBaseIndex >= vertexCount)
            {
                continue;
            }

            for (u8 vertexIndex = 0; vertexIndex < polygonVertexCount; ++vertexIndex)
            {
                u8 a = allIndices[vertexRefOffset + vertexIndex];
                u8 b = allIndices[vertexRefOffset + (vertexIndex + 1) % polygonVertexCount];
                if (a > b)
                {
                    std::swap(a, b);
                }
                if (a < vertexCount && b < vertexCount)
                {
                    edgeSet.insert({ a, b });
                }
            }

            for (u8 vertexIndex = 1; vertexIndex + 1 < polygonVertexCount; ++vertexIndex)
            {
                u8 b = allIndices[vertexRefOffset + vertexIndex];
                u8 c = allIndices[vertexRefOffset + vertexIndex + 1];

                if (b >= vertexCount || c >= vertexCount)
                {
                    continue;
                }

                if (polygonBaseIndex == b || polygonBaseIndex == c || b == c)
                {
                    continue;
                }

                outTriangles.push_back(polygonBaseIndex);
                outTriangles.push_back(b);
                outTriangles.push_back(c);
            }
        }

        outEdges.assign(edgeSet.begin(), edgeSet.end());
    }
}
