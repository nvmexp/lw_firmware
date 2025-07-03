/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
/*
      File fieldpriv.h.   Version 20 September 2002

      Declarations for field routines which the application program
      doesn't need to know about.
*/

#ifndef __FIELDPRIV_H
#define __FIELDPRIV_H 1
#include "bigpriv.h"
#include "field.h"

ENTER_PK_NAMESPACE;

DRM_API DRM_BOOL DRM_CALL Kequaler_default (const digit_t *, const digit_t *,
                                     const DRM_DWORD, const field_desc_t*, struct bigctx_t *f_pBigCtx);

DRM_API DRM_BOOL DRM_CALL Kfdesc_initialize(field_desc_t*, const DRM_DWORD, struct bigctx_t *f_pBigCtx);

#ifndef USE_FN_PTRS
// LWE (nkuo) - we want to avoid usage of function pointers to help the static analysis
// and that MSFT has signed off on calling the exact functions directly. Since now these
// functions can be called from anywhere, so they can't be declared as static anymore
DRM_API DRM_BOOL DRM_CALL Kprime_adder(const digit_t *, const digit_t *, digit_t *, const DRM_DWORD, const field_desc_t *, struct bigctx_t *);
DRM_API DRM_BOOL DRM_CALL Kprime_mulpower2er(const digit_t *, const DRM_LONG , digit_t *, const DRM_DWORD, const field_desc_t *, struct bigctx_t *);
DRM_API DRM_BOOL DRM_CALL Kprime_negater(const digit_t *, digit_t *, const DRM_DWORD , const field_desc_t *, struct bigctx_t *);
DRM_API DRM_BOOL DRM_CALL Kprime_subtracter(const digit_t *, const digit_t *, digit_t *, const DRM_DWORD, const field_desc_t *, struct bigctx_t *);
DRM_API DRM_BOOL DRM_CALL Kprime_multiplier1(const digit_t *, const digit_t *, digit_t *, const field_desc_t *, digit_t *, struct bigctx_t *);
DRM_API DRM_BOOL DRM_CALL Kprime_ilwerter1(const digit_t *, digit_t *, const field_desc_t *, const digit_tempinfo_t *, struct bigctx_t *);
#endif

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM, "Function signature must match function pointer")
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_BUFFER_PARAM_25033, "Function signature must match function pointer")
DRM_API DRM_BOOL DRM_CALL Kiszeroer_default(const digit_t *, const DRM_DWORD,
                                     const field_desc_t*, struct bigctx_t *f_pBigCtx);
PREFAST_POP /* Function signature must match function pointer */
PREFAST_POP /* Function signature must match function pointer */

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM, "Function signature must match function pointer")
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_BUFFER_PARAM_25033, "Function signature must match function pointer")
DRM_API DRM_BOOL DRM_CALL Kzeroizer_default(digit_t *, const DRM_DWORD,
                                     const field_desc_t*, struct bigctx_t *f_pBigCtx);
PREFAST_POP /* Function signature must match function pointer */
PREFAST_POP /* Function signature must match function pointer */

EXIT_PK_NAMESPACE;

#endif /* __FIELDPRIV_H */
