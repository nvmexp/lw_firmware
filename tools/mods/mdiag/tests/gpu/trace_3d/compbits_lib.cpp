/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/rc.h"
#include "gpu/include/gpudev.h"
#include "mdiag/utils/utils.h"
#include "core/include/utility.h"
#include "tracesubchan.h"
#include "tracechan.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"
// LW_PLTCG_LTCS_LTSS_CBC_PARAM_CACHE_LINE_SIZE
// LW_PLTCG_LTC0_LTS0_CBC_PARAM2_GOBS_PER_COMPTAGLINE_PER_SLICE
#include "volta/gv100/dev_ltc.h"
#include "Lwcm.h"

#include "compbits.h"
#include "mdiag/utils/lwgpu_classes.h"

//////////////////////////////////////////////////////////

namespace CompBits
{

TRCChipFamily GetTRCChipFamily(const TraceSubChannel* pSubChan)
{
    if (EngineClasses::IsGpuFamilyClassOrLater(
        pSubChan->GetClassNum(), LWGpuClasses::GPU_CLASS_KEPLER))
    {
        if (EngineClasses::IsGpuFamilyClassOrLater(
            pSubChan->GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_A))
        {
            return PascalAndLater;
        }
        return PrePascal;
    }
    ErrPrintf("MODS TRC Tests: Unsupoorted device class %x.\n",
        pSubChan->GetClass());
    return Unsupported;
}

//////////////////Settings//////////////////////

Settings::AmapInfo::AmapInfo()
    : m_ChunkSize(0),
    m_ComptagsPerChunk(0),
    m_GobsPerComptagPerSlice(0)
{
}

void Settings::AmapInfo::Init(size_t parts, size_t slices, size_t cachelineSize)
{
    size_t gobs_per_comptagline = (CEIL_DIV((64 / 2), parts)) * 4;
    m_GobsPerComptagPerSlice = (UINT16)CEIL_DIV(gobs_per_comptagline , slices);
    m_ComptagsPerChunk = cachelineSize / m_GobsPerComptagPerSlice;
    m_ChunkSize = cachelineSize * parts * slices;

    DebugPrintf("CBC: ------Expected AMAP Callwlation--------\n");
    DebugPrintf("GOBs per comptag per slice: %u\n",
        int(m_GobsPerComptagPerSlice)
        );
    DebugPrintf("Comptags per Chunk: %u\n",
        int(m_ComptagsPerChunk)
        );
    DebugPrintf("Chunk size: %lu (0x%lx)\n",
        long(m_ChunkSize),
        long(m_ChunkSize)
        );
    DebugPrintf("CBC: ------Expected AMAP Callwlation END--------\n");
}

size_t Settings::AmapInfo::GetComptagsFromCachelineSize(size_t parts,
    size_t slices, size_t clSize)
{
    AmapInfo info;
    info.Init(parts, slices, clSize);
    return info.m_ComptagsPerChunk;
}

Settings::Settings()
    : m_pTestName(0),
    m_MaxwellCachelineSize(0),
    m_TRCChipFamily(Unsupported),
    m_NumLtcs(0),
    m_SlicesPerLtc(0),
    m_Basis(GobSize)
{
}

UINT08 Settings::GetComptagsPerTile() const
{
    return LWOS32_ALLOC_COMPR_COVG_BITS_2;
}

int Settings::VerifyBackingStoreBase(UINT32 cbcBase,
    UINT64 expVal, UINT64 actVal) const
{
    if (expVal == actVal)
    {
        InfoPrintf("%s:   CBC base address 0x%x -> backing store base address: "
            "expected 0x%llx actual 0x%llx - MATCH.\n",
            m_pTestName, cbcBase, expVal, actVal);
        return 0;
    }
    ErrPrintf("%s:   CBC base address 0x%x -> backing store base address: "
        "expected 0x%llx actual 0x%llx - MISMATCH.\n",
        m_pTestName, cbcBase, expVal, actVal);
    return 1;
}

int Settings::VerifyCBCBase(UINT64 bsBase,
    UINT32 expVal, UINT32 actVal) const
{
    if (expVal == actVal)
    {
        InfoPrintf("%s:   Backing store base address 0x%llx -> CBC base address: "
            "expected 0x%x actual 0x%x - MATCH.\n",
            m_pTestName, bsBase, expVal, actVal);
        return 0;
    }
    InfoPrintf("%s:   Backing store base address 0x%llx -> CBC base address: "
        "expected 0x%x actual 0x%x - MISMATCH.\n",
        m_pTestName, bsBase, expVal, actVal);
    return 1;
}

int Settings::VerifyCoupledBasesOfCBCAndBackingStore() const
{
    MASSERT(m_pTestName != 0);

    int errors = 0;
    // Expected backing store base.
    UINT64 expBsBase = GetBackingStoreBaseFromCBCBase();
    // Actual backing store base.
    UINT64 actBsBase = GetBackingStoreStartPA();

    UINT32 expCbcBase = GetCBCBaseFromBackingStoreBase();
    UINT32 actCbcBase = m_RegVals.m_CbcBase;

    errors += VerifyBackingStoreBase(actCbcBase, expBsBase, actBsBase);
    errors += VerifyCBCBase(actBsBase, expCbcBase, actCbcBase);

    return errors;
}

Settings::RegVals::RegVals()
    : m_CbcBase(0),
    m_ComptagsPerChunk(0),
    m_CachelineSize(0),
    m_GobsPerComptagPerSlice(0)
{
}

void Settings::RegVals::Print() const
{
    DebugPrintf("CBC: ------RegVals--------\n");
    DebugPrintf("CompCacheLine size %u\n",
        int(m_CachelineSize)
        );
    DebugPrintf("Comptags per Chunk %u\n",
        int(m_ComptagsPerChunk)
        );
    DebugPrintf("GOBs per comptag per slice %u\n",
        int(m_GobsPerComptagPerSlice)
        );
    DebugPrintf("CBC base address 0x%x\n",
        m_CbcBase
        );
    DebugPrintf("CBC: ----RegVals END--------\n");
}

Settings::RMInfo::RMInfo()
{
    Size = 0;
    Address = 0;
    MaxCompbitLine = 0;
    comptagsPerCacheLine = 0;
    cacheLineSize = 0;
    backingStoreBase = 0;
    cacheLineFetchAlignment = 0;
}

void Settings::RMInfo::Print() const
{
    DebugPrintf("CBC: ------CompBit Store Info--------\n");
    DebugPrintf("store size: %lu (0x%lx)\n",
        long(Size),
        long(Size)
        );
    DebugPrintf("compbit store addr (AllocPA): 0x%llx\n",
        Address
        );
    DebugPrintf("max compbit line: %u (0x%x)\n",
        MaxCompbitLine,
        MaxCompbitLine
        );
    DebugPrintf("comptags per cacheline (comptagsPerChunk): %u (0x%x)\n",
        comptagsPerCacheLine,
        comptagsPerCacheLine
        );
    DebugPrintf("cacheline size (ChunkSize): %u (0x%x)\n",
        cacheLineSize,
        cacheLineSize
        );
    DebugPrintf("store base (StartPA): 0x%llx, physical, actual offset\n",
        backingStoreBase
        );
    DebugPrintf("cacheline fetch alignment: 0x%x\n",
        cacheLineFetchAlignment
        );
    DebugPrintf("CBC: ------CompBit Store Info End----\n");
}

RC Settings::Init(LWGpuResource *pGpuRes, LwRm* pLwRm)
{
    RC rc;
    GpuDevice *pGpuDev = pGpuRes->GetGpuDevice();

    // RM Info, BackingStore info
    LW0080_CTRL_FB_GET_COMPBIT_STORE_INFO_PARAMS* pParm;

    // Get the POD part of the RMInfo type object.
    pParm = &m_RMInfo;
    CHECK_RC(pLwRm->Control(
            pLwRm->GetDeviceHandle(pGpuDev),
            LW0080_CTRL_CMD_FB_GET_COMPBIT_STORE_INFO,
            pParm, sizeof(*pParm)));
    m_RMInfo.Print();
    if (m_RMInfo.AddressSpace != LW0080_CTRL_CMD_FB_GET_COMPBIT_STORE_INFO_ADDRESS_SPACE_FBMEM)
    {
        ErrPrintf("MODS only supports backing store mappings in FB!");
        return RC::SOFTWARE_ERROR;
    }

    // StartPA >= AllocPA
    MASSERT(m_RMInfo.backingStoreBase >= m_RMInfo.Address);

    // Registers
    GpuSubdevice *gpuSubdevice = pGpuDev->GetSubdevice(0);
    m_RegVals.m_CachelineSize = gpuSubdevice->GetCompCacheLineSize();
    DebugPrintf("CBC: per slice cacheline size %lu.\n",
        long(m_RegVals.m_CachelineSize)
        );
    m_RegVals.m_ComptagsPerChunk = gpuSubdevice->GetComptagLinesPerCacheLine();
    m_RegVals.m_GobsPerComptagPerSlice
        = DRF_VAL(_PLTCG, _LTC0_LTS0_CBC_PARAM2, _GOBS_PER_COMPTAGLINE_PER_SLICE,
            pGpuRes->RegRd32(LW_PLTCG_LTC0_LTS0_CBC_PARAM2));
    m_RegVals.m_CbcBase = gpuSubdevice->GetCbcBaseAddress();
    m_RegVals.Print();

    return rc;
}

size_t Settings::GetCachelineFBEnd(size_t ctagIdx) const
{
    return ALIGN_UP(GetComptagFBOffset(ctagIdx) + m_RMInfo.cacheLineSize,
        (unsigned int)m_RMInfo.cacheLineFetchAlignment);
}

size_t Settings::GetCachelineFetchSize() const
{
    return (GetCacheLineFetchAlignment() > 1) ?
        ALIGN_UP((GetComptagChunkSize() + GetCacheLineFetchAlignment() * 2 + 128 * 8) / (128 * 8),
            sizeof(UINT64))
        : 0;
}

int Settings::VerifyRegValsAndRMInfoPerSliceCachelineSize() const
{
    MASSERT(m_pTestName != 0);

    int expSize = m_RMInfo.GetPerSliceCachelineSize(GetLtcCount(), GetMaxSlicesPerLtc());
    int actSize = m_RegVals.m_CachelineSize;

    if (expSize == actSize)
    {
        InfoPrintf("CBC: %s: Based on RM info, Expected per slice cacheline size: "
            "%u (0x%x), RegVals actual %u (0x%x) - MATCH.\n",
            m_pTestName,
            expSize, expSize,
            actSize, actSize
            );
        return 0;
    }
    ErrPrintf("CBC: %s: Based on RM info, Expected per slice cacheline size: "
        "%u (0x%x), RegVals actual %u (0x%x) - MISMATCH.\n",
        m_pTestName,
        expSize, expSize,
        actSize, actSize
        );
    return 1;
}

int Settings::VerifyRegValsAndRMInfoComptagChunkSize() const
{
    MASSERT(m_pTestName != 0);
    long expSize = m_RegVals.GetComptagChunkSize(GetLtcCount(), GetMaxSlicesPerLtc());
    long actSize = m_RMInfo.cacheLineSize;

    if (expSize == actSize)
    {
        InfoPrintf("CBC: %s: Based on RegVals, Expected Chunk (total cacheline) size: "
            "%lu (0x%lx), RM info actual %lu (0x%lx) - MATCH.\n",
            m_pTestName,
            expSize, expSize,
            actSize, actSize
            );
        return 0;
    }
    ErrPrintf("CBC: %s: Based on RegVals, Expected Chunk size (total cacheline) size: "
        "%lu (0x%lx), RM info actual %lu (0x%lx) - MISMATCH.\n",
        m_pTestName,
        expSize, expSize,
        actSize, actSize
        );
    return 1;
}

int Settings::VerifyRegValsAndRMInfoComptagsPerChunk() const
{
    MASSERT(m_pTestName != 0);
    long regVal = m_RegVals.m_ComptagsPerChunk;
    long rmVal = m_RMInfo.comptagsPerCacheLine;
    if (regVal == rmVal)
    {
        InfoPrintf("CBC: %s: comptags per cacheline values of RM StoreInfo and RegVals MATCH, "
            "%lu == %lu.\n",
            m_pTestName, rmVal, regVal);
        return 0;
    }
    ErrPrintf("CBC: %s: comptags per cacheline values of RM StoreInfo and RegVals MISMATCH, "
        "%lu != %lu.\n",
        m_pTestName, rmVal, regVal);
    return 1;
}

int Settings::VerifyRegValsAndRMInfo() const
{
    int errors = 0;

    errors += VerifyRegValsAndRMInfoComptagsPerChunk();
    errors += VerifyRegValsAndRMInfoComptagChunkSize();
    errors += VerifyRegValsAndRMInfoPerSliceCachelineSize();

    return errors;
}

int Settings::VerifyRegValsAndAmapInfo(const AmapInfo& info) const
{
    int errors = 0;
    UINT32 regVal = m_RegVals.m_ComptagsPerChunk;
    if (regVal != info.m_ComptagsPerChunk)
    {
        ErrPrintf("CBC: %s: comptags per Chunk values of RegVals and AmapInfo MISMATCH, "
            "%u != %u.\n",
            m_pTestName,
            regVal,
            info.m_ComptagsPerChunk
            );
        ++ errors;
    }
    else
    {
        InfoPrintf("CBC: %s: comptags per Chunk values of RegVals and AmapInfo MATCH, "
            "%u == %u.\n",
            m_pTestName,
            regVal,
            info.m_ComptagsPerChunk
            );
    }
    regVal = m_RegVals.m_GobsPerComptagPerSlice;
    if (regVal != info.m_GobsPerComptagPerSlice)
    {
        ErrPrintf("CBC: %s: GOGs per comptag per sice values of RegVals and AmapInfo MISMATCH, "
            "%u != %u.\n",
            m_pTestName,
            regVal,
            info.m_GobsPerComptagPerSlice
            );
        ++ errors;
    }
    else
    {
        InfoPrintf("CBC: %s: GOGs per comptag per sice values of RegVals and AmapInfo MATCH, "
            "%u == %u.\n",
            m_pTestName,
            regVal,
            info.m_GobsPerComptagPerSlice
            );
    }

    return errors;
}

int Settings::VerifyRMInfoAndAmapInfoComptagChunkSize(size_t chunkSize) const
{
    long expSize = m_RMInfo.cacheLineSize;
    long actSize = chunkSize;
    if (expSize == actSize)
    {
        InfoPrintf("CBC: %s: (RM Info) Expected Chunk size: %lu (0x%lx), "
            "amaplib actual %lu (0x%lx) - MATCH.\n",
            m_pTestName,
            expSize, expSize,
            actSize, actSize
            );
        return 0;
    }
    ErrPrintf("CBC: %s: (RM Info) Expected Chunk size: %lu (0x%lx), "
        "amaplib actual %lu (0x%lx) - MISMATCH.\n",
        m_pTestName,
        expSize, expSize,
        actSize, actSize
        );
    return 1;
}

int Settings::VerifyRMInfoAndAmapInfoComptagsPerChunk(size_t comptags) const
{
    long expSize = m_RMInfo.comptagsPerCacheLine;
    long actSize = comptags;

    if (expSize == actSize)
    {
        InfoPrintf("CBC: %s: (RM Info) Expected comptags per Chunk: %lu (0x%lx), "
            "amaplib actual %lu (0x%lx) - MATCH.\n",
            m_pTestName,
            expSize, expSize,
            actSize, actSize
            );
        return 0;
    }
    ErrPrintf("CBC: %s: (RM Info) Expected comptags per Chunk: %lu (0x%lx), "
        "amaplib actual %lu (0x%lx) - MISMATCH.\n",
        m_pTestName,
        expSize, expSize,
        actSize, actSize
            );
    return 1;
}

int Settings::VerifyRMInfoAndAmapInfoCachelineSizeFromComptags() const
{
    long comptags = m_RMInfo.comptagsPerCacheLine;
    int expSize = m_AmapInfo.GetCachelineSizeFromComptags(comptags);
    int actSize = m_RegVals.m_CachelineSize;

    if (expSize == actSize)
    {
        InfoPrintf("CBC: %s: (RM Info) comptags per Chunk: %lu (0x%lx), (fake-amapinfo) "
            "Expected per slice cacheline size: %u (0x%x), RegVals actual %u (0x%x) - MATCH.\n",
            m_pTestName,
            comptags, comptags,
            expSize, expSize,
            actSize, actSize
            );
        return 0;
    }
    long newComptags = AmapInfo::GetComptagsFromCachelineSize(GetLtcCount(),
        GetMaxSlicesPerLtc(),
        actSize);
    if (newComptags == comptags)
    {
        WarnPrintf("CBC: %s: (RM Info) comptags per Chunk: %lu (0x%lx), (fake-amapinfo) "
            "Expected per slice cacheline size: %u (0x%x), RegValus actual %u (0x%x) - "
            "Could MATCH or can't be determined; ignore the difference.\n",
            m_pTestName,
            comptags, comptags,
            expSize, expSize,
            actSize, actSize
            );
        return 0;
    }
    ErrPrintf("CBC: %s: (RM Info) comptags per Chunk: %lu (0x%lx), (fake-amapinfo) "
        "Expected per slice cacheline size: %u (0x%x), RegVals actual %u (0x%x) - MISMATCH.\n",
        m_pTestName,
        comptags, comptags,
        expSize, expSize,
        actSize, actSize
        );

    return 1;
}

int Settings::VerifyRMInfoAndAmapInfo(const AmapInfo& info) const
{
    int errors = 0;
    errors += VerifyRMInfoAndAmapInfoComptagChunkSize(info.m_ChunkSize);
    errors += VerifyRMInfoAndAmapInfoComptagsPerChunk(info.m_ComptagsPerChunk);
    errors += VerifyRMInfoAndAmapInfoCachelineSizeFromComptags();

    return errors;
}

RC Settings::ReportErrors(int errors) const
{
    if (errors > 0)
    {
        ErrPrintf("CBC: %s: ---CheckConfTest FAIL (%u errors found)---\n",
            m_pTestName,
            errors);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        InfoPrintf("CBC: %s: ---CheckConfTest PASS---\n",
            m_pTestName);
    }
    return OK;
}

int Settings::BasicCheck() const
{
    int errors = 0;

    errors += VerifyCoupledBasesOfCBCAndBackingStore();
    errors += VerifyRegValsAndRMInfo();

    return errors;
}

RC Settings::Check(const AmapInfo* pInfo) const
{
    int errors = 0;

    errors += BasicCheck();

    if (pInfo != 0)
    {
        errors += VerifyRegValsAndAmapInfo(*pInfo);
        errors += VerifyRMInfoAndAmapInfo(*pInfo);
    }

    return ReportErrors(errors);
}

///////////////CBC Session///////////////////

RC SurfCompOperation::WrVerifyCompBits(size_t offset, UINT32 compbits)
{
    UINT32 bits = 0;

    PutCompBits(offset, compbits);
    GetCompBits(offset, &bits);
    if (!CompBitsCompare(compbits, bits))
    {
        ErrPrintf("CBC: WrVerify failed @ offset 0x%lx, 0x%x != 0x%x (AND val 0x%x).\n",
            long(offset),
            CompBitsMaskedValue(bits),
            CompBitsMaskedValue(compbits),
            (bits & compbits)
            );
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    return OK;
}

RC SurfCompOperation::CopyAndVerifyCompBits2(size_t offset, SurfCompOperation* pDst)
{
    UINT32 compbits = CopyCompBits2(offset, pDst);
    UINT32 bits = 0;
    pDst->GetCompBits(offset, &bits);
    if (!CompBitsCompare(compbits, bits))
    {
        ErrPrintf("CBC: CopyAndVerify failed @ offset 0x%lx, 0x%x != 0x%x.\n",
            long(offset),
            CompBitsMaskedValue(bits),
            CompBitsMaskedValue(compbits)
            );
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    return OK;
}

RC SurfCompOperation::CopyAndVerifyPageCompBits2(size_t pageIdx, SurfCompOperation* pDst)
{
    UINT32 pCompbits[Settings::TilesPer64K];
    GetPageCompBits(pageIdx, pCompbits);
    pDst->PutPageCompBits(pageIdx, pCompbits);
    UINT32 pBits[Settings::TilesPer64K];
    pDst->GetPageCompBits(pageIdx, pBits);
    if (0 != memcpy(pCompbits, pBits, sizeof pCompbits))
    {
        ErrPrintf("CBC: CopyAndVerify failed on page 0x%x.\n",
            pageIdx);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    return OK;
}

////////////////////////CBC Surface//////////////////////////

TestSurf::TestSurf()
    : m_CompCvgHandle(0),
    m_pTest(0),
    m_pSurf(0),
    m_pCompOperation(0),
    m_SurfBase(0),
    m_SurfOff(0),
    m_SurfVirtBase(0),
    m_Size(0),
    m_PageIdx(0)
{
    memset(&m_CompCvg, 0, sizeof m_CompCvg);
}

TestSurf::~TestSurf()
{
    Clear();
}

void TestSurf::Clear()
{
    if (m_pCompOperation != 0)
    {
        MASSERT(m_pTest != 0 && m_pTest->GetCBCLib() != 0);
        m_pTest->GetCBCLib()->ReleaseCompOperation(m_pCompOperation);
        m_pCompOperation = 0;
    }
}

void TestSurf::CompCvgPrint(const char* pDesc) const
{
    DebugPrintf("CBC: (%s), Compression Coverage [%u:%u], format %u\n",
        pDesc, MinComptag(), MaxComptag(), m_CompCvg.format);
}

UINT32 TestSurf::GetComptag(size_t off) const
{
    UINT32 ctag;
    // Per 64KB page (shifting 16 bits on Pascal, and 17 bits on Maxwell).
    // Since the Fermi Architecture, is built for a 128K page. It is built assuming
    // that the comptags for a page (128K) are stored in the same comptagline.
    // See //hw/doc/gpu/maxwell/maxwell/design/IAS/L2/maxwell_cbc.pptx for more information on the CBC.
    // For Pascal, the maximum page size is 64KB and 128KB pages are no longer
    // supported. The granularity of the comptagline has also changed to 64KB.
    if (PrePascal == m_pTest->GetSettings()->GetTRCChipFamily())
    {
        ctag = MinComptag() + (off >> 17);
    }
    else
    {
        ctag = MinComptag() + (off >> 16);
    }
    if (ctag < MinComptag() || ctag > MaxComptag())
    {
        ErrPrintf("CBC: comptag %u not within correct range [%u:%u]\n",
            ctag, MinComptag(), MaxComptag());
    }
    return ctag;
}

RC TestSurf::ProbeCompCvg(const char* pDesc, bool bCheck)
{
    RC rc;
    LwRm* pLwRm = m_pTest->GetTraceChan()->GetLwRmPtr();

    CHECK_RC(pLwRm->Control(m_CompCvgHandle,
            LW0041_CTRL_CMD_GET_SURFACE_COMPRESSION_COVERAGE,
            &m_CompCvg,
            sizeof(m_CompCvg)
            ));

    CompCvgPrint(pDesc);
    if (bCheck && !CheckCompCvgFormat())
    {
        return RC::DATA_MISMATCH;
    }

    return rc;
}

RC TestSurf::Init(CompBitsTest* pTest, MdiagSurf* pSurf,
        bool bProbeCvg, bool bCheck)
{
    RC rc;

    Clear();
    m_pTest = pTest;
    m_pCompOperation = m_pTest->GetCBCLib()->FetchCompOperation(this);
    m_pSurf = pSurf;
    SetCompCvgHandle(m_pSurf->GetMemHandle());
    if (bProbeCvg)
    {
        CHECK_RC(ProbeCompCvg("Init", bCheck));
    }
    m_Size = pSurf->GetAllocSize();
    m_SurfBase = pSurf->GetVidMemOffset();
    m_SurfVirtBase = pSurf->GetCtxDmaOffsetGpu();
    DebugPrintf("CBC: Surfce (%s) vidmem base 0x%016llx total size 0x%016llx.\n",
        pSurf->GetName().c_str(),
        m_SurfBase,
        m_Size
        );

    return rc;
}

ssize_t TestSurf::FineOffset(size_t dataSize) const
{
    ssize_t off = 0;
    if (CompBits::PrePascal == m_pTest->GetSettings()->GetTRCChipFamily())
    {
        off = GetRWAddr_Maxwell(off, dataSize);
    }
    else
    {
        off = GetRWAddr(off, dataSize);
    }
    return off;
}

ssize_t TestSurf::GetRWAddr_Maxwell(UINT64 in, size_t size) const
{
    UINT64 out = in;
    out += GetOffset() & (~(Settings::GobSize - 1));
    DebugPrintf("CBC: RWAddr: per offset 0x%016llx, fined#1 address from 0x%016llx to 0x%016llx\n",
        GetOffset(), in + GetOffset(), out);
    out += (GetOffset() & Settings::TileSize) ? size : 0;
    DebugPrintf("CBC: RWAddr: per offset 0x%016llx, fined#2 address from 0x%016llx to 0x%016llx for read/write\n",
        GetOffset(), in + GetOffset(), out);
    return out;
}

ssize_t TestSurf::GetRWAddr(UINT64 in, size_t size) const
{
    UINT64 out = in + GetOffset();
    out = ALIGN_UP(out, (unsigned int)Settings::TileSize);
    DebugPrintf("CBC: RWAddr: per offset 0x%016llx, fined address from 0x%016llx to 0x%016llx for read/write\n",
        GetOffset(), in + GetOffset(), out);
    return out;
}

bool TestSurf::CheckCompCvgFormat() const
{
    if (0 == m_CompCvg.format)
    {
        ErrPrintf("CBC: Compression Coverage format is 0, no compression\n");
        return false;
    }
    return true;
}

UINT32* TestSurf::CreateCachelineBuffer()
{
    UINT08 *line = new UINT08[m_pTest->GetSettings()->GetCachelineSize()];
    memset(line, 0, m_pTest->GetSettings()->GetCachelineSize());
    return reinterpret_cast<UINT32*>(line);
}

RC TestSurf::Map() const
{
    return m_pSurf->Map();
}

void TestSurf::Unmap() const
{
    m_pSurf->Unmap();
}

RC TestSurf::Write(const vector<UINT08>& data) const
{
    RC rc;

    // Write to surface at command-line argument offset.
    DebugPrintf("CBC: surface BAR1 address is %p\n", m_pSurf->GetAddress());

    uintptr_t writeAddress = reinterpret_cast<uintptr_t>(m_pSurf->GetAddress());
    writeAddress += FineOffset(data.size());

    UINT32 savedBackdoor = 0;
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        Platform::EscapeRead("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, &savedBackdoor);
        DebugPrintf("CBC: BACKDOOR_ACCESS = %u\n", savedBackdoor);
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
    }

    DebugPrintf("CBC: Debug writing data @ addr 0x%llx\n", (UINT64)writeAddress);
    Platform::VirtualWr(reinterpret_cast<void *>(writeAddress),
            static_cast<const void*>(&data[0]), data.size());
    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, savedBackdoor);
    }

    return rc;
}

RC TestSurf::MapAndWrite(const vector<UINT08>& data) const
{
    RC rc;
    CHECK_RC(Map());
    CHECK_RC(Write(data));
    // A flush is requried after Write() and ahead of Unmap() on Pascal.
    CHECK_RC(m_pTest->FlushCompTags());
    Unmap();
    return rc;
}

////////////////////////////////////////////////////

RC SurfCopier::Init(CompBitsTest* pTest, MdiagSurf* pSurf,
    bool bProbeCvg, bool bCheck)
{
    m_pTest = pTest;
    MASSERT(pSurf != 0);
    m_pSrcSurf = pSurf;
    m_pDstSurf = new MdiagSurf;
    m_pOrigDstSurf = m_pDstSurf;

    RC rc;

    CHECK_RC(m_Src.Init(m_pTest, m_pSrcSurf, bProbeCvg, bCheck));
    CHECK_RC(m_pTest->ReplicateSurface(m_Src.GetMdiagSurf(),
        m_pDstSurf, "NewSurface"));
    CHECK_RC(m_Dst.Init(m_pTest, m_pDstSurf, bProbeCvg, bCheck));

    return rc;
}

void SurfCopier::Clear()
{
    if (m_pDstSurf == m_pOrigDstSurf)
    {
        m_pDstSurf = 0;
    }
    else
    {
        m_pSrcSurf = 0;
    }
    m_pOrigDstSurf->Free();
    delete m_pOrigDstSurf;
    m_pOrigDstSurf = 0;
    m_Src.Clear();
    m_Dst.Clear();
}

RC SurfCopier::ExchangeSurface()
{
    SetOffset(0);
    MdiagSurf* tmp = m_pSrcSurf;
    m_pSrcSurf = m_pDstSurf;
    m_pDstSurf = tmp;

    RC rc;
    CHECK_RC(m_Src.Init(m_pTest, m_pSrcSurf, true, false));
    CHECK_RC(m_Dst.Init(m_pTest, m_pDstSurf, true, false));
    return rc;
}

RC SurfCopier::CopyAndVerifyAllCompBits()
{
    RC rc;
    size_t off = 0;
    for (size_t i = 0;
        i < Settings::Size64K / m_pTest->GetSettings()->GetCompBitsAccessBasis();
        i++, off += m_pTest->GetSettings()->GetCompBitsAccessBasis())
    {
        CHECK_RC(CopyAndVerifyCompBits(off));
    }
    return rc;
}

void SurfCopier::CopyAllCompBits()
{
    size_t off = 0;
    for (size_t i = 0;
        i < Settings::Size64K / m_pTest->GetSettings()->GetCompBitsAccessBasis();
        i++, off += m_pTest->GetSettings()->GetCompBitsAccessBasis())
    {
        CopyCompBits(off);
    }
}

void SurfCopier::ClearAllDstCompBits()
{
    size_t off = 0;
    for (size_t i = 0;
        i < Settings::Size64K / m_pTest->GetSettings()->GetCompBitsAccessBasis();
        i++, off += m_pTest->GetSettings()->GetCompBitsAccessBasis())
    {
        ClearDstCompBits(off);
    }
}

RC SurfCopier::CopyData()
{
    return m_pTest->CopySurfaceData(m_Src.GetMdiagSurf(),
        m_Dst.GetMdiagSurf(), m_pOrigDstSurf);
}

} // namespace CompBits

