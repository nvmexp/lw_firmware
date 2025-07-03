#!/usr/bin/perl -w

#
# Copyright 2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# Example usage :
#   > perl
#

use strict;
use warnings 'all';

use Getopt::Long;
use File::Path;
use File::Basename;

my $opt_p4_path;
my $opt_riscv_tools_path;
my $opt_sw_branch_path;
my $opt_output_dir_path;
my $opt_force_is_bp = 0;
my $opt_chip;
my $opt_fetch_script_path;
my $opt_fetch_files_local = 0;
my $opt_print_full_paths = 0;

my $bpFuncName = "riscv_trap1"; # TODO: should this be an optional input arg?
my $haltFuncName = "riscv_halt"; # TODO: should this be an optional input arg?

sub setup {

    my %cfgArgs;

    my $cfgParseResult = GetOptions(\%cfgArgs,
        'p4=s'                    => \$opt_p4_path,
        'tools-path=s'            => \$opt_riscv_tools_path,
        'branch-path=s'           => \$opt_sw_branch_path,
        'output-dir=s'            => \$opt_output_dir_path,
        'force-is-bp'             => \$opt_force_is_bp,
        'chip=s'                  => \$opt_chip,
        'fetch-script-path=s'     => \$opt_fetch_script_path,
        'fetch-files-local'       => \$opt_fetch_files_local, # if 1, fetch files from local dir instead of P4; if 2, fetch files from _out dir instead of released files
        'print-full-paths'        => \$opt_print_full_paths,
    ) or die "Unexpected option!"; # TODO: do pretty help print

    if (!$opt_sw_branch_path || !$opt_output_dir_path || !$opt_chip) {
        die "Bad options!"
    }

    if ($opt_sw_branch_path !~ /\//) {
        # This allows us to provide a branch name instead of a branch path
        if ($opt_sw_branch_path =~ /(r\d+)_\d+/) {
            # Release branch
            $opt_sw_branch_path = "//sw/rel/gpu_drv/$1/$opt_sw_branch_path";
        } else {
            # Dev branch (chips_a, bugfix_main, etc)
            $opt_sw_branch_path = "//sw/dev/gpu_drv/$opt_sw_branch_path";
        }

        print "Inferred full branch path: $opt_sw_branch_path\n";
    }

    if (!$opt_p4_path) {
        $opt_p4_path = "p4";
        print "Using default P4 exelwtable: $opt_p4_path\n";
    }

    if (!$opt_riscv_tools_path) {
        $opt_riscv_tools_path = $ELW{'LW_TOOLS'} . "/riscv/toolchain/linux/release/20190625/bin/";
        print "Using default RISCV tools path: $opt_riscv_tools_path\n";
    }

    if (!$opt_fetch_script_path) {
        my $dirname = dirname(__FILE__);
        $opt_fetch_script_path = "$dirname/../../uproc/build/scripts/riscv-ucode-file-fetch.pl";
        print "Using default RISCV fetch/analyze script path: $opt_fetch_script_path\n";
    }

    $opt_riscv_tools_path =~ s/[\/\\]$//g;
    $opt_riscv_tools_path =~ s/\\/\//g;

    $opt_output_dir_path =~ s/[\/\\]$//g;
    $opt_output_dir_path =~ s/\\/\//g;

    if (!$opt_fetch_files_local) {
        if ($opt_sw_branch_path !~ /^\/\/sw/) {
            die "P4 remote file fetch requested, so expected ucode path in P4 //sw";
        }

        if ($opt_sw_branch_path =~ /\\/) {
            die "P4 remote file fetch requested, so expected valid P4 branch path, no backslashes allowed!";
        }
    }

    $opt_sw_branch_path =~ s/[\/\\]$//g;
    $opt_sw_branch_path =~ s/\\/\//g;

    $opt_chip = lc($opt_chip);
}

my @registerNames = (
    "ra", "sp", "gp", "tp",
    "t0", "t1", "t2", "s0", "s1",
    "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    "s2", "s3", "s4", "s5", "s6", "s7", "s8",
    "s9", "s10", "s11", "t3", "t4", "t5", "t6",
);

my $registerMatchStr = join('|', @registerNames);

use constant {
    CRASH_TYPE_UNKNOWN         => "unknown (possibly RTOS internal failure)",
    CRASH_TYPE_BREAKPOINT      => "breakpoint",
    CRASH_TYPE_HALT            => "halt/panic",
    CRASH_TYPE_ADDR_MISALIGNED => "misaligned address",
    CRASH_TYPE_ACCESS_FAULT    => "mem access fault",
    CRASH_TYPE_ILLEGAL_INSTR   => "illegal instruction",
};

sub parseDataFromLog {
    my $result = {
        #TRAP_PC       => ,
        CRASH_TYPE    => CRASH_TYPE_UNKNOWN,
        CL            => -1,
        REG_MAP       => {},
        PVT_TCB_ADDR  => -1,
        #ERR_CODE_OVERRIDE => ,
        #CRASH_LINE_OVERRIDE => ,
    };

    for (@registerNames) {
        $result->{REG_MAP}->{$_} = "0";
    }

    my $recordTraceBuffer = 0;
    my @traceBufferEntries;

    while (my $line = <>) {
        if ($line =~ /OBJPMU Async:\s+(.*?)$/i) {
            my $printedData = $1;

            if ($printedData =~ /g_CHSEQ_DEBUG\[0\].*?((0x)?[0-9a-fA-F]+)/i) {
                my $chseqDebugData0 = $1;
                my $chseqDebugData0Val;
                if ($chseqDebugData0 =~ /^0x/) {
                    $chseqDebugData0Val = hex($chseqDebugData0);
                } elsif ($chseqDebugData0 =~ /^\d+$/) {
                    $chseqDebugData0Val = $chseqDebugData0 + 0;
                } else {
                    die "Failed to parse: $printedData";
                }

                if (exists $result->{ERR_CODE_OVERRIDE}) {
                    print "AT LINE:\n$line\n";
                    print "WARNING: found new g_CHSEQ_DEBUG[0] when a crash dump record has already been filled. Ignored, stopped reading input.\n";
                    last;
                }

                $result->{ERR_CODE_OVERRIDE} = $chseqDebugData0Val >> 16;
                $result->{CRASH_LINE_OVERRIDE} = $chseqDebugData0Val & 0xFFFF;
            } elsif ($printedData =~ /(Breakpoint|Halt|Halted|Panic) at line number\s+(\d+)/i) {
                my $newLineNo = $2 + 0;

                if ((exists $result->{CRASH_LINE_OVERRIDE}) && ($result->{CRASH_LINE_OVERRIDE} ne $newLineNo)) {
                    print "AT LINE:\n$line\n";
                    print "WARNING: found new exception report when a crash line record has already been filled with a different value. Ignored, stopped reading input.\n";
                    last;
                }

                $result->{CRASH_LINE_OVERRIDE} = $2 + 0;
            } elsif ($printedData =~ /Exception cause.*?\s+at\s+0x([0-9a-fA-F]+)(.*?)$/i) {
                if (exists $result->{TRAP_PC}) {
                    print "AT LINE:\n$line\n";
                    print "WARNING: found new exception report when a crash dump record has already been filled. Ignored, stopped reading input.\n";
                    last;
                }
                $result->{TRAP_PC} = lc($1);

                my $remLine = $2;
                if ($remLine =~ /:\s+Breakpoint$/i) {
                    $result->{CRASH_TYPE} = CRASH_TYPE_BREAKPOINT;
                } elsif ($remLine =~ /:\s+Illegal instruction/i) {
                    $result->{CRASH_TYPE} = CRASH_TYPE_ILLEGAL_INSTR;
                } elsif ($remLine =~ /:\s.*?misalig?ned/i) {
                    $result->{CRASH_TYPE} = CRASH_TYPE_ADDR_MISALIGNED;
                } elsif ($remLine =~ /:\s.*?access\s+fault/i) {
                    $result->{CRASH_TYPE} = CRASH_TYPE_ACCESS_FAULT;
                }

                if ($printedData =~ /(badaddr|badinstr)\s+0x([0-9a-fA-F]+)/i) {
                    if ($1 eq 'badaddr' && lc($2) ne $result->{TRAP_PC}) {
                        $result->{BADADDR} = lc($2);
                    } elsif ($1 eq 'badinstr') {
                        $result->{BADINSTR} = lc($2);
                    }
                }
            } elsif ($printedData =~ /Panic requested by task/i) {
                if ($result->{CRASH_TYPE} ne CRASH_TYPE_UNKNOWN) {
                    print "AT LINE:\n$line\n";
                    print "WARNING: found new PANIC message when a crash dump record has already been filled. Ignored, stopped reading input.\n";
                    last;
                }

                $result->{CRASH_TYPE} = CRASH_TYPE_HALT;
            } elsif ($printedData =~ /OS:\s+(\d+)$/) {
                if ($result->{CL} != -1) {
                    print "AT LINE:\n$line\n";
                    print "WARNING: found new OS: CLno line when a crash dump record has already been filled. Ignored, stopped reading input.\n";
                    last;
                }

                $result->{CL} = ($1) + 0;

                if (($result->{CL} == 0) && !$opt_fetch_files_local) {
                    die "Invalid CL number read! Cannot do P4 file fetch for crash dump where CL number is 0!";
                }
            } elsif ($printedData =~ /pvData.*?0x([0-9a-fA-F]+), pc.*?0x([0-9a-fA-F]+)/i) {
                $result->{PVT_TCB_ADDR} = $1;
                if (exists $result->{TRAP_PC} && $result->{TRAP_PC} ne $2) {
                    die "'Exception at' PC " . $result->{TRAP_PC} . " does not match TCB PC $2!"
                } elsif (!exists $result->{TRAP_PC}) {
                    $result->{TRAP_PC} = $2;
                }
            } elsif ($printedData =~ /Tracebuffer\s.*?entries.*?:/i) {
                $recordTraceBuffer = 1;
                @traceBufferEntries = ();
                next; # skip clearing the recordTraceBuffer flag
            } elsif ($recordTraceBuffer && ($printedData =~ /\[.*?\].*?\s([0-9a-fA-F]+)/i)) {
                push @traceBufferEntries, lc($1);
                next; # skip clearing the recordTraceBuffer flag
            } else {
                my $remLine = $printedData;
                while ($remLine =~ /($registerMatchStr) = 0*([0-9a-fA-F]+)\s*(.*?)$/i) {
                    $result->{REG_MAP}->{$1} = $2;
                    $remLine = $3;
                }
            }

            $recordTraceBuffer = 0;
        }
    }

    if ($opt_force_is_bp) {
        # This might kinda be
        $result->{CRASH_TYPE} = CRASH_TYPE_BREAKPOINT;
    }

    $result->{TRACE_BUFFER} = \@traceBufferEntries;

    return $result;
}

sub fetchData {
    my $parsedData = shift;

    my $chipName = $opt_chip;

    my $elfPath;

    if ($opt_fetch_files_local == 2) {
        $elfPath = "$opt_sw_branch_path/pmu_sw/_out/$chipName\_riscv/g_c85b6_$chipName\_riscv.elf";
    } else {
        $elfPath = "$opt_sw_branch_path/drivers/resman/kernel/inc/pmu/bin/g_c85b6_$chipName\_riscv.elf";
    }

    my $statusCodesPath = "$opt_sw_branch_path/drivers/resman/arch/lwalloc/common/inc/flcnretval.h";

    my $traceBufferStr = "";
    if (@{$parsedData->{TRACE_BUFFER}} > 0) {
        $traceBufferStr = join ",", @{$parsedData->{TRACE_BUFFER}};
        $traceBufferStr = '--ucode-addr="'.$traceBufferStr.'"';
    }

    my $statusVarNameArg = "";

    if (!exists $parsedData->{CRASH_LINE_OVERRIDE}) {
        # Before the crash line override was added, for breakpoints getting the status arg from the registers was fair game.
        # Now it doesn't work in 99% of the cases anymore, so don't waste time on it otherwise.
        # Besides, a print for status error codes is TODO!
        $statusVarNameArg = "--fetch-reg-for-var=status";
    }

    my $clNumArg = "--cl-number=$parsedData->{CL}";

    if ($opt_fetch_files_local) {
        $clNumArg = "";
    }

    my $isBreakpointArg = ($parsedData->{CRASH_TYPE} eq CRASH_TYPE_BREAKPOINT) ? "--trap-is-bp" : "";

    my $fetchScriptCmd = qq{$^X $opt_fetch_script_path --p4="$opt_p4_path" $clNumArg --tools-path="$opt_riscv_tools_path" --output-dir="$opt_output_dir_path" --branch-path="$opt_sw_branch_path" --ucode-elf="$elfPath" --additional-file="$statusCodesPath" --trap-addr=$parsedData->{TRAP_PC} $isBreakpointArg --ret-addr=$parsedData->{REG_MAP}->{ra} --pvt-symbol-addr=$parsedData->{PVT_TCB_ADDR} $traceBufferStr --bp-func-name="$bpFuncName" $statusVarNameArg --output-ignore-ctxsw};
    print "$fetchScriptCmd\n\n";
    my $fetchScriptData = `$fetchScriptCmd`;

    #print "\n\n\nFetch script output:\n\n$fetchScriptData\n\n\n";

    my $fetchScriptOutputFileName = "$opt_output_dir_path/fetch_script_output.txt";

    # Assume output dir already exists at this point
    open(FETCH_SCRIPT_OUTPUT, '>', $fetchScriptOutputFileName) or die "Couldn't open $fetchScriptOutputFileName for read";
    print FETCH_SCRIPT_OUTPUT $fetchScriptData;
    close(FETCH_SCRIPT_OUTPUT);

    $fetchScriptData =~ s/\r//g;
    my @fetchedLines = split(/\n/, $fetchScriptData);

    my $result = {
        TASK_NAME => "",
        ERROR_CODE => "N/A",
    };

    my $retvalFilePath = "";
    my $statusVal = -1;
    my $statusValFound = 0;

    my @codeTraceOutput;

    for my $fetchedLine (@fetchedLines) {
        if ($fetchedLine =~ /flcnretval.h/i) {
            my @fetchedLineParts = split(/ /, $fetchedLine);
            $fetchedLineParts[0] =~ s/^[\/\\]//;
            $retvalFilePath = "$opt_output_dir_path/fetched_branch_files/" . $fetchedLineParts[0];
        } elsif ($fetchedLine =~ /.*?\.[ch]:\d+\s.*?\.[ch]#(\?|\d+)/) {
            push @codeTraceOutput, $fetchedLine;
        } elsif ($fetchedLine =~ /^pvt_symbol:LwosTcbPvt_(.*?)$/i) {
            $result->{TASK_NAME} = $1;
        } elsif ($fetchedLine =~ /status:($registerMatchStr)/) {
            $statusVal = hex($parsedData->{REG_MAP}->{$1});
            $statusValFound = 1;
        }
    }

    if (exists $parsedData->{ERR_CODE_OVERRIDE}) {
        if (!$statusValFound) {
            $statusVal = $parsedData->{ERR_CODE_OVERRIDE};
        } elsif ($statusVal != $parsedData->{ERR_CODE_OVERRIDE}) {
            # Can't imagine a situation where this is legitimate, so crash
            die "Error-code conflict: debug prints indicate error code = " . $parsedData->{ERR_CODE_OVERRIDE} . ", but DWARF data indicates it's $statusVal";
        }
    }

    $result->{TRACE_OUTPUT} = \@codeTraceOutput;

    open(RETVAL_FILE, '<:encoding(UTF-8)', "$retvalFilePath") or die "Couldn't open $retvalFilePath for read";
    while (my $line = <RETVAL_FILE>) {
        # TODO: if any bright soul ever has the genius idea of putting a comment between an err code name and its value this thing will blow UP
        if ($line =~ /^\s*\#\s*define\s*(FLCN_.*?)\s+\(?((0x)?[0-9a-fA-F]+)[Uu]?\)?/) {
            my $errCodeName = $1;
            my $errCodeValue = $2;
            if ($errCodeValue =~ /^0x/) {
                $errCodeValue = hex($errCodeValue);
            } else {
                $errCodeValue += 0;
            }

            #print "$errCodeName : $errCodeValue\n";

            if ($errCodeValue == $statusVal) {
                $result->{ERROR_CODE} = $errCodeName;
                last;
            }
        }
    }
    close(RETVAL_FILE);

    return $result;
}

setup();

print "Please provide crash data as input:\n";

my $parsedData = parseDataFromLog();

print "\nParsed data, ilwoking fetch script...\n\n";

if ($opt_fetch_files_local) {
    print "NOTE: local file fetch requested, so will copy files from supplied local branch path instead of P4 fetch\n\n";
}

my $crashData = fetchData($parsedData);

print "============================================================ PMU detailed crash report: ============================================================\n\n";
print "PMU has hit a $parsedData->{CRASH_TYPE}!\n\n";
if ($parsedData->{CRASH_TYPE} ne CRASH_TYPE_BREAKPOINT) {
    print "This might be problematic! Please contact the PMU team ".
          "(dl sw-gpu-rm-pmu-dev / slack #sw-gpu-pmu-dev) for support!\n";
    print "NOTE: please make sure this is NOT a memory issue before contacting the PMU team. ".
          "PMU is EXPECTED to crash if memory gets corrupted.\n\n";
}
print "The PC addr where the failure happened was $parsedData->{TRAP_PC}\n";
if (exists $parsedData->{BADADDR}) {
    print "The failure oclwrred when accessing memory at this addr: $parsedData->{BADADDR}\n";
} elsif (exists $parsedData->{BADINSTR}) {
    print "The opcode of the illegal instruction was $parsedData->{BADINSTR}\n";
}
print "\n";

print "Trace output (sanitized from context-switches):\n";
my $prevIterWasBPOrHalt = 0;
my $needLineNoWarning = 0;
for (@{$crashData->{TRACE_OUTPUT}}) {
    my $lwrrPrintData = $_;

    if (($lwrrPrintData =~ /$bpFuncName/) || ($lwrrPrintData =~ /$haltFuncName/)) {
        $prevIterWasBPOrHalt = 1;
        next;
    } elsif ($prevIterWasBPOrHalt) {
        if (exists $parsedData->{CRASH_LINE_OVERRIDE}) {
            my $trueLineNo = $parsedData->{CRASH_LINE_OVERRIDE};

            $lwrrPrintData =~ s/:\d+/:$trueLineNo (line-no patched from crashdump)/;
        } elsif (($parsedData->{CRASH_TYPE} eq CRASH_TYPE_BREAKPOINT) || ($parsedData->{CRASH_TYPE} eq CRASH_TYPE_HALT)) {
            $lwrrPrintData .= " (LINENO MIGHT BE INVALID!)";
            $needLineNoWarning = 1;
        }

        $prevIterWasBPOrHalt = 0;
    }

    if ($opt_print_full_paths) {
        if ($lwrrPrintData =~ /^\//) {
            $lwrrPrintData = "$opt_output_dir_path/fetched_branch_files$lwrrPrintData";
        } else {
            $lwrrPrintData = "$opt_output_dir_path/fetched_branch_files/$lwrrPrintData";
        }
    }

    print "$lwrrPrintData\n";
}

if ($needLineNoWarning) {
    print "\nNOTE: cannot confirm line-number for $parsedData->{CRASH_TYPE}. Since PMU is compiled with -Os, if there are multiple breakpoints in this function, this line number might be incorrect!\n"
}

print "\nCrashed in task $crashData->{TASK_NAME}\n";

if (exists $crashData->{ERROR_CODE}) {
    print "Error code: $crashData->{ERROR_CODE}\n";

    if (($crashData->{ERROR_CODE} ne 'N/A') && (exists $parsedData->{CRASH_LINE_OVERRIDE})) {
        print "NOTE: make sure to check out the exact $parsedData->{CRASH_TYPE} location! ".
              "Sometimes PMU code sets a relevant error code in 'status' AFTER ilwoking the $parsedData->{CRASH_TYPE}, instead of BEFORE.\n";
    }
}
