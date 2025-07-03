#!/home/gnu/bin/perl -w
#
# updallfmods.pl:  On any chipa sw tree, with all elw variables set, 
# Script submits newer fmodels to DVS.

use strict;
no strict "refs";
use IO::File;
use File::Basename;

die "syntax:  updallfmods.pl " if @ARGV != 0;

# Config Var's
my $date = `date`;

# place to dump out all the log files:
# Create the directory with time stamp
 my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
$yr += 1900;
my $logdir = "/home/vyadav/fmodUpdateLog/bugfix_main/log_".$yr.$month.$dayofm.$$hr.$min.$sec;
# Create Directory
mkdir($logdir, 0755) || die "Not able to create directory $logdir\n";
# Master Logfile taken out of p4 sync and running sim mods on each chips.
# similar to qsub.log in cron_fmodupd.sh 
my $validatefmodellog = "$logdir"."\/bugfix_main.log";
my $fmodelList = $ELW{HOME}."/.fmodelAttemptedBugfix_main";
my $changelistfile = "changelist.txt";
my $statusfile = "$logdir"."\/report.log";
my $emailLog = "$logdir"."\/email.log";
my $debug = 1;
my $dofast = 0;
my @hasBugArray = (0 , 0);


# Test Name Arr's
my @passTests = undef;
my @failTests = undef;
my @unsuTests = undef;
my @skipTests = undef;

# Test Count Var's
my ($totalTc, $passTc, $failTc, $unsuTc, $skipTc) = undef;
my @runSummary = undef;


# Global Var's.
my $lhwcl = undef;
my $lswcl = undef;
my $newClNetlist = undef;
my @cl_desc = undef;
my $lastSubTotSwCl = undef; 

my $p4spec = undef;
my $as2spec = undef;


my @needupdchips = undef;
my @sanityfailedchips = undef;
my @up2datechips = undef;
my %fmodinregsys = undef;

my %regsys =  (
#   "gt212" => "/home/scratch.regress04/linux50/tesla2_tree",
#   "gt215" => "/home/scratch.regress04/linux50/tesla2_gt215",
#   "gt216" => "/home/scratch.regress04/linux50/tesla2_gt216",
#   "gt218" => "/home/scratch.regress04/linux50/tesla2_gt218",
   "igt21a" => "/home/scratch.regress04/linux50/tesla2_tree",
#   "gf100" => "/home/scratch.regress04/linux50/fermi1_gf100",
   "gf104" => "/home/scratch.regress04/linux50/fermi1_gf10x",
   "gf106" => "/home/scratch.regress04/linux50/fermi1_gf10x",
);

my %fmodlocation =  (
#   "gt212" => "/home/scratch.vyadav_fermi-arch/src/t1",
#   "gt215" => "/home/scratch.vyadav_fermi-arch/src/t1",
#   "gt216" => "/home/scratch.vyadav_fermi-arch/src/t1",
#   "gt218" => "/home/scratch.vyadav_fermi-arch/src/t1",
   "igt21a" => "/home/scratch.vyadav_fermi-arch/src/t1",
#   "gf100" => "/home/scratch.vyadav_fermi-arch/src/t1",
   "gf104" => "/home/scratch.vyadav_fermi-arch/src/t1",
   "gf106" => "/home/scratch.vyadav_fermi-arch/src/t1",
);

my %netlists = (
#   "gt212" => "//sw/dev/gpu_drv/bugfix_main/drivers/resman/kernel/inc/gt212/net/netlists.h",
#   "gt215" => "//sw/dev/gpu_drv/bugfix_main/drivers/resman/kernel/inc/gt215/net/netlists.h",
#   "gt216" => "//sw/dev/gpu_drv/bugfix_main/drivers/resman/kernel/inc/gt216/net/netlists.h",
#   "gt218" => "//sw/dev/gpu_drv/bugfix_main/drivers/resman/kernel/inc/gt218/net/netlists.h",
   "igt21a" => "//sw/dev/gpu_drv/bugfix_main/drivers/resman/kernel/inc/igt21a/net/netlists.h",
#   "gf100" => "//sw/dev/gpu_drv/bugfix_main/drivers/resman/kernel/inc/fermi/gf100/net/netlists.h",
   "gf104" => "//sw/dev/gpu_drv/bugfix_main/drivers/resman/kernel/inc/fermi/gf104/net/netlists.h",
   "gf106" => "//sw/dev/gpu_drv/bugfix_main/drivers/resman/kernel/inc/fermi/gf106/net/netlists.h",
);

my %updhwcl = (
#   "gt212" => 0,
#   "gt215" => 0,
#   "gt216" => 0,
#   "gt218" => 0,
   "igt21a" => 0,
#   "gf100" => 0,
   "gf104" => 0,
   "gf106" => 0,
);

my %updswcl = (
#   "gt212" => 0,
#   "gt215" => 0,
#   "gt216" => 0,
#   "gt218" => 0,
   "igt21a" => 0,
#   "gf100" => 0,
   "gf104" => 0,
   "gf106" => 0,
);



# Validate Script environment requirements
sub verifyelwironment {
    my $okClient = 1;

    if($ELW{HOST} !~ /^l-xterm-[0-9][0-9]/) {
        print "Fmodel submission can occur only on l-xterms\n";
        print "As regsys database is invisible in other forms\n";
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

    # Load destination fmodel location
    $p4spec = "/home/utils/bin/p4 -c $ELW{P4CLIENT} -p $ELW{P4PORT}";
    $as2spec = "p4";
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

    $descline = "\n\tBug 478135\n\tBug 421682\n\tBug 540544\n\tBug 565272\n";
    push (@cl_desc, $descline);
    $descline = "\treviewed	by : tpurankar\n";
    push(@cl_desc, $descline);
               
    return \@cl_desc;
}

sub p4EditFiles {
    my $editfiles = undef;
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

    return; 
}


sub p4revertFiles {
    my $revertfiles = undef;

    foreach my $chip (keys %netlists)  {
       $revertfiles = "$revertfiles"." "."$netlists{$chip}"; 
    }

    my @revertlog = `$p4spec revert $revertfiles 2>&1`;

    print @revertlog;
    print P4SUB @revertlog;

    return;
}

sub getFilesInCL {
   my @filesInCl = undef;

    foreach my $chip (@needupdchips)  {

       next if !defined($chip);
       push(@filesInCl, $netlists{$chip}); 

    }

    print "Filese in CL";
    print P4SUB @filesInCl;
    
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

    while ($subdir = readdir(DIR)) {

        # Skip if it's non-numeric.
        next if $subdir !~ m/^[0-9]+$/;

        # Skip if it's not a directory, or if there doesn't appear to be an
        # fmodel DLL inside.
        next if ! -d "$dir/$subdir";
        next if (! -f "$dir/$subdir/$chip"."_fmodel/build.tgz");

        push @builds, $subdir;
    }
    closedir(DIR);
    return @builds;
}


sub getlatesthwclregsys {
    my($chip) = @_;
    my $regsysPath = undef; 

    $regsysPath =  $regsys{$chip} ;

    if ( !defined($fmodinregsys{$chip}) ) {
        my @list = getbuilds($regsysPath, $chip);
        my @fmodlist = reverse sort(@list);    
        $fmodinregsys{$chip} = \@fmodlist;
    }
    
    print "regsys : $regsysPath  chip : $chip  hwcl : $fmodinregsys{$chip}->[0]\n" if $debug;
      
    return $fmodinregsys{$chip}->[0];
}

sub fillLatestHwcl {
   # fill latest hwcl for each chip
   foreach my $chip (keys %updhwcl) {
      $updhwcl{$chip} = getlatesthwclregsys($chip);
   }
}

# get the latest fmodel  
sub getlatestfmodpath {
    my ($chip) = @_;
    
    my $fmodpath = $regsys{$chip}."\/".$updhwcl{$chip}."\/".$chip."_fmodel\/build.tgz";

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
    
    my $destpath = $fmodlocation{$chip}."\/build_$chip.tgz"; 
    my $srcfmodpath = getlatestfmodpath($chip);
     
    # delete current fmodel if any
    if ( -e $destpath) {
        unlink($destpath);
    }
    
    print("cp $srcfmodpath $destpath\n");
    
    if(!system("cp $srcfmodpath $destpath"))
    {
        fillSwclForChipHwcl($chip, $updhwcl{$chip});
        print "copied fmodel with HWCL : $updhwcl{$chip} and SWCL $updswcl{$chip}\n";
    }
    
    return; 
}

# extract latest fmodel 
sub extractFmodel {
    my ($chip) = @_; 
    # Fmodel validation log file
    open(LOGFILE, "+>>$validatefmodellog");
    
    print "system(rm -rf $fmodlocation{$chip}/hw/tesla2*)\n" if $debug;
    system("rm -rf $fmodlocation{$chip}/hw/tesla2*");
    print "system(rm -rf $fmodlocation{$chip}/hw/fermi1_*)\n" if $debug;
    system("rm -rf $fmodlocation{$chip}/hw/fermi1_*");
    
    chdir "$fmodlocation{$chip}" or die "cannot change directory: $!\n";
    
    print "$fmodlocation{$chip}\/build_$chip.tgz doesn't exist \n" if(! -f "$fmodlocation{$chip}\/build_$chip.tgz");
    my @tarlog = `tar -zxvf $fmodlocation{$chip}\/build_$chip.tgz`;
    
    # update log file
    print LOGFILE @tarlog;
    close(LOGFILE);
}

# get Last submitted Tot P4 Changelist
sub getLastSubTotP4Cl() {

    my $lastCl = undef; 
    my $lastClDate = undef; 

    my $lastCldesc = `/home/utils/bin/p4 changes -s submitted -m 1`;
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
    open(LOGFILE, "+>>$validatefmodellog");
    
    print LOGFILE "\n\nSync the client spec ... $date \n\n";
    
    $lastSubTotSwCl = getLastSubTotP4Cl();
    my @p4synclog = `$p4spec sync -f $ELW{P4ROOT}/sw/dev/gpu_drv/bugfix_main/...\@$lastSubTotSwCl 2>&1`;
#    my @p4synclog = `$p4spec sync $ELW{P4ROOT}/sw/dev/gpu_drv/bugfix_main/...\@$lastSubTotSwCl 2>&1`;
    print LOGFILE @p4synclog;
    
    print LOGFILE "\n\nBelow are lwrrently opened files are not synced\n\n" ;
    my @p4openedfiles = `$p4spec opened 2>&1`;
    print LOGFILE @p4openedfiles;
    
    close (LOGFILE);
}

# perform a global make
sub buildChipsa {
    # Fmodel validation log file
    open(LOGFILE, "+>>$validatefmodellog");
    print LOGFILE "\n\nCompilation is in progress ...\n\n" ;
   
    $ELW{"INCLUDE_LWWATCH"} = "false"; 
    chdir "$ELW{MODS_PATH}" or die "cannot change directory $ELW{MODS_PATH}: $!\n";
	if ( ! $dofast ) {
      my @makecleanlog = `make clean_all 2>&1`;
      print LOGFILE @makecleanlog;
	}

    my @makebuildlog = `make build_all 2>&1`;
    print LOGFILE @makebuildlog;

    print LOGFILE "\n\nBuild Completed ...\n\n" ;
    close (LOGFILE);
}

# LD_LIBRARY_PATH = clib path + fmod/lib path + MODS_RUNSPACE.
# Assumes fmodel store and untar are done at fmodel location
# Fmodel build.tgz extract will create this dir structure
# hw
# +- fermi1_gf100 tesla2_gt200 tesla2 # (fermi|tesla)[ 12]_($chip) full fmod name
# +-- clib
# +--- Linux
# +-- fmod
# +--- lib
# +---- Linux

sub setLdLiraryPath {
   my($chip) = @_;
   my $subdir = undef;
   my $fmoddir = undef;
   my $ldlibpath = undef;
   
   while (my ($key, $fmodloc) = each(%fmodlocation)) {
     
     # Skip if not this chip
     next if ($chip !~ m/$key/); 

     print "\nChip Lib : $key\n";

     # Skip if fmodel isn't extracted 
     next if (! -d $fmodloc."\/hw" ) ;
        
     opendir(DIR, $fmodloc."\/hw") or die "Can't open $fmodloc\/hw : $!";

     while ($subdir = readdir(DIR)) {
        # Skip if dir is not fermi1_xxx tesla2_xxx tesla2 tesla 
        next if $subdir !~ m/(fermi|tesla)[ 12]/;

        if ($chip =~ /(gt21[0-9a-f])/i) {
            if ( $subdir =~ m/tesla[ 12]_($chip)/) {
                $fmoddir = $subdir;
                last;
            } elsif ( ($chip =~ /igt/) && ($subdir =~ m/^tesla2$/) ) {
                $fmoddir = $subdir;
                last;
            } elsif ( $subdir =~ /tesla/ ) {
                $fmoddir = $subdir;
            }
        } elsif($chip =~ /(gf10[0-9]|gf11[0-9])/) {
            if ( $subdir =~ m/fermi[ 12]_($chip)/) {
                $fmoddir = $subdir;
                last;
            } elsif ( $subdir =~ /fermi[ 12]/ ) {
                $fmoddir = $subdir;
            }
        }
     }
     closedir(DIR);
   }

   # no extracted fmods
   if (! defined ($fmoddir)) {
      die "Error : Fmodel $chip not extracted\n"; 
      $ELW{LD_LIBRARY_PATH} = "";
      print "\nLD PATH : ".$ELW{"LD_LIBRARY_PATH"};
   } else {
      # set LD_LIBRARY_PATH
      my $clibdir = $fmodlocation{$chip} . "\/hw\/" . $fmoddir . "\/clib\/Linux"; 
      my $fmodlibdir = $fmodlocation{$chip} . "\/hw\/" . $fmoddir . "\/fmod\/lib\/Linux"; 
      
      my $ld_path =  $clibdir . ":" . $fmodlibdir . ":" . $ELW{"MODS_RUNSPACE"};
     $ELW{LD_LIBRARY_PATH} = $ld_path; 

      print "\nLD PATH : ".$ELW{"LD_LIBRARY_PATH"};
   }
}

# run rmtests to validate fmodel with latest files.
sub runRmTest {
        my ($chip, $modslog) = @_;
        
        open(LOGFILE, "+>>$validatefmodellog");

        open(MODSLOG, "+> $modslog");
        
    chdir "$ELW{MODS_RUNSPACE}" or die "cannot change directory: $!\n";

    print "sim.pl -chip $chip -fmod rmtest.js -dvs -run_on_error"; 
    my @modsrunlog = `$ELW{MODS_RUNSPACE}/sim.pl -chip $chip -fmod rmtest.js -dvs -run_on_error 2>&1`; 
    #system("$ELW{MODS_RUNSPACE}/sim.pl -chip $chip -fmod rmtest.js -dvs 2>&1"); 

    print LOGFILE @modsrunlog;
    print MODSLOG @modsrunlog;
    close(LOGFILE);
}


# Parse mods.log to find Passing Failing Skipped and Unsupported tests 
# Match following sequence of string/line to say a test is passing/failing.
# Enter Run(TestName), No Unsupported test message,  Exit 0 : TestName ok => Passing
# Enter Run(TestName), Unsupported test message,  Exit 0 : TestName ok => Unsupported
# Enter Run(TestName), Exit ErrNumber : TestName ErrMesg => Failing
# Forcebly Skipping Test TestName => Skipped 
sub parse_mods_log {
    my ($chip, $absModslogPath) = @_; 
    my $test = undef;           # Store Test Name under the log
    my $rmtestsRunBlock = 0;    # Flag to differentiate Gpu/Platform load log and rmtests run log
    my $insideTestlog = 0;      # Flag dictates EnterTest -- ExitTest 
    my $isSupportedTest =0;   # Flag marks whether test is already marked as unsupported
    my $modsRunCmdLine = undef; # Command line specified while running rmtests
    my $trackAssertFailLines= 0;# - Var to track ASSERT()
    my $line = 0;                 # For counting lines
    
    # free used arr's.
    $#passTests = 0;
    $#failTests = 0;
    $#unsuTests = 0;
    $#skipTests = 0;

    open(MODSLOG, "+< $absModslogPath") or die "Couldn't open file $absModslogPath : $!";

    while(<MODSLOG>)
    {
        $line++;
        if ( ! $rmtestsRunBlock ) 
        {
            if ( $_ =~ /rmtest.js\s*:\s*([0-9]+)/ ) {
                $totalTc = $1;
                $rmtestsRunBlock = 1;
            }
        
            if ( $_ =~ /Command Line : (rmtest.js.*)/) {
                $modsRunCmdLine = $1;
            }
        } 
        else 
        { 
            # While inside test run log ... 
            # /Exit (ErrorCode) : Global.(TestName) (okOrErrMsg)/ => Pass/Fail Test
            # /Skipping unsupported test (TestName)/ => Unsupported 
            # /Forcebly Skipping Test/ => Not Expected Invalid String: Ignore.
            # /Enter Global./ => Invalid log file contents
            if ( $insideTestlog ) 
            {
                # $1 match TestName 
                if ( /Exit 0 : Global\.Run(\w*) ok/ ) {
                    $test = $1;
                    print "Passing test : $test \n" if ( $debug && (!$isSupportedTest));
                    $insideTestlog = 0;
                    push(@passTests, $test) if (!$isSupportedTest);
                    $isSupportedTest = 0;
                }
                
                # $2 match TestName 
                if ( /Exit ([0-9][0-9]+) : Global.Run(\w*) (.*)$/ ) {
                    $test = $2;
                    print "Failing test : $test \n" if $debug;
                    $insideTestlog = 0;
                    push(@failTests, $test);
                }

                # $1 match TestName 
                if ( /Skipping unsupported test (\w*)/ ) {
                    $test = $1;
                    $isSupportedTest = 1;
                    print "Unsupported test : $test \n" if $debug;
                    push(@unsuTests, $test);
                }

                if ( /Enter Global\.(\w*)\s*$/ ) {
                    die "Error in mods.log file at $line : $_";
                }
            } 

            # While outside test run log ...
            # /Enter Global./ => Enter New test run log
            # /Forcebly Skipping Test (testName)/ => File Skipped Through Command line
            else 
            {

                if ( /Enter Global\.Run(\w*)\s*$/ ) {
                    $insideTestlog = 1;
                }

                if ( /Forcebly Skipping Test\s*:\s*(\w*)\s*$/ ) {
                    $test = $1;
                    print "Skipped test : $test \n" if $debug;
                    push(@skipTests, $test);
                }

            }
            # Sequence of following matches => Run failed with assert.
            # Successfully opened cmd.txt
            # Successfully opened gdb.txt
            #----------call stack----------
            #----------call stack----------
            if ((/----------call stack----------/) || 
                (/Successfully opened ....txt/)  ) {
                $trackAssertFailLines++;
            }

            if ($trackAssertFailLines eq 4) {
                last;
                close(MODSLOG);
            }
        }
    } #<end while>

    if ($#failTests ) {
        return $#failTests;
    } elsif ( $trackAssertFailLines eq 4 ) {
        return 1;
    } else {
        return 0;
    }
    close(MODSLOG);
}


sub print_report {
    my ($reportfile, $chip) = @_;
    my $tempstr = undef;
    my $i = undef;
    $totalTc = $#passTests + $#failTests + $#skipTests; 
    
    open(REPORT, "> $reportfile");
   
    if ($totalTc eq 0) {
        $tempstr = sprintf("Provided invalid mods.log file, may be no tests ran\n");
    print REPORT $tempstr;
        exit(0); 
    }
    
    print REPORT "+----------------------------------------------------+\n";
    print REPORT "|               1. PASSING TESTS                     |\n";
    print REPORT "+----------------------------------------------------+\n";
    
    for($i = 1 ; $i <= $#passTests ; $i++ ) {
        $tempstr = sprintf("|               %-37s|\n", $passTests[$i]);
        print REPORT $tempstr;
    }
    
    print REPORT "+----------------------------------------------------+\n";
    print REPORT "|               2. FAILING TESTS                     |\n";
    print REPORT "+----------------------------------------------------+\n";
    
    for($i = 1 ; $i <= $#failTests ; $i++ ) {
        $tempstr = sprintf("|               %-37s|\n", $failTests[$i]);
        print REPORT $tempstr;
    }
    
    print REPORT "+----------------------------------------------------+\n";
    print REPORT "|               3. UNSUPPORTED TESTS                 |\n";
    print REPORT "+----------------------------------------------------+\n";
    
    for($i = 1 ; $i <= $#unsuTests ; $i++ ) {
        $tempstr = sprintf("|               %-37s|\n", $unsuTests[$i]);
        print REPORT $tempstr;
    }

    print REPORT "+----------------------------------------------------+\n";
    print REPORT "|               4. SKIPPED TESTS                     |\n";
    print REPORT "+----------------------------------------------------+\n";
    
    for($i = 1 ; $i <= $#skipTests ; $i++ ) {
        $tempstr = sprintf("|               %-37s|\n", $skipTests[$i]);
        print REPORT $tempstr;
    }
    print REPORT "+----------------------------------------------------+\n";
    print REPORT "| Total | Passing | Failing | Skipped  | Unsupported |\n";
    print REPORT "+----------------------------------------------------+\n";
    $tempstr = sprintf("| %5d | %7d | %7d | %8d | %11d |\n", $totalTc, $#passTests, $#failTests, $#skipTests, $#unsuTests);
    print REPORT $tempstr;
    print REPORT "+----------------------------------------------------+\n";
    $tempstr = sprintf("|  %%age |   %03.2f|    %2.2f |     %2.2f |             |\n", ($#passTests * 100.00 / $totalTc), ($#failTests * 100.00 / $totalTc), ($#skipTests * 100.00 / $totalTc));
    print REPORT $tempstr;
    print REPORT "+----------------------------------------------------+\n";

    # Hold summary for final report 
    my $chipDesRef = [$chip, $totalTc, $#passTests, $#failTests, $#skipTests, $#unsuTests] ;
    push @runSummary, $chipDesRef;	

    close(REPORT);
}

# Validate the compilation and dvs local run.
# and further email the status here.
sub checkBuildBreaks {

    my $compilerError = 0;
    my $emaillog = undef;
    my $status = "Build and Local DVS Run Succeded";

    open(LOG, "<$ELW{HOME}/tmp/chipsa.log");

    while(<LOG>) 
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
    close(LOG);
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
#
sub FmodUpdHistory {

    my ($chip, $hwcl) = @_;
	my $rethwcl = undef;

    if( open(FMOD, "< $fmodelList") ) {
		my @fmodAttempt = <FMOD>;
		close(FMOD);
	
		open(FMODN, "> $fmodelList") or die "Could not open file $fmodelList : $!";
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
        system("touch $fmodelList");
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


    FmodUpdHistory($chip, $updhwcl{$chip});

    # Do edit netlists.h, add in HWCL and SWCL. 
    print "Updating newer hwcl and swcl to $chip netlist file\n";
    open(NET, "< $lwrnetlist") or die "Could not open netlist file $lwrnetlist : $!";
    my @net = <NET>; 
    close(NET);


 
    open(NETW, "> $lwrnetlist" ) or die "Could not open file $lwrnetlist :$!";

    foreach $line (@net) {

        if($line =~ /^#define.*$chip*_NETLIST_DVS\s*999\s*\/\/\s*hw\@(\w*)\s*sw\@(\w*)\s*fmpc\@(\w*)\s*fmlx\@(\w*)\s*$/i) {
            print "Previous: $line";
            $line  = "#define ".uc($chip)."_NETLIST_DVS 999 // hw@"."$updhwcl{$chip}"." "."sw@"."$updswcl{$chip} "."fmpc\@$3 "."fmlx@"."$updhwcl{$chip}\n";
            print "New     : $line";
            
            print NETW $line;
        } else {
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
    
    open(LOG, "+>>$ELW{HOME}/tmp/chipsa.log");
    
    # Do p4 change -i < istream, read new CL.
    # Change 2980344 created with 1 open file(s).
    $newClNetlist = `$p4spec change -i < $logdir\/$changelistfile`;
    if ( $newClNetlist =~ m/Change\s*(\w*)\s*created\s*with\s*(\w*)\s*open\s*file/) {
        $newClNetlist = $1; 
        $filesInCl = $2;
    } else {
        die "Eror while creating new CL : $newClNetlist";
    }

    if ( !($#needupdchips eq $filesInCl) ) {
        print "Error: Fmodels to be updated != Number of files in this Changelist \n";
    }
        
    print LOG "\n\tNew netlists.h Cl : $newClNetlist\n" if $debug;

    close (LOG);

    return $newClNetlist;    
}

# Perform Bundle submission.
sub submissionForAS2 {
    
    open(LOG, "+>>$ELW{HOME}/tmp/chipsa.log");

    my $path2netlist = dirname(pathToNetlist($needupdchips[0]));
    
    chdir "$path2netlist" or die "cannot change directory: $path2netlist : $!\n";

#    print "User : $ELW{P4USER} -c $ELW{P4CLIENT} \n";
#    my @Info = `$as2spec -r test 2>&1`;
#    print @Info;

    print LOG "\n$as2spec submit -c $newClNetlist 2>&1\n";
    my @bundleInfo = system("$as2spec submit -c $newClNetlist 2>&1");
#    my @bundleInfo = `/home/lw/bin/as2 -r submit -c $newClNetlist 2>&1`;

    print LOG @bundleInfo;

    close(LOG);
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
    my($chip, $logdir) = @_; 
    
    my $modslog = "$logdir"."\/mods_$chip.log";
    my $reportlog = "$logdir"."\/report_$chip.log";

    copyfmod($chip) ;

    extractFmodel($chip);

    setLdLiraryPath($chip);
	
	# Check if we need to care about bug 561479, if yes then install a
	# clean copy of MODS before running rmtests.
	if (HasBug(561479))
	{
	    InstallCleanModsToModsRunspace();	
	}

    runRmTest($chip, $modslog);

    $status = parse_mods_log($chip, $modslog);

    print_report($reportlog, $chip);

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
#      unlink($logdir.$file);
#  }
   open(REPRT, "> $statusfile") or die "Can\'t open $statusfile :$!"; 

   foreach my $chip (keys %regsys) {

      next if ! defined($chip);

      $lasthwcl = FmodUpdHistory($chip, $lastsubmit);
      $newhwcl = getlatesthwclregsys($chip);
      
      # new fmodel to updated
      if($lasthwcl < $newhwcl) {
          $retvalue = IsLocalRmtestRunOk($chip, $logd);    
          
          if ( $retvalue ) {
              push(@sanityfailedchips, $chip);
          } else {
              push(@needupdchips, $chip);
          }
          
          # appending mods run log here
          my $reportfile = "$logd"."\/report_$chip.log";
          open(RPT, "<$reportfile");

    	  print REPRT "\n\n\n+----------------------------------------------------+\n";
          $tempstr = sprintf("|  RMTEST STATUS ON FMODEL : %6s : HWCL : %7d |\n", uc($chip), $newhwcl);
          print REPRT $tempstr;
          
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

    open (P4SUB, "> $logdir/p4submit.log") or die "can't open file $logdir/p4submit.log : $!";
    
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
    
    my $clfile = "$logdir"."\/"."$changelistfile"; 

    p4CreateNewChangelist($clfile);

    p4GetNewChangeListNumber();

    submissionForAS2(); 
   
	if ( ! $dofast ) {
       system("sleep 1h");
    }

	p4revertFiles();

    return; 
}

# email the status on fmodel updates
# body of email stored in file logdir/email.log
# 
#
sub mailStatus {
    my $email_status = "mail -s \"[bugfix_main]Status of RMTESTS on TOT sync with latest regsys fmodels \" sw-rmtest\@lwpu.com < $emailLog" ;

    open(MAIL, "> $emailLog") or die "Can\'t open file $emailLog : $!";

	print MAIL "\[ This is an autogenerated mail \]\n\n";

    # Print Fmodel Update status
#   print MAIL "\nUpdating Newer Fmodels To DVS For :\n";
#   
#   foreach my $chip (@needupdchips) {
#       print MAIL "\t$chip   =>   $updhwcl{$chip}\n" if (defined($chip));
#   }
#   
#   print MAIL "\nLastest Fmodels Exist At DVS For  :\n";
#
#   foreach my $chip (@up2datechips) {
#       print MAIL "\t$chip   =>   $updhwcl{$chip}\n" if (defined($chip));
#   }
#
#   print MAIL "\nLocal DVS Run Failed For RegSys Fmods: \n";
#
#   foreach my $chip (@sanityfailedchips) {
#       print MAIL "\t$chip   =>   $updhwcl{$chip}\n" if (defined($chip));
#   }

    # 

    # Summary in the beginning 
    my ($chipStatRef, $tempstr) = undef;
    print MAIL "Summary of RMTESTS Run On bugfix_main branch at Changelist $lastSubTotSwCl :\n";
    print MAIL "+-------+----------------------------------------------------+\n";
    print MAIL "| Chip  | Total | Passing | Failing | Skipped  | Unsupported |\n";
    print MAIL "+-------+----------------------------------------------------+\n";
    foreach $chipStatRef (@runSummary) {
	 next if !defined($chipStatRef);
     $tempstr = sprintf("|%6s | %5d | %7d | %7d | %8d | %11d |\n", 
                 uc($chipStatRef->[0]), $chipStatRef->[1], $chipStatRef->[2], $chipStatRef->[3], $chipStatRef->[4], $chipStatRef->[5]);
     print MAIL $tempstr;
    }
    print MAIL "+-------+----------------------------------------------------+\n";
 
    # Append status 
    open(REP, "< $statusfile") or die "Can\'t open file $statusfile : $!";

    print MAIL "Detailed Report Below : \n";
    while(<REP>) {
        # ignore empty lines.
        next if /^\s*$/;

        print MAIL $_;
    }
    close(REP);     
         
    close(MAIL);
    system($email_status);
}


# Main Task Routine to get the work done.
sub regsys2dvs {

    # get available hwcl from regsys
    fillLatestHwcl();
    
    verifyelwironment();

    configureElwironment();

    # delete old log file
    if ( -e $validatefmodellog ) {
      unlink($validatefmodellog);
    }

    p4revertFiles();

    syncChipsa();

    buildChipsa();

    fillListOfNeed2UpdChips($logdir);

    updateNetlistFiles();

    prepareCL4Submit();

    mailStatus(); 

}

sub main {
    regsys2dvs();
}

&main();
