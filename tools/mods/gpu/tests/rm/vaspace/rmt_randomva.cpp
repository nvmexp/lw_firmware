/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>
#include "core/include/utility.h"
#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"

#include "class/cl90f1.h"  // FERMI_VASPACE_A

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/rmt_memalloc.h"
#include "gpu/tests/rm/utility/rmt_memmap.h"
#include "gpu/tests/rm/utility/rmt_vaspace.h"

#include "core/include/memcheck.h"

using namespace rmt;

//! Boilerplate test class.
class RandomVATest: public RmTest
{
public:
    RandomVATest();
    virtual ~RandomVATest();

    virtual string   IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

    SETGET_PROP(numActions,     UINT32);
    SETGET_PROP(actionMask,     UINT32);
    SETGET_PROP(breakOnAction,  UINT32);
    SETGET_PROP(chunkVerifMode, UINT32);
    SETGET_PROP(largeAllocWeight, UINT32);
    SETGET_PROP(vmmRegkey,      UINT32);
    SETGET_PROP(largePageInSysmem, UINT32);

private:
    //! Method signature for random actions.
    typedef RC (RandomVATest::*ActionFunc)(const UINT32 id, const UINT32 seed, bool *pBValid);

    //! Method signature to check if action is lwrrently allowed.
    typedef bool (RandomVATest::*ActionCheck)();

    //! Descriptor for random actions.
    struct ActionDesc
    {
        UINT32      weight;
        const char *pName;
        UINT32      numValid;
        UINT32      numTotal;
    };

    //! WAR wrapper for GCC 2.95 bug.
    struct ActionFuncWrapper
    {
        ActionFunc pFunc;
    };

    //! WAR wrapper for GCC 2.95 bug.
    struct ActionCheckWrapper
    {
        ActionCheck pCheck;
    };

    //! @sa ActionCheck
    bool CheckVASCreate()
    {
        return m_poolVAS.all.size() < 100;
    }

    //! @sa ActionCheck
    bool CheckVASDestroy()
    {
        return m_poolVAS.all.size() > 1;
    }

    //! @sa ActionCheck
    bool CheckVASResize()
    {
        return m_poolVAS.all.size() > 0;
    }

    //! @sa ActionCheck
    bool CheckVASSubset()
    {
        return m_poolVAS.subsets.size() > 0;
    }

    //! @sa ActionCheck
    bool CheckVASSubsetCommit()
    {
        return m_poolVAS.subsets.size() > 0;
    }

    //! @sa ActionCheck
    bool CheckVASSubsetRevoke()
    {
        return m_poolVAS.subsets.size() > 0;
    }

    //! @sa ActionCheck
    bool CheckPhysAlloc()
    {
        return m_poolPhys.size() < 2500;
    }

    //! @sa ActionCheck
    bool CheckPhysFree()
    {
        return m_poolPhys.size() > 1;
    }

    //! @sa ActionCheck
    bool CheckVirtAlloc()
    {
        return (m_poolVAS.all.size() > 0) &&
               (m_poolVAS.numAllocs < 5000);
    }

    //! @sa ActionCheck
    bool CheckVirtFree()
    {
        return m_poolVAS.hasAllocs.size() > 0;
    }

    //! @sa ActionCheck
    bool CheckMap()
    {
        // Can only map if we have physical and virtual allocations.
        return (m_poolVAS.numMaps < 5000) && CheckPhysFree() && CheckVirtFree();
    }

    //! @sa ActionCheck
    bool CheckUnmap()
    {
        return m_poolVAS.hasMaps.size() > 0;
    }

    //! @sa ActionCheck
    bool CheckCopy()
    {
        // Can only copy if we have mappings.
        return CheckUnmap();
    }

    RC RandomVASCreate(const UINT32 id, const UINT32 seed, bool *pBValid);
    RC RandomVASDestroy(const UINT32 id, const UINT32 seed, bool *pBValid);
    RC RandomVASResize(const UINT32 id, const UINT32 seed, bool *pBValid);
    RC RandomVASSubsetCommit(const UINT32 id, const UINT32 seed, bool *pBValid);
    RC RandomVASSubsetRevoke(const UINT32 id, const UINT32 seed, bool *pBValid);
    RC RandomPhysAlloc(const UINT32 id, const UINT32 seed, bool *pBValid);
    RC RandomPhysFree(const UINT32 id, const UINT32 seed, bool *pBValid);
    RC RandomVirtAlloc(const UINT32 id, const UINT32 seed, bool *pBValid);
    RC RandomVirtFree(const UINT32 id, const UINT32 seed, bool *pBValid);
    RC RandomMap(const UINT32 id, const UINT32 seed, bool *pBValid);
    RC RandomUnmap(const UINT32 id, const UINT32 seed, bool *pBValid);
    RC RandomCopy(const UINT32 id, const UINT32 seed, bool *pBValid);

    LwRm      *m_pLwRm;
    GpuDevice *m_pGpuDev;

    //! Number of random actions to execute during the test.
    UINT32 m_numActions;

    //! Bitmask of action indices that are enabled.
    //!
    //! This can be used to only run a subset of actions
    //! (e.g. stress physical or virtual allocs).
    UINT32 m_actionMask;

    //! Index of action on which to break into debugger.
    //! Action indices start at 1 so initializing this to 0 does nothing.
    //!
    //! This is pure colwenience over a conditional breakpoint
    //! (e.g. you know in advance which action you want to step through).
    UINT32 m_breakOnAction;

    enum CHUNK_VERIF_MODE
    {
        CHUNK_VERIF_ON_FREE = 0, //!< Verify when physical memory freed.
        CHUNK_VERIF_ON_COPY = 1, //!< Verify destination after each copy.
    };

    //! Mode of verification for physical memory chunks.
    UINT32 m_chunkVerifMode;

    //! Weight multiplier for large (>1MB) physical allocations.
    UINT32 m_largeAllocWeight;

    //! VMM enablement regkey.
    UINT32 m_vmmRegkey;

    //! Regkey to enable big pages in sysmem.
    UINT32 m_largePageInSysmem;

    //! Pool of randomized physical allocations.
    rmt::Pool<MemAllocPhys*> m_poolPhys;

    //! Pool of randomized VA space objects.
    //! Each VAS has a pool of randomized virtual allocations.
    //! Each virtual allocation has a pool of randomized mappings.
    VASTracker m_poolVAS;
};

//! Construct test (no client/GPU yet).
//-----------------------------------------------------------------------------
RandomVATest::RandomVATest() :
    m_pLwRm(NULL),
    m_pGpuDev(NULL),
    m_numActions(100),
    m_actionMask(LW_U32_MAX),
    m_breakOnAction(0),
    m_chunkVerifMode(CHUNK_VERIF_ON_FREE),
    m_largeAllocWeight(1),
    m_vmmRegkey(1)
{
    SetName("RandomVATest");
}

//-----------------------------------------------------------------------------
RandomVATest::~RandomVATest()
{
}

//-----------------------------------------------------------------------------
string RandomVATest::IsTestSupported()
{
    if (!IsClassSupported(FERMI_VASPACE_A))
        return "FERMI_VASPACE_A class is not supported";
    return RUN_RMTEST_TRUE;
}

//! Setup initial state (now with client/GPU).
//-----------------------------------------------------------------------------
RC RandomVATest::Setup()
{
    RC rc;

    // Setup basic test state from JS settings.
    CHECK_RC(InitFromJs());

    // Set the channel type for the VA space test channels.
    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);
    m_TestConfig.SetAllowMultipleChannels(true);

    m_pLwRm   = GetBoundRmClient();
    m_pGpuDev = GetBoundGpuDevice();

    return OK;
}

//! Top-level test loop.
//-----------------------------------------------------------------------------
RC RandomVATest::Run()
{
    //
    // NOTE: These Action* structs and arrays could be simpler,
    //       but GCC 2.95 is still used for 32-bit MODS builds, and contains
    //       compiler bugs related to arrays of member function pointers.
    //
    vector<ActionDesc>         actionDescs;
    vector<ActionFuncWrapper>  actionFuncs;
    vector<ActionCheckWrapper> actionChecks;
    #define ADD_ACTION(weight, name)                                    \
        {                                                               \
            ActionDesc desc = { weight, #name, 0, 0, };                 \
            actionDescs.push_back(desc);                                \
            ActionFuncWrapper func = { &RandomVATest::Random##name };   \
            actionFuncs.push_back(func);                                \
            ActionCheckWrapper check = { &RandomVATest::Check##name };  \
            actionChecks.push_back(check);                              \
        }

    //
    // Declare possible random actions and their weights.
    //
    // The weights must be chosen carefully to ensure
    // the pools fill up but don't overflow. No cannonballs allowed.
    //
    ADD_ACTION(  3, VASCreate);
    ADD_ACTION(100, PhysAlloc);
    ADD_ACTION(200, VirtAlloc);
    ADD_ACTION(200, Map);
    ADD_ACTION(200, Copy);
    ADD_ACTION(  5, VASResize);
    ADD_ACTION( 50, VASSubsetCommit);
    ADD_ACTION( 25, VASSubsetRevoke);
    ADD_ACTION( 60, Unmap);
    ADD_ACTION( 60, VirtFree);
    ADD_ACTION( 30, PhysFree);
    ADD_ACTION(  1, VASDestroy);

    // Below code assumes at least one action registered.
    MASSERT(!actionDescs.empty());

    // Prepare the weights for random picking.
    vector<Random::PickItem> actionPicker;

    for (UINT32 i = 0; i < actionDescs.size(); i++)
    {
        Random::PickItem picker = {0};
        picker.Min = i;
        picker.Max = i;
        if (m_actionMask & BIT(i))
        {
            picker.RelProb = actionDescs[i].weight;
        }
        actionPicker.push_back(picker);
    }
    Random::PreparePickItems((INT32)actionPicker.size(), &actionPicker[0]);

    // Print resulting probabilities.
    Printf(Tee::PriHigh, "RandomVATest Action Settings (Mask 0x%04X):\n",
           m_actionMask);
    UINT32 lastProb = 0;
    for (size_t i = 0; i < actionDescs.size(); i++)
    {
        if (!(m_actionMask & BIT(i)))
        {
            // WAR issue with Random::PickItem (0% prob not supported).
            actionPicker[i].ScaledProb = lastProb;
        }
        double prob = (actionPicker[i].ScaledProb - lastProb) / (double)LW_U32_MAX;
        lastProb = actionPicker[i].ScaledProb;
        Printf(Tee::PriHigh, "    0x%04X %-10s %1.4f%%%s\n",
               BIT(i), actionDescs[i].pName, prob * 100,
               (m_actionMask & BIT(i)) ? "" : " (disabled)");
    }

    RC rc;
    const UINT32 seed = m_TestConfig.Seed();

    Printf(Tee::PriHigh, "RandomVATest Seed: 0x%08X\n", seed);
    RandomInit rand(seed);

    // Force a lucky VA space to kick-start the party.
    while (m_poolVAS.all.empty())
    {
        bool bValid = false;
        CHECK_RC(RandomVASCreate(0, rand.GetRandom(), &bValid));
    }

    //
    // Act randomly until either:
    //   1. We pass out.               (numActions exhausted)
    //   2. The police arrive.         (environment time limit)
    //   3. Someone crashes the party. (exciting new bug found)
    //
    // TODO: set default numActions based on platform or monitor time limit?
    //
    for (UINT32 i = 1; i <= m_numActions; i++)
    {
        const UINT32 seed = rand.GetRandom();
        UINT32 act;
        do
        {
            act = rand.Pick(&actionPicker[0]);
        } while (!(this->*actionChecks[act].pCheck)());
        Printf(Tee::PriHigh, "Action %u %s(0x%08X)\n",
               i, actionDescs[act].pName, seed);
        if (i == m_breakOnAction)
        {
            Xp::BreakPoint();
        }
        bool bValid = false;
        rc = (this->*actionFuncs[act].pFunc)(i, seed, &bValid);
        if (rc != OK)
        {
            MASSERT(0);
            break; // Explicit abort to print the result counts.
        }
        actionDescs[act].numValid += bValid ? 1 : 0;
        actionDescs[act].numTotal += 1;
    }

    // If anyone is still conscious, give them something to read.
    Printf(Tee::PriHigh, "RandomVATest Action Results:\n");
    for (size_t i = 0; i < actionDescs.size(); i++)
    {
        Printf(Tee::PriHigh, "    0x%04X %-10s %4u / %4u = %3.2f%% valid\n",
               BIT(i), actionDescs[i].pName,
               actionDescs[i].numValid, actionDescs[i].numTotal,
               100 * actionDescs[i].numValid / (double)actionDescs[i].numTotal);
    }

    return rc;
}

//! Tear-down.
//-----------------------------------------------------------------------------
RC RandomVATest::Cleanup()
{
    // Destroy all remainining VAS and physical allocations.
    while (!m_poolPhys.empty())
    {
        // TODO: colwert MemAllocs to use pool tracker.
        MemAllocPhys *pItem = m_poolPhys.getAny();
        Randomizable::deleter(pItem);
        m_poolPhys.erase(pItem);
    }
    while (!m_poolVAS.all.empty())
    {
        Randomizable::deleter(m_poolVAS.all.getAny());
    }
    return OK;
}

//! Create a random VA space and add it to the pool if valid.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomVASCreate(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;
    RandomInit rand(seed);

    // Sometimes create subset VAS.
    VASpace *pParent = NULL;
    if (!m_poolVAS.parentable.empty() && (rand.GetRandom(0, 100) < 50))
    {
        pParent = m_poolVAS.parentable.getRandom(&rand);
    }

    // Create randomized VAS.
    unique_ptr<VASpaceWithChannel> pVAS(new VASpaceWithChannel(m_pLwRm, m_pGpuDev,
                                                             &m_poolVAS, pParent,
                                                             m_vmmRegkey, id));
    CHECK_RC(pVAS->CreateRandom(rand.GetRandom()));

    // Only use if valid.
    if (pVAS->GetHandle())
    {
        // Create channel for virtual memcpy actions.
        CHECK_RC(pVAS->CreateChannel(&m_TestConfig));

        // Commit ownership to the VAS pool.
        pVAS.release();
        *pBValid = true;
    }

    return rc;
}

//! Destroy a random VA space if one exists.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomVASDestroy(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;

    RandomInit rand(seed);
    Randomizable::deleter(m_poolVAS.all.getRandom(&rand));
    *pBValid = true;

    return rc;
}

//! Resize a random VA space if one exists.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomVASResize(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;

    RandomInit rand(seed);

    VASpace *pVAS = m_poolVAS.all.getRandom(&rand);
    VASpaceResizer resizer(m_pLwRm, m_pGpuDev, pVAS);
    CHECK_RC(resizer.CreateRandom(rand.GetRandom()));

    *pBValid = resizer.GetValid();

    return rc;
}

//! Commit a subset VAS.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomVASSubsetCommit(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;

    RandomInit rand(seed);

    VASpace *pVAS = m_poolVAS.subsets.getRandom(&rand);
    Printf(Tee::PriHigh, "%s(%u)\n", pVAS->GetTypeName(), pVAS->GetID());
    CHECK_RC(pVAS->SubsetCommit());

    *pBValid = true;

    return rc;
}

//! Commit a subset VAS.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomVASSubsetRevoke(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;

    RandomInit rand(seed);

    VASpace *pVAS = m_poolVAS.subsets.getRandom(&rand);
    Printf(Tee::PriHigh, "%s(%u)\n", pVAS->GetTypeName(), pVAS->GetID());
    CHECK_RC(pVAS->SubsetRevoke());

    *pBValid = true;

    return rc;
}

//! Create a random physical allocation and add it to the pool if valid.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomPhysAlloc(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;
    RandomInit rand(seed);

    // Create randomized physical allocation.
    unique_ptr<MemAllocPhys> pPhys(new MemAllocPhys(m_pLwRm, m_pGpuDev,
                                                  m_largeAllocWeight,
                                                  m_largePageInSysmem, id));
    CHECK_RC(pPhys->CreateRandom(rand.GetRandom()));

    // TODO: Maintain rough physical heap usage (wo/ blocks)?

    // Only use if valid.
    if (pPhys->GetHandle())
    {
        // Fill initial chunk w/ BAR1 mapping.
        const MemAlloc::Params &params = pPhys->GetParams();
        pPhys->AddChunk(0, params.size, rand.GetRandom(), true);

        // Commit ownership to the physical alloc pool.
        m_poolPhys.insert(pPhys.release());
        *pBValid = true;
    }
    else
    {
        // TODO: On Failure - check heap usage unchanged
    }

    return rc;
}

//! Destroy a random physical allocation if one exists.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomPhysFree(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;

    RandomInit rand(seed);
    MemAllocPhys *pItem = m_poolPhys.getRandom(&rand);
    Randomizable::deleter(pItem);
    m_poolPhys.erase(pItem);
    *pBValid = true;

    return rc;
}

//! Create a random virtual allocation and add it to the pool if valid.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomVirtAlloc(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;
    RandomInit rand(seed);

    // Pick a random VAS to allocate from.
    VASpace *pVAS = m_poolVAS.all.getRandom(&rand);
    MASSERT(pVAS);

    // Create randomized virtual allocation.
    unique_ptr<MemAllocVirt> pVirt(new MemAllocVirt(m_pLwRm, m_pGpuDev, pVAS, id));
    CHECK_RC(pVirt->CreateRandom(rand.GetRandom()));

    // Only use if valid.
    if (pVirt->GetHandle())
    {
        // TODO: Pass - verify can't alloc overlap

        // Commit onwership to the VAS (already linked).
        pVirt.release();
        *pBValid = true;

        // Print heap summary. TODO: Use RM heap dump.
        Heap *pHeap = pVAS->GetHeap();
        Printf(Tee::PriHigh, "%s(%u): Heap Summary:\n"
               "      Total Free: 0x%llX\n"
               "    Largest Free: 0x%llX\n",
               pVAS->GetTypeName(), pVAS->GetID(),
               pHeap->GetTotalFree(), pHeap->GetLargestFree());
    }
    else
    {
        // TODO: On Failure - check heap usage unchanged
    }

    return rc;
}

//! Destroy a random virtual allocation if one exists.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomVirtFree(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;
    RandomInit rand(seed);

    // Pick a random VAS to free from.
    VASpace *pVAS = m_poolVAS.hasAllocs.getRandom(&rand);
    MASSERT(pVAS);

    Randomizable::deleter(pVAS->GetAllocs().getRandom(&rand));

    *pBValid = true;

    return rc;
}

//! Create a random memory mapping and add it to the pool if valid.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomMap(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;
    RandomInit rand(seed);

    // Pick a random physical allocation to map.
    MemAllocPhys *pPhys = m_poolPhys.getRandom(&rand);
    MASSERT(pPhys);

    // Pick a random VAS to map into.
    VASpace *pVAS = m_poolVAS.hasAllocs.getRandom(&rand);
    MASSERT(pVAS);

    // Pick a random virtual allocation to map into.
    MemAllocVirt *pVirt = pVAS->GetAllocs().getRandom(&rand);
    MASSERT(pVirt);

    // Create randomized mapping.
    unique_ptr<MemMapGpu> pMap(new MemMapGpu(m_pLwRm, m_pGpuDev, pVirt, pPhys, id));
    CHECK_RC(pMap->CreateRandom(rand.GetRandom()));

    // Only use if valid.
    if (pMap->GetVirtAddr())
    {
        // TODO: Get PDE/PTE info - verify PD+PTs

        // Commit onwership to the virtual and physical
        // allocations (already linked).
        pMap.release();
        *pBValid = true;
    }
    else
    {
        // TODO: On Failure - check heap usage unchanged
    }

    return rc;
}

//! Destroy a random memory mapping if one exists.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomUnmap(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;
    RandomInit rand(seed);

    // Pick a random VAS to unmap from.
    VASpace *pVAS = m_poolVAS.hasMaps.getRandom(&rand);
    MASSERT(pVAS);

    Randomizable::deleter(pVAS->GetMappings().getRandom(&rand));

    *pBValid = true;

    return rc;
}

//! Copy data between random regions of two random virtual memory mappings.
//-----------------------------------------------------------------------------
RC RandomVATest::RandomCopy(const UINT32 id, const UINT32 seed, bool *pBValid)
{
    RC rc;
    RandomInit rand(seed);

    // Pick random VAS.
    // TODO: channel should be separated to avoid unsafe cast.
    VASpaceWithChannel *pVAS = (VASpaceWithChannel*)m_poolVAS.hasMaps.getRandom(&rand);
    MASSERT(pVAS);

    // Pick random mappings from VAS (might be the same).
    MemMapGpu *pMapSrc = pVAS->GetMappings().getRandom(&rand);
    MemMapGpu *pMapDst = pVAS->GetMappings().getRandom(&rand);
    MASSERT(pMapSrc && pMapDst);

    // Check for valid read/write access.
    const MemMapGpu::Params &paramsSrc = pMapSrc->GetParams();
    const MemMapGpu::Params &paramsDst = pMapDst->GetParams();
    if (FLD_TEST_DRF(OS46, _FLAGS, _ACCESS, _WRITE_ONLY, paramsSrc.flags) ||
        FLD_TEST_DRF(OS46, _FLAGS, _ACCESS, _READ_ONLY,  paramsDst.flags))
    {
        Printf(Tee::PriHigh, "%s: Skipping due to read/write access\n",
               __FUNCTION__);
        return rc;
    }

    // Pick random offsets and length (4-byte aligned).
    const UINT64 maxLengthSrc = LW_ALIGN_UP(paramsSrc.length, 4);
    const UINT64 offsetSrcRand = rand.GetRandom64(0, maxLengthSrc - 1);
    const UINT64 offsetSrc = LW_ALIGN_DOWN(offsetSrcRand, 4);

    const UINT64 maxLengthDst = LW_ALIGN_UP(paramsDst.length, 4);
    const UINT64 offsetDstRand = rand.GetRandom64(0, maxLengthDst - 1);
    const UINT64 offsetDst = LW_ALIGN_DOWN(offsetDstRand, 4);

    const UINT64 lengthRand = rand.GetRandom64(4, MIN(maxLengthSrc - offsetSrc,
                                                      maxLengthDst - offsetDst));
    const UINT64 length = LW_ALIGN_UP(lengthRand, 4);

    // Callwlate relative physical offsets.
    MemAllocPhys *pPhysSrc = pMapSrc->GetPhys();
    MemAllocPhys *pPhysDst = pMapDst->GetPhys();
    const UINT64 physSrc = LW_ALIGN_DOWN(paramsSrc.offsetPhys, 4) + offsetSrc;
    const UINT64 physDst = LW_ALIGN_DOWN(paramsDst.offsetPhys, 4) + offsetDst;

    const MemAlloc::Params &allocParams = pPhysDst->GetParams();
    if (!FLD_TEST_DRF(OS32, _ATTR, _FORMAT, _PITCH, allocParams.attr))
    {
        //
        // Do not copy in case format is SWIZZLED or BLOCK_LINEAR. This is done
        // to prevent breaking of the existing BAR1/CE copy readback model while
        // exelwting VerifyChunk().
        //
        Printf(Tee::PriHigh, "%s: Skipping copy since format is not PITCH\n",
               __FUNCTION__);
        return rc;
    }
    // If same backing physical memory, check for overlap.
    if (pPhysSrc == pPhysDst)
    {
        const UINT64 limitSrc = physSrc + length - 1;
        const UINT64 limitDst = physDst + length - 1;
        if (((physSrc <= physDst) && (physDst <= limitSrc)) ||
            ((physDst <= physSrc) && (physSrc <= limitDst)))
        {
            Printf(Tee::PriHigh, "%s: Skipping due to physical overlap\n",
                   __FUNCTION__);
            return rc;
        }
        //Printf(Tee::PriHigh, "%s: Skipping due to same physical\n",
        //       __FUNCTION__);
        //return rc;
    }

    // Callwlate absolute virtual addresses.
    MemAllocVirt *pVirtSrc = pMapSrc->GetVirt();
    MemAllocVirt *pVirtDst = pMapDst->GetVirt();
    const UINT64 virtSrc = LW_ALIGN_DOWN(pMapSrc->GetVirtAddr(), 4) + offsetSrc;
    const UINT64 virtDst = LW_ALIGN_DOWN(pMapDst->GetVirtAddr(), 4) + offsetDst;

    // We're committed - print detailed info.
    Printf(Tee::PriHigh, "%s: Copying 0x%llX bytes:\n"
           "    Src: %s(%u) + 0x%010llX => %s(%u) => %s(%u) @ 0x%010llX\n"
           "    Dst: %s(%u) + 0x%010llX => %s(%u) => %s(%u) @ 0x%010llX\n",
           __FUNCTION__, length,
           pPhysSrc->GetTypeName(), pPhysSrc->GetID(), physSrc,
           pMapSrc->GetTypeName(), pMapSrc->GetID(),
           pVirtSrc->GetTypeName(), pVirtSrc->GetID(), virtSrc,
           pPhysDst->GetTypeName(), pPhysDst->GetID(), physDst,
           pMapDst->GetTypeName(), pMapDst->GetID(),
           pVirtDst->GetTypeName(), pVirtDst->GetID(), virtDst);

    // TODO: ilwalidate TLB/MMU if defer flag is set on any participant.

    // Execute copy between the virtual mappings.
    CHECK_RC(pVAS->GetChannel()->MemCopy(virtDst, virtSrc, (UINT32)length));

    // Record expected changes to physical memory chunks.
    CHECK_RC(pPhysDst->CopyChunks(physDst, physSrc, length, pPhysSrc));

    // If enabled, verify destination physical memory.
    if (m_chunkVerifMode == CHUNK_VERIF_ON_COPY)
    {
        CHECK_RC(pPhysDst->VerifyChunks());
    }
    // TODO: Super-slow debug mode to verify all physical allocs?
    *pBValid = true;

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(RandomVATest, RmTest,
                 "RandomVATest RM test.");

CLASS_PROP_READWRITE(RandomVATest, numActions, UINT32,
                     "Number of random actions");
CLASS_PROP_READWRITE(RandomVATest, actionMask, UINT32,
                     "Mask of enabled actions");
CLASS_PROP_READWRITE(RandomVATest, breakOnAction, UINT32,
                     "Action number to break on");
CLASS_PROP_READWRITE(RandomVATest, chunkVerifMode, UINT32,
                     "Set level of physical chunk verification");
CLASS_PROP_READWRITE(RandomVATest, largeAllocWeight, UINT32,
                     "Weight multiplier for large physical allocations");
CLASS_PROP_READWRITE(RandomVATest, vmmRegkey, UINT32,
                     "New VMM path regkey");
CLASS_PROP_READWRITE(RandomVATest, largePageInSysmem, UINT32,
                     "Regkey to enable big pages in sysmem");
