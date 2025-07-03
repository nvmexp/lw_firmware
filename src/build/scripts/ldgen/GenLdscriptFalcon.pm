#!/usr/bin/perl -w

#
# Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# This module is responsible for sourcing a project's OverlaysImem.pm and
# OverlaysDmem.pm and generating all overlay-specific portions of the project's
# linker-script.
#

package GenLdscriptFalcon;

use strict;
no strict qw(vars);
use warnings 'all';

use impl::ProfilesImpl;                # Implementations for Profiles
use impl::OverlaysImpl;                # Implementations for Overlays
use impl::TasksImpl;                   # Implementations for Tasks
use ldgen::elf::ElfOutputSectionImem;
use ldgen::elf::ElfOutputSectionDmem;

use chipGroups;                        # imports CHIPS from Chips.pm and chip functions

use Carp;
use Utils;
use GenFile;
use IO::Handle;                        # for ->flush() method
use IO::File;                          # more methods

my $EMIT = undef;                      # ref to GenFile to emit g_xxx from template
my $EMIT_OBJREF = undef;               # object ref (if any) for emitted code


# global variable to fit old coding style
my $OverlaysImem;
my $OverlaysDmem;
my $profileRef;
my $Profiles;
my $Tasks;
my $DmemOvlVarsType;
my $OpMode;

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

#
# Create an ELF output-section object for each of the DMEM overlays described in the
# OverlaysDmem.pm file.
#
# @return
#     An array of references to each of the newly created ELF output-section
#     objects.
#
sub createOutputSectionsForOverlaysDmem
{
    my @elfSections;

    #
    # Iterate over all items/overlays in the group and create an ELF
    # output-section for each one..
    #
    foreach my $overlayName (@{$OverlaysDmem->grpItemsListRef()})
    {
        # Get a reference to the current overlay (by name).
        my $overlayRef = $OverlaysDmem->grpItemRef($overlayName);

        next unless $OverlaysDmem->isOverlayEnabled($overlayName);

        # Create name for the ELF output-section base on $overlayName
        my $sectionName = $OverlaysDmem->getSectionName($overlayName);

        # Now create a new ELF output-section for the overlay.
        my $elfSectionRef = new ElfOutputSectionDmem($sectionName);

        # Set the section description if it was specified in the .pm file.
        $elfSectionRef->setDescription($overlayRef->{DESCRIPTION})
            if defined $overlayRef->{DESCRIPTION};

        # Set the section flags if they were specified in the .pm file.
        $elfSectionRef->setFlags($overlayRef->{FLAGS})
            if defined $overlayRef->{FLAGS};

        # DMEM overlays are always block aligned
        $elfSectionRef->setAlignment(256);

        $elfSectionRef->setMaxSizeBlocks($overlayRef->{MAX_SIZE_BLOCKS});

        push(@elfSections, $elfSectionRef);
    }

    $OverlaysDmem->{ELF_SECTIONS_LIST} = \@elfSections;
}

#
# Create an ELF output-section object for each of the IMEM overlays described in the
# OverlaysImem.pm file.
#
# @return
#     An array of references to each of the newly created ELF output-section
#     objects.
#
sub createOutputSectionsForOverlaysImem
{
    my @elfSections;

    #
    # Iterate over all items/overlays in the group and create an ELF
    # output-section for each one..
    #
    foreach my $overlayName (@{$OverlaysImem->grpItemsListRef()})
    {
        # Get a reference to the current overlay (by name).
        my $overlayRef = $OverlaysImem->grpItemRef($overlayName);

        # skip this overlay is not enabled
        next unless $OverlaysImem->isOverlayEnabled($overlayName);

        # Create name for the ELF output-section base on $overlayName
        my $sectionName = $OverlaysImem->getSectionName($overlayName);

        # Now create a new ELF output-section for the overlay.
        my $elfSectionRef = new ElfOutputSectionImem($sectionName);

        #
        # If an explicit section-command was specified, use it. Otherwise, add
        # any input-sections to the output-section (the section-command will be
        # derived from the input-sections).
        #
        if (defined $overlayRef->{SECTION_DESCRIPTION})
        {
            my $description = join("\n", @{$overlayRef->{SECTION_DESCRIPTION}});
            $elfSectionRef->setSectionDescription($description);
        }
        else
        {
            if (defined $overlayRef->{INPUT_SECTIONS})
            {
                my @inputSections = @{$overlayRef->{INPUT_SECTIONS}};
                foreach my $i (@inputSections)
                {
                    my $filename    = @{$i}[0];
                    my $sectionName = @{$i}[1];
                    $elfSectionRef->addInputSection($sectionName, $filename);
                }
            }
            if (defined $overlayRef->{KEEP_SECTIONS})
            {
                my @keepSections = @{$overlayRef->{KEEP_SECTIONS}};
                foreach my $i (@keepSections)
                {
                    my $sectionName = @{$i}[0];
                    $elfSectionRef->addKeepSection($sectionName);
                }
            }
        }

        # Set the section alignment if it was specified in the .pm file.
        $elfSectionRef->setAlignment($overlayRef->{ALIGNMENT})
            if defined $overlayRef->{ALIGNMENT};

        # Set the section description if it was specified in the .pm file.
        $elfSectionRef->setDescription($overlayRef->{DESCRIPTION})
            if defined $overlayRef->{DESCRIPTION};

        # Set the section flags if they were specified in the .pm file.
        $elfSectionRef->setFlags($overlayRef->{FLAGS})
            if defined $overlayRef->{FLAGS};

        if ($elfSectionRef->isFlagSet(':HS_OVERLAY'))
        {
            # Ensure that we have a keep section specified
            if (!defined $overlayRef->{KEEP_SECTIONS})
            {
                croak "Error: HS overlay needs a KEEP_SECTION specified";
            }
            # Force 256-byte alignment for HS overlay
            $elfSectionRef->setAlignment(256);
        }
        else
        {
            if ($elfSectionRef->isFlagSet(':HS_LIB'))
            {
                croak "Error: HS_LIB must also be a HS_OVERLAY";
            }
        }

        # Set the section description if it was specified in the .pm file.
        $elfSectionRef->setLsSigGroup($overlayRef->{LS_SIG_GROUP})
            if defined $overlayRef->{LS_SIG_GROUP};

        push(@elfSections, $elfSectionRef);
    }

    $OverlaysImem->{ELF_SECTIONS_LIST} = \@elfSections;
}

################################################################################

#
# Emit Memory Section Size
#
#    CODE (X)  : ORIGIN = 0x00000000, LENGTH = 0x10000
#    DATA (A)  : ORIGIN = 0x10000000, LENGTH = 0xC000
#    DESC (A)  : ORIGIN = 0x30000000, LENGTH = 0x400
#
sub EMIT_MEMORY_SECTIONS
{

    $EMIT->printf("    CODE (X)  : ORIGIN = 0x00000000, LENGTH = 0x%X\n", $Profiles->imemMaxSize($profileRef));
    $EMIT->printf("    DATA (A)  : ORIGIN = 0x10000000, LENGTH = 0x%X\n", $Profiles->dmemMaxSize($profileRef));
    $EMIT->printf("    DESC (A)  : ORIGIN = 0x30000000, LENGTH = 0x%X\n", $Profiles->descMaxSize($profileRef));

    return;
}

#
# Emit RM_SHARE and NUM_IMEM_BLOCKS
#   PROVIDE (__rm_share_size   = 0xA00);
#   PROVIDE (__num_imem_blocks = 0x60); /* 24kB */
#
sub EMIT_PROVIDE_MEMORY_SIZE_ITEMS
{
    my $imemSize = $Profiles->imemPhysicalSize($profileRef);
    my $dmemSize = $Profiles->dmemPhysicalSize($profileRef);

    $EMIT->printf("PROVIDE (__rm_share_size   = 0x%04X);\n", $Profiles->rmShareSize($profileRef));
    $EMIT->printf("PROVIDE (__num_imem_blocks = 0x%04X); /* %dkB */\n",
                  $imemSize / 256, $imemSize / 1024); # blocks, kB
    $EMIT->printf("PROVIDE (__num_dmem_blocks = 0x%04X); /* %dkB */\n",
                  $dmemSize / 256, $dmemSize / 1024); # blocks, kB
    if ($Profiles->dmemVaBound($profileRef))
    {
        my $blocks = $Profiles->dmemVaBound($profileRef) / 256;
        $EMIT->printf("PROVIDE (__dmem_va_bound   = 0x%04X); /* %d blocks */\n",
                $blocks, $blocks);
    }

    return;
}

#
# Helper function to emit the section-command for each DMEM overlay output-section.
#
sub _emit_dmem_overlay_output_sections
{
    @_ == 2 or croak 'usage: _emit_dmem_overlay_output_sections( OVERLAY-RESIDENT, INDENT )';
    my $isResident = shift;
    my $indent = shift;

    foreach my $elfSectionRef (@{$OverlaysDmem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->isOvlResident() eq $isResident)
        {
            if ($isResident eq 0)
            {
                #
                # Dummy guard block to help catch issues where we overrun from one
                # overlay into another that might have been (un)luckily paged in and
                # masked the access error.
                #
                $EMIT->printf($indent."_lwr_dmem_va_location += 256;\n");
            }

            my $description = $elfSectionRef->getDescription();
            my $name        = $elfSectionRef->getName();

            $EMIT->print($indent."/* ${description} */\n")   if defined $description;
            $EMIT->print_lines_indented($indent, $elfSectionRef->getSectionDescription($isResident));

            #
            # Emit the initial load stop location, used by all the static data. We
            # will use this information for the current size of the overlay.
            #
            $EMIT->printf($indent."__load_stop_initial_$name = LOADADDR(.$name) + SIZEOF(.$name);\n");

            if ($isResident eq 0)
            {
                # Modify the current location pointer
                $EMIT->printf($indent."_lwr_dmem_va_location += %d;\n\n", ($elfSectionRef->getMaxSizeBlocks()*256));
            }

            #
            # Emit the load start and max load stop locations of the DMEM overlay.
            # The max load stop address accounts for all the static data as well as
            # the remaining part of the overlay which can be used as a heap.
            #
            $EMIT->printf($indent."__load_start_$name = LOADADDR(.$name);\n");
            if ($isResident eq 0)
            {
                $EMIT->printf($indent."__load_stop_max_$name  = _lwr_dmem_va_location;\n");
            }
            else
            {
                $EMIT->printf($indent."__load_stop_max_$name  = __load_start_$name + ((SIZEOF(.$name) > 0) ? %d : 0);\n", ($elfSectionRef->getMaxSizeBlocks()*256));
            }
        }
    }
}

#
# Emit the section-command for each DMEM overlay output-section.
#
sub EMIT_DMEM_OVERLAY_OUTPUT_SECTIONS
{
    @_ == 2 or croak 'usage: EMIT_DMEM_OVERLAY_OUTPUT_SECTIONS( OVERLAY-RESIDENT, INDENT )';
    my $isResident = shift;
    my $indent = shift;

    if ($isResident eq 'non_resident_overlay')
    {
        if (! $Profiles->dmemVaSize($profileRef))
        {
            # Nothing to print if we don't have DMEM VA enabled
            return;
        }

        $EMIT->print($indent."/* DMEM overlay output sections */\n");

        #
        # Emit a symbol that we will use to determine the address of each DMEM
        # overlay section
        #
        $EMIT->print($indent."_lwr_dmem_va_location = ORIGIN(DATA) + __end_physical_dmem;\n");

        _emit_dmem_overlay_output_sections(0, $indent);

        $EMIT->printf($indent."__dmem_va_blocks_max = (_lwr_dmem_va_location & 0xFFFFFFF) >> 8;\n");
    }
    elsif ($isResident eq 'resident_overlay')
    {
        $EMIT->print($indent."/* resident DMEM overlay output sections */\n");

        _emit_dmem_overlay_output_sections(1, $indent);
    }

    return;
}

#
# Emit the section-contents of the Resident IMEM segment
# @param[in]  shift  The prefix of each line
#
sub EMIT_IMEM_RESIDENT_SECTION
{
    @_ == 1 or croak 'usage: EMIT_IMEM_RESIDENT_SECTION(  INDENT )';
    my $indent = shift;

    $ovlCount = scalar @{$OverlaysImem->{ELF_SECTIONS_LIST}};
    if ($ovlCount ne 0)
    {
        # There are overlay sections, so exclude the code which will go in the overlay segments
        $EMIT->print($indent."*(.start)\n");
        $EMIT->print($indent."*(\n");
        $EMIT->print($indent."    EXCLUDE_FILE\n");
        $EMIT->print($indent."    (\n");
        $EMIT->print($indent."        */*.o\n");
        $EMIT->print($indent."    )\n");
        $EMIT->print($indent."    .text.*\n");
        $EMIT->print($indent." )\n");
        $EMIT->print($indent."*(\n");
        $EMIT->print($indent."    EXCLUDE_FILE\n");
        $EMIT->print($indent."    (\n");
        $EMIT->print($indent."        /*\n");
        $EMIT->print($indent."         * GCC helpers explicitly moved into library overlays.\n");
        $EMIT->print($indent."         */\n");
        $EMIT->print($indent."        *di3.o          /* 64-bit integer math. */\n");

        $EMIT->print($indent."        *sf.o           /* Single precision (32-bit) FP math. */\n");
        $EMIT->print($indent."        *si.o           /* Single precision (32-bit) FP math. */\n");
        $EMIT->print($indent."        *si2.o          /* Single precision (32-bit) FP math. */\n");

        $EMIT->print($indent."    )\n");
        $EMIT->print($indent."    .text\n");
        $EMIT->print($indent." )\n");
        $EMIT->print($indent."*(.imem_resident.*)\n");
        $EMIT->print($indent."*(.imem_instRuntime.*)\n");
        $EMIT->print($indent."*(.imem_ustreamerRuntime.*)\n");
    }
    else
    {
        # There are no overlay sections, so this is an all-resident memory layout
        # Place all the sections into this segment
        $EMIT->print($indent."*(.start)\n");
        $EMIT->print($indent."*(.text.*)\n");
        $EMIT->print($indent."*(.text)\n");
        $EMIT->print($indent."*(.imem_*.*)");
    }
    return;
}

#
# Emit the section-contents of the Resident DMEM section
# @param[in]  shift  The prefix of each line
#
sub EMIT_DMEM_RESIDENT_SECTION
{
    @_ == 1 or croak 'usage: EMIT_DMEM_RESIDENT_SECTION(  INDENT )';
    my $indent = shift;

    $EMIT->print($indent."*(.nullSection.*)\n");
    $EMIT->print($indent."*(.alignedData256.*)\n");
    $EMIT->print($indent."*(.alignedData128.*)\n");
    $EMIT->print($indent."*(.alignedData64.*)\n");
    $EMIT->print($indent."*(.alignedData32.*)\n");
    $EMIT->print($indent."*(.alignedData16.*)\n");
    $EMIT->print($indent."*(.rodata)\n");
    if ((defined $OverlaysDmem->{ELF_SECTIONS_LIST}) && (scalar @{$OverlaysDmem->{ELF_SECTIONS_LIST}} ne 0))
    {
        $EMIT->print($indent."*(.dmem_residentLwos.*)\n");
    }
    else
    {
        # There are no DMEM overlay sections, so place all the data in this segment
        $EMIT->print($indent."*(.dmem_*.*)\n");
    }
    $EMIT->print($indent."*(.data)\n");
    $EMIT->print($indent."*(.bss)\n");
    $EMIT->print($indent."*(COMMON)");

    return;
}

#
# Emit the section-command for each IMEM overlay output-section.
# @param[in]  ovlSelwrityType  'hs_overlay'     if heavy secure
#                              'non_hs_overlay' if non-heavy secure
#
sub EMIT_IMEM_OVERLAY_OUTPUT_SECTIONS
{
    @_ == 2 or croak 'usage: EMIT_IMEM_OVERLAY_OUTPUT_SECTIONS( OVERLAY-SECURITY-TYPE, INDENT )';
    my $ovlSelwrityType = shift;
    my $indent = shift;

    $EMIT->print($indent."__${ovlSelwrityType}_code_start = ALIGN(256);\n");
    $EMIT->print($indent."__${ovlSelwrityType}_base       = __${ovlSelwrityType}_code_start;\n\n");

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() eq $ovlSelwrityType)
        {
            my $description = $elfSectionRef->getDescription();

            $EMIT->print($indent."/* ${description} */\n")   if defined $description;
            $EMIT->print_lines_indented($indent, $elfSectionRef->getSectionDescription());
        }
    }

    $EMIT->print($indent."__overlay_end_imem = .;\n");

    $EMIT->print($indent."__${ovlSelwrityType}_code_end = ALIGN(256);\n");

    return;
}

#
# Emit the section-command for each IMEM overlay output-section.
# @param[in]  ovlSelwrityType  'hs_overlay'     if heavy secure
#                              'non_hs_overlay' if non-heavy secure
#
sub EMIT_CODE_PATCH_HOLES
{
    @_ == 2 or croak 'usage: EMIT_CODE_PATCH_HOLES( PATCH-TYPE, INDENT )';
    my $patchType = shift;
    my $indent = shift;

    if (! $Profiles->hsEncryptionSymbols($profileRef))
    {
        return;
    }

    #
    # If BOOT_FROM_HS is enabled, then skip output of 
    # dbg and prod specific patch holes and only
    # output a single non-specific patch hole.
    #
    if ($Profiles->bootFromHSEnabled($profileRef))
    {
        if (($patchType eq "patch_dbg_") || ($patchType eq "patch_prd_"))
        {
            return;
        }
    }
    else
    {
        if ($patchType eq "patch_")
        {
            return;       
        }
    }

    $EMIT->print($indent."__${patchType}_code_start = ALIGN(256);\n");

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() eq 'hs_overlay')
        {
            my $name = $elfSectionRef->getName();
            my $sectionName = $patchType . $name;
            $sectionName =~ s/imem_//;

            $EMIT->print($indent. $sectionName . " : ALIGN(256)\n");
            $EMIT->print($indent."{\n");
            # Emit a dummy byte else this whole section gets optimized out as unused
            # costs us an extra block in VA space per patch section
            $EMIT->print($indent.$indent."BYTE(0x00)\n");
            $EMIT->print($indent.$indent.". = . + SIZEOF(." . $name . ");\n");
            $EMIT->print($indent."}");
            $EMIT->print(" > CODE\n");
            $EMIT->print($indent."\n");
            $EMIT->print($indent."PROVIDE (_" . $sectionName . " = ADDR(" . $sectionName . "));\n");
            $EMIT->print($indent.$indent.". = . + SIZEOF(." . $name . ");\n");
        }
    }

    $EMIT->print($indent."__${patchType}_code_end = ALIGN(256);\n");

    return;
}

#
# Emit the LS Signature group section at end of IMEM overlay output-section.
# @param[in] indent Indentation required
#
sub EMIT_IMEM_LS_SIG_GROUP_SECTIONS
{
    @_ == 3 or croak 'usage: EMIT_IMEM_LS_SIG_GROUP_SECTIONS( INDENT, resident_code_start, resident_code_end_aligned )';
    my $indent = shift;
    my $rstart = shift;
    my $rend   = shift;

    my $allprints  = "";
    my $entryCount = 0;
    my $bFoundAtleastOne =  0;

    # Add resident code in group
    $entryCount += 1;
    $allprints = $allprints.$indent.$indent."LONG($rstart);";
    $allprints = $allprints."LONG($rend);";
    $allprints = $allprints."LONG(0xFF);\n";

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        my $lsSigGroup = $elfSectionRef->getLsSigGroup();
        if(($elfSectionRef->isOvlResident()) &&
           ($elfSectionRef->getOvlSelwrityType() ne 'hs_overlay'))
        {
            $lsSigGroup = 0xFF;
        }
        if ($lsSigGroup > 0)
        {
            my $name  = $elfSectionRef->getName();
            $entryCount += 1;
            $allprints = $allprints.$indent.$indent."LONG(__load_start_$name);";
            $allprints = $allprints."LONG(__load_stop_$name);";
            $allprints = $allprints."LONG($lsSigGroup);\n";

            if ($lsSigGroup != 0xFF)
            {
                $bFoundAtleastOne = 1;
            }
        }
    }

    if (($entryCount > 0) && ($bFoundAtleastOne == 1))
    {
        $EMIT->print($indent."__ls_sig_group_desc_start = ALIGN(256);\n");
        $EMIT->print($indent."__ls_sig_group_desc__base = __ls_sig_group_desc_start;\n\n");

        #
        # MAGIC PATTERN will be used to identify LS sig groups.
        # First 3 DWORDS has the pattern. 4th DWORD will have number of entries
        #
        $EMIT->print($indent.".imem_ls_sig_group : ALIGN(256)\n");
        $EMIT->print($indent."{\n");
        $EMIT->print($indent.$indent."LONG(0xED300700);");
        $EMIT->print("LONG(0xEE703000);");
        $EMIT->print("LONG(0xCE300300);");
        $EMIT->print("LONG($entryCount);\n");
        $EMIT->print($allprints);
        $EMIT->print($indent.$indent.". = ALIGN(256);\n");
        $EMIT->print($indent."} > CODE\n");
        $EMIT->print($indent."__ls_sig_group_desc_stop = .;\n");
    }

    return;
}

#
# Helper function to emit IMEM overlay symbols
# @param[in]  ovlSelwrityType  'hs_overlay'     if heavy secure
#                              'non_hs_overlay' if non-heavy secure
#
sub _emitImemOverlaySymbols
{
    @_ == 2 or croak 'usage: _emitImemOverlaySymbols( OVERLAY-SECURITY-TYPE, INDENT )';
    my $ovlSelwrityType = shift;
    my $indent = shift;

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        my $name = $elfSectionRef->getName();

        if ($elfSectionRef->getOvlSelwrityType() eq $ovlSelwrityType)
        {
            $EMIT->print($indent."__load_stop_$name   = LOADADDR(.$name) + SIZEOF(.$name);\n");
            $EMIT->print($indent."__load_start_$name  = LOADADDR(.$name) & ~(0xFF);\n");
            $EMIT->print($indent."__overlay_id_imem__count += 1;\n\n");
            $EMIT->print($indent."__overlay_id_$name  = ((SIZEOF(.$name) > 0) ? ".
                         "__overlay_id_imem__count : 0);\n");

            if ($elfSectionRef->isFlagSet(':HS_OVERLAY'))
            {
                my $isHsLib = 0;

                # Emit symbols used by siggen (only needed for HS overlays)
                $EMIT->print($indent."_$name" . "BaseFB = ADDR(.$name);\n");
                $EMIT->print($indent."_$name" . "Size   = ALIGN(SIZEOF(.$name), 256);\n");

                # Emit symbol to mark if it is a library
                if ($elfSectionRef->isFlagSet(':HS_LIB'))
                {
                    $isHsLib = 1;
                }
                $EMIT->printf($indent."__overlay_$name"."_is_hs_lib = %d;\n",
                              $isHsLib);
            }
        }
    }
}

#
# Helper function to emit the signature symbols for HS overlays
# @param[in]  signType         'dbg' for debug signatures
#                              'prd' for production signatures
# @param[in]  sigSize          size of signature in DWORD size
# @param[in]  numSigPerHsUcode number of signatures per hs overlay
#
sub _emit_imem_hs_signatures
{
    @_ == 4 or croak 'usage: _emit_imem_hs_signatures( SIGNATURE-TYPE, INDENT, SIGNATURE-SIZE, NUM_SIG_PER_HS_UCODE )';
    my $signType = shift;
    my $indent   = shift;
    my $sigSize  = shift;
    my $numSigPerHsUcode = shift;

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() ne 'hs_overlay')
        {
            next;
        }

        my $name = $elfSectionRef->getName();

        if($Profiles->rmPatchingHsSignatureEnabled($profileRef))
        {
            $EMIT->print($indent."_g_$name" ."_signature = .;\n");
            for (my $i=0; $i<$sigSize; $i++)
            {
                $EMIT->print($indent."LONG(0x00000000);\n");
            }
        }
        else
        {
            #
            # If signType is blank, then be sure not to put in an extra '_'
            #
            if ($signType eq "")
            {
                $EMIT->print($indent."_g_$name" ."_signature = .;\n");
            }
            else
            {
                $EMIT->print($indent."_g_$name"."_$signType"."_signature = .;\n");
            }
            for (my $j=0; $j<$numSigPerHsUcode; $j++)
            {
                for (my $i=0; $i<$sigSize; $i++)
                {
                    $EMIT->print($indent."LONG(0x00000000);\n");
                }
            }
        }
    }
}

#
# Emit HS overlay signatures
#
sub EMIT_IMEM_HS_SIGNATURES_LIST
{
    @_ == 4 or croak 'usage: EMIT_IMEM_HS_SIGNATURES_LIST( INDENT, SIGNATURE-TYPE, SECTION, ADDRESS )';
    my $indent = shift;
    my $signType = shift;
    my $section = shift;
    my $address = shift;
    my $sigSizeInBits = 128; # default AES 128 bits
    my $numSigPerHsUcode = $Profiles->hsNumSigs($profileRef);

    if ($Profiles->hsPkcEnabled($profileRef))
    {
        $sigSizeInBits = $Profiles->hsPkcSigSize($profileRef);
    }

    #
    # If BOOT_FROM_HS is enabled, then skip output of 
    # dbg and prod signatures and only output single
    # non specific signatures.
    #
    if ($Profiles->bootFromHSEnabled($profileRef) || $Profiles->rmPatchingHsSignatureEnabled($profileRef))
    {
        if (($signType eq "dbg") || ($signType eq "prd"))
        {
            return;
        }
    }
    else
    {
        if ($signType eq "")
        {
            return;       
        }
    }

    $EMIT->print("    .$address : ALIGN(16)\n");
    $EMIT->print("    {\n");
    _emit_imem_hs_signatures($signType, $indent, $sigSizeInBits/32, $numSigPerHsUcode);
    $EMIT->print("\n    }\n");
    $EMIT->print("    PROVIDE ($section = ADDR(.$address));");

    return;
}

#
# Emit the ucode id for the overlay (to be patched by siggen)
#
sub EMIT_IMEM_HS_SIGNATURE_UCODE_ID
{
    @_ == 1 or croak 'usage: EMIT_IMEM_HS_SIGNATURE_UCODE_ID( INDENT )';
    my $indent = shift;

    if (!$Profiles->hsPkcEnabled($profileRef))
    {
        return;
    }

    $EMIT->print("__imem_hs_sig_ucode_id = .;\n");


    #
    # We don't really need this dummy ucode id for the invalid overlays but
    # it's done to have a cleaner one to one mapping, essentially so that
    # we can use ovlIdx directly instead of using (ovlIdx-1) while
    # accessing this data
    #
    $EMIT->print($indent."_g_imem_dummy_ucode_id = .;\n");
    $EMIT->print($indent."BYTE(0x00);\n");

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() ne 'hs_overlay')
        {
            next;
        }
        my $name = $elfSectionRef->getName();

        $EMIT->print($indent."_g_${name}_ucode_id = .;\n");
        $EMIT->print($indent."BYTE(0x00);\n");
    }

    return;
}

#
# Emit the fuse version for the overlay (to be patched by siggen)
#
# This is required if multiple HS signatures are supported by the ucode
#
sub EMIT_IMEM_HS_SIGNATURE_FUSE_VER
{
    @_ == 1 or croak 'usage: EMIT_IMEM_HS_SIGNATURE_FUSE_VER( INDENT )';
    my $indent = shift;

    if (!$Profiles->hsPkcEnabled($profileRef))
    {
        return;
    }

    $EMIT->print("__imem_hs_sig_ucode_fuse_ver = .;\n");

    #
    # We don't really need this dummy ucode id for the invalid overlays but
    # it's done to have a cleaner one to one mapping, essentially so that
    # we can use ovlIdx directly instead of using (ovlIdx-1) while
    # accessing this data
    #
    $EMIT->print($indent."_g_imem_dummy_fuse_ver = .;\n");
    $EMIT->print($indent."BYTE(0x00);\n");

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() ne 'hs_overlay')
        {
            next;
        }
        my $name = $elfSectionRef->getName();

        $EMIT->print($indent."_g_${name}_fuse_ver = .;\n");
        $EMIT->print($indent."BYTE(0x00);\n");
    }

    return;
}

#
# Emit the algorithm type for the overlay (to be patched by siggen)
#
# Ucode can have both AES and PKC based HS overlays
#
sub EMIT_IMEM_HS_SIGNATURE_ALGO_TYPE
{
    @_ == 1 or croak 'usage: EMIT_IMEM_HS_SIGNATURE_ALGO_TYPE( INDENT )';
    my $indent = shift;

    if (!$Profiles->hsPkcEnabled($profileRef))
    {
        return;
    }

    $EMIT->print("__imem_hs_sig_ucode_algo_type = .;\n");

    #
    # We don't really need this dummy ucode id for the invalid overlays but
    # it's done to have a cleaner one to one mapping, essentially so that
    # we can use ovlIdx directly instead of using (ovlIdx-1) while
    # accessing this data
    #
    $EMIT->print($indent."_g_imem_dummy_algo_type = .;\n");
    $EMIT->print($indent."BYTE(0x00);\n");

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() ne 'hs_overlay')
        {
            next;
        }
        my $name = $elfSectionRef->getName();

        $EMIT->print($indent."_g_${name}_algo_type = .;\n");
        $EMIT->print($indent."BYTE(0x00);\n");
    }

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
    } else {
        $EMIT->print($fieldValue);
    }

    return;
}


#
# Emit DMEM overlay IDs
#
sub EMIT_DMEM_OVERLAY_ID
{
    @_ == 1 or croak 'usage: EMIT_DMEM_OVERLAY_ID( INDENT )';
    my $indent = shift;

    # Start with the required DMEM overlay: os heap
    $EMIT->print($indent."__overlay_id_dmem__count  = 0;\n\n");
    $EMIT->print($indent."__overlay_id_dmem_os      = __overlay_id_dmem__count;\n");
    $EMIT->print($indent."__overlay_id_dmem__count += 1;\n\n");

    # Then move to the ones part of the profile
    foreach my $elfSectionRef (@{$OverlaysDmem->{ELF_SECTIONS_LIST}})
    {
        my $name = $elfSectionRef->getName();
        $EMIT->print($indent."__overlay_id_$name  = __overlay_id_dmem__count;\n");
        $EMIT->print($indent."__overlay_id_dmem__count += 1;\n\n");
    }
}

#
# Emit the size of the resident HS overlays
#
sub EMIT_IMEM_HS_RESIDENT_OVERLAY_SIZE
{
    $EMIT->printf("0");

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->isOvlResident())
        {
            my $name = $elfSectionRef->getName();
            $EMIT->printf(" + SIZEOF(.$name) ");
        }
    }
    $EMIT->printf(";\n");
    return;
}

sub _emitImemOverlayAttributeResident
{
    @_ == 4 or croak 'usage: _emitImemOverlayAttributeResident( OVERLAY-SECURITY-TYPE, LWR-BYTE, LWR-COUNTER, INDENT )';
    my $ovlSelwrityType = shift;
    my $lwrByte = shift;
    my $lwrCounter = shift;
    my $indent = shift;

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() eq $ovlSelwrityType)
        {
            if ($elfSectionRef->isOvlResident())
            {
                $lwrByte |= (1 << $lwrCounter);
            }
            $lwrCounter++;
            if (($lwrCounter % 8) == 0)
            {
                $EMIT->print($indent."BYTE($lwrByte);\n");
                $lwrByte = 0;
                $lwrCounter = 0;
            }
        }
    }
    return ($lwrByte, $lwrCounter);
}

#
# Emit the resident attribtues of IMEM overlays
#
sub EMIT_IMEM_OVERLAY_ATTRIBUTE_RESIDENT
{
    @_ == 1 or croak 'usage: EMIT_IMEM_OVERLAY_ATTRIBUTE_RESIDENT( INDENT )';
    my $indent = shift;

    # First check if he have any HS overlays on this build
    my $count = _getNumHsOverlays();

    if ($count == 0)
    {
        # No HS overlays, hence no need for resident overlays; return early
        return;
    }

    my $lwrByte = 0;
    my $lwrCounter = 1; # Skip 0th overlay (invalid)

    ($lwrByte, $lwrCounter) = _emitImemOverlayAttributeResident('hs_overlay', $lwrByte, $lwrCounter, $indent);
    ($lwrByte, $lwrCounter) = _emitImemOverlayAttributeResident('non_hs_overlay', $lwrByte, $lwrCounter, $indent);

    if ($lwrCounter != 0)
    {
        # Emit any spillovers that were left after the helpers finished
        $EMIT->print($indent."BYTE($lwrByte);\n");
    }
    return;
}

#
# Emit the resident attribtues of DMEM overlays
#
sub EMIT_DMEM_OVERLAY_ATTRIBUTE_RESIDENT
{
    @_ == 1 or croak 'usage: EMIT_DMEM_OVERLAY_ATTRIBUTE_RESIDENT( INDENT )';
    my $indent = shift;

    my $lwrByte = 0;
    my $lwrCounter = 1; # Skip 0th overlay (i.e. os_heap)

    foreach my $elfSectionRef (@{$OverlaysDmem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->isOvlResident())
        {
            $lwrByte |= (1 << $lwrCounter);
        }
        $lwrCounter++;
        if (($lwrCounter % 8) == 0)
        {
            $EMIT->print($indent."BYTE($lwrByte);\n");
            $lwrByte = 0;
            $lwrCounter = 0;
        }
    }

    if ($lwrCounter != 0)
    {
        # Emit any spillovers that were left after the helpers finished
        $EMIT->print($indent."BYTE($lwrByte);\n");
    }
    return;
}

#
# Emit start addresses of each DMEM overlay
#
sub EMIT_DMEM_OVERLAY_START_ADDRESS
{
    @_ == 1 or croak 'usage: EMIT_DMEM_OVERLAY_START_ADDRESS( INDENT )';
    my $indent = shift;

    # Start with the required DMEM overlay: os heap
    $EMIT->print($indent."LONG(__heap);\n");

    # Then move to the overlays specified in OverlaysDmem.pm
    foreach my $elfSectionRef (@{$OverlaysDmem->{ELF_SECTIONS_LIST}})
    {
        my $name = $elfSectionRef->getName();
        $EMIT->printf($indent."LONG(__load_start_$name & ~(0x10000000));\n");
    }
}

#
# Emit current sizes of each DMEM overlay
#
sub EMIT_DMEM_OVERLAY_SIZE_LWRRENT
{
    @_ == 1 or croak 'usage: EMIT_DMEM_OVERLAY_SIZE_LWRRENT( INDENT )';
    my $indent = shift;

    #
    # Start with the required DMEM overlay: os heap. Its current size is zero
    # initially.
    #
    $EMIT->print($indent."$DmemOvlVarsType(0x0000);\n");

    # Then move to the overlays specified in OverlaysDmem.pm
    foreach my $elfSectionRef (@{$OverlaysDmem->{ELF_SECTIONS_LIST}})
    {
        my $name = $elfSectionRef->getName();

        $EMIT->print($indent."$DmemOvlVarsType(__load_stop_initial_$name - __load_start_$name);\n");
    }
}

#
# Emit max sizes of each DMEM overlay
#
sub EMIT_DMEM_OVERLAY_SIZE_MAX
{
    @_ == 1 or croak 'usage: EMIT_DMEM_OVERLAY_SIZE_MAX( INDENT )';
    my $indent = shift;

    # Start with the required DMEM overlay: os heap
    $EMIT->print($indent."$DmemOvlVarsType(__heap_size);\n");

    # Then move to the overlays specified in OverlaysDmem.pm
    foreach my $elfSectionRef (@{$OverlaysDmem->{ELF_SECTIONS_LIST}})
    {
        my $name = $elfSectionRef->getName();
        $EMIT->printf($indent."$DmemOvlVarsType(__load_stop_max_$name - __load_start_$name);\n");
    }
}

#
# Emit load start/stop and ids for each IMEM overlay.
#
sub EMIT_IMEM_OVERLAY_SYMBOLS
{
    @_ == 1 or croak 'usage: EMIT_IMEM_OVERLAY_SYMBOLS( INDENT )';
    my $indent = shift;

    #
    # Overlays are assigned their IDs starting from '1' to reserve value '0' for
    # invalid overlay ID.  All invalid overlays (those of zero size) are assigned
    # invalid ID (0x00).  Any changes to this assignment has to be in sync with
    # changes to corresponding RTOS code.
    #
    $EMIT->print($indent."__overlay_id_imem__count = 0;\n\n");

    #
    # We start with the HS overlays (if they exist),
    # and then move to the non-HS overlays.
    #
    _emitImemOverlaySymbols('hs_overlay', $indent);
    _emitImemOverlaySymbols('non_hs_overlay', $indent);

    return;
}

#
# Helper function to emit overlay descriptor table
# @param[in]  ovlSelwrityType  'hs_overlay'     if heavy secure
#                              'non_hs_overlay' if non-heavy secure
#
sub _emitImemOverlayDescriptorTable
{
    @_ == 2 or croak 'usage: _emitImemOverlayDescriptorTable( OVERLAY-SECURITY-TYPE, INDENT )';
    my $ovlSelwrityType = shift;
    my $indent = shift;

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() eq $ovlSelwrityType)
        {
            my $name = $elfSectionRef->getName();

            $EMIT->print($indent."LONG(LOADADDR(.$name));\n");
            $EMIT->print($indent."LONG(__load_stop_$name);\n");
            $EMIT->printf($indent."LONG(%d);\n\n", $elfSectionRef->isOvlResident());
        }
    }
}

#
# Emit the IMEM overlay descriptor table. The descriptor table is simply a set
# of load start/stop words that are to be written to the non-exelwtable .desc
# section of the image (later used by mkimage scripts).
#
sub EMIT_IMEM_OVERLAY_DESCRIPTOR_TABLE
{
    @_ == 1 or croak 'usage: EMIT_IMEM_OVERLAY_DESCRIPTOR_TABLE( INDENT )';
    my $indent = shift;

    #
    # We start with the HS overlays (if they exist),
    # and then move to the non-HS overlays.
    #
    _emitImemOverlayDescriptorTable('hs_overlay', $indent);
    _emitImemOverlayDescriptorTable('non_hs_overlay', $indent);

    return;
}

#
# Emit the DMEM overlay descriptor table. The descriptor table is simply a set
# of load start/stop words that are to be written to the non-exelwtable .desc
# section of the image (later used by mkimage scripts).
#
sub EMIT_DMEM_OVERLAY_DESCRIPTOR_TABLE
{
    @_ == 1 or croak 'usage: EMIT_DMEM_OVERLAY_DESCRIPTOR_TABLE( INDENT )';
    my $indent = shift;

    # Start with the os overlay
    $EMIT->print($indent."LONG(__heap);\n");      # startAddress
    $EMIT->print($indent."LONG(0x00000000);\n");  # lwrrentSize
    $EMIT->print($indent."LONG(__heap_size);\n"); # maxSize
    $EMIT->print($indent."LONG(0x00000001);\n");  # resident

    # Then the overlays from OverlaysDmem.pm (if any)
    foreach my $elfSectionRef (@{$OverlaysDmem->{ELF_SECTIONS_LIST}})
    {
        my $name = $elfSectionRef->getName();

        $EMIT->print($indent."LONG(__load_start_$name);\n");                             # startAddress
        $EMIT->print($indent."LONG(__load_stop_initial_$name - __load_start_$name);\n"); # lwrrentSize
        $EMIT->print($indent."LONG(__load_stop_max_$name - __load_start_$name);\n");     # maxSize
        $EMIT->printf($indent."LONG(%d);\n\n", $elfSectionRef->isOvlResident());         # resident
    }

    return;
}

#
# Helper function to emit IMEM overlay tag list
# @param[in]  ovlSelwrityType  'hs_overlay'     if heavy secure
#                              'non_hs_overlay' if non-heavy secure
#
sub _emitImemOverlayTagList
{
    @_ == 2 or croak 'usage: _emitImemOverlayTagList( OVERLAY-SECURITY-TYPE, INDENT )';
    my $ovlSelwrityType = shift;
    my $indent = shift;

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() eq $ovlSelwrityType)
        {
            my $name = $elfSectionRef->getName();

            $EMIT->print($indent."SHORT(__load_start_$name >> 8);\n");
        }
    }
}

#
# Helper function to emit patched IMEM overlay tag list
# @param[in]  prefix  prefix to be used to generate linker script symbols
#
sub _emitPatchedImemOverlayTagList
{
    @_ == 2 or croak 'usage: _emitPatchedImemOverlayTagList( PREFIX, INDENT )';
    my $prefix = shift;
    my $indent = shift;

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() eq 'hs_overlay')
        {
            my $name = $elfSectionRef->getName();
            $name =~ s/imem_//;
            $EMIT->print($indent."SHORT(_" . $prefix . "$name >> 8);\n");
        }
    }
}

#
# Emit some dummy data in order to generate a hole at DMEM
#
sub EMIT_DMEM_HOLE
{
    @_ == 1 or croak 'usage: EMIT_DMEM_HOLE( INDENT )';
    my $indent = shift;

    # First check if we have any HS overlay on this build
    my $count = _getNumHsOverlays();

    if ($count == 0)
    {
        # DMEM hole is only needed when there is any HS overlay; return early
        return;
    }

    $EMIT->printf($indent."BYTE(0x00);");
    return;
}

#
# Emit a list of 16-bit tags corresponding to the load-addresses of all known
# overlays.
#
sub EMIT_IMEM_OVERLAY_TAG_LIST
{
    @_ == 1 or croak 'usage: EMIT_IMEM_OVERLAY_TAG_LIST( INDENT )';
    my $indent = shift;

    # Adding dummy tag for invalid overlay.
    $EMIT->print($indent."SHORT(0x0000);\n");

    #
    # We start with the HS overlays (if they exist),
    # and then move to the non-HS overlays.
    #
    _emitImemOverlayTagList('hs_overlay', $indent);
    _emitImemOverlayTagList('non_hs_overlay', $indent);

    return;
}

#
# Emit a list of 16-bit tags corresponding to the load-addresses of all known
# overlays.
#
sub EMIT_PATCHED_IMEM_OVERLAY_TAG_LIST
{
    @_ == 4 or croak 'usage: EMIT_IMEM_OVERLAY_TAG_LIST( PREFIX, INDENT, SECTION, ADDRESSem';
    my $prefix = shift;
    my $indent = shift;
    my $section = shift;
    my $address = shift;

    if (! $Profiles->hsEncryptionSymbols($profileRef))
    {
        return;
    }

    #
    # If BOOT_FROM_HS is enabled, then skip output of 
    # dbg and prod specific tags and only output a
    # single non-specific tag.
    #
    if ($Profiles->bootFromHSEnabled($profileRef))
    {
        if (($prefix eq "patch_dbg_") || ($prefix eq "patch_prd_"))
        {
            return;
        }
    }
    else
    {
        if ($prefix eq "patch_")
        {
            return;       
        }
    }

    $EMIT->print("    .$address : ALIGN(2)\n");
    $EMIT->print("    {\n");
    # Adding dummy tag for invalid overlay.
    $EMIT->print($indent."SHORT(0x0000);\n");

    _emitPatchedImemOverlayTagList($prefix, $indent);
    $EMIT->print("\n    }\n");
    $EMIT->print("    PROVIDE ($section = ADDR(.$address));\n");

    return;
}

#
# Helper function to emit IMEM overlay size list
# @param[in]  ovlSelwrityType  'hs_overlay'     if heavy secure
#                              'non_hs_overlay' if non-heavy secure
#
sub _emitImemOverlaySizeList
{
    @_ == 2 or croak 'usage: _emitImemOverlaySizeList( OVERLAY-SECURITY-TYPE, INDENT )';
    my $ovlSelwrityType = shift;
    my $indent = shift;

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() eq $ovlSelwrityType)
        {
            my $name = $elfSectionRef->getName();

            $EMIT->print($indent."BYTE(SIZEOF(.$name) > 0 ? "          .
                         "((__load_stop_$name + 0xFF) >> 8)"   .
                         " - (__load_start_$name >> 8) : 0);\n");
        }
    }
}

#
# Emit a list of 8-bit sizes (in blocks) corresponding to all known IMEM overlays.
#
sub EMIT_IMEM_OVERLAY_SIZE_LIST
{
    @_ == 1 or croak 'usage: EMIT_IMEM_OVERLAY_SIZE_LIST( INDENT )';
    my $indent = shift;

    # Adding dummy size for invalid overlay.
    $EMIT->print($indent."BYTE(0x00);\n");

    #
    # We start with the HS library overlays (if they exist),
    # then move on to the HS overlays (if they exist),
    # and then move to the non-HS overlays.
    #
    _emitImemOverlaySizeList('hs_overlay', $indent);
    _emitImemOverlaySizeList('non_hs_overlay', $indent);

    return;
}

#
# Helper function to emit IMEM overlay end address list
# @param[in]  ovlSelwrityType  'hs_overlay'     if heavy secure
#                              'non_hs_overlay' if non-heavy secure
#
sub _emitImemOverlayEndAddrList
{
    @_ == 2 or croak 'usage: _emitImemOverlaySizeList( OVERLAY-SECURITY-TYPE, INDENT )';
    my $ovlSelwrityType = shift;
    my $indent = shift;

    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->getOvlSelwrityType() eq $ovlSelwrityType)
        {
            my $name = $elfSectionRef->getName();

            $EMIT->print($indent."LONG(__load_stop_$name);\n");
        }
    }
}

#
# Emit a list of 32-bit addresses corresponding to the end address of all known
# IMEM overlays.
#
# @note This is a special case, as we want to refrain from emitting the symbol
# for the end address list when it is not needed. Therefore, we use this
# function to emit the ENTIRE linker script entry, not just the list of
# addresses. If the entry is not needed, nothing is emitted.
#
sub EMIT_IMEM_OVERLAY_END_ADDR_LD_ENTRY_AND_LIST
{
    @_ == 1 or croak 'usage: EMIT_IMEM_OVERLAY_END_ADDR_LD_ENTRY_AND_LIST( INDENT )';
    my $indent = shift;

    # Lwrrently, the list only needed by SEC2 for IMEM on-demand paging.
    if ($OpMode ne "sec2")
    {
        return;
    }

    #
    # Emit the start of the linker entry.
    # List must be 4 byte aligned as entries are 32 bits.
    #
    $EMIT->print($indent.".imemOvlEndAddr : ALIGN(4)\n");
    $EMIT->print($indent."{\n");

    #
    # Start of the actual list of IMEM overlay end addresses.
    # Use double indentation here inside the list.
    # First entry is dummy address for invalid overlay.
    #
    $EMIT->print($indent.$indent."LONG(0x00);\n");

    #
    # We start with the HS library overlays (if they exist),
    # then move on to the HS overlays (if they exist),
    # and then move to the non-HS overlays.
    #
    _emitImemOverlayEndAddrList('hs_overlay', $indent.$indent);
    _emitImemOverlayEndAddrList('non_hs_overlay', $indent.$indent);
    $EMIT->print("\n");

    #
    # End of IMEM overlay end address list - close list and provide symbol.
    # Use regular indentation from here on out.
    #
    $EMIT->print($indent."}\n");
    $EMIT->print($indent."PROVIDE (__imem_ovl_end_addr = ADDR(.imemOvlEndAddr));\n");

    return;
}

#
# @brief    Emit a list of 8-bit indexes representing MRU overlays.
#
# The doubly-linked list of Most Recently Used (MRU) overlays is represented
# by two arrays holding indexes of previous and next overlays respectively.
# In order to avoid handling corner cases (within list operations) this list
# always contains two nodes (HEAD and TAIL) that are otherwise treated as an
# invalid overlays. On the build using N overlays indexes are as:
#  0    - Invalid overlay index (OVL_IDEX_ILWALID) reused for HEAD node.
#         Pointing to the first resident overlay that is not being paged.
#  1..N - Indexes of used (valid) overlays with direc (1-1) mapping
#  N+1  - Artifitially added overlay to be used as TAIL node
# It is linker script's responsibility to establish init state in which:
#   _overlay_mru_next_list[0] = N + 1
#   _overlay_mru_prev_list[N + 1] = 0
#  while all remaining indexes should point to zero (OVL_IDEX_ILWALID).
#
# @note     For actual usage refer to code in ../uproc/libs/rtos/src/ostask.c
#
sub EMIT_OVERLAY_MRU_LIST
{
    @_ == 3 or croak 'usage: EMIT_OVERLAY_MRU_PREV_LIST(INDENT, IMEM/DMEM)';
    my $indent = shift;
    my $type = shift;
    my $list = shift;
    my $resOvlCount = 0;
    my $ovlCount = 0;
    my $str = $indent."BYTE(0x00);\n";

    if ($type eq 'imem')
    {
        $resOvlCount = 1;
        $ovlCount = scalar @{$OverlaysImem->{ELF_SECTIONS_LIST}};
    }
    elsif ($type eq 'dmem')
    {
        $resOvlCount = 1; # dmem_os_heap
        $ovlCount = scalar @{$OverlaysDmem->{ELF_SECTIONS_LIST}};
    }
    else
    {
        croak 'parameter type must be imem or dmem.';
    }

    # Emit HEAD node...
    if ($list eq 'prev')
    {
        $EMIT->print($str);
    }
    elsif ($list eq 'next')
    {
        # ... and link it to TAIL node ...
        $EMIT->print($indent."BYTE(".($resOvlCount + $ovlCount).");\n");
    }
    else
    {
        croak 'parameter type must be prev or next.';
    }

    # Emit N valid overlays + TAIL node.
    $str = $str x ($resOvlCount + $ovlCount);
    $EMIT->print($str);

    return;
}

#
# Helper function to get the number of HS overlays, including HS library overlays
#
sub _getNumHsOverlays
{
    my $count = 0;
    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        if ($elfSectionRef->isFlagSet(':HS_OVERLAY'))
        {
            $count++;
        }
    }
    return $count;
}

#
# Emit the number of HS overlays, including HS library overlays
#
sub EMIT_NUM_HS_OVERLAYS
{
    my $count = _getNumHsOverlays();
    $EMIT->print("$count");
    return;
}

#
# Emit the number of IMEM overlays
#
sub EMIT_NUM_IMEM_OVERLAYS
{
    my $count = 0;

    # Add up additional IMEM overlays from the build
    foreach my $elfSectionRef (@{$OverlaysImem->{ELF_SECTIONS_LIST}})
    {
        $count++;
    }
    $EMIT->print("$count");

    return;
}

#
# Emit the number of DMEM overlays
#
sub EMIT_NUM_DMEM_OVERLAYS
{
    my $count = 1; # OS overlay is always present

    # Add up additional DMEM overlays from the build
    foreach my $elfSectionRef (@{$OverlaysDmem->{ELF_SECTIONS_LIST}})
    {
        $count++;
    }
    $EMIT->print("$count");

    return;
}

#
# Emit the ISR stack size, that is determined by a profile variable.
#
sub EMIT_ISR_STACK_SIZE
{
    my $self = $EMIT_OBJREF;
    my $isrStackSize = $Profiles->flcnIsrStackSize($profileRef);

    if (! $isrStackSize)
    {
        $self->error("ISR_STACK not defined or set to 0\n");
    }

    if (($isrStackSize % 4) != 0)
    {
        $self->error("ISR_STACK must be multiple of 4\n");
    }

    $EMIT->print($isrStackSize);

    return;
}

#
# Emit the ESR stack size, that is determined by a profile variable.
#
# ESR - Exception Service Routine
#
sub EMIT_ESR_STACK_SIZE
{
    my $self = $EMIT_OBJREF;
    my $esrStackSize = $Profiles->flcnEsrStackSize($profileRef);

    if (! $esrStackSize)
    {
        $self->error("ESR_STACK not defined or set to 0\n");
    }

    if (($esrStackSize % 4) != 0)
    {
        $self->error("ESR_STACK must be multiple of 4\n");
    }

    $EMIT->print($esrStackSize);

    return;
}

#
# Emit all the chip/feature specific regions for the linker script
#
sub EMIT_BUILD_SPECIFIC_REGIONS
{
    @_ == 1 or croak 'usage: EMIT_PROVIDE_BUILD_SPECIFIC_REGIONS( INDENT )';
    my $indent = shift;

    my $self = $EMIT_OBJREF;

    if (! $Profiles->dmemVaSize($profileRef))
    {
        # Profile without DMEM VA support
        $EMIT->printf($indent."PROVIDE (__end_physical_dmem = LENGTH(DATA));\n");
    }
    else
    {
        # Profile with DMEM VA support
        $EMIT->printf($indent."PROVIDE (__end_physical_dmem = 0x%3X);\n",
                      $Profiles->dmemVaBound($profileRef));
    }

    if ($Profiles->freeableHeapSize($profileRef))
    {
        $EMIT->printf($indent."__freeable_heap       = __isr_stack_end;\n");
        $EMIT->printf($indent."__freeable_heap_size  = 0x%3X;\n", $Profiles->freeableHeapSize($profileRef));
        $EMIT->printf($indent."__freeable_heap_end   = __freeable_heap + __freeable_heap_size;\n");
        $EMIT->printf($indent."__heap                = __freeable_heap_end;\n");
    }
    else
    {
        $EMIT->printf($indent."PROVIDE (__heap             = __isr_stack_end);\n");
    }

    $EMIT->printf($indent."PROVIDE (__resident_data_end   = __heap);\n");

    if (defined $ELW{'EMEM_SUPPORTED'} &&
                $ELW{'EMEM_SUPPORTED'} eq 'true')
    {
        if ((! $Profiles->dmemVaSize($profileRef)) || (! $Profiles->ememPhysicalSize($profileRef)))
        {
            croak 'DMEM_VA and EMEM must be defined in Profiles.pm!';
        }

        # Get the OS heap size = All of physically addressable DMEM - stack heap size - resident data size
        $EMIT->printf($indent."PROVIDE (__heap_size  = __end_physical_dmem - __resident_data_end);\n");

        $EMIT->printf($indent."PROVIDE (__heap_end   = __heap + __heap_size);\n");

        # When EMEM is enabled, __start_emem = end of DMEM VA space
        $EMIT->printf($indent."PROVIDE (__start_emem = 0x%3X);\n", $Profiles->dmemVaSize($profileRef));
        $EMIT->printf($indent."PROVIDE (__end_emem   = LENGTH(DATA));\n");

        # NOTE: There is no PROVIDE here as that can cause the linker to fail
        #       The reason is a lack of reference to __rm_share_size in the code
        $EMIT->printf($indent."__rm_share_start      = LENGTH(DATA) - __rm_share_size;\n");
    }
    else
    {
        # Get the OS heap size = All of physically addressable DMEM - stack heap size - resident data size - rm share size
        $EMIT->printf($indent."PROVIDE (__heap_size = __end_physical_dmem - __resident_data_end" .
                        " - __rm_share_size);\n");

        $EMIT->printf($indent."PROVIDE (__heap_end  = __heap + __heap_size);\n");

        # NOTE: There is no PROVIDE here as that can cause the linker to fail
        #       The reason is a lack of reference to __rm_share_size in the code
        $EMIT->printf($indent."__rm_share_start     = __end_physical_dmem - __rm_share_size;\n");
    }

    return;
}

# init()
sub init {
    (@_ == 1) or croak 'usage: obj->init( )';

    my $self = shift;

    createOutputSectionsForOverlaysImem();

    if ($Profiles->dmemVaSize($profileRef))
    {
        #
        # Additional DMEM overlays other than OS are only for builds with
        # DMEM VA support
        #
        createOutputSectionsForOverlaysDmem();
    }
}

sub new {
    (@_ == 8) or croak 'usage: GenLdscriptFalcon->new( NAME, PROFILE-NAME, LW-ROOT, OVERLAYSIMEM-FILE, OVERLAYSDMEM-FILE, VERBOSE, DMEM_OVL_VARS_LONG, )';

    my ($class, $name, $profile, $lwroot, $opt_overlaysimem_file, $opt_overlaysdmem_file, $verbose, $dmem_ovl_vars_long) = @_;

    my ($buildName);

    $profile =~ /^(\w+)-(\w+)/;  # pmu-gf11x ->pmu, gf11x;   dpu-v0201 -> dpu, v0201
                                 # fbflcn-tu10x-gddr -> fbflcn, tu10x (gddr is uased as config not as profile)
    ($OpMode, $buildName) = ($1, $2);

    $Profiles = Profiles->new($name);
    $profileRef = $Profiles->grpItemRef($buildName);

    #
    # Instantiate a new OverlaysImem object to bring-in all of the overlays defined in
    # OverlaysImem.pm. OverlaysImem is a Group object where each item represents an
    # overlay. Then, using the information that was loaded from the .pm file,
    # create ELF output-sections for each of the overlays.
    #
    $OverlaysImem = Overlays->new($name, 'imem', $opt_overlaysimem_file);

    if ($Profiles->dmemVaSize($profileRef))
    {
        #
        # If DMEM VA is supported, instantiate a new OverlaysDmem object to
        # bring in all of the overlays defined in OverlaysDmem.pm. OverlaysDmem
        # is a Group object where each item represents an overlay. Then, using
        # the information that was loaded from the .pm file, create ELF output
        # sections for each of the overlays.
        #
        $OverlaysDmem = Overlays->new($name, 'dmem', $opt_overlaysdmem_file);
    }

    # Instantiate a new Tasks object for accessing TasksImpl.pm methods
    $Tasks = Tasks->new($name,
                        $profile,
                        $lwroot,
                        $verbose);

    if ($dmem_ovl_vars_long)
    {
        $DmemOvlVarsType = 'LONG';
    }
    else
    {
        $DmemOvlVarsType = 'SHORT';
    }

    my $self = {
        NAME                =>  $name,
        PROFILE_NAME        =>  $profile,
        LW_ROOT             =>  $lwroot,
        BUILD               =>  $buildName,
        OP_MODE             =>  $OpMode,
        VERBOSE             =>  $verbose,
        PROFILE_REF         =>  $profileRef,
        IMEM_OVERLAYS_REF   =>  $OverlaysImem,
        DMEM_OVERLAYS_REF   =>  $OverlaysDmem,
        TASKS_REF           =>  $Tasks,
    };

    return bless $self, $class;
}

1;
