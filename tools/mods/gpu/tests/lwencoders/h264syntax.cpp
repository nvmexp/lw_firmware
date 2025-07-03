/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014, 2018 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <cstring>
#include <iterator>
#include <map>
#include <utility>

#include "core/include/types.h"
#include "backinsertiterator.h"

#include "h264syntax.h"

namespace H264
{

struct LevelToSizeInit
{
    unsigned int level;
    unsigned int byteSize;

    operator pair<const unsigned int, unsigned int>() const
    {
        return make_pair(level, byteSize);
    }
} levelToSizeInit[] =
{
    { 9,   152064}, // level 1b
    {10,   152064}, // level 1
    {12,   912384}, // level 1.2
    {13,   912384}, // ...
    {20,   912384},
    {21,  1824768},
    {22,  3110400},
    {30,  3110400},
    {31,  6912000},
    {32,  7864320},
    {40, 12582912},
    {41, 12582912},
    {42, 13369344},
    {50, 42393600},
    {51, 70778880},
    {52, 70778880}
};

unsigned int SeqParameterSet::DpbSize() const
{
    static const map<unsigned int, unsigned int> levelToSize(
        &levelToSizeInit[0],
        &levelToSizeInit[0] +
            sizeof(levelToSizeInit) / sizeof(levelToSizeInit[0]));

    unsigned int size = 0;

    if (m_empty)
    {
        return 0;
    }

    map<unsigned int, unsigned int>::const_iterator it;
    it = levelToSize.find(level_idc);
    if (levelToSize.end() == it)
    {
        if (11 == level_idc)
        {
            if (FREXT_HP > profile_idc && FREXT_CAVLC444 != profile_idc && constraint_set3_flag)
            {
                size = 152064;
            }
            else
            {
                size = 345600;
            }
        }
        else
        {
            size = 0;
        }
    }
    else
    {
        size = it->second;
    }

    unsigned int picSize =
        (pic_width_in_mbs_minus1 + 1) *
        (pic_height_in_map_units_minus1 + 1) *
        (frame_mbs_only_flag ? 1 : 2) * 384;
    size /= picSize;
    size = min(16U, size);

    if (vui_parameters_present_flag &&
        vui_seq_parameters.bitstream_restriction_flag)
    {
        int size_vui;
        size_vui = max(
            1U,
            static_cast<unsigned int>(vui_seq_parameters.max_dec_frame_buffering));
        size = size_vui;
    }

    return size;
}

template <class T>
void NALU::CreateDataFrom(const T &t, NalRefIdcPriority refIdc, NALUType naluType)
{
    m_rbspData.clear();
    BackInsertIterator it(m_rbspData);
    BitOArchive<BackInsertIterator> oa(it);

    NBits<startCodeSize * 8> startCode = 1;
    NBits<1> forbidden_zero_bit = 0;
    oa << startCode << forbidden_zero_bit;

    nal_ref_idc = refIdc;
    nal_unit_type = naluType;
    oa << nal_ref_idc << nal_unit_type << t;

    NBits<1> stopBit = 1;
    oa << stopBit;
    oa.FillByteAlign();

    BitIArchive<vector<UINT08>::const_iterator> ia(m_rbspData.begin(), m_rbspData.end());
    ia >> startCode >> forbidden_zero_bit;
    m_valid = !forbidden_zero_bit;

    if (m_valid)
    {
        ia >> nal_ref_idc >> nal_unit_type;
    }

    // callwlate EBSP size, omitting the start code and the first byte
    size_t ebspSize = H26X::CalcEBSPSize(
        m_rbspData.begin() + startCodeSize + 1,
        m_rbspData.end()
    ) + startCodeSize + 1;

    // create EBSP payload
    m_ebspData.resize(ebspSize);
    copy(
        m_rbspData.begin(),
        m_rbspData.begin() + startCodeSize + 1,
        m_ebspData.begin()
    );
    H26X::ColwertRBSP2EBSP(
        m_rbspData.begin() + startCodeSize + 1,
        m_rbspData.end(),
        m_ebspData.begin() + startCodeSize + 1
    );
}

void NALU::CreateDataFromSPS(const SeqParameterSet &sps)
{
    CreateDataFrom(sps, NALU_PRIORITY_HIGHEST, NAL_TYPE_SEQ_PARAM);
}

void NALU::CreateDataFromPPS(const PicParameterSet &pps)
{
    CreateDataFrom(pps, NALU_PRIORITY_HIGHEST, NAL_TYPE_PIC_PARAM);
}

void NALU::CreateFromSubsetSPS(const SubsetSeqParameterSet &subsetSps)
{
    CreateDataFrom(subsetSps, NALU_PRIORITY_HIGHEST, NAL_TYPE_SUBSET_SPS);
}
} // namespace H264
