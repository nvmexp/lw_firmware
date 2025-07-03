/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef MEM_HS_H
#define MEM_HS_H

/*!
 * @file mem_hs.h
 * This files holds the inline definitions for standard memory functions.
 * This header should only be included by heavy secure microcode.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"

#ifndef NULL
#define NULL 0
#endif

/* ------------------------- Prototypes ------------------------------------- */
int memcmp_hs(const void *ptr1, const void *ptr2, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libMemHs", "memcmp_hs");
void *memcpy_hs(void *dest, const void *src, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libMemHs", "memcpy_hs");
void *memset_hs(void *ptr, LwU8 value, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libMemHs", "memset_hs");

#endif // MEM_HS_H
