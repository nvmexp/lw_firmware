/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2018,2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <vector>

#include <boost/ptr_container/ptr_vector.hpp>

#include "core/include/rc.h"

#include "h264decodedpicturesrc.h"
#include "h264dpb.h"
#include "h264syntax.h"
#include "h26xsyntax.h"

namespace H264
{
using H26X::NALIArchive;

extern unsigned char Default_4x4_Intra[16];
extern unsigned char Default_4x4_Inter[16];
extern unsigned char Default_8x8_Intra[64];
extern unsigned char Default_8x8_Inter[64];

//! An abstract interface to access the PicOrderCnt. This syntax element
//! enumerates pictures and fields in display order. See section 8.2.1.
class PicOrderCntSrc
{
public:
    virtual ~PicOrderCntSrc() {}

    int GetPicOrderCnt() const
    {
        return DoGetPicOrderCnt();
    }

    int GetPicOrderCnt(size_t idx) const
    {
        return DoGetPicOrderCnt(idx);
    }

private:
    virtual int DoGetPicOrderCnt() const = 0;
    virtual int DoGetPicOrderCnt(size_t idx) const = 0;
};

//! An abstract base class for various types of slices: I, P and B.
class Slice
{
    friend class Picture;
    template <class T> friend class SliceContainer;

public:
    typedef vector<size_t>::const_iterator ref_iterator;

    Slice(const SliceHeader &sliceHeader);

    virtual ~Slice()
    {}

    Slice* Clone() const
    {
        return DoClone();
    }

    int GetPicOrderCnt() const
    {
        return m_picOrderCnt->GetPicOrderCnt();
    }

    int GetPicOrderCnt(size_t idx) const
    {
        return m_picOrderCnt->GetPicOrderCnt(idx);
    }

    const UINT08* GetEbspPayload() const
    {
        return m_sliceHeader.GetEbspPayload();
    }

    size_t GetEbspSize() const
    {
        return m_sliceHeader.GetEbspSize();
    }

    const char* GetSliceTypeStr()
    {
        return DoGetSliceTypeStr();
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

    size_t l0_front() const
    {
        return *l0_begin();
    }

    size_t l1_front() const
    {
        return *l1_begin();
    }

    size_t l0_size() const
    {
        return do_l0_size();
    }

    size_t l1_size() const
    {
        return do_l1_size();
    }

protected:
    const DPB& GetDPB() const
    {
        return m_DPBSrc->GetDPB();
    }

    bool IsIDR() const
    {
        return m_sliceHeader.IsIDR();
    }

    bool IsReference() const
    {
        return m_sliceHeader.IsReference();
    }

    size_t NumRefIdxActive(size_t listIdx) const
    {
        return m_sliceHeader.num_ref_idx_active_minus1[listIdx] + 1;
    }

    bool RefPicListReorderingFlagL0() const
    {
        return m_sliceHeader.RefPicListReorderingFlagL0();
    }

    bool RefPicListReorderingFlagL1() const
    {
        return m_sliceHeader.RefPicListReorderingFlagL1();
    }

    void ReorderRefPicList(vector<size_t> *refList, size_t listIdx);

private:
    virtual Slice* DoClone() const = 0;
    virtual void DoCreateRefLists() = 0;
    virtual const char* DoGetSliceTypeStr() = 0;
    virtual ref_iterator do_l0_begin() const = 0;
    virtual ref_iterator do_l0_end() const = 0;
    virtual ref_iterator do_l1_begin() const = 0;
    virtual ref_iterator do_l1_end() const = 0;
    virtual bool do_l0_empty() const = 0;
    virtual bool do_l1_empty() const = 0;
    virtual size_t do_l0_size() const = 0;
    virtual size_t do_l1_size() const = 0;

    const SeqParameterSet& GetSPS() const
    {
        return m_sliceHeader.GetSPS();
    }

    const PicParameterSet& GetPPS() const
    {
        return m_sliceHeader.GetPPS();
    }

    unsigned int PicParameterSetId() const
    {
        return m_sliceHeader.pic_parameter_set_id;
    }

    const SeqParamSetSrc* GetSeqParamSrc() const
    {
        return m_sliceHeader.GetSeqParamSrc();
    }

    void SetSeqParamSrc(const SeqParamSetSrc *src)
    {
        m_sliceHeader.SetSeqParamSrc(src);
    }

    const PicParamSetSrc* GetPicParamSrc() const
    {
        return m_sliceHeader.GetPicParamSrc();
    }

    void SetPicParamSrc(const PicParamSetSrc *src)
    {
        m_sliceHeader.SetPicParamSrc(src);
    }

    void SetDPBSrc(const DPBSrc *src)
    {
        m_DPBSrc = src;
    }

    void SetPicOrderCntSrc(const PicOrderCntSrc *src)
    {
        m_picOrderCnt= src;
    }

    bool IsTheSamePic(const Slice& slice);

    void CreateRefLists()
    {
        DoCreateRefLists();
    }

    bool HasMMCO5() const
    {
        return m_sliceHeader.HasMMCO5();
    }

    unsigned int FrameNum() const
    {
        return m_sliceHeader.frame_num;
    }

    bool FieldPicFlag() const
    {
        return m_sliceHeader.field_pic_flag;
    }

    bool BottomFieldFlag() const
    {
        return m_sliceHeader.bottom_field_flag;
    }

    bool MbaffFrameFlag() const
    {
        return m_sliceHeader.MbaffFrameFlag;
    }

    unsigned int NalRefIdc() const
    {
        return m_sliceHeader.GetNaluRefIdc();
    }

    unsigned int IDRPicId() const
    {
        return m_sliceHeader.idr_pic_id;
    }

    int DeltaPicOderCnt(size_t idx) const
    {
        return m_sliceHeader.delta_pic_order_cnt[idx];
    }

    int DeltaPicOderCntBottom() const
    {
        return m_sliceHeader.delta_pic_order_cnt_bottom;
    }

    unsigned int PicOderCntLsb() const
    {
        return m_sliceHeader.pic_order_cnt_lsb;
    }

    bool NoOutputOfPriorPicsFlag() const
    {
        return m_sliceHeader.no_output_of_prior_pics_flag;
    }

    bool LongTermReferenceFlag() const
    {
        return m_sliceHeader.long_term_reference_flag;
    }

    bool AdaptiveRefPicMarkingModeFlag() const
    {
        return m_sliceHeader.adaptive_ref_pic_marking_mode_flag;
    }

    const list<DecRefPicMarking>& DecRefPicMarkingList() const
    {
        return m_sliceHeader.dec_ref_pic_marking_list;
    }

    const list<RefListModifier>& RefPicListMods(size_t listIdx)
    {
        return m_sliceHeader.RefPicListMods(listIdx);
    }

    unsigned int LwrrPicNum() const
    {
        return m_sliceHeader.LwrrPicNum();
    }

    unsigned int MaxPicNum() const
    {
        return m_sliceHeader.MaxPicNum();
    }

    SliceHeader           m_sliceHeader;
    const DPBSrc         *m_DPBSrc;
    const PicOrderCntSrc *m_picOrderCnt;
    size_t                m_startOffset;
    size_t                m_naluSize;
};

inline
Slice* new_clone(const Slice& a)
{
    return a.Clone();
}

class ISlice : public Slice
{
public:
    ISlice(const SliceHeader &sliceHeader)
      : Slice(sliceHeader)
    {}

private:
    Slice* DoClone() const override
    {
        return new ISlice(*this);
    }

    void DoCreateRefLists() override
    {}

    const char* DoGetSliceTypeStr() override
    {
        return IsIDR() ? "IDR" : (IsReference() ? " I " : " i ");
    }

    ref_iterator do_l0_begin() const override
    {
        return ref_iterator();
    }

    ref_iterator do_l0_end() const override
    {
        return ref_iterator();
    }

    ref_iterator do_l1_begin() const override
    {
        return ref_iterator();
    }

    ref_iterator do_l1_end() const override
    {
        return ref_iterator();
    }

    bool do_l0_empty() const override
    {
        return true;
    }

    bool do_l1_empty() const override
    {
        return true;
    }

    size_t do_l0_size() const override
    {
        return 0;
    }

    size_t do_l1_size() const override
    {
        return 0;
    }
};

class PSlice : public Slice
{
public:
    PSlice(const SliceHeader &sliceHeader)
      : Slice(sliceHeader)
    {}

private:
    Slice* DoClone() const override
    {
        return new PSlice(*this);
    }

    void DoCreateRefLists() override;

    const char* DoGetSliceTypeStr() override
    {
        return IsReference() ? " P " : " p ";
    }

    ref_iterator do_l0_begin() const override
    {
        return m_refList.begin();
    }

    ref_iterator do_l0_end() const override
    {
        return m_refList.end();
    }

    ref_iterator do_l1_begin() const override
    {
        return ref_iterator();
    }

    ref_iterator do_l1_end() const override
    {
        return ref_iterator();
    }

    bool do_l0_empty() const override
    {
        return m_refList.empty();
    }

    bool do_l1_empty() const override
    {
        return true;
    }

    size_t do_l0_size() const override
    {
        return m_refList.size();
    }

    size_t do_l1_size() const override
    {
        return 0;
    }

    vector<size_t> m_refList;
};

//! Implements `Slice` interface for bi-derectional predicted slices.
class BSlice : public Slice
{
public:
    BSlice(const SliceHeader &sliceHeader)
      : Slice(sliceHeader)
    {}

private:
    Slice* DoClone() const override
    {
        return new BSlice(*this);
    }

    void DoCreateRefLists() override;

    const char* DoGetSliceTypeStr() override
    {
        return IsReference() ? " B " : " b ";
    }

    ref_iterator do_l0_begin() const override
    {
        return m_refList0.begin();
    }

    ref_iterator do_l0_end() const override
    {
        return m_refList0.end();
    }

    ref_iterator do_l1_begin() const override
    {
        return m_refList1.begin();
    }

    ref_iterator do_l1_end() const override
    {
        return m_refList1.end();
    }

    bool do_l0_empty() const override
    {
        return m_refList0.empty();
    }

    bool do_l1_empty() const override
    {
        return m_refList1.empty();
    }

    size_t do_l0_size() const override
    {
        return m_refList0.size();
    }

    size_t do_l1_size() const override
    {
        return m_refList1.size();
    }

    vector<size_t> m_refList0;
    vector<size_t> m_refList1;
};

class SeqParameterSet;
class PicParameterSet;
class Picture;

//! This class serves for optimization of the copy constructor of a derived
//! class. The inheritance in this case is not behavioral, but structural. It is
//! assumed that the derived class is an implementor of the `SeqParamSetSrc`,
//! `PicParamSetSrc`, `DPBSrc` and `PicOrderCntSrc` interfaces.
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
            m_slices[i].SetSeqParamSrc(This());
            m_slices[i].SetPicParamSrc(This());
            m_slices[i].SetDPBSrc(This());
            m_slices[i].SetPicOrderCntSrc(This());
        }
    }

    SliceContainer& operator = (const SliceContainer& rhs)
    {
        if (this != &rhs)
        {
            m_slices = rhs.m_slices;
            for (size_t i = 0; m_slices.size() > i; ++i)
            {
                m_slices[i].SetSeqParamSrc(This());
                m_slices[i].SetPicParamSrc(This());
                m_slices[i].SetDPBSrc(This());
                m_slices[i].SetPicOrderCntSrc(This());
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

//! This class serves for optimization of the copy constructor of a derived
//! class. The inheritance in this case is not behavioral, but structural. It is
//! assumed that the derived class is an implementor of the `SeqParamSetSrc`
//! interface.
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
        m_pps.SetParamSrc(This());
    }

    PPSField& operator = (const PPSField &rhs)
    {
        if (this != &rhs)
        {
            m_pps = rhs.m_pps;
            m_pps.SetParamSrc(This());
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
    public SeqParamSetSrc
  , public PicParamSetSrc
  , public DPBSrc
  , public PicOrderCntSrc
  , public SliceContainer<Picture>
  , public PPSField<Picture>
{
    template <class T> friend class PictureContainer;
    friend class SliceContainer<Picture>;

    static const size_t removedPixMaxSize = 32;

public:
    static const size_t pixForOutMaxSize = 32;

    typedef SliceContainer<Picture>::iterator       slice_iterator;
    typedef SliceContainer<Picture>::const_iterator slice_const_iterator;
    typedef SliceContainer<Picture>::Slices         Slices;

    Picture(
        const SeqParamSetSrc *seqParamSrc,
        const PicParamSetSrc *picParamSrc,
        const DecodedPictureSrc *picSrc,
        size_t idx
    );
    Picture(Slice *newSlice, const DecodedPictureSrc *picSrc, size_t idx);

    virtual ~Picture()
    {}

    unsigned int Width() const
    {
        return m_sps.Width();
    }

    unsigned int Height() const
    {
        return m_sps.Height();
    }

    unsigned int PicWidthInMbs() const
    {
        return m_sps.PicWidthInMbs();
    }

    unsigned int FrameHeightInMbs() const
    {
        return m_sps.FrameHeightInMbs();
    }

    unsigned int MaxNumRefFrames() const
    {
        return max_num_ref_frames;
    }

    RC AddSliceFromStream(NALU &nalu, unique_ptr<Slice> *ppRejectedSlice);

    void ResetDpb();

    //! Copy DPB from prevPic and insert it into the copied buffer. The Picture
    //! class and the DPB class have no knowledge of where and how pictures are
    //! stored. Instead they access pictures by their indices using a pointer to
    //! the PictureSrc interface.
    void CopyDpbAndInsert(const Picture &prevPic, size_t prevPicIdx);

    size_t DpbSize() const
    {
        return m_dpb.size();
    }

    const Picture* GetDpbPic(const DPB::value_type& e) const
    {
        return m_picSrc->GetPicture(e.GetPicIdx());
    }

    void CreateRefLists()
    {
        for (size_t i = 0; m_slices.size() > i; ++i)
        {
            m_slices[i].CreateRefLists();
        }
    }

    size_t GetNumSlices() const
    {
        return m_slices.size();
    }

    size_t GetTotalNalusSize() const
    {
        slice_const_iterator it;
        size_t acc = 0;
        for (it = slices_begin(); slices_end() != it; ++it)
        {
            acc += it->GetEbspSize();
        }

        return acc;
    }

    size_t GetPicIdx() const
    {
        return m_picIdx;
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

    const size_t* out_begin() const
    {
        return &m_picsToOutput[0];
    }

    const size_t* out_end() const
    {
        return &m_picsToOutput[0] + m_picsToOutSize;
    }

    const size_t* rm_begin() const
    {
        return &m_removedPics[0];
    }

    const size_t* rm_end() const
    {
        return &m_removedPics[0] + m_removedPicsSize;
    }

    bool IsIDR() const
    {
        return m_isIDR;
    }

    bool IsReference() const
    {
        return m_isReference;
    }

    bool HasMMCO5() const
    {
        return m_hasMMCO5;
    }

    int PicOrderCnt() const
    {
        return m_PicOrderCnt;
    }

    int TopFieldOrderCnt() const
    {
        return m_TopFieldOrderCnt;
    }

    int BottomFieldOrderCnt() const
    {
        return m_BottomFieldOrderCnt;
    }

    unsigned int FrameNum() const
    {
        return frame_num;
    }

    bool MbaffFrameFlag() const
    {
        return m_MbaffFrameFlag;
    }

    bool FieldPicFlag() const
    {
        return field_pic_flag;
    }

    bool BottomFieldFlag() const
    {
        return bottom_field_flag;
    }

#if defined(PRINT_PIC_INFO)
    // for debugging outside MODS, there is no practical reason to define
    // PRINT_PIC_INFO in MODS
    const char* GetPicTypeStr()
    {
        return m_slices.front().GetSliceTypeStr();
    }
#endif

    const SeqParameterSet& GetSPS() const
    {
        return m_sps;
    }

    const PicParameterSet& GetPPS() const
    {
        return m_pps;
    }

private:
    void SetSeqParamSrc(const SeqParamSetSrc *src)
    {
        m_seqParamSrc = src;
    }

    void SetPicParamSrc(const PicParamSetSrc *src)
    {
        m_picParamSrc = src;
    }

    void SetPicSrc(const DecodedPictureSrc *src)
    {
        m_picSrc = src;
    }

    void InitFromFirstSlice();

    const SeqParameterSet& DoGetSPS(size_t id) const override
    {
        MASSERT(id == m_sps.seq_parameter_set_id);
        return m_sps;
    }

    const PicParameterSet& DoGetPPS(size_t id) const override
    {
        MASSERT(id == m_pps.pic_parameter_set_id);
        return m_pps;
    }

    const DPB& DoGetDPB() const override
    {
        return m_dpb;
    }

    int DoGetPicOrderCnt() const override
    {
        return PicOrderCnt();
    }

    int DoGetPicOrderCnt(size_t idx) const override
    {
        const Picture *p = m_picSrc->GetPicture(idx);
        return p->PicOrderCnt();
    }

    unsigned int MaxFrameNum() const
    {
        return m_MaxFrameNum;
    }

    unsigned int PicOderCntLsb() const
    {
        return pic_order_cnt_lsb;
    }

    bool NoOutputOfPriorPicsFlag() const
    {
        return no_output_of_prior_pics_flag;
    }

    bool LongTermReferenceFlag() const
    {
        return long_term_reference_flag;
    }

    bool AdaptiveRefPicMarkingModeFlag() const
    {
        return adaptive_ref_pic_marking_mode_flag;
    }

    const list<DecRefPicMarking>& DecRefPicMarkingList() const
    {
        return dec_ref_pic_marking_list;
    }

    // Picture order code callwlation functions
    void CalcPOC0(const Slice &slice);
    void CalcPOC1(const Slice &slice);
    void CalcPOC2(const Slice &slice);

    // Adaptive memory management functions
    void UnmarkShortTermForRef(const Picture &pic, const DecRefPicMarking &mmco);
    void UnmarkLongTermForRef(const DecRefPicMarking &mmco);
    void UnmarkLongTermForRefByIdx(int long_term_frame_idx);
    void AssignLongTermFrameIdx(const Picture &pic, const DecRefPicMarking &mmco);
    void UpdateMaxLongTermFrameIdx(const DecRefPicMarking &mmco);
    void UnmarkAllForRef();

    bool         m_isReference;
    bool         m_hasMMCO5;
    bool         m_isIDR;
    unsigned int m_MaxFrameNum;
    bool         m_MbaffFrameFlag;

    unsigned int           pic_order_cnt_lsb;
    unsigned int           frame_num;
    unsigned int           max_num_ref_frames;
    bool                   no_output_of_prior_pics_flag;
    bool                   long_term_reference_flag;
    bool                   adaptive_ref_pic_marking_mode_flag;
    bool                   field_pic_flag;
    bool                   bottom_field_flag;
    list<DecRefPicMarking> dec_ref_pic_marking_list;

    const SeqParamSetSrc    *m_seqParamSrc;
    const PicParamSetSrc    *m_picParamSrc;
    const DecodedPictureSrc *m_picSrc;

    SeqParameterSet m_sps;

    // POC type 0
    int m_TopFieldOrderCnt;
    int m_BottomFieldOrderCnt;
    int m_PicOrderCnt;
    int m_PicOrderCntMsb;

    // POC type 1, 2
    unsigned int m_FrameNumOffset;

    DPB            m_dpb;
    unsigned int   m_maxDpbSize;
    size_t         m_picsToOutput[pixForOutMaxSize];
    size_t         m_picsToOutSize;
    size_t         m_removedPics[removedPixMaxSize];
    size_t         m_removedPicsSize;

    size_t         m_picIdx;
};

template <class T>
class PPSContainer
{
protected:
    static const size_t ppsMaxSize = 256;
    PicParameterSet  m_picParameterSets[ppsMaxSize];

    PPSContainer()
    {}

    PPSContainer(const PPSContainer& rhs)
    {
        for (size_t i = 0; ppsMaxSize > i; ++i)
        {
            if (!rhs.m_picParameterSets[i].IsEmpty())
            {
                m_picParameterSets[i] = rhs.m_picParameterSets[i];
                m_picParameterSets[i].SetParamSrc(This());
            }
        }
    }

    PPSContainer& operator = (const PPSContainer &rhs)
    {
        if (this != &rhs)
        {
            for (size_t i = 0; ppsMaxSize > i; ++i)
            {
                m_picParameterSets[i].SetEmpty();
                m_picParameterSets[i].SetParamSrc(0);
            }
            for (size_t i = 0; ppsMaxSize > i; ++i)
            {
                if (!rhs.m_picParameterSets[i].IsEmpty())
                {
                    m_picParameterSets[i] = rhs.m_picParameterSets[i];
                    m_picParameterSets[i].SetParamSrc(This());
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
    typedef vector<Picture>          Pictures;
    typedef Pictures::iterator       iterator;
    typedef Pictures::const_iterator const_iterator;

    Pictures m_pictures;

    PictureContainer()
    {}

    PictureContainer(const PictureContainer& r)
        : m_pictures(r.m_pictures)
    {
        iterator it;
        for (it = m_pictures.begin(); m_pictures.end() != it; ++it)
        {
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
                it->SetSeqParamSrc(This());
                it->SetPicParamSrc(This());
                it->SetPicSrc(This());
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

class Parser :
    public SeqParamSetSrc
  , public PicParamSetSrc
  , public DecodedPictureSrc
  , public PPSContainer<Parser>
  , public PictureContainer<Parser>
{
    typedef Parser ThisClass;

    static const size_t spsMaxSize = 32;

    SeqParameterSet m_seqParameterSets[spsMaxSize];

    bool      m_hasLastRefPicture;
    size_t    m_lastRefPictureIdx = 0;

public:
    typedef PictureContainer<ThisClass>::iterator       pic_iterator;
    typedef PictureContainer<ThisClass>::const_iterator pic_const_iterator;
    typedef PictureContainer<ThisClass>::Pictures       Pictures;

    Parser()
      : m_hasLastRefPicture(false)
    {}

    virtual ~Parser()
    {}

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

    Picture& pic_back()
    {
        return *(m_pictures.end() - 1);
    }

    const Picture& pic_back() const
    {
        return *(m_pictures.end() - 1);
    }

    unsigned int GetWidth() const
    {
        if (m_pictures.empty())
            return 0;

        return m_pictures.front().Width();
    }

    unsigned int GetHeight() const
    {
        if (m_pictures.empty())
            return 0;

        return m_pictures.front().Height();
    }

    unsigned int GetWidthInMb() const
    {
        return m_pictures.front().PicWidthInMbs();
    }

    unsigned int GetHeightInMb() const
    {
        return m_pictures.front().FrameHeightInMbs();
    }

    size_t GetMaxDpbSize() const;

    template <class InputIterator>
    RC ParseStream(InputIterator start, InputIterator finish)
    {
        using namespace BitsIO;

        RC rc;

        NALIArchive<InputIterator> ia(start, finish);
        NALU nalu;

        m_pictures.clear();

        for (size_t i = 0; spsMaxSize > i; ++i)
        {
            m_seqParameterSets[i].SetEmpty();
        }

        for (size_t i = 0; ppsMaxSize > i; ++i)
        {
            m_picParameterSets[i].SetEmpty();
        }
        m_hasLastRefPicture = false;

        while (!ia.IsEOS())
        {
            ia >> nalu;
            if (nalu.IsValid())
            {
                switch (nalu.GetUnitType())
                {
                    case NAL_TYPE_DP_A_SLICE:
                    case NAL_TYPE_DP_B_SLICE:
                    case NAL_TYPE_DP_C_SLICE:
                        return RC::ILWALID_FILE_FORMAT;

                    case NAL_TYPE_SEQ_PARAM:
                    {
                        SeqParameterSet sps;
                        BitIArchive<NALU::RBSPIterator> ia(nalu.RBSPBegin(), nalu.RBSPEnd());
                        ia >> sps;
                        if (spsMaxSize <= sps.seq_parameter_set_id)
                        {
                            return RC::ILWALID_FILE_FORMAT;
                        }

                        // we do not support fields
                        if (!sps.frame_mbs_only_flag)
                        {
                            return RC::ILWALID_FILE_FORMAT;
                        }
                        m_seqParameterSets[sps.seq_parameter_set_id] = sps;
                        break;
                    }
                    case NAL_TYPE_PIC_PARAM:
                    {
                        PicParameterSet pps(this);
                        BitIArchive<NALU::RBSPIterator> ia(nalu.RBSPBegin(), nalu.RBSPEnd());
                        ia >> pps;
                        if (ppsMaxSize <= pps.pic_parameter_set_id)
                        {
                            return RC::ILWALID_FILE_FORMAT;
                        }
                        m_picParameterSets[pps.pic_parameter_set_id] = pps;
                        break;
                    }
                    case NAL_TYPE_NON_IDR_SLICE:
                    case NAL_TYPE_IDR_SLICE:
                    {
                        PictureContainer<ThisClass>::iterator lastPicIt;
                        if (m_pictures.empty())
                        {
                            m_pictures.push_back(Picture(this, this, this, 0));
                        }
                        lastPicIt = m_pictures.begin() + m_pictures.size() - 1;
                        unique_ptr<Slice> rejectedSlice;
                        CHECK_RC(lastPicIt->AddSliceFromStream(
                            nalu,
                            &rejectedSlice
                        ));
                        if (rejectedSlice) // not from this picture
                        {
                            size_t idx = lastPicIt - m_pictures.begin();
                            // the last picture is finished, create a new one
                            if (lastPicIt->IsReference())
                            {
                                m_hasLastRefPicture = true;
                                m_lastRefPictureIdx = idx;
                            }

                            m_pictures.emplace_back(rejectedSlice.release(), this, idx + 1);
                        }

                        break;
                    }
                }
            }
        }

        // Create DPB, reference lists
        Pictures::iterator lwrrPic;
        Pictures::iterator prevPic = m_pictures.begin(); // homage to GCC 2.95
        bool firstIter = true;
        for (lwrrPic = m_pictures.begin(); m_pictures.end() != lwrrPic; ++lwrrPic)
        {
            if (firstIter)
            {
                lwrrPic->ResetDpb();
            }
            else
            {
                lwrrPic->CopyDpbAndInsert(*prevPic, prevPic - m_pictures.begin());
#if defined(PRINT_PIC_INFO)
                // for debugging outside MODS, there is no practical reason to
                // define PRINT_PIC_INFO in MODS
                printf(
                    "%05d(%s)%9d%6d\n",
                    prevPic->PicOrderCnt() / 2,
                    prevPic->GetPicTypeStr(),
                    prevPic->PicOrderCnt(),
                    prevPic->FrameNum()
                );
#endif
            }

            lwrrPic->CreateRefLists();

            prevPic = lwrrPic;
            firstIter = false;
        }

        return rc;
    }

private:
    const SeqParameterSet& DoGetSPS(size_t id) const override
    {
        return m_seqParameterSets[id];
    }

    const PicParameterSet& DoGetPPS(size_t id) const override
    {
        return m_picParameterSets[id];
    }

    Picture* DoGetLastPicture() override
    {
        return m_pictures.empty() ? 0 : &m_pictures.back();
    }

    const Picture* DoGetLastPicture() const override
    {
        return m_pictures.empty() ? 0 : &m_pictures.back();
    }

    Picture* DoGetLastRefPicture() override
    {
        return m_hasLastRefPicture ? &m_pictures[m_lastRefPictureIdx] : 0;
    }

    const Picture* DoGetLastRefPicture() const override
    {
        return m_hasLastRefPicture ? &m_pictures[m_lastRefPictureIdx] : 0;
    }

    Picture* DoGetPicture(size_t idx) override
    {
        return &m_pictures[idx];
    }

    const Picture* DoGetPicture(size_t idx) const override
    {
        return &m_pictures[idx];
    }
};

void CreateWeightMatrices(const SeqParameterSet& sps, const PicParameterSet& pps,
                          unsigned char(*w4x4)[6][4][4], unsigned char(*w8x8)[2][8][8]);
} // namespace H264
