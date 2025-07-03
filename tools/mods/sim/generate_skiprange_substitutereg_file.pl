#!/usr/bin/elw perl

#                                                                      
# LWIDIA_COPYRIGHT_BEGIN                                                
#                                                                       
# Copyright 2016 by LWPU Corporation.  All rights reserved.  All 
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.                       
#                                                                       
# LWIDIA_COPYRIGHT_END                                                  
#-------------------------------------------------------------------------------
# 45678901234567890123456789012345678901234567890123456789012345678901234567890

use strict;
use Getopt::Long;

my $output = "skiprange_substitutereg.txt";
my $modsLog = "mods.log";
my $verbose = 0;

GetOptions(
    'help|h'            => \&useage,
    'modsLog:s'         => \$modsLog,
    'output:s'          => \$output,
    'verbose'           => \$verbose,
) or exit(1);

sub useage
{
    print <<EOD;
    Welcome to use $0 to generate skip range and substitute register .txt file!

    useage: $0 [options]

    -help|-h        show manuals
    -verbose        show debug info 
    -modsLog        mods log pointer, please make sure add option "-dump_physical_access" when you run the test, default is mods.log
    -output         output skip range and substitute register file, default is skiprange_substitutereg.txt
EOD
    exit;
}

# 'reg_offset' => reg_data 
my %substitute_reg_offset = (
    'GV100' => {
        '0x00246218' => 0x00000000,
        '0x00246418' => 0x00000000,
        '0x00246618' => 0x00000000,
        '0x00246818' => 0x00000000,
        '0x00246a18' => 0x00000000,
        '0x00246c18' => 0x00000000,
        '0x00246e18' => 0x00000000,
        '0x00120078' => 0x00000001,
        '0x00244018' => 0x00000001,
        '0x00244218' => 0x00000001,
        '0x00244418' => 0x00000001,
        '0x00244618' => 0x00000001,
        '0x00244818' => 0x00000001,
        '0x00244a18' => 0x00000001,
        '0x00244c18' => 0x00000001,
        '0x00244e18' => 0x00000001,
    },
);

# [range_start, range_end]
my %skip_range = (
    'GV100' => [
        [0x00400000, 0x005fffff],
        [0x0013cbe0, 0x0013ccfc], 
    ],
);

open INPUT, "<$modsLog" or die "ERROR: fail to open $modsLog!!\n";
my @all_log_lines = <INPUT>;
my $all_logs = join("",@all_log_lines);

my $Device_Id = undef;
if ($all_logs =~ m/Device\s+Id\s+:\s+([A-Z]{2}[0-9]{2}[0-9A-Z]{1})/)
{
    $Device_Id = $1;
    print "Device Id is $Device_Id\n" if $verbose;
    if (!defined($skip_range{$Device_Id}) and !defined($substitute_reg_offset{$Device_Id}))
    {
        die "ERROR: there's no neither skip range table nor substiture register table defined for $Device_Id";
    }
}
else
{
    die "ERROR: fail to get Device Id in $modsLog!!\n";
}

my $BAR0_BASE = undef;
if ($all_logs =~ m/LW\s+Base\s+:\s+0x([0-9a-f]{8})/)
{
    $BAR0_BASE = hex($1);
}
else
{
    die "ERROR: fail to get LW Base  in $modsLog!!\n";
}

sub InSkipRegRange($)
{
    my ($offset) = @_;

    return 0 unless defined($skip_range{$Device_Id});

    foreach my $range (@{$skip_range{$Device_Id}})
    {
        if($offset >= $range->[0] and $offset <= $range->[1] )
        {
            return 1;
        }
    }
    
    return 0;
}

foreach my $line (@all_log_lines)
{
    if($line =~ m/prd\s+0x([0-9a-f]{8}):\s+([0-9a-f]{1,2})\s+([0-9a-f]{1,2})\s+([0-9a-f]{1,2})\s+([0-9a-f]{1,2})\s+/)
    {
        my $addr = hex($1);
        my @values = ($2, $3, $4, $5);
        foreach( @values)
        {
            $_ = "0".$_ if(length($_) == 1);
        }
        next if ($addr < $BAR0_BASE);
        my $offset = $addr - $BAR0_BASE;
        my $offsetStr = sprintf("0x%08x", $offset);
        if (defined($substitute_reg_offset{$Device_Id}{$offsetStr}) or InSkipRegRange($offset))
        {
            $substitute_reg_offset{$Device_Id}{$offsetStr} = hex("$values[3]$values[2]$values[1]$values[0]");
        }
    }
}

open OUTPUT, ">$output";
printf "BAR0_BASE     0x%08x\n", $BAR0_BASE if $verbose;
printf OUTPUT "BAR0_BASE     0x%08x\n", $BAR0_BASE;

foreach my $value (@{$skip_range{$Device_Id}})
{
    printf "SKIP_RANGE    0x%08x    0x%08x\n",$value->[0],$value->[1] if $verbose;
    printf OUTPUT "SKIP_RANGE    0x%08x    0x%08x\n", $value->[0], $value->[1];  
}

foreach my $offset (sort keys %{$substitute_reg_offset{$Device_Id}})
{
    printf "SKIP_REG      $offset    0x%08x\n", $substitute_reg_offset{$Device_Id}{$offset} if $verbose;
    printf OUTPUT "SKIP_REG      $offset    0x%08x\n", $substitute_reg_offset{$Device_Id}{$offset};
}
