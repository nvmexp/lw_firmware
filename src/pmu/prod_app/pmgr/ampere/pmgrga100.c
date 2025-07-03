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
 * @file   pmgrga100.c
 * @brief  Contains PMGR routines specific to GA100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_fuse.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmu_objfuse.h"

#include "config/g_pmgr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Reads the VCM Coarse Offset value from LW_FUSE_OPT_ADC_CAL_SENSE[15:12] and
 * writes it to LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL2[31:28].
 *
 * @note    This HAL should be called only from @ref pwrDevLoad.
 */
FLCN_STATUS
pmgrPwrDevGpuAdc10CoarseOffsetUpdate_GA100
(
    LwU32 *pReg32
)
{
    LwU32 vcmCoarseOffset;
    LwU32 fuseVal = fuseRead(LW_FUSE_OPT_ADC_CAL_SENSE);

    // Extract the VCM Coarse Offset from the fuse
    vcmCoarseOffset = DRF_VAL(_FUSE, _OPT_ADC_CAL_SENSE,
        _VCM_COARSE_CALIBRATION_DATA, fuseVal);
    *pReg32 = FLD_SET_DRF_NUM(_PTRIM_SYS_NAFLL, _SYS_PWR_SENSE_ADC_CTRL2,
        _ADC_CTRL_COARSE_OFFSET, vcmCoarseOffset, *pReg32);

    return FLCN_OK;
}
