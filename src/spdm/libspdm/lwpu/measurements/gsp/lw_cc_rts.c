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
 * @file   lw_cc_rts.c
 * @brief  The functions needed to access Root of Storage SubWPr from SPDM.
 *         RTS still needs to be initialized properly by ACR, and the exelwting
 *         Falcon needs to implement a hook to locate RTS.
 */

/* --------------------------- System Includes ----------------------------- */
#include "base.h"
#include "lwuproc.h"

/* ------------------------- Application Includes --------------------------- */
#include "industry_standard/spdm.h"
#include "cc_rts.h"
#include "lw_cc_rts.h"
#include "lw_utils.h"
#include "hw_rts_msr_usage.h"

/* --------------------------- Macro Definitions ---------------------------- */

/* ------------------------- Macro Definitions  ----------------------------- */
//
// These macros correspond to MSRs in hw_rts_msr_usage.h.
//
// Hashing is implied by the absence of 
// SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_RAW_BIT_STREAM.
//
#define CC_RTS_MSR_TYPE_FALCON_PMU       (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE)
#define CC_RTS_MSR_TYPE_FALCON_FECS      (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE)
#define CC_RTS_MSR_TYPE_FALCON_GPCCS     (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE)
#define CC_RTS_MSR_TYPE_FALCON_LWDEC     (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE)
#define CC_RTS_MSR_TYPE_FALCON_SEC2      (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE)
#define CC_RTS_MSR_TYPE_FUSES            (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_HARDWARE_CONFIGURATION)
#define CC_RTS_MSR_TYPE_MMU_STATE        (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_HARDWARE_CONFIGURATION)
#define CC_RTS_MSR_TYPE_DECODE_TRAPS     (SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_HARDWARE_CONFIGURATION)

/* --------------------------- Global Variables ----------------------------- */


/* ------------------------- Function Prototypes ---------------------------- */


/* ------------------------- lw_cc_rts.h Functions ------------------------- */


/**
  @brief Reads the MSR/MSRS defined by msr_idx from the RTS into memory pointed
         to by dst.
         HCC-TODO eddichang : 

  @note Must call apm_rts_offset_initialize before using this function.

  @param[in]  msr_idx      The index of the MSR
  @param[out] dst          The destination buffer in DMEM
  @param[in]  read_shadow  Read shadow or ordinary MSR

  @retval TRUE   MSR read successfully
  @retval FALSE  MSR read failed
 */
boolean
cc_rts_read_msr
(
    IN  uint8     msr_idx,
    OUT void     *pdst,
    IN  boolean   read_shadow
)
{
    // HCC-TODO: eddichang talk to Sanket/Rahul to get detail

    return TRUE;
}

/**
   @brief The function returns the type (encoding and origin) of the specified MSR.
          // HCC-TODO: When implementing GH100 SPDM turn this into a HAL
   
   @param[in]  msr_idx  The index of the MSR whose type is requested
   @param[out] msr_type The pointer to the destination uint8

   @return return_status
   @return RETURN_SUCCESS           Successful exelwtion
   @retval RETURN_ILWALID_PARAMETER msr_idx out of range or msr_type is NULL
 */
return_status
cc_rts_get_msr_type
(
    IN  uint8  msr_idx,
    OUT uint8 *msr_type
)
{
    // HCC-TODO: eddichang talk to Sanket/Rahul to get detail
    *msr_type = (uint8)SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_RAW_BIT_STREAM;
 
   return RETURN_SUCCESS;
}
