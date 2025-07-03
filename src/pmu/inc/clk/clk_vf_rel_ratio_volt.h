/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_REL_RATIO_VOLT_H
#define CLK_VF_REL_RATIO_VOLT_H

/*!
 * @file clk_vf_rel_ratio_volt.h
 * @brief @copydoc clk_vf_rel_ratio_volt.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_rel_ratio.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Accessor macro for CLK_VF_REL_RATIO_VOLT::vfSmoothEntriesCount
 *
 * @param[in] pVfRelRatioVolt  CLK_VF_REL_RATIO_VOLT pointer
 *
 * @return @ref CLK_VF_REL_RATIO_VOLT::vfSmoothEntriesCount
 */
#define clkVfRelRatioVoltVfSmoothDataEntriesCountGet(pVfRelRatioVolt)               \
    ((pVfRelRatioVolt)->vfSmoothDataGrp.vfSmoothDataEntriesCount)

/*!
 * Helper Macro to test if the input particular VF relationship object
 * requires smoothening or not. Unlike 
 * @ref clkVfRelRatioVoltIsVFSmoothingRequired, which checks for smoothening
 * at the VF-point level; this macro tests it at the VF-REL level.
 * 
 * The way it is done internally, is to check for the number of valid 
 * VF-smoothing entries. If zero, then no valid entries are present, and 
 * no smoothing required.
 * 
 * Uses @ref clkVfRelRatioVoltVfSmoothDataEntriesCountGet for 
 * implementation.
 * 
 *
 * @return LW_TRUE  Smoothening required for this VF REL.
 * @return LW_FALSE Smoothening not required for this VF REL.
 */
#define clkVfRelRatioVoltIsVfSmoothingRequiredForVFRel(pVfRelRatioVolt) \
    ((clkVfRelRatioVoltVfSmoothDataEntriesCountGet(pVfRelRatioVolt)) > 0)

/*!
 * Helper Macro to test if it is required to implement the VF smoothing logic
 * for a given VF point controlled by this VF Relationship entry
 * 
 * The way this is done here is to check the base voltage of the first 
 * element in the VF Smoothing array, if it is equal to zero. If so,
 * smoothening is not required (no valid base voltage at which to
 * start smoothening). If smoothening is required, the base voltage
 * at least of the first element must be zero.
 * 
 * This follows on from the requirement that base voltage of multiple
 * VF ramp rates (smoothening) must strictly be in increasing order.
 *
 * @return LW_TRUE  If it is required to implement VF smoothing
 * @return LW_FALSE If is is NOT required to implement VF smoothing
 */
#define clkVfRelRatioVoltIsVFSmoothingRequired(pVfRelRatioVolt, voltageuV)    \
    ((clkDomainsVfSmootheningEnforced())                                &&    \
     (clkDomainsVfMonotonicityEnforced())                               &&    \
     (clkVfRelRatioVoltIsVfSmoothingRequiredForVFRel(pVfRelRatioVolt))  &&    \
     ((voltageuV) >= (pVfRelRatioVolt)->vfSmoothDataGrp.vfSmoothDataEntries[LW_U8_MIN].baseVFSmoothVoltuV))

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_VF_REL_RATIO_VOLT and CLK_VF_REL_RATIO_VOLT
 * structures so that can use the type in interface definitions.
 */
typedef struct CLK_VF_REL_RATIO_VOLT CLK_VF_REL_RATIO_VOLT;

/*!
 * Clock VF Relationship - Ratio Voltage
 * Struct containing parameters specific to ratio based VF relationship.
 */
struct CLK_VF_REL_RATIO_VOLT
{
    /*!
     * super class. Must always be first object in the structure.
     */
    CLK_VF_REL_RATIO  super;

    /*!
     * The encapsulated structure containing the count of valid
     * entries and the entries themselves of the VF smoothing
     * data. 
     * 
     * @ LW2080_CTRL_CLK_CLK_VF_REL_RATIO_VOLT_VF_SMOOTH_DATA_GRP
     * */
    LW2080_CTRL_CLK_CLK_VF_REL_RATIO_VOLT_VF_SMOOTH_DATA_GRP 
        vfSmoothDataGrp;
};

/*!
 * @interface CLK_VF_REL_RATIO_VOLT
 * 
 * Accessor function to obtain 
 * CLK_VF_REL_RATIO_VOLT::vfSmoothDataEntries::maxFreqStepSizeMHz. 
 * Iterates over all valid Smoothing data entries, to find the 
 * matching entry, whose base voltage is at or below the provided
 * voltage (in uV) (assumed part of the VF-point). The max frequency
 * step size corresponding to the matching entry is returned.
 * Intended for voltage-dependent frequency step-size.
 * 
 * @param[in]   pVfRelRatioVolt  CLK_VF_REL_RATIO_VOLT pointer.
 * @param[in]   voltageuV        Voltage to find the voltage-dependent step size.
 * 
 * @return The maximum frequency step size in MHz, if VF smooth entry is valid
 * @return LW_U16_MIN, otherwise.
 */
#define ClkVfRelRatioVoltMaxFreqStepSizeMHzGet(fname) LwU16 (fname)(CLK_VF_REL_RATIO_VOLT *pVfRelRatioVolt, LwU32 voltageuV) 

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet                       (clkVfRelGrpIfaceModel10ObjSet_RATIO_VOLT)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfRelGrpIfaceModel10ObjSet_RATIO_VOLT");

// CLK_VF_REL interfaces
ClkVfRelSmoothing                       (clkVfRelSmoothing_RATIO_VOLT)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelSmoothing_RATIO_VOLT");

// CLK_VF_REL_RATIO_VOLT interfaces
ClkVfRelRatioVoltMaxFreqStepSizeMHzGet  (clkVfRelRatioVoltMaxFreqStepSizeMHzGet)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelRatioVoltMaxFreqStepSizeMHzGet");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_VF_REL_RATIO_VOLT_H
