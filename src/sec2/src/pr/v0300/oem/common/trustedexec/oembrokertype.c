/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oembroker.h>

ENTER_PK_NAMESPACE_CODE;
#if defined(SEC_COMPILE)
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
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Oem_Broker_IsTEE( DRM_VOID ) { return TRUE; }
#endif
EXIT_PK_NAMESPACE_CODE;

