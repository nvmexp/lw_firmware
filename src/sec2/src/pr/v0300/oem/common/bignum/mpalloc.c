/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#define DRM_BUILDING_MPALLOC_C
#include "bignum.h"
#include "bigpriv.h"
#include <drmlastinclude.h>
#if defined(SEC_COMPILE)
ENTER_PK_NAMESPACE_CODE;
/*
        File mpalloc.c.   Version 20 November, 2002


        This file has helpers for bignum allocation requests.
        The user must provide

             DRM_VOID* bignum_alloc(DRM_DWORD cblen)
             DRM_VOID  bignum_free(DRM_VOID *pvMem)
             DRM_VOID* bignum_realloc(DRM_VOID * pvMem, DRM_DWORD cblen)

        with semantics matching the standard C
        routines malloc, free, realloc.
        [bignum_realloc is not used as of November, 2002.]

    Application-callable routines and macros:

          digit_allocate(DRM_DWORD nelmt, const DRM_CHAR *name)

                 Allocates nelmt elements of type digit_t.
                 In debug mode, -name- will be used to identify this storage.

          possible_digit_allocate(digit_tempinfo_t *tempinfo,
                                          const DRM_CHAR *name)
                 For internal use only.  The tempinfo struct
                 holds the amount of data needed and a possible
                 address supplied (typically be the caller of the user).
                 Use the supplied address if not NULL,
                 but allocate if the supplied address is NULL.

 */


/*****************************************************************************/
DRM_NO_INLINE DRM_API digit_t* DRM_CALL digit_allocate(
    const DRM_DWORD     nelmt,
    struct bigctx_t     *f_pBigCtx )
/*
   Allocate an array with nelmt digit_t elements.
   May return NULL if unsuccessful.
*/
{
    digit_t *temps = NULL;

    if ((nelmt > 0) &&
        (nelmt*sizeof(digit_t) > nelmt))  /* check for integer underflow/overflow */
    {
        temps = (digit_t*)bignum_alloc(nelmt*sizeof(digit_t), f_pBigCtx);
    }

    if (temps == NULL) {
        TRACE( ( "Cannot allocate %ld digit_t entities\n",
               ( DRM_LONG )nelmt ) );
    }

    return temps;
} // end digit_allocate
/*****************************************************************************/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL possible_digit_allocate(
    digit_tempinfo_t        *tempinfo,
    struct bigctx_t         *f_pBigCtx )
/*
        Look at tempinfo.address.  If this has a non-NULL
        pointer, assume this array has the required space for
        temporaries.  If tempinfo.address is NULL
        and tempinfo.nelmt > 0, then allocate a temporary array,
        storing its address in tempinfo.address, and set tempinfo.need_to_free.
*/
{
    DRM_BOOL OK = TRUE;

    if (tempinfo->need_to_free) {
        OK = FALSE;
        SetMpErrno_clue(MP_ERRNO_ILWALID_DATA,
                        "possible_digit_allocate");
               // Caller is required to clear flag this beforehand.
    }

    if (OK && tempinfo->address == NULL
           && tempinfo->nelmt != 0) {

        tempinfo->address = digit_allocate(tempinfo->nelmt,
                                           f_pBigCtx);
        if (tempinfo->address == NULL) {
            OK = FALSE;
        } else {
            tempinfo->need_to_free = TRUE;
        }
    }
    return OK;
}  // end possible_digit_allocate
/*****************************************************************************/
#endif
EXIT_PK_NAMESPACE_CODE;
