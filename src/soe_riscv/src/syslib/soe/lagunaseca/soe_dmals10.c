/*
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_dmalr10.c
 * @brief  SOE DMA Functions for LR10
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
#include "soe_objsoe.h"
#include "dev_lwlsaw_ip.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*
 * The XAL_UPPER_ADDR is 12 bits wide. This macro helps pull the top 12 bits from
 * a 4 byte value.
 */
#define LW_SOE_DMA_XAL_UPPER_ADDR_SHIFT 20

/* ------------------------- Private Functions ------------------------------ */
sysSYSLIB_CODE void
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
 
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Perform a DMA write to system memory.
 *
 * @param[in]  pBuf             pointer to data buffer in DMEM (source)
 * @param[in]  dmaHandle        DMA handle (destination)
 * @param[in]  dmaOffset        optional offset into DMA handle
 * @param[in]  bufSize          size of the DMEM buffer
 *
 * @return FLCN_OK
 *     If DMA write was issued successfully.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
soeDmaWriteToSysmem_LS10(void *pBuf, RM_FLCN_U64 dmaHandle, LwU32 dmaOffset, LwU32 bufSize)
{
    RM_FLCN_MEM_DESC memDesc;
    FLCN_STATUS ret;

    lwosDmaLockAcquire();
    lwrtosENTER_CRITICAL();

    soeDmaSetSysAddr_HAL(&Soe, dmaHandle, &memDesc);
    ret = dmaWrite(pBuf, &memDesc, dmaOffset, bufSize);

    lwrtosEXIT_CRITICAL();
    lwosDmaLockRelease();

    return ret;
}

/*!
 * Perform a DMA read from system memory.
 *
 * @param[in]  pBuf             pointer to data buffer in DMEM (destination)
 * @param[in]  dmaHandle        DMA handle (source)
 * @param[in]  dmaOffset        optional offset into DMA handle
 * @param[in]  bufSize          size of the DMEM buffer
 *
 * @return FLCN_OK
 *     If DMA read was issued successfully.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
soeDmaReadFromSysmem_LS10(void *pBuf, RM_FLCN_U64 dmaHandle, LwU32 dmaOffset, LwU32 bufSize)
{
    RM_FLCN_MEM_DESC memDesc;
    FLCN_STATUS ret;

    lwosDmaLockAcquire();
    lwrtosENTER_CRITICAL();

    soeDmaSetSysAddr_HAL(&Soe, dmaHandle, &memDesc);
    ret = dmaRead(pBuf, &memDesc, dmaOffset, bufSize);

    lwrtosEXIT_CRITICAL();
    lwosDmaLockRelease();

    return ret;
}
