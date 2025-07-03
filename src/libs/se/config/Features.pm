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
# se-config file that specifies all known se-config features
#

package Features;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Features is derived from 'Groups'

use chipGroups;                 # imports CHIPS from Chips.pm and chip functions

@ISA = qw(Groups);

#
# This list contains all se-config features.
#
my $featuresRef = [

    # platform the software will run on.
    # For now, just have SE and unknown
    PLATFORM_UNKNOWN =>
    {
       DESCRIPTION         => "Running on an unknown platform",
       DEFAULT_STATE       => DISABLED,
    },

    PLATFORM_SE =>
    {
       DESCRIPTION         => "Running on the SE",
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
