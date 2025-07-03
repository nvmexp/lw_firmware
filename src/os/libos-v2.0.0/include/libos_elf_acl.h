/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#pragma once
#include "libos_ilwerted_pagetable.h"
#include <sdk/lwpu/inc/lwtypes.h>

//
//  Access Control List structures
//      PHDR index = count - LIBOS_MANIFEST_PHDR_ACLS
//      PHDR type  = LIBOS_PT_MANIFEST_ACLS
//
//      Section is an array of type libos_phdr_acl_t[].  One entry for
//      each PHDR in the ELF header.
//
//      NOTE: The acl_read_only() and acl_read_write() lists are disjoint.
//

typedef struct
{
    LwU8 count;
    libos_pasid_t pasid[1];
} libos_acl_t;

typedef struct
{
    // Relative pointer to libos_phdr_acl_t from start of this structure
    LwU32 offset_read_only;
    LwU32 offset_read_write;
} libos_phdr_acl_t;

static inline libos_acl_t *acl_read_only(libos_phdr_acl_t *acl)
{
    return (libos_acl_t *)((LwU8 *)acl + acl->offset_read_only);
}

static inline libos_acl_t *acl_read_write(libos_phdr_acl_t *acl)
{
    return (libos_acl_t *)((LwU8 *)acl + acl->offset_read_write);
}
