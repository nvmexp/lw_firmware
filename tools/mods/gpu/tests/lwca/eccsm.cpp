/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
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
#include "ctrl/ctrl2080.h"       // LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/xp.h"
#include "gpu/utility/bglogger.h"
#include "cheetah/include/teggpurgown.h"

// TODO(stewarts): Old test. If this test is to be revived, all references of
// SBE/DBE from RM need to be updated to corr/uncorr.

//--------------------------------------------------------------------
//! \brief ECC SM Test
//!
//! Test the ECC (Error Correcting Code) feature in the SM registers.
//! The ECC can correct SBEs (Single-Bit Errors) and detect DBEs
//! (Double-Bit Errors).
//!
class EccSMTest : public LwdaStreamTest
{
public:
    EccSMTest();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual void PrintJsProperties(Tee::Priority Pri);

    SETGET_PROP(PrintCounts, UINT32);

    // Enumerations for when to print the ECC counts arrays.
    enum
    {
        PRINT_ALL_COUNTS = 0x1,
        PRINT_BAD_COUNTS = 0x2
    };

private:
    RC InjectAndCheckErrors();
    RC WriteGlobal(Lwca::Global Dst, const void *pSrc, UINT32 numWords);
    using Counts = Ecc::DetailedErrCounts;
    RC GetErrCounts
    (
        Counts*       pEccCountsDelta,
        Counts*       pLastEccCounts,
        const Counts& startEccCounts,
        bool          sbeError,
        UINT32        errorsPerInjection
    );

    UINT32 m_PrintCounts;
    Random *m_pRandom;            //!< Random-number generator
    UINT32 m_NumSms;
    UINT32 m_WarpsPerSM;

    StickyRC m_ErrorRc;           //!< Used to store error

    UINT32 m_SavedFbBackdoor;     //!< To restore BACKDOOR_ACCESS in cleanup
    UINT32 m_SavedHostFbBackdoor; //!< To restore BACKDOOR_ACCESS in cleanup
    bool m_RestoreBackdoorInCleanup; //!< m_Saved*Backdoor has valid data

    Lwca::Module m_LwdaModule;
    Lwca::Function m_LwdaFunction;
    Lwca::Event m_LwdaFunctionDoneEvent;
    Lwca::Stream m_LwdaMemcpyStream;
    Lwca::HostMemory m_LwdaMemcpyBuffer;
    Lwca::ClientMemory m_CmdMem;
    Surface2D m_CmdSurf;
    UINT32 m_CmdSurfSize;

    enum
    {
        ECC_ENABLED = 0,
        ECC_DISABLED
    };
};

//--------------------------------------------------------------------
//! \brief constructor
//!
EccSMTest::EccSMTest() :
    m_PrintCounts(0),
    m_pRandom(&GetFpContext()->Rand),
    m_NumSms(0),
    m_WarpsPerSM(32),
    m_SavedFbBackdoor(0),
    m_SavedHostFbBackdoor(0),
    m_RestoreBackdoorInCleanup(false),
    m_CmdSurfSize(0)
{
    // Disable the RC watchdog on the subdevice for the duration of the test
    SetDisableWatchdog(true);
}

//--------------------------------------------------------------------
//! \brief Test whether SM ECC is supported and enabled
//!
//! This method checks the following:
//! a. Whether RM supports querying ECC status.
//! b. ECC on SM registers is enabled.
//! c. Ecc error counter is initialized successfully.
//! d. Lwca compute capability of the device >= 2.0
//!
/* virtual */ bool EccSMTest::IsSupported()
{
    if (!LwdaStreamTest::IsSupported())
        return false;

    LwRm *pLwRm = GetBoundRmClient();
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    Lwca::Device LwdaDevice = GetBoundLwdaDevice();
    Lwca::Capability cap = LwdaDevice.GetCapability();

    // But in T186/GP10B LRF ECC - undercounts and overcounts
    if (pGpuSubdevice->HasBug(1795584))
    {
        return false;
    }

    LW2080_CTRL_GPU_QUERY_ECC_STATUS_PARAMS EccStatus = {};
    if (OK != pLwRm->ControlBySubdevice(pGpuSubdevice,
                                        LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS,
                                        &EccStatus, sizeof(EccStatus)))
    {
        return false;
    }

    // Bug 1755852: This test is deprecated on Volta+ gpus
    if (EccStatus.units[LW2080_CTRL_GPU_ECC_UNIT_SM].enabled &&
            pGpuSubdevice->GetEccErrCounter()->IsInitialized() &&
            cap < Lwca::Capability::SM_70)
    {
        // make sure we have a special VBIOS that can toggle ECC enable state
        bool bEccEnable   = false;
        bool bEccOverride = false;

        // Save ECC checkbits state and restore on exit
        if (OK != pGpuSubdevice->GetSmEccEnable(&bEccEnable, &bEccOverride))
        {
            return false;
        }
        const bool bEccOrigEnable = bEccEnable;
        DEFER
        {
            pGpuSubdevice->SetSmEccEnable(bEccOrigEnable, bEccOverride);
        };

        // Flip ECC checkbits state
        bEccEnable = !bEccEnable;
        if (OK != pGpuSubdevice->SetSmEccEnable(bEccEnable, true))
        {
            return false;
        }

        // Confirm checkbits were changed
        bool bLwrEccEnable = false;
        if (OK != pGpuSubdevice->GetSmEccEnable(&bLwrEccEnable, nullptr))
        {
            return false;
        }
        if (bLwrEccEnable != bEccEnable)
        {
            Printf(Tee::PriLow, "Can't toggle SM ECC state. Check your VBIOS!\n");
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
//! \brief Set up the test
//!
//! Note: In addition to the setups done in this function, this test
//! assumes that the EccErrCounter class already waited for the RM to
//! finish scrubbing the ECC.
//!
/* virtual */ RC EccSMTest::Setup()
{
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    RC rc;

    CHECK_RC(LwdaStreamTest::Setup());
    m_pRandom->SeedRandom(pTestConfig->Seed());
    m_NumSms = GetBoundLwdaDevice().GetShaderCount();

    // Allocate the lwca objects
    //
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("eccsm", &m_LwdaModule));

    const UINT32 threadsPerSM = GetBoundLwdaDevice().GetAttribute(
            LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);

    m_WarpsPerSM = threadsPerSM/WARP_SIZE;

    m_LwdaFunction = m_LwdaModule.GetFunction("InjectErrors");

    m_LwdaFunction.SetGridDim(m_NumSms);
    m_LwdaFunction.SetBlockDim(WARP_SIZE, m_WarpsPerSM);

    m_LwdaFunctionDoneEvent = GetLwdaInstance().CreateEvent();
    CHECK_RC(m_LwdaFunctionDoneEvent.InitCheck());
    m_LwdaMemcpyStream = GetLwdaInstance().CreateStream();
    CHECK_RC(m_LwdaMemcpyStream.InitCheck());
    CHECK_RC(GetLwdaInstance().AllocHostMem(1024, &m_LwdaMemcpyBuffer));

    /* Each thread sends 1 word for command and 1 word for argument */
    m_CmdSurfSize = m_NumSms * m_WarpsPerSM * sizeof(UINT32) * 2;

    m_CmdSurf.SetArrayPitch(m_CmdSurfSize);
    m_CmdSurf.SetColorFormat(ColorUtils::I8);
    m_CmdSurf.SetLocation(Memory::Coherent);
    CHECK_RC(m_CmdSurf.Alloc(pGpuDevice));
    m_CmdMem = GetLwdaInstance().MapClientMem(m_CmdSurf.GetMemHandle(), 0,
                                              m_CmdSurfSize, pGpuDevice,
                                              m_CmdSurf.GetLocation());
    CHECK_RC(m_CmdMem.InitCheck());

    // Disable backdoor accesses
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

    m_ErrorRc.Clear();

    return rc;
}

//--------------------------------------------------------------------
//! \brief Run the test
//!
/* virtual */ RC EccSMTest::Run()
{
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    EccErrCounter *pEccErrCounter = pGpuSubdevice->GetEccErrCounter();
    BgLogger::PauseBgLogger DisableBgLogger;
    LwRm::DisableRcRecovery DisableRcRecovery(pGpuSubdevice);
    RC rc;

    unique_ptr<TegraGpuRailGateOwner> pTegraRG;
    if (pGpuSubdevice->IsSOC())
    {
        pTegraRG.reset(new TegraGpuRailGateOwner);
        CHECK_RC(pTegraRG->Claim());
        CHECK_RC(pTegraRG->DisableAllGpuPowerGating());
    }

    // Tell the EccErrCounter that this test is about to start
    // injecting errors.
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::SM_CORR));
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::SM_UNCORR));
    Utility::CleanupOnReturn<EccErrCounter, RC>
        ExpectingErrors(pEccErrCounter, &EccErrCounter::EndExpectingAllErrors);

    // Start injecting and checking for errors
    //
    CHECK_RC(InjectAndCheckErrors());

    // Tell the EccErrCounter that this test is done injecting errors.
    //
    ExpectingErrors.Cancel();
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::SM_CORR));
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::SM_UNCORR));

    return m_ErrorRc;
}

//--------------------------------------------------------------------
//! \brief Clean up the test
//!
/* virtual */ RC EccSMTest::Cleanup()
{
    RC rc;
    if (m_RestoreBackdoorInCleanup)
    {
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4,
                              m_SavedFbBackdoor);
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_HOSTFB, 4,
                              m_SavedHostFbBackdoor);
        m_RestoreBackdoorInCleanup = false;
    }

    m_CmdSurf.Free();
    m_CmdMem.Free();
    m_LwdaMemcpyBuffer.Free();
    m_LwdaMemcpyStream.Free();
    m_LwdaFunctionDoneEvent.Free();
    m_LwdaModule.Unload();
    CHECK_RC(LwdaStreamTest::Cleanup());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Print the test's JavaScript properties
//!
/* virtual */ void EccSMTest::PrintJsProperties(Tee::Priority Pri)
{
    LwdaStreamTest::PrintJsProperties(Pri);
    Printf(Pri, "EccSMTest Js Properties:\n");
}

//--------------------------------------------------------------------
//! \brief Inject and check for multiple errors
//!
//! This method kicks off the LWCA kernel. The kernel interacts with
//! the test using the global "HostCommand" array.
//! The general sequence for every thread is as follows:
//! 1. Callwlate the offset in the shared surface that it will use to
//     communicate with the host.
//! 2. Each thread runs for as many number of loops as specified by JS
//! 3. In each loop, there is a random thread number generated that will
//!    run the ECC test on its registers. This ensures that only 1 thread in
//!    every warp will run the test at a time(Running multiple tests in the
//!    same warp does not cause any difference in the ECC count).
//! 4. In the loop, the active thread does the following:
//!    1. Ask host to store the thread number
//!    2. Ask host to store the type of error to be generated.
//!    3. Ask host to store the random value to be written
//!    4. Ask host to disable the checkbits
//!    5. Toggle 1/2 bits of the victim register
//!    6. Ask host to enable checkbits
//!    7. Ask host to verify the value that is read back
//!       a. Host checks if the value is the same as written value in case of SBE
//!       b. Host checks if the value is different as written value in case of DBE
//!    8. Ask host to verify the number of ECC errors.
//!       Host will ask RM to get the updated ECC counters.
//!       The new ECC counts should be equal the expected ECC counts callwlated based
//        on the number of threads running.
//!    9. After all the threads are done running, SM gives command 'GPU_DONE'
//!       In case of an ECC error, the threads bail out and send 'GPU_DONE' command
//!
RC EccSMTest::InjectAndCheckErrors()
{
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    UINT32 loops = pTestConfig->Loops();
    RC rc;

    UINT32 storeVal = 0;

    UINT32 lwrrentThreadId = 0;
    bool bEccEnable   = false;
    bool bEccOverride = false;

    CHECK_RC(pGpuSubdevice->GetSmEccEnable(&bEccEnable, &bEccOverride));
    CHECK_RC(pGpuSubdevice->SetSmEccEnable(true, true));

    //Make sure we restore the EccState on exit.
    DEFER
    {
        pGpuSubdevice->SetSmEccEnable(bEccEnable, bEccOverride);
    };

    vector<UINT32> buffer(m_NumSms * m_WarpsPerSM * 2, HOST_DONE);

    SurfaceUtils::WriteSurface(m_CmdSurf, 0, &buffer[0], m_CmdSurfSize, 0);

    UINT64 devPtr = m_CmdMem.GetDevicePtr();
    CHECK_RC(WriteGlobal(m_LwdaModule["HostCommand"], &devPtr, 2));

    // Seed that will generate the sequence of random numbers
    UINT32 seed = m_pRandom->GetRandom();
    CHECK_RC(WriteGlobal(m_LwdaModule["Seed"], &seed, 1));

    // Store the number of thread loops(as passed by JS) per thread
    CHECK_RC(WriteGlobal(m_LwdaModule["NumLoops"], &loops, 1));

    Counts eccCounts;
    CHECK_RC(pGpuSubdevice->GetSubdeviceGraphics()->GetRfDetailedEccCounts(&eccCounts));

    // Start the kernel
    //
    CHECK_RC(m_LwdaFunction.Launch());
    // TODO: We should have a way to terminate the kernel, otherwise it will keep
    // running if any CHECK_RC below returns from the function.
    CHECK_RC(m_LwdaFunctionDoneEvent.Record());

    /* lwrrentLoop incremented when GPU asks host to store thread id */
    UINT32 lwrrentLoop = 0;

    // LWCA's GetShaderCount() returns a multiple of GetTpcCount()
    const UINT32 numTpcs = pGpuSubdevice->GetTpcCount();
    MASSERT(numTpcs <= m_NumSms);
    MASSERT(m_NumSms % numTpcs == 0);
    const UINT32 errorsPerInjection = m_WarpsPerSM * (m_NumSms / numTpcs);

    /* Tolerance?
       After the fist ECC error is detected by SM, it takes some clock cycles
       for the SM to go to the special state, in this window, if there are more
       ECC errors, the ECC counter keeps on getting incremented. With tolerance,
       we are assuming that the number of ECC errors that occur during this window is
       not more than 12 DBE per SM(Callwlated as per trial runs done locally). */
    const UINT32 maxSbePerInjection = errorsPerInjection + 0;
    const UINT32 maxDbePerInjection = errorsPerInjection + 12;

    bool sbeError = false;

    /* Detecting the GPU hang. If we don't receive a command from the GPU in TimeoutMs
       assume the GPU is hung and bail out.
    */
    FLOAT64 timeoutMs = GetTestConfiguration()->TimeoutMs();
    UINT64 waitStartMs = Xp::GetWallTimeMS();
    LwRm::ExclusiveLockRm lockRm(GetBoundRmClient());
    Tee::Priority pri = GetVerbosePrintPri();
    Tee::Priority priPrintCounts = (m_PrintCounts ? Tee::PriNormal : pri);

    while (1)
    {
        UINT32 hostCommand;
        UINT32 arg;
        bool   newCommand = true;

        CHECK_RC(SurfaceUtils::ReadSurface(m_CmdSurf, 0, &buffer[0], m_CmdSurfSize, 0));

        for (UINT32 ii = 0; ii < m_NumSms * m_WarpsPerSM * 2; ii += 2)
        {
            if (buffer[ii + COMMAND_OFFSET] == HOST_DONE)
            {
                newCommand = false;
                break;
            }
            else if (buffer[ii + COMMAND_OFFSET] != buffer[0 + COMMAND_OFFSET])
            {
                MASSERT(!"Kernels have diverged in SMs");
                m_ErrorRc = RC::SOFTWARE_ERROR;
                newCommand = true;
                break;
            }
        }

        if (!newCommand)
        {
            if ((Xp::GetWallTimeMS() - waitStartMs) > timeoutMs)
            {
                Printf(Tee::PriError, "*** Host timed out waiting for a command from GPU. ***\n");
                m_ErrorRc = RC::TIMEOUT_ERROR;
                return rc;
            }
            continue;
        }

        // Only yield when processing a new command and ECC is actually enabled
        // (so we aren't yielding when the checkbits are disabled and we are
        // actually injecting errors which could cause problems with the test)
        if (bEccEnable)
            Tasker::Yield();

        hostCommand = buffer[COMMAND_OFFSET];
        arg         = buffer[ARG_OFFSET];

        switch (hostCommand)
        {

            case HOST_STORE_THREAD_ID:
                lwrrentThreadId = arg;
                Printf(pri, "Current Thread: %d\n", lwrrentThreadId);
                lwrrentLoop ++;
                break;

            case HOST_PRINT:
                Printf(pri, "Print 0x%08x\n", arg);
                break;

            case HOST_STORE_ERR_TYPE:
                sbeError = (arg == 0) ? true : false;
                Printf(pri, "ErrorType: %s\n", sbeError? "SBE" : "DBE");
                break;

            case HOST_STORE_VAL:
                Printf(pri, "STORE 0x%08x\n", arg);
                storeVal = arg;
                break;

            case HOST_DISABLE_CHECKBITS:
                Printf(pri, "Disabling checkbits\n");
                bEccEnable = false;
                CHECK_RC(lockRm.AcquireLock());
                CHECK_RC(pGpuSubdevice->SetSmEccEnable(bEccEnable, true));
                break;

            case HOST_ENABLE_CHECKBITS:
                Printf(pri, "Enabling checkbits\n");
                bEccEnable = true;
                CHECK_RC(pGpuSubdevice->SetSmEccEnable(bEccEnable, true));
                lockRm.ReleaseLock();
                break;

            case HOST_CHECK_VAL:
                Printf(pri, "CHECK 0x%08x\n", arg);
                for (UINT32 warpId = 0;
                     warpId < m_NumSms * m_WarpsPerSM * 2; warpId += 2)
                {
                    const UINT32 checkVal = buffer[warpId + ARG_OFFSET];
                    bool error = false;

                    if (sbeError)
                    {
                        if (checkVal != storeVal)
                        {
                            error = true;
                        }
                    }
                    else if ((storeVal != 0) && (checkVal == storeVal))
                    {
                        //If Stored value is 0 then after DED detection it is
                        //not guaranteed that the new value will remain 0
                        error = true;
                    }

                    if (error)
                    {
                        Printf(Tee::PriError,
                               "[SM: %u, Warp: %u, Thread: %u, Loop: %u]"
                               "%s inserted, but the read value(0x%08x)"
                               " is %s as written value(0x%08x).\n",
                               (warpId/2U) / m_WarpsPerSM,
                               (warpId/2U) % m_WarpsPerSM,
                               lwrrentThreadId, lwrrentLoop,
                               sbeError ? "SBE" : "DBE",
                               checkVal,
                               sbeError ? "NOT the same" : "the SAME",
                               storeVal);
                        m_ErrorRc = RC::BAD_MEMORY;
                        GetJsonExit()->AddFailLoop(lwrrentLoop);
                    }
                }
                break;

            case HOST_CHECK_COUNTS:
            {
                Printf(pri, "Checking error counts\n");

                const Counts oldEccCounts = eccCounts;
                Counts eccCountsDelta;

                rc = GetErrCounts(&eccCountsDelta, &eccCounts, oldEccCounts,
                                  sbeError, errorsPerInjection);
                if (rc != OK)
                {
                    // When there is an error, the counters will not increment
                    // and we will notice it and catch the error.
                    rc.Clear();
                }

                bool reportError = false;

                for (UINT32 gpc = 0; gpc < eccCountsDelta.eccCounts.size(); gpc++)
                {
                    for (UINT32 tpc = 0; tpc < eccCountsDelta.eccCounts[gpc].size(); tpc++)
                    {
                        const auto&  cntDelta = eccCountsDelta.eccCounts[gpc][tpc];
                        const UINT64 count    = sbeError ? cntDelta.corr : cntDelta.uncorr;
                        const auto&  cntOld   = oldEccCounts.eccCounts[gpc][tpc];
                        const auto&  cntNew   = eccCounts.eccCounts[gpc][tpc];
                        const UINT64 maxCount = sbeError ? maxSbePerInjection : maxDbePerInjection;
                        Printf(priPrintCounts, "[GPC: %u, TPC: %u] LRF ECC(old:new:delta): "
                                "SBE(%09lld:%09lld:%04lld) DBE(%09lld:%09lld:%04lld)\n",
                                gpc, tpc,
                                cntOld.corr, cntNew.corr, cntDelta.corr,
                                cntOld.uncorr, cntNew.uncorr, cntDelta.uncorr);
                        if (count < errorsPerInjection || count > maxCount)
                        {
                            Printf(Tee::PriError,
                                    "[GPC: %u, TPC: %u, Thread: %u, Loop: %u] "
                                    "Bad LRF %s error count: "
                                    "count = %llu, expected = %u\n",
                                    gpc, tpc, lwrrentThreadId, lwrrentLoop,
                                    sbeError ? "SBE" : "DBE",
                                    count, errorsPerInjection);
                            reportError = true;
                        }
                    }
                }

                if (reportError)
                {
                    m_ErrorRc = RC::BAD_MEMORY;
                    GetJsonExit()->AddFailLoop(lwrrentLoop);
                }
                break;
            }

            case GPU_DONE:
                Printf(pri, "All gpu threads are DONE\n");
                break;

        }

        /* Return the error code to the kernel threads so that they stop in case of error */
        for (UINT32 ii = 0; ii < m_NumSms * m_WarpsPerSM * 2; ii += 2)
        {
            buffer[ii + REPLY_OFFSET] = HOST_DONE;
        }

        SurfaceUtils::WriteSurface(m_CmdSurf, 0, &buffer[0], m_CmdSurfSize, 0);

        if (hostCommand == GPU_DONE)
            break;

        waitStartMs = Xp::GetWallTimeMS();
    }

    CHECK_RC(m_LwdaFunctionDoneEvent.Synchronize());
    return rc;
}

RC EccSMTest::WriteGlobal(Lwca::Global Dst, const void *pSrc, UINT32 numWords)
{
    MASSERT(pSrc != NULL);
    UINT32 size = numWords * sizeof(UINT32);
    RC rc;
    CHECK_RC(Dst.InitCheck());

    if (size > Dst.GetSize() || size > m_LwdaMemcpyBuffer.GetSize())
    {
        MASSERT(!"Bad size passed to EccSMTest::WriteGlobal");
        return RC::SOFTWARE_ERROR;
    }

    memcpy(m_LwdaMemcpyBuffer.GetPtr(), pSrc, size);
    CHECK_RC(Dst.Set(m_LwdaMemcpyStream, m_LwdaMemcpyBuffer.GetPtr(),
                     0, size));
    CHECK_RC(m_LwdaMemcpyStream.Synchronize());
    return rc;
}

RC EccSMTest::GetErrCounts
(
    Counts*       pEccCountsDelta,
    Counts*       pLastEccCounts,
    const Counts& startEccCounts,
    bool          sbeError,
    UINT32        errorsPerInjection
)
{
    // Ensure all delta counters are initially set to 0
    *pEccCountsDelta = startEccCounts - startEccCounts;

    // Poll and wait until the ECC counters increment by expected amount
    return Tasker::PollHw(GetTestConfiguration()->TimeoutMs(),
        // The poll lambda returns true if counters for all TPCs
        // have incremented by expected amount and false if it needs
        // to be ilwoked again by PollHw().
        [&]()->bool
        {
            auto& gr = *GetBoundGpuSubdevice()->GetSubdeviceGraphics();
            if (OK != gr.GetRfDetailedEccCounts(pLastEccCounts))
            {
                return true;
            }
            *pEccCountsDelta = *pLastEccCounts - startEccCounts;
            bool allOK = true;
            for (const auto& gpc : pEccCountsDelta->eccCounts)
            {
                for (const auto& counts : gpc)
                {
                    const UINT64 count = sbeError ? counts.corr : counts.uncorr;
                    if (count < errorsPerInjection)
                    {
                        allOK = false;
                    }
                }
            }
            return allOK;
        }, __FUNCTION__);
}

//--------------------------------------------------------------------
// JavaScript interface
//
JS_CLASS_INHERIT(EccSMTest, LwdaStreamTest, "EccSMTest object");

CLASS_PROP_READWRITE(EccSMTest, PrintCounts, UINT32,
        "Prints ECC counts for each loop (1=Print all counts, 2=Print only bad counts");
