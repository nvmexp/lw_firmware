/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __OEMCOMPILER_H__
#define __OEMCOMPILER_H__

/*
** The default definitions for a number of compiler-specific settings are in drmcompiler.h.
** This file is provided to enable you to change these settings.
*/

/*
** +-----------------------+
** | Turning on Debug Mode |
** +-----------------------+
**
** By default, Debug Mode for the PlayReady PK is turned on iff the DBG macro is defined.
** To turn on Debug Mode based on another macro defined by your compiler, define DRM_DBG to 1 here.
**
** Example:
**   #if MY_COMPILER_IS_COMPILING_A_DEBUG_VERSION
**   #define DRM_DBG 1
**   #endif
*/

/*
** +-----------------------------+
** | Overriding Compiler version |
** +-----------------------------+
**
** If you want the default compiler settings to match the defaults set for Microsoft's compilers,
** define DRM_MSC_VER here instead to act as if _MSC_VER is set to the specified value.
**
** For example, adding the following will set the default drmcompiler.h settings to match MSC version 1300.
**   #define DRM_MSC_VER 1300
**
** If you want the default compiler settings to match the defaults set for a specific version of GNU
** but your compiler does not set the __GNUC__ macro, then set the following two macros to match the
** desired major and minor version of GNU.  These correspond to the __GNUC__ and __GNUC_MINOR__ macros.
**   DRM_GNUC_MAJOR
**   DRM_GNUC_MINOR
**
** For example, adding the following will set the default drmcompiler.h settings to match GNU version 4.6.
**   #define DRM_GNUC_MAJOR 4
**   #define DRM_GNUC_MINOR 6
*/

/*
** +------------------------------+
** | Changing DRM_VOID definition |
** +------------------------------+
**
** If you need to change the definition of DRM_VOID (unlikely), do so here and set DRM_VOID_DEFINED to 1.
*/
#ifndef DRM_VOID_DEFINED
#define DRM_VOID_DEFINED 0
#endif /* DRM_VOID_DEFINED */

/*
** +--------------------------------------------+
** | Changing PRAGMA WARNING MACROS definitions |
** +--------------------------------------------+
**
** If you need to set the definition of any of the following macros,
** define ALL of them here and set DRM_PRAGMA_WARNING_MACROS_DEFINED to 1.
**
**  PRAGMA_INTRINSIC(func)
**  PRAGMA_WARNING_DEFAULT(warningnum)
**  PRAGMA_WARNING_DISABLE(warningnum)
**  PRAGMA_WARNING_PUSH
**  PRAGMA_WARNING_POP
**  PRAGMA_WARNING_PUSH_WARN(warningnum)
**  PRAGMA_PACK_PUSH_VALUE(packval)
**  PRAGMA_PACK_POP
**  PRAGMA_DIAG_OFF(x, reason)
**  PRAGMA_DIAG_ON(x)
*/
#ifndef DRM_PRAGMA_WARNING_MACROS_DEFINED
#define DRM_PRAGMA_WARNING_MACROS_DEFINED 0
#endif /* DRM_PRAGMA_WARNING_MACROS_DEFINED */

/*
** +----------------------------+
** | 64-BIT TARGET ARCHITECTURE |
** +----------------------------+
**
** If you want to guarantee that the PlayReady PK is compiling for a 32-bit or 64-bit architecture,
** set the following macros here, where DRM_64BIT_TARGET should be set to 0 for a
** 32-bit architecture or 1 for a 64-bit architecture.
**
** #define DRM_64BIT_TARGET_DEFINED 1
** #define DRM_64BIT_TARGET 1
*/
#ifndef DRM_64BIT_TARGET_DEFINED
#define DRM_64BIT_TARGET_DEFINED 0
#endif /* DRM_64BIT_TARGET_DEFINED */

/*
** +------------------------------------------------------------+
** | Changing GS STRICT and DBG OPTIMIZATION PRAGMA definitions |
** +------------------------------------------------------------+
**
** If you need to set the definition of any of the following macros,
** define ALL of them here and set DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED to 1.
**
** PRAGMA_STRICT_GS_PUSH_ON
** PRAGMA_STRICT_GS_POP
** PRAGMA_DBG_OPTIMIZATION_OFF
** PRAGMA_DBG_OPTIMIZATION_ON
** PRAGMA_ARM_OPTIMIZATION_OFF
** PRAGMA_ARM_OPTIMIZATION_ON
** PRAGMA_GCC_OPTIMIZATION_OFF
** PRAGMA_GCC_OPTIMIZATION_ON
*/
#ifndef DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED
#define DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED 0
#endif /* DRM_STRICT_AND_OPTIMIZATION_PRAGMAS_DEFINED */

/*
** +--------------------------------------------+
** | Changing COMPILATION QUALIFIER DEFINITIONS |
** +--------------------------------------------+
**
** If you need to set the definition of any of the following macros,
** define ALL of them here and set DRM_COMPILATION_QUALIFIERS_DEFINED to 1.
**
** DRM_CCALL
** DRM_DLLEXPORT
** DRM_DLLIMPORT
** DRM_ALIGN_4
** DRM_ALIGN_8
** DRM_PACKED
** DRM_DISCARDABLE
*/
#ifndef DRM_COMPILATION_QUALIFIERS_DEFINED
#define DRM_COMPILATION_QUALIFIERS_DEFINED 0
#endif /* DRM_COMPILATION_QUALIFIERS_DEFINED */

/*
** +------------------------------+
** | Setting COMPILATION BEHAVIOR |
** +------------------------------+
**
** If you need to set the definition of any of the following macros,
** define ALL of them here and set DRM_COMPILATION_BEHAVIOR_DEFINED to 1.
**
** Note that this is *required* unless either DRM_MSC_VER (potentially via _MSC_VER)
** or DRM_GNUC_MAJOR (potentially via __GNUC__) is defined.
**
** DRM_NO_INLINE_ATTRIBUTE
** DRM_ALWAYS_INLINE_ATTRIBUTE
** DRM_CALL
** DRM_ALWAYS_INLINE
** DRM_EXTERN_INLINE
** DRM_EXPORTED_INLINE
** DRM_INLINING_SUPPORTED
** DRM_DWORD_ALIGN
** DRM_NO_INLINE
*/
#ifndef DRM_COMPILATION_BEHAVIOR_DEFINED
#define DRM_COMPILATION_BEHAVIOR_DEFINED 0
#endif /* DRM_COMPILATION_BEHAVIOR_DEFINED */

/*
** +----------------+
** | Setting TARGET |
** +----------------+
**
** If drmcompiler.h is unable to determine the target for which you are compiling
** or you find that it is determining the platform incorrectly, you must set the
** following two macros to 0 or 1 to indicate the endianness of the platform and
** whether the platform supports unaligned pointers pointing to 32 bit values respectively.
**
** TARGET_LITTLE_ENDIAN
** TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS
*/

/*
** +-------------------------------------+
** | Setting BASIC TYPE/SIZE DEFINITIONS |
** +-------------------------------------+
**
** The PlayReady PK requires its basic types to have specific constant sizes on all platforms.
** If any of these types has an incorrect size, a build break will occur in drmdebug.inc.
** To resolve this build break, set DRM_BASIC_TYPE_SIZES_DEFINED to 1 and typedef all of
** the following types to native types such that they have the sign and size indicated.
**
**   Type              Sign       Size      Notes
**   ----              ----       ----      -----
** DRM_BYTE          unsigned      1
** DRM_CHAR            signed      1        Used to represent an ANSI character.
** DRM_WORD          unsigned      2
** DRM_WCHAR         unsigned      2        Used to represent a UTF 16 character in little endian.  For example, only typedef as wchar_t if wchar_t represents UTF16.
** DRM_BOOL          unsigned      4
** DRM_DWORD         unsigned      4
** DRM_WCHAR32       unsigned      4        Used to represent a UTF 32 character in little endian.  Only used by unicode colwersion code.
** DRM_LONG            signed      4
** DRM_WCHAR_NATIVE  unsigned      2        Used to represent a UTF 16 character in native platform endianness.
*/
#ifndef DRM_BASIC_TYPE_SIZES_DEFINED
#define DRM_BASIC_TYPE_SIZES_DEFINED 0
#endif /* DRM_BASIC_TYPE_SIZES_DEFINED */

/*
** +---------------------------------+
** | Setting 64-BIT TYPE DEFINITIONS |
** +---------------------------------+
**
** If your platform does NOT support native 64-bit types, you should set
** DRM_SUPPORT_NATIVE_64BIT_TYPES to 0 in your compilation profile
** and leave this section unchanged.
**
** Otherwise, your platform supports native 64-bit types (e.g. int64_t and uint64_t),
** and you should set DRM_SUPPORT_NATIVE_64BIT_TYPES in your compilation profile.
**
** If you do this but you get a build break because your compiler is not recognized
** by drmcompiler.h, set DRM_NATIVE_64BIT_TYPES_DEFINED to 1 and typedef all of
** the following types to native types such that they have the sign and size indicated.
**
** Note that a 64-bit architecture (i.e. DRM_64BIT_TARGET is set to 1) requires
** native support for 64-bit types.  
**
**   Type              Sign       Size
**   ----              ----       ----
** DRM_UINT64        unsigned      8
** DRM_INT64           signed      8
*/
#ifndef DRM_NATIVE_64BIT_TYPES_DEFINED
#define DRM_NATIVE_64BIT_TYPES_DEFINED 0
#endif /* DRM_NATIVE_64BIT_TYPES_DEFINED */

#endif   /* __OEMCOMPILER_H__ */

