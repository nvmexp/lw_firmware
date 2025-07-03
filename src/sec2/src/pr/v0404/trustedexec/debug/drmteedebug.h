/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef _DRMTEEBASEDEBUG_H_
#define _DRMTEEBASEDEBUG_H_ 1

#include <drmteetypes.h>

ENTER_PK_NAMESPACE;

#if DRM_DBG

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_BASE_DEBUG_Init( DRM_VOID );
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_BASE_DEBUG_DeInit( DRM_VOID );
DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_BASE_TRACE_LogStr( __in_z const DRM_CHAR *f_pszToLog );
DRM_API_VOID DRM_VOID DRM_CALL DRM_TEE_IMPL_BASE_TRACE_LogHex( __in DRM_DWORD f_dwToLog );
DRM_API DRM_DWORD DRM_CALL DRM_TEE_IMPL_BASE_TRACE_LogDec( __in DRM_DWORD f_dwToLog );

#else   /* DRM_DBG */

#define DRM_TEE_IMPL_BASE_DEBUG_Init()
#define DRM_TEE_IMPL_BASE_DEBUG_DeInit()
#define DRM_TEE_IMPL_BASE_TRACE_LogStr( f_pszToLog )
#define DRM_TEE_IMPL_BASE_TRACE_LogHex( f_dwToLog )
#define DRM_TEE_IMPL_BASE_TRACE_LogDec( f_dwToLog )

#endif /* DRM_DBG */

EXIT_PK_NAMESPACE;

#endif /* _DRMTEEBASEDEBUG_H_ */

