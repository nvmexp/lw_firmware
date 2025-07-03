/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Eric Colter
 * @author  Antone Vogt-Varvak
 */

#ifndef CLK3_DOM_FREQDOMAIN_APLLLDIV_DISP_H
#define CLK3_DOM_FREQDOMAIN_APLLLDIV_DISP_H

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "config/pmu-config.h"
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_DISP_DYN_RAMP_SUPPORTED))

/* ------------------------ Includes --------------------------------------- */

// Superclass
#include "clk3/dom/clkfreqdomain_adynldiv.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

/*!
 * @class   ClkADynLdivFreqDomain
 * @extends ClkADynLdivFreqDomain
 */
typedef ClkADynLdivFreqDomain            ClkDispFreqDomain;
typedef ClkADynLdivFreqDomain_Virtual    ClkDispFreqDomain_Virtual;


/* ------------------------ External Definitions --------------------------- */

//! Per-class Virtual Table
extern const ClkDispFreqDomain_Virtual clkVirtual_DispFreqDomain;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkDispFreqDomain
 * @version     Clocks 3.1 and after
 * @protected
 */
/**@{*/
#define     clkRead_DispFreqDomain       clkRead_ADynLdivFreqDomain       // Inherit
FLCN_STATUS clkConfig_DispFreqDomain(ClkDispFreqDomain *this, const ClkTargetSignal *pTarget)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_DispFreqDomain");
#define     clkProgram_DispFreqDomain    clkProgram_ADynLdivFreqDomain    // Inherit
#define     clkCleanup_DispFreqDomain    clkCleanup_ADynLdivFreqDomain    // Inherit
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#else

/* ------------------------ Includes --------------------------------------- */

// Superclass
#include "clk3/dom/clkfreqdomain_apllldiv.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

/*!
 * @class   ClkAPllLdivFreqDomain
 * @extends ClkAPllLdivFreqDomain
 */
typedef ClkAPllLdivFreqDomain            ClkDispFreqDomain;
typedef ClkAPllLdivFreqDomain_Virtual    ClkDispFreqDomain_Virtual;


/* ------------------------ External Definitions --------------------------- */

//! Per-class Virtual Table
extern const ClkDispFreqDomain_Virtual clkVirtual_DispFreqDomain;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkDispFreqDomain
 * @protected
 */
/**@{*/
#define     clkRead_DispFreqDomain       clkRead_APllLdivFreqDomain       // Inherit
FLCN_STATUS clkConfig_DispFreqDomain(ClkDispFreqDomain *this, const ClkTargetSignal *pTarget)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_DispFreqDomain");
#define     clkProgram_DispFreqDomain    clkProgram_APllLdivFreqDomain    // Inherit
#define     clkCleanup_DispFreqDomain    clkCleanup_APllLdivFreqDomain    // Inherit
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif //  PMU_CLK_DISP_DYN_RAMP_SUPPORTED

#endif // CLK3_DOM_FREQDOMAIN_APLLLDIV_DISP_H

