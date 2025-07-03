/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2018 by LWPU Corpora0x00840804tion.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <aes128.h>
#include <oemteecrypto.h>
#include <oemteecryptointernaltypes.h>

#include "sec2sw.h"
#include "sec2_hs.h"
#include "sec2_objpr.h"
#include "sec2_objsec2.h"
#include "pr/pr_lassahs.h"
#include "pr/pr_lassahs_hs.h"
#include "playready/4.4/lw_playready.h"
#include "lw_mutex.h"
#include "lib_mutex.h"

#include "dev_therm.h"
#include "dev_therm_addendum.h"

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_COMPILE)

DRM_ALWAYS_INLINE DRM_API DRM_VOID* DRM_CALL doCleanupAndHalt(LwU32 errorCode)
{
    // write mailbox
    REG_WR32_STALL(CSB, LW_CSEC_FALCON_MAILBOX0, errorCode);
    // halt falcon
    falc_halt();

    return NULL;
}

/*
** Synopsis:
**
** We have some global variables which needs to be shared with LWDEC.  Those shared variables are
** put in certain FB area, SEC2 or LWDEC can access that specific area with the protection of
** critical section (i.e. mutex will be acquired/released).  When either SEC2 or LWDEC acquires
** the mutex in order to access the shared variables, it will page in (via DMA) the shared variables
** from FB to its DMEM for usage.  After it finishes the usage of shared variables, it will then
** page out (via DMA) the shared variables back to FB before releasing the mutex so that the data on
** FB will always be the most updated.
**
** This function is used to fill the FB address information of those shared variables to a secure
** scratch register, so that LWDEC knows where to access these shared variables.  The sequence will be:
** 1. Enter critical section so that mutex will be acquired and the shared variables will be paged into SEC2's DMEM
** 2. Reset the shared variables to default value since this function is called at init time
** 3. (From HS mode) Fill the FB address information of each shared global variable into a shared struct (i.e. g_sharedDataOffsets)
**    and fill the address of this shared struct to secure scratch register.
** 4. Write the data of this shared struct back to FB (via DMA).
** 5. Leave critical section so that mutex will be released and the shared variables will be paged back to FB.
**
** On failure,
** DRM_E_OEM_DMA_FAILURE is returned if SEC2 failed to DMA data to FB.
** Other error code can be returned from each called function, and will be bubbled up directly.
*/
SHARED_DATA_OFFSETS g_sharedDataOffsets GCC_ATTRIB_ALIGN(256) = {0};
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_InitSharedDataStruct( DRM_VOID )
{
    DRM_RESULT       drRet                 = DRM_SUCCESS;  /* Do not use dr to ensure no jump macros are used. */
    DRM_RESULT       drTmp                 = DRM_SUCCESS;  /* Do not use dr to ensure no jump macros are used. */
    LwU64            prSharedStructStartFB = 0;
    RM_FLCN_MEM_DESC memDesc;

    // Size of the shared struct needs to meet the min requirement of dma write
    if (!VAL_IS_ALIGNED(sizeof(SHARED_DATA_OFFSETS),
                        DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MIN_WRITE)))
    {
        SEC2_HALT();
    }

    //
    // Enter critical section to page the shared data to DMEM
    // (Never use jump macros while holding the critical section)
    //
    if (!DRM_SUCCEEDED(drRet = OEM_TEE_BASE_CRITSEC_Enter()))
    {
        return drRet;
    }

    // Reset all shared data to default value
    OEM_TEE_ZERO_MEMORY(g_rgoCache, OEM_TEE_CACHE_MAX_ENTRIES * sizeof(DRM_TEE_CACHE_ENTRY));
    g_iHead        = DRM_TEE_CACHE_HEAD_ENTRY;
    g_iTail        = DRM_TEE_CACHE_TAIL_ENTRY;
    g_iFree        = 0;
    g_cCache       = 0;
    g_fInitialized = TEE_CACHE_UNINITIALIZED;

    // Generate the shared struct data at HS mode
    drRet = OEM_TEE_HsCommonEntryWrapper(PR_HS_SHARED_STRUCT, OVL_INDEX_ILWALID, NULL);

    if (DRM_SUCCEEDED(drRet))
    {
        // Get the FB base of shared struct
        if (!DRM_SUCCEEDED(drRet = OEM_TEE_FlcnStatusToDrmStatus(prGetSharedDataRegionDetails_HAL(&PrHal, PR_GET_SHARED_STRUCT_START, &prSharedStructStartFB))))
        {
            return drRet;
        }

        // Write the shared struct back to FB so that LWDEC can start using it
        RM_FLCN_U64_PACK(&(memDesc.address), &prSharedStructStartFB);
        memDesc.params = 0;
        memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                             RM_SEC2_DMAIDX_UCODE, memDesc.params);
        memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                             sizeof(SHARED_DATA_OFFSETS), memDesc.params);

        if (dmaWriteUnrestricted(&g_sharedDataOffsets, &memDesc, 0,
                sizeof(SHARED_DATA_OFFSETS)) != FLCN_OK)
        {
            drRet = DRM_E_OEM_DMA_FAILURE;
        }
    }

    // Leave critical section (the shared data will be paged back to FB)
    drTmp = OEM_TEE_BASE_CRITSEC_Leave();

    // If there is any error before leaving critical section, then return that error code directly
    if (DRM_SUCCEEDED(drRet))
    {
        drRet = drTmp;
    }

    return drRet;
}

/*
** Synopsis:
**
** The wrapper to help load the HS overlays, configure the HS mode before calling
** into the common HS entry function.  This wrapper also helps cleanup the HS configuration
** and unload the HS overlays after the HS exelwtion is done.
**
** Arguments:
**
** f_action:    (in)       The action we want to do at HS mode
** f_ovlId:     (in)       The ID of the extra overlay needs to be loaded except the .prHsCommon overlay
** f_pArgs:     (in/out)   Arguments required for the corresponding action
**
** On failure,
** Return the error code returned from OEM_TEE_HsCommonEntry
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_HsCommonEntryWrapper
(
    __in          OEM_TEE_HS_ACTION_CODE    f_action,
    __in          DRM_BYTE                  f_ovlId,
    __inout       DRM_VOID                 *f_pArgs
)
{
    DRM_RESULT dr = DRM_SUCCESS;
    
    //
    // Load the required HS overlays (Don't need to load any overlay in LASSAHS mode
    // since the HS entry of LASSAHS already takes care of it)
    //
    if (!g_prLassahsData.bActive)
    {
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(prHsCommon));
        if (f_ovlId != OVL_INDEX_ILWALID)
        {
            OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(f_ovlId);
        }
    }
    // Setup entry into HS mode
    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(prHsCommon), NULL, 0, HASH_SAVING_DISABLE);

    // Call into the HS entry function to start the HS exelwtion
    dr = OEM_TEE_HsCommonEntry(f_action, f_pArgs);

    // Cleanup after returning from HS mode
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    //
    // Unload the HS overlays (Don't need to unload any overlay in LASSAHS mode
    // since the HS exit of LASSAHS already takes care of it)
    //
    if (!g_prLassahsData.bActive)
    {
        if (f_ovlId != OVL_INDEX_ILWALID)
        {
            OSTASK_DETACH_IMEM_OVERLAY_COND_HS(f_ovlId);
        }
        OSTASK_DETACH_IMEM_OVERLAY_COND_HS(OVL_INDEX_IMEM(prHsCommon));
    }
    return dr;
}

/*
** Synopsis:
**
** The common HS mode entry function which dispatches the request to the corresponding
** functions/codes
**
** Arguments:
**
** f_action:    (in)       The action we want to do at HS mode
** f_pArgs:     (in/out)   Arguments required for the corresponding action
**
** On failure,
** DRM_E_OEM_HS_CHK_ILWALID_INPUT is returned if the input argument is invalid.
** DRM_E_OEM_UNSUPPORTED_HS_ACTION is returned if the requested HS action is not supported.
** Other error code can be returned from each HS function, and will be bubbled up directly.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_HsCommonEntry
(
    __in          OEM_TEE_HS_ACTION_CODE    f_action,
    __inout       DRM_VOID                 *f_pArgs
)
{
    //
    // Check return PC to make sure that it is not in HS
    // This must always be the first statement in HS entry function
    //
    VALIDATE_RETURN_PC_AND_HALT_IF_HS();

    DRM_RESULT  dr        = DRM_SUCCESS;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs, HASH_SAVING_DISABLE);

    // Make sure we are ready to run at HS mode
    ChkDR(OEM_TEE_FlcnStatusToDrmStatus_HS(prHsPreCheck_HAL(&PrHal, g_prLassahsData.bActive)));

    switch (f_action)
    {
        case PR_HS_AES_KDF:
        {
            ChkPtrAndReturn(f_pArgs, DRM_E_OEM_HS_CHK_ILWALID_INPUT);

            PAES_KDF_ARGS pAesKdfArgs = (PAES_KDF_ARGS)f_pArgs;
            ChkDR(Oem_Aes_EncryptOneBlockWithKeyForKDFMode_LWIDIA_HS(pAesKdfArgs->f_pbBuffer, pAesKdfArgs->f_fCryptoMode));
            break;
        }

        case PR_HS_DMHASH:
        {
            ChkPtrAndReturn(f_pArgs, DRM_E_OEM_HS_CHK_ILWALID_INPUT);

            PCOMP_DMHASH_ARGS pDmhashArgs = (PCOMP_DMHASH_ARGS)f_pArgs;
            ChkDR(ComputeDmhash_HS(pDmhashArgs->f_pbHash, pDmhashArgs->f_pbData, sizeof(LW_KB_CDKBData)));
            break;
        }

        case PR_HS_SHARED_STRUCT:
        {
            LwU64 prSharedStructStartFB;

            //
            // Get the FB base of shared struct by adding the data memory base to the
            // offset of the shared struct from base of data memory (in VA space)
            //
            ChkDR(OEM_TEE_FlcnStatusToDrmStatus_HS(prGetSharedDataRegionDetailsHS_HAL(&PrHal, PR_GET_SHARED_STRUCT_START, &prSharedStructStartFB)));

            // The FB base of shared struct should be 256 aligned. So make sure it meets the assumption first
            if (!VAL_IS_ALIGNED(prSharedStructStartFB, BIT(LW_THERM_SELWRE_WR_SCRATCH_SEC2_LWDEC_SHARED_STRUCT_BASE_BIT_ALIGNMENT)))
            {
                ChkDR(DRM_E_OEM_HS_CHK_ILWALID_INPUT);
            }

            //
            // Fill the address information of each variable (delta between DMEM VA
            // address of the variable of interest and base of g_sharedDataOffsets) to
            // the share struct
            //
            prInitializeSharedStructHS_HAL(&PrHal);

            //
            // Provide the FB offset of the shared struct to LWDEC via scratch register
            // This will be a 32 bits value after the 40 bits PA is right shifted 8 bits,
            // so a single 32 bits register is enough.
            //
            ChkDR(OEM_TEE_FlcnStatusToDrmStatus_HS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_SELWRE_WR_SCRATCH_SEC2_LWDEC_SHARED_STRUCT,
                      (prSharedStructStartFB >> LW_THERM_SELWRE_WR_SCRATCH_SEC2_LWDEC_SHARED_STRUCT_BASE_BIT_ALIGNMENT))));
            break;
        }

        case PR_HS_UPDATE_DISP_BLANKING_POLICY:
        {
            ChkPtrAndReturn(f_pArgs, DRM_E_OEM_HS_CHK_ILWALID_INPUT);

            OS_SEC_HS_LIB_VALIDATE(libVprPolicyHs, HASH_SAVING_DISABLE);
            PUPDATE_DISP_BLANKING_POLICYARGS pArgs = (PUPDATE_DISP_BLANKING_POLICYARGS)f_pArgs;
            ChkDR(OEM_TEE_FlcnStatusToDrmStatus_HS(sec2UpdateDispBlankingPolicyHS_HAL(&Sec2, pArgs->f_policyValForContext, (LwBool)pArgs->f_type1LockingRequired)));
            break;
        }

        case PR_SB_HS_ENTRY:
        {
            OS_SEC_HS_LIB_VALIDATE(prSbHsEntry, HASH_SAVING_DISABLE);
            ChkDR(OEM_TEE_FlcnStatusToDrmStatus_HS(prSbHsEntry()));
            break;
        }

        case PR_SB_HS_DECRYPT_MPK:
        {
            ChkPtrAndReturn(f_pArgs, DRM_E_OEM_HS_CHK_ILWALID_INPUT);

            OS_SEC_HS_LIB_VALIDATE(prSbHsDecryptMPK, HASH_SAVING_DISABLE);
            ChkDR(OEM_TEE_FlcnStatusToDrmStatus_HS(prSbHsDecryptMPK((LwU32*)f_pArgs)));
            break;
        }

        case PR_SB_HS_EXIT:
        {
            OS_SEC_HS_LIB_VALIDATE(prSbHsExit, HASH_SAVING_DISABLE);
            ChkDR(OEM_TEE_FlcnStatusToDrmStatus_HS(prSbHsExit()));
            break;
        }

        case PR_HS_MUTEX_ACQUIRE_RELEASE:
        {
            ChkPtrAndReturn(f_pArgs, DRM_E_OEM_HS_CHK_ILWALID_INPUT);

            PHS_MUTEX_ACQUIRE_RELEASE_ARGS pHsMutexArgs = (PHS_MUTEX_ACQUIRE_RELEASE_ARGS)f_pArgs;
            if ((pHsMutexArgs->f_logicalMutexId >= LW_MUTEX_ID__COUNT) ||
                (pHsMutexArgs->f_pbToken == NULL))
            {
                ChkDR(DRM_E_OEM_HS_CHK_ILWALID_INPUT);
            }
            // Temporary WAR to ensure LS can not acquire/release HS mutices
            pHsMutexArgs->f_logicalMutexId = LW_MUTEX_ID_PLAYREADY;

            if (pHsMutexArgs->f_fMutexAcquire == TRUE)
            {
                ChkDR(OEM_TEE_FlcnStatusToDrmStatus_HS(mutexAcquire_hs(pHsMutexArgs->f_logicalMutexId, pHsMutexArgs->f_timeoutNs, pHsMutexArgs->f_pbToken)));
            }
            else
            {
                ChkDR(OEM_TEE_FlcnStatusToDrmStatus_HS(mutexRelease_hs(pHsMutexArgs->f_logicalMutexId, *(pHsMutexArgs->f_pbToken))));
            }
            break;
        }

        default:
        {
            ChkDR(DRM_E_OEM_UNSUPPORTED_HS_ACTION);
            break;
        }
    }

ErrorExit:

    //
    // Re-configure the RNG CTRL registers before leaving HS to ensure RNG
    // still working at LS
    //
    sec2ForceStartScpRNGHs_HAL();

    // We shouldn't disable big hammer lockdown when under LASSAHS mode
    if (!g_prLassahsData.bActive || (f_action == PR_SB_HS_EXIT))
    {
        DISABLE_BIG_HAMMER_LOCKDOWN();
    }

    return dr;
}

/*
** Synopsis:
**
** Colwert the SEC2 error code to PK's error code (This is LS version)
**
** Arguments:
**
** f_flcnStatus:  (in)  The SEC2 error code to be colwerted
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_FlcnStatusToDrmStatus

(
    __in    FLCN_STATUS    f_flcnStatus
)
{
    return _colwertFlcnStatusToDrmStatus(f_flcnStatus);
}

/*
** Synopsis:
**
** Colwert the SEC2 error code to PK's error code (This is HS version)
**
** Arguments:
**
** f_flcnStatus:  (in)  The SEC2 error code to be colwerted
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_FlcnStatusToDrmStatus_HS

(
    __in    FLCN_STATUS    f_flcnStatus
)
{
    return _colwertFlcnStatusToDrmStatus(f_flcnStatus);
}

EXIT_PK_NAMESPACE;

#endif
