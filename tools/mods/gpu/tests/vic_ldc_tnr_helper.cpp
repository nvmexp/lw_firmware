/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "class/clc5b6.h"
#include "core/include/channel.h"
#include "core/include/fileholder.h"
#include "core/include/utility.h"
#include "gpu/utility/surfrdwr.h"
#include "vic_ldc_tnr.h"

// These are copied from //hw/gpu_ip/unit/vic/4.2/cmod/geotran/geoDriver.cpp
typedef unsigned char uchar;

static const int g_nearest[][4] =
{
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 256, 0, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    },
    {
        0, 0, 256, 0
    }
};

static const int g_bilinear[][4] =
{
    {
        0, 256, 0, 0
    },
    {
        0, 248, 8, 0
    },
    {
        0, 240, 16, 0
    },
    {
        0, 232, 24, 0
    },
    {
        0, 224, 32, 0
    },
    {
        0, 216, 40, 0
    },
    {
        0, 208, 48, 0
    },
    {
        0, 200, 56, 0
    },
    {
        0, 192, 64, 0
    },
    {
        0, 184, 72, 0
    },
    {
        0, 176, 80, 0
    },
    {
        0, 168, 88, 0
    },
    {
        0, 160, 96, 0
    },
    {
        0, 152, 104, 0
    },
    {
        0, 144, 112, 0
    },
    {
        0, 136, 120, 0
    },
    {
        0, 128, 128, 0
    },
    {
        0, 120, 136, 0
    },
    {
        0, 112, 144, 0
    },
    {
        0, 104, 152, 0
    },
    {
        0, 96, 160, 0
    },
    {
        0, 88, 168, 0
    },
    {
        0, 80, 176, 0
    },
    {
        0, 72, 184, 0
    },
    {
        0, 64, 192, 0
    },
    {
        0, 56, 200, 0
    },
    {
        0, 48, 208, 0
    },
    {
        0, 40, 216, 0
    },
    {
        0, 32, 224, 0
    },
    {
        0, 24, 232, 0
    },
    {
        0, 16, 240, 0
    },
    {
        0, 8, 248, 0
    }
};

static const int g_bilwbic[][4] =
{
    {
        0, 256, 0, 0
    },
    {
        -4, 255, 4, 1
    },
    {
        -7, 254, 10, -1
    },
    {
        -10, 251, 16, -1
    },
    {
        -12, 247, 23, -2
    },
    {
        -14, 242, 31, -3
    },
    {
        -16, 236, 39, -3
    },
    {
        -17, 229, 48, -4
    },
    {
        -18, 222, 58, -6
    },
    {
        -19, 214, 68, -7
    },
    {
        -19, 205, 78, -8
    },
    {
        -19, 196, 89, -10
    },
    {
        -19, 186, 100, -11
    },
    {
        -18, 176, 111, -13
    },
    {
        -18, 166, 122, -14
    },
    {
        -17, 155, 133, -15
    },
    {
        -16, 144, 144, -16
    },
    {
        -15, 133, 155, -17
    },
    {
        -14, 122, 166, -18
    },
    {
        -13, 111, 176, -18
    },
    {
        -11, 100, 186, -19
    },
    {
        -10, 89, 196, -19
    },
    {
        -9, 78, 205, -18
    },
    {
        -7, 68, 214, -19
    },
    {
        -6, 58, 222, -18
    },
    {
        -5, 48, 229, -16
    },
    {
        -4, 39, 236, -15
    },
    {
        -3, 31, 242, -14
    },
    {
        -2, 23, 247, -12
    },
    {
        -1, 16, 251, -10
    },
    {
        0, 10, 254, -8
    },
    {
        0, 4, 255, -3
    }
};

union VicLdcTnrHelper::m_u32_f32
{
    unsigned int u32;
    float f32;
};

unsigned int VicLdcTnrHelper::F32ToU32(float in_f32) const
{
    union m_u32_f32 temp;
    temp.f32 = in_f32;
    return temp.u32;
}

RC VicLdcTnr42Helper::GetSurfaceParamsVicPipe
(
    Vic42::PIXELFORMAT PixelFormat
    ,UINT32 *numPlanes
    ,UINT32 *numComp
) const
{
    RC rc;
    if (numPlanes == nullptr || numComp == nullptr)
        return RC::SOFTWARE_ERROR;

    switch(PixelFormat) {
        case Vic42::T_Y8___V8U8_N420:
            *numPlanes = 2;
            numComp[0] = 0;
            numComp[1] = 1;
            break;
        case Vic42::T_Y10___V10U10_N444:
            *numPlanes  = 2;
            numComp[0] = 0;
            numComp[1] = 1;
            break;
        default :
            Printf(Tee::PriError, "Unknown pixel format\n");
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

int VicLdcTnr42Helper::GetBetaCalcStepBeta(int c, int clip_s, int clip_e) const
{
    int c_r = std::min(std::max(c, clip_s), clip_e);
    return c_r;
}

/* virtual */ RC VicLdcTnr42Helper::FillConfigStruct
(
    vector<UINT08> *pCfgBuffer
    ,const SrcImages &srcImages
    ,const DstImage &dstImage
) const
{
    RC rc;
    pCfgBuffer->resize(sizeof(Vic42::GeoTranConfigStruct));
    std::fill(pCfgBuffer->begin(), pCfgBuffer->end(), 0);

    Vic42::GeoTranConfigStruct *geoTranConfigStruct =
            reinterpret_cast<Vic42::GeoTranConfigStruct*>(&(*pCfgBuffer)[0]);
    Vic42::GeoTranConfigParamStruct *geoTranStruct = &geoTranConfigStruct->paramConfig;
    Vic42::TNR3ConfigParamStruct *tnr3config = &geoTranConfigStruct->tnr3Config;
    Vic42::BFRangeTableItems *bfRangeTableLuma = &geoTranConfigStruct->BFRangeTableLuma[0];
    Vic42::BFRangeTableItems *bfRangeTableChroma = &geoTranConfigStruct->BFRangeTableChroma[0];
    Vic42::CoeffPhaseParamStruct *filterCoeff = &geoTranConfigStruct->FilterCoeff[0];
    const TNR3Params &tnr3Params = m_InputParams.tnr3Params;
    const LDCParams &ldcParams = m_InputParams.ldcParams;
    const ImageDimensions &srcDims = srcImages.dimensions;
    const ImageDimensions &dstDims = dstImage.dimensions;

    geoTranStruct->GeoTranEn          = ldcParams.geoTranEn;
    // Lwrrently supporting only Mode 1 which is IPT Mode
    geoTranStruct->GeoTranMode        = 1;
    geoTranStruct->IPTMode            = 1;
    geoTranStruct->PixelFilterType    = m_InputParams.pixelFilterType;
    geoTranStruct->PixelFormat        = m_InputParams.pixelFormat;
    geoTranStruct->CacheWidth         = Vic42::CACHE_WIDTH_16Bx16;
    geoTranStruct->SrcBlkKind         = m_InputParams.blockKind;
    geoTranStruct->SrcBlkHeight       =
            (m_InputParams.blockKind == Vic42::BLK_KIND_GENERIC_16Bx2) ? 4 : 0;
    geoTranStruct->DestBlkKind        = m_OutputParams.blockKind;
    geoTranStruct->DestBlkHeight      =
            (m_OutputParams.blockKind == Vic42::BLK_KIND_GENERIC_16Bx2) ? 4 : 0;;
    geoTranStruct->MskBitMapEn        = IGNORED;
    geoTranStruct->MaskedPixelFillMode= IGNORED;
    geoTranStruct->XSobelMode         = IGNORED;
    geoTranStruct->SubFrameEn         = IGNORED;
    geoTranStruct->XSobelBlkKind      = IGNORED;
    geoTranStruct->XSobelBlkHeight    = IGNORED;
    geoTranStruct->XSobelDSBlkKind    = IGNORED;
    geoTranStruct->XSobelDSBlkHeight  = IGNORED;
    geoTranStruct->NonFixedPatchEn    = IGNORED;

    // These are copied from //hw/gpu_ip/unit/vic/4.2/cmod/geotran/geoDriver.cpp
    geoTranStruct->log2HorSpace_0     = 2;
    geoTranStruct->log2VerSpace_0     = 2;
    geoTranStruct->log2HorSpace_1     = 2;
    geoTranStruct->log2VerSpace_1     = 2;
    geoTranStruct->log2HorSpace_2     = 2;
    geoTranStruct->log2VerSpace_2     = 2;
    geoTranStruct->log2HorSpace_3     = 2;
    geoTranStruct->log2VerSpace_3     = 2;

    geoTranStruct->HorRegionNum       = 0; //using only one horizontal region
    geoTranStruct->VerRegionNum       = 0; //using only one vertical region
    // Sum of all valid horRegionWidth and verRegionHeight should be equal to
    // DestFrame_Width and DestFrame_Height
    geoTranStruct->horRegionWidth_0   = m_OutputParams.imageWidth - 1;
    geoTranStruct->horRegionWidth_1   = IGNORED;
    geoTranStruct->horRegionWidth_2   = IGNORED;
    geoTranStruct->horRegionWidth_3   = IGNORED;
    geoTranStruct->verRegionHeight_0  = m_OutputParams.imageHeight - 1;
    geoTranStruct->verRegionHeight_1  = IGNORED;
    geoTranStruct->verRegionHeight_2  = IGNORED;
    geoTranStruct->verRegionHeight_3  = IGNORED;
    geoTranStruct->IPT_M11            = F32ToU32(1.0);
    geoTranStruct->IPT_M12            = F32ToU32(0);
    geoTranStruct->IPT_M13            = F32ToU32(0);
    geoTranStruct->IPT_M21            = F32ToU32(0);
    geoTranStruct->IPT_M22            = F32ToU32(1.0);
    geoTranStruct->IPT_M23            = F32ToU32(0);
    geoTranStruct->IPT_M31            = F32ToU32(0);
    geoTranStruct->IPT_M32            = F32ToU32(0);
    geoTranStruct->IPT_M33            = F32ToU32(0);
    geoTranStruct->SourceRectLeft     = 0;
    geoTranStruct->SourceRectRight    = m_InputParams.imageWidth - 1;
    geoTranStruct->SourceRectTop      = 0;
    geoTranStruct->SourceRectBottom   = m_InputParams.imageHeight - 1;
    geoTranStruct->SrcImgWidth        = m_InputParams.imageWidth - 1;
    geoTranStruct->SrcImgHeight       = m_InputParams.imageHeight - 1;

    UINT32 xStride[maxSurfaces];
    UINT32 bpc[MAXPLANES];
    UINT32 numComp[MAXPLANES];
    UINT32 numPlanes = 0;

    Vic42::PIXELFORMAT format = (Vic42::PIXELFORMAT)geoTranStruct->PixelFormat;
    CHECK_RC(GetSurfParams(format, false, nullptr, xStride, nullptr, nullptr)); //bytes

    CHECK_RC(GetSurfaceParamsVicPipe(format, &numPlanes, numComp));

    for(UINT32 i = 0; i < numPlanes; i++)
    {
        if (format == Vic42::T_R8)
            bpc[i]  = 1 << xStride[i];
        else
            bpc[i] = 1<<(xStride[i] - numComp[i]);
    }

    // VIC 4.2 IAS document Chapter 6.1.6.7 specifies this
    // Source Surface Luma Stride
    // Actual_stride_in_bytes = (SrcSfcLumaWidth + 1) * 64Bytes
    // so SrcSfcLumaWidth = Actual_stride_in_bytes/64 -1;
    // For PL: Actual_stride_in_bytes should be 256 Bytes aligned
    geoTranStruct->SrcSfcLumaWidth    = srcDims.lumaWidth  * bpc[0] / 64 - 1;
    geoTranStruct->SrcSfcLumaHeight   = srcDims.lumaHeight - 1;
    geoTranStruct->SrcSfcChromaWidth  = srcDims.chromaWidth  * bpc[1] / 64 - 1;
    geoTranStruct->SrcSfcChromaHeight = srcDims.chromaHeight - 1;
    geoTranStruct->DestRectLeft       = 0;
    geoTranStruct->DestRectRight      = m_OutputParams.imageWidth-1;
    geoTranStruct->DestRectTop        = 0;
    geoTranStruct->DestRectBottom     = m_OutputParams.imageHeight-1;
    geoTranStruct->SubFrameRectTop    = IGNORED;
    geoTranStruct->SubFrameRectBottom = IGNORED;
    geoTranStruct->DestSfcLumaWidth   = dstDims.lumaWidth  * bpc[0] / 64 - 1;
    geoTranStruct->DestSfcLumaHeight  = dstDims.lumaHeight - 1;
    geoTranStruct->DestSfcChromaWidth = dstDims.chromaWidth  * bpc[1] / 64 - 1;
    geoTranStruct->DestSfcChromaHeight= dstDims.chromaHeight - 1;
    geoTranStruct->SparseWarpMapWidth = 0;
    geoTranStruct->SparseWarpMapHeight= 0;
    geoTranStruct->SparseWarpMapStride= 0;

    //MaskBitEnable is zero
    geoTranStruct->MaskBitMapWidth    = IGNORED;
    geoTranStruct->MaskBitMapHeight   = IGNORED;
    geoTranStruct->MaskBitMapStride   = IGNORED;
    geoTranStruct->XSobelWidth        = IGNORED;
    geoTranStruct->XSobelHeight       = IGNORED;
    geoTranStruct->XSobelStride       = IGNORED;
    geoTranStruct->DSStride           = IGNORED;
    geoTranStruct->maskY              = IGNORED;
    geoTranStruct->maskU              = 32768;
    geoTranStruct->maskV              = 32768;

    const int (*filter_coeff)[4] = {0};
    if (m_InputParams.pixelFilterType == Vic42::FILTER_TYPE_NORMAL)
    {
        filter_coeff = g_nearest;
    }
    else if (m_InputParams.pixelFilterType == Vic42::FILTER_TYPE_NOISE)
    {
        filter_coeff = g_bilinear;
    }
    else if (m_InputParams.pixelFilterType == Vic42::FILTER_TYPE_DETAIL)
    {
        filter_coeff = g_bilwbic;
    }

    for (int i = 0; i < 17; i++)
    {
        filterCoeff[i].coeff_0 = filter_coeff[i][0];
        filterCoeff[i].coeff_1 = filter_coeff[i][1];
        filterCoeff[i].coeff_2 = filter_coeff[i][2];
        filterCoeff[i].coeff_3 = filter_coeff[i][3];
    }

    tnr3config->TNR3En                    = tnr3Params.tnr3En;
    tnr3config->BetaBlendingEn            = tnr3Params.betaBlendingEn;
    tnr3config->AlphaBlendingEn           = tnr3Params.alphaBlendingEn;
    tnr3config->AlphaSmoothEn             = tnr3Params.alphaSmoothEn;
    tnr3config->TempAlphaRestrictEn       = tnr3Params.tempAlphaRestrictEn;
    tnr3config->AlphaClipEn               = tnr3Params.alphaClipEn;
    tnr3config->BFRangeEn                 = tnr3Params.bFRangeEn;
    tnr3config->BFDomainEn                = tnr3Params.bFDomainEn;

    UINT32 rangeSigmaLuma    = tnr3Params.rangeSigmaLuma;
    UINT32 rangeSigmaChroma  = tnr3Params.rangeSigmaChroma;
    UINT32 shift=0;
    while (3*rangeSigmaLuma>(LW_VIC_TNR3_BF_RANGE_LUMA_TABLE_SIZE<<shift))
    {
        shift++;
    }
    for (UINT32 i=0; i<LW_VIC_TNR3_BF_RANGE_LUMA_TABLE_SIZE; i+=4)
    {
        UINT32 x = i<<shift;
        bfRangeTableLuma[i/4].item0 = (uchar)64*(exp(-(double)x*x/
                (2*rangeSigmaLuma*rangeSigmaLuma)));
        x = (i+1)<<shift;
        bfRangeTableLuma[i/4].item1 = (uchar)64*(exp(-(double)x*x/
                (2*rangeSigmaLuma*rangeSigmaLuma)));
        x = (i+2)<<shift;
        bfRangeTableLuma[i/4].item2 = (uchar)64*(exp(-(double)x*x/
                (2*rangeSigmaLuma*rangeSigmaLuma)));
        x = (i+3)<<shift;
        bfRangeTableLuma[i/4].item3 = (uchar)64*(exp(-(double)x*x/
                (2*rangeSigmaLuma*rangeSigmaLuma)));
    }
    tnr3config->BFRangeLumaShift          = shift;
    shift=0;
    while (3*rangeSigmaChroma>(LW_VIC_TNR3_BF_RANGE_CHROMA_TABLE_SIZE<<shift))
    {
        shift++;
    }
    for (UINT32 i=0; i<LW_VIC_TNR3_BF_RANGE_CHROMA_TABLE_SIZE; i+=4)
    {
        UINT32 x = i<<shift;
        bfRangeTableChroma[i/4].item0 = (uchar)64*(exp(-(double)x*x/
                (2*rangeSigmaChroma*rangeSigmaChroma)));
        x = (i+1)<<shift;
        bfRangeTableChroma[i/4].item1 = (uchar)64*(exp(-(double)x*x/
                (2*rangeSigmaChroma*rangeSigmaChroma)));
        x = (i+2)<<shift;
        bfRangeTableChroma[i/4].item2 = (uchar)64*(exp(-(double)x*x/
                (2*rangeSigmaChroma*rangeSigmaChroma)));
        x = (i+3)<<shift;
        bfRangeTableChroma[i/4].item3 = (uchar)64*(exp(-(double)x*x/
                (2*rangeSigmaChroma*rangeSigmaChroma)));
    }

    tnr3config->BFRangeChromaShift        = shift;
    tnr3config->SADMultiplier             = tnr3Params.SADMultiplier;
    tnr3config->SADWeightLuma             = tnr3Params.SADWeightLuma;
    tnr3config->TempAlphaRestrictIncCap   = tnr3Params.TempAlphaRestrictIncCap;
    tnr3config->AlphaScaleIIR             = tnr3Params.AlphaScaleIIR;
    tnr3config->AlphaClipMaxLuma          = tnr3Params.AlphaClipMaxLuma;
    tnr3config->AlphaClipMinLuma          = tnr3Params.AlphaClipMinLuma;
    tnr3config->AlphaClipMaxChroma        = tnr3Params.AlphaClipMaxChroma;
    tnr3config->AlphaClipMinChroma        = tnr3Params.AlphaClipMinChroma;
    tnr3config->BetaCalcMaxBeta           = tnr3Params.BetaCalcMaxBeta;
    tnr3config->BetaCalcMinBeta           = tnr3Params.BetaCalcMinBeta;
    tnr3config->BetaCalcBetaX1            = tnr3Params.BetaCalcBetaX1;
    tnr3config->BetaCalcBetaX2            = tnr3Params.BetaCalcBetaX2;
    tnr3config->BetaCalcStepBeta          = GetBetaCalcStepBeta((int)
            (float(tnr3config->BetaCalcMaxBeta - tnr3config->BetaCalcMinBeta) /
             float(tnr3config->BetaCalcBetaX2 - tnr3config->BetaCalcBetaX1) * 32), 0, 64*32-1);

    int domainSigmaLuma   = tnr3Params.domainSigmaLuma;
    int domainSigmaChroma = tnr3Params.domainSigmaChroma;

    tnr3config->BFDomainLumaCoeffC00 = (uchar)64*exp(-(double)(0*0+0*0)/
            (2*domainSigmaLuma*domainSigmaLuma));
    tnr3config->BFDomainLumaCoeffC01 = (uchar)64*exp(-(double)(0*0+1*1)/
            (2*domainSigmaLuma*domainSigmaLuma));
    tnr3config->BFDomainLumaCoeffC02 = (uchar)64*exp(-(double)(0*0+2*2)/
            (2*domainSigmaLuma*domainSigmaLuma));
    tnr3config->BFDomainLumaCoeffC11 = (uchar)64*exp(-(double)(1*1+1*1)/
            (2*domainSigmaLuma*domainSigmaLuma));
    tnr3config->BFDomainLumaCoeffC12 = (uchar)64*exp(-(double)(1*1+2*2)/
            (2*domainSigmaLuma*domainSigmaLuma));
    tnr3config->BFDomainLumaCoeffC22 = (uchar)64*exp(-(double)(2*2+2*2)/
            (2*domainSigmaLuma*domainSigmaLuma));
    tnr3config->BFDomainChromaCoeffC00 = (uchar)64*exp(-(double)(0*0+0*0)/
            (2*domainSigmaChroma*domainSigmaChroma));
    tnr3config->BFDomainChromaCoeffC01 = (uchar)64*exp(-(double)(0*0+1*1)/
            (2*domainSigmaChroma*domainSigmaChroma));
    tnr3config->BFDomainChromaCoeffC02 = (uchar)64*exp(-(double)(0*0+2*2)/
            (2*domainSigmaChroma*domainSigmaChroma));
    tnr3config->BFDomainChromaCoeffC11 = (uchar)64*exp(-(double)(1*1+1*1)/
            (2*domainSigmaChroma*domainSigmaChroma));
    tnr3config->BFDomainChromaCoeffC12 = (uchar)64*exp(-(double)(1*1+2*2)/
            (2*domainSigmaChroma*domainSigmaChroma));
    tnr3config->BFDomainChromaCoeffC22 = (uchar)64*exp(-(double)(2*2+2*2)/
            (2*domainSigmaChroma*domainSigmaChroma));

    UINT32 destFrameWidthAlign = 64;
    UINT32 destFrameWidthAliglwal = ALIGN_UP(m_OutputParams.imageWidth,
            destFrameWidthAlign);
    UINT32 destFrameHeightAlign = 16;
    UINT32 destFrameHeightAliglwal = ALIGN_UP(m_OutputParams.imageHeight,
            destFrameHeightAlign);
    UINT32 bufSizeAlign = 256;
    tnr3config->LeftBufSize             = ALIGN_UP((destFrameHeightAliglwal/16)
            *TNR3LEFTCOLTILESIZE, bufSizeAlign);
    tnr3config->TopBufSize              = ALIGN_UP((destFrameWidthAliglwal/64)
            *TNR3ABOVEROWTILESIZE, bufSizeAlign);
    tnr3config->AlphaSufStride          = ALIGN_UP(8*(destFrameWidthAliglwal/2),
            destFrameWidthAliglwal) / 64 - 1;
    geoTranStruct->XSobelTopOffset    = ALIGN_UP(destFrameHeightAliglwal*2*8, bufSizeAlign);
    return OK;
}

//--------------------------------------------------------------------
//! \brief Write methods to the VIC engine to composit one frame
//!
/* virtual */ RC VicLdcTnr42Helper::WriteMethods
(
    const void *pConfigStruct,
    const SrcImages &srcImages,
    const unique_ptr<Image> &dstImage
)
{
    const Vic42::GeoTranConfigStruct &cfg =
        *reinterpret_cast<const Vic42::GeoTranConfigStruct*>(pConfigStruct);

    const Vic42::GeoTranConfigParamStruct *geoTranStruct = &cfg.paramConfig;
    const Vic42::TNR3ConfigParamStruct *tnr3config = &cfg.tnr3Config;

    Surface2D sparseWarpMapBuffer;
    RC rc;

    UINT32 destFrameWidthAlign = 64;
    UINT32 destFrameWidthAliglwal = ALIGN_UP(m_OutputParams.imageWidth
            ,destFrameWidthAlign);
    UINT32 destFrameHeightAlign = 16;
    UINT32 destFrameHeightAliglwal = ALIGN_UP(m_OutputParams.imageHeight
            ,destFrameHeightAlign);
    UINT32 neighDataSizeAlign = 256;
    UINT32 alphaSize = ALIGN_UP
            (destFrameWidthAliglwal/2 * destFrameHeightAliglwal/2, neighDataSizeAlign);

    // Write setup methods
    //
    CHECK_RC(m_pCh->Write(0, LWC5B6_VIDEO_COMPOSITOR_SET_APPLICATION_ID, 0x2));
    CHECK_RC(m_pCh->Write(
                    0, LWC5B6_VIDEO_COMPOSITOR_SET_CONTROL_PARAMS,
                    DRF_NUM(C5B6_VIDEO_COMPOSITOR, _SET_CONTROL_PARAMS,
                            _CONFIG_STRUCT_SIZE,
                            static_cast<UINT32>(sizeof(Vic42::GeoTranConfigStruct) >> 4))));
    CHECK_RC(m_pCh->Write(0, LWC5B6_VIDEO_COMPOSITOR_SET_CONTEXT_ID, 0));

    SurfaceUtils::MappingSurfaceWriter surfaceWriter(m_CfgSurface);
    CHECK_RC(surfaceWriter.WriteBytes(0, &cfg, sizeof(Vic42::GeoTranConfigStruct)));
    CHECK_RC(m_pCh->WriteWithSurface(
                    0, LWC5B6_VIDEO_COMPOSITOR_SET_CONFIG_STRUCT_OFFSET,
                    m_CfgSurface, 0, Vic42::SURFACE_SHIFT));

    if (tnr3config->TNR3En)
    {
        // This buffer is needed by hardware when enabling tnr3
        CHECK_RC(m_pCh->WriteWithSurface(
                        0, LWC5B6_VIDEO_COMPOSITOR_SET_TNR3_NEIGHBOR_BUFFER_OFFSET,
                        m_Tnr3NeighBuffer, 0, Vic42::SURFACE_SHIFT));

        // This buffer is needed by hardware when enabling tnr3
        CHECK_RC(m_pCh->WriteWithSurface(
                        0, LWC5B6_VIDEO_COMPOSITOR_SET_TNR3_LWR_ALPHA_SURFACE_OFFSET,
                        m_Tnr3LwrrAlphaBuffer, 0, Vic42::SURFACE_SHIFT));

        if (m_pTest->GetTestSampleTNR3Image())
        {
            FileHolder fileHolder;
            string filename = "vic_tnr3_preAlpha_image.bin";

            CHECK_RC(fileHolder.Open(filename, "rb"));

            vector<UINT08> data(alphaSize);
            if (fread(&data[0], alphaSize, 1, fileHolder.GetFile()) != 1)
            {
                Printf(Tee::PriError, "Failed to read file %s\n", filename.c_str());
                return RC::FILE_READ_ERROR;
            }
            SurfaceUtils::MappingSurfaceWriter surfaceWriter(m_Tnr3PreAlphaBuffer);
            CHECK_RC(surfaceWriter.WriteBytes(0, &data[0], alphaSize));
        }
        CHECK_RC(m_pCh->WriteWithSurface(
                        0, LWC5B6_VIDEO_COMPOSITOR_SET_TNR3_PREV_ALPHA_SURFACE_OFFSET,
                        m_Tnr3PreAlphaBuffer, 0, Vic42::SURFACE_SHIFT));
    }
    CHECK_RC(m_pCh->WriteWithSurface(
                    0, LWC5B6_VIDEO_COMPOSITOR_SET_CRC_STRUCT_OFFSET,
                    m_CrcStruct, 0, Vic42::SURFACE_SHIFT));

    if (geoTranStruct->GeoTranMode == 0 && geoTranStruct->GeoTranEn == 1)
    {
        UINT32 warpmapBufSize = m_InputParams.ldcParams.sparseWarpMapStride
                * m_InputParams.ldcParams.sparseWarpMapHeight;

        if (m_pTest->GetTestSampleLDCImage())
        {
            CHECK_RC(AllocDataSurface(&sparseWarpMapBuffer, warpmapBufSize,
                    "vic_ldc_sparseWrapImage.bin"));
        }
        else
        {
            CHECK_RC(AllocDataSurface(&sparseWarpMapBuffer, NULL, warpmapBufSize, true));
        }

        CHECK_RC(m_pCh->WriteWithSurface(
                    0, LWC5B6_VIDEO_COMPOSITOR_SET_SPARSE_WARP_MAP_OFFSET,
                    sparseWarpMapBuffer, 0, Vic42::SURFACE_SHIFT));
    }

    CHECK_RC(dstImage->Write(
                m_pCh,
                LWC5B6_VIDEO_COMPOSITOR_SET_OUTPUT_SURFACE_LUMA_OFFSET,
                LWC5B6_VIDEO_COMPOSITOR_SET_OUTPUT_SURFACE_CHROMA_U_OFFSET,
                LWC5B6_VIDEO_COMPOSITOR_SET_OUTPUT_SURFACE_CHROMA_V_OFFSET));

    // Write the src images
    //
    for (SrcImages::ImageMap::const_iterator
             iter = srcImages.images.begin();
             iter != srcImages.images.end(); ++iter)
    {
        const UINT32 surfaceIdx = iter->first;
        const Image &image = *iter->second;

        if (surfaceIdx == Vic42::LWRRENT_FIELD)
        {
            CHECK_RC(image.Write(m_pCh,
                             GetLumaMethod(0, surfaceIdx),
                             GetChromaUMethod(0, surfaceIdx),
                             GetChromaVMethod(0, surfaceIdx)));
        }
        else if (surfaceIdx == Vic42::PREV_FIELD)
        {
            if (tnr3config->TNR3En == 1)
            {
                CHECK_RC(image.Write(m_pCh,
                    LWC5B6_VIDEO_COMPOSITOR_SET_TNR3_PREV_FRM_SURFACE_LUMA_OFFSET,
                    LWC5B6_VIDEO_COMPOSITOR_SET_TNR3_PREV_FRM_SURFACE_CHROMA_U_OFFSET,
                    LWC5B6_VIDEO_COMPOSITOR_SET_TNR3_PREV_FRM_SURFACE_CHROMA_V_OFFSET));
            }
        }
    }
    // Launch the operation and wait for VIC to finish
    //
    CHECK_RC(m_pCh->Write(
            0, LWC5B6_VIDEO_COMPOSITOR_EXELWTE,
            DRF_DEF(C5B6, _VIDEO_COMPOSITOR_EXELWTE, _AWAKEN, _ENABLE)));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_pCh->WaitIdle(m_pTestConfig->TimeoutMs()));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Dump a VIC4 config struct to a human-readable text file
//!
/* virtual */ RC VicLdcTnr42Helper::DumpConfigStruct
(
    FILE *pFile,
    const void *pConfigStruct
) const
{
//Enabled only during development if needed
#ifdef ISDEVELOPMENTBUILD
    const Vic42::GeoTranConfigStruct *geoTranConfigStruct =
            reinterpret_cast<const Vic42::GeoTranConfigStruct*>(pConfigStruct);
    const Vic42::GeoTranConfigParamStruct *geoTranStruct = &geoTranConfigStruct->paramConfig;
    const Vic42::TNR3ConfigParamStruct *tnr3config = &geoTranConfigStruct->tnr3Config;
    const Vic42::BFRangeTableItems *bfRangeTableLuma =
            &geoTranConfigStruct->BFRangeTableLuma[0];
    const Vic42::BFRangeTableItems *bfRangeTableChroma =
            &geoTranConfigStruct->BFRangeTableChroma[0];
    const Vic42::CoeffPhaseParamStruct *filterCoeff = &geoTranConfigStruct->FilterCoeff[0];

    fprintf(pFile, "GeoTranEn           :%ld\n", geoTranStruct->GeoTranEn);
    fprintf(pFile, "GeoTranMode         :%ld\n", geoTranStruct->GeoTranMode);
    fprintf(pFile, "IPTMode             :%ld\n", geoTranStruct->IPTMode);
    fprintf(pFile, "PixelFilterType     :%ld\n", geoTranStruct->PixelFilterType);
    fprintf(pFile, "PixelFormat         :%ld\n", geoTranStruct->PixelFormat);
    fprintf(pFile, "CacheWidth          :%ld\n", geoTranStruct->CacheWidth);
    fprintf(pFile, "SrcBlkKind          :%ld\n", geoTranStruct->SrcBlkKind);
    fprintf(pFile, "SrcBlkHeight        :%ld\n", geoTranStruct->SrcBlkHeight);
    fprintf(pFile, "DestBlkKind         :%ld\n", geoTranStruct->DestBlkKind);
    fprintf(pFile, "DestBlkHeight       :%ld\n", geoTranStruct->DestBlkHeight);
    fprintf(pFile, "MskBitMapEn         :%ld\n", geoTranStruct->MskBitMapEn);
    fprintf(pFile, "MaskedPixelFillMode :%ld\n", geoTranStruct->MaskedPixelFillMode);
    fprintf(pFile, "XSobelMode          :%ld\n", geoTranStruct->XSobelMode);
    fprintf(pFile, "SubFrameEn          :%ld\n", geoTranStruct->SubFrameEn);
    fprintf(pFile, "XSobelBlkKind       :%ld\n", geoTranStruct->XSobelBlkKind);
    fprintf(pFile, "XSobelBlkHeight     :%ld\n", geoTranStruct->XSobelBlkHeight);
    fprintf(pFile, "XSobelDSBlkKind     :%ld\n", geoTranStruct->XSobelDSBlkKind);
    fprintf(pFile, "XSobelDSBlkHeight   :%ld\n", geoTranStruct->XSobelDSBlkHeight);
    fprintf(pFile, "NonFixedPatchEn     :%ld\n", geoTranStruct->NonFixedPatchEn);
    fprintf(pFile, "HorRegionNum        :%ld\n", geoTranStruct->HorRegionNum);
    fprintf(pFile, "VerRegionNum        :%ld\n", geoTranStruct->VerRegionNum);
    fprintf(pFile, "log2HorSpace_0      :%ld\n", geoTranStruct->log2HorSpace_0);
    fprintf(pFile, "log2VerSpace_0      :%ld\n", geoTranStruct->log2VerSpace_0);
    fprintf(pFile, "log2HorSpace_1      :%ld\n", geoTranStruct->log2HorSpace_1);
    fprintf(pFile, "log2VerSpace_1      :%ld\n", geoTranStruct->log2VerSpace_1);
    fprintf(pFile, "log2HorSpace_2      :%ld\n", geoTranStruct->log2HorSpace_2);
    fprintf(pFile, "log2VerSpace_2      :%ld\n", geoTranStruct->log2VerSpace_2);
    fprintf(pFile, "log2HorSpace_3      :%ld\n", geoTranStruct->log2HorSpace_3);
    fprintf(pFile, "log2VerSpace_3      :%ld\n", geoTranStruct->log2VerSpace_3);
    fprintf(pFile, "horRegionWidth_0    :%ld\n", geoTranStruct->horRegionWidth_0);
    fprintf(pFile, "horRegionWidth_1    :%ld\n", geoTranStruct->horRegionWidth_1);
    fprintf(pFile, "horRegionWidth_2    :%ld\n", geoTranStruct->horRegionWidth_2);
    fprintf(pFile, "horRegionWidth_3    :%ld\n", geoTranStruct->horRegionWidth_3);
    fprintf(pFile, "verRegionHeight_0   :%ld\n", geoTranStruct->verRegionHeight_0);
    fprintf(pFile, "verRegionHeight_1   :%ld\n", geoTranStruct->verRegionHeight_1);
    fprintf(pFile, "verRegionHeight_2   :%ld\n", geoTranStruct->verRegionHeight_2);
    fprintf(pFile, "verRegionHeight_3   :%ld\n", geoTranStruct->verRegionHeight_3);
    fprintf(pFile, "IPT_M11             :%ld\n", geoTranStruct->IPT_M11);
    fprintf(pFile, "IPT_M12             :%ld\n", geoTranStruct->IPT_M12);
    fprintf(pFile, "IPT_M13             :%ld\n", geoTranStruct->IPT_M13);
    fprintf(pFile, "IPT_M21             :%ld\n", geoTranStruct->IPT_M21);
    fprintf(pFile, "IPT_M22             :%ld\n", geoTranStruct->IPT_M22);
    fprintf(pFile, "IPT_M23             :%ld\n", geoTranStruct->IPT_M23);
    fprintf(pFile, "IPT_M31             :%ld\n", geoTranStruct->IPT_M31);
    fprintf(pFile, "IPT_M32             :%ld\n", geoTranStruct->IPT_M32);
    fprintf(pFile, "IPT_M33             :%ld\n", geoTranStruct->IPT_M33);
    fprintf(pFile, "SourceRectLeft      :%ld\n", geoTranStruct->SourceRectLeft);
    fprintf(pFile, "SourceRectRight     :%ld\n", geoTranStruct->SourceRectRight);
    fprintf(pFile, "SourceRectTop       :%ld\n", geoTranStruct->SourceRectTop);
    fprintf(pFile, "SourceRectBottom    :%ld\n", geoTranStruct->SourceRectBottom);
    fprintf(pFile, "SrcImgWidth         :%ld\n", geoTranStruct->SrcImgWidth);
    fprintf(pFile, "SrcImgHeight        :%ld\n", geoTranStruct->SrcImgHeight);
    fprintf(pFile, "SrcSfcLumaWidth     :%ld\n", geoTranStruct->SrcSfcLumaWidth);
    fprintf(pFile, "SrcSfcLumaHeight    :%ld\n", geoTranStruct->SrcSfcLumaHeight);
    fprintf(pFile, "SrcSfcChromaWidth   :%ld\n", geoTranStruct->SrcSfcChromaWidth);
    fprintf(pFile, "SrcSfcChromaHeight  :%ld\n", geoTranStruct->SrcSfcChromaHeight);
    fprintf(pFile, "DestRectLeft        :%ld\n", geoTranStruct->DestRectLeft);
    fprintf(pFile, "DestRectRight       :%ld\n", geoTranStruct->DestRectRight);
    fprintf(pFile, "DestRectTop         :%ld\n", geoTranStruct->DestRectTop);
    fprintf(pFile, "DestRectBottom      :%ld\n", geoTranStruct->DestRectBottom);
    fprintf(pFile, "DestImageWidth      :%d\n",  m_OutputParams.imageWidth);
    fprintf(pFile, "DestImageHeight     :%d\n",  m_OutputParams.imageHeight);
    fprintf(pFile, "SubFrameRectTop     :%ld\n", geoTranStruct->SubFrameRectTop);
    fprintf(pFile, "SubFrameRectBottom  :%ld\n", geoTranStruct->SubFrameRectBottom);
    fprintf(pFile, "DestSfcLumaWidth    :%ld\n", geoTranStruct->DestSfcLumaWidth);
    fprintf(pFile, "DestSfcLumaHeight   :%ld\n", geoTranStruct->DestSfcLumaHeight);
    fprintf(pFile, "DestSfcChromaWidth  :%ld\n", geoTranStruct->DestSfcChromaWidth);
    fprintf(pFile, "DestSfcChromaHeight :%ld\n", geoTranStruct->DestSfcChromaHeight);
    fprintf(pFile, "SparseWarpMapWidth  :%ld\n", geoTranStruct->SparseWarpMapWidth);
    fprintf(pFile, "SparseWarpMapHeight :%ld\n", geoTranStruct->SparseWarpMapHeight);
    fprintf(pFile, "SparseWarpMapStride :%ld\n", geoTranStruct->SparseWarpMapStride);
    fprintf(pFile, "MaskBitMapWidth     :%ld\n", geoTranStruct->MaskBitMapWidth);
    fprintf(pFile, "MaskBitMapHeight    :%ld\n", geoTranStruct->MaskBitMapHeight);
    fprintf(pFile, "MaskBitMapStride    :%ld\n", geoTranStruct->MaskBitMapStride);
    fprintf(pFile, "XSobelWidth         :%ld\n", geoTranStruct->XSobelWidth);
    fprintf(pFile, "XSobelHeight        :%ld\n", geoTranStruct->XSobelHeight);
    fprintf(pFile, "XSobelStride        :%ld\n", geoTranStruct->XSobelStride);
    fprintf(pFile, "DSStride            :%ld\n", geoTranStruct->DSStride);
    fprintf(pFile, "XSobelTopOffset     :%ld\n", geoTranStruct->XSobelTopOffset);
    fprintf(pFile, "maskY               :%ld\n", geoTranStruct->maskY);
    fprintf(pFile, "maskU               :%ld\n", geoTranStruct->maskU);
    fprintf(pFile, "maskV               :%ld\n", geoTranStruct->maskV);

    for (int i = 0; i < 17; i++)
    {
        fprintf(pFile, "FilerCoeff[%d].coeff_0   : %ld\n", i, filterCoeff[i].coeff_0);
        fprintf(pFile, "FilerCoeff[%d].coeff_1   : %ld\n", i, filterCoeff[i].coeff_1);
        fprintf(pFile, "FilerCoeff[%d].coeff_2   : %ld\n", i, filterCoeff[i].coeff_2);
        fprintf(pFile, "FilerCoeff[%d].coeff_3   : %ld\n", i, filterCoeff[i].coeff_3);
    }

    fprintf(pFile, "TNR3En                  : %ld\n", tnr3config->TNR3En);
    fprintf(pFile, "BetaBlendingEn          : %ld\n", tnr3config->BetaBlendingEn);
    fprintf(pFile, "AlphaBlendingEn         : %ld\n", tnr3config->AlphaBlendingEn);
    fprintf(pFile, "AlphaSmoothEn           : %ld\n", tnr3config->AlphaSmoothEn);
    fprintf(pFile, "TempAlphaRestrictEn     : %ld\n", tnr3config->TempAlphaRestrictEn);
    fprintf(pFile, "AlphaClipEn             : %ld\n", tnr3config->AlphaClipEn);
    fprintf(pFile, "BFRangeEn               : %ld\n", tnr3config->BFRangeEn);
    fprintf(pFile, "BFDomainEn              : %ld\n", tnr3config->BFDomainEn);
    fprintf(pFile, "BFRangeLumaShift        : %ld\n", tnr3config->BFRangeLumaShift);
    fprintf(pFile, "BFRangeChromaShift      : %ld\n", tnr3config->BFRangeChromaShift);
    fprintf(pFile, "SADMultiplier           : %ld\n", tnr3config->SADMultiplier);
    fprintf(pFile, "SADWeightLuma           : %ld\n", tnr3config->SADWeightLuma);
    fprintf(pFile, "TempAlphaRestrictIncCap : %ld\n", tnr3config->TempAlphaRestrictIncCap);
    fprintf(pFile, "AlphaScaleIIR           : %ld\n", tnr3config->AlphaScaleIIR);
    fprintf(pFile, "AlphaClipMaxLuma        : %ld\n", tnr3config->AlphaClipMaxLuma);
    fprintf(pFile, "AlphaClipMinLuma        : %ld\n", tnr3config->AlphaClipMinLuma);
    fprintf(pFile, "AlphaClipMaxChroma      : %ld\n", tnr3config->AlphaClipMaxChroma);
    fprintf(pFile, "AlphaClipMinChroma      : %ld\n", tnr3config->AlphaClipMinChroma);
    fprintf(pFile, "BetaCalcMaxBeta         : %ld\n", tnr3config->BetaCalcMaxBeta);
    fprintf(pFile, "BetaCalcMinBeta         : %ld\n", tnr3config->BetaCalcMinBeta);
    fprintf(pFile, "BetaCalcBetaX1          : %ld\n", tnr3config->BetaCalcBetaX1);
    fprintf(pFile, "BetaCalcBetaX2          : %ld\n", tnr3config->BetaCalcBetaX2);
    fprintf(pFile, "BetaCalcStepBeta        : %ld\n", tnr3config->BetaCalcStepBeta);
    fprintf(pFile, "BFDomainLumaCoeffC00    : %ld\n", tnr3config->BFDomainLumaCoeffC00);
    fprintf(pFile, "BFDomainLumaCoeffC01    : %ld\n", tnr3config->BFDomainLumaCoeffC01);
    fprintf(pFile, "BFDomainLumaCoeffC02    : %ld\n", tnr3config->BFDomainLumaCoeffC02);
    fprintf(pFile, "BFDomainLumaCoeffC11    : %ld\n", tnr3config->BFDomainLumaCoeffC11);
    fprintf(pFile, "BFDomainLumaCoeffC12    : %ld\n", tnr3config->BFDomainLumaCoeffC12);
    fprintf(pFile, "BFDomainLumaCoeffC22    : %ld\n", tnr3config->BFDomainLumaCoeffC22);
    fprintf(pFile, "BFDomainChromaCoeffC00  : %ld\n", tnr3config->BFDomainChromaCoeffC00);
    fprintf(pFile, "BFDomainChromaCoeffC01  : %ld\n", tnr3config->BFDomainChromaCoeffC01);
    fprintf(pFile, "BFDomainChromaCoeffC02  : %ld\n", tnr3config->BFDomainChromaCoeffC02);
    fprintf(pFile, "BFDomainChromaCoeffC11  : %ld\n", tnr3config->BFDomainChromaCoeffC11);
    fprintf(pFile, "BFDomainChromaCoeffC12  : %ld\n", tnr3config->BFDomainChromaCoeffC12);
    fprintf(pFile, "BFDomainChromaCoeffC22  : %ld\n", tnr3config->BFDomainChromaCoeffC22);
    fprintf(pFile, "LeftBufSize             : %ld\n", tnr3config->LeftBufSize);
    fprintf(pFile, "TopBufSize              : %ld\n", tnr3config->TopBufSize);
    fprintf(pFile, "AlphaSufStride          : %ld\n", tnr3config->AlphaSufStride);

    for (int i = 0; i < 16; i++)
    {
        fprintf(pFile, "bfRangeTableLuma[%d].item0: %ld\n", i, bfRangeTableLuma[i].item0);
        fprintf(pFile, "bfRangeTableLuma[%d].item1: %ld\n", i, bfRangeTableLuma[i].item1);
        fprintf(pFile, "bfRangeTableLuma[%d].item2: %ld\n", i, bfRangeTableLuma[i].item2);
        fprintf(pFile, "bfRangeTableLuma[%d].item3: %ld\n", i, bfRangeTableLuma[i].item3);
    }

    for (int i = 0; i < 16; i++)
    {
        fprintf(pFile, "bfRangeTableChroma[%d].item0: %ld\n", i, bfRangeTableChroma[i].item0);
        fprintf(pFile, "bfRangeTableChroma[%d].item1: %ld\n", i, bfRangeTableChroma[i].item1);
        fprintf(pFile, "bfRangeTableChroma[%d].item2: %ld\n", i, bfRangeTableChroma[i].item2);
        fprintf(pFile, "bfRangeTableChroma[%d].item3: %ld\n", i, bfRangeTableChroma[i].item3);
    }
#endif
    return OK;
}

//--------------------------------------------------------------------
//! \brief Verify that the VIC 4.2 config struct contains valid values
//!
/* virtual */ bool VicLdcTnr42Helper::CheckConfigStruct
(
    const void *pConfigStruct
) const
{
    //TODO Implementation
    return true;
}

//--------------------------------------------------------------------
//! Return the LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_LUMA_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 VicLdcTnr42Helper::GetLumaMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    UINT32 SURFACE_STRIDE =
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE1_LUMA_OFFSET(0) -
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_LUMA_OFFSET(0);
    return (LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_LUMA_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}

//--------------------------------------------------------------------
//! Return the LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_CHROMA_U_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 VicLdcTnr42Helper::GetChromaUMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    UINT32 SURFACE_STRIDE =
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE1_CHROMA_U_OFFSET(0) -
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_U_OFFSET(0);
    return (LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_U_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}

//--------------------------------------------------------------------
//! Return the LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_CHROMA_V_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 VicLdcTnr42Helper::GetChromaVMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    UINT32 SURFACE_STRIDE =
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE1_CHROMA_V_OFFSET(0) -
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_V_OFFSET(0);
    return (LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_V_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}
