/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef ENGINE_H
#define ENGINE_H

// Register headers
#include <dev_falcon_v4.h>
#include <dev_riscv_pri.h>

#if defined(ENGINE_gsp)
#include <dev_gsp.h>
#define ENGINE_PREFIX _PGSP
#define ENGINE_REG(X) LW_PGSP ## X
#define ENGINE_BASE LW_FALCON_GSP_BASE
#define RISCV_BASE  LW_FALCON2_GSP_BASE
// PMU defines
#elif defined(ENGINE_pmu)
#include <dev_pwr_pri.h>
#define ENGINE_PREFIX _PPWR
#define ENGINE_REG(X) LW_PPWR ## X
#define ENGINE_BASE LW_FALCON_PWR_BASE
#define RISCV_BASE  LW_FALCON2_PWR_BASE
#elif defined(ENGINE_sec)
#include <dev_sec_pri.h>
#define ENGINE_PREFIX _PSEC
#define ENGINE_REG(X) LW_PSEC ## X
#define ENGINE_BASE 0x840000
#define RISCV_BASE  LW_FALCON2_SEC_BASE
#else
#error "Define UPROC_ENGINE properly or add support for engine."
#endif

#endif // ENGINE_H
