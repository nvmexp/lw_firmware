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
# se-config file that specifies all engines known to SE applications
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
# This list contains all engines that se-config is aware of.
#
# IMPORTANT: New source files must be added to both Engines.pm and Sources.def.
#

# se-config does not lwrrently manage any engines yet, so this list has a temporary member below.
my $enginesRef = [

    SE =>
    {
        DESCRIPTION         => "SE",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "se/pascal/se_segp10x.c"        ,
                                "se/pascal/se_eccryptogp10x.c"  ,
                                "se/pascal/se_mutexgp10x.c"     ,
                                "se/pascal/se_trnggp10x.c"      ,
                                "se/pascal/sec_busgp10x.c"      ,
                                "se/turing/se_mutextu10x.c"     ,
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
