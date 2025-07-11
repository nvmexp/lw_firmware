/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMSAL_H
#define DRMSAL_H

#include <drmcompiler.h>

#if (defined( _PREFAST_ )) || ( defined (DRM_MSC_VER) && ( defined (_M_IX86) || defined (_M_IA64) || defined (_M_AMD64) || defined (_M_ARM) || defined (_M_ARM64) ) )
    /*
    ** Include the Standard Annotation Langauge header for compilers that have it (ie MSVC)
    */
    #include <specstrings.h>

    /*
    ** suppress.h defines certain values twice with two different values!
    ** Since we need the lost values, define our own version here.
    */
    #define __WARNING_ENUM_TYPEDEF_5496                          5496
    #define __WARNING_UNINITIALIZED_MEMORY_6001                  6001
    #define __WARNING_RETURNING_UNINITIALIZED_MEMORY_6101        6101
    #define __WARNING_USING_FAILED_RESULT_6102                   6102
    #define __WARNING_RETURN_FAILED_RESULT_6103                  6103
    #define __WARNING_POTENTIAL_ARGUMENT_TYPE_MISMATCH_6328      6328
    #define __WARNING_READ_OVERRUN_6385                          6385
    #define __WARNING_WRITE_OVERRUN_6386                         6386
    #define __WARNING_MUSTCHECK_ON_VOID_6505                     6505
    #define __WARNING_NONCONST_LOCAL_VARIABLE_25003             25003
    #define __WARNING_NONCONST_PARAM_25004                      25004
    #define __WARNING_STATIC_FUNCTION_25007                     25007
    #define __WARNING_MISSING_OVERRIDE_25014                    25014
    #define __WARNING_DOESNT_OVERRIDE_25015                     25015
    #define __WARNING_POOR_DATA_ALIGNMENT_25021                 25021
    #define __WARNING_DANGEROUS_POINTERCAST_25024               25024
    #define __WARNING_NONCONST_BUFFER_PARAM_25033               25033
    #define __WARNING_FALSE_CONSTANT_EXPRESSION_IN_AND_25038    25038
    #define __WARNING_STRINGCONST_ASSIGNED_TO_NONCONST_25048    25048
    #define __WARNING_COUNT_REQUIRED_FOR_WRITABLE_BUFFER_25057  25057
    #define __WARNING_SUPERFLUOUS_CAST_25059                    25059
    #define __WARNING_USE_WIDE_API_25068                        25068
    #define __WARNING_NO_MEMBERINIT_25070                       25070
    #define __WARNING_NO_MEMBERINIT_25071                       25071
    #define __WARNING_URL_NEEDS_REVIEW_25085                    25085
    #define __WARNING_ENUM_TYPEDEF_25096                        25096
    #define __WARNING_COUNT_REQUIRED_FOR_VOIDPTR_BUFFER_25120   25120
    #define __WARNING_LOCAL_ARRAY_SHOULD_BE_POINTER_25137       25137
    #define __WARNING_WRITABLE_GLOBAL_FUNCTION_POINTER_25143    25143
    #define __WARNING_MISSING_ANNOTATION_25352                  25352

    #ifndef __WARNING_ANSI_APICALL
        #define __WARNING_ANSI_APICALL 38020
    #endif /* __WARNING_ANSI_APICALL */

    #ifndef UNREFERENCED_PARAMETER
        #define UNREFERENCED_PARAMETER(p) (p)
    #endif

#else

    /*
    ** Ensure the SAL tags are a no-op during compilation.
    */
    #undef __ecount
    #undef __bcount
    #undef __in
    #undef __in_ecount
    #undef __in_bcount
    #undef __in_z
    #undef __in_ecount_z
    #undef __in_bcount_z
    #undef __in_nz
    #undef __in_ecount_nz
    #undef __in_bcount_nz
    #undef __out
    #undef __out_ecount
    #undef __out_bcount
    #undef __out_ecount_part
    #undef __out_bcount_part
    #undef __out_ecount_full
    #undef __out_bcount_full
    #undef __out_z
    #undef __out_z_opt
    #undef __out_ecount_z
    #undef __out_bcount_z
    #undef __out_ecount_part_z
    #undef __out_bcount_part_z
    #undef __out_ecount_full_z
    #undef __out_bcount_full_z
    #undef __out_nz
    #undef __out_nz_opt
    #undef __out_ecount_nz
    #undef __out_bcount_nz
    #undef __inout
    #undef __inout_ecount
    #undef __inout_bcount
    #undef __inout_ecount_part
    #undef __inout_bcount_part
    #undef __inout_ecount_full
    #undef __inout_bcount_full
    #undef __inout_z
    #undef __inout_ecount_z
    #undef __inout_bcount_z
    #undef __inout_nz
    #undef __inout_ecount_nz
    #undef __inout_bcount_nz
    #undef __ecount_opt
    #undef __bcount_opt
    #undef __in_opt
    #undef __in_ecount_opt
    #undef __in_bcount_opt
    #undef __in_z_opt
    #undef __in_ecount_z_opt
    #undef __in_bcount_z_opt
    #undef __in_nz_opt
    #undef __in_ecount_nz_opt
    #undef __in_bcount_nz_opt
    #undef __out_opt
    #undef __out_ecount_opt
    #undef __out_bcount_opt
    #undef __out_ecount_part_opt
    #undef __out_bcount_part_opt
    #undef __out_ecount_full_opt
    #undef __out_bcount_full_opt
    #undef __out_ecount_z_opt
    #undef __out_bcount_z_opt
    #undef __out_ecount_part_z_opt
    #undef __out_bcount_part_z_opt
    #undef __out_ecount_full_z_opt
    #undef __out_bcount_full_z_opt
    #undef __out_ecount_nz_opt
    #undef __out_bcount_nz_opt
    #undef __inout_opt
    #undef __inout_ecount_opt
    #undef __inout_bcount_opt
    #undef __inout_ecount_part_opt
    #undef __inout_bcount_part_opt
    #undef __inout_ecount_full_opt
    #undef __inout_bcount_full_opt
    #undef __inout_z_opt
    #undef __inout_ecount_z_opt
    #undef __inout_ecount_z_opt
    #undef __inout_bcount_z_opt
    #undef __inout_nz_opt
    #undef __inout_ecount_nz_opt
    #undef __inout_bcount_nz_opt
    #undef __deref_ecount
    #undef __deref_bcount
    #undef __deref_out
    #undef __deref_out_ecount
    #undef __deref_out_bcount
    #undef __deref_out_ecount_part
    #undef __deref_out_bcount_part
    #undef __deref_out_ecount_full
    #undef __deref_out_bcount_full
    #undef __deref_out_z
    #undef __deref_out_ecount_z
    #undef __deref_out_bcount_z
    #undef __deref_out_nz
    #undef __deref_out_ecount_nz
    #undef __deref_out_bcount_nz
    #undef __deref_inout
    #undef __deref_inout_z
    #undef __deref_inout_ecount
    #undef __deref_inout_bcount
    #undef __deref_inout_ecount_part
    #undef __deref_inout_bcount_part
    #undef __deref_inout_ecount_full
    #undef __deref_inout_bcount_full
    #undef __deref_inout_z
    #undef __deref_inout_ecount_z
    #undef __deref_inout_bcount_z
    #undef __deref_inout_nz
    #undef __deref_inout_ecount_nz
    #undef __deref_inout_bcount_nz
    /* Note: Opt at the end of a __deref TYPE **ppfoo means *ppfoo may be NULL on OUTPUT */
    #undef __deref_ecount_opt
    #undef __deref_bcount_opt
    #undef __deref_out_opt
    #undef __deref_out_ecount_opt
    #undef __deref_out_bcount_opt
    #undef __deref_out_ecount_part_opt
    #undef __deref_out_bcount_part_opt
    #undef __deref_out_ecount_full_opt
    #undef __deref_out_bcount_full_opt
    #undef __deref_out_z_opt
    #undef __deref_out_ecount_z_opt
    #undef __deref_out_bcount_z_opt
    #undef __deref_out_nz_opt
    #undef __deref_out_ecount_nz_opt
    #undef __deref_out_bcount_nz_opt
    #undef __deref_inout_opt
    #undef __deref_inout_ecount_opt
    #undef __deref_inout_bcount_opt
    #undef __deref_inout_ecount_part_opt
    #undef __deref_inout_bcount_part_opt
    #undef __deref_inout_ecount_full_opt
    #undef __deref_inout_bcount_full_opt
    #undef __deref_inout_z_opt
    #undef __deref_inout_ecount_z_opt
    #undef __deref_inout_bcount_z_opt
    #undef __deref_inout_nz_opt
    #undef __deref_inout_ecount_nz_opt
    #undef __deref_inout_bcount_nz_opt
    /* Note: Opt at the start of a __deref TYPE **ppfoo means ppfoo may be NULL on INPUT */
    #undef __deref_opt_in_ecount
    #undef __deref_opt_ecount
    #undef __deref_opt_bcount
    #undef __deref_opt_out
    #undef __deref_opt_out_z
    #undef __deref_opt_out_ecount
    #undef __deref_opt_out_bcount
    #undef __deref_opt_out_ecount_part
    #undef __deref_opt_out_bcount_part
    #undef __deref_opt_out_ecount_full
    #undef __deref_opt_out_bcount_full
    #undef __deref_opt_inout
    #undef __deref_opt_inout_ecount
    #undef __deref_opt_inout_bcount
    #undef __deref_opt_inout_ecount_part
    #undef __deref_opt_inout_bcount_part
    #undef __deref_opt_inout_ecount_full
    #undef __deref_opt_inout_bcount_full
    #undef __deref_opt_inout_z
    #undef __deref_opt_inout_ecount_z
    #undef __deref_opt_inout_bcount_z
    #undef __deref_opt_inout_nz
    #undef __deref_opt_inout_ecount_nz
    #undef __deref_opt_inout_bcount_nz
    #undef __deref_opt_ecount_opt
    #undef __deref_opt_bcount_opt
    #undef __deref_opt_out_opt
    #undef __deref_opt_out_ecount_opt
    #undef __deref_opt_out_bcount_opt
    #undef __deref_opt_out_ecount_part_opt
    #undef __deref_opt_out_bcount_part_opt
    #undef __deref_opt_out_ecount_full_opt
    #undef __deref_opt_out_bcount_full_opt
    #undef __deref_opt_out_z_opt
    #undef __deref_opt_out_ecount_z_opt
    #undef __deref_opt_out_bcount_z_opt
    #undef __deref_opt_out_nz_opt
    #undef __deref_opt_out_ecount_nz_opt
    #undef __deref_opt_out_bcount_nz_opt
    #undef __deref_opt_inout_opt
    #undef __deref_opt_inout_ecount_opt
    #undef __deref_opt_inout_bcount_opt
    #undef __deref_opt_inout_ecount_part_opt
    #undef __deref_opt_inout_bcount_part_opt
    #undef __deref_opt_inout_ecount_full_opt
    #undef __deref_opt_inout_bcount_full_opt
    #undef __deref_opt_inout_z_opt
    #undef __deref_opt_inout_ecount_z_opt
    #undef __deref_opt_inout_bcount_z_opt
    #undef __deref_opt_inout_nz_opt
    #undef __deref_opt_inout_ecount_nz_opt
    #undef __deref_opt_inout_bcount_nz_opt
    #undef __success
    #undef __nullterminated
    #undef __nullnullterminated
    #undef __reserved
    #undef __checkReturn
    #undef __typefix
    #undef __override
    #undef __callback
    #undef __format_string
    #undef __blocksOn
    #undef __control_entrypoint
    #undef __data_entrypoint
    #undef __fallthrough
    #undef __analysis_assume
    #undef _Field_size_bytes_full_opt_
    #undef __drv_freesMem
    #undef _Field_size_full_opt_





    #define __ecount(size)
    #define __bcount(size)
    #define __in
    #define __in_ecount(size)
    #define __in_bcount(size)
    #define __in_z
    #define __in_ecount_z(size)
    #define __in_bcount_z(size)
    #define __in_nz
    #define __in_ecount_nz(size)
    #define __in_bcount_nz(size)
    #define __out
    #define __out_ecount(size)
    #define __out_bcount(size)
    #define __out_ecount_part(size,length)
    #define __out_bcount_part(size,length)
    #define __out_ecount_full(size)
    #define __out_bcount_full(size)
    #define __out_z
    #define __out_z_opt
    #define __out_ecount_z(size)
    #define __out_bcount_z(size)
    #define __out_ecount_part_z(size,length)
    #define __out_bcount_part_z(size,length)
    #define __out_ecount_full_z(size)
    #define __out_bcount_full_z(size)
    #define __out_nz
    #define __out_nz_opt
    #define __out_ecount_nz(size)
    #define __out_bcount_nz(size)
    #define __inout
    #define __inout_ecount(size)
    #define __inout_bcount(size)
    #define __inout_ecount_part(size,length)
    #define __inout_bcount_part(size,length)
    #define __inout_ecount_full(size)
    #define __inout_bcount_full(size)
    #define __inout_z
    #define __inout_ecount_z(size)
    #define __inout_bcount_z(size)
    #define __inout_nz
    #define __inout_ecount_nz(size)
    #define __inout_bcount_nz(size)
    #define __ecount_opt(size)
    #define __bcount_opt(size)
    #define __in_opt
    #define __in_ecount_opt(size)
    #define __in_bcount_opt(size)
    #define __in_z_opt
    #define __in_ecount_z_opt(size)
    #define __in_bcount_z_opt(size)
    #define __in_nz_opt
    #define __in_ecount_nz_opt(size)
    #define __in_bcount_nz_opt(size)
    #define __out_opt
    #define __out_ecount_opt(size)
    #define __out_bcount_opt(size)
    #define __out_ecount_part_opt(size,length)
    #define __out_bcount_part_opt(size,length)
    #define __out_ecount_full_opt(size)
    #define __out_bcount_full_opt(size)
    #define __out_ecount_z_opt(size)
    #define __out_bcount_z_opt(size)
    #define __out_ecount_part_z_opt(size,length)
    #define __out_bcount_part_z_opt(size,length)
    #define __out_ecount_full_z_opt(size)
    #define __out_bcount_full_z_opt(size)
    #define __out_ecount_nz_opt(size)
    #define __out_bcount_nz_opt(size)
    #define __inout_opt
    #define __inout_ecount_opt(size)
    #define __inout_bcount_opt(size)
    #define __inout_ecount_part_opt(size,length)
    #define __inout_bcount_part_opt(size,length)
    #define __inout_ecount_full_opt(size)
    #define __inout_bcount_full_opt(size)
    #define __inout_z_opt
    #define __inout_ecount_z_opt(size)
    #define __inout_ecount_z_opt(size)
    #define __inout_bcount_z_opt(size)
    #define __inout_nz_opt
    #define __inout_ecount_nz_opt(size)
    #define __inout_bcount_nz_opt(size)
    #define __deref_ecount(size)
    #define __deref_bcount(size)
    #define __deref_out
    #define __deref_out_ecount(size)
    #define __deref_out_bcount(size)
    #define __deref_out_ecount_part(size,length)
    #define __deref_out_bcount_part(size,length)
    #define __deref_out_ecount_full(size)
    #define __deref_out_bcount_full(size)
    #define __deref_out_z
    #define __deref_out_ecount_z(size)
    #define __deref_out_bcount_z(size)
    #define __deref_out_nz
    #define __deref_out_ecount_nz(size)
    #define __deref_out_bcount_nz(size)
    #define __deref_inout
    #define __deref_inout_z
    #define __deref_inout_ecount(size)
    #define __deref_inout_bcount(size)
    #define __deref_inout_ecount_part(size,length)
    #define __deref_inout_bcount_part(size,length)
    #define __deref_inout_ecount_full(size)
    #define __deref_inout_bcount_full(size)
    #define __deref_inout_z
    #define __deref_inout_ecount_z(size)
    #define __deref_inout_bcount_z(size)
    #define __deref_inout_nz
    #define __deref_inout_ecount_nz(size)
    #define __deref_inout_bcount_nz(size)
    #define __deref_ecount_opt(size)
    #define __deref_bcount_opt(size)
    #define __deref_out_opt
    #define __deref_out_ecount_opt(size)
    #define __deref_out_bcount_opt(size)
    #define __deref_out_ecount_part_opt(size,length)
    #define __deref_out_bcount_part_opt(size,length)
    #define __deref_out_ecount_full_opt(size)
    #define __deref_out_bcount_full_opt(size)
    #define __deref_out_z_opt
    #define __deref_out_ecount_z_opt(size)
    #define __deref_out_bcount_z_opt(size)
    #define __deref_out_nz_opt
    #define __deref_out_ecount_nz_opt(size)
    #define __deref_out_bcount_nz_opt(size)
    #define __deref_inout_opt
    #define __deref_inout_ecount_opt(size)
    #define __deref_inout_bcount_opt(size)
    #define __deref_inout_ecount_part_opt(size,length)
    #define __deref_inout_bcount_part_opt(size,length)
    #define __deref_inout_ecount_full_opt(size)
    #define __deref_inout_bcount_full_opt(size)
    #define __deref_inout_z_opt
    #define __deref_inout_ecount_z_opt(size)
    #define __deref_inout_bcount_z_opt(size)
    #define __deref_inout_nz_opt
    #define __deref_inout_ecount_nz_opt(size)
    #define __deref_inout_bcount_nz_opt(size)
    #define __deref_opt_in_ecount(size)
    #define __deref_opt_ecount(size)
    #define __deref_opt_bcount(size)
    #define __deref_opt_out
    #define __deref_opt_out_z
    #define __deref_opt_out_ecount(size)
    #define __deref_opt_out_bcount(size)
    #define __deref_opt_out_ecount_part(size,length)
    #define __deref_opt_out_bcount_part(size,length)
    #define __deref_opt_out_ecount_full(size)
    #define __deref_opt_out_bcount_full(size)
    #define __deref_opt_inout
    #define __deref_opt_inout_ecount(size)
    #define __deref_opt_inout_bcount(size)
    #define __deref_opt_inout_ecount_part(size,length)
    #define __deref_opt_inout_bcount_part(size,length)
    #define __deref_opt_inout_ecount_full(size)
    #define __deref_opt_inout_bcount_full(size)
    #define __deref_opt_inout_z
    #define __deref_opt_inout_ecount_z(size)
    #define __deref_opt_inout_bcount_z(size)
    #define __deref_opt_inout_nz
    #define __deref_opt_inout_ecount_nz(size)
    #define __deref_opt_inout_bcount_nz(size)
    #define __deref_opt_ecount_opt(size)
    #define __deref_opt_bcount_opt(size)
    #define __deref_opt_out_opt
    #define __deref_opt_out_ecount_opt(size)
    #define __deref_opt_out_bcount_opt(size)
    #define __deref_opt_out_ecount_part_opt(size,length)
    #define __deref_opt_out_bcount_part_opt(size,length)
    #define __deref_opt_out_ecount_full_opt(size)
    #define __deref_opt_out_bcount_full_opt(size)
    #define __deref_opt_out_z_opt
    #define __deref_opt_out_ecount_z_opt(size)
    #define __deref_opt_out_bcount_z_opt(size)
    #define __deref_opt_out_nz_opt
    #define __deref_opt_out_ecount_nz_opt(size)
    #define __deref_opt_out_bcount_nz_opt(size)
    #define __deref_opt_inout_opt
    #define __deref_opt_inout_ecount_opt(size)
    #define __deref_opt_inout_bcount_opt(size)
    #define __deref_opt_inout_ecount_part_opt(size,length)
    #define __deref_opt_inout_bcount_part_opt(size,length)
    #define __deref_opt_inout_ecount_full_opt(size)
    #define __deref_opt_inout_bcount_full_opt(size)
    #define __deref_opt_inout_z_opt
    #define __deref_opt_inout_ecount_z_opt(size)
    #define __deref_opt_inout_bcount_z_opt(size)
    #define __deref_opt_inout_nz_opt
    #define __deref_opt_inout_ecount_nz_opt(size)
    #define __deref_opt_inout_bcount_nz_opt(size)
    #define __success(expr)
    #define __nullterminated
    #define __nullnullterminated
    #define __reserved
    #define __checkReturn
    #define __typefix(ctype)
    #define __override
    #define __callback
    #define __format_string
    #define __blocksOn(resource)
    #define __control_entrypoint(category)
    #define __data_entrypoint(category)
    #define __fallthrough
    #define __analysis_assume(expr)
    #define _Field_size_bytes_full_opt_(expr)
    #define __drv_freesMem(type) 
    #define _Field_size_full_opt_(size)

#endif

#ifndef DRM_MSC_VER
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(p)
#endif  /* UNREFERENCED_PARAMETER */
#endif  /* DRM_MSC_VER */

#if DRM_API_DEFAULT
#undef DRM_API
#define DRM_API  __checkReturn
#define DRM_API_VOID
#endif /* DRM_API_DEFAULT */


/*
** for DRM_TEE_ APIs DRM_TEE_BYTE_BLOB parameters are not optional
** yet for some their inner pb variables are. This annotation describes that
*/
#define __in_tee_opt __in
#define __inout_tee_opt __inout

#endif  // DRMSAL_H

