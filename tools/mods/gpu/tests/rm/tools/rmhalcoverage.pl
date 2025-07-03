#!/home/gnu/bin/perl
#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2007 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

#
# This Script will be helpful to get hal level coverage report in .csv format
# for speified chip. it will also update the covfile if option provided. 

use strict;
use Switch;
use Getopt::Long;

# commandline vars

my ($resman, $covfile, $chip, $updatecov, $outputdir, $inputRegionFile,  $help);

# Bullseye's BIN path
my $bullseye_bin = '/home/scratch.jkhurana_fermi-sw/src/sw/tools/BullseyeCoverage-7.13.25/bin';
# No of regions to run together are specified by run
my $run = 50;

# Parse Cmdline Options
my $result = GetOptions("chip=s" => \$chip,
                        "resmanPath=s" => \$resman,
                        "run=s" => \$run,
			"covFile=s" => \$covfile,
                        "bullseye_bin=s" => \$bullseye_bin,
			"updateCov" => \$updatecov,
			"outDir=s" => \$outputdir,
                        "inputRegionFile=s" => \$inputRegionFile,
			"help" => \$help);

# Help Message
sub print_help_msg()
{
    print ("***************** options available ********************\n");
    print ("-chip <chipname>    : specify the chipname for whom to take hal coverage, its mandatory argument\n");    
    print ("-resmanPath <path>  : Specify the absolute path to resman directory, its mandatory argument\n");    
    print ("-covFile <path>     : Specify the absolute path to coverage file, its mandatory argument\n");    
    print ("-outDir             : Spcify the absolute path of directory where to get the output, default is current diretory\n");
    print ("-updateCov          : Specify if along with hal coverage output also wants to update coverage file\n");
    print ("-bullseye_bin <path>: Specify abs Path of the bulls eye bin\n");
    print ("-inputRegionFile    : use regions to include for a particular chip from inputRegionFile\n");
    print ("-run                :Include <run> regions at a time ,default 50\n");
    print ("-help               : To get description of various commandline options\n");
    print ("*********************************************************\n");
}

# Main routine
sub main()
{
    if (!$result || defined($help))
    {
        print_help_msg();
	exit 0;
    }
    die "Mandatory arguments are not specified, try -help for more info\n" if ( (!defined($resman) && !defined($chip)) && !defined($inputRegionFile) ) || !defined($covfile);

    # Local scalar variable
    my ($funcindex, $halindex, $region, $kernel);
    
    # Local vector variable
    my (@hal_data, @func_data);
    
    # Create the directory with time stamp
    my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
   
    my $dirname = "rmhalcoverage_".$sec.$min.$hr.$dayofm.$month.$yr;
     
    if (defined($outputdir))
    {
        $dirname = $outputdir."/".$dirname;
        $dirname = colwertToOsPath($dirname);
    }
    
    # Create Directory
    mkdir($dirname, 0755) || die "Not able to create directory $dirname\n";
    
    # Change Directory
    chdir $dirname || die " Unable to change directory to $dirname \n";
    
    print "rmhalcoverage.pl : Creating Hal list for $chip \n";
   
    if(!defined($inputRegionFile))
    {
        my $regionCommand = "perl $resman/kernel/inc/tools/mcheck.pl --funclist -g $chip > hallist_$chip.log";
        $regionCommand    = colwertToOsPath($regionCommand);
        system($regionCommand);
        die "mcheck.pl Fail $resman/kernel/inc/tools/mcheck.pl --funclist -g $chip > hallist_$chip.log" if(($?>>8) != 0);
        open(HAL,"+<","hallist_$chip.log") || die "Not able to open hallist_$chip.log";
    }
    else
    {
        open(HAL,"+<","$inputRegionFile") || die "Not able to open ";
    }
    @hal_data = <HAL>;
    close(HAL);
    
    # Create complete list of function coverage
    
    my $functionCoverageCommand = "$bullseye_bin/covfn -f$covfile  -c";
    $functionCoverageCommand    = colwertToOsPath($functionCoverageCommand);
    
    @func_data = `$functionCoverageCommand`;
    die "failed to create function coverage file" unless @func_data;
    my $all_functions = 1;

    # Create a  .csv file to gets hal level coverage
    open(HALCOVERAGE,"+>>","halcoverage.csv") || die "Not able to open halcoverage.csv";
    
    # Print the heading
    print HALCOVERAGE $func_data[0];
    
     my %HOC;
    
    # Create the hash Index
    for($funcindex=1; $funcindex<=$#func_data; $funcindex++)
    {	
        my ($hashindex,@array);
        if($func_data[$funcindex] =~ /\(/)
        {
            @array =  split(/\(/,$func_data[$funcindex],2);
            $hashindex = $array[0];
            $hashindex =~ s/\"//g;
        }
        else 
        {
            $hashindex = split(',',$func_data[$funcindex],2);
        }
        $HOC{$hashindex} =  $func_data[$funcindex] ;
    }
    
    # Search for specified function name
    for($halindex=0; $halindex<=$#hal_data; $halindex++)
    {
        my @hal_field = split(':', $hal_data[$halindex],3);
        if($hal_field[0] =~ /^func$/)
        {
            my (@funcline,@temp,$sourcefunc,,$filename);
            my @halfuncname = split(' ',$hal_field[1]);
            print HALCOVERAGE "$HOC{$halfuncname[0]}";
            if($updatecov == 1)
            {
                if($HOC{$halfuncname[0]} =~ /\(/)
                {
                    @temp = split(/\)\"\,/,$HOC{$halfuncname[0]},2);
                    $sourcefunc = $temp[0]."\)";
                    @funcline = split(',',$temp[1]);
                    $filename = $funcline[0];
                 }
                else
                {
                    @funcline = split(',',$HOC{$halfuncname[0]});
                    $sourcefunc = $halfuncname[0];
                    $filename = $funcline[1];
                }
                $sourcefunc =~ s?"??g;
                $sourcefunc=~ s/^\s+//g;
                if($filename =~ /^"([a-zA-Z0-9.:_\/]*)"$/)
                {
                    $region = $region." ".$1.":"."\"$sourcefunc\"";
                    $sourcefunc = '';
                    if($1 =~ /^([a-zA-Z0-9.:_\/]*resman\/kernel\/).*/ && !defined($kernel))
                    {		
                        $kernel = $1;
                        print " resman folder : $kernel \n";
                    }	
                }		
            }
            else
            {
                print "$halfuncname[0]\n";
            }
        }       
    }   
    close(HALCOVERAGE);
    if($updatecov == 1)
    {
        my ($bullseyeCommand,$excludeKernelCommand,$includeRegionCommand) ;
        $bullseyeCommand = "$bullseye_bin/covselect -f$covfile -a -v";
        $bullseyeCommand = colwertToOsPath($bullseyeCommand);
        $excludeKernelCommand = "$bullseyeCommand !$kernel";
        system($excludeKernelCommand);
        die "failed to exclude kernel directory  $excludeKernelCommand\n" if (($?>>8) != 0);
        my @lines = split(' ',$region);
        my $temp_region = '';
        my $counter = 0;
        foreach my $line(@lines)
        {
            $temp_region .= "$line ";
            $counter++;
            if($counter >= $run)
            {
                $includeRegionCommand = "$bullseyeCommand $temp_region";
                system($includeRegionCommand);		
                $temp_region = '';
                $counter = 0;
            }
        }
        if($temp_region)
        {
            $includeRegionCommand = "$bullseyeCommand $temp_region";
            system($includeRegionCommand);		
        }
    }
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

# Call Main Function
main();

