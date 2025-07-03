#
# Copyright 2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
#
# Set of functions to parse and readin chip-config config file (.cfg) for
# for uproc scripts to use.
#

# TODO:
#   lwrrently the macros to parse chip-config config file is inside chip-config.pl
#   and the macro functions access global var (e.g. $Features, $Engines...) from
#   chip-config.pl directly.  That is not a clear design.  It's better to create
#   a module to parse .cfg file and return the result in a hash like what it does
#   in this file.  Once that is ready, this file can be a sub-class of that module.

package CfgReaderUproc;
use strict;
use warnings;

use Utils;                             # import rmconfig utility functions

# global var to cache the parsing result
my @Profiles;

sub DEFINE_PROFILE {
    my ($profile,@categories) = @_;

    push @Profiles, $profile;
}

# Those function can be removed from chip-config.pl once we confirm those are not in use
# sub DEFINE {}
# sub UNDEFINE {}
# sub UNDEF {}
# sub SET {}

# stubs
sub MINIMAL_MODE {}
sub DEFAULT_PROFILE {}
sub CATEGORY {}
sub ENABLE_CHIPS {}
sub DISABLE_CHIPS {}
sub ENABLE_FEATURES {}
sub DISABLE_FEATURES {}

sub ENABLE_ENGINES {}
sub DISABLE_ENGINES {}
sub ENABLE_CLASSES {}
sub DISABLE_CLASSES {}
sub ENABLE_APIS {}
sub DISABLE_APIS {}

# read and parse the chip-config config file
sub readCfgFile {
    my $file = shift;

    # read the Perl syntax .cfg file
    my $status = do $file;
    if ( ! defined($status)) {
        utilError "couldn't parse $file: $@"       if $@;
        utilError "couldn't parse (do) $file: $!"  unless defined $status;
        utilError "couldn't parse (run) $file"     unless $status;
        return undef;
    }

    # return the parsed result in a hash structure
    return {
        PROFILE_LIST => \@Profiles,
    };
}

1; #End of module
