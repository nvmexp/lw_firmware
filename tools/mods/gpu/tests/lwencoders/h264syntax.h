/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014,2016-2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef H264SYNTAX_H
#define H264SYNTAX_H

#include <algorithm>
#include <cstddef>
#include <list>
#include <vector>

#include "core/include/massert.h"
#include "core/include/types.h"
#include "core/include/utility.h"

#include "h26xsyntax.h"
#include "nbits.h"
#include "splitserialization.h"

namespace H264
{

// In this file many classes have attributes without our colwentional m_ prefix.
// These attribute names reflect names in H.264 recommendation to ease match
// between the source code and the H.264 document.

using Vid::NBits;
using BitsIO::SplitSerialization;
using H26X::Ue;
using H26X::Se;
using H26X::Num;
using H26X::BitIArchive;
using H26X::BitOArchive;

enum SliceType
{
    TYPE_P   = 0,
    TYPE_B   = 1,
    TYPE_I   = 2,
    TYPE_SP  = 3,
    TYPE_SI  = 4,
    TYPE2_P  = 5,
    TYPE2_B  = 6,
    TYPE2_I  = 7,
    TYPE2_SP = 8,
    TYPE2_SI = 9
};

enum NALUType
{
    NAL_TYPE_ILWALID         = 0,
    NAL_TYPE_NON_IDR_SLICE   = 1,
    NAL_TYPE_DP_A_SLICE      = 2,
    NAL_TYPE_DP_B_SLICE      = 3,
    NAL_TYPE_DP_C_SLICE      = 4,
    NAL_TYPE_IDR_SLICE       = 5,
    NAL_TYPE_SEI             = 6,
    NAL_TYPE_SEQ_PARAM       = 7,
    NAL_TYPE_PIC_PARAM       = 8,
    NAL_TYPE_ACCESS_UNIT     = 9,
    NAL_TYPE_END_OF_SEQ      = 10,
    NAL_TYPE_END_OF_STREAM   = 11,
    NAL_TYPE_FILLER_DATA     = 12,
    NAL_TYPE_SEQ_EXTENSION   = 13,
    NAL_TYPE_PREFIX          = 14,
    NAL_TYPE_SUBSET_SPS      = 15,
    NAL_TYPE_AUX_PIC_SLICE   = 19,
    NAL_TYPE_CODED_SLICE_EXT = 20
};

enum NalRefIdcPriority
{
    NALU_PRIORITY_HIGHEST    = 3,
    NALU_PRIORITY_HIGH       = 2,
    NALU_PRIORITY_LOW        = 1,
    NALU_PRIORITY_DISPOSABLE = 0
};

//! Loads and saves scaling matrices for quantize coefficients. See 7.3.2.1.1.1,
//! 8.5.8 and 8.5.9 of ITU-T H.264
template <class Archive>
void ScalingList(Archive& sm, unsigned char *scalingList, size_t sizeOfScalingList,
                 bool *useDefaultScalingMatrixFlag)
{
    static const UINT08 ZZ_SCAN[16]  =
    {  0,  1,  4,  8,  5,  2,  3,  6,  9, 12, 13, 10,  7, 11, 14, 15 };

    static const UINT08 ZZ_SCAN8[64] =
    {
         0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5, 12, 19,
        26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28, 35, 42, 49, 56,
        57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31,
        39, 46, 53, 60, 61, 54, 47, 55, 62, 63
    };

    int lastScale = 8;
    int nextScale = 8;
    int scanj;
    Se deltaScale;

    for (size_t j = 0; j < sizeOfScalingList; ++j)
    {
        scanj = (16 == sizeOfScalingList) ? ZZ_SCAN[j] : ZZ_SCAN8[j];
        if (0 != nextScale)
        {
            if (Archive::isSaving)
            {
                deltaScale = scalingList[scanj] - lastScale;
                if (deltaScale > 127)
                    deltaScale = deltaScale - 256;
                else if (deltaScale < -128)
                    deltaScale = deltaScale + 256;
            }

            sm & deltaScale;
            if (Archive::isLoading)
            {
                nextScale = (lastScale + deltaScale + 256) % 256;
            }
            else
            {
                nextScale = scalingList[scanj];
            }
            *useDefaultScalingMatrixFlag = (0 == scanj && 0 == nextScale);
        }
        lastScale = (0 == nextScale) ? lastScale : nextScale;
        scalingList[scanj] = static_cast<unsigned char>(lastScale);
    }
}

//! A syntax element of H.264 stream. See section E.1.2 in Annex E of ITU-T
//! H.264.
struct HrdParameters
{
    Ue       cpb_cnt_minus1;
    NBits<4> bit_rate_scale;
    NBits<4> cpb_size_scale;
    Ue       bit_rate_value_minus1[32];
    Ue       cpb_size_value_minus1[32];
    NBits<1> cbr_flag             [32];
    NBits<5> initial_cpb_removal_delay_length_minus1;
    NBits<5> cpb_removal_delay_length_minus1;
    NBits<5> dpb_output_delay_length_minus1;
    NBits<5> time_offset_length;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & cpb_cnt_minus1;
        sm & bit_rate_scale;
        sm & cpb_size_scale;
        for (size_t i = 0; cpb_cnt_minus1 >= i; ++i)
        {
            sm & bit_rate_value_minus1[i];
            sm & cpb_size_value_minus1[i];
            sm & cbr_flag[i];
        }
        sm & initial_cpb_removal_delay_length_minus1;
        sm & cpb_removal_delay_length_minus1;
        sm & dpb_output_delay_length_minus1;
        sm & time_offset_length;
    }
};

//! A syntax element of H.264 stream. See section E.1.1 in Annex E of ITU-T
//! H.264.
struct VuiSeqParameters
{
    NBits<1>      aspect_ratio_info_present_flag;
    NBits<8>      aspect_ratio_idc;
    NBits<16>     sar_width;
    NBits<16>     sar_height;
    NBits<1>      overscan_info_present_flag;
    NBits<1>      overscan_appropriate_flag;
    NBits<1>      video_signal_type_present_flag;
    NBits<3>      video_format;
    NBits<1>      video_full_range_flag;
    NBits<1>      colour_description_present_flag;
    NBits<8>      colour_primaries;
    NBits<8>      transfer_characteristics;
    NBits<8>      matrix_coefficients;
    NBits<1>      chroma_location_info_present_flag;
    Ue            chroma_sample_loc_type_top_field;
    Ue            chroma_sample_loc_type_bottom_field;
    NBits<1>      timing_info_present_flag;
    NBits<32>     num_units_in_tick;
    NBits<32>     time_scale;
    NBits<1>      fixed_frame_rate_flag;
    NBits<1>      nal_hrd_parameters_present_flag;
    HrdParameters nal_hrd_parameters;
    NBits<1>      vcl_hrd_parameters_present_flag;
    HrdParameters vcl_hrd_parameters;
    NBits<1>      low_delay_hrd_flag;
    NBits<1>      pic_struct_present_flag;
    NBits<1>      bitstream_restriction_flag;
    NBits<1>      motion_vectors_over_pic_boundaries_flag;
    Ue            max_bytes_per_pic_denom;
    Ue            max_bits_per_mb_denom;
    Ue            log2_max_mv_length_vertical;
    Ue            log2_max_mv_length_horizontal;
    Ue            max_num_reorder_frames;
    Ue            max_dec_frame_buffering;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & aspect_ratio_info_present_flag;
        if (aspect_ratio_info_present_flag)
        {
            sm & aspect_ratio_idc;
            if (0xff == aspect_ratio_idc)
            {
                sm & sar_width;
                sm & sar_height;
            }
        }
        sm & overscan_info_present_flag;
        if (overscan_info_present_flag)
        {
            sm & overscan_appropriate_flag;
        }
        sm & video_signal_type_present_flag;
        if (video_signal_type_present_flag)
        {
            sm & video_format;
            sm & video_full_range_flag;
            sm & colour_description_present_flag;
            if (colour_description_present_flag)
            {
                sm & colour_primaries;
                sm & transfer_characteristics;
                sm & matrix_coefficients;
            }
        }
        sm & chroma_location_info_present_flag;
        if (chroma_location_info_present_flag)
        {
            sm & chroma_sample_loc_type_top_field;
            sm & chroma_sample_loc_type_bottom_field;
        }
        sm & timing_info_present_flag;
        if (timing_info_present_flag)
        {
            sm & num_units_in_tick;
            sm & time_scale;
            sm & fixed_frame_rate_flag;
        }
        sm & nal_hrd_parameters_present_flag;
        if (nal_hrd_parameters_present_flag)
        {
            sm & nal_hrd_parameters;
        }
        sm & vcl_hrd_parameters_present_flag;
        if (vcl_hrd_parameters_present_flag)
        {
            sm & vcl_hrd_parameters;
        }
        if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
        {
            sm & low_delay_hrd_flag;
        }
        sm & pic_struct_present_flag;
        sm & bitstream_restriction_flag;
        if (bitstream_restriction_flag)
        {
            sm & motion_vectors_over_pic_boundaries_flag;
            sm & max_bytes_per_pic_denom;
            sm & max_bits_per_mb_denom;
            sm & log2_max_mv_length_horizontal;
            sm & log2_max_mv_length_vertical;
            sm & max_num_reorder_frames;
            sm & max_dec_frame_buffering;
        }
    }
};

enum ProfileIDC
{
    FREXT_CAVLC444 = 44,  //!< YUV 4:4:4/14 "CAVLC 4:4:4"
    BASELINE       = 66,  //!< YUV 4:2:0/8  "Baseline"
    MAIN           = 77,  //!< YUV 4:2:0/8  "Main"
    SCALABLE_BASE  = 83,  //!< YUV 4:2:0/8  "Scalable Baseline"
    SCALABLE_HIGH  = 86,  //!< YUV 4:2:0/8  "Scalable High"
    EXTENDED       = 88,  //!< YUV 4:2:0/8  "Extended"
    FREXT_HP       = 100, //!< YUV 4:2:0/8  "High"
    FREXT_Hi10P    = 110, //!< YUV 4:2:0/10 "High 10"
    FREXT_Hi422    = 122, //!< YUV 4:2:2/10 "High 4:2:2"
    FREXT_Hi444    = 244, //!< YUV 4:4:4/14 "High 4:4:4"
    MVC_HIGH       = 118, //!< YUV 4:2:0/8  "Multiview High"
    STEREO_HIGH    = 128  //!< YUV 4:2:0/8  "Stereo High"
};

enum ColorFormat
{
    YUV400     =  0, //!< Monochrome
    YUV420     =  1, //!< 4:2:0
    YUV422     =  2, //!< 4:2:2
    YUV444     =  3  //!< 4:4:4
};

//! An CRTP interface for retrieving ChromaArrayType syntax element. See the
//! definition of ChromaArrayType in 7.4.2.1.1 of ITU-T H.264. It is used to
//! read PredWeightTable.
template <class T>
class ChromaArrayTypeSrc
{
public:
    UINT32 ChromaArrayType() const
    {
        return This()->ChromaArrayType();
    }

private:
    const T* This() const
    {
        return static_cast<const T*>(this);
    }
};

//! A syntax element of H.264 stream. See section 7.3.2.1.1 of ITU-T H.264.
class SeqParameterSet :
    public ChromaArrayTypeSrc<SeqParameterSet>
{
public:
    static const size_t max_offset_for_ref_frame_size = 256;

    SeqParameterSet()
      : m_empty(true)
      , m_maxDpbSize(0)
    {
        memset(scalingList4x4, 0, sizeof(scalingList4x4));
        memset(scalingList8x8, 0, sizeof(scalingList8x8));
        memset(useDefaultScalingMatrix4x4Flag, 0, sizeof(useDefaultScalingMatrix4x4Flag));
        memset(useDefaultScalingMatrix8x8Flag, 0, sizeof(useDefaultScalingMatrix8x8Flag));
        ExpectedDeltaPerPicOrderCntCycle = 0;
    }

    void SetEmpty()
    {
        m_empty = true;
    }

    void UnsetEmpty()
    {
        m_empty = false;
    }

    //! An H.264 variable as it is defined in equation (7-9) in section
    //! 7.4.2.1.1 of ITU-T H.264
    unsigned int MaxFrameNum() const
    {
        return 1 << (log2_max_frame_num_minus4 + 4);
    }

    //! An H.264 variable as it is defined in equation (7-10) in section
    //! 7.4.2.1.1 of ITU-T H.264
    unsigned int MaxPicOrderCntLsb() const
    {
        return 1 << (log2_max_pic_order_cnt_lsb_minus4 + 4);
    }

    //! Returns DPB size based on the level and VUI information.
    unsigned int DpbSize() const;

    //! Returns maximum DPB size that the decoder has to allocate. It is equal
    //! either to DpbSize() + 1 or max_num_ref_frames + 1. Addition of 1 was
    //! taken from the reference software without analyzing why.
    unsigned int MaxDpbSize()  const
    {
        return m_maxDpbSize;
    }

    //! An H.264 variable as it is defined in equation (7-12) in section
    //! 7.4.2.1.1 of ITU-T H.264
    unsigned int PicWidthInMbs() const
    {
        return pic_width_in_mbs_minus1 + 1;
    }

    //! An H.264 variable as it is defined in equation (7-17) in section
    //! 7.4.2.1.1 of ITU-T H.264
    unsigned int FrameHeightInMbs() const
    {
        return (2 - (frame_mbs_only_flag ? 1 : 0)) * (pic_height_in_map_units_minus1 + 1);
    }

    unsigned int Width() const
    {
        return PicWidthInMbs() * 16;
    }

    unsigned int Height() const
    {
        return FrameHeightInMbs() * 16;
    }

    //! Real picture width that is not necessarily proportional to 16.
    unsigned int CroppedWidth() const
    {
        if (!frame_cropping_flag)
        {
            return Width();
        }

        unsigned int CropUnitX = 1;

        unsigned int chromaArrayType = ChromaArrayType();
        if (0 == chromaArrayType)
        {
            CropUnitX = 1;
        }
        else
        {
            unsigned int SubWidthC = 2;
            if (3 == chromaArrayType)
            {
                SubWidthC = 1;
            }
            CropUnitX = SubWidthC;
        }

        return Width() - (frame_crop_left_offset + frame_crop_right_offset) * CropUnitX;
    }

    //! Real picture height that is not necessarily proportional to 16.
    unsigned int CroppedHeight() const
    {
        if (!frame_cropping_flag)
        {
            return Height();
        }

        unsigned int CropUnitY;

        unsigned int chromaArrayType = ChromaArrayType();
        if (0 == chromaArrayType)
        {
            CropUnitY = 2 - (frame_mbs_only_flag ? 1 : 0);
        }
        else
        {
            unsigned int SubHeightC = 1;
            if (1 == chromaArrayType)
            {
                SubHeightC = 2;
            }
            CropUnitY = SubHeightC * (2 - (frame_mbs_only_flag ? 1 : 0));
        }

        return Height() - (frame_crop_top_offset + frame_crop_bottom_offset) * CropUnitY;
    }

    //! An H.264 variable as it is defined in section 7.4.2.1.1 of ITU-T H.264
    //! (see the description of separate_colour_plane_flag in that section).
    UINT32 ChromaArrayType() const
    {
        if (separate_colour_plane_flag)
        {
            return 0;
        }
        else
        {
            return chroma_format_idc;
        }
    }

private:
    bool         m_empty;
    unsigned int m_maxDpbSize;

public:
    NBits<8>         profile_idc;
    NBits<1>         constraint_set0_flag;
    NBits<1>         constraint_set1_flag;
    NBits<1>         constraint_set2_flag;
    NBits<1>         constraint_set3_flag;
    NBits<1>         constraint_set4_flag;
    NBits<1>         constraint_set5_flag;
    NBits<8>         level_idc;
    Ue               seq_parameter_set_id;
    Ue               chroma_format_idc;

    NBits<1>         seq_scaling_matrix_present_flag;
    NBits<1>         seq_scaling_list_present_flag[12];
    unsigned char    scalingList4x4[6][16];
    unsigned char    scalingList8x8[6][64];
    bool             useDefaultScalingMatrix4x4Flag[6];
    bool             useDefaultScalingMatrix8x8Flag[6];

    Ue               bit_depth_luma_minus8;
    Ue               bit_depth_chroma_minus8;
    NBits<1>         qpprime_y_zero_transform_bypass_flag;
    Ue               log2_max_frame_num_minus4;
    Ue               pic_order_cnt_type;
    Ue               log2_max_pic_order_cnt_lsb_minus4;
    NBits<1>         delta_pic_order_always_zero_flag;
    Se               offset_for_non_ref_pic;
    Se               offset_for_top_to_bottom_field;
    Ue               num_ref_frames_in_pic_order_cnt_cycle;
    Se               offset_for_ref_frame[max_offset_for_ref_frame_size];
    int              ExpectedDeltaPerPicOrderCntCycle;
    Ue               max_num_ref_frames;
    NBits<1>         gaps_in_frame_num_value_allowed_flag;
    Ue               pic_width_in_mbs_minus1;
    Ue               pic_height_in_map_units_minus1;
    NBits<1>         frame_mbs_only_flag;
    NBits<1>         mb_adaptive_frame_field_flag;
    NBits<1>         direct_8x8_inference_flag;
    NBits<1>         frame_cropping_flag;
    Ue               frame_crop_left_offset;
    Ue               frame_crop_right_offset;
    Ue               frame_crop_top_offset;
    Ue               frame_crop_bottom_offset;
    NBits<1>         vui_parameters_present_flag;
    VuiSeqParameters vui_seq_parameters;
    NBits<1>         separate_colour_plane_flag;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        if (Archive::isSaving)
        {
            if (m_empty)
            {
                return;
            }
        }

        sm & profile_idc;
        sm & constraint_set0_flag;
        sm & constraint_set1_flag;
        sm & constraint_set2_flag;
        sm & constraint_set3_flag;
        sm & constraint_set4_flag;
        sm & constraint_set5_flag;

        NBits<2> reserved;
        sm & reserved;

        sm & level_idc;
        sm & seq_parameter_set_id;

        if (Archive::isLoading)
        {
            chroma_format_idc = YUV420;
            bit_depth_luma_minus8   = 0;
            bit_depth_chroma_minus8 = 0;
            qpprime_y_zero_transform_bypass_flag   = 0;
            separate_colour_plane_flag = 0;
        }

        static const UINT32 highProfiles[] =
        {
            FREXT_HP,
            FREXT_Hi10P,
            FREXT_Hi422,
            FREXT_Hi444,
            FREXT_CAVLC444,
            SCALABLE_BASE,
            SCALABLE_HIGH,
            MVC_HIGH,
            STEREO_HIGH
        };
        const UINT32 *start = &highProfiles[0];
        const UINT32 *finish = &highProfiles[0] + NUMELEMS(highProfiles);
        if (std::find(start, finish, static_cast<UINT32>(profile_idc)) != finish)
        {
            sm & chroma_format_idc;
            if (YUV444 == chroma_format_idc)
            {
                sm & separate_colour_plane_flag;
            }
            sm & bit_depth_luma_minus8;
            sm & bit_depth_chroma_minus8;
            sm & qpprime_y_zero_transform_bypass_flag;
            sm & seq_scaling_matrix_present_flag;
            if (seq_scaling_matrix_present_flag)
            {
                for (int i = 0; i < ((YUV444 != chroma_format_idc) ? 8 : 12); ++i)
                {
                    sm & seq_scaling_list_present_flag[i];
                    if (seq_scaling_list_present_flag[i])
                    {
                        if (6 > i)
                        {
                            ScalingList(sm, scalingList4x4[i], 16,
                                &useDefaultScalingMatrix4x4Flag[i]);
                        }
                        else
                        {
                            ScalingList(sm, scalingList8x8[i - 6], 64,
                                &useDefaultScalingMatrix8x8Flag[i - 6]);
                        }
                    }
                }
            }
        }
        sm & log2_max_frame_num_minus4;
        sm & pic_order_cnt_type;
        if (0 == pic_order_cnt_type)
        {
            sm & log2_max_pic_order_cnt_lsb_minus4;
        }
        else if (1 == pic_order_cnt_type)
        {
            sm & delta_pic_order_always_zero_flag;
            sm & offset_for_non_ref_pic;
            sm & offset_for_top_to_bottom_field;
            sm & num_ref_frames_in_pic_order_cnt_cycle;
            ExpectedDeltaPerPicOrderCntCycle = 0;
            for (size_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i)
            {
                sm & offset_for_ref_frame[i];
                ExpectedDeltaPerPicOrderCntCycle += offset_for_ref_frame[i];
            }
        }
        sm & max_num_ref_frames;
        sm & gaps_in_frame_num_value_allowed_flag;
        sm & pic_width_in_mbs_minus1;
        sm & pic_height_in_map_units_minus1;
        sm & frame_mbs_only_flag;
        if (!frame_mbs_only_flag)
        {
            sm & mb_adaptive_frame_field_flag;
        }
        sm & direct_8x8_inference_flag;
        sm & frame_cropping_flag;
        if (frame_cropping_flag)
        {
            sm & frame_crop_left_offset;
            sm & frame_crop_right_offset;
            sm & frame_crop_top_offset;
            sm & frame_crop_bottom_offset;
        }
        sm & vui_parameters_present_flag;
        if (vui_parameters_present_flag)
        {
            sm & vui_seq_parameters;
        }

        if (Archive::isLoading)
        {
            m_empty = false;
            unsigned int dpbSize = DpbSize();
            m_maxDpbSize = 0 == dpbSize ? max_num_ref_frames + 1 : dpbSize + 1;
        }
    }
};

class SeqParameterSet;
class PicParameterSet;

//! An interface to access a collection of SPS.
class SeqParamSetSrc
{
public:
    virtual ~SeqParamSetSrc() {}

    const SeqParameterSet& GetSPS(size_t id) const
    {
        return DoGetSPS(id);
    }

private:
    virtual const SeqParameterSet& DoGetSPS(size_t id) const = 0;
};

//! An interface to access a collection of PPS.
class PicParamSetSrc
{
public:
    virtual ~PicParamSetSrc() {}

    const PicParameterSet& GetPPS(size_t id) const
    {
        return DoGetPPS(id);
    }

private:
    virtual const PicParameterSet& DoGetPPS(size_t id) const = 0;
};

//! This class checks that all left in an input stream is a bit 1 and zero or
//! more 0 bits until the end of a byte. See section 7.3.2.11 of ITU-T H.264.
template <class Archive>
struct MoreRbspDataCheck
{
    static bool Ilwoke(Archive &sm)
    {
        return sm.MoreRbspData();
    }
};

//! A stub for output streams that allows to share the code for input and
//! output.
template <class Archive>
struct MoreRbspDataCheckStub
{
    static bool Ilwoke(Archive &)
    {
        return false;
    }
};

template <bool isLoading, class Archive>
struct MoreRbspDataCheckSelector {};

template <class Archive>
struct MoreRbspDataCheckSelector<true, Archive>
{
    typedef MoreRbspDataCheck<Archive> Type;
};

template <class Archive>
struct MoreRbspDataCheckSelector<false, Archive>
{
    typedef MoreRbspDataCheckStub<Archive> Type;
};

//! Returns false if loading from the stream and the only data left is a
//! stop bit and zero bits up to the end of the byte. See section 7.3.2.11 of
//! ITU-T H.264. It always returns `false` for output streams.
template <class Archive>
bool MoreRbspData(Archive &st)
{
    typedef typename MoreRbspDataCheckSelector<
        Archive::isLoading
      , Archive
      >::Type Check;
    return Check::Ilwoke(st);
}

//! A syntax element of H.264 stream. See section 7.3.2.2 of ITU-T H.264.
class PicParameterSet
{
    template <class T> friend class PPSContainer;
    template <class T> friend class PPSField;
    friend class Picture;

    static const size_t max_num_slice_groups_size = 8;
public:
    PicParameterSet()
      : m_paramSrc(nullptr)
      , m_empty(true)
    {
        Clear();
    }

    explicit PicParameterSet(const SeqParamSetSrc *src)
        : m_paramSrc(src)
        , m_empty(true)
    {
        Clear();
    }

    const SeqParameterSet* GetSPS() const
    {
        if (m_paramSrc)
        {
            return &m_paramSrc->GetSPS(seq_parameter_set_id);
        }

        return nullptr;
    }

    bool IsEmpty() const
    {
        return m_empty;
    }

    void SetEmpty()
    {
        m_empty = true;
    }

    void UnsetEmpty()
    {
        m_empty = false;
    }

private:
    void Clear()
    {
        memset(scalingList4x4, 0, sizeof(scalingList4x4));
        memset(scalingList8x8, 0, sizeof(scalingList8x8));
        memset(useDefaultScalingMatrix4x4Flag, 0, sizeof(useDefaultScalingMatrix4x4Flag));
        memset(useDefaultScalingMatrix8x8Flag, 0, sizeof(useDefaultScalingMatrix8x8Flag));
    }

    void SetParamSrc(const SeqParamSetSrc *src)
    {
        m_paramSrc = src;
    }

    const SeqParamSetSrc *m_paramSrc;
    bool                  m_empty;

public:
    Ue                   pic_parameter_set_id;
    Ue                   seq_parameter_set_id;
    NBits<1>             entropy_coding_mode_flag;
    NBits<1>             transform_8x8_mode_flag;
    NBits<1>             pic_scaling_matrix_present_flag;
    NBits<1>             pic_scaling_list_present_flag[12];
    unsigned char        scalingList4x4[6][16];
    unsigned char        scalingList8x8[6][64];
    bool                 useDefaultScalingMatrix4x4Flag[6];
    bool                 useDefaultScalingMatrix8x8Flag[6];
    NBits<1>             bottom_field_pic_order_in_frame_present_flag;
    Ue                   num_slice_groups_minus1;
    Ue                   slice_group_map_type;
    Ue                   run_length_minus1[max_num_slice_groups_size];
    Ue                   top_left[max_num_slice_groups_size];
    Ue                   bottom_right[max_num_slice_groups_size];
    NBits<1>             slice_group_change_direction_flag;
    Ue                   slice_group_change_rate_minus1;
    Ue                   pic_size_in_map_units_minus1;
    vector<Num<UINT08> > slice_group_id;
    Ue                   num_ref_idx_l0_default_active_minus1;
    Ue                   num_ref_idx_l1_default_active_minus1;
    NBits<1>             weighted_pred_flag;
    NBits<2>             weighted_bipred_idc;
    Se                   pic_init_qp_minus26;
    Se                   pic_init_qs_minus26;
    Se                   chroma_qp_index_offset;
    Se                   cb_qp_index_offset;
    Se                   cr_qp_index_offset;
    Se                   second_chroma_qp_index_offset;
    NBits<1>             deblocking_filter_control_present_flag;
    NBits<1>             constrained_intra_pred_flag;
    NBits<1>             redundant_pic_cnt_present_flag;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        if (Archive::isSaving)
        {
            if (m_empty)
            {
                return;
            }
        }

        sm & pic_parameter_set_id;
        sm & seq_parameter_set_id;
        sm & entropy_coding_mode_flag;
        sm & bottom_field_pic_order_in_frame_present_flag;
        sm & num_slice_groups_minus1;
        if (0 < num_slice_groups_minus1)
        {
            sm & slice_group_map_type;
            if (0 == slice_group_map_type)
            {
                for (size_t iGroup = 0; iGroup <= num_slice_groups_minus1; ++iGroup)
                {
                    sm & run_length_minus1[iGroup];
                }
            }
            else if (2 == slice_group_map_type)
            {
                for (size_t iGroup = 0; iGroup < num_slice_groups_minus1; ++iGroup)
                {
                    sm & top_left[iGroup];
                    sm & bottom_right[iGroup];
                }
            }
            else if (3 == slice_group_map_type || 4 == slice_group_map_type ||
                     5 == slice_group_map_type)
            {
                sm & slice_group_change_direction_flag;
                sm & slice_group_change_rate_minus1;
            }
            else if (6 == slice_group_map_type)
            {
                int numberBitsPerSliceGroupId;
                if (3 < num_slice_groups_minus1)
                {
                    numberBitsPerSliceGroupId = 3;
                }
                else if (1 < num_slice_groups_minus1)
                {
                    numberBitsPerSliceGroupId = 2;
                }
                else
                {
                    numberBitsPerSliceGroupId = 1;
                }
                if (Archive::isSaving)
                {
                    pic_size_in_map_units_minus1 = static_cast<UINT32>(slice_group_id.size() - 1);
                }
                sm & pic_size_in_map_units_minus1;
                if (Archive::isLoading)
                {
                    slice_group_id.resize(pic_size_in_map_units_minus1 + 1);
                }
                for (size_t i = 0; slice_group_id.size() > i; ++i)
                {
                    sm & slice_group_id[i](numberBitsPerSliceGroupId);
                }
            }
        }
        sm & num_ref_idx_l0_default_active_minus1;
        sm & num_ref_idx_l1_default_active_minus1;
        sm & weighted_pred_flag;
        sm & weighted_bipred_idc;
        sm & pic_init_qp_minus26;
        sm & pic_init_qs_minus26;
        sm & chroma_qp_index_offset;
        sm & deblocking_filter_control_present_flag;
        sm & constrained_intra_pred_flag;
        sm & redundant_pic_cnt_present_flag;

        if (Archive::isLoading && !MoreRbspData(sm)) return;

        sm & transform_8x8_mode_flag;
        sm & pic_scaling_matrix_present_flag;
        if (pic_scaling_matrix_present_flag)
        {
            UINT32 chroma_format_idc = GetSPS()->chroma_format_idc;
            size_t maxCount = 6 + (((chroma_format_idc != 3) ? 2 : 6) * (transform_8x8_mode_flag ? 1 : 0));
            for (size_t i = 0; i < maxCount; ++i)
            {
                sm & pic_scaling_list_present_flag[i];
                if (pic_scaling_list_present_flag[i])
                {
                    if (6 > i)
                    {
                        ScalingList(sm, scalingList4x4[i], 16,
                            &useDefaultScalingMatrix4x4Flag[i]);
                    }
                    else
                    {
                        ScalingList(sm, scalingList8x8[i - 6], 64,
                            &useDefaultScalingMatrix8x8Flag[i - 6]);
                    }
                }
            }
        }
        sm & second_chroma_qp_index_offset;

        if (Archive::isLoading)
        {
            m_empty = false;
        }
    }
};

//! A syntax element that holds a single memory management control operation.
//! See 7.3.3.3 and 7.4.3.3 of ITU-T H.264 for a description.
struct DecRefPicMarking
{
    Ue memory_management_control_operation;
    Ue difference_of_pic_nums_minus1;
    Ue long_term_pic_num;
    Ue long_term_frame_idx;
    Ue max_long_term_frame_idx_plus1;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Archive>
    void Load(Archive& sm)
    {
        sm & memory_management_control_operation;
        if (1 == memory_management_control_operation ||
            3 == memory_management_control_operation)
        {
            sm & difference_of_pic_nums_minus1;
        }
        if (2 == memory_management_control_operation)
        {
            sm & long_term_pic_num;
        }
        if (3 == memory_management_control_operation ||
            6 == memory_management_control_operation)
        {
            sm & long_term_frame_idx;
        }
        if (4 == memory_management_control_operation)
        {
            sm & max_long_term_frame_idx_plus1;
        }
    }
};

//! An CRTP interface to access the number of entries in the modified reference
//! picture lists RefPicList0 and RefPicList1. These values correspond to
//! num_ref_idx_l0_active_minus1 + 1 and num_ref_idx_l1_active_minus1 + 1
//! syntax elements respectively. See 7.4.2.2, 7.4.3, 7.4.3.1 and 8.2.4 of ITU-T
//! H.264.
template <class T>
class RefIdxSrc
{
public:
    UINT32 RefIdxL0() const
    {
        return This()->RefIdxL0();
    }

    UINT32 RefIdxL1() const
    {
        return This()->RefIdxL1();
    }

private:
    const T* This() const
    {
        return static_cast<const T*>(this);
    }
};

//! An CRTP interface to access slice type.
template <class T>
class SliceTypeSrc
{
public:
    UINT32 SliceType() const
    {
        return This()->SliceType();
    }

    bool TypeIsP() const
    {
        return SliceTypeIsP(SliceType());
    }

    bool TypeIsB() const
    {
        return SliceTypeIsB(SliceType());
    }

    bool TypeIsI() const
    {
        return SliceTypeIsI(SliceType());
    }

    bool TypeIsSP() const
    {
        return SliceTypeIsSP(SliceType());
    }

    bool TypeIsSI() const
    {
        return SliceTypeIsSI(SliceType());
    }

    static
    bool SliceTypeIsP(UINT32 slice_type)
    {
        return TYPE_P == slice_type % 5;
    }

    static
    bool SliceTypeIsB(UINT32 slice_type)
    {
        return TYPE_B == slice_type % 5;
    }

    static
    bool SliceTypeIsI(UINT32 slice_type)
    {
        return TYPE_I == slice_type % 5;
    }

    static
    bool SliceTypeIsSP(UINT32 slice_type)
    {
        return TYPE_SP == slice_type % 5;
    }

    static
    bool SliceTypeIsSI(UINT32 slice_type)
    {
        return TYPE_SI == slice_type % 5;
    }

private:
    const T* This() const
    {
        return static_cast<const T*>(this);
    }
};

//! A syntax element that holds tables for weighted prediction. See 7.3.3.2,
//! 7.4.3.2 and 8.4.3 of ITU-T H.264.
template <class T>
class PredWeightTable
{
    template <class U>
    friend class PredWeightTableContainer;

    static const size_t max_reference_pictures = 32;

    PredWeightTable(const SliceTypeSrc<T>       *sliceTypeSrc,
                    const RefIdxSrc<T>          *refIdxSrc,
                    const ChromaArrayTypeSrc<T> *chromaArrayTypeSrc)
      : m_sliceTypeSrc(sliceTypeSrc)
      , m_refIdxSrc(refIdxSrc)
      , m_chromaArrayTypeSrc(chromaArrayTypeSrc)
      , m_l0size(0)
      , m_l1size(0)
    {}

    const SliceTypeSrc<T>       *m_sliceTypeSrc;
    const RefIdxSrc<T>          *m_refIdxSrc;
    const ChromaArrayTypeSrc<T> *m_chromaArrayTypeSrc;

    bool SliceTypeIsB() const
    {
        return m_sliceTypeSrc->TypeIsB();
    }

    UINT32 RefIdxL0() const
    {
        return m_refIdxSrc->RefIdxL0();
    }

    UINT32 RefIdxL1() const
    {
        return m_refIdxSrc->RefIdxL1();
    }

    UINT32 ChromaArrayType() const
    {
        return m_chromaArrayTypeSrc->ChromaArrayType();
    }

public:
    Ue     luma_log2_weight_denom;
    Ue     chroma_log2_weight_denom;
    Se     luma_weight_l0     [max_reference_pictures];
    Se     luma_offset_l0     [max_reference_pictures];
    Se     chroma_weight_l0[2][max_reference_pictures];
    Se     chroma_offset_l0[2][max_reference_pictures];
    Se     luma_weight_l1     [max_reference_pictures];
    Se     luma_offset_l1     [max_reference_pictures];
    Se     chroma_weight_l1[2][max_reference_pictures];
    Se     chroma_offset_l1[2][max_reference_pictures];
    size_t m_l0size;
    size_t m_l1size;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Archive>
    void Load(Archive& sm)
    {
        sm & luma_log2_weight_denom;
        if (0 != ChromaArrayType())
        {
            sm & chroma_log2_weight_denom;
        }
        m_l0size = RefIdxL0();
        NBits<1> luma_weight_l0_flag;
        for (size_t i = 0; i < m_l0size; ++i)
        {
            sm & luma_weight_l0_flag;
            if (luma_weight_l0_flag)
            {
                sm & luma_weight_l0[i];
                sm & luma_offset_l0[i];
            }
            else
            {
                luma_weight_l0[i] = 1 << luma_log2_weight_denom;
                luma_offset_l0[i] = 0;
            }
            if (0 != ChromaArrayType())
            {
                NBits<1> chroma_weight_l0_flag;
                sm & chroma_weight_l0_flag;
                for (size_t j = 0; j < 2; ++j)
                {
                    if (chroma_weight_l0_flag)
                    {
                        sm & chroma_weight_l0[j][i];
                        sm & chroma_offset_l0[j][i];
                    }
                    else
                    {
                        chroma_weight_l0[j][i] = 1 << chroma_log2_weight_denom;
                        chroma_offset_l0[j][i] = 0;
                    }
                }
            }
        }
        if (SliceTypeIsB())
        {
            m_l1size = RefIdxL1();
            NBits<1> luma_weight_l1_flag;
            for (size_t i = 0; i < m_l1size; ++i)
            {
                sm & luma_weight_l1_flag;
                if (luma_weight_l1_flag)
                {
                    sm & luma_weight_l1[i];
                    sm & luma_offset_l1[i];
                }
                else
                {
                    luma_weight_l1[i] = 1 << luma_log2_weight_denom;
                    luma_offset_l1[i] = 0;
                }
                if (0 != ChromaArrayType())
                {
                    NBits<1> chroma_weight_l1_flag;
                    sm & chroma_weight_l1_flag;
                    for (size_t j = 0; j < 2; ++j)
                    {
                        if (chroma_weight_l1_flag)
                        {
                            sm & chroma_weight_l1[j][i];
                            sm & chroma_offset_l1[j][i];
                        }
                        else
                        {
                            chroma_weight_l1[j][i] = 1 << chroma_log2_weight_denom;
                            chroma_offset_l1[j][i] = 0;
                        }
                    }
                }
            }
        }
    }
};

//! An elementary modification operation of a reference list. This structure
//! holds a type of the operation and a number that after some operations can be
//! transformed to a picture number. This picture then is inserted into a
//! reference list to a certain position and is removed from all positions after
//! the used for insertion. The modification can insert duplicates to a
//! reference list. x264, for example, uses it to increase the granularity of
//! motion compensation. See 7.3.3.1, 7.4.3.1 and 8.2.4.3 of ITU-T H.264.
struct RefListModifier
{
    Ue modification_of_pic_nums_idc;
    Ue abs_diff_pic_num_minus1;
    Ue long_term_pic_num;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & modification_of_pic_nums_idc;
        if (0 == modification_of_pic_nums_idc || 1 == modification_of_pic_nums_idc)
        {
            sm & abs_diff_pic_num_minus1;
        }
        else if (2 == modification_of_pic_nums_idc)
        {
            sm & long_term_pic_num;
        }
    }
};

//! An elementary modification operation of a reference list for slices not in
//! the base view of a stream with MVC extension of H.264. See H.7.3.3.1.1,
//! H.7.4.3.1.1 and H.8.2.2.3 of ITU-T H.264.
struct RefListMVCModifier
{
    Ue modification_of_pic_nums_idc;
    Ue abs_diff_pic_num_minus1;
    Ue long_term_pic_num;
    Ue abs_diff_view_idx_minus1;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & modification_of_pic_nums_idc;
        if (0 == modification_of_pic_nums_idc || 1 == modification_of_pic_nums_idc)
        {
            sm & abs_diff_pic_num_minus1;
        }
        else if (2 == modification_of_pic_nums_idc)
        {
            sm & long_term_pic_num;
        }
        else if (4 == modification_of_pic_nums_idc || 5 == modification_of_pic_nums_idc)
        {
            sm & abs_diff_view_idx_minus1;
        }
    }
};

//! A list of modifiers for a reference list. See 7.3.3.1, 7.4.3.1 and 8.2.4.3
//! of ITU-T H.264.
template <class T>
class RefPicListModification
{
    template <class U>
    friend class RefPicListModificationContainer;

    explicit RefPicListModification(const SliceTypeSrc<T> *_sliceTypeSrc)
      : sliceTypeSrc(_sliceTypeSrc)
    {}

    UINT32 GetSliceType() const
    {
        return sliceTypeSrc->GetSliceType();
    }

    bool SliceTypeIsI()
    {
        return sliceTypeSrc->TypeIsI();
    }

    bool SliceTypeIsSI()
    {
        return sliceTypeSrc->TypeIsSI();
    }

    bool SliceTypeIsB()
    {
        return sliceTypeSrc->TypeIsB();
    }

    const SliceTypeSrc<T> *sliceTypeSrc;

public:
    NBits<1>   ref_pic_list_modification_flag_l0;
    NBits<1>   ref_pic_list_modification_flag_l1;
    list<RefListModifier> mods[2];

    template <class Archive>
    void Serialize(Archive& sm)
    {
        mods[0].clear();
        mods[1].clear();
        if (!SliceTypeIsI() && !SliceTypeIsSI())
        {
            sm & ref_pic_list_modification_flag_l0;
            if (ref_pic_list_modification_flag_l0)
            {
                RefListModifier mod;
                for (;;)
                {
                    sm & mod;
                    if (3 == mod.modification_of_pic_nums_idc) break;
                    mods[0].push_back(mod);
                }
            }
        }
        if (SliceTypeIsB())
        {
            sm & ref_pic_list_modification_flag_l1;
            if (ref_pic_list_modification_flag_l1)
            {
                RefListModifier mod;
                for (;;)
                {
                    sm & mod;
                    if (3 == mod.modification_of_pic_nums_idc) break;
                    mods[1].push_back(mod);
                }
            }
        }
    }
};

//! A syntax element for SVC extension. See G.14.1 of ITU-T H.264.
struct SvcVuiParametersExtensionEntry
{
    NBits<3>      vui_ext_dependency_id;
    NBits<4>      vui_ext_quality_id;
    NBits<3>      vui_ext_temporal_id;
    NBits<1>      vui_ext_timing_info_present_flag;
    NBits<32>     vui_ext_num_units_in_tick;
    NBits<32>     vui_ext_time_scale;
    NBits<1>      vui_ext_fixed_frame_rate_flag;
    NBits<1>      vui_ext_nal_hrd_parameters_present_flag;
    HrdParameters vui_ext_nal_hrd_parameters;
    NBits<1>      vui_ext_vcl_hrd_parameters_present_flag;
    HrdParameters vui_ext_vcl_hrd_parameters;
    NBits<1>      vui_ext_low_delay_hrd_flag;
    NBits<1>      vui_ext_pic_struct_present_flag;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & vui_ext_dependency_id;
        sm & vui_ext_quality_id;
        sm & vui_ext_temporal_id;
        sm & vui_ext_timing_info_present_flag;
        if (vui_ext_timing_info_present_flag)
        {
            sm & vui_ext_num_units_in_tick;
            sm & vui_ext_time_scale;
            sm & vui_ext_fixed_frame_rate_flag;
        }
        sm & vui_ext_nal_hrd_parameters_present_flag;
        if (vui_ext_nal_hrd_parameters_present_flag)
        {
            sm & vui_ext_nal_hrd_parameters;
        }
        sm & vui_ext_vcl_hrd_parameters_present_flag;
        if (vui_ext_vcl_hrd_parameters_present_flag)
        {
            sm & vui_ext_vcl_hrd_parameters;
        }
        if (vui_ext_nal_hrd_parameters_present_flag ||
            vui_ext_vcl_hrd_parameters_present_flag)
        {
            sm & vui_ext_low_delay_hrd_flag;
        }
        sm & vui_ext_pic_struct_present_flag;
    }
};

//! A syntax element for SVC extension. See G.14.1 of ITU-T H.264.
class SvcVuiParametersExtension
{
public:
    template <class Archive>
    void Serialize(Archive& sm)
    {
        Ue vui_ext_num_entries_minus1;
        if (Archive::isSaving)
        {
            vui_ext_num_entries_minus1 = static_cast<UINT32>(entries.size() - 1);
        }
        sm & vui_ext_num_entries_minus1;
        if (Archive::isLoading)
        {
            entries.resize(vui_ext_num_entries_minus1 + 1);
        }
        for (size_t i = 0; entries.size() > i; ++i)
        {
            sm & entries[i];
        }
    }

    vector<SvcVuiParametersExtensionEntry> entries;
};

//! A syntax element for MVC extension. See H.14.1 of ITU-T H.264.
class MvcVuiParametersExtensionEntry
{
public:
    NBits<3>      vui_mvc_temporal_id;
    vector<Ue>    vui_mvc_view_id;
    NBits<1>      vui_mvc_timing_info_present_flag;
    NBits<32>     vui_mvc_num_units_in_tick;
    NBits<32>     vui_mvc_time_scale;
    NBits<1>      vui_mvc_fixed_frame_rate_flag;
    NBits<1>      vui_mvc_nal_hrd_parameters_present_flag;
    HrdParameters vui_mvc_nal_hrd_parameters;
    NBits<1>      vui_mvc_vcl_hrd_parameters_present_flag;
    HrdParameters vui_mvc_vcl_hrd_parameters;
    NBits<1>      vui_mvc_low_delay_hrd_flag;
    NBits<1>      vui_mvc_pic_struct_present_flag;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & vui_mvc_temporal_id;
        Ue vui_mvc_num_target_output_views_minus1;
        if (Archive::isSaving)
        {
            vui_mvc_num_target_output_views_minus1 = static_cast<UINT32>(vui_mvc_view_id.size() - 1);
        }
        sm & vui_mvc_num_target_output_views_minus1;
        if (Archive::isLoading)
        {
            vui_mvc_view_id.resize(vui_mvc_num_target_output_views_minus1 + 1);
        }
        for (size_t i = 0; vui_mvc_view_id.size() > i; ++i)
        {
            sm & vui_mvc_view_id[i];
        }
        sm & vui_mvc_timing_info_present_flag;
        if (vui_mvc_timing_info_present_flag)
        {
            sm & vui_mvc_num_units_in_tick;
            sm & vui_mvc_time_scale;
            sm & vui_mvc_fixed_frame_rate_flag;
        }
        sm & vui_mvc_nal_hrd_parameters_present_flag;
        if (vui_mvc_nal_hrd_parameters_present_flag)
        {
            sm & vui_mvc_nal_hrd_parameters;
        }
        sm & vui_mvc_vcl_hrd_parameters_present_flag;
        if (vui_mvc_vcl_hrd_parameters_present_flag)
        {
            sm & vui_mvc_vcl_hrd_parameters;
        }
        if (vui_mvc_nal_hrd_parameters_present_flag ||
            vui_mvc_vcl_hrd_parameters_present_flag)
        {
            sm & vui_mvc_low_delay_hrd_flag;
        }
        sm & vui_mvc_pic_struct_present_flag;
    }
};

//! A syntax element for MVC extension. See H.14.1 of ITU-T H.264.
class MvcVuiParametersExtension
{
public:
    template <class Archive>
    void Serialize(Archive& sm)
    {
        Ue vui_mvc_num_ops_minus1;
        if (Archive::isSaving)
        {
            vui_mvc_num_ops_minus1 = static_cast<UINT32>(entries.size() - 1);
        }
        sm & vui_mvc_num_ops_minus1;
        if (Archive::isLoading)
        {
            entries.resize(vui_mvc_num_ops_minus1 + 1);
        }
        for (size_t i = 0; entries.size() > i; ++i)
        {
            sm & entries[i];
        }
    }

    vector<MvcVuiParametersExtensionEntry> entries;
};

//! A SVC extension for sequence parameter sets. See G.7.3.2.1.4 of ITU-T H.264.
template <class T>
class SeqParameterSetSvcExtension
{
    template <class U>
    friend class SeqParameterSetSvcExtensionContainer;

    SeqParameterSetSvcExtension(const ChromaArrayTypeSrc<T> *chromaArrayTypeSrc)
      : m_chromaArrayTypeSrc(chromaArrayTypeSrc)
    {}

    const ChromaArrayTypeSrc<T> *m_chromaArrayTypeSrc;

    UINT32 ChromaArrayType() const
    {
        return m_chromaArrayTypeSrc->ChromaArrayType();
    }

public:
    NBits<1> inter_layer_deblocking_filter_control_present_flag;
    NBits<2> extended_spatial_scalability_idc;
    NBits<1> chroma_phase_x_plus1_flag;
    NBits<2> chroma_phase_y_plus1;
    NBits<1> seq_ref_layer_chroma_phase_x_plus1_flag;
    NBits<2> seq_ref_layer_chroma_phase_y_plus1;
    Se       seq_scaled_ref_layer_left_offset;
    Se       seq_scaled_ref_layer_top_offset;
    Se       seq_scaled_ref_layer_right_offset;
    Se       seq_scaled_ref_layer_bottom_offset;
    NBits<1> seq_tcoeff_level_prediction_flag;
    NBits<1> adaptive_tcoeff_level_prediction_flag;
    NBits<1> slice_header_restriction_flag;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & inter_layer_deblocking_filter_control_present_flag;
        sm & extended_spatial_scalability_idc;
        if (1 == ChromaArrayType() || 2 == ChromaArrayType())
        {
            sm & chroma_phase_x_plus1_flag;
        }
        if (1 == ChromaArrayType())
        {
            sm & chroma_phase_y_plus1;
        }
        if (1 == extended_spatial_scalability_idc)
        {
            if (0 < ChromaArrayType())
            {
                sm & seq_ref_layer_chroma_phase_x_plus1_flag;
                sm & seq_ref_layer_chroma_phase_y_plus1;
            }
            sm & seq_scaled_ref_layer_left_offset;
            sm & seq_scaled_ref_layer_top_offset;
            sm & seq_scaled_ref_layer_right_offset;
            sm & seq_scaled_ref_layer_bottom_offset;
        }
        sm & seq_tcoeff_level_prediction_flag;
        if (seq_tcoeff_level_prediction_flag)
        {
            sm & adaptive_tcoeff_level_prediction_flag;
        }
        sm & slice_header_restriction_flag;
    }
};

//! This class serves for optimization of the copy constructor of a derived
//! class. The inheritance in this case is not behavioral, but structural. It is
//! assumed that the derived class is an implementor of the `ChromaArrayTypeSrc`
//! interface. During copy and assignment the outer class that aggregates
//! `seq_parameter_set_svc_extension` through inheritance has to switch the
//! implementor of `ChromaArrayTypeSrc`. This is the purpose of this class.
template <class T>
class SeqParameterSetSvcExtensionContainer
{
protected:
    SeqParameterSetSvcExtensionContainer(const T *srcImpl)
      : seq_parameter_set_svc_extension(srcImpl)
    {}

    SeqParameterSetSvcExtensionContainer(const SeqParameterSetSvcExtensionContainer& rhs)
        : seq_parameter_set_svc_extension(rhs.seq_parameter_set_svc_extension)
    {
        seq_parameter_set_svc_extension.m_chromaArrayTypeSrc = This();
    }

    SeqParameterSetSvcExtensionContainer& operator = (const SeqParameterSetSvcExtensionContainer& rhs)
    {
        if (this != &rhs)
        {
            seq_parameter_set_svc_extension = rhs.seq_parameter_set_svc_extension;
            seq_parameter_set_svc_extension.m_chromaArrayTypeSrc = This();
        }

        return *this;
    }

    SeqParameterSetSvcExtension<T> seq_parameter_set_svc_extension;

private:
    const T* This() const
    {
        return static_cast<const T*>(this);
    }
};

//! A part of `seq_parameter_set_mvc_extension` H.264 MVC extension. See
//! H.7.3.2.1.4 of ITU-T H.264.
class AnchorRefs
{
public:
    vector<Ue> anchor_ref_l0;
    vector<Ue> anchor_ref_l1;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        Ue num_anchor_refs_l0;
        if (Archive::isSaving)
        {
            num_anchor_refs_l0 = static_cast<UINT32>(anchor_ref_l0.size());
        }
        sm & num_anchor_refs_l0;
        if (Archive::isLoading)
        {
            anchor_ref_l0.resize(num_anchor_refs_l0);
        }
        for (size_t i = 0; anchor_ref_l0.size() > i; ++i)
        {
            sm & anchor_ref_l0[i];
        }

        Ue num_anchor_refs_l1;
        if (Archive::isSaving)
        {
            num_anchor_refs_l1 = static_cast<UINT32>(anchor_ref_l1.size());
        }
        sm & num_anchor_refs_l1;
        if (Archive::isLoading)
        {
            anchor_ref_l1.resize(num_anchor_refs_l1);
        }
        for (size_t i = 0; anchor_ref_l1.size() > i; ++i)
        {
            sm & anchor_ref_l1[i];
        }
  }
};

//! A part of `seq_parameter_set_mvc_extension` H.264 MVC extension. See
//! H.7.3.2.1.4 of ITU-T H.264.
class NonAnchorRefs
{
public:
    vector<Ue> non_anchor_ref_l0;
    vector<Ue> non_anchor_ref_l1;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        Ue num_non_anchor_refs_l0;
        if (Archive::isSaving)
        {
            num_non_anchor_refs_l0 = static_cast<UINT32>(non_anchor_ref_l0.size());
        }
        sm & num_non_anchor_refs_l0;
        if (Archive::isLoading)
        {
            non_anchor_ref_l0.resize(num_non_anchor_refs_l0);
        }
        for (size_t i = 0; non_anchor_ref_l0.size() > i; ++i)
        {
            sm & non_anchor_ref_l0[i];
        }

        Ue num_non_anchor_refs_l1;
        if (Archive::isSaving)
        {
            num_non_anchor_refs_l1 = static_cast<UINT32>(non_anchor_ref_l1.size());
        }
        sm & num_non_anchor_refs_l1;
        if (Archive::isLoading)
        {
            non_anchor_ref_l1.resize(num_non_anchor_refs_l1);
        }
        for (size_t i = 0; non_anchor_ref_l1.size() > i; ++i)
        {
            sm & non_anchor_ref_l1[i];
        }
  }
};

//! A part of `seq_parameter_set_mvc_extension` H.264 MVC extension. See
//! H.7.3.2.1.4 of ITU-T H.264.
class ApplicableOp
{
public:
    NBits<3>   applicable_op_temporal_id;
    vector<Ue> applicable_op_target_view_id;
    Ue         applicable_op_num_views_minus1;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & applicable_op_temporal_id;
        Ue applicable_op_num_target_views_minus1;
        if (Archive::isSaving)
        {
            applicable_op_num_target_views_minus1 = static_cast<UINT32>(
                applicable_op_target_view_id.size() - 1
            );
        }
        sm & applicable_op_num_target_views_minus1;
        if (Archive::isLoading)
        {
            applicable_op_target_view_id.resize(applicable_op_num_target_views_minus1 + 1);
        }
        for (size_t i = 0; applicable_op_target_view_id.size() > i; ++i)
        {
            sm & applicable_op_target_view_id[i];
        }
        sm & applicable_op_num_views_minus1;
    }
};

//! A part of `seq_parameter_set_mvc_extension` H.264 MVC extension. See
//! H.7.3.2.1.4 of ITU-T H.264.
class LevelValueSignalled
{
public:
    NBits<8>             level_idc;
    vector<ApplicableOp> applicable_op;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & level_idc;
        Ue num_applicable_ops_minus1;
        if (Archive::isSaving)
        {
            num_applicable_ops_minus1 = static_cast<UINT32>(
                applicable_op.size() - 1
            );
        }
        sm & num_applicable_ops_minus1;
        if (Archive::isLoading)
        {
            applicable_op.resize(num_applicable_ops_minus1 + 1);
        }
        for (size_t i = 0; applicable_op.size() > i; ++i)
        {
            sm & applicable_op[i];
        }
    }
};

//! This is an implementation of `seq_parameter_set_mvc_extension` syntax
//! element of H.264 MVC extension. See H.7.3.2.1.4 of ITU-T H.264.
class SeqParameterSetMvcExtension
{
public:
    vector<Ue>                  view_id;
    vector<AnchorRefs>          anchor_refs;
    vector<NonAnchorRefs>       non_anchor_refs;
    vector<LevelValueSignalled> level_values_signalled;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        Ue num_views_minus1;
        if (Archive::isSaving)
        {
            num_views_minus1 = static_cast<UINT32>(view_id.size() - 1);
            MASSERT(view_id.size() == anchor_refs.size());
            MASSERT(view_id.size() == non_anchor_refs.size());
        }
        sm & num_views_minus1;
        if (Archive::isLoading)
        {
            view_id.resize(num_views_minus1 + 1);
            anchor_refs.resize(num_views_minus1 + 1);
            non_anchor_refs.resize(num_views_minus1 + 1);
        }
        for (size_t i = 0; num_views_minus1 >= i; ++i)
        {
            sm & view_id[i];
        }
        for (size_t i = 1; num_views_minus1 >= i; ++i)
        {
            sm & anchor_refs[i];
        }
        for (size_t i = 1; num_views_minus1 >= i; ++i)
        {
            sm & non_anchor_refs[i];
        }

        Ue num_level_values_signalled_minus1;
        if (Archive::isSaving)
        {
            num_level_values_signalled_minus1 = static_cast<UINT32>(
                level_values_signalled.size() - 1
            );
        }
        sm & num_level_values_signalled_minus1;
        if (Archive::isLoading)
        {
            level_values_signalled.resize(num_level_values_signalled_minus1 + 1);
        }
        for (size_t i = 0; level_values_signalled.size() > i; ++i)
        {
            sm & level_values_signalled[i];
        }
    }
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4355)
#endif
//! Subset sequence parameters set syntax element. It is used for SVC and MVC
//! extensions of H.264. See 7.3.2.1.3 of ITU-T H.264.
class SubsetSeqParameterSet :
    public    ChromaArrayTypeSrc<SubsetSeqParameterSet>
  , protected SeqParameterSetSvcExtensionContainer<SubsetSeqParameterSet>
{
public:
    SubsetSeqParameterSet()
      : SeqParameterSetSvcExtensionContainer<SubsetSeqParameterSet>(this)
    {}

    UINT32 ChromaArrayType() const
    {
        return seq_parameter_set_data.ChromaArrayType();
    }

    SeqParameterSet             seq_parameter_set_data;
    NBits<1>                    svc_vui_parameters_present_flag;
    SvcVuiParametersExtension   svc_vui_parameters_extension;
    SeqParameterSetMvcExtension seq_parameter_set_mvc_extension;
    NBits<1>                    mvc_vui_parameters_present_flag;
    SvcVuiParametersExtension   mvc_vui_parameters_extension;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & seq_parameter_set_data;
        if (SCALABLE_BASE == seq_parameter_set_data.profile_idc ||
            SCALABLE_HIGH == seq_parameter_set_data.profile_idc)
        {
            sm & seq_parameter_set_svc_extension;
            sm & svc_vui_parameters_present_flag;
            if (svc_vui_parameters_present_flag)
            {
                sm & svc_vui_parameters_extension;
            }
        }
        else if (MVC_HIGH    == seq_parameter_set_data.profile_idc ||
                 STEREO_HIGH == seq_parameter_set_data.profile_idc)
        {
            NBits<1> bit_equal_to_one(true);
            sm & bit_equal_to_one;
            sm & seq_parameter_set_mvc_extension;
            sm & mvc_vui_parameters_present_flag;
            if (mvc_vui_parameters_present_flag)
            {
                sm & mvc_vui_parameters_extension;
            }
        }
        NBits<1> additional_extension2_flag(false);
        sm & additional_extension2_flag;
        if (Archive::isLoading && additional_extension2_flag)
        {
            while (MoreRbspData(sm))
            {
                sm & additional_extension2_flag;
            }
        }
    }
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
//! A list of modifiers for a reference list for a picture not in the base view
//! in a MVC extension H.264 stream.
template <class T>
class RefPicListMVCModification
{
   template <class U>
   friend class RefPicListModificationContainer;

    RefPicListMVCModification(const SliceTypeSrc<T> *_sliceTypeSrc)
      : sliceTypeSrc(_sliceTypeSrc)
    {}

    UINT32 GetSliceType() const
    {
        return sliceTypeSrc->GetSliceType();
    }

    bool SliceTypeIsI()
    {
        return sliceTypeSrc->TypeIsI();
    }

    bool SliceTypeIsSI()
    {
        return sliceTypeSrc->TypeIsSI();
    }

    bool SliceTypeIsB()
    {
        return sliceTypeSrc->TypeIsB();
    }

    const SliceTypeSrc<T> *sliceTypeSrc;

public:
    NBits<1>   ref_pic_list_modification_flag_l0;
    NBits<1>   ref_pic_list_modification_flag_l1;
    list<RefListMVCModifier> mods[2];

    template <class Archive>
    void Serialize(Archive& sm)
    {
        mods[0].clear();
        mods[1].clear();
        if (!SliceTypeIsI() && !SliceTypeIsSI())
        {
            sm & ref_pic_list_modification_flag_l0;
            if (ref_pic_list_modification_flag_l0)
            {
                mods[0].clear();
                RefListMVCModifier mod;
                for (;;)
                {
                    sm & mod;
                    if (3 == mod.modification_of_pic_nums_idc) break;
                    mods[0].push_back(mod);
                }
            }
        }
        if (SliceTypeIsB())
        {
            sm & ref_pic_list_modification_flag_l1;
            if (ref_pic_list_modification_flag_l1)
            {
                mods[1].clear();
                RefListMVCModifier mod;
                for (;;)
                {
                    sm & mod;
                    if (3 == mod.modification_of_pic_nums_idc) break;
                    mods[1].push_back(mod);
                }
            }
        }
    }
};

//! An extension of of NALU header for SVC extension of H.264. See 7.3.1 and
//! G.7.3.1.1 of of ITU-T H.264.
struct NaluHeaderSvcExtension
{
public:
    NBits<1> idr_flag;
    NBits<6> priority_id;
    NBits<1> no_inter_layer_pred_flag;
    NBits<3> dependency_id;
    NBits<4> quality_id;
    NBits<3> temporal_id;
    NBits<1> use_ref_base_pic_flag;
    NBits<1> discardable_flag;
    NBits<1> output_flag;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & idr_flag;
        sm & priority_id;
        sm & no_inter_layer_pred_flag;
        sm & dependency_id;
        sm & quality_id;
        sm & temporal_id;
        sm & use_ref_base_pic_flag;
        sm & discardable_flag;
        sm & output_flag;
        NBits<2> reserved_three_2bits(3);
        sm & reserved_three_2bits;
    }
};

//! An extension of of NALU header for MVC extension of H.264. See 7.3.1 and
//! G.7.3.1.1 of of ITU-T H.264.
struct NaluHeaderMvcExtension
{
public:
    NBits<1>  non_idr_flag;
    NBits<6>  priority_id;
    NBits<10> view_id;
    NBits<3>  temporal_id;
    NBits<1>  anchor_pic_flag;
    NBits<1>  inter_view_flag;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & non_idr_flag;
        sm & priority_id;
        sm & view_id;
        sm & temporal_id;
        sm & anchor_pic_flag;
        sm & inter_view_flag;

        NBits<1> reserved_one_bit(1);
        sm & reserved_one_bit;
    }
};

//! A holder of NALU SVC and MVC extensions that allocates these extensions in
//! the heap id they are required. It's a small optimization, since in most of
//! the cases NALUs don't have these extensions.
class NaluHeaderExtension
{
public:
    NaluHeaderExtension()
      : m_svcExt(nullptr)
      , m_mvcExt(nullptr)
    {}

    NaluHeaderExtension(const NaluHeaderExtension &ext)
    {
        if (nullptr != ext.m_svcExt)
        {
            m_svcExt = new NaluHeaderSvcExtension(*ext.m_svcExt);
        }
        else
        {
            m_svcExt = nullptr;
        }

        if (nullptr != ext.m_mvcExt)
        {
            m_mvcExt = new NaluHeaderMvcExtension(*ext.m_mvcExt);
        }
        else
        {
            m_mvcExt = nullptr;
        }
    }

    NaluHeaderExtension& operator=(const NaluHeaderExtension &ext)
    {
        if (this != &ext)
        {
            if (nullptr == m_svcExt)
            {
                if (nullptr != ext.m_svcExt)
                {
                    m_svcExt = new NaluHeaderSvcExtension;
                    *m_svcExt = *ext.m_svcExt;
                }
            }
            else
            {
                if (nullptr != ext.m_svcExt)
                {
                    *m_svcExt = *ext.m_svcExt;
                }
                else
                {
                    delete m_svcExt;
                    m_svcExt = nullptr;
                }
            }

            if (nullptr == m_mvcExt)
            {
                if (nullptr != ext.m_mvcExt)
                {
                    m_mvcExt = new NaluHeaderMvcExtension;
                    *m_mvcExt = *ext.m_mvcExt;
                }
            }
            else
            {
                if (nullptr != ext.m_mvcExt)
                {
                    *m_mvcExt = *ext.m_mvcExt;
                }
                else
                {
                    delete m_mvcExt;
                    m_mvcExt = nullptr;
                }
            }
        }

        return *this;
    }

    ~NaluHeaderExtension()
    {
        if (nullptr != m_svcExt) delete m_svcExt;
        if (nullptr != m_mvcExt) delete m_mvcExt;
    }

    NaluHeaderSvcExtension& SvcExt()
    {
        if (nullptr == m_svcExt)
        {
            m_svcExt = new NaluHeaderSvcExtension;
        }
        return *m_svcExt;
    }

    NaluHeaderMvcExtension& MvcExt()
    {
        if (nullptr == m_mvcExt)
        {
            m_mvcExt = new NaluHeaderMvcExtension;
        }
        return *m_mvcExt;
    }

private:
    NaluHeaderSvcExtension *m_svcExt;
    NaluHeaderMvcExtension *m_mvcExt;
};

class NALUData
{
public:
    typedef vector<UINT08>::const_iterator RBSPIterator;
    typedef vector<UINT08>::const_iterator EBSPIterator;
protected:
    vector<UINT08> m_rbspData;
    vector<UINT08> m_ebspData;
};

//! An H.264 NAL (Network Abstraction Layer) unit of data.
class NALU : public NALUData
{
    static const unsigned int startCodeSize = 4;

    bool           m_valid;
    size_t         m_offset;

    NBits<2>       nal_ref_idc;
    NBits<5>       nal_unit_type;
    NBits<1>       svc_extension_flag;

    NaluHeaderExtension m_headerExtension;

public:
    NALU()
      : m_valid(false)
      , m_offset(0)
    {}

    template <class InputIterator>
    void InitFromEBSP(InputIterator start, InputIterator finish, size_t offset)
    {
        m_offset = offset;

        // add start code first
        m_ebspData.assign(3, 0);
        m_ebspData.insert(m_ebspData.end(), 1, 1);

        m_ebspData.insert(m_ebspData.end(), start, finish);

        // Because of the existence of SVC and MVC extensions, we need first to
        // read some information from the raw NALU before we can remove emulation
        // prevention bytes.
        BitIArchive<vector<UINT08>::const_iterator> ia(
            m_ebspData.begin() + startCodeSize,
            m_ebspData.end()
        );

        NBits<1> isValidIlwerted;
        ia >> isValidIlwerted;
        if (!isValidIlwerted)
        {
            m_valid = true;
            ia >> nal_ref_idc >> nal_unit_type;
            if (HasExtension())
            {
                ia >> svc_extension_flag;
                if (svc_extension_flag)
                    ia >> m_headerExtension.SvcExt();
                else
                    ia >> m_headerExtension.MvcExt();
            }
            size_t rbspStart = static_cast<size_t>(ia.GetLwrrentOffset() / 8);
            m_rbspData.assign(m_ebspData.begin() + rbspStart + startCodeSize, m_ebspData.end());
            if (m_rbspData.size() > 3)
            {
                // remove emulation_prevention_three_byte, see 7.3.1 of ITU-T H.264
                UINT08 *finish = &m_rbspData[0] + m_rbspData.size() - 2;
                for (UINT08 *p = &m_rbspData[0]; p < finish; ++p)
                {
                    if (p[0] == 0 && p[1] == 0 && p[2] == 3)
                    {
                        ++p;
                        memmove(p + 1, p + 2, finish - p);
                        --finish;
                    }
                }
                m_rbspData.resize(finish + 2 - &m_rbspData[0]);
            }
        }
        else
        {
            m_valid = false;
        }
    }

    size_t GetOffset() const
    {
        return m_offset;
    }

    const UINT08* GetEbspFromStartCode() const
    {
        return &m_ebspData[0];
    }

    const UINT08* GetEbspPayload() const
    {
        return &m_ebspData[0] + startCodeSize;
    }

    size_t GetEbspSizeWithStart() const
    {
        return m_ebspData.size();
    }

    size_t GetEbspSize() const
    {
        return m_ebspData.size() - startCodeSize;
    }

    RBSPIterator RBSPBegin() const
    {
        return m_rbspData.begin();
    }

    RBSPIterator RBSPEnd() const
    {
        return m_rbspData.end();
    }

    bool HasExtension() const
    {
        return (NAL_TYPE_PREFIX == nal_unit_type) || (NAL_TYPE_CODED_SLICE_EXT == nal_unit_type);
    }

    bool IsSvcExtension() const
    {
        return svc_extension_flag;
    }

    const NaluHeaderExtension& GetExtension() const
    {
        return m_headerExtension;
    }

    UINT08 GetRefIdc() const
    {
        return static_cast<UINT08>(nal_ref_idc);
    }

    UINT08 GetUnitType() const
    {
        return static_cast<UINT08>(nal_unit_type);
    }

    bool IsValid() const
    {
        return m_valid;
    }

    void CreateDataFromSPS(const SeqParameterSet &sps);
    void CreateDataFromPPS(const PicParameterSet &pps);
    void CreateFromSubsetSPS(const SubsetSeqParameterSet &subsetSps);

private:
    // does not support NALU types 14 and 20 yet
    template <class T>
    void CreateDataFrom(const T &t, NalRefIdcPriority refIdc, NALUType naluType);
};
} // namespace H264

namespace BitsIO
{
template <>
struct SerializationTraits<H264::NALU>
{
    static constexpr bool isPrimitive = true;
};
}

namespace H264
{
//! This class serves for optimization of the copy constructor of a derived
//! class. The inheritance in this case is not behavioral, but structural. It is
//! assumed that the derived class is an implementor of the `SliceTypeSrc`,
//! `RefIdxSrc` and `ChromaArrayTypeSrc` interfaces. During copy and assignment
//! the outer class that aggregates `pred_weight_table` through inheritance has
//! to switch the implementor of `SliceTypeSrc`, `RefIdxSrc` and
//! `ChromaArrayTypeSrc`. This is the purpose of this class. Ideally the
//! inheritance has to be private, however this would require a lot of work to
//! overcome poor compilers shortcomings.
template <class T>
class PredWeightTableContainer
{
protected:
    PredWeightTableContainer(const T *srcImpl)
      : pred_weight_table(srcImpl, srcImpl, srcImpl)
    {}

    PredWeightTableContainer(const PredWeightTableContainer& rhs)
        : pred_weight_table(rhs.pred_weight_table)
    {
        pred_weight_table.m_sliceTypeSrc = This();
        pred_weight_table.m_refIdxSrc = This();
        pred_weight_table.m_chromaArrayTypeSrc = This();
    }

    PredWeightTableContainer& operator = (const PredWeightTableContainer& rhs)
    {
        if (this != &rhs)
        {
            pred_weight_table = rhs.pred_weight_table;
            pred_weight_table.m_sliceTypeSrc = This();
            pred_weight_table.m_refIdxSrc = This();
            pred_weight_table.m_chromaArrayTypeSrc = This();
        }

        return *this;
    }

    PredWeightTable<T> pred_weight_table;

private:
    const T* This() const
    {
        return static_cast<const T*>(this);
    }
};

//! This class serves for optimization of the copy constructor of a derived
//! class. The inheritance in this case is not behavioral, but structural. It is
//! assumed that the derived class is an implementor of the `SliceTypeSrc`
//! interface. During copy and assignment the outer class that aggregates
//! `ref_pic_list_modification` and `ref_pic_list_mvc_modification` through
//! inheritance has to switch the implementor of `SliceTypeSrc`. This is the
//! purpose of this class. Ideally the inheritance has to be private, however
//! this would require a lot of work to overcome poor compilers shortcomings.
template <class T>
class RefPicListModificationContainer
{
protected:
    RefPicListModificationContainer(const T *srcImpl)
      : ref_pic_list_modification(srcImpl)
      , ref_pic_list_mvc_modification(srcImpl)
    {}

    RefPicListModificationContainer(const RefPicListModificationContainer& rhs)
      : ref_pic_list_modification(rhs.ref_pic_list_modification)
      , ref_pic_list_mvc_modification(rhs.ref_pic_list_mvc_modification)
    {
        ref_pic_list_modification.sliceTypeSrc = This();
        ref_pic_list_mvc_modification.sliceTypeSrc = This();
    }

    RefPicListModificationContainer& operator = (const RefPicListModificationContainer& rhs)
    {
        if (this != &rhs)
        {
            ref_pic_list_modification = rhs.ref_pic_list_modification;
            ref_pic_list_modification.sliceTypeSrc = This();
            ref_pic_list_mvc_modification = rhs.ref_pic_list_mvc_modification;
            ref_pic_list_mvc_modification.sliceTypeSrc = This();
        }

        return *this;
    }

    RefPicListModification<T> ref_pic_list_modification;
    RefPicListMVCModification<T> ref_pic_list_mvc_modification;

private:
    const T* This() const
    {
        return static_cast<const T*>(this);
    }
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4355)
#endif
//! A slice header syntax element.
class SliceHeader :
    public SliceTypeSrc<SliceHeader>
  , public RefIdxSrc<SliceHeader>
  , public ChromaArrayTypeSrc<SliceHeader>
  , public RefPicListModificationContainer<SliceHeader>
  , public PredWeightTableContainer<SliceHeader>
{
    friend class Slice;

public:
    SliceHeader(
        NALU &nalu,
        const SeqParamSetSrc *seqParamSrc,
        const PicParamSetSrc *picParamSrc
    )
      : RefPicListModificationContainer<SliceHeader>(this)
      , PredWeightTableContainer<SliceHeader>(this)
      , m_seqParamSrc(seqParamSrc)
      , m_picParamSrc(picParamSrc)
      , m_naluType(nalu.GetUnitType())
      , m_naluRefIdc(nalu.GetRefIdc())
      , idr_flag(false)
      , m_hasMMCO5(false)
      , frame_num(0)
      , pic_order_cnt_lsb(0)
      , slice_group_change_cycle(0)
    {
        m_ebspPayload.assign(
            nalu.GetEbspFromStartCode(),
            nalu.GetEbspFromStartCode() + nalu.GetEbspSizeWithStart()
        );
        idr_flag = (NAL_TYPE_IDR_SLICE == m_naluType);
        BitIArchive<NALU::RBSPIterator> ia(nalu.RBSPBegin(), nalu.RBSPEnd());
        ia >> *this;
    }

    const UINT08* GetEbspPayload() const
    {
        return &m_ebspPayload[0];
    }

    size_t GetEbspSize() const
    {
        return m_ebspPayload.size();
    }

    const SeqParameterSet& GetSPS() const
    {
        return m_seqParamSrc->GetSPS(GetPPS().seq_parameter_set_id);
    }

    const PicParameterSet& GetPPS() const
    {
        return m_picParamSrc->GetPPS(pic_parameter_set_id);
    }

    UINT32 SliceType() const
    {
        return slice_type;
    }

    UINT32 RefIdxL0() const
    {
        return num_ref_idx_active_minus1[0] + 1;
    }

    UINT32 RefIdxL1() const
    {
        return num_ref_idx_active_minus1[1] + 1;
    }

    UINT32 ChromaArrayType() const
    {
        return GetSPS().ChromaArrayType();
    }

    UINT32 GetNaluRefIdc() const
    {
        return m_naluRefIdc;
    }

    bool IsIDR() const
    {
        return idr_flag;
    }

    bool IsReference() const
    {
        return 0 != GetNaluRefIdc();
    }

    bool HasMMCO5() const
    {
        return m_hasMMCO5;
    }

    bool RefPicListReorderingFlagL0() const
    {
        return ref_pic_list_modification.ref_pic_list_modification_flag_l0;
    }

    bool RefPicListReorderingFlagL1() const
    {
        return ref_pic_list_modification.ref_pic_list_modification_flag_l1;
    }

    const list<RefListModifier>& RefPicListMods(size_t listIdx)
    {
        return ref_pic_list_modification.mods[listIdx];
    }

    unsigned int LwrrPicNum() const
    {
        if (!field_pic_flag)
        {
            return frame_num;
        }
        else
        {
            return 2 * frame_num + 1;
        }
    }

    unsigned int MaxPicNum() const
    {
        if (!field_pic_flag)
        {
            return GetSPS().MaxFrameNum();
        }
        else
        {
            return 2 * GetSPS().MaxFrameNum();
        }
    }

private:
    const SeqParamSetSrc* GetSeqParamSrc() const
    {
        return m_seqParamSrc;
    }

    void SetSeqParamSrc(const SeqParamSetSrc *src)
    {
        m_seqParamSrc = src;
    }

    const PicParamSetSrc* GetPicParamSrc() const
    {
        return m_picParamSrc;
    }

    void SetPicParamSrc(const PicParamSetSrc *src)
    {
        m_picParamSrc = src;
    }

    const SeqParamSetSrc  *m_seqParamSrc;
    const PicParamSetSrc  *m_picParamSrc;

    UINT32                 m_naluType;
    UINT32                 m_naluRefIdc;
    vector<UINT08>         m_ebspPayload;

    bool                   idr_flag;

    bool                   m_hasMMCO5;

public:
    Ue                     first_mb_in_slice;
    Ue                     slice_type;
    Ue                     pic_parameter_set_id;
    NBits<2>               colour_plane_id;
    Num<UINT32>            frame_num;
    NBits<1>               field_pic_flag;
    NBits<1>               bottom_field_flag;
    bool                   MbaffFrameFlag;
    Ue                     idr_pic_id;
    Num<UINT32>            pic_order_cnt_lsb;
    Se                     delta_pic_order_cnt_bottom;
    Se                     delta_pic_order_cnt[2];
    Ue                     redundant_pic_cnt;
    NBits<1>               direct_spatial_mv_pred_flag;
    NBits<1>               num_ref_idx_active_override_flag;
    Ue                     num_ref_idx_active_minus1[2];

    NBits<1>               no_output_of_prior_pics_flag;
    NBits<1>               long_term_reference_flag;
    NBits<1>               adaptive_ref_pic_marking_mode_flag;
    list<DecRefPicMarking> dec_ref_pic_marking_list;

    Ue                     cabac_init_idc;
    Se                     slice_qp_delta;
    NBits<1>               sp_for_switch_flag;
    Se                     slice_qs_delta;
    Ue                     disable_deblocking_filter_idc;
    Se                     slice_alpha_c0_offset_div2;
    Se                     slice_beta_offset_div2;
    Num<UINT32>            slice_group_change_cycle;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Archive>
    void Load(Archive& sm)
    {
        m_hasMMCO5 = false;

        sm & first_mb_in_slice;
        sm & slice_type;
        sm & pic_parameter_set_id;

        const PicParameterSet &pps = GetPPS();
        const SeqParameterSet &sps = GetSPS();

        if (sps.separate_colour_plane_flag)
        {
            sm & colour_plane_id;
        }

        sm & frame_num(sps.log2_max_frame_num_minus4 + 4);
        if (!sps.frame_mbs_only_flag)
        {
            sm & field_pic_flag;
            if (field_pic_flag)
            {
                sm & bottom_field_flag;
            }
        }
        MbaffFrameFlag = sps.mb_adaptive_frame_field_flag && !field_pic_flag;
        if (NAL_TYPE_IDR_SLICE == m_naluType)
        {
            sm & idr_pic_id;
        }
        if (0 == sps.pic_order_cnt_type)
        {
            sm & pic_order_cnt_lsb(sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
            if (pps.bottom_field_pic_order_in_frame_present_flag && !field_pic_flag)
            {
                sm & delta_pic_order_cnt_bottom;
            }
        }
        if (1 == sps.pic_order_cnt_type && !sps.delta_pic_order_always_zero_flag)
        {
            sm & delta_pic_order_cnt[0];
            if (pps.bottom_field_pic_order_in_frame_present_flag && !field_pic_flag)
            {
                sm & delta_pic_order_cnt[1];
            }
        }
        if (pps.redundant_pic_cnt_present_flag)
        {
            sm & redundant_pic_cnt;
        }
        if (TypeIsB())
        {
            sm & direct_spatial_mv_pred_flag;
        }
        num_ref_idx_active_minus1[0] = pps.num_ref_idx_l0_default_active_minus1;
        num_ref_idx_active_minus1[1] = pps.num_ref_idx_l1_default_active_minus1;
        if (TypeIsP() || TypeIsSP() || TypeIsB())
        {
            sm & num_ref_idx_active_override_flag;
            if (num_ref_idx_active_override_flag)
            {
                sm & num_ref_idx_active_minus1[0];
                if (TypeIsB())
                {
                    sm & num_ref_idx_active_minus1[1];
                }
            }
        }
        if (NAL_TYPE_CODED_SLICE_EXT == m_naluType)
        {
            sm & ref_pic_list_mvc_modification;
        }
        else
        {
            sm & ref_pic_list_modification;
        }
        if ((pps.weighted_pred_flag && (TypeIsP() || TypeIsSP()))
            || (1 == pps.weighted_bipred_idc && TypeIsB()))
        {
            sm & pred_weight_table;
        }
        if (0 != m_naluRefIdc)
        {
            if (NAL_TYPE_IDR_SLICE == m_naluType)
            {
                sm & no_output_of_prior_pics_flag;
                sm & long_term_reference_flag;
            }
            else
            {
                sm & adaptive_ref_pic_marking_mode_flag;
                if (adaptive_ref_pic_marking_mode_flag)
                {
                    DecRefPicMarking dec_ref_pic_marking;
                    do
                    {
                        sm & dec_ref_pic_marking;
                        if (0 != dec_ref_pic_marking.memory_management_control_operation)
                        {
                            dec_ref_pic_marking_list.push_back(dec_ref_pic_marking);
                            if (5 == dec_ref_pic_marking.memory_management_control_operation)
                            {
                                m_hasMMCO5 = true;
                            }

                        }
                    } while (0 != dec_ref_pic_marking.memory_management_control_operation);
                }
            }
        }
        if (pps.entropy_coding_mode_flag && !TypeIsI() && !TypeIsSI())
        {
            sm & cabac_init_idc;
        }
        sm & slice_qp_delta;
        if (TypeIsSP() || TypeIsSI())
        {
            if (TypeIsSP())
            {
                sm & sp_for_switch_flag;
            }
            sm & slice_qs_delta;
        }
        if (pps.deblocking_filter_control_present_flag)
        {
            sm & disable_deblocking_filter_idc;
            if (1 != disable_deblocking_filter_idc)
            {
                sm & slice_alpha_c0_offset_div2;
                sm & slice_beta_offset_div2;
            }
        }
        if (0 < pps.num_slice_groups_minus1 && 3 <= pps.slice_group_map_type
            && pps.slice_group_map_type <= 5)
        {
            unsigned int len = (sps.pic_height_in_map_units_minus1 + 1)
                * (sps.pic_width_in_mbs_minus1 + 1)
                / (pps.slice_group_change_rate_minus1 + 1);
            if (((sps.pic_height_in_map_units_minus1 + 1)
                * (sps.pic_width_in_mbs_minus1 + 1))
                % (pps.slice_group_change_rate_minus1 + 1))
            {
                ++len;
            }
            --len;
            unsigned int bits = 0;

            while (len != 0)
            {
                len >>= 1;
                ++bits;
            }

            sm & slice_group_change_cycle(bits);
        }
    }
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
} // namespace H264

#endif // H264SYNTAX_H
