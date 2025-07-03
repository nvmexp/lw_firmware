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
# soe-config file that specifies all known soe-config features
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
# This list contains all soe-config features.
#
my $featuresRef = [

    # platform the software will run on.
    # For now, just have unknown and SOE
    PLATFORM_UNKNOWN =>
    {
       DESCRIPTION         => "Running on an unknown platform",
       DEFAULT_STATE       => DISABLED,
    },
    PLATFORM_SOE =>
    {
       DESCRIPTION         => "Running on the SOE",
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

    # This feature is special.  It and the SOECORE_<<gpuFamily>> features will
    # be automatically enabled by rmconfig based on the enabled gpus.
    SOECORE_BASE =>
    {
       DESCRIPTION         => "SOECORE Base",
       SRCFILES            => [
                               "hal/lw/soe_objhal.c" ,
                               "hal/lw/soe_halstub.c",
                               "ic/lw/soe_objic.c"   ,
                               "os/lw/soe_os.c"      ,
                               "main.c"               ,
                              ],
    },

    SOECORE_TU10X =>
    {
       DESCRIPTION         => "SOECORE for LR10",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "hal/limerock/soe_hallr10.c" ],
    },

    SOECORE_GH10X =>
    {
       DESCRIPTION         => "SOECORE for LS10",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ "hal/lagunaseca/soe_halls10.c" ],
    },

    SOETASK_CMDMGMT =>
    {
       DESCRIPTION         => "SOE Command Management Task (CORE)",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ "task_cmdmgmt.c" ],
    },

    SOETASK_THERM =>
    {
       DESCRIPTION         => "SOE Therm Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ "task_therm.c" ],
    },

    SOETASK_BIF =>
    {
       DESCRIPTION         => "SOE Bus Interface Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ "task_bif.c" ],
    },

    SOETASK_CORE =>
    {
       DESCRIPTION         => "SOE Task for core functionality",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ "task_core.c" ],
    },

    SOETASK_SPI =>
    {
       DESCRIPTION         => "SOE SPI Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ "task_spi.c" ],
    },

    SOETASK_IFR =>
    {
       DESCRIPTION         => "SOE Inforom Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ "task_ifr.c" ],
    },

    SOETASK_SMBPBI =>
    {
       DESCRIPTION         => "SOE SMBPBI Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ "task_smbpbi.c" ],
    },

    DYNAMIC_FLCN_PRIV_LEVEL =>
    {
       DESCRIPTION         => "Dynamic Flcn privilege level",
       DEFAULT_STATE       => ENABLED,
    },

    DBG_PRINTF_FALCDEBUG =>
    {
       DESCRIPTION         => "Redirect SOE Debug Spew to falc_debug",
       DEFAULT_STATE       => DISABLED,
    },

    BUG_200272812_HALT_INTR_MASK_WAR =>
    {
        DESCRIPTION        => "To track Halt interrupt mask WAR added for bug 200272812",
        DEFAULT_STATE      => DISABLED,
    },

    SOE_SMBPBI =>
    {
        DESCRIPTION         => "Enable/disable SMBus PostBox Interface",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "task_smbpbi.c",
                                "smbpbi/limerock/soe_smbpbilr10.c",
                                "smbpbi/limerock/soe_smbpbiecclr10.c",
                                "smbpbi/limerock/soe_smbpbilwlinklr10.c",
                                "smbpbi/lagunaseca/soe_smbpbilwlinkls10.c",
                               ],
    },

    SOE_PRIV_SEC_HALT_IF_DISABLED =>
    {
        DESCRIPTION         => "Halt SOE if PRIV_SEC is disabled on production boards",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [ "lsf/limerock/soe_lsflr10.c", ],
    },

    SOE_LWLINK_SLM =>
    {
        DESCRIPTION         => "Lwlink Single Lane Mode",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "lwlink/limerock/soe_lwlinklr10.c",
                                "therm/limerock/soe_thermlr10.c",
                               ],
    },

    SOE_MINION_SECCMD =>
    {
        DESCRIPTION         => "Minion Secure Command and Interrupt Interface",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "lwlink/lagunaseca/soe_lwlinkls10.c",
                                "therm/lagunaseca/soe_thermls10.c",
                               ],
    },        

    SOE_ONLY_LR10 =>
    {
        DESCRIPTION         => "Limerock specific Implementations",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "soe/limerock/soe_soelr10.c",
                                "soe/limerock/soe_dmalr10.c",
                                "lwlink/limerock/soe_lwlinklr10.c",
                                "therm/limerock/soe_thermlr10.c",
                                "gpio/limerock/soe_gpiolr10.c",
                                "smbpbi/limerock/soe_smbpbilwlinklr10.c",
                               ],
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
