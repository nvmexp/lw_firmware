/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "tcm_resident_page.h"

resident_page_header LIBOS_SECTION_DMEM_PINNED(page_by_pa)[HASH_BUCKETS];

LIBOS_SECTION_IMEM_PINNED resident_page_header *resident_page_bucket(LwU64 address)
{
    return &page_by_pa[(address % 1023u) & (HASH_BUCKETS - 1u)];
}

LIBOS_SECTION_IMEM_PINNED void
resident_page_insert(resident_page_header *root, resident_page_header *value)
{
    value->prev       = root;
    value->next       = root->next;
    value->prev->next = value;
    value->next->prev = value;
}

LIBOS_SECTION_IMEM_PINNED void resident_page_remove(resident_page_header *value)
{
    value->prev->next = value->next;
    value->next->prev = value->prev;
}