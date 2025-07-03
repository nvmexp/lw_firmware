/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

#include "core/include/massert.h"
#include "core/include/rc.h"

#include "h26xparser.h"
#include "h264parser.h"
#include "h264syntax.h"

namespace H264
{
using H26X::UnusedPred;
using H26X::IsOutputPred;

unsigned char Default_4x4_Intra[16] =
{
     6, 13, 20, 28,
    13, 20, 28, 32,
    20, 28, 32, 37,
    28, 32 ,37, 42
};

unsigned char Default_4x4_Inter[16] =
{
    10, 14, 20, 24,
    14, 20, 24, 27,
    20, 24, 27, 30,
    24, 27, 30, 34
};

unsigned char Default_8x8_Intra[64] =
{
     6, 10, 13, 16, 18, 23, 25, 27,
    10, 11, 16, 18, 23, 25, 27, 29,
    13, 16, 18, 23, 25, 27, 29, 31,
    16, 18, 23, 25, 27, 29, 31, 33,
    18, 23, 25, 27, 29, 31, 33, 36,
    23, 25, 27, 29, 31, 33, 36, 38,
    25, 27, 29, 31, 33, 36, 38, 40,
    27, 29, 31, 33, 36, 38, 40, 42
};

unsigned char Default_8x8_Inter[64] =
{
     9, 13, 15, 17, 19, 21, 22, 24,
    13, 13, 17, 19, 21, 22, 24, 25,
    15, 17, 19, 21, 22, 24, 25, 27,
    17, 19, 21, 22, 24, 25, 27, 28,
    19, 21, 22, 24, 25, 27, 28, 30,
    21, 22, 24, 25, 27, 28, 30, 32,
    22, 24, 25, 27, 28, 30, 32, 33,
    24, 25, 27, 28, 30, 32, 33, 35
};

template <class T>
struct PicNumPred
{
    PicNumPred(const T &dpb, int picNum)
        : m_dpb(dpb)
        , m_picNum(picNum)
    {}

    bool operator() (const typename T::value_type& e) const
    {
        return e.IsUsedForReference() && !e.IsLongTerm() && e.GetPicNum() == m_picNum;
    }

    bool operator() (size_t idx) const
    {
        return operator()(m_dpb[idx]);
    }

    const T &m_dpb;
    int      m_picNum;
};

template <class T>
struct LongTermPicNumPred
{
    LongTermPicNumPred(const T &dpb, int picNum)
        : m_dpb(dpb)
        , m_picNum(picNum)
    {}

    bool operator() (const typename T::value_type& e) const
    {
        return e.IsUsedForReference() && e.IsLongTerm() && e.GetLongTermPicNum() == m_picNum;
    }

    bool operator() (size_t idx) const
    {
        return operator()(m_dpb[idx]);
    }

    const T &m_dpb;
    int      m_picNum;
};

template <class T>
struct LongTermFrameIdxPred
{
    explicit LongTermFrameIdxPred(int frameIdx)
        : m_frameIdx(frameIdx)
    {}

    bool operator() (const typename T::value_type& e) const
    {
        return e.GetLongTermFrameIdx() == m_frameIdx;
    }

    int m_frameIdx;
};

struct POCOrder
{
    explicit POCOrder(const DecodedPictureSrc *picSrc)
        : m_picSrc(picSrc)
    {}

    bool operator()(size_t idx1, size_t idx2) const
    {
        const Picture *pic1 = m_picSrc->GetPicture(idx1);
        const Picture *pic2 = m_picSrc->GetPicture(idx2);

        return pic1->PicOrderCnt() < pic2->PicOrderCnt();
    }

    const DecodedPictureSrc *m_picSrc;
};

template <class T>
struct IsShortTermPred
{
    bool operator()(const typename T::value_type &e) const
    {
        return e.IsUsedForReference() && !e.IsLongTerm();
    }
};

template <class T>
struct IsLongTermPred
{
    bool operator()(const typename T::value_type &e) const
    {
        return e.IsUsedForReference() && e.IsLongTerm();
    }
};

template <class T>
struct PicNumRefListOrder
{
    explicit PicNumRefListOrder(const T &_dpb)
        : m_dpb(_dpb)
    {}

    bool operator ()(size_t e1, size_t e2) const
    {
        return m_dpb[e1].GetPicNum() > m_dpb[e2].GetPicNum();
    }

    const T &m_dpb;
};

template <class T>
struct LTPicNumRefListOrder
{
    explicit LTPicNumRefListOrder(const T &_dpb)
        : m_dpb(_dpb)
    {}

    bool operator ()(size_t e1, size_t e2) const
    {
        return m_dpb[e1].GetLongTermPicNum() < m_dpb[e2].GetLongTermPicNum();
    }

    const T &m_dpb;
};

template <class T>
PicNumRefListOrder<T>
DescRefListPicNum(const T& dpb)
{
    return PicNumRefListOrder<T>(dpb);
}

template <class T>
LTPicNumRefListOrder<T>
AscenRefListLTPPicNum(const T& dpb)
{
    return LTPicNumRefListOrder<T>(dpb);
}

template <class DPB, class PicCntSrc, class Op>
struct POCRefListOrder
{
    POCRefListOrder(const DPB &dpb, const PicCntSrc *picCntSrc)
        : m_dpb(dpb)
        , m_picCntSrc(picCntSrc)
    {}

    bool operator()(size_t r1, size_t r2) const
    {
        return Op()(m_picCntSrc->GetPicOrderCnt(m_dpb[r1].GetPicIdx()),
                    m_picCntSrc->GetPicOrderCnt(m_dpb[r2].GetPicIdx()));
    }

    const DPB    &m_dpb;
    const PicCntSrc *m_picCntSrc;
};

template <class DPB, class PicSrc>
POCRefListOrder<DPB, PicSrc, less<int> >
AscenRefListPOC(const DPB &dpb, const PicSrc *picSrc)
{
    return POCRefListOrder<DPB, PicSrc, less<int> >(dpb, picSrc);
}

template <class DPB, class PicSrc>
POCRefListOrder<DPB, PicSrc, greater<int> >
DescRefListPOC(const DPB &dpb, const PicSrc *picSrc)
{
    return POCRefListOrder<DPB, PicSrc, greater<int> >(dpb, picSrc);
}

Slice::Slice(const SliceHeader &sliceHeader)
    : m_sliceHeader(sliceHeader)
    , m_DPBSrc(0)
    , m_picOrderCnt(0)
    , m_startOffset(0)
    , m_naluSize(0)
{
}

bool Slice::IsTheSamePic(const Slice& slice)
{
    const SeqParameterSet& sps = GetSPS();
    const PicParameterSet& pps = GetPPS();

    if (PicParameterSetId() != slice.PicParameterSetId())
        return false;
    if (FrameNum() != slice.FrameNum())
        return false;
    if (FieldPicFlag() != slice.FieldPicFlag())
        return false;
    if (FieldPicFlag() && (BottomFieldFlag() != slice.BottomFieldFlag()))
        return false;
    if (NalRefIdc() != slice.NalRefIdc() && (0 == NalRefIdc() || 0 == slice.NalRefIdc()))
        return false;
    if (IsIDR() != slice.IsIDR())
        return false;
    if (IsIDR() && (IDRPicId() != slice.IDRPicId()))
        return false;
    if (0 == sps.pic_order_cnt_type)
    {
        if (PicOderCntLsb() != slice.PicOderCntLsb())
            return false;
        if (pps.bottom_field_pic_order_in_frame_present_flag && !FieldPicFlag())
        {
            if (DeltaPicOderCntBottom() != slice.DeltaPicOderCntBottom())
                return false;
        }
    }
    if (1 == sps.pic_order_cnt_type)
    {
        if (!sps.delta_pic_order_always_zero_flag)
        {
            if (DeltaPicOderCnt(0) != slice.DeltaPicOderCnt(0))
                return false;
            if (pps.bottom_field_pic_order_in_frame_present_flag && !FieldPicFlag())
            {
                if (DeltaPicOderCnt(1) != slice.DeltaPicOderCnt(1))
                    return false;
            }
        }
    }

    return true;
}

void Slice::ReorderRefPicList(vector<size_t> *refList, size_t listIdx)
{
    const DPB& dpb = GetDPB();
    const list<RefListModifier> &mods = RefPicListMods(listIdx);
    int MaxPicNum = this->MaxPicNum();
    int LwrrPicNum = this->LwrrPicNum();
    int picNumLX;
    int picNumLXPred = LwrrPicNum;
    int picNumLXNoWrap;

    list<RefListModifier>::const_iterator it;
    size_t refIdxLX;
    for (it = mods.begin(), refIdxLX = 0; mods.end() != it; ++it, ++refIdxLX)
    {
        if (it->modification_of_pic_nums_idc < 2)
        {
            int abs_diff_pic_num = it->abs_diff_pic_num_minus1 + 1;
            if (it->modification_of_pic_nums_idc == 0)
            {
                if ((picNumLXPred - abs_diff_pic_num) < 0)
                    picNumLXNoWrap = picNumLXPred - abs_diff_pic_num + MaxPicNum;
                else
                    picNumLXNoWrap = picNumLXPred - abs_diff_pic_num;
            }
            else
            {
                if (picNumLXPred + abs_diff_pic_num >= MaxPicNum)
                    picNumLXNoWrap = picNumLXPred + abs_diff_pic_num - MaxPicNum;
                else
                    picNumLXNoWrap = picNumLXPred + abs_diff_pic_num;
            }
            picNumLXPred = picNumLXNoWrap;

            if (picNumLXNoWrap > LwrrPicNum)
                picNumLX = picNumLXNoWrap - MaxPicNum;
            else
                picNumLX = picNumLXNoWrap;
            PicNumPred<DPB> pred(dpb, picNumLX);
            DPB::const_iterator picIt = find_if(dpb.begin(), dpb.end(), pred);
            refList->insert(refList->begin() + refIdxLX, picIt - dpb.begin());
            refList->erase(remove_if(refList->begin() + refIdxLX + 1, refList->end(), pred),
                           refList->end());
        }
        else
        {
            LongTermPicNumPred<DPB> pred(dpb, it->long_term_pic_num);
            DPB::const_iterator picIt = find_if(dpb.begin(), dpb.end(), pred);
            refList->insert(refList->begin() + refIdxLX, picIt - dpb.begin());
            refList->erase(remove_if(refList->begin() + refIdxLX + 1, refList->end(), pred),
                           refList->end());
        }
    }
    refList->resize(NumRefIdxActive(listIdx));
}

void PSlice::DoCreateRefLists()
{
    m_refList.clear();
    DPB::const_iterator it;
    const DPB &dpb = GetDPB();
    for (it = dpb.begin(); dpb.end() != it; ++it)
    {
        if (it->IsUsedForReference() && !it->IsLongTerm())
        {
            m_refList.push_back(it - dpb.begin());
        }
    }
    sort(m_refList.begin(), m_refList.end(), DescRefListPicNum(dpb));
    size_t stRefSize = m_refList.size();
    for (it = dpb.begin(); dpb.end() != it; ++it)
    {
        if (it->IsUsedForReference() && it->IsLongTerm())
        {
            m_refList.push_back(it - dpb.begin());
        }
    }
    sort(m_refList.begin() + stRefSize, m_refList.end(), AscenRefListLTPPicNum(dpb));
    // By definition the size of the reference list is equal to
    // num_ref_idx_l0_active_minus1 + 1, but at this point we cannot blindly
    // set the size just to num_ref_idx_l0_active_minus1 + 1, because its value
    // can be larger than the current size of the list. The list will grow in
    // this case later, during a reordering operation. If the size of the list
    // here is less than num_ref_idx_l0_active_minus1 + 1, the reordering
    // operation will insert some duplicates to the list.
    m_refList.resize(min(m_refList.size(), NumRefIdxActive(0)));
    if (RefPicListReorderingFlagL0())
    {
        ReorderRefPicList(&m_refList, 0);
    }
#if defined(PRINT_REFLIST)
    // for debugging outside MODS, there is no practical reason to define
    // PRINT_REFLIST in MODS
    if (!m_refList.empty())
    {
        printf("P slice ref list 0:\n");
        for (size_t i = 0; m_refList.size() > i; ++i)
        {
            int poc = GetPicOrderCnt(dpb[m_refList[i]].GetPicIdx());
            int picNum = dpb[m_refList[i]].GetPicNum();
            printf("   %2d -> POC: %4d PicNum: %4d\n", i, poc, picNum);
        }
    }
    else
    {
        printf("P slice ref list 0 is empty.\n");
    }
#endif
}

void BSlice::DoCreateRefLists()
{
    size_t prevListSize;
    m_refList0.clear();
    m_refList1.clear();

    DPB::const_iterator it;
    const DPB &dpb = GetDPB();
    for (it = dpb.begin(); dpb.end() != it; ++it)
    {
        if (it->IsUsedForReference() && !it->IsLongTerm())
        {
            if (GetPicOrderCnt() >= GetPicOrderCnt(it->GetPicIdx()))
            {
                m_refList0.push_back(it - dpb.begin());
            }
        }
    }
    sort(m_refList0.begin(), m_refList0.end(), DescRefListPOC(dpb, this));
    prevListSize = m_refList0.size();
    for (it = dpb.begin(); dpb.end() != it; ++it)
    {
        if (it->IsUsedForReference() && !it->IsLongTerm())
        {
            if (GetPicOrderCnt() < GetPicOrderCnt(it->GetPicIdx()))
            {
                m_refList0.push_back(it - dpb.begin());
            }
        }
    }
    sort(m_refList0.begin() + prevListSize,
         m_refList0.end(),
         AscenRefListPOC(dpb, this));
    m_refList1.assign(m_refList0.begin() + prevListSize, m_refList0.end());
    m_refList1.insert(
        m_refList1.end(),
        m_refList0.begin(),
        m_refList0.begin() + prevListSize);
    prevListSize = m_refList0.size();
    for (it = dpb.begin(); dpb.end() != it; ++it)
    {
        if (it->IsUsedForReference() && it->IsLongTerm())
        {
            m_refList0.push_back(it - dpb.begin());
        }
    }
    sort(m_refList0.begin() + prevListSize,
         m_refList0.end(),
         AscenRefListLTPPicNum(dpb));
    m_refList1.insert(
        m_refList1.end(),
        m_refList0.begin() + prevListSize,
        m_refList0.end());

    if (m_refList1.size() > 1 && m_refList0.size() == m_refList1.size())
    {
        if (m_refList0.end() ==
            mismatch(
                m_refList0.begin(),
                m_refList0.end(),
                m_refList1.begin()).first)
        {
            swap(m_refList1[0], m_refList1[1]);
        }
    }
    // By definition the size of the reference list is equal to
    // num_ref_idx_lX_active_minus1 + 1, but at this point we cannot blindly
    // set the size just to num_ref_idx_lX_active_minus1 + 1, because its value
    // can be larger than the current size of the list. The list will grow in
    // this case later, during a reordering operation. If the size of the list
    // here is less than num_ref_idx_lX_active_minus1 + 1, the reordering
    // operation will insert some duplicates to the list.
    m_refList0.resize(min(m_refList0.size(), NumRefIdxActive(0)));
    m_refList1.resize(min(m_refList1.size(), NumRefIdxActive(1)));

    if (RefPicListReorderingFlagL0())
    {
        ReorderRefPicList(&m_refList0, 0);
    }
    if (RefPicListReorderingFlagL1())
    {
        ReorderRefPicList(&m_refList1, 1);
    }
#if defined(PRINT_REFLIST)
    // for debugging outside MODS, there is no practical reason to define
    // PRINT_REFLIST in MODS
    if (!m_refList0.empty())
    {
        printf("B slice ref list 0:\n");
        for (size_t i = 0; m_refList0.size() > i; ++i)
        {
            int poc = GetPicOrderCnt(dpb[m_refList0[i]].GetPicIdx());
            int picNum = dpb[m_refList0[i]].GetPicNum();
            printf("   %2d -> POC: %4d PicNum: %4d\n", i, poc, picNum);
        }
    }
    else
    {
        printf("B slice ref list 0 is empty.\n");
    }
    if (!m_refList1.empty())
    {
        printf("B slice ref list 1:\n");
        for (size_t i = 0; m_refList1.size() > i; ++i)
        {
            int poc = GetPicOrderCnt(dpb[m_refList1[i]].GetPicIdx());
            int picNum = dpb[m_refList1[i]].GetPicNum();
            printf("   %2d -> POC: %4d PicNum: %4d\n", i, poc, picNum);
        }
    }
    else
    {
        printf("B slice ref list 1 is empty.\n");
    }
#endif
}

Picture::Picture(
    const SeqParamSetSrc *seqParamSrc,
    const PicParamSetSrc *picParamSrc,
    const DecodedPictureSrc *picSrc,
    size_t idx
)
    : m_isReference(false)
    , m_hasMMCO5(false)
    , m_isIDR(false)
    , m_MaxFrameNum(0)
    , m_MbaffFrameFlag(0)
    , pic_order_cnt_lsb(0)
    , frame_num(0)
    , max_num_ref_frames(0)
    , no_output_of_prior_pics_flag(false)
    , long_term_reference_flag(false)
    , adaptive_ref_pic_marking_mode_flag(false)
    , field_pic_flag(false)
    , bottom_field_flag(false)
    , m_seqParamSrc(seqParamSrc)
    , m_picParamSrc(picParamSrc)
    , m_picSrc(picSrc)
    , m_TopFieldOrderCnt(0)
    , m_BottomFieldOrderCnt(0)
    , m_PicOrderCnt(0)
    , m_PicOrderCntMsb(0)
    , m_FrameNumOffset(0)
    , m_maxDpbSize(0)
    , m_picsToOutSize(0)
    , m_removedPicsSize(0)
    , m_picIdx(idx)
{
    memset(&m_picsToOutput, 0, sizeof(m_picsToOutput));
    memset(m_removedPics, 0, sizeof(m_removedPics));
}

Picture::Picture(Slice *newSlice, const DecodedPictureSrc *picSrc, size_t idx)
    : m_isReference(false)
    , m_isIDR(false)
    , m_seqParamSrc(newSlice->GetSeqParamSrc())
    , m_picParamSrc(newSlice->GetPicParamSrc())
    , m_picSrc(picSrc)
    , m_picsToOutSize(0)
    , m_removedPicsSize(0)
    , m_picIdx(idx)
{
    m_sps = newSlice->GetSPS();
    m_pps = newSlice->GetPPS();
    m_pps.SetParamSrc(this);
    newSlice->SetSeqParamSrc(this);
    newSlice->SetPicParamSrc(this);
    newSlice->SetDPBSrc(this);
    newSlice->SetPicOrderCntSrc(this);
    m_slices.push_back(newSlice);
    InitFromFirstSlice();
}

void Picture::InitFromFirstSlice()
{
    const_iterator it = m_slices.cbegin();
    m_isReference = it->IsReference();
    m_hasMMCO5 = it->HasMMCO5();
    m_isIDR = it->IsIDR();
    pic_order_cnt_lsb = it->PicOderCntLsb();
    frame_num = it->FrameNum();
    no_output_of_prior_pics_flag = it->NoOutputOfPriorPicsFlag();
    long_term_reference_flag = it->LongTermReferenceFlag();
    adaptive_ref_pic_marking_mode_flag = it->AdaptiveRefPicMarkingModeFlag();
    if (adaptive_ref_pic_marking_mode_flag)
    {
        dec_ref_pic_marking_list = it->DecRefPicMarkingList();
    }
    m_MbaffFrameFlag = it->MbaffFrameFlag();
    field_pic_flag = it->FieldPicFlag();
    bottom_field_flag = it->BottomFieldFlag();

    m_maxDpbSize = m_sps.MaxDpbSize();
    m_MaxFrameNum = m_sps.MaxFrameNum();
    max_num_ref_frames = m_sps.max_num_ref_frames;

    switch (m_sps.pic_order_cnt_type)
    {
    case 0:
        CalcPOC0(*it);
        break;
    case 1:
        CalcPOC1(*it);
        break;
    case 2:
        CalcPOC2(*it);
        break;
    }
}

RC Picture::AddSliceFromStream(NALU &nalu, unique_ptr<Slice> *ppRejectedSlice)
{
    RC rc;

    MASSERT(nullptr != ppRejectedSlice);

    ppRejectedSlice->reset();

    SliceHeader sh(nalu, m_seqParamSrc, m_picParamSrc);
    unique_ptr<Slice> slice;
    switch (sh.SliceType())
    {
    case TYPE_I:
    case TYPE2_I:
        slice.reset(new ISlice(sh));
        break;
    case TYPE_P:
    case TYPE2_P:
        slice.reset(new PSlice(sh));
        break;
    case TYPE_B:
    case TYPE2_B:
        slice.reset(new BSlice(sh));
        break;
    default:
        return RC::ILWALID_FILE_FORMAT;
        break;
    }

    if (m_slices.empty())
    {
        // cache SPS and PPS, since SPS and PPS stored globally can be
        // overwritten by the subsequent NALUs
        m_sps = slice->GetSPS();
        m_pps = slice->GetPPS();
        m_pps.SetParamSrc(this);
        slice->SetSeqParamSrc(this);
        slice->SetPicParamSrc(this);
        m_slices.push_back(slice.release());
        InitFromFirstSlice();
    }
    else
    {
        if (m_slices.back().IsTheSamePic(*slice))
        {
            slice->SetSeqParamSrc(this);
            slice->SetPicParamSrc(this);
            slice->SetDPBSrc(this);
            slice->SetPicOrderCntSrc(this);
            m_slices.push_back(slice.release());
        }
        else
        {
            *ppRejectedSlice = move(slice);
        }
    }

    return rc;
}

void Picture::ResetDpb()
{
    m_dpb.clear();
}

void Picture::CopyDpbAndInsert(const Picture &prevPic, size_t prevPicIdx)
{
    const DPB &prevDpb = prevPic.GetDPB();

    size_t prevIndices[removedPixMaxSize];
    size_t prevIndicesSize = prevDpb.size();
    transform(
        prevDpb.begin(),
        prevDpb.end(),
        &prevIndices[0],
        mem_fun_ref(&DPBElement::GetPicIdx));
    sort(&prevIndices[0], &prevIndices[0] + prevIndicesSize);

    size_t newIndices[removedPixMaxSize];
    size_t newIndicesSize = 0;

    m_picsToOutSize = 0;
    m_removedPicsSize = 0;

    DPBElement de(prevPicIdx);
    de.SetUsedForReference(prevPic.IsReference());
    if (prevPic.IsIDR())
    {
        if (!prevPic.NoOutputOfPriorPicsFlag())
        {
            DPB::const_iterator dpbIt;
            for (dpbIt = prevDpb.begin(); prevDpb.end() != dpbIt; ++dpbIt)
            {
                if (!dpbIt->IsOutput())
                {
                    m_picsToOutput[m_picsToOutSize++] = dpbIt->GetPicIdx();
                }
            }
            sort(m_picsToOutput, m_picsToOutput + m_picsToOutSize, POCOrder(m_picSrc));
        }

        ResetDpb();
        if (de.IsUsedForReference())
        {
            de.SetLongTerm(prevPic.LongTermReferenceFlag());
            if (de.IsLongTerm())
            {
                de.SetLongTermFrameIdx(0);
            }
        }
    }
    else
    {
        m_dpb = prevPic.GetDPB();
        if (de.IsUsedForReference())
        {
            de.SetLongTerm(false);
        }
        if (prevPic.IsReference() && prevPic.AdaptiveRefPicMarkingModeFlag())
        {
            list<DecRefPicMarking>::const_iterator mmcoIt;
            const list<DecRefPicMarking> &mmco = prevPic.DecRefPicMarkingList();
            for (mmcoIt = mmco.begin(); mmco.end() != mmcoIt; ++mmcoIt)
            {
                switch (mmcoIt->memory_management_control_operation)
                {
                case 1:
                    UnmarkShortTermForRef(prevPic, *mmcoIt);
                    break;
                case 2:
                    UnmarkLongTermForRef(*mmcoIt);
                    break;
                case 3:
                    AssignLongTermFrameIdx(prevPic, *mmcoIt);
                    break;
                case 4:
                    UpdateMaxLongTermFrameIdx(*mmcoIt);
                    break;
                case 5:
                    UnmarkAllForRef();
                    break;
                case 6:
                    UnmarkLongTermForRefByIdx(mmcoIt->long_term_frame_idx);
                    de.SetLongTerm(true);
                    de.SetLongTermFrameIdx(mmcoIt->long_term_frame_idx);
                    break;
                }
            }
        }
    }

    if (!prevPic.IsIDR() &&
        (prevPic.IsReference() && !prevPic.AdaptiveRefPicMarkingModeFlag()))
    {
        // sliding window
        size_t numShortTerm = count_if(m_dpb.begin(), m_dpb.end(), IsShortTermPred<DPB>());
        size_t numLongTerm = count_if(m_dpb.begin(), m_dpb.end(), IsLongTermPred<DPB>());
        if (numLongTerm + numShortTerm == max(MaxNumRefFrames(), 1U))
        {
            DPB::iterator firstStIt;
            firstStIt = find_if(m_dpb.begin(), m_dpb.end(), IsShortTermPred<DPB>());
            if (m_dpb.end() != firstStIt)
            {
                firstStIt->SetUsedForReference(false);
            }
        }
    }

    // remove unused frames (were output and are not used for reference)
    if (m_dpb.size() == m_maxDpbSize)
    {
        DPB::iterator newEnd;
        newEnd = remove_if(m_dpb.begin(), m_dpb.end(), UnusedPred<DPB>());
        m_dpb.erase(newEnd, m_dpb.end());
    }

    // then output frames until we can remove a picture from DPB
    while (m_dpb.size() == m_maxDpbSize)
    {
        DPB::iterator minIt;
        DPB::iterator it =
            find_if(m_dpb.begin(), m_dpb.end(), not1(IsOutputPred<DPB>()));
        minIt = it;
        // find a picture with the minimum POC that wasn't output yet
        if (m_dpb.end() != it)
        {
            for (; m_dpb.end() != ++it;)
            {
                const Picture *pic1 = m_picSrc->GetPicture(it->GetPicIdx());
                const Picture *pic2 = m_picSrc->GetPicture(minIt->GetPicIdx());
                if (!it->IsOutput() && (pic1->PicOrderCnt() < pic2->PicOrderCnt()))
                {
                    minIt = it;
                }
            }
        }
        m_picsToOutput[m_picsToOutSize++] = minIt->GetPicIdx();
        minIt->SetIsOutput(true);

        if (!minIt->IsUsedForReference())
        {
            m_dpb.erase(minIt);
        }
    }

    m_dpb.push_back(de);

    DPB::iterator dpbIt;
    for (dpbIt = m_dpb.begin(); m_dpb.end() != dpbIt; ++dpbIt)
    {
        if (dpbIt->IsUsedForReference() && !dpbIt->IsLongTerm())
        {
            const Picture *pic = dpbIt->GetPicture(m_picSrc);
            if (pic->FrameNum() > FrameNum())
            {
                dpbIt->SetPicNum(pic->FrameNum() - MaxFrameNum());
            }
            else
            {
                dpbIt->SetPicNum(pic->FrameNum());
            }
        }
        else if (dpbIt->IsUsedForReference() && dpbIt->IsLongTerm())
        {
            dpbIt->SetLongTermPicNum(dpbIt->GetLongTermFrameIdx());
        }
    }
    newIndicesSize = m_dpb.size();
    transform(
        m_dpb.begin(),
        m_dpb.end(),
        &newIndices[0],
        mem_fun_ref(&DPBElement::GetPicIdx));
    sort(&newIndices[0], &newIndices[0] + newIndicesSize);
    m_removedPicsSize = set_difference(
        &prevIndices[0],
        &prevIndices[0] + prevIndicesSize,
        &newIndices[0],
        &newIndices[0] + newIndicesSize,
        &m_removedPics[0]) - &m_removedPics[0];
#if defined(PRINT_DPB)
    // for debugging outside MODS, there is no practical reason to define
    // PRINT_DPB in MODS
    for (dpbIt = m_dpb.begin(); m_dpb.end() != dpbIt; ++dpbIt)
    {
        printf("(");
        printf("fn=%5d  ", GetDpbPic(*dpbIt)->FrameNum());
        printf("poc=%5d)  ", GetDpbPic(*dpbIt)->PicOrderCnt());
        if (dpbIt->IsUsedForReference())
        {
            if (!dpbIt->IsLongTerm()) printf ("ref        ");
            else                      printf ("lt_ref     ");
        }
        else if (dpbIt->IsOutput())
        {
            printf ("           ");
        }
        if (dpbIt->IsOutput()) printf ("out  ");
        printf ("\n");
    }
#endif
}

void Picture::CalcPOC0(const Slice &slice)
{
    unsigned int MaxPicOrderCntLsb = m_sps.MaxPicOrderCntLsb();

    int prevPicOrderCntMsb;
    unsigned int prevPicOrderCntLsb;
    if (IsIDR())
    {
        prevPicOrderCntMsb = 0;
        prevPicOrderCntLsb = 0;
    }
    else
    {
        const Picture *lastRefPic = m_picSrc->GetLastRefPicture();
        if (lastRefPic->HasMMCO5())
        {
            prevPicOrderCntMsb = 0;
            prevPicOrderCntLsb = lastRefPic->m_TopFieldOrderCnt;
        }
        else
        {
            prevPicOrderCntMsb = lastRefPic->m_PicOrderCntMsb;
            prevPicOrderCntLsb = lastRefPic->PicOderCntLsb();
        }
    }
    if ((slice.PicOderCntLsb() < prevPicOrderCntLsb) &&
        ((prevPicOrderCntLsb - slice.PicOderCntLsb()) >=
         (MaxPicOrderCntLsb / 2)))
    {
        m_PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    }
    else if ((slice.PicOderCntLsb() > prevPicOrderCntLsb) &&
             ((slice.PicOderCntLsb() - prevPicOrderCntLsb) >
              (MaxPicOrderCntLsb / 2)))
    {
        m_PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
    }
    else
    {
        m_PicOrderCntMsb = prevPicOrderCntMsb;
    }

    m_TopFieldOrderCnt = m_PicOrderCntMsb + slice.PicOderCntLsb();
    m_BottomFieldOrderCnt = m_TopFieldOrderCnt + slice.DeltaPicOderCntBottom();
    m_PicOrderCnt = min(m_TopFieldOrderCnt, m_BottomFieldOrderCnt);
}

void Picture::CalcPOC1(const Slice &slice)
{
    if (IsIDR())
    {
        m_FrameNumOffset = 0;
    }
    else
    {
        const Picture *lastPic = m_picSrc->GetLastPicture();
        unsigned int prevFrameNum = lastPic->FrameNum();
        unsigned int prevFrameNumOffset;

        if (lastPic->HasMMCO5())
        {
            prevFrameNumOffset = 0;
        }
        else
        {
            prevFrameNumOffset = lastPic->m_FrameNumOffset;
        }

        if (prevFrameNum > slice.FrameNum())
        {
            m_FrameNumOffset = prevFrameNumOffset + m_sps.MaxFrameNum();
        }
        else
        {
            m_FrameNumOffset = prevFrameNumOffset;
        }
    }

    unsigned int absFrameNum;
    if (0 != m_sps.num_ref_frames_in_pic_order_cnt_cycle)
    {
        absFrameNum = m_FrameNumOffset + slice.FrameNum();
    }
    else
    {
        absFrameNum = 0;
    }

    if (!IsReference() && 0 < absFrameNum)
    {
        --absFrameNum;
    }

    int expectedPicOrderCnt;
    if (0 < absFrameNum)
    {
        int picOrderCntCycleCnt = (absFrameNum - 1) / m_sps.num_ref_frames_in_pic_order_cnt_cycle;
        int frameNumInPicOrderCntCycle = (absFrameNum - 1) % m_sps.num_ref_frames_in_pic_order_cnt_cycle;
        expectedPicOrderCnt = picOrderCntCycleCnt * m_sps.ExpectedDeltaPerPicOrderCntCycle;
        for (int i = 0; i <= frameNumInPicOrderCntCycle; ++i)
        {
            expectedPicOrderCnt = expectedPicOrderCnt + m_sps.offset_for_ref_frame[i];
        }
    }
    else
    {
        expectedPicOrderCnt = 0;
    }

    if (!IsReference())
    {
        expectedPicOrderCnt = expectedPicOrderCnt + m_sps.offset_for_non_ref_pic;
    }

    m_TopFieldOrderCnt = expectedPicOrderCnt + slice.DeltaPicOderCnt(0);
    m_BottomFieldOrderCnt = m_TopFieldOrderCnt +
                            m_sps.offset_for_top_to_bottom_field +
                            slice.DeltaPicOderCnt(1);
    m_PicOrderCnt = min(m_TopFieldOrderCnt, m_BottomFieldOrderCnt);
}

void Picture::CalcPOC2(const Slice &slice)
{
    if (IsIDR())
    {
        m_FrameNumOffset = 0;
        m_PicOrderCnt = m_TopFieldOrderCnt = m_BottomFieldOrderCnt = 0;
    }
    else
    {
        const Picture *lastPic = m_picSrc->GetLastPicture();
        unsigned int prevFrameNum = lastPic->FrameNum();
        unsigned int prevFrameNumOffset;

        if (lastPic->HasMMCO5())
        {
            prevFrameNumOffset = 0;
        }
        else
        {
            prevFrameNumOffset = lastPic->m_FrameNumOffset;
        }

        if (prevFrameNum > slice.FrameNum())
        {
            unsigned int MaxFrameNum = 1 << (m_sps.log2_max_frame_num_minus4 + 4);
            m_FrameNumOffset = prevFrameNumOffset + MaxFrameNum;
        }
        else
        {
            m_FrameNumOffset = prevFrameNumOffset;
        }

        unsigned int absFrameNum = m_FrameNumOffset + slice.FrameNum();
        if (!IsReference())
        {
            m_PicOrderCnt = m_TopFieldOrderCnt = m_BottomFieldOrderCnt = 2 * absFrameNum - 1;
        }
        else
        {
            m_PicOrderCnt = m_TopFieldOrderCnt = m_BottomFieldOrderCnt = 2 * absFrameNum;
        }
    }
}

void Picture::UnmarkShortTermForRef(const Picture &pic, const DecRefPicMarking &mmco)
{
    int picNum = pic.FrameNum() - (mmco.difference_of_pic_nums_minus1 + 1);
    PicNumPred<DPB> pred(m_dpb, picNum);
    DPB::iterator picIt = find_if(m_dpb.begin(), m_dpb.end(), pred);
    if (m_dpb.end() != picIt)
    {
        picIt->SetUsedForReference(false);
    }
}

void Picture::UnmarkLongTermForRef(const DecRefPicMarking &mmco)
{
    LongTermPicNumPred<DPB> pred(m_dpb, mmco.long_term_pic_num);
    DPB::iterator picIt = find_if(m_dpb.begin(), m_dpb.end(), pred);
    if (m_dpb.end() != picIt)
    {
        picIt->SetUsedForReference(false);
    }
}

void Picture::UnmarkLongTermForRefByIdx(int long_term_frame_idx)
{
    LongTermFrameIdxPred<DPB> ltpred(long_term_frame_idx);
    DPB::iterator picIt = find_if(m_dpb.begin(), m_dpb.end(), ltpred);
    if (m_dpb.end() != picIt)
    {
        picIt->SetUsedForReference(false);
    }
}

void Picture::AssignLongTermFrameIdx(const Picture &pic, const DecRefPicMarking &mmco)
{
    int picNum = pic.FrameNum() - (mmco.difference_of_pic_nums_minus1 + 1);
    UnmarkLongTermForRefByIdx(mmco.long_term_frame_idx);
    PicNumPred<DPB> pred(m_dpb, picNum);
    DPB::iterator picIt = find_if(m_dpb.begin(), m_dpb.end(), pred);
    if (m_dpb.end() != picIt)
    {
        picIt->SetLongTerm(true);
        picIt->SetLongTermFrameIdx(mmco.long_term_frame_idx);
        picIt->SetLongTermPicNum(mmco.long_term_frame_idx);
    }
}

void Picture::UpdateMaxLongTermFrameIdx(const DecRefPicMarking &mmco)
{
    int maxLongTermPicIdx = mmco.max_long_term_frame_idx_plus1 + 1;

    DPB::iterator dpbIt;
    for (dpbIt = m_dpb.begin(); m_dpb.end() != dpbIt; ++dpbIt)
    {
        if (dpbIt->IsUsedForReference() && dpbIt->IsLongTerm() &&
            dpbIt->GetLongTermFrameIdx() > maxLongTermPicIdx)
        {
            dpbIt->SetUsedForReference(false);
        }
    }
}

void Picture::UnmarkAllForRef()
{
    DPB::iterator dpbIt;
    for (dpbIt = m_dpb.begin(); m_dpb.end() != dpbIt; ++dpbIt)
    {
        dpbIt->SetUsedForReference(false);
    }
}

struct DpbSizeLess : public binary_function<Picture, Picture, bool>
{
    bool operator ()(const Picture &p1, const Picture &p2) const
    {
        return p1.DpbSize() < p2.DpbSize();
    }
};

size_t Parser::GetMaxDpbSize() const
{
    return std::max_element(
        m_pictures.begin(),
        m_pictures.end(),
        DpbSizeLess()
    )->DpbSize();
}

void AssignConst(unsigned char (*to)[4][4], unsigned char c)
{
    for (size_t i = 0; 4 > i; ++i)
        for (size_t j = 0; 4 > j; ++j)
            (*to)[i][j] = c;
}

void AssignConst(unsigned char (*to)[8][8], unsigned char c)
{
    for (size_t i = 0; 8 > i; ++i)
        for (size_t j = 0; 8 > j; ++j)
            (*to)[i][j] = c;
}

void Assign2D(unsigned char (*to)[4][4], const unsigned char (&from)[4][4])
{
    for (size_t i = 0; 4 > i; ++i)
        for (size_t j = 0; 4 > j; ++j)
            (*to)[i][j] = from[i][j];
}

void Assign1D(unsigned char (*to)[4][4], const unsigned char (&from)[16])
{
    for (size_t i = 0; 16 > i; ++i)
        (*to)[i / 4][i % 4] = from[i];
}

void Assign1D(unsigned char (*to)[8][8], const unsigned char (&from)[64])
{
    for (size_t i = 0; 64 > i; ++i)
        (*to)[i / 8][i % 8] = from[i];
}

void CreateWeightMatrices(const SeqParameterSet& sps, const PicParameterSet& pps,
                          unsigned char(*w4x4)[6][4][4], unsigned char(*w8x8)[2][8][8])
{
    // Filling in the scaling prediction matrices.
    // See Table 7-2 and the description of seq_scaling_list_present_flag
    // (section 7.4.2.1.1) and pic_scaling_list_present_flag (section 7.4.2.2)
    // in Rec. ITU-T H.264.
    if (!pps.pic_scaling_matrix_present_flag && !sps.seq_scaling_matrix_present_flag)
    {
        for (size_t i = 0; i < 6; ++i)
        {
            AssignConst(&(*w4x4)[i], 16);
        }
        for (size_t i = 0; i < 2; ++i)
        {
            AssignConst(&(*w8x8)[i], 16);
        }
    }
    else
    {
        if (sps.seq_scaling_matrix_present_flag)
        {
            for (size_t i = 0; i < 6; ++i)
            {
                if (!sps.seq_scaling_list_present_flag[i])
                {
                    // fall-back rule A
                    if (0 == i)
                    {
                        Assign1D(&(*w4x4)[i], H264::Default_4x4_Intra);
                    }
                    else if (3 == i)
                    {
                        Assign1D(&(*w4x4)[i], H264::Default_4x4_Inter);
                    }
                    else
                    {
                        Assign2D(&(*w4x4)[i], (*w4x4)[i - 1]);
                    }
                }
                else
                {
                    if (sps.useDefaultScalingMatrix4x4Flag[i])
                    {
                        if (i < 3)
                        {
                            Assign1D(&(*w4x4)[i], H264::Default_4x4_Intra);
                        }
                        else
                        {
                            Assign1D(&(*w4x4)[i], H264::Default_4x4_Inter);
                        }
                    }
                    else
                    {
                        Assign1D(&(*w4x4)[i], sps.scalingList4x4[i]);
                    }
                }
            }

            if (!sps.seq_scaling_list_present_flag[6] || sps.useDefaultScalingMatrix8x8Flag[0])
            {
                // here is a union of the fallback rule A and a case when
                // useDefaultScalingMatrix8x8Flag is true
                Assign1D(&(*w8x8)[0], H264::Default_8x8_Intra);
            }
            else // scaling list present and not to use default
            {
                Assign1D(&(*w8x8)[0], sps.scalingList8x8[0]);
            }

            if (!sps.seq_scaling_list_present_flag[7] || sps.useDefaultScalingMatrix8x8Flag[1])
            {
                // here is a union of the fallback rule A and a case when
                // useDefaultScalingMatrix8x8Flag is true
                Assign1D(&(*w8x8)[1], H264::Default_8x8_Inter);
            }
            else // scaling list present and not to use default
            {
                Assign1D(&(*w8x8)[1], sps.scalingList8x8[1]);
            }
        }

        if (pps.pic_scaling_matrix_present_flag)
        {
            for (size_t i = 0; i < 6; ++i)
            {
                if (!pps.pic_scaling_list_present_flag[i])
                {
                    // fall-back rule B
                    if (0 == i && !sps.seq_scaling_matrix_present_flag)
                    {
                        Assign1D(&(*w4x4)[i], H264::Default_4x4_Intra);
                    }
                    else if (3 == i && !sps.seq_scaling_matrix_present_flag)
                    {
                        Assign1D(&(*w4x4)[i], H264::Default_4x4_Inter);
                    }
                    else
                    {
                        Assign2D(&(*w4x4)[i], (*w4x4)[i - 1]);
                    }
                }
                else
                {
                    if (0 != pps.useDefaultScalingMatrix4x4Flag[i])
                    {
                        if (i < 3)
                        {
                            Assign1D(&(*w4x4)[i], H264::Default_4x4_Intra);
                        }
                        else
                        {
                            Assign1D(&(*w4x4)[i], H264::Default_4x4_Inter);
                        }
                    }
                    else
                    {
                        Assign1D(&(*w4x4)[i], pps.scalingList4x4[i]);
                    }
                }
            }

            if ((!sps.seq_scaling_list_present_flag[6] && !pps.pic_scaling_list_present_flag[6]) ||
                (pps.pic_scaling_list_present_flag[6] && pps.useDefaultScalingMatrix8x8Flag[0]))
            {
                Assign1D(&(*w8x8)[0], H264::Default_8x8_Intra);
            }
            else if (pps.pic_scaling_list_present_flag[6] && !pps.useDefaultScalingMatrix8x8Flag[0])
            {
                Assign1D(&(*w8x8)[0], sps.scalingList8x8[0]);
            }

            if ((!sps.seq_scaling_list_present_flag[7] && !pps.pic_scaling_list_present_flag[7]) ||
                (pps.pic_scaling_list_present_flag[7] && pps.useDefaultScalingMatrix8x8Flag[1]))
            {
                Assign1D(&(*w8x8)[1], H264::Default_8x8_Inter);
            }
            else if (pps.pic_scaling_list_present_flag[7] && !pps.useDefaultScalingMatrix8x8Flag[1])
            {
                Assign1D(&(*w8x8)[1], sps.scalingList8x8[1]);
            }
        }
    }
}

} // namespace H264
