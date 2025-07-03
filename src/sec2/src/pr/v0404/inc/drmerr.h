/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMERR_H__
#define __DRMERR_H__

#include <drmtypes.h>
#include <drmdebug.h>
#include <drmresults.h>
#include <drmpragmas.h> /* Required to ChkDR( constant ); */
#include "flcntypes.h"

#if DRM_DBG

ENTER_PK_NAMESPACE;
    extern void (*g_pfDebugAnalyzeDR)(unsigned long, const char*, unsigned long, const char*);
EXIT_PK_NAMESPACE;

#define SetDbgAnalyzeFunction(pfn) g_pfDebugAnalyzeDR = pfn;
#define GetDbgAnalyzeFunction() g_pfDebugAnalyzeDR

#define ExamineDRValue(_drval_,_file_,_line_,_expr_) DRM_DO {                                           \
    if( g_pfDebugAnalyzeDR != NULL )                                                                    \
    {                                                                                                   \
        (*g_pfDebugAnalyzeDR)((unsigned long)(_drval_), (_file_), (unsigned long)(_line_), (_expr_));   \
    }                                                                                                   \
} DRM_WHILE_FALSE

#else  /* DRM_DBG */

#define SetDbgAnalyzeFunction(pfn)
#define ExamineDRValue(_drval_,_file_,_line_,_expr_)
#define GetDbgAnalyzeFunction()

#endif  /* DRM_DBG */

#define ChkDRAllowENOTIMPL(expr) DRM_DO {               \
    dr = (expr);                                        \
    if( dr == DRM_E_NOTIMPL )                           \
    {                                                   \
        dr = DRM_SUCCESS;                               \
    }                                                   \
    else                                                \
    {                                                   \
        ExamineDRValue(dr, __FILE__, __LINE__, #expr);  \
        if( DRM_FAILED( dr ) )                          \
        {                                               \
            goto ErrorExit;                             \
        }                                               \
    }                                                   \
} DRM_WHILE_FALSE

#define ChkDRNoGOTO(expr) DRM_DO {                      \
    dr = (expr);                                        \
    ExamineDRValue(dr, __FILE__, __LINE__, #expr);      \
} DRM_WHILE_FALSE

#define ChkDR(expr) DRM_DO {                            \
    ChkDRNoGOTO( expr );                                \
    if( DRM_FAILED( dr ) )                              \
    {                                                   \
        goto ErrorExit;                                 \
    }                                                   \
} DRM_WHILE_FALSE

#define ChkMem(expr) DRM_DO {                                                                   \
    if( DRM_UNLIKELY( NULL == (expr) ) )                                                        \
    {                                                                                           \
        DRM_DBG_TRACE( ("Allocation failure at %s : %d.\n%s\n", __FILE__, __LINE__, #expr) );   \
        dr = DRM_E_OUTOFMEMORY;                                                                 \
        ExamineDRValue(dr, __FILE__, __LINE__, #expr);                                          \
        goto ErrorExit;                                                                         \
    }                                                                                           \
} DRM_WHILE_FALSE

#define ChkArgError() DRM_DO {                                                                  \
    DRM_DBG_TRACE( ("Invalid argument at %s : %d.\nFALSE\n", __FILE__, __LINE__) );             \
    dr = DRM_E_ILWALIDARG;                                                                      \
    ExamineDRValue(dr, __FILE__, __LINE__, "FALSE");                                            \
    goto ErrorExit;                                                                             \
} DRM_WHILE_FALSE

#define ChkArg(expr) DRM_DO {                                                                   \
    if( DRM_UNLIKELY( !(expr) ) )                                                               \
    {                                                                                           \
        DRM_DBG_TRACE( ("Invalid argument at %s : %d.\n%s\n", __FILE__, __LINE__, #expr) );     \
        dr = DRM_E_ILWALIDARG;                                                                  \
        ExamineDRValue(dr, __FILE__, __LINE__, #expr);                                          \
        goto ErrorExit;                                                                         \
    }                                                                                           \
} DRM_WHILE_FALSE

#define ChkPtr(expr) DRM_DO {                                                                   \
    if( DRM_UNLIKELY( NULL == (expr) ) )                                                        \
    {                                                                                           \
        DRM_DBG_TRACE( ("NULL pointer at %s : %d.\n%s\n", __FILE__, __LINE__, #expr) );         \
        dr = DRM_E_POINTER;                                                                     \
        ExamineDRValue(dr, __FILE__, __LINE__, #expr);                                          \
        goto ErrorExit;                                                                         \
    }                                                                                           \
} DRM_WHILE_FALSE

#define ChkPtrAndReturn(expr, error) do {                                                       \
    if( DRM_UNLIKELY( NULL == (expr) ) )                                                        \
    {                                                                                           \
        DRM_DBG_TRACE(("NULL pointer at %s : %d.\n%s\n", __FILE__, __LINE__, #expr));           \
        dr = error;                                                                             \
        ExamineDRValue(dr, __FILE__, __LINE__, #expr);                                          \
        goto ErrorExit;                                                                         \
    }                                                                                           \
} DRM_WHILE_FALSE

#define ChkDRMString(s) DRM_DO {                                                                \
    if( !(s) || (s)->pwszString == NULL || (s)->cchString == 0 )                                \
    {                                                                                           \
        DRM_DBG_TRACE( ("Invalid argument at %s : %d.\n%s\n", __FILE__, __LINE__, #s) );        \
        dr = DRM_E_ILWALIDARG;                                                                  \
        ExamineDRValue(dr, __FILE__, __LINE__, #s);                                             \
        goto ErrorExit;                                                                         \
    }                                                                                           \
} DRM_WHILE_FALSE

#define ChkDRMANSIString(s) DRM_DO {                                                            \
    if( !(s) || (s)->pszString == NULL || (s)->cchString == 0 )                                 \
    {                                                                                           \
        DRM_DBG_TRACE( ("Invalid argument at %s : %d.\n%s\n", __FILE__, __LINE__, #s) );        \
        dr = DRM_E_ILWALIDARG;                                                                  \
        ExamineDRValue(dr, __FILE__, __LINE__, #s);                                             \
        goto ErrorExit;                                                                         \
    }                                                                                           \
} DRM_WHILE_FALSE

#define ChkBOOL(fExpr,err) DRM_DO {                             \
    if( !(fExpr) )                                              \
    {                                                           \
        dr = (err);                                             \
        ExamineDRValue(dr, __FILE__, __LINE__, #fExpr);         \
        goto ErrorExit;                                         \
    }                                                           \
} DRM_WHILE_FALSE

#define ChkVOID(fExpr) DRM_DO { \
    fExpr;                      \
} DRM_WHILE_FALSE

#define ChkDRContinue(expr) DRM_DO {                \
    dr=(expr);                                      \
    ExamineDRValue(dr, __FILE__, __LINE__, #expr);  \
    if( DRM_FAILED( dr ) )                          \
    {                                               \
        continue;                                   \
    }                                               \
} DRM_WHILE_FALSE

#define ChkDRMap( expr, drOriginal, drMapped ) DRM_DO { \
    dr = ( expr );                                      \
    ExamineDRValue(dr, __FILE__, __LINE__, #expr);      \
    if( dr == ( drOriginal ) )                          \
    {                                                   \
        dr = ( drMapped );                              \
        ExamineDRValue(dr, __FILE__, __LINE__, #expr);  \
    }                                                   \
    if( DRM_FAILED( dr ) )                              \
    {                                                   \
        goto ErrorExit;                                 \
    }                                                   \
} DRM_WHILE_FALSE

#define MapDR( drOriginal, drMapped ) DRM_DO {                                                                      \
    DRM_DBG_TRACE( ("Error code 0x%X mapped at %s : %d. to 0x%X \n", drOriginal,  __FILE__, __LINE__, drMapped) );  \
    drOriginal = ( drMapped );                                                                                      \
} DRM_WHILE_FALSE

#define AssertLogicError() DRM_DO {     \
    DRMASSERT( FALSE );                 \
    dr = DRM_E_LOGICERR;                \
    goto ErrorExit;                     \
} DRM_WHILE_FALSE

#define AssertChkBOOL(expr) DRM_DO {    \
    DRM_BOOL _f = (expr);               \
    DRMASSERT( _f );                    \
    if( DRM_UNLIKELY( !_f ) )           \
    {                                   \
        dr = DRM_E_LOGICERR;            \
        goto ErrorExit;                 \
    }                                   \
    __analysis_assume( expr );          \
} DRM_WHILE_FALSE

#define AssertChkArg(expr) AssertChkBOOL(expr)

#define AssertChkFeature(expr) DRM_DO {                                                                         \
    DRM_BOOL _f = (expr);                                                                                       \
    if( DRM_UNLIKELY( !_f ) )                                                                                   \
    {                                                                                                           \
        DRM_DBG_TRACE( ( "Incompatible Feature Set Detected at %s : %d.\n%s\n", __FILE__, __LINE__, #expr ) );  \
        DRMASSERT( FALSE );                                                                                     \
        ChkDR( DRM_E_BCERT_ILWALID_FEATURE );                                                                   \
    }                                                                                                           \
} DRM_WHILE_FALSE

#define DRM_REQUIRE_BUFFER_TOO_SMALL( expr ) DRM_DO {   \
    DRM_RESULT __drTemp = (expr);                       \
    if( __drTemp != DRM_E_BUFFERTOOSMALL )              \
    {                                                   \
        ChkDR( __drTemp );                              \
        DRMASSERT( FALSE );                             \
        ChkDR( DRM_E_LOGICERR );                        \
    }                                                   \
} DRM_WHILE_FALSE

#define InitOutputPtr( lhs, rhs ) DRM_DO {  \
    if( (lhs) != NULL )                     \
    {                                       \
        *(lhs) = (rhs);                     \
    }                                       \
} DRM_WHILE_FALSE

#define ChkAndInitOutputPtr( lhs, rhs ) DRM_DO {    \
    ChkArg( (lhs) != NULL );                        \
    *(lhs) = (rhs);                                 \
} DRM_WHILE_FALSE

/*
** To ensure that safe math isn't required for numerous callwlations across various protocols,
** limit the size of things like custom data to no more than ~16 MB.
*/
#define ChkMaxProtocolData( __cb )  ChkArg( ( __cb ) <= ( (DRM_DWORD)0xFFFFFFUL ) )

#define ChkMaxVersionLen( __pSelwreCoreCtx ) DRM_DO {                                               \
    ChkArg( ( __pSelwreCoreCtx ) != NULL );                                                         \
    if( ( __pSelwreCoreCtx )->pClientInfo != NULL )                                                 \
    {                                                                                               \
        ChkMaxProtocolData( ( __pSelwreCoreCtx )->pClientInfo->m_dastrClientVersion.cchString );    \
    }                                                                                               \
} DRM_WHILE_FALSE

//
// LWE (nkuo) -  Helper function colwerting LW FLCN_STATUS error code to PK's DRM_RESULT error code.
// Intentionally declared it as inline so that it can be embedded into both LS and HS code.
//
static DRM_ALWAYS_INLINE DRM_API DRM_RESULT DRM_CALL _colwertFlcnStatusToDrmStatus(FLCN_STATUS f_flcnStatus)
    GCC_ATTRIB_ALWAYSINLINE();

#define ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(errCodeSuffix) \
    case FLCN_ERR_##errCodeSuffix:                         \
        return DRM_E_OEM_##errCodeSuffix;

static DRM_ALWAYS_INLINE DRM_API DRM_RESULT DRM_CALL _colwertFlcnStatusToDrmStatus
(
    __in    FLCN_STATUS    f_flcnStatus
)
{
    switch (f_flcnStatus)
    {
        case FLCN_OK:
        {
            return DRM_SUCCESS;
        }
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_ILWALID_INPUT);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_CHIP_NOT_SUPPORTED);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_UCODE_REVOKED);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_NOT_IN_LSMODE);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_ILWALID_LS_PRIV_LEVEL);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_ILWALID_REGIONCFG);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_PRIV_SEC_DISABLED_ON_PROD);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_SW_FUSING_ALLOWED_ON_PROD);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_INTERNAL_SKU_ON_PROD);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_DEVID_OVERRIDE_ENABLED_ON_PROD);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_INCONSISTENT_PROD_MODES);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_CHK_HUB_ENCRPTION_DISABLED);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_PR_ILLEGAL_LASSAHS_STATE_AT_HS_ENTRY);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_PR_ILLEGAL_LASSAHS_STATE_AT_MPK_DECRYPT);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_PR_ILLEGAL_LASSAHS_STATE_AT_HS_EXIT);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(DPU_TIMEOUT_FOR_HDCP_TYPE1_LOCK_REQUEST);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(DPU_IS_BUSY);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HDCP_TYPE1_LOCK_FAILED);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HDCP_TYPE1_LOCK_UNKNOWN);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_MUTEX_ACQUIRE_FAILED);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(HS_MUTEX_RELEASE_FAILED);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(BAR0_PRIV_WRITE_ERROR);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(BAR0_PRIV_READ_ERROR);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(CSB_PRIV_READ_ERROR);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(CSB_PRIV_WRITE_ERROR);
        ADD_FLCN_STATUS_TO_DRM_STATUS_ENTRY(WAIT_FOR_BAR0_IDLE_FAILED);
    }

    return (DRM_E_OEM_FLCN_STATUS_TO_DRM_STATUS_TRANSLATION_FAILED | f_flcnStatus);
}

//
// Macro to check FALCON return status and colwert to
// Playready defined return passed in "drStatus"
//
#define ChkFlcnStatusToDr(expr, drStatus) DRM_DO {  \
    FLCN_STATUS  flcnStatus;                        \
    flcnStatus = (expr);                            \
    if (flcnStatus != FLCN_OK)                      \
    {                                               \
        dr = drStatus;                              \
        goto ErrorExit;                             \
    }                                               \
    else                                            \
    {                                               \
        dr = DRM_SUCCESS;                           \
    }                                               \
} DRM_WHILE_FALSE

#endif /* __DRMERR_H__ */

