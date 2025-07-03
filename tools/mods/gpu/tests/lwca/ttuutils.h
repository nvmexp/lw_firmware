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

#pragma once
#include "core/include/massert.h"
#include "core/include/types.h"
#include <array>

// MODS-specific
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwca/lwdawrap.h"
#include "gpu/tests/lwca/ttu/ttustress.h"

#define TTU_CACHE_LINE_BYTES 128

enum class TriangleMode : UINT08
{
    Uncompressed = 0,
    Compressed = 1,
    MotionBlur = 2,
    CompressedVM = 3,
    MotionBlurVM = 4,
    Micromesh = 5,
};

enum class LeafType : UINT08
{
    ItemRange = 0,
    TriRange = 1,
    InstNode = 2,
    DisplacedMicromesh = 3
};

enum class ChildFormat : UINT08
{
    Standard = 0,
    MotionBlur = 1,
    MultiBox = 2,
    MotionBlurMultiBox = 3
};

enum class ShearSelect : UINT08
{
    AB = 0,
    CD = 1,
    EF = 2,
    BD = 3,
    CE = 4,
    AF = 5,
    Reserved0 = 6,
    Reserved1 = 7,
    A = 8,
    B = 9,
    C = 10,
    D = 11,
    E = 12,
    F = 13,
    Reserved2 = 14,
    Reserved3 = 15
};

enum class ShearCoefficient : UINT08
{
    POS_1_8 = 0,
    POS_1_4 = 1,
    POS_3_8 = 2,
    POS_1_2 = 3,
    POS_5_8 = 4,
    POS_3_4 = 5,
    POS_7_8 = 6,
    POS_1 = 7,
    NEG_1_8 = 8,
    NEG_1_4 = 9,
    NEG_3_8 = 10,
    NEG_1_2 = 11,
    NEG_5_8 = 12,
    NEG_3_4 = 13,
    NEG_7_8 = 14,
    NEG_1 = 15
};

struct ShearData
{
    bool enabled;
    ShearSelect select;
    ShearCoefficient coeff0;
    ShearCoefficient coeff1;
};

struct BoundingBox
{
    float xmin;
    float ymin;
    float zmin;
    float xmax;
    float ymax;
    float zmax;

    void ToScl(UINT08* pXscl, UINT08* pYscl, UINT08* pZscl) const;
    float MaxEdgeLen() const
    {
        return std::max({xmax - xmin, ymax - ymin, zmax - zmin});
    }
};

struct Child
{
    UINT08 xlo;
    UINT08 xhi;
    UINT08 ylo;
    UINT08 yhi;
    UINT08 zlo;
    UINT08 zhi;
    UINT08 rval;
    UINT08 data;

    void SetAABB(const BoundingBox& bbox, const BoundingBox& childAABB);
};

struct Vertex
{
    float x;
    float y;
    float z;

    bool operator==(const Vertex& vert) const
    {
        return x == vert.x && y == vert.y && z == vert.z;
    }
};

struct Triangle
{
    UINT32 triId;
    Vertex v0;
    Vertex v1;
    Vertex v2;

    bool operator==(const Triangle& tri) const
    {
        return triId == tri.triId && v0 == tri.v0 && v1 == tri.v1 && v2 == tri.v2;
    }
};

struct ComprVert
{
    UINT32 x;
    UINT32 y;
    UINT32 z;

    bool operator==(const ComprVert& compr) const
    {
        return x == compr.x && y == compr.y && z == compr.z;
    }
};

using VisibilityBlock = std::array<UINT08, TTU_CACHE_LINE_BYTES>;

class TriangleRange
{
public:
    explicit TriangleRange(bool useMotionBlur, bool useVisibilityMasks)
    : m_UseMotionBlur(useMotionBlur)
    , m_UseVisibilityMasks(useVisibilityMasks)
    {};

    RC FinalizeComprBlock();
    RC AddTri(const Triangle& tri, const Triangle& endTri, UINT08 prec);
    UINT32 TriIdx() const { return s_FirstTriIdx; }
    UINT32 NumLines() const;
    UINT32 SizeBytes() const { return TTU_CACHE_LINE_BYTES * NumLines(); }
    BoundingBox GenerateAABB(bool useEndTris) const;
    void Serialize(UINT08* dst, UINT08* vBlockDst, UINT64 vBlockDeviceAddr) const;
    RC DumpToJson() const;
    void AddVisibilityBlock(VisibilityBlock& vBlock) { m_VisibilityBlocks.push_back(vBlock); }
    UINT32 VisibilityBlocksSizeBytes() const
    {
        return TTU_CACHE_LINE_BYTES * static_cast<UINT32>(m_VisibilityBlocks.size());
    }
    vector<Triangle> GetTris() const { return m_Triangles; }
    vector<Triangle> GetEndTris() const { return m_EndTriangles; }

private:
    RC InitComprBlock(const Triangle& firstTri, UINT32 prec);

    struct ComprBlockData
    {
        UINT32 numTris;
        UINT32 baseTriId;
        UINT32 baseX;
        UINT32 baseY;
        UINT32 baseZ;

        UINT32 idPrec;
        UINT32 prec;
        UINT32 shift;
    };
    struct CompressorState
    {
        UINT32 diffTriId;
        UINT32 diffX;
        UINT32 diffY;
        UINT32 diffZ;
        bool   blockInitialized;
    };

    // Triangle compressor state (used when adding compressed triangles)
    CompressorState m_ComprState = {};

    // Compressed triangle block metadata
    vector<ComprBlockData> m_ComprData;

    // Uncompressed triangle storage
    vector<Triangle> m_Triangles;

    // Motion-blur Primatives
    bool m_UseMotionBlur = false;
    vector<Triangle> m_EndTriangles;

    bool m_UseVisibilityMasks = false;
    vector<VisibilityBlock> m_VisibilityBlocks;

    static constexpr UINT32 s_FirstTriIdx = 0;
    static constexpr UINT32 s_MaxTrisPerComprBlock = 16;
    static constexpr UINT32 s_MaxTrisPerUncomprBlock = 3;
    static constexpr UINT32 s_MaxTriBlocks = 7; // field complet.child.data.lines is 3 bits

    // Visibility blocks are always cacheline-aligned (128B offets).
    // 11b (max offset 1920B = 128B*(s_MaxTrisPerComprBlock-1)) + 1b vmOffsets are signed
    static constexpr UINT32 s_PrecisiolwMOffset = 12;

    // number of utris per base tri (in visibility masks)
    static constexpr UINT32 s_VMLevel = 5; // 4^5 = 1024
};

class DisplacedMicromesh
{
public:
    using DisplacementBlock = std::array<UINT08, TTU_CACHE_LINE_BYTES>;

    DisplacedMicromesh
    (
        Triangle baseTri,
        Vertex dir0,
        Vertex dir1,
        Vertex dir2,
        float scale,
        DisplacementBlock dispData
    )
    : m_BaseTriangle(baseTri)
    , m_DisplacementDir0(dir0)
    , m_DisplacementDir1(dir1)
    , m_DisplacementDir2(dir2)
    , m_DisplacementScale(scale)
    , m_DisplacementData(dispData)
    {};

    UINT32 SizeBytes() const;
    BoundingBox GenerateAABB() const;
    void Serialize(UINT08* dst, UINT64 meshDeviceAddr,
                   UINT08* vBlockDst, UINT64 vBlockDeviceAddr) const;
    RC DumpToJson() const;
    void SetVisibilityBlock(VisibilityBlock& vBlock) { m_VisibilityBlock = vBlock; }
    UINT32 VisibilityBlocksSizeBytes() { return m_UseVisibilityMasks ? TTU_CACHE_LINE_BYTES : 0; }

    // number of utris per base tri (4^s_BaseResolution)
    static constexpr UINT32 s_BaseResolution = 5; // 4^5 = 1024

    // number of complet leaves per base tri
    static constexpr UINT32 s_SubTrisPerBaseTri = 1;

    // number of utris per sub tri
    static constexpr UINT32 s_DispBlockResolution = 1024; // 32x32

    // bytes per sub tri
    static constexpr UINT32 s_DispBlockSizeBytes = TTU_CACHE_LINE_BYTES;

    // number of utris per base tri (in visibility masks)
    static constexpr UINT32 s_VMLevel = 5; // 4^5 = 1024

private:
    Triangle m_BaseTriangle;

    // Displacement for base verticies
    Vertex m_DisplacementDir0;
    Vertex m_DisplacementDir1;
    Vertex m_DisplacementDir2;
    float m_DisplacementScale;
    DisplacementBlock m_DisplacementData;

    bool m_UseVisibilityMasks = false;
    VisibilityBlock m_VisibilityBlock;
};

class Complet
{
public:
    Complet
    (
        Lwca::Capability computeCap,
        ChildFormat format,
        LeafType type,
        const BoundingBox& bbox,
        bool isRoot,
        ShearData shearData
    )
    : m_ComputeCap(computeCap)
    , m_IsRoot(isRoot)
    , m_LeafType(type)
    , m_Format(format)
    , m_ShearData(shearData)
    , m_CompletAABB(bbox)
    {};

    BoundingBox GetAABB() const { return m_CompletAABB; }
    RC AddChildComplet(Complet* pChildComplet);
    RC AddChildTriRange(TriangleRange* pTriRange);
    RC AddChildMicromesh(DisplacedMicromesh* pMicromesh);

    UINT64 BVHSizeBytes() const;
    UINT64 SerializeBVHAbs(UINT64 const rootPtr, UINT08* const dst);

    UINT64 SizeBytes () const { return m_CompletData.size() * sizeof(m_CompletData[0]); }
    void Serialize(UINT08* dst);

    UINT08* data() { return m_CompletData.data(); }
    RC DumpToJson() const;

    void set_itemBaseLo(UINT32 itemBaseLo);
    void set_itemBaseHi(UINT32 itemBaseHi);
    void set_leafPtrLo(UINT32 leafPtrLo);
    void set_leafPtr(UINT64 leafPtr);
    void set_childOfs(UINT32 childOfs);
    void set_childPtr(UINT64 childPtr);
    void set_parentOfs(UINT32 parentOfs);
    void set_parentPtr(UINT64 parentPtr);
    void set_parentLeafIdx(UINT08 parentLeafIdx);
    void set_firstTriIdx(UINT32 firstTriIdx);
    void set_shearData();

    static constexpr UINT32 s_MaxChildren = 11;
    static constexpr bool s_UseRelLeafPtr = true;
    static constexpr bool s_UseRelCompletPtr = false;

private:
    // Complet fields (pre-serialization)
    Lwca::Capability m_ComputeCap;
    bool m_IsRoot;
    LeafType m_LeafType;
    ChildFormat m_Format;
    ShearData m_ShearData;

    // During serialization the max coordinates get colwerted to
    // a scalar factor. As a result some precision will be lost
    BoundingBox m_CompletAABB = {};

    // These pointers are filled
    vector<Complet*> m_ChildComplets;
    vector<TriangleRange*> m_TriRanges;
    vector<DisplacedMicromesh*> m_Micromeshes;

    // Utility methods 
    UINT32 GetMaxChildren() const;
    RC CheckChildCapacity() const;

    UINT64 SerializeBVHAbs
    (
        UINT64 const rootPtr,
        UINT08* const dst,
        UINT64 const completOffset,
        UINT64 offset
    );
    std::array<UINT08, TTU_CACHE_LINE_BYTES> m_CompletData = {};

    void set_leafType(UINT08 leafType);
    void set_relPtr(UINT08 relPtr);
    void set_lptr(UINT08 lptr);
    void set_format(UINT08 format);

    void set_complet_bbox(const BoundingBox& bbox);
    void set_xscl(UINT08 xscl);
    void set_yscl(UINT08 yscl);
    void set_zscl(UINT08 zscl);
    void set_xmin(float xmin);
    void set_ymin(float ymin);
    void set_zmin(float zmin);

    void set_child_ilwalid(UINT32 childIdx);
    void set_child(UINT32 childIdx, UINT64 child);
    void set_child_data(UINT32 childIdx, UINT08 data);
    void set_child_rval(UINT32 childIdx, UINT08 rval);
    void set_child_bbox(UINT32 childIdx,
                        UINT08 zhi, UINT08 zlo,
                        UINT08 yhi, UINT08 ylo,
                        UINT08 xhi, UINT08 xlo);
};

// For detailed information on the hit types, refer to:
// https://wiki.lwpu.com/gpuhwvolta/index.php/TTU/Programming#Hit_Types
struct TTUHit
{
    HitType hitType;

    virtual string ToStr() = 0;
    static unique_ptr<TTUHit> GetHit(const Retval& ret);
};

// Normal Hit Type of a ray intersecting a triangle
struct HitTriangle : TTUHit
{
    UINT32 triId;
    float t;
    float u;
    float v;
    UINT32 alpha : 1;
    UINT32 facing : 1;

    HitTriangle(const Retval& ret)
    {
        hitType = HitType::Triangle;
        triId   = ret.data[0];
        alpha   = ret.data[1] >> 31;
        facing  = ret.data[2] >> 31;

        UINT32 tmp;
        tmp = ret.data[1] & 0x7FFFFFFFu;
        t = reinterpret_cast<float*>(&tmp)[0];
        tmp = ret.data[2] & 0x7FFFFFFFu;
        u = reinterpret_cast<float*>(&tmp)[0];
        tmp = ret.data[3] & 0x7FFFFFFFu;
        v = reinterpret_cast<float*>(&tmp)[0];
    }
    string ToStr()
    {
        return Utility::StrPrintf("TriangleHit: TriID=%d t=%f u=%f v=%f a=%d f=%d",
                                  triId, t, u, v, alpha, facing);
    }
};

// Hit Type of a ray intersecting a triangle range
// This is returned when we instruct the TTU to not continue with the triangle intersection test
struct HitTriRange : TTUHit
{
    device_ptr triRangePtr;
    float t;
    UINT32 triIdx : 4;
    UINT32 triEnd : 4;
    UINT32 lines  : 3;

    HitTriRange(const Retval& ret)
    {
        hitType     = HitType::TriRange;
        triRangePtr = (static_cast<device_ptr>(ret.data[1] & 0x1FFFF) << 32) | ret.data[0];
        triIdx = (ret.data[1] >> 21) & 0xF;
        triEnd = (ret.data[1] >> 25) & 0xF;
        lines  = (ret.data[1] >> 29) & 0x7;
        t      = reinterpret_cast<const float*>(&ret.data[2])[0];
    }
    string ToStr()
    {
        return Utility::StrPrintf("TriRangeHit: ptr=0x%llx TriIdx=%d TriEnd=%d Lines=%d t=%f",
                                  triRangePtr, triIdx, triEnd, lines, t);
    }
};


// Hit Type of a ray intersecting a displaced micromesh sub triangle
// This is returned when the displaced micro-mesh mode flags indicate a return to SM
// https://confluence.lwpu.com/display/ADASMTTU/FD+Ada-21+Displaced+Micro-meshes
struct HitDisplacedSubTri : TTUHit
{
    device_ptr triPtr;
    UINT32 subTriIdx : 6;
    float t;

    HitDisplacedSubTri(const Retval& ret)
    {
        hitType     = HitType::DisplacedSubTri;
        triPtr      = ((static_cast<device_ptr>(ret.data[1] & 0x3FF) << 32) |
                        ret.data[0]) << 7; // 128b aligned
        subTriIdx   = (ret.data[1] >> 10) & 0x3F;
        t           = reinterpret_cast<const float*>(&ret.data[2])[0];
    }
    string ToStr()
    {
        return Utility::StrPrintf("DisplacedSubTri: triPtr=0x%llx subTriIdx=%d t=%f",
                                  triPtr, subTriIdx, t);
    }
};

// For decoding the error, refer to:
// https://wiki.lwpu.com/gpuhwvolta/index.php/TTU/Programming#Error_Conditions
//
// Hit Errors are caused by data corruption or by setting up the TTU query incorrectly.
struct HitError : TTUHit
{
    device_ptr errorPtr;
    UINT32 lines : 3;
    UINT32 errorCode : 6;

    HitError(const Retval& ret)
    {
        hitType   = HitType::Error;
        errorPtr  = (static_cast<device_ptr>(ret.data[1] & 0x1FFFF) << 32) | ret.data[0];
        lines     = (ret.data[1] >> 29) & 0x7;
        errorCode = ret.data[3] & 0x3F;
    }
    string ToStr()
    {
        return Utility::StrPrintf("Error: ptr=0x%llx Lines=%d ErrorCode=0x%02x",
                                  errorPtr, lines, errorCode);
    }
};

// This is the Hit Type returned when a ray doesn't intersect anything
struct HitNone : TTUHit
{
    HitNone()
    {
        hitType = HitType::None;
    }
    string ToStr()
    {
        return "HitNone (MISS)";
    }
};

// Unknown hit type. This should only be returned if the data was corrupted.
struct HitIlwalid : TTUHit
{
    UINT32 ilwalidType;
    HitIlwalid(const Retval& ret)
    {
        hitType     = HitType::INVALID;
        ilwalidType = ret.data[3] >> 28;
    }
    string ToStr()
    {
        return Utility::StrPrintf("INVALID: HitType=%d", ilwalidType);
    }
};

namespace TTUUtils
{
    const char* QueryTypeToStr(QueryType queryType);
    Point GetTriCentroid(const Triangle& tri);
    Triangle InterpolateTris(const Triangle& tri, const Triangle& endTri,
                             const float timestamp);
}

// TTUGen contains functions for randomly generating structures used by
// the TTU (complets, triranges, displaced micromeshes, etc).
// Functions suffixed with B actually perform the generation while
// functions suffixed with A don't only advance the RandomState in the
// same manner as their B function counterpart.
namespace TTUGen
{
    struct RandomState
    {
        Random randomGen;
        Random randomTri;
        Random randomTriEnd;
    };

    void GenTriangleA(const bool endTris, RandomState* const pRandState,
                      UINT32* const pRngAccTri, UINT32* const pRngAccTriEnd);
    Triangle GenTriangleB(const BoundingBox& bbox, const UINT32 triId,
                          const bool endTris, RandomState* const pRandState);

    void AddTriGroupA(const UINT32 trisToAdd, const bool useMotionBlur,
                      RandomState* const pRandState, UINT32* const pRngAccTri,
                      UINT32* const pRngAccTriEnd);
    RC AddTriGroupB(TriangleRange* const pTriRange, const BoundingBox& bbox,
                    const UINT32 trisToAdd, const bool useMotionBlur,
                    const UINT32 precision, UINT32* const pTriId,
                    RandomState* const pRandState);

    void AddTrisUniformA(const UINT32 numTris, const UINT32 triGroupSize,
                         const bool useMotionBlur, RandomState* const pRandState,
                         UINT32* const pRngAccTri, UINT32* const pRngAccTriEnd);
    RC AddTrisUniformB(TriangleRange* const pTriRange, const BoundingBox& bbox,
                       const UINT32 numTris, const UINT32 triGroupSize,
                       const bool useMotionBlur, const UINT32 precision,
                       UINT32* const pTriId, RandomState* const pRandState);

    void AddMicromeshA(RandomState* const pRandState, UINT32* const pRngAccTri);
    void AddMicromeshB(vector<DisplacedMicromesh>* const pMicromeshes,
                       const BoundingBox& bbox, UINT32* const pTriId,
                       RandomState* const pRandState);

    void GenRandomShearDataA(const bool shearComplets, RandomState* const pRandState,
                             UINT32* const pRngAccGen);
    ShearData GenRandomShearDataB(const bool shearComplets, RandomState* const pRandState);

    void GelwisibilityBlockA(RandomState* const pRandState, UINT32* const pRngAccTri);
    VisibilityBlock GelwisibilityBlockB(const float visibilityMaskOpacity,
                                        RandomState* const pRandState);

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
    );
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
    );

} // namespace TTUGen
