#!/home/gnu/bin/perl -w
#
# updallfmods_chipsa.pl:  On any chipa sw tree, with all elw variables set,
# Script submits newer fmodels to DVS.

use strict;
no strict "refs";
use IO::File;
use File::Basename;
use Cwd;

# die "syntax: updallfmods_chipsa.pl" if @ARGV != 0;

################### branch and user specific variables ########################
my $branch = "chipsa";
my $fmodUpdateData = $ELW{"fmodUpdateData"};
my $fmodUpdateGlobalData = $ELW{"fmodUpdateGlobalData"};

my $globalLogDirThisBranch = "$fmodUpdateData/logs/$branch";
my $previousFmodUpdatedList = "$fmodUpdateGlobalData/$branch"."/previousFmodUpdatedList";
my $lastKnownGoodFmodLoc = "$fmodUpdateData/lastKnownGoodFmodels";
###############################################################################


# Config Var's
my $date = `date`;
print "Current date is $date\n";
# place to dump out all the log files:
# Create the directory with time stamp
 my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
$yr += 1900;
$month += 1;

######################### variables for this run ##############################
my $direc = "$yr.$month.$dayofm.$hr.$min.$sec";
my $logDirForThisRun =
"$globalLogDirThisBranch/"."$direc";
#my $logDirForThisRun = "$globalLogDirThisBranch/".$yr.".".$month.".".$dayofm.".".$hr.".".$min.".".$sec;
# Create Directory
print "Trying to create directory $logDirForThisRun for storing logs of this run\n";
mkdir($logDirForThisRun, 0755) || die "Not able to create directory $logDirForThisRun\n";
print "Created directory $logDirForThisRun for storing logs\n";

# Master Logfile taken out of p4 sync and running sim mods on each chips.
# similar to qsub.log in cron_fmodupd.sh
my $masterLogFileThisRun = "$logDirForThisRun"."/masterLogThisRun.log";
my $changelistfile = "changelist.txt";
my $statusfile = "$logDirForThisRun"."/report.log";
my $emailLog = "$logDirForThisRun"."/email.html";
###############################################################################

my $debug = 1;
my $dofast = 0;
my @hasBugArray = (0,
                   0);


# Test Name Arr's
my @passTests = undef;
my @failTests = undef;
my @unsuTests = undef;
my @skipTests = undef;

# Test Count Var's
my ($totalTc, $passTc, $failTc, $unsuTc, $skipTc) = undef;
my @runSummary = undef;
my %finalChipStatus;

# Global Var's.
my $lhwcl = undef;
my $lswcl = undef;
my $newClNetlist = undef;
my @cl_desc = undef;
my $lastSubTotSwCl = undef;
my $assertFail = undef;
my $modsrunstatus = undef;
my $p4spec = undef;
my $as2spec = undef;

my @needupdchips = undef;
my @sanityfailedchips = undef;
my @up2datechips = undef;
my %fmodinregsys;
my $hwClToUpd = undef;

# Chip passed as args, default : all
my $argsChip = 'all';

# Sm2 variables
my $sm2_bin = "$ELW{P4ROOT}/sw/apps/gpu/drivers/common/sm2/bin";
my $sm2_configdir = "$ELW{P4ROOT}/sw/dev/gpu_drv/chips_a/drivers/resman/sm2tests";

my %inifile = (
#   "gm107" => "dvs/gm107fmodel.ini",
#   "gm108" => "dvs/gm108fmodel.ini",
#   "gm000" => "dvs/gm000fmodel.ini",
#   "gm200" => "fmod_update/gm200fmodel.ini",
#   "gm204" => "fmod_update/gm204fmodel.ini",
#   "gm206" => "fmod_update/gm206fmodel.ini",
#   "gp100" => "fmod_update/gp100fmodel.ini",
#   "gp104" => "fmod_update/gp104fmodel.ini",
#   "gv000" => "fmod_update/gv000fmodel.ini",
#   "gv100" => "fmod_update/gv100fmodel.ini",
   "tu102" => "fmod_update/tu102fmodel.ini",
   "ga100" => "fmod_update/ga100fmodel.ini",
   "ga102" => "fmod_update/ga102fmodel.ini"
);

my %regsys =  (
#   "gt212" => "/home/scratch.regress04/linux50/tesla2_tree",
#   "gt215" => "/home/scratch.regress04/linux50/tesla2_gt215",
#   "gt216" => "/home/scratch.regress04/linux50/tesla2_gt216",
#   "gt218" => "/home/scratch.regress04/linux50/tesla2_gt218",
#   "igt21a" => "/home/scratch.regress04/linux50/tesla2_tree",
#   "gf100" => "/home/scratch.regress04/linux50/fermi1_gf100",
#   "gf104" => "/home/scratch.regress04/linux50/fermi1_gf10x",
#   "gf106" => "/home/scratch.regress04/linux50/fermi1_gf10x",
#   "gf108" => "/home/scratch.regress04/linux50/fermi1_gf10y",
#   "gf117" => "/home/scratch.autobuild1/packages/Linux/fermi1_gf11x",
#   "gf119" => "/home/scratch.autobuild1/packages/Linux/fermi1_gf119",
#   "gk104" => "/home/scratch.autobuild1/packages/Linux/kepler1_gklit1",
#   "gk110" => "/home/scratch.autobuild1/packages/Linux/kepler1_gk110",
#   "gk20a" => "/home/scratch.autobuild1/packages/Linux/kepler1_gk100",
#   "gk208" => "/home/scratch.autobuild1/packages/Linux/kepler1_gklit4",
#   "gm107" => "/home/scratch.autobuild1/packages/Linux/lwgpu_gmlit1",
#   "gm108" => "/home/scratch.autobuild1/packages/Linux/lwgpu_gmlit1",
#   "gm000" => "/home/scratch.autobuild5/packages/Linux/maxwell_staging",
#   "gm200" => "/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_gmlit2",
#   "gm204" => "/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_gmlit2",
#   "gm206" => "/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_gmlit2",
#   "gp100" => "/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_gmlit4",
#   "gp104" => "/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_gplit3",
#   "gv000" => "/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu",
#   "gv100" => "/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_gvlit1",
   "tu102" => "/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_tu102",
   "ga100" => "/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_ga100",
   "s000" => "/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu",
   "ga102" => "/home/scratch.autobuild5/packages/Linux_x86_64/lwgpu_ga102"
);

my %projectName = (
#   "gt212" => "tesla2",
#   "gt215" => "tesla2_gt215",
#   "gt216" => "tesla2_gt216",
#   "gt218" => "tesla2_gt218",
#   "igt21a" => "tesla2",
#   "gf100" => "fermi1_gf100",
#   "gf104" => "fermi1_gf10x",
#   "gf106" => "fermi1_gf10x",
#   "gf108" => "fermi1_gf10y",
#   "gf117" => "fermi1_gf11x",
#   "gf119" => "fermi1_gf11x",
#   "gk104" => "gklit1_fmodel",
#   "gk110" => "gklit2_fmodel",
#   "gk20a" => "gklit3_fmodel",
#   "gk208" => "gklit4_fmodel",
#   "gm107" => "gmlit1_fmodel",
#   "gm108" => "gmlit1_fmodel",
#   "gm000" => "gmlit2_fmodel",
#   "gm200" => "gmlit2_fmodel",
#   "gm204" => "gmlit2_fmodel",
#   "gm206" => "gmlit2_fmodel",
#   "gp100" => "gmlit4_fmodel",
#   "gp104" => "gplit3_fmodel",
#   "gv000" => "glit0_fmodel",
#   "gv100" => "gvlit1_fmodel",
   "tu102" => "tulit2_fmodel",
   "ga100" => "galit1_fmodel",
   "s000" => "slit0_fmodel",
   "ga102" => "galit2_fmodel"
);
my %projectMgr = (
#   "gf117" => "CInman\@exchange.lwpu.com",
#   "gf119" => "DKumar\@exchange.lwpu.com",
#   "gk104" => "dswoboda\@exchange.lwpu.com",
#   "gk110" => "SCheung\@exchange.lwpu.com vsharma\@exchange.lwpu.com", 
#   "gk20a" => "dswoboda\@exchange.lwpu.com tpurankar\@exchange.lwpu.com",   
#   "gk208" => "dkumar\@exchange.lwpu.com jakumar\@exchange.lwpu.com",
#   "gm107" => "dkumar\@exchange.lwpu.com jakumar\@exchange.lwpu.com",
#   "gm108" => "dkumar\@exchange.lwpu.com jakumar\@exchange.lwpu.com",
#   "gm000" => "scheung\@exchange.lwpu.com",
#   "gm200" => "vsharma\@exchange.lwpu.com",
#   "gm204" => "vsharma\@exchange.lwpu.com",
#   "gm206" => "vsharma\@exchange.lwpu.com",
#   "gp100" => "vsharma\@exchange.lwpu.com",
#   "gp104" => "vsharma\@exchange.lwpu.com lachang\@exchange.lwpu.com",
#   "gv000" => "vsharma\@exchange.lwpu.com amanjunatha\@exchange.lwpu.com asinghal\@exchange.lwpu.com",
#   "gv100" => "vsharma\@exchange.lwpu.com amanjunatha\@exchange.lwpu.com asinghal\@exchange.lwpu.com",
   "tu102" => "vsharma\@exchange.lwpu.com jamesx\@exchange.lwpu.com",
   "ga100" => "vsharma\@exchange.lwpu.com msoman\@exchange.lwpu.com arvindg\@exchange.lwpu.com",
   "s000" => "vsharma\@exchange.lwpu.com msoman\@exchange.lwpu.com arvindg\@exchange.lwpu.com tcarraher\@exchange.lwpu.com",
   "ga102" => "vsharma\@exchange.lwpu.com ssapre\@exchange.lwpu.com arvindg\@exchange.lwpu.com"
);

my %projectMgr_cc = (
#   "gf117" => "CInman\@exchange.lwpu.com",
#   "gf119" => "DKumar\@exchange.lwpu.com",
#   "gk104" => "ycao\@exchange.lwpu.com",
);

my @email_monitor_list = ("kwadhwa\@exchange.lwpu.com", "nitishk\@exchange.lwpu.com", "asinghal\@exchange.lwpu.com");

my %fmodlocation =  (
#   "gt212" => "$fmodUpdateData/fmodels/forChipsa/gt212",
#   "gt215" => "$fmodUpdateData/fmodels/forChipsa/gt215",
#   "gt216" => "$fmodUpdateData/fmodels/forChipsa/gt216",
#   "gt218" => "$fmodUpdateData/fmodels/forChipsa/gt218",
#   "igt21a" => "$fmodUpdateData/fmodels/forChipsa/igt21a",
#   "gf100" => "$fmodUpdateData/fmodels/forChipsa/gf100",
#   "gf104" => "$fmodUpdateData/fmodels/forChipsa/gf104",
#   "gf106" => "$fmodUpdateData/fmodels/forChipsa/gf106",
#   "gf108" => "$fmodUpdateData/fmodels/forChipsa/gf108",
#   "gf117" => "$fmodUpdateData/fmodels/forChipsa/gf117",
#   "gf119" => "$fmodUpdateData/fmodels/forChipsa/gf119",
#   "gk104" => "$fmodUpdateData/fmodels/forChipsa/gk104",
#   "gk110" => "$fmodUpdateData/fmodels/forChipsa/gk110",
#   "gk20a" => "$fmodUpdateData/fmodels/forChipsa/gk20a",
#   "gk208" => "$fmodUpdateData/fmodels/forChipsa/gk208",
#   "gm107" => "$fmodUpdateData/fmodels/forChipsa/gm107",
#   "gm108" => "$fmodUpdateData/fmodels/forChipsa/gm108",
#   "gm000" => "$fmodUpdateData/fmodels/forChipsa/gm000",
#   "gm200" => "$fmodUpdateData/fmodels/forChipsa/gm200",
#   "gm204" => "$fmodUpdateData/fmodels/forChipsa/gm204",
#   "gm206" => "$fmodUpdateData/fmodels/forChipsa/gm206",
#   "gp100" => "$fmodUpdateData/fmodels/forChipsa/gp100",
#   "gp104" => "$fmodUpdateData/fmodels/forChipsa/gp104",
#   "gv000" => "$fmodUpdateData/fmodels/forChipsa/gv000",
#   "gv100" => "$fmodUpdateData/fmodels/forChipsa/gv100",
   "tu102" => "$fmodUpdateData/fmodels/forChipsa/tu102",
   "ga100" => "$fmodUpdateData/fmodels/forChipsa/ga100",
   "ga102" => "$fmodUpdateData/fmodels/forChipsa/ga102"
);

my %netlists = (
#   "gt212" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/gt212/net/netlists.h",
#   "gt215" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/gt215/net/netlists.h",
#   "gt216" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/gt216/net/netlists.h",
#   "gt218" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/gt218/net/netlists.h",
#   "igt21a" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/igt21a/net/netlists.h",
#   "gf100" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/fermi/gf100/net/dvs_package.h",
#   "gf104" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/fermi/gf104/net/dvs_package.h",
#   "gf106" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/fermi/gf106/net/dvs_package.h",
#   "gf108" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/fermi/gf108/net/dvs_package.h",
#   "gf117" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/fermi/gf117/net/dvs_package.h",
#   "gf119" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/fermi/gf119/net/dvs_package.h",
#   "gk104" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/kepler/gk104/net/dvs_package.h",
#   "gk110" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/kepler/gk110/net/dvs_package.h",
#   "gk20a" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/kepler/dt124/net/dvs_package.h",
#   "gk208" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/kepler/gk208/net/dvs_package.h",
#   "gm107" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/maxwell/gm107/net/dvs_package.h",
#   "gm108" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/maxwell/gm108/net/dvs_package.h",
#   "gm000" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/maxwell/gm000/net/dvs_package.h",
#   "gm200" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/maxwell/gm200/net/dvs_package.h",
#   "gm204" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/maxwell/gm204/net/dvs_package.h",
#   "gm206" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/maxwell/gm206/net/dvs_package.h",
#   "gp100" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/pascal/gp100/net/dvs_package.h",
#   "gp104" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/pascal/gp104/net/dvs_package.h",
#   "gv000" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/volta/gv000/net/dvs_package.h",
#   "gv100" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/volta/gv100/net/dvs_package.h",
   "tu102" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/turing/tu102/net/dvs_package.h",
   "ga100" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/ampere/ga100/net/dvs_package.h",
   "s000" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/ampere/s000/net/dvs_package.h",
   "ga102" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/ampere/ga102/net/dvs_package.h"
);

my %updhwcl = (
#   "gt212" => 0,
#   "gt215" => 0,
#   "gt216" => 0,
#   "gt218" => 0,
#   "igt21a" => 0,
#   "gf100" => 0,
#   "gf104" => 0,
#   "gf106" => 0,
#   "gf108" => 0,
#   "gf117" => 0,
#   "gf119" => 0,
#   "gk104" => 0,
#   "gk110" => 0,
#   "gk20a" => 0,
#   "gk208" => 0,
#   "gm107" => 0,
#   "gm108" => 0,
#   "gm000" => 0,
#   "gm200" => 0,
#   "gm204" => 0,
#   "gm206" => 0,
#   "gp100" => 0,
#   "gp104" => 0,
#   "gv000" => 0,
#   "gv100" => 0,
   "tu102" => 0,
   "ga100" => 0,
   "s000" => 0,
   "ga102" => 0
);

my %updswcl = (
#   "gt212" => 0,
#   "gt215" => 0,
#   "gt216" => 0,
#   "gt218" => 0,
#   "igt21a" => 0,
#   "gf100" => 0,
#   "gf104" => 0,
#   "gf106" => 0,
#   "gf108" => 0,
#   "gf117" => 0,
#   "gf119" => 0,
#   "gk104" => 0,
#   "gk110" => 0,
#   "gk20a" => 0,
#   "gk208" => 0,
#   "gm107" => 0,
#   "gm108" => 0,
#   "gm000" => 0,
#   "gm200" => 0,
#   "gm204" => 0,
#   "gm206" => 0,
#   "gp100" => 0,
#   "gp104" => 0,
#   "gv000" => 0,
#   "gv100" => 0,
   "tu102" => 0,
   "ga100" => 0,
   "s000" => 0,
   "ga102" => 0
);

my %external2InternalChip = (
#   "gf117" => "gf117",
#   "gf119" => "gf119",
#   "gk104" => "gk104",
#   "gk110" => "gk110",
#   "gk20a" => "dT124",
#   "gk208" => "gk208",
#   "gm107" => "gm107",
#   "gm108" => "gm108",
#   "gm000" => "gm000",
#   "gm200" => "gm200",
#   "gm204" => "gm204",
#   "gm206" => "gm206",
#   "gp100" => "gp100",
#   "gp104" => "gp104",
#   "gv000" => "gv000",
#   "gv100" => "gv100",
   "tu102" => "tu102",
   "ga100" => "ga100",
   "s000" => "s000",
   "ga102" => "ga102"
);

my %noTestPassed = (
#   "gt212" => 0,
#   "gt215" => 0,
#   "gt216" => 0,
#   "gt218" => 0,
#   "igt21a" => 0,
#   "gf100" => 0,
#   "gf104" => 0,
#   "gf106" => 0,
#   "gf108" => 0,
#   "gf117" => 0,
#   "gf119" => 0,
#   "gk104" => 0,
#   "gk110" => 0,
#   "gk20a" => 0,
#   "gk208" => 0,
#   "gm107" => 0,
#   "gm108" => 0,
#   "gm000" => 0,
#   "gm200" => 0,
#   "gm204" => 0,
#   "gm206" => 0,
#   "gp100" => 0,
#   "gp104" => 0,
#   "gv000" => 0,
#   "gv100" => 0,
   "tu102" => 0,
   "ga100" => 0,
   "s000" => 0,
   "ga102" => 0
);

# Validate Script environment requirements
sub verifyelwironment {
    my $okClient = 1;

    if(($ELW{HOST} !~ /^l-xterm-[0-9][0-9]/) && ($ELW{HOST} !~ /^sc-cron/) && ($ELW{HOST} !~ /^o-xterm-[0-9][0-9]/) && ($ELW{HOST} !~ /^dc1-xterm-[0-9][0-9]/)) {
        print "Fmodel submission can occur only on l-xterms\n";
        print "As regsys database is invisible in other forms\n";
        print "Current host = $ELW{HOST}";
        $okClient = 0;
    }

    # set p4elw from .p4 config
    setp4elwfromconfig();

    if (!defined($ELW{"P4ROOT"})) {
        print "Elw : P4ROOT not defined \n";
        $okClient = 0;
    }

    if (!defined($ELW{"P4USER"})) {
        print "Elw : P4USER not defined \n";
        $okClient = 0;
    }

    if (!defined($ELW{"P4CLIENT"})) {
        print "Elw : P4CLIENT not defined \n";
        $okClient = 0;
    }

    if (!defined($ELW{MODS_PATH})) {
        print "Elw : MODS_PATH not defined \n";
        $okClient = 0;
    }

    if (!defined($ELW{"MODS_RUNSPACE"})) {
        print "Elw : MODS_RUNSPACE not defined \n";
        $okClient = 0;
    }


    if (!$okClient) {
        exit 0;
    }

    return $okClient;
}

# Setup environment for script, sets globals to good values.
sub configureElwironment {
    # Need for sucessful building of RM with all RMTESTS
    $ELW{INCLUDE_RMTEST} = "true";

    # Need to disable all fmodel info and warning level debug prints as the test times have shot up considerably
    #$ELW{CSL} = "[disable]assert,-info,-warning";

    # Load destination fmodel location
    $p4spec = "/home/utils/bin/p4 -c $ELW{P4CLIENT} -p $ELW{P4PORT}";
    $as2spec = "/home/lw/bin/as2  -c $ELW{P4CLIENT} -p $ELW{P4PORT} -u $ELW{P4USER} ";
}

# Read .p4config and update p4elw's
sub setp4elwfromconfig {

    open(CONFIG, "< "."$ELW{P4ROOT}"."/sw/.p4config") or die ".p4config file not found $!";

    while(<CONFIG>) {
    if ($_ =~ /P4PORT=(.*)/) {
            $ELW{"P4PORT"} = $1;
}
        if ($_ =~ /P4CLIENT=(.*)/) {
            $ELW{"P4CLIENT"} = $1;
        }
    }
    close(CONFIG);
}

# Return Netlists.h Cl description info.
sub getp4ClDesc {
    my $descline = undef;

    $descline = "\n\tFmodel update on to DVS\n\t$date";
    push (@cl_desc, $descline);
    foreach my $chip (@needupdchips) {
       if ( defined($chip)) {
          $descline = "\n\tChip : ".uc($chip)." HWCL : $updhwcl{$chip} SWCL : $updswcl{$chip}";
          push (@cl_desc, $descline);
       }
    }

    $descline = "\n\tBug 478135 Bug 421682\n";
    push (@cl_desc, $descline);
    $descline = "\treviewed by : vsharma, kwadhwa\n";
    push(@cl_desc, $descline);

#    $descline = "\tdvs_skip_auto_builds\n";
#    push(@cl_desc, $descline);
    $descline = "\tdvs_skip_auto_tests\n";
    push(@cl_desc, $descline);
    $descline = "\tSkipping DVS builds and tests since these CLs are already tested before submit, avoiding unnecessary DVS load.\n";
    push(@cl_desc, $descline);

    return \@cl_desc;
}

sub p4EditFiles {
    my $editfiles = undef;
    open (P4SUB, "> $logDirForThisRun/p4submit.log") or die "can't open file $logDirForThisRun/p4submit.log : $!";

    foreach my $chip (@needupdchips)  {
       if (defined ($chip)) {
          $editfiles = "$editfiles"." "."$netlists{$chip}";
       }
    }

    if (defined($editfiles)) {
        # force sync before editing files
        print P4SUB "$p4spec sync -f $editfiles 2>&1";
        my @synclog = `$p4spec sync -f $editfiles 2>&1`;

        print P4SUB "$p4spec edit $editfiles 2>&1";
        my @editlog = `$p4spec edit $editfiles 2>&1`;

        print @synclog;
        print @editlog;

        print P4SUB @synclog;
        print P4SUB @editlog;
    }
    close(P4SUB);

    return;
}


sub p4revertFiles {
    my $revertfiles = undef;
    open (P4SUB, "> $logDirForThisRun/p4submit.log") or die "can't open file $logDirForThisRun/p4submit.log : $!";

    foreach my $chip (keys %regsys)  {
       $revertfiles = "$revertfiles"." "."$netlists{$chip}";
    }

    my @revertlog = `$p4spec revert $revertfiles 2>&1`;

    print @revertlog;
    print P4SUB @revertlog;

    my $p4ClDeleteLog = `$p4spec change -d $newClNetlist`;
    print P4SUB $p4ClDeleteLog;
    close(P4SUB);

    return;
}

sub getFilesInCL {
   my @filesInCl = undef;
   open (P4SUB, "> $logDirForThisRun/p4submit.log") or die "can't open file $logDirForThisRun/p4submit.log : $!";

    foreach my $chip (@needupdchips)  {

       next if !defined($chip);
       push(@filesInCl, $netlists{$chip});

    }

    print "Files in CL";
    print P4SUB @filesInCl;
    close(P4SUB);

    return \@filesInCl;
}

sub p4CreateNewChangelist {

    my ($newclfile) = @_;

   open(NEWCL, ">$newclfile");

# header print
print NEWCL "

#
#  Change:      The change number. 'new' on a new changelist.
#  Date:        The date this specification was last modified.
#  Client:      The client on which the changelist was created.  Read-only.
#  User:        The user who created the changelist.
#  Status:      Either 'pending' or 'submitted'. Read-only.
#  Description: Comments about the changelist.  Required.
#  Jobs:        What opened jobs are to be closed by this changelist.
#               You may delete jobs from this list.  (New changelists only.)
#  Files:       What opened files from the default changelist are to be added
#               to this changelist.  You may delete files from this list.
#               (New changelists only.)
\n";

# specification
print NEWCL "

Change: new

Client: ".$ELW{"P4CLIENT"}."

User:   ".$ELW{"P4USER"}."

Status: pending
\n";

    # Cl description
    print NEWCL "Description:\n\n";
    getp4ClDesc();
    print NEWCL @cl_desc;

    # Print files in CL
    print NEWCL "\nFiles:\n\n";
    foreach my $chip (@needupdchips)  {
       if ( defined($chip)) {
           print NEWCL "\t$netlists{$chip}\n";
       }
    }
    close(NEWCL);
}


# Return a sorted list of sucessfully built directories in the build tree.
sub getbuilds {
    my ($dir, $chip) = @_;
    my @builds;
    my $subdir;

    print "Fmodel Dir : $dir chip : $chip\n" if ($debug);

    opendir(DIR, $dir) or die "Can't open $dir !";
    # This hack is needed for gf11x, where fmodel directories are not named
    # gf110_fmodel or gf118_fmodel. They are named like gf11x_fmodel, gflit1_fmodel
    my $actualFmodDir = getActualFmodDirName($chip);

    while ($subdir = readdir(DIR)) {

        # Skip if it's non-numeric.
        next if $subdir !~ m/^[0-9]+$/;

        # Skip if it's not a directory, or if there doesn't appear to be an
        # fmodel DLL inside.
        next if ! -d "$dir/$subdir";
        next if (! -f "$dir/$subdir/$actualFmodDir/build.tgz");

        push @builds, $subdir;
    }
    closedir(DIR);
    return @builds;
}

#
# Returns the actual fmod directory into which we should look for fmodel
# Traditionally, it has been "<chip>_fmodel", like "gf100_fmodel" or "gf104_fmodel"
# But starting with gf11x, this changed :( Now we have:
# gf110, gf114 etc = gf11x_fmodel
# gf118, gf119 = gflit1_fmodel
# gk104 = gklit1_fmodel
#
sub getActualFmodDirName {
    my ($chip) = @_;
    
    print "For chip $chip, Actual fmod dir = $projectName{$chip}\n" if ($debug);
    return $projectName{$chip};
}

sub getlatesthwclregsys {
    my($chip) = @_;
    my $regsysPath = undef;

    $regsysPath =  $regsys{$chip} ;

    if ( !defined($fmodinregsys{$chip}) ) {
        my @list = getbuilds($regsysPath, $chip);
        #my @fmodlist = reverse sort(@list);
        # numerical sort so CL numbers with 8 digits come first
        my @fmodlist = reverse sort {$a <=> $b} @list;
        $fmodinregsys{$chip} = \@fmodlist;
    }

    print "regsys : $regsysPath  chip : $chip  hwcl : $fmodinregsys{$chip}->[0]\n" if $debug;

    return $fmodinregsys{$chip}->[0];
}

sub fillLatestHwcl {
   # fill latest hwcl for each chip
   foreach my $chip (keys %regsys) {
      $updhwcl{$chip} = getlatesthwclregsys($chip);
   }
}

# get the latest fmodel
sub getlatestfmodpath {
    my ($chip) = @_;
    my $actualFmodDir = getActualFmodDirName($chip);

    my $fmodpath = $regsys{$chip}."\/".$updhwcl{$chip}."\/".$actualFmodDir."\/build.tgz";

    print "fmod path $chip : $fmodpath\n" if ($debug);

    return $fmodpath;
}

sub fillSwclForChipHwcl {
    my ($chip, $hwcl) = @_;
    my $swcl = undef;

    if ( !defined($hwcl) or !defined($chip) ) {
        print "Error : chip and hwcl not defined for getting swcl \n";
        return ;
    }

    my $fmodpath = $regsys{$chip}."\/".$hwcl."\/".$chip."_fmodel\/build.tgz";

    my $modsChangelist = $fmodpath;
    $modsChangelist =~ s/($chip)_fmodel\/build.tgz/mods.changelist/;
    print "mods.changelist path : $modsChangelist\n" if ($debug);

    if(open(SWCL, "$modsChangelist")) {
       $swcl = <SWCL>;
       chomp($swcl);
       close(SWCL);
    } else {
       print "Error : Failed to opened $modsChangelist file\n";
       $swcl = "12345678";
    }

    $updswcl{$chip} = $swcl;

    print "Chip : $chip, Hwcl : $hwcl,  Upd swcl : $updswcl{$chip}\n" if ($debug);
}

# copy fmodel to user space.
sub copyfmod {
    my($chip) = @_;
    my $fmodDestDir = $fmodlocation{$chip};
    if(! -d $fmodDestDir)
    {
        system("mkdir $fmodDestDir");
    }
    my $destpath = $fmodDestDir."\/build_$chip.tgz";
    my $destfmodpath = $lastKnownGoodFmodLoc."\/temp.build_".$chip."_"."$updhwcl{$chip}".".tgz";
    my $srcfmodpath = getlatestfmodpath($chip);

    # delete all files in the destination directory
    print "system(rm -rf $fmodDestDir/*)\n" if $debug;
    system("rm -rf $fmodDestDir/*");

    print("cp $srcfmodpath $destpath\n");

    if(!system("cp $srcfmodpath $destpath"))
    {
        fillSwclForChipHwcl($chip, $updhwcl{$chip});
        print "copied fmodel with HWCL : $updhwcl{$chip} and SWCL $updswcl{$chip}\n";
    }
    # copying same for lastKnownGoodFmod ref
    print("cp $destpath $destfmodpath\n");
    if(!system("cp $destpath $destfmodpath"))
    {
        print "copied fmodel with HWCL : $updhwcl{$chip} and SWCL $updswcl{$chip} to lastKnownGoodFmod location\n";
    } 

    return;
}

# extract latest fmodel
sub extractFmodel {
    my ($chip) = @_;
    # Fmodel validation log file
    open(LOGFILE, "+>>$masterLogFileThisRun");


    chdir "$fmodlocation{$chip}" or die "cannot change directory: $!\n";

    print "$fmodlocation{$chip}\/build_$chip.tgz doesn't exist \n" if(! -f "$fmodlocation{$chip}\/build_$chip.tgz");
    my @tarlog = `tar -zxvf $fmodlocation{$chip}\/build_$chip.tgz`;

    # update log file
    print LOGFILE @tarlog;

    # Earlier there were two directories "clib/Linux" and "fmod/lib/Linux"
    # which contained all the .so files. New directories keep getting added
    # which contain more .so files, and we'll need to keep updating this script
    # to add these new directories to LD_LIBRARY_PATH before running the test.
    # Instead, lets move all the .so files to the current directory which we're in
    # i.e. $fmodlocation{$chip} i.e. where we copied and extracted the fmodel.
    print LOGFILE "\nFor Chip $chip, this is the log for moving all so files to parent directory:\n";

    if (! HasBug(622825))
    {
        # Eventually, this should work, when bug 622825 is fixed: moving from all subdirectories @ 1 go.
        my @moveLog = `find . -iname "*.so" | xargs -t -iTHISSOFILE mv THISSOFILE . 2>&1`;
        print LOGFILE @moveLog;
    } else {
        my $fmodDir = $fmodlocation{$chip};
        my $project = $projectName{$chip};
        my $regsysDirForThisChip = $regsys{$chip};
        if ($regsysDirForThisChip =~ "\/home\/scratch.regress04") {
            $fmodDir = $fmodDir."\/hw\/".$project;
        } elsif ($regsysDirForThisChip =~ "\/home\/scratch.autobuild1") {
            # Nothing, $fmodDir is where we'll find clib and fmod directories
        } else {
            die "Which fetch directory is this, neither regsys, nor ab2.? : $regsysDirForThisChip\n";
        }
        # move .so files from clib directory to current
        my @moveLog = `find $fmodDir/clib/ -iname "*.so" | xargs -t -iTHISSOFILE mv THISSOFILE . 2>&1`;
        print LOGFILE @moveLog;

        # move .so files from fmod directory to current
        @moveLog = `find $fmodDir/fmod/ -iname "*.so" | xargs -t -iTHISSOFILE mv THISSOFILE . 2>&1`;
        print LOGFILE @moveLog;

        if ($chip =~ "gf11") {
            # move .so files from uproc directory to current
            @moveLog = `find $fmodDir/uproc/ -iname "*.so" | xargs -t -iTHISSOFILE mv THISSOFILE . 2>&1`;
            print LOGFILE @moveLog;

            # move .so files from bin directory to current
            @moveLog = `find $fmodDir/bin/ -iname "*.so" | xargs -t -iTHISSOFILE mv THISSOFILE . 2>&1`;
            print LOGFILE @moveLog;
        }
    }

    close(LOGFILE);
}

# get Last submitted Tot P4 Changelist
sub getLastSubTotP4Cl() {

    my $lastCl = undef;
    my $lastClDate = undef;

    my $lastCldesc = `$p4spec changes -s submitted -m 1`;
    print $lastCldesc if ($debug);

    if( $lastCldesc =~ /Change ([0-9]+) on ([0-9\/]+) by .*/) {
       $lastCl = $1;
       $lastClDate = $2;
    } else {
       die "Failed to retrieve last CL\n";
    }
    print "Last Submitted Cl : $lastCl on $lastClDate\n" if ($debug);

    return $lastCl;
}

# sync to latest chipsa branch directory
sub syncChipsa {
    # Fmodel validation log file
    open(LOGFILE, "+>>$masterLogFileThisRun");

    print LOGFILE "\n\nSync the client spec ... $date \n\n";

    $lastSubTotSwCl = getLastSubTotP4Cl();
    my @p4synclog;
    if ( ! $dofast ) {
        @p4synclog = `$p4spec sync -f $ELW{P4ROOT}/sw/dev/gpu_drv/chips_a/... $ELW{P4ROOT}/sw/mods/... $ELW{P4ROOT}/sw/tools/...\@$lastSubTotSwCl 2>&1`;
    } else {
        @p4synclog = `$p4spec sync $ELW{P4ROOT}/sw/dev/gpu_drv/chips_a/... $ELW{P4ROOT}/sw/mods/...\@$lastSubTotSwCl 2>&1`;
    }
    print LOGFILE @p4synclog;

    # Sync sm2 files
    print LOGFILE "\n\nSync sm2 binaries and INI files.\n\n";
    @p4synclog = `$p4spec sync -f $sm2_bin/... $sm2_configdir/... 2>&1`;
    print LOGFILE @p4synclog;

    print LOGFILE "\n\nBelow are lwrrently opened files are not synced\n\n" ;
    my @p4openedfiles = `$p4spec opened 2>&1`;
    print LOGFILE @p4openedfiles;

    close (LOGFILE);
}

# perform a global make
sub buildChipsa {
    # Fmodel validation log file
    open(LOGFILE, "+>>$masterLogFileThisRun");
    print LOGFILE "\n\nCompilation is in progress ...\n\n" ;

    $ELW{"INCLUDE_LWWATCH"} = "false";
    chdir "$ELW{MODS_PATH}" or die "cannot change directory $ELW{MODS_PATH}: $!\n";
    if ( ! $dofast ) {
      my @makecleanlog = `make clean_all 2>&1`;
      print LOGFILE @makecleanlog;

      my @makeclobberlog = `make clobber_all 2>&1`;
      print LOGFILE @makeclobberlog;
    }

    my @makebuildlog = `make -j4 build_all 2>&1`;
    print LOGFILE @makebuildlog;

    print LOGFILE "\n\nBuild Completed ...\n\n" ;
    close (LOGFILE);
}

# LD_LIBRARY_PATH = fmodDir + MODS_RUNSPACE.
# Assumes fmodel store and untar are done at fmodel location
# And all so files are moved to the parent fmod directory where untar was done

sub setLdLiraryPath {
   my($chip) = @_;
   my $subdir = undef;
   my $fmoddir = undef;
   my $ldlibpath = undef;

   while (my ($key, $fmodloc) = each(%fmodlocation)) {

     # Skip if not this chip
     next if ($chip !~ m/$key/);

     print "\nChip Lib : $key, fmod dir : $fmodloc\n";

     $fmoddir = $fmodloc;
     last;
   }

   # no extracted fmods
   if (! defined ($fmoddir)) {
      die "Error : Fmodel $chip not extracted\n";
      $ELW{LD_LIBRARY_PATH} = "";
      print "\nLD PATH : ".$ELW{"LD_LIBRARY_PATH"};
   } else {
      # set LD_LIBRARY_PATH
      my $ld_path =  $fmoddir . ":" . $ELW{"MODS_RUNSPACE"};
      $ELW{LD_LIBRARY_PATH} = $ld_path;
      print "\nLD PATH : ".$ELW{"LD_LIBRARY_PATH"}."\n";
   }
}

# run rmtests to validate fmodel with latest files.
sub runRmTest {
    my ($chip, $modslog, $sm2logdir) = @_;
    
#   open(LOGFILE, "+>>$masterLogFileThisRun");
#    open(MODSLOG, "+> $modslog");

    my $modsrunspace = $ELW{"MODS_RUNSPACE"};

    chdir $sm2_bin or die "cannot change directory: $!\n";
    print "\nRunning rmtests now ..\n";
    print "java -jar sm2.jar -runlevel $inifile{$chip} -configdir $sm2_configdir -outdir $sm2logdir -SMODSRUNSPACE=$modsrunspace -verbose\n";
    system("java -jar sm2.jar -runlevel $inifile{$chip} -configdir $sm2_configdir -outdir $sm2logdir -SMODSRUNSPACE=$modsrunspace -verbose > $modslog");
    print "Completed running rmtests..\n";

#print LOGFILE @modsrunlog;
#print MODSLOG @modsrunlog;
#close(LOGFILE);
}


# Parse mods.log to find Passing Failing Skipped and Unsupported tests
# Match following sequence of string/line to say a test is passing/failing.
# Enter Run(TestName), No Unsupported test message,  Exit 0 : TestName ok => Passing
# Enter Run(TestName), Unsupported test message,  Exit 0 : TestName ok => Unsupported
# Enter Run(TestName), Exit ErrNumber : TestName ErrMesg => Failing
# Forcebly Skipping Test TestName => Skipped
sub parse_summary_log {
    my ($chip, $reportlog, $sm2logdir) = @_;

    open(SUMMARYLOG, "+< $sm2logdir/results.txt") or die "Couldn't open file $sm2logdir/results.txt : $!";

    print "Parsing result summary file $sm2logdir/results.txt\n";

    my $total = 0;
    my $passed = 0;
    my $failed = 0;
    my $skipped = 0;
    while(<SUMMARYLOG>)
    {
        if($_ =~ m/NoTest(\s*)PASSED/)
        {
           $noTestPassed{$chip} = 1;
        }

        if($_ =~ m/([\S]*)(\s*)PASSED(\s*)([\S]*)/)
        {
            $passed++;
            print " Passed, testname = $1, runtime = $4\n";
        }
        elsif($_ =~ m/([\S]*)(\s*)FAILED(\s*)([\S]*)/)
        {
            $failed++
        }
        elsif($_ =~ m/([\S]*)(\s*)SKIPPED(\s*)([\S]*)/)
        {
            $skipped++;
        }
    }
    close(SUMMARYLOG);

    # Copy summary in report file
    !system("cp $sm2logdir/results.txt $reportlog") or die "Couldn't copy file $sm2logdir/results.txt to $reportlog";

    print "chip = $chip, passed =  $passed, failed = $failed, skipped = $skipped\n";
    $total = $passed + $failed + $skipped;

    # Summary for final report
    my $chipDesRef = [$chip, $lastSubTotSwCl, $updhwcl{$chip}, $total, $passed, $failed, $skipped];
    push @runSummary, $chipDesRef;

    # Store the summary status for comparison to last run
    fillStatus($chipDesRef);

    if($failed == 0)
    {
        $noTestPassed{$chip} = 0;
    }
   
    if($total == 0 || $failed != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

sub delete_sm2logdir {
    my($sm2logdir) = @_;

    system("rm -rf $sm2logdir");
}

# Validate the compilation and dvs local run.
# and further email the status here.
sub checkBuildBreaks {

    my $compilerError = 0;
    my $emaillog = undef;
    my $status = "Build and Local DVS Run Succeded";

    open(LOGFILE, "<$masterLogFileThisRun");

    while(<LOGFILE>)
    {
        # check compilation failure
        if($_ =~ /^make\[\d\]: \*\*\* \[.*\] Error/){
            $compilerError = 1;
            $status = "Compilation failure";
        }

        if( /^Build Completed \.\.\./ ) {
            $compilerError = 0;
        }
    }
    close(LOGFILE);
}

sub pathToNetlist {
    my($chip) = @_;
    my $fullpath = undef;

    $fullpath = $ELW{"P4ROOT"};
    $fullpath = "$fullpath"."\/$netlists{$chip}";

    return $fullpath;
}

sub readHwclFromNetlist {
    my($chip) = @_;
    my $hwcl = undef;
    my $netlistfile = pathToNetlist($chip);

    open(NETLIST, "< $netlistfile") or die "Can\'t open file $netlistfile : $!";

    while(<NETLIST>) {
        if (/\#define.*(\w*)_NETLIST_DVS\s*999\s*\/\/\s*hw\@(\w*)\s*sw\@(\w*)/) {
            $hwcl = $2;
            last;
        }
    }
    close(NETLIST);

    die "Invalid netlist file  $netlistfile " if (!defined $hwcl);

    return $hwcl;
}


#
# update with last hwcl from file if hwcl not defined.
# set with latest hwcl from file if hwcl defined.
# While adding new chip add chip in previousFmodUpdatedList file too
sub FmodUpdHistory {

    my ($chip, $hwcl) = @_;
    my $rethwcl = undef;

    if( open(FMOD, "< $previousFmodUpdatedList") ) {
        my @fmodAttempt = <FMOD>;
        close(FMOD);

        open(FMODN, "> $previousFmodUpdatedList") or die "Could not open file $previousFmodUpdatedList : $!";
        foreach my $line (@fmodAttempt) {
            # append hwcl at the end.
            if ($line =~ /$chip/i) {
                if (defined $hwcl) {
                    chomp($line);
                    $line = "$line"." "."$hwcl";
                    print "Updated .fmodelAttempted file For chip : $chip hwcl : $hwcl\n";
                    print FMODN "$line\n";
                } elsif($line =~/$chip.*\s([0-9][0-9]*)\s*$/) {
                    $rethwcl = $1;
                    print "Read last attempted chip : $chip hwcl : $rethwcl\n";
                    print FMODN "$line";
                }
            } else {
                print FMODN "$line";
            }
        }
        close(FMODN);

    } else {
        system("touch $previousFmodUpdatedList");
    }
    close(FMOD);
    return $rethwcl;
}

# Update the NETLIST.H with NEWCL's
sub updateNetlistFile {
    my($chip) = @_;
    my $lwrnetlist = pathToNetlist($chip);
    my $line = undef;

    # read current HWCL and update statistics file.
    my $lasthwcl = readHwclFromNetlist($chip);

    if ($noTestPassed{$chip})
    {
        $hwClToUpd = $lasthwcl;
    }
    else
    {
        $hwClToUpd = $updhwcl{$chip};
    }

    # Dont do this here. We've updated the history already before we tried rmtests - both pass and fail cases
    #FmodUpdHistory($chip, $updhwcl{$chip});

    # Do edit netlists.h, add in HWCL and SWCL.
    print "Updating newer hwcl and swcl to $chip netlist file\n";
    open(NET, "< $lwrnetlist") or die "Could not open netlist file $lwrnetlist : $!";
    my @net = <NET>;
    close(NET);

    open(NETW, "> $lwrnetlist" ) or die "Could not open file $lwrnetlist :$!";
    for( $a = 0; $a < @net; $a = $a + 1 )
    {
        $line = $net[$a];
        if ($line =~ m/regsys built fmodel for Linux/)
        {
            if ($net[$a + 1] !~ m/Gpu BooTs fine/)
            {
                print NETW $line;
                $line = "// fmgbt   HW changelist # for which Gpu BooTs fine\n";
                print "Added     : $line";
            }
            print NETW $line;
            print $line
        }
        elsif($line =~ /^#define.*$external2InternalChip{$chip}*_NETLIST_DVS\s*999\s*\/\/\s*hw\@(\w*)\s*sw\@(\w*)\s*fmpc\@(\w*)\s*fmlx\@(\w*).*$/i) 
        {
            print "Previous: $line";
            # Prafull I've changed this line.
            #$line  = "#define ".uc($chip)."_NETLIST_DVS 999 // hw@"."$updhwcl{$chip}"." "."sw@"."$updswcl{$chip} "."fmpc\@$3 "."fmlx@"."$updhwcl{$chip}\n";
            $line  = "#define ".$external2InternalChip{$chip}."_NETLIST_DVS 999 // hw@"."$hwClToUpd "."sw@"."$updswcl{$chip} "."fmpc\@$3 "."fmlx@"."$hwClToUpd "."fmgbt@"."$updhwcl{$chip}\n";
            print "New     : $line";
            print NETW $line;
        }
        else
        {
            print NETW $line;
            print "$line";
        }
    }
#    fsync(\*NETW) or die "fsync: $!";

    close(NETW);
}

# Update NETLISTS.H with NEWCL description for HW and SW.
sub p4GetNewChangeListNumber {
    my $filesInCl = undef;

    open(LOGFILE, "+>>$masterLogFileThisRun");

    # Do p4 change -i < istream, read new CL.
    # Change 2980344 created with 1 open file(s).
    $newClNetlist = `$p4spec change -i < $logDirForThisRun\/$changelistfile`;
    if ( $newClNetlist =~ m/Change\s*(\w*)\s*created\s*with\s*(\w*)\s*open\s*file/) {
        $newClNetlist = $1;
        $filesInCl = $2;
    } else {
        print "Eror while creating new CL : $newClNetlist with no file opened";
        $newClNetlist = -1;
        $filesInCl = 0;
    }

    if ( !($#needupdchips eq $filesInCl) ) {
        print "Error: Fmodels to be updated != Number of files in this Changelist \n";
    }

    print LOGFILE "\n\tNew netlists.h Cl : $newClNetlist\n" if $debug;

    close (LOGFILE);

    return $newClNetlist;
}

# Perform Bundle submission.
sub submissionForAS2 {

    open(LOGFILE, "+>>$masterLogFileThisRun");

    my $path2netlist = dirname(pathToNetlist($needupdchips[0]));

    chdir "$path2netlist" or die "cannot change directory: $path2netlist : $!\n";

#    print "User : $ELW{P4USER} -c $ELW{P4CLIENT} \n";
#    my @Info = `$as2spec -r test 2>&1`;
#    print @Info;
    my $as2CmdForSubmit = "$as2spec submit -f -D \"fmod update for DVS use, Does not affect any builds or tasks\" -c $newClNetlist < $fmodUpdateGlobalData/chipsa/yForAutoSubmit 2>&1";
    print LOGFILE "\n$as2CmdForSubmit\n";
    #my @bundleInfo = "skipping as2 submission\n";
    my @bundleInfo = system($as2CmdForSubmit);

    print LOGFILE @bundleInfo;

    close(LOGFILE);
}

#
# Subroutine to check if the passed bug no. is present in the global hasBugArray
# If it is, then returns 1, else returns 0
#
sub HasBug
{
    # Get the bug no. to search for from the passed argument @_
    my($bugNoToSearchFor) = @_;
    foreach my $bugNoFromHasBugArray (@hasBugArray)
    {
        # if the passed bug no. matches the bug no. in hasBugArray, return 1
        if ($bugNoToSearchFor == $bugNoFromHasBugArray)
        {
            return 1;
        }
    }

    # bug no. not found in the hasBugArray, so return 0
    return 0;
}


# Subroutine to install a clean copy of MODS to MODS_RUNSPACE
sub InstallCleanModsToModsRunspace
{
    # Change directory to MODS build dir, i.e. MODS_PATH
    chdir "$ELW{MODS_PATH}" or die "cannot change directory $ELW{MODS_PATH}: $!\n";

    # Install MODS
    my @makeInstallLog = `make build_install 2>&1`;
    print LOGFILE @makeInstallLog;
}
#
# Verifies whether fmodel is flaky? then return true or false.
# Steps for checking...
# 1. copy, untar and set libary path to new fmodel taken from regsys
# 2. Run rmtests and create log files mods_chip.log.
# 3. Parse mods log to get report_chip.log
# 4. See what tests are failing.
sub IsLocalRmtestRunOk {
    my $status = 0;
    my($chip, $logDirForThisRun) = @_;

    my $modslog = "$logDirForThisRun"."\/mods_$chip.log";
    my $reportlog = "$logDirForThisRun"."\/report_$chip.log";
    my $sm2logdir = "$logDirForThisRun"."\/sm2logs_$chip";

    copyfmod($chip) ;

    extractFmodel($chip);

    setLdLiraryPath($chip);

    # Check if we need to care about bug 561479, if yes then install a
    # clean copy of MODS before running rmtests.
    if (HasBug(561479))
    {
        InstallCleanModsToModsRunspace();
    }

    runRmTest($chip, $modslog, $sm2logdir);

    $status = parse_summary_log($chip,$reportlog,$sm2logdir);

    # SM2 is generating a huge log directory. We anyway copy all mods log in chip_mods.log
    # Delete the log directory.
    delete_sm2logdir($sm2logdir);

    return $status;
}


# Builds the list of fmodels and their status.
# status.log file contents
# fmodel name : passing : failing : unsupported
# 1. Clean up log directory.
# 2. Check if newer fmodel available than attempted.
# 3. Run rmtest locally on fmodels one by one.
# 4. Update status.log(merged report_chip.log) file and needupdchips.
sub fillListOfNeed2UpdChips {
   my ($logd) = @_;
   my ($lasthwcl, $newhwcl) = undef;
   my $retvalue = undef;
   my $lastsubmit = undef;
   my $tempstr = undef;
#  opendir(LOGDIR, $logd) or die "Can't open dir $logd : $!";
#  while($file = readdir(LOGDIR)) {
#      next if -d $file;
#      unlink($logDirForThisRun.$file);
#  }
   
   open(REPRT, "> $statusfile") or die "Can\'t open $statusfile :$!";

   foreach my $chip (keys %regsys) {

      next if ! defined($chip);

      #Skip if arg passed chip does not matches current chip
      next if($argsChip ne 'all' and $argsChip ne $chip);

      $lasthwcl = FmodUpdHistory($chip, $lastsubmit);
      $newhwcl = $updhwcl{$chip};

      print "lasthwcl = $lasthwcl, newhwcl = $newhwcl\n";

      # new fmodel to updated
      if($lasthwcl < $newhwcl) {
          # update history file that we're trying with this fmodel. Both passing and failing fmods are updated in the history file so that they're not repeated.
          FmodUpdHistory($chip, $updhwcl{$chip});
          if ($chip eq "s000") 
          {
              $retvalue = 0;
          }
          else
          {
              $retvalue = IsLocalRmtestRunOk($chip, $logd);
          }
          
          if ( $retvalue ) 
          {
              push(@sanityfailedchips, $chip);
              if ( $noTestPassed{$chip} )
              {
                      push(@needupdchips, $chip);
              }
          }
          else
          {
              push(@needupdchips, $chip);
          }

          # appending mods run log here
          my $reportfile = "$logd"."\/report_$chip.log";
          open(RPT, "<$reportfile");

          print REPRT "+----------------------------------------------------+\n";
          $tempstr = sprintf("|  RMTEST STATUS ON FMODEL : %6s : HWCL : %7d |\n", uc($chip), $newhwcl);
          print REPRT $tempstr;
          print REPRT "+----------------------------------------------------+\n";

          while(<RPT>) {
              print REPRT $_;
          }
          close(RPT);

      # no new fmodel available
      } else {
          push(@up2datechips, $chip);
      }
   }
   close(REPRT);
}

#
# Get the netlist files ready for as2submit for need2updchips.
# 1. p4 revert all opened files.
# 2. p4 force sync all chip files.
# 3. p4 edit only need2 upd chips.
# 4. Update netlists with new hwcl and swcl.
sub updateNetlistFiles {

    open (P4SUB, "> $logDirForThisRun/p4submit.log") or die "can't open file $logDirForThisRun/p4submit.log : $!";

    p4revertFiles();

    # sync and then edit for needupdchips
    p4EditFiles();

    foreach my $chip (@needupdchips) {
       if (defined($chip)) {
           updateNetlistFile($chip);
       }
    }

    close(P4SUB);
}


# 1. Create changelist description file.
# 2. Create CL and get CL number with CL description file.
# 3. Perform as2 submit for this.
sub prepareCL4Submit {

    my $clfile = "$logDirForThisRun"."\/"."$changelistfile";

    p4CreateNewChangelist($clfile);

    my $clno = p4GetNewChangeListNumber();
    if($clno == -1)
    {
        return;
    }
    submissionForAS2();

    if ( ! $dofast ) {
        system("sleep 5s");
    }

    p4revertFiles();

    return;
}

sub fillStatus {
    my ($st) = @_;
    my $chip = $st->[0];
    my $logfile = "$globalLogDirThisBranch/status_$chip.log";
    
    if(-e $logfile) 
    {
        open(PRELOG, "<$logfile") or die "Can\'t open file $logfile : $!";
        my @lines = <PRELOG>;
        my $oldpass = undef;
        my $oldfail = undef;
        for my $line(@lines)
        {
            print split(" : ",$line);
            if((split(" : ",$line))[0] eq "Passing")
            {
                $oldpass = (split(" : ",$line))[1];
            }
            if((split(" : ",$line))[0] eq "Failing")
            {
                $oldfail = (split(" : ",$line))[1];
            }
        }
        close(PRELOG);
        if($st->[5] == 0 && $st->[3] != 0)
        {
            if($st->[4] > $oldpass || $st->[5] < $oldfail)
            {
                $finalChipStatus{$chip} = "New Success";            
            }
            else
            {
                $finalChipStatus{$chip} = "Old Success";            
            }
            # update the last known good fmodel
            my $src = $lastKnownGoodFmodLoc."\/temp.build_".$chip."_$st->[0]".".tgz";

            my $dest = $lastKnownGoodFmodLoc."\/build_$chip" . "_$st->[0].tgz";
            my $delete_loc = $lastKnownGoodFmodLoc."\/build_$chip" . "_*";

            print "Change name of fmodel from temp to actual and delete the old fmodels Now...";
            if( (-e $src) && !system("mv $src $dest") )
            {
                chomp($delete_loc);
                my @delete_files = `ls $delete_loc`;
                @delete_files = sort(@delete_files);

                # delete all old good fmodels.
                for(my $i=0; $i < scalar(@delete_files)-1; $i++)
                {
                    if(!system("rm $delete_files[$i]"))
                    {
                        print "succesfully deleted fmodel $delete_files[$i]";
                    }
                }

            }
        }
        else
        {
            if($st->[5] > $oldfail || ($st->[4]==0 && $st->[4]!=$oldpass))
            {
                $finalChipStatus{$chip} = "New Failure";
            }
            else
            {
                $finalChipStatus{$chip} = "Old Failure";
            }
            my $src = $lastKnownGoodFmodLoc."\/temp.build_".$chip."_$st->[0]".".tgz";
            if((-e $src) && !system("rm $src"))
            {
                print "Deleted fmodel $src as it causes failures";
            }
        }
    }
    open(PRELOG, "> $logfile"); #or die "Can\'t open file $logfile : $!";
    print PRELOG "SWCL : $st->[1]\n";
    print PRELOG "HWCL : $st->[2]\n";
    print PRELOG "Toatl : $st->[3]\n";
    print PRELOG "Passing : $st->[4]\n";
    print PRELOG "Failing : $st->[5]\n";
    print PRELOG "Skipped : $st->[6]\n";
    close(PRELOG);
}

# Store highlevel info which webpage will refer
sub webpage_info {

    my $allf = "$globalLogDirThisBranch/fmodhistory_all.log";
    open(FILE_ALL, "+>>$allf");
    print FILE_ALL "\ndirectory     $direc\n";

    foreach my $chipStatRef (@runSummary)
    {
        next if !defined($chipStatRef);

        my $chip = $chipStatRef->[0];
        # This data goes in history files which are used by webpage, don't change the formate
        print FILE_ALL "$chip     $updhwcl{$chip}     $finalChipStatus{$chip}\n";
        my $f = "$globalLogDirThisBranch/fmodhistory_$chip.log";
        open(FILE_CHIP, "+>>$f");
        print FILE_CHIP "$direc     $updhwcl{$chip}     $finalChipStatus{$chip}     $lastSubTotSwCl\n";
        close(FILE_CHIP);
    }
    close(FILE_ALL);
}

# email the status on fmodel updates
# body of email stored in file logdir/email.html
sub send_email {
    
    open(MAIL, "> $emailLog") or die "Can\'t open file $emailLog : $!";

    print MAIL "<html><body>";

    print MAIL "\[ This is an autogenerated email, please do not reply to rmtest-autocheckin \]";

    my $table1 = "<br><br><h3 align=\"left\">Fmodel update status:<h3>";
    $table1 = $table1."<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\" align=\"left\" bgcolor=\"#F5F5DC\">";
    $table1 = $table1."<tr><th>CHIP</th><th>SWCL</th><th>HWCL</th><th>STATUS</th></tr>";

    my $table2 = "\n\n\n";
    $table2 = $table2."\n\n<h3 align=\"left\">Summary of rmtests run on chips_a branch:<h3>";
    $table2 = $table2."\n<h3 align=\"left\">Log file path: /home/scratch.sw-rmtest-autoCheckin_sw/fmodUpdateData/logs/chipsa/$direc<h2>";
    $table2 = $table2."<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\" align=\"left\" bgcolor=\"#F5F5DC\">";
    $table2 = $table2."<tr><th>CHIP</th><th>TOTAL</th><th>PASS</th><th>FAIL</th><th>SKIPPED</th></tr>";

    # Polulate result summary tables to show in email.
    foreach my $chipStatRef (@runSummary) {
        next if !defined($chipStatRef);

        $table1 = $table1."<tr><td>$chipStatRef->[0]</td><td>$chipStatRef->[1]</td><td>$chipStatRef->[2]</td>";
        if ($chipStatRef->[3] != 0 && $chipStatRef->[5] == 0){
            $table1 = $table1."<td bgcolor=\"green\">PASSED</td></tr>";
        }
        else {
            $table1 = $table1."<td bgcolor=\"red\">FAILED</td></tr>";
        }

        #my $summarylink = "<a href = http://rmpune/fmod/data.php?chip=$chipStatRef->[0]&date=$direc&type=report> here</a>\n";
        #my $loglink = "<a href = http://rmpune/fmod/data.php?chip=$chipStatRef->[0]&date=$direc&type=modslog> here</a>\n";
        #my $summarylink = "<td $direc/report_$chipStatRef->[0].log </td>\n";
        #my $loglink = "<td $direc/mods_$chipStatRef->[0].log </td>\n";
        #$table2 = $table2."<tr><td>$chipStatRef->[0]</td><td>$chipStatRef->[3]</td><td>$chipStatRef->[4]</td><td>$chipStatRef->[5]</td><td>$chipStatRef->[6]</td><td>$summarylink</td><td>$loglink</td></tr>";
        $table2 = $table2."<tr><td>$chipStatRef->[0]</td><td>$chipStatRef->[3]</td><td>$chipStatRef->[4]</td><td>$chipStatRef->[5]</td><td>$chipStatRef->[6]</td></tr>";
    }
    $table1 = $table1."</table><br><br><br><br><br>";
    $table2 = $table2."</table><br><br><br><br>";

    print MAIL $table1;
    print MAIL $table2;

    #print MAIL "For more information go to this link http://rmpune";
    print MAIL "</body></html>";
    close(MAIL);

    # Get the list of members to which this email needs to be sent.
    my $email_send_failed = undef;
    my $email_send_failed_cc = undef;
    my $email_send_passed = undef;

    $email_send_failed_cc = "sw-gpu-rm-autofmodupdate-notify\@exchange.lwpu.com";
    foreach my $chip (@sanityfailedchips) {
        if(defined($chip)){
            $email_send_failed = "$email_send_failed"." "."$projectMgr{$chip}";
        }
    }

    foreach my $member (@email_monitor_list)
    {
        $email_send_passed = "$email_send_passed"." "."$member";
        $email_send_failed = "$email_send_failed"." "."$member";
    }

    # Send email now.
    if($#sanityfailedchips)
    {
        system("mutt -e \"my_hdr Content-Type: text/html\" -s \"[Status]Auto Fmod Update Results <failed>\" -c $email_send_failed_cc $email_send_failed < $emailLog ");
    }
    else
    {
        system("mutt -e \"my_hdr Content-Type: text/html\" -s \"[Status]Auto Fmod Update Results <passed>\" $email_send_passed < $emailLog ");
    }
    print "\nSent email succesfully\n";
}

#delete log files older than 6 months.
sub deleteOldDir {

    my $dir = "$ELW{fmodUpdateData}/logs/chipsa";

    my $lwrDir = fastgetcwd;
    chdir($dir) or die "Cant chdir to $dir $!";

    my $dirForDelete = "*.".(($month-2)%12).".$dayofm."."*".".*."."*";

    my @arrayOfDir = `ls -d $dirForDelete`;

    foreach my $dirname (@arrayOfDir) {
        system("rm -r $dirname");
    }

    chdir($lwrDir) or die "Cant chdir to $lwrDir $!";
}

sub processArgs {
    if (defined $ARGV[0]) {
        $argsChip = $ARGV[0];
    }

}


# Main Task Routine to get the work done.
sub regsys2dvs {

    # Process passed arguments
    processArgs();

    # get available hwcl from regsys
    fillLatestHwcl();

    verifyelwironment();

    configureElwironment();

    # delete old log file
    if ( -e $masterLogFileThisRun ) {
      unlink($masterLogFileThisRun);
    }

    p4revertFiles();

    syncChipsa();

    buildChipsa();

    fillListOfNeed2UpdChips($logDirForThisRun);

    updateNetlistFiles();

    prepareCL4Submit();
    
    if($#sanityfailedchips || $#needupdchips){
        webpage_info();
        send_email();
        deleteOldDir();
    }

}

sub main {
    # The scratchspace is getting filled by core.<some number> files appearing in MODS_RUNSPACE. Each file around 1GB, the scratchspace goes out of space frequently and
    # the scripts start to fail due to no disk space. These files seem to be process memory images. We don't need them, until we find a right way to stop their
    # unnecessary creation, lets keep a command here in the script itself to delete these files.

    #system("rm -f $MODS_RUNSPACE/core.*");
    system("rm -f $ELW{MODS_RUNSPACE}/core.*");

    regsys2dvs();
}

&main();
