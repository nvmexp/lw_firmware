#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#

# File Name:   UtilsUproc.pm
#
#   Utility functions shared  by flcn-rtos-script.pl sub-modules.  This
#   can be seen as an extansion of chip-config::Utils.pm.  The functions
#   cannot be moved to chip-config domain since it requires $UPROC.
#
package UtilsUproc;

use strict;

use Carp;                              # for 'croak', 'cluck', etc.
use Utils;                             # import rmconfig utility functions
use Rmconfig::CfgHash;
use chipGroups;                        # imports CHIPS from Chips.pm and chip functions

# HACK to get access to $UPROC from major script (e.g. rtos-flcn-script.pl)
our $UPROC;
*UPROC  = \$main::UPROC;

# parse the parameter block and return the value of matched entry
#
#   [
#       value1 => DEFAULT,
#       value2 => [ PASCAL_and_later, CHEETAH, ],       # chip family list   or
#       value2 => [ pmu-gp100..., -pmu-*_riscv, ],    # profile name pattern list
#   ]
#
sub parseHalParamBlock {
    (@_ == 2) or croak 'usage: utilParseHalParamBlock( TAG, PARAM-BLOCK )';
    $UPROC    or croak 'utilParseHalParamBlock() is called when $main::UPROC is unavailable';

    my ($tag, $block) = @_;

    # if it is not a hal block, then return the value directly
    return $block   unless (ref($block) eq 'ARRAY');

    my $tmpHashRef  = cfgHashRef($block, $tag);
    my $ProfileUtil = $UPROC->{PROFILE_UTIL};

    my $match;
    my $default;

    # OPTION => [ CHIP_FAMILIES, ]      or
    # OPTION => [ profile-patterns, ]   or
    # OPTION => DEFAULT
    foreach my $option (keys %{$tmpHashRef}) {
        my $conditions  = $tmpHashRef->{$option};

        if ($conditions =~ m/^DEFAULT$/i) {
            if (defined $default) {
                utilError "$tag: More than one DEFAULT value in parameter block";
                return undef;
            }
            $default = $option;
            next;
        }

        if (ref $conditions ne 'ARRAY') {
            utilError "$tag: Condition should be either 'DEFAULT' or a list of Profile patterns or Chips";
            return undef;
        }

        # Both profile list and chip family list are ref of array
        # The profile pattern is generally like 'pmu-tu10x' 'pmu-*' 'pmu-gm20x...'
        # Test those special characters to find out profile lists
        my $isProfiles = grep { $_ =~ m/[\*\.]/ or $_ =~ m/\w+-/ } @$conditions;
        my $bMatched;
        if ($isProfiles) {
            my $profilesRef = $conditions;
            $bMatched = $ProfileUtil->isTargetProfileInPatternList($profilesRef);

        } else {
            my $chipListRef  = $conditions;
            @$chipListRef = chipEnabledListFromFamilyList(@$chipListRef);
            $bMatched = scalar @$chipListRef
        }

        if ($bMatched) {
            # error if overlappying chips list
            if (defined $match) {
                utilError "$tag: More than one match in parameter block: value '$match' and value '$option'";
                return undef;
            }
            $match = $option;
        }
    }

    if (! (defined $match || defined $default)) {
        utilError "$tag: Missing default value and there is no match for this case";
        return undef;
    }

    if (defined $match) {
        return $match;
    } else {
        return $default;
    }
}
