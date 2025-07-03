#!/home/utils/perl-5.22/5.22.0-022/bin/perl -w
###############################################################################
# $Id: / $ (Will be populated when it is pushed to the repository)
# $Date: 7/6/18 $
# $Author: Apoorv Gupta $
# Original Author: Sahana Seshadri (sahanas@lwpu.com)
###############################################################################

use strict;
#use warnings;
use Time::HiRes qw(usleep);
use Getopt::Long;

my $suffix = "";
my $chip = "";
my $hdr_filename = "";
my $patch_type = "";
my $bin_file = "";
my $map_file = "";
my $dmem_variable = "";
my $sig_file = "";
my $sig_string = "";
my $sig_type = "";
my $falcon_map_file = "";
my $falcon_dmem_variable = "";
my $app_variable = "";
my $app_index = 0;
my $verbose = 0;

GetOptions("suffix=s"                       => \$suffix,
        "chip=s"                            => \$chip,
        "hdr=s"                             => \$hdr_filename,
        "patch_type=s"                      => \$patch_type,
        "bin_file=s"                        => \$bin_file,
        "map_file=s"                        => \$map_file,
        "dmem_variable=s"                   => \$dmem_variable,
        "sig_file=s"                        => \$sig_file,
        "sig_string=s"                      => \$sig_string,
        "sig_type=s"                        => \$sig_type,
        "falcon_map_file=s"                 => \$falcon_map_file,
        "falcon_dmem_variable=s"            => \$falcon_dmem_variable,
        "app_variable=s"                    => \$app_variable,
        "app_index=o"                       => \$app_index,
        "verbose"                           => \$verbose);

# do we have everything specified?
if ($hdr_filename eq "") { exit_error("use --hdr to specify Header file"); }
if ($patch_type eq "") { exit_error("use --patch_type to specify patch type"); }
if ($map_file eq "") { exit_error("use --map_file to specify map file"); }
if ($dmem_variable eq "") { exit_error("use --dmem_variable to specify dmem_variable which needs to be replaced"); }


# Global Variables

my $renamedfile = $bin_file."_copy";
my $renamed_hdr_filename = $hdr_filename."_copy";

my $HEADER_ARRAY_NAME              = "soe_ucode_header_".$chip."_".$suffix;
my $DATA_ARRAY_NAME                = "soe_ucode_data_".$chip."_".$suffix;

# Constants
use constant APP_BASE_INDEX                 => 3;
use constant IMAGE_INDEX                    => 0;
use constant BL_NS_INDEX                    => 1;
use constant BL_HS_INDEX                    => 2;
use constant NUM_APPS_INDEX                 => 1;
use constant APP_ITEM_COUNT                 => 8;


my %appItemIndex =
(
    APP_VERSION                    => 0,
    APP_CODE_START_OFFSET          => 1,
    APP_CODE_SIZE                  => 2,
    APP_CODE_IMEM_OFFSET           => 3,
    APP_CODE_IS_SELWRE             => 4,
    APP_DATA_START_OFFSET          => 5,
    APP_DATA_SIZE                  => 6,
    APP_DATA_DMEM_OFFSET           => 7
);

use constant APP_VERSION                    => 0;
use constant APP_CODE_START_OFFSET          => 1;
use constant APP_CODE_SIZE                  => 2;
use constant APP_CODE_IMEM_OFFSET           => 3;
use constant APP_CODE_IS_SELWRE             => 4;
use constant APP_DATA_START_OFFSET          => 5;
use constant APP_DATA_SIZE                  => 6;
use constant APP_DATA_DMEM_OFFSET           => 7;

use constant BL_DMEM_BASE_NAME          => "dataBaseOffset";
use constant FALCON_DMEM_BASE_NAME      => "dataBaseDMEM";

use constant NUM_BYTES_PER_DWORD   => 4;
use constant NUM_DWORDS_PER_LINE   => 8;

# More Constants for colors/effects
use constant BOLD                                   => "\033[1m";
use constant GREEN                                  => "\033[32m";
use constant RED                                    => "\033[31m";
use constant YELLOW                                 => "\033[33m";
use constant UNDERLINE                              => "\033[4m";
use constant NORMAL                                 => "\033[0m";
use constant LWRSOR_UP                              => "\033[1A";
use constant KILL_LINE                              => "\033[K";



###############################################################################
# subroutines
###############################################################################

sub  trim { my $s = shift; $s =~ s/^\s+|\s+$//g; return $s };

sub makehex { my $val = $_[0]; return sprintf("%08x",$val); }

########################### print_debug #######################################
sub print_debug
{
    if($verbose){
        print("DEBUG: @_\n");
    }
}

########################### fatal_error #######################################
sub fatal_error
{
     if($bin_file ne ""){
        rename $renamedfile,$bin_file;        
    }
    if($hdr_filename ne ""){
        rename $renamed_hdr_filename,$hdr_filename;
    }
    die "\n$0 ERROR: @_\n\n";
}

####################################rename#####################################
sub rename_files()
{
    if($bin_file ne ""){
        rename $bin_file,$renamedfile;        
    }
    if($hdr_filename ne ""){
        rename $hdr_filename,$renamed_hdr_filename;
    }
}

######################## delete_temp_files ####################################
sub delete_temp_files()
{
    print_debug("Deleting $renamedfile and $renamed_hdr_filename");
    unlink $renamedfile;
    unlink $renamed_hdr_filename;
}

###########################get the address#####################################
sub get_word
{
    my ($map_file, $dmem_variable, $dmem_base) = @_;
    my $varFound = 0;
    my $baseFound = 0;
    my $varAddress;
    my $baseAddress;
    open(FILE, $map_file) or die "$!";
    while (<FILE>) {
        if(($_ =~ m/\b_$dmem_variable\b/)) {
            if ($varFound == 1) {
                fatal_error("Duplicate variable $dmem_variable found in $map_file");
            }
            $varAddress = substr($_,0,8);
            print_debug($dmem_variable . " variable found at address 0x".$varAddress." in the DMEM\n");
            $varFound = 1;
        }
        if(($_ =~ m/\b_$dmem_base\b/)) {
            if ($baseFound == 1) {
                fatal_error("Duplicate variable $dmem_base found in $map_file");
            }
            $baseAddress = substr($_,0,8);
            print_debug($dmem_base . " variable found at address 0x".$baseAddress." in the DMEM\n");
            $baseFound = 1;
        }
    }
    close FILE;
    if ($varFound == 0){
        fatal_error("DMEM variable $dmem_variable not found in $map_file");
    }
    if($baseFound == 0){
        fatal_error("DMEM base variable $dmem_base not found in $map_file");
    }
    my $varOffset = hex($varAddress) - hex($baseAddress);
    print_debug("varOffset is ". $varOffset. "\n");
    return ($varOffset, $baseAddress, $varAddress);
}

###############################parse_file######################################
sub parse_sig_file
{
    my $incr = 0;
    my $replacing_string = "";
    open(FILE2, $sig_file) or die "$!";
    if (grep{/$sig_string/} <FILE2>) {
        open(FILE3,$sig_file) or die "$!";
        my @lines = <FILE3>;
        my $line;

        foreach $line (@lines) {
            $incr++;
            if($line =~ m/$sig_string/) {
                if ($sig_type =~ "ls") {
                    if($sig_type eq "ls0") {                      #IMEM
                        $replacing_string = $lines[$incr+1] ;
                        $replacing_string =~ s/^\s+|\s+$//g;
                        $replacing_string =~ s/[{}]//g;
                    }
                    elsif($sig_type eq "ls1") {                   #DMEM
                        $replacing_string = $lines[$incr+2] ;
                        $replacing_string =~ s/^\s+|\s+$//g;
                        $replacing_string =~ s/[{}]//g;
                    }
                    elsif($sig_type eq "ls3") {                   #kdf
                        $replacing_string = $line;
                        $replacing_string =~ s/^\s+|\s+$//g;
                        $replacing_string =~ s/[{}]//g;
                    }
                }
                else {
                    $replacing_string = $lines[$incr] ;
                    $replacing_string =~ s/^\s+|\s+$//g;
                }
                print_debug("Parsed signature is $replacing_string from file $sig_file");
            }
        }
    }
    else {
        close FILE2; 
        close FILE3;  
        fatal_error("String $sig_string not found in file $sig_file");
    }
    close FILE2; 
    close FILE3;
    return $replacing_string;  
}


##########################Split the argument##################################
sub arg_split
{
    my ($replacing_string, $type) = @_; 
    my @data;
    my @string = split (/\,/,$replacing_string);
    my $temp;
    my $cnt = 0;

    foreach my $n (@string) {
        $n =~ s/^\s+|\s+$//g;
        if ($type eq "hs") {
            ++$cnt;
            print_debug("HS Sig: $n");
            for (my $i = 8; $i > 0; $i-=2) {
                $temp = substr($n,$i,2);
                print_debug("HS Sig Byte $i: $temp");
                push (@data, $temp);
            }
        } elsif( $type =~ "ls") {
            ++$cnt;
            $temp = substr($n,2,2);
            print_debug("LS Sig: $temp");
            push (@data, $temp);
        }
        else{
            ++$cnt;
            print_debug("DMEM Patch: $n");
            for (my $i = 8; $i > 0; $i-=2) {
                $temp = substr($n,($i-2),2);
                print_debug("DMEM Byte $i: $temp");
                push (@data, $temp);
            }
        }
    }
    if ((($type eq "hs") && ($cnt != 4)) ||
        (($type =~ "ls") && ($cnt != 16))||
        (($type eq "dmem_patch") && ($cnt != 1))
        ) {
        fatal_error("Improperly formatted input string cnt=$cnt.  Should be 4 for HS, 16 for LS and 1 for DMEM Patch");
    }

    if( $type ne "dmem_patch"){
        my $stringcount = $#data + 1;
        if ($stringcount != 16) {
            fatal_error("Bytecount expected=16 actual=$stringcount");
        }
    }
    return @data;
}

############################replace binary#####################################
sub patch_binary
{
    my ($start, $renamedfile, $bin_file, @data) = @_;
    my $count = $#data +1;
    my $end = $count + $start;
    my $in = $renamedfile;    
    $| = 1;
    my $out = $bin_file;
    my $byteCount = 0;
    open(IN, "< $in") or die "$!";
    binmode(IN);
    open(OUT, "> $out")or die "$!";
    binmode(OUT);
    my $t = 0;
    while (read(IN,$b,1))
    {  
        my( $hex ) = unpack( 'H*', $b );
        if ($byteCount >=$start && $byteCount <$end)
        {
            $hex = $data[$t++];
            print_debug(sprintf("substituting addr 0x%08x = 0x%02s", $byteCount, $hex));
        }
        my( $out ) = pack( 'H*', $hex );    
        print(OUT $out);
        $byteCount++;
    }
    close(IN);
    close(OUT);
    print_debug("New binary image $out is created with byte count = $byteCount");
}

########################### replace_header_file ###############################
sub patch_header_file
{
    my ($hdr_offset, $renamedfile, $hdr_file, @data) = @_;

    my $array_found = 0;
    my $offset = 0;
    my $insert_done = 0;
    my $close_found = 0;
    my $count = $#data +1;

    my $i;
    my $j;
    my $tmp;

    my $dword_addr;
    my @data_line;
    my @dword_data;
    my @dword_offset;

    if( ($count%4) != 0){
        fatal_error("Unexpected number of bytes in data:" . $count);
    }

    print_debug("dmem_addr = $hdr_offset");

    for ($i = 0; $i < $count/4; $i++) {
        $dword_data[$i] = "";
        for ($j = 0; $j < 4; $j++) {
            $dword_data[$i] = $data[(NUM_BYTES_PER_DWORD*$i)+$j] . $dword_data[$i];
        }
        $dword_data[$i] = "0x" . $dword_data[$i];
        $dword_offset[$i] = $hdr_offset + (NUM_BYTES_PER_DWORD * $i);
        print_debug("Data offset $dword_offset[$i] = $dword_data[$i]");
    }

    open(NEWFILE, '>', $hdr_filename) or die "$!";

    open(HDRFILE, $renamed_hdr_filename) or die "$!";

    my @img = <HDRFILE>;
    $_ = "@img";
    my $arr_name = $DATA_ARRAY_NAME;
    if (! /$arr_name\[\]\s*=\s*{([0-9a-fx,\s]*)/i) { fatal_error("invalid header file for $arr_name"); }

    my @inData = split /,/, $1;

    if( ($#inData % NUM_DWORDS_PER_LINE) != 0){
        fatal_error("Unexpected number of DWORDS in header file");
    }

    my $start = $hdr_offset/NUM_BYTES_PER_DWORD;
    for ($i = 0; $i < $count/4; $i++) {
        $inData[$start+$i] = " ".$dword_data[$i];
        print_debug("Substitued at index ".($start+$i)." with content ".$dword_data[$i]);
    }
    close(HDRFILE);

    open(HDRFILE, $renamed_hdr_filename) or die "$!";

    while (<HDRFILE>) {
        if ($_ =~ $DATA_ARRAY_NAME) {
            $array_found = 1;
        }
        elsif($insert_done && !$close_found && $_ =~"};"){
            $close_found = 1;
        }
        elsif($array_found && !$insert_done){
            my $total = $#inData;
            my $replaced;
            for ($i = 0; $i <$total; $i += NUM_DWORDS_PER_LINE){
                $replaced = "   ";
                for($j =0 ; $j < NUM_DWORDS_PER_LINE; $j++){
                    $replaced .= trim($inData[$i+$j]);
                    $replaced .= ", ";
                }
                print NEWFILE $replaced;
                print NEWFILE "\n";
            }
            $insert_done = 1;
            next;
        }elsif ($_ =~ $DATA_ARRAY_NAME) {
            $array_found = 1;
        }
        elsif ($insert_done && !$close_found){
            #Skip extra data array content
            next;
        }
        print NEWFILE $_;
    }

    if (!$close_found) {
        fatal_error("DMEM insertion was unsuccessful");
    }

    close(HDRFILE);
    close(NEWFILE);
}

########################### get_os_data_offset ################################
sub get_data_offset
{
    my ($renamed_hdr_filename, $array_name, $data_offset_entry_num ) = @_;
    my $array_found = 0;
    my $hdr_entry = 0;
    my $data_offset = "";

    open(HDRFILE, $renamed_hdr_filename) or die "$!";

    # find where dmem image starts in the hdrfile data array
    # this is located in "OS Data Offset", entry 2 of array
    while (<HDRFILE>) {
        if (($hdr_entry == $data_offset_entry_num) && ($data_offset eq "")) {
            $data_offset = $_;
            # remove newline
            chomp $data_offset;
            # pull out the digits
            $data_offset =~ s/\D//g;
            print_debug("Data_offset = $data_offset");
            # exit while loop, we are done
            close(HDRFILE);
            return($data_offset);
        } elsif ($array_found == 1) {
            ++$hdr_entry;
        } elsif ($_ =~ $array_name) {
            $array_found = 1;
        }
    }
    close(HDRFILE);
    fatal_error("Data Offset not found");
}

###############################################################################

# Main: Call Functions

my $replacing_string;
my $offset;
my $baseAddress;
my $varAddress;
my $index = 0;

rename_files();

# First get the offset and varAddress of DMEM variable in boot binary image to patch
($offset, $baseAddress, $varAddress) = get_word($map_file, $dmem_variable, BL_DMEM_BASE_NAME);    

if(($patch_type eq "LS_PATCH") || ($patch_type eq "HS_PATCH")){
    # This is for parsing sig file
    $replacing_string = parse_sig_file();    
}
elsif($patch_type eq "DMEM_PATCH_VAR"){
    # This is for reading the content from SOE IMage MAP file and add it back to BL DMEM
    my ($offset1, $baseAddress1, $varAddress1) = get_word($falcon_map_file, $falcon_dmem_variable, FALCON_DMEM_BASE_NAME);
    $replacing_string = $varAddress1;
    $sig_type = "dmem_patch";
}
elsif($patch_type eq "DMEM_PATCH_VAR_HEADER"){
    # This is for reading from header file and patch it to BL DMEM
    $index = APP_BASE_INDEX+($app_index * APP_ITEM_COUNT) + $appItemIndex{$app_variable};
    print_debug("Index for $app_variable is $index\n");
    $replacing_string = makehex(get_data_offset($renamed_hdr_filename, $HEADER_ARRAY_NAME, $index));
    $sig_type = "dmem_patch";
}

my @data = arg_split($replacing_string, $sig_type);

$index = APP_BASE_INDEX + ( BL_NS_INDEX * APP_ITEM_COUNT) + $appItemIndex{APP_DATA_START_OFFSET};
print_debug("Index for APP_DATA_START_OFFSET is $index\n");
my $hdr_offset = get_data_offset($renamed_hdr_filename, $HEADER_ARRAY_NAME, $index);

print_debug("Offset from header is $hdr_offset\n");
print_debug("Count of data is ". ($#data + 1) ."\n");

patch_binary($hdr_offset + $offset, $renamedfile, $bin_file, @data);

patch_header_file($hdr_offset + $offset, $renamed_hdr_filename, $hdr_filename, @data);

delete_temp_files();
exit;
