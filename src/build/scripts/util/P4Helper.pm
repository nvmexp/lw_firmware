#
# Copyright 2011-2016 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
#
# Set of functions to integrate p4 with perl scripts
# Code was originally from release-imgs-if-changed.pl
# Taken out in order to be of use from within other scripts
#

package p4Helper;
use strict;
use warnings;

use Exporter qw(import);
our @EXPORT_OK = qw(p4AddOrEdit p4Add p4Edit p4Exists p4SetPath);

my $p4 = "ERROR"; # make the commands fail if a script forgets set p4

# Routine to set the p4 command path used by the other routines
sub p4SetPath
{
    print "USAGE: p4SetPath( p4commandPath )" unless @_ == 1;
    $p4 = shift;
}

#
# Utility perforce routine that either adds a new file
# or edits an existing one
#
sub p4AddOrEdit
{
    my $file  = shift;
    if (p4Exists($file))
    {
        return p4Edit($file);
    }
    return p4Add($file);
}

# Utility perforce routine that adds a file to perforce.
sub p4Add
{
    my $file   = shift;
    my @output = `$p4 -s add $file`;
    foreach (@output)
    { 
        print;
        chomp;
        if ($_ =~ m/^info: (.+)#(\d+) - (lwrrently )?opened for add$/)
        {
            return 1;
        }
    }
    return 0;
}

# Utility perforce routine that attempts to 'p4 edit' a file.
sub p4Edit
{
    my $file   = shift;
    my @output = `$p4 -s edit $file`;
    #
    # Ilwoking p4 with '-s' will cause p4 to prefix each line with a status tag
    # like "info:", "error:", "text:", etc... For the purposes of 'p4 edit', we
    # should only see "error" and "info". Return success if we dont encounter an
    # "error:" tag.
    #
    foreach (@output)
    { 
        chomp;
        if ($_ =~ m/^(error|warning):/)   
        {
            return 0;
        }
    }
    return 1;
}

# 
# Utility perforce routine that checks to see if a given file exists in
# perforce. 'p4 have' is used since it is a passive operation and perforce does
# not provide a 'p4 exists' command.
#
sub p4Exists
{
    my $file   = shift;
    my @output = `$p4 -s have $file`;
    foreach (@output)
    { 
        chomp;
        if ($_ =~ /^(error|warning):/)
        {
            return 0;
        }
    }
    return 1;
}

1; #End of module
