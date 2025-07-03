#
# Copyright 2013-2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# This module defines a perl class to represent ELF output-sections. The linker
# takes the output-sections defined in the linker-script and combines them to
# create the final ELF image.
#
package ElfOutputSectionImem;

use File::Basename;

BEGIN
{
    # add the dir of this file to @INC path
    # for using 'ElfInputSectionImem' and other modules
    unshift @INC, dirname __FILE__;
}

use ElfInputSectionImem;
use ElfKeepSectionImem;

#
# Create a new ELF Output Section object.
#
# @param[in]  name  Name to associate with the output-section.
#
# @return
#     Reference to the newly instantiated object.
#
sub new
{
    my $class = shift;
    my $self  =
    {
        _name          => shift,
        _inputSections => []   ,
        _keepSections  => []   ,
        _flags         => ''   ,
    };

    bless $self, $class;

    $self->addDefaultInputSection();

    return $self;
}

#
# Add/map an input-section into this output-section. The input-section is
# described by a section name and an optional filename or file glob.
#
# @param[in]  sectionName  Section name the represents the input-section.
# @param[in]  filename
#     Optional filename or file glob to defined the file(s) that makeup the
#     input-section. If unspecified, '*' is used to match all object files
#     in the output directory.
#
sub addInputSection
{
    my ($self, $sectionName, $filename) = @_;

    $filename = '*'
        if (! defined $filename);

    push(@{$self->{_inputSections}},
         new ElfInputSectionImem($filename, $sectionName));
}

#
# Add an input-section to this output-section that instructs the linker to map
# all sections matching the name of this output-section into this section.
#
sub addDefaultInputSection
{
    my $self = shift;
    $self->addInputSection(".$self->{_name}.*", '*');
}

#
# Add/map an keep-section into this output-section. The keep-section is
# described by a section name.
#
# @param[in]  sectionName  Section name the represents the keep-section.
#
sub addKeepSection
{
    my ($self, $sectionName) = @_;

    push(@{$self->{_keepSections}},
         new ElfKeepSectionImem($sectionName));
}

#
# Force a specific alignment for this section.
#
# @param[in]  alignment  Desired section alignment.
#
sub setAlignment
{
    my ($self, $alignment) = @_;
    $self->{_alignment} = $alignment;
}

#
# Set an optional pure-text description for this section.
#
# @param[in]  description  Desired section description.
#
sub setDescription
{
    my ($self, $description) = @_;
    $self->{_textDescription} = $description;
}

#
# Set optional flags for this section
#
# @param[in]  flags  Desired flags.
#
sub setFlags
{
    my ($self, $flags) = @_;
    $self->{_flags} = $flags;
}

#
# Get the pure-text description for this section (if defined).
#
# @return
#     Text description of this section if defined; 'undef' if undefined.
#
sub getDescription
{
    my $self = shift;
    return $self->{_textDescription};
}

#
# Return if the specified flag is set or not
# @param[in]  flags  Flag to check for
#
sub isFlagSet
{
    my $self = shift;
    my $flag = shift;

    return ($self->{_flags}.':') =~ m/$flag:/;
}

#
# Return the security type of the overlay
#
# @return
#     'non_hs_overlay'  if non-heavy secure overlay
#     'hs_overlay'      if heavy secure overlay
#
sub getOvlSelwrityType
{
    my $self = shift;

    if (!$self->isFlagSet(':HS_OVERLAY'))
    {
        return 'non_hs_overlay';
    }
    return 'hs_overlay';
}

#
# Checks if the overlay is resident
#
sub isOvlResident
{
    my $self = shift;

    if ($self->isFlagSet(':RESIDENT'))
    {
        return 1;
    }
    return 0;
}

#
# Get the name of this output-section.
#
# @return
#     Name of the output-section. Same name as provided to the object
#     constructor.
#
sub getName
{
    my $self = shift;
    return $self->{_name};
}

#
# Force an explicit output-section description. Must be provided in offical
# linker-script syntax. When a description is forced, any input-sections that
# are mapped into this output-section (via addInputSection) are ignored. This
# method should only be used under very special cirlwmstances.
#
# @param[in]  description  Desired output-section description.
#
sub setSectionDescription
{
    my ($self, $description) = @_;
    $self->{_description} = $description;
}

#
# Get the output-section description in offical linker-script syntax. The
# returned description may be used as a section-command in the linker-script.
#
# @return
#     A string containing the section description.
#
sub getSectionDescription
{
    my $self = shift;
    my $forceSizeAlign = 0;

    return $self->{_description} if defined $self->{_description};

    my $description = '.' . $self->{_name} . ' : ';

    # Align the overlay to 256 if LS SIG GRP is defined
    if ((defined $self->{_lssiggroup}) &&
        ($self->{_lssiggroup} > 0))
    {
        # Since 256 is max alignment possible, its ok to override the alignment if available
        $self->setAlignment(256);
        $forceSizeAlign = 1;
    }

    if (defined $self->{_alignment})
    {
        $description .= "ALIGN($self->{_alignment})";
    }
    $description .= "\n";

    $description .= '{';
    $description .= "\n";

    foreach my $section (@{$self->{_keepSections}})
    {
        $description .= '    ';
        $description .= $section->getSectionDescription();
        $description .= "\n";
    }

    foreach my $section (@{$self->{_inputSections}})
    {
        $description .= '    ';
        $description .= $section->getSectionDescription();
        $description .= "\n";
    }

    if (defined $self->{_alignment})
    {
        # HS expects the end of the overlay to also be aligned
        if (($self->isFlagSet(':HS_OVERLAY')) ||
            ($forceSizeAlign == 1))
        {
            $description .= '    ';
            $description .= ". = ALIGN($self->{_alignment});\n";
        }
    }

    $description .= '}';

    return $description;
}

#
# Set the LS signature group for an overlay
#
# @param[in] group Group ID
#
sub setLsSigGroup
{
    my ($self, $group) = @_;
    $self->{_lssiggroup} = $group;
}

#
# Get the LS signature group if defined else return 0
#
# @return Group ID
#
sub getLsSigGroup
{
    my $self = shift;
    return ($self->{_lssiggroup} || 0);
}


1;


