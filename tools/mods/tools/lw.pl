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
# $Id: //sw/integ/gpu_drv/stage_rel/diag/mods/tools/lw.pl#3 $
#-------------------------------------------------------------------------------
# Strip C/C++ header files for #define macros.
#-------------------------------------------------------------------------------
# 45678901234567890123456789012345678901234567890123456789012345678901234567890

use strict;
use FileHandle;
use File::Basename;

#-------------------------------------------------------------------------------
# Check the arguments.
#-------------------------------------------------------------------------------

if ( @ARGV == 0 )
{
   print "Usage: perl $0 file1 file2 ...\n";
   exit 1;
}

my @InFiles = map { basename $_ } @ARGV;
my $Now     = localtime;

#-------------------------------------------------------------------------------
# Open output file, print header, and print header file guard.
#-------------------------------------------------------------------------------

my $OutFileName = "lw_user.h";
my $OutFile     = new FileHandle ">$OutFileName"
                     or die "Can not open $OutFileName.\n";

print $OutFile <<END;
//------------------------------------------------------------------------------
// $OutFileName was automatically generated from @InFiles.
// $Now
//------------------------------------------------------------------------------

#ifndef INCLUDED_LW_USER_H
#define INCLUDED_LW_USER_H

END

#-------------------------------------------------------------------------------
# Strip C/C++ header files for #define macros.
#-------------------------------------------------------------------------------

my %Macros;

while ( <> )
{
   # Skip lines that do not start with #define.
   next unless /^\s*#\s*define/;

   # Add '_' between LW and device.
   s/LW([0-9a-fA-F]{3})_/LW_$1_/;

   # Find the macro name, and make sure it is not been seen before.
   my $Macro = (/^\s*#define\s+(\w+)/)[0];
   next if exists $Macros{$Macro};
   $Macros{$Macro} = 1;

   # Print macro to file.
   print $OutFile $_;

   # Take care of multiline macros.
   while ( /\\$/ )
   {
      $_ = <>;
      print $OutFile $_;
   }

} # for each line

#-------------------------------------------------------------------------------
# Print header file guard.
#-------------------------------------------------------------------------------

print $OutFile <<END;

#endif // !INCLUDED_LW_USER_H
END
