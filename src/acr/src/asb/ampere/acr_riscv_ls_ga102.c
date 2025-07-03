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
 * @file: acr_riscv_ls_ga102.c
 */

// Sanity check.
#ifndef ACR_RISCV_LS
#error "Including acr_riscv_ls_ga102.c in build but RISC-V LS is not enabled!"
#endif
#include "lwtypes.h"
#include "acr.h"
#include "acr_objacrlib.h"
#include "dev_riscv_pri.h"

/*
 * @brief: Initiaze boot environment of RISCV
 *
 * @param[in] pFlcnCfg   Falcon configuration information
 * @param[in] bootvec    Boot vector #Depricated for GA10X
 * @param[in] regionId   WPR region ID
 * @param[in] pLsbHeader LSB header of RISCV core
 * @param[in] wprAddress WPR address of base of image
 */
ACR_STATUS
acrlibSetupBootvecRiscv_GA102
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    LwU32               regionId,
    PLSF_LSB_HEADER     pLsbHeader,
    LwU64               wprAddress
)
{
    /*
    LwU64 val = 0;
    val = (wprAddress + pLsbHeader->monitorCodeOffset)>>8;
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_LO, LwU64_LO32(val));
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_HI, LwU64_HI32(val));

    val = (wprAddress + pLsbHeader->monitorDataOffset)>>8;
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_LO,  LwU64_LO32(val));
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_HI,  LwU64_HI32(val));

    val = (wprAddress + pLsbHeader->manifestOffset)>>8;
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_HI, LwU64_HI32(val));
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_PKCPARAM_LO, LwU64_LO32(val));

    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMACFG_SEC, regionId);

    LwU32 dmaCfg = DRF_DEF(_PRISCV_RISCV, _BCR_DMACFG, _TARGET, _LOCAL_FB)    |
                   DRF_DEF(_PRISCV_RISCV, _BCR_DMACFG, _LOCK,   _LOCKED);
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMACFG, dmaCfg);
   */
    return ACR_OK;
}

ACR_STATUS
acrlibSetupBootvecRiscvExt_GA102
(
    PACR_FLCN_CONFIG          pFlcnCfg,
    LwU32                     regionId,
    PLSF_LSB_HEADER_WRAPPER   pLsbHeaderWrapper,
    LwU64                     wprAddress
)
{
    LwU64 val               = 0;
    LwU32 data              = 0;
    LwU32 monitorCodeOffset = 0;
    LwU32 monitorDataOffset = 0;
    LwU32 manifestOffset    = 0;
    ACR_STATUS status       = ACR_ERROR_ILWALID_OPERATION;


    data = ACR_REG_RD32(BAR0,  pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK);
    data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL, _MASK_READ_PROTECTION, _ALL_LEVELS_ENABLED, data);
    data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL, _MASK_WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, data);

    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK, data);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_MONITOR_CODE_OFFSET,
                                                                        pLsbHeaderWrapper, &monitorCodeOffset));
    val = (wprAddress + monitorCodeOffset)>>8;
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_LO, LwU64_LO32(val));
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_HI, LwU64_HI32(val));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_MONITOR_DATA_OFFSET,
                                                                        pLsbHeaderWrapper, &monitorDataOffset));
    val = (wprAddress + monitorDataOffset)>>8;
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_LO,  LwU64_LO32(val));
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_DMAADDR_FMCDATA_HI,  LwU64_HI32(val));


    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_MANIFEST_OFFSET,
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
