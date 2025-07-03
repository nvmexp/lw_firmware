/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_REGS_H
#define DMVA_REGS_H

#include "dev_pwr_csb.h"
#include "dev_sec_csb.h"
#include "lw_ref_dev_lwdec_csb.h"
#include "dev_sec_pri.h"
#include "dev_pwr_pri.h"
#include "dev_lwdec_pri.h"

#include "dev_falcon_v4.h"
#include "dev_fbif_v4.h"

#include <falcon-intrinsics.h>

// TODO: rename with _
#define DMVACSBWR(addr, val) falc_stxb((LwU32*)(addr), (val))

#define DMVACSBRD(addr) falc_ldxb((LwU32*)(addr), 0)

#define DMVAREGWR(reg, val) DMVACSBWR(CSBADDR(reg), (val))

#define DMVAREGRD(reg) DMVACSBRD(CSBADDR(reg))

#ifdef DMVA_FALCON_PMU
#define PREFIX PWR
#elif defined DMVA_FALCON_SEC2
#define PREFIX SEC
#elif defined DMVA_FALCON_GSP
#define PREFIX GSP
// Add here because GSP doesn't exist for pascal.
#include "dev_gsp_csb.h"
#include "dev_gsp.h"
#elif defined DMVA_FALCON_LWDEC
#define PREFIX LWDEC
#else
#error unknown or unsupported falcon
#endif

#define REGS_CAT_I(a, b) a##b
#define REGS_CAT(a, b) REGS_CAT_I(a, b)

#define CSBADDR(reg) ((LwU32*)REGS_CAT(LW_C, REGS_CAT(PREFIX, REGS_CAT(_FALCON_, reg))))
#define PRIVADDR(reg) (REGS_CAT(LW_P, REGS_CAT(PREFIX, REGS_CAT(_FALCON_, reg))))

#endif
