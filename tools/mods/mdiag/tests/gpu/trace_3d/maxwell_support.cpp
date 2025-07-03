/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"
#include "class/clb097.h" // MAXWELL_A
#include "class/clb197.h" // MAXWELL_B
#include "class/clc097.h" // PASCAL_A
#include "teegroups.h"
#include "maxwell/gm20b/dev_ltc.h"

#define MSGID() T3D_MSGID(ChipSupport)

RC TraceChannel::SetupMaxwellA(LWGpuSubChannel* pSubch)
{
    RC rc = OK;

    CHECK_RC(SetupKepler(pSubch));

    DebugPrintf("Setting up initial MaxwellA Graphics on gpu %d:0\n",
               m_Test->GetBoundGpuDevice()->GetDeviceInst());

    if (!m_bUsePascalPagePool && params->ParamPresent("-pagepool_size"))
    {
        // GPCS_SWDX_RM_PAGEPOOL should be programmed whenever SCC_RM_PAGEPOOL is programmed
        UINT32 poolSize = params->ParamUnsigned("-pagepool_size");
        UINT32 poolTotal, poolMax;
        UINT32 i, data, mask;
        RegHal& regHal = m_Test->GetGpuResource()->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, GetLwRmPtr(), GetSmcEngine());

        poolTotal = regHal.GetField(poolSize, MODS_PGRAPH_PRI_GPCS_SWDX_RM_PAGEPOOL_TOTAL_PAGES);
        poolMax = regHal.GetField(poolSize, MODS_PGRAPH_PRI_GPCS_SWDX_RM_PAGEPOOL_MAX_VALID_PAGES);

        // SWDX takes TOTAL_PAGES and MAX_VALID_PAGES
        data = regHal.SetField(MODS_PGRAPH_PRI_GPCS_SWDX_RM_PAGEPOOL_TOTAL_PAGES, poolTotal) |
               regHal.SetField(MODS_PGRAPH_PRI_GPCS_SWDX_RM_PAGEPOOL_MAX_VALID_PAGES, poolMax);
        mask = regHal.LookupMask(MODS_PGRAPH_PRI_GPCS_SWDX_RM_PAGEPOOL_TOTAL_PAGES) |
               regHal.LookupMask(MODS_PGRAPH_PRI_GPCS_SWDX_RM_PAGEPOOL_MAX_VALID_PAGES);
        CHECK_RC(pSubch->MethodWriteRC(LWB097_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWB097_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LWB097_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(pSubch->MethodWriteRC(LWB097_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(pSubch->MethodWriteRC(LWB097_SET_FALCON04, regHal.LookupAddress(MODS_PGRAPH_PRI_GPCS_SWDX_RM_PAGEPOOL)));
        for(i=0; i<10; i++) CHECK_RC(pSubch->MethodWriteRC(LWB097_NO_OPERATION, 0));
    }

    // Normally the attribute cirlwlar buffer is allocated by RM, but RM doesn't allocate the buffer
    // for Amodel because Amodel did not need to handle the primitive cirlwlar buffer in the past.
    // Due to changes in Maxwell, Amodel now needs to know the location and the size of the buffer,
    // but adding the allocation to RM was deeemed to be too much work.  Instead it was decided that
    // trace3d would allocate the buffer for Amodel and pass the address and size via escape-write.
    if ((Platform::GetSimulationMode() == Platform::Amodel) && !m_AmodelCirlwlarBuffer.IsAllocated())
    {
        UINT32 bufferSize = params->ParamUnsigned("-amodel_cirlwlar_buffer_size", 0x20000);

        m_AmodelCirlwlarBuffer.SetArrayPitch(bufferSize);
        m_AmodelCirlwlarBuffer.SetLocation(Memory::Fb);
        m_AmodelCirlwlarBuffer.SetColorFormat(ColorUtils::Y8);
        if(this->params->ParamPresent("-va_reverse"))
        {
            m_AmodelCirlwlarBuffer.SetVaReverse(true);
        }
        m_AmodelCirlwlarBuffer.SetGpuVASpace(GetVASpaceHandle());
        m_AmodelCirlwlarBuffer.Alloc(m_Test->GetBoundGpuDevice(), GetLwRmPtr());

        m_Test->m_BuffInfo.SetDmaBuff("AmodelCirlwlarBuffer", m_AmodelCirlwlarBuffer, 0, 0);

        UINT64 virtualAddress = m_AmodelCirlwlarBuffer.GetCtxDmaOffsetGpu();
        UINT32 writeDataSize = sizeof(bufferSize) + sizeof(virtualAddress);
        if (m_Test->GetGpuResource()->IsMultiVasSupported())
        {
            //channel id
            writeDataSize += sizeof(LwU32);
        }
        vector<UINT08> writeBuffer(writeDataSize);
        UINT32 i;
        UINT32 byteIndex = 0;

        for (i = 0; i < sizeof(virtualAddress); ++i)
        {
            writeBuffer[byteIndex++] = (UINT08)((virtualAddress >> (8 * i)) & 0xff);
        }
        for (i = 0; i < sizeof(bufferSize); ++i)
        {
            writeBuffer[byteIndex++] = (UINT08)((bufferSize >> (8 * i)) & 0xff);
        }
        if (m_Test->GetGpuResource()->IsMultiVasSupported())
        {
            UINT32 chId = GetCh()->ChannelNum();
            for (i = 0; i < sizeof(UINT32); ++i)
            {
                writeBuffer[byteIndex++] = (UINT08)((chId >> (8 * i)) & 0xff);
            }
        }

        MASSERT(byteIndex == writeDataSize);

        m_Test->GetBoundGpuDevice()->EscapeWriteBuffer("AmodelCirlwlarBuffer",  0, writeDataSize, (const void *)&writeBuffer[0]);
    }

    DebugPrintf("Done setting up initial MaxwellA Graphics state\n");
    return rc;
}

RC TraceChannel::SetupMaxwellB(LWGpuSubChannel* pSubch)
{
    RC rc;

    DebugPrintf("Setting up initial MaxwellB Graphics on gpu %d:0\n",
        m_Test->GetBoundGpuDevice()->GetDeviceInst());

    CHECK_RC(SetupMaxwellA(pSubch));

    if (params->ParamPresent("-enable_tir"))
    {
        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_TIR, DRF_DEF(B197, _SET_TIR, _MODE, _RASTER_N_TARGET_M)));
    }

    if (params->ParamPresent("-tirrzmode"))
    {
        UINT32 mode = params->ParamUnsigned("-tirrzmode");
        UINT32 data;

        switch (mode)
        {
            case 0:
                data = DRF_DEF(B197, _SET_ANTI_ALIAS_RASTER, _SAMPLES, _MODE_1X1);
                break;
            case 1:
                data = DRF_DEF(B197, _SET_ANTI_ALIAS_RASTER, _SAMPLES, _MODE_2X1_D3D);
                break;
            case 2:
                data = DRF_DEF(B197, _SET_ANTI_ALIAS_RASTER, _SAMPLES, _MODE_4X2_D3D);
                break;
            case 3:
                data = DRF_DEF(B197, _SET_ANTI_ALIAS_RASTER, _SAMPLES, _MODE_2X2);
                break;
            case 4:
                data = DRF_DEF(B197, _SET_ANTI_ALIAS_RASTER, _SAMPLES, _MODE_4X4);
                break;
            default:
                ErrPrintf("Illegal -tirrzmode value '%u'\n", mode);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_ANTI_ALIAS_RASTER, data));
    }

    if (params->ParamPresent("-tir_modulation"))
    {
        UINT32 mode = params->ParamUnsigned("-tir_modulation");
        UINT32 data;

        switch (mode)
        {
            case 0:
                data = DRF_DEF(B197, _SET_TIR_MODULATION, _COMPONENT_SELECT, _NO_MODULATION);
                break;
            case 1:
                data = DRF_DEF(B197, _SET_TIR_MODULATION, _COMPONENT_SELECT, _MODULATE_RGB);
                break;
            case 2:
                data = DRF_DEF(B197, _SET_TIR_MODULATION, _COMPONENT_SELECT, _MODULATE_ALPHA_ONLY);
                break;
            case 3:
                data = DRF_DEF(B197, _SET_TIR_MODULATION, _COMPONENT_SELECT, _MODULATE_RGBA);
                break;
            default:
                ErrPrintf("Illegal -tir_modulation value '%u'\n", mode);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_TIR_MODULATION, data));
    }

    if (params->ParamPresent("-tir_modulation_table"))
    {
        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_TIR_MODULATION_COEFFICIENT_TABLE(0), params->ParamNUnsigned("-tir_modulation_table", 0, 0)));
        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_TIR_MODULATION_COEFFICIENT_TABLE(1), params->ParamNUnsigned("-tir_modulation_table", 0, 1)));
        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_TIR_MODULATION_COEFFICIENT_TABLE(2), params->ParamNUnsigned("-tir_modulation_table", 0, 2)));
        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_TIR_MODULATION_COEFFICIENT_TABLE(3), params->ParamNUnsigned("-tir_modulation_table", 0, 3)));
    }

    // The following methods should only be sent for older traces (i.e. ones that don't use the ALLOC_SURFACE trace header command)
    // or if the command-line argument -sent_rt_methods was specified.
    if (!m_Test->GetAllocSurfaceCommandPresent() || params->ParamPresent("-send_rt_methods"))
    {
        for (UINT32 i = 0; i < 4; ++i)
        {
            UINT32 data = GetProgrammableSampleData(m_Test->surfZ, i, m_Test->GetParam());
            CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_ANTI_ALIAS_SAMPLE_POSITIONS(i), data));
        }
    }

    if ((params->ParamPresent("-map_region")) ||
        (params->ParamPresent("-map_regionZ")) ||
        (params->ParamPresent("-inflate_rendertarget_and_offset_viewport")) ||
        (params->ParamPresent("-inflate_rendertarget_and_offset_window")))
    {
        UINT32 data = DRF_DEF(B197, _SET_ZT_SPARSE, _ENABLE, _TRUE);

        if (params->ParamPresent("-zt_sparse_fail_always"))
        {
            data = FLD_SET_DRF(B197, _SET_ZT_SPARSE, _UNMAPPED_COMPARE,
                _ZT_SPARSE_FAIL_ALWAYS, data);
        }
        else
        {
            data = FLD_SET_DRF(B197, _SET_ZT_SPARSE, _UNMAPPED_COMPARE,
                _ZT_SPARSE_UNMAPPED_0, data);
        }

        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_ZT_SPARSE, data));
    }

    if (params->ParamPresent("-vdc_2_to_1"))
    {
        InfoPrintf("Setting VDC to 2:1.\n");

        //ROP PRI definitions are changed since Ampere, now read from maunal file first
        UINT32 data{0}, mask{0}, regAddr{0};
        auto lwgpu = GetGpuResource();
        RegHal& regHal = lwgpu->GetRegHal(Gpu::UNSPECIFIED_SUBDEV, GetLwRmPtr(), GetSmcEngine());
        if (0 == lwgpu->GetRegNum("LW_PGRAPH_PRI_GPCS_ROPS_CROP_DEBUG3", &regAddr))
        {
            auto reg = lwgpu->GetRegister("LW_PGRAPH_PRI_GPCS_ROPS_CROP_DEBUG3");
            auto field = reg->FindField("LW_PGRAPH_PRI_GPCS_ROPS_CROP_DEBUG3_COMP_VDC_4TO2_DISABLE");
            lwgpu->GetRegFldDef("LW_PGRAPH_PRI_GPCS_ROPS_CROP_DEBUG3", "_COMP_VDC_4TO2_DISABLE", "_FORCE", &data);
            data <<= field->GetStartBit();
            mask = field->GetMask();
        }
        else if (regHal.IsSupported(MODS_PGRAPH_PRI_BES_CROP_DEBUG3))
        {
             data = regHal.SetField(MODS_PGRAPH_PRI_BES_CROP_DEBUG3_COMP_VDC_4TO2_DISABLE_FORCE);
             mask = regHal.LookupMask(MODS_PGRAPH_PRI_BES_CROP_DEBUG3_COMP_VDC_4TO2_DISABLE);
             regAddr = regHal.LookupAddress(MODS_PGRAPH_PRI_BES_CROP_DEBUG3);
        }

        if (regAddr != 0)
        {
            CHECK_RC(pSubch->MethodWriteRC(LWB197_WAIT_FOR_IDLE, 0));
            CHECK_RC(pSubch->MethodWriteRC(LWB197_WAIT_FOR_IDLE, 0));
            CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_MME_SHADOW_SCRATCH(0), 0));
            CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_MME_SHADOW_SCRATCH(1), data));
            CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_MME_SHADOW_SCRATCH(2), mask));
            CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_FALCON04, regAddr));
            for (UINT32 i = 0; i < 10; i++)
            {
                CHECK_RC(pSubch->MethodWriteRC(LWB197_NO_OPERATION, 0));
            }
        }

        // (LW_PLTCG_LTCS_LTSS_DSTG_CFG0
        data = DRF_DEF(_PLTCG, _LTCS_LTSS_DSTG_CFG0, _VDC_4TO2_DISABLE, _FORCE);
        mask = DRF_SHIFTMASK(LW_PLTCG_LTCS_LTSS_DSTG_CFG0_VDC_4TO2_DISABLE);
        CHECK_RC(pSubch->MethodWriteRC(LWB197_WAIT_FOR_IDLE, 0));
        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_MME_SHADOW_SCRATCH(0), 0));
        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_MME_SHADOW_SCRATCH(1), data));
        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_MME_SHADOW_SCRATCH(2), mask));
        CHECK_RC(pSubch->MethodWriteRC(LWB197_SET_FALCON04, LW_PLTCG_LTCS_LTSS_DSTG_CFG0));
        for(UINT32 i=0; i<10; i++) CHECK_RC(pSubch->MethodWriteRC(LWB197_NO_OPERATION, 0));
    }

    DebugPrintf("Done setting up initial MaxwellB Graphics state\n");

    return rc;
}

UINT32 GenericTraceModule::MassageMaxwellBMethod(UINT32 subdev, UINT32 method, UINT32 data)
{
    UINT32 realMethod = method << 2;

    switch (realMethod)
    {
        case LWB197_SET_TIR:
            if (params->ParamPresent("-enable_tir"))
            {
                data = FLD_SET_DRF(B197, _SET_TIR, _MODE, _RASTER_N_TARGET_M,
                                   data);
            }
            break;

        case LWB197_SET_ANTI_ALIAS_RASTER:
            if (params->ParamPresent("-tirrzmode"))
            {
                UINT32 mode = params->ParamUnsigned("-tirrzmode");

                switch (mode)
                {
                    case 0:
                        data = FLD_SET_DRF(B197, _SET_ANTI_ALIAS_RASTER,
                                           _SAMPLES, _MODE_1X1, data);
                        break;
                    case 1:
                        data = FLD_SET_DRF(B197, _SET_ANTI_ALIAS_RASTER,
                                           _SAMPLES, _MODE_2X1_D3D, data);
                        break;
                    case 2:
                        data = FLD_SET_DRF(B197, _SET_ANTI_ALIAS_RASTER,
                                           _SAMPLES, _MODE_4X2_D3D, data);
                        break;
                    case 3:
                        data = FLD_SET_DRF(B197, _SET_ANTI_ALIAS_RASTER,
                                           _SAMPLES, _MODE_2X2, data);
                        break;
                    case 4:
                        data = FLD_SET_DRF(B197, _SET_ANTI_ALIAS_RASTER,
                                           _SAMPLES, _MODE_4X4, data);
                        break;
                    default:
                        ErrPrintf("Illegal -tirrzmode '%u'\n", mode);
                        MASSERT(0);
                        break;
                }
            }
            break;

        case LWB197_SET_TIR_MODULATION:
            if (params->ParamPresent("-tir_modulation"))
            {
                UINT32 mode = params->ParamUnsigned("-tir_modulation");

                switch (mode)
                {
                    case 0:
                        data = FLD_SET_DRF(B197, _SET_TIR_MODULATION,
                                           _COMPONENT_SELECT, _NO_MODULATION,
                                           data);
                        break;
                    case 1:
                        data = FLD_SET_DRF(B197, _SET_TIR_MODULATION,
                                           _COMPONENT_SELECT, _MODULATE_RGB,
                                           data);
                        break;
                    case 2:
                        data = FLD_SET_DRF(B197, _SET_TIR_MODULATION,
                                           _COMPONENT_SELECT,
                                           _MODULATE_ALPHA_ONLY, data);
                        break;
                    case 3:
                        data = FLD_SET_DRF(B197, _SET_TIR_MODULATION,
                                           _COMPONENT_SELECT, _MODULATE_RGBA,
                                           data);
                        break;
                    default:
                        ErrPrintf("Illegal -tir_modulation value '%u'\n", mode);
                        MASSERT(0);
                        break;
                }
            }
            break;

        case LWB197_SET_TIR_MODULATION_COEFFICIENT_TABLE(0):
            if (params->ParamPresent("-tir_modulation_table"))
            {
                data = params->ParamNUnsigned("-tir_modulation_table", 0, 0);
            }
            break;

        case LWB197_SET_TIR_MODULATION_COEFFICIENT_TABLE(1):
            if (params->ParamPresent("-tir_modulation_table"))
            {
                data = params->ParamNUnsigned("-tir_modulation_table", 0, 1);
            }
            break;

        case LWB197_SET_TIR_MODULATION_COEFFICIENT_TABLE(2):
            if (params->ParamPresent("-tir_modulation_table"))
            {
                data = params->ParamNUnsigned("-tir_modulation_table", 0, 2);
            }
            break;

        case LWB197_SET_TIR_MODULATION_COEFFICIENT_TABLE(3):
            if (params->ParamPresent("-tir_modulation_table"))
            {
                data = params->ParamNUnsigned("-tir_modulation_table", 0, 3);
            }
            break;

        case LWB197_SET_ZT_SPARSE:
            if ((params->ParamPresent("-map_region")) ||
                (params->ParamPresent("-map_regionZ")))
            {
                data = FLD_SET_DRF(B197, _SET_ZT_SPARSE, _ENABLE, _TRUE, data);
            }
            if (params->ParamPresent("-zt_sparse_fail_always"))
            {
                data = FLD_SET_DRF(B197, _SET_ZT_SPARSE, _UNMAPPED_COMPARE,
                                   _ZT_SPARSE_FAIL_ALWAYS, data);
            }
            break;

        default:
            return MassageFermiMethod(subdev, method, data);
            break;
    }

    return data;
}

// This function callwlates the correct data to use for the
// RELOC_RESET_METHOD trace header command if the graphics channel
// is the MaxwellB class.
//
// Note: the first set of MaxwellB methods in the switch statement are
// identical to Fermi methods, so GetFermiMethodResetValue is called.
//
UINT32 Trace3DTest::GetMaxwellBMethodResetValue(UINT32 method) const
{
    switch (method)
    {
        case LWB197_SET_COLOR_TARGET_FORMAT(0):
        case LWB197_SET_COLOR_TARGET_FORMAT(1):
        case LWB197_SET_COLOR_TARGET_FORMAT(2):
        case LWB197_SET_COLOR_TARGET_FORMAT(3):
        case LWB197_SET_COLOR_TARGET_FORMAT(4):
        case LWB197_SET_COLOR_TARGET_FORMAT(5):
        case LWB197_SET_COLOR_TARGET_FORMAT(6):
        case LWB197_SET_COLOR_TARGET_FORMAT(7):
        case LWB197_SET_COLOR_TARGET_MEMORY(0):
        case LWB197_SET_COLOR_TARGET_MEMORY(1):
        case LWB197_SET_COLOR_TARGET_MEMORY(2):
        case LWB197_SET_COLOR_TARGET_MEMORY(3):
        case LWB197_SET_COLOR_TARGET_MEMORY(4):
        case LWB197_SET_COLOR_TARGET_MEMORY(5):
        case LWB197_SET_COLOR_TARGET_MEMORY(6):
        case LWB197_SET_COLOR_TARGET_MEMORY(7):
        case LWB197_SET_COLOR_TARGET_ARRAY_PITCH(0):
        case LWB197_SET_COLOR_TARGET_ARRAY_PITCH(1):
        case LWB197_SET_COLOR_TARGET_ARRAY_PITCH(2):
        case LWB197_SET_COLOR_TARGET_ARRAY_PITCH(3):
        case LWB197_SET_COLOR_TARGET_ARRAY_PITCH(4):
        case LWB197_SET_COLOR_TARGET_ARRAY_PITCH(5):
        case LWB197_SET_COLOR_TARGET_ARRAY_PITCH(6):
        case LWB197_SET_COLOR_TARGET_ARRAY_PITCH(7):
        case LWB197_SET_COLOR_TARGET_WIDTH(0):
        case LWB197_SET_COLOR_TARGET_WIDTH(1):
        case LWB197_SET_COLOR_TARGET_WIDTH(2):
        case LWB197_SET_COLOR_TARGET_WIDTH(3):
        case LWB197_SET_COLOR_TARGET_WIDTH(4):
        case LWB197_SET_COLOR_TARGET_WIDTH(5):
        case LWB197_SET_COLOR_TARGET_WIDTH(6):
        case LWB197_SET_COLOR_TARGET_WIDTH(7):
        case LWB197_SET_COLOR_TARGET_HEIGHT(0):
        case LWB197_SET_COLOR_TARGET_HEIGHT(1):
        case LWB197_SET_COLOR_TARGET_HEIGHT(2):
        case LWB197_SET_COLOR_TARGET_HEIGHT(3):
        case LWB197_SET_COLOR_TARGET_HEIGHT(4):
        case LWB197_SET_COLOR_TARGET_HEIGHT(5):
        case LWB197_SET_COLOR_TARGET_HEIGHT(6):
        case LWB197_SET_COLOR_TARGET_HEIGHT(7):
        case LWB197_SET_COLOR_TARGET_THIRD_DIMENSION(0):
        case LWB197_SET_COLOR_TARGET_THIRD_DIMENSION(1):
        case LWB197_SET_COLOR_TARGET_THIRD_DIMENSION(2):
        case LWB197_SET_COLOR_TARGET_THIRD_DIMENSION(3):
        case LWB197_SET_COLOR_TARGET_THIRD_DIMENSION(4):
        case LWB197_SET_COLOR_TARGET_THIRD_DIMENSION(5):
        case LWB197_SET_COLOR_TARGET_THIRD_DIMENSION(6):
        case LWB197_SET_COLOR_TARGET_THIRD_DIMENSION(7):
        case LWB197_SET_ZT_SELECT:
        case LWB197_SET_ZT_FORMAT:
        case LWB197_SET_ZT_BLOCK_SIZE:
        case LWB197_SET_ZT_ARRAY_PITCH:
        case LWB197_SET_ZT_SIZE_A:
        case LWB197_SET_ZT_SIZE_B:
        case LWB197_SET_ZT_SIZE_C:
        case LWB197_SET_ANTI_ALIAS:
        case LWB197_SET_ANTI_ALIAS_ENABLE:
            return GetFermiMethodResetValue(method);
            break;

        case LWB197_SET_ANTI_ALIAS_SAMPLE_POSITIONS(0):
            return GetProgrammableSampleData(surfZ, 0, params);
            break;

        case LWB197_SET_ANTI_ALIAS_SAMPLE_POSITIONS(1):
            return GetProgrammableSampleData(surfZ, 1, params);
            break;

        case LWB197_SET_ANTI_ALIAS_SAMPLE_POSITIONS(2):
            return GetProgrammableSampleData(surfZ, 2, params);
            break;

        case LWB197_SET_ANTI_ALIAS_SAMPLE_POSITIONS(3):
            return GetProgrammableSampleData(surfZ, 3, params);
            break;

        default:
            ErrPrintf("Don't know how to reset method with offset 0x%x\n", method);
            MASSERT(0);
            return 0;
    }

    return 0;
}

RC TraceChannel::SendPostTraceMethodsMaxwellB(LWGpuSubChannel* pSubch)
{
    RC rc;

    CHECK_RC(SendPostTraceMethodsKepler(pSubch));

    if (params->ParamPresent("-decompress_in_placeZ") > 0)
    {
        CHECK_RC(pSubch->MethodWriteRC(LWB197_DECOMPRESS_ZETA_SURFACE,
                DRF_DEF(B197, _DECOMPRESS_ZETA_SURFACE, _Z_ENABLE, _TRUE)));
    }

    return rc;
}
