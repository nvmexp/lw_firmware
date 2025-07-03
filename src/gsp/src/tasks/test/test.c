/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    test.c
 * @brief   Test task (for RM MODS tests).
 */

/* ------------------------ System Includes -------------------------------- */
#include <lwmisc.h>

/* ------------------------ Register Includes ------------------------------ */
#include <dev_top.h>
#include <riscv_csr.h>
#include <engine.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <lwrtos.h>
#include <sections.h>
#include <lwostmrcallback.h>

/* ------------------------ RISC-V system library  -------------------------- */
#include <shlib/syscall.h>
#include <syslib/syslib.h>
#include <drivers/drivers.h>
#include <drivers/hardware.h>
#include <drivers/mm.h>
#include <drivers/mpu.h>
#include <drivers/odp.h>
#include <drivers/vm.h>
#include <lwriscv-mpu.h>
#include <lwriscv/print.h>
#include "scp_rand.h"

/* ------------------------ Module Includes -------------------------------- */
#include "config/g_profile.h"
#include "unit_dispatch.h"
#include "tasks.h"
#if LWRISCV_PARTITION_SWITCH
#include "partswitch.h" // Interface for SHM
#endif // LWRISCV_PARTITION_SWITCH

#define tprintf(level, ...) dbgPrintf(level, "test: " __VA_ARGS__)

/*!
 * These Error defines convey fail/success status between
 * the main lwRTOS function and handleInitStat( ) function
 * As of now we only pass on FAIL/OK status msgmgmt queue.
 * Later if needed this can further be passed on to RM.
 */
typedef enum TEST_TASK_ERR_TYPES
{
    ERR_NONE = 0,                               // No Error
    ERR_UMODE_CTX_SWITCH_FAILED,                // U-mode CTX switch test FAILED.
    ERR_UMODE_CRTCL_SECTN_FAILED,               // U-mode critical section test FAILED
    ERR_INTR_DISABLED,                          // Interrupts disabled for ordinary task
    ERR_CRTCL_SECTN_INTR_ENABLED,               // Interrupts were not disabled in critical section
    ERR_CRTCL_SECTN_EXIT_INTR_DISABLED,         // Interrupts should be enabled after critical section exit.
    ERR_TMR_CALLBACK,                           // Failed to stop the calllback for timer
    ERR_FAILED_WAITING_ON_QUEUE,                // Failed waiting on queue
    ERR_KRNL_DMA_TEST_FAILED,                   // Failed Kernel DMA Test.
    ERR_KRNL_DMA_TEST_MAP_FAILED,               // Failed Kernel DMA Test while Mapping.
    ERR_KRNL_DMA_TEST_ALLOC_FAILED,             // Failed Allocation in Kernel DMA Test.
    ERR_KRNL_DMA_TEST_VA2PA_FAILED,             // Failed in Va to PA map in Kernel DMA Test.
    ERR_KRNL_DMA_TEST_FB_WRITE_FAILED,          // Failed during FB Write in Kernel DMA Test.
    ERR_KRNL_DMA_TEST_FB_WRITE_MATCH_FAILED,    // Mismatch in FB Writes in Kernel DMA test.
    ERR_KRNL_DMA_TEST_FB_READ_FAILED,           // Failed during FB Read in Kernel DMA Test.
    ERR_KRNL_DMA_TEST_FB_READ_MATCH_FAILED,     // Mismatch in FB Reads in Kernel DMA test.
    ERR_ILWALID_TEST_REQUEST,                   // Invalid Test Request sent.
    ERR_ODP_FAILED,                             // Failure in ODP test.
    ERR_VA_TO_PA_MAP_FAILED,                    // Failed to Map PS for the given VA.
    ERR_SCP_RNG_FAILED,                         // Random Number Generator Programing Failed.
    ERR_PART_SWITCH_FAILED,                     // Partition switch test failed
    ERR_MAX                                     // MAX Possible Error Type
} TEST_TASK_ERR_TYPES;

const char *ERR_MSG_LIST[] = {
    dbgStr(LEVEL_CRIT, "Error None."),
    dbgStr(LEVEL_CRIT, "U-mode CTX switch test FAILED."),
    dbgStr(LEVEL_CRIT, "U-mode critical section test FAILED."),
    dbgStr(LEVEL_CRIT, "Interrupts disabled for ordinary task."),
    dbgStr(LEVEL_CRIT, "Interrupts should be disabled in critical section."),
    dbgStr(LEVEL_CRIT, "Interrupts should be enabled after critical section exit."),
    dbgStr(LEVEL_CRIT, "Failed to stop the RmMsgTest callback."),
    dbgStr(LEVEL_CRIT, "test: Failed waiting on queue."),
    dbgStr(LEVEL_CRIT, "Kernel DMA test FAILED"),
    dbgStr(LEVEL_CRIT, "Kernel DMA test FAILED(map)"),
    dbgStr(LEVEL_CRIT, "Kernel DMA test FAILED(allocate)"),
    dbgStr(LEVEL_CRIT, "Kernel DMA test FAILED (va2pa)"),
    dbgStr(LEVEL_CRIT, "Kernel DMA test FAILED (DMA FB write)"),
    dbgStr(LEVEL_CRIT, "Kernel DMA FB write test FAILED"),
    dbgStr(LEVEL_CRIT, "Kernel DMA test FAILED (DMA FB read)"),
    dbgStr(LEVEL_CRIT, "Kernel DMA FB read test FAILED"),
    dbgStr(LEVEL_CRIT, "Invalid Sub-Test ID Received"),
    dbgStr(LEVEL_CRIT, "ODP test FAILED"),
    dbgStr(LEVEL_CRIT, "Failed to get Physical Address for the given Virtual Address"),
    dbgStr(LEVEL_CRIT, "Failed to program SCP Random Number Generator"),
};

static sysTASK_DATA OS_TMR_CALLBACK testTaskOsTmrCb;

static TEST_TASK_ERR_TYPES testDma(void);
static TEST_TASK_ERR_TYPES testMemOps(void);
static TEST_TASK_ERR_TYPES testKernelCriticalSections(void);
static TEST_TASK_ERR_TYPES testKernelMode(void);
#ifdef USE_SCPLIB
static TEST_TASK_ERR_TYPES testSCP_RNG(void);
#endif


#ifdef USE_SCPLIB
#define SCP_RAND_DATA_SIZE_IN_BYTES     16
LwU8 scpData[SCP_RAND_DATA_SIZE_IN_BYTES]
GCC_ATTRIB_SECTION("taskSharedScp", "mySCPDataTest")
GCC_ATTRIB_ALIGN(RISCV_CACHE_LINE_SIZE);

// Check SCP State
static void chkSCPState(void)
{
    LwU32 scpStatus1 = csbRead(ENGINE_REG(_SCP_STATUS));
    LwU32 scpStatus2 = intioRead(LW_PRGNLCL_RISCV_SCPDMAPOLL);
    tprintf(LEVEL_ALWAYS, "LW_PGSP_SCP_STATUS Status =========> 0x%x \n ", scpStatus1);
    tprintf(LEVEL_ALWAYS, "LW_PRGNLCL_RISCV_SCPDMAPOLL Status  ==> 0x%x \n ", scpStatus2);
    return;
}

/*
 * SubTask(Test) to generate Random Numbers using SCP.
 * We detrmine the state of RNG via SCP_STATUS/DMAPOLL.
 */
static TEST_TASK_ERR_TYPES testSCP_RNG(void)
{
    TEST_TASK_ERR_TYPES ret = ERR_NONE;
    FLCN_STATUS status = FLCN_OK;
    LwUPtr physAddr;
    LwU8* myOffset;
    LwU8 lastData[SCP_RAND_DATA_SIZE_IN_BYTES];

    myOffset = (LwU8*)(scpData);
    status = sysVirtToPhys((void*)myOffset, &physAddr);

    if (status == FLCN_OK)
    {
        tprintf(LEVEL_DEBUG, "testSCP_RNG::Virtual Address of SCP BUffer is %p \n ", myOffset);
        tprintf(LEVEL_DEBUG, "testSCP_RNG::Physical Address of SCP BUffer is %llx \n ", physAddr);
    }
    else
    {
        tprintf(LEVEL_ERROR, "testSCP_RNG::Error during Adress Translation (Err_Code = %d) \n ", status);
        return ERR_VA_TO_PA_MAP_FAILED;
    }

    tprintf(LEVEL_CRIT, "Filing SCP Buffer with known Data...\n ");
    for (int i = 0; i < SCP_RAND_DATA_SIZE_IN_BYTES; i++)
    {
        scpData[i] = (i + 5);
        lastData[i] = (i + 5);
        tprintf(LEVEL_CRIT, "%x ", scpData[i]);
    }
    tprintf(LEVEL_DEBUG, "\n");

    /* First iteration of the loop verifies that SCP to DMEM DMA transfer is fine.
     * Second iteration checks if the RNG generated values are truly random.
     */
    for (int k = 0; k < 2; k++)
    {
        chkSCPState();
        scpStartRand();
        chkSCPState();
        scpGetRand128(myOffset);
        chkSCPState();
        scpStopRand();
        chkSCPState();

        if (k == 0)
        {
            tprintf(LEVEL_DEBUG, "testSCP_RNG: Checking Random Values Against known values filled in !!! \n");
        }
        else
        {
            tprintf(LEVEL_DEBUG, "testSCP_RNG: Checking Random Values across mutiple generation attempts !!!\n");
        }

        if (memcmp(scpData, lastData, sizeof(scpData)) == 0)
        {
            tprintf(LEVEL_CRIT, "[%s]::Random Number Generation Failed !!!\n ", __FUNCTION__);
            return ERR_SCP_RNG_FAILED;
        }
        else
        {
            memcpy(lastData, scpData, sizeof(scpData));
        }

        tprintf(LEVEL_DEBUG, "[%s]: Printing Recevied Random Data !!!\n ", __FUNCTION__);
        for (int i = 0; i < SCP_RAND_DATA_SIZE_IN_BYTES; i++)
        {
            tprintf(LEVEL_ALWAYS, "%x ", scpData[i]);
        }

        tprintf(LEVEL_DEBUG, "\n");
    }
    return ret;
}
#endif


static FLCN_STATUS
s_testTaskTimerCallback(OS_TMR_CALLBACK *pCallback)
{
    tprintf(LEVEL_TRACE, "Callback triggered at tick: %d.\n", lwrtosTaskGetTickCount32());

    return FLCN_OK;
}

static void handleErrMsg(TEST_TASK_ERR_TYPES errCd)
{
    /*!
     * We refrain from dying silently in RISCV-CORE and instead prefer
     * to populate the Error in RM (No More sysTaskExit only)
     */
    tprintf(LEVEL_CRIT, "[%s]: ERR_CODE 0x%x \n", __FUNCTION__, errCd);

    if ((errCd != ERR_NONE) && (errCd < ERR_MAX))
        sysTaskExit(ERR_MSG_LIST[errCd], FLCN_ERROR);
}

extern sysTASK_DATA LwU32 cmdqOffsetShared;
static void handleInitStat(TEST_TASK_ERR_TYPES errCd)
{
    RM_FLCN_MSG_GSP gspMsg;

    // fill-in the header of the TEST message and send it to the msgmgmt.
    gspMsg.hdr.unitId = RM_GSP_UNIT_INIT;
    gspMsg.hdr.ctrlFlags = RM_FLCN_QUEUE_HDR_FLAGS_EVENT;
    gspMsg.hdr.seqNumId = 0;
    gspMsg.hdr.size = RM_GSP_MSG_SIZE(INIT, UNIT_READY);
    gspMsg.msg.init.msgUnitState.msgType = RM_GSP_MSG_TYPE(INIT, UNIT_READY);
    // This is the real unit ID sending ACK back.
    gspMsg.msg.init.msgUnitState.taskId = RM_GSP_TASK_ID_TEST;
    gspMsg.msg.init.msgUnitState.taskStatus = (errCd == ERR_NONE) ? FLCN_OK : FLCN_ERROR;

    handleErrMsg(errCd);

    // This further calls sysSendSwGen(SYS_INTR_SWGEN0);
    lwrtosQueueSendBlocking(rmMsgRequestQueue, &gspMsg, sizeof(RM_FLCN_MSG_GSP));
}

static void
handleTest(LwU8 testType, const RM_GSP_COMMAND *pCmd)
{
    TEST_TASK_ERR_TYPES ret = ERR_NONE;
    RM_FLCN_MSG_GSP gspMsg;
    LwU8 cmdType = pCmd->cmd.test.cmdType;
    LwU32 subCmdType = pCmd->cmd.test.commonTest.subCmdType;

    // By default just send ACK
    gspMsg.hdr = pCmd->hdr;
    gspMsg.msg.test.commonTest.msgType = cmdType;

    switch (subCmdType)
    {
        case RM_UPROC_COMMON_TEST_MEM_OPS:
        {
            ret = testMemOps();
            break;
        }
        case RM_UPROC_COMMON_TEST_DMA:
        {
            ret = testDma();
            break;
        }
        case RM_UPROC_COMMON_TEST_KERNEL_CRITICAL_SECTIONS:
        {
            ret = testKernelCriticalSections();
            break;
        }
        case RM_UPROC_COMMON_TEST_KERNEL_MODE:
        {
            ret = testKernelMode();
            break;
        }
        case RM_UPROC_COMMON_TEST_SCP_RNG:
        {
#ifdef USE_SCPLIB
            ret = testSCP_RNG();
#else
            tprintf(LEVEL_ERROR, "[%s] :: Test Not Supported on Falcon Arch !!!\n", __FUNCTION__);
#endif
            break;
        }
        // TODO: Implemenet the rest of Sub tests Type
        default:
        {
            ret = ERR_ILWALID_TEST_REQUEST;
            tprintf(LEVEL_ERROR, "INVALID Sub-Test request %x received.\n", subCmdType);
        }
    }

    handleErrMsg(ret);

    gspMsg.msg.test.commonTest.status = (ret == ERR_NONE) ? FLCN_OK : FLCN_ERROR;
    lwrtosQueueSendBlocking(rmMsgRequestQueue, &gspMsg, sizeof(gspMsg));
}

static TEST_TASK_ERR_TYPES testMemOps(void)
{
    TEST_TASK_ERR_TYPES ret = ERR_NONE;
// Disable due to not relevant for TSEC, will cause a crash.
#if !RUN_ON_SEC
#if USE_CBB
    //TODO: not sure if bug exists. Also this really needs to be fmodel only
    //      check but we haven't implemented it yet.
    LwBool bug2677468Exists = LW_TRUE;
#else
    LwU32 platform = bar0Read(LW_PTOP_PLATFORM);
    LwBool bug2677468Exists = FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _FMODEL, platform);
#endif

    tprintf(LEVEL_DEBUG, "%s: fence-based flush ops\n", __FUNCTION__);
    SYS_FLUSH_MEM();
    SYS_FLUSH_IO();
    SYS_FLUSH_ALL();

    //
    // Some memops are kernel-only, or require explicit access be granted thru
    // SYSOPEN and MISCOPEN.
    //
    if (FLD_TEST_DRF_NUM64(_PRGNLCL_RISCV, _LWCONFIG, _SYSOP_CSR_EN, 1,
                           intioRead(LW_PRGNLCL_RISCV_LWCONFIG)))
    {
        if (lwrtosIS_PRIV())
        {
            LwU64 val;
    
            tprintf(LEVEL_TRACE, "%s: kernel-level ilwalidate ops\n", __FUNCTION__);
            val = csr_read(LW_RISCV_CSR_SSYSOPEN);
            tprintf(LEVEL_TRACE, "LW_RISCV_CSR_SSYSOPEN=0x%llx\n", val);
            val = csr_read(LW_RISCV_CSR_SMISCOPEN);
            tprintf(LEVEL_TRACE, "LW_RISCV_CSR_SMISCOPEN=0x%llx\n", val);
    
            SYS_DCACHEOP(0);
            #if defined(SYS_MTLBILWOP)
                SYS_MTLBILWOP(0, 0);
            #endif  // defined(SYS_MTLBILWOP)
    
            if (!bug2677468Exists)
            {
                // Skip these for now -- they seem to be hanging on GA100 and GA102 fmodel.
                SYS_L2SYSILW();
                SYS_L2PEERILW();
                SYS_L2CLNCOMP();
                SYS_L2FLHDTY();
            }
        }
        else
        {
            LwU64 savedSMiscOpen;
            LwU64 savedSSysOpen;
    
            tprintf(LEVEL_TRACE, "%s: user-level ilwalidate ops\n", __FUNCTION__);
    
            // Set SSYSOPEN and SMISCOPEN to enable user mode memops
            sysPrivilegeRaise();
            savedSSysOpen = csr_read(LW_RISCV_CSR_SSYSOPEN);
            savedSMiscOpen = csr_read(LW_RISCV_CSR_SMISCOPEN);
            csr_write(LW_RISCV_CSR_SSYSOPEN, savedSSysOpen | 0xFF);
            csr_write(LW_RISCV_CSR_SMISCOPEN, savedSMiscOpen | 0xFF);
            sysPrivilegeLower();
    
            // Do the tests
            SYS_DCACHEOP(0);
            #if defined(SYS_UTLBILWOP)
                SYS_UTLBILWOP(0, 0);
            #endif  // defined(SYS_UTLBILWOP)
    
            if (!bug2677468Exists)
            {
                SYS_L2SYSILW();
                SYS_L2PEERILW();
                SYS_L2CLNCOMP();
                SYS_L2FLHDTY();
            }
    
            // Reset SSYSOPEN and SMISCOPEN to old state
            sysPrivilegeRaise();
            csr_write(LW_RISCV_CSR_SSYSOPEN, savedSSysOpen);
            csr_write(LW_RISCV_CSR_SMISCOPEN, savedSMiscOpen);
            sysPrivilegeLower();
        }
    }

    tprintf(LEVEL_DEBUG, "%s: complete\n", __FUNCTION__);
#endif // !RUN_ON_SEC
    return ret;
}

#define DMA_SIZE    (LW_RISCV_CSR_MPU_PAGE_SIZE)

#if !RUN_ON_SEC && defined(DMA_SIZE)
#define DMA_INDEX   (RM_FLCN_DMAIDX_PHYS_VID_FN0)

#define COPY_SIZE   (DMA_SIZE)
#define MAX_ERRORS  128

// A buffer in FB and a buffer in DMEM
#define BUFFER_SIZE (DMA_SIZE/sizeof(LwU32))
static LwU32 pBuf0[BUFFER_SIZE]
    GCC_ATTRIB_SECTION("taskSharedDataFb", "test_pBuf0") GCC_ATTRIB_ALIGN(RISCV_CACHE_LINE_SIZE);
static LwU32 pBuf1[BUFFER_SIZE]
    GCC_ATTRIB_SECTION("taskSharedDataDmemRes", "test_pBuf1") GCC_ATTRIB_ALIGN(RISCV_CACHE_LINE_SIZE);

#endif  // !RUN_on_SEC && defined(DMA_SIZE)

static TEST_TASK_ERR_TYPES testDma(void)
{
#if !RUN_ON_SEC && defined(DMA_SIZE)
    if (!lwrtosIS_PRIV())
    {
        tprintf(LEVEL_CRIT, "DMA tests must be run in kernel mode.  Aborting\n");
        return ERR_NONE;
    }

    #define GEN_SIGNATURE_0(_val)       (((~_val) << 16) | _val)
    #define GEN_SIGNATURE_1(_val)       (0x11110000 | _val)

    tprintf(LEVEL_DEBUG, "Kernel DMA tests: start\n");
    {
        LwU64 fbpaOffset;
        LwU32 i;
        LwU32 mismatch = 0;
        LwU32 unmodified = 0;

        RM_FLCN_MEM_DESC fbMemDesc;
        FLCN_STATUS status;

        // Scribble in target buffer
        for (i = 0; i < BUFFER_SIZE; i++)
        {
            pBuf0[i] = GEN_SIGNATURE_0(i);
            pBuf1[i] = GEN_SIGNATURE_1(i);
        }

        status = mmVirtToPhysFb((LwU64)pBuf0, &fbpaOffset);
        if (status != FLCN_OK)
        {
            return ERR_KRNL_DMA_TEST_VA2PA_FAILED;
        }

        fbpaOffset -= LW_RISCV_AMAP_FBGPA_START;
#ifdef LW_PRGNLCL_TFBIF_TRANSCFG
        //
        // On CheetAh Engines with a TFBIF, address bits[41:40]
        // are used to select the streamId of the load/store
        // mask these out so that we can get the external address
        //
        fbpaOffset &= ~(0x0000030000000000);
#endif //LW_PRGNLCL_TFBIF_TRANSCFG
        fbMemDesc.address.lo = LwU64_LO32(fbpaOffset);
        fbMemDesc.address.hi = LwU64_HI32(fbpaOffset);
        fbMemDesc.params =
            DRF_NUM(_RM_FLCN, _MEM_DESC, _PARAMS_SIZE, DMA_SIZE) |
            DRF_NUM(_RM_FLCN, _MEM_DESC, _PARAMS_DMA_IDX, DMA_INDEX) |
            0;

#ifdef LW_PRGNLCL_TFBIF_TRANSCFG
        // SWID 2 is used to select streamID 1, so that we can do DMA to a GVA
        status = sysConfigureTfbif(DMA_INDEX, 2, 0, 0);
        if (status != FLCN_OK)
        {
            tprintf(LEVEL_ERROR, "%s: Tfbif config failed\n", __FUNCTION__);
            return ERR_KRNL_DMA_TEST_FB_WRITE_FAILED;
        }
#endif //LW_PRGNLCL_TFBIF_TRANSCFG

        tprintf(LEVEL_TRACE, "%s: Starting FB-to-DMEM DMA test (Read)\n",
                __FUNCTION__);
        status = sysDmaTransfer(pBuf1, &fbMemDesc, 0, COPY_SIZE, LW_TRUE, LW_FALSE);
        if (status != FLCN_OK)
        {
            tprintf(LEVEL_ERROR, "%s: FB-to-DMEM DMA failed\n", __FUNCTION__);
            return ERR_KRNL_DMA_TEST_FB_WRITE_FAILED;
        }

        for (i = 0; i < COPY_SIZE/sizeof(LwU32); i++)
        {
            if (pBuf0[i] != pBuf1[i])
            {
                mismatch++;
                tprintf(LEVEL_ERROR,
                        "Kernel FB-to-DMEM DMA compare mismatch @%3x: pBuf0=0x%x != pBuf1=0x%x\n",
                        i, pBuf0[i], pBuf1[i]);
            }

            if (pBuf1[i] == GEN_SIGNATURE_1(i))
            {
                unmodified++;
                tprintf(LEVEL_ERROR,
                        "Kernel DMA compare unmodified @%3x: pBuf1=0x%x\n",
                        i, pBuf1[i]);
            }

            if (mismatch + unmodified > MAX_ERRORS)
            {
                tprintf(LEVEL_ERROR,
                        "Kernel DMA compare: too many errors.  Aborting.\n");
                break;
            }
        }

        if ((mismatch != 0) || (unmodified != 0))
        {
            tprintf(LEVEL_ERROR, "Kernel DMA FB-to-DMEM tests: %d mismatches\n", mismatch);
            tprintf(LEVEL_ERROR, "Kernel DMA FB-to-DMEM tests: %d unmodified\n", unmodified);
            return ERR_KRNL_DMA_TEST_FB_WRITE_MATCH_FAILED;
        }

        // Scribble in target buffer
        for (i = 0; i < BUFFER_SIZE; i++)
        {
            pBuf0[i] = GEN_SIGNATURE_0(i);
            pBuf1[i] = GEN_SIGNATURE_1(i);
        }

        tprintf(LEVEL_TRACE, "%s: Starting DMEM-to-FB DMA test (Write)\n",
                __FUNCTION__);
        status = sysDmaTransfer(pBuf1, &fbMemDesc, 0, COPY_SIZE, LW_FALSE, LW_FALSE);
        if (status != FLCN_OK)
        {
            tprintf(LEVEL_ERROR, "%s: DMEM-to-FB DMA failed\n", __FUNCTION__);
            return ERR_KRNL_DMA_TEST_FB_WRITE_FAILED;
        }

        for (i = 0; i < COPY_SIZE/sizeof(LwU32); i++)
        {
            if (pBuf0[i] != pBuf1[i])
            {
                mismatch++;
                tprintf(LEVEL_ERROR,
                        "Kernel DMEM-to-FB DMA compare mismatch @%3x: pBuf0=0x%x != pBuf1=0x%x\n",
                        i, pBuf0[i], pBuf1[i]);
            }

            if (pBuf0[i] == GEN_SIGNATURE_0(i))
            {
                unmodified++;
                tprintf(LEVEL_ERROR,
                        "Kernel DMA compare unmodified @%3x: pBuf0=0x%x\n",
                        i, pBuf0[i]);
            }

            if (mismatch + unmodified > MAX_ERRORS)
            {
                tprintf(LEVEL_ERROR,
                        "Kernel DMA compare: too many errors.  Aborting.\n");
                break;
            }
        }

        if ((mismatch != 0) || (unmodified != 0))
        {
            tprintf(LEVEL_ERROR, "Kernel DMA DMEM-to-FB tests: %d mismatches\n", mismatch);
            tprintf(LEVEL_ERROR, "Kernel DMA DMEM-to-FB tests: %d unmodified\n", unmodified);
            return ERR_KRNL_DMA_TEST_FB_WRITE_MATCH_FAILED;
        }

        tprintf(LEVEL_TRACE, "%s: DMA DMEM-to-FB test complete.\n", __FUNCTION__);
    }
    tprintf(LEVEL_DEBUG, "Kernel DMA tests: end\n");
#else   // !RUN_ON_SEC && defined(DMA_SIZE)
    tprintf(LEVEL_DEBUG, "Skipping kernel DMA test\n");
#endif  // !RUN_ON_SEC && defined(DMA_SIZE)
    return ERR_NONE;
}

#define SIE_INTERRUPTS_ALL  (DRF_NUM64(_RISCV_CSR, _SIE, _SEIE, 1) | \
                             DRF_NUM64(_RISCV_CSR, _SIE, _STIE, 1) | \
                             DRF_NUM64(_RISCV_CSR, _SIE, _SSIE, 1))

static TEST_TASK_ERR_TYPES testKernelCriticalSections(void)
{
    TEST_TASK_ERR_TYPES ret = ERR_NONE;
    tprintf(LEVEL_TRACE, "Testing critical sections... Kernel-mode\n");

    // Timer interrupt should be enabled
    if ((csr_read(LW_RISCV_CSR_SIE) & SIE_INTERRUPTS_ALL) != SIE_INTERRUPTS_ALL)
    {
        handleInitStat(ERR_INTR_DISABLED);
    }

    // Critical section should disable timer interrupt
    lwrtosENTER_CRITICAL();
    if ((csr_read(LW_RISCV_CSR_SIE) & SIE_INTERRUPTS_ALL) != 0)
    {
        handleInitStat(ERR_CRTCL_SECTN_INTR_ENABLED);
    }
    lwrtosEXIT_CRITICAL();
    if ((csr_read(LW_RISCV_CSR_SIE) & SIE_INTERRUPTS_ALL) != SIE_INTERRUPTS_ALL)
    {
        handleInitStat(ERR_CRTCL_SECTN_EXIT_INTR_DISABLED);
    }

    tprintf(LEVEL_TRACE, "Tested critical sections: interrupts ... kernel-mode\n");
    return ret;
}

#if ODP_ENABLED
GCC_ATTRIB_SECTION("odpTestData", "c_odpTestDataRead")
static volatile char s_odpTestData[] = "Foobar";
GCC_ATTRIB_SECTION("odpTestData", "s_odpPrgTestData")
static volatile int (*s_odpPrgTestData)(int, int);

static char c_odpTestDataOriginal1[] = "Foobar";
static char c_odpTestDataOriginal2[] = "fOOBAR";

GCC_ATTRIB_SECTION("odpTestCode", "odpPrgTest")
static int odpPrgTest(int a, int b)
{
    return a + b;
}

static TEST_TASK_ERR_TYPES testOdp(LwBool bIsKernel)
{
    unsigned int passCount = 0;
    unsigned int testCount = 0;
    LwBool pass;

    tprintf(LEVEL_DEBUG, "Testing ODP (%s mode)\n", bIsKernel ? "KERNEL" : "USER");

    tprintf(LEVEL_TRACE, "ODP: Check regular...\n");
    pass = LW_TRUE;
    for (int i = 0; i < 6; i++)
    {
        char orig = c_odpTestDataOriginal1[i];
        char test = s_odpTestData[i];
        if (test != orig)
        {
            tprintf(LEVEL_ERROR,
                    "ODP: Couldn't match test data1 %d (%02X != %02X)\n", i,
                    test, orig);
            pass = LW_FALSE;
        }
        else
        {
            s_odpTestData[i] ^= 0x20;
        }
    }

    testCount++;
    if (pass)
    {
        passCount++;
    }

    if (bIsKernel)
    {
        tprintf(LEVEL_TRACE, "ODP: Flush ilwalidate...\n");
        odpFlushIlwalidateRange((void*)s_odpTestData, sizeof(s_odpTestData));
    }

    tprintf(LEVEL_TRACE, "ODP: Check ilwert...\n");
    pass = LW_TRUE;
    for (int i = 0; i < 6; i++)
    {
        char orig = c_odpTestDataOriginal2[i];
        char test = s_odpTestData[i];
        if (test != orig)
        {
            tprintf(LEVEL_ERROR,
                    "ODP: Couldn't match test data2 %d (%02X != %02X)\n", i,
                    test, orig);
            pass = LW_FALSE;
        }
        else
        {
            s_odpTestData[i] ^= 0x20;
        }
    }

    testCount++;
    if (pass)
    {
        passCount++;
    }

    if (bIsKernel)
    {
        tprintf(LEVEL_TRACE, "ODP: Flush...\n");
        odpFlushRange((void*)s_odpTestData, sizeof(s_odpTestData));
    }

    tprintf(LEVEL_TRACE, "ODP: Check revert...\n");
    pass = LW_TRUE;
    for (int i = 0; i < 6; i++)
    {
        char orig = c_odpTestDataOriginal1[i];
        char test = s_odpTestData[i];
        if (test != orig)
        {
            tprintf(LEVEL_ERROR,
                    "ODP: Couldn't match test data1 %d (%02X != %02X)\n", i,
                    test, orig);
            pass = LW_FALSE;
        }
    }

    testCount++;
    if (pass)
    {
        passCount++;
    }

    s_odpPrgTestData = odpPrgTest;
    if (bIsKernel)
    {
        tprintf(LEVEL_TRACE, "ODP: Flush ilwalidate...\n");
        odpFlushIlwalidateAll();
    }

    tprintf(LEVEL_TRACE, "ODP: Check program\n");
    pass = LW_TRUE;
    for (int i = 0; i < 10; i++)
    {
        int expected = 28 + i;
        int result = s_odpPrgTestData(i + 8, 20);
        if (result != expected)
        {
            tprintf(LEVEL_ERROR,
                    "ODP: Couldn't get proper result from program %d (%d != %d)\n",
                    i, result, expected);
            pass = LW_FALSE;
        }
    }

    testCount++;
    if (pass)
    {
        passCount++;
    }

    if (bIsKernel)
    {
        tprintf(LEVEL_TRACE, "ODP: Flush ilwalidate...\n");
        odpFlushIlwalidateAll();
    }

    tprintf(LEVEL_DEBUG, "ODP: Passed: %d/%d\n", passCount, testCount);
    return (passCount == testCount) ? ERR_NONE : ERR_ODP_FAILED;
}
#endif

static TEST_TASK_ERR_TYPES testKernelMode(void)
{
    TEST_TASK_ERR_TYPES ret = ERR_NONE;
    tprintf(LEVEL_DEBUG, "Testing Kernel mode\n");

    // Sanity check in M mode - we can access MIE
    sysPrivilegeRaise();

    // Test Kernel Critical Sections.
    ret = testKernelCriticalSections();
    if (ret)
        return ret;

    // Test kernel mode memops
    ret = testMemOps();
    if (ret)
        return ret;

    // Test basic DMA
    ret = testDma();
    if (ret)
        return ret;

#if ODP_ENABLED
    ret = testOdp(LW_TRUE);
    if (ret)
        return ret;
#endif

    sysPrivilegeLower();

    tprintf(LEVEL_DEBUG, "Kernel mode sanity PASSED.\n");
    return ret;
}

lwrtosTASK_FUNCTION(testMain, pvParameters)
{
    LwU32 tmrCallbackCount = 0;
    TEST_TASK_ERR_TYPES ret = ERR_NONE;

    tprintf(LEVEL_INFO, "started.\n");

    tprintf(LEVEL_DEBUG, "Start testing timer callback...\n");
    memset(&testTaskOsTmrCb, 0, sizeof(OS_TMR_CALLBACK));
    osTmrCallbackCreate(&testTaskOsTmrCb,                   // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_NO_SKIP,
                                                            // type
                0,                                          // ovlImem
                s_testTaskTimerCallback,                    // pTmrCallbackFunc
                testRequestQueue,                           // queueHandle
                100,                                        // periodNormalus
                100,                                        // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,           // bRelaxedUseSleep
                RM_GSP_UNIT_TEST);                          // taskId
    osTmrCallbackSchedule(&testTaskOsTmrCb);

    tprintf(LEVEL_INFO, "Testing kernel mode...\n");
    ret = testKernelMode();
    if (ret != ERR_NONE)
        handleInitStat(ret);

    tprintf(LEVEL_INFO, "Testing user mode...\n");
    // Test user mode memops
    ret = testMemOps();
    if (ret != ERR_NONE)
        handleInitStat(ret);

#if ODP_ENABLED
    if ((ret = testOdp(LW_FALSE)) != ERR_NONE)
        handleInitStat(ret);
#endif

#if PARTITION_SWITCH_TEST
    // TODO: cleanup partition switch test and time measurement.
    if (!partitionSwitchTest())
        handleInitStat(ERR_PART_SWITCH_FAILED);
#endif

    // Print debug spew so that we always cover at least part of debugging code.
#if LWRISCV_DEBUG_PRINT_LEVEL > LEVEL_INFO
    syscall0(LW_APP_SYSCALL_OOPS);
#endif

    while(LW_TRUE)
    {
        DISP2UNIT_CMD disp2Unit;
        ret = lwrtosQueueReceive(testRequestQueue, &disp2Unit, sizeof(disp2Unit), lwrtosMAX_DELAY);
        if (ret == FLCN_OK)
        {
            if (disp2Unit.eventType == DISP2UNIT_EVT_OS_TMR_CALLBACK)
            {
                tprintf(LEVEL_TRACE, "Handle callback event.\n");
                tmrCallbackCount++;
                // For test purpose, only respond to first 10 callbacks
                if (tmrCallbackCount < 10)
                {
                    osTmrCallbackExelwte(&testTaskOsTmrCb);
                }
                else
                {
                    osTmrCallbackCancel(&testTaskOsTmrCb);
                    if (tmrCallbackCount > 10)
                    {
                        handleInitStat(ERR_TMR_CALLBACK);
                    }
                    // Already received sufficient Timer callbacks.Ack 'no-error' here.
                    handleInitStat(ERR_NONE);
                }
            }
            else
            {
                const RM_GSP_COMMAND *pCmd = disp2Unit.pCmd;
                // MK TODO: that should not be checked as cmdmgmt does it
                if (pCmd->hdr.unitId == RM_GSP_UNIT_TEST)
                {
                    handleTest(pCmd->cmd.test.cmdType, pCmd);
                }
                else
                {
                    tprintf(LEVEL_ERROR, "Message to other unit received: %d\n",
                            pCmd->hdr.unitId);
                }
            }
        }
        else
        {
            handleInitStat(ERR_FAILED_WAITING_ON_QUEUE);
        }
    }
}
