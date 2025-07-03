#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#

# booter-config file that specifies all known booter-config features

package Features;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Features is derived from 'Groups'

use chipGroups;                 # imports CHIPS from Chips.pm and chip functions

@ISA = qw(Groups);

#
# This list contains all booter-config features.
#
my $featuresRef = [

    # Platform the software will run on.
    # For now, just have unknown and Booter
    PLATFORM_UNKNOWN =>
    {
       DESCRIPTION         => "Running on an unknown platform",
       DEFAULT_STATE       => DISABLED,
    },
    PLATFORM_BOOTER =>
    {
       DESCRIPTION         => "Running on the Booter",
       DEFAULT_STATE       => DISABLED,
    },

    # Target architecture
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

    # Enabled by default
    BOOTER_COMMON =>
    {
       DESCRIPTION         => "Booter base common code",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [
                                  'common/lw/booter_start.c',
                                  'common/turing/booter_register_access_tu10x.c',
                                  'common/turing/booter_dma_tu10x.c',
                                  'common/turing/booter_timer_access_tu10x.c',
                                  'common/turing/booter_trng_tu10x.c',
                                  'common/turing/booter_helper_functions_tu10x.c',
                                  'common/turing/booter_sanity_checks_tu10x.c',
                                  'common/turing/booter_se_access_tu10x.c',
                              ],
    },

    BOOTER_COMMON_TU10X =>
    {
       DESCRIPTION         => "Booter base code for TU10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'common/turing/booter_sanity_checks_tu10x_only.c',
                                  'common/turing/booter_sanity_checks_tu10x_tu11x_only.c',
                              ],
    },

    BOOTER_COMMON_TU11X =>
    {
       DESCRIPTION         => "Booter base code for TU11X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'common/turing/booter_sanity_checks_tu11x.c',
                                  'common/turing/booter_sanity_checks_tu10x_tu11x_only.c',
                              ],
    },

    BOOTER_COMMON_GA100 =>
    {
       DESCRIPTION         => "Booter base code for GA100",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'common/ampere/booter_sanity_checks_ga100.c',
                                  'common/ampere/booter_sanity_checks_ga100_and_later.c',
                              ],
    },

    BOOTER_COMMON_GA10X =>
    {
       DESCRIPTION         => "Booter base code for GA10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'common/ampere/booter_sanity_checks_ga100_and_later.c',
                                  'common/ampere/booter_sanity_checks_ga10x.c',
                                  'common/ampere/booter_war_functions_ga10x_only.c',
                              ],
    },

    BOOTER_LOAD_COMMON =>
    {
       DESCRIPTION         => "Booter load common code",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'load/turing/booter_load_tu10x.c',
                                  'load/turing/booter_sub_wpr_tu10x.c',
                                  'load/turing/booter_wpr1_tu10x.c',
                                  'load/turing/booter_wpr2_tu10x.c',
                                  'se/turing/booter_se_tu10x.c',
                                  'sha/turing/booter_sha_tu10x.c',
                                  'load/turing/booter_sig_verif_tu10x.c',
                                  'common/turing/booter_mem_utils_tu10x.c',
                                  'load/turing/booter_wpr_ctrl_tu10x.c',
                                  'load/turing/booter_boot_gsprm_tu10x.c',
                              ],
    },

    BOOTER_LOAD_TU10X =>
    {
       DESCRIPTION         => "Booter load code for TU10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'load/turing/booter_bootvec_tu10x.c',
                                  'load/turing/booter_boot_gsprm_tu10x_ga100.c',
                              ],
    },

    BOOTER_LOAD_TU11X =>
    {
       DESCRIPTION         => "Booter load code for TU11X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'load/turing/booter_bootvec_tu10x.c',
                                  'load/turing/booter_boot_gsprm_tu10x_ga100.c',
                              ],
    },

    BOOTER_LOAD_GA100 =>
    {
       DESCRIPTION         => "Booter load code for GA100",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'load/turing/booter_bootvec_tu10x.c',
                                  'load/turing/booter_boot_gsprm_tu10x_ga100.c',
                              ],
    },

    BOOTER_LOAD_GA10X =>
    {
       DESCRIPTION         => "Booter load code for GA10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'load/ampere/booter_bootvec_ga10x.c',
                                  'load/ampere/booter_boot_gsprm_ga10x.c',
                                  'load/ampere/booter_sig_verif_ga10x.c',
                              ],
    },

    BOOTER_RELOAD_COMMON =>
    {
       DESCRIPTION         => "Booter reload common code",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'reload/turing/booter_reload_tu10x.c',
                                  'load/turing/booter_boot_gsprm_tu10x.c',
                              ],
    },

    BOOTER_RELOAD_TU10X =>
    {
       DESCRIPTION         => "Booter reload code for TU10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'load/turing/booter_bootvec_tu10x.c',
                                  'load/turing/booter_boot_gsprm_tu10x_ga100.c',
                              ],
    },

    BOOTER_RELOAD_TU11X =>
    {
       DESCRIPTION         => "Booter reload code for TU11X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'load/turing/booter_bootvec_tu10x.c',
                                  'load/turing/booter_boot_gsprm_tu10x_ga100.c',
                              ],
    },

    BOOTER_RELOAD_GA100 =>
    {
       DESCRIPTION         => "Booter reload code for GA100",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'load/turing/booter_bootvec_tu10x.c',
                                  'load/turing/booter_boot_gsprm_tu10x_ga100.c',
                              ],
    },

    BOOTER_RELOAD_GA10X =>
    {
       DESCRIPTION         => "Booter reload code for GA10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'load/ampere/booter_bootvec_ga10x.c',
                                  'load/ampere/booter_boot_gsprm_ga10x.c',
                              ],
    },

    BOOTER_UNLOAD_COMMON =>
    {
       DESCRIPTION         => "Booter unload common code",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                  'load/turing/booter_sub_wpr_tu10x.c',
                                  'load/turing/booter_wpr1_tu10x.c',
                                  'unload/turing/booter_unload_tu10x.c',
                              ],
    },

    BOOTER_UNLOAD_TU10X =>
    {
       DESCRIPTION         => "Booter unload code for TU10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                              ],
    },

    BOOTER_UNLOAD_TU11X =>
    {
       DESCRIPTION         => "Booter unload code for TU11X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                              ],
    },

    BOOTER_UNLOAD_GA100 =>
    {
       DESCRIPTION         => "Booter unload code for GA100",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                              ],
    },

    BOOTER_UNLOAD_GA10X =>
    {
       DESCRIPTION         => "Booter unload code for GA10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                              ],
    },

    BOOTER_BOOT_FROM_HS =>
    {
       DESCRIPTION         => "Booter - Boot from HS build",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               '../../bootloader/src/hs_entry_checks.c',
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
