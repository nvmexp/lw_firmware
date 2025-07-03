/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013,2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "sparsetex_copy_amap.h"

#include "core/include/channel.h"
#include "sim/IChip.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"
#include "mdiag/utils/surfutil.h"

#include "maxwell/gm200/dev_ltc.h"
#include "maxwell/gm200/dev_fb.h"

#include "class/cl90b5.h" // GF100_DMA_COPY
#include "class/cla0b5.h" // KEPLER_DMA_COPY_A
#include "class/clb0b5.h" // MAXWELL_DMA_COPY_A
#include "class/cla06fsubch.h"

extern const ParamDecl SparseTexCopyAmap_params[] =
{
    PARAM_SUBDECL(FBTest::fbtest_common_params),
    UNSIGNED_PARAM("-mem_loc_buf0",      "specify the location of memory for r/w in buf0. Can be 0(vid), 1(peer), 2(coh), 3(ncoh))"),
    UNSIGNED_PARAM("-mem_loc_buf1",      "specify the location of memory for r/w in buf1. Can be 0(vid), 1(peer), 2(coh), 3(ncoh))"),
    UNSIGNED64_PARAM("-size",  "Size of testing buffer"),
    UNSIGNED_PARAM("-page_size_buf0", "page size of dmabuffer buf0"),
    UNSIGNED_PARAM("-page_size_buf1", "page size of dmabuffer buf1"),
    STRING_PARAM("-pte_kind_buf0", "pte_kind of dmabuffer buf0"),
    STRING_PARAM("-pte_kind_buf1", "pte_kind of dmabuffer buf1"),
    UNSIGNED64_PARAM("-test_phys_addr_src", "phys address hint of src"),
    UNSIGNED64_PARAM("-test_phys_addr_srcAlias", "phys address hint of srcAlias"),
    UNSIGNED64_PARAM("-test_phys_addr_dst", "phys address hint of dst"),
    UNSIGNED64_PARAM("-test_phys_addr_dstAlias", "phys address hint of dstAlias"),
    UNSIGNED64_PARAM("-size_check",  "amount of allocated surface to verify"),
    LAST_PARAM
};

//-mem_loc 0 -pte_kind_buf0 PITCH -page_size_buf0 4 -phys_addr_hint_buf0 0x210001000
SparseTexCopyAmapTest::SparseTexCopyAmapTest(ArgReader *params) :
    FBTest(params),
    m_LocalStatus(1),
    m_PhysAddrSrc(0),
    m_PhysAddrSrcAlias(0),
    m_PhysAddrDst(0),
    m_PhysAddrDstAlias(0),
    m_TargetBuf0(LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM),
    m_TargetBuf1(LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM),
    m_Size(4096),
    m_PageSizeBuf0(64),
    m_PageSizeBuf1(4),
    m_PteKindBuf0("GENERIC_16BX2"),
    m_PteKindBuf1("PITCH"),
    m_SizeCheck(4096)
{

    m_Size = params->ParamUnsigned64("-size", 4 * 1024);
    m_Size = m_Size & ~0x3; // Size must be 4B aligned

    m_PteKindBuf0 = params->ParamStr("-pte_kind_buf0", "GENERIC_16BX2");
    m_PteKindBuf1 = params->ParamStr("-pte_kind_buf1", "PITCH");

    m_PageSizeBuf0 = params->ParamUnsigned("-page_size_buf0", 64);
    MASSERT((m_PageSizeBuf0==4 || m_PageSizeBuf0==64 || m_PageSizeBuf0==128 || m_PageSizeBuf0==2048) && "Unsupported page size for page_size_buf0 !!!");

    m_PageSizeBuf1 = params->ParamUnsigned("-page_size_buf1", 4);
    MASSERT((m_PageSizeBuf1==4 || m_PageSizeBuf1==64 || m_PageSizeBuf1==128 || m_PageSizeBuf1==2048) && "Unsupported page size for page_size_buf1 !!!");

    m_TargetBuf0 = params->ParamUnsigned("-mem_loc_buf0", 0x0);
    m_TargetBuf1 = params->ParamUnsigned("-mem_loc_buf1", 0x0);

    m_PhysAddrSrc = params->ParamUnsigned64("-test_phys_addr_src", 0x0);
    m_PhysAddrSrcAlias = params->ParamUnsigned64("-test_phys_addr_src", 0x0);
    m_PhysAddrDst = params->ParamUnsigned64("-test_phys_addr_src", 0x0);
    m_PhysAddrDstAlias = params->ParamUnsigned64("-test_phys_addr_src", 0x0);
    m_SizeCheck = params->ParamUnsigned64("-size_check", 4 * 1024);

    m_DmaCopyAlloc.SetOldestSupportedClass(KEPLER_DMA_COPY_A);

    FBTest::DebugPrintArgs();
}

SparseTexCopyAmapTest::~SparseTexCopyAmapTest(void)
{
}

STD_TEST_FACTORY(SparseTexCopyAmap, SparseTexCopyAmapTest)

void SparseTexCopyAmapTest::SetSurfaceAperture(DmaBuffer &m_buffer, UINT32 m_target)
{
    switch (m_target) {
    case 0x0:
        m_buffer.SetLocation(Memory::Fb);
        break;
    case 0x2:
        m_buffer.SetLocation(Memory::Coherent);
        break;
    case 0x3:
        m_buffer.SetLocation(Memory::NonCoherent);
        break;
    default:
        ErrPrintf("Unsupported mem location for src buffer: %d, expected 0x0, 0x2, 0x3.\n", m_buffer.GetLocation());
        MASSERT(0 && "Unsupported aperture");
        break;
    }
    if(m_EnablePrint>0)
        InfoPrintf("SparseTexCopyAmapTest: Set surface memory location to %d\n", m_target);
}

int SparseTexCopyAmapTest::Setup() {
    /*
      mods/mdiag/lwgpu.h contains the LWGpuResource class, which is the class that lwgpu points to.
      That class has a method called GetGPuDevice which returns the GpuDevice for channel/object/FB alloc or control calls.
      GpuDevice class is defined in mods/gpu/include/gpudev.h

      this test is derived from FBTest class. The FBTest class is implemented in fb_common.cpp and already contains
      pointers to lwgpu and gpuSubDev

    */

    m_LocalStatus = 1;
    InfoPrintf("SparseTexCopyAmapTest::Setup() starts\n");
    getStateReport()->enable();

    if (!FBTest::Setup()) {
        ErrPrintf("SparseTexCopyAmapTest::Setup() Fail\n");
        return 0;
    }

    if( m_Size>(2*1024*1024)) {
        InfoPrintf("SparseTexCopyAmapTest:: setting size to 2MB if it is more than 2MB\n");
        m_Size = 2*1024*1024;
    }

    static const UINT32 MAXW = 1024;

    if (m_Size <= MAXW) {
        m_Width = m_Size, m_Height = 1;
    }
    else {
        m_Width = MAXW, m_Height = m_Size / MAXW;
    }
    m_Size = m_Width * m_Height;

    if(m_SizeCheck < 4){
        InfoPrintf("SparseTexCopyAmapTest:: size_check at least need to be 4.\n");
        m_SizeCheck = 4;
    }

    uint32 allocSize = m_Size;
    if( (allocSize < (m_PageSizeBuf0 * 1024)) || (allocSize < (m_PageSizeBuf1 * 1024))){
        //Allocate at least a page; bigger of the two page sizes.
        allocSize = (m_PageSizeBuf0 >= m_PageSizeBuf1) ? (m_PageSizeBuf0 * 1024) : (m_PageSizeBuf1 * 1024);
    }
    InfoPrintf("SparseTexCopyAmapTest:: setting surface size %d for allocation alignment purpose but will access %d and check %d\n",allocSize, m_Size, m_SizeCheck);
    m_SrcBuffer.SetArrayPitch(allocSize);
    m_SrcAliasBuffer.SetArrayPitch(allocSize);
    m_DstBuffer.SetArrayPitch(allocSize);
    m_DstAliasBuffer.SetArrayPitch(allocSize);

    InfoPrintf("SparseTexCopyAmapTest:: setting color format\n");
    m_SrcBuffer.SetColorFormat(ColorUtils::Y8);
    m_SrcAliasBuffer.SetColorFormat(ColorUtils::Y8);
    m_DstBuffer.SetColorFormat(ColorUtils::Y8);
    m_DstAliasBuffer.SetColorFormat(ColorUtils::Y8);

    InfoPrintf("SparseTexCopyAmapTest:: setting force alloc size\n");
    m_SrcBuffer.SetForceSizeAlloc(true);
    m_SrcAliasBuffer.SetForceSizeAlloc(true);
    m_DstBuffer.SetForceSizeAlloc(true);
    m_DstAliasBuffer.SetForceSizeAlloc(true);

    InfoPrintf("SparseTexCopyAmapTest:: setting RW permission\n");
    m_SrcBuffer.SetProtect(Memory::ReadWrite);
    m_SrcAliasBuffer.SetProtect(Memory::ReadWrite);
    m_DstBuffer.SetProtect(Memory::ReadWrite);
    m_DstAliasBuffer.SetProtect(Memory::ReadWrite);

    InfoPrintf("SparseTexCopyAmapTest:: setting address hint 0x%llx  0x%llx  0x%llx  0x%llx\n", m_PhysAddrSrc, m_PhysAddrSrcAlias, m_PhysAddrDst, m_PhysAddrDstAlias);
    if (m_PhysAddrSrc > 0)
        m_SrcBuffer.SetFixedPhysAddr(m_PhysAddrSrc);
    if (m_PhysAddrSrcAlias > 0)
        m_SrcAliasBuffer.SetFixedPhysAddr(m_PhysAddrSrcAlias);
    if (m_PhysAddrDst > 0)
        m_DstBuffer.SetFixedPhysAddr(m_PhysAddrDst);
    if (m_PhysAddrDstAlias > 0)
        m_DstAliasBuffer.SetFixedPhysAddr(m_PhysAddrDstAlias);

    InfoPrintf("SparseTexCopyAmapTest:: setting contig alloc\n");
    m_SrcBuffer.SetPhysContig(true);
    m_SrcAliasBuffer.SetPhysContig(true);
    m_DstBuffer.SetPhysContig(true);
    m_DstAliasBuffer.SetPhysContig(true);

    InfoPrintf("SparseTexCopyAmapTest:: setting page size %d and %d\n", m_PageSizeBuf0, m_PageSizeBuf1);
    m_SrcBuffer.SetPageSize(m_PageSizeBuf0);
    m_SrcAliasBuffer.SetPageSize(m_PageSizeBuf1);
    m_DstBuffer.SetPageSize(m_PageSizeBuf0);
    m_DstAliasBuffer.SetPageSize(m_PageSizeBuf1);

    InfoPrintf("SparseTexCopyAmapTest:: setting kind buf0 to src %s\n", m_PteKindBuf0.c_str());
    if (OK != SetPteKind(m_SrcBuffer, m_PteKindBuf0, lwgpu->GetGpuDevice())) {
        return 1;
    }
    InfoPrintf("SparseTexCopyAmapTest:: setting kind buf1 to srcAlias %s\n", m_PteKindBuf1.c_str());
    if (OK != SetPteKind(m_SrcAliasBuffer, m_PteKindBuf1, lwgpu->GetGpuDevice())) {
        return 1;
    }
    InfoPrintf("SparseTexCopyAmapTest:: setting kind buf0 to dest %s\n", m_PteKindBuf0.c_str());
    if (OK != SetPteKind(m_DstBuffer, m_PteKindBuf0, lwgpu->GetGpuDevice())) {
        return 1;
    }
    InfoPrintf("SparseTexCopyAmapTest:: setting kind buf1 to destAlias %s\n", m_PteKindBuf1.c_str());
    if (OK != SetPteKind(m_DstAliasBuffer, m_PteKindBuf1, lwgpu->GetGpuDevice())) {
        return 1;
    }

    InfoPrintf("SparseTexCopyAmapTest:: setting aperture %d %d\n",m_TargetBuf0, m_TargetBuf1);

    if(!lwgpu->GetGpuSubdevice()->FbHeapSize() && (!m_TargetBuf0 || !m_TargetBuf1))
    {
        ErrPrintf("This is Zero FB chip, please use -mem_loc_buf0/2 <2/3> to change buffer aperture.\n");
        MASSERT(0 && "Chip has no FB!");
    }

    SetSurfaceAperture( m_SrcBuffer, m_TargetBuf0);
    SetSurfaceAperture( m_SrcAliasBuffer, m_TargetBuf1);
    SetSurfaceAperture( m_DstBuffer, m_TargetBuf0);
    SetSurfaceAperture( m_DstAliasBuffer, m_TargetBuf1);

    // Allocate larger buffers first to force the surfaces with the small page size to be aligned to bigger page size,
    // and ensure that all surfaces are on different pages (see bug 200035971#47)
    if (m_PageSizeBuf0 > m_PageSizeBuf1) {
        InfoPrintf("SparseTexCopyAmapTest:: Alloc src buffer\n");
        if (OK != m_SrcBuffer.Alloc(lwgpu->GetGpuDevice())) {
            ErrPrintf("can't create src buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 1;
        }
        InfoPrintf("SparseTexCopyAmapTest:: Map src buffer\n");
        if (OK != m_SrcBuffer.Map()) {
            ErrPrintf("can't map the src buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Alloc srcAlias buffer\n");
        if (OK != m_SrcAliasBuffer.Alloc(lwgpu->GetGpuDevice())) {
            ErrPrintf("can't create srcAlias buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Map srcAlias buffer\n");
        if (OK != m_SrcAliasBuffer.Map()) {
            ErrPrintf("can't map the srcAlias buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Alloc dest buffer\n");
        if (OK != m_DstBuffer.Alloc(lwgpu->GetGpuDevice())) {
            ErrPrintf("can't create dest buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Map dest buffer\n");
        if (OK != m_DstBuffer.Map()) {
            ErrPrintf("can't map the dest buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Alloc destAlias buffer\n");
        if (OK != m_DstAliasBuffer.Alloc(lwgpu->GetGpuDevice())) {
            ErrPrintf("can't create destAlias buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Map destAlias buffer\n");
        if (OK != m_DstAliasBuffer.Map()) {
            ErrPrintf("can't map the destAlias buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            return 1;
        }
    }
    else {
        InfoPrintf("SparseTexCopyAmapTest:: Alloc srcAlias buffer\n");
        if (OK != m_SrcAliasBuffer.Alloc(lwgpu->GetGpuDevice())) {
            ErrPrintf("can't create srcAlias buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Map srcAlias buffer\n");
        if (OK != m_SrcAliasBuffer.Map()) {
            ErrPrintf("can't map the srcAlias buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Alloc src buffer\n");
        if (OK != m_SrcBuffer.Alloc(lwgpu->GetGpuDevice())) {
            ErrPrintf("can't create src buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 1;
        }
        InfoPrintf("SparseTexCopyAmapTest:: Map src buffer\n");
        if (OK != m_SrcBuffer.Map()) {
            ErrPrintf("can't map the src buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Alloc destAlias buffer\n");
        if (OK != m_DstAliasBuffer.Alloc(lwgpu->GetGpuDevice())) {
            ErrPrintf("can't create destAlias buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Map destAlias buffer\n");
        if (OK != m_DstAliasBuffer.Map()) {
            ErrPrintf("can't map the destAlias buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Alloc dest buffer\n");
        if (OK != m_DstBuffer.Alloc(lwgpu->GetGpuDevice())) {
            ErrPrintf("can't create dest buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 1;
        }

        InfoPrintf("SparseTexCopyAmapTest:: Map dest buffer\n");
        if (OK != m_DstBuffer.Map()) {
            ErrPrintf("can't map the dest buffer, line %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            return 1;
        }
    }

    m_OffsetSrc = m_SrcBuffer.GetExtraAllocSize();
    m_OffsetSrcAlias = m_SrcAliasBuffer.GetExtraAllocSize();
    m_OffsetDest = m_DstBuffer.GetExtraAllocSize();
    m_OffsetDestAlias = m_DstAliasBuffer.GetExtraAllocSize();

    InfoPrintf("srcOffset=0x%llx destOffset=0x%llx\n",  m_OffsetSrc, m_OffsetDest);
    InfoPrintf("srcAliasOffset=0x%llx destAliasOffset=0x%llx\n",  m_OffsetSrcAlias, m_OffsetDestAlias);

    InfoPrintf("SparseTexCopyAmapTest::Create src buffer at 0x%llx\n", GetPhysAddress(m_SrcBuffer));
    InfoPrintf("SparseTexCopyAmapTest::Create srcAlias buffer at 0x%llx\n", GetPhysAddress(m_SrcAliasBuffer));
    InfoPrintf("SparseTexCopyAmapTest::Create dest buffer at 0x%llx\n", GetPhysAddress(m_DstBuffer));
    InfoPrintf("SparseTexCopyAmapTest::Create destAlias buffer at 0x%llx\n", GetPhysAddress(m_DstAliasBuffer));

    m_RefData = new UINT32[m_Size/4];
    m_RefData2 = new UINT32[m_Size/4];
    m_RefData3 = new UINT32[m_Size/4];

    if (!chHelper->acquire_channel() ) {
        ErrPrintf("Channel Create failed\n");
        return 1;
    }

    ch = chHelper->channel();
    MASSERT(ch && "Getting channel failed!");

    GpuSubdevice* pGpuSubdevice = lwgpu->GetGpuSubdevice();
    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;
    if (pGpuSubdevice->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        vector<UINT32> supportedCEs;
        if (OK != lwgpu->GetSupportedCeNum(pGpuSubdevice, &supportedCEs, LwRmPtr().Get()))
        {
            ErrPrintf("Cannot get Supported CE engines\n");
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 1;
        }
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

    // if unsuccessful, clean up and exit
    if (!m_LocalStatus) {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with m_LocalStatus 0x%x\n",m_LocalStatus);
        CleanUp();
        return 0;
    }
    return 1;
}

void SparseTexCopyAmapTest::CleanUp()
{
    DebugPrintf("SparseTexCopyAmapTest::CleanUp() starts\n");

    if (m_SrcBuffer.GetMemHandle()) {
        m_SrcBuffer.Unmap();
        m_SrcBuffer.Free();
    }

    if (m_SrcAliasBuffer.GetMemHandle()) {
        m_SrcAliasBuffer.Unmap();
        m_SrcAliasBuffer.Free();
    }

    if (m_DstBuffer.GetMemHandle()) {
        m_DstBuffer.Unmap();
        m_DstBuffer.Free();
    }

    if (m_DstAliasBuffer.GetMemHandle()) {
        m_DstAliasBuffer.Unmap();
        m_DstAliasBuffer.Free();
    }

    m_DmaCopyAlloc.Free();

    delete[] m_RefData;
    delete[] m_RefData2;
    delete[] m_RefData3;

    FBTest::CleanUp();
}

void SparseTexCopyAmapTest::Run(void)
{
    SetStatus(Test::TEST_INCOMPLETE);
    InfoPrintf("SparseTexCopyAmapTest::Run() starts\n");

    m_LocalStatus = 1;

    InitBuffer();
    fbHelper->SetBackdoor(false);
    ReadWriteTestUsingBar1CePte();

    if (m_LocalStatus) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    }
    else {
        getStateReport()->crcCheckFailed();
        SetStatus(Test::TEST_FAILED_CRC);
    }

    return;
}

void SparseTexCopyAmapTest::InitBuffer()
{
    for (UINT32 i = 0; i < m_Size; i+=4) {
        m_RefData[i/4] = 0xAA000000 + i/4;
        m_RefData2[i/4] = m_RefData[i/4]+1;
        m_RefData3[i/4] = m_RefData[i/4]+2;
    }
}

void SparseTexCopyAmapTest::FillPteMemParamsFlagsAperture(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS &params, const DmaBuffer &m_buffer) {
    Memory::Location bufferLocation = m_buffer.GetLocation();

    switch(bufferLocation)
    {
        case Memory::Fb:
            params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _APERTURE, _VIDEO_MEMORY, params.flags);
            break;
        case Memory::Coherent:
            params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _APERTURE, _SYSTEM_COHERENT_MEMORY, params.flags);
            break;
        case Memory::NonCoherent:
            params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _APERTURE, _SYSTEM_NON_COHERENT_MEMORY, params.flags);
            break;
        default:
            ErrPrintf("Unsupported mem location for buffer: %d, expected 0x0, 0x2, 0x3.\n", bufferLocation);
            MASSERT(0 && "Unsupported aperture");
            break;
    }
}

UINT64 SparseTexCopyAmapTest::GetPhysAddress(const DmaBuffer &m_buffer)
{
    Memory::Location bufferLocation = m_buffer.GetLocation();
    UINT64 address = 0;

    if(Memory::Fb == bufferLocation)
    {
        address = m_buffer.GetVidMemOffset();
    }
    else if(Memory::Coherent == bufferLocation || Memory::NonCoherent == bufferLocation)
    {
        address = m_buffer.GetPhysAddress();
    }
    else
    {
        ErrPrintf("Unsupported mem location for buffer: %d, expected 0x0, 0x2, 0x3.\n", bufferLocation);
        MASSERT(0 && "Unsupported aperture");
    }

    return address;
}

RC SparseTexCopyAmapTest::ReadWriteTestUsingBar1CePte()
{
    int status = 1;
    RC      rc;

    InfoPrintf("SparseTexCopyAmapTest: ReadWriteTestUsingBar1CePte\n");

    LwRmPtr pLwRm;
    GpuDevice *pGpuDevice = m_SrcAliasBuffer.GetGpuDev();

    UINT32 pageCount_buf0 = (UINT32)((m_Size>>10)/m_PageSizeBuf0);
    if( ((pageCount_buf0<<10)*m_PageSizeBuf0) < m_Size) pageCount_buf0++; //ceil

    UINT32 pageCount_buf1 = (UINT32)((m_Size>>10)/m_PageSizeBuf1);
    if( ((pageCount_buf1<<10)*m_PageSizeBuf1) < m_Size) pageCount_buf1++; //ceil

    UINT64 srcStartPA = GetPhysAddress(m_SrcBuffer) + m_OffsetSrc;
    UINT64 destStartPA = GetPhysAddress(m_DstBuffer) + m_OffsetDest;
    UINT64 srcAliasStartPA = GetPhysAddress(m_SrcAliasBuffer) + m_OffsetSrcAlias;
    UINT64 destAliasStartPA = GetPhysAddress(m_DstAliasBuffer) + m_OffsetDestAlias;

    PrintDmaBufferParams(m_SrcBuffer);
    PrintDmaBufferParams(m_SrcAliasBuffer);
    PrintDmaBufferParams(m_DstBuffer);
    PrintDmaBufferParams(m_DstAliasBuffer);

    InfoPrintf("srcPhysAddr=0x%llx destPhysAddr=0x%llx\n ",  srcStartPA, destStartPA);
    InfoPrintf("srcAliasPhysAddr=0x%llx destAliasPhysAddr=0x%llx\n",  srcAliasStartPA, destAliasStartPA);
    InfoPrintf("srcNumPages=0x%x srcAliasNumPages=0x%x\n ", pageCount_buf0, pageCount_buf1);

    uintptr_t srcPtr = reinterpret_cast<uintptr_t>(m_SrcBuffer.GetAddress());
    uintptr_t srcAliasPtr = reinterpret_cast<uintptr_t>(m_SrcAliasBuffer.GetAddress());
    uintptr_t destPtr = reinterpret_cast<uintptr_t>(m_DstBuffer.GetAddress());
    uintptr_t destAliasPtr = reinterpret_cast<uintptr_t>(m_DstAliasBuffer.GetAddress());

    //Write reference data into source buffer
    InfoPrintf("SparseTexCopyAmapTest::writing src, srcAlias, dest, and destAlias at physSrc=0x%llx PhysDest=0x%llx physSrcAlias=0x%llx PhysDestAlias=0x%llx\n",
                srcStartPA, destStartPA, srcAliasStartPA, destAliasStartPA);

    for (UINT64 addr = 0; addr < m_Size; addr += 4) {
        UINT32 refData = m_RefData[addr/4];
        MEM_WR32(srcPtr+addr, refData);
        MEM_WR32(srcAliasPtr+addr, 0xBB000000 + (addr/4));
        MEM_WR32(destPtr+addr, 0xCC000000 + (addr/4));
        MEM_WR32(destAliasPtr+addr, 0xDD000000 + (addr/4));
    }

    Platform::FlushCpuWriteCombineBuffer();

    InfoPrintf("SparseTexCopyAmapTest::reading src (RAW) at virt=0x%llx phys=0x%llx\n", srcPtr, srcStartPA);
    for (UINT64 addr = 0; addr < m_SizeCheck; addr += 4) {
        UINT32 refData = m_RefData[addr/4];
        UINT32 readData = refData + 1;
        readData = MEM_RD32(srcPtr+addr);
        if (refData != readData) {
            ErrPrintf("SparseTexCopyAmapTest: Error comparing source-write rd(bar0)/wr(bar1) at 0x%llx, read=%x != ref=%x\n", srcStartPA + addr, readData, refData);
            status = 0;
            break;
        }
    }

    if (gpuSubDev->IsSOC())
        FlushIlwalidateL2Cache();

    InfoPrintf("SparseTexCopyAmapTest::Changing PTE entries of aliased surfaces\n");

    UINT32 aliasPageSize = m_PageSizeBuf1 * 1024;
    //Change Physical mapping of m_SrcAliasBuffer to m_SrcBuffer and m_DstAliasBuffer to m_DstBuffer
    LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS params = {0};
    params.pageCount = pageCount_buf1;
    params.pageArray = (LwP64)&srcStartPA;
    params.pageSize = (LwU32)aliasPageSize;
    params.gpuAddr = (LwU64)m_SrcAliasBuffer.GetCtxDmaOffsetGpu();
    params.hwResource.hClient = pLwRm->GetClientHandle();
    params.hwResource.hDevice = pLwRm->GetDeviceHandle(pGpuDevice);
    params.hwResource.hMemory = m_SrcAliasBuffer.GetMemHandle();
    params.offset =  0;
    params.pteMem = 0;
    params.hSrcVASpace = 0;
    params.hTgtVASpace = 0;

    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _CONTIGUOUS, _TRUE, params.flags);
    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _TRUE, params.flags);
    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _OVERRIDE_PAGE_SIZE, _TRUE, params.flags);

    FillPteMemParamsFlagsAperture(params, m_SrcAliasBuffer);

    CHECK_RC(pLwRm->ControlByDevice(m_SrcAliasBuffer.GetGpuDev(), LW0080_CTRL_CMD_DMA_FILL_PTE_MEM, &params, sizeof(params)));
    InfoPrintf("SparseTexCopyAmapTest::Done src aliasing\n");

    memset(&params, 0, sizeof(params));
    params.pageCount = pageCount_buf1;
    params.pageArray = (LwP64)&destStartPA;
    params.pageSize = (LwU32)aliasPageSize;
    params.gpuAddr = (LwU64)m_DstAliasBuffer.GetCtxDmaOffsetGpu();
    params.hwResource.hClient = pLwRm->GetClientHandle();
    params.hwResource.hDevice = pLwRm->GetDeviceHandle(pGpuDevice);
    params.hwResource.hMemory = m_DstAliasBuffer.GetMemHandle();
    params.offset =  0;
    params.pteMem = 0;
    params.hSrcVASpace = 0;
    params.hTgtVASpace = 0;

    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _CONTIGUOUS, _TRUE, params.flags);
    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _TRUE, params.flags);
    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _OVERRIDE_PAGE_SIZE, _TRUE, params.flags);

    FillPteMemParamsFlagsAperture(params, m_DstAliasBuffer);

    CHECK_RC(pLwRm->ControlByDevice(m_SrcAliasBuffer.GetGpuDev(), LW0080_CTRL_CMD_DMA_FILL_PTE_MEM, &params, sizeof(params)));
    InfoPrintf("SparseTexCopyAmapTest::Done dest aliasing\n");

    //Ilwalidate TLBs
    lwgpu->RegWr32( LW_PFB_PRI_MMU_ILWALIDATE,
                    DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _ALL_VA, _TRUE) |
                    DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _ALL_PDB, _TRUE) |
                    DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _HUBTLB_ONLY, _TRUE) |
                    DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _TRIGGER, _TRUE));

    InfoPrintf("SparseTexCopyAmapTest::Done TLB ilwalidation\n");
    UINT32 hChipClass;

    //copy the surface from src to dest by using srcAlias and destAlias
    CHECK_RC(m_DmaCopyAlloc.Alloc(ch->ChannelHandle(), lwgpu->GetGpuDevice()));
    hChipClass = m_DmaCopyAlloc.GetHandle();
    MASSERT(hChipClass);
    m_SubChannel = LWA06F_SUBCHANNEL_COPY_ENGINE;
    ch->SetObject(m_SubChannel, hChipClass);
    InfoPrintf("Subchannel %d set\n", m_SubChannel);

    UINT64 srcVA = m_SrcAliasBuffer.GetCtxDmaOffsetGpu();
    UINT64 destVA = m_DstAliasBuffer.GetCtxDmaOffsetGpu();

    InfoPrintf("Copying from src mem to dst mem...\n");
    ch->MethodWrite(m_SubChannel, LW90B5_OFFSET_IN_UPPER, (UINT32)(srcVA >> 32));
    ch->MethodWrite(m_SubChannel, LW90B5_OFFSET_IN_LOWER, (UINT32)(srcVA & 0xFFFFFFFF));
    ch->MethodWrite(m_SubChannel, LW90B5_LINE_LENGTH_IN, m_Width);
    ch->MethodWrite(m_SubChannel, LW90B5_PITCH_IN, m_Width);
    ch->MethodWrite(m_SubChannel, LW90B5_OFFSET_OUT_UPPER, (UINT32)(destVA >> 32));
    ch->MethodWrite(m_SubChannel, LW90B5_OFFSET_OUT_LOWER, (UINT32)(destVA & 0xFFFFFFFF));
    ch->MethodWrite(m_SubChannel, LW90B5_PITCH_OUT, m_Width);
    ch->MethodWrite(m_SubChannel, LW90B5_LINE_COUNT, m_Height);
    ch->MethodWrite(m_SubChannel, LW90B5_LAUNCH_DMA, 0x201 | 0x80 | 0x100);

    ch->MethodFlush();
    ch->WaitForIdle();

    if (gpuSubDev->IsSOC())
        FlushIlwalidateL2Cache();

    //Restore Physical mapping of m_SrcAliasBuffer and m_DstAliasBuffer to original values
    memset(&params, 0, sizeof(params));
    params.pageCount = pageCount_buf1;
    params.pageArray = (LwP64)&srcAliasStartPA;
    params.pageSize = (LwU32)aliasPageSize;
    params.gpuAddr = (LwU64)m_SrcAliasBuffer.GetCtxDmaOffsetGpu();
    params.hwResource.hClient = pLwRm->GetClientHandle();
    params.hwResource.hDevice = pLwRm->GetDeviceHandle(pGpuDevice);
    params.hwResource.hMemory = m_SrcAliasBuffer.GetMemHandle();
    params.offset =  0;
    params.pteMem = 0;
    params.hSrcVASpace = 0;
    params.hTgtVASpace = 0;

    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _CONTIGUOUS, _TRUE, params.flags);
    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _TRUE, params.flags);
    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _OVERRIDE_PAGE_SIZE, _TRUE, params.flags);

    FillPteMemParamsFlagsAperture(params, m_SrcAliasBuffer);

    CHECK_RC(pLwRm->ControlByDevice(m_SrcAliasBuffer.GetGpuDev(), LW0080_CTRL_CMD_DMA_FILL_PTE_MEM, &params, sizeof(params)));

    memset(&params, 0, sizeof(params));
    params.pageCount = pageCount_buf1;
    params.pageArray = (LwP64)&destAliasStartPA;
    params.pageSize = (LwU32)aliasPageSize;
    params.gpuAddr = (LwU64)m_DstAliasBuffer.GetCtxDmaOffsetGpu();
    params.hwResource.hClient = pLwRm->GetClientHandle();
    params.hwResource.hDevice = pLwRm->GetDeviceHandle(pGpuDevice);
    params.hwResource.hMemory = m_DstAliasBuffer.GetMemHandle();
    params.offset =  0;
    params.pteMem = 0;
    params.hSrcVASpace = 0;
    params.hTgtVASpace = 0;

    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _CONTIGUOUS, _TRUE, params.flags);
    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _TRUE, params.flags);
    params.flags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _OVERRIDE_PAGE_SIZE, _TRUE, params.flags);

    FillPteMemParamsFlagsAperture(params, m_DstAliasBuffer);

    CHECK_RC(pLwRm->ControlByDevice(m_SrcAliasBuffer.GetGpuDev(), LW0080_CTRL_CMD_DMA_FILL_PTE_MEM, &params, sizeof(params)));

    //Ilwalidate TLBs
    lwgpu->RegWr32( LW_PFB_PRI_MMU_ILWALIDATE,
                    DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _ALL_VA, _TRUE) |
                    DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _ALL_PDB, _TRUE) |
                    DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _HUBTLB_ONLY, _TRUE) |
                    DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _TRIGGER, _TRUE));

    if (gpuSubDev->IsSOC())
        FlushIlwalidateL2Cache();

    UINT64 addr;
    // Still holding on the following code that we might use in future for extra sanitization. Commenting now for speedup the test exelwtion.

    /*
    //ReadData by using aliased surface and write into dest surface
    InfoPrintf("SparseTexCopyAmapTest: reading source alias surface ...cpuPhysAddr=0x%llx\n", m_SrcAliasBuffer.GetVidMemOffset());
    InfoPrintf("SparseTexCopyAmapTest: ...cpuVirtAddress=0x%llx\n",  m_SrcAliasBuffer.GetAddress());
    InfoPrintf("SparseTexCopyAmapTest: ...gpuPhysAddress=0x%llx\n",  m_SrcAliasBuffer.GetPhysAddress());
    InfoPrintf("SparseTexCopyAmapTest: ...gpuVirtAddress=0x%llx\n",  m_SrcAliasBuffer.GetCtxDmaOffsetGpu());
    for (addr = 0; addr < m_SizeCheck; addr += 4) {
        m_RefData2[addr/4] = MEM_RD32(srcAliasPtr + addr);
        if (m_RefData[addr/4] == m_RefData2[addr/4]) {
            ErrPrintf("SparseTexCopyAmapTest: Error comparing (expect differnt value) dest at 0x%llx, read=0x%x != ref=0x%x\n",
                      m_SrcAliasBuffer.GetVidMemOffset() + addr + m_OffsetSrcAlias, m_RefData2[addr/4], m_RefData[addr/4]);
            status = 0;
            break;
        }
    }

    //Reading dest alias buffer into ref3 data
    InfoPrintf("SparseTexCopyAmapTest: reading dest alias surface ...cpuPhysAddr=0x%llx\n", m_DstAliasBuffer.GetVidMemOffset());
    InfoPrintf("SparseTexCopyAmapTest: ...cpuVirtAddress=0x%llx\n",  m_DstAliasBuffer.GetAddress());
    InfoPrintf("SparseTexCopyAmapTest: ...gpuPhysAddress=0x%llx\n",  m_DstAliasBuffer.GetPhysAddress());
    InfoPrintf("SparseTexCopyAmapTest: ...gpuVirtAddress=0x%llx\n",  m_DstAliasBuffer.GetCtxDmaOffsetGpu());
    for (addr = 0; addr < m_SizeCheck; addr += 4) {
        m_RefData3[addr/4] = MEM_RD32(destAliasPtr + addr);
        if (m_RefData[addr/4] == m_RefData3[addr/4]) {
            ErrPrintf("SparseTexCopyAmapTest: Error comparing (expect differnt value) dest at 0x%llx, read=0x%x != ref=0x%x\n",
                      m_DstAliasBuffer.GetVidMemOffset() + addr + m_OffsetDestAlias, m_RefData3[addr/4], m_RefData[addr/4]);
            status = 0;
            break;
        }
    }
    */

    //ReadData from dest surface which was already modified (copied into) by copy engine
    InfoPrintf("SparseTexCopyAmapTest: reading dest surface ...PhysAddr=0x%llx\n", GetPhysAddress(m_DstBuffer));
    for (addr = 0; addr < m_SizeCheck; addr += 4) {
        m_RefData2[addr/4] = MEM_RD32(destPtr + addr);
        if (m_RefData[addr/4] != m_RefData2[addr/4]) {
            ErrPrintf("SparseTexCopyAmapTest: Error comparing dest at 0x%llx, read=%x != ref=%x\n",
                     destStartPA + addr, m_RefData2[addr/4], m_RefData[addr/4]);
            status = 0;
            //break;
        }
    }

    if (status == 0) {
        ErrPrintf("SparseTexCopyAmapTest: test FAILED\n");
        m_LocalStatus = 0;
    }
    else {
        InfoPrintf("SparseTexCopyAmapTest: test PASSED\n");
    }
    return rc;
}

void SparseTexCopyAmapTest::FlushIlwalidateL2Cache(){

    unsigned flushStatus, numLTC;
    numLTC = lwgpu->RegRd32(LW_PLTCG_LTC0_LTS0_CBC_NUM_ACTIVE_LTCS);
    InfoPrintf("SparseTexCopyAmapTest: Flushing L2... numLTCs=%d\n",numLTC);

    lwgpu->RegWr32(LW_PLTCG_LTCS_LTSS_TSTG_CMGMT_1, 0x60000301);

    if(numLTC>0){
#ifdef LW_PLTCG_LTC0_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC0_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>1){
#ifdef LW_PLTCG_LTC1_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC1_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }

    if(numLTC>2){
#ifdef LW_PLTCG_LTC2_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC2_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>3){
#ifdef LW_PLTCG_LTC3_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC3_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>4){
#ifdef LW_PLTCG_LTC4_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC4_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>5){
#ifdef LW_PLTCG_LTC5_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC5_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>6){
#ifdef LW_PLTCG_LTC6_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC6_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>7){
#ifdef LW_PLTCG_LTC7_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC7_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>8){
#ifdef LW_PLTCG_LTC8_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC8_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>9){
#ifdef LW_PLTCG_LTC9_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC9_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>10){
#ifdef LW_PLTCG_LTC10_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC10_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>11){
#ifdef LW_PLTCG_LTC11_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC11_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>12){
#ifdef LW_PLTCG_LTC12_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC12_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>13){
#ifdef LW_PLTCG_LTC13_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC13_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>14){
#ifdef LW_PLTCG_LTC14_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC14_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>15){
#ifdef LW_PLTCG_LTC15_LTSS_TSTG_CMGMT_1
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC15_LTSS_TSTG_CMGMT_1);
        } while (flushStatus & 0x1);
#endif
    }

    lwgpu->RegWr32(LW_PLTCG_LTCS_LTSS_TSTG_CMGMT_0, 0x60000301);

    if(numLTC>0){
#ifdef LW_PLTCG_LTC0_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC0_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>1){
#ifdef LW_PLTCG_LTC1_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC1_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }

    if(numLTC>2){
#ifdef LW_PLTCG_LTC2_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC2_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>3){
#ifdef LW_PLTCG_LTC3_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC3_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>4){
#ifdef LW_PLTCG_LTC4_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC4_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>5){
#ifdef LW_PLTCG_LTC5_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC5_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>6){
#ifdef LW_PLTCG_LTC6_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC6_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>7){
#ifdef LW_PLTCG_LTC7_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC7_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>8){
#ifdef LW_PLTCG_LTC8_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC8_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>9){
#ifdef LW_PLTCG_LTC9_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC9_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>10){
#ifdef LW_PLTCG_LTC10_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC10_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>11){
#ifdef LW_PLTCG_LTC11_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC11_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>12){
#ifdef LW_PLTCG_LTC12_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC12_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>13){
#ifdef LW_PLTCG_LTC13_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC13_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>14){
#ifdef LW_PLTCG_LTC14_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC14_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }
    if(numLTC>15){
#ifdef LW_PLTCG_LTC15_LTSS_TSTG_CMGMT_0
        do {
            flushStatus = lwgpu->RegRd32(LW_PLTCG_LTC15_LTSS_TSTG_CMGMT_0);
        } while (flushStatus & 0x1);
#endif
    }

    InfoPrintf("SparseTexCopyAmapTest::Done L2 Clean+Invalid\n");

}
