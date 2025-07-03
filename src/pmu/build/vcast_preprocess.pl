#!/usr/bin/perl -w
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# vcast_preprocess.pl
#   This script has 2 functionalities:
#   1. To pre-process all C files to identify blocks of code containing
#   error handling code such as PMU_HALT/BREAKPOINT and wrap them with
#   pragmas to ensure they are not instrumented for vectorcast since this
#   code can never be tested functionally. Only used in vectorcast builds
#   
#   2. To skip instrumentation of compiler optimized out functions to get
#   more accurate coverage numbers. The following steps are ilwolved:
#   - Create an reference function list by using addr2line riscv tool to 
#     run through all addresses in the objdump. This gives a list of functions
#     present in the final objdump (regular and inlined fucntions)
#   - Use ctags to retrieve all the funtions present in every file being
#     instrumented
#   - Use the reference list to filter out functions from the ctags list 
#     which are not being compiled into the final objdump and provide 
#     this list to VCAST_SUPPRESS_COVERABLE_FUNCTIONS so these functions 
#     can be skipped from being instrumented 
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
use Cwd qw(cwd);
use Scalar::Util qw(looks_like_number);

#
# Global variables
#
my $opt_verbose = 0; # Set to print verbose info
my $opt_clean;       # Set directory to clean/restore backed up files
my @opt_excludeList; # Set to build list of unsused functions to exclude for input project

my $g_restoreFailedCount = 0;   # File count of failed restores from backup during --clean
my @g_fileList;                 # List of files to pre-process
my @refFunctions;               # Reference list of functions
my $ctagsPath;                  # Path to ctags
my $addr2linePath;              # Path to addr2line
my $objdumpPath;                # Path to pmu objdump
my $elfPath;                    # Path to pmu elf
my $osType;                     # OS type
my $imgPrefix;                  # Pmu profile name
my $opt_lwTools;                # Path to //sw/tools dir
my $opt_lwRoot;                 # Path to lw_root dir


#
# Helper Functioins
#
sub verbose { print(@_, "\n") if ( $opt_verbose ); }

#
# Functions
#

# setup for pre-processing
sub setup
{
    my $status = 0;

    $imgPrefix = $opt_excludeList[0];
    $osType = $opt_excludeList[1];

    verbose("os = $osType, img = $imgPrefix\n");
    
    # Save metadata in JSON format
    vcastRecordMetadata();

    if (!$opt_lwTools)
    {
        print STDERR "lw_tools path not defined\n";
        goto SETUP_EXIT;
    }

    if (!$opt_lwRoot)
    {
        print STDERR "lw_root path not defined\n";
        goto SETUP_EXIT;
    }

    # Set ctags path based on OS
    if ($osType =~ m/Linux/)
    {
        $ctagsPath = "ctags";
    }
    else
    {
        $ctagsPath = "$opt_lwTools/win32/MiscBuildTools/ctags.exe";
    }
    unless (-e $ctagsPath)
    {
        print STDERR "ctags not found. check path\n";
        goto SETUP_EXIT;
    }

    # Set addr2line path based on OS
    if ($osType =~ m/Linux/)
    {
        $addr2linePath = "$opt_lwTools/riscv/toolchain/linux/release/20190625/bin/riscv64-elf-addr2line";
    }
    else
    {
        $addr2linePath = "$opt_lwTools/riscv/toolchain/MinGW/release/20190625/bin/riscv64-elf-addr2line.exe";
    }
    unless (-e $addr2linePath)
    {
        print STDERR "ctags not found. check path\n";
        goto SETUP_EXIT;
    }

    # Set and check if objdump is present
    $objdumpPath= "$opt_lwRoot/drivers/resman/kernel/inc/pmu/bin/$imgPrefix.objdump";
    unless (-e $objdumpPath)
    {
        print STDERR "pmu objdump file not found. check path ($objdumpPath)\n";
        goto SETUP_EXIT;
    }

    # Set and check if elf file is present
    $elfPath= "$opt_lwRoot/drivers/resman/kernel/inc/pmu/bin/$imgPrefix.elf";
    unless (-e $elfPath)
    {
        print STDERR "pmu elf file not found. check path ($elfPath)\n";
        goto SETUP_EXIT;
    }

    $status = 1;

SETUP_EXIT:
    if (!$status)
    {
        print "Errors in setup. Skipping pre-processing\n";
        exit;
    }
}

# usage
sub usage
{
    print <<__USAGE__;

Usage:

   perl vcast_preprocess.pl [ options ]

Options:

    --help                                      This message

    --verbose                                   Enable verbose messages while pre-processing

    --fileList <list_of_files>                  List of C files to be pre-processed

    --clean <dir_to_clean>                      Restore all C files from backups present under the specified
                                                directory and sub-directories

    --buildExcludeList <project_prefix> <os>    Build a list of functions which have been optimized out 
                                                from final build to exclude from instrumentation

    --lwTools <path_to_lw_root>                 Path to //sw/tools

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
        "filelist=s{1,}"            => \@g_fileList,        # --filelist list_of_files
        "verbose|v"                 => \$opt_verbose,       # --verbose -v
        "clean=s"                   => \$opt_clean,         # --clean dir_to_clean
        "buildExcludeList=s{2}"     => \@opt_excludeList,   # --buildExcludeList project_prefix os
        "lwTools=s"                 => \$opt_lwTools,       # --lwTools path_to_tools
        "lwRoot=s"                  => \$opt_lwRoot,        # --lwroot path_to_lw_root
        );

    usage()             if ! $result;

    usage()             if $help;
}

# restore files to original state on clean/clobber command
sub restoreFiles
{
    my $inFile = $File::Find::name;
    # if backup file found, restore it
    if($inFile =~ m/\.prebak$/)
    {
        my $outfile = substr($inFile, 0, -7);
        my $info = stat($outfile);
        # Make sure original file is writeable (i.e. still checked out on perforce)
        if (($info->mode & S_IWUSR) == 0)
        {
            print "\tFile $outfile is write-protected! Skipping restore and deleting backup\n";
            unlink($inFile) or warn "unable to delete $inFile: $!";
            $g_restoreFailedCount++;
        }
        else
        {
            print "\tRestoring $outfile\n";
            move("$inFile", "$outfile") or die "Restore failed: $!\n";
        }
    }
}

# Restore original files if clean/clobber arg is passed
sub clean
{
    find({ wanted => \&restoreFiles,}, "$opt_clean");
    if ($g_restoreFailedCount != 0)
    {
        print "$g_restoreFailedCount files were not restored! Please revert manually!\n";
    }
}

# Retain only unique elements in array
sub uniq {
    my %seen;
    grep !$seen{$_}++, @_;
}

#
# Build a reference list of functions by using addr2line untility on the objdump addresses to determine
# all the functions that have been compiled in including static functions
#
sub vcastBuildFuncRefList
{
    my @funcRefList;
    my $startAddr;
    my $endAddr;
    print "\tBuilding function reference list...";
    STDOUT->flush();

    open (FHOBJDUMP, '<', "$objdumpPath") or die "unable to open pmu objdump";
    my @objLines = <FHOBJDUMP>;
    foreach my $line (@objLines)
    {
        if($line =~ /^[0-9]/)
        {
            ($startAddr) = $line =~ /^[0-9a-f]+/g;
            verbose("startAddr = $startAddr\n");
            last;
        }
    }
    foreach my $line (reverse @objLines)
    {
        if($line =~ /^[0-9]/)
        {
            ($endAddr) = $line =~ /^[0-9a-f]+/g;
            verbose("endAddr = $endAddr\n");
            last;
        }
    }
    @objLines = ();
    close (FHOBJDUMP);

    my $hexString;
    # Optimization to run addr2line on multiple addresses as each shell spawn costs a lot of time
    my $factor = 5000;  # Number of addresses to process at a time
    my $addrSkip = 2;   # Check function every n addresses
    for (my $i = hex($startAddr); $i <= hex($endAddr); $i+=$factor)
    {
        $hexString = ();
        for(my $j = $i; $j < $i + $factor; $j+=$addrSkip)
        {
            my $hex = sprintf(" 0x%x", $j);
            $hexString = $hexString.$hex;
        }

        my $cmd = "$addr2linePath -fpe $elfPath $hexString";
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
                my @fields = split(' ',$line);
                push @funcRefList, $fields[0];
            }
        }
    }

    print "done\n";
    print "\tRemoving duplicate entries...";
    STDOUT->flush();

    # Filter the array to remove all duplicate function names
    @refFunctions = uniq(@funcRefList);

    print "done\n";


}

sub vcastPreprocess
{
    my $vcastStart = "#pragma vcast_dont_instrument_start\n"; # Pragma to insert before code block
    my $vcastEnd = "#pragma vcast_dont_instrument_end\n";     # Pragma to insert after code block
    my $keyFound = 0;                                         # Track if key pattern is found
    my $blockStart = 0;                                       # Track the start of a code block
    my $funcCnt = 0;
    my @funcExcludeList;

    # Patterns to look for to find code blocks
    my @patterns = (
        qr"^\s*if\s*\(",
        qr"^\s*else if\s*\(",
        qr"^\s*else$",
    );
    
    # Keywords to tag
    my @keywords = (
        qr"PMU_HALT\s*\(",
        qr"PMU_BREAKPOINT\s*\(",
        qr"CHSEQ_FAILURE_EX\s*\(",
    );

    foreach my $arg (@g_fileList)
    {
        print "\tVCAST pre-processing file $arg\n";

        if (@opt_excludeList)
        {
            # Use ctags to retrieve all functions present in a source file
            my $cmd = "$ctagsPath -x --c-kinds=f --format=1 $arg";
            my @outputData = `$cmd`;
            if ($? != 0)
            {
                die "error running ctags: $! ($?)\n";
            }
            # Store all functions found in an array
            if (@outputData)
            {
                foreach my $line (@outputData)
                {
                    my @fields = split(' ', $line);
                    my $funcName = $fields[0];
                    push @funcExcludeList, $funcName;
                }
            }
        }

        # Create backup of original file
        copy("$arg", "$arg\.prebak") or die "file $arg backup failed\n";

        open(FHIN, '<', $arg) or die "unable to open input file $arg\n";

        # Open an intermediate file for pre-processing
        open(FHOUT, '>', "$arg\.preproc") or die "unable to open output file for $arg\n";

        # Read in all lines from input file
        my @lines = <FHIN>;
        for (my $i=0; $i<@lines; $i++)
        {
            # look for code blocks with the required patterns
            foreach my $pattern (@patterns)
            {
                # If found, mark as start of code block
                if ($lines[$i] =~ m/$pattern/)
                {
                    verbose("code blk $pattern starting at line $i\n");
                    $blockStart = $i;
                    $keyFound = 0;
                }
            }

            # Special handling for "default" pattern
            if ($lines[$i] =~ m/^\s*default:/)
            {
                if ($lines[$i+1] =~ m/^\s*{/)
                {
                    verbose("start blk default at $i\n");
                    $blockStart = $i;
                    $keyFound = 0;
                }
            }

            # Look for keywords within code block.
            foreach my $key (@keywords)
            {
                # Mark key found if keyword present
                if ($lines[$i] =~ m/$key/)
                {
                    verbose("keyword $key found at line $i\n");
                    $keyFound = 1;
                }
            }

            # Look for end of block "}". If keyword found within a block of code, insert pragmas to ignore vcast instrumentation
            # for this block of code. Reset variables used to track code block and keywords
            if ($lines[$i] =~ /\}$/)
            {
                verbose("code blk end at line $i keyword found=$keyFound start block line=$blockStart\n");
                if ($keyFound == 1 && $blockStart != 0)
                {
                    splice(@lines, $blockStart, 0, $vcastStart);
                    splice(@lines, $i+2, 0, $vcastEnd);
                    $keyFound = 0;
                }
                $blockStart = 0;
            }
        }

        # Print all lines out to file
        for my $line (@lines)
        {
            print FHOUT $line;
        }

        close(FHIN);
        close(FHOUT);

        # Replace original file with pre-processed file
        move("$arg\.preproc", "$arg") or die "file $arg move failed\n";
    }

    if (@opt_excludeList)
    {
        open(FHCCAST, '>>', "../prod_app/CCAST_.CFG") or die "unable to open file CCAST_.CFG\n";
        print FHCCAST "VCAST_SUPPRESS_COVERABLE_FUNCTIONS:";

        # Build reference function list using addr2line
        vcastBuildFuncRefList();

        print "\tGathering unused functions for skipping instrumentation...";
        STDOUT->flush();

        my @uniqExcludeList = uniq(@funcExcludeList);

        #
        # Compare function list from source file against reference function list and skip
        # instruementation of any function not found by adding the function to CCAST_.CFG
        # under the option of VCAST_SUPPRESS_COVERABLE_FUNCTIONS
        #
        for (my $i=0; $i<@uniqExcludeList; $i++)
        {
            my $found = first { /$uniqExcludeList[$i]/ } @refFunctions;

            if (!$found)
            {
                print FHCCAST " $uniqExcludeList[$i],";
            }
        }
        print "done\n";
        close(FHCCAST);
    }
}

# Function records metadata in JSON format in metadata.json used for submission
sub vcastRecordMetadata()
{
    my %metadata;
    my $p4;

    # Collect pmu-profile being instrumented
    $metadata{'pmuProfile'} = substr $imgPrefix, 8;

    # Collect driver branch being instrumented
    my $cwd = cwd();
    my @dirs = File::Spec->splitdir($cwd);
    pop @dirs;
    pop @dirs;
    $metadata{'driverBranch'} = pop @dirs;

    # Initialize date and changelist number to null
    $metadata{'date'} = "";
    $metadata{'changelistNumber'} = "";

    if ($ELW{DVS_SW_CHANGELIST})
    {
        print "Build CL#$ELW{DVS_SW_CHANGELIST}\n";
        $metadata{'changelistNumber'} = $ELW{DVS_SW_CHANGELIST};
        goto RECORD_METADATA_EXIT;
    }

    # Setting absolute paths here since PATH environment var seems to be nulled out when launched from a build
    if ($osType =~ m/Linux/)
    {
        $p4 = "/home/utils/bin/p4";
    }
    else
    {
        $p4 = "C:/Program Files/Perforce/p4.exe";
    }
    unless (-e $p4)
    {
        print "Unable to find p4 at $p4. Please check path in vcast_preprocess.pl";
        goto RECORD_METADATA_EXIT;
    }

    # Use p4 command to get changelist number and date of synced tree
    my $cmd = "\"$p4\" changes -m 1 -s submitted ../../...#have";
    my @outputData = `$cmd`;
    if ($? != 0)
    {
        print "error running p4 command ($cmd): $! ($?)\n";
        goto RECORD_METADATA_EXIT;
    }
    if (@outputData)
    {
        foreach my $line (@outputData)
        {
            my @fields = split(' ', $line);
            if (!looks_like_number($fields[1]))
            {
                print "Changelist number not detected as a number ($fields[1]). Leaving field empty";
            }
            else
            {
                $metadata{'changelistNumber'} = $fields[1];
            }
            if ($fields[3] !~ /(\d{4})\/(\d\d)\/(\d\d)/)
            {
                print "Changelist date not detected as a number ($fields[3]). Leaving field empty";
            }
            else
            {
                $metadata{'date'} = $fields[3];
            }
        }
    }

RECORD_METADATA_EXIT:
    # Write to metadata.json
    open(FHOUT, '>', '../prod_app/vcast_elw/metadata.json') or die "cannot open metadata.json\n";
    my $jsonPrint = JSON->new->pretty->encode(\%metadata);
    print FHOUT "$jsonPrint";
    close(FHOUT);
}

#
# Start exelwtion here!
#

my $start_time = time();
parseArgs();

if ($opt_clean)
{
    clean();
}
else
{
    setup();
    vcastPreprocess();
}

my $end_time = time();
my $run_time = $end_time - $start_time;
print "\tVCAST pre-process script runtime = $run_time secs\n";
