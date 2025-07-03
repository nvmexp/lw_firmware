#!/usr/bin/perl -w

#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# This script generates pre-processed PMU headers to implement functionality that otherwise would require
# two passes of the compiler's pre-processor. This allows developer friendly syntax to be used closely resembling
# functionality and integration of LWOC.
#
# >perl $(LW_SOURCE)/drivers/common/shared/scripts/pmu_preprocess.pl --input_file my_header.h --output_file g_my_header.h
#
use warnings;
use strict;
use Getopt::Long;   # GetOptions
use File::Basename; # Basename

##############################################################################
# Define locals
##############################################################################
my $input_file;
my $output_file;
my $include_guard_define;
my $file_text;
my $file_handle;

##############################################################################
# Capture command line arguments
##############################################################################
GetOptions('input_file=s'  => \$input_file,
           'output_file=s' => \$output_file)
or die("Error in command line arguments\n");

##############################################################################
# Slurp in the input file to a local string
##############################################################################
open $file_handle, '<', $input_file
or die("Failed to open $input_file");
local $/ = undef;
$file_text = <$file_handle>;
close $file_handle;

##############################################################################
# Create the include guard from file name
##############################################################################
 # Get the output file's basename
$include_guard_define = basename($input_file);

# Colwert to include guard (include.h => include_h)
$include_guard_define =~ s/(\w+).(\w+)/$1_$2/smg;

# Uppercase it (include_h => INCLUDE_H)
$include_guard_define = uc $include_guard_define;

##############################################################################
# Transform the include guards in the generated file
##############################################################################
# #ifndef
$file_text =~ s/#ifndef([\s\S]+?)$include_guard_define/#ifndef$1G_$include_guard_define/g;

# #define
$file_text =~ s/#define([\s\S]+?)$include_guard_define/#define$1G_$include_guard_define/g;

# #endif ...
$file_text =~ s/([\s\S]+?) $include_guard_define/$1 G_$include_guard_define/g;

##############################################################################
# In-place multi-line search find and replace
##############################################################################
# // Before
# mockable FunctionPrototypeDefine    (function)
#     GCC_ATTRIB_SECTION("imem_thing", "function");
#
# // After
# FunctionPrototypeDefine (function_IMPL)
#     GCC_ATTRIB_SECTION("imem_thing", "function_IMPL");
# #ifdef UTF_FUNCTION_MOCKING
# FunctionPrototypeDefine (function_MOCK)
#     GCC_ATTRIB_SECTION("imem_thing", "function_MOCK");
# #define function(...) function_MOCK(__VA_ARGS__)
# #else
# #define function(...) function_IMPL(__VA_ARGS__)
# #endif
$file_text =~ s/^mockable\s+(\w+)\s+\((\w+)\)([\s\S]+?)\g2([\s\S]+?)\;$/\n$1 ($2_IMPL)$3$2_IMPL$4;\n#ifdef UTF_FUNCTION_MOCKING\n$1 ($2_MOCK)$3$2_MOCK$4;\n#define $2(...) $2_MOCK(__VA_ARGS__)\n#else\n#define $2(...) $2_IMPL(__VA_ARGS__)\n#endif\n/gm;

# // Before
# mockable FunctionPrototypeDefineAlt (functionAlt);
#
# // After
# FunctionPrototypeDefineAlt (functionAlt_IMPL);
# #ifdef UTF_FUNCTION_MOCKING
# FunctionPrototypeDefineAlt (functionAlt_MOCK);
# #define functionAlt(...) functionAlt_MOCK(__VA_ARGS__)
# #else
# #define functionAlt(...) functionAlt_IMPL(__VA_ARGS__)
# #endif
$file_text =~ s/^mockable\s+(\w+)\s+\((\w+)\)\;$/\n$1 ($2_IMPL);\n#ifdef UTF_FUNCTION_MOCKING\n$1 ($2_MOCK);\n#define $2(...) $2_MOCK(__VA_ARGS__)\n#else\n#define $2(...) $2_IMPL(__VA_ARGS__)\n#endif\n/gm;

# // before
# mockable LwU32 myFunc2(int arg1, char *pArg2, pFunc (func));
#
# // after
# LwU32 myFunc2_IMPL(int arg1, char *pArg2, pFunc (func));
# #ifdef UTF_FUNCTION_MOCKING
# LwU32 myFunc2_MOCK(int arg1, char *pArg2, pFunc (func));
# #define myFunc2(...) myFunc2_MOCK(__VA_ARGS__)
# #else
# #define myFunc2(...) myFunc2_IMPL(__VA_ARGS__)
# #endif
$file_text =~ s/^mockable\s+?([\w\W\s]+?)\s+?(\w+?)\s*?(\([\s\S]+?\));$/$1 $2_IMPL$3;\n#ifdef UTF_FUNCTION_MOCKING\n$1 $2_MOCK$3;\n#define $2(...) $2_MOCK(__VA_ARGS__)\n#else\n#define $2(...) $2_IMPL(__VA_ARGS__)\n#endif\n/gm;

# // before
# mockable LwU32 myFunc(int arg1, char *pArg2, pFunc (func))
#     GCC_ATTRIB_SECTION("imem_perfVf", "myFunc");
#
# // after
# LwU32 myFunc_IMPL(int arg1, char *pArg2, pFunc (func))
#     GCC_ATTRIB_SECTION("imem_perfVf", "myFunc_IMPL");
# #ifdef UTF_FUNCTION_MOCKING
# LwU32 myFunc_MOCK(int arg1, char *pArg2, pFunc (func))
#     GCC_ATTRIB_SECTION("imem_perfVf", "myFunc_MOCK");
# #define myFunc(...) myFunc_MOCK(__VA_ARGS__)
# #else
# #define myFunc(...) myFunc_IMPL(__VA_ARGS__)
# #endif
$file_text =~ s/^mockable\s+?([\w\W\s]+?)\s+?(\w+?)\s*?(\([\s\S]+?\))([\s\S]+?)\g2([\s\S]+?);$/$1 $2_IMPL$3$4$2_IMPL$5;\n#ifdef UTF_FUNCTION_MOCKING\n$1 $2_MOCK$3$4$2_MOCK$5;\n#define $2(...) $2_MOCK(__VA_ARGS__)\n#else\n#define $2(...) $2_IMPL(__VA_ARGS__)\n#endif\n/gm;

##############################################################################
# Open output file and write to ie
##############################################################################
open($file_handle, '>', $output_file)
or die("Failed to open $output_file");
print $file_handle $file_text;
close($file_handle);
