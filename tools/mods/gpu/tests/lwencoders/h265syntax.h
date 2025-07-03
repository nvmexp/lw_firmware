/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2018,2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <algorithm>
#include <cstring>
#include <vector>
#include <type_traits>

#include <cassert>

#include "core/include/massert.h"
#include "core/include/types.h"
#include "core/include/utility.h"

#include "h26xsyntax.h"
#include "nbits.h"
#include "splitserialization.h"

#include "h265consts.h"

namespace H265
{
// In this file many classes have attributes without our colwentional m_ prefix.
// These attribute names reflect names in H.265 recommendation to ease match
// between the source code and the H.265 document.

using Vid::NBits;
using BitsIO::SplitSerialization;
using H26X::Ue;
using H26X::Se;
using H26X::Num;
using H26X::BitIArchive;
using H26X::BitOArchive;

// see (5-5) of ITU-T H.265
template <class T1, class T2, class T3>
remove_cv_t<T3>
Clip3(
    T1 x, T2 y, T3 z,
    enable_if_t<is_integral<remove_cv_t<T1>>::value> *dummy1 = 0,
    enable_if_t<is_integral<remove_cv_t<T2>>::value> *dummy2 = 0,
    enable_if_t<is_integral<remove_cv_t<T3>>::value> *dummy3 = 0
)
{
    if (z < x)
        return static_cast<remove_cv_t<T3>>(x);
    if (z > y)
        return static_cast<remove_cv_t<T3>>(y);
    return z;
}

enum NALUType
{
    // _N denotes sub-layer non-reference pictures
    NAL_TYPE_TRAIL_N        = 0,  // Coded slice segment of a non-TSA, non-STSA trailing picture
    NAL_TYPE_TSA_N          = 2,  // Coded slice segment of a TSA picture
    NAL_TYPE_STSA_N         = 4,  // Coded slice segment of an STSA picture
    NAL_TYPE_RADL_N         = 6,  // Coded slice segment of a RADL picture
    NAL_TYPE_RASL_N         = 8,  // Coded slice segment of a RASL picture
    NAL_TYPE_RSV_VCL_N10    = 10, // Reserved non-IRAP sub-layer non-reference VCL NAL unit types
    NAL_TYPE_RSV_VCL_N12    = 12, // Reserved non-IRAP sub-layer non-reference VCL NAL unit types
    NAL_TYPE_RSV_VCL_N14    = 14, // Reserved non-IRAP sub-layer non-reference VCL NAL unit types

    // _R denotes sub-layer reference pictures
    NAL_TYPE_TRAIL_R        = 1,  // Coded slice segment of a non-TSA, non-STSA trailing picture
    NAL_TYPE_TSA_R          = 3,  // Coded slice segment of a TSA picture
    NAL_TYPE_STSA_R         = 5,  // Coded slice segment of an STSA picture
    NAL_TYPE_RADL_R         = 7,  // Coded slice segment of a RADL picture
    NAL_TYPE_RASL_R         = 9,  // Coded slice segment of a RASL picture
    NAL_TYPE_RSV_VCL_R11    = 11, // Reserved non-IRAP sub-layer reference VCL NAL unit types
    NAL_TYPE_RSV_VCL_R13    = 13, // Reserved non-IRAP sub-layer reference VCL NAL unit types
    NAL_TYPE_RSV_VCL_R15    = 15, // Reserved non-IRAP sub-layer reference VCL NAL unit types

    // From NAL_TYPE_BLA_W_LP to NAL_TYPE_RSV_IRAP_VCL23 are IRAP pictures. IRAP pictures are
    // considered as sub-layer reference pictures.
    NAL_TYPE_BLA_W_LP       = 16, // Coded slice segment of a BLA picture
    NAL_TYPE_BLA_W_RADL     = 17, // Coded slice segment of a BLA picture
    NAL_TYPE_BLA_N_LP       = 18, // Coded slice segment of a BLA picture
    NAL_TYPE_IDR_W_RADL     = 19, // Coded slice segment of an IDR picture
    NAL_TYPE_IDR_N_LP       = 20, // Coded slice segment of an IDR picture
    NAL_TYPE_CRA_NUT        = 21, // Coded slice segment of a CRA picture
    NAL_TYPE_RSV_IRAP_VCL22 = 22, // Reserved IRAP VCL NAL unit types
    NAL_TYPE_RSV_IRAP_VCL23 = 23, // Reserved IRAP VCL NAL unit types

    NAL_TYPE_RSV_VCL24      = 24, // Reserved non-IRAP VCL NAL unit types
    NAL_TYPE_RSV_VCL25      = 25, // Reserved non-IRAP VCL NAL unit types
    NAL_TYPE_RSV_VCL26      = 26, // Reserved non-IRAP VCL NAL unit types
    NAL_TYPE_RSV_VCL27      = 27, // Reserved non-IRAP VCL NAL unit types
    NAL_TYPE_RSV_VCL28      = 28, // Reserved non-IRAP VCL NAL unit types
    NAL_TYPE_RSV_VCL29      = 29, // Reserved non-IRAP VCL NAL unit types
    NAL_TYPE_RSV_VCL30      = 30, // Reserved non-IRAP VCL NAL unit types
    NAL_TYPE_RSV_VCL31      = 31, // Reserved non-IRAP VCL NAL unit types

    // Non-VCL NAL unit types
    NAL_TYPE_VPS_NUT        = 32, // Video parameter set
    NAL_TYPE_SPS_NUT        = 33, // Sequence parameter set
    NAL_TYPE_PPS_NUT        = 34, // Picture parameter set
    NAL_TYPE_AUD_NUT        = 35, // Access unit delimiter
    NAL_TYPE_EOS_NUT        = 36, // End of sequence
    NAL_TYPE_EOB_NUT        = 37, // End of bitstream
    NAL_TYPE_FD_NUT         = 38, // Filler data
    NAL_TYPE_PREFIX_SEI_NUT = 39, // Supplemental enhancement information
    NAL_TYPE_SUFFIX_SEI_NUT = 40, // Supplemental enhancement information
    NAL_TYPE_RSV_LWCL41     = 41, // Reserved
    NAL_TYPE_RSV_LWCL42     = 42, // Reserved
    NAL_TYPE_RSV_LWCL43     = 43, // Reserved
    NAL_TYPE_RSV_LWCL44     = 44, // Reserved
    NAL_TYPE_RSV_LWCL45     = 45, // Reserved
    NAL_TYPE_RSV_LWCL46     = 46, // Reserved
    NAL_TYPE_RSV_LWCL47     = 47, // Reserved
    NAL_TYPE_UNSPEC48       = 48, // Unspecified
    NAL_TYPE_UNSPEC49       = 49, // Unspecified
    NAL_TYPE_UNSPEC50       = 50, // Unspecified
    NAL_TYPE_UNSPEC51       = 51, // Unspecified
    NAL_TYPE_UNSPEC52       = 52, // Unspecified
    NAL_TYPE_UNSPEC53       = 53, // Unspecified
    NAL_TYPE_UNSPEC54       = 54, // Unspecified
    NAL_TYPE_UNSPEC55       = 55, // Unspecified
    NAL_TYPE_UNSPEC56       = 56, // Unspecified
    NAL_TYPE_UNSPEC57       = 57, // Unspecified
    NAL_TYPE_UNSPEC58       = 58, // Unspecified
    NAL_TYPE_UNSPEC59       = 59, // Unspecified
    NAL_TYPE_UNSPEC60       = 60, // Unspecified
    NAL_TYPE_UNSPEC61       = 61, // Unspecified
    NAL_TYPE_UNSPEC62       = 62, // Unspecified
    NAL_TYPE_UNSPEC63       = 63  // Unspecified
};

class NalUnitType
{
public:
    NalUnitType()
    {}

    NalUnitType(NBits<6> nal_unit_type)
      : m_nalUnitType(static_cast<NALUType>(static_cast<UINT08>(nal_unit_type)))
    {}

    NalUnitType& operator =(NALUType rhs)
    {
        m_nalUnitType = rhs;

        return *this;
    }

    operator NALUType() const
    {
        return m_nalUnitType;
    }

    bool IsIRAP() const
    {
        return NAL_TYPE_BLA_W_LP <= m_nalUnitType && NAL_TYPE_RSV_IRAP_VCL23 >= m_nalUnitType;
    }

    bool IsIDR() const
    {
        return NAL_TYPE_IDR_W_RADL == m_nalUnitType || NAL_TYPE_IDR_N_LP == m_nalUnitType;
    }

    bool IsCRA() const
    {
        return NAL_TYPE_CRA_NUT == m_nalUnitType;
    }

    bool IsBLA() const
    {
        return NAL_TYPE_BLA_W_LP <= m_nalUnitType && NAL_TYPE_BLA_N_LP >= m_nalUnitType;
    }

    // If a picture has nal_unit_type equal to TRAIL_N, TSA_N, STSA_N, RADL_N,
    // RASL_N, RSV_VCL_N10, RSV_VCL_N12, or RSV_VCL_N14, the picture is a
    // sub-layer non-reference picture. Otherwise, the picture is a sublayer
    // reference picture.
    // Note that IRAP pictures are always considered as reference. Even if the
    // the whole stream contains only IDR pictures, they all are considered as
    // "reference".  This is defined in article 7.4.2.2 of ITU-T H.265.
    bool IsReference() const
    {
        return
            (NAL_TYPE_RSV_VCL_R15 >= m_nalUnitType && 0 != m_nalUnitType % 2)
            ||
            IsIRAP();
    }

private:
    NALUType m_nalUnitType;
};

enum ColorFormat
{
    YUV400 = 0, //!< Monochrome
    YUV420 = 1, //!< 4:2:0
    YUV422 = 2, //!< 4:2:2
    YUV444 = 3  //!< 4:4:4
};

enum ScalingListSize
{
    SCALING_MTX_4x4 = 0,
    SCALING_MTX_8x8,
    SCALING_MTX_16x16,
    SCALING_MTX_32x32
};

enum CoefScanType
{
    SCAN_DIAG = 0,
    SCAN_HOR,
    SCAN_VER
};

enum SarIdc
{
    SAR_1x1      = 1,
    SAR_12x11    = 2,
    SAR_10x11    = 3,
    SAR_16x11    = 4,
    SAR_40x33    = 5,
    SAR_24x11    = 6,
    SAR_20x11    = 7,
    SAR_32x11    = 8,
    SAR_80x33    = 9,
    SAR_18x11    = 10,
    SAR_15x11    = 11,
    SAR_64_33    = 12,
    SAR_160x99   = 13,
    SAR_4x3      = 14,
    SAR_3x2      = 15,
    SAR_2x1      = 16,
    EXTENDED_SAR = 255
};

enum SliceType
{
    SLICE_TYPE_B = 0,
    SLICE_TYPE_P = 1,
    SLICE_TYPE_I = 2
};

template <class T, class Allocator = allocator<T> >
class Array2D
{
    friend class ArrayView;
    friend class SubArray;

public:
    typedef Array2D<T, Allocator> ThisClass;

private:
    class ArrayView
    {
    public:
        ArrayView(ThisClass *thisClass, size_t colStart, size_t colFinish,
                                        size_t rowStart, size_t rowFinish)
        {
            m_start = thisClass->m_data + rowStart * thisClass->GetCols() + colStart;
            m_strides[0] = thisClass->GetCols();
            m_strides[1] = 1;
            m_extents[0] = rowFinish - rowStart;
            m_extents[1] = colFinish - colStart;
        }

        ArrayView & operator =(const ArrayView &rhs)
        {
            if (&rhs != this)
            {
                MASSERT(m_extents[0] == rhs.m_extents[0]);
                MASSERT(m_extents[1] == rhs.m_extents[1]);
                for (size_t i = 0; m_extents[0] > i; ++i)
                {
                    for (size_t j = 0; m_extents[1] > j; ++j)
                    {
                        m_start[i * m_strides[0] + j * m_strides[1]] =
                            rhs.m_start[i * rhs.m_strides[0] + j * rhs.m_strides[1]];
                    }
                }
            }

            return *this;
        }

    private:
        size_t m_extents[2];
        size_t m_strides[2];
        T     *m_start;
    };

    class SubArray
    {
    public:
        SubArray(const ThisClass *parent, size_t row)
          : m_data(parent->m_data)
          , m_row(row)
          , m_rows(parent->m_rows)
          , m_cols(parent->m_cols)
          , m_numElems(parent->m_numElems)
        {}

        T& at(size_t col)
        {
            MASSERT(m_row < m_rows);
            MASSERT(col < m_cols);

            size_t idx = m_cols * m_row + col;
            MASSERT(idx < m_numElems);

            return m_data[idx];
        }

        const T& at(size_t col) const
        {
            MASSERT(m_row < m_rows);
            MASSERT(col < m_cols);

            size_t idx = m_cols * m_row + col;
            MASSERT(idx < m_numElems);

            return m_data[idx];
        }

    private:
        T        *m_data;
        size_t    m_row;
        size_t    m_rows;
        size_t    m_cols;
        size_t    m_numElems;
    };

public:
    Array2D()
      : m_rows(0)
      , m_cols(0)
      , m_numElems(0)
    {
        AllocateSpace();
    }

    Array2D(size_t rows, size_t cols)
      : m_rows(rows)
      , m_cols(cols)
      , m_numElems(rows * cols)
    {
        AllocateSpace();
    }

    Array2D(size_t squareSize)
      : m_rows(squareSize)
      , m_cols(squareSize)
      , m_numElems(squareSize * squareSize)
    {
        AllocateSpace();
    }

    Array2D(const Array2D &rhs)
      : m_rows(rhs.m_rows)
      , m_cols(rhs.m_cols)
      , m_numElems(m_rows * m_cols)
    {
        AllocateSpace();
        copy(rhs.m_data, rhs.m_data + rhs.m_numElems, m_data);
    }

    ~Array2D()
    {
        DeallocateSpace();
    }

    ThisClass& Resize(size_t rows, size_t cols)
    {
        ThisClass newArray(rows, cols);

        size_t minRows = min(m_rows, rows);
        size_t minCols = min(m_cols, cols);

        ArrayView thisView(this, 0, minCols, 0, minRows);
        ArrayView newView(&newArray, 0, minCols, 0, minRows);

        newView = thisView;

        swap(m_data, newArray.m_data);
        swap(m_rows, newArray.m_rows);
        swap(m_cols, newArray.m_cols);
        swap(m_numElems, newArray.m_numElems);
        swap(m_allocator, newArray.m_allocator);

        return *this;
    }

    ThisClass& Resize(size_t squareSize)
    {
        return Resize(squareSize, squareSize);
    }

    T* operator[](size_t row)
    {
        return &m_data[m_cols * row];
    }

    const T* operator[](size_t row) const
    {
        return &m_data[m_cols * row];
    }

    SubArray at(size_t row)
    {
        return SubArray(this, row);
    }

    const SubArray at(size_t row) const
    {
        return SubArray(this, row);
    }

    ThisClass& operator=(const ThisClass &rhs)
    {
        if (&rhs != this)
        {
            Resize(rhs.m_rows, rhs.m_cols);

            copy(rhs.m_data, rhs.m_data + rhs.m_numElems, m_data);
        }

        return *this;
    }

    size_t GetRows() const
    {
        return m_rows;
    }

    size_t GetCols() const
    {
        return m_cols;
    }

private:
    void AllocateSpace()
    {
        m_data = m_allocator.allocate(m_numElems);
        uninitialized_fill_n(m_data, m_numElems, T());
    }

    void DeallocateSpace()
    {
        if (nullptr != m_data)
        {
            for (T *i = m_data; m_data + m_numElems != i; ++i)
            {
                m_allocator.destroy(i);
            }
            m_allocator.deallocate(m_data, m_numElems);
        }
    }

    T        *m_data;
    size_t    m_rows;
    size_t    m_cols;
    size_t    m_numElems;
    Allocator m_allocator;
};

// See 7.4.3.2 of ITU-T H.265.
// The array ScanOrder[log2BlockSize][scanIdx][sPos][sComp] specifies the
// mapping of the scan position sPos, ranging from 0 to (1 << log2BlockSize)
// * (1 << log2BlockSize) - 1, inclusive, to horizontal and vertical components
// of the scan-order matrix. The array index scanIdx equal to 0 specifies an
// up-right diagonal scan order, scanIdx equal to 1 specifies a horizontal scan
// order, and scanIdx equal to 2 specifies a vertical scan order. The array
// index sComp equal to 0 specifies the horizontal component and the array index
// sComp equal to 1 specifies the vertical component.
class ScanOrder
{
public:
    ScanOrder();

    size_t operator()(size_t log2BlockSize, size_t scanIdx, size_t sPos, size_t sComp)
    {
        return m_scanOrder[log2BlockSize][scanIdx][sPos][sComp];
    }

private:
    Array2D<size_t> m_scanOrder[4][3];
};

extern ScanOrder scanOrder;

class ProfileTierLevel
{
    static const size_t maxMaxSubLayersMinius1 = 6;

public:
    NBits<2> general_profile_space;
    NBits<1> general_tier_flag;
    NBits<5> general_profile_idc;
    NBits<1> general_profile_compatibility_flag[32];
    NBits<1> general_progressive_source_flag;
    NBits<1> general_interlaced_source_flag;
    NBits<1> general_non_packed_constraint_flag;
    NBits<1> general_frame_only_constraint_flag;
    NBits<8> general_level_idc;
    NBits<1> sub_layer_profile_present_flag[maxMaxSubLayersMinius1];
    NBits<1> sub_layer_level_present_flag[maxMaxSubLayersMinius1];
    NBits<2> sub_layer_profile_space[maxMaxSubLayersMinius1];
    NBits<1> sub_layer_tier_flag[maxMaxSubLayersMinius1];
    NBits<5> sub_layer_profile_idc[maxMaxSubLayersMinius1];
    NBits<1> sub_layer_profile_compatibility_flag[maxMaxSubLayersMinius1][32];
    NBits<1> sub_layer_progressive_source_flag[maxMaxSubLayersMinius1];
    NBits<1> sub_layer_interlaced_source_flag[maxMaxSubLayersMinius1];
    NBits<1> sub_layer_non_packed_constraint_flag[maxMaxSubLayersMinius1];
    NBits<1> sub_layer_frame_only_constraint_flag[maxMaxSubLayersMinius1];
    NBits<8> sub_layer_level_idc[maxMaxSubLayersMinius1];

    template <class Archive>
    void Serialize(Archive& sm)
    {
        MASSERT(maxNumSubLayersMinus1 <= maxMaxSubLayersMinius1);

        sm & LWP(general_profile_space);
        sm & LWP(general_tier_flag);
        sm & LWP(general_profile_idc);
        for (UINT32 j = 0; NUMELEMS(general_profile_compatibility_flag) > j; ++j)
        {
            sm & LWP1(general_profile_compatibility_flag, j);
        }
        sm & LWP(general_progressive_source_flag);
        sm & LWP(general_interlaced_source_flag);
        sm & LWP(general_non_packed_constraint_flag);
        sm & LWP(general_frame_only_constraint_flag);

        NBits<44> general_reserved_zero_44bits;
        sm & LWP(general_reserved_zero_44bits);
        if (Archive::isLoading)
        {
            MASSERT(0 == general_reserved_zero_44bits);
        }
        sm & LWP(general_level_idc);

        MASSERT(GetMaxNumSubLayersMinus1() <= maxMaxSubLayersMinius1);
        for (UINT32 i = 0; GetMaxNumSubLayersMinus1() > i; ++i) {
            sm & LWP1(sub_layer_profile_present_flag, i);
            sm & LWP1(sub_layer_level_present_flag, i);
        }
        if (GetMaxNumSubLayersMinus1() > 0)
        {
            NBits<2> reserved_zero_2bits;
            for (UINT32 i = GetMaxNumSubLayersMinus1(); 8 > i; ++i)
            {
                sm & LWP(reserved_zero_2bits);
                if (Archive::isLoading)
                {
                    MASSERT(0 == reserved_zero_2bits);
                }
            }
        }
        for (UINT32 i = 0; GetMaxNumSubLayersMinus1() > i; ++i) {
            if (sub_layer_profile_present_flag[i]) {
                sm & LWP1(sub_layer_profile_space, i);
                sm & LWP1(sub_layer_tier_flag, i);
                sm & LWP1(sub_layer_profile_idc, i);
                for (UINT32 j = 0; 32 > j; ++j)
                {
                    sm & LWP2(sub_layer_profile_compatibility_flag, i, j);
                }
                sm & LWP1(sub_layer_progressive_source_flag, i);
                sm & LWP1(sub_layer_interlaced_source_flag, i);
                sm & LWP1(sub_layer_non_packed_constraint_flag, i);
                sm & LWP1(sub_layer_frame_only_constraint_flag, i);
                NBits<44> sub_layer_reserved_zero_44bits;
                sm & LWP(sub_layer_reserved_zero_44bits);
                if (Archive::isLoading)
                {
                    MASSERT(0 == sub_layer_reserved_zero_44bits);
                }
            }
            else if (Archive::isLoading)
            {
                sub_layer_tier_flag[i] = false;
            }

            if (sub_layer_level_present_flag[i])
            {
                sm & LWP1(sub_layer_level_idc, i);
            }
        }
    }

    UINT32 GetMaxNumSubLayersMinus1() const
    {
        return maxNumSubLayersMinus1;
    }

    void SetMaxNumSubLayersMinus1(UINT32 val)
    {
        maxNumSubLayersMinus1 = val;
    }

private:
    UINT32 maxNumSubLayersMinus1;
};

class SubLayerHrdParameters
{
    static const size_t maxCpbCnt = 64;

public:
    Ue       bit_rate_value_minus1[maxCpbCnt];
    Ue       cpb_size_value_minus1[maxCpbCnt];
    Ue       cpb_size_du_value_minus1[maxCpbCnt];
    Ue       bit_rate_du_value_minus1[maxCpbCnt];
    NBits<1> cbr_flag[maxCpbCnt];

    template <class Archive>
    void Serialize(Archive& sm)
    {
        MASSERT(GetCpbCnt() < maxCpbCnt);
        for (UINT32 i = 0; GetCpbCnt() >= i; ++i)
        {
            sm & LWP1(bit_rate_value_minus1, i);
            sm & LWP1(cpb_size_value_minus1, i);
            if (GetSubPicHrdParamsPresentFlag())
            {
                sm & LWP1(cpb_size_du_value_minus1, i);
                sm & LWP1(bit_rate_du_value_minus1, i);
            }
            sm & LWP1(cbr_flag, i);
        }
    }

    size_t GetCpbCnt() const
    {
        return cpbCnt;
    }

    void SetCpbCnt(size_t val)
    {
        cpbCnt = val;
    }

    bool GetSubPicHrdParamsPresentFlag() const
    {
        return sub_pic_hrd_params_present_flag;
    }

    void SetSubPicHrdParamsPresentFlag(bool val)
    {
        sub_pic_hrd_params_present_flag = val;
    }

private:
    size_t cpbCnt;
    bool   sub_pic_hrd_params_present_flag;
};

class HrdParameters
{
public:
    NBits<1>              nal_hrd_parameters_present_flag;
    NBits<1>              vcl_hrd_parameters_present_flag;
    NBits<1>              sub_pic_hrd_params_present_flag;
    NBits<8>              tick_divisor_minus2;
    NBits<5>              du_cpb_removal_delay_increment_length_minus1;
    NBits<1>              sub_pic_cpb_params_in_pic_timing_sei_flag;
    NBits<5>              dpb_output_delay_du_length_minus1;
    NBits<4>              bit_rate_scale;
    NBits<4>              cpb_size_scale;
    NBits<4>              cpb_size_du_scale;
    NBits<5>              initial_cpb_removal_delay_length_minus1;
    NBits<5>              au_cpb_removal_delay_length_minus1;
    NBits<5>              dpb_output_delay_length_minus1;
    NBits<1>              fixed_pic_rate_general_flag[maxTLayer];
    NBits<1>              fixed_pic_rate_within_cvs_flag[maxTLayer];
    Ue                    elemental_duration_in_tc_minus1[maxTLayer];
    NBits<1>              low_delay_hrd_flag[maxTLayer];
    Ue                    cpb_cnt_minus1[maxTLayer];
    SubLayerHrdParameters nalSubLayerHrdParameters[maxTLayer];
    SubLayerHrdParameters vclSubLayerHrdParameters[maxTLayer];

    template <class Archive>
    void Serialize(Archive& sm)
    {
        MASSERT(GetMaxNumSubLayersMinus1() < maxTLayer);

        if (GetCommonInfPresentFlag())
        {
            sm & LWP(nal_hrd_parameters_present_flag);
            sm & LWP(vcl_hrd_parameters_present_flag);
            if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
            {
                sm & LWP(sub_pic_hrd_params_present_flag);
                if (sub_pic_hrd_params_present_flag)
                {
                    sm & LWP(tick_divisor_minus2);
                    sm & LWP(du_cpb_removal_delay_increment_length_minus1);
                    sm & LWP(sub_pic_cpb_params_in_pic_timing_sei_flag);
                    sm & LWP(dpb_output_delay_du_length_minus1);
                }
                sm & LWP(bit_rate_scale);
                sm & LWP(cpb_size_scale);
                if (sub_pic_hrd_params_present_flag)
                {
                    sm & LWP(cpb_size_du_scale);
                }
                sm & LWP(initial_cpb_removal_delay_length_minus1);
                sm & LWP(au_cpb_removal_delay_length_minus1);
                sm & LWP(dpb_output_delay_length_minus1);
            }
        }

        for (UINT32 i = 0; GetMaxNumSubLayersMinus1() >= i; ++i)
        {
            sm & LWP1(fixed_pic_rate_general_flag, i);
            if (!fixed_pic_rate_general_flag[i])
            {
                sm & LWP1(fixed_pic_rate_within_cvs_flag, i);
            }
            else
            {
                if (Archive::isLoading)
                {
                    fixed_pic_rate_within_cvs_flag[i] = true;
                }
            }

            if (Archive::isLoading)
            {
                low_delay_hrd_flag[i] = false;
                cpb_cnt_minus1[i] = 0;
            }

            if (fixed_pic_rate_within_cvs_flag[i])
            {
                sm & LWP1(elemental_duration_in_tc_minus1, i);
            }
            else
            {
                sm & LWP1(low_delay_hrd_flag, i);
            }

            if (!low_delay_hrd_flag[i])
            {
                sm & LWP1(cpb_cnt_minus1, i);
            }

            if (nal_hrd_parameters_present_flag)
            {
                if (Archive::isLoading)
                {
                    nalSubLayerHrdParameters[i].SetCpbCnt(cpb_cnt_minus1[i]);
                    nalSubLayerHrdParameters[i].SetSubPicHrdParamsPresentFlag(sub_pic_hrd_params_present_flag);
                }

                sm & LWP1(nalSubLayerHrdParameters, i);
            }
            if (vcl_hrd_parameters_present_flag)
            {
                if (Archive::isLoading)
                {
                    vclSubLayerHrdParameters[i].SetCpbCnt(cpb_cnt_minus1[i]);
                    vclSubLayerHrdParameters[i].SetSubPicHrdParamsPresentFlag(sub_pic_hrd_params_present_flag);
                }
                sm & LWP1(vclSubLayerHrdParameters, i);
            }
        }
    }

    bool GetCommonInfPresentFlag() const
    {
        return commonInfPresentFlag;
    }

    void SetCommonInfPresentFlag(bool val)
    {
        commonInfPresentFlag = val;
    }

    UINT32 GetMaxNumSubLayersMinus1() const
    {
        return maxNumSubLayersMinus1;
    }

    void SetMaxNumSubLayersMinus1(UINT32 val)
    {
        maxNumSubLayersMinus1 = val;
    }

private:
    bool   commonInfPresentFlag;
    UINT32 maxNumSubLayersMinus1;
};

class VidParameterSet
{
    static const size_t maxOpSets = 1024;
    static const size_t maxNuhReservedZeroLayerId = 1;

public:
    VidParameterSet()
      : m_empty(true)
    {}

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
    bool                  m_empty;

public:
    NBits<4>              vps_video_parameter_set_id;
    NBits<6>              vps_max_layers_minus1;
    NBits<3>              vps_max_sub_layers_minus1;
    NBits<1>              vps_temporal_id_nesting_flag;
    ProfileTierLevel      profile_tier_level;
    NBits<1>              vps_sub_layer_ordering_info_present_flag;
    Ue                    vps_max_dec_pic_buffering_minus1[maxTLayer];
    Ue                    vps_max_num_reorder_pics[maxTLayer];
    Ue                    vps_max_latency_increase_plus1[maxTLayer];
    NBits<6>              vps_max_layer_id;
    Ue                    vps_num_layer_sets_minus1;
    NBits<1>              layer_id_included_flag[maxOpSets][maxNuhReservedZeroLayerId];
    NBits<1>              vps_timing_info_present_flag;
    NBits<32>             vps_num_units_in_tick;
    NBits<32>             vps_time_scale;
    NBits<1>              vps_poc_proportional_to_timing_flag;
    Ue                    vps_num_ticks_poc_diff_one_minus1;
    Ue                    vps_num_hrd_parameters;
    vector<Ue>            hrd_layer_set_idx;
    vector<NBits<1> >     cprms_present_flag;
    vector<HrdParameters> hrd_parameters;

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

        NBits<16> vps_reserved_0xffff_16bits;
        NBits<2>  vps_reserved_three_2bits;

        sm & LWP(vps_video_parameter_set_id);
        if (Archive::isSaving)
        {
            vps_reserved_three_2bits = 3;
        }
        sm & LWP(vps_reserved_three_2bits);
        if (Archive::isLoading)
        {
            MASSERT(3 == vps_reserved_three_2bits);
        }
        sm & LWP(vps_max_layers_minus1);
        sm & LWP(vps_max_sub_layers_minus1);
        sm & LWP(vps_temporal_id_nesting_flag);
        if (Archive::isSaving)
        {
            vps_reserved_0xffff_16bits = 0xffff;
        }
        sm & LWP(vps_reserved_0xffff_16bits);
        if (Archive::isLoading)
        {
            MASSERT(0xffff == vps_reserved_0xffff_16bits);
        }
        if (Archive::isLoading)
        {
            profile_tier_level.SetMaxNumSubLayersMinus1(vps_max_sub_layers_minus1);
        }
        sm & LWP(profile_tier_level);
        sm & LWP(vps_sub_layer_ordering_info_present_flag);
        MASSERT(maxTLayer > vps_max_sub_layers_minus1);
        if (vps_sub_layer_ordering_info_present_flag)
        {
            for (UINT32 i = 0; vps_max_sub_layers_minus1 >= i; ++i)
            {
                sm & LWP1(vps_max_dec_pic_buffering_minus1, i);
                sm & LWP1(vps_max_num_reorder_pics, i);
                sm & LWP1(vps_max_latency_increase_plus1, i);
            }
        }
        else
        {
            sm & LWP1(vps_max_dec_pic_buffering_minus1, vps_max_sub_layers_minus1);
            sm & LWP1(vps_max_num_reorder_pics, vps_max_sub_layers_minus1);
            sm & LWP1(vps_max_latency_increase_plus1, vps_max_sub_layers_minus1);
            if (Archive::isLoading)
            {
                for (UINT32 i = 0; vps_max_sub_layers_minus1 > i; ++i)
                {
                    vps_max_dec_pic_buffering_minus1[i] = vps_max_dec_pic_buffering_minus1[vps_max_sub_layers_minus1];
                    vps_max_num_reorder_pics[i] = vps_max_num_reorder_pics[vps_max_sub_layers_minus1];
                    vps_max_latency_increase_plus1[i] = vps_max_latency_increase_plus1[vps_max_sub_layers_minus1];
                }
            }
        }
        if (Archive::isSaving)
        {
            MASSERT(maxNuhReservedZeroLayerId >= vps_max_layer_id);
            MASSERT(maxOpSets >= vps_num_layer_sets_minus1);
        }
        sm & LWP(vps_max_layer_id);
        sm & LWP(vps_num_layer_sets_minus1);
        if (Archive::isLoading)
        {
            MASSERT(maxNuhReservedZeroLayerId >= vps_max_layer_id);
            MASSERT(maxOpSets >= vps_num_layer_sets_minus1);
        }
        for (UINT32 i = 1; vps_num_layer_sets_minus1 >= i; ++i)
        {
            for (UINT32 j = 0; vps_max_layer_id >= j; ++j)
            {
                sm & LWP2(layer_id_included_flag, i, j);
            }
        }
        sm & LWP(vps_timing_info_present_flag);
        if (vps_timing_info_present_flag)
        {
            sm & LWP(vps_num_units_in_tick);
            sm & LWP(vps_time_scale);
            sm & LWP(vps_poc_proportional_to_timing_flag);
            if (vps_poc_proportional_to_timing_flag)
            {
                sm & LWP(vps_num_ticks_poc_diff_one_minus1);
            }
            if (Archive::isSaving)
            {
                vps_num_hrd_parameters = static_cast<Ue::ValueType>(hrd_layer_set_idx.size());
                MASSERT(cprms_present_flag.size() == vps_num_hrd_parameters);
                MASSERT(hrd_parameters.size() == vps_num_hrd_parameters);
            }
            sm & LWP(vps_num_hrd_parameters);
            if (Archive::isLoading)
            {
                hrd_layer_set_idx.resize(vps_num_hrd_parameters);
                cprms_present_flag.resize(vps_num_hrd_parameters);
                hrd_parameters.resize(vps_num_hrd_parameters);
                cprms_present_flag[0] = true;
            }
            for (UINT32 i = 0; vps_num_hrd_parameters > i; ++i)
            {
                sm & LWP1(hrd_layer_set_idx, i);
                if (0 < i)
                {
                    sm & LWP1(cprms_present_flag, i);
                }
                if (Archive::isLoading)
                {
                    hrd_parameters[i].SetCommonInfPresentFlag(cprms_present_flag[i]);
                    hrd_parameters[i].SetMaxNumSubLayersMinus1(vps_max_layers_minus1);
                }
                sm & LWP1(hrd_parameters, i);
            }
        }

        if (Archive::isLoading)
        {
            m_empty = false;
        }
    }
};

class SeqParameterSet;
class PicParameterSet;

class VidParamSetSrc
{
public:
    virtual ~VidParamSetSrc()
    {}

    const VidParameterSet& GetVPS(size_t id) const
    {
        return DoGetVPS(id);
    }

private:
    virtual const VidParameterSet& DoGetVPS(size_t id) const = 0;
};

class SeqParamSetSrc
{
public:
    const SeqParameterSet& GetSPS(size_t id) const
    {
        return DoGetSPS(id);
    }

private:
    virtual const SeqParameterSet& DoGetSPS(size_t id) const = 0;
};

class PicParamSetSrc
{
public:
    const PicParameterSet& GetPPS(size_t id) const
    {
        return DoGetPPS(id);
    }

private:
    virtual const PicParameterSet& DoGetPPS(size_t id) const = 0;
};

class ScalingListData
{
    friend class DefaultScalingListData;
public:
    ScalingListData()
      : m_loaded(false)
    {}

    template <class Archive>
    void Serialize(Archive& sm)
    {
        SplitSerialization(sm, *this);
    }

    UINT08 ScalingFactor(size_t sizeId, size_t matrixId, size_t x, size_t y) const
    {
        MASSERT(m_loaded);
        return m_scalingFactor[sizeId][matrixId][y][x];
    }

    static
    const ScalingListData& GetDefaultScalingListData();

    template <class Archive>
    void Load(Archive& sm)
    {
        for (size_t sizeId = 0; numScalingMatrixSizes > sizeId; ++sizeId)
        {
            UINT32 numScalingMatrices = numScalingMatricesPerSize[sizeId];
            for (size_t matrixId = 0; numScalingMatrices > matrixId; ++matrixId)
            {
                m_scalingFactor[sizeId][matrixId].Resize(scalingListSize[sizeId]);
            }
        }

        for (UINT32 sizeId = 0; numScalingMatrixSizes > sizeId; ++sizeId)
        {
            UINT32 numScalingMatrices = numScalingMatricesPerSize[sizeId];
            vector<UINT08> scalingListDC(numScalingMatrices);
            vector<vector<UINT08> > scalingList(numScalingMatrices);

            for (UINT32 matrixId = 0; numScalingMatrices > matrixId; ++matrixId)
            {
                scalingList[matrixId].resize(min(maxScalingListCoefNum, scalingList1DSize[sizeId]));

                NBits<1> scaling_list_pred_mode_flag;
                sm & scaling_list_pred_mode_flag;
                if (!scaling_list_pred_mode_flag)
                {
                    // The values of the scaling list are the same as the values
                    // of a reference scaling list.

                    Ue scaling_list_pred_matrix_id_delta;
                    sm & scaling_list_pred_matrix_id_delta;

                    UINT32 refMatrixId = matrixId - scaling_list_pred_matrix_id_delta;

                    // scalingListDC[matrixId] specifies the DC value of the scaling list for 16x16
                    // size when sizeId is equal to 2 and specifies the DC value of the scaling list
                    // for 32x32 size when sizeId is equal to 3. The value of
                    // scalingListDC[matrixId] shall be in the range of 1 to 255, inclusive.
                    // When scaling_list_pred_matrix_id_delta is equal to 0, and sizeId is greater
                    // than 1, the value of scalingListDC[matrixId] is inferred to be equal to 16.
                    // When scaling_list_pred_matrix_id_delta is not equal to 0 and sizeId is
                    // greater than 1, the value of scalingListDC[matrixId] is inferred to be equal
                    // to scalingListDC[refMatrixId].

                    // Copy DC coefficients
                    if (SCALING_MTX_8x8 < sizeId)
                    {
                        if (matrixId == refMatrixId)
                        {
                            scalingListDC[matrixId] = 16;
                        }
                        else
                        {
                            scalingListDC[matrixId] = scalingListDC[refMatrixId];
                        }
                    }

                    // scaling_list_pred_matrix_id_delta specifies the reference scaling list used
                    // to derive scalingList[matrixId] as follows :
                    // - If scaling_list_pred_matrix_id_delta is equal to 0, the scaling list is
                    //   inferred from the default scaling list scalingList[sizeId][matrixId] as
                    //   specified in Table 7-5 and Table 7-6.
                    // - Otherwise, the scaling list is inferred from the reference scaling list as
                    //   follows:
                    //     refMatrixId = matrixId - scaling_list_pred_matrix_id_delta;
                    //     scalingList[matrixId] = scalingList[refMatrixId];

                    // Copy lists
                    if (matrixId == refMatrixId)
                    {
                        // copy from default
                        const UINT08 *defaultList;
                        if (SCALING_MTX_4x4 == sizeId)
                        {
                            defaultList = default4x4ScalingList;
                        }
                        else if (SCALING_MTX_8x8 == sizeId || SCALING_MTX_16x16 == sizeId)
                        {
                            if (matrixId < 3)
                            {
                                defaultList = default8x8IntraScalingList;
                            }
                            else
                            {
                                defaultList = default8x8InterScalingList;
                            }
                        }
                        else
                        {
                            if (matrixId < 1)
                            {
                                defaultList = default8x8IntraScalingList;
                            }
                            else
                            {
                                defaultList = default8x8InterScalingList;
                            }
                        }
                        vector<UINT08> &dest = scalingList[matrixId];
                        copy(defaultList, defaultList + dest.size(), dest.begin());
                    }
                    else
                    {
                        scalingList[matrixId] = scalingList[refMatrixId];
                    }
                }
                else
                {
                    // scaling_list_pred_mode_flag = true

                    vector<UINT08> &dest = scalingList[matrixId];

                    // See 7.3.4 of ITU-T H.265
                    size_t coefNum = dest.size();
                    INT32 nextCoef = 8;

                    if (sizeId > SCALING_MTX_8x8)
                    {
                        Se scaling_list_dc_coef_minus8;
                        sm & scaling_list_dc_coef_minus8;
                        nextCoef = scaling_list_dc_coef_minus8 + 8;
                        scalingListDC[matrixId] = static_cast<UINT08>(nextCoef);
                    }
                    for (size_t i = 0; coefNum > i; ++i)
                    {
                        Se scaling_list_delta_coef;
                        sm & scaling_list_delta_coef;
                        nextCoef = (nextCoef + scaling_list_delta_coef + 256) % 256;
                        dest[i] = static_cast<UINT08>(nextCoef);
                    }
                }

                // Build matrices from lists
                switch (sizeId)
                {
                    case SCALING_MTX_4x4:
                    {
                        for (size_t i = 0; scalingList[matrixId].size() > i; ++i)
                        {
                            // See equation 7-32 of ITU-T H.265
                            size_t x = scanOrder(2, SCAN_DIAG, i, 0);
                            size_t y = scanOrder(2, SCAN_DIAG, i, 1);

                            m_scalingFactor
                                [SCALING_MTX_4x4]
                                [matrixId]
                                [y][x] = scalingList[matrixId][i];
                        }
                        break;
                    }
                    case SCALING_MTX_8x8:
                    {
                        for (size_t i = 0; scalingList[matrixId].size() > i; ++i)
                        {
                            // See equation 7-33 of ITU-T H.265
                            size_t x = scanOrder(3, SCAN_DIAG, i, 0);
                            size_t y = scanOrder(3, SCAN_DIAG, i, 1);

                            m_scalingFactor
                                [SCALING_MTX_8x8]
                                [matrixId]
                                [y][x] = scalingList[matrixId][i];
                        }
                        break;
                    }
                    case SCALING_MTX_16x16:
                    {
                        for (size_t i = 0; scalingList[matrixId].size() > i; ++i)
                        {
                            for (size_t j = 0; 2 > j; ++j)
                            {
                                for (size_t k = 0; 2 > k; ++k)
                                {
                                    // See equation 7-34 of ITU-T H.265
                                    size_t x = scanOrder(3, SCAN_DIAG, i, 0);
                                    size_t y = scanOrder(3, SCAN_DIAG, i, 1);

                                    m_scalingFactor
                                        [SCALING_MTX_16x16]
                                        [matrixId]
                                        [y * 2 + j]
                                        [x * 2 + k] = scalingList[matrixId][i];
                                }
                            }
                        }
                        // See equation 7-35 of ITU-T H.265
                        m_scalingFactor[SCALING_MTX_16x16][matrixId][0][0] = scalingListDC[matrixId];
                        break;
                    }
                    case SCALING_MTX_32x32:
                    {
                        for (size_t i = 0; scalingList[matrixId].size() > i; ++i)
                        {
                            for (size_t j = 0; 4 > j; ++j)
                            {
                                for (size_t k = 0; 4 > k; ++k)
                                {
                                    // See equation 7-36 of ITU-T H.265
                                    size_t x = scanOrder(3, SCAN_DIAG, i, 0);
                                    size_t y = scanOrder(3, SCAN_DIAG, i, 1);

                                    m_scalingFactor
                                        [SCALING_MTX_32x32]
                                        [matrixId]
                                        [y * 4 + j]
                                        [x * 4 + k] = scalingList[matrixId][i];
                                }
                            }
                        }
                        // See equation 7-37 of ITU-T H.265
                        m_scalingFactor[SCALING_MTX_32x32][matrixId][0][0] = scalingListDC[matrixId];
                        break;
                    }
                }
            }
        }

        m_loaded = true;
    }

    template <class Archive>
    void Save(Archive& sm) const
    {
        MASSERT(!"Saving scaling_list_data is not supported");
    }

#if defined(SUPPORT_TEXT_OUTPUT)
    void Save(H26X::TextOArchive& sm) const;
#endif

private:
    void InitFromDefault();

    bool            m_loaded;
    Array2D<UINT08> m_scalingFactor[numScalingMatrixSizes][maxNumScalingMatrices];
};

template <class T>
class ShortTermRefPicSet;

template <class T>
class ShortTermRefPicSetsSrc
{
public:
    ShortTermRefPicSet<T>& GetShortTermRefPicSet(UINT32 idx)
    {
        return This()->GetRefPicSet(idx);
    }

    const ShortTermRefPicSet<T>& GetShortTermRefPicSet(UINT32 idx) const
    {
        return This()->GetShortTermRefPicSet(idx);
    }

    UINT32 GetNumShortTermRefPicSets() const
    {
        return This()->GetNumShortTermRefPicSets();
    }

private:
    T* This()
    {
        return static_cast<T*>(this);
    }

    const T* This() const
    {
        return static_cast<const T*>(this);
    }
};

template <class T>
class ShortTermRefPicSet
{
    friend class ShortTermRefPicSetCont;

public:
    ShortTermRefPicSet()
      : m_thisIdx(0)
      , m_refPicSetsSrc(nullptr)
    {}

private:
    ShortTermRefPicSet(const ShortTermRefPicSetsSrc<T> *refPicSetsSrc, UINT32 thisIdx)
      : m_thisIdx(thisIdx)
      , m_refPicSetsSrc(refPicSetsSrc)
    {}

public:
    void SetIdx(UINT32 val)
    {
        m_thisIdx = val;
    }

    void SetRefPicSetsSrc(const ShortTermRefPicSetsSrc<T> * val)
    {
        m_refPicSetsSrc = val;
    }

    NBits<1> inter_ref_pic_set_prediction_flag;
    // The following fields are stored in the class object for the purpose of
    // saving. It is not very proper, they'd be better callwlated from
    // DeltaPocS0, UsedByLwrrPicS0, DeltaPocS1, UsedByLwrrPicS1 during the save
    // operation. It's a TODO for later improvement.
    Ue       delta_idx_minus1;
    NBits<1> delta_rps_sign;
    Ue       abs_delta_rps_minus1;
    NBits<1> used_by_lwrr_pic_flag[maxNumRefPics];
    NBits<1> use_delta_flag[maxNumRefPics];
    Ue       delta_poc_s0_minus1[maxNumRefPics];
    NBits<1> used_by_lwrr_pic_s0_flag[maxNumRefPics];
    Ue       delta_poc_s1_minus1[maxNumRefPics];
    NBits<1> used_by_lwrr_pic_s1_flag[maxNumRefPics];
    Ue       num_negative_pics;
    Ue       num_positive_pics;

    INT32    DeltaPocS0[maxNumRefPics];
    bool     UsedByLwrrPicS0[maxNumRefPics];
    UINT32   NumNegativePics;
    INT32    DeltaPocS1[maxNumRefPics];
    bool     UsedByLwrrPicS1[maxNumRefPics];
    UINT32   NumPositivePics;
    UINT32   NumDeltaPocs;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        SerializeCommon(sm);
    }

#if defined(SUPPORT_TEXT_OUTPUT)
    void Serialize(H26X::TextOArchive& sm)
    {
        SerializeCommon(sm);
        sm & LWP(NumNegativePics);
        sm & LWP(NumPositivePics);
        sm & LWP(NumDeltaPocs);
        for (UINT32 i = 0; NumNegativePics > i; ++i)
        {
            sm & LWP1(UsedByLwrrPicS0, i);
            sm & LWP1(DeltaPocS0, i);
        }
        for (UINT32 i = 0; NumPositivePics > i; ++i)
        {
            sm & LWP1(UsedByLwrrPicS1, i);
            sm & LWP1(DeltaPocS1, i);
        }
    }
#endif

private:
    template <class Archive>
    void SerializeCommon(Archive& sm)
    {
        if (0 != m_thisIdx)
        {
            sm & LWP(inter_ref_pic_set_prediction_flag);
        }
        else
        {
            inter_ref_pic_set_prediction_flag = false;
        }

        if (inter_ref_pic_set_prediction_flag)
        {
            if (m_thisIdx == m_refPicSetsSrc->GetNumShortTermRefPicSets())
            {
                // If we are here, it means that the RPS was signaled from a
                // slice header instead of from SPS. Comparison of the index
                // with num_short_term_ref_pic_sets in order to determine
                // whether the set is from the SPS of the slice header is the
                // way described in ITU-T H.265. See NOTE 2 in article 8.3.2.
                sm & LWP(delta_idx_minus1);
            }
            else
            {
                delta_idx_minus1 = 0;
            }

            UINT32 refIdx = m_thisIdx - 1 - delta_idx_minus1;
            const ShortTermRefPicSet &refRPS = m_refPicSetsSrc->GetShortTermRefPicSet(refIdx);

            sm & LWP(delta_rps_sign);
            sm & LWP(abs_delta_rps_minus1);

            int deltaRps = (delta_rps_sign ? -1 : 1) * (abs_delta_rps_minus1 + 1);

            MASSERT(maxNumRefPics > refRPS.NumDeltaPocs);
            for (UINT32 i = 0; refRPS.NumDeltaPocs >= i; ++i)
            {
                sm & LWP1(used_by_lwrr_pic_flag, i);
                if (!used_by_lwrr_pic_flag[i])
                {
                    sm & LWP1(use_delta_flag, i);
                }
                else
                {
                    use_delta_flag[i] = true;
                }
            }

            if (Archive::isLoading)
            {
                // Equation 7-47 of ITU-T H.265
                UINT32 s0Pos = 0;
                for (int j = refRPS.NumPositivePics - 1; 0 <= j; --j)
                {
                    int dPoc = refRPS.DeltaPocS1[j] + deltaRps;
                    if (0 > dPoc && use_delta_flag[refRPS.NumNegativePics + j])
                    {
                        DeltaPocS0[s0Pos] = dPoc;
                        UsedByLwrrPicS0[s0Pos++] = used_by_lwrr_pic_flag[refRPS.NumNegativePics + j];
                    }
                }
                if (0 > deltaRps && use_delta_flag[refRPS.NumDeltaPocs])
                {
                    DeltaPocS0[s0Pos] = deltaRps;
                    UsedByLwrrPicS0[s0Pos++] = used_by_lwrr_pic_flag[refRPS.NumDeltaPocs];
                }
                for (UINT32 j = 0; refRPS.NumNegativePics > j; ++j)
                {
                    int dPoc = refRPS.DeltaPocS0[j] + deltaRps;
                    if (0 > dPoc && use_delta_flag[j])
                    {
                        DeltaPocS0[s0Pos] = dPoc;
                        UsedByLwrrPicS0[s0Pos++] = used_by_lwrr_pic_flag[j];
                    }
                }
                NumNegativePics = s0Pos;

                // Equation 7-48 of ITU-T H.265
                UINT32 s1Pos = 0;
                for (int j = refRPS.NumNegativePics - 1; 0 <= j; --j)
                {
                    int dPoc = refRPS.DeltaPocS0[j] + deltaRps;
                    if (0 < dPoc && use_delta_flag[j])
                    {
                        DeltaPocS1[s1Pos] = dPoc;
                        UsedByLwrrPicS1[s1Pos++] = used_by_lwrr_pic_flag[j];
                    }
                }
                if (0 < deltaRps && use_delta_flag[refRPS.NumDeltaPocs])
                {
                    DeltaPocS1[s1Pos] = deltaRps;
                    UsedByLwrrPicS1[s1Pos++] = used_by_lwrr_pic_flag[refRPS.NumDeltaPocs];
                }
                for (UINT32 j = 0; refRPS.NumPositivePics > j; ++j)
                {
                    int dPoc = refRPS.DeltaPocS1[j] + deltaRps;
                    if (0 < dPoc && use_delta_flag[refRPS.NumNegativePics + j])
                    {
                        DeltaPocS1[s1Pos] = dPoc;
                        UsedByLwrrPicS1[s1Pos++] = used_by_lwrr_pic_flag[refRPS.NumNegativePics + j];
                    }
                }
                NumPositivePics = s1Pos;
            }
        }
        else
        {
            sm & LWP(num_negative_pics);
            sm & LWP(num_positive_pics);

            NumNegativePics = num_negative_pics;
            NumPositivePics = num_positive_pics;

            MASSERT(maxNumRefPics >= NumNegativePics);
            MASSERT(maxNumRefPics >= NumPositivePics);

            for (UINT32 i = 0; num_negative_pics > i; ++i)
            {
                sm & LWP1(delta_poc_s0_minus1, i);
                sm & LWP1(used_by_lwrr_pic_s0_flag, i);

                if (Archive::isLoading)
                {
                    UsedByLwrrPicS0[i] = used_by_lwrr_pic_s0_flag[i];
                    if (0 == i)
                    {
                        DeltaPocS0[i] = -(static_cast<INT32>(delta_poc_s0_minus1[i]) + 1);
                    }
                    else
                    {
                        DeltaPocS0[i] = DeltaPocS0[i - 1] - (delta_poc_s0_minus1[i] + 1);
                    }
                }
            }
            for (UINT32 i = 0; num_positive_pics > i; ++i)
            {
                sm & LWP1(delta_poc_s1_minus1, i);
                sm & LWP1(used_by_lwrr_pic_s1_flag, i);

                if (Archive::isLoading)
                {
                    UsedByLwrrPicS1[i] = used_by_lwrr_pic_s1_flag[i];
                    if (0 == i)
                    {
                        DeltaPocS1[i] = delta_poc_s1_minus1[i] + 1;
                    }
                    else
                    {
                        DeltaPocS1[i] = DeltaPocS1[i - 1] + (delta_poc_s1_minus1[i] + 1);
                    }
                }
            }
        }
        if (Archive::isLoading)
        {
            NumDeltaPocs = NumNegativePics + NumPositivePics;
        }
    }

    UINT32                           m_thisIdx;
    const ShortTermRefPicSetsSrc<T> *m_refPicSetsSrc;
};

class ShortTermRefPicSetCont : public ShortTermRefPicSetsSrc<ShortTermRefPicSetCont>
{
public:
    typedef ShortTermRefPicSet<ShortTermRefPicSetCont> Set;

    ShortTermRefPicSetCont()
    {}

    ShortTermRefPicSetCont(const ShortTermRefPicSetCont &rhs)
      : short_term_ref_pic_set(rhs.short_term_ref_pic_set)
    {
        for (size_t i = 0; short_term_ref_pic_set.size() > i; ++i)
        {
            short_term_ref_pic_set[i].m_refPicSetsSrc = this;
        }
    }

    ShortTermRefPicSetCont& operator =(const ShortTermRefPicSetCont &rhs)
    {
        if (this != &rhs)
        {
            short_term_ref_pic_set = rhs.short_term_ref_pic_set;
            for (size_t i = 0; short_term_ref_pic_set.size() > i; ++i)
            {
                short_term_ref_pic_set[i].m_refPicSetsSrc = this;
            }
        }
        return *this;
    }

    ShortTermRefPicSet<ShortTermRefPicSetCont>& GetShortTermRefPicSet(UINT32 idx)
    {
        return short_term_ref_pic_set[idx];
    }

    const ShortTermRefPicSet<ShortTermRefPicSetCont>& GetShortTermRefPicSet(UINT32 idx) const
    {
        return short_term_ref_pic_set[idx];
    }

    UINT32 GetNumShortTermRefPicSets() const
    {
        return static_cast<UINT32>(short_term_ref_pic_set.size());
    }

    void SetNumShortTermRefPicSets(UINT32 size)
    {
        short_term_ref_pic_set.resize(size);
    }

    template <class Archive>
    void Serialize(Archive& sm)
    {
        Ue num_short_term_ref_pic_sets;
        if (Archive::isSaving)
        {
            num_short_term_ref_pic_sets = static_cast<UINT32>(short_term_ref_pic_set.size());
        }
        sm & LWP(num_short_term_ref_pic_sets);
        if (Archive::isLoading)
        {
            short_term_ref_pic_set.clear();
            for (UINT32 i = 0; num_short_term_ref_pic_sets > i; ++i)
            {
                short_term_ref_pic_set.push_back(Set(this, i));
            }
        }

        for (UINT32 i = 0; num_short_term_ref_pic_sets > i; ++i)
        {
            sm & LWP1(short_term_ref_pic_set, i);
        }
    }

private:
    vector<ShortTermRefPicSet<ShortTermRefPicSetCont> > short_term_ref_pic_set;
};

class VuiParameters
{
    template <class T> friend class VuiParametersField;

    VuiParameters(const SeqParameterSet *srcSps)
      : m_srcSps(srcSps)
    {}

public:
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
    NBits<8>      matrix_coeffs;
    NBits<1>      chroma_loc_info_present_flag;
    Ue            chroma_sample_loc_type_top_field;
    Ue            chroma_sample_loc_type_bottom_field;
    NBits<1>      neutral_chroma_indication_flag;
    NBits<1>      field_seq_flag;
    NBits<1>      frame_field_info_present_flag;
    NBits<1>      default_display_window_flag;
    Ue            def_disp_win_left_offset;
    Ue            def_disp_win_right_offset;
    Ue            def_disp_win_top_offset;
    Ue            def_disp_win_bottom_offset;
    NBits<1>      vui_timing_info_present_flag;
    NBits<32>     vui_num_units_in_tick;
    NBits<32>     vui_time_scale;
    NBits<1>      vui_poc_proportional_to_timing_flag;
    Ue            vui_num_ticks_poc_diff_one_minus1;
    NBits<1>      vui_hrd_parameters_present_flag;
    HrdParameters hrd_parameters;
    NBits<1>      bitstream_restriction_flag;
    NBits<1>      tiles_fixed_structure_flag;
    NBits<1>      motion_vectors_over_pic_boundaries_flag;
    NBits<1>      restricted_ref_pic_lists_flag;
    Ue            min_spatial_segmentation_idc;
    Ue            max_bytes_per_pic_denom;
    Ue            max_bits_per_min_lw_denom;
    Ue            log2_max_mv_length_horizontal;
    Ue            log2_max_mv_length_vertical;

    template <class Archive>
    void Serialize(Archive& sm);

private:
    const SeqParameterSet *m_srcSps;
};

template <class T>
class VuiParametersField
{
protected:
    VuiParametersField()
      : vui_parameters(This())
    {}

    VuiParametersField(const VuiParametersField& rhs)
      : vui_parameters(rhs.vui_parameters)
    {
        vui_parameters.m_srcSps = This();
    }

    VuiParametersField& operator=(const VuiParametersField &rhs)
    {
        if (this != &rhs)
        {
            vui_parameters = rhs.vui_parameters;
            vui_parameters.m_srcSps = This();
        }

        return *this;
    }

    VuiParameters vui_parameters;

private:
    const T* This() const
    {
        return static_cast<const T*>(this);
    }
};

class SpsRangeExtension
{
public:
    NBits<1> transform_skip_rotation_enabled_flag;
    NBits<1> transform_skip_context_enabled_flag;
    NBits<1> implicit_rdpcm_enabled_flag;
    NBits<1> explicit_rdpcm_enabled_flag;
    NBits<1> extended_precision_processing_flag;
    NBits<1> intra_smoothing_disabled_flag;
    NBits<1> high_precision_offsets_enabled_flag;
    NBits<1> persistent_rice_adaptation_enabled_flag;
    NBits<1> cabac_bypass_alignment_enabled_flag;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & LWP(transform_skip_rotation_enabled_flag);
        sm & LWP(transform_skip_context_enabled_flag);
        sm & LWP(implicit_rdpcm_enabled_flag);
        sm & LWP(explicit_rdpcm_enabled_flag);
        sm & LWP(extended_precision_processing_flag);
        sm & LWP(intra_smoothing_disabled_flag);
        sm & LWP(high_precision_offsets_enabled_flag);
        sm & LWP(persistent_rice_adaptation_enabled_flag);
        sm & LWP(cabac_bypass_alignment_enabled_flag);
    }
};

class SpsMultilayerExtension
{
public:
    NBits<1> inter_view_mv_vert_constraint_flag;

    template <class Archive>
    void Serialize(Archive& sm)
    {
        sm & LWP(inter_view_mv_vert_constraint_flag);
    }
};

class SeqParameterSet : public VuiParametersField<SeqParameterSet>
{
    template <class T> friend class SPSContainer;
    template <class T> friend class SPSField;
    friend class Picture;
public:
    SeqParameterSet()
      : VuiParametersField<SeqParameterSet>()
      , m_vpsSrc(nullptr)
      , m_empty(true)
    {}

    explicit SeqParameterSet(const VidParamSetSrc *src)
      : VuiParametersField<SeqParameterSet>()
      , m_vpsSrc(src)
      , m_empty(true)
    {}

    const VidParameterSet* GetVPS() const
    {
        if (m_vpsSrc)
        {
            return &m_vpsSrc->GetVPS(sps_video_parameter_set_id);
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

    UINT32 GetNumShortTermRefPicSets() const
    {
        return short_term_ref_pic_sets.GetNumShortTermRefPicSets();
    }

private:
    void SetVidParamSrc(const VidParamSetSrc *src)
    {
        m_vpsSrc = src;
    }

    const VidParamSetSrc *m_vpsSrc;
    bool                  m_empty;

public:
    NBits<4>               sps_video_parameter_set_id;
    NBits<3>               sps_max_sub_layers_minus1;
    NBits<1>               sps_temporal_id_nesting_flag;
    ProfileTierLevel       profile_tier_level;
    Ue                     sps_seq_parameter_set_id;
    Ue                     chroma_format_idc;
    NBits<1>               separate_colour_plane_flag;
    Ue                     pic_width_in_luma_samples;
    Ue                     pic_height_in_luma_samples;
    NBits<1>               conformance_window_flag;
    Ue                     conf_win_left_offset;
    Ue                     conf_win_right_offset;
    Ue                     conf_win_top_offset;
    Ue                     conf_win_bottom_offset;
    Ue                     bit_depth_luma_minus8;
    Ue                     bit_depth_chroma_minus8;
    Ue                     log2_max_pic_order_cnt_lsb_minus4;
    NBits<1>               sps_sub_layer_ordering_info_present_flag;
    Ue                     sps_max_dec_pic_buffering_minus1[maxTLayer];
    Ue                     sps_max_num_reorder_pics[maxTLayer];
    Ue                     sps_max_latency_increase_plus1[maxTLayer];
    Ue                     log2_min_luma_coding_block_size_minus3;
    Ue                     log2_diff_max_min_luma_coding_block_size;
    Ue                     log2_min_transform_block_size_minus2;
    Ue                     log2_diff_max_min_transform_block_size;
    Ue                     max_transform_hierarchy_depth_inter;
    Ue                     max_transform_hierarchy_depth_intra;
    NBits<1>               scaling_list_enabled_flag;
    NBits<1>               sps_scaling_list_data_present_flag;
    ScalingListData        scaling_list_data;
    NBits<1>               amp_enabled_flag;
    NBits<1>               sample_adaptive_offset_enabled_flag;
    NBits<1>               pcm_enabled_flag;
    NBits<4>               pcm_sample_bit_depth_luma_minus1;
    NBits<4>               pcm_sample_bit_depth_chroma_minus1;
    Ue                     log2_min_pcm_luma_coding_block_size_minus3;
    Ue                     log2_diff_max_min_pcm_luma_coding_block_size;
    NBits<1>               pcm_loop_filter_disabled_flag;
    ShortTermRefPicSetCont short_term_ref_pic_sets;
    NBits<1>               long_term_ref_pics_present_flag;
    Ue                     num_long_term_ref_pics_sps;
    Num<UINT32>            lt_ref_pic_poc_lsb_sps[maxNumLongTermRefPicsSps];
    NBits<1>               used_by_lwrr_pic_lt_sps_flag[maxNumLongTermRefPicsSps];
    NBits<1>               sps_temporal_mvp_enabled_flag;
    NBits<1>               strong_intra_smoothing_enabled_flag;
    NBits<1>               vui_parameters_present_flag;
    NBits<1>               sps_extension_present_flag;
    NBits<1>               sps_range_extension_flag;
    NBits<1>               sps_multilayer_extension_flag;
    NBits<6>               sps_extension_6bits;
    SpsRangeExtension      sps_range_extension;
    SpsMultilayerExtension sps_multilayer_extension;

    UINT08 SubWidthC;           // Table 6-1 of ITU-T H.265
    UINT08 SubHeightC;          // Table 6-1 of ITU-T H.265
    UINT08 BitDepthY;           // (7-4) of ITU-T H.265
    UINT08 QpBdOffsetY;         // (7-5) of ITU-T H.265
    UINT08 BitDepthC;           // (7-6) of ITU-T H.265
    UINT08 QpBdOffsetC;         // (7-7) of ITU-T H.265
    UINT32 MaxPicOrderCntLsb;   // (7-8) of ITU-T H.265
    UINT08 MinCbLog2SizeY;      // (7-10) of ITU-T H.265
    UINT08 CtbLog2SizeY;        // (7-11) of ITU-T H.265
    UINT32 MinCbSizeY;          // (7-12) of ITU-T H.265
    UINT32 CtbSizeY;            // (7-13) of ITU-T H.265
    UINT32 PicWidthInMinCbsY;   // (7-14) of ITU-T H.265
    UINT32 PicWidthInCtbsY;     // (7-15) of ITU-T H.265
    UINT32 PicHeightInMinCbsY;  // (7-16) of ITU-T H.265
    UINT32 PicHeightInCtbsY;    // (7-17) of ITU-T H.265
    UINT32 PicSizeInMinCbsY;    // (7-18) of ITU-T H.265
    UINT32 PicSizeInCtbsY;      // (7-19) of ITU-T H.265
    UINT64 PicSizeInSamplesY;   // (7-20) of ITU-T H.265
    UINT32 PicWidthInSamplesC;  // (7-21) of ITU-T H.265
    UINT32 PicHeightInSamplesC; // (7-22) of ITU-T H.265
    UINT32 CtbWidthC;           // (7-23) of ITU-T H.265
    UINT32 CtbHeightC;          // (7-24) of ITU-T H.265
    UINT08 PcmBitDepthY;        // (7-25) of ITU-T H.265
    UINT08 PcmBitDepthC;        // (7-26) of ITU-T H.265
    INT32  CoeffMinY;           // (7-27) of ITU-T H.265
    INT32  CoeffMinC;           // (7-28) of ITU-T H.265
    INT32  CoeffMaxY;           // (7-29) of ITU-T H.265
    INT32  CoeffMaxC;           // (7-30) of ITU-T H.265
    UINT08 WpOffsetBdShiftY;    // (7-31) of ITU-T H.265
    UINT08 WpOffsetBdShiftC;    // (7-32) of ITU-T H.265
    UINT32 WpOffsetHalfRangeY;  // (7-33) of ITU-T H.265
    UINT32 WpOffsetHalfRangeC;  // (7-34) of ITU-T H.265

    template <class Archive>
    void Serialize(Archive& sm)
    {
        SerializeCommon(sm);
    }

#if defined(SUPPORT_TEXT_OUTPUT)
    void Serialize(H26X::TextOArchive& sm);
#endif

private:
    template <class Archive>
    void SerializeCommon(Archive& sm)
    {
        if (Archive::isSaving)
        {
            if (m_empty)
            {
                return;
            }
        }

        sm & LWP(sps_video_parameter_set_id);
        sm & LWP(sps_max_sub_layers_minus1);
        sm & LWP(sps_temporal_id_nesting_flag);
        if (Archive::isLoading)
        {
            profile_tier_level.SetMaxNumSubLayersMinus1(sps_max_sub_layers_minus1);
        }
        sm & LWP(profile_tier_level);
        sm & LWP(sps_seq_parameter_set_id);
        sm & LWP(chroma_format_idc);
        if (YUV444 == chroma_format_idc)
        {
            sm & LWP(separate_colour_plane_flag);
        }
        else if (Archive::isLoading)
        {
            separate_colour_plane_flag = false;
        }
        sm & LWP(pic_width_in_luma_samples);
        sm & LWP(pic_height_in_luma_samples);
        sm & LWP(conformance_window_flag);
        if (conformance_window_flag)
        {
            sm & LWP(conf_win_left_offset);
            sm & LWP(conf_win_right_offset);
            sm & LWP(conf_win_top_offset);
            sm & LWP(conf_win_bottom_offset);
        }
        sm & LWP(bit_depth_luma_minus8);
        sm & LWP(bit_depth_chroma_minus8);
        sm & LWP(log2_max_pic_order_cnt_lsb_minus4);
        sm & LWP(sps_sub_layer_ordering_info_present_flag);
        MASSERT(maxTLayer > sps_max_sub_layers_minus1);
        if (sps_sub_layer_ordering_info_present_flag)
        {
            for (UINT32 i = 0; sps_max_sub_layers_minus1 >= i; ++i)
            {
                sm & LWP1(sps_max_dec_pic_buffering_minus1, i);
                sm & LWP1(sps_max_num_reorder_pics, i);
                sm & LWP1(sps_max_latency_increase_plus1, i);
            }
        }
        else
        {
            sm & LWP1(sps_max_dec_pic_buffering_minus1, sps_max_sub_layers_minus1);
            sm & LWP1(sps_max_num_reorder_pics, sps_max_sub_layers_minus1);
            sm & LWP1(sps_max_latency_increase_plus1, sps_max_sub_layers_minus1);
            if (Archive::isLoading)
            {
                for (UINT32 i = 0; sps_max_sub_layers_minus1 > i; ++i)
                {
                    sps_max_dec_pic_buffering_minus1[i] = sps_max_dec_pic_buffering_minus1[sps_max_sub_layers_minus1];
                    sps_max_num_reorder_pics[i] = sps_max_num_reorder_pics[sps_max_sub_layers_minus1];
                    sps_max_latency_increase_plus1[i] = sps_max_latency_increase_plus1[sps_max_sub_layers_minus1];
                }
            }
        }

        sm & LWP(log2_min_luma_coding_block_size_minus3);
        sm & LWP(log2_diff_max_min_luma_coding_block_size);
        sm & LWP(log2_min_transform_block_size_minus2);
        sm & LWP(log2_diff_max_min_transform_block_size);
        sm & LWP(max_transform_hierarchy_depth_inter);
        sm & LWP(max_transform_hierarchy_depth_intra);
        sm & LWP(scaling_list_enabled_flag);
        if (scaling_list_enabled_flag)
        {
            sm & LWP(sps_scaling_list_data_present_flag);
            if (sps_scaling_list_data_present_flag)
            {
                sm & LWP(scaling_list_data);
            }
        }
        else if (Archive::isLoading)
        {
            sps_scaling_list_data_present_flag = false;
        }

        sm & LWP(amp_enabled_flag);
        sm & LWP(sample_adaptive_offset_enabled_flag);
        sm & LWP(pcm_enabled_flag);
        if (pcm_enabled_flag)
        {
            sm & LWP(pcm_sample_bit_depth_luma_minus1);
            sm & LWP(pcm_sample_bit_depth_chroma_minus1);
            sm & LWP(log2_min_pcm_luma_coding_block_size_minus3);
            sm & LWP(log2_diff_max_min_pcm_luma_coding_block_size);
            sm & LWP(pcm_loop_filter_disabled_flag);
        }
        else if (Archive::isLoading)
        {
            pcm_loop_filter_disabled_flag = false;
        }

        sm & LWP(short_term_ref_pic_sets);
        sm & LWP(long_term_ref_pics_present_flag);
        if (long_term_ref_pics_present_flag)
        {
            sm & LWP(num_long_term_ref_pics_sps);
            MASSERT(maxNumLongTermRefPicsSps >= num_long_term_ref_pics_sps);
            for (UINT32 i = 0; num_long_term_ref_pics_sps > i; ++i)
            {
                if (Archive::isLoading)
                {
                    lt_ref_pic_poc_lsb_sps[i].SetNumBits(log2_max_pic_order_cnt_lsb_minus4 + 4);
                }
                else
                {
                    MASSERT(lt_ref_pic_poc_lsb_sps[i].GetNumBits() ==
                            log2_max_pic_order_cnt_lsb_minus4 + 4);
                }

                sm & LWP1(lt_ref_pic_poc_lsb_sps, i);
                sm & LWP1(used_by_lwrr_pic_lt_sps_flag, i);
            }
        }
        sm & LWP(sps_temporal_mvp_enabled_flag);
        sm & LWP(strong_intra_smoothing_enabled_flag);
        sm & LWP(vui_parameters_present_flag);
        if (vui_parameters_present_flag)
        {
            sm & LWP(vui_parameters);
        }
        sm & LWP(sps_extension_present_flag);
        if (sps_extension_present_flag)
        {
            sm & LWP(sps_range_extension_flag);
            sm & LWP(sps_multilayer_extension_flag);
            sm & LWP(sps_extension_6bits);
        }
        else if (Archive::isLoading)
        {
            sps_range_extension_flag = false;
            sps_multilayer_extension_flag = false;
            sps_extension_6bits = 0;
        }
        if (sps_range_extension_flag)
        {
            sm & LWP(sps_range_extension);
        }
        else if (Archive::isLoading)
        {
            SpsRangeExtension &sre = sps_range_extension;
            sre.transform_skip_rotation_enabled_flag = false;
            sre.transform_skip_context_enabled_flag = false;
            sre.implicit_rdpcm_enabled_flag = false;
            sre.explicit_rdpcm_enabled_flag = false;
            sre.extended_precision_processing_flag = false;
            sre.intra_smoothing_disabled_flag = false;
            sre.high_precision_offsets_enabled_flag = false;
            sre.persistent_rice_adaptation_enabled_flag = false;
            sre.cabac_bypass_alignment_enabled_flag = false;
        }
        if (sps_multilayer_extension_flag)
        {
            sm & LWP(sps_multilayer_extension);
        }
        else if (Archive::isLoading)
        {
            sps_multilayer_extension.inter_view_mv_vert_constraint_flag = false;
        }

        if (Archive::isLoading)
        {
            if (!separate_colour_plane_flag)
            {
                // Table 6-1 of ITU-T H.265
                const static UINT08 subWidthC[]  = {1, 2, 2, 1};
                const static UINT08 subHeightC[] = {1, 2, 1, 1};

                MASSERT(chroma_format_idc < NUMELEMS(subWidthC));
                SubWidthC = subWidthC[chroma_format_idc];
                SubHeightC = subHeightC[chroma_format_idc];
            }
            else
            {
                SubWidthC = SubHeightC = 1;
            }

            MASSERT(log2_max_pic_order_cnt_lsb_minus4 <= 12);
            BitDepthY = static_cast<UINT08>(8 + bit_depth_luma_minus8);                 // (7-4)
            QpBdOffsetY = static_cast<UINT08>(6 * bit_depth_luma_minus8);               // (7-5)
            BitDepthC = static_cast<UINT08>(8 + bit_depth_chroma_minus8);               // (7-6)
            QpBdOffsetC = static_cast<UINT08>(6 * bit_depth_chroma_minus8);             // (7-7)
            MaxPicOrderCntLsb =
                static_cast<UINT08>(1 << (log2_max_pic_order_cnt_lsb_minus4 + 4));      // (7-8)
            MinCbLog2SizeY =
                static_cast<UINT08>(log2_min_luma_coding_block_size_minus3 + 3);        // (7-10)
            CtbLog2SizeY =
                static_cast<UINT08>(MinCbLog2SizeY +
                                    log2_diff_max_min_luma_coding_block_size);          // (7-11)
            MinCbSizeY = 1 << MinCbLog2SizeY;                                           // (7-12)
            CtbSizeY = 1 << CtbLog2SizeY;                                               // (7-13)
            PicWidthInMinCbsY = pic_width_in_luma_samples / MinCbSizeY;                 // (7-14)
            PicWidthInCtbsY = (pic_width_in_luma_samples + CtbSizeY - 1) / CtbSizeY;    // (7-15)
            PicHeightInMinCbsY = pic_height_in_luma_samples / MinCbSizeY;               // (7-16)
            PicHeightInCtbsY = (pic_height_in_luma_samples + CtbSizeY - 1) / CtbSizeY;  // (7-17)
            PicSizeInMinCbsY = PicWidthInMinCbsY * PicHeightInMinCbsY;                  // (7-18)
            PicSizeInCtbsY = PicWidthInCtbsY * PicHeightInCtbsY;                        // (7-19)
            PicSizeInSamplesY = pic_width_in_luma_samples * pic_height_in_luma_samples; // (7-20)
            PicWidthInSamplesC = pic_width_in_luma_samples / SubWidthC;                 // (7-21)
            PicHeightInSamplesC = pic_height_in_luma_samples / SubHeightC;              // (7-21)
            CtbWidthC = CtbSizeY / SubWidthC;                                           // (7-23)
            CtbHeightC = CtbSizeY / SubHeightC;                                         // (7-24)
            PcmBitDepthY = pcm_sample_bit_depth_luma_minus1 + 1;                        // (7-25)
            PcmBitDepthC = pcm_sample_bit_depth_chroma_minus1 + 1;                      // (7-26)
            const SpsRangeExtension &sre = sps_range_extension;
            UINT08 bitsY = sre.extended_precision_processing_flag ? max(15, BitDepthY + 6) : 15;
            UINT08 bitsC = sre.extended_precision_processing_flag ? max(15, BitDepthC + 6) : 15;
            CoeffMinY = -(1 << bitsY);                                                  // (7-27)
            CoeffMinC = -(1 << bitsC);                                                  // (7-28)
            CoeffMaxY = (1 << bitsY) - 1;                                               // (7-29)
            CoeffMaxC = (1 << bitsC) - 1;                                               // (7-30)
            WpOffsetBdShiftY = sre.high_precision_offsets_enabled_flag ? 0 : (BitDepthY - 8);
            WpOffsetBdShiftC = sre.high_precision_offsets_enabled_flag ? 0 : (BitDepthC - 8);
            WpOffsetHalfRangeY = 1 << (sre.high_precision_offsets_enabled_flag ? (BitDepthY - 1) : 7);
            WpOffsetHalfRangeC = 1 << (sre.high_precision_offsets_enabled_flag ? (BitDepthC - 1) : 7);

            m_empty = false;
        }
    }
};

template <class Archive>
void VuiParameters::Serialize(Archive& sm)
{
    sm & LWP(aspect_ratio_info_present_flag);
    if (aspect_ratio_info_present_flag)
    {
        sm & LWP(aspect_ratio_idc);
        if (EXTENDED_SAR == aspect_ratio_idc)
        {
            sm & LWP(sar_width);
            sm & LWP(sar_height);
        }
    }
    sm & LWP(overscan_info_present_flag);
    if (overscan_info_present_flag)
    {
        sm & LWP(overscan_appropriate_flag);
    }
    sm & LWP(video_signal_type_present_flag);
    if (video_signal_type_present_flag)
    {
        sm & LWP(video_format);
        sm & LWP(video_full_range_flag);
        sm & LWP(colour_description_present_flag);
        if (colour_description_present_flag)
        {
            sm & LWP(colour_primaries);
            sm & LWP(transfer_characteristics);
            sm & LWP(matrix_coeffs);
        }
    }
    sm & LWP(chroma_loc_info_present_flag);
    if (chroma_loc_info_present_flag)
    {
        sm & LWP(chroma_sample_loc_type_top_field);
        sm & LWP(chroma_sample_loc_type_bottom_field);
    }
    sm & LWP(neutral_chroma_indication_flag);
    sm & LWP(field_seq_flag);
    sm & LWP(frame_field_info_present_flag);
    sm & LWP(default_display_window_flag);
    if (default_display_window_flag)
    {
        sm & LWP(def_disp_win_left_offset);
        sm & LWP(def_disp_win_right_offset);
        sm & LWP(def_disp_win_top_offset);
        sm & LWP(def_disp_win_bottom_offset);
    }
    sm & LWP(vui_timing_info_present_flag);
    if (vui_timing_info_present_flag)
    {
        sm & LWP(vui_num_units_in_tick);
        sm & LWP(vui_time_scale);
        sm & LWP(vui_poc_proportional_to_timing_flag);
        if (vui_poc_proportional_to_timing_flag)
        {
            sm & LWP(vui_num_ticks_poc_diff_one_minus1);
        }
        sm & LWP(vui_hrd_parameters_present_flag);
        if (vui_hrd_parameters_present_flag)
        {
            hrd_parameters.SetMaxNumSubLayersMinus1(m_srcSps->sps_max_sub_layers_minus1);
            hrd_parameters.SetCommonInfPresentFlag(true);
            sm & LWP(hrd_parameters);
        }
    }
    sm & LWP(bitstream_restriction_flag);
    if (bitstream_restriction_flag)
    {
        sm & LWP(tiles_fixed_structure_flag);
        sm & LWP(motion_vectors_over_pic_boundaries_flag);
        sm & LWP(restricted_ref_pic_lists_flag);
        sm & LWP(min_spatial_segmentation_idc);
        sm & LWP(max_bytes_per_pic_denom);
        sm & LWP(max_bits_per_min_lw_denom);
        sm & LWP(log2_max_mv_length_horizontal);
        sm & LWP(log2_max_mv_length_vertical);
    }
}

class PpsRangeExtension
{
    template <class T> friend class PpsRangeExtensionField;

    PpsRangeExtension(const PicParameterSet *srcPps)
      : m_srcPps(srcPps)
    {}

    static const size_t maxChromaQpOffsetListLen = 6;

public:
    Ue       log2_max_transform_skip_block_size_minus2;
    NBits<1> cross_component_prediction_enabled_flag;
    NBits<1> chroma_qp_offset_list_enabled_flag;
    Ue       diff_lw_chroma_qp_offset_depth;
    Ue       chroma_qp_offset_list_len_minus1;
    Se       cb_qp_offset_list[maxChromaQpOffsetListLen];
    Se       cr_qp_offset_list[maxChromaQpOffsetListLen];
    Ue       log2_sao_offset_scale_luma;
    Ue       log2_sao_offset_scale_chroma;

    template <class Archive>
    void Serialize(Archive& sm);

private:
    const PicParameterSet *m_srcPps;
};

template <class T>
class PpsRangeExtensionField
{
public:
    PpsRangeExtension pps_range_extension;

protected:
    PpsRangeExtensionField()
      : pps_range_extension(This())
    {}

    PpsRangeExtensionField(const PpsRangeExtensionField& rhs)
      : pps_range_extension(rhs.pps_range_extension)
    {
        pps_range_extension.m_srcPps = This();
    }

    PpsRangeExtensionField& operator=(const PpsRangeExtensionField &rhs)
    {
        if (this != &rhs)
        {
            pps_range_extension = rhs.pps_range_extension;
            pps_range_extension.m_srcPps = This();
        }

        return *this;
    }

private:
    const T* This() const
    {
        return static_cast<const T*>(this);
    }
};

class PpsMultilayerExtension
{
public:
    template <class Archive>
    void Serialize(Archive& sm)
    {
        // not supported
    }
};

class PicParameterSet : public PpsRangeExtensionField<PicParameterSet>
{
    template <class T> friend class PPSContainer;
    template <class T> friend class PPSField;
    friend class Picture;
public:
    PicParameterSet()
      : PpsRangeExtensionField<PicParameterSet>()
      , m_spsSrc(nullptr)
      , m_empty(true)
    {}

    explicit PicParameterSet(const SeqParamSetSrc *src)
      : PpsRangeExtensionField<PicParameterSet>()
      , m_spsSrc(src)
      , m_empty(true)
    {}

    const SeqParameterSet* GetSPS() const
    {
        if (m_spsSrc)
        {
            return &m_spsSrc->GetSPS(pps_seq_parameter_set_id);
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
    void SetSeqParamSrc(const SeqParamSetSrc *src)
    {
        m_spsSrc = src;
    }

    const SeqParamSetSrc *m_spsSrc;
    bool                  m_empty;

public:
    Ue                     pps_pic_parameter_set_id;
    Ue                     pps_seq_parameter_set_id;
    NBits<1>               dependent_slice_segments_enabled_flag;
    NBits<1>               output_flag_present_flag;
    NBits<3>               num_extra_slice_header_bits;
    NBits<1>               sign_data_hiding_enabled_flag;
    NBits<1>               cabac_init_present_flag;
    Ue                     num_ref_idx_l0_default_active_minus1;
    Ue                     num_ref_idx_l1_default_active_minus1;
    Se                     init_qp_minus26;
    NBits<1>               constrained_intra_pred_flag;
    NBits<1>               transform_skip_enabled_flag;
    NBits<1>               lw_qp_delta_enabled_flag;
    Ue                     diff_lw_qp_delta_depth;
    Se                     pps_cb_qp_offset;
    Se                     pps_cr_qp_offset;
    NBits<1>               pps_slice_chroma_qp_offsets_present_flag;
    NBits<1>               weighted_pred_flag;
    NBits<1>               weighted_bipred_flag;
    NBits<1>               transquant_bypass_enabled_flag;
    NBits<1>               tiles_enabled_flag;
    NBits<1>               entropy_coding_sync_enabled_flag;
    Ue                     num_tile_columns_minus1;
    Ue                     num_tile_rows_minus1;
    NBits<1>               uniform_spacing_flag;
    vector<Ue>             column_width_minus1;
    vector<Ue>             row_height_minus1;
    NBits<1>               loop_filter_across_tiles_enabled_flag;
    NBits<1>               pps_loop_filter_across_slices_enabled_flag;
    NBits<1>               deblocking_filter_control_present_flag;
    NBits<1>               deblocking_filter_override_enabled_flag;
    NBits<1>               pps_deblocking_filter_disabled_flag;
    Se                     pps_beta_offset_div2;
    Se                     pps_tc_offset_div2;
    NBits<1>               pps_scaling_list_data_present_flag;
    ScalingListData        scaling_list_data;
    NBits<1>               lists_modification_present_flag;
    Ue                     log2_parallel_merge_level_minus2;
    NBits<1>               slice_segment_header_extension_present_flag;
    NBits<1>               pps_extension_present_flag;
    NBits<1>               pps_range_extension_flag;
    NBits<1>               pps_multilayer_extension_flag;
    NBits<1>               pps_extension_6bits;
    PpsMultilayerExtension pps_multilayer_extension;

    UINT08                 Log2MaxTransformSkipSize;    // (7-37) of ITU-T H.265
    UINT08                 Log2MinLwChromaQpOffsetSize; // (7-38) of ITU-T H.265

    template <class Archive>
    void Serialize(Archive& sm)
    {
        SerializeCommon(sm);
    }

#if defined(SUPPORT_TEXT_OUTPUT)
    void Serialize(H26X::TextOArchive& sm);
#endif

    template <class Archive>
    void SerializeCommon(Archive& sm)
    {
        if (Archive::isSaving)
        {
            if (m_empty)
            {
                return;
            }
        }

        sm & LWP(pps_pic_parameter_set_id);
        sm & LWP(pps_seq_parameter_set_id);
        sm & LWP(dependent_slice_segments_enabled_flag);
        sm & LWP(output_flag_present_flag);
        sm & LWP(num_extra_slice_header_bits);
        sm & LWP(sign_data_hiding_enabled_flag);
        sm & LWP(cabac_init_present_flag);
        sm & LWP(num_ref_idx_l0_default_active_minus1);
        sm & LWP(num_ref_idx_l1_default_active_minus1);
        sm & LWP(init_qp_minus26);
        sm & LWP(constrained_intra_pred_flag);
        sm & LWP(transform_skip_enabled_flag);
        sm & LWP(lw_qp_delta_enabled_flag);
        if (lw_qp_delta_enabled_flag)
        {
            sm & LWP(diff_lw_qp_delta_depth);
        }
        else if (Archive::isLoading)
        {
            diff_lw_qp_delta_depth = 0;
        }

        sm & LWP(pps_cb_qp_offset);
        sm & LWP(pps_cr_qp_offset);
        sm & LWP(pps_slice_chroma_qp_offsets_present_flag);
        sm & LWP(weighted_pred_flag);
        sm & LWP(weighted_bipred_flag);
        sm & LWP(transquant_bypass_enabled_flag);
        sm & LWP(tiles_enabled_flag);
        sm & LWP(entropy_coding_sync_enabled_flag);
        if (tiles_enabled_flag)
        {
            sm & LWP(num_tile_columns_minus1);
            sm & LWP(num_tile_rows_minus1);
            sm & LWP(uniform_spacing_flag);
            if (!uniform_spacing_flag)
            {
                if (Archive::isLoading)
                {
                    column_width_minus1.resize(num_tile_columns_minus1);
                    row_height_minus1.resize(num_tile_rows_minus1);
                }
                else
                {
                    MASSERT(column_width_minus1.size() == num_tile_columns_minus1);
                    MASSERT(row_height_minus1.size() == num_tile_rows_minus1);
                }
                for (UINT32 i = 0; num_tile_columns_minus1 > i; ++i)
                {
                    sm & LWP1(column_width_minus1, i);
                }
                for (UINT32 i = 0; num_tile_rows_minus1 > i; ++i)
                {
                    sm & LWP1(row_height_minus1, i);
                }
            }
            sm & LWP(loop_filter_across_tiles_enabled_flag);
        }
        else if (Archive::isLoading)
        {
            num_tile_columns_minus1 = 0;
            num_tile_rows_minus1 = 0;
            uniform_spacing_flag = true;
            loop_filter_across_tiles_enabled_flag = true;
        }

        sm & LWP(pps_loop_filter_across_slices_enabled_flag);
        sm & LWP(deblocking_filter_control_present_flag);
        if (deblocking_filter_control_present_flag)
        {
            sm & LWP(deblocking_filter_override_enabled_flag);
            sm & LWP(pps_deblocking_filter_disabled_flag);
            if (!pps_deblocking_filter_disabled_flag)
            {
                sm & LWP(pps_beta_offset_div2);
                sm & LWP(pps_tc_offset_div2);
            }
        }
        else if (Archive::isLoading)
        {
            deblocking_filter_override_enabled_flag = false;
            pps_deblocking_filter_disabled_flag = false;
        }

        sm & LWP(pps_scaling_list_data_present_flag);
        if (pps_scaling_list_data_present_flag)
        {
            sm & LWP(scaling_list_data);
        }
        sm & LWP(lists_modification_present_flag);
        sm & LWP(log2_parallel_merge_level_minus2);
        sm & LWP(slice_segment_header_extension_present_flag);
        sm & LWP(pps_extension_present_flag);
        if (pps_extension_present_flag)
        {
            sm & LWP(pps_range_extension_flag);
            sm & LWP(pps_multilayer_extension_flag);
            sm & LWP(pps_extension_6bits);
        }
        else if (Archive::isLoading)
        {
            pps_range_extension_flag = false;
            pps_multilayer_extension_flag = false;
            pps_extension_6bits = 0;
        }
        if (pps_range_extension_flag)
        {
            sm & LWP(pps_range_extension);
        }
        else if (Archive::isLoading)
        {
            PpsRangeExtension &pre = pps_range_extension;
            pre.log2_max_transform_skip_block_size_minus2 = 0;
            pre.cross_component_prediction_enabled_flag = false;
            pre.log2_sao_offset_scale_luma = 0;
            pre.log2_sao_offset_scale_chroma = 0;
        }
        if (pps_multilayer_extension_flag)
        {
            sm & LWP(pps_multilayer_extension);
        }
        else if (Archive::isLoading)
        {
            // defaults for pps_multilayer_extension
        }

        if (Archive::isLoading)
        {
            const PpsRangeExtension &pre = pps_range_extension;
            Log2MaxTransformSkipSize =
                static_cast<UINT08>(pre.log2_max_transform_skip_block_size_minus2 + 2);
            Log2MinLwChromaQpOffsetSize =
                static_cast<UINT08>(GetSPS()->CtbLog2SizeY - pre.diff_lw_chroma_qp_offset_depth);

            m_empty = false;
        }
    }
};

template <class Archive>
void PpsRangeExtension::Serialize(Archive& sm)
{
    if (m_srcPps->transform_skip_enabled_flag)
    {
        sm & LWP(log2_max_transform_skip_block_size_minus2);
    }
    else if (Archive::isLoading)
    {
        log2_max_transform_skip_block_size_minus2 = 0;
    }
    sm & LWP(cross_component_prediction_enabled_flag);
    sm & LWP(chroma_qp_offset_list_enabled_flag);
    if (chroma_qp_offset_list_enabled_flag)
    {
        sm & LWP(diff_lw_chroma_qp_offset_depth);
        sm & LWP(chroma_qp_offset_list_len_minus1);
        for (UINT32 i = 0; chroma_qp_offset_list_len_minus1 >= i; ++i)
        {
            MASSERT(i < maxChromaQpOffsetListLen);
            sm & LWP1(cb_qp_offset_list, i);
            sm & LWP1(cr_qp_offset_list, i);
        }
    }
    sm & LWP(log2_sao_offset_scale_luma);
    sm & LWP(log2_sao_offset_scale_chroma);;
}

class NALUData
{
public:
    typedef vector<UINT08>::const_iterator RBSPIterator;
    typedef vector<UINT08>::const_iterator EBSPIterator;
protected:
    vector<UINT08> m_rbspData;
    vector<UINT08> m_ebspData;
};

//! An H.265 NAL (Network Abstraction Layer) unit of data.
class NALU : public NALUData
{
    static const unsigned int startCodeSize = 4;

    bool     m_valid;
    size_t   m_offset;

    NBits<6> nal_unit_type;
    NBits<6> nuh_layer_id;
    NBits<3> nuh_temporal_id_plus1;

public:
    NALU()
      : m_valid(false)
    {}

    template <class InputIterator>
    void InitFromEBSP(InputIterator start, InputIterator finish, size_t offset)
    {
        m_offset = offset;

        // add start code first
        m_ebspData.assign(3, 0);
        m_ebspData.insert(m_ebspData.end(), 1, 1);

        m_ebspData.insert(m_ebspData.end(), start, finish);

        BitIArchive<vector<UINT08>::const_iterator> ia(
            m_ebspData.begin() + startCodeSize,
            m_ebspData.end()
        );

        NBits<1> forbidden_zero_bit;
        ia >> forbidden_zero_bit;

        m_valid = !forbidden_zero_bit;
        if (m_valid)
        {
            ia >> nal_unit_type >> nuh_layer_id >> nuh_temporal_id_plus1;
            size_t rbspStart = static_cast<size_t>(ia.GetLwrrentOffset() / 8);
            m_rbspData.assign(m_ebspData.begin() + rbspStart + startCodeSize, m_ebspData.end());
            if (m_rbspData.size() > 3)
            {
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

    NalUnitType GetUnitType() const
    {
        return nal_unit_type;
    }

    UINT08 GetLayerId() const
    {
        return nuh_layer_id;
    }

    UINT08 GetTemporalId() const
    {
        return nuh_temporal_id_plus1 - 1;
    }

    bool IsValid() const
    {
        return m_valid;
    }

    void CreateDataFromVPS(const VidParameterSet &vps, UINT32 layerId, UINT32 temporalId);
    void CreateDataFromSPS(const SeqParameterSet &sps, UINT32 layerId, UINT32 temporalId);
    void CreateDataFromPPS(const PicParameterSet &pps, UINT32 layerId, UINT32 temporalId);

private:
    template <class T>
    void CreateDataFrom(const T &t, NALUType naluType, UINT32 layerId, UINT32 temporalId);
};

inline
UINT08 CeilLog2(UINT32 num)
{
    UINT08 bits = 1;
    while (num > (1ul << bits))
    {
        ++bits;
    }

    return bits;
}

class SliceSegmentHeader;
class RefPicListsModification
{
    template <class T> friend class RefPicListsModificationField;

    RefPicListsModification(const SliceSegmentHeader *sliceHeader)
      : m_sliceHeader(sliceHeader)
      , ref_pic_list_modification_flag_l0(false)
      , ref_pic_list_modification_flag_l1(false)
    {}

    const SliceSegmentHeader *m_sliceHeader;

public:
    NBits<1>    ref_pic_list_modification_flag_l0;
    Num<UINT32> list_entry_l0[32];
    NBits<1>    ref_pic_list_modification_flag_l1;
    Num<UINT32> list_entry_l1[32];

    template <class Archive>
    void Serialize(Archive& sm);
};

template <class T>
class RefPicListsModificationField
{
protected:
    RefPicListsModificationField()
      : ref_pic_lists_modification(This())
    {}

    RefPicListsModificationField(const RefPicListsModificationField& rhs)
      : ref_pic_lists_modification(rhs.ref_pic_lists_modification)
    {
        ref_pic_lists_modification.m_sliceHeader = This();
    }

    RefPicListsModificationField& operator=(const RefPicListsModificationField &rhs)
    {
        if (this != &rhs)
        {
            ref_pic_lists_modification = rhs.ref_pic_lists_modification;
            ref_pic_lists_modification.m_sliceHeader = This();
        }

        return *this;
    }

public:
    RefPicListsModification ref_pic_lists_modification;

private:
    const T* This() const
    {
        return static_cast<const T*>(this);
    }
};

class PredWeightTable
{
    template <class T> friend class PredWeightTableField;

    PredWeightTable(const SliceSegmentHeader *sliceHeader)
      : m_sliceHeader(sliceHeader)
    {}

    const SliceSegmentHeader *m_sliceHeader;

public:
    Ue       luma_log2_weight_denom;
    Se       delta_chroma_log2_weight_denom;
    NBits<1> luma_weight_l0_flag[maxNumRefPics];
    NBits<1> chroma_weight_l0_flag[maxNumRefPics];
    Se       delta_luma_weight_l0[maxNumRefPics];
    Se       luma_offset_l0[maxNumRefPics];
    Se       delta_chroma_weight_l0[maxNumRefPics][2];
    Se       delta_chroma_offset_l0[maxNumRefPics][2];
    NBits<1> luma_weight_l1_flag[maxNumRefPics];
    NBits<1> chroma_weight_l1_flag[maxNumRefPics];
    Se       delta_luma_weight_l1[maxNumRefPics];
    Se       luma_offset_l1[maxNumRefPics];
    Se       delta_chroma_weight_l1[maxNumRefPics][2];
    Se       delta_chroma_offset_l1[maxNumRefPics][2];

    template <class Archive>
    void Serialize(Archive& sm);
};

template <class T>
class PredWeightTableField
{
protected:
    PredWeightTableField()
      : pred_weight_table(This())
    {}

    PredWeightTableField(const PredWeightTableField& rhs)
      : pred_weight_table(rhs.pred_weight_table)
    {
        pred_weight_table.m_sliceHeader = This();
    }

    PredWeightTableField& operator=(const PredWeightTableField &rhs)
    {
        if (this != &rhs)
        {
            pred_weight_table = rhs.pred_weight_table;
            pred_weight_table.m_sliceHeader = This();
        }

        return *this;
    }

    PredWeightTable pred_weight_table;

private:
    const T* This() const
    {
        return static_cast<const T*>(this);
    }
};

class SliceSegmentHeader :
    public RefPicListsModificationField<SliceSegmentHeader>
  , public PredWeightTableField<SliceSegmentHeader>
{
    friend class PredWeightTable;
    friend class Slice;

public:
    typedef ShortTermRefPicSetCont::Set ShortTermRefPicSet;

    SliceSegmentHeader(
        NALU &nalu,
        const VidParamSetSrc *vidParamSrc,
        const SeqParamSetSrc *seqParamSrc,
        const PicParamSetSrc *picParamSrc
    );

    const ShortTermRefPicSet *GetLwrrRps() const
    {
        const ShortTermRefPicSet *lwrrRps = nullptr;

        const SeqParameterSet &sps = GetSPS();
        if (LwrrRpsIdx == sps.GetNumShortTermRefPicSets())
        {
            lwrrRps = &short_term_ref_pic_set;
        }
        else
        {
            lwrrRps = &sps.short_term_ref_pic_sets.GetShortTermRefPicSet(LwrrRpsIdx);
        }

        return lwrrRps;
    }

private:
    const VidParamSetSrc* GetVidParamSrc() const
    {
        return m_vidParamSrc;
    }

    void SetVidParamSrc(const VidParamSetSrc *src)
    {
        m_vidParamSrc = src;
    }

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

    const VidParamSetSrc  *m_vidParamSrc;
    const SeqParamSetSrc  *m_seqParamSrc;
    const PicParamSetSrc  *m_picParamSrc;

#if defined(SUPPORT_TEXT_OUTPUT)
    size_t                 m_offset;
#endif

public:
    NBits<1>             first_slice_segment_in_pic_flag;
    NBits<1>             no_output_of_prior_pics_flag;
    Ue                   slice_pic_parameter_set_id;
    NBits<1>             dependent_slice_segment_flag;
    Num<UINT32>          slice_segment_address;
    Ue                   slice_type;
    NBits<1>             pic_output_flag;
    NBits<2>             colour_plane_id;
    Num<UINT32>          slice_pic_order_cnt_lsb;
    NBits<1>             short_term_ref_pic_set_sps_flag;
    ShortTermRefPicSet   short_term_ref_pic_set;
    Num<UINT32>          short_term_ref_pic_set_idx;
    Ue                   num_long_term_sps;
    Ue                   num_long_term_pics;
    Num<UINT32>          lt_idx_sps[maxNumLongTermRefPicsSps];
    Num<UINT32>          poc_lsb_lt[maxNumLongTermRefPicsSps];
    NBits<1>             used_by_lwrr_pic_lt_flag[maxNumLongTermRefPicsSps];
    NBits<1>             delta_poc_msb_present_flag[maxNumLongTermRefPicsSps];
    Ue                   delta_poc_msb_cycle_lt[maxNumLongTermRefPicsSps];
    NBits<1>             slice_temporal_mvp_enabled_flag;
    NBits<1>             slice_sao_luma_flag;
    NBits<1>             slice_sao_chroma_flag;
    NBits<1>             num_ref_idx_active_override_flag;
    Ue                   num_ref_idx_l0_active_minus1;
    Ue                   num_ref_idx_l1_active_minus1;
    NBits<1>             mvd_l1_zero_flag;
    NBits<1>             cabac_init_flag;
    NBits<1>             collocated_from_l0_flag;
    Ue                   collocated_ref_idx;
    Ue                   five_minus_max_num_merge_cand;
    Se                   slice_qp_delta;
    Se                   slice_cb_qp_offset;
    Se                   slice_cr_qp_offset;
    NBits<1>             deblocking_filter_override_flag;
    NBits<1>             slice_deblocking_filter_disabled_flag;
    Se                   slice_beta_offset_div2;
    Se                   slice_tc_offset_div2;
    NBits<1>             slice_loop_filter_across_slices_enabled_flag;
    Ue                   num_entry_point_offsets;
    Ue                   offset_len_minus1;
    vector<Num<UINT32> > entry_point_offset_minus1;
    Ue                   slice_segment_header_extension_length;
    vector<NBits<8> >    slice_segment_header_extension_data_byte;

    UINT08               TemporalId;
    UINT32               LwrrRpsIdx;
    UINT08               NumPocTotalLwrr;
    bool                 UsedByLwrrPicLt[maxNumRefPics];
    UINT32               PocLsbLt[maxNumRefPics];
    UINT32               DeltaPocMsbCycleLt[maxNumRefPics];
    unsigned int         sw_hdr_skip_length;

    NalUnitType          nal_unit_type;
#if defined(SUPPORT_TEXT_OUTPUT)
    UINT32               nuh_layer_id;
#endif

#if defined(SUPPORT_TEXT_OUTPUT)
    size_t GetOffset() const
    {
        return m_offset;
    }
#endif

    const VidParameterSet& GetVPS() const
    {
        MASSERT(nullptr != m_vidParamSrc);
        MASSERT(!m_vidParamSrc->GetVPS(GetSPS().sps_video_parameter_set_id).IsEmpty());
        return m_vidParamSrc->GetVPS(GetSPS().sps_video_parameter_set_id);
    }

    const SeqParameterSet& GetSPS() const
    {
        MASSERT(nullptr != m_seqParamSrc);
        MASSERT(!m_seqParamSrc->GetSPS(GetPPS().pps_seq_parameter_set_id).IsEmpty());
        return m_seqParamSrc->GetSPS(GetPPS().pps_seq_parameter_set_id);
    }

    const PicParameterSet& GetPPS() const
    {
        MASSERT(nullptr != m_picParamSrc);
        MASSERT(!m_picParamSrc->GetPPS(slice_pic_parameter_set_id).IsEmpty());
        return m_picParamSrc->GetPPS(slice_pic_parameter_set_id);
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

    template <class Archive>
    void Serialize(Archive& sm)
    {
        SerializeCommon(sm);
    }

#if defined(SUPPORT_TEXT_OUTPUT)
    void Serialize(H26X::TextOArchive& sm);
#endif

    const UINT08* GetEbspPayload() const
    {
        return &m_ebspPayload[0];
    }

    size_t GetEbspSize() const
    {
        return m_ebspPayload.size();
    }

    vector<UINT08>::iterator ebsp_begin()
    {
        return m_ebspPayload.begin();
    }

    vector<UINT08>::const_iterator ebsp_begin() const
    {
        return m_ebspPayload.begin();
    }

    vector<UINT08>::iterator ebsp_end()
    {
        return m_ebspPayload.end();
    }

    vector<UINT08>::const_iterator ebsp_end() const
    {
        return m_ebspPayload.end();
    }

private:
    template <class Archive>
    void StartCounting(Archive& sm)
    {}

    template <class Archive>
    void StopCounting(Archive& sm)
    {}

    template <class InputIterator>
    void StartCounting(H26X::BitIArchive<InputIterator>& sm)
    {
        sw_hdr_skip_length = static_cast<unsigned int>(sm.GetLwrrentOffset());
    }

    template <class InputIterator>
    void StopCounting(H26X::BitIArchive<InputIterator>& sm)
    {
        sw_hdr_skip_length = static_cast<unsigned int>(sm.GetLwrrentOffset()) - sw_hdr_skip_length;
    }

    template <class Archive>
    void SerializeCommon(Archive& sm)
    {
        if (Archive::isLoading)
        {
            NumPocTotalLwrr = 0;
        }
        sm & LWP(first_slice_segment_in_pic_flag);
        if (IsIRAP())
        {
            sm & LWP(no_output_of_prior_pics_flag);
        }
        sm & LWP(slice_pic_parameter_set_id);

        const SeqParameterSet &sps = GetSPS();
        const PicParameterSet &pps = GetPPS();

        if (!first_slice_segment_in_pic_flag)
        {
            if (pps.dependent_slice_segments_enabled_flag)
            {
                sm & LWP(dependent_slice_segment_flag);
            }
            else if (Archive::isLoading)
            {
                dependent_slice_segment_flag = false;
            }
            slice_segment_address.SetNumBits(CeilLog2(sps.PicSizeInCtbsY));
            sm & LWP(slice_segment_address);
        }
        else if (Archive::isLoading)
        {
            slice_segment_address = 0;
        }

        if (!dependent_slice_segment_flag)
        {
            if (0 < pps.num_extra_slice_header_bits)
            {
                Num<UINT08> slice_reserved_flag;
                slice_reserved_flag.SetNumBits(GetPPS().num_extra_slice_header_bits);
                sm & LWP(slice_reserved_flag);
            }
            sm & LWP(slice_type);

            StartCounting(sm);

            if (pps.output_flag_present_flag)
            {
                sm & LWP(pic_output_flag);
            }
            else if (Archive::isLoading)
            {
                pic_output_flag = true;
            }
            if (sps.separate_colour_plane_flag)
            {
                sm & LWP(colour_plane_id);
            }
            if (!IsIDR())
            {
                slice_pic_order_cnt_lsb.SetNumBits(sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
                sm & LWP(slice_pic_order_cnt_lsb);
                sm & LWP(short_term_ref_pic_set_sps_flag);
                if (!short_term_ref_pic_set_sps_flag)
                {
                    short_term_ref_pic_set.SetRefPicSetsSrc(&sps.short_term_ref_pic_sets);
                    short_term_ref_pic_set.SetIdx(sps.GetNumShortTermRefPicSets());
                    sm & LWP(short_term_ref_pic_set);

                    if (Archive::isLoading)
                        LwrrRpsIdx = sps.GetNumShortTermRefPicSets();
                }
                else
                {
                    if (sps.GetNumShortTermRefPicSets() > 1)
                    {
                        short_term_ref_pic_set_idx.SetNumBits(CeilLog2(sps.GetNumShortTermRefPicSets()));
                        sm & LWP(short_term_ref_pic_set_idx);
                    }
                    else if (Archive::isLoading)
                    {
                        short_term_ref_pic_set_idx = 0;
                    }
                    if (Archive::isLoading)
                        LwrrRpsIdx = short_term_ref_pic_set_idx;
                }

                if (sps.long_term_ref_pics_present_flag)
                {
                    if (0 < sps.num_long_term_ref_pics_sps)
                    {
                        sm & LWP(num_long_term_sps);
                    }
                    else if (Archive::isLoading)
                    {
                        num_long_term_sps = 0;
                    }
                    sm & LWP(num_long_term_pics);
                    UINT32 totLongTermRefPics = num_long_term_sps + num_long_term_pics;
                    for (UINT32 i = 0; totLongTermRefPics > i; ++i)
                    {
                        if (num_long_term_sps > i)
                        {
                            if (sps.num_long_term_ref_pics_sps > 1)
                            {
                                lt_idx_sps[i].SetNumBits(CeilLog2(sps.num_long_term_ref_pics_sps));
                                sm & LWP1(lt_idx_sps, i);
                            }
                            else if (Archive::isLoading)
                            {
                                lt_idx_sps[i] = 0;
                            }
                            if (Archive::isLoading)
                            {
                                PocLsbLt[i] = sps.lt_ref_pic_poc_lsb_sps[lt_idx_sps[i]];
                                UsedByLwrrPicLt[i] = sps.used_by_lwrr_pic_lt_sps_flag[lt_idx_sps[i]];
                            }
                        }
                        else
                        {
                            poc_lsb_lt[i].SetNumBits(sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
                            sm & LWP1(poc_lsb_lt, i);
                            sm & LWP1(used_by_lwrr_pic_lt_flag, i);
                            if (Archive::isLoading)
                            {
                                PocLsbLt[i] = poc_lsb_lt[i];
                                UsedByLwrrPicLt[i] = used_by_lwrr_pic_lt_flag[i];
                            }
                        }
                        sm & LWP1(delta_poc_msb_present_flag, i);
                        if (delta_poc_msb_present_flag[i])
                        {
                            sm & LWP1(delta_poc_msb_cycle_lt, i);
                        }
                        else if (Archive::isLoading)
                        {
                            delta_poc_msb_cycle_lt[i] = 0;
                        }
                        if (Archive::isLoading)
                        {
                            if (0 == i || num_long_term_sps == i)
                            {
                                DeltaPocMsbCycleLt[i] = delta_poc_msb_cycle_lt[i];
                            }
                            else
                            {
                                DeltaPocMsbCycleLt[i] = delta_poc_msb_cycle_lt[i] +
                                                        DeltaPocMsbCycleLt[i - 1];
                            }
                        }
                    }
                }
                else if (Archive::isLoading)
                {
                    num_long_term_pics = 0;
                }

                if (Archive::isLoading)
                {
                    // Equation 7-43 of ITU-T H.265
                    const ShortTermRefPicSet *lwrrRps = GetLwrrRps();
                    for (UINT32 i = 0; lwrrRps->NumNegativePics > i; ++i)
                        if (lwrrRps->UsedByLwrrPicS0[i]) ++NumPocTotalLwrr;
                    for (UINT32 i = 0; lwrrRps->NumPositivePics > i; ++i)
                        if (lwrrRps->UsedByLwrrPicS1[i]) ++NumPocTotalLwrr;
                    UINT32 totLongTermRefPics = num_long_term_sps + num_long_term_pics;
                    for (UINT32 i = 0; totLongTermRefPics > i; ++i)
                        if (UsedByLwrrPicLt[i])
                            ++NumPocTotalLwrr;
                    // End of equation 7-43 of ITU-T H.265

                    if (IsCRA() || IsBLA())
                    {
                        MASSERT(0 == NumPocTotalLwrr);
                    }
                }

                StopCounting(sm);

                if (sps.sps_temporal_mvp_enabled_flag)
                {
                    sm & LWP(slice_temporal_mvp_enabled_flag);
                }
                else if (Archive::isLoading)
                {
                    slice_temporal_mvp_enabled_flag = false;
                }
            }
            else
            {
                StopCounting(sm);
                if (Archive::isLoading)
                {
                    // It's equal 0 for IDR pictures, except for the case of
                    // generating unavailable reference pictures decoding process,
                    // which is described in 8.3.3 of ITU-T H.265 and we do not
                    // support.
                    slice_pic_order_cnt_lsb = 0;
                }
            }

            if (sps.sample_adaptive_offset_enabled_flag)
            {
                sm & LWP(slice_sao_luma_flag);
                sm & LWP(slice_sao_chroma_flag);
            }
            else if (Archive::isLoading)
            {
                slice_sao_luma_flag = false;
                slice_sao_chroma_flag = false;
            }
            if (SLICE_TYPE_P == slice_type || SLICE_TYPE_B == slice_type)
            {
                sm & LWP(num_ref_idx_active_override_flag);
                if (num_ref_idx_active_override_flag)
                {
                    sm & LWP(num_ref_idx_l0_active_minus1);
                    if (SLICE_TYPE_B == slice_type)
                    {
                        sm & LWP(num_ref_idx_l1_active_minus1);
                    }
                }
                else if (Archive::isLoading)
                {
                    num_ref_idx_l0_active_minus1 = pps.num_ref_idx_l0_default_active_minus1;
                    if (SLICE_TYPE_B == slice_type)
                    {
                        num_ref_idx_l1_active_minus1 = pps.num_ref_idx_l1_default_active_minus1;
                    }
                }
                if (pps.lists_modification_present_flag && 1 < NumPocTotalLwrr)
                {
                    sm & LWP(ref_pic_lists_modification);
                }
                if (SLICE_TYPE_B == slice_type)
                {
                    sm & LWP(mvd_l1_zero_flag);
                }
                if (pps.cabac_init_present_flag)
                {
                    sm & LWP(cabac_init_flag);
                }
                else if (Archive::isLoading)
                {
                    cabac_init_flag = 0;
                }
                if (slice_temporal_mvp_enabled_flag)
                {
                    if (SLICE_TYPE_B == slice_type)
                    {
                        sm & LWP(collocated_from_l0_flag);
                    }
                    else if (Archive::isLoading)
                    {
                        collocated_from_l0_flag = true;
                    }
                    if ((collocated_from_l0_flag && 0 < num_ref_idx_l0_active_minus1) ||
                        (!collocated_from_l0_flag && 0 < num_ref_idx_l1_active_minus1))
                    {
                        sm & LWP(collocated_ref_idx);
                    }
                }
                if ((pps.weighted_pred_flag && SLICE_TYPE_P == slice_type) ||
                    (pps.weighted_bipred_flag && SLICE_TYPE_B == slice_type))
                {
                    sm & LWP(pred_weight_table);
                }
                sm & LWP(five_minus_max_num_merge_cand);
            }
            sm & LWP(slice_qp_delta);
            if (pps.pps_slice_chroma_qp_offsets_present_flag)
            {
                sm & LWP(slice_cb_qp_offset);
                sm & LWP(slice_cr_qp_offset);
            }
            else if (Archive::isLoading)
            {
                slice_cb_qp_offset = 0;
                slice_cr_qp_offset = 0;
            }
            if (pps.deblocking_filter_override_enabled_flag)
            {
                sm & LWP(deblocking_filter_override_flag);
            }
            else if (Archive::isLoading)
            {
                deblocking_filter_override_flag = false;
            }
            if (deblocking_filter_override_flag)
            {
                sm & LWP(slice_deblocking_filter_disabled_flag);
                if (!slice_deblocking_filter_disabled_flag)
                {
                    sm & LWP(slice_beta_offset_div2);
                    sm & LWP(slice_tc_offset_div2);
                }
            }
            else if (Archive::isLoading)
            {
                slice_deblocking_filter_disabled_flag = pps.pps_deblocking_filter_disabled_flag;
            }
            if (pps.pps_loop_filter_across_slices_enabled_flag &&
                (slice_sao_luma_flag || slice_sao_chroma_flag ||
                 !slice_deblocking_filter_disabled_flag))
            {
                sm & LWP(slice_loop_filter_across_slices_enabled_flag);
            }
            else if (Archive::isLoading)
            {
                slice_loop_filter_across_slices_enabled_flag = pps.pps_loop_filter_across_slices_enabled_flag;
            }
        } // !dependent_slice_segment_flag
        if (pps.tiles_enabled_flag || pps.entropy_coding_sync_enabled_flag)
        {
            sm & LWP(num_entry_point_offsets);
            if (0 < num_entry_point_offsets)
            {
                if (Archive::isLoading)
                {
                    entry_point_offset_minus1.resize(num_entry_point_offsets);
                }
                else
                {
                    MASSERT(entry_point_offset_minus1.size() == num_entry_point_offsets);
                }
                sm & LWP(offset_len_minus1);
                for (UINT32 i = 0; num_entry_point_offsets > i; ++i)
                {
                    entry_point_offset_minus1[i].SetNumBits(offset_len_minus1 + 1);
                    sm & LWP1(entry_point_offset_minus1, i);
                }
            }
        }
        else if (Archive::isLoading)
        {
            num_entry_point_offsets = 0;
        }
        if (pps.slice_segment_header_extension_present_flag)
        {
            sm & LWP(slice_segment_header_extension_length);
            if (Archive::isLoading)
            {
                slice_segment_header_extension_data_byte.resize(slice_segment_header_extension_length);
            }
            else
            {
                MASSERT(slice_segment_header_extension_data_byte.size() == slice_segment_header_extension_length);
            }
            for (UINT32 i = 0; slice_segment_header_extension_length > i; ++i)
            {
                sm & LWP1(slice_segment_header_extension_data_byte, i);
            }
        }
    }

    vector<UINT08> m_ebspPayload;
};

template <class Archive>
void RefPicListsModification::Serialize(Archive& sm)
{
    sm & LWP(ref_pic_list_modification_flag_l0);
    if (ref_pic_list_modification_flag_l0)
    {
        for (UINT32 i = 0; m_sliceHeader->num_ref_idx_l0_active_minus1 >= i; ++i)
        {
            list_entry_l0[i].SetNumBits(CeilLog2(m_sliceHeader->NumPocTotalLwrr));
            sm & LWP1(list_entry_l0, i);
        }
    }
    if (SLICE_TYPE_B == m_sliceHeader->slice_type)
    {
        sm & LWP(ref_pic_list_modification_flag_l1);
        if (ref_pic_list_modification_flag_l1)
        {
            for (UINT32 i = 0; m_sliceHeader->num_ref_idx_l1_active_minus1 >= i; ++i)
            {
                list_entry_l1[i].SetNumBits(CeilLog2(m_sliceHeader->NumPocTotalLwrr));
                sm & LWP1(list_entry_l1, i);
            }
        }
    }
}

template <class Archive>
void PredWeightTable::Serialize(Archive& sm)
{
    UINT32 chroma_format_idc = m_sliceHeader->GetSPS().chroma_format_idc;
    UINT32 num_ref_idx_l0_active_minus1 = m_sliceHeader->num_ref_idx_l0_active_minus1;
    UINT32 num_ref_idx_l1_active_minus1 = m_sliceHeader->num_ref_idx_l1_active_minus1;

    sm & LWP(luma_log2_weight_denom);
    if (YUV400 != chroma_format_idc)
    {
        sm & LWP(delta_chroma_log2_weight_denom);
    }
    for (UINT32 i = 0; num_ref_idx_l0_active_minus1 >= i; ++i)
    {
        sm & LWP1(luma_weight_l0_flag, i);
    }
    if (YUV400 != chroma_format_idc)
    {
        for (UINT32 i = 0; num_ref_idx_l0_active_minus1 >= i; ++i)
        {
            sm & LWP1(chroma_weight_l0_flag, i);
        }
    }
    else if (Archive::isLoading)
    {
        for (UINT32 i = 0; num_ref_idx_l0_active_minus1 >= i; ++i)
        {
            chroma_weight_l0_flag[i] = false;
        }
    }
    for (UINT32 i = 0; num_ref_idx_l0_active_minus1 >= i; ++i)
    {
        if (luma_weight_l0_flag[i])
        {
            sm & LWP1(delta_luma_weight_l0, i);
            sm & LWP1(luma_offset_l0, i);
        }
        if (chroma_weight_l0_flag[i])
        {
            for (UINT08 j = 0; 2 > j; ++j)
            {
                sm & LWP2(delta_chroma_weight_l0, i, j);
                sm & LWP2(delta_chroma_offset_l0, i, j);
            }
        }
    }
    if (SLICE_TYPE_B == m_sliceHeader->slice_type)
    {
        for (UINT32 i = 0; num_ref_idx_l1_active_minus1 >= i; ++i)
            sm & LWP1(luma_weight_l1_flag, i);
        if (YUV400 != chroma_format_idc)
        {
            for (UINT32 i = 0; num_ref_idx_l1_active_minus1 >= i; ++i)
            {
                sm & LWP1(chroma_weight_l1_flag, i);
            }
        }
        else if (Archive::isLoading)
        {
            for (UINT32 i = 0; num_ref_idx_l1_active_minus1 >= i; ++i)
            {
                chroma_weight_l1_flag[i] = false;
            }
        }
        for (UINT32 i = 0; num_ref_idx_l1_active_minus1 >= i; ++i)
        {
            if (luma_weight_l1_flag[i])
            {
                sm & LWP1(delta_luma_weight_l1, i);
                sm & LWP1(luma_offset_l1, i);
            }
            if (chroma_weight_l1_flag[i])
            {
                for (UINT08 j = 0; 2 > j; ++j)
                {
                    sm & LWP2(delta_chroma_weight_l1, i, j);
                    sm & LWP2(delta_chroma_offset_l1, i, j);
                }
            }
        }
    }
}

} // namespace H265

namespace BitsIO
{
    template <>
    struct SerializationTraits<H265::NALU>
    {
        static constexpr bool isPrimitive = true;
    };
}

namespace H265
{

} // namespace H265
