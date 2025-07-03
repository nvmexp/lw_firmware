#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#-------------------------------------------------------------------------------

package MLParser;

=head1 NAME

MLParser - A Parser for MODS Logger

=head1 SYNOPSIS

    use FindBin;
    use Data::Dumper;
    use lib "$FindBin::RealBin/logger";
    use MLParser;

    my $parser = MLParser->new({});
    $parsedData = $parser->startParse($self->{'attributes'}{'cmdline'});

=head1 DESCRIPTION

This is module embeded to MODS to parse raw log data for MODS Logger.

B<NOTE:> This module works basing on SimpleTree.pm.

=cut

use strict;
use warnings;

use Carp;
use Sys::Hostname;
use Time::Local;

# SimpleTree is a module modified from Tree::Fast in CPAN
# the location of SimpleTree is same as MLParser
use SimpleTree;

use Const::Fast;
const my $LOGFILENAME => 'ModsLogger.log';
const my $MODSLOGFILENAME => 'mods.log';
const my $LOGMODE => '>>';
const my $STARTTIME => qr/MODS start:(\s)(.+)/;
const my $ENDTIME => qr/MODS end  :(\s)(.+)/;
const my $SIMFLAG => qr/^-(?<!-)(\S+)(?!)/;                         # match flag pattern in cmdline e.g. -gpubios
const my $SIMFLAGARGS => qr/^(?!)(?!-)(\S+)(?!)/;                   # match args of flag pattern in cmdline e.g. 0 after -gpubios
const my $MODSJS => qr/^(?<!-)(\S+)\.js$/;                          # match mods file pattern in cmdline e.g. "mdiag.js", or mdiag.js (double quote is not added if it's a RTL test)
const my $MODSFLAG => qr/^-(\S+)$/;                                 # match mods flag pattern in cmdline e.g. "-simclk_mode"
const my $MODSFLAGARGS => qr/^(?<!-)(?!-)(\S+)$/;                   # match mods args of flag pattern in cmdline e.g. "gild"
const my $CFGFLAG => qr/^-(?<!-)(\S+)(?!)/;                         # match flag pattern in .cfg file e.g. -gpubios
const my $CFGFLAGARGS => qr/^(?!)(?!-)(\S+)(?!)/;                   # match args of flag pattern in .cfg file e.g. 0 after -gpubios
const my $ERRORREX => qr/(ERROR|Error)/;                            # match ERROR in mods.log
const my $WARNREX => qr/(Warn|WARN)/;                               # match Warn in mods.log
const my $CHIPREX => qr/Device Id(?:\s*):\s(\S+)/;                  # match chip info pattern e.g. Device Id: TU104
const my $RESULTFLAG => qr/Mdiag: (.+)/;                            # match test result info pattern e.g. Mdiag: all tests passed. TODO: better to parse trep.txt which support some corner cases like immediate exit after FLR
const my $CRITICAL => qr/Critical:/;                                # match critical content e.g. Critical: Single test specified failed to initialize correctly
const my $MDIAGREPORTFUNCTION => qr/MDIAG: ReportTestResults()./;   # match report content e.g. MDIAG: ReportTestResults().
const my $INITTIME => 1900;
const my $TABSPACE => 4;


my %parsedData = ();
my %stages = ();

=head1 METHODS

=head2 Constructor

=over 4

=item B<new(\%config)>

This will return a MLParser object. It will accept one reference to a hash
table which will become the option setting for ModsLogger. All other parameters
will be ignored.

=back

=cut

sub new{
    my ($class, $config) = @_;
    my $self = {};

    bless $self, $class;

    return $self;
}


=head2 Member Subroutines

=over 4

=item B<getParseResults($config)>

This is a helper function for debug.

=back

=cut

sub getParseResults{
    return \%parsedData;
}

=over 4

=item B<startParse($cmdline)>

This subroutine starts the parsing process with $cmdline as input.

=cut
# read from mods.log, $cmdline, ...
sub startParse{
    my ($self, $cmdline) = @_;

    $self->parseCmdline($cmdline);

    # parse for simclk
    $self->checkSimclkInCmdline();

    $self->parseUser($cmdline);
    $self->parseModsLog();
    $self->parseDataSource();

    return \%parsedData;
}

=over 4

=item B<parseCmdline($cmdline)>

This subroutine take $cmdline from sim.pl as input and parse all
flags and arguments. Store the result in $parsedData{'args'}.

It calls the following subroutines:

=cut

sub parseCmdline{
    my ($self, $cmdline) = @_;

    my @args = split(' ',$cmdline);
    my $key = '';
    my $modsDir = shift @args;
    my $lwrPos = 0;
    my $nextPos = 1;
    my ($key1, $lwrArg);
    my $argsNum = scalar @args;


    $parsedData{'mods_dir'} = "$modsDir";

    while ($lwrPos < $argsNum) {
        if ($args[$lwrPos] =~ /$SIMFLAG/) {
            $lwrArg = $1;
            $key = $lwrArg;
            if ($args[$nextPos] =~ /$SIMFLAGARGS/) {
                $nextPos = &getSimFlagArgs($nextPos, $argsNum, \@args, $key);
            } else {
                $parsedData{'args'}{$key} = '';
                ++$nextPos;
            }
        } elsif ($args[$lwrPos] =~ /$MODSJS/) {
            $lwrArg = $1;
            $key1 = $lwrArg;
            if ($args[$nextPos] =~ /$MODSFLAG/) {
                $nextPos = &getModsFlags($nextPos, $argsNum, \@args, $key1);
            } else {
                $parsedData{'args'}{$key1} = '';
                ++$nextPos;
            }
        } else {}
        $lwrPos = $nextPos++;
    }

    # check if -f exists
    if (exists $parsedData{'args'} &&
        exists $parsedData{'args'}{'mdiag'} &&
        exists $parsedData{'args'}{'mdiag'}{'f'}) {
        &parseCfg($parsedData{'args'}{'mdiag'}{'f'});
    }

    return 0;
}

=over 4

=item getSimFlagArgs($nextPos, $argsNum, \@args, $key)

This subroutine used to get simulation flags' arguments in cmdline.
It takes next cmdline element index ($nextPos), total number of arguments
in cmdline ($argsNum), reference to splited arguments in cmdline ($args)
and current flag ($key) as key for $parsedDatat.

=back

=cut

sub getSimFlagArgs{
    my ($nextPos, $argsNum, $args, $key) = @_;
    my $lwrArgsCount = 0;
    my $value = '';
    my $nextArg;

    $value = '';
    while ($nextPos < $argsNum && @$args[$nextPos] =~ /$SIMFLAGARGS/ && @$args[$nextPos] !~ /$MODSJS/) {
        $nextArg = $1;
        $value = $nextArg;
        ++$nextPos;
        if ($lwrArgsCount == 1) {
            my @argsValue = ();
            @argsValue = ($parsedData{'args'}{$key}, $value);
            $parsedData{'args'}{$key} = \@argsValue;
        } elsif ($lwrArgsCount > 1) {
            push @{$parsedData{'args'}{$key}}, $value;
        } else {
            $parsedData{'args'}{$key} = $value;
        }
        ++$lwrArgsCount;
    }
    $lwrArgsCount = 0;

    return $nextPos;
}

=over 4

=item getModsArgs($secPos, $argsNum, $args, $key1, $key2)

This subroutine used to get mods flags' arguments in cmdline.
It takes next cmdline element index ($secPos), total number of arguments
in cmdline ($argsNum), reference to splited arguments in cmdline ($args),
 flag mods.js ($key1) and mods flag ($key2) as keys for $parsedDatat.

=cut

sub getModsArgs{
    my ($secPos, $argsNum, $args, $key1, $key2) = @_;
    my $value = '';
    my $lwrArgsCount = 0;
    my $secArg;
    my @argsValue = ();

    $value = '';
    $lwrArgsCount = 0;
    while ($secPos < $argsNum && @$args[$secPos] =~ /$MODSFLAGARGS/) {
        $secArg = $1;
        $value = $secArg;
        push @argsValue, $value;
        ++$secPos;
        ++$lwrArgsCount;
    }

    if ($lwrArgsCount == 1) {
        &addArgsValue($key1, $key2, $argsValue[0]);
    } else {
        &addArgsValue($key1, $key2, \@argsValue);
    }

    return $secPos;
}

=over 4

=item B<addArgsValue($key1, $key2, \@argsValue)>

This subroutine adds the arguments value into the nested hash table.

=back

=back

=cut

sub addArgsValue{
    my ($key1, $key2, $secArg) = @_;

    if (exists $parsedData{'args'}{$key1}{$key2}) {
        if ('ARRAY' eq ref $secArg) {
            if ('ARRAY' eq ref $parsedData{'args'}{$key1}{$key2}[0]) {
                push @{$parsedData{'args'}{$key1}{$key2}}, $secArg;
            } else {
                my @argsArr = ();
                @argsArr = ($parsedData{'args'}{$key1}{$key2}, $secArg);
                $parsedData{'args'}{$key1}{$key2} = \@argsArr;
            }
        } else {
            push @{$parsedData{'args'}{$key1}{$key2}}, $secArg;
        }
    } else {
        $parsedData{'args'}{$key1}{$key2} = $secArg;
    }
}

=over 4

=item B<getModsFlags()>

This subroutine parses the flags for MODS.

=cut

sub getModsFlags{
    my ($nextPos, $argsNum, $args, $key1) = @_;
    my ($nextArg, $key2, $secPos, $secArg);

    while ($nextPos < $argsNum && @$args[$nextPos] =~ /$MODSFLAG/) {
        $nextArg = $1;
        $key2 = $nextArg;
        $secPos = $nextPos + 1;

        # deal with same flag indicated multiple times

        if (exists $parsedData{'args'}{$key1}{$key2}) {
            if ('ARRAY' cmp ref $parsedData{'args'}{$key1}{$key2}) {
                my @dupKeyList = ();
                @dupKeyList = ($parsedData{'args'}{$key1}{$key2});
                $parsedData{'args'}{$key1}{$key2} = \@dupKeyList;
            }
        }

        # e.g. "-chipargs" "-noinstance_display"
        if (checkspecialflag(\%parsedData, $key2, $nextArg) && $secPos < $argsNum) {
            $secArg = @$args[$secPos];
            $secArg =~ s/\"//g;
            $secArg =~ s/-//g;
            &addArgsValue($key1, $key2, $secArg);
            ++$secPos;
        } elsif ($secPos < $argsNum && @$args[$secPos] =~ /$MODSFLAGARGS/) {
            $secPos = &getModsArgs($secPos, $argsNum, $args, $key1, $key2);
        } else {
            &addArgsValue($key1, $key2, '');
        }

        $nextPos = $secPos;
    }

    return $nextPos;
}


=over 4

=item B<checkspecialflag()>

This subroutine checks if current flag is a special flag with more arguments.

=back

=back

=cut

sub checkspecialflag{
    my ($hashRef, $key, $thisArg) = @_;
    my %specialArgs = (
                        'chipargs' => 1,
        );

    if (exists $specialArgs{$thisArg}) {
        return 1;
    } else {
        return 0;
    }
}

=over 4

=item B<parseCfg($cfgFilePath)>

This subroutine parses trace info in .cfg file.

=cut

sub parseCfg{
    my $cfgFilePath = shift;
    if (open(my $ifh, '<', $cfgFilePath)) {
        my $lineCount = 0;
        while ( my $line = <$ifh> ) {
            my @cfgArgs = split(' ',$line);
            my $traceName = shift @cfgArgs;

            $lineCount += 1;
            $parsedData{'cfg'}{$traceName}{$lineCount} = &parseCfgArgs(@cfgArgs);
        }
        close $ifh;
    } else {
        die "\nCould not open $cfgFilePath file!";
    }
    return 0;
}

=over 4

=item B<parseCfgArgs(@argumentsArray)>

This subroutine parses options of each line of .cfg file.

=back

=cut

sub parseCfgArgs{
    my @args = @_;
    my $argsNum = scalar @args;
    my $lwrPos = 0;
    my $nextPos = 1;
    my %cfgLineArgsHash = ();
    my ($lwrArg, $key);

    while ($lwrPos < $argsNum) {
        if ($args[$lwrPos] =~ /$CFGFLAG/) {
            $lwrArg = $1;
            $key = $lwrArg;
            if ($args[$nextPos] =~ /$CFGFLAGARGS/) {
                $nextPos = &getCfgFlagArgs($nextPos, $argsNum, \@args, $key, \%cfgLineArgsHash);
            } else {
                $cfgLineArgsHash{$key} = '';
                ++$nextPos;
            }
        }
        $lwrPos = $nextPos++;
    }
    return \%cfgLineArgsHash;
}

=over 4

=item B<getCfgFlagArgs($nextPos, $argsNum, $args, $key, $cfgHash)>

This subroutine parses option arguments of each line of .cfg file, similar
to flag args for $cmdline.

=back

=back

=back

=cut

sub getCfgFlagArgs{
    my ($nextPos, $argsNum, $args, $key, $cfgHash) = @_;
    my $lwrArgsCount = 0;
    my $value = '';
    my $nextArg;

    $value = '';
    while ($nextPos < $argsNum && @$args[$nextPos] =~ /$CFGFLAGARGS/) {
        $nextArg = $1;
        $value = $nextArg;
        ++$nextPos;
        if ($lwrArgsCount == 1) {
            my @argsValue = ();
            @argsValue = ($cfgHash->{$key}, $value);
            $cfgHash->{$key} = \@argsValue;
        } elsif ($lwrArgsCount > 1) {
            push @{$cfgHash->{$key}}, $value;
        } else {
            $cfgHash->{$key} = $value;
        }
        ++$lwrArgsCount;
    }
    $lwrArgsCount = 0;

    return $nextPos;
}

=over 4

=item B<checkSimclkInCmdline()>

This subroutine checks if simclk flag in $cmdline.

=cut

sub checkSimclkInCmdline{
    my $self = shift;
    if (exists $parsedData{'args'}{'mdiag'}{'simclk'} ||
        exists $parsedData{'args'}{'mdiag'}{'simclk_mode'} ||
        exists $parsedData{'args'}{'mdiag'}{'simclk_profile'}) {
        if (exists $parsedData{'args'}{'mdiag'}{'simclk_profile'}) {
            $self->parseSimclk($parsedData{'args'}{'mdiag'}{'simclk_profile'});
        } else {
            $self->parseSimclk('SimClk_Profile.gold');
        }
        $self->parseWallclk();
    }
}

=over 4

=item B<parseSimclk($simclk_profile)>

This subroutine parses the profile file, e.g. SimClk_Profile.gold.

=back

=cut

sub parseSimclk{
    my ($self, $simclkName) = @_;
    my ($state, $action, $time);
    my $preLevel = 0;
    my $level = 0; # current level of tree
    my $levelName = '';
    my @levelStack = ();
    my $value = '';
    my ($node, $index, $arrIndex, $parentOfNewNode, $newActionNode);
    my @actionList = ();
    my $root = SimpleTree->new('root');

    if (open(my $ifh, '<', $simclkName)) {
        $root->add_child({},SimpleTree->new('total'));
        while ( my $line = <$ifh> ) {
            my @matches = $line =~ m/([ ]{4})/g; # match indentions
            my $lineLevel = scalar(@matches); # input line level start from 0
            my $beginSpace = $TABSPACE * $lineLevel;
            if ($line =~ /(\S+):[0-9]+:(\S+) = (\d+)$/) {
                # in the form of "PLATFORM_INIT:0:RM_ALLOC = 0'
                $state = $1;
                $action = $2;
                $time = $3;
                $value = $time + 0;
                $node = SimpleTree->new({"$state" => $value});
                $index = 0;
                while ($index <= $#actionList && !($actionList[$index] eq $action)) {
                    ++$index;
                }
                if ($index <= $#actionList) {
                    # this $action exists before
                    my $indexOfAction = $index + 1;
                    $parentOfNewNode = $root->children($indexOfAction);
                } else {
                    # this $action shows for the first time
                    $newActionNode = SimpleTree->new("$action");
                    $root->add_child({},$newActionNode);
                    push @actionList, $action;
                    $parentOfNewNode = $newActionNode;
                }
                for (my $i=0; $i < $lineLevel; $i++) {
                   $parentOfNewNode = $parentOfNewNode->get_last_child();
                }
                $stages{"$state:$action"} = $node;
                $parentOfNewNode->add_child({},$node);
            } elsif ($line =~ /^[ ]{$beginSpace}(\S+):(?:[0-9]+) = (\d+)$/) {
                # in the form of "PLATFORM_INIT:0 = 77"
                $state = $1;
                $time = $2;
                $value = $time + 0;
                $node = SimpleTree->new({"$state" => $value});
                $stages{"$state"} = $node;
                $parentOfNewNode = $root->children(0);
                for (my $i=0; $i < $lineLevel; $i++) {
                   $parentOfNewNode = $parentOfNewNode->get_last_child();
                }
                $parentOfNewNode->add_child({},$node);
            } else {}

        }


        $parsedData{'simclk_result'} = $root;

        close $ifh;
    } else {
        #TODO: need to differentiate Exit immediately from other expected failures
        # since both will not generate simclk profile
        warn "\nCould not open $simclkName file!";
        return 1;
    }

    return 0;
}

=over 4

=item B<parseWallclk()>

This subroutine parses wallclk time from mods.log.

=back

=back

=cut

sub parseWallclk{
    my $self = shift;
    my ($firstLine, $state, $action, $wallclk);
    if (open(my $ifh, '<', $MODSLOGFILENAME)) {
        $firstLine = <$ifh>;
        while ( my $line = <$ifh> ) {
            chomp $line;
            if ($line =~ /\[[0-9.]*\]\s([A-Za-z0-9_]*):[0-9]+, simclk.*, wallclk = ([0-9.]*)\ss/){
                # match e.g. [57120] TEST_RUN:0, simclk = 5398 ns, wallclk = 5.000000 s
                #            or [05/31/2019 00:57:08] [7552584] PLATFORM_INIT:0, simclk = 755258 ns, wallclk = 1156.000000 s
                $state = $1;
                $wallclk = $2;
                # wallclk will only be set for the first time, e.g. the problem of one extra TEST_SETUP
                if (exists $stages{"$state"} and not defined $stages{"$state"}->walltime()){
                    $stages{"$state"}->set_walltime($wallclk);
                    # warn $state.' '.$wallclk."\n";
                }
            } elsif ($line =~ /\[[0-9.]+\] ([A-Za-z0-9_]*):[0-9]+, ([A-Za-z0-9_]*)\[[0-9]+\]/) {
                # match e.g. [52876] T3D_SETUP_SURF:0, RM_ALLOC[9]
                $state = $1;
                # wallclk will only be set for the first time, e.g. the problem of one extra TEST_SETUP
                if (exists $stages{"$state"}) {
                    #check if $state exists and then match RM_ALLOC[9](simclk = 812 ns, wallclk = 0.000000 s),  RM_FREE[0](simclk = 0 ns, wallclk = 0.000000 s),  RM_CTRL[87](simclk = 1778 ns, wallclk = 2.000000 s)
                    while ($line =~ /,\s+([A-Za-z0-9_]*)\[[0-9]+\]\(simclk = [0-9]+ ns, wallclk = ([0-9.]+) s\)/g) {
                        $action = $1;
                        $wallclk = $2;
                        if (not defined $stages{"$state:$action"}->walltime()) {
                            $stages{"$state:$action"}->set_walltime($wallclk);
                        }
                    }

                }
            } else {}
        }
        close $ifh;
        return 0;
    } else {
        die "\nMODS Logger Error: Could not open mods.log file! Wallclk parse failed.";
    }
}

=over 4

=item B<parseUser($cmdline)>

This subroutine parses user and host info from $cmdline.

=back

=cut

sub parseUser{
    use Sys::Hostname;
    my ($self, $cmdline) = @_;
    my $username = undef;
    my $hostname = hostname;

    $username = (($^O eq 'MSWin32') ? getlogin : getpwuid($<))
             or warn "\nMODS Logger Error: Cannot determine real user name\n";

    $parsedData{'userName'} = $username;
    $parsedData{'hostName'} = $hostname;
    return 0;
}

=over 4

=item B<parseModsLog()>

This subroutine parses mods.log to get error, warn, chip, result and
"MDIAG: ReportTestResults()" result.

=cut

sub parseModsLog{
    my $self = shift;
    my ($startTime, $endTime);

    $parsedData{'errorLog'} = '';
    $parsedData{'warnLog'} = '';

    if (open(my $ifh, '<', $MODSLOGFILENAME)) {
        while ( my $line = <$ifh> ) {
            # log Error
            if ($line =~ /$ERRORREX/) {
                $parsedData{'errorLog'} = $parsedData{'errorLog'}.$line;
            }

            if ($line =~ /$WARNREX/) {
                $parsedData{'warnLog'} = $parsedData{'errorLog'}.$line;
            }

            chomp $line;  # get rid of \n
         
            # log start time 
            if ($line =~ /$STARTTIME/) {
                $parsedData{'runtime'}{'init'} = $line;
                my @startStrings = split ' ', $line, 2;
                $startTime = &colwertTime($startStrings[1]);
            }

            # log end time 
            if ($line =~ /$ENDTIME/) {
                $parsedData{'runtime'}{'end'} = $line;
                my @stripFrontString = split '\:', $line, 2;
                my @stripBackString = split '\[', $stripFrontString[1], 2;
                $endTime = &colwertTime($stripBackString[0]);
            }


            # log chip info
            if ($line =~ /$CHIPREX/) {
                $parsedData{'chipInfo'} = $1;
            }

            # log MODS result
            if ($line =~ /$RESULTFLAG/) {
                $parsedData{'result'} = $1;
            }

            # log MODS failed reason possibility 1
            if ($line =~ /$MDIAGREPORTFUNCTION/) {
                $line = <$ifh>;
                $line =~ s/\[.*\]\s+//g;      # substitute string in []
                $line =~ s/\(.*\)//g;         # substitute string in ()
                $line =~ s/\s+/ /g;           # substitute multiple space to 1 space
                $line =~ s/[\r\n]//g;         # drop newline and carriage newline
                $parsedData{'failureType'} = $line;
            }

        }
        close $ifh;

        # log time
        if (!defined($endTime)) # mods exits abnormally
        {
            my $lwrrentTime = localtime;
            $endTime = &colwertTime($lwrrentTime);
            $parsedData{'runtime'}{'end'} = $lwrrentTime;
        }
        my $duration = $endTime - $startTime;
        $parsedData{'runtime'}{'total'} = $duration;
    } else {
        die "\nMODS Logger Error: Could not open mods.log file!";
    }
}

=over 4

=item B<colwertTime($timeString)>

This subroutine colwert time string to proper format.

=back

=back

=cut

sub colwertTime{
    my $dstring = shift;
    my %monmap = (
            'Jan' => 0, 'Feb' => 1, 'Mar' => 2, 'Apr' => 3,
            'May' => 4, 'Jun' => 5, 'Jul' => 6, 'Aug' => 7,
            'Sep' => 8, 'Oct' => 9, 'Nov' => 10, 'Dec' => 11,
            );

    if ($dstring =~ /(\S+)\s+(\S+)\s+(\d+)\s+(\d{2}):(\d{2}):(\d{2})\s+(\d{4})/)
    {
        my ($wday, $month, $day, $h, $monmap, $s, $year) = ($1, $2, $3, $4, $5, $6, $7);
        my $mnumber = $monmap{$month}; # should handle errors here

        return timelocal( $s, $monmap, $h, $day, $mnumber, $year - $INITTIME );
    }
    else
    {
        die "\nMODS Logger Error: Fsrmat not recognized: ", $dstring, "\n";
        return 1;
    }
}

=over 4

=item B<parseDataSource()>

This is a hard-coded subroutine, subject to change. Get data source.

=back

=back

=cut

# Current data source is testgen, hard coded temporarily. Will modify once have
# support from testgen team.
sub parseDataSource{
    $parsedData{'dataSource'} = 'testgen';
    return 0;
}

1;
