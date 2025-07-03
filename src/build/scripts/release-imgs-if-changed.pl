#!/usr/bin/perl -w
#
# Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

use strict;
no strict qw(vars);
use warnings "all";

use Cwd qw(realpath);
use File::Copy;
use File::Path;
use File::Compare;
use File::Basename;
use File::Spec::Functions qw(catfile abs2rel rel2abs);
use Getopt::Long;
use Carp;

BEGIN
{
    if(__FILE__ && -f __FILE__)
    {
        my $scriptDir = dirname __FILE__;
        unshift @INC, $scriptDir if (! grep {$_ eq $scriptDir} @INC);
    }
}
use util::P4Helper;

my $releasePath            = "";
my $releasePathHal         = "";
my $srcImage               = "";
my $dstImage               = "";
my $srcSymbols             = "";
my $dstSymbols             = "";
my $srcLoggingBin          = "";
my $dstLoggingBin          = "";
my @releaseFiles           = ();
my @releaseFilesHal        = ();
my $output_prefix          = "";
my $p4                     = "p4";
my $bForce                 = '';
my $onDVS                  = defined $ELW{'LW_DVS_BLD'};

# extract command-line arguments
GetOptions("release-path=s"             => \$releasePath,
           "release-path-hal=s"         => \$releasePathHal,
           "release-files=s"            => \@releaseFiles,
           "release-files-hal=s"        => \@releaseFilesHal,
           "image=s"                    => \$srcImage,
           "symbols=s"                  => \$srcSymbols,
           "logging=s"                  => \$srcLoggingBin,
           "output-prefix=s"            => \$output_prefix,
           "force"                      => \$bForce,
           "p4=s"                       => \$p4);

p4Helper::p4SetPath($p4); # Set the path for the other p4 routines

@releaseFiles    = split(/,/,join(',',@releaseFiles));
@releaseFilesHal = split(/,/,join(',',@releaseFilesHal));

#
# Compare the source and destination images to see if they differ. Provide a
# custom line-comparison routine to Perl's builtin file-comparison function so
# that we can ignore date and version strings since they will always differ.
#
$dstImage = catfile($releasePath, basename($srcImage));
my $bImagesDiffer = File::Compare::compare($srcImage, $dstImage);

$dstImage = catfile($releasePathHal, basename($srcImage));
my $bHalImagesDiffer = File::Compare::compare($srcImage, $dstImage);

my $bSymbolsDiffer = 0;
if ($srcSymbols ne "")
{
    $dstSymbols = catfile($releasePath, basename($srcSymbols));
    $bSymbolsDiffer = File::Compare::compare_text($srcSymbols, $dstSymbols, \&symbols_differ);
}

my $bLoggingBinsDiffer = 0;
if ($srcLoggingBin ne "")
{
    $dstLoggingBin = catfile($releasePath, basename($srcLoggingBin));
    $bLoggingBinsDiffer = File::Compare::compare($srcLoggingBin, $dstLoggingBin);
}

#
# If the images differ (or if --force was specfied), perform a "release" by
# copying the release files to the release directory. The release directory
# should already contain old versions of the files, and the files should be
# checked into perforce. 'p4 edit' them before the copy. If the the files do
# not yet exist in the release directory, create the release directory (if
# required), copy the files, and 'p4 add' them.
#
if ($bImagesDiffer || $bSymbolsDiffer || $bLoggingBinsDiffer || $bForce)
{
    if (!$bImagesDiffer && !$bForce)
    {
        if ($bSymbolsDiffer)
        {
            if ($bLoggingBinsDiffer)
            {
                print "Only symbols and logging metadata changed...\n";
            }
            else
            {
                print "Only symbols changed...\n";
            }

            # Some files are not generated when only symbols are changed
            # filter out missing files
            @releaseFiles = grep { -e $_ } @releaseFiles;
        }
        elsif ($bLoggingBinsDiffer)
        {
            print "Only logging metadata changed...\n";
        }
    }
    print "Releasing files...\n";
    release(\@releaseFiles          , $releasePath);
}

if ($bHalImagesDiffer || $bForce)
{
    release(\@releaseFilesHal       , $releasePathHal);
}

# release all files in arg1 (array) to the destination path in arg2
sub release
{
    my $files = shift;
    my $path  = shift;

    foreach my $srcFile (@$files)
    {
        my $dstFile  = catfile($path, basename($srcFile));
        my $bSuccess = 0;
        my $bAddFile = 0;

        # On DVS we do not want to perform any p4 related operations
        if (not $onDVS)
        {
            $bSuccess = p4Helper::p4Edit($dstFile);
        }
        else
        {
            $bSuccess = 1;
        }

        #
        # If the 'p4 edit' failed, it could be because we're releasing a set of
        # new images that don't yet exist in the release directory. Check to
        # see if that is the case. If so, set a flag that tells us to 'p4 add'
        # them after copying them. If not, we hit some other unexpected
        # failure; just bail.
        #
        # If the files don't exist yet, the release directory itself may not
        # even exist. In that case, it needs created before the files can be
        # copied.
        #
        if (! -d $path) # If perforce failed because of missing path, make the path
        {

            print "Making directory: $path\n";
            create_dirs($path);
            $bAddFile = 1;
        }
        elsif (!$bSuccess) # Otherwise perforce failed for some other reason
        {
            die "Unexpected error moving $srcFile to: $dstFile" unless !p4Helper::p4Exists($dstFile);
            $bAddFile = 1;
        }

        copy($srcFile, $dstFile) or die "Copy $srcFile to $dstFile failed: $!";

        # Force updating the timestamp to the local time
        # Signature files from server are with timestamp of the remote system and sometimes
        # cause 'make' dependency problem when the files are processed by other tools (bindata.pl).
        touch($dstFile);

        if ($bAddFile and not $onDVS)
        {
            p4Helper::p4Add($dstFile) or die "Cannot add $dstFile to perforce.";
        }
    }
}

# perl doesn't provide 'touch'
# implement the utility helper function with 'utime'
sub touch
{
    my $file = shift;
    utime(undef, undef, $file)  or die "Cannot touch $file: $!";
}

# Utility function to relwrsively create directories
sub create_dirs
{
    my $path = shift;
    my @dirs = File::Spec->splitdir($path);
    my $dir;
    for (my $i = 1; $i <= $#dirs; $i++)
    {
        $dir = File::Spec->catdir(@dirs[0..$i]);
        next if -d $dir;
        File::Path::mkpath($dir);
    }
}

# Utility function to determine if two lines in a symbols file differ.
# To be used with File::Compare::compare_text().
sub symbols_differ
{
    $line1 = shift;
    $line2 = shift;

    # AppVersion line at the tops of the symbol files can change from build to
    # build. Masking out these lines to prevent false-positive matches on
    # a symbol diff.
    $line1 =~ s/AppVersion.*/ /g;
    $line2 =~ s/AppVersion.*/ /g;

    return $line1 ne $line2;
}
