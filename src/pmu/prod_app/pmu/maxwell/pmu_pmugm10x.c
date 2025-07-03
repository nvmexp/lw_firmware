/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pmugm10x.c
 * PMU Hal Functions for GM10X
 *
 * PMU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pri_ringstation_sys.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_bar0.h"

#include "config/g_pmu_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Add all levels to access list for DECODE_TRAP9_PRIV_MASK
 *
 * Bug 1333388  - Ctxsw ucode uses DECODE_TRAP9 to block transactions during
 * context reset and the PRIV LEVEL is set too high for FECS to access
 * resulting in FECS error  LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_RESET.  The
 * SW WAR is to give FECS access. This should be fixed in subsequent chips.
 * The bug for subsequent chips is 1348433.
 */
void
pmuPreInitWarForFecsBug1333388_GM10X(void)
{
     REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP9_PRIV_LEVEL_MASK,
        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP9_PRIV_LEVEL_MASK,
                _CONTROL_READ_PROTECTION, _ALL_LEVELS_ENABLED)  |
        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP9_PRIV_LEVEL_MASK,
                _CONTROL_READ_VIOLATION, _REPORT_ERROR)         |
        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP9_PRIV_LEVEL_MASK,
                _CONTROL_WRITE_PROTECTION, _ALL_LEVELS_ENABLED) |
        DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP9_PRIV_LEVEL_MASK,
                _CONTROL_WRITE_VIOLATION, _REPORT_ERROR));
}
