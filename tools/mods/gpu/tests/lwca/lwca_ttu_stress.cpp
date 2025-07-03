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

#include "core/include/display.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwdastst.h"
#include "gpu/tests/lwca/ttuutils.h"

class LwdaTTUStress : public LwdaStreamTest
{
public:
    LwdaTTUStress()
    : LwdaStreamTest()
    {
        SetName("LwdaTTUStress");
    }

    bool IsSupported() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC Setup() override;
    RC Cleanup() override;
    RC Run() override;

    // JS property accessors
    SETGET_PROP(RuntimeMs, UINT32);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(InnerIterations, UINT64);
    SETGET_PROP(NumBlocks, UINT32);
    SETGET_PROP(NumThreads, UINT32);
    SETGET_PROP(TestTriRange, bool);
    SETGET_PROP(TestBVHTriangle, bool);
    SETGET_PROP(TestBVHTriRange, bool);
    SETGET_PROP(TestBVHMicromesh, bool);
    SETGET_PROP(TestSanityCheck, bool);
    SETGET_PROP(BVHWidth, UINT32);
    SETGET_PROP(BVHDepth, UINT32);
    SETGET_PROP(BVHProbSplit, float);
    SETGET_PROP(NumTris, UINT32);
    SETGET_PROP(TriGroupSize, UINT08);
    SETGET_PROP(UseMotionBlur, bool);
    SETGET_PROP(Precision, UINT08);
    SETGET_PROP(ErrorLogLen, UINT32);
    SETGET_PROP(DumpMiscompares, bool);
    SETGET_PROP(ReportFailingSmids, bool);
    SETGET_PROP(SmidFailureLimit, UINT64);
    SETGET_PROP(DumpToJson, bool);
    SETGET_PROP(RaysToDump, UINT32);
    SETGET_PROP(ClusterRays, bool);
    SETGET_PROP(ShearComplets, bool);
    SETGET_PROP(VisibilityMaskOpacity, float);

private:
    RC ValidateParameters();
    RC GenRandomBVH
    (
        const UINT32 randSeed,
        const BoundingBox& rootAABB,
        const Lwca::Capability computeCap,
        const bool useMicromesh,
        vector<Complet>* pComplets,
        vector<TriangleRange>* pTriRanges,
        vector<DisplacedMicromesh>* pMicromeshes
    );
    RC GenerateBVH();
    RC RaysToJson
    (
        const vector<unique_ptr<TTUHit>>& r,
        const vector<Point>& o,
        const vector<Direction>& d
    ) const;
    RC SanityCheckTriRange
    (
        const TriangleRange& triRange,
        const vector<Triangle>& tris,
        const vector<Triangle>& endTris,
        const bool testMotionBlur,
        const bool expectHits,
        const Point& rayOrigin,
        const Lwca::Function& func,
        TTUGen::RandomState* const pRandState
    ) const;
    RC SanityCheckBVH
    (
        vector<Complet>& complets,
        const vector<TriangleRange>& triRanges,
        const QueryType queryType,
        const Point& rayOrigin,
        const Lwca::Function& func
    ) const;
    RC RunSanityChecks() const;

    TTUStressParams m_Params = {};
    Lwca::Module m_Module;
    Lwca::DeviceMemory m_DeviceTriangleRange;
    Lwca::HostMemory   m_HostTriangleRange;
    Lwca::DeviceMemory m_DeviceBVH;
    Lwca::HostMemory   m_HostBVH;
    Lwca::DeviceMemory m_DeviceBVHMicromesh;
    Lwca::HostMemory   m_HostBVHMicromesh;

    Lwca::DeviceMemory m_MiscompareCount;
    Lwca::HostMemory   m_HostMiscompareCount;
    Lwca::HostMemory   m_HostErrorLog;
    Lwca::HostMemory   m_DebugResult;

    Random m_RandomRay;
    map<UINT16, UINT64> m_SmidMiscompareCount;

    // JS properties
    UINT32 m_NumBlocks    = 0;
    UINT32 m_NumThreads   = 128;

    UINT32 m_RuntimeMs    = 5000;
    bool   m_KeepRunning  = false;
    UINT64 m_InnerIterations = 1024;
    bool   m_TestTriRange = true;
    bool   m_TestBVHTriangle = true;
    bool   m_TestBVHTriRange = true;
    bool   m_TestBVHMicromesh = false;
    bool   m_TestSanityCheck = false;
    UINT32 m_BVHWidth     = 8;
    UINT32 m_BVHDepth     = 4;
    float  m_BVHProbSplit = 1.0;
    UINT32 m_NumTris      = 21;
    UINT08 m_TriGroupSize  = 2;
    bool   m_UseMotionBlur = false;
    UINT08 m_Precision    = 32;
    UINT32 m_ErrorLogLen  = 8192;
    bool   m_DumpMiscompares = false;
    bool   m_ReportFailingSmids = true;
    UINT64 m_SmidFailureLimit = 1;
    bool   m_DumpToJson   = false;
    UINT32 m_RaysToDump   = 0;
    bool   m_ClusterRays  = true;
    bool   m_ShearComplets = false;
    float  m_VisibilityMaskOpacity = 0.0f;
};

JS_CLASS_INHERIT(LwdaTTUStress, LwdaStreamTest, "TTU Stress Test");
CLASS_PROP_READWRITE(LwdaTTUStress, RuntimeMs, UINT32,
                     "Run the kernel for at least the specified amount of time. "
                     "If RuntimeMs=0 TestConfiguration.Loops will be used.");
CLASS_PROP_READWRITE(LwdaTTUStress, KeepRunning, bool,
                     "While this is true, Run will continue even beyond RuntimeMs.");
CLASS_PROP_READWRITE(LwdaTTUStress, InnerIterations, UINT64,
                     "Number of iterations per ray in the TTU kernel");
CLASS_PROP_READWRITE(LwdaTTUStress, NumBlocks, UINT32,
                     "Number of kernel blocks to launch per loop");
CLASS_PROP_READWRITE(LwdaTTUStress, NumThreads, UINT32,
                     "Number of kernel threads to run per block");
CLASS_PROP_READWRITE(LwdaTTUStress, BVHWidth, UINT32,
                     "Width of randomly-generated BVH (number of children in node).");
CLASS_PROP_READWRITE(LwdaTTUStress, BVHDepth, UINT32,
                     "Depth of randomly-generated BVH. At depth=0 there are no complet children.");
CLASS_PROP_READWRITE(LwdaTTUStress, BVHProbSplit, float,
                     "Probabilty of generating a child node in TestBVH modes. "
                     "prob=1.0 results in a tree of uniform depth.");
CLASS_PROP_READWRITE(LwdaTTUStress, NumTris, UINT32,
                     "Number of triangles per TriangleRange");
CLASS_PROP_READWRITE(LwdaTTUStress, TriGroupSize, UINT08,
                     "Size of groups for shared triangle edges/verticies.");
CLASS_PROP_READWRITE(LwdaTTUStress, UseMotionBlur, bool,
                     "Enable motion blur of the BVH complets and triangles");
CLASS_PROP_READWRITE(LwdaTTUStress, Precision, UINT08,
                     "Compressed triangle vertex coord size in bits. "
                     "If 0, use uncompressed triangles");
CLASS_PROP_READWRITE(LwdaTTUStress, ErrorLogLen, UINT32,
                     "Max number of errors that can be logged with detailed information");
CLASS_PROP_READWRITE(LwdaTTUStress, DumpMiscompares, bool,
                     "Print out the T, U, V result data of miscomparing rays");
CLASS_PROP_READWRITE(LwdaTTUStress, ReportFailingSmids, bool,
                     "Report which SMID and HW TPC computed the miscomparing ray trace");
CLASS_PROP_READWRITE(LwdaTTUStress, SmidFailureLimit, UINT64,
                     "Lower bound of miscompares at which to report a SM/TPC as failing");
CLASS_PROP_READWRITE(LwdaTTUStress, DumpToJson, bool,
                     "Dump the complet and triangle data to the JSON log");
CLASS_PROP_READWRITE(LwdaTTUStress, RaysToDump, UINT32,
                     "Number of rays to dump if DumpToJson=true");
CLASS_PROP_READWRITE(LwdaTTUStress, TestTriRange, bool,
                     "Test triangle-range triangle intersection TTU queries");
CLASS_PROP_READWRITE(LwdaTTUStress, TestBVHTriangle, bool,
                     "Test complet traversal (BVH) queries intersecting against triangles");
CLASS_PROP_READWRITE(LwdaTTUStress, TestBVHTriRange, bool,
                     "Test complet traversal (BVH) queries intersecting against triangle ranges");
CLASS_PROP_READWRITE(LwdaTTUStress, TestBVHMicromesh, bool,
                     "Test complet traversal (BVH) queries intersecting against triangles in "
                     "displaced micromeshes");
CLASS_PROP_READWRITE(LwdaTTUStress, TestSanityCheck, bool,
                     "Performs a series of TTU sanity check queries to verify that ray tracing "
                     "is working as expected.");
CLASS_PROP_READWRITE(LwdaTTUStress, ClusterRays, bool,
                     "Rays of the same warp point in the same general direction");
CLASS_PROP_READWRITE(LwdaTTUStress, ShearComplets, bool,
                     "Complet AABBs are randomly sheared.");
CLASS_PROP_READWRITE(LwdaTTUStress, VisibilityMaskOpacity, float,
                     "Probability of a visibility mask micro triangle being opaque. "
                     "If 0, visibility masks are not used.");

bool LwdaTTUStress::IsSupported()
{
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }

    // WAR for fmodel: lwbugs/3416806
    if (!(Platform::GetSimulationMode() == Platform::SimulationMode::Fmodel &&
          GetBoundLwdaDevice().GetCapability() >= Lwca::Capability::SM_86 &&
          GetBoundLwdaDevice().GetCapability() <= Lwca::Capability::SM_89))
    {
        if (GetBoundGpuSubdevice()->GetRTCoreCount() == 0)
        {
            Printf(Tee::PriLow, "TTU is not supported on this board SKU\n");
            return false;
        }
    }

    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap != Lwca::Capability::SM_75 &&
        cap != Lwca::Capability::SM_86 &&
        cap != Lwca::Capability::SM_87 &&
        cap != Lwca::Capability::SM_89)
    {
        Printf(Tee::PriLow, "LwdaTTUStress does not support this SM version\n");
        return false;
    }

    return true;
}

void LwdaTTUStress::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);

    Printf(pri, "LwdaTTUStress Js Properties:\n");
    Printf(pri, "\tRuntimeMs:                      %u\n", m_RuntimeMs);
    Printf(pri, "\tKeepRunning:                    %s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "\tInnerIterations:                %llu\n", m_InnerIterations);
    Printf(pri, "\tNumBlocks:                      %u\n", m_NumBlocks);
    Printf(pri, "\tNumThreads:                     %u\n", m_NumThreads);
    Printf(pri, "\tTestTriRange:                   %s\n", m_TestTriRange ? "true" : "false");
    Printf(pri, "\tTestBVHTriangle:                %s\n", m_TestBVHTriangle ? "true" : "false");
    Printf(pri, "\tTestBVHTriRange:                %s\n", m_TestBVHTriRange ? "true" : "false");
    Printf(pri, "\tTestBVHMicromesh:               %s\n", m_TestBVHMicromesh ? "true" : "false");
    Printf(pri, "\tTestSanityCheck:                %s\n", m_TestSanityCheck ? "true" : "false");
    Printf(pri, "\tBVHWidth:                       %u\n", m_BVHWidth);
    Printf(pri, "\tBVHDepth:                       %u\n", m_BVHDepth);
    Printf(pri, "\tBVHProbSplit:                   %f\n", m_BVHProbSplit);
    Printf(pri, "\tNumTris:                        %u\n", m_NumTris);
    Printf(pri, "\tTriGroupSize:                   %u\n", m_TriGroupSize);
    Printf(pri, "\tUseMotionBlur:                  %s\n", m_UseMotionBlur ? "true" : "false");
    Printf(pri, "\tPrecision:                      %u\n", m_Precision);
    Printf(pri, "\tErrorLogLen:                    %u\n", m_ErrorLogLen);
    Printf(pri, "\tDumpMiscompares:                %s\n", m_DumpMiscompares ? "true" : "false");
    Printf(pri, "\tReportFailingSmids:             %s\n", m_ReportFailingSmids ? "true" : "false");
    Printf(pri, "\tClusterRays:                    %s\n", m_ClusterRays ? "true" : "false");
    Printf(pri, "\tShearComplets:                  %s\n", m_ShearComplets ? "true" : "false");
    Printf(pri, "\tVisibilityMaskOpacity:          %f\n", m_VisibilityMaskOpacity);
    if (m_ReportFailingSmids)
    {
        Printf(pri, "\tSmidFailureLimit:               %llu\n", m_SmidFailureLimit);
    }
    Printf(pri, "\tDumpToJson:                     %s\n", m_DumpToJson ? "true" : "false");
    if (m_DumpToJson)
    {
        Printf(pri, "\tRaysToDump:                     %d\n", m_RaysToDump);
    }
}

RC LwdaTTUStress::ValidateParameters()
{
    RC rc;
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();

    // Make sure we test _something_
    const bool anyTestEnabled =
        m_TestTriRange || m_TestBVHTriangle || m_TestBVHTriRange || m_TestBVHMicromesh;
    if (!(m_TestSanityCheck || anyTestEnabled))
    {
        Printf(Tee::PriError,
               "At least one test mode must be true. Available test modes: TestTriRange, "
               "TestBVHTriangle, TestBVHTriRange, TestBVHMicromesh, TestSanityCheck\n");
        return RC::BAD_PARAMETER;
    }

    if (m_TestBVHMicromesh && cap < Lwca::Capability::SM_89)
    {
        Printf(Tee::PriError, "TestBVHMicromesh is not supported on SM %d.%d\n",
               cap.MajorVersion(), cap.MinorVersion());
        return RC::BAD_PARAMETER;
    }

    // Check for MotionBlur capability if in use
    if (m_UseMotionBlur && cap < Lwca::Capability::SM_86)
    {
        Printf(Tee::PriError, "Motion blur is not supported on SM %d.%d\n",
               cap.MajorVersion(), cap.MinorVersion());
        return RC::BAD_PARAMETER;
    }

    if (m_ShearComplets && cap < Lwca::Capability::SM_89)
    {
        Printf(Tee::PriError, "Sheared complets are not supported on SM %d.%d\n",
               cap.MajorVersion(), cap.MinorVersion());
        return RC::BAD_PARAMETER;
    }

    if (m_VisibilityMaskOpacity < 0.0f || m_VisibilityMaskOpacity > 1.0f)
    {
        Printf(Tee::PriError, "VisibilityMaskOpacity must be between 0.0 and 1.0 inclusive.\n");
        return RC::BAD_PARAMETER;
    }

    if (m_VisibilityMaskOpacity > 0.0f)
    {
        if (cap < Lwca::Capability::SM_89)
        {
            Printf(Tee::PriError, "Visibility masks are not supported on SM %d.%d\n",
                   cap.MajorVersion(), cap.MinorVersion());
            return RC::BAD_PARAMETER;
        }
    }

    if (m_TriGroupSize < 1 || m_TriGroupSize > 4)
    {
        Printf(Tee::PriError, "TriGroupSize must be between 1 and 4 inclusive.\n");
        return RC::BAD_PARAMETER;
    }

    if (m_BVHProbSplit < 0.0f || m_BVHProbSplit > 1.0f)
    {
        Printf(Tee::PriError, "BVHProbSplit must be between 0.0 and 1.0 inclusive.\n");
        return RC::BAD_PARAMETER;
    }

    // Validate BVHWidth
    UINT32 maxChildren = (cap >= Lwca::Capability::SM_86) ? 11 : 10;
    if (m_UseMotionBlur)
    {
        maxChildren /= 2;
    }
    if (m_BVHWidth > maxChildren)
    {
        Printf(Tee::PriError,
               "BVHWidth (%d) > max (%d on SM %d.%d when motion blur is %s)\n",
               m_BVHWidth, maxChildren, cap.MajorVersion(), cap.MinorVersion(),
               m_UseMotionBlur ? "enabled" : "disabled");
        return RC::BAD_PARAMETER;
    }
    MASSERT(maxChildren <= Complet::s_MaxChildren);

    // Validate Precision
    if (m_Precision == 0)
    {
        if (m_UseMotionBlur)
        {
            Printf(Tee::PriError,
                   "Triangle compression (Precision > 0) is required for UseMotionBlur.\n");
            return RC::BAD_PARAMETER;
        }

        if (m_TriGroupSize > 1)
        {
            Printf(Tee::PriWarn, "To take advantage of increased triangle lwlling rate with "
                   "TriGroupSize > 1, triangle compression (Precision > 0) should be enabled.\n");
        }

        if (m_VisibilityMaskOpacity > 0.0f)
        {
            Printf(Tee::PriError, "Triangle compression (Precision > 0) is required to use "
                                  "visibility masks (VisibilityMaskOpacity > 0.0).\n");
            return RC::BAD_PARAMETER;
        }
    }

    if (m_Precision > 32)
    {
        Printf(Tee::PriError, "Precision must be <= 32 bits\n");
        return RC::BAD_PARAMETER;
    }

    return rc;
}

RC LwdaTTUStress::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());

    // Set kernel geometry if required
    if (!m_NumBlocks)
    {
        m_NumBlocks = GetBoundLwdaDevice().GetShaderCount();
    }
    if (!m_NumThreads)
    {
        m_NumThreads =
            GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);
    }
    if (!m_RaysToDump)
    {
        m_RaysToDump = m_NumThreads * m_NumBlocks;
    }

    // Validate LwdaTTUStress arguments
    CHECK_RC(ValidateParameters());

    // Initialize module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("ttustress", &m_Module));

    const bool anyTestEnabled =
        m_TestTriRange || m_TestBVHTriangle || m_TestBVHTriRange || m_TestBVHMicromesh;
    if (anyTestEnabled)
    {
        // Allocate miscompare count / error log
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(sizeof(UINT64), &m_MiscompareCount));
        CHECK_RC(GetLwdaInstance().AllocHostMem(sizeof(UINT64), &m_HostMiscompareCount));
        CHECK_RC(GetLwdaInstance().AllocHostMem(m_ErrorLogLen * sizeof(TTUError),
                                                &m_HostErrorLog));

        // Set kernel parameters
        m_Params = {};
        m_Params.errorCountPtr = m_MiscompareCount.GetDevicePtr();
        m_Params.errorLogPtr = m_HostErrorLog.GetDevicePtr(GetBoundLwdaDevice());
        m_Params.errorLogLen = m_ErrorLogLen;
        m_Params.iterations  = m_InnerIterations;
        m_Params.clusterRays = static_cast<UINT32>(m_ClusterRays);

        // Allocate memory for ray debugging if requested
        if (m_DumpToJson)
        {
            CHECK_RC(GetLwdaInstance().AllocHostMem(
                m_RaysToDump * (sizeof(Retval) + sizeof(Point) + sizeof(Direction)),
                &m_DebugResult));
            CHECK_RC(m_DebugResult.Clear());
            m_Params.debugResultPtr = m_DebugResult.GetDevicePtr(GetBoundLwdaDevice());
            m_Params.raysToDump = m_RaysToDump;
        }
        else
        {
            m_Params.debugResultPtr = reinterpret_cast<device_ptr>(nullptr);
            m_Params.raysToDump = 0;
        }

        CHECK_RC(GenerateBVH());
    }

    return rc;
}

//! \brief Randomly generate a BVH based around the provided root bounding box
//!
//! Reset Test RNG using MODS seed
//!
//! \param randSeed : seed for the random number generators
//! \param rootAABB : boundingBox for root Complet
//! \param computeCap : LWCA Compute Capability
//! \param useMicromesh : if true, pMicromeshes will be used, otherwise
//!                       pTriRanges is used
//! \param pComplets : pointer to vector of Complets we are generating
//!                    Vector must be empty
//! \param pTriRanges : pointer to vector of Triangle Ranges we are generating
//!                     Vector must be empty
//! \param pMicromeshes : pointer to vector of Micromesh we are generating.
//!                       Vector must be empty
//!
RC LwdaTTUStress::GenRandomBVH
(
    const UINT32 randSeed,
    const BoundingBox& rootAABB,
    const Lwca::Capability computeCap,
    const bool useMicromesh,
    vector<Complet>* pComplets,
    vector<TriangleRange>* pTriRanges,
    vector<DisplacedMicromesh>* pMicromeshes
)
{
    MASSERT(pComplets && pComplets->size() == 0);
    MASSERT(useMicromesh ?
            (pMicromeshes && pMicromeshes->size() == 0) :
            (pTriRanges && pTriRanges->size() == 0));
    RC rc;

    // Init RNG
    TTUGen::RandomState randState;
    randState.randomGen.SeedRandom(randSeed);
    randState.randomTri.SeedRandom(randSeed + 1);
    randState.randomTriEnd.SeedRandom(randSeed + 2);

    // Reserve memory for Triangle Ranges and Complets
    UINT32 numTriRanges = 0;
    UINT32 numMicromeshes = 0;
    UINT32 numComplets = 0;
    UINT32 rngAccGen = 0;
    UINT32 rngAccTri = 0;
    UINT32 rngAccTriEnd = 0;
    TTUGen::GenRandomShearDataA(m_ShearComplets, &randState, &rngAccGen);
    TTUGen::GenRandomBVHPartA(m_BVHWidth, m_BVHDepth, m_BVHProbSplit, m_NumTris, m_TriGroupSize,
                              m_UseMotionBlur, m_ShearComplets, m_VisibilityMaskOpacity,
                              useMicromesh, &numTriRanges, &numMicromeshes, &numComplets,
                              &randState, &rngAccGen, &rngAccTri, &rngAccTriEnd);
    const UINT32 postARngValComplet = randState.randomGen.GetRandom();
    const UINT32 postARngValTri     = randState.randomTri.GetRandom();
    const UINT32 postARngValTriEnd  = randState.randomTriEnd.GetRandom();
    VerbosePrintf("BVH Complets: %d\n", numComplets);
    if (useMicromesh)
    {
        VerbosePrintf("BVH Micromeshes: %d\nBVH MicroTriangles: %d\n", numMicromeshes,
                      numMicromeshes *
                      DisplacedMicromesh::s_SubTrisPerBaseTri *
                      DisplacedMicromesh::s_DispBlockResolution);
        pMicromeshes->reserve(numMicromeshes);
    }
    else
    {
        VerbosePrintf("BVH TriRanges: %d\nBVH Triangles: %d\n", numTriRanges,
                      numTriRanges * m_NumTris);
        pTriRanges->reserve(numTriRanges);
    }
    pComplets->reserve(numComplets);

    // Reset RNG (since we need to match the result from PartA)
    randState.randomGen.SeedRandom(randSeed);
    randState.randomTri.SeedRandom(randSeed + 1);
    randState.randomTriEnd.SeedRandom(randSeed + 2);

    // Add root complet
    const ShearData shearData = TTUGen::GenRandomShearDataB(m_ShearComplets, &randState);
    if (useMicromesh)
    {
        pComplets->emplace_back(computeCap, ChildFormat::Standard, LeafType::DisplacedMicromesh,
                                rootAABB, true, shearData);
    }
    else
    {
        const ChildFormat format =
            (m_UseMotionBlur) ? ChildFormat::MotionBlur : ChildFormat::Standard;
        pComplets->emplace_back(computeCap, format, LeafType::TriRange, rootAABB,
                                true, shearData);
    }

    // Generate BVH
    UINT32 triId = 0;
    CHECK_RC(TTUGen::GenRandomBVHPartB(&pComplets->front(), computeCap, m_BVHWidth, m_BVHDepth,
                                       m_BVHProbSplit, m_NumTris, m_TriGroupSize, m_Precision,
                                       m_UseMotionBlur, m_ShearComplets, m_VisibilityMaskOpacity,
                                       useMicromesh, pTriRanges, pMicromeshes, pComplets, &triId,
                                       &randState));
    const UINT32 postBRngValComplet = randState.randomGen.GetRandom();
    const UINT32 postBRngValTri     = randState.randomTri.GetRandom();
    const UINT32 postBRngValTriEnd  = randState.randomTriEnd.GetRandom();

    // Check that PartA and PartB advance the RNG in the same manner
    // (We also always need to print rngAcc to prevent compiler optimizations)
    Printf(Tee::PriDebug, "General RNG Acc Value: %d\n", rngAccGen);
    Printf(Tee::PriDebug, "Tri     RNG Acc Value: %d\n", rngAccTri);
    Printf(Tee::PriDebug, "TriEnd  RNG Acc Value: %d\n", rngAccTriEnd);
    if (postBRngValComplet != postARngValComplet)
    {
        Printf(Tee::PriError, "General RNG mismatch between PartA and PartB of BVH generation\n"
                              "Both parts should advance the RNG in the same manner\n");
        return RC::SOFTWARE_ERROR;
    }
    if (postBRngValTri != postARngValTri)
    {
        Printf(Tee::PriError, "Tri RNG mismatch between PartA and PartB of BVH generation\n"
                              "Both parts should advance the RNG in the same manner\n");
        return RC::SOFTWARE_ERROR;
    }
    if (postBRngValTriEnd != postARngValTriEnd)
    {
        Printf(Tee::PriError, "TriEnd RNG mismatch between PartA and PartB of BVH generation\n"
                              "Both parts should advance the RNG in the same manner\n");
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(useMicromesh ?
            (pMicromeshes->size() == numMicromeshes) :
            (pTriRanges->size() == numTriRanges));
    MASSERT(pComplets->size() == numComplets);

    return rc;
}

RC LwdaTTUStress::GenerateBVH()
{
    RC rc;

    const Lwca::Capability computeCap = GetBoundLwdaDevice().GetCapability();
    const UINT32 randSeed = GetTestConfiguration()->Seed();
    const BoundingBox rootAABB =
    {
        -4.0f / 3.0f,
        -4.0f / 3.0f,
        -4.0f / 3.0f,
        +4.0f / 3.0f,
        +4.0f / 3.0f,
        +4.0f / 3.0f,
    };

    // ==============================
    // Init Standalone Triangle Range
    // ==============================

    // Initialize test triangle data
    if (m_TestTriRange)
    {
        const bool useVM = m_VisibilityMaskOpacity > 0.0f;
        TriangleRange triRange(m_UseMotionBlur, useVM);
        UINT32 triId = 0;

        TTUGen::RandomState randState;
        randState.randomGen.SeedRandom(randSeed);
        randState.randomTri.SeedRandom(randSeed + 1);
        randState.randomTriEnd.SeedRandom(randSeed + 2);
        CHECK_RC(TTUGen::AddTrisUniformB(&triRange, rootAABB, m_NumTris, m_TriGroupSize,
                                         m_UseMotionBlur, m_Precision, &triId, &randState));

        if (useVM)
        {
            for (UINT32 i = 0; i < m_NumTris; i++)
            {
                VisibilityBlock vBlock =
                    TTUGen::GelwisibilityBlockB(m_VisibilityMaskOpacity, &randState);
                triRange.AddVisibilityBlock(vBlock);
            }
        }

        m_Params.numLines = triRange.NumLines();

        // Allocate space for TriRange data and visibility blocks
        const UINT32 triRangeSize = triRange.SizeBytes();
        const UINT32 totalSize = triRangeSize + triRange.VisibilityBlocksSizeBytes();

        CHECK_RC(GetLwdaInstance().AllocHostMem(totalSize, &m_HostTriangleRange));
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(totalSize, &m_DeviceTriangleRange));
        VerbosePrintf("TriRange Data: %u Bytes\n", totalSize);

        // Visibility blocks are placed in memory after the triRange. Note: this is different
        // from Complet::SerializeBVHAbs where all of a complet's leaves' vBlocks are placed
        // before complet leaves (tri ranges and micromeshes)
        triRange.Serialize(static_cast<UINT08*>(m_HostTriangleRange.GetPtr()),
                           static_cast<UINT08*>(m_HostTriangleRange.GetPtr()) + triRangeSize,
                           m_DeviceTriangleRange.GetDevicePtr() + triRangeSize);

        // Copy TriRange data to framebuffer
        CHECK_RC(m_DeviceTriangleRange.Set(m_HostTriangleRange.GetPtr(),
                                           m_HostTriangleRange.GetSize()));
    }

    // ===================================
    // Init BVH containing Triangle Ranges
    // ===================================

    if (m_TestBVHTriangle || m_TestBVHTriRange)
    {
        // Generate BVH
        vector<TriangleRange> triRanges;
        vector<Complet> complets;
        CHECK_RC(GenRandomBVH(randSeed, rootAABB, computeCap, false, &complets, &triRanges,
                              nullptr));

        // Allocate enough memory for the BVH
        const auto bvhSize = complets[0].BVHSizeBytes();
        CHECK_RC(GetLwdaInstance().AllocHostMem(bvhSize, &m_HostBVH));
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(bvhSize, &m_DeviceBVH));
        VerbosePrintf("BVH Data: %llu Bytes (%llu MB)\n", bvhSize, bvhSize / (1u << 20));

        // Relwrsively set absolute pointers in BVH and serialize data
        UINT64 offset = complets[0].SerializeBVHAbs(m_DeviceBVH.GetDevicePtr(),
                                                    static_cast<UINT08*>(m_HostBVH.GetPtr()));
        if (offset != m_HostBVH.GetSize())
        {
            MASSERT(offset == m_HostBVH.GetSize());
            Printf(Tee::PriError,
                   "BVH serialization error. Final Offset %llu > Expected Size %zu\n",
                   offset, m_HostBVH.GetSize());
            return RC::SOFTWARE_ERROR;
        }

        // Copy BVH data to framebuffer
        CHECK_RC(m_DeviceBVH.Set(m_HostBVH.GetPtr(), m_HostBVH.GetSize()));

        // Dump BVH to JSON if requested
        if (m_DumpToJson)
        {
            for (const auto& triRange : triRanges)
            {
                CHECK_RC(triRange.DumpToJson());
            }
            for (const auto& complet : complets)
            {
                CHECK_RC(complet.DumpToJson());
            }
        }
    }

    // =========================================
    // Init BVH containing Displaced Micromeshes
    // =========================================

    if (m_TestBVHMicromesh)
    {
        vector<Complet> complets;
        vector<DisplacedMicromesh> micromeshes;
        CHECK_RC(GenRandomBVH(randSeed, rootAABB, computeCap, true, &complets, nullptr,
                              &micromeshes));

        // Allocate enough memory for the BVH
        const auto bvhSize = complets[0].BVHSizeBytes();
        CHECK_RC(GetLwdaInstance().AllocHostMem(bvhSize, &m_HostBVHMicromesh));
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(bvhSize, &m_DeviceBVHMicromesh));
        VerbosePrintf("BVH Data: %llu Bytes (%llu MB)\n", bvhSize, bvhSize / (1u << 20));

        // Relwrsively set absolute pointers in BVH and serialize data
        UINT64 offset = 
            complets[0].SerializeBVHAbs(m_DeviceBVHMicromesh.GetDevicePtr(),
                                        static_cast<UINT08*>(m_HostBVHMicromesh.GetPtr()));
        if (offset != m_HostBVHMicromesh.GetSize())
        {
            MASSERT(offset == m_HostBVHMicromesh.GetSize());
            Printf(Tee::PriError,
                   "BVH serialization error. Final Offset %llu > Expected Size %zu\n",
                   offset, m_HostBVHMicromesh.GetSize());
            return RC::SOFTWARE_ERROR;
        }

        // Copy BVH data to framebuffer
        CHECK_RC(m_DeviceBVHMicromesh.Set(m_HostBVHMicromesh.GetPtr(),
                                          m_HostBVHMicromesh.GetSize()));

        // Dump BVH to JSON if requested
        if (m_DumpToJson)
        {
            for (const auto& complet : complets)
            {
                CHECK_RC(complet.DumpToJson());
            }
            for (const auto& micromesh : micromeshes)
            {
                CHECK_RC(micromesh.DumpToJson());
            }
        }
    }

    return rc;
}

RC LwdaTTUStress::RaysToJson
(
    const vector<unique_ptr<TTUHit>>& r,
    const vector<Point>& o,
    const vector<Direction>& d
) const
{
    RC rc;
    JavaScriptPtr pJs;

    for (UINT32 i = 0; i < r.size(); i++)
    {
        JsonItem jsi = JsonItem();
        jsi.SetTag("Ray");
        switch (r[i]->hitType)
        {
            case HitType::Triangle:
            {
                const HitTriangle* pTriHit = dynamic_cast<HitTriangle*>(r[i].get());
                MASSERT(pTriHit);

                JsArray jshit(1);
                pJs->ToJsval(pTriHit->t, &jshit[0]);
                jsi.SetField("hit", &jshit);
                jsi.SetField("triId", pTriHit->triId);
                break;
            }
            case HitType::TriRange:
            {
                const HitTriRange* pTriRangeHit = dynamic_cast<HitTriRange*>(r[i].get());
                MASSERT(pTriRangeHit);

                JsArray jshit(1);
                pJs->ToJsval(pTriRangeHit->t, &jshit[0]);
                jsi.SetField("hit", &jshit);
                jsi.SetField("triRangePtr", pTriRangeHit->triRangePtr);
                break;
            }
            case HitType::None:
            {
                JsArray jshit(1);
                pJs->ToJsval(-1.0, &jshit[0]);
                jsi.SetField("hit", &jshit);
                jsi.SetField("triId", -1);
                break;
            }
            default:
            {
                Printf(Tee::PriError, "Dumping of hit type [%s] not supported!",
                       r[i]->ToStr().c_str());
                return RC::SOFTWARE_ERROR;
            }
        }

        JsArray jsori(3);
        pJs->ToJsval(o[i].x, &jsori[0]);
        pJs->ToJsval(o[i].y, &jsori[1]);
        pJs->ToJsval(o[i].z, &jsori[2]);
        jsi.SetField("ori", &jsori);

        JsArray jsdir(3);
        pJs->ToJsval(d[i].x, &jsdir[0]);
        pJs->ToJsval(d[i].y, &jsdir[1]);
        pJs->ToJsval(d[i].z, &jsdir[2]);
        jsi.SetField("dir", &jsdir);

        CHECK_RC(jsi.WriteToLogfile());
    }
    return rc;
}

RC LwdaTTUStress::SanityCheckTriRange(const TriangleRange& triRange, const vector<Triangle>& tris,
                                      const vector<Triangle>& endTris, const bool testMotionBlur,
                                      const bool expectHits, const Point& rayOrigin,
                                      const Lwca::Function& func,
                                      TTUGen::RandomState* const pRandState) const
{
    if (testMotionBlur)
    {
        MASSERT(tris.size() == endTris.size());
    }

    RC rc;
    TTUSingleRayParams params = {};
    params.queryType = QueryType::TriRange_Triangle;
    params.numLines = triRange.NumLines();
    params.timestamp = 0;
    params.rayOrigin = rayOrigin;

    // Prepare result pointer
    Lwca::HostMemory singleRayResult;
    CHECK_RC(GetLwdaInstance().AllocHostMem(sizeof(Retval), &singleRayResult));
    CHECK_RC(singleRayResult.Clear());
    params.retvalPtr = singleRayResult.GetDevicePtr(GetBoundLwdaDevice());

    // Prepare input data
    Lwca::HostMemory hostMem;
    Lwca::DeviceMemory deviceMem;
    const UINT32 triRangeSize = triRange.SizeBytes();
    const UINT32 totalSize = triRangeSize + triRange.VisibilityBlocksSizeBytes();
    CHECK_RC(GetLwdaInstance().AllocHostMem(totalSize, &hostMem));
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(totalSize, &deviceMem));
    triRange.Serialize(static_cast<UINT08*>(hostMem.GetPtr()),
                       static_cast<UINT08*>(hostMem.GetPtr()) + triRangeSize,
                       deviceMem.GetDevicePtr() + triRangeSize);
    CHECK_RC(deviceMem.Set(hostMem.GetPtr(), totalSize));
    params.dataPtr = deviceMem.GetDevicePtr();

    // Test rays pointed towards and away from each triangle
    for (UINT32 i = 0; i < tris.size(); i++)
    {
        Triangle targetTri = tris[i];
        if (testMotionBlur)
        {
            MASSERT(pRandState);
            const float timestamp = pRandState->randomGen.GetRandomFloat(0.0f, 1.0f);
            params.timestamp = timestamp;
            targetTri = TTUUtils::InterpolateTris(tris[i], endTris[i], timestamp);
        }
        const Point target = TTUUtils::GetTriCentroid(targetTri);
        Direction rayDir = {};
        rayDir.x = target.x - rayOrigin.x;
        rayDir.y = target.y - rayOrigin.y;
        rayDir.z = target.z - rayOrigin.z;

        // Launch ray towards
        params.rayDir = rayDir;
        CHECK_RC(singleRayResult.Clear());
        CHECK_RC(func.Launch(params));
        GetLwdaInstance().Synchronize();

        // Make sure we hit something (if expectHits is set)
        // TODO: Lwrrently does not check triId, because the
        // ray might have hit a closer triangle on its way to targetTri.
        const Retval* pTowardResult = reinterpret_cast<Retval*>(singleRayResult.GetPtr());
        const unique_ptr<TTUHit> pTowardHit = TTUHit::GetHit(*pTowardResult);
        if (expectHits)
        {
            if (pTowardHit->hitType != HitType::Triangle)
            {
                Printf(Tee::PriError, "Expected any triangle hit, got: %s\n",
                       pTowardHit->ToStr().c_str());
                return RC::GPU_COMPUTE_MISCOMPARE;
            }
        }
        else
        {
            if (pTowardHit->hitType != HitType::None)
            {
                Printf(Tee::PriError, "Unexpected hit type, got: %s\n",
                       pTowardHit->ToStr().c_str());
                return RC::GPU_COMPUTE_MISCOMPARE;
            }
        }

        // Launch ray away
        params.rayDir = {-rayDir.x, -rayDir.y, -rayDir.z};
        CHECK_RC(singleRayResult.Clear());
        CHECK_RC(func.Launch(params));
        GetLwdaInstance().Synchronize();

        // We expect no hits, but we might hit a triangle that isn't targetTri.
        const Retval* pAwayResult = reinterpret_cast<Retval*>(singleRayResult.GetPtr());
        const unique_ptr<TTUHit> pAwayHit = TTUHit::GetHit(*pAwayResult);
        if (pAwayHit->hitType != HitType::None)
        {
            if (pAwayHit->hitType == HitType::Triangle)
            {
                const HitTriangle* const pHitTri = static_cast<HitTriangle*>(pAwayHit.get());
                if (targetTri.triId == pHitTri->triId)
                {
                    Printf(Tee::PriError, "Unexpected hit on triId %u, got: %s\n", targetTri.triId,
                           pAwayHit->ToStr().c_str());
                    return RC::GPU_COMPUTE_MISCOMPARE;
                }
            }
            else
            {
                Printf(Tee::PriError, "Unexpected hit type, got: %s\n", pAwayHit->ToStr().c_str());
                return RC::GPU_COMPUTE_MISCOMPARE;
            }
        }
    }

    return rc;
}

RC LwdaTTUStress::SanityCheckBVH(vector<Complet>& complets, const vector<TriangleRange>& triRanges,
                                 const QueryType queryType, const Point& rayOrigin,
                                 const Lwca::Function& func) const
{
    RC rc;
    TTUSingleRayParams params = {};
    params.queryType = queryType;
    params.rayOrigin = rayOrigin;

    // Prepare result pointer
    Lwca::HostMemory singleRayResult;
    CHECK_RC(GetLwdaInstance().AllocHostMem(sizeof(Retval), &singleRayResult));
    CHECK_RC(singleRayResult.Clear());
    params.retvalPtr = singleRayResult.GetDevicePtr(GetBoundLwdaDevice());

    // Prepare input data
    Lwca::HostMemory hostMem;
    Lwca::DeviceMemory deviceMem;
    const UINT64 totalSize = complets[0].BVHSizeBytes();
    CHECK_RC(GetLwdaInstance().AllocHostMem(totalSize, &hostMem));
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(totalSize, &deviceMem));
    const UINT64 offset = complets[0].SerializeBVHAbs(deviceMem.GetDevicePtr(),
                                                      static_cast<UINT08*>(hostMem.GetPtr()));
    if (offset != totalSize)
    {
        Printf(Tee::PriError,
               "BVH serialization error. Offset: %llu, Expected: %llu\n", offset, totalSize);
        MASSERT(offset != totalSize);
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(deviceMem.Set(hostMem.GetPtr(), totalSize));
    params.dataPtr = deviceMem.GetDevicePtr();

    // Test rays pointed towards and away from each triangle/triRange
    for (const TriangleRange& targetTriRange : triRanges)
    {
        for (const Triangle& targetTri : targetTriRange.GetTris())
        {
            const Point target = TTUUtils::GetTriCentroid(targetTri);
            Direction rayDir = {};
            rayDir.x = target.x - rayOrigin.x;
            rayDir.y = target.y - rayOrigin.y;
            rayDir.z = target.z - rayOrigin.z;

            // Launch ray towards
            params.rayDir = rayDir;
            CHECK_RC(singleRayResult.Clear());
            CHECK_RC(func.Launch(params));
            GetLwdaInstance().Synchronize();

            // Make sure we hit something
            // TODO: Lwrrently does not check if we hit the correct triangle/triRange
            // because the ray might have hit something closer.
            const Retval* pTowardResult = reinterpret_cast<Retval*>(singleRayResult.GetPtr());
            const unique_ptr<TTUHit> pTowardHit = TTUHit::GetHit(*pTowardResult);
            if (queryType == QueryType::Complet_TriRange)
            {
                if (pTowardHit->hitType != HitType::TriRange)
                {
                    Printf(Tee::PriError, "Expected any TriRange hit, got: %s\n",
                           pTowardHit->ToStr().c_str());
                    return RC::GPU_COMPUTE_MISCOMPARE;
                }
            }
            else
            {
                if (pTowardHit->hitType != HitType::Triangle)
                {
                    Printf(Tee::PriError, "Expected any Triangle hit, got: %s\n",
                           pTowardHit->ToStr().c_str());
                    return RC::GPU_COMPUTE_MISCOMPARE;
                }
            }

            // Launch ray away
            params.rayDir = {-rayDir.x, -rayDir.y, -rayDir.z};
            CHECK_RC(singleRayResult.Clear());
            CHECK_RC(func.Launch(params));
            GetLwdaInstance().Synchronize();

            // We expect no hits, but we might hit something on the way to targetTri.
            const Retval* pAwayResult = reinterpret_cast<Retval*>(singleRayResult.GetPtr());
            const unique_ptr<TTUHit> pAwayHit = TTUHit::GetHit(*pAwayResult);
            const HitType awayHitType = pAwayHit->hitType;
            if (queryType == QueryType::Complet_TriRange)
            {
                // TODO: Lwrrently does not check that hit triRange != targetTriRange
                if (awayHitType != HitType::None && awayHitType != HitType::TriRange)
                {
                    Printf(Tee::PriError, "Unexpected hit type, got: %s\n",
                           pAwayHit->ToStr().c_str());
                    return RC::GPU_COMPUTE_MISCOMPARE;
                }
            }
            else
            {
                if (awayHitType == HitType::Triangle)
                {
                    const HitTriangle* const pHitTri = static_cast<HitTriangle*>(pAwayHit.get());
                    if (targetTri.triId == pHitTri->triId)
                    {
                        Printf(Tee::PriError, "Unexpected hit on triId %u, got: %s\n",
                               targetTri.triId, pAwayHit->ToStr().c_str());
                        return RC::GPU_COMPUTE_MISCOMPARE;
                    }
                }
                else if (awayHitType != HitType::None)
                {
                    Printf(Tee::PriError, "Unexpected hit type, got: %s\n",
                           pAwayHit->ToStr().c_str());
                    return RC::GPU_COMPUTE_MISCOMPARE;
                }
            }
        }
    }

    return rc;
}

RC LwdaTTUStress::RunSanityChecks() const
{
    RC rc;
    vector<RC> results;
    const BoundingBox bbox = { -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    // rayOrigin should be kept outside of bbox for checks
    // that expect ray misses to work properly
    const Point rayOrigin = {0.0f, 0.0f, 1.5f};

    TTUGen::RandomState randState;
    const UINT32 randSeed = GetTestConfiguration()->Seed();
    randState.randomGen.SeedRandom(randSeed);
    randState.randomTri.SeedRandom(randSeed + 1);
    randState.randomTriEnd.SeedRandom(randSeed + 2);

    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap == Lwca::Capability::SM_UNKNOWN)
    {
        return RC::SOFTWARE_ERROR;
    }

    Lwca::Function func = m_Module.GetFunction("TTUSingleRay");
    CHECK_RC(func.InitCheck());

    // Sanity Check 0 (Uncompressed TriRange):
    // Generates an uncompressed triangle range and tests
    // rays fired towards and away from each triangle.
    vector<Triangle> tris;
    vector<Triangle> endTris;
    TriangleRange triRange = TriangleRange(false, false);
    for (UINT32 i = 0; i < 21; i++)
    {
        tris.push_back(TTUGen::GenTriangleB(bbox, i, false, &randState));
        CHECK_RC(triRange.AddTri(tris[i], tris[i], 0));
    }
    results.push_back(SanityCheckTriRange(triRange, tris, endTris, false, true, rayOrigin,
                                          func, &randState));

    // Sanity Check 1 (Compressed TriRange):
    // Generates a compressed triangle range and tests
    // rays fired towards and away from each triangle.
    tris.clear();
    endTris.clear();
    triRange = TriangleRange(false, false);
    for (UINT32 i = 0; i < 21; i++)
    {
        tris.push_back(TTUGen::GenTriangleB(bbox, i, false, &randState));
        CHECK_RC(triRange.AddTri(tris[i], tris[i], 32));
    }
    results.push_back(SanityCheckTriRange(triRange, tris, endTris, false, true, rayOrigin,
                                          func, &randState));

    // Sanity Check 2 (Compressed TriRange + Motion Blur):
    // Generates a compressed triangle range with motion blur triangles
    // and tests rays fired towards and away from each triangle.
    if (cap < Lwca::Capability::SM_86)
    {
        results.push_back(RC::UNSUPPORTED_DEVICE);
    }
    else
    {
        tris.clear();
        endTris.clear();
        triRange = TriangleRange(true, false);
        for (UINT32 i = 0; i < 7; i++)
        {
            tris.push_back(TTUGen::GenTriangleB(bbox, i, false, &randState));
            endTris.push_back(TTUGen::GenTriangleB(bbox, i, true, &randState));
            CHECK_RC(triRange.AddTri(tris[i], endTris[i], 32));
        }
        results.push_back(SanityCheckTriRange(triRange, tris, endTris, true, true, rayOrigin,
                                              func, &randState));
    }

    // Sanity Check 3 (Compressed TriRange + Opaque VM):
    // Generates a compressed triangle range with fully opaque visibility masks
    // and tests rays fired towards and away from each triangle.
    if (cap < Lwca::Capability::SM_89)
    {
        results.push_back(RC::UNSUPPORTED_DEVICE);
    }
    else
    {
        tris.clear();
        endTris.clear();
        triRange = TriangleRange(false, true);
        for (UINT32 i = 0; i < 7; i++)
        {
            tris.push_back(TTUGen::GenTriangleB(bbox, i, false, &randState));
            CHECK_RC(triRange.AddTri(tris[i], tris[i], 32));
            VisibilityBlock vBlock = TTUGen::GelwisibilityBlockB(1.0f, &randState);
            triRange.AddVisibilityBlock(vBlock);
        }
        results.push_back(SanityCheckTriRange(triRange, tris, endTris, false, true, rayOrigin,
                                              func, &randState));
    }

    // Sanity Check 4 (Compressed TriRange + Transparent VM):
    // Generates a compressed triangle range with fully transparent visibility masks
    // and tests rays fired towards and away from each triangle.
    if (cap < Lwca::Capability::SM_89)
    {
        results.push_back(RC::UNSUPPORTED_DEVICE);
    }
    else
    {
        tris.clear();
        endTris.clear();
        triRange = TriangleRange(false, true);
        for (UINT32 i = 0; i < 7; i++)
        {
            tris.push_back(TTUGen::GenTriangleB(bbox, i, false, &randState));
            CHECK_RC(triRange.AddTri(tris[i], tris[i], 32));
            VisibilityBlock vBlock = TTUGen::GelwisibilityBlockB(0.0f, &randState);
            triRange.AddVisibilityBlock(vBlock);
        }
        results.push_back(SanityCheckTriRange(triRange, tris, endTris, false, false, rayOrigin,
                                              func, &randState));
    }

    // Sanity Check 5 (BVH TriRange, TriRange intersections):
    // Generates a BVH of complets with compressed triRange leaves (w/normal tris)
    // and tests rays fired towards and away from each triRange.
    vector<Complet> complets;
    complets.reserve(1 + 3 + 9 + 27);
    vector<TriangleRange> triRanges;
    triRanges.reserve(81);
    UINT32 triId = 0;
    complets.emplace_back(cap, ChildFormat::Standard, LeafType::TriRange, bbox, true,
                          TTUGen::GenRandomShearDataB(false, &randState));
    CHECK_RC(TTUGen::GenRandomBVHPartB(&complets.front(), cap, 3, 3, 1.0f, 21,
                                       4, 32, false, false, 0.0f, false, &triRanges,
                                       nullptr, &complets, &triId, &randState));
    results.push_back(SanityCheckBVH(complets, triRanges, QueryType::Complet_TriRange,
                                     rayOrigin, func));

    // Sanity Check 6 (BVH TriRange, Triangle intersections):
    // Generates a BVH of complets with compressed triRange leaves (w/normal tris)
    // and tests rays fired towards and away from each triangle.
    complets.clear();
    complets.reserve(1 + 3 + 9 + 27);
    triRanges.clear();
    triRanges.reserve(81);
    triId = 0;
    complets.emplace_back(cap, ChildFormat::Standard, LeafType::TriRange, bbox, true,
                          TTUGen::GenRandomShearDataB(false, &randState));
    CHECK_RC(TTUGen::GenRandomBVHPartB(&complets.front(), cap, 3, 3, 1.0f, 21,
                                       4, 32, false, false, 0.0f, false, &triRanges,
                                       nullptr, &complets, &triId, &randState));
    results.push_back(SanityCheckBVH(complets, triRanges, QueryType::Complet_Triangle,
                                     rayOrigin, func));

    // TODO Add more sanity checks:
    // Add check for complets with ShearComplets
    // Add check for complets with Displaced Micromeshes
    // Add check for complets with combinations of (MotionBlur, VM, ShearComplets, leafType)

    // Print results
    for (UINT32 i = 0; i < results.size(); i++)
    {
        Printf(Tee::PriNormal, "Sanity Check %u: ", i);
        switch (results[i])
        {
            case RC::OK:
                Printf(Tee::PriNormal, "Pass\n");
                break;
            case RC::UNSUPPORTED_DEVICE:
                Printf(Tee::PriNormal, "Skipped\n");
                break;
            default:
                Printf(Tee::PriNormal, "Fail\n");
        }
    }

    // Fail if any check failed
    for (const RC& result : results)
    {
        if (!(result == RC::OK || result == RC::UNSUPPORTED_DEVICE))
        {
            return result;
        }
    }
    return rc;
}

RC LwdaTTUStress::Run()
{
    RC rc;
    StickyRC stickyRc;
    const bool bPerpetualRun = m_KeepRunning;
    UINT64 kernLaunchCount = 0;
    double duration = 0.0f;

    if (m_TestSanityCheck)
    {
        CHECK_RC(RunSanityChecks());
    }

    // Early exit if no tests were requested (besides TestSanityCheck)
    const bool anyTestEnabled =
        m_TestTriRange || m_TestBVHTriangle || m_TestBVHTriRange || m_TestBVHMicromesh;
    if (!anyTestEnabled)
    {
        return rc;
    }

    // Re-seed RNG used for ray generation
    m_RandomRay.SeedRandom(GetTestConfiguration()->Seed());

    if (!bPerpetualRun)
    {
        if (m_RuntimeMs)
        {
            CHECK_RC(PrintProgressInit(m_RuntimeMs));
        }
        else
        {
            CHECK_RC(PrintProgressInit(GetTestConfiguration()->Loops()));
        }
    }

    // If KeepRunning is true, run at least once and keep running until it is false.
    // Otherwise run for RuntimeMs if it is set.
    // Otherwise run for TestConfiguration.Loops loops.
    for
    (
        UINT64 loop = 0;
        bPerpetualRun || (m_RuntimeMs ?
                          duration < static_cast<double>(m_RuntimeMs) :
                          loop < GetTestConfiguration()->Loops());
        loop++
    )
    {
        // Create events for timing kernel
        Lwca::Event startEvent(GetLwdaInstance().CreateEvent());
        Lwca::Event stopEvent(GetLwdaInstance().CreateEvent());

        // Clear error counter
        CHECK_RC(m_MiscompareCount.Clear());

        // Use a different RNG seed and timestamp for each loop
        m_Params.randSeed = m_RandomRay.GetRandom();

        // Get Run function
        Lwca::Function func =
            m_Module.GetFunction("TTUStress", m_NumBlocks, m_NumThreads);
        CHECK_RC(func.InitCheck());

        // Launch kernel, recording elaspsed time with events
        Printf(Tee::PriLow, "Launching Outer: %llu\n", loop);
        CHECK_RC(startEvent.Record());
        if (m_TestTriRange)
        {
            // Run only ray-intersect test
            m_Params.triRangePtr = m_DeviceTriangleRange.GetDevicePtr();
            m_Params.completPtr  = reinterpret_cast<device_ptr>(nullptr);
            m_Params.queryType   = QueryType::TriRange_Triangle;
            CHECK_RC(func.Launch(m_Params));
            kernLaunchCount++;
        }
        if (m_TestBVHTriangle)
        {
            // Run BVH traversal with triangle intersection
            m_Params.triRangePtr = reinterpret_cast<device_ptr>(nullptr);
            m_Params.completPtr  = m_DeviceBVH.GetDevicePtr();
            m_Params.queryType   = QueryType::Complet_Triangle;
            CHECK_RC(func.Launch(m_Params));
            kernLaunchCount++;
        }
        if (m_TestBVHTriRange)
        {
            // Run standalone BVH traversal without triangle intersection
            m_Params.triRangePtr = reinterpret_cast<device_ptr>(nullptr);
            m_Params.completPtr  = m_DeviceBVH.GetDevicePtr();
            m_Params.queryType   = QueryType::Complet_TriRange;
            CHECK_RC(func.Launch(m_Params));
            kernLaunchCount++;
        }
        if (m_TestBVHMicromesh)
        {
            // Run BVH traversal with triangle intersections on displaced micromeshes
            m_Params.triRangePtr = reinterpret_cast<device_ptr>(nullptr);
            m_Params.completPtr  = m_DeviceBVHMicromesh.GetDevicePtr();
            m_Params.queryType   = QueryType::Complet_Triangle;
            CHECK_RC(func.Launch(m_Params));
            kernLaunchCount++;
        }

        CHECK_RC(stopEvent.Record());

        // Get error count
        CHECK_RC(m_MiscompareCount.Get(m_HostMiscompareCount.GetPtr(),
                                       m_HostMiscompareCount.GetSize()));

        // Synchronize and get time for kernel completion
        GetLwdaInstance().Synchronize();
        duration += stopEvent.TimeMsElapsedSinceF(startEvent);

        // Update test progress
        if (!bPerpetualRun)
        {
            if (m_RuntimeMs)
            {
                CHECK_RC(PrintProgressUpdate(std::min(static_cast<UINT64>(duration),
                                                      static_cast<UINT64>(m_RuntimeMs))));
            }
            else
            {
                CHECK_RC(PrintProgressUpdate(loop + 1));
            }
        }

        // Handle errors
        UINT64 numMiscompares = *static_cast<UINT64*>(m_HostMiscompareCount.GetPtr());
        if (numMiscompares > 0)
        {
            Printf(Tee::PriError,
                   "LwdaTTUStress found %llu miscompare(s) on loop %llu\n", numMiscompares, loop);
            TTUError* pErrors = static_cast<TTUError*>(m_HostErrorLog.GetPtr());

            if (numMiscompares > m_ErrorLogLen)
            {
                Printf(Tee::PriWarn,
                       "%llu miscompares, but error log only contains %u entries. "
                       "Some failing SMID/TPCs may not be reported.\n",
                       numMiscompares, m_ErrorLogLen);
            }
            for (UINT32 i = 0; i < numMiscompares && i < m_ErrorLogLen; i++)
            {
                const auto& error = pErrors[i];
                if (m_DumpMiscompares)
                {
                    const auto pRef = TTUHit::GetHit(error.ref);
                    const auto pRet = TTUHit::GetHit(error.ret);
                    Printf(Tee::PriError,
                           "QueryType %s, Ray %u, Iteration %llu\nRef %s\nRet %s\n\n",
                           TTUUtils::QueryTypeToStr(error.queryType), error.rayId, error.iteration,
                           pRef->ToStr().c_str(), pRet->ToStr().c_str());
                }
                if (m_ReportFailingSmids)
                {
                    m_SmidMiscompareCount[error.smid]++;
                }
            }
            stickyRc = RC::GPU_COMPUTE_MISCOMPARE;
            if (GetGoldelwalues()->GetStopOnError())
            {
                break;
            }
        }

        // In perpetual-run mode exit when KeepRunning is set to false
        if (bPerpetualRun && !m_KeepRunning)
        {
            break;
        }
    }

    VerbosePrintf("Kernel Runtime: %fms, %fs\n", duration, duration / 1000.0f);
    UINT64 raysTraced = kernLaunchCount *
                        m_InnerIterations *
                        static_cast<UINT64>(m_NumBlocks * m_NumThreads);
    const double gigaRaysPerSec =
        duration ? (static_cast<double>(raysTraced) / 1e9) / (duration / 1e3) : 0.0;
    VerbosePrintf("Rays Traced: %llu\n", raysTraced);
    VerbosePrintf("GRays / Sec: %f\n", gigaRaysPerSec);
    CHECK_RC(SetPerformanceMetric(ENCJSENT("GRaysPerSec"), gigaRaysPerSec));

    // Print which SM/TPCs failed. The SM data isn't really useful,
    // but the TPC can tell us which TTUs are bad.
    if (m_ReportFailingSmids)
    {
        CHECK_RC(ReportFailingSmids(m_SmidMiscompareCount, m_SmidFailureLimit));
    }

    // Dump a reduced number of result rays if requested
    // (If we dump too many rays the JS engine dies from running out of memory)
    if (m_DumpToJson)
    {
        vector<unique_ptr<TTUHit>> r;
        vector<Point> o;
        vector<Direction> d;

        const UINT64 debugPtr = reinterpret_cast<UINT64>(m_DebugResult.GetPtr());
        auto retvals = reinterpret_cast<Retval*>(debugPtr);
        auto origins = reinterpret_cast<Point*>(
            debugPtr + m_RaysToDump * sizeof(Retval));
        auto dirs = reinterpret_cast<Direction*>(
            debugPtr + m_RaysToDump * (sizeof(Retval) + sizeof(Point)));

        for (UINT32 i = 0; i < m_RaysToDump; i++)
        {
            r.push_back(TTUHit::GetHit(retvals[i]));
            VerbosePrintf("Ray%04d %s\n", i, r[i]->ToStr().c_str());
        }
        for (UINT32 i = 0; i < m_RaysToDump; i++)
        {
            auto origin = origins[i];
            o.push_back(origin);
            VerbosePrintf("Ray%04d x: %.2f y: %.2f z: %.2f\n", i, origin.x, origin.y, origin.z);
        }
        for (UINT32 i = 0; i < m_RaysToDump; i++)
        {
            auto dir = dirs[i];
            d.push_back(dir);
            VerbosePrintf("Ray%04d dirX: %.2f dirY: %.2f dirZ: %.2f\n", i, dir.x, dir.y, dir.z);
        }

        CHECK_RC(RaysToJson(r, o, d));
    }

    return stickyRc;
}

RC LwdaTTUStress::Cleanup()
{
    m_DeviceTriangleRange.Free();
    m_HostTriangleRange.Free();
    m_DeviceBVH.Free();
    m_HostBVH.Free();
    m_DeviceBVHMicromesh.Free();
    m_HostBVHMicromesh.Free();

    m_MiscompareCount.Free();
    m_HostMiscompareCount.Free();
    m_HostErrorLog.Free();
    m_DebugResult.Free();

    m_Module.Unload();
    return LwdaStreamTest::Cleanup();
}
