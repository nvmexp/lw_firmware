/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "device/include/bufmgr.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "device/include/memtrk.h"
#include "lwtypes.h"
#include <algorithm>
#include "cheetah/include/logmacros.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "BufferMgr"

BufferMgr::BufferMgr(Controller *pCtrl)
{
    m_vMemBlocks.clear();

    m_HwMaxFragments = 0;
    m_HwMinDataWidth = 0;
    //set default address bits
    m_AddrBits = MEM_ADDRESS_32BIT;
    //set default to UC
    m_MemAttr = Memory::UC;
    m_MaxFragSize = ~0x0;
    m_BoundaryMask = 0;
    m_pCtrl = pCtrl;
}

BufferMgr::~BufferMgr()
{
    ClearBuffer();
}

RC BufferMgr::InitBufferAttr( UINT32 HwMaxFragments,
                              UINT32 HwMinDataWidth,
                              UINT32 HwMaxFragSize,
                              UINT32 AddrBits,
                              Memory::Attrib MemAttr,
                              UINT32  BoundaryMask)
{
    if( HwMaxFragments == 0 )
    {
        Printf(Tee::PriError, "HwMaxFragments must >0, invalid value %d specified\n",
               HwMaxFragments);
        return RC::BAD_PARAMETER;
    }
    if( (HwMinDataWidth&(HwMinDataWidth-1)))
    {
        Printf(Tee::PriError, "Invalid HwMinDataWidth %d, only powers of 2 are supported.\n",
               HwMinDataWidth);
        return RC::BAD_PARAMETER;
    }
    if( HwMaxFragSize == 0 )
    {
        Printf(Tee::PriError, "HwMaxFragSize must >0, invalid value %d specified.\n",
               HwMaxFragSize);
        return RC::BAD_PARAMETER;
    }
    if( (MemAttr < Memory::UC)
        || (MemAttr > Memory::WB) )
    {
        Printf(Tee::PriError, "Invalid MemAttr %d, only 1-5 are supported.\n",
               MemAttr);
        return RC::BAD_PARAMETER;
    }

    m_HwMaxFragments = HwMaxFragments;
    m_HwMinDataWidth = HwMinDataWidth;
    m_MaxFragSize = HwMaxFragSize;
    m_AddrBits = AddrBits;
    m_MemAttr = MemAttr;
    m_BoundaryMask = BoundaryMask;
    return OK;
}

namespace
{
    // This is required by std::shuffle()
    class RandomGenerator
    {
        public:
            RandomGenerator()
            {
                m_Rnd.SeedRandom(static_cast<UINT32>(Xp::GetWallTimeNS()));
            }

            UINT32 operator()()
            {
                return m_Rnd.GetRandom();
            }

            using result_type = UINT32;
            static constexpr UINT32 min() { return 0; }
            static constexpr UINT32 max() { return ~0U; }

        private:
            Random m_Rnd;
    };
}

RC BufferMgr::GenerateFragmentList(MemoryFragment::FRAGLIST *pFragList,
                                   UINT08 Modes,
                                   UINT32 Alignment,
                                   UINT32 TotalBytes,
                                   UINT32 DataWidth,
                                   AllocParam* pNumFragments,
                                   AllocParam* pFragSizes,
                                   AllocParam* pOffsets,
                                   bool IsRealloc,
                                   bool IsShuffle)
{
    RC rc = OK;

    //check validity of input parameters
    if( (Modes&MODE_ALIGN) && Alignment == 0 )
    {
        Printf(Tee::PriError, " Alignment must >0 in aligned allocation.\n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(CheckDataWidth(DataWidth));
    CHECK_RC(CheckSize(TotalBytes, DataWidth));

    if( (!pFragList) || (!pNumFragments)  || (!pFragSizes) || (!pOffsets))
    {
        Printf(Tee::PriError, " Input pointer is invalid.\n");
        return RC::BAD_PARAMETER;
    }

    if( !IsRealloc )
    {
        if( m_vMemBlocks.size()==0 )
        {
            Printf(Tee::PriError, " No buffer initialized for PRDs.\n");
            return RC::TEGRA_TEST_ILWALID_SETUP;
        }
        if( TotalMemSize() < TotalBytes )
        {
            Printf(Tee::PriError, " Allocated memory is too small to hold %d bytes.\n",
                   TotalBytes);
            return RC::BAD_PARAMETER;
        }
    }

    vector<AllocInfo> allocList;

    pFragList->clear();
    //start the following procedure
    // 1. callwlate the number of fragments
    CHECK_RC(CallwlateNumFragments(&allocList,
                                   TotalBytes,
                                   DataWidth,
                                   pNumFragments->minNum,
                                   pNumFragments->maxNum));

    // 2. callwlate the byte size for each fragment
    CHECK_RC(CallwlateFragSizes(&allocList,
                                TotalBytes,
                                DataWidth,
                                Modes & MODE_RND_FRAGSIZE,
                                pFragSizes->minNum,
                                pFragSizes->maxNum));
    // 3.A callwlate offset for each fragment if Mode&MODE_ALIGN == 1
    if(Modes&MODE_ALIGN)
    {
        CHECK_RC(CallwlateOffsets(&allocList,
                                  DataWidth,
                                  pOffsets->minNum,
                                  pOffsets->maxNum,
                                  pOffsets->stepSize));
    }
    // 3.B callwlate gaps for each fragment if Mode&MODE_ALIGN == 0
    else
    {

        CHECK_RC(CallwlateGaps(&allocList,
                               Modes & MODE_RND_GAP,
                               TotalBytes,
                               DataWidth,
                               IsRealloc));
    }

    // 4. allocate the memory for the fragment list according to the allocation list
    UINT32 align = (Modes&MODE_ALIGN) ? Alignment : 0;
    //allocate aligned memory
    CHECK_RC(AllocMemory(pFragList, &allocList, align, DataWidth, IsRealloc));

    if(IsShuffle)
    {
        RandomGenerator rnd;
        shuffle(pFragList->begin(), pFragList->end(), rnd);
    }

    return OK;
}

RC BufferMgr::GenerateFixedFragList(MemoryFragment::FRAGLIST *pFragList,
                                    vector<AllocInfo> *pAllocList,
                                    UINT32 TotalBytes,
                                    UINT32 DataWidth,
                                    UINT32 Alignment,
                                    bool IsRealloc,
                                    bool IsShuffle)

{
    RC rc;

    CHECK_RC(CheckDataWidth(DataWidth));
    CHECK_RC(CheckSize(TotalBytes, DataWidth));
    CHECK_RC(CheckVector(pAllocList, true));

    if(!IsRealloc &&  m_vMemBlocks.size()==0 )
    {
        Printf(Tee::PriError, " No buffer initialized for PRDs.\n");
        return RC::TEGRA_TEST_ILWALID_SETUP;
    }

    // Check size
    UINT32 totalSize = 0;
    UINT32 tmpSize = 0;
    UINT32 off = 0;

    for (UINT32 i = 0; i < pAllocList->size(); i++)
    {
        tmpSize = (*pAllocList)[i].size;
        if (!tmpSize || tmpSize < m_HwMinDataWidth || tmpSize > m_MaxFragSize)
        {
            Printf(Tee::PriError, "FixedAlloc index(0x%x): wrong size 0x%x\n",
                   i, tmpSize);
            return RC::BAD_PARAMETER;
        }
        totalSize += tmpSize;

        off = (*pAllocList)[i].offset;
        if (off % DataWidth)
        {
            Printf(Tee::PriError, "FixedAlloc index(0x%x): the off (0x%x) doesn't "
                                  "align with datawidth (0x%x)\n",
                   i, off, DataWidth);
            return RC::BAD_PARAMETER;
        }

    }

    if (totalSize != TotalBytes)
    {
        Printf(Tee::PriError, " The totalSize (0x%x) of FixedAlloc doesn't match "
                              "the totalBytes(0x%x)\n",
               totalSize, TotalBytes);
        return RC::BAD_PARAMETER;
    }

    if(!IsRealloc  && TotalMemSize() < totalSize   )
    {
        Printf(Tee::PriError, " Allocated memory is too small to hold %d bytes "
                              "for FixedAlloc.\n",
               totalSize);
        return RC::BAD_PARAMETER;
    }

    pFragList->clear();

    // 4. allocate the memory for the fragment list according to the allocation list
    CHECK_RC(AllocMemory(pFragList, pAllocList, Alignment, DataWidth, IsRealloc));

    if(IsShuffle)
    {
        RandomGenerator rnd;
        shuffle(pFragList->begin(), pFragList->end(), rnd);
    }

    return OK;
}

RC BufferMgr::CallwlateFragSizes(vector<AllocInfo> *pAllocList,
                                 UINT32 TotalBytes,
                                 UINT32 DataWidth,
                                 bool   IsRndMode,
                                 UINT32 MinSize,
                                 UINT32 MaxSize)

{
    RC rc;

    if(IsRndMode)
    {
        //generate random size of fragments
        CHECK_RC(GenerateRandomFragSizes(pAllocList,
                                         TotalBytes,
                                         DataWidth,
                                         MinSize,
                                         MaxSize));
    }
    else
    {
        //generate fixed size of fragments
        CHECK_RC(GenerateAverageFragSizes(pAllocList,
                                          TotalBytes,
                                          DataWidth));
    }

    return OK;
}

RC BufferMgr::CallwlateGaps(vector<AllocInfo> *pAllocList,
                            bool   IsRndMode /*=true*/,
                            UINT32 TotalBytes,
                            UINT32 DataWidth,
                            bool IsRealloc)
{
    RC rc;

    //tentatively, default setting is to take 20% of TotalBytes as extra bytes in non-aligned allocation
    UINT32 extraBytes = Xp::GetPageSize();
    UINT32 totalMem = TotalMemSize();
    if(!IsRealloc)
    {
        if( extraBytes + TotalBytes > totalMem )
        {
            extraBytes = totalMem - TotalBytes;
        }
    }

    Printf(Tee::PriDebug, "extraBytes in all gaps: %d\n", extraBytes);

    if(IsRndMode)
        //random size for gaps
        CHECK_RC(GenerateRandomGaps(pAllocList, extraBytes, DataWidth, 0, extraBytes));
    else
        //average size for gaps
        CHECK_RC(GenerateAverageGaps(pAllocList, extraBytes, DataWidth));

    return OK;
}

RC BufferMgr::AllocMemBuffer(UINT32 ByteSize,
                             bool IsAligned,
                             UINT32 Alignment)
{
    RC rc;
    if(ByteSize<=0)
    {
        Printf(Tee::PriError, " Can not init buffer with zero size.\n");
        return RC::BAD_PARAMETER;
    }

    //allocate memory blocks
    CHECK_RC(ClearBuffer());

    UINT32 numBlocks = ByteSize/m_MaxFragSize;
    if(ByteSize%m_MaxFragSize)
    {
        numBlocks++;
    }

    UINT32 remainSize = ByteSize;

    for(UINT32 i=0; i<numBlocks; i++)
    {
        UINT32 allocSize = (i==numBlocks-1) ? remainSize : m_MaxFragSize;
        CHECK_RC(GrowMemBlock(allocSize, Alignment));
        remainSize -= allocSize;
    }

    return OK;
}

RC BufferMgr::FillPattern(vector<UINT08> *pPattern)
{
    RC rc;

    if(pPattern==NULL)
    {
        Printf(Tee::PriError, " Input pointer pPattern=NULL.\n");
        return RC::BAD_PARAMETER;
    }

    UINT32 iBlock;
    UINT32 numBlocks = (UINT32)m_vMemBlocks.size();

    for(iBlock=0; iBlock<numBlocks; ++iBlock)
    {
        CHECK_RC(Memory::FillPattern(m_vMemBlocks[iBlock].pVA,
                                     pPattern,
                                     m_vMemBlocks[iBlock].size));
    }

    return OK;
}

RC BufferMgr::FillPattern(vector<UINT16> *pPattern)
{
    RC rc;

    if(pPattern==NULL)
    {
        Printf(Tee::PriError, " Input pointer pPattern=NULL.\n");
        return RC::BAD_PARAMETER;
    }

    UINT32 iBlocks;
    UINT32 numBlocks = (UINT32)m_vMemBlocks.size();

    for(iBlocks=0; iBlocks<numBlocks; ++iBlocks)
    {
       CHECK_RC(Memory::FillPattern(m_vMemBlocks[iBlocks].pVA,
                                    pPattern,
                                    m_vMemBlocks[iBlocks].size));
    }

    return OK;
}

RC BufferMgr::FillPattern(vector<UINT32> *pPattern)
{
    RC rc;

    if(pPattern==NULL)
    {
        Printf(Tee::PriError, " Input pointer pPattern=NULL.\n");
        return RC::BAD_PARAMETER;
    }

    UINT32 iBlocks;
    UINT32 numBlocks = (UINT32)m_vMemBlocks.size();

    for(iBlocks=0; iBlocks<numBlocks; ++iBlocks)
    {
       CHECK_RC(Memory::FillPattern(m_vMemBlocks[iBlocks].pVA,
                                     pPattern,
                                     m_vMemBlocks[iBlocks].size));
    }

    return OK;
}

RC BufferMgr::CheckSize(UINT32 TotalBytes, UINT32 DataWidth /*=0*/)
{
    if( TotalBytes == 0 )
    {
        Printf(Tee::PriError, " TotalBytes must >0, invalid value %d.\n", TotalBytes);
        return RC::BAD_PARAMETER;
    }
    if (!DataWidth)
    {
        return OK;
    }

    if (TotalBytes & (DataWidth - 1))
    {
        Printf(Tee::PriError, "TotalByte 0x%x must be aligned with dataWidth 0x%x\n",
               TotalBytes,
               DataWidth);
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC BufferMgr::CheckDataWidth(UINT32  DataWidth)
{
    if( DataWidth < m_HwMinDataWidth )
    {
        Printf(Tee::PriError, "DataWidth %d less than the minimum value %d "
                              "limited by the harware.\n",
               DataWidth, m_HwMinDataWidth);
        return RC::BAD_PARAMETER;
    }

    if( DataWidth&(DataWidth-1) )
    {
        Printf(Tee::PriError, " Invalid DataWidth %d.\n", DataWidth);
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC BufferMgr::CheckVector(vector<AllocInfo>* pAllocList, bool IsCheckSize /*= true*/)
{
    if(!pAllocList)
    {
        Printf(Tee::PriError, " Invalid input pointer pAllocList.\n");
        return RC::BAD_PARAMETER;
    }
    if(IsCheckSize == true && pAllocList->size()==0)
    {
        Printf(Tee::PriError, " Cannot generate fragment sizes for an empty fragment list.\n");
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC BufferMgr::CheckMinMax(UINT32 Min, UINT32 Max, bool IsCheckZero /*=true*/)
{
    RC rc = OK;
    if (Min > Max)
    {
        rc = RC::BAD_PARAMETER;
    }
    if (IsCheckZero)
    {
        if ((Min == 0 && Max == 0) || Min == 0)
        {
            rc = RC::BAD_PARAMETER;
        }
    }
    if (rc != OK)
        Printf(Tee::PriError, " Invalid Min / Max set (%d, %d).\n", Min, Max);

    return rc;
}

// merge the User input params with HW limitation, and size up the array
// dependency:
// m_MaxFragSize, m_HwMaxFragments
RC BufferMgr::CallwlateNumFragments(vector<AllocInfo>* pAllocList,
                                    UINT32 TotalBytes,
                                    UINT32 DataWidth,
                                    UINT32 MinFragments,
                                    UINT32 MaxFragments)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(CheckVector(pAllocList, false));
    CHECK_RC(CheckSize(TotalBytes, DataWidth));
    CHECK_RC(CheckMinMax(MinFragments, MaxFragments));

    //clear the list of allocations first
    pAllocList->clear();
    //check the number of fragments against TotalBytes and memory block size
    UINT32 minFrags = TotalBytes / m_MaxFragSize;
    if(TotalBytes % m_MaxFragSize)
    {
        minFrags++;
    }

    MinFragments = max(MinFragments, minFrags);
    MaxFragments = min(MaxFragments, m_HwMaxFragments);

    if (MinFragments > MaxFragments)
    {
        Printf(Tee::PriError, "Inputed range(MinFragments 0x%x, MaxFragments 0x%x)"
                              "doesn't match the range (MinFrags 0x%x, m_HwMaxFragments 0x%x)\n",
               MinFragments, MaxFragments, minFrags, m_HwMaxFragments);

        Printf(Tee::PriError, "TotalBytes: 0x%x,m_MaxFragSize 0x%x\n",
               TotalBytes, m_MaxFragSize);

        return RC::BAD_PARAMETER;
    }

    if(MinFragments == MaxFragments)
    {
        pAllocList->resize(MinFragments);
        return OK;
    }

    //random num
    UINT32 numFrags = DefaultRandom::GetRandom(MinFragments, MaxFragments);
    pAllocList->resize(numFrags);
    LOG_EXT();
    return OK;
}

RC BufferMgr::GenerateRandomFragSizes(vector<AllocInfo>* pAllocList,
                                      UINT32 TotalBytes,
                                      UINT32 DataWidth,
                                      UINT32 MinFragSize,
                                      UINT32 MaxFragSize)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(CheckVector(pAllocList));
    CHECK_RC(CheckDataWidth(DataWidth));
    CHECK_RC(CheckSize(TotalBytes, DataWidth));
    CHECK_RC(CheckMinMax(MinFragSize, MaxFragSize));

    if ((MinFragSize == MaxFragSize) && (MinFragSize & (DataWidth - 1)))
    {
        Printf(Tee::PriError, "FragSize (0x%x) doesn't match DataWidth (0x%x)\n",
               MinFragSize, DataWidth);
        return RC::BAD_PARAMETER;
    }
    // limit fragment sizes to m_MaxFragSize
    if( MinFragSize > m_MaxFragSize )
    {
        Printf(Tee::PriError, " MinFragSize %d greater than Maximum Memory Block size %d\n",
                  MinFragSize, m_MaxFragSize);
        return RC::BAD_PARAMETER;
    }
    if( MaxFragSize > m_MaxFragSize )
    {
        Printf(Tee::PriWarn, "MaxFragSize %d greater than Maximum Memory Block size %d\n",
               MaxFragSize, m_MaxFragSize);
        Printf(Tee::PriWarn, " Setting MaxFragSize to %d\n", m_MaxFragSize);
        MaxFragSize = m_MaxFragSize;
    }

    UINT32 minDataElem = MinFragSize / DataWidth;
    if( MinFragSize&(DataWidth-1) )
    {
        //MinFragSize is not aligned to DataWidth, round up
        minDataElem++;
    }

    UINT32 maxDataElem = MaxFragSize / DataWidth;
    if( ( MaxFragSize&(DataWidth-1) ) && maxDataElem )
    {
        //MaxFragSize is not aligned to DataWidth, round down
        maxDataElem--;
    }

    UINT32 totalDataElem = TotalBytes / DataWidth;

    UINT32 numFrags = (UINT32)pAllocList->size();

    //Check possibility of the requested allocation
    if( (numFrags > totalDataElem)
        || (minDataElem*numFrags > totalDataElem)
        || (maxDataElem*numFrags < totalDataElem) )
    {
        Printf(Tee::PriError, "Invalid input parameters. "
                              "Can not allocate %d fragments with size in (%d, %d).\n",
               numFrags, MinFragSize, MaxFragSize);
        return RC::BAD_PARAMETER;
    }

    DefaultRandom::SeedRandom((UINT32)Xp::QueryPerformanceCounter());

    UINT32 extraElem = totalDataElem - minDataElem*numFrags;
    if(minDataElem == maxDataElem)
    {
        //reduced to fixed size allocation case
        for(UINT32 i=0; i<numFrags; i++)
        {
            (*pAllocList)[i].size = minDataElem*DataWidth;
            extraElem -= minDataElem;
        }
        return OK;
    }

    //random size allocation
    for(UINT32 i=0; i<numFrags; i++)
    {
        UINT32 fragElem;

        UINT32 minSize = extraElem > (maxDataElem-minDataElem)*(numFrags-i-1) ?
                         (extraElem - (maxDataElem-minDataElem)*(numFrags-i-1)) : 0;

        fragElem = minDataElem + DefaultRandom::GetRandom(minSize, min(extraElem, maxDataElem - minDataElem));

        extraElem -= (fragElem - minDataElem);
        (*pAllocList)[i].size = fragElem*DataWidth;
    }

    LOG_EXT();
    return OK;
}

RC BufferMgr::GenerateAverageFragSizes(vector<AllocInfo>* pAllocList,
                                       UINT32 TotalBytes,
                                       UINT32 DataWidth)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(CheckVector(pAllocList));
    CHECK_RC(CheckDataWidth(DataWidth));
    CHECK_RC(CheckSize(TotalBytes, DataWidth));

    UINT32 totalDataElem = TotalBytes / DataWidth;
    UINT32 numFrags = (UINT32)pAllocList->size();
    if( (totalDataElem < numFrags)
        || (TotalBytes > TotalMemSize()) )
    {
        Printf(Tee::PriError, "Can not distribute %d bytes to %d fragments.\n",
               TotalBytes, numFrags);
        return RC::BAD_PARAMETER;
    }

    //callwlate the average fragment size
    UINT32 avgElem = totalDataElem / numFrags;
    UINT32 extraElem = totalDataElem - avgElem * numFrags;

    for(UINT32 i=0; i<numFrags; i++)
    {
        UINT32 fragElem = avgElem;
        if(i < extraElem)
        {
            fragElem++;
        }
        (*pAllocList)[i].size = fragElem * DataWidth;
    }
    LOG_EXT();
    return OK;
}

RC BufferMgr::CallwlateOffsets(vector<AllocInfo>* pAllocList,
                               UINT32 DataWidth,
                               UINT32 MinOffset,
                               UINT32 MaxOffset,
                               UINT32 StepSize  /*=0*/)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(CheckVector(pAllocList));
    CHECK_RC(CheckDataWidth(DataWidth));
    CHECK_RC(CheckMinMax(MinOffset, MaxOffset, false));

    UINT32 numFrags = (UINT32)pAllocList->size();
    UINT32 minElem = MinOffset / DataWidth;
    if( MinOffset&(DataWidth-1) )
        minElem++;

    UINT32 maxElem = MaxOffset / DataWidth;
    if(( MaxOffset&(DataWidth-1)) && (maxElem))
        maxElem--;

    UINT32 stepElem = StepSize / DataWidth;
    if( StepSize&(DataWidth-1) )
        stepElem++;

    if( minElem == maxElem )
    {
        for(UINT32 i=0; i<numFrags; i++)
        {
            (*pAllocList)[i].offset = minElem*DataWidth;
        }
    }
    else if ( StepSize ) // step
    {
        UINT32 totalSteps = 0;
        for(UINT32 i=0; i<numFrags; i++)
        {
            (*pAllocList)[i].offset = (minElem + totalSteps)*DataWidth;
            totalSteps = (totalSteps+stepElem)%(maxElem - minElem); // maxElem != minElem
        }
    }
    else
    {
        for(UINT32 i=0; i<numFrags; i++)
        {
            UINT32 maxOffset = (m_MaxFragSize - (*pAllocList)[i].size) / DataWidth;
            if( m_BoundaryMask )
            {
                UINT32 tmp = (m_BoundaryMask + 1 - (*pAllocList)[i].size) / DataWidth;
                maxOffset = min(maxOffset, tmp);
            }
            maxElem = min(maxOffset, maxElem);

            if(maxElem < minElem)
            {
                Printf(Tee::PriError, "Bad parameter combination\n");
                return RC::BAD_PARAMETER;
            }
            (*pAllocList)[i].offset = DefaultRandom::GetRandom(minElem, maxElem) * DataWidth;
        }
    }

    // fine tune, boundary check and protect
    for(UINT32 i=0; i<numFrags; i++)
    {
        if( ( (*pAllocList)[i].offset + (*pAllocList)[i].size ) > m_MaxFragSize )
        {
            (*pAllocList)[i].offset = (m_MaxFragSize - (*pAllocList)[i].size) & (~(DataWidth - 1));
        }
    }
    if( m_BoundaryMask )
    {
        for(UINT32 i=0; i<numFrags; i++)
        {
            if( ( (*pAllocList)[i].offset + (*pAllocList)[i].size ) > ( m_BoundaryMask + 1 ) )
            {
                (*pAllocList)[i].offset = (( m_BoundaryMask + 1 ) - (*pAllocList)[i].size) & (~(DataWidth - 1));
            }
        }
    }

    LOG_EXT();
    return OK;
}

RC BufferMgr::GenerateRandomGaps(vector<AllocInfo>* pAllocList,
                                 UINT32 TotalExtraBytes,
                                 UINT32 DataWidth,
                                 UINT32 MinGapSize,
                                 UINT32 MaxGapSize)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(CheckVector(pAllocList));
    CHECK_RC(CheckDataWidth(DataWidth));
    CHECK_RC(CheckMinMax(MinGapSize, MaxGapSize, false));

    UINT32 numFrags = (UINT32)pAllocList->size();

    UINT32 minGapElem = MinGapSize/DataWidth;
    if( MinGapSize&(DataWidth-1) )
        minGapElem++;

    UINT32 maxGapElem = MaxGapSize/DataWidth;
    if( MaxGapSize&(DataWidth-1) )
        maxGapElem++;

    UINT32 totalGapElem = TotalExtraBytes/DataWidth;
    if( TotalExtraBytes&(DataWidth-1) )
        totalGapElem++;

    //Check possibility of the requested allocation
    if( (minGapElem*numFrags > totalGapElem)
        || (maxGapElem*numFrags < totalGapElem) )
    {
        Printf(Tee::PriError, "Invalid input parameters. Can not allocate "
                              "%d bytes with size in (%d, %d).\n",
               TotalExtraBytes, MinGapSize, MaxGapSize);
        return RC::BAD_PARAMETER;
    }

    UINT32 extraElem = totalGapElem - minGapElem*numFrags;
    if(minGapElem==maxGapElem)
    {
        //reduced to fixed size allocation case
        for(UINT32 i=0; i<numFrags; i++)
        {
            (*pAllocList)[i].offset = minGapElem*DataWidth;
            if( (*pAllocList)[i].offset + (*pAllocList)[i].size > m_MaxFragSize )
                (*pAllocList)[i].offset = (m_MaxFragSize - (*pAllocList)[i].size)&(~(DataWidth - 1));

            extraElem -= (*pAllocList)[i].offset/DataWidth;
        }
        return OK;
    }

    //random size allocation
    for(UINT32 i=0; i<numFrags; i++)
    {
        UINT32 gapElem;

        UINT32 minSize = extraElem > (maxGapElem-minGapElem)*(numFrags-i-1) ?
                         (extraElem - (maxGapElem-minGapElem)*(numFrags-i-1)) : 0;
        UINT32 maxSize = min(extraElem, maxGapElem - minGapElem);
        gapElem = minGapElem + DefaultRandom::GetRandom(0, maxSize);
        gapElem = max(gapElem, minSize + minGapElem);

        if(i == numFrags-1)
            gapElem = extraElem + minGapElem;

        if( gapElem*DataWidth + (*pAllocList)[i].size > m_MaxFragSize )
            gapElem = (m_MaxFragSize - (*pAllocList)[i].size)/DataWidth;

        if(gapElem > minGapElem)
            extraElem -= (gapElem - minGapElem);
        else
            extraElem += (minGapElem - gapElem);
        (*pAllocList)[i].offset = gapElem*DataWidth;
    }
    LOG_EXT();
    return OK;
}

RC BufferMgr::GenerateAverageGaps(vector<AllocInfo>* pAllocList,
                                  UINT32 TotalExtraBytes,
                                  UINT32 DataWidth)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(CheckVector(pAllocList));
    CHECK_RC(CheckDataWidth(DataWidth));
    UINT32 totalGapElem = TotalExtraBytes/DataWidth;

    if(TotalExtraBytes&(DataWidth-1) )
        //allocBytes is not aligned to DataWidth, round up
        totalGapElem++;

    UINT32 numFrags = (UINT32)pAllocList->size();

    //callwlate the average fragment size
    UINT32 avgGapElem=totalGapElem/numFrags;
    UINT32 extraElem = totalGapElem - avgGapElem*numFrags;
    UINT32 remainElem = totalGapElem;

    for(UINT32 i=0; i<numFrags; i++)
    {
        UINT32 gapElem;

        gapElem = avgGapElem;

        if(extraElem > 0)
        {
            //randomly distribute the extra elements
            gapElem += DefaultRandom::GetRandom(0,extraElem);
        }

        if(i == numFrags-1)
            gapElem = remainElem;

        if( gapElem*DataWidth + (*pAllocList)[i].size > m_MaxFragSize )
            gapElem = (m_MaxFragSize - (*pAllocList)[i].size)/DataWidth;

        if(gapElem > avgGapElem)
            extraElem -= (gapElem - avgGapElem);
        else
            extraElem += (avgGapElem - gapElem);

        (*pAllocList)[i].offset = gapElem*DataWidth;
        remainElem -= gapElem;
    }
    LOG_EXT();
    return OK;
}

RC BufferMgr::GrowMemBlock(size_t Size, UINT32 Align)
{
    RC rc;
    LOG_ENT();
    //allocate new continuous memory blocks to hold the requested buffers
    MemBlock newMemBlock;
    if (Align)
    {
        CHECK_RC(MemoryTracker::AllocBufferAligned(Size,
                                                   &newMemBlock.pVA,
                                                   Align,
                                                   m_AddrBits,
                                                   m_MemAttr,
                                                   m_pCtrl));
    }
    else
        CHECK_RC(MemoryTracker::AllocBuffer(Size,
                                            &newMemBlock.pVA,
                                            true,
                                            m_AddrBits,
                                            m_MemAttr,
                                            m_pCtrl));

    newMemBlock.size = Size;
    m_vMemBlocks.push_back(newMemBlock);
    LOG_EXT();
    return OK;
}

#define round_up(x, y) (((x) + (y) - 1) & ~((y) - 1))
#define round_down(x, y) ((x) & ~((y) - 1))

RC BufferMgr::AllocMemory(MemoryFragment::FRAGLIST *pFragList,
                          vector<AllocInfo>* pAllocList,
                          UINT32 Alignment,
                          UINT32 DataWidth,
                          bool IsRealloc)
{
    RC rc;
    LOG_ENT();
    CHECK_RC(CheckVector(pAllocList));
    CHECK_RC(CheckDataWidth(DataWidth));
    if( Alignment&(Alignment-1) )
    {
        Printf(Tee::PriError, "Invalid Alignment 0x%08x. Must be power of 2.\n",
               Alignment);
        return RC::BAD_PARAMETER;
    }

    if (Alignment)
    {
        //set the alignment to be the larger one between Alignment and DataWidth
        Alignment = max(Alignment,DataWidth);

        if( (!IsRealloc) && (m_vMemBlocks.size()>0) )
        {
            if( ((LwUPtr)m_vMemBlocks[0].pVA)&(Alignment-1) )
            {
                Printf(Tee::PriError, "pre-allocated buffer address does not match "
                                      "the requested alignment.\n");
                return RC::TEGRA_TEST_ILWALID_SETUP;
            }
        }
    }

    UINT32 iFrags = 0, iBlocks = 0;
    UINT32 numFrags = (UINT32)pAllocList->size();
    UINT32 numBlocks = (UINT32)m_vMemBlocks.size();
    bool   isAlignBoundary = false; // alloc memory aligning on boundary.

    //set the number of fragment list accordingly
    pFragList->clear();
    pFragList->resize(pAllocList->size());
    do
    {
        // Step1:
        // If there are no mem blocks or we use up all the mem Blocks,
        // allocate one more block
        if(iBlocks >= numBlocks)
        {
            if(!IsRealloc)
            {
                pFragList->clear();
                Printf(Tee::PriError, "Exceeds static assigned buffer size.\n");
                return RC::BAD_PARAMETER;
            }

            UINT32 align = Alignment;
            if (isAlignBoundary)
            {
                if (align < m_BoundaryMask + 1)
                    align = m_BoundaryMask + 1;
            }
            // Try to allocate memory with m_BoudnaryMask + 1 if required.
            rc = GrowMemBlock(m_MaxFragSize, align);
            // Probably the allocation fails because of inappropriate m_BoudnaryMask value,
            // then allocate memory again with default Alignment value.
            // Lwrrently, ide has 64k boundary mask restriction.
            if (rc != OK && isAlignBoundary)
                CHECK_RC(GrowMemBlock(m_MaxFragSize, Alignment));

            numBlocks++;
        }

        // Step2: use mem block from the beginning iBlocks = 0
        MemBlock  & block = m_vMemBlocks[iBlocks];
        // align on datawidth

        void *pBlockEnd = (void*)((char*)block.pVA + block.size - 1);

        Printf(Tee::PriDebug, "iblock 0x%x, blksize 0x%x, VirtStart 0x%p, VirtEnd 0x%p\n",
               iBlocks, block.size, block.pVA, pBlockEnd);

        UINT64  phyStart = Platform::VirtualToPhysical(block.pVA);
        UINT64  phyEnd = phyStart + block.size - 1;
        if (phyEnd < phyStart)
            phyEnd = ~((UINT64)0);

        phyStart = round_up(phyStart, (UINT64)DataWidth);

        // Step3: on the current mem block, try to allocate memory that pAllocList needed
        // till current mem block meets the required memory size of pAllocList no longer
        for(;iFrags<numFrags; iFrags++)
        {
            if (Alignment)
                phyStart = round_up(phyStart, (UINT64)Alignment);

            AllocInfo & alloc =(*pAllocList)[iFrags];
            UINT32 fragSize = alloc.size + alloc.offset;

            // Lwrrently memblock has not enough memory for current fragsize of memory
            // Quit current loop and use the next memblock
            if(phyStart + fragSize - 1 > phyEnd)
                break;

            if (m_BoundaryMask) //check boundary
            {
                if (fragSize > m_BoundaryMask + 1)
                {
                    Printf(Tee::PriError, "Fraglist index: 0x%x. The sum of size: "
                                          "0x%x and offset 0x%x is greater than boundaryMask 0x%x\n",
                           iFrags, alloc.size, alloc.offset, m_BoundaryMask);
                    return RC::SOFTWARE_ERROR;
                }

                UINT64 fragPhyStart = phyStart & ~((UINT64)m_BoundaryMask);
                UINT64 fragPhyEnd = phyStart + fragSize - 1;
                fragPhyEnd = fragPhyEnd & ~((UINT64)m_BoundaryMask);

                if (fragPhyStart != fragPhyEnd)
                {
                    // If memory region crosses boundary and no other blocks remain,
                    // tell GrownMemBlock to allocate memory aligning on boundary
                    if (iBlocks == numBlocks - 1)
                        isAlignBoundary = true;

                     break;
                }
            }

            MemoryFragment::FRAGMENT & frag = (*pFragList)[iFrags];

            // Step4: Fill the pFragList with the memory avaliable in memblock
            frag.Address = Platform::PhysicalToVirtual(phyStart + alloc.offset);
            frag.PhysAddr = 0;
            frag.ByteSize = alloc.size;

            Printf(Tee::PriDebug, "iFrag 0x%x, fragSize: 0x%x, VirtStart: 0x%p, virtEnd: 0x%p\n",
                   iFrags, frag.ByteSize, frag.Address,
                   (void *)((uintptr_t)frag.Address + frag.ByteSize - 1));

            phyStart = phyStart + fragSize;
        }
        // Use next memblock for memory allocation
        iBlocks += 1;

    // If we finish memory allocation of all the fraglists of pAllocList, quit the loop
    } while (iFrags < numFrags);

    if(IsRealloc)
        //set index to the first not used block
        CHECK_RC(FreeExtraMem(iBlocks));

    return OK;
}

RC BufferMgr::ClearBuffer()
{
    for(UINT32 i=0; i<m_vMemBlocks.size(); i++)
    {
        MemoryTracker::FreeBuffer(m_vMemBlocks[i].pVA);
    }
    m_vMemBlocks.clear();
    return OK;
}

RC BufferMgr::FreeExtraMem(UINT32 UsedBlocks)
{
    //if used blocks are less than 25% of total blocks
    //release 50% of memory blocks
    UINT32 numBlocks = (UINT32)m_vMemBlocks.size();

    if( UsedBlocks<(numBlocks/4) )
    {
        //pop from the back to avoid moving elements in the vector
        INT32 lastBlock = (INT32)(min(UsedBlocks*2, numBlocks/2));
        for(INT32 i=numBlocks-1; i>=lastBlock; i--)
        {
            MemoryTracker::FreeBuffer(m_vMemBlocks[i].pVA);
            m_vMemBlocks.pop_back();
        }
    }

    return OK;
}

UINT32 BufferMgr::TotalMemSize()
{
    UINT32 numBlocks = (UINT32)m_vMemBlocks.size();
    UINT32 totalSize = 0;

    for(UINT32 i=0; i<numBlocks; i++)
    {
        totalSize += m_vMemBlocks[i].size;
    }

    return totalSize;
}

void BufferMgr::PrintAllocList(vector<AllocInfo>* pAllocList)
{
    if(pAllocList)
    {
        for (UINT32 i = 0; i < pAllocList->size(); ++i)
        {
            Printf(Tee::PriHigh, "Index 0x%x, size 0x%x, off 0x%x\n",
                   i,
                   (*pAllocList)[i].size,
                   (*pAllocList)[i].offset);
         }
    }
}

UINT32 BufferMgr::GetMaxFragSize()
{
    return m_MaxFragSize;
}

void BufferMgr::SetMaxFragSize(UINT32 MaxFragSize)
{
    m_MaxFragSize = MaxFragSize;
}

UINT32 BufferMgr::GetBoundaryMask()
{
    return m_BoundaryMask;
}

UINT32 BufferMgr::GetChunkSize()
{   // return the size parameter passed to GrowMemBlock()
    // so far it's fixed to m_MaxFragSize,
    // later we could add a standalone attrib for this purpose, user could set it
    return m_MaxFragSize;
}

