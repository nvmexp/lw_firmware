#!/usr/bin/perl -w
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# vcast_postprocess.pl
#

use strict;
use warnings "all";

use Getopt::Long;
use IO::File;
use IO::Handle;
use File::Copy;
use File::Find;
use File::stat;
use File::Spec;
use File::Basename;
use JSON;
use Fcntl ':mode';
use List::Util 'first';
use Cwd;
use Scalar::Util qw(looks_like_number);

#
# Global variables
#
my $opt_verbose = 0; # Set to print verbose info
my $opt_clean;       # Set directory to clean/restore backed up files
my @opt_excludeList; # Set to build list of unsused functions to exclude for input project

my $g_restoreFailedCount = 0;   # File count of failed restores from backup during --clean
my @g_AllFileList;              # List of all files in prod_app
my @g_fileList;                 # List of files to pre-process
my @refFunctions;               # Reference list of functions
my $addr2linePath;              # Path to addr2line
my $objdumpPath;                # Path to pmu objdump
my $elfPath;                    # Path to pmu elf
my $opt_imgPrefix;              # Pmu profile name
my $opt_riscvTools;             # Path to riscv tools
my $opt_analysisPath;           # Path to objdump analysis files
my $opt_lwRoot;                 # Path to lw_root dir
my $pmuSw;                      # Path to pmu_sw dir
my $fileCount = 0;


#
# Helper Functions
#
sub verbose { print(@_, "\n") if ( $opt_verbose ); }

#
# Functions
#

# setup for post-processing
sub setup
{
    my $status = 0;

    if (!$opt_analysisPath)
    {
        print STDERR "analysisPath not defined\n";
        goto SETUP_EXIT;
    }

    if (!$opt_lwRoot)
    {
        print STDERR "lw_root path not defined\n";
        goto SETUP_EXIT;
    }

    if (!$opt_imgPrefix)
    {
        print STDERR "imgPrefix not defined\n";
        goto SETUP_EXIT;
    }

    if (!$opt_riscvTools)
    {
        print STDERR "riscvTools path not defined\n";
        goto SETUP_EXIT;
    }

    # Create full analysis path
    $opt_analysisPath = "$opt_lwRoot/pmu_sw/prod_app/$opt_analysisPath";
    $opt_analysisPath =~ s/\\/\//g;

    # Create pmu_sw path
    $pmuSw = "$opt_lwRoot/pmu_sw";
    $pmuSw =~ s/\\/\//g;

    # Set addr2line path
    $addr2linePath = "$opt_riscvTools/bin/riscv64-elf-addr2line";
    $addr2linePath =~ s/\\/\//g;

    unless (-e $addr2linePath || -e "$addr2linePath.exe")
    {
        print STDERR "addr2line not found. check path ($addr2linePath)\n";
        goto SETUP_EXIT;
    }

    # Set and check if elf file is present
    $elfPath = "$opt_lwRoot/drivers/resman/kernel/inc/pmu/bin/$opt_imgPrefix.elf";
    $elfPath =~ s/\\/\//g;

    unless (-e $elfPath)
    {
        print STDERR "pmu elf file ($opt_imgPrefix.elf) not found. check path ($elfPath)\n";
        goto SETUP_EXIT;
    }

    $status = 1;

SETUP_EXIT:
    if (!$status)
    {
        print "Errors in setup. Skipping post-processing\n";
        exit;
    }
}

# usage
sub usage
{
    print <<__USAGE__;

Usage:

   perl vcast_postprocess.pl [ options ]

Options:

    --help                                      This message

    --verbose                                   Enable verbose messages while post-processing

    --analysisPath <path_to_analyzed_files>     Path to objdump analysis files are present

    --riscvTools <path_to_tools>                Path to riscv tools

    --imgPrefix <img_prefix>                    Image prefix of the objdump to analyze

    --lwRoot <path_to_lw_root>                  Path to LW_SOURCE of the branch being built

__USAGE__

    exit 0;
}

# Parse command line arguments
sub parseArgs
{
    my $result;
    my $help;

    $result = GetOptions(
        "help"                      => \$help,              # --help
        "verbose|v"                 => \$opt_verbose,       # --verbose -v
        "analysisPath=s"            => \$opt_analysisPath,  # --analysisPath analysis_path
        "riscvTools=s"              => \$opt_riscvTools,    # --riscvTools path_to_tools 
        "imgPrefix=s"               => \$opt_imgPrefix,     # --imgPrefix img_prefix 
        "lwRoot=s"                  => \$opt_lwRoot,        # --lwroot path_to_lw_root
        );

    usage()             if ! $result;

    usage()             if $help;
}

# Retain only unique elements in array
sub uniq {
    my %seen;
    grep !$seen{$_}++, @_;
}

sub vcastGenerateFileList()
{
    my %funcHashList;
    my @addrList = ();
    my @files = (
        "calltree_task_lpwr.txt", 
        "calltree_task_idle.txt",
        "calltree_vPortTrapHandler.txt");

    print "Processing calltree...";

    foreach my $file (@files)
    {
        open (FHIN, '<', "$opt_analysisPath/$file") or die "unable to open $opt_analysisPath/$file\n";

        my @lines = <FHIN>;

        #
        # Use the list of functions/files under prod_app to match against functions present in
        # task_lpwr's calltree to create a list of files which should be instrumented with 
        # data stored in resident memory
        #
        foreach my $line (@lines)
        {
            if ($line =~ m/ \[0*([0-9A-F]+)\] /i)
            {
                push @addrList, $1;
            }
        }
        close (FHIN);
    }

    # Special case: Make all instrumented functions accessed before odpInit as resident
    open (FHIN, '<', "$opt_analysisPath/calltree__start.txt") or die "unable to open $opt_analysisPath/calltree__start.txt\n";

    my @lines = <FHIN>;

    foreach my $line (@lines)
    {
        if ($line =~ m/odpInit/)
        {
            last;
        }
        if ($line =~ m/ \[0*([0-9A-F]+)\] /i)
        {
            push @addrList, $1;
        }
    }
    close (FHIN);

    print "done\n";

    @addrList = uniq(@addrList);
    my $hexString = ();
    for (my $i = 0; $i < @addrList; $i++)
    {
        my $hex = sprintf(" 0x%s", $addrList[$i]);
        $hexString = $hexString.$hex
    }

    my $cmd = "$addr2linePath -i -e $elfPath $hexString";
    my @outputData = `$cmd`;
    if ($? != 0)
    {
        die "error running addr2line: $! ($?)\n";
    }
    # If functions found, store them in an array
    if (@outputData)
    {
        foreach my $line (@outputData)
        {
            if ($line =~ m/pmu_sw/)
            {
                $line =~ s/.*[\\\/]pmu_sw[\\\/]//;
                $line =~ s/\\/\//g;
                my @fields = split(/:\d+/,$line);
                push @g_fileList, "$pmuSw/$fields[0]";
            }
        }
    }

    @g_fileList = uniq(@g_fileList);

    open (FHOUT, '>', "$opt_analysisPath/vcast_resident_filelist.txt") or die "unable to open output file\n";

    foreach my $line (@g_fileList)
    {
        print FHOUT "$line\n";
    }

    close (FHOUT);

}

sub vcastPostProcess
{
    my $keyFound = 0;                                         # Track if key pattern is found
    my $blockStart = 0;                                       # Track the start of a code block
    my $funcCnt = 0;
    my @funcExcludeList;

    # Generate a list of files which require to use resident memory
    vcastGenerateFileList();

    open(FHIN, '<', "../prod_app/vcast_elw/vcast_c_options.h") or die "unable to open file vcast_c_options.h\n";
    my @vcastLines = <FHIN>;
    close(FHIN);

    foreach my $arg (@g_fileList)
    {
        last if (!$arg);

        my $number;

        # Read in all lines from input file
        if (!open(FHIN, '<', $arg))
        {
            warn "unable to open input file $arg. Skipping...\n";
            next;
        }
        {
            local $/ = undef;
            my $inputLines = <FHIN>;
            if ($inputLines =~ m/vcast_unit_(.*)_bytes_(\d+)/)
            {
                $number = $2;
            }
        }
        close(FHIN);
        next if (!defined $number);
        print "\tVCAST post-processing file $arg...index = $number\n";

        #
        # Use the list of files to check if any of them are being instrumented. If so, find the vcast
        # variable indes used in that file and mark that entry as dmem_resident in the header the
        # the variable is declared.
        #
        for (my $i=0; $i<@vcastLines; $i++)
        {
            if ($vcastLines[$i] =~ m/^char vcast_unit_(.*)_bytes_$number\[/)
            {
                my $varName;
                if ($vcastLines[$i] =~ m/char (.*)\[/)
                {
                    $varName = $1;
                    verbose("varName = $varName");
                }
                verbose("$varName found at line $i\n"); 
                if ($vcastLines[$i] =~ m/ = /)
                {
                    $vcastLines[$i] =~ s/=/GCC_ATTRIB_SECTION("dmem_resident", "$varName") =/;
                }
                else
                {
                    $vcastLines[$i] =~ s/;/ GCC_ATTRIB_SECTION("dmem_resident", "$varName");/;
                }
                verbose("final = $vcastLines[$i]");
            }
        }
    }

    open(FHOUT, '>', "../prod_app/vcast_elw/vcast_c_options.h") or die "unable to open file vcast_c_options.h\n";
    for my $line (@vcastLines)
    {
        print FHOUT $line;
    }
    close(FHOUT);
}

#
# Start exelwtion here!
#

my $start_time = time();
parseArgs();
setup();
vcastPostProcess();

my $end_time = time();
my $run_time = $end_time - $start_time;
print "\tVCAST post-process script runtime = $run_time secs\n";
