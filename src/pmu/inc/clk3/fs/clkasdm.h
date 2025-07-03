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


#ifndef CLK3_FS_ASDM_H
#define CLK3_FS_ASDM_H

/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkapll.h"
#include "pmu/pmuifclk.h"


/* ------------------------ Macros ----------------------------------------- */
#define LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN_MAX  4096
#define LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN_MIN -4096


/* ------------------------ Datatypes -------------------------------------- */

typedef ClkWire_Virtual ClkASdm_Virtual;


/*!
 * @brief       SDM PLL Per-Phase Status
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
} ClkASdmPhase;


/*!
 * @extends     ClkAPll
 * @brief       Sigma-Delta Modulation for Analog Phase Locked Loop
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
 * @details     ClkASdm inherits from ClkAPll.
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
 * @brief       Per-phase Dynamic State
 */
    ClkASdmPhase
                phase[CLK_MAX_PHASE_COUNT];
} ClkASdm;


/* ------------------------ External Definitions --------------------------- */
/*!
 * @brief       Virtual table
 * @memberof    ClkASdm
 * @protected
 */
extern const ClkASdm_Virtual clkVirtual_ASdm;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkASdm
 * @protected
 * @details     Sdm class is lwrrently used only for REFMPLL for reading hardware
 *              coefficients. Since, REFMPLL is programmed in fbfalcon the config,
 *              program and cleanup are never called here. As such assigned those
 *              functions to NULL.
 */
/**@{*/
FLCN_STATUS clkRead_ASdm(ClkASdm *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_ASdm");
#define     clkConfig_ASdm       NULL
#define     clkProgram_ASdm      NULL
#define     clkCleanup_ASdm      NULL
#if CLK_PRINT_ENABLED
void clkPrint_ASdm(ClkASdm *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_ASdm");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_ASDM_H

