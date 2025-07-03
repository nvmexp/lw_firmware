/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "zbctable.h"
#include "lwgpu.h"
#include "core/include/tee.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/zbccolwertutil.h"
#include "fermi/gf100/dev_graphics_nobundle.h"
#include "mdiag/tests/gpu/trace_3d/tracesubchan.h"
#include "mdiag/utils/lwgpu_classes.h"

ZbcTable::ZbcTable(LWGpuResource *lwgpu, LwRm* pLwRm)
    : m_Lwgpu(lwgpu),
      m_AllowAutoZbc(true)
{
}

ZbcTable::~ZbcTable()
{
}

// Add individual color, depth, and stencil values to the ZBC tables
// based on user specified command-line arguments.
//
RC ZbcTable::AddArgumentEntries(const ArgReader *params, LwRm* pLwRm)
{
    RC rc;

    GpuSubdevice *gpuSubdevice = m_Lwgpu->GetGpuSubdevice();
    bool skipL2Table = params->ParamPresent("-zbc_skip_ltc_setup");
    UINT32 i;

    for (i = 0; i < params->ParamPresent("-zbc_color_table_entry"); ++i)
    {
        ColorUtils::Format format = ColorUtils::StringToFormat(
            params->ParamNStr("-zbc_color_table_entry", i, 0));
        UINT32 rgba[4];

        for (UINT32 colorIndex = 0; colorIndex < 4; ++colorIndex)
        {
            rgba[colorIndex] = params->ParamNUnsigned("-zbc_color_table_entry", i, colorIndex + 1);
        }
        RGBAFloat value = RGBAFloat(rgba);

        CHECK_RC(AddColorToZbcTable(gpuSubdevice, format, value, pLwRm, skipL2Table));
        m_AllowAutoZbc = false;
    }

    for (i = 0; i < params->ParamPresent("-zbc_depth_table_entry"); ++i)
    {
        ColorUtils::Format format = ColorUtils::StringToFormat(
            params->ParamNStr("-zbc_depth_table_entry", i, 0));
        ZFloat value(params->ParamNUnsigned("-zbc_depth_table_entry", i, 1));

        CHECK_RC(AddDepthToZbcTable(gpuSubdevice, format, value, pLwRm, skipL2Table));
        m_AllowAutoZbc = false;
    }

    for (i = 0; i < params->ParamPresent("-zbc_stencil_table_entry"); ++i)
    {
        ColorUtils::Format format = ColorUtils::StringToFormat(
            params->ParamNStr("-zbc_stencil_table_entry", i, 0));
        Stencil value(params->ParamNUnsigned("-zbc_stencil_table_entry", i, 1));

        CHECK_RC(AddStencilToZbcTable(gpuSubdevice, format, value, pLwRm, skipL2Table));
        m_AllowAutoZbc = false;
    }

    return rc;
}

// Add a value to the ZBC color table.
//
RC ZbcTable::AddColorToZbcTable
(
    GpuSubdevice *gpuSubdevice,
    ColorUtils::Format colorFormat,
    const RGBAFloat &colorClear,
    LwRm* pLwRm,
    bool skipL2Table
)
{
    RC rc;
    ZbcTableHandle zbcHandle(gpuSubdevice, pLwRm);

    LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS colorParams;
    memset(&colorParams, 0, sizeof(colorParams));

    ColwertColor(colorFormat, colorClear, colorParams.colorFB,
        colorParams.colorDS, &colorParams.format);

#ifdef LW_VERIF_FEATURES
    colorParams.bSkipL2Table = skipL2Table;
#endif

    rc = pLwRm->Control(zbcHandle,
        LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR,
        &colorParams,
        sizeof(LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS));

    if (OK != rc)
    {
        ErrPrintf("Couldn't add color value to ZBC table: %s\n", rc.Message());
        return rc;
    }

    return rc;
}

// Colwert the color clear value to the appropriate values that RM
// needs for it's ZBC color clear API.
//
// Note: the code in this function originally resided in the function
// TraceChannel::SetClearTableRegisters in fermi_support.cpp.
//
void ZbcTable::ColwertColor
(
    ColorUtils::Format colorFormat,
    const RGBAFloat &colorClear,
    UINT32 *colorFB,
    UINT32 *colorDS,
    UINT32 *clearFormat
)
{
    *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_ILWALID;
    cARGB origColor;
    bool unormFormat = false;

    origColor.red.ui = colorClear.GetRFloat32();
    origColor.green.ui = colorClear.GetGFloat32();
    origColor.blue.ui = colorClear.GetBFloat32();
    origColor.alpha.ui = colorClear.GetAFloat32();

    bool allOnes =
           (origColor.red.ui == 0x3f800000) &&
           (origColor.green.ui == 0x3f800000) &&
           (origColor.blue.ui == 0x3f800000) &&
           (origColor.alpha.ui == 0x3f800000);

    // Determine the ZBC clear format from the color format.
    // Also, any color channel that is not used in the color format
    // should be set to zero.  This is necessary for determining
    // if LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_ZERO should be used below.
    // Also, mark any unorm formats.
    switch (colorFormat)
    {
      case ColorUtils::A8R8G8B8:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_A8R8G8B8;
        unormFormat = true;
        break;

      case ColorUtils::X8R8G8B8:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_A8R8G8B8;
        origColor.alpha.ui = 0;
        break;

      case ColorUtils::A8B8G8R8:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_A8B8G8R8;
        unormFormat = true;
        break;

      case ColorUtils::X8B8G8R8:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_A8B8G8R8;
        origColor.alpha.ui = 0;
        break;

      case ColorUtils::AN8BN8GN8RN8:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_AN8BN8GN8RN8;
        break;

      case ColorUtils::AS8BS8GS8RS8:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_AS8BS8GS8RS8;
        break;

      case ColorUtils::AU8BU8GU8RU8:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_AU8BU8GU8RU8;
        break;

      case ColorUtils::A8RL8GL8BL8:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_A8RL8GL8BL8;
        unormFormat = true;
        break;

      case ColorUtils::X8RL8GL8BL8:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_A8RL8GL8BL8;
        origColor.alpha.ui = 0;
        break;

      case ColorUtils::A8BL8GL8RL8:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_A8BL8GL8RL8;
        unormFormat = true;
        break;

      case ColorUtils::X8BL8GL8RL8:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_A8BL8GL8RL8;
        origColor.alpha.ui = 0;
        break;

      case ColorUtils::RF32_GF32_BF32_AF32:
      case ColorUtils::RS32_GS32_BS32_AS32:
      case ColorUtils::RU32_GU32_BU32_AU32:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RF32_GF32_BF32_AF32;
        break;

      case ColorUtils::RF32_GF32_BF32_X32:
      case ColorUtils::RS32_GS32_BS32_X32:
      case ColorUtils::RU32_GU32_BU32_X32:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RF32_GF32_BF32_AF32;
        origColor.alpha.ui = 0;
        break;

      case ColorUtils::RF32_GF32:
      case ColorUtils::RS32_GS32:
      case ColorUtils::RU32_GU32:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RF32_GF32_BF32_AF32;
        origColor.blue.ui = origColor.red.ui;
        origColor.alpha.ui = origColor.green.ui;
        break;

      case ColorUtils::RS32:
      case ColorUtils::RU32:
      case ColorUtils::RF32:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RF32_GF32_BF32_AF32;
        origColor.green.ui = origColor.red.ui;
        origColor.blue.ui = origColor.red.ui;
        origColor.alpha.ui = origColor.red.ui;
        break;

      case ColorUtils::R16_G16_B16_A16:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_R16_G16_B16_A16;
        unormFormat = true;
        break;

      case ColorUtils::R16_G16:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_R16_G16_B16_A16;
        if ((origColor.red.ui == 0x3f800000) &&
                (origColor.green.ui == 0x3f800000))
        {
            allOnes = true;
            origColor.blue.ui = 0x3f800000;
            origColor.alpha.ui = 0x3f800000;
        }
        else
        {
            origColor.blue.ui = origColor.red.ui;
            origColor.alpha.ui = origColor.green.ui;
        }
        unormFormat = true;
        break;

      case ColorUtils::RN16_GN16_BN16_AN16:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RN16_GN16_BN16_AN16;
        break;

      case ColorUtils::RN16_GN16:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RN16_GN16_BN16_AN16;
        origColor.blue.ui = origColor.red.ui;
        origColor.alpha.ui = origColor.green.ui;
        break;

      case ColorUtils::RU16_GU16_BU16_AU16:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RU16_GU16_BU16_AU16;
        break;

      case ColorUtils::RU16_GU16:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RU16_GU16_BU16_AU16;
        origColor.blue.ui = origColor.red.ui;
        origColor.alpha.ui = origColor.green.ui;
        break;

      case ColorUtils::RS16_GS16_BS16_AS16:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RS16_GS16_BS16_AS16;
        break;

      case ColorUtils::RS16_GS16:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RS16_GS16_BS16_AS16;
        origColor.blue.ui = origColor.red.ui;
        origColor.alpha.ui = origColor.green.ui;
        break;

      case ColorUtils::RF16_GF16_BF16_AF16:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RF16_GF16_BF16_AF16;
        break;

      case ColorUtils::RF16_GF16_BF16_X16:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RF16_GF16_BF16_AF16;
        origColor.alpha.ui = 0;
        break;

      case ColorUtils::RF16_GF16:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_RF16_GF16_BF16_AF16;
        origColor.blue.ui = origColor.red.ui;
        origColor.alpha.ui = origColor.green.ui;
        break;

      case ColorUtils::A2B10G10R10:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_A2B10G10R10;
        unormFormat = true;
        break;

      case ColorUtils::AU2BU10GU10RU10:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_AU2BU10GU10RU10;
        break;

      case ColorUtils::A2R10G10B10:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_A2R10G10B10;
        unormFormat = true;
        break;

      case ColorUtils::BF10GF11RF11:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_BF10GF11RF11;
        break;

      default:
        *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_ILWALID;
        break;
    }

    if (*clearFormat != LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_ILWALID)
    {
        // If all the colors channels are zero (or are unused), the Fermi
        // hardware requires the special ZERO ZBC format to be used.
        if ((origColor.red.ui == 0) &&
            (origColor.green.ui == 0) &&
            (origColor.blue.ui == 0) &&
            (origColor.alpha.ui == 0))
        {
            *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_ZERO;
        }

        // If the color format is a unorm format and if the color represents
        // the color white, the Fermi hardware requires the special
        // UNORM_ONE ZBC format to be used.
        else if (unormFormat && allOnes)
        {
            *clearFormat = LW_PGRAPH_PRI_DS_ZBC_COLOR_FMT_VAL_UNORM_ONE;
        }
    }

    zbcColorColwertUtil colwertUtil;
    cARGB colwertedColor;

    colwertUtil.ColorColwertToFB(origColor, *clearFormat, &colwertedColor);
    colorFB[0] = colwertedColor.cout[0].ui;
    colorFB[1] = colwertedColor.cout[1].ui;
    colorFB[2] = colwertedColor.cout[2].ui;
    colorFB[3] = colwertedColor.cout[3].ui;
    colorDS[0] = origColor.red.ui;
    colorDS[1] = origColor.green.ui;
    colorDS[2] = origColor.blue.ui;
    colorDS[3] = origColor.alpha.ui;
}

// Add a value to the ZBC depth table.
//
RC ZbcTable::AddDepthToZbcTable
(
    GpuSubdevice *gpuSubdevice,
    ColorUtils::Format zetaFormat,
    const ZFloat &depthClear,
    LwRm* pLwRm,
    bool skipL2Table
)
{
    RC rc;
    ZbcTableHandle zbcHandle(gpuSubdevice, pLwRm);

    CHECK_RC(ReadZbcDepthTable(zbcHandle, pLwRm));

    // Some Z buffer formats require that the corresponding depth value be
    // placed at or below a specific index in the table.
    UINT32 maxIndex = GetMaxDepthIndex(zetaFormat);

    bool emptySpace = false;
    UINT32 depthValue = depthClear.GetFloat();
    UINT32 depthFormat = LW9096_CTRL_CMD_SET_ZBC_DEPTH_CLEAR_FMT_FP32;

    // RM doesn't know about Z buffer formats and has no way to restrict the
    // index of a ZBC depth table entry.  In order to get around this issue,
    // the code below will check for unused depth entries in the ZBC table.
    // If there is an unused depth entry at or below the maximum legal index
    // for the current depth format/value, then it is okay to call RM as it
    // will always choose the lowest indices first.  This function will also
    // immediately return if the depth value already exists in the table.
    for (auto tableEntry : m_DepthTable)
    {
        if (tableEntry.first <= maxIndex)
        {
            if (tableEntry.second.bIndexValid)
            {
                if ((tableEntry.second.value.depth == depthValue) &&
                    (tableEntry.second.format == depthFormat))
                {
                    // The value is already in the table, so just return.
                    return rc;
                }
            }
            else
            {
                emptySpace = true;
            }
        }
    }

    if (!emptySpace)
    {
        ErrPrintf("No room in ZBC depth table for value 0x%x\n", depthValue);
        return RC::SOFTWARE_ERROR;
    }

    LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS setParams = { 0 };

    setParams.depth = depthValue;
    setParams.format = depthFormat;
#ifdef LW_VERIF_FEATURES
    setParams.bSkipL2Table = skipL2Table;
#endif

    rc = pLwRm->Control(zbcHandle,
        LW9096_CTRL_CMD_SET_ZBC_DEPTH_CLEAR,
        &setParams,
        sizeof(LW9096_CTRL_SET_ZBC_DEPTH_CLEAR_PARAMS));

    if (OK != rc)
    {
        ErrPrintf("Couldn't add depth value to ZBC table: %s\n", rc.Message());
        return rc;
    }

    return rc;
}

// Add a value to the ZBC stencil table.
//
RC ZbcTable::AddStencilToZbcTable
(
    GpuSubdevice *gpuSubdevice,
    ColorUtils::Format zetaFormat,
    const Stencil &stencilClear,
    LwRm* pLwRm,
    bool skipL2Table
)
{
    RC rc;
    ZbcTableHandle zbcHandle(gpuSubdevice, pLwRm);

    CHECK_RC(ReadZbcStencilTable(zbcHandle, pLwRm));

    // Some Z buffer formats require that the corresponding stencil value be
    // placed at or below a specific index in the table.
    UINT32 maxIndex = GetMaxStencilIndex(zetaFormat);    

    bool emptySpace = false;
    UINT32 stencilValue = stencilClear.Get();
    UINT32 stencilFormat = LW9096_CTRL_CMD_SET_ZBC_STENCIL_CLEAR_FMT_U8;

    // RM doesn't know about Z buffer formats and has no way to restrict the
    // index of a ZBC stencil table entry.  In order to get around this issue,
    // the code below will check for unused stencil entries in the ZBC table.
    // If there is an unused stencil entry at or below the maximum legal index
    // for the current stencil format/value, then it is okay to call RM as it
    // will always choose the lowest indices first.  This function will also
    // immediately return if the stencil value already exists in the table.
    for (auto tableEntry : m_StencilTable)
    {
        if (tableEntry.first <= maxIndex)
        {
            if (tableEntry.second.bIndexValid)
            {
                if ((tableEntry.second.value.stencil == stencilValue) &&
                    (tableEntry.second.format == stencilFormat))
                {
                    // The value is already in the table, so just return.
                    return rc;
                }
            }
            else
            {
                emptySpace = true;
            }
        }
    }

    if (!emptySpace)
    {
        ErrPrintf("No room in ZBC stencil table for value 0x%x\n", stencilValue);
        return RC::SOFTWARE_ERROR;
    }

    LW9096_CTRL_SET_ZBC_STENCIL_CLEAR_PARAMS setParams = { 0 };

    setParams.stencil = stencilValue;
    setParams.format = stencilFormat;
#ifdef LW_VERIF_FEATURES
    setParams.bSkipL2Table = skipL2Table;
#endif

    rc = pLwRm->Control(zbcHandle,
        LW9096_CTRL_CMD_SET_ZBC_STENCIL_CLEAR,
        &setParams,
        sizeof(LW9096_CTRL_SET_ZBC_STENCIL_CLEAR_PARAMS));

    if (OK != rc)
    {
        ErrPrintf("Couldn't add stencil value to ZBC table: %s\n", rc.Message());
        return rc;
    }

    return rc;
}

RC ZbcTable::GetTableIndex
(
    const ZbcTableHandle &zbcHandle,
    LwRm* pLwRm,
    LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE type,
    UINT32 *indexStart,
    UINT32 *indexEnd
)
{
    RC rc;

    LW9096_CTRL_GET_ZBC_CLEAR_TABLE_SIZE_PARAMS indexParams;
    memset(&indexParams, 0, sizeof(indexParams));

    indexParams.tableType = type;

    CHECK_RC(pLwRm->Control(zbcHandle,
        LW9096_CTRL_CMD_GET_ZBC_CLEAR_TABLE_SIZE,
        &indexParams,
        sizeof(LW9096_CTRL_GET_ZBC_CLEAR_TABLE_SIZE_PARAMS)));

    *indexStart = indexParams.indexStart;
    *indexEnd = indexParams.indexEnd;

    return rc;
}

// When the GPU is clearing a Z buffer, there are some Z buffer formats
// that limit which ZBC depth table entries will be allowed.
//
UINT32 ZbcTable::GetMaxDepthIndex
(
    ColorUtils::Format zetaFormat
)
{
    UINT32 maxIndex = m_DepthTableIndex.indexEnd;

    // If the Z format is Z16, only index 1 and 2 can be used.
    if (zetaFormat == ColorUtils::Z16)
    {
        maxIndex = 2;
    }

    // For GP10X and later, if the Z format is Z24S8, only index 1 and 2
    // can be used.
    else if (EngineClasses::IsGpuFamilyClassOrLater(
                TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_B) &&
        ((zetaFormat == ColorUtils::Z24S8) ||
         (zetaFormat == ColorUtils::S8Z24)))
    {
        maxIndex = 2;
    }

    return maxIndex;
}

// When the GPU is clearing a stencil buffer, there are some Z buffer formats
// that limit which ZBC stencil table entries will be allowed.
//
UINT32 ZbcTable::GetMaxStencilIndex
(
    ColorUtils::Format zetaFormat
)
{
    UINT32 maxIndex = m_StencilTableIndex.indexEnd;

    // For GP10X and later, if the stencil format is one of Z24S8, S8Z24,
    // or ZF32_X24S8, only indices 1 through 3 can be used.
    if (EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_B) &&
        ((zetaFormat == ColorUtils::Z24S8) ||
         (zetaFormat == ColorUtils::S8Z24) ||
         (zetaFormat == ColorUtils::ZF32_X24S8)))
    {
        maxIndex = 3;
    }

    return maxIndex;
}

// This function reads all entries of the ZBC color table.
//
RC ZbcTable::ReadZbcColorTable(const ZbcTableHandle &zbcHandle, LwRm* pLwRm)
{
    RC rc;

    CHECK_RC(GetTableIndex(zbcHandle, pLwRm, LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_COLOR,
        &(m_ColorTableIndex.indexStart), &(m_ColorTableIndex.indexEnd)));

    for (UINT32 index = m_ColorTableIndex.indexStart; index <= m_ColorTableIndex.indexEnd; ++index)
    {
        memset(&m_ColorTable[index], 0, sizeof(m_ColorTable[index]));

        m_ColorTable[index].index = index;
        m_ColorTable[index].tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_COLOR;

        rc = pLwRm->Control(zbcHandle,
            LW9096_CTRL_CMD_GET_ZBC_CLEAR_TABLE_ENTRY,
            &m_ColorTable[index],
            sizeof(LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS));

        if (OK != rc)
        {
            ErrPrintf("Can't get ZBC color table info for entry %u: %s\n",
                index, rc.Message());
            return rc;
        }
    }

    return rc;
}

// This function reads all entries of the ZBC depth table.
//
RC ZbcTable::ReadZbcDepthTable(const ZbcTableHandle &zbcHandle, LwRm* pLwRm)
{
    RC rc;

    CHECK_RC(GetTableIndex(zbcHandle, pLwRm, LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_DEPTH,
        &(m_DepthTableIndex.indexStart), &(m_DepthTableIndex.indexEnd)));
            
    for (UINT32 index = m_DepthTableIndex.indexStart; index <= m_DepthTableIndex.indexEnd; ++index)
    {
        memset(&m_DepthTable[index], 0, sizeof(m_DepthTable[index]));

        m_DepthTable[index].index = index;
        m_DepthTable[index].tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_DEPTH;

        rc = pLwRm->Control(zbcHandle,
            LW9096_CTRL_CMD_GET_ZBC_CLEAR_TABLE_ENTRY,
            &m_DepthTable[index],
            sizeof(LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS));

        if (OK != rc)
        {
            ErrPrintf("Can't get ZBC depth table info for entry %u: %s\n",
                index, rc.Message());
            return rc;
        }
    }

    return rc;
}

// This function reads all entries of the ZBC stencil table.
//
RC ZbcTable::ReadZbcStencilTable(const ZbcTableHandle &zbcHandle, LwRm* pLwRm)
{
    RC rc;

    CHECK_RC(GetTableIndex(zbcHandle, pLwRm, LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_STENCIL,
        &(m_StencilTableIndex.indexStart), &(m_StencilTableIndex.indexEnd)));

    for (UINT32 index = m_StencilTableIndex.indexStart; index <= m_StencilTableIndex.indexEnd; ++index)
    {
        memset(&m_StencilTable[index], 0, sizeof(m_StencilTable[index]));

        m_StencilTable[index].index = index;
        m_StencilTable[index].tableType = LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE_STENCIL;

        rc = pLwRm->Control(zbcHandle,
            LW9096_CTRL_CMD_GET_ZBC_CLEAR_TABLE_ENTRY,
            &m_StencilTable[index],
            sizeof(LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS));

        if (OK != rc)
        {
            ErrPrintf("Can't get ZBC stencil table info for entry %u: %s\n",
                index, rc.Message());
            return rc;
        }
    }

    return rc;
}

// Clear all of the entries of the ZBC table.  This function assumes
// that there are no ZBC surfaces allocated at the time it is called.
//
void ZbcTable::ClearTables(GpuSubdevice *gpuSubdevice, LwRm* pLwRm)
{
    // There isn't an RM API to clear the ZBC table, but RM will clear
    // the ZBC table when freeing the ZBC object if there are no
    // lwrrently allocated surfaces with the ZBC attribute.
    // Instantiating the ZBC handle will allocate and free the ZBC object.
    ZbcTableHandle zbcHandle(gpuSubdevice, pLwRm);
}

// This function is for debugging only and should not be called by
// production code.
//
RC ZbcTable::PrintZbcTable(GpuSubdevice *gpuSubdevice, LwRm* pLwRm)
{
    RC rc;
    ZbcTableHandle zbcHandle(gpuSubdevice, pLwRm);

    CHECK_RC(ReadZbcColorTable(zbcHandle, pLwRm));
    CHECK_RC(ReadZbcDepthTable(zbcHandle, pLwRm));
    CHECK_RC(ReadZbcStencilTable(zbcHandle, pLwRm));

    for (auto tableEntry : m_ColorTable)
    {
        if (tableEntry.second.bIndexValid)
        {
            InfoPrintf("m_ColorTable[%u].index = %u\n", tableEntry.first,
                tableEntry.second.index);
            InfoPrintf("m_ColorTable[%u].format = 0x%x\n", tableEntry.first,
                tableEntry.second.format);
            InfoPrintf("m_ColorTable[%u].tableType = %u\n", tableEntry.first,
                tableEntry.second.tableType);

            for (UINT32 colorIndex = 0;
                 colorIndex < LW9096_CTRL_SET_ZBC_COLOR_CLEAR_VALUE_SIZE;
                 ++colorIndex)
            {
                InfoPrintf("m_ColorTable[%u].value.colorFB[%u] = 0x%08x\n",
                    tableEntry.first, colorIndex,
                    tableEntry.second.value.colorFB[colorIndex]);

                InfoPrintf("m_ColorTable[%u].value.colorDS[%u] = 0x%08x\n",
                    tableEntry.first, colorIndex,
                    tableEntry.second.value.colorDS[colorIndex]);
            }
        }
    }

    for (auto tableEntry : m_DepthTable)
    {
        if (tableEntry.second.bIndexValid)
        {
            InfoPrintf("m_DepthTable[%u].index = %u\n", tableEntry.first,
                tableEntry.second.index);
            InfoPrintf("m_DepthTable[%u].format = 0x%x\n", tableEntry.first,
                tableEntry.second.format);
            InfoPrintf("m_DepthTable[%u].tableType = %u\n", tableEntry.first,
                tableEntry.second.tableType);
            InfoPrintf("m_DepthTable[%u].value.depth = 0x%08x\n", tableEntry.first,
                tableEntry.second.value.depth);
        }
    }

    for (auto tableEntry : m_StencilTable)
    {
        if (tableEntry.second.bIndexValid)
        {
            InfoPrintf("m_StencilTable[%u].index = %u\n", tableEntry.first,
                tableEntry.second.index);
            InfoPrintf("m_StencilTable[%u].format = 0x%x\n", tableEntry.first,
                tableEntry.second.format);
            InfoPrintf("m_StencilTable[%u].tableType = %u\n", tableEntry.first,
                tableEntry.second.tableType);
            InfoPrintf("m_StencilTable[%u].value.stencil = 0x%08x\n", tableEntry.first,
                tableEntry.second.value.stencil);
        }
    }

    return rc;
}

// Allocate the ZBC object and save the handle.
// Care must be taken to not to have more than one of these active at a time.
// (In the future, reference counting could be added.)
//
ZbcTable::ZbcTableHandle::ZbcTableHandle(GpuSubdevice *gpuSubdevice, LwRm *pLwRm)
{
    m_pLwRm = pLwRm;
    LwRm::Handle subdevHandle = m_pLwRm->GetSubdeviceHandle(gpuSubdevice);
    RC rc = m_pLwRm->Alloc(subdevHandle, &m_Handle, GF100_ZBC_CLEAR, 0);
    if (OK != rc)
    {
        MASSERT(!"Can't allocate GF100_ZBC_CLEAR.");
    }
}

// Free the ZBC object.
ZbcTable::ZbcTableHandle::~ZbcTableHandle()
{
    m_pLwRm->Free(m_Handle);
}
