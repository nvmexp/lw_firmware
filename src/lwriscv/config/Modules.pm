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
# riscvlib-config file that specifies all modules known to RISCVLIB applications
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
# This list contains all Modules that riscvlib-config is aware of.
#

# Each item in Modules.pm matches to one haldef (.hal) file.  New added modules
# must be enabled in sub-client's cfg file (e.g. syslib/config/syslib-config.cfg)
my $ModulesRef = [

    DRIVERS =>
    {
        DESCRIPTION         => "Hardware driver library",
        DEFAULT_STATE       => DISABLED,        # Enabled in drivers-config.cfg
    },

    MONITOR =>
    {
        DESCRIPTION         => "Simple HS monitor",
        DEFAULT_STATE       => DISABLED,        # Enabled in monitor-config.cfg
    },

    SEPKERN =>
    {
        DESCRIPTION         => "Separation kernel",
        DEFAULT_STATE       => DISABLED,        # Enabled in sepkern-config.cfg
    },

    SYSLIB =>
    {
        # This library is for interfacing between user-mode and kernel-mode.
        # Syscalls and such are located here.
        DESCRIPTION         => "System interface for user-mode tasks",
        DEFAULT_STATE       => DISABLED,        # Enabled in syslib-config.cfg
    },

    SHLIB =>
    {
        DESCRIPTION         => "Shared code between user and kernel",
        DEFAULT_STATE       => DISABLED,        # Enabled in shlib-config.cfg
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
