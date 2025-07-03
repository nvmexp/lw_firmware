/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __OEMCOMMONLW_H__
#define __OEMCOMMONLW_H__

#include <drmresults.h>
#include "pr/pr_common.h"
#include "sec2/sec2ifcmn.h"
#include "scp_internals.h"
#include "lwtypes.h"
#include "dev_sec_csb.h"
#include "dev_disp.h"
#include "oemaescommon.h"
#include "oemteetypes.h"
#include "drmhashcache.h"
#include "drmteecache.h"
#include "drmtypes.h"
#include "oemtee.h"

#if defined(__cplusplus) && USE_CLAW

#include <clawrt/rt.h>

#else /* defined(__cplusplus) && USE_CLAW */

#define CLAW_AUTO_RANDOM_CIPHER
#define CLAW_ATTRIBUTE_NOINLINE

#endif /* defined(__cplusplus) && USE_CLAW */

ENTER_PK_NAMESPACE;

/*
 ** Defines of the actions which can be run at HS mode.
 */
typedef enum
{
    PR_HS_AES_KDF,
    PR_HS_DMHASH,
    PR_HS_SHARED_STRUCT,
    PR_HS_UPDATE_DISP_BLANKING_POLICY,
    PR_SB_HS_ENTRY,
    PR_SB_HS_DECRYPT_MPK,
    PR_SB_HS_EXIT,
    PR_HS_MUTEX_ACQUIRE_RELEASE,
} OEM_TEE_HS_ACTION_CODE;

/*
 ** Struct for arguments needed when performing AES KDF at HS.
 */
typedef struct
{
    DRM_BYTE   *f_pbBuffer;
    DRM_DWORD   f_fCryptoMode;
} AES_KDF_ARGS, *PAES_KDF_ARGS;

/*
 ** Struct for arguments needed when running dmhash computation at HS.
 */
typedef struct
{
   DRM_BYTE    *f_pbHash;
   DRM_BYTE    *f_pbData;
} COMP_DMHASH_ARGS, *PCOMP_DMHASH_ARGS;

/*
 ** Struct for arguments needed when updating display blanking policies at HS.
 */
typedef struct
{
   DRM_DWORD    f_policyValForContext;
   DRM_BOOL     f_type1LockingRequired;
} UPDATE_DISP_BLANKING_POLICY_ARGS, *PUPDATE_DISP_BLANKING_POLICYARGS;

/*
 ** Struct for arguments needed when acquiring/releasing mutex at HS.
 */
typedef struct
{
   DRM_BYTE     f_logicalMutexId;
   DRM_DWORD    f_timeoutNs;
   DRM_BYTE    *f_pbToken;
   DRM_BOOL     f_fMutexAcquire;
} HS_MUTEX_ACQUIRE_RELEASE_ARGS, *PHS_MUTEX_ACQUIRE_RELEASE_ARGS;

/*
 ** Lists of global variables that will be shared with LWDEC.
 */
extern DRM_TEE_CACHE_ENTRY g_rgoCache[OEM_TEE_CACHE_MAX_ENTRIES] PR_SHARED_ATTR_DATA_OVLY(_g_rgoCache)     GCC_ATTRIB_ALIGN(64);
extern DRM_BYTE            g_iHead                               PR_SHARED_ATTR_DATA_OVLY(_g_iHead)        GCC_ATTRIB_ALIGN(4);
extern DRM_BYTE            g_iTail                               PR_SHARED_ATTR_DATA_OVLY(_g_iTail)        GCC_ATTRIB_ALIGN(4);
extern DRM_BYTE            g_iFree                               PR_SHARED_ATTR_DATA_OVLY(_g_iFree)        GCC_ATTRIB_ALIGN(4);
extern DRM_BYTE            g_cCache                              PR_SHARED_ATTR_DATA_OVLY(_g_cCache)       GCC_ATTRIB_ALIGN(4);
extern DRM_BYTE            g_fInitialized                        PR_SHARED_ATTR_DATA_OVLY(_g_fInitialized) GCC_ATTRIB_ALIGN(4);

/*
 ** Error codes to help debug
 */
#define  ERROR_CODE_000_ILWALID_USE_OF_COLWERT_OEM_TEE_KEY_MACRO     0xdeb00000

//
// LWE (kwilson) - While we have picked 1ms as a safe default, it may be possible
// to constrain this number further based on testing.
// 
#define PR_MUTEX_ACQUIRE_TIMEOUT_NS       (1000000) // 1ms

/*
 ** Macro used to determine if the request of HDCP2.2 type1 lock succeeds or not
 */
#define IS_TYPE1_LOCK_REQ_SUCCESS(data)                                                                           \
    (FLD_TEST_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _SUCCESS_TYPE1_ENGAGED, data) || \
     FLD_TEST_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _SUCCESS_HDCP22_OFF, data)    || \
     FLD_TEST_DRF(_PDISP, _SELWRE_WR_SCRATCH_0, _VPR_HDCP22_TYPE1_LOCK_RESPONSE, _SUCCESS_TYPE1_OFF, data))

/*********************** LWPU OEM function prototypes ***********************/
DRM_ALWAYS_INLINE DRM_API DRM_VOID* DRM_CALL doCleanupAndHalt(LwU32 errorCode);

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_InitSharedDataStruct(DRM_VOID);

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_HsCommonEntryWrapper(
    __in          OEM_TEE_HS_ACTION_CODE    f_action,
    __in          DRM_BYTE                  f_ovlId,
    __inout       DRM_VOID                 *f_pArgs );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_HsCommonEntry(
    __in          OEM_TEE_HS_ACTION_CODE    f_action,
    __inout       DRM_VOID                 *f_pArgs )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_FlcnStatusToDrmStatus(
    __in          FLCN_STATUS               f_flcnStatus );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_FlcnStatusToDrmStatus_HS(
    __in          FLCN_STATUS               f_flcnStatus );

EXIT_PK_NAMESPACE;

#endif   //__OEMCOMMONLW_H__
