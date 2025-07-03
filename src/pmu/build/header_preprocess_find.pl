#!/usr/bin/perl -w

#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# This script finds all pre-processable PMU headers. This is to be used by makefiles instead of
# a wildcard for populating a list of targets or prerequisites.
#
# >perl $(LW_SOURCE)/pmu_sw/build/header_preprocess_find.pl --search_path $(LW_SOURCE)/pmu_sw/inc
#
use warnings;
use strict;
use Getopt::Long;   # GetOptions
use File::Basename; # Basename
use File::Find;     # find

##############################################################################
# Define locals
##############################################################################
my $search_path;
my $file_text;
my $file_handle;
my $header_file_basename;

##############################################################################
# Capture command line arguments
##############################################################################
GetOptions('search_path=s'  => \$search_path)
or die("Error in command line arguments\n");

##############################################################################
# Find all header files in search_path that include generated versions of
# themselves
##############################################################################

my $ext = "h";
find({ wanted => (sub {
    if (-f && $_ =~ /^.*?\.$ext$/) {
        my $header_file = $_;
        # Open file
        open $file_handle, '<', $header_file
        or die("Failed to open $header_file");
        local $/ = undef;
        $file_text = <$file_handle>;
        close $file_handle;

        # Get the output file's basename
        $header_file_basename = basename($header_file);

        #
        # If the header is including its own generated file, it is probably looking
        # to be pre-processed
        #
        if ($file_text =~ /#include "g_$header_file_basename"/g )
        {
            print("$header_file ");
        }
    }
}), no_chdir => 1 }, $search_path);
