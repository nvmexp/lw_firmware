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
 * @file: pmu_acrtypes.h
 * @brief Primary header file defining types that may be used in the ACR
 *        application. This file defines all types not already defined
 *        in lwtypes.h (from the SDK).
 */

#ifndef ACR_TYPES_H
#define ACR_TYPES_H

#ifdef UPROC_FALCON
#include <falcon-intrinsics.h>
#endif
#include <lwtypes.h>
#include "rmlsfm.h"
#include "acr_status_codes.h"
#include "rmflcnbl.h"
#include "pmu/pmuifcmn.h"
#include "pmu/pmuifpmu.h"
#include "dpu/dpuifdpu.h"
#include "sec2/sec2ifsec2.h"

typedef struct _def_flcn_config
{
    LwU32  falconId;
    LwU32  falconInstance;
    LwU32  pmcEnableMask;
    LwU32  registerBase;
    LwU32  fbifBase;
    LwU32  range0Addr;
    LwU32  range1Addr;
    LwU32  regCfgAddr;
    LwU32  regCfgMaskAddr;
    LwU32  ctxDma;
    LwU32  regSelwreResetAddr;
    LwU32  imemPLM;
    LwU32  dmemPLM;
    LwU32  subWprRangeAddrBase;
    LwU32  subWprRangePlmAddr;
    LwU32  maxSubWprSupportedByHw;
    LwBool bOpenCarve;
    LwBool bFbifPresent;
    LwBool bIsBoOwner;
    LwBool bStackCfgSupported;
    // bug 2085538
    // From Ampere, LW_PMC_ENABLE is getting deprecated. We need use LW_PMC_DEVICE_ENABLE(idx) to enable/disable LS falcon.
    // Example to enable device: REG_WR32(LW_PMC_DEVICE_ENABLE(N/32), REG_RD32(LW_PMC_DEVICE_ENABLE(N/32)) | (1 << (N % 32)))
    // pmcEnableIdx is used to save register index(N/32) for LW_PMC_DEVICE_ENABLE() group.
    LwU32  pmcEnableRegIdx;
} ACR_FLCN_CONFIG;

typedef enum _def_bar0_flcn_tgt
{
    BAR0_FLCN,
    BAR0_FBIF,
    CSB_FLCN,
    ILWALID_TGT
} FLCN_REG_TGT;

typedef enum _def_reg_label
{
    REG_LABEL_FLCN_SCTL_PRIV_LEVEL_MASK,
    REG_LABEL_FLCN_SCTL,
    REG_LABEL_FLCN_CPUCTL,
    REG_LABEL_FLCN_DMACTL,
    REG_LABEL_FLCN_DMATRFBASE,
    REG_LABEL_FLCN_DMATRFCMD,
    REG_LABEL_FLCN_DMATRFMOFFS,
    REG_LABEL_FLCN_DMATRFFBOFFS,
    REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK,
    REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK,
    REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK,
    REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK,
    REG_LABEL_FLCN_IRQTMR_PRIV_LEVEL_MASK,
    REG_LABEL_FLCN_MTHDCTX_PRIV_LEVEL_MASK,
    REG_LABEL_FLCN_HWCFG,
    REG_LABEL_FLCN_BOOTVEC,
    REG_LABEL_FLCN_DBGCTL,
    REG_LABEL_FLCN_SCPCTL,
    REG_LABEL_FLCN_END,
    REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK,
    REG_LABEL_FBIF_REGIONCFG,
    REG_LABEL_FBIF_CTL,
    REG_LABEL_FBIF_END,
    REG_LABEL_END
} ACR_FLCN_REG_LABEL;

typedef struct _def_acr_reg_mapping
{
    LwU32 bar0Off;
    LwU32 pwrCsbOff;
} ACR_REG_MAPPING;

#endif // ACR_TYPES_H
