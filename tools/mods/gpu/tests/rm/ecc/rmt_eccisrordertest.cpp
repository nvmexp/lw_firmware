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
//! \file rmt_EccIsrOrderTest.cpp
//! \brief Checks that ECC interrupts from bit errors are serviced as follows:
//!         1. appropriate notifier is set
//!         2. appropriate checkbit is cleared
//!
//! TODO: this test only supports FB and LTC. When SM3.0 is supported, will add
//!       support for L1C and SM RF.

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl90b5.h"       // GF100_DMA_COPY
#include "class/cl90b5sw.h"
#include "class/cla0b5.h"       // KEPLER_DMA_COPY_A
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_COPY_ENGINE
#include "class/cl0000.h"       // LW01_NULL_OBJECT
#include "ctrl/ctrl208f.h"
#include "class/cl90e6.h" // GF100_SUBDEVICE_MASTER
#include "ctrl/ctrl90e6.h"
#include "core/utility/errloggr.h"
#include "gpu/utility/surf2d.h"
#include <vector>
#include "gpu/tests/rmtest.h"
#include "gpu/utility/ecccount.h"
#include "core/include/platform.h"
#include "sim/IChip.h"
#include "rmt_eccisrordertest.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/include/gpudev.h"
#include "gpu/utility/bglogger.h"
#include "gpu/utility/modscnsl.h"
#include "class/cl0005.h"
#include "kepler/gk110/dev_fbpa.h" // FB status library
#include "kepler/gk110/dev_graphics_nobundle.h" // L1C & SM status library
#include "kepler/gk110/dev_ltc.h" // L2C status library
#include "gpu/utility/chanwmgr.h"
#include "gpu/tests/lwca/ecc/eccdefs.h"
#include "kepler/gk110/hwproject.h"
#include "kepler/gk110/dev_top.h"
#include "core/include/utility.h"
#include "core/include/memcheck.h"

typedef struct
{
    LwU32 eccEventType; //input parameter
    LwU32 errLocation; //input parameter
    bool eccEventOclwrred; //return value set by EccCallback
    bool eccIsInterruptSet; //return value set by EccCallback
    LW90E6_CTRL_MASTER_GET_ECC_INTR_OFFSET_MASK_PARAMS *eccOffsetParams;
} EccCallbackParams;

GpuSubdevice* masterGpu;
static EccErrCounter *pEccErrCounter;
static FLOAT64 intrTimeout;
void * m_InterruptLoc;

static EccCallbackParams eccCallbackParams;

static void EccCallback(void *parg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status);
static void PrintEccErrorCount(EccErrCounter* pErrCounter);

class EccIsrOrderTest : public RmTest
{
public:
    EccIsrOrderTest();
    virtual ~EccIsrOrderTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    // ECC parameters
    LwRm::Handle m_eccSbeEvent;
    LwRm::Handle m_eccDbeEvent;
    LwRm::Handle m_sbdvHandle;
    LWOS10_EVENT_KERNEL_CALLBACK_EX eccSbeCallback;
    LWOS10_EVENT_KERNEL_CALLBACK_EX eccDbeCallback;
    LW90E6_CTRL_MASTER_GET_ECC_INTR_OFFSET_MASK_PARAMS eccOffsetParams;
    UINT32 m_SavedFbBackdoor;     //!< To restore BACKDOOR_ACCESS in cleanup
    UINT32 m_SavedHostFbBackdoor; //!< To restore BACKDOOR_ACCESS in cleanup
    bool m_RestoreBackdoorInCleanup; //!< m_Saved*Backdoor has valid data
    LwU32 numLocations;

    // error injection surface for testing ISR order on FB and LTC
    Surface2D* pErrSurface;

    RC AllocCeCompliantMem(LwU32 size, Memory::Location loc, Surface2D** allocationPointer);
    RC FlushTo(LwU32 targetLoc);
    RC ToggleWriteKill(LwU32 loc, LwU64 addr, LwU32 pteKind, LwU32 pageSizeKB, bool val);
    RC InjectError(LwU32 errorType, LwU32 errorLoc, LwU32 offset);
};

//------------------------------------------------------------------------------
EccIsrOrderTest::EccIsrOrderTest() :
    m_sbdvHandle(0),
    m_SavedFbBackdoor(0),
    m_SavedHostFbBackdoor(0),
    m_RestoreBackdoorInCleanup(false)
{
    m_eccSbeEvent = 0;
    m_eccDbeEvent = 0;
    memset(&eccSbeCallback, 0, sizeof(eccSbeCallback));
    memset(&eccDbeCallback, 0, sizeof(eccDbeCallback));
    memset(&eccOffsetParams, 0, sizeof(eccOffsetParams));

    pErrSurface = 0;

    masterGpu = 0;
    pEccErrCounter = 0;
    intrTimeout = 0;
    numLocations = 0;
    memset(&eccCallbackParams, 0, sizeof(eccCallbackParams));

    SetName("EccIsrOrderTest");
}

//------------------------------------------------------------------------------
EccIsrOrderTest::~EccIsrOrderTest()
{
}

//! \brief IsTestSupported() checks if the the master GPU is correctly initialized
//!
//! EccIsrOrderTest needs: ECC enabled and EccErrCounter initialized
//!
//! \return returns RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         else returns "NOT SUPPORTED"
//------------------------------------------------------------------------------
string EccIsrOrderTest::IsTestSupported()
{
    LwRm *pLwRm = GetBoundRmClient();
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    LW2080_CTRL_GPU_QUERY_ECC_STATUS_PARAMS EccStatus;

    // step 1: check if ECC is enabled
    memset(&EccStatus, 0, sizeof(EccStatus));
    if (OK != pLwRm->ControlBySubdevice(pGpuSubdevice,
                                        LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS,
                                        &EccStatus, sizeof(EccStatus)))
    {
        return "ECC not Enabled";
    }

    if (EccStatus.units[LW2080_CTRL_GPU_ECC_UNIT_FBPA].enabled)
    {
        numLocations++;
    }

    if (EccStatus.units[LW2080_CTRL_GPU_ECC_UNIT_L2].enabled)
    {
        numLocations++;
    }

    // step 2: verify that EccErrCounter is ready
    if (GetBoundGpuSubdevice()->GetEccErrCounter()->IsInitialized())
        return RUN_RMTEST_TRUE;

    return "NOT SUPPORTED";
}

//! \brief Makes all the necessary allocations for EccIsrOrderTest
//!
//! Allocates surface to which we inject errors, registers callback, registers
//! for SBE and DBE notifiers
//!
//! \return RC structure to denote the error status of Setup()
//------------------------------------------------------------------------------
RC EccIsrOrderTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;
    int temp = 0;
    LW0005_ALLOC_PARAMETERS allocSbeParams = {0};
    LW0005_ALLOC_PARAMETERS allocDbeParams = {0};
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventSbeParams = {0};
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventDbeParams = {0};

    // step 1: get the basic parameters of this test
    CHECK_RC(InitFromJs());
    intrTimeout = m_TestConfig.TimeoutMs();
    masterGpu = GetBoundGpuSubdevice();

    // step 3: allocate necessary surfaces
    CHECK_RC_CLEANUP(AllocCeCompliantMem(BLOCK_SIZE, Memory::Fb, &pErrSurface));

    //
    //step 4: setup callbacks for ECC SBE & DBE
    //step 4a: register EccCallback() for SBE interrupts
    //
    eccSbeCallback.func = EccCallback;
    eccSbeCallback.arg = &eccCallbackParams;

    allocSbeParams.hParentClient = pLwRm->GetClientHandle();
    allocSbeParams.hClass = LW01_EVENT_KERNEL_CALLBACK_EX;
    allocSbeParams.notifyIndex = LW2080_NOTIFIERS_ECC_SBE;
    allocSbeParams.data = LW_PTR_TO_LwP64(&eccSbeCallback);

    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
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

    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                          &m_eccDbeEvent,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocDbeParams));

    //step 4c: register to listen to ECC SBE notifier
    eventSbeParams.event = LW2080_NOTIFIERS_ECC_SBE;
    eventSbeParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &eventSbeParams, sizeof(eventSbeParams)));

    //step 4d: register to listen to ECC DBE notifier
    eventDbeParams.event = LW2080_NOTIFIERS_ECC_DBE;
    eventDbeParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &eventDbeParams, sizeof(eventDbeParams)));

    //step 5: prep EccErrCounter
    CHECK_RC(ErrorLogger::StartingTest());
    pEccErrCounter = masterGpu->GetEccErrCounter();

    //
    //step6: Allocate subdevice master to map the top level interrupt
    // They will be mapped to m_InterruptLoc to be read from an offset and
    // mask that is obtained from  the LW90E6_CTRL_CMD_MASTER_GET_ECC_INTR_OFFSET_MASK
    // control call
    //
    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                &m_sbdvHandle,
                GF100_SUBDEVICE_MASTER,
                (void *)&temp));

    CHECK_RC(pLwRm->MapMemory(m_sbdvHandle, 0, sizeof(GF100MASTERMap),
                              &m_InterruptLoc, DRF_DEF(OS33, _FLAGS, _ACCESS, _READ_ONLY),
                              GetBoundGpuSubdevice() ));

    CHECK_RC(pLwRm->Control(m_sbdvHandle, LW90E6_CTRL_CMD_MASTER_GET_ECC_INTR_OFFSET_MASK,
                      &eccOffsetParams, sizeof(eccOffsetParams)));

    eccCallbackParams.eccOffsetParams = &eccOffsetParams;

    //
    // Disable simulation backdoors by writing 0 to the 32-bit fields (4 bytes).
    // Note that if simulation backdoors are enabled, access to FB bypass L2C.
    // This cirlwmvents FB ECC since FB ECC logic is in the interface between FB
    // and L2C.
    //
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        Platform::EscapeRead("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4,
                             &m_SavedFbBackdoor);
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
        Platform::EscapeRead("BACKDOOR_ACCESS", IChip::EBACKDOOR_HOSTFB, 4,
                             &m_SavedHostFbBackdoor);
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_HOSTFB, 4, 0);
        m_RestoreBackdoorInCleanup = true;
    }

Cleanup:
    return rc;
}

//! \brief Runs the EccIsrOrderTest
//!
//! Tests if RM upholds the ISR convention of first setting the notifier and then
//! clearing interrupt bits. Triggers ECC SBE and DBE on FB only.
//!
//! \return RC structure to denote the error status of Run()
//------------------------------------------------------------------------------
RC EccIsrOrderTest::Run()
{
    RC rc;
    LwU32 buffer = 0;
    LwU32 i = 0, j = 0;
    SurfaceUtils::MappingSurfaceReader reader(*pErrSurface, masterGpu->GetSubdeviceInst());

    // save off the state of ECC for restoration when we are done
    unique_ptr<FrameBuffer::CheckbitsHolder> checkbitsHolder(
            masterGpu->GetFB()->SaveFbEccCheckbits());

    //step 1: setup EccErrCounter
    BgLogger::PauseBgLogger DisableBgLogger;
    //Tasker::Sleep(intrTimeout);
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::FB_CORR));
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::FB_UNCORR));
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::L2_CORR));
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::L2_UNCORR));

    for (i = 0; i < NUM_EVENT_TYPES; i++)
    {
        for (j = 0; j < numLocations; j++)
        {
            // step 2: sanitize surface on FB and L2C and initialize event parameters
            pErrSurface->Fill(0xaa);
            eccCallbackParams.eccIsInterruptSet = false;
            eccCallbackParams.eccEventOclwrred = false;

            //
            // step 3: choose where (FB or LTC) and what kind of error to inject
            // (SBE or DBE)
            //
            eccCallbackParams.eccEventType = i;
            eccCallbackParams.errLocation = j;

            // step 4: inject the error at offset 0
            InjectError(eccCallbackParams.eccEventType, eccCallbackParams.errLocation, 0);

            //
            // step 5: test the ISR order by triggering the error and use the callback
            // to check if the appropriate interrupt bit is still set. We only need
            // to sleep for an instant to let the interrupts be serviced, but we
            // sleep for the amount of time needed to not be considered an interrupt
            // storm.
            //
            reader.ReadBytes(0, &buffer, sizeof(LwU32));
            Tasker::Sleep(intrTimeout);

            if (!eccCallbackParams.eccEventOclwrred)
            {
                Printf(Tee::PriNormal,
                       "injected error location: 0x%X injected error type: 0x%X\n",
                       (unsigned int) eccCallbackParams.errLocation,
                       (unsigned int) eccCallbackParams.eccEventType);
                return RC::LWRM_IRQ_NOT_FIRING;
            }
            else if (!eccCallbackParams.eccIsInterruptSet)
            {
                Printf(Tee::PriNormal,
                       "injected error location: 0x%X injected error type: 0x%X\n",
                       (unsigned int) eccCallbackParams.errLocation,
                       (unsigned int) eccCallbackParams.eccEventType);
                return RC::ILWALID_IRQ;
            }

            CHECK_RC(pEccErrCounter->Update());
            PrintEccErrorCount(pEccErrCounter);
        }
    }

    // step 6: cleanup
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::FB_CORR));
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::FB_UNCORR));
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::L2_CORR));
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::L2_UNCORR));

    return rc;
}

//------------------------------------------------------------------------------
RC EccIsrOrderTest::Cleanup()
{
    RC rc;

    if (pErrSurface)
    {
        pErrSurface->Free();
        delete pErrSurface;
    }

    return rc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief allocation of a 1D block of memory via Surface2D
//!
//! Use this function for CE compliant memory allocations. Pointer to block
//! is stored in allocationPointer.
//!
//! NOTE: initializes the block to 0
//!
//! \return exit status of AllocCeCompliantMem()
//------------------------------------------------------------------------------
RC EccIsrOrderTest::AllocCeCompliantMem(LwU32 size, Memory::Location loc,
                                            Surface2D** allocationPointer)
{
    RC rc;

    Surface2D* temp = new Surface2D;
    temp->SetForceSizeAlloc(true);
    temp->SetPhysContig(1);
    temp->SetLayout(Surface2D::Pitch);
    temp->SetLocation(loc);
    temp->SetColorFormat(ColorUtils::Y8); //each "pixel" is 8 bits
    temp->SetArrayPitch(size);

    CHECK_RC_CLEANUP(temp->Alloc(GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(temp->Map());
    Platform::FlushCpuWriteCombineBuffer();

    *allocationPointer = temp;
    return rc;

Cleanup:
    delete temp;
    *allocationPointer = 0;
    return rc;
}

//! \brief injects either a SBE or a DBE onto a given surface
//!
//! NOTE: surface must be on FB and not sysmem
//! Warning: SM and L1C options lwrrently not implemented
//!
//! \return RC struct denoting the exit status of InjectError()
//------------------------------------------------------------------------------
RC EccIsrOrderTest::InjectError(LwU32 errorType, LwU32 errorLoc, LwU32 offset)
{
    SurfaceUtils::MappingSurfaceReader reader(*pErrSurface, masterGpu->GetSubdeviceInst());
    SurfaceUtils::MappingSurfaceWriter writer(*pErrSurface, masterGpu->GetSubdeviceInst());
    const LwU32 pteKind  = pErrSurface->GetPteKind();
    LwU32       pagesize = 0;
    LwU32       buffer;
    LwU32       readBack = 0;

    // step 1: get what data was stored on the surface
    FlushTo(errorLoc);
    reader.ReadBytes(0, &buffer, sizeof(LwU32));

    // step 2: toggle the bits as necessary
    switch (errorType)
    {
        case SBE:
            buffer = SBE_DATA ^ buffer;
            break;

        case DBE:
            buffer = DBE_DATA ^ buffer;
            break;

        default:
            return RC::BAD_PARAMETER;
    }

    // step 2: write the error
    switch (errorLoc)
    {
        case L2_CACHE:
            // pagesize is uninitialized and not used for L2C
            ToggleWriteKill(errorLoc, offset, pteKind, pagesize, true);
            writer.WriteBytes(offset, &buffer, sizeof(LwU32));
            reader.ReadBytes(0, &readBack, sizeof(LwU32));
            FlushTo(errorLoc);
            ToggleWriteKill(errorLoc, offset, pteKind, pagesize, false);
            if (readBack == buffer)
                return OK;

        case FB:
            // grab page size for decoding the fb address
            pagesize = pErrSurface->GetPageSize();
            if (pagesize == 0)
                pagesize = pErrSurface->GetActualPageSizeKB();

            ToggleWriteKill(errorLoc, offset, pteKind, pagesize, true);
            writer.WriteBytes(offset, &buffer, sizeof(LwU32));
            reader.ReadBytes(0, &readBack, sizeof(LwU32));
            FlushTo(errorLoc);
            ToggleWriteKill(errorLoc, offset, pteKind, pagesize, false);
            if (readBack == buffer)
                return OK;

        default:
            return RC::BAD_PARAMETER;
    }
}

//! \brief performs the necessary operations to flush to a given target
//!
//! NOTE: targetLoc should only be L2_CACHE or FB
//! Warning: SM option lwrrently not implemented
//!
//! \return RC struct denoting the exit status of FlushTo()
//------------------------------------------------------------------------------
RC EccIsrOrderTest::FlushTo(LwU32 targetLoc)
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
            Params.targetUnit =  DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                          _FB, _ENABLE);
            CHECK_RC(GetBoundRmClient()->ControlByDevice(GetBoundGpuDevice(),
                                                         LW0080_CTRL_CMD_DMA_FLUSH,
                                                         &Params, sizeof(Params)));

            Params.targetUnit = 0;
            Params.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                        _L2, _ENABLE);
            CHECK_RC(GetBoundRmClient()->ControlByDevice(GetBoundGpuDevice(),
                                                         LW0080_CTRL_CMD_DMA_FLUSH,
                                                         &Params, sizeof(Params)));
            Utility::SleepUS(1000000);
            CHECK_RC(masterGpu->IlwalidateL2Cache(0));
            Utility::SleepUS(1000000);
            return rc;

        default:
            return RC::BAD_PARAMETER;
    }

    return RC::SOFTWARE_ERROR;
}

//! \brief performs the necessary operations to toggle write kill at a given target
//!
//! NOTE: loc can be either L2_CACHE or FB
//!
//! \return RC struct denoting the exit status of ToggleWriteKilL()
//------------------------------------------------------------------------------
RC EccIsrOrderTest::ToggleWriteKill(LwU32 loc, LwU64 addr, LwU32 pteKind,
                                    LwU32 pageSizeKB, bool val)
{
    RC rc;

    //
    // step 1: ilwert val so it now represents if we want to turn on or off ECC checkbits
    // as opposed to turning on or off writekill
    //
    val = !val;

    // step 2: turn off ECC checkbits in the appropriate location
    switch (loc)
    {
        case L2_CACHE:
            masterGpu->SetL2EccCheckbitsEnabled(val);
            break;

        case FB:
            if (val)
                CHECK_RC_CLEANUP(masterGpu->GetFB()->EnableFbEccCheckbits());
            else
                CHECK_RC_CLEANUP(masterGpu->GetFB()->DisableFbEccCheckbitsAtAddress(addr, pteKind, pageSizeKB));
            break;

        default:
            /* SM RF not supported yet */
            return RC::BAD_PARAMETER;
    }

    return OK;

Cleanup:
    return RC::BAD_PARAMETER;
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
    LwU32 val = 0;

    params->eccEventOclwrred = true;
    val = MEM_RD32(((LwU8*)m_InterruptLoc) + params->eccOffsetParams->offset) & params->eccOffsetParams->mask;
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
    Printf(Tee::PriNormal, "FB: SBE = %u DBE = %u\n",
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::FB_CORR),
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::FB_UNCORR));
    Printf(Tee::PriNormal, "L2: CORR = %u UNCORR = %u\n",
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::L2_CORR),
           (unsigned int) pErrCounter->GetTotalErrors(EccErrCounter::L2_UNCORR));
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(EccIsrOrderTest, RmTest, "ECC ISR order test for FB and LTC.");
