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
# Generate the properties and methods.
#-------------------------------------------------------------------------------

my %Names;
my $FileName;
my $File;
my $FileNum = 1;
my $Lines   = 10001;

while ( <> )
{
   # GCC can not handle very long files. Keep files to about 10000 lines.
   if ( $Lines > 10000 )
   {
      close $File if defined $File;

      $FileName = "lw32_$FileNum.cpp";
      $File = new FileHandle ">$FileName" or die "Can not open $FileName.\n";

      PrintFileHeader();

      ++$FileNum;

      $Lines = 0;
   }

   # Skip lines that do not start with #define. 
   next unless /^\s*#\s*define/;

   #---------------------------------------------------------------------------
   # Search:
   #
   #     #define name number
   #
   # Generate:
   #
   #     SProperty
   #---------------------------------------------------------------------------

   if
   (
      /
         ^                          # beginning of line
         \s*\#\s*define             # #define
         \s+(\w+)                   # name == $1
         \s+\(?                     # optional (
         \s*(0?[xX]?[0-9a-fA-F]+)   # number == $2
         \s*\)?                     # optional )
         \s*$                       # end of line
      /x
   )
   {
      PrintNumberProperty( $1, $2 ) unless exists $Names{$1};
      $Names{$1} =  1;
      $Lines    += 11;

      next;
   }

   #---------------------------------------------------------------------------
   # Search:
   #
   #     #define name number1:number2
   #
   # Generate:
   #
   #     SProperty
   #---------------------------------------------------------------------------

   if
   (
      /
         ^                          # beginning of line
         \s*\#\s*define             # #define
         \s+(\w+)                   # name == $1
         \s+(\d+:\d+)               # number1:number2 == $2
         \s*$                       # end of line
      /x
   )
   {
      PrintStringProperty( $1, $2 )  unless exists $Names{$1};
      $Names{$1} =  1;
      $Lines    += 11;

      next;
   }

   #---------------------------------------------------------------------------
   # Search:
   #
   #     #define name(parameters) body
   #
   #     Parameters can contain either 1 or 2 arguments.
   #
   # Generate:
   #
   #     SMethod
   #---------------------------------------------------------------------------

   if
   (
      /
         ^                          # beginning of line
         \s*\#\s*define             # #define
         \s+(\w+)                   # name == $1
         \(\s*(\S+)\s*\)            # parameters == $2
         \s+(\S+?)\\?               # body == $3
         \s*$                       # end of line
      /x
   )
   {
      my $Name       = $1;
      my $Parameters = $2;
      my $Body       = $3;

      # Check if this line extends to the next line.
      if ( /\\$/ )
      {
         my $NextLine = <>;

         if ( $NextLine =~ /\s*(\S+)\s*$/ )
         {
            $Body .= $1;
         }
      }

      # Parse the parameters.
      my @Arguments    = split /\s*,\s*/, $Parameters;
      my $NumArguments = @Arguments;

      # Generate the method.
      if ( !exists $Names{$Name} )
      {
         if ( 1 == $NumArguments )
         {
            PrintOneArgumentMethod( $Name, \@Arguments, $Body );
            $Lines += 38;
         }
         elsif ( 2 == $NumArguments )
         {
            PrintTwoArgumentMethod( $Name, \@Arguments, $Body );
            $Lines += 41;
         }
         else
         {
            print "#define $Name($Parameters) $Body is not valid.\n";
            exit 2;
         }
      }

      $Names{$Name} = 1;

      next;
   }

} # for each line

#-------------------------------------------------------------------------------
# PrintFileHeader
#-------------------------------------------------------------------------------

sub PrintFileHeader
{
   print $File <<END;
//------------------------------------------------------------------------------
// $FileName was automatically generated from @InFiles.
// $Now
//------------------------------------------------------------------------------

#include "modscom.h"
#include "mods.h"
#include "jscript.h"
#include "script.h"
#include <cassert>

END

}

#-------------------------------------------------------------------------------
# PrintNumberProperty
#-------------------------------------------------------------------------------

sub PrintNumberProperty
{
   my ( $Name, $Number ) = @_;

   print $File <<END;
static SProperty Global_$Name
(
   "$Name",
   0,
   $Number,
   0,
   0,
   JSPROP_READONLY,
   ""
);

END

}

#-------------------------------------------------------------------------------
# PrintStringProperty
#-------------------------------------------------------------------------------

sub PrintStringProperty
{
   my ( $Name, $String ) = @_;

   print $File <<END;
static SProperty Global_$Name
(
   "$Name",
   0,
   "U$String",
   0,
   0,
   JSPROP_READONLY,
   ""
);

END

}

#-------------------------------------------------------------------------------
# PrintOneArgumentMethod
#-------------------------------------------------------------------------------

sub PrintOneArgumentMethod
{
    my ( $Name, $rArguments, $Body ) = @_;

    print $File <<END;
C_( Global_$Name )
{
   assert( pContext     != 0 );
   assert( pArguments   != 0 );
   assert( pReturlwalue != 0 );

   JavaScriptPtr pJavaScript;

   *pReturlwalue = JSVAL_NULL;

   // Check the arguments.
   UINT32 $rArguments->[0] = 0;
   if
   (
         ( NumArguments != 1 )
      || ( OK != pJavaScript->FromJsval( pArguments[0], &$rArguments->[0] ) )
   )
   {
      JS_ReportError( pContext, "Usage: $Name( $rArguments->[0] )" );
      return JS_FALSE;
   }

   if ( OK != pJavaScript->ToJsval( $Body, pReturlwalue ))
   {
      JS_ReportError( pContext, "Error oclwrred in $Name()" );
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

static SMethod Global_$Name
(
   "$Name",
   C_Global_$Name,
   1,
   ""
);

END

}

#-------------------------------------------------------------------------------
# PrintTwoArgumentMethod
#-------------------------------------------------------------------------------

sub PrintTwoArgumentMethod
{
   my ( $Name, $rArguments, $Body ) = @_;

   print $File <<END;
C_( Global_$Name )
{
   assert( pContext     != 0 );
   assert( pArguments   != 0 );
   assert( pReturlwalue != 0 );

   JavaScriptPtr pJavaScript;

   *pReturlwalue = JSVAL_NULL;

   // Check the arguments.
   UINT32 $rArguments->[0] = 0;
   UINT32 $rArguments->[1] = 0;
   if
   (
         ( NumArguments != 2 )
      || ( OK != pJavaScript->FromJsval( pArguments[0], &$rArguments->[0] ) )
      || ( OK != pJavaScript->FromJsval( pArguments[1], &$rArguments->[1] ) )
   )
   {
      JS_ReportError( pContext,
      "Usage: $Name( $rArguments->[0], $rArguments->[1] )" );
      return JS_FALSE;
   }

   if ( OK != pJavaScript->ToJsval( $Body, pReturlwalue ))
   {
      JS_ReportError( pContext, "Error oclwrred in $Name()" );
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

static SMethod Global_$Name
(
   "$Name",
   C_Global_$Name,
   2,
   ""
);

END

}
