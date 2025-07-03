#
# Copyright 2015 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# This module defines a perl class to represent ELF output-sections. The linker
# takes the output-sections defined in the linker-script and combines them to
# create the final ELF image.
#
package ElfOutputSectionDmem;

use File::Basename;

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
    my $name  = shift;
    my $inputSection = "*(.$name.*)";
    my $self  =
    {
        _name         => $name,
        _inputSection => $inputSection,
        _flags        => ''   ,
    };

    bless $self, $class;

    return $self;
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
# Set the max size of this section in blocks.
#
# @param[in]  maxSizeBlocks  Max size in blocks.
#
sub setMaxSizeBlocks
{
    my ($self, $maxSizeBlocks) = @_;
    $self->{_maxSizeBlocks} = $maxSizeBlocks;
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
# Get the max size of this section in blocks.
#
# @return
#     Max size in blocks.
#
sub getMaxSizeBlocks
{
    my $self = shift;
    return $self->{_maxSizeBlocks};
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
    my $residentSection = shift;
    return $self->{_description} if defined $self->{_description};

    my $description = '.' . $self->{_name};
    if ($residentSection eq 0)
    {
        #
        # Place the section at the current DMEM overlay location. It will be
        # modified at the end of every DMEM overlay placement
        #
        $description .= " _lwr_dmem_va_location : ";
    }
    else
    {
        $description .= " : ";
    }

    if (defined $self->{_alignment})
    {
        $description .= "ALIGN($self->{_alignment})";
    }
    $description .= "\n";

    # Add static input sections
    $description .= '{' . "\n" . '    ' . $self->{_inputSection}. "\n" . '}' . "\n";

    return $description;
}
1;


