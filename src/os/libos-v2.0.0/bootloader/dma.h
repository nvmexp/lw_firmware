/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMA_H
#define DMA_H

#include "lwtypes.h"

#define DMAIDX_PHYS_VID_FN0     1
#define DMAIDX_PHYS_SYS_COH_FN0 2

#define DMAFLAG_READ  0
#define DMAFLAG_WRITE 1

void dma_init(void);
void dma_copy(LwU64 phys_addr, LwU32 tcm_offset, LwU32 length, LwU32 dma_idx, LwU32 dma_flag);
void dma_flush(void);

#endif // DMA_H
