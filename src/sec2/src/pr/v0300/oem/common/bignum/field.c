/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#define DRM_BUILDING_FIELD_C
#include "bignum.h"
#include "field.h"
#include "mprand.h"
#include "bigpriv.h"
#include "fieldpriv.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#ifdef NONE
/*
        This file (field.c) has the routines in field.h,
        for field arithmetic.
        Arithmetic is defined on GF(q) (q odd prime) or GF(2^m).
        GF(2^m) arithmetic can use normal or polynomial bases over GF(2)
        A field element is an array of type digit_t.

    Arithmetic routines (not necessarily on this file):

        Kadd(f1, f2, f3, &fdesc) -- f3 = f1 + f2
        Kdiv(f1, f2, f3, &fdesc, ftemps) -- f3 = f1 / f2
        Kequal (f1, f2,  &fdesc) -- Is f1 == f2?
        Kimmediate(scalar, f1, &fdesc) -- f1 = scalar (a long)
        Kilwert(f1, f2,  &fdesc, temps) -- f2 = 1/f1, temps supplied
        Kiszero(f1,      &fdesc) -- Is f1 == 0?
        Kmul(f1, f2, f3, &fdesc, temps) -- f3 = f1 * f2, temps supplied
        Kmuladd(f1, f2, f3, f4, &fdesc, ftemps) -- f4 = f1 * f2 + f3
        Knegate(f1, f2,  &fdesc) -- f2 = -f1
        Ksub(f1, f2, f3, &fdesc) -- f3 = f1 - f2

    Miscellaneous routines:

        Kclear_many(f1, nelmt,    &fdesc) -- Set nelmt elements to zero.
        Kfree  (&fdesc)                   -- Free any memory malloc-ed
                                             when field was initialized.
        Kinitialize_prime(&modulus, &modmultemps, &fdesc)
                                       -- Initialize field with prime modulus.
*/



DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kfdesc_initialize
        (field_desc_t *fdesc,                // OUT
         const DRM_DWORD     nalloc,               // IN
         struct bigctx_t *f_pBigCtx)
{
//   Initialize many fields of fdesc,
//   before those for the specific field type are set up by the caller.
//   fdesc->deallocate is set to an array of length nalloc digit_t entities.
//   This array is set to binary zero (not necessarily field zero).
//   For internal use only.

    DRM_BOOL OK = TRUE;

    if (fdesc->deallocate != NULL) {
        OK = FALSE;
        SetMpErrno_clue(MP_ERRNO_ILWALID_DATA,
             "Kfdesc_initialize -- fdesc->deallocate not NULL on entry");
    }

    if (OK) {
        OEM_SELWRE_ZERO_MEMORY(fdesc, sizeof(*fdesc));
                                          // In case something missed below

        fdesc->arithmetic = NULL;
        fdesc->base_field = NULL;
        fdesc->degree = 0;
        fdesc->elng = 0;
        fdesc->freduc = NULL;
        fdesc->free_modulus = FALSE;
        fdesc->ftype = FIELD_TYPE_ILWALID;
        fdesc->ilwerse_adjustment = NULL;
        fdesc->leading_ilwerse = NULL;
        fdesc->lng_mulshifts = 0;
        fdesc->mask_used_bits = DRM_RADIXM1;
        fdesc->minimal_polynomial = NULL;
        fdesc->modulo = NULL;
        fdesc->mulshifts = NULL;
        fdesc->ndigtemps_mul = 0;
        fdesc->ndigtemps_ilwert1 = 0;
        fdesc->ndigtemps_arith = 0x12345678;
        fdesc->nonzero_trace_power = 0;
        fdesc->one = NULL;
        fdesc->T = 0;
        fdesc->unused_bits_top = 0;
    }
    if (OK && nalloc != 0) {
        fdesc->deallocate = digit_allocate(nalloc, f_pBigCtx);
        if (fdesc->deallocate == NULL) {
            OK = FALSE;
        } else {
            OEM_SELWRE_ZERO_MEMORY(fdesc->deallocate,( nalloc)*sizeof( digit_t ));
        }
    }
    return OK;
} // Kfdesc_initialize



DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kfree(field_desc_t *fdesc, struct bigctx_t *f_pBigCtx)
/*
   Free any parts of a field descriptor
   which may have been malloc-ed.
*/
{
    DRM_BOOL OK = TRUE;

    if (fdesc->ftype == FIELD_TYPE_ILWALID) {
        OK = FALSE;
        SetMpErrno_clue(MP_ERRNO_ILWALID_DATA,
                        "Kfree -- argument does not point to active field");
    }

    OK = OK && (fdesc->arithmetic->freer)(fdesc, f_pBigCtx);
                  // Anything dependent on ftype

    if (OK && fdesc->deallocate != NULL) {
        Free_Temporaries(fdesc->deallocate, f_pBigCtx);
        fdesc->deallocate = NULL;
    }

    fdesc->arithmetic = NULL;
    fdesc->ftype = FIELD_TYPE_ILWALID;
    return OK;
} // end Kfree




DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kimmediate
        (const sdigit_t scalar,  // Cast a scalar to an element of the field
         digit_t *f3,
         const field_desc_t *fdesc,
         struct bigctx_t *f_pBigCtx)
{
    DRM_BOOL OK = TRUE;

    OK = OK && Kimmediate_many(&scalar, f3, 1, fdesc, f_pBigCtx);
                                // Treat as one-element array
    return OK;
} /* Kimmediate */
#endif

#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kmul_many
        (const digit_t *f1,     // Set f3 = f1 * f2
         const digit_t *f2,
         digit_t  *f3,
         const DRM_DWORD nelmt,
         const field_desc_t *fdesc,
         digit_t  *supplied_temps,
         struct bigctx_t *f_pBigCtx)
{
    DRM_BOOL OK = TRUE;
    digit_tempinfo_t tempinfo;
    DRM_DWORD i;
    const DRM_DWORD elng = fdesc->elng;

    tempinfo.address = supplied_temps;
    tempinfo.nelmt = fdesc->ndigtemps_mul;
    tempinfo.need_to_free = FALSE;

         // Use user array if supplied.
         // Otherwise allocate our own, if ndigtemps_mul <> 0.

    OK = OK && possible_digit_allocate(&tempinfo, f_pBigCtx);

    for (i = 0; OK && i != elng*nelmt; i += elng) {
#ifdef USE_FN_PTRS
        OK = OK && (fdesc->arithmetic->multiplier1)(f1 + i, f2 + i, f3 + i,
            fdesc, tempinfo.address, f_pBigCtx);
#else
        //
        // LWE (nkuo) - we want to avoid usage of function pointers to help the static analysis
        // and that MSFT has signed off on calling the exact functions directly
        //
        OK = OK && Kprime_multiplier1(f1 + i, f2 + i, f3 + i,
            fdesc, tempinfo.address, f_pBigCtx);
#endif
    }
    if (tempinfo.need_to_free) {
        Free_Temporaries(tempinfo.address, f_pBigCtx);
    }

    if (!OK) {
       TRACE( ( "Kmul_Many error, ftype = %ld, elng = %ld, degree = %ld,"
                "ndigtemps = %ld, supplied_temps = %p\n",
                ( DRM_LONG )fdesc->ftype, ( DRM_LONG )fdesc->elng,
                ( DRM_LONG )fdesc->degree, ( DRM_LONG )fdesc->ndigtemps_mul,
                supplied_temps ) );
    }
    return OK;
} /* Kmul_Many */


DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kequaler_default
        (const digit_t      *f1,
         const digit_t      *f2,        // Is f1 == f2?
         const DRM_DWORD      nelmt,
         const field_desc_t *fdesc,
         struct bigctx_t *f_pBigCtx)
/*
     Test two arrays of field elements for equality, assuming
     each field element has a unique binary representaiton.
*/
{
    return (compare_same(f1, f2, nelmt * fdesc->elng) == 0);
} /* Kequaler_default */


PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM, "Function signature must match function pointer")
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_BUFFER_PARAM_25033, "Function signature must match function pointer")
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kiszeroer_default
        (const digit_t *f1,   // Is f1 == 0?
         const DRM_DWORD nelmt,
         const field_desc_t *fdesc,
         struct bigctx_t *f_pBigCtx)
/*
     Test an array of field elements for zero, assuming the only
     zero field element is a binary zero.
*/
{
    (DRM_VOID)f_pBigCtx;
    return significant_digit_count(f1,  nelmt * fdesc->elng) == 0;
} /* Kiszeroer_default */
PREFAST_POP /* Function signature must match function pointer */
PREFAST_POP /* Function signature must match function pointer */

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM, "Function signature must match function pointer")
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_BUFFER_PARAM_25033, "Function signature must match function pointer")
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Kzeroizer_default
        (digit_t    *f3,
         const DRM_DWORD   nelmt,
         const field_desc_t *fdesc,
         struct bigctx_t *f_pBigCtx)
/*
       This zeros an array of field elements in the
       usual case where the binary zero is also a field zero.
*/
{
    DRM_BOOL OK = TRUE;
    (DRM_VOID)f_pBigCtx;
    OEM_SELWRE_ZERO_MEMORY(f3,( nelmt*fdesc->elng)*sizeof( digit_t ));
    return OK;
} // end Kzeroizer_default
PREFAST_POP /* Function signature must match function pointer */
PREFAST_POP /* Function signature must match function pointer */
#endif
EXIT_PK_NAMESPACE_CODE;
