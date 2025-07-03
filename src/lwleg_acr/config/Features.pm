#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#

# acr-config file that specifies all known acr-config features

package Features;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Features is derived from 'Groups'

use chipGroups;                 # imports CHIPS from Chips.pm and chip functions

@ISA = qw(Groups);

#
# This list contains all acr-config features.
#
my $featuresRef = [

    # platform the software will run on.
    # For now, just have unknown and ACR
    PLATFORM_UNKNOWN =>
    {
       DESCRIPTION         => "Running on an unknown platform",
       DEFAULT_STATE       => DISABLED,
    },
    PLATFORM_ACR =>
    {
       DESCRIPTION         => "Running on the ACR",
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
    ACRCORE_BASE =>
    {
       DESCRIPTION         => "ACRCORE Base",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [
                               "hal/lw/acr_halstub.c",
                               "init/lw/acr.c",
                               "wpr/t21x/acr_wprt210.c",
                               "falcon/maxwell/acr_falct210.c",
                               "init/lw/acrinit.c",
                               "acr_utils/acrutils.c",
                             ],
    },

    ACRCORE_GM20X =>
    {
       DESCRIPTION         => "ACRCORE for GM20X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "hal/maxwell/acr_halgm20x.c",
                              ],
    },

    ACRCORE_T21X =>
    {
       DESCRIPTION         => "ACRCORE for T21X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "hal/t21x/acr_halt21x.c",
                              ],
    },

    ACRCORE_T18X =>
    {
       DESCRIPTION         => "ACRCORE for T186",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "hal/t18x/acr_halt18x.c",
                              ],
    },

    ACRCORE_GP10X =>
    {
       DESCRIPTION         => "ACRCORE for GP10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "hal/pascal/acr_halgp10x.c",
                              ],
    },

    ACRCORE_T19X =>
    {
       DESCRIPTION         => "ACRCORE for T194",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "hal/t19x/acr_halt19x.c",
                              ],
    },

    ACRCORE_GV11X =>
    {
       DESCRIPTION         => "ACRCORE for GV11X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "hal/volta/acr_halgv11x.c",
                              ],
    },

    ACRCORE_T23XG =>
    {
       DESCRIPTION         => "ACRCORE for T234 iGPU",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "hal/t23x/acr_halt23x.c",
                               "wpr/t23x/acr_wprt234.c",
                              ],
    },

    ACR_LOAD =>
    {
       DESCRIPTION         => "Defined for acr load config profile build",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "sig_verif/t21x/acr_verift210.c",
                               "init/t21x/acrluct210.c",
                               "acrlib/maxwell/acruclibgm200.c",
			                   "dma/maxwell/acr_dmagm200.c",
                               "dma/volta/acr_dmagv11b.c",
                               "dma/t23x/acr_dmat234.c",
                               "security/volta/acr_secgv11b.c",
                               "security/t23x/acr_sect234.c",
			                   "security/t21x/acr_sect210.c",
			                   "security/pascal/acr_sect186.c",
			                   "security/t21x/acr_sspt210.c",
			                   "boot_falcon/t21x/acr_boott210.c",
                              ],
    },

    ACR_UNLOAD =>
    {
       DESCRIPTION         => "Defined for acr-gm20x_unload config profile build",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "acrlib/maxwell/acruclibgm200.c",
                               "dma/volta/acr_dmagv11b.c",
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
