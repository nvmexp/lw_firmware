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


    # This feature is special.  It and the ACRCORE_<<gpuFamily>> features will
    # be automatically enabled by rmconfig based on the enabled gpus.
    ACRCORE_BASE =>
    {
       DESCRIPTION         => "ACRCORE Base",
       SRCFILES            => [
                               'acrshared/lw/acr_helper_functions.c',
                               'acrshared/hopper/acr_helper_functions_gh100.c',
                               'acrshared/hopper/acr_sanity_checks_gh100.c',
                               'lsauth/hopper/acr_verify_ls_signature_gh100.c',
                               'lsauth/hopper/acr_signature_manager_gh100.c',
                               'lsauth/hopper/acr_se_access_gh100.c',
                               'lsauth/hopper/acr_decrypt_ls_ucode_gh100.c',
                               'lsboot/hopper/acr_ls_riscv_boot_gh100.c',
                               'lsboot/hopper/acr_ls_falcon_boot_gh100.c',
                               'unload/lw/acr_command_unload.c',
                               'unload/hopper/acr_unload_gh100.c',
                               'init/lw/acr_partition_entry.c',
                               'wprsetup/lw/acr_command_wpr.c',
                               'lsboot/lw/acr_command_ls_boot.c',
                               'riscv/lw/acr_riscv_stubs.c',
                               'utils/hopper/acr_timer_access_gh100.c',
                               'utils/hopper/acr_dma_gh100.c',
                               'utils/hopper/acr_mem_utils_gh100.c',
                               'utils/hopper/acr_register_access_gh100.c',
                               'utils/hopper/acr_fuse_gh100.c',
                               'wprsetup/hopper/acr_sub_wpr_gh100.c',
                               'wprsetup/hopper/acr_lock_wpr_gh100.c',
                               'wprsetup/hopper/acr_wpr_ctrl_gh100.c',
                             ],
    },
    
    ACRCORE_GH10X =>
    {
       DESCRIPTION         => "ACRCORE for GH100",
       DEFAULT_STATE       => DISABLED,
    },

    ACRCORE_GH20X =>
    {
       DESCRIPTION         => "ACRCORE for GH20X",
       DEFAULT_STATE       => DISABLED,
    },

    GSPRM_BOOT =>
    {
       DESCRIPTION         => "ACR boot support for GSP-RM",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [ 
                               'gsprmboot/lw/acr_command_gsprm_boot.c',
                               'gsprmboot/hopper/acr_boot_gsprm_gh100.c',
                              ],
    },

    GSPRM_PROXY_BOOT =>
    {
       DESCRIPTION         => "ACR boot support for GSP-RM Proxy",
       DEFAULT_STATE       => ENABLED,
       SRCFILES            => [
                               'gsprmboot/lw/acr_command_gsprm_proxy_boot.c',
                               'gsprmboot/hopper/acr_boot_gsprm_proxy_gh100.c',
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
