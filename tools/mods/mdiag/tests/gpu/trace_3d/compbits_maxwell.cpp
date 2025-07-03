/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016, 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/rc.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/floorsweepimpl.h"
#include "core/include/utility.h"
#include "tracesubchan.h"
#include "class/cla097.h"
#include "class/cla197.h"
#include "class/clb097.h"
#include "class/clb197.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"
#include "Lwcm.h"

#include "compbits.h"

// APIs used to call CompBits lib for pre-Pascal (Maxwell/Kepler).
#include "../shared/inc/swizzle_compbits.h"
#include "../shared/inc/FermiAddressMapping_MAXWELL2.h"

///////////////////////////////////////////////////////////

namespace CompBits_Maxwell
{
// Pre-Pascal (Maxwell/Kepler...) CompBits interfaces implmentation.

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
        UINT32 fcbits = 0;
        SingleGetCompBits(offset, pCompbits, &fcbits, false);
        DebugPrintf("CBC: GetCompBits: fcbits = 0x%x compbits = 0x%x\n",
            fcbits, *pCompbits);

        // Get the other 64KB part just for debugging.
        UINT32 otherbits = 0;
        SingleGetCompBits(offset, &otherbits, &fcbits, true);
        DebugPrintf("CBC: GetCompBits: other 64: fcbits = 0x%x compbits = 0x%x\n",
            fcbits, otherbits);
    }

    virtual void PutCompBits(size_t offset, UINT32 compbits)
    {
        bool bWriteFc = false;
        const UINT32 fcbits = 0;
        DebugPrintf("CBC: PutCompBits: writeFc = false  fcbits = 0x0  compbits = 0x%x\n",
            compbits);
        SinglePutCompBits(offset, compbits, fcbits, bWriteFc);

        bWriteFc = true;
        DebugPrintf("CBC: PutCompBits: writeFc = true  fcbits = 0x0  compbits = 0x%x\n",
            compbits);
        SinglePutCompBits(offset, compbits, fcbits, bWriteFc);
    }

    virtual void GetPageCompBits(size_t pageIdx,
        UINT32 pCompbits[CompBits::Settings::TilesPer64K])
    {
        // Maxwell only needs 128 compbits.
        memset(pCompbits, 0, CompBits::Settings::GobsPer64K * sizeof(pCompbits[0]));
        const size_t off = m_pTestSurf->GetPageOffset(pageIdx);
        const UINT32 comptagLine = m_pTestSurf->GetComptag(off);

        ReadCompBits64KB(
            m_pTestSurf->GetPhyBase(),
            comptagLine,
            pageIdx,
            pCompbits,
            m_pTestSurf->Upper64KBSel(off)
            );
    }

    virtual void PutPageCompBits(size_t pageIdx,
        const UINT32 pCompbits[CompBits::Settings::TilesPer64K])
    {
        const size_t off = m_pTestSurf->GetPageOffset(pageIdx);
        const UINT32 comptagLine = m_pTestSurf->GetComptag(off);

        WriteCompBits64KB(
            m_pTestSurf->GetPhyBase(),
            comptagLine,
            pageIdx,
            pCompbits,
            m_pTestSurf->Upper64KBSel(off)
            );
    }

    bool ReadCompBits64KB(
        UINT64 DataPhysicalStart, // Source
        UINT32 ComptagLine, // Source
        UINT32 page64KB,
        UINT32 compbitBuffer[],
        bool   upper64KBCompbitSel
        )
    {
        LW2080_CTRL_CMD_FB_COMPBITCOPY_READ_COMPBITS64KB_PARAMS parm = {0};

        parm.SrcDataPhysicalStart = DataPhysicalStart;
        parm.SrcComptagLine = ComptagLine;
        parm.page64KB = page64KB;
        parm.compbitBuffer = compbitBuffer;
        parm.upper64KBCompbitSel = !!upper64KBCompbitSel;

        LW_STATUS status = compBitCopyReadCompBits64KB(m_pCBCLib->GetLibHandle(), &parm);
        return LW_OK == status;
    }

    bool WriteCompBits64KB(
        UINT64 DataPhysicalStart, // Dest
        UINT32 ComptagLine, // Dest
        UINT32 page64KB,
        const UINT32 compbitBuffer[],
        bool   upper64KBCompbitSel
        )
    {
        LW2080_CTRL_CMD_FB_COMPBITCOPY_WRITE_COMPBITS64KB_PARAMS parm = {0};

        parm.DstDataPhysicalStart = DataPhysicalStart;
        parm.DstComptagLine = ComptagLine;
        parm.page64KB = page64KB;
        parm.compbitBuffer = const_cast<UINT32*>(compbitBuffer);
        parm.upper64KBCompbitSel = !!upper64KBCompbitSel;

        LW_STATUS status = compBitCopyWriteCompBits64KB(m_pCBCLib->GetLibHandle(), &parm);
        return LW_OK == status;
    }

private:
    bool SingleGetCompBits(size_t offset,
        UINT32* pCompbits,
        UINT32* pFcbits,
        bool bOtherPartOf64KB)
    {
        const UINT32 comptagLine = m_pTestSurf->GetComptag(offset);
        bool finsel = m_pTestSurf->Upper64KBSel(offset);
        if (bOtherPartOf64KB)
        {
            finsel = !finsel;
        }

        LW2080_CTRL_CMD_FB_COMPBITCOPY_GET_COMPBITS_PARAMS parm = {0};

        parm.fcbits = pFcbits;
        parm.compbits = pCompbits;
        parm.dataPhysicalStart = m_pTestSurf->GetPhyBase();
        parm.surfaceOffset = offset;
        parm.comptagLine = comptagLine;
        parm.upper64KBCompbitSel = !!finsel;

        LW_STATUS status = compBitCopyGetCompBits(m_pCBCLib->GetLibHandle(), &parm);
        return LW_OK == status;
    }

    bool SinglePutCompBits(size_t offset,
        UINT32 compbits,
        UINT32 fcbits,
        bool bWriteFc
        )
    {
        const UINT32 comptagLine = m_pTestSurf->GetComptag(offset);
        LW2080_CTRL_CMD_FB_COMPBITCOPY_PUT_COMPBITS_PARAMS parm = {0};

        parm.fcbits = fcbits;
        parm.compbits = compbits;
        parm.writeFc = !!bWriteFc;
        parm.dataPhysicalStart = m_pTestSurf->GetPhyBase();
        parm.surfaceOffset = offset;
        parm.comptagLine = comptagLine;
        parm.upper64KBCompbitSel = !!m_pTestSurf->Upper64KBSel(offset);

        LW_STATUS status = compBitCopyPutCompBits(m_pCBCLib->GetLibHandle(), &parm);
        return LW_OK == status;
    }

};

/////////Chip specific CBC Connection implemention/////////

class LibWrapperImpl
    : public CompBits::CBCLibWrapper
{
public:
    LibWrapperImpl(CompBitsTest* pTest)
        : CompBits::CBCLibWrapper(pTest)
    {}

    virtual  ~LibWrapperImpl()
    {
        if (m_pHandle != 0)
        {
            freeCompBitCopy(m_pHandle);
            m_pHandle = 0;
        }
    }

    virtual RC Init();
    virtual RC GetCBCLib();
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

private:
    void InitLib(
        UINT32 ComptagsPerCacheLine,
        UINT32 UnpackedComptagLinesPerCacheLine,
        UINT32 CompCacheLineSize,
        UINT32 UnpackedCompCacheLineSizePerLTC,
        UINT32 BitsPerRAMEntry,
        UINT32 RAMBankWidth,
        UINT32 BitsPerComptagLine,
        UINT32 RAMEntriesPerCompCacheLine,
        UINT32 ComptagLineSize,
        UINT32 CBCBaseAddress,
        UINT32 PageSizeSrc,
        UINT32 PageSizeDest,
        UINT32 SlicesPerLTC,
        UINT32 NumActiveLTCs,
        UINT32 FamilyName,
        UINT32 ChipName
        )
    {
        CompBitCopyConstructParams parm = {0};

        parm.comptagsPerCacheLine = ComptagsPerCacheLine;
        parm.unpackedComptagLinesPerCacheLine = UnpackedComptagLinesPerCacheLine;
        parm.compCacheLineSizePerLTC = CompCacheLineSize;
        parm.unpackedCompCacheLineSizePerLTC = UnpackedCompCacheLineSizePerLTC;
        parm.bitsPerRAMEntry = BitsPerRAMEntry;
        parm.ramBankWidth = RAMBankWidth;
        parm.bitsPerComptagLine = BitsPerComptagLine;
        parm.ramEntriesPerCompCacheLine = RAMEntriesPerCompCacheLine;
        parm.comptagLineSize = ComptagLineSize;
        parm.defaultPageSize = PageSizeSrc;
        parm.slicesPerLTC = SlicesPerLTC;
        parm.numActiveLTCs = NumActiveLTCs; //Arch code calls this FB parititons, which is not accurate for all chips
        parm.familyName = FamilyName;
        parm.chipName = ChipName;

        m_pHandle = allocAndInitCompBitCopy(&parm);
    }

    bool SetContext(
        UINT32 CBCBaseAddress,
        UINT64 backingStorePA,
        UINT08 *backingStoreVA,
        UINT64 backingStoreChunkPA,
        UINT08 *backingStoreChunkVA,
        UINT32 backingStoreChunkSize,
        UINT08 *cacheWriteBitMap,
        bool  backingStoreChunkOverfetch,
        UINT32 PageSizeSrc,
        UINT32 PageSizeDest
        )
    {
        LW2080_CTRL_CMD_FB_COMPBITCOPY_SET_CONTEXT_PARAMS parm = {0};

        parm.CBCBaseAddress = CBCBaseAddress;
        parm.backingStorePA = backingStorePA;
        parm.backingStoreVA = backingStoreVA;
        parm.backingStoreChunkPA = backingStoreChunkPA;
        parm.backingStoreChunkVA = backingStoreChunkVA;
        parm.backingStoreChunkSize = backingStoreChunkSize;
        parm.cacheWriteBitMap = cacheWriteBitMap;
        parm.backingStoreChunkOverfetch = !!backingStoreChunkOverfetch;
        parm.PageSizeSrc = PageSizeSrc;
        parm.PageSizeDest = PageSizeDest;

        LW_STATUS status = compBitCopySetContext(m_pHandle, &parm);
        return LW_OK == status;
    }

};

RC LibWrapperImpl::Init()
{
    RC rc;

    CompBits::Settings *pCfg = m_pTest->GetSettings();
    UINT32 comptagsPerChunk = pCfg->GetComptagsPerChunk();
    size_t cachelineSize = pCfg->GetCachelineSize();

    pCfg->SetMaxwellCachelineSize(cachelineSize);
    size_t numLtcs = 0;
    GpuSubdevice *gpuSubdevice = m_pTest->GetGpuDev()->GetSubdevice(0);
    UINT32 fbMask = gpuSubdevice->GetFsImpl()->FbMask();

    DebugPrintf("CBC: FB mask = 0x%x\n", fbMask);

    for (UINT32 i = 0; fbMask != 0; ++i, fbMask >>= 1)
    {
        if ((fbMask & 1) != 0)
        {
            UINT32 L2Mask = gpuSubdevice->GetFsImpl()->L2Mask(i);
            DebugPrintf("CBC: ROP L2 mask(%d) = 0x%x\n", i, L2Mask);
            numLtcs += Utility::CountBits(L2Mask);
        }
    }

    size_t slices = gpuSubdevice->GetFB()->GetMaxL2SlicesPerFbp();
    UINT32 ltcsPerFbp = gpuSubdevice->GetFB()->GetMaxLtcsPerFbp();
    slices /= ltcsPerFbp;

    MASSERT(numLtcs > 0);
    MASSERT(slices > 0);
    pCfg->SetLtcCount(numLtcs);
    pCfg->SetSlicesPerLtc(slices);
    pCfg->InitAmapInfo();

    DebugPrintf("CBC: comptag lines per cacheline (per Chunk) = %u\n", comptagsPerChunk);
    DebugPrintf("CBC: comp cache line size = %u\n", int((cachelineSize)));
    DebugPrintf("CBC: CBC base address = 0x%x\n", pCfg->GetCBCBase());
    DebugPrintf("CBC: number of LTCs = %u\n", pCfg->GetLtcCount());
    DebugPrintf("CBC: slices per LTC = %u\n", pCfg->GetMaxSlicesPerLtc());

    // Default values (based on GK110)
    //
    UINT32 bitsPerRamEntry = 276;
    UINT32 ramBankWidth = 244;
    UINT32 bitsPerComptagLine = 52;
    UINT32 ramEntriesPerCompCacheLine = 14;
    UINT32 comptagLineSize = 17;
    UINT32 pageSizeSrc = 64;
    UINT32 pageSizeDest = 64;
    UINT32 family = AMAP_MAXWELL2::FermiPhysicalAddress::KEPLER;
    UINT32 chip = 0;

    UINT32 devClass = m_pTest->GetTraceSubChan()->GetClass();

    switch (devClass)
    {
        case KEPLER_A:
            switch (numLtcs)
            {
                case 2:
                    bitsPerComptagLine = 132;
                    break;
                case 3:
                    bitsPerComptagLine = 92;
                    break;
                case 4:
                    bitsPerComptagLine = 68;
                    break;
                default:
                    ErrPrintf("Unexpected LTC count %u for class 0x%04x.\n",
                        numLtcs,
                        devClass);
                    return RC::SOFTWARE_ERROR;
            }
            chip = AMAP_MAXWELL2::FermiPhysicalAddress::GKLIT1;
            break;

        case KEPLER_B:
            switch (numLtcs)
            {
                case 5:
                    bitsPerComptagLine = 60;
                    break;
                case 6:
                    bitsPerComptagLine = 52;
                    break;
                default:
                    ErrPrintf("Unexpected LTC count %u for class 0x%04x.\n",
                        numLtcs,
                        devClass);
                    return RC::SOFTWARE_ERROR;
            }
            chip = AMAP_MAXWELL2::FermiPhysicalAddress::GK110;
            break;

        case MAXWELL_A:
            comptagsPerChunk /= 2;
            cachelineSize /= 2;
            bitsPerRamEntry = 264;
            ramBankWidth = 232;
            switch (numLtcs)
            {
                case 1:
                    bitsPerComptagLine = 260;
                    break;
                case 2:
                    bitsPerComptagLine = 132;
                    break;
                default:
                    ErrPrintf("Unexpected LTC count %u for class 0x%04x.\n",
                        numLtcs,
                        devClass);
                    return RC::SOFTWARE_ERROR;
            }
            family = AMAP_MAXWELL2::FermiPhysicalAddress::MAXWELL;
            chip = AMAP_MAXWELL2::FermiPhysicalAddress::GMLIT1;
            break;

        case MAXWELL_B:
            switch (numLtcs)
            {
                case 4:
                    bitsPerComptagLine = 136;
                    break;
                case 6:
                    bitsPerComptagLine = 104;
                    break;
                case 7:
                    // !!! SHOULD BE 88 FOR 128K PAGE SIZE !!!
                    bitsPerComptagLine = 104;
                    break;
                case 8:
                case 10:
                    bitsPerComptagLine = 68;
                    break;
                case 12:
                    bitsPerComptagLine = 52;
                    break;
                default:
                    ErrPrintf("Unexpected LTC count %u for class 0x%04x.\n",
                        numLtcs,
                        devClass);
                    return RC::SOFTWARE_ERROR;
            }
            family = AMAP_MAXWELL2::FermiPhysicalAddress::MAXWELL;
            chip = AMAP_MAXWELL2::FermiPhysicalAddress::GMLIT2;
            break;

        default:
            ErrPrintf("Class 0x%04x is unsupported for compbit tests.\n",
                devClass);
            return RC::SOFTWARE_ERROR;
    }
    pCfg->SetMaxwellCachelineSize(cachelineSize);

    InitLib(comptagsPerChunk,
        comptagsPerChunk,
        pCfg->GetMaxwellCachelineSize(),
        pCfg->GetMaxwellCachelineSize(),
        bitsPerRamEntry,
        ramBankWidth,
        bitsPerComptagLine,
        ramEntriesPerCompCacheLine,
        comptagLineSize,
        pCfg->GetCBCBase(),
        pageSizeSrc,
        pageSizeDest,
        pCfg->GetMaxSlicesPerLtc(),
        pCfg->GetLtcCount(),
        family,
        chip
        );

    return rc;
}

RC LibWrapperImpl::GetCBCLib()
{
    SetContext(
        m_pTest->GetSettings()->GetCBCBase(),
        m_pTest->GetSettings()->GetBackingStoreAllocPA(),
        static_cast<UINT08*>(m_pTest->GetBackingStoreBar1Addr()),
        0,
        0,
        0,
        0,
        false,
        64,
        64);

    return OK;
}

///////////////////////////////////////////////////////////////////////////

static void SetForceBar1(bool force)
{
    LW2080_CTRL_CMD_FB_COMPBITCOPY_SET_FORCE_BAR1_PARAMS parm = {0};
    parm.bForceBar1 = !!force;
    compBitSetForceBar1(0, &parm);
}

/////////////////////Creators///////////////////////////

CompBits::CBCLibWrapper* CreateCBCLibWrapper(CompBitsTest* pTest)
{
    SetForceBar1(true);
    return new LibWrapperImpl(pTest);
}

/////////////////////////////////////////////////////
} // namespace CompBits_Maxwell

namespace CompBits
{

CompBitsTest* TestCreator::CreateMaxwellTest(TraceSubChannel* pSubChan, const TestID testID)
{
    // As of now we don't have Maxwell specific test.
    // So just create common tests.
    CompBitsTest* pTest = CreateCommonTest(pSubChan, testID);

    pTest->SetTestID(testID);
    pTest->SetCBCLib(CompBits_Maxwell::CreateCBCLibWrapper(pTest));

    return pTest;
}

} // namespace CompBits

