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

# This will take two coverage files and give as output their merged coverage in a .csv.
# Works both on windows and linux

use strict;
use Switch;
use Getopt::Long;
use File::Basename;
use mergeCoverageFiles;
use Cwd;
# commandline vars
my ($covFile1, $covFile2, $outputDir, $bullseyeBin, $help,$object);

# Parse Cmdline Options
my $result = GetOptions("covFile1=s"      => \$covFile1,
                        "covFile2=s"      => \$covFile2,
                        "bullseyeBin=s"  => \$bullseyeBin,
                        "outputDir=s"    => \$outputDir,
                        "help"           => \$help);

# Help Message
sub print_help_msg()
{
    print ("********************** options available ***************************\n");
    print ("-covFile1 <path>    : Specify the absolute path to coverage file to \n"); 
    print ("                      be merged\n");    
    print ("-covFile2 <path>    : Specify the absolute path to coverage file to \n"); 
    print ("                      be merged\n");    
    print ("-bullseyeBin <path> : Specify the absolute path of the bullseyeBin\n");
    print ("OPTIONAL ARGUMENTS\n");
    print ("-outputDir          : Specify the absolute path of the output Directory(will create \n");
    print ("                     if non existing), Default is lwrrentdirectory/total_timestamp\n"); 
    print ("-help               : To get description of various \n");
    print ("                      commandline options\n");
    print ("********************************************************************\n");
}


sub giveCoverage
{
    my $myobject = shift;
    my $argument = shift;
    my ($bullseyeCommand , $funcCoverageCommand, @output, $first, $second, $third, $fourth, $restData, $restName ,$object);
    $bullseyeCommand = "$bullseyeBin/covfn -c";
    $bullseyeCommand = colwertToOsPath($bullseyeCommand);
    $funcCoverageCommand = "$bullseyeCommand -f".$argument;
    @output = `$funcCoverageCommand`;
    
    my $firstLine = 1;
    foreach my $statement(@output)
    {
        if($firstLine == 1)
        {
            $firstLine = 0;
        }
        else
        {
            chomp ($statement);
            if($statement =~ /\(/)
            {
                ($first,$restData) = split(/\)\"\,/,$statement);
                ($first,$restName) = split(/\(/,$first);
	            #Used $restName as a temp ,we won't be using its value
	            ($second,$third,$fourth,$restName) = split(/,/,$restData);
             }
             # For earlier bins,prior to 7.13.25 
             else
             {
                ($first,$second,$third,$fourth,$restData) = split(',',$statement);
             }
            $second  = colwertToOsPath ($second);
            $second  = giveSoftwarePath($second);
            my $string = "$second%$first";
            $string =~ s?"??g;
            $myobject -> addList($string,$fourth);
          } 
     }
    return;
}

sub giveSoftwarePath
{
    my $argument = @_[0];
    my $splitAt;
    my @tokens;
    if($^O eq "MSWin32")
    {
        $splitAt = '\\';
    }
    else
    {
        $splitAt = '\/';
    }
    
    if($^O eq "MSWin32")
    {
        @tokens = split(/\\/,$argument);
    }
    else
    {
        @tokens = split(/\//,$argument);
    }
    my $foundSw = 0;
    my $return = '';
    foreach my $token(@tokens)
    {
        if($token ne '')
        {
            if($foundSw == 1)
            {
                $return = "$return".$splitAt."$token";
            }
            else
            {
                if($token eq 'sw')
                {
                    $return = "$return".$splitAt."$token";
                    $foundSw = 1;
                }
            }
        }
    }
    return $return;
}

sub main()
{
    if (!$result || defined($help))
    {
        print_help_msg();
        exit 0;
    }
    die "Mandatory arguments are not specified, try -help for more info\n" if (!defined($covFile1))
        ||(!defined($covFile2))
        ||(!defined($bullseyeBin));
    if(!(-f $covFile1))
    {
        print "$covFile1 does not exist \n";
        exit 0;
    }
    if(!(-f $covFile2))
    {
        print "$covFile2 does not exist \n";
        exit 0;
    }
    my ($coverage_data , $dirCoverageObj);
    my ($path,$directory,$function,$coveredOrNot,$key,$value,$file,$dir);
    my $cwd = getcwd(); # Create the directory with time stamp
    my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();    
    if(!defined $outputDir)
    {
        my $outputName = "totalcoverage_".$sec.$min.$hr.$dayofm.$month.$yr;
        $outputDir = "$cwd/$outputName";
    }    
    $outputDir    = colwertToOsPath($outputDir);
    
    $coverage_data = mergeCoverageFiles:: -> new(); 
    giveCoverage($coverage_data,$covFile1);
    giveCoverage($coverage_data,$covFile2);
    $dirCoverageObj = mergeCoverageFiles:: ->new();
    my %hash1 = %$coverage_data;
    while(($key,$value) = each(%hash1))
    {
        my $match = ".c";
        # Bug Found use seperators carefuly
        ($path,$function) = split(/%/,$key);
        if($path =~ m/$match/)
        {
            $file       = basename($path);    
            $directory  = dirname($path);
            
            my $val = "$file"."+"."$function^$value";
            $dirCoverageObj -> addDir($directory,$val,$value);
        }
    }
    $dirCoverageObj -> printDirList($outputDir);

    return ;
}

main();

sub colwertToOsPath
{
    my $argument = @_[0];
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
