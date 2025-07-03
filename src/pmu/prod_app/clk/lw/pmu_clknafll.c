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
 * @file   pmu_clknafll.c
 * @brief  File for CLK NAFLL and LUT routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "perf/3x/vfe.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "volt/objvolt.h"
#include "dmemovl.h"

#include "clk3/dom/clkfreqdomain_singlenafll.h"
#include "clk3/dom/clkfreqdomain_multinafll.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clkNafllIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "s_clkNafllIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clkNafllIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "s_clkNafllIfaceModel10SetEntry");
static FLCN_STATUS  s_clkNafllFreqsIterate_FINAL(CLK_NAFLL_DEVICE *pNafllDev, LwU16 *pFreqMHz)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkNafllFreqsIterate_FINAL");

/* ------------------------- Global Variables ------------------------------- */
/*!
 * @brief   Array of all vtables pertaining to different CLK_ADC_DEVICE object types.
 */
BOARDOBJ_VIRTUAL_TABLE *ClkNafllDeviceVirtualTables[LW2080_CTRL_BOARDOBJ_TYPE(CLK, NAFLL_DEVICE, MAX)] =
{
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, NAFLL_DEVICE, BASE)]  = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, NAFLL_DEVICE, V10) ]  = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, NAFLL_DEVICE, V20) ]  = CLK_NAFLL_DEVICE_V20_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, NAFLL_DEVICE, V30) ]  = NULL,
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief NAFLL_DEVICE handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydetails BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
clkNafllDevBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfsInit)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        clkNafllAllMaskGet_HAL(&Clk, &Clk.clkNafll.nafllSupportedMask);

        status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,    // _grpType
            NAFLL_DEVICE,                               // _class
            pBuffer,                                    // _pBuffer
            s_clkNafllIfaceModel10SetHeader,                  // _hdrFunc
            s_clkNafllIfaceModel10SetEntry,                   // _entryFunc
            pascalAndLater.clk.clkNafllDeviceGrpSet,    // _ssElement
            OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
            ClkNafllDeviceVirtualTables);               // _ppObjectVtables
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Super-class implementation.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
clkNafllGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    CLK_NAFLL_DEVICE   *pNafllDev;
    FLCN_STATUS         status;
    RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJ_SET *pTmpDesc =
        (RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJ_SET *)pBoardObjDesc;

    //
    // Return with error if the ID isn't include in the nafllSupportedMask.
    // We don't want to construct objects if the ID is not supported.
    //
    if ((LWBIT_TYPE(pTmpDesc->id, LwU32) & Clk.clkNafll.nafllSupportedMask) == 0U)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERROR;
        goto clkNafllGrpIfaceModel10ObjSet_SUPER_exit;
    }

    // Return with error if railIdxForLut is out of range
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (pTmpDesc->railIdxForLut >= LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto clkNafllGrpIfaceModel10ObjSet_SUPER_exit;
        }
    }

    //
    // Return with error if any of the dividers is '0' to avoid a div_by_zero
    // error later down the stack.
    //
    if ((pTmpDesc->mdiv == 0) || (pTmpDesc->inputRefClkDivVal == 0))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkNafllGrpIfaceModel10ObjSet_SUPER_exit;
    }

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto clkNafllGrpIfaceModel10ObjSet_SUPER_exit;
    }
    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pNafllDev, *ppBoardObj, CLK, NAFLL_DEVICE, BASE,
                                  &status, clkNafllGrpIfaceModel10ObjSet_SUPER_exit);

    pNafllDev->id                       = pTmpDesc->id;
    pNafllDev->mdiv                     = pTmpDesc->mdiv;
    pNafllDev->railIdxForLut            = pTmpDesc->railIdxForLut;
    pNafllDev->inputRefClkFreqMHz       = pTmpDesc->inputRefClkFreqMHz;
    pNafllDev->inputRefClkDivVal        = pTmpDesc->inputRefClkDivVal;
    pNafllDev->clkDomain                = pTmpDesc->clkDomain;
    pNafllDev->freqCtrlIdx              = pTmpDesc->freqCtrlIdx;
    pNafllDev->bSkipPldivBelowDvcoMin   = pTmpDesc->bSkipPldivBelowDvcoMin;
    pNafllDev->dvco.bDvco1x             = pTmpDesc->bDvco1x;
    VFE_EQU_IDX_COPY_RM_TO_PMU(pNafllDev->dvco.minFreqIdx, pTmpDesc->dvcoMinFreqVFEIdx);

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_BROADCAST)
    // Import NAFLL LUT PRIMARY MASK LW2080->PMU
    boardObjGrpMaskInit_E32(&(pNafllDev->lutProgBroadcastSecondaryMask));
    status = boardObjGrpMaskImport_E32(&(pNafllDev->lutProgBroadcastSecondaryMask),
                                       &(pTmpDesc->lutProgBroadcastSecondaryMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllGrpIfaceModel10ObjSet_SUPER_exit;
    }
#endif

clkNafllGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @brief Find the NAFLL device corresponding to the nafll id
 *
 * @param[in]   nafllId  ID of the NAFLL device to find
 *
 * @return Pointer to the NAFLL device, if successfully found
 * @return 'NULL' if the NAFLL device does not exist or is not supported
 */
CLK_NAFLL_DEVICE *
clkNafllDeviceGet
(
    LwU8  nafllId
)
{
    CLK_NAFLL        *pClkNafll = &Clk.clkNafll;
    CLK_NAFLL_DEVICE *pNafllDev = NULL;
    LwBoardObjIdx     devIdx;

    if ((LWBIT_TYPE(nafllId, LwU32) & pClkNafll->nafllSupportedMask) == 0U)
    {
        return NULL;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(NAFLL_DEVICE, pNafllDev, devIdx, NULL)
    {
        if (pNafllDev->id == nafllId)
        {
            return pNafllDev;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return  NULL;
}

/*!
 * @brief Get the Primary NAFLL device corresponding to the target clock domain.
 *
 * @details There is no concept of primary-secondary NAFLLs in the definition of
 *       NAFLL device table. This is purely for the purpose of reading SW
 *       state like target frequency, regime etc which is same for all the
 *       NAFLLs corresponding to a clock domain.
 *
 * @param[in]   clkDomain  clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>.
 *
 * @return Pointer to the NAFLL device, if successfully found
 * @return 'NULL' if the NAFLL device not found
 */
CLK_NAFLL_DEVICE *
clkNafllPrimaryDeviceGet
(
    LwU32   clkDomain
)
{
    CLK_NAFLL_DEVICE *pNafllDev = NULL;
    LwBoardObjIdx     devIdx;

    // Iterate over the primary nafll devices
    NAFLL_LUT_PRIMARY_ITERATOR_BEGIN(pNafllDev, devIdx)
    {
        if (pNafllDev->clkDomain == clkDomain)
        {
            break;
        }
    }
    NAFLL_LUT_PRIMARY_ITERATOR_END;

    return  pNafllDev;
}

/*!
 * @brief Colwert clock domain to NAFLL ID
 *
 * @param[in]   clkDomain       Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
 * @param[out]  pNafllIdMask    Mask of all NAFLL IDs that belong to this clock
 *                              domain
 *
 * @return FLCN_OK     Successfully mapped the given domain to device-id mask.
 * @return FLCN_ERROR  If the clock domain to Nafll device id mapping does not exist.
 */
FLCN_STATUS
clkNafllDomainToIdMask
(
    LwU32   clkDomain,
    LwU32  *pNafllIdMask
)
{
    CLK_NAFLL_DEVICE *pNafllDev = NULL;
    LwBoardObjIdx     devIdx;

    *pNafllIdMask = LW2080_CTRL_CLK_NAFLL_MASK_UNDEFINED;

    BOARDOBJGRP_ITERATOR_BEGIN(NAFLL_DEVICE, pNafllDev, devIdx, NULL)
    {
        if (pNafllDev->clkDomain == clkDomain)
        {
            *pNafllIdMask |= LWBIT_TYPE(pNafllDev->id, LwU32);
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    if (*pNafllIdMask == LW2080_CTRL_CLK_NAFLL_MASK_UNDEFINED)
    {
        return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @brief Given a frequency, compute the NDIV
 *
 * @param[in]   pNafllDev      Pointer to the NAFLL device
 * @param[in]   freqMHz        Clock frequency in MHz. This can either be a 1x
 *                             or 2x clock value
 * @param[out]  pNdiv          Computed N-Divider value
 * @param[in]   bFloor         Boolean indicating whether the frequency should
 *                             be quantized via a floor (LW_TRUE) or ceiling
 *                             (LW_FALSE) to the next closest supported value.
 *
 * @return FLCN_OK                   Successfully computed ndiv
 * @return FLCN_ERR_ILWALID_ARGUMENT At least one of the input arguments passed
 *                                   is invalid
 */
FLCN_STATUS
clkNafllNdivFromFreqCompute
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU16               freqMHz,
    LwU16              *pNdiv,
    LwBool              bFloor
)
{
    // Sanity Check
    if ((pNafllDev                     == NULL)       ||
        (pNdiv                         == NULL)       ||
        (pNafllDev->mdiv               == 0U)         ||
        (pNafllDev->inputRefClkFreqMHz == 0U)         ||
        (pNafllDev->inputRefClkDivVal  == 0U))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Adjust the frequency value based on 1x/2x DVCO.
    // Case 1: When 1x domains are supported(i.e. VF lwrve having 1x freqeuncy)
    //         and 2x DVCO is in use, double the frequency.
    // Case 2: When 1x domains are not supported(i.e. VF lwrve having 2x freqeuncy)
    //         and 1x DVCO is in use, half the frequency.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_SUPPORTED) && !pNafllDev->dvco.bDvco1x)
    {
        //
        // If bFloor, add 1MHz to take care of 1X <-> 2X colwersion error.
        // Required for odd 2x POR frequencies which are rounded down in
        // the VF lwrve while colwerting to 1x.
        //
        // Example: Ndiv = 111, Mdiv = 16, Input Freq = 405MHz
        // CS computes 1x frequency step size (= 405/32 = 12.65625) then
        // compute 1x frequency as 12.65625 * 111 = 1404.84375 and just put
        // the integer part(i.e. 1404MHz) of it in the POR.
        // Without +1MHz here, we will compute 2x = 2808MHz and Ndiv computation
        // logic below will compute Ndiv = 110 and that is one step lower than
        // expected.
        //
        freqMHz = (LwU16)((freqMHz * 2U) + (bFloor ? 1U : 0U));
    }
    else if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_SUPPORTED) && pNafllDev->dvco.bDvco1x)
    {
        freqMHz /= 2U;
    }
    else
    {
        // Do nothing
    }

    //
    // NOTE:
    // 1. The / operation always round down the NDIV to the nearest integer.
    //
    // 2. We don't use PLDIV in the computation of NDIV. The output frequency is
    //    limited by the DVCO Min frequency and we engage PLDIV only when DVCO
    //    Min is reached. At all other points above DVCO Min, PLDIV is set to 1.
    //    As a consequence, we don't take PLDIV into account when computing the
    //    NDIV - i.e. PLDIV is an separate knob in deciding the frequencies only
    //    below DVCO Min
    //
    // 3. Because the frequency step size (inputRefClkFreqMHz /
    //    (inputRefClkDivVal * mdiv)) may not be a whole number and we are using
    //    integer math, we need to add a correction factor of 1 MHz in certain
    //    cases to account for the rounding error.
    //
    //    Here is an example for the math -
    //    Say a POR V/F point is 2784MHz@850mV, the input frequency is 405 MHz
    //    and the REF_CLK divider is 1.
    //    If we colwert it to NDIV, it would  be (2784 * 16 * 1) / 405 = 109.985,
    //    which gets rounded down to 109 before being programmed in the LUT.
    //    For that NDIV, the frequency that comes out of the NAFLL is
    //    (109 * 405) / (16 * 1) = 2759MHz which is one step lower than what we
    //    want. Adding a 1MHz here would give us 2785MHz which gives an NDIV of
    //    110, which would result in NAFLL generating a frequency of 2784MHz,
    //    exactly what we want.
    //
    // 4. In certain cases, adding a correction factor could result in ceiling
    //    instead of floor operation. For example, if the input frequency is
    //    2758 MHz when floored it should be set to NDIV = 108 but due to the
    //    correction factor of 1 MHz, we end up with
    //    ((2758 + 1)* (16 * 1) / 405) = 109 assuming inputFreq = 405 & REF_CLK
    //    div = 1. 
    //    To fix this, we are using the modulo operation to determine if it is
    //    required to add correction factor.
    //
    //
    if (bFloor)
    {
        if ((((freqMHz + 1U) * pNafllDev->mdiv * pNafllDev->inputRefClkDivVal) % pNafllDev->inputRefClkFreqMHz) != 0U)
        {
            *pNdiv = (LwU16)(((freqMHz + 1U) * pNafllDev->mdiv * pNafllDev->inputRefClkDivVal) /
                            pNafllDev->inputRefClkFreqMHz);
        }
        else
        {
            *pNdiv = (freqMHz * pNafllDev->mdiv * pNafllDev->inputRefClkDivVal) /
                            pNafllDev->inputRefClkFreqMHz;
        }
    }
    //
    // To apply the ceiling quantization (!bFloor) add in inputRefClkFreqMHz - 1
    // before dividing by inputRefClkFreqMHz. Otherwise, the unadjusted division
    // (bFloor) will result in a floor quantization.
    //
    else
    {
        *pNdiv =
            (LwU16)(((freqMHz * pNafllDev->mdiv * pNafllDev->inputRefClkDivVal) +
                    (pNafllDev->inputRefClkFreqMHz - 1U)) / pNafllDev->inputRefClkFreqMHz);
    }

    *pNdiv = (*pNdiv == 0U) ? (LwU16)1U : *pNdiv;

    return FLCN_OK;
}

/*!
 * @brief Given a NDIV, compute the frequency
 *
 * @param[in]   pNafllDev   Pointer to the NAFLL device
 * @param[out]  ndiv        N-Divider value
 *
 * @return  Frequency value, in MHz, for the given ndiv
 */
LwU16
clkNafllFreqFromNdivCompute
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU16               ndiv
)
{
    LwU16   freqMHz = 0U;

    freqMHz = ((ndiv * pNafllDev->inputRefClkFreqMHz) /
               (pNafllDev->mdiv * pNafllDev->inputRefClkDivVal));

    //
    // Adjust the frequency value based on 1x/2x DVCO.
    // Case 1: When 1x domains are supported(i.e. VF lwrve having 1x freqeuncy)
    //         and 2x DVCO is in use, half the frequency.
    // Case 2: When 1x domains are not supported(i.e. VF lwrve having 2x freqeuncy)
    //         and 1x DVCO is in use, double the frequency.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_SUPPORTED) && !pNafllDev->dvco.bDvco1x)
    {
        freqMHz /= 2U;
    }
    else if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_SUPPORTED) && pNafllDev->dvco.bDvco1x)
    {
        freqMHz *= 2U;
    }
    else
    {
        // Do nothing
    }

    return freqMHz;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 */
FLCN_STATUS
clkNafllDeviceIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS                     status  = FLCN_OK;
    CLK_NAFLL_DEVICE               *pNafllDev;
    RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJ_GET_STATUS *pNafllGetStatus;

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
    // Attach overlays
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList); // NJ??
#endif

    if (pPayload == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkNafllDeviceIfaceModel10GetStatus_exit;
    }

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkNafllDeviceIfaceModel10GetStatus_exit;
    }
    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pNafllDev, pBoardObj, CLK, NAFLL_DEVICE, BASE,
                                  &status, clkNafllDeviceIfaceModel10GetStatus_exit);

    pNafllGetStatus = (RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJ_GET_STATUS *)pPayload;

    pNafllGetStatus->lwrrentRegimeId = pNafllDev->pStatus->regime.lwrrentRegimeId;
    pNafllGetStatus->dvcoMinFreqMHz  = pNafllDev->pStatus->dvco.minFreqMHz;
    pNafllGetStatus->bDvcoMinReached = pNafllDev->pStatus->dvco.bMinReached;
    pNafllGetStatus->swOverrideMode  = pNafllDev->lutDevice.swOverrideMode;
    pNafllGetStatus->swOverride      = pNafllDev->lutDevice.swOverride;

    // Retrieve version based LUT VF Lwrve data
    status = clkNafllLutVfLwrveGet(pNafllDev, &pNafllGetStatus->lutVfLwrve);
    if (status != FLCN_OK)
    {
        goto clkNafllDeviceIfaceModel10GetStatus_exit;
    }

clkNafllDeviceIfaceModel10GetStatus_exit:

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
#endif

    return status;
}

/*!
 * @brief Queries all NAFLL devices
 *
 * @copydetails BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
clkNafllDevicesBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32,       // _grpType
            NAFLL_DEVICE,                                   // _class
            pBuffer,                                        // _pBuffer
            NULL,                                           // _hdrFunc
            clkNafllDeviceIfaceModel10GetStatus,                        // _entryFunc
            pascalAndLater.clk.clkNafllDeviceGrpGetStatus); // _ssElement
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Given a index (ndiv point), find the corresponding frequency.
 *
 * @param[in]   clkDomain  Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
 * @param[in]   idx        Index (ndiv point) .
 * @param[out]  pfreqMHz   Frequency value mapped at the input index.
 *
 * @return FLCN_OK                   Successfully found the frequency mapped at
 *                                   input index (ndiv).
 * @return FLCN_ERR_ILWALID_ARGUMENT At least one of the input arguments passed
 *                                   is invalid
 */
FLCN_STATUS
clkNafllGetFreqMHzByIndex_IMPL
(
    LwU32   clkDomain,
    LwU16   idx,
    LwU16  *pFreqMHz
)
{
    CLK_NAFLL_DEVICE *pNafllDev = NULL;

    // Sanity check the input arguments
    if (pFreqMHz == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Find the matching NAFLL device.
    pNafllDev = clkNafllPrimaryDeviceGet(clkDomain);
    if (pNafllDev == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Colwert index to frequency
    *pFreqMHz = clkNafllFreqFromNdivCompute(pNafllDev, idx);

    return FLCN_OK;
}

/*!
 * @brief Public interface to get the DVCO min frequency for input clock domain.
 *
 * @param[in]   clkDomain  Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
 * @param[out]  pfreqMHz   DVCO min freq value based on current set voltage.
 * @param[out]  pBSkipPldivBelowDvcoMin
 *                         Flag to indicate whether to skip setting Pldiv
 *                         below DVCO min or not.
 *
 * @pre IMEM OVERLAY : imem_perf
 * @pre DMEM OVERLAY : OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf);
 *
 * @return FLCN_OK                   Successfully found the DVCO min value
 * @return FLCN_ERR_ILWALID_INDEX    If a matching nafll device is not found
 *                                   for the given clock domain
 * @return FLCN_ERR_ILWALID_ARGUMENT At least one of the input arguments passed
 *                                   is invalid
 */
FLCN_STATUS
clkNafllDvcoMinFreqMHzGet
(
    LwU32   clkDomain,
    LwU16  *pFreqMHz,
    LwBool *pBSkipPldivBelowDvcoMin
)
{
    CLK_NAFLL_DEVICE *pNafllDev = NULL;

    // Sanity check the input arguments
    if ((pFreqMHz == NULL) ||
        (pBSkipPldivBelowDvcoMin == NULL))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Find the matching NAFLL device.
    pNafllDev = clkNafllPrimaryDeviceGet(clkDomain);
    if (pNafllDev == NULL)
    {
        return FLCN_ERR_ILWALID_INDEX;
    }

    // Copy-out the param values.
    *pFreqMHz                   = Clk.clkNafll.dvcoMinFreqMHz[pNafllDev->railIdxForLut];
    *pBSkipPldivBelowDvcoMin    = pNafllDev->bSkipPldivBelowDvcoMin;

    return FLCN_OK;
}

/*!
 * @brief Given a target frequency, get the ndiv point.
 *
 * @param[in]   clkDomain  Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
 * @param[in]   freqMHz    Input frequency in MHz
 * @param[in]   bFloor     Boolean indicating whether the index should be found
 *                         via a floor (LW_TRUE) or ceiling (LW_FALSE) to the
 *                         next closest supported value.
 * @param[out]  pIdx       Index (ndiv) corresponding to the input frequency.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT At least one of the input arguments passed
 *                                   is invalid
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
FLCN_STATUS
clkNafllGetIndexByFreqMHz_IMPL
(
    LwU32   clkDomain,
    LwU16   freqMHz,
    LwBool  bFloor,
    LwU16  *pIdx
)
{
    CLK_NAFLL_DEVICE *pNafllDev = NULL;

    // Sanity check the input arguments
    if (pIdx == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Find the matching NAFLL device.
    pNafllDev = clkNafllPrimaryDeviceGet(clkDomain);
    if (pNafllDev == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Compute the index value for the given input frequency.
    return clkNafllNdivFromFreqCompute(pNafllDev, freqMHz, pIdx, bFloor);
}

/*!
 * @brief Given a target frequency, quantize it to the NAFLL frequency step
 *
 * @param[in]     pNafllDev  Pointer to the NAFLL device
 * @param[in,out] pfreqMHz   Non-quantized 2x frequency in MHz will be input and
 *                           quantized 2x frequency in MHz value will be output
 * @param[in]     bFloor     Boolean indicating whether the frequency should be
 *                           quantized via a floor (LW_TRUE) or ceiling (LW_FALSE)
 *                           to the next closest supported value.
 *
 * @return FLCN_ERROR                if the NAFLL device is not found
 * @return FLCN_ERR_ILWALID_ARGUMENT At least one of the input arguments passed
 *                                   is invalid
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
FLCN_STATUS
clkNafllFreqMHzQuantize_IMPL
(
    LwU32   clkDomain,
    LwU16  *pFreqMHz,
    LwBool  bFloor
)
{
    CLK_NAFLL_DEVICE *pNafllDev = NULL;
    LwBoardObjIdx     devIdx;
    LwU16             ndiv;
    FLCN_STATUS       status;

    // Sanity check the input arguments
    if (pFreqMHz == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(NAFLL_DEVICE, pNafllDev, devIdx, NULL)
    {
        if (pNafllDev->clkDomain == clkDomain)
        {
            //
            // First compute the NDIV value for the given target frequency.
            // Colwerting the frequency to NDIV will result in quantization to
            // the NAFLL frequency step.
            //
            status = clkNafllNdivFromFreqCompute(pNafllDev, *pFreqMHz, &ndiv, bFloor);
            if (status != FLCN_OK)
            {
                return status;
            }

            //
            // Now colwert the NDIV back to frequency. The resultant value
            // will be the output quantized frequency.
            //
            *pFreqMHz = clkNafllFreqFromNdivCompute(pNafllDev, ndiv);
            return status;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    PMU_BREAKPOINT();
    return FLCN_ERROR;
}

/*!
 * @brief Load all NAFLL_DEVICES.
 *
 * @param[in]   actionMask  contains the bit-mask of specific action to be taken
 *                          @ref LW_RM_PMU_CLK_LOAD_ACTION_MASK_<XYZ>
 *
 * @return FLCN_OK if all NAFLL devices loaded successfully
 * @return other   Descriptive status code from sub-routines on success/failure
 */
FLCN_STATUS
clkNafllsLoad
(
   LwU32 actionMask
)
{
    FLCN_STATUS status = FLCN_OK;

    if (FLD_TEST_DRF(_RM_PMU_CLK_LOAD, _ACTION_MASK,
                     _LUT_REPROGRAM, _YES, actionMask))
    {
        status = clkNafllUpdateLutForAll();
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllsLoad_exit;
        }
    }

clkNafllsLoad_exit:
    return status;
}

/*!
 * @brief Given the input frequency, found the next lower freq supported by the
 * given clock domain.
 *
 * @param[in]       clkDomain  Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
 * @param[in,out]   pFreqMHz   Pointer to the buffer which is used to iterate
 *                             over the frequency.
 *
 * @note We are iterating from highest -> lowest freq value.
 *
 * @return FLCN_ERR_NOT_SUPPORTED If the NAFLL device does not exist for the
 *                                given clock domain
 * @return other                  Descriptive status code from sub-routines on
 *                                success/failure
 *
 * @ref s_clkNafllFreqsIterate_FINAL
 */
FLCN_STATUS
clkNafllFreqsIterate
(
    LwU32   clkDomain,
    LwU16  *pFreqMHz
)
{
    CLK_NAFLL_DEVICE   *pNafllDev   = NULL;
    LwBoardObjIdx       devIdx;
    FLCN_STATUS         status      = FLCN_ERR_NOT_SUPPORTED;

    BOARDOBJGRP_ITERATOR_BEGIN(NAFLL_DEVICE, pNafllDev, devIdx, NULL)
    {
        if (pNafllDev->clkDomain == clkDomain)
        {
            status = s_clkNafllFreqsIterate_FINAL(pNafllDev, pFreqMHz);
            return status;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    PMU_BREAKPOINT();
    return status;
}

/*!
 * @brief Given a frequency controller index, get the pointer to the NAFLL device
 *
 * @param[in] controllerIdx  Index of the frequency controller
 *
 * @return Pointer to the NAFLL device, if found
 * @return 'NULL' if the NAFLL device was not found
 */
CLK_NAFLL_DEVICE *
clkNafllFreqCtrlByIdxGet
(
    LwU8 controllerIdx
)
{
    CLK_NAFLL_DEVICE  *pDev = NULL;
    LwBoardObjIdx      devIdx;

    BOARDOBJGRP_ITERATOR_BEGIN(NAFLL_DEVICE, pDev, devIdx, NULL)
    {
        if (pDev->freqCtrlIdx == controllerIdx)
        {
            return pDev;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    PMU_BREAKPOINT();
    return NULL;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Given the input frequency, found the next lower freq supported by the
 * given NAFLL source entry.
 *
 * @param[in]       pNafllDev  Pointer to the NAFLL device
 * @param[in,out]   pFreqMHz   Pointer to the buffer which is used to iterate over
 *                             the frequency.
 *
 * @note we are iterating from highest -> lowest freq value.
 *
 *
 * @return FLCN_OK Successfully found the next lower freq value
 * @return other   Descriptive status code from sub-routines on success/failure
 *
 * @ref clkNafllNdivFromFreqCompute
 */
static FLCN_STATUS
s_clkNafllFreqsIterate_FINAL
(
    CLK_NAFLL_DEVICE       *pNafllDev,
    LwU16                  *pFreqMHz
)
{
    LwU16       ndivMax;
    FLCN_STATUS status;

    // Callwlate the max ndiv values from the last max frequency.
    status = clkNafllNdivFromFreqCompute(pNafllDev, *pFreqMHz, &ndivMax, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }

    //
    // If the input frequency is not quantized to NDIV point, we will consider
    // the NDIV quantized as the next possible lower frequency. On the otherhand,
    // if the input frequency is quantized to NDIV point, decrement the ndivMax
    // by one to get the next highest frequency lower than the input frequency.
    //
    if (clkNafllFreqFromNdivCompute(pNafllDev, ndivMax) == *pFreqMHz)
    {
        ndivMax--;
    }

    *pFreqMHz = clkNafllFreqFromNdivCompute(pNafllDev, ndivMax);

    return FLCN_OK;
}

/*!
 * @brief NAFLL_DEV implementation to parse BOARDOBJGRP header.
 *
 * @copydetails BoardObjGrpIfaceModel10SetHeader()
 */
static FLCN_STATUS
s_clkNafllIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJGRP_SET_HEADER *pNafllSet =
        (RM_PMU_CLK_CLK_NAFLL_DEVICE_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    CLK_NAFLL *pNafllDev = (CLK_NAFLL *)pBObjGrp;
    FLCN_STATUS status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkNafllIfaceModel10SetHeader_exit;
    }

    // Copy global NAFLL_DEV state.
    pNafllDev->lutNumEntries     = pNafllSet->lutNumEntries;
    pNafllDev->lutStepSizeuV     = pNafllSet->lutStepSizeuV;
    pNafllDev->lutMilwoltageuV   = pNafllSet->lutMilwoltageuV;
    pNafllDev->lutMaxVoltageuV   = (pNafllDev->lutMilwoltageuV +
                                    (((LwU32)pNafllDev->lutNumEntries - 1U) *
                                     pNafllDev->lutStepSizeuV));
    pNafllDev->maxDvcoMinFreqMHz = pNafllSet->maxDvcoMinFreqMHz;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_BROADCAST)
    // Import NAFLL LUT PRIMARY MASK LW2080->PMU
    boardObjGrpMaskInit_E32(&(pNafllDev->lutProgPrimaryMask));
    status = boardObjGrpMaskImport_E32(&(pNafllDev->lutProgPrimaryMask),
                                       &(pNafllSet->lutProgPrimaryMask));
#endif

s_clkNafllIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @brief Construct a NAFLL_DEVICE object.
 *
 * @copydetails BoardObjGrpIfaceModel10SetEntry()
 */
static FLCN_STATUS
s_clkNafllIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICE_V10))
            {
                status = clkNafllGrpIfaceModel10ObjSet_V10(pModel10, ppBoardObj,
                    sizeof(CLK_NAFLL_DEVICE_V10), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICE_V20))
            {
                status = clkNafllGrpIfaceModel10ObjSet_V20(pModel10, ppBoardObj,
                    sizeof(CLK_NAFLL_DEVICE_V20), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICE_V30))
            {
                status = clkNafllGrpIfaceModel10ObjSet_V30(pModel10, ppBoardObj,
                    sizeof(CLK_NAFLL_DEVICE_V30), pBuf);
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
