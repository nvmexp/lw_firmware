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
my $file;

my $result = GetOptions("file=s"      => \$file);


sub main()
{
    my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
    my $outputDir = getcwd;
    my $outputFile = "$outputDir/output.$sec.$min.$hr.$dayofm.$month.$yr.html";
    $outputFile = replaceIfWindows($outputFile);
    my (@lines,%hashPerc);
    my $flag  = 1;
    open(DAT1,">> $outputFile") || die "Cannot open file : $outputFile\n";
    print DAT1 "<html>\n<title bgcolor=\"blue\"></title>\n<body>\n<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\" align=\"center\"\n";
    open(DAT2,"$file") || die "Cannot open file : $file\n";
    @lines = <DAT2>;
    close DAT2;
    print DAT1 "<TR> ";

    foreach my $line(@lines)
    {
        my @tokens = split(/,/,$line);
        print DAT1 "<TR> ";
        my $tokenNo = 1;
        my $cnt = 1;
        foreach my $token(@tokens)
        {
            $token =~ s?"??g;
            if($flag == 1)
            {
                if($token =~ m/%/)
                {
                   $hashPerc{$cnt} = 1;
                }
                print DAT1"<TD bgcolor=\"#73656E\"> <b> $token  </b> </TD> ";
                $cnt++;
            }
            else
            {
                if(exists $hashPerc{$tokenNo})
                {
                    if($token < 25)
                    {
                        print DAT1"<TD bgcolor=\"red\"> <b> $token  </b> </TD> ";
                    }
                    elsif($token < 75)
                    {
                        print DAT1"<TD bgcolor=\"yellow\"> <b> $token  </b> </TD> ";		
                    }
                    else
                    {
                        print DAT1"<TD bgcolor=\"green\"> <b> $token  </b> </TD> ";
                    }
                }
                else
                {
                    print DAT1"<TD bgcolor=\"#FDEEF4\"> $token </TD> ";
                }	
                $tokenNo++;
            }
            
        }
        print DAT1 "</TR>\n";
        $flag = 0;
    }
    print DAT1 "</table> \n </body> \n </html> \n";
    close DAT1;
    return;
}
main();
sub replaceIfWindows
{
    my $argument = @_[0];
    # ^O eq MsWin32 for all versions of Windows .
    if ($^O eq "MSWin32") 
    {
        $argument =~ s?/?\\?g;
    }
    return $argument;
}

