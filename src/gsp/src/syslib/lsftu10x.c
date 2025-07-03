/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lsftu10x.c
 * @brief  Light Secure Falcon (LSF) TU10X Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "gspsw.h"
#include "rmlsfm.h"
#include "gsp_bar0.h"
#include "dev_gsp_addendum.h"
#include "gspifcmn.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_lsf_hal.h"
#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */
/*!
 * @brief  Lsf initialization.
 *
 * @return  FLCN_OK on success.
 * @return  Error code from sub-routines.
 */
sysSYSLIB_CODE FLCN_STATUS
lsfInit_TU10X(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Setup aperture settings (protected TRANSCONFIG regs)
    status = lsfInitApertureSettings_HAL(&LsfHal);
    if (status != FLCN_OK)
    {
        goto lsfInit_TU10X_exit;
    }

lsfInit_TU10X_exit:
    return status;
}

/*!
 * @brief  Setting up FBIF apertures.
 *
 * @return  FLCN_OK on success.
 */
sysSYSLIB_CODE FLCN_STATUS
lsfInitApertureSettings_TU10X(void)
{
    // Generic PHYS_VID_FN0 aperture
    intioWrite(LW_PRGNLCL_FBIF_TRANSCFG(RM_GSP_DMAIDX_PHYS_VID_FN0),
        (DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
         DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _TARGET,   _LOCAL_FB)));

    // Generic COHERENT_SYSMEM aperture
    intioWrite(LW_PRGNLCL_FBIF_TRANSCFG(RM_GSP_DMAIDX_PHYS_SYS_COH_FN0),
        (DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
         DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _TARGET,   _COHERENT_SYSMEM)));

    // Generic NONCOHERENT_SYSMEM aperture
    intioWrite(LW_PRGNLCL_FBIF_TRANSCFG(RM_GSP_DMAIDX_PHYS_SYS_NCOH_FN0),
        (DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
         DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _TARGET,   _NONCOHERENT_SYSMEM)));

    return FLCN_OK;
}

/*!
 * Checks whether an access using the specified dmaIdx is permitted.
 *  
 * @param[in]  dmaIdx    DMA index to check
 * 
 * @return     LwBool    LW_TRUE  if the access is permitted
 *                       LW_FALSE if the access is not permitted
 */
sysSYSLIB_CODE LwBool
lsfIsDmaIdxPermitted_TU10X
(
    LwU8 dmaIdx
)
{
    LwU32 val = REG_RD32(BAR0, LW_PGSP_FBIF_REGIONCFG);

    return (DRF_IDX_VAL(_PGSP_FBIF_, REGIONCFG, _T, dmaIdx, val) ==
            LSF_UNPROTECTED_REGION_ID);
}
