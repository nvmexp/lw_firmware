#
# Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

package Overlays;

# < File orginization >
#
#   OverlaysImem.pm
#       The raw data of IMEM Overlays definition
#       Located in 'build' dir of each module (RTOS-based falcon)
#
#   OverlaysDmem.pm
#       The raw data of DMEM Overlays definition (if DMEM VA is supported)
#       Located in 'build' dir of each module (RTOS-based falcon)
#
#   OverlaysImpl.pm
#       The implementation and functions for Overlays.
#       Shared by RTOS-Flcn builds
#

use strict;

use Carp;                              # for 'croak', 'cluck', etc.
use Utils;                             # import rmconfig utility functions
use util::UtilsUproc;                  # for 'utilParseHalParamBlock'
use Rmconfig::CfgHash;
use chipGroups;                        # imports CHIPS from Chips.pm and chip functions

use Groups;

our @ISA = qw( Groups Messages );

# HACK to get access to $UPROC from major script (e.g. rtos-flcn-script.pl)
our $UPROC;
*UPROC  = \$main::UPROC;

# return ref of overlay item for the matching section name
# the section name format can be with or without the prefix '.':
#   '.imem_init' and 'imem_init' are both token
sub getOverlayRefFromName
{
    (@_ == 2) or croak 'usage: obj->getOverlayRefFromName( OVERLAY-NAME )';

    my $self = shift;
    my $sectionName = shift;

    # remove the prefix '.' if any
    $sectionName =~ s/^\.//;

    foreach my $overlayName (@{$self->grpItemsListRef()})
    {
        my $overlayRef = $self->grpItemRef($overlayName);

        return $overlayRef     if ($sectionName eq $self->getSectionName($overlayName));
    }
    return;
}

# parseOverlaysDmemStackInfo
#
# Parse the STACK_* field to retrieve the stack size for current chip (based on
# falcon-profile)
#
# STACK_CMDMGMT =>
#     {
#         DESCRIPTION     => 'Stack for the CMDMGMT task.',
#         MAX_SIZE_BLOCKS => 2,
#         ENABLED_ON      => [ dPASCAL_and_later, -GP100, -GP10B, -GV11B, ],
#    }
sub parseOverlaysDmemStackInfo
{
    (@_ == 1) or croak 'usage: obj->parseOverlaysDmemStackInfo()';
    my $self = shift;

    croak 'Function only supports DMEM overlay' if $self->{MEM_TARGET} ne 'DMEM';
    my %stackInfo;

    # Loop though $dmemOverlaysRaw entries
    foreach my $dmemOverlayName (@{$self->grpItemsListRef()}) {
        my $overlayRef = $self->grpItemRef($dmemOverlayName);

        #check if it's a stack entry
        if (($dmemOverlayName =~ m/^STACK_.+/) && $self->isOverlayEnabled($dmemOverlayName)) {
            # Colwert stack name to camelCasing (To match entry in Tasks.pm)
            $stackInfo{lcfirst(utilTagToCamelCase($dmemOverlayName))} = $overlayRef->{MAX_SIZE_BLOCKS};
        }
    }

    $self->{STACKS} = \%stackInfo;
}

# Return true when the overlay is enabled, or false otherwise
sub isOverlayEnabled
{
    (@_ == 2) or croak 'usage: obj->isOverlayEnabled( OVERLAY-NAME )';

    my ($self, $overlayName) = @_;

    my $overlayRef = $self->grpItemRef($overlayName);

    my $profilesRef = $overlayRef->{PROFILES};

    my $ProfileUtil = $UPROC->{PROFILE_UTIL};

    # dump some information to help migration
    printf("OVL $overlayName:\n")           if ($UPROC->{opt}->{verbose} == 1);

    my $bEnabled;
    # Check profiles first
    if ($profilesRef) {
        $bEnabled = $ProfileUtil->isTargetProfileInPatternList($profilesRef);

        if ($UPROC->{opt}->{verbose} == 1) {
            printf("    %s by profile list : [ %s ] \n".
                "    ... %s\n\n",
                $bEnabled? 'ENABLED ' : 'DISABLED',
                (join ', ', @$profilesRef),
                $ProfileUtil->getPatternListCommentStr($profilesRef) );
        }

        return $bEnabled;
    }

    my $chipsRef = $overlayRef->{ENABLED_ON};

    # scalar of the chip list is either 1 or 0
    $bEnabled = scalar chipEnabledListFromFamilyList(@$chipsRef);

    if ($UPROC->{opt}->{verbose} == 1) {
        printf("    %s by chip list\n\n",
            $bEnabled? 'ENABLED ' : 'DISABLED' );
    }

    return $bEnabled
}


# Return the name for the ELF output-section that will be constructed for
# the overlay.
# Format : imem_pmgrLibLoad
sub getSectionName
{
    (@_ == 2) or croak 'usage: obj->getSectionName( OVERLAY-NAME )';

    my $self = shift;
    my $overlayName = shift;

    my $overlayRef = $self->grpItemRef($overlayName);

    return      unless $overlayRef;

    return $overlayRef->{SECTION_NAME}     if $overlayRef->{SECTION_NAME};

    # If a name was specified in the .pm file, use it;
    # otherwise create a name from the overlay's item-name.
    if (!$overlayRef->{NAME}) {
        # pmgrLibLoad
        $overlayRef->{NAME} = lcfirst(utilTagToCamelCase($overlayName));
    }

    # imem_pmgrLibLoad
    $overlayRef->{SECTION_NAME} = (lc $self->{MEM_TARGET}).'_'.$overlayRef->{NAME};

    return $overlayRef->{SECTION_NAME};
}

sub readOverlaysConfigFile
{
    (@_ == 2) or croak 'usage: readOverlaysConfigFile( NAME, FILE-NAME )';

    my ($name, $fileName) = @_;

    my $dataRaw = utilReadConfigFile($name, $fileName);

    # check and expand any '__INCLUDE__'
    my $dataExpanded;
    while (my $item = shift @$dataRaw) {
        if ($item eq '__INCLUDE__') {
            my $includeFileName = shift @$dataRaw;

            my $dataIncluded = readOverlaysConfigFile($name ,$includeFileName);
            push @$dataExpanded, @$dataIncluded;
            next;
        }

        push @$dataExpanded, $item;

    }

    return $dataExpanded;

}

sub validateFields
{
    (@_ == 1) or croak 'usage: obj->validateFields()';

    my $self = shift;

    my %validFieldH = (
        NAME            => 1,
        ALIGNMENT       => 1,
        DESCRIPTION     => 1,
        INPUT_SECTIONS  => 1,
        ENABLED_ON      => 1,
        PROFILES        => 1,
        MAX_SIZE_BLOCKS => 1,
        KEEP_SECTIONS   => 1,
        FLAGS           => 1,
        LS_SIG_GROUP    => 1,
    );

    foreach my $overlayName (@{$self->grpItemsListRef()}) {
        my $overlayRef = $self->grpItemRef($overlayName);

        # check if any invalid fields
        foreach my $field (keys %$overlayRef) {
            utilError "$overlayName: Invalid Field '$field'"     unless $validFieldH{$field};
        }

        # check for required field -- ENABLED_ON or PROFILES
        if (not exists $overlayRef->{ENABLED_ON} and not exists $overlayRef->{PROFILES}) {
            utilError "Overlay '$overlayName' must have field 'ENABLED_ON' or 'PROFILES'";
        }

        # check for required field MAX_SIZE_BLOCKS for IMEM overlays
        if ($self->{MEM_TARGET} eq 'DMEM') {
            if (not exists $overlayRef->{MAX_SIZE_BLOCKS}) {
                utilError "DMEM Overlay '$overlayName' must have field 'MAX_SIZE_BLOCKS'";
            }
        }
    }
}


sub initializeHalFields
{
    (@_ == 1) or croak 'usage: obj->initializeHalFields()';

    my $self = shift;

    # so far only dmem overlay uses hal param block
    return unless $self->{MEM_TARGET} eq 'DMEM';

    # Loop though $dmemOverlaysRaw entries
    foreach my $overlayName (@{$self->grpItemsListRef()})
    {
        my $overlayRef = $self->grpItemRef($overlayName);

        # MAX_SIZE_BLOCKS can be different value on each chip
        $overlayRef->{MAX_SIZE_BLOCKS} = UtilsUproc::parseHalParamBlock("$overlayName: MAX_SIZE_BLOCKS", $overlayRef->{MAX_SIZE_BLOCKS});
    }
}
sub new
{
    (@_ == 4) or croak 'usage: obj->new( NAME, TARGET, FILENAME )';

    my ($class, $name, $target, $fileName) = @_;

    my $overlaysRaw = readOverlaysConfigFile($name, $fileName);

    my $self  = Groups->new($name, $overlaysRaw);

    $self->{MEM_TARGET}   = uc $target;

    bless $self, $class;

    $self->validateFields();

    $self->initializeHalFields();

    return $self;
}

1;
