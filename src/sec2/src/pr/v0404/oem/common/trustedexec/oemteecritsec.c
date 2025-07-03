/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include "sec2sw.h"
#include "sec2_objsec2.h"
#include "sec2_objpr.h"
#include "oemteecryptointernaltypes.h"
#include "playready/4.4/lw_playready.h"
#include "sec2/sec2ifcmn.h"
#include "oemtee.h"
#include "oemplatform.h"
#include "drmlastinclude.h"
#include "oemcommonlw.h"
#include "pr/pr_common.h"
#include "lw_mutex.h"
#include "lib_mutex.h"
#include "lwosreg.h"

ENTER_PK_NAMESPACE_CODE;

#ifndef DRM_ENABLE_TEE_CRITSEC
/*
** The TEE critical section code is a sample implementation that is only enabled during
** debug builds.  If parallel TEE access is allowed by your port of the PlayReady
** Porting Kit, then the OEM_TEE_BASE_CRITSEC_xxx APIs should be fully implemented for
** all build flavors.
*/
#if DRM_DBG || defined(SEC_COMPILE)
#define DRM_ENABLE_TEE_CRITSEC 1
#else
#define DRM_ENABLE_TEE_CRITSEC 0
#endif /* DRM_DBG */
#endif /* DRM_ENABLE_TEE_CRITSEC */

#if DRM_ENABLE_TEE_CRITSEC
OEM_CRITICAL_SECTION g_DrmTeeCritSec      PR_ATTR_DATA_OVLY(_g_DrmTeeCritSec) = { 0 };
#ifdef NONE
static DRM_BOOL             s_fDrmTeeCritSecInit = FALSE;
#endif
#endif

#if NONE
/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** By default, it is assumed that all calls into the TEE are serialized,
** i.e. only one thread of exelwtion oclwrs within the TEE at any given time.
**
** However, if multiple threads of exelwtion can conlwrrently occur within
** the TEE, then OEM_TEE_BASE_CRITSEC_Initialize, OEM_TEE_BASE_CRITSEC_Enter,
** and OEM_TEE_BASE_CRITSEC_Leave must be implemented in order to
** synchronize global state within the TEE.
**
** This function initializes a single, global critical section handle
** which will be used during future calls to the OEM_TEE_BASE_CRITSEC_Enter
** and OEM_TEE_BASE_CRITSEC_Leave functions.  If implemented, this function
** MUST be called inside the TEE during OEM_TEE_PROXY_Initialize.
** Refer to OEM_TEE_PROXY_Initialize for details.
**
** The global TEE critical section does NOT need to support re-entrancy.
** (A call to OEM_TEE_BASE_CRITSEC_Enter will always be followed
** by a call to OEM_TEE_BASE_CRITSEC_Leave and never by another
** call to OEM_TEE_BASE_CRITSEC_Enter.)
**
** If you have your own global state which requires synchronization,
** you are encouraged to implement the functions in this file and use
** them to synchronize access to your own global state.
**
** Operations Performed:
**
**  1. Initialize the global critical section.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_CRITSEC_Initialize( DRM_VOID )
{
#if DRM_ENABLE_TEE_CRITSEC
    if( !s_fDrmTeeCritSecInit )
    {
        DRM_RESULT drRet = Oem_CritSec_Initialize( &s_DrmTeeCritSec );
        if( DRM_SUCCEEDED( drRet ) )
        {
            s_fDrmTeeCritSecInit = TRUE;
        }
        return drRet;
    }
    else
    {
        return DRM_S_FALSE;
    }
#else
    return DRM_SUCCESS;
#endif
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Uninitializes the global critical section.
** Refer to OEM_TEE_BASE_CRITSEC_Initialize more information.
**
** Operations Performed:
**
**  1. Uninitialize the global critical section.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_CRITSEC_Uninitialize( DRM_VOID )
{
#if DRM_ENABLE_TEE_CRITSEC
    if( s_fDrmTeeCritSecInit )
    {
        DRM_RESULT drRet = Oem_CritSec_Delete( &s_DrmTeeCritSec );
        s_fDrmTeeCritSecInit = FALSE;
        return drRet;
    }
    else
    {
        return DRM_SUCCESS;
    }
#else
    return DRM_SUCCESS;
#endif
}
#endif

#if defined(SEC_COMPILE)
/*
** Helper function paging the prShared data from/to the FB.
**
** Arguments:
**
** f_pageIn:  (in)  Indicates whether we want to page the data from or to the FB.
*/
extern LwU32 _load_start_dmem_prShared[];
extern LwU32 _load_stop_max_dmem_prShared[];
GCC_ATTRIB_NO_STACK_PROTECT()
static DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL _OEM_TEE_BASE_PageSharedData(
    __in              DRM_BOOL            f_pageIn)
{
    DRM_RESULT       dr = DRM_E_FAIL;
    FLCN_STATUS      status;
    LwU32            prSharedDataStartDMEM = LWOS_DATA_SYM_IMAGE_LOC_TO_DMEM_LOC((LwU32)_load_start_dmem_prShared);
    LwU32            prSharedDataSize = ((LwU32)_load_stop_max_dmem_prShared - (LwU32)_load_start_dmem_prShared);
    RM_FLCN_MEM_DESC memDesc;
    LwU64            prSharedDataStartFB;

    // Get the FB base of shared actual data
    ChkDR(OEM_TEE_FlcnStatusToDrmStatus(prGetSharedDataRegionDetails_HAL(&PrHal, PR_GET_SHARED_DATA_START, &prSharedDataStartFB)));

    RM_FLCN_U64_PACK(&(memDesc.address), &prSharedDataStartFB);

    memDesc.params = 0;
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
        RM_SEC2_DMAIDX_UCODE, memDesc.params);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
        prSharedDataSize, memDesc.params);

    if (f_pageIn == TRUE)
    {
        status = dmaReadUnrestricted((void *)prSharedDataStartDMEM, &memDesc,
            0, prSharedDataSize);
    }
    else
    {
        status = dmaWriteUnrestricted((void *)prSharedDataStartDMEM, &memDesc,
            0, prSharedDataSize);
    }

    if (status != FLCN_OK)
    {
        ChkDR(DRM_E_OEM_DMA_FAILURE);
    }

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Enters the global critical section.
** Refer to OEM_TEE_BASE_CRITSEC_Initialize more information.
**
** Operations Performed:
**
**  1. Enter the global critical section.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_CRITSEC_Enter( DRM_VOID )
{
    DRM_RESULT         dr = DRM_SUCCESS;
    PMUTEX_ENGINE_INFO pEI = MUTEX_GET_ENG_INFO(LW_MUTEX_ID_PLAYREADY);

    if (pEI == NULL)
    {
        dr = DRM_E_ILWALIDARG;
        ChkDR(dr);
    }

    if (pEI->accessBus != REG_BUS_SE)
    {
        ChkDR(OEM_TEE_FlcnStatusToDrmStatus(mutexAcquire(LW_MUTEX_ID_PLAYREADY, PR_MUTEX_ACQUIRE_TIMEOUT_NS, &g_DrmTeeCritSec.mutexToken)));
    }
    else
    {
        // We need to access SE secure bus from HS mode
        HS_MUTEX_ACQUIRE_RELEASE_ARGS hsMutexArgs;
        hsMutexArgs.f_logicalMutexId = LW_MUTEX_ID_PLAYREADY;
        hsMutexArgs.f_timeoutNs = PR_MUTEX_ACQUIRE_TIMEOUT_NS;
        hsMutexArgs.f_pbToken = &g_DrmTeeCritSec.mutexToken;
        hsMutexArgs.f_fMutexAcquire = TRUE;

        ChkDR(OEM_TEE_HsCommonEntryWrapper(PR_HS_MUTEX_ACQUIRE_RELEASE, OVL_INDEX_ILWALID, (DRM_VOID *)&hsMutexArgs));
    }

    // LWE (nkuo): Refresh the shared data on DMEM
    ChkDR(_OEM_TEE_BASE_PageSharedData(TRUE));

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Leaves the global critical section.
** Refer to OEM_TEE_BASE_CRITSEC_Initialize more information.
**
** Operations Performed:
**
**  1. Leaves the global critical section.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_CRITSEC_Leave( DRM_VOID )
{
    DRM_RESULT         dr = DRM_SUCCESS;
    PMUTEX_ENGINE_INFO pEI = MUTEX_GET_ENG_INFO(LW_MUTEX_ID_PLAYREADY);

    // LWE (nkuo): Flush the shared data back to FB
    ChkDR(_OEM_TEE_BASE_PageSharedData(FALSE));

    if (pEI == NULL)
    {
        dr = DRM_E_ILWALIDARG;
        ChkDR(dr);
    }

    if (pEI->accessBus != REG_BUS_SE)
    {
        // LWE (kwilson) Release mutex and token
        ChkDR(OEM_TEE_FlcnStatusToDrmStatus(mutexRelease(LW_MUTEX_ID_PLAYREADY, g_DrmTeeCritSec.mutexToken)));
    }
    else
    {
        // We need to access SE secure bus from HS mode
        HS_MUTEX_ACQUIRE_RELEASE_ARGS hsMutexArgs;
        hsMutexArgs.f_logicalMutexId = LW_MUTEX_ID_PLAYREADY;
        hsMutexArgs.f_timeoutNs = 0;
        hsMutexArgs.f_pbToken = &g_DrmTeeCritSec.mutexToken;
        hsMutexArgs.f_fMutexAcquire = FALSE;

        ChkDR(OEM_TEE_HsCommonEntryWrapper(PR_HS_MUTEX_ACQUIRE_RELEASE, OVL_INDEX_ILWALID, (DRM_VOID *)&hsMutexArgs));
    }

ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

