/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmutilitieslite.h>
#include <drmbytemanip.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_UTL_IncrementID(
    __inout_ecount( 1 ) DRM_ID      *f_pid,
    __in                DRM_BOOL     f_fWrapToOne )
{
    /*
    ** Treat the ID as a 128-bit big-endian unsigned integer value.
    */
    DRM_DWORD   *pdw128     = DRM_REINTERPRET_CAST( DRM_DWORD, f_pid );
    DRM_DWORD   *pdwLowest  = &pdw128[3];
    DRM_DWORD   *pdwLow     = &pdw128[2];
    DRM_DWORD   *pdwHigh    = &pdw128[1];
    DRM_DWORD   *pdwHighest = &pdw128[0];

    DRMASSERT( f_pid != NULL );
    __analysis_assume( f_pid != NULL );

    /* Colwert each DWORD to big endian (if required). */
#if TARGET_LITTLE_ENDIAN
    REVERSE_BYTES_DWORD(*pdwHighest);
    REVERSE_BYTES_DWORD(*pdwHigh);
    REVERSE_BYTES_DWORD(*pdwLow);
    REVERSE_BYTES_DWORD(*pdwLowest);
#endif /* TARGET_LITTLE_ENDIAN */

    if( *pdwLowest != DRM_MAX_UNSIGNED_TYPE( DRM_DWORD ) )
    {
        *pdwLowest = *pdwLowest + 1;
    }
    else if( *pdwLow != DRM_MAX_UNSIGNED_TYPE( DRM_DWORD ) )
    {
        *pdwLowest = 0;
        *pdwLow    = *pdwLow + 1;
    }
    else if( *pdwHigh != DRM_MAX_UNSIGNED_TYPE( DRM_DWORD ) )
    {
        *pdwLowest = 0;
        *pdwLow    = 0;
        *pdwHigh   = *pdwHigh + 1;
    }
    else if( *pdwHighest != DRM_MAX_UNSIGNED_TYPE( DRM_DWORD ) )
    {
        *pdwLowest  = 0;
        *pdwLow     = 0;
        *pdwHigh    = 0;
        *pdwHighest = *pdwHighest + 1;
    }
    else
    {
        if( f_fWrapToOne )
        {
            *pdwLowest  = 1;
        }
        else
        {
            *pdwLowest  = 0;
        }
        *pdwLow     = 0;
        *pdwHigh    = 0;
        *pdwHighest = 0;
    }

    /* Colwert each DWORD back to native endian (if required) */
#if TARGET_LITTLE_ENDIAN
    REVERSE_BYTES_DWORD(*pdwHighest);
    REVERSE_BYTES_DWORD(*pdwHigh);
    REVERSE_BYTES_DWORD(*pdwLow);
    REVERSE_BYTES_DWORD(*pdwLowest);
#endif /* TARGET_LITTLE_ENDIAN */
}
#endif
EXIT_PK_NAMESPACE_CODE;
