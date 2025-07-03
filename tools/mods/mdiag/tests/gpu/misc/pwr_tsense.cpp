/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.

#include "mdiag/tests/stdtest.h"
#include "core/include/gpu.h"
#include "pwr_tsense.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"

static bool tsense_direct = false;
extern const ParamDecl tsense_params[] =
{
    SIMPLE_PARAM("-tsense_direct", "TSENSE direct test"),
    // below are modifiers to the TSENSE direct test
    UNSIGNED_PARAM("-opt_ts_cal_avg", "Ideal TSENSE bandgap calibration value"),
    UNSIGNED_PARAM("-opt_ts_tc", "Ideal TSENSE temperature coefficient value"),
    SIGNED_PARAM("-opt_int_ts_a", "Temperature colwersion A coefficient adjustment"),
    SIGNED_PARAM("-opt_int_ts_b", "Temperature colwersion B coefficient adjustment"),
    SIMPLE_PARAM("-skip_lwlink", "Skip checking of LWLINK TSENSE"),

    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

TSENSETest::TSENSETest(ArgReader *params):
    LWGpuSingleChannelTest(params)
{
    if(params->ParamPresent("-tsense_direct")) tsense_direct = true;
    else tsense_direct = false;
    m_OptTsCalAvg = params->ParamUnsigned("-opt_ts_cal_avg", 0x20);  // tsmc7 default (ga100)
    m_OptTsTc = params->ParamUnsigned("-opt_ts_tc", 0x4);             // tsmc7 default (ga100)
    m_OptIntTsA = params->ParamSigned("-opt_int_ts_a", 0);
    m_OptIntTsB = params->ParamSigned("-opt_int_ts_b", 0);
    if(params->ParamPresent("-skip_lwlink")) m_SkipLwlink = true;
    else m_SkipLwlink = false;
}
TSENSETest::~TSENSETest(void)
{
}

STD_TEST_FACTORY(tsense, TSENSETest)

int TSENSETest::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    m_arch = lwgpu->GetArchitecture();
    InfoPrintf("m_arch = 0x%x", m_arch);
    macros.MacroInit(m_arch);

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("1 - TSENSETest::Setup failed to create channel\n");
        return 0;
    }

    getStateReport()->init("TSENSE");
    getStateReport()->enable();
    return 1;
}
void TSENSETest::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("TSENSETest::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void TSENSETest::Run(void)
{
    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : TSENSE Test starting ....\n");
    errCnt = 0;

    SetStatus(TEST_INCOMPLETE);
    if (tsense_direct) errCnt += tsense_direct_test();

    if(errCnt)
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        InfoPrintf("TSENSETest:: Test Failed with %d errors\n", errCnt);
    }
    else
    {
        SetStatus(TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
        InfoPrintf("TSENSETest:: Test Passed\n");
    }
    InfoPrintf("TSENSE Test END.\n");
}

int TSENSETest::tsense_direct_test(void)// returns error count
{
    int error;
    const char *my_fs_sig;
    UINT32 i, j; 
    UINT32 ts_dis_mask, data, golden_temp;
    UINT32 ts_dis_mask_low, ts_dis_mask_high;
    int act_temp, min_temp, max_temp;

    InfoPrintf("tsense_direct_test begins\n");
    error = 0;
    data  = 0;

    // Poke into TSENSE BFM to force each BJT to different temperature values
    // GPC#BJT# temperature = GPC# * 5C + BJT# (e.g. GPC1-BJT2 temp=1*5C+2C=7C) 
    Platform::EscapeWrite("pwr_test_tsense_force_temp", 0, 32, 0x1);

    // Override fuse values
    data = m_OptTsCalAvg - 2;
    Platform::EscapeWrite("drv_opt_ts_cal_avg", 0, 32, data); // LW_THERM_GPC_TS_ADJ_BANDGAP_VOLT_INIT - 2
    Platform::EscapeWrite("drv_opt_ts_cal_offset", 0, 32, 2); // opt_ts_cal_avg + opt_ts_cal_offset = LW_THERM_GPC_TS_ADJ_BANDGAP_VOLT_INIT
    data = m_OptTsTc;
    Platform::EscapeWrite("drv_opt_ts_tc", 0, 32, data);      // LW_THERM_GPC_TS_ADJ_BANDGAP_TEMP_COEF_INIT
    data = (UINT32) m_OptIntTsA;
    Platform::EscapeWrite("drv_opt_int_ts_a", 0, 32, data);   // fuse_a = 0
    data = (UINT32) m_OptIntTsB;
    Platform::EscapeWrite("drv_opt_int_ts_b", 0, 32, data);   // fuse_b = 0
    ts_dis_mask = 0;
    ts_dis_mask_low = 0;
    ts_dis_mask_high = 0;
    for (i=0; i<macros.lw_scal_chip_num_gpcs; i++)
    {
        UINT32 dis_idx = i % macros.lw_pwr_per_gpc_num_sensors +
            i * macros.lw_pwr_per_gpc_num_sensors;
        if (dis_idx < 32)
        {
            ts_dis_mask_low = ts_dis_mask_low | (1<<dis_idx); 
        }
        else
        {
            dis_idx = dis_idx - 32;
            ts_dis_mask_high = ts_dis_mask_high | (1<<dis_idx); 
        }
    }
    InfoPrintf ("Fuse ts_dis_mask = 0x%08x%08x \n", ts_dis_mask_high, ts_dis_mask_low);
    Platform::EscapeWrite("drv_opt_ts_dis_mask_low", 0, 32, ts_dis_mask_low);
    Platform::EscapeWrite("drv_opt_ts_dis_mask_high", 0, 32, ts_dis_mask_high);

    // Program PRIV registers in SYS PWR
    lwgpu->SetRegFldDef("LW_THERM_GPC_INDEX", "_READINCR", "_DISABLED");
    lwgpu->SetRegFldDef("LW_THERM_GPC_INDEX", "_WRITEINCR", "_DISABLED");
    for (i=0; i<macros.lw_scal_chip_num_gpcs; i++)
    {
        InfoPrintf("INFO: Programming GPC%0d registers\n", i);
        lwgpu->SetRegFldNum("LW_THERM_GPC_INDEX", "_INDEX", i);
        lwgpu->SetRegFldNum("LW_THERM_GPC_TS_ADJ", "_BANDGAP_VOLT", 0);
        lwgpu->SetRegFldNum("LW_THERM_GPC_TS_ADJ", "_BANDGAP_TEMP_COEF", 0);
        lwgpu->SetRegFldDef("LW_THERM_GPC_TS_AUTO_CONTROL", "_AUTO_POLLING", "_ON");
        lwgpu->SetRegFldNum("LW_THERM_GPC_TS_AUTO_CONTROL", "_POLLING_INTERVAL", 0x7);
        lwgpu->SetRegFldNum("LW_THERM_GPC_TS_AUTO_CONTROL", "_SAMPLE_DELAY", 2100);
        lwgpu->SetRegFldDef("LW_THERM_GPC_TSENSE", "_CALI_SRC", "_FUSE");
        lwgpu->SetRegFldDef("LW_THERM_GPC_TSENSE", "_TC_SRC", "_FUSE");
        lwgpu->SetRegFldDef("LW_THERM_GPC_TSENSE", "_CONTROL_SRC", "_LOGIC");
        lwgpu->SetRegFldNum("LW_THERM_GPC_TSENSE_CONFIG_COPY_INDEX", "_INDEX", i);
        lwgpu->SetRegFldDef("LW_THERM_GPC_TSENSE_CONFIG_COPY", "_NEW_SETTING", "_SEND");
    }
    if (!m_SkipLwlink)
    {
        InfoPrintf("INFO: Programming LWL%0d registers\n", 0);
        lwgpu->SetRegFldNum("LW_THERM_LWL_TS_ADJ", "_BANDGAP_VOLT", 0);
        lwgpu->SetRegFldNum("LW_THERM_LWL_TS_ADJ", "_BANDGAP_TEMP_COEF", 0);
        lwgpu->SetRegFldDef("LW_THERM_LWL_TS_AUTO_CONTROL", "_AUTO_POLLING", "_ON");
        lwgpu->SetRegFldNum("LW_THERM_LWL_TS_AUTO_CONTROL", "_POLLING_INTERVAL", 0x7);
        lwgpu->SetRegFldNum("LW_THERM_LWL_TS_AUTO_CONTROL", "_SAMPLE_DELAY", 2100);
        lwgpu->SetRegFldDef("LW_THERM_LWL_TSENSE", "_CALI_SRC", "_FUSE");
        lwgpu->SetRegFldDef("LW_THERM_LWL_TSENSE", "_TC_SRC", "_FUSE");
        lwgpu->SetRegFldDef("LW_THERM_LWL_TSENSE", "_CONTROL_SRC", "_LOGIC");
        lwgpu->SetRegFldDef("LW_THERM_LWL_TSENSE_CONFIG_COPY", "_NEW_SETTING", "_SEND");
    }
    lwgpu->SetRegFldDef("LW_THERM_SENSOR_1", "_SELECT_SW_A", "_OFF");
    lwgpu->SetRegFldDef("LW_THERM_SENSOR_1", "_SELECT_SW_B", "_OFF");

    // Wait for all BJT rawcode to be sent to SYS PWR
    // account for config serialization, num_bjt * sample_delay, temperature serialization
    // serialization delay per bit is 10 clocks on utilsclk and 10 clocks on TS_CLK
    // config is 87 bit packet, temperature is 20 bit packet, utilsclk is 108 MHz, TS_CLK is 3 MHz
    int utilsclk_mhz = 108;
    int ts_clk_mhz = 3;
    int delay_us = 0;
    delay_us += 10 * (87 + 20) / utilsclk_mhz;  // delay on utilsclk
    delay_us += 10 * (87 + 20) / ts_clk_mhz;  // delay on TS_CLK
    delay_us += 2100 * macros.lw_pwr_per_gpc_num_sensors / ts_clk_mhz;  // delay to sample all BJTs
    InfoPrintf("INFO: Delay %0d us for temperature sense\n", delay_us);
    Platform::Delay(delay_us);

    // Validate read back temperature for each GPC#BJT#. Allow +-1C error
    for (i=0; i<macros.lw_scal_chip_num_gpcs; i++)
    {
        switch (i)
        {
            case 0: my_fs_sig  = "fs2gpc0_gpc_disable";  break;
            case 1: my_fs_sig  = "fs2gpc1_gpc_disable";  break;
            case 2: my_fs_sig  = "fs2gpc2_gpc_disable";  break;
            case 3: my_fs_sig  = "fs2gpc3_gpc_disable";  break;
            case 4: my_fs_sig  = "fs2gpc4_gpc_disable";  break;
            case 5: my_fs_sig  = "fs2gpc5_gpc_disable";  break;
            case 6: my_fs_sig  = "fs2gpc6_gpc_disable";  break;
            case 7: my_fs_sig  = "fs2gpc7_gpc_disable";  break;
            case 8: my_fs_sig  = "fs2gpc8_gpc_disable";  break;
            case 9: my_fs_sig  = "fs2gpc9_gpc_disable";  break;
            case 10: my_fs_sig = "fs2gpc10_gpc_disable"; break;
            case 11: my_fs_sig = "fs2gpc11_gpc_disable"; break;
            default: my_fs_sig = "fs2gpc0_gpc_disable"; break;
        }

        InfoPrintf("INFO: my_fs_sig is %s\n", my_fs_sig);

        // Check if the current GPC was floorswept
        data = 0;  // clear all bytes since EscapeRead updates 1 byte and then checks 4 bytes for floorsweep
        Platform::EscapeRead(my_fs_sig, 0, 1, &data);
        // Skip if GPC was floorswept
        if(data!=0)
        {
            InfoPrintf("INFO: Skipping GPC%0d TSENSE temperature check (due to GPC floorsweep)\n", i);
            continue;
        }
        InfoPrintf("Start checking TSENSE temperatures in GPC%d...\n", i);

        for(j = 0; j < macros.lw_pwr_per_gpc_num_sensors; j++)
        {
            UINT32 DISBIT = i * macros.lw_pwr_per_gpc_num_sensors + j;
            if (DISBIT < 32)
            {
                ts_dis_mask = ts_dis_mask_low;
            }
            else
            {
                DISBIT = DISBIT - 32;
                ts_dis_mask = ts_dis_mask_high;
            }
            lwgpu->SetRegFldNum("LW_THERM_GPC_TSENSE_INDEX", "_GPC_INDEX", i);
            lwgpu->SetRegFldNum("LW_THERM_GPC_TSENSE_INDEX", "_GPC_BJT_INDEX", j);
            if( ts_dis_mask & (1 << DISBIT))  // Disabled BJT, check TEMP_SENSOR_GPC_TSENSE_LWRR_STATE == _ILWALID
            {
                lwgpu->GetRegFldNum("LW_THERM_TEMP_SENSOR_GPC_TSENSE", "_STATE", &data);
                if(data>0)
                {
                    ErrPrintf ("Expecting _STATE==_ILWALID for GPC%d BJT%d, but getting _STATE==_VALID\n", i, j);
                    error ++;
                }
                else
                {
                    InfoPrintf ("GPC%d BJT%d is seeing _STATE==_ILWALID as expected\n", i, j);
                }
            }
            else  // Enabled BJT, TEMP_SENSOR_GPC_TSENSE_LWRR_INTEGER with Golden = (GPC# * 5C + BJT#)
            {
                golden_temp = i*5+j;
                lwgpu->GetRegFldNum("LW_THERM_TEMP_SENSOR_GPC_TSENSE", "_INTEGER", &data);
                if(data > golden_temp + 1 || data < golden_temp - 1)
                {
                    ErrPrintf ("GPC%d BJT%d: golden temp = %d, real temp = %d\n", i, j, golden_temp, data);
                    error ++;
                }
                else
                {
                    InfoPrintf ("GPC%d BJT%d: golden temp = %d, real temp = %d\n", i, j, golden_temp, data);
                }
            }
        }
    }
    // skip checking LWLINK TSENSE (for chips that do not support LWLINK TSENSE)
    if (m_SkipLwlink)
    {
        InfoPrintf("Skipping LWL TSENSE temperature check (due to -skip_lwlink switch)\n");
    }
    else
    {
        // Validate read back temperature for each LWL#BJT#. Allow +-1C error
        InfoPrintf("Start checking TSENSE temperatures in LWL0...\n");
        for(j = 0; j < macros.lw_pwr_lwl_num_sensors; j++)
        {
            lwgpu->SetRegFldNum("LW_THERM_LWL_TSENSE_INDEX", "_INDEX", j);
            // Enabled BJT, TEMP_SENSOR_LWL_TSENSE_LWRR_INTEGER with Golden = (LWL# * 5C + BJT#)
            golden_temp = 0*5+j;
            min_temp = golden_temp - 1;
            max_temp = golden_temp + 1;
            lwgpu->GetRegFldNum("LW_THERM_TEMP_SENSOR_LWL_TSENSE", "_INTEGER", &data);
            // colwert int9 value packed inside uint9 variable to int9 variable
            if (data > 256)
            {
                act_temp = data - 512;
            }
            else
            {
                act_temp = data;
            }
            if ((act_temp > max_temp) || (act_temp < min_temp))
            {
                ErrPrintf ("LWL0 BJT%d: golden temp = %d, real temp = %d\n", j, golden_temp, act_temp);
                error ++;
            }
            else
            {
                InfoPrintf ("LWL0 BJT%d: golden temp = %d, real temp = %d\n", j, golden_temp, act_temp);
            }
        }
    }
    InfoPrintf ("error = %d\n", error);
    return error;
}
