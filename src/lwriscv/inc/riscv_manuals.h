/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef RISCV_MANUALS_H
#define RISCV_MANUALS_H

/*!
 * @file    riscv_manuals.h
 */

// Include RISCV engine headers
#if defined(GSP_RTOS)
#ifdef RUN_ON_SEC
#include "lw_sec_riscv_address_map.h"
#include "dev_sec_riscv_csr_64.h"
#include "dev_sec_prgnlcl.h"
#if USE_CSB
#include "dev_sec_csb.h"
#else // USE_CSB
#include "dev_sec_pri.h"
#endif // USE_CSB
#else // RUN_ON_SEC
#include "lw_gsp_riscv_address_map.h"
#include "dev_gsp_riscv_csr_64.h"
#include "dev_gsp_prgnlcl.h"
#if USE_CSB
#include "dev_gsp_csb.h"
#else // USE_CSB
#include "dev_gsp.h"
#endif // USE_CSB
#endif // RUN_ON_SEC

#elif defined(PMU_RTOS)
#include "lw_pmu_riscv_address_map.h"
#include "dev_pmu_riscv_csr_64.h"
#include "dev_pmu_prgnlcl.h"
#if USE_CSB
#include "dev_pwr_csb.h"
#else // USE_CSB
#include "dev_pwr_pri.h"
#endif // USE_CSB

#elif defined(SOE_RTOS)
#include "lw_soe_riscv_address_map.h"
#include "dev_soe_riscv_csr_64.h"
#include "dev_soe_prgnlcl.h"
#if USE_CSB
#include "dev_soe_csb.h"
#endif // USE_CSB

#elif defined(SEC2_RTOS)
#include "lw_sec_riscv_address_map.h"
#include "dev_sec_riscv_csr_64.h"
#include "dev_sec_prgnlcl.h"
#if USE_CSB
#include "dev_sec_csb.h"
#else // USE_CSB
#include "dev_sec_pri.h"
#endif // USE_CSB
#else // defined(GSP_RTOS), defined(PMU_RTOS), defined(SEC2_RTOS), defined(SOE_RTOS)
#error "Profile not supported."

#endif // defined(GSP_RTOS), defined(PMU_RTOS), defined(SEC2_RTOS), defined(SOE_RTOS)

#endif // RISCV_MANUALS_H
