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
# dpu-config file that specifies all known dpu-config features
#

package Features;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Features is derived from 'Groups'

@ISA = qw(Groups);


# This list contains all dpu-config features.
#
my $featuresRef = [

    # platform the software will run on.
    # For now, just have unknown and DPU
    PLATFORM_UNKNOWN =>
    {
       DESCRIPTION         => "Running on an unknown platform",
       DEFAULT_STATE       => DISABLED,
    },
    PLATFORM_DPU =>
    {
       DESCRIPTION         => "Running on the DPU",
       DEFAULT_STATE       => DISABLED,
    },

    # target architecture
    ARCH_UNKNOWN =>
    {
       DESCRIPTION         => "unknown arch",
       DEFAULT_STATE       => DISABLED,
    },
    ARCH_FALCON =>
    {
       DESCRIPTION         => "FALCON architecture",
       DEFAULT_STATE       => DISABLED,
    },


    # This feature is special.  It and the DPUCORE_<<gpuFamily>> features will
    # be automatically enabled by rmconfig based on the enabled gpus.
    DPUCORE_BASE =>
    {
       DESCRIPTION         => "DPUCORE Base",
    },

    DPUCORE_GF11X =>
    {
       DESCRIPTION         => "DPUCORE GF11x/v02_00",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "hal/v02/dpu_hal0200.c" ],
    },

    DPUCORE_GK10X =>
    {
       DESCRIPTION         => "DPUCORE KEPLER/v02_01",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "hal/v02/dpu_hal0201.c" ],
    },

    DPUCORE_GM20X =>
    {
       DESCRIPTION         => "DPUCORE GM20X/v02_05",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "hal/v02/dpu_hal0205.c" ],
    },

    DPUCORE_GP10X =>
    {
       DESCRIPTION         => "DPUCORE GP10X/v02_07",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "hal/v02/dpu_hal0207.c" ],
    },

    DPUCORE_GV10X =>
    {
       DESCRIPTION         => "DPUCORE GV10X (GSP-Lite)",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "hal/volta/dispflcn_halgv100.c" ],
    },

    DPUCORE_TU10X =>
    {
       DESCRIPTION         => "DPUCORE TU10X (GSP)",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "hal/turing/dispflcn_haltu10x.c" ],
    },

    DPUCORE_GA10X =>
    {
       DESCRIPTION         => "DPUCORE GA10X (GSP)",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "hal/ampere/dispflcn_halga10x.c" ],
    },

    DPUCORE_AD10X =>
    {
       DESCRIPTION         => "DPUCORE AD10X (GSP)",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "hal/ada/dispflcn_halad10x.c" ],
    },

    DPUTASK_DISPATCH =>
    {
        DESCRIPTION        => "DPU Command Management Task (CORE)",
        DEFAULT_STATE      => ENABLED,
        SRCFILES           => [ "task1_mgmt.c" ],
    },

    DPUTASK_WKRTHD =>
    {
       DESCRIPTION         => "DPU Worker Thread Task (CORE)",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ "task_wkrthd.c" ],
    },

    DPUJOB_HS_SWITCH_TEST =>
    {
       DESCRIPTION         => "GSPLite/GSP HS Switch Job to test LS/HS switching",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    DPUJOB_RTTIMER_TEST =>
    {
       DESCRIPTION         => "RTTIMER test Job",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    DPUJOB_MSCG_TEST    =>
    {
       DESCRIPTION         => "GSP MSCG wake-up test (core support). Includes priv blocker subtest.",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    DPUJOB_MSCG_FBBLOCKER_TEST    =>
    {
       DESCRIPTION         => "GSP MSCG wake-up fb blocker subtest.",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    DPUTASK_HDCP =>
    {
        DESCRIPTION        => "Hdcp Task",
        DEFAULT_STATE      => DISABLED,
        SRCFILES           => [ "task5_hdcp.c",
                                "hdcp/dpu_libintfchdcp.c",
                              ],
    },

    DPUTASK_HDCP22WIRED =>
    {
        DESCRIPTION        => "Hdcp2.2 wired Task",
        DEFAULT_STATE      => DISABLED,
        SRCFILES           => [ "task6_hdcp22.c",
                                "hdcp22/dpu_libintfchdcp22wired.c",
                              ],
    },

    DPU_I2C =>
    {
       DESCRIPTION         => "Dpu interface for I2C library",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "i2c/dpu_libintfci2c.c", ],
    },

    DPUTASK_SCANOUTLOGGING =>
    {
        DESCRIPTION         => "ScanoutLogging Task",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [ "task8_scanoutlogging.c" ],
    },

    DPUTASK_MSCGWITHFRL =>
    {
        DESCRIPTION         => "MSCG with FRL Task",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [ "task9_mscgwithfrl.c" ],
        ENGINES_REQUIRED    => [ ISOHUB ],
    },

    DPUTASK_VRR =>
    {
       DESCRIPTION         => "Variable Refresh Rate Task",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "task4_vrr.c",
                                "vrr/lw/dpu_objvrr.c",
                              ],
       ENGINES_REQUIRED    => [ VRR ],
    },

    DBG_PRINTF_OS_TRACE_FB =>
    {
       DESCRIPTION         => "Redirect DPU Debug Spew to falc_trace (DMA memory logger)",
       DEFAULT_STATE       => DISABLED,
       CONFLICTS           => [ DBG_PRINTF_OS_TRACE_REG, ],
    },

    DBG_PRINTF_OS_TRACE_REG =>
    {
       DESCRIPTION         => "Redirect DPU Debug Spew to falc_debug",
       DEFAULT_STATE       => DISABLED,
       CONFLICTS           => [ DBG_PRINTF_OS_TRACE_FB, ],
    },

    LIGHT_SELWRE_FALCON =>
    {
       DESCRIPTION         => "Light Secure Falcon functionality",
       DEFAULT_STATE       => DISABLED,
       ENGINES_REQUIRED    => [ LSF ],
    },

    DYNAMIC_FLCN_PRIV_LEVEL =>
    {
       DESCRIPTION         => "Dynamic Flcn privilege level",
       DEFAULT_STATE       => DISABLED,
    },

    DPUTIMER0_FOR_OS_TICKS =>
    {
       DESCRIPTION         => "Use Timer0 for OS scheduler",
       DEFAULT_STATE       => DISABLED,
    },

    # The feature maps to PDB - UCODE_FEATURE_RTTIMER_FOR_OS_TICKS in dpu.def
    # The chip list of the PDB needs to be syncronized with the configuration in dpu/config/dpu-config.cfg
    DPURTTIMER_FOR_OS_TICKS =>
    {
       DESCRIPTION         => "Use RTTimer for OS scheduler",
       DEFAULT_STATE       => DISABLED,
    },

    DPU_OS_CALLBACK_CENTRALISED =>
    {
       DESCRIPTION         => "Global switch used to enable centralized callbacks",
       DEFAULT_STATE       => DISABLED,
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
