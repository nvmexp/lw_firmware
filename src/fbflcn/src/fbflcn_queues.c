/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>

// include manuals
#include "dev_fbfalcon_csb.h"
#include "dev_pwr_pri.h"
#include "dev_top.h"
#include "dev_fuse.h"
#include "dev_fbpa.h"

#include "lwuproc.h"

#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_interrupts.h"
#include "fbflcn_queues.h"
#include "fbflcn_hbm_mclk_switch.h"
#include "fbflcn_dummy_mclk_switch.h"
#include "priv.h"
#include "revocation.h"

#include "rmfbflcncmdif.h"
#include "pmu_fbflcn_if.h"

#include "osptimer.h"
#include "fbflcn_objfbflcn.h"
extern OBJFBFLCN Fbflcn;
#include "config/g_fbfalcon_private.h"

extern LwU8 gPlatformType;
extern LW_STATUS initStatus;

#define FBFLCN_CMD_QUEUE_BUFFERINX_RM2FBFLCN 2
#define FBFLCN_CMD_QUEUE_BUFFERINX_PMU2FBFLCN 0
#define FBFLCN_CMD_QUEUE_BUFFERINX_FBFLCN2RM 3
#define FBFLCN_CMD_QUEUE_BUFFERINX_FBFLCN2PMU 1

// This value should be kept in sync with PMU LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_EXCEPTION_TFAW_VAL !
#define PERF_PERF_CF_MEM_TUNE_EXCEPTION_TFAW_VAL       80U

/* ------------------------ Global variables ------------------------------- */

FBFLCN_QUEUE_RESOURCE gQueueResource[FBFLCN_CMD_QUEUE_NUM];



/* ------------------------ Public Functions -------------------------------- */
/*!
 * the queueId (or order in the struct needs to have all the internal command
 * queues that map to an interrupt first, the queue number on the interrupt is
 * directly used as queue Id to lookup in the queue struct.
 */


#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))

/*!
 * doStateUnloadPAFalcon propagates the state unload request to the pafalcon to
 * properly suspend the pafalcon operations similar to the fbfalcon
 */

GCC_ATTRIB_SECTION("resident", "doStateUnloadPAFalcon")
static void
doStateUnloadPAFalcon(void)
{
#include "dev_pafalcon_pri.h"
    extern LwBool gbl_en_fb_mclk_sw;

    // Send halt to PA
    if (!gbl_en_fb_mclk_sw)
    {
        // mask out start bit
        LwU32 syncCtrl = REG_RD32(BAR0, LW_PPAFALCON_SYNC_CTRL) & 0xFFFFFFFE;
        syncCtrl = (syncCtrl & 0x0000FFFF) | (FBFLCN_ERROR_CODE_FBFLCN_HALT_REQUEST_EXELWTED << 16);

        // mask halt interrupt in PA
        while (FLD_TEST_DRF(_PPAFALCON, _FALCON_IRQMASK, _HALT, _ENABLE,
                REG_RD32(BAR0, LW_PPAFALCON_FALCON_IRQMASK)))
        {
            REG_WR32_STALL(BAR0, LW_PPAFALCON_FALCON_IRQMCLR, 0x00000000 |
                    DRF_DEF(_PPAFALCON,_FALCON_IRQMCLR,_HALT,_SET)
            );
        }
        REG_WR32(BAR0, LW_PPAFALCON_SYNC_CTRL, syncCtrl);

#ifdef LW_CFBFALCON_FALCON_RESET_PRIV_LEVEL_MASK
        LwU32 plm = REG_RD32(BAR0, LW_PPAFALCON_FALCON_RESET_PRIV_LEVEL_MASK);
        plm = FLD_SET_DRF(_PPAFALCON,_FALCON_RESET_PRIV_LEVEL_MASK,_READ_PROTECTION,_ALL_LEVELS_ENABLED,plm);
        plm = FLD_SET_DRF(_PPAFALCON,_FALCON_RESET_PRIV_LEVEL_MASK,_WRITE_PROTECTION,_ALL_LEVELS_ENABLED,plm);
        REG_WR32_STALL(BAR0, LW_PPAFALCON_FALCON_RESET_PRIV_LEVEL_MASK,plm);
#endif
    }
}
#endif


/*!
 * doLaunchMclkSwitch is a wrapper around the mclk switch when requested from
 * the cmd queue
 */
GCC_ATTRIB_SECTION("resident", "doLaunchMclkSwitch")
static LwU32
doLaunchMclkSwitch
(
        LwU32 targetFreqMHz
)
{
    LwU32 fbStopTimeNs = 0x0;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM2))
    lock();
    fbStopTimeNs = doMclkSwitch(targetFreqMHz);
    unlock();
    return fbStopTimeNs;
#else

    // support dummy mclk switch, limited to Ampere and beyond due to availability of the PTOP PLATFORM
    // register - Bug 2712837
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    if ((gPlatformType == LW_PTOP_PLATFORM_TYPE_FMODEL) ||
            (gPlatformType == LW_PTOP_PLATFORM_TYPE_EMU)) {
	lock();
	fbStopTimeNs = doDummyMclkSwitch(targetFreqMHz);
	unlock();
	return fbStopTimeNs;
    }
    
    // block the mclk switch exelwtion when initState indicates a problem.
    if (initStatus != LW_OK)
    {
        FBFLCN_HALT((initStatus << 16)|FBFLCN_ERROR_CODE_INIT_STATUS_ERROR);
    }
#endif

    LwU8 tableInx = 0x0;
    fbfalconCheckRevocation_HAL();
    tableInx = fbfalconFindFrequencyTableEntry_HAL(&Fbflcn,targetFreqMHz);

    if (tableInx != 0xff)
    {
        lock();
        fbStopTimeNs = doMclkSwitch(targetFreqMHz);
        unlock();
    }

    REG_WR32(CSB, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(0),LW_CEIL(fbStopTimeNs, 1000));

    return fbStopTimeNs;
#endif
}


void
initQueueResources
(
        void
)
{

    // Setup the command queue[0] as the RM-2-FBFALCN command channel.
    PFBFLCN_QUEUE_RESOURCE pQR = &gQueueResource[0];
    pQR->headPriv            = LW_CFBFALCON_CMD_QUEUE_HEAD(0);
    pQR->tailPriv            = LW_CFBFALCON_CMD_QUEUE_TAIL(0);
    pQR->response            = FBFLCN_CMD_QUEUE_FBFLCN2RM;
    pQR->flags.internal      = 1;
    pQR->flags.swgen_irq     = 0;
    pQR->flags.enabled       = 1;

    // Setup the command queue[1] as the PMU-2-FBFALCN command channel.
    pQR++;
    pQR->headPriv            = LW_CFBFALCON_CMD_QUEUE_HEAD(1);
    pQR->tailPriv            = LW_CFBFALCON_CMD_QUEUE_TAIL(1);
    pQR->response            = FBFLCN_CMD_QUEUE_FBFLCN2PMU;
    pQR->flags.internal      = 1;
    pQR->flags.swgen_irq     = 0;
    pQR->flags.enabled       = 1;

    // Setup the message queue as the FBFALCN-2-RM command channel.
    pQR++;
    pQR->headPriv            = LW_CFBFALCON_MSGQ_HEAD(0);
    pQR->tailPriv            = LW_CFBFALCON_MSGQ_TAIL(0);
    pQR->response            = -1;
    pQR->flags.internal      = 1;
    pQR->flags.swgen_irq     = 1;
    pQR->flags.enabled       = 1;

    // Setup the PMU command queue[2] as the FBFLCN-2-PMU command channel.
    pQR++;
    pQR->headPriv            = LW_PPWR_PMU_QUEUE_HEAD(PMU_CMD_QUEUE_IDX_FBFLCN);
    pQR->tailPriv            = LW_PPWR_PMU_QUEUE_TAIL(PMU_CMD_QUEUE_IDX_FBFLCN);
    pQR->response            = -1;
    pQR->flags.internal      = 0;
    pQR->flags.swgen_irq     = 0;
    pQR->flags.enabled       = 0;

#if (FBFALCONCFG_FEATURE_ENABLED(MINING_LOCK_SUPPORT))
    // Setup the command queue[2] as the 2nd PMU-2-FBFALCN command channel.
    pQR++;
    pQR->headPriv            = LW_CFBFALCON_CMD_QUEUE_HEAD(2);
    pQR->tailPriv            = LW_CFBFALCON_CMD_QUEUE_TAIL(2);
    pQR->response            = -1;
    pQR->flags.internal      = 1;
    pQR->flags.swgen_irq     = 0;
    pQR->flags.enabled       = 1;
#endif
}


void
fbflcnQInterruptHandler
(
        void
)
{
    LwU32 intrstat = REG_RD32(CSB, LW_CFBFALCON_CMD_HEAD_INTRSTAT);

    if (intrstat != 0)
    {
        // CMD queue interrupt hit, save queue location and record event.
        if (FLD_TEST_DRF(_CFBFALCON, _CMD_HEAD_INTRSTAT, _0, _UPDATED, intrstat))
        {
            LwU32 temp = DRF_DEF(_CFBFALCON, _CMD_HEAD_INTRSTAT, _0, _CLEAR);
            REG_WR32_STALL(CSB, LW_CFBFALCON_CMD_HEAD_INTRSTAT, temp);
            intrstat = FLD_SET_DRF(_CFBFALCON, _CMD_HEAD_INTRSTAT, _0, _INIT, intrstat);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
            enableIntEvent(EVENT_MASK_CMD_QUEUE0);
#else
            enableEvent(EVENT_MASK_CMD_QUEUE0);
#endif
        }
        if (FLD_TEST_DRF(_CFBFALCON, _CMD_HEAD_INTRSTAT, _1, _UPDATED, intrstat))
        {
            LwU32 temp = DRF_DEF(_CFBFALCON, _CMD_HEAD_INTRSTAT, _1, _CLEAR);
            REG_WR32_STALL(CSB, LW_CFBFALCON_CMD_HEAD_INTRSTAT, temp);
            intrstat = FLD_SET_DRF(_CFBFALCON, _CMD_HEAD_INTRSTAT, _1, _INIT, intrstat);
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
            enableIntEvent(EVENT_MASK_CMD_QUEUE1);
#else
            enableEvent(EVENT_MASK_CMD_QUEUE1);
#endif
        }
#if (FBFALCONCFG_FEATURE_ENABLED(MINING_LOCK_SUPPORT))
        if (FLD_TEST_DRF(_CFBFALCON, _CMD_HEAD_INTRSTAT, _2, _UPDATED, intrstat))
        {
            LwU32 temp = DRF_DEF(_CFBFALCON, _CMD_HEAD_INTRSTAT, _2, _CLEAR);
            REG_WR32_STALL(CSB, LW_CFBFALCON_CMD_HEAD_INTRSTAT, temp);
            intrstat = FLD_SET_DRF(_CFBFALCON, _CMD_HEAD_INTRSTAT, _2, _INIT, intrstat);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
            enableIntEvent(EVENT_MASK_CMD_QUEUE2);
#else
            enableEvent(EVENT_MASK_CMD_QUEUE2);
#endif
        }
#endif  // #if (FBFALCONCFG_FEATURE_ENABLED(MINING_LOCK_SUPPORT))

        // check that all interrupts have been processed.
        if (intrstat != 0)
        {
            FBFLCN_HALT(FBFLCN_ERROR_CODE_FBFLCN_HALT_UNPROCESSED_INTERRUPT)
        }
    }
}


/*!
 * Initializes the fbfalcons queues, sets up interrupts and sends init messages
 * to RM to acknowledge proper start.
 */
void
fbflcnQInitDirect(void)
{
    // turn off the mailbox interrupt enable while we setup the queue regs
    REG_WR32_STALL(CSB, LW_CFBFALCON_CMD_INTREN, 0x0);

    // clear cmd intrastat
    REG_WR32_STALL(CSB, LW_CFBFALCON_CMD_HEAD_INTRSTAT, 0xF);
    // enable CMD queue interrupt masks.
    LwU32 cmdIntren = 0;
    // interrupt for queue 0: reserved for rm to fbfalcon
    cmdIntren = FLD_SET_DRF(_CFBFALCON,_CMD_INTREN,_HEAD_0_UPDATE,_ENABLED,cmdIntren);
    // interrupt for queue 1: reserved for pmu to fbfalcon
    cmdIntren = FLD_SET_DRF(_CFBFALCON,_CMD_INTREN,_HEAD_1_UPDATE,_ENABLED,cmdIntren);
#if (FBFALCONCFG_FEATURE_ENABLED(MINING_LOCK_SUPPORT))
    cmdIntren = FLD_SET_DRF(_CFBFALCON,_CMD_INTREN,_HEAD_2_UPDATE,_ENABLED,cmdIntren);
#endif
    REG_WR32_STALL(CSB, LW_CFBFALCON_CMD_INTREN, cmdIntren);

// return early for single binary boot training (gh100)
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_BOOT_TRAINING))
    return;
#endif
    PFBFLCN_QUEUE_RESOURCE pQR = &gQueueResource[FBFLCN_CMD_QUEUE_FBFLCN2RM];

    //LwU32 dataTail = 0x0;
    LwU32 dataHead = REF_NUM(LW_RM_FBFLCN_HEAD_SEQ, 0x0) |
            REF_NUM(LW_RM_FBFLCN_HEAD_CMD, RM_FBFLCN_UNIT_INIT) |
            REF_NUM(LW_RM_FBFLCN_HEAD_TYPE, LW_RM_FBFLCN_HEAD_TYPE_EVENT) |
            REF_NUM(LW_RM_FBFLCN_HEAD_DATA16, 0x0);

    //REG_WR32(CSB, pQR->tailPriv, dataTail);
    REG_WR32(CSB, pQR->headPriv, dataHead);

    if (pQR->flags.swgen_irq == 1)
    {
        REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQSSET,
                DRF_SHIFTMASK(LW_CFBFALCON_FALCON_IRQSSET_SWGEN1));
    }
}



/**
 * exelwteFbfalconHaltRequest
 * code to halt the fbfalcon on request without halt interrutp to rm
 */
GCC_ATTRIB_SECTION("resident", "exelwteFbfalconHaltRequest")
void
exelwteFbfalconHaltRequest
(
        void
)
{
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    doStateUnloadPAFalcon();
#endif

    // mask halt interrupt
    maskHaltInterrupt();

    // lower plm on engine reset (anyone can reset the falcon now)
    //  full engine reset was added at the same time that the code base moved to LS and
    //  required the reset downgrade on state unload
#ifdef LW_CFBFALCON_FALCON_RESET_PRIV_LEVEL_MASK
    LwU32 plm = REG_RD32(CSB, LW_CFBFALCON_FALCON_RESET_PRIV_LEVEL_MASK);
    plm = FLD_SET_DRF(_CFBFALCON,_FALCON_RESET_PRIV_LEVEL_MASK,_READ_PROTECTION,_ALL_LEVELS_ENABLED,plm);
    plm = FLD_SET_DRF(_CFBFALCON,_FALCON_RESET_PRIV_LEVEL_MASK,_WRITE_PROTECTION,_ALL_LEVELS_ENABLED,plm);
    REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_RESET_PRIV_LEVEL_MASK,plm);
#endif

    // Halt the falcon
    FBFLCN_HALT(FBFLCN_ERROR_CODE_FBFLCN_HALT_REQUEST_EXELWTED)
}


#if (FBFALCONCFG_FEATURE_ENABLED(MINING_LOCK_SUPPORT))
/**
 * Processes command in queue2 only for a dedicated halt request from pmu
 *
 * @param queueId - the secondary pmu queue id (all other queues are rejected)
 *
 * possible improvments: As all the queues have moved to the head/tail register
 * approach only, this could could be joined with the main processCmdQueueDirect
 * function and would result in binary size reduction. With the mechanism still
 * being in development I opted to not do this for now (stefans)
 */

void
processCmdQueue2DirectForHalt
(
        LwU32 queueId
)
{
    if (queueId != FBFLCN_CMD_QUEUE_PMU2FBFLCN_SECONDARY)
    {
        FW_MBOX_WR32(13,queueId);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_QUEUE_DIRECT_QUEUE_ACCESS_VIOLATION)
    }

    PFBFLCN_QUEUE_RESOURCE pQR = &gQueueResource[queueId];
    LwU32 head = REG_RD32(BAR0, pQR->headPriv);

    //LwU32 seq = REF_VAL(LW_PMU_FBFLCN_HEAD_SEQ,head);
    LwU32 cmd = REF_VAL(LW_PMU_FBFLCN_HEAD_CMD,head);
    //LwU32 cya = REF_VAL(LW_PMU_FBFLCN_HEAD_CYA,head);
    //LwU32 data16 = REF_VAL(LW_PMU_FBFLCN_HEAD_DATA16,head);

    switch(cmd)
    {
    case PMU_FBFLCN_CMD_ID_PMU_HALT_NOTIFY:
    {
        // check feature fuse
        if (REG_RD32(BAR0,LW_FUSE_SPARE_BIT_1) == LW_FUSE_SPARE_BIT_1_DATA_ENABLE)
        {
            // program tFaw to hardcoded value of PERF_PERF_CF_MEM_TUNE_EXCEPTION_TFAW_VAL
            LwU32 config3 = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG3);
            LwU32 slowedtFAW = PERF_PERF_CF_MEM_TUNE_EXCEPTION_TFAW_VAL;
            config3 = FLD_SET_DRF_NUM(_PFB_FBPA, _CONFIG3, _FAW, slowedtFAW, config3);
            REG_WR32(BAR0,LW_PFB_FBPA_CONFIG3,config3);

            exelwteFbfalconHaltRequest();
        }
    }
    break;
    }
}
#endif  // FBFALCONCFG_FEATURE_ENABLED(MINING_LOCK_SUPPORT)


/**
 * Processes command queue messages for the head/tail register direct model (w/o a data queueu)
 *
 * @param queueId - ID of the queue to be processes
 */

GCC_ATTRIB_SECTION("resident", "processCmdQueueDirect")
void
processCmdQueueDirect
(
        LwU32 queueId
)
{
    PFBFLCN_QUEUE_RESOURCE pQR = &gQueueResource[queueId];
    LwU32 tail = REG_RD32(BAR0, pQR->tailPriv);
    LwU32 head = REG_RD32(BAR0, pQR->headPriv);

    LwU32 seq = REF_VAL(LW_PMU_FBFLCN_HEAD_SEQ,head);
    LwU32 cmd = REF_VAL(LW_PMU_FBFLCN_HEAD_CMD,head);
    LwU32 cya = REF_VAL(LW_PMU_FBFLCN_HEAD_CYA,head);
    LwU32 data16 = REF_VAL(LW_PMU_FBFLCN_HEAD_DATA16,head);
    LwU32 data32 = tail;

    if (queueId == FBFLCN_CMD_QUEUE_PMU2FBFLCN)
    {
        switch(cmd)
        {
        case PMU_FBFLCN_CMD_ID_MCLK_SWITCH:
        {
            LwU32 fbStopTimeNs = doLaunchMclkSwitch(data32 / 1000);

            // prepare response
            if (fbStopTimeNs == 0) {
                data16  = FBFLCN_PMU_MCLK_SWITCH_CMD_STATUS_FREQ_NOT_SUPPORTED;
            } else {
                data16  = FBFLCN_PMU_MCLK_SWITCH_CMD_STATUS_OK;
            }
            LwU32 dataTail = LW_CEIL(fbStopTimeNs, 1000);

            LwU32 dataHead = 0x0;
            dataHead = REF_NUM(LW_PMU_FBFLCN_HEAD_SEQ, seq) |
                    REF_NUM(LW_PMU_FBFLCN_HEAD_CMD, cmd) |
                    REF_NUM(LW_PMU_FBFLCN_HEAD_CYA, cya) |
                    REF_NUM(LW_PMU_FBFLCN_HEAD_DATA16, data16);

            PFBFLCN_QUEUE_RESOURCE pQRResponse = &gQueueResource[pQR->response];
            REG_WR32(BAR0, pQRResponse->tailPriv, dataTail);
            REG_WR32(BAR0, pQRResponse->headPriv, dataHead);
            break;
        }
        break;
        }
    }

    if (queueId == FBFLCN_CMD_QUEUE_RM2FBFLCN)
    {
        switch(cmd)
        {
        case RM_FBFLCN_UNIT_UNLOAD:
        {
            // prepare response
            //LwU32 dataTail = 0x0;
            LwU32 data16 = 0x0;
            LwU32 dataHead = 0x0;
            dataHead = REF_NUM(LW_RM_FBFLCN_HEAD_SEQ, seq) |
                    REF_NUM(LW_RM_FBFLCN_HEAD_CMD, cmd) |
                    REF_NUM(LW_RM_FBFLCN_HEAD_TYPE, LW_RM_FBFLCN_HEAD_TYPE_RESPONSE) |
                    REF_NUM(LW_RM_FBFLCN_HEAD_DATA16, data16);

            PFBFLCN_QUEUE_RESOURCE pQRResponse = &gQueueResource[pQR->response];
            //REG_WR32(CSB, pQRResponse->tailPriv, dataTail);
            REG_WR32(CSB, pQRResponse->headPriv, dataHead);

            if (pQRResponse->flags.swgen_irq == 1)
            {
                REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_IRQSSET,
                        DRF_SHIFTMASK(LW_CFBFALCON_FALCON_IRQSSET_SWGEN1));
            }

            exelwteFbfalconHaltRequest();
        }
        break;
        }

    }
}








