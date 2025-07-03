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

# This Script will be helpful to get the diretory level coverage. 

use strict;
use Switch;
use Getopt::Long;
use warnings;

# commandline vars
my ($covFile, $pathDir,,$outputDir,$bullseyeBin,$help);

# other variables
my (@lines,$argument,$absPath,$relPath,$prefix);

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


# Will give the coverage of the given directory and it 's sub directories
sub listDirectoriesCoverage()
{
    my(%records,$inLoop,$key,$val,$outputFile,$output);
    $outputFile = 'coverage_report.csv';
    # Flag to be used later
    $inLoop = 0;
    # Get the path till the depot 
    givePrefix();
    # Prepend it with the path given by the user
    $pathDir = "$prefix/$pathDir";
    
    # Remove any unwanted slash 
    $pathDir =~ s?//?/?g;
       
    if(!(-d $pathDir))
    {
        print ("Error in pathDir(give Path from the software tree)$pathDir\n");
        exit 0;
    }
    # make a hash map with key directory and value its data values
    foreach(@lines)
    {
        ($key,$val) = split(',',$_,2);
        $records {$key} = $val;
    }
    
    # Get the names of the files and directories in $pathDIr
    opendir DIR,$pathDir;
    my @dir  = readdir(DIR);
    close DIR;
    mkdir $outputDir || die "Cannot make coverage directory $outputDir\n";
    open(DAT,">>$outputDir/$outputFile") || die("Cannot Open File $outputDir/$outputFile");
    open(ERROR,">>$outputDir/error.log") || die("Cannot Open File $outputDir/$outputFile");
    # Print the names of the fields in the output .csv File
    print DAT "Directory,FnCov,Outof,%,CndCov,Outof,%\n";          
    
    foreach my $directory(@dir)
    {  
        # if current field is a directory
        if(-d "$pathDir/$directory" && $directory ne '..')
        {    
            # if it is the current directory the enter the absolute Path in the .csv File
            if($directory eq '.')
            {
                $relPath = $pathDir;
                $absPath = "$pathDir/";
                print "\n$relPath\nSub Directories\n";
            }
            # Otherwise Enter the name of the directory
            else
            {
                $relPath = $directory;
                $absPath = "$pathDir/$directory/";
                print ($relPath ,"\n");
            }
         
            # Replace // with / if any
            $absPath =~ s?//?/?g;

            # Get the coverage
            # If in the hash map
            if(exists($records{'"'.$absPath.'"'}))
            {
                $output = $relPath.",".$records{'"'.$absPath.'"'};
                print DAT $output;
               
            }
            else
            {   
                foreach my $line(@lines)
                {
                    if($line =~ /$absPath/)
                    {
                        giveCoverage();
                        $inLoop = 1;
                        last;
                    }
                }
                # If this flag is not set then there was an error
                if($inLoop == 0)
                {
                    print ERROR ("NO COVERAGE INFO FOR => $relPath\n");
                }
                else
                {
                    $inLoop = 0;
                }
            }
        }
    }
    close DAT;
    close ERROR;
    print "\nSEE COVERAGE REPORT IN => $outputDir/$outputFile \n";
    print "\nSEE ERRORS   REPORT IN => $outputDir/error.log\n";
}


# This function will give the coverage of the directory in $absPath and write it in the output file
sub giveCoverage
{
    my $output;
    # Get the last line from the output of covdir Command
    $output      = `$bullseyeBin/covdir --no-banner -c -f $covFile $absPath | tail -1`;
    if($output)
    {
        # Accumulate whatever we have to write in the file in $output and write it .
        $output  = $relPath.$output;
        $output  =~ s?"Total"??g;
        print DAT $output;
    }
}

# This function will give the path of the depot in the coverage File
sub givePrefix()
{
    my $bullseyeCommand;
    $bullseyeCommand   = $bullseyeBin."/covdir";
    # Replace '//' with '/';
    $bullseyeCommand   =~ s?//?/?g;
    @lines             = `$bullseyeCommand --no-banner -c -f $covFile `;
    # For all lines check if /sw/ is there and get the Path
    foreach my $line(@lines)
    {
        $argument   = $line;
        checkForPrefix();
        if($prefix)
        {
            last;
        }
    }
    # replace the unwanted token 
    $prefix =~ s?"??g;
}

# This subroutine checks if the word /sw/ is there in the line, if yes it returns the prefix ie till /sw/ path
sub checkForPrefix()
{
    my @tokens;
    # Check if /sw/ is in the argument line given
    # If yes get the line in $prefix argument
    if($argument =~ m?/sw/?)
    {
        @tokens  = split('/sw/',$argument);
        $prefix  = $tokens[0];
    }
}

# This routine sees if the Bullseye version that the user is using is later than 7.12.3 or not 
sub checkVersion
{
    my ($first,$second,$third,$versionCommand,$versionOutput,$discard);
    $versionCommand           = "$bullseyeBin/covmgr -n";
    $versionCommand           =~ s?//?/?g;
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
    if(!defined($outputDir))
    {
        # Create the defaultdirectory  with time stamp
        my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
        $outputDir = "directoryCoverage_".$sec.$min.$hr.$dayofm.$month.$yr;
    }
    else
    {
        if(-d $outputDir)
        {
            print ("\nThe directory already exists\n");
            exit 0;
        }
    }
    if(!(-f $covFile))
    {
        print ("No such COVFILE \n");
        exit 0;
    }
    listDirectoriesCoverage();
}

main();
