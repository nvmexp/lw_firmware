/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_eccscrubbeddeallocationtest.cpp
//! \brief Verifies that deallocated memory on FB is erased
//!
//! WARNING: This test assumes that 0's are written to erase FB. Change
//! ERASE_PATTERN found in rmt_eccscrubbeddeallocationtest.h accordingly if a
//! different pattern (e.g. 0xDEADBEEF) is used to erase FB.

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl90b5.h"       // GF100_DMA_COPY
#include "class/cl90b5sw.h"
#include "class/cla0b5.h"       // KEPLER_DMA_COPY_A
#include "class/clb0b5.h"       // MAXWELL_DMA_COPY_A
#include "class/clc0b5.h"       // PASCAL_DMA_COPY_A
#include "class/clc3b5.h"       // VOLTA_DMA_COPY_A
#include "class/clc5b5.h"       // TURING_DMA_COPY_A 
#include "class/clc6b5.h"       // AMPERE_DMA_COPY_A
#include "class/clc7b5.h"       // AMPERE_DMA_COPY_B
#include "class/clc8b5.h"       // HOPPER_DMA_COPY_A
#include "class/clc9b5.h"       // BLACKWELL_DMA_COPY_A
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_COPY_ENGINE
#include "class/cl0000.h"       // LW01_NULL_OBJECT
#include "core/utility/errloggr.h"
#include "gpu/utility/surf2d.h"
#include <vector>
#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"
#include "gpu/utility/ecccount.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/include/gpudev.h"
#include "gpu/utility/bglogger.h"
#include "gpu/utility/modscnsl.h"
#include "rmt_eccscrubbeddeallocationtest.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/types.h"

// engine & class inclusion lists
static const LwU32 enginesToKeep[] = {
    LW2080_ENGINE_TYPE_COPY0,
    LW2080_ENGINE_TYPE_COPY1,
    LW2080_ENGINE_TYPE_COPY2
};

static const LwRm::Handle classesToKeep[] = {
    BLACKWELL_DMA_COPY_A,
    HOPPER_DMA_COPY_A,
    AMPERE_DMA_COPY_B,
    AMPERE_DMA_COPY_A,
    TURING_DMA_COPY_A,
    VOLTA_DMA_COPY_A,
    PASCAL_DMA_COPY_A,
    MAXWELL_DMA_COPY_A,
    KEPLER_DMA_COPY_A
};

class EccScrubbedDeallocationTest : public RmTest
{
public:
    EccScrubbedDeallocationTest();
    virtual ~EccScrubbedDeallocationTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    // engine/class information
    GpuSubdevice* masterGpu;
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engineParams;
    LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS classParams;

    // CE information
    LwU32 engineType;
    LwRm::Handle engineClass;
    LwRm::Handle hCeObj;
    Surface2D* pSema;
    Surface2D* pDest;

    // channel/subchannel information
    Channel *pCh;
    LwRm::Handle hCh;
    LwU32 subChannel;

    // interrupt information
    ModsEvent* pModsEvent;
    FLOAT64 intrTimeout;
    LwRm::Handle hNotifyEvent;
    void* pEventAddr;

    RC CeCopyBlock(LwRm::Handle hLwrCeObj, LwU32 locSubChannel,
        LwU64 source, Memory::Location srcLoc, LwU32 srcAddrType, Surface2D* pLocSrc,
        LwU64 dest, Memory::Location destLoc, LwU32 destAddrType, Surface2D* pLocDest,
        LwU64 blockSize, Surface2D* pLocSema);
    RC GetAllSupportedEngines(LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS* locEngineParams);
    RC GetAllSupportedClasses(LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS* locClassParams,
                    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS* locEngineParam);
    RC PruneEngineList(LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS* locEngineParams,
                    const LwU32* locEnginesToKeep, LwU32 keepEnginesCount);
    RC PruneClassList(LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS* locClassParams,
                    const LwRm::Handle* locClassesToKeep, LwU32 keepClassesCount);
    RC AllocCeCompliantMem(LwU32 size, Memory::Location loc, LwU64 addrHint, Surface2D** allocationPointer);
    RC ClassToEngine(LwRm::Handle engineClass, LwU32* engine);
    void WaitCeFinish(LwRm::Handle locEngineClass, ModsEvent* pLocModsEvent);
    RC FlushTo(LwU32 targetLoc);
};

//------------------------------------------------------------------------------
EccScrubbedDeallocationTest::EccScrubbedDeallocationTest()
{
    masterGpu = 0;
    memset(&engineParams, 0, sizeof(engineParams));
    memset(&classParams, 0, sizeof(classParams));
    engineType = 0;
    engineClass = 0;
    hCeObj = 0;
    pSema = 0;
    pDest = 0;
    pCh = 0;
    hCh = 0;
    subChannel = 0;
    pModsEvent = 0;
    intrTimeout = 0;
    hNotifyEvent = 0;
    pEventAddr = 0;

    SetName("EccScrubbedDeallocationTest");
}

//------------------------------------------------------------------------------
EccScrubbedDeallocationTest::~EccScrubbedDeallocationTest()
{
}

//! \brief IsTestSupported() checks if the the master GPU has the hardware needed
//!        by this test
//!
//! EccScrubbedDeallocationTest needs: KEPLER_DMA_COPY_A engine
//!
//! \return returns RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         else returns "NOT SUPPORTED"
//------------------------------------------------------------------------------
string EccScrubbedDeallocationTest::IsTestSupported()
{
    //
    // step 1: check if we're running on silicon, this test does not work if it
    // is not run on silicon
    //
    if(Platform::Hardware != Platform::GetSimulationMode())
        return "EccFbFullScanTest only runs on hardware";

    // step 2: check if the correct CE is supported
    if (IsClassSupported(GF100_DMA_COPY)     ||
        IsClassSupported(KEPLER_DMA_COPY_A)  ||
        IsClassSupported(MAXWELL_DMA_COPY_A) ||
        IsClassSupported(PASCAL_DMA_COPY_A)  ||
        IsClassSupported(VOLTA_DMA_COPY_A)   ||
        IsClassSupported(TURING_DMA_COPY_A)  ||
        IsClassSupported(AMPERE_DMA_COPY_A)  ||
        IsClassSupported(AMPERE_DMA_COPY_B)  ||
        IsClassSupported(HOPPER_DMA_COPY_A)  ||
        IsClassSupported(BLACKWELL_DMA_COPY_A))
        return RUN_RMTEST_TRUE;

    return "CE not supported";
}

//! \brief Makes all the necessary allocations for EccScrubbedDeallocationTest
//!
//! for this test, Setup() must allocate the resources for operation of a CE
//! (channel, event object, destination & semaphore surfaces, CE object).
//! source surface is allocated in Run() as it is the core of the test.
//!
//! Note: The CE can only copy data formatted as a surface. AllocCeCompliantMem
//! is needed in this
//!
//! \return RC structure to denote the error status of Setup()
//------------------------------------------------------------------------------
RC EccScrubbedDeallocationTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventParams = {0};
    LW90B5_ALLOCATION_PARAMETERS allocParams = {0};

    // step 1: get the basic parameters of this test
    CHECK_RC(InitFromJs());
    intrTimeout = m_TestConfig.TimeoutMs();
    masterGpu = GetBoundGpuSubdevice();

    // step 2: get all the classes we want
    CHECK_RC_CLEANUP(GetAllSupportedEngines(&engineParams));
    CHECK_RC_CLEANUP(PruneEngineList(&engineParams, enginesToKeep,
                                    sizeof(enginesToKeep) / sizeof(LwU32)));

    CHECK_RC_CLEANUP(GetAllSupportedClasses(&classParams, &engineParams));
    CHECK_RC_CLEANUP(PruneClassList(&classParams, classesToKeep,
                                    sizeof(classesToKeep) / sizeof(LwRm::Handle)));

    // step 3: allocate a channel
    //
    // KEPLER_CHANNEL_GPFIFO_A requires CE to be in a particular subchannel, so
    // use this subchannel for all CE classes
    //
    subChannel = LWA06F_SUBCHANNEL_COPY_ENGINE;
    engineClass = ((LwU32*)classParams.classList)[0];
    ClassToEngine(engineClass, &engineType);
    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&pCh, &hCh, engineType));

    // step 4: allocate necessary surfaces
    CHECK_RC_CLEANUP(AllocCeCompliantMem(MAX_BLOCK_SIZE, Memory::NonCoherent, 0, &pSema));
    CHECK_RC_CLEANUP(AllocCeCompliantMem(MAX_BLOCK_SIZE, Memory::NonCoherent, 0, &pDest));

    // step 5: instantiate a CE object
    allocParams.version = 0;
    allocParams.engineInstance = engineType - LW2080_ENGINE_TYPE_COPY0;
    CHECK_RC_CLEANUP(pLwRm->Alloc(hCh, &hCeObj, engineClass, &allocParams));

    // step 6: setup non-blocking interrupt
    pModsEvent = Tasker::AllocEvent();
    pEventAddr = Tasker::GetOsEvent(
            pModsEvent,
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

    CHECK_RC_CLEANUP(pLwRm->AllocEvent(pLwRm->GetSubdeviceHandle(masterGpu),
                                        &hNotifyEvent,
                                        LW01_EVENT_OS_EVENT,
                                        LW2080_NOTIFIERS_CE(engineType - LW2080_ENGINE_TYPE_COPY0),
                                        pEventAddr));

    eventParams.event = LW2080_NOTIFIERS_CE(engineType - LW2080_ENGINE_TYPE_COPY0);
    eventParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(masterGpu),
                                    LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                                    &eventParams, sizeof(eventParams)));

Cleanup:
    return rc;
}

//! \brief Runs the EccScrubbedDeallocationTest
//!
//! To test if deallocated FB memory is erased, Run() allocates a surface on FB,
//! writes some non-zero pattern to it, and deallocates it. To read from the
//! deallocated surface, a CE is used to copy the deallocated memory to a surface
//! on sysmem which we can legally read from.
//!
//! \return RC structure to denote the error status of Run()
//------------------------------------------------------------------------------
RC EccScrubbedDeallocationTest::Run()
{
    RC rc;
    Surface2D *pTestSurface = 0;
    PHYSADDR srcPhysAddr = 0;
    LwU32 subdevInst = GetBoundGpuSubdevice()->GetSubdeviceInst();
    LwU32 buffer [(SURFACE_PITCH*MAX_BLOCK_SIZE)/sizeof(LwU32)];
    PHYSADDR temp;

    // step 1: prep the memory by allocating it, and record its physical address
    CHECK_RC_CLEANUP(AllocCeCompliantMem(MAX_BLOCK_SIZE, Memory::Fb, 0x0, &pTestSurface));
    srcPhysAddr = pTestSurface->GetVidMemOffset();

    // step 2: fill it up with any non-zero pattern
    pTestSurface->Fill(FILL_PATTERN);
    CHECK_RC_CLEANUP(FlushTo(FB));

    // step 3: deallocate the memory
    pTestSurface->Free();
    delete pTestSurface;
    FlushTo(FB);

    //
    // step 4: wait until asynchronous scrubber has scrubbed the surface we just
    // deallocated. we do this by trying to allocate a surface at the same physical
    // address as the one we originally allocated.
    //
    while(1)
    {
        CHECK_RC_CLEANUP(AllocCeCompliantMem(MAX_BLOCK_SIZE, Memory::Fb, srcPhysAddr, &pTestSurface));
        temp = pTestSurface->GetVidMemOffset();

        pTestSurface->Free();
        delete pTestSurface;
        FlushTo(FB);

         if (temp == srcPhysAddr)
            break;

        //
        // Optimization: wait some amount of time to let the CE scrub. This value
        // is completely arbitrary.
        //
        Tasker::Sleep(m_TestConfig.TimeoutMs());
    }

    //
    // step 5: copy the memory that surface was on so that we can read it and
    // check if it has been zeroed out
    //

    CHECK_RC_CLEANUP(CeCopyBlock(hCeObj, subChannel,
        srcPhysAddr, Memory::Fb, PHYSICAL, 0,
        pDest->GetCtxDmaOffsetGpu(), Memory::NonCoherent, VIRTUAL, pDest,
        pDest->GetSize(), pSema));
    WaitCeFinish(engineClass, pModsEvent);

    //
    // step 6: read and verify that the surface is zeroed out. We divide by 4
    // because each "pixel" in the surface is 1 byte, but each element in our buffer
    // is 32-bits.
    //
    SurfaceUtils::ReadSurface(*pDest, 0, &(buffer[0]), SURFACE_PITCH*MAX_BLOCK_SIZE, subdevInst);
    for (LwU32 i = 0; i < (pDest->GetSize() / sizeof(LwU32)); i++)
    {
        if (ERASE_PATTERN != buffer[i])
        {
            Printf(Tee::PriNormal, "ERASE_PATTERN not found for buffer[%d] = 0x%08x\n",
                   i, buffer[i]);
            return RC::SOFTWARE_ERROR;
        }
    }

Cleanup:
    return rc;

}

//------------------------------------------------------------------------------
RC EccScrubbedDeallocationTest::Cleanup()
{
    LwRmPtr pLwRm;

    if (hNotifyEvent)
        pLwRm->Free(hNotifyEvent);

    if (pModsEvent)
        Tasker::FreeEvent(pModsEvent);

    if (hCeObj)
        pLwRm->Free(hCeObj);

    if (pSema)
    {
        pSema->Free();
        delete pSema;
    }

    if (pDest)
    {
        pDest->Free();
        delete pDest;
    }

    if (pCh)
        m_TestConfig.FreeChannel(pCh);

    if (classParams.classList)
        delete [] (LwU32*)classParams.classList;

    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief CE driven copy of a block of 1D memory (either FB, coherent, or non-coherent)
//!
//! WARNING: This function was written with the intention to be used with
//! memory allocated using EccScrubbedDeallocationTest::AllocCeCompliantMem() or blocks of
//! physically contiguous memory if physical addresses are passed in.
//!
//! \return RC struct denoting exit status of CeCopyBlock()
//------------------------------------------------------------------------------
RC EccScrubbedDeallocationTest::CeCopyBlock( LwRm::Handle hLwrCeObj, LwU32 locSubChannel,
    LwU64 source, Memory::Location srcLoc, LwU32 srcAddrType, Surface2D* pLocSrc,
    LwU64 dest, Memory::Location destLoc, LwU32 destAddrType, Surface2D* pLocDest,
    LwU64 blockSize, Surface2D* pLocSema)
{
    LwU32 addrTypeFlags = 0;

    // step 1: set the src and destination addresses
    pCh->SetObject(locSubChannel, hLwrCeObj);
    pCh->Write(locSubChannel, LWA0B5_OFFSET_IN_UPPER, LwU64_HI32(source));
    pCh->Write(locSubChannel, LWA0B5_OFFSET_IN_LOWER, LwU64_LO32(source));

    pCh->Write(locSubChannel, LWA0B5_OFFSET_OUT_UPPER, LwU64_HI32(dest));
    pCh->Write(locSubChannel, LWA0B5_OFFSET_OUT_LOWER, LwU64_LO32(dest));

    //
    // step 2: set where the addresses point to (either FB, non-coherent sysmem,
    // or coherent sysmem
    //
    switch (srcLoc)
    {
        case Memory::Fb:
            pCh->Write(locSubChannel, LWA0B5_SET_SRC_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _LOCAL_FB));
            break;
        case Memory::NonCoherent:
            pCh->Write(locSubChannel, LWA0B5_SET_SRC_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM));
            break;
        case Memory::Coherent:
            pCh->Write(locSubChannel, LWA0B5_SET_SRC_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _COHERENT_SYSMEM));
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    switch (destLoc)
    {
        case Memory::Fb:
            pCh->Write(locSubChannel, LWA0B5_SET_DST_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _LOCAL_FB));
            break;
        case Memory::NonCoherent:
            pCh->Write(locSubChannel, LWA0B5_SET_DST_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM));
            break;
        case Memory::Coherent:
            pCh->Write(locSubChannel, LWA0B5_SET_DST_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _COHERENT_SYSMEM));
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    //
    // step 3: set the necessary surface/block parameters based on what kind of
    // address was passed in. address passed in can either be VIRTUAL or PHYSICAL
    //
    switch (srcAddrType)
    {
        case VIRTUAL:
            if (NULL == pLocSrc)
                return RC::BAD_PARAMETER;

            pCh->Write(locSubChannel, LWA0B5_PITCH_IN, pLocSrc->GetPitch());
            pCh->Write(locSubChannel, LWA0B5_SET_SRC_DEPTH, 1);
            pCh->Write(locSubChannel, LWA0B5_SET_SRC_LAYER, 0);
            pCh->Write(locSubChannel, LWA0B5_SET_SRC_WIDTH, LwU64_LO32(pLocSrc->GetSize()));
            pCh->Write(locSubChannel, LWA0B5_SET_SRC_HEIGHT, 1);
            pCh->Write(locSubChannel, LWA0B5_LINE_LENGTH_IN, LwU64_LO32(pLocSrc->GetSize()));
            addrTypeFlags |= DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL);
            break;
        case PHYSICAL:
            pCh->Write(locSubChannel, LWA0B5_LINE_LENGTH_IN, LwU64_LO32(blockSize));
            addrTypeFlags |= DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _PHYSICAL);
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    switch (destAddrType)
    {
        case VIRTUAL:
            if (NULL == pLocDest)
                return RC::BAD_PARAMETER;

            pCh->Write(locSubChannel, LWA0B5_PITCH_OUT, pLocDest->GetPitch());
            pCh->Write(locSubChannel, LWA0B5_SET_DST_DEPTH, 1);
            pCh->Write(locSubChannel, LWA0B5_SET_DST_LAYER, 0);
            pCh->Write(locSubChannel, LWA0B5_SET_DST_WIDTH, LwU64_LO32(pLocDest->GetSize()));
            pCh->Write(locSubChannel, LWA0B5_SET_DST_HEIGHT, 1);
            addrTypeFlags |= DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);
            break;
        case PHYSICAL:
            addrTypeFlags |= DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _PHYSICAL);
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    // step 4: set how big the block of memory is
    pCh->Write(locSubChannel, LWA0B5_LINE_COUNT, 1);

    // step 5: set the semaphores
    pCh->Write(locSubChannel, LWA0B5_SET_SEMAPHORE_PAYLOAD, SEMA_CODE);

    pCh->Write(locSubChannel, LWA0B5_SET_SEMAPHORE_A,
                DRF_NUM(A0B5, _SET_SEMAPHORE_A, _UPPER, LwU64_HI32(pLocSema->GetCtxDmaOffsetGpu())));
    pCh->Write(locSubChannel, LWA0B5_SET_SEMAPHORE_B, LwU64_LO32(pLocSema->GetCtxDmaOffsetGpu()));

    // step 6: launch the CE
    pCh->Write(locSubChannel, LWA0B5_LAUNCH_DMA,
            DRF_DEF(A0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
            DRF_NUM(A0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, LW90B5_LAUNCH_DMA_INTERRUPT_TYPE_NON_BLOCKING) |
            DRF_DEF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
            DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
            DRF_DEF(A0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
            DRF_DEF(A0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED) |
            addrTypeFlags);

    pCh->Flush();

    return OK;

}

//! \brief populates engineParams with all engines supported by master GPU
//!
//! \return RC structure to denote the error status of GetAllSupportedEngines()
//------------------------------------------------------------------------------
RC EccScrubbedDeallocationTest::GetAllSupportedEngines(LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS* locEngineParams)
{
    RC rc;
    LwRmPtr pLwRm;

    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(masterGpu),
                    LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                    locEngineParams,
                    sizeof(*locEngineParams)));

Cleanup:
    return rc;
}

//! \brief populates classParams with all engine classes accross all engines
//!        supported by the master GPU
//!
//! WARNING: flattens class list from all engines.
//!
//! \return RC structure to denote the error status of GetAllSupportedEngines().
//!         throws error if there are no engines in engineParams.
//------------------------------------------------------------------------------
RC EccScrubbedDeallocationTest::GetAllSupportedClasses(LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS* locClassParams,
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS* locEngineParams)
{
    RC rc;
    LwRmPtr pLwRm;
    LwU32 totalClassCount = 0;
    LwU32 counter = 0;
    LwU32* classList = 0;
    LwU32* engineList = (LwU32*)locEngineParams->engineList;

    // step 1: if there are no engines to be supported, error!
    if (locEngineParams->engineCount == 0)
        return RC::BAD_PARAMETER;

    // step 2: figure out how many classes there are across all engines
    for (LwU32 i = 0; i < locEngineParams->engineCount; i++)
    {
        locClassParams->engineType = engineList[i];
        locClassParams->numClasses = 0;
        locClassParams->classList = 0;

        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(masterGpu),
                                LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                                locClassParams, sizeof(LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS)));

        totalClassCount += locClassParams->numClasses;
    }

    // step 3: allocate enough space for all the classes
    classList = new LwU32[totalClassCount];

    // step 4: populate the class list with all the classes from all engines supported
    for (LwU32 i = 0; i < locEngineParams->engineCount; i++)
    {
        locClassParams->engineType = engineList[i];
        locClassParams->classList = LW_PTR_TO_LwP64(classList + counter);

        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(masterGpu),
                                LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                                locClassParams, sizeof(LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS)));

        counter += locClassParams->numClasses;
    }

    locClassParams->classList = LW_PTR_TO_LwP64(classList);

Cleanup:
    return rc;
}

//! \brief removes any undesired engines from engineList
//!
//! \return RC structure denoting the exit status of PrunEngineList
//------------------------------------------------------------------------------
RC EccScrubbedDeallocationTest::PruneEngineList(LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS* locEngineParams,
    const LwU32* locEnginesToKeep, LwU32 keepEnginesCount)
{
    LwU32* engineList = (LwU32*)locEngineParams->engineList;
    LwU32 j;

    if (0 >= keepEnginesCount)
        return RC::BAD_PARAMETER;

    // step 1: choose a supported engine
    for (INT32 i = locEngineParams->engineCount - 1; 0 <= i; i--)
    {
        // step 2: try to find it in our list
        for (j = 0; j < keepEnginesCount; j++)
        {
            if (locEnginesToKeep[j] == engineList[i])
                break;
        }

        // step 3: if we can't find it, remove it from consideration, else keep it
        if (j >= keepEnginesCount)
        {
            LwU32 temp = engineList[i];
            engineList[i] = engineList[locEngineParams->engineCount - 1];
            engineList[locEngineParams->engineCount - 1] = temp;
            locEngineParams->engineCount--;
        }
    }

    return OK;
}

//! \brief removes any undesired engine classes from classList
//!
//! \return RC structure denoting the exit status of PruneClassList
//------------------------------------------------------------------------------
RC EccScrubbedDeallocationTest::PruneClassList(LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS* locClassParams,
    const LwRm::Handle* locClassesToKeep, LwU32 keepClassesCount)
{
    LwU32* classList = (LwU32*)(locClassParams->classList);
    LwU32 j;

    if (0 >= keepClassesCount)
        return RC::BAD_PARAMETER;

    // step 1: choose a supported class
    for (INT32 i = locClassParams->numClasses - 1; 0 <= i; i--)
    {
        // step 2: try to find the class amongst the classes we wish to keep
        for (j = 0; j < keepClassesCount; j++)
        {
            if (locClassesToKeep[j] == classList[i])
                break;
        }

        // step 3: if we can't find it, remove it from consideration, else keep it
        if (j >= keepClassesCount)
        {
            LwU32 temp = classList[i];
            classList[i] = classList[locClassParams->numClasses - 1];
            classList[locClassParams->numClasses - 1] = temp;
            locClassParams->numClasses--;
        }
    }

    return OK;
}

//! \brief allocation a block of memory compliant with CeCopyBlock()
//!
//! CE's can only copy data formatted as 2D surfaces. Here we allocate a
//! '2D surface' which CeCopyBlock() can understand.
//!
//! NOTE: initializes the block to 0
//! NOTE: for EccScrubbedDeallocationTest, surface must be physically contiguous,
//! or else copy engine cannot copy it properly after it has been deallocated
//! WARNING: parameter 'size' is in bytes.
//!
//! \return RC structure denoting the exit status of AllocCeCompliantMem()
//------------------------------------------------------------------------------
RC EccScrubbedDeallocationTest::AllocCeCompliantMem(LwU32 size, Memory::Location loc,
                                            LwU64 addrHint, Surface2D** allocationPointer)
{
    RC rc;

    Surface2D* temp = new Surface2D;
    temp->SetForceSizeAlloc(true);
    temp->SetArrayPitch(SURFACE_PITCH);
    temp->SetArraySize(size);
    temp->SetColorFormat(ColorUtils::A8R8G8B8);
    temp->SetAddressModel(Memory::Paging);
    temp->SetLayout(Surface2D::BlockLinear);

    //
    // Allocating a compressible surface so scrubber will be
    // enabled for emulation and silicon
    //
    temp->SetCompressed(true);
    temp->SetLocation(loc);
    temp->SetGpuPhysAddrHint(addrHint);
    temp->SetPhysContig(true);
    CHECK_RC_CLEANUP(temp->Alloc(GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(temp->Map());
    //temp->Fill(0);
    Platform::FlushCpuWriteCombineBuffer();

    *allocationPointer = temp;
    return rc;

Cleanup:
    delete temp;
    *allocationPointer = 0;
    return rc;
}

//! \brief tells you the supported engine given a supported class
//!
//! Engine type is stored in *engine. Puts LW2080_ENGINE_TYPE_NULL into
//! *engine if class->engine mapping cannot be determined.
//!
//! \return RC structure denoting the exit status of ClassToEngine()
//------------------------------------------------------------------------------
RC EccScrubbedDeallocationTest::ClassToEngine(LwRm::Handle engineClass, LwU32* engine)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS locEngineParams = {0};
    LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS locClassParams = {0};
    vector<LwU32> locClassList;

    // step 1: query the supported engines
    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(masterGpu),
                    LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                    &locEngineParams,
                    sizeof(locEngineParams)));

    // step 2: choose an engine to get all the classes for
    for (LwU32 i = 0; i < locEngineParams.engineCount; i++)
    {
        // step 3: get all the classes for the selected engine
        locClassParams.engineType = locEngineParams.engineList[i];
        locClassParams.numClasses = 0;
        locClassParams.classList = 0;
        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(masterGpu),
                        LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                        &locClassParams, sizeof(locClassParams)));

        locClassList.resize(locClassParams.numClasses);
        locClassParams.classList = LW_PTR_TO_LwP64(&(locClassList[0]));

        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(masterGpu),
                LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                &locClassParams, sizeof(locClassParams)));

        // step 4: check if our class is in that engine
        for (LwU32 j = 0; j < locClassParams.numClasses; j++)
        {
            if (engineClass == locClassList[j])
            {
                *engine = locEngineParams.engineList[i];
                return rc;
            }
        }
    }

Cleanup:
    *engine = LW2080_ENGINE_TYPE_NULL;
    return rc;
}

//! \brief waits for either a notifier to come back or timeout from semaphore to
//!        come back from the CE
//!
//! \return nothing
void EccScrubbedDeallocationTest::WaitCeFinish(LwRm::Handle locEngineClass, ModsEvent* pLocModsEvent)
{
    switch (locEngineClass)
    {
        case BLACKWELL_DMA_COPY_A:
        case HOPPER_DMA_COPY_A:
        case AMPERE_DMA_COPY_B:
        case AMPERE_DMA_COPY_A:
        case VOLTA_DMA_COPY_A:
        case TURING_DMA_COPY_A:
        case PASCAL_DMA_COPY_A:
        case KEPLER_DMA_COPY_A:
        case MAXWELL_DMA_COPY_A:
            do
            {
                if (MEM_RD32(pSema->GetAddress()) == SEMA_CODE)
                    break;

            }while(Tasker::WaitOnEvent(pLocModsEvent, intrTimeout));
            Tasker::ResetEvent(pLocModsEvent);
            return;

        case GF100_DMA_COPY:
        default:
            pCh->WaitIdle(intrTimeout);
            return;
    }
}

//! \brief performs the necessary operations to flush to a given target
//!
//! NOTE: targetLoc should only be L2_CACHE, or FB
//! Warning: SM option lwrrently not implemented
//!
//! \return RC struct denoting the exit status of FlushTo()
RC EccScrubbedDeallocationTest::FlushTo(LwU32 targetLoc)
{
    RC rc;
    LW0080_CTRL_DMA_FLUSH_PARAMS Params = {0};

    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());

    switch (targetLoc)
    {
        case L2_CACHE:
            Params.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT, _L2, _ENABLE);
            CHECK_RC(GetBoundRmClient()->ControlByDevice(GetBoundGpuDevice(),
                                                        LW0080_CTRL_CMD_DMA_FLUSH,
                                                        &Params, sizeof(Params)));
            return rc;

        case FB:
            Params.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                        _L2, _ENABLE)
                              | DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                        _FB, _ENABLE);
            CHECK_RC(GetBoundRmClient()->ControlByDevice(GetBoundGpuDevice(),
                                                       LW0080_CTRL_CMD_DMA_FLUSH,
                                                       &Params, sizeof(Params)));
            CHECK_RC(masterGpu->IlwalidateL2Cache(0));
            return rc;

        default:
            return RC::BAD_PARAMETER;
    }

    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(EccScrubbedDeallocationTest, RmTest, "ECC scrubbed deallocation test.");

