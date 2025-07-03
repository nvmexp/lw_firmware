/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkadc.c
 * @brief  File for common ADC CLK routines applicable across all platforms.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clkAdcDevIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "s_clkAdcDevIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clkAdcDevIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "s_clkAdcDevIfaceModel10SetEntry");
static BoardObjIfaceModel10GetStatus              (s_clkAdcDeviceIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkAdcDeviceIfaceModel10GetStatus");
static FLCN_STATUS s_clkAdcEnable(CLK_ADC_DEVICE *pAdcDevice, LwU8 clientId, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkAdcEnable");
static FLCN_STATUS s_clkAdcPowerOn(CLK_ADC_DEVICE *pAdcDevice, LwU8 clientId, LwBool bPowerOn)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkAdcPowerOn");

/* ------------------------- Global Variables ------------------------------- */
/*!
 * @brief   Array of all vtables pertaining to different CLK_ADC_DEVICE object types.
 */
BOARDOBJ_VIRTUAL_TABLE *ClkAdcDeviceVirtualTables[LW2080_CTRL_BOARDOBJ_TYPE(CLK, ADC_DEVICE, MAX)] =
{
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, ADC_DEVICE, BASE)]  = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, ADC_DEVICE, V10) ]  = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, ADC_DEVICE, V20) ]  = CLK_ADC_DEVICE_V20_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, ADC_DEVICE, V30) ]  = NULL,
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Pre-inits the CLK_ADC feature, including loading any necessary HW state.
 */
void
clkAdcPreInit(void)
{
    // Keep the access to ADC registers disabled by default.
    Clk.clkAdc.adcAccessDisabledMask = BIT(LW2080_CTRL_CLK_ADC_CLIENT_ID_INIT);
}

/*!
 * @brief ADC_DEVICE handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydetails BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
clkAdcDevBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = FLCN_OK;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfsInit)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32, // _grpType
            ADC_DEVICE,                              // _class
            pBuffer,                                 // _pBuffer
            s_clkAdcDevIfaceModel10SetHeader,              // _hdrFunc
            s_clkAdcDevIfaceModel10SetEntry,               // _entryFunc
            pascalAndLater.clk.clkAdcDeviceGrpSet,   // _ssElement
            CLK_ADCS_DMEM_OVL_INDEX(),               // _ovlIdxDmem
            ClkAdcDeviceVirtualTables);              // _ppObjectVtables
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        goto clkAdcDevBoardObjGrpIfaceModel10Set_exit;
    }

clkAdcDevBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @brief Constructor for the ADC devices group base class
 *
 * @copydetails ClkAdcDevicesIfaceModel10SetHeader
 */
FLCN_STATUS
clkIfaceModel10SetHeaderAdcDevices_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10            *pModel10,
    RM_PMU_BOARDOBJGRP    *pHdrDesc,
    LwU16                   size,
    CLK_ADC               **ppAdcs
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJGRP_SET_HEADER *pAdcSet =
        (RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    CLK_ADC    *pAdcs   = NULL;
    FLCN_STATUS status  = FLCN_OK;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto _clkConstructAdcDevices_SUPER_exit;
    }

    // Allocate memory for CLK_ADC now if it hasn't been allocated already
    if (*ppAdcs == NULL)
    {
        *ppAdcs = (CLK_ADC *)lwosCalloc(pBObjGrp->ovlIdxDmem, size);
        if (*ppAdcs == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            PMU_BREAKPOINT();
            goto _clkConstructAdcDevices_SUPER_exit;
        }
    }

    pAdcs = (*ppAdcs);

    // Copy global data
    pAdcs->version              = pAdcSet->version;
    pAdcs->bAdcIsDisableAllowed = pAdcSet->bAdcIsDisableAllowed;

   clkAdcAllMaskGet_HAL(&Clk, &pAdcs->adcSupportedMask);

    boardObjGrpMaskInit_E32(&(pAdcs->nafllAdcDevicesMask));
    status = boardObjGrpMaskImport_E32(&(pAdcs->nafllAdcDevicesMask),
                                       &(pAdcSet->nafllAdcDevicesMask));
    if (status != FLCN_OK)
    {
        goto _clkConstructAdcDevices_SUPER_exit;
    }

_clkConstructAdcDevices_SUPER_exit:
    return status;
}

/*!
 * @brief ADC Device construct super-class implementation.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
clkAdcDevGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    CLK_ADC_DEVICE                            *pDev;
    FLCN_STATUS                                status;
    RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJ_SET    *pTmpDesc =
        (RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJ_SET *)pBoardObjDesc;
    LwBool                                     bFirstConstruct = (*ppBoardObj == NULL);

    //
    // Return with error if the ID isn't include in the adcSupportedMask.
    //  We don't want to construct objects if the ID is not supported.
    //
    if ((LWBIT_TYPE(pTmpDesc->id, LwU32) & Clk.clkAdc.pAdcs->adcSupportedMask) == 0U)
    {
        status = FLCN_ERROR;
        goto clkAdcDevGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto clkAdcDevGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pDev, *ppBoardObj, CLK, ADC_DEVICE, BASE,
                                  &status, clkAdcDevGrpIfaceModel10ObjSetImpl_SUPER_exit);

    pDev->id               = pTmpDesc->id;
    pDev->voltRailIdx      = pTmpDesc->voltRailIdx;
    pDev->overrideMode     = pTmpDesc->overrideMode;
    pDev->nafllsSharedMask = pTmpDesc->nafllsSharedMask;
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION)
    pDev->bDynCal          = pTmpDesc->bDynCal;
#endif // PMU_PERF_ADC_RUNTIME_CALIBRATION

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_LOGICAL_INDEXING)
    pDev->logicalApiId     = pTmpDesc->logicalApiId;
#endif // PMU_CLK_ADC_LOGICAL_INDEXING

    // No state change required across soft reboot, only required at first construct
    if (bFirstConstruct)
    {
        //
        // Note: We are assuming that the default state of the ADC is powered off
        // and disabled when the ADC device is constructed. There will be PMU code
        // that will power on and enable the ADCs before it gets used.
        //
        // Also initialize the state of the client masks disable and hwAccess
        // both to - _PMU as these will get reset once the ADCs get enabled
        //
        pDev->bPoweredOn         = LW_FALSE;
        pDev->disableClientsMask = BIT(LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU);

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC)
        pDev->hwRegAccessClientsMask = BIT(LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU);
#endif // PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC

        status = clkAdcRegMapInit_HAL(&Clk, pDev);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkAdcDevGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }
    }

clkAdcDevGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @brief Load all ADC_DEVICES.
 *
 * @param[in]   actionMask  contains the bit-mask of specific action to be taken
 *                          @ref LW_RM_PMU_CLK_LOAD_ACTION_MASK_<XYZ>
 *
 * @return FLCN_OK  If all ADC devices loaded successfully
 * @return other    Descriptive error code from sub-routines on failure
 */
FLCN_STATUS
clkAdcsLoad
(
    LwU32 actionMask
)
{
    CLK_ADC_DEVICE *pDev;
    LwBoardObjIdx    devIdx;
    FLCN_STATUS      status = FLCN_OK;

    if ((FLD_TEST_DRF(_RM_PMU_CLK_LOAD, _ACTION_MASK,
                      _ADC_HW_CAL_PROGRAM, _YES, actionMask)) &&
        (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_HW_CALIBRATION)))
    {
        // Program the ADC calibration values and enable HW ADC calibration
        BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pDev, devIdx, NULL)
        {
            status = clkAdcCalProgram(pDev, LW_TRUE);
            if (status != FLCN_OK)
            {
                goto clkAdcsLoad_exit;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

    //
    // If supported, power on and enable NAFLL ADCs. ISINK ADCs don't have this
    // capability due to L3 write priv protection. If new ADC device types
    // other than NAFLL and ISINK are added in future then this code should
    // be revisited as we're looping over NAFLL ADCs only.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_ALWAYS_ON))
    {
        // Program the ADC calibration values and enable HW ADC calibration
        BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pDev, devIdx,
            &Clk.clkAdc.pAdcs->nafllAdcDevicesMask.super)
        {
            status = clkAdcPowerOnAndEnable(pDev,
                        LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU,
                        LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED,
                        LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkAdcsLoad_exit;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_EXTENDED))
    {
        status = clkAdcsLoad_EXTENDED(actionMask);
    }

clkAdcsLoad_exit:
    return status;
}

/*!
 * @brief Enable or disables a ADC device
 *
 * @note ISINK ADCs are enabled in the VBIOS devinit and can't be enabled
 *       or disabled at runtime. Caller function @ref clkAdcPowerOnAndEnable
 *       handles this requirement.
 *
 * @param[in]   pAdcDevice  Pointer to ADC device
 * @param[in]   clientId    Client-Id that is requesting enable or disable
 * @param[in]   bEnable     Flag for enable/disable
 *
 * @return FLCN_OK                   Successfully enabled/disabled the ADC or
 *                                   nothing changed
 * @return FLCN_ERROR                If the device cannot be enabled/disabled
 *                                   for any reason.
 * @return FLCN_ERR_ILWALID_ARGUMENT If pAdcDevice is not valid pointer or
 *                                   if the client-id is invalid
 * @return other                     Descriptive error code from sub-routines
 *                                   on failure
 */
static FLCN_STATUS
s_clkAdcEnable
(
    CLK_ADC_DEVICE *pAdcDevice,
    LwU8            clientId,
    LwBool          bEnable
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU16       prevMask;

    // Sanity checks
    if ((pAdcDevice == NULL) ||
        (clientId >= LW2080_CTRL_CLK_ADC_CLIENT_ID_MAX_NUM))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_clkAdcEnable_exit;
    }

    //
    // Nothing to do if we are not allowed to disable the ADCs
    // The LPWR GPC-RG feature is an exception to this flag since we explicity
    // need to disable and power-off the ADCs to avoid inconsistent states
    //
    if (!bEnable && !Clk.clkAdc.pAdcs->bAdcIsDisableAllowed &&
        (clientId != LW2080_CTRL_CLK_ADC_CLIENT_ID_LPWR_GRRG))
    {
        status = FLCN_OK;
        goto s_clkAdcEnable_exit;
    }

    // Nothing to do if we are trying to enable a device if its already enabled
    if (bEnable && (pAdcDevice->disableClientsMask == 0U))
    {
        status = FLCN_OK;
        goto s_clkAdcEnable_exit;
    }

    if (bEnable && !pAdcDevice->bPoweredOn)
    {
        //
        // return an error if we are trying to enable a device which is not
        // powered on.
        //
        status = FLCN_ERROR;
        goto s_clkAdcEnable_exit;
    }

    prevMask = pAdcDevice->disableClientsMask;

    // Update the disable mask
    if (!bEnable)
    {
        pAdcDevice->disableClientsMask |= LWBIT_TYPE(clientId, LwU16);
    }
    else
    {
        pAdcDevice->disableClientsMask &= ~LWBIT_TYPE(clientId, LwU16);
    }

    // Enable/Disable the ADC device if the state changed
    if (((prevMask == 0U) &&                        // Was enabled
         (pAdcDevice->disableClientsMask != 0U)) || // and getting disabled now
        ((prevMask != 0U) &&                        // Was disabled
         (pAdcDevice->disableClientsMask == 0U)))   // and getting enabled now
    {
        status = clkAdcEnable_HAL(&Clk, pAdcDevice, bEnable);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkAdcEnable_exit;
        }
    }

s_clkAdcEnable_exit:
    return status;
}

/*!
 * @brief Powers on or off an ADC device
 *
 * @note ISINK ADCs are powered on in the VBIOS devinit and can't be powered
 *       on or off at runtime. Caller function @ref clkAdcPowerOnAndEnable
 *       handles this requirement.
 *
 * @param[in]   pAdcDevice  Pointer to ADC device
 * @param[in]   clientId    Client-Id that is requesting power-on or power-off
 * @param[in]   bPowerOn    Flag for power on/off
 *
 * @return FLCN_OK                   Successfully powered on/off the ADC or
 *                                   nothing changed
 * @return FLCN_ERROR                If the device cannot be powered on/off
 *                                   for any reason.
 * @return FLCN_ERR_ILWALID_ARGUMENT If pAdcDevice is not valid pointer
 * @return other                     Descriptive error code from sub-routines
 *                                   on failure
 */
static FLCN_STATUS
s_clkAdcPowerOn
(
    CLK_ADC_DEVICE *pAdcDevice,
    LwU8            clientId,
    LwBool          bPowerOn
)
{
    FLCN_STATUS status  = FLCN_OK;

    if (pAdcDevice == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_clkAdcPowerOn_exit;
    }

    //
    // Nothing to do if we are not allowed to disable the ADCs. Note, we
    // rely on bAdcIsDisableAllowed to tell us if we can power off or not.
    // There is no separate control.
    //
    // The LPWR GPC-RG feature is an exception to this flag since we explicity
    // need to disable and power-off the ADCs to avoid inconsistent states
    //
    if (!bPowerOn && !Clk.clkAdc.pAdcs->bAdcIsDisableAllowed &&
        (clientId != LW2080_CTRL_CLK_ADC_CLIENT_ID_LPWR_GRRG))
    {
        status = FLCN_OK;
        goto s_clkAdcPowerOn_exit;
    }

    if (bPowerOn == pAdcDevice->bPoweredOn)
    {
        // Nothing to do if the power state is same as what is being requested
        status = FLCN_OK;
        goto s_clkAdcPowerOn_exit;
    }

    if (!bPowerOn && (pAdcDevice->disableClientsMask == 0U))
    {
        //
        // return an error if we are trying to power off a device which is not
        // disabled
        //
        status = FLCN_ERROR;
        goto s_clkAdcPowerOn_exit;
    }

    status = clkAdcPowerOn_HAL(&Clk, pAdcDevice, bPowerOn);
    if (status != FLCN_OK)
    {
        goto s_clkAdcPowerOn_exit;
    }

    // Cache the power status in the ADC device
    pAdcDevice->bPoweredOn = bPowerOn;

s_clkAdcPowerOn_exit:
    return status;
}

/*!
 * @brief Power On and enable / disable ADC.
 *
 * @param[in]   pAdcDevice  Pointer to the ADC device
 * @param[in]   clientId    Client-Id that is requesting enable or disable
 * @param[in]   nafllId     Nafll-Id that is requesting enable or disable
 * @param[in]   bEnable     enable or disable
 *
 * @return FLCN_OK                   Successfully enabled/disabled the ADC device
 * @return FLCN_ERR_ILWALID_ARGUMENT If pAdcDevice is not valid pointer
 * @return other                     Descriptive error code from sub-routines
 *                                   on failure
 */
FLCN_STATUS
clkAdcPowerOnAndEnable
(
    CLK_ADC_DEVICE     *pAdcDevice,
    LwU8                clientId,
    LwU8                nafllId,
    LwBool              bEnable
)
{
    LwU8                devIdx;
    FLCN_STATUS         status;
    BOARDOBJGRPMASK_E32 adcMask;

    // Sanity check the input args.
    if (pAdcDevice == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcPowerOnAndEnable_exit;
    }

    //
    // ISINK ADCs are powered on & enabled in the VBIOS devinit and can't be
    // disabled at runtime so bail out early.
    //
    if (BOARDOBJ_GET_TYPE(pAdcDevice) ==
            LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10)
    {
        status = FLCN_OK;
        goto clkAdcPowerOnAndEnable_exit;
    }

    //
    // Client can pass undefined nafll id if it wants to turn on/off
    // all ADCs as part on init, load or unload operations.
    //
    if (nafllId != LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED)
    {
        //
        // If ADC is shared among multiple NAFLL devices, then it must
        // be kept power on and enabled if any of the shared NAFLL is
        // in FR / VR. In other words, ADC can only be disabled/enabled
        // when requesting NAFLL is transitioning to/from FFR and the
        // rest of the NAFLLs sharing the same ADC are already in FFR.
        //
        FOR_EACH_INDEX_IN_MASK(32, devIdx, pAdcDevice->nafllsSharedMask)
        {
            LwU8 lwrrentRegimeId = clkNafllRegimeByIdGet(devIdx);
            if ((devIdx != nafllId) &&
                (LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR !=
                 lwrrentRegimeId) &&
                (LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN !=
                 lwrrentRegimeId))
            {
                status  = FLCN_OK;
                goto clkAdcPowerOnAndEnable_exit;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

    // Initialize the adcMask to point to the current ADC
    boardObjGrpMaskInit_E32(&adcMask);
    boardObjGrpMaskBitSet(&adcMask, BOARDOBJ_GET_GRP_IDX(&pAdcDevice->super));

    //
    // We need to have the IDDQ and ENABLE to happen atomically in order to
    // avoid inconsistet ADC states where we could have a disabled+powered-on
    // or an enabled+powered-off ADC.
    //
    appTaskCriticalEnter();
    {
        if (bEnable)
        {
            status = s_clkAdcPowerOn(pAdcDevice, clientId, bEnable);
            if (status != FLCN_OK)
            {
                goto clkAdcPowerOnAndEnable_exit;
            }

            status = s_clkAdcEnable(pAdcDevice,
                                  clientId,
                                  bEnable);
            if (status != FLCN_OK)
            {
                goto clkAdcPowerOnAndEnable_exit;
            }

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC)
            //
            // Synchronize the access to HW registers for the ADC device with the
            // device's enablement state
            //
            status = clkAdcDeviceHwRegisterAccessSync(&adcMask, clientId, bEnable);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkAdcPowerOnAndEnable_exit;
            }
#endif // PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC
        }
        else
        {
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC)
            //
            // Synchronize the access to HW registers for the ADC device with the
            // device's enablement state
            //
            status = clkAdcDeviceHwRegisterAccessSync(&adcMask, clientId, bEnable);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkAdcPowerOnAndEnable_exit;
            }
#endif // PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC

            status = s_clkAdcEnable(pAdcDevice,
                                  clientId,
                                  bEnable);
            if (status != FLCN_OK)
            {
                goto clkAdcPowerOnAndEnable_exit;
            }

            status = s_clkAdcPowerOn(pAdcDevice, clientId, bEnable);
            if (status != FLCN_OK)
            {
                goto clkAdcPowerOnAndEnable_exit;
            }
        }
    }
    appTaskCriticalExit();

clkAdcPowerOnAndEnable_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 */
FLCN_STATUS
clkAdcDeviceIfaceModel10GetStatusEntry_SUPER
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS                    status  = FLCN_OK;
    CLK_ADC_DEVICE                *pAdcDev;
    RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJ_GET_STATUS
                                  *pAdcGetStatus;
    if (pPayload == NULL)
    {
        return FLCN_ERROR;
    }

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pAdcDev, pBoardObj, CLK, ADC_DEVICE, BASE,
                                  &status, clkAdcDeviceIfaceModel10GetStatusEntry_SUPER_exit);
    pAdcGetStatus = (RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJ_GET_STATUS *)pPayload;

    pAdcGetStatus->overrideCode       = pAdcDev->overrideCode;
    pAdcGetStatus->sampledCode        = pAdcDev->adcAccSample.sampledCode;
    pAdcGetStatus->actualVoltageuV    = pAdcDev->adcAccSample.actualVoltageuV;
    pAdcGetStatus->correctedVoltageuV = pAdcDev->adcAccSample.correctedVoltageuV;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_EXTENDED))
    {
        // Read back the ADC_MONITOR for the instantaneous ADC CODE
        status = clkAdcInstCodeGet(pAdcDev, &pAdcGetStatus->instCode);
    }

clkAdcDeviceIfaceModel10GetStatusEntry_SUPER_exit:
    return status;
}

/*!
 * @brief Queries all ADC devices
 *
 * @copydetails BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
clkAdcDevicesBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
    };

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_EXTENDED))
    {
        OSTASK_OVL_DESC ovlDescListVolt[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkVolt)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListVolt);
        {
            // Read back the current voltage values
            clkAdcVoltageSampleCallback(NULL, 0, 0);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListVolt);
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32,       // _grpType
            ADC_DEVICE,                                     // _class
            pBuffer,                                        // _pBuffer
            NULL,                                           // _hdrFunc
            s_clkAdcDeviceIfaceModel10GetStatus,                        // _entryFunc
            pascalAndLater.clk.clkAdcDeviceGrpGetStatus);   // _ssElement
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Callwlates and programs ADC calibration gain/offset values
 *
 * @param[in]   pAdcDevice    Pointer to ADC device
 * @param[in]   bEnable       Flag to indicate enable/disable operation
 *
 * @return FLCN_OK                   Operation completed successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT Invalid ADC device
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
FLCN_STATUS
clkAdcCalProgram
(
    CLK_ADC_DEVICE *pAdcDevice,
    LwBool          bEnable
)
{
    FLCN_STATUS status  = FLCN_OK;

    if (pAdcDevice == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcCalProgram_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_STATIC_CALIBRATION))
    {
        status = clkAdcCalProgram_HAL(&Clk, pAdcDevice, bEnable);
        if (status != FLCN_OK)
        {
            goto clkAdcCalProgram_exit;
        }
    }

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION)
    if (pAdcDevice->bDynCal &&
        bEnable)
    {
        status = clkAdcCalCoeffCache(pAdcDevice);
        if (status != FLCN_OK)
        {
            goto clkAdcCalProgram_exit;
        }
    }
#endif // PMU_PERF_ADC_RUNTIME_CALIBRATION

    // Cache the HW calibration status in the ADC device
    pAdcDevice->bHwCalEnabled = bEnable;

clkAdcCalProgram_exit:
    return status;
}

/*!
 * @brief Cache the coefficients for temperature and voltage-temperature (V,T)
 *          based offset and gain correction respectively for ADC calibration.
 *
 * @param[in]   pAdcDevice    Pointer to ADC device
 *
 * @return FLCN_OK                   Operation completed successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT Invalid ADC device
 * @return FLCN_ERR_NOT_SUPPORTED    ADC device type doesn't support the interface
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
FLCN_STATUS
clkAdcCalCoeffCache
(
    CLK_ADC_DEVICE *pAdcDevice
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (pAdcDevice == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkAdcCalCoeffCache_exit;
    }

    switch (BOARDOBJ_GET_TYPE(pAdcDevice))
    {
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V30))
            {
                status = clkAdcCalCoeffCache_V30(pAdcDevice);
            }
            break;
        }
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V30_ISINK_V10))
            {
                status = clkAdcV30IsinkCalCoeffCache_V10(pAdcDevice);
            }
            break;
        }
        default:
        {
            // Do Nothing
            break;
        }
    }

clkAdcCalCoeffCache_exit:
    return status;
}

/*!
 * @brief Check if all the ADCs in the mask are powered on and enabled
 *
 * @return LW_FALSE   If the ADC mask is out of what we support or at least one
 *    of the ADCs in the mask is disabled.
 * @return LW_TRUE    If all the ADCs in the given mask are enabled
 */
LwBool
clkAdcsEnabled
(
    BOARDOBJGRPMASK_E32     adcMask
)
{
    CLK_ADC_DEVICE *pAdcDev;
    LwBoardObjIdx   adcDevIdx;

    // Sanity check the mask
    if (boardObjGrpMaskIsZero(&adcMask) ||
        !boardObjGrpMaskIsSubset(&adcMask, &Clk.clkAdc.super.objMask))
    {
        return LW_FALSE;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, adcDevIdx, &adcMask.super)
    {
        if ((!pAdcDev->bPoweredOn)|| (pAdcDev->disableClientsMask != 0U))
        {
            return LW_FALSE;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return LW_TRUE;
}

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC)
/*!
 * @brief Helper interface which synchronizes access to ADC registers per ADC device.
 *
 * @param[in] pAdcMask       Pointer to the mask of ADC devices to loop over
 * @param[in] clientId       ID of the client that wants to enable/disable the
 *                           access.
 * @param[in] bAccessEnable  Boolean indicating whether access is to enabled
 *                           or disabled.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT  In case any argument validation fails
 * @return other                      Bubble up any other status values from
 *                                    calls within
 */
FLCN_STATUS
clkAdcDeviceHwRegisterAccessSync
(
    BOARDOBJGRPMASK_E32 *pAdcMask,
    LwU8                 clientId,
    LwBool               bAccessEnable
)
{
    CLK_ADC_DEVICE *pAdcDev;
    FLCN_STATUS     status = FLCN_OK;
    LwBoardObjIdx   adcDevIdx;

    //
    // Return early if ADC/NAFLL devices are not supported eg RTL runs with
    // no Alocks & Avfs tables
    //
    if (!CLK_IS_AVFS_SUPPORTED())
    {
        goto clkAdcDeviceHwRegisterAccessSync_exit;
    }

    // Return early for invalid params
    if (clientId >= LW2080_CTRL_CLK_ADC_CLIENT_ID_MAX_NUM)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkAdcDeviceHwRegisterAccessSync_exit;
    }


    // Cache the last value just before disabling
    if (!bAccessEnable)
    {
        status = clkAdcsVoltageReadWrapper(pAdcMask, NULL, LW_TRUE, clientId);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkAdcDeviceHwRegisterAccessSync_exit;
        }
    }

    //
    // Elevate to critical to synchronize protected the SW state.
    //
    appTaskCriticalEnter();
    {
        BOARDOBJGRP_ITERATOR_BEGIN(ADC_DEVICE, pAdcDev, adcDevIdx, &pAdcMask->super)
        {
            if (bAccessEnable)
            {
                pAdcDev->hwRegAccessClientsMask &= ~LWBIT_TYPE(clientId, LwU32);
            }
            else
            {
                pAdcDev->hwRegAccessClientsMask |= LWBIT_TYPE(clientId, LwU32);
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    appTaskCriticalExit();

    // Refresh the cached value immediately after enabling
    if (bAccessEnable)
    {
        status = clkAdcsVoltageReadWrapper(pAdcMask, NULL, LW_TRUE, clientId);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkAdcDeviceHwRegisterAccessSync_exit;
        }
    }

clkAdcDeviceHwRegisterAccessSync_exit:
    return status;
}
#endif // PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief ADC_DEV implementation to parse BOARDOBJGRP header.
 *
 * @copydetails BoardObjGrpIfaceModel10SetHeader()
 */
static FLCN_STATUS
s_clkAdcDevIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJGRP_SET_HEADER *pAdcSet =
        (RM_PMU_CLK_CLK_ADC_DEVICE_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    FLCN_STATUS status = FLCN_ERR_ILWALID_STATE;

    switch (pAdcSet->version)
    {
        case LW2080_CTRL_CLK_ADC_DEVICES_V10:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_1X))
            {
                status = clkIfaceModel10SetHeaderAdcDevices_1X(pModel10, pHdrDesc,
                                                   (LwU16)sizeof(CLK_ADC_V10), &Clk.clkAdc.pAdcs);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                }
            }
            break;
        }
        case LW2080_CTRL_CLK_ADC_DEVICES_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_2X))
            {
                status = clkIfaceModel10SetHeaderAdcDevices_2X(pModel10, pHdrDesc,
                                                   (LwU16)sizeof(CLK_ADC_V20), &Clk.clkAdc.pAdcs);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                }
            }
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }
    return status;
}

/*!
 * @brief Relays the 'construct' request to the appropriate ADC_DEVICE implementation.
 *
 * @copydetails BoardObjGrpIfaceModel10SetEntry()
 */
static FLCN_STATUS
s_clkAdcDevIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V10:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V10))
            {
                status = clkAdcDevGrpIfaceModel10ObjSetImpl_V10(pModel10, ppBoardObj,
                                   sizeof(CLK_ADC_DEVICE_V10), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V20))
            {
                status = clkAdcDevGrpIfaceModel10ObjSetImpl_V20(pModel10, ppBoardObj,
                                   sizeof(CLK_ADC_DEVICE_V20), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V30))
            {
                status = clkAdcDevGrpIfaceModel10ObjSetImpl_V30(pModel10, ppBoardObj,
                                   sizeof(CLK_ADC_DEVICE_V30), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V30_ISINK_V10))
            {
                status = clkAdcDevV30IsinkGrpIfaceModel10ObjSetImpl_V10(pModel10, ppBoardObj,
                                   sizeof(CLK_ADC_DEVICE_V30_ISINK_V10), pBuf);
            }
            break;
        }
        default:
        {
            // Do Nothing
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 */
static FLCN_STATUS
s_clkAdcDeviceIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V10:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V10))
            {
                status = clkAdcDeviceIfaceModel10GetStatusEntry_V10(pModel10, pPayload);
            }
            break;
        }
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V20))
            {
                status = clkAdcDeviceIfaceModel10GetStatusEntry_V20(pModel10, pPayload);
            }
            break;
        }
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V30))
            {
                status = clkAdcDeviceIfaceModel10GetStatusEntry_V30(pModel10, pPayload);
            }
            break;
        }
        case LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICE_V30_ISINK_V10))
            {
                status = clkAdcDeviceV30IsinkIfaceModel10GetStatusEntry_V10(pModel10, pPayload);
            }
            break;
        }
        default:
        {
            // Do Nothing
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}

