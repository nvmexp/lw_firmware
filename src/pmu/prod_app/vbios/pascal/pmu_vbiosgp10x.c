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
 * @file   pmu_vbiosgp10x.c
 * @brief  HAL-interface for the VBIOS functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objvbios.h"
#include "dev_disp.h"

#include "config/g_vbios_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define LW_DEVINIT_ADDR_REG                       23:0
#define LW_DEVINIT_ADDR_USE_DPIPE                 31:31
#define LW_DEVINIT_ADDR_USE_DPIPE_ENABLED    0x00000001
#define LW_DEVINIT_ADDR_USE_DEVICE                30:30
#define LW_DEVINIT_ADDR_USE_DEVICE_ENABLED   0x00000001
#define LW_DEVINIT_ADDR_USE_SUBLINK               29:29
#define LW_DEVINIT_ADDR_USE_SUBLINK_ENABLED  0x00000001

/* --------------------- Private functions Prototypes ----------------------- */
/* --------------------------- Public functions ----------------------------- */

/*!
* @brief       patch address with orIndex and linkIndex
*
* @param[in]   pCtx     devinit engine context
* @param[out]  pData    data
*
* @return      patched address
*/
LwU32
vbiosOpcodePatchAddr_GP10X(VBIOS_IED_CONTEXT *pCtx, LwU32 addr)
{
    if (FLD_TEST_DRF(_DEVINIT, _ADDR, _USE_DEVICE, _ENABLED, addr))
    {
        addr += pCtx->orIndex * (LW_PDISP_DAC_CAP(1) - LW_PDISP_DAC_CAP(0));
    }

    if (FLD_TEST_DRF(_DEVINIT, _ADDR, _USE_SUBLINK, _ENABLED, addr))
    {
        addr += (LW_PDISP_SOR_DP_LINKCTL(1, pCtx->linkIndex) - LW_PDISP_SOR_DP_LINKCTL(1, 0));
    }

    return DRF_VAL(_DEVINIT, _ADDR, _REG, addr);
}

