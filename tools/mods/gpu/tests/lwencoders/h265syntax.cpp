/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2016, 2018 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>

#include "h265syntax.h"
#include "backinsertiterator.h"

namespace H265
{

const UINT08 default4x4ScalingList[16] =
{
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16
};

// See table 7-6 of ITU-T H.265
const UINT08 default8x8IntraScalingList[64] = // matrix coefficients diagonal by diagonal
{                                             // the correspondent matrix looks like following:
    16, 16, 16, 16, 16, 16, 16, 16,           // 16 16 16 16 17 18 21 24
    16, 16, 17, 16, 17, 16, 17, 18,           // 16 16 16 16 17 19 22 25
    17, 18, 18, 17, 18, 21, 19, 20,           // 16 16 17 18 20 22 25 29
    21, 20, 19, 21, 24, 22, 22, 24,           // 16 16 18 21 24 27 31 36
    24, 22, 22, 24, 25, 25, 27, 30,           // 17 17 20 24 30 35 41 47
    27, 25, 25, 29, 31, 35, 35, 31,           // 18 19 22 27 35 44 54 65
    29, 36, 41, 44, 41, 36, 47, 54,           // 21 22 25 31 41 54 70 88
    54, 47, 65, 70, 65, 88, 88, 115           // 24 25 29 36 47 65 88 115
};

// See table 7-6 of ITU-T H.265
const UINT08 default8x8InterScalingList[64] = // matrix coefficients diagonal by diagonal
{                                             // the correspondent matrix looks like following:
    16, 16, 16, 16, 16, 16, 16, 16,           // 16 16 16 16 17 18 20 24
    16, 16, 17, 17, 17, 17, 17, 18,           // 16 16 16 17 18 20 24 25
    18, 18, 18, 18, 18, 20, 20, 20,           // 16 16 17 18 20 24 25 28
    20, 20, 20, 20, 24, 24, 24, 24,           // 16 17 18 20 24 25 28 33
    24, 24, 24, 24, 25, 25, 25, 25,           // 17 18 20 24 25 28 33 41
    25, 25, 25, 28, 28, 28, 28, 28,           // 18 20 24 25 28 33 41 54
    28, 33, 33, 33, 33, 33, 41, 41,           // 20 24 25 28 33 41 54 71
    41, 41, 54, 54, 54, 71, 71, 91            // 24 25 28 33 41 54 71 91
};

const UINT32 numScalingMatricesPerSize[numScalingMatrixSizes] = {6,  6,    6,    2};
const UINT32 scalingListSize[numScalingMatrixSizes]           = {4,  8,   16,   32};
const UINT32 scalingList1DSize[numScalingMatrixSizes]         = {16, 64, 256, 1024};

void ScalingListData::InitFromDefault()
{
    for (size_t sizeId = 0; numScalingMatrixSizes > sizeId; ++sizeId)
    {
        UINT32 numScalingMatrices = numScalingMatricesPerSize[sizeId];
        for (size_t matrixId = 0; numScalingMatrices > matrixId; ++matrixId)
        {
            m_scalingFactor[sizeId][matrixId].Resize(scalingListSize[sizeId]);
            switch (sizeId)
            {
                case SCALING_MTX_4x4:
                {
                    for (size_t i = 0; NUMELEMS(default4x4ScalingList) > i; ++i)
                    {
                        // See equation 7-32 of ITU-T H.265
                        size_t x = scanOrder(2, SCAN_DIAG, i, 0);
                        size_t y = scanOrder(2, SCAN_DIAG, i, 1);

                        m_scalingFactor
                            [SCALING_MTX_4x4]
                            [matrixId]
                            [y][x] = default4x4ScalingList[i];
                    }
                    break;
                }
                case SCALING_MTX_8x8:
                {
                    for (size_t i = 0; NUMELEMS(default8x8IntraScalingList) > i; ++i)
                    {
                        // See equation 7-33 of ITU-T H.265
                        size_t x = scanOrder(3, SCAN_DIAG, i, 0);
                        size_t y = scanOrder(3, SCAN_DIAG, i, 1);

                        if (matrixId < 3)
                        {
                            m_scalingFactor
                                [SCALING_MTX_8x8]
                                [matrixId]
                                [y][x] = default8x8IntraScalingList[i];
                        }
                        else
                        {
                            m_scalingFactor
                                [SCALING_MTX_8x8]
                                [matrixId]
                                [y][x] = default8x8InterScalingList[i];
                        }
                    }
                    break;
                }
                case SCALING_MTX_16x16:
                {
                    for (size_t i = 0; NUMELEMS(default8x8IntraScalingList) > i; ++i)
                    {
                        for (size_t j = 0; 2 > j; ++j)
                        {
                            for (size_t k = 0; 2 > k; ++k)
                            {
                                // See equation 7-34 of ITU-T H.265
                                size_t x = scanOrder(3, SCAN_DIAG, i, 0);
                                size_t y = scanOrder(3, SCAN_DIAG, i, 1);

                                if (matrixId < 3)
                                {
                                    m_scalingFactor
                                        [SCALING_MTX_16x16]
                                        [matrixId]
                                        [y * 2 + j]
                                        [x * 2 + k] = default8x8IntraScalingList[i];
                                }
                                else
                                {
                                    m_scalingFactor
                                        [SCALING_MTX_16x16]
                                        [matrixId]
                                        [y * 2 + j]
                                        [x * 2 + k] = default8x8InterScalingList[i];
                                }
                            }
                        }
                    }
                    // See equation 7-35 of ITU-T H.265
                    m_scalingFactor[SCALING_MTX_16x16][matrixId][0][0] = 16;
                    break;
                }
                case SCALING_MTX_32x32:
                {
                    for (size_t i = 0; NUMELEMS(default8x8IntraScalingList) > i; ++i)
                    {
                        for (size_t j = 0; 4 > j; ++j)
                        {
                            for (size_t k = 0; 4 > k; ++k)
                            {
                                // See equation 7-36 of ITU-T H.265
                                size_t x = scanOrder(3, SCAN_DIAG, i, 0);
                                size_t y = scanOrder(3, SCAN_DIAG, i, 1);

                                if (matrixId < 1)
                                {
                                    m_scalingFactor
                                        [SCALING_MTX_32x32]
                                        [matrixId]
                                        [y * 4 + j]
                                        [x * 4 + k] = default8x8IntraScalingList[i];
                                }
                                else
                                {
                                    m_scalingFactor
                                        [SCALING_MTX_32x32]
                                        [matrixId]
                                        [y * 4 + j]
                                        [x * 4 + k] = default8x8InterScalingList[i];
                                }
                            }
                        }
                    }
                    // See equation 7-37 of ITU-T H.265
                    m_scalingFactor[SCALING_MTX_32x32][matrixId][0][0] = 16;
                    break;
                }
            }
        }
    }
    m_loaded = true;
}

class DefaultScalingListData
{
public:
    DefaultScalingListData()
    {
        m_scalingListData.InitFromDefault();
    }

    const ScalingListData& GetDefaultScalingListData() const
    {
        return m_scalingListData;
    }

private:
    ScalingListData m_scalingListData;
};

const ScalingListData& ScalingListData::GetDefaultScalingListData()
{
    static DefaultScalingListData defaultScalingListData;

    return defaultScalingListData.GetDefaultScalingListData();
}

ScanOrder scanOrder;

ScanOrder::ScanOrder()
{
    for (size_t log2BlockSize = 0; 4 > log2BlockSize; ++log2BlockSize)
    {
        for (size_t scanIdx = 0; 3 > scanIdx; ++scanIdx)
        {
            size_t blkSize = 1ULL << log2BlockSize;
            m_scanOrder[log2BlockSize][scanIdx].Resize(blkSize * blkSize, 2);
            switch (scanIdx)
            {
                case SCAN_DIAG:
                {
                    // See 6.5.3 of ITU-T H.265.
                    size_t maxX = 2 * blkSize - 1;
                    size_t j = 0;
                    for (size_t i = 0; maxX > i; ++i)
                    {
                        size_t x, y;
                        for (y = i, x = 0; i >= x; --y, ++x)
                        {
                            if (blkSize > x && blkSize > y)
                            {
                                m_scanOrder[log2BlockSize][scanIdx][j][0] = x;
                                m_scanOrder[log2BlockSize][scanIdx][j][1] = y;
                                ++j;
                            }
                        }
                    }

                    break;
                }
                case SCAN_HOR:
                {
                    // See 6.5.4 of ITU-T H.265.
                    size_t i = 0;
                    for (size_t y = 0; y < blkSize; ++y)
                    {
                        for (size_t x = 0; x < blkSize; ++x)
                        {
                            m_scanOrder[log2BlockSize][scanIdx][i][0] = x;
                            m_scanOrder[log2BlockSize][scanIdx][i][1] = y;
                            ++i;
                        }
                    }

                    break;
                }
                case SCAN_VER:
                {
                    // See 6.5.5 of ITU-T H.265.
                    size_t i = 0;
                    for (size_t x = 0; x < blkSize; ++x)
                    {
                        for (size_t y = 0; y < blkSize; ++y)
                        {
                            m_scanOrder[log2BlockSize][scanIdx][i][0] = x;
                            m_scanOrder[log2BlockSize][scanIdx][i][1] = y;
                            ++i;
                        }
                    }

                    break;
                }
            }
        }
    }
}

#if defined(SUPPORT_TEXT_OUTPUT)
void ScalingListData::Save(H26X::TextOArchive& sm) const
{
    if (!m_loaded)
    {
        return;
    }

    FILE *of = sm.GetOFile();
    string indent(sm.GetIndent(), ' ');

    for (UINT32 sizeId = 0; numScalingMatrixSizes > sizeId; ++sizeId)
    {
        if (0 != sizeId)
        {
            fprintf(of, "\n");
        }

        if (SCALING_MTX_16x16 == sizeId)
        {
            for (size_t i = 0; 16 > i; i += 2)
            {
                fprintf(of, "%s", indent.c_str());
                UINT32 numScalingMatrices = numScalingMatricesPerSize[sizeId];
                for (UINT32 matrixId = 0; numScalingMatrices > matrixId; ++matrixId)
                {
                    if (0 != matrixId)
                    {
                        fprintf(of, "  ");
                    }

                    for (size_t j = 0; 16 > j; j += 2)
                    {
                        if (0 == i && 0 == j)
                        {
                            fprintf(of, "%4u, %4u",
                                m_scalingFactor[sizeId][matrixId][0][0],
                                m_scalingFactor[sizeId][matrixId][0][1]
                            );
                        }
                        else if (0 == j)
                        {
                            fprintf(of, "%10u", m_scalingFactor[sizeId][matrixId][i][0]);
                        }
                        else
                        {
                            fprintf(of, "%4u", m_scalingFactor[sizeId][matrixId][i][j]);
                        }
                    }
                }
                fprintf(of, "\n");
            }
        }
        else if (SCALING_MTX_32x32 == sizeId)
        {
            for (size_t i = 0; 32 > i; i += 4)
            {
                fprintf(of, "%s", indent.c_str());
                UINT32 numScalingMatrices = numScalingMatricesPerSize[sizeId];
                for (UINT32 matrixId = 0; numScalingMatrices > matrixId; ++matrixId)
                {
                    if (0 != matrixId)
                    {
                        fprintf(of, "  ");
                    }

                    for (size_t j = 0; 32 > j; j += 4)
                    {
                        if (0 == i && 0 == j)
                        {
                            fprintf(of, "%4u, %4u",
                                m_scalingFactor[sizeId][matrixId][0][0],
                                m_scalingFactor[sizeId][matrixId][0][1]
                            );
                        }
                        else if (0 == j)
                        {
                            fprintf(of, "%10u", m_scalingFactor[sizeId][matrixId][i][0]);
                        }
                        else
                        {
                            fprintf(of, "%4u", m_scalingFactor[sizeId][matrixId][i][j]);
                        }
                    }
                }
                fprintf(of, "\n");
            }
        }
        else
        {
            const size_t numRows = m_scalingFactor[sizeId][0].GetRows();
            for (size_t i = 0; numRows > i; ++i)
            {
                fprintf(of, "%s", indent.c_str());
                UINT32 numScalingMatrices = numScalingMatricesPerSize[sizeId];
                for (UINT32 matrixId = 0; numScalingMatrices > matrixId; ++matrixId)
                {
                    if (0 != matrixId)
                    {
                        fprintf(of, "  ");
                    }

                    for (size_t j = 0; m_scalingFactor[sizeId][matrixId].GetCols() > j; ++j)
                    {
                        fprintf(of, "%4u", m_scalingFactor[sizeId][matrixId][i][j]);
                    }
                }
                fprintf(of, "\n");
            }
        }
    }
}

void SeqParameterSet::Serialize(H26X::TextOArchive& sm)
{
    SerializeCommon(sm);
    sm & LWP(SubWidthC);
    sm & LWP(SubHeightC);
    sm & LWP(BitDepthY);
    sm & LWP(QpBdOffsetY);
    sm & LWP(BitDepthC);
    sm & LWP(QpBdOffsetC);
    sm & LWP(MaxPicOrderCntLsb);
    sm & LWP(MinCbLog2SizeY);
    sm & LWP(CtbLog2SizeY);
    sm & LWP(MinCbSizeY);
    sm & LWP(CtbSizeY);
    sm & LWP(PicWidthInMinCbsY);
    sm & LWP(PicWidthInCtbsY);
    sm & LWP(PicHeightInMinCbsY);
    sm & LWP(PicHeightInCtbsY);
    sm & LWP(PicSizeInMinCbsY);
    sm & LWP(PicSizeInCtbsY);
    sm & LWP(PicSizeInSamplesY);
    sm & LWP(PicWidthInSamplesC);
    sm & LWP(PicHeightInSamplesC);
    sm & LWP(CtbWidthC);
    sm & LWP(CtbHeightC);
    sm & LWP(PcmBitDepthY);
    sm & LWP(PcmBitDepthC);
    sm & LWP(CoeffMinY);
    sm & LWP(CoeffMinC);
    sm & LWP(CoeffMaxY);
    sm & LWP(CoeffMaxC);
    sm & LWP(WpOffsetBdShiftY);
    sm & LWP(WpOffsetBdShiftC);
    sm & LWP(WpOffsetHalfRangeY);
    sm & LWP(WpOffsetHalfRangeC);
}

void PicParameterSet::Serialize(H26X::TextOArchive& sm)
{
    SerializeCommon(sm);
    sm & LWP(Log2MaxTransformSkipSize);
    sm & LWP(Log2MinLwChromaQpOffsetSize);
}

void SliceSegmentHeader::Serialize(H26X::TextOArchive& sm)
{
    SerializeCommon(sm);
    if (!dependent_slice_segment_flag)
    {
        sm & LWP(sw_hdr_skip_length);
        if (!IsIDR())
        {
            sm & LWP(NumPocTotalLwrr);
            sm & LWP(LwrrRpsIdx);

            FILE *of = sm.GetOFile();
            string indent(sm.GetIndent(), ' ');

            UINT32 totLongTermRefPics = num_long_term_sps + num_long_term_pics;
            if (0 < totLongTermRefPics)
            {
                fprintf(of, "%s%s[%u] = {", indent.c_str(), "PocLsbLt", totLongTermRefPics);
                for (UINT32 i = 0; totLongTermRefPics > i; ++i)
                {
                    if (0 == i)
                        fprintf(of, "%u", PocLsbLt[i]);
                    else
                        fprintf(of, ", %u", PocLsbLt[i]);
                }
                fprintf(of, "}\n");

                fprintf(of, "%s%s[%u] = {", indent.c_str(), "UsedByLwrrPicLt", totLongTermRefPics);
                for (UINT32 i = 0; totLongTermRefPics > i; ++i)
                {
                    if (0 == i)
                        fprintf(of, "%s", UsedByLwrrPicLt[i] ? "true" : "false");
                    else
                        fprintf(of, ", %s", UsedByLwrrPicLt[i] ? "true" : "false");
                }
                fprintf(of, "}\n");

                fprintf(of, "%s%s[%u] = {", indent.c_str(), "DeltaPocMsbCycleLt", totLongTermRefPics);
                for (UINT32 i = 0; totLongTermRefPics > i; ++i)
                {
                    if (0 == i)
                        fprintf(of, "%u", DeltaPocMsbCycleLt[i]);
                    else
                        fprintf(of, ", %u", DeltaPocMsbCycleLt[i]);
                }
                fprintf(of, "}\n");
            }
        }
    }
}
#endif

SliceSegmentHeader::SliceSegmentHeader(
    NALU &nalu,
    const VidParamSetSrc *vidParamSrc,
    const SeqParamSetSrc *seqParamSrc,
    const PicParamSetSrc *picParamSrc
)
  : RefPicListsModificationField<SliceSegmentHeader>()
  , PredWeightTableField<SliceSegmentHeader>()
  , m_vidParamSrc(vidParamSrc)
  , m_seqParamSrc(seqParamSrc)
  , m_picParamSrc(picParamSrc)
#if defined(SUPPORT_TEXT_OUTPUT)
  , m_offset(nalu.GetOffset())
#endif
  , TemporalId(nalu.GetTemporalId())
  , nal_unit_type(nalu.GetUnitType())
#if defined(SUPPORT_TEXT_OUTPUT)
  , nuh_layer_id(nalu.GetLayerId())
#endif
{
    m_ebspPayload.assign(
        nalu.GetEbspFromStartCode(),
        nalu.GetEbspFromStartCode() + nalu.GetEbspSizeWithStart()
    );
    BitIArchive<NALU::RBSPIterator> ia(nalu.RBSPBegin(), nalu.RBSPEnd());
    ia >> *this;
}

template <class T>
void NALU::CreateDataFrom(const T &t, NALUType naluType, UINT32 layerId, UINT32 temporalId)
{
    m_rbspData.clear();
    BackInsertIterator it(m_rbspData);
    BitOArchive<BackInsertIterator> oa(it);

    NBits<startCodeSize * 8> startCode = 1;
    NBits<1> forbidden_zero_bit = 0;
    oa << startCode << forbidden_zero_bit;

    nal_unit_type = naluType;
    nuh_layer_id = layerId;
    nuh_temporal_id_plus1 = temporalId;

    oa << nal_unit_type << nuh_layer_id << nuh_temporal_id_plus1 << t;

    NBits<1> stopBit = 1;
    oa << stopBit;
    oa.FillByteAlign();

    BitIArchive<vector<UINT08>::const_iterator> ia(m_rbspData.begin(), m_rbspData.end());
    ia >> startCode >> forbidden_zero_bit;
    m_valid = !forbidden_zero_bit;

    if (m_valid)
    {
        ia >> nal_unit_type >> nuh_layer_id >> nuh_temporal_id_plus1;
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

void NALU::CreateDataFromVPS(const VidParameterSet &vps, UINT32 layerId, UINT32 temporalId)
{
    CreateDataFrom(vps, NAL_TYPE_VPS_NUT, layerId, temporalId);
}

void NALU::CreateDataFromSPS(const SeqParameterSet &sps, UINT32 layerId, UINT32 temporalId)
{
    CreateDataFrom(sps, NAL_TYPE_SPS_NUT, layerId, temporalId);
}

void NALU::CreateDataFromPPS(const PicParameterSet &pps, UINT32 layerId, UINT32 temporalId)
{
    CreateDataFrom(pps, NAL_TYPE_PPS_NUT, layerId, temporalId);
}

} // namespace H265
