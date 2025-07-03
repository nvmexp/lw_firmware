/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "turingfalcon.h"

#include "core/include/utility.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputest.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/surf2d.h"

#include "gpu/tests/rm/dmemva/ucode/dmvacommon.h"

class GspSanity : public GpuTest
{
public:
    GspSanity();
    ~GspSanity();

    virtual bool IsSupported() override;
    virtual RC Setup() override;
    virtual RC Cleanup() override;
    virtual RC Run() override;

    struct MemDesc
    {
        Surface2D surf2d;
        UINT32 gpuAddrBlkFb;
        UINT32 *pCpuAddrFb;
    };

    typedef RC (GspSanity::*TestFunc)();
    struct TestCase
    {
        TestFunc func;
        string desc;
        bool isSupported;
        bool requiresHS;
        bool shouldResetAfterTest;
    };

    SETGET_PROP(GspTestMask, UINT64);
    SETGET_PROP(SkipHS, bool);

private:
    RC AllocPhysicalBuffer(MemDesc *pMemDesc, UINT32 size);
    RC RunDmemvaTestSuite(UINT64 testMask);
    RC Reset();

    RC InitTestCases();
    RC GenericTest();
    RC DmlvlExceptionTest();
    RC DmemExceptionTest();
    RC ReadRegsTest();

    static const TestCase m_pTestCases[DMEMVA_NUM_TESTS];

    GpuSubdevice *m_pSubdev      = nullptr;
    unique_ptr<Falcon> m_pFalcon;
    UINT32 m_LwrTestIdx          = 0;
    MemDesc m_DmemDesc           = {};
    MemDesc m_ImemDesc           = {};

    UINT64 m_GspTestMask         = 0;
    bool m_SkipHS                = false;
};

//-----------------------------------------------------------------------------

/* This exports the test to the JS interface */
JS_CLASS_INHERIT(GspSanity, GpuTest, "GspSanity");

CLASS_PROP_READWRITE(GspSanity, GspTestMask, UINT64,
                     "UINT64: Test Mask for Falcon on GSP");

CLASS_PROP_READWRITE(GspSanity, SkipHS, bool,
                     "bool: Skip all tests that require HS falcon");

//-----------------------------------------------------------------------------

GspSanity::GspSanity()
{
}

GspSanity::~GspSanity()
{
}

RC GspSanity::Setup()
{
    RC rc;
    m_pSubdev = GetBoundGpuSubdevice();

    CHECK_RC(GpuTest::Setup());
    CHECK_RC(AllocPhysicalBuffer(&m_DmemDesc, DMA_BUFFER_DMEM_SIZE));
    CHECK_RC(AllocPhysicalBuffer(&m_ImemDesc, DMA_BUFFER_IMEM_SIZE));

    return OK;
}

RC GspSanity::Run()
{
    RC rc;

    if (!m_GspTestMask)
    {
        Printf(Tee::PriWarn, "No tests run\n");
        return OK;
    }

    m_pFalcon = make_unique<TuringGspFalcon>(m_pSubdev);
    m_pFalcon->UCodePatchSignature();

    for (UINT32 i = 0; i < GetTestConfiguration()->Loops(); i++)
    {
        CHECK_RC(RunDmemvaTestSuite(m_GspTestMask));
    }

    return rc;
}

RC GspSanity::Cleanup()
{
    StickyRC rc;

    m_DmemDesc.surf2d.Free();
    m_ImemDesc.surf2d.Free();
    rc = GpuTest::Cleanup();

    return rc;
}

bool GspSanity::IsSupported()
{
    // GSP Sanity is enabled only GV100 onwards
    if (m_GspTestMask && !(GetBoundGpuSubdevice()->DeviceId() >= Gpu::LwDeviceId::GV100))
    {
        VerbosePrintf("GSP Sanity test only supported on GV100 and above\n");
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------------------

RC GspSanity::AllocPhysicalBuffer(MemDesc *pMemDesc, UINT32 size)
{
    MASSERT(pMemDesc);

    RC rc;
    PHYSADDR vidMemPhy;
    Surface2D &pVidMemSurface = pMemDesc->surf2d;

    pVidMemSurface.SetForceSizeAlloc(true);
    pVidMemSurface.SetLayout(Surface2D::Pitch);
    pVidMemSurface.SetAddressModel(Memory::Paging);
    pVidMemSurface.SetColorFormat(ColorUtils::Y8);
    pVidMemSurface.SetLocation(Memory::Fb);
    pVidMemSurface.SetArrayPitchAlignment(256); // DMEM, IMEM have 256 byte blocks
    pVidMemSurface.SetArrayPitch(1);
    pVidMemSurface.SetArraySize(size);

    CHECK_RC(pVidMemSurface.Alloc(GetBoundGpuDevice()));
    CHECK_RC(pVidMemSurface.Map(m_pSubdev->GetSubdeviceInst()));

    pVidMemSurface.GetPhysAddress(0, &vidMemPhy);
    pMemDesc->gpuAddrBlkFb = (UINT32)(vidMemPhy >> 8);
    pMemDesc->pCpuAddrFb = (UINT32 *)pVidMemSurface.GetAddress();

    return rc;
}

RC GspSanity::InitTestCases()
{
    RC rc;

    m_pFalcon->SetMailbox0(DMVA_NULL);
    m_pFalcon->SetMailbox1(0); // 0 = InitTestCases Test ID
    m_pFalcon->Bootstrap(0);

    // Seed for the ucode
    m_pFalcon->UCodeWrite32(0);

    // Send GPU FB addresses to UCode for DMA transfers
    m_pFalcon->UCodeWrite32(m_DmemDesc.gpuAddrBlkFb);
    m_pFalcon->UCodeWrite32(m_ImemDesc.gpuAddrBlkFb);

    // Check return code
    CHECK_RC(m_pFalcon->WaitForStop());
    CHECK_RC(m_pFalcon->GetTestRC());
    return rc;
}

RC GspSanity::RunDmemvaTestSuite(UINT64 testMask)
{
    RC rc;

    CHECK_RC(Reset());
    // Index starts from 1 since Test 0 is InitTestCases
    for (m_LwrTestIdx = 1; m_LwrTestIdx < DMEMVA_NUM_TESTS; m_LwrTestIdx++)
    {
        const TestCase *pLwrTest;
        pLwrTest = &m_pTestCases[m_LwrTestIdx];
        // If test is not supported or,
        // not in the test bitmask or,
        // requires HS when SkipHS is set, don't run it
        if ((!pLwrTest->isSupported) ||
            (!(testMask & ((UINT64)(1) << m_LwrTestIdx))) ||
            (m_SkipHS && pLwrTest->requiresHS))
        {
            continue;
        }

        m_pFalcon->SetMailbox1(m_LwrTestIdx);
        m_pFalcon->ClearInterruptOnHalt();

        rc = ((this->*(pLwrTest->func))());
        if (rc != OK)
        {
            Printf(Tee::PriError, "Test %u failed \n", m_LwrTestIdx);
            return rc;
        }

        if (pLwrTest->shouldResetAfterTest)
        {
            // We can't run any other tests if initialization didn't work
            CHECK_RC(Reset());
        }
    }

    return rc;
}

RC GspSanity::GenericTest()
{
    StickyRC rc;
    UINT32 subTestIndex = 0;

    // Loops across all subtests of the test
    // Breaks when an appropriate RC is set by the falcon
    while (true)
    {
        // Preload with DMVA_NULL the MAILBOX (0, 1) registers which will be used to communicate
        // a return code.
        // If the falcon waits for any reason before setting this register, we will detect it.
        if (subTestIndex > 0)
        {
            m_pFalcon->SetMailbox1(DMVA_NULL);
        }
        m_pFalcon->SetMailbox0(DMVA_NULL);
        m_pFalcon->Resume();

        UINT32 mailbox0;
        while (true)
        {
            CHECK_RC(m_pFalcon->WaitForStop());
            mailbox0 = m_pFalcon->GetMailbox0();
            if (mailbox0 == DMVA_REQUEST_PRIV_RW_FROM_RM)
            {
                m_pFalcon->SetMailbox0(DMVA_NULL);
                m_pFalcon->Resume();
                m_pFalcon->HandleRegRWRequest();
            }
            else
            {
                break;
            }
        }

        // DMVA_FORWARD_PROGRESS is received whenever a subtest is marked as started
        if (mailbox0 != DMVA_FORWARD_PROGRESS)
        {
            rc = m_pFalcon->GetTestRC();
            if (subTestIndex > 0 && rc != OK)
            {
                Printf(Tee::PriError, "Subtest %u of Test %u failed\n",
                                       subTestIndex, m_LwrTestIdx);
            }
            break;
        }
        UINT32 falcTestIdx = m_pFalcon->GetMailbox1();
        if (falcTestIdx != subTestIndex)
        {
            Printf(Tee::PriError, "Subtest %3u: Corrupted subtest index."
                                  "Expected %u, but got %u.\n",
                                   subTestIndex, subTestIndex, falcTestIdx);
            return RC::DATA_MISMATCH;
        }

        subTestIndex++;
    }

    return rc;
}

RC GspSanity::DmlvlExceptionTest()
{
    StickyRC rc;

    UINT32 nSubTests = 1;
    for (UINT32 subTestIndex = 0; subTestIndex < nSubTests; subTestIndex++)
    {
        // Start the test, with the proper testIndex
        Reset();
        m_pFalcon->SetMailbox0(DMVA_NULL);
        m_pFalcon->SetMailbox1(m_LwrTestIdx);
        m_pFalcon->Resume();

        m_pFalcon->UCodeWrite32(subTestIndex);
        nSubTests = m_pFalcon->UCodeRead32();

        CHECK_RC(m_pFalcon->VerifyDmlvlExceptionAccess());
        CHECK_RC(m_pFalcon->GetTestRC());  // We must pass ucode's tests as well
    }
    return rc;
}

RC GspSanity::DmemExceptionTest()
{
    RC rc;

    UINT32 nSubTests = 1;
    for (UINT32 subTestIndex = 0; subTestIndex < nSubTests; subTestIndex++)
    {
        // Start the test, with the proper testIndex
        Reset();
        m_pFalcon->SetMailbox0(DMVA_NULL);
        m_pFalcon->SetMailbox1(m_LwrTestIdx);
        m_pFalcon->Resume();

        nSubTests = m_pFalcon->UCodeRead32();
        m_pFalcon->UCodeWrite32(subTestIndex);

        // Verify
        CHECK_RC(m_pFalcon->WaitForHalt());
        CHECK_RC(m_pFalcon->GetTestRC());
    }
    return rc;
}

RC GspSanity::ReadRegsTest(void)
{
    RC rc;

    m_pFalcon->Resume();
    CHECK_RC(m_pFalcon->WaitForStop());

    UINT32 mailbox0 = m_pFalcon->GetMailbox0();
    UINT32 mailbox1 = m_pFalcon->GetMailbox1();

    if (mailbox0 != REG_TEST_MAILBOX0 || mailbox1 != REG_TEST_MAILBOX1)
    {
        Printf(Tee::PriError, "Expected (MAILBOX0, MAILBOX1) to be (%08x, %08x), "
                              "but got (%08x, %08x)\n",
                               REG_TEST_MAILBOX0, REG_TEST_MAILBOX1, mailbox0, mailbox1);
        return RC::REGISTER_READ_WRITE_FAILED;
    }
    return rc;
}

RC GspSanity::Reset()
{
    RC rc;

    // Reset the engine and controls
    CHECK_RC(m_pFalcon->Reset());

    // Load IMEM and DMEM
    m_pFalcon->ImemAccess(0, m_pFalcon->GetCode(), m_pFalcon->GetCodeSize(), DMEMWR, 0, 0);
    m_pFalcon->DmemAccess(0, m_pFalcon->GetData(), m_pFalcon->GetDataSize(), DMEMWR, 0);
    m_pFalcon->ImemAccess(m_pFalcon->GetCodeSize(),
                          m_pFalcon->GetAppCode(0),
                          m_pFalcon->GetAppCodeSize(0),
                          DMEMWR,
                          m_pFalcon->GetCodeSize() >> 8,
                          0);
    m_pFalcon->DmemAccess(m_pFalcon->GetDataSize(),
                          m_pFalcon->GetAppData(0),
                          m_pFalcon->GetAppDataSize(0),
                          DMEMWR,
                          m_pFalcon->GetDataSize() >> 8);

    // Store the entire IMEM image in FB
    UINT32 codeSize = m_pFalcon->GetCodeSize();
    for (UINT32 i = 0; i < m_pFalcon->GetNumApps(); i++)
    {
        codeSize += m_pFalcon->GetAppCodeSize(i);
    }
    for (UINT32 codeIdx = 0; codeIdx < codeSize; codeIdx += 4)
    {
        MEM_WR32(m_ImemDesc.pCpuAddrFb + codeIdx / sizeof(UINT32),
                 m_pFalcon->GetCode()[codeIdx / sizeof(UINT32)]);
    }
    m_ImemDesc.surf2d.FlushCpuCache(Surface2D::Flush);

    CHECK_RC(InitTestCases());

    return rc;
}

const GspSanity::TestCase GspSanity::m_pTestCases[DMEMVA_NUM_TESTS] =
{
    // test function                 description                               en     HS     reset     ID
    { &GspSanity::InitTestCases, "initialize tests.                         ", true,  false, false }, //00
    { &GspSanity::ReadRegsTest,  "read registers.                           ", true,  false, false }, //01
    { &GspSanity::GenericTest,   "blocks are invalid after reset.           ", true,  false, false }, //02
    { &GspSanity::GenericTest,   "dmem instructions: read/write to PA/VA.   ", true,  false, false }, //03
    { &GspSanity::GenericTest,   "dmblk: can read block status.             ", true,  true,  false }, //04
    { &GspSanity::GenericTest,   "write sets dirty bit.                     ", true,  false, false }, //05
    { &GspSanity::GenericTest,   "dmtag: can read block status.             ", true,  true,  false }, //06
    { &GspSanity::GenericTest,   "DMVACTL.bound.                            ", true,  false, false }, //07
    { &GspSanity::GenericTest,   "EMEM.                                     ", true,  false, false }, //08
    { &GspSanity::GenericTest,   "CMEM.                                     ", true,  false, false }, //09
    { &GspSanity::GenericTest,   "DMEMC: write, va=0, settag=0, valid, clean", true,  false, false }, //10
    { &GspSanity::GenericTest,   "DMEMC: write, va=0, settag=0, invalid.    ", true,  false, false }, //11
    { &GspSanity::GenericTest,   "DMEMC: write, va=0, settag=1, any status. ", true,  false, false }, //12
    { &GspSanity::GenericTest,   "DMEMC: write, va=1, settag=0, any status. ", true,  false, false }, //13
    { &GspSanity::GenericTest,   "DMEMC: write, va=1, settag=1, any status. ", true,  false, false }, //14
    { &GspSanity::GenericTest,   "DMEMC: read, va=0, settag=x, any status.  ", true,  false, false }, //15
    { &GspSanity::GenericTest,   "DMEMC: read, va=1, settag=x, any status.  ", true,  false, false }, //16
    { &GspSanity::GenericTest,   "DMA: settag=0.                            ", true,  false, false }, //17
    { &GspSanity::GenericTest,   "DMA: settag=1.                            ", true,  false, false }, //18
    { &GspSanity::GenericTest,   "HS: bootrom with physical signature/stack.", true,  true,  false }, //19
    { &GspSanity::GenericTest,   "HS: bootrom with virtual signature/stack. ", true,  true,  false }, //20
    { &GspSanity::GenericTest,   "carveouts.                                ", false, true,  false }, //21
    { &GspSanity::GenericTest,   "dmlvl: add/remove permissions from block. ", true,  true,  false }, //22
    { &GspSanity::GenericTest,   "DMA: add/remove permissions from block.   ", true,  true,  false }, //23
    { &GspSanity::DmlvlExceptionTest, "Exception on inselwre ucode access.  ", true,  true,  true  }, //24
    { &GspSanity::GenericTest,   "DMATRF: security test.                    ", true,  true,  false }, //25
    { &GspSanity::GenericTest,   "DMEMC: security test.                     ", true,  true,  false }, //26
    { &GspSanity::GenericTest,   "SCP: read/write using PA/VA, (un)mapped.  ", true,  false, false }, //27
    { &GspSanity::DmemExceptionTest,  "dmem instructions: dmem exceptions.  ", true,  false, true  }, //28
    { &GspSanity::GenericTest,   "dmilw: block status becomes invalid.      ", true,  false, false }, //29
    { &GspSanity::GenericTest,   "dmclean: block status becomes clean.      ", true,  false, false }, //30
    { &GspSanity::GenericTest,   "setdtag: block status becomes valid&clean.", true,  false, false }, //31
    // DMA tagging tests are disabled
    { &GspSanity::GenericTest,   "DMA tagging: interrupts and polling.      ", false, false, false }, //32
};
