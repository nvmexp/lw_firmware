##!/usr/bin/perl -w

#
# Copyright 2014 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

package ReadElf;

use strict;
use Carp;
use List::Util 'first';
use Exporter 'import';

use cpan::List::MoreUtils 'first_index';
use ovlCheck::Table;
use ovlCheck::SimpleLogger;

our @EXPORT = qw(parseReadelfFile getSectionTableByChangelist);

# given a changelist number, returns a hash with all the sections tables in the
# readelf file
# used by storm tool
sub getSectionTableByChangelist {
    ( @_ >= 1 ) or croak 'usage: getSectionTableByChangelist( CHANGELIST-NUMBER, OPTIONAL-COMPONENT )';
    
    my $changelist = shift;
    my $component;    # whether pmu or dpu
    $_ ? $component = $_ : $component = 'pmu';

    # get changelist summary
    my @diffLog = `p4 describe -s $changelist`;

    # get all readelf files in the changelist
    my @diffFiles = grep { /$component.+readelf/ } @diffLog;

    # hash that will be returned
    my %sectionTables;
    foreach (@diffFiles)
    {
        # extract file info
        my ( $ellipsis, $filePath, $fileRevision, $edit ) = split /[ #]/, $_;

        # get ubuild name
        my $ucode = ( split /[\._]/, $filePath )[-2];
        my $table;

        if ( $edit ne 'delete' ) {
            SimpleLogger::logInfo("\nParsing file $filePath#$fileRevision \n");
            $table = parseReadelfFile(`p4 print -q $filePath#$fileRevision`);
        }
        else {
            $table = new Table();
        }
        $sectionTables{$ucode} = $table;
    }

    return %sectionTables;
}

# This function parses the readelf file and returns the section table
sub parseReadelfFile {
    ( @_ >= 1 ) or croak 'usage: parseReadelfFile( TEXT-ARRAY )';
    
    # read file contents into array
    my $file_String = \@_;

    # get number of sections in array
    my $match = first {/Number of section headers:/} @{$file_String};
    $match =~ /([0-9]+)/;
    my $numberofSections = $1
      or die('Couldn\'t find section table information in readelf file');

    SimpleLogger::logInfo("Found section table with $numberofSections entries\n");

    # copy section table contents
    my $sectionTableIndex = (first_index {/Section Headers:/} @{$file_String}) + 1;

    my @sectionTableText =
        @{$file_String}[$sectionTableIndex .. $sectionTableIndex + $numberofSections ];

    my $sectionTable = new Table();
    $sectionTable->parseTable(\@sectionTableText);
    $sectionTable->setKey('Name');
    return $sectionTable;
}

1;
