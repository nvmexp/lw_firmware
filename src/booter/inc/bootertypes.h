/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: bootertypes.h
 * @brief Main header file defining types that may be used in the Booter
 *        application. This file defines all types not already defined
 *        in lwtypes.h (from the SDK).
 */

#ifndef BOOTER_TYPES_H
#define BOOTER_TYPES_H

#include <falcon-intrinsics.h>
#include <lwtypes.h>

// Constant defines
#define BOOTER_FALCON_BLOCK_SIZE_IN_BYTES (256)

typedef union _def_booter_timestamp
{
    LwU64 time;
    struct
    {
        LwU32 lo;   //!< Low 32-bits  (Must be first!!)
        LwU32 hi;   //!< High 32-bits (Must be second!!)
    } parts;
}BOOTER_TIMESTAMP, *PBOOTER_TIMESTAMP;

typedef enum _def_selwrebus_target
{
    SEC2_SELWREBUS_TARGET_SE,
    LWDEC_SELWREBUS_TARGET_SE,
} BOOTER_SELWREBUS_TARGET;

typedef struct _def_booter_dma_prop
{
    LwU64 baseAddr;
    LwU32 ctxDma;
    LwU32 regionID;
}BOOTER_DMA_PROP, *PBOOTER_DMA_PROP;

typedef enum _def_booter_dma_direction
{
    BOOTER_DMA_TO_FB,
    BOOTER_DMA_FROM_FB,
    BOOTER_DMA_TO_FB_SCRUB  // Indicates to DMA code that source is just 256 bytes and it needs to be reused for all DMA instructions
}BOOTER_DMA_DIRECTION;

typedef enum _def_booter_dma_sync_type
{
    BOOTER_DMA_SYNC_NO,
    BOOTER_DMA_SYNC_AT_END,
    BOOTER_DMA_SYNC_ALL
}BOOTER_DMA_SYNC_TYPE;

typedef enum _def_booter_dma_context
{
    BOOTER_DMA_FB_NON_WPR_CONTEXT,
    BOOTER_DMA_FB_WPR_CONTEXT,
    BOOTER_DMA_SYSMEM_CONTEXT,
    BOOTER_DMA_CONTEXT_COUNT
}BOOTER_DMA_CONTEXT;

#endif // BOOTER_TYPES_H
