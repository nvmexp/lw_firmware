#!/usr/bin/perl -w

#
# Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# This module is responsible for sourcing a project's Sections scripts and
# generating large portions of the project's linker-script.
#

package GenLdscriptRiscv;

use strict;
no strict qw(vars);
use warnings 'all';

use impl::ProfilesImpl;                # Implementations for Profiles
use impl::SectionsImpl;                # Implementations for Sections

use chipGroups;                        # imports CHIPS from Chips.pm and chip functions

use Carp;
use Utils;
use GenFile;
use IO::Handle;                        # for ->flush() method
use IO::File;                          # more methods

my $EMIT = undef;                      # ref to GenFile to emit g_xxx from template
my $EMIT_OBJREF = undef;               # object ref (if any) for emitted code


# global variable to fit old coding style
my $SectionsCode;
my $SectionsData;
my $profileRef;
my $Profiles;

################################################################################

#
# Emit output file using template.
#
sub emitFileFromTemplate
{
    (@_ == 3) or croak 'usage: obj->emitFileFromTemplate( TEMPLATE-FILE, OUTPUT-FILE )';

    my $self           = shift;
    my $templateFile   = shift;
    my $outputFileName = shift;

    my $templateObj;
    my @templateFilelines;

    $EMIT_OBJREF = $self;

    $EMIT = GenFile->new('C', $outputFileName,
                         { update_only_if_changed => 1, } );  # PARAMS-HASH-REF

    croak "Error: Could not create output file '$outputFileName' : $!"     if ! $EMIT;

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

    # Run the template
    $templateObj->fill_in
    (
        OUTPUT     => $EMIT->{FH},
    ) or croak "Error: failed to create template output file '$outputFileName' : $Text::Template::ERROR";

    $EMIT->closefile();

    $EMIT = undef;
}

################################################################################

sub EMIT_ENTRYPOINT
{
    $EMIT->printf("ENTRY(_start)\n");

    return;
}

sub EMIT_HIDDEN_PA_POINTERS
{
    my $self = $EMIT_OBJREF;

    $EMIT->printf("HIDDEN(__dmem_carveout_ptr  = ORIGIN(dmem_carveout));\n");
    $EMIT->printf("HIDDEN(__emem_ptr           = ORIGIN(emem));\n");
    $EMIT->printf("HIDDEN(__dmem_ptr           = ORIGIN(dmem));\n");
    $EMIT->printf("HIDDEN(__imem_ptr           = ORIGIN(imem));\n");
    $EMIT->printf("HIDDEN(__fbgpa_ptr          = ORIGIN(fbgpa));\n");
    $EMIT->printf("HIDDEN(__elfpa_ptr          = ORIGIN(elfpa));\n");
    $EMIT->printf("HIDDEN(__elf_odp_cow_pa_ptr = ORIGIN(elf_odp_cow_pa));\n");

    return;
}

sub EMIT_RISCV_SECTION
{
    my ($name, $memSection, $alignment, $permission, $heapSize, $entryListRef, $sectionNonLoadable) = @_;
    my $self = $EMIT_OBJREF;

    # We allow section names with . in them, but we can't have variable names with . in them
    my $nameOrig = $name;
    $name =~ s/\./_/g;

    my $mpuAlignSize = $profileRef->{MPU_GRANULARITY};
    if (! defined $mpuAlignSize) {
        $EMIT->error("MPU_GRANULARITY not set for this profile!");
    }

    if (! $alignment) {
        $alignment = $mpuAlignSize;
    }

    my $physAlignment = $alignment;
    my $virtAlignment = $alignment;

    # If ODP is disabled, error out on paged sections
    if (! $Profiles->odpEnabled($profileRef)) {
        if (($memSection eq 'dmem_odp') || ($memSection eq 'imem_odp')) {
            $EMIT->error("Found paged section '$name', but ODP is not enabled!");
        }
    }
    if ($memSection eq 'dmem_odp') {
        if (! defined $profileRef->{ODP_DATA_PAGE_SIZE}) {
            $EMIT->error("Using data ODP page on section '$name', but no ODP_DATA_PAGE_SIZE defined!");
        }
    }
    elsif ($memSection eq 'imem_odp') {
        if (! defined $profileRef->{ODP_CODE_PAGE_SIZE}) {
            $EMIT->error("Using code ODP page on section '$name', but no ODP_CODE_PAGE_SIZE defined!");
        }
    }

    # If creating a pre-extracted image, error out on non-resident sections.
    if (($profileRef->{PRE_EXTRACTED}) && (! $sectionNonLoadable)) {
        if (($memSection ne 'imem_res') && ($memSection ne 'dmem_res')) {
            $EMIT->error("Found non-resident loadable section '$name' while trying to create pre-extracted image!");
        }
    }

    my %physLocationHash = (
        'dmem_odp'  => 'fbgpa', # Pages backed by FB
        'imem_odp'  => 'fbgpa', # Pages backed by FB
        'dmem_res'  => 'dmem',
        'imem_res'  => 'imem',
        'emem'      => 'emem',
        'fb'        => 'fbgpa',
    );

    my $physLocation = $physLocationHash{$memSection};
    if (! defined $physLocation) {
        $EMIT->error("$name location '$memSection' invalid");
    }

    # NOTE: Try to keep these the same as gt_sections_riscv.h
    my %locationHash = (
        'dmem_odp'  => 0,
        'imem_odp'  => 1,
        'dmem_res'  => 2,
        'imem_res'  => 3,
        'emem'      => 4,
        'fb'        => 5,
    );
    my $locationIndex = $locationHash{$memSection};
    if (! defined $locationIndex) {
        $EMIT->error("$name location '$memSection' invalid");
    }

    if ($memSection eq 'dmem_odp') {
        if ($profileRef->{ODP_DATA_PAGE_SIZE} > $virtAlignment) {
            $virtAlignment = $profileRef->{ODP_DATA_PAGE_SIZE};
        }
    }
    elsif ($memSection eq 'imem_odp') {
        if ($profileRef->{ODP_CODE_PAGE_SIZE} > $virtAlignment) {
            $virtAlignment = $profileRef->{ODP_CODE_PAGE_SIZE};
        }
    }

    my $maxAlignment = $virtAlignment;
    if ($maxAlignment < $physAlignment) {
        $maxAlignment = $physAlignment;
    }

    my %permissionHash = (
        'r'  => 1,
        'w'  => 2,
        'x'  => 4,
        'R'  => 8,
        'W'  => 16,
        'X'  => 32,
    );

    my $permissiolwalue = 0;
    foreach my $ch (split //, $permission) {
        if (! defined $permissionHash{$ch}) {
            $EMIT->error("$name permission character '$ch' invalid");
        }
        $permissiolwalue |= $permissionHash{$ch};
    }

    my $physPseudoLocation = $physLocation;

    # Run-in-place sections are relative to start of ELF file
    if ($self->{ELF_IN_PLACE}) {
        if (isElfPaSection($memSection, $permissiolwalue, \%permissionHash)) {
            $physLocation = 'elfpa';
            $physPseudoLocation = 'elfpa';

            if ($memSection eq 'fb') {
                $locationIndex = 6; # location 6 for UPROC_SECTION_LOCATION_ELF
            }
        } elsif ($self->{ELF_IN_PLACE_ODP_COW} && ($memSection eq 'dmem_odp')) {
            $physLocation = 'fbgpa';
            $physPseudoLocation = 'elf_odp_cow_pa';
        }
    }

    # We can't generate a linker script from bad data, so exit if errors were detected
    utilExitIfErrors();

    $EMIT->printf("    __%s\_ptr = ALIGN(__%s\_ptr, 0x%X);\n", $physLocation, $physLocation, $physAlignment);

    # This is needed because we have to reserve space for the DMEM ODP sections in the "normal" FB,
    # because they will be "moved" there from the ELF on first flush.
    # So, the phys addr is remapped to the "fake" pseudo-location base.
    $EMIT->printf("    __section_%s\_physaddr = (__%s\_ptr - __%s\_physical_base) + __%s\_physical_base;\n",
                    $name, $physLocation, $physLocation, $physPseudoLocation);

    $EMIT->printf("    __section_%s\_location = %d;\n", $name, $locationIndex);
    $EMIT->printf("    __section_%s\_permission = %d;\n", $name, $permissiolwalue);
    $EMIT->printf("    .%s ALIGN(0x%X) %s : AT(__section_%s\_physaddr) {\n", $nameOrig, $virtAlignment,
                  ($sectionNonLoadable ? "(COPY)" : ""), $name);
    $EMIT->printf("        __section_%s\_data_start = .;\n", $name);
    foreach my $entry (@$entryListRef) {
        $EMIT->printf("        %s\n", $entry);
    }
    $EMIT->printf("        . = ALIGN(8);\n");
    $EMIT->printf("        __section_%s\_heap_start = .;\n", $name);
    if ($heapSize) {
        if (($heapSize . '') =~ /\D/) { # has non-digits, print as-is
            $EMIT->printf("        . += %s;\n", $heapSize);
        }
        else { # only digits, colwert to hex
            $EMIT->printf("        . += 0x%X;\n", $heapSize);
        }
    }
    $EMIT->printf("        . = ALIGN(0x%X);\n", $maxAlignment);
    $EMIT->printf("    }\n");
    $EMIT->printf("    __%s\_ptr += SIZEOF(.%s);\n", $physLocation, $nameOrig);
    $self->{ASSERT_STR} .= sprintf("ASSERT (".
        "((__section_%s\_physaddr + __section_%s\_max_size) <= (__%s\_physical_base + __%s\_physical_size)), ".
        "\"ERROR: Section %s overflows physical storage %s\");\n",
        $name, $name, $physPseudoLocation, $physPseudoLocation, $nameOrig, $physPseudoLocation);
    $EMIT->printf("    __section_%s\_start = ADDR(.%s);\n", $name, $nameOrig);
    $EMIT->printf("    __section_%s\_max_size = SIZEOF(.%s);\n", $name, $nameOrig);
    $EMIT->printf("    __section_%s\_initial_size = __section_%s\_heap_start - __section_%s\_data_start;\n",
                  $name, $name, $name);
    $EMIT->printf("    __section_%s\_heap_size = __section_%s\_max_size - __section_%s\_initial_size;\n",
                  $name, $name, $name);

    return;
}

sub EMIT_RISCV_VMA_HOLE
{
    my ($name) = @_;

    my $mpuAlignSize = $profileRef->{MPU_GRANULARITY};
    if (! defined $mpuAlignSize) {
        $EMIT->error("MPU_GRANULARITY not set for this profile!");
    }
    my $vmaHoleSize = $profileRef->{VMA_HOLE_SIZE};
    if (! defined $vmaHoleSize) {
        $vmaHoleSize = 0;
    }

    if ($vmaHoleSize != 0) {
        $EMIT->printf("    /* Memory corruption catching virtual memory hole for .%s */\n", $name);
        $EMIT->printf("    . += 0x%X;\n", $vmaHoleSize);
        $EMIT->printf("    . = ALIGN(0x%X);\n", $mpuAlignSize);
    }

    return;
}

sub _riscvSectionListGet
{
    my ($Sections, $sectionName, $skipSuffix) = @_;
    my $self = $EMIT_OBJREF;

    my $sectionRef = $Sections->grpItemRef($sectionName);
    my $sectionRealName = $Sections->getSectionName($sectionName);

    my @inputSections = ();
    if (exists $sectionRef->{PREFIX}) {
        push(@inputSections, $sectionRef->{PREFIX} . ';');
    }

    push(@inputSections, "*(.$sectionRealName .$sectionRealName.*);");
    foreach my $sec (@{$sectionRef->{INPUT_SECTIONS}}) {
        my $newSection;
        if (ref($sec) eq 'ARRAY') {
            # Normal section description
            $newSection = @$sec[0];
            # If the source was a library, but no files were indicated, imply all.
            if($newSection =~ /\.a$/) {
                $newSection .= ':*';
            }
            $newSection .= "(@$sec[1]);";
        }
        else {
            # Assume it's a raw string to add
            $newSection = $sec . ";";
        }
        push(@inputSections, $newSection);

        if (($newSection =~ /\.bss/) && (! $profileRef->{PRE_EXTRACTED})) {
            #
            # Our bootloader skips zero-padding sections in pre-silicon,
            # so just in case make sure that we don't end up with a situation
            # where the ELF would *actually* require zero-padding.
            #
            # We can skip doing this for pre-extracted images as any necessary
            # zero-padding is done at build time.
            #
            # Note that, as a side-effect of this precaution, empty .bss
            # sections will always consume at least one MPU page in the output
            # file, but this should not affect the final memory footprint of
            # the loaded application.
            #
            push @inputSections, 'LONG(0xfeedfeed);';
        }
    }
    if (exists $sectionRef->{SUFFIX} && !$skipSuffix) {
        push(@inputSections, $sectionRef->{SUFFIX} . ';');
    }

    return @inputSections;
}

sub _emitRiscvSections {
    my ($sections) = @_;

    my $self = $EMIT_OBJREF;

    foreach my $sectionName (@{$sections->grpItemsListRef()}) {
        next unless $sections->isSectionEnabled($sectionName);

        my $sectionRef = $sections->grpItemRef($sectionName);
        my $sectionRealName = $sections->getSectionName($sectionName);
        my $sectionAlignment = $sectionRef->{ALIGNMENT};
        my $sectionAlignmentRem = 0;

        my $mpuAlignSize = $profileRef->{MPU_GRANULARITY};
        if (! defined $mpuAlignSize) {
            $EMIT->error("MPU_GRANULARITY not set for this profile!");
        }

        if ($sectionAlignment < $mpuAlignSize) {
            # Minimum size is MPU_GRANULARITY
            $sectionAlignment = $mpuAlignSize;
        } elsif (($sectionAlignmentRem = ($sectionAlignment % $mpuAlignSize)) != 0) {
            # Align up to MPU_GRANULARITY
            $sectionAlignment += ($mpuAlignSize - $sectionAlignmentRem);
        }

        my @inputSections;

        if (exists $sectionRef->{CHILD_SECTIONS}) {
            # Delay adding suffix until after we've added all child sections
            @inputSections = _riscvSectionListGet($sections, $sectionName, 1);
            $inputSections[-1] .= "\n"; # break after first chunk of input sections

            my $childSections = $sections->getChildSections($sectionName);

            foreach my $childSectionName (@{$childSections->grpItemsListRef()}) {
                next unless $childSections->isSectionEnabled($childSectionName);

                my $childSectionRealName = $childSections->getSectionName($childSectionName);

                push @inputSections, sprintf("__section_%s\_data_start = .;", $childSectionRealName);
                push @inputSections, _riscvSectionListGet($childSections, $childSectionName);
                push @inputSections, sprintf("__section_%s\_data_end = .;\n", $childSectionRealName);
            }

            if (exists $sectionRef->{SUFFIX}) {
                push(@inputSections, $sectionRef->{SUFFIX} . ';');
            }
        }
        else {
            @inputSections = _riscvSectionListGet($sections, $sectionName);
        }

        $EMIT->printf("    /******** $sectionName ********/\n");
        EMIT_RISCV_SECTION($sectionRealName, $sectionRef->{LOCATION}, $sectionAlignment,
                        $sectionRef->{PERMISSIONS}, $sectionRef->{HEAP_SIZE}, \@inputSections,
                        $sectionRef->{NO_LOAD});

        $EMIT->printf("    __section_%s\_index = %d;\n\n",
                    $sectionRealName, $self->{SECTION_IDX});
        $self->{SECTION_IDX} += 1;

        if (exists $sectionRef->{CHILD_SECTIONS}) {
            my $childSections = $sections->getChildSections($sectionName);

            foreach my $childSectionName (@{$childSections->grpItemsListRef()}) {
                next unless $childSections->isSectionEnabled($childSectionName);

                my $childSectionRealName = $childSections->getSectionName($childSectionName);

                $EMIT->printf("    __section_%s\_physaddr = __section_%s\_physaddr + " .
                                "(__section_%s\_data_start - __section_%s\_data_start);\n",
                                $childSectionRealName, $sectionRealName,
                                $childSectionRealName, $sectionRealName);
                $EMIT->printf("    __section_%s\_start = __section_%s\_start + " .
                                "(__section_%s\_data_start - __section_%s\_data_start);\n",
                                $childSectionRealName, $sectionRealName,
                                $childSectionRealName, $sectionRealName);
                $EMIT->printf("    __section_%s\_max_size = __section_%s\_data_end - " .
                                "__section_%s\_data_start;\n",
                                $childSectionRealName, $childSectionRealName,
                                $childSectionRealName);
                $EMIT->printf("    __section_%s\_index = __section_%s\_index;\n",
                                $childSectionRealName, $sectionRealName);
                $EMIT->printf("    __section_%s\_location = __section_%s\_location;\n",
                                $childSectionRealName, $sectionRealName);
                $EMIT->printf("    __section_%s\_permission = __section_%s\_permission;\n\n",
                                $childSectionRealName, $sectionRealName);
            }
        }

        $EMIT->printf("\n");
        EMIT_RISCV_VMA_HOLE($sectionRealName);
        $EMIT->printf("\n\n");
    }

    return;
}

sub EMIT_RISCV_CODE_SECTIONS
{
    _emitRiscvSections($SectionsCode);
}

sub EMIT_RISCV_DATA_SECTIONS
{
    _emitRiscvSections($SectionsData);
}

sub EMIT_SYSTEM_STACK
{
    my ($memSection) = @_;

    $EMIT->printf("    /* Kernel/Trap stack */\n");
    EMIT_RISCV_SECTION('sysStack', $memSection, 0, 'RW', 0, [
        sprintf(". += 0x%X;\n", $Profiles->riscvSysStackSize($profileRef)),
    ]);
    EMIT_RISCV_VMA_HOLE('sysStack');

    return;
}

sub EMIT_DMEM_END_CARVEOUT_SYMBOLS
{
    my $self = $EMIT_OBJREF;
    
    #
    # MMINTS-TODO: switch the dmesg buffer MPU mapping to
    # be a mapping for the whole carveout and use the dmem_end_carveout
    # symbols.
    #
    $EMIT->printf("    /* DMEM-end carveout */\n");
    $EMIT->printf("    __dmem_end_carveout_physaddr = ORIGIN(dmem_carveout);\n");
    $EMIT->printf("    _dmem_end_carveout_start     = .;\n");
    $EMIT->printf("    _dmem_end_carveout_size      = LENGTH(dmem_carveout);\n");
    $EMIT->printf("    . += _dmem_end_carveout_size;\n");
    $EMIT->printf("    _dmem_carveout_end       = .;\n");

    if ($self->{DMESG_BUFFER_SIZE} > $self->{DMEM_END_CARVEOUT_SIZE}) {
        $EMIT->error("Dmesg buffer overflows DMEM end carveout! Buffer size: 0x%X, Carveout size: 0x%X",
                     $self->{DMESG_BUFFER_SIZE}, $self->{DMEM_END_CARVEOUT_SIZE});
    }

    utilExitIfErrors();

    $EMIT->printf("    _dmesg_buffer_size      = 0x%X;\n", $self->{DMESG_BUFFER_SIZE});
    $EMIT->printf("    _dmesg_buffer_end       = _dmem_carveout_end;\n");
    $EMIT->printf("    _dmesg_buffer_start     = _dmesg_buffer_end - _dmesg_buffer_size;\n");
    $EMIT->printf("    __dmesg_buffer_physaddr = __dmem_end_carveout_physaddr + _dmem_end_carveout_size - _dmesg_buffer_size;\n");

    EMIT_RISCV_VMA_HOLE('dmesgBuffer');

    return;
}

sub EMIT_ODP_SYMBOLS
{
    if (! $Profiles->odpEnabled($profileRef)) {
        return;
    }

    $EMIT->printf("    . = __imem_ptr;\n");
    $EMIT->printf("    __res_imem_end = ALIGN(0x%X);\n", $profileRef->{MPU_GRANULARITY});
    $EMIT->printf("    __odp_imem_base = ALIGN(0x%X);\n", $profileRef->{ODP_CODE_PAGE_SIZE}); # align to page size for better TLB usage
    $EMIT->printf("    __odp_imem_region_size = (__imem_physical_base + __imem_physical_size - __res_imem_end);\n");
    $EMIT->printf("    __odp_imem_page_size = 0x%X;\n", $profileRef->{ODP_CODE_PAGE_SIZE});
    $EMIT->printf("    __odp_imem_max_num_pages = (__imem_physical_base + __imem_physical_size - __odp_imem_base) / __odp_imem_page_size;\n");
    $EMIT->printf("    __odp_imem_max_config_num_pages = 0x%X;\n", $profileRef->{ODP_CODE_PAGE_MAX_COUNT});

    $EMIT->printf("    . = __dmem_ptr;\n");
    $EMIT->printf("    __res_dmem_end = ALIGN(0x%X);\n", $profileRef->{MPU_GRANULARITY});
    $EMIT->printf("    __odp_dmem_base = ALIGN(0x%X);\n", $profileRef->{ODP_DATA_PAGE_SIZE}); # align to page size for better TLB usage
    $EMIT->printf("    __odp_dmem_region_size = (__dmem_physical_base + __dmem_physical_size - __res_dmem_end);\n");
    $EMIT->printf("    __odp_dmem_page_size = 0x%X;\n", $profileRef->{ODP_DATA_PAGE_SIZE});
    $EMIT->printf("    __odp_dmem_max_num_pages = (__dmem_physical_base + __dmem_physical_size - __odp_dmem_base) / __odp_dmem_page_size;\n");
    $EMIT->printf("    __odp_dmem_max_config_num_pages = 0x%X;\n", $profileRef->{ODP_DATA_PAGE_MAX_COUNT});

    if ($profileRef->{ODP_MPU_SHARED_COUNT}) {
        $EMIT->printf("    __odp_config_mpu_shared_count = 0x%X;\n", $profileRef->{ODP_MPU_SHARED_COUNT});
    }

    return;
}

# Note: this should NOT be ilwoked in parts of the ld template
# where virtual addresses are handled, since this mangles .
sub EMIT_IDENTITY_SECTIONS
{
    my $self = $EMIT_OBJREF;

    if ($self->{IDENTITY_MAPPING}) {

        $EMIT->printf("    . = __imem_ptr;\n");
        EMIT_RISCV_SECTION('imem_identity', 'imem_res', 0, 'RX', 0, [
            'KEEP(*(.imem_identity .imem_identity.*))',
        ]);
        $EMIT->printf("    __section_%s\_index = %d;\n\n",
                        'imem_identity', -1); # hack to help riscv-stats-gen.pl
        $EMIT->printf("    . = __dmem_ptr;");
        EMIT_RISCV_SECTION('dmem_identity', 'dmem_res', 0, 'RW', 0, [
            'KEEP(*(.dmem_identity .dmem_identity.*))',
        ]);
        $EMIT->printf("    __section_%s\_index = %d;\n\n",
                        'dmem_identity', -1); # hack to help riscv-stats-gen.pl
    }

    return;
}

# Note: this should NOT be ilwoked in parts of the ld template
# where virtual addresses are handled, since this mangles .
sub EMIT_LOGGING_SECTION
{
    # MMINTS-TODO: do not emit this when raw-mode prints are disabled in makefile?
    my $self = $EMIT_OBJREF;

    # This helps save space. They are not "true" virtual addrs so this is not an issue.
    $EMIT->printf("    . = 0x0;\n");

    # Use non-zero heap to avoid crashes when doing objdump if section is empty
    EMIT_RISCV_SECTION('logging_metadata', 'fb', 0, '', 0x400, [
        # logging_metadata must go first to ensure that simple
        # logging section changes don't affect the metadata offsets in the actual objdump.
        # The metadata offsets will only change when a new print is added.
        '*(.logging_metadata .logging_metadata.*);',
    ], 1);

    EMIT_RISCV_SECTION('logging', 'fb', 0, '', 0x400, [
        # .logging is the data that gets referenced in code directly by address (const strs),
        # so it should follow the metadata; however, to account for size-neutral changes
        # to such const strs and to edge-cases like last-string-in-the-list changes
        # (neither of which could trigger changes in the main image), we add this to the
        # logging output-section, which gets dumped and checked when deciding the release
        # action (i.e. normal release vs "only logging metadata changed" release).
        #
        # MMINTS-TODO: this is sort of hacky. We might consider using double-pointers for str
        # constants to avoid changes to the objdump when a non-format string constant is changed.
        # That would need to be coordinated with GSP-RM.
        '*(.logging .logging.*);',

        # The print format strings, translation unit basenames and line number copies go here.
        # The _const section is data that is not referenced directly in the objdump, but
        # is still expected to change more or less frequently.
        '*(.logging_const .logging_const.*);',
    ], 1);

    # Below is a hack to help riscv-stats-gen.pl: the logging sections need indices
    $EMIT->printf("    __section_%s\_index = %d;\n\n",
                    'logging_metadata', -1);
    $EMIT->printf("    __section_%s\_index = %d;\n\n",
                    'logging', -1);

    return;
}

sub _heapTypeToSectionName
{
    my ($heapType) = @_;

    $heapType = lc $heapType;

    if (exists $profileRef->{DATA_SECTION_PREFIX}) {
        return $profileRef->{DATA_SECTION_PREFIX} . $heapType . 'Heap';
    }
    else {
        return $heapType . 'Heap';
    }
}

sub EMIT_HEAP_SYMBOL
{
    my ($heapType) = @_;

    $heapType = lc $heapType;

    my $heapSize;

    if ($heapType eq 'freeable') {
        $heapSize = $profileRef->{FREEABLE_HEAP};
    }
    elsif ($heapType eq 'os') {
        $heapSize = $profileRef->{OS_HEAP};
    }
    else {
        $heapSize = 0;
    }

    my $sectionName = _heapTypeToSectionName($heapType);

    if ($heapSize) {
        $EMIT->printf("    __%s\_heap_profile_size = 0x%X;\n", $heapType, $heapSize);
    }

    return;
}

#
# Emit values for freeable heap section with specific names for backward compatibility
# MMINTS-TODO: remove this and replace all uses of these symbols with the sectionName versions.
#
sub EMIT_HEAP_CONSTANTS
{
    my $sectionName = _heapTypeToSectionName('freeable');

    if ($profileRef->{FREEABLE_HEAP}) {
        $EMIT->printf("    __freeable_heap_physaddr = __section_%s\_physaddr;\n", $sectionName);
        $EMIT->printf("    _freeable_heap           = __section_%s\_start;\n", $sectionName);
        $EMIT->printf("    _freeable_heap_size      = __section_%s\_max_size;\n", $sectionName);
        $EMIT->printf("    _freeable_heap_end       = _freeable_heap + _freeable_heap_size;\n");
    }

    return;
}

sub EMIT_ASSERTIONS
{
    my $self = $EMIT_OBJREF;

    $EMIT->printf("%s", $self->{ASSERT_STR});

    return;
}

sub isElfPaSection
{
    # if section is fb, imem_odp, or dmem_odp, and is read-only, then it is run-in-place when enabled
    my ($memSection, $permissiolwalue, $permissionHash) = @_;
    my %runInPlaceLocations = ( fb => 1, imem_odp => 1, dmem_odp => 1, );
    return (!($permissiolwalue & ($permissionHash->{'w'} | $permissionHash->{'W'}))) && # not writeable
            ($runInPlaceLocations{$memSection});
}

sub getFbgpaLength
{
    # MMINTS-TODO: source this magic number from somewhere!
    return 0x0E000000;
}

sub getElfpaLength
{
    # lwrrently getting value as Fbgpa; any section that goes in ELF should
    # already fit in the ELF due to how the linker works
    return getFbgpaLength();
}

#
# Emit Memory Section Size
#
#    CODE (X)  : ORIGIN = 0x00000000, LENGTH = 0x10000
#    DATA (A)  : ORIGIN = 0x10000000, LENGTH = 0xC000
#    DESC (A)  : ORIGIN = 0x30000000, LENGTH = 0x400
#
sub EMIT_MEMORY_SECTIONS
{
    my $self = $EMIT_OBJREF;

    # MMINTS-TODO: too many hard-coded constants here,
    # find a way to source them from configs / headers
    # + PMU doesn't even have EMEM, so would be nice to find a way to remove that on PMU
    my @ememBase = (0x00000000,0x01200000);
    my @dmemBase = (0x00000000,0x00180000);
    my @imemBase = (0x00000000,0x00100000);

    # Since FB load bases are dynamically allocated, fbgpaBase and elfpaBase
    # are replaced with markers which are later patched by the bootloader
    # to include the correct offset.
    #
    # For fbpa the offset would be the extracted data location,
    # for elfpa the offset would be the elf location.
    my @fbgpaBase          = (0xFF000000, 0x00000000);
    my @elfpaBase          = (0xFF100000, 0x00000000);
    my @elf_odp_cow_paBase = (0xFF200000, 0x00000000);

    # Default values in case partition boot is not enabled
    my $dmemCarveout = 0x0;
    my $imemCarveout = 0x0;

    my $dmemEndCarveout = $self->{DMEM_END_CARVEOUT_SIZE};

    if (defined $profileRef->{DMEM_START_CARVEOUT_SIZE}) {
        $dmemCarveout = $profileRef->{DMEM_START_CARVEOUT_SIZE};
    }
    if (defined $profileRef->{IMEM_START_CARVEOUT_SIZE}) {
        $imemCarveout = $profileRef->{IMEM_START_CARVEOUT_SIZE};
    }

    $dmemBase[1] += $dmemCarveout; # SK+partitions DMEM reservation
    $imemBase[1] += $imemCarveout; # SK+partitions IMEM reservation

    $EMIT->printf("    /* VIRTUALLY ADDRESSED REGIONS */\n");
    $EMIT->printf("    vma             : ORIGIN = 0x20000000, LENGTH = 0x80000000\n");
    $EMIT->printf("\n");
    $EMIT->printf("    /* PHYSICALLY ADDRESSED REGIONS */\n");
    $EMIT->printf("    emem            : ORIGIN = 0x%08X%08X, LENGTH = 0x%X\n",
                  $ememBase[0], $ememBase[1],
                  $Profiles->ememPhysicalSize($profileRef));
    $EMIT->printf("    dmem            : ORIGIN = 0x%08X%08X, LENGTH = 0x%X\n",
                  $dmemBase[0], $dmemBase[1],
                  $Profiles->dmemPhysicalSize($profileRef) - $dmemCarveout -
                  $dmemEndCarveout);
    $EMIT->printf("    dmem_carveout   : ORIGIN = 0x%08X%08X, LENGTH = 0x%X\n",
                  $dmemBase[0],
                  $dmemBase[1] + $Profiles->dmemPhysicalSize($profileRef) -
                    $dmemCarveout - $dmemEndCarveout,
                  $dmemEndCarveout);
    $EMIT->printf("    imem            : ORIGIN = 0x%08X%08X, LENGTH = 0x%X\n",
                  $imemBase[0], $imemBase[1],
                  $Profiles->imemPhysicalSize($profileRef) - $imemCarveout);
    $EMIT->printf("    fbgpa           : ORIGIN = 0x%08X%08X, LENGTH = 0x%X\n",
                  $fbgpaBase[0], $fbgpaBase[1],
                  getFbgpaLength());
    $EMIT->printf("    elfpa           : ORIGIN = 0x%08X%08X, LENGTH = 0x%X\n",
                  $elfpaBase[0], $elfpaBase[1],
                  getElfpaLength());

    $EMIT->printf("    elf_odp_cow_pa  : ORIGIN = 0x%08X%08X, LENGTH = 0x%X\n",
                  $elf_odp_cow_paBase[0], $elf_odp_cow_paBase[1],
                  getFbgpaLength());

    return;
}

#
# Emit RM_SHARE and NUM_IMEM_BLOCKS
#   PROVIDE (__rm_share_size   = 0xA00);
#   PROVIDE (__num_imem_blocks = 0x60); /* 24kB */
#
sub EMIT_PROVIDE_MEMORY_SIZE_ITEMS
{
    $EMIT->printf("PROVIDE (__emem_physical_size           = LENGTH(emem));\n");
    $EMIT->printf("PROVIDE (__dmem_physical_size           = LENGTH(dmem));\n");
    $EMIT->printf("PROVIDE (__dmem_carveout_physical_size  = LENGTH(dmem_carveout));\n");
    $EMIT->printf("PROVIDE (__imem_physical_size           = LENGTH(imem));\n");
    $EMIT->printf("PROVIDE (__fbgpa_physical_size          = LENGTH(fbgpa));\n");
    $EMIT->printf("PROVIDE (__elfpa_physical_size          = LENGTH(elfpa));\n");
    $EMIT->printf("PROVIDE (__elf_odp_cow_pa_physical_size = LENGTH(elf_odp_cow_pa));\n");

    $EMIT->printf("PROVIDE (__emem_physical_base           = ORIGIN(emem));\n");
    $EMIT->printf("PROVIDE (__dmem_physical_base           = ORIGIN(dmem));\n");
    $EMIT->printf("PROVIDE (__dmem_carveout_physical_base  = ORIGIN(dmem_carveout));\n");
    $EMIT->printf("PROVIDE (__imem_physical_base           = ORIGIN(imem));\n");
    $EMIT->printf("PROVIDE (__fbgpa_physical_base          = ORIGIN(fbgpa));\n");
    $EMIT->printf("PROVIDE (__elfpa_physical_base          = ORIGIN(elfpa));\n");
    $EMIT->printf("PROVIDE (__elf_odp_cow_pa_physical_base = ORIGIN(elf_odp_cow_pa));\n");

    return;
}

#
# Emit wrapper to print content of $profileRef
#
sub EMIT_PROFILE_DEFINITION
{
    (@_ == 1 or @_ == 2)  or croak 'usage: EMIT_PROFILE_DEFINITION( [FORMAT-STR], FIELD-NAME )';

    my $formatStr;
    my $fieldName;

    if (@_ == 2) {
        $formatStr = shift;
    }
    $fieldName = shift;

    my $fieldValue = $profileRef->{$fieldName};
    croak "EMIT_PROFILE_DEFINITION(): field '$fieldName' is not defined in profile" unless defined $fieldValue;

    if ($formatStr) {
        $EMIT->printf($formatStr, $fieldValue);
    }
    else {
        $EMIT->print($fieldValue);
    }

    return;
}

#
# Emit the section containing MPU information.
#
sub EMIT_MPU_DESCRIPTOR
{
    my $self = $EMIT_OBJREF;
    my $memTarget;

    #
    # This section is normally only used during boot (hence placing it in FB
    # to save on TCM) but pre-extracted images require all sections to be
    # resident, so target DMEM in those instances.
    #
    $EMIT->printf("    /******** LWRISCV MPU descriptor section ********/\n");

    if ($profileRef->{PRE_EXTRACTED}) {
        $EMIT->printf("    . = __dmem_ptr;\n");
        $memTarget = "dmem_res";
    }
    else {
        $EMIT->printf("    . = __fbgpa_ptr;\n");
        $memTarget = "fb";
    }

    EMIT_RISCV_SECTION('LWPU.mpu_info', $memTarget, 0x8, 'RW', 0, [
        'KEEP(*(.LWPU.mpu_info))',
    ]);

    return;
}

# init()
sub init {
    (@_ == 1) or croak 'usage: obj->init( )';

    my $self = shift;
}

sub new {
    (@_ == 10) or croak 'usage: GenLdscriptRiscv->new( NAME, PROFILE-NAME, LW-ROOT, DMEM-END-CARVEOUT-SIZE, DMESG-BUFFER-SIZE, VERBOSE, ELF-IN-PLACE, ELF-IN-PLACE-ODP-COW, IDENTITY-MAPPING)';

    my ($class, $name, $profile, $lwroot, $dmem_end_carveout_size, $dmesg_buffer_size,
        $verbose, $elf_in_place, $elf_in_place_odp_cow, $identity_mapping) = @_;

    my ($buildName, $opmode);

    $profile =~ /^(\w+)-(\w+)/;  # pmu-gf11x ->pmu, gf11x;   dpu-v0201 -> dpu, v0201
                                 # fbflcn-tu10x-gddr -> fbflcn, tu10x (gddr is used as config not as profile)
    ($opmode, $buildName) = ($1, $2);

    $Profiles = Profiles->new($name, 'RISCV');
    $profileRef = $Profiles->grpItemRef($buildName);

    $SectionsCode = Sections->new($name, 'code', 'RiscvSectionsCode.pm', $profileRef->{CODE_SECTION_PREFIX});
    $SectionsData = Sections->new($name, 'data', 'RiscvSectionsData.pm', $profileRef->{DATA_SECTION_PREFIX});

    my $self = {
        NAME                     =>  $name,
        PROFILE_NAME             =>  $profile,
        LW_ROOT                  =>  $lwroot,
        DMEM_END_CARVEOUT_SIZE   =>  $dmem_end_carveout_size,
        DMESG_BUFFER_SIZE        =>  $dmesg_buffer_size,
        IDENTITY_MAPPING         =>  $identity_mapping,
        BUILD                    =>  $buildName,
        OP_MODE                  =>  $opmode,
        VERBOSE                  =>  $verbose,
        ELF_IN_PLACE             =>  $elf_in_place,
        ELF_IN_PLACE_ODP_COW     =>  $elf_in_place_odp_cow,
        PROFILE_REF              =>  $profileRef,
        CODE_SECTIONS_REF        =>  $SectionsCode,
        DATA_SECTIONS_REF        =>  $SectionsData,
        SECTION_IDX              =>  0,
        ASSERT_STR               =>  "",
    };

    return bless $self, $class;
}

1;
