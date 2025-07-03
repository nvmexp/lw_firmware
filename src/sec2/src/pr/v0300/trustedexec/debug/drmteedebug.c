/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmtee.h>
#include <drmteedebug.h>
#include <oemtee.h>
#include <oemcommon.h>
#include <oembyteorder.h>
#include <oemteeproxy.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;


#ifdef NONE
#if DRM_DBG
/*
** We are going to log into a cirlwlar buffer such that we have
** one or more messages separated by one or more '\0' characters.
** Note that the last character of the buffer is ALWAYS '\0'.
** A caller *in this file* ilwokes:
**  1. _BeginMessage with the number of chars in the message
**  2. One or more static _Log* methods in any sequence
**  3. _EndMessage
**
** A caller outside this file can only call a single DRM_TEE_BASE_TRACE_Log* method.
** (There is lwrrently no support for formatted messages for code outside this file.)
**
** Here is the resultant cirlwlar log buffer data and updated offset
** after a set of messages assuming that sizeof(g_rgbDebugCirlwlarLog) == 10.
**
** Note that the caller of DRM_TEE_BASE_GetDebugInformation has to understand this format.
** Specifically, there can be multiple embedded '\0' chars that should be skipped.
**
**  LOG hello
**  h e l l o 0 0 0 0 0
**  6
**
**  LOG mor
**  h e l l o 0 m o r 0
**  0
**
**  LOG foo
**  f o o 0 0 0 m o r 0
**  4
**
**  LOG XY
**  f o o 0 X Y 0 0 0 0
**  7
**
**  LOG bla
**  b l a 0 X Y 0 0 0 0
**  4
**
**  LOG BCDEF
**  b l a 0 B C D E F 0
**  0
**
**  LOG J
**  J 0 0 0 B C D E F 0
**  2
**
**  LOG abcdefgh
**  a b c d e f g h 0 0
**  9
**
**  LOG n
**  n 0 0 0 0 0 0 0 0 0
**  2
**
**  LOG PQRSTUVWX.YZ    [truncated, ".XY" removed]
**  P Q R S T U V W X 0
**  0
*/

/* 0x12345678 -> 10 chars and 2^32-1 -> 4294967295 -> 10 chars, so 10 chars works for both hex and decimal */
#define DRM_TEE_DEBUG_TRACE_CHARS_PER_NUMBER   10

void( *g_pfDebugAnalyzeDROrig )( unsigned long, const char*, unsigned long, const char* ) = NULL;
static DRM_BYTE     g_rgbDebugCirlwlarLog[OEM_TEE_PROXY_OUTPUT_LENGTH__MAX] = {0};
static DRM_DWORD    g_ichDebugCirlwlarLog   = 0;
static DRM_RESULT   g_drLast                = DRM_SUCCESS;

static DRM_DWORD DRM_CALL _GetStrLen( __in_z const DRM_CHAR *f_psz )
{
    DRM_DWORD cch = 0;
    while( *f_psz != '\0' )
    {
        f_psz++;
        cch++;
    }
    return cch;
}

static DRM_VOID DRM_CALL _BeginMessage( __in DRM_DWORD f_cch )
{
    /* Never use jump macros while holding the critical section. */
    // 
    // LWE (kwilson) - The function below (OEM_TEE_BASE_CRITSEC_Enter) has been modified by 
    // Lwpu to return DRM_RESULT.  However, the surrounding macro below has not been changed from
    // ChkVOID to ChkDr in the interest of minimizing the number of diffs since the current function
    // is not being compiled into Lwpu TEE.
    //     
    ChkVOID(OEM_TEE_BASE_CRITSEC_Enter());

    if( f_cch >= sizeof( g_rgbDebugCirlwlarLog ) )
    {
        f_cch = sizeof(g_rgbDebugCirlwlarLog) - 1;
    }

    if( f_cch >= sizeof(g_rgbDebugCirlwlarLog) - g_ichDebugCirlwlarLog )
    {
        /* Restart at beginning of buffer */
        while( g_ichDebugCirlwlarLog < sizeof(g_rgbDebugCirlwlarLog) )
        {
            /* Zero rest of buffer so we don't have partial messages. */
            g_rgbDebugCirlwlarLog[g_ichDebugCirlwlarLog++] = '\0';
        }
        g_ichDebugCirlwlarLog = 0;
    }
}

static DRM_VOID DRM_CALL _EndMessage( DRM_VOID )
{
    g_rgbDebugCirlwlarLog[ g_ichDebugCirlwlarLog ] = '\0';
    g_ichDebugCirlwlarLog++;
    DRMASSERT( g_ichDebugCirlwlarLog <= sizeof( g_rgbDebugCirlwlarLog ) );
    if( g_ichDebugCirlwlarLog == sizeof( g_rgbDebugCirlwlarLog ) )
    {
        g_ichDebugCirlwlarLog = 0;
    }

    // 
    // LWE (kwilson) - The function below (OEM_TEE_BASE_CRITSEC_Leave) has been modified by 
    // Lwpu to return DRM_RESULT.  However, the surrounding macro below has not been changed from
    // ChkVOID to ChkDr in the interest of minimizing the number of diffs since the current function
    // is not being compiled into Lwpu TEE.
    //     
    ChkVOID(OEM_TEE_BASE_CRITSEC_Leave());
}

static DRM_VOID DRM_CALL _LogStrImpl( __in_z const DRM_CHAR *f_pszToLog, __in DRM_DWORD f_cch )
{
    DRMASSERT( g_ichDebugCirlwlarLog < sizeof( g_rgbDebugCirlwlarLog ) );
    if( f_pszToLog != NULL )
    {
        DRM_DWORD  ich   = 0;

        /*
        ** Truncate the message if it is longer than the cirlwlar buffer. The function _BeginMessage will ensure
        ** that if the message is larger than the remaining buffer we start at the beginning of the cirlwlar
        ** buffer, but it does not verify the length of the message is less than the cirlwlar buffer.
        */
        if( f_cch > sizeof(g_rgbDebugCirlwlarLog) - g_ichDebugCirlwlarLog )
        {
            f_cch = sizeof(g_rgbDebugCirlwlarLog) - g_ichDebugCirlwlarLog;
        }
        
        ChkVOID( OEM_TEE_MEMCPY( &g_rgbDebugCirlwlarLog[ g_ichDebugCirlwlarLog ], f_pszToLog, f_cch ) );
        g_ichDebugCirlwlarLog += f_cch;

        if( g_ichDebugCirlwlarLog < sizeof( g_rgbDebugCirlwlarLog ) )
        {
            /* We may have written partially into a previous log message.  Zero it so we don't have partial messages. */
            ich = g_ichDebugCirlwlarLog;
            while( g_rgbDebugCirlwlarLog[ ich ] != '\0' )
            {
                g_rgbDebugCirlwlarLog[ ich++ ] = '\0';
                DRMASSERT( ich < sizeof( g_rgbDebugCirlwlarLog ) );
            }
        }
        else
        {
            g_ichDebugCirlwlarLog = 0;
            g_rgbDebugCirlwlarLog[sizeof(g_rgbDebugCirlwlarLog)-1] = '\0';
        }
    }
}

DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_TRACE_LogStr( __in_z const DRM_CHAR *f_pszToLog )
{
    DRM_DWORD cch;
    cch = _GetStrLen( f_pszToLog );
    ChkVOID( _BeginMessage( cch ) );
    ChkVOID( _LogStrImpl( f_pszToLog, cch ) );
    ChkVOID( _EndMessage() );
}

static DRM_VOID DRM_CALL _LogHexImpl( __in DRM_DWORD f_dwToLog )
{
    DRM_CHAR szLog[ DRM_TEE_DEBUG_TRACE_CHARS_PER_NUMBER + 1 ] = { 0 };
    DRM_CHAR *psz = &szLog[ DRM_NO_OF( szLog ) - 2 ];     /* Start at end of buffer */
    while( f_dwToLog != 0 )
    {
        DRM_BYTE bVal = f_dwToLog & 15;
        if( bVal < 10 )
        {
            *psz-- = '0' + bVal;
        }
        else
        {
            *psz-- = 'A' + bVal - 10;
        }
        f_dwToLog >>= 4;
    }
    while( psz >= szLog )
    {
        /* Pad with zeroes so it's always of form 0x12345678 */
        *psz-- = '0';
    }
    szLog[ 1 ] = 'x';
    _LogStrImpl( szLog, DRM_TEE_DEBUG_TRACE_CHARS_PER_NUMBER );   /* Whole buffer is always used */
}

DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_TRACE_LogHex( __in DRM_DWORD f_dwToLog )
{
    ChkVOID( _BeginMessage( DRM_TEE_DEBUG_TRACE_CHARS_PER_NUMBER ) );
    ChkVOID( _LogHexImpl( f_dwToLog ) );
    ChkVOID( _EndMessage() );
}

static DRM_DWORD DRM_CALL _LogDecImpl( __in DRM_DWORD f_dwToLog, __in DRM_BOOL f_fLog )
{
    DRM_CHAR szLog[ DRM_TEE_DEBUG_TRACE_CHARS_PER_NUMBER + 1 ] = { 0 };
    DRM_CHAR *psz = &szLog[ DRM_NO_OF( szLog ) - 2 ];     /* Start at end of buffer */
    DRM_DWORD cch = 0;
    if( f_dwToLog == 0 )
    {
        *psz = '0';
    }
    else
    {
        while( f_dwToLog != 0 )
        {
            DRM_BYTE bVal = f_dwToLog % 10;
            *psz-- = '0' + bVal;
            f_dwToLog /= 10;
        }
        psz++;
    }
    cch = _GetStrLen( psz );
    if( f_fLog )
    {
        _LogStrImpl( psz, cch );
    }
    return cch;
}

DRM_API DRM_DWORD DRM_CALL DRM_TEE_BASE_TRACE_LogDec( __in DRM_DWORD f_dwToLog )
{
    DRM_DWORD cch = 0;
    ChkVOID( _BeginMessage( _LogDecImpl( f_dwToLog, FALSE ) ) );
    cch = _LogDecImpl( f_dwToLog, TRUE );
    ChkVOID( _EndMessage() );
    return cch;
}

static void _DebugAnalyzeDRFunc( unsigned long f_ulDR, const char *f_pszFile, unsigned long f_ulLine, const char *f_pszExpr )
{
    DRM_DWORD cchFile = 0;
    DRM_DWORD cchExpr = 0;
    {
        void( *pfDebugAnalyzeDROrigLocal )( unsigned long, const char*, unsigned long, const char* ) = NULL;

        {
            /* Never use jump macros while holding the critical section. */
            // 
            // LWE (kwilson) - The functions below (OEM_TEE_BASE_CRITSEC_Enter/Leave) have been modified by 
            // Lwpu to return DRM_RESULT.  However, the surrounding macros below have not been changed from
            // ChkVOID to ChkDr in the interest of minimizing the number of diffs since the current function
            // is not being compiled into Lwpu TEE.
            //     
            ChkVOID(OEM_TEE_BASE_CRITSEC_Enter());
            pfDebugAnalyzeDROrigLocal = g_pfDebugAnalyzeDROrig;
            ChkVOID( OEM_TEE_BASE_CRITSEC_Leave() );
        }

        if( pfDebugAnalyzeDROrigLocal != NULL )
        {
            DRMASSERT( pfDebugAnalyzeDROrigLocal != _DebugAnalyzeDRFunc );
            pfDebugAnalyzeDROrigLocal( f_ulDR, f_pszFile, f_ulLine, f_pszExpr );
        }
    }

    switch( f_ulDR )
    {
    case DRM_SUCCESS:
    case DRM_E_BUFFERTOOSMALL:
    case DRM_S_FALSE:
        /* no-op: Do not log these results */
        break;
    default:
        cchFile = _GetStrLen( f_pszFile );
        cchExpr = _GetStrLen( f_pszExpr );

        ChkVOID( _BeginMessage( 12 + cchFile + _LogDecImpl( (DRM_DWORD)f_ulLine, FALSE ) + cchExpr + DRM_TEE_DEBUG_TRACE_CHARS_PER_NUMBER ) );

        /* Each comment indicates how its length was factored into the overall message length in the call to _BeginMessage above. */
        ChkVOID( _LogStrImpl( "ERROR: ", 7 ) );                 /* 7 */
        ChkVOID( _LogStrImpl( f_pszFile, cchFile ) );           /* cchFile */
        ChkVOID( _LogStrImpl( "@", 1 ) );                       /* 1 */
        (DRM_VOID)_LogDecImpl( (DRM_DWORD)f_ulLine, TRUE );     /* _LogDecImpl( (DRM_DWORD)f_ulLine, FALSE ) */
        ChkVOID( _LogStrImpl( " (", 2 ) );                      /* 2 */
        ChkVOID( _LogStrImpl( f_pszExpr, cchExpr ) );           /* cchExpr */
        ChkVOID( _LogStrImpl( ")=", 2 ) );                      /* 2 */
        ChkVOID( _LogHexImpl( (DRM_DWORD)f_ulDR ) );            /* DRM_TEE_DEBUG_TRACE_CHARS_PER_NUMBER */

        g_drLast = (DRM_RESULT)f_ulDR;

        ChkVOID( _EndMessage( ) );
        break;
    }
}

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_IMPL_DEBUG_Init( DRM_VOID )
{
    /* Never use jump macros while holding the critical section. */
    // 
    // LWE (kwilson) - The functions below (OEM_TEE_BASE_CRITSEC_Enter/Leave) have been modified by 
    // Lwpu to return DRM_RESULT.  However, the surrounding macros below have not been changed from
    // ChkVOID to ChkDr in the interest of minimizing the number of diffs since the current function
    // is not being compiled into Lwpu TEE.
    //     
    ChkVOID(OEM_TEE_BASE_CRITSEC_Enter());
    if( g_pfDebugAnalyzeDROrig == NULL )
    {
        g_pfDebugAnalyzeDROrig = GetDbgAnalyzeFunction();
        if( g_pfDebugAnalyzeDROrig == _DebugAnalyzeDRFunc )
        {
            g_pfDebugAnalyzeDROrig = NULL;
        }
    }
    ChkVOID( SetDbgAnalyzeFunction( _DebugAnalyzeDRFunc ) );
    ChkVOID( OEM_TEE_BASE_CRITSEC_Leave() );
}

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_BASE_IMPL_DEBUG_DeInit( DRM_VOID )
{
    /* Never use jump macros while holding the critical section. */
    // 
    // LWE (kwilson) - The functions below (OEM_TEE_BASE_CRITSEC_Enter/Leave) have been modified by 
    // Lwpu to return DRM_RESULT.  However, the surrounding macros below have not been changed from
    // ChkVOID to ChkDr in the interest of minimizing the number of diffs since the current function
    // is not being compiled into Lwpu TEE.
    //     
    ChkVOID(OEM_TEE_BASE_CRITSEC_Enter());
    ChkVOID( SetDbgAnalyzeFunction( g_pfDebugAnalyzeDROrig ) );
    g_pfDebugAnalyzeDROrig = NULL;
    ChkVOID( OEM_TEE_ZERO_MEMORY( g_rgbDebugCirlwlarLog, sizeof( g_rgbDebugCirlwlarLog ) ) );
    g_drLast = DRM_SUCCESS;
    ChkVOID( OEM_TEE_BASE_CRITSEC_Leave() );
}

#endif /* DRM_DBG */
#endif
/*
** Synopsis:
**
** This function gets debug information from the TEE.
** It returns DRM_E_NOTIMPL on non-DBG builds.
** This is determined at compile time.
**
** Operations Performed:
**
** #if DRM_DBG
**  1. Verify the signature on the given DRM_TEE_CONTEXT.
**  2. Return debug information.
** #else
**  1. Return DRM_E_NOTIMPL.
** #endif
**
** Arguments:
**
** f_pContext:     (in/out) The TEE context returned from
**                          DRM_TEE_BASE_AllocTEEContext.
** f_pdwLastHR:    (out)    The last error code encountered by the TEE.
** f_pLog:         (out)    Log information from the TEE.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_BASE_GetDebugInformation(
    __inout                     DRM_TEE_CONTEXT              *f_pContext,
    __out                       DRM_DWORD                    *f_pdwLastHR,
    __out                       DRM_TEE_BYTE_BLOB            *f_pLog )
{
#if DRM_DBG
    DRM_RESULT        dr    = DRM_SUCCESS;
    DRM_TEE_BYTE_BLOB oLog  = DRM_TEE_BYTE_BLOB_EMPTY;

    DRMASSERT( f_pContext  != NULL );
    DRMASSERT( f_pdwLastHR != NULL );
    DRMASSERT( f_pLog != NULL );

    {
        /* Never use jump macros while holding the critical section. */
        // 
        // LWE (kwilson) - The functions below (OEM_TEE_BASE_CRITSEC_Enter/Leave) have been modified by 
        // Lwpu to return DRM_RESULT.  However, the surrounding macros below have not been changed from
        // ChkVOID to ChkDr in the interest of minimizing the number of diffs since the current function
        // is not being compiled into Lwpu TEE.
        //     
        ChkVOID(OEM_TEE_BASE_CRITSEC_Enter());

        dr = DRM_TEE_BASE_AllocBlob(
            f_pContext,
            DRM_TEE_BLOB_ALLOC_BEHAVIOR_NEW,
            sizeof( g_rgbDebugCirlwlarLog ),
            NULL,
            &oLog );

        if( DRM_SUCCEEDED( dr ) )
        {
            /* Ensure the oldest messages appear first in our output. */
            ChkVOID( OEM_TEE_MEMCPY( (DRM_BYTE*)oLog.pb, &g_rgbDebugCirlwlarLog[ g_ichDebugCirlwlarLog ], sizeof(g_rgbDebugCirlwlarLog)-g_ichDebugCirlwlarLog ) );
            ChkVOID( OEM_TEE_MEMCPY( (DRM_BYTE*)&oLog.pb[ sizeof(g_rgbDebugCirlwlarLog)-g_ichDebugCirlwlarLog ], g_rgbDebugCirlwlarLog, g_ichDebugCirlwlarLog ) );

            ChkVOID( DRM_TEE_BASE_TransferBlobOwnership( f_pLog, &oLog ) );
            *f_pdwLastHR = (DRM_DWORD)g_drLast;

            ChkVOID( DRM_TEE_BASE_FreeBlob( f_pContext, &oLog ) );
        }

        ChkVOID( OEM_TEE_BASE_CRITSEC_Leave() );
    }

    return dr;
#else   /* DRM_DBG */
    return DRM_E_NOTIMPL;
#endif  /* DRM_DBG */
}

EXIT_PK_NAMESPACE_CODE;

