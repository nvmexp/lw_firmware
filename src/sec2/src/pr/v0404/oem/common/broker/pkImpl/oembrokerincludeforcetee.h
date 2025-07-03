/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#if !defined(DRM_COMPILE_FOR_SELWRE_WORLD) || DRM_COMPILE_FOR_SELWRE_WORLD != 1
#error Attempting to compile secure world broker when DRM_COMPILE_FOR_SELWRE_WORLD != 1
#endif  /* !defined(DRM_COMPILE_FOR_SELWRE_WORLD) || DRM_COMPILE_FOR_SELWRE_WORLD != 1 */

#ifdef __OEMBROKERINCLUDEFORCETEE_H__
#error Attempting to compile secure world broker multiple times
#endif  /* __OEMBROKERINCLUDEFORCETEE_H__ */

#define __OEMBROKERINCLUDEFORCETEE_H__ 1

#include <oembrokerbase.c>
#include <oembrokersha.c>
#include <oembrokertype.c>

#if DRM_BROKER_NEEDS_AES
#include <oembrokeraes.c>
#endif  /* DRM_BROKER_NEEDS_AES */

#ifdef NONE
#if DRM_BROKER_NEEDS_CRITSEC
#include <oembrokercritsec_pk.c>
#endif  /* DRM_BROKER_NEEDS_CRITSEC */

#if DRM_BROKER_NEEDS_ECCSIGN
#include <oembrokereccsign.c>
#endif  /* DRM_BROKER_NEEDS_ECCSIGN */
#endif

#if DRM_BROKER_NEEDS_ECCVERIFY
#include <oembrokereccverify_pk.c>
#endif  /* DRM_BROKER_NEEDS_ECCVERIFY */

