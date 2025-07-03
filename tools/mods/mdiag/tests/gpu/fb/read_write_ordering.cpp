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

#include "read_write_ordering.h"
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

extern const ParamDecl read_write_ordering_params[] = {
    PARAM_SUBDECL(FBTest::fbtest_common_params),
    MEMORY_SPACE_PARAMS("_buf0",      "dma buffer 0 used for testing"),
    UNSIGNED_PARAM("-size",           "size in bytes of the dma buffer to read/write"),
    UNSIGNED64_PARAM("-offset",       "Offset relative to the base of the memory allocation for test read_write_ordering"),
    UNSIGNED64_PARAM("-phys_addr",    "address to test"),
    UNSIGNED_PARAM("-buffer_location",      "specify the location of memory for r/w. Can be 0(vid), 1(peer), 2(coh), 3(ncoh))"),
    LAST_PARAM
};

read_write_ordering::read_write_ordering(ArgReader *reader) :
    FBTest(reader),
    m_frontdoor_check(true)
{
    params = reader;

    ParseDmaBufferArgs(m_testBuffer, params, "buf0", &m_pteKindName, 0);

    m_size = params->ParamUnsigned("-size", 4096);
    m_testData = new UINT32[m_size/4];

    m_phys_addr = params->ParamUnsigned64("-phys_addr", 0x0);

    if (params->ParamPresent("-buffer_location")) {
        m_target = params->ParamUnsigned("-buffer_location", 0x0);
        switch (m_target) {
          case 0x0:
            m_testBuffer.SetLocation(Memory::Fb);
            break;
          case 0x2:
            m_testBuffer.SetLocation(Memory::Coherent);
            break;
          case 0x3:
            m_testBuffer.SetLocation(Memory::NonCoherent);
            break;
          default:
            ErrPrintf("Unsupported mem location: %d, expected 0x0, 0x2, 0x3.\n", m_testBuffer.GetLocation());
            MASSERT(0 && "Unsupported aperture");
            break;
        }
    }
    else {
        switch (m_testBuffer.GetLocation()) {
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
            ErrPrintf("Unsupported mem location: %d, expected 0x0, 0x2, 0x3.\n", m_testBuffer.GetLocation());
            MASSERT(0 && "Unsupported aperture");
            break;
        }
    }

}

read_write_ordering::~read_write_ordering(void)
{
    delete m_testData;
}

STD_TEST_FACTORY(read_write_ordering, read_write_ordering)

// a little extra setup to be done
int read_write_ordering::Setup(void)
{
    local_status = 1;

    InfoPrintf("read_write_ordering::Setup() starts\n");

    getStateReport()->enable();

    if (!FBTest::Setup()) {
        ErrPrintf("read_write_ordering::Setup() Fail\n");
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
    PrintDmaBufferParams(m_testBuffer);

    InfoPrintf("Created Surface at %d and 0x%x Mapped at 0x%x\n",m_testBuffer.GetLocation(), m_testBuffer.GetAddress(), m_testBuffer.GetVidMemOffset());

    // if unsuccessful, clean up and exit
    if(!local_status) {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with local_status 0x%x\n",local_status);
        CleanUp();
        return(0);
    }
    InfoPrintf("read_write_ordering::Setup() done\n");

    return (1);
}

// a little extra cleanup to be done
void read_write_ordering::CleanUp(void)
{
    DebugPrintf("read_write_ordering::CleanUp() starts\n");

    if (m_testBuffer.GetSize() != 0) {
        m_testBuffer.Unmap();
        m_testBuffer.Free();
    }

    FBTest::CleanUp();

    DebugPrintf("read_write_ordering::CleanUp() done\n");
}

void read_write_ordering::Run(void)
{

    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("read_write_ordering::Run() starts\n");

    local_status = 1;

    InfoPrintf("read_write_ordering: Physical Address of FB Start: 0x%08x.\n", lwgpu->PhysFbBase());
    InfoPrintf("read_write_ordering: BAR1 Buffer Start: 0x%08x.\n", m_testBuffer.GetAddress());

    InfoPrintf("read_write_ordering: Generating test data...\n");

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
    //fbHelper->SetBackdoor(false);

    InfoPrintf("Done with read_write_ordering test\n");

    if(local_status) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } else {
        SetStatus(Test::TEST_FAILED_CRC);
    }

    return;
}

int read_write_ordering::FBFlush()
{

    // Send down an FB_FLUSH, MODS will use the correct register based on
    // PCIe GEN of HW.
    lwgpu->GetGpuSubdevice()->FbFlush(Tasker::NO_TIMEOUT);

    return 0;
}

void read_write_ordering::EnableBackdoor()
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

void read_write_ordering::DisableBackdoor()
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

void read_write_ordering::TestMemoryRW(int lwrLoop)
{
    int local_rw_status = 0;
    // Write randomly generated Data by BAR1 through frontdoor
    DisableBackdoor();

    INT32 limit = (m_size - 1) / 4 + 1;
    INT32 i=0;
    INT32 count=0;

    InfoPrintf("read_write_ordering: Writing data from BAR1(frontdoor)...\n");

    UINT32 mask = 15;

    UINT32 regData = lwgpu->RegRd32(0x00100C04);//LW_PFB_NISO_ARB0
    UINT32 val = regData  & (mask<<24);//DRF_VAL(_PFB,_NISO_ARB0,_PRIO_HOST_NB,regData);
    InfoPrintf("read_write_ordering: read LW_PFB_NISO_ARB0_PRIO_HOST_NB ... read value 0x%x whole 32bit 0x%x\n",val,regData);

    lwgpu->RegWr32(0x00100C04, ( ~(mask<<24)  & regData) | ((UINT32)6<<24) ) ;
    regData = lwgpu->RegRd32(0x00100C04);//LW_PFB_NISO_ARB0);
    val = regData  & (mask<<24);//    val = DRF_VAL(_PFB,_NISO_ARB0,_PRIO_HOST_NB,regData)
    InfoPrintf("read_write_ordering: read LW_PFB_NISO_ARB0_PRIO_HOST_NB after writing 6 ... read value 0x%x whole 32bit 0x%x\n",val,regData);

    lwgpu->RegWr32(0x00100C04, ( ~(mask<<24)  & ~0) ) ;//RegWr32(LW_PFB_NISO_ARB0, 0);
    regData = lwgpu->RegRd32(0x00100C04);//LW_PFB_NISO_ARB0);
    val = regData  & (mask<<24);//    val = DRF_VAL(_PFB,_NISO_ARB0,_PRIO_HOST_NB,regData)
    InfoPrintf("read_write_ordering: read LW_PFB_NISO_ARB0_PRIO_HOST_NB after writing 0 ... read value 0x%x whole 32bit 0x%x\n",val,regData);

    mask=15;
    lwgpu->RegWr32(0x00100C04, ( ~(mask<<24)  & ~0) ) ;//RegWr32(LW_PFB_NISO_ARB0, 0);
    lwgpu->RegWr32(0x00100C08, 0xFFFFFFFF) ;//RegWr32(LW_PFB_NISO_ARB1, 0xFFFFFFFF);

    for (i = 0; i < limit; ++i) 
    {
        m_testData[i] = i+lwrLoop;
        uintptr_t addr = reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress())+i*4;
        Platform::VirtualWr((void*)addr, m_testData+i, 4);
    }

    InfoPrintf("read_write_ordering: Reading back data from BAR1(frontdoor)...starting from first\n");

    i=0;
    count=0;

    mask=14;
    lwgpu->RegWr32( 0x00100C04, ( ~(mask<<24)  & ~0) );

    while(count<limit) {
        UINT32 rt = MEM_RD32(reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress()) + i*4);
        //InfoPrintf("Loop %d i=%d: Expected 0x%x got 0x%x\n",lwrLoop,i,m_testData[i],rt);
        if (m_testData[i] != rt) {
            ErrPrintf("Loop %d i=%d: read_write_ordering test failed. Expected 0x%x got 0x%x\n",lwrLoop,i,m_testData[i],rt);
            local_rw_status++;
            local_status = 0;
        }
        count++;
        i++;
        if(i==limit)
            i=0;
    }

    mask=15;
    lwgpu->RegWr32(0x00100C04, ( ~(mask<<24)  & ~0) ) ;//RegWr32(LW_PFB_NISO_ARB0, 0);
    lwgpu->RegWr32(0x00100C08, 0xFFFFFFFF) ;//RegWr32(LW_PFB_NISO_ARB1, 0xFFFFFFFF);

    for (i = 0; i < limit; i++) 
    {
        m_testData[i] = i+lwrLoop;
        uintptr_t addr = reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress())+i*4;
        Platform::VirtualWr((void*)addr, m_testData+i, 4);
    }

    InfoPrintf("read_write_ordering: Reading back data from BAR1(frontdoor)...starting from last\n");
    //from back
    i=limit-1;
    count =0;

    mask=14;
    lwgpu->RegWr32(0x00100C04, ( ~(mask<<24)  & ~0) ) ;//RegWr32(LW_PFB_NISO_ARB0, 0);

    while(count<limit) {
        UINT32 rt = MEM_RD32(reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress()) + i*4);
        //InfoPrintf("Loop %d i=%d: Expected 0x%x got 0x%x\n",lwrLoop,i,m_testData[i],rt);
        if (m_testData[i] != rt) {
            ErrPrintf("Loop %d i=%d: read_write_ordering test failed. Expected 0x%x got 0x%x\n",lwrLoop,i,m_testData[i],rt);
            local_rw_status++;
            local_status = 0;

        }
        count++;
        i--;
        if(i<0)
            i=limit-1;
    }

    mask=15;
    lwgpu->RegWr32(0x00100C04, ( ~(mask<<24)  & ~0) ) ;//RegWr32(LW_PFB_NISO_ARB0, 0);
    lwgpu->RegWr32(0x00100C08, 0xFFFFFFFF) ;//RegWr32(LW_PFB_NISO_ARB1, 0xFFFFFFFF);

    for (i = 0; i < limit; i++) 
    {
        m_testData[i] = i+lwrLoop;
        uintptr_t addr = reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress())+i*4;
        Platform::VirtualWr((void*)addr, m_testData+i, 4);
    }

    // Read back test through backdoor
    InfoPrintf("read_write_ordering: Reading back data from BAR1(frontdoor)...starting from middle\n");
    //from back

    i=limit/2;
    count=0;

    mask=14;
    lwgpu->RegWr32(0x00100C04, ( ~(mask<<24)  & ~0) ) ;//RegWr32(LW_PFB_NISO_ARB0, 0);

    while(count<limit) {
        UINT32 rt = MEM_RD32(reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress()) + i*4);
        //InfoPrintf("Loop %d i=%d: Expected 0x%x got 0x%x\n",lwrLoop,i,m_testData[i],rt);
        if (m_testData[i] != rt) {
            ErrPrintf("Loop %d i=%d: read_write_ordering test failed. Expected 0x%x got 0x%x\n",lwrLoop,i,m_testData[i],rt);
            local_rw_status++;
            local_status = 0;

        }
        count++;
        i++;
        if(i>=limit)
            i=0;
    }

    mask=15;
    lwgpu->RegWr32(0x00100C04, ( ~(mask<<24)  & ~0) ) ;//RegWr32(LW_PFB_NISO_ARB0, 0);
    lwgpu->RegWr32(0x00100C08, 0xFFFFFFFF) ;//RegWr32(LW_PFB_NISO_ARB1, 0xFFFFFFFF);

    for (i = 0; i < limit; i++) 
    {
        m_testData[i] = i+lwrLoop;
        uintptr_t addr = reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress())+i*4;
        Platform::VirtualWr((void*)addr, m_testData+i, 4);
    }

    InfoPrintf("read_write_ordering: Reading back data from BAR1(frontdoor)...starting from 3Q\n");
    //from back

    i=(3*limit)/4;
    count =0;

    mask=14;
    lwgpu->RegWr32(0x00100C04, ( ~(mask<<24)  & ~0) ) ;//RegWr32(LW_PFB_NISO_ARB0, 0);

    while(count<limit) {
        UINT32 rt = MEM_RD32(reinterpret_cast<uintptr_t>(m_testBuffer.GetAddress()) + i*4);
        //InfoPrintf("Loop %d i=%d: Expected 0x%x got 0x%x\n",lwrLoop,i,m_testData[i],rt);
        if (m_testData[i] != rt) {
            ErrPrintf("Loop %d i=%d: read_write_ordering test failed. Expected 0x%x got 0x%x\n",lwrLoop,i,m_testData[i],rt);
            local_rw_status++;
            local_status = 0;

        }
        count++;
        i++;
        if(i>=limit)
            i=0;
    }

    if(local_rw_status==0)
        InfoPrintf("Loop %d : read_write_ordering test passed\n",lwrLoop);
    else
        InfoPrintf("Loop %d : read_write_ordering test failed\n",lwrLoop);

    return;
}
