/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oembroker.h>

ENTER_PK_NAMESPACE_CODE;

#if DRM_COMPILE_FOR_NORMAL_WORLD

/*
** Synopsis:
**
** This function returns a boolean value to indicate whether it is the TEE implementation
** or the normal world implementation of the OEM broker library.
**
** Arguments: N/A
**
** Returns:
**
** This function returns TRUE for TEE implementation; otherwise, the function returns FALSE.
**
*/
#ifdef NONE
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Oem_Broker_IsTEE( DRM_VOID ) { return FALSE; }
#endif


#elif DRM_COMPILE_FOR_SELWRE_WORLD  /* DRM_COMPILE_FOR_NORMAL_WORLD */

#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Oem_Broker_IsTEE( DRM_VOID ) { return TRUE; }
#endif

#else

#error Both DRM_COMPILE_FOR_NORMAL_WORLD and DRM_COMPILE_FOR_SELWRE_WORLD are set to 0.

#endif /* DRM_COMPILE_FOR_SELWRE_WORLD || DRM_COMPILE_FOR_NORMAL_WORLD */

EXIT_PK_NAMESPACE_CODE;

