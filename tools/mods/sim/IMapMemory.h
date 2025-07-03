/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _IMAP_MEMORY_H_
#define _IMAP_MEMORY_H_

#include "ITypes.h"
#include "IIface.h"

// The IMapMemory interface provides methods needed for running the "V" sysspec on chiplibs
// where memory mapping of address ranges, address translation (linear->physical), memory
// range settings (e.g., MTRRs), and miscellaneous other things (enabling and configuring GART
// memory, etc.) must be done in cooperation with a system that doesn't let you directly wack on
// the h/w registers: e.g., linux, win2k.
//

class IMapMemory : public IIfaceObject
{
public:
    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;

    // IMapMemory Interface

    // returns the physical address mapping of the first byte of the range
    // specified by (virtual_base, virtual_size) in *physical_base_p.
    // *physical_size_p indicates how many contiguous byte are mapped
    virtual void GetPhysicalAddress(void *virtual_base, LwU032 virtual_size,
                                    LwU032 *physical_base_p, LwU032 *physical_size_p) = 0;

    virtual LwU032 LinearToPhysical(void *LinearAddress) = 0;
    virtual void *PhysicalToLinear(LwU032 PhysicalAddress) = 0;

    // guarantees that a memory region is accessable by the program, and
    // sets the memory type (location, cacheability)
    // return 0 on success, non-zero on failure
    virtual int MapMemoryRegion(LwU032 base, LwU032 size, LwU016 type) = 0;
};

#endif // _IMAP_MEMORY_H_
