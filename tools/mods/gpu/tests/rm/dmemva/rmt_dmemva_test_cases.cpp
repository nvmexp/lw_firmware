/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <stdarg.h>

#include "gpu/tests/rmtest.h"
#include "core/include/gpu.h"
#include "core/include/tee.h"
#include "core/include/rc.h"
#include "core/include/utility.h"
#include "gpu/tests/gputest.h"
#include "core/include/lwrm.h"
#include "gpu/utility/surf2d.h"
#include "core/include/xp.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"

#include "rmt_dmemva.h"
#include "rmt_dmemva_test_cases.h"

#include "turing/tu102/dev_falcon_v4.h"
#include "turing/tu102/dev_master.h"
#include "turing/tu102/dev_fuse.h"

#include "gpu/tests/rm/dmemva/ucode/dmvacommon.h"

static LwU32 nTestsRun = 0;
static LwU32 nTestsFailed = 0;
static LwU32 nSubtestsRun = 0;
static LwU32 nSubtestsFailed = 0;

typedef struct
{
    Surface2D surf2d;
    LwU32 gpuAddrBlkFb;
    LwU32 *pCpuAddrFb;
} MemDesc;

static MemDesc g_dmemDesc;
static MemDesc g_imemDesc;

const FalconInfo *FalconInfo::getFalconInfo(GpuSubdevice *pSubdev, enum FALCON_INSTANCE instance)
{
    if ((pSubdev->DeviceId() >= Gpu::TU102))
        return getTuringFalconInfo(instance);
    else if ((pSubdev->DeviceId() >= Gpu::GV100))
        return getVoltaFalconInfo(instance);
    else if ((pSubdev->DeviceId() >= Gpu::GP100))
        return getPascalFalconInfo(instance);
    else
        return NULL;
}

static LwU64 GetPhysAddress(const Surface &surf, LwU64 offset)
{
    LwRmPtr pLwRm;
    LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0,0,0,0,0,0,0,0,0};

    params.memOffset = offset;
    RC rc = pLwRm->Control(surf.GetMemHandle(), LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR, &params,
                           sizeof(params));

    MASSERT(rc == OK);

    return params.memOffset;
}

static RC allocPhysicalBuffer(GpuDevice* pDev, MemDesc *pMemDesc, LwU32 size)
{
    StickyRC rc;
    Surface2D *pVidMemSurface = &(pMemDesc->surf2d);
    pVidMemSurface->SetForceSizeAlloc(1);
    pVidMemSurface->SetLayout(Surface2D::Pitch);
    pVidMemSurface->SetColorFormat(ColorUtils::Y8);
    pVidMemSurface->SetLocation(Memory::Fb);
    pVidMemSurface->SetArrayPitchAlignment(256);
    pVidMemSurface->SetArrayPitch(1);
    pVidMemSurface->SetArraySize(size);
    pVidMemSurface->SetAddressModel(Memory::Paging);
    pVidMemSurface->SetAlignment(256);
    pVidMemSurface->SetPhysContig(true);
    pVidMemSurface->SetLayout(Surface2D::Pitch);

    rc = pVidMemSurface->Alloc(pDev);
    if (rc != OK)
    {
        return rc;
    }
    rc = pVidMemSurface->Map();
    if (rc != OK)
    {
        pVidMemSurface->Free();
        return rc;
    }

    pMemDesc->gpuAddrBlkFb = (LwU32)(GetPhysAddress(*pVidMemSurface, 0) >> 8);
    pMemDesc->pCpuAddrFb = (LwU32 *)pVidMemSurface->GetAddress();

    return OK;
}

class DmvaTestCases;
typedef RC (DmvaTestCases::*TestFunc)(void);
struct TestCase
{
    TestFunc func;
    const char *name;
    bool bShouldRunOnFalcArr[4];
    bool bShouldResetAfterTest;
};

class DmvaTestCases
{
   public:
    DmvaTestCases(DmvaTest *pDmvaTest, const FalconInfo *pFalconInfo)
        : m_pSubdev(pDmvaTest->GetBoundGpuSubdevice()), m_pLwrrentTest(NULL), m_pInfo(pFalconInfo), m_pDmvaTest(pDmvaTest), m_dmaRegion(0)
    {
    }

    RC runTestSuite(void);
    virtual ~DmvaTestCases(void)
    {
    }

   protected:
    RC getTestRC(void);
    RC initTestCases(void);
    RC genericTest(void);
    RC readRegsTest(void);
    RC carveoutTest(void);
    RC dmlvlExceptionTest(void);
    RC dmemExceptionTest(void);

    static const TestCase p_TestCaseArr[DMEMVA_NUM_TESTS];

    static inline LwU32 getNumTests(void)
    {
        return sizeof(p_TestCaseArr) / sizeof(p_TestCaseArr[0]);
    }

    static const LwU32 FALC_RESET_TIMEOUT_US = 30 * 1000 * 1000;  // 30s

    GpuSubdevice* m_pSubdev;
    LwRmPtr m_pLwRm;
    const TestCase *m_pLwrrentTest;
    const FalconInfo *m_pInfo;
    LwU32 m_testIndex;
    DmvaTest *m_pDmvaTest;
    LwRm::Handle m_dmaRegion;

    enum ReadWrite
    {
        DMVARD,
        DMVAWR,
    };

    void dmvaPrintf(const char *format, ...);

    LwU32 imemAccess(LwU32 addr, LwU32 *codeArr, LwU32 codeSize, ReadWrite rw, LwU16 tag,
                     LwBool bSelwre);
    LwU32 dmemAccess(LwU32 addr, LwU32 *dataArr, LwU32 dataSize, ReadWrite rw,
                     LwU16 tag);  // TODO: FIX THIS
    inline LwU32 dRead32(LwU32 addr)
    {
        m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_DMEMC(0), addr);
        return m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_DMEMD(0));
        /*
        LwU32 data;
        dmemAccess(addr, &data, sizeof(LwU32), DMVARD, 0);
        return data;
        */
    }

    inline void dWrite32(LwU32 addr, LwU32 data)
    {
        m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_DMEMC(0), addr);
        return m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_DMEMD(0), data);
        // dmemAccess(addr, &data, sizeof(LwU32), DMVAWR, 0);
    }

    void dumpImem(LwU32 addr, LwU32 size);
    void dumpDmem(LwU32 addr, LwU32 size);
    inline void dumpAllImem(void)
    {
        dumpImem(0, getCodeSize());
    }

    inline void dumpAllDmem(void)
    {
        dumpDmem(0, getDataSize());
    }

    inline void ucodeBarrier(void)
    {
        waitForStop();
        resume();
    }

    // these functions syncronize to ucode before performing a read/write
    inline LwU32 ucodeRead32(void)
    {
        waitForStop();
        LwU32 data = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX1);
        resume();
        return data;
    }

    inline void ucodeWrite32(LwU32 data)
    {
        waitForStop();
        m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX1, data);
        resume();
    }

    void bootstrap(LwU32 bootvector);
    void resume(void);
    RC reset(void);

    RC poll(bool (DmvaTestCases::*pollFunction)(void), LwU32 timeoutUS);
    bool isScrubbingDone(void);
    bool isNotRunning(void);
    bool isHalted(void);
    bool isStopped(void);
    RC waitForHalt(void);
    RC waitForStop(void);

    inline const char *getTestName(void) const
    {
        return m_pLwrrentTest->name;
    }

    inline TestFunc getTestFunc(void) const
    {
        return m_pLwrrentTest->func;
    }

    LwU32 getEngineId(void) { return m_pInfo->getEngineId(); }
    const char *getFalconName(void) const { return m_pInfo->getFalconName(); }
    LwU32 getEngineBase(void) const { return m_pInfo->getEngineBase(); }
    LwU32 *getUcodeData(void) const { return m_pInfo->getUcodeData(); }
    LwU32 *getUcodeHeader(void) const { return m_pInfo->getUcodeHeader(); }
    LwU32 setPMC(LwU32 lwPmc, LwU32 state) { return m_pInfo->setPMC(lwPmc, state); }
    void engineReset(void) const { return m_pInfo->engineReset(m_pSubdev); }

    inline LwU32 getCodeOffset(void) const
    {
        return getUcodeHeader()[0];
    }

    inline LwU32 getCodeSize(void) const
    {
        return getUcodeHeader()[1];
    }

    inline LwU32 getDataOffset(void) const
    {
        return getUcodeHeader()[2];
    }

    inline LwU32 getDataSize(void) const
    {
        return getUcodeHeader()[3];
    }

    inline LwU32 *getCode(void) const
    {
        return &(getUcodeData()[getCodeOffset() / sizeof(LwU32)]);
    }

    inline LwU32 *getData(void) const
    {
        return &(getUcodeData()[getDataOffset() / sizeof(LwU32)]);
    }

    inline LwU32 getNumApps(void) const
    {
        return getUcodeHeader()[4];
    }

    inline LwU32 *getAppCode(LwU32 appId) const
    {
        MASSERT(appId < getNumApps());
        return &(getUcodeData()[getUcodeHeader()[5 + 2 * appId] / sizeof(LwU32)]);
    }

    inline LwU32 getAppCodeSize(LwU32 appId) const
    {
        MASSERT(appId < getNumApps());
        return getUcodeHeader()[5 + 2 * appId + 1];
    }

    inline LwU32 *getAppData(LwU32 appId) const
    {
        MASSERT(appId < getNumApps());
        return &(getUcodeData()[getUcodeHeader()[5 + 2 * getNumApps() + 2 * appId] /
                                sizeof(LwU32)]);
    }

    inline LwU32 getAppDataSize(LwU32 appId) const
    {
        MASSERT(appId < getNumApps());
        return getUcodeHeader()[5 + 2 * getNumApps() + 2 * appId + 1];
    }
};

void DmvaTestCases::dmvaPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    if (m_pLwrrentTest == NULL)
    {
        Printf(Tee::PriHigh, "%5s: ", getFalconName());
    }
    else
    {
        Printf(Tee::PriHigh, "%5s: %s ", getFalconName(), getTestName());
    }
    ModsExterlwAPrintf(Tee::PriHigh, Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL, format, args);
    va_end(args);
}

static const char *getBlockStatusString(BlockStatus blockStatus)
{
    switch (blockStatus)
    {
        case INVALID:
            return "INVALID";
        case VALID_AND_CLEAN:
            return "VALID_AND_CLEAN";
        case VALID_AND_DIRTY:
            return "VALID_AND_DIRTY";
        case PENDING:
            return "PENDING";
        default:
            return "UNKNOWN_BLOCK_STATUS";
    }
}

RC DmvaTestCases::getTestRC(void)
{
    LwU32 m0 = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX0);
    LwU32 m1 = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX1);
    LwU32 exCause;
    switch (m0)
    {
        case DMVA_NULL:
            dmvaPrintf("The return code was not set by falcon.\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_FAIL:
            dmvaPrintf("Generic failure.\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_ASSERT_FAIL:
            dmvaPrintf("Ucode assert failed on line %u.\n", m1);
            return RC::SOFTWARE_ERROR;
        case DMVA_BAD_TAG:
            dmvaPrintf("Bad tag. Expected %04x, but got %04x\n", m1 >> 16, m1 & 0xffff);
            return RC::SOFTWARE_ERROR;
        case DMVA_BAD_STATUS:
            dmvaPrintf("Bad status. Expected %s, but got %s\n",
                       getBlockStatusString((BlockStatus)(m1 >> 16)),
                       getBlockStatusString((BlockStatus)(m1 & 0xffff)));
            return RC::SOFTWARE_ERROR;
        case DMVA_BAD_MASK:
            dmvaPrintf("Bad mask. Expected %01x, but got %01x\n", m1 >> 16, m1 & 0xffff);
            return RC::SOFTWARE_ERROR;
        case DMVA_DMEMC_BAD_DATA_READ:
            dmvaPrintf("DMEMC: bad data read.\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_DMEMC_BAD_DATA_WRITTEN:
            dmvaPrintf("DMEMC: bad data written.\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_DMEMC_BAD_LVLERR:
            dmvaPrintf("Bad DMEMC.lvlerr\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_DMLVL_SHOULD_EXCEPT:
            dmvaPrintf("Read/Write should have caused exception, but it didn't\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_EMEM_BAD_DATA:
            dmvaPrintf("Bad data at address %08x\n", m1);
            return RC::SOFTWARE_ERROR;
        case DMVA_BAD_DATA:
            dmvaPrintf("Bad data. Expected %08x, but got %08x\n", dRead32(m1),
                       dRead32(m1 + sizeof(LwU32)));
            return RC::SOFTWARE_ERROR;
        case DMVA_DMEMC_BAD_MISS:
            dmvaPrintf("Bad DMEMC.miss\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_BAD_ZEROS:
            dmvaPrintf("Bad dmblk.zeros\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_BAD_VALID:
            dmvaPrintf("Bad dm{blk,tag}.valid\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_BAD_PENDING:
            dmvaPrintf("Bad dm{blk,tag}.pending\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_BAD_DIRTY:
            dmvaPrintf("Bad dm{blk,tag}.dirty\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_DMA_TAG_BAD_NEXT_TAG:
            dmvaPrintf("Bad DMA tag: expected increment\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_DMA_TAG_HANDLER_WAS_NOT_CALLED:
            dmvaPrintf("Interrupt handler was not called after DMA2\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_UNEXPECTED_EXCEPTION:
            exCause = REF_VAL(LW_PFALCON_FALCON_EXCI_EXCAUSE, m1);
            switch (exCause)
            {
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_TRAP0:
                    dmvaPrintf("Unexpected exception: trap0\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_TRAP1:
                    dmvaPrintf("Unexpected exception: trap1\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_TRAP2:
                    dmvaPrintf("Unexpected exception: trap2\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_TRAP3:
                    dmvaPrintf("Unexpected exception: trap3\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_ILL_INS:
                    dmvaPrintf("Unexpected exception: illegal instruction\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_ILW_INS:
                    dmvaPrintf("Unexpected exception: invalid instruction\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_MISS_INS:
                    dmvaPrintf("Unexpected exception: IMEM miss\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_DHIT_INS:
                    dmvaPrintf("Unexpected exception: IMEM multihit\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_BRKPT_INS:
                    dmvaPrintf("Unexpected exception: breakpoint\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_DMEM_MISS_INS:
                    dmvaPrintf("Unexpected exception: DMEM miss\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_DMEM_DHIT_INS:
                    dmvaPrintf("Unexpected exception: DMEM multihit\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_DMEM_PAFAULT_INS:
                    dmvaPrintf("Unexpected exception: DMEM PA fault\n");
                    break;
                case LW_PFALCON_FALCON_EXCI_EXCAUSE_DMEM_PERMISSION_INS:
                    dmvaPrintf("Unexpected exception: DMEM permission exception\n");
                    break;
                default:
                    dmvaPrintf("Unexpected exception: unknown exception %02x\n", exCause);
                    break;
            }
            dmvaPrintf("Exception PC: %05x\n", REF_VAL(LW_PFALCON_FALCON_EXCI_EXPC, m1));
            return RC::SOFTWARE_ERROR;
        case DMVA_PLATFORM_NOT_SUPPORTED:
            dmvaPrintf("This test is not supported on this engine\n");
            return RC::SOFTWARE_ERROR;
        case DMVA_OK:
            return OK;
        default:
            dmvaPrintf("Corrupted return code. (MAILBOX0, MAILBOX1) = (%08x, %08x)\n", m0, m1);
            return RC::SOFTWARE_ERROR;
    }
}

RC DmvaTestCases::initTestCases(void)
{
    StickyRC rc;
    m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX0, DMVA_NULL);
    bootstrap(0);

    // send seed to ucode
    LwU32 seed = DefaultRandom::GetRandom();
    // dmvaPrintf("Seeding with %08x\n", seed);
    ucodeWrite32(seed);

    // send gpu FB addresses to ucode for DMA transfers
    ucodeWrite32(g_dmemDesc.gpuAddrBlkFb);
    ucodeWrite32(g_imemDesc.gpuAddrBlkFb);

    // check return code
    rc = waitForStop();
    rc = getTestRC();
    return rc;
}

RC DmvaTestCases::genericTest(void)
{
    StickyRC rc;
    for (LwU32 subTestIndex = 0;; subTestIndex++)
    {
        if (subTestIndex > 0)
        {
            dmvaPrintf("Subtest %3u: Running...\n", subTestIndex);
            nSubtestsRun++;
        }

        // Preload with DMVA_NULL the MAILBOX{0,1} registers, which will be used to communicate a
        // return code.
        // If the falcon waits for any reason before setting this register, we will detect it.
        if (subTestIndex > 0)
        {
            m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX1, DMVA_NULL);
        }
        m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX0, DMVA_NULL);
        resume();

        LwU32 mailbox0;
        while (true)
        {
            rc = waitForStop();
            mailbox0 = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX0);
            if (mailbox0 == DMVA_REQUEST_PRIV_RW_FROM_RM)
            {
                m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX0, DMVA_NULL);
                resume();
                LwU32 addr = ucodeRead32();
                LwBool bRead = ucodeRead32();
                if (bRead)
                {
                    ucodeWrite32(m_pSubdev->RegRd32(addr));
                }
                else
                {
                    LwU32 data = ucodeRead32();
                    m_pSubdev->RegWr32(addr, data);
                    ucodeBarrier();
                }
            }
            else
            {
                break;
            }
        }

        if (mailbox0 != DMVA_FORWARD_PROGRESS)
        {
            rc = getTestRC();
            if (subTestIndex > 0 && rc != OK)
            {
                nSubtestsFailed++;
            }
            break;
        }
        LwU32 falcTestIdx = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX1);
        if (falcTestIdx != subTestIndex)
        {
            dmvaPrintf("Subtest %3u: Corrupted subtest index. Expected %u, but got %u.\n",
                       subTestIndex, subTestIndex, falcTestIdx);
            rc = RC::SOFTWARE_ERROR;
            nSubtestsFailed++;
            break;
        }
    }
    return rc;
}

RC DmvaTestCases::carveoutTest(void)
{
#ifdef NO_PRIV_SEC
    return OK;
#endif
    m_pSubdev->RegWr32(LW_FUSE_OPT_PRIV_SEC_EN, 0x01);
    StickyRC rc;
    resume();
    LwU32 nTests = ucodeRead32();
    for (LwU32 subTestIndex = 0; subTestIndex < nTests; subTestIndex++)
    {
        dmvaPrintf("Subtest %3u: Running...\n", subTestIndex + 1);
        nSubtestsRun++;
        LwU32 addr = ucodeRead32();
        LwU32 data = ucodeRead32();
        LwU32 newData = ucodeRead32();
        LwBool bOnCarveout = ucodeRead32();
        LwBool bPrivMeetsMask = ucodeRead32();

        rc = waitForStop();
        // TODO: replace deadbeef and dead2222 with the correct constants
        LwU32 expectedData = bOnCarveout ? (bPrivMeetsMask ? data : 0xdead2222) : 0xdeadbeef;
        LwU32 dataRead = dRead32(addr);
        LwU32 badDataRead = expectedData != dataRead;
        LwBool lvlErr = REF_VAL(LW_PFALCON_FALCON_DMEMC_LVLERR,
                                m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_DMEMC(0)));
        if (badDataRead)
        {
            rc = RC::SOFTWARE_ERROR;
            dmvaPrintf("RM expected to read %08x, but it read %08x. Data was %08x.\n", expectedData,
                       dataRead, data);
            dmvaPrintf("Lvlerr is %01u\n", lvlErr);
        }
        dWrite32(addr, newData);
        resume();
        LwU32 badDataWritten = ucodeRead32();
        if (badDataWritten)
        {
            rc = RC::SOFTWARE_ERROR;
            nSubtestsFailed++;
            dmvaPrintf("RM changed DMEM when it didn't have permissions\n");
        }
    }
    return rc;
}

RC DmvaTestCases::dmlvlExceptionTest(void)
{
    StickyRC rc;
    LwU32 nSubtests = 1;
    for (LwU32 subTestIndex = 0; subTestIndex < nSubtests; subTestIndex++)
    {
        dmvaPrintf("Subtest %3u: Running...\n", subTestIndex + 1);
        nSubtestsRun++;

// start the test, with the proper testIndex
#ifndef DONT_RESET_ON_EXCEPTION
        reset();
#endif
        m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX0, DMVA_NULL);
        m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX1, m_testIndex);
#ifndef DONT_RESET_ON_EXCEPTION
        resume();
#else
        bootstrap(0);
#endif

        ucodeWrite32(subTestIndex);
        nSubtests = ucodeRead32();
        LwBool bShouldExcept = ucodeRead32();
        LwU32 dmemAddr = ucodeRead32();

        // verify
        rc = waitForHalt();
        LwU32 exci = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_EXCI);
        LwU32 exci2 = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_EXCI2);
        LwU32 exCause = REF_VAL(LW_PFALCON_FALCON_EXCI_EXCAUSE, exci);

        LwU32 expectedCause =
            bShouldExcept ? LW_PFALCON_FALCON_EXCI_EXCAUSE_DMEM_PERMISSION_INS : 0;
        LwU32 expectedAddr = bShouldExcept ? dmemAddr : 0;

        LwBool badCause = exCause != expectedCause;

        LwBool badAddr = exci2 != dmemAddr && exci2 != (dmemAddr - 4);

        if (badCause || badAddr)
        {
            dmvaPrintf(
                "Expected (EXCI.excause, EXCI2) to be (%02x, %08x), but it was (%02x, %08x). "
                "EXCI = %08x\n",
                expectedCause, expectedAddr, exCause, exci2, exci);
            rc = RC::SOFTWARE_ERROR;
            nSubtestsFailed++;
        }

        rc = getTestRC();  // We must pass ucode's tests as well
    }
    return rc;
}

RC DmvaTestCases::dmemExceptionTest(void)
{
    StickyRC rc;
    LwU32 nTests = 1;
    for (LwU32 subTestIndex = 0; subTestIndex < nTests; subTestIndex++)
    {
        dmvaPrintf("Subtest %3u: Running...\n", subTestIndex + 1);
        nSubtestsRun++;

// start the test, with the proper testIndex
#ifndef DONT_RESET_ON_EXCEPTION
        reset();
#endif
        m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX0, DMVA_NULL);
        m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX1, m_testIndex);
#ifndef DONT_RESET_ON_EXCEPTION
        resume();
#else
        bootstrap(0);
#endif

        nTests = ucodeRead32();
        ucodeWrite32(subTestIndex);

        // verify
        rc = waitForHalt();
        if (getTestRC() != OK)
        {
            rc = RC::SOFTWARE_ERROR;
            nSubtestsFailed++;
        }
    }
    return rc;
}

RC DmvaTestCases::readRegsTest(void)
{
    StickyRC rc;
    resume();
    rc = waitForStop();
    LwU32 mailbox0 = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX0);
    LwU32 mailbox1 = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX1);
    if (mailbox0 != REG_TEST_MAILBOX0 || mailbox1 != REG_TEST_MAILBOX1)
    {
        dmvaPrintf("Expected (MAILBOX0, MAILBOX1) to be (%08x, %08x), but got (%08x, %08x)\n",
                   REG_TEST_MAILBOX0, REG_TEST_MAILBOX1, mailbox0, mailbox1);
        rc = RC::SOFTWARE_ERROR;
    }
    return rc;
}

// TODO: move this
const TestCase DmvaTestCases::p_TestCaseArr[DMEMVA_NUM_TESTS] =
{
    // test function                       description                              engine enable reset       ID
    { &DmvaTestCases::initTestCases,      "initialize tests.                          ", {1,1,1,1}, false }, // 00
    { &DmvaTestCases::readRegsTest,       "read registers.                            ", {1,1,1,1}, false }, // 01
    { &DmvaTestCases::genericTest,        "blocks are invalid after reset.            ", {1,1,1,1}, false }, // 02
    { &DmvaTestCases::genericTest,        "dmem instructions: read/write to PA/VA.    ", {1,1,1,1}, false }, // 03
    { &DmvaTestCases::genericTest,        "dmblk: can read block status.              ", {1,1,1,1}, false }, // 04
    { &DmvaTestCases::genericTest,        "write sets dirty bit.                      ", {1,1,1,1}, false }, // 05
    { &DmvaTestCases::genericTest,        "dmtag: can read block status.              ", {1,1,1,1}, false }, // 06
    { &DmvaTestCases::genericTest,        "DMVACTL.bound.                             ", {1,1,1,1}, false }, // 07
    { &DmvaTestCases::genericTest,        "EMEM.                                      ", {1,0,0,1}, false }, // 08
    { &DmvaTestCases::genericTest,        "CMEM.                                      ", {1,1,0,1}, false }, // 09
    { &DmvaTestCases::genericTest,        "DMEMC: write, va=0, settag=0, valid, clean.", {1,1,1,1}, false }, // 10
    { &DmvaTestCases::genericTest,        "DMEMC: write, va=0, settag=0, invalid.     ", {1,1,1,1}, false }, // 11
    { &DmvaTestCases::genericTest,        "DMEMC: write, va=0, settag=1, any status.  ", {1,1,1,1}, false }, // 12
    { &DmvaTestCases::genericTest,        "DMEMC: write, va=1, settag=0, any status.  ", {1,1,1,1}, false }, // 13
    { &DmvaTestCases::genericTest,        "DMEMC: write, va=1, settag=1, any status.  ", {1,1,1,1}, false }, // 14
    { &DmvaTestCases::genericTest,        "DMEMC: read, va=0, settag=x, any status.   ", {1,1,1,1}, false }, // 15
    { &DmvaTestCases::genericTest,        "DMEMC: read, va=1, settag=x, any status.   ", {1,1,1,1}, false }, // 16
    { &DmvaTestCases::genericTest,        "DMA: settag=0.                             ", {1,1,1,1}, false }, // 17
    { &DmvaTestCases::genericTest,        "DMA: settag=1.                             ", {1,1,1,1}, false }, // 18
    { &DmvaTestCases::genericTest,        "HS: bootrom with physical signature/stack. ", {1,1,1,1}, false }, // 19
    { &DmvaTestCases::genericTest,        "HS: bootrom with virtual signature/stack.  ", {1,1,1,1}, false }, // 20
    { &DmvaTestCases::carveoutTest,       "carveouts.                                 ", {0,1,0,0}, false }, // 21
    { &DmvaTestCases::genericTest,        "dmlvl: add/remove permissions from block.  ", {1,1,1,1}, false }, // 22
    { &DmvaTestCases::genericTest,        "DMA: add/remove permissions from block.    ", {1,1,1,1}, false }, // 23
    { &DmvaTestCases::dmlvlExceptionTest, "dmlvl: exception on inselwre ucode access. ", {1,1,1,1}, true  }, // 24
    { &DmvaTestCases::genericTest,        "DMATRF: security test.                     ", {1,1,1,1}, false }, // 25
    { &DmvaTestCases::genericTest,        "DMEMC: security test.                      ", {1,1,1,1}, false }, // 26
    { &DmvaTestCases::genericTest,        "SCP: read/write using PA/VA, (un)mapped.   ", {1,1,1,1}, false }, // 27
    { &DmvaTestCases::dmemExceptionTest,  "dmem instructions: dmem exceptions.        ", {1,1,1,1}, true  }, // 28
    { &DmvaTestCases::genericTest,        "dmilw: block status becomes invalid.       ", {1,1,1,1}, false }, // 29
    { &DmvaTestCases::genericTest,        "dmclean: block status becomes clean.       ", {1,1,1,1}, false }, // 30
    { &DmvaTestCases::genericTest,        "setdtag: block status becomes valid&clean. ", {1,1,1,1}, false }, // 31
    // DMA tagging tests are disabled
    { &DmvaTestCases::genericTest,        "DMA tagging: interrupts and polling.       ", {0,0,0,0}, false }, // 32
};

LwU32 DmvaTestCases::imemAccess(LwU32 addr, LwU32 *codeArr, LwU32 codeSize, ReadWrite rw, LwU16 tag,
                                LwBool bSelwre)
{
    MASSERT((addr & 0x03) == 0);
    MASSERT((codeSize & 0x03) == 0);
    MASSERT(rw == DMVARD || rw == DMVAWR);
    LwU32 imemc = DRF_NUM(_PFALCON, _FALCON_IMEMC, _OFFS, addr & 0xFF) |
                  DRF_NUM(_PFALCON, _FALCON_IMEMC, _BLK, addr >> 8) |
                  DRF_NUM(_PFALCON, _FALCON_IMEMC, _AINCW, rw == DMVAWR) |
                  DRF_NUM(_PFALCON, _FALCON_IMEMC, _AINCR, rw == DMVARD) |
                  DRF_NUM(_PFALCON, _FALCON_IMEMC, _SELWRE, bSelwre ? 1 : 0);
    m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_IMEMC(0), imemc);

    LwU32 lastDWORD = 0;
    for (LwU32 imemAddrOff = 0; imemAddrOff < codeSize; imemAddrOff += sizeof(LwU32))
    {
        if (((addr + imemAddrOff) & 0xFF) == 0 && rw == DMVAWR)
        {
            m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_IMEMT(0), (LwU32)tag++);
        }
        if (rw == DMVARD)
        {
            lastDWORD = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_IMEMD(0));
            if (codeArr != NULL)
                codeArr[imemAddrOff / sizeof(LwU32)] = lastDWORD;
        }
        else if (rw == DMVAWR)
        {
            m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_IMEMD(0),
                            codeArr[imemAddrOff / sizeof(LwU32)]);
        }
    }
    return lastDWORD;
}

LwU32 DmvaTestCases::dmemAccess(LwU32 addr, LwU32 *dataArr, LwU32 dataSize, ReadWrite rw, LwU16 tag)
{
    MASSERT((addr & 0x03) == 0);
    MASSERT((dataSize & 0x03) == 0);
    MASSERT(rw == DMVARD || rw == DMVAWR);
    (void)tag;
    LwU32 dmemc = DRF_NUM(_PFALCON, _FALCON_DMEMC, _OFFS, addr & 0xFF) |
                  DRF_NUM(_PFALCON, _FALCON_DMEMC, _BLK, addr >> 8) |
                  DRF_NUM(_PFALCON, _FALCON_DMEMC, _AINCW, rw == DMVAWR) |
                  DRF_NUM(_PFALCON, _FALCON_DMEMC, _AINCR, rw == DMVARD);
    m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_DMEMC(0), dmemc);

    LwU32 lastDWORD = 0;
    for (LwU32 dmemAddrOff = 0; dmemAddrOff < dataSize; dmemAddrOff += sizeof(LwU32))
    {
        if (rw == DMVARD)
        {
            lastDWORD = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_DMEMD(0));
            if (dataArr != NULL)
                dataArr[dmemAddrOff / sizeof(LwU32)] = lastDWORD;
        }
        else if (rw == DMVAWR)
        {
            m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_DMEMD(0),
                            dataArr[dmemAddrOff / sizeof(LwU32)]);
        }
    }
    return lastDWORD;
}

void DmvaTestCases::dumpImem(LwU32 addr, LwU32 size)
{
    dmvaPrintf("Dumping IMEM[%08x:%08x)...\n", addr, addr + size);
    MASSERT((size & 0x03) == 0);
    for (LwU32 imemAddrOff = 0; imemAddrOff < size; imemAddrOff += sizeof(LwU32))
    {
        dmvaPrintf("%s IMEM[%08x] = %08x\n", getFalconName(), addr + imemAddrOff,
                   imemAccess(addr + imemAddrOff, NULL, sizeof(LwU32), DMVARD, 0, 0));
    }
}

void DmvaTestCases::dumpDmem(LwU32 addr, LwU32 size)
{
    dmvaPrintf("Dumping DMEM[%08x:%08x)...\n", addr, addr + size);
    MASSERT((size & 0x03) == 0);
    for (LwU32 dmemAddrOff = 0; dmemAddrOff < size; dmemAddrOff += sizeof(LwU32))
    {
        dmvaPrintf("%s DMEM[%08x] = %08x\n", getFalconName(), addr + dmemAddrOff,
                   dmemAccess(addr + dmemAddrOff, NULL, sizeof(LwU32), DMVARD, 0));
    }
}

RC DmvaTestCases::poll(bool (DmvaTestCases::*pollFunction)(void), LwU32 timeoutUS)
{
    static const LwU32 SLEEP_US = 1000;
    MASSERT(timeoutUS >= SLEEP_US);
    for (LwU32 uSec = 0; uSec < timeoutUS; uSec += SLEEP_US)
    {
        Utility::SleepUS(SLEEP_US);
        if ((this->*pollFunction)())
            return OK;
    }
    dmvaPrintf("TIMEOUT\n");
    return RC::TIMEOUT_ERROR;
}

bool DmvaTestCases::isScrubbingDone(void)
{
    LwU32 FalconDmaCtl = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_DMACTL);
    LwU32 DmemScrub = DRF_VAL(_PFALCON, _FALCON_DMACTL, _DMEM_SCRUBBING, FalconDmaCtl);
    LwU32 ImemScrub = DRF_VAL(_PFALCON, _FALCON_DMACTL, _IMEM_SCRUBBING, FalconDmaCtl);

    return ((DmemScrub == LW_PFALCON_FALCON_DMACTL_DMEM_SCRUBBING_DONE) &&
            (ImemScrub == LW_PFALCON_FALCON_DMACTL_IMEM_SCRUBBING_DONE));
}

bool DmvaTestCases::isNotRunning(void)
{
    LwU32 cpuCtl = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_CPUCTL);
    return FLD_TEST_DRF_NUM(_PFALCON, _FALCON_CPUCTL, _HALTED, 0x1, cpuCtl) ||
           FLD_TEST_DRF_NUM(_PFALCON, _FALCON_CPUCTL, _STOPPED, 0x1, cpuCtl);
}

bool DmvaTestCases::isHalted(void)
{
    LwU32 cpuCtl = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_CPUCTL);
    return FLD_TEST_DRF_NUM(_PFALCON, _FALCON_CPUCTL, _HALTED, 0x1, cpuCtl);
}

bool DmvaTestCases::isStopped(void)
{
    LwU32 cpuCtl = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_CPUCTL);
    return FLD_TEST_DRF_NUM(_PFALCON, _FALCON_CPUCTL, _STOPPED, 0x1, cpuCtl);
}

RC DmvaTestCases::waitForHalt(void)
{
    RC rc;
    CHECK_RC(poll(&DmvaTestCases::isNotRunning, FALC_RESET_TIMEOUT_US));
    if (isHalted())
    {
        return OK;
    }
    else
    {
        dmvaPrintf("Falcon waited when it should have halted.\n");
        return RC::SOFTWARE_ERROR;
    }
}

RC DmvaTestCases::waitForStop(void)
{
    RC rc;
    CHECK_RC(poll(&DmvaTestCases::isNotRunning, FALC_RESET_TIMEOUT_US));
    if (isStopped())
    {
        return OK;
    }
    else
    {
        dmvaPrintf("Falcon halted when it should have waited.\n");
        return RC::SOFTWARE_ERROR;
    }
}

void DmvaTestCases::bootstrap(LwU32 bootvector)
{
    // Clear DMACTL
    m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_DMACTL, 0);

    // Set Bootvec
    m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_BOOTVEC,
                    DRF_NUM(_PFALCON, _FALCON_BOOTVEC, _VEC, bootvector));

    LwU32 cpuCtl = DRF_DEF(_PFALCON, _FALCON_CPUCTL, _STARTCPU, _TRUE);
    m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_CPUCTL, cpuCtl);
    if (isNotRunning())
        dmvaPrintf(
            "WARNING: DETECTED SUSPICIOUS WAIT/HALT IMMEDIATELY AFTER BOOT. FALCON POSSIBLY "
            "IGNORED STARTCPU\n");
}

RC DmvaTestCases::reset(void)
{
    StickyRC rc;

    const TestCase *pLwrrentTest = m_pLwrrentTest;
    m_pLwrrentTest = &p_TestCaseArr[0];

    engineReset();

    // Disable context DMA support
    LwU32 FalconDmaCtl = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_DMACTL);
    m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_DMACTL,
                    FLD_SET_DRF(_PFALCON_FALCON, _DMACTL, _REQUIRE_CTX, _FALSE, FalconDmaCtl));

    LwU32 pmc = m_pSubdev->RegRd32(LW_PMC_ENABLE);
    pmc = setPMC(pmc, 0);
    m_pSubdev->RegWr32(LW_PMC_ENABLE, pmc);

    pmc = setPMC(pmc, 1);
    m_pSubdev->RegWr32(LW_PMC_ENABLE, pmc);

    // Wait for the scrubbing to stop
    CHECK_RC(poll(&DmvaTestCases::isScrubbingDone, FALC_RESET_TIMEOUT_US));

    imemAccess(0, getCode(), getCodeSize(), DMVAWR, 0, 0);
    dmemAccess(0, getData(), getDataSize(), DMVAWR, 0);
    imemAccess(getCodeSize(), getAppCode(0), getAppCodeSize(0), DMVAWR, getCodeSize() >> 8, 0);
    dmemAccess(getDataSize(), getAppData(0), getAppDataSize(0), DMVAWR, getDataSize() >> 8);

    // store the entire IMEM image in FB
    LwU32 codeSize = getCodeSize();
    for (LwU32 i = 0; i < getNumApps(); i++)
    {
        codeSize += getAppCodeSize(i);
    }

    for (LwU32 codeIdx = 0; codeIdx < codeSize; codeIdx += 4)
    {
        m_pDmvaTest->memwr32(g_imemDesc.pCpuAddrFb + codeIdx / sizeof(LwU32),
                             getCode()[codeIdx / sizeof(LwU32)]);
    }

    g_imemDesc.surf2d.FlushCpuCache(Surface2D::Flush);

#ifndef NO_FB_VERIFY
    // verify fb imem
    for (LwU32 codeIdx = 0; codeIdx < codeSize; codeIdx += 4)
    {
        if (m_pDmvaTest->memrd32(g_imemDesc.pCpuAddrFb + codeIdx / sizeof(LwU32)) !=
            getCode()[codeIdx / sizeof(LwU32)])
        {
            dmvaPrintf("Could not verify IMEM loaded in FB.\n");
            return RC::SOFTWARE_ERROR;
        }
    }
#endif

    /*
    // dump fb imem
    for(LwU64 codeIdx = 0; codeIdx < codeSize; codeIdx += 4) {
        dmvaPrintf("imemAddr(%08x) cpuAddr(%016llx) fbAddr(%016llx) data(%08x)\n",
               (LwU32)codeIdx,
               (LwU64)(g_imemDesc.pCpuAddrFb + codeIdx/sizeof(LwU32)),
               (LwU64)((LwU64)(GetPhysAddress(g_imemDesc.surf2d, 0)) + codeIdx),
               m_pDmvaTest->memrd32(g_imemDesc.pCpuAddrFb +
                                    codeIdx/sizeof(LwU32))
            );
    }
    */

    m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX1, 0 /* initTestCases ID */);
    rc = initTestCases();
    m_pLwrrentTest = pLwrrentTest;
    return rc;
}

void DmvaTestCases::resume(void)
{
    m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_IRQSSET,
                    FLD_SET_DRF(_PFALCON, _FALCON_IRQSSET, _SWGEN0, _SET, 0));
    if (isNotRunning())
        dmvaPrintf(
            "WARNING: DETECTED SUSPICIOUS WAIT/HALT IMMEDIATELY AFTER RESUME. FALCON POSSIBLY "
            "IGNORED INTERRUPT\n");
}

RC DmvaTestCases::runTestSuite(void)
{
    static const bool resetAfterEachTest = false;
    RC rc;

    dmvaPrintf("Initializing tests...\n");

    CHECK_RC(reset());
    for (m_testIndex = 1; m_testIndex < getNumTests(); m_testIndex++)
    {
        m_pLwrrentTest = &p_TestCaseArr[m_testIndex];
        // don't run the test if its not meant for this engine
        if (!m_pLwrrentTest->bShouldRunOnFalcArr[getEngineId()])
            continue;
        m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_MAILBOX1, m_testIndex);
        // don't send an interrupt to RM when the falcon halts
        LwU32 irqmclr = m_pSubdev->RegRd32(getEngineBase() + LW_PFALCON_FALCON_IRQMCLR);
        m_pSubdev->RegWr32(getEngineBase() + LW_PFALCON_FALCON_IRQMCLR,
                        FLD_SET_DRF(_PFALCON, _FALCON_IRQMCLR, _HALT, _SET, irqmclr));

        dmvaPrintf("Running...\n");
        nTestsRun++;
        if (((this->*(m_pLwrrentTest->func))()) != OK)
        {
            rc = RC::SOFTWARE_ERROR;
            dmvaPrintf("FAIL\n");
            nTestsFailed++;
        }

        if (resetAfterEachTest || m_pLwrrentTest->bShouldResetAfterTest)
        {
            if (reset() != OK)
            {
                // we can't run any other tests if initialization didn't work
                return RC::SOFTWARE_ERROR;
            }
        }
    }
    return rc;
}

void FalconInfo::ucodePatchSignature(LwU32 *pImg, LwU32 *pSig, LwU32 patchLoc)
{
    patchLoc /= sizeof(LwU32);

    for (LwU32 i = 0; i < 16 / sizeof(LwU32); i++)
    {
        pImg[patchLoc + i] = pSig[i];
    }
}

static void ucodePatchSignature(const FalconInfo *pFalcon)
{
    FalconInfo::ucodePatchSignature(pFalcon->getUcodeData(),
                                    pFalcon->getSignature(true),
                                    pFalcon->getPatchLocation());
}

RC runTestSuite(DmvaTest *pDmvaTest, TestCtl testCtl)
{
    GpuSubdevice *pSubdev = pDmvaTest->GetBoundGpuSubdevice();
    StickyRC rc;
    DefaultRandom::SeedRandom((LwU32)Xp::GetWallTimeUS());

    // allocate some space in FB for DMA transfers
    rc = allocPhysicalBuffer(pDmvaTest->GetBoundGpuDevice(), &g_dmemDesc, DMA_BUFFER_DMEM_SIZE);
    MASSERT(rc == OK);  // don't start tests if we could not allocate the buffer
    rc = allocPhysicalBuffer(pDmvaTest->GetBoundGpuDevice(), &g_imemDesc, DMA_BUFFER_IMEM_SIZE);
    MASSERT(rc == OK);

    if (testCtl.bRunOnSec && FalconInfo::getFalconInfo(pSubdev, FalconInfo::FALCON_SEC2))
    {
        const FalconInfo *pFalcon = FalconInfo::getFalconInfo(pSubdev, FalconInfo::FALCON_SEC2);

        ucodePatchSignature(pFalcon);
        rc = DmvaTestCases(pDmvaTest, pFalcon).runTestSuite();
    }
    if (testCtl.bRunOnPmu && FalconInfo::getFalconInfo(pSubdev, FalconInfo::FALCON_PMU))
    {
        const FalconInfo *pFalcon = FalconInfo::getFalconInfo(pSubdev, FalconInfo::FALCON_PMU);

        ucodePatchSignature(pFalcon);
        rc = DmvaTestCases(pDmvaTest, pFalcon).runTestSuite();
    }
    if (testCtl.bRunOnLwdec && FalconInfo::getFalconInfo(pSubdev, FalconInfo::FALCON_LWDEC))
    {
        const FalconInfo *pFalcon = FalconInfo::getFalconInfo(pSubdev, FalconInfo::FALCON_LWDEC);

        ucodePatchSignature(pFalcon);
        rc = DmvaTestCases(pDmvaTest, pFalcon).runTestSuite();
    }
    if (testCtl.bRunOnGsp && FalconInfo::getFalconInfo(pSubdev, FalconInfo::FALCON_GSP))
    {
        const FalconInfo *pFalcon = FalconInfo::getFalconInfo(pSubdev, FalconInfo::FALCON_GSP);

        ucodePatchSignature(pFalcon);
        rc = DmvaTestCases(pDmvaTest, pFalcon).runTestSuite();
    }

    Printf(Tee::PriHigh, "%u/%u tests failed\n", nTestsFailed, nTestsRun);
    Printf(Tee::PriHigh, "%u/%u subtests failed\n", nSubtestsFailed, nSubtestsRun);

    if (rc == OK)
        Printf(Tee::PriHigh, "DMEMVA tests passed on all falcons.\n");
    else
        Printf(Tee::PriHigh, "DMEMVA tests failed.\n");

    g_dmemDesc.surf2d.Free();
    g_imemDesc.surf2d.Free();

    return rc;
}
