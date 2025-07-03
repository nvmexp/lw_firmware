/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJPMU_H
#define PMU_OBJPMU_H

#include "g_pmu_objpmu.h"

#ifndef G_PMU_OBJPMU_H
#define G_PMU_OBJPMU_H

/*!
 * @file pmu_objpmu.h
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "pmu_objtimer.h"
#include "config/g_pmu_hal.h"
/*
 * Note: include pmu_bar0.h with <angle brackets> so that when we do a unit test
 * build, it includes the unit test version of pmu_bar0.h. Using "" would cause
 * The pmu_bar0.h in the same directory as this header to take precedence.
 */
#include <pmu_bar0.h>
#include "pmu_gpuarch.h"

/* ------------------------ Defines ---------------------------------------- */

#define PMU_IMEM_BLOCK_SIZE               (256U)

/*!
 * Wrapper macro for pmuPollReg32Ns
 */
#define PMU_REG32_POLL_NS(a,k,v,t,m)    pmuPollReg32Ns(a,k,v,t,m)
#define PMU_REG32_POLL_US(a,k,v,t,m)    pmuPollReg32Ns(a,k,v,t * 1000U,m)

/*!
 * Defines polling modes for pmuPollReg32Ns.
 */
#define LW_PMU_REG_POLL_REG_TYPE                                          0:0
#define LW_PMU_REG_POLL_REG_TYPE_CSB                                       (0)
#define LW_PMU_REG_POLL_REG_TYPE_BAR0                                      (1)
#define LW_PMU_REG_POLL_OP                                                1:1
#define LW_PMU_REG_POLL_OP_EQ                                              (0)
#define LW_PMU_REG_POLL_OP_NEQ                                             (1)

#define PMU_REG_POLL_MODE_CSB_EQ   (DRF_DEF(_PMU, _REG_POLL, _REG_TYPE, _CSB)|\
                                    DRF_DEF(_PMU, _REG_POLL, _OP, _EQ))
#define PMU_REG_POLL_MODE_CSB_NEQ  (DRF_DEF(_PMU, _REG_POLL, _REG_TYPE, _CSB)|\
                                    DRF_DEF(_PMU, _REG_POLL, _OP, _NEQ))
#define PMU_REG_POLL_MODE_BAR0_EQ  (DRF_DEF(_PMU, _REG_POLL, _REG_TYPE, _BAR0)|\
                                    DRF_DEF(_PMU, _REG_POLL, _OP, _EQ))
#define PMU_REG_POLL_MODE_BAR0_NEQ (DRF_DEF(_PMU, _REG_POLL, _REG_TYPE, _BAR0)|\
                                    DRF_DEF(_PMU, _REG_POLL, _OP, _NEQ))

// Helper macro to see if we are running on a 0FB config
#define PMU_IS_ZERO_FB_CONFIG()                          \
    (PMUCFG_FEATURE_ENABLED(PMU_IS_ZERO_FB) &&           \
     (FLD_TEST_DRF(_RM_PMU, _CMD_LINE_ARGS_FLAGS,        \
                   _ZERO_FB, _TRUE, PmuInitArgs.flags)))

// Helper macro to get pwr clock value in Hz
#define PMU_GET_PWRCLK_HZ()    (PmuInitArgs.cpuFreqHz)

/*!
 * @brief   Macro to add filter to prevent elevation of falcon privilege level.
 *
 * @note    Not required on RISCV since priv level is set by a CSR.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_DYNAMIC_FLCN_PRIV_LEVEL) && !defined(UPROC_RISCV))
#define PMU_BAR0_WR32_SAFE(addr, data)  pmuBar0Wr32Safe((addr), (data))
#else
#define PMU_BAR0_WR32_SAFE(addr, data)  REG_WR32(BAR0, (addr), (data))
#endif

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/*!
 * PMU object Definition
 */
typedef struct
{
#if PMUCFG_FEATURE_ENABLED(PMU_CHIP_INFO)
    PMU_CHIP_INFO   chipInfo;
    LwU16           gpuDevId;
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_PRIV_SEC)
/*!
 * This variable on RISCV caches priv level of supervisor mode.
 * It is possible for tasks to have priv level 0 so use this boolean with caution
 * when using within tasks.
 * MMINTS-TODO: split this out into a separate variable and link it into kernel data
 * on RISCV.
 */
    LwBool          bLSMode;
#endif
} OBJPMU;

extern OBJPMU Pmu;

// PmuVSBCache caches the scratch register used for API-level protection
extern LwU32  PmuVSBCache;

//
// These variables are meaningless on RISCV, since SCTL is not used,
// and priv level is different for U mode and S mode so there's no
// need to reset to a "default".
//
#ifndef UPROC_RISCV
// Define variables to cache PMU default privilege level state
extern LwU8      PmuPrivLevelExtDefault;
extern LwU8      PmuPrivLevelCsbDefault;
extern LwU32     PmuPrivLevelCtrlRegAddr;
#endif // UPROC_RISCV

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_WAIT_UNTIL_DONE_OR_TIMEOUT)
LwBool pmuWaitUntilDoneOrTimeout(LwBool (*pFunc)(LwS32 *pArgs), LwS32 *pArgs, LwU32 maxTicks)
    GCC_ATTRIB_SECTION("imem_resident", "pmuWaitUntilDoneOrTimeout");
#endif // PMUCFG_FEATURE_ENABLED(PMU_WAIT_UNTIL_DONE_OR_TIMEOUT)

/*!
 * Poll on a register until the specified condition is hit.
 *
 * @param[in]  addr      The BAR0/CSB address to read
 * @param[in]  mask      Masks the value read from CSB addr before compare
 * @param[in]  val       Value used to compare against the masked csb register
 *                       value
 * @param[in]  timeoutNs Timeout value in ns. (granularity is 32ns, as
 *                       PTIMER_TIME_0 ignores the 5 LSBs)
 * @param[in]  mode
 *
 *   PMU_REG_POLL_REG_TYPE_CSB  - poll from a CSB register
 *   PMU_REG_POLL_REG_TYPE_BAR0 - poll from a BAR0 register
 *   PMU_REG_POLL_OP_EQ         - returns LW_TRUE if the masked register value
 *                                is equal to the passed value,
 *                                LW_FALSE otherwise
 *   PMU_REG_POLL_OP_NEQ        - returns LW_TRUE if the masked register value
 *                                is not equal to the passed value,
 *                                LW_FALSE otherwise
 *
 * @return LW_TRUE  - if the specificied condition is met
 * @return LW_FALSE - if timed out.
 */
mockable LwBool pmuPollReg32Ns(LwU32 addr, LwU32 mask, LwU32 val, LwU32 timeoutNs, LwU32 mode)
    GCC_ATTRIB_SECTION("imem_resident", "pmuPollReg32Ns");

void pmuBar0Wr32Safe(LwU32 addr, LwU32 data)
    GCC_ATTRIB_SECTION("imem_resident", "pmuBar0Wr32Safe");

/*!
 * @brief   Verifies that version of the running ucode is supported.
 *
 * @details This is required for security purposes. If the check fails, this
 *          function both HALTs the PMU and, in case the core gets restarted,
 *          returns an error.
 *
 * @return  FLCN_OK                 Ucode version is supported.
 * @return  FLCN_ERR_ILWALID_STATE  Ucode version is not supported.
 */
FLCN_STATUS pmuVerifyVersion(void)
    GCC_ATTRIB_SECTION("imem_init", "pmuVerifyVersion");

/* ------------------------ Misc macro definitions ------------------------- */
#endif // G_PMU_OBJPMU_H
#endif // PMU_OBJPMU_H
