/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: rv_acr.h
 */

#ifndef RISCV_TEGRA_ACR_H
#define RISCV_TEGRA_ACR_H

/* Common headers */
#include "acrutils.h"
#include "acrtypes.h"
#include "external/rmlsfm.h"
#include "acr_status_codes.h"

/* Source specific headers */
#include "acr_rv_wprt234.h"
#include "acr_rv_dmat234.h"
#include "acr_rv_boott234.h"
#include "acr_rv_falct234.h"
#include "acruc_rv_libt234.h"
#include "acr_rv_sect234.h"
#include "acr_rv_verift234.h"

/* Current boostrap owner */
#ifndef ACR_SAFE_BUILD
#define ACR_LSF_LWRRENT_BOOTSTRAP_OWNER   LSF_BOOTSTRAP_OWNER_PMU_RISCV
#else
#define ACR_LSF_LWRRENT_BOOTSTRAP_OWNER   LSF_BOOTSTRAP_OWNER_GSP_RISCV
#endif

/* PRIV_LEVEL mask defines */
/*
 * These defines will grant access to all PRI sources having required PRIV level authorization.
 * POR PLM values registers are at //hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_PRI_source_isolation_FD.docx
 */
#define ACR_PLMASK_READ_L2_WRITE_L2       0xffffffclw
#define ACR_PLMASK_READ_L0_WRITE_L2       0xffffffcfU
#define ACR_PLMASK_READ_L0_WRITE_L0       0xffffffffU
#define ACR_PLMASK_READ_L0_WRITE_L3       0xffffff8fU
#define ACR_PLMASK_READ_L3_WRITE_L3       0xffffff88U

/* SUB-WPR mask defines */
#define ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED    0xF
#define ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED    0xF
#define ACR_SUB_WPR_MMU_RMASK_L2_AND_L3             0xC
#define ACR_SUB_WPR_MMU_WMASK_L2_AND_L3             0xC
#define ACR_SUB_WPR_MMU_RMASK_L3                    0x8
#define ACR_SUB_WPR_MMU_WMASK_L3                    0x8
#define ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_DISABLED   0x0
#define ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED   0x0
#define ACR_UNLOCK_READ_MASK      (0x0)
#define ACR_UNLOCK_WRITE_MASK     (0x0)

/* Additional defines to ease the programming of SubWpr MMU registers */
#define FALCON_SUB_WPR_INDEX_REGISTER_STRIDE            (LW_PFB_PRI_MMU_FALCON_PMU_CFGA(1) - LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0))
#define FALCON_SUB_WPR_CONFIG_REGISTER_OFFSET_DIFF      (LW_PFB_PRI_MMU_FALCON_PMU_CFGB(0) - LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0))

/* Alignment defines */
#define ACR_DESC_ALIGN   (16)
#define ACR_REGION_ALIGN (0x20000U)

/* Timeout defines */
/* TODO: Check if the timout val matches betwen falcon and riscv */
#define ACR_DEFAULT_TIMEOUT_NS            (100*1000*1000)  // 100 ms

/* Common defines */
#define FLCN_IMEM_BLK_SIZE_IN_BYTES       (256U)
#define FLCN_DMEM_BLK_SIZE_IN_BYTES       (256U)
#define FLCN_IMEM_BLK_ALIGN_BITS          (8U)

/* Validation defines */
#define IS_FALCONID_ILWALID(id, off) (((id) == LSF_FALCON_ID_ILWALID) || ((id) >= LSF_FALCON_ID_END) || ((off) == 0U))

/* Size defines */
#define DMA_SIZE          0x400U

/* Signature indexes */
#define ACR_LS_VERIF_SIGNATURE_CODE_INDEX (LSF_UCODE_COMPONENT_INDEX_CODE)
#define ACR_LS_VERIF_SIGNATURE_DATA_INDEX (LSF_UCODE_COMPONENT_INDEX_DATA)

/* Extern declarations */
extern RISCV_ACR_DESC  g_desc;
extern LwBool          g_bIsDebug;
extern ACR_DMA_PROP    g_dmaProp;
extern LSF_WPR_HEADER  g_pWprHeader[LSF_WPR_HEADERS_TOTAL_SIZE_MAX/sizeof(LSF_WPR_HEADER)];
extern ACR_SCRUB_STATE g_scrubState;

#endif // RISCV_TEGRA_ACR_H
