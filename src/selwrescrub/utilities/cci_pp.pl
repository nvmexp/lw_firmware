#!/home/gnu/bin/perl

# Original path is //hw/gpuvideo/msdec/bin/cci_pp.pl
# Modified by hshi (mostly cosmetic code cleanup)

# This function colwerts the user readable CCR fields into a 16 bit number used
# by the falcon assmembler
# This script requires at least one commad line option -h or the filename.scp
# it then parses the file and produces the output to test.s

# lookup tables, basically #defines
%Memtarget =
(
    "IMEM" => '1',
    "DMEM" => '0',
);

%KeywordHash =
(
    #"SVEC" => '9', #this is just a placeholder until the real values are added
);

my $outputString;
my $cciFormat;

#command line parsing
my $sec_enum_file = $ARGV[0];
my $input_file    = $ARGV[1];
my $output_file   = $ARGV[2];

sub get_op()
{
    my %Hash_Opcode = ();
    my @incl_variants;
    my $variant;

    open (OPCODE, "<$sec_enum_file") || die ("Cannot open $sec_enum_file: $!");
    while (<OPCODE>)
    {
       @list = split(/\s+/, $_);
       #spliting based on the spaces
       $ii = 0;
       while ($ii <= $#list)
       {
            if ($list[$ii] eq "#define" )
            {
                #we have found a #  define Now split again on "_"
                @op = split(/\_/,$list[$ii+1]);
                $list[$ii+1] =~ s/ENUM_SCP_OP_//g;
                # got the Opcode and it's encoding
                my $key = $list[$ii+1];
                my $value = $list[$ii+2];
                $Hash_Opcode{$key} = $value;
            }
            $ii = $ii + 1;
       }
    }
    return %Hash_Opcode;
    close (OPCODE);
}

#FILE HANDLING STUFF
open (INPUT , "<$input_file" ) || die (" Cannot open the  $input_file ");
$_ = $input_file;

$input_file =~ /(?:.*\/)*(\w+)\.\w+/g;
my $fileroot = $1;

@list = split(/\./,$input_file);
my %Opcode = get_op();

while (<INPUT>)
{
    $input = $_;
    @list = split(/\s+/, $input);
    $listIndex    = 0;
    $termIndex    = 0;
    $indexFound   = 0;
    $skipThisTerm = 0;
    foreach $word (@list) 
    {
        if (($input =~ /cci.*</  || $input =~ /ccr *</) && ($indexFound == 0) && ($list[$listIndex] =~ /</))
        {
            $indexFound = 1;
        }
        $termIndex = $termIndex + 1;
        if ($indexFound == 0) 
        {
            $listIndex = $listIndex + 1;
        }
    }

    if ($indexFound)
    {
        $list[$listIndex] =~ s/<?>?//g;    #strip out the '<' and '>' characters
        @operands  = split(/\,/,$list[$listIndex]);
        $inst_type = $list[$listIndex-1];
        $cciFormat = 1;
        if ($inst_type =~ /cci/) 
        {
            $IMM = 2 ** 15;
        } 
        elsif ($inst_type =~ /ccr/) 
        {
            $IMM = 0;
        } 
        elsif ($list[$listIndex-2] =~ /cci/) 
        {
            $IMM = 0;
            $cciFormat = 2;
        } 
        else 
        {
            die "what is $inst_type? instruction must be either ccr or cci to be assembled by this script.\n";
        }

        $N_OPER = $#operands;
        $count = 0;

        $Prev = 0; # this determines if the bits given are for the lower or the higher 16 bits
        while ($count <= $N_OPER )
        {
            @field = split(/\:/,$operands[$count]);

            if ($field[0] eq "ry" )
            {
                if ($cciFormat == 2) 
                { 
                    die "ry found in cci F5 format!\n"; 
                }
                if ($field[0] > 7 || $field[0] < 0) 
                {
                    die "ry must be between 0 and 7\n";
                }
                $IMM = $IMM + $field[1];
            }
            elsif ($field[0] eq "rx" )
            {
                if ($cciFormat == 2) 
                { 
                    die "rx found in cci F5 format!\n"; 
                }
                if ($field[0] > 7 || $field[0] < 0) 
                {
                    die "rx must be between 0 and 7\n";
                }
                $IMM = $IMM + $field[1] * (2 ** 4);
            }
            elsif ($field[0] eq "imm" )
            {
                if ($cciFormat == 2) 
                { 
                    die "imm found in cci F5 format!\n"; 
                }
                if ($field[0] > 31 || $field[0] < 0) 
                {
                    die "imm must be between 0 and 31\n";
                }
                $IMM = $IMM + $field[1] * (2 ** 4);
            }
            elsif ($field[0] eq "op" )
            {
                $found=0;
                while (my($key, $value) = each(%Opcode))
                {
                    if ($key eq $field[1])
                    {
                       if ($cciFormat==1) 
                       {
                          $IMM = $IMM + $value * (2 ** 10);
                       } 
                       else 
                       {
                          $IMM = $IMM + $value;
                       }
                       $found = 1;
                    }
                }
                if ($found == 0) 
                {
                    die("op: $field[1] not found!\n");
                }
            } 
            elsif ($cciFormat == 2) 
            {
                die "$field[0] is unknown for an F5 format cci\n";
            } 
            elsif($field[0] eq "tc") 
            {
                if ($field[0] > 31 || $field[0] < 0) 
                {
                    die "tc must be between 0 and 31\n";
                }
                $IMM = $IMM + $field[1];
            }
            elsif ($field[0] eq "sup") 
            {
                if ($field[0] > 1 || $field[0] < 0) 
                {
                    die "sup must be either 0 or 1\n";
                }
                $IMM = $IMM + $field[1] * (2 ** 5);
            }
            elsif ($field[0] eq "sc") 
            {
                if ($field[0] > 1 || $field[0] < 0)
                {
                    die "sc must be either 0 or 1\n";
                }
                $IMM = $IMM + $field[1] * (2 ** 6);
            }
            elsif ($field[0] eq "mt") 
            {
                $found = 0;
                while (my($key, $value) = each(%Memtarget)) 
                {
                    if ($key eq $field[1]) 
                    {
                        $found = 1;
                        $val = $value;
                    }
                }
                if ($found == 0) 
                { 
                    die "what is $field[1]? try IMEM or DMEM\n"; 
                }
                if ($val < 0 || $val > 1) 
                {
                    die "mt must be either IMEM or DMEM\n";
                }
                $IMM = $IMM + $val * (2 ** 7);
            }
            else 
            {
                die "field $field[0] is unknown.\n";
            }
            $count = $count + 1;
        }

        my $cci_op_value = sprintf("0x%x", $IMM);
        my $output_line = $_;
        $output_line =~ s/cc[ir](\s.*)<\s*$list[$listIndex]\s*>\s*/cci$1$cci_op_value/g;
        $outputString = $outputString.$output_line;
    }
    else
    {
        $outputString = $outputString.$_;
    }
}

close(INPUT);

if ($output_file eq "")
{
    open(OUTPUT, ">$fileroot.s" ) || die(" Cannot open the file $fileroot.s ");
    print OUTPUT $outputString;
    close(OUTPUT);
}
else
{
    open(OUTPUT, ">$output_file" ) || die(" Cannot open the file $output_file ");
    print OUTPUT $outputString;
    close(OUTPUT);
}
