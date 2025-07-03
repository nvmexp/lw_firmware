#!/usr/bin/perl

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

##---------------------------------------------------------------------------##
# $Id: //sw/integ/gpu_drv/stage_rel/diag/mods/tools/jsdox.pl#3 $
##---------------------------------------------------------------------------##

###############################################################################
#                                                                             #
# jsdox.pl                                                                    #
# --------                                                                    #
#                                                                             #
# Perl script for use as an input filter to Doxygen.  Takes C++ files with    #
# JavaScript object and method definitions, and prints to STDOUT a version    #
# that Doxygen will be able to process.  Basically colwerts the JavaScript    #
# class/method/property structure into a dummy C++ class/method/property      #
# structure.                                                                  #
#                                                                             #
###############################################################################

use strict;

my %classHash;

#indices for a class array
my $iMethods = 0;
my $iReadOnlyProps = 1;
my $iReadWriteProps = 2;

my $gDesc = "";

open(INFILE, "< $ARGV[0]") or die "Can't open $ARGV[0]!\n";
#open(OUTFILE, "> $ARGV[0]-dox") or die "Can't open $ARGV[0]-dox\n";

while(<INFILE>) {
    # Takes in something of one of these forms:
    #       SMethod MyClass_MyMethod
    #       (
    #        params
    #       )
    #
    #       STest   MyClass_MyMethod
    #       (
    #        params
    #        );
    # Leaving this in $_:
    #       var JS_MyClass::MyMethod(...);
    # (eliminates parameter list).
    if (
        /(SMethod|STest)\s+(.+)_(.+)/
        )
    {
       my $methodName = $3;
       my $className = $2;
       my $params = &getParams;

       $methodName = $methodName . $params;

       $_ = "JS_" . $className . "::$methodName;\n";
       if ($1 =~ /SMethod/) {
          $_ = "var " . $_;
          $methodName = "var " . $methodName;
       }
        elsif ($1 =~ /STest/) {
           $_ = "RC " . $_;
           $methodName = "RC " . $methodName;
        }

        print "//! " . $gDesc;
        #print OUTFILE "//! " . $gDesc;

        addMethodToClass($methodName, $className);
    }

    # Takes in:
    #       SProperty ClassName_PropName
    #       (
    #        params
    #       );
    # outputs:
    #       (const) jsval ClassName::PropName = [init-val];
    # (eliminates param. list)
    # and records the property as either a read-only
    # or read-write property of the class, depending
    # on the flag in the SProperty declaration. Was \S* at the end
    elsif (
           /SProperty\s+(\S*)_(.+)/
           )
    {
        my $propName = $2;
        my $className = "JS_" . $1;

        my @paramList;
        my $defaultVal;
        
        while (<INFILE>) {
            push @paramList, $_;
            if ( /\);/ ) {
                last;
            }
        }
        
        $_ = "var ".$className."::".$propName;
        
        @_ = @paramList;
        
        if (&propHasDefaultVal) {
            $defaultVal = &getDefaultPropVal;
            $_ = $_ . " = " . $defaultVal;
        }
        print "//! " . $paramList[scalar(@paramList) - 2];
        #print OUTFILE "//! " . $paramList[scalar(@paramList) - 2];

        $_ = $_ . ";\n";

        if (&readOnlyProp) {
            addReadOnlyPropToClass($propName, $className);
            $_ = "const " . $_;
        }
        else {
            addReadWritePropToClass($propName, $className);
        }
    }

    # Replaces any of these:
    #       (static|extern) SObject MyClass_Object;
    #       (static|extern) SObject g_MyClass_Object;
    # with:
    #       /**
    #          @class JS_MyClass
    #        */
    #       class JS_MyClass;
    elsif (
           s/(?:static|extern)\s+SObject\s+(?:g_)?(\S+)_Object;/class JS_$1;/
           ) {}

    # Replaces any of these:
    #       (static|extern|/* JS_DOX */) SObject MyClass_Object
    #       (static|extern|/* JS_DOX */) SObject g_MyClass_Object
    #       (
    #        params
    #       );
    # with:
    #       /**
    #          @class JS_MyClass
    #        */
    #       class JS_MyClass;
    # (eliminates param. list).
    elsif (
           s/(?:static|extern|\/\*\s*JS_DOX\s*\*\/)\s+SObject\s+(?:g_)?(\S+)_Object.*/class JS_$1;/
           )
    {
        $_ = "/**\n  \@class JS_$1\n */\n" . $_;
        my $temp = $_;
        
        # Skip all the SObject parameters.
        while (<INFILE>) {
            if ( /\);/ ) {
                last;
            }
        }
        
        $_ = $temp;
    }

    # Puts
    # #ifndef DOXYGEN_IGNORE
    # ...
    # #endif
    # around a declaration or
    # definition that begins with
    # JS_CLASS, C_, or P_.
    #
    # Note that the definition
    # must end with a lwrly brace
    # at the beginning of a line.
    #
    elsif (
           /^\s*(?:JS_CLASS\s*\(|C_\s*\(|P_\s*\()/
           )
    {
          print "#ifndef DOXYGEN_IGNORE\n";
          #print OUTFILE "#ifndef DOXYGEN_IGNORE\n";
       print $_;
       #print OUTFILE $_;

        # If this is the definition, put the endif
        # at the end of the function.
        if (!( /\);/ )) {
           my $junk = /(\s*).*/;
           my $indent = $1;
            while (<INFILE>) {
               print $_;
               #print OUTFILE $_;
               if ( /^$indent}.*/ ) {

                   last;
                }
            }
        }
        
        $_ = "#endif\n";
    }
    
    elsif (/^\s*PROP_CONST\s*\(\s*(\S+)\s*,\s*(\S+)\s*,\s*(\S+)\s*\);.*/ ) 
    {
       print $_;
       #print OUTFILE $_;
       
       my $className = "JS_" . $1;
       my $propName = $2;
       
       $_ = "const var ".$className."::".$propName . " = " . $3 . ";\n";

       addReadOnlyPropToClass($propName, $className);
    }

    elsif (/(?:PROP_READONLY|PROP_READWRITE)\s*\(\s*(\S+)\s*,\s*(\S+)\s*,\s*(\S+)\s*,\s*(\S+),\s*(".+")\s*.*/ ) 
    {
       print "#ifndef DOXYGEN_IGNORE\n";
       #print OUTFILE "#ifndef DOXYGEN_IGNORE\n";
       print $_;
       #print OUTFILE $_;
       print "#endif\n";
       #print OUTFILE "#endif\n";

       my $className = "JS_" . $1;
       my $propName = $3;
       
       print "//! " . $5 . "\n";
       #print OUTFILE "//! " . $5 . "\n";
       $_ = "const var ".$className."::".$propName . ";\n";

       if(/^\s*PROP_READONLY.*/)
       {
          addReadOnlyPropToClass($propName, $className);
       }
       else
       {
          addReadWritePropToClass($propName, $className);
       }
    }

    elsif (/\s*JS_CLASS_NO_ARGS\s*\(\s*(\S+)\s*,\s*(".*"),\s*.*/) 
    {
       my $className = "JS_" . $1;
       my $comment = $2;

       print $_;
       #print OUTFILE $_;

       $_ = "/**\n  \@class $className\n  $comment\n */\nclass $className;\n";
    }

    # Yuck.  GPU specific code
    elsif(/(?:GPU_READFUNC|GPU_WRITEFUNC)\s*\(\s*(\S+)\s*,\s*(".+").*/ ) 
    {
       print $_;
       #print OUTFILE $_;
       
       my $methodName = $1;
       my $className = "Gpu";
       my $desc = $2;

       if (/^\s*GPU_READFUNC.*/) {
          $methodName = $methodName . "(var Offset)";
          $_ = "var " . "JS_Gpu::" . $methodName . ";\n";
          $methodName = "var " . $methodName;
       }
       else
       {
          $methodName = $methodName . "(var Offset, var Data)";
          $_ = "RC " . "JS_Gpu::" . $methodName . ";\n";
          $methodName = "RC " . $methodName;
       }
       
       addMethodToClass($methodName, $className);

       print "//! " . $desc . "\n";
       #print OUTFILE "//! " . $desc . "\n";
    }

    print; # prints $_
    #print OUTFILE $_;
}

print "\n";
#print OUTFILE "\n";

&printJSClasses;
#
# End of main loop
#


sub getParams {
    my @tempArr;
    my $numArgs;
    my $argList = "(";
    my $i;
    
    while (<INFILE>) {
        push @tempArr, $_;
        if ( /\);/ ) {
            last;
        }
        else
        {
           $gDesc = $_;
        }
    }

    # Get the number of arguments to this method.
    while ($numArgs = pop @tempArr) {
        # If this is the last numerical argument in the declaration...
        if ($numArgs =~ /\s*(\d+),/) {
            $numArgs = $1;
            last;
        }
    }

    if ($numArgs != 0) {
        for ($i = 1; $i <= $numArgs - 1; $i++) {
            $argList = $argList . "var arg$i, ";
        }
        $argList = $argList . "var arg$i"; # no space and comma after the last one
    }
    

    $argList = $argList . ")";
    return $argList;
}


# first arg = name of method
# second arg = name of class
sub addMethodToClass($$) {
    my $methodName = shift(@_);
    my $className = shift(@_);
    $className = "JS_" . $className;

    push @{ $classHash{$className}->[$iMethods] }, $methodName;
}


# @_ should contain the params used in the declaration
# of the SProperty.
#
# Returns 1 if and only if the property has a default value
# in its parameters.
sub propHasDefaultVal() {
    my $counter = 0;

    # count the number of parameters in the declaration.
    foreach (@_) {
        # Only look to the left of a //
        if ( /\/\//gc ) {
            $_ = substr $_, 0, pos($_)-2;
        }
        if (/,/) {
            $counter++;
        }
    }

    $counter++; # Count the last param (no comma in that one).

    # if the SProperty had 8 parameters, then it included
    # a default value.

    return ($counter == 8);
}


# @_ should contain the params used in the declaration
# of the SProperty.
#
# Returns the default value of this property.
sub getDefaultPropVal() {
    my $counter = 0;

    # the default val will be the 4th parameter.
    foreach (@_) {
        # Only look to the left of a //
        if ( /\/\//gc ) {
            $_ = substr $_, 0, pos($_)-2;
        }
        if (/,/) {
            $counter++;
        }
        if ($counter == 4) {
            # if it's a string...
            if ( /\"/ ) {
                /(\".*\"),/;                
                return $1;
            }
            # if it's a number...
            else {
                /(\S*),/;
                return $1;
            }
        }
    }

    return 0;
}


# @_ should contain the params used in the declaration
# of the SProperty.
#
# returns 1 if and only if the property whose
# declaration we read is a read-only property.
sub readOnlyProp() {
    my $flag = 0;
    
    foreach (@_) {
        # Only look to the left of a //
        if ( /\/\//gc ) {
            $_ = substr $_, 0, pos($_)-2;
        }
        
        if ( /JSPROP_READONLY/ ) {
            $flag = 1;
            last;
        }
    }

    return $flag;
}

# first arg = name of property
# second arg = name of class
sub addReadOnlyPropToClass($$) {
    my $propName = shift(@_);
    my $className = shift(@_);

    push @{ $classHash{$className}->[$iReadOnlyProps] }, $propName;
}


# first arg = name of property
# second arg = name of class
sub addReadWritePropToClass($$) {
    my $propName = shift(@_);
    my $className = shift(@_);

    push @{ $classHash{$className}->[$iReadWriteProps] }, $propName;
}


sub printJSClasses {
    my $counter;
    # for every class in the file...
    for $counter (keys %classHash) {
        
        print "class $counter : public ___JavaScript_Classes___\n";
        #print OUTFILE "class $counter : public ___JavaScript_Classes___\n";
        print "{\n";
        #print OUTFILE "{\n";
        print " public:\n";
        #print OUTFILE " public:\n";
        if ( $classHash{$counter}->[$iMethods] ) {
            @_ = @{ $classHash{$counter}->[$iMethods] };
            &printMethods;
            print "\n\n";
            #print OUTFILE "\n\n";
        }
        
        if ( $classHash{$counter}->[$iReadOnlyProps] ) {
            print "/** \@name Read-Only Properties\n";
            #print OUTFILE "/** \@name Read-Only Properties\n";
            print "*/\n";
            #print OUTFILE "*/\n";
            print "//\@{\n";
            #print OUTFILE "//\@{\n";
            @_ = @{ $classHash{$counter}->[$iReadOnlyProps] };
            &printReadOnlyProps;
            print "//\@}\n\n";
            #print OUTFILE "//\@}\n\n";
        }
        
        if ( $classHash{$counter}->[$iReadWriteProps] ) {
            print "/** \@name Read-Write Properties\n";
            #print OUTFILE "/** \@name Read-Write Properties\n";
            print "*/\n";
            #print OUTFILE "*/\n";
            print "//\@{\n";
            #print OUTFILE "//\@{\n";
            @_ = @{ $classHash{$counter}->[$iReadWriteProps] };
            &printReadWriteProps;
            print "//\@}\n\n";
            #print OUTFILE "//\@}\n\n";
        }
        
        print "};\n\n";
        #print OUTFILE "};\n\n";
    }    
}


sub printMethods() {
    my $method;

    foreach $method (@_) {
        print "   $method;\n";
        #print OUTFILE "   $method;\n";
    }
}


sub printReadOnlyProps() {
    my $prop;

    foreach $prop (@_) {
        print "   const var $prop;\n";
        #print OUTFILE "   const var $prop;\n";
    }
}


sub printReadWriteProps() {
    my $prop;

    foreach $prop (@_) {
        print "   var $prop;\n";
        #print OUTFILE "   var $prop;\n";
    }
}
