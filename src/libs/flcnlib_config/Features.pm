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
# flcnlib-config file that specifies all known flcnlib-config features
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
# This list contains all flcnlib-config features.
#
my $featuresRef = [

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

    # The ucode is platform independent
    PLATFORM_DEFAULT =>
    {
        DESCRIPTION         => "Running on any platform",
        DEFAULT_STATE       => ENABLED,
    },

    # The HW bug exists only on GM20X and will be fixed in future
    # chips. The SW WAR should be applied only on GM20X.
    HDCP_SW_WAR_BUG_1503447 =>
    {
        DESCRIPTION        => "SW WAR for GM20X HW bug 1503447 related to Private Bus access ",
        DEFAULT_STATE      => DISABLED,
    },

    HDCP22WIRED_OS_CALLBACK_CENTRALISED =>
    {
       DESCRIPTION         => "Global switch used to enable centralized callbacks",
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
