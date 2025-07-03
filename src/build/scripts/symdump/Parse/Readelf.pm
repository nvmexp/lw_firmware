package Parse::Readelf;

# Author, Copyright and License: see end of file

=head1 NAME

Parse::Readelf - handle readelf's output with a class

=head1 SYNOPSIS

  use Parse::Readelf;

  my $readelf_data = new Parse::Readelf($exelwtable);
  $readelf_data->print_structure_layout($re_identifier, 1);

=head1 ABSTRACT

Parse::Readelf parses (some of) the output of C<readelf> and stores
its interesting details in some objects to ease access.

At the moment only a very limited access to the structure layout of
data types and variables is supported.

=head1 DESCRIPTION

Normally an object of this class is constructed with the file name of
an object file to be parsed.  Upon construction the file is analysed
and all relevant information about its debug info section is stored
inside of the object or one of its subobjects.  This information can
be accessed afterwards using a bunch of getter methods, see
L</"METHODS"> for details.

This is BETA software, use at your own risk.

at the moment only information regarding the binary arrangement of
variables (Structure Layout) is supported (and that is regularly used
at my company, so the worst bugs should by found by now).  Other data
is ignored for now.

Lwrrently only output for B<Dwarf version 2> is supported.  Please
contact the author for other versions and provide some example
C<readelf> outputs.

=cut

#########################################################################

use 5.006001;
use strict;
use warnings;
use Carp;

our $VERSION = '0.09';

use Parse::Readelf::Debug::Line;
use Parse::Readelf::Debug::Info;

#########################################################################

=head1 EXPORT

Nothing is exported by default as it's normally not needed to modify
the following variable:

This module exports nothing directly, it should be accessed via its
methods only.

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

require Exporter;

our @ISA = qw(Exporter);
our @EXPORT = qw();
our %EXPORT_TAGS = ( 'all' => [ qw(@structure_layout_types) ] );
our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

#########################################################################

=head2 I<@structure_layout_types>

is a list of the types that can be printed in a structure layout.  Its
elements are basically the tag identifieres from C<readelf>'s output
without the prefix B<DW_TAG_>.

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

our @structure_layout_types = qw(class
				 enumerat
				 member
				 structure
				 subrange
				 typedef
				 union
				 variable);

#########################################################################

=head2 new - get readelf's output into an object

    $readelf_data = new Parse::Readelf($file_name);

=head3 example:

    $readelf_data1 = new Parse::Readelf('program');
    $readelf_data2 = new Parse::Readelf('module.o');

=head3 parameters:

    $file_name          name of exelwtable or object file

=head3 description:

    This method parses the output of several C<readelf> commands and
    stores its interesting details internally to be accessed later by
    getter methods described below.

=head3 returns:

    The method returns the blessed Parse::Readelf object or an
    exception in case of an error.

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
sub new($$)
{
    my $this = shift;
    my $class = ref($this) || $this;
    my ($file_name) = @_;
    my %self = (line_info => undef,
		debug_info => undef);
    local $_;

    # checks:
    if (! $file_name)
    { croak 'bad call to new of ', __PACKAGE__ }
    if (ref($this))
    { carp 'cloning of a ', __PACKAGE__, ' object is not supported' }
    if (! -f $file_name)
    { croak __PACKAGE__, " can't find ", $file_name }

    # parse all supported readelf sections:
    $self{line_info} = new Parse::Readelf::Debug::Line($file_name);
    $self{debug_info} =
	new Parse::Readelf::Debug::Info($file_name, $self{line_info});

    # last consistency check:
    confess 'incomplete constructed object in ', __PACKAGE__
	unless defined $self{line_info} and defined $self{debug_info};

    bless \%self, $class;
}

#########################################################################

=head2 print_structure_layout - print structure layout of variables/types

    $readelf_data->print_structure_layout($re_name [, $print_location]);

=head3 example:

    $readelf_data->print_structure_layout('_t$');
    $readelf_data->print_structure_layout('_t$', 1);

=head3 parameters:

    $re_name            reg. exp. matching name of variable or data type
    $print_location     optional flag to print location with every definition

=head3 description:

    This method prints the structure layout of one or more variables
    or data types that match the regular expression for their name.
    If the optional parameter $print_location is true, each line also
    contains source location information, if availablble.

=head3 returns:

    nothing

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
sub print_structure_layout($$;$)
{
    my $this = shift;
    my ($re_name, $print_location) = @_;
    local $_;

    # get item IDs for all matching items:
    my @ids =
	$this->{debug_info}
	    ->item_ids_matching($re_name,
				'^DW_TAG_(?:'.
				join('|', @structure_layout_types).
				')'
			       );

    # get layout for each item
    my @layouts = ();
    foreach (@ids)
    { push @layouts, $this->{debug_info}->structure_layout($_) }

    # get maximum width for each field:
    my ($level_name_width, $offset_width, $bit_offset_width, $type_width) =
	(1, 1, 0, 1);
    foreach (@layouts)
    {
	my $width = length($_->[1]) + 2 * $_->[0];
	$level_name_width = $width if $level_name_width < $width;
        $offset_width = length($_->[5]) if $offset_width < length($_->[5]);
        $bit_offset_width = length($_->[7]) + 1
	    if defined $_->[7]  and  $bit_offset_width < length($_->[7]) + 1;
	$width = $_->[2] ne '' ? length($_->[2]) + 3 : 2;
	$width += length($_->[3]);
	$width += length(defined $_->[6]) + 4 if defined $_->[6];
	$type_width = $width if $width > $type_width;
    }

    # now print items:
    printf("%s   %-*s   %-*s   %s\n",
	   substr('OFFSET  ', 0 , $offset_width + $bit_offset_width),
	   $level_name_width, 'STRUCTURE',
	   $type_width, 'TYPE (SIZE)',
	   ($print_location ? 'SOURCE LOCATION' : ''));
    foreach (@layouts)
    {
	my $type_size = sprintf("%s(%s%d)",
				($_->[2] ne '' ? $_->[2].' ' : ''),
				(defined $_->[6] ? $_->[6].' in ' : ''),
				$_->[3]);
	my $location = '';
	if ($print_location  and  defined $_->[4])
	{
	    $location =
		$this->{line_info}->file($_->[4]->[0], $_->[4]->[1])
		    . ':' . $_->[4]->[2];
	}
	printf("%0*d%-*s   %-*s   %-*s   %s\n",
	       $offset_width, $_->[5],
	       $bit_offset_width, (defined $_->[7] ? '.'.$_->[7] : ''),
	       $level_name_width, ('  ' x $_->[0]) . $_->[1],
	       $type_width, $type_size,
	       $location);
    }
}

1;

#########################################################################

__END__

=head1 KNOWN BUGS

Did I mentioned that this is Beta code?

Only Dwarf version 2 is supported.  Please contact the author for
other versions and provide some example C<readelf> outputs.

This has only be tested in a Unix like environment, namely Linux and
Solaris.

=head1 SEE ALSO

L<Parse::Readelf::Debug::Info>, L<Parse::Readelf::Debug::Line> and the
C<readelf> man page

=head1 AUTHOR

Thomas Dorner, E<lt>dorner (AT) pause.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2007-2009 by Thomas Dorner

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.6.1 or,
at your option, any later version of Perl 5 you may have available.

=cut
