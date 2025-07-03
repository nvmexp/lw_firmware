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
# sec2-config file that specifies all engines known to SEC2 applications
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
# This list contains all engines that sec2-config is aware of.
#
# IMPORTANT: New source files must be added to both Engines.pm and Sources.def.
#

# sec2-config does not lwrrently manage any engines yet, so this list has a temporary member below.
my $enginesRef = [

    IC =>
    {
        DESCRIPTION         => "Interrupt Controller",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/ic/hopper/sec2_icgh100.c",
                               ],
    },

    LSF =>
    {
        DESCRIPTION         => "Light Secure Falcon",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/lsf/lw/sec2_objlsf.c",
                                "syslib/lsf/hopper/lsfgh100.c",
                               ],
    },

    SEC2 =>
    {
        DESCRIPTION         => "SEC2 RISCV Engine ",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/sec2/hopper/sec2_sec2gh100.c",
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
