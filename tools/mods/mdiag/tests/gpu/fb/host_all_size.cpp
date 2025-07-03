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

#include "host_all_size.h"
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

extern const ParamDecl host_all_size_params[] = {
    PARAM_SUBDECL(FBTest::fbtest_common_params),
    MEMORY_SPACE_PARAMS("_buf0",      "dma buffer 0 used for testing"),
    UNSIGNED_PARAM("-size",           "size in bytes of the dma buffer to read/write"),
    UNSIGNED64_PARAM("-offset",       "Offset relative to the base of the memory allocation for test host_all_size"),
    UNSIGNED64_PARAM("-phys_addr",    "address to test"),
    LAST_PARAM
};

host_all_size::host_all_size(ArgReader *reader) :
    FBTest(reader),
    local_status(1)
{
    params = reader;

    ParseDmaBufferArgs(m_testBuffer, params, "buf0", &m_pteKindName, 0);

    m_size = params->ParamUnsigned("-size", 4096);
    m_size = m_size & ~0x3;

    m_testData = new UINT32[m_size/4];
    m_testRdData = new UINT32[m_size/4];

    m_phys_addr = params->ParamUnsigned64("-phys_addr", 0x0);

}

host_all_size::~host_all_size(void)
{
    delete m_testData;
}

STD_TEST_FACTORY(host_all_size, host_all_size)

// a little extra setup to be done
int host_all_size::Setup(void)
{
    local_status = 1;

    InfoPrintf("host_all_size::Setup() starts\n");

    getStateReport()->enable();

    if (!FBTest::Setup()) {
        ErrPrintf("host_all_size::Setup() Fail\n");
        return 0;
    }

    m_testBuffer.SetArrayPitch(m_size);
    m_testBuffer.SetColorFormat(ColorUtils::Y8);
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

    InfoPrintf("Created Surface at 0x%x Mapped at 0x%x\n",m_testBuffer.GetVidMemOffset(), m_testBuffer.GetAddress());

    // if unsuccessful, clean up and exit
    if(!local_status) {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with local_status 0x%x\n",local_status);
        CleanUp();
        return(0);
    }
    InfoPrintf("host_all_size::Setup() done\n");

    return (1);
}

// a little extra cleanup to be done
void host_all_size::CleanUp(void)
{
    DebugPrintf("host_all_size::CleanUp() starts\n");

    if (m_testBuffer.GetSize() != 0) {
        m_testBuffer.Unmap();
        m_testBuffer.Free();
    }

    FBTest::CleanUp();

    DebugPrintf("host_all_size::CleanUp() done\n");
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
void host_all_size::Run(void)
{

    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("host_all_size::Run() starts\n");

    local_status = 1;

    InfoPrintf("host_all_size: Physical Address of FB Start: 0x%08x.\n", lwgpu->PhysFbBase());
    InfoPrintf("host_all_size: BAR1 Buffer Start: 0x%08x.\n", m_testBuffer.GetAddress());

    InfoPrintf("host_all_size: Generating test data...\n");

    // UINT32 backup = fbHelper->PhysRd32_Bar0Window(UINT64 m_phys_addr);
    for (unsigned int loop = 0; loop < loopCount; ++loop) {

        for (unsigned int i = 0; i < (m_size - 1) / 4 + 1; ++i) {
            UINT32 *testData = (UINT32 *)m_testData;
            testData[i] = i+loop;//0xdeadbeef;
        }

        TestMemoryRW(loop);

        if (local_status == 0)
            break;
    }
    // fbHelper->PhysWr32_Bar0Window(UINT64 m_phys_addr, UINT32 backup);

    InfoPrintf("Done with host_all_size test\n");

    if(local_status) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } else {
        SetStatus(Test::TEST_FAILED_CRC);
    }

    return;
}

int host_all_size::FBFlush()
{
    // Send down an FB_FLUSH, MODS will use the correct register based on
    // PCIe GEN of HW.
    lwgpu->GetGpuSubdevice()->FbFlush(Tasker::NO_TIMEOUT);

    return 0;
}

void host_all_size::EnableBackdoor()
{
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 1);
        // Turn off backdoor accesses
        // bug 527528 - Comments by Joseph Harwood:
        //   DUT in the fermi fullchip testbench is the GPU. Only backdoor sysmem accesses will work in this testbench,
        //   because only the GPU is on the PCI, and the GPU (the real chip) does not process memory accesses
        //   for other chips, only for itself.
        //
        // Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_SYS, 4, 1);
    }
}

void host_all_size::DisableBackdoor()
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

void host_all_size::TestMemoryRW(int lwrLoop)
{
    int local_error_status = 0;
    // Write randomly generated Data by BAR1 through frontdoor
    DisableBackdoor();

    INT32 blkLength = 4;
    INT32 limit;
    INT32 i=0,j=0;
    INT32 count=0;

    INT32 blkLimit = 7; //256 128 64 32 16 8 4

    for(int blk=0; blk < blkLimit; blk++)
    {
        limit = (m_size - 1) / blkLength + 1;
        i = count = 0;

        InfoPrintf("host_all_size: Writing-Flushing-Reading data from/through BAR1(frontdoor)...%dB chunk ...\n",blkLength);
        for (i = 0; i < limit; ++i) 
        {
            for (j = 0; j < (blkLength / 4); j++)
            {
                m_testData[i*(blkLength/4)+j] = i*(blkLength/4)+j+lwrLoop;
            }
            uintptr_t addr = reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress())+i*blkLength;
            Platform::VirtualWr((void*)addr, m_testData+i*(blkLength/4), blkLength);
            FBFlush();
            Platform::VirtualRd((void*)addr, m_testRdData+i*(blkLength/4),blkLength);
            for (j = 0; j < (blkLength / 4); j++)
            {
                if (m_testData[i*(blkLength/4)+j] != m_testRdData[i*(blkLength/4)+j]) 
                {
                    ErrPrintf("Loop %d i=%d: host_all_size test failed. Expected 0x%x got 0x%x\n",
                        lwrLoop, i, m_testData[i], m_testRdData[i*(blkLength/4)+j]);
                    local_error_status++;
                    local_status=0;
                }
            }

        }

        blkLength <<= 1;

    }

    if(local_error_status==0)
        InfoPrintf("Loop %d : host_all_size test passed\n",lwrLoop);
    else
        InfoPrintf("Loop %d : host_all_size test failed\n",lwrLoop);

    return;
}
