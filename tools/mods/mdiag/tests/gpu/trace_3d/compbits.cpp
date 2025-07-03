/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/rc.h"
#include "core/include/memtypes.h"
#include "core/include/argread.h"
#include "gpu/include/gpudev.h"
#include "mdiag/utils/utils.h"
#include "core/include/utility.h"
#include "tracechan.h"
#include "tracesubchan.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"
#include "class/cl906f.h"
#include "class/cla0b5.h"
#include "class/cla06f.h"
#include "class/cla16f.h"
#include "class/clb06f.h"
#include "class/clc06f.h"
#include "class/clc36f.h"
#include "class/clc46f.h"
#include "class/clc3b5.h"
#include "class/clc56f.h"       // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h"       // HOPPER_CHANNEL_GPFIFO_A
#include "ctrl/ctrl2080/ctrl2080fb.h"
#include "Lwcm.h"

#include "compbits.h"
#include "mdiag/utils/lwgpu_classes.h"

///////////////////////////////////////////////////

CompBitsTest::CompBitsTest(TraceSubChannel* pSubChan)
    :
    m_pCBCLib(0),
    m_Initialized(false),
    m_BoundGpuDevice(0),
    m_Channel(0),
    m_pTraceSubch(pSubChan),
    m_Params(0),
    m_TimeoutMs(0),
    m_BackingStoreCpuAddress(0),
    m_pSurfCA(0),
    m_ID(CompBits::tidUndefined)
{
    m_Settings.SetTRCChipFamily(CompBits::GetTRCChipFamily(pSubChan));
}

RC CompBitsTest::SetArgs
(
    MdiagSurf* pSurfCA,
    GpuDevice* pGpuDev,
    TraceChannel* pTraceChan,
    const ArgReader* pParam,
    FLOAT64 timeoutMs
)
{
    RC rc;

    m_pSurfCA = pSurfCA;
    MASSERT(m_pSurfCA != 0);
    m_BoundGpuDevice = pGpuDev;
    m_Channel = pTraceChan;
    m_Params = pParam;
    m_TimeoutMs = timeoutMs;
    CHECK_RC(m_Settings.Init(pTraceChan->GetGpuResource(), m_Channel->GetLwRmPtr()));
    m_Settings.SetTestName(TestName());
    CHECK_RC(m_pCBCLib->Init());

    return rc;
}

CompBitsTest::~CompBitsTest()
{
    CleanUp();
}

void CompBitsTest::CleanUp()
{
    if (m_pCBCLib!= 0)
    {
        delete m_pCBCLib;
        m_pCBCLib = 0;
    }
    m_BackingStore.reset(0);
    m_Initialized = false;
}

RC CompBitsTest::SetupParam()
{
    DebugPrintf("CBC: CompBitsTest default SetupParam.\n");
    return OK;
}

RC CompBitsTest::DoPostRender()
{
    DebugPrintf("CBC: CompBitsTest default DoPostRender.\n");
    return OK;
}

RC CompBitsTest::InitBackingStore()
{
    RC rc;

    DebugPrintf("CBC: CompBitsTest::InitBackingStore - BEGIN\n");

    if (m_Initialized)
    {
        return rc;
    }

    // Cleanup if failed
    Utility::CleanupOnReturn<CompBitsTest> cleanup(this, &CompBitsTest::CleanUp);

    // Sanity check
    //
    if (m_BoundGpuDevice == NULL ||
        m_pTraceSubch == NULL ||
        m_BoundGpuDevice->GetNumSubdevices() != 1)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (!EngineClasses::IsGpuFamilyClassOrLater(
            m_pTraceSubch->GetClassNum(), LWGpuClasses::GPU_CLASS_KEPLER) ||
        EngineClasses::IsGpuFamilyClassOrLater(
            m_pTraceSubch->GetClassNum(), LWGpuClasses::GPU_CLASS_VOLTA))
    {
        ErrPrintf("CBC: Unsupported class (0x%x) for TRC test.\n",
            m_pTraceSubch->GetClass());
        return RC::SOFTWARE_ERROR;
    }

    // Init input buffers
    //
    CHECK_RC(InitInputSurfaces());

    m_Initialized = true;

    cleanup.Cancel();

    DebugPrintf("CBC: CompBitsTest::InitBackingStore - END\n");

    return rc;
}

RC CompBitsTest::InitInputSurfaces()
{
    RC rc;

    UINT64 backingStorePA = m_Settings.GetBackingStoreAllocPA();
    m_BackingStore.reset(new MdiagSurf);
    m_BackingStore->SetName("BackingStore");
    m_BackingStore->SetArrayPitch(m_Settings.GetBackingStoreSize());
    m_BackingStore->SetAlignment(512);
    m_BackingStore->SetColorFormat(ColorUtils::Y8);
    m_BackingStore->SetForceSizeAlloc(true);
    m_BackingStore->SetLocation(Memory::Fb);
    m_BackingStore->SetLayout(Surface2D::Pitch);
    m_BackingStore->SetType(LWOS32_TYPE_TEXTURE);
    m_BackingStore->SetFixedPhysAddr(backingStorePA);
    m_BackingStore->SetPageSize(4);

    DebugPrintf("CBC: backing store allocation - BEGIN\n");
    CHECK_RC(m_BackingStore->Alloc(m_BoundGpuDevice, m_Channel->GetLwRmPtr()));
    DebugPrintf("CBC: backing store allocation - END\n");

    if (m_BackingStore->GetVidMemOffset() != backingStorePA)
    {
        ErrPrintf("CBC: m_BackingStore->GetVidmemOffset 0x%llx doesn't equal m_PhysAddrBackingStore 0x%llx\n",
            m_BackingStore->GetVidMemOffset(), backingStorePA);
        return RC::SOFTWARE_ERROR;
    }

    DebugPrintf("CBC: backing store mapping - BEGIN\n");
    CHECK_RC(m_BackingStore->Map());
    DebugPrintf("CBC: backing store mapping - END\n");

    m_BackingStoreCpuAddress = m_BackingStore->GetAddress();

    DebugPrintf("CBC: backing store CPU address = %p\n",
        m_BackingStoreCpuAddress);

    return rc;
}

RC CompBitsTest::DumpBackingStore(string dumpName)
{
    RC rc;

    CHECK_RC(FlushCompTags());

    UINT32 bsSize = m_Settings.GetBackingStoreSize();
    vector<UINT08> data;
    data.resize(bsSize);

    DebugPrintf("CBC: Reading %u bytes of backing store\n", bsSize);
    Platform::VirtualRd(m_BackingStoreCpuAddress,
        static_cast<void*>(&data[0]),
        bsSize);

    DebugPrintf("CBC: Dumping %u bytes of backing store\n", bsSize);
    CHECK_RC(DumpRawSurfaceMemory(m_Channel->GetLwRmPtr(), m_Channel->GetSmcEngine(),
            m_BackingStore.get(), 0,
            m_BackingStore->GetArrayPitch(), dumpName, false,
            m_Channel->GetGpuResource(), m_Channel->GetCh(), m_Params, &data));

    return rc;
}

void CompBitsTest::CheckpointBackingStore
(
    string name,
    const vector<size_t>* pLastChanges
)
{
    m_MismatchOffsets.clear();
    DebugPrintf("CBC: CheckpointBackingStore(%s)\n", name.c_str());

    UINT32 bsSize = m_Settings.GetBackingStoreSize();

    vector<UINT08> data;
    data.resize(bsSize);

    DebugPrintf("CBC: Reading %u (0x%x) bytes of backing store\n",
        bsSize, bsSize);

    ReadBackingStoreData(&data);

    // Counting non-zero bytes
    size_t num = 0;

    // If this is the first this function is called, save the data and
    // don't do any comparisons.
    if (m_InitialBackingStoreData.empty())
    {
        m_InitialBackingStoreData = data;
        m_LwrrentBackingStoreData = data;
        for (size_t i = 0; i < data.size(); ++i)
        {
            if (data[i] != 0)
            {
                num ++;
            }
        }
    }

    // Otherwise, compare all of the bytes from the previous backing store
    // checkpoint to the current bytes.
    else
    {
        for (UINT32 i = 0; i < data.size(); ++i)
        {
            const unsigned long bsAddr = GetBackingStoreAddr(i);
            UINT08 byte1 = m_LwrrentBackingStoreData[i];
            UINT08 byte2 = data[i];
            if (byte2 != 0)
            {
                num ++;
            }

            if (pLastChanges != 0 && pLastChanges->size() > 0
                    && bsAddr == (*pLastChanges)[0])
            {
                DebugPrintf("CBC: check point @ offset 0x%lx (0x%lx) : last 0x%x new 0x%x\n",
                    long(i), bsAddr, byte1, byte2);
            }
            if (byte1 != byte2)
            {
                DebugPrintf("CBC: %s: backing store diff at offset 0x%lx (0x%lx) : 0x%x => 0x%x\n",
                        name.c_str(), long(i), bsAddr, byte1, byte2);
                m_LwrrentBackingStoreData[i] = data[i];
                m_MismatchOffsets.push_back(size_t(bsAddr));
            }
        }
    }
    if (num > 0)
    {
        DebugPrintf("CBC: number of non-zero backing store bytes: 0x%lx.\n",
            long(num));
    }
    DebugPrintf("CBC: Checkpoint: changed %lu bytes.\n",
        (long)m_MismatchOffsets.size());
}

void CompBitsTest::ReadBackingStoreData(size_t offset,
    size_t size, vector<UINT08>* pData)
{
    size_t bsSize = m_Settings.GetBackingStoreSize();
    MASSERT(bsSize > 0 && size > 0 && (offset + size <= bsSize));
    (void)bsSize;
    if (size > pData->size())
    {
        pData->resize(size);
    }
    DebugPrintf("CBC: ReadBackingStoreData, off 0x%lx size 0x%lx.\n",
        long(offset),
        long(size)
        );
    Platform::VirtualRd(reinterpret_cast<UINT08*>(m_BackingStoreCpuAddress) + offset,
        static_cast<void*>(&(*pData)[0]), size);
}

void CompBitsTest::ReadBackingStoreData
(
    vector<UINT08> *data
)
{
    ReadBackingStoreData(0, m_Settings.GetBackingStoreSize(), data);
}

RC CompBitsTest::WriteBackingStoreData(size_t offset,
    const vector<UINT08>& newData)
{
    RC rc;
    UINT32 bsSize = m_Settings.GetBackingStoreSize();
    MASSERT(bsSize > 0 && newData.size() > 0 &&
        (offset + newData.size() <= bsSize));
    (void)bsSize;

    DebugPrintf("CBC: WriteBackingStoreData off 0x%lx size 0x%lx.\n",
        long(offset),
        long(newData.size())
        );

    CHECK_RC(IlwalidateCompTags());

    Platform::VirtualWr(reinterpret_cast<UINT08*>(m_BackingStoreCpuAddress) + offset,
        static_cast<const void*>(&newData[0]),
        newData.size());

    vector<UINT08> checkData;
    ReadBackingStoreData(offset, newData.size(), &checkData);

    for (UINT32 i = 0; i < newData.size(); ++i)
    {
        if (checkData[i] != (newData)[i])
        {
            ErrPrintf("CBC: write data doesnt match read data at offset 0x%x  (0x%x != 0x%x)\n",
                i, newData[i], checkData[i]);
        }
    }

    return rc;
}
RC CompBitsTest::WriteBackingStoreData
(
    const vector<UINT08>& newData
)
{
    UINT32 bsSize = m_Settings.GetBackingStoreSize();
    if (newData.size() != bsSize)
    {
        ErrPrintf("attempting to write %lu bytes to backing store of size %u.\n",
            long(newData.size()), bsSize);
        return RC::SOFTWARE_ERROR;
    }
    return WriteBackingStoreData(0, newData);
}

size_t CompBitsTest::GetBackingStoreAddr(size_t offset) const
{
    return m_Settings.GetBackingStoreAllocPA() + offset;
}

RC CompBitsTest::FlushCompTags()
{
    RC rc;
    LWGpuSubChannel *pSubch =  m_pTraceSubch->GetSubCh();

    DebugPrintf("CBC: FlushCompTags - BEGIN\n");

    // In-band flush L2 to backing store
    //
    switch (pSubch->channel()->GetChannelClass())
    {
    case KEPLER_CHANNEL_GPFIFO_A:
        CHECK_RC(WaitForIdle());
        CHECK_RC(pSubch->MethodWriteRC(LWA06F_MEM_OP_A, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWA06F_MEM_OP_B,
                DRF_DEF(A06F, _MEM_OP_B, _OPERATION, _L2_CLEAN_COMPTAGS)));
        break;

    case KEPLER_CHANNEL_GPFIFO_B:
        CHECK_RC(pSubch->MethodWriteRC(LWA16F_WFI, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWA16F_MEM_OP_A, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWA16F_MEM_OP_B,
                DRF_DEF(A16F, _MEM_OP_B, _OPERATION, _L2_CLEAN_COMPTAGS)));
        break;

    case MAXWELL_CHANNEL_GPFIFO_A:
        CHECK_RC(pSubch->MethodWriteRC(LWB06F_WFI, DRF_DEF(B06F, _WFI, _SCOPE, _ALL)));
        CHECK_RC(pSubch->MethodWriteRC(LWB06F_MEM_OP_C, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWB06F_MEM_OP_D,
                DRF_DEF(B06F, _MEM_OP_D, _OPERATION, _L2_CLEAN_COMPTAGS)));
        break;

    case PASCAL_CHANNEL_GPFIFO_A:
    case VOLTA_CHANNEL_GPFIFO_A:
    case TURING_CHANNEL_GPFIFO_A:
    case AMPERE_CHANNEL_GPFIFO_A:
    case HOPPER_CHANNEL_GPFIFO_A:
        CHECK_RC(pSubch->MethodWriteRC(LWC06F_WFI, DRF_DEF(C06F, _WFI, _SCOPE, _ALL)));
        CHECK_RC(pSubch->MethodWriteRC(LWC06F_MEM_OP_A, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC06F_MEM_OP_B, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC06F_MEM_OP_C, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWC06F_MEM_OP_D,
            DRF_DEF(C06F, _MEM_OP_D, _OPERATION, _L2_CLEAN_COMPTAGS)));
        break;

    default:
        ErrPrintf("CBC: Unsupported GPFIFO class (0x%x) for TRC test.\n",
            pSubch->channel()->GetChannelClass());
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(WaitForIdle());

    DebugPrintf("CBC: FlushCompTags - ilwalidating L2 cache\n");

    GpuSubdevice *gpuSubdevice = m_BoundGpuDevice->GetSubdevice(0);
    MASSERT(gpuSubdevice != 0);

    gpuSubdevice->IlwalidateL2Cache(0);

    DebugPrintf("CBC: FlushCompTags - END\n");

    return rc;
}

RC CompBitsTest::IlwalidateCompTags()
{
    RC rc;
    GpuSubdevice *gpuSubdevice = m_BoundGpuDevice->GetSubdevice(0);
    MASSERT(gpuSubdevice != 0);

    DebugPrintf("CBC: IlwalidateCompTags - BEGIN\n");

    gpuSubdevice->IlwalidateCompBitCache(m_TimeoutMs);

    DebugPrintf("CBC: IlwalidateCompTags - END\n");

    return rc;
}

RC CompBitsTest::WaitForIdle()
{
    LWGpuSubChannel *pSubch =  m_pTraceSubch->GetSubCh();

    return pSubch->channel()->WaitForIdleRC();
}

RC CompBitsTest::CompareInitialBackingStore()
{
    MASSERT(m_InitialBackingStoreData.size() == m_LwrrentBackingStoreData.size());
    RC rc;
    UINT32 errorCount = 0;

    for (UINT32 i = 0; i < m_InitialBackingStoreData.size(); ++i)
    {
        UINT08 byte1 = m_InitialBackingStoreData[i];
        UINT08 byte2 = m_LwrrentBackingStoreData[i];

        if (byte1 != byte2)
        {
            ErrPrintf("CBC: backing store mismatch: offset 0x%x (0x%lx) Initial data 0x%x final data 0x%x\n",
                i, (long)GetBackingStoreAddr(i), byte1, byte2);
            ++errorCount;
        }
    }

    if (errorCount > 0)
    {
        ErrPrintf("CBC: %u backing store mismatches during comparing to Initial backing store.\n", errorCount);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    InfoPrintf("CBC: Compared latest data to initial backing store data: MATCH.\n");

    return rc;
}

RC CompBitsTest::GetComptagLines
(
    MdiagSurf *surface,
    UINT32 *minLine,
    UINT32 *maxLine
)
{
    RC rc;
    LwRm* pLwRm = m_Channel->GetLwRmPtr();
    LW0041_CTRL_GET_SURFACE_COMPRESSION_COVERAGE_PARAMS compParams = {0};

    CHECK_RC(pLwRm->Control(surface->GetMemHandle(),
            LW0041_CTRL_CMD_GET_SURFACE_COMPRESSION_COVERAGE,
            &compParams,
            sizeof(LW0041_CTRL_GET_SURFACE_COMPRESSION_COVERAGE_PARAMS)));

    DebugPrintf("CBC: %s: compParams.lineMin = %u\n",
        surface->GetName().c_str(), compParams.lineMin);
    DebugPrintf("CBC: %s: compParams.lineMax = %u\n",
        surface->GetName().c_str(), compParams.lineMax);

    *minLine = compParams.lineMin;
    *maxLine = compParams.lineMax;

    return rc;
}

RC CompBitsTest::CopySurfaceData(MdiagSurf *pSrcSurf,
    MdiagSurf *pDstSurf,
    MdiagSurf *pOrigDst)
{
    RC rc;

    // Have to keep original Source->Dest order for Release/Reacquire compression.
    MdiagSurf* pOrigSrc = pOrigDst == pDstSurf ? pSrcSurf : pDstSurf;

    LwRm* pLwRm = m_Channel->GetLwRmPtr();
    pLwRm->VidHeapReleaseCompression(pOrigSrc->GetMemHandle(),
        m_BoundGpuDevice);
    pLwRm->VidHeapReleaseCompression(pOrigDst->GetMemHandle(),
        m_BoundGpuDevice);

    rc = DoCopySurfaceData(pSrcSurf, pDstSurf);

    pLwRm->VidHeapReacquireCompression(pOrigSrc->GetMemHandle(),
        m_BoundGpuDevice);
    pLwRm->VidHeapReacquireCompression(pOrigDst->GetMemHandle(),
        m_BoundGpuDevice);

    return OK;
}

RC CompBitsTest::DoCopySurfaceData
(
    MdiagSurf *sourceSurface,
    MdiagSurf *destSurface
)
{
    RC rc;

    DebugPrintf("CBC: CopySurfaceData(%s, %s) - BEGIN\n",
        sourceSurface->GetName().c_str(), destSurface->GetName().c_str());

    UINT64 sourceVirtAddress = sourceSurface->GetCtxDmaOffsetGpu();
    UINT64 destVirtAddress = destSurface->GetCtxDmaOffsetGpu();

    LWGpuChannel* ch = m_Channel->GetCh();
    ch->CtxSwEnable(false);

    // Enable privileged access for DmaCheck
    Channel* pChannel = ch->GetModsChannel();
    MASSERT(pChannel);
    pChannel->SetPrivEnable(true);

    // reuse subchannel if needed
    UINT32 subChannel = 4;
    //CHECK_RC(ch->GetSubChannelNumClass(m_DmaObjHandle, &subChannel, 0));
    //CHECK_RC(ch->SetObjectRC(subChannel, m_DmaObjHandle));

    UINT32 data = DRF_DEF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _LOCAL_FB);
    CHECK_RC(ch->MethodWriteRC(subChannel, LWA0B5_SET_SRC_PHYS_MODE, data));

    data = DRF_DEF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _LOCAL_FB);
    CHECK_RC(ch->MethodWriteRC(subChannel, LWA0B5_SET_DST_PHYS_MODE, data));

    CHECK_RC(ch->MethodWriteRC(subChannel, LWA0B5_PITCH_IN, 0));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWA0B5_PITCH_OUT, 0));

    CHECK_RC(ch->MethodWriteRC(subChannel, LWA0B5_OFFSET_IN_UPPER,
            LwU64_HI32(sourceVirtAddress)));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWA0B5_OFFSET_IN_LOWER,
            LwU64_LO32(sourceVirtAddress)));

    CHECK_RC(ch->MethodWriteRC(subChannel, LWA0B5_OFFSET_OUT_UPPER,
            LwU64_HI32(destVirtAddress)));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWA0B5_OFFSET_OUT_LOWER,
            LwU64_LO32(destVirtAddress)));

    UINT32 size = sourceSurface->GetAllocSize();
    UINT32 pageSize = sourceSurface->GetActualPageSizeKB() << 10;
    size = ALIGN_UP(size, pageSize);
    DebugPrintf("CBC: LINE_LENGTH_IN = 0x%x\n", size);
    CHECK_RC(ch->MethodWriteRC(subChannel, LWA0B5_LINE_LENGTH_IN, size));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWA0B5_LINE_COUNT, 1));

    data = 0;
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH, data);

    CHECK_RC(ch->MethodWriteRC(subChannel, LWA0B5_LAUNCH_DMA, data));
    CHECK_RC(ch->WaitForIdleRC());

    ch->CtxSwEnable(true);

    DebugPrintf("CBC: CopySurfaceData(%s, %s) - END\n",
        sourceSurface->GetName().c_str(), destSurface->GetName().c_str());

    return OK;
}

RC CompBitsTest::DoSaveSurfaceData
(
    MdiagSurf *surface,
    vector<UINT08> *data
)
{
    RC rc;

    data->resize(surface->GetAllocSize());

    CHECK_RC(surface->Map());

    Platform::VirtualRd(surface->GetAddress(),
        static_cast<void*>(&(*data)[0]),
        surface->GetAllocSize());

    surface->Unmap();

    return rc;
}

RC CompBitsTest::SaveSurfaceData
(
    MdiagSurf *surface,
    vector<UINT08> *data
)
{
    RC rc;

    DebugPrintf("CBC: SaveSurfaceData(%s) - BEGIN\n",
        surface->GetName().c_str());

    LwRm* pLwRm = m_Channel->GetLwRmPtr();
    pLwRm->VidHeapReleaseCompression(surface->GetMemHandle(),
        m_BoundGpuDevice);

    CHECK_RC(DoSaveSurfaceData(surface, data));

    pLwRm->VidHeapReacquireCompression(surface->GetMemHandle(),
        m_BoundGpuDevice);

    DebugPrintf("CBC: SaveSurfaceData(%s) - END\n",
        surface->GetName().c_str());

    return rc;
}

RC CompBitsTest::RestoreSurfaceData
(
    MdiagSurf *surface,
    vector<UINT08> *data
)
{
    RC rc;

    DebugPrintf("CBC: RestoreSurfaceData(%s) - BEGIN\n",
        surface->GetName().c_str());
    LwRm* pLwRm = m_Channel->GetLwRmPtr();

    pLwRm->VidHeapReleaseCompression(surface->GetMemHandle(),
        m_BoundGpuDevice);

    CHECK_RC(surface->Map());

    Platform::VirtualWr(surface->GetAddress(),
        static_cast<const void*>(&(*data)[0]),
        surface->GetAllocSize());

    surface->Unmap();

    pLwRm->VidHeapReacquireCompression(surface->GetMemHandle(),
        m_BoundGpuDevice);

    DebugPrintf("CBC: RestoreSurfaceData(%s) - END\n",
        surface->GetName().c_str());

    return rc;
}

void CompBitsTest::CopySurfAttr(MdiagSurf* pSrcSurf, MdiagSurf* pDstSurf) const
{
    pDstSurf->SetWidth(pSrcSurf->GetWidth());
    pDstSurf->SetHeight(pSrcSurf->GetHeight());
    pDstSurf->SetDepth(pSrcSurf->GetDepth());
    pDstSurf->SetArraySize(pSrcSurf->GetArraySize());
    pDstSurf->SetBytesPerPixel(pSrcSurf->GetBytesPerPixel());
    pDstSurf->SetColorFormat(pSrcSurf->GetColorFormat());
    pDstSurf->SetLogBlockWidth(pSrcSurf->GetLogBlockWidth());
    pDstSurf->SetLogBlockHeight(pSrcSurf->GetLogBlockHeight());
    pDstSurf->SetLogBlockDepth(pSrcSurf->GetLogBlockDepth());
    pDstSurf->SetType(pSrcSurf->GetType());
    pDstSurf->SetName(pSrcSurf->GetName());
    pDstSurf->SetYCrCbType(pSrcSurf->GetYCrCbType());
    pDstSurf->SetLocation(pSrcSurf->GetLocation());
    pDstSurf->SetProtect(pSrcSurf->GetProtect());
    pDstSurf->SetShaderProtect(pSrcSurf->GetShaderProtect());
    pDstSurf->SetDmaProtocol(pSrcSurf->GetDmaProtocol());
    pDstSurf->SetLayout(pSrcSurf->GetLayout());
    pDstSurf->SetVASpace(pSrcSurf->GetVASpace());
    pDstSurf->SetTiled(pSrcSurf->GetTiled());
    pDstSurf->SetCompressed(pSrcSurf->GetCompressed());
    pDstSurf->SetCompressedFlag(pSrcSurf->GetCompressedFlag());
    pDstSurf->SetComptagStart(pSrcSurf->GetComptagStart());
    pDstSurf->SetComptagCovMin(pSrcSurf->GetComptagCovMin());
    pDstSurf->SetComptagCovMax(pSrcSurf->GetComptagCovMax());
    pDstSurf->SetComptags(pSrcSurf->GetComptags());
    pDstSurf->SetAAMode(pSrcSurf->GetAAMode());
    pDstSurf->SetDisplayable(pSrcSurf->GetDisplayable());
    pDstSurf->SetAlignment(pSrcSurf->GetAlignment());
    pDstSurf->SetVirtAlignment(pSrcSurf->GetVirtAlignment());
    pDstSurf->SetVaReverse(pSrcSurf->GetVaReverse());
    pDstSurf->SetZbcMode(pSrcSurf->GetZbcMode());
    pDstSurf->SetPriv(pSrcSurf->GetPriv());
    if (pSrcSurf->GetCreatedFromAllocSurface())
    {
        pDstSurf->SetCreatedFromAllocSurface();
    }
    pDstSurf->SetMemoryMappingMode(pSrcSurf->GetMemoryMappingMode());
    pDstSurf->SetFbSpeed(pSrcSurf->GetFbSpeed());
    pDstSurf->SetSplit(pSrcSurf->GetSplit());
    pDstSurf->SetSplitLocation(pSrcSurf->GetSplitLocation());
    pDstSurf->SetPitch(pSrcSurf->GetPitch());
    pDstSurf->SetHiddenAllocSize(pSrcSurf->GetHiddenAllocSize());
    pDstSurf->SetExtraAllocSize(pSrcSurf->GetExtraAllocSize());
    pDstSurf->SetGpuCacheMode(pSrcSurf->GetGpuCacheMode());
    pDstSurf->SetP2PGpuCacheMode(pSrcSurf->GetP2PGpuCacheMode());
    pDstSurf->SetSplitGpuCacheMode(pSrcSurf->GetSplitGpuCacheMode());
    pDstSurf->SetPhysContig(pSrcSurf->GetPhysContig());
    pDstSurf->SetSkedReflected(pSrcSurf->GetSkedReflected());
    pDstSurf->SetIsSparse(pSrcSurf->GetIsSparse());
    pDstSurf->SetTileWidthInGobs(pSrcSurf->GetTileWidthInGobs());
    pDstSurf->SetExtraWidth(pSrcSurf->GetExtraWidth());
    pDstSurf->SetExtraHeight(pSrcSurf->GetExtraHeight());
}

RC CompBitsTest::ReplicateSurface(MdiagSurf* pSrcSurf,
    MdiagSurf* pDstSurf, const char* pDstSurfName) const
{
    RC rc;

    CopySurfAttr(pSrcSurf, pDstSurf);
    pDstSurf->SetName(pDstSurfName);
    DebugPrintf("CBC: New color surface %s allocation - BEGIN\n",
        pDstSurf->GetName().c_str()
        );
    CHECK_RC(pDstSurf->Alloc(m_BoundGpuDevice, m_Channel->GetLwRmPtr()));
    DebugPrintf("CBC: New color surface %s allocation @ 0x%llx- END\n",
        pDstSurf->GetName().c_str(),
        pDstSurf->GetVidMemOffset()
        );

    return rc;
}

RC CompBitsTest::RebuildSurface(MdiagSurf* pSurf)
{
    RC rc;

    // Save new surface data
    vector<UINT08> newSurfData;
    CHECK_RC(SaveSurfaceData(pSurf, &newSurfData));//, true));

    UINT64 virtAddress = pSurf->GetCtxDmaOffsetGpu();
    UINT32 comptagLineMin[3];
    UINT32 comptagLineMax[3];

    CHECK_RC(GetComptagLines(pSurf,
            &comptagLineMin[0], &comptagLineMax[0]));

    UINT32 comptagDiff = comptagLineMax[0] - comptagLineMin[0];

    comptagLineMin[1] = comptagLineMax[0] + 1;
    comptagLineMax[1] = comptagLineMin[1] + comptagDiff;
    comptagLineMin[2] = comptagLineMax[1] + 1;
    comptagLineMax[2] = comptagLineMin[2] + comptagDiff;
    UINT32 comptagLine = comptagLineMin[1];

    InfoPrintf("CBC: RebuildSurface change comptag form %u to %u.\n",
        comptagLineMin[0],
        comptagLine
        );

    MdiagSurf saveSurface;
    CopySurfAttr(pSurf, &saveSurface);

    pSurf->Free();

    CopySurfAttr(&saveSurface, pSurf);

    pSurf->SetFixedVirtAddr(virtAddress);
    pSurf->SetPaReverse(!pSurf->GetPaReverse());
    pSurf->SetCtagOffset(DRF_NUM(OS32, _ALLOC, _COMPTAG_OFFSET_START, comptagLine)
        | DRF_DEF(OS32, _ALLOC, _COMPTAG_OFFSET_USAGE, _FIXED));

    CHECK_RC(pSurf->Alloc(m_BoundGpuDevice, m_Channel->GetLwRmPtr()));
    CHECK_RC(RestoreSurfaceData(pSurf, &newSurfData));

    return rc;
}

RC CompBitsTest::OnCheckpoint(size_t num, const char* pStr)
{
    if (m_MismatchOffsets.size() != 1)
    {
        ErrPrintf("%s changed %u bytes in the backing store - expected exactly a %u byte(s) change\n",
            pStr, m_MismatchOffsets.size(), int(num));
        if (!GetArgReader()->ParamPresent("-compbit_ignore_checkpoint_fail"))
        {
            return RC::GOLDEN_VALUE_MISCOMPARE;
        }
    }

    return OK;
}

RC CompBitsTest::SetupTest()
{
    DebugPrintf("CBC: CompBitsTest default SetupTest.\n");
    RC rc;

    CHECK_RC(m_TestSurf.Init(this, GetSurfCA(), true, true));
    m_TestSurf.SetOffset(GetArgReader()->ParamUnsigned("-compbit_test_offset", 0));
    DebugPrintf("CBC: compbit_test_offset uses %lu (0x%lx).\n",
        long(m_TestSurf.GetOffset()), long(m_TestSurf.GetOffset()));

    return rc;
}

void CompBitsTest::CleanupTest()
{
    DebugPrintf("CBC: CompBitsTest default CleanupTest.\n");
    m_TestSurf.Clear();
}

RC CompBitsTest::StartTest()
{
    DebugPrintf("CBC: CompBitsTest default StartTest.\n");

    RC rc;
    CHECK_RC(m_pCBCLib->GetCBCLib());
    if (!GetArgReader()->ParamPresent("-compbit_no_cfgchk"))
    {
        CompBits::Settings::AmapInfo *pInfo;
        pInfo = m_pCBCLib->GetAmapInfo();
        rc = m_Settings.Check(pInfo);
        if (pInfo != 0)
        {
            delete pInfo;
        }
    }
    return rc;
}

/////////////////////////////////////////////////////////////////////////

namespace CompBits
{

///////////////////////////////////////////////////////////////////////////

RC SingleTileTest1::Init()
{
    RC rc;

    CHECK_RC(CompBitsTestIF::Init());

    CHECK_RC(FlushCompTags());
    // compbits == 0 prior to clear.
    // Set this as the backing store check baseline (Initial state).
    // As option -scissor_correct 8x8+0+0 specified, it clears a single ROP
    // tile of a color surface. Hence when in DoPostRender(), there'll be a
    // single byte change in backing store.
    CheckpointBackingStore("Initial");

    return rc;
}

RC SingleTileTest1::DoPostRender()
{
    RC rc;

    CHECK_RC(FlushCompTags());
    CheckpointBackingStore("PostClear");
    CHECK_RC(IlwalidateCompTags());

    // compbits become non-zero after a targeted clear.
    CHECK_RC(OnCheckpoint(1, "clearing a single tile"));

    UINT32 compbits = 0;
    m_TestSurf.GetCompBits(&compbits);

    DebugPrintf("CBC: GetCompBits: New compbits (PostClear) = 0x%x of surfOff 0x%llx.\n",
        compbits, m_TestSurf.GetOffset()
        );

    // Expected: postclear compbits
    UINT32 bitsPostClear = compbits;

    if (compbits != 0)
    {
        // Restore to Initial
        compbits = 0;
    }
    else
    {
        // Error
        compbits = 0xffffffff;
    }

    DebugPrintf("CBC: PutCompBits: putting Initial compbits = 0x%x for surfoff 0x%llx.\n",
        compbits, m_TestSurf.GetOffset()
        );
    m_TestSurf.PutCompBits(compbits);

    CheckpointBackingStore("MatchInitial");
    CHECK_RC(CompareInitialBackingStore());

    // Expected: postclear compbits
    // Fixup (restore to PostClear)
    compbits = bitsPostClear;
    DebugPrintf("CBC: PutCompBits: Restore to PostClear compbits = 0x%x for surfoff 0x%llx.\n",
        compbits, m_TestSurf.GetOffset()
        );
    m_TestSurf.PutCompBits(compbits);

    return rc;
}

///////////////////////////////////////////////////////////////////////////

RC SingleTileTest2::SetupParam()
{
    DebugPrintf("CBC: Test2 SetupParam.\n");
    m_Data.resize(CompBits::Settings::SectorSize);
    for (size_t i = 0; i < CompBits::Settings::SectorSize; ++i)
    {
        m_Data[i] = i;
    }
    return OK;
}

RC SingleTileTest2::DoPostRender()
{
    RC rc;

    DebugPrintf("CBC: offset 0x%08lx total size 0x%08lx\n",
        (long)m_TestSurf.GetOffset(),
        (long)m_TestSurf.GetSize()
        );

    // Unlike in SingleTileTest1, the initial checkpoint of the
    // backing store has to happen after graphics rendering.
    CHECK_RC(FlushCompTags());
    // compbits become non-zero after an untargeted clear.
    CheckpointBackingStore("Initial");
    CHECK_RC(IlwalidateCompTags());

    UINT32 compbits = 0;
    m_TestSurf.GetCompBits(&compbits);

    DebugPrintf("CBC: GetCompBits(post clear): Initial compbits = 0x%x of surfoff 0x%llx.\n",
        compbits, m_TestSurf.GetOffset()
        );
    UINT32 bitsInitial = compbits;

    // The surface should be entirely compressed at this point, so the
    // compbits should be non-zero.
    if (0 == compbits)
    {
        ErrPrintf("CBC: Initial compbits should return non-zero value\n");
    }

    // Write to surface at command-line argument offset.
    m_TestSurf.MapAndWrite(m_Data);

    CHECK_RC(FlushCompTags());
    CheckpointBackingStore("PostBar1Write");

    CHECK_RC(OnCheckpoint(1, "Writing to a single tile"));

    // Getting PostBar1Write compbits.
    m_TestSurf.GetCompBits(&compbits);

    DebugPrintf("CBC: GetCompBits(post BAR1 write): New compbits = 0x%x of surfoff 0x%llx.\n",
        compbits, m_TestSurf.GetOffset()
        );

    // The BAR1 write to the surface should have decompressed the tile.  If
    // the GetCompBits function is getting the same compbits as before the
    // BAR1 write, then either the tile wasn't decompressed or GetCompBits
    // is getting the compbits from the wrong tile.  If the problem is the
    // latter, we want to make sure that the following PutCompBits function
    // causes a change to the backing store because it will help in debugging.
    if (compbits == bitsInitial)
    {
        // Test == Initial, unexpected.
        ErrPrintf("CBC: BAR1 write to tile did not change the corresponding GetCompBits value.\n");

        // Flip all of the bits so that the following PutCompBits function will
        // be guaranteed to cause a backing store difference.
        compbits ^= 0xffffffff;
    }
    else
    {
        // Test != Initial as expected. Restore to initial.
        compbits = bitsInitial;
    }

    // Expected: Initial.

    DebugPrintf("CBC: PutCompBits: Restore to Initial compbits = 0x%x for surfoff 0x%llx.\n",
        compbits, m_TestSurf.GetOffset()
        );
    m_TestSurf.PutCompBits(compbits);

    CheckpointBackingStore("MatchInitial");
    CHECK_RC(CompareInitialBackingStore());

    // Ilwalidate the PostBar1Write compbits we've made.
    CHECK_RC(IlwalidateCompTags());

    return rc;
}

///////////////////////////////////////////////////////////////////////////

RC SurfMoveTest::DoPostRender()
{
    RC rc;

    CHECK_RC(FlushCompTags());
    CheckpointBackingStore("Initial");

    UINT32 pageCount = m_TestSurf.PageCount();
    vector<vector<UINT32> > compbitBuffers;
    compbitBuffers.resize(pageCount);

    DebugPrintf("CBC: ColorA pageCount = %u\n", pageCount);

    InfoPrintf("CBC: SurfaceMoveTest: surface PHY base: 0x%llx\n",
        m_TestSurf.GetPhyBase()
        );
    for (UINT32 pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        DebugPrintf("CBC: GetPageCompBits pageIndex = %u\n", pageIndex);
        compbitBuffers[pageIndex].resize(CompBits::Settings::TilesPer64K);
        m_TestSurf.GetCompOperation()->GetPageCompBits(pageIndex,
            &compbitBuffers[pageIndex][0]);
    }

    vector<UINT08> oldDecompData;
    CHECK_RC(SaveSurfaceDecompData(GetSurfCA(), &oldDecompData));
    const UINT64 oldPhyBase = m_TestSurf.GetPhyBase();
    CHECK_RC(RebuildSurface(m_TestSurf.GetMdiagSurf()));
    MASSERT(m_TestSurf.GetMdiagSurf() == GetSurfCA());
    // Perform a re-init
    CHECK_RC(m_TestSurf.Init(this, GetSurfCA(), true, true));
    MASSERT(oldPhyBase != m_TestSurf.GetPhyBase());
    (void)oldPhyBase;
    InfoPrintf("CBC: SurfaceMoveTest: surface PHY base: 0x%llx after rebuild\n",
        m_TestSurf.GetPhyBase()
        );

    for (UINT32 pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        DebugPrintf("CBC: PutPageCompBits pageIndex = %u\n", pageIndex);
        m_TestSurf.GetCompOperation()->PutPageCompBits(pageIndex,
            &compbitBuffers[pageIndex][0]);
    }

    CheckpointBackingStore("PostSwizzle");
    CHECK_RC(IlwalidateCompTags());

    if (m_MismatchOffsets.size() == 0)
    {
        ErrPrintf("CBC: no backing store bytes changed after restoring the compression bits.\n");
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    vector<UINT08> newDecompData;
    CHECK_RC(SaveSurfaceDecompData(GetSurfCA(), &newDecompData));
    if (newDecompData != oldDecompData)
    {
        ErrPrintf("CBC: SurfaceMoveTest: Surface decompressed data source & dest MISMATCH after test.\n");
        CompBits::ArrayReportFirstMismatch<vector<UINT08> >(oldDecompData,
            newDecompData);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    else
    {
        InfoPrintf("CBC: SurfaceMoveTest: Surface decompressed data source & dest MATCH after test.\n");
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////

bool CheckOffsetTest::Entry::Match() const
{
    bool res = m_BsOff == m_LibOff;
    InfoPrintf("CBC: Entry: surfOff 0x%08lx -> "
        "bsOff 0x%08lx -> "
        "libOff 0x%08lx\n",
        long(m_SurfOff), long(m_BsOff), long(m_LibOff),
        res ? "PASS" : "**Offset Mismatch**"
        );
    return res;
}

RC CheckOffsetTest::SetupParam()
{
    DebugPrintf("CBC: ChkOffTest SetupParam.\n");
    for (size_t i = 0; i < m_Data.size(); ++i)
    {
        m_Data[i] = i;
    }
    if (GetArgReader()->ParamPresent("-compbit_test_offset"))
    {
        m_Loops = 1;
    }
    DebugPrintf("CBC: Initial test loops %lu\n", long(m_Loops));

    return OK;
}

RC CheckOffsetTest::Run()
{
    RC rc;

    m_OffsetChanges.clear();
    UINT32 compbits = 0;
    m_TestSurf.GetCompBits(&compbits);
    UINT32 bitsInitial = compbits;
    DebugPrintf("CBC: got Initial compbits 0x%x of surfoff 0x%llx.\n",
        bitsInitial, m_TestSurf.GetOffset()
        );
    m_TestSurf.Write(m_Data);
    CHECK_RC(FlushCompTags());
    CheckpointBackingStore("PostBar1Write", &m_OffsetChanges);
    CHECK_RC(OnCheckpoint(1, "writing to a single tile"));

    Entry ent;
    if (m_MismatchOffsets.size() > 0)
    {
        ent.m_BsOff = m_MismatchOffsets[0];
    }
    m_TestSurf.GetCompBits(&compbits);
    DebugPrintf("CBC: lib getbits test, got PostBar1Write compbits 0x%x of surfoff 0x%llx.\n",
        compbits, m_TestSurf.GetOffset()
        );
    if (compbits == bitsInitial)
    {
        ErrPrintf("CBC: Unexpected PostBar1Write compbits == Initial compbits 0x%x of surfoff 0x%llx.\n",
            compbits, m_TestSurf.GetOffset()
            );
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    // Restore to Initial
    compbits = bitsInitial;
    DebugPrintf("CBC: lib restoring to Initial compbits 0x%x for surfoff 0x%llx.\n",
        compbits, m_TestSurf.GetOffset()
        );
    //if (CompBits::PrePascal == m_Settings.GetTRCChipFamily())
    //{
        // BUG OF CBC LIB? Looks like Maxwell will fail if use tough WrVerify,
        // so use PutCompBits.
    //    m_TestSurf.PutCompBits(compbits);
    //}
    //else
    //{
        CHECK_RC(m_TestSurf.WrVerifyCompBits(compbits));
    //}
    DebugPrintf("CBC: probing offset, expected to be 0x%016llx.\n",
        ent.m_BsOff
        );
    CheckpointBackingStore("MatchInitial", &m_OffsetChanges);
    CHECK_RC(OnCheckpoint(1, "Restore to Initial compbits"));
    if (m_MismatchOffsets.size() > 0)
    {
        ent.m_LibOff = m_MismatchOffsets[0];
    }
    ent.m_SurfOff = m_TestSurf.GetOffset();
    m_Entries.push_back(ent);

    CHECK_RC(CompareInitialBackingStore());
    CHECK_RC(IlwalidateCompTags());

    return rc;
}

RC CheckOffsetTest::DoPostRender()
{
    RC rc;

    CHECK_RC(FlushCompTags());
    CheckpointBackingStore("Initial");

    if (m_Loops < 1)
    {
        m_Loops = (m_TestSurf.GetSize() + (m_Settings.GetCompBitsAccessBasis() - 1)) / m_Settings.GetCompBitsAccessBasis();
        DebugPrintf("CBC: Updated test loops to %lu\n", long(m_Loops));
    }
    CHECK_RC(m_TestSurf.Map());
    if (m_Loops > 1)
    {
        for (size_t i = 0; i < m_Loops; i++)
        {
            UINT64 off = i * m_Settings.GetCompBitsAccessBasis();
            m_TestSurf.SetOffset(off);
            DebugPrintf("CBC: ---Testing loop %lu/%lu offset 0x%08lx "
                "total size 0x%08lx---\n",
                long(i + 1), long(m_Loops),
                (long)m_TestSurf.GetOffset(),
                (long)m_TestSurf.GetSize()
                );
            rc = Run();
            if (rc != OK)
            {
                break;
            }
        }
    }
    else
    {
        DebugPrintf("CBC: offset 0x%08lx total size 0x%08lx\n",
            (long)m_TestSurf.GetOffset(),
            (long)m_TestSurf.GetSize()
            );
        rc = Run();
    }
    DebugPrintf("CBC: ---Tested Offsets 0x%lx---\n", long(m_Entries.size()));
    int errors = 0;
    for (ListType::const_iterator it = m_Entries.begin(); it != m_Entries.end(); it++)
    {
        if (!it->Match())
        {
            ++ errors;
        }
    }
    m_TestSurf.Unmap();
    m_Entries.clear();
    if (0 == errors && OK == rc)
    {
        DebugPrintf("CBC: ---CheckOffsetTest test PASS.---\n");
    }
    else
    {
        ErrPrintf("CBC: ---CheckOffsetTest test FAIL: %u (0x%x) errors.---\n",
            errors, errors
            );
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    return rc;
}

////////////////////////Test Creator///////////////////////

CompBitsTest* TestCreator::GetCommonTest(TraceSubChannel* pSubChan, const TestID id)
{
    CompBitsTest* pTest = 0;
    switch (id)
    {
    case tidTest1:
        pTest = new SingleTileTest1(pSubChan);
        break;
    case tidTest2:
        pTest = new SingleTileTest2(pSubChan);
        break;
    case tidSurfMove:
        pTest = new SurfMoveTest(pSubChan);
        break;
    case tidCheckOffset:
        pTest = new CheckOffsetTest(pSubChan);
        break;
    default:
        pTest = new UnsupportedTest(pSubChan);
        break;
    }

    return pTest;
}

CompBitsTest* TestCreator::CreateCommonTest(TraceSubChannel* pSubChan, const TestID id)
{
    if (id != tidUnitTest)
    {
        return GetCommonTest(pSubChan, id);
    }
    return GetCommonUnitTest(pSubChan);
}

CompBitsTest* TestCreator::Create(TraceSubChannel* pSubChan, const TestID id)
{
    InfoPrintf("MODS TRC Tests version: %s\n", TestCreator::TestVersion());
    CompBitsTest* pTest = 0;
    const TRCChipFamily family = GetTRCChipFamily(pSubChan);
    if (PrePascal == family)
    {
        pTest = TestCreator::CreateMaxwellTest(pSubChan, id);
    }
    else if (PascalAndLater == family)
    {
        pTest = TestCreator::CreatePascalTest(pSubChan, id);
    }
    else
    {
        ErrPrintf("CBC: MODS TRC doesn't support the chip running on.\n");
    }

    return pTest;
}

} //namespace CompBits

///////////////////////////////////////////////////////////////////////////

RC CompBitsTestIF::Init()
{
    RC rc;

    CompBitsTest* pTest = dynamic_cast<CompBitsTest*>(this);
    MASSERT(pTest != 0);
    DebugPrintf("CBC: Default Init: Check compressed surface...\n");
    if (!pTest->GetSurfCA()->GetCompressed())
    {
        ErrPrintf("CBC: ColorA is not compressed.\n");
        return RC::SOFTWARE_ERROR;
    }
    else if (pTest->GetSurfCA()->GetActualPageSizeKB() != 64)
    {
        ErrPrintf("CBC: ColorA surface isn't allocated with Expected big page size (Actual: %u).\n",
            pTest->GetSurfCA()->GetActualPageSizeKB());
        return RC::SOFTWARE_ERROR;
    }

    DebugPrintf("CBC: Default Init: Calling InitBackingStore()...\n");
    CHECK_RC(pTest->InitBackingStore());

    return rc;
}

RC CompBitsTestIF::PostRender()
{
    RC rc;

    CompBitsTest* pTest = dynamic_cast<CompBitsTest*>(this);
    MASSERT(pTest != 0);
    DebugPrintf("CBC: Default PostRender: Calling SetupParam()...\n");
    CHECK_RC(pTest->SetupParam());
    DebugPrintf("CBC: Default PostRender: Calling SetupTest()...\n");
    CHECK_RC(pTest->SetupTest());
    DebugPrintf("CBC: Default PostRender: Calling StartTest()...\n");
    CHECK_RC(pTest->StartTest());
    DebugPrintf("CBC: Default PostRender: Calling DoPostRender()...\n");
    rc = pTest->DoPostRender();
    DebugPrintf("CBC: Default PostRender: Calling CleanupTest()...\n");
    pTest->CleanupTest();

    return rc;
}

CompBitsTestIF* CompBitsTestIF::CreateInstance(TraceSubChannel* pSubChan,
    UINT32 testType)
{
    return CompBits::TestCreator::Create(pSubChan, CompBits::TestID(testType));
}

