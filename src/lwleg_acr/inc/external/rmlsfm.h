/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef RMLSFM_H
#define RMLSFM_H

/*****************************************************************************/
/*             This file is shared between ACR, SEC2 Binaries                */
/*             Do not update this file without updating ACR/SEC2             */
/*****************************************************************************/

/*!
 * @file   rmlsfm.h
 * @brief  Top-level header-file that defines Light Secure Falcon Managment
           SW shared interfaces.
 */

/*!
 * READ/WRITE masks for WPR region
 */
#define LSF_WPR_REGION_RMASK                    (0xLW) // Readable only from level 2 and 3 client
#define LSF_WPR_REGION_WMASK                    (0xLW) // Writable only from level 2 and 3 client
#define LSF_WPR_REGION_RMASK_SUB_WPR_ENABLED    (0x8) // Readable only from level 3 client
#define LSF_WPR_REGION_WMASK_SUB_WPR_ENABLED    (0x8) // Writable only from level 3 client
#define LSF_WPR_REGION_ALLOW_READ_MISMATCH_NO   (0x0) // Disallow read mis-match for all clients
#define LSF_WPR_REGION_ALLOW_WRITE_MISMATCH_NO  (0x0) // Disallow write mis-match for all clients

/*!
 * READ mask for WPR region on CheetAh
 * This is required until we update cheetah binaries, Bug 200281517
 * TODO: dgoyal - Remove this once cheetah binaries are updated
 */
#define LSF_WPR_REGION_RMASK_FOR_TEGRA   (0xFU)

/*!
 * Expected REGION ID to be used for the unprotected FB region (region that 
 * does not have read or write protections)
 */
#define LSF_UNPROTECTED_REGION_ID   (0x0U)

/*!
 * Expected REGION ID to be used for the WPR region for the falcon microcode (includes data).
 * ACR allots client requests to each region based on read/write masks and it is supposed
 * to provide first priority to requests from LSFM. Providing first priority will naturally assign
 * region ID 1 to LSFM and this define will provide a way for different parties to sanity check
 * this fact. Also there are other falcons (FECS/video falcons) which depends on this define, so please
 * be aware while modifying this.
 */
#define LSF_WPR_EXPECTED_REGION_ID  (0x1U)

/*!
 * Expected REGION ID to be used for the unused WPR region.
 */
#define LSF_WPR_UNUSED_REGION_ID    (0x2U)

/*!
 * Invalid LS falcon subWpr ID
 */
#define LSF_SUB_WPR_ID_ILWALID      (0xFFFFFFFFU)

/*!
 * Expected REGION ID to be used for the VPR region.
 */
#define LSF_VPR_REGION_ID           (0x3U)

/*!
 * Size of the separate bootloader data that could be present in WPR region.
 */
#define LSF_LS_BLDATA_EXPECTED_SIZE (0x100U)

/*!
 * since we dont check signatures in GC6 exit, we need to hardcode the WPR offset
 */
#define LSF_WPR_EXPECTED_OFFSET     (0x0U)

/*!
 * CTXDMA to be used while loading code/data in target falcons
 */
#define LSF_BOOTSTRAP_CTX_DMA_FECS                (0x0)

/*!
 * Context DMA ID 6 is reserved for Video UCODE
 */
#define LSF_BOOTSTRAP_CTX_DMA_VIDEO               (0x6)
#define LSF_BOOTSTRAP_CTX_DMA_BOOTSTRAP_OWNER     (0x0)
#define LSF_BOOTSTRAP_CTX_DMA_FBFLCN              (0x0)

/*!
 * Falcon Id Defines
 * Defines a common Light Secure Falcon identifier.
 * Codesigning infra. assumes LSF_FALCON_ID_ prefix for units,
 * Changes to the define needs to be reflected in path [1]
 */
//! [1] //sw/dev/gpu_drv/chips_a/apps/codesigning/lib/perl5/SelwreSign/LSFalcon/Unit.pm

#define LSF_FALCON_ID_PMU       (0U)
#define LSF_FALCON_ID_DPU       (1U)
#define LSF_FALCON_ID_GSPLITE   LSF_FALCON_ID_DPU
#define LSF_FALCON_ID_FECS      (2U)
#define LSF_FALCON_ID_GPCCS     (3U)
#define LSF_FALCON_ID_LWDEC     (4U)
#define LSF_FALCON_ID_LWENC     (5U)
#define LSF_FALCON_ID_LWENC0    (5U)
#define LSF_FALCON_ID_LWENC1    (6U)
#define LSF_FALCON_ID_SEC2      (7U)
#define LSF_FALCON_ID_LWENC2    (8U)
#define LSF_FALCON_ID_MINION    (9U)
#define LSF_FALCON_ID_FBFALCON  (10U)
#define LSF_FALCON_ID_XUSB      (11U)
#define LSF_FALCON_ID_GSP_RISCV (12U)
#define LSF_FALCON_ID_PMU_RISCV (13U)
#define LSF_FALCON_ID_SOE       (14U)
#define LSF_FALCON_ID_LWDEC1    (15U)
#define LSF_FALCON_ID_OFA       (16U)
#define LSF_FALCON_ID_END       (17U)
#define LSF_FALCON_ID_ILWALID   (0xFFFFFFFFU)

#define LSF_FALCON_INSTANCE_DEFAULT_0           (0x0)
// Lwrrently max supported instance is 8 for FECS/GPCCS SMC
#define LSF_FALCON_INSTANCE_FECS_GPCCS_MAX      (0x8)
#define LSF_FALCON_INSTANCE_ILWALID             (0xFFFFFFFFU)

// CheetAh PMU will not have impact. These definitions are just used to make compiler happy.
// PMU OBJACR_ALIGNED_256 size will vary with LSF_FALCON_ID_END.
// https://lwgrok-02:8621/resman/xref/chips_a/sw/dev/gpu_drv/chips_a/pmu_sw/inc/acr/pmu_objacr.h#101
// PMU could run out of DMEM in case we increase LSF_FALCON_ID_END more and more.
// Since PMU support ACR task pre_Turing, we just need to use fixed value to fix this issue.
// 
#define LSF_FALCON_ID_END_PMU               (LSF_FALCON_ID_FBFALCON + 1) 
#define LSF_WPR_HEADERS_TOTAL_SIZE_MAX_PMU  (LW_ALIGN_UP((sizeof(LSF_WPR_HEADER) * LSF_FALCON_ID_END_PMU), LSF_WPR_HEADER_ALIGNMENT))

/*!
 * Source ID defines
 * Defines source IDs for clients using PRI access.
 */
#define PRI_SOURCE_ID_XVE      (0U)
#define PRI_SOURCE_ID_PMU      (1U)
#define PRI_SOURCE_ID_GSP      (3U)
#define PRI_SOURCE_ID_FECS     (4U)
#define PRI_SOURCE_ID_GPCCS    (5U)
#define PRI_SOURCE_ID_ALL      (0x3FU)

/*!
 * Size in entries of the ucode descriptor's dependency map.
 * This used to be LSF_FALCON_ID_END, but since that had to grow and we did not want to break any
 * existing binaries, they had to be split.
 *
 * Increasing this number should be done with care.
 */
#define LSF_FALCON_DEPMAP_SIZE  (11)

/*!
 * Falcon Binaries version defines
 */
#define LSF_FALCON_BIN_VERSION_ILWALID (0xFFFFFFFFU)

//
// Lwrrently 2 components in ucode image have signature.
// One is code section and another is data section.
//
#define LSF_UCODE_COMPONENT_INDEX_CODE          (0)
#define LSF_UCODE_COMPONENT_INDEX_DATA          (1)
#define LSF_UCODE_COMPONENT_INDEX_MAX           (2)

//
// Lwrrently, for version 2, we set MAX LS signature size to be 512.
//
#define RSA3K_SIGNATURE_SIZE_BYTE               (384)
#define RSA3K_SIGNATURE_SIZE_BITS               (3072) // 384 * 8
#define RSA3K_SIGNATURE_PADDING_SIZE_BYTE       (128)
#define RSA3K_SIGNATURE_PADDED_SIZE_BYTE        (RSA3K_SIGNATURE_SIZE_BYTE + RSA3K_SIGNATURE_PADDING_SIZE_BYTE)
#define RSA3K_SIGNATURE_PADDED_SIZE_DWORD       (RSA3K_SIGNATURE_PADDED_SIZE_BYTE/4U)
#define RSASSA_PSS_HASH_LENGTH_BYTE             (32)
#define RSASSA_PSS_SALT_LENGTH_BYTE             (RSASSA_PSS_HASH_LENGTH_BYTE)

// Size in bytes for RSA 3K (RSA_3K struct from bootrom_pkc_parameters.h
#define RSA3K_PK_SIZE_BYTE                      (2048)

#define LSF_SIGNATURE_SIZE_RSA3K_BYTE           RSA3K_SIGNATURE_PADDED_SIZE_BYTE
#define LSF_SIGNATURE_SIZE_MAX_BYTE             LSF_SIGNATURE_SIZE_RSA3K_BYTE
#define LSF_PK_SIZE_MAX                         RSA3K_PK_SIZE_BYTE

// LS Encryption Defines
#define LS_ENCRYPTION_AES_CBC_IV_SIZE_BYTE      (16) 

/*!
 * Light Secure Falcon Ucode Description Defines 
 * This stucture is prelim and may change as the ucode signing flow evolves.
 */
typedef struct
{
    LwU8  prdKeys[2][16];
    LwU8  dbgKeys[2][16];
    LwU32 bPrdPresent;
    LwU32 bDbgPresent;
    LwU32 falconId;
    LwU32 bSupportsVersioning;
    LwU32 version;
    LwU32 depMapCount;
    LwU8  depMap[LSF_FALCON_DEPMAP_SIZE * 2 * 4];
    LwU8  kdf[16];
} LSF_UCODE_DESC, *PLSF_UCODE_DESC;

/*!
* WPR generic struct header
* identifier - To identify type of WPR struct i.e. WPR vs SUBWPR vs LSB vs LSF_UCODE_DESC
* version    - To specify version of struct, for backward compatibility
* size       - Size of struct, include header and body
*/
typedef struct
{
    LwU16 identifier;
    LwU16 version;
    LwU32 size;
} WPR_GENERIC_HEADER, *PWPR_GENERIC_HEADER;

/*!
 * Light Secure Falcon Ucode Version 2 Description Defines
 * This stucture is prelim and may change as the ucode signing flow evolves.
 */
typedef struct
{
    LwU32 falconId;  // LsEnginedId
    LwU8  bPrdPresent;
    LwU8  bDbgPresent;
    LwU16 reserved;
    LwU32 sigSize;
    LwU8  prodSig[LSF_UCODE_COMPONENT_INDEX_MAX][LSF_SIGNATURE_SIZE_RSA3K_BYTE];
    LwU8  debugSig[LSF_UCODE_COMPONENT_INDEX_MAX][LSF_SIGNATURE_SIZE_RSA3K_BYTE];
    LwU16 sigAlgoVer;
    LwU16 sigAlgo;
    LwU16 hashAlgoVer;
    LwU16 hashAlgo;
    LwU32 sigAlgoPaddingType;
    LwU8  depMap[LSF_FALCON_DEPMAP_SIZE * 8];
    LwU32 depMapCount;
    LwU8  bSupportsVersioning;
    LwU8  pad[3];
    LwU32 lsUcodeVersion; // LsUcodeVersion
    LwU32 lsUcodeId;      // LsUcodeId
    LwU32 bUcodeLsEncrypted;
    LwU32 lsEncAlgoType;
    LwU32 lsEncAlgoVer;
    LwU8  lsEncIV[LS_ENCRYPTION_AES_CBC_IV_SIZE_BYTE];
    LwU8  rsvd[36];              // reserved for any future usage
} LSF_UCODE_DESC_V2, *PLSF_UCODE_DESC_V2;

//
// The wrapper for LSF_UCODE_DESC, start support from version 2.
//
typedef struct
{
   WPR_GENERIC_HEADER genericHeader;
   union
   {
       LSF_UCODE_DESC_V2 lsfUcodeDescV2;
   };
} LSF_UCODE_DESC_WRAPPER, *PLSF_UCODE_DESC_WRAPPER;

/*!
 * Light Secure WPR Header 
 * Defines state allowing Light Secure Falcon bootstrapping.
 *
 * falconId       - LS falcon ID
 * lsbOffset      - Offset into WPR region holding LSB header
 * bootstrapOwner - Bootstrap OWNER (either PMU or SEC2)
 * bLazyBootstrap - Skip bootstrapping by ACR
 * status         - Bootstrapping status
 */
typedef struct
{
    LwU32  falconId;
    LwU32  lsbOffset;
    LwU32  bootstrapOwner;
    LwU32  bLazyBootstrap;
    LwU32  bilwersion;
    LwU32  status;
} LSF_WPR_HEADER, *PLSF_WPR_HEADER;

/*!
 * LSF shared SubWpr Header
 *
 * useCaseId  - Shared SubWpr se case ID (updated by RM)
 * startAddr  - start address of subWpr (updated by RM)
 * size4K     - size of subWpr in 4K (updated by RM)
 */
typedef struct
{
    LwU32 useCaseId;
    LwU32 startAddr;
    LwU32 size4K;
} LSF_SHARED_SUB_WPR_HEADER, *PLSF_SHARED_SUB_WPR_HEADER;

// Shared SubWpr use case IDs
typedef enum
{
    LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_FRTS_VBIOS_TABLES     = 1,
    LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_PLAYREADY_SHARED_DATA = 2
} LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_ENUM;

#define LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_MAX         LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_PLAYREADY_SHARED_DATA
#define LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_ILWALID     (0xFFFFFFFFU)

#define MAX_SUPPORTED_SHARED_SUB_WPR_USE_CASES          LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_MAX

//
// Static sizes of shared subWPRs
// Minimum granularity supported is 4K
//
#define LSF_SHARED_DATA_SUB_WPR_FRTS_VBIOS_TABLES_SIZE_IN_4K             (0x100)   // 1MB in 4K
#define LSF_SHARED_DATA_SUB_WPR_PLAYREADY_SHARED_DATA_SIZE_IN_4K         (0x1)     // 4K

/*!
 * Bootstrap Owner Defines 
 */
#define LSF_BOOTSTRAP_OWNER_PMU       (LSF_FALCON_ID_PMU)
#define LSF_BOOTSTRAP_OWNER_SEC2      (LSF_FALCON_ID_SEC2)
#define LSF_BOOTSTRAP_OWNER_GSPLITE   (LSF_FALCON_ID_GSPLITE)
#define LSF_BOOTSTRAP_OWNER_DEFAULT   (LSF_BOOTSTRAP_OWNER_PMU)
#define LSF_BOOTSTRAP_OWNER_PMU_RISCV (LSF_FALCON_ID_PMU_RISCV)
#define LSF_BOOTSTRAP_OWNER_GSP_RISCV (LSF_FALCON_ID_GSP_RISCV)

/*!
 * Image Status Defines 
 */
#define LSF_IMAGE_STATUS_NONE                           (0U)
#define LSF_IMAGE_STATUS_COPY                           (1U)
#define LSF_IMAGE_STATUS_VALIDATION_CODE_FAILED         (2U)
#define LSF_IMAGE_STATUS_VALIDATION_DATA_FAILED         (3U)
#define LSF_IMAGE_STATUS_VALIDATION_DONE                (4U)
#define LSF_IMAGE_STATUS_VALIDATION_SKIPPED             (5U)
#define LSF_IMAGE_STATUS_BOOTSTRAP_READY                (6U)
#define LSF_IMAGE_STATUS_REVOCATION_CHECK_FAILED        (7U)

/*!
 * Light Secure Bootstrap Header 
 * Defines state allowing Light Secure Falcon bootstrapping.
 *
 * signature     - Code/data signature details for this LS falcon
 * ucodeOffset   - Offset into WPR region where UCODE is located
 * ucodeSize     - Size of ucode
 * dataSize      - Size of ucode data
 * blCodeSize    - Size of bootloader that needs to be loaded by bootstrap owner
 * blImemOffset  - BL starting virtual address. Need for tagging.
 * blDataOffset  - Offset into WPR region holding the BL data
 * blDataSize    - Size of BL data
 * appCodeOffset - Offset into WPR region where Application UCODE is located
 * appCodeSize   - Size of Application UCODE
 * appDataOffset - Offset into WPR region where Application DATA is located
 * appDataSize   - Size of Application DATA
 * blLoadCodeAt0 - Load BL at 0th IMEM offset
 * bSetVACtx     - Make sure to set the code/data loading CTX DMA to be virtual before exiting
 * bDmaReqCtx    - This falcon requires a ctx before issuing DMAs
 * bForcePrivLoad- Use priv loading method instead of bootloader/DMAs
 */

#define LW_FLCN_ACR_LSF_FLAG_LOAD_CODE_AT_0             0:0
#define LW_FLCN_ACR_LSF_FLAG_LOAD_CODE_AT_0_FALSE       0
#define LW_FLCN_ACR_LSF_FLAG_LOAD_CODE_AT_0_TRUE        1
#define LW_FLCN_ACR_LSF_FLAG_SET_VA_CTX                 1:1
#define LW_FLCN_ACR_LSF_FLAG_SET_VA_CTX_FALSE           0
#define LW_FLCN_ACR_LSF_FLAG_SET_VA_CTX_TRUE            1
#define LW_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX             2:2
#define LW_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX_FALSE       0
#define LW_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX_TRUE        1
#define LW_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD            3:3
#define LW_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_FALSE      0
#define LW_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_TRUE       1

typedef struct
{
    LSF_UCODE_DESC    signature;
    LwU32 ucodeOffset;
    LwU32 ucodeSize;
    LwU32 dataSize;
    LwU32 blCodeSize;
    LwU32 blImemOffset;
    LwU32 blDataOffset;
    LwU32 blDataSize;
    LwU32 appCodeOffset;
    LwU32 appCodeSize;
    LwU32 appDataOffset;
    LwU32 appDataSize;
    LwU32 flags;
} LSF_LSB_HEADER, *PLSF_LSB_HEADER;

typedef struct
{
    LSF_UCODE_DESC_WRAPPER signature;
    LwU32 ucodeOffset;
    LwU32 ucodeSize;
    LwU32 dataSize;
    LwU32 blCodeSize;
    LwU32 blImemOffset;
    LwU32 blDataOffset;
    LwU32 blDataSize;
    LwU32 appCodeOffset;
    LwU32 appCodeSize;
    LwU32 appDataOffset;
    LwU32 appDataSize;
    LwU32 flags;
} LSF_LSB_HEADER_V2, *PLSF_LSB_HEADER_V2;

/*!
 * Light Secure WPR Content Alignments
 */
#define LSF_WPR_HEADER_ALIGNMENT        (256U)
#define LSF_SUB_WPR_HEADER_ALIGNMENT    (256U)
#define LSF_LSB_HEADER_ALIGNMENT        (256U)
#define LSF_BL_DATA_ALIGNMENT           (256U)
#define LSF_BL_DATA_SIZE_ALIGNMENT      (256U)
#define LSF_BL_CODE_SIZE_ALIGNMENT      (256U)
#define LSF_DATA_SIZE_ALIGNMENT         (256U)
#define LSF_CODE_SIZE_ALIGNMENT         (256U)

// MMU excepts subWpr sizes in units of 4K
#define SUB_WPR_SIZE_ALIGNMENT          (4096U)

/*!
 * Maximum WPR Header size
 */
#define LSF_WPR_HEADERS_TOTAL_SIZE_MAX      (LW_ALIGN_UP((sizeof(LSF_WPR_HEADER) * LSF_FALCON_ID_END), LSF_WPR_HEADER_ALIGNMENT))
#define LSF_LSB_HEADER_TOTAL_SIZE_MAX       (LW_ALIGN_UP(sizeof(LSF_LSB_HEADER), LSF_LSB_HEADER_ALIGNMENT))

// Maximum SUB WPR header size 
#define LSF_SUB_WPR_HEADERS_TOTAL_SIZE_MAX  (LW_ALIGN_UP((sizeof(LSF_SHARED_SUB_WPR_HEADER) * LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_MAX), LSF_SUB_WPR_HEADER_ALIGNMENT))

/*!
 * For the ucode surface alignment, We align to RM_PAGE_SIZE because of
 * busMapRmAperture issues, not because of Falcon ucode alignment requirements
 * which lwrrently are that it be at least 256.
 */
#define LSF_UCODE_DATA_ALIGNMENT RM_PAGE_SIZE

/*!
 * ACR Descriptors used by ACR UC
 */

/*!
 * Supporting maximum of 2 regions.
 * This is needed to pre-allocate space in DMEM
 */
#define RM_FLCN_ACR_MAX_REGIONS                  (2)
#define LSF_BOOTSTRAP_OWNER_RESERVED_DMEM_SIZE   (0x200)

/*!
 * startAddress   - Starting address of region
 * endAddress     - Ending address of region
 * regionID       - Region ID
 * readMask       - Read Mask
 * writeMask      - WriteMask
 * clientMask     - Bit map of all clients lwrrently using this region
 * shadowMemStartAddress- FB location from where contents need to be copied to startAddress 
 */
typedef struct def_acr_dmem_region_prop
{
    LwU32   startAddress;
    LwU32   endAddress;
    LwU32   regionID;
    LwU32   readMask;
    LwU32   writeMask;
    LwU32   clientMask;
    LwU32   shadowMemStartAddress;
} RM_FLCN_ACR_REGION_PROP, *PRM_FLCN_ACR_REGION_PROP;


/*!
 * noOfRegions   - Number of regions used by RM.
 * regionProps   - Region properties
 */
typedef struct def_acr_regions
{
    LwU32                     noOfRegions;
    RM_FLCN_ACR_REGION_PROP   regionProps[RM_FLCN_ACR_MAX_REGIONS];
} RM_FLCN_ACR_REGIONS, *PRM_FLCN_ACR_REGIONS;

/*!
 * bVprEnabled      : When set, ACR_LOCKDOWN phase programs VPR range. Needs to be
                    : LwU32 because of alignment
 * vprStartAddress  : Start address of VPR region. SEC2 binary updates this value
 * vprEndAddress    : End address of VPR region. SEC2 binary updates this value
 * hdcpPolicies     : VPR display policies. SEC2 binary updates this value 
 */
typedef struct def_acr_vpr_dmem_desc
{
    LwU32 bVprEnabled;
    LwU32 vprStartAddress;
    LwU32 vprEndAddress;
    LwU32 hdcpPolicies;
} ACR_BSI_VPR_DESC, *PACR_BSI_VPR_DESC;

/*!
 * reservedDmem - When the bootstrap owner has done bootstrapping other falcons,
 *                and need to switch into LS mode, it needs to have its own actual
 *                DMEM image copied into DMEM as part of LS setup. If ACR desc is at
 *                location 0, it will definitely get overwritten causing data corruption.
 *                Hence we are reserving 0x200 bytes to give room for any loading data.
 *                NOTE: This has to be the first member always
 * signature    - Signature of ACR ucode. 
 * wprRegionID  - Region ID holding the WPR header and its details
 * wprOffset    - Offset from the WPR region holding the wpr header
 * regions      - Region descriptors
 * ucodeBlobBase- Used for CheetAh, stores non-WPR start address where kernel stores ucode blob
 * ucodeBlobSize- Used for CheetAh, stores the size of the ucode blob
 */
typedef struct def_acr_dmem_desc
{
    LwU32                signatures[4];
    LwU32                wprRegionID;
    LwU32                wprOffset;
    LwU32                mmuMemoryRange;
    RM_FLCN_ACR_REGIONS  regions;
    LwU32                ucodeBlobSize;
    // uCodeBlobBase is moved after ucodeBlobSize to inherently align to qword (8 bytes)
    LwU64                LW_DECLARE_ALIGNED(ucodeBlobBase, 8);

    /*!
     * Do not change the offset of this descriptor as it shared between
     * ACR_REGION_LOCKDOWN HS binary and SEC2. Any change in this structure
     * need recompilation of SEC2 and ACR_LOCKDOWN HS binary
     */
    ACR_BSI_VPR_DESC     vprDesc;
} RM_FLCN_ACR_DESC, *PRM_FLCN_ACR_DESC;

/*!
 * wprRegionID  - Region ID holding the WPR header and its details
 * wprOffset    - Offset from the WPR region holding the wpr header
 * regions      - Region descriptors
 * ucodeBlobBase- Used for CheetAh, stores non-WPR start address where kernel stores ucode blob
 * ucodeBlobSize- Used for CheetAh, stores the size of the ucode blob
 */
typedef struct def_acr_dmem_desc_riscv
{
    LwU32                wprRegionID;
    LwU32                wprOffset;
    RM_FLCN_ACR_REGIONS  regions;
    LwU32                ucodeBlobSize;
    // uCodeBlobBase is moved after ucodeBlobSize to inherently align to qword (8 bytes)
    LwU64                LW_DECLARE_ALIGNED(ucodeBlobBase, 8);
    LwU64                pmuUcodeDescBase;
    // mode varibale is used to set multiple features bitwise given below:
    // Emulate mode       7:0 bit
    // MIG mode          15:8 bit
    LwU32                mode;
} RISCV_ACR_DESC, *PRISCV_ACR_DESC;

typedef struct def_riscv_pmu_ucode_desc {
    LwU32 version;
    LwU32 bootloader_offset;
    LwU32 bootloader_size;
    LwU32 bootloader_param_offset;
    LwU32 bootloader_param_size;
    LwU32 next_core_elf_offset;
    LwU32 next_core_elf_size;
    LwU32 app_version;
    /* manifest contains information about monitor and it is input to br */
    LwU32 manifest_offset;
    LwU32 manifest_size;
    /* monitor data offset within next_core image and size */
    LwU32 monitor_data_offset;
    LwU32 monitor_data_size;
    /* monitor code offset withtin next_core image and size */
    LwU32 monitor_code_offset;
    LwU32 monitor_code_size;
    LwBool is_monitor_enabled;
} RISCV_PMU_DESC, *PRISCV_PMU_DESC;

/*!
* Hub keys/nonce Structure in BSI
*/
#define MAX_SFBHUB_ENCRYPTION_REGION_KEY_SIZE 4

typedef struct def_acr_hub_scratch_data
{
    LwU32    key[MAX_SFBHUB_ENCRYPTION_REGION_KEY_SIZE];
    LwU32    nonce[MAX_SFBHUB_ENCRYPTION_REGION_KEY_SIZE];
} ACR_BSI_HUB_DESC, *PACR_BSI_HUB_DESC;

#define MAX_HUB_ENCRYPTION_REGION_COUNT 3
typedef struct def_acr_hub_scratch_array
{
    ACR_BSI_HUB_DESC entries[MAX_HUB_ENCRYPTION_REGION_COUNT];
} ACR_BSI_HUB_DESC_ARRAY, *PACR_BSI_HUB_DESC_ARRAY;

typedef struct def_acr_reserved_dmem
{
    LwU32            reservedDmem[(LSF_BOOTSTRAP_OWNER_RESERVED_DMEM_SIZE/4)];  // Always first.. 
} ACR_RESERVED_DMEM, *PACR_RESERVED_DMEM;

#define LW_FLCN_ACR_DESC_FLAGS_SIG_VERIF           0:0
#define LW_FLCN_ACR_DESC_FLAGS_SIG_VERIF_DISABLE   0
#define LW_FLCN_ACR_DESC_FLAGS_SIG_VERIF_ENABLE    1

/*!
 * Size of ACR phase in dword
 */
#define ACR_PHASE_SIZE_DWORD                       sizeof(RM_FLCN_ACR_DESC)/sizeof(LwU32)

/*!
 * Falcon Mode Tokens
 * This is the value logged to a mailbox register to indicate that the
 * falcon isn't booted in secure mode.
 */
#define LSF_FALCON_MODE_TOKEN_FLCN_INSELWRE   (0xDEADDEADU)

#endif // RMLSFM_H

