/*
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2ga100only.c
 * @brief  SEC2 HAL Functions for GA100
 *
 * SEC2 HAL functions implement chip-specific functionality, limited to GA100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "dev_ram.h"
#include "sec2_csb.h"
#include "mmu/mmucmn.h"
#include "dev_pbdma_addendum.h"
#include "dev_sec_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_sec2_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
// Size of IV stored in instance block during secure channel allocation.
#define INST_BLOCK_IV_SIZE_IN_BYTES (12)

/* ------------------------- Implementation --------------------------------- */
/*!
 * @brief Retrieve (or save) the encryption IV slot ID and the value of the
 *        decryption IV stored in the instance block for secure channels.
 *        If the secure channel IV context is not valid, that means the channel
 *        is not properly alloacted as a secure channel, and we return failure.
 * 
 *        This is used only by APM to retrieve IVs for secure channel
 *        encryption/decryption.
 * 
 * @param[in]  bSave     If true, saves the info. If false, restores it. 
 * @param[out] pSlotId   On restore, pointer to int which will hold slot ID.
 * @param[out] pDecIv    Pointer to buffer that holds/will hold decryption IV.
 *                       Must have at least minimum DMA read/write alignment.
 * @param[in]  decIvSize Number of bytes of the decryption IV to write/read.
 * @param[in]  ctx       Entire ctx (current/next) register value.
 *
 * @return FLCN_STATUS indicating success or relevant failure.
 */
FLCN_STATUS
sec2SaveRestoreSelwreChannelIvInfo_GA100
(
    LwBool  bSave,
    LwU32  *pSlotId,
    LwU8   *pDecIv,
    LwU32   decIvSize,
    LwU32   ctx
)
{
    FLCN_STATUS      flcnStatus      = FLCN_OK;
    LwU64            instBlkAddr     = 0;
    LwU64            encIvSlotIdAddr = 0;
    LwU64            decIvAddr       = 0;
    LwU32            ivContext       = 0;
    LwU8             dmaIdx          = 0;
    RM_FLCN_MEM_DESC memDesc;

    memset(&memDesc, 0, sizeof(memDesc));
  
    if (pSlotId == NULL || pDecIv == NULL || decIvSize == 0 ||
        decIvSize > INST_BLOCK_IV_SIZE_IN_BYTES)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    dmaIdx = sec2GetPhysDmaIdx_HAL(
        &Sec2, DRF_VAL(_CSEC, _FALCON_LWRCTX, _CTXTGT, ctx));

    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     dmaIdx, memDesc.params);

    //
    // Instance block is at 4KB aligned address, host drops lowest 12 bits.
    // Left shift 4KB to get actual address.
    //
    instBlkAddr   = DRF_VAL(_CSEC, _FALCON_LWRCTX, _CTXPTR, ctx);
    instBlkAddr <<= SHIFT_4KB;
 
    //
    // Get addresses of IV information, stored in subcontext fields of RAMIN.
    // SEC2 uses the subcontext fields as 'scratch state' to store slot ID
    // and decrypt IV. The subcontext fields in the instance block for SEC2
    // channels are ignored by HW.
    //
    // Data is stored starting at LW_RAMIN_SC_PAGE_DIR_BASE_TARGET(0), with IV
    // context first, and decryption IV stored afterwards. As DRF offsets from
    // instance block memory are in bits, divide by 8 to get a byte offset.
    //
    encIvSlotIdAddr = instBlkAddr + (DRF_BASE(LW_RAMIN_SC_PAGE_DIR_BASE_TARGET(0)) >> 3);
    decIvAddr       = encIvSlotIdAddr + sizeof(LwU32);

    // First, read secure channel IV context, returning failure if invalid.
    RM_FLCN_U64_PACK(&(memDesc.address), &encIvSlotIdAddr);
    CHECK_FLCN_STATUS(dmaRead(&ivContext, &memDesc, 0, sizeof(ivContext)));
    if (!FLD_TEST_DRF(_PPBDMA, _HCE_CTRL, _CC_CTX_ID_VALID, _TRUE, ivContext))
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // Save/restore the relevant information.
    if (bSave)
    {
        // We only need to save decryption IV, as slot ID should not update.
        RM_FLCN_U64_PACK(&(memDesc.address), &decIvAddr);
        CHECK_FLCN_STATUS(dmaWrite(pDecIv, &memDesc, 0, decIvSize));
    }
    else
    {
        // Retrieve the slot ID from context.
        *pSlotId = DRF_VAL(_PPBDMA, _HCE_CTRL, _CC_CTX_ID, ivContext);

        // Read decryption IV.
        RM_FLCN_U64_PACK(&(memDesc.address), &decIvAddr);
        CHECK_FLCN_STATUS(dmaRead(pDecIv, &memDesc, 0, decIvSize));
    }

ErrorExit:
 
    return flcnStatus;
}

/*!
 * @brief Do cleanup of scratch registers
 * Note that this is an HS function
 **/
FLCN_STATUS
sec2CleanupScratchHs_GA100(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32                i = 0;

    // Clean up the secure scratch register groups
    for (i = 0; i < LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0__SIZE_1; i++)
    {
        CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0(i),
                                                 LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0_VAL_INIT));

        CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_1(i),
                                                 LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_1_VAL_INIT));

        CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_2(i),
                                                 LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_2_VAL_INIT));

        CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_3(i),
                                                 LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_3_VAL_INIT));
    }

ErrorExit:
    return flcnStatus;
}
