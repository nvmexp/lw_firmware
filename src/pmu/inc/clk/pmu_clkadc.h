/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKADC_H
#define PMU_CLKADC_H

#include "g_pmu_clkadc.h"

#ifndef G_PMU_CLKADC_H
#define G_PMU_CLKADC_H

/*!
 * @file pmu_clkadc.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "main.h"
#include "lwostimer.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_ADC_DEVICE CLK_ADC_DEVICE, ADC_DEVICE, ADC_DEVICE_BASE;

/*!
 * The new LW2080_CTRL_CLK_ADC_ACC_SAMPLE structure has the exact same data layout
 * as the old CLK_ADC_ACC_SAMPLE.  Using this typedef to prevent having to
 * change all references to the old type.
 */
typedef LW2080_CTRL_CLK_ADC_ACC_SAMPLE CLK_ADC_ACC_SAMPLE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET()
 */
#define BOARDOBJGRP_DATA_LOCATION_ADC_DEVICE \
    (&(Clk.clkAdc.super.super))

/*!
 * Macro to get pointer to ADC devices group. Returns NULL if the feature is
 * not supported.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES)
#define CLK_ADCS_GET()                         \
    (Clk.clkAdc.pAdcs)
#else
#define CLK_ADCS_GET()                         \
    (NULL)
#endif

/*!
 * Macro for the INVALID entry in the CLK_ADC_ADDRESS_MAP table.
 */
#define CLK_ADC_ADDRESS_MAP_ILWALID_IDX                                   (0xFFU)

/*!
 * Macro for the INVALID value for the ADC override code as '0' is also a
 * valid entry
 */
#define CLK_ADC_OVERRIDE_CODE_ILWALID                                     (0xFFU)

/*!
 * @brief   Retrieves an object pointer of the CLK_ADC_DEVICE class for the 
 *          given index, or NULL
 *
 * @param[in]   _objIdx Index of an object to retrieve.
 *
 * @return  pointer to CLK_ADC_DEVICE class casted BOARDOBJ object at the
 *          specified index in the group.
 */
#define CLK_ADC_DEVICE_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(ADC_DEVICE, (_objIdx)))

/*!
 * Helper macro to check whether ADC is powered on and enabled
 */
#define CLK_ADC_IS_POWER_ON_AND_ENABLED(pAdcDevice)                             \
    (((pAdcDevice) != NULL) ?                                                   \
     (((pAdcDevice)->bPoweredOn) && ((pAdcDevice)->disableClientsMask == 0U)) : \
     LW_FALSE)

/*!
 * Helper macro for clients to be able to get sensed voltage ADC samples.
 * Any client wanting to read the sensed voltage throug ADCs should use this and
 * no other internal function should be called directly. This helps ensure that
 * clients don't get access to parameters which are not meant for them. 
 */
#define CLK_ADCS_VOLTAGE_READ_EXTERNAL(_adcMask, _pAdcAccSample)              \
    clkAdcsVoltageReadWrapper(&(_adcMask), _pAdcAccSample, LW_FALSE,          \
        LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU)

/*!
 * Helper macro to get the ADC device's HW access state
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC)
#define CLK_ADC_IS_HW_ACCESS_ENABLED(pAdcDevice)                                \
    (((pAdcDevice) != NULL) && ((pAdcDevice)->hwRegAccessClientsMask == 0U))
#else
#define CLK_ADC_IS_HW_ACCESS_ENABLED(pAdcDevice)                                \
    ((pAdcDevice) != NULL)
#endif // PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC

/*!
 * Helper macro to get the ADC device's dmem overlay
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_ADCS_DMEM_DEFINED))
#define CLK_ADCS_DMEM_OVL_INDEX()         (OVL_INDEX_DMEM(clkAdcs))
#else
#define CLK_ADCS_DMEM_OVL_INDEX()         (OVL_INDEX_DMEM(perf))
#endif

/*!
 * Timeout of 5 us for RDACK to reflect programming to _CAPTURE_ACC
 */
#define CLK_ADC_ACC_RDACK_TIMEOUT_US                                      (0x5U)


/*!
 * Helper Macro for checking if the given voltage value is inside the POR range
 */
#define CLK_ADC_IS_VOLTAGE_IN_POR_RANGE(voltuV)         \
    (((voltuV)  >= CLK_NAFLL_LUT_MIN_VOLTAGE_UV()) &&   \
     ((voltuV)  <= CLK_NAFLL_LUT_MAX_VOLTAGE_UV()))

/*!
 * Helper Macro to colwert a given voltage value (in microvolts) to ADC Code
 */
#define CLK_ADC_CODE_GET(voltuV) \
    (((voltuV) - CLK_NAFLL_LUT_MIN_VOLTAGE_UV()) / CLK_NAFLL_LUT_STEP_SIZE_UV())

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Enumeration of register types used to control ADC-s.
 */
#define CLK_ADC_REG_TYPE_CTRL                        0U
#define CLK_ADC_REG_TYPE_OVERRIDE                    1U
#define CLK_ADC_REG_TYPE_ACC                         2U
#define CLK_ADC_REG_TYPE_NUM_SAMPLES                 3U
#define CLK_ADC_REG_TYPE_CAL                         4U
#define CLK_ADC_REG_TYPE_MONITOR                     5U
#if (PMU_PROFILE_GP100 || PMU_PROFILE_GP10X)
#define CLK_ADC_REG_TYPE__COUNT                      6U
#else
#define CLK_ADC_REG_TYPE_CTRL2                       6U
#define CLK_ADC_REG_TYPE_BACKUP_CTRL                 7U
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
#define CLK_ADC_REG_TYPE_RAM_ASSIST_ADC_CTRL         8U
#define CLK_ADC_REG_TYPE_RAM_ASSIST_THRESH           9U
#define CLK_ADC_REG_TYPE_RAM_ASSIST_DBG_0            10U
#define CLK_ADC_REG_TYPE__COUNT                      11U
#else
#define CLK_ADC_REG_TYPE__COUNT                      8U
#endif // PMU_CLK_ADC_RAM_ASSIST
#endif

/*!
 * Structure describing an individual ADC Device
 */
struct CLK_ADC_DEVICE
{
    /*!
     * Board Object super class
     */
    BOARDOBJ    super;

    /*!
     * ADC device ID - @ref LW2080_CTRL_CLK_ADC_ID_<xyz>
     */
    LwU8        id;

    /*!
     * Voltage rail index that the ADC samples.
     */
    LwU8        voltRailIdx;

    /*!
     * Power state of this ADC device.
     */
    LwBool      bPoweredOn;

    /*!
     * Enable state of HW calibration for this ADC device.
     */
    LwBool      bHwCalEnabled;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION)
    /*!
     * Set if dynamic calibration needs to be enabled
     */
    LwBool      bDynCal;
#endif // PMU_PERF_ADC_RUNTIME_CALIBRATION

    /*!
     * ADC MUX SW override mode of operation init to POR value
     * during first construct but can be updated by run time code.
     * @ref LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_<abc>
     *
     * Client MUST trigger VF change to program ADC with new
     * override mode and voltage values.
     */
    LwU8        overrideMode;

    /*!
     * SW cached value of the override code (0-127)
     */
    LwU8        overrideCode;

#if (PMU_PROFILE_GP100)
    /*!
     * Address of the ADC register used as a base (CTRL one).
     *
     * @note    GP100 implementation differs from GP102+ since GP100 was running
     *          our of DMEM and we had to decrease its memory footprint.
     */
    LwU32       adcRegAddrBase;
#else
    /*!
     * Index into register map table
     */
    LwU8        regMapIdx;
#endif

    /*!
     * Mask of clients requested to disable the ADC.
     * @ref LW2080_CTRL_CLK_ADC_CLIENT_ID_<xyz>.
     */
    LwU16       disableClientsMask;

    /*!
     * Mask of NAFLL devices sharing this ADC device
     */
    LwU32       nafllsSharedMask;

    /*!
     * ADC voltage params - aclwmulator and numSamples
     */
    CLK_ADC_ACC_SAMPLE adcAccSample;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC)
    /*!
     * Mask which tracks the synchronized register access state.  When the
     * mask is non-zero, HW register access to the particular ADC device is
     * disabled and the cached register value will be used instead.
     * In particular we are looking at ADC_ACC registers, but can and should
     * easily be extended to any/all ADC related registers in future.
     *
     * TODO: kwadhwa - merge this with @ref CLK_ADC_DEVICE::disableClientsMask
     */
    LwU32       hwRegAccessClientsMask;
#endif // PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_LOGICAL_INDEXING)
    /*!
     * Logical GPC index based API ADC ID of this device.
     * For non-GPC ADC devices, logical and physical IDs are same
     * @ref LW2080_CTRL_CLK_ADC_ID_<XYZ>
     */
    LwU8        logicalApiId;

    /*!
     * Logical Register Map IDX for accessing 
     * virtually addressed registers
     */
    LwU8        logicalRegMapIdx;
#endif
};

/*!
 * Structure to specify the list, number of ADCs and any global parameters
 */
typedef struct CLK_ADC
{
    /*!
     * Version of underlying ADC device table - usually corresponding to the
     * VBIOS table version. @ref LW2080_CTRL_CLK_ADC_DEVICES_<xyz>
     */
    LwU8                version;

    /*!
     * Global disable control for all ADCs. If set to LW_FALSE, the
     * devices are not allowed to be disabled and powered off.
     */
    LwBool              bAdcIsDisableAllowed;

    /*!
     * Mask of all supported ADCs in the chip
     */
    LwU32               adcSupportedMask;

    /*!
     * Mask of ADC_DEVICEs that support NAFLL functionality
     */
    BOARDOBJGRPMASK_E32 nafllAdcDevicesMask;
} CLK_ADC;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @interface CLK_ADC
 *
 * @brief Static function interface to construct the AVFS_ADCOBJS structure including
 * doing any necessary sub-class object construction.
 *
 * @param[in]      pBObjGrp     BOARDOBJGRP pointer
 * @param[in]      pHdrDesc     RM_PMU_BOARDOBJGRP pointer
 * @param[in]      size         size of @ref ppAdcs
 * @param[in/out]  ppAdcs       Pointer to CLK_ADC
 *
 * @return 'FLCN_OK'
 *     If construction successful.
 * @return 'FLCN_ERR_NOT_SUPPORTED'
 *     This implementation of construction is not supported.
 */
#define ClkAdcDevicesIfaceModel10SetHeader(fname) FLCN_STATUS (fname)(BOARDOBJGRP_IFACE_MODEL_10 *pModel10, RM_PMU_BOARDOBJGRP *pHdrDesc, LwU16 size, CLK_ADC **ppAdcs)


// Routines that belong to the .init overlay
void clkAdcPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "clkAdcPreInit");

// Routines that belong to the .perfClkAvfsInit overlay
BoardObjGrpIfaceModel10CmdHandler  (clkAdcDevBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkAdcDevBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10ObjSet (clkAdcDevGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkAdcDevGrpIfaceModel10ObjSetImpl_SUPER");
ClkAdcDevicesIfaceModel10SetHeader (clkIfaceModel10SetHeaderAdcDevices_SUPER)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkIfaceModel10SetHeaderAdcDevices_SUPER");

// Routines that belong to the .perfClkAvfs overlay
FLCN_STATUS clkAdcPowerOnAndEnable(CLK_ADC_DEVICE *pAdcDevice, LwU8 clientId, LwU8 nafllId, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcPowerOnAndEnable");

FLCN_STATUS clkAdcPreSiliconSwOverrideSet(CLK_ADC_DEVICE *pAdcDevice, LwU32 voltageuV, LwU8 clientId, LwBool bSet)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcPreSiliconSwOverrideSet");
BoardObjGrpIfaceModel10CmdHandler (clkAdcDevicesBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkAdcDevicesBoardObjGrpIfaceModel10GetStatus");
BoardObjIfaceModel10GetStatus (clkAdcDeviceIfaceModel10GetStatusEntry_SUPER)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcDeviceIfaceModel10GetStatusEntry_SUPER");
FLCN_STATUS clkAdcCalProgram(CLK_ADC_DEVICE *pAdcDevice, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcCalProgram");
FLCN_STATUS clkAdcCalCoeffCache(CLK_ADC_DEVICE *pAdcDevice)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcCalCoeffCache");
FLCN_STATUS clkAdcsLoad(LwU32 actionMask)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcsLoad");
FLCN_STATUS clkAdcsLoad_EXTENDED(LwU32 actionMask)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcsLoad_EXTENDED");
OsTimerCallback (clkAdcVoltageSampleCallback)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "clkAdcVoltageSampleCallback")
    GCC_ATTRIB_USED();
FLCN_STATUS clkAdcInstCodeGet(CLK_ADC_DEVICE *pAdcDevice, LwU8 *pInstCode)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcInstCodeGet");
FLCN_STATUS clkAdcProgram(CLK_ADC_DEVICE *pAdcDevice, LwU8 overrideCode, LwU8 overrideMode)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcProgram");

// ADC Group interfaces.
FLCN_STATUS clkAdcsComputeCodeOffset(LwBool bVFEEvalRequired, LwBool bTempChange, LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pTargetVoltList)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcsComputeCodeOffset");
FLCN_STATUS clkAdcsProgramCodeOffset(LwBool bPreVoltOffsetProg)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcsProgramCodeOffset");
mockable FLCN_STATUS clkAdcsProgram(LW2080_CTRL_CLK_ADC_SW_OVERRIDE_LIST *pAdcSwOverrideList)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcsProgram");
LwBool clkAdcsEnabled(BOARDOBJGRPMASK_E32 adcMask)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkAdcsEnabled");

// Routines that belong to the .libClkVolt overlay
FLCN_STATUS clkAdcsVoltageReadWrapper(BOARDOBJGRPMASK_E32 *pAdcMask, CLK_ADC_ACC_SAMPLE *pAdcAccSample, LwBool bOnlyUpdateAccSampleCache, LwU8 clientId)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "clkAdcsVoltageReadWrapper");
void clkAdcHwAccessSync(LwBool bAccessEnable, LwU8 clientId)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "clkAdcHwAccessSync");
FLCN_STATUS clkAdcDeviceHwRegisterAccessSync(BOARDOBJGRPMASK_E32 *pAdcMask, LwU8 clientId, LwBool bAccessEnable)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "clkAdcDeviceHwRegisterAccessSync");
FLCN_STATUS clkAdcsAccInit(void)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "clkAdcsAccInit");
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_RAM_ASSIST)
FLCN_STATUS clkAdcsRamAssistCtrlEnable(BOARDOBJGRPMASK_E32 *pAdcMask, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "clkAdcsRamAssistCtrlEnable");
FLCN_STATUS clkAdcsProgramVcritThresholds(BOARDOBJGRPMASK_E32 *pAdcMask, LwU32 vcritThreshLowuV, LwU32 vcritThreshHighuV)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "clkAdcsProgramVcritThresholds");
FLCN_STATUS clkAdcsRamAssistIsEngaged(BOARDOBJGRPMASK_E32 *pAdcMask, LwU32 *pRamAssistEngagedMask)
    GCC_ATTRIB_SECTION("imem_libClkVolt", "clkAdcsRamAssistIsEngaged");
#endif // PMU_CLK_ADC_RAM_ASSIST

/* ------------------------ Include Derived Types -------------------------- */
 // Includes related to CLK_ADC
#include "clk/pmu_clkadcs_1x.h"
#include "clk/pmu_clkadcs_2x.h"

// Includes related to CLK_ADC_DEVICE
#include "clk/pmu_clkadc_v10.h"
#include "clk/pmu_clkadc_v20.h"
#include "clk/pmu_clkadc_v30.h"
#include "clk/pmu_clkadc_v30_isink_v10.h"

#include "clk/pmu_clkadc_reg_map.h"

#endif // G_PMU_CLKADC_H
#endif // PMU_CLKADC_H
