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
 * @file   lsrga100.c
 * @brief  Contains new functions added for RISC-V LS support on GA100.
 */

/* --------------------------- Sanity Check --------------------------------- */
#ifndef ACR_RISCV_LS
#error "Including lsrga100.c in build but RISC-V LS not enabled!"
#endif

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "acr.h"
#include "acr_objacrlib.h"
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
lsrAcrSetupLSRiscv_GA100
(
    PRM_FLCN_U64        pWprBase,
    LwU32               wprRegionID,
    PACR_FLCN_CONFIG    pFlcnCfg,
    PLSF_LSB_HEADER     pLsbHeader
)
{
    ACR_STATUS  status     = ACR_OK;
    LwU64       wprAddress = 0;

    // Sanity check.
    if (pFlcnCfg->uprocType != ACR_TARGET_ENGINE_CORE_RISCV)
        return ACR_ERROR_ILWALID_OPERATION;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupTargetRegisters_HAL(pAcrlib, pFlcnCfg));

    // RISC-V boots directly from FB.
    wprAddress = (pWprBase->lo | ((LwU64)pWprBase->hi << 32)) +
                  pLsbHeader->ucodeOffset;

    //
    // Set the bootvector. This will also install the "trampoline" boot stub needed on GA100
    // due to Bug #2031674 (COREUCODES-531).
    //
    acrlibSetupBootvecRiscv_HAL(pAcrlib, pFlcnCfg, wprRegionID, pLsbHeader, wprAddress);

    //
    // Program the IMEM and DMEM PLMs to the final desired values (which may
    // be level 3 for certain falcons) now that acrlib is done loading the
    // code onto the falcon.
    //
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, pFlcnCfg->imemPLM);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, pFlcnCfg->dmemPLM);

    return status;
}
