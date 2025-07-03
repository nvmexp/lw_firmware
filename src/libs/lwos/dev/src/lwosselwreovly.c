/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes ------------------------------- */
#include "lwoslayer.h"
#include "lwosselwreovly.h"
#include "lwrtos.h"
#include "lwos_ovl_priv.h"

/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Static Variables ------------------------------ */
/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
/*!
 * @brief Setup entry to heavy secure (HS) mode for RTOS tasks
 *
 * This function disables interrupts, and loads the HS signature for the HS
 * overlay by doing a trapped DMA. Note that this function does not actually
 * jump to the HS code, but relies on the task to enter into HS mode by calling
 * a function at its 0th overlay. It is the responsibility of the task to call
 * the cleanup function after it has finished its HS mode tasks and returned to
 * a less secure code block (see @osHSModeCleanup).
 *
 * If pOvlList is non-NULL and HS_OVERLAYS_ENABLED is set,
 * call @lwosOvlImemRefresh and pass LW_TRUE as the last argument
 * to refresh the overlays in pOvlList in HS.
 *
 * NOTE: This function has us enter into critical section, and it is required
 * that the task calls @osHSModeCleanup with the same pOvlList and ovlCnt
 * after we're out of HS mode
 *
 * @param[in]  ovlIdx      overlay index of the HS overlay we're going to use
 * @param[in]  bInTask     if we're called from a task context
 * @param[in]  pOvlList    list of overlay index(es) we're going to refresh
 * @param[in]  ovlCnt      number of the overlay index(es) in pOvlList
 * @param[in]  bHashSaving toggle to turn overlay hash saving on/off
 *
 * Note: Until resident shared overlays are fully supported,
 * do not use these parmaters with non-NULL/non-zero values
 *
 * TODO: support for shared LS/HS resident IMEM overlays is yet to be added.
 */
void osHSModeSetup
(
    LwU8    ovlIdx,
    LwBool  bInTask,
    LwU8   *pOvlList,
    LwU8    ovlCnt,
    LwBool  bHashSaving
)
{
    LwU32 *pSignature;

    // Check that we're setting up a valid HS overlay
    if ((ovlIdx == OVL_INDEX_ILWALID) || (ovlIdx > OVL_COUNT_IMEM_HS()))
    {
        OS_HALT();
    }

    // Check that bHashSaving has a valid value
    if ((bHashSaving != HASH_SAVING_ENABLE) && (bHashSaving != HASH_SAVING_DISABLE))
    {
        // Note: To avoid halting / throwing an error here, we will continue with
        // hash saving disabled if the value passed to the function doesn't make sense
        bHashSaving = HASH_SAVING_DISABLE;
    }

    // We will be in critical section all of the time that we're in HS mode
    if (bInTask)
    {
        lwrtosENTER_CRITICAL();
    }

#ifdef HS_UCODE_ENCRYPTION
    //
    // force reload all the resident HS overlays since they may be in IMEM in
    // decrypted form
    //
    lwosOvlImemLoadAllResidentHS();
#endif

#ifdef HS_OVERLAYS_ENABLED
    //
    // Refresh IMEM ovls if non-NULL pOvlList is given.
    // Refresh all requested resident LS IMEM overlays required in HS mode.
    //
    if (pOvlList != NULL)
    {
        lwosOvlImemRefresh(pOvlList, ovlCnt, LW_TRUE);
    }
#endif

#ifdef IS_PKC_ENABLED

    LwU32 i;
    LwU32 signatureIndex     = 0;
    LwU32 hwRevocationFpfVal = osHWFpfVersion(_imem_hs_sig_ucode_id[ovlIdx]);

    // Copy the correct signature from the binary for verification by BROM. 
    if (hwRevocationFpfVal <= _imem_hs_sig_ucode_fuse_ver[ovlIdx])
    {
        //
        // Siggen stores signatures in the following order
        // When NUM_SIG_PER_UCODE is set. 
        //  sig_for_fuse = NUM_SIG_PER_UCODE
        //  sig_for_fuse = NUM_SIG_PER_UCODE - 1 
        //  sig_for_fuse = NUM_SIG_PER_UCODE - 2
        //  sig_for_fuse = NUM_SIG_PER_UCODE - 3
        //  ....
        //  sig_for_fuse = NUM_SIG_PER_UCODE - (NUM_SIG_PER_UCODE - 1)
        //  sig_for_fuse = 0  (Signature for Fuse version 0 is always present for internal use)
        //
        if (hwRevocationFpfVal == 0)
        {
            signatureIndex = NUM_SIG_PER_UCODE - 1;
        }
        else
        {
            signatureIndex = (NUM_SIG_PER_UCODE - hwRevocationFpfVal - 1);
        }
        // Invalid signatureIndex will be handled by PKC Bootrom which will reject the ucode
    }
    else
    {
        //
        // Newer fuse, older ucode. This means this ucode is already revoked.
        // We don't have a signature that corresponds to this revision, so don't do anything
        // BROM will reject this during signature validation.
        // 
    }

#ifdef RUNTIME_HS_OVL_SIG_PATCHING
    pSignature = OS_SEC_HS_SIGNATURE_PKC_RUNTIME_SIG_PATCHING(ovlIdx);
#else
    pSignature = OS_SEC_HS_SIGNATURE_PKC(ovlIdx, signatureIndex);
#endif // RUNTIME_HS_OVL_SIG_PATCHING

    for(i = 0; i < SIG_SIZE_IN_BITS/32; i++)
    {
        pkcSignatureParams.signature.rsa3k_sig.S[i] = pSignature[i];
    }
    pkcSignatureParams.pk          = NULL;
    pkcSignatureParams.hash_saving = bHashSaving;

    falc_stxb_i((LwU32*)FALCON_REG(_FALCON_BROM_PARAADDR(0)),   0, (LwU32)&pkcSignatureParams);
    falc_stxb_i((LwU32*)FALCON_REG(_FALCON_BROM_ENGIDMASK),     0, ENGINE_ID);

    falc_stxb_i((LwU32*)FALCON_REG(_FALCON_BROM_LWRR_UCODE_ID), 0,
       FALCON_DRF_NUM(FALCON_TYPE, _FALCON_BROM_LWRR_UCODE_ID, _VAL, _imem_hs_sig_ucode_id[ovlIdx]));

    falc_stxb_i((LwU32*)FALCON_REG(_FALCON_MOD_SEL),            0,
       FALCON_DRF_NUM(FALCON_TYPE, _FALCON_MOD_SEL, _ALGO, FALCON_REG(_FALCON_MOD_SEL_ALGO_RSA3K)));
#else
    //
    // AES Legacy signature verification
    //
    pSignature = OS_SEC_HS_SIGNATURE(ovlIdx);
    asm ("cci 0x2;");
    falc_dmwrite(0, ((6 << 16) | (LwU32)(pSignature)));
    falc_dmwait();
#endif // IS_PKC_ENABLED

#ifdef HS_UCODE_ENCRYPTION
    falc_wspr(SEC, SVEC_REG((LwU32)(_imem_ovl_tag[ovlIdx]),
                            (LwU32)(_imem_ovl_size[ovlIdx]),
                            0, 1));
#else
    falc_wspr(SEC, SVEC_REG((LwU32)(_imem_ovl_tag[ovlIdx]),
                            (LwU32)(_imem_ovl_size[ovlIdx]),
                            0, 0));
#endif
}

/*!
 * @brief Cleanup after exit from heavy secure (HS) mode for RTOS tasks
 *
 * This function clears the SEC special purpose register and re-enables
 * interrupts after exit from HS mode.
 *
 * If pOvlList is non-NULL and HS_OVERLAYS_ENABLED is set,
 * call @lwosOvlImemRefresh and pass LW_FALSE as the last argument
 * to refresh the overlays in pOvlList in LS.
 *
 * NOTE: It is expected that tasks call the corresponding entry setup function
 * @osHSModeSetup with the same pOvlList and ovlCnt before entering HS mode
 *
 * @param[in]  pOvlList    list of overlay index(es) we're going to refresh
 * @param[in]  ovlCnt      number of the overlay index(es) in pOvlList
 *
 * Note: Until resident shared overlays are fully supported,
 * do not use these parmaters with non-NULL/non-zero values
 *
 * TODO: support for shared LS/HS resident IMEM overlays is yet to be added.
 */
void
osHSModeCleanup
(
    LwBool  bInTask,
    LwU8   *pOvlList,
    LwU8    ovlCnt
)
{
    // Clear SEC after returning to LS mode
    falc_wspr(SEC, SVEC_REG(0, 0, 0, 0));

#ifdef IS_PKC_ENABLED
    falc_stxb_i((LwU32*)FALCON_REG(_FALCON_BROM_PARAADDR(0)),   0, 0);
    falc_stxb_i((LwU32*)FALCON_REG(_FALCON_BROM_ENGIDMASK),     0, 0);
    falc_stxb_i((LwU32*)FALCON_REG(_FALCON_BROM_LWRR_UCODE_ID), 0,
       FALCON_DRF_NUM(FALCON_TYPE, _FALCON_BROM_LWRR_UCODE_ID, _VAL, 0));
#endif

#ifdef HS_OVERLAYS_ENABLED
    //
    // Refresh IMEM ovls if non-NULL pOvlList is given.
    // Refresh all requested resident HS IMEM overlays required in LS mode.
    //
    if (pOvlList != NULL)
    {
        lwosOvlImemRefresh(pOvlList, ovlCnt, LW_FALSE);
    }
#endif

    // Exit critical section once we're back in LS mode
    if (bInTask)
    {
        lwrtosEXIT_CRITICAL();

#ifdef HS_UCODE_ENCRYPTION
        // force a reload to re-evaluate HS overlays
        lwrtosRELOAD();
#endif
    }
}
