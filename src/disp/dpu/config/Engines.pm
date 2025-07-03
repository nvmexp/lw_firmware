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
# dpu-config file that specifies all engines known to DPU applications
#

package Engines;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Engines is derived from 'Groups'

@ISA = qw(Groups);

#
# The actual engine definitions.
# This list contains all engines that dpu-config is aware of.
#
my $enginesRef = [

    DPU =>
    {
        DESCRIPTION         => "DPU",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "dpu/lw/dpu_objdpu.c",
                                "dpu/v02/dpu_dpu0200.c",
                                "dpu/v02/dpu_queue0200.c",
                                "dpu/v02/dpu_dpu0201.c",
                                "dpu/v02/dpu_dpu0205.c",
                                "dpu/v02/dpu_dpu0207.c",
                                "dpu/v02/dpu_mutex0205.c",
                                "dpu/v02/dpu_mutex0207.c",
                                "dpu/volta/dispflcngv10x.c",
                                "dpu/volta/dispflcn_prierrgv10x.c",
                                "dpu/volta/dispflcn_mutexgv10x.c",
                                "dpu/turing/dispflcntu10x.c",
                                "dpu/turing/dispflcntu116.c",
                                "dpu/ampere/dispflcnga10x.c",
                                "dpu/ada/dispflcnad10x.c",                            
                               ],
    },

    IC =>
    {
        DESCRIPTION         => "Interrupt Controller",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "ic/v02/dpu_ic0200.c",
                                "ic/v02/dpu_ic0204.c",
                                "ic/v02/dpu_ic0205.c",
                                "ic/v02/dpu_ic0207.c",
                                "ic/volta/dispflcn_icgv10x.c",
                               ],
    },

    LSF =>
    {
        DESCRIPTION         => "Light Secure Falcon Engine",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "lsf/lw/dpu_objlsf.c",
                                "lsf/v02/dpu_lsf0205.c",
                                "lsf/volta/dpu_lsfgv10x.c",
                                "lsf/ampere/dpu_lsfga10x.c",
                               ],
    },

    ISOHUB =>
    {
        DESCRIPTION         => "ISOHUB Engine",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "isohub/lw/dpu_objisohub.c",
                                "isohub/v02/dpu_isohub0205.c",
                               ],
    },

    VRR =>
    {
        DESCRIPTION         => "VRR Engine",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "vrr/lw/dpu_objvrr.c",
                                "vrr/v02/dpu_vrr0207.c",
                               ],
    },

];

# Create the item group
sub new {

    @_ == 1 or croak 'usage: obj->new()';

    my $type = shift;

    my $self = Groups->new("engine", $enginesRef);

    return bless $self, $type;
}

# end of the module
1;
