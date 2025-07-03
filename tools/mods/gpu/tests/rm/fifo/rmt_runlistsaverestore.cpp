/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2014,2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_runlistsaverestore.cpp
//! \brief To verify basic functionality of KEPLER_CHANNEL_GROUP_A object
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string>          // Only use <> for built-in C++ stuff
#include <vector>
#include <algorithm>
#include <map>
#include "core/utility/errloggr.h"
#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/notifier.h"
#include "lwRmApi.h"
#include "core/include/utility.h"
#include "random.h"
#include "gpu/include/gralloc.h"
#include "core/include/tasker.h"
#include "gpu/include/notifier.h"
#include "gpu/tests/rm/utility/changrp.h"

#include "ctrl/ctrla06f.h"
#include "ctrl/ctrla06c.h"
#include "ctrl/ctrl2080.h"

#include "class/cla06c.h"
#include "class/cla06f.h"
#include "class/cla16f.h"
#include "class/cla197.h"
#include "class/cla1c0.h"
#include "class/cla0b5.h"
#include "class/cla06fsubch.h"
#include "class/cl9067.h"
#include "class/cl007d.h"

// Must be last
#include "core/include/memcheck.h"

class RunlistSaveRestoreTest : public RmTest
{
    public:
    RunlistSaveRestoreTest();
    virtual ~RunlistSaveRestoreTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(ForceTest, string);

    private:
    RC BasicCopyTest();

    LwU32 GetGrCopyInstance(LwU32);

    RC AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj, LwU32 engineInstance = (LwU32) 0);
    void FreeObjects(Channel *pCh);

    RC CleanObjects();

    bool bRunOnError = false;
    Surface2D m_semaSurf;
    string m_ForceTest;

    map<LwRm::Handle, DmaCopyAlloc> m_ceAllocs;
    bool m_bLateChannelAlloc = false;
};

//! \brief RunlistSaveRestoreTest constructor
//!
//! Placeholder : doesnt do much, much funcationality in Setup()
//!
//! \sa Setup
//------------------------------------------------------------------------------
RunlistSaveRestoreTest::RunlistSaveRestoreTest()
{
    SetName("RunlistSaveRestoreTest");
}

//! \brief RunlistSaveRestoreTest destructor
//!
//! Placeholder : doesnt do much, most functionality in Setup()
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
RunlistSaveRestoreTest::~RunlistSaveRestoreTest()
{

}

//! \brief IsSupported(), Looks for whether test can execute in current elw.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string RunlistSaveRestoreTest::IsTestSupported()
{
    LwRmPtr    pLwRm;
    return pLwRm->IsClassSupported(KEPLER_CHANNEL_GROUP_A, GetBoundGpuDevice()) ? RUN_RMTEST_TRUE : "Channel groups not supported";
}

//! \brief  Setup(): Generally used for any test level allocation
//!
//! Allocating the channel and software object for use in the test
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC RunlistSaveRestoreTest::Setup()
{
    RC rc;
    CHECK_RC(InitFromJs());
    m_TestConfig.SetAllowMultipleChannels(true);

    m_semaSurf.SetForceSizeAlloc(true);
    m_semaSurf.SetArrayPitch(1);
    m_semaSurf.SetArraySize(0x1000);
    m_semaSurf.SetColorFormat(ColorUtils::VOID32);
    m_semaSurf.SetAddressModel(Memory::Paging);
    m_semaSurf.SetLayout(Surface2D::Pitch);
    m_semaSurf.SetLocation(Memory::Fb);
    CHECK_RC(m_semaSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_semaSurf.Map());

    return OK;
}

//! \brief  Cleanup():
//!
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC RunlistSaveRestoreTest::Cleanup()
{
    m_semaSurf.Free();
    return OK;
}

//! \brief  Run():
//!
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC RunlistSaveRestoreTest::Run()
{
    RC rc = OK;
    RC errorRc = OK;

#define RUN_TEST(name)\
    if (!m_ForceTest.length() || m_ForceTest.compare(#name) == 0)\
    {\
        Printf(Tee::PriLow, "%s: %s running \n", __FUNCTION__, #name);\
        rc = name(); \
        if (bRunOnError && rc != OK)\
        {\
            if (errorRc == OK)\
                errorRc = rc;\
            Printf(Tee::PriHigh, "%s: %s failed with status: %s.\n", __FUNCTION__, #name, rc.Message());\
            rc.Clear();\
        }\
        else if (rc == OK)\
            Printf(Tee::PriLow, "%s: %s passed\n", __FUNCTION__, #name);\
        else \
            CHECK_RC(rc);\
        if(CleanObjects() != OK)\
            Printf(Tee::PriHigh, "%s: CleanObjects had to clean state after %s!\n", __FUNCTION__, #name);\
    }\
    else\
    {\
        Printf(Tee::PriLow, "%s: Not running %s\n", __FUNCTION__, #name);\
    }

    // Basic functionality, allocate and send methods ensuring exelwtion order.

    RUN_TEST(BasicCopyTest);

    CHECK_RC(errorRc);
    return rc;
}

static bool modsSleep(void *pArg)
{
    return false;
}

/**
 * @brief The test will submit tsgs at offset 2 and checks if it passes
 *        and then submits the same tsgs with the same workload at
 *        offset 0 and ensures it times out
 * @return
 */
RC RunlistSaveRestoreTest::BasicCopyTest()
{
    StickyRC rc;
    LwRmPtr    pLwRm;
    ChannelGroup chGrp1(&m_TestConfig);
    ChannelGroup chGrp2(&m_TestConfig);
    Channel *pCh1, *pCh2;
    LwRm::Handle hObj1;
    LwRm::Handle hObj2;
    LWA06C_CTRL_TIMESLICE_PARAMS tsParams = {0};
    LW2080_CTRL_FIFO_SUBMIT_RUNLIST_PARAMS submitRunlist = {0};
    int trial = 1;
    chGrp1.SetEngineId(LW2080_ENGINE_TYPE_COPY0);
    chGrp2.SetEngineId(LW2080_ENGINE_TYPE_COPY0);
    CHECK_RC_CLEANUP(chGrp1.Alloc());
    CHECK_RC_CLEANUP(chGrp2.Alloc());

    CHECK_RC_CLEANUP(chGrp1.AllocChannel(&pCh1));
    CHECK_RC_CLEANUP(chGrp2.AllocChannel(&pCh2));

    // timeslice = (TSG_TIMESLICE_TIMEOUT << TSG_TIMESLICE_SCALE) * 1024 nanoseconds
    // #define LW_RAMRL_ENTRY_TSG_TIMESLICE_TIMEOUT     (31+0*32):(24+0*32) /* RWXUF */
    //
    // We can only set a max of 255 << 15 = ~8.35 seconds for a timeslice

    //
    // Set the timeslice to 2 seconds. Halfway through below we check if the semaphore has exelwted yet.
    // In the normal (non-timeout) case, it definitely should have by now
    // In the timeout case, the acquire would still be hanging. Check it before the timeslice expires
    //   and it switches anyway because of timeslice expiration.
    //
    tsParams.timesliceUs = 2000000;

    CHECK_RC_CLEANUP(pLwRm->Control(chGrp1.GetHandle(), LWA06C_CTRL_CMD_SET_TIMESLICE, &tsParams, sizeof(tsParams)));
    CHECK_RC_CLEANUP(pLwRm->Control(chGrp2.GetHandle(), LWA06C_CTRL_CMD_SET_TIMESLICE, &tsParams, sizeof(tsParams)));

    pCh1->SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(true);
    pCh2->SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(true);

    // CHANNEL GROUP 1 ACQUIRE
    CHECK_RC_CLEANUP(AllocObject(pCh1, LWA06F_SUBCHANNEL_COPY_ENGINE, &hObj1));
    // CHANNEL GROUP 2 RELEASE
    CHECK_RC_CLEANUP(AllocObject(pCh2, LWA06F_SUBCHANNEL_COPY_ENGINE, &hObj2));

    while (trial <= 2)
    {
        Printf(Tee::PriHigh, "%s: Running test case %s \n", __FUNCTION__, (trial == 1) ? "POSITIVE" : "NEGATIVE");
        MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x10, 0xdeadbeef);

        Printf(Tee::PriHigh, "%s: Enqueueing acquire on 0x00dead00 methods for TSG1 \n",
               __FUNCTION__);

        // Push acquire
        CHECK_RC_CLEANUP(pCh1->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, hObj1));
        CHECK_RC_CLEANUP(pCh1->Write(LWA06F_SUBCHANNEL_COPY_ENGINE,LWA06F_SEMAPHOREA,LwU64_HI32(m_semaSurf.GetCtxDmaOffsetGpu()+0x10)));
        CHECK_RC_CLEANUP(pCh1->Write(LWA06F_SUBCHANNEL_COPY_ENGINE,LWA06F_SEMAPHOREB,LwU64_LO32(m_semaSurf.GetCtxDmaOffsetGpu()+0x10)));
        CHECK_RC_CLEANUP(pCh1->Write(LWA06F_SUBCHANNEL_COPY_ENGINE,LWA06F_SEMAPHOREC, 0x00dead00));

        CHECK_RC_CLEANUP(pCh1->Write(LWA06F_SUBCHANNEL_COPY_ENGINE,LWA06F_SEMAPHORED,
                           DRF_DEF(A06F, _SEMAPHORED,_OPERATION,_ACQUIRE)|
                   DRF_DEF(A06F, _SEMAPHORED,_ACQUIRE_SWITCH,_DISABLED)));

        // Push release
        Printf(Tee::PriHigh, "%s: Enqueueing release of 0x00dead00 methods for TSG2 \n",
               __FUNCTION__);
        CHECK_RC_CLEANUP(pCh2->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, hObj2));
        CHECK_RC(pCh2->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SEMAPHORE_A,LwU64_HI32(m_semaSurf.GetCtxDmaOffsetGpu() + 0x10)));
        CHECK_RC(pCh2->Write(LWA06F_SUBCHANNEL_COPY_ENGINE,LWA0B5_SET_SEMAPHORE_B,LwU64_LO32(m_semaSurf.GetCtxDmaOffsetGpu() + 0x10)));
        CHECK_RC(pCh2->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SEMAPHORE_PAYLOAD, 0x00dead00));
        CHECK_RC(pCh2->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_LAUNCH_DMA,
                        DRF_DEF(A0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
                        DRF_DEF(A0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                        DRF_DEF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                        DRF_DEF(A0B5, _LAUNCH_DMA,_DATA_TRANSFER_TYPE,_NONE)));

        Printf(Tee::PriHigh, "%s: Enqueing TSGs in the runlist in the order TSG1, TSG2 \n",
               __FUNCTION__);

        // Adds channels for scheduling but does not submit it yet
        CHECK_RC_CLEANUP(chGrp1.Schedule(true, true));
        CHECK_RC_CLEANUP(chGrp2.Schedule(true, true));

        CHECK_RC_CLEANUP(chGrp1.Flush());
        CHECK_RC_CLEANUP(chGrp2.Flush());

        // Submits the channel at offset 2 otherwise test would timeout
        submitRunlist.submitOffset = (trial == 1) ? 1 : 0;
        Printf(Tee::PriHigh, "%s: Calling RM to submit runlist at offset %d \n",
               __FUNCTION__, submitRunlist.submitOffset);

        rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()), LW2080_CTRL_CMD_FIFO_SUBMIT_RUNLIST,
                            &submitRunlist, sizeof(submitRunlist));
        if(rc == LW_OK)
        {
            Printf(Tee::PriHigh, "%s: submitted runlist at offset %d sleep/poll\n", __FUNCTION__, submitRunlist.submitOffset);


            Printf(Tee::PriHigh,
                   "%s: Check with RM if TSG1 is idle \n"
                   "- for positive test TSG1 should be idle and never timeout \n"
                   "- for negative test TSG1 should never idle and the RM channel idle test should timeout\n",
                   __FUNCTION__);

            Printf(Tee::PriHigh, "%s: Dumping semaphore before wait for idle %x \n", __FUNCTION__,
                            MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10));

            // Wait for 1 second - halfway through the timeslice
            (void) POLLWRAP(&modsSleep, NULL, 1000);

            Printf(Tee::PriHigh, "%s: Dumping semaphore after wait for idle %x \n", __FUNCTION__,
                            MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10));
            Printf(Tee::PriHigh, "%s: Test completed check for semaphore validity:\n", __FUNCTION__);
            if (MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10) == 0x00dead00)
            {
                if (trial == 2)
                {
                    rc = RC::DATA_MISMATCH;
                    Printf(Tee::PriHigh, "%s: NEG TEST FAILED: Unexpectedly semaphore matched %x \n", __FUNCTION__,
                            MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10));
                    break;
                }
                else
                {
                    Printf(Tee::PriHigh, "%s: POS TEST PASSED: Semaphore matched %x \n", __FUNCTION__,
                            MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10));

                    rc.Clear();
                }
            }
            else
            {
                if (trial == 2)
                {
                    Printf(Tee::PriHigh, "%s: NEG TEST PASSED: Semaphore did not match as expected %x \n", __FUNCTION__,
                            MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10));
                    rc.Clear();
                    break;
                }
                else
                {
                    rc = RC::DATA_MISMATCH;
                    Printf(Tee::PriHigh, "%s: POS TEST FAILED Unexpected semaphore mismatch - test didnt run?%x \n", __FUNCTION__,
                            MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10));
                    break;
                }
            }

            rc.Clear();
        }
        else
        {
            Printf(Tee::PriHigh, "ERROR: Submission failed\n");
            goto Cleanup;
        }

        CHECK_RC_CLEANUP(chGrp1.Schedule(false, false));
        CHECK_RC_CLEANUP(chGrp2.Schedule(false, false));

        trial++;
   }

   Printf(Tee::PriHigh, "%s: TEST COMPLETED Releasing semaphore 00dead00 from CPU %x \n", __FUNCTION__,
                            MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10));

   // Test is complete, let the channel idle by releasing the semaphore, before cleanup.
   MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x10, 0x00dead00);

   Printf(Tee::PriHigh, "%s: Cleaning up TSG1 and TSG2 \n", __FUNCTION__);

   // rc =chGrp1.WaitIdle();
   // Printf(Tee::PriHigh, "%s: Cleaning up after waiting for TSG1 to idle %s \n", __FUNCTION__, (rc ==LW_OK) ? "SUCCESS" : "FAIL");

   // rc =chGrp2.WaitIdle();
   // Printf(Tee::PriHigh, "%s: Cleaning up after waiting for TSG2 to idle %s \n", __FUNCTION__, (rc ==LW_OK) ? "SUCCESS" : "FAIL");

Cleanup:
   FreeObjects(chGrp1.GetChannel(0));
   FreeObjects(chGrp2.GetChannel(0));
   chGrp1.Free();
   chGrp2.Free();

   Printf(Tee::PriHigh, "%s: FREED all resources returning %s \n", __FUNCTION__, (rc ==LW_OK) ? "SUCCESS" : "FAIL");
   return rc;
}

RC RunlistSaveRestoreTest::AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj, LwU32 engineInstance)
{
    RC rc;
    LwRm::Handle hCh = pCh->GetHandle();
    MASSERT(hObj);

    if (engineInstance == (LwU32) -1)
         engineInstance = GetGrCopyInstance(pCh->GetClass());
    CHECK_RC(m_ceAllocs[hCh].AllocOnEngine(hCh,
                                           LW2080_ENGINE_TYPE_COPY(engineInstance),
                                           GetBoundGpuDevice(),
                                           LwRmPtr().Get()));
    *hObj = m_ceAllocs[hCh].GetHandle();
    return rc;
}

void RunlistSaveRestoreTest::FreeObjects(Channel *pCh)
{
    LwRm::Handle hCh = pCh->GetHandle();

    if (m_ceAllocs.count(hCh)){ m_ceAllocs[hCh].Free(); m_ceAllocs.erase(hCh); }
}

RC RunlistSaveRestoreTest::CleanObjects()
{
    RC rc = OK;
    if (!m_ceAllocs.empty()){ m_ceAllocs.clear(); rc = -1; }
    return rc;
}

LwU32 RunlistSaveRestoreTest::GetGrCopyInstance(LwU32 parentClass)
{
    LwRmPtr pLwRm;
    RC rc;
    LW2080_CTRL_GPU_GET_ENGINE_PARTNERLIST_PARAMS partnerParams = {0};

    partnerParams.engineType = LW2080_ENGINE_TYPE_GRAPHICS;
    partnerParams.partnershipClassId = parentClass;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
            LW2080_CTRL_CMD_GPU_GET_ENGINE_PARTNERLIST,
            &partnerParams, sizeof(partnerParams));
    MASSERT(rc == OK);
    MASSERT(partnerParams.numPartners == 1);

    return partnerParams.partnerList[0] - LW2080_ENGINE_TYPE_COPY0;
}

JS_CLASS_INHERIT(RunlistSaveRestoreTest, RmTest,
    "RunlistSaveRestoreTest RMTEST that tests RUNIST SAVE RESTORE functionality");
CLASS_PROP_READWRITE(RunlistSaveRestoreTest, ForceTest, string,
        "Run specific test");

