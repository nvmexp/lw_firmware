/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef FMC_H
#define FMC_H
#include <stdint.h>
#include <lwtypes.h>
#include <lwmisc.h>

void fmc_icd(void);
void fmc_halt(void);
void fmc_trap_handler(void);

#define APP_ENTRY_POINT 0xA00000
#define APP_DATA_VA     0xB00000

extern LwU32 __fmc_size;
extern LwU32 __fmc_carveout_size;

#endif // FMC_H
