#
# Copyright 2014-2015 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# This module defines a perl class to represent ELF keep-sections. ELF keep
# sections instruct the linker on which sections to keep regardless of whether
# they are referenced in code or not.
#

package ElfKeepSectionImem;

#
# Create a new ELF Keep Section object.
#
# @param[in]  sectionname Section name that comprises the keep-section.
#
# @return
#     Reference to the newly instantiated object.
#
sub new
{
    my $class = shift;
    my $self  =
    {
        _sections => [@_] ,
    };
    return bless $self, $class;
}

#
# Get the keep-section description in offical linker-script syntax. The
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

    $string .= 'KEEP(*';
    $string .= '(';
    $string .= join(' ', @{$self->{_sections}});
    $string .= '))';

    return $string;
}

1;

