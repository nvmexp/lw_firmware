/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/*
** This file includes the #defines normally set by a profile.
** It should be #included at the top of drmfeatures.h if you
** are not using the provided .mk files.
*/

#ifndef DRMPROFILEOEMMK_H
#define DRMPROFILEOEMMK_H


#define TARGET_LITTLE_ENDIAN 1
#define TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS 0
#define SEC_COMPILE

// LWE (kwilson) Remove this to use default GDK implementation
#define SEC_USE_CERT_TEMPLATE

#ifndef DRM_BUILD_PROFILE
#define DRM_BUILD_PROFILE 10
#endif /* DRM_BUILD_PROFILE */

#ifndef DRM_ACTIVATION_PLATFORM
#define DRM_ACTIVATION_PLATFORM 0
#endif /* DRM_ACTIVATION_PLATFORM */

#ifndef DRM_SUPPORT_FILE_LOCKING
#define DRM_SUPPORT_FILE_LOCKING 0
#endif /* DRM_SUPPORT_FILE_LOCKING */

#ifndef DRM_SUPPORT_MULTI_THREADING
#define DRM_SUPPORT_MULTI_THREADING 0
#endif /* DRM_SUPPORT_MULTI_THREADING */

#ifndef DRM_SUPPORT_ECCPROFILING
#define DRM_SUPPORT_ECCPROFILING 1
#endif /* DRM_SUPPORT_ECCPROFILING */

#ifndef DRM_SUPPORT_INLINEDWORDCPY
#define DRM_SUPPORT_INLINEDWORDCPY 0
#endif /* DRM_SUPPORT_INLINEDWORDCPY */

#ifndef DRM_SUPPORT_DATASTORE_PREALLOC
#define DRM_SUPPORT_DATASTORE_PREALLOC 1
#endif /* DRM_SUPPORT_DATASTORE_PREALLOC */

#ifndef DRM_SUPPORT_REACTIVATION
#define DRM_SUPPORT_REACTIVATION 0
#endif /* DRM_SUPPORT_REACTIVATION */

#ifndef DRM_SUPPORT_NATIVE_64BIT_TYPES
#define DRM_SUPPORT_NATIVE_64BIT_TYPES 1
#endif /* DRM_SUPPORT_NATIVE_64BIT_TYPES */

#ifndef DRM_SUPPORT_FORCE_ALIGN
#define DRM_SUPPORT_FORCE_ALIGN 0
#endif /* DRM_SUPPORT_FORCE_ALIGN */

#ifndef DRM_SUPPORT_ASSEMBLY
#define DRM_SUPPORT_ASSEMBLY 1
#endif /* DRM_SUPPORT_ASSEMBLY */

#ifndef DRM_SUPPORT_PRECOMPUTE_GTABLE
#define DRM_SUPPORT_PRECOMPUTE_GTABLE 0
#endif /* DRM_SUPPORT_PRECOMPUTE_GTABLE */

#ifndef DRM_SUPPORT_SELWREOEMHAL
#define DRM_SUPPORT_SELWREOEMHAL 0
#endif /* DRM_SUPPORT_SELWREOEMHAL */

#ifndef DRM_SUPPORT_SELWREOEMHAL_PLAY_ONLY
#define DRM_SUPPORT_SELWREOEMHAL_PLAY_ONLY 0
#endif /* DRM_SUPPORT_SELWREOEMHAL_PLAY_ONLY */

/********************************************************************************
**
** proxy are all included in the image. This is a TEST only configuration that allows the 
** TEE libraries and the TEE proxy/stub serialization code to be linked into one image.  
** This is used primarily for testing to assure good coverage of the serialization code.
**
********************************************************************************/
#ifndef DRM_SUPPORT_TEE
#define DRM_SUPPORT_TEE 2
#endif /* DRM_SUPPORT_TEE */

#ifndef DRM_SUPPORT_TRACING
#define DRM_SUPPORT_TRACING 0
#endif /* DRM_SUPPORT_TRACING */

#ifndef DRM_SUPPORT_PRIVATE_KEY_CACHE
#define DRM_SUPPORT_PRIVATE_KEY_CACHE 0
#endif /* DRM_SUPPORT_PRIVATE_KEY_CACHE */

#ifndef DRM_SUPPORT_DEVICE_SIGNING_KEY
#define DRM_SUPPORT_DEVICE_SIGNING_KEY 0
#endif /* DRM_SUPPORT_DEVICE_SIGNING_KEY */

#ifndef DRM_SUPPORT_NOLWAULTSIGNING
#define DRM_SUPPORT_NOLWAULTSIGNING 1
#endif /* DRM_SUPPORT_NOLWAULTSIGNING */

#ifndef DRM_USE_IOCTL_HAL_GET_DEVICE_INFO
#define DRM_USE_IOCTL_HAL_GET_DEVICE_INFO 0
#endif /* DRM_USE_IOCTL_HAL_GET_DEVICE_INFO */

#ifndef _DATASTORE_WRITE_THRU
#define _DATASTORE_WRITE_THRU 1
#endif /* _DATASTORE_WRITE_THRU */

#ifndef _ADDLICENSE_WRITE_THRU
#define _ADDLICENSE_WRITE_THRU 0
#endif /* _ADDLICENSE_WRITE_THRU */

#ifndef DRM_HDS_COPY_BUFFER_SIZE
#define DRM_HDS_COPY_BUFFER_SIZE 32768
#endif /* DRM_HDS_COPY_BUFFER_SIZE */

#ifndef DRM_SUPPORT_TOOLS_NET_IO
#define DRM_SUPPORT_TOOLS_NET_IO 0
#endif /* DRM_SUPPORT_TOOLS_NET_IO */

#ifndef DRM_TEST_SUPPORT_ACTIVATION
#define DRM_TEST_SUPPORT_ACTIVATION 0
#endif /* DRM_TEST_SUPPORT_ACTIVATION */

#ifndef USE_PK_NAMESPACES
#define USE_PK_NAMESPACES 0
#endif /* USE_PK_NAMESPACES */

#ifndef DRM_INCLUDE_PK_NAMESPACE_USING_STATEMENT
#define DRM_INCLUDE_PK_NAMESPACE_USING_STATEMENT 0
#endif /* DRM_INCLUDE_PK_NAMESPACE_USING_STATEMENT */

#ifndef DRM_KEYFILE_VERSION
#define DRM_KEYFILE_VERSION 3
#endif /* DRM_KEYFILE_VERSION */

#ifndef DRM_NO_OPT
#define DRM_NO_OPT 0
#endif /* DRM_NO_OPT */


#endif // DRMPROFILEOEMMK_H

