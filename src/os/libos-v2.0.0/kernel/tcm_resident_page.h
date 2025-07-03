/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_RESIDENT_PAGE_H
#define LIBOS_RESIDENT_PAGE_H

#include "kernel.h"

#define HASH_BUCKETS 32u

// fb_offset flags
#define PAGE_DIRTY    1ULL
#define PAGE_RESIDENT 2ULL
#define PAGE_MASK     0xFFFFFFFFFFFFF000ULL

typedef struct resident_page_header
{
    struct resident_page_header *next, *prev;
} resident_page_header;

typedef struct resident_page
{
    resident_page_header header;
    LwU64 fb_offset;
} resident_page;

extern resident_page_header LIBOS_SECTION_DMEM_PINNED(page_by_pa)[HASH_BUCKETS];

LIBOS_SECTION_IMEM_PINNED resident_page_header *resident_page_bucket(LwU64 address);
LIBOS_SECTION_IMEM_PINNED void resident_page_insert(resident_page_header *root, resident_page_header *value);
LIBOS_SECTION_IMEM_PINNED void resident_page_remove(resident_page_header *value);

#endif
