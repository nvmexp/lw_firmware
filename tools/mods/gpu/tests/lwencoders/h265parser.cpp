/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014,2019-2020 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>

#include "h26xsyntax.h"
#include "h265syntax.h"

#include "h265parser.h"

namespace H265
{

Picture::Picture(
    const VidParamSetSrc    *vidParamSrc,
    const SeqParamSetSrc    *seqParamSrc,
    const PicParamSetSrc    *picParamSrc,
          DecodedPictureSrc *picSrc,
          PicIdx             idx
)
  : TemporalId(0)
  , NumPocTotalLwrr(0)
  , NoRaslOutputFlag(false)
  , MaxPicOrderCntLsb(0)
  , PicOrderCntVal(0)
  , sw_hdr_skip_length(0)
  , num_bits_short_term_ref_pics_in_slice(0)
  , m_refPicListTemp0Size(0)
  , m_refPicListTemp1Size(0)
  , m_vidParamSrc(vidParamSrc)
  , m_seqParamSrc(seqParamSrc)
  , m_picParamSrc(picParamSrc)
  , m_picSrc(picSrc)
  , m_picIdx(idx)
{}

Picture::Picture(Slice *newSlice, DecodedPictureSrc *picSrc, PicIdx idx)
  : m_refPicListTemp0Size(0)
  , m_refPicListTemp1Size(0)
  , m_vidParamSrc(newSlice->GetVidParamSrc())
  , m_seqParamSrc(newSlice->GetSeqParamSrc())
  , m_picParamSrc(newSlice->GetPicParamSrc())
  , m_picSrc(picSrc)
  , m_picIdx(idx)
{
    // the first segment must be independent
    MASSERT(!newSlice->Header().dependent_slice_segment_flag);

    m_vps = newSlice->GetVPS();
    m_sps = newSlice->GetSPS();
    m_pps = newSlice->GetPPS();
    m_sps.SetVidParamSrc(this);
    m_pps.SetSeqParamSrc(this);

    // Switch the new slice to use the locally stored VPS, SPS and PPS. This is
    // necessary, since in the global storage VPS, SPS and PPS can be
    // overwritten by the consequent NALUs.
    newSlice->SetVidParamSrc(this);
    newSlice->SetSeqParamSrc(this);
    newSlice->SetPicParamSrc(this);

    m_slices.push_back(newSlice);

    InitFromFirstSlice();

    Slice &sl = m_slices.front();
    sl.ModifyRefLists(
        GetRefPicListTemp0(), GetRefPicListTemp0Size(),
        GetRefPicListTemp1(), GetRefPicListTemp1Size()
    );
}

RC Picture::AddSliceFromStream(NALU &nalu, unique_ptr<Slice> *ppRejectedSlice)
{
    RC rc;

    MASSERT(nullptr != ppRejectedSlice);
    ppRejectedSlice->reset();

    SliceSegmentHeader sh(nalu, m_vidParamSrc, m_seqParamSrc, m_picParamSrc);
    if (sh.dependent_slice_segment_flag)
    {
        MASSERT(!m_slices.empty());
        m_slices.back().AddSegment(sh);

        return rc;
    }

    unique_ptr<Slice> slice;
    switch (sh.slice_type)
    {
    case SLICE_TYPE_I:
        slice.reset(new ISlice(sh));
        break;
    case SLICE_TYPE_P:
        slice.reset(new PSlice(sh));
        break;
    case SLICE_TYPE_B:
        slice.reset(new BSlice(sh));
        break;
    default:
        return RC::ILWALID_FILE_FORMAT;
        break;
    }

    if (m_slices.empty())
    {
        m_vps = slice->GetVPS();
        m_sps = slice->GetSPS();
        m_pps = slice->GetPPS();
        m_sps.SetVidParamSrc(this);
        m_pps.SetSeqParamSrc(this);
        slice->SetVidParamSrc(this);
        slice->SetSeqParamSrc(this);
        slice->SetPicParamSrc(this);
        m_slices.push_back(slice.release());

        InitFromFirstSlice();

        Slice &sl = m_slices.front();
        sl.ModifyRefLists(
            GetRefPicListTemp0(), GetRefPicListTemp0Size(),
            GetRefPicListTemp1(), GetRefPicListTemp1Size()
        );
    }
    else
    {
        if (!slice->Header().first_slice_segment_in_pic_flag)
        {
            slice->SetVidParamSrc(this);
            slice->SetSeqParamSrc(this);
            slice->SetPicParamSrc(this);
            slice->ModifyRefLists(
                GetRefPicListTemp0(), GetRefPicListTemp0Size(),
                GetRefPicListTemp1(), GetRefPicListTemp1Size()
            );
            m_slices.push_back(slice.release());
        }
        else
        {
            *ppRejectedSlice = move(slice);
        }
    }

    return rc;
}

void Picture::InitFromFirstSlice()
{
    const SliceSegmentHeader &sh = m_slices.front().Header();
    nal_unit_type = sh.nal_unit_type;
    TemporalId = sh.TemporalId;
    NumPocTotalLwrr = sh.NumPocTotalLwrr;
    sw_hdr_skip_length = sh.sw_hdr_skip_length;
    num_bits_short_term_ref_pics_in_slice = sh.short_term_ref_pic_set_idx.GetNumBits();

    if (IsIDR() || IsBLA() || m_picSrc->IsSequenceBegin())
    {
        NoRaslOutputFlag = true;
    }
    else
    {
        NoRaslOutputFlag = false;
    }

    CallwlatePOC();
    ConstructRPS();

    if (0 < NumPocTotalLwrr)
    {
        // Since num_ref_idx_l0_active_minus1 and num_ref_idx_l1_active_minus1
        // can be different per slice, per picture we callwlate the largest size
        // possible 16. Every slice will truncate these lists according to
        // correspondent NumPocTotalLwrr and num_ref_idx_l0_active_minus1 and
        // num_ref_idx_l1_active_minus1. Note that NumPocTotalLwrr is the same
        // for all slices.
        m_refPicListTemp0Size = static_cast<UINT32>(NUMELEMS(RefPicListTemp0));
        m_refPicListTemp1Size = static_cast<UINT32>(NUMELEMS(RefPicListTemp1));

        UINT32 rIdx = 0;
        while (rIdx < m_refPicListTemp0Size)
        {
            for (UINT08 i = 0; m_rps.NumPocStLwrrBefore > i && m_refPicListTemp0Size > rIdx; ++rIdx, ++i)
                RefPicListTemp0[rIdx] = m_rps.RefPicSetStLwrrBefore[i];
            for (UINT08 i = 0; m_rps.NumPocStLwrrAfter > i && m_refPicListTemp0Size > rIdx; ++rIdx, ++i)
                RefPicListTemp0[rIdx] = m_rps.RefPicSetStLwrrAfter[i];
            for (UINT08 i = 0; m_rps.NumPocLtLwrr > i && m_refPicListTemp0Size > rIdx; ++rIdx, ++i)
                RefPicListTemp0[rIdx] = m_rps.RefPicSetLtLwrr[i];
        }

        rIdx = 0;
        while (rIdx < m_refPicListTemp1Size)
        {
            for (UINT08 i = 0; m_rps.NumPocStLwrrAfter > i && m_refPicListTemp1Size > rIdx; ++rIdx, ++i)
                RefPicListTemp1[rIdx] = m_rps.RefPicSetStLwrrAfter[i];
            for (UINT08 i = 0; m_rps.NumPocStLwrrBefore > i && m_refPicListTemp1Size > rIdx; ++rIdx, ++i)
                RefPicListTemp1[rIdx] = m_rps.RefPicSetStLwrrBefore[i];
            for (UINT08 i = 0; m_rps.NumPocLtLwrr > i && m_refPicListTemp1Size > rIdx; ++rIdx, ++i)
                RefPicListTemp1[rIdx] = m_rps.RefPicSetLtLwrr[i];
        }
    }
    else
    {
        m_refPicListTemp0Size = 0;
        m_refPicListTemp1Size = 0;
    }
}

const ScalingListData* Picture::GetScalingListData() const
{
    if (m_sps.scaling_list_enabled_flag)
    {
        if (m_pps.pps_scaling_list_data_present_flag)
        {
            return &m_pps.scaling_list_data;
        }
        else if (m_sps.sps_scaling_list_data_present_flag)
        {
            return &m_sps.scaling_list_data;
        }
        else
        {
            return &ScalingListData::GetDefaultScalingListData();
        }
    }
    else
    {
        return nullptr;
    }
}

void Picture::CallwlatePOC()
{
    const SliceSegmentHeader &sh = m_slices.front().Header();

    INT32 PicOrderCntMsb;
    INT32 slice_pic_order_cnt_lsb = static_cast<INT32>(sh.slice_pic_order_cnt_lsb);
    INT32 prevPicOrderCntLsb = 0;
    INT32 prevPicOrderCntMsb = 0;

    MaxPicOrderCntLsb = static_cast<INT32>(GetSPS().MaxPicOrderCntLsb);

    if (IsIRAP() && NoRaslOutputFlag)
    {
        // This will restart POC from 0 for IDR and BLA pictures.
        PicOrderCntMsb = 0;
    }
    else
    {
        prevPicOrderCntLsb = m_picSrc->GetPrevPicOrderCntLsb();
        prevPicOrderCntMsb = m_picSrc->GetPrevPicOrderCntMsb();

        // (8-1) of ITU-T H.265
        if (
            (slice_pic_order_cnt_lsb < prevPicOrderCntLsb) &&
            ((prevPicOrderCntLsb - slice_pic_order_cnt_lsb) >= (MaxPicOrderCntLsb / 2))
        )
        {
            PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
        }
        else if (
            (slice_pic_order_cnt_lsb > prevPicOrderCntLsb) &&
            ((slice_pic_order_cnt_lsb - prevPicOrderCntLsb) > (MaxPicOrderCntLsb / 2))
        )
        {
            PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
        }
        else
        {
            PicOrderCntMsb = prevPicOrderCntMsb;
        }
    }

    PicOrderCntVal = PicOrderCntMsb + slice_pic_order_cnt_lsb;

    if (IsTid0Pic())
    {
        m_picSrc->SetPrevPicOrderCntMsb(PicOrderCntMsb);
        m_picSrc->SetPrevPicOrderCntLsb(slice_pic_order_cnt_lsb);
    }
}

void Picture::ConstructRPS()
{
    const SliceSegmentHeader &sh = m_slices.front().Header();

    m_rps.NumPocStLwrrBefore = 0;
    m_rps.NumPocStLwrrAfter = 0;
    m_rps.NumPocStFoll = 0;
    m_rps.NumPocLtLwrr = 0;
    m_rps.NumPocLtFoll = 0;

    if (!IsIDR())
    {
        const ShortTermRefPicSet *lwrrPRS = sh.GetLwrrRps();
        for (size_t i = 0; lwrrPRS->NumNegativePics > i; ++i)
        {
            if (lwrrPRS->UsedByLwrrPicS0[i])
            {
                MASSERT(m_rps.NumPocStLwrrBefore < NUMELEMS(m_rps.PocStLwrrBefore));
                m_rps.PocStLwrrBefore[m_rps.NumPocStLwrrBefore++] = PicOrderCntVal + lwrrPRS->DeltaPocS0[i];
            }
            else
            {
                MASSERT(m_rps.NumPocStFoll < NUMELEMS(m_rps.PocStFoll));
                m_rps.PocStFoll[m_rps.NumPocStFoll++] = (PicOrderCntVal + lwrrPRS->DeltaPocS0[i]);
            }
        }
        for (size_t i = 0; lwrrPRS->NumPositivePics > i; ++i)
        {
            if (lwrrPRS->UsedByLwrrPicS1[i])
            {
                MASSERT(m_rps.NumPocStLwrrAfter < NUMELEMS(m_rps.PocStLwrrAfter));
                m_rps.PocStLwrrAfter[m_rps.NumPocStLwrrAfter++] = PicOrderCntVal + lwrrPRS->DeltaPocS1[i];
            }
            else
            {
                MASSERT(m_rps.NumPocStFoll < NUMELEMS(m_rps.PocStFoll));
                m_rps.PocStFoll[m_rps.NumPocStFoll++] = PicOrderCntVal + lwrrPRS->DeltaPocS1[i];
            }
        }
        size_t totLongTermRefPics = sh.num_long_term_sps + sh.num_long_term_pics;
        for (size_t i = 0; totLongTermRefPics > i; ++i)
        {
            UINT32 pocLt = sh.PocLsbLt[i];
            if (sh.delta_poc_msb_present_flag[i])
            {
                pocLt += PicOrderCntVal - sh.DeltaPocMsbCycleLt[i] * MaxPicOrderCntLsb -
                         sh.slice_pic_order_cnt_lsb;
            }
            if (sh.UsedByLwrrPicLt[i])
            {
                MASSERT(m_rps.NumPocLtLwrr < NUMELEMS(m_rps.PocLtLwrr));
                m_rps.PocLtLwrr[m_rps.NumPocLtLwrr] = pocLt;
                m_rps.LwrrDeltaPocMsbPresentFlag[m_rps.NumPocLtLwrr++] = sh.delta_poc_msb_present_flag[i];
            }
            else
            {
                MASSERT(m_rps.NumPocLtFoll < NUMELEMS(m_rps.PocLtFoll));
                m_rps.PocLtFoll[m_rps.NumPocLtFoll] = pocLt;
                m_rps.FollDeltaPocMsbPresentFlag[m_rps.NumPocLtFoll++] = sh.delta_poc_msb_present_flag[i];
            }
        }

        for (UINT08 i = 0; m_rps.NumPocLtLwrr > i; ++i)
        {
            if (!m_rps.LwrrDeltaPocMsbPresentFlag[i])
            {
                m_rps.RefPicSetLtLwrr[i] = m_picSrc->GetIdxFromLsb(m_rps.PocLtLwrr[i]);
            }
            else
            {
                m_rps.RefPicSetLtLwrr[i] = m_picSrc->GetIdxFromPoc(m_rps.PocLtLwrr[i]);
            }
        }
        for (UINT08 i = 0; m_rps.NumPocLtFoll > i; ++i)
        {
            if (!m_rps.FollDeltaPocMsbPresentFlag[i])
            {
                m_rps.RefPicSetLtFoll[i] = m_picSrc->GetIdxFromLsb(m_rps.PocLtFoll[i]);
            }
            else
            {
                m_rps.RefPicSetLtFoll[i] = m_picSrc->GetIdxFromPoc(m_rps.PocLtFoll[i]);
            }
        }
        for (UINT08 i = 0; m_rps.NumPocStLwrrBefore > i; ++i)
        {
            m_rps.RefPicSetStLwrrBefore[i] = m_picSrc->GetIdxFromPoc(m_rps.PocStLwrrBefore[i]);
        }
        for (UINT08 i = 0; m_rps.NumPocStLwrrAfter > i; ++i)
        {
            m_rps.RefPicSetStLwrrAfter[i] = m_picSrc->GetIdxFromPoc(m_rps.PocStLwrrAfter[i]);
        }
        for (UINT08 i = 0; m_rps.NumPocStFoll > i; ++i)
        {
            m_rps.RefPicSetStFoll[i] = m_picSrc->GetIdxFromPoc(m_rps.PocStFoll[i]);
        }
    }

    if (IsTid0Pic())
    {
        m_picSrc->ClearSetOfPrevPocVals();
        m_picSrc->AddRPSToSetOfPrevPocVals(m_rps);
    }
    m_picSrc->AddIdxToSetOfPrevPocVals(m_picIdx);
}

class PocEqualPred
{
public:
    PocEqualPred(INT32 PicOrderCntVal)
      : m_PicOrderCntVal(PicOrderCntVal)
    {}

    bool operator()(const Picture &pic)
    {
        return pic.PicOrderCntVal == m_PicOrderCntVal;
    }

private:
    INT32 m_PicOrderCntVal;
};

struct DpbSizeLess : public binary_function<Picture, Picture, bool>
{
    bool operator ()(const Picture &p1, const Picture &p2) const
    {
        return p1.GetMaxDecPicBuffering() < p2.GetMaxDecPicBuffering();
    }
};

UINT32 Parser::GetMaxDecPicBuffering() const
{
    return max_element(
        m_pictures.begin(),
        m_pictures.end(),
        DpbSizeLess()
    )->GetMaxDecPicBuffering();
}

PicIdx Parser::DoGetIdxFromPoc(INT32 poc)
{
    reverse_iterator it;
    it = find_if(m_pictures.rbegin(), m_pictures.rend(), PocEqualPred(poc));
    MASSERT(m_pictures.rend() != it);

    return it.base() - m_pictures.begin() - 1;
}

PicIdx Parser::DoGetIdxFromLsb(INT32 lsb)
{
    PicIdx picIdx = -1;

    set<PicIdx>::const_iterator picIt;
    for (picIt = setOfPrevPic.begin(); setOfPrevPic.end() != picIt; ++picIt)
    {
        const Picture &pic = m_pictures[*picIt];
        INT32 picLsb = pic.PicOrderCntVal % pic.MaxPicOrderCntLsb;
        if (0 > picLsb)
        {
            picLsb += pic.MaxPicOrderCntLsb;
        }

        if (lsb == picLsb)
        {
            picIdx = *picIt;
            break;
        }
    }

    MASSERT(-1 != picIdx);

    return picIdx;
}

void Parser::DoClearSetOfPrevPocVals()
{
    setOfPrevPic.clear();
}

void Parser::DoAddRPSToSetOfPrevPocVals(const RefPicSet &rps)
{
    for (UINT08 i = 0; rps.NumPocLtLwrr > i; ++i)
        setOfPrevPic.insert(rps.RefPicSetLtLwrr[i]);
    for (UINT08 i = 0; rps.NumPocLtFoll > i; ++i)
        setOfPrevPic.insert(rps.RefPicSetLtFoll[i]);
    for (UINT08 i = 0; rps.NumPocStLwrrBefore > i; ++i)
        setOfPrevPic.insert(rps.RefPicSetStLwrrBefore[i]);
    for (UINT08 i = 0; rps.NumPocStLwrrAfter > i; ++i)
        setOfPrevPic.insert(rps.RefPicSetStLwrrAfter[i]);
    for (UINT08 i = 0; rps.NumPocStFoll > i; ++i)
        setOfPrevPic.insert(rps.RefPicSetStFoll[i]);
}

void Parser::DoAddIdxToSetOfPrevPocVals(PicIdx idx)
{
    setOfPrevPic.insert(idx);
}

const char* ISlice::DoGetSliceTypeStr() const
{
    return Header().IsIDR() ? "IDR" : (Header().IsReference() ? "I" : "i");
}

void PSlice::DoModifyRefLists(const PicIdx *l0, UINT32 l0size, const PicIdx *, UINT32)
{
    const SliceSegmentHeader &sh = Header();
    const RefPicListsModification &rlMod = sh.ref_pic_lists_modification;

    for (UINT32 i = 0; sh.num_ref_idx_l0_active_minus1 >= i; ++i)
    {
        if (rlMod.ref_pic_list_modification_flag_l0)
        {
            MASSERT(l0size > rlMod.list_entry_l0[i]);
            RefPicList0.push_back(l0[rlMod.list_entry_l0[i]]);
        }
        else
        {
            MASSERT(l0size > i);
            RefPicList0.push_back(l0[i]);
        }
    }
}

const char* PSlice::DoGetSliceTypeStr() const
{
    return Header().IsReference() ? "P" : "p";
}

void BSlice::DoModifyRefLists(const PicIdx *l0, UINT32 l0size, const PicIdx *l1, UINT32 l1size)
{
    const SliceSegmentHeader &sh = Header();
    const RefPicListsModification &rlMod = sh.ref_pic_lists_modification;

    for (UINT32 i = 0; sh.num_ref_idx_l0_active_minus1 >= i; ++i)
    {
        if (rlMod.ref_pic_list_modification_flag_l0)
        {
            MASSERT(l0size > rlMod.list_entry_l0[i]);
            RefPicList0.push_back(l0[rlMod.list_entry_l0[i]]);
        }
        else
        {
            MASSERT(l0size > i);
            RefPicList0.push_back(l0[i]);
        }
    }

    for (UINT32 i = 0; sh.num_ref_idx_l1_active_minus1 >= i; ++i)
    {
        if (rlMod.ref_pic_list_modification_flag_l1)
        {
            MASSERT(l1size > rlMod.list_entry_l1[i]);
            RefPicList1.push_back(l1[rlMod.list_entry_l1[i]]);
        }
        else
        {
            MASSERT(l1size > i);
            RefPicList1.push_back(l1[i]);
        }
    }
}

const char* BSlice::DoGetSliceTypeStr() const
{
    return Header().IsReference() ? "B" : "b";
}

} // namespace H265
