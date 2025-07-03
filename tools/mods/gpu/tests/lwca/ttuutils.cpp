/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <math.h>
#include <algorithm>
#include "gpu/tests/lwca/ttuutils.h"
#include "core/include/utility.h"
#include "core/include/bitfield.h"

namespace
{
    UINT32 CreateMask(UINT32 numBits)
    {
        MASSERT(numBits <= 32);
        return (numBits == 32) ? (~0u) : ((1u << numBits) - 1);
    }
}

void BoundingBox::ToScl(UINT08* pXscl, UINT08* pYscl, UINT08* pZscl) const
{
    MASSERT(pXscl);
    MASSERT(pYscl);
    MASSERT(pZscl);

    // In the TTU, max = min + (1 << scl)
    // In other words, scl is the exponent of the float which we add to min
    //
    // See https://wiki.lwpu.com/gpuhwvolta/index.php/TTU/uArch/RCT#Complet_Decompression
    //
    float xlen = xmax - xmin;
    float ylen = ymax - ymin;
    float zlen = zmax - zmin;
    UINT32 xData = reinterpret_cast<UINT32*>(&xlen)[0];
    UINT32 yData = reinterpret_cast<UINT32*>(&ylen)[0];
    UINT32 zData = reinterpret_cast<UINT32*>(&zlen)[0];

    // To colwert from our desired max to scl, we grab the exponent from the float
    UINT08 xscl = (xData >> 23) & 0xFF;
    UINT08 yscl = (yData >> 23) & 0xFF;
    UINT08 zscl = (zData >> 23) & 0xFF;

    // Add 1 to scl if the resulting bounding-box isn't large enough
    if (xData & 0x7FFFFF) xscl++;
    if (yData & 0x7FFFFF) yscl++;
    if (zData & 0x7FFFFF) zscl++;

    *pXscl = xscl;
    *pYscl = yscl;
    *pZscl = zscl;
}

void Child::SetAABB(const BoundingBox& bbox, const BoundingBox& childAABB)
{
    UINT08 xscl = 0;
    UINT08 yscl = 0;
    UINT08 zscl = 0;
    bbox.ToScl(&xscl, &yscl, &zscl);

    UINT32 xscl_UINT32 = (xscl << 23);
    UINT32 yscl_UINT32 = (yscl << 23);
    UINT32 zscl_UINT32 = (zscl << 23);

    float dx = reinterpret_cast<float*>(&xscl_UINT32)[0];
    float dy = reinterpret_cast<float*>(&yscl_UINT32)[0];
    float dz = reinterpret_cast<float*>(&zscl_UINT32)[0];

    // lo values are 0..255  lo = child_offset * (256 / complet_len)
    // round down
    double xtmp = floor(256.0 * (childAABB.xmin - bbox.xmin) / dx);
    double ytmp = floor(256.0 * (childAABB.ymin - bbox.ymin) / dy);
    double ztmp = floor(256.0 * (childAABB.zmin - bbox.zmin) / dz);

    // Shouldn't have out-of-bounds children
    MASSERT(xtmp >= 0.0 && xtmp <= 256.0);
    MASSERT(ytmp >= 0.0 && ytmp <= 256.0);
    MASSERT(ztmp >= 0.0 && ztmp <= 256.0);

    // Make sure lo is within bounds
    xlo = static_cast<UINT08>(min(255.0, max(0.0, xtmp)));
    ylo = static_cast<UINT08>(min(255.0, max(0.0, ytmp)));
    zlo = static_cast<UINT08>(min(255.0, max(0.0, ztmp)));

    // hi values are 1..256  hi = child_offset * (256 / complet_len) - 1
    // round up
    //
    // We subtract one since we can only store 0..255 in 8 bits
    xtmp = ceil((256.0 * (childAABB.xmax - bbox.xmin) / dx) - 1.0);
    ytmp = ceil((256.0 * (childAABB.ymax - bbox.ymin) / dy) - 1.0);
    ztmp = ceil((256.0 * (childAABB.zmax - bbox.zmin) / dz) - 1.0);

    // Make sure hi is within bounds
    xhi = static_cast<UINT08>(min(255.0, max(0.0, xtmp)));
    yhi = static_cast<UINT08>(min(255.0, max(0.0, ytmp)));
    zhi = static_cast<UINT08>(min(255.0, max(0.0, ztmp)));
}

UINT32 TriangleRange::NumLines() const
{
    // Each entry in m_ComprData corresponds to a compressed triangle block
    // Does not include visibility blocks (if present) as these are serialized separately
    if (!m_ComprData.empty())
    {
        MASSERT(m_ComprData.size() <= s_MaxTriBlocks);
        return static_cast<UINT32>(m_ComprData.size());
    }
    // Uncompressed triangle ranges can have their size be callwlated directly
    else
    {
        UINT32 lastTriUsed = s_FirstTriIdx + static_cast<UINT32>(m_Triangles.size() - 1);
        UINT32 numLines = lastTriUsed / s_MaxTrisPerUncomprBlock + 1;
        MASSERT(numLines <= s_MaxTriBlocks);
        return numLines;
    }
}

BoundingBox TriangleRange::GenerateAABB(bool useEndTris) const
{
    MASSERT(m_Triangles.size());

    float xmin = std::numeric_limits<float>::infinity();
    float ymin = xmin;
    float zmin = ymin;

    float xmax = -std::numeric_limits<float>::infinity();
    float ymax = xmax;
    float zmax = ymax;
    if (useEndTris)
    {
        MASSERT(m_UseMotionBlur);
        MASSERT(m_EndTriangles.size() == m_Triangles.size());
        for (const auto& endTri : m_EndTriangles)
        {
            // V0
            xmin = min(xmin, endTri.v0.x);
            ymin = min(ymin, endTri.v0.y);
            zmin = min(zmin, endTri.v0.z);
            xmax = max(xmax, endTri.v0.x);
            ymax = max(ymax, endTri.v0.y);
            zmax = max(zmax, endTri.v0.z);
            // V1
            xmin = min(xmin, endTri.v1.x);
            ymin = min(ymin, endTri.v1.y);
            zmin = min(zmin, endTri.v1.z);
            xmax = max(xmax, endTri.v1.x);
            ymax = max(ymax, endTri.v1.y);
            zmax = max(zmax, endTri.v1.z);
            // V2
            xmin = min(xmin, endTri.v2.x);
            ymin = min(ymin, endTri.v2.y);
            zmin = min(zmin, endTri.v2.z);
            xmax = max(xmax, endTri.v2.x);
            ymax = max(ymax, endTri.v2.y);
            zmax = max(zmax, endTri.v2.z);
        }
    }
    else
    {
        for (const auto& tri : m_Triangles)
        {
            // V0
            xmin = min(xmin, tri.v0.x);
            ymin = min(ymin, tri.v0.y);
            zmin = min(zmin, tri.v0.z);
            xmax = max(xmax, tri.v0.x);
            ymax = max(ymax, tri.v0.y);
            zmax = max(zmax, tri.v0.z);
            // V1
            xmin = min(xmin, tri.v1.x);
            ymin = min(ymin, tri.v1.y);
            zmin = min(zmin, tri.v1.z);
            xmax = max(xmax, tri.v1.x);
            ymax = max(ymax, tri.v1.y);
            zmax = max(zmax, tri.v1.z);
            // V2
            xmin = min(xmin, tri.v2.x);
            ymin = min(ymin, tri.v2.y);
            zmin = min(zmin, tri.v2.z);
            xmax = max(xmax, tri.v2.x);
            ymax = max(ymax, tri.v2.y);
            zmax = max(zmax, tri.v2.z);
        }
    }
    return {xmin, ymin, zmin, xmax, ymax, zmax};
}

RC TriangleRange::InitComprBlock(const Triangle& firstTri, UINT32 prec)
{
    RC rc;

    // Check that there are enough lines for a new triangle block
    if (m_ComprData.size() + 1 > s_MaxTriBlocks)
    {
        Printf(Tee::PriError,
               "Unable to initialize compressed triangle block. "
               "Too many triangles in triangle range.\n");
        return RC::SOFTWARE_ERROR;
    }

    // Reset state
    m_ComprState = {};

    // Set up block data
    ComprBlockData compr = {};
    compr.baseTriId = firstTri.triId;
    compr.prec = prec;

    // Base Triangle - X, Y, Z
    compr.baseX = reinterpret_cast<const UINT32*>(&firstTri.v0.x)[0];
    compr.baseY = reinterpret_cast<const UINT32*>(&firstTri.v0.y)[0];
    compr.baseZ = reinterpret_cast<const UINT32*>(&firstTri.v0.z)[0];

    // Add triangle block to triangle range
    m_ComprState.blockInitialized = true;
    m_ComprData.push_back(compr);
    return rc;
}

RC TriangleRange::FinalizeComprBlock()
{
    RC rc;
    auto& compr = m_ComprData.back();

    // Callwlate the mask needed for compressing the vertices
    const INT32 msb = max(max(Utility::BitScanReverse(m_ComprState.diffX),
                              Utility::BitScanReverse(m_ComprState.diffY)),
                              Utility::BitScanReverse(m_ComprState.diffZ));
    // Find highest common bit. This bit is part of the 'prec' bits,
    // so '(msb + 1) - (prec)' bits are discared by compression
    compr.shift = static_cast<UINT32>(max((msb + 1) - static_cast<INT32>(compr.prec), 0));

    // Finalize block
    m_ComprState.blockInitialized = false;
    return rc;
}

RC TriangleRange::AddTri(const Triangle& tri, const Triangle& endTri, UINT08 prec)
{
    RC rc;

    if ((m_UseMotionBlur || m_UseVisibilityMasks) && !prec)
    {
        MASSERT(!"Motion blur and visibility masks require triangle compression");
        return RC::SOFTWARE_ERROR;
    }

    // Add triangle to compressed triangle range
    if (prec)
    {
        if (m_ComprData.empty() && !m_Triangles.empty())
        {
            MASSERT(!"Mixing compressed and uncompressed tri blocks is lwrrently unsupported!");
            return RC::SOFTWARE_ERROR;
        }

        // If we are adding the first triangle of a new triangle block,
        // initialize the block using that triangle as the base
        if (!m_ComprState.blockInitialized)
        {
            CHECK_RC(InitComprBlock(tri, prec));
        }

        auto& compr = m_ComprData.back();
        if (prec != compr.prec)
        {
            MASSERT(!"All triangles of a triangle block must have the same precision!");
            return RC::SOFTWARE_ERROR;
        }

        // Callwlate the compression needed for the triangle IDs (min is 1 bit)
        const UINT32 tmpDiffTriId = m_ComprState.diffTriId | (compr.baseTriId ^ tri.triId);
        const UINT32 tmpIdPrec =
            static_cast<UINT32>(max(Utility::BitScanReverse(tmpDiffTriId) + 1, 1));

        // Callwlate the number of bits required for the triangle block if the triangle is added
        const UINT32 tmpNumTris = compr.numTris + 1;
        const UINT32 overheadBits =
            96 + // VertexBase
             2 + // Base triangle alpha/no-lwll bits in VertexIDs
            (m_UseVisibilityMasks ? 50 : 0) + // VMOffset.Base (43b) and VMInfo (7b)
            32 + // TriangleID Base
            32;  // Header

        // TODO we could get better compression by making use of shared edges in this callwlation
        const UINT32 vertexIdNumTris = (m_UseMotionBlur) ? tmpNumTris : tmpNumTris - 1;
        const UINT32 numVertices = (m_UseMotionBlur) ? 6 : 3;
        const UINT32 visibilityMaskBits =
            (m_UseVisibilityMasks) ? tmpNumTris * 6 + (tmpNumTris - 1) * s_PrecisiolwMOffset : 0;
        const UINT32 bitsRequired =
            overheadBits +
            (tmpNumTris * numVertices - 1) * (3 * prec) + // VertexPositions
            (tmpNumTris - 1) * tmpIdPrec +                // TriangleIDs
            vertexIdNumTris * (numVertices * 4 + 2) +     // VertexIDs
            visibilityMaskBits;                           // Visibility Masks

        // If the triangle doesn't fit in the block, finalize the block and retry with a new block
        if (bitsRequired > 1024 || tmpNumTris > s_MaxTrisPerComprBlock)
        {
            MASSERT(compr.numTris > 0);
            CHECK_RC(FinalizeComprBlock());

            // Retry with a new block
            return AddTri(tri, endTri, prec);
        }

        // Otherwise add the triangle
        compr.numTris = tmpNumTris;
        compr.idPrec = tmpIdPrec;

        // For the first triangle skip over the base id and base vertex
        if (compr.numTris > 1)
        {
            m_ComprState.diffTriId = tmpDiffTriId;
            // V0
            m_ComprState.diffX |= compr.baseX ^ reinterpret_cast<const UINT32*>(&tri.v0.x)[0];
            m_ComprState.diffY |= compr.baseY ^ reinterpret_cast<const UINT32*>(&tri.v0.y)[0];
            m_ComprState.diffZ |= compr.baseZ ^ reinterpret_cast<const UINT32*>(&tri.v0.z)[0];
        }
        // V1
        m_ComprState.diffX |= compr.baseX ^ reinterpret_cast<const UINT32*>(&tri.v1.x)[0];
        m_ComprState.diffY |= compr.baseY ^ reinterpret_cast<const UINT32*>(&tri.v1.y)[0];
        m_ComprState.diffZ |= compr.baseZ ^ reinterpret_cast<const UINT32*>(&tri.v1.z)[0];
        // V2
        m_ComprState.diffX |= compr.baseX ^ reinterpret_cast<const UINT32*>(&tri.v2.x)[0];
        m_ComprState.diffY |= compr.baseY ^ reinterpret_cast<const UINT32*>(&tri.v2.y)[0];
        m_ComprState.diffZ |= compr.baseZ ^ reinterpret_cast<const UINT32*>(&tri.v2.z)[0];

        // Motion Blur End Triangle
        if (m_UseMotionBlur)
        {
            // V0
            m_ComprState.diffX |= compr.baseX ^ reinterpret_cast<const UINT32*>(&endTri.v0.x)[0];
            m_ComprState.diffY |= compr.baseY ^ reinterpret_cast<const UINT32*>(&endTri.v0.y)[0];
            m_ComprState.diffZ |= compr.baseZ ^ reinterpret_cast<const UINT32*>(&endTri.v0.z)[0];
            // V1
            m_ComprState.diffX |= compr.baseX ^ reinterpret_cast<const UINT32*>(&endTri.v1.x)[0];
            m_ComprState.diffY |= compr.baseY ^ reinterpret_cast<const UINT32*>(&endTri.v1.y)[0];
            m_ComprState.diffZ |= compr.baseZ ^ reinterpret_cast<const UINT32*>(&endTri.v1.z)[0];
            // V2
            m_ComprState.diffX |= compr.baseX ^ reinterpret_cast<const UINT32*>(&endTri.v2.x)[0];
            m_ComprState.diffY |= compr.baseY ^ reinterpret_cast<const UINT32*>(&endTri.v2.y)[0];
            m_ComprState.diffZ |= compr.baseZ ^ reinterpret_cast<const UINT32*>(&endTri.v2.z)[0];
        }
    }
    // Add triangle to uncompressed triangle range
    else
    {
        if (!m_ComprData.empty())
        {
            MASSERT(!"Mixing compressed and uncompressed tri blocks is lwrrently unsupported!");
            return RC::SOFTWARE_ERROR;
        }
        static constexpr UINT32 maxTris = s_MaxTrisPerUncomprBlock * s_MaxTriBlocks;
        const UINT32 triToUse = s_FirstTriIdx + static_cast<UINT32>(m_Triangles.size());
        if (triToUse >= maxTris)
        {
            Printf(Tee::PriError,
                   "Unable to add triangle at idx %d.\n"
                   "Max %d uncompressed triangles per triangle range\n",
                   triToUse, s_MaxTriBlocks * s_MaxTrisPerUncomprBlock);
            return RC::SOFTWARE_ERROR;
        }
    }

    // Copy triangle to internal storage
    m_Triangles.push_back(tri);
    if (m_UseMotionBlur)
    {
        m_EndTriangles.push_back(endTri);
    }
    return rc;
}

// Most of this is a more readable (but less general) rewrite of
// https://sc-p4-viewer-01.lwpu.com/get/hw/lwgpu_sma/clib/RefTTU/refttu/spec/ttuTypes.hpp
//
// See https://wiki.lwpu.com/gpuhwvolta/index.php/TTU/Programming#Triangle_Blocks
// for the documentation on the binary representation of the Triangle Block
//
void TriangleRange::Serialize(UINT08* dst, UINT08* vBlockDst, UINT64 vBlockDeviceAddr) const
{
    MASSERT(m_Triangles.size() > 0);

    // Handle compressed triangle blocks
    if (!m_ComprData.empty())
    {
        UINT32 baseTriIdx = 0;
        for (const auto& compr : m_ComprData)
        {
            Bitfield<UINT32, TTU_CACHE_LINE_BYTES * 8> bitField;
            vector<ComprVert> vertexPos;
            vector<UINT32> triangleIds;
            vector<UINT32> vertexIds;

            // Part 1/2: Generate all the compressed triangle data for the triangle block
            //
            // The first triangle is treated differently:
            //     The triangle ID is the base triangle ID
            //     The first vertex (VertexId 0) is always the base vertex
            //     The other vertices (VertexId 1 and 2) are first in the vertex position list
            //
            //  For motion blur triangles the vertices of the first triangle don't _have_ to be
            //  the base triangle, but we'll keep this behavior to make things easier.
            //  As such the first triangle must always have unique vertices.
            //
            const Triangle& triBase = m_Triangles[baseTriIdx];
            const Vertex& vBase = triBase.v0;
            const UINT32 bitMask = CreateMask(compr.prec);

            // Triangle IDs
            // The triangles have compressed TriangleIDs, based off of the base triangle
            //
            for (UINT32 i = baseTriIdx + 1; i < baseTriIdx + compr.numTris; i++)
            {
                const auto& tri = m_Triangles[i];
                const UINT32 idMask = CreateMask(compr.idPrec);
                triangleIds.push_back(tri.triId & idMask);
            }

            // Vertex Positions and IDs
            // The triangle vertices are stored as references in the Vertex ID array
            //
            for (UINT32 i = baseTriIdx; i < baseTriIdx + compr.numTris; i++)
            {
                const auto& tri = m_Triangles[i];

                // For non-motion blur cases the first triangle has implicitly-indexed vertices
                const bool implicitTri = (i == baseTriIdx) && (!m_UseMotionBlur);

                if (tri.v0 == vBase)
                {
                    if (!implicitTri)
                    {
                        vertexIds.push_back(0);
                    }
                }
                else
                {
                    ComprVert v0;

                    v0.x = (reinterpret_cast<const UINT32*>(&tri.v0.x)[0] >> compr.shift) & bitMask;
                    v0.y = (reinterpret_cast<const UINT32*>(&tri.v0.y)[0] >> compr.shift) & bitMask;
                    v0.z = (reinterpret_cast<const UINT32*>(&tri.v0.z)[0] >> compr.shift) & bitMask;
                    auto it = std::find(vertexPos.begin(), vertexPos.end(), v0);
                    const bool found = (it != vertexPos.end());
                    MASSERT(!(found && implicitTri));
                    if (!found)
                    {
                        vertexPos.push_back(v0);
                        it = vertexPos.end() - 1;
                    }
                    if (!implicitTri)
                    {
                        const UINT32 idx = static_cast<UINT32>(std::distance(vertexPos.begin(), it)) + 1;
                        vertexIds.push_back(idx);
                    }
                }

                if (tri.v1 == vBase)
                {
                    vertexIds.push_back(0);
                }
                else
                {
                    ComprVert v1;
                    v1.x = (reinterpret_cast<const UINT32*>(&tri.v1.x)[0] >> compr.shift) & bitMask;
                    v1.y = (reinterpret_cast<const UINT32*>(&tri.v1.y)[0] >> compr.shift) & bitMask;
                    v1.z = (reinterpret_cast<const UINT32*>(&tri.v1.z)[0] >> compr.shift) & bitMask;
                    auto it = std::find(vertexPos.begin(), vertexPos.end(), v1);
                    const bool found = (it != vertexPos.end());
                    MASSERT(!(found && implicitTri));
                    if (!found)
                    {
                        vertexPos.push_back(v1);
                        it = vertexPos.end() - 1;
                    }
                    if (!implicitTri)
                    {
                        const UINT32 idx = static_cast<UINT32>(std::distance(vertexPos.begin(), it)) + 1;
                        vertexIds.push_back(idx);
                    }
                }

                if (tri.v2 == vBase)
                {
                    vertexIds.push_back(0);
                }
                else
                {
                    ComprVert v2;
                    v2.x = (reinterpret_cast<const UINT32*>(&tri.v2.x)[0] >> compr.shift) & bitMask;
                    v2.y = (reinterpret_cast<const UINT32*>(&tri.v2.y)[0] >> compr.shift) & bitMask;
                    v2.z = (reinterpret_cast<const UINT32*>(&tri.v2.z)[0] >> compr.shift) & bitMask;
                    auto it = std::find(vertexPos.begin(), vertexPos.end(), v2);
                    const bool found = (it != vertexPos.end());
                    MASSERT(!(found && implicitTri));
                    if (!found)
                    {
                        vertexPos.push_back(v2);
                        it = vertexPos.end() - 1;
                    }
                    if (!implicitTri)
                    {
                        const UINT32 idx = static_cast<UINT32>(std::distance(vertexPos.begin(), it)) + 1;
                        vertexIds.push_back(idx);
                    }
                }

                // Motion-blur triangles have 6 vertices (3 extra for the end triangle)
                if (m_UseMotionBlur)
                {
                    MASSERT(!implicitTri);
                    const auto& endTri = m_EndTriangles[i];
                    if (endTri.v0 == vBase)
                    {
                        vertexIds.push_back(0);
                    }
                    else
                    {
                        ComprVert v0_end;
                        v0_end.x = (reinterpret_cast<const UINT32*>(&endTri.v0.x)[0] >> compr.shift) & bitMask;
                        v0_end.y = (reinterpret_cast<const UINT32*>(&endTri.v0.y)[0] >> compr.shift) & bitMask;
                        v0_end.z = (reinterpret_cast<const UINT32*>(&endTri.v0.z)[0] >> compr.shift) & bitMask;
                        auto it = std::find(vertexPos.begin(), vertexPos.end(), v0_end);
                        const bool found = (it != vertexPos.end());
                        if (!found)
                        {
                            vertexPos.push_back(v0_end);
                            it = vertexPos.end() - 1;
                        }
                        const UINT32 idx = static_cast<UINT32>(std::distance(vertexPos.begin(), it)) + 1;
                        vertexIds.push_back(idx);
                    }

                    if (endTri.v1 == vBase)
                    {
                        vertexIds.push_back(0);
                    }
                    else
                    {
                        ComprVert v1_end;
                        v1_end.x = (reinterpret_cast<const UINT32*>(&endTri.v1.x)[0] >> compr.shift) & bitMask;
                        v1_end.y = (reinterpret_cast<const UINT32*>(&endTri.v1.y)[0] >> compr.shift) & bitMask;
                        v1_end.z = (reinterpret_cast<const UINT32*>(&endTri.v1.z)[0] >> compr.shift) & bitMask;
                        auto it = std::find(vertexPos.begin(), vertexPos.end(), v1_end);
                        const bool found = (it != vertexPos.end());
                        if (!found)
                        {
                            vertexPos.push_back(v1_end);
                            it = vertexPos.end() - 1;
                        }
                        const UINT32 idx = static_cast<UINT32>(std::distance(vertexPos.begin(), it)) + 1;
                        vertexIds.push_back(idx);
                    }

                    if (endTri.v2 == vBase)
                    {
                        vertexIds.push_back(0);
                    }
                    else
                    {
                        ComprVert v2_end;
                        v2_end.x = (reinterpret_cast<const UINT32*>(&endTri.v2.x)[0] >> compr.shift) & bitMask;
                        v2_end.y = (reinterpret_cast<const UINT32*>(&endTri.v2.y)[0] >> compr.shift) & bitMask;
                        v2_end.z = (reinterpret_cast<const UINT32*>(&endTri.v2.z)[0] >> compr.shift) & bitMask;
                        auto it = std::find(vertexPos.begin(), vertexPos.end(), v2_end);
                        const bool found = (it != vertexPos.end());
                        if (!found)
                        {
                            vertexPos.push_back(v2_end);
                            it = vertexPos.end() - 1;
                        }
                        const UINT32 idx = static_cast<UINT32>(std::distance(vertexPos.begin(), it)) + 1;
                        vertexIds.push_back(idx);
                    }
                }
            }
            baseTriIdx += compr.numTris;

            // Part 2/2: Pack the bits into the binary object
            const UINT32 numVertices = (m_UseMotionBlur) ? 6 : 3;
            TriangleMode triRangeMode = TriangleMode::Compressed;
            if (m_UseMotionBlur)
            {
                triRangeMode =
                    (m_UseVisibilityMasks) ? TriangleMode::MotionBlurVM : TriangleMode::MotionBlur;
            }
            else
            {
                triRangeMode =
                    (m_UseVisibilityMasks) ? TriangleMode::CompressedVM : TriangleMode::Compressed;
            }

            static constexpr UINT32 headerOff = 992;
            static constexpr UINT32 idBaseOff = 960;
            static constexpr UINT32 vertexPosOff = 96;
            UINT32 vertexIdOff = 
                m_UseMotionBlur ?
                    idBaseOff - (compr.numTris * (numVertices * 4 + 2)) :
                    idBaseOff - 2 - ((compr.numTris - 1) * (numVertices * 4 + 2));
            if (m_UseVisibilityMasks)
            {
                // VMOffset.Base (43b) and VMInfo (7b)
                vertexIdOff -= 50;
            }
            const UINT32 vmInfoOff = (m_UseVisibilityMasks) ? idBaseOff - 7 : idBaseOff;
            const UINT32 vmOffsetBaseOff = (m_UseVisibilityMasks) ? vmInfoOff - 43 : idBaseOff;

            //
            // VertexBase - fixed offset
            //
            UINT32 offset = 0;
            bitField.SetBits(offset, 32, compr.baseX);
            offset += 32;
            bitField.SetBits(offset, 32, compr.baseY);
            offset += 32;
            bitField.SetBits(offset, 32, compr.baseZ);
            offset += 32;
            MASSERT(offset == vertexPosOff);

            //
            // VertexPositions - fixed offset
            //
            offset = vertexPosOff;
            for (const auto& comprVert : vertexPos)
            {
                MASSERT(comprVert.x <= CreateMask(compr.prec));
                MASSERT(comprVert.y <= CreateMask(compr.prec));
                MASSERT(comprVert.z <= CreateMask(compr.prec));
                bitField.SetBits(offset, compr.prec, comprVert.x);
                offset += compr.prec;
                bitField.SetBits(offset, compr.prec, comprVert.y);
                offset += compr.prec;
                bitField.SetBits(offset, compr.prec, comprVert.z);
                offset += compr.prec;
            }
            // vertexPosOff + sizeof(VertexPositionArray)
            MASSERT(offset == vertexPosOff + 3 * vertexPos.size() * compr.prec);

            //
            // Header - fixed offset
            //
            offset = headerOff;
            MASSERT(compr.prec    >= 1);
            MASSERT(compr.idPrec  >= 1);
            MASSERT(compr.numTris >= 1);
            MASSERT(compr.prec    <= (1u << 5));
            MASSERT(compr.idPrec  <= (1u << 5));
            MASSERT(compr.numTris <= (1u << 4));
            MASSERT(compr.shift   <= (1u << 5));
            bitField.SetBits(offset, 5, compr.prec - 1);
            offset += 5;
            bitField.SetBits(offset, 5, compr.prec - 1);
            offset += 5;
            bitField.SetBits(offset, 5, compr.prec - 1);
            offset += 5;
            bitField.SetBits(offset, 5, compr.idPrec - 1);
            offset += 5;
            bitField.SetBits(offset, 4, compr.numTris - 1);
            offset += 4;
            bitField.SetBits(offset, 5, compr.shift);
            offset += 5;
            bitField.SetBits(offset, 3, static_cast<UINT08>(triRangeMode));
            offset += 3;
            MASSERT(offset == headerOff + 32);

            //
            // Base triangle ID - fixed offset
            //
            bitField.SetBits(idBaseOff, 32, compr.baseTriId);

            // VMInfo and VMOffset.Base
            if (m_UseVisibilityMasks)
            {
                // VMInfo: 6b Precision.VMOffset, 1b vmAlignment
                MASSERT((s_PrecisiolwMOffset >> 6) == 0);
                const UINT08 vmInfo = (s_PrecisiolwMOffset << 1);
                bitField.SetBits(vmInfoOff, 7, vmInfo);

                // VMOffset.Base (43b) bits [48:6] of visibility mask offset for tri 0
                MASSERT((vBlockDeviceAddr >> 49) == 0);
                MASSERT((vBlockDeviceAddr & 0x3F) == 0);
                bitField.SetBits(vmOffsetBaseOff, 43, vBlockDeviceAddr >> 6);
            }

            //
            // VertexIDs - end of array has fixed offset
            // The IDs are serialized from greatest to least
            //
            offset = vmOffsetBaseOff;

            // Base triangle is implicit for non-motion-blur case
            if (!m_UseMotionBlur)
            {
                // Base triangle is opaque (alpha=0)
                offset -= 1;
                bitField.SetBits(offset, 0, 0x0);

                // Set 'Force NoLwll' for base triangle (ignore triangle direction)
                offset -= 1;
                bitField.SetBits(offset, 1, 0x1);
            }

            MASSERT(vertexIds.size() % numVertices == 0);
            for (UINT32 i = 0; i < vertexIds.size(); i += numVertices)
            {
                // Motion-blur triangles have the alpha/nolwll at the end
                // (Right after the vertices of the end triangle)
                if (m_UseMotionBlur)
                {
                    // Triangle is opaque (alpha=0)
                    offset -= 1;
                    bitField.SetBits(offset, 1, 0x0);

                    // Set 'Force NoLwll' for triangle (ignore triangle direction)
                    offset -= 1;
                    bitField.SetBits(offset, 1, 0x1);

                    // We can run out of vertices easily if the triangles have high compression
                    MASSERT(vertexIds[i + 5] <= CreateMask(4));
                    MASSERT(vertexIds[i + 4] <= CreateMask(4));
                    MASSERT(vertexIds[i + 3] <= CreateMask(4));
                    // Vertices are serialized from least to greatest
                    offset -= 4;
                    bitField.SetBits(offset, 4, vertexIds[i + 5]);
                    offset -= 4;
                    bitField.SetBits(offset, 4, vertexIds[i + 4]);
                    offset -= 4;
                    bitField.SetBits(offset, 4, vertexIds[i + 3]);
                }

                // We can run out of vertices easily if the triangles have high compression
                MASSERT(vertexIds[i + 2] <= CreateMask(4));
                MASSERT(vertexIds[i + 1] <= CreateMask(4));
                MASSERT(vertexIds[i + 0] <= CreateMask(4));

                // Vertices are serialized from least to greatest
                offset -= 4;
                bitField.SetBits(offset, 4, vertexIds[i + 2]);
                offset -= 4;
                bitField.SetBits(offset, 4, vertexIds[i + 1]);
                offset -= 4;
                bitField.SetBits(offset, 4, vertexIds[i + 0]);

                // Normal triangles have the alpha/nolwll at the beginning
                if (!m_UseMotionBlur)
                {
                    // Triangle is opaque (alpha=0)
                    offset -= 1;
                    bitField.SetBits(offset, 1, 0x0);

                    // Set 'Force NoLwll' for triangle (ignore triangle direction)
                    offset -= 1;
                    bitField.SetBits(offset, 1, 0x1);
                }
            }
            MASSERT(offset == vertexIdOff);

            //
            // TriangleIDs - variable offset (ends where VertexIDs begin)
            // The IDs are serialized from greatest to least
            //
            offset = vertexIdOff;
            for (const auto& comprId : triangleIds)
            {
                MASSERT(comprId <= CreateMask(compr.idPrec));
                offset -= compr.idPrec;
                bitField.SetBits(offset, compr.idPrec, comprId);
            }
            // vertexIdOff - sizeof(TriangleIdArray)
            MASSERT(offset == vertexIdOff - ((compr.numTris - 1) * compr.idPrec));

            // Visibility Masks - variable offset (ends where TriangleIDs begin)
            if (m_UseVisibilityMasks)
            {
                for (UINT32 i = 0; i < compr.numTris; i++)
                {
                    offset -= 4;
                    MASSERT((s_VMLevel >> 4) == 0);
                    bitField.SetBits(offset, 4, s_VMLevel);

                    // vmType (1 = 2-state VM)
                    offset -= 1;
                    bitField.SetBits(offset, 1, 0x1);

                    // vmAp (0 = absolute ptrs)
                    offset -= 1;
                    bitField.SetBits(offset, 1, 0x0);

                    // vmOffset (Tri 0 uses VMOffset.Base)
                    if (i != 0)
                    {
                        offset -= s_PrecisiolwMOffset;
                        const UINT32 vmOffset = i * TTU_CACHE_LINE_BYTES;
                        MASSERT((vmOffset >> s_PrecisiolwMOffset) == 0);
                        bitField.SetBits(offset, s_PrecisiolwMOffset, vmOffset);
                    }
                }

                // vertexIdOff - sizeof(TriangleIdArray) - sizeof(VisibilityMaskArray)
                MASSERT(offset == vertexIdOff -
                        ((compr.numTris - 1) * compr.idPrec) -
                        (compr.numTris * 6 + (compr.numTris - 1) * s_PrecisiolwMOffset));
            }

            // Reverse endianess (since BitField is big endian)
            std::reverse(bitField.begin(), bitField.end());

            // Copy data
            Platform::MemCopy(dst, bitField.GetData(), TTU_CACHE_LINE_BYTES);
            dst += TTU_CACHE_LINE_BYTES;
        }

        if (m_UseVisibilityMasks)
        {
            for (UINT32 i = 0; i < m_VisibilityBlocks.size(); i++)
            {
                Platform::MemCopy(vBlockDst + i * TTU_CACHE_LINE_BYTES,
                                  m_VisibilityBlocks[i].data(), TTU_CACHE_LINE_BYTES);
            }
        }
    }
    // Otherwise uncompressed
    else
    {
        MASSERT(!m_UseVisibilityMasks && m_VisibilityBlocks.size() == 0);

        for (UINT32 i = 0; i < m_Triangles.size(); i++)
        {
            const auto& tri = m_Triangles[i];
            const UINT32 triUsed = s_FirstTriIdx + i;
            const UINT32 line    = triUsed / s_MaxTrisPerUncomprBlock;
            const UINT32 triIdx  = triUsed % s_MaxTrisPerUncomprBlock;

            UINT32* triData = reinterpret_cast<UINT32*>(dst + TTU_CACHE_LINE_BYTES * line);
            Vertex* vertices = reinterpret_cast<Vertex*>(dst + TTU_CACHE_LINE_BYTES* line);

            // Triangle Block Initialization
            if (triIdx == 0)
            {
                // Zero Triangle Block and set NumTris
                // Mode  = 0x0 (uncompressed)
                // Alpha = 0x0 (no alpha triangles)
                memset(triData, 0x0, TTU_CACHE_LINE_BYTES);

                // Set Number of triangles in triangle block - 1
                const UINT32 trisRemaining = static_cast<UINT32>(m_Triangles.size() - i);
                const UINT32 numTris = trisRemaining > s_MaxTrisPerUncomprBlock ?
                                       s_MaxTrisPerUncomprBlock :
                                       trisRemaining;
                triData[31] |= (numTris - 1) << 20;
            }

            // Set Triangle ID
            triData[30 - triIdx] |= tri.triId;

            // Set 'Force NoLwll' for triangle (ignore triangle direction)
            triData[31] |= (0x1 << triIdx) << 17;

            // Set vertices
            vertices[triIdx * 3 + 0] = tri.v0;
            vertices[triIdx * 3 + 1] = tri.v1;
            vertices[triIdx * 3 + 2] = tri.v2;
        }
    }
}

//! \brief Write the triangle vertices out to JSON.
//!        Useful for visualizing the BVH.
RC TriangleRange::DumpToJson() const
{
    RC rc;

    JavaScriptPtr pJs;
    JsonItem jsi = JsonItem();
    jsi.SetTag("TriangleRange");

    // For TriangleRanges we are just going to trust that our serialization is correct,
    // no sense in decoding the raw binary.
    JsArray jsa(m_Triangles.size() * 10);
    for (UINT32 i = 0; i < m_Triangles.size(); i++)
    {
        const auto& tri = m_Triangles[i];
        pJs->ToJsval(tri.triId, &jsa[i * 10 + 0]);

        pJs->ToJsval(tri.v0.x, &jsa[i * 10 + 1]);
        pJs->ToJsval(tri.v0.y, &jsa[i * 10 + 2]);
        pJs->ToJsval(tri.v0.z, &jsa[i * 10 + 3]);

        pJs->ToJsval(tri.v1.x, &jsa[i * 10 + 4]);
        pJs->ToJsval(tri.v1.y, &jsa[i * 10 + 5]);
        pJs->ToJsval(tri.v1.z, &jsa[i * 10 + 6]);

        pJs->ToJsval(tri.v2.x, &jsa[i * 10 + 7]);
        pJs->ToJsval(tri.v2.y, &jsa[i * 10 + 8]);
        pJs->ToJsval(tri.v2.z, &jsa[i * 10 + 9]);
    }
    jsi.SetField("coords", &jsa);

    if (m_UseMotionBlur)
    {
        JsArray jsa(m_EndTriangles.size() * 10);
        for (UINT32 i = 0; i < m_EndTriangles.size(); i++)
        {
            const auto& endTri = m_EndTriangles[i];
            pJs->ToJsval(endTri.triId, &jsa[i * 10 + 0]);

            pJs->ToJsval(endTri.v0.x, &jsa[i * 10 + 1]);
            pJs->ToJsval(endTri.v0.y, &jsa[i * 10 + 2]);
            pJs->ToJsval(endTri.v0.z, &jsa[i * 10 + 3]);

            pJs->ToJsval(endTri.v1.x, &jsa[i * 10 + 4]);
            pJs->ToJsval(endTri.v1.y, &jsa[i * 10 + 5]);
            pJs->ToJsval(endTri.v1.z, &jsa[i * 10 + 6]);

            pJs->ToJsval(endTri.v2.x, &jsa[i * 10 + 7]);
            pJs->ToJsval(endTri.v2.y, &jsa[i * 10 + 8]);
            pJs->ToJsval(endTri.v2.z, &jsa[i * 10 + 9]);
        }
        jsi.SetField("coords_end", &jsa);
    }
    CHECK_RC(jsi.WriteToLogfile());

    return rc;
}

UINT32 DisplacedMicromesh::SizeBytes() const
{
    // Does not include the size of visibility blocks (if present) as these
    // are serialized separately (See VisibilityBlocksSizeBytes).
    // Displaced Micromesh Triangle Block + Displacement Blocks
    return TTU_CACHE_LINE_BYTES +
           (DisplacedMicromesh::s_SubTrisPerBaseTri * DisplacedMicromesh::s_DispBlockSizeBytes);
}

BoundingBox DisplacedMicromesh::GenerateAABB() const
{
    float xmin = std::numeric_limits<float>::infinity();
    float ymin = xmin;
    float zmin = ymin;

    float xmax = -std::numeric_limits<float>::infinity();
    float ymax = xmax;
    float zmax = ymax;

    xmin = min(xmin, m_BaseTriangle.v0.x);
    ymin = min(ymin, m_BaseTriangle.v0.y);
    zmin = min(zmin, m_BaseTriangle.v0.z);
    xmax = max(xmax, m_BaseTriangle.v0.x);
    ymax = max(ymax, m_BaseTriangle.v0.y);
    zmax = max(zmax, m_BaseTriangle.v0.z);

    xmin = min(xmin, m_BaseTriangle.v1.x);
    ymin = min(ymin, m_BaseTriangle.v1.y);
    zmin = min(zmin, m_BaseTriangle.v1.z);
    xmax = max(xmax, m_BaseTriangle.v1.x);
    ymax = max(ymax, m_BaseTriangle.v1.y);
    zmax = max(zmax, m_BaseTriangle.v1.z);

    xmin = min(xmin, m_BaseTriangle.v2.x);
    ymin = min(ymin, m_BaseTriangle.v2.y);
    zmin = min(zmin, m_BaseTriangle.v2.z);
    xmax = max(xmax, m_BaseTriangle.v2.x);
    ymax = max(ymax, m_BaseTriangle.v2.y);
    zmax = max(zmax, m_BaseTriangle.v2.z);

    return {xmin, ymin, zmin, xmax, ymax, zmax};
}

// See https://confluence.lwpu.com/display/ADASMTTU/FD+Ada-21+Displaced+Micro-meshes
void DisplacedMicromesh::Serialize(UINT08* dst, UINT64 meshDeviceAddr,
                                   UINT08* vBlockDst, UINT64 vBlockDeviceAddr) const
{
    Bitfield<UINT32, TTU_CACHE_LINE_BYTES * 8> bitField;
    UINT32 offset = 0;

    // Set base tri vertex positions
    bitField.SetBits(offset, 32, reinterpret_cast<const UINT32*>(&m_BaseTriangle.v0.x)[0]);
    offset += 32;
    bitField.SetBits(offset, 32, reinterpret_cast<const UINT32*>(&m_BaseTriangle.v0.y)[0]);
    offset += 32;
    bitField.SetBits(offset, 32, reinterpret_cast<const UINT32*>(&m_BaseTriangle.v0.z)[0]);
    offset += 32;

    bitField.SetBits(offset, 32, reinterpret_cast<const UINT32*>(&m_BaseTriangle.v1.x)[0]);
    offset += 32;
    bitField.SetBits(offset, 32, reinterpret_cast<const UINT32*>(&m_BaseTriangle.v1.y)[0]);
    offset += 32;
    bitField.SetBits(offset, 32, reinterpret_cast<const UINT32*>(&m_BaseTriangle.v1.z)[0]);
    offset += 32;

    bitField.SetBits(offset, 32, reinterpret_cast<const UINT32*>(&m_BaseTriangle.v2.x)[0]);
    offset += 32;
    bitField.SetBits(offset, 32, reinterpret_cast<const UINT32*>(&m_BaseTriangle.v2.y)[0]);
    offset += 32;
    bitField.SetBits(offset, 32, reinterpret_cast<const UINT32*>(&m_BaseTriangle.v2.z)[0]);
    offset += 32;

    // Set base tri vertex displacement directions
    bitField.SetBits(offset, 16, Utility::Float32ToFloat16(m_DisplacementDir0.x));
    offset += 16;
    bitField.SetBits(offset, 16, Utility::Float32ToFloat16(m_DisplacementDir0.y));
    offset += 16;
    bitField.SetBits(offset, 16, Utility::Float32ToFloat16(m_DisplacementDir0.z));
    offset += 16;

    bitField.SetBits(offset, 16, Utility::Float32ToFloat16(m_DisplacementDir1.x));
    offset += 16;
    bitField.SetBits(offset, 16, Utility::Float32ToFloat16(m_DisplacementDir1.y));
    offset += 16;
    bitField.SetBits(offset, 16, Utility::Float32ToFloat16(m_DisplacementDir1.z));
    offset += 16;

    bitField.SetBits(offset, 16, Utility::Float32ToFloat16(m_DisplacementDir2.x));
    offset += 16;
    bitField.SetBits(offset, 16, Utility::Float32ToFloat16(m_DisplacementDir2.y));
    offset += 16;
    bitField.SetBits(offset, 16, Utility::Float32ToFloat16(m_DisplacementDir2.z));
    offset += 16;

    // reserved
    offset += 128;

    // Set subTriArray (Lwrrently only one subtri is supported)
    MASSERT(s_SubTrisPerBaseTri == 1);
    offset += (63 * 3); // skip subtris 63 through 1

    // Set subSize.0
    MASSERT(s_DispBlockResolution == 1024);
    bitField.SetBits(offset, 2, 2); // 2 = 32x32 (1024 utris)
    offset += 2;

    // Set subRes.0
    MASSERT(s_DispBlockSizeBytes == 128);
    bitField.SetBits(offset, 1, 1); // 1 = 128B
    offset += 1;

    // Set displacement scale
    bitField.SetBits(offset, 32, reinterpret_cast<const UINT32*>(&m_DisplacementScale)[0]);
    offset += 32;

    // reserved
    offset += 32;

    // Skip utriStart (already 0) and DMOffset64 (not used)
    offset += (20 + 33);

    // Set DMOffset128
    const UINT64 displacementBlockAddr = meshDeviceAddr + TTU_CACHE_LINE_BYTES;
    MASSERT((displacementBlockAddr >> 49) == 0); // max 49 bits
    MASSERT((displacementBlockAddr & 0x7F) == 0); // should be 128b aligned
    const UINT64 dispBlockAddrAligned = displacementBlockAddr >> 7;
    bitField.SetBits(offset, 42, dispBlockAddrAligned);
    offset += 42;

    // Set VMOffset (if needed)
    if (m_UseVisibilityMasks)
    {
        // VMOffset (52b) = cacheline addr (42b) + bit index within cacheline (10b)
        MASSERT((vBlockDeviceAddr >> 42) == 0);
        // Each mesh's vBlock is exactly 1 cacheline, so the bit index is 0
        bitField.SetBits(offset, 52, vBlockDeviceAddr << 10);
    }
    offset += 52;

    // Set triangle ID
    bitField.SetBits(offset, 32, m_BaseTriangle.triId);
    offset += 32;

    // reserved
    offset += 4;

    // Set DMM Header
    // Skip edgeDecimation and dmAp (not used)
    offset += 4;

    // Set baseResolution
    MASSERT((s_BaseResolution >> 4) == 0); // max 4 bits
    bitField.SetBits(offset, 4, s_BaseResolution);
    offset += 4;

    // Skip vmAper (already 0 = absolute ptrs)
    offset += 1;

    // Set vmType (if needed)
    if (m_UseVisibilityMasks)
    {
        bitField.SetBits(offset, 1, 0x1); // 1 = 2-state VM
    }
    offset += 1;

    // Set vmLevel
    const UINT08 vmLevel = (m_UseVisibilityMasks) ? s_VMLevel : 15; // 15 = no VM
    MASSERT((vmLevel >> 4) == 0);
    bitField.SetBits(offset, 4, vmLevel);
    offset += 4;

    // Skip alpha (already 0)
    offset += 1;

    // Set fnc
    bitField.SetBits(offset, 1, 1); // 1 = force-no-lwll
    offset += 1;

    // Skip NumTris (already 0)
    MASSERT((s_SubTrisPerBaseTri - 1) == 0);
    offset += 6;

    // Set Mode
    bitField.SetBits(offset, 3, static_cast<UINT08>(TriangleMode::Micromesh));
    offset += 3;

    MASSERT(offset == TTU_CACHE_LINE_BYTES * 8);

    // Reverse endianess (since BitField is big endian)
    std::reverse(bitField.begin(), bitField.end());

    // Copy Displaced Micromesh Triangle Block
    Platform::MemCopy(dst, bitField.GetData(), TTU_CACHE_LINE_BYTES);

    // Create displacement block
    Platform::MemCopy(dst + TTU_CACHE_LINE_BYTES, m_DisplacementData.data(), s_DispBlockSizeBytes);

    if (m_UseVisibilityMasks)
    {
        Platform::MemCopy(vBlockDst, m_VisibilityBlock.data(), TTU_CACHE_LINE_BYTES);
    }
}

RC DisplacedMicromesh::DumpToJson() const
{
    RC rc;
    JavaScriptPtr pJs;
    JsonItem jsi = JsonItem();
    jsi.SetTag("DisplacedMicromesh");

    // Lwrrently this only dumps the base triangle.
    JsArray jsa(10);
    pJs->ToJsval(m_BaseTriangle.triId, &jsa[0]);

    pJs->ToJsval(m_BaseTriangle.v0.x, &jsa[1]);
    pJs->ToJsval(m_BaseTriangle.v0.y, &jsa[2]);
    pJs->ToJsval(m_BaseTriangle.v0.z, &jsa[3]);

    pJs->ToJsval(m_BaseTriangle.v1.x, &jsa[4]);
    pJs->ToJsval(m_BaseTriangle.v1.y, &jsa[5]);
    pJs->ToJsval(m_BaseTriangle.v1.z, &jsa[6]);

    pJs->ToJsval(m_BaseTriangle.v2.x, &jsa[7]);
    pJs->ToJsval(m_BaseTriangle.v2.y, &jsa[8]);
    pJs->ToJsval(m_BaseTriangle.v2.z, &jsa[9]);

    jsi.SetField("coords", &jsa);
    CHECK_RC(jsi.WriteToLogfile());
    return rc;
}

UINT32 Complet::GetMaxChildren() const
{
    UINT32 maxChildren = 0;
    switch (m_Format)
    {
        case ChildFormat::Standard:
            maxChildren = (m_ComputeCap >= Lwca::Capability::SM_86) ? 11 : 10;
            break;
        case ChildFormat::MotionBlur:
            maxChildren = (m_ComputeCap >= Lwca::Capability::SM_86) ? 5 : 0;
            break;
        default:
            break;
    }
    return maxChildren;
}

RC Complet::CheckChildCapacity() const
{
    RC rc;
    const UINT32 newChildren = static_cast<UINT32>(
        m_TriRanges.size() + m_Micromeshes.size() + m_ChildComplets.size()) + 1;

    const UINT32 maxChildren = GetMaxChildren();
    MASSERT(maxChildren);
    MASSERT(maxChildren <= Complet::s_MaxChildren);
    if (!maxChildren)
    {
        Printf(Tee::PriError,
               "Unsupported child format %d\n", static_cast<UINT32>(m_Format));
        return RC::SOFTWARE_ERROR;
    }
    if (newChildren > maxChildren)
    {
        Printf(Tee::PriError, "Too many children (%d/%d)\n",
               newChildren, maxChildren);
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC Complet::AddChildComplet(Complet* pChildComplet)
{
    RC rc;
    MASSERT(pChildComplet);
    CHECK_RC(CheckChildCapacity());

    m_ChildComplets.push_back(pChildComplet);
    return rc;
}

RC Complet::AddChildTriRange(TriangleRange* pTriRange)
{
    RC rc;
    MASSERT(pTriRange);
    CHECK_RC(CheckChildCapacity());

    if (m_LeafType != LeafType::TriRange)
    {
        Printf(Tee::PriError,
               "Complet is not of type TriangleRange. Cannot add TriangleRange as child!\n");
        return RC::SOFTWARE_ERROR;
    }

    m_TriRanges.push_back(pTriRange);
    return rc;
}

RC Complet::AddChildMicromesh(DisplacedMicromesh* pMesh)
{
    RC rc;
    MASSERT(pMesh);
    CHECK_RC(CheckChildCapacity());

    if (m_LeafType != LeafType::DisplacedMicromesh)
    {
        Printf(Tee::PriError, "Complet does not have leaftype DisplacedMicromesh. "
               "Cannot add DisplacedMicromesh as child!\n");
        return RC::SOFTWARE_ERROR;
    }

    m_Micromeshes.push_back(pMesh);
    return rc;
}

//! \brief Relwrsively compute the required memory in bytes for this complet and all children
//!
//! The size includes leaf nodes
//! Function should be kept in sync with the SerializeBVHAbs function
//!
UINT64 Complet::BVHSizeBytes() const
{
    UINT64 total = TTU_CACHE_LINE_BYTES;
    for (auto m_TriRange : m_TriRanges)
    {
        total += m_TriRange->SizeBytes();
        total += m_TriRange->VisibilityBlocksSizeBytes();
    }
    for (auto m_Mesh : m_Micromeshes)
    {
        total += m_Mesh->SizeBytes();
        total += m_Mesh->VisibilityBlocksSizeBytes();
    }
    for (auto m_Child : m_ChildComplets)
    {
        total += m_Child->BVHSizeBytes();
    }
    return total;
}

//! \brief Serialize the entire BVH starting at the root node
//!
//! For each complet:
//! (1) The absolute pointers are computed using the provided rootPtr as a base
//! (2) The complet data is serialized and stored locally
//! (3) This data (now fully serialized) is then copied to dst
//!
//! It is the user's resonsiblity to copy the memory from host to device memory
//!
//! \param rootPtr : LWCA Device Pointer of the root node
//! \param dst     : Host memory pointer of the root node
//!
UINT64 Complet::SerializeBVHAbs(UINT64 const rootPtr, UINT08* const dst)
{
    MASSERT(m_IsRoot);
    return SerializeBVHAbs(rootPtr, dst, 0, 0);
}

//! This relwrsive function is private so it can't be called by users
//! \param rootPtr       : LWCA Device Pointer of the root node
//! \param dst           : Host memory pointer of the root node
//! \param completOffset : Offset of current Complet (relative to dst)
//! \param offset        : Offset of next unwritten space (relative to dst)
UINT64 Complet::SerializeBVHAbs
(
    UINT64 const rootPtr,
    UINT08* const dst,
    UINT64 const completOffset,
    UINT64 offset
)
{
    // Lwrrently this function only produces absolute child/parent
    // complet pointers (lptr = 0)
    MASSERT(!s_UseRelCompletPtr);

    // Base case
    if (m_IsRoot)
    {
        MASSERT(offset == 0);
        MASSERT(completOffset == 0);
        set_parentPtr(0);
        set_parentLeafIdx(0xF);
        offset += TTU_CACHE_LINE_BYTES;
    }

    // Reserve space for visibility blocks which are serialized seperately from their respective
    // leaves because TriRanges need to be within s_MaxTriBlocks of each other
    const UINT64 vBlockOffset = offset;
    UINT64 lwrVBlockOffset = vBlockOffset;
    for (TriangleRange* pTriRange : m_TriRanges)
    {
        offset += pTriRange->VisibilityBlocksSizeBytes();
    }
    for (DisplacedMicromesh* pMesh : m_Micromeshes)
    {
        offset += pMesh->VisibilityBlocksSizeBytes();
    }

    // Serialize leaves
    const UINT64 leafOffset = offset;
    switch (m_LeafType)
    {
        case LeafType::TriRange:
        {
            for (TriangleRange* pTriRange : m_TriRanges)
            {
                // TODO Add support for Triangle Ranges that are used by multiple complets
                pTriRange->Serialize(dst + offset, dst + lwrVBlockOffset,
                                     rootPtr + lwrVBlockOffset);
                offset += pTriRange->SizeBytes();
                lwrVBlockOffset += pTriRange->VisibilityBlocksSizeBytes();
            }
            break;
        }
        
        case LeafType::DisplacedMicromesh:
        {
            for (DisplacedMicromesh* pMesh : m_Micromeshes)
            {
                pMesh->Serialize(dst + offset, rootPtr + offset, dst + lwrVBlockOffset,
                                 rootPtr + lwrVBlockOffset);
                offset += pMesh->SizeBytes();
                lwrVBlockOffset += pMesh->VisibilityBlocksSizeBytes();
            }
            break;
        }

        default:
            MASSERT(!"Complet::SerializeBVHAbs does not support this leaf type.");
            break;
    }
    MASSERT(lwrVBlockOffset == leafOffset);

    // Update offset to make room for children
    const UINT64 childOffset = offset;
    offset += TTU_CACHE_LINE_BYTES * m_ChildComplets.size();

    // Serialize this complet
    const UINT64 leafPtr = (m_TriRanges.size() + m_Micromeshes.size() > 0) ?
                           rootPtr + leafOffset : 0x0;
    set_leafPtr((s_UseRelLeafPtr && leafPtr != 0x0) ? leafOffset - completOffset : leafPtr);
    set_shearData();
    set_childPtr(m_ChildComplets.size() ? rootPtr + childOffset : 0x0);
    Serialize(dst + completOffset);

    // Relwrse on children
    for (UINT32 i = 0; i < m_ChildComplets.size(); i++)
    {
        auto pChild = m_ChildComplets[i];
        pChild->set_parentPtr(rootPtr + completOffset);
        const UINT32 completIdx = (m_Format == ChildFormat::MotionBlur) ? i * 2 : i;
        pChild->set_parentLeafIdx(static_cast<UINT08>(m_TriRanges.size() +
                                  m_Micromeshes.size()) + completIdx);
        offset = pChild->SerializeBVHAbs(rootPtr, dst, childOffset + i * TTU_CACHE_LINE_BYTES,
                                         offset);
    }
    return offset;
}

void Complet::Serialize(UINT08* dst)
{
    // mode
    set_leafType(static_cast<UINT08>(m_LeafType));
    set_relPtr(s_UseRelLeafPtr);
    set_lptr(!s_UseRelCompletPtr);
    set_format(static_cast<UINT08>(m_Format));

    // complet dimensions
    set_complet_bbox(m_CompletAABB);

    // All children not being used must be set to invalid
    set_child_ilwalid(0);
    set_child_ilwalid(1);
    set_child_ilwalid(2);
    set_child_ilwalid(3);
    set_child_ilwalid(4);
    set_child_ilwalid(5);
    set_child_ilwalid(6);
    set_child_ilwalid(7);
    set_child_ilwalid(8);
    set_child_ilwalid(9);
    set_child_ilwalid(10);

    Child* pChild = reinterpret_cast<Child*>(&m_CompletData[32]);

    // Only 1 leaf type allowed 
    MASSERT(m_TriRanges.size() == 0 || m_Micromeshes.size() == 0);

    for (auto pTriRange : m_TriRanges)
    {
        // Clear child entry
        *pChild = {};

        // Set number of lines
        pChild->data &= 0b11111000;
        pChild->data |= pTriRange->NumLines();

        // Set triIdx
        pChild->data &= 0b10000111;
        pChild->data |= pTriRange->TriIdx() << 3;

        // Set alpha to false
        pChild->data &= 0b01111111;

        // Set child bounding box depending on the triangles
        pChild->SetAABB(m_CompletAABB, pTriRange->GenerateAABB(false));
        pChild++;

        // If using motion blur, set the bounding box of the end triangles
        if (m_Format == ChildFormat::MotionBlur)
        {
            // Clear second half of motion child
            *pChild = {};

            // Set the ending bounding box (t=1.0)
            // Data and rval are unused by the second half
            pChild->SetAABB(m_CompletAABB, pTriRange->GenerateAABB(true));
            pChild++;
        }
    }

    bool isFirstLeaf = true;
    for (auto pMesh : m_Micromeshes)
    {
        // Clear child entry
        *pChild = {};

        // Set alpha to false
        pChild->data = 0b01000000;

        if (isFirstLeaf)
        {
            // subTriIdx already set to 0
            isFirstLeaf = false;
        }
        else
        {
            // set nextLine
            pChild->data |= 0b00100000;

            // set lineStride
            const UINT08 lineStride = (pMesh->SizeBytes() / TTU_CACHE_LINE_BYTES) - 1;
            pChild->data |= (lineStride & 0x1F);
        }

        // Set bounding box
        pChild->SetAABB(m_CompletAABB, pMesh->GenerateAABB());
        pChild++;
    }

    for (auto pComplet : m_ChildComplets)
    {
        // Clear child entry
        *pChild = {};

        // Child complet entries only have a bounding box
        pChild->SetAABB(m_CompletAABB, pComplet->GetAABB());
        pChild++;

        // If using motion blur, set the bounding box of the end child complet
        if (m_Format == ChildFormat::MotionBlur)
        {
            // Clear second half of motion child
            *pChild = {};

            // TODO for now do not utilize child complet motion blur
            pChild->SetAABB(m_CompletAABB, pComplet->GetAABB());
            pChild++;
        }
    }

    Platform::MemCopy(dst, m_CompletData.data(), m_CompletData.size());
}

//! \brief Write the complet and children BoundingBox info out to JSON.
//!        Useful for visualizing the BVH.
RC Complet::DumpToJson() const
{
    RC rc;

    JavaScriptPtr pJs;
    JsonItem jsi = JsonItem();
    jsi.SetTag("Complet");

    // For Complets we will decode from the raw binary.
    // This way we can double-check our encoding when visualizing the complet.
    float xmin = reinterpret_cast<const float*>(&m_CompletData[4])[0];
    float ymin = reinterpret_cast<const float*>(&m_CompletData[4])[1];
    float zmin = reinterpret_cast<const float*>(&m_CompletData[4])[2];

    JsArray jsmin(3);
    pJs->ToJsval(xmin, &jsmin[0]);
    pJs->ToJsval(ymin, &jsmin[1]);
    pJs->ToJsval(zmin, &jsmin[2]);
    jsi.SetField("min", &jsmin);

    JsArray jsscl(3);
    UINT08 xscl = m_CompletData[1];
    UINT08 yscl = m_CompletData[2];
    UINT08 zscl = m_CompletData[3];
    pJs->ToJsval(xscl, &jsscl[0]);
    pJs->ToJsval(yscl, &jsscl[1]);
    pJs->ToJsval(zscl, &jsscl[2]);
    jsi.SetField("scl", &jsscl);

    const Child* pChild = reinterpret_cast<const Child*>(&m_CompletData[32]);
    const UINT32 maxChildren = GetMaxChildren();
    MASSERT(maxChildren);
    MASSERT(maxChildren <= Complet::s_MaxChildren);
    for (UINT32 childIdx = 0; childIdx < maxChildren; childIdx++)
    {

        // Skip invalid/non-existing children
        if (pChild->zlo == 0xFF && pChild->zhi == 0x00)
        {
            continue;
        }
        {
            JsArray jsaabb(6);
            pJs->ToJsval(pChild->xlo, &jsaabb[0]);
            pJs->ToJsval(pChild->xhi, &jsaabb[1]);
            pJs->ToJsval(pChild->ylo, &jsaabb[2]);
            pJs->ToJsval(pChild->yhi, &jsaabb[3]);
            pJs->ToJsval(pChild->zlo, &jsaabb[4]);
            pJs->ToJsval(pChild->zhi, &jsaabb[5]);
            string str = "childAABB" + Utility::StrPrintf("%u", childIdx);
            jsi.SetField(str.c_str(), &jsaabb);
            pChild++;
        }
        if (m_Format == ChildFormat::MotionBlur)
        {
            JsArray jsaabb(6);
            pJs->ToJsval(pChild->xlo, &jsaabb[0]);
            pJs->ToJsval(pChild->xhi, &jsaabb[1]);
            pJs->ToJsval(pChild->ylo, &jsaabb[2]);
            pJs->ToJsval(pChild->yhi, &jsaabb[3]);
            pJs->ToJsval(pChild->zlo, &jsaabb[4]);
            pJs->ToJsval(pChild->zhi, &jsaabb[5]);
            string str = "childAABB" + Utility::StrPrintf("%u", childIdx) + "_end";
            jsi.SetField(str.c_str(), &jsaabb);
            pChild++;
        }
    }

    CHECK_RC(jsi.WriteToLogfile());
    return rc;
}

void Complet::set_xscl(UINT08 xscl)
{
    m_CompletData[1] = xscl;
    return;
}

void Complet::set_yscl(UINT08 yscl)
{
    m_CompletData[2] = yscl;
    return;
}

void Complet::set_zscl(UINT08 zscl)
{
    m_CompletData[3] = zscl;
    return;
}

void Complet::set_xmin(float xmin)
{
    reinterpret_cast<float*>(&m_CompletData[4])[0] = xmin;
    return;
}

void Complet::set_ymin(float ymin)
{
    reinterpret_cast<float*>(&m_CompletData[4])[1] = ymin;
    return;
}

void Complet::set_zmin(float zmin)
{
    reinterpret_cast<float*>(&m_CompletData[4])[2] = zmin;
    return;
}

void Complet::set_complet_bbox(const BoundingBox& bbox)
{
    // Set scl
    UINT08 xscl = 0;
    UINT08 yscl = 0;
    UINT08 zscl = 0;
    bbox.ToScl(&xscl, &yscl, &zscl);

    set_xscl(xscl);
    set_yscl(yscl);
    set_zscl(zscl);

    // Set xyz min
    if (m_LeafType == LeafType::DisplacedMicromesh)
    {
        // When using displaced micromeshes (leaftype == 3) the LSB of each of the xyz min
        // fields are FP31 (se8m22) with the LSBs becoming shearExpOffset (3 bits)
        const UINT32 xminFP31 = reinterpret_cast<const UINT32*>(&bbox.xmin)[0] & ~1; // clear LSB
        const UINT32 yminFP31 = reinterpret_cast<const UINT32*>(&bbox.ymin)[0] & ~1;
        const UINT32 zminFP31 = reinterpret_cast<const UINT32*>(&bbox.zmin)[0] & ~1;
        set_xmin(reinterpret_cast<const float*>(&xminFP31)[0]);
        set_ymin(reinterpret_cast<const float*>(&yminFP31)[0]);
        set_zmin(reinterpret_cast<const float*>(&zminFP31)[0]);
    }
    else
    {
        set_xmin(bbox.xmin);
        set_ymin(bbox.ymin);
        set_zmin(bbox.zmin);
    }

    return;
}

// leafType = 0 => itemBaseLo
void Complet::set_itemBaseLo(UINT32 itemBaseLo)
{
    reinterpret_cast<UINT32*>(&m_CompletData[16])[0] = itemBaseLo;
    return;
}
void Complet::set_itemBaseHi(UINT32 itemBaseHi)
{
    UINT32* pMisc = reinterpret_cast<UINT32*>(&m_CompletData[20]);
    pMisc[0] = pMisc[0] & 0xFFFF8000;
    pMisc[0] |= (itemBaseHi & 0x7FFF);
    return;
}

void Complet::set_leafPtr(UINT64 leafPtr)
{
    if (m_ShearData.enabled)
    {
        // Sheared complets reduce the addressable space for leaf pointers:
        // https://confluence.lwpu.com/display/ADASMTTU/FD+Ada-28+Sheared+AABB+Complets
        // Lwrrently only leafType 1 (trirange) and 3 (micromesh) are supported as these
        // are the only leaf types we generate.
        MASSERT(m_LeafType == LeafType::TriRange ||
                m_LeafType == LeafType::DisplacedMicromesh);
        MASSERT((leafPtr >> 44) == 0);             // leafPtr should be 44 bits max
        MASSERT((leafPtr & 0x7F) == 0);            // bits [6:0] implied zero

        // (leafType = 1/3, shear = 1) => bits [38:7] of absolute/relative pointer to cacheline
        //                              containing first triangle of compressed triangle range
        //                              in first leaf, bits [6:0] implied zero (128-byte align)
        UINT32 leafPtrLo = static_cast<UINT32>(leafPtr >> 7);
        reinterpret_cast<UINT32*>(&m_CompletData[16])[0] = leafPtrLo;

        // (leafType = 1/3) => bits[43:39] of absolute/relative pointer to cacheline containing
        //                   first triangle of compressed triangle range in first leaf
        UINT32 leafPtrShortHi = static_cast<UINT32>((leafPtr >> 39) & 0x1F);
        UINT32* pMisc = reinterpret_cast<UINT32*>(&m_CompletData[20]);
        pMisc[0] &= (0xFFFFFFE0);
        pMisc[0] |= leafPtrShortHi;
    }
    else
    {
        // leafType = 0 => itemBaseLo
        // leafType = 1 => bits [31:0] of absolute/relative pointer to cacheline containing
        //                 first triangle of compressed triangle range in first leaf,
        //                 bits [6:0] must be zero (128-byte align)
        // leafType = 2 => bits [31:0] of absolute/relative pointer to cacheline containing
        //                 first instance node transform, bits [5:0] must be zero (64-byte align)
        UINT32 leafPtrLo = static_cast<UINT32>(leafPtr & 0xFFFFFFFF);
        reinterpret_cast<UINT32*>(&m_CompletData[16])[0] = leafPtrLo;

        UINT32 leafPtrHi = static_cast<UINT32>(leafPtr >> 32);
        UINT32* pMisc = reinterpret_cast<UINT32*>(&m_CompletData[20]);
        pMisc[0] &= (0xFFFE0000);
        pMisc[0] |= (leafPtrHi & 0x1FFFF);
    }
    return;
}

void Complet::set_shearData()
{
    UINT32* pMisc = reinterpret_cast<UINT32*>(&m_CompletData[20]);
    pMisc[0] &= (0xFFFDFFFF); // clear bit 17 (shear enable)

    if (m_ShearData.enabled)
    {
        pMisc[0] |= (1 << 17);
        pMisc[0] &= (0xFFFE001F); // clear bits [16:5]
        pMisc[0] |= ((static_cast<UINT08>(m_ShearData.select) & 0xF) << 13);
        pMisc[0] |= ((static_cast<UINT08>(m_ShearData.coeff0) & 0xF) << 9);
        pMisc[0] |= ((static_cast<UINT08>(m_ShearData.coeff1) & 0xF) << 5);
    }
    return;
}

//    (lptr = 0) parentOfs bits [38:7] of signed relative pointer to parent,
//               bits [6:0] implicitly zero
void Complet::set_parentOfs(UINT32 parentOfs)
{
    reinterpret_cast<UINT32*>(&m_CompletData[24])[0] = parentOfs;
    return;
}

// (lptr = 1) parentPtrLo bits [31:0] of absolute pointer to parent,
//            bits [6:0] must be zero
//
//            parentPtrHi bits [48:32] of absolute pointer to parent
void Complet::set_parentPtr(UINT64 parentPtr)
{
    // Set parentPtrLo
    UINT32 parentPtrLo = static_cast<UINT32>(parentPtr & 0xFFFFFFFF);
    reinterpret_cast<UINT32*>(&m_CompletData[24])[0] = parentPtrLo;

    // Set parentPtrHi
    UINT32 parentPtrHi = static_cast<UINT32>(parentPtr >> 32);
    UINT64* longPtrData = reinterpret_cast<UINT64*>(&m_CompletData[120]);
    longPtrData[0] &= (0xFFFFFFFFFFFE0000);
    longPtrData[0] |= (parentPtrHi & 0x1FFFF);
    return;
}

// (lptr = 0) childOfs bits [38:7] of signed relative pointer to first child complet,
//            bits [6:0] implicitly zero
void Complet::set_childOfs(UINT32 childOfs)
{
    reinterpret_cast<UINT32*>(&m_CompletData[28])[0] = childOfs;
    return;
}

// (lptr = 1) childPtrLo bits [31:0] of absolute pointer to first child complet,
//            bits [6:0] must be zero
//
//            childPtrHi bits [48:32] of absolute pointer to parent
void Complet::set_childPtr(UINT64 childPtr)
{
    UINT32 childPtrLo = static_cast<UINT32>(childPtr & 0xFFFFFFFF);
    reinterpret_cast<UINT32*>(&m_CompletData[28])[0] = childPtrLo;

    UINT32 childPtrHi = static_cast<UINT32>(childPtr >> 32);
    UINT64* longPtrData = reinterpret_cast<UINT64*>(&m_CompletData[120]);
    longPtrData[0] &= 0xFFFE0000FFFFFFFF;
    longPtrData[0] |= (static_cast<UINT64>(childPtrHi) & 0x1FFFF) << 32;
    return;
}

void Complet::set_leafType(UINT08 leafType)
{
    m_CompletData[0] = m_CompletData[0] & 0x3F;
    m_CompletData[0] |= (leafType & 0x03) << 6;
    return;
}

void Complet::set_relPtr(UINT08 relPtr)
{
    m_CompletData[0] = m_CompletData[0] & 0xDF;
    m_CompletData[0] |= (relPtr & 0x01) << 5;
    return;
}

void Complet::set_lptr(UINT08 lptr)
{
    m_CompletData[0] = m_CompletData[0] & 0xEF;
    m_CompletData[0] |= (lptr & 0x01) << 4;
    return;
}

void Complet::set_format(UINT08 format)
{
    m_CompletData[0] = m_CompletData[0] & 0xF0;
    m_CompletData[0] |= format & 0x0F;
    return;
}

void Complet::set_parentLeafIdx(UINT08 parentLeafIdx)
{
    UINT32* misc = reinterpret_cast<UINT32*>(&m_CompletData[20]);
    misc[0] = misc[0] & 0xFFFFFFF;
    misc[0] |= (parentLeafIdx & 0x0F) << 28;
    return;
}

void Complet::set_firstTriIdx(UINT32 firstTriIdx)
{
    UINT32* misc = reinterpret_cast<UINT32*>(&m_CompletData[20]);
    misc[0] = misc[0] & 0xF0FFFFFF;
    misc[0] |= (firstTriIdx & 0x0F) << 24;
    return;
}

void Complet::set_child(UINT32 childIdx, UINT64 child)
{
    UINT64* children = reinterpret_cast<UINT64*>(&m_CompletData[32]);
    children[childIdx] = child;
    return;
}

void Complet::set_child_data(UINT32 childIdx, UINT08 data)
{
    UINT64* children = reinterpret_cast<UINT64*>(&m_CompletData[32]);
    UINT08* child = reinterpret_cast<UINT08*>(&children[childIdx]);
    child[7] = data;
    return;
}

void Complet::set_child_rval(UINT32 childIdx, UINT08 rval)
{
    UINT64* children = reinterpret_cast<UINT64*>(&m_CompletData[32]);
    UINT08* child = reinterpret_cast<UINT08*>(&children[childIdx]);
    child[6] = rval;
    return;
}

void Complet::set_child_ilwalid(UINT32 childIdx)
{
    UINT64* children = reinterpret_cast<UINT64*>(&m_CompletData[32]);
    UINT08* child = reinterpret_cast<UINT08*>(&children[childIdx]);
    child[5] = 0x00;
    child[4] = 0xFF;
    child[3] = 0x00;
    child[2] = 0xFF;
    child[1] = 0x00;
    child[0] = 0xFF;
    return;
}

void Complet::set_child_bbox
(
    UINT32 childIdx,
    UINT08 zhi, UINT08 zlo,
    UINT08 yhi, UINT08 ylo,
    UINT08 xhi, UINT08 xlo
)
{
    UINT64* children = reinterpret_cast<UINT64*>(&m_CompletData[32]);
    UINT08* child = reinterpret_cast<UINT08*>(&children[childIdx]);
    child[5] = zhi;
    child[4] = zlo;
    child[3] = yhi;
    child[2] = ylo;
    child[1] = xhi;
    child[0] = xlo;
    return;
}

// Factory function that makes use of unique_ptrs to support dynamic dispatch
unique_ptr<TTUHit> TTUHit::GetHit(const Retval& ret)
{
    switch (GetHitType(ret.data[3]))
    {
        case HitType::Triangle:
            return make_unique<HitTriangle>(ret);
        case HitType::TriRange:
            return make_unique<HitTriRange>(ret);
        case HitType::DisplacedSubTri:
            return make_unique<HitDisplacedSubTri>(ret);
        case HitType::Error:
            return make_unique<HitError>(ret);
        case HitType::None:
            return make_unique<HitNone>();
        default:
            return make_unique<HitIlwalid>(ret);
    }
}

namespace TTUUtils
{
    const char* QueryTypeToStr(QueryType queryType)
    {
        switch (queryType)
        {
            case QueryType::TriRange_Triangle:
                return "TriRange";
            case QueryType::Complet_Triangle:
                return "Complet (Intersecting Triangle)";
            case QueryType::Complet_TriRange:
                return "Complet (Intersecting TriRange)";
            default:
                return "INVALID QUERY TYPE";
        }
    }

    Point GetTriCentroid(const Triangle& tri)
    {
        Point p = {};
        p.x = (tri.v0.x + tri.v1.x + tri.v2.x) / 3.0f;
        p.y = (tri.v0.y + tri.v1.y + tri.v2.y) / 3.0f;
        p.z = (tri.v0.z + tri.v1.z + tri.v2.z) / 3.0f;
        return p;
    }

    Triangle InterpolateTris(const Triangle& tri, const Triangle& endTri,
                             const float timestamp)
    {
        Triangle result = {};
        result.v0.x = tri.v0.x + timestamp * (endTri.v0.x - tri.v0.x);
        result.v0.y = tri.v0.y + timestamp * (endTri.v0.y - tri.v0.y);
        result.v0.z = tri.v0.z + timestamp * (endTri.v0.z - tri.v0.z);
        result.v1.x = tri.v1.x + timestamp * (endTri.v1.x - tri.v1.x);
        result.v1.y = tri.v1.y + timestamp * (endTri.v1.y - tri.v1.y);
        result.v1.z = tri.v1.z + timestamp * (endTri.v1.z - tri.v1.z);
        result.v2.x = tri.v2.x + timestamp * (endTri.v2.x - tri.v2.x);
        result.v2.y = tri.v2.y + timestamp * (endTri.v2.y - tri.v2.y);
        result.v2.z = tri.v2.z + timestamp * (endTri.v2.z - tri.v2.z);
        return result;
    }
}

namespace TTUGen
{
    void GenTriangleA(const bool endTris, RandomState* const pRandState, UINT32* const pRngAccTri,
                      UINT32* const pRngAccTriEnd)
    {
        Random& rndToUse = (endTris) ? pRandState->randomTriEnd : pRandState->randomTri;

        // 3 verticies * 3 floats/vertex
        for (UINT32 i = 0; i < 9; i++)
        {
            *pRngAccTri += rndToUse.GetRandom();
        }
    }

    // Generates 3 random verticies inside bbox.
    Triangle GenTriangleB(const BoundingBox& bbox, const UINT32 triId,
                          const bool endTris, RandomState* const pRandState)
    {
        Random& rndToUse = (endTris) ? pRandState->randomTriEnd : pRandState->randomTri;
        Triangle tri = {};
        tri.triId = triId;

        tri.v0.x = rndToUse.GetRandomFloat(bbox.xmin, bbox.xmax);
        tri.v0.y = rndToUse.GetRandomFloat(bbox.ymin, bbox.ymax);
        tri.v0.z = rndToUse.GetRandomFloat(bbox.zmin, bbox.zmax);

        tri.v1.x = rndToUse.GetRandomFloat(bbox.xmin, bbox.xmax);
        tri.v1.y = rndToUse.GetRandomFloat(bbox.ymin, bbox.ymax);
        tri.v1.z = rndToUse.GetRandomFloat(bbox.zmin, bbox.zmax);

        tri.v2.x = rndToUse.GetRandomFloat(bbox.xmin, bbox.xmax);
        tri.v2.y = rndToUse.GetRandomFloat(bbox.ymin, bbox.ymax);
        tri.v2.z = rndToUse.GetRandomFloat(bbox.zmin, bbox.zmax);

        return tri;
    }

    void AddTriGroupA(const UINT32 trisToAdd, const bool useMotionBlur,
                      RandomState* const pRandState, UINT32* const pRngAccTri,
                      UINT32* const pRngAccTriEnd)
    {
        MASSERT(pRandState);
        MASSERT(pRngAccTri);
        MASSERT(pRngAccTriEnd);
        
        // Build the first triangle
        GenTriangleA(false, pRandState, pRngAccTri, pRngAccTriEnd);
        if (useMotionBlur)
        {
            GenTriangleA(true, pRandState, pRngAccTri, pRngAccTriEnd);
        }

        // Build remaining triangles
        for (UINT32 i = 1; i < trisToAdd; i++)
        {
            // Random triangle index
            *pRngAccTri += pRandState->randomTri.GetRandom();

            // New vertex
            *pRngAccTri += pRandState->randomTri.GetRandom();
            *pRngAccTri += pRandState->randomTri.GetRandom();
            *pRngAccTri += pRandState->randomTri.GetRandom();

            // New vertex, end triangle
            if (useMotionBlur)
            {
                *pRngAccTriEnd += pRandState->randomTriEnd.GetRandom();
                *pRngAccTriEnd += pRandState->randomTriEnd.GetRandom();
                *pRngAccTriEnd += pRandState->randomTriEnd.GetRandom();
            }

            // Random vertex to replace
            *pRngAccTri += pRandState->randomTri.GetRandom();
        }
    }

    //! \brief Add a triangle group of random triangles to a triangle range
    //! 
    //! - Each triangle is assigned a Triangle ID from pTriId, which is then incremented
    //! - A triangle group contains triangles that share edges and verticies for the purpose of 
    //!   increasing the lwlling rate of the TTU's Ray Triangle Test (RTT) unit by taking advantage
    //!   of the Triangle Pair (Ampere-501) and 4x Triangle Lwlling (Ada-24) features.
    //! - A TriGroupSize of 4 will generate at most 6 verticies and 9 edges.
    //! - A TriGroupSize of 3 will generate at most 5 verticies and 7 edges.
    //! - A TriGroupSize of 2 will generate at most 4 verticies and 5 edges.
    //! - A TriGroupSize of 1 will generate at most 3 verticies and 3 edges. This effectively
    //!   generates all triangles independently with no vertex/edge sharing.
    //! - Motion blur triangles can also be generated with vertex/edge sharing, however, they are
    //!   always processed 1 at a time and do not experience an increased lwlling rate.
    RC AddTriGroupB(TriangleRange* const pTriRange, const BoundingBox& bbox,
                    const UINT32 trisToAdd, const bool useMotionBlur, const UINT32 precision,
                    UINT32* const pTriId, RandomState* const pRandState)
    {
        RC rc;
        vector<Triangle> triGroup;
        vector<Triangle> endTriGroup;

        triGroup.reserve(trisToAdd);
        endTriGroup.reserve(trisToAdd);

        // Randomly generate the first triangle
        Triangle firstTri = GenTriangleB(bbox, *pTriId, false, pRandState);
        triGroup.push_back(firstTri);

        Triangle firstTriEnd = {};
        firstTriEnd.triId = *pTriId;
        if (useMotionBlur)
        {
            firstTriEnd = GenTriangleB(bbox, *pTriId, true, pRandState);
        }
        endTriGroup.push_back(firstTriEnd);
        (*pTriId)++;

        // Generate the remaining triangles
        for (UINT32 i = 1; i < trisToAdd; i++)
        {
            // Randomly pick an existing triangle to copy.
            const UINT32 rndIdx = pRandState->randomTri.GetRandom() % triGroup.size();
            Triangle newTri(triGroup[rndIdx]);
            Triangle newEndTri(endTriGroup[rndIdx]);

            // Update the triId
            newTri.triId = *pTriId;
            newEndTri.triId = *pTriId;
            (*pTriId)++;

            // Randomly generate a new vertex
            Vertex vNew = {};
            vNew.x = pRandState->randomTri.GetRandomFloat(bbox.xmin, bbox.xmax);
            vNew.y = pRandState->randomTri.GetRandomFloat(bbox.ymin, bbox.ymax);
            vNew.z = pRandState->randomTri.GetRandomFloat(bbox.zmin, bbox.zmax);

            Vertex vNewEnd = {};
            if (useMotionBlur)
            {
                vNewEnd.x = pRandState->randomTriEnd.GetRandomFloat(bbox.xmin, bbox.xmax);
                vNewEnd.y = pRandState->randomTriEnd.GetRandomFloat(bbox.ymin, bbox.ymax);
                vNewEnd.z = pRandState->randomTriEnd.GetRandomFloat(bbox.zmin, bbox.zmax);
            }

            // Replace one of the triangle's verticies with the new one. This ensures
            // that the new triangle will at minimum share 2 verticies and 1 edge
            // with the chosen existing triangle.
            const UINT32 vToReplace = pRandState->randomTri.GetRandom() % 3;
            if (vToReplace == 0)
            {
                newTri.v0 = vNew;
                newEndTri.v0 = vNewEnd;
            }
            else if (vToReplace == 1)
            {
                newTri.v1 = vNew;
                newEndTri.v1 = vNewEnd;
            }
            else
            {
                newTri.v2 = vNew;
                newEndTri.v2 = vNewEnd;
            }

            triGroup.push_back(newTri);
            endTriGroup.push_back(newEndTri);
        }

        // Add generated trigroup to trirange
        for (UINT32 i = 0; i < triGroup.size(); i++)
        {
            CHECK_RC(pTriRange->AddTri(triGroup[i], endTriGroup[i], precision));
        }
        return rc;
    }

    void AddTrisUniformA(const UINT32 numTris, const UINT32 triGroupSize,
                         const bool useMotionBlur, RandomState* const pRandState,
                         UINT32* const pRngAccTri, UINT32* const pRngAccTriEnd)
    {
        MASSERT(pRandState);
        MASSERT(pRngAccTri);
        MASSERT(pRngAccTriEnd);

        for (UINT32 i = 0; i < numTris; i += triGroupSize)
        {
            const UINT32 trisToAdd = std::min<UINT32>(numTris - i, triGroupSize);
            AddTriGroupA(trisToAdd, useMotionBlur, pRandState, pRngAccTri, pRngAccTriEnd);
        }
    }

    //! \brief Add numTris random triangles to a given TriangleRange
    //!
    RC AddTrisUniformB(TriangleRange* const pTriRange, const BoundingBox& bbox,
                       const UINT32 numTris, const UINT32 triGroupSize,
                       const bool useMotionBlur, const UINT32 precision,
                       UINT32* const pTriId, RandomState* const pRandState)
    {
        RC rc;

        for (UINT32 i = 0; i < numTris; i += triGroupSize)
        {
            const UINT32 trisToAdd = std::min<UINT32>(numTris - i, triGroupSize);
            CHECK_RC(AddTriGroupB(pTriRange, bbox, trisToAdd, useMotionBlur, precision,
                                  pTriId, pRandState));
        }

        // If using compressed triangles, explicitly finalize the last triangle block
        if (precision)
        {
            CHECK_RC(pTriRange->FinalizeComprBlock());
        }
        return rc;
    }

    void AddMicromeshA(RandomState* const pRandState, UINT32* const pRngAccTri)
    {
        MASSERT(pRandState);
        MASSERT(pRngAccTri);
        const UINT32 num = 9 + // vertex
                           9 + // direction
                           1;  // scale
        for (UINT32 i = 0; i < num; i++)
        {
            *pRngAccTri += pRandState->randomTri.GetRandom();
        }
        for (UINT32 i = 0; i < DisplacedMicromesh::s_DispBlockSizeBytes; i++)
        {
            if (!(i >= 110 && i <= 121))
            {
                *pRngAccTri += pRandState->randomTri.GetRandom();
            }
        }
    }

    //! \brief Generates a random displaced micromesh.
    //!
    void AddMicromeshB(vector<DisplacedMicromesh>* const pMicromeshes,
                       const BoundingBox& bbox, UINT32* const pTriId,
                       RandomState* const pRandState)
    {
        Triangle baseTri = GenTriangleB(bbox, *pTriId, false, pRandState);
        (*pTriId)++;

        Vertex dir0 = {};
        Vertex dir1 = {};
        Vertex dir2 = {};

        dir0.x = pRandState->randomTri.GetRandomFloat(bbox.xmin, bbox.xmax);
        dir0.y = pRandState->randomTri.GetRandomFloat(bbox.ymin, bbox.ymax);
        dir0.z = pRandState->randomTri.GetRandomFloat(bbox.zmin, bbox.zmax);

        dir1.x = pRandState->randomTri.GetRandomFloat(bbox.xmin, bbox.xmax);
        dir1.y = pRandState->randomTri.GetRandomFloat(bbox.ymin, bbox.ymax);
        dir1.z = pRandState->randomTri.GetRandomFloat(bbox.zmin, bbox.zmax);

        dir2.x = pRandState->randomTri.GetRandomFloat(bbox.xmin, bbox.xmax);
        dir2.y = pRandState->randomTri.GetRandomFloat(bbox.ymin, bbox.ymax);
        dir2.z = pRandState->randomTri.GetRandomFloat(bbox.zmin, bbox.zmax);

        // Must be kept small to prevent lots of ray misses
        const float scale = pRandState->randomTri.GetRandomFloat(-0.001, 0.001);

        // Generate random compressed displacement block
        constexpr UINT32 dispBytes = DisplacedMicromesh::s_DispBlockSizeBytes;
        DisplacedMicromesh::DisplacementBlock dispData = {};
        for (UINT32 i = 0; i < dispBytes; i++)
        {
            if (i >= 110 && i <= 121)
            {
                // bits [882:969] empty space
                dispData[i] = 0;
            }
            else
            {
                dispData[i] = pRandState->randomTri.GetRandom();
            }
        }
        // bits [1022:1023] reserved (must be 0)
        dispData[dispBytes - 1] &= ~static_cast<UINT08>(3);

        pMicromeshes->emplace_back(baseTri, dir0, dir1, dir2, scale, dispData);
    }

    void GenRandomShearDataA(const bool shearComplets, RandomState* const pRandState,
                             UINT32* const pRngAccGen)
    {
        MASSERT(pRandState);
        MASSERT(pRngAccGen);
        if (shearComplets)
        {
            *pRngAccGen += pRandState->randomGen.GetRandom();
            *pRngAccGen += pRandState->randomGen.GetRandom();
            *pRngAccGen += pRandState->randomGen.GetRandom();
        }
    }

    ShearData GenRandomShearDataB(const bool shearComplets, RandomState* const pRandState)
    {
        ShearData data = {};
        data.enabled = shearComplets;
        if (shearComplets)
        {
            // Choose one of (BD, CE, AF) shear selects. Only these 3 are
            // used because they lead to additional FADD/FSCADD's and
            // because they allow for the full range of coefficients.
            const UINT08 rnd = pRandState->randomGen.GetRandom() % 3;
            switch (rnd)
            {
                case 0:
                    data.select = ShearSelect::BD;
                    break;
                case 1:
                    data.select = ShearSelect::CE;
                    break;
                default:
                    data.select = ShearSelect::AF;
                    break;
            }
            data.coeff0 = ShearCoefficient(pRandState->randomGen.GetRandom() % 16);
            data.coeff1 = ShearCoefficient(pRandState->randomGen.GetRandom() % 16);
        }
        return data;
    }

    void GelwisibilityBlockA(RandomState* const pRandState, UINT32* const pRngAccTri)
    {
        MASSERT(pRandState);
        MASSERT(pRngAccTri);
        for (UINT32 i = 0; i < TTU_CACHE_LINE_BYTES; i++)
        {
            for (UINT32 j = 0; j < 8; j++)
            {
                *pRngAccTri += pRandState->randomTri.GetRandom();
            }
        }
    }

    // Generates an uncompressed visibility block (1024 1-bit, 2-state visibility states)
    // https://confluence.lwpu.com/display/ADASMTTU/FD+Ada-20+Visibility+Masks
    VisibilityBlock GelwisibilityBlockB(const float visibilityMaskOpacity,
                                        RandomState* const pRandState)
    {
        VisibilityBlock visibilityStates = {};
        for (UINT32 i = 0; i < TTU_CACHE_LINE_BYTES; i++)
        {
            visibilityStates[i] = 0;
            // Each visibility state is 1 bit
            for (UINT32 j = 0; j < 8; j++)
            {
                if (pRandState->randomTri.GetRandomFloat(0.0, 1.0) <= visibilityMaskOpacity)
                {
                    visibilityStates[i] |= (1 << j);
                }
            }
        }
        return visibilityStates;
    }

    //! \brief Get the number of TriangleRanges and Complets required by a randomly-generated BVH
    void GenRandomBVHPartA
    (
        const UINT32 width,
        const UINT32 depth,
        const float probSplit,
        const UINT32 numTris,
        const UINT32 triGroupSize,
        const bool useMotionBlur,
        const bool shearComplets,
        const float visibilityMaskOpacity,
        const bool useMicromesh,
        UINT32* const pNumTriRanges,
        UINT32* const pNumMicromeshes,
        UINT32* const pNumComplets,
        RandomState* const pRandState,
        UINT32* const pRngAccGen,
        UINT32* const pRngAccTri,
        UINT32* const pRngAccTriEnd
    )
    {
        MASSERT(pNumTriRanges);
        MASSERT(pNumMicromeshes);
        MASSERT(pNumComplets);
        MASSERT(pRandState);
        MASSERT(pRngAccGen);
        MASSERT(pRngAccTri);
        MASSERT(pRngAccTriEnd);
        (*pNumComplets)++;

        // Advance the RNG (this must match with PartB)
        for (UINT32 i = 1; i < width; i++)
        {
            *pRngAccGen += pRandState->randomGen.GetRandom();
        }

        for (UINT32 i = 0; i < width; i++)
        {
            float r = pRandState->randomGen.GetRandomFloat(0.0, 1.0);
            if (depth > 0 && r <= probSplit)
            {
                GenRandomShearDataA(shearComplets, pRandState, pRngAccGen);
                GenRandomBVHPartA(width, depth - 1, probSplit, numTris, triGroupSize,
                                  useMotionBlur, shearComplets, visibilityMaskOpacity,
                                  useMicromesh, pNumTriRanges, pNumMicromeshes, pNumComplets,
                                  pRandState, pRngAccGen, pRngAccTri, pRngAccTriEnd);
            }
            else
            {
                if (useMicromesh)
                {
                    AddMicromeshA(pRandState, pRngAccTri);
                    (*pNumMicromeshes)++;
                }
                else
                {
                    AddTrisUniformA(numTris, triGroupSize, useMotionBlur, pRandState,
                                    pRngAccTri, pRngAccTriEnd);

                    if (visibilityMaskOpacity > 0.0f)
                    {
                        for (UINT32 i = 0; i < numTris; i++)
                        {
                            GelwisibilityBlockA(pRandState, pRngAccTri);
                        }
                    }

                    (*pNumTriRanges)++;
                }
            }
        }
    }

    //! \brief Randomly generate a BVH, relwrsively
    //!
    RC GenRandomBVHPartB
    (
        Complet* const pLwrComplet,
        const Lwca::Capability computeCap,
        const UINT32 width,
        const UINT32 depth,
        float probSplit,
        const UINT32 numTris,
        const UINT32 triGroupSize,
        const UINT32 precision,
        const bool useMotionBlur,
        const bool shearComplets,
        const float visibilityMaskOpacity,
        const bool useMicromesh,
        vector<TriangleRange>* const pTriRanges,
        vector<DisplacedMicromesh>* const pMicromeshes,
        vector<Complet>* const pComplets,
        UINT32* const pTriId,
        RandomState* const pRandState
    )
    {
        MASSERT(useMicromesh ? pMicromeshes != nullptr : pTriRanges != nullptr);
        MASSERT(pComplets);
        MASSERT(pTriId);
        MASSERT(pRandState);
        RC rc;

        // We use uninitialized arrays for better perf
        std::array<float, Complet::s_MaxChildren> maxlens;
        std::array<BoundingBox, Complet::s_MaxChildren> bboxes;
        bboxes[0]  = pLwrComplet->GetAABB();
        maxlens[0] = bboxes[0].MaxEdgeLen();

        // Split the parent bbox into 'width' children
        // by relwrsively splitting along the longest edge
        for (UINT32 i = 1; i < width; i++)
        {
            // Find bbox with the longest edge
            const size_t idx =
                std::distance(maxlens.begin(), std::max_element(maxlens.begin(),
                                                                maxlens.begin() + i));

            // Callwlate new edge length obtained by splitting the longest edge
            const float maxlen = maxlens[idx];
            constexpr float jitter = 1.0f / 16.0f;
            const float newlen =
                (maxlen / 2.0f) * pRandState->randomGen.GetRandomFloat(1.0f - jitter,
                                                                       1.0f + jitter);

            // Clone bbox containing longest edge
            BoundingBox* pOld = &bboxes[idx];
            BoundingBox* pNew = &bboxes[i];
            *pNew = *pOld;

            // Divide old and new bboxes along the longest edge
            if (maxlen == pOld->xmax - pOld->xmin)
            {
                pOld->xmax = pOld->xmin + newlen;
                pNew->xmin = pOld->xmin + newlen;
            }
            else if (maxlen == pOld->ymax - pOld->ymin)
            {
                pOld->ymax = pOld->ymin + newlen;
                pNew->ymin = pOld->ymin + newlen;
            }
            else if (maxlen == pOld->zmax - pOld->zmin)
            {
                pOld->zmax = pOld->zmin + newlen;
                pNew->zmin = pOld->zmin + newlen;
            }
            else
            {
                MASSERT(!"CANNOT FIND LONGEST EDGE IN AABB");
                return RC::SOFTWARE_ERROR;
            }

            // Update longest edge length of the modified bboxes
            maxlens[idx] = pOld->MaxEdgeLen();
            maxlens[i]   = pNew->MaxEdgeLen();
        }

        for (UINT32 i = 0; i < width; i++)
        {
            float r = pRandState->randomGen.GetRandomFloat(0.0, 1.0);
            if (depth > 0 && r <= probSplit)
            {
                // Generate child complet
                if (pComplets->size() + 1 > pComplets->capacity())
                {
                    Printf(Tee::PriError,
                        "Vector capacity must be set before generating BVH child complet, "
                        "otherwise resizing the vector may result in incorrect pointers\n");
                    return RC::SOFTWARE_ERROR;
                }

                const ShearData shearData = GenRandomShearDataB(shearComplets, pRandState);
                if (useMicromesh)
                {
                    pComplets->emplace_back(computeCap, ChildFormat::Standard,
                                            LeafType::DisplacedMicromesh,
                                            bboxes[i], false, shearData);
                }
                else
                {
                    const ChildFormat format =
                            (useMotionBlur) ? ChildFormat::MotionBlur : ChildFormat::Standard;
                    pComplets->emplace_back(computeCap, format, LeafType::TriRange, bboxes[i],
                                            false, shearData);
                }
                pLwrComplet->AddChildComplet(&pComplets->back());
                CHECK_RC(GenRandomBVHPartB(&pComplets->back(), computeCap, width, depth - 1,
                                           probSplit, numTris, triGroupSize, precision,
                                           useMotionBlur, shearComplets, visibilityMaskOpacity,
                                           useMicromesh, pTriRanges, pMicromeshes, pComplets,
                                           pTriId, pRandState));
            }
            else
            {
                // Generate leaf (trirange or micromesh triangle)
                if (useMicromesh ? 
                    (pMicromeshes->size() + 1 > pMicromeshes->capacity()) :
                    (pTriRanges->size() + 1 > pTriRanges->capacity()))
                {
                    Printf(Tee::PriError,
                        "Vector capacity must be set before generating BVH leaf, "
                        "otherwise resizing the vector may result in incorrect pointers\n");
                    return RC::SOFTWARE_ERROR;
                }

                if (useMicromesh)
                {
                    AddMicromeshB(pMicromeshes, bboxes[i], pTriId, pRandState);
                    CHECK_RC(pLwrComplet->AddChildMicromesh(&pMicromeshes->back()));
                }
                else
                {
                    const bool useVM = visibilityMaskOpacity > 0.0f;
                    pTriRanges->emplace_back(useMotionBlur, useVM);
                    TriangleRange* pLwrTriRange = &pTriRanges->back();

                    CHECK_RC(AddTrisUniformB(pLwrTriRange, bboxes[i], numTris, triGroupSize,
                                             useMotionBlur, precision, pTriId, pRandState));

                    if (useVM)
                    {
                        for (UINT32 i = 0; i < numTris; i++)
                        {
                            VisibilityBlock vBlock = GelwisibilityBlockB(visibilityMaskOpacity,
                                                                         pRandState);
                            pLwrTriRange->AddVisibilityBlock(vBlock);
                        }
                    }

                    CHECK_RC(pLwrComplet->AddChildTriRange(pLwrTriRange));
                }
            }
        }

        return rc;
    }

} // namespace TTUGen
