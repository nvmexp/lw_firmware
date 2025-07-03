/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lsrga102.c
 * @brief  Contains new functions added for RISC-V LS support on GA10X.
 */

/* --------------------------- Sanity Check --------------------------------- */
#ifndef ACR_RISCV_LS
#error "Including lsrga102.c in build but RISC-V LS not enabled!"
#endif

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
/* ------------------------- Application Includes --------------------------- */
#include "acr.h"
#include "acr_objacrlib.h"
#include "sec2_objacr.h"
#include "config/g_lsr_hal.h"
/* ------------------------- Macros and Defines ----------------------------- */

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Prepares a RISC-V uproc for boot.
 *
 * TODO: Update this function per the results of bugs 2720166 and 2720167.
 *
 * @param[in] pWprBase     Base address of WPR
 * @param[in] wprRegionID  ACR region ID
 * @param[in] pFlcnCfg     Falcon configuration
 * @param[in] pLsbHeader   LSB header
 */
ACR_STATUS
lsrAcrSetupLSRiscv_GA102
(
    PRM_FLCN_U64        pWprBase,
    LwU32               wprRegionID,
    PACR_FLCN_CONFIG    pFlcnCfg,
    PLSF_LSB_HEADER     pLsbHeader
)
{
    ACR_STATUS  status  = ACR_OK;

    if (pFlcnCfg->uprocType != ACR_TARGET_ENGINE_CORE_RISCV)
    {
        return ACR_ERROR_ILWALID_OPERATION;
    }

    RM_FLCN_U64 physAddr = {0};
    physAddr.lo          = pLsbHeader->ucodeOffset;
    LwU64_ALIGN32_ADD(&physAddr, pWprBase, &physAddr);

    LwU64 wprAddr = 0;
    LwU64_ALIGN32_UNPACK(&wprAddr, &physAddr);
    status = acrlibSetupBootvecRiscv_HAL(pAcrlib, pFlcnCfg, wprRegionID, pLsbHeader,wprAddr);

    return status;
}

/*!
 * @brief Prepares a RISC-V uproc for boot with new wpr blob defines.
 *
 * TODO: Update this function per the results of bugs 2720166 and 2720167.
 *
 * @param[in] pWprBase     Base address of WPR
 * @param[in] wprRegionID  ACR region ID
 * @param[in] pFlcnCfg     Falcon configuration
 * @param[in] pLsbHeader   LSB header
 */
ACR_STATUS
lsrAcrSetupLSRiscvExt_GA102
(
    PRM_FLCN_U64                pWprBase,
    LwU32                       wprRegionID,
    PACR_FLCN_CONFIG            pFlcnCfg,
    PLSF_LSB_HEADER_WRAPPER     pLsbHeaderWrapper
)
{
    ACR_STATUS  status  = ACR_OK;
    LwU32       ucodeOffset;

    if (pFlcnCfg->uprocType != ACR_TARGET_ENGINE_CORE_RISCV)
    {
        return ACR_ERROR_ILWALID_OPERATION;
    }

    status = acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                               pLsbHeaderWrapper, (void *)&ucodeOffset);
    if (status != ACR_OK)
    {       
        return status;
    }

    RM_FLCN_U64 physAddr = {0};
    physAddr.lo          = ucodeOffset;
    LwU64_ALIGN32_ADD(&physAddr, pWprBase, &physAddr);

    LwU64 wprAddr = 0;
    LwU64_ALIGN32_UNPACK(&wprAddr, &physAddr);
    status = acrlibSetupBootvecRiscvExt_HAL(pAcrlib, pFlcnCfg, wprRegionID, pLsbHeaderWrapper, wprAddr);

    return status;
}
