/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016, 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   gp10xpm.cpp
 * @brief  Implemetation of the GP10x performance monitor
 *
 */

#include "gm10xpm.h"
#include "gp10xpm.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "pascal/gp102/dev_ltc.h"
#include "pascal/gp102/dev_perf.h"
#include "pascal/gp102/pm_signals.h"
#include "pascal/gp102/dev_graphics_nobundle.h"

//*************************************************************************************************
// Contacts: Robert Hero
// To get to correct signal routing and perfmon programming you need to do the
// following:
//
// 1. Search through the //hw/tools/perfalyze/chips/gm204/pml_files/ltc.pml
//    template file to find and experiment that captures the ltc hit & miss
//    counts. ie I found these and created text files for each Gpu:
// Experiment                       Found in GPU's ltc.pml file
// ltc_pm_tag_hit_fbp0_l2slice0     GP102, GP104, GP106, GP107, GP108
// ltc_pm_tag_miss_fbp0_l2slice0    GP102, GP104, GP106, GP107, GP108
// ltc_pm_tag_hit_fbp0_l2slice1     GP102, GP104, GP106, GP107, GP108
// ltc_pm_tag_miss_fbp0_l2slice1    GP102, GP104, GP106, GP107, GP108
// ltc_pm_tag_hit_fbp1_l2slice0     GP102, GP104, GP106, GP107, GP108
// ltc_pm_tag_miss_fbp1_l2slice0    GP102, GP104, GP106, GP107, GP108
// ltc_pm_tag_hit_fbp1_l2slice1     GP102, GP104, GP106, GP107, GP108
// ltc_pm_tag_miss_fbp1_l2slice1    GP102, GP104, GP106, GP107, GP108
// ltc_pm_tag_hit_fbp2_l2slice0     GP102, GP104, GP106, GP107
// ltc_pm_tag_miss_fbp2_l2slice0    GP102, GP104, GP106, GP107
// ltc_pm_tag_hit_fbp2_l2slice1     GP102, GP104, GP106, GP107
// ltc_pm_tag_miss_fbp2_l2slice1    GP102, GP104, GP106, GP107
// ltc_pm_tag_hit_fbp3_l2slice0     GP102, GP104, GP106, GP107
// ltc_pm_tag_miss_fbp3_l2slice0    GP102, GP104, GP106, GP107
// ltc_pm_tag_hit_fbp3_l2slice1     GP102, GP104, GP106, GP107
// ltc_pm_tag_miss_fbp3_l2slice1    GP102, GP104, GP106, GP107
// ltc_pm_tag_hit_fbp4_l2slice0     GP102, GP104, GP106,
// ltc_pm_tag_miss_fbp4_l2slice0    GP102, GP104, GP106,
// ltc_pm_tag_hit_fbp4_l2slice1     GP102, GP104, GP106,
// ltc_pm_tag_miss_fbp4_l2slice1    GP102, GP104, GP106,
// ltc_pm_tag_hit_fbp5_l2slice0     GP102, GP104, GP106,
// ltc_pm_tag_miss_fbp5_l2slice0    GP102, GP104, GP106,
// ltc_pm_tag_hit_fbp5_l2slice1     GP102, GP104, GP106,
// ltc_pm_tag_miss_fbp5_l2slice1    GP102, GP104, GP106,
// ltc_pm_tag_hit_fbp6_l2slice0     GP102, GP104,
// ltc_pm_tag_miss_fbp6_l2slice0    GP102, GP104,
// ltc_pm_tag_hit_fbp6_l2slice1     GP102, GP104,
// ltc_pm_tag_miss_fbp6_l2slice1    GP102, GP104,
// ltc_pm_tag_hit_fbp7_l2slice0     GP102, GP104,
// ltc_pm_tag_miss_fbp7_l2slice0    GP102, GP104,
// ltc_pm_tag_hit_fbp7_l2slice1     GP102, GP104,
// ltc_pm_tag_miss_fbp7_l2slice1    GP102, GP104,
// ltc_pm_tag_hit_fbp8_l2slice0     GP102
// ltc_pm_tag_miss_fbp8_l2slice0    GP102
// ltc_pm_tag_hit_fbp8_l2slice1     GP102
// ltc_pm_tag_miss_fbp8_l2slice1    GP102
// ltc_pm_tag_hit_fbp9_l2slice0     GP102
// ltc_pm_tag_miss_fbp9_l2slice0    GP102
// ltc_pm_tag_hit_fbp9_l2slice1     GP102
// ltc_pm_tag_miss_fbp9_l2slice1    GP102
// ltc_pm_tag_hit_fbp10_l2slice0    GP102
// ltc_pm_tag_miss_fbp10_l2slice0   GP102
// ltc_pm_tag_hit_fbp10_l2slice1    GP102
// ltc_pm_tag_miss_fbp10_l2slice1   GP102
// ltc_pm_tag_hit_fbp11_l2slice0    GP102
// ltc_pm_tag_miss_fbp11_l2slice0   GP102
// ltc_pm_tag_hit_fbp11_l2slice1    GP102
// ltc_pm_tag_miss_fbp11_l2slice1   GP102
//
// 2. Add these experiments to a text file (ie mods_exp.txt)
// 3. Run the pmlsplitter program to generate the register setting for the PM.
// ie. I created a temp dir at //hw/tools/pmlsplitter/gp102/Larry and created a cmd.sh file with
//     the following commands:
/* cmd.sh contents
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp0_0.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_0_0.xml
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp1_1.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_1_1.xml
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp2_2.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_2_2.xml
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp3_3.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_3_3.xml
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp4_4.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_4_4.xml
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp5_5.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_5_5.xml
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp6_6.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_6_6.xml
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp7_7.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_7_7.xml
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp8_8.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_8_8.xml
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp9_9.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_9_9.xml
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp10_10.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_10_10.xml
 pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gp102/pml_files/experiments.pml -p ../../chips/gp102/pm_programming_guide.txt -o ../../gp102/Larry -chip gp102 -output_format mods -e ../../gp102/Larry/mods_exp_fbp11_11.txt //$
 mv ../../gp102/Larry/modspml_0000.xml ../../gp102/Larry/modspml_11_11.xml
*/
// Then from: //hw/tools/pmlsplitter/bin/linux  I ran "../../gp102/Larry/cmd.sh"
// 4. Using the output from pmlsplitter (modspml_*_*.xml) you can get the signal busIndex values
//    needed to program the PM and the proper registers that should be written to program each PM.
//
// To debug you can add the following line in gputest.js:LwdaL2Test()
//    Gpu.LogAccessRegions(
//      [
//          [ 0x0040415c, 0x00404160 ], //LW_PGRAPH_PRI_FE_PERFMON
//          [ 0x00140000, 0x0017e3cc ], //LW_PLTCG_LTC0 - LW_PLTCG_LTCS_LTSS_G_PRI_IQ_CG1
//          [ 0x001A0040, 0x001A1500 ], //LW_PERF_PMMFBP_*(0)
//          [ 0x001B4000, 0x001B4124 ]
//      ]
//      );
//
// Note: For more supporting documentation please refer to http://lwbugs/1294252
// L2 Cache PerfMon(PM) signals:
// There is no header file where these are available to software, however you
// can find the signal programming in the hardware tree.
// hw/tools/pmlsplitter/gp102/pm_programming_guide.txt. Also refer to
// hw/lwgpu/manuals/pri_perf_pmm.ref
// also see: https://wiki.lwpu.com/gpuhwdept/index.php/Tool/HardwarePerformanceMonitor

#define GPLIT3_SIG_L2C_REQUEST_ACTIVE_S0_S2 0x0B // BusIndex="11" for slice 0 & 2
#define GPLIT3_SIG_L2C_HIT_S0_S2            0x12 // BusIndex="18" for slice 0 & 2
#define GPLIT3_SIG_L2C_MISS_S0_S2           0x13 // BusIndex="19" for slice 0 & 2
#define GPLIT3_SIG_L2C_REQUEST_ACTIVE_S1_S3 0x29 // BusIndex="41" for slice 1 & 3
#define GPLIT3_SIG_L2C_HIT_S1_S3            0x30 // BusIndex="48" for slice 1 & 3
#define GPLIT3_SIG_L2C_MISS_S1_S3           0x31 // BusIndex="49" for slice 1 & 3

#define GPLIT4_SIG_L2C_REQUEST_ACTIVE_S0_S2 0x25 // BusIndex="37" for slice 0 & 2
#define GPLIT4_SIG_L2C_HIT_S0_S2            0x2C // BusIndex="44" for slice 0 & 2
#define GPLIT4_SIG_L2C_MISS_S0_S2           0x2D // BusIndex="45" for slice 0 & 2
#define GPLIT4_SIG_L2C_REQUEST_ACTIVE_S1_S3 0x01 // BusIndex="1" for slice 1 & 3
#define GPLIT4_SIG_L2C_HIT_S1_S3            0x08 // BusIndex="8" for slice 1 & 3
#define GPLIT4_SIG_L2C_MISS_S1_S3           0x09 // BusIndex="9" for slice 1 & 3

/*************************************************************************************************
 * GP10XGpuPerfMon Implementation
 * This is a very thin wrapper around GF100GpuPerfMon to create the correct L2CacheExperiment
 ************************************************************************************************/
GP10xGpuPerfmon::GP10xGpuPerfmon(GpuSubdevice *pSubdev) : GpuPerfmon(pSubdev)
{
}

//----------------------------------------------------------------------------
//! \brief Create a L2 Cache Experiment
//!
//! \param FbpNum   : FBP number for the experiment
//! \param L2Slice  : L2 Cache slice for the experiment
//! \param pHandle  : Pointer to the newly created experiment
//!
//! \return OK if creating the experiment was successful, not OK otherwise
//!
RC GP10xGpuPerfmon::CreateL2CacheExperiment
(
    UINT32 FbpNum,
    UINT32 L2Slice,
    const GpuPerfmon::Experiment **pHandle
)
{
    RC rc;
    unique_ptr<Experiment> newExperiment;
    GpuSubdevice *pSubdev = GetGpuSubdevice();
    const UINT32 fbpCount = pSubdev->GetFB()->GetFbpCount();

    *pHandle = NULL;

    if (fbpCount <= FbpNum)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (L2Slice >= pSubdev->GetFB()->GetMaxL2SlicesPerFbp())
    {
        return RC::SOFTWARE_ERROR;
    }

    switch (pSubdev->DeviceId())
    {

#if LWCFG(GLOBAL_ARCH_PASCAL)
        case Gpu::GP100:
            if (Platform::GetSimulationMode() == Platform::RTL ||
                Platform::GetSimulationMode() == Platform::Hardware)
            {   // Note this is not a mistake. GP100 came from the GMlit2 hw litter.
                newExperiment.reset((Experiment*)(new GMLit2L2CacheExperiment(
                            FbpNum, L2Slice, this)));
                break;
            }
            else
                return OK;

        case Gpu::GP102:
        case Gpu::GP104:
        case Gpu::GP106:
            if (Platform::GetSimulationMode() == Platform::RTL ||
                Platform::GetSimulationMode() == Platform::Hardware)
            {
                newExperiment.reset((Experiment*)(new GPLit3L2CacheExperiment(
                            FbpNum, L2Slice, this)));
                break;
            }
            else
                return OK;

        case Gpu::GP107:
        case Gpu::GP108:
            if (Platform::GetSimulationMode() == Platform::RTL ||
                Platform::GetSimulationMode() == Platform::Hardware)
            {
                newExperiment.reset((Experiment*)(new GPLit4L2CacheExperiment(
                            FbpNum, L2Slice, this)));
                break;
            }
            else
                return OK;
#endif

        default:
            Printf(Tee::PriError, "Perfmon implementation for the current chip is missing.\n");
            return RC::UNSUPPORTED_FUNCTION;
    }
    CHECK_RC(AddExperiment(newExperiment.get()));
    *pHandle = newExperiment.release();
    return rc;
}

// GP102/GP104/GP106/GP107 Bus Signal index values
// Signal       chiplet     ltc     slice   busIndex    Gpu
// req_active   0/1         0/1     0/2     11 (0x0b)   GP102/GP104/GP106
// hit          0/1         0/1     0/2     18 (0x12)   GP102/GP104/GP106
// miss         0/1         0/1     0/2     19 (0x13)   GP102/GP104/GP106
// req_active   0/1         0/1     1/3     41 (0x29)   GP102/GP104/GP106
// hit          0/1         0/1     1/3     48 (0x30)   GP102/GP104/GP106
// miss         0/1         0/1     1/3     49 (0x31)   GP102/GP104/GP106

// req_active   0/1         0/1     0/2     37 (0x25)   GP107
// hit          0/1         0/1     0/2     44 (0x2c)   GP107
// miss         0/1         0/1     0/2     45 (0x2d)   GP107
// req_active   0/1         0/1     1/3      1 (0x01)   GP107
// hit          0/1         0/1     1/3      8 (0x08)   GP107
// miss         0/1         0/1     1/3      9 (0x09)   GP107

// req_active   2/3         0/1     0/2     11 (0x0b)   GP102/GP104
// hit          2/3         0/1     0/2     18 (0x12)   GP102/GP104
// miss         2/3         0/1     0/2     19 (0x13)   GP102/GP104
// req_active   2/3         0/1     1/3     41 (0x29)   GP102/GP104
// hit          2/3         0/1     1/3     48 (0x30)   GP102/GP104
// miss         2/3         0/1     1/3     49 (0x31)   GP102/GP104

// req_active   2           0/1     0/2     11 (0x0b)   GP106
// hit          2           0/1     0/2     18 (0x12)   GP106
// miss         2           0/1     0/2     19 (0x13)   GP106
// req_active   2           0/1     1/3     41 (0x29)   GP106
// hit          2           0/1     1/3     48 (0x30)   GP106
// miss         2           0/1     1/3     49 (0x31)   GP106

// req_active   4/5         0/1     0/2     11 (0x0b)   GP102
// hit          4/5         0/1     0/2     18 (0x12)   GP102
// miss         4/5         0/1     0/2     19 (0x13)   GP102
// req_active   4/5         0/1     1/3     41 (0x29)   GP102
// hit          4/5         0/1     1/3     48 (0x30)   GP102
// miss         4/5         0/1     1/3     49 (0x31)   GP102

GPLit3L2CacheExperiment::GPLit3L2CacheExperiment
(
   UINT32 fbpNum,
   UINT32 l2Slice,
   GpuPerfmon* pPerfmon
)
: GML2CacheExperiment(fbpNum, l2Slice, pPerfmon)
{
    if (l2Slice & 0x01)
    {
        m_L2Bsi.ltcActive = GPLIT3_SIG_L2C_REQUEST_ACTIVE_S1_S3;
        m_L2Bsi.ltcHit = GPLIT3_SIG_L2C_HIT_S1_S3;
        m_L2Bsi.ltcMiss = GPLIT3_SIG_L2C_MISS_S1_S3;
        m_L2Bsi.pmTrigger = LW_PERF_PMMFBP_LTC0_SIGVAL_PMA_TRIGGER;
    }
    else
    {
        m_L2Bsi.ltcActive = GPLIT3_SIG_L2C_REQUEST_ACTIVE_S0_S2;
        m_L2Bsi.ltcHit = GPLIT3_SIG_L2C_HIT_S0_S2;
        m_L2Bsi.ltcMiss = GPLIT3_SIG_L2C_MISS_S0_S2;
        m_L2Bsi.pmTrigger = LW_PERF_PMMFBP_LTC0_SIGVAL_PMA_TRIGGER;
    }
    /**********************************************************************************************
    Debug, please don't remove. We use this to debug the pmlsplitter files during development
    pPerfmon->SetPrintPri(Tee::PriNormal);
    REGISTER_RANGE NewRegions[] =
    {
        { 0x00140550, 0x00140b50 },
        { 0x00141550, 0x00141b50 },
        { 0x00142550, 0x00142b50 },
        { 0x00143550, 0x00143b50 },
        { 0x0017e350, 0x0017e350 },
        { 0x001a0200, 0x001a05ff }, // ltc 0-1, fpb 0
        { 0x001a1200, 0x001a15ff }, // ltc 0-1, fpb 1
        { 0x001a2200, 0x001a25ff }, // ltc 0-1, fbp 2
        { 0x001a3200, 0x001a35ff }, // ltc 0-1, fbp 3
        { 0x001a4200, 0x001a45ff }, // ltc 0-1, fbp 4
        { 0x001a5200, 0x001a55ff }, // ltc 0-1, fbp 5
        { 0x001b4000, 0x001b4124 },
        { 0x001bc018, 0x001bc018 }, // SLCG chiplet 0
        { 0x001bc218, 0x001bc218 }, // SLCG chiplet 1
        { 0x001bc418, 0x001bc418 }, // SLCG chiplet 2
        { 0x001bc618, 0x001bc618 }, // SLCG chiplet 3
        { 0x001bc818, 0x001bc818 }, // SLCG chiplet 4
        { 0x001bca18, 0x001bca18 }, // SLCG chiplet 5
        { 0x0040415c, 0x0040415c }
    };
    g_GpuLogAccessRegions.clear();
    for (size_t i = 0; i < NUMELEMS(NewRegions); i++)
        g_GpuLogAccessRegions.push_back(NewRegions[i]);
    *********************************************************************************************/
}

GPLit4L2CacheExperiment::GPLit4L2CacheExperiment
(
   UINT32 fbpNum,
   UINT32 l2Slice,
   GpuPerfmon* pPerfmon
)
: GML2CacheExperiment(fbpNum, l2Slice, pPerfmon)
{
    if (l2Slice & 0x01)
    {
        m_L2Bsi.ltcActive = GPLIT4_SIG_L2C_REQUEST_ACTIVE_S1_S3;
        m_L2Bsi.ltcHit    = GPLIT4_SIG_L2C_HIT_S1_S3;
        m_L2Bsi.ltcMiss   = GPLIT4_SIG_L2C_MISS_S1_S3;
        m_L2Bsi.pmTrigger = LW_PERF_PMMFBP_LTC0_SIGVAL_PMA_TRIGGER;
    }
    else
    {
        m_L2Bsi.ltcActive = GPLIT4_SIG_L2C_REQUEST_ACTIVE_S0_S2;
        m_L2Bsi.ltcHit    = GPLIT4_SIG_L2C_HIT_S0_S2;
        m_L2Bsi.ltcMiss   = GPLIT4_SIG_L2C_MISS_S0_S2;
        m_L2Bsi.pmTrigger = LW_PERF_PMMFBP_LTC0_SIGVAL_PMA_TRIGGER;
    }
    /**********************************************************************************************
    Debug, please don't remove. We use this to debug the pmlsplitter files during development
    pPerfmon->SetPrintPri(Tee::PriNormal);
    REGISTER_RANGE NewRegions[] =
    {
        { 0x00140550, 0x00140b50 },
        { 0x00141550, 0x00141b50 },
        { 0x00142550, 0x00142b50 },
        { 0x00143550, 0x00143b50 },
        { 0x0017e350, 0x0017e350 },
        { 0x001a0200, 0x001a05ff }, // ltc 0-1, fpb 0
        { 0x001a1200, 0x001a15ff }, // ltc 0-1, fpb 1
        { 0x001a2200, 0x001a25ff }, // ltc 0-1, fbp 2
        { 0x001a3200, 0x001a35ff }, // ltc 0-1, fbp 3
        { 0x001a4200, 0x001a45ff }, // ltc 0-1, fbp 4
        { 0x001a5200, 0x001a55ff }, // ltc 0-1, fbp 5
        { 0x001b4000, 0x001b4124 },
        { 0x001bc018, 0x001bc018 }, // SLCG chiplet 0
        { 0x001bc218, 0x001bc218 }, // SLCG chiplet 1
        { 0x001bc418, 0x001bc418 }, // SLCG chiplet 2
        { 0x001bc618, 0x001bc618 }, // SLCG chiplet 3
        { 0x001bc818, 0x001bc818 }, // SLCG chiplet 4
        { 0x001bca18, 0x001bca18 }, // SLCG chiplet 5
        { 0x0040415c, 0x0040415c }
    };
    g_GpuLogAccessRegions.clear();
    for (size_t i = 0; i < NUMELEMS(NewRegions); i++)
        g_GpuLogAccessRegions.push_back(NewRegions[i]);
    *********************************************************************************************/
}

