#!/usr/bin/perl -w

#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# Example usage :
#   > perl Profiles.pm.old Profiles.pm.new
#

use strict;
use warnings 'all';

use Carp;
use File::Basename;
use File::Spec;

BEGIN {
    # HACK: add current directory to @INC path (for ScriptUpdate.pm)
    my $buildDir = dirname __FILE__;
    unshift @INC, File::Spec->catfile($buildDir);
}

use util::ScriptUpdate;

my ($inFn, $outFn) = @ARGV;

croak 'Invalid arguments.' if ((! defined $inFn) or (! defined $outFn));

open(my $inFh, '<:encoding(UTF-8)', $inFn) or croak "Couldn't open $inFn for read";
open(my $outFh, '>:encoding(UTF-8)', $outFn) or croak "Couldn't open $outFn for read";

my %fieldAdjustments = %ScriptUpdate::profilesAdjustments;

sub atoi {
    my ($str) = @_;

    if (begins_with($str, '0'))
    {
        return oct($str);
    }
    else
    {
        return int($str);
    }
}

sub begins_with {
    return substr($_[0], 0, length($_[1])) eq $_[1];
}

sub valueAdjust {
    my ($section, $value) = @_;

    if (defined $fieldAdjustments{$section}->{ADJUST})
    {
        return $fieldAdjustments{$section}->{ADJUST}->($value);
    }
    else
    {
        return $value;
    }
}

sub commentSet {
    my ($section, $value, $comment) = @_;

    if (defined $fieldAdjustments{$section}->{COMMENT})
    {
        return '# '.$fieldAdjustments{$section}->{COMMENT}->($value);
    }
    else
    {
        return $comment;
    }
}

my $profileParseStep = 0;
my $indentOne = '    ';
my $indentTwo = $indentOne . $indentOne;

my $workingCommon = '';
my $workingFalcon = '';

while (my $row = <$inFh>) {
    my $newRow = $row;
    # Remove comments inside profiles, otherwise keep them.
    if ($row =~ /^\s*#/) {
        goto LINE_DONE if ($profileParseStep == 0);
        next;
    }

    if ($profileParseStep == 2) { # Inside of profile definition
        # Don't include "link information" comment, as it's outdated and bad
        next if ($row =~ /link information/);
        $newRow = '';

        if ($row =~ /^\s+}/) {
            $newRow .= $indentTwo . "ARCH => 'FALCON',\n";
            $newRow .= $indentTwo . "COMMON => [\n";
            $newRow .= $workingCommon;
            $newRow .= $indentTwo . "],\n\n";
            $newRow .= $indentTwo . "FALCON => [\n";
            $newRow .= $workingFalcon;
            $newRow .= $indentTwo . "],\n";
            $newRow .= $indentOne . "},\n";
            $profileParseStep = 0;
            goto LINE_DONE;
        }

        # Reformat the values
        if ($row =~ /([^\s]+)\s+=>\s+([^,]+),(\s+(#.*))?/) {
            my ($section, $value, $comment) = ($1, $2, $4);

            croak "Couldn't adjust field $section!" if (! defined $fieldAdjustments{$section});
            # Remove some rows
            next if ($fieldAdjustments{$section}->{REMOVE});

            croak "No value for field $section!" if (! defined $value);
            $value = valueAdjust($section, atoi($value));
            $comment = commentSet($section, $value, $comment);

            $row = '';
            if (defined $fieldAdjustments{$section}->{PREFIX_COMMENT}) {
                $row = "\n" . $indentTwo . $indentOne . '# ' . $fieldAdjustments{$section}->{PREFIX_COMMENT} . "\n" . $row;
            }

            my $format = $fieldAdjustments{$section}->{FORMAT};
            if (! defined $format) {
                $format = "0x%X";
            }

            my $line = sprintf("%-25s=> $format,", $section, $value);
            if (defined $comment) {
                $line = sprintf("%-40s %s", $line, $comment);
            }
            $row .= $indentTwo . $indentOne . $line . "\n";

            if (defined $fieldAdjustments{$section}->{NEWSECTION} and $fieldAdjustments{$section}->{NEWSECTION} eq 'common')
            {
                $workingCommon .= $row;
            }
            else
            {
                $workingFalcon .= $row;
            }
        }
    }
    elsif ($profileParseStep == 1) { # Starting brace for profile
        $profileParseStep += 1;
        $workingCommon = '';
        $workingFalcon = '';
    }
    else { # Outside of profile
        if ($row =~ /^    ([^s]+) =>$/) {
            $profileParseStep = 1;
        }
    }

LINE_DONE:
    print $outFh $newRow;
}
