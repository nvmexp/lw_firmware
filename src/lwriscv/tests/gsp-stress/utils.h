/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef UTILS_H
#define UTILS_H

#include <lwtypes.h>
#include <liblwriscv/io.h>

#define MPU_ATTR_CACHE ((1 << 18) | (1 < 19))
#define MPU_ATTR_RW (0x3 | (0x3 << 3))
#define MPU_ATTR_RWX (0x7 | (0x7 << 3))
#define MPU_ATTR_RX (0x5 | (0x5 << 3))

extern LwU64 g_beginOfImemFreeSpace;
extern LwU64 __data_fb_buffer;
extern LwU64 __imem_test_buffer_start;

void appEnableRiscvPerf(void);
void appRunFromFb(void);
void appRunFromImem(void);

void app_icd(void);
void app_exception_entry(void);

void mToS(void);
void sToM(void);

void releasePriLockdown(void);

void changeFbCachable(LwBool bEnable);
void appShutdown(void);

#define ICD_HALT do {\
    localWrite(LW_PRGNLCL_RISCV_ICD_CMD, LW_PRGNLCL_RISCV_ICD_CMD_OPC_STOP);\
    __asm__ __volatile__("fence");\
} while (0);

#endif // UTILS_H
