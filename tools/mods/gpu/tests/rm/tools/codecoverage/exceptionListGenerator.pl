#!/home/gnu/bin/perl
#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2009 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

#This script takes a parent directory and segregates coverage on basis of module and family inside it .
#(Wherever this relationship is possible) .

#use strict;
use Switch;
use Getopt::Long;
use warnings;

my ($mainChip,$diffChip ,$resmanPath,$outputDir,$help);

my $result = GetOptions("mainChip=s"      => \$mainChip,
						"diffChip=s"      => \$diffChip,
                        "resmanPath=s"    => \$resmanPath,
                        "outputDir=s"     => \$outputDir,
                    	"help"            => \$help);

my (%funcListmainChip , %funcListdiffChip ,$dirname);

# Help Message
sub print_help_msg()
{
    print ("***************** options available ********************\n");
    print ("-mainChip <chipname>    : specify the chipname for whom to take hal coverage, its mandatory argument\n"); 
    print ("-diffChip <chipname>    : specify the chipname for whom to generate exceptionlist with, its mandatory argument\n");       
    print ("-resmanPath <path>      : Specify the absolute path to resman directory, its mandatory argument\n");  
    print ("-outputDir <path>       : Specify the output dir which will have the diff \n");
    print ("-help                   : To get description of various commandline options\n");
    print ("*********************************************************\n");
}
sub generateExceptionList()
{
    my ($outputFile , $mainChipHalFunctions,$diffChipHalFunctions,@mainChipHal,@diffChipHal,$rmconfigPath);
	$outputFile = "$dirname/$mainChip.txt";
	$outputFile = colwertToOsPath($outputFile);
	open(DAT,">>",$outputFile) || die("Cannot Open File $outputFile");
	$rmconfigPath = "$resmanPath/config";
	$rmconfigPath = colwertToOsPath($rmconfigPath);
    # Change Directory to rmconfig s directory 
    chdir $rmconfigPath || die " Unable to change directory to $rmconfigPath \n";
	$mainChipHalFunctions = `perl $rmconfigPath/rmconfig.pl --print $mainChip:nonstub:halfns`;
	$diffChipHalFunctions = `perl $rmconfigPath/rmconfig.pl --print $diffChip:nonstub:halfns`;
	@mainChipHal = split('\n',$mainChipHalFunctions);
	@diffChipHal = split('\n',$diffChipHalFunctions);
	
	foreach $function(@mainChipHal)
	{	
		chomp($function);
		if($function =~ /(.+)(_v[0-9]+_[0-9]+)$/)
		{
			$funcListmainChip{$function} = 1;
		}
	}

	foreach $function(@diffChipHal)
	{
		chomp($function);
		if($function =~ /(.+)(_v[0-9]+_[0-9]+)$/)
		{

			$funcListdiffChip{$function} = 1;
		}
	}
	
	foreach my $key(keys %funcListmainChip)
	{
		if(!exists ($funcListdiffChip{$key}))
		{
			print DAT "$key\n";
		}
	}
	print "See output in $outputFile\n";
	close DAT;
}
sub main()
{
	if (!$result || defined($help))
    {
        print_help_msg();
		exit 0;
    }
    die "Mandatory arguments are not specified, try -help for more info\n" if ( !defined($mainChip) || !defined($diffChip) || !defined($resmanPath) );

 	if (defined($outputDir))
    {
        $dirname = "$outputDir/";
    }
    else
    {
		my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
        $dirname = $mainChip.$sec.$min.$hr.$dayofm.$month.$yr;
    }
    $dirname = colwertToOsPath($dirname);
    # Create Directory
    mkdir $dirname || die "Not able to create directory $dirname\n";   
    # Write the output to a log file , open handle before changing dir

	generateExceptionList();
}

sub colwertToOsPath
{
    my $argument = shift;
    # ^O eq MsWin32 for all versions of Windows .
    if ($^O eq "MSWin32") 
    {
        $argument =~ s?/?\\?g;
    }
    else
    {
        $argument =~ s?\\?/?g;
    }
    $argument =~ s?//?/?g ;
    $argument  =~ s{\\\\}{\\}g;
    return $argument;
}

sub colwertToLinuxPath
{
    my $argument = shift;
    $argument =~ s?\\?/?g;
    $argument =~ s?//?/?g ;
    return $argument;
}
main();