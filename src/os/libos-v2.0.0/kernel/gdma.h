/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_GDMA_H
#define LIBOS_GDMA_H

#include "kernel.h"

void                LIBOS_SECTION_TEXT_COLD kernel_gdma_load(void);
void                LIBOS_SECTION_TEXT_COLD kernel_gdma_flush(void);
void                LIBOS_SECTION_TEXT_COLD kernel_gdma_xfer_reg(LwU64 src_pa, LwU64 dst_pa, LwU32 length, LwU8 chan_idx, LwU8 sub_chan_idx);
void LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD kernel_syscall_gdma_register_tcm(LwU64 va, LwU32 size);
void LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD kernel_syscall_gdma_copy(LwU64 dst_va, LwU64 src_va, LwU32 size);
void LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD kernel_syscall_gdma_flush(void);

#endif
