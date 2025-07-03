#!/home/gnu/bin/perl -w

use IO::File;
use File::Basename;
use Cwd;


my $date = `date`;
print "Current date is $date\n";
# place to dump out all the log files:
# Create the directory with time stamp
my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
$yr += 1900;
$month += 1;
my $globalLogDirThisBranch = "/home/scratch.sw-rmtest-autoCheckin_sw/fmodUpdate_hsmb/logs/";
my $direc = "$yr.$month.$dayofm.$hr.$min.$sec";
my $logDirForThisRun = "$globalLogDirThisBranch/"."$direc";
print "Trying to create directory $logDirForThisRun for storing logs of this run\n";
mkdir($logDirForThisRun, 0755) || die "Not able to create directory $logDirForThisRun\n";
print "Created directory $logDirForThisRun for storing logs\n";

my $masterLogFileThisRun = "$logDirForThisRun"."/masterLogThisRun.log";
my $changelistfile = "changelist.txt";
my $newClNetlist = undef;

my %netlists = (
   "gh100" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/hopper/gh100/net/dvs_package.h",
   "gh202" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/hopper/gh202/net/dvs_package.h",
   "ls10"  => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/lwswitch/ls10/net/dvs_package.h",
   "ad102" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/ada/ad102/net/dvs_package.h",
   "gb100" => "//sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/blackwell/gb100/net/dvs_package.h"
);


# Validate Script environment requirements
sub verifyelwironment {
    my $okClient = 1;

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

    if (!$okClient) {
        exit 0;
    }

    return $okClient;
}


# Setup environment for script, sets globals to good values.
sub configureElwironment {

    $p4spec = "/home/utils/bin/p4 -c $ELW{P4CLIENT} -p $ELW{P4PORT}";
    $as2spec = "/home/lw/bin/as2  -c $ELW{P4CLIENT} -p $ELW{P4PORT} -u $ELW{P4USER} ";
}

sub p4EditFiles {
    my $editfiles = undef;
    my $chip = $_[0];
    open (P4SUB, "> $logDirForThisRun/p4submit.log") or die "can't open file $logDirForThisRun/p4submit.log : $!";

    $editfiles = "$netlists{$chip}";

    print P4SUB $editfiles;


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

sub pathToNetlist {
    my($chip) = @_;
    my $fullpath = undef;

    $fullpath = $ELW{"P4ROOT"};
    $fullpath = "$fullpath"."\/$netlists{$chip}";

    return $fullpath;
}

# Update the NETLIST.H with NEWCL's
sub updateNetlistFile {
    my($chip,$hwClToUpd) = @_;
    my $lwrrentlist = pathToNetlist($chip);
    my $line = undef;

    # Do edit netlists.h, add in HWCL.
    print "Updating newer hwcl to $chip netlist file\n";
    open(NET, "< $lwrrentlist") or die "Could not open netlist file $lwrrentlist : $!";
    my @net = <NET>;
    close(NET);

    open(NETW, "> $lwrrentlist" ) or die "Could not open file $lwrrentlist :$!";
    for( $a = 0; $a < @net; $a = $a + 1 )
    {
        $line = $net[$a];
        if($line =~ /^#define.*$chip*_NETLIST_DVS\s*999\s*\/\/\s*hw\@(\w*)\s*sw\@(\w*)\s*fmpc\@(\w*)\s*fmlx\@(\w*).*$/i) 
        {
            print "Previous: $line";
            $line  = "#define ".$chip."_NETLIST_DVS 999 // hw@"."$hwClToUpd "."sw@"."12345678 "."fmpc\@0 "."fmlx@"."$hwClToUpd "."fmgbt@"."$hwClToUpd\n";
            print "New     : $line";
            print NETW $line;
        }
        else
        {
            print NETW $line;
        }
    }
    close(NETW);
}



# Return Netlists.h Cl description info.
sub getp4ClDesc {
    my ($chip, $hwcl) = @_;
    my $descline = undef;

    $descline = "\n\tFmodel update on to DVS\n\t$date";
    push (@cl_desc, $descline);
    $descline = "\n\tChip : ".$chip." HWCL : $hwcl";
    push (@cl_desc, $descline);

    $descline = "\n\tBug 478135 Bug 421682\n";
    push (@cl_desc, $descline);
    $descline = "\treviewed by : vsharma\n";
    push(@cl_desc, $descline);

    $descline = "\tdvs_skip_auto_tests\n";
    push(@cl_desc, $descline);
    $descline = "\tSkipping DVS builds and tests since these CLs are already tested before submit, avoiding unnecessary DVS load.\n";
    push(@cl_desc, $descline);

    return \@cl_desc;
}


sub p4CreateNewChangelist {

    my ($chip, $hwcl, $newclfile) = @_;

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
    getp4ClDesc($chip, $hwcl);
    print NEWCL @cl_desc;

    # Print files in CL
    print NEWCL "\nFiles:\n\n";
    print NEWCL "\t$netlists{$chip}\n";
    close(NEWCL);
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

    print LOGFILE $newClNetlist;

    if ( !(1 eq $filesInCl) ) {
        print "Error: Fmodels to be updated != Number of files in this Changelist \n";
    }

    print LOGFILE "\n\tNew netlists.h Cl : $newClNetlist\n";

    close (LOGFILE);

    return $newClNetlist;
}

# Perform Bundle submission.
sub submissionForAS2 {

    my ($chip) = @_;
    open(LOGFILE, "+>>$masterLogFileThisRun");

    my $path2netlist = dirname(pathToNetlist($chip));

    chdir "$path2netlist" or die "cannot change directory: $path2netlist : $!\n";

    print "User : $ELW{P4USER} -c $ELW{P4CLIENT} \n";
    my @Info = `$as2spec -r test 2>&1`;
    print @Info;
    my $as2CmdForSubmit = "echo \"y\" | $as2spec submit -f -D \"fmod update for DVS use, Does not affect any builds or tasks\" -c $newClNetlist 2>&1";
    print LOGFILE "\n$as2CmdForSubmit\n";
    # my @bundleInfo = "skipping as2 submission\n";
    my @bundleInfo = system($as2CmdForSubmit);

    print LOGFILE @bundleInfo;

    close(LOGFILE);
}

sub p4revertFiles {
    my ($chip) = @_;
    my $revertfiles = undef;
    open (P4SUB, "> $logDirForThisRun/p4submit.log") or die "can't open file $logDirForThisRun/p4submit.log : $!";

    $revertfiles = "$netlists{$chip}";

    my @revertlog = `$p4spec revert $revertfiles 2>&1`;

    print @revertlog;
    print P4SUB @revertlog;

    my $p4ClDeleteLog = `$p4spec change -d $newClNetlist`;
    print P4SUB $p4ClDeleteLog;
    close(P4SUB);

    return;
}

# 1. Create changelist description file.
# 2. Create CL and get CL number with CL description file.
# 3. Perform as2 submit for this.
sub prepareCL4Submit {

    my ($chip, $hwcl) = @_;
    my $clfile = "$logDirForThisRun"."\/"."$changelistfile";

    p4CreateNewChangelist($chip, $hwcl, $clfile);

    my $clno = p4GetNewChangeListNumber();
    if($clno == -1)
    {
        return;
    }
    submissionForAS2($chip);

    p4revertFiles($chip);

    return;
}


sub main {

    my $HWCL = $ARGV[0];
    my $chip = $ARGV[1];

    print "CL number passed: ",$HWCL," GPU: ",$chip,"\n";

    verifyelwironment();

    configureElwironment();

    p4EditFiles($chip);

    updateNetlistFile($chip, $HWCL);

    prepareCL4Submit($chip, $HWCL);

}

&main();
