/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2012,2014-2016,2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   gm10xpm.cpp
 * @brief  Implemetation of the GM10x performance monitor
 *
 */

#include "gm10xpm.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "maxwell/gm107/dev_ltc.h"
#include "maxwell/gm107/dev_perf.h"
#include "maxwell/gm107/pm_signals.h"
#include "maxwell/gm107/dev_graphics_nobundle.h"
/**************************************************************************************************
 Note: The register address's have changed but the register definitions haven't

 Contacts: Robert Hero
 To get to correct signal routing and perfmon programming you need to do the
 following:

 1. Search through the //hw/tools/perfalyze/chips/gm107/pml_files/ltc.pymlt
    template file to find and experiment that captures the ltc hit & miss
    counts. ie
  I found these for GM107       and these for GM108
  ltc_pm_tag_hit_fbp0_l2slice0          ltc_pm_tag_hit_fbp0_l2slice0
  ltc_pm_tag_miss_fbp0_l2slice0         ltc_pm_tag_miss_fbp0_l2slice0
  ltc_pm_tag_hit_fbp1_l2slice0          ltc_pm_tag_hit_fbp0_l2slice1
  ltc_pm_tag_miss_fbp1_l2slice0         ltc_pm_tag_miss_fbp0_l2slice1
  ltc_pm_tag_hit_fbp0_l2slice1          ltc_pm_tag_hit_fbp0_l2slice2
  ltc_pm_tag_miss_fbp0_l2slice1         ltc_pm_tag_miss_fbp0_l2slice2
  ltc_pm_tag_hit_fbp1_l2slice1          ltc_pm_tag_hit_fbp0_l2slice3
  ltc_pm_tag_miss_fbp1_l2slice1         ltc_pm_tag_miss_fbp0_l2slice3
  ltc_pm_tag_hit_fbp0_l2slice2
  ltc_pm_tag_miss_fbp0_l2slice2
  ltc_pm_tag_hit_fbp1_l2slice2
  ltc_pm_tag_miss_fbp1_l2slice2
  ltc_pm_tag_hit_fbp0_l2slice3
  ltc_pm_tag_miss_fbp0_l2slice3
  ltc_pm_tag_hit_fbp1_l2slice3
  ltc_pm_tag_miss_fbp1_l2slice3

 2. Add these experiments to a text file (ie mods_exp.txt)

 3. Run the pmlsplitter program to generate the register setting for the PM.
 ie. I created a temp dir at //hw/tools/pmlsplitter/gm107/Larry and ran the
     following command from: //hw/tools/pmlsplitter/bin/linux
     pmlsplitter -i ../../../perfalyze/chips/gm107/pml_files/experiments.pml -p ../../gm107/pm_programming_guide.txt -o ../../gm107/Larry -chip gm107 -output_format mods -e ../../gm107/Larry/mods_exp.txt //$
     pmlsplitter -i ../../../perfalyze/chips/gm108/pml_files/experiments.pml -p ../../gm108/pm_programming_guide.txt -o ../../gm108/Larry -chip gm108 -output_format mods -e ../../gm108/Larry/mods_exp.txt //$
     pmlsplitter -i ../../../perfalyze/chips/gm204/pml_files/experiments.pml -p ../../chips/gm204/pm_programming_guide.txt -floorsweeping ../../gm204/Larry/floorsweeping_gm204.xml -o ../../gm204/Larry -chip gm204 -output_format mods -e ../../gm204/Larry/mods_exp_fbp0_7.txt //$
     pmlsplitter -i ../../../perfalyze/chips/gm200/pml_files/experiments.pml -p ../../chips/gm200/pm_programming_guide.txt -floorsweeping ../../gm200/Larry/floorsweeping_gm200_fbp_0x3f.xml -o ../../gm200/Larry -chip gm200 -output_format mods -e ../../gm200/Larry/mods_exp_fbp0_11.txt //$

 4. Using the output from pmlsplitter you can get the signal busIndex values and the proper
    registers that should be written to program each Perfmon.

 5. To debug you can uncomment the code in the L2CacheExperiment constructor to log any register
    access to the perfmons. Or you can do the same thing using Gpu.LogAccessRegions() in
    gputest.js:LwdaL2Test().

 Note: For more supporting documentation please refer to http://lwbugs/1294252
**************************************************************************************************/

// L2 Cache PerfMon(PM) signals:
// There is no header file where these are available to software, however you can find the signal
// programming in the hardware tree at hw/tools/pmlsplitter/gm107/pm_programming_guide.txt.
// Also refer to hw/lwgpu/manuals/pri_perf_pmm.ref and
// https://wiki.lwpu.com/gpuhwdept/index.php/Tool/HardwarePerformanceMonitor

// GM107 & GM108 Bus Signal index values
// Signal       chiplet     ltc     slice   busIndex
// req_active   0           0       0       35 (0x23)
// hit          0           0       0       42 (0x2a)
// miss         0           0       0       43 (0x2b)
// req_active   0           0       1       1 (0x1)
// hit          0           0       1       8 (0x8)
// miss         0           0       1       9 (0x9)
// req_active   0           1       2       35 (0x23)
// hit          0           1       2       42 (0x2a)
// miss         0           1       2       43 (0x2b)
// req_active   0           1       3       1 (0x1)
// hit          0           1       3       8 (0x8)
// miss         0           1       3       9 (0x9)
//
#define GMLIT1_SIG_L2C_REQUEST_ACTIVE_S0_S2 0x23 // the ltc0.ltc2pm_ltc_lts0_request_active signal
#define GMLIT1_SIG_L2C_HIT_S0_S2            0x2A // the ltc0.ltc2pm_ltc_lts0_hit signal
#define GMLIT1_SIG_L2C_MISS_S0_S2           0x2B // the ltc0.ltc2pm_ltc_lts0_miss signal
#define GMLIT1_SIG_L2C_REQUEST_ACTIVE_S1_S3 0x01 // the ltc0.ltc2pm_ltc_lts1_request_active signal
#define GMLIT1_SIG_L2C_HIT_S1_S3            0x08 // the ltc0.ltc2pm_ltc_lts1_hit signal
#define GMLIT1_SIG_L2C_MISS_S1_S3           0x09 // the ltc0.ltc2pm_ltc_lts1_miss signal
/*************************************************************************************************
 * GM10XGpuPerfMon Implementation
 * This is a very thin wrapper around GpuPerfMon to create the correct type of
 * L2CacheExperiment
 ************************************************************************************************/
GM10xGpuPerfmon::GM10xGpuPerfmon(GpuSubdevice *pSubdev) : GpuPerfmon(pSubdev)
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
RC GM10xGpuPerfmon::CreateL2CacheExperiment
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
#if LWCFG(GLOBAL_ARCH_MAXWELL)
        case Gpu::GM200:
        case Gpu::GM204:
        case Gpu::GM206:
            if (Platform::GetSimulationMode() == Platform::RTL ||
                Platform::GetSimulationMode() == Platform::Hardware)
            {
                newExperiment.reset((Experiment*)(new GMLit2L2CacheExperiment(
                            FbpNum, L2Slice, this)));
                break;
            }
            else
                return OK;
        case Gpu::GM107:
        case Gpu::GM108:
            if (Platform::GetSimulationMode() == Platform::RTL ||
                Platform::GetSimulationMode() == Platform::Hardware)
            {
                newExperiment.reset((Experiment*)(new GMLit1L2CacheExperiment(
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

//----------------------------------------------------------------------------
//! \brief Constructor
//!
GMLit1L2CacheExperiment::GMLit1L2CacheExperiment
(
   UINT32 fbpNum,
   UINT32 l2Slice,
   GpuPerfmon* pPerfmon
)
: GML2CacheExperiment(fbpNum, l2Slice, pPerfmon)
{
    if (l2Slice & 0x01)
    {
        m_L2Bsi.ltcActive = GMLIT1_SIG_L2C_REQUEST_ACTIVE_S1_S3;
        m_L2Bsi.ltcHit = GMLIT1_SIG_L2C_HIT_S1_S3;
        m_L2Bsi.ltcMiss = GMLIT1_SIG_L2C_MISS_S1_S3;
        m_L2Bsi.pmTrigger = LW_PERF_PMMFBP_LTC0_SIGVAL_PMA_TRIGGER;
    }
    else
    {
        m_L2Bsi.ltcActive = GMLIT1_SIG_L2C_REQUEST_ACTIVE_S0_S2;
        m_L2Bsi.ltcHit = GMLIT1_SIG_L2C_HIT_S0_S2;
        m_L2Bsi.ltcMiss = GMLIT1_SIG_L2C_MISS_S0_S2;
        m_L2Bsi.pmTrigger = LW_PERF_PMMFBP_LTC0_SIGVAL_PMA_TRIGGER;
    }
}

#define GMLIT2_SIG_L2C_REQUEST_ACTIVE_S0_S2 0x25 // BusIndex="37"
#define GMLIT2_SIG_L2C_HIT_S0_S2            0x2C // BusIndex="44"
#define GMLIT2_SIG_L2C_MISS_S0_S2           0x2D // BusIndex="45"
#define GMLIT2_SIG_L2C_REQUEST_ACTIVE_S1_S3 0x01 // BusIndex="1"
#define GMLIT2_SIG_L2C_HIT_S1_S3            0x08 // BusIndex="8"
#define GMLIT2_SIG_L2C_MISS_S1_S3           0x09 // BusIndex="9"
// GM204, GM206 Bus Signal index values
// Signal       chiplet     ltc     slice   busIndex    Gpu
// req_active   0/1         0/1     0/2     37 (0x25)   GM204/GM206
// hit          0/1         0/1     0/2     44 (0x2c)   GM204/GM206
// miss         0/1         0/1     0/2     45 (0x2d)   GM204/GM206
// req_active   0/1         0/1     1/3     1 (0x1)     GM204/GM206
// hit          0/1         0/1     1/3     8 (0x8)     GM204/GM206
// miss         0/1         0/1     1/3     9 (0x9)     GM204/GM206

// req_active   2/3         0/1     0/2     37 (0x25)   GM204
// hit          2/3         0/1     0/2     44 (0x2c)   GM204
// miss         2/3         0/1     0/2     45 (0x2d)   GM204
// req_active   2/3         0/1     1/3     1 (0x1)     GM204
// hit          2/3         0/1     1/3     8 (0x8)     GM204
// miss         2/3         0/1     1/3     9 (0x9)     GM204

// GP100 Bus Signal index values (GP100 uses GMLit4)
// Signal       chiplet     ltc     slice   busIndex    Gpu
// req_active   0/1         0/1     0/2     37 (0x25)   GP100
// hit          0/1         0/1     0/2     44 (0x2c)   GP100
// miss         0/1         0/1     0/2     45 (0x2d)   GP100
// req_active   0/1         0/1     1/3     1 (0x1)     GP100
// hit          0/1         0/1     1/3     8 (0x8)     GP100
// miss         0/1         0/1     1/3     9 (0x9)     GP100
// req_active   2/3         0/1     0/2     37 (0x25)   GP100
// hit          2/3         0/1     0/2     44 (0x2c)   GP100
// miss         2/3         0/1     0/2     45 (0x2d)   GP100
// req_active   2/3         0/1     1/3     1 (0x1)     GP100
// hit          2/3         0/1     1/3     8 (0x8)     GP100
// miss         2/3         0/1     1/3     9 (0x9)     GP100
// req_active   4/5         0/1     0/2     37 (0x25)   GP100
// hit          4/5         0/1     0/2     44 (0x2c)   GP100
// miss         4/5         0/1     0/2     45 (0x2d)   GP100
// req_active   4/5         0/1     1/3     1 (0x1)     GP100
// hit          4/5         0/1     1/3     8 (0x8)     GP100
// miss         4/5         0/1     1/3     9 (0x9)     GP100
// req_active   6/7         0/1     0/2     37 (0x25)   GP100
// hit          6/7         0/1     0/2     44 (0x2c)   GP100
// miss         6/7         0/1     0/2     45 (0x2d)   GP100
// req_active   6/7         0/1     1/3     1 (0x1)     GP100
// hit          6/7         0/1     1/3     8 (0x8)     GP100
// miss         6/7         0/1     1/3     9 (0x9)     GP100

// fbp         chiplet              slice
// 0            0                   0/1
// 1            0                   2/3
// 2            1                   0/1
// 3            1                   2/3
// 4            2                   0/1
// 5            2                   2/3
// 6            3                   0/1
// 7            3                   2/3
// 8            4                   0/1
// 9            4                   2/3
// 10           5                   0/1
// 11           5                   2/3
// 12           6                   0/1
// 13           6                   2/3
// 14           7                   0/1
// 15           7                   2/3
//----------------------------------------------------------------------------
//! \brief Constructor
//!
GMLit2L2CacheExperiment::GMLit2L2CacheExperiment
(
   UINT32 fbpNum,
   UINT32 l2Slice,
   GpuPerfmon* pPerfmon
)
: GML2CacheExperiment(fbpNum, l2Slice, pPerfmon)
{
    if (l2Slice & 0x01)
    {
        m_L2Bsi.ltcActive = GMLIT2_SIG_L2C_REQUEST_ACTIVE_S1_S3;
        m_L2Bsi.ltcHit = GMLIT2_SIG_L2C_HIT_S1_S3;
        m_L2Bsi.ltcMiss = GMLIT2_SIG_L2C_MISS_S1_S3;
        m_L2Bsi.pmTrigger = LW_PERF_PMMFBP_LTC0_SIGVAL_PMA_TRIGGER;
    }
    else
    {
        m_L2Bsi.ltcActive = GMLIT2_SIG_L2C_REQUEST_ACTIVE_S0_S2;
        m_L2Bsi.ltcHit = GMLIT2_SIG_L2C_HIT_S0_S2;
        m_L2Bsi.ltcMiss = GMLIT2_SIG_L2C_MISS_S0_S2;
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
    **********************************************************************************************/
}

//----------------------------------------------------------------------------
//! \brief Constructor
//!
GML2CacheExperiment::GML2CacheExperiment
(
    UINT32 fbpNum,
    UINT32 l2Slice,
    GpuPerfmon* pPerfmon
)
:   L2CacheExperiment(pPerfmon, GpuPerfmon::Experiment::Fb, fbpNum, l2Slice)
{
}

//----------------------------------------------------------------------------
//! \brief Start the experiments
//!
//! \return OK if starting was successful, not OK otherwise
//!
//Example Experiment:
//Count the number of conditions when:
//  m_SigLtcRegActive & m_SigLtcHit are true on LW_PERF_PMMFBP_EVENT_SEL PM &
//  m_SigLtcRegActive & m_SigLtcMiss are true on LW_PERF_PMMFBP_TRIG0_SEL PM
//
//    i.e.
//    event                   LtcRegActive && LtcHit
//   LW_PERF_PMMFBP_EVENT_SEL(0)    -> 0x0000232a // SEL = X, X, LtcRegActive, LtcHit
//   LW_PERF_PMMGPC_EVENT_OP(0)     -> 0x00008888 // OP = LtcRegActive && LtcHit
//
//   LW_PERF_PMMFBP_TRIG0_SEL(0)    -> 0x0000232b // SEL = X, X, LtcRegActive, LtcMiss
//   LW_PERF_PMMGPC_TRIG0_OP(0)     -> 0x00008888 // OP = LtcRegActive && LtcMiss
//                                //
//                                //EVENT_SEL/TRIG0_SEL:
//                                //     X    X    LtcRegActive  LtcHit/LtcMiss
//                                //     |    |    |             |
//                                //     |    |  .-'             |
//                                //     |    `-.|               |
//                                //     `-----.||.--------------'
//                                //           ||||
//                                //           ||||
//                                //EVENT_OP:  ||||    truth
//                                //           VVVV    table
//                                //                   output
//                                //   bit0    0000     0
//                                //   bit1    0001     0
//                                //   bit2    0010     0
//                                //   bit3    0011     1
//                                //   bit4    0100     0
//                                //   bit5    0101     0
//                                //   bit6    0110     0
//                                //   bit7    0111     1
//                                //   bit8    1000     0
//                                //   bit9    1001     0
//                                //   bit10   1010     0
//                                //   bit11   1011     1
//                                //   bit12   1100     0
//                                //   bit13   1101     0
//                                //   bit14   1110     0
//                                //   bit15   1111     1
// To measure the cache hit/miss rate we use the perfmons that are in the Ltc0--Ltc3 domains.
// The perfmon domains are ordered alphabetically and have the following values:
// GPxxx and older: fbp=0, ltc0=1, ltc1=2, rop0=3, rop1=4.
// GVxxx: fbp0=0, fbp1=1, ltc0=2, ltc1=3, ltc2=4, ltc3=5.
// The L2 slices are spread across all Ltc domains.
// L2 slices 0/1 use ltc0, slices 2/3 use ltc1, slices 4/5 use ltc2, slices 6/7 use ltc3. see
// CalcPmIdx()
// PmIdx is the domain index, so set the PmIdx based on the slice and use the Fbp number to select
// the specific permon in that domain.
// To callwlate the LW_PERF_PMMFBP_* base address we use:
// Pascal & older: LW_PERF_PMMFBP_* base address = LW_PERF_PMMFBP_*(PmIdx) + (fbp * 0x1000)
// Volta: LW_PERF_PMMFBP_* base address = LW_PERF_PMMFBP_*(PmIdx) + (fbp * 0x4000)
//
// From gm10x - gpxxx, the l2slices in a partition are split across perfmons.
// The table below shows the distribution of these for an example chip with 4
// FBPs and 4 l2slices per FBP.
// -----------------------------------------------------------------------------------------------
//|       C++ Code                  | PmlSplitter Experiment tags                |LW_PERF_PMMFBP_*|
//|FB           |L2Slice|Perfmon    |ltc?|_chiplet?|_fbp?|_l2slice?|event/trig0  |starting address|
//|partition    |FBP    |domain     |    |         |     |         |select       |                |
//|             |       |(pmIdx)    |    |         |     |         |             |                |
//|fbpNum*0x1000|       |pmIdx*0x200|    |         |     |         |             |                |
//|-----------------------------------------------------------------------------------------------|
//|   0         |  0    |  Ltc0(1)  |  0 |    0    |  0  |   0     |0x252c/0x252d| 0x1a02??       |
//|   0         |  1    |  Ltc0(1)  |  0 |    0    |  0  |   1     |0x0108/0x0109| 0x1a02??       |
//|   0         |  2    |  Ltc1(2)  |  1 |    0    |  1  |   0     |0x252c/0x252d| 0x1a04??       |
//|   0         |  3    |  Ltc1(2)  |  1 |    0    |  1  |   1     |0x0108/0x0109| 0x1a04??       |
//|   1         |  0    |  Ltc0(1)  |  0 |    1    |  2  |   0     |0x252c/0x252d| 0x1a12??       |
//|   1         |  1    |  Ltc0(1)  |  0 |    1    |  2  |   1     |0x0108/0x0109| 0x1a12??       |
//|   1         |  2    |  Ltc1(2)  |  1 |    1    |  3  |   0     |0x252c/0x252d| 0x1a14??       |
//|   1         |  3    |  Ltc1(2)  |  1 |    1    |  3  |   1     |0x0108/0x0109| 0x1a14??       |
//|   2         |  0    |  Ltc0(1)  |  0 |    2    |  4  |   0     |0x252c/0x252d| 0x1a22??       |
//|   2         |  1    |  Ltc0(1)  |  0 |    2    |  4  |   1     |0x0108/0x0109| 0x1a22??       |
//|   2         |  2    |  Ltc1(2)  |  1 |    2    |  5  |   0     |0x252c/0x252d| 0x1a24??       |
//|   2         |  3    |  Ltc1(2)  |  1 |    2    |  5  |   1     |0x0108/0x0109| 0x1a24??       |
//|   3         |  0    |  Ltc0(1)  |  0 |    3    |  6  |   0     |0x252c/0x252d| 0x1a32??       |
//|   3         |  1    |  Ltc0(1)  |  0 |    3    |  6  |   1     |0x0108/0x0109| 0x1a32??       |
//|   3         |  2    |  Ltc0(2)  |  1 |    3    |  7  |   0     |0x252c/0x252d| 0x1a34??       |
//|   3         |  3    |  Ltc0(2)  |  1 |    3    |  7  |   1     |0x0108/0x0109| 0x1a34??       |
//|   4         |  0    |  Ltc0(1)  |  0 |    4    |  8? |   0     |0x252c/0x252d| 0x1a42??       |
//|   4         |  1    |  Ltc0(1)  |  0 |    4    |  8? |   1     |0x0108/0x0109| 0x1a42??       |
//|   4         |  2    |  Ltc1(2)  |  1 |    4    |  9? |   0     |0x252c/0x252d| 0x1a44??       |
//|   4         |  3    |  Ltc1(2)  |  1 |    4    |  9? |   1     |0x0108/0x0109| 0x1a44??       |
//|   5         |  0    |  Ltc0(1)  |  0 |    5    | 10? |   0     |0x252c/0x252d| 0x1a52??       |
//|   5         |  1    |  Ltc0(1)  |  0 |    5    | 10? |   1     |0x0108/0x0109| 0x1a52??       |
//|   5         |  2    |  Ltc0(2)  |  1 |    5    | 11? |   0     |0x252c/0x252d| 0x1a54??       |
//|   5         |  3    |  Ltc0(2)  |  1 |    5    | 11? |   1     |0x0108/0x0109| 0x1a54??       |

//-------------------------------------------------------------------------------------------------
// Callwlate the proper PerfMon index based on the current L2 slice.
/* virtual */ UINT32 GML2CacheExperiment::CalcPmIdx()
{
    switch (GetL2Slice())
    {
        case 0:
        case 1:
            return 1; // we are using Ltc0 domain
        case 2:
        case 3:
            return 2; // we are using Ltc1 domain
        default:
            MASSERT(!"Dont know what PerfMon index to use.");
            return 0;
    }
}

//-------------------------------------------------------------------------------------------------
// Use the MODS RegHal to properly callwlate the FBP chiplet separation.
UINT32 GML2CacheExperiment::GetFbpChipletSeparation()
{
    GpuPerfmon* const pGpuPerfmon = static_cast<GpuPerfmon *>(GetPerfmon());
    GpuSubdevice* const pSubdev = pGpuPerfmon->GetGpuSubdevice();
    RegHal &gpuRegs = pSubdev->Regs();
    return gpuRegs.LookupAddress(MODS_PERF_PMMFBP_CHIPLET1_ROUTER_CG2) -
           gpuRegs.LookupAddress(MODS_PERF_PMMFBPROUTER_CG2);
}

//-------------------------------------------------------------------------------------------------
/* virtual */ RC GML2CacheExperiment::Start()
{
    RC rc;
    GpuPerfmon* const pGpuPerfmon   = GetPerfmon();
    GpuSubdevice* const pSubdev     = pGpuPerfmon->GetGpuSubdevice();
    RegHal &gpuRegs                 = pSubdev->Regs();
    UINT32 regAddr                  = 0;
    UINT32 regValue                 = 0;
    Tee::Priority pri               = pGpuPerfmon->GetPrintPri();
    SetPmIdx(CalcPmIdx());
    const UINT32 pmIdx = GetPmIdx();

    ResetStats();

    // callwlate the offset for the correct FBP chiplet. Chiplets are logically addressed and are
    // not affected by floor sweeping.
    const UINT32 fbpOffset = GetFbp() * pGpuPerfmon->GetFbpPmmOffset();

    Printf(pri, "SetupFbpExperiment(Fbp:%d PmIdx:%d )\n", GetFbp(), pmIdx);
    CHECK_RC(pGpuPerfmon->SetupFbpExperiment(GetFbp(), GetPmIdx()));

    Printf(pri, "GML2CacheExperiment::Start(Fbp:%d Slice:%d PmIdx:%d)\n",
           GetFbp(), GetL2Slice(), pmIdx);

    // Enable PM in a slice
    // Note: This is the same for every slice. Broadcast out the PM enable.
    regValue = gpuRegs.Read32(MODS_PLTCG_LTCS_LTSS_MISC_LTC_LTS_PM);
    gpuRegs.SetField(&regValue, MODS_PLTCG_LTCS_LTSS_MISC_LTC_LTS_PM_ENABLE_ENABLED);
    gpuRegs.SetField(&regValue, MODS_PLTCG_LTCS_LTSS_MISC_LTC_LTS_PM_SELECT_GRP0);
    gpuRegs.Write32(MODS_PLTCG_LTCS_LTSS_MISC_LTC_LTS_PM, regValue);

    //Enable the FrontEnd Perfmon to allow the PM trigger to propagate forward through the pipe.
    gpuRegs.Write32(MODS_PGRAPH_PRI_FE_PERFMON_ENABLE_ENABLE);

    // Disable SLCG, common on all GMxxx
    Printf(pri, "GML2CacheExperiment::Disable SLCG\n");
    regAddr = gpuRegs.LookupAddress(MODS_PERF_PMMFBPROUTER_CG2) +
        (GetFbp() * GetFbpChipletSeparation());
    regValue = m_SLCG = pSubdev->RegRd32(regAddr);
    gpuRegs.SetField(&regValue, MODS_PERF_PMMFBPROUTER_CG2_SLCG_DISABLED);
    pSubdev->RegWr32(regAddr, regValue);

    // Enable the PerfMon
    regAddr = gpuRegs.LookupAddress(MODS_PERF_PMMFBP_CONTROL, pmIdx) + fbpOffset;
    regValue = pSubdev->RegRd32(regAddr);
    gpuRegs.SetField(&regValue, MODS_PERF_PMMFBP_CONTROL_MODE_B);
    gpuRegs.SetField(&regValue, MODS_PERF_PMMFBP_CONTROL_ADDTOCTR_INCR);
    gpuRegs.SetField(&regValue, MODS_PERF_PMMFBP_CONTROL_CTXSW_MODE_DISABLED);
    gpuRegs.SetField(&regValue, MODS_PERF_PMMFBP_CONTROL_SHADOW_STATE_ILWALID);
    pSubdev->RegWr32(regAddr, regValue);

    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_ENGINE_SEL, pmIdx) + fbpOffset,
                     m_L2Bsi.pmTrigger);

    // Measure hits in the event counter
    Printf(pri, "Measure hits in the event counter\n");
    regValue = gpuRegs.SetField(MODS_PERF_PMMFBP_EVENT_SEL_SEL1, m_L2Bsi.ltcActive);
    gpuRegs.SetField(&regValue, MODS_PERF_PMMFBP_EVENT_SEL_SEL0, m_L2Bsi.ltcHit);
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_EVENT_SEL, pmIdx) + fbpOffset,
                     regValue);

    // Count any time m_SigLtcRegActive & m_SigLtcHit are true
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_EVENT_OP, pmIdx) + fbpOffset,
                     gpuRegs.SetField(MODS_PERF_PMMFBP_EVENT_OP_FUNC, 0x8888));

    // Measure misses in the trig0 counter
    Printf(pri, "Measure misses in the trig0 counter\n");
    regValue = gpuRegs.SetField(MODS_PERF_PMMFBP_TRIG0_SEL_SEL1, m_L2Bsi.ltcActive);
    gpuRegs.SetField(&regValue, MODS_PERF_PMMFBP_TRIG0_SEL_SEL0, m_L2Bsi.ltcMiss);
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIG0_SEL, pmIdx) + fbpOffset,
                     regValue);

    // Count anytime m_SigLtcRegActive & m_SigLtcMiss are true.
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIG0_OP, pmIdx) + fbpOffset,
                     gpuRegs.SetField(MODS_PERF_PMMFBP_TRIG0_OP_FUNC, 0x8888));

    // Zero out the current counters
    Printf(pri, "Zero out counters\n");
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_TRIGGERCNT, pmIdx) + fbpOffset,
                     gpuRegs.SetField(MODS_PERF_PMMFBP_TRIGGERCNT_VAL, 0x0000));

    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_EVENTCNT, pmIdx) + fbpOffset,
                     gpuRegs.SetField(MODS_PERF_PMMFBP_EVENTCNT_VAL, 0x0000));

    // Start the experiment
    Printf(pri, "Start the experiment\n");
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_STARTEXPERIMENT, pmIdx) + fbpOffset,
                     gpuRegs.SetField(MODS_PERF_PMMFBP_STARTEXPERIMENT_START_DOIT));

    // read back the last register to make sure everything has been written.
    pSubdev->RegRd32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_STARTEXPERIMENT, pmIdx) + fbpOffset);
    Printf(pri, "GML2CacheExperiment::Experiment started...\n");

    SetStarted(true);
    return rc;
}

//------------------------------------------------------------------------------
//! \brief End the experiments
//!
//! \return OK if starting was successful, not OK otherwise
//!
/* virtual */ RC GML2CacheExperiment::End()
{
    GpuPerfmon* const pGpuPerfmon   = GetPerfmon();
    GpuSubdevice* const pSubdev     = pGpuPerfmon->GetGpuSubdevice();
    RegHal &gpuRegs                 = pSubdev->Regs();
    UINT32 regAddr                  = 0;
    UINT32 regValue                 = 0;
    const UINT32 fbpNum             = GetFbp();
    const UINT32 pmIdx              = GetPmIdx();
    Tee::Priority pri               = pGpuPerfmon->GetPrintPri();

    Printf(pri, "GML2CacheExperiment::End()");
    RC rc = L2CacheExperiment::End();

    // Stop any active triggers. Without this the
    // LW_PPWR_PMU_IDLE_STATUS_XBAR_IDLE never goes idle.
    Printf(pri, "Stop any active triggers.\n");
    gpuRegs.Write32(MODS_PERF_PMASYS_TRIGGER_GLOBAL_MANUAL_STOP_PULSE);

    // Disable PM in LTC (this is done in L2CacheExperiment::End())
    // Disable PM in a slice
    // Note: This is the same for every slice so broadcast out the PM enable.
    regValue = gpuRegs.Read32(MODS_PLTCG_LTCS_LTSS_MISC_LTC_LTS_PM);
    gpuRegs.SetField(&regValue, MODS_PLTCG_LTCS_LTSS_MISC_LTC_LTS_PM_ENABLE_DISABLED);
    gpuRegs.SetField(&regValue, MODS_PLTCG_LTCS_LTSS_MISC_LTC_LTS_PM_SELECT_GRP0);
    gpuRegs.Write32(MODS_PLTCG_LTCS_LTSS_MISC_LTC_LTS_PM, regValue);

    // Disable PM at the front end of the pipe
    gpuRegs.Write32(MODS_PGRAPH_PRI_FE_PERFMON_ENABLE_DISABLE);

    // Disable the PerfMon
    regAddr = gpuRegs.LookupAddress(MODS_PERF_PMMFBP_CONTROL, pmIdx) +
              (pGpuPerfmon->GetFbpPmmOffset() * fbpNum);
    regValue = pSubdev->RegRd32(regAddr);
    gpuRegs.SetField(&regValue, MODS_PERF_PMMFBP_CONTROL_MODE_DISABLE);
    pSubdev->RegWr32(regAddr, regValue);

    // This is common to all GMxxx GPUs
    Printf(pri, "Restore SLCG\n");
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBPROUTER_CG2) +
                     GetFbp() * GetFbpChipletSeparation(), m_SLCG);

    return rc;
}

