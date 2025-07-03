#                                                                      
# LWIDIA_COPYRIGHT_BEGIN                                                
#                                                                       
# Copyright 1999-2003 by LWPU Corporation.  All rights reserved.  All 
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.                       
#                                                                       
# LWIDIA_COPYRIGHT_END                                                  
#-------------------------------------------------------------------------------
# 45678901234567890123456789012345678901234567890123456789012345678901234567890

#-------------------------------------------------------------------------------
# $Id: //sw/integ/gpu_drv/stage_rel/diag/mods/tools/csum.pl#3 $
#-------------------------------------------------------------------------------

# Report checksum differences.

use strict;
use File::Basename;

# Check the arguments.
if (@ARGV != 1) {
    print "usage: perl $0 file\n";
    exit 1;
}
my $LogFileName = $ARGV[0];

my %Results;
my $Id = 0;
my %Min;
my %Max;
my %Total;
my $Samples = 0;

# Open result file.
my $ResultFileName = (fileparse($LogFileName, '\..*'))[0] . '.txt';
open RESULT_FILE, ">$ResultFileName" or die "Can not open $ResultFileName.\n";

# Parse the log file and collect data.
open LOG_FILE, $LogFileName or die "Can not open $LogFileName.\n";
print "Generating $ResultFileName from $LogFileName.\n";
while (my $Line = <LOG_FILE>) {
    chop $Line;
    if (index($Line, 'GLRandom,') != -1) {
        # test,name,checks,loops,rc,bad inst mem,bad bins,alpha,red,green,blue,stencil,depth,bin-number(s) with miscompare
        # Example: "GLRandom,Dos_LW20,14,6999,83,0,2,0,0,128,0,0,0,20,74,"
        my @Elements = split /,/, $Line;

        # Record statistics.
        ++$Samples;
        Process('Bins',    $Elements[ 6]);
        Process('Alpha',   $Elements[ 7]);
        Process('Red',     $Elements[ 8]);
        Process('Green',   $Elements[ 9]);
        Process('Blue',    $Elements[10]);
        Process('Stencil', $Elements[11]);
        Process('Depth',   $Elements[12]);
        next;
    }
    if (index($Line, 'Test ID') != -1) {
        # Example: "Test ID : 31".
        $Id = (split / : /, $Line)[1];
        next;
    }
    if (index($Line, 'Error Code') != -1) {
        # Example: "Error Code = 7083".
        my $ErrorCode  = (split / = /, $Line)[1];

        # Store results.
        my %min    = %Min;
        my %max    = %Max;
        my %total  = %Total;
        my %Errors = (Samples => $Samples, Min => \%min, Max => \%max, Total => \%total);
        $Results{$Id} = {'ErrorCode' => $ErrorCode, 'Errors' => \%Errors};

        # Reset statistics for next run.
        $Samples = 0;
        %Min     = ();
        %Max     = ();
        %Total   = ();

        next;
    }
}

# Report results.
foreach $Id (sort keys %Results) {
    printf RESULT_FILE "%s\n", '-' x 80;
    printf RESULT_FILE "Test ID    = %s\n", $Id;
    printf RESULT_FILE "Error Code = %s\n", $Results{$Id}{'ErrorCode'};

    my $rErrors = $Results{$Id}{'Errors'};
    $Samples   = $rErrors->{Samples};
    if ($Samples != 0) {
        my $rMin   = $rErrors->{Min};
        my $rMax   = $rErrors->{Max};
        my $rTotal = $rErrors->{Total};
        printf RESULT_FILE "Samples    = %d\n\n", $Samples;
        printf RESULT_FILE "     %-10s%-10s%-10s%-10s%-10s%-10s%-10s\n", 'bins', 'alpha', 'red', 'green', 'blue', 'stencil', 'depth';
        printf RESULT_FILE "min  %-10u%-10u%-10u%-10u%-10u%-10u%-10u\n",
            $rMin->{Bins},
            $rMin->{Alpha},
            $rMin->{Red},
            $rMin->{Green},
            $rMin->{Blue},
            $rMin->{Stencil},
            $rMin->{Depth};
        printf RESULT_FILE "max  %-10u%-10u%-10u%-10u%-10u%-10u%-10u\n",
            $rMax->{Bins},
            $rMax->{Alpha},
            $rMax->{Red},
            $rMax->{Green},
            $rMax->{Blue},
            $rMax->{Stencil},
            $rMax->{Depth};
        printf RESULT_FILE "avg  %-10u%-10u%-10u%-10u%-10u%-10u%-10u\n",
            $rTotal->{Bins}/$Samples,
            $rTotal->{Alpha}/$Samples,
            $rTotal->{Red}/$Samples,
            $rTotal->{Green}/$Samples,
            $rTotal->{Blue}/$Samples,
            $rTotal->{Stencil}/$Samples,
            $rTotal->{Depth}/$Samples;
    }
}

#-------------------------------------------------------------------------------
# subroutines
#
sub Total
{
    my ($Param, $Val) = @_;

    if (!defined $Total{$Param}) {
        $Total{$Param} = 0;
    }

    $Total{$Param} += $Val;
}

sub Min
{
    my ($Param, $Val) = @_;

    if (!defined $Min{$Param}) {
        $Min{$Param} = $Val;
        return;
    }

    if ($Val < $Min{$Param}) {
        $Min{$Param} = $Val;
    }
}

sub Max
{
    my ($Param, $Val) = @_;

    if (!defined $Max{$Param}) {
        $Max{$Param} = $Val;
        return;
    }

    if ($Val > $Max{$Param}) {
        $Max{$Param} = $Val;
    }
}

sub Process
{
    my ($Param, $Val) = @_;
 
    Total($Param, $Val);
    Min($Param, $Val);
    Max($Param, $Val);
}
