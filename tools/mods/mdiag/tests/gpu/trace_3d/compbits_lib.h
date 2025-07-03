/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _COMPBITS_LIB_H_
#define _COMPBITS_LIB_H_

// Dependency eliminated:
// This header file shall not include any ohter files.

class CompBitsTest;

namespace CompBits
{

//
// CBC: Compression Bit Cache.
// TRC: Tiled Resources Compression.
// AMAP: Address Mapping drivers/common/amaplib/
//
// Definitions:
// Copied from drivers/display/lddm/lwlddmkm/lwlAllocation.h, etc.
//
// Cacheline = Comp cacheline per slice.
// Ctag = Comptag.
// 1x ComptagChunk = 1x totalCacheline (in FB) = Cacheline * Partitions * Slices.
// BackingStore = all totalCachelines stored in FB (GPU DRAM).
// CtagSysmemChunk = CPU copy of ComptagChunk cached in sysmem & used for compbits swizzling.
// ctagFBOffset = PA or VA offset of a Ctag in the actual backing store
//    (actual = in FB, not the CPU copy).
// ctagOffset = VA offset of a Ctag in a CtagSysmemChunk.
// Maybe tilesPerChunk == comptagsPerChunk (when 1x comptag per each tile)
//    also == comptagsPerTotalCacheline.
// Chunk.ctagStart = Comptag#.
// (Chunk.ctagStart / tilesPerChunk) =
//    (chunk.ctagStart / ctagsPerChunk) =
//    find the index of a Chunk (of a totalCacheline).

//|<- Start of the backing store.
//|<----------1 totalCacheline (chunk) size in bytes =  bytesPerChunks---------------->|
//|------------------------------------------------------------------------------------|
//|------------------------------------------------------------------------------------|
//|------------------------------------------------------------------------------------|

enum TRCChipFamily
{
    PrePascal,
    PascalAndLater,
    Unsupported
};

extern TRCChipFamily GetTRCChipFamily(const TraceSubChannel* pSubChan);

// Maxwell: 3 bits per compbits.
// Pascal: 1 byte per GOB.
// So we use 1-byte should be OK for all known tests.
const UINT08 COMPBITS_MASK = 0xff;
inline UINT32 CompBitsMaskedValue(UINT32 compbits)
{
    return compbits & COMPBITS_MASK;
}
inline bool CompBitsCompare(UINT32 compbitsA, UINT32 compbitsB)
{
   return CompBitsMaskedValue(compbitsA) == CompBitsMaskedValue(compbitsB);
}

/////////////////////////Test ID///////////////////////////

enum TestID
{
    tidUnitTest,        // Unit tests
    tidTest1,           // Single Tile Test1
    tidTest2,           // Single Tile Test2
    tidSurfMove,        // Surface Move Test
    tidSurfCopy,        // Surface Copy Test
    tidCheckOffset,     // Check Offset Test

    tidUndefined
};

////////////////Settings/////////////////

// Tests won't pass unless verified CBC configuration from RegVals,
// RM Info and AMAPLib all match.
// Also need to save some users specified test settings for use.
class Settings
{
public:
    static const UINT32 SectorSize = 32;
    static const UINT32 TileSize = 256;
    static const UINT32 GobSize = TileSize * 2;
    static const UINT32 Size64K = (64 << 10);
    static const UINT32 GobsPer64K = Size64K / GobSize;
    static const UINT32 TilesPer64K = Size64K / TileSize;

    class AmapInfo
    {
    public:
        size_t m_ChunkSize;
        UINT32 m_ComptagsPerChunk;
        UINT16 m_GobsPerComptagPerSlice;

        AmapInfo();
        void Init(size_t numParts, size_t numSlices, size_t cachelineSize);

    protected:
        friend class CompBits::Settings;

        // Per slice cacheline size
        size_t GetCachelineSizeFromComptags(size_t comptags) const
        {
            return comptags * m_GobsPerComptagPerSlice;
        }
        static size_t GetComptagsFromCachelineSize(size_t parts,
            size_t slices, size_t clSize);

    };

    Settings();

    void SetTRCChipFamily(const TRCChipFamily chipFamily)
    {
        m_TRCChipFamily = chipFamily;
    }
    TRCChipFamily GetTRCChipFamily() const { return m_TRCChipFamily; }

    // Access basis: per tile or per GOB, etc.
    // Used when putting/getting a 64K pages compbits or in CheckOffsetTest.
    size_t GetCompBitsAccessBasis() const { return m_Basis; }

    RC Init(LWGpuResource* pGpuRes, LwRm* pLwRm);
    void SetTestName(const char *pName) { m_pTestName = pName; }

    size_t GetComptagChunkSize() const { return m_RMInfo.cacheLineSize; }
    size_t GetComptagsPerChunk() const { return m_RMInfo.comptagsPerCacheLine; }
    size_t GetCacheLineFetchAlignment() const { return m_RMInfo.cacheLineFetchAlignment; }
    UINT64 GetBackingStoreAllocPA() const { return m_RMInfo.Address; }
    UINT64 GetBackingStoreStartPA() const { return m_RMInfo.backingStoreBase; }
    size_t GetBackingStoreSize() const { return m_RMInfo.Size; }
    UINT32 GetCachelineSize() const { return m_RegVals.m_CachelineSize; }

    // Maxwell case needs to overwrite cacheline size.
    void SetMaxwellCachelineSize(size_t size) { m_MaxwellCachelineSize = size; }
    size_t GetMaxwellCachelineSize() { return m_MaxwellCachelineSize; }

    UINT32 GetCBCBase() const { return m_RegVals.m_CbcBase;; }

    void SetLtcCount(size_t count) { m_NumLtcs = count; }
    size_t GetLtcCount() const { return m_NumLtcs; }
    void SetSlicesPerLtc(size_t count) { m_SlicesPerLtc = count; }
    size_t GetMaxSlicesPerLtc() const { return m_SlicesPerLtc; }
    void InitAmapInfo()
    {
        m_AmapInfo.Init(GetLtcCount(), GetMaxSlicesPerLtc(),
            m_RegVals.m_CachelineSize);
    }

    // Comptag/Cacheline offset in FB.
    size_t GetCachelineFBStart(size_t ctagIdx) const
    {
        return (GetComptagFBOffset(ctagIdx) / m_RMInfo.cacheLineFetchAlignment) *
            m_RMInfo.cacheLineFetchAlignment;
    }
    size_t GetCachelineFBEnd(size_t ctagIdx) const;
    size_t GetCachelineFetchSize() const;

    // Perform a Check Config Test.
    RC Check(const AmapInfo* pInfo) const;

    // RM current only support 0 or 2.
    UINT08 GetComptagsPerTile() const;

private:

    struct RegVals
    {
        UINT32 m_CbcBase;
        UINT32 m_ComptagsPerChunk;
        // Per slice cacheline size.
        UINT32 m_CachelineSize;
        UINT16 m_GobsPerComptagPerSlice;

        RegVals();
        size_t GetComptagChunkSize(size_t numParts, size_t numSlices) const
        {
            return m_CachelineSize * numParts * numSlices;
        }

        void Print() const;
    };

    struct RMInfo
        : public LW0080_CTRL_FB_GET_COMPBIT_STORE_INFO_PARAMS
    {
        RMInfo();
        void Print() const;
        UINT32 GetPerSliceCachelineSize(size_t numParts, size_t numSlices) const
        {
            return cacheLineSize / numParts / numSlices;
        }
    };

    int BasicCheck() const;
    RC ReportErrors(int errors) const;

    // Coupled CBC base address and backing store base address (StartPA) pair.

    int VerifyCoupledBasesOfCBCAndBackingStore() const;
    int VerifyBackingStoreBase(UINT32 cbcBase,
        UINT64 expVal, UINT64 actVal) const;
    int VerifyCBCBase(UINT64 bsBase, UINT32 expVal, UINT32 actVal) const;
    static UINT64 CeilPos(UINT64 X) { return ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X)); }
    static UINT64 Ceil(UINT64 X) { return (((X) > 0) ? CeilPos(X) : (int)(X)); }
    UINT32 GetCBCBaseFromBackingStoreBase() const
    {
        // StartPA / (Partitions << 11)
        return (UINT32)Ceil(m_RMInfo.backingStoreBase / (GetLtcCount() << 11));
    }
    UINT64 GetBackingStoreBaseFromCBCBase() const
    {
        return m_RegVals.m_CbcBase * (GetLtcCount() << 11);
    }

    // Verifying RegVals, RMInfo and AMAPInfo

    int VerifyRegValsAndRMInfo() const;
    int VerifyRegValsAndRMInfoComptagsPerChunk() const;
    int VerifyRegValsAndRMInfoPerSliceCachelineSize() const;
    int VerifyRegValsAndRMInfoComptagChunkSize() const;

    int VerifyRegValsAndAmapInfo(const AmapInfo& info) const;

    int VerifyRMInfoAndAmapInfo(const AmapInfo& info) const;
    int VerifyRMInfoAndAmapInfoComptagChunkSize(size_t chunkSize) const;
    int VerifyRMInfoAndAmapInfoComptagsPerChunk(size_t comptags) const;
    int VerifyRMInfoAndAmapInfoCachelineSizeFromComptags() const;

    size_t GetComptagFBOffset(size_t ctagIdx) const
    {
        // Start offset of BackingStore VA == 0, so ignored in below line.
        return m_RMInfo.cacheLineSize * (ctagIdx / m_RMInfo.comptagsPerCacheLine);
    }

    RegVals m_RegVals;
    RMInfo m_RMInfo;
    AmapInfo m_AmapInfo;
    const char* m_pTestName;
    UINT32 m_MaxwellCachelineSize;
    TRCChipFamily m_TRCChipFamily;
    UINT32 m_NumLtcs;
    UINT16 m_SlicesPerLtc;
    UINT16 m_Basis;

};

// Only for debugging purpose, printing first 256 items despite of
// matching or not.
template<typename T>
inline void ArrayReportFirstMismatch(const T& src, const T& dst)
{
        for (size_t i = 0; i < min(src.size(), size_t(Settings::TileSize)); i++)
        {
            if (src[i] != dst[i])
            {
                ErrPrintf("CBC: Array item values @ offset 0x%08lx of source & dest MISMATCH, 0x08%x vs 0x08%x.\n",
                    long(i), src[i], dst[i]);
            }
        }
}

/////////////////CBC lib wrapper//////////////////////

class TestSurf;

// A SurfCompOperation to be used for putting/getting compbits of a surface.
class SurfCompOperation;

//
// CBCLibWrapper wrapping CBC lib (RM or AMAP) configuring/initialization APIs.
// Pascal or Pre-Pascal requires its own CBC lib wrapper implementation.
// For tests, only an abstract lib wrapper is visible to avoid from handling
// libs difference or specific details.
//
// It's a 1on1 relationship between CompBtisTest and CBC lib wrapper.
//
class CBCLibWrapper
{
public:
    CBCLibWrapper(CompBitsTest* pTest)
        :
        m_pTest(pTest),
        m_pHandle(0)
    {}
    virtual ~CBCLibWrapper(){}

    // Init the wrapper.
    virtual RC Init() = 0;

    // Init lib & fetch lib's handle.
    virtual RC GetCBCLib() = 0;
    // Fetch a CompOperation for a surface.
    virtual SurfCompOperation* FetchCompOperation(TestSurf* pSurf) = 0;
    virtual void ReleaseCompOperation(SurfCompOperation* pOps) = 0;

    virtual Settings::AmapInfo* GetAmapInfo() { return 0; }

public:
    // Share lib handle with CompOperation, etc
    void* GetLibHandle() { return m_pHandle; }

protected:
    CompBitsTest* m_pTest;
    // CBC lib handle
    void* m_pHandle;
};

/////////////////SurfCompOperation//////////////////////////

//
// Ether each per offset compbits getting/putting operation or each per
// page compbits getting/putting operation shall be always performed on a
// surface (except hacking purpose), that is TestSurf within our context.
//
// SurfCompOperation(s) opened by the CBCLibWrapper is just another abstraction
// for such purpose.
//
// It's also a 1on1 relationship between TestSurf and SurfCompOperation.
//
class SurfCompOperation
{
public:
    SurfCompOperation(CBCLibWrapper* pLibWrapper, TestSurf* pTestSurf)
        :
        m_pCBCLib(pLibWrapper),
        m_pTestSurf(pTestSurf)
    {}
    virtual ~SurfCompOperation(){}

    // Per offset basis
    virtual void GetCompBits(size_t offset, UINT32* pCompbits) = 0;
    virtual void PutCompBits(size_t offset, UINT32 compbits) = 0;

    // Per 64K page basis, use tilesPer64K to add enough room for
    // a couple 512B comptag cachelines
    virtual void GetPageCompBits(size_t pageIdx,
        UINT32 pCompbits[Settings::TilesPer64K]) = 0;
    virtual void PutPageCompBits(size_t pageIdx,
        const UINT32 pCompbits[Settings::TilesPer64K]) = 0;

public:
    // Per offset basis

    // Write and verify bits
    RC WrVerifyCompBits(size_t offset, UINT32 compbits);

    // Copy compbits of source surface to dest surface.
    // @return compbits just copied for further use.
    UINT32 CopyCompBits2(size_t offset, SurfCompOperation* pDst)
    {
        UINT32 compbits = 0;
        GetCompBits(offset, &compbits);
        pDst->PutCompBits(offset, compbits);
        return compbits;
    }

    // Read and write and verify bits
    RC CopyAndVerifyCompBits2(size_t offset, SurfCompOperation* pDst);

    void ClearCompBits(size_t offset) { PutCompBits(offset, 0); }

    // Per 64K page basis

    void CopyPageCompBits2(size_t pageIdx, SurfCompOperation* pDst)
    {
        UINT32 pCompbits[Settings::TilesPer64K];
        GetPageCompBits(pageIdx, pCompbits);
        pDst->PutPageCompBits(pageIdx, pCompbits);
    }

    // Read and write and verify bits, per page
    RC CopyAndVerifyPageCompBits2(size_t pageIdx, SurfCompOperation* pDst);

    void ClearPageCompBits(size_t pageIdx)
    {
        UINT32 pCompbits[Settings::TilesPer64K];
        memset(pCompbits, 0, sizeof pCompbits);
        PutPageCompBits(pageIdx, pCompbits);
    }

    // A revised version of the above ClearPageCompBits when zero
    // bits has been provided already.
    void ClearPageCompBits(size_t pageIdx,
        UINT32 pCompbits[Settings::TilesPer64K])
    {
        PutPageCompBits(pageIdx, pCompbits);
    }

protected:
    CBCLibWrapper* m_pCBCLib;
    TestSurf* m_pTestSurf;
};

////////////////////////CBC Surface//////////////////////////

class TestSurf
{
public:
    TestSurf();
    ~TestSurf();
    RC Init(CompBitsTest* pTest, MdiagSurf* pSurf,
        bool bProbeCvg, bool bCheck);
    void Clear();
    void SetCompOperation(SurfCompOperation* pOps)
    {
        m_pCompOperation = pOps;
    }
    SurfCompOperation* GetCompOperation() { return m_pCompOperation; }

    // Cursor could be offset and/or page index.
    // Some quick Get/Put CompBits by using internal CBC Session,
    // when the cursor (offset) has been set already.

    // Per offset basis
    void GetCompBits(UINT32* pCompbits)
    {
        MASSERT(GetCompOperation() != 0);
        GetCompOperation()->GetCompBits(GetOffset(), pCompbits);
    }
    void PutCompBits(UINT32 compbits)
    {
        MASSERT(GetCompOperation() != 0);
        GetCompOperation()->PutCompBits(GetOffset(), compbits);
    }
    RC WrVerifyCompBits(UINT32 compbits)
    {
        MASSERT(GetCompOperation() != 0);
        return GetCompOperation()->WrVerifyCompBits(GetOffset(), compbits);
    }

    // CompCvg: Compression coverage, how many Comptags, in a range
    // [MinCtag#, MaxCtag#], have been allocated for a surface.
    // Each compressed surface has its own range of Comptags.
    void SetCompCvgHandle(LwRm::Handle handle) { m_CompCvgHandle = handle; }
    RC ProbeCompCvg(const char* pDesc, bool bCheck);
    UINT32 MaxComptag() const { return m_CompCvg.lineMax; }
    UINT32 MinComptag() const { return m_CompCvg.lineMin; }
    UINT32 GetComptag(size_t offset) const;
    bool Upper64KBSel(size_t offset) const
    {
        return (GetVirtAddr(offset) & Settings::Size64K) != 0;
    }
    void CompCvgPrint(const char* pDesc) const;

    void SetOffset(size_t off)
    {
        m_SurfOff = off;
        m_PageIdx = off / CompBits::Settings::Size64K;
    }
    size_t GetOffset() const { return m_SurfOff; }
    void SetPageIndex(size_t pageIdx)
    {
        m_PageIdx = pageIdx;
        m_SurfOff = GetPageOffset(m_PageIdx);
    }
    UINT32 GetPageIndex() const { return m_PageIdx; }
    size_t GetPageOffset(size_t pageIdx)
    {
        return pageIdx * CompBits::Settings::Size64K;
    }

    UINT64 GetPhyBase() const { return m_SurfBase; }
    UINT64 GetVirtBase() const { return m_SurfVirtBase; }
    UINT64 GetPhyAddr() const { return m_SurfBase + m_SurfOff; }
    UINT64 GetVirtAddr() const { return GetVirtAddr(m_SurfOff); }
    UINT64 GetVirtAddr(size_t offset) const { return m_SurfVirtBase + offset; }
    UINT64 GetSize() const { return m_Size; }
    size_t PageCount() const { return (GetSize() + 0xffff) / 0x10000; }
    size_t TileCount() const { return PageCount() * Settings::TilesPer64K; }

    // Creating a per slice cacheline buffer.
    UINT32* CreateCachelineBuffer();

    MdiagSurf* GetMdiagSurf() const { return m_pSurf; }

    RC Map() const;
    void Unmap() const;
    RC Write(const vector<UINT08>& data) const;
    RC MapAndWrite(const vector<UINT08>& data) const;

private:
    // Use FinedOffset to be compatible with Maxwell strange behavior.
    // See also GetRWAddr_Maxwell.
    ssize_t FineOffset(size_t dataSize) const;
    ssize_t GetRWAddr_Maxwell(UINT64 in, size_t size) const;
    ssize_t GetRWAddr(UINT64 in, size_t size) const;
    bool CheckCompCvgFormat() const;

    LW0041_CTRL_GET_SURFACE_COMPRESSION_COVERAGE_PARAMS m_CompCvg;
    LwRm::Handle m_CompCvgHandle;
    CompBitsTest* m_pTest;
    MdiagSurf* m_pSurf;
    SurfCompOperation* m_pCompOperation;
    // Surface physical base addr == vidmem offset.
    UINT64 m_SurfBase;
    // Surface physical offset == offset to vidmem offset.
    UINT64 m_SurfOff;
    UINT64 m_SurfVirtBase;
    UINT64 m_Size;
    UINT32 m_PageIdx;
};

////////////////////CBC Surface Copier//////////////////

//
// A surface copier accomplishes the following:
// 1. Replicate a new surface (dest surface) having the same attributes
//    as source surface;
// 2. Copy data from source surface to the dest surface;
// 3. Swizzle compbits of source surface for the dest surface;
// 4. TODO Later:
//    Move source or dest surface or both to new FB locations.
//
class SurfCopier
{
public:
    SurfCopier()
        :
        m_pTest(0),
        m_pSrcSurf(0),
        m_pDstSurf(0),
        m_pOrigDstSurf(0)
    {}

    RC Init(CompBitsTest* pTest, MdiagSurf* pSurf, bool bProbeCvg, bool bCheck);
    void Clear();
    RC CopyData();
    TestSurf* GetSrcSurf() { return &m_Src; }
    TestSurf* GetDstSurf() { return &m_Dst; }
    // Exchange MdiagSurf of the two TestSurf attached
    // with each other.
    RC ExchangeSurface();

    RC CopyAndVerifyAllCompBits();
    void CopyAllCompBits();

    void CopyAllPageCompBits()
    {
        const size_t pageCount = m_Dst.PageCount();
        for (size_t i = 0; i < pageCount; i++)
        {
            CopyPageCompBits(i);
        }
    }

    void ClearAllDstCompBits();

    void ClearAllDstPageCompBits()
    {
        UINT32 pCompbits[Settings::TilesPer64K];
        memset(pCompbits, 0, sizeof pCompbits);
        const size_t pageCount = m_Dst.PageCount();
        for (size_t i = 0; i < pageCount; i++)
        {
            ClearDstPageCompBits(i, pCompbits);
        }
    }

private:
    void SetOffset(size_t off)
    {
        m_Src.SetOffset(off);
        m_Dst.SetOffset(off);
    }
    size_t GetOffset() const { return m_Dst.GetOffset(); }
    void SetPageIndex(UINT32 pageIdx)
    {
        m_Src.SetPageIndex(pageIdx);
        m_Dst.SetPageIndex(pageIdx);
    }
    UINT32 GetPageIndex() const { return m_Dst.GetPageIndex(); }
    size_t GetSize() const { return m_Dst.GetSize(); }

    RC CopyAndVerifyCompBits(size_t offset)
    {
        return m_Src.GetCompOperation()->CopyAndVerifyCompBits2(offset,
            m_Dst.GetCompOperation());
    }
    void CopyCompBits(size_t offset)
    {
        m_Src.GetCompOperation()->CopyCompBits2(offset, m_Dst.GetCompOperation());
    }

    void CopyPageCompBits(size_t pageIdx)
    {
        m_Src.GetCompOperation()->CopyPageCompBits2(pageIdx,
            m_Dst.GetCompOperation());
    }

    void ClearDstCompBits(size_t offset)
    {
        m_Dst.GetCompOperation()->ClearCompBits(offset);
    }

    void ClearDstPageCompBits(size_t pageIdx)
    {
        m_Dst.GetCompOperation()->ClearPageCompBits(pageIdx);
    }

    void ClearDstPageCompBits(size_t pageIdx,
        UINT32 pCompbits[Settings::TilesPer64K])
    {
        m_Dst.GetCompOperation()->ClearPageCompBits(pageIdx, pCompbits);
    }

    TestSurf m_Src;
    TestSurf m_Dst;
    CompBitsTest* m_pTest;
    MdiagSurf* m_pSrcSurf;
    MdiagSurf* m_pDstSurf;
    MdiagSurf* m_pOrigDstSurf;
};

} // namespace CompBits

#endif

