/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "device/include/bufbuild.h"
#include "core/include/tee.h"
#include "core/include/rc.h"
#include "cheetah/include/tegradrf.h"
#include "core/include/memfrag.h"
#include "cheetah/include/logmacros.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "BufBuild"

enum
{
    PRD_INFO = 0,           // For bufbuild use
    PRD_PARAMETERS = 1,     // For bufbuild use
    TEST_INFO_PRIVATE = 2,  // For private test print
};

BufBuild::BufBuild()
{
    LOG_ENT();

    // Params pass to BufManager, default HW limitation
    m_HwMaxFragments = 1<<16;
    m_HwMinDataWidth = 1;
    m_AddrBits = MEM_ADDRESS_32BIT;
    m_MemAttr = Memory::UC;
    m_MaxFragSize = 1<<20;
    m_BoundaryMask = 0;
    // PRD params
    p_SizeMin = 0;
    p_SizeMax = 0;
    p_NumMin = 1;
    p_NumMax = 1;
    p_OffMin = 0;
    p_OffMax = 0;
    p_OffStep = 0;
    p_DataWidth = 1;
    p_Align = 0;
    p_IsShuffle = true;
    // custom mode params
    m_vCstmSize.clear();
    m_vCstmOff.clear();
    m_LwstomSizeLength = 0;
    m_LwstomOffsetLength = 0;
    m_LwstomLength = 0;
    LOG_EXT();
}

BufBuild::~BufBuild()
{
    LOG_ENT();
    m_vCstmSize.clear();
    m_vCstmOff.clear();

    Clear();
    LOG_EXT();
}

RC BufBuild::Clear(UINT64 BufferIndex /* = 0*/)
{
    LOG_ENT();

    Nodes::iterator nodeIter = m_BufNodeMap.begin();
    for (; nodeIter != m_BufNodeMap.end(); )
    {
        if (BufferIndex && (BufferIndex != nodeIter->first))
        {   // skip if not the expected BufMgr
            ++nodeIter;
            continue;
        }
        BufferMgr *pBuf = (BufferMgr *)nodeIter->first;
        if (pBuf)
        {
            delete pBuf;
        }
        BufNode & node = nodeIter->second;
        node.FragListMap.clear();
        node.AllocInfo.clear();

        m_BufNodeMap.erase(nodeIter++);
    }
    //m_BufNodeMap.clear();
    LOG_EXT();
    return OK;
}

RC BufBuild::PrintProperties()
{
    Printf(Tee::PriHigh, "    BSizeMin   : %5d    Mininum size for each buffer fragment\n", p_SizeMin);
    Printf(Tee::PriHigh, "    BSizeMax   : %5d    Maxinum size for each buffer fragment\n", p_SizeMax);
    Printf(Tee::PriHigh, "    BNumMin    : %5d    Mininum number of buffers fragment\n", p_NumMin);
    Printf(Tee::PriHigh, "    BNumMax    : %5d    Maxinum number of buffers fragment\n", p_NumMax);
    Printf(Tee::PriHigh, "    BOffMin    : %5d    Mininum offset for each buffer fragment\n", p_OffMin);
    Printf(Tee::PriHigh, "    BOffMax    : %5d    Maxinum offset for each buffer fragment\n", p_OffMax);
    Printf(Tee::PriHigh, "    BOffStep   : %5d    Offset inc. step\n", p_OffStep);
    Printf(Tee::PriHigh, "    BDataWidth : %5d    DataWidth / Buffer granularity\n", p_DataWidth);
    Printf(Tee::PriHigh, "    BAlign     : %5d    Alignment (Address of each buffer fragment = Align + Offset)\n", p_Align);
    Printf(Tee::PriHigh, "    IsBShuffle :     %s    Whether to use shuffle mode \n", p_IsShuffle?"T":"F");

    PrintLwstomProperties();
    return OK;
}

RC BufBuild::PrintTestInfo()
{
    Printf(Tee::PriHigh, "Bit %d - Print PRD size or offset Info\n", PRD_INFO);
    Printf(Tee::PriHigh, "Bit %d - Print PRD parameters from user's input\n", PRD_PARAMETERS);
    return OK;
}

RC BufBuild::PrintLwstomProperties()
{
    if (m_LwstomSizeLength)
    {
        Printf(Tee::PriHigh, "Custom Size setting:\n");
        for (UINT32 i = 0; i < m_LwstomSizeLength; i++)
            Printf(Tee::PriHigh, "  Index  %3d,  size   0x%08x\n", i, m_vCstmSize[i]);
    }

    if (m_LwstomOffsetLength)
    {
        Printf(Tee::PriHigh, "Custom Offset setting:\n");
        for (UINT32 i = 0; i < m_LwstomOffsetLength; i++)
            Printf(Tee::PriHigh, "  Index  %3d,  offset 0x%08x\n", i, m_vCstmOff[i]);
    }
    Printf(Tee::PriHigh, "\n");

    return OK;
}

//! \brief this function is called by javascript function to transfer user's custom setting
RC BufBuild::SetLwstomProperties
(
    vector<UINT32> *pvBufSize,
    vector<UINT32> *pvBufOff
)
{
    LOG_ENT();
    UINT32 itemSize = 0, itemOff  = 0;

    m_vCstmSize.clear();
    m_vCstmOff.clear();

    if (pvBufSize)
    {
        itemSize = pvBufSize->size();
    }

    if (pvBufOff)
    {
        itemOff = pvBufOff->size();
    }

    if (itemSize)
    {
        Printf(Tee::PriDebug, "User input buffer sizes:\n");
        for (UINT32 i = 0; i < itemSize; ++i)
        {
                Printf(Tee::PriDebug, "index 0x%x, size 0x%x\n", i, (*pvBufSize)[i]);
        }
    }

    if (itemOff)
    {
        Printf(Tee::PriDebug, "User input buffer offsets:\n");
        for (UINT32 i = 0; i < itemOff; ++i)
        {
            Printf(Tee::PriDebug, "index 0x%x, offset 0x%x\n", i, (*pvBufOff)[i]);
        }
    }

    if (itemSize && itemOff && itemSize != itemOff)
    {
        Printf(Tee::PriError, "The length of Buf Size Array is not equal to "
                              "length of Buf Off Array\n");
        return RC::BAD_PARAMETER;
    }

    for (UINT32 i = 0; i < itemSize; ++i)
    {
        UINT32 size = (*pvBufSize)[i];
        if (!size)
        {
            Printf(Tee::PriError, "index 0x%x, size == 0\n", i);
            return RC::BAD_PARAMETER;
        }

        m_vCstmSize.push_back(size);
    }

    for (UINT32 i = 0; i < itemOff; ++i)
    {
        m_vCstmOff.push_back((*pvBufOff)[i]);
    }

    m_LwstomSizeLength = itemSize;
    m_LwstomOffsetLength = itemOff;
    m_LwstomLength = itemSize ? itemSize : itemOff;

    // Now, update TotalBytes of BufNode  to zero and
    // tell code to re-run update custom setting into BufNode,
    // if users want to update custom setting with this function
    // in interactive mode
    Nodes::iterator iNode = m_BufNodeMap.begin();

    for ( ; iNode != m_BufNodeMap.end(); ++iNode)
    {
        iNode->second.TotalBytes = 0;
    }

    LOG_EXT();
    return OK;
}

    // More parameters check would be proceeded in bufmgr.
    // This function only handles some own-defined input
    // such as average-mode, size-fix mode, check the random range
    // and the case of setting sizeMax = TotalBytes
RC BufBuild::ValidateSizeNum(BufferMgr *pBufMgr,
                             UINT32 TotalBytes,
                             UINT32 DataWidth,
                             UINT32& SizeMin,
                             UINT32& SizeMax,
                             UINT32& NumMin,
                             UINT32& NumMax,
                             UINT32& Align)
{   // once the exact mode has been determined and verified, returns OK.
    LOG_ENT();
    RC rc;
    CHECK_RC(ValidateAlign(Align));

    if (!pBufMgr)
    {
        Printf(Tee::PriError, "BufManager Null pointer\n");
        return RC::ILWALID_INPUT;
    }
    if ( 0 == DataWidth )
    {
        Printf(Tee::PriError, "DataWidth didn't set\n");
        return RC::ILWALID_INPUT;
    }
    if ( 0 == TotalBytes )
    {
        Printf(Tee::PriError, "TotalBytes didn't set\n");
        return RC::ILWALID_INPUT;
    }

    if (SizeMax == 0 && SizeMin)
    {   // need Totalbytes as default maximum value
        Printf(Tee::PriLow, "MODS set SizeMax to %d\n", TotalBytes);
        SizeMax = TotalBytes;
    }

    UINT32 sizeLimit = pBufMgr->GetMaxFragSize();
    UINT32 boundaryMask = pBufMgr->GetBoundaryMask();
    UINT32 chunkSize = pBufMgr->GetChunkSize();
    //---- Custom Mode
    if ( m_LwstomLength )
    {
        for (UINT32 i = 0; i < m_LwstomLength; i++)
        {
            if ( m_LwstomSizeLength )
            {   // size check
                if ( m_vCstmSize[i] > sizeLimit )
                {
                    Printf(Tee::PriError, "Segment %u: Size 0x%x exceeds HW fragment "
                                          "size limit 0x%x\n",
                           i, m_vCstmSize[i], sizeLimit);
                    CHECK_RC_CLEANUP(RC::ILWALID_INPUT);
                }
            }
            if ( m_LwstomOffsetLength )
            {
                if ( ( m_vCstmOff[i] >= Align ) && (Align > 1) )
                {   // if not in Gap mode, offset should not be larger than Alignment, this makes no sense
                    Printf(Tee::PriError, "Segment %u: Offset %u exceeds Alignment %u\n",
                           i, m_vCstmOff[i], Align);
                    CHECK_RC_CLEANUP(RC::ILWALID_INPUT);
                }
            }

            UINT32 cstmSize = m_LwstomSizeLength?m_vCstmSize[i]:0;
            UINT32 cstmOff = m_LwstomOffsetLength?m_vCstmOff[i]:0;
            if ( ( cstmSize + cstmOff ) > chunkSize )
            {
                Printf(Tee::PriError, "Segment %u: Size %u + Offset %u exceeds "
                                      "SW memory chunk size limit %u\n",
                       i, cstmSize, cstmOff, chunkSize);
                CHECK_RC_CLEANUP(RC::ILWALID_INPUT);
            }
            if ( boundaryMask )
            {   // offset + size shouldn't cross the doundary
                if ( ( cstmSize + cstmOff ) > boundaryMask + 1 )
                {
                    Printf(Tee::PriError, "Segment %u: Size %u + Offset %u exceeds "
                                          "HW boundary limit %u\n",
                           i, cstmSize, cstmOff, boundaryMask + 1);
                    CHECK_RC_CLEANUP(RC::ILWALID_INPUT);
                }
            }
        }
        return OK;
    }

    //---- Average Size Mode, check for Num setting
    if (SizeMax == 0 && SizeMin == 0)
    {
        if (NumMin == NumMax && NumMin)
        {
            if ( (TotalBytes / NumMax) > sizeLimit )
            {
                Printf(Tee::PriError, "Average segment size %u: exceeds HW limit 0x%x\n",
                       TotalBytes / NumMax, sizeLimit);
                CHECK_RC_CLEANUP(RC::ILWALID_INPUT);
            }
            return OK;
        }
        Printf(Tee::PriError, "Average mode. NumMin %u, NumMax %u is not equal.\n", NumMin, NumMax);
        CHECK_RC_CLEANUP(RC::ILWALID_INPUT);
    }
    //---- Fixed Size Mode, determain the Num
    else if (NumMin == 0 && NumMax == 0)
    {   // Size fix mode would be used. In this mode, p_NumMin and p_NumMax are initialized to zero.
        // Calwlate the num value dynamically accroding to totalbytes
        if (SizeMin == SizeMax && SizeMin)
        {
            if (TotalBytes % SizeMin)
            {
                Printf(Tee::PriError, "SizeFix mode. TotalBytes (0x%x) can't be "
                                      "divided by Size (0x%x)\n",
                       TotalBytes, SizeMin);
                CHECK_RC_CLEANUP(RC::ILWALID_INPUT);
            }
            NumMin = NumMax = TotalBytes / SizeMin;
            return OK;
        }

        Printf(Tee::PriError, "No valid fragement size provided (SizeMin = SizeMax), "
                              "can't calwlate the number of fragments\n");
        CHECK_RC_CLEANUP(RC::ILWALID_INPUT);
    }
    // Need to dynamically adjust the range between p_NumMin and p_NumMax according to dynamical Totalbytes.
    // Often, users choose a large range of buffer numbers and directly transfer these parameters into bufmgr.
    // If no adjustment is made, error would happen easily.
    //---- Random Mode
    if (NumMin && (NumMin <= NumMax) && SizeMin && (SizeMin <= SizeMax))
    {
        if (SizeMin == SizeMax && (TotalBytes % SizeMin))
        {
            Printf(Tee::PriError, "TotalBytes (0x%x) can't be divided by fixSize (0x%x)\n",
                   TotalBytes, SizeMin);
            return RC::ILWALID_INPUT;
        }

        UINT32 minDataElem = SizeMin / DataWidth;
        if(SizeMin & (DataWidth-1))
        {   //MinFragSize is not aligned to DataWidth, round up
            ++minDataElem;
        }
        UINT32 minSizeAligned = minDataElem * DataWidth;

        if (SizeMax > sizeLimit)
        {
            Printf(Tee::PriLow, "Changing SizeMax %u to HW MaxFragmengSize %u\n",
                   SizeMax, sizeLimit);
            SizeMax = sizeLimit;
        }
        if (SizeMax > TotalBytes)
        {
            Printf(Tee::PriLow, "Changing SizeMax %u to TotalBytes %u\n",
                   SizeMax, TotalBytes);
            SizeMax = TotalBytes;
        }

        UINT32 maxSizeAligned = SizeMax & ~(DataWidth - 1); //round down
        if (0 == maxSizeAligned)
        {
            Printf(Tee::PriError, "MaxSize %u round down with DataWidth %u = 0\n",
                   SizeMax, DataWidth);
            return RC::BAD_PARAMETER;
        }
        UINT32 minFragLimit = TotalBytes / maxSizeAligned;
        if (TotalBytes % maxSizeAligned)
        {
            ++minFragLimit;
        }

        UINT32 maxFragLimit = TotalBytes / minSizeAligned;
        if (TotalBytes % maxFragLimit)
        {
            ++maxFragLimit;
        }

        NumMin = max(NumMin, minFragLimit);
        NumMax = min(NumMax, maxFragLimit);

        if (NumMin > NumMax)
        {
            Printf(Tee::PriError, "Can't get the valid segment array with given parameters\n");
            CHECK_RC_CLEANUP(RC::ILWALID_INPUT);
        }
        return OK;
    }

Cleanup:
    Printf(Tee::PriError, "Invalid parameters combination: \n  BSizeMin (0x%x), "
                          "BSizeMax (0x%x) BNumMin 0x%x BNumMax 0x%x\n",
           SizeMin, SizeMax, NumMin, NumMax);
    return RC::ILWALID_INPUT;
}

RC BufBuild::ValidateAlign(UINT32& Align)
{
    LOG_ENT();
    if (Align)
    {
        return OK;
    }

    // Only when alignment parameter has no-zero value, inputed offset value
    // can be used. We set to no-zero value in order to use offset function of bufmgr
    if ((m_vCstmOff.size() || p_OffMin || p_OffMax) && !Align)
    {
        Printf(Tee::PriWarn, "Offset is non-zero, MODS set Align to 4096 automatically\n");
        Align = 4096;
    }
    LOG_EXT();
    return OK;
}

RC BufBuild::ValidateIndex(UINT64 Index)
{
    if (m_BufNodeMap.find(Index) == m_BufNodeMap.end())
    {
        Printf(Tee::PriError, "ValidateIndex: Invalid Index(0x%08llx) \n", Index);
        return RC::BAD_PARAMETER;
    }
    return OK;
}

bool BufBuild::IsKeyPresent(UINT64 Index, UINT32 Key)
{
    if (OK != ValidateIndex(Index))
    {
        return false;
    }

    BufNode & node = m_BufNodeMap[Index];

    if (node.FragListMap.find(Key) == node.FragListMap.end())
    {
        return false;
    }
    return true;
}

RC BufBuild::ValidateKey(UINT64 Index, UINT32 Key)
{
    if (!IsKeyPresent(Index, Key))
    {
        Printf(Tee::PriError, "ValidateKey: Invalid key(0x%x) \n", Key);
        return RC::BAD_PARAMETER;
    }
    return OK;
}

//! \brief each Index responds to a BufMgr object
//  In fact, the Index is the poiner of BufMgr
RC BufBuild::GetBufferMgr(UINT64 Index, BufferMgr **ppBufMgr)
{
    RC rc = OK;

    if (!ppBufMgr)
    {
        Printf(Tee::PriError, "GetBufferMgr: ppBufMgr pointer is null\n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(ValidateIndex(Index));
    *ppBufMgr = (BufferMgr*)Index;
    return OK;
}

RC BufBuild::GetFragList(UINT64 Index, Frags **ppFragList, UINT32 Key)
{
    RC rc = OK;

    if (!ppFragList)
    {
        Printf(Tee::PriError, "GetFragList: ppFragList is null\n");
        return RC::BAD_PARAMETER;
    }

    *ppFragList = NULL;
    CHECK_RC(ValidateKey(Index, Key));
    *ppFragList = &((m_BufNodeMap[Index].FragListMap)[Key]);

    return OK;
}

UINT64 BufBuild::FindFragmentList(Frags *pFragList, UINT32 *pNumOfFrags)
{
    LOG_ENT();
    if (pNumOfFrags)
    {
        *pNumOfFrags = 0;
    }
    UINT64 index = 0;
    map<UINT64,BufNode>::iterator it0 = m_BufNodeMap.begin();
    for(; it0 != m_BufNodeMap.end(); ++it0)
    {
        index = it0->first;
        if (OK != ValidateIndex(index))
        {
            return 0;
        }

        map<UINT32, Frags>::iterator it1 = m_BufNodeMap[index].FragListMap.begin();
        for(; it1 != m_BufNodeMap[index].FragListMap.end(); ++it1)
        {
            if (pFragList == &(it1->second))
            {
                Printf(Tee::PriLow, "Found the FragmentList with vAddr %p\n", pFragList);
                if (pNumOfFrags)
                {
                    *pNumOfFrags = m_BufNodeMap[index].FragListMap.size();
                }
                return index;
            }
        }
    }
    LOG_EXT();
    return 0;
}

RC BufBuild::GenerateNumFrags(BufferMgr* pBufMgr,
                              UINT32  TotalBytes,
                              UINT32  DataWidth,
                              Allocs* pAllocList,
                              UINT32  NumMin,
                              UINT32  NumMax)
{
    LOG_ENT();
    RC rc = OK;
    if (!pBufMgr)
    {
        Printf(Tee::PriError, "Buf Manager NULL Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    // If custom parameters have been setup we do nothing.
    if (m_LwstomLength)
    {
        return OK;
    }
    CHECK_RC(pBufMgr->CallwlateNumFragments(pAllocList,
                                            TotalBytes,
                                            DataWidth,
                                            NumMin,
                                            NumMax));
    LOG_EXT();
    return OK;
}

RC BufBuild::GenerateFragSizes(BufferMgr *pBufMgr,
                               UINT32 TotalBytes,
                               UINT32 DataWidth,
                               Allocs *pAllocList,
                               UINT32 SizeMin,
                               UINT32 SizeMax)
{
    LOG_ENT();
    RC rc = OK;
    if (!pBufMgr)
    {
        Printf(Tee::PriError, "Buf Manager NULL Pointer\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_LwstomSizeLength)
    {
        return OK;
    }

    //SizeMax == 0, use averageMode, would be true
    CHECK_RC(pBufMgr->CallwlateFragSizes(pAllocList,
                                         TotalBytes,
                                         DataWidth,
                                         SizeMax ? true : false,
                                         SizeMin,
                                         SizeMax));
    LOG_EXT();
    return OK;
}

RC BufBuild::GenerateOffsets(BufferMgr *pBufMgr,
                             UINT32 TotalBytes,
                             UINT32 DataWidth,
                             Allocs *pAllocList,
                             UINT32 Align)
{
    LOG_ENT();
    RC rc = OK;
    if (!pBufMgr)
    {
        Printf(Tee::PriError, "Buf Manager NULL Pointer\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_LwstomOffsetLength)
        return OK;

    if(Align)
    {
            CHECK_RC(pBufMgr->CallwlateOffsets(pAllocList,
                                               DataWidth,
                                               p_OffMin,
                                               p_OffMax,
                                               p_OffStep));
    }
    else
    {
            CHECK_RC(pBufMgr->CallwlateGaps(pAllocList,
                                            true,
                                            TotalBytes,
                                            DataWidth,
                                            p_IsShuffle));
    }

    LOG_EXT();
    return OK;
}

/* UpdateLwstomProperties   - save custom setting for each bufmgr object
 *
 */

RC BufBuild::UpdateLwstomProperties(Allocs *pAllocInfo)
{
    if (!pAllocInfo)
    {
        Printf(Tee::PriError, "UpdaeCstm: pAllocInfo is null\n");
        return RC::BAD_PARAMETER;
    }

    if (!m_LwstomLength)
        return OK;

    if (m_LwstomSizeLength && m_LwstomOffsetLength && (m_LwstomSizeLength != m_LwstomOffsetLength))
    {
        Printf(Tee::PriError, "Error cstmSize len is not equalt to cstmOff len\n");
        return RC::ILWALID_INPUT;
    }

    pAllocInfo->clear();
    pAllocInfo->resize(m_LwstomLength);
    if (m_LwstomSizeLength)
    {
        for (UINT32 i = 0; i < m_LwstomSizeLength; i++)
        {
            (*pAllocInfo)[i].size =  m_vCstmSize[i];
        }
    }

    if (m_LwstomOffsetLength)
    {
        for (UINT32 i = 0; i < m_LwstomOffsetLength; i++)
        {
            (*pAllocInfo)[i].offset = m_vCstmOff[i];
        }
    }

    return OK;
}

RC BufBuild::PrintParameters()
{
    Printf(Tee::PriHigh, "\nBufBuild parameters:");
    Printf(Tee::PriHigh, "BSizeMin   0x%x, BSizeMax  0x%x, BNumMin   0x%x, BNumMax   0x%x",
             p_SizeMin, p_SizeMax, p_NumMin, p_NumMax);
    Printf(Tee::PriHigh, "BOffMin    0x%x, BOffMax   0x%x, BOffStep  0x%x",
             p_OffMin, p_OffMax, p_OffStep);
    Printf(Tee::PriHigh, "BAlign     0x%x, IsShuffle: %s,  DataWidth 0x%x",
            p_Align, p_IsShuffle ? "true" : "false", p_DataWidth);

    PrintLwstomProperties();
    return OK;
}

RC BufBuild::PrintPriParams(UINT64 Index)
{
    Nodes::iterator iNode = m_BufNodeMap.find(Index);
    if (iNode == m_BufNodeMap.end())
    {
        Printf(Tee::PriError, "Buffer index does not exist\n");
        return RC::BAD_PARAMETER;
    }
    PriPara* pPriPara = &(iNode->second.BufParam);

    if ( RF_VAL(FRAGLIST_MIN_LENGTH:FRAGLIST_MIN_LENGTH, pPriPara->setProperties) )
        Printf(Tee::PriNormal, "  NumMin %u",pPriPara->numMin);
    if ( RF_VAL(FRAGLIST_MAX_LENGTH:FRAGLIST_MAX_LENGTH, pPriPara->setProperties) )
        Printf(Tee::PriNormal, "  NumMax %u",pPriPara->numMax);
    if ( RF_VAL(FRAGLIST_MIN_SIZE:FRAGLIST_MIN_SIZE, pPriPara->setProperties) )
        Printf(Tee::PriNormal, "  SizeMin %u",pPriPara->sizeMin);
    if ( RF_VAL(FRAGLIST_MAX_SIZE:FRAGLIST_MAX_SIZE, pPriPara->setProperties) )
        Printf(Tee::PriNormal, "  SizeMax %u",pPriPara->sizeMax);
    if ( RF_VAL(FRAGLIST_MIN_OFFSET:FRAGLIST_MIN_OFFSET, pPriPara->setProperties) )
        Printf(Tee::PriNormal, "  OffsetMin %u",pPriPara->offsetMin);
    if ( RF_VAL(FRAGLIST_MAX_OFFSET:FRAGLIST_MAX_OFFSET, pPriPara->setProperties) )
        Printf(Tee::PriNormal, "  OffsetMax %u",pPriPara->offsetMax);
    if ( RF_VAL(FRAGLIST_DATAWIDTH:FRAGLIST_DATAWIDTH, pPriPara->setProperties) )
        Printf(Tee::PriNormal, "  DataWidth %u",pPriPara->dataWidth);
    if ( RF_VAL(FRAGLIST_ALIGN:FRAGLIST_ALIGN, pPriPara->setProperties) )
        Printf(Tee::PriNormal, "  Align %u",pPriPara->align);
    if ( RF_VAL(FRAGLIST_IS_SHUFFLE:FRAGLIST_IS_SHUFFLE, pPriPara->setProperties) )
        Printf(Tee::PriNormal, "  IsShuffle %s",pPriPara->isShuffle?"T":"F");

    return OK;
}

RC BufBuild::GenerateFragmentList(UINT64 Index,
                                  UINT32 TotalBytes,
                                  Frags **ppFragList,
                                  UINT32 Key,
                                  UINT32 PrintMask)
{
    LOG_ENT();
    RC rc = OK;

    if (!TotalBytes)
    {
        Printf(Tee::PriError, "TotalBytes is zero\n");
        return RC::BAD_PARAMETER;
    }

    Nodes::iterator iNode = m_BufNodeMap.find(Index);
    if (iNode == m_BufNodeMap.end())
    {
        Printf(Tee::PriError, "Buffer index does not exist\n");
        return RC::BAD_PARAMETER;
    }

    PriPara* pPriPara = &(iNode->second.BufParam);
    Allocs* pAllocInfo = &(iNode->second.AllocInfo);

    BufferMgr* pBufMgr = NULL;
    CHECK_RC(GetBufferMgr(Index, &pBufMgr));

    if (PrintMask & 0x1 /*IsPrint(McpTest::PRD_PARAMETERS)*/)
    {
        Printf(Tee::PriHigh, "TotalBytes 0x%x", TotalBytes);
        if (pPriPara->setProperties)
        {
            Printf(Tee::PriNormal, "Using private setting:");
            PrintPriParams(Index);
        }
        PrintParameters();
    }

    if (!iNode->second.TotalBytes) //first run
    {
        // Save custom parameters into allocInfo structure once
        CHECK_RC(UpdateLwstomProperties(pAllocInfo));
    }
    if (iNode->second.TotalBytes != TotalBytes)
    {
        iNode->second.TotalBytes = TotalBytes;
    }

    CHECK_RC(GenerateFragListProperties(pPriPara,
                                        pBufMgr,
                                        pAllocInfo,
                                        TotalBytes));

    Frags  *pvFragList = NULL;
    bool   isFragListPresent = true;
    Frags  tmpFragList;

    // Check the Key presence. If not, we create it,
    // Otherwise, we directly use it.
    if (IsKeyPresent(Index, Key))
    {
        CHECK_RC(GetFragList(Index, &pvFragList, Key));
    }
    else
    {
        isFragListPresent = false;
        pvFragList = &tmpFragList;
    }

    // handle the possible shuffle update
    bool isShuffle = (RF_VAL(FRAGLIST_IS_SHUFFLE:FRAGLIST_IS_SHUFFLE,pPriPara->setProperties)) ?
                     (pPriPara->isShuffle != 0) : p_IsShuffle;
    CHECK_RC(pBufMgr->GenerateFixedFragList(pvFragList,
                                            pAllocInfo,
                                            TotalBytes,
                                            p_DataWidth,
                                            p_Align,
                                            true,
                                            isShuffle));

    if (!isFragListPresent)
    {
        (iNode->second.FragListMap)[Key] = *pvFragList;
    }

    pvFragList = &(iNode->second.FragListMap)[Key];

    if (ppFragList)
    {
        *ppFragList = pvFragList;
    }

    // For -testInfo operation, print each fraglist size, virtAddr, physAddr
    if (pvFragList && (PrintMask & 0x2) /*IsPrint(McpTest::PRD_INFO)*/)
    {
        Printf(Tee::PriHigh, "Generated FragList info:");
        MemoryFragment::PrintFragList(pvFragList,Tee::PriNormal);
    }

    LOG_EXT();
    return OK;
}

// private method
RC BufBuild::GenerateFragListProperties
(
    PriPara* pPriPara,
    BufferMgr* pBufMgr,
    Allocs* pAllocInfo,
    UINT32 TotalBytes
)
{
    LOG_ENT();
    RC rc = OK;

    // The fragment list properties can be set in 3 different ways:
    // 1. Specific to each buffer, private params - by SetupFragListProperty()
    // 2. By setting custom properties - interactive / JS mode
    // 3. Public global parameters which are valid across bufbuild

    bool isSkipNum  = m_LwstomLength ? true : false;
    bool isSkipSize = m_LwstomSizeLength ? true : false;
    bool isSkipOffset  = m_LwstomOffsetLength ? true : false;

    // Also if user provides a private struct by calling SetFragListProperty()
    if (pPriPara->setProperties)
    {
        isSkipNum = isSkipSize = isSkipOffset = false;
    }

    // When the parameters have been setup using SetFragListProperty() we need to make sure that
    // its called.
    UINT32  sizeMin = (RF_VAL(FRAGLIST_MIN_SIZE:FRAGLIST_MIN_SIZE,pPriPara->setProperties)) ?
                       pPriPara->sizeMin : p_SizeMin;
    UINT32  sizeMax = (RF_VAL(FRAGLIST_MAX_SIZE:FRAGLIST_MAX_SIZE,pPriPara->setProperties)) ?
                       pPriPara->sizeMax : p_SizeMax;
    UINT32  numMin =  (RF_VAL(FRAGLIST_MIN_LENGTH:FRAGLIST_MIN_LENGTH,pPriPara->setProperties)) ?
                       pPriPara->numMin : p_NumMin;
    UINT32  numMax =  (RF_VAL(FRAGLIST_MAX_LENGTH:FRAGLIST_MAX_LENGTH,pPriPara->setProperties)) ?
                       pPriPara->numMax : p_NumMax;
    UINT32  dataWidth = (RF_VAL(FRAGLIST_DATAWIDTH:FRAGLIST_DATAWIDTH,pPriPara->setProperties)) ?
                        pPriPara->dataWidth : p_DataWidth;
    UINT32  align = p_Align;

    // Some parameters need to be set dynamically
    // Some parameters need to be limited to a safe range.
    // We don't write anything to member variables since they are used across BufBuild for different
    // buffer managers. Instead we save the values to the local variables.
    CHECK_RC(ValidateSizeNum(pBufMgr, TotalBytes, dataWidth, sizeMin, sizeMax, numMin, numMax, align));
    // CHECK_RC(ValidateAlign(align));

    if (!isSkipNum)
    {
            CHECK_RC(GenerateNumFrags(pBufMgr, TotalBytes, dataWidth, pAllocInfo, numMin, numMax));
    }

    if (!isSkipSize)
    {
            CHECK_RC(GenerateFragSizes(pBufMgr, TotalBytes, dataWidth, pAllocInfo, sizeMin, sizeMax));
    }

    if (!isSkipOffset)
    {
            CHECK_RC(GenerateOffsets(pBufMgr, TotalBytes, dataWidth, pAllocInfo, align));
    }

    bool isChanged = false;
    if (numMin != p_NumMin || numMax != p_NumMax
        || sizeMin != p_SizeMin || sizeMax != p_SizeMax
        || align != p_Align )
    {
        isChanged = true;
    }
    Printf(Tee::PriLow, "Ajusted parameters: \n");
    if (isChanged)
    {
        if (numMin != p_NumMin || numMax != p_NumMax)
        {
            Printf(Tee::PriLow, "  BNumMin    0x%x, BNumMax   0x%x\n", numMin, numMax);
        }
        if (sizeMin != p_SizeMin || sizeMax != p_SizeMax)
        {
            Printf(Tee::PriLow, "  BSizeMin   0x%x, BSizeMax  0x%x\n", sizeMin, sizeMax);
        }
        if (align != p_Align)
        {
            Printf(Tee::PriLow, "  Align      0x%x\n", align);
        }
    }
    else
    {
        Printf(Tee::PriLow, "  None\n");
    }

    LOG_EXT();
    return rc;
}

RC BufBuild::SetAttr(UINT32 HwMaxFragments,
                     UINT32 HwMinDataWidth,
                     UINT32 HwMaxFragSize,
                     UINT32 AddrBits,
                     Memory::Attrib MemAttr,
                     UINT32 BoundaryMask)
{
    if ( 0 == HwMinDataWidth)
    {
        Printf(Tee::PriError, "Invalid DataWidth 0\n");
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

RC BufBuild::CreateBufMgrs(V_UINT64 *pvIndexReturned,
                           UINT32 BufferIndex,
                           UINT32 TotalBytes,
                           Controller *pCtrl)
{
    RC rc = OK;
    LOG_ENT();
    BufferMgr *pBufMgr = NULL;

    if (!pvIndexReturned)
    {
        Printf(Tee::PriError, "BufBuild->CreateBuffer: pvIndexReturned is null\n");
        return RC::BAD_PARAMETER;
    }
    if (!BufferIndex)
    {
        Printf(Tee::PriError, "BufBuild->CreateBuffer: BufferIndex (0x0) should be "
                              "greater than zero\n");
        return RC::BAD_PARAMETER;
    }
    pvIndexReturned->clear();

    for(UINT32 i = 0; i < BufferIndex; ++i)
    {
        pBufMgr = new BufferMgr(pCtrl);
        if (!pBufMgr)
        {
            Printf(Tee::PriError, "Can't create BufferMgr object\n");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        CHECK_RC_CLEANUP(pBufMgr->InitBufferAttr(m_HwMaxFragments,  //maximum PRD table length
                                                 m_HwMinDataWidth,  //minimum data unit size
                                                 m_MaxFragSize,     //maximum buffer size pointed by a single PRD entry
                                                 m_AddrBits,        //address bits used to allocate memory
                                                 m_MemAttr,
                                                 m_BoundaryMask));  //memory type allocated

        // initialize the size and content of the memory block
        if (TotalBytes)
        {
            CHECK_RC_CLEANUP(pBufMgr->AllocMemBuffer(TotalBytes,      //memory size requested
                                                     p_Align ? true : false,
                                                     p_Align));  //whether the memory is aligned and the alignment
        }
        else
        {
            Printf(Tee::PriDebug, "No memory allocated for created buffer manager\n");
        }

        BufNode node;
        node.FragListMap.clear();
        node.TotalBytes = 0;
        Memory::Fill08(static_cast<void*>(&node.BufParam),
                       0,
                       sizeof(node.BufParam));
        node.BufParam.setProperties = 0;    // on peatrans this field is not cleared for some reason

        m_BufNodeMap[(UINT64)pBufMgr] = node;   // For each BufMgr, we allocate a BufNode structure
                                                // to save relevant info. We save all the BufNodes into
                                                // m_BufNodeMap. Each BufNode's key is the pointer of its' BufMgr
                                                // object.

        pvIndexReturned->push_back((UINT64)pBufMgr);
    }
    LOG_EXT();
    return OK;

Cleanup:
    if (pBufMgr)
    {
        delete pBufMgr;
    }

    LOG_EXT();
    return rc;
}

//! \ Debug function
RC BufBuild::PrintNode()
{
    Printf(Tee::PriHigh, "Current BufNodeMap has %ld BufMgr\n", m_BufNodeMap.size());
    Nodes::iterator iter = m_BufNodeMap.begin();
    for (;iter != m_BufNodeMap.end(); iter++)
    {
        BufNode & bufNode = iter->second;
        Printf(Tee::PriHigh, "Bufmgr obj vAddr:  0x%llx", iter->first);
        Printf(Tee::PriHigh, "BufNode obj vAddr: 0x%llx",  (UINT64)&bufNode);
        Printf(Tee::PriHigh, "Number of Fragelist %d", (UINT32)bufNode.FragListMap.size());

        FragsMap & fragsListMap = bufNode.FragListMap;
        FragsMap::iterator fragMapIter = fragsListMap.begin();
        for (; fragMapIter != fragsListMap.end(); fragMapIter++)
        {
            Printf(Tee::PriHigh, "-- Key : 0x%x  Fraglist obj vaddr: 0x%llx --\n",
                    fragMapIter->first,
                    (UINT64)&(fragMapIter->second));
            MemoryFragment::PrintFragList(&(fragMapIter->second));
        }
    }
    return OK;
}

RC BufBuild::SetFragListProperty
(
    UINT64 BufferIndex,
    UINT32 Property,
    UINT32 Value
)
{
    RC rc = OK;
    Nodes::iterator iNode = m_BufNodeMap.find(BufferIndex);

    if (iNode == m_BufNodeMap.end())
    {
        Printf(Tee::PriError, "Invalid Buffer Index\n");
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        switch(Property)
        {
        case FRAGLIST_MIN_LENGTH:
            iNode->second.BufParam.numMin = Value;
            iNode->second.BufParam.setProperties |=
                RF_SET(FRAGLIST_MIN_LENGTH:FRAGLIST_MIN_LENGTH);
            break;
        case FRAGLIST_MAX_LENGTH:
            iNode->second.BufParam.numMax = Value;
            iNode->second.BufParam.setProperties |=
                RF_SET(FRAGLIST_MAX_LENGTH:FRAGLIST_MAX_LENGTH);
            break;
        case FRAGLIST_MIN_SIZE:
            iNode->second.BufParam.sizeMin = Value;
            iNode->second.BufParam.setProperties |=
                RF_SET(FRAGLIST_MIN_SIZE:FRAGLIST_MIN_SIZE);
            break;
        case FRAGLIST_MAX_SIZE:
            iNode->second.BufParam.sizeMax = Value;
            iNode->second.BufParam.setProperties |=
                RF_SET(FRAGLIST_MAX_SIZE:FRAGLIST_MAX_SIZE);
            break;
        case FRAGLIST_MIN_OFFSET:
            iNode->second.BufParam.offsetMin = Value;
            iNode->second.BufParam.setProperties |=
                RF_SET(FRAGLIST_MIN_OFFSET:FRAGLIST_MIN_OFFSET);
            break;
        case FRAGLIST_MAX_OFFSET:
            iNode->second.BufParam.offsetMax = Value;
            iNode->second.BufParam.setProperties |=
                RF_SET(FRAGLIST_MAX_OFFSET:FRAGLIST_MAX_OFFSET);
            break;
        case FRAGLIST_DATAWIDTH:
            iNode->second.BufParam.dataWidth = Value;
            iNode->second.BufParam.setProperties |=
                RF_SET(FRAGLIST_DATAWIDTH:FRAGLIST_DATAWIDTH);
            break;
        case FRAGLIST_ALIGN:
            iNode->second.BufParam.align = Value;
            iNode->second.BufParam.setProperties |=
                RF_SET(FRAGLIST_ALIGN:FRAGLIST_ALIGN);
            break;
        case FRAGLIST_IS_SHUFFLE:
            iNode->second.BufParam.isShuffle = Value;
            iNode->second.BufParam.setProperties |=
                RF_SET(FRAGLIST_IS_SHUFFLE:FRAGLIST_IS_SHUFFLE);
            break;
        default:
            Printf(Tee::PriError, "Unknown Property : %d\n", Property);
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

// FillPattern are not recommended to use, use MemoryFragment::Fill Pattern instead
RC BufBuild::FillPattern(UINT64 Index, V_UINT08 *pPattern)
{
    RC rc;

    BufferMgr *pBufMgr = NULL;
    CHECK_RC(GetBufferMgr(Index, &pBufMgr));
    CHECK_RC(pBufMgr->FillPattern(pPattern));
    return OK;
}

RC BufBuild::FillPattern(UINT64 Index, V_UINT16 *pPattern)
{
    RC rc;
    BufferMgr *pBufMgr = NULL;
    CHECK_RC(GetBufferMgr(Index, &pBufMgr));
    CHECK_RC(pBufMgr->FillPattern(pPattern));
    return OK;
}

RC BufBuild::FillPattern(UINT64 Index, V_UINT32 *pPattern)
{
    RC rc;
    BufferMgr *pBufMgr = NULL;
    CHECK_RC(GetBufferMgr(Index, &pBufMgr));
    CHECK_RC(pBufMgr->FillPattern(pPattern));
    return OK;
}
