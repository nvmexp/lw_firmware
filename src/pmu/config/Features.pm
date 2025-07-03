#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#
#
#
# pmu-config file that specifies all known pmu-config features
#
# @endsection
#

package Features;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Features is derived from 'Groups'

use chipGroups;                 # imports CHIPS from Chips.pm and chip functions

@ISA = qw(Groups);

#
# This list contains all pmu-config features.
#
my $featuresRef = [

    # platform the software will run on.
    # For now, just have unknown and PMU
    PLATFORM_UNKNOWN =>
    {
        DESCRIPTION         => "Running on an unknown platform",
        DEFAULT_STATE       => DISABLED,
    },
    PLATFORM_PMU =>
    {
        DESCRIPTION         => "Running on the PMU",
        DEFAULT_STATE       => DISABLED,
    },

    # target architecture
    ARCH_FALCON =>
    {
        DESCRIPTION         => "FALCON architecture",
        PROFILES            => [ 'pmu-*', '-pmu-*_riscv', ],
    },
    ARCH_RISCV =>
    {
        DESCRIPTION         => "RISCV architecture",
        PROFILES            => [ 'pmu-*_riscv', ],
        SRCFILES            => [ "pmu/lw/pmu_riscv.c" ],
    },


    # This feature is special.  It and the PMUCORE_<<gpuFamily>> features will
    # be automatically enabled by rmconfig based on the enabled gpus.
    PMUCORE_BASE =>
    {
        DESCRIPTION         => "PMUCORE Base",
    },

    PMUCORE_GM10X =>
    {
        DESCRIPTION         => "PMUCORE for GM10X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_GM20X =>
    {
        DESCRIPTION         => "PMUCORE for GM20X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_GP10X =>
    {
        DESCRIPTION         => "PMUCORE for GP10X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_GV10X =>
    {
        DESCRIPTION         => "PMUCORE for GV10X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_GV11X =>
    {
        DESCRIPTION         => "PMUCORE for GV11X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_TU10X =>
    {
        DESCRIPTION         => "PMUCORE for TU10X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_GA10X =>
    {
        DESCRIPTION         => "PMUCORE for GA10X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_GA10B =>
    {
        DESCRIPTION         => "PMUCORE for GA10B",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_AD10X =>
    {
        DESCRIPTION         => "PMUCORE for AD10X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_AD10B =>
    {
        DESCRIPTION         => "PMUCORE for AD10B",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_GH10X =>
    {
        DESCRIPTION         => "PMUCORE for GH10X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_GH20X =>
    {
        DESCRIPTION         => "PMUCORE for GH20X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_GB10X =>
    {
        DESCRIPTION         => "PMUCORE for GB10X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_G00X =>
    {
        DESCRIPTION         => "PMUCORE for G00X",
        DEFAULT_STATE       => DISABLED,
    },

    PMUCORE_IG00X =>
    {
        DESCRIPTION         => "PMUCORE for IG00X",
        DEFAULT_STATE       => DISABLED,
    },

    PMU_FBQ =>
    {
        DESCRIPTION         => "Enables FB queue for both command and message queues to/from the RM",
        PROFILES            => [ 'pmu-gp100...', ],
    },

    PMU_HALSTUB =>
    {
        DESCRIPTION         => "Stubs for HAL functions",
        PROFILES            => [ 'pmu-gm20x', ],
    },

    PMU_ODP_SUPPORTED =>
    {
        DESCRIPTION         => "Should be explicitely listed as a requirement for all features supported only on ODP builds",
        PROFILES            => [ 'pmu-ga10x_riscv...', ],
    },

    PMU_UCODE_PROFILE_AUTO =>
    {
        DESCRIPTION         => "Set when RM bootstraps using Auto PMU ucode profile",
        PROFILES            => [ ],
    },

    PMU_FB_DDR_SUPPORTED =>
    {
        DESCRIPTION         => "Support for DDR memory (GDDR5, GDDR5X, GDDR6, etc.)",
        PROFILES            => [ 'pmu-*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_FB_HBM_SUPPORTED =>
    {
        DESCRIPTION         => "Support for HBM memory (high bandwidth)",
        PROFILES            => [ 'pmu-tu10x', 'pmu-ga100', 'pmu-gh100*', 'pmu-gb100_riscv', 'pmu-g00x_riscv', ],
    },

    PMU_RPC_CHECKERS =>
    {
        DESCRIPTION         => "Enables input parameter checkers in RPC infrastructure",
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b' ],
    },

    PMU_SELWRITY_HARDENING =>
    {
        DESCRIPTION         => "Security hardening in PMU microcode prevents various vulnerabilities, threats, and architectural issues",
        PROFILES            => [ 'pmu-gp10x...', ],
    },

    PMU_SELWRITY_HARDENING_SEQ =>
    {
        DESCRIPTION         => "Security hardening the Sequencer",
        PROFILES            => [ 'pmu-gp10x...', ],
    },

    PMU_SELWRITY_HARDENING_PERF_CF =>
    {
        DESCRIPTION         => "Security hardening for PerfCf",
        REQUIRES            => [ PMUTASK_PERF_CF, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PBI_SUPPORT =>
    {
        DESCRIPTION         => "PMU Postbox Interface",
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pbi/lw/pbi.c",
                                 "pbi/maxwell/pbi_gm10x.c",
                                 "pbi/hopper/pbi_gh100.c", ],
    },

    PMU_PBI_MODE_SET =>
    {
        DESCRIPTION         => "PMU Postbox Interface for triggering PMU modeset",
        REQUIRES            => [ PMU_PBI_SUPPORT, ],
        PROFILES            => [ 'pmu-gm20x', 'pmu-gp10x', 'pmu-tu10x', '-pmu-gv11b', ],
        SRCFILES            => [ "pbi/lw/pbi.c"              ,
                                 "pmu/maxwell/pmu_pmugm20x.c",
                                 "pbi/maxwell/pbi_gm10x.c",
                                 "pbi/maxwell/pbi_gm20x.c",
                                 "pbi/hopper/pbi_gh100.c", ],
    },

    PMU_SHA1 =>
    {
        DESCRIPTION         => "PMU SHA-1 Implementation",
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "sha1/pmu_sha1.c", ],
    },

    PMU_GID_SUPPORT =>
    {
        DESCRIPTION         => "PMU GPU ID generation support",
        ENGINES_REQUIRED    => [ ECID ],
        REQUIRES            => [ PMU_SHA1, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "gid/pmu_gid.c",
                                 "gid/maxwell/gid_gm20x.c",
                                 "gid/ampere/gid_ga10x.c",],
    },

    PMUTASK_CMDMGMT =>
    {
        DESCRIPTION         => "PMU Command Management Task (CORE)",
        PROFILES            => [ 'pmu-*', ],
        SRCFILES            => [ "task_cmdmgmt.c", ],
    },

    PMUTASK_GCX =>
    {
        DESCRIPTION         => "PMU GCX Task",
        ENGINES_REQUIRED    => [ DISP, GCX, ],
        PROFILES            => [ 'pmu-gm*', ],
        SRCFILES            => [ "task_gcx.c"      ,
                                 "gcx/lw/gcx_rpc.c", ],
    },

    PMUTASK_LPWR =>
    {
        DESCRIPTION         => "PMU LPWR Task",
        ENGINES_REQUIRED    => [ FIFO, PG, LPWR, ],
        PROFILES            => [ 'pmu-*', ],
        SRCFILES            => [ "task_lpwr.c", ],
    },

    PMUTASK_LPWR_LP =>
    {
        DESCRIPTION         => "PMU LPWR LowPriority Task",
        ENGINES_REQUIRED    => [ PG, LPWR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "task_lpwr_lp.c", ],
    },

    PMUTASK_I2C =>
    {
        DESCRIPTION         => "PMU I2C Task",
        ENGINES_REQUIRED    => [ I2C, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "task_i2c.c"        ,
                                 "i2c/lw/pmu_hwi2c.c",
                                 "i2c/lw/pmu_swi2c.c",
                                 "i2c/lw/i2c_rpc.c", ],
    },

    PMU_DPAUX_SUPPORT =>
    {
        DESCRIPTION         => "PMU DPAUX Support",
        ENGINES_REQUIRED    => [ I2C, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "i2c/lw/dpaux.c", ],
    },

    PMUTASK_SEQ =>
    {
        DESCRIPTION         => "PMU Sequencer Task",
        ENGINES_REQUIRED    => [ FB, SEQ, ],
        PROFILES            => [ '...pmu-tu10x', '-pmu-gv11b', ],
        SRCFILES            => [ "task_sequencer.c",
                                 "seq/lw/seq_rpc.c", ],
    },

    PMUTASK_PCMEVT =>
    {
        DESCRIPTION         => "PMU Chip Pcm Event Task",
        PROFILES            => [ 'pmu-gm20x', ],
        SRCFILES            => [ "task_pcmEvent.c", ],
    },

    PMUTASK_PERF =>
    {
        DESCRIPTION         => "PMU PERF Task",
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "task_perf.c", ],
    },

    PMUTASK_PERF_DAEMON =>
    {
        DESCRIPTION         => "PMU PERF DAEMON Task",
        ENGINES_REQUIRED    => [ PERF, ],    # PERF_DAEMON is daemon task for PERF engine
        REQUIRES            => [ PMUTASK_PERF, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "task_perf_daemon.c"   ,
                                 "perf/lw/perf_daemon.c", ],
    },

    PMUTASK_PMGR =>
    {
        DESCRIPTION         => "PMU PMGR Task",
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "task_pmgr.c", ],
    },

    PMUTASK_PERFMON =>
    {
        DESCRIPTION         => "PerfMon",
        ENGINES_REQUIRED    => [ FIFO, ],
        CONFLICTS           => [ PMUTASK_PERF_CF, ],
        PROFILES            => [ '...pmu-gv10x', 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', 'pmu-gv11b', ],
        SRCFILES            => [ "task_perfmon.c"          ,
                                 "perfmon/lw/perfmon_rpc.c", ],
    },

    PMUTASK_DISP =>
    {
        DESCRIPTION         => "Display Task",
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "task_disp.c", ],
    },

    PMUTASK_NNE =>
    {
        DESCRIPTION         => "Neural Net Engine (NNE) Task",
        ENGINES_REQUIRED    => [ NNE, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "task_nne.c", ],
    },

    PMU_NNE_BOARDOBJGRP_SUPPORT =>
    {
        DESCRIPTION         => "Enable/disable support for Board Object Group handling.",
        REQUIRES            => [ PMUTASK_NNE, ],
        ENGINES_REQUIRED    => [ NNE, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_NNE_VAR =>
    {
        DESCRIPTION         => "Enable/disable support for NNE_VAR's",
        REQUIRES            => [ PMUTASK_NNE, ],
        ENGINES_REQUIRED    => [ NNE, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "nne/lw/nne_var.c",
                                 "nne/lw/nne_var_freq.c",
                                 "nne/lw/nne_var_pm.c",
                                 "nne/lw/nne_var_chip_config.c",
                                 "nne/lw/nne_var_power_dn.c",
                                 "nne/lw/nne_var_power_total.c", ],
    },

    PMU_NNE_LAYER =>
    {
        DESCRIPTION         => "Enable/disable support for NNE_LAYER's",
        REQUIRES            => [ PMUTASK_NNE, ],
        ENGINES_REQUIRED    => [ NNE, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "nne/lw/nne_layer.c",
                                 "nne/lw/nne_layer_fc_10.c", ],
    },

    PMU_NNE_DESC =>
    {
        DESCRIPTION         => "Enable/disable support for NNE_DESC's",
        REQUIRES            => [ PMUTASK_NNE, ],
        ENGINES_REQUIRED    => [ NNE, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "nne/lw/nne_desc.c",
                                 "nne/lw/nne_desc_fc_10.c",
                                 "nne/lw/nne_desc_client.c", ],
    },

    PMU_NNE_INFERENCE_RPC =>
    {
        DESCRIPTION         => "Enable/disable NNE inference RPC support",
        REQUIRES            => [ PMUTASK_NNE, PMU_NNE_DESC, ],
        ENGINES_REQUIRED    => [ NNE, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_NNE_DESC_INFERENCE_PROFILING =>
    {
        DESCRIPTION         => "Enable profiling of NNE-driven infererence.",
        REQUIRES            => [ PMUTASK_NNE, PMU_NNE_DESC, ],
        ENGINES_REQUIRED    => [ NNE, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_DISP_CHECK_VRR_HACK_BUG_1604175 =>
    {
        DESCRIPTION         => "Check for VRR hack. Bug 1604175",
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_DISP_LWSR_MODE_SET =>
    {
        DESCRIPTION         => "PMU BSOD modeset with LWSR (SR + GC6)",
        PROFILES            => [ 'pmu-gp10x', 'pmu-tu10x', '-pmu-gv11b', ],
        SRCFILES            => [ "disp/pascal/pmu_dispgp10x.c",
                                 "disp/pascal/pmu_dispgp10xonly.c",
                                 "disp/turing/pmu_disptu10x.c",
                                 "disp/turing/pmu_disptu10xonly.c",],
    },

    PMU_DISP_LWSR_GC6_EXIT =>
    {
        DESCRIPTION         => "PMU GC6 exit modeset with LWSR (SR + GC6)",
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "disp/turing/pmu_disptu10x.c",
                                 "disp/turing/pmu_disptu10xonly.c",],
    },

    PMU_DISPLAYLESS =>
    {
        DESCRIPTION         => "Enable this feature for DISPLAYLESS profile",
        PROFILES            => [ 'pmu-ga100', 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', 'pmu-gh100*', 'pmu-gb100_riscv', ],
    },

    PMU_DISP_IMP_INFRA =>
    {
        DESCRIPTION         => "Infrastructure to cache/program the applicable IMP settings for multiple features such as MCLK switching, MSCG watermarks, etc.",
        REQUIRES            => [ PMUTASK_DISP, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "disp/lw/disp_imp.c",
                                 "disp/turing/pmu_isohubtu10x.c",
                                 "disp/ampere/pmu_isohubga10x.c",],
    },

    PMU_DISP_MS_RG_LINE_WAR_200401032 =>
    {
        DESCRIPTION         => "MS display RGLine war for Bug: 200401032",
        REQUIRES            => [ PMU_PG_MS, PMUTASK_DISP, ],
        ENGINES_REQUIRED    => [ DISP, ],
        PROFILES            => [ 'pmu-tu10x', ],
    },

    PMU_DISP_PROG_ONE_SHOT_START_DELAY =>
    {
        DESCRIPTION         => "Feature which controls programming one shot start delay for MSCG/MCLK in oneshot mode",
        REQUIRES            => [ PMUTASK_DISP, PMUTASK_LPWR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_DISP_MSCG_LOW_FPS_WAR_200637376 =>
    {
        DESCRIPTION         => "MSCG WAR for Low FPS for Bug: 200637376",
        REQUIRES            => [ PMU_PG_MS, PMUTASK_DISP, ],
        ENGINES_REQUIRED    => [ DISP, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
    },

    PMU_DISP_PROG_FRAME_TIMER =>
    {
        DESCRIPTION         => "Feature which controls programming of frame time register by multiple clients",
        REQUIRES            => [ PMUTASK_DISP, PMUTASK_LPWR, PMU_PG_MS, ],
        ENGINES_REQUIRED    => [ DISP, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', '-pmu-gv11b', ],
    },

    PMUTASK_THERM =>
    {
        DESCRIPTION         => "Primary THERM task - holds FANCTRL and SMBPBI, will be expanded for others",
        ENGINES_REQUIRED    => [ THERM, FB, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "task_therm.c", ],
    },

    PMUTASK_HDCP =>
    {
        DESCRIPTION         => "PMU HDCP Task",
        REQUIRES            => [ PMU_SHA1, ],
        PROFILES            => [ 'pmu-gm10x', ],
        SRCFILES            => [ "task_hdcp.c"        ,
                                 "hdcp/hdcp_rpc.c"    ,
                                 "hdcp/pmu_auth.c"    ,
                                 "hdcp/pmu_bigint.c"  ,
                                 "hdcp/pmu_pvtbus.c"  ,
                                 "hdcp/pmu_upstream.c", ],
    },

    PMUTASK_ACR =>
    {
        DESCRIPTION         => "Acr",
        ENGINES_REQUIRED    => [ ACR, ],
        PROFILES            => [ 'pmu-gm2*', 'pmu-gp*', 'pmu-gv*', 'pmu-ga1*b*', 'pmu-ga10f_riscv', ],
        SRCFILES            => [ "task_acr.c", ],
    },


    PMUTASK_SPI =>
    {
        DESCRIPTION         => "PMU SPI Task",
        ENGINES_REQUIRED    => [ SPI, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "task_spi.c"         ,
                                 "spi/lw/pmu_objspi.c",
                                 "spi/lw/pmu_spirom.c", ],
    },

    PMUTASK__IDLE =>
    {
        DESCRIPTION         => "PMU Custom Idle Task for RISCV / SafeRTOS8",
        REQUIRES            => [ ARCH_RISCV, ],
        PROFILES            => [ 'pmu-*_riscv', ],
    },

    PMU_OS_CALLBACK_CENTRALISED =>
    {
        DESCRIPTION         => "Global switch used to enable centralized callbacks",
        PROFILES            => [ 'pmu-gp10x...', '-pmu-gv11b', ],
    },

    # Block Activity feature(s)

    PMU_BLOCK_ACTIVITY_ENABLED =>
    {
        DESCRIPTION         => "Set this top level feature only if BA is enabled on the chip/sku",
        ENGINES_REQUIRED    => [ THERM, PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    # PG Features

    PMU_PG_PMU_ONLY =>
    {
        DESCRIPTION         => "ELPG Pmu Only Feature",
        REQUIRES            => [ PMUTASK_LPWR, ],
        ENGINES_REQUIRED    => [ PG, LPWR, ],
        PROFILES            => [ 'pmu-*', '-pmu-gp100', '-pmu-gv10*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_LPWR_GRP_GR =>
    {
        DESCRIPTION         => "LPWR GR Group Feature",
        REQUIRES            => [ PMUTASK_LPWR, PMU_PG_PMU_ONLY, ],
        ENGINES_REQUIRED    => [ PG, LPWR, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-gv10*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_LPWR_GRP_MS =>
    {
        DESCRIPTION         => "LPWR MS Group Feature",
        REQUIRES            => [ PMUTASK_LPWR, PMU_PG_PMU_ONLY, ],
        ENGINES_REQUIRED    => [ PG, LPWR, ],
        PROFILES            => [ 'pmu-*', '-pmu-gp100', '-pmu-gv10*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_PG_GR =>
    {
        DESCRIPTION         => "LPWR_ENG(GR) LPWR state machine. It manages GR-ELPG and GFX-RPPG features.",
        REQUIRES            => [ PMU_LPWR_GRP_GR, ],
        ENGINES_REQUIRED    => [ GR, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-gv10*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_PG_GR_RG =>
    {
        DESCRIPTION         => "LPWR_ENG(GR_RG) LPWR state machine. It manages GR Rail Gating feature.",
        REQUIRES            => [ PMU_LPWR_GRP_GR, ],
        ENGINES_REQUIRED    => [ GR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_CTRL_GR_PASSIVE =>
    {
        DESCRIPTION         => "GR_Passive LPWR HW state machine.",
        REQUIRES            => [ PMU_LPWR_GRP_GR, ],
        ENGINES_REQUIRED    => [ GR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PG_MS =>
    {
        DESCRIPTION         => "LPWR_ENG(MS) LPWR HW state machine. It manages MSCG feature.",
        REQUIRES            => [ PMU_LPWR_GRP_MS, ],
        ENGINES_REQUIRED    => [ MS, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gp100', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_CTRL_MS_LTC =>
    {
        DESCRIPTION         => "LPWR_ENG(MS_LTC) LPWR HW state machine. It manages MS_LTC feature.",
        REQUIRES            => [ PMU_LPWR_GRP_MS, ],
        ENGINES_REQUIRED    => [ MS, ],
        PROFILES            => [ 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', ],
    },

    PMU_LPWR_CTRL_MS_PASSIVE =>
    {
        DESCRIPTION         => "MS_Passive LPWR HW state machine.",
        REQUIRES            => [ PMU_LPWR_GRP_MS, ],
        ENGINES_REQUIRED    => [ MS, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_CTRL_EI =>
    {
        DESCRIPTION         => "EI: LPWR HW state machine. It manages EI feature.",
        REQUIRES            => [ PMU_PG_PMU_ONLY, PMUTASK_LPWR_LP, ],
        ENGINES_REQUIRED    => [ EI, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_CTRL_EI_PASSIVE =>
    {
        DESCRIPTION         => "EI_Passive LPWR HW state machine.",
        REQUIRES            => [ PMU_PG_PMU_ONLY, PMUTASK_LPWR_LP, ],
        ENGINES_REQUIRED    => [ EI, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_DIFR =>
    {
        DESCRIPTION         => "DIFR: This config feature will manages DIFR feature.",
        REQUIRES            => [ PMU_PG_PMU_ONLY, PMUTASK_LPWR_LP, ],
        ENGINES_REQUIRED    => [ DFPR, MS, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gh20x*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_CTRL_DFPR =>
    {
        DESCRIPTION         => "DIFR: This config feature will manage DIFR Layer 1 FSM",
        REQUIRES            => [ PMU_LPWR_DIFR, ],
        ENGINES_REQUIRED    => [ DFPR, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-g00x*', '-pmu-gh20x*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_CTRL_MS_DIFR_SW_ASR =>
    {
        DESCRIPTION         => "DIFR: This config feature will manage the DIFR SW ASR FSM",
        REQUIRES            => [ PMU_LPWR_GRP_MS, PMU_LPWR_DIFR, ],
        ENGINES_REQUIRED    => [ MS, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gh20x*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_CTRL_MS_DIFR_CG =>
    {
        DESCRIPTION         => "DIFR: This config feature will manage the DIFR CG FSM",
        REQUIRES            => [ PMU_LPWR_GRP_MS, PMU_LPWR_DIFR, ],
        ENGINES_REQUIRED    => [ MS, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gh20x*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_TEST =>
    {
        DESCRIPTION         => "LPWR test support.",
        # Lwrrently MS ODP related tests are only supported.
        # The below fields will require an update, adding tests related to other engines.
        REQUIRES            => [ PMU_PG_MS, PMUTASK_LPWR_LP, ],
        ENGINES_REQUIRED    => [ MS, ],
        # Supported only on ODP supported PMU profiles.
        PROFILES            => [ ],
    },

    PMU_PG_THRESHOLD_UPDATE =>
    {
        DESCRIPTION         => "Move idle and PPU threshold programming to PMU",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-gp100', '-pmu-gv10*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_PG_TASK_MGMT =>
    {
        DESCRIPTION         => "PG task thrashing/starvation detection and prevention feature",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_SAC_V1 =>
    {
        DESCRIPTION         => "LPWR sleep aware callback version 1",
        REQUIRES            => [ PMU_PG_TASK_MGMT, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv',  '-pmu-g00x_riscv', '-pmu-ga10x_riscv', '-pmu-ga10x_selfinit_riscv', '-pmu-ad10x_riscv', ],
    },

    PMU_LPWR_SAC_V2 =>
    {
        DESCRIPTION         => "LPWR sleep aware callback version 2",
        REQUIRES            => [ PMU_PG_TASK_MGMT, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
    },

    PMU_LPWR_IDLE_SNAP =>
    {
        DESCRIPTION         => "Idle-snap support",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-gv10*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_LPWR_LP_IDLE_SNAP_NOTIFICATION =>
    {
        DESCRIPTION         => "Idle-snap Notification support on LPWR_LP Task",
        REQUIRES            => [ PMUTASK_LPWR_LP, PMU_LPWR_IDLE_SNAP, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_ENG =>
    {
        DESCRIPTION         => "LPWR_ENG support",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_LPWR_WITH_PS35 =>
    {
        DESCRIPTION         => "Lpwr with PS3.5 is supported",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_DEBUG =>
    {
        DESCRIPTION         => "Debug infra for Lpwr",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
    },

    PMU_LPWR_CENTRALISED_CALLBACK =>
    {
        DESCRIPTION         => "Centralised LPWR callback",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-gp100', '-pmu-gv10*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_LPWR_UNIFIED_PRIV_FLUSH_SEQ =>
    {
        DESCRIPTION         => "Common GPU Priv Path Flush Sequence",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        ENGINES_REQUIRED    => [ LPWR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-gh20x*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL =>
    {
        DESCRIPTION         => "Unified Priv Blocker Infrastructure",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        ENGINES_REQUIRED    => [ LPWR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gh20x*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_LPWR_SEQ =>
    {
        DESCRIPTION         => "LPWR Sequencer Support",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-gv*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_FG_RPPG =>
    {
        DESCRIPTION         => "LPWR FG-RPPG Support",
        REQUIRES            => [ PMU_LPWR_SEQ ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_STATS =>
    {
        DESCRIPTION         => "LPWR Statistics Infrastructure Support",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ ],
    },

    PMU_LPWR_GRP_MUTUAL_EXCLUSION =>
    {
        DESCRIPTION         => "LPWR_GRP mutual exclusion policy",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        ENGINES_REQUIRED    => [ LPWR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PG_SFM_2 =>
    {
        DESCRIPTION         => "Sub feature Mask 2.0",
        REQUIRES            => [PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-gv10*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_LPWR_CLK_SLOWDOWN =>
    {
        DESCRIPTION         => "Clock Slow down feature",
        REQUIRES            => [ PMU_PG_MS, PMU_LPWR_CTRL_EI, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS =>
    {
        DESCRIPTION         => "Config Feature to temporary fix the compilation for Hopper developement",
        PROFILES            => [ 'pmu-gh100...', 'pmu-ad10b_riscv', ],
    },

    PMU_PG_STATS_64 =>
    {
        DESCRIPTION         => "64-bit ns PG stats for use by other features within the PMU.",
        REQUIRES            => [ PMUTASK_LPWR, ],
        ENGINES_REQUIRED    => [ PG, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_LPWR_COMPILATION_FIX_AD10B_PLUS =>
    {
        DESCRIPTION         => "Config Feature to temporary fix the compilation for ADA developement",
        PROFILES            => [ 'pmu-ad10b_riscv...', '-pmu-ad10x_riscv', '-pmu-gh100*' ],
    },

    # PG_ENG(GR) features
    PMU_GR_POWER_GATING =>
    {
        DESCRIPTION         => "GR-ELPG feature",
        REQUIRES            => [ PMU_PG_GR, ],
        PROFILES            => [ 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', 'pmu-gv11b', ],
    },

    PMU_GR_WAKEUP_PENDING =>
    {
        DESCRIPTION         => "Abort GR if wake-up pending in LPWR queue",
        REQUIRES            => [ PMU_PG_GR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_GR_UNBIND_IGNORE_WAR200428328 =>
    {
        DESCRIPTION         => "WAR200428328 that ignore unbind step in GR power gating",
        REQUIRES            => [ PMU_GR_POWER_GATING, ],
        PROFILES            => [ 'pmu-tu10x', 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PG_GR_WAR200124305_PPU_THRESHOLD =>
    {
        DESCRIPTION         => "PPU Threshold WAR",
        REQUIRES            => [ PMU_PG_GR, ],
        PROFILES            => [ 'pmu-gp10x', ],
    },

    PMU_GR_PERFMON_RESET_WAR =>
    {
        DESCRIPTION         => "WAR to reset PERFMON unit",
        REQUIRES            => [ PMU_GR_POWER_GATING, ],
        PROFILES            => [ 'pmu-tu10x', ],
    },

    # PG_ENG(DI) features
    PMU_DI_SEQUENCE_COMMON =>
    {
        DESCRIPTION         => "Deep Idle sequence managed by OBJDI",
        ENGINES_REQUIRED    => [ DI, ],
        PROFILES            => [ 'pmu-gm*', ],
    },

    PMU_DI_SEQ_SWITCH_IDLE_DETECTOR_CIRLWIT =>
    {
        DESCRIPTION         => "Specify if switching of idle detector circuit is required",
        REQUIRES            => [ PMU_DI_SEQUENCE_COMMON, ],
        PROFILES            => [ 'pmu-gm*', ],
    },

    PMU_MS_HALF_FBPA =>
    {
        DESCRIPTION         => "Specify if Half-Fbpa is enabled on the system",
        REQUIRES            => [ PMU_PG_MS, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_MS_RPG =>
    {
        DESCRIPTION         => "MS coupled RPG Support in PMU",
        REQUIRES            => [ PMU_PG_MS, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    # PG_ENG(MS) features
    PMU_MSCG_L2_CACHE_OPS =>
    {
        DESCRIPTION         => "L2 cache OPerations in MSCG entry sequence",
        REQUIRES            => [ PMUTASK_LPWR, ],
        ENGINES_REQUIRED    => [ PG, MS, ],
        PROFILES            => [ 'pmu-gm*', 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PG_SEC2 =>
    {
        DESCRIPTION         => "Support in PG LowPower features for SEC2 priv and FB blockers for SEC2 RTOS",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PG_GSP =>
    {
        DESCRIPTION         => "Support in LowPower features sequence for GSP priv and FB blockers for GSP",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_PG_GR_UNIFIED_POWER_SEQ =>
    {
        DESCRIPTION         => "Unified GR engine power sequencer",
        REQUIRES            => [ PMU_PG_GR, ],
        PROFILES            => [ 'pmu-gv11b', 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_PG_LOG =>
    {
        DESCRIPTION         => "PG LOG Feature",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-gv11b', 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ]
    },

    PMU_PG_WAR_BUG_2432734 =>
    {
        DESCRIPTION         => "SW WAR for bug 2432734 for IDLE SNAP. WAR implements idle snap interrupt for GR-FE as valid wake up event.",
        REQUIRES            => [ PMU_PG_GR, ],
        PROFILES            => [ 'pmu-gp10x', ],
    },

    PMU_PG_SEC2_RTOS_WAR_200089154 =>
    {
        DESCRIPTION         => "WAR 200089154 for SEC2 RTOS. WAR is resposible for waking GR and MS from SEC2 RTOS.",
        REQUIRES            => [ PMU_PG_SEC2, ],
        PROFILES            => [ 'pmu-gp10x', ],
    },

    PMU_PSI =>
    {
        DESCRIPTION         => "PSI (Phase State Indicator) Feature",
        REQUIRES            => [ PMUTASK_LPWR, ],
        ENGINES_REQUIRED    => [ PG, PSI, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gp100', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_PSI_FLAVOUR_LWRRENT_AWARE =>
    {
        DESCRIPTION         => "Current aware PSI",
        REQUIRES            => [ PMU_PSI,],
        ENGINES_REQUIRED    => [ PG, PMGR, ],
        PROFILES            => [ 'pmu-gm*', 'pmu-gp10x', ],
    },

    PMU_PSI_MULTIRAIL =>
    {
        DESCRIPTION         => "Multirail PSI Support",
        REQUIRES            => [ PMU_PSI,],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_PSI_I2C_INTERFACE =>
    {
        DESCRIPTION         => "I2C based PSI Support",
        REQUIRES            => [ PMU_PSI, PMUTASK_LPWR_LP, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gh20x*', '-pmu-g00x_riscv', ],
    },

    PMU_PSI_CTRL_GR =>
    {
        DESCRIPTION         => "ELPG Coupled PSI Feature",
        REQUIRES            => [ PMU_PSI, PMU_PG_GR, ],
        ENGINES_REQUIRED    => [ PG, GR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PSI_CTRL_MS =>
    {
        DESCRIPTION         => "MSCG Coupled PSI Feature",
        REQUIRES            => [ PMU_PSI, PMU_PG_MS, ],
        ENGINES_REQUIRED    => [ PG, MS, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gp100', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PSI_CTRL_DI =>
    {
        DESCRIPTION         => "DI Coupled PSI Feature",
        REQUIRES            => [ PMU_PSI, ],
        PROFILES            => [ 'pmu-gm*', ],
    },

    PMU_PSI_CTRL_PSTATE =>
    {
        DESCRIPTION         => "Pstate coupled PSI features",
        REQUIRES            => [ PMU_PSI_I2C_INTERFACE, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gh20x*', '-pmu-g00x_riscv', ],
    },

    PMU_PSI_FLAVOUR_AUTO =>
    {
        DESCRIPTION         => "Auto PSI Feature",
        REQUIRES            => [ PMU_PSI, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_PSI_WITH_SCI =>
    {
        DESCRIPTION         => "SCI based PSI",
        REQUIRES            => [ PMU_PSI, PMU_PGISLAND, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_PSI_PMGR_SLEEP_CALLBACK =>
    {
        DESCRIPTION         => "Current callwlation in PG",
        REQUIRES            => [ PMU_PSI_FLAVOUR_LWRRENT_AWARE, ],
        PROFILES            => [ 'pmu-gp10x', ],
    },

    PMU_GR_RPPG =>
    {
        DESCRIPTION         => "GR Coupled RPPG Feature",
        REQUIRES            => [ PMU_LPWR_SEQ, PMU_PG_GR, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],

    },

    PMU_PGISLAND_SCI_PMGR_GPIO_SYNC =>
    {
        DESCRIPTION         => "Synchronizing SCI GPIOs with PMGR GPIOs",
        REQUIRES            => [ PMU_PGISLAND, ],
        PROFILES            => [ 'pmu-gm*', 'pmu-gp10x', ],
    },

    PMU_PGISLAND_SCI_GPIO_FORCE_ILWALID_WAR_1877803 =>
    {
        DESCRIPTION         => "WAR to force disable sefl-wake interrupt",
        REQUIRES            => [ PMU_PGISLAND, ],
        PROFILES            => [ 'pmu-gp10x', ],
    },

    PMU_I2C_INT =>
    {
        DESCRIPTION         => "PMU-PMU I2C Interface",
        REQUIRES            => [ PMUTASK_I2C, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PG_IDLE_FLIPPED_RESET =>
    {
        DESCRIPTION         => "Reset IDLE_FLIPPED if FB traffic is generated only by PMU",
        REQUIRES            => [ PMU_PG_MS, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-gv*', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PG_SW_POWER_DOWN =>
    {
        DESCRIPTION         => "Feature related to SW power Down. Its used by DI.",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-gm20x', ],
    },

    # Clock Gating Features

    PMU_CG =>
    {
        DESCRIPTION         => "Clock Gating Feature",
        REQUIRES            => [ PMUTASK_LPWR, ],
        ENGINES_REQUIRED    => [ PG, ],
        PROFILES            => [ 'pmu-*', ],
        SRCFILES            => [ "lpwr/lw/lpwr_cg.c", ],
    },

    PMU_CG_ELCG =>
    {
        DESCRIPTION         => "Engine Level Clock Gating Feature",
        REQUIRES            => [ PMU_CG, ],
        ENGINES_REQUIRED    => [ PG, ],
        PROFILES            => [ 'pmu-gm20x...', ],
    },

    PMU_CG_GR_ELCG_OSM_WAR =>
    {
        DESCRIPTION         => "Disable GR-ELCG state machine before gating GPC clock from Root/OSM",
        REQUIRES            => [ PMU_PG_MS, PMU_CG, ],
        ENGINES_REQUIRED    => [ LPWR, ],
        PROFILES            => [ 'pmu-*', '-pmu-gp100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_MS_CG_STOP_CLOCK =>
    {
        DESCRIPTION         => "MSCG Clock Gating using the Stop Clock profile",
        REQUIRES            => [ PMU_PG_MS, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_CG_ELCG_WAR1603090_PRIV_ACCESS =>
    {
        DESCRIPTION         => "WAR1603090: Use priv access to change state of ELCG",
        REQUIRES            => [ PMU_CG_ELCG, ],
        PROFILES            => [ 'pmu-gp100', ],
    },

    PMU_PG_GR_WAR1696192_ELCG_FORCE_OFF =>
    {
        DESCRIPTION         => "WAR1696192: Force ELCG OFF during a brief time instant during ELPG entry and exit",
        REQUIRES            => [ PMU_CG_ELCG, ],
        PROFILES            => [ 'pmu-gp10x', ],
    },

    PMU_LPWR_HOLDOFF_WAR1748086_PRIV_ACCESS =>
    {
        DESCRIPTION         => "WAR1748086: Use priv access to change pre-emptive holdoff state",
        REQUIRES            => [ PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-gp10x', 'pmu-tu10x'],
    },

    PMU_LPWR_UNIFIED_POWER_SEQ_CONFIG =>
    {
        DESCRIPTION         => "Program the Power Seqeuncer Registers",
        REQUIRES            => [PMU_PG_PMU_ONLY, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_CHIP_INFO =>
    {
        DESCRIPTION         => "Populate the chip info for the GPU on which the PMU is running",
        PROFILES            => [ 'pmu-*', ],
    },

    # PG Island Features

    PMU_PGISLAND =>
    {
        DESCRIPTION         => "PG Island Feature",
        REQUIRES            => [ PMUTASK_LPWR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pg/maxwell/pmu_pgislandgm10x.c",
                                 "pg/maxwell/pmu_pgislandgm20x.c",
                                 "pg/maxwell/pmu_pgislandgm1xxga100.c",
                                 "pg/ampere/pmu_pgislandga10x.c", ],
    },

    # Adaptive Power Features

    PMU_AP =>
    {
        DESCRIPTION         => "Adaptive Power",
        REQUIRES            => [ PMU_LPWR_CENTRALISED_CALLBACK, ],
        ENGINES_REQUIRED    => [ PG, AP, ],
        PROFILES            => [ 'pmu-gm2*', 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', 'pmu-gv11b', ],
    },

    PMU_AP_CTRL_GR =>
    {
        DESCRIPTION         => "Adaptive GR controller",
        REQUIRES            => [ PMU_AP, PMU_PG_GR, PMU_PG_THRESHOLD_UPDATE, ],
        PROFILES            => [ 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', 'pmu-gv11b', ],
    },

    PMU_AP_CTRL_DI_DEEP_L1 =>
    {
        DESCRIPTION         => "Adaptive DeepL1 controller. Its used by DI.",
        REQUIRES            => [ PMU_AP, PMU_DI_SEQUENCE_COMMON, ],
        PROFILES            => [ 'pmu-gm20x', ],
    },

    PMU_AP_CTRL_MSCG =>
    {
        DESCRIPTION         => "Adaptive MSCG controller",
        REQUIRES            => [ PMU_AP, PMU_PG_MS, ],
        PROFILES            => [ ],
    },

    PMU_AP_WAR_INCREMENT_HIST_CNTR =>
    {
        DESCRIPTION         => "Simulates behavior as if we are incrementing Histogram HW counters",
        REQUIRES            => [ PMU_AP_CTRL_DI_DEEP_L1, ],
        PROFILES            => [ 'pmu-gm20x', ],
    },

    # DIDLE Features

    PMU_GCX_DIDLE =>
    {
        DESCRIPTION         => "DIDLE feature managed by GCX task. DIDLE includes GC4 and GC5.",
        REQUIRES            => [ PMUTASK_GCX, ],
        ENGINES_REQUIRED    => [ GCX, ],
        PROFILES            => [ 'pmu-gm*', ],
    },

    PMU_GC6_PLUS =>
    {
        DESCRIPTION         => "GC6 Plus",
        REQUIRES            => [ PMUTASK_LPWR, ],
        PROFILES            => [ 'pmu-*', '-pmu-gp100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_DIDLE_SSC =>
    {
        DESCRIPTION         => "Deep Idle/GC5 Sleep Synchronized to Content (DI-SSC)",
        REQUIRES            => [ PMU_GCX_DIDLE, PMU_PG_MS, ],
        PROFILES            => [ 'pmu-gm*', ],
    },

    PMU_DIDLE_OS =>
    {
        DESCRIPTION         => "Deep Idle/GC5 Opportunistic Sleep (DI-OS)",
        REQUIRES            => [ PMU_GCX_DIDLE, PMU_PG_MS, ],
        PROFILES            => [ 'pmu-gm*', ],
    },

    PMU_DIDLE_NAPLL_WAR_1371047 =>
    {
        DESCRIPTION         => "Special handling for NA-PLL powerup/powerdown sequence",
        REQUIRES            => [ PMU_GCX_DIDLE, ],
        PROFILES            => [ 'pmu-gm*', ],
    },

    # DEBUG Features

    PMUDBG_PRINT =>
    {
        DESCRIPTION         => "Enable High-Level PMU Print Macros",
        PROFILES            => [ 'pmu-*_riscv', ],
    },

    PMUDBG_OS_TRACE_REG =>
    {
        DESCRIPTION         => "Redirect PMU Debug Spew to osTraceReg",
        REQUIRES            => [ PMUDBG_PRINT, ],
        PROFILES            => [ ],
    },

    # SEQ Features

    PMU_SEQ_CONFIGURE_LWLINK_TRAFFIC =>
    {
        DESCRIPTION         => "Enable/disable support for configuring HSHUB/MMU for LWLink",
        REQUIRES            => [ PMUTASK_SEQ, ],
        ENGINES_REQUIRED    => [ SEQ, ],
        PROFILES            => [ 'pmu-gp100', 'pmu-gv10x', 'pmu-tu10*', ],
    },

    PMU_SEQ_SCRIPT_FROM_BUFFER =>
    {
        DESCRIPTION         => "Run the sequencer script from a buffer",
        REQUIRES            => [ PMUTASK_SEQ, ],
        ENGINES_REQUIRED    => [ SEQ, ],
        PROFILES            => [ '...pmu-tu10x', '-pmu-gp100', '-pmu-gv11b', ],
    },

    PMU_SEQ_UPDATE_TRP =>
    {
       DESCRIPTION          => "Enable/disable support to update tRP value (FBPA CONFIG0)",
       REQUIRES             => [ PMUTASK_SEQ, ],
       ENGINES_REQUIRED     => [ SEQ, ],
       PROFILES             => [ 'pmu-gv10x', ],
    },

    PMU_SEQ_SCRIPT_RPC_SUPER_SURFACE =>
    {
        DESCRIPTION         => "Feature to use the super surface and RPC framework to send/recieve sequencer script data",
        PROFILES            => [ 'pmu-gp10x...pmu-tu10x', ],
    },

    PMU_SEQ_SCRIPT_RPC_PRE_GP102 =>
    {
        DESCRIPTION         => "Feature to RPC framework to send/recieve sequencer script data for pre-GP102",
        PROFILES            => [ '...pmu-gp100', ],
    },

    # THERM Features

    PMU_THERM_SLCT =>
    {
        DESCRIPTION         => "Enable/disable support for the slowdown control",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_RPC_SLCT =>
    {
        DESCRIPTION         => "Enable/disable support for the SLCT RPC call",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_SLCT, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_RPC_SLCT_EVENT_TEMP_THRESHOLD_SET =>
    {
        DESCRIPTION         => "Enable/disable support for the SLCT_EVENT_TEMP_THRESHOLD_SET RPC call",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_SLCT, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_SW_SLOW =>
    {
        DESCRIPTION         => "Enable/disable support for the RM_PMU_SW_SLOW feature",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_SLCT, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmswslow.c", ],
    },

    PMU_THERM_HW_FS_SLOW_NOTIFICATION =>
    {
        DESCRIPTION         => "Enable/disable support for the HW failsafe slowdown notifications",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_SLCT, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmslct.c", ],
    },

    PMU_THERM_HWFS_CFG =>
    {
        DESCRIPTION         => "Enable/disable support for the HW failsafe slowdown control",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_SLCT, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmhwfs.c", ],
    },

    PMU_THERM_RPC_HWFS_EVENT_STATUS_GET =>
    {
        DESCRIPTION         => "Enable/disable support for the EVENT GET RPC call",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_SLCT, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_WAR_BUG_1357298 =>
    {
        DESCRIPTION         => "Excludes _ALERT_NEG1H from _ANY_HW_FS event tracking. See bugs 1357298 & 1373838 for more details.",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gm10x', ],
    },

    PMU_THERM_INTR =>
    {
        DESCRIPTION         => "Enable/disable support for THERM interrupt handling.",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmintr.c", ],
    },

    PMU_THERM_FAN =>
    {
        DESCRIPTION         => "Enable/disable basic fan control",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/pmu_fanctrl.c", ],
    },

    PMU_THERM_FAN_BOARDOBJGRP_SUPPORT =>
    {
        DESCRIPTION         => "Enable/disable support for Board Object Group handling.",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_FAN_2X_AND_LATER =>
    {
        DESCRIPTION         => "Enable/disable 2X+ fan control",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_BOARDOBJGRP_SUPPORT, PMU_THERM_FAN, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_FAN_3X =>
    {
        DESCRIPTION         => "Enable/disable 3X fan control",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_BOARDOBJGRP_SUPPORT, PMU_THERM_FAN, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_FAN_COOLER =>
    {
        DESCRIPTION         => "Enable/disable FAN_COOLER object which represents a fan cooler.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_2X_AND_LATER, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fancooler.c", ],
    },

    PMU_THERM_FAN_COOLER_ACTIVE =>
    {
        DESCRIPTION         => "Enable/disable FAN_COOLER object which represents a ACTIVE fan cooler.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_COOLER, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fancooler_active.c", ],
    },

    PMU_THERM_FAN_COOLER_ACTIVE_PWM =>
    {
        DESCRIPTION         => "Enable/disable FAN_COOLER object which represents a ACTIVE_PWM fan cooler.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_COOLER_ACTIVE, PMU_LIB_PWM_ACT_GET, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fancooler_active_pwm.c", ],
    },

    PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR =>
    {
        DESCRIPTION         => "Enable/disable FAN_COOLER object which represents a ACTIVE_PWM_TACH_CORR fan cooler.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_COOLER_ACTIVE_PWM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fancooler_active_pwm_tach_corr.c", ],
    },

    PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR_ACT_CORR =>
    {
        DESCRIPTION         => "Enable/disable an ACTIVE_PWM_TACH_CORR cooler's floor limit offset when used on fan sharing designs (Gemini)",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_COOLER_ACTIVE_PWM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_FAN_COOLER_HW_MAX_FAN =>
    {
        DESCRIPTION         => "Enable/disable HW MAXFAN feature.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_COOLER_ACTIVE_PWM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_FAN_COOLER_PMUMON =>
    {
        DESCRIPTION         => "Enable/disable FAN COOLER PMUMON functionality",
        REQUIRES            => [ PMU_THERM_FAN_COOLER, PMU_LIB_PMUMON, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-gh20x', '-pmu-gv11b', ],
        SRCFILES            => [ 'fan/lw/fancooler_pmumon.c', ],
    },

    PMU_THERM_FAN_POLICY =>
    {
        DESCRIPTION         => "Enable/disable FAN_POLICY object which represents a fan policy.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_2X_AND_LATER, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fanpolicy.c", ],
    },

    PMU_THERM_FAN_POLICY_V20 =>
    {
        DESCRIPTION         => "Enable/disable FAN_POLICY object which represents a V20 fan policy.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_POLICY, PMU_THERM_FAN_COOLER, ],
        CONFLICTS           => [ PMU_THERM_FAN_POLICY_V30, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ '...pmu-ga100', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fan2x/20/fanpolicy_v20.c", ],
    },

    PMU_THERM_FAN_POLICY_V30 =>
    {
        DESCRIPTION         => "Enable/disable FAN_POLICY object which represents a V30 fan policy.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_POLICY, ],
        CONFLICTS           => [ PMU_THERM_FAN_POLICY_V20, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fan3x/30/fanpolicy_v30.c", ],
    },

    PMU_THERM_FAN_POLICY_THERM_CHANNEL_USE =>
    {
        DESCRIPTION         => "Enable/Disable selecting temperature source in the fan policy.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_POLICY, PMU_THERM_THERM_SENSOR_2X, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM =>
    {
        DESCRIPTION         => "Enable/disable FAN_POLICY object which represents a IIR_TJ_FIXED_DUAL_SLOPE_PWM policy.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_2X_AND_LATER, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fanpolicy_iir_tj_fixed_dual_slope_pwm.c", ],
    },

    PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20 =>
    {
        DESCRIPTION         => "Enable/disable FAN_POLICY object which represents a V20 IIR_TJ_FIXED_DUAL_SLOPE_PWM policy.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_POLICY_V20, PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM, ],
        CONFLICTS           => [ PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ '...pmu-ga100', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fan2x/20/fanpolicy_iir_tj_fixed_dual_slope_pwm_v20.c", ],
    },

    PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 =>
    {
        DESCRIPTION         => "Enable/disable FAN_POLICY object which represents a V30 IIR_TJ_FIXED_DUAL_SLOPE_PWM policy.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_POLICY_V30, PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM, ],
        CONFLICTS           => [ PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fan3x/30/fanpolicy_iir_tj_fixed_dual_slope_pwm_v30.c", ],
    },

    PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP =>
    {
        DESCRIPTION         => "Enable/disable FAN_POLICY object which represents an IIR_TJ_FIXED_DUAL_SLOPE_PWM's FAN_STOP sub-policy.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM, PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X_THERM_PWR_CHANNEL_QUERY, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_FAN_ARBITER =>
    {
        DESCRIPTION         => "Enable/disable FAN_ARBITER object which represents a fan arbiter.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_3X, PMU_THERM_FAN_POLICY, PMU_THERM_FAN_COOLER, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fan3x/fanarbiter.c", ],
    },

    PMU_THERM_FAN_ARBITER_V10 =>
    {
        DESCRIPTION         => "Enable/disable FAN_ARBITER object which represents a V10 fan arbiter.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_ARBITER, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fan/lw/fan3x/10/fanarbiter_v10.c", ],
    },

    PMU_THERM_FAN_MULTI_FAN =>
    {
        DESCRIPTION         => "Enable/disable multi fan support.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_FAN_2X_AND_LATER, PMU_OS_CALLBACK_CENTRALISED, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_SENSOR =>
    {
        DESCRIPTION         => "Enable/disable Temperature Sensors",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/tempsim.c"   ,
                                 "therm/lw/thrmsensor.c", ],
    },

    PMU_THERM_BOARDOBJGRP_SUPPORT =>
    {
        DESCRIPTION         => "Enable/disable support for Board Object Group handling.",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_THERM_SENSOR_2X =>
    {
        DESCRIPTION         => "Enable/disable Thermal Sensor 2.0.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_THERM_DEVICE =>
    {
        DESCRIPTION         => "Enable/disable THERM_DEVICE object which represents a Thermal Device.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_SENSOR_2X, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmdevice.c", ],
    },

    PMU_THERM_THERM_DEVICE_LOAD =>
    {
        DESCRIPTION         => "Enable/disable THERM_DEVICE object load.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmdevice.c", ],
    },

    PMU_THERM_THERM_DEVICE_GPU =>
    {
        DESCRIPTION         => "Enable/disable THERM_DEVICE object which represents a GPU Thermal Device.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmdevice_gpu.c", ],
    },

    PMU_THERM_THERM_DEVICE_GPU_GPC_TSOSC =>
    {
        DESCRIPTION         => "Enable/disable THERM_DEVICE object which represents a GPU_GPC_TSOSC Thermal Device.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100...', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmdevice_gpu_gpc_tsosc.c", ],
    },

    PMU_THERM_THERM_DEVICE_GPU_SCI =>
    {
        DESCRIPTION         => "Enable/disable THERM_DEVICE object which represents a GPU_SCI Thermal Device.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmdevice_gpu_sci.c", ],
    },

    PMU_THERM_THERM_DEVICE_GPU_GPC_COMBINED =>
    {
        DESCRIPTION         => "Enable/disable THERM_DEVICE object which represents a GPU_GPC_COMBINED Thermal Device.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmdevice_gpu_gpc_combined.c", ],
    },

    PMU_THERM_THERM_DEVICE_GDDR6_X_COMBINED =>
    {
        DESCRIPTION         => "Enable/disable THERM_DEVICE object which represents a GDDR6_X_COMBINED Thermal Device.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE, PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY, PMU_THERM_FBFLCN_LOAD_DEPENDENCY, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmdevice_gddr6_x_combined.c", ],
    },

    PMU_THERM_THERM_DEVICE_I2C =>
    {
        DESCRIPTION         => "Enable/disable THERM_DEVICE object which represents a I2C Thermal Device.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE, PMU_I2C_INT, PMU_PMGR_I2C_DEVICE ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmdevice_i2c.c", ],
    },

    PMU_THERM_THERM_DEVICE_I2C_TMP411 =>
    {
        DESCRIPTION         => "Enable/disable THERM_DEVICE object which represents a I2C TMP411 Thermal Device.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE_I2C, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmdevice_i2c_tmp411.c", ],
    },

    PMU_THERM_THERM_DEVICE_HBM2_SITE =>
    {
        DESCRIPTION         => "Enable/disable THERM_DEVICE object which represents a HBM2_SITE Thermal Device.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE, PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-tu10x', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmdevice_hbm2_site.c",
                                 "fb/volta/pmu_fbhbm2gv10x.c"     ,
                                 "fb/volta/pmu_fbhbm2gv10xonly.c" ,
                                 "fb/ampere/pmu_fbhbm2ga100.c"    , ],
    },

    PMU_THERM_THERM_DEVICE_HBM2_COMBINED =>
    {
        DESCRIPTION         => "Enable/disable THERM_DEVICE object which represents a HBM2_COMBINED Thermal Device.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE, PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-tu10x', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmdevice_hbm2_combined.c",
                                 "fb/volta/pmu_fbhbm2gv10x.c"         ,
                                 "fb/volta/pmu_fbhbm2gv10xonly.c"     ,
                                 "fb/ampere/pmu_fbhbm2ga100.c"        , ],
    },

    PMU_THERM_THERM_DEVICE_TEMPERATURE_OFFSET =>
    {
        DESCRIPTION         => "Enable/disable temperature offset callwlation.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE, PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY, PMU_THERM_THERM_DEVICE_HBM2_SITE, PMU_THERM_THERM_DEVICE_HBM2_COMBINED, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_THERM_CHANNEL =>
    {
        DESCRIPTION         => "Enable/disable THERM_CHANNEL object which represents a Thermal Channel.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_SENSOR_2X, PMU_THERM_THERM_DEVICE, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmchannel.c", ],
    },

    PMU_THERM_THERM_CHANNEL_DEVICE =>
    {
        DESCRIPTION         => "Enable/disable THERM_CHANNEL object which represents a DEVICE class Thermal Channel.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_CHANNEL, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmchannel_device.c", ],
    },

    PMU_THERM_THERM_CHANNEL_PMUMON =>
    {
        DESCRIPTION         => "Enable/disable THERM_CHANNEL PMUMON functionality",
        REQUIRES            => [ PMU_THERM_THERM_CHANNEL, PMU_LIB_PMUMON, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-gh20x', '-pmu-gv11b', ],
        SRCFILES            => [ 'therm/lw/thrmchannel_pmumon.c', ],
    },

    PMU_THERM_THERM_POLICY =>
    {
        DESCRIPTION         => "Enable/disable THERM_POLICY object which represents a Thermal Policy.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_SENSOR_2X, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmpolicy.c", ],
    },

    PMU_THERM_THERM_POLICY_DOMGRP =>
    {
        DESCRIPTION         => "Enable/disable THERM_POLICY_DOMGRP Thermal Policy class.",
        REQUIRES            => [ PMU_THERM_THERM_POLICY, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gm20x', ],
        SRCFILES            => [ "therm/lw/thrmpolicy_domgrp.c", ],
    },

    PMU_THERM_THERM_POLICY_DTC =>
    {
        DESCRIPTION         => "Enable/disable THERM_POLICY_DTC Thermal Policy class.",
        REQUIRES            => [ PMU_THERM_THERM_POLICY, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmpolicy_dtc.c", ],
    },

    PMU_THERM_THERM_POLICY_DTC_VF =>
    {
        DESCRIPTION         => "Enable/disable THERM_POLICY_DTC_VF Thermal Policy class.",
        REQUIRES            => [ PMU_THERM_THERM_POLICY_DOMGRP, PMU_THERM_THERM_POLICY_DTC, PMU_PERF_ELW_PSTATES_2X, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gm20x', ],
        SRCFILES            => [ "therm/lw/thrmpolicy_dtc_vf.c", ],
    },

    PMU_THERM_THERM_POLICY_DTC_VOLT =>
    {
        DESCRIPTION         => "Enable/disable THERM_POLICY_DTC_VOLT Thermal Policy class.",
        REQUIRES            => [ PMU_THERM_THERM_POLICY_DOMGRP, PMU_THERM_THERM_POLICY_DTC, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gm20x', ],
        SRCFILES            => [ "therm/lw/thrmpolicy_dtc_volt.c", ],
    },

    PMU_THERM_THERM_POLICY_DTC_PWR =>
    {
        DESCRIPTION         => "Enable/disable THERM_POLICY_DTC_PWR Thermal Policy class.",
        REQUIRES            => [ PMU_THERM_THERM_POLICY_DTC, PMU_PMGR_PWR_POLICY_SEMAPHORE, PMU_PMGR_PWR_POLICY_SEMANTIC_POLICY_TABLE, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmpolicy_dtc_pwr.c", ],
    },

    PMU_THERM_THERM_POLICY_DIAGNOSTICS =>
    {
        DESCRIPTION         => "Enable/disable thermal policy diagnostic features.",
        REQUIRES            => [ PMU_THERM_THERM_POLICY, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ad10b_riscv', ],
        SRCFILES            => [ "therm/lw/thrmpolicy_diagnostics.c", ],
    },

    PMU_THERM_THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN =>
    {
        DESCRIPTION         => "Enable/disable computation and reporting of the global minimum distance in degC from thermal capping.",
        REQUIRES            => [ PMU_THERM_THERM_POLICY_DIAGNOSTICS, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ad10b_riscv', ],
        SRCFILES            => [ "therm/lw/thrmpolicy_diagnostics_limit_countdown.c", ],
    },

    PMU_THERM_THERM_POLICY_DIAGNOSTICS_CAPPED =>
    {
        DESCRIPTION         => "Enable/disable reporting the temperature metrics causing thermal capping.",
        REQUIRES            => [ PMU_THERM_THERM_POLICY_DIAGNOSTICS, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ad10b_riscv', ],
        SRCFILES            => [ "therm/lw/thrmpolicy_diagnostics_capped.c", ],
    },

    PMU_THERM_THERM_MONITOR =>
    {
        DESCRIPTION         => "Enable/disable THERM_MONITOR object which represents a Thermal Monitor.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmmon.c", ],
    },

    PMU_THERM_THERM_MONITOR_WRAP_AROUND =>
    {
        DESCRIPTION         => "Enable/Disable THERM_MONITOR counter wraparound",
        REQUIRES            => [ PMU_THERM_THERM_MONITOR, ],
        PROFILES            => [ 'pmu-gh100...', ],
    },

    PMU_THERM_THERM_MONITOR_PATCH =>
    {
        DESCRIPTION         => "VBIOS DEVINIT patch for Thermal Monitor.",
        REQUIRES            => [ PMU_THERM_THERM_MONITOR, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga100', ],
    },

    PMU_THERM_PWR_SUPPLY_HOT_UNPLUG_SHUTDOWN =>
    {
        DESCRIPTION         => "Enable/disable power supply hot unplug shutdown",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ '...pmu-tu10x', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmintr.c", ],
    },

    PMU_THERM_PWR_SUPPLY_HOT_UNPLUG_SHUTDOWN_AUTO_CONFIG =>
    {
        DESCRIPTION         => "Enable/disable automatic configuration of power supply hot unplug shutdown",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_PWR_SUPPLY_HOT_UNPLUG_SHUTDOWN, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gp100...pmu-tu10x', '-pmu-gv11b', ],
    },

    PMU_THERM_SUPPORT_DEDICATED_OVERT =>
    {
        DESCRIPTION         => "DEDICATED_OVERT support",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_META_EVENT_ANY_EDP =>
    {
        DESCRIPTION         => "ANY_EDP meta-event support",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_VIOLATION_TIMERS_EXTENDED =>
    {
        DESCRIPTION         => "Enables the Event Violation feature to track 64-bit time [ns]",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_SLCT, PMU_THERM_INTR, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_VIOLATION_COUNTERS_EXTENDED =>
    {
        DESCRIPTION         => "Enables the Event Violation feature to track 64-bit counters",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_SLCT, PMU_THERM_INTR, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_RPC_TEST_EXELWTE =>
    {
        DESCRIPTION         => "Enable/disable support for exelwting Therm tests",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ad10x_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "therm/lw/thrmtest.c", ], # TODO: this is broken on Ada due to a hwref manuals change! Ilwestigation needed!
    },

    PMU_THERM_CLK_CNTR =>
    {
        DESCRIPTION         => "Clock counter commands in THERM task",
        REQUIRES            => [ PMUTASK_THERM, PMU_CLK_CNTR, ],
        ENGINES_REQUIRED    => [ THERM, CLK, ],
        PROFILES            => [ 'pmu-gm20x', ],
    },

    PMU_LIB_PWM_DEFAULT     =>
    {
        DESCRIPTION         => "Support for 'DEFAULT' PWMs.",
        PROFILES            => [ 'pmu-*', ],
    },

    PMU_LIB_PWM_TRIGGER     =>
    {
        DESCRIPTION         => "Support for 'TRIGGER' PWMs.",
        PROFILES            => [ 'pmu-tu10x...', ],
    },

    PMU_LIB_PWM_CONFIGURE =>
    {
        DESCRIPTION         => "Support for 'CONFIGURE' PWMs.",
        PROFILES            => [ 'pmu-tu10x...', ],
    },

    PMU_LIB_PWM_CLKSRC     =>
    {
        DESCRIPTION         => "Support for 'CLKSRC' PWMs.",
        PROFILES            => [ 'pmu-ga10x_riscv...', ],
    },

    PMU_LIB_PWM_ACT_GET =>
    {
        DESCRIPTION         => "Support for the PWM 'act get' interface.",
        PROFILES            => [ 'pmu-*', ],
    },

    PMU_THERM_BA_WAR_200433679 =>
    {
        DESCRIPTION         => "WAR for Bug 200433679 to control enable/disable of GPC & FBP BA's",
        REQUIRES            => [ PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ PMGR, THERM, ],
        PROFILES            => [ 'pmu-tu10x', ],
    },

    PMU_THERM_BA_DEBUG_INIT =>
    {
        DESCRIPTION         => "Initialize the BA debug interface",
        REQUIRES            => [ PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gv10x', 'pmu-tu10x', 'pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_THERM_INIT_CFG =>
    {
        DESCRIPTION         => "Used to configure basic thermal functionality within PMU",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_TASK_LOAD =>
    {
        DESCRIPTION         => "Used to load/unload features in THERM PMU task",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_OBJECT_LOAD =>
    {
        DESCRIPTION         => "Command sent to initiate the THERMAL load/unload process",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_MXM_OVERRIDE =>
    {
        DESCRIPTION         => "Applies MXM override to thermal POR settings",
        REQUIRES            => [ PMUTASK_THERM, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gp100...pmu-tu10x', '-pmu-gv11b', ],
    },

    PMU_THERM_DETACH_REQUEST =>
    {
        DESCRIPTION         => "Allow THERM task to process PMU DETACH command",
        REQUIRES            => [ PMUTASK_THERM, PMU_DETACH, ],
        # Ideally we would make this "CONFLICTS" with an ODP feature until http://lwbugs/2807666 is resolved.
        CONFLICTS           => [ PMU_HALT_ON_DETACH, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ '...pmu-tu10x', '-pmu-gv11b', ],
    },

    PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY =>
    {
        DESCRIPTION         => "Delay temperature reads from memory devices during suspend/resume.",
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-gv10x', 'pmu-ga100...', ],
    },

    PMU_THERM_FBFLCN_LOAD_DEPENDENCY =>
    {
        DESCRIPTION         => "GPU supports the THERM PMU's FBFLCN load dependency for reading GDDR6/GDDR6X thermal sensors.",
        REQUIRES            => [ PMUTASK_THERM, PMU_THERM_THERM_DEVICE_GDDR6_X_COMBINED, PMU_THERM_THERM_DEVICE_LOAD, ],
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_THERM_PMUMON_WAR_3076546 =>
    {
        DESCRIPTION         => "THERM_PMUMON SYS TSENSE Recovery functionality",
        ENGINES_REQUIRED    => [ THERM, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
        SRCFILES            => [ "therm/lw/therm_pmumon.c", ],
    },

    # SMBPBI Features

    PMU_SMBPBI =>
    {
        DESCRIPTION         => "Enable/disable SMBus PostBox Interface",
        REQUIRES            => [ PMUTASK_THERM, PMU_I2C_INT, ],
        ENGINES_REQUIRED    => [ SMBPBI, THERM, PERF, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "smbpbi/lw/smbpbi_rpc.c", ],
    },

    PMU_SMBPBI_ASYNC_REQUESTS =>
    {
        DESCRIPTION         => "Enable/disable SMBPBI asyncronous requests",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "smbpbi/lw/smbpbias.c", ],
    },

    PMU_SMBPBI_PG_RETIREMENT_STATS =>
    {
        DESCRIPTION         => "Enable/disable SMBPBI page retirement stats request",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_RRL_STATS =>
    {
        DESCRIPTION         => "Enable/disable SMBPBI row-remapping stats request",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_ACLWMULATE_UTIL_COUNTERS =>
    {
        DESCRIPTION         => "Enable/disable aclwmulation of util counters",
        REQUIRES            => [ PMU_PERFMON_TIMER_BASED_SAMPLING, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_LWLINK_INFO =>
    {
        DESCRIPTION         => "Enable/disable SMBPBI Lwlink info requests",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_LWLINK_INFO_AVAILABILITY =>
    {
        DESCRIPTION         => "Enable/disable SMBPBI Lwlink info availability requests",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_LWLINK_INFO_LINK_STATE_V2 =>
    {
        DESCRIPTION         => "Enable/disable SMBPBI Lwlink State V2 requests",
        REQUIRES            => [ PMU_SMBPBI, PMU_SMBPBI_LWLINK_INFO, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv',  '-pmu-gv11b', ],
    },

    PMU_SMBPBI_LWLINK_INFO_SUBLINK_WIDTH =>
    {
        DESCRIPTION         => "Enable/disable SMBPBI Lwlink Sublink Width requests",
        REQUIRES            => [ PMU_SMBPBI, PMU_SMBPBI_LWLINK_INFO, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv',  '-pmu-gv11b', ],
    },

    PMU_SMBPBI_LWLINK_INFO_THROUGHPUT =>
    {
        DESCRIPTION         => "Enable/disable SMBPBI Lwlink Throughput requests",
        REQUIRES            => [ PMU_SMBPBI, PMU_SMBPBI_LWLINK_INFO, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv',  '-pmu-gv11b', ],
    },

    PMU_SMBPBI_FW_WRITE_PROTECT_MODE =>
    {
        DESCRIPTION         => "Enable/disable firmware write protection mode SMBPBI request",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_CLOCK_FREQ_INFO =>
    {
        DESCRIPTION         => "Enable/disable clock frequency info requests",
        REQUIRES            => [ PMU_SMBPBI, PMU_PERF_ELW_PSTATES_35, ], # This should be working for PS35+
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_CLOCK_LWRRENT_PSTATE =>
    {
        DESCRIPTION         => "Enable/disable current pstate query",
        REQUIRES            => [ PMU_SMBPBI, PMU_PERF_CHANGE_SEQ], # This should be working for PS35+
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_OBD_VERSION_2 =>
    {
        DESCRIPTION         => "Enable/disable this feature to use OBD v2 in inforom",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_PCIE_INFO =>
    {
        DESCRIPTION         => "Enable/disable this PCIE speed and bandwidth info query",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_GET_PCIE_LINK_INFO =>
    {
        DESCRIPTION         => "Enable/disable this PCIE link info query",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "smbpbi/turing/pcieinfotu10x.c", ],
    },

    PMU_SMBPBI_GET_PCIE_LINK_TARGET_SPEED =>
    {
        DESCRIPTION         => "Enable/disable this PCIE link target speed query",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_TGP_LIMIT =>
    {
        DESCRIPTION         => "Enable/disable this TGP limit query",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_BUNDLED_REQUESTS =>
    {
        DESCRIPTION         => "Enable/disable handling of bundled requests",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "smbpbi/lw/smbpbi_bundle.c", ],
    },

    PMU_SMBPBI_DRIVER_EVENT_MSG =>
    {
        DESCRIPTION         => "Enable/disable handling of Driver Event Messages",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "smbpbi/lw/smbpbi_dem.c", ],
    },

    PMU_SMBPBI_HOSTMEM_SUPER_SURFACE =>
    {
        DESCRIPTION         => "Enable using the inforom on supersurface, instead of embedded pointer",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ ],
    },

    PMU_FSP_RPC =>
    {
        DESCRIPTION         => "Collection of RPCs used to communicate with FSP",
        REQUIRES            => [ PMU_SMBPBI, ],
        ENGINES_REQUIRED    => [ SMBPBI, ],
        PROFILES            => [ 'pmu-gh100_riscv', ],
        SRCFILES            => ['cmdmgmt/lw/cmdmgmt_queue_fsp.c',
                                'smbpbi/lw/smbpbi_fsp_rpc.c'],
    },

    # PMGR Features

    PMU_PMGR_REQ_LIB_FXP =>
    {
        DESCRIPTION         => "Helper feature indicating whether PMGR task requires PMU_LIB_FXP, and whether .libFxp should be attached to PMGR task at init.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_LIB_FXP, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_32BIT_OVERFLOW_FIXES =>
    {
        DESCRIPTION         => "Fix required to prevent possible overflow in power tuple values.",
        REQUIRES            => [ PMUTASK_PMGR, ],
        CONFLICTS           => [ PMU_PMGR_PWR_MONITOR_1X, PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_REGULATOR_LOSS_EST, PMU_PMGR_PWR_POLICY_WORKLOAD, PMU_PMGR_PWR_POLICY_BANG_BANG_VF, PMU_PMGR_PWR_POLICY_MARCH, ], # Marking features which we are leaving unsafe for overflow. In case of a build-conflict, please search using keyword: 32BIT_OVERFLOW
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_PMGR_AVG_EFF_FREQ =>
    {
        DESCRIPTION         => "Enable/disable PMGR support for querying average effective frequency.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_CLK_CNTR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ ],
    },

    PMU_PMGR_BOARDOBJGRP_SUPPORT =>
    {
        DESCRIPTION         => "Enable/disable support for Board Object Group handling.",
        REQUIRES            => [ PMUTASK_PMGR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_ACC =>
    {
        DESCRIPTION         => "Enable/disable support for PMGR aclwmulator library.",
        REQUIRES            => [ PMUTASK_PMGR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pmgr_acc.c", ],
    },

    PMU_PMGR_I2C_DEVICE =>
    {
        DESCRIPTION         => "Enable/disable I2C_DEVICE functionality which represents a secondary device on I2C bus.",
        REQUIRES            => [ PMUTASK_PMGR, PMUTASK_I2C, PMU_I2C_INT, PMU_PMGR_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ PMGR, I2C, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/i2cdev.c", ],
    },

    PMU_PMGR_I2C_DEVICE_INA219 =>
    {
        DESCRIPTION         => "Enable/disable I2C_DEVICE functionality which represents a INA219 I2C device.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_I2C_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/i2cdev_ina219.c", ],
    },

    PMU_PMGR_I2C_DEVICE_STM32F051K8U6TR =>
    {
        DESCRIPTION         => "Enable/disable I2C_DEVICE functionality which represents a MLW device on the GPU.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_I2C_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/i2cdev_stm32f051k8u6tr.c", ],
    },

    PMU_PMGR_I2C_DEVICE_STM32L031G6U6TR =>
    {
        DESCRIPTION         => "Enable/disable I2C_DEVICE functionality which represents a MLW device on the SLI bridge.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_I2C_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/i2cdev_stm32l031g6u6tr.c", ],
    },

    PMU_PMGR_PWR_DEVICE =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE object which represents a power sensor.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-gv11b', '-pmu-ad10b_riscv', ],
        SRCFILES            => [ "pmgr/lw/pwrdev.c", ],
    },

    PMU_PMGR_PWR_DEVICE_INA219 =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE object which represents an INA219 power sensor.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_DEVICE, PMU_PMGR_I2C_DEVICE_INA219, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_ina219.c", ],
    },

    PMU_PMGR_PWR_DEVICE_INA3221 =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE object which represents an INA3221 power sensor.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_DEVICE, PMU_PMGR_I2C_DEVICE_INA219, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_ina3221.c", ],
    },

    PMU_PMGR_PWR_DEVICE_NCT3933U =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE object which represents an NCT3933U power controller.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_DEVICE, PMU_PMGR_I2C_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gv10x', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_nct3933u.c", ],
    },

    PMU_PMGR_PWR_LWRRENT_COLWERSION =>
    {
        DESCRIPTION         => "Enable/disable Power to Current limit colwersion.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_NCT3933U, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gv10x', ],
    },

    PMU_PMGR_PWR_DEVICE_BA =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE object which represents BA-device-independent HW state.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_DEVICE, PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_DEVICE_BA_SWASR_WAR_BUG_1168927 =>
    {
        DESCRIPTION         => "SW WAR to disable BA before entering MSCG to allow XBAR to go idle.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_BA, PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm10x', ],
    },

    PMU_PMGR_PWR_DEVICE_BA1XHW =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE objects that represent BA1XHW power sensors.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_BA, PMU_PMGR_REQ_LIB_FXP, PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ '...pmu-ga10x_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_ba1xhw.c"         ,
                                 "therm/kepler/pmu_thrm_pwr_ba1x.c", ],
    },

    PMU_PMGR_PWR_DEVICE_BA12X =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE objects that represent BA12HW power sensors.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_BA1XHW, PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm*', ],
        SRCFILES            => [ "therm/maxwell/pmu_thrm_pwr_ba12.c", ],
    },

    PMU_PMGR_PWR_DEVICE_BA13X =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE objects that represent BA13HW power sensors.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_BA1XHW, PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp*', ],
        SRCFILES            => [ "therm/pascal/pmu_thrm_pwr_ba13.c", ],
    },

    PMU_PMGR_PWR_DEVICE_BA14 =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE objects that represent BA14HW power sensors with energy aclwmulation.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_BA13X, PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER, PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp10x', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_ba14hw.c"         ,
                                 "therm/pascal/pmu_thrm_pwr_ba14.c", ],
    },

    PMU_PMGR_PWR_DEVICE_BA15 =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE objects that represent BA15HW power sensors.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_BA1XHW, PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gv10x...pmu-ga100', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_ba15hw.c"        ,
                                 "therm/volta/pmu_thrm_pwr_ba15.c", ],
    },

    PMU_PMGR_PWR_DEVICE_BA16 =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE objects that represent BA16HW power sensors.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_BA1XHW, PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_ba16hw.c" ,],
    },

    PMU_PMGR_PWR_DEVICE_BA20 =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE objects that represent BA20 power sensors.",
        REQUIRES            => [ PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_ba20.c" ,],
    },

    PMU_PMGR_PWR_DEVICE_BA20_FBVDD_MODE =>
    {
        DESCRIPTION         => "Enable/disable BA20 power sensor operation in FBVDD mode.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_BA20, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ad10b_riscv', '-pmu-gh100*', ],
    },

    PMU_PMGR_PWR_DEVICE_2X =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE functionality to read tuple of power, current and voltage in one go.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_DEVICE_GPUADC1X =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE object which represents an on-chip GPUADC 1.X.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_2X, PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER, PMU_PMGR_ACC, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_gpuadc1x.c", ],
    },

    PMU_PMGR_PWR_DEVICE_GPUADC10 =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE object which represents an on-chip GPUADC 1.0.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_2X, PMU_PMGR_PWR_DEVICE_GPUADC1X, PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...pmu-ga100', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_gpuadc10.c", ],
    },

    PMU_PMGR_PWR_DEVICE_GPUADC10_COARSE_OFFSET =>
    {
        DESCRIPTION         => "Enable/disable VCM Coarse Offset support on GPUADC10",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_GPUADC10, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga100', ],
    },

    PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT =>
    {
        DESCRIPTION         => "Enable/disable beacon interrupt functionality",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_2X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_DEVICE_GPUADC11 =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE object which represents an on-chip GPUADC 1.1.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_2X, PMU_PMGR_PWR_DEVICE_GPUADC1X, PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER, PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ad10x_riscv...', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_gpuadc11.c", ],
    },

    PMU_PMGR_PWR_DEVICE_GPUADC13 =>
    {
        DESCRIPTION         => "Enable/disable PWR_DEVICE object which represents an on-chip GPUADC 1.3.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_2X, PMU_PMGR_PWR_DEVICE_GPUADC1X, PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER, PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gh100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrdev_gpuadc13.c", ],
    },

    PMU_PMGR_PWR_DEVICE_VOLT_STATE_SYNC =>
    {
        DESCRIPTION         => "Enable/disable VOLT STATE Sync to PWR DEVICEs. Lwrrently requires for BA1.3+ Power Devices only.",
        REQUIRES            => [ PMU_PMGR_PWR_DEVICE_BA, PMU_VOLT, ],
        ENGINES_REQUIRED    => [ PMGR, VOLT, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_MONITOR =>
    {
        DESCRIPTION         => "Enable/disable PWR_MONITOR functionality which collects power readings on a set of channels.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrchannel.c",
                                 "pmgr/lw/pwrmonitor.c", ],
    },

    PMU_PMGR_PWR_MONITOR_SEMAPHORE =>
    {
        DESCRIPTION         => "Enable/disable protection of critical section of PWR_MONITOR tuple status from simultaneous access by PERF_CF and PMGR",
        REQUIRES            => [ PMU_PMGR_PWR_MONITOR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_MONITOR_LAZY_CONSTRUCT =>
    {
        DESCRIPTION         => "Enable/disable PWR_MONITOR lazy-construct from pmgrPwrMonitorIfaceModel10SetHeader_DEFAULT, as opposed to from TASK_PMGR post init.",
        REQUIRES            => [ PMU_PMGR_PWR_MONITOR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ '...pmu-gv10x', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_MONITOR_1X =>
    {
        DESCRIPTION         => "Enable/disable PWR_MONITOR v1.X functionality.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR, PMU_PMGR_PWR_MONITOR_LAZY_CONSTRUCT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm10x', ],
        SRCFILES            => [ "pmgr/lw/pwrchannel_1x.c",
                                 "pmgr/lw/pwrmonitor_1x.c", ],
    },

    PMU_PMGR_PWR_MONITOR_2X =>
    {
        DESCRIPTION         => "Enable/disable PWR_MONITOR functionality which collects power readings on a set of channels that specific to Power Topology Table 2.0 shipped on GM10X+.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrchannel_2x.c"    ,
                                 "pmgr/lw/3x/pwrchannel_est.c",
                                 "pmgr/lw/pwrmonitor_2x.c"    , ],
    },

    PMU_PMGR_PWR_MONITOR_20 =>
    {
        DESCRIPTION         => "Enable/disable PWR_MONITOR functionality which collects power readings on a set of channels that specific to Power Topology Table 2.0 shipped on GM10X+.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR_2X, PMU_PMGR_PWR_MONITOR_LAZY_CONSTRUCT, ],
        CONFLICTS           => [ PMU_PMGR_PWR_CHANNEL_35, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ '...pmu-gv10x', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_CHANNEL_35 =>
    {
        DESCRIPTION         => "Enable/disable PWR_CHANNEL_35 functionality which supports devices with aclwmulators for power readings.",
        REQUIRES            => [ PMU_PMGR_PWR_MONITOR_2X, ],
        CONFLICTS           => [ PMU_PMGR_PWR_MONITOR_20, PMU_PMGR_PWR_MONITOR_LAZY_CONSTRUCT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrmonitor_35.c",
                                 "pmgr/lw/pwrchannel_35.c", ]
    },

    PMU_PMGR_PWR_MONITOR_2X_REDUCED_CHANNEL_STATUS =>
    {
        DESCRIPTION         => "Enable/disable decreasing LW2080_CTRL_PMGR_PWR_TUPLE array size to half to reduce stack usage on GM10X",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR_2X, PMU_PMGR_PWR_POLICY_3X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm10x', ],
    },

    PMU_PMGR_PWR_MONITOR_2X_MIN_CLIENT_SAMPLE_PERIOD =>
    {
        DESCRIPTION         => "Enable/disable returning cached power tuple if requests from RM are faster than the minimum client sampling period",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR_2X, PMU_PMGR_PWR_POLICY_3X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ '...pmu-gv10x', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_MONITOR_2X_READABLE_CHANNEL =>
    {
        DESCRIPTION         => "Enable/disable PWR_MONITOR functionality to call pwrChannelTupleStatusGet only for readable channels",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR_2X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_MONITOR_TOPOLOGY =>
    {
        DESCRIPTION         => "Enable/disable PWR_MONITOR functionality specific to Power Topology Table 1.0 shipped on MAXWELL+",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrchannel_sensor.c",
                                 "pmgr/lw/pwrchannel_sum.c"   ,
                                 "pmgr/lw/pwrchrel.c"         ,
                                 "pmgr/lw/pwrchrel_weight.c"  , ],
    },

    PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT =>
    {
        DESCRIPTION         => "Enable/disable PWR_CHANNEL_PSTATE_ESTIMATION_LUT functionality",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR_2X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrchannel_pstate_est_lut.c", ],
    },

    PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT_DYNAMIC =>
    {
        DESCRIPTION         => "Enable/disable Dynamic LUT functionality for PWR_CHANNEL_PSTATE_ESTIMATION_LUT channel",
        REQUIRES            => [ PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-gv11b', '-pmu-ad10b_riscv', ],
    },

    PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_BALANCED_PHASE_EST =>
    {
        DESCRIPTION         => "Enable/disable PWR_CHANNEL_RELATIONSHIP_BALANCED_PHASE_EST functionality.",
        REQUIRES            => [ PMU_PMGR_PWR_MONITOR_TOPOLOGY, PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrchrel_balanced_phase_est.c", ],
    },

    PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_BALANCING_PWM_WEIGHT =>
    {
        DESCRIPTION         => "Enable/disable PWR_CHANNEL_RELATIONSHIP_BALANCING_PWM_WEIGHT functionality.",
        REQUIRES            => [ PMU_PMGR_PWR_MONITOR_TOPOLOGY, PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrchrel_balancing_pwm_weight.c", ],
    },

    PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_REGULATOR_LOSS_EST =>
    {
        DESCRIPTION         => "Enable/disable PWR_CHANNEL_RELATIONSHIP_REGULATOR_LOSS_EST functionality.",
        REQUIRES            => [ PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm*', ],
        SRCFILES            => [ "pmgr/lw/pwrchrel_regulator_loss_est.c", ],
    },

    PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_REGULATOR_EFF_EST_V1 =>
    {
        DESCRIPTION         => "Enable/disable PWR_CHANNEL_RELATIONSHIP_REGULATOR_EFFICIENCY_EST_V1 functionality.",
        REQUIRES            => [ PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrchrel_regulator_eff_est_v1.c", ],
    },

    PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X =>
    {
        DESCRIPTION         => "Enable/disable PWR_MONITOR functionality specific to Power Topology Table 2.0 shipped on GM10X+",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR_2X, PMU_PMGR_PWR_MONITOR_TOPOLOGY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrchrel_2x.c"      ,
                                 "pmgr/lw/pwrchrel_weighted.c", ],
    },

    PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X_THERM_PWR_CHANNEL_QUERY =>
    {
        DESCRIPTION         => "Enable/disable PWR_MONITOR functionality of THERM/FAN task PWR_CHANNEL query that is specific to Power Topology Table 2.0 shipped on GM10X+",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_MONITOR_LOW_POWER_POLLING =>
    {
        DESCRIPTION         => "Enable/disable PWR_MONITOR low power polling period.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        SRCFILES            => [ "pmgr/lw/pwrmonitor.c", ],
        PROFILES            => [ 'pmu-gm*', ],
    },

    PMU_PMGR_PWR_CHANNEL_SENSOR_CLIENT_ALIGNED =>
    {
        DESCRIPTION         => "Enable/disable PWR_CHANNEL_SENSOR_CLIENT_ALIGNED class specific to Power Topology Table 2.0",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X, PMU_PMGR_ACC, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrchannel_sensor_client_aligned.c", ],
    },

    PMU_PMGR_PWR_EQUATION =>
    {
        DESCRIPTION         => "Enable/disable PWR_EQUATION functionality",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrequation.c", ],
    },

    PMU_PMGR_PWR_EQUATION_LEAKAGE =>
    {
        DESCRIPTION         => "Enable/disable PWR_EQUATION_LEAKAGE functionality",
        REQUIRES            => [ PMU_PMGR_PWR_EQUATION, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrequation_leakage.c", ],
    },

    PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS11 =>
    {
        DESCRIPTION         => "Enable/disable PWR_EQUATION_LEAKAGE_DTCS11 functionality",
        REQUIRES            => [ PMU_PMGR_PWR_EQUATION_LEAKAGE, PMU_PMGR_REQ_LIB_FXP, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrequation_leakage_dtcs11.c", ],
    },

    PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS12 =>
    {
        DESCRIPTION         => "Enable/disable PWR_EQUATION_LEAKAGE_DTCS12 functionality",
        REQUIRES            => [ PMU_PMGR_PWR_EQUATION_LEAKAGE, PMU_PMGR_REQ_LIB_FXP, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrequation_leakage_dtcs12.c", ],
    },

    PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS13 =>
    {
        DESCRIPTION         => "Enable/disable PWR_EQUATION_LEAKAGE_DTCS13 functionality",
        REQUIRES            => [ PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS12, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrequation_leakage_dtcs13.c", ],
    },

    PMU_PMGR_PWR_EQUATION_BA1X_SCALE =>
    {
        DESCRIPTION         => "Enable/disable PWR_EQUATION_BA1x_SCALE functionality",
        REQUIRES            => [ PMU_PMGR_PWR_EQUATION, PMU_PMGR_REQ_LIB_FXP, PMU_BLOCK_ACTIVITY_ENABLED, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ '...pmu-gp10x', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrequation_ba1x_scale.c", ],
    },

    PMU_PMGR_PWR_EQUATION_DYNAMIC =>
    {
        DESCRIPTION         => "Enable/disable PWR_EQUATION_DYNAMIC functionality",
        REQUIRES            => [ PMU_PMGR_PWR_EQUATION, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrequation_dynamic.c", ],
    },

    PMU_PMGR_PWR_EQUATION_DYNAMIC_10 =>
    {
        DESCRIPTION         => "Enable/disable PWR_EQUATION_DYNAMIC 1.0 functionality",
        REQUIRES            => [ PMU_PMGR_PWR_EQUATION_DYNAMIC, PMU_PMGR_REQ_LIB_FXP, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrequation_dynamic_10.c", ],
    },

    PMU_PMGR_PWR_EQUATION_BA15_SCALE =>
    {
        DESCRIPTION         => "Enable/disable PWR_EQUATION_BA15_SCALE functionality",
        REQUIRES            => [ PMU_PMGR_PWR_EQUATION, PMU_PMGR_REQ_LIB_FXP, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrequation_ba15_scale.c", ],
    },

    PMU_PMGR_PWR_POLICY =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY functionality",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR, PMU_PMGR_BOARDOBJGRP_SUPPORT, PMU_BOARDOBJ_VTABLE, PMU_BOARDOBJ_INTERFACE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy.c"  ,
                                 "pmgr/lw/pwrpolicies.c", ],
    },

    PMU_PMGR_PWR_POLICY_LAZY_CONSTRUCT =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY lazy-construct from pmgrPwrPolicyIfaceModel10SetHeader, as opposed to from TASK_PMGR post init.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ '...pmu-gv10x', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_2X =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY 2X functionality",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, PMU_PMGR_PWR_POLICY_LAZY_CONSTRUCT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm10x', ],
        SRCFILES            => [ "pmgr/lw/2x/pwrpolicy_2x.c"  ,
                                 "pmgr/lw/2x/pwrpolicies_2x.c", ],
    },

    PMU_PMGR_PWR_POLICY_3X =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY 3X functionality",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/3x/pwrpolicy_3x.c"  ,
                                 "pmgr/lw/3x/pwrpolicies_3x.c", ],
    },

    PMU_PMGR_PWR_POLICY_3X_SLEEP_RELAXED =>
    {
        DESCRIPTION         => "Enable/disable SLEEP_RELAXED functionality in PWR_POLICY_3X",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_3X, PMU_PMGR_CALLBACKS_CENTRALIZED, PMU_LPWR_SAC_V2, PMU_PMGR_CALLBACKS_CENTRALIZED, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
    },

    PMU_PMGR_PWR_POLICY_30 =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY 3X functionality",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_3X, PMU_PMGR_PWR_POLICY_LAZY_CONSTRUCT, ],
        CONFLICTS           => [ PMU_PMGR_PWR_POLICY_35, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ '...pmu-gv10x', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/3x/pwrpolicy_30.c"  ,
                                 "pmgr/lw/3x/pwrpolicies_30.c", ],
    },

    PMU_PMGR_PWR_POLICY_35 =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY 35 functionality",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_3X, PMU_PMGR_PWR_POLICY_3X_ONLY, ],
        CONFLICTS           => [ PMU_PMGR_PWR_POLICY_30, PMU_PMGR_PWR_POLICY_LAZY_CONSTRUCT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/3x/pwrpolicy_35.c"  ,
                                 "pmgr/lw/3x/pwrpolicies_35.c", ],
    },

    PMU_PMGR_PWR_POLICY_35_FILTER_PAYLOAD_PERF_CF_CONTROLLERS_REDUCED_STATUS =>
    {
        DESCRIPTION         => "Enable/disable filter payload to include perfCf controllers status",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_35, PMU_PERF_CF_CONTROLLERS_2X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_LIB =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY shared function library",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/lib_pmgr.c", ],
    },

    PMU_PMGR_PWR_POLICY_3X_ONLY =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY 3X functionality excluding GM10X series as it uses both _2X and _3X",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, PMU_PMGR_PWR_POLICY_3X, ],
        CONFLICTS           => [ PMU_PMGR_PWR_POLICY_2X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_CALLBACKS_CENTRALIZED =>
    {
        DESCRIPTION         => "Enable/disable centralized callbacks on PMGR task with power policy 3X functionality",
        REQUIRES            => [ PMU_OS_CALLBACK_CENTRALISED, PMU_PMGR_PWR_POLICY_3X_ONLY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_SEMAPHORE =>
    {
        DESCRIPTION         => "Enable/disable protection of critical section of PWR_POLICY from simultaneous access by THERM and PMGR",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_SEMANTIC_POLICY_TABLE =>
    {
        DESCRIPTION         => "Enable/disable the power policy table & API that translates LW2080 semantic indices to power policy boardObjGrp indices",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_SMBPBI_PWR_POLICY_INDEX_ENABLED =>
    {
        DESCRIPTION         => "Enable/disable querying power using SMBPBI policy index",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gp100', '-pmu-gv11b', ],
    },

    PMU_PMGR_TAMPER_DETECT_HANDLE_2X =>
    {
        DESCRIPTION         => "Enable/disable tamper detection and handling functionality for PWR_DEVICE ",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_3X, PMU_PMGR_PWR_DEVICE_2X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm10x', ],
    },

    PMU_PMGR_PWR_POLICY_BANG_BANG_VF =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_BANG_BANG_VF power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_DOMGRP, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm*', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_bangbang.c", ],
    },

    PMU_PMGR_PWR_POLICY_DOMGRP =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_DOMGRP power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, PERF, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_domgrp.c", ],
    },

    PMU_PMGR_PWR_POLICY_MARCH =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_MARCH power policy interface.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm*', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_march.c", ],
    },

    PMU_PMGR_PWR_POLICY_MARCH_VF =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_MARCH_VF power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_MARCH, PMU_PMGR_PWR_POLICY_DOMGRP, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm10x', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_march_vf.c", ],
    },

    PMU_PMGR_PWR_POLICY_LIMIT =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_LIMIT power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_limit.c", ],
    },

    PMU_PMGR_PWR_POLICY_PROP_LIMIT =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_PROP_LIMIT power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_LIMIT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_proplimit.c", ],
    },

    PMU_PMGR_PWR_POLICY_TOTAL_GPU =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_TOTAL_GPU power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_LIMIT, PMU_PMGR_PWR_POLICY_DOMGRP, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_totalgpu_iface.c",
                                 "pmgr/lw/pwrpolicy_totalgpu.c"      , ],
    },

    PMU_PMGR_PWR_POLICY_SHARED_WORKLOAD =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_WORKLOAD_SHARED power policy workload library.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_DOMGRP, PMU_PMGR_PWR_EQUATION, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_workload_shared.c", ],
    },

    PMU_PMGR_PWR_POLICY_WORKLOAD =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_WORKLOAD power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_DOMGRP, PMU_PMGR_PWR_EQUATION, PMU_PMGR_PWR_POLICY_SHARED_WORKLOAD, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm*', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_workload.c", ],
    },

    PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_WORKLOAD_MULTIRAIL power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_DOMGRP, PMU_PMGR_PWR_EQUATION, PMU_PMGR_PWR_POLICY_SHARED_WORKLOAD, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_workload_multirail_iface.c",
                                 "pmgr/lw/pwrpolicy_workload_multirail.c"      , ],
    },

    PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_PERF_CF_PWR_MODEL class.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_PWR_MONITOR, PMU_PMGR_PWR_POLICY_35, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_perf_cf_pwr_model.c", ],
    },

    PMU_PMGR_PWR_POLICY_DOMGRP_DATA_MAP =>
    {
        DESCRIPTION         => "Enable/disable the Domain Group Data map & APIs that use it",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_SINGLE_WORKLOAD_1X =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_WORKLOAD_SINGLE_1X power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, PMU_PMGR_PWR_EQUATION_DYNAMIC, PMU_PMGR_PWR_EQUATION_LEAKAGE,  PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL, PMU_PERF_CHANGE_SEQ, PMU_PMGR_PWR_POLICY_DOMGRP_DATA_MAP, PMU_VOLT_VOLT_RAIL_CLK_DOMAINS_PROG_MASK, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_workload_single_1x.c", ],
    },

    PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_WORKLOAD_COMBINED_1X power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_DOMGRP, PMU_PMGR_PWR_POLICY_SINGLE_WORKLOAD_1X, PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL, PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_workload_combined_1x.c", ],
    },

    PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL =>
    {
        DESCRIPTION         => "Enable/disable LWDCLK optimization for COMBINED_1X power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X, PMU_PMGR_PWR_POLICY_35_FILTER_PAYLOAD_PERF_CF_CONTROLLERS_REDUCED_STATUS, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_BIDIRECTIONAL_SEARCH =>
    {
        DESCRIPTION         => "Enable/disable bidirectional search in WORKLOAD_MULTIRAIL evaluation.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE =>
    {
        DESCRIPTION         => "Enable/disable using sensed voltage in WORKLOAD_MULTIRAIL evaluation.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD, PMU_VOLT_VOLTAGE_SENSED, PMU_PERF_ADC_DEVICES_ALWAYS_ON, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_PG_RESIDENCY =>
    {
        DESCRIPTION         => "Enable/disable using sensed voltage in WORKLOAD_MULTIRAIL evaluation.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD, PMU_PG_STATS_64, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_HW_THRESHOLD =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_HW_THRESHOLD power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_LIMIT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_hwthreshold.c", ],
    },

    PMU_PMGR_PWR_POLICY_BALANCE =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_BALANCE power policy class.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy_balance.c", ],
    },

    PMU_PMGR_PWR_POLICY_BALANCE_VIOLATION_AWARE =>
    {
        DESCRIPTION         => "Enable/disable the feature to make BALANCE power policy aware of power violations.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_BALANCE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_RELATIONSHIP =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_RELATIONSHIP functionality",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicyrel.c", ],
    },

    PMU_PMGR_PWR_POLICY_RELATIONSHIP_LOAD =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_RELATIONSHIP LOAD functionality",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicyrel.c", ],
    },

    PMU_PMGR_PWR_POLICY_RELATIONSHIP_WEIGHT =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_RELATIONSHIP_WEIGHT functionality",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_RELATIONSHIP, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicyrel_weight.c", ],
    },

    PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE =>
    {
        DESCRIPTION         => "Enable/disable PWR_POLICY_RELATIONSHIP_BALANCE functionality",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_RELATIONSHIP, PMU_PMGR_PWR_POLICY_BALANCE, PMU_PMGR_PWR_POLICY_RELATIONSHIP_LOAD, PMU_PMGR_PWR_POLICY_PROP_LIMIT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicyrel_balance.c", ],
    },

    PMU_PMGR_PWR_VIOLATION =>
    {
        DESCRIPTION         => "Enable/disable PWR_VIOLATION functionality",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_RELATIONSHIP, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrviolation.c", ],
    },

    PMU_PMGR_PWR_VIOLATION_PROPGAIN =>
    {
        DESCRIPTION         => "Enable/disable PWR_VIOLATION_PROPGAIN functionality",
        REQUIRES            => [ PMU_PMGR_PWR_VIOLATION, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrviolation_propgain.c", ],
    },

    PMU_PMGR_PWR_POLICY_INTEGRAL_CONTROL =>
    {
        DESCRIPTION         => "Enable/disable Integral Control functionality in PMU_PMGR_PWR_POLICY",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pwrpolicy.c", ],
    },

    PMU_PMGR_PWR_POLICY_LWRRENT_AWARE =>
    {
        DESCRIPTION         => "Enable/disable current awarness in PMU_PMGR_PWR_POLICY",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_DOMGRP_PMU_PERF_LIMITS =>
    {
        DESCRIPTION         => "Enable/disable current awarness in PMU_PMGR_PWR_POLICY",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_DOMGRP, PMU_PERF_PERF_LIMIT, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_GPUMON_PWR =>
    {
        DESCRIPTION         => "Enable/disable power sample logging using GPUMON",
        REQUIRES            => [ PMU_LOGGER, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_GPUMON_PWR_3X =>
    {
        DESCRIPTION         => "Enable/disable GPUMON for chips which use power policy 3.x logging",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_3X, PMU_PMGR_PWR_POLICY_SEMANTIC_POLICY_TABLE, PMU_PMGR_GPUMON_PWR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_I2C_HW =>
    {
        DESCRIPTION         => "Enable/disable HW I2C in PMU",
        REQUIRES            => [ PMU_PMGR_I2C_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "i2c/lw/pmu_hwi2c.c", ],
    },

    PMU_PMGR_I2C_400KHZ =>
    {
        DESCRIPTION         => "Enable/disable 400KHZ I2C functionality",
        REQUIRES            => [ PMU_PMGR_I2C_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_I2C_300KHZ =>
    {
        DESCRIPTION         => "Enable/disable 300KHZ I2C functionality",
        REQUIRES            => [ PMU_PMGR_I2C_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_I2C_200KHZ =>
    {
        DESCRIPTION         => "Enable/disable 200KHZ I2C functionality",
        REQUIRES            => [ PMU_PMGR_I2C_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_I2C_WAR_BUG_200125155 =>
    {
        DESCRIPTION         => "Enable/disable WAR for bug 200125155",
        REQUIRES            => [ PMU_PMGR_I2C_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp100', ],
    },

    PMU_PMGR_I2C_EDDC_SUPPORT_HW_I2C =>
    {
        DESCRIPTION         => "Enable/disable EDDC Support in PMU HWI2C",
        REQUIRES            => [ PMU_PMGR_I2C_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_I2C_POLICY_FOLLOW =>
    {
        DESCRIPTION         => "Enable/disable, follow RM I2c policy",
        REQUIRES            => [ PMU_PMGR_I2C_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_INPUT_FILTERING =>
    {
        DESCRIPTION         => "Enable/disable input filtering functionality in PMU_PMGR_PWR_POLICY",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_3X, PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/3x/pwrpolicy_3x.c", ],
    },

    PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER =>
    {
        DESCRIPTION         => "Enable/disable, energy aclwmulation for channels",
        REQUIRES            => [ PMU_PMGR_PWR_MONITOR_TOPOLOGY_2X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_3X_TGP_MULTI_INFLECTIONS =>
    {
        DESCRIPTION         => "Enable/disable, multiple inflection points for TGP class",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY_3X, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    # Tariq-TODO: Remove this feature and move all profiles to use dmem_pmgr
    PMU_PMGR_PWR_POLICY_ALLOCATE_FROM_OS_HEAP =>
    {
        DESCRIPTION         => "Enable/disable allocating memory from OS heap",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ '...pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE =>
    {
        DESCRIPTION         => "Enable/disable support for disabling inflection points.",
        REQUIRES            => [ PMU_PMGR_PWR_POLICY, PMU_PMGR_PWR_POLICY_DOMGRP, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PWR_CHANNELS_PMUMON =>
    {
        DESCRIPTION         => "Enable/disable, PWR_CHANNELS PMUMON functionality",
        REQUIRES            => [ PMU_PMGR_PWR_MONITOR, PMU_LIB_PMUMON, PMU_PMGR_PWR_POLICY_SEMANTIC_POLICY_TABLE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-gh20x', '-pmu-gv11b', ],
        SRCFILES            => [ 'pmgr/lw/pwrchannel_pmumon.c', ],
    },

    # SPI Features

    PMU_SPI_DEVICE =>
    {
        DESCRIPTION         => "Enable/disable SPI_DEVICE functionality which represents a device on SPI bus.",
        REQUIRES            => [ PMUTASK_SPI, ],
        ENGINES_REQUIRED    => [ SPI, ],
        PROFILES            => [ 'pmu-gm20x', 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "spi/lw/spidev.c", ],
    },

    PMU_SPI_DEVICE_ROM =>
    {
        DESCRIPTION         => "Enable/disable SPI_DEVICE functionality which represents a ROM",
        REQUIRES            => [ PMU_SPI_DEVICE, ],
        ENGINES_REQUIRED    => [ SPI, ],
        PROFILES            => [ 'pmu-gm20x', 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "spi/lw/spidev_rom.c", ],
    },

    # PERF Features

    PMU_PERF_ELW_PSTATES =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PSTATES environment. Primary feature, not associated with any particular version.",
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_ELW_PSTATES_2X =>
    {
        DESCRIPTION         => "Enable/disable PMU support for Perf Table Pstates prior to 3.0.  Must be kept in sync with RM PDB PSTATE_2X_SUPPORTED.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PERF_ELW_PSTATES, ],
        CONFLICTS           => [ PMU_PERF_ELW_PSTATES_3X, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gm*', ],
        SRCFILES            => [ "perf/lw/2x/perfps20.c", ],
    },

    PMU_PERF_ELW_PSTATES_3X =>
    {
        DESCRIPTION         => "Enable/disable PMU support for Pstates 3.0 and later.",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_ELW_PSTATES, PMU_PERF_BOARDOBJGRP_SUPPORT, ],
        CONFLICTS           => [ PMU_PERF_ELW_PSTATES_2X, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_ELW_PSTATES_30 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for Pstates 3.0.  Must be kept in sync with RM PDB PSTATE_30_SUPPORTED.",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES_3X, ],
        CONFLICTS           => [ PMU_PERF_ELW_PSTATES_35, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-tu10x...', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/perfps30.c", ],
    },

    PMU_PERF_ELW_PSTATES_35 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for Pstates 3.5.  Must be kept in sync with RM PDB PSTATE_35_SUPPORTED.",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES_3X, PMU_VOLT_35, ],
        CONFLICTS           => [ PMU_PERF_ELW_PSTATES_30, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_ELW_PSTATES_40 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for Pstates 4.0.  Must be kept in sync with RM PDB PSTATE_40_SUPPORTED.",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES_35, PMU_CLK_CLK_DOMAIN_40, PMU_VOLT_40, ],   # PS 4.0 is built on top of PS 3.5
        CONFLICTS           => [ PMU_PERF_ELW_PSTATES_30,  ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_PSTATES_35_DOMGRP =>
    {
        DESCRIPTION         => "Enable/disable PMU support for the legacy domain groups",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES_35, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/perfps35.c", ],
    },

    PMU_PERF_DOMAIN_GROUP_LIMITS_XBAR =>
    {
        DESCRIPTION         => "Enable/disable PMU support of XBAR Domain group limit",
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK =>
    {
        DESCRIPTION         => "Enable/disable PMU support of XBAR Domain group limit",
        REQUIRES            => [PMU_PMGR_PWR_POLICY_DOMGRP, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_ELW_PSTATES_40_TEST =>
    {
        DESCRIPTION         => "Feature meant to denote that these profiles are for testing only. NOT TO BE ENABLED FOR PRODUCTION PROFILES",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES_40, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_PSTATES =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PSTATE objects.",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/pstate.c", ],
    },

    PMU_PERF_PSTATES_INFRA =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PSTATE infrastructure required on top of base package.",
        REQUIRES            => [ PMU_PERF_PSTATES, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/pstate_infra.c", ],
    },

    PMU_PERF_PSTATES_3X =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PSTATE_3X objects.",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES_3X, PMU_PERF_PSTATE_BOARDOBJ_INTERFACES, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/pstate_3x.c", ],
    },

    PMU_PERF_PSTATES_3X_INFRA =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PSTATE_3X infrastructure required on top of base package.",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES_3X, PMU_PERF_PSTATE_BOARDOBJ_INTERFACES, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/pstate_3x_infra.c", ],
    },

    PMU_PERF_PSTATES_30 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PSTATE_30 objects.",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES_30, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-tu10x...', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/30/pstate_30.c", ],
    },

    PMU_PERF_PSTATES_35 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PSTATE_35 objects.",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES_35, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/35/pstate_35.c", ],
    },

    PMU_PERF_PSTATES_35_INFRA =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PSTATE 3.5 infrastructure required on top of base package.",
        REQUIRES            => [ PMU_PERF_PSTATES_INFRA, PMU_PERF_PSTATES_35, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/35/pstate_35_infra.c", ],
    },

    PMU_PERF_PSTATES_SELF_INIT =>
    {
        DESCRIPTION         => "Enable/disable self-initialization support for the PSTATES.",
        REQUIRES            => [ PMU_PERF_PSTATES_VBIOS, ],
        PROFILES            => [ 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', 'pmu-gb100_riscv...', ],
        SRCFILES            => [ "perf/lw/pstates_self_init.c", ],
    },

    PMU_PERF_PSTATES_VBIOS =>
    {
        DESCRIPTION         => "Enable/disable PSTATES VBIOS parsing support.",
        REQUIRES            => [ PMU_VBIOS_IMAGE_ACCESS, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        SRCFILES            => [ "perf/lw/vbios/pstates_vbios.c", ],
    },

    PMU_PERF_PSTATES_VBIOS_6X =>
    {
        DESCRIPTION         => "Enable/disable PSTATES VBIOS parsing support for Performance Table 6.X.",
        REQUIRES            => [ PMU_PERF_PSTATES_VBIOS, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        SRCFILES            => [ "perf/lw/vbios/pstates_vbios_6x.c", ],
    },

    PMU_PERF_PSTATE_BOARDOBJ_INTERFACES =>
    {
        DESCRIPTION         => "Enable/disable PMU support for BOARDOBJ interfaces on the PSTATE object.",
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_PERF_MODE =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PERF MODE object.",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES_3X, PMU_PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/perf_mode.c", ],
    },

    PMU_PERF_PERF_VPSTATES_20 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for Vpstates 2.0 generic accessors",
        REQUIRES            => [ PMU_PERF_ELW_PSTATES_2X, ],
        ENGINES_REQUIRED    => [ PERF, ],
        CONFLICTS           => [ PMU_PERF_VPSTATES, PMU_PERF_DOMAIN_GROUP_LIMITS_XBAR, ],
        PROFILES            => [ 'pmu-gm*', ],
        SRCFILES            => [ "perf/lw/2x/vpstate_2x.c", ],
    },

    PMU_PERF_VPSTATES =>
    {
        DESCRIPTION         => "Enable/disable PMU support for Vpstates",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ PERF, ],
        CONFLICTS           => [ PMU_PERF_PERF_VPSTATES_20, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/vpstate.c", ],
    },

    PMU_PERF_VPSTATES_3X =>
    {
        DESCRIPTION         => "Enable/disable PMU support for Vpstates 3.x",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_VPSTATES, ],    # Please add PMU_PERF_ELW_PSTATES_3X,
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/vpstate_3x.c", ],
    },

    PMU_PERF_DISABLE_PERF_LIMIT =>
    {
        DESCRIPTION         => "Explicitly disable PMU arbiter.",
        REQUIRES            => [ PMU_UCODE_PROFILE_AUTO, ],
        PROFILES            => [ ],
    },

    PMU_PERF_PERF_LIMIT =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PERF_LIMIT objects",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_BOARDOBJGRP_SUPPORT, PMU_PERF_ELW_PSTATES_35, PMU_PERF_VFE_PMU_EQU_MONITOR, PMU_PERF_PSTATES_35, ],
        ENGINES_REQUIRED    => [ PERF, ],
        CONFLICTS           => [ PMU_PERF_DISABLE_PERF_LIMIT, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/perf_limit.c",
                                 "perf/lw/perf_limit_client.c",
                                 "perf/lw/perf_limit_vf.c",
                               ],
    },

    PMU_PERF_PERF_LIMIT_35 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PERF_LIMIT_35 objects",
        REQUIRES            => [ PMU_PERF_PERF_LIMIT, PMU_PERF_VPSTATES_3X, PMU_VOLT_VOLT_RAIL_CLK_DOMAINS_PROG_MASK, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/35/perf_limit_35.c", ],
    },

    PMU_PERF_PERF_LIMIT_35_10 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PERF_LIMIT_35_10 objects",
        REQUIRES            => [ PMU_PERF_PERF_LIMIT_35, ],
        CONFLICTS           => [ PMU_PERF_PERF_LIMIT_40, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x', 'pmu-ga100', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/35/perf_limit_35_10.c", ],
    },

    PMU_PERF_PERF_LIMIT_35_10_TU10X_SET =>
    {
        DESCRIPTION         => "Enable/disable PMU support for TU10X set of PERF_LIMIT_35_10 perf limits",
        REQUIRES            => [ PMU_PERF_PERF_LIMIT_35_10, ],
        CONFLICTS           => [ PMU_PERF_PERF_LIMIT_40, PMU_PERF_PERF_LIMIT_35_10_GA100_SET, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_PERF_LIMIT_35_10_GA100_SET =>
    {
        DESCRIPTION         => "Enable/disable PMU support for GA100 set of PERF_LIMIT_35_10 perf limits",
        REQUIRES            => [ PMU_PERF_PERF_LIMIT_35_10, ],
        CONFLICTS           => [ PMU_PERF_PERF_LIMIT_40, PMU_PERF_PERF_LIMIT_35_10_TU10X_SET, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_PERF_PERF_LIMIT_40 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PERF_LIMIT_40 objects",
        REQUIRES            => [ PMU_PERF_PERF_LIMIT_35, ],
        CONFLICTS           => [ PMU_PERF_PERF_LIMIT_35_10, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/40/perf_limit_40.c", ],
    },

    PMU_PERF_POLICY =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PERF_POLICY objects",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_BOARDOBJGRP_SUPPORT, PMU_PERF_PERF_LIMIT, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/perfpolicy.c" ],
    },

    PMU_PERF_POLICY_35 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for PERF_POLICY_35 objects",
        REQUIRES            => [ PMU_PERF_POLICY, PMU_PERF_PERF_LIMIT_35, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/35/perfpolicy_35.c" ],
    },

    PMU_PERF_POLICY_PMUMON =>
    {
        DESCRIPTION         => "Enable/disable PERF POLICY PMUMON functionality",
        REQUIRES            => [ PMU_PERF_POLICY, PMU_LIB_PMUMON, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-gh20x', '-pmu-gv11b', ],
        SRCFILES            => [ 'perf/lw/perfpolicy_pmumon.c', ],
    },

    PMU_PERF_PSTATES_35_FMAX_AT_VMIN =>
    {
        DESCRIPTION         => "Feature to callwlate Vmin for all Volt domains and Fmax per pstate per clk domain",
        REQUIRES            => [ PMU_PERF_PSTATES_35, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    # VOLT Features

    PMU_VOLT_IN_PERF =>
    {
        DESCRIPTION         => "Enables handling of VOLT CMDs from within PERF task.",
        REQUIRES            => [ PMUTASK_PERF, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT_BOARDOBJGRP_SUPPORT =>
    {
        DESCRIPTION         => "Enable/disable support for Board Object Group handling.",
        REQUIRES            => [ PMUTASK_PERF, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT =>
    {
        DESCRIPTION         => "Enable/disable VOLT object.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "volt/lw/objvolt.c", ],
    },

    PMU_VOLT_30 =>
    {
        DESCRIPTION         => "Enable/disable VOLT 3.0 object.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        CONFLICTS           => [ PMU_VOLT_35, ],
        PROFILES            => [ 'pmu-gp100...pmu-gv10x', '-pmu-gv11b', ],
    },

    PMU_VOLT_35 =>
    {
        DESCRIPTION         => "Enable/disable VOLT 3.5 object.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT, PMU_VOLT_SEMAPHORE, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        CONFLICTS           => [ PMU_VOLT_30, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT_40 =>
    {
        DESCRIPTION         => "Enable/disable VOLT 4.0 object.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT, PMU_VOLT_SEMAPHORE, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT_VOLT_RAIL =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a Volt Rail.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "volt/lw/voltrail.c", ],
    },

    PMU_VOLT_VOLT_RAIL_OFFSET =>
    {
        DESCRIPTION         => "Enable/disable voltage offsets via voltage rails.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_RAIL, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        CONFLICTS           => [ PMU_VOLT_VOLT_POLICY_OFFSET, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT_VOLT_RAIL_VOLT_MARGIN_OFFSET =>
    {
        DESCRIPTION         => "Enable/disable voltage margin offsets via voltage rails.",
        REQUIRES            => [ PMU_VOLT_VOLT_RAIL_OFFSET, PMU_PERF_CHANGE_SEQ_VOLT_OFFSET_CHANGE, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT_VOLT_RAIL_ACTION =>
    {
        DESCRIPTION         => "Enable/disable support for voltage rail feature associated with control action for the rail.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_DEVICE, PMU_VOLT_40, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT_VOLT_RAILS_PMUMON =>
    {
        DESCRIPTION         => "Enable/disable, VOLT_RAILS PMUMON functionality",
        REQUIRES            => [ PMU_VOLT_VOLT_RAIL, PMU_LIB_PMUMON, PMU_PERF_CHANGE_SEQ, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-gh20x', '-pmu-gv11b', ],
        SRCFILES            => [ 'volt/lw/voltrail_pmumon.c', ],
    },

    PMU_VOLT_VOLT_RAIL_CLK_DOMAINS_PROG_MASK =>
    {
        DESCRIPTION         => "Enable/disable VOLT_RAIL::clkDomainsProgMask.",
        REQUIRES            => [ PMU_VOLT_VOLT_RAIL, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT_VOLT_RAIL_SCALING_PWR_EQUATION =>
    {
        DESCRIPTION         => "Enable/disable Scaling Power Equation tied to Volt Rail.",
        REQUIRES            => [ PMU_VOLT_VOLT_RAIL, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, PMGR, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT_VOLT_DEVICE =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a Volt Device.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_RAIL, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "volt/lw/voltdev.c", ],
    },

    PMU_VOLT_VOLT_DEVICE_VID =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a Wide VID Volt Device.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_DEVICE, PMU_LIB_GPIO, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100...', '-pmu-gv11b', ],
        SRCFILES            => [ "volt/lw/voltdev_vid.c", ],
    },

    PMU_VOLT_VOLT_DEVICE_PWM =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a PWM Serial VID Volt Device.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_DEVICE, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "volt/lw/voltdev_pwm.c", ],
    },

    PMU_VOLT_VOLT_DEVICE_PWM_LPWR =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a LPWR PWM Serial VID Volt Device.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_DEVICE_PWM, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp10x', ],
    },

    PMU_VOLT_VOLT_DEVICE_PWM_PGOOD =>
    {
        DESCRIPTION         => "Enable/disable PGOOD signal support for a given PWM voltage device.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_DEVICE_PWM, PMU_VOLT_VOLT_RAIL_ACTION, PMU_VOLT_40, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
    },

    PMU_VOLT_VOLT_DEVICE_PWM_WAR_BUG_200628767 =>
    {
        DESCRIPTION         => "Accommodate for the board level limitation in bug 200628767.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_DEVICE_PWM, PMU_VOLT_VOLT_RAIL_ACTION, PMU_VOLT_40, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
    },

    PMU_VOLT_VOLT_POLICY =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a Volt Policy.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_DEVICE, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "volt/lw/voltpolicy.c", ],
    },

    PMU_VOLT_VOLT_POLICY_SINGLE_RAIL =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a Single Rail Volt Policy.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_POLICY, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "volt/lw/voltpolicy_single_rail.c", ],
    },

    PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_MULTI_STEP =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a Single Rail Multi Step Volt Policy.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_POLICY_SINGLE_RAIL, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gv10x', ],
        SRCFILES            => [ "volt/lw/voltpolicy_single_rail_multi_step.c", ],
    },

    PMU_VOLT_VOLT_POLICY_SPLIT_RAIL =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a Split Rail Volt Policy.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_POLICY, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp*', ],
        SRCFILES            => [ "volt/lw/voltpolicy_split_rail.c", ],
    },

    PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_MULTI_STEP =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a Split Rail Multi Step Volt Policy.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_POLICY_SPLIT_RAIL, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp*', ],
        SRCFILES            => [ "volt/lw/voltpolicy_split_rail_multi_step.c", ],
    },

    PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a Split Rail Single Step Volt Policy.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_POLICY_SPLIT_RAIL, PMU_THERM_VID_PWM_TRIGGER_MASK, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-gp10x', ],
        SRCFILES            => [ "volt/lw/voltpolicy_split_rail_single_step.c", ],
    },

    PMU_VOLT_VOLT_POLICY_MULTI_RAIL =>
    {
        DESCRIPTION         => "Enable/disable VOLT object which represents a Multi Rail Volt Policy.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_POLICY, PMU_VOLT_40, PMU_THERM_VID_PWM_TRIGGER_MASK, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "volt/lw/voltpolicy_multi_rail.c", ],
    },

    PMU_VOLT_VOLT_POLICY_OFFSET =>
    {
        DESCRIPTION         => "Enable/disable voltage offsets via voltage policies.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT_VOLT_POLICY, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        CONFLICTS           => [ PMU_VOLT_VOLT_RAIL_OFFSET, ],
        PROFILES            => [ 'pmu-gp100...pmu-tu10x', '-pmu-gv11b', ],
    },

    PMU_VOLT_RPC_TEST_EXELWTE =>
    {
        DESCRIPTION         => "Enable/disable support for exelwting Volt tests",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "volt/lw/volttest.c", ],
    },

    PMU_VOLT_SEMAPHORE =>
    {
        DESCRIPTION         => "Enable/disable the support for VOLT read-write semaphore.",
        REQUIRES            => [ PMUTASK_PERF, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT_IPC_VMIN =>
    {
        DESCRIPTION         => "Enable/disable support for IPC Vmin.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT, PMU_LIB_PWM_CONFIGURE, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT_VOLTAGE_SENSED =>
    {
        DESCRIPTION         => "Enable/disable support for sensed voltage.",
        REQUIRES            => [ PMUTASK_PERF, PMU_VOLT, PMU_VOLT_VOLT_RAIL, PMU_PERF_ADC_DEVICES_ALWAYS_ON, ],
        ENGINES_REQUIRED    => [ PERF, VOLT, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VOLT_RAM_ASSIST =>
    {
        DESCRIPTION         => "Enable/disable support for RAM ASSIST.",
        REQUIRES            => [ PMU_VOLT_VOLT_RAIL, PMU_PERF_ADC_DEVICES_ALWAYS_ON, ],
        PROFILES            => [ 'pmu-ad10x*', 'pmu-gh100*', 'pmu-gb100_riscv', ],
    },

    PMU_PERF_SEMAPHORE =>
    {
        DESCRIPTION         => "Enable/disable the support for perf read-write semaphore.",
        REQUIRES            => [ PMUTASK_PERF, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_AVFS =>
    {
        DESCRIPTION         => "Enable/disable PMU support for AVFS",
        REQUIRES            => [ PMUTASK_PERF, PMU_CLK_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_BOARDOBJGRP_SUPPORT =>
    {
        DESCRIPTION         => "Enable/disable support for Board Object Group handling.",
        REQUIRES            => [ PMUTASK_PERF, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_VFE =>
    {
        DESCRIPTION         => "Enable/disable VFE feature.",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/vfe.c", ],
    },

    PMU_PERF_VFE_EXTENDED_RELWRSION_DEPTH =>
    {
        DESCRIPTION         => "Enable/disable VFE extended relwrsion depth feature.",
        REQUIRES            => [ PMU_PERF_VFE, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_VFE_35 =>
    {
        DESCRIPTION         => "Enable/disable generic VFE infrastructure for Pstates 3.5 feature.",
        REQUIRES            => [ PMU_PERF_VFE, PMU_PERF_ELW_PSTATES_35, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_VFE_VAR =>
    {
        DESCRIPTION         => "Enable/disable VFE_VAR object which represents a VFE Independent Variable.",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_VFE, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/vfe_var.c"                   ,
                                 "perf/lw/3x/vfe_var_derived.c"           ,
                                 "perf/lw/3x/vfe_var_derived_product.c"   ,
                                 "perf/lw/3x/vfe_var_derived_sum.c"       ,
                                 "perf/lw/3x/vfe_var_single.c"            ,
                                 "perf/lw/3x/vfe_var_single_frequency.c"  ,
                                 "perf/lw/3x/vfe_var_single_sensed.c"     ,
                                 "perf/lw/3x/vfe_var_single_sensed_temp.c",
                                 "perf/lw/3x/vfe_var_single_voltage.c"    , ],
    },

    PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_BASE =>
    {
        DESCRIPTION         => "Enable/disable VFE_VAR_SINGLE_SENSE_FUSE_BASE object",
        REQUIRES            => [ PMU_PERF_VFE_VAR, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/vfe_var_single_sensed_fuse_base.c", ],
    },

    PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_LEGACY =>
    {
        DESCRIPTION         => "Enable/disable VFE_VAR_SINGLE_SENSE_FUSE object",
        REQUIRES            => [ PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_BASE, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/vfe_var_single_sensed_fuse.c", ],
    },

    PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_BASE_EXTENDED =>
    {
        DESCRIPTION         => "Enable/disable VFE_VAR_SINGLE_SENSE_FUSE_BASE_EXTENDED member variable",
        REQUIRES            => [ PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_BASE, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ad10b_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_20 =>
    {
        DESCRIPTION         => "Enable/disable VFE_VAR_SINGLE_SENSE_FUSE_20 object",
        REQUIRES            => [ PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_BASE_EXTENDED, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ad10b_riscv', '-pmu-g00x_riscv', ],
        SRCFILES            => [ "perf/lw/3x/vfe_var_single_sensed_fuse_20.c", ],
    },

    PMU_PERF_VFE_VAR_35 =>
    {
        DESCRIPTION         => "Enable/disable additional VFE_VAR object which represents a VFE Independent Variable.",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_VFE, PMU_PERF_VFE_VAR, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/vfe_var_single_caller_specified.c"    , ],
    },

    PMU_PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED =>
    {
        DESCRIPTION         => "Enable/disable single globally specified VFE_VAR object which represents a VFE Independent Variable.",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_VFE, PMU_PERF_VFE_VAR_35, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/vfe_var_single_globally_specified.c"    , ],
    },

    PMU_PERF_VFE_EQU =>
    {
        DESCRIPTION         => "Enable/disable VFE_EQU object which represents a VFE Equation.",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_VFE, PMU_PERF_VFE_VAR, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/vfe_equ.c"             ,
                                 "perf/lw/3x/vfe_equ_compare.c"     ,
                                 "perf/lw/3x/vfe_equ_minmax.c"      ,
                                 "perf/lw/3x/vfe_equ_monitor.c"     ,
                                 "perf/lw/3x/vfe_equ_quadratic.c"   , ],
    },

    PMU_PERF_VFE_EQU_35 =>
    {
        DESCRIPTION         => "Enable/disable additional VFE_EQU object which represents a VFE Equation.",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_VFE, PMU_PERF_VFE_VAR, PMU_PERF_VFE_EQU, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/3x/vfe_equ_equation_scalar.c", ],
    },

    PMU_PERF_VFE_EQU_16BIT_INDEX =>
    {
        DESCRIPTION         => "Enables/disable VFE EQU index being 16-bit (if disabled VFE EQU index is 8-bit).",
        REQUIRES            => [ PMU_PERF_VFE_EQU, ],
        CONFLICTS           => [ PMU_CLK_CLK_PROG, ],
        ENGINES_REQUIRED    => [ PERF, CLK, VOLT, ],
        PROFILES            => [ 'pmu-gh100_riscv...', 'pmu-ad10x*', ],
    },

    PMU_PERF_VFE_EQU_BOARDOBJGRP_1024 =>
    {
        DESCRIPTION         => "Enables or disables SW having a max of 1024 VFE EQU objects.",
        REQUIRES            => [ PMU_PERF_VFE_EQU, PMU_PERF_VFE_EQU_16BIT_INDEX, PMU_BOARDOBJ_GRP_E1024, ],
        ENGINES_REQUIRED    => [ PERF, CLK, VOLT, ],
        PROFILES            => [ 'pmu-gh100_riscv...', 'pmu-ad10x*', ],
    },

    PMU_PERF_VFE_PMU_EQU_MONITOR =>
    {
        DESCRIPTION         => "Enable/disable VFE_EQU_MONITOR for use within PMU PERF code..",
        REQUIRES            => [ PMU_PERF_VFE_EQU, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_ADC_DEVICES =>
    {
        DESCRIPTION         => "Enable/disable PMU support for ADC Devices",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_AVFS, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc.c"         ,
                                 "clk/pascal/pmu_clkadcgp100.c"         ,
                                 "clk/pascal/pmu_clkadcgp10x.c"         ,
                                 "clk/pascal/pmu_clkadcgp10x_only.c"    ,
                                 "clk/pascal/pmu_clkadcgpxxx_ghxxx.c"   ,
                                 "clk/volta/pmu_clkadcgv10x.c"          ,
                                 "clk/ampere/pmu_clkadcga100.c"         ,
                                 "clk/ampere/pmu_clkadcga10x.c"         , ],
    },

    PMU_PERF_ADC_DEVICES_EXTENDED =>
    {
        DESCRIPTION         => "Enable/disable support of extended features for ADC Devices",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_extended.c"          ,
                                 "clk/pascal/pmu_clkadcgpxxx_tuxxx.c"    ,
                                 "clk/pascal/pmu_clkadcgp10x_extended.c" ,
                                 "clk/ampere/pmu_clkadcga100_extended.c" , ],
    },

    PMU_PERF_ADC_DEVICES_1X =>
    {
        DESCRIPTION         => "Enable/disable PMU support for ADC devices (group) version 1X",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100...', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadcs_1x.c", ],
    },

    PMU_PERF_ADC_DEVICES_2X =>
    {
        DESCRIPTION         => "Enable/disable PMU support for ADC devices (group) version 2X",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadcs_2x.c", ],
    },

    PMU_PERF_ADC_DEVICE_V10 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V1.0 ADC Devices",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-tu10x...', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_v10.c", ],
    },

    PMU_PERF_ADC_DEVICE_V20 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V2.0 ADC Devices",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_v20.c",  ],
    },

    PMU_PERF_ADC_DEVICE_V20_EXTENDED =>
    {
        DESCRIPTION         => "Enable/disable support of extended features specific to V2.0 ADC Devices",
        REQUIRES            => [ PMU_PERF_ADC_DEVICE_V20, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_v20_extended.c"  ,
                                 "clk/turing/pmu_clkadctuxxx_only.c" , ],
    },

    PMU_PERF_ADC_DEVICE_V30 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V30 ADC Devices",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_v30.c", ],
    },

    PMU_PERF_ADC_DEVICE_V30_EXTENDED =>
    {
        DESCRIPTION         => "Enable/disable support of extended features specific to V3.0 ADC Devices",
        REQUIRES            => [ PMU_PERF_ADC_DEVICE_V30, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_v30_extended.c", ],
    },

    PMU_PERF_ADC_DEVICE_ISINK =>
    {
        DESCRIPTION         => "Enable/disable PMU support for ISINK feature",
        REQUIRES            => [ PMU_PERF_ADC_DEVICE_V30, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x*', 'pmu-gb100_riscv...', '-pmu-ad10b_riscv', ],
    },

    PMU_PERF_ADC_DEVICE_V30_ISINK_V10 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for ISINK V10 ADC V30 Devices",
        REQUIRES            => [ PMU_PERF_ADC_DEVICE_ISINK, PMU_PERF_ADC_DEVICE_V30, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x*', 'pmu-gb100_riscv...', '-pmu-ad10b_riscv', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_v30_isink_v10.c", ],
    },

    PMU_PERF_ADC_DEVICE_V30_ISINK_V10_EXTENDED =>
    {
        DESCRIPTION         => "Enable/disable support of extended features specific to ISINK V10 ADC V30 Devices",
        REQUIRES            => [ PMU_PERF_ADC_DEVICE_V30_ISINK_V10, PMU_PERF_ADC_DEVICE_V30_EXTENDED, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x*', 'pmu-gb100_riscv...', '-pmu-ad10b_riscv', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_v30_isink_v10_extended.c", ],
    },

    PMU_PERF_ADC_STATIC_CALIBRATION =>
    {
        DESCRIPTION         => "Enable support for static ADC calibration gain, offset and coarse offset programming",
        REQUIRES            => [ PMU_PERF_ADC_HW_CALIBRATION, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp10x...pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-gh100*', '-pmu-gv11b', ],
    },

    PMU_PERF_ADC_RUNTIME_CALIBRATION =>
    {
        DESCRIPTION         => "Enable support for run-time ADC calibration offset programming",
        REQUIRES            => [ PMU_PERF_ADC_HW_CALIBRATION, PMU_PERF_ADC_DEVICES_2X, PMU_PERF_ADC_DEVICE_V30_EXTENDED, ],
        CONFLICTS           => [ PMU_PERF_ADC_TEMPERATURE_VARIATION_WAR_BUG_200423771, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x*', 'pmu-gb100_riscv...', '-pmu-ad10b_riscv', ],
    },

    PMU_PERF_ADC_TEMPERATURE_VARIATION_WAR_BUG_200423771 =>
    {
        DESCRIPTION         => "Support of high temperature variation SW WAR required only on TU102 for GPC5 ADC",
        REQUIRES            => [ PMU_PERF_ADC_DEVICE_V20_EXTENDED, ],
        CONFLICTS           => [ PMU_PERF_ADC_RUNTIME_CALIBRATION, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-tu10x', ],
    },

    CLK_ADC_ADDRESS_MAP_HAL_V10 =>
    {
        DESCRIPTION         => "Enable/disable PMU ADC register map HAL version V1.0",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        CONFLICTS           => [ CLK_ADC_ADDRESS_MAP_HAL_V20, CLK_ADC_ADDRESS_MAP_HAL_V30, CLK_ADC_ADDRESS_MAP_HAL_V40, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp10x', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_reg_map_10.c", ],
    },

    CLK_ADC_ADDRESS_MAP_HAL_V20 =>
    {
        DESCRIPTION         => "Enable/disable PMU ADC register map HAL version V2.0",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        CONFLICTS           => [ CLK_ADC_ADDRESS_MAP_HAL_V10, CLK_ADC_ADDRESS_MAP_HAL_V30, CLK_ADC_ADDRESS_MAP_HAL_V40, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gv10x', 'pmu-tu10x', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_reg_map_20.c", ],
    },

    CLK_ADC_ADDRESS_MAP_HAL_V30 =>
    {
        DESCRIPTION         => "Enable/disable PMU ADC register map HAL version V3.0",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        CONFLICTS           => [ CLK_ADC_ADDRESS_MAP_HAL_V10, CLK_ADC_ADDRESS_MAP_HAL_V20, CLK_ADC_ADDRESS_MAP_HAL_V40, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ad10x*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gb100_riscv...', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_reg_map_30.c", ],
    },

    CLK_ADC_ADDRESS_MAP_HAL_V40 =>
    {
        DESCRIPTION         => "Enable/disable PMU ADC register map HAL version V4.0",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        CONFLICTS           => [ CLK_ADC_ADDRESS_MAP_HAL_V10, CLK_ADC_ADDRESS_MAP_HAL_V20, CLK_ADC_ADDRESS_MAP_HAL_V30, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x*', 'pmu-gb100_riscv...', '-pmu-ad10b_riscv', ],
        SRCFILES            => [ "clk/lw/pmu_clkadc_reg_map_40.c", ],
    },

    PMU_CLK_ADC_LOGICAL_INDEXING =>
    {
        DESCRIPTION         => "Indicates if Logical Indexing is to be used for certain GPC Specific Registers",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ad10b_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_PERF_ADC_DEVICES_ALWAYS_ON =>
    {
        DESCRIPTION         => "Keep ADCs always ON after PMU ADC load.",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_ADC_ACC_BATCH_LATCH_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/disable to support batched latching/unlatching of ADC aclwmulators to avoid error in the sensed voltage readings for multiple ADCs on the same voltage rail",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES_EXTENDED, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC =>
    {
        DESCRIPTION         => "Support per ADC device HW register access sync",
        REQUIRES            => [ PMU_PERF_ADC_DEVICE_V30, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_ADCS_DMEM_DEFINED =>
    {
        DESCRIPTION         => "Support for separate ADC devices dmem overlay",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_ADC_IS_SENSED_VOLTAGE_PTIMER_BASED =>
    {
        DESCRIPTION         => "Indicates if the clock ADC sensed voltage is PTIMER based",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES_EXTENDED, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_ADC_RAM_ASSIST =>
    {
        DESCRIPTION         => "Indicates if the RAM Assist feature is enabled or not",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES_EXTENDED, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x*', 'pmu-gh100*', 'pmu-gb100_riscv', ],
    },

    PMU_PERF_NAFLL_DEVICES =>
    {
        DESCRIPTION         => "Enable/disable PMU support for NAFLL Devices",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_AVFS, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll.c"        ,
                                 "clk/pascal/pmu_clkavfsgp100.c",
                                 "clk/pascal/pmu_clkavfsgp10x.c",
                                 "clk/pascal/pmu_clkavfsgpxxx_gvxxx.c",
                                 "clk/volta/pmu_clkavfsgv100.c" ,
                                 "clk/turing/pmu_clkavfstu10x.c", ],
    },

    PMU_PERF_NAFLL_DEVICES_INIT =>
    {
        DESCRIPTION         => "Enable/disable PMU support for NAFLL Devices initialization.",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_AVFS, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-tu10x', 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', 'pmu-gh20x*' ],
        SRCFILES            => [ "clk/ampere/pmu_clkavfsga10x.c" , ],
    },

    PMU_PERF_NAFLL_DEVICE_V10 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V1.0 NAFLL",
        REQUIRES            => [ PMU_PERF_NAFLL_DEVICES, ],
        CONFLICTS           => [ PMU_PERF_NAFLL_DEVICE_V20, PMU_PERF_NAFLL_DEVICE_V30, ], # add PMU_CLK_CLKS_ON_PMU
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-tu10x...', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_10.c"        , ],
    },

    PMU_PERF_NAFLL_DEVICE_V20 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V2.0 NAFLL",
        REQUIRES            => [ PMU_PERF_NAFLL_DEVICES, ],
        CONFLICTS           => [ PMU_PERF_NAFLL_DEVICE_V10, PMU_PERF_NAFLL_DEVICE_V30, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100...', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_20.c"             ,
                                 "clk/turing/pmu_clkavfstuxxx_only.c"   , ],
    },

    PMU_PERF_NAFLL_DEVICE_V30 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V3.0 NAFLL",
        REQUIRES            => [ PMU_PERF_NAFLL_DEVICES, ],
        CONFLICTS           => [ PMU_PERF_NAFLL_DEVICE_V10, PMU_PERF_NAFLL_DEVICE_V20, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_30.c"             ,
                                 "clk/ampere/pmu_clkavfsga100.c"        , ],
    },

    PMU_PERF_NAFLL_LUT =>
    {
        DESCRIPTION         => "Enable/disable PMU support for NAFLL LUT programming",
        REQUIRES            => [ PMU_PERF_NAFLL_DEVICES, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_lut.c"     , ],
    },

    PMU_PERF_NAFLL_LUT_V10 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V1.0 NAFLL LUT programming.",
        REQUIRES            => [ PMU_PERF_NAFLL_LUT ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-tu10x...', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_lut_10.c"  , ],
    },

    PMU_PERF_NAFLL_LUT_V20 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V2.0 NAFLL LUT programming.",
        REQUIRES            => [ PMU_PERF_NAFLL_LUT ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100...', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_lut_20.c"  , ],
    },

    PMU_PERF_NAFLL_LUT_V30 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V3.0 NAFLL LUT programming.",
        REQUIRES            => [ PMU_PERF_NAFLL_LUT ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_lut_30.c"  , ],
    },

    CLK_NAFLL_ADDRESS_MAP_HAL_V10 =>
    {
        DESCRIPTION         => "Enable/disable PMU NAFLL register map HAL version V1.0",
        REQUIRES            => [ PMU_PERF_NAFLL_LUT, ],
        CONFLICTS           => [ CLK_NAFLL_ADDRESS_MAP_HAL_V20, CLK_NAFLL_ADDRESS_MAP_HAL_V30, CLK_NAFLL_ADDRESS_MAP_HAL_V40, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp10x', '-pmu-gv11b' ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_lut_reg_map_10.c", ],
    },

    CLK_NAFLL_ADDRESS_MAP_HAL_V20 =>
    {
        DESCRIPTION         => "Enable/disable PMU NAFLL register map HAL version V2.0",
        REQUIRES            => [ PMU_PERF_NAFLL_LUT, ],
        CONFLICTS           => [ CLK_NAFLL_ADDRESS_MAP_HAL_V10, CLK_NAFLL_ADDRESS_MAP_HAL_V30, CLK_NAFLL_ADDRESS_MAP_HAL_V40, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100...', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_lut_reg_map_20.c", ],
    },

    CLK_NAFLL_ADDRESS_MAP_HAL_V30 =>
    {
        DESCRIPTION         => "Enable/disable PMU NAFLL register map HAL version V3.0",
        REQUIRES            => [ PMU_PERF_NAFLL_LUT, ],
        CONFLICTS           => [ CLK_NAFLL_ADDRESS_MAP_HAL_V10, CLK_NAFLL_ADDRESS_MAP_HAL_V20, CLK_NAFLL_ADDRESS_MAP_HAL_V40, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ad10x*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_lut_reg_map_30.c", ],
    },

    CLK_NAFLL_ADDRESS_MAP_HAL_V40 =>
    {
        DESCRIPTION         => "Enable/disable PMU NAFLL register map HAL version V4.0",
        REQUIRES            => [ PMU_PERF_NAFLL_LUT, ],
        CONFLICTS           => [ CLK_NAFLL_ADDRESS_MAP_HAL_V10, CLK_NAFLL_ADDRESS_MAP_HAL_V20, CLK_NAFLL_ADDRESS_MAP_HAL_V30, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_lut_reg_map_40.c", ],
    },

    PMU_PERF_NAFLL_REGIME =>
    {
        DESCRIPTION         => "Enable/disable PMU support for NAFLL regime programming",
        REQUIRES            => [ PMU_PERF_NAFLL_DEVICES, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_regime.c"  , ],
    },

    PMU_PERF_NAFLL_REGIME_V2X =>
    {
        DESCRIPTION         => "Enable/disable PMU support for common V1.0 and V2.0 NAFLL regime programming",
        REQUIRES            => [ PMU_PERF_NAFLL_REGIME, ],
        CONFLICTS           => [ PMU_PERF_NAFLL_REGIME_V30, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100...', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_regime_2x.c", ],
    },

    PMU_PERF_NAFLL_REGIME_V10 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V1.0 NAFLL regime programming",
        REQUIRES            => [ PMU_PERF_NAFLL_REGIME_V2X, ],
        CONFLICTS           => [ PMU_PERF_NAFLL_REGIME_V20, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-tu10x...', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_regime_10.c"  , ],
    },

    PMU_PERF_NAFLL_REGIME_V20 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V2.0 NAFLL regime programming",
        REQUIRES            => [ PMU_PERF_NAFLL_REGIME_V2X, ],
        CONFLICTS           => [ PMU_PERF_NAFLL_REGIME_V10, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-ga100...', '-pmu-gv11b', ],
    },

    PMU_PERF_NAFLL_BYPASS_CLKS_ON_PMU =>
    {
        DESCRIPTION         => "Enable/disable PMU support for NAFLL clocks programming without using clk 3.0",
        REQUIRES            => [ PMU_PERF_NAFLL_REGIME, PMU_UCODE_PROFILE_AUTO, ],
        CONFLICTS           => [ PMU_CLK_CLKS_ON_PMU, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_regime_20.c"  , ],
    },

    PMU_PERF_NAFLL_REGIME_V30 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for V3.0 NAFLL regime programming",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES_ALWAYS_ON, PMU_PERF_NAFLL_REGIME, ],
        CONFLICTS           => [ PMU_PERF_NAFLL_REGIME_V2X, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clknafll_regime_30.c", ],
    },

    PMU_PERF_NAFLL_LUT_BROADCAST =>
    {
        DESCRIPTION         => "Enable/disable PMU support for NAFLL LUT Broadcast Programming",
        REQUIRES            => [ PMU_PERF_NAFLL_DEVICES, PMU_PERF_ADC_HW_CALIBRATION, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_NAFLL_ADC_OVERRIDE_10 =>
    {
        DESCRIPTION         => "Enable/disable support for ADC SW override on simulation/emulation during NAFLL programming. version 1.0 which only supports mode - HW and SW, no support for MIN(HW, SW).",
        REQUIRES            => [ PMU_PERF_NAFLL_DEVICES, PMU_PERF_ADC_DEVICES, ],
        CONFLICTS           => [ PMU_PERF_NAFLL_ADC_OVERRIDE_20, ],
        PROFILES            => [ 'pmu-gv10x', 'pmu-tu10x', ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
    },

    PMU_PERF_NAFLL_ADC_OVERRIDE_20 =>
    {
        DESCRIPTION         => "Enable/disable support for ADC SW override. version 2.0 will support SW, HW, MIN(SW, HW) modes of operations.",
        REQUIRES            => [ PMU_PERF_NAFLL_DEVICES, PMU_PERF_ADC_DEVICES, PMU_PERF_ADC_DEVICES_ALWAYS_ON, PMU_PERF_CHANGE_SEQ, ],
        CONFLICTS           => [ PMU_PERF_NAFLL_ADC_OVERRIDE_10, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
    },

    PMU_PERF_LUT_OVERRIDE_MIN_WAR_BUG_1588803 =>
    {
        DESCRIPTION         => "SW WAR for bug 1588803. Always set VFGAIN=0xF when in MIN mode",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_AVFS, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100', ],
    },

    PMU_PERF_ADC_VOLTAGE_SAMPLE_CALLBACK =>
    {
        DESCRIPTION         => "Enable/disable voltage sampling callback",
        REQUIRES            => [ PMU_PERF_ADC_DEVICES_EXTENDED, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_PROGRAM_LUT_BEFORE_ENABLING_ADC_WAR_BUG_1629772 =>
    {
        DESCRIPTION          => "SW WAR for bug 1629772. Set the 0th LUT entry and ADC override to 0 before ADC is enabled.",
        REQUIRES             => [ PMUTASK_PERF, PMU_PERF_AVFS, PMU_PERF_ADC_DEVICES, ],
        ENGINES_REQUIRED     => [ PERF, CLK, ],
        PROFILES             => [ 'pmu-gp100', ],
    },

    PMU_PERF_ADC_HW_CALIBRATION =>
    {
        DESCRIPTION         => "Enable/disable PMU support for ADC calibration in HW",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_AVFS, PMU_PERF_ADC_DEVICES],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_NAFLL_CPM =>
    {
        DESCRIPTION         => "Enable/disable CPM support for NAFLL Devices",
        REQUIRES            => [ ], # TODO - Add PMU_PERF_NAFLL_LUT_V30,
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ ], # TODO - pmu-ga100
    },

    PMU_PERF_NAFLL_SEC_LWRVE =>
    {
        DESCRIPTION         => "Enable/disable secondary VF lwrve support for NAFLL Devices",
        REQUIRES            => [ PMU_CLK_CLK_VF_POINT_SEC, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_NAFLL_MULTIPLE_SEC_LWRVE =>
    {
        DESCRIPTION         => "Enable/disable support for multiple secondary VF lwrve(s) for NAFLL Devices",
        REQUIRES            => [ PMU_PERF_NAFLL_SEC_LWRVE, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_NAFLL_LUT_QUICK_SLOWDOWN_ENGAGE_SUPPORT =>
    {
        DESCRIPTION         => "Support for engaging/disengaging quick slowdown for NAFLL Devices",
        REQUIRES            => [ PMU_PERF_NAFLL_SEC_LWRVE, PMU_PERF_NAFLL_DEVICE_V30, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ad10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_VFIELD_VALIDATION =>
    {
        DESCRIPTION         => "Validate VFIELD register addresses coming from RM (for security hardening)",
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-gp10x...pmu-ga100', '-pmu-gv11b', ],
    },

    # Logger Features

    PMU_LOGGER =>
    {
        DESCRIPTION         => "PMU Logger",
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "lib/logger.c"          ,
                                 "logger/lw/logger_rpc.c", ],
    },

    # Clock Features

    PMU_CLK_IN_PERF =>
    {
        DESCRIPTION         => "Enables handling of CLK CMDs from within PERF task.",
        REQUIRES            => [ PMUTASK_PERF, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_IN_PERF_DAEMON_PASS_THROUGH_PERF =>
    {
        DESCRIPTION         => "Enables handling of CLK CMDs from within PERF DAEMON task.",
        REQUIRES            => [ PMUTASK_PERF, PMUTASK_PERF_DAEMON, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CNTR =>
    {
        DESCRIPTION         => "Enable/disable clock counter feature.",
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkcntr.c", ],
    },

    PMU_CLK_CNTR_64BIT_LIBS =>
    {
        DESCRIPTION         => "Enable/disable 64 bit library support for clock counter.",
        REQUIRES            => [ PMU_CLK_CNTR, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CNTR_SYS2CLK_WAR_BUG_1677020 =>
    {
        DESCRIPTION         => "SW WAR for bug 1677020. Set clock counter source for sys2clk to sysnafll",
        REQUIRES            => [ PMU_CLK_CNTR, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100', ],
    },

    PMU_CLK_CNTR_HBM_MCLK_2X_SCALING =>
    {
        DESCRIPTION         => "Apply 2x (insteaod of 1x) scaling factor when MCLK is using High Bandwidth Memory (HBM)",
        REQUIRES            => [ PMU_CLK_CNTR, PMU_FB_HBM_SUPPORTED, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-ga100', 'pmu-gh100*', 'pmu-gb100_riscv', 'pmu-g00x_riscv', ],
    },

    PMU_CLK_CNTR_PCM_CALLBACK =>
    {
        DESCRIPTION         => "Enable/disable PCM callback support for clock counter.",
        REQUIRES            => [ PMUTASK_PCMEVT, PMU_CLK_CNTR, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gm20x', ],
    },

    PMU_CLK_CNTR_PERF_CALLBACK =>
    {
        DESCRIPTION         => "Enable/disable PERF callback support for clock counter.",
        REQUIRES            => [ PMUTASK_PERF, PMU_CLK_CNTR, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CNTR_TOGGLE_GATED_LOGIC =>
    {
        DESCRIPTION         => "Enable/disable the given clock counter logic when HW access is gated",
        REQUIRES            => [ PMU_CLK_CNTR, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_BOARDOBJGRP_SUPPORT =>
    {
        DESCRIPTION         => "Enable/disable support for Board Object Group handling.",
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN =>
    {
        DESCRIPTION         => "Enable/disable CLK_DOMAIN feature.",
        REQUIRES            => [ PMU_CLK_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_BASE =>
    {
        DESCRIPTION         => "Enable/disable CLK_DOMAIN base feature set required for ALL profiles.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_BASE_AUTO =>
    {
        DESCRIPTION         => "Enable/disable CLK_DOMAIN base feature set required for AUTO profiles.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_BASE, PMU_UCODE_PROFILE_AUTO, ],
        CONFLICTS           => [ PMU_CLK_CLK_DOMAIN_EXTENDED, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ ],
    },

    PMU_CLK_CLK_DOMAIN_EXTENDED =>
    {
        DESCRIPTION         => "Enable/disable CLK_DOMAIN extended feature set required for big GPU profiles.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_BASE, ],
        CONFLICTS           => [ PMU_CLK_CLK_DOMAIN_BASE_AUTO, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_BASE_3X =>
    {
        DESCRIPTION         => "Enable/disable CLK_DOMAIN_3X base feature, which is a part of pstates 3.X.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_BASE, ],
        CONFLICTS           => [ PMU_CLK_CLK_DOMAIN_40, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_EXTENDED_3X =>
    {
        DESCRIPTION         => "Enable/disable CLK_DOMAIN_3X additional feature, which is a part of pstates 3.X.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_BASE_3X, PMU_CLK_CLK_DOMAIN_EXTENDED, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_30 =>
    {
        DESCRIPTION         => "Enable/disable CLK_DOMAIN_30 feature, which is a part of pstates 3.0.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_BASE_3X, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-tu10x...', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_BASE_35 =>
    {
        DESCRIPTION         => "Enable/disable CLK_DOMAIN_35 base feature, which is a part of pstates 3.5.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_BASE_3X, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x', 'pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_EXTENDED_35 =>
    {
        DESCRIPTION         => "Enable/disable CLK_DOMAIN_35 additional feature, which is a part of pstates 3.5.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_BASE_35, PMU_CLK_CLK_DOMAIN_EXTENDED, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x', 'pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_40 =>
    {
        DESCRIPTION         => "Enable/disable CLK_DOMAIN_40 feature, which is a part of pstates 4.0.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_BASE, ],
        CONFLICTS           => [ PMU_CLK_CLK_DOMAIN_BASE_3X, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_PROG_VOLT_RAIL_VMIN_MASK =>
    {
        DESCRIPTION         => "Enable/disable support for volt rail Vmin mask.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_BASE, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAINS_PMUMON =>
    {
        DESCRIPTION         => "Enable/disable, CLK_DOMAINS PMUMON functionality",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN, PMU_LIB_PMUMON, PMU_PERF_CHANGE_SEQ, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-gh20x', '-pmu-gv11b', ],
        SRCFILES            => [ 'clk/lw/clk_domain_pmumon.c', ],
    },

    PMU_CLK_CLK_DOMAIN_CLIENT_INDEX_MAP =>
    {
        DESCRIPTION         => "Enable/disable mapping support of client clock domain index to internal clock domain index",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLIENT_CLK_DOMAIN =>
    {
        DESCRIPTION         => "Enable/disable CLIENT_CLK_DOMAIN feature.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ ],
    },

    PMU_CLK_CLK_PROG =>
    {
        DESCRIPTION         => "Enable/disable CLK_PROG feature.",
        REQUIRES            => [ PMU_CLK_BOARDOBJGRP_SUPPORT, ],
        CONFLICTS           => [ PMU_CLK_CLK_ENUM, PMU_CLK_CLK_VF_REL, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_PROG_BASE_3X =>
    {
        DESCRIPTION         => "Enable/disable CLK_PROG_3X base feature, which is a part of pstates 3.X.",
        REQUIRES            => [ PMU_CLK_CLK_PROG, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_PROG_BASE_3X_AUTO =>
    {
        DESCRIPTION         => "Enable/disable CLK_PROG_3X BASE AUTO specific POR feature, which is a part of pstates 3.X.",
        REQUIRES            => [ PMU_CLK_CLK_PROG_BASE_3X, PMU_CLK_CLK_DOMAIN_BASE_AUTO, PMU_UCODE_PROFILE_AUTO, ],
        CONFLICTS           => [ PMU_CLK_CLK_PROG_EXTENDED_3X, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ ],
    },

    PMU_CLK_CLK_PROG_EXTENDED_3X =>
    {
        DESCRIPTION         => "Enable/disable CLK_PROG_3X feature, which is a part of pstates 3.X.",
        REQUIRES            => [ PMU_CLK_CLK_PROG_BASE_3X, PMU_CLK_CLK_DOMAIN_EXTENDED, ],
        CONFLICTS           => [ PMU_CLK_CLK_PROG_BASE_3X_AUTO, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_PROG_30 =>
    {
        DESCRIPTION         => "Enable/disable CLK_PROG_30 feature, which is a part of pstates 3.0.",
        REQUIRES            => [ PMU_CLK_CLK_PROG_BASE_3X, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-tu10x...', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_PROG_BASE_35 =>
    {
        DESCRIPTION         => "Enable/disable CLK_PROG_35 base feature, which is a part of pstates 3.5.",
        REQUIRES            => [ PMU_CLK_CLK_PROG_BASE_3X, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x', 'pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_PROG_EXTENDED_35 =>
    {
        DESCRIPTION         => "Enable/disable CLK_PROG_35 feature, which is a part of pstates 3.5.",
        REQUIRES            => [ PMU_CLK_CLK_PROG_BASE_35, PMU_CLK_CLK_DOMAIN_EXTENDED, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x', 'pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_PROP =>
    {
        DESCRIPTION         => "Enable/disable CLK_PROP feature.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_40, ],
        ENGINES_REQUIRED    => [ CLK, PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_PROP_REGIME =>
    {
        DESCRIPTION         => "Enable/disable CLK_PROP_REGIME feature.",
        REQUIRES            => [ PMU_CLK_CLK_PROP, ],
        ENGINES_REQUIRED    => [ CLK, PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_PROP_TOP =>
    {
        DESCRIPTION         => "Enable/disable CLK_PROP_TOP feature.",
        REQUIRES            => [ PMU_CLK_CLK_PROP, ],
        ENGINES_REQUIRED    => [ CLK, PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLIENT_CLK_PROP_TOP_POL =>
    {
        DESCRIPTION         => "Enable/disable CLIENT_CLK_PROP_TOP_POL feature.",
        REQUIRES            => [ PMU_CLK_CLK_PROP_TOP, PMU_PERF_PERF_LIMIT, ],
        ENGINES_REQUIRED    => [ CLK, PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_PROP_TOP_REL =>
    {
        DESCRIPTION         => "Enable/disable CLK_PROP_TOP_REL feature.",
        REQUIRES            => [ PMU_CLK_CLK_PROP_TOP, ],
        ENGINES_REQUIRED    => [ CLK, PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_PROP_TOP_REL_VFE =>
    {
        DESCRIPTION         => "Enable/disable VFE based CLK_PROP_TOP_REL feature.",
        REQUIRES            => [ PMU_CLK_CLK_PROP_TOP_REL, ],
        ENGINES_REQUIRED    => [ CLK, PERF, ],
        PROFILES            => [ ],
    },

    PMU_CLK_CLK_ENUM =>
    {
        DESCRIPTION         => "Enable/disable CLK_ENUM feature.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_40, ],
        CONFLICTS           => [ PMU_CLK_CLK_PROG, ],
        ENGINES_REQUIRED    => [ CLK, PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_VF_REL =>
    {
        DESCRIPTION         => "Enable/disable CLK_VF_REL feature.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_40, ],
        CONFLICTS           => [ PMU_CLK_CLK_PROG, ],
        ENGINES_REQUIRED    => [ CLK, PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_VF_POINT =>
    {
        DESCRIPTION         => "Enable/disable CLK_VF_POINT and its child classes.",
        REQUIRES            => [ PMU_CLK_BOARDOBJGRP_SUPPORT, PMU_BOARDOBJ_GRP_E512, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_VF_POINT_BASE_VF_CACHE =>
    {
        DESCRIPTION         => "Enable/disable CLK_VF_POINT functionality to cache base VF lwrves.",
        REQUIRES            => [ PMU_CLK_CLK_VF_POINT, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ ],
    },

    PMU_CLK_CLK_VF_POINT_OFFSET_VF_CACHE =>
    {
        DESCRIPTION         => "Enable/disable CLK_VF_POINT functionality to cache offset VF lwrves.",
        REQUIRES            => [ PMU_CLK_CLK_VF_POINT, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ ],
    },

    PMU_CLK_CLK_VF_POINT_3X =>
    {
        DESCRIPTION         => "Enable/disable CLK_VF_POINT 3X and its child classes.",
        REQUIRES            => [ PMU_CLK_CLK_VF_POINT, ],
        CONFLICTS           => [ PMU_CLK_CLK_VF_POINT_40, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_VF_POINT_30 =>
    {
        DESCRIPTION         => "Enable/disable CLK_VF_POINT_30 feature, which is a part of pstates 3.0.",
        REQUIRES            => [ PMU_CLK_CLK_VF_POINT_3X, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-tu10x...', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_VF_POINT_35 =>
    {
        DESCRIPTION         => "Enable/disable CLK_VF_POINT_35 feature, which is a part of pstates 3.5.",
        REQUIRES            => [ PMU_CLK_CLK_VF_POINT_3X, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x', 'pmu-ga100', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_VF_POINT_40 =>
    {
        DESCRIPTION         => "Enable/disable CLK_VF_POINT_40 feature, which is a part of pstates 4.0.",
        REQUIRES            => [ PMU_CLK_CLK_VF_POINT, PMU_CLK_CLK_DOMAIN_40, ],
        CONFLICTS           => [ PMU_CLK_CLK_VF_POINT_3X, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_VF_POINT_SEC =>
    {
        DESCRIPTION         => "Enable/disable support for secondary VF lwrves.",
        REQUIRES            => [ PMU_CLK_CLK_VF_POINT, ],
        CONFLICTS           => [ PMU_CLK_CLK_VF_POINT_30, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_VF_POINT_TRIM_VF_LWRVE_BY_ENUM_MAX =>
    {
        DESCRIPTION         => "Trim the VF lwrve by offset adjusted clock enumeation max frequency.",
        REQUIRES            => [ PMU_CLK_CLK_VF_POINT, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_VF_LOOK_UP_USING_BINARY_SEARCH =>
    {
        DESCRIPTION         => "Enable/disable support for VF look up using binary search algorithm.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_EXTENDED, ],
        CONFLICTS           => [ PMU_CLK_CLK_DOMAIN_30, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/disable OV/OC support. This feature does not work on PS 3.0",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_EXTENDED, ],
        CONFLICTS           => [ PMU_CLK_CLK_DOMAIN_30, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_VF_SMOOTHENING_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/disable code resposible for ensuring smooth VF lwrve.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_EXTENDED, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_DOMAIN_VF_MONOTONICITY_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/disable code resposible for ensuring monotonically increasing VF lwrve.",
        REQUIRES            => [ PMU_CLK_CLK_DOMAIN_BASE, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLIENT_CLK_VF_POINT =>
    {
        DESCRIPTION         => "Enable/disable CLIENT_CLK_VF_POINT and its child classes.",
        REQUIRES            => [ PMU_CLK_CLK_VF_POINT_40, PMU_CLK_CLK_DOMAIN_40, PMU_CLK_CLK_VF_REL, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
    },

    PMU_CLK_CLK_FREQ_COUNTED_AVG =>
    {
        DESCRIPTION         => "Enable/disable counted average frequency feature",
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_CONTROLLER =>
    {
        DESCRIPTION        => "Enable/Disable clock controllers and its child classes.",
        ENGINES_REQUIRED   => [ CLK, ],
        PROFILES           => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_FREQ_CONTROLLER =>
    {
        DESCRIPTION         => "Enable/disable CLK_FREQ_CONTROLLER and its child classes.",
        REQUIRES            => [ PMU_CLK_CNTR, PMU_CLK_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_VOLT_CONTROLLER =>
    {
        DESCRIPTION         => "Enable/disable CLK_VOLT_CONTROLLER and its child classes.",
        REQUIRES            => [ PMU_CLK_BOARDOBJGRP_SUPPORT, PMU_LIB_AVG, PMU_CLK_CLK_CONTROLLER, PMU_PERF_CHANGE_SEQ_VOLT_OFFSET_CHANGE, ],
        ENGINES_REQUIRED    => [ CLK, PERF, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_VOLT_CONTROLLER_PROP =>
    {
        DESCRIPTION         => "Enable/disable proportional CLK_VOLT_CONTROLLER and its child classes.",
        REQUIRES            => [ PMU_CLK_CLK_VOLT_CONTROLLER, PMU_VOLT_VOLTAGE_SENSED, PMU_VOLT_VOLT_RAIL_OFFSET, ],
        ENGINES_REQUIRED    => [ CLK, PERF, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_FREQ_CONTROLLER_V10 =>
    {
        DESCRIPTION         => "Enable/disable CLK_FREQ_CONTROLLER Version 1.0 and its child classes.",
        REQUIRES            => [ PMU_CLK_CLK_FREQ_CONTROLLER, PMU_VOLT_VOLT_POLICY_OFFSET, ],
        CONFLICTS           => [ PMU_CLK_CLK_FREQ_CONTROLLER_V20, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...pmu-tu10x', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_FREQ_CONTROLLER_V20 =>
    {
        DESCRIPTION         => "Enable/disable CLK_FREQ_CONTROLLER Version 2.0 and its child classes.",
        REQUIRES            => [ PMU_CLK_CLK_FREQ_CONTROLLER, PMU_VOLT_VOLT_RAIL_OFFSET, PMU_PERF_CHANGE_SEQ_VOLT_OFFSET_CHANGE, ],
        CONFLICTS           => [ PMU_CLK_CLK_FREQ_CONTROLLER_V10, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ ],
    },

    PMU_CLK_CLK_FREQ_CONTROLLER_BA_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/disable BA slowdown support in CLK_FREQ_CONTROLLER",
        REQUIRES            => [ PMU_CLK_CLK_FREQ_CONTROLLER, PMU_THERM_THERM_MONITOR, ],
        ENGINES_REQUIRED    => [ CLK, THERM, ],
        PROFILES            => [ 'pmu-gv10x', ],
    },

    PMU_CLK_CLK_FREQ_CONTROLLER_DROOPY_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/disable droopy VR support in CLK_FREQ_CONTROLLER",
        REQUIRES            => [ PMU_CLK_CLK_FREQ_CONTROLLER, PMU_THERM_THERM_MONITOR, ],
        ENGINES_REQUIRED    => [ CLK, THERM, ],
        PROFILES            => [ 'pmu-gv10x...pmu-tu10x', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_FREQ_CONTROLLER_CONTINUOUS_MODE_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/disable continuous mode support for CLK_FREQ_CONTROLLER",
        REQUIRES            => [ PMU_CLK_CLK_FREQ_CONTROLLER, PMU_CLK_CLK_FREQ_COUNTED_AVG, ],
        ENGINES_REQUIRED    => [ CLK, THERM, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_PLL_DEVICE =>
    {
        DESCRIPTION         => "Enable/disable PLL_DEVICE and its child classes.",
        REQUIRES            => [ PMU_CLK_BOARDOBJGRP_SUPPORT, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_CLK_FREQ_EFFECTIVE_AVG =>
    {
        DESCRIPTION         => "Enable/disable effective average frequency feature",
        REQUIRES            => [ PMU_CLK_CNTR, PMU_CLK_CNTR_64BIT_LIBS, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    #
    # Table of interaction between these 1x/2x features:
    #                           1X_SUPPORTED    1X_AND_2X_SUPPORTED
    # Only 2x domains:                 No            No
    # Only 1x domains:                 Yes           Don't Care
    # Mix of 1x and 2x domains:        No            Yes
    #
    # See OBJCLK::clkDist1xDomMask for more details.
    #
    PMU_CLK_DOMAINS_1X_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/disable support for 1x clock domains",
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gv10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    # See above
    PMU_CLK_DOMAINS_1X_AND_2X_SUPPORTED =>
    {
        DESCRIPTION         => "Both 1x and 2x clock domains may exist",
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-gp100', 'pmu-gp10x', ],
    },

    #
    # Clocks 3.x (aka Clocks on PMU)
    # Note: PMU_CLK_CLK_DOMAIN_3X and similar are not Clocks 3.x.
    # Note: Many Clocks 3.x files are family-specific and
    # managed in pmu_sw/config/srcdefs/Sources.def.
    # https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
    #
    PMU_CLK_CLKS_ON_PMU =>
    {
        DESCRIPTION         => "Enable/disable the programming of clocks on PMU",
        REQUIRES            => [ PMU_BOARDOBJ, PMU_CLK_BOARDOBJGRP_SUPPORT, ],
        CONFLICTS           => [ PMU_PERF_NAFLL_BYPASS_CLKS_ON_PMU, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/clk3/clk3.c"                           ,
                                 "clk/clk3/clksignal.c"                      ,
                                 "clk/clk3/dom/clkfreqdomain.c"              ,
                                 "clk/clk3/dom/clkfreqdomain_bif.c"          ,
                                 "clk/clk3/dom/clkfreqdomain_mclk.c"         ,
                                 "clk/clk3/dom/clkfreqdomain_multinafll.c"   ,
                                 "clk/clk3/dom/clkfreqdomain_osm.c"          ,
                                 "clk/clk3/dom/clkfreqdomain_apllldiv_disp.c",   # Turing (or Volta)
                                 "clk/clk3/dom/clkfreqdomain_singlenafll.c"  ,
                                 "clk/clk3/dom/clkfreqdomain_stub.c"         ,   # Bug 200400746: Not POR
                                 "clk/clk3/dom/clkfreqdomain_swdivhpll.c"    ,
                                 "clk/clk3/dom/clkfreqdomain_swdivapll.c"    ,
                                 "clk/clk3/fs/clkbif.c"                      ,
                                 "clk/clk3/fs/clkadynramp.c"                 ,
                                 "clk/clk3/fs/clkhpll.c"                     ,   # GA10x+
                                 "clk/clk3/fs/clkldivunit.c"                 ,   # Turing (or Volta)
                                 "clk/clk3/fs/clkldivv2.c"                   ,   # Ampere+
                                 "clk/clk3/fs/clkmultiplier.c"               ,
                                 "clk/clk3/fs/clkdivider.c"                  ,
                                 "clk/clk3/fs/clkmux.c"                      ,
                                 "clk/clk3/fs/clknafll.c"                    ,
                                 "clk/clk3/fs/clkosm.c"                      ,   # Turing (or Volta)
                                 "clk/clk3/fs/clkpdiv.c"                     ,   # GA10x+
                                 "clk/clk3/fs/clkapll.c"                     ,   # Turing (or Volta), AD10X
                                 "clk/clk3/fs/clkreadonly.c"                 ,
                                 "clk/clk3/fs/clkropdiv.c"                   ,
                                 "clk/clk3/fs/clkrosdm.c"                    ,
                                 "clk/clk3/fs/clkrovco.c"                    ,
                                 "clk/clk3/fs/clkasdm.c"                     ,
                                 "clk/clk3/fs/clksppll.c"                    ,   # Turing and GA100/GA101
                                 "clk/clk3/fs/clkswdiv2automatic.c"          ,   # GA10x+
                                 "clk/clk3/fs/clkswdiv4automatic.c"          ,   # GA10x+
                                 "clk/clk3/fs/clkswdiv4.c"                   ,   # Ampere+
                                 "clk/clk3/fs/clkwire.c"                     ,
                                 "clk/clk3/fs/clkxtal.c"                     ,
                                 "clk/clk3/sd/clksdg00x.c"                   ,
                                 "clk/clk3/sd/clksdtu10x.c"                  ,
                                 "clk/clk3/sd/clksdga100.c"                  ,
                                 "clk/clk3/sd/clksdga10x.c"                  ,
                                 "clk/clk3/sd/clksdad10x.c"                  ,
                                 "clk/clk3/sd/clksdgh100.c"                  ,
                                 "clk/clk3/sd/clksdgh20x.c"                  , ],
    },

    # Temporary feature to enable CLK 3.0 testing.
    PMU_CLK_CLKS_ON_PMU_TESTING =>
    {
        DESCRIPTION         => "Enable/disable the testing of programming clocks on PMU",
        REQUIRES            => [ PMU_CLK_CLKS_ON_PMU, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_FREQ_DOMAIN_PROGRAM_BYPASS_PATH_SELWRITY =>
    {
        DESCRIPTION         => "Enable/Disable the security check on clock frequency domain bypass path.",
        REQUIRES            => [ PMU_CLK_CLKS_ON_PMU, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/ampere/pmu_clkga10x.c", ],
    },

    PMU_CLK_FBFLCN_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/Disable the support for communicating with FB falcon from PMU",
        REQUIRES            => [ PMU_CLK_IN_PERF_DAEMON_PASS_THROUGH_PERF, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gv10*', 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clkfbflcn.c", ],
    },

    PMU_CLK_RM_MCLK_RPC_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/Disable the support for RM -> PMU -> FBFLCN RPC path for MCLK switch.",
        REQUIRES            => [ PMU_CLK_FBFLCN_SUPPORTED, ],
        CONFLICTS           => [ PMU_CLK_CLKS_ON_PMU, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gv10x', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_FBFLCN_TRRD_PROGRAM_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/Disable the support for programming tRRD",
        REQUIRES            => [ PMU_CLK_FBFLCN_SUPPORTED, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
        SRCFILES            => [ "clk/ampere/pmu_clkfbflcnga10x.c", ],
    },

    PMU_CLK_PROGRAMMABLE_MCLK_SUPPORTED =>
    {
        DESCRIPTION         => "Indicates whether MCLK switching is supported on the PMU",
        REQUIRES            => [ PMU_CLK_FBFLCN_SUPPORTED, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gv10*', 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/clk3/dom/clkfreqdomain_mclk.c", ],
    },

    PMU_CLK_FBFLCN_MCLK_SWITCH_FB_STOP_WAR =>
    {
        DESCRIPTION         => "Indicates whether the WAR to execute some part of the FB stop sequence on PMU is enabled (Bug 1898603)",
        REQUIRES            => [ PMU_CLK_PROGRAMMABLE_MCLK_SUPPORTED, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gv10*', ],
        SRCFILES            => [ "fb/volta/fb_stop_war_gv10x_only.c", ],
    },

    PMU_CLK_MCLK_DDR_SUPPORTED =>
    {
        DESCRIPTION         => "Clocks 3.x supports DDR memory (GDDR5, GDDR5X, GDDR6, etc.)",
        REQUIRES            => [ PMU_CLK_CLKS_ON_PMU, PMU_FB_DDR_SUPPORTED, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_CLK_MCLK_HBM_SUPPORTED =>
    {
        DESCRIPTION         => "Clocks 3.x supports HBM memory (high bandwidth)",
        REQUIRES            => [ PMU_CLK_CLKS_ON_PMU, PMU_FB_HBM_SUPPORTED, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-ga100', 'pmu-gh100*', 'pmu-gb100_riscv', ],
    },

    PMU_CLK_SWDIV_SUPPORTED =>
    {
        DESCRIPTION         => "Does this chip have Switch Dividers (SWDIVs)?",
        REQUIRES            => [ PMU_CLK_CLKS_ON_PMU, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    #
    # This feature is specific to Turing and Ada Analog PLLs (APLL), and does
    # not apply to Hybrid PLLs (HPLL) on GA10x & GH100 and later.
    #
    PMU_CLK_DISP_DYN_RAMP_SUPPORTED =>
    {
        DESCRIPTION         => "Enable/disable the DISPPLL dynamic ramping",
        REQUIRES            => [ PMU_CLK_CLKS_ON_PMU, ],
        CONFLICTS           => [ PMU_DISPLAYLESS, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-tu10x', 'pmu-ad10x_riscv', ],
        SRCFILES            => [ "clk/clk3/fs/clkadynramp.c", ],
    },

    #
    # To-do akshatam: Break this up into multiple dedicated CLK_LPWR features to
    # be able to club features like PMU_PERF_NAFLL_DEVICES_INIT under GPC-RG.
    #
    PMU_CLK_LPWR =>
    {
        DESCRIPTION         => "Support for clock init/de-init for different LPWR features like GPC-RG",
        REQUIRES            => [ PMU_PG_GR_RG, PMU_CLK_DOMAIN_ACCESS_CONTROL, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "clk/lw/pmu_clklpwr.c", ],
    },

    PMU_CLK_LPWR_VERIF =>
    {
        DESCRIPTION         => "Verif feature - for clock LPWR features",
        REQUIRES            => [ PMU_PG_GR_RG, PMU_CLK_DOMAIN_ACCESS_CONTROL, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ ], #'pmu-ga10x_riscv...', '-pmu-g*0b*', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', ],
    },

    PMU_CLK_DOMAIN_ACCESS_CONTROL =>
    {
        DESCRIPTION         => "Support for enabling/disabling access(read/program) to different clock domains",
        REQUIRES            => [ PMU_CLK_CLKS_ON_PMU, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', 'pmu-tu10x', '-pmu-g00x_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_SEQ_FB_STOP_SUPPORTED =>
    {
        DESCRIPTION         => "Indicates whether MCLK switch is supported via PMU",
        ENGINES_REQUIRED    => [ SEQ, ],
        REQUIRES            => [ PMUTASK_SEQ, ],
        PROFILES            => [ '...pmu-tu10x', '-pmu-gv11b', ],
        SRCFILES            => [ "fb/maxwell/fbgm10x.c", ],
    },

    #
    # Clocks monitors
    # https://confluence.lwpu.com/pages/viewpage.action?pageId=138011558
    #
    PMU_CLK_CLOCK_MON =>
    {
        DESCRIPTION         => "Enable/disable clock monitors",
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ 'pmu-ga100', ],
        SRCFILES            => [
                                 "clk/lw/clk_clockmon.c",
                                 "clk/turing/pmu_clkmontu10x.c",
                                 "clk/ampere/pmu_clkmonga100.c",
                               ],
    },

    #
    # TODO: Remove current feature and associated code after FMON verification
    #       completes. http://lwbugs/1971316
    #
    PMU_CLK_CLOCK_MON_FAULT_STATUS_CHECK =>
    {
        DESCRIPTION         => "Enable/disable clock monitors fault status check",
        REQUIRES            => [ PMU_CLK_CLOCK_MON, ],
        ENGINES_REQUIRED    => [ CLK, ],
        PROFILES            => [ ],
    },

    # GC6 Features

    PMU_LPWR_GC6 =>
    {
        DESCRIPTION         => "Enable/disable core GC6 support",
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gp100', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmu/kepler/pmu_pwrgkxxx.c"      ,
                                 "pmu/maxwell/pmu_pwrgmxxx.c"     ,
                                 "pmu/maxwell/pmu_pwrgmxxxadxxx.c",
                                 "pg/maxwell/pmu_gc6gm10x.c"      ,
                                 "pg/maxwell/pmu_gc6gm20x.c"      ,
                                 "pg/maxwell/pmu_gc6gm10xad10x.c" ,
                                 "pg/pascal/pmu_gc6gp10x.c"       ,
                                 "pg/pascal/pmu_gc6gp10xonly.c"   , ],
    },

    PMU_GC6_DMEM_VA_COPY =>
    {
        DESCRIPTION         => "Save/restore the original DMEM-VA state across GC6 cycles",
        PROFILES            => [ 'pmu-tu10x', ],
        REQUIRES            => [ PMU_LPWR_GC6, ],
    },

    PMU_GC6_BSI_BOOTSTRAP =>
    {
        DESCRIPTION         => "Enable/disable GC6 exit BSI bootstrap support",
        REQUIRES            => [ PMU_LPWR_GC6, PMU_GC6_PLUS, ],
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gp100', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_GC6_CONFIG_SPACE_RESTORE =>
    {
        DESCRIPTION         => "Enable/disable PMU Config Space Restore",
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gp100', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_GC6_PRIV_AWARE_CONFIG_SPACE_RESTORE =>
    {
        DESCRIPTION         => "Enable/disable priv level aware config space restore",
        REQUIRES            => [ PMU_GC6_CONFIG_SPACE_RESTORE, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    PMU_GC6_WAR_BUG_1279179 =>
    {
        DESCRIPTION         => "WAR for clkreq bug 1279179",
        REQUIRES            => [ PMU_LPWR_GC6, PMU_GC6_PLUS, ],
        PROFILES            => [ 'pmu-gm*', 'pmu-gp10x', ],
    },

    PMU_GC6_WAR_BUG_1255637 =>
    {
        DESCRIPTION         => "WAR for clkreq bug 1255637",
        REQUIRES            => [ PMU_LPWR_GC6, PMU_GC6_PLUS, ],
        PROFILES            => [ 'pmu-gm*', ],
    },

    #
    # The FBSR in PMU feature should only be enabled
    # in Turing and GP102 for RTD3 POC
    # Remove gh10x because there is no HAL yet.
    # TODO - SC enable HAL back for GH10X once manual is out.See bug#200460861.
    #
    PMU_GC6_FBSR_IN_PMU =>
    {
        DESCRIPTION         => "Do FB self-refresh in PMU",
        REQUIRES            => [ PMU_LPWR_GC6, PMU_GC6_PLUS, ],
        PROFILES            => [ 'pmu-gp10x', 'pmu-tu10x...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_GC6_LOWER_PMU_PRIV_LEVEL =>
    {
        DESCRIPTION         => "Lower the PMU PRIV level during GC6 transaction so RM is able to reset PMU",
        REQUIRES            => [ PMU_LPWR_GC6, PMU_GC6_PLUS, ],
        PROFILES            => [ 'pmu-gp10x', 'pmu-tu10x...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
    },

    # GC5 Features

    # For more description about the oneshot mode, please refer to the Maxwell display GC6+ IAS at:
    # https://p4viewer.lwpu.com/get///hw/doc/gpu/display/GPUDisplay/2.4/specifications/Maxwell_Display_GC6+_IAS.docx
    PMU_DISP_ONESHOT =>
    {
        DESCRIPTION         => "Enable/disable disp oneshot (RG stalled and ELV blocked) support",
        PROFILES            => [ ],
        SRCFILES            => [ "disp/maxwell/pmu_dispgmxxx.c", ],
    },

    PMU_MS_OSM =>
    {
        DESCRIPTION         => "RM based Oneshot state machine",
        REQUIRES            => [ PMUTASK_DISP, ],
        ENGINES_REQUIRED    => [ DISP, ],
        PROFILES            => [ 'pmu-gp10x', ],
    },

    PMU_MS_OSM_1 =>
    {
        DESCRIPTION         => "MSCG with Oneshot state machine 1.0",
        REQUIRES            => [ PMU_PG_MS, PMU_MS_OSM, ],
        PROFILES            => [ 'pmu-gp10x', ],
    },

    # Library Features

    PMU_LIB_FXP =>
    {
        DESCRIPTION         => "Enable/disable FXP math library.",
        PROFILES            => [ 'pmu-*', ],
        SRCFILES            => [ "lib/lib_fxp.c"       ,
                                 "lib/lib_fxp_falcon.c",
                                 "lib/lib_fxp_riscv.c" , ],
    },

    PMU_LIB_PWM =>
    {
        DESCRIPTION         => "Enable/disable PWM source control.",
        PROFILES            => [ 'pmu-*', ],
        SRCFILES            => [ "lib/lib_pwm.c", ],
    },

    PMU_LIB_GPIO =>
    {
        DESCRIPTION         => "Enable/disable GPIO library.",
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "lib/lib_gpio.c", ],
    },

    PMU_LIB_AVG =>
    {
        DESCRIPTION         => "Enable/disable avg math library.",
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "lib/lib_avg.c", ],
    },

    PMU_LIB_PMUMON =>
    {
        DESCRIPTION         => "Enable/disable PMUMON library.",
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-gh20x', '-pmu-gv11b', ],
        REQUIRES            => [ ARCH_RISCV, ],
        SRCFILES            => [ "lib/lib_pmumon.c", ],
    },

    PMU_LIB_PMUMON_OVER_USTREAMER =>
    {
        DESCRIPTION         => "Select uStreamer as transport layer for PMUMon",
        PROFILES            => [ ],
        REQUIRES            => [ ARCH_RISCV, PMU_LIB_PMUMON ],
        SRCFILES            => [ ],
    },

    PMU_LIB_NOCAT =>
    {
        DESCRIPTION         => "Enable/disable NOCAT library.",
        PROFILES            => [ 'pmu-tu10x...', ],
        SRCFILES            => [ "lib/lib_nocat.c", ],
    },

    # BOARDOBJ Features
    PMU_BOARDOBJ =>
    {
        DESCRIPTION         => "Enable/disable BOARDOBJ base object.",
        PROFILES            => [ 'pmu-*', ],
        SRCFILES            => [ "boardobj/lw/boardobj.c"        ,
                                 "boardobj/lw/boardobjgrp.c"     ,
                                 "boardobj/lw/boardobjgrp_e32.c" ,
                                 "boardobj/lw/boardobjgrp_e255.c",
                                 "boardobj/lw/boardobjgrpmask.c" , ],
    },

    PMU_BOARDOBJ_GET_STATUS_COPY_IN =>
    {
        DESCRIPTION         => "Enable copy-in of the GRP data within getStatus() code.",
        PROFILES            => [ 'pmu-tu10x...', ],
    },

    PMU_BOARDOBJ_VTABLE =>
    {
        DESCRIPTION         => "Enable/disable support for BOARDOBJ virtual table.",
        PROFILES            => [ 'pmu-*', ],
        SRCFILES            => [ "boardobj/lw/boardobj_vtable.c", ],
    },

    PMU_BOARDOBJ_INTERFACE =>
    {
        DESCRIPTION         => "Enable/disable BOARDOBJ INTERFACE base object.",
        PROFILES            => [ 'pmu-*', ],
        SRCFILES            => [ "boardobj/lw/boardobj_interface.c", ],
    },

    PMU_BOARDOBJ_CLASS_ID =>
    {
        DESCRIPTION         => "Enable/disable BOARDOBJ::classId variable.",
        PROFILES            => [ 'pmu-ga10x_riscv...', ],
    },

    PMU_BOARDOBJGRP_VIRTUAL_TABLES =>
    {
        DESCRIPTION         => "Enable/disable support for BOARDOBJGRPs storing virtual tables for each of its object's class types.",
        PROFILES            => [ ],
    },

    PMU_BOARDOBJ_DYNAMIC_CAST =>
    {
        DESCRIPTION         => "Enable/disable BOARDOBJ dynamic casting interface.",
        REQUIRES            => [ PMU_BOARDOBJGRP_VIRTUAL_TABLES, PMU_BOARDOBJ_CLASS_ID, ],
        PROFILES            => [ ],
    },

    PMU_BOARDOBJ_GRP_E512 =>
    {
        DESCRIPTION         => "Enable/disable BOARDOBJGRP_E512 support.",
        PROFILES            => [ 'pmu-gp100...', ],
        SRCFILES            => [ "boardobj/lw/boardobjgrp_e512.c", ],
    },

    PMU_BOARDOBJ_GRP_E1024 =>
    {
        DESCRIPTION         => "Enable/disable BOARDOBJGRP_E1024 support.",
        PROFILES            => [ 'pmu-gh100_riscv...', 'pmu-ad10x*', ],
        SRCFILES            => [ "boardobj/lw/boardobjgrp_e1024.c", ],
    },

    PMU_BOARDOBJ_GRP_E2048 =>
    {
        DESCRIPTION         => "Enable/disable BOARDOBJGRP_E2048 support.",
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ad10x*', ],
        SRCFILES            => [ "boardobj/lw/boardobjgrp_e2048.c", ],
    },

    PMU_BOARDOBJ_CONSTRUCT_VALIDATE =>
    {
        DESCRIPTION         => "Enable/disable BOARDOBJ constructor validation model.",
        REQUIRES            => [ PMU_BOARDOBJ, ],
        PROFILES            => [ ], # Under the development.
    },

    PMU_BOARDOBJGRP_SRC =>
    {
        DESCRIPTION         => "Whether the abstracted BOARDOBJGRP_SRC functionality is available.",
        REQUIRES            => [ PMU_BOARDOBJ, ],
        PROFILES            => [ 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv...', '-pmu-ad10b_riscv', '-pmu-gh100*', ],
        SRCFILES            => [ "boardobj/lw/boardobjgrp_src.c", ],
    },

    PMU_BOARDOBJGRP_SRC_VBIOS =>
    {
        DESCRIPTION         => "Whether the abstracted BOARDOBJGRP_SRC functionality is available.",
        REQUIRES            => [ PMU_BOARDOBJGRP_SRC, PMU_VBIOS_IMAGE_ACCESS, ],
        PROFILES            => [ 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv...', '-pmu-ad10b_riscv', '-pmu-gh100*', ],
        SRCFILES            => [ "boardobj/lw/boardobjgrp_src_vbios.c", ],
    },

    PMU_BOARDOBJ_MODEL_10 =>
    {
        DESCRIPTION         => "Controls whether the '1.0' version of the BOARDOBJ and BOARDOBJGRP model is enabled.",
        REQUIRES            => [ PMU_BOARDOBJ, ],
        PROFILES            => [ 'pmu-*', ],
        SRCFILES            => [ "boardobj/lw/boardobjgrp_iface_model_10.c",
                                 "boardobj/lw/boardobj_iface_model_10.c",       ],
    },

    # PerfMon Features

    PMU_PERFMON_PEX_UTIL_COUNTERS =>
    {
        DESCRIPTION         => "Enable/disable PEX utilization counters usage in PMU task 11",
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100...', '-pmu-gv11b', ], # TODO: re-enable PEX once XVE->XPL fix is in
        SRCFILES            => [ "pmu/maxwell/pmu_pwrgmxxx.c", ],
    },

    PMU_PERFMON_TIMER_BASED_SAMPLING =>
    {
        DESCRIPTION         => "Enable/disable timer-based utilization callwlations in PMU task 11",
        PROFILES            => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERFMON_NO_RESET_COUNTERS =>
    {
        DESCRIPTION         => "Enable/disable free running util counters PMU task 11",
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmu/pascal/pmu_pwrgpxxx.c", ],
    },

    # Queue Features

    PMU_QUEUE_HALT_ON_FULL =>
    {
        DESCRIPTION         => "Halt when the queue being used is full.  For debugging",
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    # CMDMGMT Features

    PMU_CMDMGMT_RPC =>
    {
        DESCRIPTION         => "Enable/disable code to handle CMDMGMT specific RPCs",
        PROFILES            => [ 'pmu-*', ],
    },

    PMU_CMDMGMT_RPC_DMEM_INFO_GET =>
    {
        DESCRIPTION         => "Enable/disable code to handle DMEM_INFO_GET specific RPCs",
        REQUIRES            => [ PMU_CMDMGMT_RPC, ],
        PROFILES            => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CMDMGMT_RPC_PMU_INIT_DONE =>
    {
        DESCRIPTION         => "Enable/disable support for the PMU_INIT_DONE RPC",
        REQUIRES            => [ PMU_CMDMGMT_RPC, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_CMDMGMT_SUPER_SURFACE_MEMBER_DESCRIPTORS =>
    {
        DESCRIPTION         => "Initialize super-surface member descriptor look-up table required for LWGPU",
        PROFILES            => [ ],
    },

    PMU_CMDMGMT_VC =>
    {
        DESCRIPTION         => "VC code coverage support",
        REQUIRES            => [ PMU_CMDMGMT_RPC,  ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
        SRCFILES            => [ "cmdmgmt/lw/cmdmgmt_vc.c", ],
    },

    PMU_VC_RAW_DATA =>
    {
        DESCRIPTION         => "VC coverage report will be raw data sent to RM",
        REQUIRES            => [ PMU_CMDMGMT_RPC, PMU_CMDMGMT_VC, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
    },

    PMU_CMDMGMT_SANITIZER_COV =>
    {
        DESCRIPTION         => "SanitizerCoverage (GCC coverage-guided fuzzing instrumentation) support.",
        REQUIRES            => [ PMU_CMDMGMT_RPC,  ],
        PROFILES            => [ 'pmu-*_riscv', '-pmu-ga1*b_riscv', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-g00x_riscv', ],
        SRCFILES            => [ "cmdmgmt/lw/cmdmgmt_sanitizer_cov.c", ],
    },

    PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER =>
    {
        DESCRIPTION         => "Support for allowing each task to use its own buffer for holding RPCs.",
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        REQUIRES            => [ PMU_FBQ, ],
    },

    PMU_CMDMGMT_RPC_AUTO_DMA =>
    {
        DESCRIPTION         => "Supports prefetching and write-backs for supersurface data via DMA before RPC handlers are called. ",
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-gh20x', '-pmu-gv11b', ],
        REQUIRES            => [ PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER, ARCH_RISCV, ],
    },

    PMU_CMDMGMT_INSTRUMENTATION =>
    {
        DESCRIPTION         => "Instrumentation support",
        REQUIRES            => [ PMU_CMDMGMT_RPC, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "cmdmgmt/lw/cmdmgmt_instrumentation.c", ],
    },

    PMU_CMDMGMT_USTREAMER =>
    {
        DESCRIPTION         => "uStreamer support",
        REQUIRES            => [ PMU_CMDMGMT_RPC, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "cmdmgmt/lw/cmdmgmt_ustreamer.c", ],
    },

    PMU_DETACH =>
    {
        DESCRIPTION         => "Controls PMU support for the DETACH",
        REQUIRES            => [ PMU_CMDMGMT_RPC, ],
        PROFILES            => [ 'pmu-*', ],
    },

    PMU_HALT_ON_DETACH =>
    {
        DESCRIPTION         => "Halts the PMU on detach",
        REQUIRES            => [ PMU_CMDMGMT_RPC, ],
        CONFLICTS           => [ PMU_THERM_DETACH_REQUEST, ],
        PROFILES            => [ 'pmu-ga100...', ],
    },

    # Security Features

    PMU_LIGHT_SELWRE_FALCON =>
    {
        DESCRIPTION         => "Light Secure Falcon functionality",
        ENGINES_REQUIRED    => [ LSF, ],
        PROFILES            => [ 'pmu-gm20x...', ],
    },

    PMU_LIGHT_SELWRE_FALCON_GC6_PRELOAD =>
    {
        DESCRIPTION         => "Control INIT overlays preloading.",
        REQUIRES            => [ PMU_LIGHT_SELWRE_FALCON, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_DYNAMIC_FLCN_PRIV_LEVEL =>
    {
        DESCRIPTION         => "Dynamic Falcon privilege level",
        PROFILES            => [ 'pmu-gm20x...', ],
        SRCFILES            => [ "pmu/maxwell/pmu_pmugm20x.c", ],
    },

    PMU_LOWER_PMU_PRIV_LEVEL =>
    {
        DESCRIPTION         => "Lower the PMU PRIV level so RM is able to reset PMU",
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_FUSE_ACCESS_SUPPORTED =>
    {
        DESCRIPTION         => "Support reading fuse registers in the PMU",
        ENGINES_REQUIRED    => [ FUSE, ],
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "fuse/lw/pmu_objfuse.c",
                                 "fuse/maxwell/pmu_fusegm200.c", ],
    },

    PMU_ALTER_JTAG_CLK_AROUND_FUSE_ACCESS =>
    {
        DESCRIPTION         => "Ungate JTAG clock/restore it's original gating setting before/after fuse reg accesses.",
        ENGINES_REQUIRED    => [ FUSE, ],
        REQUIRES            => [ PMU_FUSE_ACCESS_SUPPORTED, ],
        PROFILES            => [ 'pmu-gp10x...pmu-tu10x', '-pmu-gv11b', ],
        SRCFILES            => [ "fuse/maxwell/pmu_fusegm200.c", ],
    },

    PMU_QUEUE_MSG_INCREASE_SIZE =>
    {
        DESCRIPTION         => "Increase the message queue size",
        PROFILES            => [ 'pmu-gm20x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PRIV_SEC =>
    {
        DESCRIPTION         => "Enable/disable PMU privledge security support",
        PROFILES            => [ 'pmu-gm20x...', ],
    },

    PMU_QUEUE_REGISTERS_PROTECT =>
    {
        DESCRIPTION         => "Enable/disables PLM protection of the command queue tail and message queue head pointer registers.",
        REQUIRES            => [ PMU_PRIV_SEC, ],
        PROFILES            => [ 'pmu-tu10x...', ],
    },

    PMU_PRIV_SEC_RAISE_PRIV_LEVEL =>
    {
        DESCRIPTION         => "Write the priv level mask for selected registers to block level 0 and 1 access",
        REQUIRES            => [ PMU_PRIV_SEC, ],
        PROFILES            => [ 'pmu-gm20x', ],
        SRCFILES            => [ "pmu/maxwell/pmu_pmugm20x_only.c", ],
    },

    PMU_VERIFY_PRIV_SEC_ENABLED =>
    {
        DESCRIPTION         => "As part of new signing requirements as of ga100, we need to ensure that priv security is enabled on production (non-debug-fused) boards. Halt if not.",
        REQUIRES            => [ PMU_LIGHT_SELWRE_FALCON, PMU_PRIV_SEC, ],
        PROFILES            => [ 'pmu-ga100...', ],
    },

    PMU_VERIFY_VERSION =>
    {
        DESCRIPTION         => "Verify the ucode version is supported.",
        #
        # Note: the necessary registers don't seem to exist in the manuals for
        # g00x, so this feature is disabled for that profile.
        #
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-g00x_riscv', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_VERIFY_ENGINE =>
    {
        DESCRIPTION         => "Verify whether the microcode is actually running on the PMU",
        PROFILES            => [ 'pmu-ga10x_riscv...', ],
    },

    PMU_VERIFY_CHIP_FOR_PROFILE =>
    {
        DESCRIPTION         => "Whether to verify the running profile for the chip on which it's running",
        PROFILES            => [ 'pmu-ga10*', ],
    },

    PMU_VBIOS_SELWRITY_BYPASS =>
    {
        DESCRIPTION         => "Whether to cache the security bypass information from the VBIOS",
        PROFILES            => [ 'pmu-*', ],
    },

    PMU_WAIT_UNTIL_DONE_OR_TIMEOUT =>
    {
        DESCRIPTION         => "Enables the 'wait until done or timeout' interface.",
        PROFILES            => [ 'pmu-*', ],
    },

    PMU_MSCG_16FF_IOBRICK_PWRDOWNS =>
    {
        DESCRIPTION         => "16FF has added new cirlwitry within the IOBricks that must be powered-down for MSCG.",
        REQUIRES            => [ PMU_PG_MS, ],
        PROFILES            => [ 'pmu-gp10x', 'pmu-tu10x...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_VBIOS_BSOD_FETCH =>
    {
        DESCRIPTION         => "VBIOS related functions",
        ENGINES_REQUIRED    => [ VBIOS, ],
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv*', '-pmu-ga100', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "vbios/lw/objvbios.c"          ,
                                 "vbios/lw/vbios_ied.c"         ,
                                 "vbios/lw/vbios_opcodes.c"     ,
                                 "vbios/pascal/pmu_vbiosgp10x.c",
                                 "vbios/turing/pmu_vbiostu10x.c",],
    },

    PMU_VBIOS_DIRT =>
    {
        DESCRIPTION         => "Controls enablement of VBIOS Data ID Reference Table lookup functionality.",
        ENGINES_REQUIRED    => [ VBIOS, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_VBIOS_FRTS =>
    {
        DESCRIPTION         => "Controls whether the VBIOS FRTS functionality is enabled.",
        ENGINES_REQUIRED    => [ VBIOS, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_VBIOS_FRTS_ACCESS_DMA =>
    {
        DESCRIPTION         => "Controls whether FRTS is accessed via DMA.",
        REQUIRES            => [ PMU_VBIOS_FRTS, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_VBIOS_FRTS_ACCESS_DMA_BIT_INTERNAL_USE_ONLY =>
    {
        DESCRIPTION         => "Controls whether the BIT_INTERNAL_USE_ONLY table can be accessed via DMA from FRTS.",
        REQUIRES            => [ PMU_VBIOS_FRTS_ACCESS_DMA, ],
        PROFILES            => [ 'pmu-ga10x_selfinit_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_VBIOS_FRTS_ACCESS_DMA_PERFORMANCE_TABLE_6X =>
    {
        DESCRIPTION         => "Controls whether the Performance Table 6.X table can be accessed via DMA from FRTS.",
        REQUIRES            => [ PMU_VBIOS_FRTS_ACCESS_DMA, PMU_PERF_PSTATES_VBIOS_6X, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_VBIOS_FRTS_CERT30 =>
    {
        DESCRIPTION         => "Whether FRTS should expect to use the CERT30 structures.",
        REQUIRES            => [ PMU_VBIOS_FRTS, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_VBIOS_IMAGE_ACCESS =>
    {
        DESCRIPTION         => "Whether access to the VBIOS image is enabled or not.",
        REQUIRES            => [ PMU_VBIOS_DIRT, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_VBIOS_SKU =>
    {
        DESCRIPTION         => "Controls whether SKU information about the VBIOS is retrieved.",
        REQUIRES            => [ PMU_VBIOS_IMAGE_ACCESS, ],
        PROFILES            => [ 'pmu-ga10x_selfinit_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    # BIF features
    PMUTASK_BIF =>
    {
        DESCRIPTION         => "PMU BIF Task",
        ENGINES_REQUIRED    => [ BIF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "task_bif.c", ],
    },

    PMU_BIF_PMU_PERF_LIMIT =>
    {
        DESCRIPTION         => "Enable/Disable bif genspeed limits on PMU arbiter",
        REQUIRES            => [ PMUTASK_BIF, PMU_PERF_PERF_LIMIT, ],
        ENGINES_REQUIRED    => [ BIF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_BIF_XUSB =>
    {
        DESCRIPTION         => "USB-C feature on Turing",
        REQUIRES            => [ PMUTASK_BIF, ],
        ENGINES_REQUIRED    => [ BIF, ],
        PROFILES            => [ 'pmu-tu10x', ],
    },

    # Enabling only for GH100. Blackwell will probably look totally different for BIF
    PMU_BIF_ASPM_SPLIT_CONTROL =>
    {
        DESCRIPTION         => "GH100 and later, we have separate registers for ASPM control from CPU and PMU",
        REQUIRES            => [ PMUTASK_BIF, ],
        ENGINES_REQUIRED    => [ BIF, ],
        PROFILES            => [ 'pmu-gh100*', ],
    },

    PMU_BIF_LANE_MARGINING =>
    {
        DESCRIPTION         => "Handle lane margining commands",
        REQUIRES            => [ PMUTASK_LOWLATENCY, PMUTASK_BIF, ],
        PROFILES            => [ 'pmu-tu10x', 'pmu-ga100', 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', 'pmu-gh100_riscv', 'pmu-gb100_riscv', ],
    },

    PMU_BIF_LANE_MARGINING_LINK_RECOVERY_WAR =>
    {
        DESCRIPTION         => "Enable SW WAR for link entering into the recovery state during Lane Margining",
        REQUIRES            => [ PMU_BIF_LANE_MARGINING, ],
        PROFILES            => [ 'pmu-ga100', ],
    },

    PMU_BIF_LANE_MARGINING_HW_ERROR_THRESHOLD =>
    {
        DESCRIPTION         => "HW(and not SW) checks for error count against error threshold",
        REQUIRES            => [ PMU_BIF_LANE_MARGINING, ],
        PROFILES            => [ 'pmu-ga100', ],
    },

    PMU_BIF_PCIE_POISON_CONTROL_ERR_DATA_CONTROL =>
    {
        DESCRIPTION         => "Whether support for PCIE poison control error data modification is enabled or not.",
        REQUIRES            => [ PMUTASK_BIF, ],
        PROFILES            => [ 'pmu-gh100...', ]
    },

    # WATCHDOG features
    PMUTASK_WATCHDOG =>
    {
        DESCRIPTION         => "PMU Watchdog Task",
        REQUIRES            => [ PMU_UCODE_PROFILE_AUTO, ],
        PROFILES            => [ ],
        SRCFILES            => [ "task_watchdog.c", ],
    },

    # PERF_CF features
    PMUTASK_PERF_CF =>
    {
        DESCRIPTION         => "PMU PERF_CF Task",
        ENGINES_REQUIRED    => [ PERF_CF, ],
        CONFLICTS           => [ PMUTASK_PERFMON, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],

        SRCFILES            => [ "task_perf_cf.c"      ,
                                 "perf/lw/cf/perf_cf.c", ],
    },

    # LOWLATENCY features
    PMUTASK_LOWLATENCY =>
    {
        DESCRIPTION         => "PMU LOWLATENCY Task",
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "task_lowlatency.c", ],
    },

    PMU_PERF_CF_SENSOR =>
    {
        DESCRIPTION         => "Enable/disable PERF_CF_SENSOR object which represents a PERF_CF Sensors.",
        REQUIRES            => [ PMUTASK_PERF_CF, PMU_PG_STATS_64, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_sensor.c"           ,
                                 "perf/lw/cf/perf_cf_sensor_pmu.c"       ,
                                 "perf/lw/cf/perf_cf_sensor_pmu_fb.c"    ,
                                 "perf/lw/cf/perf_cf_sensor_pex.c"       ,
                                 "perf/lw/cf/perf_cf_sensor_ptimer.c"    ,
                                 "perf/lw/cf/perf_cf_sensor_ptimer_clk.c",
                                 "perf/lw/cf/perf_cf_sensor_clkcntr.c"   ,
                                 "perf/lw/cf/perf_cf_sensor_pgtime.c"    , ],
    },

    PMU_PERF_CF_SENSOR_THERM_MONITOR =>
    {
        DESCRIPTION         => "Enable/disable Thermal Monitor object within PERF_CF Sensors.",
        REQUIRES            => [ PMU_PERF_CF_SENSOR, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-ga100', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_sensor_therm_monitor.c", ],
    },

    PMU_PERF_CF_PM_SENSOR =>
    {
        DESCRIPTION         => "Enable/disable PERF_CF_PM_SENSOR object which represents PERF_CF_PM Sensors.",
        REQUIRES            => [ PMUTASK_PERF_CF, PMU_BOARDOBJ_GRP_E512, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_pm_sensor.c" , ],
    },

    PMU_PERF_CF_PM_SENSOR_SNAP_ELCG_WAR =>
    {
        DESCRIPTION         => "Enable/disable ELCG WAR to disable ELCG around Snap() of PERF_CF_PM_SENSOR.  Mitigates potential signal corruption from ELCG engagement during snap.",
        REQUIRES            => [ PMU_PERF_CF_PM_SENSOR, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CF_PM_SENSOR_V00 =>
    {
        DESCRIPTION         => "Enable/disable Version00 PERF_CF_PM_SENSOR object which represents a Tu10x PERF_CF_PM Sensors.",
        REQUIRES            => [ PMU_PERF_CF_PM_SENSOR, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_pm_sensor_v00.c" , ],
    },

    PMU_PERF_CF_PM_SENSOR_V10 =>
    {
        DESCRIPTION         => "Enable/disable Version10 PERF_CF_PM_SENSOR object which represents a Ga10x PERF_CF_PM Sensors.",
        REQUIRES            => [ PMU_PERF_CF_PM_SENSOR, PMU_PERF_CF_PM_SENSOR_SNAP_ELCG_WAR, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_pm_sensor_v10.c" , ],
    },

    PMU_PERF_CF_PM_SENSOR_CLW_V10 =>
    {
        DESCRIPTION         => "Enable/disable Version10 PERF_CF_PM_SENSOR_CLW objects which represents a PERF_CF_PM Sensors.",
        REQUIRES            => [ PMU_PERF_CF_PM_SENSOR, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-gh100_riscv...', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_pm_sensor_clw_v10.c" , ],
    },

    PMU_PERF_CF_PM_SENSOR_CLW_DEV_V10 =>
    {
        DESCRIPTION         => "Enable/disable Version10 PERF_CF_PM_SENSOR_CLW_DEV objects which represents a PERF_CF_PM Sensors.",
        REQUIRES            => [ PMU_PERF_CF_PM_SENSOR, PMU_PERF_CF_PM_SENSOR_CLW_V10, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-gh100_riscv...', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_pm_sensor_clw_dev_v10.c" , ],
    },

    PMU_PERF_CF_PM_SENSOR_CLW_MIG_V10 =>
    {
        DESCRIPTION         => "Enable/disable Version10 PERF_CF_PM_SENSOR_CLW_MIG objects which represents a PERF_CF_PM Sensors.",
        REQUIRES            => [ PMU_PERF_CF_PM_SENSOR, PMU_PERF_CF_PM_SENSOR_CLW_V10, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-gh100_riscv...', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_pm_sensor_clw_mig_v10.c" , ],
    },

    PMU_PERF_CF_PM_SENSORS_PMUMON =>
    {
        DESCRIPTION         => "Enable/disable, PERF_CF_PM_SENSOR PMUMON functionality",
        REQUIRES            => [ PMU_PERF_CF_PM_SENSOR, PMU_LIB_PMUMON, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-gh20x', '-pmu-gb100_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ 'perf/lw/cf/perf_cf_pm_sensors_pmumon.c', ],
    },

    PMU_PERF_CF_TOPOLOGY =>
    {
        DESCRIPTION         => "Enable/disable PERF_CF_TOPOLOGY object which represents a PERF_CF Topologies.",
        REQUIRES            => [ PMUTASK_PERF_CF, PMU_PERF_CF_SENSOR, PMU_BOARDOBJ_GET_STATUS_COPY_IN, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_topology.c"            ,
                                 "perf/lw/cf/perf_cf_topology_sensed_base.c",
                                 "perf/lw/cf/perf_cf_topology_min_max.c"    , ],
    },

    PMU_PERF_CF_TOPOLOGY_SENSED =>
    {
        DESCRIPTION         => "Enable/disable PERF_CF_TOPOLOGY_SENSED sub-class.",
        REQUIRES            => [ PMU_PERF_CF_TOPOLOGY, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_topology_sensed.c", ],
    },

    PMU_PERF_CF_TOPOLOGIES_PMUMON =>
    {
        DESCRIPTION         => "Enable/disable, PERF_CF_TOPOLOGIES PMUMON functionality",
        REQUIRES            => [ PMU_PERF_CF_TOPOLOGY, PMU_LIB_PMUMON, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-gh20x', '-pmu-gv11b', ],
        SRCFILES            => [ 'perf/lw/cf/perf_cf_topology_pmumon.c', ],
    },

    PMU_PERF_CF_CONTROLLER =>
    {
        DESCRIPTION         => "Enable/disable PERF_CF_CONTROLLER object which represents a PERF_CF Controllers.",
        REQUIRES            => [ PMUTASK_PERF_CF, PMU_PERF_CF_TOPOLOGY, PMU_PERF_PSTATES_35, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_controller.c"        ,
                                 "perf/lw/cf/perf_cf_controller_util.c"   ,
                                 "perf/lw/cf/perf_cf_controller_optp_2x.c", ],
    },

    PMU_CLIENT_PERF_CF_CONTROLLER =>
    {
        DESCRIPTION         => "Enable/disable CLIENT_PERF_CF_CONTROLLER object which represents a client PERF_CF Controllers.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/client_perf_cf_controller.c" , ],
    },

    PMU_CLIENT_PERF_CF_CONTROLLER_MEM_TUNE =>
    {
        DESCRIPTION         => "Enable/disable PMU_CLIENT_PERF_CF_CONTROLLER_MEM_TUNE.",
        REQUIRES            => [ PMU_CLIENT_PERF_CF_CONTROLLER, PMU_PERF_CF_CONTROLLER_MEM_TUNE, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
        SRCFILES            => [ "perf/lw/cf/client_perf_cf_controller_mem_tune.c", ],
    },

    PMU_PERF_CF_CONTROLLER_MEM_TUNE_ENHANCEMENT =>
    {
        DESCRIPTION         => "Enable/disable mem tune controller enhancement support.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER_MEM_TUNE, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
    },

    PMU_PERF_CF_CONTROLLER_DVCO_MIN_WAR =>
    {
        DESCRIPTION        => "Hack to workaround round down behavior below DVCO min of NAFLL clocks.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, ],
        CONFLICTS           => [ PMU_PERF_NAFLL_REGIME_V30, ],
        PROFILES            => [ 'pmu-tu10x', ],
    },

    PMU_PERF_CF_CONTROLLERS_1X =>
    {
        DESCRIPTION         => "Enable/disable PERF_CF_CONTROLLERS_2X group.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, ],
        CONFLICTS           => [ PMU_PERF_CF_CONTROLLERS_2X, ],
        PROFILES            => [ 'pmu-tu10x...pmu-ga100', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_controller_1x.c", ],
    },

    PMU_PERF_CF_CONTROLLERS_2X =>
    {
        DESCRIPTION         => "Enable/disable PERF_CF_CONTROLLERS_2X group.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, ],
        CONFLICTS           => [ PMU_PERF_CF_CONTROLLERS_1X, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_controller_2x.c", ],
    },

    PMU_PERF_CF_CONTROLLERS_PROFILING =>
    {
        DESCRIPTION         => "Enable/disable PERF_CF_CONTROLLERS profiling.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_PERF_CF_CONTROLLER_PWR_POLICY_INFLECTION_POINTS_DISABLE =>
    {
        DESCRIPTION         => "Enable/disable the ability for PERF_CF_CONTROLLERs to request inflection points be disabled.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CF_CONTROLLER_MEM_TUNE =>
    {
        DESCRIPTION         => "Enable/disable PMU_PERF_CF_CONTROLLER_MEM_TUNE.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLERS_2X, PMU_PERF_CF_PM_SENSOR_V10, PMU_CLK_FBFLCN_TRRD_PROGRAM_SUPPORTED, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_controller_mem_tune.c", ],
    },

    PMU_PERF_CF_CONTROLLER_MEM_TUNE_2X =>
    {
        DESCRIPTION         => "Enable/disable PMU_PERF_CF_CONTROLLER_MEM_TUNE_2X.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLERS_2X, PMU_PERF_CF_PM_SENSOR_V10, PMU_CLK_FBFLCN_TRRD_PROGRAM_SUPPORTED, ],
        PROFILES            => [ ], # DO NOT ENABLE THIS FEATURE WITHOUT PROPER PERMISSIONS.
        SRCFILES            => [ "perf/lw/cf/perf_cf_controller_mem_tune_2x.c", ],
    },

    PMU_PERF_CF_CONTROLLER_MEM_TUNE_OVERRIDE =>
    {
        DESCRIPTION         => "Enable/disable PMU_PERF_CF_CONTROLLER_MEM_TUNE override based on active PCIe and display settings.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER_MEM_TUNE, ],
        PROFILES            => [ ], # DO NOT ENABLE THIS FEATURE WITHOUT PROPER PERMISSIONS.
    },

    PMU_PERF_CF_CONTROLLER_MEM_TUNE_STATIC_ALWAYS_ON_OVERRIDE =>
    {
        DESCRIPTION         => "Enable/disable PMU_PERF_CF_CONTROLLER_MEM_TUNE static always-on override based on SKUs.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER_MEM_TUNE, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ], # DO NOT ENABLE THIS FEATURE WITHOUT PROPER PERMISSIONS.
    },

    PMU_PERF_CF_CONTROLLER_UTIL_2X =>
    {
        DESCRIPTION         => "Enable/disable PMU_PERF_CF_CONTROLLER_UTIL_2X.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLERS_2X, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_controller_util_2x.c", ],
    },

    PMU_PERF_CF_CONTROLLER_FREQ_FLOOR_AT_POR_MIN_DISABLED =>
    {
        DESCRIPTION         => "Enable/disable the ability for PERF_CF_CONTROLLERs to request frequency floor at POR min frequency.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, ],
        PROFILES            => [ 'pmu-gh100...', ],
    },

    PMU_PERF_CF_CONTROLLER_UTIL_THRESHOLD_SCALE_BY_VM_COUNT =>
    {
        DESCRIPTION         => "Enable/Disable PMU_PERF_CF_CONTROLLER_UTIL Threshold Scaling based on VM Count Support",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL =>
    {
        DESCRIPTION         => "Enable/disable usage of PWR_CHANNELs in multiplier data.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, PMU_PMGR_PWR_MONITOR, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY =>
    {
        DESCRIPTION         => "Enable/disable usage of PWR_POLICYs in multiplier data.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, PMU_PMGR_PWR_POLICY, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CF_CONTROLLER_DLPPC_1X =>
    {
        DESCRIPTION         => "Enable/disable the DLPPC 1.X PERF_CF controller",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, PMU_PERF_CF_CONTROLLERS_2X, PMU_PERF_CF_PWR_MODEL_DLPPM_1X, PMU_PERF_PERF_LIMIT, PMU_PMGR_PWR_EQUATION_DYNAMIC, PMU_PERF_CF_CONTROLLER_PWR_POLICY_INFLECTION_POINTS_DISABLE, PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL, PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_controller_dlppc_1x.c", ],
    },

    PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING =>
    {
        DESCRIPTION         => "Enable/disable profiling of the DLPPC 1.X PERF_CF controller",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER_DLPPC_1X, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_PERF_CF_CONTROLLER_XBAR_FMAX_AT_VOLT =>
    {
        DESCRIPTION         => "Enable/disable the support for Fmax @ V behavior for XBAR and its secondaries.",
        REQUIRES            => [ PMU_PERF_CF_CONTROLLER, PMU_PERF_PERF_LIMIT, PMU_PERF_CF_CONTROLLERS_2X, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CF_POLICY =>
    {
        DESCRIPTION         => "Enable/disable PERF_CF_POLICY object which represents a PERF_CF Policies.",
        REQUIRES            => [ PMUTASK_PERF_CF, PMU_PERF_CF_CONTROLLER, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_policy.c"          ,
                                 "perf/lw/cf/perf_cf_policy_ctrl_mask.c", ],
    },

    PMU_CLIENT_PERF_CF_POLICY =>
    {
        DESCRIPTION         => "Enable/disable CLIENT_PERF_CF_POLICY object which represents a client PERF_CF Policies.",
        REQUIRES            => [ PMU_PERF_CF_POLICY, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/client_perf_cf_policy.c"          ,
                                 "perf/lw/cf/client_perf_cf_policy_ctrl_mask.c", ],
    },

    PMU_PERF_CF_PWR_MODEL =>
    {
        DESCRIPTION         => "Enable/disable PERF_CF_PWR_MODEL object which represents a PERF_CF Pwr Models.",
        REQUIRES            => [ PMUTASK_PERF_CF, PMU_PERF_CF_TOPOLOGY, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/perf_cf_pwr_model.c", ],
    },

    PMU_PERF_CF_PWR_MODEL_SCALE_RPC =>
    {
        DESCRIPTION         => "Enable/disable the PERF_CF PWR_MODEL_SCALE RPC.",
        REQUIRES            => [ PMU_PERF_CF_PWR_MODEL, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CF_PWR_MODEL_SCALE_RPC_ODP =>
    {
        DESCRIPTION         => "Enable/disable the PERF_CF PWR_MODEL_SCALE RPC for profiles with ODP.",
        REQUIRES            => [ PMU_PERF_CF_PWR_MODEL_SCALE_RPC, ],
        CONFLICTS           => [ PMU_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP =>
    {
        DESCRIPTION         => "Enable/disable the PERF_CF PWR_MODEL_SCALE RPC for profiles without ODP.",
        REQUIRES            => [ PMU_PERF_CF_PWR_MODEL_SCALE_RPC, ],
        CONFLICTS           => [ PMU_PERF_CF_PWR_MODEL_SCALE_RPC_ODP, ],
        PROFILES            => [ 'pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CF_PWR_MODEL_DLPPM_1X =>
    {
        DESCRIPTION        => "Enable/disable PERF_CF_PWR_MODEL_DLPPM_1X power model.",
        REQUIRES           => [ PMU_PERF_CF_PWR_MODEL, PMU_PERF_CF_PM_SENSOR, ],
        ENGINES_REQUIRED   => [ PERF_CF, NNE, ],
        CONFLICTS          => [ PMU_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP, ],
        PROFILES           => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES           => [ "perf/lw/cf/perf_cf_pwr_model_dlppm_1x.c", ],
    },

    PMU_PERF_CF_PWR_MODEL_DLPPM_1X_PROFILING =>
    {
        DESCRIPTION        => "Enable/disable PERF_CF_PWR_MODEL_DLPPM_1X profiling.",
        REQUIRES           => [ PMU_PERF_CF_PWR_MODEL_DLPPM_1X, ],
        PROFILES           => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
    },

    PMU_PERF_CF_PWR_MODEL_TGP_1X =>
    {
        DESCRIPTION        => "Enable/disable PERF_CF_PWR_MODEL_TGP_1X power model.",
        REQUIRES           => [ PMU_PERF_CF_PWR_MODEL, PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X, ],
        ENGINES_REQUIRED   => [ PERF_CF, ],
        PROFILES           => [ 'pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES           => [ "perf/lw/cf/perf_cf_pwr_model_tgp_1x.c", ],
    },

    PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE =>
    {
        DESCRIPTION         => "Enable/disable CLIENT_PERF_CF_PWR_MODEL_PROFILE object which represents a Client PERF_CF Pwr Model Profiles.",
        REQUIRES            => [ PMUTASK_PERF_CF, PMU_PERF_CF_PWR_MODEL, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/client_perf_cf_pwr_model_profile.c", ],
    },

    PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X =>
    {
        DESCRIPTION         => "Enable/disable TGP_1X CLIENT_PERF_CF_PWR_MODEL_PROFILE object class.",
        REQUIRES            => [ PMUTASK_PERF_CF, PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/cf/client_perf_cf_pwr_model_profile_tgp_1x.c", ],
    },

    PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_RPC =>
    {
        DESCRIPTION         => "Enable/disable CLIENT_PERF_CF_PWR_MODEL_PROFILE scaling RPC.",
        REQUIRES            => [ PMUTASK_PERF_CF, PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE, PMU_CLK_CLK_DOMAIN_CLIENT_INDEX_MAP, ],
        ENGINES_REQUIRED    => [ PERF_CF, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_IDLE_COUNTERS =>
    {
        DESCRIPTION        => "Enable/disable support for the PMU's idle counters",
        PROFILES           => [ 'pmu-*', ],
    },

    PMU_IS_ZERO_FB =>
    {
        DESCRIPTION         => "Whether PMU supports running a 0FB config",
        PROFILES            => [ 'pmu-tu10x...', ],
    },

    PMU_PMGR_ILLUM =>
    {
        DESCRIPTION         => "Enable/disable Illumination Control feature.",
        REQUIRES            => [ PMUTASK_PMGR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/objillum.c", ],
    },

    PMU_PMGR_ILLUM_DEVICE =>
    {
        DESCRIPTION         => "Enable/disable ILLUM_DEVICE object representing an Illumination Device.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_ILLUM, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/illum_device.c",
                                 "pmgr/lw/illum_device_mlwv10.c",
                                 "pmgr/lw/illum_device_gpio_pwm_single_color_v10.c",
                                 "pmgr/lw/illum_device_gpio_pwm_rgbw_v10.c", ],
    },

    PMU_PMGR_ILLUM_ZONE =>
    {
        DESCRIPTION         => "Enable/disable ILLUM_ZONE object representing an Illumination Zone.",
        REQUIRES            => [ PMUTASK_PMGR, PMU_PMGR_ILLUM, PMU_PMGR_ILLUM_DEVICE, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/illum_zone.c",
                                 "pmgr/lw/illum_zone_rgb.c",
                                 "pmgr/lw/illum_zone_rgbw.c",
                                 "pmgr/lw/illum_zone_single_color.c", ],
    },

    PMU_ILLUM_DEVICE_GPIO =>
    {
        DESCRIPTION         => "GPIO based LED control is supported only from Ampere",
        PROFILES            => [ 'pmu-ga10x_riscv...', ],
    },

    PMU_ILLUM_DISP_SOR_CLK_WAR_BUG_200629496 =>
    {
        DESCRIPTION         => "Enable/disable the SOR clk WAR, where SOR PWM is using gated clk",
        REQUIRES            => [ PMU_PMGR_ILLUM_DEVICE, ],
        PROFILES            => [ 'pmu-tu10x', 'pmu-ga10*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-gv11b', ],
    },

    PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR =>
    {
        DESCRIPTION         => "Enable/disable piecewise frequency floor functionality.",
        REQUIRES            => [ PMUTASK_PMGR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pff.c", ],
    },

    PMU_PMGR_PERF_VF_ILWALIDATION_NOTIFY =>
    {
        DESCRIPTION         => "Enable/disable notifications from perf that the VF lwrve has been ilwalidated.",
        REQUIRES            => [ PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_PERF_VF_ILWALIDATION_NOTIFY =>
    {
        DESCRIPTION         => "Enable/disable notifications from perf that the VF lwrve has been ilwalidated.",
        REQUIRES            => [ PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_THERM_VID_PWM_TRIGGER_MASK =>
    {
        DESCRIPTION         => "Enable/disable the VID PWM trigger mask interface.",
        PROFILES            => [ 'pmu-gp10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_HOST_ENGINE_EXPANSION =>
    {
        DESCRIPTION         => "Host engine expansion support in HW (Ampere-10)",
        ENGINES_REQUIRED    => [ FIFO, ],
        PROFILES            => [ 'pmu-ga100...', ],
    },

    PMU_RISCV_COMPILATION_HACKS =>
    {
        DESCRIPTION         => "RISCV compilation hacks enabled/disabled",
        PROFILES            => [ 'pmu-*_riscv', ],
    },

    PMU_PWR_POLICY_INTEGRAL_LIMIT_BOUNDS_REQUIRED =>
    {
        DESCRIPTION         => "No need to bound power policy limits",
        PROFILES            => [ '...pmu-gv10x', ],
    },

    # LWLINK features
    PMU_CMDMGMT_LWLINK_HSHUB =>
    {
        DESCRIPTION         => "Update HSHUB registers for LWLink/PCIe traffic through cmdmgmt",
        REQUIRES            => [ PMU_CMDMGMT_RPC, ],
        ENGINES_REQUIRED    => [ LWLINK, ],
        PROFILES            => [ 'pmu-gp100', 'pmu-gv10x', ],
        SRCFILES            => [ "lwlink/pascal/pmu_lwlinkgp100.c",
                                 "lwlink/volta/pmu_lwlinkgv10x.c" ,
                                 "cmdmgmt/lw/cmdmgmt_lwlink.c"    , ],
    },

    PMU_PMGR_RPC_TEST_EXELWTE =>
    {
        DESCRIPTION         => "Enable/disable support for exelwting Pmgr tests",
        REQUIRES            => [ PMUTASK_PMGR, ],
        ENGINES_REQUIRED    => [ PMGR, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "pmgr/lw/pmgrtest.c", ],
    },

    PMU_PERF_CHANGE_SEQ =>
    {
        DESCRIPTION         => "Enable/disable PMU support for perf change sequence",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_ELW_PSTATES, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/changeseq.c", ],
    },

    PMU_PERF_CHANGE_SEQ_ODP =>
    {
        DESCRIPTION         => "Enable/disable PMU support for perf change sequence that rely on PMU ODP",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CHANGE_SEQ_NOTIFICATION =>
    {
        DESCRIPTION         => "Enable/disable PMU support for perf change sequencer notification",
        REQUIRES            => [ PMUTASK_PERF, PMU_PERF_ELW_PSTATES, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    # Perf change sequencer dependents on multiple engines so must be at the end.

    PMU_PERF_CHANGE_SEQ_35 =>
    {
        DESCRIPTION         => "Enable/disable PMU support for perf change sequence 3.5",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ, PMU_PERF_ELW_PSTATES_35, PMUTASK_PERF_DAEMON, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/35/changeseq_35.c"       ,
                                 "perf/lw/changeseq_daemon.c"      ,
                                 "perf/lw/35/changeseq_35_daemon.c",
                                 "perf/lw/35/changeseqscriptstep_clk.c", ],
    },

    PMU_PERF_CHANGE_SEQ_LPWR =>
    {
        DESCRIPTION         => "Enable/disable PMU support for perf change sequence LPWR scripts such as GPC-RG",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ, PMU_PERF_ELW_PSTATES_35, PMUTASK_PERF_DAEMON, PMU_PG_GR_RG, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/changeseq_lpwr_daemon.c",
                                 "perf/lw/changeseq_lpwr.c",         ],
    },

    PMU_PERF_CHANGE_SEQ_LPWR_STEP_CLK =>
    {
        DESCRIPTION         => "Enable/disable PMU support for perf change sequence LPWR script's Clock init / deinit and NAFLL init steps.",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_LPWR, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CHANGE_SEQ_LPWR_STEP_VOLT =>
    {
        DESCRIPTION         => "Enable/disable PMU support for perf change sequence LPWR script's volt rail gate / ungate steps.",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_LPWR, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CHANGE_SEQ_LPWR_STEP_LPWR =>
    {
        DESCRIPTION         => "Enable/disable PMU support for perf change sequence LPWR script's LPWR steps around voltage gate / ungate steps.",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_LPWR, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CHANGE_SEQ_LPWR_PERF_RESTORE =>
    {
        DESCRIPTION         => "Enable/disable PMU support for perf change sequence LPWR script's sequence to restore perf on feature exit.",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_LPWR, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100*', '-pmu-gb100_riscv', '-pmu-g00x_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CHANGE_SEQ_LPWR_PERF_RESTORE_SANITY =>
    {
        DESCRIPTION         => "Enable/disable PMU support for perf change sequence LPWR script's sequence to restore perf on feature exit.",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_LPWR, PMU_PERF_CHANGE_SEQ_LPWR_PERF_RESTORE, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ ], # We are observing some failures that needs to be root caused before re-enable. Bug # 200677168
    },

    PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_RM =>
    {
        DESCRIPTION         => "Enable/disable RM support for exelwting pre | post perf change steps",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ, PMUTASK_PERF_DAEMON, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ ], # Disable on all as it has very high impact on VF switch latency.
    },

    PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_PMU =>
    {
        DESCRIPTION         => "Enable/disable PMU support for exelwting pre | post perf change steps",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ, PMUTASK_PERF_DAEMON, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/changeseqscriptstep_change.c", ],
    },

    PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_PSTATE_RM =>
    {
        DESCRIPTION         => "Enable/disable RM support for exelwting pre | post pstate change steps",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ, PMUTASK_PERF_DAEMON, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_PSTATE_PMU =>
    {
        DESCRIPTION         => "Enable/disable PMU support for exelwting pre | post pstate change steps",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ, PMUTASK_PERF_DAEMON, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/changeseqscriptstep_pstate.c", ],
    },

    PMU_PERF_CHANGE_SEQ_STEP_VOLT =>
    {
        DESCRIPTION         => "Enable/disable PMU support for exelwting perf change volt step",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ, PMUTASK_PERF_DAEMON, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/changeseqscriptstep_volt.c", ],
    },

    PMU_PERF_CHANGE_SEQ_STEP_LPWR =>
    {
        DESCRIPTION         => "Enable/disable PMU support for exelwting perf change LPWR step",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ, PMUTASK_PERF_DAEMON, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ ], # Disable on all as it is not being used on any SKUs.
        SRCFILES            => [ "perf/lw/changeseqscriptstep_lpwr.c", ],
    },

    PMU_PERF_CHANGE_SEQ_STEP_BIF =>
    {
        DESCRIPTION         => "Enable/disable PMU support for exelwting perf change BIF step",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ, PMUTASK_PERF_DAEMON, PMUTASK_BIF, ],
        ENGINES_REQUIRED    => [ PERF, BIF ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/changeseqscriptstep_bif.c", ],
    },

    PMU_PERF_CHANGE_SEQ_STEP_NOISE_AWARE_UNAWARE_CLKS =>
    {
        DESCRIPTION         => "Enable/disable PMU support for exelwting perf change noise aware vs unaware clock steps",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_31, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ ],
    },

    PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS =>
    {
        DESCRIPTION         => "Enable/disable PMU support for exelwting perf change pre | post voltage clocks steps",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_35, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS =>
    {
        DESCRIPTION         => "Enable/disable PMU support for exelwting perf change pre | post voltage NAFLL clocks steps",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_35, PMU_PERF_NAFLL_BYPASS_CLKS_ON_PMU, PMU_UCODE_PROFILE_AUTO, ],
        CONFLICTS           => [ PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ ], # NJ-TODO
        SRCFILES            => [ "perf/lw/35/changeseqscriptstep_nafllclk.c", ],
    },

    PMU_PERF_CHANGE_SEQ_STEP_CLK_MON =>
    {
        DESCRIPTION         => "Enable/disable PMU support for updating clock monitors during perf change.",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_35, PMU_CLK_CLOCK_MON, PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_PMU, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga100', ],
        SRCFILES            => [ "perf/lw/35/changeseqscriptstep_clkmon.c", ],
    },

    PMU_PERF_DAEMON_FBFLCN =>
    {
        DESCRIPTION         => "Enable/Disable the support for Perf Daemon Fbflcn functionality",
        REQUIRES            => [ PMUTASK_PERF_DAEMON, ],
        ENGINES_REQUIRED    => [ PERF, CLK, ],
        PROFILES            => [ 'pmu-gv10*', 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
        SRCFILES            => [ "perf/lw/perf_daemon_fbflcn.c", ],
    },

    PMU_PERF_CHANGE_SEQ_VOLT_OFFSET_CHANGE =>
    {
        DESCRIPTION         => "Enable/disable PMU support for updating voltage offset through perf change sequence.",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_35, ],
        CONFLICTS           => [ PMU_CLK_CLK_FREQ_CONTROLLER_V10, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE =>
    {
        DESCRIPTION         => "Enable/disable PMU support for updating memory settings tuning through perf change sequence.",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_35, PMU_CLK_FBFLCN_TRRD_PROGRAM_SUPPORTED, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
        SRCFILES            => [ "perf/lw/changeseqscriptstep_mem_tune.c", ],
    },

    PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK =>
    {
        DESCRIPTION         => "Enable/disable PMU support for temporarily XBAR boost around MCLK switch through perf change sequence.",
        REQUIRES            => [ PMU_PERF_CHANGE_SEQ_35, PMU_PERF_ELW_PSTATES_40, ],
        CONFLICTS           => [ PMU_PERF_CHANGE_SEQ_STEP_CLK_MON, PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS, ],
        ENGINES_REQUIRED    => [ PERF, ],
        PROFILES            => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_ACR_TEGRA_PROFILE =>
    {
        DESCRIPTION         => "Comment out ACR code not used by PMU CheetAh Profiles",
        ENGINES_REQUIRED    => [ ACR, ],
        PROFILES            => [ 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', 'pmu-gv11b', ],
    },

    PMU_ACR_SEC2_PRESENT =>
    {
        DESCRIPTION         => "Enable/Disable SEC2 specific code in ACR",
        ENGINES_REQUIRED    => [ ACR, ],
        PROFILES            => [ 'pmu-gm20x', 'pmu-gp100', 'pmu-gp10x', 'pmu-gv10x', ],
    },

    PMU_ACR_LWENC_PRESENT =>
    {
        DESCRIPTION         => "Enable/Disable LWENC specific code in ACR",
        ENGINES_REQUIRED    => [ ACR, ],
        PROFILES            => [ 'pmu-gm20x', 'pmu-gp100', ],
    },

    PMU_ACR_LWDEC_PRESENT =>
    {
        DESCRIPTION         => "Enable/Disable LWDEC specific code in ACR",
        ENGINES_REQUIRED    => [ ACR, ],
        PROFILES            => [ 'pmu-gm20x', 'pmu-gp100', 'pmu-gp10x', 'pmu-gv10x', ],
    },

    PMU_ACR_DPU_PRESENT =>
    {
        DESCRIPTION         => "Enable/Disable DPU specific code in ACR",
        ENGINES_REQUIRED    => [ ACR, ],
        PROFILES            => [ 'pmu-gm20x', 'pmu-gp100', 'pmu-gp10x', ],
    },

    PMU_ACR_GSP_PRESENT =>
    {
        DESCRIPTION         => "Enable/Disable SEC2 specific code in ACR",
        ENGINES_REQUIRED    => [ ACR, ],
        PROFILES            => [ 'pmu-gv10x', ],
    },

    PMU_ACR_UNLOAD =>
    {
        DESCRIPTION         => "ACR_UNLOAD define is originally used in ACR for defining PMU specific variables",
        ENGINES_REQUIRED    => [ ACR, ],
        PROFILES            => [ 'pmu-gm20x', 'pmu-gp100', 'pmu-gp10x', 'pmu-gv10x', ],
    },

    PMU_ACR_DMEM_APERT_ENABLED =>
    {
        DESCRIPTION         => "Enable/Disable DMEM aperture",
        ENGINES_REQUIRED    => [ ACR, ],
        PROFILES            => [ 'pmu-gm20x', 'pmu-gp100', 'pmu-gp10x', 'pmu-gv10x', ],
    },

    PMU_ACR_HS_PARTITION =>
    {
        DESCRIPTION         => "Enable/Disable RISCV HS partition",
        REQUIRES            => [ PMU_ACR_TEGRA_PROFILE, ARCH_RISCV, ],
        ENGINES_REQUIRED    => [ ACR, ],
        PROFILES            => [ 'pmu-ga1*b_riscv', 'pmu-ga10f_riscv', ],
    },

    # IC features
    PMU_IC_MISCIO =>
    {
        DESCRIPTION         => "Support for the MISCIO interrupt.",
        PROFILES            => [ 'pmu-*', ],
    },

    PMU_IC_IO_BANK_2_SUPPORTED =>
    {
        DESCRIPTION         => "Enable this feature if interrupts on GPIO Bank2 need to be handled",
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    PMU_IC_ELPG_INTERRUPTS =>
    {
        DESCRIPTION         => "Support for ELPG interrupts.",
        PROFILES            => [ 'pmu-*', ],
    },

    PMU_IC_HOLDOFF_INTERRUPT =>
    {
        DESCRIPTION         => "Support for the holdoff interrupts.",
        PROFILES            => [ 'pmu-*', ],
    },

    # memory subsystem features
    PMU_FB_WAR_BUG_2805650 =>
    {
        DESCRIPTION         => "SW WAR for bug 2805650 that uses PMU to write the PROD value for LW_PLTS_TSTG_CFG_5_DISCARD_ALL_DATA",
        PROFILES            => [ 'pmu-ga100', ],
    },

    PMU_HOSTCLK_PRESENT =>
    {
        DESCRIPTION         => "Enable this feature for profiles without HOSTCLK domain",
        PROFILES            => [ 'pmu-*', '-pmu-gh100...', '-pmu-g00x_riscv', ],
    },

    PMU_SANDBAG_ENABLED =>
    {
        DESCRIPTION         => "Explicitely limit SANDBAG feature to desired profiles",
        PROFILES            => [ 'pmu-*riscv', ],
    },

    # NOCAT features
    PMU_NOCAT_TEMPSIM =>
    {
        DESCRIPTION         => "Enable/disable NOCAT TempSim notification",
        REQUIRES            => [ PMU_THERM_SENSOR, ],
        PROFILES            => [ 'pmu-tu10x...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gv11b', ],
    },

    # VGPU features
    PMU_CMDMGMT_VGPU_VF_BAR1_SIZE_UPDATE =>
    {
        DESCRIPTION         => "Update the per-VF BAR1 size",
        REQUIRES            => [ PMU_CMDMGMT_RPC, ],
        ENGINES_REQUIRED    => [ VGPU, ],
        PROFILES            => [ 'pmu-ga10x_riscv', ],
    },
];

# Create the Features group
sub new {

    @_ == 1 or croak 'usage: obj->new()';

    my $type = shift;

    my $self = Groups->new("feature", $featuresRef);

    $self->{GROUP_PROPERTY_INHERITS} = 1;   # FEATUREs inherit based on name

    return bless $self, $type;
}

# end of the module
1;
