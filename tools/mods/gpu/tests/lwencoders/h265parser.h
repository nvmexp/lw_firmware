/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <map>
#include <memory>
#include <set>
#include <type_traits>

#include <boost/ptr_container/ptr_vector.hpp>

#include "core/include/rc.h"
#include "core/include/types.h"

#include "h265syntax.h"

namespace H265
{
using H26X::NALIArchive;

typedef make_signed_t<size_t> PicIdx;

struct RefPicSet
{
    // used for reference in current picture, smaller POC
    INT32         PocStLwrrBefore[maxNumRefPics];
    UINT08        NumPocStLwrrBefore;

    // used for reference in current picture, larger POC
    INT32         PocStLwrrAfter[maxNumRefPics];
    UINT08        NumPocStLwrrAfter;

    // not used for reference in current picture, but in future picture
    INT32         PocStFoll[maxNumRefPics];
    UINT08        NumPocStFoll;

    // used in current picture
    INT32         PocLtLwrr[maxNumRefPics];
    bool          LwrrDeltaPocMsbPresentFlag[maxNumRefPics];
    UINT08        NumPocLtLwrr;

    // used in some future picture
    INT32         PocLtFoll[maxNumRefPics];
    bool          FollDeltaPocMsbPresentFlag[maxNumRefPics];
    UINT08        NumPocLtFoll;

    // RefPicSetStLwrrBefore, RefPicSetStLwrrAfter, and RefPicSetLtLwrr contain
    // all reference pictures that may be used for inter prediction of the
    // current picture and one or more pictures that follow the current picture
    // in decoding order.
    PicIdx        RefPicSetStLwrrBefore[maxNumRefPics];
    PicIdx        RefPicSetStLwrrAfter[maxNumRefPics];
    PicIdx        RefPicSetLtLwrr[maxNumRefPics];

    // RefPicSetStFoll and RefPicSetLtFoll consist of all reference pictures
    // that are not used for inter prediction of the current picture but may be
    // used in inter prediction for one or more pictures that follow the current
    // picture in decoding order.
    PicIdx        RefPicSetStFoll[maxNumRefPics];
    PicIdx        RefPicSetLtFoll[maxNumRefPics];
};

class Picture;
//! An abstract interface to access decoded pictures in whatever high level
//! class that holds them.
class DecodedPictureSrc
{
public:
    Picture* GetPicture(PicIdx idx)
    {
        return DoGetPicture(idx);
    }

    const Picture* GetPicture(PicIdx idx) const
    {
        return DoGetPicture(idx);
    }

    INT32 PicOrderCnt(PicIdx picX) const
    {
        return DoPicOrderCnt(picX);
    }

    INT32 DiffPicOrderCnt(PicIdx picA, PicIdx picB) const
    {
        return DoDiffPicOrderCnt(picA, picB);
    }

    bool IsSequenceBegin() const
    {
        return DoIsSequenceBegin();
    }

    INT32 GetPrevPicOrderCntLsb() const
    {
        return DoGetPrevPicOrderCntLsb();
    }

    void SetPrevPicOrderCntLsb(INT32 val)
    {
        DoSetPrevPicOrderCntLsb(val);
    }

    INT32 GetPrevPicOrderCntMsb() const
    {
        return DoGetPrevPicOrderCntMsb();
    }

    void SetPrevPicOrderCntMsb(INT32 val)
    {
        DoSetPrevPicOrderCntMsb(val);
    }

    PicIdx GetIdxFromPoc(INT32 poc)
    {
        return DoGetIdxFromPoc(poc);
    }

    PicIdx GetIdxFromLsb(INT32 lsb)
    {
        return DoGetIdxFromLsb(lsb);
    }

    void ClearSetOfPrevPocVals()
    {
        DoClearSetOfPrevPocVals();
    }

    void AddRPSToSetOfPrevPocVals(const RefPicSet &rps)
    {
        DoAddRPSToSetOfPrevPocVals(rps);
    }

    void AddIdxToSetOfPrevPocVals(PicIdx idx)
    {
        DoAddIdxToSetOfPrevPocVals(idx);
    }

private:
    virtual Picture*       DoGetPicture(PicIdx idx) = 0;
    virtual const Picture* DoGetPicture(PicIdx idx) const = 0;
    virtual INT32          DoPicOrderCnt(PicIdx picX) const = 0;
    virtual INT32          DoDiffPicOrderCnt(PicIdx picA, PicIdx picB) const = 0;
    virtual bool           DoIsSequenceBegin() const = 0;
    virtual INT32          DoGetPrevPicOrderCntLsb() const = 0;
    virtual void           DoSetPrevPicOrderCntLsb(INT32 val) = 0;
    virtual INT32          DoGetPrevPicOrderCntMsb() const = 0;
    virtual void           DoSetPrevPicOrderCntMsb(INT32 val) = 0;
    virtual PicIdx         DoGetIdxFromPoc(INT32 poc) = 0;
    virtual PicIdx         DoGetIdxFromLsb(INT32 lsb) = 0;
    virtual void           DoClearSetOfPrevPocVals() = 0;
    virtual void           DoAddRPSToSetOfPrevPocVals(const RefPicSet &rps) = 0;
    virtual void           DoAddIdxToSetOfPrevPocVals(PicIdx idx) = 0;
};

class Slice
{
    friend class Picture;
    template <class T> friend class SliceContainer;

    typedef vector<SliceSegmentHeader> SliceSegments;

public:
    typedef SliceSegmentHeader::ShortTermRefPicSet ShortTermRefPicSet;

    typedef SliceSegments::iterator       segment_iterator;
    typedef SliceSegments::const_iterator segment_const_iterator;

    typedef vector<PicIdx>::const_iterator ref_iterator;

    Slice(const SliceSegmentHeader &sliceHeader)
    {
        m_segments.push_back(sliceHeader);
    }

    virtual ~Slice()
    {}

    segment_iterator segments_begin()
    {
        return m_segments.begin();
    }

    segment_const_iterator segments_begin() const
    {
        return m_segments.begin();
    }

    segment_iterator segments_end()
    {
        return m_segments.end();
    }

    segment_const_iterator segments_end() const
    {
        return m_segments.end();
    }

    ref_iterator l0_begin() const
    {
        return do_l0_begin();
    }

    ref_iterator l0_end() const
    {
        return do_l0_end();
    }

    ref_iterator l1_begin() const
    {
        return do_l1_begin();
    }

    ref_iterator l1_end() const
    {
        return do_l1_end();
    }

    bool l0_empty() const
    {
        return do_l0_empty();
    }

    bool l1_empty() const
    {
        return do_l1_empty();
    }

    Slice* Clone() const
    {
        return DoClone();
    }

    const SliceSegmentHeader& Header() const
    {
        return m_segments[0];
    }

    void AddSegment(const SliceSegmentHeader &sliceHeader)
    {
        m_segments.push_back(sliceHeader);
    }

    //! Creates reference lists RefPicList0 and RefPicList1 out of specified
    //! RefPicListTemp0 and RefPicListTemp1.
    void ModifyRefLists(const PicIdx *l0, UINT32 l0size, const PicIdx *l1, UINT32 l1size)
    {
        DoModifyRefLists(l0, l0size, l1, l1size);
    }

    const PicIdx* GetRefPicList0() const
    {
        return DoGetRefPicList0();
    }

    const PicIdx* GetRefPicList1() const
    {
        return DoGetRefPicList1();
    }

    UINT32 GetL0Size() const
    {
        return DoGetL0Size();
    }

    UINT32 GetL1Size() const
    {
        return DoGetL1Size();
    }

    const char* GetSliceTypeStr() const
    {
        return DoGetSliceTypeStr();
    }

private:
    virtual ref_iterator  do_l0_begin() const = 0;
    virtual ref_iterator  do_l0_end() const = 0;
    virtual ref_iterator  do_l1_begin() const = 0;
    virtual ref_iterator  do_l1_end() const = 0;
    virtual bool          do_l0_empty() const = 0;
    virtual bool          do_l1_empty() const = 0;
    virtual void          DoModifyRefLists(const PicIdx *l0, UINT32 l0size,
                                           const PicIdx *l1, UINT32 l1size) = 0;
    virtual const PicIdx* DoGetRefPicList0() const = 0;
    virtual const PicIdx* DoGetRefPicList1() const = 0;
    virtual UINT32        DoGetL0Size() const = 0;
    virtual UINT32        DoGetL1Size() const = 0;
    virtual const char*   DoGetSliceTypeStr() const = 0;

    virtual Slice* DoClone() const = 0;

    const VidParameterSet& GetVPS() const
    {
        return m_segments[0].GetVPS();
    }

    const SeqParameterSet& GetSPS() const
    {
        return m_segments[0].GetSPS();
    }

    const PicParameterSet& GetPPS() const
    {
        return m_segments[0].GetPPS();
    }

    const VidParamSetSrc* GetVidParamSrc() const
    {
        return m_segments[0].GetVidParamSrc();
    }

    void SetVidParamSrc(const VidParamSetSrc *src)
    {
        m_segments[0].SetVidParamSrc(src);
    }

    const SeqParamSetSrc* GetSeqParamSrc() const
    {
        return m_segments[0].GetSeqParamSrc();
    }

    void SetSeqParamSrc(const SeqParamSetSrc *src)
    {
        m_segments[0].SetSeqParamSrc(src);
    }

    const PicParamSetSrc* GetPicParamSrc() const
    {
        return m_segments[0].GetPicParamSrc();
    }

    void SetPicParamSrc(const PicParamSetSrc *src)
    {
        m_segments[0].SetPicParamSrc(src);
    }

    // A slice consists of an independent slice segment plus zero or more
    // dependent segments. See 6.3.1 of ITU-T H.265.
    vector<SliceSegmentHeader> m_segments;
};

inline
Slice* new_clone(const Slice& a)
{
    return a.Clone();
}

class ISlice : public Slice
{
public:
    ISlice(const SliceSegmentHeader &sliceHeader)
      : Slice(sliceHeader)
    {}

private:
    Slice* DoClone() const
    {
        return new ISlice(*this);
    }

    ref_iterator do_l0_begin() const
    {
        return ref_iterator();
    }

    ref_iterator do_l0_end() const
    {
        return ref_iterator();
    }

    ref_iterator do_l1_begin() const
    {
        return ref_iterator();
    }

    ref_iterator do_l1_end() const
    {
        return ref_iterator();
    }

    bool do_l0_empty() const
    {
        return true;
    }

    bool do_l1_empty() const
    {
        return true;
    }

    void DoModifyRefLists(const PicIdx *l0, UINT32 l0size, const PicIdx *l1, UINT32 l1size)
    {}

    const PicIdx* DoGetRefPicList0() const
    {
        return nullptr;
    }

    const PicIdx* DoGetRefPicList1() const
    {
        return nullptr;
    }

    UINT32 DoGetL0Size() const
    {
        return 0;
    }

    UINT32 DoGetL1Size() const
    {
        return 0;
    }

    const char* DoGetSliceTypeStr() const;
};

class PSlice : public Slice
{
public:
    PSlice(const SliceSegmentHeader &sliceHeader)
      : Slice(sliceHeader)
    {}

private:
    Slice* DoClone() const
    {
        return new PSlice(*this);
    }

    ref_iterator do_l0_begin() const
    {
        return RefPicList0.begin();
    }

    ref_iterator do_l0_end() const
    {
        return RefPicList0.end();
    }

    ref_iterator do_l1_begin() const
    {
        return ref_iterator();
    }

    ref_iterator do_l1_end() const
    {
        return ref_iterator();
    }

    bool do_l0_empty() const
    {
        return RefPicList0.empty();
    }

    bool do_l1_empty() const
    {
        return true;
    }

    void DoModifyRefLists(const PicIdx *l0, UINT32 l0size, const PicIdx *l1, UINT32 l1size);

    const PicIdx* DoGetRefPicList0() const
    {
        return &RefPicList0[0];
    }

    const PicIdx* DoGetRefPicList1() const
    {
        return nullptr;
    }

    UINT32 DoGetL0Size() const
    {
        return static_cast<UINT32>(RefPicList0.size());
    }

    UINT32 DoGetL1Size() const
    {
        return 0;
    }

    const char* DoGetSliceTypeStr() const;

    vector<PicIdx> RefPicList0;
};

class BSlice : public Slice
{
public:
    BSlice(const SliceSegmentHeader &sliceHeader)
      : Slice(sliceHeader)
    {}

private:
    Slice* DoClone() const
    {
        return new BSlice(*this);
    }

    ref_iterator do_l0_begin() const
    {
        return RefPicList0.begin();
    }

    ref_iterator do_l0_end() const
    {
        return RefPicList0.end();
    }

    ref_iterator do_l1_begin() const
    {
        return RefPicList1.begin();
    }

    ref_iterator do_l1_end() const
    {
        return RefPicList1.end();
    }

    bool do_l0_empty() const
    {
        return RefPicList0.empty();
    }

    bool do_l1_empty() const
    {
        return RefPicList1.empty();
    }

    void DoModifyRefLists(const PicIdx *l0, UINT32 l0size, const PicIdx *l1, UINT32 l1size);

    const PicIdx* DoGetRefPicList0() const
    {
        return &RefPicList0[0];
    }

    const PicIdx* DoGetRefPicList1() const
    {
        return &RefPicList1[0];
    }

    UINT32 DoGetL0Size() const
    {
        return static_cast<UINT32>(RefPicList0.size());
    }

    UINT32 DoGetL1Size() const
    {
        return static_cast<UINT32>(RefPicList1.size());
    }

    const char* DoGetSliceTypeStr() const;

    vector<PicIdx> RefPicList0;
    vector<PicIdx> RefPicList1;
};

template <class T>
class SliceContainer
{
protected:
    typedef boost::ptr_vector<Slice> Slices;
    typedef Slices::iterator         iterator;
    typedef Slices::const_iterator   const_iterator;

    Slices m_slices;

    SliceContainer()
    {}

    SliceContainer(const SliceContainer& r)
      : m_slices(r.m_slices)
    {
        for (size_t i = 0; m_slices.size() > i; ++i)
        {
            m_slices[i].SetVidParamSrc(This());
            m_slices[i].SetSeqParamSrc(This());
            m_slices[i].SetPicParamSrc(This());
        }
    }

    SliceContainer& operator = (const SliceContainer& rhs)
    {
        if (this != &rhs)
        {
            m_slices = rhs.m_slices;
            for (size_t i = 0; m_slices.size() > i; ++i)
            {
                m_slices[i].SetVidParamSrc(This());
                m_slices[i].SetSeqParamSrc(This());
                m_slices[i].SetPicParamSrc(This());
            }
        }

        return *this;
    }

private:
    const T * This() const
    {
        return static_cast<const T *>(this);
    }
};

template <class T>
class SPSField
{
protected:
    SeqParameterSet m_sps;

    SPSField()
    {}

    SPSField(const SPSField& rhs)
      : m_sps(rhs.m_sps)
    {
        m_sps.SetVidParamSrc(This());
    }

    SPSField& operator=(const SPSField &rhs)
    {
        if (this != &rhs)
        {
            m_sps = rhs.m_sps;
            m_sps.SetVidParamSrc(This());
        }

        return *this;
    }

private:
    const T * This() const
    {
        return static_cast<const T *>(this);
    }
};

template <class T>
class PPSField
{
protected:
    PicParameterSet m_pps;

    PPSField()
    {}

    PPSField(const PPSField& rhs)
      : m_pps(rhs.m_pps)
    {
        m_pps.SetSeqParamSrc(This());
    }

    PPSField& operator=(const PPSField &rhs)
    {
        if (this != &rhs)
        {
            m_pps = rhs.m_pps;
            m_pps.SetSeqParamSrc(This());
        }

        return *this;
    }

private:
    const T * This() const
    {
        return static_cast<const T *>(this);
    }
};

class Picture :
    public VidParamSetSrc
  , public SeqParamSetSrc
  , public PicParamSetSrc
  , public SPSField<Picture>
  , public PPSField<Picture>
  , public SliceContainer<Picture>
{
    template <class T> friend class PictureContainer;
    friend class SliceContainer<Picture>;

public:
    typedef SliceContainer<Picture>::iterator       slice_iterator;
    typedef SliceContainer<Picture>::const_iterator slice_const_iterator;
    typedef SliceContainer<Picture>::Slices         Slices;
    typedef Slice::ShortTermRefPicSet               ShortTermRefPicSet;

    Picture(
        const VidParamSetSrc    *vidParamSrc,
        const SeqParamSetSrc    *seqParamSrc,
        const PicParamSetSrc    *picParamSrc,
              DecodedPictureSrc *picSrc,
              PicIdx             idx
    );

    Picture(Slice *newSlice, DecodedPictureSrc *picSrc, PicIdx idx);

    virtual ~Picture()
    {}

    RC AddSliceFromStream(NALU &nalu, unique_ptr<Slice> *ppRejectedSlice);

    UINT32 Width() const
    {
        return m_sps.pic_width_in_luma_samples;
    }

    UINT32 Height() const
    {
        return m_sps.pic_height_in_luma_samples;
    }

    unsigned int GetSlicesEBSPSize() const
    {
        size_t count = 0;
        slice_const_iterator slIt;
        for (slIt = slices_begin(); slices_end() != slIt; ++slIt)
        {
            Slice::segment_const_iterator segIt;
            for (segIt = slIt->segments_begin(); slIt->segments_end() != segIt; ++segIt)
            {
                count += segIt->GetEbspSize();
            }
        }

        return static_cast<unsigned int>(count);
    }

    UINT32 GetMaxDecPicBuffering() const
    {
        return GetSPS().sps_max_dec_pic_buffering_minus1[GetSPS().sps_max_sub_layers_minus1] + 1;
    }

    const PicIdx* GetRefPicListTemp0() const
    {
        // 0 instead of nullptr is the homage to GCC 2.95
        return 0 == m_refPicListTemp0Size ? 0 : &RefPicListTemp0[0];
    }

    const PicIdx* GetRefPicListTemp1() const
    {
        // 0 instead of nullptr is the homage to GCC 2.95
        return 0 == m_refPicListTemp1Size ? 0 : &RefPicListTemp1[0];
    }

    UINT32 GetRefPicListTemp0Size() const
    {
        return m_refPicListTemp0Size;
    }

    UINT32 GetRefPicListTemp1Size() const
    {
        return m_refPicListTemp1Size;
    }

    unsigned char GetNumRefFrames() const
    {
        return static_cast<unsigned char>(NumPocTotalLwrr);
    }

    slice_iterator slices_begin()
    {
        return m_slices.begin();
    }

    slice_const_iterator slices_begin() const
    {
        return m_slices.begin();
    }

    slice_iterator slices_end()
    {
        return m_slices.end();
    }

    slice_const_iterator slices_end() const
    {
        return m_slices.end();
    }

    Slice& slices_front()
    {
        return m_slices.front();
    }

    const Slice& slices_front() const
    {
        return m_slices.front();
    }

    Slice& slices_back()
    {
        return m_slices.back();
    }

    const Slice& slices_back() const
    {
        return m_slices.back();
    }

    UINT32 GetNumSlices() const
    {
        return static_cast<UINT32>(m_slices.size());
    }

    PicIdx GetPicIdx() const
    {
        return m_picIdx;
    }

    const VidParameterSet& GetVPS() const
    {
        return m_vps;
    }

    const SeqParameterSet& GetSPS() const
    {
        return m_sps;
    }

    const PicParameterSet& GetPPS() const
    {
        return m_pps;
    }

    bool IsIRAP() const
    {
        return nal_unit_type.IsIRAP();
    }

    bool IsIDR() const
    {
        return nal_unit_type.IsIDR();
    }

    bool IsCRA() const
    {
        return nal_unit_type.IsCRA();
    }

    bool IsBLA() const
    {
        return nal_unit_type.IsBLA();
    }

    bool IsReference() const
    {
        return nal_unit_type.IsReference();
    }

    // From 8.3.1 of ITU-T H.265: let prevTid0Pic be the previous picture in
    // decoding order that has TemporalId equal to 0 and that is not a RASL
    // picture, a RADL picture, or a sub-layer non-reference picture. NAL unit
    // type is tested against only RASL_R and RADL_R, but not RASL_N and RADL_N,
    // because the latter were excluded by IsReference().
    bool IsTid0Pic() const
    {
        return 0 == TemporalId && IsReference() &&
               NAL_TYPE_RASL_R != nal_unit_type &&
               NAL_TYPE_RADL_R != nal_unit_type;
    }

    const RefPicSet& GetRps() const
    {
        return m_rps;
    }

    const ScalingListData* GetScalingListData() const;

    NalUnitType  nal_unit_type;
    UINT08       TemporalId;
    UINT08       NumPocTotalLwrr;
    bool         NoRaslOutputFlag;
    INT32        MaxPicOrderCntLsb;
    INT32        PicOrderCntVal;
    unsigned int sw_hdr_skip_length;
    unsigned int num_bits_short_term_ref_pics_in_slice;

private:
    void SetVidParamSrc(const VidParamSetSrc *src)
    {
        m_vidParamSrc = src;
    }

    void SetSeqParamSrc(const SeqParamSetSrc *src)
    {
        m_seqParamSrc = src;
    }

    void SetPicParamSrc(const PicParamSetSrc *src)
    {
        m_picParamSrc = src;
    }

    void SetPicSrc(DecodedPictureSrc *src)
    {
        m_picSrc = src;
    }

    const VidParameterSet& DoGetVPS(size_t id) const
    {
        // GCC 2.95 thinks vps_video_parameter_set_id is signed, so static_cast
        MASSERT(id == static_cast<size_t>(m_vps.vps_video_parameter_set_id));
        return m_vps;
    }

    const SeqParameterSet& DoGetSPS(size_t id) const
    {
        MASSERT(id == m_sps.sps_seq_parameter_set_id);
        return m_sps;
    }

    const PicParameterSet& DoGetPPS(size_t id) const
    {
        MASSERT(id == m_pps.pps_pic_parameter_set_id);
        return m_pps;
    }

    void InitFromFirstSlice();
    void CallwlatePOC();
    void ConstructRPS();

    // RefPicListTemp0 and RefPicListTemp1 almost correspond to ITU_T H.264 as
    // it defines it in (8-8) and (8-10). However since
    // num_ref_idx_l0_active_minus1 and num_ref_idx_l1_active_minus1 can be
    // different per slice, per picture we callwlate the largest size possible:
    // 16. Every slice will truncate these lists according to correspondent
    // NumPocTotalLwrr and num_ref_idx_l0_active_minus1 and
    // num_ref_idx_l1_active_minus1. Note that NumPocTotalLwrr is the same for
    // all slices.
    PicIdx RefPicListTemp0[maxNumRefPics];
    PicIdx RefPicListTemp1[maxNumRefPics];
    UINT32 m_refPicListTemp0Size;
    UINT32 m_refPicListTemp1Size;

    const VidParamSetSrc    *m_vidParamSrc;
    const SeqParamSetSrc    *m_seqParamSrc;
    const PicParamSetSrc    *m_picParamSrc;
          DecodedPictureSrc *m_picSrc;

    ScalingListData          m_scalingListData;

    VidParameterSet          m_vps; // SPS and PPS are inherited to simplify copying

    RefPicSet                m_rps = {};

    PicIdx                   m_picIdx;
};

template <class T>
class SPSContainer
{
    typedef map<size_t, SeqParameterSet> SPSs;

protected:
    SPSs m_seqParameterSets;

    SPSContainer()
    {}

    SPSContainer(const SPSContainer& rhs)
    {
        SPSs::const_iterator it;
        for (it = rhs.m_seqParameterSets.begin(); rhs.m_seqParameterSets.end() != it; ++it)
        {
            if (!it->second.IsEmpty())
            {
                m_seqParameterSets.insert(*it).first->second.SetVidParamSrc(This());
            }
        }
    }

    SPSContainer& operator=(const SPSContainer &rhs)
    {
        if (this != &rhs)
        {
            m_seqParameterSets.clear();
            SPSs::const_iterator it;
            for (it = rhs.m_seqParameterSets.begin(); rhs.m_seqParameterSets.end() != it; ++it)
            {
                if (!it->second.IsEmpty())
                {
                    m_seqParameterSets.insert(*it).first->second.SetVidParamSrc(This());
                }
            }
        }

        return *this;
    }

private:
    const T * This() const
    {
        return static_cast<const T *>(this);
    }
};

template <class T>
class PPSContainer
{
    typedef map<size_t, PicParameterSet> PPSs;

protected:
    PPSs m_picParameterSets;

    PPSContainer()
    {}

    PPSContainer(const PPSContainer& rhs)
    {
        PPSs::const_iterator it;
        for (it = rhs.m_picParameterSets.begin(); rhs.m_picParameterSets.end() != it; ++it)
        {
            if (!it->second.IsEmpty())
            {
                m_picParameterSets.insert(*it).first->second.SetSeqParamSrc(This());
            }
        }
    }

    PPSContainer& operator = (const PPSContainer &rhs)
    {
        if (this != &rhs)
        {
            m_picParameterSets.clear();
            PPSs::const_iterator it;
            for (it = rhs.m_picParameterSets.begin(); rhs.m_picParameterSets.end() != it; ++it)
            {
                if (!it->second.IsEmpty())
                {
                    m_picParameterSets.insert(*it).first->second.SetSeqParamSrc(This());
                }
            }
        }

        return *this;
    }

private:
    const T * This() const
    {
        return static_cast<const T *>(this);
    }
};

template <class T>
class PictureContainer
{
protected:
    typedef vector<Picture>                  Pictures;
    typedef Pictures::iterator               iterator;
    typedef Pictures::const_iterator         const_iterator;
    typedef Pictures::reverse_iterator       reverse_iterator;
    typedef Pictures::const_reverse_iterator const_reverse_iterator;

    Pictures m_pictures;

    PictureContainer()
    {}

    PictureContainer(const PictureContainer& r)
        : m_pictures(r.m_pictures)
    {
        iterator it;
        for (it = m_pictures.begin(); m_pictures.end() != it; ++it)
        {
            it->SetVidParamSrc(This());
            it->SetSeqParamSrc(This());
            it->SetPicParamSrc(This());
            it->SetPicSrc(This());
        }
    }

    PictureContainer& operator = (const PictureContainer& rhs)
    {
        if (this != &rhs)
        {
            m_pictures = rhs.m_pictures;
            iterator it;
            for (it = m_pictures.begin(); m_pictures.end() != it; ++it)
            {
                it->SetVidParamSrc(This());
                it->SetSeqParamSrc(This());
                it->SetPicParamSrc(This());
                it->SetPicSrc(This());
            }
        }

        return *this;
    }

private:
    T * This()
    {
        return static_cast<T *>(this);
    }

    const T * This() const
    {
        return static_cast<const T *>(this);
    }
};

class Parser :
    public VidParamSetSrc
  , public SeqParamSetSrc
  , public PicParamSetSrc
  , public DecodedPictureSrc
  , public SPSContainer<Parser>
  , public PPSContainer<Parser>
  , public PictureContainer<Parser>
{
    map<size_t, VidParameterSet> m_vidParameterSets;

public:
    typedef PictureContainer<Parser>::iterator       pic_iterator;
    typedef PictureContainer<Parser>::const_iterator pic_const_iterator;
    typedef PictureContainer<Parser>::Pictures       Pictures;

    size_t GetNumFrames() const
    {
        return m_pictures.size();
    }

    pic_iterator pic_begin()
    {
        return m_pictures.begin();
    }

    pic_const_iterator pic_begin() const
    {
        return m_pictures.begin();
    }

    pic_iterator pic_end()
    {
        return m_pictures.end();
    }

    pic_const_iterator pic_end() const
    {
        return m_pictures.end();
    }

    Picture& pic_front()
    {
        return *(m_pictures.begin());
    }

    const Picture& pic_front() const
    {
        return *(m_pictures.begin());
    }

    Picture& pic_back()
    {
        return *(m_pictures.end() - 1);
    }

    const Picture& pic_back() const
    {
        return *(m_pictures.end() - 1);
    }

    UINT32 GetWidth() const
    {
        if (m_pictures.empty())
            return 0;

        return m_pictures.front().Width();
    }

    UINT32 GetHeight() const
    {
        if (m_pictures.empty())
            return 0;

        return m_pictures.front().Height();
    }

    UINT32 GetMaxDecPicBuffering() const;

    UINT32 GetBitDepthLuma() const
    {
        return pic_front().GetSPS().bit_depth_luma_minus8 + 8;
    }

    UINT32 GetBitDepthChroma() const
    {
        return pic_front().GetSPS().bit_depth_chroma_minus8 + 8;
    }

    UINT32 GetChromaFormatIdc() const
    {
        return pic_front().GetSPS().chroma_format_idc;
    }

    UINT32 PicWidthInCtbsY() const
    {
        return pic_front().GetSPS().PicWidthInCtbsY;
    }

    UINT32 PicHeightInCtbsY() const
    {
        return pic_front().GetSPS().PicHeightInCtbsY;
    }

    UINT32 CtbSizeY() const
    {
        return pic_front().GetSPS().CtbSizeY;
    }

    UINT32 CtbLog2SizeY() const
    {
        return pic_front().GetSPS().CtbLog2SizeY;
    }

    template <class InputIterator>
    RC ParseStream(InputIterator start, InputIterator finish)
    {
        using namespace BitsIO;

        RC rc;

        m_seqBegin = true;
        bool metEOS = false;

        NALIArchive<InputIterator> ia(start, finish);
        NALU nalu;

        while (!ia.IsEOS())
        {
            ia >> nalu;

            if (nalu.IsValid())
            {
                if (NAL_TYPE_EOB_NUT == nalu.GetUnitType())
                {
                    break;
                }

                switch (nalu.GetUnitType())
                {
                    case NAL_TYPE_VPS_NUT:
                    {
                        VidParameterSet vps;
                        BitIArchive<NALU::RBSPIterator> ia(nalu.RBSPBegin(), nalu.RBSPEnd());
                        ia >> vps;

                        m_vidParameterSets[vps.vps_video_parameter_set_id] = vps;

                        break;
                    }
                    case NAL_TYPE_SPS_NUT:
                    {
                        SeqParameterSet sps(this);
                        BitIArchive<NALU::RBSPIterator> ia(nalu.RBSPBegin(), nalu.RBSPEnd());
                        ia >> sps;

                        m_seqParameterSets[sps.sps_seq_parameter_set_id] = sps;

                        break;
                    }
                    case NAL_TYPE_PPS_NUT:
                    {
                        PicParameterSet pps(this);
                        BitIArchive<NALU::RBSPIterator> ia(nalu.RBSPBegin(), nalu.RBSPEnd());
                        ia >> pps;

                        m_picParameterSets[pps.pps_pic_parameter_set_id] = pps;

                        break;
                    }
                    case NAL_TYPE_TRAIL_R:
                    case NAL_TYPE_TRAIL_N:
                    case NAL_TYPE_TSA_R:
                    case NAL_TYPE_TSA_N:
                    case NAL_TYPE_STSA_R:
                    case NAL_TYPE_STSA_N:
                    case NAL_TYPE_BLA_W_LP:
                    case NAL_TYPE_BLA_W_RADL:
                    case NAL_TYPE_BLA_N_LP:
                    case NAL_TYPE_IDR_W_RADL:
                    case NAL_TYPE_IDR_N_LP:
                    case NAL_TYPE_CRA_NUT:
                    case NAL_TYPE_RADL_N:
                    case NAL_TYPE_RADL_R:
                    case NAL_TYPE_RASL_N:
                    case NAL_TYPE_RASL_R:
                    {
                        PictureContainer<Parser>::iterator lastPicIt;
                        if (m_pictures.empty())
                        {
                            m_pictures.push_back(Picture(this, this, this, this, 0));
                        }
                        lastPicIt = m_pictures.begin() + m_pictures.size() - 1;
                        unique_ptr<Slice> rejectedSlice;
                        CHECK_RC(lastPicIt->AddSliceFromStream(nalu, &rejectedSlice));
                        if (rejectedSlice) // not from this picture
                        {
                            if (!metEOS)
                            {
                                m_seqBegin = false;
                            }

                            PicIdx idx = lastPicIt - m_pictures.begin();
                            m_pictures.push_back(Picture(rejectedSlice.release(), this, idx + 1));

                            if (metEOS)
                            {
                                m_seqBegin = false;
                            }
                        }

                        break;
                    }
                    default:
                        break;
                }
                if (NAL_TYPE_EOS_NUT == nalu.GetUnitType())
                {
                    metEOS = true;
                    m_seqBegin = true;
                }
            }
        }

        return rc;
    }

private:
    const VidParameterSet& DoGetVPS(size_t id) const
    {
        return m_vidParameterSets.find(id)->second;
    }

    const SeqParameterSet& DoGetSPS(size_t id) const
    {
        return m_seqParameterSets.find(id)->second;
    }

    const PicParameterSet& DoGetPPS(size_t id) const
    {
        return m_picParameterSets.find(id)->second;
    }

    virtual Picture* DoGetPicture(PicIdx idx)
    {
        return &m_pictures[idx];
    }

    virtual const Picture* DoGetPicture(PicIdx idx) const
    {
        return &m_pictures[idx];
    }

    virtual bool DoIsSequenceBegin() const
    {
        return m_seqBegin;
    }

    virtual INT32 DoGetPrevPicOrderCntLsb() const
    {
        return prevPicOrderCntLsb;
    }

    virtual void DoSetPrevPicOrderCntLsb(INT32 val)
    {
        prevPicOrderCntLsb = val;
    }

    virtual INT32 DoGetPrevPicOrderCntMsb() const
    {
        return prevPicOrderCntMsb;
    }

    virtual void DoSetPrevPicOrderCntMsb(INT32 val)
    {
        prevPicOrderCntMsb = val;
    }

    virtual INT32 DoPicOrderCnt(PicIdx picX) const
    {
        return m_pictures[picX].PicOrderCntVal;
    }

    virtual INT32 DoDiffPicOrderCnt(PicIdx picA, PicIdx picB) const
    {
        return m_pictures[picA].PicOrderCntVal - m_pictures[picB].PicOrderCntVal;
    }

    virtual PicIdx DoGetIdxFromPoc(INT32 poc);
    virtual PicIdx DoGetIdxFromLsb(INT32 lsb);
    virtual void DoClearSetOfPrevPocVals();
    virtual void DoAddRPSToSetOfPrevPocVals(const RefPicSet &rps);
    virtual void DoAddIdxToSetOfPrevPocVals(PicIdx idx);

    INT32       prevPicOrderCntLsb;
    INT32       prevPicOrderCntMsb;
    set<PicIdx> setOfPrevPic;

    bool m_seqBegin;
};

} // namespace H265
