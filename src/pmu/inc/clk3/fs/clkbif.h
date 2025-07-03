/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Chandrabhanu Mahapatra
 */


#ifndef CLK3_FS_BIF_H
#define CLK3_FS_BIF_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkfreqsrc.h"
#include "pmu/pmuifbif.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkBif               ClkBif;
typedef        ClkFreqSrc_Virtual   ClkBif_Virtual;


/*!
 * @class       ClkBif
 * @extends     ClkFreqSrc
 * @brief       Bus Inferface Gen Speed (PCIE)
 * @protected
 *
 * @details     This class calls into OBJBIF to get/set the PCIE link speed
 *              (genspeed).
 *
 *              Unlink other ClkFreqSrc classes, ClkSignal::freqSrc contains
 *              the genspeed rather than a frequency.  As of July 2015, this
 *              class supports Gen1 through Gen4, a superset of OBJBIF.
 *
 *              Typically, an objects of this class is the root for the
 *              PCIEGenClk domain, symbolized by clkWhich_PCIEGenClk and
 *              LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK
 */
struct ClkBif
{
    /*!
     * @brief       Inherited state
     *
     * @ilwariant   Inherited state must always be first.
     */
    ClkFreqSrc super;

    /*!
    * @brief        Max Gen speed that can be programmed
    */
    const RM_PMU_BIF_LINK_SPEED maxGenSpeed;

    /*!
     * @brief       Gen Speed lwrrently programmed
     */
    RM_PMU_BIF_LINK_SPEED lwrrentGenSpeed;

    /*!
     * @brief       Gen Speed to be programmed
     */
    RM_PMU_BIF_LINK_SPEED targetGenSpeed;

    struct
    {
        /*!
         * @brief   Gen Speed from last call to bifGetBusSpeed or bifChangeBusSpeed
         * @note    Debugging only
         *
         * @details Zero usually means error.
         *          255 means there was nothing to do in clkProgram_Bif.
         *          Other values are defined in bifChangeBusSpeed.
         */
        LwU8 bifGenSpeed;

        /*!
         * @brief   Error ID from last call to bifGetBusSpeed or bifChangeBusSpeed
         * @note    Debugging only
         *
         * @details Zero means no error.
         *          255 means bifGetBusSpeed failed.
         *          Other values are defined in bifChangeBusSpeed.
         */
        LwU32 bifErrorId;
    } debug;
};


/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Virtual table
 * @memberof    ClkBif
 * @protected
 */
extern const ClkBif_Virtual clkVirtual_Bif;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkBif
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_Bif(ClkBif *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_Bif");
FLCN_STATUS clkConfig_Bif(ClkBif *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_Bif");
FLCN_STATUS clkProgram_Bif(ClkBif *this, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_Bif");
FLCN_STATUS clkCleanup_Bif(ClkBif *this, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkCleanup_Bif");
#if CLK_PRINT_ENABLED
void clkPrint_Bif(ClkBif *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_Bif");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_BIF_H

