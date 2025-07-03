#!/usr/bin/elw perl

use strict;
use Expect;
use Getopt::Long;
&Getopt::Long::config("permute", "passthrough");

my  $chip;
my  $chips;
my  @chips;
my  $dstDir = "";
my  $srcDir="";
my  $date="";
my  $romsubmitneeded = 0;         # initial value zero indicating no rom submission is needed.
my  $vbiosCL = 0;
my  $DESCRIPTION_DEFAULT = "<enter description here>";
my  $FILE_KEY = "Files:";
my  $specfile  = "specfile.tmp";
my  $description = "This is submitted by autosubmitvbios.pl.\n";;
my  $filelist;
my  $pendingCL;
my  $as2 = 1;       # default as2 true since that's what going to happen from now on.

my  %romlist;

my  %lw50roms = (
    "lw50n.rom"     => "g80fmod/sim",
    "lw50n321.rom"  => "g80fmod/sim",
    "lw50n333.rom"  => "g80fmod/sim",
    "lw50n700.rom"  => "g80fmod/sim",
    "lw50n712.rom"  => "g80fmod/sim",
    "lw50nopll.rom" => "g80fmodnopll/sim",
    "lw50ddr4.rom"  => "g80fmodddr4/sim",
    "isop50OJ.rom"  => "g80fpga/fpga",
);

my  %g82roms = (
    "g82n.rom"     => "g82fmod/sim",
    "g82n321.rom"  => "g82fmod/sim",
    "g82n333.rom"  => "g82fmod/sim",
    "g82n700.rom"  => "g82fmod/sim",
    "g82n712.rom"  => "g82fmod/sim",
    "g82nopll.rom" => "g82fmodnopll/sim",
    "g82ddr4.rom"  => "g82fmodddr4/sim",
);

my  %g84roms = (
    "g84n.rom"     => "g84fmod/sim",
    "g84n321.rom"  => "g84fmod/sim",
    "g84n333.rom"  => "g84fmod/sim",
    "g84n700.rom"  => "g84fmod/sim",
    "g84n712.rom"  => "g84fmod/sim",
    "g84nopll.rom" => "g84fmodnopll/sim",
    "g84ddr4.rom"  => "g84fmodddr4/sim",
    "g84ddr2.rom"  => "g84fmodddr2/sim",
);

my  %g86roms = (
    "g86n.rom"     => "g86fmod/sim",
    "g86n321.rom"  => "g86fmod/sim",
    "g86n333.rom"  => "g86fmod/sim",
    "g86n700.rom"  => "g86fmod/sim",
    "g86n712.rom"  => "g86fmod/sim",
    "g86nopll.rom" => "g86fmodnopll/sim",
    "g86ddr4.rom"  => "g86fmodddr4/sim",
    "g86ddr2.rom"  => "g86fmodddr2/sim",
);

my  %g88roms = (
    "g88n.rom"     => "g86fmod/sim",
    "g88n321.rom"  => "g86fmod/sim",
    "g88n333.rom"  => "g86fmod/sim",
    "g88n700.rom"  => "g86fmod/sim",
    "g88n712.rom"  => "g86fmod/sim",
    "g88nopll.rom" => "g86fmodnopll/sim",
    "g88ddr4.rom"  => "g86fmodddr4/sim",
    "g88ddr2.rom"  => "g86fmodddr2/sim",
);

my  %g92roms = (
    "g92n.rom"     => "g92fmod/sim",
    "g92n321.rom"  => "g92fmod/sim",
    "g92n333.rom"  => "g92fmod/sim",
    "g92n700.rom"  => "g92fmod/sim",
    "g92n712.rom"  => "g92fmod/sim",
    "g92nopll.rom" => "g92fmodnopll/sim",
    "g92ddr4.rom"  => "g92fmodddr4/sim",
    "g92ddr2.rom"  => "g92fmodddr2/sim",
);

my  %g94roms = (
    "g94n.rom"     => "g94fmod/sim",
    "g94n321.rom"  => "g94fmod/sim",
    "g94n333.rom"  => "g94fmod/sim",
    "g94n700.rom"  => "g94fmod/sim",
    "g94n712.rom"  => "g94fmod/sim",
    "g94nopll.rom" => "g94fmodnopll/sim",
    "g94ddr4.rom"  => "g94fmodddr4/sim",
    "g94ddr2.rom"  => "g94fmodddr2/sim",
);

my  %g96roms = (
    "g96n.rom"     => "g96fmod/sim",
    "g96n321.rom"  => "g96fmod/sim",
    "g96n333.rom"  => "g96fmod/sim",
    "g96n700.rom"  => "g96fmod/sim",
    "g96n712.rom"  => "g96fmod/sim",
    "g96nopll.rom" => "g96fmodnopll/sim",
    "g96ddr4.rom"  => "g96fmodddr4/sim",
    "g96ddr2.rom"  => "g96fmodddr2/sim",
);

my  %g98roms = (
    "g98n.rom"     => "g98fmod/sim",
    "g98n321.rom"  => "g98fmod/sim",
    "g98n333.rom"  => "g98fmod/sim",
    "g98n700.rom"  => "g98fmod/sim",
    "g98n712.rom"  => "g98fmod/sim",
    "g98nopll.rom" => "g98fmodnopll/sim",
    "g98ddr4.rom"  => "g98fmodddr4/sim",
    "g98ddr2.rom"  => "g98fmodddr2/sim",
);

my  %gt200roms = (
    "gt200n.rom"     => "gt200fmod/sim",
    "gt200n321.rom"  => "gt200fmod/sim",
    "gt200n333.rom"  => "gt200fmod/sim",
    "gt200n700.rom"  => "gt200fmod/sim",
    "gt200n712.rom"  => "gt200fmod/sim",
    "gt200nopll.rom" => "gt200fmodnopll/sim",
    "gt200ddr4.rom"  => "gt200fmodddr4/sim",
    "gt200ddr2.rom"  => "gt200fmodddr2/sim",
);

my  %gt206roms = (
    "gt206n.rom"     => "gt206fmod/sim",
    "gt206n321.rom"  => "gt206fmod/sim",
    "gt206n333.rom"  => "gt206fmod/sim",
    "gt206n700.rom"  => "gt206fmod/sim",
    "gt206n712.rom"  => "gt206fmod/sim",
    "gt206nopll.rom" => "gt206fmodnopll/sim",
    "gt206ddr4.rom"  => "gt206fmodddr4/sim",
    "gt206ddr2.rom"  => "gt206fmodddr2/sim",
);

my  %gt214roms = (
    "gt214n.rom"     => "gt214fmod/sim",
    "gt214n321.rom"  => "gt214fmod/sim",
    "gt214n333.rom"  => "gt214fmod/sim",
    "gt214n700.rom"  => "gt214fmod/sim",
    "gt214n712.rom"  => "gt214fmod/sim",
    "gt214nopll.rom" => "gt214fmodnopll/sim",
    "gt214ddr4.rom"  => "gt214fmodddr4/sim",
    "gt214ddr2.rom"  => "gt214fmodddr2/sim",
);

###############################################################################
#
# Subroutine to get date if there is no -date argument specified.
#
###############################################################################
sub get_date
{
    if($date eq "")
    {
        my  $exp_date = new Expect;
        $exp_date->raw_pty(1);
        $exp_date->spawn("date +%Y%m%d\n") or die "Cannot spawn date.\n";
        $exp_date->expect(1, '-re', '\d+');
        $date = $exp_date->match();
        $exp_date->hard_close();
    }
}

###############################################################################
#
# Subroutine to check chip and set up romlist properly.
#   exit if valid chip is not found.
#
###############################################################################
sub check_chip
{
    if($chip eq "lw50")
    {
        $as2 = 0;
        %romlist = %lw50roms;
    }
    elsif($chip eq "g82")
    {
        $as2 = 0;
        %romlist = %g82roms;
    }
    elsif($chip eq "g84")
    {
        $as2 = 0;
        %romlist = %g84roms;
    }
    elsif($chip eq "g86")
    {
        $as2 = 0;
        %romlist = %g86roms;
    }
    elsif($chip eq "g88")
    {
        $as2 = 0;
        %romlist = %g88roms;
    }
    elsif($chip eq "g92")
    {
        $as2 = 0;
        %romlist = %g92roms;
    }
    elsif($chip eq "g94")
    {
        $as2 = 0;
        %romlist = %g94roms;
    }
    elsif($chip eq "g96")
    {
        $as2 = 0;
        %romlist = %g96roms;
    }
    elsif($chip eq "g98")
    {
        $as2 = 0;
        %romlist = %g98roms;
    }
    elsif($chip eq "gt200")
    {
        %romlist = %gt200roms;
    }
    elsif($chip eq "gt206")
    {
        %romlist = %gt206roms;
    }
    elsif($chip eq "gt214")
    {
        %romlist = %gt214roms;
    }
    else
    {
        die "check_chip: $chip doesn't exist!\n";
    }
}

###############################################################################
#
# Subroutine to check source directory to see if rom file present.
#   exit if source directory or any rom is not found.
#
###############################################################################
sub check_src_dir
{
    my  $chipDir = "/home/emulab/hw/" . $chip;
    my  $lwrrentDir;
    my  $latestDir;         # The directory which has the biggest changelist number
    my  $latestCL = 0;

    if($srcDir ne "")
    {
        if(!(-e $srcDir))
        {
            # srcDir as a complete path doesn't exist.
            $chipDir = $chipDir . "/" . $srcDir;
            if(-e $chipDir)
            {
                $srcDir = $chipDir;
            }
            else
            {
                die "check_src_dir: Source directory $chipDir doesn't exist.\n";
            }
        }
    }
    else
    {
        opendir( CHIPDIR, $chipDir ) or die "check_src_dir: couldn't open directory $chipDir.\n";
        while( $lwrrentDir = readdir CHIPDIR )
        {
            if( $lwrrentDir =~ /\.(\d{4,})/ )
            {
                if($1 > $latestCL) 
                {
                    $latestDir = $lwrrentDir;
                    $latestCL = $1;
                }
            }
        }
        $srcDir = $chipDir . "/" . $latestDir;
    }
    print "Info: Source directory is $srcDir\n";
    
    $vbiosCL = $latestCL;
    print "Info: $chip VBIOS CL# is $vbiosCL\n";
}

###############################################################################
#
# Subroutine to check destinate dir.
#
###############################################################################
sub check_dst_dir
{
    if($dstDir eq "")
    {
        $dstDir = $ELW{TREEROOT} . "/sw/pvt/main_lw5x/diag/mods/sim/resources";
    }
    print "Info: Destination directory is $dstDir\n";
}

###############################################################################
#
# Subroutine to check roms.
#   If rom file doesn't exist in srcDir, exit.
#   If rom file write-able in dstDir, revert it.
#   If rom file exist, p4 sync to tot.
#   If rom file doesn't exist, p4 add.
#   If no rom difference encountered and no new rom, exit.
#
###############################################################################
sub check_rom
{
    my $rom;
    my $path;
    my $result;

    $romsubmitneeded=0;     # reset to 0.

    while(($rom, $path) = each %romlist)
    {
        my  $exp_rom = new Expect;
        if(!(-e "$srcDir/$path/$rom"))
        {
            die "ERROR: rom file $srcDir/$path/$rom doesn't exist.\n"
        }
        
        if(-w "$dstDir/$rom")
        {
            system("p4 revert $dstDir/$rom\n");
            if(-w "$dstDir/$rom")               # if it is still write-able means it is not in p4.
            {
                system("rm -f $dstDir/$rom\n");
            }
        }

        if(-e "$dstDir/$rom")
        {
            system("p4 sync $dstDir/$rom\n");
            $exp_rom->spawn("cmp $srcDir/$path/$rom $dstDir/$rom"); 
            $result = $exp_rom->expect(2, 'differ');
            if(defined($result))
            {
                $romsubmitneeded = 1;
                system("p4 edit $dstDir/$rom\n");
                system("cp $srcDir/$path/$rom $dstDir/$rom");
            }
            else
            {
                delete $romlist{$rom};
            }
        }
        else
        {  
            system("cp $srcDir/$path/$rom $dstDir/$rom");
            system("p4 add $dstDir/$rom\n");
            $romsubmitneeded=1;
        }
    }

    if(!$romsubmitneeded)
    {
        print "No rom difference encountered for $chip.\n";
    }
}

###############################################################################
#
# Subroutine to prepare changelist spec.
#
###############################################################################
sub prepare_change_list
{
    my  $rom;
    my  $path;
    my  $filestring;

    $description .= "\t$chip vbios images come from: " . $srcDir . "\n";
    if($vbiosCL)
    {
        $description .= "\t$chip vbios perforce changelist is: " . $vbiosCL . "\n";
    }

    while(($rom, $path) = each %romlist)
    {
        $filestring = "$dstDir/$rom";
        $filestring =~ s/.+(\/sw\/.+)/\t\/$1 #edit\n/;
        $filelist .= $filestring;
    }
}


###############################################################################
#
# Subroutine to create changelist with proper perforce change specification.
#
###############################################################################
sub create_change_list
{
    my  $result;

    (my $changespec = `p4 change -o`) =~ s/$DESCRIPTION_DEFAULT/$description/;

    if ( not( $changespec =~ s/^(\s*$FILE_KEY).*/$1\n/ims ) )
    {
        $changespec .= "\n$FILE_KEY\n";
    }

    open( SPECFILE, ">$specfile" ) or die "ERROR: can't open $specfile for writing.\n";
    print SPECFILE $changespec;
    print SPECFILE $filelist;
    close( SPECFILE );

    my  $exp_change = new Expect;
    $exp_change->spawn("p4 change -i < $specfile\n") or die "Cannot spawn p4 change.\n";
    $result = $exp_change->expect(5, '-re', '\d{4,}');
    if(defined($result))
    {
        $pendingCL = $exp_change->match();
        print "The pending changelist is : " . $pendingCL . "\n";
    }
    else 
   {
       die "Create Changelist failed!\n";
   }
}

###############################################################################
#
# Subroutine to run autosubmit with proper arguments.
#
###############################################################################
sub run_autosubmit
{
    my  $autosubmitArgs;

    if($pendingCL)
    {
        $autosubmitArgs .= " -swChangelist $pendingCL";
    }

    # forward unrecognized args to testgen
    my $userArgs .= join ' ', map { quotemeta } @ARGV;
    $autosubmitArgs .= " " . $userArgs;
    print $autosubmitArgs . "\n";

    if($chip eq "lw50")
    {
        chdir("$ELW{TREEROOT}/hw/lw5x");
    }
    elsif($chip eq "g84")
    {
        chdir("$ELW{TREEROOT}/hw/g84");
    }
    elsif($chip eq "g86")
    {
        chdir("$ELW{TREEROOT}/hw/g86");
    }
    elsif($chip eq "g88")
    {
        chdir("$ELW{TREEROOT}/hw/g88");
    }
    elsif($chip eq "g92")
    {
        chdir("$ELW{TREEROOT}/hw/g92");
    }
    elsif(($chip eq "g94") || ($chip eq "g96") || ($chip eq "g98") )
    {
        chdir("$ELW{TREEROOT}/hw/tesla");
    }
    elsif(($chip eq "gt206") || ($chip eq "gt200"))
    {
        chdir("$ELW{TREEROOT}/hw/tesla2");
    }
    else
    {
        die "Invalid $chip.\n";
    }

    system("rm .autosubmit/previous_desc");

    system("p4 sync bin/...");
    
    system("bin/autosubmit $autosubmitArgs");

#    chdir("$ELW{TREEROOT}/sw/pvt/main_lw5x/diag/mods/tools");
}

###############################################################################
#
# Subroutine to run autosubmit2 with proper arguments.
#
###############################################################################
sub run_autosubmit2
{
    my  $autosubmitArgs;

    if($pendingCL)
    {
        $autosubmitArgs .= " -c $pendingCL";
    }

    # forward unrecognized args to testgen
    my $userArgs .= join ' ', map { quotemeta } @ARGV;
    $autosubmitArgs .= " " . $userArgs;
    print $autosubmitArgs . "\n";

    system("as2 submit $autosubmitArgs");

}

###############################################################################
#
# This is the main script
#
###############################################################################

GetOptions(
    "date=s"=>\$date,
    "srcDir=s"=>\$srcDir,
    "dstDir=s"=>\$dstDir,
    "chip=s"=>\@chips,
    );

my  $savedChip;
while(defined($chip = shift(@chips)))
{
    check_chip();
    check_src_dir();
    check_dst_dir();
    check_rom();
    prepare_change_list();
    $srcDir = "";
    $savedChip =  $chip;
}
$chip = $savedChip;
create_change_list();
if($as2)
{
   run_autosubmit2();
}
else
{
   run_autosubmit();
}
