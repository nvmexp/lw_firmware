/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acrtypes.h
 * @brief Main header file defining types that may be used in the ACR
 *        application. This file defines all types not already defined
 *        in lwtypes.h (from the SDK).
 */

#ifndef ACR_TYPES_H
#define ACR_TYPES_H

#include <falcon-intrinsics.h>
#include <lwtypes.h>
#include "rmlsfm.h"
#include "acr_status_codes.h"
#include "rmflcnbl.h"
#include "riscvifriscv.h"
#include "pmu/pmuifcmn.h"
#include "pmu/pmuifpmu.h"
#include "dpu/dpuifdpu.h"
#include "sec2/sec2ifsec2.h"

// Constant defines
#define ACR_FALCON_BLOCK_SIZE_IN_BYTES (256)
#define ACR_AES128_KEY_SIZE_IN_BYTES   (16)
#define ACR_SCP_BLOCK_SIZE_IN_BYTES    (16)
#define ACR_AES128_SIG_SIZE_IN_BYTES   ACR_AES128_KEY_SIZE_IN_BYTES
#define ACR_AES128_DMH_SIZE_IN_BYTES   ACR_AES128_KEY_SIZE_IN_BYTES

#define ACR_TARGET_ENGINE_CORE_FALCON    (0)
#define ACR_TARGET_ENGINE_CORE_RISCV     (1)
#define ACR_TARGET_ENGINE_CORE_RISCV_EB  (2)

typedef struct _def_flcn_config
{
    LwU32  falconId;
    LwU32  falconInstance;
    LwU32  pmcEnableMask;
    LwU32  registerBase;
    LwU32  fbifBase;
    LwU32  riscvRegisterBase;
    LwU32  falcon2RegisterBase;
    LwU32  range0Addr;
    LwU32  range1Addr;
    LwU32  regCfgAddr;
    LwU32  scpP2pCtl;
    LwU32  regCfgMaskAddr;
    LwU32  ctxDma;
    LwU32  regSelwreResetAddr;
    LwU32  imemPLM;
    LwU32  dmemPLM;
    LwU32  subWprRangeAddrBase;
    LwU32  subWprRangePlmAddr;
    LwU32  maxSubWprSupportedByHw;
    // bug 2085538
    // From Ampere, LW_PMC_ENABLE is getting deprecated. We need use LW_PMC_DEVICE_ENABLE(idx) to enable/disable LS falcon.
    // Example to enable device: REG_WR32(LW_PMC_DEVICE_ENABLE(N/32), REG_RD32(LW_PMC_DEVICE_ENABLE(N/32)) | (1 << (N % 32)))
    // pmcEnableIdx is used to save register index(N/32) for LW_PMC_DEVICE_ENABLE() group.
    LwU32  pmcEnableRegIdx;
    LwBool bOpenCarve;
    LwBool bFbifPresent;
    LwBool bIsBoOwner;
    LwBool bStackCfgSupported;
    LwU8   uprocType;
    LwU32  targetMaskIndex;
} ACR_FLCN_CONFIG, *PACR_FLCN_CONFIG;

typedef union _def_acr_timestamp
{
    LwU64 time;
    struct
    {
        LwU32 lo;   //!< Low 32-bits  (Must be first!!)
        LwU32 hi;   //!< High 32-bits (Must be second!!)
    } parts;
}ACR_TIMESTAMP, *PACR_TIMESTAMP;

typedef struct _def_acr_lsb_hdr_wrap
{
    LSF_LSB_HEADER hdr;
    LwU32          wprHdrOff;  // Offset of wprHeader in g_wprHeader
}ACR_LSB_HDR_WRAP, *PACR_LSB_HDR_WRAP;

typedef struct _def_acr_dma_prop
{
    LwU64 wprBase;
    LwU32 ctxDma;
    LwU32 regionID;
}ACR_DMA_PROP, *PACR_DMA_PROP;

typedef enum _def_bar0_flcn_tgt
{
    BAR0_FLCN,
    BAR0_FBIF,
    BAR0_RISCV,
    CSB_FLCN,
    ILWALID_TGT
}FLCN_REG_TGT, *PFLCN_REG_TGT;

typedef enum _def_acr_dma_direction
{
    ACR_DMA_TO_FB,
    ACR_DMA_FROM_FB,
    ACR_DMA_TO_FB_SCRUB  // Indicates to DMA code that source is just 256 bytes and it needs to be reused for all DMA instructions
}ACR_DMA_DIRECTION;

typedef enum _def_acr_dma_sync_type
{
    ACR_DMA_SYNC_NO,
    ACR_DMA_SYNC_AT_END,
    ACR_DMA_SYNC_ALL
}ACR_DMA_SYNC_TYPE;

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
#ifdef NEW_WPR_BLOBS    
    REG_LABEL_FLCN_BOOTVEC_PRIV_LEVEL_MASK,
    REG_LABEL_FLCN_DMA_PRIV_LEVEL_MASK,
#endif
    REG_LABEL_FLCN_END,
#ifdef ACR_RISCV_LS
    REG_LABEL_RISCV_BOOT_VECTOR_LO,
    REG_LABEL_RISCV_BOOT_VECTOR_HI,
    REG_LABEL_RISCV_CPUCTL,
    REG_LABEL_RISCV_END,
#endif
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
} ACR_REG_MAPPING, *PACR_REG_MAPPING;

#define ACR_SCRUB_ZERO_BUF_SIZE_BYTES    (0x100)
#define ACR_SCRUB_ZERO_BUF_ALIGN         (0x100)
typedef struct _def_acr_scrub_state
{
    LwU8  zeroBuf[ACR_SCRUB_ZERO_BUF_SIZE_BYTES];
    LwU32 scrubTrack;
} ACR_SCRUB_STATE, PACR_SCRUB_STATE;

#define ACR_FLCN_BL_CMDLINE_ALIGN        (ACR_FALCON_BLOCK_SIZE_IN_BYTES)
#define ACR_FLCN_BL_RESERVED_ASM         (0x02f80000)
typedef union _def_acr_flcn_bl_cmdline_args
{
    LOADER_CONFIG         legacyBlArgs;
    RM_FLCN_BL_DMEM_DESC  genericBlArgs;
} ACR_FLCN_BL_CMDLINE_ARGS;

//
// TODO: There is pending item to port all this into a selwrebuslib.
//       Once that is complete this will be removed (Bug 1732094)
//
typedef enum _def_selwrebus_target
{
    SEC2_SELWREBUS_TARGET_HUB,
    SEC2_SELWREBUS_TARGET_SE,
    PMU_SELWREBUS_TARGET_SE
} ACR_SELWREBUS_TARGET;

//
// LS overlay group signature data structures and defines.
// At present we have room only for one group and hence restricting to GRP ID 1
//

//
// Three MAGIC DWORDS that will confirm the presence of LS SIG Group.
// This is temporary solution and will soon be replaced with an entry in LSB header.
//
#define ACR_LS_SIG_GROUP_MAGIC_PATTERN_1 0xED300700
#define ACR_LS_SIG_GROUP_MAGIC_PATTERN_2 0xEE703000
#define ACR_LS_SIG_GROUP_MAGIC_PATTERN_3 0xCE300300

// This can be increased if number of LS sig group entries goes beyond 20.
#define ACR_LS_SIG_GRP_LOAD_BUF_SIZE   (ACR_FALCON_BLOCK_SIZE_IN_BYTES)

// This data structure defines each entry present in LS Sig group block
typedef struct _def_acr_ls_sig_grp_entry
{
    LwU32 startPc;
    LwU32 endPc;
    LwU32 grpId;
} ACR_LS_SIG_GRP_ENTRY;

// List of all group Ids supported
enum _enum_ls_sig_grp_ids
{
    ENUM_LS_SIG_GRP_ID_ILWALID        = 0x0,
    ENUM_LS_SIG_GRP_ID_1              = 0x1,
    ENUM_LS_SIG_GRP_ID_TOTAL          = 0x1,
    ENUM_LS_SIG_GRP_ID_ALL            = 0xFF,
};

// This is the header at the start of LS Sig group
typedef struct _def_acr_ls_sig_grp_header
{
    LwU32                magicPattern1;
    LwU32                magicPattern2;
    LwU32                magicPattern3;
    LwU32                numLsSigGrps;
} ACR_LS_SIG_GRP_HEADER;

#endif // ACR_TYPES_H
