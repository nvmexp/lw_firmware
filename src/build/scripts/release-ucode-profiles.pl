#!/usr/bin/perl -w
#
# Copyright 2020 by LWPU Corporation.  All rights reserved.  All
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
use Data::Dumper;

BEGIN
{
    if(__FILE__ && -f __FILE__)
    {
        my $scriptDir = dirname __FILE__;
        unshift @INC, $scriptDir if (! grep {$_ eq $scriptDir} @INC);
    }
}
use util::P4Helper;

# Help Options
sub help
{
    print("Usage:\n");
    die("release-ucode-profiles.pl --profiles <profile-list> --source-path <src-path> --dest-path <dest-path>\n");
}

# Constants
my $p4               = "/home/utils/bin/p4";
my $binaryImageExt   = "_image.bin";
my $markedForRelease = "+";

# Path and file variables
my $profileFile    = "";
my $srcPath        = "";
my $destPath       = "";
my $bReleaseAll    = "";
my $bAlwaysRelease = "";
my $bOthers        = "";
my $bVerbose       = "";

# Set the path for the other p4 routines
p4Helper::p4SetPath($p4);

GetOptions("profiles=s"      => \$profileFile,
           "source-path=s"   => \$srcPath,
           "dest-path=s"     => \$destPath,
           "release-all"     => \$bReleaseAll, # Release all profiles and orphans
           "always-release"  => \$bAlwaysRelease,  # Release profile even if image binary has not changed
           "release-others"  => \$bOthers,
           "verbose"         => \$bVerbose);

# Sanity Checks
if ( ( $profileFile eq "" ) and ( ! $bReleaseAll ) )
{
    print("Profile list missing...\n");
    help();
}

if ( $srcPath eq "" )
{
    print("Source path missing...\n");
    help();
}

if ( $destPath eq "" )
{
    print("Destination path missing...\n");
    help();
}

# Flag descriptions
if ( $bReleaseAll )
{
    print("(release-all) All detected profiles and orphan files will be copied from src to dest.\n");
}

if ( $bAlwaysRelease )
{
    print("(always-release) Profiles marked for release will be released reguardless of change in image.\n");
}

if ( $bOthers )
{
    print("(release-others) Orphan files will be copied from src to dest.\n");
}

if ( $bVerbose )
{
    print("(verbose) Verbose script messaging enabled.\n");
}

##
## File Preparation
##
## Determine what files exist, and the profiles available.
## Find the relationship between the files in the source path
## and the profiles found. This will be used as apart of the
## release process.
##
my @sourceFiles = ();
my @sourceFileUsed = ();
my @profiles = ();
my @srcProfileList = ();
my @srcProfileListToRelease = ();
my @srcProfileListBuilt = ();

# Get all files from source dir
opendir my $sourceDir, "$srcPath" or die "Unable to open source directory: $!";
@sourceFiles = readdir $sourceDir;
closedir $sourceDir;

# Get all profiles flagged for release by makefile
if ( ! $bReleaseAll )
{
    open(FH, '<', $profileFile) or die "File '$profileFile' can't be opened";
    @srcProfileList = (<FH>);
    chomp(@srcProfileList);
    close(FH);

    # mark all profiles as built or for release
    foreach ( @srcProfileList )
    {
        if ( substr($_, 0, 1) eq $markedForRelease )
        {
            # get all of the profiles to release
            push(@srcProfileListToRelease, substr($_, 1, length($_)-1));
        }
        else
        {
            # get all of the profiles built
            push(@srcProfileListBuilt, $_);
        }
    }
}

if ( $bVerbose )
{
    print("Releasing profiles...\n\n");
    print("***** PROFILES BUILT\n");
    print(Dumper(@srcProfileListBuilt));
    print("***** PROFILES REQUESTED FOR RELEASE\n");
    print(Dumper(@srcProfileListToRelease));
}

# Initialize arrays to point files to profiles
foreach ( @sourceFiles )
{
    push(@sourceFileUsed, -1);

    # check the extension
    if ( substr($_, -10, 10) eq $binaryImageExt )
    {
        my $profile = substr($_, 0, length($_) - length($binaryImageExt));

        # Sometimes "prod" and "dbg" images will be made for a profile instead
        # of a single image
        if ( substr($profile,-4,4) eq "prod" )
        {
            $profile = substr($profile,0, length($profile)-5);
        }
        elsif ( substr($profile,-3,3) eq "dbg" )
        {
            $profile = substr($profile,0, length($profile)-4);
        }

        if ( ! grep $_ eq $profile, @profiles )
        {
            push(@profiles, $profile);
        }
    }
}

# sort profiles based on string length from longest to shortest
@profiles = reverse sort { length($a) <=> length($b) } @profiles;

# Find all files attributed to profile
for my $p ( 0 .. scalar(@profiles)-1 )
{
    my $profile = $profiles[$p];

    for my $f ( 0 .. scalar(@sourceFiles)-1 )
    {
        my $file = $sourceFiles[$f];

        if ( $sourceFileUsed[$f] != -1 ) { next; }

        if ( index($file, $profile) != -1 )
        {
            $sourceFileUsed[$f] = $p;
        }
    }
}

# Display file to profile relationships
if ( $bVerbose )
{
    print("***** PROFILES\n");
    print(Dumper(@profiles));
    print("***** FILES\n");
    for my $i ( 0 .. scalar(@sourceFiles)-1 )
    {
        if ( $sourceFileUsed[$i] != -1 )
        {
            print sprintf("%-80s => %-40s\n", $sourceFiles[$i], $profiles[$sourceFileUsed[$i]]);
        }
        else
        {
            print sprintf("%-80s => %-40d\n", $sourceFiles[$i], -1);
        }
    }
}

##
## Release Loop
##
## Determine what files need to be potentially release,
## perform a comparision and then the actual action of
## release if necessary.
##

# Release files attributed to profiles
for my $p ( 0 .. scalar(@profiles)-1 )
{
    # Standard image
    my $file = join("", $profiles[$p], $binaryImageExt);
    my $srcImage = catfile($srcPath, $file);
    my $destImage = catfile($destPath, $file);

    # Hypothetical _prod and _dbg images
    my $prodFile = join("", $profiles[$p], "_prod", $binaryImageExt);
    my $prodSrcImage = catfile($srcPath, $prodFile);
    my $prodDestImage = catfile($destPath, $prodFile);

    my $dbgFile = join("", $profiles[$p], "_dbg", $binaryImageExt);
    my $dbgSrcImage = catfile($srcPath, $dbgFile);
    my $dbgDestImage = catfile($destPath, $dbgFile);

    my @filesToRelease = ();

    # differed(1), equal(0), error(-1)
    my $bImagesDiffer = 0;
    my $bDbgImagesDiffer = 0;
    my $bProdImagesDiffer = 0;

    # scenarios:
    # | src exists | dst exists | result
    # | 1          | 1          | compare files
    # | 1          | 0          | images differ, copy source
    # | 0          | 1          | nothing to copy over, do nothing
    # | 0          | 0          | no files to compare/copy, do nothing

    # singular image comparison
    if (-e $srcImage)
    {
        if (-e $destImage)
        {
            $bImagesDiffer = File::Compare::compare($srcImage, $destImage);
        }
        else
        {
            # copy over the file by default
            $bImagesDiffer = 1;
        }
    }

    # debug image comparison
    if (-e $dbgSrcImage)
    {
        if (-e $dbgDestImage)
        {
            $bDbgImagesDiffer = File::Compare::compare($dbgSrcImage, $dbgDestImage);
        }
        else
        {
            # copy over the file by default
            $bDbgImagesDiffer = 1;
        }
    }

    # prod image comparison
    if (-e $prodSrcImage)
    {
        if (-e $prodDestImage)
        {
            $bProdImagesDiffer = File::Compare::compare($prodSrcImage, $prodDestImage);
        }
        else
        {
            # copy over the file by default
            $bProdImagesDiffer = 1;
        }
    }

    # combine comparison values
    $bImagesDiffer = $bImagesDiffer || $bDbgImagesDiffer || $bProdImagesDiffer;

    if ( $bVerbose )
    {
        print("file: $file\n");
        print("Source: $srcImage\n");
        print("Dest: $destImage\n");
        print("Differed?: $bImagesDiffer\n");
        print("\n");
    }

    # Make a list of files to release
    $totalFiles = 0;
    $copyFileCount = 0;
    for my $i ( 0 .. scalar(@sourceFileUsed)-1 )
    {
        if ( $sourceFileUsed[$i] == $p )
        {
            # compare src & dest file
            my $bFilesDiffer = 0;
            my $srcFile = catfile($srcPath, $sourceFiles[$i]);
            my $destFile = catfile($destPath, $sourceFiles[$i]);

            $totalFiles = $totalFiles + 1;

            # copy over only if differed(1), not equal(0) or error(-1)
            if (-e $srcFile)
            {
                if (-e $destFile)
                {
                    $bFilesDiffer = File::Compare::compare($srcFile, $destFile);
                }
                else
                {
                    # copy over the file by default
                    $bFilesDiffer = 1;
                }
            }

            if ( $bFilesDiffer == 1 )
            {
                $copyFileCount = $copyFileCount + 1;
                push(@filesToRelease, catfile($srcPath, $sourceFiles[$i]));
            }
        }
    }

    # copy over all profiles no matter what
    if ( $bReleaseAll )
    {
        if ( $bVerbose )
        {
            print("Releasing profile...\n");
            printf("( %d / %d ) files to be released.\n\n", $copyFileCount, $totalFiles);
        }

        # Perform the release
        release(\@filesToRelease, $destPath);
        next;
    }

    # This profile is built by the compiler and needs to be released
    if ( grep $_ eq $profiles[$p], @srcProfileListToRelease )
    {

        if ( $bImagesDiffer || $bAlwaysRelease )
        {
            if ( $bVerbose )
            {
                print("Releasing profile...\n");
                printf("( %d / %d ) files to be released.\n\n", $copyFileCount, $totalFiles);
            }

            # Perform the release
            release(\@filesToRelease, $destPath);
        }
    }

    # This profile is built by the compiler and doesnt need to be released
    elsif ( grep $_ eq $profiles[$p], @srcProfileListBuilt )
    {
        # do nothing
    }

    # Profiles that are not built by the compiler
    else
    {
        if ( $bVerbose )
        {
            print("Releasing profile...\n");
            printf("( %d / %d ) files to be released.\n\n", $copyFileCount, $totalFiles);
        }
        # Perform the release
        release(\@filesToRelease, $destPath);
    }
}

# Release files not associated with profile
if ( ! $bOthers and ! $bReleaseAll )
{
    exit;
}
elsif ( $bVerbose )
{
    print("Releasing orphaned files...\n");
}

my @filesToRelease = ();

for my $f ( 0 .. scalar(@sourceFiles)-1 )
{
    if ( ( $sourceFiles[$f] eq "." ) || ( $sourceFiles[$f] eq ".." ) )
    {
        next;
    }

    if ( $sourceFileUsed[$f] == -1 )
    {
        push(@filesToRelease, catfile($srcPath, $sourceFiles[$f]));
    }
}

if ( $bVerbose )
{
    print("***** ORPHANS\n");
    print(Dumper(@filesToRelease));
}

# Perform the release
release(\@filesToRelease, $destPath);

##
## Helper Functions
##
## Borrowed from ucode/build/scripts/release-imgs-if-changed.pl
##

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

        $bSuccess = p4Helper::p4Edit($dstFile);

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

        if ($bAddFile)
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
