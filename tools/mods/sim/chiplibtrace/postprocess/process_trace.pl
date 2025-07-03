#!/home/utils/perl-5.8.8/bin/perl -w

use strict;
use warnings;
use Getopt::Long;
use File::Spec::Functions qw(rel2abs);
use File::Basename;

use lib dirname(rel2abs($0));

use LwMan;

my $help  = 0;
my $chip  = "";
my $trace = "";

my @line_filters = ("Unable to get current GPU subdevice.  This should only happen during RmFindDevices");
my @str_filters = (" rd data must match captured data", " write operation");  

GetOptions(
    'help'     => \$help,
    'chip=s'   => \$chip,
    'trace=s'  => \$trace,
);

my @ops;
open (TRACE, "<$trace") or die "Can't open trace file: $trace"; 
chomp(@ops = <TRACE>);
close TRACE;

#
# Remove all ops that match @line_filters
#
foreach my $filter (@line_filters) 
{
    @ops = grep (!/$filter/, @ops);
}

#
# Remove sub-strings that match @str_filters
#
foreach my $filter (@str_filters) 
{
    s/$filter// for @ops;
}

#
# Get bar0 info and process unknowns
#
my @bar0_info = grep (/BAR0 base =/, @ops);
if (@bar0_info)
{
    # Extract bar0 base and size from log
    # Log string looks like:
    # BAR0 base = 0x0_A2000000,     size = 0x01000000
    $bar0_info[0] =~ m/.*BAR0 base = (0x0_[A-Fa-f0-9]+),     size = (0x0[A-Fa-f0-9]+).*/;
    my $bar0_base = hex $1;
    my $bar0_size = hex $2;
    @ops = map
    {
        # For each "rd/wr unknow" operation in the log
        # check if the address is withing bar0 range
        # if so, change "unknow" to "bar0" and address
        # to offset
        my $op = $_;
        if ($op =~ m/^.. unknow (0x[a-fA-F0-9]+) (.*)/)
        {
            my $s_adr = $1;
            my $i_adr = hex $1;
            if ($i_adr >= $bar0_base and $i_adr <= $bar0_base + $bar0_size)
            {
                $op =~ s/unknow/bar0/;
                my $i_offset = $i_adr - $bar0_base;
                my $s_offset = sprintf("0x%08x", $i_offset);
                $op =~ s/$s_adr/$s_offset/;
            }
        }
        $op;
    } @ops;
}

#
# Condense reads to same address
# with same data
#
my $i = 0;
my $size = $#ops;
while ( $i < $size ) 
{
    my $op = $ops[$i];

    # Remove trailing comments from operation 
    $op =~ s/ #.*//;

    if ($op =~ /^rd .*/) 
    {
        # Extract address and data from the read operation 
        $op =~ m/^rd [[:alpha:]]+[0-9]* (0x[a-fA-F0-9]+) (.*)/;
        my $adr = $1;
        my $data = $2;

        # Walk the log starting from current read operation
        my $j = $i+1;
        while ($j < $size) {
            my $op_n = $ops[$j];

            # Remove trailing comments from operation
            $op_n =~ s/ #.*//;

            # If we find a write operation we are done
            if ($op_n =~ /^wr/) 
            {
                last;
            }

            # If we find a read operation:
            # remove the operation from the log if
            # the address and data match the original operation
            # stop processing if the address matches but the data
            # is different
            if ($op_n =~ /^rd .*/)
            {
                $op_n =~ m/^rd [[:alpha:]]+[0-9]* (0x[a-fA-F0-9]+) (.*)/;
                if ($adr eq $1 && $data eq $2)
                {
                    splice @ops, $j, 1;
                    $size--;
                    next;
                } 
                elsif ($adr eq $1) 
                {
                    last;
                }
            }
            $j++;
        }
    }
    $i++;
}

#
# Change endiannes of data
# and pack into 32 bits chunks
#
#for (my $i=0; $i<=$#ops; $i++)
while(0)
{
    my $op = $ops[$i];
    if ($op =~ /^wr / || $op =~ /^rd /)
    {
        $op =~ m/^.. [[:alpha:]]+[0-9]* (0x[0-9a-fA-F]+)(( 0x[0-9a-fA-F]+)+)  #.*/;
        my $value = $2;
        $value =~ s/0x//g;
        $value =~ s/^ //;
        my @values = reverse(split(' ', $value));
        $value = join('', @values);
        @values = ($value =~ m/......../g);
        @values = map { "0x$_"} @values;
        $value = join(' ', @values);
        $op =~ s/(0x[0-9a-fA-F]+)(( 0x[0-9a-fA-F]+)+)  #/$1 $value  #/;
        $ops[$i] = $op;
    }
}


#
# Map bar0 registers 
#
my $uname = `whoami`;
my $cache_file;
chomp $uname;
if (-d "/home/$uname") {
    $cache_file = "/home/$uname/.lwman";
}
my %init;
$init{chip} = $chip;
$init{load} = $cache_file;

my $m = LwManual->new( %init );
sub manual : lvalue { $m }

# For each bar0 operation
# call LwMan's get_registers_by_offset to
# get the register name for the given offset
for (my $i=0; $i<=$#ops; $i++)
{
    my $op = $ops[$i];
    if ($op =~ /^wr bar0/ || $op =~ /^rd bar0/)
    {
        $op =~ m/^.. bar0 (0x[0-9a-fA-F]+)(( 0x[0-9a-fA-F]+)+)  #.*/;
        my $offset = $1;
        my $value = $2;
        my @regs = manual()->get_registers_by_offset(hex $offset);
        if ($#regs > 1 || !@regs) 
        {
            next;
        }
        my $name = $regs[0]->{name};
        if ($name)
        {
            $op =~ s/$offset/$name/ if $name;
            $ops[$i] = $op; 
        }
    }
}

#
# Print the processed trace
#
print join("\n", @ops);
print "\n";
