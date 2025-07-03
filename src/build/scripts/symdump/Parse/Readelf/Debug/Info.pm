package Parse::Readelf::Debug::Info;

# Author, Copyright and License: see end of file

=head1 NAME

Parse::Readelf::Debug::Info - handle readelf's debug info section with a class

=head1 SYNOPSIS

  use Parse::Readelf::Debug::Info;

  my $debug_info = new Parse::Readelf::Debug::Info($exelwtable);

  my @item_ids = $debug_info->item_ids('l_object2a');
  my @structure_layout1 = $debug_info->structure_layout($item_ids[0]);
  my @some_item_ids = $debug_info->item_ids_matching('^var', 'variable');
  my @all_item_ids = $debug_info->item_ids_matching('');
  my @all_struct_ids = $debug_info->item_ids_matching('', '.*structure.*');

=head1 ABSTRACT

Parse::Readelf::Debug::Info parses the output of C<readelf
--debug-dump=info> and stores its interesting details in an object to
ease access.

=head1 DESCRIPTION

Normally an object of this class is constructed with the file name of
an object file to be parsed.  Upon construction the file is analysed
and all relevant information about its debug info section is stored
inside of the object.  This information can be accessed afterwards
using a bunch of getter methods, see L</"METHODS"> for details.

AT THE MOMENT ONLY INFORMATION REGARDING THE BINARY ARRANGEMENT OF
VARIABLES (STRUCTURE LAYOUT) IS SUPPORTED.  Other data is ignored for
now.

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

#########################################################################

=head1 EXPORT

Nothing is exported by default as it's normally not needed to modify
any of the variables declared in the following export groups:

=head2 :all

all of the following groups

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

require Exporter;

our @ISA = qw(Exporter);
our @EXPORT = qw();

our %EXPORT_TAGS =
    (command => [ qw($command) ],
     config => [ qw($display_nested_items $re_substructure_filter) ],
     fixed_regexps => [ qw($re_section_start
               $re_section_stop
               $re_dwarf_version) ],
     versioned_regexps => [ qw(@re_item_start
                   @re_bit_offset
                   @re_bit_size
                   @re_byte_size
                   @re_const_value
                   @re_decl_file
                   @re_decl_line
                   @re_external
                   @re_member_location
                   @re_name_tag
                   @re_specification
                   @re_type
                   @re_upper_bound
                   @tag_needs_attributes
                   @ignored_tags) ]
    );
$EXPORT_TAGS{all} = [ map { @$_ } values(%EXPORT_TAGS) ];

our @EXPORT_OK = ( @{ $EXPORT_TAGS{all} } );

#########################################################################

=head2 :command

=over

=item I<$command>

is the variable holding the command to run C<readelf> to get the
information relevant for this module, normally C<readelf
--debug-dump=line>.

=back

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

our $command = 'falcon-elf-readelf --debug-dump=info';

#########################################################################

=head2 :config

=over

=item I<$display_nested_items>

is a variable which controls if nested items (e.g. sub-structures) are
not displayed unless actually used (e.g. as data type of members of
their parent) or if they are always displayed - which might confuse
the reader.  The default is 0, any other value switches on the
unconditional display.

=item I<$re_substructure_filter>

is a regular expression that allows you to cut away the details of all
substructures whose type names match the filter.  This is useful if
you have a bunch of types that you consider so basic that you like to
blend out their details, e.g. the internal representation of a complex
number datatype.  The filter has the value C<^string$> for C++
standard strings as default.

=back

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

our $display_nested_items = 0;

our $re_substructure_filter = '^string$';

#########################################################################

=head2 :fixed_regexps

=over

=item I<$re_section_start>

is the regular expression that recognises the start of the line debug
output of C<readelf>.

=item I<$re_section_stop>

is the regular expression that recognises the start of another debug
output of C<readelf>.

=item I<$re_dwarf_version>

is the regular expression that recognises the Dwarf version line in a
line debug output of C<readelf>.  The version number must be an
integer number which will (must) be stored in C<$1>.

=back

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

our $re_section_start =
    qr(^The section \.debug_info contains:|^Contents of the \.debug_info section:);

our $re_section_stop =
    qr(^The section \.debug_.* contains:|^Contents of the \.debug_.* section:);

our $re_dwarf_version = qr(^\s*Version:\s+(\d+)\s*$);

#########################################################################

=head2 :versioned_regexps

These regular expressions are those that recognise the (yet) supported
tags of the item nodes of a readelf debug info output.  Each of them
is actually a list using the Dwarf version as index:

=over

=item I<@re_item_start>

recognises the start of a new item in the debug info list.  C<$1> is
the level, C<$2> the internal (unique) item ID, C<$3> the numeric type
ID and C<$4> the type tag.

=item I<@re_bit_offset>

recognises the bit offset tag of an item.  C<$1> will contain the offset.

=item I<@re_bit_size>

recognises the bit size tag of an item.  C<$1> will contain the size.

=item I<@re_byte_size>

recognises the byte size tag of an item.  C<$1> will contain the size.

=item I<@re_comp_dir>

recognises the compilation directory tag of an item.  C<$1> will
contain the compilation directory as string.

=item I<@re_const_value>

recognises the const value tag of an item.  C<$1> will contain the value.

=item I<@re_decl_file>

recognises the declaration file tag of an item.  C<$1> will contain
the number of the file name (see L<Parse::Readelf::Debug::Line>).

=item I<@re_decl_line>

recognises the declaration line tag of an item.  C<$1> will contain
the line number.

=item I<@re_declaration>

recognises the declaration tag of an item.  C<$1> will usually contain a
1 indicating that it is set.

=item I<@re_encoding>

recognises the encoding tag of an item.  C<$1> will contain the
encoding as text.

=item I<@re_external>

recognises the external tag of an item.  C<$1> will usually contain a
1 indicating that it is set.

=item I<@re_language>

recognises the language tag of an item.  C<$1> will contain the
language as text.

=item I<@re_location>

recognises the data member location tag of an item.  C<$1> will
contain the offset.

=item I<@re_member_location>

recognises the data location tag of an item.  C<$1> will contain the
hex value (with spaces between each byte).

=item I<@re_name_tag>

recognises the name tag of an item.  C<$1> will contain the name.

=item I<@re_producer>

recognises the producer tag of an item.  C<$1> will contain the
producer as string.

=item I<@re_specification>

recognises the specification tag of an item.  C<$1> will contain the
internal item ID of the specification.

=item I<@re_type>

recognises the type tag of an item.  C<$1> will contain the internal
item ID of the type.

=item I<@re_upper_bound>

recognises the upper bound tag of a subrange item.  C<$1> will contain
the upper bound.

=item I<@re_ignored_attributes>

recognises all attributes that are simply ignored (yet).

=back

The last two lists are a bit different, they control what is parsed by
this module.  They are also arrays using the Dwarf version as index.
What is inside each of this arrays is described below:

=over

=item I<@tag_needs_attributes>

holds hashes of the type tags that are processed.  Each element points
to a list of the absolutely needed attributes for that type of item.

=item I<@ignored_tags>

is a list of the type tags (see C<@re_item_start> above) that are
lwrrently ignored.

=back

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

our @re_item_start =
    ( undef, undef,
      '^\s*<(\d+)><([0-9A-F]+)>:\s+abbrev\s+number:\s+(\d+)\s+\((.*)\)'
    );

our @re_bit_offset =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_bit_offset\s*:\s+(\d+))i );

our @re_bit_size =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_bit_size\s*:\s+(\d+))i );

our @re_byte_size =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_byte_size\s*:\s+(\d+))i );

our @re_comp_dir =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_comp_dir\s*:(?:.+:)?\s+(.+))i );

our @re_const_value =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_const_value\s*:\s+([-\d]+))i );

our @re_decl_file =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_decl_file\s*:\s+(\d+))i );

our @re_decl_line =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_decl_line\s*:\s+(\d+))i );

our @re_declaration =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_declaration\s*:\s+(\d+))i );

our @re_encoding =
    ( undef, undef,
      '^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_encoding\s*:\s+\d+\s+\(([a-z ]+)\)' );

our @re_external =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_external\s*:\s+(\d+))i );

our @re_language =
    ( undef, undef,
      '^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_language\s*:\s+\d+\s+\((.+)\)' );

our @re_location =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_location\s*:\s*\d+ byte block:\s+([[:xdigit:]]{1,2}(?: [[:xdigit:]]{1,2})*)\s+\W)i
    );

our @re_member_location =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_data_member_location:.*DW_OP_plus_uconst:\s+(\d+))i
    );

our @re_name_tag =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_name\s*.*:\s+(.*[\w>]))i );

our @re_producer =
    ( undef, undef,
      '^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_producer\s*:(?:\s+\(.+\):)?\s+(.+)' );

our @re_specification =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_specification\s*:\s+<(?:0x)?([0-9A-F]+)>)i );

our @re_type =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_type\s*:\s+<(?:0x)?([0-9A-F]+)>)i );

our @re_upper_bound =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_upper_bound\s*:\s+(\d+))i );

our @re_ignored_attributes =
    ( undef, undef,
      qr(^\s*(?:<[0-9A-F ]+>)?\s*DW_AT_(?:artificial|(?:entry|high|low)_pc|macro_info|ranges|sibling|stmt_list|location\s*:\s*0x))i );

our @tag_needs_attributes =
    (
     undef,
     undef,
     {
      DW_TAG_array_type => [ qw(type) ],
      DW_TAG_base_type => [ qw(name) ],
      DW_TAG_class_type => [],
      DW_TAG_const_type => [ qw(type) ],
      DW_TAG_compile_unit => [ qw(name) ],
      DW_TAG_enumerator => [ qw(name) ],
      DW_TAG_enumeration_type => [ qw(name byte_size) ],
      DW_TAG_formal_parameter => [ qw(name type) ],
      DW_TAG_member => [ qw(name type member_location) ],
      DW_TAG_pointer_type => [ qw(byte_size) ],
      DW_TAG_ptr_to_member_type => [ qw(containing_type) ],
      DW_TAG_reference_type => [ qw(type byte_size) ],
      DW_TAG_structure_type => [],
      DW_TAG_subrange_type => [ qw(upper_bound) ],
      DW_TAG_typedef => [ qw(name type) ],
      DW_TAG_union_type => [ qw(name byte_size) ],
      DW_TAG_variable => [ qw(name type) ],
      DW_TAG_volatile_type => [ qw(type) ]
     }
    );

our @ignored_tags =
    (
     undef,
     undef,
     [
      qw(
    DW_TAG_inlined_subroutine
    DW_TAG_imported_declaration
    DW_TAG_imported_module
    DW_TAG_inheritance
    DW_TAG_label
    DW_TAG_lexical_block
    DW_TAG_namespace
    DW_TAG_subprogram
    DW_TAG_subroutine_type
    DW_TAG_unspecified_parameters
      )
     ]
    );

#########################################################################

=head2 new - get readelf's debug info section into an object

    $debug_info = new Parse::Readelf::Debug::Info($file_name,
                                                 [$line_info]);

=head3 example:

    $debug_info1 = new Parse::Readelf::Debug::Info('program');
    $line_info = new Parse::Readelf::Debug::Line('module.o');
    $debug_info2 = new Parse::Readelf::Debug::Info('module.o',
                                                   $line_info);

=head3 parameters:

    $file_name          name of exelwtable or object file
    $line_info          a L<Parse::Readelf::Debug::Line> object 

=head3 description:

    This method parses the output of C<readelf --debug-dump=info> and
    stores its interesting details internally to be accessed later by
    getter methods described below.

    If no L<Parse::Readelf::Debug::Line> object is passed as second
    parameter the method creates one internally at it is needed to
    locate the source files.

=head3 global variables used:

    The method uses all of the variables described above in the
    L</"EXPORT"> section.

=head3 returns:

    The method returns the blessed Parse::Readelf::Debug::Info object
    or an exception in case of an error.

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
sub new($$;$)
{
    my $this = shift;
    my $class = ref($this) || $this;
    my ($file_name, $line_info) = @_;
    my %self = (line_info => $line_info,
        items => [],
        item_map => {},
            name_map => {});
    local $_;

    # checks:
    if (! $file_name)
    { croak 'bad call to new of ', __PACKAGE__ }
    if (ref($this))
    { carp 'cloning of a ', __PACKAGE__, ' object is not supported' }
    if (! -f $file_name)
    { croak __PACKAGE__, " can't find ", $file_name }
    if (defined $line_info  and
    ref($line_info) ne 'Parse::Readelf::Debug::Line')
    { croak 'bad Parse::Readelf::Debug::Line object passed to ', __PACKAGE__ }

    # first get debug line section parsed:
    $self{line_info} = new Parse::Readelf::Debug::Line($file_name)
    unless defined $line_info;

    # call readelf and prepare parsing output:
    open READELF, '-|', $command.' '.$file_name  or
    croak "can't parse ", $file_name, ' with "', $command, '" in ',
        __PACKAGE__, ': ', $!;

    # find start of section:
    while (<READELF>)
    { last if m/$re_section_start/; }

    # parse section:
    my $version = -1;
    my @level_stack = (undef);
    my $item = undef;
    my $needed_attributes = undef;
    my %is_ignored = ();
    my $tag_needs_attributes = undef;
    my $compilation_unit = -1;
    while (<READELF>)
    {
    if (m/$re_dwarf_version/)
    {
        $version = $1;
        confess 'DWARF version ', $version, ' not supported in ',
        __PACKAGE__
            unless defined $re_item_start[$version];
        %is_ignored = map { $_ => 1 } @{$ignored_tags[$version]};
        $tag_needs_attributes = $tag_needs_attributes[$version];
        $compilation_unit++;
    }
    next unless $version >= 0;

    # stop at end of section:
    if (m/$re_section_stop/)
    {
        my $dummy = grep /nothing/, <READELF>; # avoid SIGPIPE in close
        last;
    }

    # handle the beginning (and therefore the change) of an item:
    if (m/$re_item_start[$version]/i)
    {
        # check if item is complete and store it:
        if (defined $item)
        {
        foreach (@$needed_attributes)
        {
            next if defined $item->{$_};
            # special handling of items that contain
            # additional info needed by other items:
            if ($item->{type_tag} eq 'DW_TAG_member' &&
            defined $item->{member_location} &&
            defined $item->{type} &&
            defined $self{item_map}->{$item->{type}} &&
            ! defined
            $self{item_map}->{$item->{type}}->{member_location})
            {
            $self{item_map}->{$item->{type}}->{member_location} =
                $item->{member_location};
            }
#TODO: activate check again later:
#            carp('necessary attribute tag ', $_, ' is missing in ',
#             $item->{type_tag},
#             (defined $item->{name} ? ' for '.$item->{name} : ''),
#             ' at position ', $item->{id});
            $item = undef;
            last;
        }
        }
        if (defined $item)
        {
        confess 'item ', $item, ' has no type tag in ', __PACKAGE__
            unless $item->{type_tag};
        push @{$self{items}}, $item;
        $self{item_map}->{$item->{id}} = $item;
        # handle stack of item levels:
        if ($item->{level} >= 1)
        {
            push @{$level_stack[$item->{level} - 1]->{sub_items}},
            $item
                if $item->{level} > 1;
            pop @level_stack while ($#level_stack >= $item->{level});
            $level_stack[$item->{level}] = $item;
        }
        # Take special care of structure names that are stored
        # in another node:
        my $name = $item->{name};
        if (not defined $name  and
            $item->{type_tag} =~ m/^DW_TAG_(?:class|structure)_type$/
            and
            defined $item->{specification}  and
            defined $self{item_map}->{$item->{specification}})
        {
            $name = $self{item_map}->{$item->{specification}}->{name};
        }
        # the name map can store items with unique names
        # (simple reference) and identical names (array of
        # references):
        if (defined $name)
        {
            if (not defined $self{name_map}->{$name})
            { $self{name_map}->{$name} = $item }
            elsif (ref($self{name_map}->{$name}) eq 'HASH')
            {
            $self{name_map}->{$name} =
                [ $self{name_map}->{$name}, $item ]
            }
            elsif (ref($self{name_map}->{$name}) eq 'ARRAY')
            { push @{$self{name_map}->{$name}}, $item }
            else
            {
            confess 'internal error: invalid reference type ',
                ref($self{name_map}->{$name}),
                ' in name_map in ', __PACKAGE__
            }
        }
        # for items with known location add object id:
        $item->{compilation_unit} = $compilation_unit
            if defined $item->{decl_file};
        # brush up stored item with a few item tag specific fixes:
        $item->{name} = 'void'
            if ($item->{type_tag} eq 'DW_TAG_pointer_type'  and
            not defined $item->{name}  and
            not defined $item->{type});
        # save a bit of memory (strings):
        }
        # prepare node for next item (we ignore the type ID in $3
        # except for the carp below as the ID uses a new sequence
        # for every compilation unit and is therefore pretty much
        # worthless for us):
        $item = { level => $1, id => $2, type_tag => $4, sub_items => [] };
        if (defined $tag_needs_attributes->{$4})
        {
        $needed_attributes = $tag_needs_attributes->{$4};
        }
        elsif ($is_ignored{$4})
        {
        pop @level_stack while ($#level_stack >= $item->{level});
        $item = undef;
        }
        else
        {
        carp 'unknow item type ', $4, ' (', $3,
            ') found at position ', $2;
        $item = undef;
        }
    }
    elsif (not defined $item)
    { next }
    elsif (m/$re_bit_offset[$version]/)
    { $item->{bit_offset} = $1 }
    elsif (m/$re_bit_size[$version]/)
    { $item->{bit_size} = $1 }
    elsif (m/$re_byte_size[$version]/)
    { $item->{byte_size} = $1 }
    elsif (m/$re_comp_dir[$version]/)
    { $item->{comp_dir} = $1 }
    elsif (m/$re_const_value[$version]/)
    { $item->{const_value} = $1 }
    elsif (m/$re_decl_file[$version]/)
    { $item->{decl_file} = $1 }
    elsif (m/$re_decl_line[$version]/)
    { $item->{decl_line} = $1 }
    elsif (m/$re_declaration[$version]/)
    { $item->{declaration} = $1 }
    elsif (m/$re_encoding[$version]/)
    { $item->{encoding} = $1 }
    elsif (m/$re_external[$version]/)
    { $item->{external} = $1 }
    elsif (m/$re_language[$version]/)
    { $item->{language} = $1 }
    elsif (m/$re_location[$version]/)
    { $item->{location} = $1 }
    elsif (m/$re_member_location[$version]/)
    { $item->{member_location} = $1 }
    elsif (m/$re_name_tag[$version]/)
    { $item->{name} = $1 }
    elsif (m/$re_producer[$version]/)
    { $item->{producer} = $1 }
    elsif (m/$re_specification[$version]/)
    { $item->{specification} = $1 }
    elsif (m/$re_type[$version]/)
    { $item->{type} = $1 }
    elsif (m/$re_upper_bound[$version]/)
    { $item->{upper_bound} = $1 }
    elsif (m/$re_ignored_attributes[$version]/)
    {}
    elsif (m/^\s*(?:<[0-9A-F ]+>)?\s*(DW_AT_\w+)\s/)
    {
        chomp;
        carp('unknown attribute type ', $1, ' found at position ',
         $item->{id}, ' : ', $_);
    }
    }

    # now we're finished:
    close READELF  or
    croak 'error while attempting to parse ', $file_name,
        ' (maybe not an object file?)';
    @{$self{items}} > 0  or
    croak 'aborting: debug info section seems empty in ', __PACKAGE__;

    bless \%self, $class;
}

#########################################################################

=head2 item_ids - get object ID(s) of (named) item

    @item_ids = $debug_info->item_ids($identifier);

=head3 example:

    @item_ids = $debug_info->item_ids('my_variable');

=head3 parameters:

    $identifier         name of item (e.g. variable name)

=head3 description:

    This method returns the internal item ID of all identifiers with
    the given name as array.

=head3 returns:

    If a name is unique, the method returns an array with exactly one
    element, if a name does not exist it returns an empty array and
    otherwise an array containing the IDs of all matching itmes is
    returned.

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
sub item_ids($$)
{
    my $this = shift;
    my ($identifier) = @_;
    local $_;

    my $id = $this->{name_map}{$identifier};
    return
    map { $_->{id} }
        (! defined $id ? ()
         : ref($id) eq 'HASH' ? ($id)
         : @{$id});
}

#########################################################################

=head2 item_ids_matching - get object IDs of items matching constraints

    @item_ids = $debug_info->item_ids_matching($re_name, [$re_type_tag]);

=head3 example:

    @some_item_ids = $debug_info->item_ids_matching('^var', 'variable');
    @all_item_ids = $debug_info->item_ids_matching('');
    @all_structure_ids = $debug_info->item_ids_matching('', '.*structure.*');

=head3 parameters:

    $re_name            regular expression matching name of items
    $re_type_tag        regular expression matching type tag of items

=head3 description:

    This method returns an array containing the internal item ID of
    all identifiers that match both the regular expression for their
    name and their type tags.  Note that an empty string will match
    any name or type tag, even missing ones.  Also note that type tags
    in Dwarf 2 always begin with C<DW_TAG_>.

=head3 returns:

    If a name is unique, the method returns an array with exactly one
    element, if a name does not exist it returns an empty array and
    otherwise an array containing the IDs of all matching itmes is
    returned.  The IDs are sorted alphabetically according to their
    names.

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
sub item_ids_matching($$;$)
{
    my $this = shift;
    my ($re_name, $re_type_tag) = (@_, '.');
    $re_name = '.' if $re_name eq '';
    local $_;

    my @ids = ();
    foreach (map { ref($_) eq 'HASH' ? $_ : @$_ }
         values %{$this->{name_map}})
    {
    next if defined $_->{name}  and  $_->{name} !~ m/$re_name/;
    next if (not defined $_->{name}  and
         $re_name ne ''  and
         not ($_->{type_tag} =~ m/^DW_TAG_(?:class|structure)_type$/
              and
              defined $_->{specification}  and
              $this->{item_map}->{$_->{specification}}->{name}
              =~ m/$re_name/));
    next if defined $_->{type_tag}  and  $_->{type_tag} !~ m/$re_type_tag/;
    next if not defined $_->{type_tag}  and  $re_type_tag ne '';
    push @ids, [ $_->{id}, ( defined $_->{name} ? $_->{name} : '' ) ];
    }
    return
    map { $_->[0] }
        sort { $a->[1] cmp $b->[1] }
        @ids;
}

#########################################################################

=head2 structure_layout - get structure layout of variable or data type

    @structure_layout =
        $debug_info->structure_layout($id, [$initial_offset]);

=head3 example:

    @structure_layout1 =
        $debug_info->structure_layout('1a8');
    @structure_layout2 =
        $debug_info->structure_layout('2f0', 4);

=head3 parameters:

    $id                 internal ID of item
    $initial_offset     offset to be used for the beginning of the layout

=head3 description:

    This method returns the structure layout of a variable or data
    type with the given item ID (which can be found with the method
    L<"item_ids"> or L<"item_ids_matching">).  For each element of a
    structure it returns a quintuple containing (in that order)
    I<relative level>, I<name>, I<data type>, I<size> and I<offset>
    allthough some of the information might be missing (which is
    indicated by an empty string).  For bit fields two additional
    fields are added: I<bit-size> and I<bit-offset> (either both are
    defined or none at all).

=head3 returns:

    The method returns an array of the quintuples described above.

=cut

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
sub structure_layout($$;$)
{
    my $this = shift;
    my ($id, $initial_offset) = @_;
    $initial_offset = 0 unless defined $initial_offset;
    local $_;

    my $item = $this->{item_map}->{$id};
    # ignore undefined items or standard items (as standard data types):
    return () unless defined $item and defined $item->{decl_file};

    # handle relative level - 1:
    if (defined $this->{sl_level})
    { $this->{sl_level}++ }
    else
    {
    $this->{sl_level} = 0;
    $this->{tag_stack} = [];
    }
    my $level = $this->{sl_level};

    # maintain a stack of the item tags:
    $this->{tag_stack}->[$level] = $item->{type_tag};

    # check for nested structures (if applicable) and don't process
    # anything if we found one:
    my @result = ();
    if ($display_nested_items  or
    $item->{type_tag} ne 'DW_TAG_structure_type'  or
    $level < 1 or
    $item->{type_tag} ne $this->{tag_stack}->[$level - 1])
    {
    # get name:
    my $name = $item->{name};
    if (not defined $name  and
        $level < 1 and
        $item->{type_tag} =~ m/^DW_TAG_(?:class|structure)_type$/  and
        defined $item->{specification}  and
        defined $this->{item_map}->{$item->{specification}})
    {
        $name = $this->{item_map}->{$item->{specification}}->{name};
    }
    $name = '' unless defined $name;

    # handle offset:
    my $offset =
        defined $item->{member_location} ? $item->{member_location} : 0;
    $offset += $initial_offset;

    # handle size - 1:
    my $size = defined $item->{byte_size} ? $item->{byte_size} : 0;

    # handle bit size and offset:
    my @bit_data = ();
    if (defined $item->{bit_size} or defined $item->{bit_offset})
    {
        $bit_data[0] =
        defined $item->{bit_size} ? $item->{bit_size} : 0;
        $bit_data[1] =
        defined $item->{bit_offset} ? $item->{bit_offset} : 0;
    }

    # handle types:
    my $type_name = '';
    my @sub_layout = ();
    if (defined $item->{type})
    {
        my $type = $this->{item_map}->{$item->{type}};
        my $prefix = '';
        my $postfix = '';
        # for special types use shortlwt to their sub-types:
        while ($type->{type_tag})
        {
        # const:
        if ($type->{type_tag} eq 'DW_TAG_const_type')
        {
            $prefix .= 'const ' unless $prefix =~ m/const/;
            $type = $this->{item_map}->{$type->{type}};
            next;
        }
        # volatile:
        elsif ($type->{type_tag} eq 'DW_TAG_volatile_type')
        {
            $prefix .= 'volatile ' unless $prefix =~ m/volatile/;
            $type = $this->{item_map}->{$type->{type}};
            next;
        }
        # reference:
        elsif ($type->{type_tag} eq 'DW_TAG_reference_type')
        {
            $postfix .= '&';
            $type = $this->{item_map}->{$type->{type}};
            next;
        }
        # pointer:
        elsif ($type->{type_tag} eq 'DW_TAG_pointer_type')
        {
            $postfix .= '*';
            if (defined $type->{type}  and
            defined $this->{item_map}->{$type->{type}})
            {
            $type = $this->{item_map}->{$type->{type}};
            next;
            }
        }
        # arrays:
        elsif ($type->{type_tag} eq 'DW_TAG_array_type')
        {
            $name .= '[';
            $name .= $type->{sub_items}->[0]->{upper_bound} + 1
            if defined $type->{sub_items}->[0]->{upper_bound};
            $name .= ']';
            $type = $this->{item_map}->{$type->{type}};
            next;
        }
        last;
        }

        # handle size - 2:
        $size = $type->{byte_size}
        if ($size == 0  and
            defined $type->{byte_size}  and
            $type->{byte_size} > 0);

        # handle details of types in relwrsion:
        @sub_layout = $this->structure_layout($item->{type}, $offset);

        # for templates use shortlwt to their specification:
        if (defined $type->{type_tag})
        {
            if ($type->{type_tag} =~ m/^DW_TAG_(?:class|structure)_type$/  and
                defined $type->{specification})
            { 
                $type = $this->{item_map}->{$type->{specification}} 
            }
        }

        # set type name:
        $type_name = $type->{name} if defined $type->{name};
        # TODO: all shold be known in later version:
        $type_name = '<unknown>'
        if $type_name eq '' and ($prefix or $postfix);
        $type_name = $prefix.$type_name if $prefix;
        $type_name .= $postfix if $postfix;

        # apply structure filter, if applicable:
        @sub_layout = ()
        if  defined $re_substructure_filter  and
            $type_name =~ m/$re_substructure_filter/;

        # handle size - 3:
        while ($size == 0  and
           defined $type->{type}  and
           $type = $this->{item_map}->{$type->{type}})
        {
        $size = $type->{byte_size}
            if (defined $type->{byte_size}  and
            $type->{byte_size} > 0);
        }
    }

    # handle size - 4:
    if ($name =~ m/\[(\d+)\]$/ and $1 > 0)
    { $size *= $1 }

    # for structured items continue relwrsion:
    foreach (@{$item->{sub_items}})
    { push @sub_layout, $this->structure_layout($_->{id}, $offset) }

    # sort sub-structure:
    if (@sub_layout)
    {
        @sub_layout =
        sort {
            $a->[5] <=> $b->[5]
            ||
                (defined $a->[7]
                 ? (defined $b->[7] ? $a->[7] <=> $b->[7] : 1)
                 : (defined $b->[7] ? -1 : 0)
                )
            }
            @sub_layout;
    }

    # handle location of definition:
    my $location = [];
    if (defined $item->{compilation_unit}  and
        defined $item->{decl_file}  and
        defined $item->{decl_line})
    {
        $location = [$item->{compilation_unit},
             $item->{decl_file},
             $item->{decl_line} ];
    }

    # for unnamed singular substructures eliminate singular level:
    if ($item->{type_tag} =~ m/^DW_TAG_(?:class|structure)_type$/  and
        not $name  and
        not $type_name  and
        0 == @bit_data)
    {
        @result = @sub_layout;
    }
    else
    {
        @result = ([$level, $name, $type_name, $size, $location,
            $offset, @bit_data],
               @sub_layout);
    }
    }

    # handle relative level - 2:
    if ($this->{sl_level} > 0)
    { $this->{sl_level}-- }
    else
    {
    delete $this->{tag_stack};
    delete $this->{sl_level};
    }

    # put everything together and return:
    return @result;
}

1;

#########################################################################

__END__

=head1 KNOWN BUGS

For references the size of the referenced data is shown, not the
internal size of the reference self.  This is a feature.

Only Dwarf version 2 is supported.  Please contact the author for
other versions and provide some example C<readelf> outputs.

This has only be tested in a Unix like environment, namely Linux and
Solaris.

=head1 SEE ALSO

L<Parse::Readelf>, L<Parse::Readelf::Debug::Line> and the C<readelf>
man page

=head1 AUTHOR

Thomas Dorner, E<lt>dorner (AT) pause.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2007-2010 by Thomas Dorner

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.6.1 or,
at your option, any later version of Perl 5 you may have available.

=cut
