/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#ifndef __BIGPRIV_H
#define __BIGPRIV_H 1
/*
      File bigpriv.h.   Version 20 September, 2002

      Declarations accessible to bignum library but invisible to application.
      Also see fieldpriv.h
*/

#include "bignum.h"


ENTER_PK_NAMESPACE;


/*
     Some routines allow the caller to supply temps, but
     accept a NULL argument to say "Allocate them yourself!".
     possible_digit_allocate assists in doing the allocate
     if the caller passed NULL.
*/

typedef struct digit_tempinfo_t {
    digit_t *address;       /* Address supplied by user. */
                            /* Updated to specify address */
                            /* to use for temporaries. */
    DRM_DWORD nelmt;         /* Number of digit_t elements needed */
    DRM_BOOL  need_to_free;  /* Should be set FALSE by application. */
                            /* Changed to TRUE if a free is required. */
} digit_tempinfo_t;

DRM_API DRM_BOOL DRM_CALL possible_digit_allocate(
    __inout_ecount( 1 ) digit_tempinfo_t        *tempinfo,
    __inout             struct bigctx_t         *f_pBigCtx );

DRM_API DRM_BOOL DRM_CALL modmul_choices1(
    __inout_ecount( 1 )     mp_modulus_t *pmodulo,
    __inout_ecount( 1 )     DRM_LONG     *pindex );

DRM_API DRM_BOOL DRM_CALL modmul_from_right_default(
    __in_ecount( pmodulo->length )        const digit_t       *a,
    __in_ecount( pmodulo->length )        const digit_t       *b,
    __out_ecount( pmodulo->length )             digit_t       *c,
    __in_ecount( 1 )                      const mp_modulus_t  *pmodulo,
    __inout_ecount( pmodulo->length * 2 )       digit_t       *temps );

DRM_API DRM_BOOL DRM_CALL modmul_from_right_default_modulo8(
    __in_ecount( pmodulo->length )        const digit_t       *a,
    __in_ecount( pmodulo->length )        const digit_t       *b,
    __out_ecount( pmodulo->length )             digit_t       *c,
    __in_ecount( 1 )                      const mp_modulus_t  *pmodulo,
    __inout_ecount( pmodulo->length * 2 )       digit_t       *temps );

#define LNGQ_MODULO_8  8
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL modmul_from_right_default_modulo8_P256(
    __in_ecount( pmodulo->length )        const digit_t       *a,
    __in_ecount( pmodulo->length )        const digit_t       *b,
    __out_ecount( pmodulo->length )             digit_t       *c,
    __in_ecount( 1 )                      const mp_modulus_t  *pmodulo,
    __inout_ecount( pmodulo->length * 2 )       digit_t       *temps );

EXIT_PK_NAMESPACE;


#endif  /*  __BIGPRIV_H */
