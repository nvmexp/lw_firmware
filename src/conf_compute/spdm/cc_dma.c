/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include <lwmisc.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>
#include <liblwriscv/fbif.h>
#include <liblwriscv/dma.h>
#include <liblwriscv/libc.h>
#include <lwriscv/fence.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <lwriscv/gcc_attrs.h>
#include "gsp/gspifspdm.h"
#include "misc.h"
#include "flcnretval.h"
#include "cc_dma.h"

// TODO: Should move to HAL layer
FLCN_STATUS
confComputeSetupApertureCfg
(
    LwU32  regionId,
    LwU8   dmaIdx,
    LwU32  addrSpace
)
{
    FBIF_APERTURE_CFG ccApertCfg = {0};
    FBIF_TRANSCFG_TARGET  target;

    if (addrSpace == ADDR_SYSMEM)
    {
       target = FBIF_TRANSCFG_TARGET_COHERENT_SYSTEM;
    }
    else if (addrSpace == ADDR_FBMEM)
    {
       target = FBIF_TRANSCFG_TARGET_LOCAL_FB;
    }
    else
    {
        return LW_ERR_ILWALID_ARGUMENT;
    }

    ccApertCfg.apertureIdx             = dmaIdx;
    ccApertCfg.target                  = target;
    ccApertCfg.bTargetVa               = LW_FALSE;
    ccApertCfg.l2cWr                   = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL;
    ccApertCfg.l2cRd                   = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL;
    ccApertCfg.fbifTranscfWachk0Enable = LW_FALSE;
    ccApertCfg.fbifTranscfWachk1Enable = LW_FALSE;
    ccApertCfg.fbifTranscfRachk0Enable = LW_FALSE;
    ccApertCfg.fbifTranscfRachk1Enable = LW_FALSE;
    ccApertCfg.regionid                = (LwU8)regionId;

    if (fbifConfigureAperture(&ccApertCfg, 1) != LWRV_OK)
    {
        return FLCN_ERR_DMA_GENERIC;
    }

    return FLCN_OK;
}

// TODO: Should move to HAL layer
FLCN_STATUS
confComputeIssueDma
(
    void               *pOff,
    GCC_ATTR_UNUSED LwBool             bIsImem,
    LwU32              offset,
    LwU32              sizeInBytes,
    CC_DMA_DIRECTION   dmaDirection,
    GCC_ATTR_UNUSED CC_DMA_SYNC_TYPE   dmaSync,
    PCC_DMA_PROP       pDmaProp
)
{
    LwU32  dmaRet     = LWRV_ERR_GENERIC;
    LwBool bReadExt   = LW_FALSE;
    LwU64  tcmOffset  = (LwU64)pOff;

    //
    // [NOTE] : We have skipped the step of Programming the DMA base like it was done previously in
    // falcon architecture. The DMA functionality in RISCV requires the direct physical address.
    // So we add the offset (received as the parameter), and the wpr base for the respective DMA_PROP
    // object and then pass the result to the dmaPa function.
    //

    if (pOff == NULL || pDmaProp == NULL)
    {
        return  FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Check the required direction of DMA transaction
    switch (dmaDirection)
    {
        case CC_DMA_TO_FB:
        case CC_DMA_TO_FB_SCRUB:
        case CC_DMA_TO_SYS:
        case CC_DMA_TO_SYS_SCRUB:
            bReadExt = LW_FALSE;
            break;
        case CC_DMA_FROM_FB:
        case CC_DMA_FROM_SYS:
            bReadExt = LW_TRUE;
            break;
        default:
            return  FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Here the DMA operation is triggered. The system address(dmaAddr) that is present in the pDmaProp is a 256 Byte
    // aligned address. But the dmaPa function operates on the physical addresses, and thus needs that as
    // an argument.
    //
    dmaRet = dmaPa(tcmOffset, (pDmaProp->dmaAddr + offset), (LwU64)sizeInBytes, pDmaProp->dmaIdx, bReadExt);
    if (dmaRet != LWRV_OK)
    {
        return FLCN_ERR_DMA_GENERIC;
    }

    // Fence function is ilwoked to wait for all the external transactions to complete.
    riscvFenceRWIO();

    return FLCN_OK;
}
