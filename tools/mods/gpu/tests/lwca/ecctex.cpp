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
#include "gpu/utility/eclwtil.h"

// TODO(stewarts): Old test. If this test is to be revived, all references of
// SBE/DBE from RM need to be updated to corr/uncorr.

//--------------------------------------------------------------------------------------------------
//! \brief ECC TEX Test
//!
//! Test the ECC feature in the Texture cache Memory.
//! The ECC can correct SEC (Single-Bit Errors) and detect DED(Double-Bit Errors).
//!
class EccTexTest : public LwdaStreamTest
{
public:
    EccTexTest();
    virtual ~ EccTexTest(){};
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    SETGET_PROP(NumSMs, UINT32);

private:
    enum {
        ScratchOffset = 32
    };
    RC               CheckValuesAndAdjustCounts
                    (
                        UINT32 lwrrentTid,
                        UINT64 oldSbeCount,
                        UINT64 oldDbeCount,
                        UINT64 newSbeCount,
                        UINT64 newDbeCount,
                        bool sbeError,
                        Tee::Priority pri
                    );
    RC              DumpCommandBuffer(Tee::Priority pri);
    void            DumpTestSurface(Tee::Priority pri);
    RC              GetErrCounts
                    (
                        UINT64 *pSbeCount,
                        UINT64 *pDbeCount,
                        UINT64 minSbeCount,
                        UINT64 minDbeCount,
                        Tee::Priority pri
                    );
    RC              InjectAndCheckErrors();
    RC              WriteGlobal
                    (
                        Lwca::Global dst,
                        const void *pSrc,
                        UINT32 numWords
                    );

    Random *        m_pRandom;                  //!< Random-number generator
    UINT32          m_NumSMs         = 0;
    UINT32          m_NumWarpsPerSM  = 2;       //!< Number of warps per SM needed to test all
                                                //!< TEX pipes
    UINT32          m_NumWarps =0;                 //!< Total number of warps running per loop.
    StickyRC        m_ErrorRc = OK;
    UINT32          m_Loops = 0;
    GpuSubdevice *  m_pSubdev = nullptr;
    UINT32          m_SubdevInst = 0;
    UINT32          m_SavedFbBackdoor = 0;          //!< To restore BACKDOOR_ACCESS in cleanup
    UINT32          m_SavedHostFbBackdoor = 0;      //!< To restore BACKDOOR_ACCESS in cleanup
    bool            m_RestoreBackdoorInCleanup = false;
    Lwca::Module    m_LwdaModule;
    Lwca::Function  m_LwdaFunction;
    Lwca::Event     m_LwdaFunctionDoneEvent;
    Lwca::Stream    m_LwdaMemcpyStream;
    Lwca::HostMemory m_LwdaMemcpyBuffer;
    Lwca::ClientMemory m_CmdMem;
    Surface2D       m_CmdSurf;
    UINT32          m_CmdSurfSize = 0;
    UINT32          m_ThreadsPerWarp = 0;
    Surface2D       m_TestSurf;
    UINT32          m_TestSurfSize = 0;
    Lwca::ClientMemory m_TestMem;
};

//--------------------------------------------------------------------------------------------------
EccTexTest::EccTexTest():
      m_pRandom(&GetFpContext()->Rand)
{
    // Disable the RC watchdog on the subdevice for the duration of the test
    SetDisableWatchdog(true);
}

//--------------------------------------------------------------------------------------------------
bool EccTexTest::IsSupported()
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

    if (EccStatus.units[LW2080_CTRL_GPU_ECC_UNIT_TEX].enabled &&
            m_pSubdev->GetEccErrCounter()->IsInitialized() &&
            cap >= Lwca::Capability::SM_60)
    {
        // make sure we have a special VBIOS that can toggle ECC enable state
        bool bEccEnable   = false;
        bool bEccOverride = false;

        // Save ECC checkbits state and restore on exit
        if (OK != m_pSubdev->GetTexEccEnable(&bEccEnable, &bEccOverride))
        {
            return false;
        }
        const bool bEccOrigEnable = bEccEnable;
        DEFER
        {
            m_pSubdev->SetTexEccEnable(bEccOrigEnable, bEccOverride);
        };

        // Flip ECC checkbits state
        bEccEnable = !bEccEnable;
        if (OK != m_pSubdev->SetTexEccEnable(bEccEnable, true))
        {
            return false;
        }

        // Confirm checkbits were changed
        bool bLwrEccEnable = false;
        if (OK != m_pSubdev->GetTexEccEnable(&bLwrEccEnable,nullptr))
        {
            return false;
        }
        if (bLwrEccEnable != bEccEnable)
        {
            Printf(Tee::PriLow, "Can't toggle TEX ECC state. Check your VBIOS!\n");
            return false;
        }
        else
        {
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------------------------------------
/* virtual */ RC EccTexTest::Setup()
{
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    RC rc;

    CHECK_RC(LwdaStreamTest::Setup());
    m_pRandom->SeedRandom(pTestConfig->Seed());

    // If NumSMs was not passed via JS.
    if (m_NumSMs == 0)
    {
        // Use the GpuSubdevice shader count (which is real SMs and not
        // virtualized SMs).  The 2 tex pipes are shared between the 2 virtual
        // SMs per physical SM.
        m_NumSMs = GetBoundGpuSubdevice()->ShaderCount();
    }
    m_ThreadsPerWarp = GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_WARP_SIZE);

    // Note: we may need to adjust m_NumWarpsPerSM here to account for chips that don't have
    //       2 tex pipes per SM.
    m_NumWarps = m_NumWarpsPerSM * m_NumSMs;

    //Default is to test every thread in a warp.
    m_Loops = GetTestConfiguration()->Loops();
    if (m_Loops == 0)
    {
        m_Loops = m_ThreadsPerWarp;
    }

    // Allocate the lwca objects
    //
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("ecctex", &m_LwdaModule));

    m_LwdaFunction = m_LwdaModule.GetFunction("InjectErrors");
    m_LwdaFunction.SetGridDim(m_NumWarps);
    m_LwdaFunction.SetBlockDim(m_ThreadsPerWarp);
    m_LwdaFunctionDoneEvent = GetLwdaInstance().CreateEvent();
    CHECK_RC(m_LwdaFunctionDoneEvent.InitCheck());
    m_LwdaMemcpyStream = GetLwdaInstance().CreateStream();
    CHECK_RC(m_LwdaMemcpyStream.InitCheck());
    CHECK_RC(GetLwdaInstance().AllocHostMem(1024, &m_LwdaMemcpyBuffer));

    // Each thread writes 2 64bit values
    // Each Block has 64 threads(1 warp for each TEX Pipe)
    m_TestSurfSize = m_NumWarps * m_ThreadsPerWarp * sizeof(UINT64)*2;
    m_TestSurf.SetWidth(m_TestSurfSize);
    m_TestSurf.SetHeight(1);
    m_TestSurf.SetColorFormat(ColorUtils::A8R8G8B8);
    m_TestSurf.SetLocation(Memory::Coherent);
    CHECK_RC(m_TestSurf.Alloc(pGpuDevice));
    m_TestMem = GetLwdaInstance().MapClientMem(m_TestSurf.GetMemHandle(), 0,
                                                m_TestSurfSize, pGpuDevice,
                                                m_TestSurf.GetLocation());
    CHECK_RC(m_TestMem.InitCheck());

    // Each thread sends 1 word for command and 1 word for argument
    // In a CTA, at one time only 1 thread sends command. It is ensured by
    // an if condition in the kernel. i.e. if(threadId == LoopIdx)
    // Each TEX pipe is tied to an SM partition. Each SM has 2 partitions. So we need a minimum of
    // 2 warps per SM to test both TEX pipes.

    m_CmdSurfSize = m_NumWarps * sizeof(UINT32) * 2;
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

//--------------------------------------------------------------------------------------------------
/* virtual */ RC EccTexTest::Run()
{
    RC rc;
    m_pSubdev = GetBoundGpuSubdevice();
    m_SubdevInst = m_pSubdev->GetSubdeviceInst();
    EccErrCounter *pEccErrCounter = m_pSubdev->GetEccErrCounter();
    BgLogger::PauseBgLogger DisableBgLogger;
    LwRm::DisableControlTrace DisableControlTrace;
    LwRm::DisableRcRecovery DisableRcRecovery(m_pSubdev);

    // Tell the EccErrCounter that this test is about to start
    // injecting errors.  (Note: The RM uses an ISR to update the ECC
    // error counts
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::TEX_CORR));
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::TEX_UNCORR));
    Utility::CleanupOnReturn<EccErrCounter, RC>
        ExpectingErrors(pEccErrCounter, &EccErrCounter::EndExpectingAllErrors);

    // Start injecting and checking for errors
    CHECK_RC(InjectAndCheckErrors());

    // Tell the EccErrCounter that this test is done injecting errors.
    ExpectingErrors.Cancel();
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::TEX_CORR));
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::TEX_UNCORR));

    return m_ErrorRc;
}

//--------------------------------------------------------------------------------------------------
/* virtual */ RC EccTexTest::Cleanup()
{
    if (m_RestoreBackdoorInCleanup)
    {
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4,
                              m_SavedFbBackdoor);
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_HOSTFB, 4,
                              m_SavedHostFbBackdoor);
        m_RestoreBackdoorInCleanup = false;
    }

    m_TestMem.Free();
    m_TestSurf.Free();
    m_CmdMem.Free();
    m_CmdSurf.Free();
    m_LwdaMemcpyBuffer.Free();
    m_LwdaMemcpyStream.Free();
    m_LwdaFunctionDoneEvent.Free();
    m_LwdaModule.Unload();
    return LwdaStreamTest::Cleanup();
}

//--------------------------------------------------------------------------------------------------
//! \brief Inject and check Ecc errors in TEX
//!
//! This function starts a LWCA kernel(InjectErrors).
//! Kernel running on device interacts with mods running as the host by using a
//! "HostCommand" object.
//! Code flow:
//! 1. Kernel asks the host to store the thread number. (each thread)
//! 2. Kernel asks the host to store the type of error SBE/DBE to be generated.
//! 3. Kernel asks the host to disable ECC.
//! 4. Kernel then modifies the stored data.
//! 5. Kernel asks the host to enable ECC.
//! 6. Kernel reads stored value to generate ECC error.
//! 7. Kernel asks the host to check the number of errors. Number of errors injected
//!    should match the ecc error counter value (TEX_CORR and TEX_UNCORR).
//! 8. Kernel threads finish and notify the host by writing GPU_DONE in the Hostcommand
RC EccTexTest :: InjectAndCheckErrors()
{
    RC rc;
    bool bEccEnable   = false;
    bool bEccOverride = false;
    Tee::Priority pri = GetVerbosePrintPri();
    CHECK_RC(m_pSubdev->GetTexEccEnable(&bEccEnable, &bEccOverride));
    CHECK_RC(m_pSubdev->SetTexEccEnable(true,true));

    //Make sure we restore the EccState on exit.
    DEFER
    {
        m_pSubdev->SetTexEccEnable(bEccEnable, bEccOverride);
    };

    vector<UINT32> buffer(m_CmdSurfSize/sizeof(UINT32), 0);
    for(UINT32 ii = 0; ii < m_NumWarps * 2; ii += 2)
    {
        buffer[ii] = HOST_DONE;
    }
    SurfaceUtils::WriteSurface(m_CmdSurf, 0, &buffer[0], m_CmdSurfSize, m_SubdevInst);
    UINT64 devPtr = m_CmdMem.GetDevicePtr();
    CHECK_RC(WriteGlobal(m_LwdaModule["HostCommand"], &devPtr, 2));

    //Global data used by the kernel.
    // 2 entries per warp, are used as follows:
    // 1st entry:
    // -Stores the value written when ECC is disabled
    // 2nd entry:
    // -ReadOp is used to flush the cache line and clear the flops. This is required to overcome
    // a hardware optimization. This address must be a mutiple of a cache line(128 bytes)
    // from the 1st entry to clear the correct hw flops.
    // -WritOp stores the value read back when ECC is enabled.
    vector<UINT64> buffer2((m_TestSurfSize/sizeof(UINT64)),0xDEADBEEF);
    SurfaceUtils::WriteSurface(m_TestSurf, 0, &buffer2[0], m_TestSurfSize, m_SubdevInst);
    UINT64 devPtr2 = m_TestMem.GetDevicePtr();
    CHECK_RC(WriteGlobal(m_LwdaModule["TestSurface"], &devPtr2, 2));

    // Seed that will generate the sequence of random numbers
    UINT32 seed = m_pRandom->GetRandom();
    CHECK_RC(WriteGlobal(m_LwdaModule["Seed"], &seed, 1));

    // Store the number of thread loops(as passed by JS) per thread
    CHECK_RC(WriteGlobal(m_LwdaModule["NumLoops"], &m_Loops, 1));

    // Start the kernel
    //
    CHECK_RC(m_LwdaFunction.Launch());
    CHECK_RC(m_LwdaFunctionDoneEvent.Record());

    // Record the number of SBE and DBE errors before injecting the error.
    //
    UINT64 oldSbeCount  = 0;
    UINT64 oldDbeCount  = 0;
    UINT64 newSbeCount  = 0;
    UINT64 newDbeCount  = 0;
    UINT64 origSbeCount = 0;
    UINT64 origDbeCount = 0;
    UINT32 lwrrentTid   = 0;
    bool sbeError       = true;
    LwRm::ExclusiveLockRm lockRm(GetBoundRmClient());

    //! Get initial values
    CHECK_RC(GetErrCounts(&oldSbeCount, &oldDbeCount,oldSbeCount, oldDbeCount, Tee::PriLow));
    origSbeCount = oldSbeCount;
    origDbeCount = oldDbeCount;

    //! This is the core loop of test where we communicate with the InjectError kernel.
    while (1)
    {
        UINT32 command;
        UINT32 arg;
        bool   NewCommand = true;

        CHECK_RC(SurfaceUtils::ReadSurface(m_CmdSurf, 0, &buffer[0], m_CmdSurfSize, m_SubdevInst));

        for (UINT32 ii = 0; ii < m_NumWarps * 2; ii += 2)
        {
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

        // Don't yield when ECC is disabled. When SM Ecc is disabled we inject errors
        if (bEccEnable)
        {
            Tasker::Yield();
        }

        //Note: the command for all SMs should be the same, however the value of arg may be
        //      different depending on the command.
        command = buffer[0];
        arg = buffer[1];

        switch (command)
        {
            case HOST_STORE_THREAD_ID:
                lwrrentTid = arg;
                Printf(pri, "STORE_THREAD_ID:%d\n", arg);
                break;

            case HOST_PRINT:
                Printf(pri,"HOST_PRINT: ");
                for (UINT32 ii = 0; ii < m_NumWarps * 2; ii += 2)
                {
                    Printf(pri, "Warp:%02d SM%02d Arg:0x%x(%d) ",
                           ii/2, ii/(2*m_NumWarpsPerSM), buffer[ii+1],buffer[ii+1]);
                }
                Printf(pri,"\n");
                break;

            case HOST_STORE_ERR_TYPE:
                sbeError = arg ? false : true;
                Printf(pri, "STORE_ERR_TYPE: %s\n", (sbeError?"SBE":"DBE"));
                break;

            case HOST_DISABLE_CHECKBITS:
                Printf(pri, "DISABLE_CHECKBITS:\n");
                bEccEnable = false;
                CHECK_RC(lockRm.AcquireLock());
                CHECK_RC(m_pSubdev->SetTexEccEnable(bEccEnable,true));
                break;

            case HOST_ENABLE_CHECKBITS:
            {
                Printf(pri, "ENABLE_CHECKBITS:\n");
                bEccEnable = true;
                CHECK_RC(m_pSubdev->SetTexEccEnable(bEccEnable, true));
                lockRm.ReleaseLock();
                break;
            }

            case HOST_CHECK_COUNTS:
            {
                Printf(pri, "CHECK_COUNTS:\n");
                //Perform a crude error count check where we expect 1 error to be generated
                //per warp. If that check fails then perform a closer analysis of the test
                //data to determine pass/fail.
                UINT64 expSbeCount = sbeError ? oldSbeCount + m_NumWarps : oldSbeCount;
                UINT64 expDbeCount = !sbeError ? oldDbeCount + m_NumWarps : oldDbeCount;
                CHECK_RC(GetErrCounts(&newSbeCount, &newDbeCount,
                                      expSbeCount, expDbeCount, Tee::PriLow));

                // Crude check failed, perform a more detailed check.
                if ((newSbeCount < expSbeCount) || (newDbeCount < expDbeCount))
                {
                    m_ErrorRc = CheckValuesAndAdjustCounts(lwrrentTid,
                                                           oldSbeCount, oldDbeCount,
                                                           newSbeCount, newDbeCount,
                                                           sbeError, pri);
                }
                else if ((newSbeCount > (expSbeCount << 1)) || (newDbeCount > (expDbeCount << 1)))
                {
                    Printf(Tee::PriError,
                           "Tex SBE/DBE counts beyound reasonable limits!\n"
                           "SbeCount:(old:%08llu, new:%08llu, delta:%08llu)"
                           " DbeCount:(old:%08llu new:%08llu delta:%08llu)\n",
                           oldSbeCount, newSbeCount, (newSbeCount-oldSbeCount),
                           oldDbeCount, newDbeCount, (newDbeCount-oldDbeCount));
                    m_ErrorRc = RC::SOFTWARE_ERROR;
                }
                Printf(pri,
                       "Tex SbeCount:(old:%08llu, new:%08llu, delta:%08llu)"
                       " DbeCount:(old:%08llu new:%08llu delta:%08llu)\n",
                       oldSbeCount, newSbeCount, (newSbeCount-oldSbeCount),
                       oldDbeCount, newDbeCount, (newDbeCount-oldDbeCount));
                oldSbeCount = newSbeCount;
                oldDbeCount = newDbeCount;
                break;
            }

            case GPU_DONE:
                Printf(pri, "GPU_DONE:All gpu threads are DONE\n");
                break;
        }

        // Release all of the warps to perform the next step.
        for (UINT32 ii = 0; ii < m_NumWarps * 2; ii += 2)
        {
            buffer[ii] = HOST_DONE;
        }
        SurfaceUtils::WriteSurface(m_CmdSurf, 0, &buffer[0], m_CmdSurfSize, m_SubdevInst);

        if (command == GPU_DONE)
            break;
    }
    CHECK_RC(m_LwdaFunctionDoneEvent.Synchronize());

    // Sanity check the counts.
    if ((oldSbeCount <= origSbeCount) || (oldDbeCount <= origDbeCount))
    {
        Printf(Tee::PriError, "Not enough SBE or DBE errors generated to pass this test.\n\t"
                              "OrigSbeCount=%lld FinalSbeCount=%lld "
                              "OrigDbeCount=%lld FinalDbeCount=%lld\n",
               origSbeCount, oldSbeCount, origDbeCount, oldDbeCount);
        m_ErrorRc = RC::SOFTWARE_ERROR;
    }
    return (rc);
}

//--------------------------------------------------------------------------------------------------
// Read the test surface and compare the values written against the values read.
// For SBE errors:
// We write a value with a single bit set and expect the value read to be zero. However
// if the value read back is the same as written then we probably have an issue were the
// L1 cache lines were ilwalided and we got a reload from the L2 cache with good checkbits.
// In that case reduce the number of expected errors by one and don't declare a failure.
// For DBE errors:
// We write a value with 2 bits set and expect and error to be generated. In this case the
// value read is undefined and can even be the same as the value written. However if the value
// read is equal to the value written we may have the same issue as above (L1 cache line has
// been ilwalidated) so we cant definitively say that we have an error. What we do know is the
// following:
// a) if the value read != value written, then we SHOULD have an error count.
// b) if the value read == value written, then we MAY have an error count.
//
RC EccTexTest::CheckValuesAndAdjustCounts
(   UINT32 tid,         //current thread ID
    UINT64 oldSbeCount,
    UINT64 oldDbeCount,
    UINT64 newSbeCount,
    UINT64 newDbeCount,
    bool sbeError,
    Tee::Priority pri)
{
    StickyRC rc;
    MASSERT(m_TestSurfSize == (m_NumWarps * m_ThreadsPerWarp * 2 * sizeof(UINT64)));
    vector<UINT64> buffer((m_TestSurfSize/sizeof(UINT64)),0xDEADBEEF);
    UINT64 expSbeCount = oldSbeCount;
    UINT64 expDbeCount = oldDbeCount;
    bool bDumpTestSurface = false;
    UINT32 idx = 0;

    //Read the test surface
    SurfaceUtils::ReadSurface(m_TestSurf, 0, &buffer[0], m_TestSurfSize, m_SubdevInst);

    // buffer[idx] is value written after ECC was disabled
    // buffer[idx+ScratchOffset] is value read back after ECC was enabled
    for (UINT32 warp = 0; warp < m_NumWarps; warp++)
    {
        idx = (warp * m_ThreadsPerWarp * 2) + tid;
        if ( sbeError )
        {
            // SBEs the value read should either be zero or the value written. Anything else
            // would be unexpected and we should fail.
            if (buffer[idx+ScratchOffset] == 0x00)
            {
                expSbeCount++;
            }
            else if (buffer[idx] == buffer[idx+ScratchOffset])
            {   // L1 cache lines were ilwalidated and reload from L2 cache with good checkbits.
                // Dump the test surface in verbose mode.
                bDumpTestSurface = true;
            }
            else
            {
                rc = RC::BAD_MEMORY;
                Printf(Tee::PriError, "Warp:%02d SM%02d: TID:%d value written:0x%08llx"
                       " vs value read:0x%08llx is not expected.\n",
                       warp, (warp / m_NumWarpsPerSM),tid, buffer[idx], buffer[idx+ScratchOffset]);
            }
        }
        else
        {
            // DBEs the value read is undefined when we get a DBE. However if the value read
            // matches the value written there is a chance that the L1 cache lines were ilwalided
            // and a reload from the L2 cache with good check bits oclwred.
            if( buffer[idx] != buffer[idx+ScratchOffset])
            {
                expDbeCount++;
            }
        }
    }

    // RM has the potential to undercount SBEs & DEBs by 1.
    if (m_pSubdev->HasBug(1740263))
    {
        if (sbeError && expSbeCount)
        {
            expSbeCount--;
        }
        else if (!sbeError && expDbeCount)
        {
            expDbeCount--;
        }
    }

    // Error conditions:
    // 1. new SBE count < expected SBE count
    // 2. new DBE count < expected DBE count
    // 3. if SBE and we have zero new SBEs
    // 4. if DBE and we have zero new DBEs
    if (sbeError && (newSbeCount == oldSbeCount))
    {
        rc = RC::BAD_MEMORY;    // we got zero new SBEs
        Printf(Tee::PriError, "No SBEs detected in the test surface for this loop.\n");
        pri = Tee::PriNormal;
    }
    else if (!sbeError && (newDbeCount == oldDbeCount))
    {
        rc = RC::BAD_MEMORY;    // we got zero new DBEs
        Printf(Tee::PriError, "No DBEs detected in the test surface for this loop.\n");
        pri = Tee::PriNormal;
    }
    else if ((newSbeCount < expSbeCount) || (newDbeCount < expDbeCount))
    {
        rc = RC::BAD_MEMORY;
        pri = Tee::PriNormal;
    }

    // Failure annalysis support for verbose output
    if (OK != rc)
    {
        bDumpTestSurface = true;
        Printf(pri,"oldSbe:%lld, newSbe:%lld expSbe:%lld\n oldDbe:%lld newDbe:%lld expDbe:%lld\n",
               oldSbeCount,newSbeCount,expSbeCount, oldDbeCount,newDbeCount,expDbeCount);
        GetErrCounts(&newSbeCount, &newDbeCount,expSbeCount,expDbeCount,pri);
    }
    if (bDumpTestSurface)
    {
        DumpTestSurface(pri);
    }

    return (rc);

}

//--------------------------------------------------------------------------------------------------
//Global data used by the kernel.
// 2 entries per thread, are used as follows:
// 1st entry: Stores the value written when ECC is disabled
// 2nd entry(scratch pad): Used to flush the cache line and clear the flops. This is required
// to overcome a hardware optimization. This entry must be a multiple of 128 bytes from the 1st
// entry to clear the proper cache line.
/*
      ____________________________
     |0x0000| TID 0 bad val       | Warp 0, SM 0
     |------|---------------------
     |0x0008| TID 1 bad val       |
     |------|---------------------
     |      ...                   |
     |------|---------------------
     |      | TID 31 bad val      |
     |------|---------------------
     |0x0100| TID 0 scratch pad   | Warp 0, SM 0
     |------|---------------------
     |      | TID 1 scratch pad   |
     |------|---------------------
     |      ...                   |
     |------|---------------------
     |      | TID 31 scratch pad  |
     |------|---------------------
     |0x0200| TID 0 bad val       | Warp 1, SM 0
     |------|---------------------
     |0x0208| TID 1 bad val       |
     |------|---------------------
     |      ...                   |

*/
void EccTexTest::DumpTestSurface(Tee::Priority pri)
{

    MASSERT(m_TestSurfSize == (m_NumWarps * m_ThreadsPerWarp * 2 * sizeof(UINT64)));
    vector<UINT64> buffer((m_TestSurfSize/sizeof(UINT64)),0xDEADBEEF);

    //For debugging print the test surface
    SurfaceUtils::ReadSurface(m_TestSurf, 0, &buffer[0], m_TestSurfSize, m_SubdevInst);
    UINT32 idx = 0;
    for (UINT32 warp = 0; warp < m_NumWarps; warp++)
    {
        idx = warp * m_ThreadsPerWarp * 2;
        Printf(pri, "Warp:%02d SM%02d: ", warp, (warp / m_NumWarpsPerSM));
        for (UINT32 tid = 0; tid < m_ThreadsPerWarp; tid++)
        {
            Printf(pri, "tid:%d(%08llx/%08llx) ", tid,buffer[idx], buffer[idx+ScratchOffset]);
            idx++;
        }
        Printf(pri,"\n");
    }
    Printf(pri,"\n");
}

//--------------------------------------------------------------------------------------------------
//Command buffer uses 2 entries per warp
// 1st entry: -stores the command
// 2nd entry: -stores the argument
/*
     _____________________
    | Command  | Arg      | Warp 0, SM 0
    |---------------------|
    | Command  | Arg      | Warp 1, SM 0
    |---------------------|
    | Command  | Arg      | Warp 2, SM 1
    |---------------------|
    | Command  | Arg      | Warp 3, SM 1
     ---------------------
    ...
*/
RC EccTexTest::DumpCommandBuffer(Tee::Priority pri)
{
    RC rc;
    vector<UINT32> buffer((m_CmdSurfSize/sizeof(UINT32)),0xDEADBEEF);
    CHECK_RC(SurfaceUtils::ReadSurface(m_CmdSurf, 0, &buffer[0], m_CmdSurfSize, m_SubdevInst));

    Printf(pri,"Warp SM Command   Argument\n");
    for (UINT32 ii = 0; ii < m_NumSMs * m_NumWarpsPerSM * 2; ii += 2)
    {
        Printf(pri,"%02d   %02d 0x%08x 0x%08x\n",
               ii/2, ii/(2*m_NumWarpsPerSM), buffer[ii], buffer[ii+1] );
    }
    return OK;
}

//--------------------------------------------------------------------------------------------------
RC EccTexTest::WriteGlobal(Lwca::Global dst, const void *pSrc, UINT32 numWords)
{
    RC rc;
    MASSERT(pSrc != NULL);
    UINT32 size = numWords * sizeof(UINT32);
    CHECK_RC(dst.InitCheck());

    if (size > dst.GetSize() || size > m_LwdaMemcpyBuffer.GetSize())
    {
        MASSERT(!"Bad size passed to EccTextTest::WriteGlobal");
        return RC::SOFTWARE_ERROR;
    }

    memcpy(m_LwdaMemcpyBuffer.GetPtr(), pSrc, size);
    CHECK_RC(dst.Set(m_LwdaMemcpyStream, m_LwdaMemcpyBuffer.GetPtr(),0, size));
    CHECK_RC(m_LwdaMemcpyStream.Synchronize());

    return rc;
}

//--------------------------------------------------------------------------------------------------
// Args passed to PollErrCounts()
struct PollErrCountsParams
{
    GpuSubdevice *pSubdev;
    UINT64 MinSbeCount;
    UINT64 MinDbeCount;
    UINT64 SbeCount;
    UINT64 DbeCount;
    SubdeviceGraphics::GetTexDetailedEccCountsParams TexParams;
};

//--------------------------------------------------------------------------------------------------
static bool PollForErrCounts(void *pPollForErrCountsParams)
{
    PollErrCountsParams *pParams =
        (PollErrCountsParams*)pPollForErrCountsParams;
    GpuSubdevice *pSubdev = pParams->pSubdev;
    Tasker::MutexHolder Lock(pSubdev->GetEccErrCounter()->GetMutex());
    RC rc;

    SubdeviceGraphics::GetTexDetailedEccCountsParams TexParams;
    Ecc::ErrCounts ZeroData = {0};
    rc = pSubdev->GetSubdeviceGraphics()->GetTexDetailedEccCounts(&TexParams);
    if (rc != OK)
    {
        return true;
    }

    pParams->SbeCount = 0;
    pParams->DbeCount = 0;
    if (pParams->TexParams.eccCounts.size() < TexParams.eccCounts.size())
    {
        pParams->TexParams.eccCounts.resize(TexParams.eccCounts.size());
    }
    for (UINT32 gpc = 0; gpc < TexParams.eccCounts.size(); ++gpc)
    {
        if (pParams->TexParams.eccCounts[gpc].size() < TexParams.eccCounts[gpc].size())
        {
            pParams->TexParams.eccCounts[gpc].resize(TexParams.eccCounts[gpc].size());
        }

        for (UINT32 tpc = 0; tpc < TexParams.eccCounts[gpc].size(); ++tpc)
        {
            // create a reference to cleanup some formatting.
            vector<Ecc::ErrCounts>& tec = pParams->TexParams.eccCounts[gpc][tpc];
            if (tec.size() < TexParams.eccCounts[gpc][tpc].size())
            {
                tec.resize(TexParams.eccCounts[gpc][tpc].size(),ZeroData);
            }

            for (UINT32 tex = 0; tex < TexParams.eccCounts[gpc][tpc].size(); ++tex)
            {
                pParams->SbeCount += TexParams.eccCounts[gpc][tpc][tex].corr;
                pParams->DbeCount += TexParams.eccCounts[gpc][tpc][tex].uncorr;

                tec[tex].corr = TexParams.eccCounts[gpc][tpc][tex].corr;
                tec[tex].uncorr = TexParams.eccCounts[gpc][tpc][tex].uncorr;
            }
        }
    }
    return (pParams->SbeCount >= pParams->MinSbeCount &&
            pParams->DbeCount >= pParams->MinDbeCount);
}

//--------------------------------------------------------------------------------------------------
RC EccTexTest::GetErrCounts
(
    UINT64 *pSbeCount,
    UINT64 *pDbeCount,
    UINT64 MinSbeCount,
    UINT64 MinDbeCount,
    Tee::Priority pri
)
{
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    RC rc;
    PollErrCountsParams Params = {0};
    Params.pSubdev = GetBoundGpuSubdevice();
    Params.MinSbeCount = MinSbeCount;
    Params.MinDbeCount = MinDbeCount;

    POLLWRAP_HW(PollForErrCounts, &Params, pTestConfig->TimeoutMs());
    *pSbeCount = Params.SbeCount;
    *pDbeCount = Params.DbeCount;

    for (UINT32 gpc = 0; gpc < Params.TexParams.eccCounts.size(); ++gpc)
    {
        for (UINT32 tpc = 0; tpc < Params.TexParams.eccCounts[gpc].size(); ++tpc)
        {
            UINT64 correctable = 0;
            UINT64 uncorrectable = 0;
            Printf(pri,"GPC %d TPC %d ",gpc,tpc);
            for (UINT32 tex = 0; tex < Params.TexParams.eccCounts[gpc][tpc].size(); ++tex)
            {
                correctable = Params.TexParams.eccCounts[gpc][tpc][tex].corr;
                uncorrectable = Params.TexParams.eccCounts[gpc][tpc][tex].uncorr;
                Printf(pri,"Tex[%d]CORRECTED:%02llu UNCORRECTED:%02llu ",
                       tex,
                       correctable,
                       uncorrectable);
            }
            Printf(pri,"\n");
        }
    }
    Printf(pri,"\n");

    return rc;
}

//--------------------------------------------------------------------------------------------------
// JavaScript interface
JS_CLASS_INHERIT(EccTexTest, LwdaStreamTest, "EccTexTest object");
CLASS_PROP_READWRITE(EccTexTest, NumSMs, UINT32,
            "Number of SMs to run per launch");
