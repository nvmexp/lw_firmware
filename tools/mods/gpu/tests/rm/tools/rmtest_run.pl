#!/home/gnu/bin/perl -w


use IO::File;
use Cwd;

# Global Var's
my $isWindows = undef;

my $runlistfile = undef;
my $modsOrBasic = undef;
my $chip = undef;

# Config Var's
my $debug = 0;
my $modsSanityFile = "modssim_rm.cmd";
my $modsSanityXheapFile = "modssim_rm_xh.cmd";
my $basicSanityFile = "mods_rm.cmd";
my $workdir = "log";
my $masterfile = "mods_all.log";
my $externalHeap = undef;
my $exitWithError = undef;

# Detect Windows vs. Unix
if ($^O eq "MSWin32") {
    $isWindows = 1;
} elsif ($^O eq "cygwin") {
    $isWindows = 1;
} elsif ($^O eq "linux") {
    $isWindows = 0;
} else {
    die "Could not identify operating system";
}

# set dir separator based on OS
if($isWindows) {
    $separator = "\\";
} else {
    $separator = "//";
}

# Prints Usage of script
sub Usage {

print "
    rmtest_run.pl
    -chip fmodel => mods sanity other wise basic sanity.
    -runlist <runfile> => run commands in this file.
             default files : modssim_rm.cmd  if option is -chip.
                             mods_rm.cmd if no -chip option.
";

}

sub ParseArgs {
    my $i = 0;
    my $usedRunlist = 0;
    # By default it is basic sanity
    $runlistfile = $basicSanityFile;
    $modsOrBasic = "basic";

    print "Command line args : @ARGV \n" if $debug;
    for ( $i = 0; $i < @ARGV ; $i++ ) {
        if ($ARGV[$i] =~ /-chip/) {
            $chip = $ARGV[$i+1];

            if ( (!defined($chip)) or ($chip !~ /(g[ftk][12][0-9][0-9a-z]|ig[tf][0-9][0-9][a-z]|^g[789][0-9])/i) ) {
                die "Invalid fmodel name : $ARGV[$i+1]";
            }
            $modsOrBasic = "mods";

        }

        if ($ARGV[$i] =~ /-runlist/) {
            $runlistfile = $ARGV[$i+1];
            $usedRunlist = 1;
        }

        if ($ARGV[$i] =~ /-xh/) {
            $externalHeap = "-external_heap";
        }

        if ($ARGV[$i] =~ /(-h|--help)/) {
            Usage();
            exit;
        }
    }

    if (! $usedRunlist) {
        if( defined($externalHeap) && ($modsOrBasic =~ /mods/ ) ) {
            $runlistfile = $modsSanityXheapFile;
        } elsif($modsOrBasic =~ /mods/) {
            $runlistfile = $modsSanityFile;
        }
    }

}

# Parses and runs runlist.cmd file to run rmtests.
sub ModsOrBasicSanity {
    # open runlist file.
    open(CMD, "<$runlistfile") or die "cannot open file $runlistfile : $!";

    # create log directory
    system("mkdir -p $workdir");

    # Run commands in runlist file.
    # store mods.log in log directory
    my $i = 1;
    while(<CMD>) {
        $lwrrent_cmd = $_;
        chop($lwrrent_cmd);

        if ($modsOrBasic =~ /^mods$/) {
            my @exclude_chip = 0;
            my $nextcmd = 0;
            my $chip_index = 0;
            my @lwrrent_cmd_args = split(' ', $lwrrent_cmd);

            # Search , if we need to skip this command for any chip
            for (my $index = 0; $index <= $#lwrrent_cmd_args; $index++) {
                if ($lwrrent_cmd_args[$index] =~ /^-excludeChip$/) {
                    $exclude_chip[$chip_index++] = $lwrrent_cmd_args[++$index];
                }
            }

            # If this chip is in exclusion , go for next cmd
            foreach (@exclude_chip) {
                $lwrrent_cmd =~ s/-excludeChip([ ]*)$_//g;
                if ($_ =~ /^$chip$/) {
                    $nextcmd = 1;
                }
            }

            # For gk20a, skip commands that force anything to sysmem.
            if ( ($chip eq "gk20a") &&
                 ($lwrrent_cmd =~ /coh_/ || $lwrrent_cmd =~ /ncoh_/ ||
                  $lwrrent_cmd =~ /_coh/ || $lwrrent_cmd =~ /_ncoh/)) {
                print "Skipping command $lwrrent_cmd on gk20a chip\n";
                $nextcmd = 1;
            }
            if ($nextcmd) {
                next;
            }

            $lwrrent_cmd =~ s/-chip/-chip $chip/g;
        }

        # fiter based on chip and mods args
        if ( ($lwrrent_cmd =~ /sysmem_base\s*0xffffffff/) && ($chip !~ /gf100/)) {
            next;
        }

        if (defined($externalHeap) ) {
           $lwrrent_cmd = $lwrrent_cmd." ".$externalHeap;
           print "$lwrrent_cmd\n";
        }

        push(@commandlist, $lwrrent_cmd);

        # Run mods command
        print "$lwrrent_cmd\n" if $debug;
        my $status = system($lwrrent_cmd)   if (!$debug);

        my $testfailed = ParseModsLogExitOnError();

        if ($status || $testfailed ) {
            $exitWithError = -1 ; # sim.pl exited with error (may be memleak).
        }

        # Move mods.log to log/mods_<cmdno>.log dir
        if ($isWindows) {
            if ($debug ) {
               print "move mods.log $workdir/mods_$i.log\n";
            } else {
               system("move mods.log $workdir/mods_$i.log");
            }
        } else {
            if ($debug) {
               print "move mods.log $workdir/mods_$i.log\n";
            } else {
               system("mv mods.log $workdir/mods_$i.log");
            }
        }

        $i++;
    }
    close(CMD);


    # merge all child mods.log files into master mods file.
    open(MODSALL, ">$masterfile") or die "Couldn't open file $masterfile : $!";
    opendir(DIR, "$workdir" ) or die "Error in opening dir $workdir\n";

    $i = 0;
    while( ($filename = readdir(DIR))) {

       # if file is mods_<cmdno>.log add it to mods_all.log
       if ($filename =~ /mods_([0-9]*)\.log/) {

           # print the header command file here.
           print MODSALL "\n******************************************************************\n";
           printf MODSALL "* $commandlist[$i]";
           print MODSALL "******************************************************************\n";
           open(MODSLOG, "<$workdir$separator$filename") or die "Couldn't open file $filename : $!";
           while(<MODSLOG>) {
               print MODSALL "$_";
           }
           close(MODSLOG);

           # next command
           $i++;
       }
    }
    closedir(DIR);
    close(MODSALL);

    if(defined ($exitWithError))
    {
       exit($exitWithError);
    }
}

# Parse mods.log to find Passing Failing Skipped and Unsupported tests
# Match following sequence of string/line to say a test is passing/failing.
# Enter Run(TestName), No Unsupported test message,  Exit 0 : TestName ok => Passing
# Enter Run(TestName), Unsupported test message,  Exit 0 : TestName ok => Unsupported
# Enter Run(TestName), Exit ErrNumber : TestName ErrMesg => Failing
# Forcebly Skipping Test TestName => Skipped
sub ParseModsLogExitOnError()
{

    my $test = undef;           # Store Test Name under the log
    my $rmtestsRunBlock = 0;    # Flag to differentiate Gpu/Platform load log and rmtests run log
    my $insideTestlog = 0;      # Flag dictates EnterTest -- ExitTest
    my $isSupportedTest =0;   # Flag marks whether test is already marked as unsupported
    my $modsRunCmdLine = undef; # Command line specified while running rmtests
    my $trackAssertFailLines= 0;# - Var to track ASSERT()
    my $line = 0;                 # For counting lines
    my $pwd = cwd;
    my $absModslogPath = "$pwd"."\/mods.log";
    my $totalTc = undef;

    my (@passTests, @failTests, @skipTests, @unsuTests) = undef;

    open(MODSLOG, "<$absModslogPath") or die "Couldn't open mods.log file : $!";

    while(<MODSLOG>)
    {
        $line++;
        if ( ! $rmtestsRunBlock )
        {
            if ( $_ =~ /Loading chip model from (.*)_fmodel.so/) {
                $chip = $1;
            }

            if ( ($_ =~ /rmtest.js\s*:\s*([0-9]+)/) ||
                 ($_ =~ /Command Line\s*:\s*rmtest.js/) ) {
                $totalTc = $1;
                $rmtestsRunBlock = 1;
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
                # $2 match TestName
                if ( /Exit ([0]+) : Global\.Run(\w*) ok/ ) {
                    $test = $2;
                    print "Passing test : $test \n" if ( $debug && (!$isSupportedTest));
                    $insideTestlog = 0;
                    push(@passTests, $test) if (!$isSupportedTest);
                    $isSupportedTest = 0;
                }

                # $2 match TestName
                elsif ( /Exit ([0-9]+) : Global.Run(\w*) (.*)$/ ) {
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
                # Bug 944918 - Allow conlwrrent test exelwtion, which needs the
                # following check to be removed.
                #if ( /Enter Global\.(\w*)\s*$/ ) {
                #    die "Error in mods.log file at $line : $_";
                #}
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

            if ($trackAssertFailLines > 2) {
                close(MODSLOG);
                print "Provided mods.log file, which failed on ASSERT()\nAborting ...";
                $exitWithError = -1;
            }
        }
    } #<end while>
    close(MODSLOG);

    if (defined($failTests[0]))
    {
        $exitWithError = -1;
    }

    return $exitWithError;
}




sub main {

    ParseArgs();

    ModsOrBasicSanity();

}

&main();


##############################################################################################################################
#
# Below is code for sli tesing
#
##############################################################################################################################
my @rmtest_arr = (
         "ActiveChannel3DTest",
         "AllocContextDma2Test",
         "AllocObjectTest",
         "Class007dTest",
         "Class307eTest",
         "ClassTurboCipherTest",
         "Class507dEventTest",
         "Class507fTest",
         "DupObjectTest",
         "Class005dTest",
         "MapMemoryTest",
         "Class0070Test",
         "PeerToPeerTest",
         "RobustChannelTest",
         "SimpleMapTest",
         "MemoryLimitCheckTest",
         "Class826fTest",
         "Class50a0Test",
         "Ctrl0080DmaTest",
         "Class502dTest",
         "Ctrl2080FbTest",
         "ClassBar1Test",
         "DetectEdidTest",
         "SysconTest",
         "Class5097Test",
         "Ctrl0080GrTest",
         "Ctrl2080GrTest",
         "CtrlSysTest",
         "CtrlDispTest",
         "Class5039Test",
         "PerfMon",
         "ClockTest",
         "Class208fTest",
         "FandFTest",
         "StressPowerChangeTest",
         "FbImpTest",
         "GpuFifoStressTest",
         "CompTest",
         "Class88b1Test",
         "Class88b2Test",
         "Class88b3Test",
         "Class88b4Test",
         "DpTest",
         "MapMemoryDmaTest",
         "MMUTester",
         "SliMemLeakTest",
         "Ctrl2080GpuTest",
         "ClassGsyncTest",
         "LwDpsTest",
         "ProdTest",
         "QuickSuspendResumeGPU",
         "CtrlI2CTest",
         "Class85b5Test",
         "Class906fTest",
         "Class906eTest",
         "ClassMXMTest",
         "Class9096Test",
         "ChannelMemInfo",
         "Class85b6Test",
         "Class9068Test",
         "Class907fTest",
         "ElpgTest",
         "Ctrl0080IntrTest",
         "Class85b1Test",
         "Class85b2Test",
         "Class85b3Test",
         "Ctrl2080McTest",
         "Ctrl0080VpTest",
         "Class8697Test",
         "FifoRunlistTest",
         "Class90b5Test",
         "Class9072Test",
         "Virtual2Physical",
         "Class86b1Test",
         "Class86b6Test",
         "SafeRm64bitAddrOpTest"
    );
my @powMgmtTests = ( );
my @uniBroadCastTests = ( );


#------------------------------------------------------------------------------------
# Input : mods_all.log
# Output: testRunStatus.log
#  Enter.Global(TestName)
#  ...
#  ...
#  Exit.Global
#------------------------------------------------------------------------------------
sub extractTestRunLog {
    my ($modsalllog) = @_;


    open (MODSALL, "< $modsalllog") or die "Can't open file $modsalllog : $!";

    while(<MODSALL>) {
        if( $_ =~ //) {
        } else {
        }
    }
}


sub getTestNumFromTestName {
    my ($testname) = @_;

    my $testNum = 0;

    foreach my $test (@rmtest_arr) {
        $testNum++;
        if($test =~ /$testname/) {
            return $testNum;
        }
    }
}

sub getTestNameFromTestNum {
    my ($testNum) = @_;

    return $rmtest_arr[$testNum];

}
