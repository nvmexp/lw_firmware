package Utils;
#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

# useful but it requires some perl modules to be included in the sw tree
#use Time::HiRes;

require Exporter;

@ISA = qw(Exporter AutoLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(

);

# --------------------------------------------------------------------

$VERSION = '0.02';

# Author: G.P. Saggese
# Date of last revision: 02-23-06

# global debugging level
our $DBG_LVL = 1;
# if != 0 -> does the system call
our $SYSCALL_EXEC = 1;

# --------------------------------------------------------------------
sub AppendFile {
  my ($fileName, $str, $lvl) = @_;
  open (IFH, ">>", "$fileName") or die "File $fileName does not exist.";
  print IFH $str;
  close IFH;
  &Note("NOTE: File $fileName written", $lvl);
}

# --------------------------------------------------------------------
sub PrintFile {
  my ($fileName, $str, $lvl) = @_;
  open (IFH, ">", "$fileName") or die "I can't write file '$fileName'";
  print IFH $str;
  close IFH;
  &Note("NOTE: File $fileName written", $lvl);
}

# --------------------------------------------------------------------
sub Test {
  print "Test\n";
  print "\@INC is @INC\n";
}

# --------------------------------------------------------------------
sub Line {
  my ($type, $lvl) = @_;
  if ($type eq 1) {
    &Note(
      "======================================================================",
      $lvl);
  } elsif ($type eq 1) {
    &Note(
      "**********************************************************************",
      $lvl);
  } elsif ($type eq 3) {
    &Note(
      "######################################################################",
      $lvl);
  } else {
    &Note(
      "----------------------------------------------------------------------",
      $lvl);
  }
}

# --------------------------------------------------------------------
sub Frame {
  my ($txt, $type, $lvl) = @_;
  &Line($type, $lvl);
  &Note($txt, $lvl);
  &Line($type, $lvl);
}

# --------------------------------------------------------------------

sub SysCallCaptureOutput {
  my ($cmd, $ptrTxt, $lvl) = (@_);

  my $str = "==> SysCall: $cmd";
  if ($SYSCALL_EXEC ne 0) {
    $str .= " : Exelwting";
  } else {
    $str .= " : Skipping";
  }
  Utils::Note($str, $lvl);
  #my $ret = system($cmd) unless ($SYSCALL_EXEC eq 0);
  #if ($EXEC_ABORT && $ret ne 0) {
  #  die "syscall: $cmd returned $ret";
  #}
  
  $$ptrTxt = "";
  open (LH, "$cmd|") or die "Error in exelwting $cmd";
  while (<LH>) {
    $$ptrTxt .= $_;
  }
  close LH;
  return 1;
}

# --------------------------------------------------------------------
# SysCall
# --------------------------------------------------------------------

sub SysCall {
  my ($cmd, $lvl, $EXEC_ABORT) = @_;

#  if (undefined($EXEC_ABORT)) {
#    $EXEC_ABORT = 0;
#  }

  my $str = "==> SysCall: $cmd";
  if ($SYSCALL_EXEC ne 0) {
    $str .= " : Exelwting";
  } else {
    $str .= " : Skipping";
  }
  Utils::Note($str, $lvl);
  my $ret = system($cmd) unless ($SYSCALL_EXEC eq 0);
  Utils::Note("SysCall ret-code is $ret", $lvl);
  if (($EXEC_ABORT ne 0) && $ret ne 0) {
    die "SYSCALL ERROR: exec_abort= $EXEC_ABORT , $cmd returned $ret";
  }
}

# --------------------------------------------------------------------
# note
# --------------------------------------------------------------------

sub PrintDebugInfo {
  print "DBG_LVL = $DBG_LVL\n";
}

sub Note { 
  my ($message, $verb_this) = @_;
  my @message;

  if ($message) {
    # Break into separate lines
    @message = split(/\n/,$message);
  }                             ##end of if

  # print if the verbosity passed is <= of the VERB_LEVEL
  if ($DBG_LVL >= $verb_this) {
    foreach (@message) {
      print STDOUT "$_\n";
    }                           ##end of foreach
  }                             ##end of if
}                               ##end of sub note

sub PrintBool {
  my ($bool) = @_;
  if ($bool eq 1) {
    return "YES";
  } else {
    return "NO";
  }
}

sub PrintArray {
  my ($ptr) = @_;
  my $tot = scalar(@$ptr);
  my $str;
  for (my $i = 0; $i < $tot; $i++) {
    $str .= ($i+1)." / $tot : ".($$ptr[$i])."\n";
  }
  return $str;
}

sub PrintLinearArray {
  my ($ptr) = @_;
  my $str = scalar(@$ptr)." : ( ";
  foreach my $elem (@$ptr) {
    $str .= "$elem, ";
  }
  $str .= ")";
  return $str;
}

sub PrintHash {
  my ($ptr) = @_;
  my $str = "";
  foreach my $key (keys %$ptr) {
    $str .= "${key} = ".($ptr->{$key})."\n";
  }
  return $str;
}

sub PrintHashKeys {
  my ($ptr) = @_;
  my $str = scalar(keys %$ptr)." : ( ";
  foreach my $key (keys %$ptr) {
    $str .= "${key} ";
  }
  $str .= ")";
  return $str;
}

sub AreYouSure {
  my ($msg) = @_;
  
  local( $| )= 1;
  print "$msg\n".
      "Are you *really* sure [yes/no]? ";
  my $resp= <STDIN>;
  if ($resp ne "yes\n") {
    print "It seems that you are not *really* sure...\n";
    die;
    return 1;
  } else {
    return 0;
  }
}

sub PressAnyKey {
  my ($msg) = @_;
  
  local( $| )= 1;
  print "$msg\nPress return to go ahead...";
  my $resp= <STDIN>;
}


# --------------------------------------------------------------------------
sub DiffTwoFiles {
  my ($file1, $file2) = @_;

  my ($sub, $dbg) = ("Utils::DiffTwoFiles", 5);

  if (!(-e $file1)) {
    &Note("WARNING in $sub: $file1 does not exist", 1);
    return -1;
  }
  if (!(-e $file2)) {
    &Note("WARNING in $sub: $file2 does not exist", 1);
    return -1;
  }
  
  my $cmd = "diff --brief ${file1} ${file2}";
  Utils::Note("Exelwting: $cmd", $dbg);

  my $res = 1;
  open (LH, "${cmd}|");
  my $line = <LH>;
  Utils::Note("diff= ", $dbg);
  if ($line =~ /differ/i) {
    $res = 1;
    Utils::Frame("ERROR: $cmd", 2);
    $cmd = "diff $file1 $file2";
    Utils::SysCall($cmd, 1, 1);
  } else {
    $res = 0;
  }
  close(LH);

  return $res;
}

# --------------------------------------------------------------------------
sub FileNamesWithExtension {
  my ($DIR, $START, $EXT, $arrPtr) = @_;

  my ($sub, $dbg) = ("FileNamesWithExtension", 3);
  &Note("$sub : DIR= $DIR, START= $START, EXT= $EXT", $dbg);

  @$arrPtr = ();

  # read all the files to analyze
  my $toGlob = "$DIR/${START}*${EXT}";
  my @filesWithExt = <${toGlob}>;
  &Note("$sub : $toGlob -> @filesWithExt", $dbg);
  
  for my $file (@filesWithExt) {
    my $name = $file;
    $name =~ s/^${DIR}\///;
    if ($EXT ne "") {
      $name =~ s/${EXT}$//;
    }
    push(@$arrPtr, $name);
  }
}

sub FileMustExist {
  my ($fn) = @_;

  die "I didn't find the file $fn" unless (-e $fn);
}

sub DirMustExist {
  my ($fn) = @_;

  die "I didn't find the dir $fn" unless (-d $fn);
}

sub AppendToArray {
  my ($ptr, $str) = @_;
  for (my $i = 0; $i < scalar(@$ptr); $i++) {
    $$ptr[$i] .= $str; 
  }
}

sub SysCallCaptureOutput {
  my ($cmd, $ptrTxt, $lvl) = (@_);

  my $str = "==> SysCall: $cmd";
  if ($SYSCALL_EXEC ne 0) {
    $str .= " : Exelwting";
  } else {
    $str .= " : Skipping";
  }
  Utils::Note($str, $lvl);
  #my $ret = system($cmd) unless ($SYSCALL_EXEC eq 0);
  #if ($EXEC_ABORT && $ret ne 0) {
  #  die "syscall: $cmd returned $ret";
  #}
  
  $$ptrTxt = "";
  open (LH, "$cmd|") or die "Error in exelwting $cmd";
  while (<LH>) {
    $$ptrTxt .= $_;
  }
  close LH;
  return 1;
}

sub FloatToStr {
  my ($num, $digits) = @_;
  my $fmtStr = "%.${digits}f";
  my $tmp = sprintf($fmtStr, $num);
  return $tmp;
}


# ------------------------------------------------------------------------
sub LocalTime {
  my @months = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
  my @weekDays = qw(Sun Mon Tue Wed Thu Fri Sat Sun);
  my ($second, $minute, $hour, $dayOfMonth, $month, $yearOffset, $dayOfWeek, $dayOfYear, $daylightSavings) = localtime();
  my $year = 1900 + $yearOffset;
  my ($month_, $day_) = ($month, $dayOfMonth);
  if ($month < 10) { $month_ = "0".$month; }
  if ($dayOfMonth < 10) { $day_ = "0".$dayOfMonth; }
  my ($hour_, $second_, $minute_) = ($hour, $second, $minute);
  if ($hour < 10) { $hour_ = "0".$hour; }
  if ($second < 10) { $second_ = "0".$second; }
  if ($minute < 10) { $minute_ = "0".$minute; }
  return "${hour_}:${minute_}:${second_}, $weekDays[$dayOfWeek] $months[$month] ${day_}, $year";
}

# ------------------------------------------------------------------------
sub GetTimeStamp {
  my ($second, $minute, $hour, $dayOfMonth, $month, $yearOffset, $dayOfWeek, $dayOfYear, $daylightSavings) = localtime();
  my $year = 1900 + $yearOffset;
  my ($month_, $day_) = ($month+1, $dayOfMonth);
  if ($month_ < 10) { $month_ = "0".$month_; }
  if ($dayOfMonth < 10) { $day_ = "0".$dayOfMonth; }
  my ($hour_, $second_, $minute_) = ($hour, $second, $minute);
  if ($hour < 10) { $hour_ = "0".$hour; }
  if ($second < 10) { $second_ = "0".$second; }
  if ($minute < 10) { $minute_ = "0".$minute; }
  return "${month_}-${day_}-${year}_${hour_}-${minute_}-${second_}";
}

our $Time_T0;

sub StartTime {
  my ($str, $lvl) = @_;

  my $time = &LocalTime();
  Utils::Note("$str *started* at $time\n", $lvl);
  #$Time_T0 = [Time::HiRes::gettimeofday];
  $Time_T0 = 0;
}

sub StopTime {
  my ($str, $lvl) = @_;

  #my $t1 = [Time::HiRes::gettimeofday];
  my $t1 = 0;

  #my $t0_t1 = Time::HiRes::tv_interval $Time_T0, $t1;
  my $t0_t1 = 0;
  my $txt = "$str *terminated* at ".&LocalTime().
    "\n\tElapsed time= $t0_t1 secs (".
      Utils::FloatToStr(($t0_t1/3600), 3)." hours)";
  Utils::Note($txt, $lvl);
}

sub Slurp {
  my ($fn, $ptr) = @_;
  $$ptr = "";
  open(FH, "<", $fn) or die "$fn does not exist";
  while(<FH>) {
    $$ptr .= $_;
  }
  close (FH);
}

sub CleanDirectory {
  my ($dir) = @_;
  DirMustExist($dir);
  my $cmd = "ls ${dir}/*";
  Utils::SysCall($cmd, 1, 1);
  Utils::AreYouSure("I'm about to overwrite the lwrr files...");
  my $cmd = "rm ${dir}/*";
  Utils::SysCall($cmd, 1, 0);
}

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
