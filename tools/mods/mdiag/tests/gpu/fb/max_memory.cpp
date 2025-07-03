/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012,2019-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "max_memory.h"
#include "sim/IChip.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"
#include "mdiag/utils/surfutil.h"

#include "t12x/t124/dev_armc.h"
#include "t12x/t124/dev_aremc.h"
#include "t12x/t124/project_relocation_table.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"

const UINT64 BUFFER_SIZE_256B = 256;
const UINT64 BUFFER_SIZE_1K = 1024;
const UINT64 BUFFER_SIZE_4K = 4 * 1024;
const UINT64 BUFFER_SIZE_16K = 16 * 1024;

const UINT64 BLK_RW_TEST_BUFFER_SIZE = BUFFER_SIZE_256B;

//
// This test reads/writes to min/max address using BAR0 window.
//
extern const ParamDecl max_memory_params[] = {
    PARAM_SUBDECL(FBTest::fbtest_common_params),
    MEMORY_SPACE_PARAMS("_buf0",    "dma buffer 0 used for testing"),
    UNSIGNED_PARAM("-maxErrCount",  "max error allowed during running test."),
    UNSIGNED_PARAM("-size",         "size of dma buffer for testing"),
    UNSIGNED_PARAM("-stride",       "Buffer Rd/Wr Stride"),
    UNSIGNED_PARAM("-mem_loc",      "specify the location of memory for r/w. Can be 0(vid), 1(peer), 2(coh), 3(ncoh))"),
    UNSIGNED_PARAM("-test_phys_addr_percent", "percent of address wrt max_addr range"),
    UNSIGNED64_PARAM("-test_phys_addr", "address to test"),
    UNSIGNED_PARAM("-test_phys_size", "size of data for r/w test"),
    UNSIGNED_PARAM("-access_mode",       "0=use max memory capacity callwlation and access required amount of memory by default through BAR0, 1=use specified pa to allocate buffer with given page size, kind, and aperture through BAR1, 2=use given pa and size to access through BAR0, others=use non-mode access"),
    UNSIGNED_PARAM("-page_size_buf0", "page size of dmabuffer buf0"),
    UNSIGNED_PARAM("-enable_print", "enable printing accessed address"),
    LAST_PARAM
};

max_memoryTest::max_memoryTest(ArgReader *reader) :
    FBTest(reader),
    error_count(0),
    params(reader),
    m_dramCap(0),
    m_num_fbpas(8),
    m_bom(0),
    m_offset(0),
    m_size(4096),
    m_stride(128),
    m_access_mode(100), // default mode; does not use any mode specific actions
    m_page_size(4),
    m_enable_print(0),
    m_target(LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM),
    m_blk_phys_rw_test_buff_size(BUFFER_SIZE_4K)
{
    ParseDmaBufferArgs(m_dmaBuffer, params, "buf0", &m_pteKindName, 0);

    maxErrCount = params->ParamUnsigned("-maxErrCount", 1024);
    m_size = params->ParamUnsigned("-size", 4096);
    m_phys_addr = params->ParamUnsigned64("-test_phys_addr", 0x0);
    m_phys_size = params->ParamUnsigned("-test_phys_size", 0x0);
    m_phys_addr_percent = params->ParamUnsigned("-test_phys_addr_percent", 101);
    m_stride = params->ParamUnsigned("-stride", 128);
    m_access_mode = params->ParamUnsigned("-access_mode", 100);
    m_page_size = params->ParamUnsigned("-page_size_buf0", 4);
    m_enable_print = params->ParamUnsigned("-enable_print", 0);

    if (params->ParamPresent("-mem_loc")) {
        m_target = params->ParamUnsigned("-mem_loc", 0x0);
        switch (m_target) {
        case 0x0:
            m_dmaBuffer.SetLocation(Memory::Fb);
            if(m_enable_print>0)
              InfoPrintf("max_memoryTest: Setting aperture to vidmem \n");
            break;
        case 0x2:
            m_dmaBuffer.SetLocation(Memory::Coherent);
            if(m_enable_print>0)
              InfoPrintf("max_memoryTest: Setting aperture to coherent \n");
            break;
        case 0x3:
            m_dmaBuffer.SetLocation(Memory::NonCoherent);
            if(m_enable_print>0)
              InfoPrintf("max_memoryTest: Setting aperture to noncoherent \n");
            break;
        default:
            ErrPrintf("Unsupported mem location: %d, expected 0x0, 0x2, 0x3.\n", m_dmaBuffer.GetLocation());
            MASSERT(0 && "Unsupported aperture");
            break;
        }
    }
    else {
        if(m_enable_print>0)
              InfoPrintf("max_memoryTest: Setting aperture to through default mechanism \n");
        switch (m_dmaBuffer.GetLocation()) {
        case Memory::Fb:
            m_target = LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM;
            break;
        case Memory::Coherent:
            m_target = LW_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_COHERENT;
            break;
        case Memory::NonCoherent:
            m_target = LW_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_NONCOHERENT;
            break;
        default:
            ErrPrintf("Unsupported mem location: %d, expected 0x0, 0x2, 0x3.\n", m_dmaBuffer.GetLocation());
            MASSERT(0 && "Unsupported aperture");
            break;
        }
    }

    FBTest::DebugPrintArgs();
    DebugPrintf("max_memoryTest args: -maxErrCount %d\n", maxErrCount);
}

max_memoryTest::~max_memoryTest(void)
{
}

STD_TEST_FACTORY(max_memory, max_memoryTest)

// a little extra setup to be done
int max_memoryTest::Setup(void)
{
    local_status = 1;

    InfoPrintf("max_memoryTest::Setup() starts\n");

    getStateReport()->enable();

    if (!FBTest::Setup()) {
        ErrPrintf("max_memoryTest::Setup() Fail\n");
        return 0;
    }

    // if unsuccessful, clean up and exit
    if(!local_status) {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with local_status 0x%x\n",local_status);
        CleanUp();
        return 0;
    }

    m_num_fbpas = fbHelper->NumFBPAs();
    m_dramCap = fbHelper->FBSize();

    m_dmaBuffer.SetArrayPitch(m_size);
    m_dmaBuffer.SetColorFormat(ColorUtils::Y8);
    m_dmaBuffer.SetForceSizeAlloc(true);
    m_dmaBuffer.SetProtect(Memory::ReadWrite);

    if(m_access_mode==0 || m_access_mode==1 || m_access_mode==2 || m_access_mode==3){
        if (!(m_page_size == 64 || m_page_size == 128 || m_page_size == 4)){
          ErrPrintf("Page size can be either 4, 64, or 128., line %d\n", __LINE__);
          local_status = 0;
          return 0;
        }
        m_dmaBuffer.SetPhysContig(true);
        m_dmaBuffer.SetPageSize(m_page_size);
    } else {
        SetPageLayout(m_dmaBuffer, PAGE_VIRTUAL_4KB);
    }

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
        return 0;
    }
    m_offset = m_dmaBuffer.GetExtraAllocSize();

    InfoPrintf("max_memoryTest::Dma buffer at 0x%012x\n", m_dmaBuffer.GetVidMemOffset());

    m_blk_phys_rw_test_buff_size = BLK_RW_TEST_BUFFER_SIZE * m_num_fbpas;

    m_refBuff = new unsigned char[m_blk_phys_rw_test_buff_size];
    m_backup = new unsigned char[m_blk_phys_rw_test_buff_size];
    m_readBuff = new unsigned char[m_blk_phys_rw_test_buff_size];

    InfoPrintf("max_memoryTest::Setup() Initializing reference buffer\n");
    for (UINT32 i = 0; i < m_blk_phys_rw_test_buff_size / 4; i++) {
        UINT32 *refBuff = (UINT32 *)m_refBuff;
        UINT32 *readBuff = (UINT32 *)m_readBuff;

        refBuff[i] = i * 4;
        readBuff[i] = 0;
    }

    InfoPrintf("max_memoryTest::Setup() done\n");

    return 1;
}

// a little extra cleanup to be done
void max_memoryTest::CleanUp(void)
{
    DebugPrintf("max_memoryTest::CleanUp() starts\n");

    if (m_dmaBuffer.GetSize()) {
        m_dmaBuffer.Unmap();
        m_dmaBuffer.Free();
    }

    delete[] m_refBuff;
    delete[] m_backup;
    delete[] m_readBuff;

    FBTest::CleanUp();

    DebugPrintf("max_memoryTest::CleanUp() done\n");
}

void max_memoryTest::Run(void)
{
    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("\n\nmax_memoryTest::Run() starts\n\n");

    /*
      mods/mdiag/lwgpu.h contains the LWGpuResource class, which is the class that lwgpu points to.
      That class has a method called GetGPuDevice which returns the GpuDevice for channel/object/FB alloc or control calls.
      GpuDevice class is defined in mods/gpu/include/gpudev.h

      the max_test class is derived from FBTest class. The FBTest class is implemented in fb_common.cpp and already contains
      pointers to lwgpu and gpuSubDev

    */

    /*Default to using -test_phys_addr when -test_phys_addr_percent is >100
     *to allow legacy tests using -test_phys_addr to pass.*/
    m_phys_addr = (m_phys_addr_percent > 100) ? m_phys_addr : m_phys_addr_percent * fbHelper->FBSize() / 100; 
    if (!gpuSubDev->IsSOC()) {
        m_dramCap = fbHelper->FBSize();
        InfoPrintf("max_memoryTest: Capacity(%d fbpas): %lldMB(%llx).\n", m_num_fbpas, (m_dramCap >> 20), m_dramCap);
        InfoPrintf("max_memoryTest: Max address: 0x%010llx.\n", m_dramCap - 1);
        InfoPrintf("max_memoryTest: Physical Address of FB Start: 0x%08x.\n", lwgpu->PhysFbBase());
    } else {
        UINT32 value = 0;
        GpuPtr()->SocRegRd32(LW_DEVID_MC, 0, LW_PMC_EMEM_CFG, &value);
        const UINT32 sizeMB = DRF_VAL(_PMC, _EMEM_CFG, _EMEM_SIZE_MB, value);
        const UINT32 BOM = DRF_VAL(_PMC, _EMEM_CFG, _EMEM_BOM, value);
        m_dramCap = sizeMB;

        InfoPrintf("max_memoryTest: SOC MODE: Capacity %dMB(%x).\n", sizeMB, sizeMB);
        InfoPrintf("max_memoryTest: SOC MODE: BOM = %s.\n", (BOM == LW_PMC_EMEM_CFG_EMEM_BOM_B2GB)? "2GB" : "0GB" );

        if(BOM == LW_PMC_EMEM_CFG_EMEM_BOM_B2GB){
            m_bom = 0x80000000;
            m_phys_addr += m_bom;
        }
    }

    // Turn off backdoor accesses
    fbHelper->SetBackdoor(false);

    MASSERT((m_dramCap>0) && "Trying to access more than memory capacity\n");

    if(m_access_mode == 0){
        MASSERT((m_bom == 0) && "This test mode is not supported for SOC configurations\n");
        InfoPrintf("\nmax_memoryTest: Testing memory for all partition, slices, and subpart\n");
        MemBlkRdWrTestMode0();
        InfoPrintf("\nmax_memoryTest: Done testing memory for all partition, slices, and subpart\n");

    } else if(m_access_mode == 1) {
        MASSERT((m_bom == 0) && "This test mode is not supported for SOC configurations\n");
        InfoPrintf("\nmax_memoryTest: Testing memory for parameterized ptekind,page_size, and aperture\n");
        BasicBufferTestMode1(&m_dmaBuffer, m_stride);
        InfoPrintf("\nmax_memoryTest: Done testing memory for parameterized ptekind,page_size, and aperture\n");

    } else if(m_access_mode == 2){
        InfoPrintf("\nmax_memoryTest: Testing memory for specified physical address 0x%010llx of Size 0x%08x\n", m_phys_addr,m_phys_size );
        MemBlkRdWrTestMode2(m_phys_addr, m_phys_size);
        InfoPrintf("\nmax_memoryTest: Done testing memory for specified physical address 0x%010llx of Size 0x%08x\n", m_phys_addr,m_phys_size  );
    } else if(m_access_mode == 3){
        InfoPrintf("\nmax_memoryTest: Testing memory for specified physical address 0x%010llx of Size 0x%08x with only 1 access within 128B\n", m_phys_addr,m_phys_size );
        MemBlkRdWrTestMode3(m_phys_addr, m_phys_size);
        InfoPrintf("\nmax_memoryTest: Done testing memory for specified physical address 0x%010llx of Size 0x%08x with only 1 access within 128B\n", m_phys_addr,m_phys_size  );
    } else {
        // test loop
        MASSERT((m_bom == 0) && "This test mode is not supported for SOC configurations\n");
        for (UINT32 i = 0; KeepLooping() || i < loopCount; ++i) {
            if (params->ParamPresent("-test_phys_addr")) {
                DebugPrintf("reading data from 0x%010llx\n", m_phys_addr);
                UINT32 readData = fbHelper->PhysRd32_Bar0Window(m_phys_addr, m_target);
                DebugPrintf("read back data 0x%08x@0x%010llx\n", readData, m_phys_addr);
            }
            else {
                SingleRun();
            }

            if (local_status == 0) {
                SetStatus(Test::TEST_FAILED_CRC);
                getStateReport()->runFailed("run failed, see errors printed!");
                return;
            }
        }
    }
    if (local_status == 0) {
        SetStatus(Test::TEST_FAILED_CRC);
        getStateReport()->runFailed("run failed, see errors printed!");
        return;
    } else {
        InfoPrintf("Done with testing\n");
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    }
}

//! The test
//!
//! This tests FB pa maximum memory size configurations
//! The procedure is as follows:
//!
//! (1) Setup/Enable tests surfaces(buffer).
//! (2) Test r/w nearby 0x0. Make sure address 0x0 is tested
//! (3) Randomly test r/w from addr 0x0 ~ max_addr
//! (4) Test r/w nearby max_addr. Make max_addr is tested
//!
//! if memory r/w tests (2) ~ (4) pass then the test passes.
//!
void max_memoryTest::SingleRun()
{
    // r/w to min address
    InfoPrintf("max_memoryTest: Testing lower bound of memory...\n");
    MemRdWrTest32(0x0, false, 0xdeadbeef);

    // r/w to max address
    InfoPrintf("max_memoryTest: Testing upper bound of memory...\n");
    MemRdWrTest32(m_dramCap - 4, false, 0x12345678);

    // random address access test
    for (UINT32 j = 0; j < rndTestCount; ++j) {
        // workaround to get random 64bit address
        UINT64 rndAddr_lo = (UINT64)(RndStream->Random(0, 0xffffffff)) & (~0x3);
        UINT64 rndAddr_hi = (UINT64)(RndStream->Random(0, 0xffffffff)) << 32;

        if ((rndAddr_hi | rndAddr_lo) >= m_dramCap) {
            continue;
        }

        MemRdWrTest32(rndAddr_hi | rndAddr_lo, false, j);
    }

    MemBlkRdWrTest(0, m_blk_phys_rw_test_buff_size, false, m_refBuff, m_readBuff, m_backup);
    MemBlkRdWrTest(m_dramCap - m_blk_phys_rw_test_buff_size,
                   m_blk_phys_rw_test_buff_size, false,
                   m_refBuff, m_readBuff, m_backup);

    BasicBufferTest(&m_dmaBuffer, m_size / 128);
}

void max_memoryTest::BasicBufferTest(DmaBuffer *surf, UINT32 stride)
{
    int status = 1;

    InfoPrintf("max_memoryTest: Basic Buffer test. Buffer address: 0x%010llx offset 0x%010llx stride 0x%08x\n",
               (UINT64)(surf->GetVidMemOffset()), m_offset, stride);

    InfoPrintf("max_memoryTest: Write to BAR0 WINDOW & read back from BAR1\n");
    for (UINT64 addr = 0; addr < m_size; addr += stride) {
        UINT32 testData = (UINT32)addr;
        UINT32 readData = (UINT32)addr - 1;

        fbHelper->PhysWr32_Bar0Window(surf->GetVidMemOffset() + addr + m_offset, testData, m_target);
        readData = MEM_RD32(reinterpret_cast<uintptr_t>(surf->GetAddress()) + addr);
        if (testData != readData) {
            ErrPrintf("max_memoryTest: Error comparing r(bar1)/w(bar0 window) at 0x%010llx, %08x != %08x\n",
                      surf->GetVidMemOffset() + addr + m_offset, readData, testData);
            status = 0;
        }
    }

    InfoPrintf("max_memoryTest: Write to BAR1 & read back from BAR0 WINDOW\n");
    for (UINT64 addr = 0; addr < m_size; addr += stride) {
        UINT32 testData = (UINT32)addr;
        UINT32 readData = (UINT32)addr - 1;

        // Write to BAR0 WINDOW & read back from BAR1
        MEM_WR32(reinterpret_cast<uintptr_t>(surf->GetAddress()) + addr, testData);
        // null read
        MEM_RD08(reinterpret_cast<uintptr_t>(surf->GetAddress()));
        readData = fbHelper->PhysRd32_Bar0Window(surf->GetVidMemOffset() + addr + m_offset, m_target);
        if (testData != readData) {
            ErrPrintf("max_memoryTest: Error comparing r(bar0 window)/w(bar1) at 0x%010llx, %08x != %08x\n",
                      surf->GetVidMemOffset() + addr + m_offset, readData, testData);
            status = 0;
        }
    }

    if (status == 0) {
        ErrPrintf("max_memoryTest: DMA Buffer test FAILED\n");
        local_status = 0;
    }
    else {
        InfoPrintf("max_memoryTest: DMA Buffer test PASSED\n");
    }
}

//
// Do blk r/w test from a certain physical address.
//
// NOTE: Since the burst size of bar0 window can only be 4B.
//       the blk r/w are done by seqences of 4B r/w requests.
//
// Strategy(self-checking):
//   * write reference data into FB
//   * read data back and compare it with reference data
//
void max_memoryTest::MemBlkRdWrTest(UINT64 addr, UINT64 size, bool isRandom,
                                    unsigned char *refBuff,
                                    unsigned char *readBuff,
                                    unsigned char *backupBuff)
{
    InfoPrintf("max_memoryTest::MemBlkRdWrTest() doing blk read/write test(addr=0x%010llx)\n", addr);
    // Save data in this block of memory(the size is fixed)
    fbHelper->PhysRdBlk_Bar0Window(addr, backupBuff, size, m_target);

    // r/w to lowest & highest 4k page
    fbHelper->PhysWrBlk_Bar0Window(addr, refBuff, size, m_target);
    fbHelper->PhysRdBlk_Bar0Window(addr, readBuff, size, m_target);

    // check read back data with reference data
    for (UINT32 i = 0; i < size / 4; i++) {
        if (refBuff[i] != readBuff[i]) {
            local_status = 0;
            ErrPrintf("data 0x%08x at 0x%010llx doesn't match reference(=0x%08x)\n",
                      readBuff[i], addr + i * 4, refBuff[i]);
        }
    }

    // restore data
    fbHelper->PhysWrBlk_Bar0Window(addr, backupBuff, size, m_target);
}

void max_memoryTest::MemBlkRdWrTest2(UINT64 addr, UINT64 size, bool isRandom)
{
    UINT32 *backup = NULL;
    UINT32 *refData = NULL;
    UINT32 *readData = NULL;

    size >>= 2;
    try {
        backup = new UINT32[size];
        refData = new UINT32[size];
        readData = new UINT32[size];
    }
    catch (...) {
        delete[] backup;
        delete[] refData;
        delete[] readData;
    }

    for (UINT64 i = 0; i < size; i++) {
        refData[i] = i;
    }

    InfoPrintf("max_memoryTest::MemBlkRdWrTest() doing blk read/write test(addr=0x%010llx)\n", addr);
    fbHelper->PhysRdBlk_Bar0Window(addr, (unsigned char*)backup, size * 4, m_target);

    fbHelper->PhysWrBlk_Bar0Window(addr, (unsigned char*)refData, size * 4, m_target);

    fbHelper->Flush();

    fbHelper->PhysRdBlk_Bar0Window(addr, (unsigned char*)readData, size * 4, m_target);
    for (UINT32 i = 0; i < size; ++i) {
        if (refData[i] != readData[i]) {
            local_status = 0;
            ErrPrintf("data 0x%08x at 0x%010llx doesn't match reference(=0x%08x)\n", readData[i], addr + i * 4, refData[i]);
        }
    }

    fbHelper->PhysWrBlk_Bar0Window(addr, (unsigned char*)backup, size * 4, m_target);

    delete[] backup;
    delete[] refData;
    delete[] readData;
}

//
// Do a paired r/w test at a certain physical address(thru bar0 window)
//
// write data into <addr> then read back and make comparison
//
void max_memoryTest::MemRdWrTest32(UINT64 addr, bool isRandom, UINT32 data)
{
    UINT32 testData, readData, oldData;

    if (isRandom)
        testData = RndStream->Random();
    else
        testData = data;

    oldData = fbHelper->PhysRd32_Bar0Window(addr, m_target);

    fbHelper->PhysWr32_Bar0Window(addr, testData, m_target);
    readData = fbHelper->PhysRd32_Bar0Window(addr, m_target);
    if (testData != readData) {
        ErrPrintf("max_memoryTest::MemRdWrTest32() Error comparing r/w at 0x%x, %x != %x\n", addr, testData, readData);
        local_status = 0;
        error_count++;
    }

    fbHelper->PhysWr32_Bar0Window(addr, oldData, m_target);
}

// This function accesses allocates a buffer at specified physical address. The buffer is configured for various
// page_size, ptekind, and at various apaertures. All these are passed through commandline.
// This is required for all address mapping codde coverage.
// The access is through BAR1
void max_memoryTest::BasicBufferTestMode1(DmaBuffer *surf, UINT32 stride)
{
    int status = 1;

    UINT32 testData, readData, oldData;

    InfoPrintf("max_memoryTest: ptekind/page_size/aperture test through buffer using BAR1 window. Buffer address: 0x%010llx offset 0x%010llx stride 0x%08x\n",
               (UINT64)(surf->GetVidMemOffset()), m_offset, stride);

    //dummy filler value to check whether in fmodel whether this written value is seen.
    testData = 0xDEF000;

    for (UINT64 addr = 0; addr < m_size; addr += stride) {

        readData = (UINT32)addr - 1;

        if(m_enable_print>0)
          InfoPrintf("max_memoryTest: reading(old)-writing(new)-reading(new)-writing(old) 4B: address: 0x%010llx \n", (UINT64)(reinterpret_cast<uintptr_t>(surf->GetAddress()) + addr));

        oldData = MEM_RD32(reinterpret_cast<uintptr_t>(surf->GetAddress()) + addr);
        MEM_WR32(reinterpret_cast<uintptr_t>(surf->GetAddress()) + addr, testData);
        readData = MEM_RD32(reinterpret_cast<uintptr_t>(surf->GetAddress()) + addr);

        if (testData != readData) {
            ErrPrintf("max_memoryTest: Error comparing rw data at 0x%010llx, %08x != %08x\n",
                      surf->GetVidMemOffset() + addr + m_offset, readData, testData);
            status = 0;
            error_count++;
        }
        MEM_WR32(reinterpret_cast<uintptr_t>(surf->GetAddress()) + addr, oldData);
        testData = testData + 1;
    }

    if (status == 0) {
        ErrPrintf("max_memoryTest: DMA Buffer test FAILED\n");
        local_status = 0;
    }
    else {
        InfoPrintf("max_memoryTest: Mode 1 DMA Buffer test PASSED\n");
    }
}

// This function accesses any size  (passed through commandline) amount of memory starting
// at addr (physical address passed through command line). Goal is to access any physical address in the memory.
// Another goal is to access a memory region which is mapped at 8G+ address although the system does not have 8G memory.
// The access is through BAR0
void max_memoryTest::MemBlkRdWrTestMode2(UINT64 addr, UINT32 size)
{
    UINT32 *backup = NULL;
    UINT32 *refData = NULL;
    UINT32 *readData = NULL;

    MASSERT( (size <= 0x10000) && "Size too big for BAR0 window\n");

    size >>= 2;

    backup = new UINT32[size];
    refData = new UINT32[size];
    readData = new UINT32[size];

    if (backup==NULL || refData==NULL || readData==NULL) {
        MASSERT(0 && "Could not allocate memory\n");
    }

    for (UINT32 i = 0; i < size; i++) {
        refData[i] = i;
    }

    InfoPrintf("max_memoryTest::MemBlkRdWrTest() doing blk read/write test(addr=0x%010llx)\n", addr);
    fbHelper->PhysRdBlk_Bar0Window(addr, (unsigned char*)backup, size * 4, m_target);

    fbHelper->PhysWrBlk_Bar0Window(addr, (unsigned char*)refData, size * 4, m_target);

    fbHelper->Flush();

    fbHelper->PhysRdBlk_Bar0Window(addr, (unsigned char*)readData, size * 4, m_target);
    for (UINT32 i = 0; i < size; ++i) {
        if (refData[i] != readData[i]) {
            local_status = 0;
            error_count++;
            ErrPrintf("data 0x%08x at 0x%010llx doesn't match reference(=0x%08x)\n", readData[i], addr + i * 4, refData[i]);
        }
    }

    fbHelper->PhysWrBlk_Bar0Window(addr, (unsigned char*)backup, size * 4, m_target);
    fbHelper->Flush();

    delete[] backup;
    delete[] refData;
    delete[] readData;
}

// This function accesses any size (passed through commandline) amount of memory ending
// at addr (physical address passed through command line). Goal is to access any physical address in the memory.
// Goal is to access 64KB chunk to ensure that every memory partition is accesses. Only 4B is accessed every L2 line (128B) so minimize runtime of the test on RTL
// The access is through BAR0
void max_memoryTest::MemBlkRdWrTestMode3(UINT64 addr, UINT32 size)
{
    UINT32 *backup = NULL;
    UINT32 *refData = NULL;
    UINT32 *readData = NULL;

    MASSERT( (addr>=size) && "End address is less than size\n");
    addr -= size;            //Addr is the end address, colwert to starting address
    UINT64 accessAddr = addr;

    MASSERT( (size <= 0x10000) && "Size too big for BAR0 window\n");

    size >>= 7; //128B L2 lines

    backup = new UINT32[size];
    refData = new UINT32[size];
    readData = new UINT32[size];

    if (backup==NULL || refData==NULL || readData==NULL) {
        MASSERT(0 && "Could not allocate memory\n");
    }

    InfoPrintf("max_memoryTest::MemBlkRdWrTest3() doing blk read/write test(addr=0x%010llx)\n", addr);
    accessAddr = addr;
    for (UINT32 i = 0; i < size; accessAddr+=128, i++) {
        refData[i] = i;
        backup[i] = fbHelper->PhysRd32_Bar0Window(accessAddr, m_target);
        fbHelper->PhysWr32_Bar0Window(accessAddr, refData[i], m_target);
    }

    fbHelper->Flush();

    accessAddr = addr;
    for (UINT32 i = 0; i < size; accessAddr+=128, i++) {
        readData[i] = fbHelper->PhysRd32_Bar0Window(accessAddr, m_target);
        fbHelper->PhysWr32_Bar0Window(accessAddr, backup[i], m_target);
        if (refData[i] != readData[i]) {
            local_status = 0;
            error_count++;
            ErrPrintf("data 0x%08x at 0x%010llx doesn't match reference(=0x%08x)\n", readData[i], accessAddr, refData[i]);
        }
    }

    fbHelper->Flush();

    delete[] backup;
    delete[] refData;
    delete[] readData;
}

// This function accesses  m_num_fbpas * 2K region of memory from lower memory and upper memory.
// This access covers all partitions, slices, and subpartitions in the system.
// This is required for all address mapping codde coverage.
// The access is through BAR0
void max_memoryTest::MemBlkRdWrTestMode0()
{
    UINT32 testData, readData, oldData;
    UINT64 addr;

    uint32 units = m_num_fbpas * 512; // 32bytes unit RW, 2K per partition to cover all slices i=across all FBs.

    if(m_dramCap <( units<<2)){
        MASSERT(0 && "Trying to access more than memory capacity\n");
    }

    for (UINT32 i = 0; i < units; i++) {
        testData = i;
        addr = i<<2;

        if(m_enable_print>0)
          InfoPrintf("max_memoryTest: reading(old)-writing(new)-reading(new)-writing(old) 4B: address: 0x%010llx \n", addr);

        oldData = fbHelper->PhysRd32_Bar0Window(addr, m_target);
        fbHelper->PhysWr32_Bar0Window(addr, testData, m_target);
        readData = fbHelper->PhysRd32_Bar0Window(addr, m_target);

        if (testData != readData) {
            ErrPrintf("max_memoryTest::MemBlkRdWrTestMode0 Error comparing r/w at 0x%010llx, %x != %x\n", addr, testData, readData);
            local_status = 0;
            error_count++;
        }
        fbHelper->PhysWr32_Bar0Window(addr, oldData, m_target);
    }

    for (UINT32 i = 0; i < units; i++) {
        testData = i;
        addr = m_dramCap - ((i+1)<<2);

        if(m_enable_print>0)
          InfoPrintf("max_memoryTest: reading(old)-writing(new)-reading(new)-writing(old) 4B: address: 0x%010llx \n", addr);

        oldData = fbHelper->PhysRd32_Bar0Window(addr, m_target);
        fbHelper->PhysWr32_Bar0Window(addr, testData, m_target);
        readData = fbHelper->PhysRd32_Bar0Window(addr, m_target);

        if (testData != readData) {
            ErrPrintf("max_memoryTest::MemBlkRdWrTestMode0 Error comparing r/w at 0x%010llx, %x != %x\n", addr, testData, readData);
            local_status = 0;
            error_count++;
        }
        fbHelper->PhysWr32_Bar0Window(addr, oldData, m_target);
    }
}

