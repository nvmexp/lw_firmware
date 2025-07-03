/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015,2019-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"
#include "core/include/jscript.h"

#include "core/include/channel.h"
#include "max_memory2.h"
#include "sim/IChip.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"
#include "mdiag/utils/surfutil.h"

#include "kepler/gk104/dev_flush.h"
#include "kepler/gk104/dev_fifo.h"
#include "kepler/gk104/dev_ram.h"

#include "class/cla0b5.h"
#include "class/clb0b5.h"
#include "class/cl90b5.h"
#include "class/cla06fsubch.h"
#include "ctrl/ctrl2080/ctrl2080bus.h"

#include "lwos.h"

#include "gpu/utility/surf2d.h"

const UINT64 BUFFER_SIZE_1K = 1024;
const UINT64 BUFFER_SIZE_4K = 4 * 1024;
const UINT64 BUFFER_SIZE_16K = 16 * 1024;

const UINT64 BLK_RW_TEST_BUFFER_SIZE = BUFFER_SIZE_4K;

//
//   This test reads/writes to min/max address using BAR0 window.
//
extern const ParamDecl max_memory2_params[] = {
    PARAM_SUBDECL(FBTest::fbtest_common_params),
    MEMORY_SPACE_PARAMS("_buf0",    "dma buffer 0 used for testing"),
    UNSIGNED_PARAM("-size",         "size of dma buffer for testing"),
    UNSIGNED_PARAM("-mem_loc",      "specify the location of memory for r/w. Can be 0(vid), 1(peer), 2(coh), 3(ncoh))"),
    UNSIGNED64_PARAM("-test_phys_addr", "address to test"),
    SIMPLE_PARAM("-cpu_write", "Issue CPU writes after flushing the channels"),
    SIMPLE_PARAM("-dst_cache_off", "Disable GPU cache for the destination buffer"),
    LAST_PARAM
};

max_memory2Test::max_memory2Test(ArgReader *reader) :
    FBTest(reader),
    params(reader),
    m_dramCap(0),
    m_num_fbpas(8),
    m_size(1024),
    m_target(0x0),
    m_bLoopback(false),
    m_bDoCpuWrite(false),
    m_bCachable(true)
{
    ParseDmaBufferArgs(m_dmaBuffer, params, "buf0", &m_pteKindName, 0);
    m_dmaBuffer.SetPageSize(4);

    m_size = params->ParamUnsigned("-size", BUFFER_SIZE_16K); // 16K
    m_phys_addr = params->ParamUnsigned64("-test_phys_addr", 0x0);
    m_target = params->ParamUnsigned("-mem_loc", 0x0);
    m_bLoopback = m_target == 0x1;
    m_bDoCpuWrite = params->ParamPresent("-cpu_write") != 0;
    m_bCachable = params->ParamPresent("-dst_cache_off") == 0;

    MASSERT(m_target >= 0 && m_target <= 3 && "Invalid aperture!");

    m_dmaBuffer.SetLocation(Memory::Fb);

    m_DmaCopyAlloc.SetOldestSupportedClass(GF100_DMA_COPY);

    FBTest::DebugPrintArgs();
}

max_memory2Test::~max_memory2Test(void)
{
}

STD_TEST_FACTORY(max_memory2, max_memory2Test)

// a little extra setup to be done
int max_memory2Test::Setup(void)
{
    local_status = 1;

    InfoPrintf("max_memory2Test::Setup() starts\n");

    getStateReport()->enable();

    if (!FBTest::Setup()) {
        ErrPrintf("max_memory2Test::Setup() Fail\n");
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
    SetPageLayout(m_dmaBuffer, PAGE_VIRTUAL_4KB);
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

    InfoPrintf("max_memory2Test::Dma buffer Vidmem offset = 0x%012x, VA = %lx, PA = %llx\n", m_dmaBuffer.GetVidMemOffset(), m_dmaBuffer.GetAddress(), m_dmaBuffer.GetPhysAddress());
    InfoPrintf("max_memory2Test::Setup() done\n");

    return 1;
}

// a little extra cleanup to be done
void max_memory2Test::CleanUp(void)
{
    DebugPrintf("max_memory2Test::CleanUp() starts\n");

    if (m_dmaBuffer.IsMapped()) {
        m_dmaBuffer.Unmap();
    }
    if (m_dmaBuffer.GetSize()) {
        m_dmaBuffer.Free();
    }

    m_DmaCopyAlloc.Free();

    chHelper->release_channel();
    ch = NULL;

    FBTest::CleanUp();

    DebugPrintf("max_memory2Test::CleanUp() done\n");
}

void max_memory2Test::Run(void)
{
    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("max_memory2Test::Run() starts\n");

    m_dramCap = fbHelper->FBSize();

    InfoPrintf("max_memory2Test: Capacity(%d fbpas): %lldMB(%llx).\n", m_num_fbpas, (m_dramCap >> 20), m_dramCap);
    InfoPrintf("max_memory2Test: Max address: 0x%010llx.\n", m_dramCap - 1);
    InfoPrintf("max_memory2Test: Physical Address of FB Start: 0x%08x.\n", lwgpu->PhysFbBase());

    // Turn off backdoor accesses
    // bug 527528 -
    // Comments by Joseph Harwood:
    //   DUT in the fermi fullchip testbench is the GPU. Only backdoor sysmem accesses will work in this testbench,
    //   because only the GPU is on the PCI, and the GPU (the real chip) does not process memory accesses
    //   for other chips, only for itself.
    //
    // We don't have to force frontdoor for this test. As safe fix, enable backdoor for TestSysMem only.
    fbHelper->SetBackdoor(false);

    int ret = 0;

    switch (m_target)
    {
    case 2:
    case 3:
        ret = TestSysMem();
        break;

    case 1:
        ret = TestP2pMem();
        break;

    case 0:
        ret = TestVidMem();
        break;

    default:
        MASSERT(0 && "Unsupported aperture!");
        break;
    }

    if (local_status != 0 && ret == 0) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    }
    else {
        SetStatus(ret == 0 ? Test::TEST_FAILED_CRC : Test::TEST_NO_RESOURCES);
        getStateReport()->runFailed("Run failed, see errors printed!");
    }
}

int max_memory2Test::CreateLoopbackSurface(Surface2D& buf, UINT64 size)
{
    buf.SetLocation(Memory::Fb);
    buf.SetArrayPitch(size);
    buf.SetColorFormat(ColorUtils::Y8);
    buf.SetForceSizeAlloc(true);
    buf.SetProtect(Memory::ReadWrite);
    buf.SetLoopBack(true);
    if (m_phys_addr > 0)
        buf.SetGpuPhysAddrHint(m_phys_addr);
    buf.SetPhysContig(false);
    buf.SetPageSize(4);
    if (m_pteKindName != "") {
        UINT32 pteKind = 0;
        lwgpu->GetGpuDevice()->GetSubdevice(0)->GetPteKindFromName(m_pteKindName.c_str(), &pteKind);
        buf.SetPteKind(pteKind);
    }
    buf.SetGpuCacheMode(m_bCachable ? Surface2D::GpuCacheOn : Surface2D::GpuCacheOff);
    buf.SetP2PGpuCacheMode(m_bCachable ? Surface2D::GpuCacheOn : Surface2D::GpuCacheOff);

    if (OK != buf.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("Error in allocating loopback surface!\n");
        return -1;
    }
    if (OK != buf.MapLoopback()) {
        ErrPrintf("Error in mapping loopback surface!\n");
        return -1;
    }
    MASSERT(buf.IsPeerMapped());
    return 0;
}

int max_memory2Test::DoCopyEngineTest(UINT64 offset1)
{
    UINT32 width, height;
    static const UINT32 MAXW = 1024;

    if (m_size <= MAXW) {
        width = m_size, height = 1;
    }
    else {
        width = MAXW, height = m_size / MAXW;
    }
    m_size = width * height;

    InfoPrintf("Filling src buffer...\n");
    for (UINT32 i = 0; i < m_size; i += 4)
        MEM_WR32(reinterpret_cast<uintptr_t>(m_dmaBuffer.GetAddress()) + i, i / 4);

    // Creating a GF100_DMA_COPY channel
    if (!chHelper->acquire_channel() ) {
        ErrPrintf("Channel Create failed\n");
        //chHelper->release_gpu_resource();
        return 1;
    }
    ch = chHelper->channel();
    MASSERT(ch && "Getting channel failed!");

    RC rc;
    GpuSubdevice* pGpuSubdevice = lwgpu->GetGpuSubdevice();
    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;
    if (pGpuSubdevice->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        vector<UINT32> supportedCEs;
        CHECK_RC(lwgpu->GetSupportedCeNum(pGpuSubdevice, &supportedCEs, LwRmPtr().Get()));
        MASSERT(supportedCEs.size() != 0);
        engineId = MDiagUtils::GetCopyEngineId(supportedCEs[0]);
    }

    if (!chHelper->alloc_channel(engineId))
    {
        ErrPrintf("Channel Alloc failed\n");
        chHelper->release_channel();
        chHelper->release_gpu_resource();
        return 1;
    }

    UINT32 hChipClass;

    CHECK_RC(m_DmaCopyAlloc.Alloc(ch->ChannelHandle(), lwgpu->GetGpuDevice()));
    hChipClass = m_DmaCopyAlloc.GetHandle();
    MASSERT(hChipClass);
    subChannel = LWA06F_SUBCHANNEL_COPY_ENGINE;
    ch->SetObject(subChannel, hChipClass);

    DebugPrintf("Subchannel %d set\n", subChannel);

//    UINT32 data[4] = {0x0, 0x0, 0x0, 0x0 };

    UINT64 offset2 = m_dmaBuffer.GetCtxDmaOffsetGpu();

    InfoPrintf("Copying from vidmem to dst mem...\n");
    ch->MethodWrite(subChannel, LW90B5_OFFSET_IN_UPPER, (UINT32)(offset2 >> 32));
    ch->MethodWrite(subChannel, LW90B5_OFFSET_IN_LOWER, (UINT32)(offset2 & 0xFFFFFFFF));
    ch->MethodWrite(subChannel, LW90B5_LINE_LENGTH_IN, width);
    ch->MethodWrite(subChannel, LW90B5_PITCH_IN, width);
    ch->MethodWrite(subChannel, LW90B5_OFFSET_OUT_UPPER, (UINT32)(offset1 >> 32));
    ch->MethodWrite(subChannel, LW90B5_OFFSET_OUT_LOWER, (UINT32)(offset1 & 0xFFFFFFFF));
    //ch->MethodWrite(subChannel, LW90B5_LINE_LENGTH_OUT, width);
    ch->MethodWrite(subChannel, LW90B5_PITCH_OUT, width);
    ch->MethodWrite(subChannel, LW90B5_LINE_COUNT, height);
    ch->MethodWrite(subChannel, LW90B5_LAUNCH_DMA, 0x201 | 0x80 | 0x100);

    ch->MethodFlush();
    //ch->GetModsChannel()->WaitIdle(1);

    if (m_bDoCpuWrite) {
        if (!m_dmaBuffer.IsMapped()) {
            m_dmaBuffer.Map();
        }
        for (UINT32 i = 0; i < m_size; i += 4)
            MEM_WR32((uintptr_t)m_dmaBuffer.GetAddress() + i, 0xdeadbeaf);
        MEM_RD32((uintptr_t)m_dmaBuffer.GetAddress());
    }

    ch->WaitForIdle();

    InfoPrintf("Copying from dst mem to vidmem...\n");
    ch->MethodWrite(subChannel, LW90B5_OFFSET_IN_UPPER, (UINT32)(offset1 >> 32));
    ch->MethodWrite(subChannel, LW90B5_OFFSET_IN_LOWER, (UINT32)(offset1 & 0xFFFFFFFF));
    ch->MethodWrite(subChannel, LW90B5_LINE_LENGTH_IN, width);
    ch->MethodWrite(subChannel, LW90B5_PITCH_IN, width);
    ch->MethodWrite(subChannel, LW90B5_OFFSET_OUT_UPPER, (UINT32)(offset2 >> 32));
    ch->MethodWrite(subChannel, LW90B5_OFFSET_OUT_LOWER, (UINT32)(offset2 & 0xFFFFFFFF));
    ch->MethodWrite(subChannel, LW90B5_PITCH_OUT, width);
    ch->MethodWrite(subChannel, LW90B5_LINE_COUNT, height);
    ch->MethodWrite(subChannel, LW90B5_LAUNCH_DMA, 0x201 | 0x80 | 0x100);

    ch->WaitForIdle();

    InfoPrintf("Checking CE results...\n");
    UINT32 nErr = 0;

    fbHelper->SetBackdoor(true);
    for (UINT32 i = 0; i < m_size && nErr < 100; i += 4)
    {
        UINT32 data = MEM_RD32(reinterpret_cast<uintptr_t>(m_dmaBuffer.GetAddress()) + i);
        if (data != i / 4) {
            ErrPrintf("max_memory2Test: Data checking ERROR @%lx! Data = %lx\n",
                      i/4, data);
            ++nErr;
            local_status = 0;
        }
    }

    if (local_status == 0) {
        ErrPrintf("max_memory2Test: CE test FAILED\n");
    }

    //buf2.UnmapRemote(lwgpu->GetGpuDevice());

    return 0;
}

int max_memory2Test::TestVidMem()
{
    InfoPrintf("max_memory2Test: Starting vidmem test...\n");

    Surface2D buf;
    buf.SetLayout(Surface2D::Pitch);
    buf.SetLocation(Memory::Fb);
    //buf.SetPageSize(4);
    buf.SetArrayPitch(m_size);
    buf.SetColorFormat(ColorUtils::Y8);
    buf.SetForceSizeAlloc(true);
    buf.SetProtect(Memory::ReadWrite);
    buf.SetGpuPhysAddrHint(m_phys_addr);
    buf.SetPhysContig(false);
    buf.SetPageSize(4);
    if (m_pteKindName != "") {
        UINT32 pteKind = 0;
        lwgpu->GetGpuDevice()->GetSubdevice(0)->GetPteKindFromName(m_pteKindName.c_str(), &pteKind);
        buf.SetPteKind(pteKind);
    }
    if (OK != buf.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("Error in allocating 2nd surface!\n");
        return -1;
    }
    if (OK != buf.Map()) {
        ErrPrintf("Error in mapping 2nd surface!\n");
        return -1;
    }

    UINT64 offset = buf.GetCtxDmaOffsetGpu();

    InfoPrintf("max_memory2Test::2nd buffer Vidmem offset = 0x%012x, VA = %lx, PA = %llx, CtxDmaOffset = %llx\n",
               buf.GetVidMemOffset(), buf.GetAddress(), buf.GetPhysAddress(), offset);

    int ret = DoCopyEngineTest(offset);

    buf.Free();
    InfoPrintf("max_memory2Test: vidmem test done.\n");
    return ret;
}

//
// Test peer memory
//
int max_memory2Test::TestP2pMem()
{
    InfoPrintf("max_memory2Test: Starting peer memory test...\n");
//    BasicBufferTest(&m_dmaBuffer, m_size / 256);

    // Setting the 2nd buffer
    UINT64 large_size = m_size;
    Surface2D buf2;
    if (CreateLoopbackSurface(buf2, large_size))
    {
        local_status = 0;
        return -1;
    }
    buf2.Map();

    UINT64 offset1 = buf2.GetCtxDmaOffsetGpuPeer(0)
        + (large_size > m_size ? large_size - m_size : 0);

    int ret = DoCopyEngineTest(offset1);
//    UINT64 addr = buf2.GetGpuVirtAddrPeer(0);
//    MEM_WR32((uintptr_t)addr, 0xdeadbeef);

    buf2.Free();

    InfoPrintf("max_memory2Test: Peer memory test complete.\n");

    return ret;
}

//
// Test system memory
//
int max_memory2Test::TestSysMem()
{
    InfoPrintf("max_memory2Test: Starting sysmem test...\n");

    // bug 527528 -
    // Comments by Joseph Harwood:
    //   DUT in the fermi fullchip testbench is the GPU. Only backdoor sysmem accesses will work in this testbench,
    //   because only the GPU is on the PCI, and the GPU (the real chip) does not process memory accesses
    //   for other chips, only for itself.
    //
    // there is no reason to disable backdoor access for this test. It stresses FB by doing dma copy instead of CPU r/w.
    fbHelper->SetBackdoor(true);

    MdiagSurf buf;
    buf.SetLocation(m_target == 2 ? Memory::Coherent : Memory::NonCoherent);
    buf.SetPageSize(4);
    buf.SetArrayPitch(m_size);
    buf.SetColorFormat(ColorUtils::Y8);
    buf.SetForceSizeAlloc(true);
    buf.SetProtect(Memory::ReadWrite);
    SetPageLayout(buf, PAGE_VIRTUAL_4KB);
    if (OK != SetPteKind(buf, m_pteKindName, lwgpu->GetGpuDevice())) {
        ErrPrintf("Error in creating 2nd surface!\n");
        return -1;
    }
    buf.SetGpuCacheMode(m_bCachable ? Surface2D::GpuCacheOn : Surface2D::GpuCacheOff);

    buf.SetForceSizeAlloc(true);
    // buf.SetGpuPhysAddrHint(m_phys_addr);
    if (OK != buf.Alloc(lwgpu->GetGpuDevice()) || OK != buf.Map()) {
        ErrPrintf("Error in creating 2nd surface!\n");
        return -1;
    }
    UINT64 offset = buf.GetCtxDmaOffsetGpu();

    InfoPrintf("max_memory2Test::2nd buffer Vidmem offset = 0x%012x, VA = %lx, PA = %llx, CtxDmaOffset = %llx\n",
               buf.GetVidMemOffset(), buf.GetAddress(), buf.GetPhysAddress(), offset);

    int ret = DoCopyEngineTest(offset);

    buf.Free();

    InfoPrintf("max_memory2Test: Sysmem test complete.\n");

    return ret;
}
