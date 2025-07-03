/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMPRAGMAS_H__
#define __DRMPRAGMAS_H__

#include <drmcompiler.h>

/*
# Abstract:
#
# This file contains compiler directives for disabling certain compiler-time warnings
# and allow the PK environment to be compiled under elevated warning level.
#
# For example, Microsoft compiler can use warning level /W4
#
*/

#ifndef TRUE
    #define TRUE  1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

#if defined (DRM_MSC_VER)

/*
** We need to ignore unknown pragmas if not using prefast.
*/
#ifndef _PREFAST_
    #pragma warning(disable:4068)
#endif

/*
** Pragmas to disable. These warnings are expected.
*/
#pragma warning(disable:4100)  /* unreferenced formal parameter */
#pragma warning(disable:4510)  /* default constructor not possible for structure w/const field */
#pragma warning(disable:4512)  /* assignment operator not possible for struct w/const field */
#pragma warning(disable:4610)  /* struct non-instantiatable due to const field */

/*
** Pragmas to enable.  These warnings are explicitly enabled at level 3.
*/
#pragma warning(3:4101) /* unreferenced local variable */
#pragma warning(3:4152) /* nonstandard extension, function/data pointer colwersion in expression */
#pragma warning(3:4189) /* local variable is initialized but not referenced */
#pragma warning(3:4244) /* colwersion from 'type1' to 'type2', possible loss of data */
#pragma warning(3:4245) /* colwersion from 'type1' to 'type2', signed/unsigned mismatch */
#pragma warning(3:4287) /* 'operator' : unsigned/negative constant mismatch */
#pragma warning(3:4302) /* 'colwersion' : truncation from 'type 1' to 'type 2' */
#pragma warning(3:4310) /* cast truncates constant value */
#pragma warning(3:4365) /* 'action' : colwersion from 'type_1' to 'type_2', signed/unsigned mismatch */
#pragma warning(3:4505) /* unreferenced local function has been removed */
#pragma warning(3:4826) /* Colwersion from 'type1 ' to 'type_2' is sign-extended. This may cause unexpected runtime behavior. */

#if ( DRM_MSC_VER >= 1900 )
#pragma warning(3:4127)         /* "conditional expression is constant" */
#else   /* ( DRM_MSC_VER >= 1900 ) */
#pragma warning(disable:4127)   /* "conditional expression is constant" */
#endif  /* ( DRM_MSC_VER >= 1900 ) */

/*
** Pragmas to disable.  These warnings are expected but are specific to amd64 and ia64.
*/
#if defined(_M_AMD64) || defined(_M_IA64)
#pragma warning(disable:4366) /* The result of the unary operator may be unaligned */
#endif /* _M_AMD64 */

/*
** Disable select PREfast warnings.
*/
#ifdef _PREFAST_
#include <drmsal.h>
__pragma(prefast(disable:__WARNING_ENUM_TYPEDEF_5496,"typedef is required in strict ANSI C"))
__pragma(prefast(disable:__WARNING_ENUM_TYPEDEF_25096,"typedef is required in strict ANSI C"))
__pragma(prefast(disable:__WARNING_POOR_DATA_ALIGNMENT_25021,"Since PlayReady ships as source code, it is more important for types to be human-readable for member sequencing than minimum sized."))
__pragma(prefast(disable:__WARNING_UNUSED_ASSIGNMENT_28931,"Debug code often assigns values to variables which are then never referenced in order to enable viewing in the debugger."))
#endif

#define DRM_DO do

#define DRM_WHILE_FALSE             \
__pragma( warning( push ) )         \
__pragma( warning( disable:4127 ) ) \
   while( FALSE )                   \
__pragma( warning( pop ) )

#define DRM_WHILE_TRUE              \
__pragma( warning( push ) )         \
__pragma( warning( disable:4127 ) ) \
   while( TRUE )                    \
__pragma( warning( pop ) )

#define DRM_FOR_INFINITE for( ;; )

#else /* DRM_MSC_VER */

#define DRM_DO do

#define DRM_WHILE_FALSE while( FALSE )

#define DRM_WHILE_TRUE while( TRUE )

#define DRM_FOR_INFINITE for( ;; )

#endif  /* DRM_MSC_VER */

#endif  /* __DRMPRAGMAS_H__ */

