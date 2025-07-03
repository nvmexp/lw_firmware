/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2010,2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  surfcomp.h
 * @brief Utilities for reading compression tags - implementation.
 */

#include "surfcomp.h"
#include "surfrdwr.h"
#include "surf2d.h"
#include "core/include/imagefil.h"
#include "gpu/include/gpudev.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"

#include "fermi/gf100/dev_ltc.h"
#include "fermi/gf100/dev_mmu.h"
#include "fermi/gf100/hwproject.h"
#include "ctrl/ctrl0041.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl9096.h"

const UINT32 CompTagLineRange = 128*1024; // Comptag line size on Fermi is 128KB

using namespace SurfaceUtils;

bool SurfaceUtils::IsZBCValueEqual(const ZBCValue& v1, const ZBCValue& v2)
{
    if (v1.comprType != v2.comprType)
    {
        return false;
    }
    for (int i=0; i < ZBCValue::maxClearValues; i++)
    {
        if (v1.clearValue[i] != v2.clearValue[i])
        {
            return false;
        }
    }
    return true;
}

RC GpuSubdevice::SetCompressionMap
(
    Surface* pSurf,
    const vector<UINT32>& ComprMap
)
{
    RC rc;

    // Surface must be allocated, must be in vidmem and must be blocklinear
    MASSERT(pSurf->GetMemHandle());
    MASSERT(pSurf->GetLayout() == Surface::BlockLinear);
    MASSERT(pSurf->GetLocation() == Memory::Fb);

    // Get GOB decoding properties
    const UINT32 GpuGobWidth = pSurf->GetGpuDev()->GobWidth();
    const UINT32 GpuGobHeight = pSurf->GetGpuDev()->GobHeight();
    MASSERT(pSurf->GetGpuDev()->GobDepth() == 1);
    const UINT32 GpuGobSize = GpuGobWidth * GpuGobHeight;
    MASSERT((GpuGobSize & (GpuGobSize - 1)) == 0);
    const UINT32 BytesPerPixel = pSurf->GetBytesPerPixel();
    const UINT32 GobWidth = pSurf->GetGobWidth();

    // Figure out ZBC and reduction compression types for current PTE
    const ZBCComprType ZbcCompr = GetZbcComprType(pSurf->GetPteKind());
    const ZBCComprType ReductionCompr = GetReductionComprType(pSurf->GetPteKind());

    const UINT32 UncompressedPattern = 0xC0000000;
    const UINT32 ReductionCompressedPattern = 0xFFFFFFFF;

    // Create ZBC setup based on the comptag bitmap
    Printf(Tee::PriNormal, "ZBC clearing surface, PTE kind 0x%x\n", pSurf->GetPteKind());
    const UINT32 TileSize = GpuGobSize / 2;
    MASSERT((CompTagLineRange % TileSize) == 0);
    const UINT32 TilesPerCompTagLine = CompTagLineRange / TileSize;
    ZBCComprType ComprType = zbc_uncompressed;
    UINT32 ClearColor = 0;
    vector<ZBCValue> ClearValues;
    bool DoZbc = false;
    for (UINT32 Tile=0; ; Tile++)
    {
        if (((Tile % TilesPerCompTagLine) == 0) || Tile == ComprMap.size())
        {
            if (Tile != 0)
            {
                ZBCValue NewValue = { ComprType };
                NewValue.clearValue[0] = ClearColor;
                Printf(Tee::PriNormal, "ZBC rel_ctag_line %u, type %u, color 0x%08x\n",
                    Tile/TilesPerCompTagLine-1, static_cast<UINT32>(ComprType), ClearColor);

                // Colwert the color if needed
                const ColorUtils::Format ColorFormat = pSurf->GetColorFormat();
                if (ColorFormat == ColorUtils::RF16_GF16_BF16_AF16)
                {
                    // Colwert manually, because ColorUtils::Colwert from PNG to
                    // FP16 has a workaround for Tesla bugs, so we cannot
                    // use it (and we cannot change it).

                    // Extract components assuming A2B10G10R10
                    const UINT32 Red   = ClearColor & 0x3FF;
                    const UINT32 Green = (ClearColor >> 10) & 0x3FF;
                    const UINT32 Blue  = (ClearColor >> 20) & 0x3FF;

                    // Colwert to FP16 in display standard 'oversaturated' format
                    NewValue.clearValue[0] =
                        (Utility::Float32ToFloat16((float)Green) << 16) |
                        (Utility::Float32ToFloat16((float)Red) & 0xFFFF);
                    NewValue.clearValue[1] =
                        Utility::Float32ToFloat16((float)Blue);
                }

                ClearValues.push_back(NewValue);
            }

            ClearColor = 0;
            ComprType = zbc_uncompressed;

            if (Tile == ComprMap.size())
            {
                break;
            }
        }
        const UINT32 Color = ComprMap[Tile];
        // Black with alpha=0xC0 indicates uncompressed tile
        if (Color == UncompressedPattern)
        {
        }
        // White with alpha=0xFF indicates reduction-compressed tile
        else if (Color == ReductionCompressedPattern)
        {
            if (ComprType == zbc_uncompressed)
            {
                if (ReductionCompr == zbc_uncompressed)
                {
                    Printf(Tee::PriHigh, "Error: Reduction compression not supported by PTE kind 0x%x!\n", pSurf->GetPteKind());
                    Printf(Tee::PriHigh, "Offending tile: %u\n", Tile);
                    return RC::BAD_PARAMETER;
                }
                ComprType = ReductionCompr;
                DoZbc = true;
            }
            else if (ComprType != ReductionCompr)
            {
                Printf(Tee::PriHigh, "Error: Two ZBC compression types specified in one comptag line!\n");
                Printf(Tee::PriHigh, "Offending tile: %u\n", Tile);
                return RC::BAD_PARAMETER;
            }
        }
        // Any other color indicates ZBC-compressed (cleared) tile
        else
        {
            if (ComprType == zbc_uncompressed)
            {
                if (ZbcCompr == zbc_uncompressed)
                {
                    Printf(Tee::PriHigh, "Error: ZBC compression not supported by PTE kind 0x%x!\n", pSurf->GetPteKind());
                    Printf(Tee::PriHigh, "Offending tile: %u\n", Tile);
                    return RC::BAD_PARAMETER;
                }
                ComprType = ZbcCompr;
                ClearColor = Color;
                DoZbc = true;
            }
            else if (ComprType != ZbcCompr)
            {
                Printf(Tee::PriHigh, "Error: Two ZBC compression types specified in one comptag line!\n");
                Printf(Tee::PriHigh, "Offending tile: %u\n", Tile);
                return RC::BAD_PARAMETER;
            }
            else if (ClearColor != Color)
            {
                Printf(Tee::PriHigh, "Error: Two ZBC clear colors specified in one comptag line! (offending color: 0x%08X, previous color: 0x%08X)\n", Color, ClearColor);
                Printf(Tee::PriHigh, "Offending tile: %u\n", Tile);
                return RC::BAD_PARAMETER;
            }
        }
    }

    // Skip if ZBC is not necessary
    if (!DoZbc)
    {
        return OK;
    }

    // Step 1/3: Save original contents of uncompressed surface
    vector<UINT08> UncompressedData(static_cast<size_t>(pSurf->GetSize()));
    CHECK_RC(ReadSurface(*pSurf, 0, &UncompressedData[0], UncompressedData.size(), 0));

    // Step 2/3: Make portions of the surface compressed
    const size_t NumCompTagLines = min(
            static_cast<size_t>(((pSurf->GetSize()-1) / CompTagLineRange) + 1),
            ClearValues.size());
    UINT32 FirstTag = 0;
    for (UINT32 i=0; i < NumCompTagLines; i++)
    {
        if (ClearValues[i].comprType != zbc_uncompressed)
        {
            if ((i+1 == NumCompTagLines) ||
                ! IsZBCValueEqual(ClearValues[i+1], ClearValues[i]))
            {
                CHECK_RC(ClearCompTags(*pSurf, FirstTag, i, ClearValues[i]));
                FirstTag = i + 1;
            }
        }
        else
        {
            FirstTag = i + 1;
        }
    }

    // Step 3/3: Rewrite tiles which shall remain uncompressed
    MappingSurfaceWriter Writer(*pSurf);
    for (UINT32 A = 0; A < pSurf->GetArraySize(); A++)
    {
        for (UINT32 GobZ = 0; GobZ < pSurf->GetGobDepth(); GobZ++ )
        {
            for (UINT32 GobY = 0; GobY < pSurf->GetGobHeight(); GobY++)
            {
                for (UINT32 GobX = 0; GobX < pSurf->GetGobWidth(); GobX++)
                {
                    // Compute the logical offset of the gob within the surface
                    const UINT32 PixelX = GobX*GpuGobWidth/BytesPerPixel;
                    const UINT32 PixelY = GobY*GpuGobHeight;
                    const UINT32 PixelZ = GobZ*1;
                    const UINT64 Offset = pSurf->GetPixelOffset(PixelX, PixelY, PixelZ, A);
                    MASSERT(!(Offset & (GpuGobSize-1)));

                    // Discard if the GOB lies in an uncompressed area
                    const UINT32 ComptagLine = UNSIGNED_CAST(UINT32, Offset / CompTagLineRange);
                    if ((ComptagLine >= ClearValues.size()) ||
                        (ClearValues[ComptagLine].comprType == zbc_uncompressed))
                    {
                        continue;
                    }

                    // Rewrite tiles to make them uncompressed
                    const bool Tile0Uncompr = ComprMap[2*GobWidth*GobY + 2*GobX + 0] == UncompressedPattern;
                    const bool Tile1Uncompr = ComprMap[2*GobWidth*GobY + 2*GobX + 1] == UncompressedPattern;
                    if (Tile0Uncompr && Tile1Uncompr)
                    {
                        CHECK_RC(Writer.WriteBytes(Offset,
                            &UncompressedData[UNSIGNED_CAST(size_t, Offset)], GpuGobSize));
                    }
                    else if (Tile0Uncompr || Tile1Uncompr)
                    {
                        const UINT32 TileWidth = (GpuGobWidth / BytesPerPixel) / 2;
                        const UINT32 TileShift = Tile0Uncompr ? 0 : TileWidth;
                        for (UINT32 Y=0; Y < GpuGobHeight; Y++)
                        {
                            for (UINT32 X=0; X < TileWidth; X++)
                            {
                                const UINT64 PixelOffset =
                                    pSurf->GetPixelOffset(PixelX+X+TileShift,
                                        PixelY+Y, PixelZ, A);
                                CHECK_RC(Writer.WriteBytes(PixelOffset,
                                    &UncompressedData[UNSIGNED_CAST(size_t, PixelOffset)], BytesPerPixel));
                            }
                        }
                    }
                }
            }
        }
    }
    return OK;
}

RC SurfaceUtils::ClearCompTags(Surface& Surf, UINT32 InMinTag, UINT32 InMaxTag, const ZBCValue& ClearValue)
{
    MASSERT(InMinTag <= InMaxTag);

    // Account for comptag line coverage other than 100%
    UINT32 MinTag = InMinTag;
    UINT32 MaxTag = InMaxTag;
    const UINT32 CompStartOffs = static_cast<UINT32>(Surf.GetComptagStart() * Surf.GetSize() / 100);
    const UINT32 ComptagLineOffs = CompStartOffs / CompTagLineRange;
    MinTag -= ComptagLineOffs;
    MaxTag -= ComptagLineOffs;

    // Adjust and check comptag line range
    RC rc;
    LwRmPtr pLwRm;
    LW0041_CTRL_GET_SURFACE_COMPRESSION_COVERAGE_PARAMS CompCvgParams = {0};
    CHECK_RC(pLwRm->Control(
        Surf.GetMemHandle(),
        LW0041_CTRL_CMD_GET_SURFACE_COMPRESSION_COVERAGE,
        &CompCvgParams, sizeof(CompCvgParams)));
    MinTag += CompCvgParams.lineMin;
    MaxTag += CompCvgParams.lineMin;
    if ((MinTag < CompCvgParams.lineMin) || (MaxTag > CompCvgParams.lineMax))
    {
        Printf(Tee::PriHigh, "Error: Invalid comptag line range specified for surface.\n");
        Printf(Tee::PriHigh, "ZBC rel_ctag_range: %u..%u, abs_ctag_range: %u..%u, surf_ctag_range: %u..%u, surf_size=0x%llx (%u lines), surf_comp_cvg_start=%u%% (%u lines)\n",
                InMinTag, InMaxTag,
                MinTag, MaxTag,
                static_cast<unsigned>(CompCvgParams.lineMin), static_cast<unsigned>(CompCvgParams.lineMax),
                Surf.GetSize(), static_cast<UINT32>((Surf.GetSize()-1)/CompTagLineRange + 1),
                Surf.GetComptagStart(), ComptagLineOffs);
        return RC::BAD_PARAMETER;
    }

    // 4 bits per comptag for 64bpp Z formats are not supported by this code
    MASSERT(static_cast<unsigned>(ClearValue.comprType) < 4);

    // Get new color index or use existing clear color index
    //
    // LTC (Level Two Cache) keeps a table of clear colors (16 indices on Fermi).
    // A cleared comptag line has associated an index into this table.
    // For tiles marked as cleared LTC reads contents from this table
    // instead of reading the actual surface (this is how Zero Bandwidth Clear works).
    //
    // Below we allocate indices to this color table, remembering which colors
    // we already put in, so we effectively reuse indices for colors which repeat.
    const UINT32 MaxClearColors = 1 << DRF_SIZE(LW_PLTCG_LTCS_LTSS_DSTG_ZBC_INDEX_ADDRESS);
    static UINT32 LastZbcIndex = 0;
    static UINT32 UsedClearColors[MaxClearColors][ZBCValue::maxClearValues] = {{0}};
    UINT32 ZbcIndex = 1;
    for (; ZbcIndex <= LastZbcIndex; ZbcIndex++)
    {
        UINT32 j = 0;
        for (; j < ZBCValue::maxClearValues; j++)
        {
            if (ClearValue.clearValue[j] != UsedClearColors[ZbcIndex][j])
            {
                break;
            }
        }
        if (j == ZBCValue::maxClearValues)
        {
            break;
        }
    }

    // Skip index if the comptag lines are not ZBC compressed
    if (ClearValue.comprType != GetZbcComprType(Surf.GetPteKind()))
    {
        ZbcIndex = LW_PLTCG_LTCS_LTSS_CBC_CLEAR_VALUE_ZBC_INDEX_NONE;
    }

    // Set clear value for new index
    if (ZbcIndex > LastZbcIndex)
    {
        if (LastZbcIndex == MaxClearColors-1)
        {
            Printf(Tee::PriHigh, "Error: All ZBC indices have been used!\n");
            return RC::BAD_PARAMETER;
        }
        LastZbcIndex++;

        // Set clear value
        for (UINT32 Subdev=0; Subdev < Surf.GetGpuDev()->GetNumSubdevices(); Subdev++)
        {
            GpuSubdevice* const pSubdev = Surf.GetGpuSubdev(Subdev);
            pSubdev->RegWr32(LW_PLTCG_LTCS_LTSS_DSTG_ZBC_INDEX, ZbcIndex);
            pSubdev->RegWr32(LW_PLTCG_LTCS_LTSS_DSTG_ZBC_DEPTH_CLEAR_VALUE,
                    ClearValue.clearValue[0]);
            for (int i=0; i < ZBCValue::maxClearValues; i++)
            {
                pSubdev->RegWr32(
                        LW_PLTCG_LTCS_LTSS_DSTG_ZBC_COLOR_CLEAR_VALUE(0)+4*i,
                        ClearValue.clearValue[i]);
                UsedClearColors[LastZbcIndex][i] = ClearValue.clearValue[i];
            }
        }

    }
    Printf(Tee::PriNormal, "ZBC comptaglines %u..%u to [0x%08X, 0x%08X, 0x%08X, 0x%08X] with comprType %u\n",
        MinTag, MaxTag, UsedClearColors[ZbcIndex][0], UsedClearColors[ZbcIndex][1],
        UsedClearColors[ZbcIndex][2], UsedClearColors[ZbcIndex][3], static_cast<unsigned>(ClearValue.comprType));

    // Compute clear value
    const UINT32 Value =
        DRF_NUM(_PLTCG, _LTCS_LTSS_CBC_CLEAR_VALUE, _COMPBITS, ClearValue.comprType | (ClearValue.comprType << 2)) |
        DRF_NUM(_PLTCG, _LTCS_LTSS_CBC_CLEAR_VALUE, _ZBC_INDEX, ZbcIndex);

    // Do it for each subdevice
    for (UINT32 Subdev=0; Subdev < Surf.GetGpuDev()->GetNumSubdevices(); Subdev++)
    {
        // Get configuration
        GpuSubdevice* const pSubdev = Surf.GetGpuSubdev(Subdev);
        const UINT32 numLtcs   = pSubdev->GetFB()->GetLtcCount();
        MASSERT(numLtcs > 0);

        // Initiate clear
        const UINT32 OrigCBCClearValue = pSubdev->RegRd32(LW_PLTCG_LTCS_LTSS_CBC_CLEAR_VALUE);
        pSubdev->RegWr32(LW_PLTCG_LTCS_LTSS_CBC_CLEAR_VALUE, Value);
        pSubdev->RegWr32(LW_PLTCG_LTCS_LTSS_CBC_CTRL_2,
                DRF_NUM(_PLTCG, _LTCS_LTSS_CBC_CTRL_2, _CLEAR_LOWER_BOUND, MinTag));
        pSubdev->RegWr32(LW_PLTCG_LTCS_LTSS_CBC_CTRL_3,
                DRF_NUM(_PLTCG, _LTCS_LTSS_CBC_CTRL_3, _CLEAR_UPPER_BOUND, MaxTag));
        pSubdev->RegWr32(LW_PLTCG_LTCS_LTSS_CBC_CTRL_1,
                DRF_DEF(_PLTCG, _LTCS_LTSS_CBC_CTRL_1, _CLEAR, _ACTIVE));

        // Wait until comptaglines for all LTCs and slices are set
        for (UINT32 ltc = 0; ltc < numLtcs; ++ltc)
        {
            const UINT32 numSlices = pSubdev->GetFB()->GetSlicesPerLtc(ltc);
            MASSERT(numSlices > 0);

            for (UINT32 slice = 0; slice < numSlices; ++slice)
            {
                UINT32 value = 0;
                do
                {
                    Tasker::Yield();
                    value = pSubdev->RegRd32(LW_PLTCG_LTC0_LTS0_CBC_CTRL_1
                            + (ltc * LW_LTC_PRI_STRIDE)
                            + (slice * LW_LTS_PRI_STRIDE));
                } while (LW_PLTCG_LTC0_LTS0_CBC_CTRL_1_CLEAR_ACTIVE
                        == DRF_VAL(_PLTCG, _LTC0_LTS0_CBC_CTRL_1, _CLEAR, value));
            }
        }
        pSubdev->RegWr32(LW_PLTCG_LTCS_LTSS_CBC_CLEAR_VALUE, OrigCBCClearValue);
    }
    return OK;
}

SurfaceUtils::ZBCComprType SurfaceUtils::GetZbcComprType(UINT32 PteKind)
{
    switch (PteKind)
    {
        case LW_MMU_PTE_KIND_C32_2C:
        case LW_MMU_PTE_KIND_C32_MS2_2C:
        case LW_MMU_PTE_KIND_C32_MS4_2C:
        case LW_MMU_PTE_KIND_C32_MS8_MS16_2C:
        case LW_MMU_PTE_KIND_C64_2C:
        case LW_MMU_PTE_KIND_C64_MS2_2C:
        case LW_MMU_PTE_KIND_C64_MS4_2C:
        case LW_MMU_PTE_KIND_C64_MS8_MS16_2C:
        case LW_MMU_PTE_KIND_C128_2C:
        case LW_MMU_PTE_KIND_C128_MS2_2C:
        case LW_MMU_PTE_KIND_C128_MS4_2C:
        case LW_MMU_PTE_KIND_C128_MS8_MS16_2C:
        case LW_MMU_PTE_KIND_C32_2CBR:
        case LW_MMU_PTE_KIND_C32_MS2_2CBR:
        case LW_MMU_PTE_KIND_C32_MS4_2CBR:
        case LW_MMU_PTE_KIND_C64_2CBR:
        case LW_MMU_PTE_KIND_C64_MS2_2CBR:
        case LW_MMU_PTE_KIND_C64_MS4_2CBR:
        case LW_MMU_PTE_KIND_C32_2CBA:
        case LW_MMU_PTE_KIND_C32_MS4_2CBA:
        case LW_MMU_PTE_KIND_C64_2CBA:
        case LW_MMU_PTE_KIND_C64_MS4_2CBA:
        case LW_MMU_PTE_KIND_C32_2CRA:
        case LW_MMU_PTE_KIND_C32_MS2_2CRA:
        case LW_MMU_PTE_KIND_C32_MS4_2CRA:
        case LW_MMU_PTE_KIND_C32_MS8_MS16_2CRA:
        case LW_MMU_PTE_KIND_C64_2CRA:
        case LW_MMU_PTE_KIND_C64_MS2_2CRA:
        case LW_MMU_PTE_KIND_C64_MS4_2CRA:
        case LW_MMU_PTE_KIND_C64_MS8_MS16_2CRA:
        case LW_MMU_PTE_KIND_C128_2CR:
        case LW_MMU_PTE_KIND_C128_MS2_2CR:
        case LW_MMU_PTE_KIND_C128_MS4_2CR:
        case LW_MMU_PTE_KIND_C128_MS8_MS16_2CR:
            return zbc_c1;
        default:
            break;
    }
    return zbc_uncompressed;
}

SurfaceUtils::ZBCComprType SurfaceUtils::GetReductionComprType(UINT32 PteKind)
{
    switch (PteKind)
    {
        case LW_MMU_PTE_KIND_C32_2CBR:
        case LW_MMU_PTE_KIND_C32_MS2_2CBR:
        case LW_MMU_PTE_KIND_C32_MS4_2CBR:
        case LW_MMU_PTE_KIND_C64_2CBR:
        case LW_MMU_PTE_KIND_C64_MS2_2CBR:
        case LW_MMU_PTE_KIND_C64_MS4_2CBR:
            return zbc_c3;
        case LW_MMU_PTE_KIND_C32_2CRA:
        case LW_MMU_PTE_KIND_C32_MS2_2CRA:
        case LW_MMU_PTE_KIND_C32_MS4_2CRA:
        case LW_MMU_PTE_KIND_C32_MS8_MS16_2CRA:
        case LW_MMU_PTE_KIND_C64_2CRA:
        case LW_MMU_PTE_KIND_C64_MS2_2CRA:
        case LW_MMU_PTE_KIND_C64_MS4_2CRA:
        case LW_MMU_PTE_KIND_C64_MS8_MS16_2CRA:
        case LW_MMU_PTE_KIND_C32_2BRA:
        case LW_MMU_PTE_KIND_C32_MS4_2BRA:
        case LW_MMU_PTE_KIND_C64_2BRA:
        case LW_MMU_PTE_KIND_C64_MS4_2BRA:
        case LW_MMU_PTE_KIND_C128_2CR:
        case LW_MMU_PTE_KIND_C128_MS2_2CR:
        case LW_MMU_PTE_KIND_C128_MS4_2CR:
        case LW_MMU_PTE_KIND_C128_MS8_MS16_2CR:
            return zbc_c2;
        default:
            break;
    }
    return zbc_uncompressed;
}
