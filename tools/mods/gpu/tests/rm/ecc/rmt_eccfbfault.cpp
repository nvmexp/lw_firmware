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

//
// Test to inject errors to test the ECC scrubbing feature.
//

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff
#include "gpu/tests/gpumtest.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surfrdwr.h"
#include "ctrl/ctrl2080.h"
#include "class/cl0005.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/gpuutils.h"
#include "core/include/utility.h"
#include "gpu/utility/ecccount.h"
#include "core/include/platform.h"
#include "sim/IChip.h"
#include "gpu/utility/bglogger.h"
#include "core/include/memcheck.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"

#define TEST_BLOCK_HEIGHT 4
#define TEST_BLOCK_PITCH 4096
#define NUM_BLOCKS_TO_TEST 20
#define NUM_BLOCKS_TO_TEST_IN_SIM 3
#define TIME_OUT 500 // arbitrarily chosen 'long' wait time in milli-seconds

typedef struct
{
    bool eccEventOclwrred;; //input parameter
    LwU32 numCallbacks;
} EccCallbackParams;

static EccCallbackParams eccCallbackParams;
static LW208F_CTRL_CMD_FB_ECC_ERROR_INFO lwrrentErrorInfo[TEST_BLOCK_HEIGHT];
static void EccCallback(void *parg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status);

class EccFbFaultInjectionTest : public RmTest
{
public:
    EccFbFaultInjectionTest();
    virtual ~EccFbFaultInjectionTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    // Command line arguments.
    SETGET_PROP(ForceWriteKillAll, bool);   //!< Grab JS variable.
    SETGET_PROP(ForceWriteKillAddr, bool);  //!< Grab JS variable.

    // main surface for the test in FB
    Surface2D *pVidMemSurface;
    Surface2D *pVidMemSurface1;

private:
    LwRm::Handle m_eccSbeEvent;
    RC AllocTestData();
    RC Flush();
    RC TestBlock(LwU32 blockNum);
    RC ReadBack();
    LwU32 numBlocksToTest;

    LWOS10_EVENT_KERNEL_CALLBACK_EX eccSbeCallback;
    LwU32 m_IntrWaitTimeMs;
    LwU32 m_SectorWidth;
    LwU32 m_SurfaceRowPitch;
    LwU32 m_ErrorSeedInBlock[TEST_BLOCK_HEIGHT];
    LwU32 numBlocks;
    LwU32 numRows;
    LwU32 numCols;
    LwU64 *pBlock;
    UINT32 m_SavedFbBackdoor;     //!< To restore BACKDOOR_ACCESS in cleanup
    UINT32 m_SavedHostFbBackdoor; //!< To restore BACKDOOR_ACCESS in cleanup
    bool m_RestoreBackdoorInCleanup; //!< m_Saved*Backdoor has valid data
    StickyRC m_ErrorRc;
    LwU64 errOffset[TEST_BLOCK_HEIGHT];
    LwU64 surfaceErrOffset[TEST_BLOCK_HEIGHT];
    LwU32 injectByte[TEST_BLOCK_HEIGHT];
    Random* pRandom;
    LW208F_CTRL_CMD_FB_ECC_ERROR_INFO injectedErrors[TEST_BLOCK_HEIGHT];
    bool m_UseWriteKillPtrs;      //!< For HBM
    bool m_ForceWriteKillAll;
    bool m_ForceWriteKillAddr;
};

//! \brief Constructor
//------------------------------------------------------------------------------
EccFbFaultInjectionTest::EccFbFaultInjectionTest() :
    m_IntrWaitTimeMs(TIME_OUT),
    m_SavedFbBackdoor(0),
    m_SavedHostFbBackdoor(0),
    m_RestoreBackdoorInCleanup(false),
    m_UseWriteKillPtrs(false),
    m_ForceWriteKillAll(false),
    m_ForceWriteKillAddr(false)
{
    memset(&eccCallbackParams, 0, sizeof(eccCallbackParams));
    memset(&eccSbeCallback, 0, sizeof(eccSbeCallback));
    memset(&injectedErrors, 0, sizeof(injectedErrors));
    numBlocksToTest = NUM_BLOCKS_TO_TEST;
    SetDisableWatchdog(true);
    SetName("EccFbFaultInjectionTest");
}

//! \brief Destructor
//------------------------------------------------------------------------------
EccFbFaultInjectionTest::~EccFbFaultInjectionTest()
{

}

//! \brief Test needs Ecc enabled and EccErrCounter initialized
//------------------------------------------------------------------------------
string EccFbFaultInjectionTest::IsTestSupported()
{
    LwRm *pLwRm = GetBoundRmClient();
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    LW2080_CTRL_GPU_QUERY_ECC_STATUS_PARAMS EccStatus;

    // check that ECC is enabled on the current device
    memset(&EccStatus, 0, sizeof(EccStatus));
    if (OK != pLwRm->ControlBySubdevice(pGpuSubdevice,
                                        LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS,
                                        &EccStatus, sizeof(EccStatus)))
    {
        return "ECC not Enabled";
    }

    if ((Platform::Hardware != Platform::GetSimulationMode()) ||
       pGpuSubdevice->IsEmulation())
    {
        numBlocksToTest = NUM_BLOCKS_TO_TEST_IN_SIM;
    }

    // check that ECC is enabled
    if ((EccStatus.units[LW2080_CTRL_GPU_ECC_UNIT_FBPA].enabled) &&
          (pGpuSubdevice->GetEccErrCounter()->IsInitialized()) &&
          (pGpuSubdevice->GetFB()->GetFbEccSectorSize() > 0))
              return RUN_RMTEST_TRUE;

    else
        return "Test Not Supported";
}

//--------------------------------------------------------------------
//! \brief Set up the test
//!
//! Note: Function will setup up the FB surface where errors will be injected.
//! Assumption is that FB is already scrubbed during devinit/allocation of surface
//!
RC EccFbFaultInjectionTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    LW0005_ALLOC_PARAMETERS allocSbeParams = {0};
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventSbeParams = {0};
    LwU32 i;

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

    //step 4c: register to listen to ECC SBE notifier
    eventSbeParams.event = LW2080_NOTIFIERS_ECC_SBE;
    eventSbeParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &eventSbeParams, sizeof(eventSbeParams)));

    // get the basic parameters of the test
    m_SectorWidth = pGpuSubdevice->GetFB()->GetFbEccSectorSize();
    m_SurfaceRowPitch = TEST_BLOCK_PITCH;
    pRandom = &GetFpContext()->Rand;

    //
    // Setup random offsets into each ECC sector that we're going to inject SBE
    // Note: each ECC sector is 32 bytes. We get a random number, and mod it by
    // 8 to get which byte 'index' we want. We multiply the 'index' by 4 to colwert
    // into which byte offset we want.
    //
    pRandom->SeedRandom(278741,278743,278753,278767);
    for ( i = 0; i < TEST_BLOCK_HEIGHT; i++)
        m_ErrorSeedInBlock[i] = (pRandom->GetRandom() % 8) * 4;

    CHECK_RC(InitFromJs());

    CHECK_RC(AllocTestData());

    numBlocks = ((LwU32)pVidMemSurface->GetSize()) / (m_SectorWidth * TEST_BLOCK_HEIGHT);
    numRows   = ((LwU32)pVidMemSurface->GetSize()) / (TEST_BLOCK_HEIGHT * m_SurfaceRowPitch);
    numCols   = m_SurfaceRowPitch/m_SectorWidth;
    LwU32 blockNum  = 0;
    pBlock = new LwU64[numBlocks];

    //
    // initialize pBlock, the array of pointers to the blocks we're going to
    // inject SBE to
    //
    for (LwU32 row = 0; row < numRows; row++)
    {
        for (LwU32 col = 0; col < numCols; col++)
        {
            pBlock[blockNum++] = pVidMemSurface->GetVidMemOffset() +
                                 (row * m_SurfaceRowPitch * TEST_BLOCK_HEIGHT) +
                                 (col * m_SectorWidth);

        }
    }

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

    CHECK_RC(ErrorLogger::StartingTest());
    if (pGpuSubdevice->GetFB()->GetRamProtocol() == FrameBuffer::RamHBM1 ||
        pGpuSubdevice->GetFB()->GetRamProtocol() == FrameBuffer::RamHBM2)
    {
        m_UseWriteKillPtrs = true;
    }

    m_ErrorRc.Clear();

    return rc;
}

//! \brief Run the test for every memory block.
//------------------------------------------------------------------------------
RC EccFbFaultInjectionTest::Run()
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    EccErrCounter *pEccErrCounter = pGpuSubdevice->GetEccErrCounter();
    BgLogger::PauseBgLogger DisableBgLogger;
    vector<UINT08> errbuf(4, 0xaa);

    //
    // Tell RM that we're expecting ECC SBE and DBE interrupts on FB and that
    // they should therefore be non-fatal
    //
    Tasker::Sleep(m_IntrWaitTimeMs);
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::FB_CORR));
    CHECK_RC(pEccErrCounter->BeginExpectingErrors(EccErrCounter::FB_UNCORR));
    Utility::CleanupOnReturn<EccErrCounter, RC>
        ExpectingErrors(pEccErrCounter, &EccErrCounter::EndExpectingAllErrors);

    //
    // test each ECC sector
    // note: ECC divides FB into 32 bytes sectors. all bit errors oclwring in the
    // same ECC sector trigger a shared interrupt for that sector.
    //
    LwU32 numTestBlocks = numBlocks/numBlocksToTest;
    for (LwU32 blockNum = 0; blockNum < numBlocks;)
    {
        CHECK_RC(TestBlock(blockNum));
        CHECK_RC(ReadBack());
        blockNum += numTestBlocks;
    }

    // we're done testing, tell RM that we're no longer expecting ECC errors on FB
    Tasker::Sleep(m_IntrWaitTimeMs);
    ExpectingErrors.Cancel();
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::FB_CORR));
    CHECK_RC(pEccErrCounter->EndExpectingErrors(EccErrCounter::FB_UNCORR));

    if ((rc == RC::ECC_CORRECTABLE_ERROR) ||
        (rc == RC::SM_ECC_CORRECTABLE_ERROR) ||
        (rc == RC::L2_ECC_CORRECTABLE_ERROR) ||
        (rc == RC::FB_ECC_CORRECTABLE_ERROR) ||
        (rc == OK))
        return OK;
    else
        return rc;
}

//! \brief Cleanup
//------------------------------------------------------------------------------
RC EccFbFaultInjectionTest::Cleanup()
{

    ErrorLogger::TestCompleted();

    if (m_RestoreBackdoorInCleanup)
    {
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4,
                              m_SavedFbBackdoor);
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_HOSTFB, 4,
                              m_SavedHostFbBackdoor);
        m_RestoreBackdoorInCleanup = false;
    }

    if (pBlock)
        delete [] pBlock;

    if (pVidMemSurface)
        delete pVidMemSurface;

    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief Allocate one large contiguous surface
//------------------------------------------------------------------------------
RC EccFbFaultInjectionTest::AllocTestData()
{
    // Allocate a single 32k pitch surface
    pVidMemSurface = new Surface2D;
    pVidMemSurface->SetForceSizeAlloc(1);
    pVidMemSurface->SetPhysContig(1);
    pVidMemSurface->SetLayout(Surface2D::Pitch);
    pVidMemSurface->SetLocation(Memory::Fb);
    pVidMemSurface->SetColorFormat(ColorUtils::Y8);
    pVidMemSurface->SetArrayPitch(32768);
    pVidMemSurface->Alloc(GetBoundGpuDevice());
    pVidMemSurface->Map();

    Printf(Tee::PriLow,
        "EccFbFaultTest: Height %d Pitch %d Arraypitch %d Width %d total size %d:\n"
        , pVidMemSurface->GetAllocHeight()
        , pVidMemSurface->GetPitch()
        , (LwU32)pVidMemSurface->GetArrayPitch()
        , pVidMemSurface->GetWidth()
        , (LwU32)pVidMemSurface->GetSize());

    Printf(Tee::PriLow,
        "EccFbFaultTest: VidmemOffset %llu VA %p PA %llx\n", pVidMemSurface->GetVidMemOffset()
        , pVidMemSurface->GetAddress()
        , pVidMemSurface->GetPhysAddress());
    return OK;
}

//! Flush all PB and also a vidmembar
//--------------------------------------------------------------------
RC EccFbFaultInjectionTest::Flush()
{
    // Flush WC buffers on the CPU
    RC rc;
    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());

    // Force all BAR1 writes to L2 and to FB to complete
    LW0080_CTRL_DMA_FLUSH_PARAMS Params = {0};

    Params.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
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
    return rc;
}

//! For every Block of (32x8) bytes this function will
//! 1) Fill up the Box with some fixed data.
//! 2) Flush the filled data from L2 to FB
//! 3) Ilwalidate L2 Cache
//! 4) Using Erroseed array choose 8 bytes in block to inject error and save
//! 5) Take the fboffset of these bytes and get the corresponding RBC
//! 5) Set Write kill using the RBC so that we can inject faults
//! 6) Mask one bit and write the value to FB for all 8 fault points
//! 7) FLush and Ilwalidate cache
//! 8) Restore the write kill back to original
//--------------------------------------------------------------------
RC EccFbFaultInjectionTest::TestBlock(LwU32 blockNum)
{
    RC rc;
    LwRm *pLwRm = GetBoundRmClient();
    LwU32 pagesize, row, col;
    LwU64 surfaceOffset = 0;
    LwU64 blockStart;
    LwU32 fill;
    LwU32 subdevInst = GetBoundGpuSubdevice()->GetSubdeviceInst();
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    FrameBuffer *pFb = pGpuSubdevice->GetFB();
    SurfaceUtils::MappingSurfaceWriter writer(*pVidMemSurface, subdevInst);
    SurfaceUtils::MappingSurfaceReader reader(*pVidMemSurface, subdevInst);
    LwRm::ExclusiveLockRm lockRm(pLwRm);
    lockRm.AcquireLock();
    LwU32 newData;
    unique_ptr<FrameBuffer::CheckbitsHolder> checkbitsHolder(
            pFb->SaveFbEccCheckbits());

    pagesize = pVidMemSurface->GetPageSize();
    if (pagesize == 0)
        pagesize = pVidMemSurface->GetActualPageSizeKB();

    Printf(Tee::PriLow, "EccFbFaultTest: PteKind %d, PageSize %d \n",
           pVidMemSurface->GetPteKind(), pagesize);

    // Fill the block of 32 bytes x 8 bytes with data
    blockStart = pBlock[blockNum];
    surfaceOffset = blockStart - pVidMemSurface->GetVidMemOffset();
    LwU64 offset = surfaceOffset;

    for (row = 0; row < TEST_BLOCK_HEIGHT; row ++)
    {
        for (col = 0; col < (m_SectorWidth >> 2); col++)
        {
            fill = col;
            CHECK_RC(writer.WriteBytes(offset, &fill, sizeof(LwU32)));
            offset = surfaceOffset + (row * m_SurfaceRowPitch) + (col * sizeof(LwU32));
        }
    }

    // read the data and offsets with the error seeds and store them (for 8 regions in the block)
    CHECK_RC(Flush());
    for (row = 0; row < TEST_BLOCK_HEIGHT; row++)
    {
        errOffset[row] = pBlock[blockNum] + (row * m_SurfaceRowPitch) + m_ErrorSeedInBlock[row];
        surfaceErrOffset[row] = errOffset[row] - pVidMemSurface->GetVidMemOffset();
        CHECK_RC(reader.ReadBytes(surfaceErrOffset[row], &injectByte[row], sizeof(injectByte[row])));
        Printf(Tee::PriLow,
               "EccFbFaultTest: errOffset and Data Read 0x%llx and 0x%x\n",
               errOffset[row], injectByte[row]);
    }

    CHECK_RC(Flush());
    Utility::SleepUS(1000000);

    // Enable write kill and insert the error for each saved offset
    for (row = 0; row < TEST_BLOCK_HEIGHT; row++)
    {
        LW208F_CTRL_CMD_FB_ECC_GET_FORWARD_MAP_ADDRESS_PARAMS addrParams = {0};
        CHECK_RC(pLwRm->Control(pGpuSubdevice->GetSubdeviceDiag(),
                                LW208F_CTRL_CMD_FB_ECC_GET_FORWARD_MAP_ADDRESS,
                                &addrParams, sizeof(addrParams)));
        Printf(Tee::PriNormal,
               "EccFbFaultTest: FB address = 0x%llx\n", errOffset[row]);
        Printf(Tee::PriNormal,
               "EccFbFaultTest: Setting Checkbits on Address:"
               " SliceNum = 0x%x, Partition = 0x%x\n",
               0, addrParams.partition);
        Printf(Tee::PriNormal,
               "EccFbFaultTest: Sublocation = %d, Rank = %d,"
               " Bank = %d, Row = %d, Column = 0x%x\n",
               addrParams.sublocation, addrParams.extBank,
               addrParams.bank, addrParams.row, addrParams.col);

        // HBM (high bandwidth memory) introduced write kill pointers
        // as ECC changed from sideband (accessed and stored along with memory)
        // to inline (accessed in parallel and stored in its own memory)
        // Setting the boolean will utilize this method of injection
        if (m_UseWriteKillPtrs)
        {
            // get the column granularity for the injection register
            UINT32 columnGran = pFb->GetFbEccSectorSize() / 4;

            // sets the write kill at a 32 byte aligned location
            // use the write kill pointer to specify the location
            // within the 32 byte alignment
            const UINT32 kptr = (addrParams.col % columnGran) << 5;
            CHECK_RC(pFb->ApplyFbEccWriteKillPtr(
                            errOffset[row], //FbOffset
                            pVidMemSurface->GetPteKind(),
                            pagesize,
                            kptr));
            injectedErrors[row].row = addrParams.row;
            injectedErrors[row].bank = addrParams.bank;
            injectedErrors[row].col = addrParams.col;

            CHECK_RC(writer.WriteBytes(surfaceErrOffset[row],
                        &injectByte[row], sizeof(injectByte[row])));
        }
        else
        {
            FbDecode Decode;
            CHECK_RC(pFb->DecodeAddress(&Decode, errOffset[row],
                                        pVidMemSurface->GetPteKind(),
                                        pagesize));
            if (m_ForceWriteKillAll)
            {
                CHECK_RC(pFb->DisableFbEccCheckbits());
            }
            else if (m_ForceWriteKillAddr)
            {
                CHECK_RC(pFb->DisableFbEccCheckbitsAtAddress(errOffset[row],
                                pVidMemSurface->GetPteKind(), pagesize));
            }
            else
            {
                // Use WriteKill mode ADDR by default.
                CHECK_RC(pFb->DisableFbEccCheckbitsAtAddress(errOffset[row],
                                pVidMemSurface->GetPteKind(), pagesize));
            }

            injectedErrors[row].row = Decode.row;
            injectedErrors[row].bank = Decode.bank;
            injectedErrors[row].col = pFb->GetRowOffset(Decode.burst,
                                                        Decode.beat,
                                                        Decode.beatOffset) >> 2;
            LwU32 error =  (injectByte[row] ^ (1 << row));
            CHECK_RC(writer.WriteBytes(surfaceErrOffset[row],
                        &error,
                        sizeof(error)));
        }
        CHECK_RC(reader.ReadBytes(surfaceErrOffset[row],
                    &newData,
                    sizeof(newData)));
        CHECK_RC(Flush());
        Utility::SleepUS(1000000);
    }
    CHECK_RC(pGpuSubdevice->IlwalidateL2Cache(0));
    Utility::SleepUS(1000000);
    return OK;
}

//! Read back to generate ECC errors and test scrubbing
//! 1) Read back each of the 8 bytes where we injected the fault.
//! 2) The errors are generated and RM will scrub each of these errors
//! 3) Snapshot the count value after reading all 8.
//! 4) Ilwalidate L2 cache
//! 5) Read the same 8 bytes again.
//! 6) Compare the new count value to the old one to ensure that they are same
//! 7) If the values are same scrubbing has worked, if second count is
//!    greater then scrubbing failed.
//--------------------------------------------------------------------
RC EccFbFaultInjectionTest::ReadBack()
{
    RC rc;
    LwU32 row;
    EccErrCounter *pEccErrCounter = GetBoundGpuSubdevice()->GetEccErrCounter();
    LwU32 subdevInst = GetBoundGpuSubdevice()->GetSubdeviceInst();
    LwU64 sbeCountFirstPass;
    LwU64 sbeCountSecondPass;
    LwU64 dbeCount;
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    SurfaceUtils::MappingSurfaceReader reader(*pVidMemSurface, subdevInst);

    Utility::SleepUS(1000000);
    // Read the offsets where we had injected the error to cause ECC interrupts
    for (row = 0; row < TEST_BLOCK_HEIGHT; row++)
    {
        LwU32 newData;
        CHECK_RC(reader.ReadBytes(surfaceErrOffset[row],
                                  &newData,
                                  sizeof(newData)));
        if (newData != injectByte[row])
        {
            Printf(Tee::PriWarn,
                   "EccFbFaultTest: Fb ecc did not work %d != %d\n",
                   (int) newData, (int) injectByte[row]);
        }
        Printf(Tee::PriNormal, "EccFbFaultTest: New data %d\n", newData);

        //
        // we need to give some time for the ECC interrupt to be triggered and
        // for RM to service it.
        //
        Utility::SleepUS(1000000);
     }

    // record the number of SBE in preparation of our next step
    Tasker::Sleep(m_IntrWaitTimeMs);
    CHECK_RC(pEccErrCounter->Update());
    sbeCountFirstPass = pEccErrCounter->GetTotalErrors(EccErrCounter::FB_CORR);

    //
    // check that SBE's were corrected by ECC. we do this by re-reading locations
    // that we've injected errors, thereby triggering SBE errors that should've
    // been corrected by ECC. Note that
    //
    CHECK_RC(pGpuSubdevice->IlwalidateL2Cache(0));
    for (row = 0; row < TEST_BLOCK_HEIGHT; row++)
    {
        LwU32 newData;
        CHECK_RC(reader.ReadBytes(surfaceErrOffset[row],
                                  &newData,
                                  sizeof(newData)));
        if (newData != injectByte[row])
        {
            Printf(Tee::PriWarn, "EccFbFaultTest: Bad Data %d != %d\n",
                    (int) newData, (int) injectByte[row]);
        }
        Utility::SleepUS(100);
    }

    // record the number of SBE and DBE's
    CHECK_RC(pEccErrCounter->Update());
    sbeCountSecondPass = pEccErrCounter->GetTotalErrors(EccErrCounter::FB_CORR);
    dbeCount = pEccErrCounter->GetTotalErrors(EccErrCounter::FB_UNCORR);

    //
    // verify that # of SBE's hasn't risen and that no DBE's were ever found.
    // the number of SBE's shouldn't have risen since the first read of all the
    // locations we inject SBE's should've caused ECC to correct the SBE's. There
    // should be no DBE's since we never intentionally injected any.
    //
    Printf(Tee::PriNormal,
           "EccFbFaultTest: Initially= %llu after scrubbing = %llu\n",
           sbeCountFirstPass, sbeCountSecondPass);

    if (dbeCount != 0)
    {
        Printf(Tee::PriError, "EccFbFaultTest: Unexpected DBE oclwred!!!\n");
        return RC::BAD_MEMORY;
    }

    if (eccCallbackParams.numCallbacks > TEST_BLOCK_HEIGHT)
    {
        return RC::BAD_MEMORY;
    }

    for (row = 0; row < TEST_BLOCK_HEIGHT; row++)
    {
        if (IsPASCALorBetter(pGpuSubdevice->DeviceId()))
        {
            if ( (errOffset[row] != lwrrentErrorInfo[row].physAddress) )
            {
                Printf(Tee::PriError,
                       "EccFbFaultTest: Physical Address from MODS"
                       " 0x%x != Physical Address from RM = 0x%x\n",
                       (int) errOffset[row],
                       (int) lwrrentErrorInfo[row].physAddress);
                return RC::BAD_MEMORY;
            }
        }
        else
        {
            // Prior to Pascal, RBC was used to compare injection and
            // reported error addresses.
            if ( (injectedErrors[row].row != lwrrentErrorInfo[row].row)
                    ||
                 (injectedErrors[row].bank != lwrrentErrorInfo[row].bank)
                    ||
                 ((injectedErrors[row].col << 1 != lwrrentErrorInfo[row].col) &&
                  (injectedErrors[row].col << 2 != lwrrentErrorInfo[row].col)) )
            {
                Printf(Tee::PriError, "EccFbFaultTest:"
                       " ERROR ROW from mods != 0x%x ROW from RM = 0x%x\n",
                       (int) injectedErrors[row].row,
                       (int) lwrrentErrorInfo[row].row);
                Printf(Tee::PriError, "EccFbFaultTest:"
                       " ERROR BANK from mods != 0x%x BANK from RM = 0x%x\n",
                       (int) injectedErrors[row].bank,
                       (int) lwrrentErrorInfo[row].bank);
                Printf(Tee::PriError, "EccFbFaultTest:"
                       " ERROR COL from mods != 0x%x COL from RM = 0x%x\n",
                       (int) injectedErrors[row].col,
                       (int) lwrrentErrorInfo[row].col);
                return RC::BAD_MEMORY;
            }
        }
    }

    eccCallbackParams.numCallbacks = 0;

    if ((sbeCountFirstPass == sbeCountSecondPass) && (sbeCountFirstPass != 0))
    {
        Printf(Tee::PriNormal, "EccFbFaultTest: Scrubbing Passed\n");
        return OK;
    }
    else
    {
        return RC::BAD_MEMORY;
    }
}

static void EccCallback(void *parg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{
    EccCallbackParams* params = (EccCallbackParams*)parg;
    LW208F_CTRL_CMD_FB_ECC_ERROR_INFO *errorInfo = (LW208F_CTRL_CMD_FB_ECC_ERROR_INFO *) pData;

    Printf(Tee::PriNormal,
        "injected error from HW: row 0x%x bank 0x%x col 0x%x and xbar 0x%x\n",
        (int) errorInfo->row, (int) errorInfo->bank,
        (int) errorInfo->col, (int) errorInfo->xbarAddress);

    params->eccEventOclwrred = true;

    if ((params->numCallbacks > TEST_BLOCK_HEIGHT) || (!errorInfo))
    {
        return;
    }
    lwrrentErrorInfo[params->numCallbacks] = *errorInfo;
    params->numCallbacks++;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------
//! \brief Linkage to JavaScript
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(EccFbFaultInjectionTest, RmTest,
    "This Test will generate ECC single bit faults to test scrubbing");
CLASS_PROP_READWRITE(EccFbFaultInjectionTest, ForceWriteKillAddr, bool, "Set the WriteKill mode to ADDR");
CLASS_PROP_READWRITE(EccFbFaultInjectionTest, ForceWriteKillAll, bool, "Set the WriteKill mode to ALL");
