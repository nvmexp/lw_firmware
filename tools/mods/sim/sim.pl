#!/usr/bin/elw perl

#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#-------------------------------------------------------------------------------
# 45678901234567890123456789012345678901234567890123456789012345678901234567890

use strict;
use FindBin;
use File::Basename;

use POSIX qw(WNOHANG);

my ($isWindows, $isCygwin, $debug, $gdb, $ddd, $gdbserver, $windbg, $dbg, $dump, $chip, $modsOpt, $model, $wine, $sockserv, $sockCmd, $sockSessionfile, $fcra, $fcraDebugLevel, $testMetadata, $valgrind);
my ($modsProg, $chipLib, $actualChipLibName, $rootDir, $chipDir, $dispatchProg, $isRmTest);
my ($biosAbsPath, %biosFiles, $clntFile, $simvIdx, $i, @simvType, @simvFile, $dispatchFile, @simvFlags, @extraSimvFlags, $args);
my ($cmdline, $scriptPath, $modsPath, $mtime, @dirs, $libDir, $fullChipLib, $testname, $chipArgsString, $gdbserverHostPort);
my ($hostname, $dmpName, $chipTree, $useNewAmodelName, $lwPerfsim, $perfsim, $wrapper, $commandString);
my ($hwbios, $dumpcmd, $verbose, $errMissingNodispVbios);
my ($timeOutDelaySecConnect);
my ($host, $user, $password);
my ($processRawLog);
my $isSOC = 0;
my $socSize = 32;
my $fmod64bit = 0;
my $remote = 0; # $remote default to be 0 for mods logger

my $noops = 0;  # when set (-n, like p4's -n), sim.pl should do nothing, just print out what it *would* do.

$simvIdx = 0;
$simvType[$simvIdx] = "gpu"; # default
$simvFlags[$simvIdx] = "";
$clntFile = "two_rtl.txt";
$verbose = "";
$timeOutDelaySecConnect = "";
$isCygwin = 0;

# Capture Die signal and reported by ModsLogger
BEGIN{ $SIG{__DIE__} = sub {
        my $msg = shift;
        my $simPerlExitNormally = 0;
        my $modsLoggerCmd = "$FindBin::Bin/modslogger.pl --simPerlExitNormally $simPerlExitNormally --msg \'$msg\'";
        system($modsLoggerCmd) == 0 or warn "MODS Logger: system $modsLoggerCmd failed: $?";
    }; }

# Detect Windows vs. Unix
if ($^O eq "MSWin32") {
    $isWindows = 1;
} elsif ($^O eq "cygwin") {
    $isWindows = 1;
    $isCygwin = 1;
} elsif ($^O eq "linux") {
    $isWindows = 0;
} else {
    die "Could not identify operating system";
}

# Detect directory of this script
$scriptPath = $FindBin::Bin;
$modsPath = $scriptPath;

# The script path and the MODS path point to the same directory, but the path
# name style may be different.  In particular, when running on cygwin, any
# path that is used by a perl script or shell script needs to have the form
# /cygdrive/<drive letter>/<path to MODS dir> and any path that will be
# passed to MODS via command-line argument needs to have the form
# <drive letter>:\<path to MODS dir>.
if ($isCygwin)
{
    $modsPath = `cygpath -w $scriptPath`;
    chomp $modsPath;
    $modsPath =~ s/\\/\//g;
}

# Default arguments
$debug = 0;
$gdb = 0;
$ddd = 0;
$gdbserver = 0;
$windbg = 0;
$dbg = 0;
$dump = 0;
$wine = 0;
$testname = 0;
$modsOpt = "-s";
$dmpName = "./lwbus.fsdb";
$useNewAmodelName = 0;
$perfsim=0;
$lwPerfsim=0;
$hwbios=0;
$dumpcmd = 0;
$errMissingNodispVbios = 0;
$isRmTest = 0;
$processRawLog = 0;
$fcra = undef;
$fcraDebugLevel = undef;
$testMetadata = undef;
$valgrind = 0;

my $timeout = 0;

# No arguments?  Tell the user where to find instructions
if ($#ARGV < 0) {
    die "Instructions for using sim.pl can be found at https://wiki.lwpu.com/engwiki/index.php/MODS/GPU_Verification/sim.pl.  Died";  # FIXME: I think this link is invalid - sgottschalk, Jan 2010
}

#Check if the commandline contains rmtest.js. This check is useful while running tests on t124 linsim
#This is used for setting the variable $isRmTest which is later used to determine whether we should
#append some arguments (which are specific to t124) or not.
$commandString = join " ", @ARGV;
if ($commandString =~ m/rmtest\.js/) {
    $isRmTest = 1;
}

# Check if the a raw log will be generated.
# If yes, set the option to post-process the log.
if ($commandString =~ m/-dump_raw_chiplibtrace/) {
    $processRawLog = 1;
}

# Check if FCRA is enabled by ELW
if (exists $ELW{FCRA})
{
    ($fcra, $fcraDebugLevel) = split(/[,:\s]+/, $ELW{FCRA});
}

# Try to protect users from weird out-of-memory crashes
# Disabled for now, after the transition from l-build to l-xterm.  If problems
# arise, I'll add it back.
#$hostname = `hostname`;
#if ($hostname =~ /l-xterm-/) {
#    die "You cannot run on a build machine -- please use qsub to submit to a farm machine";
#}

# Parse arguments
while ($#ARGV >= 0) {
    if ($ARGV[0] eq "-amod") {
        $model = "amod";
    } elsif ($ARGV[0] eq "-socSize") {
        shift(@ARGV);
        $socSize = $ARGV[0]; # verify it's a number?
    } elsif ($ARGV[0] eq "-useNewAmodelName") {
        $useNewAmodelName = 1;
    } elsif ($ARGV[0] eq "-n") {
        $noops = 1;  # turn on "no operations" mode - do nothing, just print MODS command line
    } elsif ($ARGV[0] eq "-perfsim") {
        $perfsim = 1;
    } elsif ($ARGV[0] eq "-lwPerfsim") {
        $lwPerfsim = 1;
    } elsif ($ARGV[0] eq "-cmod") {
        $model = "cmod";
    } elsif ($ARGV[0] eq "-fmod") {
        $model = "fmod";
    } elsif ($ARGV[0] eq "-multifmod") {
        $model = "multifmod";
    } elsif ($ARGV[0] eq "-fmod64") {
        $model = "fmod";
        $fmod64bit = 1;
    } elsif ($ARGV[0] eq "-vmod") {
        $model = "vmod";
    } elsif ($ARGV[0] eq "-hwchip") {
        $model = "hwchip";
    } elsif ($ARGV[0] eq "-hwchip2") {
        $model = "hwchip";
        $chipLib = "hwchip2";
    } elsif ($ARGV[0] eq "-peatrans") {
        $model = "hwchip";
        $chipLib = "peatrans";
    } elsif ($ARGV[0] eq "-sockserv") {
        # sockserv functionality not for windows
        if ($isWindows) {
            die "sockserv functionality support not available on Windows";
        }
        shift(@ARGV);
        $sockserv = $ARGV[0];
    } elsif ($ARGV[0] eq "-sessionfile") {
        shift(@ARGV);
        $sockSessionfile = $ARGV[0];
    } elsif ($ARGV[0] eq "-chip") {
        shift(@ARGV);
        $chip = $ARGV[0];
    } elsif ($ARGV[0] eq "-chipLib") {
        shift(@ARGV);
        $chipLib = $ARGV[0];
    } elsif ($ARGV[0] eq "-bios") {
        shift(@ARGV);
        $biosFiles{0} = $ARGV[0];
    } elsif ($ARGV[0] eq "-gpubios") {
        $biosFiles{$ARGV[1]} = $ARGV[2];
        shift(@ARGV);
        shift(@ARGV);
    } elsif ($ARGV[0] eq "-debug") {
        $debug = 1;
    } elsif ($ARGV[0] eq "-gdb") {
        $gdb = 1;
    } elsif ($ARGV[0] eq "-ddd") {
        $ddd = 1;
    } elsif ($ARGV[0] eq "-gdbserver") {
        shift(@ARGV);
        $gdbserver = 1;
        $gdbserverHostPort = $ARGV[0];
    } elsif ($ARGV[0] eq "-windbg") {
        $windbg = 1;
    } elsif ($ARGV[0] eq "-dbg") {
        $dbg = 1;
    } elsif ($ARGV[0] eq "-dump") {
        $dump = 1;
    } elsif ($ARGV[0] eq "-modtime") {
        $mtime = 1;
    } elsif ($ARGV[0] eq "-testname") {
        shift(@ARGV);
        $testname = $ARGV[0];
    } elsif ($ARGV[0] eq "-timeout") {
        shift(@ARGV);
        $timeout = $ARGV[0];
    } elsif ($ARGV[0] eq "-dmpName") {
        shift(@ARGV);
        $dmpName = $ARGV[0];
    } elsif ($ARGV[0] eq "-wine") {
        $wine = 1;
    } elsif ($ARGV[0] eq "-pc") {
        $isWindows = 1;
    } elsif ($ARGV[0] eq "-wrapper") {
        shift(@ARGV);
        $wrapper = $ARGV[0];
    } elsif (substr($ARGV[0],0,1) eq "+") {
        $extraSimvFlags[0] .= " " . $ARGV[0];
    } elsif ($ARGV[0] eq "-chipargs") {
        # Hack to get MODS to run with production arguments
        shift(@ARGV);
    } elsif ($ARGV[0] eq "-verbose") {
        $verbose = "-verbose"; # pass to next level
    } elsif ($ARGV[0] eq "-startVerilogOpts") {
        shift(@ARGV);
        $simvIdx = $ARGV[0];
    } elsif ($ARGV[0] eq "-simvType") {
        shift(@ARGV);
        $simvType[$simvIdx] = $ARGV[0];
    } elsif ($ARGV[0] eq "-verilogFile") {
        shift(@ARGV);
        $simvFile[$simvIdx] = $ARGV[0];
    } elsif ($ARGV[0] eq "-verilogOptions") {
        shift(@ARGV);
        $simvFlags[$simvIdx] = $ARGV[0];
    } elsif ($ARGV[0] eq "-verilogOptionsMore") {
        shift(@ARGV);
        $extraSimvFlags[$simvIdx] .= " " . $ARGV[0];
    } elsif ($ARGV[0] eq "-timeOutDelaySecConnect") {
        shift(@ARGV);
        $timeOutDelaySecConnect .= "-timeOutDelaySecConnect " . $ARGV[0];
    } elsif ($ARGV[0] eq "-chipTree") {
        shift(@ARGV);
        $chipTree = $ARGV[0];
    } elsif ($ARGV[0] eq "-dispatchProg") {
        shift(@ARGV);
        $dispatchProg = $ARGV[0];
    } elsif ($ARGV[0] eq "-hwbios") {
        $hwbios = 1;
    } elsif ($ARGV[0] eq "-nohwbios") {
        $hwbios = 0;
    } elsif ($ARGV[0] eq "-dumpCmdLine") {
        $dumpcmd = 1;
    } elsif ($ARGV[0] eq "-errMissingNodispVbios") {
        $errMissingNodispVbios = 1;
    } elsif ($ARGV[0] eq "-remote") {
        $remote = 1;
    } elsif ($ARGV[0] eq "-host") {
        shift(@ARGV);
        $host = $ARGV[0];
    } elsif ($ARGV[0] eq "-user") {
        shift(@ARGV);
        $user = $ARGV[0];
    } elsif ($ARGV[0] eq "-password") {
        shift(@ARGV);
        $password = $ARGV[0];
    } elsif ($ARGV[0] eq "-fcra") {
        shift(@ARGV);
        $fcra = "/home/lw/utils/fcra/latest";
        $fcraDebugLevel = $ARGV[0];
    } elsif ($ARGV[0] eq "-fcra_dev") {
        shift(@ARGV);
        $fcra = $ARGV[0];
        shift(@ARGV);
        $fcraDebugLevel = $ARGV[0];
    } elsif ($ARGV[0] eq "-test_metadata") {
        shift(@ARGV);
        $testMetadata = $ARGV[0];
    } elsif ($ARGV[0] eq "-valgrind") {
        $valgrind = 1;
    } else {
        if ($ARGV[0] eq "-modsOpt") {
            shift(@ARGV);
        } else {
            print "Argument $ARGV[0] not recognized, passing remaining arguments to MODS\n";
        }
        $modsOpt = '"';
        my $IsSmcPartitioning = 0;
        my $pmuInstlocInstSpecified = 0;
        while ($#ARGV >= 0) {
            if($ARGV[0] eq "-bios") {
                shift(@ARGV);
                $biosFiles{0} = $ARGV[0];
            } elsif ($ARGV[0] eq "-gpubios") {
                $biosFiles{$ARGV[1]} = $ARGV[2];
                shift(@ARGV);
                shift(@ARGV);
            }
            else{
                if ($ARGV[0] eq "-smc_partitioning" ||
                    $ARGV[0] eq "-smc_partitioning_sys0_only" ||
                    $ARGV[0] eq "-utl_script" ||
                    $ARGV[0] eq "-smc_mem") {
                    $IsSmcPartitioning = 1;
                }
                if ($ARGV[0] eq "-pmu_instloc_inst") {
                    $pmuInstlocInstSpecified = 1;
                }
                if(length ($modsOpt) > 1){
                   $modsOpt .= '" "'.$ARGV[0];
                }
                else{
                   $modsOpt .= $ARGV[0];
                }
            }
            shift(@ARGV);
        }
        if ($IsSmcPartitioning == 1) {
            print "Note: If user specified -smc_partitioning or -smc_mem, Mods will append -no_preallocated_userd and -pmu_instloc_inst coh to affect userd and pmu allocation.\n";
            $modsOpt .= '" "' . "-smc_vgpu_policy" . '" "' . "2";
            $modsOpt .= '" "' . "-no_preallocated_userd";
            if ($pmuInstlocInstSpecified == 0) {
                $modsOpt .= '" "' . "-pmu_instloc_inst";
                $modsOpt .= '" "' . "coh";
            }
        }

        if ($ELW{"SRIOV_GFID"} != 0 and defined $ELW{"SRIOV_SMC_SYS"}) {
            print "\nsmc_vgpu_policy\n";
            $modsOpt .= '" "' . "-smc_vgpu_policy" . '" "' . "2";
        }
        $modsOpt .= '"';
        last;
    }
    shift(@ARGV);
}

$modsOpt =~ s/^""\s+"/"/;

# default enable simclk except on emulation/silicon
if ($model ne 'hwchip' and $modsOpt !~ /simclk/){
    $modsOpt =~ s/"mdiag.js"/"mdiag.js" "-simclk" "-simclk_mode" "gild"/;
}

for ($i=0; $i < scalar(@simvType); $i++)
{
  die "simv type must be either gpu or mcp, specify -simvType\n" unless
      (($simvType[$i] eq "gpu") or ($simvType[$i] eq "mcp"));
}

die "No simulator model type specified, e.g. '-cmod'" unless defined $model;
die "No chip specified, e.g., '-chip lw40'" unless defined $chip;

if ($wine) {
    $isWindows = 1;
}

# Options unsupported on Windows
if ($isWindows) {
    if ($model eq "vmod") {
        die "RTL not supported on Windows";
    }
    if ($model eq "hwchip") {
        die "Hwchip/Emulation not supported on Windows";
    }
    if ($gdb) {
        die "Running under GDB not supported on Windows; use MSVC instead";
    }
    if ($gdbserver) {
        die "Running under GDB server not supported on Windows; use MSVC instead";
    }
    if ($ddd) {
        die "Running under DDD not supported on Windows; use MSVC instead";
    }
    if ($valgrind) {
        die "Running under VALGRIND not supported on Windows";
    }
} else {
    if ($dbg) {
        die "Running under VS not supported on non-Windows platform; Use -gdb instead";
    }
    if ($windbg) {
        die "Running under windbg not supported on non-Windows; use -gdb instead";
    }
}

if ($valgrind && ($gdb || $gdbserver || $ddd)) {
    print "Warning: -gdb/-gdbserver/-ddd will be ignored when running under VALGRIND\n";
    $gdb = 0;
    $gdbserver = 0;
    $ddd = 0;
}

print "LD: " . $ELW{LD_LIBRARY_PATH} . "\n";

# MODS exelwtable name depends on OS
# MODS exelwtable is expected to live in the same path as sim.pl
$modsProg = $isWindows ? "mods.exe" : "mods";
$modsProg = "$scriptPath/$modsProg";

if (!$isWindows) {
    # Fix LD_LIBRARY_PATH to include the sim.pl path
    # Note that changes to ELW do not persist to the shell that ilwoked this script
    $ELW{LD_LIBRARY_PATH} .= ":$scriptPath";
}

# Get amodel args from a central source since multiple different scripts build the amodel cmdline.
# The complexity here is reproduce old behavior (probably not needed anymore).
my $modsAmodArgs = "";
if ($model eq "amod") {
    my $amodBasename = "lw_amodel";
    if (!$useNewAmodelName) {
        if ($lwPerfsim) {
            $amodBasename = "lw_perfsim";
        } elsif ($perfsim) {
            $amodBasename = "fermi_perfsim";
        } elsif ($debug) {
            $amodBasename = "${chip}_debug_amodel";
        } else {
            $amodBasename = "${chip}_amodel";
        }
    }
    my $ext = ".so";
    if ($isWindows) {
        $ext = ".dll";
    }
    $modsAmodArgs = `$scriptPath/sim_amodel_args.pl -basename $amodBasename -ext $ext`;
    if ($chipLib) {
        $modsAmodArgs = `$scriptPath/sim_amodel_args.pl -nochip -basename $amodBasename -ext $ext`;
        $modsAmodArgs .= " -chip $chipLib$ext";
    }

    # use the bios option returned by sim_amodel_args.pl
    # eventually this could be removed, and use the same mechanism as other chips
    $hwbios = 1;

    # bug 1536534, enable sockchip for amod, it can help to run Valgrind on sockserver
    $chipLib = $amodBasename if $sockserv;

    # Needed by direct amodel runs:
    $ELW{'OGL_ChipName'} = $chip;
}

# Decide if this is a soc based on chip name
if ($chip =~ /^t[1-9][0-9][0-9]/) {
   $isSOC = 1;
}

if ($isSOC and (($model eq "amod") or ($model eq "multifmod") or ($model eq "cmod"))) {
    die "SOC only supported for fmodel, hwchip, and vmod\n";
}


# Construct chiplib name, e.g., lw40_debug_vmodel.so
# Set chipLib if not passed to us directly.
unless($chipLib)
{
    if ($model eq "hwchip") {
        $chipLib = "hwchip";
    } else {
        $actualChipLibName = $chip;

        # Override the default chiplib name for fmod case for gf11(0|8)
        if (($model eq "fmod") or $isSOC) {
            if ($chip eq "gf117") {
                $actualChipLibName = "gf11x";
            }
            elsif ($chip eq "gf119") {
                $actualChipLibName = "gflit1";
            }
            elsif (($chip eq "gk104") || ($chip eq "gk106") || ($chip eq "gk107")) {
                $actualChipLibName = "gklit1";
            }
            elsif (($chip eq "gk110") || ($chip eq "gk110b") || ($chip eq "gk110c")) {
                $actualChipLibName = "gklit2";
            }
            elsif ($chip eq "gk20a") {
                $actualChipLibName = "gklit3";
            }
            elsif (($chip eq "gk207") || ($chip eq "gk208")) {
                $actualChipLibName = "gklit4";
            }
            elsif (($chip eq "gm108") || ($chip eq "gm107")) {
                $actualChipLibName = "gmlit1";
            }
            elsif (($chip eq "gm200") || ($chip eq "gm204") || ($chip eq "gm206")) {
                 $actualChipLibName = "gmlit2";
            }
            elsif (($chip eq "gm20b") || ($chip eq "gm21b")) {
                $actualChipLibName = "gmlit3";
            }
            elsif (($chip eq "gp100") || ($chip eq "gp000")) {
                $actualChipLibName = "gmlit4";
            }
            elsif (($chip eq "gp102") || ($chip eq "gp104") || ($chip eq "gp105") || ($chip eq "gp106")) {
                $actualChipLibName = "gplit3";
            }
            elsif (($chip eq "gp107") || ($chip eq "gp108")) {
                $actualChipLibName = "gplit4";
            }
            elsif ($chip eq "gm20d") {
                $actualChipLibName = "gmlit6";
            }
            elsif ($chip eq "gp10b") {
                $actualChipLibName = "gplit1";
            }
            elsif ($chip eq "gp10d") {
                $actualChipLibName = "gplit6";
            }
            elsif ($chip eq "gp10e") {
                $actualChipLibName = "gplit7";
            }
            elsif (($chip eq "gv100") || ($chip eq "gv105")) {
                $actualChipLibName = "gvlit1";
            }
            elsif (($chip eq "gv11b")) {
                $actualChipLibName = "gvlit5";
            }
            elsif (($chip eq "tu102")) {
                $actualChipLibName = "tulit2";
            }
            elsif (($chip eq "tu104")) {
                $actualChipLibName = "tulit2";
            }
            elsif (($chip eq "tu106")) {
                $actualChipLibName = "tulit2";
            }
            elsif (($chip eq "tu116")) {
                $actualChipLibName = "tulit3";
            }
            elsif (($chip eq "tu117")) {
                $actualChipLibName = "tulit3";
            }
            elsif (($chip eq "ga100")) {
                $actualChipLibName = "galit1";
            }
            elsif (($chip eq "ga102") || ($chip eq "ga103") || ($chip eq "ga104") || ($chip eq "ga106") || ($chip eq "ga107")) {
                $actualChipLibName = "galit2";
            }
            elsif (($chip eq "ga10b")) {
                $actualChipLibName = "galit5";
            }
            elsif (($chip eq "ga10f")) {
                $actualChipLibName = "galit6";
            }
            elsif (($chip eq "ga11b")) {
                $actualChipLibName = "galit7";
            }
            elsif (($chip eq "ad102") || ($chip eq "ad103") || ($chip eq "ad104") || ($chip eq "ad106") || ($chip eq "ad107")) {
                $actualChipLibName = "adlit1";
            }
            elsif (($chip eq "ad10b")) {
                $actualChipLibName = "adlit5";
            }
            elsif (($chip eq "gh100")) {
                $actualChipLibName = "ghlit1";
            }
            elsif (($chip eq "gh202")) {
                $actualChipLibName = "ghlit3";
            }
            elsif (($chip eq "gb100") || ($chip eq "gb102")) {
                $actualChipLibName = "gblit1";
            }
            elsif (($chip eq "g000")) {
                $actualChipLibName = "glit0";
            }
            elsif (($chip eq "ig000")) {
                $actualChipLibName = "glit1";
            }
            elsif ($isSOC) {
                $actualChipLibName = "lib${chip}_chiplib2";
            }
        }

        if(!$isSOC) {
            if ($debug) {
                $chipLib = "${actualChipLibName}_debug_${model}el";
            } else {
                $chipLib = "${actualChipLibName}_${model}el";
            }
        } else {
            $chipLib = "${actualChipLibName}";
        }

        if ($fmod64bit) {
            $chipLib = "${chipLib}_64";
        }

    }
}
if ($isWindows) {
    $chipLib = "${chipLib}.dll";
} else {
    $chipLib = "${chipLib}.so";
}

# For now, some chips need -chipargs -chipPOR=<chip> argument @ the end.
# So add this argument if not already present for fmodel and rtl
if ((($chip eq "gf117") || ($chip eq "gf119") ||
     ($chip eq "gk104") || ($chip eq "gk106") || ($chip eq "gk107") ||
     ($chip eq "gk110") || ($chip eq "gk110b") || ($chip eq "gk110c") || ($chip eq "gk20a") ||
     ($chip eq "gm20b") || ($chip eq "gm21b") || ($chip eq "gm20d") ||
     ($chip eq "gk207") || ($chip eq "gk208") ||
     ($chip eq "gm108") || ($chip eq "gm107") ||
     ($chip eq "gm200") || ($chip eq "gm204") || ($chip eq "gm206") ||
     ($chip eq "gp000") || ($chip eq "gp100") ||
     ($chip eq "gp102") || ($chip eq "gp104") || ($chip eq "gp105") || ($chip eq "gp106") ||
     ($chip eq "gp107") || ($chip eq "gp108") ||
     ($chip eq "gp10b") || ($chip eq "gp10d") || ($chip eq "gp10e") ||
     ($chip eq "gv100") || ($chip eq "gv105") || ($chip eq "gv11b") ||
     ($chip eq "tu102") || ($chip eq "tu104") || ($chip eq "tu106") || ($chip eq "tu116") || ($chip eq "tu117") ||
     ($chip eq "ga100") || ($chip eq "ga102") || ($chip eq "ga103") || ($chip eq "ga104") || ($chip eq "ga106") || ($chip eq "ga107") || ($chip eq "ga10b") || ($chip eq "ga10f") || ($chip eq "ga11b") ||
     ($chip eq "ad102") || ($chip eq "ad103") || ($chip eq "ad104") || ($chip eq "ad106") || ($chip eq "ad107") || ($chip eq "ad10b") ||
     ($chip eq "gh100") || ($chip eq "gh202") ||
     ($chip eq "gb100") || ($chip eq "gb102") ||
     ($chip eq "g000") || ($chip eq "ig000")) &&
      (($model eq "fmod") || ($model eq "vmod")) &&
      ($modsOpt !~ "-chipPOR")) {
    $modsOpt = "${modsOpt}"." \"-chipargs\" \"-chipPOR="."${chip}\"";
}

if (($chip eq "t124") and ($model eq "fmod") and $isRmTest) {
    #We need to pass the following arguments too for running rmtests on linsim for T124
    $chipArgsString  = " -chipargs \" -l -f -linsim_memstore 1 -linsim_mem_lib libtegra_mem_t124.so";
    $chipArgsString .= " -c sockfsim.cfg -libarm cmod  --  --systype Linux disable_gpu_sim=0";
    $chipArgsString .= " ltc_mem_hooks=1  gpu_ld_lib_path=\$GPU_LD_LIB_PATH -- \" ";
    $modsOpt .= $chipArgsString;
}

if (($chip eq "t210") and ($model eq "fmod") and $isRmTest) {
    #We need to pass the following arguments too for running rmtests on linsim for T210
    $chipArgsString  = " -chipargs \" -l -f -linsim_memstore 1 -linsim_mem_lib libtegra_mem_t210.so";
    $chipArgsString .= " -c sockfsim.cfg -libarm cmod  --  --systype Linux disable_gpu_sim=0";
    $chipArgsString .= " ltc_mem_hooks=1  gpu_ld_lib_path=\$GPU_LD_LIB_PATH -- \" ";
    $modsOpt .= $chipArgsString;
}

if (($model eq "vmod") and !$isSOC) {
    $rootDir = $ELW{P4ROOT};
    if (!$chipTree) {
        if ($chip eq "lw50" || $chip eq "lw50vaio1") {
            $chipTree = "lw5x";
        } elsif (($chip eq "lw46") || ($chip eq "lw47")) {
            $chipTree = "gpu";
        } else {
            $chipTree = $chip;
        }
    }
    $chipDir = "$rootDir/hw/$chipTree";
    if (!$dispatchProg) {
        if (scalar(@simvType) > 1)
        {
           $dispatchProg = "$chipDir/bin/dispatch.pl";
        }
        else
        {
           $dispatchProg = "$chipDir/bin/dispatch";
        }
    }
    if (!$simvFile[0]) {
        die "garbled options: no simv specified for gpu 0\n" unless (scalar(@simvType) == 1);
        $simvFile[0] = "$chipDir/diag/tests/$chip" . ($debug ? "_debug" : "" ) . "/sys";
    }
    $dispatchFile = "dispatch.testlist";

    # The default RTL simulation arguments
    for ($i=0; $i < scalar(@simvType); $i++)
    {
       if (!$simvFlags[$i]) {
           if ($simvType[$i] eq "gpu")
           {
              $simvFlags[$i] .= "+NoHostCMdlOk +FOREVER +SHORT_XP_PL_TIMER";
           }
       }
       if ($dump) {
           # .fsdb format is now the default for waveforms
           #$simvFlags[0] .= " +DUMP +vpdfile+dump.vpd +vcdplus";
           if ($simvType[$i] eq "gpu")
           {
              $simvFlags[$i] .= " +dmpName=$dmpName +DUMP +dump_dby";
           }
           elsif ($simvType[$i] eq "mcp")
           {
              $simvFlags[$i] .= " +dmpfile=$dmpName +dump_dby";
           }
       } else {
           if ($simvType[$i] eq "gpu")
           {
              $simvFlags[$i] .= " +NODUMP";
           }
       }
       $simvFlags[$i] .= $extraSimvFlags[$i];
       # Always wait for a license rather than failing.  Do not allow anyone to
       # override this!
       $simvFlags[$i] .= " +vcs+lic+wait";
    }

} else {
    die "Only RTL can dump waveforms" if ($dump and !$isSOC);
}

if (!%biosFiles && $ELW{"SRIOV_GFID"} == 0) {
    # Each chip has after fermi must be listed in a chip to
    # filename translation table
    my %biosTranslation = (
    #   chip name       VBIOS name
        "lw50vaio1"  => "lw50n.rom",
        "gf100"      => "gf100n.rom",
        "gf100b"     => "gf100bn.rom",
        "gf10x"      => "gf100n.rom",
        "gf104"      => "gf104_sim_gddr4.rom",
        "gf104b"     => "gf104b_sim_gddr4.rom",
        "gf105"      => "gf105_sim_gddr4.rom",
        "gf106"      => "gf106_sim_gddr4.rom",
        "gf106b"     => "gf106b_sim_gddr4.rom",
        "gf108"      => "gf108_sim_gddr4.rom",
        "gf112"      => "gf112_sim_gddr4.rom",
        "gf116"      => "gf116_sim_gddr4.rom",
        "gf117"      => "gf117_sim_gddr5.rom",
        "gf119"      => "gf119_sim_gddr4.rom",
        "gk104"      => "gk104_sim_gddr5.rom",
        "gk106"      => "gk106_sim_gddr5.rom",
        "gk107"      => "gk107_sim_gddr5.rom",
        "gk110"      => "gk110_sim_gddr5.rom",
        "gk20a"      => "gk20a_sim_sddr3.rom",
        "t124"       => "t124_sim_sddr3.rom",
        "t210"       => "t210_sim_sddr3.rom",
        "gk207"      => "gk207_sim_gddr5.rom",
        "gk208"      => "gk208_sim_gddr5.rom",
        "gm108"      => "gm108_sim_gddr5-coproc_nodisp.rom",
        "gm107"      => "gm107_sim_gddr5.rom",
        "gm200"      => "gm200_sim_gddr5.rom",
        "gm204"      => "gm204_sim_gddr5.rom",
        "gm206"      => "gm206_sim_gddr5.rom",
        "gm20b"      => "gm20b_sim_gddr3_coproc_nodisp.rom",
        "gm21b"      => "gm20b_sim_gddr3_coproc_nodisp.rom",
        "gm20d"      => "gm20d_sim_gddr3_coproc_nodisp.rom",
        "gp000"      => "gm000_sim_gddr5.rom",
        "gp100"      => "gp100_sim_hbm.rom",
        "gp102"      => "gp102_sim_gddr5.rom",
        "gp104"      => "gp104_sim_gddr5.rom",
        "gp105"      => "gp104_sim_gddr5.rom",
        "gp106"      => "gp106_sim_gddr5.rom",
        "gp107"      => "gp107_sim_gddr5.rom",
        "gp108"      => "gp108_sim_gddr5.rom",
        "gp10b"      => "gp10b_sim_sddr3.rom",
        "gp10d"      => "gp10b_sim_sddr3.rom",
        "gp10e"      => "gp10e_sim_sddr3.rom",
        "gv100"      => "gv100_sim_hbm.rom",
        "gv105"      => "gv100_sim_hbm.rom",
        "gv11b"      => "gv100_sim_hbm_nodisplay.rom",
        "tu102"      => "tu102_sim_gddr6.rom",
        "tu104"      => "tu104_sim_gddr6.rom",
        "tu106"      => "tu106_sim_gddr6.rom",
        "tu116"      => "tu116_sim_gddr6.rom",
        "tu117"      => "tu117_sim_gddr6.rom",
        "ga100"      => "ga100_sim_hbm.rom",
        "ga102"      => "ga102_sim_gddr6.rom",
        "ga103"      => "ga103_sim_gddr6.rom",
        "ga104"      => "ga104_sim_gddr6.rom",
        "ga106"      => "ga106_sim_gddr6.rom",
        "ga107"      => "ga107_sim_gddr6.rom",
        "ga10b"      => "ga102_sim_gddr6.rom",
        "ga10f"      => "ga102_sim_gddr6.rom",

        # TODO(smahadevan): change to ad10x when ROMs are ready
        "ad102"      => "ga102_sim_gddr6.rom",
        "ad103"      => "ga102_sim_gddr6.rom",
        "ad104"      => "ga102_sim_gddr6.rom",
        "ad106"      => "ga102_sim_gddr6.rom",
        "ad107"      => "ga102_sim_gddr6.rom",

        "gh100"      => "gh100_sim_hbm.rom",
        "gh202"      => "gh202_sim_gddr6.rom",

        # TODO(akshatam): change to gb100 when ROMs are ready
        "gb100"      => "gh100_sim_hbm.rom",
        "gb102"      => "gh100_sim_hbm.rom",

        "g000"       => "g000_sim_hbm.rom",
    );

    # For Kepler+ chips redirect to _cmos version of VBIOS on Fmodel
    # and GM20X & GP100 redirect to pstates + dcb-ultimate version of VBIOS
    # because clk tests fail on pstatesless VBIOS and some display tests require
    # dcb entries.
    if ($model eq "fmod") {
        $biosTranslation{"gk104"} = "gk104_sim_gddr5_cmos.rom";
        $biosTranslation{"gk106"} = "gk106_sim_gddr5_cmos.rom";
        $biosTranslation{"gk107"} = "gk107_sim_gddr5_cmos.rom";
        $biosTranslation{"gk110"} = "gk110_sim_gddr5_cmos.rom";
        $biosTranslation{"gk20a"} = "gk20a_sim_sddr3.rom";
        $biosTranslation{"gm200"} = "gm200_sim_gddr5_cmos_dcb-ultimate_p-states.rom";
        $biosTranslation{"gm204"} = "gm204_sim_gddr5_cmos_dcb-ultimate_p-states.rom";
        $biosTranslation{"gm206"} = "gm206_sim_gddr5_cmos_dcb-ultimate_p-states.rom";
        $biosTranslation{"gp100"} = "gp100_sim_hbm_p-states_dcb-ultimate.rom";
        $biosTranslation{"gp102"} = "gp102_sim_gddr5_cmos.rom";
        $biosTranslation{"gp104"} = "gp104_sim_gddr5_cmos.rom";
        $biosTranslation{"gp105"} = "gp104_sim_gddr5_cmos.rom";
        $biosTranslation{"gp106"} = "gp106_sim_gddr5_cmos.rom";
        $biosTranslation{"gp107"} = "gp107_sim_gddr5_cmos.rom";
        $biosTranslation{"gp108"} = "gp108_sim_gddr5_cmos.rom";
        $biosTranslation{"gv100"} = "gv100_sim_hbm.rom";
        $biosTranslation{"gv105"} = "gv100_sim_hbm.rom";
        $biosTranslation{"gv11b"} = "gv100_sim_hbm_nodisplay.rom";
    }

    # Switch driver handles rom loading, no need to provide so escape them.
    my @biosEscape = (
    #   chip name
        "sv10",
        "lr10",
        "ls10",
        "s000"
    );

    # Strip the chip name out of projects like gf100x222 -> gf100
    my $chipIndex = $chip;
    $chipIndex =~ s/(g[^\d]...).*/$1/;

    # First look up the full project name in case of an override
    if (exists($biosTranslation{$chip})) {
        $biosFiles{0} = $biosTranslation{$chip};
    }
    # Second try to look up the actual stripped chip name
    elsif (exists($biosTranslation{$chipIndex})) {
        $biosFiles{0} = $biosTranslation{$chipIndex};
    }
    # Third escape switch devices
    elsif (grep $_ == $chip, @biosEscape) {
        ;
    }
    # Legacy case for Lwrie and Tesla
    else {
        $biosFiles{0} = "${chip}n.rom";
    }
}

# For gf100+ "-disable_display" actually requires a "no display" vbios
# This change is lwrrently in phase 1, i.e. if the no display vbios is present
# then that will be used and "-disable_display" will be removed from modsOpt
# however if a vbios file is missing, the default behavior will be to print a
# *warning* and leave "-disable_display" as part of modsOpt.  The warning may
# be changed to an error by passing "-errMissingNodispVbios" to sim.pl
#
# Once VBIOS has created all the necessary "_nodisp" vbios files, a missing
# "nodisp" vbios file can be made an error by default.
#
if ($modsOpt =~ /(-\bdisable_display\b)/) {
   my %missingBiosFiles;
   foreach (keys(%biosFiles)) {
       my $nodisp_bios = $biosFiles{$_};
       my $coproc_bios = $nodisp_bios;
       my $coproc_nodisp_bios = $nodisp_bios;
       my $nodisplay_bios = $nodisp_bios;

       # VBIOS files are expected to live in the same path as sim.pl
       print "Info : Looking for coproc roms.\n";
       $nodisp_bios =~ s/(.*)\.rom/$modsPath\/$1_nodisp.rom/;
       $coproc_bios =~ s/(.*)\.rom/$modsPath\/$1-coproc.rom/;
       $coproc_nodisp_bios =~ s/(.*)\.rom/$modsPath\/$1-coproc_nodisp.rom/;
       $nodisplay_bios =~ s/(.*)\.rom/$modsPath\/$1_nodisplay.rom/;
       if ((! -e "$nodisp_bios") && (! -e "$nodisplay_bios") && ((! -e "$coproc_nodisp_bios") && (! -e "$coproc_bios") || ($modsOpt =~ s/\-skip_coproc//))) {
          $missingBiosFiles{$_} = $nodisp_bios;
       }
       elsif (-e "$coproc_nodisp_bios")
       {
          $biosFiles{$_} = $coproc_nodisp_bios;
          print "Info : coproc_nodisp roms found.\n";
       }
       elsif (-e "$nodisplay_bios")
       {
          $biosFiles{$_} = $nodisplay_bios;
          print "Info : nodisplay roms found.\n";
       }
       elsif (-e "$nodisp_bios")
       {
          $biosFiles{$_} = $nodisp_bios;
          print "Info : nodisp roms found.\n";
       }
       else
       {
          $biosFiles{$_} = $coproc_bios;
          print "Info : coproc roms found.\n";
       }
   }
   if (%missingBiosFiles) {

      if ($errMissingNodispVbios) {
         print "Error: The following no display bios file(s) are missing:\n";
      }
      else
      {
         print "Warning: The following no display bios file(s) are missing, leaving \"-disable_display\":\n";
      }
      foreach (keys(%missingBiosFiles)) {
         print "    $missingBiosFiles{$_}\n";
      }
      if ($errMissingNodispVbios) {
         die "\"-errMissingNodispVbios\" specified, exiting due to missing vbios files";
      }
   }
   else
   {
      $modsOpt =~ s/(.*)\"-disable_display\"(.*)/$1$2/;
   }
}

die "The $modsProg program doesn't exist" unless (-e "$modsProg");
if (($model eq "vmod") and !$isSOC) {
    die "The RTL dispatch program ($dispatchProg) doesn't exist" unless (-e $dispatchProg);
    for ($i=0; $i < scalar(@simvType); $i++)
    {
       die "The RTL sim file ($simvFile[$i]) doesn't exist" unless (-f $simvFile[$i]);
    }
}

$args = "";
if (! $hwbios) {
    foreach (keys(%biosFiles)) {
        next if ($biosFiles{$_} eq 'HW');  # 'HW' means use on-board bios, so don't forward a -gpubios option to MODS
        $args .= " -gpubios $_ ";
        $biosAbsPath = "";
        if(!($biosFiles{$_} =~ /\//))
        {
            $biosAbsPath .= "$modsPath\/";
        }
        $biosAbsPath .= $biosFiles{$_};
        if ($_ eq '0')  # set gpubios full path into elw per request of bug 2028561
        {
            $ELW{'GFW_ROM_IMG'} = $biosAbsPath;
        }
        $args .= $biosAbsPath;
    }
}
# need to start socket before mods
if($sockserv) {
    my $socketArg;
    if ($sockSessionfile) {
        $socketArg = "-sessionfile $sockSessionfile 123";
        $modsOpt  .= " -chipargs \"$socketArg\"";
    }
    else {
        my $port = 1234;
        $socketArg = "-socket $port";
        $modsOpt  .= " -chipargs \"-socket localhost $port\"";
    }
    $sockCmd  = "$sockserv -chip $chipLib $socketArg";
    $args    .= " -chip sockchip.so";

}
elsif ($model eq "amod") {
    $args .= $modsAmodArgs;
}
else {
    $args .= " -chip $chipLib";
}

# enable some debug arguments for MODS/RM
if (defined $fcra)
{
    my $neededArgs = {
        '-threadid'              => "",
        '-channel_logging'       => "",
        '-dump_raw_chiplibtrace' => "$fcraDebugLevel",
        '-no_compress_capture'   => "",
        '-print_sys_time'        => ""
    };

    if ($fcraDebugLevel > 0)
    {
        if ($modsOpt !~ /"?-d"?\s+/)
        {
            $modsOpt = "\"-d\" $modsOpt";
        }
    }

    for my $needArg (keys %$neededArgs)
    {
        if ($modsOpt !~ /mdiag\.js.*$needArg/)
        {
            my $arg = $neededArgs->{$needArg} eq "" ? "\"$needArg\"" : "\"$needArg\" \"$neededArgs->{$needArg}\"";
            $modsOpt = join("\"mdiag.js\" $arg" ,split(/"?mdiag\.js"?/, $modsOpt));
        }
    }
}

$args .= " $modsOpt";

# GDB wrapper is used to work around not having the --args GDB option
$ELW{OPSYSTYPE_OVR} = "Linux" if ($isSOC and ($socSize==32)); # otherwise when running on 64 machine linsim will look for 64b simv
if ($gdb) {
    $args = "$modsProg $args";
    $modsProg = "$scriptPath/gdbwrap.pl";
    $ELW{INTERACTIVE_DEBUG} = 1;
} elsif ($ddd) {
    $args = "-ddd $modsProg $args";
    $modsProg = "$scriptPath/gdbwrap.pl";
    $ELW{INTERACTIVE_DEBUG} = 1;
} elsif ($gdbserver) {
    $args = "-gdbserver $gdbserverHostPort $modsProg $args";
    $modsProg = "$scriptPath/gdbwrap.pl";
    $ELW{INTERACTIVE_DEBUG} = 1;
} elsif ($dbg) {
    $args = "`cygpath -w $modsProg` $args";
    $modsProg = "vsjitdebugger";
    $ELW{INTERACTIVE_DEBUG} = 1;
}  elsif ($windbg) {
    $args = "$modsProg $args";
    $modsProg = "windbg";
    $ELW{INTERACTIVE_DEBUG} = 1;
}

if ($valgrind) {
    my $valgrind_path = "/home/utils/valgrind-3.17.0/bin/valgrind";
    $modsProg = "$valgrind_path -v --log-file=valgrind_output.log --error-limit=no --leak-check=full --suppressions=$scriptPath/javascript_suppressions.supp --suppressions=$scriptPath/python_suppressions.supp $modsProg";
}

# VF mods only need to launch mods process like fmodel
# No need to launch dispatch process and its args
my $isVirtFunMode = $ELW{"SRIOV_GFID"} != 0;
if (($model eq "vmod") and !$isSOC and !$isVirtFunMode) {
    # This is really bizarre.  This file is both parsed by the dispatch program.
    # The *number* on the *second* line of the file is the "weight" of the test,
    # which must be positive.  It is swallowed and all subsequent items are
    # interpreted as arguments to the test, while the first item is interpreted
    # as the "name" of the test (really, just the program to execute).  However,
    # the very first two arguments passed in are "-tstStream" followed by the
    # name/port to connect the socket up to to talk to the simulator.  Very,
    # very strange.
    warn "###WARNING### sim.pl detected that arguments with white spaces ($modsOpt) were passed to $dispatchFile, $dispatchProg does not understands such arguments, use -f option instead\n" if($modsOpt =~m|\"\s*[^"\s]+\s+[^"\s]+.*?\"|);
    open(DISPFILE, "> $dispatchFile") || die "can't open cmd file $dispatchFile";
    print DISPFILE "# Test Name, Weight, Arguments\n";
    print DISPFILE `echo $modsProg 10 $args`;
    close DISPFILE;

    if (scalar(@simvType) > 1)
    {
       open(CLNTFILE, "> $clntFile") || die "can't open cmd file $clntFile";
       print CLNTFILE "# One line per simv\n";
       for ($i = 0; $i < scalar(@simvType); $i++)
       {
          print CLNTFILE `echo $simvType[$i] $simvFile[$i] /dev/null $simvFlags[$i] `;
       }
       close CLNTFILE;
       $cmdline = "$dispatchProg $verbose -tstFile $dispatchFile -verilogBatchFile $clntFile -name $testname ";
    }
    else
    {
       $cmdline = "$dispatchProg -tstFile $dispatchFile  -numStreams 1 -tstCnt 1 -selectMethod sequential -verilogFile $simvFile[0] -verilogOptions '$simvFlags[0]' -filesFile /dev/null $timeOutDelaySecConnect";
    }

} else {
    $cmdline = "$modsProg $args";
    if ($isSOC) {
        $cmdline .= " -chipargs \"$simvFlags[0]\""      if (defined($simvFlags[0]) && length($simvFlags[0]));
        $cmdline .= " -chipargs \"$extraSimvFlags[0]\"" if (defined($extraSimvFlags[0]) && length($extraSimvFlags[0]));
    }
    if ($mtime) {
        print "\nMODS: $modsProg was last modified at: ";
        print getModTimeString($modsProg);
        print "\n";
    }
}

if ($mtime) {
    if (!$isWindows){
        @dirs = split (/:/, $ELW{LD_LIBRARY_PATH});
        @dirs = grep (!/^$/, @dirs); #get rid of empty entries
        while ($libDir = shift(@dirs)) {
            last if -e "$libDir/$chipLib";
        }
        $chipLib = "$libDir/$chipLib";
    }
    print "\nChiplib: $chipLib was last modified at: ";
    print getModTimeString($chipLib);
    print "\n\n";
}

if ($wine) {
    $ELW{WINEDEBUG} = "fixme-all,err-imagelist";
    $ELW{WINEPREFIX} = "$ELW{HOME}/.wine2004";
    $ELW{WINESERVER} = "/home/gnu/wine2004/bin/wineserver";
    $cmdline = "/home/gnu/wine2004/bin/wine -- $cmdline";
}

if($wrapper) {
    $cmdline = "$wrapper $cmdline";
}

if ($sockserv) {
    # concat the sockCmd to cmdLine to pass sockserver cmdline as an argument to MODS
    $cmdline = "$cmdline -sockCmdLine \"$sockCmd\"";
}

print "$sockCmd &\n\n" if $sockserv;
print "$cmdline\n";

if ($noops)
{
  print "sim.pl now exiting because of -n option.\n";
  exit(0);
}

# modslogger has to read command line from command.sh or .command.txt.
my $commandFileName = "command.sh";
if (not $dumpcmd) {
    $commandFileName = ".command.txt";
}

if (open(CMDFILE, "> $commandFileName")) {
    print CMDFILE "$sockCmd &\n\n" if $sockserv;
    print CMDFILE "$cmdline\n";
    close CMDFILE;

    if ($dumpcmd) {
        chmod 0755, $commandFileName;
    }
}

my $rc = "";

if ($testname) {
    # this indicates an invocation by testgen.  If error return value, write to .err file.
    if ($remote) {
        $rc = runRemoteTest();
    } elsif ($timeout == 0){
        $rc = system($cmdline);
    } else {
        $SIG{CHLD} = sub
        {
            while(waitpid(-1,WNOHANG)>0)
            {
                $rc = $?;
            }
        };
        defined( my $child = fork );
        if($child == 0)
        {
            exec($cmdline);
        }
        $SIG{TERM} = sub { kill TERM => $child };
        sleep $timeout;
        kill TERM => $child;
        sleep 1;
    }

    if (defined $fcra)
    {
        my $python3 = "elw PYTHONPATH=/home/utils/Python-3.6.1/lib/python3.6/site-packages /home/utils/Python-3.6.1/bin/python3";
        my $logPath = getRawLogPath();
        my $fcraCmd = "timeout 10m $python3 $fcra/fcra.py -l $logPath -o $logPath -c";

        my $fcraShellScript = "fcra.sh";
        open(my $fcraHandle, '>', $fcraShellScript);
        print $fcraHandle "#! /usr/bin/elw bash";
        print $fcraHandle "\n\n";
        print $fcraHandle 'echo "Running FullChipRunAnalyzer..."';
        print $fcraHandle "\n\n";
        print $fcraHandle "$fcraCmd\n\n";
        print $fcraHandle 'if [ $? -ne 0 ]; then echo "FullChipRunAnalyzer failed!"; else echo "FullChipRunAnalyzer run is successfully completed!"; fi ';
        close($fcraHandle);
        chmod 0755, $fcraShellScript;

        if ($rc)
        {
            system("./$fcraShellScript");
        }

        system("gzip -f $logPath/vector_raw.log") == 0 or warn "FCRA: failed to compress vector_raw.log: $?\n";
    }

    # start logger
    runModsLogger($rc, $model, $chip, $remote);

    if ($rc) {
        my $errmsg = "MODS ". system_error($rc) . " Check the log for possible errors.\n";
        open TESTERROR, ">>$testname.err";
        print TESTERROR "$errmsg";
        print "$errmsg";
        close TESTERROR;

        # Propagate any signal to testgen - this info is lost in the exit value.
        if (my $signum = $rc & 0x7f) {
            kill $signum, $$;
        }
    } elsif ($processRawLog) {
        postProcessRawLog();
    }

    exit $rc >> 8;
} elsif ($isWindows) {
    # Can't use exec on Windows, it seems to be broken.
    # Exit with the same status as the system call.
    # To get the status you must shift the return value by 8
    exit(system($cmdline) >> 8);
} else {
    exec $cmdline;

    #start logger
    runModsLogger($rc, $model, $chip, $remote);

    die "couldn't run command ($cmdline): $!\n";
}

sub runRemoteTest {
    print "Enter runRemoteTest!\n";

    # The "remote_test.sh" contains all the cmds that needs to run a test like
    # "export LD_LIBRARY_PATH", "cd /home/scratch..." and "mods ..."
    my $shfilename = "$ELW{PWD}/remote_test.sh";
    dumpCmd2SHFile($shfilename, $cmdline);

    # The "remote_test.exp" is used to ssh to the remote machine like P8
    # without input password manualy.
    my $expfilename = "$ELW{PWD}/remote_test.exp";

    genExpFile4SSH($expfilename, $host, $user, $password, $shfilename);
    my $rc = system("$expfilename");

    print ("remote command returns:$rc\n");
    print "Exit runRemoteTest!\n";

    return $rc;
}

#Dump all cmds that needs to be exelwted remotely to a shell script
sub dumpCmd2SHFile {
    print "Enter dumpCmd2SHFile!\n";
    my ($rshfilename, $commandline) = @_;
    open(testcmdfh, ">$rshfilename") || die "Error Message: $!";
    print testcmdfh "#! /usr/bin/elw bash\n\n";
    print testcmdfh "export LD_LIBRARY_PATH=\"$ELW{'LD_LIBRARY_PATH'}\"\n\n";
    print testcmdfh "export LW_BUILD_PLATFORM=\"$ELW{'LW_BUILD_PLATFORM'}\"\n\n";
    print testcmdfh "export LW_PROJECT_TREE=\"$ELW{'LW_PROJECT_TREE'}\"\n\n";
    print testcmdfh "export TREE_ROOT=\"$ELW{'TREE_ROOT'}\"\n\n";
    print testcmdfh "cd $ELW{'PWD'}\n\n";
    print testcmdfh $commandline;
    print "\n";
    close(testcmdfh);
    my $mode = 0755;
    chmod $mode, $rshfilename || die "Error Message: $!";
    print "Exit dumpCmd2SHFile!\n";
}

sub genExpFile4SSH {
    print "Enter genExpFile4SSH!\n";
    my($expfilename, $host, $user, $password, $shfilename) = @_;
    open(exp4sshfh, ">$expfilename") || die "Error Message: $!";
    print exp4sshfh "#!/usr/bin/expect -f\n\n";
    print exp4sshfh "set timeout 30\n\n";
    print exp4sshfh "spawn /usr/bin/ssh $user\@$host\n\n";
    print exp4sshfh "expect \"password:\"\n\n";
    print exp4sshfh "send \"$password\\r\"\n\n";
    print exp4sshfh "expect \"]*\"\n\n";
    print exp4sshfh "send $shfilename\\r\n\n";
    print exp4sshfh "expect \"]*\"\n\n";
    print exp4sshfh "send \"exit\\r\"\n\n";
    print exp4sshfh "interact\n";
    close(exp4sshfh);
    my $mode = 0755;
    chmod $mode, $expfilename || die "Error Message: $!";
    print "Exit genExpFile4SSH!\n";
}

sub getRawLogPath
{
    my $logPath = "./";
    if ($testname) {
        $testname =~ m/(.*\/)[^\/]*$/;
        $logPath = $1;
    }
    return $logPath;
}

sub postProcessRawLog {
    print "Enter postProcessRawLog!\n";

    return if $isWindows;

    my $logPath = getRawLogPath();
    my $cmd = "$modsPath/process_trace.pl -trace ${logPath}vector_raw.log -chip $chip > ${logPath}vector_processed.log";
    system($cmd);

    print "Exit postProcessRawLog!\n";
}

sub getModTimeString($) {
    my ($filename, $dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size);
    my ($atime,$mtime,$ctime,$blksize,$blocks);
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst);
    $filename = $_[0];
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
          $atime,$mtime,$ctime,$blksize,$blocks) = stat($filename);
    ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) =
                                                   gmtime($mtime);
    $mon += 1;
    $year +=1900;
    return sprintf ("%02d/%02d/%d %02d\:%02d\:%02d GMT",
          $mon, $mday, $year, $hour, $min, $sec);
}

#
# Gives detailed information on why a call to system or `` failed.
# Only argument is the value returned by system.  Alternatively,
# you can pass in the $CHILD_ERROR (AKA $@) variable which holds
# the same information.
#
sub system_error {
    my $rc = shift;
    my @signame = ();
    use Config;
    defined $Config{sig_name} or die "No sigs?";
    my @signame = split(' ', $Config{sig_name});
    my $errmsg;
    if($rc == -1) {
        $errmsg = "failed to execute properly: OS_ERROR = ".(0+$!)." ($!).";
    }
    elsif($rc & 0x80) {
        $errmsg = "coredumped.";
    }
    elsif(my $signum = $rc & 0x7f) {
        $errmsg = "was killed with signal $signum SIG$signame[$signum].";
    }
    else {
        my $status = $rc >> 8;
        $errmsg = "exited with nonzero status of $status.";
    }
    return $errmsg;
}

sub runModsLogger{
    my ($rc, $model, $chip, $remoteFlag) = @_;
    my $simPerlExitNormally = 1;

    my $modsLoggerCmd = "$FindBin::Bin/modslogger.pl --simPerlExitNormally $simPerlExitNormally --rc $rc --model $model --chip $chip --remote $remoteFlag --metadata \"$testMetadata\"";
    system($modsLoggerCmd) == 0 or warn "MODS Logger: system $modsLoggerCmd failed: $?";
}
