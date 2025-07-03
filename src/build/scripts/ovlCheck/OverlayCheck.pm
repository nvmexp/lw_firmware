##!/usr/bin/perl -w

#
# Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

package OverlayCheck;

use constant IMEM_SIZE_THRESHOLD    => 4;
use constant PAGE_SIZE              => 256;

use strict;

use POSIX;
use File::Spec;
use Carp;

use impl::ProfilesImpl;                # Implementations for Profiles
use ovlCheck::Table;

use chipGroups;                        # imports CHIPS from Chips.pm and chip functions
                                       # [Note] OverlayCheck.pm relay on other module
                                       # (ex. Tasks.pm) to init chipGroups

# colwerts byte address to block number
sub getBlockNumber {
    my $addr = shift;
    return floor($addr / PAGE_SIZE);
}

# get size in number of blocks
sub getBlockSize {
    my $size = shift;
    return ceil($size / PAGE_SIZE);
}

# checkOverlayImemSize()
#
# return
# -1    : ERROR: overlay exceeds IMEM size
#  0    : Success. Overlay fits easily in available IMEM
#  1    : WARNING: if changing overlay offset could exceed IMEM size
#  2    : WARNING: Changing offset gets overlay size close to available IMEM
#  3    : ERROR: Couldnt find the overlay in OverlaysXmem.pl. Possibly a typo
sub checkOverlayImemSize {
    ( @_ == 3 ) or croak 'usage: obj->checkOverlayImemSize( TASK-REF, OVERLAY-LIST-REF )';

    my ( $self, $taskRef, $overlayListRef ) = @_;
    my $tasksRef = $self->{PARENT_REF};    # get $tasksRef to show error message
    my $overlayTable = $self->{OVERLAY_TABLE};
    my %blockOclwpancyList = ();
    my $overlaysString = join( ' ', @{$overlayListRef} );
    #
    # List of all overlays supported on the falcon - i.e. List of all PMU, DPU or
    # Sec2 overlays
    #
    my $supportedOverlayList = $self->{OVERLAYS_REF};

    foreach my $overlay (@{$overlayListRef})
    {
        my $bOptionalOverlay = 1  if $overlay =~ m/\?$/;

        # drop the tailed '?' if any
        $overlay =~ s/\?$//;
        # remove the prefix '.' if any
        $overlay =~ s/^\.//;
        my $overlayInfoRef = $supportedOverlayList->getOverlayRefFromName($overlay);

        if(!$overlayInfoRef)
        {
            my @retData = ($overlay, $overlaysString);
            # Return warning error code with overlay that couldnt be found
            return (3, \@retData);
        }

        my $startAddress = $overlayTable->getEntry( $overlay, 'Addr' );

        # Unable to find the overlay in the current ucode
        if (not defined $startAddress)
        {
            # Check if the overlay is marked optional
            if($bOptionalOverlay)
            {
                if(3 <= $self->{VERBOSE})
                {
                    $tasksRef->warning($taskRef->{NAME}.": Overlays %s NOT enabled on %s. Ignoring the overlay\n %s \n", $overlay, $tasksRef->{PROFILE_NAME}, $overlaysString);
                }
            }
            else
            {
                $tasksRef->error($taskRef->{NAME}.": Overlays %s NOT enabled on %s, but not marked as optional\n %s \n", $overlay, $tasksRef->{PROFILE_NAME}, $overlaysString);
            }
            next;
        }

        my $startBlock = getBlockNumber $startAddress;
        my $size = $overlayTable->getEntry( $overlay, 'Size' );

        # get how many bytes left in the first page
        my $startBlockBytesUsed = PAGE_SIZE - ($startAddress % PAGE_SIZE);
        # now fill the smaller of overlay size vs bytes used in the first page
        $startBlockBytesUsed = $size > $startBlockBytesUsed ? $startBlockBytesUsed : $size;
        # add the bytes in the first page
        $blockOclwpancyList{$startBlock++} += $startBlockBytesUsed;
        # Reduces the size by the number of bytes used by first page
        $size -= $startBlockBytesUsed;
        # Fill pages until all the bytes are exhausted
        while($size > 0)
        {
            $blockOclwpancyList{$startBlock} += $size > PAGE_SIZE ? PAGE_SIZE : $size;
            $size -= $blockOclwpancyList{$startBlock};
            $startBlock ++;
        }
    }

    # Some overlays are placed before the resident code, So using "residentCodeSize" is more robust.
    # Also resident Code always starts in a new page. So we can use the size directly to find
    # the number of pages it uses.
    my $residentCodeBlocks = getBlockSize($overlayTable->getEntry( 'text', 'Size' ));

    # Add resident code to total size
    my $totalBlocks = scalar keys(%blockOclwpancyList) + $residentCodeBlocks;

    # Now get how many extra pages are needed if overlays are arranged as per
    # the worst case possible
    my $extraBlocks = $self->getWortCaseImemSizeIncrease(\%blockOclwpancyList);
    my @tableRow = ($totalBlocks, $totalBlocks + $extraBlocks, $overlaysString);

    # overlay exceeds IMEM size return error
    if ( $totalBlocks >  $self->{NUM_IMEM_BLOCKS}) {
        return (-1, \@tableRow);
    }
    # warning if changing overlay offset could exceed IMEM size
    elsif ( $totalBlocks + $extraBlocks > $self->{NUM_IMEM_BLOCKS} ) {
        return (1, \@tableRow);
    }
    # also warn if overlays rearrangement would be close to IMEM size
    elsif ( $totalBlocks + $extraBlocks + IMEM_SIZE_THRESHOLD > $self->{NUM_IMEM_BLOCKS} ) {
        return (2, \@tableRow);
    }
    return (0, \@tableRow);
}

#
# A 2 byte overlay can fit in one page or in 2 pages - offset 255 of page 1
# and offset 0 of page 2.
# This subroutine finds the total number of bytes used by contiguous page. Checks
# how many pages are actually needed. Increases that amount by 1. If the current
# arrangment is already done that way then there is nothing to do. Else increase
# the number of pagesor blocks needed by one.
#
sub getWortCaseImemSizeIncrease {
    ( @_ == 2 ) or croak 'usage: obj->getWorstCaseImemSizeIncrease( BLOCK_OCLWPANCY_LIST )';

    my ($self ,$blockOclwpancyListRef) = @_;

    # actual size in bytes
    my $sizeInBytes = 0;
    # Number of blocks used by current arrangement
    my $blocks = 0;
    # Number of additional blocks needed
    my $additionalBlocks = 0;

    # Sort they keys as integers (Default sorted as string)
    foreach my $i ( sort {$a <=> $b} keys %{$blockOclwpancyListRef} )
    {
         $sizeInBytes += $blockOclwpancyListRef->{$i};
         $blocks++;
         # if there is a break in the list, then we are at the end of the current
         # list of contiguous blocks
         if(not defined $blockOclwpancyListRef->{$i + 1}) {
             # If the blocks used is not according to worst case arrangement
             if( ceil($sizeInBytes / PAGE_SIZE) == $blocks) {
                 # add one more block
                 $additionalBlocks++;
             }
             # now reset and start the next set of contiguous blocks
             $sizeInBytes = 0;
             $blocks = 0;
         }
    }
    return $additionalBlocks;
}

sub getOverlayTableForProfile {
    ( @_ == 1 ) or croak 'usage: obj->getOverlayTableForProfile()';

    my $self = shift;

    # find readelf files in the path
    my $filePath;

    if($self->{OP_MODE} eq 'pmu') {
        $filePath = File::Spec->catdir($self->{LW_ROOT}, $self->{OP_MODE}.'_sw',
            'prod_app/_out/',$self->{BUILD},'g_c85b6_*.h');
    } elsif(($self->{OP_MODE} eq 'dpu') && (($self->{BUILD} eq 'gv100') || ($self->{BUILD} eq 'tu10x') || ($self->{BUILD} eq 'tu116') || ($self->{BUILD} eq 'ga10x' || ($self->{BUILD} eq 'ga10x_boot_from_hs')) || ($self->{BUILD} eq 'ad10x' || ($self->{BUILD} eq 'ad10x_boot_from_hs')))){
        $filePath = File::Spec->catdir($self->{LW_ROOT}, 'uproc/disp/dpu/src/_out',
            $self->{BUILD}, 'g_dispflcn*.h');
    } elsif( $self->{OP_MODE} eq 'dpu') {
        $filePath = File::Spec->catdir($self->{LW_ROOT}, 'uproc/disp/dpu/src/_out',
            $self->{BUILD}, 'g_dpuuc*.h');
    } elsif( $self->{OP_MODE} eq 'sec2') {
        $filePath = File::Spec->catdir($self->{LW_ROOT}, 'uproc/sec2/src/_out/',
            $self->{BUILD}, 'g_sec2uc*.h');
    }

    $filePath = glob($filePath);

    croak "Couldn't find generated .h file\n" unless $filePath;
    open FH, "< $filePath" or croak "Couldn't open file : '$filePath'\n";

    my $overlayTable = new Table();
    push @{$overlayTable->{HEADER}}, 'Name', 'Addr', 'Size';
    my @tableRow = (0,0,0);
    my $residentCodeOffset = undef;
    my $residentCodeSize = undef;
    my $numImemOverlays = undef;

    #for each line in the .h file
    while(<FH>)
    {
        # Parse "nbImemOverlays<ANY CHARACTER> <HEX NUMBER>"
        if($_ =~ m%nbImemOverlays.+(0x[0-9a-f]+)\,$%)
        {
            $numImemOverlays = hex($1);
            next;
        }
        # Parse "appResidentCodeOffset<ANY CHARACTER> <HEX NUMBER>"
        if($_ =~ m%appResidentCodeOffset.+(0x[0-9a-f]+)\,$%)
        {
            $residentCodeOffset = hex($1);
            next;
        }
        # Parse "appResidentCodeSize<ANY CHARACTER> <HEX NUMBER>"
        if($_ =~ m%appResidentCodeSize.+(0x[0-9a-f]+)\,$%)
        {
            $residentCodeSize = hex($1);
            next;
        }
        # Parse  string { /* .start = */ 0x00100 , /* .size = */ 0x0177 }, // imem_test (HS)
        # to match pattern %start =.+(0x[0-9a-f]+) .+size =.+(0x[0-9a-f]+).+// (\w+)( \(HS\))?$%
        # Each unit of the pattern and its match is listed immediately below it.
        # Hopefully this makes it easier to tweak the pattern next time.
        #
        # %'start ='    '.+'    '(0x[0-9a-f]+) '    '.+'    'size ='    '.+(0x[0-9a-f]+)'   '.+'    '// '     '(\w+)'         '( \(HS\))?'             '( \(RESIDENT\))?' $%
        #   start =      */         0x00100         , /*.    size =          */ 0x0177       },      //      imem_test            (HS)------>optional      (RESIDENT)------>optional
        #
        if($_ =~ m%start =.+(0x[0-9a-f]+) .+size =.+(0x[0-9a-f]+).+// (\w+)( \(HS\))?( \(RESIDENT\))?$%)
        {
            @tableRow = ($3, hex($1), hex($2));
            push @{$overlayTable->{DATA}}, [@tableRow];
        }
    }

    croak "Couldnt find resident code address and size " if (not defined $residentCodeOffset and not defined $residentCodeSize);
    warn 'Couldnt parse all IMEM overlays. Please contact script owner shashikanths@lwpu.com' if (not defined $numImemOverlays or $numImemOverlays != @{$overlayTable->{DATA}});

    # create entry for resident code information
    @tableRow = ("text", $residentCodeOffset, $residentCodeSize);
    push @{$overlayTable->{DATA}}, [@tableRow];

    # Set table key as "Name", this makes it possible to query table for things like
    # "get size of overlay <Name>" from the table
    $overlayTable->setKey('Name');
    $self->{OVERLAY_TABLE} = $overlayTable;
}

# init()
#
# init important data for object OverlayCheck
# expecially to get 'NUM_IMEM_BLOCKS' from Profiles.pm
sub init {
    ( @_ == 2 ) or croak 'usage: obj->init(FILENAME)';

    my ($self, $filename) = @_;
    my ($profile, $buildName, $opmode);

    $profile = $self->{PARENT_REF}->{PROFILE_NAME};
    $profile =~ /^(\w+)-(\w+)$/;  # pmu-gf11x ->pmu, gf11x;   dpu-v0201 -> dpu, v0201
    ($opmode, $buildName) = ($1, $2);

    # get info and imem size for this profile
    my $Profiles    = Profiles->new('OverlayCheck');
    my $profileRef  = $Profiles->grpItemRef($buildName);
    my $overlaysRef = Overlays->new('OverlayCheck', 'imem', $filename);

    $self->{PROFILE_NAME}       = $profile;
    $self->{BUILD}              = $buildName;
    $self->{OP_MODE}            = $opmode;
    $self->{PROFILE_REF}        = $profileRef;
    $self->{OVERLAYS_REF}       = $overlaysRef;
    $self->{NUM_IMEM_BLOCKS}    = $Profiles->imemPhysicalSize($profileRef) / 256; # 1 block = 256 bytes

    $self->{LW_ROOT}            = $self->{PARENT_REF}->{LW_ROOT};
    $self->{VERBOSE}            = $self->{PARENT_REF}->{VERBOSE};

    $self->getOverlayTableForProfile();
}

sub new {
    ( @_ == 2 ) or croak 'usage: OverlayCheck->new( PARENT-REF )';

    my ($class, $parentRef) = @_;
    my $self    = {};

    $self->{PARENT_REF} = $parentRef;

    bless $self, $class;
    return $self;
}

1;
