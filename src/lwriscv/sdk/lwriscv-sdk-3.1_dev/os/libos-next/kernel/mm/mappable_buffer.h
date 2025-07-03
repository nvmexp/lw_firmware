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

#include <lwtypes.h>
#include "librbtree.h"
#include "libos_status.h"
#include "pagetable.h"
#include "../loader.h"
#include "mm/objectpool.h"

/**
 *  @brief Tracks a reservation of a range of virtual addresses.
 *        
 */
typedef struct MappableBuffer {
    //
    // Translates a physical page address to an offset within this mappable buffer
    // Required when moving a page
    //
    LibosTreeHeader   paToOffset;

    // Linked list of all mappings to this buffer
    ListHeader        mappings;


} MappableBuffer;

typedef struct {
    // Where is this mapping
    AddressSpaceRegion * containingRegion;
    LwU64                virtualAddress;
    LwU64                size;

    // What are we mapping
    MappableBuffer     * buffer;

    ListHeader           header;            // link for MappableBuffer::mappings
} Mapping;
