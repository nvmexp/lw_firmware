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

# commandline vars
my ($covFile, $familyName,@exceptionList ,$bullseyeBin ,$branchPath ,$parentDirPath ,$exceptionFile , $help ,$outputDir);

# Parse Cmdline Options
my $result = GetOptions("covFile=s"       => \$covFile,
                        "parentDirPath=s" => \$parentDirPath,
						"exceptionFile=s" => \$exceptionFile, 
                        "familyName=s"    => \$familyName,
                        "bullseyeBin=s"   => \$bullseyeBin,
                        "outputDir=s"     => \$outputDir,
                        "branchPath=s"    => \$branchPath,
                        "help"            => \$help);
                        
# Help Message
sub print_help_msg()
{
    print ("***************** options available *******************************************************\n");
    print ("MANDATORY ARGUMENTS\n");
    print ("-covFile     <path>       : Specify the absolute path to coverage file\n");    
    print ("-parentDirPath <path>     : Specify the path till parent directory eg path till drivers/resman/kernel \n");
    print ("-familyName  <familyname> : Specify the name of the family \n");
    print ("-bullseyeBin <path>       : Specify the absolute path of the bullseye bin \n");
    print ("-exceptionFile <path>     : Specify the exceptionFile for family\n");
    print ("OPTIONAL ARGUMENTS\n");
    print ("-outputDir   <path> : Specify the absolute path(name in lwr dir) of the ouput directory,default is directoryCoverage_<timestamp> \n");
    print ("-help               : To get description of various commandline options\n");
    print ("*******************************************************************************************\n");
}

sub givePreRequisite
{
    my $toolsDirectory = $ELW{'CODECOVERAGETOOLS'};
    if(defined($toolsDirectory))
    {
        print "Tools Directory $toolsDirectory \n";
    }
    if(!defined($toolsDirectory) || !(-d $toolsDirectory) )
    {
        if(!(-f "colwertToHtml.pl"))
        {
            print "Please specify CODECOVERAGETOOLSPATH elw variable or check it if Already set\n";
            exit 0;
        }
        else
        {
            return "colwertToHtml.pl";
        }
    }
    else
    {
        my $htmlScriptPath = "$toolsDirectory/colwertToHtml.pl";
        $htmlScriptPath = colwertToOsPath($htmlScriptPath);
        if(!(-f $htmlScriptPath) )
        {
            print "Helper script not found at $toolsDirectory\n";
            exit 0;
        }
        else
        {
            return $htmlScriptPath;
        }
    }
    # Control will never come here
}

# Main routine
sub main()
{
    # Create the directory with time stamp
    my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
    if (!$result || defined($help))
    {
        print_help_msg();
	    exit 0;
    }

    die "Mandatory arguments are not specified, try -help for more info\n" if (!defined($covFile) 
                                                                               || !defined($parentDirPath)
                                                                               || !defined($familyName)
                                                                               || !defined($bullseyeBin)
                                                                               || !defined($exceptionFile)) ;
   
    if(!(-f $covFile))
    {
        print ("No such COVFILE \n");
        exit 0;
    }
    if(!(-d $bullseyeBin))
    {
        print ("No such $bullseyeBin \n");
        exit 0;
    }
    $parentDirPath = "$parentDirPath/";
    $parentDirPath = colwertToOsPath($parentDirPath); 
	
	$helperScript = givePreRequisite();
	populateExceptionList();
    listfamilyCoverage();
}

sub populateExceptionList
{
	open(EXCEPTIONS,"+<",$exceptionFile) || die "Not able to open $exceptionFile";
	@exceptionList = <EXCEPTIONS>;
	close EXCEPTIONS;
}

sub findRoot
{
    my $argument = $_[0];
    my ($root,$rest) ;
    ($root,$rest) = split (/\//,$argument);
    my $returnRoot = "$root/";
    return $returnRoot;
    # confirm this
}


sub listfamilyCoverage()
{
	my ($current , $dirname ,$bullseyeCommand ,@listOfModules,$familyPath,$outputFile);
	my $root = findRoot($parentDirPath);
	if (defined($outputDir))
    {
        ($dirname = "$outputDir/") =~ s?//?/?g;
    }
    else
    {
		my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
        $dirname = "rmcoverage_".$familyName.$sec.$min.$hr.$dayofm.$month.$yr;
    }
    if(!(-d $dirname))
    {
        mkdir $dirname || die "Cannot make coverage directory $dirname\n";
    }
	$outputFile = "$dirname/output_$familyName.txt";
	$outputFile = colwertToOsPath($outputFile);
	$tempFile   = "$dirname/temp_$familyName.txt";
	# Export halification list to a txt file
	$bullseyeCommand   = $bullseyeBin."/covselect -f$covFile -e $tempFile";
	# Replace '//' with '/';
	$bullseyeCommand   = colwertToOsPath($bullseyeCommand);
	system($bullseyeCommand);
    open(EXPORTS,"+<",$tempFile) || die "Not able to open $tempFile";
	@export_data = <EXPORTS>;	
	close EXPORTS;
	open(IMPORTS,">> $outputFile") || die("Cannot Open File $outputFile");
   	# We are using linux path as that is what is used by bullseye
	foreach my $line(@export_data)
	{			
		if($line =~ /(.*)$parentDirPath(.*)\/$familyName\/(.*)/ || isException($line))
		{
			print IMPORTS $line;
		}
	}	
	close IMPORTS;
	# Remove all selections
	$bullseyeCommand   = $bullseyeBin."/covselect -f$covFile -d";
	$bullseyeCommand   = colwertToOsPath($bullseyeCommand);
	system($bullseyeCommand);
	# Exclude the root , before we can add family level includes
	$bullseyeCommand   = $bullseyeBin."/covselect -f$covFile -a !$root";
	$bullseyeCommand   = colwertToOsPath($bullseyeCommand);
	system($bullseyeCommand);
	# Import family level includes
	$bullseyeCommand   = $bullseyeBin."/covselect -f$covFile -i $outputFile";
	$bullseyeCommand   = colwertToOsPath($bullseyeCommand);
	system($bullseyeCommand);
	# Now that we have got all the modules coverage , let us take the coverage of their families and list that out with the module coverage
	# Format the report 	
	formatAndPrint($dirname);
}

sub isException()
{
	my $arg = shift;
	foreach my $exception(@exceptionList)
	{
		chomp($exception);
		if($arg =~ /$exception/)
		{
			return 1;
		}		
	}
	return 0;
}

sub formatAndPrint()
{
	my $dirname = shift;
	my ($bullseyeCommand,$hashIndex,$subDirName, $replaceString,$replaceWith,$generateReportCommand,$htmlCommand,@coverage_dirs_output,%finalOutput,$sourceDir,@outputLine);
	my (@outputLines,$functionName,$fileName,$coverageData,$moduleNo,$functionNo,@module_tokens,@function_tokens);
    my $outputFile = "$dirname/report.csv";
    $outputFile = colwertToOsPath($outputFile);
    ############################  Populate module corresponding FUNCTION Data ######################################
    ($bullseyeCommand   = "$bullseyeBin/covfn")=~ s?//?/?g;
    $generateReportCommand = "$bullseyeCommand -f$covFile -c";
    $generateReportCommand = colwertToOsPath($generateReportCommand);
    @familyFunctions = `$generateReportCommand`;
    
    die "failed to create report $generateReportCommand\n" unless @familyFunctions;
    
    
    
    foreach $function(@familyFunctions)
    {
		@outputLine = split('\",\"',$function);
		@outputLine = split(',',$outputLine[1]);
		$sourceDir = $outputLine[0];
		if($sourceDir)
		{
			# sub dir inside parent dir is hash index
			if($sourceDir =~ /($parentDirPath)([^\/]+)\/(.*)/)
			{
				# Make the module Path as the hash index and populate it
				$hashIndex = $2;
				$hashIndex = colwertToLinuxPath($hashIndex);
				push(@{$finalOutput{$hashIndex}}, $function);
			}
			else
			{
				if(!$sourceDir =~ /"Source"/)
				{
					print "SOFTWARE ERROR OR EXCEPTION LIST not in $parentDirPath";
					exit 0;
				}
			}
		}
	} 
    
    ################################################# get directory level info##########################################
    open(DAT,">>$outputFile") || die("Cannot Open File $outputFile");
    ($bullseyeCommand   = "$bullseyeBin/covdir")=~ s?//?/?g;
    $generateReportCommand = "$bullseyeCommand --no-banner -c -f$covFile";
    $generateReportCommand = colwertToOsPath($generateReportCommand);
    @coverage_dirs_output = `$generateReportCommand`;
	die "Could not find information of $parentDirPath \n" unless @coverage_dirs_output;

    $moduleNo =1;
    print DAT "SNo,SOURCEPATH,FUNCTIONCOVERAGE%,COVERED CONDITIONS,OutOf,C/D%\n";
	foreach my $line(@coverage_dirs_output)
    {
   		if($line =~ /$parentDirPath/)
		{
			# we want subdir inside parent dir as hash index 
			if($line =~ /(.*)($parentDirPath)([^\/]+)\/(.*)/)
			{
				$hashIndex = $3;
				$hashIndex = colwertToLinuxPath($hashIndex);
				@outputLines = @{$finalOutput{$hashIndex}};
				$replaceString = $2;
				$replaceWith = '';
				$subDirName = $2.$3;
				$line =~ s?$replaceString?$replaceWith?g;
				@module_tokens = split(/,/,$line,5);
				print DAT (" $moduleNo , $hashIndex :: $module_tokens[1] fncs covered outof $module_tokens[2] , $module_tokens[3] ,$module_tokens[4]") ;	
				$functionNo = 1;	
				foreach my $outputLine(@outputLines)
				{
					$outputLine =~ s?$subDirName?$replaceWith?g;
					if($outputLine =~ /\"(.+)\",\"(.+)\",(\d+),(.*)/)
					{	
						$functionName = $1;
						$fileName = $2;
						$coverageData = $4;
						my @coverage_Data = split(',',$coverageData,2);
						if($coverage_Data[0] == 1)
						{
							$coverage_Data[0] = 100;
						}
						if($functionName =~ /(.+)\((.*)\)/)
						{
							$functionName = $1;	
						}
						print DAT "-------,    $functionNo\) $fileName : $functionName , $coverage_Data[0]%,$coverage_Data[1]\n";
						$functionNo++;
					}
					else
					{
						print "Check reg ex:Software error";
						exit 0;
					}
				}
				$moduleNo++;
			}
			else
			{
				@module_tokens = split(/,/,$line,5);
				$finalLine = "TOTAL, $module_tokens[0] $module_tokens[1] fncs outof $module_tokens[2] , $module_tokens[3] ,$module_tokens[4]" ;	
			}
		}
	}
    print DAT $finalLine; 
    print ("\n\nSee their coverage in $covFile \n Find report.csv in $dirname\n");
    $htmlCommand = "perl $helperScript -file $outputFile -outputDir $dirname -outputFileName resman_$familyName.html";  
    $htmlCommand = colwertToOsPath($htmlCommand);
    system($htmlCommand);
    print "\nSEE COVERAGE REPORT IN => $dirname as resman_$familyName.html\n";
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