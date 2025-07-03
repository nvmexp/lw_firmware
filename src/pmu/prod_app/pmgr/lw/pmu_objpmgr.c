/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objpmgr.c
 * @brief  Container-object for PMU PCB Management (PMGR) routines. Contains
 *         generic non-HAL interrupt-routines plus logic required to hook-up
 *         chip-specific interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmu_i2capi.h"
#include "lib/lib_perf.h"
#include "therm/objtherm.h"
#include "pmu_obji2c.h"
#include "boardobj/boardobj.h"
#include "lwostimer.h"
#include "lib/lib_gpio.h"
#include "pmu_selwrity.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "pmu_objpsi.h"

#include "config/g_pmgr_private.h"
#include "g_pmurpc.h"
#if(PMGR_MOCK_FUNCTIONS_GENERATED)
#include "config/g_pmgr_mock.c"
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJPMGR         Pmgr;
#if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
OS_TIMER        PmgrOsTimer;
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Bug 1168927: Wait time (in us) after disabling BA HW to allow XBAR to go
 * idle.  This value was determined empirically in order to match same MSCG
 * residency as w/o BA units enabled.
 */
#define PMGR_BA_MSCG_WAR_BUG_1168927_WAIT_US                                   1

/* ------------------------- Private Function Definitions ------------------- */

static FLCN_STATUS s_pmgrPwrDevicesLoad(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pmgrPwrDevicesLoad")
    GCC_ATTRIB_NOINLINE();

static FLCN_STATUS s_pmgrLoadHelper(LwU32)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pmgrLoadHelper");

static FLCN_STATUS s_pmgrUnloadHelper(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "s_pmgrUnloadHelper");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the PMGR object.  This includes the HAL interface as well as all
 * software objects (ex. semaphores) used by the PMGR module.
 *
 * @return  FLCN_OK     On success
 * @return  Others      Error return value from failed child function
 */
FLCN_STATUS
constructPmgr(void)
{
    FLCN_STATUS status = FLCN_OK;

    memset(&Pmgr, 0x00, sizeof(OBJPMGR));

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY))
    {
        status = constructPmgrPwrPolicies();
        if (status != FLCN_OK)
        {
            goto constructPmgr_exit;
        }
    }

constructPmgr_exit:
    return status;
}

/*!
 * PMGR pre-init function.  Takes care of any SW initialization which can be
 * called from INIT overlay/task.
 */
void
pmgrPreInit(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X)
    osTimerInitTracked(OSTASK_TCB(PMGR), &PmgrOsTimer,
            PMGR_OS_TIMER_ENTRY_NUM_ENTRIES);
#else
    osTimerInitTracked(OSTASK_TCB(PMGR), &PmgrOsTimer, 0);
#endif
#endif
    // If PMGR requires .libFxpBasic and .libFxpExtended, permanently attach it.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_REQ_LIB_FXP))
    {
        // Called from INIT task, so need to specify PMGR task handle.
        OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(PMGR), OVL_INDEX_IMEM(libFxpBasic));

        if (OVL_INDEX_ILWALID != OVL_INDEX_IMEM(libFxpExtended))
        {
            OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(PMGR),
                                          OVL_INDEX_IMEM(libFxpExtended));
        }
    }

    // Permanently attach the perfVf to the PMGR task
    if (OVL_INDEX_ILWALID != OVL_INDEX_IMEM(perfVf))
    {
        OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(PMGR), OVL_INDEX_IMEM(perfVf));
    }

    // Permanently attach the libLeakage to PMGR task
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS11))
    {
        OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(PMGR), OVL_INDEX_IMEM(libLeakage));
    }

    // Cache priv security scratch register
    if (PMUCFG_FEATURE_ENABLED(PMU_PRIV_SEC))
    {
        Pmgr.i2cPortsPrivMask = i2cPortsPrivMaskGet_HAL(&I2c);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_LIB_GPIO))
    {
        gpioPreInit();
    }

    // Initialize any interrupts required by PMGR
    pmgrInterruptInit_HAL();
}

/*!
 * PMGR post-init function. Takes care of any SW initialization which
 * require running perf task such as allocating DMEM.
 */
FLCN_STATUS
pmgrPostInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Permanently attach the I2C_DEVICE DMEM overlay, if applicable.
    if (OVL_INDEX_DMEM(i2cDevice) != OVL_INDEX_ILWALID)
    {
        OSTASK_ATTACH_DMEM_OVERLAY(OVL_INDEX_DMEM(i2cDevice));
    }

    // Permanently attach the ILLUM DMEM overlay, if applicable.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_ILLUM))
    {
        OSTASK_ATTACH_DMEM_OVERLAY(OVL_INDEX_DMEM(illum));
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR))
    {
        status = pwrMonitorConstruct(&Pmgr.pPwrMonitor);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY))
    {
        status = pwrPoliciesConstruct(&Pmgr.pPwrPolicies);
    }

    return status;
}

/*!
 * PWR_EQUATION handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
pwrEquationBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,      // _grpType
        PWR_EQUATION,                               // _class
        pBuffer,                                    // _pBuffer
        pmgrPwrEquationIfaceModel10SetHeader,             // _hdrFunc
        pmgrPwrEquationIfaceModel10SetEntry,              // _entryFunc
        all.pmgr.pwrEquationGrpSet,                 // _ssElement
        OVL_INDEX_DMEM(pmgr),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
}

/*!
 * PWR_POLICY handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
pwrPolicyBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_SET_HEADER *pHdr =
        (RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_SET_HEADER *)pBuffer->pBuf;
    FLCN_STATUS     status;
    LwBool          bFirstConstruct;
    LwBool          bReload       = LW_FALSE;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libFxpBasic)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libFxpExtended)
        OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_CONSTRUCT
    };
    OSTASK_OVL_DESC ovlDescListReload[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, perfVf)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibLoad)
    };
    OSTASK_OVL_DESC ovlDescListCommon[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libLeakage)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pmgrLibConstruct)
    };

#if PMUCFG_FEATURE_ENABLED(PMU_FBQ) && PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)
    if (pBuffer->size <
        sizeof(RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_SET_HEADER_ALIGNED))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto pwrPolicyBoardObjGrpIfaceModel10Set_exit;
    }
#endif

    bFirstConstruct = !Pmgr.pwr.policies.super.bConstructed;

    //
    // Need a PWR_MONITOR object to be constructed before we can have a PWR_POLICIES
    // object.
    //
    if (Pmgr.pPwrMonitor == NULL)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListCommon);
    {
        {
            // Parse the PWR_POLICY header to figure out if PMGR unload/reload is requested.
            status = boardObjGrpIfaceModel10ReadHeader((void *)pHdr,
                sizeof(RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_SET_HEADER_ALIGNED),
                RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(all.pmgr.pwrPolicyPack.policies),
                LW_OFFSETOF(RM_PMU_PMGR_PWR_POLICY_PACK, policies));
            if (LW_OK != status)
            {
                PMU_BREAKPOINT();
                goto pwrPolicyBoardObjGrpIfaceModel10Set_exit;
            }
            else
            {
                bReload = pHdr->reload.bPmgrReload;
            }

            if (bReload)
            {
                // Unload PMGR
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListReload);
                {
                    status = s_pmgrUnloadHelper();
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListReload);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrPolicyBoardObjGrpIfaceModel10Set_exit;
                }
            }
        }

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            ATTACH_AND_LOAD_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
            {
                // Check if this function needs the class id or any other thing to construct.
                status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
                    PWR_POLICY,                                 // _class
                    pBuffer,                                    // _pBuffer
                    pmgrPwrPolicyIfaceModel10SetHeader,               // _hdrFunc
                    pmgrPwrPolicyIfaceModel10SetEntry,                // _entryFunc
                    all.pmgr.pwrPolicyPack.policies,            // _ssElement
                    OVL_INDEX_DMEM(pmgr),                       // _ovlIdxDmem
                    BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables

                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrPolicyBoardObjGrpIfaceModel10Set_exit;
                }

                //
                // If PWR_POLICY_DOMGRP class enabled, construct/iniitalize all state
                // associated with the global Domain Group ceiling feature.
                //
                if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP))
                {
                    pwrPolicyDomGrpGlobalCeilingConstruct();

                    // Request any aditional global domain group limits from RM
                    if (Pmgr.pPwrPolicies->globalCeiling.values[
                        RM_PMU_PERF_DOMAIN_GROUP_PSTATE] !=
                        RM_PMU_PERF_VPSTATE_IDX_ILWALID)
                        {
                            status = pwrPolicyDomGrpGlobalCeilingRequest(
                                LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT_CLIENT_IDX_RM,
                                &Pmgr.pPwrPolicies->globalCeiling);
                            if (status != FLCN_OK)
                            {
                                PMU_BREAKPOINT();
                                status = FLCN_ERR_ILWALID_STATE;
                                goto pwrPolicyBoardObjGrpIfaceModel10Set_exit;
                            }
                        }
                }

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_RELATIONSHIP)
                {
                    OSTASK_OVL_DESC ovlDescListPWM[] = {
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwmConstruct)
                    };

                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListPWM);
                    {
                        status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
                            PWR_POLICY_RELATIONSHIP,                    // _class
                            pBuffer,                                    // _pBuffer
                            boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
                            pmgrPwrPolicyRelIfaceModel10SetEntry,             // _entryFunc
                            all.pmgr.pwrPolicyPack.policyRels,          // _ssElementt
                            OVL_INDEX_DMEM(pmgr),                       // _ovlIdxDmem
                            BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
                    }
                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListPWM);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto pwrPolicyBoardObjGrpIfaceModel10Set_exit;
                    }
                }
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_VIOLATION)
                status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
                    PWR_VIOLATION,                              // _class
                    pBuffer,                                    // _pBuffer
                    boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
                    pmgrPwrViolationIfaceModel10SetEntry,             // _entryFunc
                    all.pmgr.pwrPolicyPack.violations,          // _ssElement
                    OVL_INDEX_DMEM(pmgr),                       // _ovlIdxDmem
                    BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrPolicyBoardObjGrpIfaceModel10Set_exit;
                }
#endif

                if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35) &&
                    bFirstConstruct)
                {
                    status = pwrPolicies35MultDataConstruct((PWR_POLICIES_35*)Pmgr.pPwrPolicies);
                    if (status != FLCN_OK)
                    {
                        goto pwrPolicyBoardObjGrpIfaceModel10Set_exit;
                    }
                }

                if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK))
                {
                    status = pwrPoliciesDomGrpLimitPolicyMaskSet();
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto pwrPolicyBoardObjGrpIfaceModel10Set_exit;
                    }
                }

pwrPolicyBoardObjGrpIfaceModel10Set_exit:
                lwosNOP();
            }
            DETACH_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if ((status == FLCN_OK) && (bReload))
        {
            // Reload PMGR only on success
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListReload);
            status = s_pmgrLoadHelper(pHdr->reload.patchMask);
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListReload);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
            }
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListCommon);

    return status;
}

/*!
 * Calls PMGR's load helper to load all relevant PMGR HW state.
 * @ref s_pmgrLoadHelper
 */
FLCN_STATUS
pmuRpcPmgrLoad
(
    RM_PMU_RPC_STRUCT_PMGR_LOAD *pParams
)
{
    return s_pmgrLoadHelper(pParams->patchMask);
}

/*!
 * Calls PMGR's unload helper to unload all PMGR state.
 * @ref s_pmgrUnloadHelper
 */
FLCN_STATUS
pmuRpcPmgrUnload
(
    RM_PMU_RPC_STRUCT_PMGR_UNLOAD *pParams
)
{
    return s_pmgrUnloadHelper();
}

/*!
 * PMGR wrapper/helper function to retrieve the current frequency of a requested
 * clock domain.
 *
 * Handles calling the appropriate interface based on PMGR features.  If @ref
 * PMU_PMGR_AVG_EFF_FREQ is enabled will use @ref clkCntrAvgFreqKHz() via the
 * .clkLibCntr overlay.  Otherwise, will fallback tos set values reported by
 * OBJPERF.
 *
 * @copydoc clkCntrAvgFreqKHz()
 */
LwU32
pmgrFreqMHzGet
(
    LwU32                    clkDom,
    CLK_CNTR_AVG_FREQ_START *pCntrStart
)
{
    LwU32 freqMHz = 0;
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_3X))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_CLK_COUNTER
        };

        // pCntrStart cannot be NULL for PStates 3.x
        if (pCntrStart == NULL)
        {
            PMU_BREAKPOINT();
            goto pmgrFreqMHzGet_exit;
        }

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Compute average frequency.
            freqMHz = clkCntrAvgFreqKHz(clkDom,
                LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED, pCntrStart);
            freqMHz = LW_UNSIGNED_ROUNDED_DIV(freqMHz, 1000);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }
    else
    {
        // Make sure that RM has sent at least one pstate change to the PMU.
        if (perfVoltageuVGet() != 0)
        {
            // Legacy clock interfaces only supports GPC2CLK.
            PMU_HALT_COND(clkDom == LW2080_CTRL_CLK_DOMAIN_GPC2CLK);

            // Colwert KHz -> MHz
            freqMHz = LW_UNSIGNED_ROUNDED_DIV(
                        perfDomGrpGetValue(RM_PMU_DOMAIN_GROUP_GPC2CLK), 1000);
        }
    }

pmgrFreqMHzGet_exit:
    return freqMHz;
}

/*!
 * Implement this interface to update PMGR objects when a new Perf update
 * is sent to PMU.
 *
 * @param[in]  pStatus         new RM_PMU_PERF_STATUS to be updated
 * @param[in]  coreVoltageuV   new core voltage settings [uV]
 */
void
pmgrProcessPerfStatus
(
    RM_PMU_PERF_STATUS *pStatus,
    LwU32 coreVoltageuV
)
{
    // Handle power policy changes when a perfStatus is updated.
    pwrPoliciesProcessPerfStatus(pStatus);

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X)
    // Handle power device BAxx changes when a perfStatus is updated.
    if (PMUCFG_FEATURE_ENABLED_PWR_DEVICE_BA12X_AND_LATER)
    {
        pwrDevProcessPerfStatus(pStatus);
    }
#endif

    // Notify PMGR of p-state change
    pmgrProcessPerfChange(coreVoltageuV);
}

/*!
 * @brief   Used to notify PMGR of p-state change
 *
 * For now handling only core voltage change.
 *
 * @param[in]   coreVoltageuV   New core voltage settings [uV]
 */
void
pmgrProcessPerfChange
(
    LwU32   coreVoltageuV
)
{
    LwU8    dIdx;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrPwrDeviceStateSync)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        FOR_EACH_INDEX_IN_MASK(32, dIdx, Pmgr.pwr.devices.objMask.super.pData[0])
        {
            BOARDOBJ *pBoardObj;

            pBoardObj = (BOARDOBJ *)PWR_DEVICE_GET(dIdx);
            if (pBoardObj == NULL)
            {
                PMU_BREAKPOINT();
                return;
            }

            (void)pwrDevVoltageSetChanged(pBoardObj, coreVoltageuV);
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/*!
 * Prepares all PMGR functionality to enter into/exit from SWASR, helping
 * coordinate any PMGR/PWR features with MSCG and GC4/5.
 *
 * @param[in] bEnter
 *     Boolean indicating whether entering into or exiting from MSCG/GC4/GC5.
 */
void
pmgrSwAsrNotify_IMPL
(
    LwBool bEnter
)
{
    //
    // MSCG/BA WAR for bug 1168927: BA units in monitored paritions send samples
    // via XBAR to BA aclwmulators/fifos in THERM/PWR.  These transactions
    // result in XBAR activity which prevents MSCG/GC4/GC5 from engaging (i.e. aborting)
    // when checking XBAR idle (so that XBARCLK can be gated).
    //
    // This WAR disables BA before entering MSCG/GC4/GC5 and gives some delay to
    // allow XBAR to idle, and not abort MSCG/GC4/GC5 entry.  When MSCG exits, BA
    // is re-enabled.
    //
    if (Pmgr.baInfo.bInitializedAndUsed)
    {
        pwrDevBaEnable(!bEnter);

        if (bEnter)
        {
            OS_PTIMER_SPIN_WAIT_US(PMGR_BA_MSCG_WAR_BUG_1168927_WAIT_US);
        }
    }
}

/*!
 * @brief   Return the I2C flags of the first port matched in the I2C device table.
 *
 * If the port doesn't match with any of the I2C device, halt the PMU.
 *
 * @param[in]   i2cPort     I2C port.
 * @param[in]   i2cAddress  I2c device address
 *
 * @return      i2cFlags    I2C flags of the matched port.
 */
LwU32
pmgrI2cGetPortFlags
(
    LwU8    i2cPort,
    LwU16   i2cAddress
)
{
    I2C_DEVICE   *pDevice;
    LwBoardObjIdx i;
    LwBool        bFound = LW_FALSE;

    // Loop over all I2C devices
    BOARDOBJGRP_ITERATOR_BEGIN(I2C_DEVICE, pDevice, i, NULL)
    {
        if ((pDevice->i2cPort == i2cPort) &&
            (pDevice->i2cAddress == i2cAddress))
        {
            bFound = LW_TRUE;
            break;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    if (!bFound)
    {
        //
        // We should not reach here. If we are reaching here, it means port is not
        // found, which is fatal and we should halt.
        //
        PMU_BREAKPOINT();
    }

    pDevice = I2C_DEVICE_GET(i);
    if (pDevice == NULL)
    {
        PMU_BREAKPOINT();
        return 0;
    }

    return pDevice->i2cFlags;
}

/*!
 * Load all relevant BA HW-state. Lwrrently it is using for loading BA DMA
 * related states only.
 *
 * @return FLCN_OK
 */
FLCN_STATUS
pmgrBaLoad(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA15))
    {
        pwrDevBaDmaMode_BA15HW();
    }
    return FLCN_OK;
}

/*!
 * @brief Compute FACTOR_A for a give voltage
 *
 * @param[in]  pDev       The power device
 * @param[in]  voltageuV  The voltage to compute FACTOR_A at, in microvolts (uV)
 *
 * @returns FACTOR_A value
 */
LwU32
pmgrComputeFactorA
(
    PWR_DEVICE *pDev,
    LwU32 voltageuV
)
{
    PWR_DEVICE_BA15HW *pBa15Hw = (PWR_DEVICE_BA15HW *)pDev;
    LwUFXP20_12 scaleA;

    // compute what the new scaling factor should be, called scaleA
    scaleA = pwrEquationBA15ScaleComputeScaleA(
                (PWR_EQUATION_BA15_SCALE *)pBa15Hw->super.pScalingEqu,
                pBa15Hw->super.bLwrrent,
                voltageuV);

    //
    // we want BA sum to be multiplied by scaleA. However, HW breaks the multiplication
    // by scaleA into a multiply by factorA, and a bit-shift by shiftA. shiftA is computed
    // at boot, verified to be a right shift, and held constant afterwards. we left shift here
    // to counteract the right shift that HW will apply
    //
    scaleA <<= pBa15Hw->super.shiftA;
    return LW_TYPES_UFXP_X_Y_TO_U32(20, 12, scaleA);
}

/* ------------------------- Private Function Definitions ------------------- */
/*!
 * Iterate over all power-device ilwoking pwrDevLoad on each instance. Like
 * @ref pmgrConstructPwrDevices, devices are removed from the power-device mask
 * (@ref OBJPMGR::pwr.devices.objMask.super.pData[0]) if the load process fails for the device.
 *
 * @return other
 *     Relays error-status returned @ref pwrDevLoad. If multiple devices fail
 *     to construct, this status reflects the error returned by the first
 *     device that failed.
 */
static FLCN_STATUS
s_pmgrPwrDevicesLoad(void)
{
    LwU8         d;
    FLCN_STATUS  status = FLCN_OK;
    FLCN_STATUS  devStatus;

    FOR_EACH_INDEX_IN_MASK(32, d, Pmgr.pwr.devices.objMask.super.pData[0])
    {
        PWR_DEVICE *pDevice;

        pDevice = PWR_DEVICE_GET(d);
        if (pDevice == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto s_pmgrPwrDevicesLoad_exit;
        }

        devStatus = pwrDevLoad(pDevice);
        if (devStatus != FLCN_OK)
        {
            Pmgr.pwr.devices.objMask.super.pData[0] &= ~BIT(d);
        }

        // keep track of the first error status only
        if (status == FLCN_OK)
        {
            status = devStatus;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X))
    {
        LwU32 ticksNow = lwrtosTaskGetTickCount32();

        // Schedule the callback function to update per device BAxx internal state
        if ((PMUCFG_FEATURE_ENABLED_PWR_DEVICE_BA12X_AND_LATER) ||
            (PMGR_PWR_DEVICE_CALLBACK_GPUADC10))
        {
            pwrDevScheduleCallback(ticksNow, LW_FALSE);
        }
    }

s_pmgrPwrDevicesLoad_exit:
    return status;
}

/*!
 * Load all relevant PMGR HW-state. Actions may include, but are not limited
 * to, operations such as programming power-device context, power-monitor and
 * power-capping specific HW initialization, etc ...
 */
static FLCN_STATUS
s_pmgrLoadHelper
(
    LwU32    patchMask
)
{
    FLCN_STATUS        status           = FLCN_OK;
    PMGR_PSI_CALLBACK *pPsiCallback;
    OSTASK_OVL_DESC ovlDescListDevice[] = {
        OSTASK_OVL_DESC_DEFINE_PWR_DEVICES_LOAD
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListDevice);
    {
        if (FLD_TEST_DRF(_RM_PMU_PMGR, _PATCH_MASK, _BUG_1969099, _PATCH, patchMask))
        {
            status = pmgrPwrDeviceBaPatchBug1969099_HAL();
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA16) &&
            FLD_TEST_DRF(_RM_PMU_PMGR, _PATCH_MASK, _BUG_200734888, _PATCH, patchMask))
        {
            status = pwrDevPatchBaWeightsBug200734888_BA16HW();
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE))
        {
            status = s_pmgrPwrDevicesLoad();
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListDevice);

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR) &&
        (status == FLCN_OK))
    {
        status = pwrChannelsLoad();
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY) &&
        (status == FLCN_OK) &&
        (Pmgr.pPwrPolicies != NULL))
    {
        status = pwrPoliciesLoad(Pmgr.pPwrPolicies);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PERF_VF_ILWALIDATION_NOTIFY)    &&
        PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY)                     &&
        (status == FLCN_OK)                                             &&
        (Pmgr.pPwrPolicies != NULL))
    {
        OSTASK_OVL_DESC ovlDescListPwrPoliciesVfIlwalidate[] = {
            OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_VF_ILWALIDATE
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListPwrPoliciesVfIlwalidate);
        {
            status = pwrPoliciesVfIlwalidate(PWR_POLICIES_GET());
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListPwrPoliciesVfIlwalidate);
    }

    if (status == FLCN_OK)
    {
        status = pmgrBaLoad();
    }

    if (status == FLCN_OK)
    {
        Pmgr.bLoaded = LW_TRUE;
    }

    // Get the PMGR_PSI_CALLBACK pointer
    PMU_ASSERT_OK_OR_GOTO(status,
        pmgrPsiCallbackGet(&pPsiCallback),
        s_pmgrLoadHelper_exit);

    if ((pPsiCallback != NULL)   &&
        pPsiCallback->bRequested &&
        pPsiCallback->bDeferred)
    {
        OSTASK_OVL_DESC ovlDescListPsiCallback[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPsiCallback)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListPsiCallback);
        {
            //
            // The first callback will be serviced after a finite X ms. We
            // need sleep current to have a valid value as soon as possible.
            // Hence explicitly calling this function here
            //
            psiSleepLwrrentCalcMultiRail();
            psiCallbackTriggerMultiRail(LW_TRUE);
            pPsiCallback->bDeferred = LW_FALSE;
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListPsiCallback);
    }

s_pmgrLoadHelper_exit:
    return status;
}

/*!
 * Unload HW/SW-state setup by @ref pmgrLoad necessary to steady-state/idle the
 * PMGR task and leave the HW in the proper state for post- driver-unload
 * operation.
 */
static FLCN_STATUS
s_pmgrUnloadHelper(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X) &&
        (Pmgr.pPwrPolicies != NULL) &&
        (Pmgr.pPwrPolicies->version >= RM_PMU_PMGR_PWR_POLICY_DESC_TABLE_VERSION_3X))
    {
        pwrPolicies3XCancelEvaluationCallback();
    }

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X)
        if (PMUCFG_FEATURE_ENABLED_PWR_DEVICE_BA12X_AND_LATER)
        {
            pwrDevCancelCallback();
        }
#endif

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR))
    {
        pwrChannelsUnload();
    }

    Pmgr.bLoaded = LW_FALSE;

    return FLCN_OK;
}
