/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "gpu/utility/chanwmgr.h"
#include "gpu/tests/lwdastst.h"
#include "gpu/utility/ecccount.h"
#include "gpu/tests/lwca/ecc/eccdefs.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/jsonlog.h"
#include "core/include/platform.h"
#include "sim/IChip.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/xp.h"
#include "gpu/utility/bglogger.h"

// TODO(stewarts): Old test. If this test is to be revived, all references of
// SBE/DBE from RM need to be updated to corr/uncorr.

//--------------------------------------------------------------------
//! \brief ECC SM-SHM Test
//!
//! Test the ECC feature in the SM Shared Memory.
//! The ECC can correct SBEs (Single-Bit Errors) and detect DBEs
//! (Double-Bit Errors).
//!
class EccSMShmTest : public LwdaStreamTest
{
public:
    EccSMShmTest();
    virtual ~ EccSMShmTest(){};
    RC InjectAndCheckErrors();
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(PrintCounts, bool);

private:
    bool   m_PrintCounts;
    Random *m_pRandom;            //!< Random-number generator
    UINT32 m_NumSms;
    UINT32 m_WarpSize;
    StickyRC m_ErrorRc;
    UINT32 m_Loops;
    GpuSubdevice *m_pSubdev;

    UINT32 m_SavedFbBackdoor;     //!< To restore BACKDOOR_ACCESS in cleanup
    UINT32 m_SavedHostFbBackdoor; //!< To restore BACKDOOR_ACCESS in cleanup
    bool m_RestoreBackdoorInCleanup;
    Lwca::Module m_LwdaModule;
    Lwca::Function m_LwdaFunction;
    Lwca::Event m_LwdaFunctionDoneEvent;
    Lwca::Stream m_LwdaMemcpyStream;
    Lwca::HostMemory m_LwdaMemcpyBuffer;
    Lwca::ClientMemory m_CmdMem;
    Surface2D m_CmdSurf;
    UINT32 m_CmdSurfSize;
    RC GetErrCounts(UINT64 *pSbeCount, UINT64 *pDbeCount,
                    UINT64 MinSbeCount, UINT64 MinDbeCount);
    RC WriteGlobal(Lwca::Global Dst, const void *pSrc, UINT32 numWords);
};

//--------------------------------------------------------------------
EccSMShmTest::EccSMShmTest()
    : m_PrintCounts(false)
    , m_pRandom(&GetFpContext()->Rand)
    , m_NumSms(0)
    , m_WarpSize(0)
    , m_Loops(1)
    , m_pSubdev(nullptr)
    , m_SavedFbBackdoor(0)
    , m_SavedHostFbBackdoor(0)
    , m_RestoreBackdoorInCleanup(false)
    , m_CmdSurfSize(0)
{
    // Disable the RC watchdog on the subdevice for the duration of the test
    SetDisableWatchdog(true);
}

//--------------------------------------------------------------------
bool EccSMShmTest::IsSupported()
{
    if (!LwdaStreamTest::IsSupported())
        return false;

    LwRm *pLwRm = GetBoundRmClient();
    m_pSubdev = GetBoundGpuSubdevice();
    Lwca::Device LwdaDevice = GetBoundLwdaDevice();
    Lwca::Capability cap = LwdaDevice.GetCapability();
    LW2080_CTRL_GPU_QUERY_ECC_STATUS_PARAMS EccStatus;

    memset(&EccStatus, 0, sizeof(EccStatus));
    if (OK != pLwRm->ControlBySubdevice(m_pSubdev,
                                        LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS,
                                        &EccStatus, sizeof(EccStatus)))
    {
        return false;
    }

    if (EccStatus.units[LW2080_CTRL_GPU_ECC_UNIT_SHM].enabled &&
            m_pSubdev->GetEccErrCounter()->IsInitialized() &&
            cap >= Lwca::Capability::SM_60)
    {
        // make sure we have a special VBIOS that can toggle ECC enable state
        bool bEccEnable   = false;
        bool bEccOverride = false;

        // Save ECC checkbits state and restore on exit
        if (OK != m_pSubdev->GetSHMEccEnable(&bEccEnable, &bEccOverride))
        {
            return false;
        }
        const bool bEccOrigEnable = bEccEnable;
        DEFER
        {
            m_pSubdev->SetSHMEccEnable(bEccOrigEnable, bEccOverride);
        };

        // Flip ECC checkbits state
        bEccEnable = !bEccEnable;
        if (OK != m_pSubdev->SetSHMEccEnable(bEccEnable, true))
        {
            return false;
        }

        // Confirm checkbits were changed
        bool bLwrEccEnable = false;
        if (OK != m_pSubdev->GetSHMEccEnable(&bLwrEccEnable, nullptr))
        {
            return false;
        }
        if (bLwrEccEnable != bEccEnable)
        {
            Printf(Tee::PriLow, "Can't toggle SHM ECC state. Check your VBIOS!\n");
            return false;
        }
        else
        {
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------
/* virtual */ RC EccSMShmTest::Setup()
{
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    RC rc;

    CHECK_RC(LwdaStreamTest::Setup());
    m_pRandom->SeedRandom(pTestConfig->Seed());
    m_NumSms = GetBoundLwdaDevice().GetShaderCount();

    // Allocate the lwca objects
    //
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("eccshm", &m_LwdaModule));

    m_WarpSize = GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_WARP_SIZE);

    m_LwdaFunction = m_LwdaModule.GetFunction("InjectErrors", m_NumSms, m_WarpSize);
    m_LwdaFunctionDoneEvent = GetLwdaInstance().CreateEvent();
    CHECK_RC(m_LwdaFunctionDoneEvent.InitCheck());
    m_LwdaMemcpyStream = GetLwdaInstance().CreateStream();
    CHECK_RC(m_LwdaMemcpyStream.InitCheck());
    CHECK_RC(GetLwdaInstance().AllocHostMem(1024, &m_LwdaMemcpyBuffer));

    /* Each thread sends 1 word for command and 1 word for argument */
    m_CmdSurfSize = m_NumSms * sizeof(UINT32) * 2;

    m_CmdSurf.SetWidth(m_CmdSurfSize);
    m_CmdSurf.SetHeight(1);
    m_CmdSurf.SetColorFormat(ColorUtils::I8);
    m_CmdSurf.SetLocation(Memory::Coherent);
    CHECK_RC(m_CmdSurf.Alloc(pGpuDevice));
    m_CmdMem = GetLwdaInstance().MapClientMem(m_CmdSurf.GetMemHandle(), 0,
                                                m_CmdSurfSize, pGpuDevice,
                                                m_CmdSurf.GetLocation());
    CHECK_RC(m_CmdMem.InitCheck());
    // Disable backdoor accesses
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

    m_ErrorRc.Clear();
    return rc;
}

//--------------------------------------------------------------------
/* virtual */ RC EccSMShmTest::Run()
{
    RC rc;
    m_pSubdev = GetBoundGpuSubdevice();
    m_Loops = GetTestConfiguration()->Loops();
    EccErrCounter *pEccErrCounter = m_pSubdev->GetEccErrCounter();
    BgLogger::PauseBgLogger DisableBgLogger;
    LwRm::DisableControlTrace DisableControlTrace;
    LwRm::DisableRcRecovery DisableRcRecovery(m_pSubdev);
    // Tell the EccErrCounter that this test is about to start
    // injecting errors.
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::SHM_CORR));
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::SHM_UNCORR));
    Utility::CleanupOnReturn<EccErrCounter, RC>
        ExpectingErrors(pEccErrCounter, &EccErrCounter::EndExpectingAllErrors);

    CHECK_RC(InjectAndCheckErrors());

    // Tell the EccErrCounter that this test is done injecting errors.
    ExpectingErrors.Cancel();
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::SHM_CORR));
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::SHM_UNCORR));

    return m_ErrorRc;
}

//--------------------------------------------------------------------
/* virtual */ RC EccSMShmTest::Cleanup()
{

    if (m_RestoreBackdoorInCleanup)
    {
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4,
                              m_SavedFbBackdoor);
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_HOSTFB, 4,
                              m_SavedHostFbBackdoor);
        m_RestoreBackdoorInCleanup = false;
    }

    m_CmdMem.Free();
    m_CmdSurf.Free();
    m_LwdaMemcpyBuffer.Free();
    m_LwdaMemcpyStream.Free();
    m_LwdaFunctionDoneEvent.Free();
    m_LwdaModule.Unload();
    return LwdaStreamTest::Cleanup();
}

//------------------------------------------------------------------------------
//! \brief Inject and check Ecc errors in SM-SHM
//!
//! This function starts a LWCA kernel. Kernel running on device interacts
//! with mods running on host by using "HostCommand" object.
//! Code flow:
//! 1. Kernel asks mods to store the thread number. (each thread)
//! 2. Kernel thread asks mods to store loop number.
//! 3. Kernel thread asks mods to store the type of error and values that
//!    are to be written.
//! 4. Kernel thread asks mods to disable ECC.
//! 5. Kernel thread then modifies the stored data. (st.shared.u32 offset, badvalue)
//! 6. Kernel thread asks mods to enable ECC.
//! 7. Kernel reads stored data again as good value.
//!    This load operation triggers ECC interrupts. (ld.shared.u32 offset)
//! 8. Kernel thread asks mods to check the bad value and good value as
//!    as per the type of error.
//! 9. Kernel thread asks mods to check the number of errors. Errors injected
//!    should match the ecc error counter value (SHM_CORR and SHM_UNCORR).
//! 10. Kernel threads finish and notify mods by writing GPU_DONE in the HostCommand
RC EccSMShmTest :: InjectAndCheckErrors()
{
    RC rc;
    bool bEccEnable   = false;
    bool bEccOverride = false;
    CHECK_RC(m_pSubdev->GetSHMEccEnable(&bEccEnable, &bEccOverride));
    CHECK_RC(m_pSubdev->SetSHMEccEnable(true, true));

    //Make sure we restore the EccState on exit.
    DEFER
    {
        m_pSubdev->SetSHMEccEnable(bEccEnable, bEccOverride);
    };

    vector<UINT32> buffer(m_NumSms * 2, 0x00);
    for(UINT32 ii = 0; ii < buffer.size(); ii += 2)
    {
        buffer[ii] = HOST_DONE;
    }
    SurfaceUtils::WriteSurface(m_CmdSurf, 0, &buffer[0], m_CmdSurfSize, 0);
    UINT64 devPtr = m_CmdMem.GetDevicePtr();
    CHECK_RC(WriteGlobal(m_LwdaModule["HostCommand"], &devPtr, 2));

    // Seed that will generate the sequence of random numbers
    UINT32 seed = m_pRandom->GetRandom();
    CHECK_RC(WriteGlobal(m_LwdaModule["Seed"], &seed, 1));

    // Store the warpsize in the global variable, for kernel to use
    CHECK_RC(WriteGlobal(m_LwdaModule["WarpSize"], &m_WarpSize, 1));

    // Store the number of thread loops(as passed by JS) per thread
    CHECK_RC(WriteGlobal(m_LwdaModule["NumThreadLoops"], &m_Loops, 1));

    // Start the kernel
    CHECK_RC(m_LwdaFunction.Launch());
    CHECK_RC(m_LwdaFunctionDoneEvent.Record());

    // Record the number of SBE and DBE errors before injecting the
    // error.
    UINT64 oldSbeCount = 0;
    UINT64 oldDbeCount = 0;

    UINT64 newSbeCount = 0;
    UINT64 newDbeCount = 0;

    UINT32 storeVal = 0;
    UINT32 checkVal = 0;

    UINT32 lwrrentTid = 0;
    UINT32 lwrrentLoop = 0;

    const UINT32 sbesPerInjection = m_NumSms;
    const UINT32 dbesPerInjection = m_NumSms;
    const UINT32 maxSbesPerInjection = m_NumSms * 2;
    const UINT32 maxDbesPerInjection = m_NumSms * 2;

    bool SbeError = false;
    LwRm::ExclusiveLockRm lockRm(GetBoundRmClient());
    Tee::Priority pri = GetVerbosePrintPri();
    Tee::Priority priPrintCounts = (m_PrintCounts ? Tee::PriNormal : pri);

    // This is the core loop of test where we communicate with the InjectError
    // kernel. This code is identical to the code used in other lwca/ecc tests.
    while (1)
    {
        UINT32 command;
        UINT32 arg;
        bool   NewCommand = true;

        CHECK_RC(SurfaceUtils::ReadSurface(m_CmdSurf, 0, &buffer[0], m_CmdSurfSize, 0));

        for (UINT32 ii = 0; ii < m_NumSms * 2; ii += 2)
        {
            //Ensure threads on all Sms are at same command state
            if (buffer[ii] == HOST_DONE)
            {
                NewCommand = false;
                break;
            }
            else if (buffer[ii] != buffer[0])
            {
                MASSERT(!"Kernels have diverged in SMs");
                m_ErrorRc = RC::SOFTWARE_ERROR;
                NewCommand = true;
                break;
            }
        }

        if (!NewCommand)
        {
            continue;
        }

        // Don't yield when SM Ecc is disabled. When SM Ecc is disabled we inject
        // errors
        if (bEccEnable)
            Tasker::Yield();

        command = buffer[0];
        arg = buffer[1];
        switch (command)
        {
            case HOST_STORE_THREAD_ID:
                lwrrentTid = arg;
                Printf(pri, "Current Tid: %d\n", arg);
                break;

            case HOST_STORE_THREAD_LOOP:
                lwrrentLoop = arg;
                CHECK_RC(GetErrCounts(&oldSbeCount, &oldDbeCount, 0, 0));
                Printf(pri, "Loop Num[%d], oldSbeCount [%lld], oldDbeCount[%lld]\n"
                    , arg, oldSbeCount, oldDbeCount);
                break;

            case HOST_PRINT:
                //For debugging print the arg
                Printf(pri, "Arg: %d\n", arg);
                break;

            case HOST_STORE_ERR_TYPE:
                SbeError = arg ? false : true;
                Printf(pri, "Type of error %s\n", (SbeError)?"SBE":"DBE");
                break;

            case HOST_STORE_VAL:
                storeVal = arg;
                Printf(pri, "Store val = 0x%08x\n", arg);
                break;

            case HOST_DISABLE_CHECKBITS:
                Printf(pri, "Disabling checkbits\n");
                bEccEnable = false;
                CHECK_RC(lockRm.AcquireLock());
                CHECK_RC(m_pSubdev->SetSHMEccEnable(bEccEnable,true));
                break;

            case HOST_ENABLE_CHECKBITS:
                Printf(pri, "Enabling checkbits\n");
                bEccEnable = true;
                CHECK_RC(m_pSubdev->SetSHMEccEnable(bEccEnable,true));
                lockRm.ReleaseLock();
                break;

            case HOST_CHECK_VAL:
                Printf(pri, "Check 0x%08x\n", arg);
                checkVal = arg;

                if (SbeError || (storeVal == 0))
                {
                    if (checkVal != storeVal)
                    {
                        Printf(Tee::PriError, "[Thread: %d, Loop: %d]SBE inserted,"
                                             " but the read value(0x%08x) is still"
                                             " NOT same as written value(0x%08x).\n",
                                             lwrrentTid, lwrrentLoop,
                                             checkVal, storeVal);
                        m_ErrorRc = RC::BAD_MEMORY;
                    }
                }
                else
                {
                    if (checkVal == storeVal)
                    {
                        Printf(Tee::PriError, "[Thread: %d, Loop: %d]DBE inserted,"
                                             " but the read value(0x%08x) is still"
                                             " SAME as written value(0x%08x).",
                                             lwrrentTid, lwrrentLoop,
                                             checkVal, storeVal);
                        m_ErrorRc = RC::BAD_MEMORY;
                    }
                }
                break;

            case HOST_CHECK_COUNTS:
                Printf(pri, "Checking error counts\n");
                if (SbeError)
                {
                    CHECK_RC(GetErrCounts(&newSbeCount, &newDbeCount,
                                      oldSbeCount + sbesPerInjection,
                                      oldDbeCount));
                    if ((newSbeCount < (oldSbeCount + sbesPerInjection)) ||
                        (newSbeCount > (oldSbeCount + maxSbesPerInjection)) )
                    {
                        Printf(Tee::PriError, "[Thread: %d, Loop: %d]Bad SHM SBE error count:"
                                             " count = %lld, expected = %lld\n",
                                             lwrrentTid, lwrrentLoop,
                                             newSbeCount, oldSbeCount + sbesPerInjection);
                        m_ErrorRc = RC::BAD_MEMORY;
                    }
                }
                else
                {
                    CHECK_RC(GetErrCounts(&newSbeCount, &newDbeCount,
                                          oldSbeCount,
                                          oldDbeCount + dbesPerInjection));

                    if ((newDbeCount < (oldDbeCount + dbesPerInjection)) ||
                        (newDbeCount > (oldDbeCount + maxDbesPerInjection)) )
                    {
                        Printf(Tee::PriError, "[Thread: %d, Loop: %d]Bad SHM DBE error count:"
                                             " count = %lld, expected = %lld\n",
                                             lwrrentTid, lwrrentLoop,
                                             newDbeCount, oldDbeCount + dbesPerInjection);
                        m_ErrorRc = RC::BAD_MEMORY;
                    }
                }

                Printf(priPrintCounts, "ShmSM Counts(old,new,delta): SBE(%llu,%llu,%llu) DBE(%llu,%llu,%llu)\n",
                        oldSbeCount, newSbeCount, (newSbeCount - oldSbeCount),
                        oldDbeCount, newDbeCount, (newDbeCount - oldDbeCount));

                oldSbeCount = newSbeCount;
                oldDbeCount = newDbeCount;
                break;

            case GPU_DONE:
                Printf(pri, "All gpu threads are DONE\n");
                break;

        }
        for (UINT32 ii = 0; ii < m_NumSms * 2; ii += 2)
        {
            buffer[ii] = HOST_DONE;
        }
        SurfaceUtils::WriteSurface(m_CmdSurf, 0, &buffer[0], m_CmdSurfSize, 0);

        if (command == GPU_DONE)
            break;
    }
    CHECK_RC(m_LwdaFunctionDoneEvent.Synchronize());
    return rc;
}

// Args passed to PollErrCounts()
struct PollErrCountsParams
{
    GpuSubdevice *pSubdev;
    UINT64 MinSbeCount;
    UINT64 MinDbeCount;
    UINT64 SbeCount;
    UINT64 DbeCount;
};

//--------------------------------------------------------------------
static bool PollForErrCounts(void *pPollForErrCountsParams)
{
    PollErrCountsParams *pParams =
        (PollErrCountsParams*)pPollForErrCountsParams;
    GpuSubdevice *pSubdev = pParams->pSubdev;
    Tasker::MutexHolder Lock(pSubdev->GetEccErrCounter()->GetMutex());
    RC rc;

    Ecc::DetailedErrCounts SMShmParams;
    rc = pSubdev->GetSubdeviceGraphics()->GetSHMDetailedEccCounts(&SMShmParams);
    if (rc != OK)
    {
        return true;
    }

    pParams->SbeCount = 0;
    pParams->DbeCount = 0;
    for (UINT32 gpc = 0; gpc < SMShmParams.eccCounts.size(); ++gpc)
    {
        for (UINT32 tpc = 0; tpc < SMShmParams.eccCounts[gpc].size(); ++tpc)
        {
            pParams->SbeCount += SMShmParams.eccCounts[gpc][tpc].corr;
            pParams->DbeCount += SMShmParams.eccCounts[gpc][tpc].uncorr;
        }
    }
    return (pParams->SbeCount >= pParams->MinSbeCount &&
            pParams->DbeCount >= pParams->MinDbeCount);
}

//--------------------------------------------------------------------
RC EccSMShmTest::GetErrCounts
(
    UINT64 *pSbeCount,
    UINT64 *pDbeCount,
    UINT64 MinSbeCount,
    UINT64 MinDbeCount
)
{
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    RC rc;

    PollErrCountsParams Params =
    {
        GetBoundGpuSubdevice(),
        MinSbeCount,
        MinDbeCount,
        0, //SbeCount
        0  //DbeCount
    };
    POLLWRAP_HW(PollForErrCounts, &Params, pTestConfig->TimeoutMs());
    *pSbeCount = Params.SbeCount;
    *pDbeCount = Params.DbeCount;

    return rc;
}

//--------------------------------------------------------------------
RC EccSMShmTest::WriteGlobal(Lwca::Global Dst, const void *pSrc, UINT32 numWords)
{
    MASSERT(pSrc != NULL);
    UINT32 size = numWords * sizeof(UINT32);
    RC rc;
    CHECK_RC(Dst.InitCheck());

    if (size > Dst.GetSize() || size > m_LwdaMemcpyBuffer.GetSize())
    {
        MASSERT(!"Bad size passed to EccL1Test::WriteGlobal");
        return RC::SOFTWARE_ERROR;
    }

    memcpy(m_LwdaMemcpyBuffer.GetPtr(), pSrc, size);
    CHECK_RC(Dst.Set(m_LwdaMemcpyStream, m_LwdaMemcpyBuffer.GetPtr(),
                     0, size));
    CHECK_RC(m_LwdaMemcpyStream.Synchronize());
    return rc;
}

//--------------------------------------------------------------------
// JavaScript interface
JS_CLASS_INHERIT(EccSMShmTest, LwdaStreamTest, "EccSMShmTest object");

CLASS_PROP_READWRITE(EccSMShmTest, PrintCounts, bool,
        "Prints ECC counts for each loop");
