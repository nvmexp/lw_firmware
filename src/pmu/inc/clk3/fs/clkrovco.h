/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
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
 * @author  Antone Vogt-Varvak
 */


#ifndef CLK3_FS_ROVCO_H
#define CLK3_FS_ROVCO_H

/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkwire.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef ClkWire_Virtual ClkRoVco_Virtual;


/*!
 * @extends     ClkWire
 * @brief       Read-Only VCO (Voltage Controlled Oscillator)
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     Each object of this class represents a voltage-controlled
 *              oscillator (VCO), which complies with the following assumptions:
 *              - The VCO will not be programmed after devinit;
 *              - The VCO is enabled after devinit;
 *              - Fractional NDIV and SDM are not being used;
 *              - DIVBY2 is not being used; and
 *              - PLDIV is not being used or is being managed by other logic.
 *
 *              Examples of VCos which meet these criteria in the context of
 *              specific chips:
 *              - TU10x:  syspll, sppll0, sppll1, drampll, (refmpll)
 *              - GA100:  sppll0, hbmpll
 *              - GA10x:  defrost, sppll0, sppll1, drampl, (refmpll)
 *              - GH100:  defrost, drampll
 *
 *              Use subclass 'ClkSdm' for sigma-delta modulation (SDM) on refmpll.
 *
 *              These fields are the only ones requisite or used in the coefficient
 *              register:
 *              - LW_PTRIM_SYS_PLL_COEFF_MDIV
 *              - LW_PTRIM_SYS_PLL_COEFF_NDIV
 *
 *              Examples of hardware PLLs types which use this class:
 *              - PLL27G_SSD_DYN_APESD_A1
 *              = HPLL16G_SSD_DYN_A1
 */
typedef struct
{
/*!
 * @brief       Inherited state
 *
 * @details     ClkRoVco inherits from ClkWire since it is the class that handles
 *              objects with exactly one potential input, which is useful to
 *              separate out the lone-potential-input logic (e.g. daisy-chaining
 *              along the schematic dag) from the ROVCO-specific logic.
 */
    ClkWire     super;

/*!
 * @brief       Coefficient register
 *
 * @details     The name of this register in dev_trim is LW_PVTRIM_SYS_xxx_COEFF,
 *              where xxx is the name of the ROVCO (with some exceptions).
 *
 *              For REFPLL, the name in dev_fbpa is LW_PFB_FBPA_REFMPLL_COEFF.
 *
 *              For HBMPLL, the name in dev_fbpa is LW_PFB_FBPA_FBIO_COEFF_CFG(i)
 *              for some 'i' basec on floorsweeping.
 *
 *              The generic name for the register is LW_PTRIM_SYS_PLL_COEFF.
 *
 *              This member can not be marked 'const' because the GH100 MCLK
 *              constructor (for example) assigns this value at runtime based
 *              on floorsweeping.  Nonetheless, it is effectively 'const'
 *              after construction.
 */
    LwU32       coeffRegAddr;

/*!
 * @brief       Signal source per @ref LW2080_CTRL_CLK_PROG_1X_SOURCE_<XYZ>
 *
 * @details     ClkSignal::source is set to this value to indicate which PLL
 *              this is.  Typical values for this class are:
 *              LW2080_CTRL_CLK_PROG_1X_SOURCE_SPPLL0 (Ampere and later)
 *              LW2080_CTRL_CLK_PROG_1X_SOURCE_SPPLL1 (Ampere and later)
 *              LW2080_CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE (Turing only)
 *
 *              Both 'clkRead' and 'clkConfig' set 'pOutout->source' to this
 *              value on return, since it indicates which sort of object is
 *              closest to the root in the active path.  Other classes may
 *              override this value after calling into this class.
 */
    const LwU8  source;

/*!
 * @brief       PLL contains a DIV2
 *
 * @details     When true, the PLL contains a DIV2 divider, which generally
 *              applies only to DRAMPLL on GDDR memory.
 */
    const LwBool bDiv2Enable : 1;

/*!
 * @brief       PLL output frequency is double of what is callwlated
 *
 * @details     When true, the PLL output frequency is considered as double of
 *              what is callwlated using its coefficients, which generally
 *              applies only to DRAMPLL on GDDR6x memory. Frequencies callwlated
 *              on G6x are different for the same data rate because of PAM4.
 *
 * @note        This field does not change after initialization.
 */
    LwBool       bDoubleOutputFreq : 1;
} ClkRoVco;


/* ------------------------ External Definitions --------------------------- */
/*!
 * @brief       Virtual table
 * @memberof    ClkRoVco
 * @protected
 */
extern const ClkRoVco_Virtual clkVirtual_RoVco;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkRoVco
 * @protected
 *
 * @details     Since this class is used only for reading hardware coefficients,
 *              clkConffig/clkProgram/clkCleanup are never called here. As such
 *              those functions are aliased to NULL.
 *
 *              For PLLs shared among clock domains (such as SPPLLs), ClkReadOnly
 *              objects should be inserted on the output side of ClkRoVco objects
 *              to ensure that these function are not called.
 *
 *              For MCLK, which is programmed on the FB Falcon, ClkMclkFreqDomain
 *              ensures that these functions are not called.
 */
/**@{*/
FLCN_STATUS clkRead_RoVco(ClkRoVco *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_RoVco");
#define     clkConfig_RoVco       NULL
#define     clkProgram_RoVco      NULL
#define     clkCleanup_RoVco      NULL
#if CLK_PRINT_ENABLED
void clkPrint_RoVco(ClkRoVco *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_RoVco");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_ROVCO_H

