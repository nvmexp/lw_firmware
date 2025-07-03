/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

// Included libSPDM copyright, as file is LW-authored but uses libSPDM code.

/*!
 * @file   lw_apm_rts.c
 * @brief  The functions needed to access Root of Storage SubWPr from SPDM.
 *         RTS still needs to be initialized properly by ACR, and the exelwting
 *         Falcon needs to implement a hook to locate RTS.
 */

/* --------------------------- System Includes ----------------------------- */
#include "base.h"
#include "lwuproc.h"
#include "lwrtos.h"
#include "lwos_dma.h"
#include "mmu/mmucmn.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2/sec2ifcmn.h"
#include "industry_standard/spdm.h"
#include "lw_apm_rts.h"
#include "lw_utils.h"
#include "sw_rts_msr_usage.h"

/* ------------------------- Macro Definitions  ----------------------------- */
//
// These macros correspond to MSRs in sw_rts_msr_usage.h.
//
// Hashing is implied by the absence of 
// SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_RAW_BIT_STREAM.
//
#define APM_RTS_MSR_TYPE_FUSES            (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_HARDWARE_CONFIGURATION)
#define APM_RTS_MSR_TYPE_VPR_MMU_STATE    (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_HARDWARE_CONFIGURATION)
#define APM_RTS_MSR_TYPE_WPR_MMU_STATE    (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_HARDWARE_CONFIGURATION)
#define APM_RTS_MSR_TYPE_DECODE_TRAPS     (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_HARDWARE_CONFIGURATION)

#define APM_RTS_MSR_TYPE_FALCON_PMU       (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE)
#define APM_RTS_MSR_TYPE_FALCON_FECS      (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE)
#define APM_RTS_MSR_TYPE_FALCON_GPCCS     (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE)
#define APM_RTS_MSR_TYPE_FALCON_LWDEC     (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE)
#define APM_RTS_MSR_TYPE_FALCON_SEC2      (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE)


/* --------------------------- Global Variables ----------------------------- */
// The buffer used to DMA the data from RTS.
static LwU8 dma_apm_group_buf[sizeof(APM_MSR_GROUP)]
    GCC_ATTRIB_SECTION("dmem_spdm", "dma_apm_group_buf")
    GCC_ATTRIB_ALIGN(256U);

//
// Offset of RTS in the FB. Must be initialized via call to
// apm_rts_offset_initialize().
//
static LwU64 apmRtsOffset
    GCC_ATTRIB_SECTION("dmem_spdm", "apmRtsOffset");

/* ------------------------- Function Prototypes ---------------------------- */
void apm_rts_offset_initialize(uint64 rts_offset)
    GCC_ATTRIB_SECTION("imem_libspdm", "apm_rts_offset_initialize");

/* ------------------------- Lw_apm_rts.h Functions ------------------------- */
/**
  Initializes the variable tracking the offset of RTS in FB.

  @param[in] rts_offset  Offset of RTS in FB.
 */
void
apm_rts_offset_initialize
(
    uint64 rts_offset
)
{
    apmRtsOffset = rts_offset;
}

/**
  Returns the variable tracking the offset of RTS in FB.

  @return  Offset of RTS in FB.
 */
uint64
apm_get_rts_offset
(
    void
)
{
    return apmRtsOffset;
}

/**
  @brief Reads the MSR group defined by msr_idx from the RTS into memory
         pointed to by dst.

         APM-TODO CONFCOMP-465: Implement synchronization for the HS milestone

  @note Must call apm_rts_offset_initialize before using this function.

  @param[in]  msr_idx  The index of the MSR group.
  @param[out] dst      The destination buffer in DMEM.

  @retval TRUE   MSR group read successfully.
  @retval FALSE  MSR group read failed.
 */
boolean
apm_rts_read_msr_group
(
    IN  uint32         msr_idx,
    OUT PAPM_MSR_GROUP dst
)
{
    FLCN_STATUS      flcnStatus = FLCN_OK;
    LwU64            offset     = 0;
    RM_FLCN_MEM_DESC memDesc;

    offset = APM_MSR_GRP_OFFSET(apmRtsOffset, msr_idx);

    RM_FLCN_U64_PACK(&(memDesc.address), &offset);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_SEC2_DMAIDX_UCODE, 0);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                     sizeof(APM_MSR_GROUP), memDesc.params);

    CHECK_FLCN_STATUS(dmaReadUnrestricted(dma_apm_group_buf, &memDesc, 0,
                                          sizeof(APM_MSR_GROUP)));

    copy_mem(dst, &dma_apm_group_buf, sizeof(APM_MSR_GROUP));

ErrorExit:

    return FLCN_STATUS_TO_BOOLEAN(flcnStatus);
}

/**
  @brief Reads the MSR/MSRS defined by msr_idx from the RTS into memory pointed
         to by dst.
         APM-TODO CONFCOMP-465: Implement synchronization for the HS milestone

  @note Must call apm_rts_offset_initialize before using this function.

  @param[in]  msr_idx      The index of the MSR
  @param[out] dst          The destination buffer in DMEM
  @param[in]  read_shadow  Read shadow or ordinary MSR

  @retval TRUE   MSR read successfully
  @retval FALSE  MSR read failed
 */
boolean
apm_rts_read_msr
(
    IN  uint32   msr_idx,
    OUT PAPM_MSR dst,
    IN  boolean  read_shadow
)
{
    FLCN_STATUS      flcnStatus = FLCN_OK;
    LwU64            offset     = 0;
    RM_FLCN_MEM_DESC memDesc;

    offset = read_shadow ? APM_MSRS_OFFSET(apmRtsOffset, msr_idx) :
                           APM_MSR_OFFSET(apmRtsOffset, msr_idx);

    RM_FLCN_U64_PACK(&(memDesc.address), &offset);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_SEC2_DMAIDX_UCODE, 0);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                     sizeof(APM_MSR), memDesc.params);

    CHECK_FLCN_STATUS(dmaReadUnrestricted(dma_apm_group_buf, &memDesc, 0,
                                          sizeof(APM_MSR)));

    copy_mem(dst, &dma_apm_group_buf, sizeof(APM_MSR));

ErrorExit:

    return FLCN_STATUS_TO_BOOLEAN(flcnStatus);
}

/**
   @brief The function returns the type (encoding and origin) of the specified MSR.
          // APM-TODO: When implementing GH100 SPDM turn this into a HAL
   
   @param[in]  msr_idx  The index of the MSR whose type is requested
   @param[out] msr_type The pointer to the destination uint8

   @return return_status
   @return RETURN_SUCCESS           Successful exelwtion
   @retval RETURN_ILWALID_PARAMETER msr_idx out of range or msr_type is NULL
 */
return_status apm_rts_get_msr_type(IN uint32 msr_idx, OUT uint8 *msr_type)
{
    if (msr_type == NULL || msr_idx >= APM_RTS_MSR_INDEX_ILWALID)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    switch (msr_idx)
    {
        case APM_RTS_MSR_INDEX_FALCON_PMU:
            *msr_type = APM_RTS_MSR_TYPE_FALCON_PMU;
            break;
        case APM_RTS_MSR_INDEX_FALCON_FECS:
            *msr_type = APM_RTS_MSR_TYPE_FALCON_FECS;
            break;
        case APM_RTS_MSR_INDEX_FALCON_GPCCS:
            *msr_type = APM_RTS_MSR_TYPE_FALCON_GPCCS;
            break; 
        case APM_RTS_MSR_INDEX_FALCON_LWDEC:
            *msr_type =  APM_RTS_MSR_TYPE_FALCON_LWDEC;
            break;
        case APM_RTS_MSR_INDEX_FALCON_SEC2:
            *msr_type = APM_RTS_MSR_TYPE_FALCON_SEC2;
            break;
        case APM_RTS_MSR_INDEX_FUSES:
            *msr_type = APM_RTS_MSR_TYPE_FUSES;
            break;
        case APM_RTS_MSR_INDEX_VPR_MMU_STATE:
            *msr_type = APM_RTS_MSR_TYPE_VPR_MMU_STATE;
            break;     
        case APM_RTS_MSR_INDEX_WPR_MMU_STATE:
            *msr_type = APM_RTS_MSR_TYPE_WPR_MMU_STATE;
            break;   
        case APM_RTS_MSR_INDEX_DECODE_TRAPS:
            *msr_type = APM_RTS_MSR_TYPE_DECODE_TRAPS;
            break;
        default:
            *msr_type = SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_RAW_BIT_STREAM;
    }

    return RETURN_SUCCESS;
}
