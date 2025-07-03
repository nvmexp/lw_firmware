/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/*      File mprand.h */

#ifndef _MPRAND_H
#define _MPRAND_H

#include "bignum.h"

ENTER_PK_NAMESPACE;

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL random_bytes(
    __out_bcount( nbyte )         DRM_BYTE          *byte_array,
    __in                    const DRM_DWORD          nbyte,
    __inout                       struct bigctx_t   *f_pBigCtx );
         /* Procedure to be supplied by application. */


DRM_API DRM_BOOL DRM_CALL random_digits(
    __out_ecount( lng )       digit_t           *dtArray,
    __in                const DRM_DWORD          lng,
    __inout                   struct bigctx_t   *f_pBigCtx );


/* Following are extern on all platforms */

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL random_digit_interval(
    __in                 const digit_t   dlow,
    __in                 const digit_t   dhigh,
    __out_ecount( 1 )          digit_t  *pdout,
    __out               struct bigctx_t *f_pBigCtx )
GCC_ATTRIB_NO_STACK_PROTECT();

          /* Generate random digit_t or DRM_DWORD in specified closed interval. */

DRM_API DRM_BOOL DRM_CALL random_mod(
    __in_ecount( lng )  const digit_t           *n,
    __out_ecount( lng )       digit_t           *arr,
    __in                const DRM_DWORD          lng,
    __inout                   struct bigctx_t   *f_pBigCtx );

/*
** Checked stublib expects the function name to be new_random_mod_nonzero
** Temporary fix.
*/
#define random_mod_nonzero new_random_mod_nonzero
DRM_API DRM_BOOL DRM_CALL random_mod_nonzero(
    __in_ecount( lng )  const digit_t           *n,
    __out_ecount( lng )       digit_t           *arr,
    __in                const DRM_DWORD          lng,
    __inout                   struct bigctx_t   *f_pBigCtx );
                        /* Generate random value modulo another value.
                           random_mod_nonzero generates a nonzero value. */

EXIT_PK_NAMESPACE;

#endif   /* _MPRAND_H */
