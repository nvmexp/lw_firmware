/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJMS_H
#define PMU_OBJMS_H

/*!
 * @file pmu_objms.h
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"

/* ------------------------ Application includes --------------------------- */
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objtimer.h"
#include "config/g_ms_hal.h"

/* ------------------------- Macros and Defines ---------------------------- */
/*!
 * @brief Get OBJSWASR
 */
#define MS_GET_SWASR()                        (Ms.pSwAsr)

/*!
 * @brief Get clock Gating Mask for MSCG.
 *
 * As a part of SW ASR Sequence we gate the DRAM Clocks (i.e MCLK). DRAM clocks
 * are not the part of Ms.cgMask. This mask only represents the GPC/XBAR/LTC
 * clocks, which we gate as a part of MS Clock Gating sequence.
 *
 * Therefore, we need to add the mask of MCLK domain as well before we send the
 * clock domain mas which are gated during the MSCG.
 */
#define MS_GET_CG_MASK()                      (Ms.cgMask | LW2080_CTRL_CLK_DOMAIN_MCLK)

/*!
 * @brief Get MS dynamic current coeff of the specified rail
 */
#define msDynamicLwrrentCoeffGet(railId)      (Ms.dynamicLwrrentCoeff[railId])

#define XVE_REG(a) (DEVICE_BASE(LW_PCFG)+a)

/*!
 * @brief XVE Blocker timeout value for EMULATION
 */
#define MS_XVE_BLOCKER_TIMEOUT_EMU_US         0xFFFF

//
// For DIFR, we need to consider both of the FSM i.e
// Layer 2 FSM - DIFR_SW_ASR
// Layer 3 FSM - DIFR_CG
//
// These FSM will always be enabled/disable together
//
#define MS_DIFR_FSM_MASK                                \
         (LWBIT(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR) |   \
          LWBIT(RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG))

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */

/*!
 * @brief : Register cache used by SW-ASR
 *
 * Structure defines register cache used by SW-ASR sequence. SW-ASR Entry
 * sequence uses this cache to store HW state before putting DRAM into self
 * refresh.
 */
typedef struct SWASRREGS
{
    LwU32  fbpaCfg0;
    LwU32  fbpaZQ;
    LwU32  fbpaRefCtrl;
    LwU32  fbpaFbioCfgPwrd;
    LwU32  fbpaFbioBytePadCtrl2;
    LwU32  fbpaFbioCmdPadCtrl;
    LwU32  fbpaFbioCfgBrickVauxgen;
    LwU32  fbpaFbioCfgVttgelwauxgen;
    LwU32  sysFbioSpare;

    //
    // mmActiveFBIOs
    // Contains active FBIO mask for Mixed-Mode memory configuration.
    // This field will be zero for uniform (Non Mixed-Mode) configuration.
    //
    LwU32  mmActiveFBIOs;

    // Cache registers for Powering down/up COMP pads
    LwU32  fbFbioCalmasterCfg;
    LwU32  fbFbioCalgroupVttgelwauxgen;
    LwU32  fbioModeSwitch;

    // cache of REFMPLL DLL CFG
    LwU32  refMpllDllCfg;

#if PMUCFG_FEATURE_ENABLED(PMU_MSCG_16FF_IOBRICK_PWRDOWNS)
    // New registers added for GP10X and later
    LwU32 fbpaFbioMddllCntl;
    LwU32 fbpaFbioVttCntl2;
    LwU32 fbpaFbioCkCk2dqs;
    LwU32 fbpaFbioSubp0WckCk2dqsMsb;
    LwU32 fbpaFbioSubp1WckCk2dqsMsb;
    LwU32 fbpaFbioVttCntl0;
    LwU32 fbpaFbioVttCntl1;
#else
    LwU32 fbpaFbioCfg8;
#endif

    //
    // TODO-pankumar: DMEM optimization scope here
    // since we need these field only on Ampere
    // and later Gpus. If needed, we can restrict
    // it to Ampere+ gpus.
    //
    LwU32 fbpaFbioCtrlSerPriv;
    LwU32 fbpaFbioVttCtrl2;

    // Cache to store the DPD Serial Register
    LwU32 fbpaSwConfig;

    // Cache to store the RefmPll Config
    LwU32 fbioRefmpllConfig;

    // Register to cache the FB Temperature tracking control
    LwU32 fbpaTempCtrl;

    LwU32 vrefPad8;
    LwU32 vrefPad9;
    LwU32 vrefMidCode;
    LwU32 vrefUpperCode;
    LwU32 cfgDqTermMode;
} SWASRREGS;

/*!
 * @brief SW-ASR Object definition
 */
typedef struct OBJSWASR
{
    // Set of registers saved/restored across SW-ASR
    SWASRREGS   regs;

    // Variable to Store DRAM type
    LwU32       dramType;

    // Mask of half-FBPA enabled FBPAs
    LwU32       halfFbpaMask;

    //
    // True when command mapping is either _GDDR3_GT215_COMP_MIRR or
    // _GDDR3_GT215_COMP_MIRR1
    //
    LwBool      bIsGDDR3MirrorCmdMapping;

    //
    // True if AutoCalibration has been asserted
    // during FBIO pad power up.
    //
    LwBool      bCalibCycleAsserted;

    // True if FB has Mixed-Mode memory configuration.
    LwBool      bMixedMemDensity;

    // GDDR5/5x/6 vs SDDR3
    LwBool      bIsGDDRx;

    // GDDR6/6x support
    LwBool      bIsGDDR6;

} OBJSWASR;

/*!
 * @brief Cache for MSCG clock gating
 */
typedef struct CLKGATINGREGS
{
    LwU32  priGlobalConfig;
    LwU32  privDecodeConfig;
    LwU32  sysClkLdivConfig;
    LwU32  lwdClkLdivConfig;
} CLKGATINGREGS;

/*!
 * @brief Macro tells whether MSCG (i.e. LPWR_ENG(MS)) is decoupled from
 * GR (i.e. LPWR_ENG(GR) and LPWR_ENG(GR_RG))
 */
#define IS_GR_DECOUPLED_MS_SUPPORTED(pMsState)                                           \
(!(PMUCFG_FEATURE_ENABLED(PMU_PG_GR) || PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG)) || \
 LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, ELPG_DECOUPLED))

typedef struct
{
    // L2 Cache Set Mgmt_1 Register
    LwU32 l2CachSetMgmt1;

    // L2 Cache Set Mgmt_3 Register
    LwU32 l2CachSetMgmt3;

    // L2 Cache Set Mgmt_5 Register
    LwU32 l2CachSetMgmt5;
} DIFR_LTC_REG;

/*!
 * @brief MS object Definition
 */
typedef struct
{
    // Structure to cache the L2 Cache Registers
    DIFR_LTC_REG    l2CacheReg;

} MS_DIFR;

/*!
 * @brief MS RPG Structure Definition
 */
typedef struct
{
    // Cache the Set Mgmt_5 Register
    LwU32  l2CachSetMgmt5;

    // Mutex token to establish synchronization with RM
    LwU8   mutexToken;

    // Boolean to track the Pre Entry step Exelwtion
    LwBool bRpgPreEntryStepDone;
} MS_RPG;

/*!
 * @brief MS object Definition
 */
typedef struct
{
    // Set of registers saved/restored across clock gating
    CLKGATINGREGS *pClkGatingRegs;

    OBJSWASR      *pSwAsr;

    // MS_DIFR object to manage DIFR Layer 2/Layer 3
    MS_DIFR       *pMsDifr;

    // MS_RPG Object to manange MS-RPG (L2-RPG)
    MS_RPG        *pMsRpg;

    // Starting time of PSI dis-engage sequence.
    FLCN_TIMESTAMP psiDisengageStartTimeNs;

    // abort timeout
    LwU32          abortTimeoutNs;

    //
    // This is either equal to "regulator settle time" OR
    // "voltage settle time" whichever is greater.
    //
    LwU32          psiSettleTimeNs;

    // Mask of enabled MSCG interrupts
    LwU32          intrRiseEnMask;

    //
    // tstgAutoclean and cbcAutoclean save L2 Cache auto flush/clean
    // registers across MSCG cycles.
    //
    LwU32          tstgAutoclean;
    LwU32          cbcAutoclean;

    //
    // Mask of clock domains @ref LW2080_CTRL_CLK_DOMAIN_<xyz> that are gated during MSCG
    //
    LwU32          cgMask;

    // Variable to cache the SMARB Timestamp register
    LwU32          smcarbTimestamp;

    // Number of LTC present in GPU
    LwU32          numLtc;

    // MSCG dynamic current coeff (scaled to 1V) for LWVDD (Logic/Sram) Rail
    LwU16          dynamicLwrrentCoeff[RM_PMU_PSI_RAIL_ID__COUNT];

    // Set when IDLE_FLIP is asserted in ISR for PG_ON interrupt.
    LwBool         bIdleFlipedIsr;

    // Set when enablement of wake interrupt is pending
    LwBool         bWakeEnablePending;

    // Set when SV1 interrupt has been serviced
    LwBool         bSv1Serviced;

    // Variable do decide if we need to exit PSI and MSCG serially (multirail support)
    LwBool         bPsiSequentialExit;

    // Set the variable when DMA is suspended by MS Features
    LwBool         bDmaSuspended;
} OBJMS;

extern OBJMS Ms;

/* ------------------------ Static variables ------------------------------- */

/*!
 * @brief DIFR SW_ASR Sequence Steps Ids
 *
 * Note:
 * 1. Please do not change the order of these steps.
 * 2. First Step must always start from value 0x1
 */
enum
{
    MS_DIFR_SW_ASR_SEQ_STEP_ID_DMA       = 0x1,
    MS_DIFR_SW_ASR_SEQ_STEP_ID_CLK            ,
    MS_DIFR_SW_ASR_SEQ_STEP_ID_IDLE_FLIP_RESET,
    MS_DIFR_SW_ASR_SEQ_STEP_ID_HOST_SEQ       ,
    MS_DIFR_SW_ASR_SEQ_STEP_ID_PRIV_BLOCKER   ,
    MS_DIFR_SW_ASR_SEQ_STEP_ID_HOLDOFF        ,
    MS_DIFR_SW_ASR_SEQ_STEP_ID_NISO_BLOCKER   ,
    MS_DIFR_SW_ASR_SEQ_STEP_ID_L2_CACHE_OPS   ,
    MS_DIFR_SW_ASR_SEQ_STEP_ID_SW_ASR         ,
    MS_DIFR_SW_ASR_SEQ_STEP_ID__COUNT         ,
};

#define MS_DIFR_SW_ASR_SEQ_STEP_ID__START  MS_DIFR_SW_ASR_SEQ_STEP_ID_DMA
#define MS_DIFR_SW_ASR_SEQ_STEP_ID__END   (MS_DIFR_SW_ASR_SEQ_STEP_ID__COUNT - 1)

/*!
 * @brief DIFR CG Sequence Steps Ids
 *
 * Note:
 * 1. Please do not change the order of these steps.
 * 2. First Step must always start from value 0x1
 */
enum
{
    MS_DIFR_CG_SEQ_STEP_ID_CLK       = 0x1,
    MS_DIFR_CG_SEQ_STEP_ID_SMCARB         ,
    MS_DIFR_CG_SEQ_STEP_ID_ISO_BLOCKER    ,
    MS_DIFR_CG_SEQ_STEP_ID_L2_CACHE_OPS   ,
    MS_DIFR_CG_SEQ_STEP_ID_SUB_UNIT       ,
    MS_DIFR_CG_SEQ_STEP_ID_CG             ,
    MS_DIFR_CG_SEQ_STEP_ID_SRAM_SEQ       ,
    MS_DIFR_CG_SEQ_STEP_ID_PSI            ,
    MS_DIFR_CG_SEQ_STEP_ID_CLK_SLOWDOWN   ,
    MS_DIFR_CG_SEQ_STEP_ID__COUNT         ,
};

#define MS_DIFR_CG_SEQ_STEP_ID__START  MS_DIFR_CG_SEQ_STEP_ID_CLK
#define MS_DIFR_CG_SEQ_STEP_ID__END   (MS_DIFR_CG_SEQ_STEP_ID__COUNT - 1)


/*
 * @brief MSCG Sequence Steps Ids
 *
 * Note:
 * 1. Please do not change the order of these steps.
 * 2. First Step must always start from value 0x1
 */
enum
{
    MS_MSCG_SEQ_STEP_ID_DMA = 0x1,
    MS_MSCG_SEQ_STEP_ID_CLK,
    MS_MSCG_SEQ_STEP_ID_IDLE_FLIP_RESET,
    MS_MSCG_SEQ_STEP_ID_SMCARB,
    MS_MSCG_SEQ_STEP_ID_HOST_SEQ,
    MS_MSCG_SEQ_STEP_ID_PRIV_BLOCKER,
    MS_MSCG_SEQ_STEP_ID_HOLDOFF,
    MS_MSCG_SEQ_STEP_ID_NISO_BLOCKER,
    MS_MSCG_SEQ_STEP_ID_ISO_BLOCKER,
    MS_MSCG_SEQ_STEP_ID_L2_CACHE_OPS,
    MS_MSCG_SEQ_STEP_ID_SUB_UNIT,
    MS_MSCG_SEQ_STEP_ID_SW_ASR,
    MS_MSCG_SEQ_STEP_ID_CG,
    MS_MSCG_SEQ_STEP_ID_SRAM_SEQ,
    MS_MSCG_SEQ_STEP_ID_PSI,
    MS_MSCG_SEQ_STEP_ID_CLK_SLOWDOWN,
    MS_MSCG_SEQ_STEP_ID__COUNT,
};

#define MS_MSCG_SEQ_STEP_ID__START MS_MSCG_SEQ_STEP_ID_DMA
#define MS_MSCG_SEQ_STEP_ID__END   (MS_MSCG_SEQ_STEP_ID__COUNT - 1)

/*!
 * @brief Abort checkpoints in MSCG
 */
enum
{
    MS_ABORT_CHKPT_AFTER_PGON              = 0x1,
    //
    // AFTER_PRIV_BLOCKERS = after both XVE BAR blocker and SEC2 priv and FB
    // blockers have been engaged. This is valid for GP102 and later.
    // Pre-GP102, we don't have SEC2 RTOS and hence, no SEC2 blockers, so SEC2
    // is a NOP, and the checkpoint only means after XVE blocker engaged. We
    // include the SEC2 FB blocker in the PRIV blocker checkpoint since we can
    // atomically engage both blockers for SEC2.
    //
    MS_ABORT_CHKPT_AFTER_PRIV_BLOCKERS     = 0x2,
    MS_ABORT_CHKPT_AFTER_NISO_BLOCKERS     = 0x3,
    MS_ABORT_CHKPT_AFTER_ALL_BLOCKERS      = 0x4,
    MS_ABORT_CHKPT_AFTER_PGON_AT_SUSPEND   = 0x5,

    // L2 Cache Action = Flush L2 Cache OR disable L2 cache auto flush
    MS_ABORT_CHKPT_AFTER_L2_CACHE_ACTION   = 0x6,

    // Host FLush Ctrl Disbable
    MS_ABORT_CHKPT_HOST_FLUSH_BIND         = 0x7,
};

/*!
 * @brief Abort reasons for MSCG
 */
enum
{
    // Explicit abortion points
    MS_ABORT_REASON_WAKEUP_PENDING           = 0x1,
    MS_ABORT_REASON_SEC2_WAKEUP_PENDING      = 0x2,
    MS_ABORT_REASON_HOST_NOT_IDLE            = 0x3,

    // Abortion caused by timeout
    MS_ABORT_REASON_FB_NOT_IDLE              = 0x10,
    MS_ABORT_REASON_POLLING_MSGBOX_TIMEOUT   = 0x200,
    MS_ABORT_REASON_NPREAD_TIMEOUT           = 0x300,
    MS_ABORT_REASON_FBP_FENCE_TIMEOUT        = 0x400,
    MS_ABORT_REASON_GPC_FENCE_TIMEOUT        = 0x500,
    MS_ABORT_REASON_SYS_FENCE_TIMEOUT        = 0x600,
    MS_ABORT_REASON_P2P_TIMEOUT              = 0x700,
    MS_ABORT_REASON_HOLDOFF_TIMEOUT          = 0x800,
    MS_ABORT_REASON_POLL_ENGSTAT_TIMEOUT     = 0x900,
    MS_ABORT_REASON_NON_STALLING_INTR        = 0xa00,
    MS_ABORT_REASON_FLUSH_TIMEOUT            = 0xb00,
    MS_ABORT_REASON_FB_BLOCKER_TIMEOUT       = 0xc00,
    MS_ABORT_REASON_BLK_EQUATION_TIMEOUT     = 0xd00,
    MS_ABORT_REASON_DRAINING_TIMEOUT         = 0xe00,
    MS_ABORT_REASON_ISO_BLOCKER_TIMEOUT      = 0xf00,
    MS_ABORT_REASON_L2_FLUSH_TIMEOUT         = 0x1000,
    MS_ABORT_REASON_HOST_INTR_PENDING        = 0x2000,
    MS_ABORT_REASON_HOST_FLUSH_ENABLED       = 0x4000,
    MS_ABORT_REASON_HOST_MEMOP_PENDING       = 0x8000,
    MS_ABORT_REASON_UNUSED                   = 0x9000,
    MS_ABORT_REASON_DMA_SUSPENSION_FAILED    = 0xa000,
    MS_ABORT_REASON_BLK_EVERYTHING_TIMEOUT   = 0xb000,
    MS_ABORT_REASON_PRIV_FLUSH_TIMEOUT       = 0xc000,
};

#define MS_DEFAULT_ABORT_TIMEOUT0_NS (100 * 1000)
#define MS_DEFAULT_ABORT_TIMEOUT1_NS (100 * 1000)


/*!
 * @ brief MSCG Events
 */
enum
{
    MS_SEQ_ENTRY = 0x1,
    MS_SEQ_EXIT  = 0x2,
    MS_SEQ_ABORT = 0x3,
};

/*!
 * @ brief L2 Cache operation used by MS Group
 */
enum
{
    MS_L2_CACHE_CBC_CLEAN = 0x1,
    MS_L2_CACHE_DATA_CLEAN,
};

/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS msPostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "msPostInit");

void        msGrpPostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "msGrpPostInit");

void        msMscgInit(RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "msMscgInit")
            GCC_ATTRIB_NOINLINE();

void        msLtcInit(RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "msLtcInit")
            GCC_ATTRIB_NOINLINE();

FLCN_STATUS msProcessInterrupts(DISPATCH_LPWR *, PG_LOGIC_STATE *)
            GCC_ATTRIB_SECTION("imem_lpwr", "msProcessInterrupts")
            GCC_ATTRIB_NOINLINE();

LwBool      msWakeupPending(void)
            GCC_ATTRIB_SECTION("imem_lpwr", "msWakeupPending")
            GCC_ATTRIB_NOINLINE();

FLCN_STATUS msDmaSequenceEntry(LwU16 *pAbortReason);
void        msDmaSequenceExit(void);

FLCN_STATUS msIdleFlipReset(LwU8 ctrlId, LwU16 *pAbortReason);

FLCN_STATUS msPsiSeq(LwU8 ctrlId, LwU32 msPsiStepMask);

void        msClkCntrActionHandler(LwU8 ctrlId, LwU8 msClkGateEventId);

void        msClkGate(LwU8 ctrlId);
void        msClkUngate(LwU8 ctrlId);

void        msSfmHandler(LwU8 ctrlId)
            GCC_ATTRIB_SECTION("imem_lpwr", "msSfmHandler");

void        msMclkAction(LwBool *pBMsEnabled, LwBool *pBWrTrainingReq)
            GCC_ATTRIB_SECTION("imem_lpwr", "msMclkAction");

LwBool      msCheckTimeout(LwU32, PFLCN_TIMESTAMP, LwS32 *);

void        msPassiveInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "msPassiveInit")
            GCC_ATTRIB_NOINLINE();

// DIFR related functions
void        msDifrSwAsrInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "msdifrSwAsrInit");

void        msDifrCgInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "msdifrCgInit");

void        msDifrSwAsrHoldoffMaskInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "msdifrSwAsrHoldoffMaskInit");

#endif // PMU_OBJMS_H
