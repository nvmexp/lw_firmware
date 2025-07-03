#!/usr/bin/perl -w

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
# Simple utility script that looks for any files/components which may not be
# synchronized with top-of-tree and prints a highly visible banner if any
# such files are found.
#

use strict;
no strict qw(vars);
use warnings "all";

use Getopt::Long;
use File::Spec::Functions qw(abs2rel);

my @checkList = ();
my $p4        = "p4";
my $errors    = 0;
my $verbose   = '';

# extract command-line arguments
GetOptions("check_list|check-list=s" => \@checkList,
           "p4=s"                    => \$p4,
           "verbose"                 => \$verbose);

@checkList = split(/,/,join(',',@checkList));

# 
# Unset/delete the PWD elwvar to ensure that perforce gets the correct current
# working-directoy when it runs. PWD can sometimes be incorrect.
#
delete $ELW{PWD};

foreach my $item (@checkList)
{
    # skip null-strings/items
    next unless $item;

    # colwert to a relative path and append "..." as appropriate
    $item = abs2rel($item);
    if (-d $item)
    {
        $item .= "/...";
    }
    else
    {
        $item .= "...";
    }

    print "check-build-elw.pl: checking $item\n" if $verbose;

    my @toUpdate = `$p4 sync -nf $item`;
    die "check-build.pl-elw: command failed 'p4 sync -n $item' (status=$?)"
        unless ($? == 0);
    
    foreach $_ (@toUpdate)
    {
        # match: perforce_path#rev - updating local_path
        if ($_ =~ m/^(.+)#(\d+) - updating (.+)$/)
        {
            # extract the name and latest revision for the out-of-sync component
            my $name      = $1;
            my $latestRev = $2;
    
            # now figure out the revision we 'have'
            my @tokens    = split(/([ #])/, `$p4 have $name`);
            die "check-build-elw.pl: command failed 'p4 have $name' (status=$?)" unless ($? == 0);
    
            my $haveRev = $tokens[2];
            warn "check-build-elw.pl: $name: latest revision (#$latestRev), have (#$haveRev)\n";
            $errors += 1;
        }
    }
}

# print a highly-visible banner if any components are out-of-date
if ($errors != 0)
{
    print "\n";
    print "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    print "!!!WARNING                                                                     WARNING!!!\n";
    print "!!!        Not all build components are up-to-date! Re-sync before submitting.        !!!\n";
    print "!!!                                                                                   !!!\n";
    print "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
}

# exit with error status (0=success=up-to-date)
exit($errors != 0);

