/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009,2019-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "basic_read_write.h"
#include "sim/IChip.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"

#include "fermi/gf100/dev_flush.h"

#include "mdiag/utils/surfutil.h"

/*
  This test do basic reads and writes for FB PA.
*/

extern const ParamDecl basic_read_write_params[] = {
    PARAM_SUBDECL(FBTest::fbtest_common_params),
    MEMORY_SPACE_PARAMS("_buf0",      "dma buffer 0 used for testing"),
    UNSIGNED_PARAM("-size",           "size in bytes of the dma buffer to read/write"),
    UNSIGNED64_PARAM("-offset",       "Offset relative to the base of the memory allocation for test surface"),
    UNSIGNED64_PARAM("-phys_addr",    "address to test"),
    SIMPLE_PARAM("-frontdoor_check",  "check data through frontdoor"),
    LAST_PARAM
};

basic_read_writeTest::basic_read_writeTest(ArgReader *reader) :
    FBTest(reader),
    m_frontdoor_check(false)
{
    params = reader;

    ParseDmaBufferArgs(m_testBuffer, params, "buf0", &m_pteKindName, 0);

    m_frontdoor_check = params->ParamPresent("-frontdoor_check") != 0;

    m_size = params->ParamUnsigned("-size", 4096);
    m_testData = new unsigned char[m_size];

    m_phys_addr = params->ParamUnsigned64("-phys_addr", 0x0);

    RndStream = new RandomStream(seed0, seed1, seed2);
}

basic_read_writeTest::~basic_read_writeTest(void)
{
    delete RndStream;
    delete m_testData;
}

STD_TEST_FACTORY(basic_read_write, basic_read_writeTest)

// a little extra setup to be done
int basic_read_writeTest::Setup(void)
{
    local_status = 1;

    InfoPrintf("basic_read_writeTest::Setup() starts\n");

    getStateReport()->enable();

    if (!FBTest::Setup()) {
        ErrPrintf("enhanced_read_writeTest::Setup() Fail\n");
        return 0;
    }

    m_testBuffer.SetArrayPitch(m_size);
    m_testBuffer.SetColorFormat(ColorUtils::Y8);
    m_testBuffer.SetForceSizeAlloc(true);
    m_testBuffer.SetProtect(Memory::ReadWrite);
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

    InfoPrintf("Created Surface at VidMemOffset 0x%llx"
               " Mapped at %p\n",
               m_testBuffer.GetVidMemOffset(), m_testBuffer.GetAddress());

    // if unsuccessful, clean up and exit
    if(!local_status) {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with local_status 0x%x\n",local_status);
        CleanUp();
        return(0);
    }
    InfoPrintf("basic_read_writeTest::Setup() done\n");

    return (1);
}

// a little extra cleanup to be done
void basic_read_writeTest::CleanUp(void)
{
    DebugPrintf("basic_read_writeTest::CleanUp() starts\n");

    if (m_testBuffer.GetSize() != 0) {
        m_testBuffer.Unmap();
        m_testBuffer.Free();
    }

    FBTest::CleanUp();

    DebugPrintf("basic_read_writeTest::CleanUp() done\n");
}

//! The test
//!
//! This tests FB pa Basic reads and writes
//! The procedure is as follows:
//!
//! (0) Turn off backdoor accesses.
//! (1) Setup/Enable tests surfaces(buffer).
//! (2) Initializes src surface with randomly generated data(save the data for later verification).
//! (3) Read data back(in both frontdoor & backdoor).
//! (4) Do 1~32 bytes write(in frontdoor).
//! (5) Do 1~32 bytes read(in frontdoor).
//!
//! if (2) and (3) match & (4) and (5) match then the test passes.
//!
void basic_read_writeTest::Run(void)
{

    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("basic_read_writeTest::Run() starts\n");

    int local_status = 1;

    InfoPrintf("basic_read_writeTest: Physical Address of FB Start: 0x%08x.\n", lwgpu->PhysFbBase());
    InfoPrintf("basic_read_writeTest: BAR1 Buffer Start: 0x%08x.\n", m_testBuffer.GetAddress());

    InfoPrintf("basic_read_writeTest: Generating test data...\n");
    for (unsigned int i = 0; i < (m_size - 1) / 4 + 1; ++i) {
//         UINT32 rndNumb = RndStream->Random();

//         surfaceTestData[i] = (unsigned char)(rndNumb & 0xff); rndNumb >>= 4;
//         surfaceTestData[i + 1] = (unsigned char)(rndNumb & 0xff); rndNumb >>= 4;
//         surfaceTestData[i + 2] = (unsigned char)(rndNumb & 0xff); rndNumb >>= 4;
//         surfaceTestData[i + 3] = (unsigned char)(rndNumb & 0xff);
        UINT32 *testData = (UINT32 *)m_testData;
        testData[i] = 0xdeadbeef;
    }

    // UINT32 backup = fbHelper->PhysRd32_Bar0Window(UINT64 m_phys_addr);
    for (unsigned int loop = 0; loop < loopCount; ++loop) {
        TestMemoryRW();

        if (local_status == 0)
            break;
    }
    // fbHelper->PhysWr32_Bar0Window(UINT64 m_phys_addr, UINT32 backup);

    InfoPrintf("Done with basic_read_write test\n");

    if(local_status) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } else {
        SetStatus(Test::TEST_FAILED_CRC);
    }

    return;
}

int basic_read_writeTest::FBFlush()
{
    // Send down an FB_FLUSH, MODS will use the correct register based on
    // PCIe GEN of HW.
    lwgpu->GetGpuSubdevice()->FbFlush(Tasker::NO_TIMEOUT);

    return 0;
}

void basic_read_writeTest::EnableBackdoor()
{
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 1);
        // Turn off backdoor accesses
        // bug 527528 -
        // Comments by Joseph Harwood:
        //   DUT in the fermi fullchip testbench is the GPU. Only backdoor sysmem accesses will work in this testbench,
        //   because only the GPU is on the PCI, and the GPU (the real chip) does not process memory accesses
        //   for other chips, only for itself.
        //
        // Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_SYS, 4, 1);
    }
}

void basic_read_writeTest::DisableBackdoor()
{
    if(Platform::GetSimulationMode() != Platform::Hardware) {
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
        // Turn off backdoor accesses
        // bug 527528 - Comments by Joseph Harwood:
        //   DUT in the fermi fullchip testbench is the GPU. Only backdoor sysmem accesses will work in this testbench,
        //   because only the GPU is on the PCI, and the GPU (the real chip) does not process memory accesses
        //   for other chips, only for itself.
        //
        // Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_SYS, 4, 0);
    }
}

void basic_read_writeTest::ReadSurface()
{
    InfoPrintf("basic_read_writeTest: Checking...\n");
    for (unsigned int i = 0; i < m_size; ++i) {
        unsigned char rt = MEM_RD08(reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress()) + i);

        if (m_testData[i] != rt) {
            ErrPrintf("surface test failed.\n");
            local_status = 0;
        }
    }
    InfoPrintf("surface test passed\n");
}

void basic_read_writeTest::WriteSurface()
{
    InfoPrintf("basic_read_writeTest: Writing test data to BAR1...\n");
    Platform::VirtualWr(m_testBuffer.GetAddress(), m_testData, m_size);
}

void basic_read_writeTest::TestMemoryRW()
{
    // Write randomly generated Data by BAR1 through frontdoor
    DisableBackdoor();
    WriteSurface();

    FBFlush();

    // Read back test through backdoor
    InfoPrintf("basic_read_writeTest: Reading back data from BAR1(backdoor)...\n");
    EnableBackdoor();
    ReadSurface();

    // Read back test through frontdoor
    if (m_frontdoor_check) {
        InfoPrintf("basic_read_writeTest: Reading back data from BAR1(frontdoor)...\n");
        DisableBackdoor();
        ReadSurface();
    }

    // Frontdoor r/w test with different size
    // NOTE: This test doesn't work exactly in expacted way, until bug 368780 is fixed.
    {
        UINT32 testData32B[] = {
            0x12345678, 0xdeadbeaf, 0x87654321, 0xbeefdead,
            0x11111111, 0x33333333, 0x66666666, 0xaaaaaaaa,
        };
        UINT32 readData32B[] = {
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
        };

        InfoPrintf("basic_read_writeTest: Testing different r/w size...\n");
        for (int i = 1; i <= 32; ++i) {
            UINT32 addr = RndStream->Random(0, m_size - 32);

            InfoPrintf("basic_read_writeTest: Writing test data to BAR1 in %dB size...\n", i);
            uintptr_t surfAddr = reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress()) + addr;
            Platform::VirtualWr((void*)surfAddr, testData32B, i);

            InfoPrintf("basic_read_writeTest: Reading back data from BAR1...\n");
            Platform::VirtualRd((void*)surfAddr, readData32B, i);
            for (int j = 0; j < i; ++j) {
                unsigned char write = ((unsigned char*)testData32B)[j];
                unsigned char read  = ((unsigned char*)readData32B)[j];

                if (write != read) {
                    ErrPrintf("FAILED! at addr 0x%x[offset %x]: 0x%x != 0x%x\n", addr, j, write, read);
                    local_status = 0;
                }

                // clear the value for next round check.
                ((unsigned char*)readData32B)[j] = 0x0;
            }
        }
        InfoPrintf("passed\n");
    }

    return;
}
