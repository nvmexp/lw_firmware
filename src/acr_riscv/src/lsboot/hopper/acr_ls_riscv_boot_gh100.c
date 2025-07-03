/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_riscv_ls_gh100.c
 */

// Sanity check.
#ifndef ACR_RISCV_LS
#error "Including acr_riscv_ls_gh100.c in build but RISC-V LS is not enabled!"
#endif
#include "lwtypes.h"
#include "acr.h"
#include "acr_objacr.h"
#include "dev_riscv_pri.h"


ACR_STATUS
acrSetupBootvecRiscvExt_GH100
(
    PACR_FLCN_CONFIG          pFlcnCfg,
    LwU32                     regionId,
    PLSF_LSB_HEADER_WRAPPER   pLsbHeaderWrapper,
    LwU64                     wprAddress
)
{
    LwU64 val               = 0;
    LwU32 monitorCodeOffset = 0;
    LwU32 monitorDataOffset = 0;
    LwU32 manifestOffset    = 0;
    ACR_STATUS status       = ACR_ERROR_ILWALID_OPERATION;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_MONITOR_CODE_OFFSET,
                                                                        pLsbHeaderWrapper, &monitorCodeOffset));
    val = (wprAddress + monitorCodeOffset)>>8;
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_LO, LwU64_LO32(val));
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_HI, LwU64_HI32(val));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_MONITOR_DATA_OFFSET,
                                                                        pLsbHeaderWrapper, &monitorDataOffset));
    val = (wprAddress + monitorDataOffset)>>8;
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_LO,  LwU64_LO32(val));
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_HI,  LwU64_HI32(val));


    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_MANIFEST_OFFSET,
                                                                        pLsbHeaderWrapper, &manifestOffset));

    val = (wprAddress + manifestOffset)>>8;
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_HI, LwU64_HI32(val));
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_LO, LwU64_LO32(val));

    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMACFG_SEC, regionId);

    // Make sure to override HW INIT Values. Do not RMW
    LwU32 dmaCfg = DRF_DEF(_PRISCV_RISCV, _BCR_DMACFG, _TARGET, _LOCAL_FB)    |
                   DRF_DEF(_PRISCV_RISCV, _BCR_DMACFG, _LOCK,   _LOCKED);
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMACFG, dmaCfg);
   return ACR_OK;
}
