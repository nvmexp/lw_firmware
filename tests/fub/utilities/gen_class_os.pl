#!perl -w
use Time::Local;

my $bin_file = "";
my $verbose = 0;
my $cheetah = 0;
MAIN:
{
  my $arg = "";
  my $hdr_file = "", my $dmp_file = "", my $path_vpm = "$ELW{'LW_TOOLS'}/falcon-gcc/falcon6/6.4.0/Linux/falcon-elf-gcc/bin";
  my $ucode_img0 = "", my $ucode_size0 = 0; 
  my $ucode_img1 = "", my $ucode_size1 = 0; 
  my $ucode_img2 = "", my $ucode_size2 = 0; 
  my $total_img  = "", my $total_size  = 0;

  # process input parameters
  if (@ARGV == 0) { print_options(); die; }

  while ($arg = shift) {
    if ($arg eq "-h") {
      print_options();
      die;
    }
    elsif ($arg eq "-hdr") {
      # get source file
      $hdr_file = shift || exit_error("source file expected after -hdr");
    }
    elsif ($arg eq "-dump") {
      # get dump file
      $dmp_file = shift || exit_error("microdump file expected after -dump");
    }
    elsif ($arg eq "-bin") {
      # get binary file
      $bin_file = shift || exit_error("binary file expected after -bin");
    }
    elsif ($arg eq "-vppath") {
      # get binary file
      $path_vpm = shift || exit_error("path expected after -vppath");
    }
    elsif ($arg eq "-v") {
      # get binary file
      $verbose = 1;
    }
    elsif ($arg eq "-chip") {
      # get binary file
      $chip = shift || exit_error("chip name expected after -chip");
    }
    elsif ($arg eq "-cheetah") {
      $cheetah = 1;
    }
    elsif ($arg eq "-version") {
      # get ucode build version
      $version = shift || exit_error("ucode build version expected after -version");
    }
    else {
      exit_error("invalid argument '$arg'");
    }
  }

  # do we have everything specified?
  if ($dmp_file eq "") { exit_error("use -dump to specify microdump file"); }
  if ($bin_file eq "") { exit_error("use -bin to specify binary file"); }
  

  system("$path_vpm/dumpexec -img $dmp_file $bin_file");
  print "dos2unix $bin_file\n";
  #system("dos2unix $dmp_file");
  # read ucode dump for os and app code
  $ucode_img0  = get_ucode_img($dmp_file,0);
  $ucode_size0 = ($ucode_img0 =~ s/x/x/g);
  ($ucode_img0, $ucode_size0) = pad_to_256_aligned($ucode_img0, $ucode_size0);
  $total_img = $ucode_img0;
  $total_size = $ucode_size0;
  
  # read in app data image
  $ucode_img2  = get_ucode_img($dmp_file,2);
  $ucode_size2 = ($ucode_img2 =~ s/x/x/g);
  ($ucode_img2, $ucode_size2) = pad_to_256_aligned($ucode_img2, $ucode_size2);
  $total_img = "$total_img"."$ucode_img2";
  $total_size += $ucode_size2;
  
  # read in os data image
  $ucode_img1  = get_ucode_img($dmp_file,1);
  $ucode_size1 = ($ucode_img1 =~ s/x/x/g);
  ($ucode_img1, $ucode_size1) = pad_to_256_aligned($ucode_img1, $ucode_size1);
  $total_img = "$total_img"."$ucode_img1";
  $total_size += $ucode_size1;

  # create array of offsets
  my @offsets_array = create_offsets_array($ucode_size0, $ucode_size1, $ucode_size2, $total_size);
  # create source file
  if ($hdr_file ne "") { write_hdr($hdr_file, $total_img, $total_size, @offsets_array); }
}

sub makehex { my $val = $_[0]; return sprintf("%x",$val); }
sub dec2hex { my $val = $_[0]; return $val;}

sub create_offsets_array
{
  my $num_elems = 0;
  my ($ucode_size0, $ucode_size1, $ucode_size2, $total_size) = @_;
  my $offsets_array = "";
  $offsets_array = $total_size;
 
  # core OS 
  my $os_offset      = 0;
  
  my $os_size        =  getsize("start") + getsize("text");
  
  $os_size           = dec2hex( roundup256($os_size) );
    
  @offsets_array = (@offsets_array, $os_offset,$os_size);
  $num_elems+=2;
  my $osovl_size       = dec2hex(getsize_roundup("ovlos") + getsize_roundup("ovlos_init") + getsize_roundup("ovlsub_app") + getsize_roundup("ovlsub_mthd") + getsize_roundup("ovlsub_ctx") + getsize_roundup("ovlsub_aelpg"));
  my $os_data_size     = dec2hex(getsize("data"));
  $os_data_size        = roundup256($os_data_size);
  
  
  # Apps and their sub overlays
  my $numapps        = 0;
  my $app_offset     = dec2hex($os_offset + $os_size + $osovl_size);
  my $app_size;

  my $cnt;
  for($cnt = 1; $cnt < 2; $cnt++)
  {
    my $app_size       = dec2hex(getsize_roundup("imem_fub"));
    if ($app_size > 0)
    {
       @offsets_array = (@offsets_array, $app_offset, $app_size);
       $app_offset += $app_size;
       $num_elems+=2;
       $numapps++;
    }
  }

  # App Data  
  for($cnt = 1; $cnt < 2; $cnt++)
  {
    my $app_size       = dec2hex(getsize_roundup("data_fub"));
    if($app_size > 0)
    {
      @offsets_array = (@offsets_array, $app_offset, $app_size);
      $num_elems+=2;
    }
    $app_offset += $app_size;
  }
    
    # insert numapps into array
  @offsets_array = (splice(@offsets_array,0,2), $numapps, splice(@offsets_array,0,4*$numapps));
  $num_elems+=1;

  # OS Data
  # This has to be computed from size to account for other data overlays not in data_ovl section
  my $os_data_offset = $os_offset + $ucode_size0 + $ucode_size2;
   @offsets_array = (splice(@offsets_array,0,2), $os_data_offset, $os_data_size, splice(@offsets_array,0,1+4*$numapps));
   $num_elems+=2;

  # OS overlay
  @offsets_array = (@offsets_array, $os_offset+$os_size, $osovl_size);
  $num_elems+=2;
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
    my($sectionName) = @_;

    my $tmpSize = getsize($sectionName);

    return roundup256($tmpSize);
}

sub getsize
{
  my ($sectionName) = @_;

  my $size = 0;

  my $cmd = "/usr/bin/readelf -S $bin_file |";
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

sub pad_to_256_aligned
{
    my ($ucode_img, $ucode_size) = @_;
    #$ucode_img =~ s/[\r\t\n]//g;
    $ucode_img = substr($ucode_img, 0, -2);
    if ($ucode_size & 0xff)
    {
      $diff = 256 - ($ucode_size & 0xff);
#     print "padded $diff bytes\n";
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
  $ucode_img =~ s/0x([0-9a-f]{2}),\s*0x([0-9a-f]{2}),\s*0x([0-9a-f]{2}),\s*0x([0-9a-f]{2})/0x$4$3$2$1/gi;
  $ucode_img =~ s/\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*/    $1, $2, $3, $4, $5, $6, $7, $8,\n/g;

  my ($release_date, $release_ts) = calc_timestamps();
  
  open SRC_FILE, ">$hdr_file" || exit_error("cannot create output source file '$hdr_file'");

  print SRC_FILE "/*"."\n";
  print SRC_FILE " * _LWRM_COPYRIGHT_BEGIN_"."\n";
  print SRC_FILE " *"."\n";
  print SRC_FILE " * Copyright 2017 by LWPU Corporation.  All rights reserved.  All"."\n";
  print SRC_FILE " * information contained herein is proprietary and confidential to LWPU"."\n";
  print SRC_FILE " * Corporation.  Any use, reproduction, or disclosure without the written"."\n";
  print SRC_FILE " * permission of LWPU Corporation is prohibited."."\n";
  print SRC_FILE " *"."\n";
  print SRC_FILE " * _LWRM_COPYRIGHT_END_"."\n";
  print SRC_FILE " */"."\n";
  print SRC_FILE ""."\n";
  if($cheetah == 1) {
      print SRC_FILE "#if defined(RTAPI_SIMULATION_BUILD)"."\n";
      print SRC_FILE "#include \"runtest_msenc.h\""."\n";
      print SRC_FILE "#else"."\n";
  }
  if($cheetah == 1) {
      print SRC_FILE "#endif"."\n";
  }
  print SRC_FILE ""."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE "/*"."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE "    DO NOT EDIT - THIS FILE WAS AUTOMATICALLY GENERATED"."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE "*/"."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE "LwU32 fub_ucode_data_".$chip."[] = {\n";
  print SRC_FILE $ucode_img;
  print SRC_FILE "};\n\n";

  # write out array of offsets and sizes
  #
  print SRC_FILE "// array of code and data overlay offsets and sizes\n";
  print SRC_FILE "// Defined as follows:\n";
  print SRC_FILE "// OS Code Offset\n";
  print SRC_FILE "// OS Code Size\n";
  print SRC_FILE "// OS Data Offset\n";
  print SRC_FILE "// OS Data Size\n";
  print SRC_FILE "// NumApps (N)\n";
  print SRC_FILE "// App   0 Code Offset\n";
  print SRC_FILE "// App   0 Code Size\n";
  print SRC_FILE "// App   1 Code Offset\n";
  print SRC_FILE "// App   1 Code Size\n";
  print SRC_FILE "// .     .   .  .\n";
  print SRC_FILE "// .     .   .  .\n";
  print SRC_FILE "// App N-1 Code Offset\n";
  print SRC_FILE "// App N-1 Code Size\n";
  print SRC_FILE "// App   0 Data Offset\n";
  print SRC_FILE "// App   0 Data Size\n";
  print SRC_FILE "// App   1 Data Offset\n";
  print SRC_FILE "// App   1 Data Size\n";
  print SRC_FILE "// .   .   .  .\n";
  print SRC_FILE "// .   .   .  .\n";
  print SRC_FILE "// App N-1 Data Offset\n";
  print SRC_FILE "// App N-1 Data Size\n";
  print SRC_FILE "// OS Ovl Offset\n";
  print SRC_FILE "// OS Ovl Size\n";
  print SRC_FILE "// \n";
  print SRC_FILE "// \n";
  print SRC_FILE " LwU32 fub_ucode_header_".$chip."[] = {\n";
  my $count;
  my $arrsize = @offsets_array;
# print("array size: $arrsize\n");
  for($count = 0; $count < $arrsize; $count++)
  {
    print SRC_FILE "    $offsets_array[$count],\n";
  }
  
  print SRC_FILE "};\n";
  my $ucode_size_dwords = $ucode_size / 4;
  print SRC_FILE "\nLwU32 fub_ucode_data"."_size_".$chip." = $ucode_size_dwords;\n"; #ucode_size in DWORDS

  # print ucode build version
  print SRC_FILE "\n//\n// Ucode build version\n//\n";
  my @ucodeBuildVersions = split ";", $version;
  for (my $i=0; $i < @ucodeBuildVersions; $i++)
  {
    print SRC_FILE "#define ".$ucodeBuildVersions[$i]."\n";
  }

  close SRC_FILE;
}


sub get_ucode_img
{
  my ($ucode_dump, $code_type) = @_;
  my @img = (), my $img_b = "", my $img_d = "";

  # read ucode dump file
  if (!open DUMP_FILE, $ucode_dump) { exit_error("cannot open ucode dump file '$ucode_dump'"); }
  @img = <DUMP_FILE>;
  close DUMP_FILE;
      $code_type ="0x"."$code_type"."0000000";
  # extract $code_type image
  $_ = "@img";
  if (! /$ucode_dump\_$code_type\[\]\s*=\s*{([0-9a-fx,\s]*)/i) { exit_error("invalid ucode dump file (cannot find $code_type image)"); }

  # finished
  return $1;
}


sub print_options
{
  print "perl ".__FILE__." [-h] -array name [-hdr hdrfile] -dump dmpfile -binary binfile\n";
  
  print "   -h             print this help\n";
  print "   -array name    name of array for code image (eg: msvld_g98)\n";
  print "   -hdr hdrfile   generate output header file (eg: msvld_code.h)\n";
  print "   -dump dmpfile  dumpexec output (e.g. c:/mypath/msvldFALCONOS_dump)\n";
  print "   -bin  binfile  binary input (eg: c:/mypath/msvldFALCONOS)\n";
  print "   -vppath path   path to dumpexec exelwtable (eg: /home/msdec/tools/falcon-gcc/current.Linux/bin/)\n";
  print "   -v             print debug information \n";
  exit;
}


sub exit_error
{
#  if (USE_COLOR) {
#    die color("red bold"), "\nerror: @_\n\n", color("reset");
#  }
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
