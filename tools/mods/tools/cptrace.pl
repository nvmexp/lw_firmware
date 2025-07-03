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
use File::Copy;

#-------------------------------------------------------------------------------
# Check the arguments.

if (@ARGV == 0) {
    print "Usage: perl $0 source destination\n";
    exit 1;
}

my $SourceRoot = $ARGV[0];
my $DestinRoot = $ARGV[1];

# Check if the source is a directory.
die "$SourceRoot is not a directory.\n" unless -d $SourceRoot;

# If the destination directory does not exist, create it.
unless (-d $DestinRoot) {
    mkdir $DestinRoot, 0755 or die "Can not create $DestinRoot.\n"
}

# Append '\' to directory names.
$SourceRoot .= '\\';
$DestinRoot .= '\\';

#-------------------------------------------------------------------------------
# Copy the files.

opendir SOURCE_ROOT, $SourceRoot or die "Can not open $SourceRoot.\n";
foreach my $Directory (readdir SOURCE_ROOT) {
    # Skip '.' and '..'.
    next if (($Directory eq '.') || ($Directory eq '..'));

    my $SourceDir = $SourceRoot . $Directory;

    # Skip files.
    next unless -d $SourceDir;

    # If destination directory does not exist, create it.
    my $DestinDir = $DestinRoot . $Directory;
    unless (-d $DestinDir) {
        mkdir $DestinDir, 0755 or die "Can not create $DestinDir.\n"
    }

    print "$SourceDir -> $DestinDir\n";

    # Copy the subdirectory files to destination directory.
    opendir SUBDIR, $SourceDir or die "Can not open $SourceDir.\n";
    foreach my $File (readdir SUBDIR) {
        # Skip '.' and '..'.
        next if (($File eq '.') || ($File eq '..'));

        # Do not copy .tga files.
        my ($Base, $Suffix) = split /\./, $File;
        next if ((!defined $Suffix) || ($Suffix eq 'tga'));

        # Copy file.
        my $SourceFile = $SourceDir . '\\' . $File;
        my $DestinFile = $DestinDir . '\\' . $File;
        copy $SourceFile, $DestinFile;        
    }
}

