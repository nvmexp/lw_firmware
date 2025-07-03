/*
 * Copyright (c) 2019-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* AES-GCM engine functions for T23X
 */
#ifndef INCLUDED_TEGRA_SE_KAC_GCM_H
#define INCLUDED_TEGRA_SE_KAC_GCM_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#if HAVE_SOC_FEATURE_KAC == 0
#error "KAC include file compiled in old system"
#endif

/*
 * New HW spec for GCM specifies that:
 *
 * OPERATION_0.INIT = 1 : If set GHASH value is forced 0. Otherwise
 *  GHASH value from context is used. Set this only with first block
 *  of AAD data OR with first block of TEXT (when there is no AAD data
 *  for that context).
 *
 * OPERATION_0.FINAL = 1 : If processing last block of both AAD or
 *  TEXT data (i.e. set twice for the same context when both AAD and
 *  TEXT data present).
 *
 * If FINAL = 1 the last block of AAD or TEXT is not a complete AES
 * block and the CRYPTO_LAST_BLOCK_0 field RESIDUAL_BITS must be set
 * nonzero to indicate number of bits used in the last AES block (0
 * means a full block).  HW will pad the last block with zeroes to
 * block boundary before processing it.
 *
 * HW GCM operations maintain the internal context value in the
 * KEY_SLOT context and it is not exposed to SW. This means that it is
 * not possible re-use this key_slot for anything else before GCM
 * completes.
 *
 * Manifest purpose for keyslots used for GCM/GMAC must use GCM PURPOSE.
 *
 * SW must provide updated counter to CRYPTO_LINEAR_CTR registers before each task
 * and maintain the correct counter value (in SW), HW does not update it internally.
 */
status_t eng_aes_gcm_get_operation_preset(const struct se_engine_aes_context *econtext,
					  uint32_t *op_preset_p);

status_t eng_aes_gcm_process_block_locked(async_aes_ctx_t *ac);

#endif /* INCLUDED_TEGRA_SE_KAC_GCM_H */
