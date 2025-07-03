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
   ARCH_RISCV =>
    {
        DESCRIPTION         => "RISCV architecture",
        PROFILES            => [ 'soe-ls10', ],
        SRCFILES            => [ ],
    },

    # This feature is special.  It and the SOECORE_<<gpuFamily>> features will
    # be automatically enabled by rmconfig based on the enabled gpus.
    SOECORE_BASE =>
    {
       DESCRIPTION         => "SOECORE Base",
       SRCFILES            => [
                               'syslib/ic/lw/soe_objic.c',
                              ],
    },

    SOECORE_GH10X =>
    {
       DESCRIPTION         => "SOECORE for GH10X",
       DEFAULT_STATE       => DISABLED,
    },

    DUAL_CORE =>
    {
       DESCRIPTION         => "Whether the build belongs a Dual core design",
       DEFAULT_STATE       => DISABLED,
    },

    SOETASK_CMDMGMT =>
    {
       DESCRIPTION         => "SOE Command Management Task (CORE)",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ ],
    },

    SOETASK_CHNMGMT =>
    {
       DESCRIPTION         => "SOE Channel Management Task (CORE)",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ ],
    },

    SOETASK_SMBPBI =>
    {
       DESCRIPTION         => "SOE Smbpbi Management Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ ],
    },

    SOETASK_THERM =>
    {
       DESCRIPTION         => "SOE Thermal Management Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ ],
    },

    SOETASK_CORE =>
    {
       DESCRIPTION         => "SOE Core Management Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ ],
    },

    SOETASK_SPI =>
    {
       DESCRIPTION         => "SOE SPI Management Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ ],
    },

    SOETASK_BIF =>
    {
       DESCRIPTION         => "SOE BIF Management Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ ],
    },

    SOETASK_LWLINK =>
    {
       DESCRIPTION         => "SOE Lwlink Management Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ ],
    },

    SOETASK_INTR =>
    {
       DESCRIPTION         => "SOE Interrupts Management Task",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ ],
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
