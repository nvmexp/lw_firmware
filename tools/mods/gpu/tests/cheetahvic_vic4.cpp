/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "tegravic.h"
#include "core/include/channel.h"
#include "class/clb0b6.h"
#include "class/clb1b6.h"
#include "class/clc5b6.h"
#include "core/include/utility.h"

//--------------------------------------------------------------------
//! \brief Write a VIC4 config struct, based on the fancyPicker loop vars
//!
//! \param[out] pCfgBuffer On exit, holds the config struct.  We
//!     return the struct as a generic buffer because the base class
//!     needs to be able to handle both VIC3 and VIC4 structs.
//! \param srcImages The allocated source images.
//!
/* virtual */ RC TegraVic4Helper::FillConfigStruct
(
    vector<UINT08> *pCfgBuffer,
    const SrcImages &srcImages
) const
{
    pCfgBuffer->resize(GetConfigSize());
    Vic4::ConfigStruct *pCfg =
        reinterpret_cast<Vic4::ConfigStruct*>(&(*pCfgBuffer)[0]);

    memset(pCfg, 0, GetConfigSize());

    // Fill pCfg->pipeConfig
    //
    Vic4::PipeConfig *pPipeConfig = &pCfg->pipeConfig;
    pPipeConfig->DownsampleHoriz = 0x4;
    pPipeConfig->DownsampleVert  = 0x4;

    // Fill pCfg->outputConfig
    //
    Vic4::OutputConfig *pOutConfig = &pCfg->outputConfig;
    pOutConfig->AlphaFillMode    = IGNORED;
    pOutConfig->AlphaFillSlot    = IGNORED;
    pOutConfig->BackgroundAlpha  = ALL_ONES;
    pOutConfig->BackgroundR      = IGNORED;
    pOutConfig->BackgroundG      = IGNORED;
    pOutConfig->BackgroundB      = IGNORED;
    pOutConfig->RegammaMode      = IGNORED;
    pOutConfig->OutputFlipX      = m_FlipX;
    pOutConfig->OutputFlipY      = m_FlipY;
    pOutConfig->OutputTranspose  = m_Transpose;
    pOutConfig->TargetRectLeft   = 0;
    pOutConfig->TargetRectRight  = m_DstImageWidth - 1;
    pOutConfig->TargetRectTop    = 0;
    pOutConfig->TargetRectBottom = m_DstImageHeight - 1;

    // Fill pCfg->outputSurfaceConfig
    //
    Vic4::OutputSurfaceConfig *pOutSurf = &pCfg->outputSurfaceConfig;
    pOutSurf->OutPixelFormat    = m_TrDstDimensions.pixelFormat;
    pOutSurf->OutChromaLocHoriz = (m_Transpose ?
                                   m_TrDstDimensions.chromaLocVert :
                                   m_TrDstDimensions.chromaLocHoriz);
    pOutSurf->OutChromaLocVert  = (m_Transpose ?
                                   m_TrDstDimensions.chromaLocHoriz :
                                   m_TrDstDimensions.chromaLocVert);
    pOutSurf->OutBlkKind        = m_TrDstDimensions.blockKind;
    pOutSurf->OutBlkHeight      = 0x0;
    pOutSurf->OutSurfaceWidth   = m_DstImageWidth - 1;
    pOutSurf->OutSurfaceHeight  = m_DstImageHeight - 1;
    pOutSurf->OutLumaWidth      = (m_Transpose ?
                                   m_TrDstDimensions.lumaHeight - 1 :
                                   m_TrDstDimensions.lumaWidth - 1);
    pOutSurf->OutLumaHeight     = (m_Transpose ?
                                   m_TrDstDimensions.lumaWidth - 1 :
                                   m_TrDstDimensions.lumaHeight - 1);
    pOutSurf->OutChromaWidth    = (m_Transpose ?
                                   m_TrDstDimensions.chromaHeight - 1 :
                                   m_TrDstDimensions.chromaWidth - 1);
    pOutSurf->OutChromaHeight   = (m_Transpose ?
                                   m_TrDstDimensions.chromaWidth - 1 :
                                   m_TrDstDimensions.chromaHeight - 1);

    // Ignore pCfg->outColorMatrixStruct
    // Ignore pCfg->clearRectStruct

    // Fill pCfg->slotStruct
    //
    for (UINT32 slotIdx = 0; slotIdx < GetNumSlots(); ++slotIdx)
    {
        FillSlotStruct(pCfg, srcImages, slotIdx);
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Used by FillConfigStruct() to fill one element of slotStruct[]
//!
//! \param[out] pCfg The config struct being written by FillConfigStruct()
//! \param srcImages The src images passed to FillConfigStruct()
//! \param slotIdx Which element of the slotStruct[] array to fill
//!
void TegraVic4Helper::FillSlotStruct
(
    Vic4::ConfigStruct *pCfg,
    const SrcImages &srcImages,
    UINT32 slotIdx
) const
{
    Vic4::SlotStruct *pSlotStruct = &pCfg->slotStruct[slotIdx];
    const ImageDimensions &dimensions = srcImages[slotIdx].dimensions;
    const SrcImagesForSlot::ImageMap &images = srcImages[slotIdx].images;
    const SlotPicks &slotPicks = m_SlotPicks[slotIdx];

    // Fill pSlotStruct->slotConfig
    //
    Vic4::SlotConfig *pSlotCfg = &pSlotStruct->slotConfig;
    pSlotCfg->SlotEnable            = true;
    pSlotCfg->DeNoise               = slotPicks.deNoise;
    pSlotCfg->AdvancedDenoise       = slotPicks.deNoise;
    pSlotCfg->CadenceDetect         = slotPicks.cadenceDetect;
    pSlotCfg->MotionMap             = slotPicks.HasMotionMap();
    pSlotCfg->MMapCombine           = slotPicks.HasMotionMap();
    pSlotCfg->IsEven                = IGNORED;
    pSlotCfg->ChromaEven            = IGNORED;
    pSlotCfg->LwrrentFieldEnable    = images.count(Vic4::LWRRENT_FIELD);
    pSlotCfg->PrevFieldEnable       = images.count(Vic4::PREV_FIELD);
    pSlotCfg->NextFieldEnable       = images.count(Vic4::NEXT_FIELD);
    pSlotCfg->NextNrFieldEnable     = images.count(Vic4::NEXT_FIELD_N);
    pSlotCfg->LwrMotionFieldEnable  = images.count(Vic4::LWRR_M_FIELD);
    pSlotCfg->PrevMotionFieldEnable = images.count(Vic4::PREV_M_FIELD);
    pSlotCfg->PpMotionFieldEnable   = false;
    pSlotCfg->CombMotionFieldEnable = images.count(Vic4::COMB_M_FIELD);
    pSlotCfg->FrameFormat           = slotPicks.frameFormat;
    pSlotCfg->FilterLengthY         = 0x2;
    pSlotCfg->FilterLengthX         = 0x2;
    pSlotCfg->Panoramic             = IGNORED;
    pSlotCfg->DetailFltClamp        = IGNORED;
    pSlotCfg->FilterNoise           = IGNORED;
    pSlotCfg->FilterDetail          = IGNORED;
    pSlotCfg->ChromaNoise           = IGNORED;
    pSlotCfg->ChromaDetail          = IGNORED;
    pSlotCfg->DeinterlaceMode       = slotPicks.deinterlaceMode;
    pSlotCfg->MotionAclwmWeight     = 0x6;
    pSlotCfg->NoiseIir              = 0x300;
    pSlotCfg->LightLevel            = ((slotPicks.deNoise)? slotPicks.lightLevel:IGNORED);
    pSlotCfg->SoftClampLow          = 0x0;
    pSlotCfg->SoftClampHigh         = ALL_ONES;
    pSlotCfg->PlanarAlpha           = ALL_ONES;
    pSlotCfg->ConstantAlpha         = !dimensions.hasAlpha;
    pSlotCfg->StereoInterleave      = IGNORED;
    pSlotCfg->ClipEnabled           = IGNORED;
    pSlotCfg->ClearRectMask         = IGNORED;
    pSlotCfg->DegammaMode           = slotPicks.deGammaMode;
    pSlotCfg->DecompressEnable      = IGNORED;
    pSlotCfg->DecompressCtbCount    = IGNORED;
    pSlotCfg->DecompressZbcColor    = IGNORED;
    pSlotCfg->SourceRectLeft        = IntToFxp(slotPicks.srcRect.left);
    pSlotCfg->SourceRectRight       = IntToFxp(slotPicks.srcRect.right);
    pSlotCfg->SourceRectTop         = IntToFxp(slotPicks.srcRect.top);
    pSlotCfg->SourceRectBottom      = IntToFxp(slotPicks.srcRect.bottom);
    pSlotCfg->DestRectLeft          = slotPicks.dstRect.left;
    pSlotCfg->DestRectRight         = slotPicks.dstRect.right;
    pSlotCfg->DestRectTop           = slotPicks.dstRect.top;
    pSlotCfg->DestRectBottom        = slotPicks.dstRect.bottom;

    // Fill pSlotStruct->slotSurfaceConfig
    //
    Vic4::SlotSurfaceConfig *pSlotSurf = &pSlotStruct->slotSurfaceConfig;
    pSlotSurf->SlotPixelFormat    = dimensions.pixelFormat;
    pSlotSurf->SlotChromaLocHoriz = dimensions.chromaLocHoriz;
    pSlotSurf->SlotChromaLocVert  = dimensions.chromaLocVert;
    pSlotSurf->SlotBlkKind        = dimensions.blockKind;
    pSlotSurf->SlotBlkHeight      = 0x0;
    pSlotSurf->SlotCacheWidth     = Vic4::CACHE_WIDTH_64Bx4;
    pSlotSurf->SlotSurfaceWidth   = dimensions.width        - 1;
    pSlotSurf->SlotSurfaceHeight  = dimensions.fieldHeight  - 1;
    pSlotSurf->SlotLumaWidth      = dimensions.lumaWidth    - 1;
    pSlotSurf->SlotLumaHeight     = dimensions.lumaHeight   - 1;
    pSlotSurf->SlotChromaWidth    = dimensions.chromaWidth  - 1;
    pSlotSurf->SlotChromaHeight   = dimensions.chromaHeight - 1;

    // Fill pSlotStruct->lumaKeyStruct
    //
    Vic4::LumaKeyStruct *pLumaKey = &pSlotStruct->lumaKeyStruct;
    switch (slotPicks.pixelFormat)
    {
        case Vic4::T_A8R8G8B8:
            pLumaKey->luma_coeff0  = 0x20de7;
            pLumaKey->luma_coeff1  = 0x40875;
            pLumaKey->luma_coeff2  = 0xc883;
            pLumaKey->luma_r_shift = 0xb;
            pLumaKey->luma_coeff3  = 0x4030;
            pLumaKey->LumaKeyLower = ALL_ONES;
            break;
        case Vic4::T_Y8___V8U8_N420:
            pLumaKey->luma_coeff0  = 0x40000;
            pLumaKey->luma_coeff1  = 0x0;
            pLumaKey->luma_coeff2  = 0x0;
            pLumaKey->luma_r_shift = 0xa;
            pLumaKey->luma_coeff3  = 0x0;
            pLumaKey->LumaKeyLower = ALL_ONES;
            break;
        default:
            MASSERT(!"Unsupported pixelFormat");
    }
    pLumaKey->LumaKeyUpper   = IGNORED;
    pLumaKey->LumaKeyEnabled = IGNORED;

    // Fill pSlotStruct->colorMatrixStruct
    //
    Vic4::MatrixStruct *pColorMatrix = &pSlotStruct->colorMatrixStruct;
    switch (slotPicks.pixelFormat)
    {
        case Vic4::T_A8R8G8B8:
            pColorMatrix->matrix_coeff00 = 0x40303;
            pColorMatrix->matrix_coeff10 = 0x0;
            pColorMatrix->matrix_coeff20 = 0x0;
            pColorMatrix->matrix_r_shift = 0xa;
            pColorMatrix->matrix_coeff01 = 0x0;
            pColorMatrix->matrix_coeff11 = 0x40303;
            pColorMatrix->matrix_coeff21 = 0x0;
            pColorMatrix->matrix_enable  = true;
            pColorMatrix->matrix_coeff02 = 0x0;
            pColorMatrix->matrix_coeff12 = 0x0;
            pColorMatrix->matrix_coeff22 = 0x40303;
            pColorMatrix->matrix_coeff03 = 0x0;
            pColorMatrix->matrix_coeff13 = 0x0;
            pColorMatrix->matrix_coeff23 = 0x0;
            break;
        case Vic4::T_Y8___V8U8_N420:
            pColorMatrix->matrix_coeff00 = 0x4abd6;
            pColorMatrix->matrix_coeff10 = 0x4abd6;
            pColorMatrix->matrix_coeff20 = 0x4abd6;
            pColorMatrix->matrix_r_shift = 0xa;
            pColorMatrix->matrix_coeff01 = 0x0;
            pColorMatrix->matrix_coeff11 = 0xe887b;
            pColorMatrix->matrix_coeff21 = 0x78d9b;
            pColorMatrix->matrix_enable  = true;
            pColorMatrix->matrix_coeff02 = 0x5f9dd;
            pColorMatrix->matrix_coeff12 = 0xcf4bd;
            pColorMatrix->matrix_coeff22 = 0x0;
            pColorMatrix->matrix_coeff03 = 0xcb5dd;
            pColorMatrix->matrix_coeff13 = 0x1f822;
            pColorMatrix->matrix_coeff23 = 0xbeb66;
            break;
        default:
            MASSERT(!"Unsupported pixelFormat");
    }

    // Fill pSlotStruct->gamutMatrixStruct
    Vic4::MatrixStruct *pGamutMatrix = &pSlotStruct->gamutMatrixStruct;
    pGamutMatrix->matrix_coeff00 = slotPicks.gamutMatrix[0][0];
    pGamutMatrix->matrix_coeff10 = slotPicks.gamutMatrix[1][0];
    pGamutMatrix->matrix_coeff20 = slotPicks.gamutMatrix[2][0];
    pGamutMatrix->matrix_r_shift = 0xa;
    pGamutMatrix->matrix_coeff01 = slotPicks.gamutMatrix[0][1];
    pGamutMatrix->matrix_coeff11 = slotPicks.gamutMatrix[1][1];
    pGamutMatrix->matrix_coeff21 = slotPicks.gamutMatrix[2][1];
    pGamutMatrix->matrix_enable  = true;
    pGamutMatrix->matrix_coeff02 = slotPicks.gamutMatrix[0][2];
    pGamutMatrix->matrix_coeff12 = slotPicks.gamutMatrix[1][2];
    pGamutMatrix->matrix_coeff22 = slotPicks.gamutMatrix[2][2];
    pGamutMatrix->matrix_coeff03 = slotPicks.gamutMatrix[0][3];
    pGamutMatrix->matrix_coeff13 = slotPicks.gamutMatrix[1][3];
    pGamutMatrix->matrix_coeff23 = slotPicks.gamutMatrix[2][3];

    // Fill pSlotStruct->blendingSlotStruct
    //
    Vic4::BlendingSlotStruct *pBlending = &pSlotStruct->blendingSlotStruct;
    pBlending->AlphaK1             = ALL_ONES;
    pBlending->AlphaK2             = IGNORED;
    pBlending->SrcFactCMatchSelect = Vic4::BLEND_SRCFACTC_K1_TIMES_SRC;
    pBlending->DstFactCMatchSelect = Vic4::BLEND_DSTFACTC_NEG_K1_TIMES_SRC;
    pBlending->SrcFactAMatchSelect = Vic4::BLEND_SRCFACTA_NEG_K1_TIMES_DST;
    pBlending->DstFactAMatchSelect = Vic4::BLEND_DSTFACTA_ONE;
    pBlending->OverrideR           = IGNORED;
    pBlending->OverrideG           = IGNORED;
    pBlending->OverrideB           = IGNORED;
    pBlending->OverrideA           = IGNORED;
    pBlending->UseOverrideR        = IGNORED;
    pBlending->UseOverrideG        = IGNORED;
    pBlending->UseOverrideB        = IGNORED;
    pBlending->UseOverrideA        = IGNORED;
    pBlending->MaskR               = IGNORED;
    pBlending->MaskG               = IGNORED;
    pBlending->MaskB               = IGNORED;
    pBlending->MaskA               = IGNORED;
}

//--------------------------------------------------------------------
// Function used by CheckConfigStruct(), copied from
// //hw/gpu_ip/unit/vic/4.0/cmod/driver/drv.cpp
namespace
{
    bool is_yuv_format(Vic4::PIXELFORMAT format)
    {
        using namespace Vic4;
        switch (format)
        {
            // 1BPP, 2BPP & Index surfaces
            case T_L8:
            case T_U8:
            case T_V8:
            case T_U8V8:
            case T_V8U8:
            case T_A4L4:
            case T_L4A4:
            case T_A8L8:
            case T_L8A8:
                // Packed YUV with Alpha formats
            case T_A8Y8U8V8:
            case T_V8U8Y8A8:
                // Packed subsampled YUV 422
            case T_Y8_U8__Y8_V8:
            case T_Y8_V8__Y8_U8:
            case T_U8_Y8__V8_Y8:
            case T_V8_Y8__U8_Y8:
                // Semi-planar YUV
            case T_Y8___U8V8_N444:
            case T_Y8___V8U8_N444:
            case T_Y8___U8V8_N422:
            case T_Y8___V8U8_N422:
            case T_Y8___U8V8_N422R:
            case T_Y8___V8U8_N422R:
            case T_Y8___U8V8_N420:
            case T_Y8___V8U8_N420:
                // Planar YUV
            case T_Y8___U8___V8_N444:
            case T_Y8___U8___V8_N422:
            case T_Y8___U8___V8_N422R:
            case T_Y8___U8___V8_N420:
                return true;
            default:
                return false;
        }
    }
};

//--------------------------------------------------------------------
//! \brief Verify that the VIC config struct contains valid values
//!
//! Copied from drv::check_config() in
//! //hw/gpu_ip/unit/vic/4.0/cmod/driver/drv.cpp
//!
/* virtual */ bool TegraVic4Helper::CheckConfigStruct
(
    const void *pConfigStruct
) const
{
    // Assorted hacks so that we can import drv::check_config() to
    // mods with minimal rewrite.
    //
    using namespace Vic4;
    const ConfigStruct *cfg = static_cast<const ConfigStruct*>(pConfigStruct);
    const UINT08 LW_VIC_SLOT_COUNT = Vic4::MAX_SLOTS;
#define fprintf FprintfForCheckConfig
#define sr_rnd_dn(fxp) static_cast<int>(FxpToIntFloor(fxp))
#define sr_rnd_up(fxp) static_cast<int>(FxpToIntCeil(fxp))
#define LW_CHIP_VIC_HISTOGRAM_CORRECTION_DISABLE 1
#define LW_CHIP_VIC_CENHANCE_DISABLE 1

    // ===============================================================
    // Everything below this point was copied from drv::check_config()
    // ===============================================================

    // we need this a couple of times.
    bool frame[LW_VIC_SLOT_COUNT];
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        frame[i] = ((cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_PROGRESSIVE) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_SUBPIC_PROGRESSIVE) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_TOP_FIELD_FIRST) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_BOTTOM_FIELD_FIRST));
    }
    bool advanced[LW_VIC_SLOT_COUNT];
    // is this too restrictive?  - SIVA: How to modify this
    for(int i=0;i<LW_VIC_SLOT_COUNT;i++) {
        advanced[i] = ((cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8___V8U8_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8___U8V8_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8_U8__Y8_V8) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8___U8___V8_N444) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8___U8___V8_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8___U8___V8_N422) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8___U8___V8_N422R) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_U8_Y8__V8_Y8));
    }

    unsigned int enable[LW_VIC_SLOT_COUNT];
    for(int i=0; i<LW_VIC_SLOT_COUNT; i++)    {
        enable[i] = cfg->slotStruct[i].slotConfig.LwrrentFieldEnable |
                    cfg->slotStruct[i].slotConfig.PrevFieldEnable << 1 |
                    cfg->slotStruct[i].slotConfig.NextFieldEnable << 2 |
                    cfg->slotStruct[i].slotConfig.NextNrFieldEnable << 3 |
                    cfg->slotStruct[i].slotConfig.LwrMotionFieldEnable << 4 |
                    cfg->slotStruct[i].slotConfig.PrevMotionFieldEnable << 5 |
                    cfg->slotStruct[i].slotConfig.PpMotionFieldEnable << 6 |
                    cfg->slotStruct[i].slotConfig.CombMotionFieldEnable << 7 ;
    }
    bool success = true;
    for(int i=0;i<LW_VIC_SLOT_COUNT; i++) {
        if((cfg->slotStruct[i].slotConfig.DegammaMode == GAMMA_MODE_REC709) && (!is_yuv_format((PIXELFORMAT) cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat))) {
            fprintf(stderr, "Config-Error: Degamma Mode REC709 Enabled on a non YUV format\n");
            success = false;
            break;
        }

        if (((cfg->slotStruct[i].slotConfig.CadenceDetect) && (((enable[i] & 7) != 7) || (frame[i]) || (!advanced[i])))) {
            fprintf(stderr, "Config-Error: Cadence detection requires field based surfaces with forward and backward reference\n");
            success = false;
            break;
        }

        if (((cfg->slotStruct[i].slotConfig.DeNoise) && (((!frame[i]) && ((enable[i] & 15) != 15)) || ((frame[i]) && ((enable[i] & 7) != 7)) || (!advanced[i])))) {
            fprintf(stderr, "Config-Error: Noise reduction requires field based surfaces with forward and backward reference plus noise reduced output or frame based surfaces with backward reference and noise reduced output\n");
            success = false;
            break;
        }

        if (((!(cfg->slotStruct[i].slotConfig.DeNoise)) && (frame[i]) && (enable[i] > 1))) {
            fprintf(stderr, "Config-Error: Frame based input without noise reduction only requires current frame\n");
            success = false;
            break;
        }

        if (((cfg->slotStruct[i].slotConfig.MotionMap) && (((enable[i] & 0x17) != 0x17) || (frame[i]) || (!advanced[i])))) {
            fprintf(stderr, "Config-Error: Motion map callwlation requires field based surfaces with forward and backward reference plus current motion buffer\n");
            success = false;
        }
        if (((cfg->slotStruct[i].slotConfig.MMapCombine) && ((!cfg->slotStruct[i].slotConfig.MotionMap) || ((enable[i] & 0xb7) != 0xb7) || (frame[i]) || (!advanced[i])))) {
            fprintf(stderr, "Config-Error: Combine motion map requires field based surfaces with current motion buffer callwlation, prev motion buffer and combined motion buffer\n");
            success = false;
        }

    }
    //if((cfg->outputConfig.RegammaMode == GAMMA_MODE_REC709) && (!is_yuv_format((PIXELFORMAT)cfg->outputSurfaceConfig.OutPixelFormat))) {
    //     fprintf(stderr, "Config-Error: Regamma Mode REC709 Enabled on a non YUV format\n");
    //     success = false;
    //}
    // check clear rect
    for(int i=0; i<LW_VIC_SLOT_COUNT;i++)
    {
        // clear rects will be checked below
        /*   if ((cfg->slotStruct[i].slotConfig.reserved0 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved1 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved2 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved3 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved4 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved5 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved6 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved7 != 0)) {
             fprintf(stderr, "Config-Error: Reserved values set to non-zero in surfaceList0Struct\n");
             success = false;
             }*/
    }
    // check ClearRectStruct
    {
        unsigned int all_clear = cfg->slotStruct[0].slotConfig.ClearRectMask;
        for(int i=1; i<LW_VIC_SLOT_COUNT;i++) all_clear |= cfg->slotStruct[i].slotConfig.ClearRectMask;

        for (int i=0; i<4; i++) {
        if (((all_clear >> (i*2)) & 1) != 0) {
            if ((cfg->clearRectStruct[i].ClearRect0Left > cfg->clearRectStruct[i].ClearRect0Right) ||
                (cfg->clearRectStruct[i].ClearRect0Top  > cfg->clearRectStruct[i].ClearRect0Bottom)) {
            fprintf(stderr, "Config-Error: Clear rect enabled but defined empty\n");
            success = false;
            }
        }
        if (((all_clear >> (i*2+1)) & 1) != 0) {
            if ((cfg->clearRectStruct[i].ClearRect1Left > cfg->clearRectStruct[i].ClearRect1Right) ||
                (cfg->clearRectStruct[i].ClearRect1Top  > cfg->clearRectStruct[i].ClearRect1Bottom)) {
            fprintf(stderr, "Config-Error: Clear rect enabled but defined empty\n");
            success = false;
            }
        }
        if ((cfg->clearRectStruct[i].reserved0 != 0) ||
            (cfg->clearRectStruct[i].reserved1 != 0) ||
            (cfg->clearRectStruct[i].reserved2 != 0) ||
            (cfg->clearRectStruct[i].reserved3 != 0) ||
            (cfg->clearRectStruct[i].reserved4 != 0) ||
            (cfg->clearRectStruct[i].reserved5 != 0) ||
            (cfg->clearRectStruct[i].reserved6 != 0) ||
            (cfg->clearRectStruct[i].reserved7 != 0)) {
            fprintf(stderr, "Config-Error: Reserved values set to non-zero in surfaceListClearRectStruct\n");
            success = false;
        }
        }
    }
    // check surfaceListSurfaceStruct
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        if (cfg->slotStruct[i].slotConfig.SlotEnable) {
            if (enable[i] == 0) {
                fprintf(stderr, "Config-Error: Slot enabled in surface list but not in fetch control\n");
                success = false;
            }
            unsigned int min_x=0, min_y=0, mult_y=0;
            switch (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat) {
                case T_A8R8G8B8:
                case T_A8B8G8R8:
                case T_X8B8G8R8:
                case T_B8G8R8X8:
                case T_R8G8B8X8:
                case T_A8Y8U8V8:
                case T_V8U8Y8A8:
                case T_A2B10G10R10:
                case T_B10G10R10A2:
                case T_R10G10B10A2:
                case T_A8:
                case T_A4L4:
                case T_L4A4:
                case T_R8:
                case T_A8L8:
                case T_R8G8:
                case T_G8R8:
                case T_B5G6R5:
                case T_R5G6B5:
                case T_B6G5R5:
                case T_R5G5B6:
                case T_A1B5G5R5:
                case T_A1R5G5B5:
                case T_B5G5R5A1:
                case T_R5G5B5A1:
                case T_A5B5G5R1:
                case T_A5R1G5B5:
                case T_B5G5R1A5:
                case T_R1G5B5A5:
                case T_X1B5G5R5:
                case T_X1R5G5B5:
                case T_B5G5R5X1:
                case T_R5G5B5X1:
                case T_A4B4G4R4:
                case T_A4R4G4B4:
                case T_B4G4R4A4:
                case T_R4G4B4A4:
                case T_Y8___U8___V8_N444:
                case T_R8G8B8A8:
                case T_X8R8G8B8:
                case T_A2R10G10B10:
                case T_B8G8R8A8:
                case T_A8P8:
                case T_P8:
                case T_P8A8:
                case T_L8A8:
                case T_L8:
                case T_A4P4:
                case T_P4A4:
                case T_U8V8:
                case T_V8U8:
                case T_U8:
                case T_V8:
                case T_Y8___V8U8_N444:
                case T_Y8___U8V8_N444:
                    min_x = mult_y = 1;
                    break;
                case T_Y8_U8__Y8_V8:
                case T_Y8_V8__Y8_U8:
                case T_U8_Y8__V8_Y8:
                case T_V8_Y8__U8_Y8:
                    min_x = 2;
                    mult_y = 1;
                    break;
                case T_Y8___V8U8_N420:
                case T_Y8___U8V8_N420:
                case T_Y8___U8___V8_N420:
                    min_x = mult_y = 2;
                    break;
                case T_Y8___V8U8_N422:
                case T_Y8___U8V8_N422:
                case T_Y8___U8___V8_N422:
                    min_x = 2;
                    mult_y = 1;
                    break;
                case T_Y8___V8U8_N422R:
                case T_Y8___U8V8_N422R:
                case T_Y8___U8___V8_N422R:
                    min_x = 1;
                    mult_y = 2;
                    break;
                default:
                    fprintf(stderr, "Config-Error: Invalid pixel format\n");
                    success = false;
            }
            if (!frame[i]) min_y = mult_y << 1;
            else min_y = mult_y;
            unsigned int left    = sr_rnd_dn(cfg->slotStruct[i].slotConfig.SourceRectLeft);
            unsigned int right   = sr_rnd_up(cfg->slotStruct[i].slotConfig.SourceRectRight) + 1;
            unsigned int top     = sr_rnd_dn(cfg->slotStruct[i].slotConfig.SourceRectTop);
            unsigned int bottom  = sr_rnd_up(cfg->slotStruct[i].slotConfig.SourceRectBottom) + 1;
            unsigned int width   = (unsigned int)cfg->slotStruct[i].slotSurfaceConfig.SlotSurfaceWidth+ 1;
            unsigned int height  = (unsigned int)cfg->slotStruct[i].slotSurfaceConfig.SlotSurfaceHeight+ 1;
            unsigned int dleft   = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectLeft;
            unsigned int dright  = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectRight + 1;
            unsigned int dtop    = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectTop;
            unsigned int dbottom = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectBottom + 1;
            unsigned int swidth  = right - left + 1;
            unsigned int sheight = bottom - top + 1;
            unsigned int dwidth  = dright - dleft + 1;
            unsigned int dheight = dbottom - dtop + 1;

            if((cfg->slotStruct[i].slotConfig.Panoramic != 0) && (dwidth < 4)) {
                fprintf(stderr, "Config-Error: Dest rect should be atleast 4 pixels wide for panoramic scaling to work\n");
                success = false;
            }
            if ((swidth < min_x) || (sheight < min_y)) {
                fprintf(stderr, "Config-Error: Source rect has to contain at least one chroma in every field\n");
                success = false;
            }
            if (((width & (min_x-1)) != 0) || ((height & (mult_y-1)) != 0)) {
                fprintf(stderr, "Config-Error: Source surface has to be a multiple of to full pixels \n details: %d %d %d %d \n",width,min_x,height,mult_y);
                success = false;
            }
            if ((left >= right) || (top >= bottom) || (dleft >= dright) || (dtop >= dbottom)) {
                fprintf(stderr, "Config-Error: Source or dest rect empty\n");
                success = false;
            }
            if (cfg->slotStruct[i].slotConfig.Panoramic != 0) {
                if ((dwidth * 7 < swidth) || (dheight * 7 < sheight)) {
                    fprintf(stderr, "Config-Error: Downsclaing ratio of 8:1 with panoramic exceeded\n");
                    success = false;
                }
            } else {
                if ((dwidth * 16 < swidth) || (dheight * 16 < sheight)) {
                    fprintf(stderr, "Config-Error: Downsclaing ratio of 16:1 exceeded\n");
                    success = false;
                }
            }
        } else {
            if (enable[i] != 0) {
                fprintf(stderr, "Config-Error: Slot enabled in fetch control but not in surface list\n");
                success = false;
            }
        }
        //TOTO: error handling for reserved bits slotConfig and slotSurfaceConfig

    }
    // check LumaKeyStruct
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        if ((cfg->slotStruct[i].lumaKeyStruct.LumaKeyEnabled) &&
                (cfg->slotStruct[i].lumaKeyStruct.LumaKeyLower > cfg->slotStruct[i].lumaKeyStruct.LumaKeyUpper)) {
            fprintf(stderr, "Config-Error: Luma key enabled but region empty\n");
            success = false;
        }/*
            if ((cfg->colorColwersionLumaAlphaStruct[i].reserved6 != 0) ||
            (cfg->colorColwersionLumaAlphaStruct[i].reserved7 != 0)) {
            fprintf(stderr, "Config-Error: Reserved values set to non-zero in colorColwersionLumaAlphaStruct\n");
            success = false;
            }*/
    }

#if (LW_CHIP_VIC_HISTOGRAM_CORRECTION_DISABLE==0)
    // check colorColwersionLutStruct
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        // nothing to check for
        if ((cfg->colorColwersionLutStruct[i].reserved0 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved1 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved2 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved3 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved4 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved5 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved6 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved7 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved8 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved9 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved10 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved11 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved12 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved13 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved14 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved15 != 0)) {
            fprintf(stderr, "Config-Error: Reserved values set to non-zero in colorColwersionLutStruct\n");
            success = false;
        }
    }
#endif
    // check colorColwersionMatrixStruct
    /*  for (unsigned int i=0; i<ColorColwersionMatrixStruct_NUM; i++) {
    // nothing to check for
    if ((cfg->slotStruct[i].slotConfig.matrixStruct.reserved0 != 0) ||
    (cfg->colorColwersionMatrixStruct[i].reserved1 != 0) ||
    (cfg->colorColwersionMatrixStruct[i].reserved2 != 0)) {
    fprintf(stderr, "Config-Error: Reserved values set to non-zero in colorColwersionMatrixStruct\n");
    success = false;
    }
    }*/
    // check colorColwersionClampStruct
    for (unsigned int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        if (cfg->slotStruct[i].slotConfig.SoftClampLow > cfg->slotStruct[i].slotConfig.SoftClampHigh) {
        fprintf(stderr, "Config-Error: Clamp low has to be less or equal to clamp high\n");
        success = false;
        }
        /*    if ((cfg->colorColwersionClampStruct[i].reserved0 != 0) ||
        (cfg->colorColwersionClampStruct[i].reserved1 != 0) ||
        (cfg->colorColwersionClampStruct[i].reserved2 != 0) ||
        (cfg->colorColwersionClampStruct[i].reserved3 != 0) ||
        (cfg->colorColwersionClampStruct[i].reserved4 != 0)) {
        fprintf(stderr, "Config-Error: Reserved values set to non-zero in colorColwersionClampStruct\n");
        success = false;
        }*/
    }
#if (LW_CHIP_VIC_CENHANCE_DISABLE==0)
    // check colorColwersionCEnhanceStruct
    for (unsigned int i=0; i<ColorColwersionCEnhanceStruct_NUM; i++) {
        if ((cfg->colorColwersionCEnhanceStruct[i].param0 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].param1 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].reserved0 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].reserved1 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec00 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec01 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec02 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec03 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec10 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec11 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec12 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec13 != 0)) {
            fprintf(stderr, "Config-Error: Reserved or P1 feature set to non-zero value in colorColwersionCEnhanceStruct\n");
        }
    }
#endif

    // check blendingStruct
    {
        switch (cfg->outputConfig.AlphaFillMode) {
            case DXVAHD_ALPHA_FILL_MODE_OPAQUE:
            case DXVAHD_ALPHA_FILL_MODE_BACKGROUND:
            case DXVAHD_ALPHA_FILL_MODE_DESTINATION:
            case DXVAHD_ALPHA_FILL_MODE_COMPOSITED:
                if (cfg->outputConfig.AlphaFillSlot != 0) {
                    fprintf(stderr, "Config-Error: Alpha fill slot not required and should be set to 0\n");
                    success = false;
                }
                break;
            case DXVAHD_ALPHA_FILL_MODE_SOURCE_STREAM:
            case DXVAHD_ALPHA_FILL_MODE_SOURCE_ALPHA:
                if (cfg->outputConfig.AlphaFillSlot > (LW_VIC_SLOT_COUNT-1)) {
                    fprintf(stderr, "Config-Error: Alpha fill slot set to illegal index\n");
                    success = false;
                } else if (enable[cfg->outputConfig.AlphaFillSlot] == 0) {
                    fprintf(stderr, "Config-Error: Alpha fill slot set to disabled slot\n");
                    success = false;
                }
                break;
            default:
                fprintf(stderr, "Config-Error: Illegal alpha fill mode\n");
                success = false;
        }
        /*    if ((cfg->blending0Struct.reserved0 != 0) ||
              (cfg->blending0Struct.reserved1 != 0) ||
              (cfg->blending0Struct.reserved2 != 0) ||
              (cfg->blending0Struct.reserved3 != 0) ||
              (cfg->blending0Struct.reserved4 != 0) ||
              (cfg->blending0Struct.reserved5 != 0) ||
              (cfg->blending0Struct.reserved6 != 0) ||
              (cfg->blending0Struct.reserved7 != 0) ||
              (cfg->blending0Struct.reserved8 != 0) ||
              (cfg->blending0Struct.reserved9 != 0) ||
              (cfg->blending0Struct.reserved10 != 0) ||
              (cfg->blending0Struct.reserved11 != 0)) {
              fprintf(stderr, "Config-Error: Reserved values set to non-zero in blending0Struct\n");
              success = false;
              }*/
    }
    // check blendingSurfaceStruct
    // TODO: check blendingSurfactStruct config values

    // check slotConfig Struct
    for(int i=0; i<LW_VIC_SLOT_COUNT; i++)
    {
        if ((cfg->slotStruct[i].slotConfig.DeinterlaceMode > DXVAHD_DEINTERLACE_MODE_PRIVATE_WEAVE_LUMA_BOB_FIELD_CHROMA) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_BOB) && (!frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_WEAVE_LUMA_BOB_FIELD_CHROMA) && (frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_NEWBOB) && (frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_BOB_FIELD) && (frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_DISI1) && (frame[i]))) {
            fprintf(stderr, "Config-Error: Illegal deinterlace mode\n");
            success = false;
            break;
        }
        /*       if ((cfg->fetchControl0Struct.reserved0 != 0) ||
             (cfg->fetchControl0Struct.reserved1 != 0) ||
             (cfg->fetchControl0Struct.reserved2 != 0) ||
             (cfg->fetchControl0Struct.reserved3 != 0) ||
             (cfg->fetchControl0Struct.reserved4 != 0) ||
             (cfg->fetchControl0Struct.reserved5 != 0) ||
             (cfg->fetchControl0Struct.reserved6 != 0) ||
             (cfg->fetchControl0Struct.reserved7 != 0) ||
             (cfg->fetchControl0Struct.reserved8 != 0) ||
             (cfg->fetchControl0Struct.reserved9 != 0) ||
             (cfg->fetchControl0Struct.reserved10 != 0) ||
             (cfg->fetchControl0Struct.reserved11 != 0) ||
             (cfg->fetchControl0Struct.reserved12 != 0) ||
             (cfg->fetchControl0Struct.reserved13 != 0) ||
             (cfg->fetchControl0Struct.reserved14 != 0) ||
             (cfg->fetchControl0Struct.reserved15 != 0) ||
             (cfg->fetchControl0Struct.reserved16 != 0) ||
             (cfg->fetchControl0Struct.reserved17 != 0) ||
             (cfg->fetchControl0Struct.reserved18 != 0) ||
             (cfg->fetchControl0Struct.reserved19 != 0)) {
             fprintf(stderr, "Config-Error: Reserved values set to non-zero in fetchControl0Struct\n");
             success = false;
             }*/
        if ((cfg->slotStruct[i].slotConfig.NoiseIir > 0x400)) {
            fprintf(stderr, "Config-Error: iir field programmed to a value more than max - 0x400\n");
            success = false;
            break;
        }
    }
    // check fetchControlCoeffStruct
    /*   for (int i=0; i<520; i++) {
         if ((cfg->fetchControlCoeffStruct[i].reserved0 != 0) ||
         (cfg->fetchControlCoeffStruct[i].reserved1 != 0) ||
         (cfg->fetchControlCoeffStruct[i].reserved2 != 0) ||
         (cfg->fetchControlCoeffStruct[i].reserved3 != 0)) {
         fprintf(stderr, "Config-Error: Reserved values set to non-zero in fetchControlCoeffStruct\n");
         success = false;
         }
         }*/
    return success;

    // ===============================================================
    // Everything above this point was copied from drv::check_config()
    // ===============================================================

#undef fprintf
#undef sr_rnd_dn
#undef sr_rnd_up
#undef LW_CHIP_VIC_HISTOGRAM_CORRECTION_DISABLE
#undef LW_CHIP_VIC_CENHANCE_DISABLE
}

//--------------------------------------------------------------------
//! \brief Write methods to the VIC engine to composit one frame
//!
/* virtual */ RC TegraVic4Helper::WriteMethods
(
    const void *pConfigStruct,
    const SrcImages &srcImages,
    const Image &dstImage
)
{
    const Vic4::ConfigStruct &cfg =
        *reinterpret_cast<const Vic4::ConfigStruct*>(pConfigStruct);
    Surface2D cfgSurface;
    Surface2D filterSurface;
    RC rc;

    // Write setup methods
    //
    CHECK_RC(m_pCh->Write(0, LWB0B6_VIDEO_COMPOSITOR_SET_APPLICATION_ID, 0x1));
    CHECK_RC(m_pCh->Write(
                    0, LWB0B6_VIDEO_COMPOSITOR_SET_CONTROL_PARAMS,
                    DRF_NUM(B0B6_VIDEO_COMPOSITOR, _SET_CONTROL_PARAMS,
                            _CONFIG_STRUCT_SIZE,
                            static_cast<UINT32>(GetConfigSize() >> 4))));
    CHECK_RC(m_pCh->Write(0, LWB0B6_VIDEO_COMPOSITOR_SET_CONTEXT_ID, 0));
    CHECK_RC(AllocDataSurface(&cfgSurface, &cfg, GetConfigSize()));
    CHECK_RC(m_pCh->WriteWithSurface(
                    0, LWB0B6_VIDEO_COMPOSITOR_SET_CONFIG_STRUCT_OFFSET,
                    cfgSurface, 0, Vic4::SURFACE_SHIFT));
    MASSERT(sizeof(Vic4::FilterStruct) == Vic4::filterSize);
    CHECK_RC(AllocDataSurface(&filterSurface, Vic4::pFilter, Vic4::filterSize));
    CHECK_RC(m_pCh->WriteWithSurface(
                    0, LWB0B6_VIDEO_COMPOSITOR_SET_FILTER_STRUCT_OFFSET,
                    filterSurface, 0, Vic4::SURFACE_SHIFT));
    CHECK_RC(m_pCh->Write(0, LWB0B6_VIDEO_COMPOSITOR_SET_PALETTE_OFFSET, 0));
    CHECK_RC(dstImage.Write(
                m_pCh,
                LWB0B6_VIDEO_COMPOSITOR_SET_OUTPUT_SURFACE_LUMA_OFFSET,
                LWB0B6_VIDEO_COMPOSITOR_SET_OUTPUT_SURFACE_CHROMA_U_OFFSET,
                LWB0B6_VIDEO_COMPOSITOR_SET_OUTPUT_SURFACE_CHROMA_V_OFFSET));

    // Write the src images
    //
    for (UINT32 slotIdx = 0; slotIdx < srcImages.size(); ++slotIdx)
    {
        for (SrcImagesForSlot::ImageMap::const_iterator
             iter = srcImages[slotIdx].images.begin();
             iter != srcImages[slotIdx].images.end(); ++iter)
        {
            const UINT32 surfaceIdx = iter->first;
            const Image &image = *iter->second;
            CHECK_RC(image.Write(m_pCh,
                                 GetLumaMethod(slotIdx, surfaceIdx),
                                 GetChromaUMethod(slotIdx, surfaceIdx),
                                 GetChromaVMethod(slotIdx, surfaceIdx)));
        }
    }

    // Launch the operation and wait for VIC to finish
    //
    CHECK_RC(m_pCh->Write(
            0, LWB0B6_VIDEO_COMPOSITOR_EXELWTE,
            DRF_DEF(B0B6, _VIDEO_COMPOSITOR_EXELWTE, _AWAKEN, _ENABLE)));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_pCh->WaitIdle(m_pTestConfig->TimeoutMs()));
    return rc;
}

//--------------------------------------------------------------------
//! Return the LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_LUMA_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 TegraVic4Helper::GetLumaMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    static const UINT32 SURFACE_STRIDE =
        LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE1_LUMA_OFFSET(0) -
        LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_LUMA_OFFSET(0);
    return (LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_LUMA_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}

//--------------------------------------------------------------------
//! Return the LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_CHROMA_U_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 TegraVic4Helper::GetChromaUMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    static const UINT32 SURFACE_STRIDE =
        LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE1_CHROMA_U_OFFSET(0) -
        LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_U_OFFSET(0);
    return (LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_U_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}

//--------------------------------------------------------------------
//! Return the LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_CHROMA_V_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 TegraVic4Helper::GetChromaVMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    static const UINT32 SURFACE_STRIDE =
        LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE1_CHROMA_V_OFFSET(0) -
        LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_V_OFFSET(0);
    return (LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_V_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}

//--------------------------------------------------------------------
//! \brief Dump a VIC4 config struct to a human-readable text file
//!
/* virtual */ RC TegraVic4Helper::DumpConfigStruct
(
    FILE *pFile,
    const void *pConfigStruct
) const
{
    const Vic4::ConfigStruct &cfg =
        *reinterpret_cast<const Vic4::ConfigStruct*>(pConfigStruct);
    string fmtString;
    const char *fmt;

    // Dump cfg.pipeConfig
    //
    const Vic4::PipeConfig &pipeConfig = cfg.pipeConfig;
    fmt = "pipeConfig.%s = 0x%llx\n";
    fprintf(pFile, fmt, "DownsampleHoriz", pipeConfig.DownsampleHoriz);
    fprintf(pFile, fmt, "DownsampleVert",  pipeConfig.DownsampleVert);
    fprintf(pFile, "\n");

    // Dump cfg.outputConfig
    //
    const Vic4::OutputConfig &outConfig = cfg.outputConfig;
    fmt = "outputConfig.%s = 0x%llx\n";
    fprintf(pFile, fmt, "AlphaFillMode",    outConfig.AlphaFillMode);
    fprintf(pFile, fmt, "AlphaFillSlot",    outConfig.AlphaFillSlot);
    fprintf(pFile, fmt, "BackgroundAlpha",  outConfig.BackgroundAlpha);
    fprintf(pFile, fmt, "BackgroundR",      outConfig.BackgroundR);
    fprintf(pFile, fmt, "BackgroundG",      outConfig.BackgroundG);
    fprintf(pFile, fmt, "BackgroundB",      outConfig.BackgroundB);
    fprintf(pFile, fmt, "RegammaMode",      outConfig.RegammaMode);
    fprintf(pFile, fmt, "OutputFlipX",      outConfig.OutputFlipX);
    fprintf(pFile, fmt, "OutputFlipY",      outConfig.OutputFlipY);
    fprintf(pFile, fmt, "OutputTranspose",  outConfig.OutputTranspose);
    fprintf(pFile, fmt, "TargetRectLeft",   outConfig.TargetRectLeft);
    fprintf(pFile, fmt, "TargetRectRight",  outConfig.TargetRectRight);
    fprintf(pFile, fmt, "TargetRectTop",    outConfig.TargetRectTop);
    fprintf(pFile, fmt, "TargetRectBottom", outConfig.TargetRectBottom);
    fprintf(pFile, "\n");

    // Dump cfg.outputSurfaceConfig
    //
    const Vic4::OutputSurfaceConfig &outSurf = cfg.outputSurfaceConfig;
    fmt = "outputSurfaceConfig.%s = 0x%llx\n";
    fprintf(pFile, fmt, "OutPixelFormat",    outSurf.OutPixelFormat);
    fprintf(pFile, fmt, "OutChromaLocHoriz", outSurf.OutChromaLocHoriz);
    fprintf(pFile, fmt, "OutChromaLocVert",  outSurf.OutChromaLocVert);
    fprintf(pFile, fmt, "OutBlkKind",        outSurf.OutBlkKind);
    fprintf(pFile, fmt, "OutBlkHeight",      outSurf.OutBlkHeight);
    fprintf(pFile, fmt, "OutSurfaceWidth",   outSurf.OutSurfaceWidth);
    fprintf(pFile, fmt, "OutSurfaceHeight",  outSurf.OutSurfaceHeight);
    fprintf(pFile, fmt, "OutLumaWidth",      outSurf.OutLumaWidth);
    fprintf(pFile, fmt, "OutLumaHeight",     outSurf.OutLumaHeight);
    fprintf(pFile, fmt, "OutChromaWidth",    outSurf.OutChromaWidth);
    fprintf(pFile, fmt, "OutChromaHeight",   outSurf.OutChromaHeight);
    fprintf(pFile, "\n");

    // Dump cfg.outColorMatrixStruct
    //
    DumpMatrixStruct(pFile, "outColorMatrixStruct", cfg.outColorMatrixStruct);

    // Dump cfg.clearRectStruct
    //
    for (UINT32 ii = 0; ii < NUMELEMS(cfg.clearRectStruct); ++ii)
    {
        const Vic4::ClearRectStruct &clearRect = cfg.clearRectStruct[ii];
        fmtString = Utility::StrPrintf(
                "clearRectStruct[%d].%%s = 0x%%llx\n", ii);
        fmt = fmtString.c_str();
        fprintf(pFile, fmt, "ClearRect0Left",   clearRect.ClearRect0Left);
        fprintf(pFile, fmt, "ClearRect0Right",  clearRect.ClearRect0Right);
        fprintf(pFile, fmt, "ClearRect0Top",    clearRect.ClearRect0Top);
        fprintf(pFile, fmt, "ClearRect0Bottom", clearRect.ClearRect0Bottom);
        fprintf(pFile, fmt, "ClearRect1Left",   clearRect.ClearRect1Left);
        fprintf(pFile, fmt, "ClearRect1Right",  clearRect.ClearRect1Right);
        fprintf(pFile, fmt, "ClearRect1Top",    clearRect.ClearRect1Top);
        fprintf(pFile, fmt, "ClearRect1Bottom", clearRect.ClearRect1Bottom);
        fprintf(pFile, "\n");
    }

    // Dump cfg.slotStruct
    //
    for (UINT32 slotIdx = 0; slotIdx < GetMaxSlots(); ++slotIdx)
    {
        DumpSlotStruct(pFile,
                       Utility::StrPrintf("slotStruct[%d]", slotIdx),
                       cfg.slotStruct[slotIdx]);
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Used by DumpConfigStruct() to dump one MatrixStruct value
//!
//! \param pFile The file that DumpConfigStruct() is writing
//! \param prefix A string with the name of the MatrixStruct variable
//!     we're dumping, such as "slotStruct[3].colorMatrixStruct"
//! \param matrix The MatrixStruct value we're dumping
//!
void TegraVic4Helper::DumpMatrixStruct
(
    FILE *pFile,
    const string &prefix,
    const Vic4::MatrixStruct &matrix
)
{
    const string fmtString = prefix + ".%s = 0x%llx\n";
    const char *fmt = fmtString.c_str();

    fprintf(pFile, fmt, "matrix_coeff00", matrix.matrix_coeff00);
    fprintf(pFile, fmt, "matrix_coeff10", matrix.matrix_coeff10);
    fprintf(pFile, fmt, "matrix_coeff20", matrix.matrix_coeff20);
    fprintf(pFile, fmt, "matrix_r_shift", matrix.matrix_r_shift);
    fprintf(pFile, fmt, "matrix_coeff01", matrix.matrix_coeff01);
    fprintf(pFile, fmt, "matrix_coeff11", matrix.matrix_coeff11);
    fprintf(pFile, fmt, "matrix_coeff21", matrix.matrix_coeff21);
    fprintf(pFile, fmt, "matrix_enable",  matrix.matrix_enable);
    fprintf(pFile, fmt, "matrix_coeff02", matrix.matrix_coeff02);
    fprintf(pFile, fmt, "matrix_coeff12", matrix.matrix_coeff12);
    fprintf(pFile, fmt, "matrix_coeff22", matrix.matrix_coeff22);
    fprintf(pFile, fmt, "matrix_coeff03", matrix.matrix_coeff03);
    fprintf(pFile, fmt, "matrix_coeff13", matrix.matrix_coeff13);
    fprintf(pFile, fmt, "matrix_coeff23", matrix.matrix_coeff23);
    fprintf(pFile, "\n");
}

//--------------------------------------------------------------------
//! \brief Used by DumpConfigStruct() to dump one SlotStruct value
//!
//! \param pFile The file that DumpConfigStruct() is writing
//! \param prefix A string with the name of the SlotStruct variable
//!     we're dumping, such as "slotStruct[3]"
//! \param slotStruct The SlotStruct value we're dumping
//!
void TegraVic4Helper::DumpSlotStruct
(
    FILE *pFile,
    const string &prefix,
    const Vic4::SlotStruct &slotStruct
)
{
    string fmtString;
    const char *fmt;

    // Dump slotStruct.slotConfig
    //
    const Vic4::SlotConfig &slotCfg = slotStruct.slotConfig;
    fmtString = prefix + ".slotConfig.%s = 0x%llx\n";
    fmt = fmtString.c_str();
    fprintf(pFile, fmt, "SlotEnable",            slotCfg.SlotEnable);
    fprintf(pFile, fmt, "DeNoise",               slotCfg.DeNoise);
    fprintf(pFile, fmt, "AdvancedDenoise",       slotCfg.AdvancedDenoise);
    fprintf(pFile, fmt, "CadenceDetect",         slotCfg.CadenceDetect);
    fprintf(pFile, fmt, "MotionMap",             slotCfg.MotionMap);
    fprintf(pFile, fmt, "MMapCombine",           slotCfg.MMapCombine);
    fprintf(pFile, fmt, "IsEven",                slotCfg.IsEven);
    fprintf(pFile, fmt, "ChromaEven",            slotCfg.ChromaEven);
    fprintf(pFile, fmt, "LwrrentFieldEnable",    slotCfg.LwrrentFieldEnable);
    fprintf(pFile, fmt, "PrevFieldEnable",       slotCfg.PrevFieldEnable);
    fprintf(pFile, fmt, "NextFieldEnable",       slotCfg.NextFieldEnable);
    fprintf(pFile, fmt, "NextNrFieldEnable",     slotCfg.NextNrFieldEnable);
    fprintf(pFile, fmt, "LwrMotionFieldEnable",  slotCfg.LwrMotionFieldEnable);
    fprintf(pFile, fmt, "PrevMotionFieldEnable", slotCfg.PrevMotionFieldEnable);
    fprintf(pFile, fmt, "PpMotionFieldEnable",   slotCfg.PpMotionFieldEnable);
    fprintf(pFile, fmt, "CombMotionFieldEnable", slotCfg.CombMotionFieldEnable);
    fprintf(pFile, fmt, "FrameFormat",           slotCfg.FrameFormat);
    fprintf(pFile, fmt, "FilterLengthY",         slotCfg.FilterLengthY);
    fprintf(pFile, fmt, "FilterLengthX",         slotCfg.FilterLengthX);
    fprintf(pFile, fmt, "Panoramic",             slotCfg.Panoramic);
    fprintf(pFile, fmt, "DetailFltClamp",        slotCfg.DetailFltClamp);
    fprintf(pFile, fmt, "FilterNoise",           slotCfg.FilterNoise);
    fprintf(pFile, fmt, "FilterDetail",          slotCfg.FilterDetail);
    fprintf(pFile, fmt, "ChromaNoise",           slotCfg.ChromaNoise);
    fprintf(pFile, fmt, "ChromaDetail",          slotCfg.ChromaDetail);
    fprintf(pFile, fmt, "DeinterlaceMode",       slotCfg.DeinterlaceMode);
    fprintf(pFile, fmt, "MotionAclwmWeight",     slotCfg.MotionAclwmWeight);
    fprintf(pFile, fmt, "NoiseIir",              slotCfg.NoiseIir);
    fprintf(pFile, fmt, "LightLevel",            slotCfg.LightLevel);
    fprintf(pFile, fmt, "SoftClampLow",          slotCfg.SoftClampLow);
    fprintf(pFile, fmt, "SoftClampHigh",         slotCfg.SoftClampHigh);
    fprintf(pFile, fmt, "PlanarAlpha",           slotCfg.PlanarAlpha);
    fprintf(pFile, fmt, "ConstantAlpha",         slotCfg.ConstantAlpha);
    fprintf(pFile, fmt, "StereoInterleave",      slotCfg.StereoInterleave);
    fprintf(pFile, fmt, "ClipEnabled",           slotCfg.ClipEnabled);
    fprintf(pFile, fmt, "ClearRectMask",         slotCfg.ClearRectMask);
    fprintf(pFile, fmt, "DegammaMode",           slotCfg.DegammaMode);
    fprintf(pFile, fmt, "DecompressEnable",      slotCfg.DecompressEnable);
    fprintf(pFile, fmt, "DecompressCtbCount",    slotCfg.DecompressCtbCount);
    fprintf(pFile, fmt, "DecompressZbcColor",    slotCfg.DecompressZbcColor);
    fprintf(pFile, fmt, "SourceRectLeft",        slotCfg.SourceRectLeft);
    fprintf(pFile, fmt, "SourceRectRight",       slotCfg.SourceRectRight);
    fprintf(pFile, fmt, "SourceRectTop",         slotCfg.SourceRectTop);
    fprintf(pFile, fmt, "SourceRectBottom",      slotCfg.SourceRectBottom);
    fprintf(pFile, fmt, "DestRectLeft",          slotCfg.DestRectLeft);
    fprintf(pFile, fmt, "DestRectRight",         slotCfg.DestRectRight);
    fprintf(pFile, fmt, "DestRectTop",           slotCfg.DestRectTop);
    fprintf(pFile, fmt, "DestRectBottom",        slotCfg.DestRectBottom);
    fprintf(pFile, "\n");

    // Dump slotStruct.slotSurfaceConfig
    //
    const Vic4::SlotSurfaceConfig &slotSurf = slotStruct.slotSurfaceConfig;
    fmtString = prefix + ".slotSurfaceConfig.%s = 0x%llx\n";
    fmt = fmtString.c_str();
    fprintf(pFile, fmt, "SlotPixelFormat",    slotSurf.SlotPixelFormat);
    fprintf(pFile, fmt, "SlotChromaLocHoriz", slotSurf.SlotChromaLocHoriz);
    fprintf(pFile, fmt, "SlotChromaLocVert",  slotSurf.SlotChromaLocVert);
    fprintf(pFile, fmt, "SlotBlkKind",        slotSurf.SlotBlkKind);
    fprintf(pFile, fmt, "SlotBlkHeight",      slotSurf.SlotBlkHeight);
    fprintf(pFile, fmt, "SlotCacheWidth",     slotSurf.SlotCacheWidth);
    fprintf(pFile, fmt, "SlotSurfaceWidth",   slotSurf.SlotSurfaceWidth);
    fprintf(pFile, fmt, "SlotSurfaceHeight",  slotSurf.SlotSurfaceHeight);
    fprintf(pFile, fmt, "SlotLumaWidth",      slotSurf.SlotLumaWidth);
    fprintf(pFile, fmt, "SlotLumaHeight",     slotSurf.SlotLumaHeight);
    fprintf(pFile, fmt, "SlotChromaWidth",    slotSurf.SlotChromaWidth);
    fprintf(pFile, fmt, "SlotChromaHeight",   slotSurf.SlotChromaHeight);
    fprintf(pFile, "\n");

    // Dump slotStruct.lumaKeyStruct
    //
    const Vic4::LumaKeyStruct &lumaKey = slotStruct.lumaKeyStruct;
    fmtString = prefix + ".lumaKeyStruct.%s = 0x%llx\n";
    fmt = fmtString.c_str();
    fprintf(pFile, fmt, "luma_coeff0",    lumaKey.luma_coeff0);
    fprintf(pFile, fmt, "luma_coeff1",    lumaKey.luma_coeff1);
    fprintf(pFile, fmt, "luma_coeff2",    lumaKey.luma_coeff2);
    fprintf(pFile, fmt, "luma_r_shift",   lumaKey.luma_r_shift);
    fprintf(pFile, fmt, "luma_coeff3",    lumaKey.luma_coeff3);
    fprintf(pFile, fmt, "LumaKeyLower",   lumaKey.LumaKeyLower);
    fprintf(pFile, fmt, "LumaKeyUpper",   lumaKey.LumaKeyUpper);
    fprintf(pFile, fmt, "LumaKeyEnabled", lumaKey.LumaKeyEnabled);
    fprintf(pFile, "\n");

    // Dump cfg.slotStruct.colorMatrixStruct
    //
    DumpMatrixStruct(pFile, prefix + ".colorMatrixStruct",
                     slotStruct.colorMatrixStruct);

    // Dump cfg.slotStruct.gamutMatrixStruct
    //
    DumpMatrixStruct(pFile, prefix + ".gamutMatrixStruct",
                     slotStruct.gamutMatrixStruct);

    // Dump cfg.slotStruct.blendingSlotStruct
    //
    const Vic4::BlendingSlotStruct &blending = slotStruct.blendingSlotStruct;
    fmtString = prefix + ".blendingSlotStruct.%s = 0x%llx\n";
    fmt = fmtString.c_str();
    fprintf(pFile, fmt, "AlphaK1",             blending.AlphaK1);
    fprintf(pFile, fmt, "AlphaK2",             blending.AlphaK2);
    fprintf(pFile, fmt, "SrcFactCMatchSelect", blending.SrcFactCMatchSelect);
    fprintf(pFile, fmt, "DstFactCMatchSelect", blending.DstFactCMatchSelect);
    fprintf(pFile, fmt, "SrcFactAMatchSelect", blending.SrcFactAMatchSelect);
    fprintf(pFile, fmt, "DstFactAMatchSelect", blending.DstFactAMatchSelect);
    fprintf(pFile, fmt, "OverrideR",           blending.OverrideR);
    fprintf(pFile, fmt, "OverrideG",           blending.OverrideG);
    fprintf(pFile, fmt, "OverrideB",           blending.OverrideB);
    fprintf(pFile, fmt, "OverrideA",           blending.OverrideA);
    fprintf(pFile, fmt, "UseOverrideR",        blending.UseOverrideR);
    fprintf(pFile, fmt, "UseOverrideG",        blending.UseOverrideG);
    fprintf(pFile, fmt, "UseOverrideB",        blending.UseOverrideB);
    fprintf(pFile, fmt, "UseOverrideA",        blending.UseOverrideA);
    fprintf(pFile, fmt, "MaskR",               blending.MaskR);
    fprintf(pFile, fmt, "MaskG",               blending.MaskG);
    fprintf(pFile, fmt, "MaskB",               blending.MaskB);
    fprintf(pFile, fmt, "MaskA",               blending.MaskA);
}

//--------------------------------------------------------------------
// Function used by CheckConfigStruct(), copied from
// //hw/gpu_ip/unit/vic/4.1/cmod/driver/Image.cpp
namespace
{
    namespace TmpNamespace
    {
        bool is_yuv_format(Vic41::PIXELFORMAT f)
        {
            using namespace Vic41;
            switch (f)
            {
            // 1BPP, 2BPP & Index surfaces
            case T_L8:
            case T_U8:
            case T_V8:
            case T_U8V8:
            case T_V8U8:
            case T_A4L4:
            case T_L4A4:
            case T_A8L8:
            case T_L8A8:
            case T_L10:
            case T_U10:
            case T_V10:
            case T_U10V10:
            case T_V10U10:
                // Packed YUV with Alpha formats
            case T_A8Y8U8V8:
            case T_V8U8Y8A8:
                // Packed subsampled YUV 422
            case T_Y8_U8__Y8_V8:
            case T_Y8_V8__Y8_U8:
            case T_U8_Y8__V8_Y8:
            case T_V8_Y8__U8_Y8:
                // Semi-planar YUV
            case T_Y8___U8V8_N444:
            case T_Y8___V8U8_N444:
            case T_Y8___U8V8_N422:
            case T_Y8___V8U8_N422:
            case T_Y8___U8V8_N422R:
            case T_Y8___V8U8_N422R:
            case T_Y8___U8V8_N420:
            case T_Y8___V8U8_N420:
            case T_Y10___U10V10_N444:
            case T_Y10___V10U10_N444:
            case T_Y10___U10V10_N422:
            case T_Y10___V10U10_N422:
            case T_Y10___U10V10_N422R:
            case T_Y10___V10U10_N422R:
            case T_Y10___U10V10_N420:
            case T_Y10___V10U10_N420:
                // Planar YUV
            case T_Y8___U8___V8_N444:
            case T_Y8___U8___V8_N422:
            case T_Y8___U8___V8_N422R:
            case T_Y8___U8___V8_N420:
            case T_Y10___U10___V10_N444:
            case T_Y10___U10___V10_N422:
            case T_Y10___U10___V10_N422R:
            case T_Y10___U10___V10_N420:
                return true;
            default:
                return false;
            }
        }
    }
}

//--------------------------------------------------------------------
//! \brief Verify that the VIC 4.1 config struct contains valid values
//!
//! Copied from drv::check_config() in
//! //hw/gpu_ip/unit/vic/4.1/cmod/driver/drv.cpp
//!
/* virtual */ bool TegraVic41Helper::CheckConfigStruct
(
    const void *pConfigStruct
) const
{
    // Assorted hacks so that we can import drv::check_config() to
    // mods with minimal rewrite.
    //
    using namespace Vic41;
    namespace Image = TmpNamespace;
    const ConfigStruct *cfg = static_cast<const ConfigStruct*>(pConfigStruct);
    const UINT08 LW_VIC_SLOT_COUNT = Vic41::MAX_SLOTS;
#define printf PrintfForCheckConfig
#define sr_rnd_dn(fxp) static_cast<int>(FxpToIntFloor(fxp))
#define sr_rnd_up(fxp) static_cast<int>(FxpToIntCeil(fxp))
#define LW_CHIP_VIC_HISTOGRAM_CORRECTION_DISABLE 1
#define LW_CHIP_VIC_CENHANCE_DISABLE 1
#define LW_VIC_SUPPORT_ALPHA_FILL_MODE_SOURCE_STREAM 1
#define LW_VIC_VDM_SUPPORT 0

    // ===============================================================
    // Everything below this point was copied from drv::check_config()
    // ===============================================================

    // we need this a couple of times.
    bool frame[LW_VIC_SLOT_COUNT];
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        frame[i] = ((cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_PROGRESSIVE) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_SUBPIC_PROGRESSIVE) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_TOP_FIELD_FIRST) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_BOTTOM_FIELD_FIRST));
    }
    bool advanced[LW_VIC_SLOT_COUNT];
    // is this too restrictive?  - SIVA: How to modify this
    for(int i=0;i<LW_VIC_SLOT_COUNT;i++) {
        advanced[i] = (
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8_U8__Y8_V8) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_U8_Y8__V8_Y8) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8___V8U8_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8___U8___V8_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y10___V10U10_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y10___U10___V10_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y12___V12U12_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y12___U12___V12_N420));
    }

    unsigned int enable[LW_VIC_SLOT_COUNT];
    for(int i=0; i<LW_VIC_SLOT_COUNT; i++)    {
        enable[i] = cfg->slotStruct[i].slotConfig.LwrrentFieldEnable |
            cfg->slotStruct[i].slotConfig.PrevFieldEnable << 1 |
            cfg->slotStruct[i].slotConfig.NextFieldEnable << 2 |
            cfg->slotStruct[i].slotConfig.NextNrFieldEnable << 3 |
            cfg->slotStruct[i].slotConfig.LwrMotionFieldEnable << 4 |
            cfg->slotStruct[i].slotConfig.PrevMotionFieldEnable << 5 |
            cfg->slotStruct[i].slotConfig.PpMotionFieldEnable << 6 |
            cfg->slotStruct[i].slotConfig.CombMotionFieldEnable << 7 ;
    }
    bool success = true;
    for(int i=0;i<LW_VIC_SLOT_COUNT; i++) {
#if (LW_VIC_VDM_SUPPORT == 0)
        if((cfg->slotStruct[i].slotConfig.DegammaMode == GAMMA_MODE_REC709) && (!Image::is_yuv_format((PIXELFORMAT) cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat))) {
            printf("Config-Error: Degamma Mode REC709 Enabled on a non YUV format\n");
            success = false;
            break;
        }
#endif

        if (((cfg->slotStruct[i].slotConfig.CadenceDetect) && (((enable[i] & 7) != 7) || (frame[i]) || (!advanced[i])))) {
            printf("Config-Error: Cadence detection requires field based surfaces with forward and backward reference\n");
            success = false;
            break;
        }

        if (((cfg->slotStruct[i].slotConfig.DeNoise) && (((!frame[i]) && ((enable[i] & 15) != 15)) || ((frame[i]) && ((enable[i] & 7) != 7)) || (!advanced[i])))) {
            printf("Config-Error: Noise reduction requires field based surfaces with forward and backward reference plus noise reduced output or frame based surfaces with backward reference and noise reduced output\n");
            success = false;
            break;
        }

        if (((!(cfg->slotStruct[i].slotConfig.DeNoise)) && (frame[i]) && (enable[i] > 1))) {
            printf("Config-Error: Frame based input without noise reduction only requires current frame\n");
            success = false;
            break;
        }

        if (((cfg->slotStruct[i].slotConfig.MotionMap) && (((enable[i] & 0x17) != 0x17) || (frame[i]) || (!advanced[i])))) {
            printf("Config-Error: Motion map callwlation requires field based surfaces with forward and backward reference plus current motion buffer\n");
            success = false;
        }
        if (((cfg->slotStruct[i].slotConfig.MMapCombine) && ((!cfg->slotStruct[i].slotConfig.MotionMap) || ((enable[i] & 0xb7) != 0xb7) || (frame[i]) || (!advanced[i])))) {
            printf("Config-Error: Combine motion map requires field based surfaces with current motion buffer callwlation, prev motion buffer and combined motion buffer\n");
            success = false;
        }

    }

    // check clear rect
    for(int i=0; i<LW_VIC_SLOT_COUNT;i++)
    {
        // clear rects will be checked below
        /*   if ((cfg->slotStruct[i].slotConfig.reserved0 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved1 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved2 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved3 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved4 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved5 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved6 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved7 != 0)) {
             printf("Config-Error: Reserved values set to non-zero in surfaceList0Struct\n");
             success = false;
             }*/
    }
    // check ClearRectStruct
    {
        unsigned int all_clear = cfg->slotStruct[0].slotConfig.ClearRectMask;
        for(int i=1; i<LW_VIC_SLOT_COUNT;i++) all_clear |= cfg->slotStruct[i].slotConfig.ClearRectMask;

        for (int i=0; i<4; i++) {
            if (((all_clear >> (i*2)) & 1) != 0) {
                if ((cfg->clearRectStruct[i].ClearRect0Left > cfg->clearRectStruct[i].ClearRect0Right) ||
                        (cfg->clearRectStruct[i].ClearRect0Top  > cfg->clearRectStruct[i].ClearRect0Bottom)) {
                    printf("Config-Error: Clear rect enabled but defined empty\n");
                    success = false;
                }
            }
            if (((all_clear >> (i*2+1)) & 1) != 0) {
                if ((cfg->clearRectStruct[i].ClearRect1Left > cfg->clearRectStruct[i].ClearRect1Right) ||
                        (cfg->clearRectStruct[i].ClearRect1Top  > cfg->clearRectStruct[i].ClearRect1Bottom)) {
                    printf("Config-Error: Clear rect enabled but defined empty\n");
                    success = false;
                }
            }
            if ((cfg->clearRectStruct[i].reserved0 != 0) ||
                    (cfg->clearRectStruct[i].reserved1 != 0) ||
                    (cfg->clearRectStruct[i].reserved2 != 0) ||
                    (cfg->clearRectStruct[i].reserved3 != 0) ||
                    (cfg->clearRectStruct[i].reserved4 != 0) ||
                    (cfg->clearRectStruct[i].reserved5 != 0) ||
                    (cfg->clearRectStruct[i].reserved6 != 0) ||
                    (cfg->clearRectStruct[i].reserved7 != 0)) {
                printf("Config-Error: Reserved values set to non-zero in surfaceListClearRectStruct\n");
                success = false;
            }
        }
    }
    // check surfaceListSurfaceStruct
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        if (cfg->slotStruct[i].slotConfig.SlotEnable) {
            if (enable[i] == 0) {
                printf("Config-Error: Slot enabled in surface list but not in fetch control\n");
                success = false;
            }
            unsigned int min_x=0, min_y=0, mult_y=0;
            switch (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat) {
                case T_A8R8G8B8:
                case T_A8B8G8R8:
                case T_X8B8G8R8:
                case T_B8G8R8X8:
                case T_R8G8B8X8:
                case T_A8Y8U8V8:
                case T_V8U8Y8A8:
                case T_A2B10G10R10:
                case T_B10G10R10A2:
                case T_R10G10B10A2:
                case T_A8:
                case T_A4L4:
                case T_L4A4:
                case T_R8:
                case T_A8L8:
                case T_R8G8:
                case T_G8R8:
                case T_B5G6R5:
                case T_R5G6B5:
                case T_B6G5R5:
                case T_R5G5B6:
                case T_A1B5G5R5:
                case T_A1R5G5B5:
                case T_B5G5R5A1:
                case T_R5G5B5A1:
                case T_A5B5G5R1:
                case T_A5R1G5B5:
                case T_B5G5R1A5:
                case T_R1G5B5A5:
                case T_X1B5G5R5:
                case T_X1R5G5B5:
                case T_B5G5R5X1:
                case T_R5G5B5X1:
                case T_A4B4G4R4:
                case T_A4R4G4B4:
                case T_B4G4R4A4:
                case T_R4G4B4A4:
                case T_Y8___U8___V8_N444:
                case T_R8G8B8A8:
                case T_X8R8G8B8:
                case T_A2R10G10B10:
                case T_B8G8R8A8:
                case T_A8P8:
                case T_P8:
                case T_P8A8:
                case T_L8A8:
                case T_L8:
                case T_A4P4:
                case T_P4A4:
                case T_U8V8:
                case T_V8U8:
                case T_U8:
                case T_V8:
                case T_Y8___V8U8_N444:
                case T_Y8___U8V8_N444:
                case T_Y10___U10___V10_N444:
                case T_L10:
                case T_U10V10:
                case T_V10U10:
                case T_U10:
                case T_V10:
                case T_Y10___V10U10_N444:
                case T_Y10___U10V10_N444:
                case T_Y12___U12___V12_N444:
                case T_L12:
                //case T_L16:
                case T_U12V12:
                case T_V12U12:
                case T_U12:
                case T_V12:
                case T_Y12___V12U12_N444:
                case T_Y12___U12V12_N444:
                //case T_A16B16G16R16:
                //case T_A16Y16U16V16:
                    min_x = mult_y = 1;
                    break;
                case T_Y8_U8__Y8_V8:
                case T_Y8_V8__Y8_U8:
                case T_U8_Y8__V8_Y8:
                case T_V8_Y8__U8_Y8:
                    min_x = 2;
                    mult_y = 1;
                    break;
                case T_Y8___V8U8_N420:
                case T_Y8___U8V8_N420:
                case T_Y8___U8___V8_N420:
                case T_Y10___V10U10_N420:
                case T_Y10___U10V10_N420:
                case T_Y10___U10___V10_N420:
                case T_Y12___V12U12_N420:
                case T_Y12___U12V12_N420:
                case T_Y12___U12___V12_N420:
                    min_x = mult_y = 2;
                    break;
                case T_Y8___V8U8_N422:
                case T_Y8___U8V8_N422:
                case T_Y8___U8___V8_N422:
                case T_Y10___V10U10_N422:
                case T_Y10___U10V10_N422:
                case T_Y10___U10___V10_N422:
                case T_Y12___V12U12_N422:
                case T_Y12___U12V12_N422:
                case T_Y12___U12___V12_N422:
                    min_x = 2;
                    mult_y = 1;
                    break;
                case T_Y8___V8U8_N422R:
                case T_Y8___U8V8_N422R:
                case T_Y8___U8___V8_N422R:
                case T_Y10___V10U10_N422R:
                case T_Y10___U10V10_N422R:
                case T_Y10___U10___V10_N422R:
                case T_Y12___V12U12_N422R:
                case T_Y12___U12V12_N422R:
                case T_Y12___U12___V12_N422R:
                    min_x = 1;
                    mult_y = 2;
                    break;
                default:
                    printf("Config-Error: Invalid pixel format\n");
                    success = false;
            }
            if (!frame[i]) min_y = mult_y << 1;
            else min_y = mult_y;
            unsigned int left    = sr_rnd_dn(cfg->slotStruct[i].slotConfig.SourceRectLeft);
            unsigned int right   = sr_rnd_up(cfg->slotStruct[i].slotConfig.SourceRectRight) + 1;
            unsigned int top     = sr_rnd_dn(cfg->slotStruct[i].slotConfig.SourceRectTop);
            unsigned int bottom  = sr_rnd_up(cfg->slotStruct[i].slotConfig.SourceRectBottom) + 1;
            unsigned int width   = (unsigned int)cfg->slotStruct[i].slotSurfaceConfig.SlotSurfaceWidth+ 1;
            unsigned int height  = (unsigned int)cfg->slotStruct[i].slotSurfaceConfig.SlotSurfaceHeight+ 1;
            unsigned int dleft   = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectLeft;
            unsigned int dright  = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectRight + 1;
            unsigned int dtop    = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectTop;
            unsigned int dbottom = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectBottom + 1;
            unsigned int swidth  = right - left + 1;
            unsigned int sheight = bottom - top + 1;
            unsigned int dwidth  = dright - dleft + 1;
            unsigned int dheight = dbottom - dtop + 1;

            if((cfg->slotStruct[i].slotConfig.Panoramic != 0) && (dwidth < 4)) {
                printf("Config-Error: Dest rect should be atleast 4 pixels wide for panoramic scaling to work\n");
                success = false;
            }
            if ((swidth < min_x) || (sheight < min_y)) {
                printf("Config-Error: Source rect has to contain at least one chroma in every field\n");
                success = false;
            }
            if (((width & (min_x-1)) != 0) || ((height & (mult_y-1)) != 0)) {
                printf("Config-Error: Source surface has to be a multiple of to full pixels \n details: %d %d %d %d \n",width,min_x,height,mult_y);
                success = false;
            }
            if ((left >= right) || (top >= bottom) || (dleft >= dright) || (dtop >= dbottom)) {
                printf("Config-Error: Source or dest rect empty\n");
                success = false;
            }
            if (cfg->slotStruct[i].slotConfig.Panoramic != 0) {
                if ((dwidth * 7 < swidth) || (dheight * 7 < sheight)) {
                    printf("Config-Error: Downscaling ratio of 7:1 with panoramic exceeded\n");
                    success = false;
                }
            } else {
                if ((dwidth * 16 < swidth) || (dheight * 16 < sheight)) {
                    printf("Config-Error: Downscaling ratio of 16:1 exceeded\n");
                    success = false;
                }
            }
        } else {
            if (enable[i] != 0) {
                printf("Config-Error: Slot enabled in fetch control but not in surface list\n");
                success = false;
            }
        }
        //TOTO: error handling for reserved bits slotConfig and slotSurfaceConfig

    }
    // check LumaKeyStruct
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        if ((cfg->slotStruct[i].lumaKeyStruct.LumaKeyEnabled) &&
                (cfg->slotStruct[i].lumaKeyStruct.LumaKeyLower > cfg->slotStruct[i].lumaKeyStruct.LumaKeyUpper)) {
            printf("Config-Error: Luma key enabled but region empty\n");
            success = false;
        }/*
            if ((cfg->colorColwersionLumaAlphaStruct[i].reserved6 != 0) ||
            (cfg->colorColwersionLumaAlphaStruct[i].reserved7 != 0)) {
            printf("Config-Error: Reserved values set to non-zero in colorColwersionLumaAlphaStruct\n");
            success = false;
            }*/
    }

#if (LW_CHIP_VIC_HISTOGRAM_CORRECTION_DISABLE==0)
    // check colorColwersionLutStruct
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        // nothing to check for
        if ((cfg->colorColwersionLutStruct[i].reserved0 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved1 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved2 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved3 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved4 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved5 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved6 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved7 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved8 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved9 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved10 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved11 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved12 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved13 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved14 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved15 != 0)) {
            printf("Config-Error: Reserved values set to non-zero in colorColwersionLutStruct\n");
            success = false;
        }
    }
#endif
    // check colorColwersionMatrixStruct
    /*  for (unsigned int i=0; i<ColorColwersionMatrixStruct_NUM; i++) {
    // nothing to check for
    if ((cfg->slotStruct[i].slotConfig.matrixStruct.reserved0 != 0) ||
    (cfg->colorColwersionMatrixStruct[i].reserved1 != 0) ||
    (cfg->colorColwersionMatrixStruct[i].reserved2 != 0)) {
    printf("Config-Error: Reserved values set to non-zero in colorColwersionMatrixStruct\n");
    success = false;
    }
    }*/
    // check colorColwersionClampStruct
    for (unsigned int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        if (cfg->slotStruct[i].slotConfig.SoftClampLow > cfg->slotStruct[i].slotConfig.SoftClampHigh) {
            printf("Config-Error: Clamp low has to be less or equal to clamp high\n");
            success = false;
        }
        /*    if ((cfg->colorColwersionClampStruct[i].reserved0 != 0) ||
              (cfg->colorColwersionClampStruct[i].reserved1 != 0) ||
              (cfg->colorColwersionClampStruct[i].reserved2 != 0) ||
              (cfg->colorColwersionClampStruct[i].reserved3 != 0) ||
              (cfg->colorColwersionClampStruct[i].reserved4 != 0)) {
              printf("Config-Error: Reserved values set to non-zero in colorColwersionClampStruct\n");
              success = false;
              }*/
    }
#if (LW_CHIP_VIC_CENHANCE_DISABLE==0)
    // check colorColwersionCEnhanceStruct
    for (unsigned int i=0; i<ColorColwersionCEnhanceStruct_NUM; i++) {
        if ((cfg->colorColwersionCEnhanceStruct[i].param0 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].param1 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].reserved0 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].reserved1 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec00 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec01 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec02 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec03 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec10 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec11 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec12 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec13 != 0)) {
            printf("Config-Error: Reserved or P1 feature set to non-zero value in colorColwersionCEnhanceStruct\n");
        }
    }
#endif

    // check blendingStruct
    {
        switch (cfg->outputConfig.AlphaFillMode) {
            case DXVAHD_ALPHA_FILL_MODE_OPAQUE:
            case DXVAHD_ALPHA_FILL_MODE_BACKGROUND:
            case DXVAHD_ALPHA_FILL_MODE_DESTINATION:
            case DXVAHD_ALPHA_FILL_MODE_COMPOSITED:
                if (cfg->outputConfig.AlphaFillSlot != 0) {
                    printf("Config-Error: Alpha fill slot not required and should be set to 0\n");
                    success = false;
                }
                break;
#if (LW_VIC_SUPPORT_ALPHA_FILL_MODE_SOURCE_STREAM == 1)
            case DXVAHD_ALPHA_FILL_MODE_SOURCE_STREAM:
#endif
            case DXVAHD_ALPHA_FILL_MODE_SOURCE_ALPHA:
                if (cfg->outputConfig.AlphaFillSlot > (LW_VIC_SLOT_COUNT-1)) {
                    printf("Config-Error: Alpha fill slot set to illegal index\n");
                    success = false;
                } else if (enable[cfg->outputConfig.AlphaFillSlot] == 0) {
                    printf("Config-Error: Alpha fill slot set to disabled slot\n");
                    success = false;
                }
                break;
            default:
                printf("Config-Error: Illegal alpha fill mode\n");
                success = false;
        }
        /*    if ((cfg->blending0Struct.reserved0 != 0) ||
              (cfg->blending0Struct.reserved1 != 0) ||
              (cfg->blending0Struct.reserved2 != 0) ||
              (cfg->blending0Struct.reserved3 != 0) ||
              (cfg->blending0Struct.reserved4 != 0) ||
              (cfg->blending0Struct.reserved5 != 0) ||
              (cfg->blending0Struct.reserved6 != 0) ||
              (cfg->blending0Struct.reserved7 != 0) ||
              (cfg->blending0Struct.reserved8 != 0) ||
              (cfg->blending0Struct.reserved9 != 0) ||
              (cfg->blending0Struct.reserved10 != 0) ||
              (cfg->blending0Struct.reserved11 != 0)) {
              printf("Config-Error: Reserved values set to non-zero in blending0Struct\n");
              success = false;
              }*/
    }
    // check blendingSurfaceStruct
    // TODO: check blendingSurfactStruct config values

    // check slotConfig Struct
    for(int i=0; i<LW_VIC_SLOT_COUNT; i++)
    {
        if ((cfg->slotStruct[i].slotConfig.DeinterlaceMode > DXVAHD_DEINTERLACE_MODE_PRIVATE_WEAVE_LUMA_BOB_FIELD_CHROMA) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_BOB) && (!frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_WEAVE_LUMA_BOB_FIELD_CHROMA) && (frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_NEWBOB) && (frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_BOB_FIELD) && (frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_DISI1) && (frame[i]))) {
            printf("Config-Error: Illegal deinterlace mode\n");
            success = false;
            break;
        }
        /*       if ((cfg->fetchControl0Struct.reserved0 != 0) ||
                 (cfg->fetchControl0Struct.reserved1 != 0) ||
                 (cfg->fetchControl0Struct.reserved2 != 0) ||
                 (cfg->fetchControl0Struct.reserved3 != 0) ||
                 (cfg->fetchControl0Struct.reserved4 != 0) ||
                 (cfg->fetchControl0Struct.reserved5 != 0) ||
                 (cfg->fetchControl0Struct.reserved6 != 0) ||
                 (cfg->fetchControl0Struct.reserved7 != 0) ||
                 (cfg->fetchControl0Struct.reserved8 != 0) ||
                 (cfg->fetchControl0Struct.reserved9 != 0) ||
                 (cfg->fetchControl0Struct.reserved10 != 0) ||
                 (cfg->fetchControl0Struct.reserved11 != 0) ||
                 (cfg->fetchControl0Struct.reserved12 != 0) ||
                 (cfg->fetchControl0Struct.reserved13 != 0) ||
                 (cfg->fetchControl0Struct.reserved14 != 0) ||
                 (cfg->fetchControl0Struct.reserved15 != 0) ||
                 (cfg->fetchControl0Struct.reserved16 != 0) ||
                 (cfg->fetchControl0Struct.reserved17 != 0) ||
                 (cfg->fetchControl0Struct.reserved18 != 0) ||
                 (cfg->fetchControl0Struct.reserved19 != 0)) {
                 printf("Config-Error: Reserved values set to non-zero in fetchControl0Struct\n");
                 success = false;
                 }*/
        if ((cfg->slotStruct[i].slotConfig.NoiseIir > 0x400)) {
            printf("Config-Error: iir field programmed to a value more than max - 0x400\n");
            success = false;
            break;
        }
    }
    // check fetchControlCoeffStruct
    /*   for (int i=0; i<520; i++) {
         if ((cfg->fetchControlCoeffStruct[i].reserved0 != 0) ||
         (cfg->fetchControlCoeffStruct[i].reserved1 != 0) ||
         (cfg->fetchControlCoeffStruct[i].reserved2 != 0) ||
         (cfg->fetchControlCoeffStruct[i].reserved3 != 0)) {
         printf("Config-Error: Reserved values set to non-zero in fetchControlCoeffStruct\n");
         success = false;
         }
         }*/
    return success;

    // ===============================================================
    // Everything above this point was copied from drv::check_config()
    // ===============================================================

#undef printf
#undef sr_rnd_dn
#undef sr_rnd_up
#undef LW_CHIP_VIC_HISTOGRAM_CORRECTION_DISABLE
#undef LW_CHIP_VIC_CENHANCE_DISABLE
#undef LW_VIC_SUPPORT_ALPHA_FILL_MODE_SOURCE_STREAM
#undef LW_VIC_VDM_SUPPORT
}

//--------------------------------------------------------------------
//! Return the LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_LUMA_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 TegraVic41Helper::GetLumaMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    static const UINT32 SURFACE_STRIDE =
        LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE1_LUMA_OFFSET(0) -
        LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_LUMA_OFFSET(0);
    return (LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_LUMA_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}

//--------------------------------------------------------------------
//! Return the LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_CHROMA_U_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 TegraVic41Helper::GetChromaUMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    static const UINT32 SURFACE_STRIDE =
        LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE1_CHROMA_U_OFFSET(0) -
        LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_U_OFFSET(0);
    return (LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_U_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}

//--------------------------------------------------------------------
//! Return the LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_CHROMA_V_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 TegraVic41Helper::GetChromaVMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    static const UINT32 SURFACE_STRIDE =
        LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE1_CHROMA_V_OFFSET(0) -
        LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_V_OFFSET(0);
    return (LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_V_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}

namespace
{
    namespace Vic42TmpNamespace
    {
        bool is_yuv_format(Vic42::PIXELFORMAT f)
        {
            using namespace Vic42;
            switch (f)
            {
            // 1BPP, 2BPP & Index surfaces
            case T_L8:
            case T_U8:
            case T_V8:
            case T_U8V8:
            case T_V8U8:
            case T_A4L4:
            case T_L4A4:
            case T_A8L8:
            case T_L8A8:
            case T_L10:
            case T_U10:
            case T_V10:
            case T_U10V10:
            case T_V10U10:
                // Packed YUV with Alpha formats
            case T_A8Y8U8V8:
            case T_V8U8Y8A8:
                // Packed subsampled YUV 422
            case T_Y8_U8__Y8_V8:
            case T_Y8_V8__Y8_U8:
            case T_U8_Y8__V8_Y8:
            case T_V8_Y8__U8_Y8:
                // Semi-planar YUV
            case T_Y8___U8V8_N444:
            case T_Y8___V8U8_N444:
            case T_Y8___U8V8_N422:
            case T_Y8___V8U8_N422:
            case T_Y8___U8V8_N422R:
            case T_Y8___V8U8_N422R:
            case T_Y8___U8V8_N420:
            case T_Y8___V8U8_N420:
            case T_Y10___U10V10_N444:
            case T_Y10___V10U10_N444:
            case T_Y10___U10V10_N422:
            case T_Y10___V10U10_N422:
            case T_Y10___U10V10_N422R:
            case T_Y10___V10U10_N422R:
            case T_Y10___U10V10_N420:
            case T_Y10___V10U10_N420:
                // Planar YUV
            case T_Y8___U8___V8_N444:
            case T_Y8___U8___V8_N422:
            case T_Y8___U8___V8_N422R:
            case T_Y8___U8___V8_N420:
            case T_Y10___U10___V10_N444:
            case T_Y10___U10___V10_N422:
            case T_Y10___U10___V10_N422R:
            case T_Y10___U10___V10_N420:
                return true;
            default:
                return false;
            }
        }
    }
}

//--------------------------------------------------------------------
//! \brief Verify that the VIC 4.2 config struct contains valid values
//!
//! Copied from drv::check_config() in
//! //hw/gpu_ip/unit/vic/4.2/cmod/driver/drv.cpp
//!
/* virtual */ bool TegraVic42Helper::CheckConfigStruct
(
    const void *pConfigStruct
) const
{
    // Assorted hacks so that we can import drv::check_config() to
    // mods with minimal rewrite.
    //
    using namespace Vic42;
    namespace Image = Vic42TmpNamespace;
    const ConfigStruct *cfg = static_cast<const ConfigStruct*>(pConfigStruct);
    const UINT08 LW_VIC_SLOT_COUNT = Vic42::MAX_SLOTS;
    #define printf PrintfForCheckConfig
    #define sr_rnd_dn(fxp) static_cast<int>(FxpToIntFloor(fxp))
    #define sr_rnd_up(fxp) static_cast<int>(FxpToIntCeil(fxp))
    #define LW_CHIP_VIC_HISTOGRAM_CORRECTION_DISABLE 1
    #define LW_CHIP_VIC_CENHANCE_DISABLE 1
    #define LW_VIC_SUPPORT_ALPHA_FILL_MODE_SOURCE_STREAM 0
    #define LW_VIC_VDM_SUPPORT 0

    // ===============================================================
    // Everything below this point was copied from drv::check_config()
    // ===============================================================

    // we need this a couple of times.
    bool frame[LW_VIC_SLOT_COUNT];
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        frame[i] = ((cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_PROGRESSIVE) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_SUBPIC_PROGRESSIVE) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_TOP_FIELD_FIRST) ||
                (cfg->slotStruct[i].slotConfig.FrameFormat == DXVAHD_FRAME_FORMAT_SUBPIC_INTERLACED_BOTTOM_FIELD_FIRST));
    }
    bool advanced[LW_VIC_SLOT_COUNT];
    // is this too restrictive?  - SIVA: How to modify this
    for(int i=0;i<LW_VIC_SLOT_COUNT;i++) {
        advanced[i] = (
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8_U8__Y8_V8) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_U8_Y8__V8_Y8) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8___V8U8_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y8___U8___V8_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y10___V10U10_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y10___U10___V10_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y12___V12U12_N420) ||
                (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat == T_Y12___U12___V12_N420));
    }

    unsigned int enable[LW_VIC_SLOT_COUNT];
    for(int i=0; i<LW_VIC_SLOT_COUNT; i++)    {
        enable[i] = cfg->slotStruct[i].slotConfig.LwrrentFieldEnable |
            cfg->slotStruct[i].slotConfig.PrevFieldEnable << 1 |
            cfg->slotStruct[i].slotConfig.NextFieldEnable << 2 |
            cfg->slotStruct[i].slotConfig.NextNrFieldEnable << 3 |
            cfg->slotStruct[i].slotConfig.LwrMotionFieldEnable << 4 |
            cfg->slotStruct[i].slotConfig.PrevMotionFieldEnable << 5 |
            cfg->slotStruct[i].slotConfig.PpMotionFieldEnable << 6 |
            cfg->slotStruct[i].slotConfig.CombMotionFieldEnable << 7 ;
    }
    bool success = true;
    for(int i=0;i<LW_VIC_SLOT_COUNT; i++) {
#if (LW_VIC_VDM_SUPPORT == 0)
        if((cfg->slotStruct[i].slotConfig.DegammaMode == GAMMA_MODE_REC709) && (!Image::is_yuv_format((PIXELFORMAT) cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat))) {
            printf("Config-Error: Degamma Mode REC709 Enabled on a non YUV format\n");
            success = false;
            break;
        }
#endif

        if (((cfg->slotStruct[i].slotConfig.CadenceDetect) && (((enable[i] & 7) != 7) || (frame[i]) || (!advanced[i])))) {
            printf("Config-Error: Cadence detection requires field based surfaces with forward and backward reference\n");
            success = false;
            break;
        }

        if (((cfg->slotStruct[i].slotConfig.DeNoise) && (((!frame[i]) && ((enable[i] & 15) != 15)) || ((frame[i]) && ((enable[i] & 7) != 7)) || (!advanced[i])))) {
            printf("Config-Error: Noise reduction requires field based surfaces with forward and backward reference plus noise reduced output or frame based surfaces with backward reference and noise reduced output\n");
            success = false;
            break;
        }

        if (((!(cfg->slotStruct[i].slotConfig.DeNoise)) && (frame[i]) && (enable[i] > 1))) {
            printf("Config-Error: Frame based input without noise reduction only requires current frame\n");
            success = false;
            break;
        }

        if (((cfg->slotStruct[i].slotConfig.MotionMap) && (((enable[i] & 0x17) != 0x17) || (frame[i]) || (!advanced[i])))) {
            printf("Config-Error: Motion map callwlation requires field based surfaces with forward and backward reference plus current motion buffer\n");
            success = false;
        }
        if (((cfg->slotStruct[i].slotConfig.MMapCombine) && ((!cfg->slotStruct[i].slotConfig.MotionMap) || ((enable[i] & 0xb7) != 0xb7) || (frame[i]) || (!advanced[i])))) {
            printf("Config-Error: Combine motion map requires field based surfaces with current motion buffer callwlation, prev motion buffer and combined motion buffer\n");
            success = false;
        }

    }

    // check clear rect
    for(int i=0; i<LW_VIC_SLOT_COUNT;i++)
    {
        // clear rects will be checked below
        /*   if ((cfg->slotStruct[i].slotConfig.reserved0 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved1 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved2 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved3 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved4 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved5 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved6 != 0) ||
             (cfg->slotStruct[i].slotConfig.reserved7 != 0)) {
             printf("Config-Error: Reserved values set to non-zero in surfaceList0Struct\n");
             success = false;
             }*/
    }
    // check ClearRectStruct
    {
        unsigned int all_clear = cfg->slotStruct[0].slotConfig.ClearRectMask;
        for(int i=1; i<LW_VIC_SLOT_COUNT;i++) all_clear |= cfg->slotStruct[i].slotConfig.ClearRectMask;

        for (int i=0; i<4; i++) {
            if (((all_clear >> (i*2)) & 1) != 0) {
                if ((cfg->clearRectStruct[i].ClearRect0Left > cfg->clearRectStruct[i].ClearRect0Right) ||
                        (cfg->clearRectStruct[i].ClearRect0Top  > cfg->clearRectStruct[i].ClearRect0Bottom)) {
                    printf("Config-Error: Clear rect enabled but defined empty\n");
                    success = false;
                }
            }
            if (((all_clear >> (i*2+1)) & 1) != 0) {
                if ((cfg->clearRectStruct[i].ClearRect1Left > cfg->clearRectStruct[i].ClearRect1Right) ||
                        (cfg->clearRectStruct[i].ClearRect1Top  > cfg->clearRectStruct[i].ClearRect1Bottom)) {
                    printf("Config-Error: Clear rect enabled but defined empty\n");
                    success = false;
                }
            }
            if ((cfg->clearRectStruct[i].reserved0 != 0) ||
                    (cfg->clearRectStruct[i].reserved1 != 0) ||
                    (cfg->clearRectStruct[i].reserved2 != 0) ||
                    (cfg->clearRectStruct[i].reserved3 != 0) ||
                    (cfg->clearRectStruct[i].reserved4 != 0) ||
                    (cfg->clearRectStruct[i].reserved5 != 0) ||
                    (cfg->clearRectStruct[i].reserved6 != 0) ||
                    (cfg->clearRectStruct[i].reserved7 != 0)) {
                printf("Config-Error: Reserved values set to non-zero in surfaceListClearRectStruct\n");
                success = false;
            }
        }
    }
    // check surfaceListSurfaceStruct
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        if (cfg->slotStruct[i].slotConfig.SlotEnable) {
            if (enable[i] == 0) {
                printf("Config-Error: Slot enabled in surface list but not in fetch control\n");
                success = false;
            }
            unsigned int min_x=0, min_y=0, mult_y=0;
            switch (cfg->slotStruct[i].slotSurfaceConfig.SlotPixelFormat) {
                case T_A8R8G8B8:
                case T_A8B8G8R8:
                case T_X8B8G8R8:
                case T_B8G8R8X8:
                case T_R8G8B8X8:
                case T_A8Y8U8V8:
                case T_V8U8Y8A8:
                case T_A2B10G10R10:
                case T_B10G10R10A2:
                case T_R10G10B10A2:
                case T_A8:
                case T_A4L4:
                case T_L4A4:
                case T_R8:
                case T_A8L8:
                case T_R8G8:
                case T_G8R8:
                case T_B5G6R5:
                case T_R5G6B5:
                case T_B6G5R5:
                case T_R5G5B6:
                case T_A1B5G5R5:
                case T_A1R5G5B5:
                case T_B5G5R5A1:
                case T_R5G5B5A1:
                case T_A5B5G5R1:
                case T_A5R1G5B5:
                case T_B5G5R1A5:
                case T_R1G5B5A5:
                case T_X1B5G5R5:
                case T_X1R5G5B5:
                case T_B5G5R5X1:
                case T_R5G5B5X1:
                case T_A4B4G4R4:
                case T_A4R4G4B4:
                case T_B4G4R4A4:
                case T_R4G4B4A4:
                case T_Y8___U8___V8_N444:
                case T_R8G8B8A8:
                case T_X8R8G8B8:
                case T_A2R10G10B10:
                case T_B8G8R8A8:
                case T_A8P8:
                case T_P8:
                case T_P8A8:
                case T_L8A8:
                case T_L8:
                case T_A4P4:
                case T_P4A4:
                case T_U8V8:
                case T_V8U8:
                case T_U8:
                case T_V8:
                case T_Y8___V8U8_N444:
                case T_Y8___U8V8_N444:
                case T_Y10___U10___V10_N444:
                case T_L10:
                case T_U10V10:
                case T_V10U10:
                case T_U10:
                case T_V10:
                case T_Y10___V10U10_N444:
                case T_Y10___U10V10_N444:
                case T_Y12___U12___V12_N444:
                case T_L12:
                case T_L16:
                case T_U12V12:
                case T_V12U12:
                case T_V16U16:
                case T_U12:
                case T_V12:
                case T_Y12___V12U12_N444:
                case T_Y12___U12V12_N444:
                case T_A16B16G16R16:
                case T_A16Y16U16V16:
                    min_x = mult_y = 1;
                    break;
                case T_Y8_U8__Y8_V8:
                case T_Y8_V8__Y8_U8:
                case T_U8_Y8__V8_Y8:
                case T_V8_Y8__U8_Y8:
                    min_x = 2;
                    mult_y = 1;
                    break;
                case T_Y8___V8U8_N420:
                case T_Y8___U8V8_N420:
                case T_Y8___U8___V8_N420:
                case T_Y10___V10U10_N420:
                case T_Y10___U10V10_N420:
                case T_Y10___U10___V10_N420:
                case T_Y12___V12U12_N420:
                case T_Y12___U12V12_N420:
                case T_Y12___U12___V12_N420:
                    min_x = mult_y = 2;
                    break;
                case T_Y8___V8U8_N422:
                case T_Y8___U8V8_N422:
                case T_Y8___U8___V8_N422:
                case T_Y10___V10U10_N422:
                case T_Y10___U10V10_N422:
                case T_Y10___U10___V10_N422:
                case T_Y12___V12U12_N422:
                case T_Y12___U12V12_N422:
                case T_Y12___U12___V12_N422:
                    min_x = 2;
                    mult_y = 1;
                    break;
                case T_Y8___V8U8_N422R:
                case T_Y8___U8V8_N422R:
                case T_Y8___U8___V8_N422R:
                case T_Y10___V10U10_N422R:
                case T_Y10___U10V10_N422R:
                case T_Y10___U10___V10_N422R:
                case T_Y12___V12U12_N422R:
                case T_Y12___U12V12_N422R:
                case T_Y12___U12___V12_N422R:
                    min_x = 1;
                    mult_y = 2;
                    break;
                default:
                    printf("Config-Error: Invalid pixel format\n");
                    success = false;
            }
            if (!frame[i]) min_y = mult_y << 1;
            else min_y = mult_y;
            unsigned int left    = sr_rnd_dn(cfg->slotStruct[i].slotConfig.SourceRectLeft);
            unsigned int right   = sr_rnd_up(cfg->slotStruct[i].slotConfig.SourceRectRight) + 1;
            unsigned int top     = sr_rnd_dn(cfg->slotStruct[i].slotConfig.SourceRectTop);
            unsigned int bottom  = sr_rnd_up(cfg->slotStruct[i].slotConfig.SourceRectBottom) + 1;
            unsigned int width   = (unsigned int)cfg->slotStruct[i].slotSurfaceConfig.SlotSurfaceWidth+ 1;
            unsigned int height  = (unsigned int)cfg->slotStruct[i].slotSurfaceConfig.SlotSurfaceHeight+ 1;
            unsigned int dleft   = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectLeft;
            unsigned int dright  = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectRight + 1;
            unsigned int dtop    = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectTop;
            unsigned int dbottom = (unsigned int)cfg->slotStruct[i].slotConfig.DestRectBottom + 1;
            unsigned int swidth  = right - left + 1;
            unsigned int sheight = bottom - top + 1;
            unsigned int dwidth  = dright - dleft + 1;
            unsigned int dheight = dbottom - dtop + 1;

            if((cfg->slotStruct[i].slotConfig.Panoramic != 0) && (dwidth < 4)) {
                printf("Config-Error: Dest rect should be atleast 4 pixels wide for panoramic scaling to work\n");
                success = false;
            }
            if ((swidth < min_x) || (sheight < min_y)) {
                printf("Config-Error: Source rect has to contain at least one chroma in every field\n");
                success = false;
            }
            if (((width & (min_x-1)) != 0) || ((height & (mult_y-1)) != 0)) {
                printf("Config-Error: Source surface has to be a multiple of to full pixels \n details: %d %d %d %d \n",width,min_x,height,mult_y);
                success = false;
            }
            if ((left >= right) || (top >= bottom) || (dleft >= dright) || (dtop >= dbottom)) {
                printf("Config-Error: Source or dest rect empty\n");
                success = false;
            }
            if (cfg->slotStruct[i].slotConfig.Panoramic != 0) {
                if ((dwidth * 8 < swidth) || (dheight * 16 < sheight)) {
                    printf("Config-Error: Downsclaing ratio of 8:1 with panoramic exceeded\n");
                    success = false;
                }
            } else {
                if ((dwidth * 16 < swidth) || (dheight * 16 < sheight)) {
                    printf("Config-Error: Downsclaing ratio of 16:1 exceeded\n");
                    success = false;
                }
            }
        } else {
            if (enable[i] != 0) {
                printf("Config-Error: Slot enabled in fetch control but not in surface list\n");
                success = false;
            }
        }
        //TODO: error handling for reserved bits slotConfig and slotSurfaceConfig

    }
    // check LumaKeyStruct
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        if ((cfg->slotStruct[i].lumaKeyStruct.LumaKeyEnabled) &&
                (cfg->slotStruct[i].lumaKeyStruct.LumaKeyLower > cfg->slotStruct[i].lumaKeyStruct.LumaKeyUpper)) {
            printf("Config-Error: Luma key enabled but region empty\n");
            success = false;
        }/*
            if ((cfg->colorColwersionLumaAlphaStruct[i].reserved6 != 0) ||
            (cfg->colorColwersionLumaAlphaStruct[i].reserved7 != 0)) {
            printf("Config-Error: Reserved values set to non-zero in colorColwersionLumaAlphaStruct\n");
            success = false;
            }*/
    }

#if (LW_CHIP_VIC_HISTOGRAM_CORRECTION_DISABLE==0)
    // check colorColwersionLutStruct
    for (int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        // nothing to check for
        if ((cfg->colorColwersionLutStruct[i].reserved0 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved1 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved2 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved3 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved4 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved5 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved6 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved7 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved8 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved9 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved10 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved11 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved12 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved13 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved14 != 0) ||
                (cfg->colorColwersionLutStruct[i].reserved15 != 0)) {
            printf("Config-Error: Reserved values set to non-zero in colorColwersionLutStruct\n");
            success = false;
        }
    }
#endif
    // check colorColwersionMatrixStruct
    /*  for (unsigned int i=0; i<ColorColwersionMatrixStruct_NUM; i++) {
    // nothing to check for
    if ((cfg->slotStruct[i].slotConfig.matrixStruct.reserved0 != 0) ||
    (cfg->colorColwersionMatrixStruct[i].reserved1 != 0) ||
    (cfg->colorColwersionMatrixStruct[i].reserved2 != 0)) {
    printf("Config-Error: Reserved values set to non-zero in colorColwersionMatrixStruct\n");
    success = false;
    }
    }*/
    // check colorColwersionClampStruct
    for (unsigned int i=0; i<LW_VIC_SLOT_COUNT; i++) {
        if (cfg->slotStruct[i].slotConfig.SoftClampLow > cfg->slotStruct[i].slotConfig.SoftClampHigh) {
            printf("Config-Error: Clamp low has to be less or equal to clamp high\n");
            success = false;
        }
        /*    if ((cfg->colorColwersionClampStruct[i].reserved0 != 0) ||
              (cfg->colorColwersionClampStruct[i].reserved1 != 0) ||
              (cfg->colorColwersionClampStruct[i].reserved2 != 0) ||
              (cfg->colorColwersionClampStruct[i].reserved3 != 0) ||
              (cfg->colorColwersionClampStruct[i].reserved4 != 0)) {
              printf("Config-Error: Reserved values set to non-zero in colorColwersionClampStruct\n");
              success = false;
              }*/
    }
#if (LW_CHIP_VIC_CENHANCE_DISABLE==0)
    // check colorColwersionCEnhanceStruct
    for (unsigned int i=0; i<ColorColwersionCEnhanceStruct_NUM; i++) {
        if ((cfg->colorColwersionCEnhanceStruct[i].param0 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].param1 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].reserved0 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].reserved1 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec00 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec01 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec02 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec03 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec10 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec11 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec12 != 0) ||
                (cfg->colorColwersionCEnhanceStruct[i].vec13 != 0)) {
            printf("Config-Error: Reserved or P1 feature set to non-zero value in colorColwersionCEnhanceStruct\n");
        }
    }
#endif

    // check blendingStruct
    {
        switch (cfg->outputConfig.AlphaFillMode) {
            case DXVAHD_ALPHA_FILL_MODE_OPAQUE:
            case DXVAHD_ALPHA_FILL_MODE_BACKGROUND:
            case DXVAHD_ALPHA_FILL_MODE_DESTINATION:
            case DXVAHD_ALPHA_FILL_MODE_COMPOSITED:
                if (cfg->outputConfig.AlphaFillSlot != 0) {
                    printf("Config-Error: Alpha fill slot not required and should be set to 0\n");
                    success = false;
                }
                break;
#if (LW_VIC_SUPPORT_ALPHA_FILL_MODE_SOURCE_STREAM == 1)
            case DXVAHD_ALPHA_FILL_MODE_SOURCE_STREAM:
#endif
            case DXVAHD_ALPHA_FILL_MODE_SOURCE_ALPHA:
                if (cfg->outputConfig.AlphaFillSlot > (LW_VIC_SLOT_COUNT-1)) {
                    printf("Config-Error: Alpha fill slot set to illegal index\n");
                    success = false;
                } else if (enable[cfg->outputConfig.AlphaFillSlot] == 0) {
                    printf("Config-Error: Alpha fill slot set to disabled slot\n");
                    success = false;
                }
                break;
            default:
                printf("Config-Error: Illegal alpha fill mode\n");
                success = false;
        }
        /*    if ((cfg->blending0Struct.reserved0 != 0) ||
              (cfg->blending0Struct.reserved1 != 0) ||
              (cfg->blending0Struct.reserved2 != 0) ||
              (cfg->blending0Struct.reserved3 != 0) ||
              (cfg->blending0Struct.reserved4 != 0) ||
              (cfg->blending0Struct.reserved5 != 0) ||
              (cfg->blending0Struct.reserved6 != 0) ||
              (cfg->blending0Struct.reserved7 != 0) ||
              (cfg->blending0Struct.reserved8 != 0) ||
              (cfg->blending0Struct.reserved9 != 0) ||
              (cfg->blending0Struct.reserved10 != 0) ||
              (cfg->blending0Struct.reserved11 != 0)) {
              printf("Config-Error: Reserved values set to non-zero in blending0Struct\n");
              success = false;
              }*/
    }
    // check blendingSurfaceStruct
    // TODO: check blendingSurfactStruct config values

    // check slotConfig Struct
    for(int i=0; i<LW_VIC_SLOT_COUNT; i++)
    {
        if ((cfg->slotStruct[i].slotConfig.DeinterlaceMode > DXVAHD_DEINTERLACE_MODE_PRIVATE_WEAVE_LUMA_BOB_FIELD_CHROMA) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_BOB) && (!frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_WEAVE_LUMA_BOB_FIELD_CHROMA) && (frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_NEWBOB) && (frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_BOB_FIELD) && (frame[i])) ||
                ((cfg->slotStruct[i].slotConfig.DeinterlaceMode == DXVAHD_DEINTERLACE_MODE_PRIVATE_DISI1) && (frame[i]))) {
            printf("Config-Error: Illegal deinterlace mode\n");
            success = false;
            break;
        }
        /*       if ((cfg->fetchControl0Struct.reserved0 != 0) ||
                 (cfg->fetchControl0Struct.reserved1 != 0) ||
                 (cfg->fetchControl0Struct.reserved2 != 0) ||
                 (cfg->fetchControl0Struct.reserved3 != 0) ||
                 (cfg->fetchControl0Struct.reserved4 != 0) ||
                 (cfg->fetchControl0Struct.reserved5 != 0) ||
                 (cfg->fetchControl0Struct.reserved6 != 0) ||
                 (cfg->fetchControl0Struct.reserved7 != 0) ||
                 (cfg->fetchControl0Struct.reserved8 != 0) ||
                 (cfg->fetchControl0Struct.reserved9 != 0) ||
                 (cfg->fetchControl0Struct.reserved10 != 0) ||
                 (cfg->fetchControl0Struct.reserved11 != 0) ||
                 (cfg->fetchControl0Struct.reserved12 != 0) ||
                 (cfg->fetchControl0Struct.reserved13 != 0) ||
                 (cfg->fetchControl0Struct.reserved14 != 0) ||
                 (cfg->fetchControl0Struct.reserved15 != 0) ||
                 (cfg->fetchControl0Struct.reserved16 != 0) ||
                 (cfg->fetchControl0Struct.reserved17 != 0) ||
                 (cfg->fetchControl0Struct.reserved18 != 0) ||
                 (cfg->fetchControl0Struct.reserved19 != 0)) {
                 printf("Config-Error: Reserved values set to non-zero in fetchControl0Struct\n");
                 success = false;
                 }*/
        if ((cfg->slotStruct[i].slotConfig.NoiseIir > 0x400)) {
            printf("Config-Error: iir field programmed to a value more than max - 0x400\n");
            success = false;
            break;
        }
    }
    // check fetchControlCoeffStruct
    /*   for (int i=0; i<520; i++) {
         if ((cfg->fetchControlCoeffStruct[i].reserved0 != 0) ||
         (cfg->fetchControlCoeffStruct[i].reserved1 != 0) ||
         (cfg->fetchControlCoeffStruct[i].reserved2 != 0) ||
         (cfg->fetchControlCoeffStruct[i].reserved3 != 0)) {
         printf("Config-Error: Reserved values set to non-zero in fetchControlCoeffStruct\n");
         success = false;
         }
         }*/
    return success;
}

//--------------------------------------------------------------------
//! Return the LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_LUMA_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 TegraVic42Helper::GetLumaMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    static const UINT32 SURFACE_STRIDE =
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE1_LUMA_OFFSET(0) -
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_LUMA_OFFSET(0);
    return (LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_LUMA_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}

//--------------------------------------------------------------------
//! Return the LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_CHROMA_U_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 TegraVic42Helper::GetChromaUMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    static const UINT32 SURFACE_STRIDE =
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE1_CHROMA_U_OFFSET(0) -
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_U_OFFSET(0);
    return (LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_U_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}

//--------------------------------------------------------------------
//! Return the LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE<m>_CHROMA_V_OFFSET(<n>)
//! method for surface <m> and slot <n>.
//!
/* virtual */ UINT32 TegraVic42Helper::GetChromaVMethod
(
    UINT32 slotIdx,
    UINT32 surfaceIdx
) const
{
    static const UINT32 SURFACE_STRIDE =
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE1_CHROMA_V_OFFSET(0) -
        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_V_OFFSET(0);
    return (LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_CHROMA_V_OFFSET(slotIdx) +
            surfaceIdx * SURFACE_STRIDE);
}

