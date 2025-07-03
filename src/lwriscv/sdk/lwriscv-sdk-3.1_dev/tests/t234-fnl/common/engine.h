/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef ENGINE_H
#define ENGINE_H

#if defined(ENGINE_gsp)
#include <ga10b/dev_gsp_prgnlcl.h>
#include <ga10b/lw_gsp_riscv_address_map.h>
#define ENGINE_BASE LW_FALCON_GSP_BASE
#define RISCV_BASE  LW_FALCON2_GSP_BASE
// PMU defines
#elif defined(ENGINE_pmu)
#include <ga10b/dev_pmu_prgnlcl.h>
#include <ga10b/lw_pmu_riscv_address_map.h>
#define ENGINE_BASE LW_FALCON_PWR_BASE
#define RISCV_BASE  LW_FALCON2_PWR_BASE
#elif defined(ENGINE_sec)
#include <t234/dev_sec_prgnlcl.h>
#include <t234/lw_sec_riscv_address_map.h>
#define ENGINE_BASE LW_FALCON_SEC_BASE
#define RISCV_BASE  LW_FALCON2_SEC_BASE
#elif defined(ENGINE_lwdec)
#include <t234/dev_lwdec_prgnlcl.h>
#include <t234/lw_lwdec_riscv_address_map.h>
#define ENGINE_BASE LW_FALCON_LWDEC_BASE
#define RISCV_BASE  LW_FALCON2_LWDEC_BASE
#else
#error "Define UPROC_ENGINE properly or add support for engine."
#endif

#define ENGINE_PREFIX _PRGNLCL
#define ENGINE_REG(X) LW_PRGNLCL ## X


#endif // ENGINE_H
