#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#

# fub-config file that specifies all known fub-config features

package Features;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Features is derived from 'Groups'

use chipGroups;                 # imports CHIPS from Chips.pm and chip functions

@ISA = qw(Groups);

#
# This list contains all fub-config features.
#
my $featuresRef = [

    # platform the software will run on.
    # For now, just have unknown and FUB
    PLATFORM_UNKNOWN =>
    {
       DESCRIPTION         => "Running on an unknown platform",
       DEFAULT_STATE       => DISABLED,
    },
    PLATFORM_FUB =>
    {
       DESCRIPTION         => "Running on the FUB",
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

    # This feature is special.  It and the FUBCORE_<<gpuFamily>> features will
    # be automatically enabled by rmconfig based on the enabled gpus.
    FUBCORE_BASE =>
    {
       DESCRIPTION         => "FUBCORE Base",
       SRCFILES            => [
                               "fub/lw/fub.c",
                             ],
    },

    FUBCORE_TU10X =>
    {
       DESCRIPTION         => "FUBCORE for TU10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "fub/turing/fubtu10x.c",   
                               "fub/turing/fubtu10xonly.c",
                               ],
    },

    FUBCORE_TU116 =>
    {
       DESCRIPTION         => "FUBCORE for TU116",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "fub/turing/fubtu10x.c",
                               "fub/turing/fubtu116only.c",
                               ],
    },

    FUBCORE_GA10X =>
    {
       DESCRIPTION         => "FUBCORE for GA100",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                              ],
    },

    FUB_BURN_GSYNC_ENABLING_FUSE =>
    {
       DESCRIPTION         => "Burn Fuse Enabling GSYNC",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    FUB_BURN_DEVID_LICENSE_REVOCATION_FUSE =>
    {
       DESCRIPTION         => "Burn Fuse to revoke DEVID based License",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    FUB_BURN_ALLOW_DRAM_CHIPID_READ_HULK_LICENSE =>
    {
       DESCRIPTION         => "Burn Fuse Allowing HULK license to read DRAM CHIPID ",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    FUB_BURN_REVOKE_DRAM_CHIPID_READ_HULK_LICENSE =>
    {
       DESCRIPTION         => "Burn Fuse to revoke HULK license to read DRAM CHIPID ",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    FUB_BURN_GEFORCE_SKU_RECOVERY =>
    {
       DESCRIPTION         => "Burn SPARE_FUSES_3[4:4] for Turing (TU104, TU106) VdChip SKU Recovery",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    FUB_BURN_GEFORCE_SKU_RECOVERY_GFW_REV =>
    {
       DESCRIPTION         => "Burn GFW_REV[2:2] for Turing (TU104, TU106) VdChip SKU Recovery",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    FUB_BURN_SELF_REVOCATION_FUSE =>
    {
       DESCRIPTION         => "Burn UCODE1 fuse to restrict FUB run once per chip",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    FUB_BURN_LWFLASH_REV_FOR_WP_BYPASS  =>
    {
       DESCRIPTION         => "Burn UCODE_ROM_FLASH_REV fuse for write protection bypass mitigation",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },

    FUB_BURN_GFW_REV_FOR_WP_BYPASS =>
    {
       DESCRIPTION         => "Burn GFW_REV fuse for write protection bypass mitigation", 
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ ],
    },
   
    FUB_BURN_AUTO_QS2_AHESASC_FUSE =>
    {
       DESCRIPTION         => "Burn fuse so that Ahesasc QS1 bin can not run on QS2", 
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "fub/turing/fub_auto_qs2_ahesasc.c", 
                              ],
    },
   
    FUB_BURN_AUTO_QS2_ASB_FUSE =>
    {
       DESCRIPTION         => "Burn fuse so that ASB QS1 bin can not run on QS2", 
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "fub/turing/fub_auto_qs2_asb.c", 
                              ],
    },
    
    FUB_BURN_AUTO_QS2_LWFLASH_FUSE =>
    {
       DESCRIPTION         => "Burn fuse so that LWFLASH QS1 bin can not run on QS2", 
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "fub/turing/fub_auto_qs2_lwflash.c", 
                              ],
    },

    FUB_BURN_AUTO_QS2_IMAGESELECT_FUSE =>
    {
       DESCRIPTION         => "Burn fuse so that IMAGESELECT QS1 bin can not run on QS2", 
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "fub/turing/fub_auto_qs2_imageselect.c", 
                              ],
    },

    FUB_BURN_AUTO_QS2_HULK_FUSE =>
    {
       DESCRIPTION         => "Burn fuse so that HULK QS1 bin can not run on QS2", 
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "fub/turing/fub_auto_qs2_hulk.c", 
                              ],
    },

    FUB_BURN_AUTO_QS2_FWSEC_FUSE =>
    {
       DESCRIPTION         => "Burn fuse so that FwSec QS1 bin can not run on QS2", 
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               "fub/turing/fub_auto_qs2_fwsec.c", 
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
