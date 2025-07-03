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
# gsp-config file that specifies all known gsp-config features
#

package Features;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Features is derived from 'Groups'

@ISA = qw(Groups);

#
# This list contains all gsp-config features.
#
my $featuresRef = [

    # The ucode is platform independent
    PLATFORM_DEFAULT =>
    {
       DESCRIPTION         => "Running on any platform",
       DEFAULT_STATE       => ENABLED,
    },

    GSPTASK_DISPLAY =>
    {
        DESCRIPTION        => "Display related functionality",
        DEFAULT_STATE      => ENABLED,
    },

    GSPTASK_SCHEDULER =>
    {
        DESCRIPTION        => "HW Scheduler functionality",
        DEFAULT_STATE      => DISABLED,
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
