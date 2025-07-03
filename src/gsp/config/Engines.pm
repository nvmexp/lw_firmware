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
# gsp-config file that specifies all engines known to GSP applications
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
# This list contains all engines that gsp-config is aware of.
#
# IMPORTANT: New source files must be added to both Engines.pm and Sources.def.
#

my $enginesRef = [

    LSF =>
    {
        DESCRIPTION         => "Light Secure Falcon Engine",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/lsftu10x.c",
                                "syslib/lsft23xd.c",
                               ],
    },

    MUTEX =>
    {
        DESCRIPTION         => "Mutex Engine",
        DEFAULT_STATE       => ENABLED,
        SRCFILES            => [
                                "syslib/mutextu10x.c",
                                "syslib/mutext23xd.c",
                               ],
    },

    DIO =>
    {
        DESCRIPTION         => "DIO Interface",
        DEFAULT_STATE       => DISABLED,
        SRCFILES            => [
                                "syslib/diot23xd.c",
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
