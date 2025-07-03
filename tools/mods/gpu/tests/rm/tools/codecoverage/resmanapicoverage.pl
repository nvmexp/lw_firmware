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

# This will be helpful to see the coverage to get the api level coverage by updating the coverage file and create the XLS sheet also.
# Works both on windows and linux

use strict;
use Switch;
use Getopt::Long;
use Cwd;
use Elw;
# commandline vars
my ($covFile, $rmconfigPath, $outputDir, $helperScript , %funcList ,$bullseyeBin, $listApis, $resmanRoot, $help, $apiListFile);

# Parse Cmdline Options
my $result = GetOptions("covFile=s"      => \$covFile,
                        "apiListFile=s"  => \$apiListFile,
                        "rmconfigPath=s" => \$rmconfigPath,
                        "resmanRoot=s"   => \$resmanRoot,
                        "bullseyeBin=s"  => \$bullseyeBin,
                        "outputDir=s"    => \$outputDir,
                        "listApis"       => \$listApis,
                        "help"           => \$help);

# Help Message
sub print_help_msg
{
    print ("********************** options available ****************************************************\n");
    print("MANDATORY ARGUMENTS  :\n");
    print ("-covFile <path>     : Specify the absolute path to coverage file\n");    
    print ("-rmconfigPath <path>: Specify abs path to rmconfig dir <Path till sw>/sw/dev/gpu_drv/chips_a/drivers/resman/config \n");
    print ("-bullseyeBin        : Specify the absolute path of the bullseyeBin\n");
    print ("OPTIONAL ARGUMENTS\n");
    print ("-listApis           : To see the list of Api s for which coverage is added on running the scripts\n");
    print ("-apiListFile <file> : give apiList in a file instead of rmconfigPath \n");
    print ("-resmanRoot         : To be used when rmconfig.pl is giving an error on windows\n");
    print ("-outputDir   <path> : Specify the absolute path of the output directory(will create) \n");
    print ("                    : if it does not exist,default is current directory/<timestamp> .\n");
    print ("-help               : To get description of various commandline options\n");
    print ("**********************************************************************************************\n");
}

# This function will give the path of the lwapi.c
sub giveApiSourcePath
{
     my ($bullseyeCommand,$temp,@lines,$argument,$source);
     ($bullseyeCommand   = "$bullseyeBin/covfn") =~ s?//?/?g;
	 $bullseyeCommand    = replaceIfWindows($bullseyeCommand);
     
     @lines              = `$bullseyeCommand -f$covFile -c`;
     $source             = '';
     # For all lines check if lwapi.c is there and get the Path
     foreach my $line(@lines)
     {
         if($source eq '')
         {
             $source = checkApiSourcePath($line,0);
         }
         else
         {
             $temp = checkApiSourcePath($line,1);
         }
     }
     # replace the unwanted token 
     $source             =~ s?"??g;
    return $source;
}

# This subroutine checks if the word/lwapi.c is there in the line, if yes it returns the lwapi.c path
sub checkApiSourcePath
{
    my (@tokens,$return,$fname,$data,$protoType);
    my ($argument,$populateFlag) = @_;
    my $search = "lwapi.c";
    if($argument =~ /$search/ ) 
    {
        if($argument =~ /\(/)
        {
            ($fname,$data) = split(/\)\"\,/,$argument);
            ($fname,$protoType) = split(/\(/,$fname);
            $protoType="\(".$protoType."\)";
            $fname =~ s?"??g;
            $funcList{$fname} = $protoType;
            if($populateFlag == 0)
            {
                @tokens    = split(',',$data);
                $return    = $tokens[0];
            }
        }
        # For earlier version bullseye ,prior to 7.13.25
        else
        {
            if($populateFlag == 0)
            {
                @tokens    = split(',',$argument);
                $return    = $tokens[1];
            }
        }
    }
    return $return;
}

sub findRoot
{
    my $argument = @_[0];
    my ($root,$rest) ;
    ($root,$rest) = split (/\//,$argument);
    my $returnRoot = "$root/";
    return $returnRoot;
    # confirm this
}

# Main routine
sub main()
{
    if (!$result || defined($help))
    {
        print_help_msg();
        exit 0;
    }
    # the next statement is written in an easy to understand manner , it can be optimized 
    die "Mandatory arguments are not specified, try -help for more info\n" if( !( defined($rmconfigPath) || defined($apiListFile) )
                                                                                   && defined($covFile) && defined($bullseyeBin) );
    
   
    if(!(-f $covFile))
    {
        print ("No such COVFILE \n");
        exit 0;
    }
    $helperScript = givePreRequisite();
    if(defined ($rmconfigPath))
    {
        if(!(-d $rmconfigPath))
        {
            print ("No such Resman Config Path \n");
            exit 0;
        }
    }
    # Local variables
    my ($covCommand,@api_data,$current,$getSource,$bullseyeCommand,$excludeCommand,$selectCommand,$generateReportCommand);
    
    # Create the directory with time stamp
    my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
    
    my $dirname = "rmapicoverage_".$sec.$min.$hr.$dayofm.$month.$yr;
    if (defined($outputDir))
    {
        ($dirname = "$outputDir/") =~ s?//?/?g;
    }
    else
    {
        $current = getcwd;
        chomp($current);
        $dirname = "$current/$dirname";
    }
    $getSource = giveApiSourcePath();
    my $root = findRoot($getSource);
    
    if(defined($rmconfigPath))
    {
        $rmconfigPath  = replaceIfWindows($rmconfigPath);
        my $rmconfigCommand = "perl $rmconfigPath/rmconfig.pl --print apis";
        $rmconfigCommand = replaceIfWindows($rmconfigCommand);
        if(defined($resmanRoot))
        {
            if(!(-d $resmanRoot))
            {
                print ("No such Resman Root:Error \n");
                exit 0;
            }
            $rmconfigCommand ="$rmconfigCommand --resman-root $resmanRoot";
        }
        @api_data = `$rmconfigCommand`;
        die "Problem with rmconfig.pl try -resmanRoot option and try again" unless @api_data;
    }
    else
    {
        open(API,"+<","$apiListFile") || die "Not able to open $apiListFile";
        @api_data = <API>;
        close(API);
    }
    print "rmapicoverage.pl : Getting api coverage \n";
    $excludeCommand = "$bullseyeBin/covselect";
    $excludeCommand = replaceIfWindows($excludeCommand);
    $excludeCommand = "$excludeCommand  -f$covFile -a -v !$root";
    system($excludeCommand);
    die "failed to exclude all directories $excludeCommand \n" if (($?>>8) != 0);
    # Done one by one so that the user knows where there is an error
    foreach my $api(@api_data)
    {
        chomp($api);
        if(exists $funcList{$api})
        {
           $api .= $funcList{$api};
        }
        else
        {
            print "Cant find info for $api or Warning : bullseyebin prior to 7.13.25";
        }
        $selectCommand = "$bullseyeBin/covselect -f$covFile -a -v \"$getSource:$api\"";
        $selectCommand = replaceIfWindows($selectCommand);
        system($selectCommand);
        print "failed to include $api directory $selectCommand\n" if (($?>>8) != 0);#Need not die here
    }
    # Create Directory
    $dirname = replaceIfWindows($dirname);
    if(!(-d $dirname))
    {
        mkdir($dirname, 0755) || die "Not able to create directory $dirname\n";
    }
    ($bullseyeCommand   = "$bullseyeBin/covfn")=~ s?//?/?g;
    my $outputFile = "$dirname/report.csv";
    $outputFile = replaceIfWindows($outputFile);
    
    open(DAT,">>$outputFile") || die("Cannot Open File $outputFile");
    $generateReportCommand = "$bullseyeCommand -f$covFile -c";
    $generateReportCommand = replaceIfWindows($generateReportCommand);
    $generateReportCommand = `$generateReportCommand`;
    die "failed to create report $generateReportCommand\n" if (!($generateReportCommand));
    print DAT $generateReportCommand;
    close DAT;
    if(defined($listApis))
    {
        print ("The list of Apis lwrrently supported :\n");
        foreach my $api(@api_data)
        {
            print "$api \n";
        }
    }
    print ("\n\nSee their coverage in $covFile \n Find report.csv in $dirname\n");
    my $htmlCommand;
    $htmlCommand = "perl $helperScript -file $outputFile -outputDir $dirname -outputFileName resmanapicoverage.html";  
    $htmlCommand = replaceIfWindows($htmlCommand);
    system($htmlCommand);
    print "\nSEE COVERAGE REPORT IN => $dirname as resmanapicoverage.html\n";
}

main();
    



# This subroutine takes a string and replaces / with \ if the platform is Windows
sub replaceIfWindows
{
    my $argument = @_[0];
    # ^O eq MsWin32 for all versions of Windows .
    if ($^O eq "MSWin32") 
    {
        $argument =~ s?/?\\?g;
    }
    $argument =~ s?//?/?g ;
    $argument  =~ s{\\\\}{\\}g;
    return $argument;
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
        $htmlScriptPath = replaceIfWindows($htmlScriptPath);
        if(!(-f $htmlScriptPath) )
        {
            print "Please sync $htmlScriptPath or check your $toolsDirectory\n";
            exit 0;
        }
        else
        {
            return $htmlScriptPath;
        }
    }
    # Control will never come here
}
