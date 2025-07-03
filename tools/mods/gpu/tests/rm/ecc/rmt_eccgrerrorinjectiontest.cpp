/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2016,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_eccgrerrorinjectiontest.cpp
//! \brief Checks that graphics injections work correctly.
//!        This test checks SM LRF and SM SHM.
//!

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "class/clc097.h"       // PASCAL_A
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_COPY_ENGINE
#include "class/cl0000.h"       // LW01_NULL_OBJECT
#include "ctrl/ctrl208f.h"      // LW208F_CTRL_CMD_GR_*_ECC_*
#include "class/cl90e6.h"       // GF100_SUBDEVICE_MASTER
#include "ctrl/ctrl90e6.h"
#include "core/utility/errloggr.h"
#include <vector>
#include "gpu/tests/rmtest.h"
#include "gpu/utility/ecccount.h"
#include "core/include/platform.h"
#include "sim/IChip.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/include/gpudev.h"
#include "gpu/utility/bglogger.h"
#include "gpu/utility/modscnsl.h"
#include "class/cl0005.h"
#include "gpu/utility/chanwmgr.h"
#include "gpu/tests/lwca/ecc/eccdefs.h"
#include "core/include/utility.h"
#include "gpu/include/gralloc.h"
#include "rmt_eccgrerrorinjectiontest.h"
#include "core/include/memcheck.h"

// function declarations
static void EccCallback(void *parg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status);
static void PrintEccErrorCount(EccErrCounter* pErrCounter);

// type definitions
typedef struct
{
    LwU32 eccEventType; //input parameter
    LwU32 errLocation; //input parameter
    bool eccEventOclwrred; //return value set by EccCallback
    bool eccIsInterruptSet; //return value set by EccCallback
    LW90E6_CTRL_MASTER_GET_ECC_INTR_OFFSET_MASK_PARAMS *eccOffsetParams;
} EccCallbackParams;

// static variables
static void * m_InterruptLoc;

// class definition
class EccGrErrorInjectionTest : public RmTest
{
public:
    EccGrErrorInjectionTest();
    virtual ~EccGrErrorInjectionTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRm::Handle m_eccSbeEvent;
    LwRm::Handle m_eccDbeEvent;
    LwRm::Handle m_hClient;
    LwRm::Handle m_sbdvHandle; //mapped to m_InterruptLoc
    LwRm::Handle m_hSubDev;
    LwRm::Handle m_hSubDevDiag;

    LWOS10_EVENT_KERNEL_CALLBACK_EX eccSbeCallback;
    LWOS10_EVENT_KERNEL_CALLBACK_EX eccDbeCallback;
    LW90E6_CTRL_MASTER_GET_ECC_INTR_OFFSET_MASK_PARAMS eccOffsetParams;

    EccErrCounter *pEccErrCounter;
    EccCallbackParams eccCallbackParams;
    bool supportedLocations[NUM_ECC_LOCATIONS];
    void SetErrorParams(LwU32 *errorLoc, LwU32 *errorType);
    RC InjectError(LwU32 errorLoc, LwU32 errorType);
    RC StartExpectingErrors();
    RC StopExpectingErrors();

    GpuDevice *pGpuDev;
    GpuSubdevice *pGpuSubdev;
    LwRm *pLwRm;

    GrClassAllocator *pGrAlloc;
    FLOAT64 intrTimeout;
};

//------------------------------------------------------------------------------
EccGrErrorInjectionTest::EccGrErrorInjectionTest()
{
    m_eccSbeEvent = 0;
    m_eccDbeEvent = 0;
    memset(&eccSbeCallback, 0, sizeof(eccSbeCallback));
    memset(&eccDbeCallback, 0, sizeof(eccDbeCallback));
    memset(&eccDbeCallback, 0, sizeof(eccOffsetParams));
    memset(&supportedLocations, 0, sizeof(supportedLocations));

    pEccErrCounter = 0;
    intrTimeout = 0;
    memset(&eccCallbackParams, 0, sizeof(eccCallbackParams));

    pLwRm = GetBoundRmClient();
    pGpuDev = GetBoundGpuDevice();
    pGpuSubdev = GetBoundGpuSubdevice();
    pGrAlloc = new ThreeDAlloc();
    pGrAlloc->SetOldestSupportedClass(PASCAL_A);

    SetName("EccGrErrorInjectionTest");
}

//------------------------------------------------------------------------------
EccGrErrorInjectionTest::~EccGrErrorInjectionTest()
{
}

//! \brief IsTestSupported() checks if the the master GPU is correctly initialized
//!
//! EccGrErrorInjectionTest needs: ECC enabled, SHM enabled,
//!                                 and EccErrCounter initialized
//!
//! \return returns RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         else returns "NOT SUPPORTED"
//------------------------------------------------------------------------------
string EccGrErrorInjectionTest::IsTestSupported()
{
    LW2080_CTRL_GPU_QUERY_ECC_STATUS_PARAMS EccStatus;

    // first check if the chip is supported
    // only pascal+ has the error injection registers
    if(!pGrAlloc->IsSupported(pGpuDev))
    {
        return "Chip not supported";
    }

    // next, get a list of ECC capabilities from RM
    memset(&EccStatus, 0, sizeof(EccStatus));
    if (OK != pLwRm->ControlBySubdevice(pGpuSubdev,
                                        LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS,
                                        &EccStatus, sizeof(EccStatus)))
    {
        return "ECC not Enabled";
    }

    // check if LRF or SHM is supported
    if (EccStatus.units[LW2080_CTRL_GPU_ECC_UNIT_SM].enabled)
    {
        supportedLocations[LRF] = true;
    }

    if (EccStatus.units[LW2080_CTRL_GPU_ECC_UNIT_SHM].enabled)
    {
        supportedLocations[SHM] = true;
    }

    if (EccStatus.units[LW2080_CTRL_GPU_ECC_UNIT_SM_L1_DATA].enabled)
    {
        supportedLocations[L1DATA] = true;
    }

    if (EccStatus.units[LW2080_CTRL_GPU_ECC_UNIT_SM_L1_TAG].enabled)
    {
        supportedLocations[L1TAG] = true;
    }

    if (EccStatus.units[LW2080_CTRL_GPU_ECC_UNIT_SM_CBU].enabled)
    {
        supportedLocations[CBU_PAR] = true;
    }

    // fail if no GR ECC components are supported
    if (!supportedLocations[LRF] && !supportedLocations[SHM]
        && !supportedLocations[L1DATA] && !supportedLocations[L1TAG]
        && !supportedLocations[CBU_PAR])
    {
        return "lw2080CtrlCmdGpuQueryEccStatus: no ECC unit is supported";
    }

    // verify that EccErrCounter is ready
    if (!pGpuSubdev->GetEccErrCounter()->IsInitialized())
    {
        return "NOT SUPPORTED - Mods EccErrCounter not ready";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Makes all the necessary allocations for EccGrErrorInjectionTest
//! This registers a callback and registers SBE and DBE notifiers
//!
//! \return RC structure to denote the error status of Setup()
//------------------------------------------------------------------------------
RC EccGrErrorInjectionTest::Setup()
{
    RC rc;
    LwRm *pLwRm = GetBoundRmClient();
    int temp = 0;
    LW0005_ALLOC_PARAMETERS allocSbeParams = {0};
    LW0005_ALLOC_PARAMETERS allocDbeParams = {0};
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventSbeParams = {0};
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventDbeParams = {0};

    // step 0: get basic gpu info
    m_hClient = pLwRm->GetClientHandle();
    m_hSubDev = pLwRm->GetSubdeviceHandle(pGpuSubdev);
    m_hSubDevDiag = pGpuSubdev->GetSubdeviceDiag();

    // step 1: get the basic parameters of this test
    CHECK_RC(InitFromJs());
    intrTimeout = m_TestConfig.TimeoutMs();

    //step 4: setup callbacks for ECC SBE & DBE
    //step 4a: register EccCallback() for SBE interrupts
    eccSbeCallback.func = EccCallback;
    eccSbeCallback.arg = &eccCallbackParams;

    allocSbeParams.hParentClient = pLwRm->GetClientHandle();
    allocSbeParams.hClass = LW01_EVENT_KERNEL_CALLBACK_EX;
    allocSbeParams.notifyIndex = LW2080_NOTIFIERS_ECC_SBE;
    allocSbeParams.data = LW_PTR_TO_LwP64(&eccSbeCallback);

    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(pGpuSubdev),
            &m_eccSbeEvent,
            LW01_EVENT_KERNEL_CALLBACK_EX,
            &allocSbeParams));

    //step 4b: register EccCallback() for DBE interrupts
    eccDbeCallback.func = EccCallback;
    eccDbeCallback.arg = &eccCallbackParams;

    allocDbeParams.hParentClient = pLwRm->GetClientHandle();
    allocDbeParams.hClass = LW01_EVENT_KERNEL_CALLBACK_EX;
    allocDbeParams.notifyIndex = LW2080_NOTIFIERS_ECC_DBE;
    allocDbeParams.data = LW_PTR_TO_LwP64(&eccDbeCallback);

    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(pGpuSubdev),
                          &m_eccDbeEvent,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocDbeParams));

    //step 4c: register to listen to ECC SBE notifier
    eventSbeParams.event = LW2080_NOTIFIERS_ECC_SBE;
    eventSbeParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpuSubdev),
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &eventSbeParams, sizeof(eventSbeParams)));

    //step 4d: register to listen to ECC DBE notifier
    eventDbeParams.event = LW2080_NOTIFIERS_ECC_DBE;
    eventDbeParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpuSubdev),
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &eventDbeParams, sizeof(eventDbeParams)));

    //step 5: prep EccErrCounter
    CHECK_RC(ErrorLogger::StartingTest());
    pEccErrCounter = pGpuSubdev->GetEccErrCounter();

    //step 6: Allocate subdevice master to map the top level interrupt
    // They will be mapped to m_InterruptLoc to be read from an offset and
    // mask that is obtained from  the LW90E6_CTRL_CMD_MASTER_GET_ECC_INTR_OFFSET_MASK
    // control call
    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(pGpuSubdev),
                &m_sbdvHandle,
                GF100_SUBDEVICE_MASTER,
                (void *)&temp));

    CHECK_RC(pLwRm->MapMemory(m_sbdvHandle, 0, sizeof(GF100MASTERMap),
                              &m_InterruptLoc, DRF_DEF(OS33, _FLAGS, _ACCESS, _READ_ONLY),
                              pGpuSubdev ));

    CHECK_RC(pLwRm->Control(m_sbdvHandle, LW90E6_CTRL_CMD_MASTER_GET_ECC_INTR_OFFSET_MASK,
                      &eccOffsetParams, sizeof(eccOffsetParams)));

    eccCallbackParams.eccOffsetParams = &eccOffsetParams;

    //step 7: Allocate a graphics channel
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));
    CHECK_RC(pGrAlloc->Alloc(m_hCh, pGpuDev, pLwRm));

    return rc;
}

//! \brief Runs the EccGrErrorInjectionTest
//!
//! Tests if RM upholds the ISR convention of first setting the notifier and then
//! clearing interrupt bits. Triggers ECC SBE and DBE on FB only.
//!
//! \return RC structure to denote the error status of Run()
//------------------------------------------------------------------------------
RC EccGrErrorInjectionTest::Run()
{
    RC rc;
    LwU32 i = 0, j = 0;

    // setup bglogger (mods tests require this)
    BgLogger::PauseBgLogger DisableBgLogger;

    // tell mods to start expecting errors
    StartExpectingErrors();

    // loop over ECC locations (LRF, SHM)
    for(i = 0; i < NUM_ECC_LOCATIONS; i++)
    {
        // loop over ECC types (SBE, DBE)
        for(j = 0; j < NUM_ECC_TYPES; j++)
        {
            // only handle an ecc type if it is supported
            if(supportedLocations[i])
            {
                if (i == CBU_PAR && j == SBE)
                {
                    continue;
                }

                // initialize event parameters
                eccCallbackParams.eccIsInterruptSet = false;
                eccCallbackParams.eccEventOclwrred = false;

                // choose where/what kind of error to inject
                // LRF/SHM/L1Tag/L1Data/CBU, SBE/DBE
                eccCallbackParams.errLocation = i;
                eccCallbackParams.eccEventType = j;

                // inject the error
                InjectError(eccCallbackParams.errLocation, eccCallbackParams.eccEventType);

                // test that the graphics errors got injected. We only need to
                // sleep for an instant to let the interrupts be serviced, but
                // we sleep for the amount of time needed to not be considered
                // an interrupt storm.
                Tasker::Sleep(intrTimeout);

                if (false == eccCallbackParams.eccEventOclwrred)
                {
                    Printf(Tee::PriNormal,
                           "injected error location: 0x%X injected error type: 0x%X\n",
                           (unsigned int) eccCallbackParams.errLocation,
                           (unsigned int) eccCallbackParams.eccEventType);
                    return RC::LWRM_IRQ_NOT_FIRING;
                }
                else if (false == eccCallbackParams.eccIsInterruptSet)
                {
                    Printf(Tee::PriNormal,
                           "injected error location: 0x%X injected error type: 0x%X\n",
                           (unsigned int) eccCallbackParams.errLocation,
                           (unsigned int) eccCallbackParams.eccEventType);
                    return RC::ILWALID_IRQ;
                }

                // Update counters and print them
                CHECK_RC(pEccErrCounter->Update());
                PrintEccErrorCount(pEccErrCounter);
            }
        }
    }

    // tell mods to stop expecting errors
    StopExpectingErrors();
    return rc;
}

//! \brief tells mods to start expecting errors
//!
//! \return void
//------------------------------------------------------------------------------
RC EccGrErrorInjectionTest::StartExpectingErrors()
{
    RC rc;

    if (supportedLocations[LRF])
    {
        CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::SM_CORR));
        CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::SM_UNCORR));
    }

    if (supportedLocations[SHM])
    {
        CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::SHM_CORR));
        CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::SHM_UNCORR));
    }

    if (supportedLocations[L1TAG])
    {
        CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::L1TAG_CORR));
        CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::L1TAG_UNCORR));
    }

    if (supportedLocations[L1DATA])
    {
        CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::L1DATA_CORR));
        CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::L1DATA_UNCORR));
    }

    if (supportedLocations[CBU_PAR])
    {
        CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::CBU_CORR));
        CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::CBU_UNCORR));
    }

    return rc;
}

//! \brief tells mods to stop expecting errors
//!
//! \return void
//------------------------------------------------------------------------------
RC EccGrErrorInjectionTest::StopExpectingErrors()
{
    RC rc;

    if (supportedLocations[LRF])
    {
        CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::SM_CORR));
        CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::SM_UNCORR));
    }

    if (supportedLocations[SHM])
    {
        CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::SHM_CORR));
        CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::SHM_UNCORR));
    }

    if (supportedLocations[L1TAG])
    {
        CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::L1TAG_CORR));
        CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::L1TAG_UNCORR));
    }

    if (supportedLocations[L1DATA])
    {
        CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::L1DATA_CORR));
        CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::L1DATA_UNCORR));
    }

    if (supportedLocations[CBU_PAR])
    {
        CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::CBU_CORR));
        CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::CBU_UNCORR));
    }

    return rc;
}

//! \brief injects either a SBE or a DBE onto a given error location
//!
//! \return RC struct denoting the exit status of InjectError()
//------------------------------------------------------------------------------
void EccGrErrorInjectionTest::SetErrorParams(LwU32 *errorLoc, LwU32 *errorType)
{
    switch(*errorLoc) {
        case LRF:
            *errorLoc = FLD_SET_DRF(208F_CTRL_GR, _ECC_INJECT_ERROR, _UNIT, _LRF, *errorLoc);
            break;

        case SHM:
            *errorLoc = FLD_SET_DRF(208F_CTRL_GR, _ECC_INJECT_ERROR, _UNIT, _SHM, *errorLoc);
            break;

        case L1DATA:
            *errorLoc = FLD_SET_DRF(208F_CTRL_GR, _ECC_INJECT_ERROR, _UNIT, _L1DATA, *errorLoc);
            break;

        case L1TAG:
            *errorLoc = FLD_SET_DRF(208F_CTRL_GR, _ECC_INJECT_ERROR, _UNIT, _L1TAG, *errorLoc);
            break;

        case CBU_PAR:
            *errorLoc = FLD_SET_DRF(208F_CTRL_GR, _ECC_INJECT_ERROR, _UNIT, _CBU, *errorLoc);
            break;

        default: //impossible
            return;
    }

    switch(*errorType) {
        case SBE:
            *errorType = FLD_SET_DRF(208F_CTRL_GR, _ECC_INJECT_ERROR, _TYPE, _SBE, *errorType);
            break;

        case DBE:
            *errorType = FLD_SET_DRF(208F_CTRL_GR, _ECC_INJECT_ERROR, _TYPE, _DBE, *errorType);
            break;

        default: //impossible
            return;
    }
}

//! \brief injects either a SBE or a DBE onto a given error location
//!
//! \return RC struct denoting the exit status of InjectError()
//------------------------------------------------------------------------------
RC EccGrErrorInjectionTest::InjectError(LwU32 errorLoc, LwU32 errorType)
{
    RC rc;

    // translate the values into ones for the ctrl call
    SetErrorParams(&errorLoc, &errorType);

    // injecting at just GPC0 TPC0. This can be enhanced later if needed
    LW208F_CTRL_GR_ECC_INJECT_ERROR_PARAMS params =
    {
        0,
        0,
        static_cast<LwU8>(errorLoc),
        static_cast<LwU8>(errorType)
    };
    CHECK_RC(LwRmControl(m_hClient, m_hSubDevDiag,
                        LW208F_CTRL_CMD_GR_ECC_INJECT_ERROR,
                        (void*)&params, sizeof(params)));
    return rc;
}

//! \brief Callback for ECC errors
//!
//! Here we check if the appropriate ECC interrupt bit is set.
//!
//! \return nothing
//------------------------------------------------------------------------------
static void EccCallback(void *parg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{
    EccCallbackParams* params = (EccCallbackParams*)parg;
    LwU32 offset = params->eccOffsetParams->offset;
    LwU32 mask = params->eccOffsetParams->mask;
    LwU32 val = 0;

    params->eccEventOclwrred = true;
    val = MEM_RD32(((LwU8*)m_InterruptLoc) + offset) & mask;
    if (val)
        params->eccIsInterruptSet = true;
    else
        params->eccIsInterruptSet = false;
    return;
}

//! \brief prints the error counts for FB and LTC
//!
//! \return nothing
//------------------------------------------------------------------------------
static void PrintEccErrorCount(EccErrCounter* pErrCounter)
{
    Printf(Tee::PriNormal, "LRF: CORR = %u UNCORR = %u\n",
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::SM_CORR),
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::SM_UNCORR));
    Printf(Tee::PriNormal, "SHM: CORR = %u UNCORR = %u\n",
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::SHM_CORR),
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::SHM_UNCORR));
    Printf(Tee::PriNormal, "L1Data: CORR = %u UNCORR = %u\n",
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::L1DATA_CORR),
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::L1DATA_UNCORR));
    Printf(Tee::PriNormal, "L1Tag: CORR = %u UNCORR = %u\n",
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::L1TAG_CORR),
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::L1TAG_UNCORR));
    Printf(Tee::PriNormal, "CBU: CORR = %u UNCORR = %u\n",
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::CBU_CORR),
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::CBU_UNCORR));
}

//------------------------------------------------------------------------------
RC EccGrErrorInjectionTest::Cleanup()
{
    if(pGrAlloc) {
        pGrAlloc->Free();
    }

    if(m_pCh) {
        m_TestConfig.FreeChannel(m_pCh);
    }

    if(m_eccSbeEvent) {
        pLwRm->Free(m_eccSbeEvent);
    }

    if(m_eccDbeEvent) {
        pLwRm->Free(m_eccDbeEvent);
    }

    if(m_sbdvHandle) {
        pLwRm->Free(m_sbdvHandle);
    }

    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(EccGrErrorInjectionTest, RmTest, "ECC graphics injection test for LRF, L1Data/tag, CBU and SHM.");
