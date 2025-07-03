#!/usr/bin/perl
#
# Copyright 2017 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

use strict;
use warnings "all";
no warnings 'once';
use Getopt::Long;

# this is the .nm file where the script will look for global initialized data.
my $nmFile;

BEGIN
{
  my $exceptionFile   = "";

  # extract command-line arguments
  GetOptions("nm-file=s"      => \$nmFile,
             "nm-exception=s" => \$exceptionFile,
   );

  unshift @INC, $exceptionFile;
}
use globalDataExceptions;

# call the check routine to check the nmFile for global initialized data.
check($nmFile);

sub check
{
 my $file = shift;

 open(FHANDLE, "<$file") || die("$0 Error: Could not open $file: $!\n");

 foreach my  $line (<FHANDLE>)
 {
    # here the regex id looking for d _varName or D _varName
    if($line =~ /\S+\s\S+\s[d|D]\s_(\w+)/g)
    {
      if(!exists($globalDataExceptions::excp_vars{$1}))
      {
        exit_error("Initialized global data '$1' is used, which is not preset in exception list.");
      }
    }
  }
 close(FHANDLE);
}

sub exit_error
{
  die "\nError: @_\n\n";
}

1;
