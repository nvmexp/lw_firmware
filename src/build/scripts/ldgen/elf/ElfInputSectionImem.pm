#
# Copyright 2013-2015 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# This module defines a perl class to represent ELF input-sections. ELF input
# sections instruct the linker on how to map input files (object files, libs,
# etc.) into your memory layout.
#

package ElfInputSectionImem;

#
# Create a new ELF Input Section IMEM object.
#
# @param[in]  filename  Filename for file glob for the input-section.
# @param[in]  ...
#     Optional list of section names that comprise the input-section.
#
# @return
#     Reference to the newly instantiated object.
#
sub new
{
    my $class = shift;
    my $self  =
    {
        _filename => shift,
        _sections => [@_] ,
    };
    return bless $self, $class;
}

#
# Get the input-section description in offical linker-script syntax. The
# returned description may be used as an output-section-command in an
# output-section description.
#
# @return
#     A string containing the section description.
#
sub getSectionDescription
{
    my $self   = shift;
    my $string = "";

    $string .= $self->{_filename};
    $string .= '(';
    $string .= join(' ', @{$self->{_sections}});
    $string .= ')';

    return $string;
}

1;

