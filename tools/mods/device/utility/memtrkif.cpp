/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "device/include/memtrk.h"
#include "cheetah/include/jstocpp.h"

JSIF_NAMESPACE(MemoryTracker, NULL, NULL, "MemoryTracker Namespace");

JS_SMETHOD_LWSTOM(MemoryTracker, AllocBuffer, 4, "Allocate a memory buffer")
{
    JSIF_INIT(MemoryTracker, AllocBuffer, 4, NumBytes, Contigous, AddressBits, Attributes);
    JSIF_GET_ARG(0, UINT32, NumBytes);
    JSIF_GET_ARG(1, bool, Contigous);
    JSIF_GET_ARG(2, UINT32, AddressBits);
    JSIF_GET_ARG(3, UINT32, Attributes);
    void* addr;
    JSIF_CALL_TEST_STATIC(MemoryTracker, AllocBuffer, NumBytes, &addr, Contigous, AddressBits, (Memory::Attrib)Attributes);
    JSIF_METHOD_CHECK_RC(MemoryTracker, AllocBuffer);
    JSIF_RETURN(reinterpret_cast<UINT64>(addr));
}

JS_SMETHOD_LWSTOM(MemoryTracker, AllocBufferAligned, 4, "Allocate a memory buffer")
{
    JSIF_INIT(MemoryTracker, AllocBufferAligned, 4, NumBytes, Align, AddressBits, Attributes);
    JSIF_GET_ARG(0, UINT32, NumBytes);
    JSIF_GET_ARG(1, UINT32, Align);
    JSIF_GET_ARG(2, UINT32, AddressBits);
    JSIF_GET_ARG(3, UINT32, Attributes);
    void* addr;
    JSIF_CALL_TEST_STATIC(MemoryTracker, AllocBufferAligned, NumBytes, &addr, Align, AddressBits, (Memory::Attrib)Attributes);
    JSIF_METHOD_CHECK_RC(MemoryTracker, AllocBufferAligned);
    JSIF_RETURN(reinterpret_cast<UINT64>(addr));
}

JS_STEST_LWSTOM(MemoryTracker, FreeBuffer, 1, "Free a memory buffer")
{
    JSIF_INIT(MemoryTracker, FreeBuffer, 1, Address);
    JSIF_GET_ARG(0, UINT64, Address);
    void* addr = reinterpret_cast<void*>(Address);
    JSIF_CALL_TEST_STATIC(MemoryTracker, FreeBuffer, addr);
    JSIF_METHOD_CHECK_RC(MemoryTracker, FreeBuffer);
    JSIF_RETURN(rc);
}

JS_SMETHOD_LWSTOM(MemoryTracker, VirtualToPhysical, 1, "Colwert a virtual address to a physical address")
{
    JSIF_INIT(MemoryTracker, VirtualToPhysical, 1, pVirtualAddr);
    JSIF_GET_ARG(0, UINT64, pVirtualAddr);
    UINT64 addr = 0;
    JSIF_CALL_TEST_STATIC(MemoryTracker, VirtualToPhysical, reinterpret_cast<void*>(pVirtualAddr), reinterpret_cast<PHYSADDR*>(&addr));
    JSIF_METHOD_CHECK_RC(MemoryTracker, VirtualToPhysical);
    JSIF_RETURN(addr);
}

JSIF_TEST_2_S(MemoryTracker, InitRsvd, UINT64, Min, UINT64, Max, "Reserve initial memory");
