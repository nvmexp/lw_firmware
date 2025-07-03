/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJDFPR_H
#define PMU_OBJDFPR_H

/*!
 * @file pmu_objdfpr.h
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_dfpr_hal.h"
/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief DIFR Prefetch Sequence Steps Ids
 *
 * Note:
 * 1. Please do not change the order of these steps.
 * 2. First Step must always start from value 0x1
 */
enum
{
    DFPR_SEQ_STEP_ID_HOLDOFF             = 0x1,
    DFPR_SEQ_STEP_ID_L2_CACHE_DEMOTE          ,
    DFPR_SEQ_STEP_ID_L2_CACHE_SETTING         ,
    DFPR_SEQ_STEP_ID_PREFETCH                 ,
    DFPF_SEQ_STEP_ID_MS_GROUP_MUTUAL_EXCLUSION,
    DFPR_SEQ_STEP_ID__COUNT                   ,
};

#define DFPR_SEQ_STEP_ID__START  DFPR_SEQ_STEP_ID_HOLDOFF
#define DFPR_SEQ_STEP_ID__END   (DFPR_SEQ_STEP_ID__COUNT - 1)

/*!
 * @brief Abort reasons for DFPR FSM
 *
 * Max Mask for abort reason can go upto BIT(15) as we are sharing it with Step
 * id during abort scenario.
 */
#define  DFPR_ABORT_REASON_WAKEUP_PENDING         LWBIT(0)
#define  DFPR_ABORT_REASON_HOLDOFF_TIMEOUT        LWBIT(1)
#define  DFPR_ABORT_REASON_L2_DEMOTE_TIMEOUT      LWBIT(2)
#define  DFPR_ABORT_REASON_L2_OPS_PENDING         LWBIT(3)
#define  DFPR_ABORT_REASON_IDLE_FLIPPED           LWBIT(4)

/*!
 * Maximum timeout value in nS for Demote operation to finish
 * As per HW folks, it should get finished within 10uS, however,
 * we are adding some marging and making it 100uS.
 */
#define DFPR_L2_DEMOTE_TIMEOUT_NS   (100 * 1000)

/* ------------------------ External definitions --------------------------- */

typedef struct
{
    // L2 Cache Set Mgmt_0 Register
    LwU32 l2CacheSetMgmt0;

    // L2 Cache Set Mgmt_4 Register
    LwU32 l2CacheSetMgmt4;

    // L2 Cache CFG 0 Register
    LwU32 l2CacheCfg0;
} DFPR_LTC_REG;

/*!
 * DFPR object Definition
 */
typedef struct
{
     // Structure to cache the L2 Cache Registers
    DFPR_LTC_REG    l2CacheReg;

    // Number of LTC present
    LwU32           numLtc;

    // Boolean to decide if we need to issue L2 Cache Demote on Exit
    LwBool          bDemoteOnExit;

    // Boolean to check if Prefetch Request Response is pending or not
    LwBool          bIsPrefetchResponsePending;

    // Boolean to check if DFPR FSM was self disallowed using SW_CLINT
    LwBool          bSelfDisallow;
} OBJDFPR;

extern OBJDFPR Dfpr;

/* ------------------------ Private Variables ------------------------------ */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS dfprPostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "dfprPostInit");

void dfprInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "dfprInit");

LwBool dfprIsIdle(LwU8 ctrlId, LwU16 *pAbortReason);
FLCN_STATUS dfprPrefetchResponseHandler(LwBool bIsPrefetchDone)
            GCC_ATTRIB_SECTION("imem_lpwr", "dfprPrefetchResponseHandler");
#endif // PMU_OBJDFPR_H
