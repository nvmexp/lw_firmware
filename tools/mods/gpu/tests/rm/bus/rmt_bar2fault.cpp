/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_bar2fault.cpp.cpp
//! \brief To verify basic BAR2 fault handling in RM
//! \brief This test does the following :
//! \brief 1. Generate a BAR2 fault by writing a zero value to BAR2 bind register
//! \brief 2. Check whether we receive the RC notification on a test channel
//! \brief 3. Test whether RM allocations fail after BAR2 fault
//! \brief 4. Do GPU reset and re-init RM
//! \brief 5. Test whether RM allocations succeed
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "lwos.h"
#include "gpu/include/gpudev.h"
#include "core/include/refparse.h"

#include "class/clc369.h"

// Must be last
#include "core/include/memcheck.h"

class Bar2FaultTest : public RmTest
{
    public:
    Bar2FaultTest();
    virtual ~Bar2FaultTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    private:
    Surface2D *m_pSemaSurf;
    Channel *m_pChan[2];
    LwRm::Handle m_hChan[2];
    LwRm::Handle m_hFaultBuffer;
    LwRm::Handle m_hNotifyEvent;
    FLOAT64 m_timeoutMs;
    ModsEvent *m_pModsEvent;
};

//! \brief Bar2FaultTest constructor
//!
//------------------------------------------------------------------------------
Bar2FaultTest::Bar2FaultTest()
{
    SetName("Bar2FaultTest");

    for (int i = 0; i < 2; i++)
    {
        m_pChan[i] = NULL;
        m_hChan[i] = 0;
    }
    m_pSemaSurf = NULL;
    m_timeoutMs = 0;
    m_hFaultBuffer = 0;
    m_pModsEvent = NULL;
    m_hNotifyEvent = 0;
}

//! \brief Bar2FaultTest destructor
//!
//------------------------------------------------------------------------------
Bar2FaultTest::~Bar2FaultTest()
{
}

//! \brief IsSupported(), Looks for whether test can execute in current elw.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Bar2FaultTest::IsTestSupported()
{
    if (IsClassSupported(MMU_FAULT_BUFFER))
        return RUN_RMTEST_TRUE;
    return "MMU_FAULT_BUFFER class not supported on this chip";
}

//! \brief  Setup(): Generally used for any test level allocation
//!
//! \return corresponding RC if setup fails
//------------------------------------------------------------------------------
RC Bar2FaultTest::Setup()
{
    RC      rc;
    LwRmPtr pLwRm;
    LwU32 mmuFaultBufferAllocParams = {0};

    CHECK_RC(InitFromJs());

    // Setup 2D semaphore surface
    m_pSemaSurf = new Surface2D();
    m_pSemaSurf->SetForceSizeAlloc(true);
    m_pSemaSurf->SetArrayPitch(1);
    m_pSemaSurf->SetArraySize(0x1000);
    m_pSemaSurf->SetColorFormat(ColorUtils::VOID32);
    m_pSemaSurf->SetAddressModel(Memory::Paging);
    m_pSemaSurf->SetLayout(Surface2D::Pitch);
    m_pSemaSurf->SetLocation(Memory::Fb);
    CHECK_RC(m_pSemaSurf->Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_pSemaSurf->Map());

    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);
    m_TestConfig.SetAllowMultipleChannels(true);
    m_timeoutMs = GetTestConfiguration()->TimeoutMs();
    for (UINT32 i = 0; i < 2; i++)
    {
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pChan[i], &m_hChan[i]));
    }

    m_pModsEvent = Tasker::AllocEvent();
    if (m_pModsEvent)
    {
        CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()), &m_hFaultBuffer,
            MMU_FAULT_BUFFER, &mmuFaultBufferAllocParams));

        //Associcate Event to Object
        void* const pOsEvent = Tasker::GetOsEvent(m_pModsEvent,
            pLwRm->GetClientHandle(), pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()));

        CHECK_RC(pLwRm->AllocEvent(m_hFaultBuffer, &m_hNotifyEvent,
            LW01_EVENT_OS_EVENT, LWC369_NOTIFIER_MMU_FAULT_NON_REPLAYABLE_IN_PRIV, pOsEvent));
    }
    else
    {
        CHECK_RC(RC::LWRM_INSUFFICIENT_RESOURCES);
    }

    return OK;
}

//! \brief  Cleanup():
//!
//------------------------------------------------------------------------------
RC Bar2FaultTest::Cleanup()
{
    LwRmPtr pLwRm;

    if (m_pSemaSurf)
    {
        m_pSemaSurf->Free();
        delete m_pSemaSurf;
    }

    for (UINT32 i = 0; i < 2; i++)
    {
        m_TestConfig.FreeChannel(m_pChan[i]);
    }

    if (m_hNotifyEvent)
    {
        pLwRm->Free(m_hNotifyEvent);
    }

    if (m_hFaultBuffer)
    {
        pLwRm->Free(m_hFaultBuffer);
    }

    if (m_pModsEvent)
    {
        Tasker::FreeEvent(m_pModsEvent);
    }

    return OK;
}

//! \brief  Run():
//!
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC Bar2FaultTest::Run()
{
    RC rc = OK;
    RC errorRc = OK;
    Channel *pTempChan;
    LwRm::Handle hTempChan = 0;
    Surface2D tempSurf;
    LwRmPtr pLwRm;
    UINT32 regVal = 0;
    PHYSADDR physAddr = 0;
    UINT32 physAddr32 = 0;
    RefManualRegister const *pRefManReg;
    RefManualRegisterField const *pRefManRegFld;
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();
    GpuDevice *pDev = GetBoundGpuDevice();
    RefManual *pRefMan = pSubdev->GetRefManual();

    // To avoid test failure due to BAR2 interrupt
    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    for (UINT32 i = 0; i < 2; i++)
    {
        CHECK_RC_CLEANUP(m_pChan[i]->ScheduleChannel(true));
    }

    // Test channel works fine before BAR2 fault
    MEM_WR32(m_pSemaSurf->GetAddress(), 0);

    CHECK_RC_CLEANUP(m_pChan[0]->SetSemaphoreOffset(m_pSemaSurf->GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(m_pChan[0]->SemaphoreRelease(0x1234));
    CHECK_RC_CLEANUP(m_pChan[0]->Flush());
    CHECK_RC_CLEANUP(m_pChan[0]->WaitIdle(m_timeoutMs));

    CHECK_RC_CLEANUP(m_pChan[1]->SetSemaphoreOffset(m_pSemaSurf->GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(m_pChan[1]->SemaphoreAcquire(0x1234));
    CHECK_RC_CLEANUP(m_pChan[1]->Flush());
    CHECK_RC_CLEANUP(m_pChan[1]->WaitIdle(m_timeoutMs));

    for (UINT32 i = 0; i < 2; i++)
    {
        errorRc = m_pChan[i]->CheckError();
        CHECK_RC_CLEANUP(errorRc);
    }

    tempSurf.SetForceSizeAlloc(true);
    tempSurf.SetArrayPitch(1);
    tempSurf.SetArraySize(0x1000);
    tempSurf.SetColorFormat(ColorUtils::VOID32);
    tempSurf.SetAddressModel(Memory::Paging);
    tempSurf.SetLayout(Surface2D::Pitch);
    tempSurf.SetLocation(Memory::Fb);
    CHECK_RC_CLEANUP(tempSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(tempSurf.Map());

#define SF_OFFSET(sf)           (((0?sf)/32)<<2)
#define SF_SHIFT(sf)            ((0?sf)&31)
#define SF_DEF(s,f,c)           ((LW ## s ## f ## c)<<SF_SHIFT(LW ## s ## f))

#define LW_RAMIN_PAGE_DIR_BASE_TARGET               (128*32+1):(128*32+0) /* RWXUF */
#define LW_RAMIN_PAGE_DIR_BASE_TARGET_ILWALID                  0x00000001 /* RW--V */

    Memory::Fill32(tempSurf.GetAddress(), 0x0,  LwU64_LO32(tempSurf.GetSize()/sizeof(UINT32)));
    MEM_WR32((LwU8 *)tempSurf.GetAddress() + SF_OFFSET(LW_RAMIN_PAGE_DIR_BASE_TARGET),  SF_DEF(_RAMIN_PAGE_DIR_BASE, _TARGET, _ILWALID));

    // Trigger BAR2 fault by writing value zero for instnace block ptr
    pRefManReg = pRefMan->FindRegister("LW_PBUS_BAR2_BLOCK");
    pRefManRegFld = pRefManReg->FindField("LW_PBUS_BAR2_BLOCK_PTR");
    regVal = pSubdev->RegRd32(pRefManReg->GetOffset());
    regVal = regVal & ~pRefManRegFld->GetMask();
    CHECK_RC_CLEANUP(tempSurf.GetPhysAddress(0, &physAddr));
    physAddr32 = LwU64_LO32(physAddr >> 12) << pRefManRegFld->GetLowBit();
    MASSERT((physAddr32 & ~pRefManRegFld->GetMask()) == 0);
    regVal |= physAddr32;
    pSubdev->RegWr32(pRefManReg->GetOffset(), regVal);
    Platform::FlushCpuWriteCombineBuffer();

    // Read the BAR2 BLOCK register to ensure that the write happened.
    regVal = pSubdev->RegRd32(pRefManReg->GetOffset());

    //
    // Poll the BIND STATUS register.
    // Only applicable for MAXWELL+ chips.
    //
    pRefManReg = pRefMan->FindRegister("LW_PBUS_BIND_STATUS");
    if (pRefManReg)
    {
        RefManualRegisterField const *pRefManRegFld2;

        pRefManRegFld = pRefManReg->FindField("LW_PBUS_BIND_STATUS_BAR2_PENDING");
        pRefManRegFld2 = pRefManReg->FindField("LW_PBUS_BIND_STATUS_BAR2_OUTSTANDING");
        UINT32 cntr = 100000;
        while(cntr)
        {
            regVal = pSubdev->RegRd32(pRefManReg->GetOffset());
            if(((regVal & pRefManRegFld->GetMask()) == 0) &&
               ((regVal & pRefManRegFld2->GetMask()) == 0))
            {
                break;
            }
            cntr--;
            Tasker::Yield();
        }

        if (cntr == 0)
        {
            CHECK_RC_CLEANUP(RC::UNEXPECTED_RESULT);
        }
    }

    DISABLE_BREAK_COND(nobp, true);
    // Generate a BAR2 fault during page table update
    tempSurf.Unmap();

    CHECK_RC_CLEANUP(Tasker::WaitOnEvent(m_pModsEvent, m_timeoutMs));

    DISABLE_BREAK_END(nobp);

    if ((m_pChan[0]->CheckError() == RC::RM_RCH_PREEMPTIVE_REMOVAL) &&
        (m_pChan[1]->CheckError() == RC::RM_RCH_PREEMPTIVE_REMOVAL))
    {
        Printf(Tee::PriHigh, "%s: Received BAR2 fault notification as expected\n", __FUNCTION__);

        for (UINT32 i = 0; i < 2; i++)
        {
            CHECK_RC_CLEANUP(m_TestConfig.FreeChannel(m_pChan[i]));
            m_pChan[i] = NULL;
            m_hChan[i] = 0;
        }

        m_pSemaSurf->Free();
        delete m_pSemaSurf;
        m_pSemaSurf = NULL;

        pLwRm->Free(m_hNotifyEvent);
        m_hNotifyEvent = 0;

        pLwRm->Free(m_hFaultBuffer);
        m_hFaultBuffer = 0;

        Tasker::FreeEvent(m_pModsEvent);
        m_pModsEvent = 0;

        tempSurf.Free();

        // Test whether further allocations fail by doing a channel and surface alloc
        DISABLE_BREAK_COND(nobp, true);
        errorRc = m_TestConfig.AllocateChannel(&pTempChan, &hTempChan);
        DISABLE_BREAK_END(nobp);

        if (errorRc == OK)
        {
            m_TestConfig.FreeChannel(pTempChan);
            Printf(Tee::PriHigh, "%s: Error: Channel alloc suceeded. Should have failed\n", __FUNCTION__);
            CHECK_RC_CLEANUP(RC::UNEXPECTED_RESULT);
        }
        else
        {
            Printf(Tee::PriHigh, "%s: Channel alloc failed as expected\n", __FUNCTION__);
            MASSERT(errorRc == RC::LWRM_RESET_REQUIRED);

            tempSurf.SetForceSizeAlloc(true);
            tempSurf.SetArrayPitch(1);
            tempSurf.SetArraySize(0x1000);
            tempSurf.SetColorFormat(ColorUtils::VOID32);
            tempSurf.SetAddressModel(Memory::Paging);
            tempSurf.SetLayout(Surface2D::Pitch);
            tempSurf.SetLocation(Memory::Fb);

            DISABLE_BREAK_COND(nobp, true);
            errorRc = tempSurf.Alloc(GetBoundGpuDevice());
            DISABLE_BREAK_END(nobp);

            if (errorRc == OK)
            {
                tempSurf.Free();
                Printf(Tee::PriHigh, "%s: Error: Surface alloc suceeded. Should have failed\n", __FUNCTION__);
                CHECK_RC_CLEANUP(RC::UNEXPECTED_RESULT);
            }
            else
            {
                Printf(Tee::PriHigh, "%s: Surface alloc failed as expected\n", __FUNCTION__);
                MASSERT(errorRc == RC::LWRM_RESET_REQUIRED);
            }

            // Do reset and recover
            DISABLE_BREAK_COND(nobp, true);
            pDev->Reset(LW0080_CTRL_BIF_RESET_FLAGS_TYPE_SW_RESET);
            pDev->RecoverFromReset();
            DISABLE_BREAK_END(nobp);

            errorRc = m_TestConfig.AllocateChannel(&pTempChan, &hTempChan);
            if (errorRc == OK)
            {
                Printf(Tee::PriHigh, "%s: Channel alloc succeeded\n", __FUNCTION__);
                m_TestConfig.FreeChannel(pTempChan);

                errorRc = tempSurf.Alloc(GetBoundGpuDevice());
                if (errorRc == OK)
                {
                    tempSurf.Free();
                    Printf(Tee::PriHigh, "%s: Surface alloc succeeded\n", __FUNCTION__);
                }
                else
                {
                    Printf(Tee::PriHigh, "%s: Error: Surface alloc failed\n", __FUNCTION__);
                    CHECK_RC_CLEANUP(RC::UNEXPECTED_RESULT);
                }
            }
            else
            {
                Printf(Tee::PriHigh, "%s: Error: Channel alloc failed\n", __FUNCTION__);
                CHECK_RC_CLEANUP(RC::UNEXPECTED_RESULT);
            }
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%s: Error: Did not received BAR2 fault notification.\n",
               __FUNCTION__);
        CHECK_RC_CLEANUP(RC::UNEXPECTED_RESULT);
    }

Cleanup:
    ErrorLogger::TestCompleted();
    return rc;
}

JS_CLASS_INHERIT(Bar2FaultTest, RmTest,
    "Bar2FaultTest RMTEST that tests BAR2 fault handling in RM");
