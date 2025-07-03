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
 * @file: acruc_rv_libt234.c
 */

#include "rv_acr.h"

#include <liblwriscv/io_pri.h>
#include <liblwriscv/io_csb.h>

#include "dev_fb.h"
#include "dev_master.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_csb.h"
#include "dev_falcon_v4.h"
#include "dev_fbif_v4.h"
#include "dev_graphics_nobundle.h"
 
#include "dev_bus.h"
#include "dpu/dpuifcmn.h"
#include "external/pmuifcmn.h"
#include "external/rmlsfm.h"

/*!
 * @brief Write status into MAILBOX0
 * @param[in] status ERROR status to be dumped to MAILBOX0.
 */
void
acrWriteStatusToFalconMailbox(ACR_STATUS status)
{
    csbWrite(LW_PRGNLCL_FALCON_MAILBOX0, (LwU32)status);
}

/*!
 * @brief Check if given falconId equals current bootstrap owner
 *
 * @param[in] falconId     FalconID to check if bootstrap owner or not
 * @return    LW_TRUE      If falconId is same as CURRENT BOOTSTRAP OWNER Id.
 * @return    LW_FALSE     If falconId is different from CURRENT BOOTSTRAP OWNER Id.
 */
LwBool
acrlibIsBootstrapOwner_T234
(
    LwU32        falconId
)
{
    if (falconId == ACR_LSF_LWRRENT_BOOTSTRAP_OWNER)
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

/*!
 * @brief Force resets a given falcon.
 *        Check if falconID is bootstrap owner and skip reset and return if not.
 *        Initiate a force falcon reset if falcon is not in HALTED state.
 *        Poll for falcon to reach HALT state.
 *        Poll for falcon reset to complete.
 * 
 * @param[in] pFlcnCfg Falcon Configuration.
 *
 * @return ACR_OK If reset is successful.
 * @return ACR_ERROR_TIMEOUT If encountered timeout polling for completion of steps ilwolved in this function.
 */
static ACR_STATUS
acrFlcnDoForceReset
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_TIMESTAMP    startTimeNs;
    LwS32            timeoutLeftNs;
    ACR_STATUS       status          = ACR_OK;

    // If pFlcnCfg is also bootstrap owner, then it wont be halted
    if (!acrlibIsBootstrapOwner_HAL(pFlcnCfg->falconId))
    {
        // Step-5: Check if the falcon is HALTED
        if (!acrlibIsFalconHalted_T234(pFlcnCfg))
        {
            // Force reset falcon only..
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibResetFalcon_HAL(pFlcnCfg,
                                                                    LW_TRUE));
        }

        // Wait for it to reach HALT state
        acrlibGetLwrrentTimeNs_HAL(&startTimeNs);
        while(!acrlibIsFalconHalted_HAL(pFlcnCfg))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(ACR_DEFAULT_TIMEOUT_NS, 
                                                            startTimeNs,
                                                            &timeoutLeftNs));
        }

        // Wait for polling to complete
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPollForScrubbing_HAL(pFlcnCfg));
    }

    return status;
}

/*!
 * @brief Check for FALCON_SCTL_AUTH_EN status.
 *
 * @param[in] pFlcnCfg Falcon Configuration.
 * 
 * @return ACR_OK If FALCON_SCTL_AUTH_EN is true.
 * @return ACR_ERROR_LS_BOOT_FAIL_AUTH if FALCON_SCTL_AUTH_EN is false.
 */
static ACR_STATUS
acrCheckAuthEnabled
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32            data        = 0;

    data = acrlibFlcnRegLabelRead_HAL(pFlcnCfg, REG_LABEL_FLCN_SCTL);
    if (!FLD_TEST_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data))
    {
        return ACR_ERROR_LS_BOOT_FAIL_AUTH;
    }

    return ACR_OK;
}

/*!
 * @brief Programs Falcon target registers to ensure that falcon goes into LS mode.
 *        Set SCTL priv level mask to only HS accessible.\n
 *        Set SCTL_RESET_LVLM_EN to FALSE.\n
 *        Set SCTL_STALLREQ_CLR_EN to FALSE.\n
 *        Set SCTL_AUTH_EN to TRUE.\n
 *        Set CPUCTL_ALIAS_EN to FALSE.\n
 *        Set PLMs for IMEM, DMEM, CPUCTL, DEBUG, EXE, IRQTMR, MTHDCTX and FBIF_REGIONCFG registers.\n
 *        Authorize LS falcon by writing to SCTL and return failure if authorisation fails.\n
 *        Set CPUCTL_ALIAS_EN to TRUE.\n
 *        Enable ICD(Inter-circuit Debug) registers.\n
 *
 * @param[in] pFlcnCfg Falcon Configuration.
 *
 * @return ACR_OK If setup of TARGET registers is successful.
 * @return ACR_ERROR_LS_BOOT_FAIL_AUTH If LS falcon authorization fails.
 * 
 */
ACR_STATUS
acrlibSetupTargetRegisters_T234
(
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    ACR_STATUS       status      = ACR_OK;
    LwU32            data        = 0;

    if (pFlcnCfg->falconId == LSF_FALCON_ID_PMU_RISCV)
    {
        return ACR_OK;
    }

    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_SCTL_PRIV_LEVEL_MASK,
                                                ACR_PLMASK_READ_L0_WRITE_L2); 

    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _FALSE, 0);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _STALLREQ_CLR_EN, _FALSE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data);
    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_SCTL, data);

    data = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, 0);
    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_CPUCTL, data);

    // Use level 2 protection for writes to IMEM/DMEM. We do not need read protection and leaving IMEM/DMEM
    // open can aid debug if a problem happens before the PLM is overwritten by ACR binary or the PMU task to
    // the final value. Note that level 2 is the max we can use here because this code is run inside PMU at
    // level 2. In future when we move the acr task from PMU to SEC2 and make it HS, we can afford to use
    // the final values from pFlcnCfg->{imem, dmem}PLM directly over here.
    //
    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK,
                                                ACR_PLMASK_READ_L0_WRITE_L2);
    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK,
                                                ACR_PLMASK_READ_L0_WRITE_L2);

    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK,
                                                ACR_PLMASK_READ_L0_WRITE_L2);
    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK,
                                                ACR_PLMASK_READ_L0_WRITE_L2);
    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_IRQTMR_PRIV_LEVEL_MASK,
                                                ACR_PLMASK_READ_L0_WRITE_L0);
    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_MTHDCTX_PRIV_LEVEL_MASK,
                                                ACR_PLMASK_READ_L0_WRITE_L0);

    if (pFlcnCfg->bFbifPresent)
    {
        acrlibFlcnRegLabelWrite_HAL(pFlcnCfg,
                                    REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK,
                                    ACR_PLMASK_READ_L0_WRITE_L2);
    }
    else
    {
        priWrite(pFlcnCfg->regCfgMaskAddr, ACR_PLMASK_READ_L0_WRITE_L2);
    }

#ifndef ACR_ONLY_BO
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnDoForceReset(pFlcnCfg));
#endif
    //
    // Step-6: Authorize falcon
    // TODO: Let the individual falcons ucodes take care of setting up TRANSCFG/REGIONCFG
    //

    // Program LSMODE here
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _TRUE, 0);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _STALLREQ_CLR_EN, _TRUE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _LSMODE, _TRUE, data);
    data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_SCTL, _LSMODE_LEVEL, 0x2, data);
    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_SCTL, data);

    // Check if AUTH_EN is still TRUE
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckAuthEnabled(pFlcnCfg));

    // Step-7: Set CPUCTL_ALIAS_EN to TRUE
    data = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _ALIAS_EN, _TRUE, 0);
    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_CPUCTL, data);

    // Step-8: Enable ICD
    acrlibEnableICD(pFlcnCfg);

    return status;
}

/*!
 * @brief Check if given timeout has elapsed or not.
 *
 * @param[in]  timeoutNs       Timeout in nano seconds.
 * @param[in]  startTimeNs     Start time for this timeout cycle.
 * @param[out] pTimeoutLeftNs  Remaining time in nano seconds.
 *
 * @return ACR_ERROR_TIMEOUT If timed out.
 * @return ACR_OK            If remaining time i.e. pTimeoutLeftNs is still > 0.
 */
ACR_STATUS
acrlibCheckTimeout_T234
(
    LwU32         timeoutNs,
    ACR_TIMESTAMP startTimeNs,
    LwS32         *pTimeoutLeftNs
)
{
    //Cast U32 to S64 to avoid loss of data (CERT-C INT30)
    //Safe as r-value cannot exceed 32 bits
    LwS64 timeoutLeftNs = ((LwS64)timeoutNs -
                            (LwS64)acrlibGetElapsedTimeNs_HAL(&startTimeNs));

    if (timeoutLeftNs <= 0)
    {
        return ACR_ERROR_TIMEOUT;
    }
    else
    {
        *pTimeoutLeftNs = (LwS32)timeoutLeftNs;
        return ACR_OK;
    }
}