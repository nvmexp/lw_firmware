#!/usr/bin/perl -w

#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#
#
# Uses the Readelf module to read in the ELF file and dump easy to parse symbol
# information to a file.
#
# Usage: symdump.pl <elf_file> <dt_file>
#

use File::Basename;

# add the current directory to @INC to ensure that the Parse module is found
BEGIN 
{
    unshift @INC, dirname $0      if ($0 && -f $0);
}

use IO::File;
use Parse::Readelf;
use Getopt::Long;

my $version = "1.0";
my $acl     = 0;

# input and output files specified on the command-line
my $num_args = $#ARGV + 1;
die "Usage: symdump.pl <elf_file> <dt_file> [-acl cl#]\n" unless $num_args >= 2;
my $elf_file = $ARGV[0];
my $dt_file  = $ARGV[1];
my $falcPath = "";

GetOptions('acl=i'          => \$acl,
           "falcon-tools=s" => \$falcPath);

# add the falcon-tools bin directory to the PATH
$ELW{'PATH'} = "$falcPath/bin";

# use Readelf to parse the ELF file and load into perl objects
my $line_info  = new Parse::Readelf::Debug::Line($elf_file);
my $debug_info = new Parse::Readelf::Debug::Info($elf_file, $line_info);

my $fdes = new IO::File($dt_file, "w") or die "Cannot open file for writing: $!";
print $fdes "#symdump v$version\n";
print $fdes "#$acl\n";
print $fdes "\n";

# dump information on structures, typedefs, and unions
@layout_types = qw(struct typedef union);
@ids = $debug_info->item_ids_matching('',
            '^DW_TAG_(?:'.
            join('|', @layout_types).
            ')'
               );

my %types;
foreach (@ids)
{
    my @typeLayout = $debug_info->structure_layout($_);
    if (@typeLayout)
    {
        # have we seen this before?
        my $name = $typeLayout[0]->[1];
        if (!exists $types{$name})
        {
            $types{$name} = \@typeLayout;
        }
    }
}

foreach my $type (keys %types)
{
    my $typeLayout = $types{$type};
    my $name       = $$typeLayout[0]->[1];
    my $size       = $$typeLayout[0]->[3];

    print $fdes "<type name=\"$name\" size=$size>\n";

    foreach my $layout (@$typeLayout)
    {
        my $level  = $layout->[0];
        my $name   = $layout->[1];
        my $type   = $layout->[2];
        my $size   = $layout->[3];
        my $offset = $layout->[5];
        print $fdes "<item name=\"$name\" offset=$offset level=$level type=\"$type\" size=$size/>\n";
    }

    print $fdes "</type>\n\n";
}

