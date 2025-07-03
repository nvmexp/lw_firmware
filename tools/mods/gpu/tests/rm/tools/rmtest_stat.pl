#!/home/gnu/bin/perl 
#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2007 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

#
# Perl script to callwlate rmtest statistics.
#


use strict;
use IO::File;
use Cwd;

die "Syntax rmtest_stat.pl mods.log" if @ARGV != 1;

# Test Name Arr's
my @passTests = undef;
my @failTests = undef;
my @unsuTests = undef;
my @skipTests = undef;

# Test Count Var's
my ($totalTc, $passTc, $failTc, $unsuTc, $skipTc) = undef;

# Misc Var's
my $chip;
my $absModslogPath = undef;

# Flag Var's for Script.
my $debug = 0;          # - Enable Script debugging
my $rmTestsGroup = 0;   # - RM Test Group String



# function to validate mods.log file for extracting rmtest statistics
# In command line with rmtest.js having following args are must
#  -dvs : run all the tests.
#  -run_on_errror : donot stop on error, run all tests.
#  -grp : allow processing of group of tests 
sub verify_log_command_line_args 
{
   my($cmdline) = @_;
   
   print "Command Line : $cmdline\n" if $debug;
   if ($cmdline =~ /-run_on_error/) {

       if ($cmdline =~ /-grp\s*(L0|L1|L2|gf100_nl|mmu|pwr|stress|maxcover|sanity|all)/){ 
           $rmTestsGroup = uc($1);
       }  
       
       if(($cmdline =~ /-(dvs)/) && (!$rmTestsGroup)) {
           $rmTestsGroup = uc($1);
       } 

       print "Test Group : $rmTestsGroup\n" if $debug;
       if (!$rmTestsGroup) {
           print "Provide mods.log which ran with either -dvs or -grp option \n";
           return "KO"; 
       }
       
   } else {
       print "Provide mods.log which ran with -run_on_error option \n";
       return "KO";
   }
   return "OK";
}


# Print of RMTEST STATISTICS in CSV format file.
# Same as Console printing. 
sub print_csv_report() 
{
    my $csvFile = "./rmtest_stat.csv";
    my $tempStr = undef;
    my $i = 0;    

    $totalTc = $#passTests + $#failTests + $#skipTests; 

    open(CSV, ">$csvFile") or die "Cant open file : $!\n";
    print CSV ",,RMTEST STATISTICS : "."$rmTestsGroup".",,\n";
    print CSV ",,,,\n";
    print CSV ",,PASSING TESTS,,\n";
    for($i = 1 ; $i <= $#passTests ; $i++ ) {
        $tempStr = sprintf(",,%s,,\n", $passTests[$i]);
        print CSV "$tempStr";
    }
    
    print CSV ",,,,\n";
    print CSV ",,FAILING TESTS,,\n";
    
    for($i = 1 ; $i <= $#failTests ; $i++ ) {
        $tempStr = sprintf( ",,%s,,\n", $failTests[$i]);
        print CSV "$tempStr";
    }
    
    print CSV ",,,,\n";
    print CSV ",,UNSUPPORTED TESTS,,\n";
    for($i = 1 ; $i <= $#unsuTests ; $i++ ) {
        $tempStr = sprintf(",,%s,,\n", $unsuTests[$i]);
        print CSV "$tempStr";
    }

    print CSV ",,,,\n";
    print CSV ",,SKIPPED TESTS,,\n";
    
    for($i = 1 ; $i <= $#skipTests ; $i++ ) {
        $tempStr = sprintf(",,%s,,\n", $skipTests[$i]);
        print CSV "$tempStr";
    }
    print CSV ",,,,\n";
    print CSV ",Total,Passing,Failing,Skipped,Unsupported,\n";
    $tempStr = sprintf(",%5d,%7d,%7d,%8d,%11d,\n", $totalTc, $#passTests, $#failTests, $#skipTests, $#unsuTests);
    print CSV "$tempStr";
    $tempStr = sprintf(", \%age,  %.2f,   %2.2f,    %2.2f,           ,\n", ($#passTests * 100.00 / $totalTc), ($#failTests * 100.00 / $totalTc), ($#skipTests * 100.00 / $totalTc));
    print CSV "$tempStr";
    close(CSV);
}


# Parse mods.log to find Passing Failing Skipped and Unsupported tests 
# Match following sequence of string/line to say a test is passing/failing.
# Enter Run(TestName), No Unsupported test message,  Exit 0 : TestName ok => Passing
# Enter Run(TestName), Unsupported test message,  Exit 0 : TestName ok => Unsupported
# Enter Run(TestName), Exit ErrNumber : TestName ErrMesg => Failing
# Forcebly Skipping Test TestName => Skipped 
sub parse_mods_log() 
{
    
    my $test = undef;           # Store Test Name under the log
    my $rmtestsRunBlock = 0;    # Flag to differentiate Gpu/Platform load log and rmtests run log
    my $insideTestlog = 0;      # Flag dictates EnterTest -- ExitTest 
    my $isSupportedTest =0;   # Flag marks whether test is already marked as unsupported
    my $modsRunCmdLine = undef; # Command line specified while running rmtests
    my $trackAssertFailLines= 0;# - Var to track ASSERT()
    my $line = 0;                 # For counting lines
    
    open(MODSLOG, "<$absModslogPath") or die "Couldn't open mods.log file : $!";

    while(<MODSLOG>)
    {
        $line++;
        if ( ! $rmtestsRunBlock ) 
        {
            if ( $_ =~ /Loading chip model from (gf100)_fmodel.so/) {
                $chip = $1;
            }

            if ( $_ =~ /rmtest.js\s*:\s*([0-9]+)/ ) {
                $totalTc = $1;
                $rmtestsRunBlock = 1;
            }
        
            if ( $_ =~ /Command Line : (rmtest.js.*)/) {
                $modsRunCmdLine = $1;
            
                my $status = verify_log_command_line_args($modsRunCmdLine);
                if( $status =~ /KO/) {
                    print "Aborting ... \n";
                    close(MODSLOG);
                    exit(1);
                }
                
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
                close(MODSLOG);
                die "Provided mods.log file, which failed on ASSERT()\nAborting ...";    
            }
        }
    } #<end while>
    close(MODSLOG);
}

# Print statistics on Console
#+---------------------------------------+
#|     RMTEST STATISTICS                 |
#+---------------------------------------+
#|     1.PASSING TESTS                   |
#+---------------------------------------+
#|                                       |
#|                                       |
#|                                       |
#+---------------------------------------+
#|     2.FAILING TESTS                   |
#+---------------------------------------+
#|                                       |
#|                                       |
#|                                       |
#+---------------------------------------+
#|     3.UNSUPPORTED TESTS               |
#+---------------------------------------+
#|                                       |
#|                                       |
#|                                       |
#+---------------------------------------+
#|     4.SKIPPED TESTS                   |
#+---------------------------------------+
#|                                       |
#|                                       |
#|                                       |
#+---------------------------------------+
#|Total|Passing|Failing|Skipped|Unsuprted|
#+---------------------------------------+
#|                                       |
#|                                       |
#|                                       |
#+---------------------------------------+
sub print_report() 
{
    my $i = undef;
    $totalTc = $#passTests + $#failTests + $#skipTests; 
   
    if ($totalTc eq 0) {
        printf("Provided invalid mods.log file, may be no tests ran\n");
        exit(0); 
    }
    
    print "+----------------------------------------------------+\n";
    printf("|           RMTEST STATISTICS : %-8s             |\n", $rmTestsGroup);
    print "+----------------------------------------------------+\n";
    print "|               1. PASSING TESTS                     |\n";
    print "+----------------------------------------------------+\n";
    
    for($i = 1 ; $i <= $#passTests ; $i++ ) {
        printf("|               %-37s|\n", $passTests[$i]);
    }
    
    print "+----------------------------------------------------+\n";
    print "|               2. FAILING TESTS                     |\n";
    print "+----------------------------------------------------+\n";
    
    for($i = 1 ; $i <= $#failTests ; $i++ ) {
        printf("|               %-37s|\n", $failTests[$i]);
    }
    
    print "+----------------------------------------------------+\n";
    print "|               3. UNSUPPORTED TESTS                 |\n";
    print "+----------------------------------------------------+\n";
    
    for($i = 1 ; $i <= $#unsuTests ; $i++ ) {
        printf("|               %-37s|\n", $unsuTests[$i]);
    }

    print "+----------------------------------------------------+\n";
    print "|               4. SKIPPED TESTS                     |\n";
    print "+----------------------------------------------------+\n";
    
    for($i = 1 ; $i <= $#skipTests ; $i++ ) {
        printf("|               %-37s|\n", $skipTests[$i]);
    }
    print "+----------------------------------------------------+\n";
    print "| Total | Passing | Failing | Skipped  | Unsupported |\n";
    print "+----------------------------------------------------+\n";
    printf("| %5d | %7d | %7d | %8d | %11d |\n", $totalTc, $#passTests, $#failTests, $#skipTests, $#unsuTests);
    print "+----------------------------------------------------+\n";
    printf("|  \%age |   %03.2f|    %2.2f |     %2.2f |             |\n", ($#passTests * 100.00 / $totalTc), ($#failTests * 100.00 / $totalTc), ($#skipTests * 100.00 / $totalTc));
    print "+----------------------------------------------------+\n";
}


# Main Function to generate RMTEST statistics.
# - Parse mods.log file. 
# - Print statistics on console.
# - Print statistics on CSV file.
sub rmtest_stat_main() 
{

	# Create absoulute mods.log path 
	my $pwd = cwd;
	my $modslog = $ARGV[0];
	chomp($pwd);

	# if path starts as C: or /<dir>/ => is already abs path
	if ($modslog !~ /(^[a-zA-Z]:|^\/\w)/) {
		$absModslogPath = "$pwd"."/"."$modslog"; 
	} else {
		$absModslogPath = "$modslog"; 
	}

	print "Absolute Input filepath : $absModslogPath\n" if $debug;
	&parse_mods_log(); 
    &print_report(); 
    &print_csv_report(); 
   
}

# Call Main Function
&rmtest_stat_main();

