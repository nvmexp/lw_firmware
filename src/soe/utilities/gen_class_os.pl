#!/home/utils/perl-5.22/5.22.0-022/bin/perl -w
use Time::Local;
use Getopt::Long;
use File::Basename;

my $blFilename   = "";
my $bootBinaryMapFile   = "";
my $chip ="";
my $suffix = "";
my $verbose = 0;
my $blDmp = "tmploader", my $imageDmp ="tmpimage";
my $dumpexec = "";

sub img_dbg_msg 
{
  my ($id, $offset, $size) = @_;
  if($verbose == 1) {
      print "Placing Image$id at offset=$offset size=$size\n";
  }
}

sub remove_whitespace($) {
    my $string = $_[0];
    $string =~ s/\s|\n|\r//g;

    return $string;
}

MAIN:
{
  my $arg = "";
  my $hdr_file = "";
  my $sign_bin = "";
  my $img_bin  = "";
  my $ucode_img0 = "", my $ucode_size0 = 0; 
  my $ucode_img1 = "", my $ucode_size1 = 0; 
  my $ucode_img2 = "", my $ucode_size2 = 0; 
  my $total_img  = "", my $total_size  = 0;

  my $blImemOffs   = 0;
  my $blDataOffs  = 0; 

  GetOptions("suffix=s"                       => \$suffix,
           "bootBinaryFilename=s"             => \$blFilename,
           "bootBinaryMapFile=s"              => \$bootBinaryMapFile,
           "bootBinaryImemOffset=o"           => \$blImemOffs,
           "bootBinaryDataOffset=o"           => \$blDataOffs,
           "img-bfile=s"                      => \$img_bin,
           "img-signfile=s"                   => \$sign_bin,
           "hdr=s"                            => \$hdr_file,
           "chip=s"                           => \$chip,
           "dumpexec=s"                       => \$dumpexec,
           "verbose"                          => \$verbose);

  # what is remaining on the command-line should just be the elf-file
  die "Usage: flcnmkimg.pl [options] <elf-file>\n" unless (($#ARGV + 1) >= 1);
  $elf = shift @ARGV;

  # do we have everything specified?
  if ($chip eq "") { exit_error("use --chip to specify chip"); }
  if ($blFilename eq "") { exit_error("use --bootBinaryFilename to specify boot binary file"); }
  if ($bootBinaryMapFile eq "") { exit_error("use --bootBinaryMapFile to specify boot binary map file"); }

  system("$dumpexec -img $imageDmp $elf");

  # read ucode code dump for Falcon Image
  $ucode_img0  = get_ucode_img($imageDmp,0,0);
  $ucode_size0 = ($ucode_img0 =~ s/x/x/g);
  ($ucode_img0, $ucode_size0) = pad_to_256_aligned($ucode_img0, $ucode_size0);
  $total_img = $ucode_img0;
  img_dbg_msg(0, $total_size, $ucode_size0);
  $total_size = $ucode_size0;

  # read in ucode data dump for Falcon Image
  $ucode_img1  = get_ucode_img($imageDmp,1,0);
  $ucode_size1 = ($ucode_img1 =~ s/x/x/g);
  ($ucode_img1, $ucode_size1) = pad_to_256_aligned($ucode_img1, $ucode_size1);
  $total_img = "$total_img"."$ucode_img1";
  img_dbg_msg(1, $total_size, $ucode_size1);
  $total_size += $ucode_size1;

  ##Now, we will read boot binary
  system("$dumpexec -img $blDmp $blFilename");

  ##Read boot binary code
  $bl_img0  = get_ucode_img($blDmp,0,$blImemOffs);
  $bl_size0 = ($bl_img0 =~ s/x/x/g);
  ($bl_img0, $bl_size0) = pad_to_256_aligned($bl_img0, $bl_size0);
  $total_img ="$total_img"."$bl_img0";
  img_dbg_msg(0, $total_size, $bl_size0);
  $total_size += $bl_size0;

  ##Read boot binary data
  $bl_img1  = get_ucode_img($blDmp,1,$blDataOffs);

  $bl_size1 = ($bl_img1 =~ s/x/x/g);
  ($bl_img1, $bl_size1) = pad_to_256_aligned($bl_img1, $bl_size1, 0);
  $total_img ="$total_img"."$bl_img1";
  img_dbg_msg(1, $total_size, $bl_size1);
  $total_size += $bl_size1;

  if($verbose == 1) {
      print "Total Image Size = $total_size\n";
      print "ucode_size0 = $ucode_size0\n";
      print "ucode_size1 = $ucode_size1\n";
      print "bl_size0 = $bl_size0\n";
      print "bl_size1 = $bl_size1\n";
  }

  # create array of offsets
  my @offsets_array = create_offsets_array($ucode_size0,
                                          $ucode_size1,
                                          $bl_size0,
                                          $bl_size1,
                                          $blImemOffs,
                                          $blDataOffs,
                                          $total_size);

  #Now process the data in format required by header
  $total_img =~ s/0x([0-9a-f]{2}),\s*0x([0-9a-f]{2}),\s*0x([0-9a-f]{2}),\s*0x([0-9a-f]{2})/0x$4$3$2$1/gi;
  $total_img =~ s/\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*/    $1, $2, $3, $4, $5, $6, $7, $8,\n/g;

  # create source file
  if ($hdr_file ne "") { 
     write_hdr($hdr_file, $total_img, $total_size, @offsets_array); 
  }

  #Now process it for binary format
  $total_img =~ s/\s*0x([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2}),\s*/$4$3$2$1/gi;

  #Create binary file with offsets for LS Signing
  if($sign_bin ne ""){
    write_sign_bin($sign_bin, $total_img, @offsets_array);
  }
  #Create output binary file 
  if($img_bin ne ""){
    write_bin($img_bin, $total_img);
  }

  unlink $blDmp;
  unlink $imageDmp;

}


sub write_sign_bin
{
    my ($outFile, $total_img, @offsets_array) = @_; 
    my $codeOffset = $offsets_array[4];
    my $codeSize =   $offsets_array[5];
    my $dataOffset = $offsets_array[8];
    my $dataSize =   $offsets_array[9];
    my $totalSize =  $codeSize + $dataSize;

    if ($verbose == 1) {
        print "write_hdr_size inputs: $outFile, 0x" . makehex($codeOffset) . ", 0x" . makehex($codeSize) . ", 0x" . makehex($dataOffset) . ", 0x" . makehex($dataSize) . ", 0x" . makehex($totalSize) . "\n";
    }

    # dump the following 5 numbers required for codesigning: code offset, code size, data offset, data size, image size
    # data will be dumped in binary as 5 integers in little endian
    open my $fh, '>', $outFile;
    binmode $fh;
    print $fh pack('V', $codeOffset);
    print $fh pack('V', $codeSize);
    print $fh pack('V', $dataOffset);
    print $fh pack('V', $dataSize);
    print $fh pack('V', $totalSize);

    #Now dump the binary
    my $binUcode = pack 'H*', $total_img;
    print $fh $binUcode;

    close $fh;
}

sub write_bin
{
    my ($outFile, $total_img) = @_; 

    open my $fh, '>', $outFile;
    binmode $fh;

    #Now dump the binary
    my $binUcode = pack 'H*', $total_img;
    print $fh $binUcode;

    close $fh;
}

sub makehex { my $val = $_[0]; return sprintf("%x",$val); }
sub dec2hex { my $val = $_[0]; return $val;}

sub create_offsets_array
{
  my ($ucode_size0,
      $ucode_size1,
      $bl_size0,
      $bl_size1,
      $blImemOffs,
      $blDataOffs,
      $total_size) = @_;

  my @offsets_array;
  my $num_elems = 0;
  my $num_apps = 0;

  $offset = 0;

  my $image_size        =  $ucode_size0;
  @offsets_array = (@offsets_array, 1); #appVersion
  $num_elems += 1;

  @offsets_array = (@offsets_array, $offset, $image_size, 0); #CodeImemOffset is assumed to be 0
  $num_elems += 3;
  $offset += $image_size;

  @offsets_array = (@offsets_array, 0); #appCodeIsSelwre
  $num_elems += 1;  

  $image_data_size        = $ucode_size1; ## Complete Data section of SOE app will be OS Data
  @offsets_array = (@offsets_array, $offset, $image_data_size, 0); #DataDmemOffset is assumed to be 0
  $num_elems += 3;
  $offset += $image_data_size;

  $num_apps += 1; ##Image app added

  ## Now we need to offset boot binary

  my $blCodeSize = $bl_size0;
  my $bl_text_size = get_value_map($bootBinaryMapFile, "textSize");
  my $ovlHs_size   = get_value_map($bootBinaryMapFile, "imem_hsSize");
  
  if($ovlHs_size != 0){
    #.text should always be 256 byte aligned here
    if ($bl_text_size % 256 != 0) { exit_error("boot binary Text section is not 256 byte aligned\n"); }
  }

  @offsets_array = (@offsets_array, 1); #appVersion
  $num_elems += 1;

  @offsets_array = (@offsets_array, $offset, $bl_text_size, $blImemOffs);
  $num_elems += 3;
  $offset += $bl_text_size;

  @offsets_array = (@offsets_array, 0); #appCodeIsSelwre
  $num_elems += 1; 

  my $blDataSize = $bl_size1;
  @offsets_array = (@offsets_array, $offset + $ovlHs_size, $blDataSize, $blDataOffs);
  $num_elems += 3;

  $num_apps += 1; ##Boot binary NS APP added

  @offsets_array = (@offsets_array, 1); #appVersion
  $num_elems += 1;

  @offsets_array = (@offsets_array, $offset, $ovlHs_size, $blImemOffs + $bl_text_size);
  $num_elems += 3;
  $offset += $ovlHs_size;
  #Update offset for NS Data also
  $offset += $blDataSize;

  @offsets_array = (@offsets_array, 1); #appCodeIsSelwre
  $num_elems += 1; 

  @offsets_array = (@offsets_array, $offset, 0, 0); #No secure Data
  $num_elems += 3;
  $offset += 0;

  $num_apps += 1; ##Boot binary HS APP added

  @offsets_array = (1, $num_apps, $blImemOffs, @offsets_array); #Version, numAPPs and codeEntryPoint
  $num_elems += 3;

  if($verbose == 1) {print "Creating header file with following offsets:\n";}
  for($cnt=0;$cnt<$num_elems;$cnt++)
  {
      my $tmpsize = makehex($offsets_array[$cnt]);
      if($verbose == 1) {print "  0x$tmpsize\n";}
  }
  return @offsets_array;
}

sub roundup256
{
    my ($size) = @_;

    return (($size + 255) & ~0xFF);

}

sub getsize_roundup
{
    my($filename, $sectionName) = @_;

    my $tmpSize = getsize($filename, $sectionName);

    return roundup256($tmpSize);
}

sub getsize
{
  my ($filename, $sectionName) = @_;

  my $size = 0;

  my $cmd = "/usr/bin/readelf -S $filename |";
  open(READELF, $cmd) || die "Can't execute pipeline '$cmd'";
  while (<READELF>) {
    next unless m/PROGBITS/;
    next unless m/ \.$sectionName /;
    my @junk = split(/PROGBITS/);
    @junk = split(/ +/, $junk[1]);
    $size = hex($junk[3]);
  } 
  close(READELF);

  return $size;
}

sub get_value_map
{
    my ($map_file, $map_variable) = @_;  
    my $varValue;
    my $varFound = 0;
    open(FILE, $map_file) or die "$!";
    while (<FILE>) {
        if(($_ =~ m/\b_$map_variable\b/)) {
            if ($varFound == 1) {
                fatal_error("Duplicate variable $map_variable found in $map_file");
            }
            $varValue = substr($_,0,8);
            $varFound = 1;
        }
    }
    close(FILE);
    if( $varFound != 1){ fatal_error($map_variable." not found in map file"); }
    $varValue = hex($varValue);
    return $varValue;

}

sub pad_to_256_aligned
{
    my ($ucode_img, $ucode_size) = @_;
    $ucode_img = substr($ucode_img, 0, -2);
    if ($ucode_size & 0xff)
    {
      $diff = 256 - ($ucode_size & 0xff);
      $cnt = 0;
      while ($cnt < $diff)
      {
      if(($ucode_size + $cnt) % 16 == 0)
  {
    $ucode_img = "$ucode_img".",\n  "."0x00";
  }
  else
  {
    $ucode_img = "$ucode_img".", "."0x00";
  }    
        $cnt++;
      }
      $ucode_size += $diff;
    }    
    $ucode_img = "$ucode_img".", ";
    return ($ucode_img, $ucode_size);
}

sub write_hdr
{
  my ($hdr_file, $ucode_img, $ucode_size, @offsets_array) = @_;
  my ($release_date, $release_ts) = calc_timestamps();
  my $header_guard = "_SOE_UCODE_".(uc $chip)."_".(uc $suffix)."_H_";

  open SRC_FILE, ">$hdr_file" || exit_error("cannot create output source file '$hdr_file'");

  print SRC_FILE "/*"."\n";
  print SRC_FILE " * _LWRM_COPYRIGHT_BEGIN_"."\n";
  print SRC_FILE " *"."\n";
  print SRC_FILE " * Copyright 2018 by LWPU Corporation.  All rights reserved.  All"."\n";
  print SRC_FILE " * information contained herein is proprietary and confidential to LWPU"."\n";
  print SRC_FILE " * Corporation.  Any use, reproduction, or disclosure without the written"."\n";
  print SRC_FILE " * permission of LWPU Corporation is prohibited."."\n";
  print SRC_FILE " *"."\n";
  print SRC_FILE " * _LWRM_COPYRIGHT_END_"."\n";
  print SRC_FILE " */"."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE "/*"."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE "    DO NOT EDIT - THIS FILE WAS AUTOMATICALLY GENERATED"."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE "*/"."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE "#ifndef ".$header_guard."\n";
  print SRC_FILE "#define ".$header_guard."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE "const LwU32 soe_ucode_data_".$chip."_".$suffix."[] = {\n";
  print SRC_FILE $ucode_img;
  print SRC_FILE "};\n\n";

  # write out array of offsets and sizes
  #
  print SRC_FILE "// Array of APP offsets and sizes\n";
  print SRC_FILE "// Defined as follows:\n";
  print SRC_FILE "// StructureVersion\n";
  print SRC_FILE "// NumApps(N)\n";
  print SRC_FILE "// Code Entry Point\n";

  print SRC_FILE "// App   0 APP Version\n";
  print SRC_FILE "// App   0 Code Offset (In Blob)\n";
  print SRC_FILE "// App   0 Code Size\n";
  print SRC_FILE "// App   0 Code IMEM Offset\n";
  print SRC_FILE "// App   0 is secure (Boolean 1/0)\n";
  print SRC_FILE "// App   0 Data Offset (In Blob)\n";
  print SRC_FILE "// App   0 Data Size\n";
  print SRC_FILE "// App   0 Data DMEM Offset\n";

  print SRC_FILE "// App   1 APP Version\n";
  print SRC_FILE "// App   1 Code Offset (In Blob)\n";
  print SRC_FILE "// App   1 Code Size\n";
  print SRC_FILE "// App   1 Code IMEM Offset\n";
  print SRC_FILE "// App   1 is secure (Boolean 1/0)\n";
  print SRC_FILE "// App   1 Data Offset (In Blob)\n";
  print SRC_FILE "// App   1 Data Size\n";
  print SRC_FILE "// App   1 Data DMEM Offset\n";

  print SRC_FILE "// .     .   .  .\n";
  print SRC_FILE "// .     .   .  .\n";
  print SRC_FILE "// App   N-1 APP Version\n";
  print SRC_FILE "// App   N-1 Code Offset (In Blob)\n";
  print SRC_FILE "// App   N-1 Code Size\n";
  print SRC_FILE "// App   N-1 Code IMEM Offset\n";
  print SRC_FILE "// App   N-1 is secure (Boolean 1/0)\n";
  print SRC_FILE "// App   N-1 Data Offset (In Blob)\n";
  print SRC_FILE "// App   N-1 Data Size\n";
  print SRC_FILE "// App   N-1 Data DMEM Offset\n";  


  my @fields =
  (
     'version', 'numApps', 'codeEntryPoint',
     'appVersion',
     'appCodeStartOffset' , 'appCodeSize' , 'appCodeImemOffset' , 'appCodeIsSelwre' ,
     'appDataStartOffset' , 'appDataSize' , 'appDataDmemOffset' ,
  );

  print SRC_FILE "\n";
  print SRC_FILE "\n";
  print SRC_FILE "const LwU32 soe_ucode_header_".$chip."_".$suffix."[] = {\n";
  my $count = 0;
  my $app = 0;
  my $arrsize = @offsets_array;

  print SRC_FILE "    /*    .$fields[$count] = */ $offsets_array[$count],\n"; #version
  print SRC_FILE "    /*    .$fields[$count+1] = */ $offsets_array[$count+1],\n"; #numApps
  print SRC_FILE "    /*    .$fields[$count+2] = */ $offsets_array[$count+2],\n"; #codeEntryPoint

  my $num_apps = $offsets_array[1];
  $count += 3;
  use constant APP_ITEMS      => 8;
  use constant APP_BASE_INDEX => 3;

  my $appItemCount = 0;

  for($app = 0; $app < $num_apps; $app++)
  {
    for($appItemCount = 0; $appItemCount < APP_ITEMS; $appItemCount++)
    {
        print SRC_FILE "    /*    .$fields[APP_BASE_INDEX+$appItemCount] = */ $offsets_array[$count+$appItemCount],\n";
    }
    $count += APP_ITEMS;  
  }

  print SRC_FILE "};\n";
  my $ucode_size_dwords = $ucode_size / 4;
  print SRC_FILE "\nconst LwU32 soe_ucode_data"."_size_".$chip."_".$suffix." = $ucode_size_dwords;\n"; #ucode_size in DWORDS

  print SRC_FILE "\n#endif //".$header_guard."\n";

  close SRC_FILE;
}


sub get_ucode_img
{
  my ($ucode_dump, $code_type, $start_offset) = @_;
  my @img = (), my $img_b = "", my $img_d = "";

  # read ucode dump file
  if (!open DUMP_FILE, $ucode_dump) { exit_error("cannot open ucode dump file '$ucode_dump'"); }
  @img = <DUMP_FILE>;
  close DUMP_FILE;
  $code_type ="0x"."$code_type"."0000000";
  # extract $code_type image
  $_ = "@img";
  if (! /$ucode_dump\_$code_type\[\]\s*=\s*{([0-9a-fx,\s]*)/i) { exit_error("invalid ucode dump file (cannot find $code_type image)"); }

  my @inData = split /,/, $1;
  my $total = $#inData;
  if ($start_offset > $total ){ exit_error("Start offset is greater than memory content"); }
  @inData = @inData[$start_offset..$total];
  $_ = join(',',@inData);

  # finished
  return $_;
}

sub exit_error
{
    unlink $blDmp;
    unlink $imageDmp;
    die "\nerror: @_\n\n";
}

sub calc_timestamps
{
  my ($sec,$min,$hour,$mday,$mon,$year,$wday, $yday,$isdst)=gmtime(time); 
  my $release_date = sprintf("%04d%02d%02d",$year+1900,$mon+1,$mday);

  my $secs = timegm($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst);
  my $stsecs = timegm(0,0,0,1,0,2009,4, $yday, $isdst); #start from Jan 1 09
  my $release_ts = ($secs - $stsecs);

  return ($release_date, $release_ts);
}
