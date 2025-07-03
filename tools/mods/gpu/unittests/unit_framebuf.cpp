/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/unittests/unittest.h"
#include "core/include/memerror.h"
#include "core/include/mgrmgr.h"
#include "core/include/xp.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "random.h"

namespace
{
    //----------------------------------------------------------------
    //! \brief Unit tests for the FrameBuffer class
    //!
    class FrameBufferTest: public UnitTest
    {
    public:
        FrameBufferTest();
        virtual void Run();
        void RunOnSubdevice(GpuSubdevice *pGpuSubdevice);

    private:
        static void UpdateBitsUsed(UINT32 bitmask,
                                   int expectedBitmaskSize,
                                   set<int> *pBitsUsed);
        set<int> AllValues(int numValues);

        Random m_Random;
    };

    ADD_UNIT_TEST(FrameBufferTest);

    // See comments in RunOneSubdevice
    const int NUM_DECODE_LOOPS   = 10000;
    const int MAX_DECODE_FIELD   = 100;
    const int NUM_MEMERROR_LOOPS = 100;
    const int MAX_PATTERN_OFFSET = 63;
}

//--------------------------------------------------------------------
FrameBufferTest::FrameBufferTest() :
    UnitTest("FrameBufferTest")

{
    m_Random.SeedRandom(static_cast<UINT32>(Xp::GetWallTimeUS()));
}

//--------------------------------------------------------------------
//! \brief Run tests on each GpuSubdevice in the system
//!
/* virtual */ void FrameBufferTest::Run()
{
    GpuDevMgr *pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);

    if (pGpuDevMgr == nullptr)
    {
        Printf(Tee::PriNormal, "GPU not initialized, skipping test\n");
        return;
    }

    for (GpuSubdevice *pGpuSubdevice = pGpuDevMgr->GetFirstGpu();
         pGpuSubdevice != nullptr;
         pGpuSubdevice = pGpuDevMgr->GetNextGpu(pGpuSubdevice))
    {
        RunOnSubdevice(pGpuSubdevice);
    }
}

//--------------------------------------------------------------------
//! \brief Run tests on one GpuSubdevice
//!
void FrameBufferTest::RunOnSubdevice(GpuSubdevice *pGpuSubdevice)
{
    const UINT32 KIND         = 0;
    const UINT32 PAGE_SIZE_KB = 4;

    FrameBuffer *pFb               = pGpuSubdevice->GetFB();
    const UINT64 fbSize            = pFb->GetGraphicsRamAmount();
    const bool   isEccOn           = pFb->IsEccOn();
    const UINT32 numPseudoChannels = pFb->GetPseudoChannelsPerSubpartition();
    const UINT32 numSubpartitions  = pFb->GetSubpartitions();
    const UINT32 fbioCount         = pFb->GetFbioCount();
    const UINT32 burstBits         = Utility::Log2i(pFb->GetRowSize() /
                                                    pFb->GetBurstSize());
    set<int> beatOffsetsUsed;
    set<int> beatsUsed;
    set<int> burstBitsUsed[2];
    set<int> rowBitsUsed[2];
    set<int> banksUsed;
    set<int> ranksUsed;
    set<int> pseudoChannelsUsed;
    set<int> subpartitionsUsed;
    set<int> fbiosUsed;
    RC rc;

    // Check DecodeAddress
    //
    for (int ii = 0; ii < NUM_DECODE_LOOPS; ++ii)
    {
        // Decode a random address to RBC
        //
        const UINT64 physAddr = m_Random.GetRandom64(0, fbSize - 1);
        FbDecode fbDecode;
        UNIT_ASSERT_RC(pFb->DecodeAddress(&fbDecode, physAddr,
                                          KIND, PAGE_SIZE_KB));

        // If EncodeAddress is supported, then verify that re-encoding
        // the RBC gives you the original address.
        //
        if (!pGpuSubdevice->HasBug(1763853))
        {
            UINT64 eccOnAddr;
            UINT64 eccOffAddr;
            rc = pFb->EncodeAddress(fbDecode, KIND, PAGE_SIZE_KB,
                                    &eccOnAddr, &eccOffAddr);
            if (rc != RC::UNSUPPORTED_FUNCTION)
            {
                UNIT_ASSERT_EQ(rc, OK);
                UNIT_ASSERT_EQ(isEccOn ? eccOnAddr : eccOffAddr, physAddr);
            }
            rc.Clear();
        }

        // Record which fbios, subpartitions, banks, etc were
        // used by this decoding.
        //
        beatOffsetsUsed.insert(fbDecode.beatOffset);
        beatsUsed.insert(fbDecode.beat);
        UpdateBitsUsed(fbDecode.burst, burstBits, &burstBitsUsed[0]);
        UpdateBitsUsed(fbDecode.row, pFb->GetRowBits(), &rowBitsUsed[0]);
        banksUsed.insert(fbDecode.bank);
        ranksUsed.insert(fbDecode.rank);
        pseudoChannelsUsed.insert(fbDecode.pseudoChannel);
        subpartitionsUsed.insert(fbDecode.subpartition);
        fbiosUsed.insert(fbDecode.virtualFbio);
    }

    // Check to make sure that all fbios, subpartitions, banks, column
    // bits, etc were used by the decodings.  For the column and row
    // bits, make sure that each bit was set to both 0 and 1 by the
    // various decodings.
    //
    // The number of decodings (NUM_DECODE_LOOPS=10000) was chosen to
    // make it highly unlikely that we missed any.  Even if one of
    // these fields has 100 different values (e.g. 100 banks), the
    // probability of missing one is only 0.99**10000=2e-44, which is
    // is low enough that it's unlikely to happen even if we ran the
    // test a million times per second until the end of the universe.
    // But just to be safe, we check in AllValues() to make sure we
    // don't exceed 100 values per field (MAX_DECODE_FIELD); we might
    // want to increase NUM_DECODE_LOOPS if that happens.
    //
    UNIT_ASSERT_EQ(beatOffsetsUsed, AllValues(pFb->GetBeatSize()));
    UNIT_ASSERT_EQ(beatsUsed, AllValues(pFb->GetBurstSize() /
                                        pFb->GetBeatSize()));
    UNIT_ASSERT_EQ(burstBitsUsed[0], AllValues(burstBits));
    UNIT_ASSERT_EQ(burstBitsUsed[1], AllValues(burstBits));
    UNIT_ASSERT_EQ(rowBitsUsed[0], AllValues(pFb->GetRowBits()));
    UNIT_ASSERT_EQ(rowBitsUsed[1], AllValues(pFb->GetRowBits()));
    UNIT_ASSERT_EQ(banksUsed, AllValues(pFb->GetBankCount()));
    UNIT_ASSERT_EQ(ranksUsed, AllValues(pFb->GetRankCount()));
    UNIT_ASSERT_EQ(pseudoChannelsUsed, AllValues(numPseudoChannels));
    UNIT_ASSERT_EQ(subpartitionsUsed, AllValues(numSubpartitions));
    UNIT_ASSERT_EQ(fbiosUsed, AllValues(fbioCount));

    // Verify that if we add all the memory from all DRAMs together,
    // we get the total amount of physical memory.
    //
    const UINT64 dramSize = (pFb->GetRowSize() *
                             (1ULL << pFb->GetRowBits()) *
                             pFb->GetBankCount() *
                             pFb->GetRankCount() *
                             numPseudoChannels *
                             numSubpartitions *
                             fbioCount);

    UINT64 availableDramSize = dramSize;

    if (pFb->IsRowRemappingOn())
    {
        // Row remapping reserves part of memory for reserved rows
        const UINT64 reservedMemory = pFb->GetRowRemapReservedRamAmount();
        UNIT_ASSERT_GE(availableDramSize, reservedMemory);
        availableDramSize -= reservedMemory;
    }

    if (isEccOn && !pFb->IsHbm())
    {
        const UINT64 maxCheckbitSize = availableDramSize / 8;
        UNIT_ASSERT_GE(pFb->GetGraphicsRamAmount(),
                       availableDramSize - maxCheckbitSize);
        UNIT_ASSERT_LT(pFb->GetGraphicsRamAmount(), availableDramSize);
    }
    else
    {
        UNIT_ASSERT_EQ(pFb->GetGraphicsRamAmount(), availableDramSize);
    }

    // Generate random errors for the MemError class.  Most of these
    // functions don't have return codes we can test; we're just
    // making sure they don't MASSERT().
    //
    MemError memError;
    UNIT_ASSERT_RC(memError.SetupTest(pFb));
    for (int ii = 0; ii < NUM_MEMERROR_LOOPS; ++ii)
    {
        UINT32 width   = 1 << m_Random.GetRandom64(3, 6);
        UINT64 maxData = ~0ULL >> (64 - width);
        UNIT_ASSERT_RC(memError.LogMysteryError());
        UNIT_ASSERT_RC(memError.LogOffsetError(
                        width,
                        ALIGN_DOWN(m_Random.GetRandom64(0, fbSize - 1),
                                   width / 8),
                        m_Random.GetRandom(0, maxData),         // actual
                        m_Random.GetRandom(0, maxData),         // expected
                        KIND, PAGE_SIZE_KB, "patternName",
                        m_Random.GetRandom(0, MAX_PATTERN_OFFSET)));
        UNIT_ASSERT_RC(memError.LogUnknownMemoryError(
                        width,
                        ALIGN_DOWN(m_Random.GetRandom64(0, fbSize - 1),
                                   width / 8),
                        m_Random.GetRandom(0, maxData),         // actual
                        m_Random.GetRandom(0, maxData),         // expected
                        KIND, PAGE_SIZE_KB));
        UNIT_ASSERT_RC(memError.LogMemoryError(
                        width,
                        ALIGN_DOWN(m_Random.GetRandom64(0, fbSize - 1),
                                   width / 8),
                        m_Random.GetRandom(0, maxData),         // actual
                        m_Random.GetRandom(0, maxData),         // expected
                        m_Random.GetRandom(0, maxData),         // reRead1
                        m_Random.GetRandom(0, maxData),         // reRead2
                        KIND, PAGE_SIZE_KB, MemError::NO_TIME_STAMP,
                        "patternName",
                        m_Random.GetRandom(0, MAX_PATTERN_OFFSET)));
        UNIT_ASSERT_RC(memError.LogFailingBits(
                        m_Random.GetRandom(0, fbioCount - 1),
                        m_Random.GetRandom(0, numSubpartitions - 1),
                        m_Random.GetRandom(0, numPseudoChannels - 1),
                        m_Random.GetRandom(0, pFb->GetRankCount() - 1),
                        m_Random.GetRandom(0, 0xff),            // beatMask
                        m_Random.GetRandom(
                                0, pFb->GetBeatSize() - 1),     // beatOffset
                        m_Random.GetRandom(0, 7),               // bitOffset
                        m_Random.GetRandom(0, 7),               // wordCount
                        m_Random.GetRandom(0, 7),               // bitCount
                        static_cast<MemError::IoType>(m_Random.GetRandom(0, 1))));
    }
    UNIT_ASSERT_RC(memError.Cleanup());
}

//--------------------------------------------------------------------
//! \brief Record which bits were set/cleared in a bitmask
//!
//! This method is used to ensure that DecodeAddress() hits the full
//! range of a bitfield.
//!
//! For example, if there are 10 column bits, then we want to make
//! sure that all 10 bits get used.  When we dedode various addresses,
//! each of those 10 bits should get set to 0 by some decodings and 1
//! by other decodings, and we should *never* set any bit to 1 outside
//! of that 10-bit range.
//!
//! This method gets called with various bitmasks.  It keeps a running
//! track of which bits got set to 0 and 1 in pBitsUsed[0] and
//! pBitsUsed[1] respectively.  Another method will check that
//! pBitsUsed[0] & pBitsUsed[1] contain all values from 0 to N-1, and
//! nothing else.
//!
/* static */ void FrameBufferTest::UpdateBitsUsed
(
    UINT32 bitmask,
    int expectedBitmaskSize,
    set<int> *pBitsUsed
)
{
    const int numBits = max<int>(expectedBitmaskSize,
                                 1 + Utility::BitScanReverse64(bitmask));
    for (int ii = 0; ii < numBits; ++ii)
    {
        if ((bitmask & (1ULL << ii)) == 0)
            pBitsUsed[0].insert(ii);
        else
            pBitsUsed[1].insert(ii);
    }
}

//--------------------------------------------------------------------
//! \brief Return a set with all ints from 0 to numValues-1
//!
set<int> FrameBufferTest::AllValues(int numValues)
{
    MASSERT(numValues >= 0);
    MASSERT(numValues < MAX_DECODE_FIELD);

    set<int> values;
    for (int ii = 0; ii < numValues; ++ii)
    {
        values.insert(ii);
    }
    return values;
}
