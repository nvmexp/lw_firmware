#!/usr/bin/perl -w

#
# Copyright 2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# Example usage :
#   > perl riscv-ucode-file-fetch.pl --p4 <path-to-p4> --tools-path <path-to-riscv-tools> --ucode-elf <path-to-ucode-elf-file> --ucode-addr <addr-in-ucode> --cl-number <p4-cl-number> --output-dir <path-to-dir>
#

use strict;
use warnings 'all';

use Getopt::Long;
use Carp;
use File::Path;
use File::Copy;
use File::Basename;
use File::Spec;
use IO::Compress::Zip qw(zip $ZipError);

my $opt_p4_path;
my $opt_riscv_tools_path;
my $opt_ucode_elf_file;
my $opt_trap_addr = "-1";
my $opt_ret_addr = "-1";
my @opt_ucode_trace_addr_list;
my $opt_cl_number;
my $opt_output_dir_path;
my $opt_sw_branch_path;
my $opt_fetch_reg_for_var = 0;
my $opt_bp_func_name = "riscv_trap1";
my $opt_halt_func_name = "riscv_halt";
my $opt_pvt_symbol_addr;
my @opt_additional_files;
my $opt_output_ignore_context_switches = 0;
my $opt_trap_is_bp = 0;

my %ucodeAddrTraceListHash;
my $trapIlwokeAddr; # if bp is inlined, this is the ebreak location; otherwise it's the jal riscv_trap1 location

sub setup {

    my %cfgArgs;

    my $cfgParseResult = GetOptions(\%cfgArgs,
        'p4=s'                    => \$opt_p4_path,
        'tools-path=s'            => \$opt_riscv_tools_path,
        'ucode-elf=s'             => \$opt_ucode_elf_file,
        'trap-addr=s'             => \$opt_trap_addr,
        'trap-is-bp'              => \$opt_trap_is_bp,
        'ret-addr=s'              => \$opt_ret_addr,
        'ucode-addr=s'            => \@opt_ucode_trace_addr_list,
        'cl-number=i'             => \$opt_cl_number,
        'output-dir=s'            => \$opt_output_dir_path,
        'branch-path=s'           => \$opt_sw_branch_path,
        'fetch-reg-for-var=s'     => \$opt_fetch_reg_for_var,
        'bp-func-name=s',         => \$opt_bp_func_name,
        'halt-func-name=s',       => \$opt_halt_func_name,
        'pvt-symbol-addr=s',      => \$opt_pvt_symbol_addr,
        'additional-file=s'       => \@opt_additional_files,
        'output-ignore-ctxsw'     => \$opt_output_ignore_context_switches,
    ) or die "Unexpected option!"; # TODO: do pretty help print

    if (!$opt_p4_path || !$opt_riscv_tools_path || !$opt_ucode_elf_file || ($opt_trap_addr eq "-1") || ($opt_ret_addr eq "-1") || !$opt_output_dir_path || !$opt_sw_branch_path) {
        die "Bad options!"
    }

    $opt_ucode_elf_file =~ s/\\/\//g;

    $opt_riscv_tools_path =~ s/[\/\\]$//g;
    $opt_riscv_tools_path =~ s/\\/\//g;

    $opt_output_dir_path =~ s/[\/\\]$//g;
    $opt_output_dir_path =~ s/\\/\//g;

    if ($opt_cl_number) {
        if ($opt_sw_branch_path !~ /^\/\/sw/) {
            die "CL number provided, so expected ucode path in P4 //sw";
        }

        if ($opt_sw_branch_path =~ /\\/) {
            die "CL number provided, so expected valid P4 branch path, no backslashes allowed!";
        }
    }

    $opt_sw_branch_path =~ s/[\/\\]$//g;
    $opt_sw_branch_path =~ s/\\/\//g;

    $trapIlwokeAddr = sprintf("%x", hex($opt_ret_addr) - 4);

    # Allow comma-separated lists as well as multiple --ucode-addr
    @opt_ucode_trace_addr_list = split(/,/, join(',', @opt_ucode_trace_addr_list));

    for (@opt_ucode_trace_addr_list) {
        $ucodeAddrTraceListHash{lc($_)} = 1;
    }

    # Allow comma-separated lists as well as multiple --additional-file
    my $additionalFilesTmp = join(',', @opt_additional_files);
    $additionalFilesTmp =~ s/\\/\//g;
    @opt_additional_files = split(/,/, $additionalFilesTmp);

    # Clean the output dir
    rmtree($opt_output_dir_path);

    # Create the output dir
    mkpath($opt_output_dir_path);
}

sub formatPath {
    my $filePath = shift;

    $filePath =~ s/\\/\//g;
    my $trimmedBranchPath = $opt_sw_branch_path;
    $trimmedBranchPath =~ s/^\/\/sw/sw/g;
    if ($filePath !~ /$trimmedBranchPath\//) {
        die "File path $filePath does not match provided branch path $opt_sw_branch_path!";
    }

    my @filePathParts = split($trimmedBranchPath, $filePath);
    return { P4_PATH => "$opt_sw_branch_path" . $filePathParts[1], RELATIVE_PATH => $filePathParts[1], };
}

sub getCFilePaths {
    my ($elfFilePath, $objdumpFilePath, $refAddrList) = @_;

    my @addrList = grep {$_} @{$refAddrList};

    my %addrToFuncCall;
    my $addrListMatchStr = join '|', @addrList;
    open(OBJDUMP_FILE, '<:encoding(UTF-8)', "$objdumpFilePath") or die "Couldn't open $objdumpFilePath for read";
    while (my $line = <OBJDUMP_FILE>) {
        if ($line =~ /($addrListMatchStr):\s+[0-9a-fA-F]+\s+jal.*?<(.*?)>$/) {
            $addrToFuncCall{lc $1} = $2;
        }
    }
    close(OBJDUMP_FILE);

    my @cFilePathsLineNums;

    for my $lwrrAddr (@addrList) {
        my $outputFilePath = "$elfFilePath.$lwrrAddr.lwrr_c_file_paths.txt";

        system("$opt_riscv_tools_path/riscv64-elf-addr2line -fie $elfFilePath $lwrrAddr > $outputFilePath");

        open(PATHS, '<:encoding(UTF-8)', $outputFilePath) or die "Couldn't open $outputFilePath for read";

        my $line = '';
        my $i = 0;
        my @cFilesData;
        my @funcNames;

        while ($line = <PATHS>) {
            if ($i % 2 == 0) {
                my $lineTrimmed = $line;
                $lineTrimmed =~ s/^\s+//g;
                $lineTrimmed =~ s/\s+$//g;
                push @funcNames, $lineTrimmed;
            } else {
                my @lineParts = split(/ /, $line);
                push @cFilesData, $lineParts[0];
            }

            $i++;
        }

        close(PATHS);

        $i = 0;
        for my $cFileData (@cFilesData) {
            if ($cFileData =~ /\?/) {
                next;
            }
            my @cFileDataParts = split(/:/, $cFileData);
            if ($cFileDataParts[-2] !~ /\.[ch]$/) {
                die "Expected .c or .h file extension for file " . $cFileDataParts[-2] . ", full c file data = $cFileData";
            }

            my $lineNum = $cFileDataParts[-1] + 0;

            push @cFilePathsLineNums, {
                P4_FILE_PATH => formatPath($cFileDataParts[-2])->{P4_PATH},
                LINE_NUM => $lineNum,
                FUNC_NAME => $funcNames[$i],
                UCODE_ADDR => $lwrrAddr,
            };

            $i++;
        }
    }

    return { FILE_PATHS_LINE_NUMS => \@cFilePathsLineNums, ADDR_TO_FUNC_CALL => \%addrToFuncCall, };
}

use constant {
    CL_MATCH_MODE_EXACT        => 0, # Exact match - for elf file when given an AS2-submitted CL
    CL_MATCH_MODE_FIRST_SUBSEQ => 1, # First subsequent - for elf file when given a CL matching release-branch manually-built ucode
                                     # (then crash dump OS# only gives us ToT CL# at submission time, so it won't match submitted CL),
                                     # see https://rmopengrok.lwpu.com/source/xref/chips_a/pmu_sw/prod_app/make-profile.lwmk#1053 (APP_P4_CL)
    CL_MATCH_MODE_FIRST_PRIOR  => 2, # First prior - for .c files when we know the .elf file's CL#, we need the latest version with CL <= the elf file's CL
};

sub fetchRelevantFileVersion {
    my ($filePath, $targetCL, $clMatchMode) = @_;

    my $refFilePathHash = formatPath($filePath);
    my $trimmedRelativePath = $refFilePathHash->{RELATIVE_PATH};
    $trimmedRelativePath =~ s/^\///;
    my $outputFilePath = "$opt_output_dir_path/fetched_branch_files/$trimmedRelativePath";
    mkpath(dirname($outputFilePath));

    if ($opt_cl_number) {
        my $p4Path = $refFilePathHash->{P4_PATH};
        system("$opt_p4_path filelog $p4Path > $outputFilePath.filelog.txt");

        open(FILELOG, '<:encoding(UTF-8)', "$outputFilePath.filelog.txt") or die "Couldn't open $outputFilePath.filelog.txt for read";

        my $prevRev = -1;
        my $prevCL = -1;
        my $foundRev = -1;
        my $foundCL = -1;
        while (my $line = <FILELOG>) {
            if ($line =~ /...\s+#(\d+)\s+change\s+(\d+)\s/) {
                my $rev = $1 + 0;
                my $cl = $2 + 0;

                if (($clMatchMode == CL_MATCH_MODE_EXACT) && ($cl == $targetCL)) {
                    $foundRev = $rev;
                    $foundCL = $cl;
                    last;
                } elsif (($clMatchMode == CL_MATCH_MODE_FIRST_SUBSEQ) && ($cl <= $targetCL)) {
                    $foundRev = $prevRev;
                    $foundCL = $prevCL;
                    last;
                } elsif (($clMatchMode == CL_MATCH_MODE_FIRST_PRIOR) && ($cl <= $targetCL)) {
                    $foundRev = $rev;
                    $foundCL = $cl;
                    last;
                }

                $prevRev = $rev;
                $prevCL = $cl;
            }
        }

        close(FILELOG);

        if ($foundRev == -1) {
            print "WARNING: could not find file $p4Path with target CL $targetCL and match-mode = $clMatchMode\n";
            return 0;
        }

        system("$opt_p4_path print -o $outputFilePath $p4Path#$foundRev > $outputFilePath.p4print.out.txt");

        return {
            FETCHED_FILE_PATH => $outputFilePath,
            RELATIVE_FETCHED_FILE_PATH => $refFilePathHash->{RELATIVE_PATH},
            FETCHED_AT_CL => $foundCL,
            FETCHED_AT_REV => $foundRev,
        };
    } else {
        # Don't use P4, just copy from local dir! Useful for crashes with local change!
        copy($refFilePathHash->{P4_PATH}, $outputFilePath);

        return {
            FETCHED_FILE_PATH => $outputFilePath,
            RELATIVE_FETCHED_FILE_PATH => $refFilePathHash->{RELATIVE_PATH},
            FETCHED_AT_CL => '?',
            FETCHED_AT_REV => '?',
        };
    }
}

sub isCLfromAS2 {
    my $clNum = shift;

    if (!$opt_cl_number) {
        return 0;
    }

    my $outputFile = "$opt_output_dir_path/p4_describe_cl_$clNum.txt";

    my $isFromAS2 = 0;

    system("$opt_p4_path describe -s $clNum > $outputFile");

    open(DESCRIBE_CL, '<:encoding(UTF-8)', "$outputFile") or die "Couldn't open $outputFile for read";
    if (my $line = <DESCRIBE_CL>) {
        if ($line =~ /Change\s+$clNum/) {
            $isFromAS2 = ($line =~ /\@AS2_client/);
        } else {
            die "Unrecognized format of p4 desribe for CL $clNum";
        }
    } else {
        die "Failed to read from $outputFile";
    }
    close(DESCRIBE_CL);

    return $isFromAS2;
}

# Hacky DWARF parsing. This is incredibly stupid, but seems to work
sub matchVarToReg {
    my ($elfFile, $varName, $bpAddr) = @_;

    my $infoFile = "$elfFile.dwarf_data_info.txt";
    my $locFile = "$elfFile.dwarf_data_loc.txt";
    system("$opt_riscv_tools_path/riscv64-elf-readelf --debug-dump=info $elfFile > $infoFile  2>$infoFile.err.txt");
    system("$opt_riscv_tools_path/riscv64-elf-readelf --debug-dump=loc $elfFile > $locFile    2>$locFile.err.txt");

    open(DWARF_INFO_FILE, '<:encoding(UTF-8)', "$infoFile") or die "Couldn't open $infoFile for read";
    my %locationList;
    my $activeTrackingVar = 0;
    my $lwrrVarMatchesName = 0;
    while (my $line = <DWARF_INFO_FILE>) {
        if ($line =~ /^\s*<[0-9a-fA-F]+?><[0-9a-fA-F]+?>:.*?\(DW_TAG_variable\)$/) {
            $activeTrackingVar = 1;
            $lwrrVarMatchesName = 0;
        } elsif ($activeTrackingVar && $line =~ /^\s*<[0-9a-fA-F]+?><[0-9a-fA-F]+?>:.*?$/) {
            $activeTrackingVar = 0;
            $lwrrVarMatchesName = 0;
        } elsif ($activeTrackingVar && $line =~ /DW_AT_name\s+:\s+\(indirect\s+string,\s+offset:\s+.+?\):\s+$varName$/) {
            $lwrrVarMatchesName = 1;
        } elsif ($lwrrVarMatchesName && $line =~ /DW_AT_location\s+:\s+0x([0-9a-fA-F]+)\s+\(location\s+list\)$/) {
            $locationList{lc($1)} = 1;
        }
    }
    close(DWARF_INFO_FILE);

    open(DWARF_LOC_FILE, '<:encoding(UTF-8)', "$locFile") or die "Couldn't open $locFile for read";
    my $activeTrackingLoc = 0;
    my $lwrrRegisterTracked = undef;
    while (my $line = <DWARF_LOC_FILE>) {
        if ($line =~ /^\s+0*([0-9a-fA-F]*)\s+(.*)$/) {
            my $lineData = $2;
            if ($locationList{lc($1)}) {
                if ($activeTrackingLoc) {
                    die "Locations overlap for two variables named $varName! I don't know how to parse DWARF, help!";
                }
                $activeTrackingLoc = 1;
            } elsif ($activeTrackingLoc && ($lineData eq "<End of list>")) {
                $activeTrackingLoc = 0;
            } elsif ($activeTrackingLoc && ($lineData =~ /0*([0-9a-fA-F]*)\s+0*([0-9a-fA-F]*)\s+\(DW_OP_reg.*?\((.*?)\)\)$/)) {
                my $rangeStart = hex($1);
                my $rangeEnd = hex($2);
                my $reg = $3;

                if (($rangeStart <= hex($bpAddr)) && (hex($bpAddr) < $rangeEnd)) {
                    if ($lwrrRegisterTracked) {
                        die "Two ranges match for $bpAddr for variable with name $varName! I don't know how to parse DWARF, help!";
                    }
                    $lwrrRegisterTracked = $reg;
                    last;
                }
            }
        }
    }
    close(DWARF_LOC_FILE);

    return $lwrrRegisterTracked;
}

sub collectObjdumpInfo {
    my ($objdumpFile, $bpFuncName, $haltFuncName) = @_;

    my $objdumpProps = {
        BP_FUNC_INLINED   => 1,
        HALT_FUNC_INLINED => 1, # Used for a sanity check. riscv_halt should never be inlined, as inlining it would actually increase code size no matter what you do.
                                # This whole code assumes that only breakpoints need to have inlining concerns.
        APP_VERSION_MATCH => 1,
    };

    my $appVersiolwerified = 0;

    open(OBJDUMP_FILE, '<:encoding(UTF-8)', "$objdumpFile") or die "Couldn't open $objdumpFile for read";
    while (my $line = <OBJDUMP_FILE>) {
        if ($line =~ /AppVersion:\s+(\d+)/) {
            my $appVersionCL = $1 + 0;
            if ($opt_cl_number && ($appVersionCL != $opt_cl_number)) {
                print "WARNING: was looking for build with APP_P4_CL=$opt_cl_number, but fetched files with APP_P4_CL=$appVersionCL!\n";
                $objdumpProps->{APP_VERSION_MATCH} = 0;
            }
            $appVersiolwerified = 1;
        } elsif ($line =~ /^00000000([0-9a-fA-F]+?)\s+<$bpFuncName>:$/) {
            $objdumpProps->{BP_FUNC_INLINED} = 0;
        } elsif ($line =~ /^00000000([0-9a-fA-F]+?)\s+<$haltFuncName>:$/) {
            $objdumpProps->{HALT_FUNC_INLINED} = 0;
        } elsif (($line =~ /^00000000([0-9a-fA-F]+?):\s+.*?\s+j\s+[0-9a-fA-F]+\s+<$bpFuncName>$/) && $ucodeAddrTraceListHash{lc($1)}) {
            $objdumpProps->{BP_FUNC_INLINED} = 0;
        }
    }
    close(OBJDUMP_FILE);

    if (!$appVersiolwerified) {
        die "Failed to verify AppVersion in objdump!";
    }

    if ($objdumpProps->{HALT_FUNC_INLINED}) {
        die "Something went wrong during objdump parsing, halt func is not supposed to be inlined!";
    }

    return $objdumpProps;
}

sub findSymbolInNmByAddr {
    my ($nmFile, $symbolAddr) = @_;

    my $result = "";
    open(NM_FILE, '<:encoding(UTF-8)', "$nmFile") or die "Couldn't open $nmFile for read";
    while (my $line = <NM_FILE>) {
        if ($line =~ /^00000000$symbolAddr\s.*?\s([^\s]+)$/) {
            $result = $1;
            last;
        }
    }
    close(NM_FILE);

    my @resultParts = split(/\./, $result);

    return $resultParts[0];
}

setup();

my $elfFileP4Path = formatPath($opt_ucode_elf_file)->{P4_PATH}; # kinda hacky, fix later

my $initialMatchMode = ($opt_sw_branch_path !~ /\/rel\//) && isCLfromAS2($opt_cl_number) ? CL_MATCH_MODE_EXACT : CL_MATCH_MODE_FIRST_SUBSEQ;
print "$initialMatchMode\n";

# Fetch and process the .objdump file
my $objdumpFile = $elfFileP4Path;
$objdumpFile =~ s/\.elf$/\.objdump/g;
my $fetchedObjdumpFileData;
my $objdumpProps;

while (1) {
    $fetchedObjdumpFileData = fetchRelevantFileVersion($objdumpFile, $opt_cl_number, $initialMatchMode);

    my $clMatch;

    if (!$fetchedObjdumpFileData) {
        $clMatch = 0;
    } else {
        $objdumpProps = collectObjdumpInfo($fetchedObjdumpFileData->{FETCHED_FILE_PATH}, $opt_bp_func_name, $opt_halt_func_name);
        $clMatch = $objdumpProps->{APP_VERSION_MATCH};
    }

    if ($clMatch) {
        # AppVersion matched on first try because we're so smart. Break out of loop.
        last;
    } elsif ($initialMatchMode == CL_MATCH_MODE_EXACT) {
        # If the AS2 wisdom hath failed us, try matching first_subseq (classic release-branch style).
        # This either means that someone is trying to use the fetching script for a chips_a local build's crashdump,
        # or perhaps I don't really understand how APP_P4_CL works!!
        print "WARNING: The CL $opt_cl_number was an AS2 CL, so expected exact CL matching to work, but it failed. Retrying with first-subsequent matching.\n";
        print "WARNING: note - you shouldn't be using the file-fetching script if your crash dump was produced by a local ucode build!\n";
        $initialMatchMode = CL_MATCH_MODE_FIRST_SUBSEQ;
    } else {
        die "Failed to find the right objdump file version for CL $opt_cl_number!";
    }
}

print $fetchedObjdumpFileData->{RELATIVE_FETCHED_FILE_PATH} . " ($objdumpFile#" . $fetchedObjdumpFileData->{FETCHED_AT_REV} . ")\n";

# This is used later for trace analysis, as well as
# the clever DWARF-parsing tricks to retrieve the reg for the status variable!
#
# Since halt-s are never supposed to be inlined, for them we just check the addr where riscv_halt is called for them.
#
# Also, if we have bp which is inlined, or any sort of crash that is not a halt and not a bp
# (e.g. some addressing failure), we should NOT use the ra-based addr for literally anything.
# This is because we have no guarantees that it was actually in this call-path! It could be left-over from a call
# that has already returned to where the bp got hit.
my $opt_trap_is_halt = 0; # TODO

$trapIlwokeAddr = ($opt_trap_is_bp && !$objdumpProps->{BP_FUNC_INLINED} || $opt_trap_is_halt) ? $trapIlwokeAddr : $opt_trap_addr;

# If bp function is not inlined, we use the ra-based ilwoke-address to get an initial read on the trace addr list.
# This is because theoretically the addr where the bp function was called might not even be in the trace addr list
# (i.e. in case of a badly-timed context-switch), but, since the ilwoke addr is the real bp location in that case,
# we want to discard the whole trace addr list and use the ilwoke addr instead!
if (@opt_ucode_trace_addr_list > 0) {
    my $lwrrAddr;
    do {
        $lwrrAddr = shift @opt_ucode_trace_addr_list;
    } while (($lwrrAddr ne $trapIlwokeAddr) && (@opt_ucode_trace_addr_list > 0));
}

my $fullAddrList;

if ($opt_trap_addr eq $trapIlwokeAddr) {
    $fullAddrList = [$opt_trap_addr, @opt_ucode_trace_addr_list];
} else {
    $fullAddrList = [$opt_trap_addr, $trapIlwokeAddr, @opt_ucode_trace_addr_list];
}

# Fetch the .elf file, and establish the CL for the binary submission we're working with
my $fetchedElfFileData = fetchRelevantFileVersion($opt_ucode_elf_file, $fetchedObjdumpFileData->{FETCHED_AT_CL}, CL_MATCH_MODE_EXACT);
print $fetchedElfFileData->{RELATIVE_FETCHED_FILE_PATH} . " (" . $elfFileP4Path . "#" . $fetchedElfFileData->{FETCHED_AT_REV} . ")\n";


# Fetch the .nm file
my $nmFile = $elfFileP4Path;
$nmFile =~ s/\.elf$/\.nm/g;
my $fetchedNmFileData = fetchRelevantFileVersion($nmFile, $fetchedObjdumpFileData->{FETCHED_AT_CL}, CL_MATCH_MODE_EXACT);
print $fetchedNmFileData->{RELATIVE_FETCHED_FILE_PATH} . " ($nmFile#" . $fetchedNmFileData->{FETCHED_AT_REV} . ")\n";

# Fetch additional files if requested
for my $additionalFile (@opt_additional_files) {
    my $additionalFileP4Path = formatPath($additionalFile)->{P4_PATH};
    # Assume CL_MATCH_MODE_FIRST_PRIOR matching required for additional files. This might be made configurable later.
    my $fetchedAdditionalFileData = fetchRelevantFileVersion($additionalFile, $fetchedObjdumpFileData->{FETCHED_AT_CL}, CL_MATCH_MODE_FIRST_PRIOR);
    print $fetchedAdditionalFileData->{RELATIVE_FETCHED_FILE_PATH} . " ($additionalFileP4Path#" . $fetchedAdditionalFileData->{FETCHED_AT_REV} . ")\n";
}

print "\n";

# Fetch the code file paths (and line numbers) from the .elf file by using addr2line
my $cFilePathsData = getCFilePaths($fetchedElfFileData->{FETCHED_FILE_PATH}, $fetchedObjdumpFileData->{FETCHED_FILE_PATH}, $fullAddrList);
my @cFilePathsLineNums = @{$cFilePathsData->{FILE_PATHS_LINE_NUMS}};

if ($opt_trap_is_bp && $cFilePathsLineNums[0]->{FUNC_NAME} ne $opt_bp_func_name) {
    die "Expected breakpoint function $opt_bp_func_name, but found " . $cFilePathsLineNums[0]->{FUNC_NAME} . " instead!";
}

my $lastFuncName = undef;
my $lastUcodeAddr = "";

for my $cFilePathLineNum (@cFilePathsLineNums) {
    if ($opt_output_ignore_context_switches && $lastFuncName &&
        ($lastUcodeAddr ne $cFilePathLineNum->{UCODE_ADDR}) && # TODO: it's kinda silly that we need to check that, maybe use list of entry-lists per each ucode addr instead?
        ((!exists $cFilePathsData->{ADDR_TO_FUNC_CALL}->{lc $cFilePathLineNum->{UCODE_ADDR}}) ||
         ((exists $cFilePathsData->{ADDR_TO_FUNC_CALL}->{lc $cFilePathLineNum->{UCODE_ADDR}}) &&
          ($cFilePathsData->{ADDR_TO_FUNC_CALL}->{lc $cFilePathLineNum->{UCODE_ADDR}} ne $lastFuncName)))) {
        #print $cFilePathLineNum->{UCODE_ADDR} . ": call to " . $cFilePathsData->{ADDR_TO_FUNC_CALL}->{lc $cFilePathLineNum->{UCODE_ADDR}} . "\n";
        next; # get an exact sequence of function calls, ignoring inconsistencies that might be caused by context switches mixed into the list
    }

    my $fetchedCFileData = fetchRelevantFileVersion($cFilePathLineNum->{P4_FILE_PATH}, $fetchedObjdumpFileData->{FETCHED_AT_CL}, CL_MATCH_MODE_FIRST_PRIOR);
    $lastFuncName = $cFilePathLineNum->{FUNC_NAME};
    $lastUcodeAddr = $cFilePathLineNum->{UCODE_ADDR};
    print $fetchedCFileData->{RELATIVE_FETCHED_FILE_PATH} . ":" . $cFilePathLineNum->{LINE_NUM} . " (" . $cFilePathLineNum->{P4_FILE_PATH} . "#" . $fetchedCFileData->{FETCHED_AT_REV} . ") in " . $cFilePathLineNum->{FUNC_NAME} . "\n";
}

# Grab a symbol name from the .nm file - with PMU we can use this to retrieve the task name from the PVT TCB
my $pvtSymbolName = "<no-data>";
if ($opt_pvt_symbol_addr) {
    $pvtSymbolName = findSymbolInNmByAddr($fetchedNmFileData->{FETCHED_FILE_PATH}, $opt_pvt_symbol_addr);
    if (!$pvtSymbolName) {
        die "Wrong .nm file fetched! Make sure you're on the right branch and make sure you're not using data from a local ucode build!";
    }
    print "pvt_symbol:$pvtSymbolName\n";
}

if ($opt_trap_is_bp && $opt_fetch_reg_for_var) {
    # Use my hacky DWARF "parser" to try and get the register for the requested variable (usually a status variable) if we're dealing with a breakpoint
    # We can't do this check for halts, because halt is a syscall, so the register with the error code may get overridden with the syscall code.
    my $foundReg = matchVarToReg($fetchedElfFileData->{FETCHED_FILE_PATH}, $opt_fetch_reg_for_var, $trapIlwokeAddr);
    print "$opt_fetch_reg_for_var:" . ($foundReg ? $foundReg : "<not-found>") . "\n";
}
