##!/usr/bin/perl -w

#
# Copyright 2014 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

package Table;

use strict;
use Carp;

use cpan::List::MoreUtils 'first_index';

#class Table
#   {
#
#   ###############################################################################################################
#   A simple class the parses a few lines of data(string) into a table.
#   Few assumptions:
#   * The table is assumed to have a header whose content class is the same as table data (can be modified/enhanced).
#   * The contents of the string are alpha-numeric(can be expaned to other type of charactes easily) separated by one or more white space
#   * If any table entry is missing, it is just warned an contiues. To an entry at (x,y) will NOT be mapped to (x,y) in the parsed table.
#     The next valid entry is used to fill its spot and the last few entries in that row(tuple) will be empty/missing.
#   * Also this class uses a SimpleLogger::mylog function to print logs. Ususally the main module would have already included this
#   ###############################################################################################################
#
#   ATTRIBUTES:
#   string HEADER[];                                # header entries
#   string DATA[][] ;                               # table entries in a 2D array
#   optional string KEY;                            # the column that can be used as the primary key (see getEntry() for how this is used)
#
#   METHODS:
#   Table new();
#   string getEntry(row, col);                      # Note: The value of row can be either indexes of the entry or (primary_id, col);
#   void parseTable(string data[][]);               # Parses the contents of the table
#   void setKey(string Key);                        # Sets the key
#   void getColumn(string header);                  # Returns the column with the selected header
#   void printTable();                              # Prints the contest of the table
#   void sort(string col);                          # Sorts the table based on the column specified
#   }

# creates an empty table
sub new {
    ( @_ == 1 ) or croak 'usage: Table->new()';

    my $class = shift;
    my $self  = {
        # header is a list of strings
        HEADER      => [],
        # contents of the table stored in a 2D array
        DATA        => [],
        # the column to be used a primary key. If value not set, getEntry works
        # only on row and column index.
        KEY         => undef,
        KEY_INDEX   => undef
    };
    bless $self, $class;
    return $self;
}

# sub-routine to get table entry. Supports $table->getEntry(2, 3) and $table->getEntry(primary_key, 'col')
sub getEntry {
    (@_ == 3) or croak 'Usage $obj->getEntry( ROW-INDEX, COLUMN-INDEX)';

    my $self = shift;

    # get row and column parameters
    my $row = shift or die 'Missing arguments need row and cloumn';
    my $col = shift or die 'Missing argument need column as well';

    # check if the row entry is a string(key)
    if ($row ^ $row) {
        # check if col is also a string and key is defined
        if (($col ^ $col) && (defined $self->{KEY})) {
            $col =~ s/[\"\']//;
            my $columnIndex = first_index {/$col/i} @{$self->{HEADER}};
            if ($columnIndex == -1) {
                warn "Invalid column passed for search";
                return;
            }

            # find the row with the matching entry
            foreach (@{$self->{DATA}})
            {
                if ($_->[$self->{KEY_INDEX}] eq $row) {
                    return $_->[$columnIndex];
                }
            }
            # entry not found
            return undef;
        }
    }
    else
    {
        return $self->{DATA}[$row]->[$col];
    }
}

# returns entries of a colums. Parameter can either be name of the column(header) or index
sub getColumn {
    ( @_ == 2 ) or croak 'usage: obj->getColumn( COLUMN-HEADER or COLUMN-INDEX )';

    my ($self, $col) = (shift, shift);

    # if $col is string, save column index to $col
    if ($col ^ $col) {
        $col =~ s/[\"\']//;
        $col = first_index {/$col/i} @{$self->{HEADER}};
    }

    my @result;
    foreach (@{$self->{DATA}})
    {
        push @result, $_->[$col];
    }

    return \@result;
}

# sets the key
sub setKey {
    ( @_ == 2 ) or croak 'usage: obj->setKey( KEY-HEADER )';

    my $self = shift;
    my $key = shift;

    # remove quotes
    $key =~ s/[\"\']//;
    $self->{KEY_INDEX} = first_index {/$key/i} @{$self->{HEADER}};
    if ($self->{KEY_INDEX} == -1) {
        warn "Key not found in table\n";
        return;
    }
    $self->{KEY} = @{$self->{HEADER}}[$self->{KEY_INDEX}];
}

sub parseTable {
    (@_ == 2) or croak 'Usage $obj->parseTable( TEXT-LIST-REF )';

    my ($self, $tableTextRef) = @_;

    # return if nothing to process
    return if not defined $tableTextRef->[0];

    # extract header
    SimpleLogger::logDebug("Parsing Table header :\n", $tableTextRef->[0]);
    foreach (split(/\s+/,delete $tableTextRef->[0]))
    {
        if ($_ =~ /([a-zA-Z0-9_.]+)/) {
            push @{$self->{HEADER}}, $1;
        }
    }

    # parse rows
    foreach my $rowData (@{$tableTextRef})
    {
        next if(not defined $rowData);
        my @rowEntry;
        foreach my $word (split(/\s+/, $rowData))
        {
            if ($word =~ /([a-zA-Z0-9_.]+)/) {
                push @rowEntry, $1;
            }
        }
        if (scalar @rowEntry != scalar @{$self->{HEADER}}) {
            SimpleLogger::logDebug('Awwww, tricky table. Row missing ',
                (scalar @{$self->{HEADER}} - scalar(@rowEntry)),
                ' entries. Skipping this row....', "$rowData\n");
        }
        else
        {
            push @{$self->{DATA}}, \@rowEntry;
        }
    }
}

sub printTable {
    (@_ >= 1) or croak 'Usage $obj->printTable( OPTIONAL-FILE-HANDLE )';

    my $self = shift;
    my $fh = *STDOUT;
    if ($_[0]) {
        $fh = shift;
    }

    # list to hold the size of each column
    my @lengths = (0) x scalar @{$self->{HEADER}};
    my $lwrrentMax;

    # for each row including the header
    foreach my $row (@{$self->{DATA}})
    {
        # for each entry in the row
        for (0 .. $#{$self->{HEADER}})
        {
            # if current entry is of larger size than previous max
            $lwrrentMax = length(${$self->{HEADER}}[$_]) > length($row->[$_]) ?
                length(${$self->{HEADER}}[$_]) : length($row->[$_]);

            # replace the max width of column
            if ($lwrrentMax > $lengths[$_]) {
                $lengths[$_] = $lwrrentMax;
            }
        }
    }

    # to find how many '-'s to print below the header
    my $sum = 0;
    foreach (@lengths)
    {
        $sum += $_ + 8;
    }

    # print separator
    printf $fh "\n%s\n", '-' x $sum;

    # print header and callwlate the value of sum
    for (0 .. $#{$self->{HEADER}})
    {
        printf $fh "%-$lengths[$_]s\t", ${$self->{HEADER}}[$_];
    }

    # print separator
    printf $fh "\n%s\n", '-' x $sum;

    # print table entries
    foreach my $row (@{$self->{DATA}})
    {
        # print entries of each row
        for (0 .. $#{$row})
        {
            printf $fh "%-$lengths[$_]s\t", @{$row}[$_];
        }
        print $fh "\n";
    }

    # print separator at the end
    printf $fh "%s\n\n", '-' x $sum;
}

sub sort {
    (@_ == 1) or croak 'Usage $obj->sort( COLUMN-HEADER )';

    my $self = shift;

    # simply return for empty table
    return if not defined $self->{DATA}->[0];
    # col based on which to sort
    my $col = shift;
    my $idx = first_index {/$col/i} @{$self->{HEADER}};

    # If specified column is string, do string compare
    if ($self->{DATA}->[0][$idx] ^ $self->{DATA}->[0][$idx]) {
        @{$self->{DATA}} = sort {($a->[$idx]) cmp($b->[$idx])} @{$self->{DATA}};
    }
    # else do number comparison
    else {
        @{$self->{DATA}} = sort {($a->[$idx]) <=> ($b->[$idx])} @{$self->{DATA}};
    }
}

1;
