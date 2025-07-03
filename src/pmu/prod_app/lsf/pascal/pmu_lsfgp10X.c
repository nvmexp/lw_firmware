/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_lsfgp10X.c
 * @brief  Light Secure Falcon (LSF) GP10X Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"

/* ------------------------ Application includes --------------------------- */
#include "pmu_objlsf.h"
#include "pmu_inforom.h"

#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */


/*!
 * @brief Initialize Inforom Data
 *
 * @param[in] pInforomData  
 */
void
lsfInitInforomData_GP10X
(
    LwU32 *pInforomData
)
{
    // Initialize Inforom Data
    inforom.startAddress = pInforomData[0];
    inforom.endAddress   = pInforomData[1];
}


/*!
 * @brief Initializes (closes) DMEM carveout regions.
 *        Raises PLM to PL3 write-protected only.
 */
void
lsfInitDmemCarveoutRegions_GP10X(void)
{
#ifndef UPROC_RISCV
    LwU32 data0 = 0x0;
    LwU32 data1 = 0x0;

    data0 = FLD_SET_DRF( _CPWR, _FALCON_DMEM_PRIV_RANGE0, _START_BLOCK, _INIT, data0);
    data0 = FLD_SET_DRF( _CPWR, _FALCON_DMEM_PRIV_RANGE0, _END_BLOCK, _INIT, data0);

    data1 = FLD_SET_DRF( _CPWR, _FALCON_DMEM_PRIV_RANGE1, _START_BLOCK, _INIT, data1);
    data1 = FLD_SET_DRF( _CPWR, _FALCON_DMEM_PRIV_RANGE1, _END_BLOCK, _INIT, data1);

    // Close the DMEM carveout regions
    REG_WR32(CSB, LW_CPWR_FALCON_DMEM_PRIV_RANGE0, data0);
    REG_WR32(CSB, LW_CPWR_FALCON_DMEM_PRIV_RANGE1, data1);

    //
    // Raise the PLM to PL2 write-access, since it is not required for
    // GP100+. If we don't raise the PLM then a compromised or hijacked PMU 
    // ucode can open up the whole DMEM as a carveout.
    //
    data0 = REG_RD32(CSB, LW_CPWR_FALCON_DMEM_PRIV_LEVEL_MASK);
    data0 = FLD_SET_DRF( _CPWR, _FALCON_DMEM_PRIV_LEVEL_MASK, _READ_PROTECTION,         _ALL_LEVELS_ENABLED, data0);
    data0 = FLD_SET_DRF( _CPWR, _FALCON_DMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE,            data0);
    data0 = FLD_SET_DRF( _CPWR, _FALCON_DMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE,            data0);
    data0 = FLD_SET_DRF( _CPWR, _FALCON_DMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE,            data0);
    data0 = FLD_SET_DRF( _CPWR, _FALCON_DMEM_PRIV_LEVEL_MASK, _READ_VIOLATION,          _REPORT_ERROR,       data0);
    data0 = FLD_SET_DRF( _CPWR, _FALCON_DMEM_PRIV_LEVEL_MASK, _WRITE_VIOLATION,         _REPORT_ERROR,       data0);
    REG_WR32(CSB, LW_CPWR_FALCON_DMEM_PRIV_LEVEL_MASK, data0);
#endif // UPROC_RISCV
}
