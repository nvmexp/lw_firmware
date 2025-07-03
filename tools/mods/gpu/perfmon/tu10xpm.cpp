/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   gv10xpm.cpp
 * @brief  Implemetation of the GV10x performance monitor
 *
 */

#include "tu10xpm.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/gpu.h"
#include "turing/tu102/dev_ltc.h"
#include "turing/tu102/dev_perf.h"
#include "turing/tu102/pm_signals.h"
#include "turing/tu102/dev_graphics_nobundle.h"

//*************************************************************************************************
// Contacts: Robert Hero
// To get to correct signal routing and perfmon programming you need to do the
// following:
//
// 1. Search through the //hw/tools/perfalyze/chips/gv100/pml_files/ltc.pml
//    template file to find any experiment that captures the ltc hit & miss
//    counts. ie I found these and created text files for each Gpu:
// Experiment                       Found in GPU's ltc.pml file
// ltc_pm_tag_hit_fbp0_l2slice0     TU102
// ltc_pm_tag_miss_fbp0_l2slice0    TU102
// ltc_pm_tag_hit_fbp0_l2slice1     TU102
// ltc_pm_tag_miss_fbp0_l2slice1    TU102
// ltc_pm_tag_hit_fbp0_l2slice2     TU102
// ltc_pm_tag_miss_fbp0_l2slice2    TU102
// ltc_pm_tag_hit_fbp0_l2slice3     TU102
// ltc_pm_tag_miss_fbp0_l2slice3    TU102
// ltc_pm_tag_hit_fbp1_l2slice0     TU102
// ltc_pm_tag_miss_fbp1_l2slice0    TU102
// ltc_pm_tag_hit_fbp1_l2slice1     TU102
// ltc_pm_tag_miss_fbp1_l2slice1    TU102
// ltc_pm_tag_hit_fbp1_l2slice2     TU102
// ltc_pm_tag_miss_fbp1_l2slice2    TU102
// ltc_pm_tag_hit_fbp1_l2slice3     TU102
// ltc_pm_tag_miss_fbp1_l2slice3    TU102
// ltc_pm_tag_hit_fbp2_l2slice0     TU102
// ltc_pm_tag_miss_fbp2_l2slice0    TU102
// ltc_pm_tag_hit_fbp2_l2slice1     TU102
// ltc_pm_tag_miss_fbp2_l2slice1    TU102
// ltc_pm_tag_hit_fbp2_l2slice2     TU102
// ltc_pm_tag_miss_fbp2_l2slice2    TU102
// ltc_pm_tag_hit_fbp2_l2slice3     TU102
// ltc_pm_tag_miss_fbp2_l2slice3    TU102
// ltc_pm_tag_hit_fbp3_l2slice0     TU102
// ltc_pm_tag_miss_fbp3_l2slice0    TU102
// ltc_pm_tag_hit_fbp3_l2slice1     TU102
// ltc_pm_tag_miss_fbp3_l2slice1    TU102
// ltc_pm_tag_hit_fbp3_l2slice2     TU102
// ltc_pm_tag_miss_fbp3_l2slice2    TU102
// ltc_pm_tag_hit_fbp3_l2slice3     TU102
// ltc_pm_tag_miss_fbp3_l2slice3    TU102
// ltc_pm_tag_hit_fbp4_l2slice0     TU102
// ltc_pm_tag_miss_fbp4_l2slice0    TU102
// ltc_pm_tag_hit_fbp4_l2slice1     TU102
// ltc_pm_tag_miss_fbp4_l2slice1    TU102
// ltc_pm_tag_hit_fbp4_l2slice2     TU102
// ltc_pm_tag_miss_fbp4_l2slice2    TU102
// ltc_pm_tag_hit_fbp4_l2slice3     TU102
// ltc_pm_tag_miss_fbp4_l2slice3    TU102
// ltc_pm_tag_hit_fbp5_l2slice0     TU102
// ltc_pm_tag_miss_fbp5_l2slice0    TU102
// ltc_pm_tag_hit_fbp5_l2slice1     TU102
// ltc_pm_tag_miss_fbp5_l2slice1    TU102
// ltc_pm_tag_hit_fbp5_l2slice2     TU102
// ltc_pm_tag_miss_fbp5_l2slice2    TU102
// ltc_pm_tag_hit_fbp5_l2slice3     TU102
// ltc_pm_tag_miss_fbp5_l2slice3    TU102
// ltc_pm_tag_hit_fbp6_l2slice0     TU102
// ltc_pm_tag_miss_fbp6_l2slice0    TU102
// ltc_pm_tag_hit_fbp6_l2slice1     TU102
// ltc_pm_tag_miss_fbp6_l2slice1    TU102
// ltc_pm_tag_hit_fbp6_l2slice2     TU102
// ltc_pm_tag_miss_fbp6_l2slice2    TU102
// ltc_pm_tag_hit_fbp6_l2slice3     TU102
// ltc_pm_tag_miss_fbp6_l2slice3    TU102
// ltc_pm_tag_hit_fbp7_l2slice0     TU102
// ltc_pm_tag_miss_fbp7_l2slice0    TU102
// ltc_pm_tag_hit_fbp7_l2slice1     TU102
// ltc_pm_tag_miss_fbp7_l2slice1    TU102
// ltc_pm_tag_hit_fbp7_l2slice2     TU102
// ltc_pm_tag_miss_fbp7_l2slice2    TU102
// ltc_pm_tag_hit_fbp7_l2slice3     TU102
// ltc_pm_tag_miss_fbp7_l2slice3    TU102
// ltc_pm_tag_hit_fbp8_l2slice0     TU102
// ltc_pm_tag_miss_fbp8_l2slice0    TU102
// ltc_pm_tag_hit_fbp8_l2slice1     TU102
// ltc_pm_tag_miss_fbp8_l2slice1    TU102
// ltc_pm_tag_hit_fbp8_l2slice2     TU102
// ltc_pm_tag_miss_fbp8_l2slice2    TU102
// ltc_pm_tag_hit_fbp8_l2slice3     TU102
// ltc_pm_tag_miss_fbp8_l2slice3    TU102
// ltc_pm_tag_hit_fbp9_l2slice0     TU102
// ltc_pm_tag_miss_fbp9_l2slice0    TU102
// ltc_pm_tag_hit_fbp9_l2slice1     TU102
// ltc_pm_tag_miss_fbp9_l2slice1    TU102
// ltc_pm_tag_hit_fbp9_l2slice2     TU102
// ltc_pm_tag_miss_fbp9_l2slice2    TU102
// ltc_pm_tag_hit_fbp9_l2slice3     TU102
// ltc_pm_tag_miss_fbp9_l2slice3    TU102
// ltc_pm_tag_hit_fbp10_l2slice0    TU102
// ltc_pm_tag_miss_fbp10_l2slice0   TU102
// ltc_pm_tag_hit_fbp10_l2slice1    TU102
// ltc_pm_tag_miss_fbp10_l2slice1   TU102
// ltc_pm_tag_hit_fbp10_l2slice2    TU102
// ltc_pm_tag_miss_fbp10_l2slice2   TU102
// ltc_pm_tag_hit_fbp10_l2slice3    TU102
// ltc_pm_tag_miss_fbp10_l2slice3   TU102
// ltc_pm_tag_hit_fbp11_l2slice0    TU102
// ltc_pm_tag_miss_fbp11_l2slice0   TU102
// ltc_pm_tag_hit_fbp11_l2slice1    TU102
// ltc_pm_tag_miss_fbp11_l2slice1   TU102
// ltc_pm_tag_hit_fbp11_l2slice2    TU102
// ltc_pm_tag_miss_fbp11_l2slice2   TU102
// ltc_pm_tag_hit_fbp11_l2slice3    TU102
// ltc_pm_tag_miss_fbp11_l2slice3   TU102
//
// 2. Add these experiments to a text file for example:
//    mods_exp_fbp0_11.txt, mods_exp_fbp0_0.txt, mods_exp_fbp1_1.txt, etc
// 3. Run the pmlsplitter program to generate the register setting for the PM.
// ie. I created a temp dir at //hw/tools/pmlsplitter/tu102/Larry and created a cmd.sh file with
//     the following commands:
/* cmd.sh contents
# run this shell script from /home/scratch.lbangerter_maxwell/src/hw/tools/pmlsplitter/bin/linux
# ie ../../tu102/Larry/cmd.sh
#
# -i = Include location of the experiments.pml
# -p = location of the Programming guide
# -floorsweeping = location of the floorsweeping xml file
# -o = output directory
# -chip = the GPU to generate the experiments
# -output_format = specify mods style of output results.
# -e = the input file containing a list of the experiments to generate.
# -disable_add_clocks = pmlsplitter by default adds dummy experiments for each clock domain.
#  Use this option to disable it.
# Create a set of experiments for  all 8 slices on each FBPA
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/tu102/pml_files/experiments.pml
            -p ../../chips/tu102/pm_programming_guide.txt
            -o ../../tu102/Larry -chip tu102
            -output_format mods -e ../../tu102/Larry/mods_exp_fbp0_11.txt
mv ../../tu102/Larry/modspml_0000.xml ../../tu102/Larry/modspml_0_11.xml
*/
// Then from: //hw/tools/pmlsplitter/bin/linux  I ran "../../tu102/Larry/cmd.sh"
// 4. Using the output from pmlsplitter (modspml_*_*.xml) you can get the signal busIndex values
//    needed to program the PM and the proper registers that should be written to program each PM.
//
// To debug you can add the following line in gputest.js:LwdaL2Test()
/*
      Gpu.LogAccessRegions([
                     [ 0x00140550, 0x00156b50]
                    ,[ 0x00204440, 0x00205300]
                    ,[ 0x00208440, 0x00209300]
                    ,[ 0x0020c440, 0x0020d300]
                    ,[ 0x00210440, 0x00211300]
                    ,[ 0x00214440, 0x00215300]
                    ,[ 0x00246018, 0x00246018]
                    ,[ 0x00246218, 0x00246218]
                    ,[ 0x00246418, 0x00246418]
                    ,[ 0x00246618, 0x00246618]
                    ,[ 0x00246818, 0x00246818]
                    ,[ 0x00246a18, 0x00246a18]
                    ,[ 0x0040415c, 0x0040415c]
                    ]);
*/
// Note: For more supporting documentation please refer to http://lwbugs/1294252
// L2 Cache PerfMon(PM) signals:
// There is no header file where these are available to software, however you
// can find the signal programming in the hardware tree.
// hw/tools/pmlsplitter/gp102/pm_programming_guide.txt. Also refer to
// hw/lwgpu/manuals/pri_perf_pmm.ref
// also see: https://wiki.lwpu.com/gpuhwdept/index.php/Tool/HardwarePerformanceMonitor
// The bus index for all slices is the same. This is new on Turing!
#define TULIT1_SIG_L2C_REQUEST_ACTIVE 0x1C // BusIndex="28" for all FBPs 0-5 LTCs 0-1 & slices 0-1
#define TULIT1_SIG_L2C_HIT            0x23 // BusIndex="35" for all FBPs 0-5 LTCs 0-1 & slices 0-1
#define TULIT1_SIG_L2C_MISS           0x24 // BusIndex="36" for all FBPs 0-5 LTCs 0-1 & slices 0-1
#define TULIT1_SIG_SIGVAL_PMA_TRIGGER 0x02 // perf_trigger for all experiments
// -----------------------------------------------------------------------------------------------
// Turing Decoder Ring definitions:
// chiplets = "# FBP chiplet per GPU", LTCs = "# ROP_L2 per FBP", SLICES = "# L2 slices per ROP_L2"
// TU102 has 6 chiplets, each chiplet has 2 LTCs, each LTC has 2 SLICES.
// GV100 Bus Signal index values
// Signal       chiplet     ltc     slice   busIndex    Gpu
// ------------ -------     ---     -----   --------    -----
// req_active   0-5         0-1     0-3     28 (0x1C)   TU102
// hit          0-5         0-1     0-3     35 (0x23)   TU102
// miss         0-5         0-1     0-3     36 (0x24)   TU102

/*************************************************************************************************
 * TU100GpuPerfMon Implementation
 * This is a very thin wrapper around GpuPerfMon to report the new offsets that are specific
 * to Volta.
 ************************************************************************************************/
TU10xGpuPerfmon::TU10xGpuPerfmon(GpuSubdevice *pSubdev) : GpuPerfmon(pSubdev)
{
}

//------------------------------------------------------------------------------------------------
//return the offset from the base FBP PMM register to the register for subsequent FBPs
/*virtual*/
UINT32 TU10xGpuPerfmon::GetFbpPmmOffset()
{
    return 0x4000;
}

RC TU10xGpuPerfmon::SetupFbpExperiment(UINT32 FbpNum, UINT32 PmIdx)
{
    Printf(GetPrintPri(), "TU10xGpuPerfmon::SetupFbpExperiment(fbp=%d pmIdx=%d)\n",
           FbpNum, PmIdx);
    GpuSubdevice *pSubdev = GetGpuSubdevice();
    RegHal &gpuRegs = pSubdev->Regs();
    // Turing doesn't have a mask value defined in dev_perf.h, so we will just write all of the
    // bits.
    gpuRegs.Write32(MODS_PERF_PMASYS_FBP_TRIGGER_START_MASK, 0xffffffff);
    gpuRegs.Write32(MODS_PERF_PMASYS_FBP_TRIGGER_STOP_MASK, 0xffffffff);
    gpuRegs.Write32(MODS_PERF_PMASYS_FBP_TRIGGER_CONFIG_MIXED_MODE, 0xffffffff);
    gpuRegs.Write32(MODS_PERF_PMASYS_TRIGGER_GLOBAL_ENABLE_ENABLED);

    ResetFbpPmControl(FbpNum, PmIdx);

    const UINT32 fbpOffset = FbpNum * GetFbpPmmOffset();
    pSubdev->RegWr32(gpuRegs.LookupAddress(MODS_PERF_PMMFBP_CLAMP_CYA_CONTROL, PmIdx) + fbpOffset,
                     gpuRegs.SetField(MODS_PERF_PMMFBP_CLAMP_CYA_CONTROL_ENABLECYA_DISABLE));

    return OK;
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
RC TU10xGpuPerfmon::CreateL2CacheExperiment
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
#if LWCFG(GLOBAL_GPU_IMPL_TU102)
        case Gpu::TU102:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU104)
        case Gpu::TU104:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU106)
        case Gpu::TU106:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU102) || LWCFG(GLOBAL_GPU_IMPL_TU104) || LWCFG(GLOBAL_GPU_IMPL_TU106)
            if (Platform::GetSimulationMode() == Platform::RTL ||
                Platform::GetSimulationMode() == Platform::Hardware)
            {
                newExperiment.reset((Experiment*)(new TULit2L2CacheExperiment(
                            FbpNum, L2Slice, this)));
                break;
            }
            else
                return OK;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_TU116)
        case Gpu::TU116:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU117)
        case Gpu::TU117:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU116) || LWCFG(GLOBAL_GPU_IMPL_TU117)
            if (Platform::GetSimulationMode() == Platform::RTL ||
                Platform::GetSimulationMode() == Platform::Hardware)
            {
                newExperiment.reset((Experiment*)(new TULit3L2CacheExperiment(
                            FbpNum, L2Slice, this)));
                break;
            }
            else
                return OK;
#endif
        default:
            Printf(Tee::PriError, "Unknown DeviceId for Turing Perfmon implementation.\n");
            return RC::UNSUPPORTED_FUNCTION;
    }
    CHECK_RC(AddExperiment(newExperiment.get()));
    *pHandle = newExperiment.release();
    return rc;
}


/*************************************************************************************************
 * TULit2L2CacheExperiment Implementation
 *
 ************************************************************************************************/

//------------------------------------------------------------------------------------------------
TULit2L2CacheExperiment::TULit2L2CacheExperiment
(
   UINT32 fbpNum,
   UINT32 l2Slice,
   GpuPerfmon* pPerfmon
)
: GML2CacheExperiment(fbpNum, l2Slice, pPerfmon)
{
    // The default values for m_L2Bsi are incorrect for Turing. Update them here.
    m_L2Bsi.ltcActive   = TULIT1_SIG_L2C_REQUEST_ACTIVE;
    m_L2Bsi.ltcHit      = TULIT1_SIG_L2C_HIT;
    m_L2Bsi.ltcMiss     = TULIT1_SIG_L2C_MISS;
    m_L2Bsi.pmTrigger   = TULIT1_SIG_SIGVAL_PMA_TRIGGER;

    /**********************************************************************************************
    Debug, please don't remove. We use this to debug the pmlsplitter files during development
    pPerfmon->SetPrintPri(Tee::PriNormal);
    REGISTER_RANGE NewRegions[] =
    {
         {0x00140550, 0x00156b50} //$
        ,{0x00200440, 0x00201300} //$
        ,{0x00204440, 0x00205300} //$
        ,{0x00208440, 0x00209300} //$
        ,{0x0020c440, 0x0020d300} //$
        ,{0x00210440, 0x00211300} //$
        ,{0x00214440, 0x00215300} //$
        ,{0x00246018, 0x00246018} //$
        ,{0x00246218, 0x00246218} //$
        ,{0x00246418, 0x00246418} //$
        ,{0x00246618, 0x00246618} //$
        ,{0x00246818, 0x00246818} //$
        ,{0x00246a18, 0x00246a18} //$
        ,{0x0040415c, 0x0040415c} //$
    };
    g_GpuLogAccessRegions.clear();
    for (size_t i = 0; i < NUMELEMS(NewRegions); i++)
        g_GpuLogAccessRegions.push_back(NewRegions[i]);
    *********************************************************************************************/
}

//------------------------------------------------------------------------------------------------
// Callwlate the proper PerfMon domain index for the LTC based on the current L2 slice.
// The perfmon domains are ordered alphabetically and have the following values:
//                      TU10x       TU11x
// Domain   PmIndex     Logical     Logical
//                      Slice       Slice
// -------- --------    --------    --------
// fbp0        0           -           -
// fpb1        1           -           -
// ltc0s0      2           0           0
// ltc0s1      3           1           1
// ltc0s2      4           2           -
// ltc0s3      5           3           -
// ltc1s0      6           4           2
// ltc1s1      7           5           3
// ltc1s2      8           6           -
// ltc1s3      9           7           -
// rop0        10          -           -
// rop1        11          -           -
// The L2 slices are spread across all Ltc domains.
// L2 slices 0-3 use ltc0, slices 4-7 use ltc1
// PmIdx is the perfmon domain index, so return the PmIdx based on the slice plus an offset of 2.
// The pm_programming_guide.txt located in src/hw/tools/perfalyze/chips/tu102 has the Pm indexes.
/* virtual */
UINT32 TULit2L2CacheExperiment::CalcPmIdx()
{
    return 2 + GetL2Slice();
}

//------------------------------------------------------------------------------------------------
// TULit3 chips have the following configurations:
// 6 FBPs, 2LTCs per FBP, & 2 slices per LTC. However the Perfmon domains as defined in the
// pm_programming_guide are using the same layout defined for TULit2 chips. We need to adjust the
// PM Index to jump passed the unused slots for the additional 2 slices (see CalcPmIdx()).
TULit3L2CacheExperiment::TULit3L2CacheExperiment
(
   UINT32 fbpNum,
   UINT32 l2Slice,
   GpuPerfmon* pPerfmon
)
: GML2CacheExperiment(fbpNum, l2Slice, pPerfmon)
{
    // The default values for m_L2Bsi are incorrect for Turing. Update them here.
    m_L2Bsi.ltcActive   = TULIT1_SIG_L2C_REQUEST_ACTIVE;
    m_L2Bsi.ltcHit      = TULIT1_SIG_L2C_HIT;
    m_L2Bsi.ltcMiss     = TULIT1_SIG_L2C_MISS;
    m_L2Bsi.pmTrigger   = TULIT1_SIG_SIGVAL_PMA_TRIGGER;

    /**********************************************************************************************
    Debug, please don't remove. We use this to debug the pmlsplitter files during development
    pPerfmon->SetPrintPri(Tee::PriNormal);
    REGISTER_RANGE NewRegions[] =
    {
         {0x00140550, 0x00156b50} //$
        ,{0x00200440, 0x00201300} //$
        ,{0x00204440, 0x00205300} //$
        ,{0x00208440, 0x00209300} //$
        ,{0x0020c440, 0x0020d300} //$
        ,{0x00210440, 0x00211300} //$
        ,{0x00214440, 0x00215300} //$
        ,{0x00246018, 0x00246018} //$
        ,{0x00246218, 0x00246218} //$
        ,{0x00246418, 0x00246418} //$
        ,{0x00246618, 0x00246618} //$
        ,{0x00246818, 0x00246818} //$
        ,{0x00246a18, 0x00246a18} //$
        ,{0x0040415c, 0x0040415c} //$
    };
    g_GpuLogAccessRegions.clear();
    for (size_t i = 0; i < NUMELEMS(NewRegions); i++)
        g_GpuLogAccessRegions.push_back(NewRegions[i]);
    *********************************************************************************************/
}

//------------------------------------------------------------------------------------------------
// Callwlate the proper PerfMon domain index for the LTC based on the current L2 slice.
// The perfmon domains are ordered alphabetically and have the following values:
//                      TU10x       TU11x
// Domain   PmIndex     Logical     Logical
//                      Slice       Slice
// -------- --------    --------    --------
// fbp0        0           -           -
// fpb1        1           -           -
// ltc0s0      2           0           0
// ltc0s1      3           1           1
// ltc0s2      4           2           -
// ltc0s3      5           3           -
// ltc1s0      6           4           2
// ltc1s1      7           5           3
// ltc1s2      8           6           -
// ltc1s3      9           7           -
// rop0        10          -           -
// rop1        11          -           -
// The L2 slices are spread across all Ltc domains.
// PmIndex is the perfmon domain index, so return the PmIdx based on the logical slice for the
// current FBP. The pm_programming_guide.txt located in src/hw/tools/perfalyze/chips/tu102 has the
// PM indexes described above.
/* virtual */
UINT32 TULit3L2CacheExperiment::CalcPmIdx()
{
    UINT32 slice = GetL2Slice();
    switch (slice)
    {
        case 0:
        case 1:
            return 2 + slice;
        case 2:
        case 3:
            return 4 + slice;
        default:
            MASSERT(!"Dont know what PerfMon index to use.");
            return 0;
    }
}

