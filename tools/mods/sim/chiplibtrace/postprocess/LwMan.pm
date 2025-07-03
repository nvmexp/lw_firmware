package CEval;
use strict;
use warnings;
use File::Temp qw(tempfile tmpnam);

our $verbose = 0;
our $tmp_dir = "/tmp";
our $cc = "gcc";
our @g_out;

sub c_gen
{
    my ($header, $main) = @_;
    my $out = '';
    
    if (defined $header) {
        $header = [$header] unless ref $header eq 'ARRAY';
        foreach (@$header) {
            $out .= "$_\n";
        }
    }

    $out .= "\nint main(void) {\n";

    if (defined $main) {
        $main = [$main] unless ref $main eq 'ARRAY';
        foreach (@$main) {
            $out .= "$_\n";
        }
    }

    $out .= "\nreturn 0;\n}\n";
   
    warn "C_GEN: $out\n" if $verbose > 1;
    return $out;
}

# This is really the neat part of the script.
# We use the C precompiler to handle everything that could legally be in
# our manual #defines anyway
# We write a source file, compile it, execute it, and eval its output
sub c_eval
{
    my ($fh, $tmp_c) = tempfile( DIR => $tmp_dir, SUFFIX => '.c' );
    die "Unable to create temporary files in $tmp_dir\n" unless $fh;
    my $tmp_exe = tmpnam();
    die "Unable to create temporary files in $tmp_dir\n" unless $tmp_exe;

    print $fh c_gen(@_);
    close $fh;
    
    my $gcc_cmd = "$cc -o $tmp_exe $tmp_c";
    warn "GCC: $gcc_cmd\n" if $verbose > 0;
    my $gcc_out = `$gcc_cmd`;

    die "gcc error: $gcc_out\n" if $gcc_out;
    die "gcc error: $tmp_exe did not get generated\n" unless -f $tmp_exe;

    @g_out = ();
    warn "EXE: $tmp_exe\n" if $verbose > 0;
    my $exe_out = `$tmp_exe`;
    warn "EXE_OUT: $exe_out\n" if $verbose > 1;
    eval $exe_out;
    die "$@\n" if $@;

    unlink "$tmp_exe";
    unlink "$tmp_c";

    return @g_out;
}

# Will be called within "eval" above
sub c_push
{
    push @g_out, @_;
}

package LwManual;
use strict;
use warnings;
use File::Basename;
use Storable qw(retrieve nstore);

our $verbose = 0;

sub new
{
    my ($class, %init) = @_;

    if (defined $init{load} and -f $init{load}) {
        warn "Loading LwManual from $init{load}\n" if $verbose > 0;        
        my $load = retrieve($init{load});
        if (defined $load and ref $load eq $class) {
            $load->{from_cache} = 1;
            $load->init(%init);
            return $load;
        }
        warn "Unable to load: $init{load}\n";
    }
    
    my $self = {
        from_cache => 0,
        cache_rd => 1,
        cache_wr => 1,
        cache => {},

        dir => undef,
        range_offsets => {},
        range_reg_prefix => {},
    };

    bless $self, $class;
    $self->init(%init);
    return $self;
}

sub init
{
    my ($self, %init) = @_;
    
    foreach (keys %init) {
        next unless defined $init{$_}; # Skip empty ones

        warn "Invalid chip: $init{$_}\n" 
            if $_ eq 'chip' and not $self->set_chip($init{$_});
        
        warn "Invalid manual directory: $init{$_}\n"
            if $_ eq 'dir' and not $self->set_dir($init{$_});
        
        $self->{$_} = $init{$_} if /^cache/;
    }
}

sub set_chip
{
    my ($self, $chip) = @_;
   
    $self->{dir} = undef;
    foreach ("/home/gpu_refmanuals/$chip",
             "/home/scratch.$chip\_master/$chip/$chip/manuals/$chip") {
        $self->set_dir($_);
        last if defined $self->{dir};
    }
   
    # TODO: Figure out some way to load these hashes based on chip
    $self->{range_offsets} = {
        'LW_PMC' => 0,
        'LW_XVE' => 0x88000,
        'LW_PES_XVD' => 0,
        'LW_PES_XVU' => 0,
    };

    $self->{range_reg_prefix} = {
        'LW_IFB' => 'LW_PBUS_IFB',
    };
    
    return defined $self->{dir};
}

sub set_dir
{
    my ($self, $dir) = @_;
    $self->{dir} = $dir if -d $dir;
    return defined $self->{dir} and $self->{dir} eq $dir;
}

sub save
{
    my ($self, $file) = @_;
    nstore $self, $file;
}

sub clear_cache
{
    my ($self) = @_;
    $self->{cache} = {dir => $self->{dir}};
}

sub check_cache
{
    my ($self) = @_;
    return 0 unless defined $self->{cache}->{dir};
    return 0 unless $self->{cache}->{dir} eq $self->{dir};
    return 1;
}

sub to_cache
{
    my ($self, $data, @key) = @_;
    return unless $self->{cache_wr};
    die unless scalar @key;

    unless ($self->check_cache) {
        warn "Cache [Faild]: dir\n" if $verbose > 0;
        $self->clear_cache;   
    }
    
    my $ptr = $self->{cache};
    my $last = pop @key;
    foreach (@key) {
        $ptr->{$_} = {} unless defined $ptr->{$_};
        $ptr = $ptr->{$_};
    }
    $ptr->{$last} = $data;

    warn sprintf "Cache [Wrote]: %-s\n", join('->',@key,$last) if $verbose > 0;
}

sub from_cache
{
    my ($self, @key) = @_;
    return unless $self->{cache_rd};
    die unless scalar @key;
    
    unless ($self->check_cache) {
        warn sprintf "Cache [STALE]: %-s\n", join('->',@key) if $verbose > 0;
        return undef;
    }

    my $ptr = $self->{cache};
    my $last = pop @key;
    foreach (@key) {
        return undef unless defined $ptr->{$_};
        $ptr = $ptr->{$_};
    }
    
    warn sprintf "Cache [%-s]: %-s\n",
        (defined $ptr->{$last} ? "Found" : "UNDEF"), join('->',@key,$last)
            if $verbose > 0;    
    
    return $ptr->{$last};
}

sub ref_files
{
    my ($self) = @_;
    die "No manual directory specified\n" unless defined $self->{dir};
    my $dh;
    warn "Using manual directory: $self->{dir}\n" if $verbose > 0;
    opendir $dh, $self->{dir} or die "Unable to open directory '$self->{dir}'\n";
    my @subs = sort readdir($dh);
    closedir $dh;
    
    my @files;
    foreach (@subs) {
        next unless /^dev_.+\.ref$/; # match dev_*.ref only
        s/^/$self->{dir}\//;         # We'll return the full path here
        push @files, $_ if -f;       # And only if its a file
    }
    warn "Found ".(scalar @files)." manual files\n" if $verbose > 0;
    return @files;
}

sub filter_ranges
{
    my ($self, $offset, @ranges) = @_;

    my %matching;
    foreach (@ranges) {
        my $check_offset = $offset;
        if (defined $self->{range_offsets}->{$_->{name}}) {
            $check_offset -= $self->{range_offsets}->{$_->{name}};
        }
        elsif ($_->{base} == 0) {
            next; # Skip these - We don't know where they actually are
        }
        
        # These exclude our test register
        next if $check_offset > $_->{extent} or $check_offset < $_->{base};
       
        warn sprintf "Matching range: %-20s %-s[0x%08x:0x%08x] %-s:%-d\n",
            $_->{name}, $_->{type}, $_->{extent}, $_->{base}, $_->{file}, $_->{line}
            if $verbose > 0;
        $matching{$_->{file} . ":" . $_->{line}} = $_;    
    }
    return values %matching;
}

sub filter_registers
{
    my ($self, $offset, @registers) = @_;
    my %matching;
    foreach (@registers) {
        # These exclude our test register
        next unless $offset == $_->{offset};
        
        $matching{$_->{file} . ":" . $_->{line}} = $_;    
    }
    return values %matching;
}

sub register_data
{
    my ($self, $reg, $value, %opts) = @_;
    my @fields = extract_data($reg->{name}, $reg->{file}, $reg->{line}, $value);

    my $us = defined $opts{_} ? '_' : '';

    my $reg_name_no_idx = $reg->{name};
    $reg_name_no_idx =~ s/\(.*\)$//;
                
    foreach (@fields) {
        my $field = $_->{field};
        my $best = undef;
        foreach (@{$_->{enum}}) {
            $best = $_ if       # Nothing < INIT < DEFAULT < PROD < Anything else
                (!defined $best) or
                ($best =~ /_INIT$/) or
                ($best =~ /_DEFAULT$/ and $_ !~ /_INIT$/) or # 
                ($best =~ /__PROD/ and $_ !~ /_INIT$/ and $_ !~ /_DEFAULT$/);
            
            s/^$field$us// if $opts{short_enums};
        }
        $best =~ s/^$field$us// if defined $best and $opts{short_enums};
        $_->{best} = defined $best ? $best : "?" if $opts{best};

        $_->{field} =~ s/^$reg_name_no_idx$us// if $opts{short_fields};
    }
                
    return @fields;
}

sub get_ranges
{
    my ($self) = @_;
    my $cached = $self->from_cache('ranges');
    return @$cached if defined $cached;

    my @ref_files = $self->ref_files;
    my @all_ranges = extract_ranges( @ref_files );
    
    $self->to_cache(\@all_ranges, 'ranges');
    return @all_ranges;
}

sub get_registers
{
    my ($self, $range_name, $range_file) = @_;

    # Factor in range renaming
    my $reg_prefix = defined $self->{range_reg_prefix}->{$range_name} ?
        $self->{range_reg_prefix}->{$range_name} : $range_name;
    
    my $cached = $self->from_cache('registers', $reg_prefix, $range_file);
    return @$cached if defined $cached;

    my @range_all_registers = extract_registers($reg_prefix, $range_file);
        
    $self->to_cache(\@range_all_registers, 'registers', $reg_prefix, $range_file);

    return @range_all_registers;
}

sub get_ranges_by_offset
{
    my ($self, $offset) = @_;
    my @all_ranges = $self->get_ranges;
    my @ranges = $self->filter_ranges($offset, @all_ranges);
    return @ranges if wantarray;
    
    # If we only want one, return undef if none, warn if more than 1, and return first
    return undef unless @ranges;
    warn sprintf "Multiple ranges for 0x%08x\n", $offset;
    return $ranges[0];
}

sub get_registers_by_offset
{
    my ($self, $offset, %opts) = @_;
    my @ranges = $opts{ranges} ? @{$opts{ranges}} : $self->get_ranges_by_offset($offset);
    my %registers;
    foreach my $range (@ranges) {
        $registers{$range->{name}} = [$self->filter_registers(
            # Factor in range offsets
            defined $self->{range_offsets}->{$range->{name}} ?
                $offset - $self->{range_offsets}->{$range->{name}} : $offset,
            $self->get_registers($range->{name}, $range->{file}),
        )];
    }
    return %registers if $opts{tree};
    
    my @registers;
    foreach (keys %registers) {
        push @registers, @{$registers{$_}};
    }
    return @registers if wantarray;

    # If we only want one, return undef if none, warn if more than 1, and return first
    return undef unless @registers;
    warn sprintf "Multiple registers for 0x%08x\n", $offset if @registers > 1;
    return $registers[0];
}


sub get_registers_by_partial_name
{
    my ($self, $partial) = @_;
    
    if ($partial !~ /^(\w+)(~?)$/) {
        warn "Invalid partial name: $partial\n";
        return () if wantarray;
        return undef;
    }
    $partial = uc $1;
        
    # Wanted suffix complete match only
    return $self->get_registers_by_regexp(qr/^\w*$partial(\(\S+)?\s/) if $2;
    
    # Only prefix needs to match
    return $self->get_registers_by_regexp(qr/^\w*$partial/);
}

sub get_registers_by_regexp
{
    my ($self, $regexp) = @_;
    my %files;
    foreach ($self->get_ranges) {
        $files{$_->{file}} = 1;
    }
    my @registers;
    foreach my $file (keys %files) {
        push @registers, extract_registers_core($regexp, $file);
    }
    return @registers;
}

sub get_registers_using_index
{
    my ($self, $regexp, $index_file) = @_;
    $index_file = $self->{dir} . "/index.ar" unless defined $index_file;
    return extract_registers_using_index($index_file, $regexp);
}

# Static methods

sub get_chips
{
    my ($dir) = @_;
    $dir = "/home/gpu_refmanuals" unless defined $dir;
    my @chips;
    foreach (split /\n/, `ls $dir/*/dev_lw_xve.ref`){
        s/.*\/(\w+)\/dev_lw_xve.ref/$1/;
        push @chips, $_;
    }
    return @chips;
}

sub new_range
{
    return {
        file => $_[0],
        line => $_[1],
        name => $_[2],
        type => $_[3],
        extent => $_[4],
        base => $_[5],
    };
}

sub extract_ranges
{
    my (@file_paths) = @_;
    my @main;
    my @header = (
        "#include <stdio.h>",
        "#define DRF_BASE(drf)           (0?drf)",
        "#define DRF_EXTENT(drf)         (1?drf)",
    );

    foreach my $file_path (@file_paths) {
        open MANUAL, "<$file_path" or die "Unable to open '$file_path'\n";
        
        while (<MANUAL>) {
            next unless /^#define\s+(\S+)/;
            my $macro = $1;
            next unless /\/\* ..--([DM]) \*\//;
            push @main, 
                "#ifndef $macro", 
                $_,
                "printf(\"c_push(LwManual::new_range('$file_path', $., '$macro', '$1', 0x%08x, 0x%08x));\\n\", 
                    DRF_EXTENT($macro), DRF_BASE($macro));",
                "#endif";
        }

        close MANUAL;
    }
    
    return CEval::c_eval(\@header, \@main);
}

sub new_register
{
    return {
        file => $_[0],
        line => $_[1],
        name => $_[2],
        type => $_[3],
        offset => $_[4],
    };
}

sub extract_registers_core
{
    my ($filter_regexp, $file) = @_;

    my @header = (
        "#include <stdio.h>",
    );
    my @main;
    my %idxs;

    open MANUAL, "<$file" or die "Unable to open '$file'\n";

    while (<MANUAL>) {
        next unless /^#define\s/;
        push @header, $_; # Push all defines into header
        
        next unless /\/\* ....([RA]) \*\//; # We don't really care except for R/A
        my $type = $1; # Remember if it was R or A
        
        s/^#define\s+//; # Get rid of this jazz
        
        next if defined $filter_regexp and $_ !~ $filter_regexp; # Filter based on some regexp
        /(\w+)/; my $macro = $1; # Macro is the first section, ignoring (i)

        if ($type eq 'A') {
            die "Bad index form: $_" unless $_ =~ /$macro\(\s*([^\)]+)\s*\)/;
            my $idx = 1;
            foreach (split /\s*,\s*/, $1) {
                $idxs{$_} = 1;
                push @main, "#ifdef $macro\__SIZE_$idx";
                push @main, "#if $macro\__SIZE_$idx > 1024", "#warning $macro\__SIZE_$idx is very large", "#else";
                
                push @main, "for($_ = 0; $_ < $macro\__SIZE_$idx; ++$_) {";
                $idx ++;
            }
            my $t = join ',', map { '%d' } (2 .. $idx);
            push @main, "printf(\"c_push(LwManual::new_register('$file', $., '$macro($t)', '$type', 0x%08x));\\n\", $1, $macro($1));";
      
            while (--$idx) {
                push @main, "}", "#endif", "#else", "#warning Expected $macro\__SIZE_$idx to be defined", "#endif";
            }
        }
        else {
            
            if (/$macro\(\s*([^\)]+)\s*\)/) {
                warn "Bad register (non-index) form: $file:$.\n    $_"; 
            }
            else {
                push @main, "printf(\"c_push(LwManual::new_register('$file', $., '$macro', '$type', 0x%08x));\\n\", $macro);";
            }
        }
    }
    push @header, sprintf "int %-s;", join ',', keys %idxs if %idxs;
    close MANUAL;

    return CEval::c_eval(\@header, \@main);
}

sub extract_registers
{
    my ($reg_prefix, $file) = @_;
    my $filter_regexp = qr/^$reg_prefix\_\w+/;
    return extract_registers_core($filter_regexp, $file);
}

sub extract_register
{
    my ($reg_name, $file) = @_;
    my $filter_regexp = qr/^$reg_name\W/;
    return extract_registers_core($filter_regexp, $file);
}

sub extract_registers_using_index
{
    my ($index_file, $regexp, %opts) = @_;
    my @registers;
    my @header = (
        "#include <stdio.h>",
    );
    my @main = 'int i = 0, j = 0, k = 0;';
    
    open INDEX, "<$index_file" or die "Unable to open index file: $index_file\n";
    while (my $grepo = <INDEX>) {
        next unless $grepo =~ $regexp;
        print "Index match: $grepo" if $verbose > 0;
        my ($file, $line, $def) = split ':', $grepo, 3;
        
        unless ($def =~ /^#define (\S+)/) {
            warn "Invalid define: $def\n";
            next;
        }
        my $macro = $1;
        my $type = $def =~ /\/\* ....(.) \*\// ? $1 : '?';
        my $value = $def =~ /$macro\s+(.*?)\s+\/\*/ ? $1 : '?';

        push @registers, new_register($file, $line, $macro, $type, $value);

        push @header, $def;
        push @main, "printf(\"c_push(LwManual::new_register('$file', $line, '$macro', '$type', 0x%08x));\\n\", $macro);";
    }

    close INDEX;
    
    return CEval::c_eval(\@header, \@main) if $opts{c_eval};
    return @registers;
}

my $g_last_data;
sub new_data
{
    my $data = {
        file    => $_[0],
        line    => $_[1],
        field   => $_[2],
        extent  => $_[3],
        base    => $_[4],
        value   => $_[5],
        enum    => [], # Selected enums
        enums   => [], # All enums with values
    };

    $g_last_data = $data;
    return $data;
}

sub add_data_enum
{
    my ($field, $enum, $value, $selected, $rw) = @_;
    die "No last_data available\n" unless defined $g_last_data;
    die "Invalid last_data: $field != $g_last_data->{field}\n"
        unless $field eq $g_last_data->{field};
    if ($selected) {    
        push @{$g_last_data->{enum}}, $enum if $rw =~ /R|C/;
    }
    push @{$g_last_data->{enums}}, {enum => $enum, value => $value, rw => $rw, sel => $selected};
}

sub extract_data
{
    my ($reg_name, $file, $line, $data) = @_;
    $reg_name =~ s/\(.*\)$//; # Replace any indexing
    my $data_c = sprintf "0x%08xl", (defined $data ? $data : 0);
    
    my @main;
    my @header = (
        "#include <stdio.h>",
        "#define DRF_BASE(drf)           (0?drf)",
        "#define DRF_EXTENT(drf)         (1?drf)",
        "#define DRF_SHIFT(drf)          ((0?drf) % 32)",
        "#define DRF_MASK(drf)           (0xFFFFFFFF>>(31-((1?drf) % 32)+((0?drf) % 32)))",
        "#define DRF_SHIFTMASK(drf)      (DRF_MASK(drf)<<(DRF_SHIFT(drf)))",
        "#define DRF_VAL(drf,v)          (((v)>>DRF_SHIFT(drf))&DRF_MASK(drf))",
    );

    open MANUAL, "<$file" or die "Unable to open '$file'\n";
   
    my $field;
    while(<MANUAL>) {
        last if $. >= $line;
    }
    while(<MANUAL>) {
        # last if (/^#define (?!$reg_name)/); # Reached another register
        last if (/^#define $reg_name.* \/\* ....[RA] \*\//); # Reached another register
        next unless /^#define ($reg_name\_\S+)/;
        push @header, $_;
        my $macro = $1;
            
        if ($macro =~ /\(/) {
            # warn "Unhandled field (index) form: $file:$.\n";
            next;
        }

        if (/\/\* ....F \*\//) {
            $field = $macro;
            push @main, "printf(\"c_push(LwManual::new_data('$file', $., '$field', \%d, \%d, 0x\%08x));\\n\",",
                "DRF_EXTENT($field), DRF_BASE($field), DRF_VAL($field, $data_c));";
            next;
        }
        
        next unless /\/\* (..)..V \*\//;
        push @main, "printf(\"LwManual::add_data_enum('$field', '$macro', \%d, \%d, '$1');\\n\", $macro, ".
                (defined $data ? "(DRF_VAL($field, $data_c) == $macro) ? 1 : 0);" : "0);");
    }
    close MANUAL;

    my @data = CEval::c_eval(\@header, \@main);
    $g_last_data = undef;
    return @data;
}

1;
