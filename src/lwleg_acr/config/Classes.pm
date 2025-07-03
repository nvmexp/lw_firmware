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
# acr-config file that specifies all known classes for ACR applications.
#

package Classes;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Classes is derived from 'Groups'

@ISA = qw(Groups);

# acr-config does not lwrrently manage any classes, so this list is empty.
my $classesRef = [
];

# Create the item group
sub new {

    @_ == 1 or croak 'usage: obj->new()';

    my $type = shift;

    my $self = Groups->new("class", $classesRef);

    $self->{GROUP_NAME_PLURAL}   = "classes";

    # CHIPS_SUPPORTED information will be derived from HALINFO when its read in.
    # Make sure the default Groups.pm behavior of:
    #
    #     CHIPS_SUPPORTED defaults to ALL_CHIPS
    #
    # doesn't get applied to the class list by creating an empty (but existent)
    # CHIPS_SUPPORTED entry for each class if it doesn't already exist.

    foreach my $iref (@{$self->grpItemRefsListRef()}) {
        if ( ! defined($iref->{CHIPS_SUPPORTED})) {
            $iref->{CHIPS_SUPPORTED} = [ ];
        }
    }

    return bless $self, $type;
}

# end of the module
1;
