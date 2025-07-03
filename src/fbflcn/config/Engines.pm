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
# This list contains all engines that fbflcn-config is aware of.
#
my $enginesRef = [

    FBFALCON =>
    {
        DESCRIPTION         => "FBFALCON",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
	                            "fbfalcon/volta/fbfalcon_gv100.c",
	                            "fbfalcon/volta/table_gv100.c",
	                            "fbfalcon/volta/interrupts_gv100.c",
	                            "fbfalcon/turing/fbfalcon_tu10x.c",
	                            "fbfalcon/turing/table_tu10x.c",
	                            "fbfalcon/turing/revocation_tu102.c",
	                            "fbfalcon/ampere/fbfalcon_ga10x.c",
	                            "fbfalcon/ampere/interrupts_ga100.c",
	                            "fbfalcon/ampere/table_ga10x.c",
	                            "fbfalcon/ampere/revocation_ga102.c",
	                            "fbfalcon/ampere/temperature_ga102.c",
	                            "fbfalcon/hopper/fbfalcon_gh100.c",
	                            "fbfalcon/hopper/table_gh100.c",
	                            "fbfalcon/hopper/revocation_gh100.c",
                               ],
    },

    MEMORY  =>
    {
        DESCRIPTION         => "MEMORY engine, encapsulates all dramc/fbio and dram functionality",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
	                            "memory/volta/memory_gv100.c",
	                            "memory/turing/memory_tu10x.c",
	                            "memory/turing/memory_tu10x_and_earlier.c",
	                            "memory/ampere/memory_ga100.c",
	                            "memory/ampere/memory_ga10x.c",
	                            "memory/hopper/memory_gh100.c",
	                            "memory/hopper/memory_hbm_gh100.c",
	                            "memory/hopper/ieee1500_gh100.c",
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
