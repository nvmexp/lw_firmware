/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_pmugm20x_only.c
 *          PMU HAL Functions for only GM20X GPUs
 *
 * PMU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pmgr.h"
#include "dev_trim.h"
#include "lwdevid.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpmu.h"
#include "config/g_pmu_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Raises the priv level for selected registers
 *        For CS and INT skus only this is controlled by a
 *        unselwre registry key value passed by RM.  For all
 *        other SKUs, this is the default behavior.
 * 
 * @param[in]  bRaisePrivSec  priv security enablement reg key value for CS/INT
 */
void
pmuRaisePrivSecLevelForRegisters_GM20X
(
    LwBool bRaisePrivSec
)
{
    LwU32 reg32;

    // Return if we are not in LSMODE
    if (!Pmu.bLSMode)
    {
        return;
    }

    // If devid is CS or INT sku and bRaisePrivSec is false, return without raising security
    if ((Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM204_E2724_SKU10_INT_Geforce) ||
        (Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM204_E2724_SKU10_CS_Geforce)  ||
        (Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM204_E2724_SKU10_INT_Quadro)  ||
        (Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM204_E2724_SKU10_CS_Quadro)   ||
        (Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM206_E2726_SKU0_DEVE)         ||
        (Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM206_E2726_SKU0_DEVF)         ||
        (Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM206_PG301_SKU300_DEVF)       ||
        (Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM206_PG301_SKU300_DEVE)       ||
        (Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM200_PG600_SKU30_DEVE)        ||
        (Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM200_PG600_SKU30_DEVF)        ||
        (Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM200_PG600_SKU30_DEV1)        ||
        (Pmu.gpuDevId == LW_PCI_DEVID_DEVICE_GM200_PG600_SKU00))
    {
        if (!bRaisePrivSec)
        {
            return;
        }
        else
        {
            // Raise security on all for CS/INT skus
            reg32 = REG_RD32(FECS, LW_PMGR_I2C2_PRIV_LEVEL_MASK);
            reg32 = FLD_SET_DRF(_PMGR, _I2C2_PRIV_LEVEL_MASK, 
                                _WRITE_PROTECTION, _LEVEL2_ENABLED_FUSE1, reg32);
            REG_WR32(FECS, LW_PMGR_I2C2_PRIV_LEVEL_MASK, reg32);

            reg32 = REG_RD32(FECS, LW_PTRIM_SYS_PRIV_SELWRE_UTILSCLK_LDIV_PRIV_LEVEL_MASK);
            reg32 = FLD_SET_DRF(_PTRIM, _SYS_PRIV_SELWRE_UTILSCLK_LDIV_PRIV_LEVEL_MASK,
                                _WRITE_PROTECTION, _LEVEL2_ONLY_ACCESS_FUSE1, reg32);
            REG_WR32(FECS, LW_PTRIM_SYS_PRIV_SELWRE_UTILSCLK_LDIV_PRIV_LEVEL_MASK, reg32);

            reg32 = REG_RD32(CSB, LW_CPWR_THERM_THERMAL_PRIV_LEVEL_MASK_MED_HIGH_1);
            reg32 = FLD_SET_DRF(_CPWR_THERM, _THERMAL_PRIV_LEVEL_MASK_MED_HIGH_1,
                                _WRITE_PROTECTION, _LEVEL2_ENABLED_FUSE1, reg32);
            REG_WR32(CSB, LW_CPWR_THERM_THERMAL_PRIV_LEVEL_MASK_MED_HIGH_1, reg32);

            reg32 = REG_RD32(CSB, LW_CPWR_THERM_THERMAL_PRIV_LEVEL_MASK_MED_HIGH_0);
            reg32 = FLD_SET_DRF(_CPWR_THERM, _THERMAL_PRIV_LEVEL_MASK_MED_HIGH_0,
                                _WRITE_PROTECTION, _LEVEL2_ENABLED_FUSE1, reg32);
            REG_WR32(CSB, LW_CPWR_THERM_THERMAL_PRIV_LEVEL_MASK_MED_HIGH_0, reg32);
        }
    }
    else
    {
        //
        // Prod SKU  - raise security by default for only I2C.
        // Priv Sec for THERM and UTILSCLKis enabled by fuses on prod skus
        // Raising security here means that PSDL will not work for I2C_C
        //
        reg32 = REG_RD32(FECS, LW_PMGR_I2C2_PRIV_LEVEL_MASK);
        reg32 = FLD_SET_DRF(_PMGR, _I2C2_PRIV_LEVEL_MASK, 
                            _WRITE_PROTECTION, _LEVEL2_ENABLED_FUSE1, reg32);
        REG_WR32(FECS, LW_PMGR_I2C2_PRIV_LEVEL_MASK, reg32);
    }
}

