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
# riscvlib-config file that specifies all known riscvlib-config features
#

package Features;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Features is derived from 'Groups'

use chipGroups;                 # imports CHIPS from Chips.pm and chip functions

@ISA = qw(Groups);

my $driversSrc = "drivers/src/";
my $syslibSrc  = "syslib/src/";
my $shlibSrc   = "shlib/src/";
my $monitorSrc = "monitor/src/";
my $sepkernSrc = "sepkern/src/";

#
# This list contains all riscvlib-config features.
#
my $featuresRef = [

    RISCV_DRIVERS =>
    {
        DESCRIPTION         => "Shared code between user and kernel",
        DEFAULT_STATE       => DISABLED,
    },

    RISCV_MONITOR =>
    {
        DESCRIPTION         => "Simple HS monitor",
        DEFAULT_STATE       => DISABLED,
    },

    RISCV_SEPKERN =>
    {
        DESCRIPTION         => "Separation kernel",
        DEFAULT_STATE       => DISABLED,
    },

    RISCV_SYSLIB =>
    {
        DESCRIPTION         => "System interface for user-mode tasks",
        DEFAULT_STATE       => DISABLED,
    },

    RISCV_SHLIB =>
    {
        DESCRIPTION         => "Shared code between user and kernel",
        DEFAULT_STATE       => DISABLED,
    },

    # The ucode is platform independent
    PLATFORM_DEFAULT =>
    {
        DESCRIPTION         => "Running on any platform",
        DEFAULT_STATE       => ENABLED,
    },

    GSP_BUILD =>
    {
        DESCRIPTION         => "Running on GSP engine",
        DEFAULT_STATE       => DISABLED,
    },

    PMU_BUILD =>
    {
        DESCRIPTION         => "Running on PMU engine",
        DEFAULT_STATE       => DISABLED,
    },

    SEC2_BUILD =>
    {
        DESCRIPTION         => "Running on SEC2 engine",
        DEFAULT_STATE       => DISABLED,
    },

    SOE_BUILD =>
    {
        DESCRIPTION         => "Running on SOE engine",
        DEFAULT_STATE       => DISABLED,
    },

    ON_DEMAND_PAGING =>
    {
        DESCRIPTION         => "Add on-demand paging to drivers",
        DEFAULT_STATE       => DISABLED,
    },

    LWRISCV_SYMBOL_RESOLVER =>
    {
        DESCRIPTION         => "Add symbol resolving capability to drivers",
        DEFAULT_STATE       => DISABLED,
    },

    LWRISCV_CORE_DUMP =>
    {
        DESCRIPTION         => "Enable core dump",
        DEFAULT_STATE       => DISABLED,
    },

    LWRISCV_DEBUG_PRINT =>
    {
        DESCRIPTION         => "Enable print",
        DEFAULT_STATE       => DISABLED,
    },

    LWRISCV_MPU_DUMP_ENABLED =>
    {
        DESCRIPTION         => "Enable MPU state dump on crash",
        DEFAULT_STATE       => DISABLED,
    },

    SCHEDULER_ENABLED =>
    {
        DESCRIPTION         => "Enable QoS scheduler",
        DEFAULT_STATE       => DISABLED,
    },

    LWRISCV_PARTITION_SWITCH =>
    {
        DESCRIPTION         => "Enable partition switch capabilities",
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
