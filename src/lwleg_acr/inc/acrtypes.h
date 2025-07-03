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
 * @brief Master header file defining types that may be used in the ACR
 *        application. This file defines all types not already defined
 *        in lwtypes.h (from the SDK).
 */

#ifndef TEGRA_ACR_TYPES_H
#define TEGRA_ACR_TYPES_H

#ifndef LW_RISCV_ACR
# ifndef UPROC_RISCV
#  include <falcon-intrinsics.h>
# endif // UPROC_RISCV
# include <lwtypes.h>
#endif // LW_RISCV_ACR

#include "acr_status_codes.h"

#ifdef PMU_RTOS
// When building as PMU submake, we want to use the "real" versions
// of the headers to avoid conflicts.
# include "rmlsfm.h"
# include "rmflcnbl.h"
# include "pmu/pmuifcmn.h"
#else // PMU_RTOS
# include "external/rmlsfm.h"
# include "external/rmflcnbl.h"
# include "external/pmuifcmn.h"
#endif // PMU_RTOS

// Constant defines
#define ACR_FALCON_BLOCK_SIZE_IN_BYTES (256U)
#define ACR_AES128_KEY_SIZE_IN_BYTES   (16U)
#define ACR_SCP_BLOCK_SIZE_IN_BYTES    (16U)
#define ACR_AES128_SIG_SIZE_IN_BYTES   ACR_AES128_KEY_SIZE_IN_BYTES
#define ACR_AES128_DMH_SIZE_IN_BYTES   ACR_AES128_KEY_SIZE_IN_BYTES

// Array defines
// This helps in indexing a regmap using enum literals
#define REG_MAP_INDEX(arr,i)               arr[(LwU32)(i)]

typedef struct def_flcn_config
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
    LwBool bOpenCarve;
    LwBool bFbifPresent;
    LwBool bIsBoOwner;
    LwBool bStackCfgSupported;
    LwU32  targetMaskIndex;
} ACR_FLCN_CONFIG, *PACR_FLCN_CONFIG;

typedef union def_acr_timestamp
{
    LwU64 lw_time;
    struct
    {
        LwU32 lo;   //!< Low 32-bits  (Must be first!!)
        LwU32 hi;   //!< High 32-bits (Must be second!!)
    } parts;
}ACR_TIMESTAMP, *PACR_TIMESTAMP;

typedef struct def_acr_lsb_hdr_wrap
{
    LSF_LSB_HEADER hdr;
    LwU32          wprHdrOff;  // Offset of wprHeader in g_wprHeader
}ACR_LSB_HDR_WRAP, *PACR_LSB_HDR_WRAP;

typedef struct def_acr_dma_prop
{
    LwU64 wprBase;
    LwU32 ctxDma;
    LwU32 regionID;
}ACR_DMA_PROP, *PACR_DMA_PROP;

typedef enum def_bar0_flcn_tgt
{
    BAR0_FLCN,
    BAR0_FBIF,
    CSB_FLCN
}FLCN_REG_TGT, *PFLCN_REG_TGT;

typedef enum def_acr_dma_direction
{
    ACR_DMA_TO_FB,
    ACR_DMA_FROM_FB,
    ACR_DMA_TO_FB_SCRUB  // Indicates to DMA code that source is just 256 bytes and it needs to be reused for all DMA instructions
}ACR_DMA_DIRECTION;

typedef enum def_acr_dma_sync_type
{
    ACR_DMA_SYNC_NO,
    ACR_DMA_SYNC_AT_END,
    ACR_DMA_SYNC_ALL
}ACR_DMA_SYNC_TYPE;

typedef enum def_reg_label
{
    REG_LABEL_START = -1,
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

typedef struct def_acr_reg_mapping
{
    LwU32 bar0Off;
    LwU32 pwrCsbOff;
} ACR_REG_MAPPING, *PACR_REG_MAPPING;

#define ACR_SCRUB_ZERO_BUF_SIZE_BYTES    (0x100U)
#define ACR_SCRUB_ZERO_BUF_ALIGN         (0x100U)
typedef struct def_acr_scrub_state
{
    LwU8  zeroBuf[ACR_SCRUB_ZERO_BUF_SIZE_BYTES];
    LwU32 scrubTrack;
} ACR_SCRUB_STATE, PACR_SCRUB_STATE;

#define ACR_FLCN_BL_CMDLINE_ALIGN        (ACR_FALCON_BLOCK_SIZE_IN_BYTES)
#define ACR_FLCN_BL_RESERVED_ASM         (0x02f80000)
typedef union def_acr_flcn_bl_cmdline_args
{
    LOADER_CONFIG         legacyBlArgs;
    RM_FLCN_BL_DMEM_DESC  genericBlArgs;
} ACR_FLCN_BL_CMDLINE_ARGS;

//
// TODO: There is pending item to port all this into a selwrebuslib.
//       Once that is complete this will be removed (Bug 1732094)
//
typedef enum def_sec_selwrebus_target
{
    SEC_SELWREBUS_TARGET_HUB,
    SEC_SELWREBUS_TARGET_SE
} ACR_SEC_SELWREBUS_TARGET;

//
// LS overlay group signature data structures and defines.
// At present we have room only for one group and hence restricting to GRP ID 1
//

//
// Three MAGIC DWORDS that will confirm the presence of LS SIG Group.
// This is temporary solution and will soon be replaced with an entry in LSB header.
//
#define ACR_LS_SIG_GROUP_MAGIC_PATTERN_1 0xED300700U
#define ACR_LS_SIG_GROUP_MAGIC_PATTERN_2 0xEE703000U
#define ACR_LS_SIG_GROUP_MAGIC_PATTERN_3 0xCE300300U

// This can be increased if number of LS sig group entries goes beyond 20.
#define ACR_LS_SIG_GRP_LOAD_BUF_SIZE   (ACR_FALCON_BLOCK_SIZE_IN_BYTES)

// This data structure defines each entry present in LS Sig group block
typedef struct def_acr_ls_sig_grp_entry
{
    LwU32 startPc;
    LwU32 endPc;
    LwU32 grpId;
} ACR_LS_SIG_GRP_ENTRY;

// List of all group Ids supported
#define    ACR_ENUM_LS_SIG_GRP_ID_ILWALID        0U
#define    ACR_ENUM_LS_SIG_GRP_ID_1              1U
#define    ACR_ENUM_LS_SIG_GRP_ID_TOTAL          1U

// This is the header at the start of LS Sig group
typedef struct def_acr_ls_sig_grp_header
{
    LwU32                magicPattern1;
    LwU32                magicPattern2;
    LwU32                magicPattern3;
    LwU32                numLsSigGrps;
} ACR_LS_SIG_GRP_HEADER;

#endif // TEGRA_ACR_TYPES_H
