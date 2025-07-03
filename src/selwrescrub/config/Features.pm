#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#

# selwrescrub-config file that specifies all known selwrescrub-config features

package Features;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Features is derived from 'Groups'

use chipGroups;                 # imports CHIPS from Chips.pm and chip functions

@ISA = qw(Groups);

#
# This list contains all selwrescrub-config features.
#
my $featuresRef = [

    # platform the software will run on.
    # For now, just have unknown and SELWRESCRUB
    PLATFORM_UNKNOWN =>
    {
       DESCRIPTION         => "Running on an unknown platform",
       DEFAULT_STATE       => DISABLED,
    },
    PLATFORM_SELWRESCRUB =>
    {
       DESCRIPTION         => "Running on the SELWRESCRUB",
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


    # This feature is special.  It and the ACRCORE_<<gpuFamily>> features will
    # be automatically enabled by rmconfig based on the enabled gpus.
    SELWRESCRUBCORE_BASE =>
    {
       DESCRIPTION         => "SELWRESCRUBCORE Base",
       SRCFILES            => [
                               "hal/lw/selwrescrub_objhal.c" ,
                               "hal/lw/selwrescrub_halstub.c",
                               "selwrescrub/lw/selwrescrub.c",
                               "selwrescrub/lw/selwrescrubinit.c",
                               "selwrescrub/lw/selwrescrub_objselwrescrub.c",
                               "selwrescrub/pascal/selwrescrubgp10x.c",
                               "selwrescrub/pascal/selwrescrubbsigp10x.c",
                             ],
    },

    SELWRESCRUBCORE_GP10X =>
    {
       DESCRIPTION         => "SELWRESCRUBCORE for GP10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "hal/pascal/selwrescrub_halgp10x.c",
                               "selwrescrub/pascal/selwrescrubdispgp10x.c",
                               "selwrescrub/pascal/selwrescrubgp10xonly.c",
                               "selwrescrub/pascal/selwrescrubgp10xgv10x.c",
                              ],
    },

    SELWRESCRUBCORE_GV10X =>
    {
       DESCRIPTION         => "SELWRESCRUBCORE for GV100",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "hal/volta/selwrescrub_halgv10x.c",
                               "selwrescrub/volta/selwrescrubdispgv10x.c",
                               "selwrescrub/volta/selwrescrubgv10x.c",
                               "selwrescrub/pascal/selwrescrubgp10xgv10x.c",
                               "selwrescrub/volta/selwrescrubgv10xonly.c",
                              ],
    },

    SELWRESCRUBCORE_TU10X =>
    {
       DESCRIPTION         => "SELWRESCRUBCORE for TU10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "hal/turing/selwrescrub_haltu10x.c",
                               "selwrescrub/volta/selwrescrubgv10x.c",
                               "selwrescrub/volta/selwrescrubdispgv10x.c",
                               "selwrescrub/turing/selwrescrubtu10x.c",
                               "selwrescrub/turing/selwrescrubtu102only.c",
                              ],
    },

    SELWRESCRUBCORE_TU116 =>
    {
       DESCRIPTION         => "SELWRESCRUBCORE for TU116",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "selwrescrub/volta/selwrescrubgv10x.c",
                               "selwrescrub/volta/selwrescrubdispgv10x.c",
                               "selwrescrub/turing/selwrescrubtu10x.c",
                               "selwrescrub/turing/selwrescrubtu116.c",
                              ],
    },

    SELWRESCRUBCORE_GA10X =>
    {
       DESCRIPTION         => "SELWRESCRUBCORE for GA10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "selwrescrub/volta/selwrescrubgv10x.c",
                               "selwrescrub/volta/selwrescrubdispgv10x.c",
                               "selwrescrub/turing/selwrescrubtu10x.c",
                               "selwrescrub/ampere/selwrescrubga10x.c",
                              ],
    },

    SELWRESCRUB_BOOT_FROM_HS =>
    {
       DESCRIPTION         => "Boot from HS build",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "../../bootloader/src/hs_entry_checks.c",
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
