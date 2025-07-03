/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/rc.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "cheetah/include/jstocpp.h"

// this file is not to be compiled unless someone wants to make changes to xp_qnx.cpp
// and run all the unit tests associated with it. In that case, once the file is
// compiled, it exposes the namespace to JavaScript. Tests can be run by calling
//
// var rc = XpQnxTest.Run();
//
// Unit tests for testing memory allocation and mapping functionality in XP layer
// for QNX
namespace XpQnxTest
{
    RC TestAllocPages
    (
        const UINT32 numPages
       ,const UINT32 numBits = 32
    )
    {
        RC rc;
        const UINT32 PAGE_SIZE = 4096;
        const UINT32 PATTERN = 0xdeadbeaf;
        void* pVirtualMemory = nullptr;

        Printf(Tee::PriNormal, "Testing with %u page(s)\n", numPages);
        // allocate number of pages requested
        CHECK_RC(Xp::AllocPages(PAGE_SIZE * numPages,
                                &pVirtualMemory,
                                true,
                                numBits,
                                Memory::UC,
                                0));
        uintptr_t ptr = reinterpret_cast<uintptr_t>(pVirtualMemory);
        PHYSADDR prevPhysAddr = Xp::VirtualToPhysical(pVirtualMemory);

        // read/write to this memory making sure that all the memory is accessible
        for (uintptr_t index=0; index<(PAGE_SIZE*numPages); index+=4)
        {
            uintptr_t addr = ptr + index;
            MEM_WR32(addr, PATTERN);
            UINT32 val = MEM_RD32(addr);

            if (val != PATTERN)
            {
                Printf(Tee::PriError, "%s failed\n", __func__);
                return RC::SOFTWARE_ERROR;
            }

            // check if physical memory is actually contigous
            if ((index != 0) && (index % PAGE_SIZE == 0))
            {
                PHYSADDR pagePhysAddr = Xp::VirtualToPhysical(reinterpret_cast<void*>(addr));

                if ((pagePhysAddr - prevPhysAddr) != PAGE_SIZE)
                {
                    Printf(Tee::PriError, "physical memory allocated is not contigous\n");
                    return RC::SOFTWARE_ERROR;
                }

                prevPhysAddr = pagePhysAddr;
            }
        }

        Xp::FreePages(pVirtualMemory);
        return OK;
    }

    RC TestMemoryAllocation()
    {
        RC rc;
        CHECK_RC(TestAllocPages(1));
        CHECK_RC(TestAllocPages(2));
        CHECK_RC(TestAllocPages(100));
        CHECK_RC(TestAllocPages(10000));
        CHECK_RC(TestAllocPages(100000));
        return OK;
    }

    // this tests both mapping device memory and also translating virtual address
    // to physical address functionality as well.
    RC TestMapDeviceMemory
    (
        const UINT32 baseAddress
       ,const UINT32 size
    )
    {
        RC rc;
        void* pVirtualAddress = nullptr;

        Printf(Tee::PriNormal, "Testing mapping device memory: 0x%08x\n",
                                baseAddress);
        CHECK_RC(Xp::MapDeviceMemory(&pVirtualAddress,
                                    baseAddress,
                                    size,
                                    Memory::UC,
                                    Memory::ReadWrite));

        // translate back virtual address to physical address and they should match
        PHYSADDR physAddr = Xp::VirtualToPhysical(pVirtualAddress);

        if (physAddr != baseAddress)
        {
            Printf(Tee::PriError, "translated physical address does not match expected value\n");
            return RC::SOFTWARE_ERROR;
        }

        return rc;
    }

    RC TestDeviceMemoryMapping()
    {
        // test using T18x device apertures
        RC rc;
        CHECK_RC(TestMapDeviceMemory(0x2490000, 0x50000));    // eqos aperture
        CHECK_RC(TestMapDeviceMemory(0x3160000, 0x10000));    // i2c1 aperture
        CHECK_RC(TestMapDeviceMemory(0x3820000, 0x10000));    // fuse aperture
        CHECK_RC(TestMapDeviceMemory(0xf200000, 0x10000));    // fuse aperture

        // negative test
        if (RC::BAD_PARAMETER != TestMapDeviceMemory(0x80000000, 0x100))
        {
            return RC::SOFTWARE_ERROR;
        }

        return OK;
    }

    RC TestPhysicalToVirtual()
    {
        const UINT32 EQOS_REGISTER = 0x2498004;
        void* pVirtualAddress = nullptr;
        RC rc;

        Printf(Tee::PriNormal, "Testing physical to virtual address mapping\n");
        // PhysRd32 ilwokes Xp::PhysicalToVirtual()
        UINT32 physVal = Platform::PhysRd32(EQOS_REGISTER);
        // Create alternate virtual mapping to ensure that we read back the
        // same.
        CHECK_RC(Xp::MapDeviceMemory(&pVirtualAddress, EQOS_REGISTER, 4,
                                     Memory::UC, Memory::ReadWrite));
        UINT32 mappedVal = MEM_RD32(pVirtualAddress);

        if (mappedVal != physVal)
        {
            Printf(Tee::PriNormal, "physical to virtual mapping failed\n");
            return RC::SOFTWARE_ERROR;
        }

        return OK;
    }

    RC Run()
    {
        RC rc;

        Printf(Tee::PriNormal, "Testing memory allocation in QNX\n");
        CHECK_RC(TestMemoryAllocation());
        CHECK_RC(TestDeviceMemoryMapping());
        CHECK_RC(TestPhysicalToVirtual());
        return OK;
    }
}

JSIF_NAMESPACE(XpQnxTest, nullptr, nullptr, "Xp QNX test namespace");
JSIF_TEST_0_S(XpQnxTest, Run, "Run all the tests");
