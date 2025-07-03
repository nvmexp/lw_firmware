/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __ASM_REGISTER_H_
#define __ASM_REGISTER_H_

#include "riscv_manuals.h"

#define LW_RISCV_CSR_MSTATUS_SIE__SHIFT         1
#define LW_RISCV_CSR_MSTATUS_SIE__SHIFTMASK     (1 << LW_RISCV_CSR_MSTATUS_SIE__SHIFT)

#define LW_RISCV_CSR_MSTATUS_SPIE__SHIFT        5
#define LW_RISCV_CSR_MSTATUS_SPIE__SHIFTMASK    (1 << LW_RISCV_CSR_MSTATUS_SPIE__SHIFT)

#define LW_RISCV_CSR_MSTATUS_SPP__SHIFT         8
#define LW_RISCV_CSR_MSTATUS_SPP__SHIFTMASK     (1 << LW_RISCV_CSR_MSTATUS_SPP__SHIFT)

#define LW_RISCV_CSR_MSTATUS_MPP__SHIFT         11
#define LW_RISCV_CSR_MSTATUS_MPP__SHIFTMASK     (3 << LW_RISCV_CSR_MSTATUS_MPP__SHIFT)
#define LW_RISCV_CSR_MSTATUS_MPP__SHIFTMASK_B0  (1 << LW_RISCV_CSR_MSTATUS_MPP__SHIFT)
#define LW_RISCV_CSR_MSTATUS_MPP__SHIFTMASK_B1  (2 << LW_RISCV_CSR_MSTATUS_MPP__SHIFT)

#define LW_RISCV_CSR_MIP_STIP__SHIFT            5
#define LW_RISCV_CSR_MIP_STIP__SHIFTMASK        (1 << LW_RISCV_CSR_MIP_STIP__SHIFT)

#define LW_RISCV_CSR_MIE_MTIE__SHIFT            7
#define LW_RISCV_CSR_MIE_MTIE__SHIFTMASK        (1 << LW_RISCV_CSR_MIE_MTIE__SHIFT)

#define LW_RISCV_CSR_MBPCFG_ALL_FLUSH           0x3B0000

#if !__ASSEMBLER__
#include <lwctassert.h>

// Compile-time asserts to verify the above defines are correct
ct_assert(LW_RISCV_CSR_MSTATUS_SIE__SHIFTMASK == DRF_NUM64(_RISCV_CSR, _MSTATUS, _SIE, 1));
ct_assert(LW_RISCV_CSR_MSTATUS_SPIE__SHIFTMASK == DRF_NUM64(_RISCV_CSR, _MSTATUS, _SPIE, 1));
ct_assert(LW_RISCV_CSR_MSTATUS_SPP__SHIFTMASK == DRF_NUM64(_RISCV_CSR, _MSTATUS, _SPP, 1));
ct_assert(LW_RISCV_CSR_MSTATUS_MPP__SHIFTMASK == DRF_NUM64(_RISCV_CSR, _MSTATUS, _MPP, 3));
ct_assert(LW_RISCV_CSR_MIP_STIP__SHIFTMASK == DRF_NUM64(_RISCV_CSR, _MIP, _STIP, 1));
ct_assert(LW_RISCV_CSR_MIE_MTIE__SHIFTMASK == DRF_NUM64(_RISCV_CSR, _MIE, _MTIE, 1));
ct_assert(LW_RISCV_CSR_MBPCFG_ALL_FLUSH == (DRF_DEF64(_RISCV_CSR, _MBPCFG, _BTB_FLUSHU, _TRUE) |
                                            DRF_DEF64(_RISCV_CSR, _MBPCFG, _BTB_FLUSHS, _TRUE) |
                                            DRF_DEF64(_RISCV_CSR, _MBPCFG, _BTB_FLUSHM, _TRUE) |
                                            DRF_DEF64(_RISCV_CSR, _MBPCFG, _BHT_FLUSH, _TRUE) |
                                            DRF_DEF64(_RISCV_CSR, _MBPCFG, _RAS_FLUSH, _TRUE)));

#endif // !__ASSEMBLER__

#endif // __SEPKERN_H_
