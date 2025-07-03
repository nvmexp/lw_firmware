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

package ModsLogger;

=head1 NAME

ModsLogger - A Logger for MODS

=head1 SYNOPSIS

    use FindBin;
    use Data::Dumper;
    use lib "$FindBin::RealBin/logger";
    use ModsLogger;
    
    $modslogger = ModsLogger->new (\%options);
    $modslogger->start($cmdline, $rc, $model, $remoteFlag);
    $modslogger->reportInternalError($rc, $msg);

=head1 DESCRIPTION

This is module embeded to MODS to filter out needed log information for MODS developer.

B<NOTE:> This module works basing on MLParser.pm.

=cut

use strict;
use warnings;

use Carp;
use Scalar::Util qw(looks_like_number);
use List::Util qw(first max maxstr min minstr reduce shuffle sum);
use Data::Dumper;
use Cwd;
use Digest::file qw(digest_file_hex);

# for LWDataflow
use FindBin;
use lib ('/home/lw/utils/gpuwa/client-api/1.0/lib/perl5', '/home/lw/lib/perl5', '/home/lw/utils/lwperl/1.3/lib/perl5');
use LW::GPUWA::Dataflow::RestClient;

# the locatioin of MLParser is in the same directory of ModsLogger.pm
use MLParser;

use Const::Fast;
const my $VERSION => 'v1.8.1';
const my $LOGFILENAME => '.modsloggerdebug.log';
const my $TESTPASSSTR => 'all tests passed';

# %translateSubroutines stores the subroutines of class ModsLogger
my %translateSubroutines = (
    'argsLog'           =>  \&transArgs,
    'runTimeLog'        =>  \&transRuntime,
    'errorLog'          =>  \&transError,
    'timeProfileLog'    =>  \&transTimeProfile,
);

# subroutine in %option with value > 10 should be called.
# this magic number should be increased if subroutine number is larger than 10 in the future.
const my $SUBROUTINE_BASE => 10;

# value has two meanings:
# 1. priority
# 2. is enabled?
my %options = (
    'chipLog'        => 1,
    'userLog'        => 2,
    'runTimeLog'     => 3,
    'argsLog'        => 4,
    'errorLog'       => 5,
    'timeProfileLog' => 6,
);

my @logEntryArray = ();
my %commonEntryHash = ();
my $parsedData;
my $isPerfsim = 0;


=head1 METHODS

=head2 Constructor

=over 4

=item B<new(\%options, $debugMode, $attributes)>

This will return a ModsLogger object. It will accept one reference to a hash 
table which will become the option setting for ModsLogger. A varable to indicate
debug mode, and a reference for attributes. All other parameters
will be ignored.

Within new subroutine, C<init($$config, $debugMode)> will be called.

=back

=cut

sub new{
  	my ($class, $config, $debugMode, $attributes) = @_;

    my $self = {
        'debugMode'             =>  undef,
        'attributes'            =>  \%{$attributes},
        'options'               =>  \%options,
        'translateSubroutines'  =>  \%translateSubroutines,
    };

  	bless $self, $class;

    $self->init($config, $debugMode);

    return $self;
}


=head2 Member Subroutines

=over 4

=item B<init($config)>

This will take C<$config> as parameter to initialize current object. 
This will only be called by C<new(\%options)>

=back

=cut

sub init {
    my ($self, $config, $debugMode) = @_;

    # debug mode setting
    $self->{'debugMode'} = $debugMode;

    # copy $config into $self.
    foreach my $key (keys %{$config}) {
        if (exists $self->{'options'}{$key}) {
            # trick: option with value > $SUBROUTINE_BASE is enabled.
            $self->{'options'}{$key} += $SUBROUTINE_BASE;
        } else {
            die "\nMODS Logger Error: Wrong Input Options: $key\n";
        }
    }
    
    return 0;
}

=over 4

=item B<start()>

This will start MODS logger in normal logging status. 

=cut

sub start{
    my $self = shift;

    # set default value, parse data
    $self->prepare();

    # translate parsed info into hash form
    $self->translate();

    # post draw information
    $self->debugInfo('start posting data to ES...');
    $self->postData();
    $self->debugInfo("done...\n");

    return 0;
}

=over 4

=item B<prepare()>

This function call MLParser to parse all raw data into nested hash.

=over 4

=item B<MLParser>

This will parse needed data into a nested data hash reference 
named C<$parsedData>.

For more information, please check MLParser.pm

=back

=cut
sub prepare{
    my $self = shift;

    $self->debugInfo('set logger version, entry ID info...');

    $self->setVersion();
    $self->generateID();
    $self->getTrepID();

    $self->debugInfo('initialize parser...');

    my $parser = MLParser->new({});

    $self->debugInfo('start parsing...');

    $parsedData = $parser->startParse($self->{'attributes'}{'cmdline'});

    $self->debugInfo(Dumper($parsedData));
}

=over 4

=item B<setVersion()>

This subroutine set the version of MODS Logger into s_version.

=back

=cut

sub setVersion{
    my $self = shift;
    if (defined $VERSION){
        $commonEntryHash{'s_version'} = $VERSION;
        return 0;
    } else {
        die "\nNo version info!\n";
    }
}

=over 4

=item B<generateID()>

This subroutine will get lwrent directory of test and generate a special testID.
s_test_id is made up by the current working directory and a current time stamp.
s_test_dir is the current working working directory.
All these fields are common fields, and will be stored in each ES Entry.

=back

=cut
sub generateID{
    my $cwd = getcwd();
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
    $year += 1900;  # transfer to normal year
    $mon += 1;      # transfer to 1-12
    $commonEntryHash{'s_test_id'} = "$cwd-$year$mon$mday$hour$min$sec";
    $commonEntryHash{'s_test_dir'} = "$cwd";
}

=over 4

=item B<getTrepID()>

This subroutine will get test name from log file name and trepid from 
testname.log.

=cut

sub getTrepID{
    my $trepID = 'Unknown';
    my $testName = getTestName();
    if ($testName) {
        $commonEntryHash{'s_test_name'} = $testName;
        $trepID = grepTrepID($testName);
        $commonEntryHash{'s_trep_id'} = $trepID;  
    } else {
        $commonEntryHash{'s_test_name'} = "Unknown";
        $commonEntryHash{'s_trep_id'} = $trepID;
    }
    return;
}


=over 4

=item getTestName()

getTestName() get test name from the name of <test_name>.cmd.sh file,
and return the name.

=back

=cut

sub getTestName{
    my @files = glob "*.cmd.sh";
    if (@files) {
        my $testName = shift @files;
        $testName =~ s/\.cmd\.sh$//;
        return $testName;
    } else {
        return undef;
    }
}

=over 4

=item grepTrepID()

grepTrepID() will grep trep ID from <test_name>.log.
Meanwhile, <test_name> will be get from the name of <test_name>.cmd.sh file.

=back

=back

=back

=cut

sub grepTrepID{
    my $testName = shift;
    my $testLogName = $testName . ".log";
    my $nextLineIsID = 0;
    my $trepID = "Log not exists";
    if (-e $testLogName) {
        if (open(my $fh, '<', $testLogName)) {
            while( my $line = <$fh>)  {
                if ($nextLineIsID) {
                    if ($line =~ /INFO:\s+(\w+)\s+/){
                        $trepID = $1;
                    } else {
                        $trepID = "Log grep failed"
                    }
                    last;
                }   
                if ($line =~ /INFO:\s+trepId/) {
                    $nextLineIsID = 1;
                    next;
                }
            }
            close $fh;
            return $trepID;
        } else {
            return "Log failed to open";
        }
    } else {
        return $trepID;
    }
}

=over 4

=item B<translate()>

This function call MLParser to parse all raw data into nested hash.

=cut
sub translate{
    my $self = shift;

    $self->debugInfo('parse success! start translate...');

    $self->debugInfo('translate test model, remote mode, chip, user, host info...');

    $self->setTraceNum();
    $self->setTestMetadata();
    $self->setModel();
    $self->setRemote();
    $self->transChipInfo();
    $self->transUser();
    $self->identifyRTLSaveRestoreTest();

    $self->debugInfo('translate other info...');

    # FIXME
    # commonEntryHash will be different among different records if one/some of the subroutines change it and
    # not broadcast the change to other record. Please pay attention to the subrotine priority defined in *options*.
    # Notice: hash iteration is random, it means we don't know which element will be accessed at frist.
    # Below foreach go through hash according to value order(priority).
    #
    # post basic information
    my %optionsHash = %{$self->{'options'}};
    foreach my $key ( sort { $optionsHash{$a} <=> $optionsHash{$b} } keys %optionsHash) {
        next if ($key eq 'userLog' or $key eq 'chipLog');
        $self->debugInfo("dealing with $key...");
        if ($self->{'options'}{$key} > $SUBROUTINE_BASE) {
            $self->{'translateSubroutines'}{$key}->();
        }
    }

    # check arg to identify Perfsim model
    if ($isPerfsim) {
        foreach my $entry (@logEntryArray) {
            # broadcast change to all records. 
            $entry->{'s_gpuType'} = "Perfsim";
        }
        $commonEntryHash{'s_gpuType'} = "Perfsim";
    }

    # get test result
    $self->setResult();
}

=over 4

=item B<setModel()>

This subroutine sets the model of current test based on cmd.

=back

=cut

sub setModel{
    my $self = shift;
    if (defined $self->{'attributes'}{'model'}) {
        $commonEntryHash{'s_gpuType'} = $self->{'attributes'}{'model'};
        return 0;
    } else {
        die "\n Test model undefined!\n";
    }
}

=over 4

=item B<setRemote()>

This subroutine sets remote test info based on input from sim.pl.

=back

=cut

sub setRemote{
    my $self = shift;
    $commonEntryHash{'s_remote'} = $self->{'attributes'}{'remoteFlag'} ? 'true' : 'false';
    return 0;
}

=over 4

=item B<setTraceNum()>

This subroutine sets trace number based on test configure file.

=back

=cut

sub setTraceNum{
    my $self = shift;
    my $length = keys %{$parsedData->{'cfg'}};
    $commonEntryHash{'l_traceNum'} = $length;
    return 0;
}

=over 4

=item B<setTestMetadata()>

This subroutine sets test metadata based on -test_metadata "<metadata>".

For example:
input : -test_meta_data "testlist:/path/to/level/file hwcl:123456"
output: s_testlist        : "level/file"
        s_testlistPath    : "/path/to/"
        s_hwcl            : "123456"

=back

=cut

sub setTestMetadata{
    my $self = shift;
    my $metadata = $self->{'attributes'}{'metadata'};
    
    if (defined $metadata) {
        #remoing leading and tailing blank
        $metadata =~ s/^\s+|\s+$//g;
        # step1: parse all test metadata 
        my @metaDataStr = split(/\s+/, $metadata);
        foreach my $oneMetaDataStr (@metaDataStr) {
            my @pair = split(/:+/, $oneMetaDataStr);
            my $key = $pair[0];
            my $value = $pair[1];

            #remoing leading and tailing blank
            $key =~ s/^\s+|\s+$//g;
            $value =~ s/^\s+|\s+$//g;

            $commonEntryHash{"s_$key"} = $value;
        }

        # step2: post-process for special test metadata.
        if (exists $commonEntryHash{'s_testlist'}) {
            if ($commonEntryHash{'s_testlist'} =~ /(.*\/levels\/)(.*)/) {
                $commonEntryHash{"s_testlistPath"} = $1;
                $commonEntryHash{'s_testlist'} = $2;
            }
            else {
                die "testlist format is weird, </path/to>/levels/<testlist/name> is expected!
                current testlist is $commonEntryHash{'s_testlist'}\n";
            }
        }
    }
}

=over 4

=item B<transChipInfo()>

This subroutine translate chip info from parsed data to required post data 
format.

=back

=cut

sub transChipInfo{
    my $self = shift;

    if (exists $parsedData->{'chipInfo'}) {
        # for Amodel, Device ID in mods.log is still AMODEL
        if ($parsedData->{'chipInfo'} =~ /AMODEL/ and defined $self->{'attributes'}{'chip'}) {
            $parsedData->{'chipInfo'} = $self->{'attributes'}{'chip'};
        }
    } else {
        # VF test doesn't have Device Id
        # TODO: Need a more comprehensive way to detect if it's a VF or not
        if (defined $self->{'attributes'}{'chip'})
        {
            $parsedData->{'chipInfo'} = $self->{'attributes'}{'chip'};
        }
        else {
            die "\nNo chip info!\n";
        }
    }
    $commonEntryHash{'s_chipInfo'} = uc $parsedData->{'chipInfo'};
    return 0;
}

=over 4

=item B<transUser()>

This subroutine translate user info from parsed data to required post data 
format.

=back

=cut

sub transUser{
    if (exists $parsedData->{'userName'}) {
        $commonEntryHash{'s_userName'} = $parsedData->{'userName'};
    } else {
        die "\nNo username info!\n";
    }

    if (exists $parsedData->{'hostName'}) {
        $commonEntryHash{'s_hostName'} = $parsedData->{'hostName'};
    } else {
        die "\nNo hostname info!\n";
    }

    return 0;
}

=over 4

=item B<transError()>

This subroutine translate error info from parsed data to required post data 
format.

=back

=cut

sub transError{
    my $self = shift;
    if ($self->{'attributes'}{'rc'} or 
        (exists $parsedData->{'result'} and not ($parsedData->{'result'} eq $TESTPASSSTR)) or 
        (not exists $parsedData->{'result'})) {
        if (exists $parsedData->{'failureType'}) {
            $commonEntryHash{'s_failureType'} = $parsedData->{'failureType'};
        } else {
            warn "\nUnknown failure type!\n";
            $commonEntryHash{'s_failureType'} = 'DNC';
        }
        if (exists $parsedData->{'errorLog'}) {
            $commonEntryHash{'text_errorLog'} = $parsedData->{'errorLog'};
        } else {
            warn "\nNo error log!\n";
        }
        
        return 0;
    } else {
        return 1;
    }
}

=over 4

=item B<identifyRTLSaveRestoreTest()>

This subroutine will mark RTL save-restore tests

=back

=cut

sub identifyRTLSaveRestoreTest{
    my $self = shift;
    
    $commonEntryHash{'s_rtl_save_restore'} = 'NA';
    if ((exists $parsedData->{'args'}) && (exists $parsedData->{'args'}{'mdiag'}))
    {
        if (exists $parsedData->{'args'}{'mdiag'}{'enable_simrtl_save'})
        {
            $commonEntryHash{'s_rtl_save_restore'} = 'rtl_save';
        }
        elsif (exists $parsedData->{'args'}{'mdiag'}{'enable_simrtl_restore'})
        {
            $commonEntryHash{'s_rtl_save_restore'} = 'rtl_restore';
        }
        else
        {}
    }
    return 0;
}


# temoprary implementation, will change to map reduce in next milestone
sub goThroughHash {
    my ($hash, $argsStack, $fn) = @_;
    while (my ($key, $value) = each %$hash) {
        if ('HASH' eq ref $value) {
            push @$argsStack, $key;
            # print $key.' '.$value."\n";
            goThroughHash($value, $argsStack, $fn);
            pop @$argsStack;
        } else {
            push @$argsStack, $key;
            $fn->($argsStack, $key, $value);
            pop @$argsStack;
        }
    }
}

sub goThroughArray{
    my $baseArrayRef = shift;
    my @baseArray = @{$baseArrayRef};
    if (@baseArray) {
        my $firstEle = shift @baseArray;
        if ('ARRAY' eq ref $firstEle) {
            return '[ '.goThroughArray($firstEle).goThroughArray(\@baseArray);
        } else {
            return $firstEle.', '.goThroughArray(\@baseArray);
        }
    } else {
        # base condition
        return '] ';
    }
}

=over 4

=item B<transArgs()>

This will get MODS arguments in current tests.

=over 4

=item * B<goThroughHash($parsedData->{'args'}, \@argsStack, $fn)>

Relwrsively iterating the parsedData hash table to get all arguments.

=item * B<goThroughArray($value)>

Relwrsively iterating the hash value list to get all arguments.

=back

=cut

sub transArgs{
    my @argsStack = ();
    my %argsEntryHash = %commonEntryHash;
    $argsEntryHash{'s_docType'} = 'Arguments';
    $argsEntryHash{'s_args'} = '';
    goThroughHash($parsedData->{'args'}, \@argsStack, sub{
        my $argsEntryHashKey = '';
        my ($argsStack, $key, $value) = @_;
        foreach my $keyPart (@$argsStack) {
            $argsEntryHashKey = $argsEntryHashKey.'_'."$keyPart";
        }
        $argsEntryHashKey =~ s/^_//;
        if ('ARRAY' eq ref $value) {
            my $nestedArr2Str = '[ '.goThroughArray($value);
            $argsEntryHash{'s_args'} .= "$argsEntryHashKey = $nestedArr2Str; ";
        } else {
            $argsEntryHash{'s_args'} .= "$argsEntryHashKey = $value; ";
        }
    });

    if (exists $parsedData->{'cfg'}) {
        setCfgArgs(\%argsEntryHash);
    }


    if (($argsEntryHash{'s_gpuType'} eq "amod") and $argsEntryHash{'s_args'} =~ /lw_perfsim\.so/) {
        $isPerfsim = 1;
    }

    #print("\nInsert argument entry:\n".Dumper(\%argsEntryHash)."\n");
    push @logEntryArray, \%argsEntryHash;

    return 0;
}

=over 4

=item * B<setCfgArgs($returnHashRef)>

this subroutine go through the hash table and flat the nested list of hash tables for .cfg file arguments.
The parsed data are stored in $parsedData->{'cfg'}.
Data structure is shown below:

=begin text

                'cfg' => {
                 'trace_3d' => {
                  '1' => {
                   'o' => '/dir...to...test/comp_one_tri',
                   'i' => '/home/dir...to...trace/test.hdr'
                  }
                 }
                }

=end text

=back

=back

=cut

sub setCfgArgs{
    my $returnHashRef = shift;
    $returnHashRef->{'s_cfg_args'} = '';
    while (my ($trace, $value) = each %{$parsedData->{'cfg'}}) {
        # each type of trace
        while (my ($lineNum, $argsHashValue) = each %$value) {
            # each instance of same type trace
            my @cfgArgsStack = ($trace, $lineNum);
            # each line has only one trace, so set $keyForMD5 here.
            my $keyForMD5 = '';
            while (my ($key, $payload) = each %$argsHashValue) {
                # each arguments of trace instance
                push @cfgArgsStack, $key;
                my $cfgEntryHashKey = '';
                foreach my $keyPart (@cfgArgsStack) {
                    $cfgEntryHashKey = $cfgEntryHashKey.'_'."$keyPart";
                }
                $cfgEntryHashKey =~ s/^_//;
                if ('ARRAY' eq ref $payload) {
                    my $nestedArr2Str = '[ '.goThroughArray($payload);
                    $returnHashRef->{'s_cfg_args'} .= "$cfgEntryHashKey = $nestedArr2Str; ";
                } else {
                    if ($key eq "i") {
                        my $lwrMD5 = 0;
                        # $payload is dir to trace file
                        $keyForMD5 = $cfgEntryHashKey;
                        $keyForMD5 =~ s/_$key$/_md5/;
                        $lwrMD5 = getMD5($payload);
                        if ($lwrMD5 eq 0) {
                            $returnHashRef->{'s_cfg_args'} .= "$keyForMD5 = Genereate MD5 for Trace Failed; ";
                        } elsif ($lwrMD5 eq 1) {
                            $returnHashRef->{'s_cfg_args'} .= "$keyForMD5 = Not an ACE trace; ";
                        } else {
                            $returnHashRef->{'s_cfg_args'} .= "$keyForMD5 = $lwrMD5; ";
                        }
                        
                    }
                    $returnHashRef->{'s_cfg_args'} .= "$cfgEntryHashKey = $payload; ";
                }
                pop @cfgArgsStack;
            }
        }
    }
    return 0;
}

sub getMD5 {
    my $traceFile = shift;
    if (-e $traceFile) {
        my $firstLine = ""; 
        if (open(my $hdrFile, '<', $traceFile)) {
            $firstLine = <$hdrFile>;
            close $hdrFile;
            if ($firstLine =~ /# Generated by ACE2/) {
                return digest_file_hex($traceFile, "MD5");
            } else {
                return 1;
            }
        }
    } else {
        return 1;
    }
    return 0;
}

=over 4

=item B<transTimeProfile()>

This will get runtime (simclk, wallclk) for current test.

=cut

sub transTimeProfile{
    # my @typeNames = ('total', 'RM_ALLOC', 'RM_FREE', 'RM_CTRL');
    if (not exists $parsedData->{'simclk_result'}) {
        return 1;
    }

    if ((exists $commonEntryHash{'s_failureType'}) && ($commonEntryHash{'s_failureType'} eq 'DNC')) {
        # SimClk_Profile.gold most likely is incompleted so stop parsing it
        return 1;
    }

    my @typeNames = ('total');
    foreach my $typeName (@typeNames) {
        totalStageTimeProfile($typeName);
        mdiagStageTimeProfile($typeName);
    }

    return 0;
}

=over 4

=item * B<mdiagStageTimeProfile($typeName)>

This will translate the profile time tree in raw data for mdiag stage.

=back

=cut

sub mdiagStageTimeProfile{
    # Lwrrently, all simclk and wallclk info are stored in 3 subtrees of a tree named 
    # simclk_result, to the simclk and wallclk in each subtree, we need to go through
    # each tree and record the path from root. The path will be key of parsedData and 
    # value of simclk and wallclk will be the value of parsedData.
    # since the data structure of tree node is not friendly with this process,
    # will refactor the data structure and simplify this sub in the next milestone.
    my $type = shift;
    my $root = $parsedData->{'simclk_result'};
    my ($index, $rmAllocRootPointer, $rmFreeRootPointer, $rmCtrlRootPointer);

    $index = $root->get_index_for("RM_ALLOC");
    $rmAllocRootPointer = $root->children($index);

    $index = $root->get_index_for("RM_FREE");
    $rmFreeRootPointer = $root->children($index);

    $index = $root->get_index_for("RM_CTRL");
    $rmCtrlRootPointer = $root->children($index);

    $index = $root->get_index_for("$type");
    $root = $root->children($index);

    # move pointers to MDIAG_RUN subtree
    $index = $root->get_index_for('MDIAG_RUN');
    $root = $root->children($index);
    $rmAllocRootPointer = $rmAllocRootPointer->children($index);
    $rmFreeRootPointer = $rmFreeRootPointer->children($index);
    $rmCtrlRootPointer = $rmCtrlRootPointer->children($index);

    # return 1 if not defined $root;
    # here $root points to simclk_result -> total -> MDIAG_RUN,
    # then go through each child to save simclk and wallclk
    foreach my $child ($root->children()) {
        my %newHash = %commonEntryHash;
        $newHash{'s_docType'} = 'Runtime';
        # get current child time type, e.g. TEST_RUN
        my $timeType = first { defined($_) } keys %{$child->value()};
        # my $typeHashKey = 's_runtime_mdiag_'.$type.'_type';
        my $simTime = $child->value()->{"$timeType"};
        my $wallTime = $child->walltime() // 0;
        # my $newSubTypeHashKey = 's_runtime_mdiag_'.$type.'_subtype';
        my $newSubTypeHashKey = 's_runtime_mdiag_subtype';

        # prepare to get RM simclk and RM wallclk
        $index = $root->get_index_for($child->value());
        my $lwrRMAllocPointer = $rmAllocRootPointer->children($index);
        my $lwrRMFreePointer = $rmFreeRootPointer->children($index);
        my $lwrRMCtrlPointer = $rmCtrlRootPointer->children($index);
        my $totalRMSimtime = $lwrRMAllocPointer->value()->{"$timeType"} 
                           + $lwrRMFreePointer->value()->{"$timeType"}
                           + $lwrRMCtrlPointer->value()->{"$timeType"};

        my $lwrRMAllocPointerWalltime = $lwrRMAllocPointer->walltime() // 0;
        my $lwrRMFreePointerWalltime = $lwrRMFreePointer->walltime() // 0;
        my $lwrRMCtrlPointerWalltime = $lwrRMCtrlPointer->walltime() // 0;

        my $totalRMWalltime = $lwrRMAllocPointerWalltime
                            + $lwrRMFreePointerWalltime
                            + $lwrRMCtrlPointerWalltime;

        # get simtime and walltime for each mdiag stages
        $newHash{'s_runtime_mdiag_type'} = $timeType;
        $newHash{'l_runtime_mdiag_simclkValue'} = $simTime;
        $newHash{'d_runtime_mdiag_wallclkValue'} = $wallTime;
        $newHash{'l_runtime_mdiag_simclkNoRMValue'} = $simTime - $totalRMSimtime;
        $newHash{'d_runtime_mdiag_wallclkNoRMValue'} = $wallTime - $totalRMWalltime;
        if ($child->children() == 0) {
            # if current mdiag stage does not have child stages
            $newHash{'s_runtime_mdiag_subtype'} = 'Others';
        } else {
            # if current mdiag stage has child stages
            foreach my $subChild ($child->children()) {
                my $subKey = first { defined($_) } keys %{$subChild->value()};
                my $subSimTime = $subChild->value()->{"$subKey"};
                my $subWallTime = $subChild->walltime() // 0;
                my %newSubHash = %newHash;

                $index = $child->get_index_for($subChild->value());
                my $subLwrrentRMAllocPointer = $lwrRMAllocPointer->children($index);
                my $subLwrrentRMFreePointer = $lwrRMFreePointer->children($index);
                my $subLwrrentRMCtrlPointer = $lwrRMCtrlPointer->children($index);
                my $subTotalRMSimtime = $subLwrrentRMAllocPointer->value()->{"$subKey"} 
                                      + $subLwrrentRMFreePointer->value()->{"$subKey"} 
                                      + $subLwrrentRMCtrlPointer->value()->{"$subKey"};

                my $subRMAllocPointerWalltime = $lwrRMAllocPointer->walltime() // 0;
                my $subRMFreePointerWalltime = $lwrRMFreePointer->walltime() // 0;
                my $subRMCtrlPointerWalltime = $lwrRMCtrlPointer->walltime() // 0;

                my $subTotalRMWalltime = $subRMAllocPointerWalltime
                                       + $subRMFreePointerWalltime
                                       + $subRMCtrlPointerWalltime;

                $newSubHash{'l_runtime_mdiag_simclkValue'} = $subSimTime;
                $newSubHash{'d_runtime_mdiag_wallclkValue'} = $subWallTime;
                $newHash{'l_runtime_mdiag_simclkNoRMValue'} = $subSimTime - $subTotalRMSimtime;
                $newHash{'d_runtime_mdiag_wallclkNoRMValue'} = $subWallTime - $subTotalRMWalltime;
                $newSubHash{'s_runtime_mdiag_subtype'} = $subKey;
                #print("\nInsert substage clock entry:\n".Dumper(\%newSubHash)."\n");
                push @logEntryArray, \%newSubHash;
                $simTime -= $subSimTime;
                $wallTime -= $subWallTime;
            }
            $newHash{'l_runtime_mdiag_simclkValue'} = $simTime;
            $newHash{'d_runtime_mdiag_wallclkValue'} = $wallTime;
            $newHash{'s_runtime_mdiag_subtype'} = 'Others';
        }

        #print("\nInsert stage clock entry:\n".Dumper(\%newHash)."\n");
        push @logEntryArray, \%newHash;
    }

    return 0;
}

=over 4

=item * B<totalStageTimeProfile($typeName)>

This will translate the profile time tree in raw data for all stages.

=back

=back

=cut

sub totalStageTimeProfile{
    my $type = shift;
    my $root = $parsedData->{'simclk_result'};
    my ($index, $rmAllocRootPointer, $rmFreeRootPointer, $rmCtrlRootPointer);

    $index = $root->get_index_for('RM_ALLOC');
    $rmAllocRootPointer = $root->children($index);

    $index = $root->get_index_for('RM_FREE');
    $rmFreeRootPointer = $root->children($index);

    $index = $root->get_index_for('RM_CTRL');
    $rmCtrlRootPointer = $root->children($index);

    $index = $root->get_index_for("$type");
    $root = $root->children($index);
    
    my $hashKey = 's_runtime_type';
    my $usedWallclk = 0;
    foreach my $child ($root->children()) {
        my %newHash = %commonEntryHash;
        $newHash{'s_docType'} = 'Runtime';

        my $timeType = first { defined($_) } keys %{$child->value()};

        $index = $root->get_index_for($child->value());
        my $lwrRMAllocPointer = $rmAllocRootPointer->children($index);
        my $lwrRMFreePointer = $rmFreeRootPointer->children($index);
        my $lwrRMCtrlPointer = $rmCtrlRootPointer->children($index);

        my $totalRMSimtime = $lwrRMAllocPointer->value()->{"$timeType"} 
                           + $lwrRMFreePointer->value()->{"$timeType"}
                           + $lwrRMCtrlPointer->value()->{"$timeType"};

        my $lwrRMAllocPointerWalltime = $lwrRMAllocPointer->walltime() // 0;
        my $lwrRMFreePointerWalltime = $lwrRMFreePointer->walltime() // 0;
        my $lwrRMCtrlPointerWalltime = $lwrRMCtrlPointer->walltime() // 0;


        my $totalRMWalltime = $lwrRMAllocPointerWalltime
                            + $lwrRMFreePointerWalltime
                            + $lwrRMCtrlPointerWalltime;

        $newHash{"$hashKey"} = $timeType;
        $newHash{'l_runtime_simclkValue'} = $child->value()->{"$timeType"};
        $newHash{'d_runtime_wallclkValue'} = $child->walltime() // 0.0;
        $newHash{'l_runtime_simclkNoRMValue'} = $child->value()->{"$timeType"} - $totalRMSimtime;
        $newHash{'d_runtime_wallclkNoRMValue'} = $child->walltime() - $totalRMWalltime;
        #print("\nInsert total stage clock:\n".Dumper(\%newHash)."\n");
        push @logEntryArray, \%newHash;

        $usedWallclk += $newHash{'d_runtime_wallclkValue'};
    }

    # Deduce GPU_CLEANUP
    # TODO: It might not be a good solution to let logger understand profiling data. We may think of how to 
    # record GPU_CLEANUP time into log file in the future.
    # We don't quite care simclk in this stage so just callwlate walltime for now
    my $timeType = "GPU_CLEANUP";
    if (exists $commonEntryHash{'l_runtime_total'})
    {
        my %newHash = %commonEntryHash;
        $newHash{'s_docType'} = 'Runtime';
        $newHash{"$hashKey"} = $timeType;
        $newHash{'d_runtime_wallclkValue'} = $commonEntryHash{'l_runtime_total'} - $usedWallclk;
        push @logEntryArray, \%newHash;
    }
    else
    {
        die "\nFailed to callwlate wallclk in stage: $timeType\n";
    }

    return 0;
}

=over 4

=item B<transRuntime()>

This will get total running time for this test.

=back

=cut

sub transRuntime{
    if (exists $parsedData->{'runtime'}) {
        if (exists $parsedData->{'runtime'}{'total'}) {
            $commonEntryHash{'l_runtime_total'} = $parsedData->{'runtime'}{'total'};
        }
        if (exists $parsedData->{'runtime'}{'init'}) {
            $commonEntryHash{'s_runtime_begin'} = $parsedData->{'runtime'}{'init'};
        }
        if (exists $parsedData->{'runtime'}{'end'}) {
            $commonEntryHash{'s_runtime_end'} = $parsedData->{'runtime'}{'end'};
        }
    } else {
        die "\nNo total runtime info!\n";
    }
    return 0;
}

=over 4

=item B<setResult()>

This subroutine sets the test result from parsed data structure into 
s_result.

=back

=back

=cut

sub setResult{
    my $self = shift;
    my %newHash = %commonEntryHash;
    $newHash{'s_docType'} = 'Result';
    if (exists $parsedData->{'result'}) {
        $newHash{'s_result'} = $parsedData->{'result'};
    } else {
        # if result does not exist in $parsedData, there should be something wrong.
        if ($self->{'attributes'}{'rc'}) {
            # if $rc failed (none 0), for test failed
            warn "\nNo Mdiag result!\n";
            $newHash{'s_result'} = '$rc failed with no Mdiag result in mods.log';
        } else {
            # if $rc is 0, there sould be no problem
            # test returns 0 value, but no Mdiag results in mods.log 
            $newHash{'s_result'} = '$rc is 0 but test might fail';
        }
        # prepare to post error log
        if (exists $parsedData->{'failureType'}) {
            $newHash{'s_failureType'} = $parsedData->{'failureType'};
        } else {
            warn "\nUnknown failure type!\n";
            $newHash{'s_failureType'} = 'DNC';
        }
        if (exists $parsedData->{'errorLog'}) {
            $newHash{'text_errorLog'} = $parsedData->{'errorLog'};
        } else {
            warn "\nNo error log!\n";
        }
    }

    foreach my $entry (@logEntryArray) {
        $entry->{'s_result'} = $newHash{'s_result'};
    }

    #print("\nInsert result entry:\n".Dumper(\%newHash)."\n");
    push @logEntryArray, \%newHash;

    return 0;
}

=over 4

=item B<postData()>

This function use LWDataflow module to post data array into LWDataflow.

=back

=back

=cut
sub postData{
    my $self = shift;
    # somem comment
    if ($#logEntryArray < 0) {
        my %newHash = %commonEntryHash;
        $newHash{'s_docType'} = 'Common';
        $newHash{'s_result'} = 'all tests passed';
        push @logEntryArray, \%newHash;
    }

    $self->postDataToLwDataflow(@logEntryArray);

    return 0;
}

sub postDataToLwDataflow{
    my ($self, $logEntryArray) = @_;

    # needed by LwDataFlow for lwstomized index split 
    my $indexSuffix = $self->makeLwdataflowIndexSuffix();
    foreach my $entry (@logEntryArray) {
        $entry->{'_shard'} = $indexSuffix; 
    }

    my $data;
    my $length = @logEntryArray;
    $data=\@logEntryArray;

    if ($self->{'debugMode'}) {
        open(my $fh, '+>', $LOGFILENAME) or warn 'Could not create log file!';
        print $fh Dumper($data);
        close $fh;
    }
  
    $self->debugInfo('initialize LWDataflow service...');
    my $service = LW::GPUWA::Dataflow::RestClient->new();
    $service->service_url('http://gpuwa/dataflow');     # url of dataflow endpoint, without trailing "/{team-project}/posting" part
    $service->project_name('arch_mods-logger');         # the dataflow project name.   
    $service->query_params($data);                      # data - Reference to Array of HashRef's. Each Hash will posted as an individual Document to elastic search.
    
    $self->debugInfo('start posting...');

    eval {
        $service->execute();                            # post to dataflow
    };
    print "\nLWDataflow Response: ";
    print Dumper $service->response();

    $self->debugInfo("$length records have been posted.");

    return 0;
}

sub getData{
    return \@logEntryArray;
}


# now we use lwstomized index split strategy, now let's have 6 indices per month 
sub makeLwdataflowIndexSuffix{
    my $suffix = '';
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
    $year += 1900;  # transfer to normal year
    $mon += 1;      # transfer to 1-12
    my $mindex = int($mday / 5);
    $mindex = 5 if $mindex > 5;
    $suffix = sprintf "%.4d%.2d-%.2d", $year, $mon, $mindex;
    return $suffix;
} 


sub debugInfo{
    my @args = @_;
    my $self = shift @args;
    my $msg = shift @args;
    if ($self->{'debugMode'}) {
        print "\nMODS Logger: $msg ";
    }
}

=over 4

=item B<reportInternalError()>

This subroutine is called when modslogger.pl finds some problems.
This will start MODS logger in report problem status, C<$msg> is the error 
message.

=back

=cut

sub reportInternalError{
    my $self = shift;
    $commonEntryHash{'s_failureType'} = $self->{'attributes'}{'failureType'};
    $commonEntryHash{'text_errorLog'} = $self->{'attributes'}{'msg'};
    $self->setVersion();
    $self->setResult();
    $self->generateID();
    $self->getTrepID();
    $self->postData();

    return 0;
}


=head1 ACKNOWLEDGEMENTS

=over 4

=item * SimpleTree.pm is based on Rob Kinyon E<lt>rob.kinyon@iinteractive.comE<gt>'s Tree.pm.

=back

=cut

1;


