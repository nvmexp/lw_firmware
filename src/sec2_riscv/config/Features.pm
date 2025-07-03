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
# sec2-config file that specifies all known sec2-config features
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
# This list contains all sec2-config features.
#
my $featuresRef = [

    # platform the software will run on.
    # For now, just have unknown and SEC2
    PLATFORM_UNKNOWN =>
    {
       DESCRIPTION         => "Running on an unknown platform",
       DEFAULT_STATE       => DISABLED,
    },
    PLATFORM_SEC2 =>
    {
       DESCRIPTION         => "Running on the SEC2",
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
        PROFILES            => [ 'sec2-gh100', ],
        SRCFILES            => [ ],
    },

    # This feature is special.  It and the SEC2CORE_<<gpuFamily>> features will
    # be automatically enabled by rmconfig based on the enabled gpus.
    SEC2CORE_BASE =>
    {
       DESCRIPTION         => "SEC2CORE Base",
       SRCFILES            => [
                               'syslib/ic/lw/sec2_objic.c',
                              ],
    },

    SEC2CORE_GH10X =>
    {
       DESCRIPTION         => "SEC2CORE for GH10X",
       DEFAULT_STATE       => DISABLED,
    },

    DUAL_CORE =>
    {
       DESCRIPTION         => "Whether the build belongs a Dual core design",
       DEFAULT_STATE       => DISABLED,
    },

    SEC2TASK_CMDMGMT =>
    {
       DESCRIPTION         => "SEC2 Command Management Task (CORE)",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [ ],
    },

    SEC2TASK_CHNMGMT =>
    {
       DESCRIPTION         => "SEC2 Channel Management Task (CORE)",
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
