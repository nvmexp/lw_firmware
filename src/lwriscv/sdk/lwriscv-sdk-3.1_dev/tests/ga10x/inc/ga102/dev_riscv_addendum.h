/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __ga102_dev_riscv_addendum_h__
#define __ga102_dev_riscv_addendum_h__

#define LW_RISCV_PA_IROM_BASE                                   0x0000000000080000
#define LW_RISCV_PA_IROM_SIZE                                   0x0000000000080000
#define LW_RISCV_PA_ITCM_BASE                                   0x0000000000100000
#define LW_RISCV_PA_ITCM_SIZE                                   0x0000000000080000
#define LW_RISCV_PA_DTCM_BASE                                   0x0000000000180000
#define LW_RISCV_PA_DTCM_SIZE                                   0x0000000000080000
#define LW_RISCV_PA_EMEM_BASE                                   0x0000000001200000
#define LW_RISCV_PA_EMEM_SIZE                                   0x0000000000080000

#define LW_RISCV_PA_PRIV_BASE                                   0x2000000000000000
#define LW_RISCV_PA_PRIV_SIZE                                   0x0000000100000000
#define LW_RISCV_PA_FBGPA_BASE                                  0x6060000000000000
#define LW_RISCV_PA_FBGPA_SIZE                                  0x0020000000000000
#define LW_RISCV_PA_SYSGPA_BASE                                 0x8060000000000000
#define LW_RISCV_PA_SYSGPA_SIZE                                 0x0020000000000000
#define LW_RISCV_PA_GVA_BASE                                    0xA000000000000000
#define LW_RISCV_PA_GVA_SIZE                                    0x0002000000000000

#endif // __ga102_dev_riscv_addendum_h__
