/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr.h
 */

#ifndef PMU_ACR_H
#define PMU_ACR_H

#ifdef UPROC_FALCON
#include <falcon-intrinsics.h>
#endif
#include "acr/pmu_acrtypes.h"
#include "rmlsfm.h"
#include "acr_status_codes.h"

#include "lwuproc.h"

// PRIV_LEVEL mask defines
#define ACR_PLMASK_READ_L2_WRITE_L2       0xcc
#define ACR_PLMASK_READ_L0_WRITE_L2       0xcf
#define ACR_PLMASK_READ_L0_WRITE_L0       0xff
#define ACR_PLMASK_READ_L0_WRITE_L3       0x8f
#define ACR_PLMASK_READ_L3_WRITE_L3       0x88

#define DMEM_APERTURE_OFFSET            BIT(28)

// Function prototypes
LwU32 falc_ldxb_i_with_halt(LwU32)
    GCC_ATTRIB_SECTION("imem_acr", "falc_ldxb_i_with_halt");
void falc_stxb_i_with_halt(LwU32, LwU32)
    GCC_ATTRIB_SECTION("imem_acr", "falc_stxb_i_with_halt");

#endif // PMU_ACR_H

