#
# Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

package Sections;

# < File orginization >
#
#   RiscvSectionsCode.pm
#       The raw data of code sections definition
#       Located in 'build' dir of each module
#
#   RiscvSectionsData.pm
#       The raw data of data sections definition
#       Located in 'build' dir of each module
#
#   SectionsImpl.pm
#       The implementation and functions for Sections.
#       Shared by RISCV falcon builds
#

use strict;

use Carp;                              # for 'croak', 'cluck', etc.
use Utils;                             # import rmconfig utility functions
use util::UtilsUproc;                  # for 'utilParseHalParamBlock'
use Rmconfig::CfgHash;
use chipGroups;                        # imports CHIPS from Chips.pm and chip functions

use Groups;
use File::Basename;

our @ISA = qw( Groups Messages );

# HACK to get access to $UPROC from major script (e.g. rtos-flcn-script.pl)
our $UPROC;
*UPROC  = \$main::UPROC;

my $EMIT = undef;                      # ref to GenFile to emit g_xxx from template
my $EMIT_OBJREF = undef;               # object ref (if any) for emitted code

sub _printSectionDefines
{
    (@_ == 3) or croak 'usage: obj->_printSectionDefines( FIRST-SECTION-IDX, TYPE )';

    my ($self, $sectionIndex, $sectionType) = @_;

    $sectionType = uc $sectionType;

    my %permissionHash = (
        'r' => "USER_R",
        'w' => "USER_W",
        'x' => "USER_X",
        # Machine/Supervisor
        'R' => "KERNEL_R",
        'W' => "KERNEL_W",
        'X' => "KERNEL_X",
    );

    foreach my $sectionName (@{$self->grpItemsListRef()}) {
        next unless $self->isSectionEnabled($sectionName);

        my $sectionRef = $self->grpItemRef($sectionName);
        my $sectionRealName = $self->getSectionName($sectionName);
        my $location = uc $sectionRef->{LOCATION};

        if (($sectionType eq 'DATA' && $location =~ /IMEM_.*/) || ($sectionType eq 'CODE' && $location =~ /DMEM_.*/)) {
            $self->error("$sectionRealName: a section of type $sectionType cannot be located in $location!");
        }

        if (! $EMIT_OBJREF->{ODP_ENABLED}) {
            if (($location eq 'DMEM_ODP') || ($location eq 'IMEM_ODP')) {
                $self->error("$sectionRealName: paged sections not supported for this profile!");
            }
        }

        if (($EMIT_OBJREF->{NO_FB_SECTIONS}) && ($location eq 'FB') && (index($sectionRealName, 'dummySection') == -1)) {
            $self->error("$sectionRealName: FB sections are not allowed!");
        }

        $EMIT->printf("#if !__ASSEMBLER__\n");
        $EMIT->printf("  UPROC_SECTION_%s_PROTOTYPES(%s)\n", $sectionType, $sectionRealName);
        $EMIT->printf("#endif // __ASSEMBLER__\n");
        $EMIT->printf("#define %-50s (%d)\n",
                      "UPROC_SECTION_${sectionRealName}_REF", $sectionIndex);
        $EMIT->printf("#define %-50s UPROC_SECTION_LOCATION_${location}\n",
                      "UPROC_SECTION_${sectionRealName}_LOCATION");
        $EMIT->printf("#define %-50s (", "UPROC_SECTION_${sectionRealName}_PERMISSION");

        my $chRemain = length($sectionRef->{PERMISSIONS});
        my $permissionStr = "";
        foreach my $ch (split //, $sectionRef->{PERMISSIONS}) {
            $chRemain -= 1;
            $self->error("$sectionRealName: Invalid permissions character '$ch'")
                if (!defined $permissionHash{$ch});
            $self->error("$sectionRealName: Exelwtable sections cannot be located in $location")
                if ((uc $ch eq 'X') && ($location =~ /DMEM_.*/));

            $permissionStr .= sprintf(" UPROC_SECTION_PERMISSION_$permissionHash{$ch}");
            if ($chRemain == 0) {
                $permissionStr .= sprintf(")\n");
            }
            else {
                $permissionStr .= sprintf(" \\\n        %50s |", "");
            }
        }
        $EMIT->printf($permissionStr);

        $EMIT->printf("\n");

        if (exists $sectionRef->{CHILD_SECTIONS}) {
            my $childSections = $self->getChildSections($sectionName);

            foreach my $childSectionName (@{$childSections->grpItemsListRef()}) {
                next unless $childSections->isSectionEnabled($childSectionName);

                my $childSectionRealName = $childSections->getSectionName($childSectionName);
                my $childLocation = uc $sectionRef->{LOCATION};
                if (! $EMIT_OBJREF->{ODP_ENABLED}) {
                    if (($childLocation eq 'DMEM_ODP') || ($childLocation eq 'IMEM_ODP')) {
                        $self->error("$childSectionRealName (child section): " .
                                     "paged sections not supported for this profile!");
                    }
                }

                $EMIT->printf("#if !__ASSEMBLER__\n");
                $EMIT->printf("  UPROC_SECTION_%s_PROTOTYPES(%s)\n", $sectionType,
                            $childSectionRealName);
                $EMIT->printf("#endif\n");
                $EMIT->printf("#define %-50s (%d)\n",
                            "UPROC_SECTION_${childSectionRealName}_REF", $sectionIndex);
                $EMIT->printf("#define %-50s UPROC_SECTION_LOCATION_${childLocation}\n",
                            "UPROC_SECTION_${childSectionRealName}_LOCATION");
                $EMIT->printf("#define %-50s ($permissionStr",
                            "UPROC_SECTION_${childSectionRealName}_PERMISSION");

                $EMIT->printf("\n");
            }
        }

        $sectionIndex += 1;
    }

    # We can't generate a sections header from bad data, so exit if errors were detected
    utilExitIfErrors();

    return $sectionIndex;
}

sub _printSectionSource
{
    (@_ == 2) or croak 'usage: obj->_printSectionSource( MACRO-NAME )';

    my ($self, $macroName) = @_;

    foreach my $sectionName (@{$self->grpItemsListRef()}) {
        next unless $self->isSectionEnabled($sectionName);

        my $sectionRef = $self->grpItemRef($sectionName);
        my $sectionRealName = $self->getSectionName($sectionName);

        # Only real sections! Child sections are not placed in the tables
        $EMIT->printf("    %s(%s),\n", $macroName, $sectionRealName);
    }
}

# Print index symbols for disabled sections
sub _printIlwalidIndexSymbolDefs {
    (@_ == 1) or croak 'usage: obj->_printIlwalidIndexSymbolDefs( )';

    my ($self) = @_;

    $EMIT->printf("// Placeholder invalid index values for disabled child-sections:\n");

    foreach my $sectionName (@{$self->grpItemsListRef()}) {
        my $sectionRef = $self->grpItemRef($sectionName);

        if (exists $sectionRef->{CHILD_SECTIONS}) {
            my $childSections = $self->getChildSections($sectionName);

            foreach my $childSectionName (@{$childSections->grpItemsListRef()}) {
                next if $childSections->isSectionEnabled($childSectionName);

                my $childSectionRealName = $childSections->getSectionName($childSectionName);

                $EMIT->printf("char __section_%s\_index = 0;\n", $childSectionRealName);
            }
        }

        next if $self->isSectionEnabled($sectionName);

        my $sectionRealName = $self->getSectionName($sectionName);

        $EMIT->printf("char __section_%s\_index = 0;\n", $sectionRealName);
    }
}

# Print extern decls for index symbols for disabled sections
sub _printIlwalidIndexSymbolDecls {
    (@_ == 1) or croak 'usage: obj->_printIlwalidIndexSymbolDecls( )';

    my ($self) = @_;

    $EMIT->printf("// Refs to invalid index values for disabled child-sections:\n");

    foreach my $sectionName (@{$self->grpItemsListRef()}) {
        my $sectionRef = $self->grpItemRef($sectionName);

        if (exists $sectionRef->{CHILD_SECTIONS}) {
            my $childSections = $self->getChildSections($sectionName);

            foreach my $childSectionName (@{$childSections->grpItemsListRef()}) {
                next if $childSections->isSectionEnabled($childSectionName);

                my $childSectionRealName = $childSections->getSectionName($childSectionName);

                $EMIT->printf("extern char __section_%s\_index;\n", $childSectionRealName);
            }
        }

        next if $self->isSectionEnabled($sectionName);

        my $sectionRealName = $self->getSectionName($sectionName);

        $EMIT->printf("extern char __section_%s\_index;\n", $sectionRealName);
    }
}

sub EMIT_ILWALID_SYMBOL_DECLS {
    my $self = $EMIT_OBJREF;

    $self->{DATA}->_printIlwalidIndexSymbolDecls();
    $self->{CODE}->_printIlwalidIndexSymbolDecls();
}

sub EMIT_DATA_SECTIONS {
    my $self = $EMIT_OBJREF;

    $self->{DATA_FIRST} = $self->{SECTION_COUNT};
    $self->{DATA_COUNT} = $self->{DATA}->_printSectionDefines($self->{DATA_FIRST}, 'data') - $self->{DATA_FIRST};
    $self->{SECTION_COUNT} += $self->{DATA_COUNT};

    return;
}

sub EMIT_CODE_SECTIONS {
    my $self = $EMIT_OBJREF;

    $self->{CODE_FIRST} = $self->{SECTION_COUNT};
    $self->{CODE_COUNT} = $self->{CODE}->_printSectionDefines($self->{CODE_FIRST}, 'code') - $self->{CODE_FIRST};
    $self->{SECTION_COUNT} += $self->{CODE_COUNT};

    return;
}

sub _emitInitialMpuEntries {
    (@_ == 2) or croak 'usage: _emitInitialMpuEntries( SECTION-REF, IS-ODP-ENABLED )';

    my ($Sections, $usingOdp) = @_;
    my $regionCount = 0;

    foreach my $sectionName (@{$Sections->grpItemsListRef()}) {
        next unless $Sections->isSectionEnabled($sectionName);
        next unless $Sections->isSectionInitiallyMapped($sectionName);
        my $sectionRealName = $Sections->getSectionName($sectionName);
        # Don't map ODP regions if ODP is enabled
        if ($usingOdp) {
            my $sectionRef = $Sections->grpItemRef($sectionName);
            my $location = uc $sectionRef->{LOCATION};
            next if ($location eq 'DMEM_ODP') || ($location eq 'IMEM_ODP');
        }

        $regionCount += 1;
        $EMIT->printf("    UPROC_SECTION_MPUCFG_REGION(%s)\n", $sectionRealName);
    }

    return $regionCount;
}

sub EMIT_INITIAL_MPU_CODE {
    my $self = $EMIT_OBJREF;

    $self->{MPU_COUNT} += _emitInitialMpuEntries($self->{CODE}, $EMIT_OBJREF->{ODP_ENABLED});

    return;
}

sub EMIT_INITIAL_MPU_DATA {
    my $self = $EMIT_OBJREF;

    $self->{MPU_COUNT} += _emitInitialMpuEntries($self->{DATA}, $EMIT_OBJREF->{ODP_ENABLED});

    return;
}

sub EMIT_FIELD {
    (@_ == 1 || @_ == 2) or croak 'usage: EMIT_FIELD( FIELD-NAME [ , FORMAT-STRING ] )';

    $EMIT->printf($_[1] ? $_[1] : "%d", $EMIT_OBJREF->{$_[0]});

    return;
}

sub printSectionsHeader
{
    (@_ == 4) or croak 'usage: Sections->printSectionsHeader( PROFILE-NAME, TEMPLATE-FILE, OUTPUT-FILE )';

    my ($class, $profile, $templateFile, $outputFile) = @_;

    $profile =~ /^(\w+)-(\w+)/;
    my ($opmode, $buildName) = ($1, $2);

    my $name = 'SECTIONS_HEADER';

    my $Profiles = Profiles->new($name, 'RISCV');
    my $profileRef = $Profiles->grpItemRef($buildName);

    my %obj = (
        CODE => Sections->new($name, 'code', 'RiscvSectionsCode.pm', $profileRef->{CODE_SECTION_PREFIX}),
        DATA => Sections->new($name, 'data', 'RiscvSectionsData.pm', $profileRef->{DATA_SECTION_PREFIX}),

        DATA_FIRST => 0,
        DATA_COUNT => 0,
        CODE_FIRST => 0,
        CODE_COUNT => 0,
        SECTION_COUNT => 0,
        MPU_COUNT => 0,
        ODP_ENABLED => $Profiles->odpEnabled($profileRef),
        NO_FB_SECTIONS => $profileRef->{NO_FB_SECTIONS}
    );
    $EMIT_OBJREF = \%obj;

    if (exists $profileRef->{DATA_SECTION_PREFIX}) {
        $obj{SHARED_DMEM_SECTION_NAME} = $profileRef->{DATA_SECTION_PREFIX} . 'taskSharedDataDmemRes';
    }
    else {
        $obj{SHARED_DMEM_SECTION_NAME} = 'taskSharedDataDmemRes';
    }

    my $templateObj;
    my @templateFilelines;

    $EMIT = GenFile->new('C', $outputFile,
                         { update_only_if_changed => 1, } );  # PARAMS-HASH-REF

    croak "Error: Could not create output file '$outputFile' : $!"     if ! $EMIT;

    # save the name for error msgs
    $EMIT->{LWRRENT_TEMPLATE_FILE} = $templateFile;

    #
    # Open the template file and read it line-by-line storing it into an array.
    #
    open(TEMPLATE_FILE, "<$templateFile") or croak
        "Error: could not open template file '$templateFile': $!";

    foreach (<TEMPLATE_FILE>)
    {
        s/\r\n/\n/;                # change CRLF to LF
        s/\r/\n/;                  # change lone CR to LF
        push @templateFilelines, $_;
    }

    close TEMPLATE_FILE or croak
        "Error: could not close template file '$templateFile': $!";

    #
    # Now create the template object that is responsible for processing the
    # template file contents and filling it in.
    #
    $templateObj =
        Text::Template->new
        (
            TYPE       => 'ARRAY',
            SOURCE     => \@templateFilelines,
            DELIMITERS => [ '{{', '}}', ],
        ) or croak
            "Error: Could not instantiate template object '$templateFile': $!";

    my $varsRef = {};

    # fold in $TEMPLATE_FILE, $PROFILE and $name
    $varsRef->{TEMPLATE_FILE} = basename $templateFile;
    $varsRef->{PROFILE}       = $profile;
    $varsRef->{name}          = 'rtos-flcn-script.pl';

    # Run the template
    $templateObj->fill_in
    (
        OUTPUT     => $EMIT->{FH},
        PACKAGE    => $class,
        HASH       => $varsRef,
    ) or croak "Error: failed to create template output file '$outputFile' : $Text::Template::ERROR";

    $EMIT->closefile();
    $EMIT = undef;
}

sub printSectionsSource
{
    (@_ == 3) or croak 'usage: obj->printSectionsSource( PROFILE-NAME, OUTPUT-FILE )';

    my ($self, $profile, $outputFile) = @_;

    $profile =~ /^(\w+)-(\w+)/;
    my ($opmode, $buildName) = ($1, $2);

    my $name = 'SECTIONS_SOURCE';

    my $Profiles = Profiles->new($name, 'RISCV');
    my $profileRef = $Profiles->grpItemRef($buildName);

    my $SectionsCode = Sections->new($name, 'code', 'RiscvSectionsCode.pm', $profileRef->{CODE_SECTION_PREFIX});
    my $SectionsData = Sections->new($name, 'data', 'RiscvSectionsData.pm', $profileRef->{DATA_SECTION_PREFIX});

    $EMIT = GenFile->new('C', $outputFile,
                         { update_only_if_changed => 1, } );  # PARAMS-HASH-REF

    croak "Error: Could not create output file '$outputFile' : $!"     if ! $EMIT;

    # MMINTS-TODO: the variable declarations below have to be resident, but they can be optimized for size!

    # Print start of include guard
    $EMIT->print(<<__CODE__);
//
// This file is automatically generated by 'rtos-flcn-script.pl' - DO NOT EDIT!
//
// Section source data
//

#include "g_sections_riscv.h"
#include "sections.h"

sysSHARED_DATA const LwUPtr DtcmRegionStart = (LwUPtr)__dmem_physical_base;
sysSHARED_DATA const LwUPtr ItcmRegionStart = (LwUPtr)__imem_physical_base;
sysSHARED_DATA const LwUPtr EmemRegionStart = (LwUPtr)__emem_physical_base;

sysSHARED_DATA const LwUPtr DtcmRegionSize  = (LwUPtr)__dmem_physical_size;
sysSHARED_DATA const LwUPtr ItcmRegionSize  = (LwUPtr)__imem_physical_size;
sysSHARED_DATA const LwUPtr EmemRegionSize  = (LwUPtr)__emem_physical_size;

__CODE__
    $SectionsData->_printIlwalidIndexSymbolDefs();
    $SectionsCode->_printIlwalidIndexSymbolDefs();
    $EMIT->printf("\n\n");

    $EMIT->printf("sysSHARED_DATA LwUPtr SectionDescStartPhysical[UPROC_SECTION_COUNT] = {\n");
    $SectionsData->_printSectionSource('UPROC_SECTION_PHYS_OFFSET');
    $SectionsCode->_printSectionSource('UPROC_SECTION_PHYS_OFFSET');
    $EMIT->printf("};\n\n");

    $EMIT->printf("#ifdef ELF_IN_PLACE_FULL_ODP_COW\n");
    $EMIT->printf("sysSHARED_DATA LwUPtr SectionDescStartDataInElf[UPROC_DATA_SECTION_COUNT] = {\n");
    $SectionsData->_printSectionSource('UPROC_SECTION_ELF_OFFSET');
    $EMIT->printf("};\n");
    $EMIT->printf("#endif // ELF_IN_PLACE_FULL_ODP_COW\n\n");

    $EMIT->printf("sysSHARED_DATA LwUPtr SectionDescStartVirtual[UPROC_SECTION_COUNT] = {\n");
    $SectionsData->_printSectionSource('UPROC_SECTION_VIRT_OFFSET');
    $SectionsCode->_printSectionSource('UPROC_SECTION_VIRT_OFFSET');
    $EMIT->printf("};\n\n");

    $EMIT->printf("sysSHARED_DATA LwLength SectionDescHeapSize[UPROC_DATA_SECTION_COUNT] = {\n");
    $SectionsData->_printSectionSource('UPROC_SECTION_INITIAL_SIZE');
    $EMIT->printf("};\n\n");

    $EMIT->printf("sysSHARED_DATA LwLength SectionDescMaxSize[UPROC_SECTION_COUNT] = {\n");
    $SectionsData->_printSectionSource('UPROC_SECTION_MAX_SIZE');
    $SectionsCode->_printSectionSource('UPROC_SECTION_MAX_SIZE');
    $EMIT->printf("};\n\n");

    $EMIT->printf("sysSHARED_DATA LwU8 SectionDescLocation[UPROC_SECTION_COUNT] = {\n");
    $SectionsData->_printSectionSource('UPROC_SECTION_LOCATION');
    $SectionsCode->_printSectionSource('UPROC_SECTION_LOCATION');
    $EMIT->printf("};\n\n");

    $EMIT->printf("sysSHARED_DATA LwU32 SectionDescPermission[UPROC_SECTION_COUNT] = {\n");
    $SectionsData->_printSectionSource('UPROC_SECTION_PERMISSION');
    $SectionsCode->_printSectionSource('UPROC_SECTION_PERMISSION');
    $EMIT->printf("};\n\n");

    $EMIT->closefile();
    $EMIT = undef;
}

# return ref of section item for the matching section name
# the section name format can be with or without the prefix '.':
#   '.imem_init' and 'imem_init' are both token
sub getSectionRefFromName
{
    (@_ == 2) or croak 'usage: obj->getSectionRefFromName( SECTION-NAME )';

    my ($self, $sectionName) = @_;

    # remove the prefix '.' if any
    $sectionName =~ s/^\.//;

    foreach my $sectionName (@{$self->grpItemsListRef()}) {
        my $sectionRef = $self->grpItemRef($sectionName);

        return $sectionRef if ($sectionName eq $self->getSectionName($sectionName));
    }
    return;
}

# Return true when the section is enabled, or false otherwise
sub isSectionEnabled
{
    (@_ == 2) or croak 'usage: obj->isSectionEnabled( SECTION-NAME )';

    my ($self, $sectionName) = @_;

    my $sectionRef = $self->grpItemRef($sectionName);

    my $profilesRef = $sectionRef->{PROFILES};

    my $ProfileUtil = $UPROC->{PROFILE_UTIL};

    printf("Section $sectionName:\n") if ($UPROC->{opt}->{verbose} >= 1);

    my $bEnabled = $ProfileUtil->isTargetProfileInPatternList($profilesRef);

    if ($UPROC->{opt}->{verbose} >= 1) {
        printf("    %s by profile list : [ %s ] \n".
            "    ... %s\n\n",
            $bEnabled? 'ENABLED ' : 'DISABLED',
            (join ', ', @$profilesRef),
            $ProfileUtil->getPatternListCommentStr($profilesRef) );
    }

    return $bEnabled;
}

# Return true when the section is mapped on boot, or false otherwise
sub isSectionInitiallyMapped
{
    (@_ == 2) or croak 'usage: obj->isSectionInitiallyMapped( SECTION-NAME )';

    my ($self, $sectionName) = @_;

    my $sectionRef = $self->grpItemRef($sectionName);

    if (!defined $sectionRef->{MAP_AT_INIT}) {
        my $location = uc $sectionRef->{LOCATION};
        if (($location eq 'DMEM_RES') || ($location eq 'IMEM_RES')) {
            # Sections in resident memory are mapped-at-init by default
            return 1;
        } else {
            return 0;
        }
    }
    else {
        return $sectionRef->{MAP_AT_INIT};
    }
}

# Return the name for the ELF output-section that will be constructed for
# the section.
sub getSectionName
{
    (@_ == 2) or croak 'usage: obj->getSectionName( SECTION-NAME )';

    my $self = shift;
    my $sectionName = shift;

    my $sectionRef = $self->grpItemRef($sectionName);

    return unless $sectionRef;

    return $sectionRef->{SECTION_NAME} if $sectionRef->{SECTION_NAME};

    # If a name was specified in the .pm file, use it;
    # otherwise create a name from the section's item-name.
    if (!$sectionRef->{NAME}) {
        $sectionRef->{NAME} = lcfirst(utilTagToCamelCase($sectionName));
    }

    $sectionRef->{SECTION_NAME} = $self->{SECTION_NAME_PREFIX} . $sectionRef->{NAME};

    return $sectionRef->{SECTION_NAME};
}

# Return the Sections object for the child sections of this section,
# or undef if there aren't any.
sub getChildSections
{
    (@_ == 2) or croak 'usage: obj->getChildSections( SECTION-NAME )';

    my $self = shift;
    my $sectionName = shift;

    my $sectionRef = $self->grpItemRef($sectionName);

    return unless $sectionRef;

    if (exists $sectionRef->{CHILD_SECTIONS}) {
        return $self->{CHILD_SECTIONS_MAP}->{$sectionName};
    }
    else {
        return undef;
    }
}

sub readSectionsConfigFile
{
    (@_ == 2) or croak 'usage: readSectionsConfigFile( NAME, FILE-NAME )';

    my ($name, $fileName) = @_;

    my $dataRaw = utilReadConfigFile($name, $fileName);

    # check and expand any '__INCLUDE__'
    my $dataExpanded;
    while (my $item = shift @$dataRaw) {
        if ($item eq '__INCLUDE__') {
            my $includeFileName = shift @$dataRaw;

            my $dataIncluded = readSectionsConfigFile($name, $includeFileName);
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
        DESCRIPTION     => ['CODE', 'DATA'],
        NAME            => ['CODE', 'DATA'],
        INPUT_SECTIONS  => ['CODE', 'DATA'],
        CHILD_SECTIONS  => ['CODE'],
        LOCATION        => ['CODE', 'DATA'],
        PERMISSIONS     => ['CODE', 'DATA'],
        MAP_AT_INIT     => ['CODE', 'DATA'],
        ALIGNMENT       => ['CODE', 'DATA'],
        HEAP_SIZE       => ['DATA'],
        PROFILES        => ['CODE', 'DATA'],
        FLAGS           => ['CODE', 'DATA'],
        PREFIX          => ['CODE', 'DATA'],
        SUFFIX          => ['CODE', 'DATA'],
        NO_LOAD         => ['DATA'],
    );

    my %requiredFieldH = (
        PROFILES        => ['CODE', 'DATA'],
        LOCATION        => ['CODE', 'DATA'],
        PERMISSIONS     => ['CODE', 'DATA'],
    );

    my %defaultFieldH = (
        INPUT_SECTIONS  => [],
        ALIGNMENT       => 0,
    );
    my %defaultCodeFieldH = (
    );
    my %defaultDataFieldH = (
        HEAP_SIZE       => 0,
    );

    if ($self->{MEM_TYPE} eq 'CODE') {
        foreach ( keys %defaultCodeFieldH ) {
            if (!exists $defaultFieldH{$_}) {
                $defaultFieldH{$_} = $defaultCodeFieldH{$_};
            }
        }
    }
    elsif ($self->{MEM_TYPE} eq 'DATA') {
        foreach ( keys %defaultDataFieldH ) {
            if (!exists $defaultFieldH{$_}) {
                $defaultFieldH{$_} = $defaultDataFieldH{$_};
            }
        }
    }

    foreach my $sectionName (@{$self->grpItemsListRef()}) {
        my $sectionRef = $self->grpItemRef($sectionName);

        # check if any invalid fields
        foreach my $field (keys %$sectionRef) {
            next if grep { $_ eq $self->{MEM_TYPE} } @{$validFieldH{$field}};

            utilError "$sectionName: Invalid field '$field'";
        }

        # check for required field
        foreach my $field (keys %requiredFieldH) {
            next if grep { $_ eq $self->{MEM_TYPE} } @{$requiredFieldH{$field}};

            utilError "$sectionName: Missing field '$field'" unless exists $sectionRef->{$field};
        }

        # merge default fields
        foreach ( keys %defaultFieldH ) {
            if (!exists $sectionRef->{$_}) {
                $sectionRef->{$_} = $defaultFieldH{$_};
            }
        }

        # debug
        if ($UPROC->{opt}->{verbose} >= 2) {
            foreach my $field (keys %$sectionRef) {
                print "[$self->{MEM_TYPE}] $sectionName: '$field' = '$sectionRef->{$field}'\n";
            }
        }
    }
}

sub initializeHalFields
{
    (@_ == 1) or croak 'usage: obj->initializeHalFields()';

    my $self = shift;

    # Loop though entries
    foreach my $sectionName (@{$self->grpItemsListRef()}) {
        my $sectionRef = $self->grpItemRef($sectionName);

        $sectionRef->{LOCATION}     = UtilsUproc::parseHalParamBlock("$sectionName: LOCATION",
                                                                     $sectionRef->{LOCATION});
        $sectionRef->{HEAP_SIZE}    = UtilsUproc::parseHalParamBlock("$sectionName: HEAP_SIZE",
                                                                     $sectionRef->{HEAP_SIZE});
        $sectionRef->{PERMISSIONS}  = UtilsUproc::parseHalParamBlock("$sectionName: PERMISSIONS",
                                                                     $sectionRef->{PERMISSIONS});
        $sectionRef->{ALIGNMENT}    = UtilsUproc::parseHalParamBlock("$sectionName: ALIGNMENT",
                                                                     $sectionRef->{ALIGNMENT});
        $sectionRef->{MAP_AT_INIT}  = UtilsUproc::parseHalParamBlock("$sectionName: MAP_AT_INIT",
                                                                     $sectionRef->{MAP_AT_INIT});
    }
}

sub populateChildSections
{
    (@_ == 1) or croak 'usage: obj->initializeHalFields()';

    my $self = shift;

    # Loop though entries
    foreach my $sectionName (@{$self->grpItemsListRef()}) {
        my $sectionRef = $self->grpItemRef($sectionName);

        if (exists $sectionRef->{CHILD_SECTIONS}) {
            $self->{CHILD_SECTIONS_MAP}->{$sectionName} = Sections->new($self->{NAME},
                                                                    $self->{MEM_TYPE},
                                                                    $sectionRef->{CHILD_SECTIONS},
                                                                    $self->{SECTION_NAME_PREFIX});
        }
    }
}

sub new
{
    (@_ == 4 || @_ == 5) or croak 'usage: obj->new( NAME, TYPE, [ FILE-NAME | RAW-SECTIONS-DATA ] [ , SECTION-NAME-PREFIX ] )';

    my ($class, $name, $mem_type, $sectionsData, $sectionNamePrefix) = @_;

    my $sectionsRaw;

    if (ref($sectionsData) eq 'ARRAY') {
        $sectionsRaw = $sectionsData;
    }
    else {
        my $fileName = $sectionsData;
        $sectionsRaw = readSectionsConfigFile($name, $fileName);
    }

    my $self = Groups->new($name, $sectionsRaw);

    $self->{MEM_TYPE} = uc $mem_type;

    $self->{SECTION_NAME_PREFIX} = "";

    if ($sectionNamePrefix) {
        $self->{SECTION_NAME_PREFIX} = $sectionNamePrefix;
    }

    bless $self, $class;

    $self->validateFields();
    $self->initializeHalFields();

    $self->{CHILD_SECTIONS_MAP} = {};
    $self->populateChildSections();

    return $self;
}

1;
