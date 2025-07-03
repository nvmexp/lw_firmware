/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "fb_common.h"

#include "sim/IChip.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/gpu/lwgpu_ch_helper.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"
#include "mdiag/utils/lwgpu_channel.h"

#include "lwmisc.h"

#include "pascal/gp100/dev_fbpa.h"
#include "pascal/gp100/dev_flush.h"
#include "pascal/gp100/dev_fifo.h"
#include "pascal/gp100/dev_ram.h"
#include "pascal/gp100/dev_fb.h"
#include "pascal/gp100/dev_ltc.h"

#include "gpu/include/floorsweepimpl.h"

#define TSTR "FBTest:"

ParamDecl FBTest::fbtest_common_params[] = {
    PARAM_SUBDECL(GpuChannelHelper::Params),
    UNSIGNED_PARAM("-loop", "Number of loops to run (default is 1)"),
    UNSIGNED_PARAM("-randTestLoop", "Number of randome tests to run (default is 64)"),
    SIMPLE_PARAM("-useSysmem", "use sysmem for mem2mem copies (default is vidmem)"),
    UNSIGNED_PARAM("-seed0",                "Random stream seed"),
    UNSIGNED_PARAM("-seed1",                "Random stream seed"),
    UNSIGNED_PARAM("-seed2",                "Random stream seed"),
    LAST_PARAM
};

FBTest::FBTest(ArgReader *params) :
    LoopingTest(params),
    m_pParams(params),
    lwgpu(NULL),
    chHelper(NULL),
    RndStream(NULL),
    fbHelper(NULL)
{
    chHelper = mk_GpuChannelHelper("fb_test:", NULL, this, LwRmPtr().Get());
    if (!chHelper) {
        ErrPrintf(TSTR "create GpuChannelHelper failed\n");
    }
    else if (!chHelper->ParseChannelArgs(params)) {
        ErrPrintf(TSTR ": Channel Helper ParseChannelArgs failed\n");
        delete chHelper;
        chHelper = 0;
    }

    seed0 = params->ParamUnsigned("-seed0", 0x1234);
    seed1 = params->ParamUnsigned("-seed1", 0x5678);
    seed2 = params->ParamUnsigned("-seed2", 0x9abc);
    useSysMem = params->ParamPresent("-useSysmem") != 0;
    loopCount = params->ParamUnsigned("-loop", 1);
    rndTestCount = params->ParamUnsigned("-randTestLoop", 64);

}

FBTest::~FBTest()
{
    if (m_pParams != NULL)
        delete m_pParams;
    if (chHelper != NULL)
        delete chHelper;
}

void FBTest::DebugPrintArgs()
{
    DebugPrintf("FBTest args: -seed0 0x%x -seed1 0x%x -seed2 0x%x -loop %d -randTestLoop %d\n",
                seed0, seed1, seed2, loopCount, rndTestCount);
}

int FBTest::Setup()
{
    InfoPrintf("FBTest::Setup() starts\n");

    if (!chHelper->acquire_gpu_resource()) 
    {
        ErrPrintf("Can't aquire GPU\n");
        return 0;
    }
    lwgpu = chHelper->gpu_resource();
    gpuSubDev = lwgpu->GetGpuSubdevice(0);

    fbHelper = new FBHelper(this, lwgpu, chHelper);
    RndStream = new RandomStream(seed0, seed1, seed2);
    InfoPrintf("FBTest::Setup() finish\n");
    return 1;
}

void FBTest::CleanUp(void)
{
    if(lwgpu != NULL) {
        DebugPrintf("FBTest::CleanUp(): Releasing GPU.\n");
        chHelper->release_gpu_resource();
    }
    lwgpu = NULL;

    if (fbHelper != NULL)
        delete fbHelper;
    if (RndStream != NULL)
        delete RndStream;

    DebugPrintf("FBTest::CleanUp() done\n");
}

FBHelper::FBHelper(FBTest *fbtest, LWGpuResource *lwgpu, GpuChannelHelper *chHelper) :
    m_fbtest(fbtest),
    m_lwgpu(lwgpu),
    m_status(1)
{
    m_gpuSubDev = m_lwgpu->GetGpuSubdevice(0);
    MASSERT((m_lwgpu != NULL));
}

FBHelper::~FBHelper()
{
}

void FBHelper::SetBackdoor(bool en)
{
    UINT32 en_v = en? 1 : 0;

    if(Platform::GetSimulationMode() != Platform::Hardware) {
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, en_v);
        // bug 527528 -
        // Comments by Joseph Harwood:
        //   DUT in the fermi fullchip testbench is the GPU. Only backdoor sysmem accesses will work in this testbench,
        //   because only the GPU is on the PCI, and the GPU (the real chip) does not process memory accesses
        //   for other chips, only for itself.
        // Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_SYS, 4, en_v);
    }

    (void)en_v;
}

void FBHelper::Flush()
{

    // Send down an FB_FLUSH, MODS will use the correct register based on
    // PCIe GEN of HW.
    m_lwgpu->GetGpuSubdevice()->FbFlush(Tasker::NO_TIMEOUT);

}

unsigned FBHelper::ReadRegister(string regName) {
    RefManual* refMan = m_gpuSubDev->GetRefManual();
    MASSERT(refMan);

    //InfoPrintf("FBTest::ReadRegister() Trying to find register %s\n", regName.c_str());
    const RefManualRegister* pReg = refMan->FindRegister(regName.c_str());
    MASSERT(pReg && pReg->IsReadable());

    //InfoPrintf("FBTest::ReadRegister() Trying to read register address %x\n", pReg->GetOffset());
    return m_gpuSubDev->RegRd32( pReg->GetOffset() );
}

UINT32 FBHelper::PhysRd32(UINT64 addr)
{
    return 0x0;
}

void FBHelper::PhysWr32(UINT64 addr, UINT32 data)
{

}

void FBHelper::PhysRdBlk(UINT64 addr, UINT32 *data, UINT32 size)
{

}

void FBHelper::PhysWrBlk(UINT64 addr, const UINT32 *data, UINT32 size)
{

}

UINT64 FBHelper::SetBar0Window(UINT64 addr, UINT32 target)
{
    UINT32 base = (UINT32)((addr >> 16) & (UINT64)0xffffffff);
    RegHal& regHal = m_gpuSubDev->Regs();
    
    UINT32 bar0Address = regHal.Read32(MODS_XAL_EP_BAR0_WINDOW);
    regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, 
        regHal.SetField(MODS_XAL_EP_BAR0_WINDOW_BASE, base));
   
    return ((UINT64)bar0Address) << 16;
}

UINT32 FBHelper::PhysRd32_Bar0Window(UINT64 addr, UINT32 target)
{
    UINT32 base = (UINT32)((addr >> 16) & (UINT64)0xffffffff);
    RegHal& regHal = m_gpuSubDev->Regs();

    UINT32 bar0Address = regHal.Read32(MODS_XAL_EP_BAR0_WINDOW);
    regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, 
        regHal.SetField(MODS_XAL_EP_BAR0_WINDOW_BASE, base));
    
    UINT32 data = m_lwgpu->RegRd32(DEVICE_BASE(LW_PRAMIN) + (UINT32)(addr & 0xffff));

    // restoring bar0 window
    regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, bar0Address);
    
    return data;
}

void FBHelper::PhysWr32_Bar0Window(UINT64 addr, UINT32 data, UINT32 target)
{
    UINT32 base = (UINT32)((addr >> 16) & (UINT64)0xffffffff);
    RegHal& regHal = m_gpuSubDev->Regs();

    UINT32 bar0Address = regHal.Read32(MODS_XAL_EP_BAR0_WINDOW);
    regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, 
        regHal.SetField(MODS_XAL_EP_BAR0_WINDOW_BASE, base));

    m_lwgpu->RegWr32(DEVICE_BASE(LW_PRAMIN) + (UINT32)(addr & 0xffff), data);

    // restoring bar0 window
    regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, bar0Address);
}

void FBHelper::PhysRdBlk_Bar0Window(UINT64 addr, unsigned char *data, UINT32 size, UINT32 target)
{
    RegHal& regHal = m_gpuSubDev->Regs();

    UINT32 bar0Address = regHal.Read32(MODS_XAL_EP_BAR0_WINDOW);
    UINT32 *p_data32 = (UINT32 *)data;
    // TBD add assertion here. size % 4 == 0

    UINT32 i = 0;
    while (i < size) 
    {
        UINT32 base = (UINT32)((addr >> 16) & (UINT64)0xffffffff);
        regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, 
            regHal.SetField(MODS_XAL_EP_BAR0_WINDOW_BASE, base));
        
        for (UINT32 j = 0; i < size && j < 0x10000; i += 4, j += 4, addr += 4) 
        {
            p_data32[i / 4] = m_lwgpu->RegRd32(DEVICE_BASE(LW_PRAMIN) + (UINT32)(addr & 0xffff));
        }
    }
    
    // restoring bar0 window
    regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, bar0Address);
}

void FBHelper::PhysWrBlk_Bar0Window(UINT64 addr, const unsigned char *data, UINT32 size, UINT32 target)
{
    RegHal& regHal = m_gpuSubDev->Regs();

    UINT32 bar0Address = regHal.Read32(MODS_XAL_EP_BAR0_WINDOW);
    const UINT32 *p_data32 = (const UINT32 *)data;
    // TBD add assertion here. size % 4 == 0

    UINT32 i = 0;
    while (i < size) 
    {
        UINT32 base = (UINT32)((addr >> 16) & (UINT64)0xffffffff);
        regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, 
            regHal.SetField(MODS_XAL_EP_BAR0_WINDOW_BASE, base));

        for (UINT32 j = 0; i < size && j < 0x10000; i += 4, j += 4, addr += 4) 
        {
            m_lwgpu->RegWr32(DEVICE_BASE(LW_PRAMIN) + (UINT32)(addr & 0xffff), p_data32[i / 4]);
        }
    }

    // restoring bar0 window
    regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, bar0Address);
}

UINT64 FBHelper::FBSize()
{
    const RegHal &regs  = m_gpuSubDev->Regs();
    const UINT32 fbioMask = m_gpuSubDev->GetFB()->GetFbioMask();
    UINT64 cap = 0; 
    for (INT32 fbio = Utility::BitScanForward(fbioMask); fbio >= 0; fbio = Utility::BitScanForward(fbioMask, fbio + 1))
    {
        const UINT32 fbioSizeMb = regs.Read32(MODS_PFB_FBPA_0_CSTATUS_RAMAMOUNT, fbio);
        const UINT64 fbioSize = static_cast<UINT64>(fbioSizeMb) << 20;
        InfoPrintf("Reg fbio_%d, dram capacity %d\n",fbio, fbioSizeMb);
        cap  += fbioSize;
    }
    
    return cap; 
}

void FBHelper::SetupSurface(Surface * buffer, UINT32 width, UINT32 height)
{
    SetupSurface(buffer, width, height, 4096, 4);
}

void FBHelper::SetupSurface(Surface * buffer, UINT32 width, UINT32 height, UINT32 align, UINT32 page_size)
{
    MASSERT(buffer != NULL);

    if(buffer == NULL) {
        ErrPrintf("frontdoor::SetupSurface - Buffer passed is null.\n");
        return;
    }

    buffer->SetWidth(width);
    buffer->SetHeight(height);
    buffer->SetLocation(Memory::Fb);
    buffer->SetColorFormat(ColorUtils::Y8);
    buffer->SetProtect(Memory::ReadWrite);
    buffer->SetAlignment(align);
    buffer->SetLayout(Surface::Pitch);

    buffer->SetPageSize(page_size);
    // buffer->SetPageSize(0);

    if(OK != buffer->Alloc(m_lwgpu->GetGpuDevice()))
    {
        m_fbtest->SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("frontdoor: Cannot create dst buffer.\n");
        return;
    }

    if(OK != buffer->Map())
    {
        ErrPrintf("Can't map the source buffer.\n");
        m_fbtest->SetStatus(Test::TEST_NO_RESOURCES);
        return;
    }

    DebugPrintf("Surf2D allocated a surface:\n");
    DebugPrintf("  Size = 0x%08x\n", buffer->GetSize());
    DebugPrintf("  hMem = 0x%08x\n", buffer->GetMemHandle());
    DebugPrintf("  hVirtMem = 0x%08x\n", buffer->GetVirtMemHandle());
    DebugPrintf("  hCtxDmaGpu = 0x%08x\n", buffer->GetCtxDmaHandle());
    DebugPrintf("  OffsetGpu = 0x%llx\n", buffer->GetCtxDmaOffsetGpu());
    DebugPrintf("  hCtxDmaIso = 0x%08x\n", buffer->GetCtxDmaHandleIso());
    DebugPrintf("  OffsetIso = 0x%llx\n", buffer->GetCtxDmaOffsetIso());
    // print this only when we did a VidHeapAlloc
    DebugPrintf("  VidMemOffset = 0x%08x\n", buffer->GetVidMemOffset());
}

void FBHelper::FreeSurface(Surface *buffer)
{
    if (buffer->GetSize()) {
        buffer->Unmap();
        buffer->Free();
    }
}

// _PAGE_LAYOUT FBHelper::PageSize()
// {
//     UINT32 size = Platform::GetPageSize();
//     _PAGE_LAYOUT pg = PAGE_VIRTUAL_4KB;

//     DebugPrintf("FBHelper::PageSize() page size %dk\n", size / 1024);
//     switch (size) {
//     case 4096:
//         pg = PAGE_VIRTUAL_4KB;
//         break;
//     case 64 * 1024:
//         pg = PAGE_VIRTUAL_64KB;
//         break;
// //     case 128 * 1024:
// //         pg = PAGE_VIRTUAL_128KB;
// //         break;
//     default:
//         ErrPrintf("FBHelper::PageSize() unknown page size 0x%x\n", size);
//         break;
//     }

//     return pg;
// }

UINT32 FBHelper::NumFBPs()
{
    return m_gpuSubDev->GetFB()->GetFbpCount();
    // return m_lwgpu->RegRd32(LW_PLTCG_LTC0_LTS0_CBC_NUM_ACTIVE_FBPS);//LW_PFB_FBHUB_NUM_ACTIVE_FBPS);
}

UINT32 FBHelper::NumFBPAs()
{
    return Utility::CountBits(m_gpuSubDev->GetFsImpl()->FbpaMask());
}

UINT32 FBHelper::NumL2Slices()
{
    // return m_lwgpu->RegRd32(LW_PLTS_CBC_PARAM_SLICES_PER_FBP);
    // FIXME[Fung] read value from proper register
    return 2;
}

//UINT32 FBHelper::NumRows()
//{
//    UINT64 cfg0 = 0, cfg1 = 0, rowa = 0, rowb = 0;
//
//    cfg0 = m_lwgpu->RegRd32(LW_PFB_FBPA_CFG0);
//    cfg1 = m_lwgpu->RegRd32(LW_PFB_FBPA_CFG1);
//
//    switch (DRF_VAL(_PFB, _FBPA_CFG1, _ROWA, cfg1)) {
//    case LW_PFB_FBPA_CFG1_ROWA_10: rowa = 1024; break;
//    case LW_PFB_FBPA_CFG1_ROWA_11: rowa = 2048; break;
//    case LW_PFB_FBPA_CFG1_ROWA_12: rowa = 4096; break;
//    case LW_PFB_FBPA_CFG1_ROWA_13: rowa = 8192; break;
//    case LW_PFB_FBPA_CFG1_ROWA_14: rowa = 16384; break;
//    default: break;
//    }
//
//    if (DRF_VAL(_PFB, _FBPA_CFG0, _EXTBANK, cfg0) == LW_PFB_FBPA_CFG0_EXTBANK_1) {
//        switch (DRF_VAL(_PFB, _FBPA_CFG1, _ROWB, cfg1)) {
//        case LW_PFB_FBPA_CFG1_ROWB_10: rowb = 1024; break;
//        case LW_PFB_FBPA_CFG1_ROWB_11: rowb = 2048; break;
//        case LW_PFB_FBPA_CFG1_ROWB_12: rowb = 4096; break;
//        case LW_PFB_FBPA_CFG1_ROWB_13: rowb = 8192; break;
//        case LW_PFB_FBPA_CFG1_ROWB_14: rowb = 16384; break;
//        default: break;
//        }
//    }
//
//    return rowa + rowb;
//}
//
//UINT32 FBHelper::NumIntBanks()
//{
//    UINT64 cfg1 = 0, bank = 0;
//
//    cfg1 = m_lwgpu->RegRd32(LW_PFB_FBPA_CFG1);
//
//    switch (DRF_VAL(_PFB, _FBPA_CFG1, _BANK, cfg1)) {
//    case LW_PFB_FBPA_CFG1_BANK_2: bank = 4; break;
//    case LW_PFB_FBPA_CFG1_BANK_3: bank = 8; break;
//    case LW_PFB_FBPA_CFG1_BANK_4: bank = 16; break;
//    default: break;
//    }
//
//    return bank;
//}
//
//UINT32 FBHelper::NumExtBanks()
//{
//    UINT64 cfg0 = 0, extbank = 0;
//
//    cfg0 = m_lwgpu->RegRd32(LW_PFB_FBPA_CFG0);
//
//    switch (DRF_VAL(_PFB, _FBPA_CFG0, _EXTBANK, cfg0)) {
//    case LW_PFB_FBPA_CFG0_EXTBANK_0: extbank = 1; break;
//    case LW_PFB_FBPA_CFG0_EXTBANK_1: extbank = 2; break;
//    default: break;
//    }
//
//    return extbank;
//}
//
//UINT32 FBHelper::NumCols()
//{
//    UINT64 cfg1 = 0, col = 0;
//
//    cfg1 = m_lwgpu->RegRd32(LW_PFB_FBPA_CFG1);
//
//    switch (DRF_VAL(_PFB, _FBPA_CFG1, _COL, cfg1)) {
//    case LW_PFB_FBPA_CFG1_COL_8: col = 256; break;
//    case LW_PFB_FBPA_CFG1_COL_9: col = 512; break;
//    case LW_PFB_FBPA_CFG1_COL_10: col = 1024; break;
//    case LW_PFB_FBPA_CFG1_COL_11: col = 2048; break;
//    case LW_PFB_FBPA_CFG1_COL_12: col = 4096; break;
//    case LW_PFB_FBPA_CFG1_COL_13: col = 8192; break;
//    case LW_PFB_FBPA_CFG1_COL_14: col = 16384; break;
//    case LW_PFB_FBPA_CFG1_COL_15: col = 32768; break;
//    default: break;
//    }
//
//    return col;
//}

Mem::Mem(void)
{
    m_valid = 0;
}

Mem::~Mem(void)
{
    this->UnMap();
}

void Mem::UnMap(void)
{
    if ( m_valid ) {
        Platform::UnMapDeviceMemory((void *)m_vbase);
        InfoPrintf("myMem::Unmap 0x%08x - 0x%08x vbase = %08x\n",m_base,m_base+m_size-1,m_vbase);
        m_valid = 0;
    }
}

// map the range of memory to use for test
int Mem::Map(UINT64 base,UINT32 size)
{
    RC rc;
    void * v;
    Memory::Attrib MapAttrib = Memory::UC;

    // first, if the previous one is valid, and the same,do nothing

    if ( m_valid  ) {
        if ( (base == m_base) && (size == m_size) ) {
            InfoPrintf("Already Map\n");
            Dump();
            return 0;
        } else {
            this->UnMap();
        }
    }

    // Assume a read/write mapping.
    rc = Platform::MapDeviceMemory(&v, base, size, MapAttrib,
                                   Memory::ReadWrite);

    if (OK != rc)
    {
        InfoPrintf("error mapping %x size %x\n",base,size);
        return 1;
    }

    m_vbase =*(UINT32 *)&v;
    m_base = base;
    m_size = size;
    m_valid = 1;

    InfoPrintf("Done mapping\n");
    Dump();

    return 0;
}

void Mem::Dump(void)
{
    if ( m_valid ) {
        InfoPrintf("myMem: 0x%08x - 0x%08x vaddr = 0x%08x\n",m_base,m_base+m_size-1,m_vbase);
    } else {
        InfoPrintf("myMem: invalid mapping\n");
    }

}
