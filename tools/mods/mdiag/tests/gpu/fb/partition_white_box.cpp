/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2019, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "fermi/gf100/dev_mmu.h"
#include "fermi/gf100/kind_macros.h"

#include "partition_white_box.h"

#include "mdiag/tests/gpu/fb/amap/FermiAddressMapping.h"
#include "mdiag/tests/gpu/fb/amap/PhysicalAddress.h"

#include <algorithm>

extern const ParamDecl partition_white_box_params[] = {
    PARAM_SUBDECL(FBTest::fbtest_common_params),
    MEMORY_SPACE_PARAMS("_buf0", "dma buffer 0 used for testing"),
    UNSIGNED_PARAM("-size", "Size of testing buffer"),
    UNSIGNED_PARAM("-run_test", "Run different test case. Can be one of <0, 1, 2>."),
    UNSIGNED_PARAM("-num_slices", "specify the slice number"),
    SIMPLE_PARAM("-test_reqs_same_rb", "-run_test 0. Requests to same row-bank"),
    SIMPLE_PARAM("-test_reqs_diff_rb", "-run_test 1. Requests to different row-bank"),
    SIMPLE_PARAM("-test_reqs_same_ft", "-run_test 2. Requests to same friend tracker"),
    SIMPLE_PARAM("-frontdoor_check", "check data through frontdoor"),
    SIMPLE_PARAM("-frontdoor_init", "Initialize data through frontdoor"),
    LAST_PARAM
};

enum TEST_CASE {
    REQS_SAME_ROW_BANK = 0,
    REQS_DIFF_ROW_BANK,
    REQS_SAME_FRIEND_TRACKER,
};

// static FermiPhysicalAddress::PAGE_SIZE layout2ps(_PAGE_LAYOUT layout)
// {
//     FermiPhysicalAddress::PAGE_SIZE ps = FermiPhysicalAddress::PS4K;

//     switch (layout) {
//     case PAGE_VIRTUAL_4KB:
//         ps = FermiPhysicalAddress::PS4K;
//         break;
//     case PAGE_VIRTUAL_64KB:
//         ps = FermiPhysicalAddress::PS64K;
//         break;
// //     case PAGE_VIRTUAL_128KB:
// //         ps = FermiPhysicalAddress::PS128K;
// //         break;
//     default:
//         ErrPrintf("layout2ps() unknown page size layout %d", layout);
//         break;
//     }

//     return ps;
// }

static void computePartSlice(UINT64 PAKS, UINT32 numb_parts, UINT32 numb_slices, UINT32 &part, UINT32 &slice);

partition_white_boxTest::partition_white_boxTest(ArgReader *params) :
    FBTest(params),
    local_status(1),
    m_num_partitions(8),
    m_num_subpartitions(2),
    m_num_slices(2),
    m_num_rows(12),
    m_num_banks(8),
    m_refData(NULL),
    m_bufData(NULL),
    m_dmaBufBegin(0),
    m_dmaBufEnd(0),
    m_frontdoor_check(false),
    m_frontdoor_init(false)
{
    ParseDmaBufferArgs(m_dmaBuffer, params, "buf0", &m_pteKindName, 0);
    m_size = params->ParamUnsigned("-size", 32 * 1024 * 1024);
    m_num_slices = params->ParamUnsigned("-num_slices", 2);
    m_run_test = params->ParamUnsigned("-run_test", REQS_SAME_ROW_BANK);
    m_frontdoor_check = params->ParamPresent("-frontdoor_check");
    m_frontdoor_init = params->ParamPresent("-frontdoor_init");

    FBTest::DebugPrintArgs();

}

partition_white_boxTest::~partition_white_boxTest()
{

}

STD_TEST_FACTORY(partition_white_box, partition_white_boxTest)

// Setup a buffer for testing
int partition_white_boxTest::Setup()
{
    local_status = 1;

    InfoPrintf("partition_white_boxTest::Setup() starts\n");
    getStateReport()->enable();

    if (!FBTest::Setup()) {
        ErrPrintf("partition_white_boxTest::Setup() fail\n");
        return 0;
    }

    m_dramCap = fbHelper->FBSize()/fbHelper->NumFBPAs();

    _DMA_TARGET target = _DMA_TARGET_VIDEO;
    // _PAGE_LAYOUT layout = PAGE_VIRTUAL_4KB;

    if (m_size >= m_dramCap) {
        m_size = m_dramCap / 2;
    }

    m_dmaBuffer.SetArrayPitch(m_size);
    m_dmaBuffer.SetColorFormat(ColorUtils::Y8);
    m_dmaBuffer.SetForceSizeAlloc(true);
    m_dmaBuffer.SetLocation(TargetToLocation(target));
    m_dmaBuffer.SetProtect(Memory::ReadWrite);
    m_dmaBuffer.SetPageSize(4);
    // SetPageLayout(m_dmaBuffer, layout);
    if (OK != SetPteKind(m_dmaBuffer, m_pteKindName, lwgpu->GetGpuDevice())) {
        return 0;
    }
    if (OK != m_dmaBuffer.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create src buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        CleanUp();
        return 0;
    }
    if (OK != m_dmaBuffer.Map()) {
        ErrPrintf("can't map the source buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        CleanUp();
        return 0;
    }
    InfoPrintf("partition_white_boxTest::Setup() Dma buffer at 0x%012x. Size 0x%llx. Offset %llx\n",
               m_dmaBuffer.GetVidMemOffset(), m_dmaBuffer.GetSize(), m_dmaBuffer.GetExtraAllocSize());

    m_dmaBufEnd = m_dmaBuffer.GetVidMemOffset() + m_dmaBuffer.GetSize();
    m_dmaBufBegin = m_dmaBufEnd - m_size;

    fbHelper->SetBackdoor(false);
    MEM_WR32(reinterpret_cast<uintptr_t>(m_dmaBuffer.GetAddress()), 0xdeadbeef);
    fbHelper->Flush();
    MEM_RD32(reinterpret_cast<uintptr_t>(m_dmaBuffer.GetAddress()));
    fbHelper->SetBackdoor(true);

    try {
        m_refData = new unsigned char[m_size];
        m_bufData = new unsigned char[m_size];
    }
    catch (const std::bad_alloc&)
    {
        ErrPrintf("can't create data buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        CleanUp();
        return 0;
    }

    InfoPrintf("partition_white_boxTest::Setup() done\n");

    return 1;
}

void partition_white_boxTest::CleanUp()
{
    DebugPrintf("partition_white_boxTest::CleanUp() starts\n");

    if (m_dmaBuffer.GetSize()) {
        m_dmaBuffer.Unmap();
        m_dmaBuffer.Free();
    }

    if (m_refData != NULL)
        delete[] m_refData;
    if (m_bufData != NULL)
        delete[] m_bufData;

    FBTest::CleanUp();

    DebugPrintf("partition_white_boxTest::CleanUp() done\n");
}

//
// The test:
//
// Testplan reference:
// FB_PA_Testing_Strategy.doc, 3.1.1 and 3.4
//
// Features tested:
// row bank, subpartition, friend tracker ways, slices,
//
// Description:
// nail the FB with whitebox fullchip testing
//
// Interesting patterns that might be corner cases:
//   All requests to same row-bank
//   All requests to different row-bank
//   All requests to 1 subpartition
//   All requests such that they will be in the same FriendTracker way
//   All requests from the same slice
//   All requests to the same bank
//
// Testing strategy:
//
// Initialization:
//  * create a reference buffer(in sys mem) initialized with sequence of 32B data
//    { 0x1, 0x4, 0x8, ... }
//  * create a buffer for testing. write reference data into testing buffer
//
// Testing:
//  * given a randomly generated physical address.
//  * find addresses within testing buffer which will go into the same row-bank in same partition/subpatition
//  * create requests to these addresses
//
//  * given a randomly generated address `PA'.
//  * find addresses within testing buffer which will go into *different* row in the same bank with previous address
//  * find addresses within testing buffer which will go into *different* row/bank with  previous address
//  * create requests to these addresses
//
//  * given a randomly generated address `PA'
//  * find addresses within testing buffer which will go into the same FriendTracker set
//  * create requests to these addresses
//
void partition_white_boxTest::Run(void)
{
    SetStatus(Test::TEST_INCOMPLETE);
    InfoPrintf("partition_white_boxTest::Run() starts\n");

    local_status = 1;

    fbHelper->SetBackdoor(!m_frontdoor_init);
    InitBuffers();

    fbHelper->SetBackdoor(false);

    switch (m_run_test) {
    case REQS_SAME_ROW_BANK:
        // -run_test 0
        // there is no restriction on pte kind of the dmabuffer
        Test_ReqsToSameRowBank();
        break;
    case REQS_DIFF_ROW_BANK:
        // -run_test 1 -pte_kind_buf0 PITCH_NO_SWIZZLE
        // Use straight wire kind to generate address
        Test_ReqsToDiffRowBank2();
        break;
    case REQS_SAME_FRIEND_TRACKER:
        // -run_test 2 -pte_kind_buf0 PITCH_NO_SWIZZLE
        // Use straight wire kind to generate address
        Test_ReqsToSameFT();
        break;
    default:
        Test_ReqsToSameRowBank();
        break;
    }

    // a null read to make sure all outstanding requests go into framebuffer
    MEM_RD08(reinterpret_cast<uintptr_t>(m_dmaBuffer.GetAddress()));
    fbHelper->Flush();

    // check the result
    fbHelper->SetBackdoor(!m_frontdoor_check);
    Platform::VirtualRd(m_dmaBuffer.GetAddress(), m_bufData, m_size);
    if (CompareBuffer(m_refData, m_bufData, m_size)) 
    {
        local_status = 0;
        ErrPrintf("partition_white_boxTest failed!\n");
    }

    if (local_status) 
    {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    }
    else 
    {
        SetStatus(Test::TEST_FAILED_CRC);
    }

    return;
}

int partition_white_boxTest::CompareBuffer(unsigned char *src, unsigned char *dst, UINT32 size)
{
    int rc = 0;

    UINT32 *p_src32 = (UINT32 *)src;
    UINT32 *p_dst32 = (UINT32 *)dst;

    for (UINT32 i = 0; i < size / 4; i++) {
        if (p_src32[i] != p_dst32[i]) {
            ErrPrintf("Difference detected at 0x%010llx(offset 0x%08x) src(=0x%08x) != dst(=0x%08x)\n",
                      (UINT64)m_dmaBuffer.GetVidMemOffset() + i * 4, i * 4, p_src32[i], p_dst32[i]);
            rc = 1;
        }
    }

    return rc;
}

bool SameRowBank(UINT32 part_v, UINT32 subpart_v, UINT32 row_v, UINT32 bank_v,
                 UINT32 part, UINT32 subpart, UINT32 row, UINT32 bank)
{
    return part == part_v && subpart == subpart_v && row == row_v && bank == bank_v;
}

bool DiffRow(UINT32 part_v, UINT32 subpart_v, UINT32 row_v, UINT32 bank_v,
             UINT32 part, UINT32 subpart, UINT32 row, UINT32 bank)
{
    return part == part_v && subpart == subpart_v && row != row_v && bank == bank_v;
}

bool DiffBank(UINT32 part_v, UINT32 subpart_v, UINT32 row_v, UINT32 bank_v,
              UINT32 part, UINT32 subpart, UINT32 row, UINT32 bank)
{
    // don't need same row in the case
    row_v = 0;
    row = 0;

    return part == part_v && subpart == subpart_v && bank != bank_v;
}

// Helper function: look for a physical address whitch meet the requirement specified by `condition'(row, bank, col)
//   the region of address from ``start'' to ``end'', return address saved through ``addr''
int partition_white_boxTest::FindPhysAddrByRowBank(Cond_Func_t condition, UINT32 part, UINT32 subpart, UINT32 row, UINT32 bank,
                                                   UINT64 start, UINT64 end, UINT64 *addr)
{
    // 4K is used for page size by default
    FermiPhysicalAddress::KIND kind = FermiPhysicalAddress::PITCHLINEAR;
    FermiPhysicalAddress::APERTURE aperture = FermiPhysicalAddress::LOCAL;
    FermiPhysicalAddress::PAGE_SIZE pagesize = FermiPhysicalAddress::PS4K;

    int rc = 0;

    for (UINT64 t_addr = start; t_addr < end; t_addr += 128) {
        FBAddress physAddr(FermiFBAddress(t_addr, kind, pagesize, aperture));
        if (condition(part, subpart, row, bank,
                      physAddr.partition(), physAddr.subPartition(), physAddr.row(), physAddr.bank())) {
            *addr = t_addr;
            rc = 1;
            break;
        }
    }

    return rc;
}

// Helper function: look for a physical address whitch meet the requirement specified by `condition'(Friend Tracker)
//   the region of address from ``start'' to ``end'', return address saved through ``addr''
int partition_white_boxTest::FindPhysAddrByFT(bool rd, UINT32 ftSet, UINT64 start, UINT64 end, UINT64 *addr)
{
    // 4K is used for page size by default
    FermiPhysicalAddress::KIND kind = FermiPhysicalAddress::PITCHLINEAR;
    FermiPhysicalAddress::APERTURE aperture = FermiPhysicalAddress::LOCAL;
    FermiPhysicalAddress::PAGE_SIZE pagesize = FermiPhysicalAddress::PS4K;

    int rc = 0;

    for (UINT64 t_addr = start; t_addr < end; t_addr += 128) {
        FBAddress physAddr(FermiFBAddress(t_addr, kind, pagesize, aperture));
        if ((rd && physAddr.rdFtSet() == ftSet) || physAddr.wrFtSet() == ftSet) {
            *addr = t_addr;
            rc = 1;
            break;
        }
    }

    return rc;
}

void partition_white_boxTest::InitBuffers()
{
    UINT32 *refBuf = (UINT32 *)m_refData;

    for (UINT32 i = 0; i < m_size / 4; ++i) 
    {
        refBuf[i] = i * 4;
    }

    Platform::VirtualWr(m_dmaBuffer.GetAddress(), m_refData, m_size);
    MEM_RD08(reinterpret_cast<uintptr_t>(m_dmaBuffer.GetAddress()));
}

// Basic routine used to test buffer r/w
// write to buffer:
//   write data into target buffer. update reference buffer at the same time.
// read from buffer:
//   read data back from target buffer and compare with corresponding data in reference buffer.
//
// tag: attached to the 2 msbs of the write data. used for debug
void partition_white_boxTest::Test_ReadWriteBuffer(bool rd, UINT64 offset, UINT64 size, UINT32 tag)
{
    UINT32 *refBuf = (UINT32 *)m_refData;

    // this test isn't supposed to stress all possible alignment
    size += offset & 0x3;
    offset &= ~(UINT64)0x3;
    uintptr_t addr = reinterpret_cast<uintptr_t>(m_dmaBuffer.GetAddress()) + (UINT32)offset;

    if (rd) 
    {
        Platform::VirtualRd((void*)addr, m_bufData, (UINT32)size);
        // check read back data byte by byte
        for (UINT32 j = 0; j < size; j++)  
        {
            if (m_bufData[j] != m_refData[offset + j]) 
            {
                ErrPrintf("Read back data doesn't match reference data at 0x%010lx(ref=0x%02x != data=0x%02x)\n",
                          offset + j, m_refData[offset + j], m_bufData[j]);
                local_status = 0;
            }
        }
    }
    else 
    {
        for (UINT32 j = 0; j < size / 4; j++) 
        {
            refBuf[offset / 4 + j] = (UINT32)(offset - m_dmaBuffer.GetVidMemOffset() + j * 4) | (tag << 30);
        }

        Platform::VirtualWr((void*)addr, m_refData + (UINT32)offset, (UINT32)size);
    }

}

// Create a lot of r/w requests to the same row bank within one(subpartition)
//
// * get a random 512B aligned address within the test buffer
// * create a lot of r/w requests go into the same 512B region(they will go into the same row-bank)
// * compare the result with reference at the end of testing
void partition_white_boxTest::Test_ReqsToSameRowBank()
{
    FermiPhysicalAddress::KIND kind = FermiPhysicalAddress::PITCHLINEAR;
    FermiPhysicalAddress::APERTURE aperture = FermiPhysicalAddress::LOCAL;
    FermiPhysicalAddress::PAGE_SIZE pagesize = FermiPhysicalAddress::PS4K;

    UINT32 pte_kind = m_dmaBuffer.GetPteKind();
    UINT32 addr_mask = 0xff;    // pitch 256B & 2slice
    UINT32 interleave = 0x100;

    // 2 slices
    if (KIND_PITCH(pte_kind)) {
        addr_mask = 0xff;
        interleave = 0x200;
        kind = KIND_NO_SWIZZLE(m_dmaBuffer.GetPteKind())?
            FermiPhysicalAddress::PITCH_NO_SWIZZLE: FermiPhysicalAddress::PITCHLINEAR;
    }
    else if (KIND_BLOCKLINEAR(pte_kind)) {
        addr_mask = 0x1ff;
        interleave = 0x400;
        kind = FermiPhysicalAddress::BLOCKLINEAR_GENERIC;
    }
    else {
        ErrPrintf("Unknown PTE_KIND: %d", pte_kind);
        Fail(Test::TEST_FAILED);
    }

    if (m_num_slices == 4) {
        addr_mask >>= 1;
    }
    else if (m_num_slices == 2) {
        ;
    }
    else {
        ErrPrintf("Unknown number of slices: %d", fbHelper->NumL2Slices());
        Fail(Test::TEST_FAILED);
    }

    for (unsigned int i = 0; i < loopCount; i++) {
        UINT64 paddr = 0x0, offset = 0x0, offset_1k_aligned = 0x0, size = 128;
        UINT32 part = 0, subpart = 0, row = 0, bank = 0;

        // 512B is enough to make sure all address go into the same row & bank.
        // Get randomly generated 512B aligned address here
        offset = RndStream->Random(0, m_size - 1) & ~(UINT64)addr_mask;
        offset_1k_aligned = offset & ~(UINT64)0x3ff;

        // Generate a bunch of requests within the 512B region, they will go into the same row bank
        // the loop count should be larger than the 2 * max length of FriendTracker list
        for (unsigned int j = 0; j < 32; j++) {
            // randomly generate a address adjuestment
            UINT64 adjust = RndStream->Random(0, addr_mask);
            UINT32 max_size = min(addr_mask - adjust + 1, (UINT64)128);
            size = RndStream->Random(1, max_size);
//             size = 128 - paddr % 128;

            paddr = m_dmaBuffer.GetVidMemOffset()
                + ((offset + adjust + j * interleave) & 0x3ff)
                + offset_1k_aligned;
            FBAddress physAddr(FermiFBAddress(paddr, kind, pagesize, aperture));
            part = physAddr.partition();
            subpart = physAddr.subPartition();
            row = physAddr.row();
            bank = physAddr.bank();

            DebugPrintf("partition_white_boxTest::Test_ReqsToSameRowBank() part %d, subpart %d, row %d, bank %d -> fb_addr 0x%012x\n",
                        part, subpart, row, bank, paddr);

            Test_ReadWriteBuffer(j >= 16, offset + adjust, size);
            // paddr += 128;
        }
        MEM_RD08(reinterpret_cast<uintptr_t>(m_dmaBuffer.GetAddress()));
    }
}

// Create a lot of r/w requests go to different row-banks within one(subpartition)
// varies row bank without changing partition & subpartition
void partition_white_boxTest::Test_ReqsToDiffRowBank()
{
    // 4K is used for page size by default
    FermiPhysicalAddress::KIND kind = FermiPhysicalAddress::PITCHLINEAR;
    FermiPhysicalAddress::APERTURE aperture = FermiPhysicalAddress::LOCAL;
    FermiPhysicalAddress::PAGE_SIZE pagesize = FermiPhysicalAddress::PS4K;

    for (unsigned int i = 0; i < loopCount; i++) {
        UINT64 paddr = 0x0, offset = 0x0, size = 0x0;
        UINT32 part = 0, subpart = 0, bank = 0, row = 0;

        // varying row within the same bank
        InfoPrintf("partition_white_boxTest::Test_ReqsToDiffRowBank() Test requests to different rows in the same bank\n");
        paddr = m_dmaBuffer.GetVidMemOffset() + RndStream->Random(0, m_size - 1);

        for (UINT32 j = 0; j < 64; j++) {
            FBAddress physAddr(FermiFBAddress(paddr, kind, pagesize, aperture));

            part = physAddr.partition();
            subpart = physAddr.subPartition();
            row = physAddr.row();
            bank = physAddr.bank();

            InfoPrintf("partition_white_boxTest::Test_ReqsToDiffRowBank() part %d, subpart %d, row %d, bank %d -> paddr 0x%012x\n",
                       part, subpart, row, bank, paddr);
            offset = paddr - m_dmaBuffer.GetVidMemOffset();
            size = 128 - offset % 128;

            Test_ReadWriteBuffer(false, offset, size);
            // Test_ReadWriteBuffer(RndStream->Random(0, 1) == 1, offset, 32);
            paddr += RndStream->Random(256, 512);

            if (FindPhysAddrByRowBank(DiffRow, part, subpart, row, bank, paddr, m_dmaBufEnd, &paddr) == 0) {
                break;
            }
        }

        // varying bank with random row
        InfoPrintf("partition_white_boxTest::Test_ReqsToDiffRowBank() Test requests to different row-banks\n");
        paddr = m_dmaBuffer.GetVidMemOffset() + RndStream->Random(0, m_size - 1);
        for (UINT32 j = 0; j < 1024; j++) {
            FBAddress physAddr(FermiFBAddress(paddr, kind, pagesize, aperture));

            part = physAddr.partition();
            subpart = physAddr.subPartition();
            row = physAddr.row();
            bank = physAddr.bank();

            InfoPrintf("partition_white_boxTest::Test_ReqsToDiffRowBank() part %d, subpart %d, row %d, bank %d -> paddr 0x%012x\n",
                       part, subpart, row, bank, paddr);
            offset = paddr - m_dmaBuffer.GetVidMemOffset();
            size = 128 - offset % 128;

            Test_ReadWriteBuffer(false, offset, size);
//            Test_ReadWriteBuffer(RndStream->Random(0, 1) == 1, offset, 32);
            paddr += 128;

            if (FindPhysAddrByRowBank(DiffBank, part, subpart, row, bank, paddr, m_dmaBufEnd, &paddr) == 0) {
                break;
            }
        }
    }
}

// another version of Test_ReqsToDiffRowBank with PITCH_NO_SWIZZLE support
// proposals for ecover plan:
//  1. record number of different row-banks hit within a fixed number of requests.
//  2. record number of different row-banks the sequence of requests for until partition/subpartition changes
//     callwlate the percentage of requests hit different row-banks vs. total requests
void partition_white_boxTest::Test_ReqsToDiffRowBank2()
{
    for (unsigned int i = 0; i < loopCount; i++) {
        UINT64 paddr = 0x0, offset = 0x0;
        UINT32 part = 0, slice = 0, subpart = 0;
        UINT32 new_part = 0, new_subpart = 0;

        bool firstRun = true;
        // paddr = m_dmaBuffer.GetVidMemOffset() + RndStream->Random(0, m_size - 1);

        for (UINT32 rd = 0; rd < 2; rd++) {
            for (UINT32 j = 0; j < 1024; j++) {
                // with pte kind PITCH_NO_SWIZZLE, PAKS = paddr
                // PAKS2QRO(paddr, m_num_partitions, Q, R, O);
                paddr = m_size - RndStream->Random(512, 1024) * j - RndStream->Random(196, 360) * j;

                offset = (paddr - m_dmaBuffer.GetVidMemOffset()) % m_size;
                paddr = m_dmaBuffer.GetVidMemOffset() + offset;

                computePartSlice(paddr, m_num_partitions, 2, new_part, slice);
                if (m_num_slices == 4) {
                    new_subpart = 0x2 & slice;
                }
                else if (m_num_slices == 2) {
                    new_subpart = slice;
                }
                else {
                    MASSERT(0 && "number of XBAR slices must be 2 or 4.");
                }

                if (firstRun || (new_part == part && new_subpart == subpart)) {
                    firstRun = false;

                    part = new_part;
                    subpart = new_subpart;

                    //const UINT64 size = 128 - offset % 128;

                    // Test_ReadWriteBuffer(false, offset, size);
                    DebugPrintf("partition_white_boxTest::Test_ReqsToSameRowBank2() part %d, subpart %d -> fb_addr 0x%012x\n",
                                part, subpart, paddr);
                    // attach tag 1' to the 30th bit of write data.
                    Test_ReadWriteBuffer(rd == 1, offset, 32, 1);
                }
            }
            // paddr += RndStream->Random(128, 256) * j + RndStream->Random(64, 196) * j / 2;
        }
    }
}

void partition_white_boxTest::Test_ReqsToSameFT()
{
    // kind & aperture can only be PITCH & LOCAL respectively
    FermiPhysicalAddress::KIND kind = FermiPhysicalAddress::PITCHLINEAR;
    FermiPhysicalAddress::APERTURE aperture = FermiPhysicalAddress::LOCAL;
    FermiPhysicalAddress::PAGE_SIZE pagesize = FermiPhysicalAddress::PS4K;

    if (KIND_PITCH(m_dmaBuffer.GetPteKind())) {
        kind = KIND_NO_SWIZZLE(m_dmaBuffer.GetPteKind())?
            FermiPhysicalAddress::PITCH_NO_SWIZZLE: FermiPhysicalAddress::PITCHLINEAR;
    }

    switch (m_dmaBuffer.GetPageSize()) {
    case 0:
        pagesize = FermiPhysicalAddress::PS4K;
        break;
    case 4:
        pagesize = FermiPhysicalAddress::PS4K;
        break;
    case 64:
    case 128:
        pagesize = FermiPhysicalAddress::PS64K;
        break;
    default:
        ErrPrintf("Unknown page size %dK\n", m_dmaBuffer.GetPageSize());
        MASSERT(0);
        break;
    }

    for (unsigned int i = 0; i < 2 * loopCount; i++) {
        UINT64 paddr = 0x0, offset = 0x0, size = 128;
        UINT32 ftSet = 0;
        bool rd = (i % 2 == 1);

        paddr = m_dmaBuffer.GetVidMemOffset() + RndStream->Random(0, m_size - 1);
        FBAddress physAddr(FermiFBAddress(paddr, kind, pagesize, aperture));

        if (rd) {
            ftSet = physAddr.rdFtSet();
        }
        else {
            ftSet = physAddr.wrFtSet();
        }

        for (unsigned int j = 0; j < 64; j++) {
            InfoPrintf("partition_white_boxTest::Test_ReqsToSameFT() %s paddr 0x%010x ftSet 0x%08x\n",
                       rd? "READ": "WRITE", paddr, ftSet);
            offset = paddr - m_dmaBuffer.GetVidMemOffset();
            size = 128 - paddr % 128;

            Test_ReadWriteBuffer(rd, offset, size);
            paddr += 128;

            if (FindPhysAddrByFT(rd, ftSet, paddr, m_dmaBufEnd, &paddr) == 0) {
                break;
            }
        }
        MEM_RD08(reinterpret_cast<uintptr_t>(m_dmaBuffer.GetAddress()));
    }
}

// helper function ported from address mapping
static UINT32 computePartitionInterleave(UINT32 numb_parts)
{
    UINT32 PartitionInterleave = 0;  // set to some value because compiler complaining that this may be used uninitialized

    // assuming we are running test with local mem support
    switch (numb_parts) {
    case 5:
    case 3:
        PartitionInterleave = 2048; // 2K for slice[1] below partition bits
        break;
    case 8:
    case 7:
    case 6:
    case 4:
    case 2:
    case 1:
        PartitionInterleave = 2048 / 2; // 2K/2 == 1K
        break;
    default:
        // TBD[Fung] assert here?
        break;
    }

    return PartitionInterleave;
}

static void PAKS2QRO(const UINT64 PAKS, const UINT32 num_parts, UINT64 &Q, UINT64 &R, UINT64 &O)
{
    // compute Q,R,O
    const UINT32 PartitionInterleave = computePartitionInterleave(num_parts);

    O = PAKS % PartitionInterleave;
    UINT32 tmp = PAKS / PartitionInterleave;
    Q = tmp / num_parts;
    R = tmp % num_parts;
}

// static UINT64 QRO2PAKS(const UINT32 num_parts, UINT64 &Q, UINT64 &R, UINT64 &O)
// {
//     UINT64 PAKS;

//     const UINT32 PartitionInterleave = computePartitionInterleave(num_parts);

//     // make sure R, O are in proper range
//     O = O % PartitionInterleave;
//     R = R % num_parts;

//     PAKS = (Q * num_parts + R) * PartitionInterleave + O;

//     return PAKS;
// }

#pragma optimize("", off)

static UINT32 computeSlice(UINT32 numb_slices, UINT32 numb_parts, UINT64 Q, UINT64 R, UINT64 O)
{
    UINT64 slice = 0;

    if (numb_slices == 4) {
        switch(numb_parts) {
        case 7:
            slice = ((bit<3>(Q) ^ bit<2>(Q) ^ bit<0>(Q) ^ bit<0>(numb_parts)) << 1) | bit<9>(O);
            break;
        case 8:
        case 4:
        case 2:
        case 1:
            slice = (bit<0>(Q) << 1) | bit<9>(O);
            break;
        case 6:
            slice = ((bit<0>(Q)  ^ bit<0>(R) ^ bit<3>(Q)    // post divide R swizzle
                      ^ bit<4>(Q) ^ bit<5>(Q) ^ bit<6>(Q) ^ bit<7>(Q) ^ bit<8>(Q) ^ bit<9>(Q) ^ bit<10>(Q)
                      ^ bit<11>(Q) ^ bit<12>(Q) ^ bit<13>(Q) ^ bit<14>(Q) ^ bit<15>(Q) ^ bit<16>(Q) ^ bit<17>(Q)
                      ^ bit<18>(Q) ^ bit<19>(Q) ^ bit<20>(Q) ^ bit<21>(Q) ^ bit<22>(Q) ^ bit<23>(Q) ^ bit<24>(Q)
                      ^ bit<25>(Q) ^ bit<26>(Q) ^ bit<27>(Q) ^ bit<28>(Q) ^ bit<29>(Q) ^ bit<30>(Q) ^ bit<31>(Q)
                         ) << 1  ) | bit<9>(O);
            break;
        case 5:
        case 3:
            slice =  bits<10,9>(O);
            break;
        default:
            break;
        }
    }
    else if (numb_slices == 2) {
        switch(numb_parts) {
        case 7:
            slice = bit<3>(Q) ^ bit<2>(Q) ^ bit<0>(Q) ^ bit<0>(numb_parts);
            break;
        case 8:
        case 4:
        case 2:
        case 1:
            slice = bit<0>(Q);
            break;
        case 6:
            slice = bit<0>(Q) ^ bit<0>(R) ^ bit<3>(Q)    // post divide R swizzle
                ^ bit<4>(Q)  ^ bit<5>(Q)  ^ bit<6>(Q)  ^ bit<7>(Q)  ^ bit<8>(Q)  ^ bit<9>(Q)  ^ bit<10>(Q)
                ^ bit<11>(Q) ^ bit<12>(Q) ^ bit<13>(Q) ^ bit<14>(Q) ^ bit<15>(Q) ^ bit<16>(Q) ^ bit<17>(Q)
                ^ bit<18>(Q) ^ bit<19>(Q) ^ bit<20>(Q) ^ bit<21>(Q) ^ bit<22>(Q) ^ bit<23>(Q) ^ bit<24>(Q)
                ^ bit<25>(Q) ^ bit<26>(Q) ^ bit<27>(Q) ^ bit<28>(Q) ^ bit<29>(Q) ^ bit<30>(Q) ^ bit<31>(Q);
            break;
        case 5:
        case 3:
            slice =  bit<10>(O);
            break;
        default:
            std::stringstream ss;
            ss << "Unsupported number of partitions in address mapping. Num partitions: " << numb_parts << std::endl;
            break;
        }
    }
    else {
        ;
        // TBD[Fung] add assert here
    }

    return (UINT32)slice;
}

#pragma optimize("", on)

static UINT32 computePart(UINT32 numb_parts, UINT64 Q, UINT64 R)
{
    UINT64 partition = 0;

    std::string error_string;
    // compute partition
    switch(numb_parts) {
    case 8:
    case 5:
    case 4:
    case 3:
    case 2:
        partition = R;
        break;
    case 7:
        partition = (R + bits<2,0>(Q)) % 7;
        break;
    case 6:
        partition = (bit<2>(Q) ^ bit<0>(R)) | (bits<2,1>(R) << 1);
        break;
    case 1:
        partition = 0;
        break;
    default:
        std::stringstream ss;
        ss << "Unsupported number of partitions in address mapping. Num partitions: " << numb_parts << std::endl;
        error_string = ss.str();
        break;
    }

    return (UINT32)partition;

}

void computePartSlice(UINT64 PAKS, UINT32 numb_parts, UINT32 numb_slices, UINT32 &part, UINT32 &slice)
{
    UINT64 Q = 0, R = 0, O = 0;

    PAKS2QRO(PAKS, numb_parts, Q, R, O);
    slice = computeSlice(numb_slices, numb_parts, Q, R, 0);
    part = computePart(numb_parts, Q, R);
}
