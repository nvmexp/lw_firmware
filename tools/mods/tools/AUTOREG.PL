#                                                                      
# LWIDIA_COPYRIGHT_BEGIN                                                
#                                                                       
# Copyright 1999-2003 by LWPU Corporation.  All rights reserved.  All 
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.                       
#                                                                       
# LWIDIA_COPYRIGHT_END                                                  
#                                                                       

##------------------------------------------------------------------------------
## $Id: //sw/integ/gpu_drv/stage_rel/diag/mods/tools/AUTOREG.PL#3 $
##------------------------------------------------------------------------------

#use strict;

#constants
*cfgFile  = \"autoreg.cfg";        # server configuration filename
*stopFile = \"autoreg.stp";
*baseReportFilename = \"mods";

# These are strings that should be found as keys in the server
# configuration file.  They'll act as hash keys.
*kNumMachines = \"NUM_CLIENTS";
*kCommandLine = \"CLIENT_COMMAND_LINE";
*kRecipientList = \"RECIPIENT_LIST";
*kMailSender = \"MAIL_SENDER";

# Indices for an array within @machines
$iNumRuns = 0;
$iUpToDate = 1;
$iFailures = 2;

#globals
%cfg;                       # Hash of everything in the config file.

@machines;                  # Array of length numMachines
                            # Each entry is a reference to another array


######### Parse config file and ensure we get all needed values ################

&parseConfigFile; # fills cfg

ensureExists($kNumMachines);
ensureExists($kCommandLine);
ensureExists($kRecipientList);
ensureExists($kMailSender);

################################################################################

&initArrs;
&deleteLeftoverResults;
&createInitialBatchFiles;


my $numMachines = $cfg{$kNumMachines};

# Main loop
while (!(-e $stopFile)) {
   # loop over all machines.
   foreach $i (0..($numMachines - 1)) {
      $name = $baseReportFilename.$i;
      # If a result file exists for this machine and we can open it
      # for read/write access...
      if (-e $name) {
          sleep 5;
          if (!(open(RESULTS, "+<".$name))) {
             print "had trouble opening $name\n";
          }
          processResults($i, RESULTS);
          close(RESULTS);
          unlink($name);
       
          giveNewInstructions($i);
      }
   }
   # See if there's a newly compiled version of mods present.
   # Mark all clients obsolete if there is.
   #if (-e $newModsFilename) {
   #    &markAllClientsObsolete;
   #    rename($newModsFilename, $oldModsFilename);
   #}
}

&sendFinalReport;

#parses all values in the config file into the %cfg
sub parseConfigFile {
    open(CFG, "<".$cfgFile) or die "Couldn't open server config file.\n";
    # for each line...
    while (<CFG>) {
        s/#+.*//;  # Get rid of anything after the first # in the line
        if (/(\S+)\s*=\s*(\S.*)/) {
            $temp = $1;
            $temp =~ tr/a-z/A-Z/; # colwert the key to uppercase.

            $temp2 = $2;
            $temp2 =~ s/\s+$//;   # clear the trailing whitespace.

            $cfg{$temp} = $temp2;
        }
    }
    close(CFG);
}

# first arg = string that should be in the config file
sub ensureExists ($) {
    my $cfgString = shift(@_);
    (exists $cfg{$cfgString}) or 
      die "$cfgString not defined in $cfgFile.  Exiting.\n";
}

# first arg = number of machine we're processing
# second arg = file handle of results file
sub processResults ($$) {
    my $machineNum = shift(@_);
    my $HANDLE = shift(@_);

    # If the machine passed the test...
    if (testSucceeded($HANDLE)) {
       reportSuccess($machineNum, $HANDLE);
    }
    # If the machine failed the test...
    else {
       reportFailure($machineNum, $HANDLE);
    }

    #in any case, increment the number of total runs for that machine.
    $machines[$machineNum][$iNumRuns]++;
}

#first arg = number of machine we're giving instructions to
sub giveNewInstructions ($) {
    my $machineNum = shift(@_);
    
    # Now give the machine its new instructions.
    # If it has the most recent version of mods,
    # tell it to run again.
    if ($machines[$machineNum][$iUpToDate]) {
       runSameVersion($machineNum);
    }
    # If it needs a new version, tell it to get it.
    else {
       getNewVersion($machineNum);
       $machines[$machineNum][$iUpToDate] = 1;
    }
}

# initializes the @machines array.
sub initArrs {
   my $numMachines = $cfg{$kNumMachines};
   my $i = 0;
   foreach $i (0..($numMachines - 1)) {
      $machines[$i][$iNumRuns] = 0;
      $machines[$i][$iUpToDate] = 1;
   }
   #for ($i=0; $i<$numMachines;$i++) {
   #   print "$i\n";
   #   print "$machines[$i][$iNumRuns]\n";
   #   print "$machines[$i][$iUpToDate]\n";
   #}
}

# creates a new machine and returns it (as a hash)
sub newMachine {
   my @machineArr;
   $machineArr[$iNumRuns] = 0;
   $machineArr[$iUpToDate] = 1;
   return @machineArr;
}

# deletes any results files turned in after the end of the last 24-hr run
sub deleteLeftoverResults {
   if (-e $stopFile) {
      unlink($stopFile);
   }
   foreach my $i (0..($cfg{$kNumMachines} - 1)) {
      if (-e "mods".$i) {
         unlink("mods".$i);
      }
   }
}

# creates batch files to start all of the clients, i.e.
# run0.bat, run1.bat, run2.bat, etc.
sub createInitialBatchFiles {
   my $i = 0;
   foreach $i (0..($cfg{$kNumMachines} - 1)) {
      createRunSameBatchFile($i);
   }
}

# sets all clients as out of date in the @machines array.
sub markAllClientsObsolete {
   print "TODO: implement markAllClientsObsolete.\n";
}

# first arg = file handle of test results file
sub testSucceeded ($) {
    my $HANDLE = shift(@_);
    while (<$HANDLE>) {
        if (/Error Code = (\d+)/) {
            return ($1 == 0);
        }
    }
    return;
}

# first arg = machine number
# second arg = file handle
sub reportSuccess ($$) {
    my ($machineNum, $HANDLE) = (@_);
    seek $HANDLE, 0, 0;      # Move the file pointer to the head of the file.
    print "Machine #$machineNum had a successful run!\n";
}

# first arg = machine number
# second arg = file handle
sub reportFailure ($$) {
   my ($machineNum, $HANDLE) = (@_);
   seek $HANDLE, 0, 0;      # Move the file pointer to the head of the file.
   my $errorCode = "none";
   while (<$HANDLE>) {
      if ($_ =~ /Error Code = (\d+)/) {
         $errorCode = $1;
         last;
      }
   }

   push @{$machines[$machineNum][$iFailures]}, [$cfg{$kCommandLine}, $errorCode];
   
   print "Machine #$machineNum failed a run.\n";
}

# first arg = number of the machine we're talking to.
sub runSameVersion ($) {
    my $machineNum = shift(@_);
    createRunSameBatchFile($machineNum);
}

# Creates a batch file telling the client to run the test with its current
# version of mods.
# First arg = the number of the client machine.
sub createRunSameBatchFile ($) {
   my $machineNum = shift(@_);

   my $filename = ">run".$machineNum.".bat";
   print "outputting $filename\n";
   open(OUTFILE, $filename); # create the instruction file for writing.

   # output a batch file that will tell the program to run.
   #print OUTFILE "@echo off\n";
   print OUTFILE "mods -l tmods%MACHINENUM% ";
   print OUTFILE "$cfg{$kCommandLine}\n";
   
   close(OUTFILE);
}

# first arg = number of the machine we're talking to.
sub getNewVersion ($) {
    my $machine = shift(@_);
    print "TODO: tell machine $machine to d/l and run a new version.\n";
}

sub sendFinalReport {
   my $date = &getZeroedDate;
   my $filename = "results\\".$date.".res";

   open(RESFILE, ">".$filename);

   foreach my $i (0..($cfg{$kNumMachines}-1)) {
      outputOneMachine(RESFILE, $i);
   }

   close(RESFILE);

   # Send mail with blat.
   my $blatSender = "-f $cfg{$kMailSender}";
   my $blatRecipient = "-t $cfg{$kRecipientList}";
   my $blatSubj = "-s \"Regression Results for ".
      ((localtime)[4]+1)."/".(localtime)[3]."/".((localtime)[5] + 1900)."\"";
   
   my $blatLine = "blat $filename -server smtp.lwpu.com $blatSender $blatRecipient $blatSubj";
   print $blatLine."\n";
   system($blatLine);
}

sub getZeroedDate {
   $monthNum = (localtime)[4] + 1;
   if (!($monthNum =~ /../)) {
      $monthZero = 1;
   }
   $dayNum = (localtime)[3];
   if (!($dayNum =~ /../)) {
      $dayZero = 1;
   }
   $yearNum = (localtime)[5] - 100;
   if (!($yearNum =~ /../)) {
      $yearZero = 1;
   }

   my $str = ($monthZero ? "0" :"").$monthNum.($dayZero ? "0":"").$dayNum;
   $str = $str.($yearZero ? "0" :"").$yearNum;

   return $str;
}

# first arg = file handle of file to write
# second arg = number of the machine we're interested in.
#
# Prints statistics from one machine out to an open filehandle.
sub outputOneMachine ($$) {
   (my $OUTHANDLE, my $machineNum) = @_;
   my $machineRef = $machines[$machineNum];
   print $OUTHANDLE "|-----------------------------------------------------\n";
   print $OUTHANDLE "|Machine #$machineNum   ($$machineRef[$iNumRuns] runs)\n";
   print $OUTHANDLE "|-----------------------------------------------------\n";
   if (defined $$machineRef[$iFailures]) {
      for (my $i = 0; $$machineRef[$iFailures][$i]; $i++) {
         print $OUTHANDLE "Failure #".($i+1)."\n";
         print $OUTHANDLE "\tcommand line: mods ";
         print $OUTHANDLE "$$machineRef[$iFailures][$i][0]\n";
         print $OUTHANDLE "\terror code: $$machineRef[$iFailures][$i][1]\n";
      }
   }
   else {
      print $OUTHANDLE "[no failures]\n";
   }
   print $OUTHANDLE "\n\n";
}
