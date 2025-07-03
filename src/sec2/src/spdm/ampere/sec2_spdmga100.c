/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_spdmga100.c
 * @brief  Contains all SPDM HALs specific to GA100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "dev_fuse.h"
#include "dev_master.h"
/* ------------------------- Application Includes --------------------------- */
#include "sec2_objspdm.h"
#include "flcnretval.h"
#include "sec2_bar0.h"
#include "apm/sec2_apmdefines.h"
#include "sec2_csb.h"
#include "dev_gc6_island.h"
#include "hwproject.h"
#include "lwdevid.h"

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Checks whether SPDM can be enabled or not.
 *
 * @return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED if not supported
 *         else FLCN_OK
 */
FLCN_STATUS
spdmCheckIfSpdmIsSupported_GA100
(
    void
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_APM_NOT_ENABLED;
    LwU32       val        = 0;
    LwU32       apmMode    = 0;
    LwU32       scratch    = 0;
    LwU32       chipId     = 0;
    LwU32       devId      = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS, &val));
    if (val != LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS_DATA_NO)
    {
        // Check for APM mode
        CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_20, &apmMode));
        if (!FLD_TEST_DRF(_SEC2, _APM_VBIOS_SCRATCH, _CCM, _ENABLE, apmMode))
        {
            flcnStatus = FLCN_ERR_HS_APM_NOT_ENABLED;
            goto ErrorExit;
        }

        // SKU Check
        CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_FUSE_OPT_PCIE_DEVIDA, &devId));
        CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_PMC_BOOT_42, &chipId));
        devId  = DRF_VAL( _FUSE, _OPT_PCIE_DEVIDA, _DATA, devId);
        chipId = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chipId);

        // Adding ct_assert to alert incase if the DevId values are changed for the defines
        ct_assert(LW_PCI_DEVID_DEVICE_P1001_SKU230 == 0x20B5);

        if (!((chipId == LW_PMC_BOOT_42_CHIP_ID_GA100) && ( devId == LW_PCI_DEVID_DEVICE_P1001_SKU230)))
        {
            flcnStatus = FLCN_ERR_HS_APM_NOT_ENABLED;
            goto ErrorExit;
        }
    }

    // APM Scratch check
    CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(
                      LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0(LW_SEC2_APM_SCRATCH_INDEX), &scratch));

    if (scratch != LW_SEC2_APM_STATUS_VALID)
    {
        flcnStatus = FLCN_ERR_HS_APM_NOT_ENABLED;
        goto ErrorExit;
    }

ErrorExit:
    return flcnStatus;
}
