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
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @see     //hw/libs/common/analog/lwpu/doc/
 * @see     //hw/doc/gpu/volta/volta/physical/clocks/Volta_clocks_block_diagram.vsd
 * @author  Chandrabhanu Mahapatra
 */


#ifndef CLK3_FS_ADYN_RAMP_H
#define CLK3_FS_ADYN_RAMP_H

/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkapll.h"
#include "pmu/pmuifclk.h"


/* ------------------------ Macros ----------------------------------------- */

#define LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN_MAX  4096
#define LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN_MIN -4096


/* ------------------------ Datatypes -------------------------------------- */

typedef ClkWire_Virtual ClkADynRamp_Virtual;


/*!
 * @brief       Dynamic Ramp PLL Per-Phase Status
 * @version     Clocks 3.1 and after
 *
 * @details     Dynamic state for dynamic ramping that differs for each phase or
 *              SDM state that differs for each phase.
 *
 *              Only functions belonging to the class and subclasses may access
 *              or modify anything in this struct.
 */
typedef struct
{
/*!
 * @brief       Value to program the SDM.
 *
 * @details     The value of the SDM is computed by clkConfig.
 *              LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN is the generic name for this field.
 *
 *              This member when enabled takes values between 0xF000 to 0x1000
 */
    LwS16       sdm;
} ClkADynRampPhase;


/*!
 * @extends     ClkAPll
 * @brief       Dynamic Ramp for Phase Locked Loop
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     Objects of this class represtent a phase-locked loop in the hardware.
 */
typedef struct
{
/*!
 * @brief       Inherited state
 *
 * @details     ClkADynRamp inherits from ClkAPll.
 *              Configuration register used to set _EN_PLL_DYNRAMP and read
 *              back _DYNRAMP_DONE.
 *              Coefficient register used to set _NDIV_NEW.
 */
    ClkAPll     super;

/*!
 * @brief       Configuration register
 *
 * @details     The name of this register in the manuals is LW_PVTRIM_SYS_xxx_CFG2.
 *              where xxx is the name of the PLL (with some exceptions).
 *              The generic name for the register is LW_PVTRIM_SYS_PLL_CFG2.
 *              Used to set _SSD_EN_SDM.
 */
    const LwU32 cfg2RegAddr;

/*!
 * @brief       SSD0 register
 *
 * @details     The name of this register in the manuals is LW_PVTRIM_SYS_xxx_SSD0
 *              where xxx is the name of the PLL i.e. SPPLLs, DISPPLL or VPLL. Its
 *              only used here to program the _SDM_DIN. The generic name for the
 *              register is LW_PVTRIM_SYS_PLL_SSD0.
 */
    const LwU32 ssd0RegAddr;

/*!
 * @brief       Dynamic Ramp register
 *
 * @details     The name of this register in the manuals is LW_PVTRIM_SYS_xxx_DYN.
 *              where xxx is the name of the PLL (with some exceptions).
 *              The generic name for the register is LW_PVTRIM_SYS_PLL_DYN.
 *              Used to program _SDM_DIN_NEW.
 */
    const LwU32 dynRegAddr;

/*!
 * @brief       Dynamic ramp completion time in nanoseconds
 *
 * @details     This value is typically 500000 ns and is used for the time
 *              for the dynamic ramp to complete.
 */
    LwU32 dynRampNs;

/*!
 * @brief       Per-phase Dynamic State
 */
    ClkADynRampPhase
                phase[CLK_MAX_PHASE_COUNT];
} ClkADynRamp;


/* ------------------------ External Definitions --------------------------- */
/*!
 * @brief       Virtual table
 * @memberof    ClkADynRamp
 * @protected
 */
extern const ClkADynRamp_Virtual clkVirtual_ADynRamp;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkADynRamp
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_ADynRamp(ClkADynRamp *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_ADynRamp");
FLCN_STATUS clkConfig_ADynRamp(ClkADynRamp *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_ADynRamp");
FLCN_STATUS clkProgram_ADynRamp(ClkADynRamp *this, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_ADynRamp");
FLCN_STATUS clkCleanup_ADynRamp(ClkADynRamp *this, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkCleanup_ADynRamp");
#if CLK_PRINT_ENABLED
void clkPrint_ADynRamp(ClkADynRamp *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_ADynRamp");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_ADYN_RAMP_H

