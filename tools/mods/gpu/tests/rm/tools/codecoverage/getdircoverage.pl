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

#use strict;
use Switch;
use Getopt::Long;
use warnings;

# commandline vars
my ($covFile, $pathDir,$outputDir,$bullseyeBin,$helperScript,$help);

# Parse Cmdline Options
my $result = GetOptions("covFile=s"     => \$covFile, 
                        "pathDir=s"     => \$pathDir,
                        "bullseyeBin=s" => \$bullseyeBin,
                        "outputDir=s"   => \$outputDir,
                        "help"          => \$help);

# Help Message
sub print_help_msg()
{
    print ("***************** options available *******************************************************\n");
    print ("MANDATORY ARGUMENTS\n");
    print ("-covFile     <path> : Specify the absolute path to coverage file\n");    
    print ("-pathDir     <path> : Specify the path of directory starting from 'sw' tree  e.g /sw/<rest>etc \n");
    print ("-bullseyeBin <path> : Specify the absolute path of the bullseye bin \n");
    print ("OPTIONAL ARGUMENTS\n");
    print ("-outputDir   <path> : Specify the absolute path(name in lwr dir) of the ouput directory,default is directoryCoverage_<timestamp> \n");
    print ("-help               : To get description of various commandline options\n");
    print ("*******************************************************************************************\n");
}

sub mylength
{
    my ($line,$temp);
    $line = shift;
    ($line,$temp) = split(',',$line,2);
    return length($line);
}

sub returnSubDirectory
{
    my ($lengthTillNow,$subDirLength,$subDir,$queryLength,@tokens,$returnSubDir);
    $subDir = shift;
    $queryDirLength = shift;
    $subDirLength = lengthOfPath($subDir);
    if($subDirLength == $queryDirLength + 1)
    {
        return $subDir;
    }
    else
    {
        $lengthTillNow = 0;
        $tokenIndex    = 0;
        $returnSubDir = findRoot($subDir);
        @tokens = split('/',$subDir);
        if(!$tokens[0])
        {
            shift(@tokens);
        }
        if($returnSubDir =~ /:/)
        {
            shift @tokens;
            $queryDirLength--;
        }
        while($lengthTillNow < $queryDirLength + 1)
        {  
            $returnSubDir .= "$tokens[$tokenIndex]/";
            $lengthTillNow++;
            $tokenIndex++;
        }
        return $returnSubDir;
    }
}


# Will give the coverage of the given directory and it 's sub directories
sub listDirectoriesCoverage()
{
    my($totalDir,$coverageCommand,$htmlCommand,$totalData,$prefix,%records,$inLoop,$outputFile,$output, $pathLength,@sortedKeys,%oclwrred,@coverage_dirs,@coverage_dirs_output,$key,$val,@tempTokens,@tokens);
    $outputFile = 'coverage_report.csv';
    # Get the path till the depot 
    $prefix = givePrefix();
    $prefix = colwertToLinuxPath($prefix);
    
    # Prepend it with the path given by the user
    $pathDir = colwertToLinuxPath($pathDir);
    $pathDir = "$prefix/$pathDir/";
   
    # Remove any unwanted slash 
    $pathDir =~ s?//?/?g;
   
    if(!(-d $outputDir))
    {
        mkdir $outputDir || die "Cannot make coverage directory $outputDir\n";
    }
    ($outputFile = "$outputDir/".$outputFile);
    $outputFile = colwertToOsPath($outputFile);
    open(DAT,">>$outputFile") || die("Cannot Open File $outputFile");
    # Print the names of the fields in the output .csv File
    $coverageCommand = "$bullseyeBin/covdir --no-banner -c -f$covFile ";
    $coverageCommand = colwertToOsPath($coverageCommand);
    @coverage_dirs_output = `$coverageCommand`;
    my $index = 0;
    foreach my $line(@coverage_dirs_output)
    {
		if($line =~ $pathDir)
		{
			$coverage_dirs[$index++] = $line;
		}
	}
    die "Could not find information for $pathDir \n" unless @coverage_dirs;
    # Path Length
    $pathLength = lengthOfPath($pathDir) ;
    
    # Sort sub directories length wise
   
    @sortedKeys = sort{
        mylength($a) <=> mylength($b)
        }@coverage_dirs;
    
    my $shortenkey = '';
    foreach my $line(@sortedKeys)
    {
        ($key,$val) = split(',',$line,2);
        $key =~ s?"??g;
        if($key ne 'Directory')
        {
            if($key ne $pathDir)
            {
                $key = returnSubDirectory($key,$pathLength);
                @tempTokens = split('/',$key);
                $shortenkey = $tempTokens[$#tempTokens];
            }
            else
            {
                $shortenkey = $pathDir;
            }
            if(!(exists $oclwrred{$shortenkey}))
            {
               print "$shortenkey\n";
                if($val)
                {
                    $oclwrred{$shortenkey} = $val;
                }
            }
        }
    }
    my @sortorder = sort keys %oclwrred;
    if($oclwrred{$pathDir})
    {
        print DAT "Directory,FnCov,Outof,%,CndCov,Outof,%\n";          
        print DAT "$pathDir,$oclwrred{$pathDir}";
    }
    else
    {
        print "No coverage for $pathDir\n";
        exit 0;
    }
    foreach my $key(@sortorder)
    {
        if($key ne $pathDir)
        {
            print DAT "$key,$oclwrred{$key}";
        }
    } 
    close DAT;
    
    $htmlCommand = "perl $helperScript -file $outputFile -outputDir $outputDir -outputFileName directory_report.html";  
    $htmlCommand = colwertToOsPath($htmlCommand);
    system($htmlCommand);
    print "\nSEE COVERAGE REPORT IN => $outputFile $outputDir/directory_report.html\n";
    return ;
}


# This function will give the path of the depot in the coverage File
sub givePrefix
{
    my ($bullseyeCommand,@lines,$prefix);
    $bullseyeCommand   = $bullseyeBin."/covdir --no-banner -c -f$covFile";
    # Replace '//' with '/';
    $bullseyeCommand   = colwertToOsPath($bullseyeCommand);
    @lines             = `$bullseyeCommand `;
    # For all lines check if /sw/ is there and get the Path
    foreach my $line(@lines)
    {
        $line = colwertToLinuxPath($line);
        $prefix = checkForPrefix($line);
        if($prefix)
        {
            last;
        }
    }
    # replace the unwanted token 
    $prefix =~ s?"??g;
    return $prefix;
}

# This subroutine checks if the word /sw/ is there in the line, if yes it returns the prefix ie till /sw/ path
sub checkForPrefix
{
    my (@tokens,$argument,$prefix);
    $argument = shift;
    # Check if /sw/ is in the argument line given
    # If yes get the line in $prefix argument
    if($argument =~ m?/sw/?)
    {
        @tokens  = split('/sw/',$argument);
        $prefix  = $tokens[0];
        return $prefix;
    }
    return '';
}

# This routine sees if the Bullseye version that the user is using is later than 7.12.3 or not 
sub checkVersion
{
    my ($first,$second,$third,$versionCommand,$versionOutput,$discard);
    $versionCommand           = "$bullseyeBin/covmgr -n";
    $versionCommand           = colwertToOsPath($versionCommand);
    $versionOutput            = `$versionCommand`;
    ($versionOutput,$discard) = split('\n',$versionOutput);
    ($discard,$versionOutput) = split(' ',$versionOutput);
    print ($versionOutput);
    ($first,$second,$third)   = split(/\./,$versionOutput);
    # Now this should be >    = 7.12.3 for this script to work properly
    if($first >= 7)
    {
        if($first == 7)
        {
            if($second >=12)
            {
                if($second == 12)
                {
                    if($third <3)
                    {
                        print ("\nError :Your Version needs to be greater or equal to 7.12.3\n");
                        exit 0;
                    }
                }
            }
            else
            {
                print ("\nError:Your Version needs to be greater or equal to 7.12.3\n");
                exit 0;
            }
        }
    }
    else
    {
        print ("\nError:Your Version needs to be greater or equal to 7.12.3\n");
        exit 0;
    }
}

# Main routine
sub main()
{
    if (!$result || defined($help))
    {
        print_help_msg();
	exit 0;
    }
    die "Mandatory arguments are not specified, try -help for more info\n" if (!defined($covFile) 
                                                                               || !defined($pathDir)
                                                                               || !defined($bullseyeBin)) ;
    
    checkVersion();

    $helperScript = givePreRequisite();

    if(!defined($outputDir))
    {
        # Create the defaultdirectory  with time stamp
        my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
        $outputDir = "directoryCoverage_".$sec.$min.$hr.$dayofm.$month.$yr;
    }
    if(!(-f $covFile))
    {
        print ("No such COVFILE \n");
        exit 0;
    }
    listDirectoriesCoverage();
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

sub findRoot
{
    my $argument = $_[0];
    my ($root,$rest) ;
    ($root,$rest) = split (/\//,$argument);
    my $returnRoot = "$root/";
    return $returnRoot;
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

sub lengthOfPath
{
    my($arg,@token);
    $arg = $_[0];
    @token = split( /\//,$arg);
    if(!$token[0])
    {
        shift(@token);
    }
    return scalar(@token);
}

main();
