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
# $Id: //sw/integ/gpu_drv/stage_rel/diag/mods/tools/pml.pl#3 $
#-------------------------------------------------------------------------------

# Parse MODS log (pml.pl) file(s) and report test results.
# -     -    -

use strict;

# Check the arguments.
if (@ARGV == 0) {
    print "usage: perl $0 files\n";
    exit 1;
}

# Parse the log files.
my $records = 0;
my %count;
while (<>) {
    next unless /^Error Code = (\d+)/;
    ++$count{$1};
    ++$records;
}

# Report results.
printf "total records = %d\n", $records;
printf "code\tcount\t%\n";
foreach my $code (sort{$a <=> $b} keys %count) {
    printf "%d\t%d\t%.2f\n", $code, $count{$code}, $count{$code}*100/$records;
}

