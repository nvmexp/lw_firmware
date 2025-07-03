#!/usr/bin/perl -w
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2014 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

use Time::Local;

my $bin_file = "";
my $verbose = 0;
my $cheetah = 0;
my $path_vpm = "/home/msdec/tools/falcon-gcc/Linux/080214_2243196/bin";

MAIN:
{
  my $arg = "";
  my $hdr_file = "", my $dmp_file = "";
  my $ucode_img0 = "", my $ucode_size0 = 0, $ucode_offset0 = 0; 
  my $ucode_img1 = "", my $ucode_size1 = 0, $ucode_offset1 = 0; 
  my $total_img  = "", my $total_size  = 0;
  my $image_base = "0x0000";
  my $image_name = "bl";

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
    elsif ($arg eq "-cheetah") {
      $cheetah = 1;
    }
    elsif ($arg eq "-base") {
      $image_base = shift || exit_error("address expected after -base");
    }
    elsif ($arg eq "-array") {
      $image_name = shift || exit_error("string expected after -array");
    }
    else {
      exit_error("invalid argument '$arg'");
    }
  }

  # do we have everything specified?
  if ($dmp_file eq "") { exit_error("use -dump to specify microdump file"); }
  if ($bin_file eq "") { exit_error("use -bin to specify binary file"); }

  system("$path_vpm/dumpexec -img $dmp_file $bin_file");

  # read ucode
  $ucode_offset0 = 0;
  $ucode_img0  = get_ucode_img($dmp_file,0);
  $ucode_size0 = ($ucode_img0 =~ s/x/x/g);
  ($ucode_img0, $ucode_size0) = pad_to_256_aligned($ucode_img0, $ucode_size0);
  $total_img = $ucode_img0;
  $total_size = $ucode_size0;

  # read data
  $ucode_offset1 = $total_size;
  $ucode_img1  = get_ucode_img($dmp_file,1);
  $ucode_size1 = ($ucode_img1 =~ s/x/x/g);
  ($ucode_img1, $ucode_size1) = pad_to_256_aligned($ucode_img1, $ucode_size1);
  $total_img = "$total_img"."$ucode_img1";
  $total_size += $ucode_size1;

  # create header file
  if ($hdr_file ne "") {
      write_hdr($hdr_file, $total_img, $total_size, $image_base, $image_name,
          $ucode_offset0, $ucode_size0, $ucode_offset1, $ucode_size1
      );
  }
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
  my ($hdr_file, $ucode_img, $ucode_size, $image_base, $image_name, $code_offset, $code_size, $data_offset, $data_size) = @_;
  $ucode_img =~ s/0x([0-9a-f]{2}),\s*0x([0-9a-f]{2}),\s*0x([0-9a-f]{2}),\s*0x([0-9a-f]{2})/0x$4$3$2$1/gi;
  $ucode_img =~ s/\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*(0x[0-9a-f]{8}),\s*/    $1, $2, $3, $4, $5, $6, $7, $8,\n/g;

  my ($release_date, $release_ts) = calc_timestamps();

  open SRC_FILE, ">$hdr_file" || exit_error("cannot create output source file '$hdr_file'");

  print SRC_FILE "/*"."\n";
  print SRC_FILE " * _LWRM_COPYRIGHT_BEGIN_"."\n";
  print SRC_FILE " *"."\n";
  print SRC_FILE " * Copyright 2019 by LWPU Corporation.  All rights reserved.  All"."\n";
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
  print SRC_FILE "#include \"rmflcnbl.h\""."\n";
  print SRC_FILE "#include \"lwtypes.h\""."\n";
  print SRC_FILE ""."\n";
  print SRC_FILE "LwU32 ".$image_name."[] = {\n";
  print SRC_FILE $ucode_img;
  print SRC_FILE "};\n\n";
  print SRC_FILE "RM_FLCN_BL_DESC ".$image_name."_desc = {\n";
  print SRC_FILE "    ".sprintf("0x%x", hex($image_base)>>8).", // blStartTag\n";
  print SRC_FILE "    0, // blDmemDescLoadOff - Unused\n";
  print SRC_FILE "    {\n";
  print SRC_FILE "        ".sprintf("0x%x", $code_offset).", // blCodeOffset\n";
  print SRC_FILE "        ".sprintf("0x%x", $code_size).", // blCodeSize\n";
  print SRC_FILE "        ".sprintf("0x%x", $data_offset).", // blDataOffset\n";
  print SRC_FILE "        ".sprintf("0x%x", $data_size)."  // blDataSize\n";
  print SRC_FILE "    }\n";
  print SRC_FILE "};\n";
  print SRC_FILE ""."\n";

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
  print "perl ".__FILE__." [-h] -array name -base address [-hdr hdrfile] -dump dmpfile -binary binfile\n";

  print "   -h             print this help\n";
  print "   -array name    name of array for code image (eg: msvld_g98)\n";
  print "   -hdr hdrfile   generate output header file (eg: msvld_code.h)\n";
  print "   -dump dmpfile  dumpexec output (e.g. c:/mypath/msvldFALCONOS_dump)\n";
  print "   -bin  binfile  binary input (eg: c:/mypath/msvldFALCONOS)\n";
  print "   -vppath path   path to dumpexec exelwtable (eg: /home/msdec/tools/falcon-gcc/current.Linux/bin/)\n";
  print "   -base address  virtual address where the code is loaded\n";
  print "   -v             print debug information \n";
  exit;
}

sub exit_error
{
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

