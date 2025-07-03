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


#ifndef CLK3_FS_ROSDM_H
#define CLK3_FS_ROSDM_H

/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkrovco.h"
#include "pmu/pmuifclk.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef ClkWire_Virtual ClkRoSdm_Virtual;


/*!
 * @extends     ClkRoVco
 * @brief       Sigma-Delta Modulation for Read-Only VCOs
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     Objects of this class represent a VCO (voltage controlled
 *              oscillator) that uses the SDM (sigma-delta modulation)
 *              feature and is not programmed.  A PLL (phase-locked loop) is
 *              typically a VCO and zero or more PLDIVs (aka PDIVs).  Use
 *              'ClkPDiv' (in addition to this class) for PLLs with PLDIV.
 *
 *              This class inherits the data structure, but no logic, from
 *              'ClkRoVco'.  'bDiv2Enable' is not supported.
 *
 *              clkConfig/clkProgram/clkCleanup are not supported.  Calls to
 *              any of these functions results in a NULL pointer exception.
 *              This is okay for 'ClkMclkFreqDomain' (and 'ClkReadOnly') since
 *              they never call these functions.
 *
 *              As with other 'ClkFreqSrc' subclasses, clkRead supports
 *              volatility in spite of the fact that it is read-only.  In other
 *              words, it reads the registers rather than depend on cached
 *              values, which is appropriate for MCLK.  Use 'ClkReadOnly'
 *              to cache and suppress volatility.
 *
 *              Summary of logic assumptions:
 *              - The VCO will not be programmed after devinit;
 *              - The VCO is enabled after devinit;
 *              - Fractional NDIV is not being used;
 *              - DIVBY2 is not being used; and
 *              - PLDIV is not being used or is being managed by other logic.
 *
 *              If any of these assumptions do not apply, use ClkPll instead.
 */
typedef struct
{
/*!
 * @brief       Inherited state
 */
    ClkRoVco    super;

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
} ClkRoSdm;


/* ------------------------ External Definitions --------------------------- */
/*!
 * @brief       Virtual table
 * @memberof    ClkRoSdm
 * @protected
 */
extern const ClkRoSdm_Virtual clkVirtual_RoSdm;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkRoSdm
 * @protected
 * @details     RoSdm class is lwrrently used only for REFMPLL for reading hardware
 *              coefficients. Since, REFMPLL is programmed in fbfalcon the config,
 *              program and cleanup are never called here. As such assigned those
 *              functions to NULL.
 */
/**@{*/
FLCN_STATUS clkRead_RoSdm(ClkRoSdm *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_RoSdm");
#define     clkConfig_RoSdm       NULL
#define     clkProgram_RoSdm      NULL
#define     clkCleanup_RoSdm      NULL
#if CLK_PRINT_ENABLED
void clkPrint_RoSdm(ClkRoSdm *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_RoSdm");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_ROSDM_H

