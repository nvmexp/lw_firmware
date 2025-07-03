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
# flcnlib-config file that specifies all modules known to FLCNLIB applications
#

package Modules;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here
use Carp;                       # for 'croak', 'cluck', etc.
use Groups;                     # Modules is derived from 'Groups'

@ISA = qw(Groups);

#
# The actual module definitions.
# This list contains all Modules that flcnlib-config is aware of.
#

# Each item in Modules.pm matches to one haldef (.hal) file.  New added modules
# must be enabled in sub-client's cfg file (e.g. i2c/config/i2c-config.cfg)
my $ModulesRef = [

    I2C =>
    {
        DESCRIPTION         => "I2C",
        DEFAULT_STATE       => DISABLED,        # Enabled in i2c-config.cfg
    },

    DPAUX =>
    {
        DESCRIPTION         => "DPAUX",
        DEFAULT_STATE       => DISABLED,        # Enabled in dpaux-config.cfg
    },

    HDCP =>
    {
        DESCRIPTION         => "HDCP",
        DEFAULT_STATE       => DISABLED,        # Enabled in hdcp-config.cfg
    },

    HDCP22WIRED =>
    {
        DESCRIPTION         => "HDCP22WIRED",
        DEFAULT_STATE       => DISABLED,        # Enabled in hdcp22wired-config.cfg
    },

    SHAHW =>
    {
        DESCRIPTION         => "SHAHW",
        DEFAULT_STATE       => DISABLED,        # Enabled in shahw-config.cfg
    },

];

# Create the item group
sub new {

    @_ == 1 or croak 'usage: obj->new()';

    my $type = shift;

    my $self = Groups->new("module", $ModulesRef);

    return bless $self, $type;
}

# end of the module
1;
