/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"
#include <sdk/lwpu/inc/lwtypes.h>

void LIBOS_SECTION_IMEM_PINNED list_link(list_header_t *id)
{
    list_header_t *next = LIBOS_PTR32_EXPAND(list_header_t, id->next);
    list_header_t *prev = LIBOS_PTR32_EXPAND(list_header_t, id->prev);
    next->prev          = LIBOS_PTR32_NARROW(id);
    prev->next          = LIBOS_PTR32_NARROW(id);
}

void LIBOS_SECTION_IMEM_PINNED list_unlink(list_header_t *id)
{
    list_header_t *next = LIBOS_PTR32_EXPAND(list_header_t, id->next);
    list_header_t *prev = LIBOS_PTR32_EXPAND(list_header_t, id->prev);
    prev->next          = LIBOS_PTR32_NARROW(next);
    next->prev          = LIBOS_PTR32_NARROW(prev);
}
