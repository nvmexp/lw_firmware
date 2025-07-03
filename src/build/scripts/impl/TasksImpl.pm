
package Tasks;

# < File orginization >
#
#   Tasks.pm
#       The raw data of Tasks definication
#       Located in 'build' dir of each modules (PMU/DPU/SEC)
#
#   TasksImpl.pm
#       The implementation and functions for Tasks.
#       Shared by RTOS-Flcn builds
#

use strict;

use Carp;                              # for 'croak', 'cluck', etc.
use Utils;                             # import rmconfig utility functions
use util::UtilsUproc;                  # for 'utilParseHalParamBlock'
use Rmconfig::CfgHash;
use chipGroups;                        # imports CHIPS from Chips.pm and chip functions
use Groups;
use GenFile;

use ovlCheck::OverlayCheck;            # Overlay size check for RTOS falcons

our @ISA = qw( Groups Messages );

# HACK to get access to $UPROC from major script (e.g. rtos-flcn-script.pl)
our $UPROC;
*UPROC  = \$main::UPROC;

my $Chips;                             # ref to obj chipGroups
my $EMIT = undef;                      # ref to GenFile to emit g_xxx from template

#
# \brief Resolve the contents of an architecture-split hash into a flat format.
#
sub _archResolve {
    (@_ == 2) or croak 'usage: obj->_hashResolve( HASH-REF )';

    my ($self, $hashRef) = @_;

    if (! (defined $hashRef->{COMMON} or
           defined $hashRef->{FALCON} or
           defined $hashRef->{RISCV})) {
        # No architecture split, don't do anything.
        return;
    }

    my $arch = $self->{PARAMETER_ARCH};
    my %newHash;

    # Get common settings first, so that they may be overridden
    if (defined $hashRef->{COMMON}) {
        my $commonRef = cfgHashRef(delete $hashRef->{COMMON}, 'FULL NAME COMMON');
        @newHash{keys %$commonRef} = values %$commonRef;
    }
    # Load architecture-specific settings.
    if (defined $hashRef->{$arch}) {
        my $archRef = cfgHashRef(delete $hashRef->{$arch}, 'FULL NAME ARCH');
        @newHash{keys %$archRef} = values %$archRef;
    }
    # Delete both architectures.
    if (defined $hashRef->{FALCON}) {
        delete $hashRef->{FALCON};
    }
    if (defined $hashRef->{RISCV}) {
        delete $hashRef->{RISCV};
    }

    # Place new keys into the root.
    @$hashRef{keys %newHash} = values %newHash;
}

# parseOstaskDefine()
#
# Parse 'OSTASK_DEFINE' field
#  * colwert list format to hash
#  * parse per-chip properties if any.
#        STACK_SIZE   => [
#                           512 => DEFAULT,
#                           576 => [ GM10X_and_later, ],
#                        ],
#
#    find a match and colwert the array to plain value:
#        STACK_SIZE   => 576,
#
sub parseOstaskDefine {
    (@_ == 2) or croak 'usage: obj->parseOstaskDefine( TASK-REF )';

    my ($self, $taskRef) = @_;

    # colwert the list format to hash
    my $listRef = delete $taskRef->{'OSTASK_DEFINE'};
    my $osDefinesRef = {};

    $osDefinesRef = cfgHashRef($listRef, $taskRef->{FULL_NAME});

    # Resolve architecture-specific settings
    $self->_archResolve($osDefinesRef);

    # check and assign default properties first
    my $name = $taskRef->{NAME};
    $name =~ s/^\w+TASK_(\w+)$/$1/;

    my %defaultTaskProp;
    if ($self->{PARAMETER_ARCH} eq 'FALCON') {
        %defaultTaskProp = (
            MAX_OVERLAYS_IMEM_LS  => 1,
            MAX_OVERLAYS_IMEM_HS  => 0,
            MAX_OVERLAYS_DMEM     => 1,
            OVERLAY_STACK         => "OVL_INDEX_ILWALID",
            OVERLAY_DMEM          => "OVL_INDEX_ILWALID",
            NAME                  => '"'.$name.'"',
            PRIV_LEVEL_EXT        => 2,
            PRIV_LEVEL_CSB        => 2,
            TASK_IMEM_PAGING      => "LW_FALSE",
            USING_FPU             => "LW_FALSE",
        );
    }
    elsif ($self->{PARAMETER_ARCH} eq 'RISCV') {
        %defaultTaskProp = (
            NAME                  => '"'.$name.'"',
            PRIV_LEVEL_EXT        => 2,
            PRIV_LEVEL_CSB        => 2,
            USING_FPU             => "LW_FALSE",
        );
    }

    foreach ( keys %defaultTaskProp ) {
        if (!exists $osDefinesRef->{$_}) {
            $osDefinesRef->{$_} = $defaultTaskProp{$_}
        }
    }

    # parse per-chip specific information
    my $value;
    my $matchChips;

    foreach my $attr (keys %{$osDefinesRef}) {
        $value = $osDefinesRef->{$attr};

        if (ref($value) eq 'ARRAY') {
            $value = UtilsUproc::parseHalParamBlock($taskRef->{NAME}."::$attr", $value);
            $osDefinesRef->{$attr} = $value;
        }
    }

    $taskRef->{'OSTASK_DEFINE'} = $osDefinesRef;
}

# parseOsWkrThdDefine()
#
# Parse 'OSWKRTHD_DEFINE' and 'JOBS' field
#  * colwert list format to hash
#
sub parseOsWkrThdDefine {
    (@_ == 2) or croak 'usage: obj->parseOsWkrThdDefine( TASK-REF )';

    my ($self, $wkrThdRef) = @_;

    # colwert the list format to hash
    my $osDefinesRef = delete $wkrThdRef->{'OSWKRTHD_DEFINE'};

    $osDefinesRef = cfgHashRef($osDefinesRef, $wkrThdRef->{FULL_NAME});

    # Resolve architecture-specific settings
    $self->_archResolve($osDefinesRef);

    # sanity check on 'OSWKRTHD_DEFINE' content
    if (defined $osDefinesRef->{'STACK_SIZE'}) {
        $self->error("For worker thread, 'STACK_SIZE' should be specified in 'JOBS->{OSJOB_DEFINE}', not in 'OSWKRTHD_DEFINE'\n");
    }
    if (defined $osDefinesRef->{'MAX_OVERLAYS_IMEM_LS'}) {
        $self->error("For worker thread, 'MAX_OVERLAYS_IMEM_LS' should be specified in 'JOBS->{OSJOB_DEFINE}', not in 'OSWKRTHD_DEFINE'\n");
    }
    if (defined $osDefinesRef->{'MAX_OVERLAYS_IMEM_HS'}) {
        $self->error("For worker thread, 'MAX_OVERLAYS_IMEM_HS' should be specified in 'JOBS->{OSJOB_DEFINE}', not in 'OSWKRTHD_DEFINE'\n");
    }
    if (defined $osDefinesRef->{'MAX_OVERLAYS_DMEM'}) {
        $self->error("For worker thread, 'MAX_OVERLAYS_DMEM' should be specified in 'JOBS->{OSJOB_DEFINE}', not in 'OSWKRTHD_DEFINE'\n");
    }

    my %defaultWkrThdProp = (
        PRIV_LEVEL_EXT   => 2,
        PRIV_LEVEL_CSB   => 2,
        DATA_PTR         => "NULL",
        TASK_IMEM_PAGING => "LW_FALSE",
        USING_FPU        => "LW_FALSE",
    );

    # check and assign default properties first
    foreach ( keys %defaultWkrThdProp ) {
        if (!exists $osDefinesRef->{$_}) {
            $osDefinesRef->{$_} = $defaultWkrThdProp{$_}
        }
    }

    # Bring in other properties from jobs. Max of jobs for now.
    my $stackSizeMax = 0;
    my $imemOverlaysMax_LS = 0;
    my $imemOverlaysMax_HS = 0;
    my $dmemOverlaysMax = 0;
    my $jobsRaw = delete $wkrThdRef->{JOBS};

    # Sanity check on 'JOBS' field
    if (! defined $jobsRaw || (scalar $jobsRaw == 0)) {
        $self->error("Missing 'JOBS' field or 'OSJOB_DEFINE' in worker thread '%s'\n", $wkrThdRef->{NAME});
    }

    # parsing '{JOBS}->[    OBJOS_DEFINE => [...], OBJOS_DEFINE => [...] ]'
    # colwert the raw data into an array of OS_DEFINE hashes
    my @jobOsDefs;
    my ($key, $val);
    for (my $i = 0; $i < @$jobsRaw; $i += 2) {
        $key = $jobsRaw->[$i];
        $val = $jobsRaw->[$i+1];

        if($key ne 'OSJOB_DEFINE') {
            $self->error("Unknown field in '%s->{JOBS}' section, it has to be 'OSJOB_DEFINE': %s\n",
                         $wkrThdRef->{NAME}, $key);
        }

        if(! defined $val || ref($val) ne 'ARRAY' ) {
            $self->error("Parsing error in '%s->{JOBS}' section. \n",
                         $wkrThdRef->{NAME});
        }

        push @jobOsDefs, cfgHashRef($val);
    }

    $wkrThdRef->{JOBS} = \@jobOsDefs;

    foreach my $jobOsDef (@jobOsDefs) {
        # Resolve architecture-specific settings
        $self->_archResolve($jobOsDef);

        if ($jobOsDef->{'STACK_SIZE'} > $stackSizeMax) {
            $stackSizeMax = $jobOsDef->{'STACK_SIZE'};
        }

        if ($jobOsDef->{'MAX_OVERLAYS_IMEM_LS'} > $imemOverlaysMax_LS) {
            $imemOverlaysMax_LS = $jobOsDef->{'MAX_OVERLAYS_IMEM_LS'};
        }

        if (defined $jobOsDef->{'MAX_OVERLAYS_IMEM_HS'}) {
            if ($jobOsDef->{'MAX_OVERLAYS_IMEM_HS'} > $imemOverlaysMax_HS) {
                $imemOverlaysMax_HS = $jobOsDef->{'MAX_OVERLAYS_IMEM_HS'};
            }
        }

        if (defined $jobOsDef->{'MAX_OVERLAYS_DMEM'}) {
            if ($jobOsDef->{'MAX_OVERLAYS_DMEM'} > $dmemOverlaysMax) {
                $dmemOverlaysMax = $jobOsDef->{'MAX_OVERLAYS_DMEM'};
            }
        }
    }

    # Assign stack size and max overlays to worker thread. Add one overlay for
    # the default worker thread overlay.
    $osDefinesRef->{'STACK_SIZE'} = $stackSizeMax;
    if (defined $osDefinesRef->{'OVERLAY_IMEM'} && $osDefinesRef->{'OVERLAY_IMEM'} ne 'OVL_INDEX_ILWALID') {
        $osDefinesRef->{'MAX_OVERLAYS_IMEM_LS'} = $imemOverlaysMax_LS + 1;
    }
    else {
        $osDefinesRef->{'MAX_OVERLAYS_IMEM_LS'} = $imemOverlaysMax_LS ;    
    }

    $osDefinesRef->{'MAX_OVERLAYS_IMEM_HS'} = $imemOverlaysMax_HS;

    if (defined $osDefinesRef->{'OVERLAY_DMEM'} && $osDefinesRef->{'OVERLAY_DMEM'} ne 'OVL_INDEX_ILWALID') {
        $osDefinesRef->{'MAX_OVERLAYS_DMEM'} = $dmemOverlaysMax + 1;
    }
    else {
        $osDefinesRef->{'MAX_OVERLAYS_DMEM'} = $dmemOverlaysMax;  
    }

    $wkrThdRef->{'OSWKRTHD_DEFINE'} = $osDefinesRef;
}

# Build {CFG_FEATURE_H} with all features from chip-config::Features.pm
# the disable items are set to zero in the hash
sub buildCfgFeatureList {

    (@_ == 2) or croak 'usage: obj->buildCfgFeatureList( CHIP-CFG-CMD )';

    my ($self, $chipCfgCmd) = @_;

    my $cmd = "$chipCfgCmd --print all-features";
    my @features = `$cmd`;
    foreach my $feature (@features) {
        chomp $feature;
        $self->{CFG_FEATURE_H}->{$feature} = 1;
    }

    $cmd = "$chipCfgCmd --print disabled-features";
    my @disabledFeatures = `$cmd`;
    foreach my $feature (@disabledFeatures) {
        chomp $feature;
        $self->{CFG_FEATURE_H}->{$feature} = 0;
    }
}


sub validateTaskNames {

    (@_ == 1) or croak 'usage: obj->validateTaskNames( )';

    my $self = shift;

    # known exceptions -- defined in Tasks.pm but not chip-config Features.pm
    my %exceptionH = (
        'PMUTASK_IDLE'  => 1,
    );

    foreach my $taskName (@{$self->grpItemsListRef()}) {
        if ((not defined $self->{CFG_FEATURE_H}->{$taskName}) && ! $exceptionH{$taskName}) {
            $self->warning("Task '$taskName' in Tasks.pm is not defined in Features.pm\n");
        }
    }
}



sub checkOverlayImemSize {

    (@_ == 2) or croak 'usage: obj->checkOverlayImemSize(FILENAME)';

    croak "checkOverlayImemSize() require Profile Util module being initialized"        unless $UPROC->{PROFILE_UTIL};

    my ($self, $filename) = @_;

    my $OvlCheckRef;        # ref to obj OverlayCheck

    $OvlCheckRef = OverlayCheck->new($self);        utilExitIfErrors();
    $OvlCheckRef->init($filename);                  utilExitIfErrors();

    my $ProfileUtil = $UPROC->{PROFILE_UTIL};

    my $ovlSizeTable = new Table();
    push @{$ovlSizeTable->{HEADER}}, 'used', 'worst case', 'overlays';

    # print header for verbose message
    if (2 <= $self->{VERBOSE}) {
        print "\n";
        printf("Profile     : %s\n", $self->{PROFILE_NAME});
        printf("IMEM blocks : %d\n", $OvlCheckRef->{NUM_IMEM_BLOCKS});
        print "\n";

    }

    foreach my $taskName (@{$self->grpItemsListRef()}) {

        # skip the overlay check if this task is diabled in chip-config explicit
        next if (defined $self->{CFG_FEATURE_H}->{$taskName}) and ($self->{CFG_FEATURE_H}->{$taskName} == 0);

        @{$ovlSizeTable->{DATA}} = ();
        # Get a reference to the current task
        my $taskRef = $self->grpItemRef($taskName);

        foreach my $ovlItms (sort keys %{$taskRef->{'OVERLAY_COMBINATION'}}) {
            my $profilesRef = $taskRef->{'OVERLAY_COMBINATION'}->{$ovlItms}->{PROFILES};
            my $profileStr;

            printf("$taskName OVL $ovlItms: \n")        if ($self->{VERBOSE} == 1);

            my $bEnabled;
            # skip the check if the target profile is not in the list
            if ($profilesRef) {
                $profileStr = join(', ', @{$profilesRef});

                $bEnabled = $ProfileUtil->isTargetProfileInPatternList($profilesRef);
                if ($self->{VERBOSE} == 1) {
                    printf("    %s by profile list : [ %s ] \n".
                           "    ... %s\n\n",
                        $bEnabled? 'ENABLED ' : 'DISABLED',
                        $profileStr,
                        $ProfileUtil->getPatternListCommentStr($profilesRef) );
                }

                next unless $bEnabled;

            } else {
                # TODO: remove this part after colwerting all chip lists to profile lists
                # skip the check if none of the chip is enabled for this profile
                my $chipsRef   = $taskRef->{'OVERLAY_COMBINATION'}->{$ovlItms}->{CHIPS};
                $profileStr = join(', ', @{$chipsRef});

                $bEnabled = chipEnabledListFromFamilyList(@$chipsRef);

                if ($self->{VERBOSE} == 1) {
                    printf("    %s by chip list\n\n",
                        $bEnabled? 'ENABLED ' : 'DISABLED' );
                }

                next unless $bEnabled;
            }

            my $ovlListRef = $taskRef->{'OVERLAY_COMBINATION'}->{$ovlItms}->{OVERLAYS};

            my $ovlStr = join(', ', @{$ovlListRef});
            my ($ret, $tableRowRef) = $OvlCheckRef->checkOverlayImemSize($taskRef, $ovlListRef);

            #
            # Value of $ret indicates error or warning case
            # -1: ERROR: overlay exceeds IMEM size
            # 0: Success. Overlay fits easily in available IMEM
            # 1: WARNING: if changing overlay offset could exceed IMEM size
            # 2: WARNING: Changing offset gets overlay size close to available IMEM
            # 3: ERROR: Overlay listed in Tasks.pm but not found on the binary
            #
            if (-1 == $ret) {
                $self->error("**********!!!!!!!!uCode out of IMEM space!!!!!!!!**********\n".
                "The overlay combination fails IMEM size check for profile %s, task %s:\n".
                "    Used: %d\tAvailable: %d\n".
                "    [ %s ] => [ %s ]",
                $self->{PROFILE_NAME}, $taskName,
                $tableRowRef->[0], $OvlCheckRef->{NUM_IMEM_BLOCKS},
                $profileStr, $ovlStr);
            }
            elsif (1 == $ret) {
                $self->warning("#############uCode Running out of IMEM space soon#############\n".
                "For worst-case arrangement overlay combination exceeds IMEM size for profile %s, task %s:\n".
                "    Used: %d\tAvailable: %d\n".
                "    [ %s ] => [ %s ]",
                $self->{PROFILE_NAME}, $taskName,
                $tableRowRef->[1], $OvlCheckRef->{NUM_IMEM_BLOCKS},
                $profileStr, $ovlStr);
            }
            elsif(2 == $ret) {
                $self->warning("uCode Running out of IMEM space soon\n".
                "For worst-case arrangement overlay combination has only %d block available for profile %s, task %s:\n".
                "    [ %s ] => [ %s ]",
                $OvlCheckRef->{NUM_IMEM_BLOCKS} - $tableRowRef->[1],
                $self->{PROFILE_NAME}, $taskName,
                $profileStr, $ovlStr);
            }
            if (3 == $ret) {
                $self->error("Couldnt find overlay %s in OverlaysXmem.pm. Possibly a typo. Profile %s, task %s:\n".
                "    [ %s ] => [ %s ]",
                $tableRowRef->[0], $self->{PROFILE_NAME}, $taskName,
                $profileStr, $ovlStr);
            }

            #
            # if verbose print enabled add to the table for printing
            # if some of the overlays could not be found in the section table,
            # then skip adding that entry
            #
            if(2 <= $self->{VERBOSE} and 3 != $ret) {
                push @{$ovlSizeTable->{DATA}}, [@{$tableRowRef}];
            }
        }

        if (2 <= $self->{VERBOSE} and @{$ovlSizeTable->{DATA}} > 0) {
            print "$taskName";
            $ovlSizeTable->printTable()
        }
    }

    # capture any error in checkOverlayImemSize() and fail the script
    utilExitIfErrors();
}

sub printOstaskHeader {
    (@_ == 2) or croak 'usage: obj->printOstaskHeader( OUTPUT-FILE )';

    my ($self, $outputFile) = @_;

    $EMIT = GenFile->new('C', $outputFile,
                         { update_only_if_changed => 1, } );  # PARAMS-HASH-REF

    croak "Error: Could not create output file '$outputFile' : $!"     if ! $EMIT;

    # Print start of include guard
    $EMIT->print(<<__CODE__);
//
// This file is automatically generated by 'rtos-flcn-script.pl' - DO NOT EDIT!
//
// Task defines
//

#ifndef _G_TASKS_H_
#define _G_TASKS_H_

__CODE__

    # Print task defines
    $self->_printOstaskDefines();

    # Print worker thread defines
    $self->_printOsWkrThdDefines();

    # Print end of include guard
    $EMIT->printf("#endif //_G_TASKS_H_\n");

    $EMIT->closefile();
    $EMIT = undef;
}

sub _printOstaskDefines {
    (@_ == 1) or croak 'usage: obj->printOstaskDefines()';

    my ($self) = @_;

    foreach my $taskName (@{$self->grpItemsListRef()}) {
        # Get a reference to the current task
        my $taskRef = $self->grpItemRef($taskName);
        my $osDefinesRef = $taskRef->{'OSTASK_DEFINE'};
        my $name = $taskRef->{NAME};
        $name =~ s/^\w+TASK_(\w+)$/$1/;

        my ($attr, $value);
        while (($attr, $value) = each %{$osDefinesRef}) {
            $EMIT->printf("#define %-40s %s\n", "OSTASK_ATTRIBUTE_$name\_$attr", $value);
        }
        $EMIT->print("\n");
    }
}

sub _printOsWkrThdDefines {
    (@_ == 1) or croak 'usage: obj->printOsWkrThdDefines()';

    my ($self) = @_;

    foreach my $wkrThdName (@{$self->grpItemsListRef()}) {
        # Get a reference to the current task
        my $wkrThdRef = $self->grpItemRef($wkrThdName);
        my $wkrThdDefinesRef = $wkrThdRef->{'OSWKRTHD_DEFINE'};
        my $name = $wkrThdRef->{NAME};
        $name =~ s/^\w+TASK_(\w+)$/$1/;

        my ($attr, $value);
        while (($attr, $value) = each %{$wkrThdDefinesRef}) {
            $EMIT->printf("#define %-40s %s\n", "OSTASK_ATTRIBUTE_$name\_$attr", $value);
        }
        $EMIT->print("\n");
    }
}

# parseOverlayCombination()
#
# Parse 'OVERLAY_COMBINATION' field, and
# colwert the original format : $taskRef->{'OVERLAY_COMBINATION'}
#   [ FERMI, ]  => [
#       [ '.pmgr', '.libFxp', '.lib'... ],
#       [ '.pmgr', '.libPerf', ... ],
#   ],
#   [ ALL, ]    => [
#       ['.pmgr', '.libFxp', '.libi2c',... ],
#   ],
#
# to the following :
#   _ITEM1 => {
#       CHIPS    => [ FERMI, ],
#       OVERLAYS => [ '.pmgr', '.libFxp', '.lib'... ],
#   },
#   _ITEM2 => {
#       CHIPS    => [ FERMI, ],
#       OVERLAYS => [ '.pmgr', '.libPerf', ... ],
#   },
#   _ITEM3 => {
#       CHIPS    => [ ALL, ],
#       OVERLAYS => [ '.pmgr', '.libFxp', '.libi2c',... ],
#   },
#
# The name '_ITEM1', '_ITEM2'... is added to facilitate parsing, they should be used in any
# generated content
#
sub parseOverlayCombination {
    (@_ == 2) or croak 'usage: obj->parseOverlayCombination( TASK-REF )';

    my ($self, $taskRef) = @_;

    my $listRef = delete $taskRef->{'OVERLAY_COMBINATION'};
    my $hashRef = {};

    for (my $i = 0, my $count = 1; $i < @$listRef; $i += 2) 
    {
        my $items = $listRef->[$i];

        #
        # Test the first item to know whether it is a profile list
        #   If it starts with small captical and numbers before the first dash '-'
        #    => 'pmu-', 'fbfalcon-', 'i2c-', 'sec2-', 'acr_pmu-'
        #   If it has * or . in the string
        #    => '...pmu-gm20x', 'pmu-*'
        my $firstItem = (scalar @$items)? $items->[0] : '';
        my $bProfileList = ($firstItem && ($firstItem =~ m/^[a-z0-9_]+\-/ || $firstItem =~ m/(\*|\.)/ ))? 1: 0;
        for (my $j = 0; $j < @{$listRef->[$i+1]}; $j++, $count++)
        {
            # if first item is a profile name, put the list to PROFILES
            # otherwise, put the list to CHIPS
            if ($bProfileList) {
                $hashRef->{'_ITEM'.$count}->{PROFILES} = $items;
            } else {
                $hashRef->{'_ITEM'.$count}->{CHIPS} = $items;
            }

            $hashRef->{'_ITEM'.$count}->{OVERLAYS} = $listRef->[$i+1][$j];
        }
    }

    $taskRef->{'OVERLAY_COMBINATION'} = $hashRef;
}


# parseTasksInfo()
#
# entry function to parse content in Tasks.pm
#
sub parseTasksInfo {
    (@_ == 1) or croak 'usage: obj->parseTasksInfo()';

    my ($self) = @_;

    foreach my $taskName (@{$self->grpItemsListRef()}) {
        # Get a reference to the current task
        my $taskRef = $self->grpItemRef($taskName);

        $taskRef->{NAME} = $taskName;
        $taskRef->{FULL_NAME} = "$self->{FULL_NAME}: $taskName";

        # Initialize taskType to a default value
        my $taskType = "0";

        foreach my $entry (keys %{$taskRef}) {
            if ($entry eq 'OSTASK_DEFINE') {
                # Sanity check to make sure a worker thread doesn't also have a task define
                if ($taskType eq "wkrThd") {
                    $self->error("A worker thread cannot have an OSTASK_DEFINE\n");
                }
                $taskType = "task";

                $self->parseOstaskDefine($taskRef);
            }
            elsif ($entry eq 'OVERLAY_COMBINATION') {
                $self->parseOverlayCombination($taskRef);
            }
            elsif ($entry eq 'OSWKRTHD_DEFINE') {
                # Sanity check to make sure a task doesn't also have a worker thread define
                if ($taskType eq "task") {
                    $self->error("A task cannot have an OSWKRTHD_DEFINE\n");
                }
                $taskType = "wkrThd";

                $self->parseOsWkrThdDefine($taskRef);
            }
        }

        # TODO : add check for undefined fields?
    }
}

sub new
{
    (@_ == 5 or @_ == 6) or croak 'usage: obj->new( NAME, PROFILE-NAME, LW-ROOT, VERBOSE[, UPROC-ARCH] )';

    my ($class, $name, $profile, $lwroot, $verbose, $arch) = @_;
    if (!defined $arch) {
        $arch = 'FALCON';
    }
    else {
        $arch = uc $arch;
    }

    my $tasksRaw = utilReadConfigFile($name, 'Tasks.pm');

    my $self = Groups->new($name, $tasksRaw);

    $self->{PARAMETER_ARCH} = $arch;
    $self->{NAME}           = $name;
    $self->{PROFILE_NAME}   = $profile;
    $self->{LW_ROOT}        = $lwroot;
    $self->{VERBOSE}        = $verbose;

    bless $self, $class;

    $self->parseTasksInfo();

    return $self;
}

1;
