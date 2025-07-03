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
       SRCFILES            => [
                               'ahesasc/turing/acr_sub_wpr_tu10x.c',
                               'bsilock/lw/acr_bsi_lock.c',
                               'acrshared/lw/acr_helper_functions.c',
                               'acrsec2shared/turing/acr_timer_access_tu10x.c',
                               'acrshared/turing/acr_se_access_tu10x.c',
                               'acrshared/turing/acr_helper_functions_tu10x.c',
                               'acrshared/turing/acr_helper_functions_tu10x_ga10x_only.c',
                               'acrshared/turing/acr_sanity_checks_tu10x.c',
                               'acrshared/turing/acr_trng_tu10x.c',
                               'acrshared/turing/acr_war_functions_tu10x.c',
                               'acrshared/ampere/acr_sanity_checks_ga100.c',
                               'acrshared/ampere/acr_war_functions_ga100_only.c',
                               'acrshared/ampere/acr_sanity_checks_ga10x.c',
                               'acrshared/ampere/acr_sanity_checks_ga100_and_later.c',
                               'acrshared/ampere/acr_war_functions_ga10x.c',
                               'acrshared/ampere/acr_war_functions_ga10x_only.c',
                               'acrshared/ampere/acr_helper_functions_ga10x.c',
                               'ahesasc/ampere/acr_sub_wpr_ga10x.c',
                               'ahesasc/ada/acr_sub_wpr_ad10x.c',
                               'ahesasc/hopper/acr_sub_wpr_gh100.c',
                               'acrsec2shared/turing/acr_register_access_tu10x.c',
                               'acrsec2shared/turing/acr_dma_tu10x.c',
                               'acrsec2shared/turing/acr_mem_utils_tu10x.c',
                               'acrsec2shared/ampere/acr_dma_ga100.c',
                               'acrsec2shared/ampere/acr_register_access_ga100.c',
                               'acrsec2shared/ampere/acr_register_access_ga10x.c',
                               'acrsec2shared/ampere/acr_register_access_ga10x_only.c',
                               'acrshared/ampere/acr_war_functions_ga100_and_later.c',
                               'acrshared/ada/acr_sanity_checks_ad10x.c',
                               'acrsec2shared/ada/acr_register_access_ad10x.c',
                               'acrshared/hopper/acr_sanity_checks_gh100.c',
                               'acrshared/hopper/acr_sanity_checks_gh20x.c',
                               'acrshared/hopper/acr_helper_functions_gh100.c',
                               'acrsec2shared/hopper/acr_register_access_gh100.c',
                               'acrsec2shared/hopper/acr_fuse_gh100.c',
                               'acrshared/g00x/acr_sanity_checks_g000.c',
                            ],
    },

    ACRCORE_TU10X =>
    {
       DESCRIPTION         => "ACRCORE for TU10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'acrshared/turing/acr_war_functions_tu1xx_only.c',
                               'acrsec2shared/turing/acr_register_access_tu10x_ga10x_only.c',
                              ],
    },

    ACRCORE_TU116 =>
    {
       DESCRIPTION         => "ACRCORE for TU116",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'acrshared/turing/acr_sanity_checks_tu116_only.c',
                               'acrsec2shared/turing/acr_register_access_tu10x_ga10x_only.c',
                              ],
    },

    ACRCORE_GA10X =>
    {
       DESCRIPTION         => "ACRCORE for GA10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'acrsec2shared/turing/acr_register_access_tu10x_ga10x_only.c',
                              ],
    },

     ACRCORE_AD10X =>
    {
       DESCRIPTION         => "ACRCORE for AD10X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'acrsec2shared/turing/acr_register_access_tu10x_ga10x_only.c',
                              ],
    },

    ACRCORE_GH10X =>
    {
       DESCRIPTION         => "ACRCORE for GH100",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'acrsec2shared/hopper/acr_register_access_gh100_only.c',
                              ],
    },

    ACRCORE_GH20X =>
    {
       DESCRIPTION         => "ACRCORE for GH20X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'acrsec2shared/hopper/acr_register_access_gh202.c',
                              ],
    },

    ACRCORE_G00X =>
    {
       DESCRIPTION         => "ACRCORE for G00X",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'acrsec2shared/hopper/acr_register_access_gh100_only.c',
                              ],
    },

    ACR_AHESASC =>
    {
       DESCRIPTION         => "Defined for acr load config profile build",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'ahesasc/turing/acr_verify_ls_signature_tu10x.c',
                               'ahesasc/turing/acr_hub_encryption_tu10x.c',
                               'ahesasc/hopper/acr_hub_encryption_gh100.c',
                               'ahesasc/turing/acr_lock_wpr_tu10x.c',
                               'ahesasc/ampere/acr_verify_ls_signature_ga10x.c',
                               'ahesasc/ampere/acr_signature_manager_ga10x.c',
                               'se/ampere/acrucsega10x.c',
                               'sha/ampere/acrucshaga100.c',
                               'ahesasc/ampere/acr_decrypt_ls_ucode_ga10x.c',
                              ],
    },

    NEW_WPR_BLOBS =>
    {
        DESCRIPTION         => "Defined for profile build which need to access new wpr blobs",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                'acrshared/ampere/acr_wpr_ctrl_ga10x.c',
                               ],
    },

    ACR_ASB =>
    {
       DESCRIPTION         => "Defined for acr load config profile build",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'asb/turing/acr_ls_falcon_boot_tu10x.c',
                               'asb/turing/acr_ls_falcon_boot_tu10xga100.c',
                               'asb/ampere/acr_ls_falcon_boot_ga100.c',
                               'asb/ampere/acr_ls_falcon_boot_ga10x.c',
                               'asb/hopper/acr_ls_falcon_boot_gh100.c',
                              ],
    },

    ACR_UNLOAD =>
    {
       DESCRIPTION         => "Defined for acr-gm20x_unload config profile build",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'asb/turing/acr_ls_falcon_boot_tu10x.c',
                               'asb/ampere/acr_ls_falcon_boot_ga10x.c',
                               'asb/hopper/acr_ls_falcon_boot_gh100.c',
                               'unload/lw/acr_unload.c',
                               'unload/turing/acr_unload_tu10x.c',
                               'unload/ampere/acr_unload_ga10x.c',
                               'unload/hopper/acr_unload_gh100.c',
                              ],
    },

    ACR_BSI_LOCK =>
    {
       DESCRIPTION         => "Defined for acr-gm20x_bsi_lockdown config profile build",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'ahesasc/turing/acr_hub_encryption_tu10x.c',
                               'ahesasc/turing/acr_lock_wpr_tu10x.c',
                               'asb/turing/acr_ls_falcon_boot_tu10x.c',
                               'bsilock/turing/acr_bsi_lock_tu10x.c',
                              ],
    },

    ACR_USING_SEC2_MUTEX =>
    {
       DESCRIPTION         => "Defined for GP10X ACR profiles using sec2 mutex for acquiring resource lock",
       DEFAULT_STATE       => DISABLED,
    },

    ACR_BOOT_FROM_HS =>
    {
       DESCRIPTION         => "Boot from HS build",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               '../../bootloader/src/hs_entry_checks.c',
                              ],
    },

    ACR_APM =>
    {
       DESCRIPTION         => "Enable Ampere Protected Memory feature (GA100+)",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                               'apm/lw/acr_apmmsr.c',
                               'apm/ampere/acr_measurements_ga100.c',
                               'apm/ampere/acr_apm_checks_ga100.c',
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
