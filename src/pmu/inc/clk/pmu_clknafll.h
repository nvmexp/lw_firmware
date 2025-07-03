/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKNAFLL_H
#define PMU_CLKNAFLL_H

#include "g_pmu_clknafll.h"

#ifndef G_PMU_CLKNAFLL_H
#define G_PMU_CLKNAFLL_H

/*!
 * @file pmu_clknafll.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "main.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_NAFLL_DEVICE CLK_NAFLL_DEVICE, NAFLL_DEVICE, NAFLL_DEVICE_BASE;

/* ------------------------ Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "clk/pmu_clkadc.h"
#include "clk/pmu_clknafll_regime.h"
#include "clk/pmu_clknafll_lut.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_NAFLL_DEVICE \
    (&(Clk.clkNafll.super.super))

/*!
 * Macro for the INVALID entry in the CLK_NAFLL_ADDRESS_MAP table.
 */
#define CLK_NAFLL_ADDRESS_MAP_ILWALID_IDX                                 (0xFFU)

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define CLK_NAFLL_DEVICE_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(NAFLL_DEVICE, (_objIdx)))

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Structure describing the state of a NAFLL
 */
struct CLK_NAFLL_DEVICE
{
    /*!
     * Board Object super class
     */
    BOARDOBJ              super;

    /*!
     * The global ID @ref LW2080_CTRL_PERF_NAFLL_ID_<xyz> of this NAFLL device.
     */
    LwU8                  id;

    /*!
     * M-Divider value for this NAFLL device
     */
    LwU8                  mdiv;

    /*!
     * Voltage rail index to use for programming the V/F lwrve in the LUT
     */
    LwU8                  railIdxForLut;

    /*!
     * Index into the frequency controller table
     */
    LwU8                  freqCtrlIdx;

    /*!
     * Flag to indicate whether to skip setting Pldiv below DVCO min or not.
     */
    LwBool                bSkipPldivBelowDvcoMin;

    /*!
     * Input pre-divider reference clock frequency for this NAFLL in MHz.
     * Input frequency for this NAFLL device = @ref inputRefClkMHz / @ref inputRefClkDivVal.
     * 
     * To-do akshatam: Today we get NAFLL input frequency from the VBIOS. This is
     * an overhead and not scalable when it comes to input frequency changes such
     * as Bug 3014997. We should look at having PMU being self-sufficient for
     * reading NAFLL input clock just like what we do for other clocks.
     */
    LwU16                 inputRefClkFreqMHz;

    /*!
     * REF_CLK divider value for this NAFLL device.
     */
    LwU16                 inputRefClkDivVal;

    /*!
     * VFGain for this NAFLL device
     */
    LwU16                 vfGain;

#if (PMU_PROFILE_GP100)
    /*!
     * Address of the NAFLL register used as a base (WR_ADDR one).
     *
     * @note    GP100 implementation differs from GP102+ since GP100 was running
     *          our of DMEM and we had to decrease its memory footprint.
     */
    LwU32                 nafllRegAddrBase;
#else
    /*!
     * Index into register map table
     */
    LwU8                  regMapIdx;
#endif

    /*!
     * The clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz> associated with
     * this NAFLL device.
     */
    LwU32                 clkDomain;

    /*!
     * The LUT device for this NAFLL.                                                         .
     * There is a 1-1 correpondence between an NAFLL and the LUT.
     * The CLK_LUT_DEVICE should capture the state of the LUT HW.
     */
    CLK_LUT_DEVICE        lutDevice;

    /*!
     * The DVCO for this NAFLL.                                                         .
     * There is a 1-1 correpondence between an NAFLL and the DVCO.
     * The CLK_NAFLL_DVCO should capture the static state of the DVCO.
     */
    CLK_NAFLL_DVCO        dvco;

    /*!
     * The regime control logic for this NAFLL.
     */
    CLK_NAFLL_REGIME_DESC regimeDesc;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_BROADCAST)
    /*!
     * Mask of NAFLLs which are broadcast secondary devices of the this NAFLL.
     * When programming this NAFLL's LUT, all of this NAFLL's settings should
     * be broadcast to each of the NAFLLs specified in this mask.
     */
    BOARDOBJGRPMASK_E32   lutProgBroadcastSecondaryMask;
#endif

    /*!
     * Pointer to the Logic ADC device that serves this NAFLL. Note, there need
     * not be  a 1-1 mapping between the Logic ADC and the NAFLL device, i.e.
     * a Logic ADC may feed more than one NAFLL.
     */
    CLK_ADC_DEVICE       *pLogicAdcDevice;

    /*!
     * Pointer to the Sram ADC device that serves this NAFLL. Note, there need
     * not be  a 1-1 mapping between the SRAM ADC and the NAFLL device, i.e.
     * a SRAM ADC may feed more than one NAFLL.
     */
    CLK_ADC_DEVICE       *pSramAdcDevice;

    /*!
     * Struct containing the dynamic status parameters of NAFLL device.
     * On CLK 3.0, this struct will be stored in NAFLL freq source object.
     *
     * @ref LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL
     *
     * On PSTATE 3.0, this struct will be allocated in NAFLL object during
     * NAFLL construct whereas on PSTATE 3.5 and later, this struct will be
     * allocated in CLK 3.0 object during NAFLL frequency domain construction.
     */
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus;

    /*!
     * Interfaces
     */
};

/*!
 * Structure describing the various NAFLLs in the GPU.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class.  Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E32 super;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_BROADCAST)
    /*!
     * Mask of NAFLLs to iterate over when programming the NAFLL LUTs.  This
     * mask is separate from the @ref BOARDOBJGRP_E32 @ref super mask because it
     * will map to the NAFLLs which will be used as the entry points for
     * broadcast LUT programming.
     */
    BOARDOBJGRPMASK_E32 lutProgPrimaryMask;
#endif

    /*!
     * Mask of all supported NAFLLs in the chip
     */
    LwU32           nafllSupportedMask;

    /*!
    * The step size for the LUT entries.
    */
    LwU32           lutStepSizeuV;

    /*!
    * The minimum voltage in uV thats present in the LUT. i.e. the base
    * "uncalibrated voltage" corresponding to code 0 in the LUT
    */
    LwU32           lutMilwoltageuV;

    /*!
    * The maximum voltage in uV thats present in the LUT. i.e. the base
    * "uncalibrated voltage" corresponding to code 80 in the LUT
    */
    LwU32           lutMaxVoltageuV;

   /*!
    * Total number of entries that each table in the LUT can hold.
    */
    LwU8            lutNumEntries;

   /*!
    * The worst case DVCO min frequency across pstate Vmin lwrve for all
    * NAFLL devices.
    */
    LwU16           maxDvcoMinFreqMHz;

   /*!
     * DVCO Min Frequency MHz. This is a cached copy in PERF DMEM which
     * is ALWAYS in sync with DVCO Min frequency in CLK 3.0 DMEM. This
     * is done to reduce the DMEM inpact on PERF Clients that just wants
     * to know DVCO min frequency and cannot take a hit of CLK 3.0 DMEM.
     *
     * Starting GA10X, this value is stored on a per rail index basis
     * @ref CLK_NAFLL_DEVICE::railIdxForLut since we can have different
     * voltage rails driving various NAFLL devices.
     */
    LwU16           dvcoMinFreqMHz[LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS];
} CLK_NAFLL;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// Routines that belong to the .perfVf overlay
CLK_NAFLL_DEVICE * clkNafllPrimaryDeviceGet(LwU32 clkDomain)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkNafllPrimaryDeviceGet");
FLCN_STATUS clkNafllNdivFromFreqCompute (CLK_NAFLL_DEVICE *pNafllDev, LwU16 freqMHz, LwU16 *pNdiv, LwBool bFloor)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkNafllNdivFromFreqCompute");
mockable FLCN_STATUS clkNafllGetIndexByFreqMHz(LwU32 clkDomain, LwU16 freqMHz, LwBool bFloor, LwU16 *pIdx)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkNafllGetIndexByFreqMHz");
mockable FLCN_STATUS clkNafllGetFreqMHzByIndex(LwU32 clkDomain, LwU16 idx, LwU16 *pFreqMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkNafllGetFreqMHzByIndex");
LwU16       clkNafllFreqFromNdivCompute (CLK_NAFLL_DEVICE *pNafllDev, LwU16 ndiv)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkNafllFreqFromNdivCompute");
mockable FLCN_STATUS clkNafllFreqMHzQuantize(LwU32 clkDomain, LwU16 *pFreqMHz, LwBool bFloor)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkNafllFreqMHzQuantize");
FLCN_STATUS clkNafllFreqsIterate(LwU32 clkDomain, LwU16 *pFreqMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkNafllFreqsIterate");
FLCN_STATUS clkNafllDvcoMinFreqMHzGet(LwU32 clkDomain, LwU16 *pFreqMHz, LwBool *pBSkipPldivBelowDvcoMin)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkNafllDvcoMinFreqMHzGet");

// Routines that belong to the .perfClkAvfsInit overlay
BoardObjGrpIfaceModel10CmdHandler   (clkNafllDevBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkNafllDevBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10ObjSet       (clkNafllGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkNafllGrpIfaceModel10ObjSet_SUPER");
CLK_NAFLL_DEVICE *      clkNafllFreqCtrlByIdxGet(LwU8 controllerIdx)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkNafllFreqCtrlByIdxGet");

// Routines that belong to the .perfClkAvfs overlay
CLK_NAFLL_DEVICE *  clkNafllDeviceGet(LwU8 nafllId)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllDeviceGet");
FLCN_STATUS         clkNafllDomainToIdMask(LwU32 clkDomain, LwU32 *pNafllIdMask)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllDomainToIdMask");
BoardObjGrpIfaceModel10CmdHandler     (clkNafllDevicesBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkNafllDevicesBoardObjGrpIfaceModel10GetStatus");
BoardObjIfaceModel10GetStatus             (clkNafllDeviceIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllDeviceIfaceModel10GetStatus");
FLCN_STATUS         clkNafllsLoad(LwU32 actionMask)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllsLoad");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/pmu_clknafll_10.h"
#include "clk/pmu_clknafll_20.h"
#include "clk/pmu_clknafll_30.h"

#endif // G_PMU_CLKNAFLL_H
#endif // PMU_CLKNAFLL_H
