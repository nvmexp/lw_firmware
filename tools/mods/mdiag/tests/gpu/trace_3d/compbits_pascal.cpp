/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/rc.h"
#include "gpu/include/gpudev.h"
#include "tracesubchan.h"
#include "tracechan.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"
#include "Lwcm.h"

// CompBits lib for Pascal & later.
#include "compbits_pascal.h"

///////////////////////////////////////////////////////////

namespace CompBits_Pascal
{

struct AmapAddr
{
    // Surface physical base addr == vidmem offset.
    UINT64 surfBase;
    // Surface physical offset == offset to vidmem offset.
    UINT64 surfOff;
    UINT32 gob; // GOB Padr
    UINT32 partition;
    UINT32 slice;
    AmapAddr()
        : surfBase(0),
        surfOff(0),
        gob(0),
        partition(0),
        slice(0)
    {}
};

// Encapsulate AMAP APIs calling code into CBC interfaces.

class CompOperationImpl
    : public CompBits::SurfCompOperation
{
public:
    CompOperationImpl(CompBits::CBCLibWrapper* pLibWrapper, CompBits::TestSurf* pTestSurf)
        : CompBits::SurfCompOperation(pLibWrapper, pTestSurf)
    {}

    virtual  ~CompOperationImpl() {}

    virtual void GetCompBits(size_t offset, UINT32 *pCompbits)
    {
        AmapAddr addr;
        addr.surfBase = m_pTestSurf->GetPhyBase();
        addr.surfOff = offset;
        const UINT32 comptagLine = m_pTestSurf->GetComptag(offset);
        GetPartSliceOffset(&addr);

        UINT32 *pCacheLine = m_pTestSurf->CreateCachelineBuffer();

        ReadCompCacheLine(
            pCacheLine,
            comptagLine,
            addr.partition,
            addr.slice
            );

        getCompBits(m_pCBCLib->GetLibHandle(), pCompbits, pCacheLine, addr.gob, comptagLine);

        delete[] pCacheLine;
    }

    virtual void PutCompBits(size_t offset, UINT32 compbits)
    {
        AmapAddr addr;
        addr.surfBase = m_pTestSurf->GetPhyBase();
        addr.surfOff = offset;
        const UINT32 comptagLine = m_pTestSurf->GetComptag(offset);
        GetPartSliceOffset(&addr);

        UINT32 *pCacheLine = m_pTestSurf->CreateCachelineBuffer();

        ReadCompCacheLine(
            pCacheLine,
            comptagLine,
            addr.partition,
            addr.slice
            );

        putCompBits(m_pCBCLib->GetLibHandle(), compbits, pCacheLine, addr.gob, comptagLine);

        WriteCompCacheLine(
            pCacheLine,
            comptagLine,
            addr.partition,
            addr.slice
            );
        delete[] pCacheLine;
    }

    virtual void GetPageCompBits(size_t pageIdx,
        UINT32 pCompbits[CompBits::Settings::TilesPer64K])
    {
        memset(pCompbits, 0, CompBits::Settings::TilesPer64K * sizeof(pCompbits[0]));
        const size_t off = m_pTestSurf->GetPageOffset(pageIdx);

        for (size_t i = 0; i < CompBits::Settings::TilesPer64K; i++)
        {
            GetCompBits(off + i * CompBits::Settings::TileSize,
                pCompbits + i);
        }
    }

    virtual void PutPageCompBits(size_t pageIdx,
        const UINT32 pCompbits[CompBits::Settings::TilesPer64K])
    {
        const size_t off = m_pTestSurf->GetPageOffset(pageIdx);

        for (size_t i = 0; i < CompBits::Settings::TilesPer64K; i++)
        {
            PutCompBits(off + i * CompBits::Settings::TileSize,
                pCompbits[i]);
        }
    }

public:
    bool Swizzle(CompTagInfoV1 *pComptagInfo, CbcChunkParamsV1 *pChunkParams)
    {
        SetupComptagInfo(pComptagInfo);
        return swizzleCompbitsTRC(m_pCBCLib->GetLibHandle(), pComptagInfo, pChunkParams);
    }

private:
    void GetPartSliceOffset(AmapAddr* pAddr) const
    {
        getPAKSSwizzle(m_pCBCLib->GetLibHandle(), pAddr->surfBase);
        uint64_t gob = 0;
        getPartSliceOffset(m_pCBCLib->GetLibHandle(), &pAddr->partition,
            &pAddr->slice, &gob, pAddr->surfBase, pAddr->surfOff);
        pAddr->gob = (UINT32)gob;
    }

    void ReadCompCacheLine(
        UINT32 compCacheLine[],
        UINT32 comptagLine,
        UINT32 partition,
        UINT32 slice
        )
    {
        CbcChunkParamsV1 chunk = {0};
        chunk.trcForceBar1Read = true;
        chunk.trcForceBar1Write = true;
        readCompCacheLine(m_pCBCLib->GetLibHandle(), compCacheLine,
            comptagLine, partition, slice, (AmapLibFBRead32)&fbrd32, &chunk);
    }

    void WriteCompCacheLine(
        UINT32 compCacheLine[],
        UINT32 comptagLine,
        UINT32 partition,
        UINT32 slice
        )
    {
        CbcChunkParamsV1 chunk = {0};
        chunk.trcForceBar1Read = true;
        chunk.trcForceBar1Write = true;
        writeCompCacheLine(m_pCBCLib->GetLibHandle(), compCacheLine, comptagLine,
            partition, slice, (AmapLibFBWrite32)&fbwr32, &chunk);
    }

    static ssize_t CheckPA(UINT64 pa, const CbcSwizzleParamsV1 *cbcParams,
        const CbcChunkParamsV1* chunkParams, const char* pRW)
    {
        UINT64 storeBase;
        MASSERT(0 == (pa & 0x3));
        if (chunkParams->trcForceBar1Read)
        {
            storeBase = cbcParams->backingStoreAllocPA;
        }
        else
        {
            storeBase = chunkParams->backingStoreChunkPA;
        }
        if (pa < storeBase)
        {
            if (chunkParams->trcForceBar1Read)
            {
                ErrPrintf("CBC: %s: backingStoreBase 0x%016llx, "
                    "expecting PA >= backingStoreBase, actual PA 0x%016llx\n",
                    pRW, storeBase, pa
                    );
            }
            else
            {
                ErrPrintf("CBC: %s: backingStoreChunkPA 0x%016llx, "
                    "expecting PA >= backingStoreChunkPA, actual PA 0x%016llx\n",
                    pRW, storeBase, pa
                    );
            }
            MASSERT(!"Invalid PA Offset, Out-of-Range issue detected.");
        }
        return pa - storeBase;
    }

    static UINT32 fbrd32(UINT64 pa, const CbcSwizzleParamsV1 *cbcParams, const CbcChunkParamsV1 *chunkParams)
    {
        const ssize_t off = CheckPA(pa, cbcParams, chunkParams, __FUNCTION__);
        if (chunkParams->trcForceBar1Read)
        {
            return MEM_RD32((uintptr_t)cbcParams->backingStoreVA + off);
        }
        return MEM_RD32((uintptr_t)chunkParams->backingStoreChunkVA + off);
    }

    static void fbwr32(UINT64 pa, UINT32 data, CbcSwizzleParamsV1 *cbcParams, CbcChunkParamsV1 *chunkParams)
    {
        const ssize_t off = CheckPA(pa, cbcParams, chunkParams, __FUNCTION__);
        if (chunkParams->trcForceBar1Write)
        {
            MEM_WR32((uintptr_t)cbcParams->backingStoreVA + off, data);
            return;
        }
        MEM_WR32((uintptr_t)chunkParams->backingStoreChunkVA + off, data);

        if (cbcParams->backingStoreChunkOverfetch > 1)
        {
            //
            // Find the index of the 128 byte-sized chunk being modified and set the
            // bit in the bitmap, representing the chunk.
            //
            UINT64 chunkIndex128B = ((UINT64)off) >> 7;
            UINT64 byteIndex = chunkIndex128B >> 3;
            UINT08 bitIndex = chunkIndex128B % 8;
            UINT08 bitMask = 0x01 << bitIndex;
            chunkParams->cacheWriteBitMap[byteIndex] |= bitMask;
        }
    }

    void SetupComptagInfo(CompTagInfoV1* pInfo)
    {
        pInfo->fbrd32 = (AmapLibFBRead32)&fbrd32;
        pInfo->fbwr32 = (AmapLibFBWrite32)&fbwr32;
    }

    void SetupChunkParam(CbcChunkParamsV1* pChunkParm)
    {
        pChunkParm->trcForceBar1Read = false;
        pChunkParm->trcForceBar1Write = false;
    }

};

/////////Chip specific CBC Connection implemention/////////

class LibWrapperImpl
    : public CompBits::CBCLibWrapper
{
public:
    LibWrapperImpl(CompBitsTest* pTest)
        : CompBits::CBCLibWrapper(pTest)
    {
        memset(&m_CfgParm, 0, sizeof m_CfgParm);
        memset(&m_SwizzleParm, 0, sizeof m_SwizzleParm);
    }

    virtual  ~LibWrapperImpl()
    {
        if (m_pHandle != 0)
        {
            freeAmapConfV1(m_pHandle);
            m_pHandle = 0;
        }
    }

    virtual RC Init() { return OK; }
    virtual RC GetCBCLib();
    virtual CompBits::Settings::AmapInfo* GetAmapInfo()
    {
        CompBits::Settings::AmapInfo* pInfo = new CompBits::Settings::AmapInfo;
        pInfo->m_ChunkSize = GetTotalCompCacheLineSize();
        UINT32 gobs = 0;
        pInfo->m_ComptagsPerChunk = GetComptagLinesPerCacheLine(&gobs);
        pInfo->m_GobsPerComptagPerSlice = gobs;
        return pInfo;
    }
    virtual CompBits::SurfCompOperation* FetchCompOperation(CompBits::TestSurf* pSurf)
    {
        CompOperationImpl* pOps = new CompOperationImpl(this, pSurf);
        pSurf->SetCompOperation(pOps);
        return pOps;
    }
    virtual void ReleaseCompOperation(CompBits::SurfCompOperation* pOps)
    {
        delete pOps;
    }

public:
    size_t GetComptagLinesPerCacheLine(UINT32* gobs)
    {
        return getComptagLinesPerCacheLine(m_pHandle, gobs);
    }
    size_t GetTotalCompCacheLineSize()
    {
        return getTotCompCacheLineSize(m_pHandle);
    }

private:
    RC SetContext(
        UINT64 backingStoreAllocPA,
        UINT08 *backingStoreVA
        )
    {
        m_SwizzleParm.backingStoreAllocPA = backingStoreAllocPA;

        LW2080_CTRL_CMD_FB_GET_AMAP_CONF_PARAMS fbGetAmapConfParams = {0};
        fbGetAmapConfParams.pAmapConfParams = &m_CfgParm;
        fbGetAmapConfParams.pCbcSwizzleParams = &m_SwizzleParm;

        MASSERT(m_pTest != 0);

        RC rc;
        LwRm* pLwRm = m_pTest->GetTraceChan()->GetLwRmPtr();
        CHECK_RC(pLwRm->Control(
                pLwRm->GetSubdeviceHandle(m_pTest->GetGpuDev()->GetSubdevice(0)),
                LW2080_CTRL_CMD_FB_GET_AMAP_CONF,
                &fbGetAmapConfParams,
                sizeof(fbGetAmapConfParams)
                ));

        m_SwizzleParm.backingStoreVA = backingStoreVA;
        // RM should not change AllocPA; Sanity check to ensure we have
        // expected backingStoreAllocPA value.
        MASSERT(m_SwizzleParm.backingStoreAllocPA != 0 &&
            m_SwizzleParm.backingStoreAllocPA == backingStoreAllocPA);

        PrintSwizzleParm();

        m_pHandle = configNewAmapTrcV1(&m_CfgParm, &m_SwizzleParm);
        MASSERT(m_pHandle != 0);
        printAmapConfV1(m_pHandle);

        return rc;
    }

    // For debugging purpose
    void PrintSwizzleParm()
    {
        DebugPrintf("CBC: ---CbcSwizzleParamsV1---\n");
        DebugPrintf("cbcBaseAddress: 0x%x\n",
            m_SwizzleParm.cbcBaseAddress
            );
        DebugPrintf("compCacheLineSize: %u (0x%x)\n",
            m_SwizzleParm.compCacheLineSize,
            m_SwizzleParm.compCacheLineSize
            );
        DebugPrintf("backingStoreStartPA: 0x%llx\n",
            (UINT64)m_SwizzleParm.backingStoreStartPA,
            (UINT64)m_SwizzleParm.backingStoreStartPA
            );
        DebugPrintf("backingStoreAllocPA: 0x%llx\n",
            (UINT64)m_SwizzleParm.backingStoreAllocPA,
            (UINT64)m_SwizzleParm.backingStoreAllocPA
            );
        DebugPrintf("backingStoreVA: %p\n",
            m_SwizzleParm.backingStoreVA
            );
        DebugPrintf("backingStoreChunkOverfetch: 0x%x\n",
            m_SwizzleParm.backingStoreChunkOverfetch
            );
        DebugPrintf("CBC: ---CbcSwizzleParamsV1 END---\n");
    }

    ConfParamsV1 m_CfgParm;
    CbcSwizzleParamsV1 m_SwizzleParm;
};

RC LibWrapperImpl::GetCBCLib()
{
    RC rc;
    CompBits::Settings* pCfg  = m_pTest->GetSettings();
    CHECK_RC(SetContext(
        pCfg->GetBackingStoreAllocPA(),
        static_cast<UINT08*>(m_pTest->GetBackingStoreBar1Addr())
        ));

    pCfg->SetLtcCount(m_CfgParm.numPartitions);
    pCfg->SetSlicesPerLtc(m_CfgParm.numSlices);
    pCfg->InitAmapInfo();

    return rc;
}

////////////////////////////////////////////////

void SurfCopyTest::ClearBuffers()
{
    m_TileInfos.clear();
    m_TileInfos.resize(m_SurfCopier.GetDstSurf()->TileCount());
    fill(m_Chunk.begin(), m_Chunk.end(), 0);
    m_CompCacheLineTemp = m_Chunk;
    fill(m_CacheLineFetchBits.begin(), m_CacheLineFetchBits.end(), 0);
}

RC SurfCopyTest::SetupTest()
{
    RC rc;

    CHECK_RC(m_SurfCopier.Init(this, GetSurfCA(), true, true));

    m_Chunk.resize(m_Settings.GetComptagChunkSize() + m_Settings.GetCacheLineFetchAlignment() * 2);
    m_CacheLineFetchBits.resize(m_Settings.GetCachelineFetchSize());
    ClearBuffers();

    return rc;
}

void SurfCopyTest::CleanupTest()
{
    m_SurfCopier.ClearAllDstPageCompBits();
    m_SurfCopier.Clear();
}

RC SurfCopyTest::CompTagSnapshot()
{
    // Referencing source code in drivers/display/lddm/lwlddmkm/lwllwlPagingFermi.cpp, etc.

    RC rc;
    UINT64 startFBOffset = m_Settings.GetCachelineFBStart(m_SurfCopier.GetSrcSurf()->MinComptag());
    UINT64 endFBOffset = m_Settings.GetCachelineFBEnd(m_SurfCopier.GetSrcSurf()->MinComptag());

    ReadBackingStoreData(startFBOffset, endFBOffset - startFBOffset, &m_Chunk);

    return rc;
}

RC SurfCopyTest::CompTagPromotion()
{
    RC rc;

    UINT64 oldFBOffset = m_SurfCopier.GetSrcSurf()->GetPhyBase();
    UINT64 dstFBOffset = m_SurfCopier.GetDstSurf()->GetPhyBase();
    MASSERT(oldFBOffset != dstFBOffset);
    CompTagInfoV1 comptagInfo = {0};
    CompTagInfoV1 *pComptagInfo = &comptagInfo;
    pComptagInfo->pTileInfo = &m_TileInfos[0];
    pComptagInfo->pCacheWriteBitMap = &m_CacheLineFetchBits[0];

    // Referencing source code in drivers/display/lddm/lwlddmkm/lwllwlPagingFermi.cpp, etc.

    pComptagInfo->updateCount = 0;
    pComptagInfo->startComptagIndex = m_SurfCopier.GetSrcSurf()->MinComptag();
    pComptagInfo->startDstComptagIndex = m_SurfCopier.GetDstSurf()->MinComptag();
    pComptagInfo->useDstComptagIdx = true;
    pComptagInfo->pCompCacheLine    = &m_Chunk[0];
    pComptagInfo->pCompCacheLineTemp = &m_CompCacheLineTemp[0];
    TileInfoV1 *pTileInfo = pComptagInfo->pTileInfo;

    size_t off = 0;
    for (UINT16 tileIdx = 0;
        tileIdx < m_SurfCopier.GetDstSurf()->TileCount();
        tileIdx += CompBits::Settings::TilesPer64K, off += CompBits::Settings::Size64K)
    {
        pTileInfo->srcAddr = (oldFBOffset + off) >> 16;
        pTileInfo->dstAddr = (dstFBOffset + off) >> 16;
        MASSERT(pTileInfo->srcAddr != pTileInfo->dstAddr);
        const UINT32 comptagLine = m_SurfCopier.GetSrcSurf()->GetComptag(off);
        MASSERT(comptagLine >= pComptagInfo->startComptagIndex);
        pTileInfo->relComptagIndex = comptagLine - pComptagInfo->startComptagIndex;
        DebugPrintf("CBC: TileInfo 0x%x srcTileAddr 0x%x dstTileAddr 0x%x comptag 0x%x.\n",
            pTileInfo - pComptagInfo->pTileInfo,
            pTileInfo->srcAddr,
            pTileInfo->dstAddr,
            pTileInfo->relComptagIndex
            );
        pTileInfo ++;

        pComptagInfo->updateCount++;
    }
    if (m_Settings.GetCacheLineFetchAlignment() > 1)
    {
        //TODO: ClearAllBits(&cacheLineFetchBitmap);
        //Lwrrently don't understand that what should do when in fetch alignment > 1 case.
    }

    size_t backingStoreChunkOff = 0;
    size_t backingStoreSize = 0;
    CHECK_RC(PerformSwizzleCompBits(pComptagInfo, &backingStoreChunkOff, &backingStoreSize));
    vector<UINT08> data;
    data.resize(backingStoreSize);
    copy(m_Chunk.begin(), m_Chunk.begin() + backingStoreSize, data.begin());
    WriteBackingStoreData(backingStoreChunkOff, data);

    return rc;
}

RC SurfCopyTest::PerformSwizzleCompBits(CompTagInfoV1 *pComptagInfo,
    size_t *pBackingStoreChunkOff, size_t *pBackingStoreChunkSize)
{
    RC rc;

    CbcChunkParamsV1 swizzleChunkParams = {0};
    CbcChunkParamsV1 *pSwizzleChunkParams = &swizzleChunkParams;
    pSwizzleChunkParams->trcForceBar1Read = false;
    pSwizzleChunkParams->trcForceBar1Write = false;

    CompOperationImpl* pOps =
        dynamic_cast<CompOperationImpl*>(m_SurfCopier.GetSrcSurf()->GetCompOperation());
    MASSERT(pOps != 0);
    if (!pOps->Swizzle(pComptagInfo, pSwizzleChunkParams))
    {
        return RC::SOFTWARE_ERROR;
    }

    UINT64 startFBOffset = m_Settings.GetCachelineFBStart(pComptagInfo->startComptagIndex);
    UINT64 endFBOffset = m_Settings.GetCachelineFBEnd(pComptagInfo->startComptagIndex);

    *pBackingStoreChunkOff = startFBOffset;
    *pBackingStoreChunkSize = endFBOffset - startFBOffset;
    DebugPrintf("CBC: PerformSwizzleCompBits backingstore chunk Off 0x%lx size 0x%lx.\n",
        long(*pBackingStoreChunkOff),
        long(*pBackingStoreChunkSize)
        );

    return rc;
}

RC SurfCopyTest::Run(const vector<UINT08>& oldDecompData, bool bReverse)
{
    RC rc;

    CHECK_RC(CompTagSnapshot());
    CHECK_RC(m_SurfCopier.CopyData());
    CHECK_RC(CompTagPromotion());
    CheckpointBackingStore(bReverse ? "ReversePromotion" : "Promotion");
    // In Promotion() the WriteBackingStoreData performs Ilwalidate comptags. CHECK_RC(IlwalidateCompTags());

    if (m_MismatchOffsets.size() == 0)
    {
        ErrPrintf("CBC: no backing store bytes changed after swizzling the compression bits.\n");
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    vector<UINT08> newDecompData;

    // When reverse testing, the Dst is the orig Src
    CHECK_RC(SaveSurfaceDecompData(m_SurfCopier.GetDstSurf()->GetMdiagSurf(),
        &newDecompData));
    if (newDecompData != oldDecompData)
    {
        ErrPrintf("CBC: SurfCopyTest: Surface old & new decompressed data MISMATCH after %stest.\n",
            bReverse ? "reverse " : ""
            );
        CompBits::ArrayReportFirstMismatch<vector<UINT08> >(oldDecompData,
            newDecompData);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    InfoPrintf("CBC: SurfCopyTest: Surface old & new decompressed data MATCH after %stest.\n",
        bReverse ? "reverse " : ""
        );

    return rc;
}

RC SurfCopyTest::DoPostRender()
{
    RC rc;

    CHECK_RC(FlushCompTags());
    CheckpointBackingStore("Initial");

    vector<UINT08> oldDecompData;
    CHECK_RC(SaveSurfaceDecompData(GetSurfCA(), &oldDecompData));

    CHECK_RC(Run(oldDecompData, false));

    // Reverse Testing Start
    CHECK_RC(m_SurfCopier.ExchangeSurface());
    m_SurfCopier.ClearAllDstPageCompBits();
    ClearBuffers();
    CheckpointBackingStore("Clearing");
    CHECK_RC(IlwalidateCompTags());
    if (m_MismatchOffsets.size() == 0)
    {
        ErrPrintf("CBC: no backing store bytes changed after clearing the compression bits.\n");
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    CHECK_RC(Run(oldDecompData, true));

    InfoPrintf("CBC: SurfCopyTest PASS.\n");

    // Restore initial src/dst order
    CHECK_RC(m_SurfCopier.ExchangeSurface());

    return rc;
}

//////////////////////////Creators////////////////////////////////////////

CompBits::CBCLibWrapper* CreateCBCLibWrapper(CompBitsTest* pTest)
{
    return new LibWrapperImpl(pTest);
}

/////////////////////////////////////////////////////////////////////////
} // namespace CompBits_Pascal

namespace CompBits
{

CompBitsTest* TestCreator::CreatePascalTest(TraceSubChannel* pSubChan, const TestID testID)
{
    CompBitsTest* pTest = 0;
    // We have one Pascal specific test SurfCopyTest.
    if (tidSurfCopy == testID)
    {
        pTest = new CompBits_Pascal::SurfCopyTest(pSubChan);
    }
    else
    {
        pTest = CreateCommonTest(pSubChan, testID);
    }

    pTest->SetTestID(testID);
    pTest->SetCBCLib(CompBits_Pascal::CreateCBCLibWrapper(pTest));

    return pTest;
}

} // namespace CompBits

