/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef _CC_DMA_H_
#define _CC_DMA_H_

#define LW_RISCVLIB_DMA_ALIGNMENT   (4)

typedef struct _def_cc_dma_prop
{
    LwU8  dmaIdx;

    LwU64 dmaAddr;

    LwU32 regionId;

    LwU32 addrSpace;

} CC_DMA_PROP, *PCC_DMA_PROP;

typedef enum _def_cc_dma_sync_type
{
    CC_DMA_SYNC_NO,
    CC_DMA_SYNC_AT_END,
    CC_DMA_SYNC_ALL
} CC_DMA_SYNC_TYPE;

typedef enum _def_dma_direction
{
    CC_DMA_TO_FB,
    CC_DMA_FROM_FB,
    CC_DMA_TO_FB_SCRUB,  // Indicates to DMA code that source is just 256 bytes and it needs to be reused for all DMA instructions
    CC_DMA_TO_SYS,
    CC_DMA_FROM_SYS,
    CC_DMA_TO_SYS_SCRUB,  // Indicates to DMA code that source is just 256 bytes and it needs to be reused for all DMA instructions
} CC_DMA_DIRECTION;

// TODO : align drivers/resman/kernel/inc/memdesc.h
#define ADDR_UNKNOWN    0         // Address space is unknown
#define ADDR_SYSMEM     1         // System memory (PCI)
#define ADDR_FBMEM      2         // Frame buffer memory space
#define ADDR_REGMEM     3         // LW register memory space
#define ADDR_VIRTUAL    4         // Virtual address space only
#define ADDR_FABRIC     5         // Multi-node fabric address space for the GPA based IMEX
#define ADDR_FABRIC_V2  6         // Multi-node fabric address space for the FLA based IMEX. Will replace ADDR_FABRIC.

extern FLCN_STATUS
confComputeIssueDma
(
    void               *pOff,
    LwBool             bIsImem,
    LwU32              offset,
    LwU32              sizeInBytes,
    CC_DMA_DIRECTION   dmaDirection,
    CC_DMA_SYNC_TYPE   dmaSync,
    PCC_DMA_PROP       pDmaProp
);

extern FLCN_STATUS
confComputeSetupApertureCfg
(
    LwU32  regionId,
    LwU8   dmaIdx,
    LwU32  addrSpace
);
#endif
