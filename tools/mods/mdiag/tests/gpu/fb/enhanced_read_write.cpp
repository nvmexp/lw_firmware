/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2019-2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "enhanced_read_write.h"

#include "sim/IChip.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"
#include "mdiag/utils/surfutil.h"

static const UINT32 FB_MAX_BURST_SIZE = 128;
static const UINT32 FB_GOB_SIZE = 512;

static const UINT32 LTC_WAYS = 16;

//
// Please refer to //hw/doc/gpu/fermi/fermi/design/testplan/FB/FB_PA_Testing_Strategy.doc for detail
//
// 3.1.1.1 Interface between L2 and FB
//
//  At the next level of detail, the partition is the interface
//  between the L2 cache and DRAM.  At both the interface to L2 and
//  DRAM, the partition needs to be tested that the correct protocols
//  are implemented.  Sizes of the reads and writes need to be tested.
//  Reads can be done from 32 to 128 byte sizes in increments of 32
//  bytes while writes with byte enables can be from 1 to 128 bytes.
//  There is an interface to the FB from every slice so these need to
//  be tested.  And the FB does some bit manipulations on the address
//  arriving from the L2 so this should be checked.
//
// Tests: (enhanced_read_write)
// 4)   Read various sizes (32-128 bytes)
// 5)   Writes of various sizes (1-128 bytes)
// 6)   Validate sequence of packets over interface
//      a. Test cases where the unrolled request is 1, 2, and 3 subrequests, 4 is illegal.
// 7)   Do reads and writes on all slice interfaces
// 8)   Test boundary conditions on each interface, i.e., make sure every wire is exercised.
// 9)   (This testing should go to the cross unit testplan for address mapping)
//      Check address mapping from L2 address to DRAM address
//        - FB does bit manipulations on the address arriving from the L2
//          to create the DRAM row/bank/subpartition/column
// 10)  Cleans and dirties (Self clean?)
// 11)  Test all fields (line id, awp, subid, last)
// 12)  Make tests to see that there are no read write conflicts
// 13)  Cleans and dirties to the same line
extern const ParamDecl enhanced_read_write_params[] = {
    PARAM_SUBDECL(FBTest::fbtest_common_params),
    MEMORY_SPACE_PARAMS("_buf0", "dma buffer 0 used for testing"),
    UNSIGNED_PARAM("-size",  "Size of testing buffer"),
    UNSIGNED_PARAM("-offset", "Offset of testing buffer"),
    SIMPLE_PARAM("-frontdoor_check", "check data through frontdoor"),
    SIMPLE_PARAM("-frontdoor_init", "Initialize data through frontdoor"),
    LAST_PARAM
};

enhanced_read_writeTest::enhanced_read_writeTest(ArgReader *params) :
    FBTest(params),
    local_status(1),
    m_frontdoor_check(false),
    m_frontdoor_init(false)
{
    ParseDmaBufferArgs(m_testBuffer, params, "buf0", &m_pteKindName, 0);

    // make sure testing surface is aligned based on the given number of partitions
    m_size = params->ParamUnsigned("-size", 1024 * 1024);
    m_frontdoor_check = params->ParamPresent("-frontdoor_check");
    m_frontdoor_init = params->ParamPresent("-frontdoor_init");

    // Size must be 4B aligned
    m_size = m_size & ~0x3;

    MASSERT((m_size >= 1024 * 1024) && "the test needs at lest 1MB buffer");

    FBTest::DebugPrintArgs();
}

enhanced_read_writeTest::~enhanced_read_writeTest(void)
{
}

STD_TEST_FACTORY(enhanced_read_write, enhanced_read_writeTest)

int enhanced_read_writeTest::Setup()
{
    local_status = 1;

    getStateReport()->enable();

    if (!FBTest::Setup()) {
        ErrPrintf("enhanced_read_writeTest::Setup() Fail\n");
        return 0;
    }

    m_num_fbpas = fbHelper->NumFBPAs();
    m_size *= m_num_fbpas;

    // 17 is a magic number from address mapping tools
    // It is callwlated by amap tools here //hw/fermi1_gf100/perfsim/code/standalone/FBSim/AddressMappingUtil/...
    UINT64 alignment = m_num_fbpas * 17 * 1024 * 1024;
    // Adjust fixed phys address to nearest address of the alignment
    if (m_testBuffer.HasFixedPhysAddr())
    {
        UINT64 physAddr  = (m_testBuffer.GetFixedPhysAddr() / alignment) * alignment;
        m_testBuffer.SetFixedPhysAddr(physAddr);
    }

    m_testBuffer.SetArrayPitch(m_size);
    m_testBuffer.SetColorFormat(ColorUtils::Y8);
    // m_testBuffer.SetAlignment(alignment);
    m_testBuffer.SetForceSizeAlloc(true);
    m_testBuffer.SetProtect(Memory::ReadWrite);
    if (OK != SetPteKind(m_testBuffer, m_pteKindName, lwgpu->GetGpuDevice())) {
        return 0;
    }
    if (OK != m_testBuffer.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create src buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        CleanUp();
        return 0;
    }
    if (OK != m_testBuffer.Map()) {
        ErrPrintf("can't map the source buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return 0;
    }
    m_offset = m_testBuffer.GetExtraAllocSize();
    InfoPrintf("max_memoryTest::Create DMA buffer at 0x%012x\n", m_testBuffer.GetVidMemOffset());

    // allocate buffers for testing
    // basic self-gild stratgy:
    //  * create a reference buffer
    //  * initialize target FB surface with data saved in reference buffer
    //  * do r/w over target FB surface & reference buffer
    //  * when do read, check read back data with corresponding data in reference
    //  * at last, read whole data back and do comparison with reference data
    m_refData = new unsigned char[m_size];
    m_readData = new unsigned char[m_size];

    // if unsuccessful, clean up and exit
    if (!local_status) {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with local_status 0x%x\n",local_status);
        CleanUp();
        return 0;
    }

    return 1;
}

void enhanced_read_writeTest::CleanUp()
{
    DebugPrintf("enhanced_read_writeTest::CleanUp() starts\n");

    if (m_testBuffer.GetMemHandle()) {
        m_testBuffer.Unmap();
        m_testBuffer.Free();
    }

    delete[] m_refData;
    delete[] m_readData;

    FBTest::CleanUp();
}

void enhanced_read_writeTest::Run(void)
{
    SetStatus(Test::TEST_INCOMPLETE);
    InfoPrintf("enhanced_read_writeTest::Run() starts\n");

    local_status = 1;

    InitBuffer();
    fbHelper->SetBackdoor(false);

    for (UINT32 loop = 0; loop < loopCount; ++loop) {
        UINT32 base = RndStream->Random(0, m_size - 512);

        // do reads & writes over, at least, two gobs to make sure all slices are tested
        for (UINT32 i = 0; i < 512; i += 128) {
            // As L2 only request 32B to FB, stress different burst size don't provide more coverage.
            // Use 32B to reduce run time.
            UINT32 offset = i / 128;
            // Increase offset every time to cover more cases
            for (UINT32 sz = 32; sz <= 128; sz += 32, ++offset) {
                StressReadWriteTest(base + i + offset, sz);
            }
        }

        // make sure every single bits of we_ are tested
        for (UINT32 i = 0; i < 32; ++i) {
            base = RndStream->Random(0, m_size - 512);
            StressReadWriteTest(base + i, 1);
        }

        // 8)   Test boundary conditions on each interface, i.e., make sure every wire is exercised.
        for (UINT64 addr = 1; (addr >> 1) < m_size; addr <<= 1) {
            UINT32 sz = RndStream->Random(1, 128);
            StressReadWriteTest((addr >> 1), sz);
        }

        // 13)  Cleans and dirties to the same line(partition, slice, cache line)
        //This test is not scalable/modular as it depends on computing addresses going to same L2 set
        //for each chip. This does not add any coverage for FBPA either and thesefore is being commented out
        //DirtyCleanTest();
    }

    fbHelper->Flush();

    SelfCheck();

    if (local_status) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    }
    else {
        getStateReport()->crcCheckFailed();
        SetStatus(Test::TEST_FAILED_CRC);
    }

    return;
}

void enhanced_read_writeTest::InitBuffer()
{
    for (UINT32 i = 0; i < m_size; ++i) {
        m_refData[i] = 0;
        m_readData[i] = 0;
    }

    fbHelper->SetBackdoor(!m_frontdoor_init);
    Platform::VirtualWr(m_testBuffer.GetAddress(), m_refData, m_size);
    MEM_RD08(reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress()));

}

void enhanced_read_writeTest::SelfCheck()
{
    UINT32 *refBuf = (UINT32 *)m_refData;
    UINT32 *readBuf = (UINT32 *)m_readData;

    fbHelper->SetBackdoor(!m_frontdoor_check);
    Platform::VirtualRd(m_testBuffer.GetAddress(), m_readData, m_size);

    // Compare reference against read back data 4B by 4B
    for (UINT32 i = 0; i < m_size / 4; ++i) {
        if (refBuf[i] != readBuf[i]) {
            ErrPrintf("Difference detected at 0x%010llx(offset 0x%08x) ref(=0x%08x) != dst(=0x%08x)\n",
                      (UINT64)(m_testBuffer.GetVidMemOffset()) + i * 4, i * 4, refBuf[i], readBuf[i]);
            local_status = 0;
        }
    }
}

// 4)   Read various sizes (32-128 bytes)
// 5)   Writes of various sizes (1-128 bytes)
void enhanced_read_writeTest::StressReadWriteTest(UINT64 addr, UINT32 size)
{
    // adjust address according to offset
    // addr += m_offset;
    uintptr_t surfAddr = reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress()) + addr;

    UINT32 this_test_status = 1;

    if (size > FB_MAX_BURST_SIZE) {
        size = FB_MAX_BURST_SIZE;
    }
    if (addr + size > m_size) {
        addr = m_size - size;
    }

    DebugPrintf("enhanced_read_writeTest::StressReadWriteTest() addr 0x%010llx, size %d\n", addr, size);

    //
    // Stress write test:
    //   1) create 256B array as reference data
    //   2) initialize 256B target buffer(128B aligned) with reference data
    //   3) write data with varying sizes(1~128B) at particular address(may not aligned).
    //      update reference data as well
    //   4) read data back through backdoor(or bar0 window or IFB?)
    //   5) compare read back data & reference data
    //
    // Stress read test:
    //   6) read data back through frontdoor with varying sizes(32~128B)
    //   7) compare read back data & reference data
    //

    // generate test data randomly. update reference buffer for later verification
    for (UINT32 i = 0; i < size; ++i) {
        m_refData[addr + i] = (unsigned char)RndStream->Random(0, 255);
    }

    DebugPrintf("enhanced_read_writeTest::frontdoor write test data(%dB) to addr 0x%08x\n",
                size, m_testBuffer.GetVidMemOffset() + addr);
    // Do write test: write data through frontdoor
    Platform::VirtualWr((void*)surfAddr, m_refData + addr, size);

    // a null read to force outstanding write data to be writen into memory.
    MEM_RD08(surfAddr);

    // Do read test: read back data with same burst size from frontdoor
    Platform::VirtualRd((void*)surfAddr, m_readData + addr, size);

    // comparing read back data against reference data byte by byte
    for (UINT32 i = 0; i < size; ++i) {
        if (m_refData[addr + i] != m_readData[addr + i]) {
            ErrPrintf("enhanced_read_writeTest: frontdoor check: Error comparing r/w at 0x%x, %x != %x\n",
                      addr + i, m_refData[addr + i], m_readData[addr + i]);
            // SetStatus(Test::TEST_FAILED);
            local_status = 0;
            this_test_status = 0;
        }
    }

    if (this_test_status) {
        DebugPrintf("enhanced_read_writeTest::StressReadWriteTest() PASS: addr 0x%010llx, size %d\n",
                    m_testBuffer.GetVidMemOffset() + addr, size);
    }
}

// below code is generated by script automatically
UINT32 adrs_to_the_same_l2set_pitch_parts1[] = {
    0x00000700,
    0x00000e00,
    0x00002500,
    0x00002c00,
    0x00004300,
    0x00004a00,
    0x00006100,
    0x00006800,
    0x00009780,
    0x00009f80,
    0x0000b580,
    0x0000bd80,
    0x0000d380,
    0x0000db80,
    0x0000f180,
    0x0000f980,
    0x00010300,
    0x00010a00,
    0x00012100,
    0x00012800,
    0x00014700,
    0x00014e00,
    0x00016500,
    0x00016c00,
    0x00019380,
    0x00019b80,
    0x0001b180,
    0x0001b980,
    0x0001d780,
    0x0001df80,
    0x0001f580,
    0x0001fd80,
};

UINT32 adrs_to_the_same_l2set_pitch_parts2[] = {
    0x00002080,
    0x00003280,
    0x00006900,
    0x00007a00,
    0x00008080,
    0x00009280,
    0x0000c900,
    0x0000da00,
    0x00012580,
    0x00013780,
    0x00016c00,
    0x00017f00,
    0x00018580,
    0x00019780,
    0x0001cc00,
    0x0001df00,
    0x00022880,
    0x00023a80,
    0x00026100,
    0x00027200,
    0x00028880,
    0x00029a80,
    0x0002c100,
    0x0002d200,
    0x00032d80,
    0x00033f80,
    0x00036400,
    0x00037700,
    0x00038d80,
    0x00039f80,
    0x0003c400,
    0x0003d700,
};

UINT32 adrs_to_the_same_l2set_pitch_parts3[] = {
    0x00003200,
    0x00007e80,
    0x00008300,
    0x00009b00,
    0x0000cf80,
    0x0000e700,
    0x00010a80,
    0x00015600,
    0x0001a900,
    0x0001f580,
    0x00021980,
    0x00023100,
    0x00026580,
    0x00027c80,
    0x00028000,
    0x0002cd80,
    0x00033e00,
    0x00037380,
    0x00038f00,
    0x0003a780,
    0x0003c280,
    0x0003db80,
    0x00041700,
    0x00044b80,
    0x0004b480,
    0x0004e800,
    0x00052500,
    0x00053d00,
    0x00055900,
    0x00057180,
    0x00058c00,
    0x0005c080,
};

UINT32 adrs_to_the_same_l2set_pitch_parts4[] = {
    0x00009e00,
    0x0000bc00,
    0x0000ce00,
    0x0000ec00,
    0x00011c80,
    0x00013b80,
    0x00014c80,
    0x00016b80,
    0x00029600,
    0x0002b400,
    0x0002c600,
    0x0002e400,
    0x00031480,
    0x00033380,
    0x00034480,
    0x00036380,
    0x00048e00,
    0x0004ac00,
    0x0004de00,
    0x0004fc00,
    0x00050c80,
    0x00052b80,
    0x00055c80,
    0x00057b80,
    0x00068600,
    0x0006a400,
    0x0006d600,
    0x0006f400,
    0x00070480,
    0x00072380,
    0x00075480,
    0x00077380,
};

UINT32 adrs_to_the_same_l2set_pitch_parts5[] = {
    0x00008280,
    0x0000ab00,
    0x0000d780,
    0x0000fe00,
    0x00011280,
    0x00013b00,
    0x00016780,
    0x00024e80,
    0x0002ac80,
    0x00033c80,
    0x00039400,
    0x0003c080,
    0x0003e900,
    0x00042480,
    0x00045000,
    0x00047980,
    0x00059700,
    0x0005be80,
    0x0005c300,
    0x0005ea80,
    0x00062700,
    0x00065300,
    0x00067a80,
    0x00070f00,
    0x0007ed00,
    0x00087c80,
    0x00088180,
    0x0008a800,
    0x0008d400,
    0x00091180,
    0x00093800,
    0x00096400,
};

UINT32 adrs_to_the_same_l2set_pitch_parts6[] = {
    0x00008080,
    0x0000b780,
    0x0000df80,
    0x00012600,
    0x00017900,
    0x0001c900,
    0x00021080,
    0x00026e80,
    0x00039280,
    0x0003bd00,
    0x0003e480,
    0x00042d00,
    0x0004da80,
    0x00052300,
    0x00054b00,
    0x00057400,
    0x00069880,
    0x0006c780,
    0x0006f080,
    0x00073e00,
    0x00076100,
    0x00078e00,
    0x00082800,
    0x00085600,
    0x0009aa00,
    0x0009cc80,
    0x000a7380,
    0x000a9480,
    0x000ac380,
    0x000b0500,
    0x000b3a00,
    0x000b5c80,
};

// only work for pitchlinear surface
void enhanced_read_writeTest::DirtyCleanTest()
{
    uintptr_t surfAddr = reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress());

    UINT32 *addrs_list = NULL;
    UINT32 addrs_numb = 0;

    switch (m_num_fbpas) {
    case 1:
        addrs_list = adrs_to_the_same_l2set_pitch_parts1;
        addrs_numb = sizeof(adrs_to_the_same_l2set_pitch_parts1) / sizeof(UINT32);
        break;
    case 2:
        addrs_list = adrs_to_the_same_l2set_pitch_parts2;
        addrs_numb = sizeof(adrs_to_the_same_l2set_pitch_parts2) / sizeof(UINT32);
        break;
    case 3:
        addrs_list = adrs_to_the_same_l2set_pitch_parts3;
        addrs_numb = sizeof(adrs_to_the_same_l2set_pitch_parts3) / sizeof(UINT32);
        break;
    case 4:
        addrs_list = adrs_to_the_same_l2set_pitch_parts4;
        addrs_numb = sizeof(adrs_to_the_same_l2set_pitch_parts4) / sizeof(UINT32);
        break;
    case 5:
        addrs_list = adrs_to_the_same_l2set_pitch_parts5;
        addrs_numb = sizeof(adrs_to_the_same_l2set_pitch_parts5) / sizeof(UINT32);
        break;
    case 6:
        addrs_list = adrs_to_the_same_l2set_pitch_parts6;
        addrs_numb = sizeof(adrs_to_the_same_l2set_pitch_parts6) / sizeof(UINT32);
        break;
    default:
        break;
    }

    DebugPrintf("enhanced_read_writeTest::DirtyCleanTest(): number fbpas=%d, %d writes.\n", m_num_fbpas, addrs_numb);
    MASSERT((addrs_numb > 16) && "Don't have enough test data to run this test.");

    for (UINT32 i = 0; i < addrs_numb && i < LTC_WAYS + 1; i++) {
        UINT64 offset = addrs_list[i];
        MASSERT((offset < m_size) && "Address out of buffer range");

        // cache line size is 128B
        for (UINT32 i = 0; i < 128; ++i) {
            m_refData[offset + i] = 0xcc;
        }

        // write to target buffer
        Platform::VirtualWr((void*)(surfAddr + offset), m_refData + offset, 128);
    }

    MEM_RD08(surfAddr);
}
