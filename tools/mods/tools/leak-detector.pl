#!/usr/bin/perl -- # -*- perl -*- -p
#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 1999-2012 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

#------------------------------------------------------------------------------
# 345678901234567890123456789012345678901234567890123456789012345678901234567890

# Working principle:
# 1) call MODS, capturing his output in leak_detect.txt
# 2) parse the output looking for the leak report
# 3) parse the leak report info, e.g., location and seqNum
# 4) generate a script for GDB with breakpoints for MODS on each instruction
#   leaking and forcing GDB to dump the call stack (with info on the
#   instruction leaking)
# 5) run MODS with gdb capturing the output in leak_detect_gdb.txt
# 6) parse leak_detect_gdb.txt to get the info about the leaks
# 7) writes a file with all the info about the leaks

# Wishlist:
# - generate the script to stop at the n-th leak
# - generate a script to stop at every constructor/destructor
#   it will be useful to have a way to post-process the alloc/dealloc so that
#   each Free is made correspondant to the alloc (the trick is for each leak,
#   get the address leaked and look for Free(address))
#   read an h and cpp file and look for new not corresponding to delete

use POSIX;
use Getopt::Long;

use strict;

# path to Utils
use lib "./doxygen/";
use lib "./";
#use lib "/home/scratch.gsaggese_g8x/src2/sw/pvt/main_lw5x/diag/mods/tools/";
#use Utils;
#use lib "./perl_libs/lib/5.6.1";
#use lib "./perl_libs/lib/site_perl/5.6.1/i686-linux";

BEGIN {
    # Extract the path of the current exec.
    my @tmp = split(/\//, $0);
    pop(@tmp);
    my $path = join("/", @tmp);
    # Add the path to INC.
    unshift(@INC, $path);
} ## end BEGIN
use Utils;

# Turn all buffering off.
select((select(STDOUT), $| = 1)[0]);
select((select(STDERR), $| = 1)[0]);
select((select(STDIN),  $| = 1)[0]);

our ($NO_FIRST_PASS, $NO_SECOND_PASS, $USE_TEE) = (0, 0, 0);
our ($ALL_MEM, $FIND_ALL_CALLS, $EXIT_AFTER_LAST_LEAK) = (0, 0, 0);
our ($GDB_VERSION) = ("pre-6.0");
our ($HELP) = (0);
our ($EXE, $OPT) = ("", "");

$Utils::DBG_LVL = 1;

if ($#ARGV < 0) {
    &Usage();
    die "You have to specify args";
}

my ${findAllCalls_script} = "";

# Parse arguments
while ($#ARGV >= 0) {
    if ($ARGV[0] eq "-no_first_pass") {
        $NO_FIRST_PASS = 1;
    } elsif ($ARGV[0] eq "-no_second_pass") {
        $NO_SECOND_PASS = 1;
    } elsif ($ARGV[0] eq "-all_mem") {
        $ALL_MEM = 1;
    } elsif ($ARGV[0] eq "-gdb_version") {
        shift(ARGV);
        $GDB_VERSION = $ARGV[0];
    } elsif ($ARGV[0] eq "-exit_after_last_leak") {
        $EXIT_AFTER_LAST_LEAK = 1;
    } elsif ($ARGV[0] eq "-find_all_calls") {
        $FIND_ALL_CALLS = 1;
        $findAllCalls_script = $ARGV[0];
        shift(ARGV);
    } elsif ($ARGV[0] eq "-show_mods") {
        $USE_TEE = 1;
    } elsif ($ARGV[0] eq "-h" || $ARGV[0] eq "--help") {
        $HELP = 1;
    } elsif ($ARGV[0] eq "-v") {
        shift(@ARGV);
        $Utils::DBG_LVL = $ARGV[0];
    } else {
        print "Argument $ARGV[0] not recognized, passing remaining arguments to MODS\n";
        $EXE = $ARGV[0];
        shift(@ARGV);
        $OPT = '"' . join('" "', @ARGV) . '"';
        last;
    }
    shift(@ARGV);
}

if ($HELP eq 1) {
    &Usage();
    exit(-1);
}

# ------------------------------------------------------------------------
# params
# ------------------------------------------------------------------------

our $LWRRENT_DIR=$ELW{PWD};

our $LOG1 = "mods_leak_log1.txt";
our $LOG2 = "mods_leak_log2.txt";
our ($GDB_SCRIPT) = ("find_all_leaks.gdb");

our $LEAK_REPORT = "leak_report.txt";

our $findMemRef_script = "find_all_ref.gdb";
our $findMemRef_log = "find_all_ref.txt";

Utils::Frame("$0\n", 3, 1);

Utils::Note("Please send feedback to MODS-GPU (MODS-GPU\@exchange.lwpu.com)\n", 1);

Utils::Note("\nParameters parsing:\n".
    "$0:\n".
    "-no_first_pass= $NO_FIRST_PASS\n".
    "-no_second_pass= $NO_SECOND_PASS\n".
    "-show_mods= $USE_TEE\n".
    "-all_mem= $ALL_MEM\n".
    "-find_all_calls= $FIND_ALL_CALLS\n".
    "-gdb_version= $GDB_VERSION\n".
    "-exit_after_last_leak= $EXIT_AFTER_LAST_LEAK\n".
    "\tMODS exelwtable path= $EXE\n".
    "\tMODS opts= $OPT\n".
    1);

Utils::Line(1, 1);
Utils::Frame("\nPATH from perl= ".$ELW{"PATH"}, 1, 1);
Utils::Line(1, 1);
Utils::SysCall("bash -c \"which gdb\"", 1, 1);
Utils::Line(1, 1);
Utils::SysCall("$EXE -v", 1, 0);

# ------------------------------------------------------------------------
# Check params. 
# ------------------------------------------------------------------------

my $cntTmp = $FIND_ALL_CALLS + $ALL_MEM;
if ($cntTmp > 1) {
  die "$0 - ERROR: you must specify one option\n";
}

if ($GDB_VERSION ne "pre-6.0" && $GDB_VERSION ne "6.5" &&
    $GDB_VERSION ne "7.0") {
    die "$0 - ERROR: you must select a valid version of GDB\n";
}

# ------------------------------------------------------------------------
# Main
# ------------------------------------------------------------------------

our $TEE;
if ($USE_TEE) {
    $TEE = " | tee ";
} else {
    $TEE = " > ";
}

my ($time, $str, $cmd) = ("", "", "");

# holds all the seqNums of the leaks
my @leakSeqNums;
# for each seqNum of a leak -> record with info
my %leakInfo;

# --------------------------------------------------------------------
# FIRST PASS
# --------------------------------------------------------------------
my $hdr = "First Pass";
Utils::Frame("* $hdr *", 2, 1);
Utils::StartTime($hdr, 1);

my $cmd = "$EXE $OPT $TEE $LOG1";
if ($NO_FIRST_PASS ne 1) {
    Utils::SysCall("$cmd", 1, 0);
} else {
    Utils::Note("Skipping: $cmd" ,1);
}

Utils::StopTime($hdr, 1);

# read the output file (fills leakSeqNums and leakInfo)
&ParseModsOutput($LOG1, \@leakSeqNums, \%leakInfo);

Utils::Note("I found ".(scalar(@leakSeqNums))." memory leaks", 1);

# POOL_DEPENDENT
# in pool.cpp poolBp points to 
#    volatile UINT32 SeqNumToBreak = 0xffffffff;
# since it is used to break on that instruction
my $poolBp = 133;

if ($FIND_ALL_CALLS eq 1) {
    # #######################################################
    # Find all the calls to a function
    # #######################################################

    Utils::Frame("Action: Find all the calls of functions", 0, 1);

    Utils::Note("Paths:\n".
                "\tlwrrent dir= $LWRRENT_DIR\n".
                "\tDestination GDB script= ${findAllCalls_script}\n".
                1);

    # generate gdb script
    open FH, ">${findAllCalls_script}" or die "I could not open ${findAllCalls_script}";
    print FH "# set the options for gdb\n".
    "set args $OPT\n\n".
    "# to initialize the breakpoints\n".
    "break 'Utility::Pool::Initialize'\n".
    "commands\ndisplay Utility::Pool::d_SequenceCounter\nc\nend\n\n".
    "\n# -------------------------------------------------------------------------------------\n".
    "break 'TO_COMPLETE'\n".
    "# e.g., break 'TraceModule::TraceModule(Trace3DTest *, basic_string<char, string_char_traits<char>, __default_alloc_template<true, 0> >, GpuTrace::TraceFileType, unsigned int)'".
    "commands\n".
    "echo TO_COMPLETE\\n".
    "bt\nc\nend\n\n".
    "\ninfo break\n".
    "\n\nrun\n";

    if ($EXIT_AFTER_LAST_LEAK) {
        print FH "echo GDB exited successfully\\n\n";
        print FH "quit\n";
    } else {
        print FH "continue\n";
    }

    Utils::Note("I wrote the script in ${findAllCalls_script}", 1);
    Utils::Note("To run:  gdb ./mods --command=${findAllCalls_script} > ${findAllCalls_script}.log", 1);
    Utils::Note("grep FUNC --line-number LOG | grep ...", 1);

} elsif ($ALL_MEM eq 1) {
    # #######################################################
    # Find all the memory references
    # #######################################################

    Utils::Frame("Action: Find all the alloc/dealloc calls", 0, 1);

    Utils::Note("Paths:\n".
                "\tlwrrent dir= $LWRRENT_DIR\n".
                "\tDestination GDB script= ${findMemRef_script}\n",
                1);

    # generate gdb script
    open FH, ">${findMemRef_script}" or die "I could not open ${findMemRef_script}";
    print FH "# set the options for gdb\n".
        "set args $OPT\n\n".
        "# to initialize the breakpoints\n".
        #  "break pool.cpp:${poolBp}\n\n";
        "break 'Utility::Pool::Initialize'\n\n".
        "run\n".
        "# stops at Bool::Initialize\n".
        "echo \\nStopped at Bool::Initialize\\n".
        "p 'Utility::Pool::d_SequenceCounter'\n".
        "\n# -------------------------------------------------------------------------------------\n".
        "break 'Utility::Pool::WriteDebugGuardBands(Utility::Pool::DebugBlockHeader *)'\n".
        "commands\n".
        "echo WriteDebugGuardBands\\n".
        "p 'Utility::Pool::d_SequenceCounter'\n".
        "bt\n".
        "c\n".
        "end\n".
        "\n# -------------------------------------------------------------------------------------\n".
        "break 'Utility::Pool::Free(void volatile *)'\n".
        "commands\n".
        "echo Free\\n".
        "p 'Utility::Pool::d_SequenceCounter'\n".
        "bt\n".
        "c\n".
        "end\n".
        "\ninfo break\n".
        "\n\nc\n";

    if ($EXIT_AFTER_LAST_LEAK) {
        print FH "echo GDB exited successfully\\n\n";
        print FH "quit\n";
    } else {
        print FH "continue\n";
    }

    close FH;

    Utils::Note("The gdb cmd file is in ${findMemRef_script}", 1);

    # --------------------------------------------------------------------
    # Retrieve info about leaks (2nd PASS)
    # --------------------------------------------------------------------

    my $hdr = "Computing all memory references";
    Utils::Frame("* $hdr *", 2, 1);
    Utils::StartTime($hdr, 1);
    my $cmd = "gdb $EXE --command=${findMemRef_script} | tee ${findMemRef_log}";
    Utils::SysCall("$cmd", 1, 0);
    Utils::StopTime($hdr, 1);

} elsif (scalar(@leakSeqNums) ne 0) {
    # #######################################################
    # Find all the leaks
    # #######################################################

    Utils::Frame("Action: Find all the leaks", 0, 1);

    Utils::Note("Paths:\n".
                "\tlwrrent dir= $LWRRENT_DIR\n".
                "\tlog file #1= $LOG1\n".
                "\tlog file #2= $LOG2\n".
                "\tGDB script= $GDB_SCRIPT\n".
                "\tleak report= $LEAK_REPORT\n",
                1);

    Utils::Note("\nActions:\n".
                "\tno 1st pass= ".Utils::PrintBool($NO_FIRST_PASS)."\n".
                "\tno 2nd pass= ".Utils::PrintBool($NO_SECOND_PASS)."\n",
                1);

    # generate gdb script
    open FH, ">${GDB_SCRIPT}" or die "I could not open ${GDB_SCRIPT}";
    print FH "# set the options for gdb\n".
      "set args $OPT\n\n".
        "# to initialize the breakpoints\n".
          #  "break pool.cpp:${poolBp}\n\n";
          "break 'Utility::Pool::Initialize'\n\n";

    print FH "run\n";
    #"display Utility::Pool::d_SequenceCounter\ndisplay Utility::Pool::SeqNumToBreak\n\n";

    my ($cnt, $tot) = (0, scalar(@leakSeqNums));

    # POOL_DEPENDENT
    # this is the frame number of the alloc
    my $frameNumber = 4;

    print FH "call Utility::Pool::AllocSeqNumbers($tot)\n";
    foreach my $seqNum (sort {$a <=> $b} @leakSeqNums) {
        print FH "call Utility::Pool::AddSeqNumberToBreak($seqNum)\n";
    }
    print FH "break Utility::Pool::BreakOnSeqNumber\n";
    foreach my $seqNum (sort {$a <=> $b} @leakSeqNums) {
        $cnt++;
        print FH "# leak $cnt / $tot\n";
        if ($GDB_VERSION eq "6.5") {
            print FH "disable 1\n";
            if ($cnt == 1) {
                print FH "f 0\n";
            } else {
                print FH "f 4\n";
            }
        }
        print FH "continue\n".
            "# here is the break point on the leaking memory\n".
            # used for extracting the bt
            "echo Start_BackTrace($seqNum)\\n\n".
            "bt\n".
            "echo \\nThe leaking instruction is\\n\n".
            "f ${frameNumber}\n".
            # used for extracting the bt
            "echo End_BackTrace($seqNum)\\n\n\n";
    }

    if ($EXIT_AFTER_LAST_LEAK) {
        print FH "echo GDB exited successfully\\n\n";
        print FH "quit\n";
    } else {
        print FH "continue\n";
    }

    close FH;

    # --------------------------------------------------------------------
    # Retrieve info about leaks (2nd PASS)
    # --------------------------------------------------------------------
    my $hdr = "Second Pass";
    Utils::Frame("* $hdr *", 2, 1);
    Utils::StartTime($hdr, 1);
    my $cmd = "gdb $EXE --command=${GDB_SCRIPT} | tee ${LOG2}";
    if ($NO_SECOND_PASS ne 1) {
        Utils::SysCall("$cmd", 1, 0);
    } else {
        Utils::Note("Skipping: $cmd" ,1);
    }
    Utils::StopTime($hdr, 1);

    # parse the output from GDB
    &ParseGdbOutput($LOG2, \%leakInfo);

    # extract from the leak info records, a map: name of file -> number of leaks
    my %leaksPerFile;
    &CountFileLeaking(\%leakInfo, \%leaksPerFile);

    # print the output in the report
    my ($cnt, $tot) = (0, scalar(@leakSeqNums));
    open FH, ">$LEAK_REPORT" or die "$0 - ERROR: I could not write file $LEAK_REPORT";
    print FH "###################################################################\n";
    print FH "Leak Report\n";
    print FH "###################################################################\n";
    print FH "Number of leaks: $tot\n";
    print FH "Number of leak per file:\n";
    foreach my $key (keys %leaksPerFile) {
        print FH "\t${key}: ".($leaksPerFile{$key})."\n";
    }
    foreach my $i (sort @leakSeqNums) {
        $cnt++;
        print FH "===================================================================\n";
        print FH "Leak number $cnt / $tot\n";
        print FH "===================================================================\n";
        print FH $leakInfo{$i}."\n";
    }
    close FH;
}

Utils::Frame("$0 exited successfully", 2, 1);
exit(0);

# ------------------------------------------------------------------------
sub Usage {
    print "$0 <script args> ./mods <mods arg> script.js <script js arg>\n".
'
    -show_mods               Show the used version MODS
    -no_first_pass           Skip the first MODS run and use the previous results (for debug only)
    -no_second_pass          Skip the second MODS run (for debug only)
    -gdb STRING              Make the script talk to different versions of the used GDB
                             The supported versions are "pre-6.0", "6.5", "7.0".
    -all_mem:                Print all the memory allocations and deallocations
    -all_calls FILE          Generate a file with a gdb script to catch all the calls of a function
    -v INT                   Debug verbosity
    -cmd MODS                The following is the MODS command line
    -exit_after_last_leak    Exit after the last leak is found analyzed
';
}

# ------------------------------------------------------------------------
sub ParseModsOutput {
    my ($file, $p_leakSeqNums, $p_leakInfo) = (@_);

    open FH, "<$file" or die "The fle $file cannot be open";
    my $line = "";
    while (<FH>) {
        chomp;
        $line = $_;
        Utils::Note($line, 2);
        #Memory leak: address= 0xa5bc660 size= 32 seqNum= 3124
        if ($line =~ /Memory leak:\s+address=\s+(\S+)\s+size=\s+(\S+)\s+seqNum=\s+(\S+)/i) {
            my ($addr, $size, $seqNum) = ($1, $2, $3);
            Utils::Note("\t=> address=$1 size=$2 seqNum=$3", 2);
            push(@$p_leakSeqNums, $seqNum);
            my $msg = "address=$1 size=$2 seqNum=$3";
            $p_leakInfo->{$seqNum}=$msg;
        }
    }
    close FH;
}

# ------------------------------------------------------------------------
sub ParseGdbOutput {
    my ($file, $p_leakInfo) = (@_);

    my ($sub, $DBG_LVL) = ("ParseGdbOutput", 3);
    Utils::Frame("$sub", 1, $DBG_LVL);

    open FH, "<$file" or die "The fle $file cannot be open";
    my $line = "";
    my $state = 0;

    #Start_BackTrace(2584)
    #0  Xp::BreakPoint () at linux/xp.cpp:623
    #1  0x080939f5 in ModsAssertFail (file=0x89b4521 "core/utility/pool.cpp", line=739, cond=0x89b4560 "d_SequenceCounter != SeqNumToBreak")
    #    at core/main/massert.cpp:35
    #2  0x0810951f in Utility::Pool::WriteDebugGuardBands (pbh=0xa49ccf0) at core/utility/pool.cpp:739
    #3  0x0810906a in Utility::Pool::Alloc (Size=172) at core/utility/pool.cpp:534
    #4  0x08097956 in __builtin_vec_new (Size=172) at core/main/modsmain.cpp:355
    #5  0x08843eff in ArgReader::ArgReader (this=0x91cc460, param_list=0x8a84d20, constraints=0x0) at utils/cmdline.cpp:554
    #6  0x082aca28 in Mdiag::Run (Argv=154723568) at mdiag/mdiag.cpp:288
    #7  0x082ac185 in C_Mdiag_Run (pContext=0x91d01f8, pObject=0x91f0f80, NumArguments=1, pArguments=0x
    #End_BackTrace(2584)

    my ($idx, $num) = (0, 0);
    my $endFound = 0;
    my $txt = "";
    while (<FH>) {
        chomp;
        $line = $_;
        Utils::Note($state." - ".$line, $DBG_LVL);

        if ($state eq 0) {
            if ($line =~ /Start_BackTrace\((\d+)\)/i) {
                $idx = $1;
                $txt = "";
                $state = 1;
            }
        } elsif ($state eq 1) {
            if ($line =~ /#(\d+)\s+/) {
                $num = $1;
                $txt .= $line."\n" if ($num > 2);
            } elsif ($line =~ /End_BackTrace\($idx\)/i) {
                $state = 0;
                $p_leakInfo->{$idx} .= "\n".$txt;
                Utils::Frame("I found the record:\n", 2, $DBG_LVL);
                Utils::Note($p_leakInfo->{$idx}."\n", $DBG_LVL);
            } else {
                # multi-line gdb
                $txt .= $line."\n" if ($num > 2);
            }
        } else {
            die "Error: state= $state";
        }
        if ($line =~ /GDB exited successfully/) {
            $endFound = 1;
        }
    }
    close FH;
    die unless $endFound;
}

# ------------------------------------------------------------------------
# extract from the leak info records, a map: name of file -> number of leaks
sub CountFileLeaking {
    my ($p_leakInfo, $p_fileList) = (@_);

    my ($sub, $DBG_LVL) = ("CountFileLeaking", 3);

    #The leaking instruction is
    #5  0x0886859f in ArgReader::ArgReader (this=0x9258498, param_list=0x8ac3120, constraints=0x0) at utils/cmdline.cpp:555
    #555	        values = new vector<string>[arg_count];

    Utils::Frame($sub, 1, $DBG_LVL);

    foreach my $leakRecord (values %$p_leakInfo) {
        Utils::Note("Record= $leakRecord", $DBG_LVL);

        $leakRecord =~ /The leaking instruction is\n.*?\)\s+at (\S+):/m;
        my $file = $1;

        if ($file eq "") {
            Utils::Note("WARNING: in the leak record $leakRecord I did not find the file name", 1);
            #die;
        }

        Utils::Note("-> file= $file", $DBG_LVL);
        if (exists($p_fileList->{$file})) {
            $p_fileList->{$file}++;
        } else {
            $p_fileList->{$file}=1;
        }
    }
}
