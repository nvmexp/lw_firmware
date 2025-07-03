/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMMATHSAFE_H_
#define __DRMMATHSAFE_H_ 1

#include <drmtypes.h>
#include <drmresults.h>
#include <drmdebug.h>
#include <drmerr.h>

ENTER_PK_NAMESPACE;
#define DRM_INLINING_MATHSAFE_SUPPORTED 0
#if DRM_INLINING_MATHSAFE_SUPPORTED
#define DRMMATHSAFEINLINE DRM_ALWAYS_INLINE
#define DRMMATHSAFEINLINE_ATTRIBUTE DRM_ALWAYS_INLINE_ATTRIBUTE
#else /* DRM_INLINING_MATHSAFE_SUPPORTED */
#define DRMMATHSAFEINLINE DRM_NO_INLINE
//#define DRMMATHSAFEINLINE_ATTRIBUTE DRM_NO_INLINE_ATTRIBUTE
#endif /* DRM_INLINING_MATHSAFE_SUPPORTED */

/* Min values */
#define DRM_LONG_MIN        ((DRM_LONG)(-2147483647 - 1))
#define DRM_INT64_MIN       DRM_I64Asgn( 0x80000000, 0 )

/* Max value */
#define DRM_LONG_MAX        ((DRM_LONG)2147483647)
#define DRM_INT64_MAX       DRM_I64Asgn( 0x7FFFFFFF, DRM_MAX_UNSIGNED_TYPE( DRM_DWORD ) )
#define DRM_UINT64_MAX      DRM_UI64HL( DRM_MAX_UNSIGNED_TYPE( DRM_DWORD ), DRM_MAX_UNSIGNED_TYPE( DRM_DWORD ) )

#define DRM_ByteIncOne( arg )       DRM_ByteAdd( (arg), 1, &(arg) )
#define DRM_WordIncOne( arg )       DRM_WordAdd( (arg), 1, &(arg) )
#define DRM_DWordIncOne( arg )      DRM_DWordAddSame( &(arg), 1 )

#define DRM_ByteDecOne( arg )       DRM_ByteSub( (arg), 1, &(arg) )
#define DRM_WordDecOne( arg )       DRM_WordSub( (arg), 1, &(arg) )
#define DRM_DWordDecOne( arg )      DRM_DWordSubSame( &(arg), 1 )

/* LONG -> SIZE_T colwersion */
#define DRM_LongToSizeT     DRM_LongToDWordPtr

/* DWORD_PTR -> BYTE colwersion */
#if DRM_64BIT_TARGET
#define DRM_DWordPtrToByte  DRM_UInt64ToByte
#else /* DRM_64BIT_TARGET */
#define DRM_DWordPtrToByte  DRM_DWordToByte
#endif /* DRM_64BIT_TARGET */

/* DWORD_PTR -> SHORT colwersion */
#if DRM_64BIT_TARGET
#define DRM_DWordPtrToShort DRM_UInt64ToShort
#else /* DRM_64BIT_TARGET */
#define DRM_DWordPtrToShort DRM_DWordToShort
#endif /* DRM_64BIT_TARGET */

/* DWORD_PTR -> WORD colwersion */
#if DRM_64BIT_TARGET
#define DRM_DWordPtrToWord  DRM_UInt64ToWord
#else /* DRM_64BIT_TARGET */
#define DRM_DWordPtrToWord  DRM_DWordToWord
#endif /* DRM_64BIT_TARGET */

/* DWORD_PTR -> LONG colwersion */
#if DRM_64BIT_TARGET
#define DRM_DWordPtrToLong  DRM_UInt64ToLong
#else /* DRM_64BIT_TARGET */
#define DRM_DWORDPtrToLong  DRM_DWordToLong
#endif /* DRM_64BIT_TARGET */

/* DWORD_PTR -> DWORD colwersion */
#if DRM_64BIT_TARGET
#define DRM_DWordPtrToDWord  DRM_UInt64ToDWord
#endif /* DRM_64BIT_TARGET */

/* SIZE_T -> BYTE colwersion */
#define DRM_SizeTToByte     DRM_DWordPtrToByte

/* SIZE_T -> SHORT colwersion */
#define DRM_SizeTToShort    DRM_DWordPtrToShort

/* SIZE_T -> WORD */
#define DRM_SizeTToWord     DRM_DWordPtrToWord

/* SIZE_T -> LONG colwersion */
#define DRM_SizeTToLong     DRM_DWordPtrToLong

/* SIZE_T -> DWORD colwersion */
#define DRM_SizeTToDWord    DRM_DWordPtrToDWord

/* INT64 -> DWORDPTR colwersion */
#if DRM_64BIT_TARGET
#define DRM_Int64ToDWordPtr DRM_Int64ToUInt64
#else /* DRM_64BIT_TARGET */
#define DRM_Int64ToDWordPtr DRM_Int64ToDWord
#endif /* DRM_64BIT_TARGET */

/* INT64 -> SIZE_T colwersion */
#define DRM_Int64ToSizeT       DRM_Int64ToDWordPtr

/* UINT64 -> DWORD_PTR colwersion */
#if !DRM_64BIT_TARGET
#define DRM_UInt64ToDWordPtr  DRM_UInt64ToDWord
#endif /* !DRM_64BIT_TARGET */

/* UINT64 -> SIZE_T colwersion */
#define DRM_UInt64ToSizeT    DRM_UInt64ToDWordPtr

/* DWORD_PTR addition */
/*
** When using DRM_DWordPtrAdd(), the offset should be multiplied
** by the sizeof the object where the pointer points to.
** Ex: A=B+C, where A and B are pointers and C is an offset
** Should be DRM_DWordPtrAdd(B,C*sizeof(*B), A)
*/
#if DRM_64BIT_TARGET
#define DRM_DWordPtrAdd       DRM_UInt64Add
#define DRM_DWordPtrAddSame   DRM_UInt64AddSame
#else /* DRM_64BIT_TARGET */
#define DRM_DWordPtrAdd       DRM_DWordAdd
#define DRM_DWordPtrAddSame   DRM_DWordAddSame
#endif /* DRM_64BIT_TARGET */

/* SIZE_T addition */
#define DRM_SizeTAdd    DRM_DWordPtrAdd

/* DWORD_PTR subtraction */
/*
** When using DRM_DWordPtrSub(), the offset should be multiplied
** by the sizeof the object where the pointer points to.
** Ex: A=B-C, where A and B are pointers and C is an offset
** Should be DRM_DWordPtrSub(B,C*sizeof(*B), A)
*/
#if DRM_64BIT_TARGET
#define DRM_DWordPtrSub    DRM_UInt64Sub
#else /* DRM_64BIT_TARGET */
#define DRM_DWordPtrSub    DRM_DWordSub
#endif /* DRM_64BIT_TARGET */

/* SIZE_T subtraction */
#define DRM_SizeTSub       DRM_DWordPtrSub

/* DWORD_PTR multiplication */
#if DRM_64BIT_TARGET
#define DRM_DWordPtrMult    DRM_UInt64Mult
#else /* DRM_64BIT_TARGET */
#define DRM_DWordPtrMult    DRM_DWordMult
#endif /* DRM_64BIT_TARGET */

/* SIZET multiplicaiton */
#define DRM_SizeTMult       DRM_DWordPtrMult

#if !DRM_INLINING_MATHSAFE_SUPPORTED

/*=============================================================================
** Colwersion functions
**
** There are three reasons for having colwersion functions:
**
** 1. We are colwerting from a signed type to an unsigned type of the same
**    size, or vice-versa.
**
** 2. We are colwerting to a smaller type, and we could therefore possibly
**    overflow.
**
** 3. We are colwerting to a bigger type, and we are signed and the type we are
**    colwerting to is unsigned.
**
**=============================================================================
*/

/* WORD -> BYTE colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_WordToByte(
    __in                      DRM_WORD       f_wOperand,
    __out_ecount(1)           DRM_BYTE      *f_pbResult );

/* LONG -> BYTE colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_LongToByte(
    __in                      DRM_LONG       f_lOperand,
    __out_ecount(1)           DRM_BYTE      *f_pbResult );

/* LONG -> WORD colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_LongToWord(
    __in                      DRM_LONG       f_lOperand,
    __out_ecount(1)           DRM_WORD      *f_pwResult );

/* LONG -> DWORD colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_LongToDWord(
    __in                      DRM_LONG       lOperand,
    __out_ecount(1)           DRM_DWORD     *f_pdwResult );

/* LONG -> DWORD_PTR colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_LongToDWordPtr(
    __in                      DRM_LONG       f_lOperand,
    __out_ecount(1)           DRM_SIZE_T    *f_pdwpResult );

/* DWORD -> BYTE colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_DWordToByte(
    __in                      DRM_DWORD     f_dwOperand,
    __out_ecount(1)           DRM_BYTE     *f_pbResult );

/* DWORD -> WORD colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_DWordToWord(
    __in                      DRM_DWORD     f_dwOperand,
    __out_ecount(1)           DRM_WORD     *f_pwResult );

/* DWORD -> LONG colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_DWordToLong(
    __in                      DRM_DWORD     f_dwOperand,
    __out_ecount(1)           DRM_LONG     *f_plResult );

/* DWORD_PTR -> DWORD colwersion */
#if !DRM_64BIT_TARGET
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_DWordPtrToDWord(
    __in                      DRM_SIZE_T     f_dwpOperand,
    __out_ecount(1)           DRM_DWORD     *f_pdwResult );
#endif /* !DRM_64BIT_TARGET */

/* INT64 -> WORD colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_Int64ToWord(
    __in                      DRM_INT64     f_i64Operand,
    __out_ecount(1)           DRM_WORD     *f_pwResult );

/* INT64 -> BYTE colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_Int64ToByte(
    __in                      DRM_INT64     f_i64Operand,
    __out_ecount(1)           DRM_BYTE     *f_pbResult );

/* INT64 -> LONG colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_Int64ToLong(
    __in                      DRM_INT64     f_i64Operand,
    __out_ecount(1)           DRM_LONG     *f_plResult );

/* INT64 -> DWORD colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_Int64ToDWord(
    __in                      DRM_INT64     f_i64Operand,
    __out_ecount(1)           DRM_DWORD    *f_pdwResult );

/* INT64 -> UINT64 colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_Int64ToUInt64(
    __in                      DRM_INT64     f_i64Operand,
    __out_ecount(1)           DRM_UINT64   *f_pui64Result );

/* UINT64 -> WORD colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_UInt64ToWord(
    __in                      DRM_UINT64     f_ui64Operand,
    __out_ecount(1)           DRM_WORD      *f_pwResult );

/* UINT64 -> BYTE colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_UInt64ToByte(
    __in                      DRM_UINT64     f_ui64Operand,
    __out_ecount(1)           DRM_BYTE      *f_pbResult );

/*  UINT64 -> LONG colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_UInt64ToLong(
    __in                      DRM_UINT64     f_ui64Operand,
    __out_ecount(1)           DRM_LONG      *f_plResult );

/* UINT64 -> DWORD colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_UInt64ToDWord(
    __in                                                                DRM_UINT64     f_ui64Operand,
    __out_ecount( 1 ) _Post_satisfies_( *f_pdwResult == f_ui64Operand ) DRM_DWORD     *f_pdwResult );

/* UINT64 -> DWORD_PTR colwersion */
#if DRM_64BIT_TARGET
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_UInt64ToDWordPtr(
    __in                      DRM_UINT64     f_ui64Operand,
    __out_ecount(1)           DRM_SIZE_T    *f_pdwpResult );
#endif /* DRM_64BIT_TARGET */

/* UINT64 -> INT64 colwersion */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_UInt64ToInt64(
    __in                      DRM_UINT64     f_ui64Operand,
    __out_ecount(1)           DRM_INT64     *f_pi64Result );

/*
** Addition functions
**
*/

/* BYTE addition */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_ByteAdd(
    __in                      DRM_BYTE       f_bAugend,
    __in                      DRM_BYTE       f_bAddend,
    __out_ecount(1)           DRM_BYTE      *f_pbResult );

/* WORD addition */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_WordAdd(
    __in                      DRM_WORD       f_wAugend,
    __in                      DRM_WORD       f_wAddend,
    __out_ecount(1)           DRM_WORD      *f_pwResult );

/* DWORD addition */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_DWordAdd(
    __in                                                                          DRM_DWORD       f_dwAugend,
    __in                                                                          DRM_DWORD       f_dwAddend,
    __out_ecount( 1 ) _Post_satisfies_( *f_pdwResult == f_dwAugend + f_dwAddend ) DRM_DWORD      *f_pdwResult );
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_DWordAddSame(
    __inout_ecount( 1 ) _Post_satisfies_( *f_pdwAugendResult == _Old_( *f_pdwAugendResult ) + f_dwAddend ) DRM_DWORD       *f_pdwAugendResult,
    __in                                                                                                   DRM_DWORD        f_dwAddend );

/* UINT64 addition */
// LWE kwilson
#ifdef NONE
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_UInt64Add(
    __in                                                                                DRM_UINT64       f_ui64Augend,
    __in                                                                                DRM_UINT64       f_ui64Addend,
#if DRM_SUPPORT_NATIVE_64BIT_TYPES
    __out_ecount( 1 ) _Post_satisfies_( *f_pui64Result == f_ui64Augend + f_ui64Addend ) DRM_UINT64      *f_pui64Result )
#else   /* DRM_SUPPORT_NATIVE_64BIT_TYPES */
    __out_ecount( 1 )                                                                   DRM_UINT64      *f_pui64Result );
#endif  /* DRM_SUPPORT_NATIVE_64BIT_TYPES */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_UInt64AddSame(
#if DRM_SUPPORT_NATIVE_64BIT_TYPES
    __inout_ecount( 1 ) _Post_satisfies_( *f_pui64AugendResult == _Old_( *f_pui64AugendResult ) + f_ui64Addend )    DRM_UINT64      *f_pui64AugendResult,
#else   /* DRM_SUPPORT_NATIVE_64BIT_TYPES */
    __inout_ecount( 1 )                                                                                             DRM_UINT64      *f_pui64AugendResult,
#endif  /* DRM_SUPPORT_NATIVE_64BIT_TYPES */
    __in                                                                                                            DRM_UINT64       f_ui64Addend );
#endif

// Function define from 3.o0
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_UInt64Add(
    __in                      DRM_UINT64       f_ui64Augend,
    __in                      DRM_UINT64       f_ui64Addend,
    __out                     DRM_UINT64      *f_pui64Result);

/*
** Subtraction functions
*/

/* BYTE subtraction */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_ByteSub(
    __in                      DRM_BYTE       f_bMinuend,
    __in                      DRM_BYTE       f_bSubtrahend,
    __out_ecount(1)           DRM_BYTE      *f_pbResult );

/* WORD subtraction */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_WordSub(
    __in                      DRM_WORD       f_wMinuend,
    __in                      DRM_WORD       f_wSubtrahend,
    __out_ecount(1)           DRM_WORD      *f_pwResult );

/* DWORD subtraction */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_DWordSub(
    __in                                                                               DRM_DWORD       f_dwMinuend,
    __in                                                                               DRM_DWORD       f_dwSubtrahend,
    __out_ecount( 1 ) _Post_satisfies_( *f_pdwResult == f_dwMinuend - f_dwSubtrahend ) DRM_DWORD      *f_pdwResult );

DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_DWordSubSame(
    __inout_ecount( 1 ) _Post_satisfies_( *f_pdwMinuend == _Old_( *f_pdwMinuend ) - f_dwSubtrahend ) DRM_DWORD      *f_pdwMinuend,
    __in                                                                                             DRM_DWORD       f_dwSubtrahend );

/* UINT64 subtraction */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_UInt64Sub(
    __in                      DRM_UINT64       f_ui64Minuend,
    __in                      DRM_UINT64       f_ui64Subtrahend,
    __out_ecount(1)           DRM_UINT64      *f_pui64Result );

/*
** Multiplication functions
*/

/* WORD multiplication */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_WordMult(
    __in                      DRM_WORD       f_wMultiplicand,
    __in                      DRM_WORD       f_wMultiplier,
    __out_ecount(1)           DRM_WORD      *f_pwResult );

/* DWORD multiplication */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_DWordMult(
    __in                                                                                    DRM_DWORD       f_dwMultiplicand,
    __in                                                                                    DRM_DWORD       f_dwMultiplier,
    __out_ecount( 1 ) _Post_satisfies_( *f_pdwResult == f_dwMultiplicand * f_dwMultiplier ) DRM_DWORD      *f_pdwResult );

/* UINT64 multiplication */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_UInt64Mult(
    __in                      DRM_UINT64       f_ui64Multiplicand,
    __in                      DRM_UINT64       f_ui64Multiplier,
    __out_ecount(1)           DRM_UINT64      *f_pui64Result );

/*
**
** signed operations
**
** Strongly consider using unsigned numbers.
**
** Signed numbers are often used where unsigned numbers should be used.
** For example file sizes and array indices should always be unsigned.
** (File sizes should be 64bit integers; array indices should be SIZE_T.)
** Subtracting a larger positive signed number from a smaller positive
** signed number with IntSubwill succeed, producing a negative number,
** that then must not be used as an array index (but can occasionally be
** used as a pointer index.) Similarly for adding a larger magnitude
** negative number to a smaller magnitude positive number.
**
** drmmathsafe.h does not protect you from such errors. It tells you if your
** integer operations overflowed, not if you are doing the right thing
** with your non-overflowed integers.
**
** Likewise you can overflow a buffer with a non-overflowed unsigned index.
*/

/*
** Signed addition functions
*/

/* LONG Addition */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_LongAdd(
    __in                      DRM_LONG       f_lAugend,
    __in                      DRM_LONG       f_lAddend,
    __out_ecount(1)           DRM_LONG      *f_plResult );


/* INT64 Addition */
/*
** The logic for this 64-bit integer operation is non-trivial
** when compiler optimizations are taken into account.
** We have no need to support it, so we don't. No definition is provided
*/
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_Int64Add(
    __in                      DRM_INT64       f_i64Augend,
    __in                      DRM_INT64       f_i64Addend,
    __out_ecount(1)           DRM_INT64      *f_pi64Result );

/*
** Signed subtraction functions
*/

/* LONG Subtraction */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_LongSub(
    __in                      DRM_LONG       f_lMinuend,
    __in                      DRM_LONG       f_lSubtrahend,
    __out_ecount(1)           DRM_LONG      *f_plResult );

/* INT64 Subtraction */
/*
** The logic for this 64-bit integer operation is non-trivial
** when compiler optimizations are taken into account.
** We have no need to support it, so we don't. No definition is provided
*/
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_Int64Sub(
    __in                      DRM_INT64       f_i64Minuend,
    __in                      DRM_INT64       f_i64Subtrahend,
    __out_ecount(1)           DRM_INT64      *f_pi64Result );

/*
** Signed multiplication functions
*/

/* LONG multiplication */
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_CALL DRM_LongMult(
    __in                      DRM_LONG       f_lMultiplicand,
    __in                      DRM_LONG       f_lMultiplier,
    __out_ecount(1)           DRM_LONG      *f_plResult );

/* INT64 multiplication */
/*
** The logic for this 64-bit integer operation is non-trivial
** when compiler optimizations are taken into account.
** We have no need to support it, so we don't. No definition is provided
*/
DRM_API DRMMATHSAFEINLINE DRM_RESULT DRM_Int64Mult(
    __in                      DRM_INT64       f_i64Multiplicand,
    __in                      DRM_INT64       f_i64Multiplier,
    __out_ecount(1)           DRM_INT64      *f_pi64Result );

#endif /* !DRM_INLINING_MATHSAFE_SUPPORTED */
EXIT_PK_NAMESPACE;

#if DRM_INLINING_MATHSAFE_SUPPORTED
#include "drmmathsafe_impl.h"
#endif /* DRM_INLINING_MATHSAFE_SUPPORTED */

#endif /* __DRMMATHSAFE_H_ */

