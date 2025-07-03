#!/home/gnu/bin/perl
################################################
# Takes a list of files as input and produces 
# cci parsed copy of the files
################################################

use strict;
use warnings;
use Config;
use Getopt::Long;
use File::Basename;
use File::Spec;

my $clean  = '';
my $suffix = '';
my $help   = '';
my $perl   = $Config{perlpath};
my $gfiles = '';
my $cpath  = dirname(File::Spec->rel2abs($0));

#################################################
# Help: parse_files.pl [options] <files>
#    suffix => Suffix to be applied while naming parsed file
#    clean  => Clean the parsed files. 
#    files  => Original files incase option is not clean. 
#              Else list of parsed files.
#################################################
GetOptions ('clean' => \$clean, 'suffix=s' => \$suffix, 'help' => \$help);

sub printHelp
{
    print "parse_files.pl [--clean] [--help] [--suffix=<suffix>] <files>\n";
    print "    clean\n"
}

sub getParseFileName
{
    return $_.".$suffix".".c";

}

sub parseFiles
{
    foreach (@ARGV)
    {
        my $newFile = getParseFileName($_);
        system($^X, "$cpath/cci_pp.pl", ("$cpath/sec_enum_types.h", $_, $newFile));
        $gfiles = $gfiles.$newFile." ";
    }
}

sub cleanFiles
{
    foreach (@ARGV)
    {
        unlink $_;
    }
}

# Do sanity checks
if (!$clean)
{
    if (($suffix eq '') ||
        ($suffix eq ' ') ||
        ($suffix =~ /\s/))
    {
        die "Invalid suffix!";
        exit;
    }
    parseFiles();
    print $gfiles;
}
else
{
    cleanFiles();
}
