/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMSAL_H__
#define __DRMSAL_H__

#include <drmcompiler.h>

#if (defined( _PREFAST_ )) || ( defined (DRM_MSC_VER) && ( defined (_M_IX86) || defined (_M_IA64) || defined (_M_AMD64) || defined (_M_ARM) || defined (_M_ARM64) ) )
    /*
    ** Values that used to be defined in suppress.h, for back compat
    */
    #define __WARNING_ENUM_TYPEDEF_5496                                  5496
    #define __WARNING_UNINITIALIZED_MEMORY_6001                          6001
    #define __WARNING_DEREFERENCING_NULL_POINTER_6011                    6011
    #define __WARNING_RETURN_VALUE_IGNORED_6031                          6031
    #define __WARNING_ZERO_TERMINATION_MISSING_6054                      6054
    #define __WARNING_RETURNING_UNINITIALIZED_MEMORY_6101                6101
    #define __WARNING_USING_FAILED_RESULT_6102                           6102
    #define __WARNING_RETURN_FAILED_RESULT_6103                          6103
    #define __WARNING_ILWALID_HRESULT_COMPARE_6219                       6219
    #define __WARNING_ILWALID_HRESULT_COMPARE_TO_INTEGER_6221            6221
    #define __WARNING_LOCAL_DECLARATION_HIDES_GLOBAL_6244                6244
    #define __WARNING_IGNORED_BY_COMMA_6319                              6319
    #define __WARNING_EXCEPTION_EXELWTE_HANDLER_6320                     6320
    #define __WARNING_EXCEPT_BLOCK_EMPTY_6322                            6322
    #define __WARNING_CONSTANT_CONSTANT_COMPARISON_6326                  6326
    #define __WARNING_POTENTIAL_ARGUMENT_TYPE_MISMATCH_6328              6328
    #define __WARNING_POTENTIAL_ARGUMENT_TYPE_MISMATCH_6340              6340
    #define __WARNING_READ_OVERRUN_6385                                  6385
    #define __WARNING_WRITE_OVERRUN_6386                                 6386
    #define __WARNING_ILWALID_PARAMETER_6387                             6387
    #define __WARNING_ILWALID_PARAMETER_6388                             6388
    #define __WARNING_MUSTCHECK_ON_VOID_6505                             6505
    #define __WARNING_ENCODE_GLOBAL_FUNCTION_POINTER_22110              22110
    #define __WARNING_PREDICTABLE_FUNCTION_POINTER_22112                22112
    #define __WARNING_IMPLICIT_CTOR_25001                               25001
    #define __WARNING_NONCONST_LOCAL_VARIABLE_25003                     25003
    #define __WARNING_NONCONST_PARAM_25004                              25004
    #define __WARNING_NONCONST_FUNCTION_25005                           25005
    #define __WARNING_STATIC_FUNCTION_25007                             25007
    #define __WARNING_MISSING_OVERRIDE_25014                            25014
    #define __WARNING_DOESNT_OVERRIDE_25015                             25015
    #define __WARNING_SAME_MEMBER_25020                                 25020
    #define __WARNING_POOR_DATA_ALIGNMENT_25021                         25021
    #define __WARNING_DANGEROUS_POINTERCAST_25024                       25024
    #define __WARNING_UNSAFE_STRING_FUNCTION_25025                      25025
    #define __WARNING_FUNCTION_NEEDS_REVIEW_25028                       25028
    #define __WARNING_HRESULT_NOT_CHECKED_25031                         25031
    #define __WARNING_NONCONST_LOCAL_BUFFER_PTR_25032                   25032
    #define __WARNING_NONCONST_BUFFER_PARAM_25033                       25033
    #define __WARNING_TRUE_CONSTANT_EXPRESSION_IN_AND_25037             25037
    #define __WARNING_FALSE_CONSTANT_EXPRESSION_IN_AND_25038            25038
    #define __WARNING_IF_CONDITION_IS_ALWAYS_TRUE_25041                 25041
    #define __WARNING_IF_CONDITION_IS_ALWAYS_FALSE_25042                25042
    #define __WARNING_LOCAL_BSTR_SHOULD_BE_CONST_WCHAR_PTR_25043        25043
    #define __WARNING_USE_SELECT_ANY_25046                              25046
    #define __WARNING_STRINGCONST_ASSIGNED_TO_NONCONST_25048            25048
    #define __WARNING_COUNT_REQUIRED_FOR_WRITABLE_BUFFER_25057          25057
    #define __WARNING_CAST_CAN_BE_CONST_25058                           25058
    #define __WARNING_SUPERFLUOUS_CAST_25059                            25059
    #define __WARNING_BACKWARD_JUMP_25061                               25061
    #define __WARNING_USE_WIDE_API_25068                                25068
    #define __WARNING_NO_MEMBERINIT_25070                               25070
    #define __WARNING_NO_MEMBERINIT_BEFORE_CONSTRUCTOR_BODY_25071       25071
    #define __WARNING_WRONG_MEMBERINIT_ORDER_25073                      25073
    #define __WARNING_URL_NEEDS_REVIEW_25085                            25085
    #define __WARNING_SD_REQUIRED_FOR_NAMED_OBJECT_25086                25086
    #define __WARNING_FORMAL_CAN_BE_BOOL_25093                          25093
    #define __WARNING_ENUM_TYPEDEF_25096                                25096
    #define __WARNING_INTEGRAL_CAST_TO_OBJECT_WITH_VTABLE_25098         25098
    #define __WARNING_DEPRECATED_LANGUAGE_TYPE_USED_25113               25113
    #define __WARNING_COUNT_REQUIRED_FOR_VOIDPTR_BUFFER_25120           25120
    #define __WARNING_POSSIBLE_STRCPY_LOOP_25126                        25126
    #define __WARNING_IMPLICIT_TEMPLATECTOR_25134                       25134
    #define __WARNING_LOCAL_ARRAY_SHOULD_BE_POINTER_25137               25137
    #define __WARNING_PRINTF_NEEDS_REVIEW_25141                         25141
    #define __WARNING_WRITABLE_GLOBAL_FUNCTION_POINTER_25143            25143
    #define __WARNING_MISSING_ANNOTATION_25352                          25352
    #define __WARNING_DEPRECATED_OVERRIDE_25359                         25359
    #define __WARNING_OBSOLETE_OVERRIDE_25360                           25360
    #define __WARNING_BUFFER_OVERFLOW_26000                             26000
    #define __WARNING_BUFFER_UNDERFLOW_26001                            26001
    #define __WARNING_READ_UNTRACKED_BUFFER_26002                       26002
    #define __WARNING_WRITE_UNTRACKED_BUFFER_26003                      26003
    #define __WARNING_ZERO_LENGTH_ARRAY_26005                           26005
    #define __WARNING_INCORRECT_ANNOTATION_26007                        26007
    #define __WARNING_POTENTIAL_OVERFLOW_26010                          26010
    #define __WARNING_POTENTIAL_UNDERFLOW_26011                         26011
    #define __WARNING_UNTRACKED_BUFFER_UNANNOTATABLE_26012              26012
    #define __WARNING_COMPLEX_EXPRESSION_26013                          26013
    #define __WARNING_INCORRECT_VALIDATION_26014                        26014
    #define __WARNING_POTENTIAL_OVERFLOW_26015                          26015
    #define __WARNING_POTENTIAL_BUFFER_OVERFLOW_NULLTERMINATED_26016    26016
    #define __WARNING_POTENTIAL_OVERFLOW_26017                          26017
    #define __WARNING_POTENTIAL_BUFFER_OVERFLOW_NULLTERMINATED_26018    26018
    #define __WARNING_INCORRECT_VALIDATION_26019                        26019
    #define __WARNING_POTENTIAL_OVERFLOW_26025                          26025
    #define __WARNING_PRECONDITION_NULLTERMINATION_VIOLATION_26035      26035
    #define __WARNING_POSTCONDITION_NULLTERMINATION_VIOLATION_26036     26036
    #define __WARNING_POTENTIAL_NULLTERMINATION_VIOLATION_26037         26037
    #define __WARNING_POTENTIAL_POSTCONDITION_BUFFER_OVERFLOW_26040     26040
    #define __WARNING_INCORRECT_VALIDATION_POSTCONDITION_26044          26044
    #define __WARNING_HIGH_PRIORITY_OVERFLOW_POSTCONDITION_26045        26045
    #define __WARNING_IRREDUCIBLE_CONTROL_FLOW_GRAPH_26051              26051
    #define __WARNING_POTENTIALLY_UNCONSTRAINED_CALL_26052              26052
    #define __WARNING_POTENTIAL_OVERFLOW_LOOP_DEPENDENT_26053           26053
    #define __WARNING_PRECONDITION_RANGE_VIOLATION_26060                26060
    #define __WARNING_POSTCONDITION_RANGE_VIOLATION_26061               26061
    #define __WARNING_POTENTIAL_RANGE_PRECONDITION_VIOLATION_26070      26070
    #define __WARNING_POTENTIAL_RANGE_POSTCONDITION_VIOLATION_26071     26071
    #define __WARNING_CALLER_FAILING_TO_HOLD_LOCK_26110                 26110
    #define __WARNING_MISSING_LOCK_HELD_ANNOTATION_26130                26130
    #define __WARNING_MISSING_LOCK_ANNOTATION_26135                     26135
    #define __WARNING_FAILING_TO_RELEASE_LOCK_26165                     26165
    #define __WARNING_FAILING_TO_HOLD_LOCK_26166                        26166
    #define __WARNING_RELEASING_UNHELD_LOCK_26167                       26167
    #define __WARNING_INCORRECT_ANNOTATION_STRING_ASSUMED_26706         26706
    #define __WARNING_INCORRECT_ANNOTATION_26707                        26707
    #define __WARNING_PRECONDITION_NULLTERMINATION_VIOLATION_26735      26735
    #define __WARNING_EXPR_NOT_TRUE_AT_THIS_CALL_28020                  28020
    #define __WARNING_INCONSISTENT_ANNOTATION_28052                     28052
    #define __WARNING_CONSIDER_USING_ANOTHER_FUNCTION_28159             28159
    #define __WARNING_DEREFERENCING_COPY_OF_NULL_POINTER_28182          28182
    #define __WARNING_POINTER_COPY_COULD_BE_NULL_28183                  28183
    #define __WARNING_REQUIREMENT_NOT_SATISFIED_EXPR_NOT_TRUE_28196     28196
    #define __WARNING_USING_POSSIBLY_UNINITIALIZED_MEMORY_28199         28199
    #define __WARNING_INCONSISTENT_ANNOTATED_OVERRIDDEN_FUNCTION_28204  28204
    #define __WARNING_UNMATCHED_ANNO_TREE_28218                         28218
    #define __WARNING_INCONSISTENT_ANNOTATION_28251                     28251
    #define __WARNING_INCONSISTENT_ANNOTATION_28252                     28252
    #define __WARNING_INCONSISTENT_ANNOTATION_28253                     28253
    #define __WARNING_SPEC_ILWALID_SYNTAX2_28285                        28285
    #define __WARNING_SPEC_ILWALID_SYNTAX2_28286                        28286
    #define __WARNING_NO_ANNOTATIONS_FOR_FIRST_DECLARATION_28301        28301
    #define __WARNING_NO_SAL_VERSION_28310                              28310
    #define __WARNING_OBSOLETE_SAL_VERSION_28311                        28311
    #define __WARNING_OBSOLETE_SAL_VERSION_ON_DEF_28312                 28312
    #define __WARNING_UNANNOTATED_BUFFER_28718                          28718
    #define __WARNING_BANNED_API_28719                                  28719
    #define __WARNING_BANNED_API_28726                                  28726
    #define __WARNING_BANNED_API_28727                                  28727
    #define __WARNING_CYCLOMATIC_COMPLEXITY_28734                       28734
    #define __WARNING_BANNED_STRLEN_API_28750                           28750
    #define __WARNING_BANNED_CRYPTO_API_USAGE_28755                     28755
    #define __WARNING_REDUNDANT_POINTER_TEST_28922                      28922
    #define __WARNING_UNUSED_POINTER_ASSIGNMENT_28930                   28930
    #define __WARNING_UNUSED_ASSIGNMENT_28931                           28931
    #define __WARNING_UNREACHABLE_CODE_28940                            28940
    #define __WARNING_USE_HTTPS_30100                                   30100
    #define __WARNING_VARIANTCLEAR_NOSAL_33006                          33006
    #define __WARNING_CALLING_SET_THREAD_LOCALE_38010                   38010
    #define __WARNING_ANSI_APICALL_38020                                38020
    #define __WARNING_CLIPBOARD_ANSI_38022                              38022
    #define __WARNING_DOMAIN_NAMES_MUST_BE_WORLD_READY_38026            38026

    /*
    ** Include the Standard Annotation Langauge header for compilers that have it (ie MSVC)
    */
    PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_USE_SELECT_ANY_25046, "Ignore warnings in non-PlayReady headers" )
    #include <specstrings.h>
    PREFAST_POP     /* __WARNING_USE_SELECT_ANY_25046 */

    #ifndef __WARNING_ANSI_APICALL
        #define __WARNING_ANSI_APICALL 38020
    #endif /* __WARNING_ANSI_APICALL */

    #ifndef UNREFERENCED_PARAMETER
        #define UNREFERENCED_PARAMETER(p) (p)
    #endif


    #define PREFAST_PUSH_IGNORE_WARNINGS_IN_NON_PLAYREADY_HEADERS                                                                                    \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "Ignore warnings in non-PlayReady headers" )                                 \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_POOR_DATA_ALIGNMENT_25021, "Ignore warnings in non-PlayReady headers" )                            \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_DANGEROUS_POINTERCAST_25024, "Ignore warnings in non-PlayReady headers" )                          \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_UNSAFE_STRING_FUNCTION_25025, "Ignore warnings in non-PlayReady headers" )                         \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_FUNCTION_NEEDS_REVIEW_25028, "Ignore warnings in non-PlayReady headers" )                          \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_HRESULT_NOT_CHECKED_25031, "Ignore warnings in non-PlayReady headers" )                            \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_LOCAL_BUFFER_PTR_25032, "Ignore warnings in non-PlayReady headers" )                      \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_BUFFER_PARAM_25033, "Ignore warnings in non-PlayReady headers" )                          \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_TRUE_CONSTANT_EXPRESSION_IN_AND_25037, "Ignore warnings in non-PlayReady headers" )                \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_IF_CONDITION_IS_ALWAYS_TRUE_25041, "Ignore warnings in non-PlayReady headers" )                    \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_LOCAL_BSTR_SHOULD_BE_CONST_WCHAR_PTR_25043, "Ignore warnings in non-PlayReady headers" )           \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_USE_SELECT_ANY_25046, "Ignore warnings in non-PlayReady headers" )                                 \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_CAST_CAN_BE_CONST_25058, "Ignore warnings in non-PlayReady headers" )                              \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NO_MEMBERINIT_BEFORE_CONSTRUCTOR_BODY_25071, "Ignore warnings in non-PlayReady headers" )          \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_FORMAL_CAN_BE_BOOL_25093, "Ignore warnings in non-PlayReady headers" )                             \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_ENUM_TYPEDEF_25096, "Ignore warnings in non-PlayReady headers" )                                   \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_DEPRECATED_LANGUAGE_TYPE_USED_25113, "Ignore warnings in non-PlayReady headers" )                  \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_POTENTIAL_BUFFER_OVERFLOW_NULLTERMINATED_26018, "Ignore warnings in non-PlayReady headers" )       \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_PRECONDITION_NULLTERMINATION_VIOLATION_26035, "Ignore warnings in non-PlayReady headers" )         \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_POSTCONDITION_NULLTERMINATION_VIOLATION_26036, "Ignore warnings in non-PlayReady headers" )        \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_HIGH_PRIORITY_OVERFLOW_POSTCONDITION_26045, "Ignore warnings in non-PlayReady headers" )           \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_EXPR_NOT_TRUE_AT_THIS_CALL_28020, "Ignore warnings in non-PlayReady headers" )                     \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_CONSIDER_USING_ANOTHER_FUNCTION_28159, "Ignore warnings in non-PlayReady headers" )                \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_REQUIREMENT_NOT_SATISFIED_EXPR_NOT_TRUE_28196, "Ignore warnings in non-PlayReady headers" )        \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_SPEC_ILWALID_SYNTAX2_28285, "Ignore warnings in non-PlayReady headers" )                           \
        PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_BANNED_API_28727, "Ignore warnings in non-PlayReady headers" )

    #define PREFAST_POP_IGNORE_WARNINGS_IN_NON_PLAYREADY_HEADERS                                                                                     \
        PREFAST_POP     /* __WARNING_BANNED_API_28727 */                                                                                             \
        PREFAST_POP     /* __WARNING_SPEC_ILWALID_SYNTAX2_28285 */                                                                                   \
        PREFAST_POP     /* __WARNING_REQUIREMENT_NOT_SATISFIED_EXPR_NOT_TRUE_28196 */                                                                \
        PREFAST_POP     /* __WARNING_CONSIDER_USING_ANOTHER_FUNCTION_28159 */                                                                        \
        PREFAST_POP     /* __WARNING_EXPR_NOT_TRUE_AT_THIS_CALL_28020 */                                                                             \
        PREFAST_POP     /* __WARNING_HIGH_PRIORITY_OVERFLOW_POSTCONDITION_26045 */                                                                   \
        PREFAST_POP     /* __WARNING_POSTCONDITION_NULLTERMINATION_VIOLATION_26036 */                                                                \
        PREFAST_POP     /* __WARNING_PRECONDITION_NULLTERMINATION_VIOLATION_26035 */                                                                 \
        PREFAST_POP     /* __WARNING_POTENTIAL_BUFFER_OVERFLOW_NULLTERMINATED_26018 */                                                               \
        PREFAST_POP     /* __WARNING_DEPRECATED_LANGUAGE_TYPE_USED_25113 */                                                                          \
        PREFAST_POP     /* __WARNING_ENUM_TYPEDEF_25096 */                                                                                           \
        PREFAST_POP     /* __WARNING_FORMAL_CAN_BE_BOOL_25093 */                                                                                     \
        PREFAST_POP     /* __WARNING_NO_MEMBERINIT_BEFORE_CONSTRUCTOR_BODY_25071 */                                                                  \
        PREFAST_POP     /* __WARNING_CAST_CAN_BE_CONST_25058 */                                                                                      \
        PREFAST_POP     /* __WARNING_USE_SELECT_ANY_25046 */                                                                                         \
        PREFAST_POP     /* __WARNING_LOCAL_BSTR_SHOULD_BE_CONST_WCHAR_PTR_25043 */                                                                   \
        PREFAST_POP     /* __WARNING_IF_CONDITION_IS_ALWAYS_TRUE_25041 */                                                                            \
        PREFAST_POP     /* __WARNING_TRUE_CONSTANT_EXPRESSION_IN_AND_25037 */                                                                        \
        PREFAST_POP     /* __WARNING_NONCONST_BUFFER_PARAM_25033 */                                                                                  \
        PREFAST_POP     /* __WARNING_NONCONST_LOCAL_BUFFER_PTR_25032 */                                                                              \
        PREFAST_POP     /* __WARNING_HRESULT_NOT_CHECKED_25031 */                                                                                    \
        PREFAST_POP     /* __WARNING_FUNCTION_NEEDS_REVIEW_25028 */                                                                                  \
        PREFAST_POP     /* __WARNING_UNSAFE_STRING_FUNCTION_25025 */                                                                                 \
        PREFAST_POP     /* __WARNING_DANGEROUS_POINTERCAST_25024 */                                                                                  \
        PREFAST_POP     /* __WARNING_POOR_DATA_ALIGNMENT_25021 */                                                                                    \
        PREFAST_POP     /* __WARNING_NONCONST_PARAM_25004 */



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
    #undef __analysis_assume
    #undef _In_count_
    #undef _Out_opt_cap_
    #undef _Deref_pre_z_
    #undef _Deref_pre_opt_z_
    #undef _Deref_post_opt_z_
    #undef _Deref_post_opt_count_
    #undef _Deref_prepost_z_
    #undef _Acquires_lock_
    #undef _Releases_lock_
    #undef __in_xcount
    #undef _Deref_pre_maybenull_

    #undef __drv_freesMem
    #undef __fallthrough
    #undef _At_
    #undef _At_buffer_
    #undef _When_
    #undef _Success_
    #undef _On_failure_
    #undef _Always_
    #undef _Const_
    #undef _In_
    #undef _In_opt_
    #undef _In_z_
    #undef _In_opt_z_
    #undef _In_reads_
    #undef _In_reads_opt_
    #undef _In_reads_bytes_
    #undef _In_reads_bytes_opt_
    #undef _In_reads_or_z_
    #undef _In_reads_or_z_opt_
    #undef _Out_
    #undef _Out_opt_
    #undef _Out_writes_
    #undef _Out_writes_opt_
    #undef _Out_writes_bytes_
    #undef _Out_writes_bytes_opt_
    #undef _Out_writes_z_
    #undef _Out_writes_opt_z_
    #undef _Out_writes_to_
    #undef _Out_writes_to_opt_
    #undef _Out_writes_all_
    #undef _Out_writes_all_opt_
    #undef _Out_writes_bytes_to_
    #undef _Out_writes_bytes_to_opt_
    #undef _Out_writes_bytes_all_
    #undef _Out_writes_bytes_all_opt_
    #undef _Inout_
    #undef _Inout_opt_
    #undef _Inout_z_
    #undef _Inout_opt_z_
    #undef _Inout_updates_
    #undef _Inout_updates_opt_
    #undef _Inout_updates_z_
    #undef _Inout_updates_opt_z_
    #undef _Inout_updates_to_
    #undef _Inout_updates_to_opt_
    #undef _Inout_updates_bytes_
    #undef _Inout_updates_bytes_opt_
    #undef _Outptr_
    #undef _Outptr_result_maybenull_
    #undef _Outptr_opt_
    #undef _Outptr_opt_result_maybenull_
    #undef _Outptr_result_z_
    #undef _Outptr_opt_result_z_
    #undef _Outptr_result_maybenull_z_
    #undef _Outptr_opt_result_maybenull_z_
    #undef _Outptr_result_nullonfailure_
    #undef _Outptr_opt_result_nullonfailure_
    #undef _Outptr_result_buffer_
    #undef _Outptr_opt_result_buffer_
    #undef _Outptr_result_buffer_to_
    #undef _Outptr_opt_result_buffer_to_
    #undef _Outptr_result_buffer_maybenull_
    #undef _Outptr_opt_result_buffer_maybenull_
    #undef _Outptr_result_bytebuffer_
    #undef _Outptr_opt_result_bytebuffer_
    #undef _Outptr_result_bytebuffer_maybenull_
    #undef _Outptr_opt_result_bytebuffer_maybenull_
    #undef _Ret_writes_bytes_to_
    #undef _Check_return_
    #undef _Printf_format_string_
    #undef _Post_equal_to_
    #undef _Pre_satisfies_
    #undef _Post_satisfies_
    #undef _Field_size_full_opt_
    #undef _Field_size_bytes_full_opt_
    #undef _Field_range_
    #undef _Pre_readable_size_
    #undef _Pre_readable_byte_size_
    #undef _Null_terminated_
    #undef _Post_maybenull_
    #undef _Analysis_assume_
    #undef _Analysis_assume_nullterminated_
    #undef _Iter_
    #undef _String_length_
    #undef _Old_


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
    #define __analysis_assume(expr)
    #define _In_count_(size)
    #define _Out_opt_cap_(size)
    #define _Deref_pre_z_
    #define _Deref_pre_opt_z_
    #define _Deref_post_opt_z_
    #define _Deref_post_opt_count_(size)
    #define _Deref_prepost_z_
    #define _Deref_pre_maybenull_
    #define _Acquires_lock_(lock)
    #define _Releases_lock_(lock)
    #define __in_xcount(size)

    #define __fallthrough
    #define __drv_freesMem(type)
    #define _At_(target, annos)
    #define _At_buffer_(target, iter, bound, annos)
    #define _When_(expr,anno)
    #define _On_failure_(annos)
    #define _Success_(expr)
    #define _Always_(annos)
    #define _Const_
    #define _In_
    #define _In_opt_
    #define _In_z_
    #define _In_opt_z_
    #define _In_reads_(size)
    #define _In_reads_opt_(size)
    #define _In_reads_bytes_(size)
    #define _In_reads_bytes_opt_(size)
    #define _In_reads_or_z_(size)
    #define _In_reads_or_z_opt_(size)
    #define _Out_
    #define _Out_opt_
    #define _Out_writes_(size)
    #define _Out_writes_opt_(size)
    #define _Out_writes_bytes_(size)
    #define _Out_writes_bytes_opt_(size)
    #define _Out_writes_z_(size)
    #define _Out_writes_opt_z_(size)
    #define _Out_writes_to_(size,count)
    #define _Out_writes_to_opt_(size,count)
    #define _Out_writes_all_(size)
    #define _Out_writes_all_opt_(size)
    #define _Out_writes_bytes_to_(size,count)
    #define _Out_writes_bytes_to_opt_(size,count)
    #define _Out_writes_bytes_all_(size)
    #define _Out_writes_bytes_all_opt_(size)
    #define _Inout_
    #define _Inout_opt_
    #define _Inout_z_
    #define _Inout_opt_z_
    #define _Inout_updates_(size)
    #define _Inout_updates_opt_(size)
    #define _Inout_updates_z_(size)
    #define _Inout_updates_opt_z_(size)
    #define _Inout_updates_to_(size,count)
    #define _Inout_updates_to_opt_(size,count)
    #define _Inout_updates_bytes_(size)
    #define _Inout_updates_bytes_opt_(size)
    #define _Outptr_
    #define _Outptr_result_maybenull_
    #define _Outptr_opt_
    #define _Outptr_opt_result_maybenull_
    #define _Outptr_result_z_
    #define _Outptr_opt_result_z_
    #define _Outptr_result_maybenull_z_
    #define _Outptr_opt_result_maybenull_z_
    #define _Outptr_result_nullonfailure_
    #define _Outptr_opt_result_nullonfailure_
    #define _Outptr_result_buffer_(size)
    #define _Outptr_opt_result_buffer_(size)
    #define _Outptr_result_buffer_to_(size, count)
    #define _Outptr_opt_result_buffer_to_(size, count)
    #define _Outptr_result_buffer_maybenull_(size)
    #define _Outptr_opt_result_buffer_maybenull_(size)
    #define _Outptr_result_bytebuffer_(size)
    #define _Outptr_opt_result_bytebuffer_(size)
    #define _Outptr_result_bytebuffer_maybenull_(size)
    #define _Outptr_opt_result_bytebuffer_maybenull_(size)
    #define _Ret_writes_bytes_to_(size,count)
    #define _Check_return_
    #define _Printf_format_string_
    #define _Post_equal_to_(expr)
    #define _Pre_satisfies_(cond)
    #define _Post_satisfies_(cond)
    #define _Field_size_full_opt_(size)
    #define _Field_size_bytes_full_opt_(expr)
    #define _Field_range_(min,max)
    #define _Pre_readable_size_(size)
    #define _Pre_readable_byte_size_(size)
    #define _Null_terminated_
    #define _Post_maybenull_
    #define _Analysis_assume_(expr)
    #define _Analysis_assume_nullterminated_(str)
    #define _Iter_
    #define _String_length_(str)
    #define _Old_(param)

    #define PREFAST_PUSH_IGNORE_WARNINGS_IN_NON_PLAYREADY_HEADERS
    #define PREFAST_POP_IGNORE_WARNINGS_IN_NON_PLAYREADY_HEADERS

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

#define _Out_writes_bts_(size) _Out_writes_to_opt_(_Old_(size),size) _Post_satisfies_(size <= _Old_(size))
#define _Out_writes_bts2_(p,size) _Out_writes_bts_(size) _When_(p == NULL, _Post_satisfies_(return<0))

#endif  /* __DRMSAL_H__ */

