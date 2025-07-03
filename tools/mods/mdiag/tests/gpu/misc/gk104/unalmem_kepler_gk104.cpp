/*
 *LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2013, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// XXX collapse all of this code back into unalmem_kepler.cpp and get rid of the derived class
#include "mdiag/tests/gpu/misc/unalmem_kepler.h"

NotPllUnalignedSysmemKepler_impl::NotPllUnalignedSysmemKepler_impl
(
    ArgReader *params
)
: NotPllUnalignedSysmemKepler(params)
{
    m_Name = "UnalignedSysmem";
    sysmem_width = 256;
    sysmem_pitch = 256;
    sysmem_height = 1;
}

NotPllUnalignedSysmemKepler_impl::~NotPllUnalignedSysmemKepler_impl()
{
}

SOCV_RC NotPllUnalignedSysmemKepler_impl::NotPllUnalignedSysmemTest()
{
    //Set up, acquire memory
    RC rc;
    m_SysMem.SetWidth(sysmem_width / ColorUtils::PixelBytes(ColorUtils::VOID32));
    m_SysMem.SetPitch(sysmem_pitch);
    m_SysMem.SetHeight(sysmem_height);
    m_SysMem.SetColorFormat(ColorUtils::VOID32);
    m_SysMem.SetLocation(Memory::Coherent);
    CHECK_RC_CLEANUP(m_SysMem.Alloc(lwgpu->GetGpuDevice()));
    CHECK_RC_CLEANUP(m_SysMem.Map());
    Cleanup:
    InfoPrintf("Test set up done\n");

    void *pAddr = m_SysMem.GetAddress();
    uintptr_t addr = (uintptr_t)pAddr;
    UINT32 data32;
    UINT32 i;
    bool bSuccess = true;
    bool bTestSuccess = true;
    UINT08 *pData32 = (UINT08 *)&data32;

    // Part 1 : Fill the surface with an incrementing pattern using 32-bit
    // writes so the data will be correctly written.  Write the data so that
    // it can be accessed in an endian agnostic manner when not reading back
    // with 32 bit access
    InfoPrintf("Filling suface with incrementing pattern using MEM_WR32\n");
    for (i = 0; i < sysmem_width; i += 4, addr+=4)
    {
        pData32[0] = i;
        pData32[1] = (i + 1);
        pData32[2] = (i + 2);
        pData32[3] = (i + 3);
        MEM_WR32(addr, data32);
    }

    // Part 2 : Read back and verify the data at every address within the
    // surface using both 8-bit reads.  If unaligned reads are not functioning,
    // this should fail on the first unaligned address.  Read back and verify
    // the data using 32-bit reads at aligned addresses to validate that the
    // data was written correctly
    InfoPrintf("Reading surface back at byte alignment using both MEM_RD32 and MEM_RD08\n");
    for (i = 0, addr = (uintptr_t)pAddr; (i <= sysmem_width-4) && bSuccess; i++, addr++)
    {
        // To avoid bus-faults, only perform 32-bit reads at aligned addresses
        // This will ensure that the data was actually written correctly
        if (!(addr & 0x03))
        {
            pData32[0] = i;
            pData32[1] = (i + 1);
            pData32[2] = (i + 2);
            pData32[3] = (i + 3);

            DebugPrintf("MEM_RD32(0x%08x) = 0x%08x [0x%08x]\n", addr, MEM_RD32(addr), i);
            if (MEM_RD32(addr) != data32)
            {
                ErrPrintf("MEM_RD08(0x%08x) = 0x%08x and should be 0x%08x\n", addr, MEM_RD32(addr), data32);
                bSuccess = false;
                bTestSuccess = false;
            }
        }

        DebugPrintf("MEM_RD08(0x%08x) = 0x%02x [0x%02x]\n", addr, MEM_RD08(addr), i);
        if (MEM_RD08(addr) != i)
        {
            ErrPrintf("MEM_RD08(0x%08x) = 0x%02x and should be 0x%02x\n", addr, MEM_RD08(addr), i);
            bSuccess = false;
            bTestSuccess = false;
        }
    }

    // Part 3 : Fill the surface with a decrementing pattern using 8-bit
    // writes that could potentially fail.
    InfoPrintf("Filling suface with decrementing pattern using MEM_WR08\n");
    addr = (uintptr_t)pAddr;
    for (i = 0; i < sysmem_width; i++, addr++)
    {
        MEM_WR08(addr, sysmem_width - 1 - i);
    }

    // Part 4 : Read back and verify the data at every address within the
    // surface using 8-bit reads.  If unaligned reads are not functioning,
    // this should fail on the first unaligned address.  Read back and verify
    // the data using 32-bit reads at aligned addresses to validate that the
    // data was written correctly.  If unaligned writes are not functioning,
    // this should fail on the first address.
    InfoPrintf("Reading suface back at byte alignment using both MEM_RD32 and MEM_RD08\n");
    for (i = sysmem_width -1 , addr = (uintptr_t)pAddr, bSuccess = true; (i >= 3) && bSuccess; i--, addr++)
    {
        // To avoid bus-faults, only perform 32-bit reads at aligned addresses
        // This will ensure that the data was actually written correctly
        if (!(addr & 0x03))
        {
            pData32[0] = i;
            pData32[1] = (i - 1);
            pData32[2] = (i - 2);
            pData32[3] = (i - 3);

            DebugPrintf("MEM_RD32(0x%08x) = 0x%08x [0x%08x]\n", addr, MEM_RD32(addr), i);
            if (MEM_RD32(addr) != data32)
            {
                ErrPrintf("MEM_RD08(0x%08x) = 0x%08x and should be 0x%08x\n", addr, MEM_RD32(addr), data32);
                bSuccess = false;
                bTestSuccess = false;
            }
        }

        DebugPrintf("MEM_RD08(0x%08x) = 0x%02x [0x%02x]\n", addr, MEM_RD08(addr),i);
        if (MEM_RD08(addr) != i)
        {
            ErrPrintf("MEM_RD08(0x%08x) = 0x%02x and should be 0x%02x\n", addr, MEM_RD08(addr), i);
            bSuccess = false;
            bTestSuccess = false;
        }
    }

    // Part 5 : Read back and verify the data at every 3rd address within the
    // surface by using Platform::MemCopy in 3 byte chunks.  If unaligned
    // reads are not functioning, this should fail on the first address
    InfoPrintf("Reading surface back 3 bytes at a time using Platform::MemCopy\n");
    UINT32 lwrData;
    for (i = sysmem_width - 1, addr = (uintptr_t)pAddr, bSuccess = true; (i >= 2) && bSuccess; i-=3, addr+=3)
    {
        pData32[0] = i;
        pData32[1] = (i - 1);
        pData32[2] = (i - 2);
        pData32[3] = 0;
        lwrData = 0;
        Platform::MemCopy((volatile void *)&lwrData, (volatile void *)addr, 3);
        DebugPrintf("Data from 0x%08x = 0x%08x [0x%08x]\n", addr, lwrData, data32);

        if (lwrData != data32)
        {
            ErrPrintf("Data from 0x%08x = 0x%08x and should be 0x%08x\n", addr, lwrData, data32);
            bSuccess = false;
            bTestSuccess = false;
        }
    }
    return bTestSuccess ? SOCV_OK : SOCV_TEST_FAILED;
}

