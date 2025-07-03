/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CG_H
#define PMU_CG_H

/*!
 * @file pmu_cg.h manages Clock Gating Features.
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "pmu_objpmgr.h"

/* ------------------------ Macros & Defines ------------------------------- */

/*!
 * @brief Macros to read and write THERM_GATE_CTRL register
 *
 * To avoid race explained in WAR1603090 (Comment http://lwbugs/1603090/22)
 * SW should always use priv access to enable/disable ELCG clock gating and
 * ELCG SM on GP100. This issue has been fixed on GP10X by bug 1598901.
 */
#define THERM_GATE_CTRL_REG_RD32(idx)                                       \
        (PMUCFG_FEATURE_ENABLED(PMU_CG_ELCG_WAR1603090_PRIV_ACCESS) ?       \
         REG_RD32(FECS, LW_THERM_GATE_CTRL(idx)) :                          \
         REG_RD32(CSB, LW_CPWR_THERM_GATE_CTRL(idx)))

#define THERM_GATE_CTRL_REG_WR32(idx, data)                                 \
        do                                                                  \
        {                                                                   \
            if (PMUCFG_FEATURE_ENABLED(PMU_CG_ELCG_WAR1603090_PRIV_ACCESS)) \
            {                                                               \
                REG_WR32(FECS, LW_THERM_GATE_CTRL(idx), data);              \
            }                                                               \
            else                                                            \
            {                                                               \
                REG_WR32(CSB, LW_CPWR_THERM_GATE_CTRL(idx), data);          \
            }                                                               \
        } while (LW_FALSE)

/*!
 * @brief Disable reason for ELCG Ctrls
 *
 * RM : ELCG is disabled by RM
 */
enum
{
    LPWR_CG_ELCG_CTRL_REASON_RM = 0x0,
    LPWR_CG_ELCG_CTRL_REASON_GR_PG   ,
    LPWR_CG_ELCG_CTRL_REASON_GR_RG   ,
    LPWR_CG_ELCG_CTRL_REASON_MS      ,
    LPWR_CG_ELCG_CTRL_REASON_DI      ,
};

/*!
 * @brief LPWR Clock Gating - Infrastructure to handle Clock related
 *                          operations for LPWR
 *
 * @Description :
 *      Different LPWR features such as MSCG / DI / GFX-RPPG require clock gating
 *      operations.
 *      These features all have different requirements such as :
 *          1. Ability to choose which clocks are to be changed.
 *          2. Ability to specify individual target modes for each clock.
 *          3. HW Requirements such as :
 *           3.1 Alternate clocks are needed to powerdown/powerup certain PLLs.
 *           3.2 Clocks should be restored to pre-LPWR state once all operations
 *               are done.
 *           3.3 Limitations on number of paths available for each clock.
 *               (Path of clock implies the path
 *                from which clock is lwrrently being sourced. In the LPWR CG Infra,
 *                clock path is specified by clock's Mode)
 *
 *      To address these requirements, LPWR Clock Gating has been introduced.
 *      LPWR-CG has a set of structures and APIs. For any operation to be done,
 *      client only needs to feed a Mask of clocks and an array of target Modes.
 *      LPWR-CG APIs are strong enough to handle other HW/SW restrictions internally.
 *
 * @APIDescription :
 *      For detailed description of APIs, refer to comments in individual functions.
 *      Here is a list of APIs :
 *       -> lpwrCgInit_HAL ()      - Accepts pointers to various Lpwr.pLpwrCg members and initializes.
 *                               - Manipulates clkMaskAll based on Clocks Capability Bit.
 *       -> lpwrCgPowerup_HAL ()   - Accepts a clkMask and list of target modes for all clocks and
 *                                 performs Powerup Operation.
 *       -> lpwrCgPowerdown_HAL () - Accepts a clkMask and list of target modes for all clocks and
 *                                     performs Powerdown Operation.
 *
 * @LwrrentUsage :
 *      Lwrrently LPWR-CG is being used in following places :
 *      --> DI   - Entry : Restore clocks to Pre MSCG state before powering down MemPlls.
 *               - Entry : Put all clocks to ALT Path before powering down Core PLLs.
 *               - Entry : Gate all clocks after PLL Powerdown is complete.
 *               - Exit  : Ungate all clocks to ALT path before powering up core plls.
 *               - Exit  : Put all clocks to pre-PG_ENG state before powering up mem PLLs.
 *               - Exit  : Restore all clocks to pre-DI path after Mem PLLs have been powered up.
 *      --> MSCG - Entry : Gate all clocks.
 *               - Exit  : Restore Clocks to Pre-MSCG state.
 *      --> GFX-RPPG : To be added by Deep.
 */

// Define to check if given clock is present in mask or not
#define IS_CLK_SUPPORTED(clkName, clkMask)                                 \
        ((clkMask) & CLK_DOMAIN_MASK_##clkName)

/*!
 * @brief : LPWR_CG_MODE : Enumerates all possible modes that the clocks can have.
 * 1. _GATED     : Clock needs to be gated.
 * 2. _ALT       : Move the clock to ALT path and ungate them.
 * 3. _ORG       : Move the clocks to their original source path. This can be
 *                 VCO or ALT. Clocks will be ungated in this mode.
 * 4. _VCO       : Move the clocks to VCO path. (Clock will be ungated).
 *                 Note: This mode is used internally by clock gating code.
 *                 Clients of clock gating code will never use it.
 */
enum
{
    LPWR_CG_TARGET_MODE_GATED = 0x0,
    LPWR_CG_TARGET_MODE_ALT        ,
    LPWR_CG_TARGET_MODE_ORG        ,
    LPWR_CG_TARGET_MODE_VCO        ,
} LPWR_CG_MODE;

/*!
 * @brief : LPWR_CG : Structure to maintain cache of clocks.
 */
typedef struct
{

    // variable to cache the original value of LW_PTRIM_SYS_STATUS_SEL_VCO register
    LwU32  selVcoCache;

    //
    // variable to cache the original value of LW_PTRIM_GPC_BCAST_STATUS_SEL_VCO register
    // This is needed for Pascal and later GPUs
    //
    LwU32  gpcSelVcoCache;

    // variable to indicate if the vco value saved is cached or not
    LwBool bCacheValid;

    //
    // This mask is chip specific and specifies the clocks that are present on the chip.
    // This mask should only be read and never modified. It is initialized based on capability
    // bits of Physical Clocks.
    //
    // This mask is based on the LW_2080_CLK_DOMAIN_XYZ format.
    //
    LwU32  clkDomainSupportMask;
} LPWR_CG;

//
// Linear divider takes ~70 PLL clock cycles to switch clock source.
// Bug 532489 Comment #204 OR CL4455814 for the details.
//
#define LPWR_CG_CLK_SWITCH_TIMEOUT_NS               (10000)

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ Function Prototypes ---------------------------- */
void        lpwrCgPreInit(void)
            GCC_ATTRIB_SECTION("imem_init", "lpwrCgPreInit");
FLCN_STATUS lpwrCgPostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "lpwrCgPostInit");
FLCN_STATUS lpwrCgPostInitRpcHandler(LwBool bIstCgSupport)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "lpwrCgPostInitRpcHandler");

FLCN_STATUS lpwrCgElcgEnable(LwU32 pmuEngId, LwU32 reasonId);
FLCN_STATUS lpwrCgElcgDisable(LwU32 pmuEngId, LwU32 reasonId);

#endif // PMU_CG_H
