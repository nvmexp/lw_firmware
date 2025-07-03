/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

//
// LWE (tandrewprinz): Including LWPU copyright, as file was forked from
// libspdm source, but contains LWPU modifications.
//

/** @file
  Processor or Compiler specific defines and types for Falcon

  LWE (tandrewprinz): This file was forked from the RISCV-32 processor_bind.h,
  most compiler defines have been removed as they were not relevant to Falcon.
**/

#ifndef PROCESSOR_BIND_H__
#define PROCESSOR_BIND_H__

//
// Make sure we are using the correct packing rules per EFI specification
//
#if !defined(__GNUC__)
#pragma pack()
#endif

//
// LWE (tandrewprinz): For all types below, assert equivalent LWPU-types
// are the same size, so we have confidence in the compatibility of our code.
// We cannot assert that signedness are equivalent, however, compiler warnings
// can help us catch issues here.
//
#include <lwctassert.h>
#include <lwtypes.h>

///
/// 8-byte unsigned value
///
typedef unsigned long long uint64 __attribute__((aligned(8)));
ct_assert(sizeof(uint64) == sizeof(LwU64));

///
/// 8-byte signed value
///
typedef long long int64 __attribute__((aligned(8)));
ct_assert(sizeof(int64) == sizeof(LwS64));

///
/// 4-byte unsigned value
///
typedef unsigned int uint32 __attribute__((aligned(4)));
ct_assert(sizeof(uint32) == sizeof(LwU32));

///
/// 4-byte signed value
///
typedef int int32 __attribute__((aligned(4)));
ct_assert(sizeof(int32) == sizeof(LwS32));

///
/// 2-byte unsigned value
///
typedef unsigned short uint16 __attribute__((aligned(2)));
ct_assert(sizeof(uint16) == sizeof(LwU16));

///
/// 2-byte signed value
///
typedef short int16 __attribute__((aligned(2)));
ct_assert(sizeof(int16) == sizeof(LwS16));

///
/// Logical Boolean.  1-byte value containing 0 for FALSE or a 1 for TRUE.  Other
/// values are undefined.
///
typedef unsigned char boolean;
ct_assert(sizeof(boolean) == sizeof(LwBool));

///
/// 1-byte unsigned value
///
typedef unsigned char uint8;
ct_assert(sizeof(uint8) == sizeof(LwU8));

///
/// 1-byte Character
///
typedef char char8;
ct_assert(sizeof(char8) == sizeof(LwU8));

///
/// 1-byte signed value
///
typedef signed char int8;
ct_assert(sizeof(int8) == sizeof(LwS8));

///
/// Unsigned value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef uint32 uintn __attribute__((aligned(4)));
ct_assert(sizeof(uintn) == sizeof(LwU32));

///
/// Signed value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef int32 intn __attribute__((aligned(4)));
ct_assert(sizeof(intn) == sizeof(LwS32));

//
// Processor specific defines
//

///
/// A value of native width with the highest bit set.
///
#define MAX_BIT 0x80000000
///
/// A value of native width with the two highest bits set.
///
#define MAX_2_BITS 0xC0000000

///
/// Maximum legal Falcon intn and uintn values.
///
#define MAX_INTN ((intn)0x7FFFFFFF)
#define MAX_UINTN ((uintn)0xFFFFFFFF)

#endif
