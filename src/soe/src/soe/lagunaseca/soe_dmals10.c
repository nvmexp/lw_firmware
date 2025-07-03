/*
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_dmals10.c
 * @brief  SOE DMA Functions for LS10
 *
 * SOE DMA functions implement chip-specific DMA operations for Limerock.
 * This is necessary because Limerock allows 64-bit DMA addressing by programming
 * the top 15 bits of the DMA address in a separate register. Future chips may do
 * this differently.
 */
/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_bar0.h"
#include "soe_objsaw.h"
#include "dev_lwlsaw_ip.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*
 * The XAL_UPPER_ADDR is 12 bits wide. This macro helps pull the top 12 bits from
 * a 4 byte value.
 */
#define LW_SOE_DMA_XAL_UPPER_ADDR_SHIFT 20

/* ------------------------- Private Functions ------------------------------ */
void
soeDmaSetSysAddr_LS10(RM_FLCN_U64 dmaHandle, RM_FLCN_MEM_DESC *pMemDesc)
{
    LwU32 shimAddr;

    pMemDesc->address = dmaHandle;
    pMemDesc->params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                        SOE_DMAIDX_PHYS_SYS_COH_FN0, 0);

    // Select the top 12 bits to be programmed to the shim
    shimAddr = FLD_SET_DRF_NUM(_LWLSAW_SOE, _SHIM_CTRL, _XAL_UPPER_ADDR,
                                dmaHandle.hi >> LW_SOE_DMA_XAL_UPPER_ADDR_SHIFT, 0);

    REG_WR32(SAW, LW_LWLSAW_SOE_SHIM_CTRL, shimAddr);

    return;
}

