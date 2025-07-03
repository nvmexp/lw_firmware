/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_lsfgp10x.c
 * @brief  Light Secure Falcon (LSF) GP10X HAL Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "dev_sec_csb.h"

/* ------------------------ Application includes --------------------------- */

#include "sec2sw.h"
#include "main.h"
#include "sec2_objlsf.h"

#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/*!
 @brief: Program MTHDCTX PRIV level mask
 */
void
lsfSetupMthdctx_GP10X(void)
{
    LwU32 data32 = REG_RD32(CSB, LW_CSEC_FALCON_SCTL);

    // Check if UCODE is running in LS mode
    if (FLD_TEST_DRF(_CSEC, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {
        data32 = REG_RD32(CSB, LW_CSEC_FALCON_MTHDCTX_PRIV_LEVEL_MASK);

        data32 = FLD_SET_DRF(_CSEC, _FALCON_MTHDCTX_PRIV_LEVEL_MASK,
                             _READ_PROTECTION, _ALL_LEVELS_ENABLED, data32);
        data32 = FLD_SET_DRF(_CSEC, _FALCON_MTHDCTX_PRIV_LEVEL_MASK,
                             _WRITE_PROTECTION_LEVEL0, _DISABLE, data32);

        REG_WR32(CSB, LW_CSEC_FALCON_MTHDCTX_PRIV_LEVEL_MASK, data32);
    }

    // Do not RMW since unselwred entity could have setup invalid fields
    data32 = 0;
    data32 = FLD_SET_DRF(_CSEC, _FALCON_ITFEN, _MTHDEN, _ENABLE, data32);
    data32 = FLD_SET_DRF(_CSEC, _FALCON_ITFEN, _CTXEN,  _ENABLE, data32);
    data32 = FLD_SET_DRF(_CSEC, _FALCON_ITFEN, _PRIV_POSTWR,  _INIT, data32);

    REG_WR32(CSB, LW_CSEC_FALCON_ITFEN, data32);

}
