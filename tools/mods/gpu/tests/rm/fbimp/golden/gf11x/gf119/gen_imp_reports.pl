#!/home/utils/perl-5.8.8/bin/perl
##
## File:   gen_imp_reports.pl
## Desc:   This script generates spreadsheet files containing the IMP output settings
##         for the verification IMP implementation of gf11x chips.
## Usage:  ./gen_imp_reports.pl $rootDir
##         The script will search the directories under $rootDir relwrsively for the
##         IMP output setting files (end with either '.out' or'.golden') and create
##         the spreadsheet file under each directory for all IMP output setting files
##         under that directory.
##           
## Author: Henry Xu
##
use strict;
use warnings;
use Data::Dumper;

#-----------------------------------------------------------------------------------------------------------
# Constant definitions.
#-----------------------------------------------------------------------------------------------------------
use constant
{
    FALSE => 0,
    TRUE  => 1
};

use constant
{
    PROGRESSIVE                     => 0,
    LW_RASTER_STRUCTURE_PROGRESSIVE => 0,
    
    INTERLACED                      => 1,
    LW_RASTER_STRUCTURE_INTERLACED  => 1
};

use constant
{
    EVR_NO_SCALING        => 0,
    EVR_IN_LT_MIN         => 1,
    EVR_IN_INSIDE_MIN_MAX => 2,
    EVR_IN_GT_MAX         => 3
};

use constant
{
    GF11X_MAX_HEADS => 2,
    GF11X_MAX_DACS  => 4,
    GF11X_MAX_SORS  => 8,
    GF11X_MAX_PIORS => 4
};

#-----------------------------------------------------------------------------------------------------------
# Public functions.
#-----------------------------------------------------------------------------------------------------------
# Entry point of this script.
&main ();

# Function: main
# Desc:     the main function of this script.
sub main ()
{
    my $strRootDir;
    my $strFilter;
    
    if (defined ($ARGV[0]) && ($ARGV[0] eq "-diff"))
    {
        if (scalar @ARGV < 3)
        {
            print "Usage: gen_imp_reports.pl -diff archImpOutDir rmImpOutDir [output_setting_filter]\n";
            return;
        }
        $strFilter = $ARGV[3];
        if (!defined ($strFilter))
        {
            $strFilter = "all";
        }
        &diff_imp_results ($ARGV[1], $ARGV[2], $strFilter);
    }
    else
    {
        if (defined ($ARGV[0]))
        {
           $strRootDir = $ARGV[0];
        }
        else
        {
            $strRootDir = `pwd`;
            chomp $strRootDir;
        }
        &gen_imp_reports ($strRootDir);
    }
}

# Function: diff_imp_results
# Desc:     this function relwrsively diffs the IMP output setting files under the
#           ARCH IMP output directory and the RM IMP output directory.
sub diff_imp_results ()
{
    my ($strArchRootDir, $strRmRootDir, $strFilter) = @_;
    my (@archImpOuts, @rmImpOuts);
    my ($refArchImpOut, $refRmImpOut);
    my @archOnlyImpOuts;
    my $bFound;
    my ($refArchImpHeadsInfo, $refRmImpHeadsInfo);
    my ($refArchImpHeadInfo, $refRmImpHeadInfo);
    my ($strKey, $strArchValue, $strRmValue);
    my (@strKeyList, @strArchValueList, @strRmValueList);
    my ($strDiffInfo, $bDiffFound);
    my $headIdx;
    
    open DIFF_FILE, ">arch_rm_imp_diff.csv";
    
    print "Load ARCH IMP output files...\n";
    &load_imp_results ($strArchRootDir, \@archImpOuts);
    print "\nLoad RM IMP output files...\n";
    &load_imp_results ($strRmRootDir, \@rmImpOuts);
    
    # Diff the common IMP output files.
    foreach $refArchImpOut (@archImpOuts)
    {
        $bFound = FALSE;
        foreach $refRmImpOut (@rmImpOuts)
        {
            if ($refArchImpOut->{inputFile} eq $refRmImpOut->{inputFile})
            {
                $strDiffInfo = "";
                $bDiffFound = FALSE;
                $refArchImpHeadsInfo = $refArchImpOut->{impHeadsInfo};
                $refRmImpHeadsInfo = $refRmImpOut->{impHeadsInfo};
                for ($headIdx = 0; $headIdx < scalar @$refArchImpHeadsInfo; $headIdx++)
                {
                    $refArchImpHeadInfo = $$refArchImpHeadsInfo[$headIdx];
                    $refRmImpHeadInfo = $$refRmImpHeadsInfo[$headIdx];
                    
                    @strKeyList = ();
                    @strArchValueList = ();
                    @strRmValueList = ();
                    foreach $strKey (sort keys %$refArchImpHeadInfo)
                    {
                        if ((($strFilter eq "all") or ($strFilter =~ m/$strKey/) or ($strKey =~ m/$strFilter/)) &&
                            !($strKey =~ m/mclk/))
                        {
                            $strArchValue = $refArchImpHeadInfo->{$strKey};
                            if (!defined ($strArchValue))
                            {
                                $strArchValue = "undef";
                            }
                            $strRmValue = $refRmImpHeadInfo->{$strKey};
                            if (!defined ($strRmValue))
                            {
                                $strRmValue = "undef";
                            }
                            
                            if ($strArchValue ne $strRmValue)
                            {
                                push @strKeyList, $strKey;
                                push @strArchValueList, $strArchValue;
                                push @strRmValueList, $strRmValue;
                            }
                        }
                    }
                    
                    # Print the key.
                    $strDiffInfo .= ", head $headIdx";
                    foreach $strKey (@strKeyList)
                    {
                        $strDiffInfo .= ", $strKey";
                        $bDiffFound = TRUE;
                    }
                    $strDiffInfo .= "\n";
                    
                    # Print the ARCH value.
                    $strDiffInfo .= ", Arch";
                    foreach $strArchValue (@strArchValueList)
                    {
                        $strDiffInfo .= ", $strArchValue";
                    }
                    $strDiffInfo .= "\n";
                    
                    # Print the RM value.
                    $strDiffInfo .= ", RM";
                    foreach $strRmValue (@strRmValueList)
                    {
                        $strDiffInfo .= ", $strRmValue";
                    }
                    $strDiffInfo .= "\n";
                }
                
                if ($bDiffFound)
                {
                    print DIFF_FILE "diff: $refArchImpOut->{inputFile}\n";
                    print DIFF_FILE $strDiffInfo;
                    print DIFF_FILE "\n";
                }
                $bFound = TRUE;
            }
        }
        if (!$bFound)
        {
            push @archOnlyImpOuts, $refArchImpOut;
        }
    }
    
    # List the arch-only IMP output files.
    foreach $refArchImpOut (@archOnlyImpOuts)
    {
        print DIFF_FILE "arch_only: $refArchImpOut->{inputFile}\n";
    }
    
    # List the rm-only IMP output files.
    foreach $refRmImpOut (@rmImpOuts)
    {
        $bFound = FALSE;
        foreach $refArchImpOut (@archImpOuts)
        {
            if ($refArchImpOut->{inputFile} eq $refRmImpOut->{inputFile})
            {
                $bFound = TRUE;
            }
        }
        if (!$bFound)
        {
            print DIFF_FILE "rm_only: $refRmImpOut->{inputFile}\n";
        }
    }
    close DIFF_FILE;
}

# Function: load_imp_results
# Desc:     this function loads the IMP output setting files into a table.
sub load_imp_results ()
{
    my ($strLwrDir, $refImpOuts) = @_;
    my (@strNameList, $strName);
    my (@strFileList, @strDirList);
    my $strFullPath;    
    
    # Info log.
    print "Load from the dir: $strLwrDir\n";
    
    # Find IMP output setting files to process and sub-directories to traverse further.
    if (opendir (DIR, $strLwrDir))
    {
        @strNameList = readdir (DIR);
        closedir (DIR);
        foreach $strName (@strNameList)
        {
            $strFullPath = "$strLwrDir/$strName";
            if (($strName eq ".") || ($strName eq ".."))
            {
                next;
            }
            elsif (($strName =~ m/\.out/) || ($strName =~ m/\.golden/))
            {
                push @$refImpOuts, &get_imp_info ($strFullPath);
            }
            elsif (-d $strFullPath)
            {
                push @strDirList, $strFullPath;
            }
        }
    }
    
    # Traverse sub-directories to load IMP output settings files under them.
    foreach $strFullPath (@strDirList)
    {
        &load_imp_results ($strFullPath, $refImpOuts);
    }
}

# Function: gen_imp_reports
# Desc:     this function relwrsively generate the IMP report spreadsheet files under
#           the specified directory and its sub-directories.
sub gen_imp_reports ()
{
    my $strLwrDir = shift @_;
    my (@strNameList, $strName);
    my (@strFileList, @strDirList);
    my $strFullPath;
    
    # Info log.
    print "Process the dir: $strLwrDir\n";
    
    # Find IMP output setting files to process and sub-directories to traverse further.
    if (opendir (DIR, $strLwrDir))
    {
        @strNameList = readdir (DIR);
        closedir (DIR);
        foreach $strName (@strNameList)
        {
            $strFullPath = "$strLwrDir/$strName";
            if (($strName eq ".") || ($strName eq ".."))
            {
                next;
            }
            elsif (($strName =~ m/\.out/) || ($strName =~ m/\.golden/))
            {
                push @strFileList, $strFullPath;
            }
            elsif (-d $strFullPath)
            {
                push @strDirList, $strFullPath;
            }
        }
    }
    
    # Generate the IMP report spreadsheet file for all IMP output setting files
    # under current directory.
    &gen_imp_report ($strLwrDir, \@strFileList);
    
    # Traverse sub-directories to generate IMP report spreadsheet files for them.
    foreach $strFullPath (@strDirList)
    {
        &gen_imp_reports ($strFullPath);
    }
}

# Function: gen_imp_report
# Desc:     this function generates the IMP report spreadsheet file for all IMP output
#           setting files under current directory
sub gen_imp_report ()
{
    my ($strLwrDir, $refImpOutputFileList) = @_;
    my $strFullPath;
    my ($refImpInfo, $refImpHeadsInfo, $refImpHeadInfo);
    my $strLeafDir;
    
    if (scalar @$refImpOutputFileList == 0)
    {
        return;
    }

    # Create the spreadsheet file.
    $strLeafDir = $strLwrDir;
    $strLeafDir =~ s|^.*/||g;
    open CSV_FILE, ">${strLeafDir}_imp_report.csv";
    foreach $strFullPath (@$refImpOutputFileList)
    {
        $refImpInfo = &get_imp_info ($strFullPath);
        print CSV_FILE "$refImpInfo->{inputFile}\n";
        
        print CSV_FILE "head, mode, width, height, hblank, vblank, pclk, " .
                       "base, ovly, scaling, maxFlipPerf, baseLineBuf, ovlyLineBuf, " .
                       ", " .
                       "dispIMP, dmiDuration, reqdDispClk, maxDispClk, reqdDispClkRatio, fbFetchRateKHz, " .
                       "dmiMeter, dispClkActual, minFillRate, avgFetchRate, " .
                       ", " .
                       "isohubIMP, latBufBlocks, memPoolBlocks, baseFullyLinesBuffered, baseLimit, " .
                       "ovlyFullyLinesBuffered, ovlyLimit, fetchMeter, fetchLimit, " .
                       "bIsASRPossible, asrEfficiency, asrMode, asrSubMode, asrHWM, asrLWM, " .
                       "mclkPossible, mclkMode, mclkSubMode, mclkDwcfWM, mclkMidWM\n";
        $refImpHeadsInfo = $refImpInfo->{impHeadsInfo};
        foreach $refImpHeadInfo (@$refImpHeadsInfo)
        {
            print CSV_FILE "$refImpHeadInfo->{head}, " .
                           "$refImpHeadInfo->{mode}, " .
                           "$refImpHeadInfo->{width}, $refImpHeadInfo->{height}, " .
                           "$refImpHeadInfo->{hblank}, $refImpHeadInfo->{vblank}, " .
                           "$refImpHeadInfo->{pclk}, " .
                           "$refImpHeadInfo->{base}, $refImpHeadInfo->{ovly}, " .
                           "$refImpHeadInfo->{scaling}, " .
                           "$refImpHeadInfo->{maxFlipPerf}, " .
                           "$refImpHeadInfo->{baseLineBuf}, $refImpHeadInfo->{ovlyLineBuf}, " .
                           ", " .
                           "$refImpHeadInfo->{dispIMP}, $refImpHeadInfo->{dmiDuration}, $refImpHeadInfo->{reqdDispClk}, " .
                           "$refImpHeadInfo->{maxDispClk}, $refImpHeadInfo->{reqdDispClkRatio}, $refImpHeadInfo->{fbFetchRateKHz}, " .
                           "$refImpHeadInfo->{dmiMeter}, $refImpHeadInfo->{dispClkActual}, $refImpHeadInfo->{minFillRate}, " .
                           "$refImpHeadInfo->{avgFetchRate}, " .
                           ", " .
                           "$refImpHeadInfo->{isohubIMP}, $refImpHeadInfo->{latBufBlocks}, $refImpHeadInfo->{memPoolBlocks}, " .
                           "$refImpHeadInfo->{baseFullyLinesBuffered}, $refImpHeadInfo->{baseLimit}, $refImpHeadInfo->{ovlyFullyLinesBuffered}, " .
                           "$refImpHeadInfo->{ovlyLimit}, $refImpHeadInfo->{fetchMeter}, $refImpHeadInfo->{fetchLimit}, " .
                           "$refImpHeadInfo->{bIsASRPossible}, $refImpHeadInfo->{asrEfficiency}, " .
                           "$refImpHeadInfo->{asrMode}, $refImpHeadInfo->{asrSubMode}, " .
                           "$refImpHeadInfo->{asrHWM}, $refImpHeadInfo->{asrLWM}, " .
                           "$refImpHeadInfo->{mclkPossible}, " .
                           "$refImpHeadInfo->{mclkMode}, $refImpHeadInfo->{mclkSubMode}, " .
                           "$refImpHeadInfo->{mclkDwcfWM}, $refImpHeadInfo->{mclkMidWM}\n";
        }
        print CSV_FILE "\n";
    }
    close CSV_FILE;
}

# Function: get_imp_info
# Desc:     this function gets the IMP input and output info from the IMP output setting file.
sub get_imp_info ()
{
    my $strFullPath = shift @_;
    my $strFileName;
    my ($strToken, @strTokenList);
    my $headIdx;
    my ($strChip, $strPlatform, $strPState, $strASR, $strMclkSwitch);
    my ($strHead, $strModeName, $strFreq, $strRaster, $strScalerMode, $strVPR, $strBaseBpp, $strBaseFos);
    my ($strOvlyBpp, $strMaxFlipPerf, $strBaseLineBuf, $strOvlyLineBuf, $strDramType);
    my ($rasterWidth, $rasterHeight, $vpWidth, $vpHeight, $hblank, $vblank, $pclk);
    my ($dispIMP, $reqdDispClk, $maxDispClk, $reqdDispClkRatio, $fbFetchRateKHz, $dmiMeter);
    my ($dispClkActual, $minFillRate, $avgFetchRate, $dmiDuration);
    my ($isohubIMP, $latBufBlocks, $memPoolBlocks, $baseFullyLinesBuffered, $baseLimit);
    my ($ovlyFullyLinesBuffered, $ovlyLimit, $fetchMeter, $fetchLimit);
    my ($asrMode, $asrSubMode, $asrHWM, $asrLWM, $bIsASRPossible, $asrEfficiency);
    my ($mclkPossible, $mclkMode, $mclkSubMode, $mclkDwcfWM, $mclkMidWM);
    my %impInfo;
    my @impHeadsInfo;
    my $refImpOutSettings;

    # Load the IMP output settings.
    $refImpOutSettings = &load_imp_output_file ($strFullPath);
    
    # Get the IMP output setting file name.
    $strFileName = $strFullPath;
    $strFileName =~ s|^.*/||;
    $strFileName =~ s/\..*$//;
    
    # Get IMP input settings shared by all heads.
    @strTokenList = split ("_", $strFileName);    
    $strChip = shift @strTokenList;
    $strPlatform = shift @strTokenList;
    $strPState = shift @strTokenList;
    $strASR = "no_asr";
    $strMclkSwitch = "no_mclk";
    while ($strTokenList[0] ne "h0")
    {
        $strToken = shift @strTokenList;
        if ($strToken eq "asr")
        {
            $strASR = $strToken;
        }
        if ($strToken eq "mclk")
        {
            $strMclkSwitch = $strToken;
        }
    }
    
    %impInfo = 
    (
        inputFile  => $strFileName,
        chip       => $strChip,
        pstate     => $strPState,
        asr        => $strASR,
        mclkSwitch => $strMclkSwitch
    );
    
    # Get per-head IMP input settings.
    for ($headIdx = 0; $headIdx < GF11X_MAX_HEADS; $headIdx ++)
    {
        if ($strTokenList[0] eq "h${headIdx}")
        {
            my %impHeadInfo;
            
            # Get IMP head in info.
            $strHead = shift @strTokenList;
            $strModeName = shift @strTokenList;
            $strFreq = shift @strTokenList;
            $strRaster = shift @strTokenList;
            if (($strTokenList[0] eq "444") || ($strTokenList[0] eq "422"))
            {
                $strScalerMode = shift @strTokenList;
                $strVPR = shift @strTokenList;
            }
            else
            {
                $strScalerMode = "NA";
                $strVPR = "no scaling";
            }
            $strBaseBpp = shift @strTokenList;
            $strBaseFos = shift @strTokenList;
            if ($strTokenList[0] eq "ovly")
            {
                shift @strTokenList;
                $strOvlyBpp = shift @strTokenList;
            }
            else
            {
                $strOvlyBpp = "none";
            }
            $strMaxFlipPerf = shift @strTokenList;
            $strBaseLineBuf = shift @strTokenList;
            $strOvlyLineBuf = shift @strTokenList;
            
            $rasterWidth = $refImpOutSettings->{"impDispHeadIn[$headIdx].RasterSize.Width"};
            $rasterHeight = $refImpOutSettings->{"impDispHeadIn[$headIdx].RasterSize.Height"};
            $vpWidth = $refImpOutSettings->{"impDispHeadIn[$headIdx].ViewportSizeOut.Width"};
            $vpHeight = $refImpOutSettings->{"impDispHeadIn[$headIdx].ViewportSizeOut.Height"};
            $hblank = $rasterWidth - $vpWidth;
            $vblank = $rasterHeight - $vpHeight;
            $pclk = $refImpOutSettings->{"impDispHeadIn[$headIdx].PixelClock.Frequency"};
            
            # Get dispIMP out info.
            $dispIMP = $refImpOutSettings->{"output.impDispOut.isPossible"};
            if (!defined ($dispIMP))
            {
                $dispIMP = $refImpOutSettings->{"output.result"};
            }
            $reqdDispClk = $refImpOutSettings->{"output.impDispOut.reqdDispClk"};
            $maxDispClk = $refImpOutSettings->{"output.impDispOut.maxDispClk"};
            $reqdDispClkRatio = $refImpOutSettings->{"output.impDispOut.reqdDispClkRatio"};
            $fbFetchRateKHz = $refImpOutSettings->{"output.impDispOut.fbFetchRateKHz[$headIdx]"};
            $dmiMeter = $refImpOutSettings->{"output.impDispOut.meter[$headIdx]"};
            $dispClkActual = $refImpOutSettings->{"output.impDispOut.dispClkActual[$headIdx]"};
            if (!defined ($dispClkActual))
            {
                $dispClkActual = $refImpOutSettings->{"output.impDispOut.dispClkActualKHz[$headIdx]"};
            }
            $minFillRate = $refImpOutSettings->{"output.impDispOut.minFillRate[$headIdx]"};
            if (!defined ($minFillRate))
            {
                $minFillRate = $refImpOutSettings->{"output.impDispOut.minFillRateKHz[$headIdx]"};
            }
            $avgFetchRate = $refImpOutSettings->{"output.impDispOut.avgFetchRate[$headIdx]"};
            if (!defined ($avgFetchRate))
            {
                $avgFetchRate = $refImpOutSettings->{"output.impDispOut.avgFetchRateKHz[$headIdx]"};
            }
            $dmiDuration = $refImpOutSettings->{"output.impIsohubOut.dmiDuration[$headIdx]"};

            # Get isohubIMP out info.
            $isohubIMP = $refImpOutSettings->{"output.impIsohubOut.bIsPossible"};
            if (!defined ($isohubIMP))
            {
                $isohubIMP = $refImpOutSettings->{"output.result"};
            }
            $latBufBlocks = $refImpOutSettings->{"output.impIsohubOut.memPoolSetting[$headIdx].latBufBlocks"};
            $memPoolBlocks = $refImpOutSettings->{"output.impIsohubOut.memPoolSetting[$headIdx].memPoolBlocks"};
            $baseFullyLinesBuffered = $refImpOutSettings->{"output.impIsohubOut.memPoolSetting[$headIdx].baseFullyLinesBuffered"};
            $baseLimit = $refImpOutSettings->{"output.impIsohubOut.memPoolSetting[$headIdx].baseLimit"};
            $ovlyFullyLinesBuffered = $refImpOutSettings->{"output.impIsohubOut.memPoolSetting[$headIdx].ovlyFullyLinesBuffered"};
            $ovlyLimit = $refImpOutSettings->{"output.impIsohubOut.memPoolSetting[$headIdx].ovlyLimit"};
            $fetchMeter = $refImpOutSettings->{"output.impIsohubOut.fetchMeter[$headIdx]"};
            $fetchLimit = $refImpOutSettings->{"output.impIsohubOut.fetchLimit[$headIdx]"};
            $asrMode = $refImpOutSettings->{"output.impIsohubOut.asrSetting[$headIdx].asrMode"};
            $asrMode = &asr_mode_get_string ($asrMode);
            $asrSubMode = $refImpOutSettings->{"output.impIsohubOut.asrSetting[$headIdx].asrSubMode"};
            $asrSubMode = &asr_sub_mode_get_string ($asrSubMode);
            $asrHWM = $refImpOutSettings->{"output.impIsohubOut.asrSetting[$headIdx].asrHWM"};
            $asrLWM = $refImpOutSettings->{"output.impIsohubOut.asrSetting[$headIdx].asrLWM"};
            $bIsASRPossible = $refImpOutSettings->{"output.impIsohubOut.bIsASRPossible"};
            $asrEfficiency = $refImpOutSettings->{"output.impIsohubOut.asrEfficiency"};
            $mclkPossible = 1; 
            $mclkMode = $refImpOutSettings->{"output.impIsohubOut.mclkSetting[$headIdx].mclkMode"};
            $mclkSubMode = $refImpOutSettings->{"output.impIsohubOut.mclkSetting[$headIdx].mclkSubMode"};
            $mclkDwcfWM = $refImpOutSettings->{"output.impIsohubOut.mclkSetting[$headIdx].mclkDwcfWM"};
            $mclkMidWM = $refImpOutSettings->{"output.impIsohubOut.mclkSetting[$headIdx].mclkMidWM"};
            
            %impHeadInfo = 
            (
                # IMP head in.
                head        => $headIdx,
                mode        => "$strModeName\@$strFreq$strPlatform\@$strRaster",
                scaling     => "$strVPR/$strScalerMode",
                base        => $strBaseBpp,
                ovly        => $strOvlyBpp,
                maxFlipPerf => $strMaxFlipPerf,
                baseLineBuf => $strBaseLineBuf,
                ovlyLineBuf => $strOvlyLineBuf,
                width       => $vpWidth,
                height      => $vpHeight,
                hblank      => $hblank,
                vblank      => $vblank,
                pclk        => $pclk,
                
                # IMP head out for dispIMP.
                dispIMP          => $dispIMP,
                reqdDispClk      => $reqdDispClk,
                maxDispClk       => $maxDispClk,
                reqdDispClkRatio => $reqdDispClkRatio,
                fbFetchRateKHz   => $fbFetchRateKHz,
                dmiMeter         => $dmiMeter,
                dispClkActual    => $dispClkActual,
                minFillRate      => $minFillRate,
                avgFetchRate     => $avgFetchRate,
                dmiDuration      => $dmiDuration,
                
                # IMP head out for isohubIMP.
                isohubIMP              => $isohubIMP,
                latBufBlocks           => $latBufBlocks,
                memPoolBlocks          => $memPoolBlocks,
                baseFullyLinesBuffered => $baseFullyLinesBuffered,
                baseLimit              => $baseLimit,
                ovlyFullyLinesBuffered => $ovlyFullyLinesBuffered,
                ovlyLimit              => $ovlyLimit,
                fetchMeter             => $fetchMeter,
                fetchLimit             => $fetchLimit,
                bIsASRPossible         => $bIsASRPossible,
                asrEfficiency          => $asrEfficiency,
                asrMode                => $asrMode,
                asrSubMode             => $asrSubMode,
                asrHWM                 => $asrHWM,
                asrLWM                 => $asrLWM,
                mclkPossible           => &make_defined ($mclkPossible),
                mclkMode               => &make_defined ($mclkMode),
                mclkSubMode            => &make_defined ($mclkSubMode),
                mclkDwcfWM             => &make_defined ($mclkDwcfWM),
                mclkMidWM              => &make_defined ($mclkMidWM)
            );
            push @impHeadsInfo, \%impHeadInfo;
        }
    }
    $impInfo{impHeadsInfo} = \@impHeadsInfo;
    
    # Get the DRAM type.
    $strDramType = shift @strTokenList;
    $impInfo{dram} = $strDramType;
    
    # Done.
    return (\%impInfo);
}

# Function: load_imp_output_file
# Desc:     this function load the IMP output setting file into a hash table.
sub load_imp_output_file ()
{
    my $strFullPath = shift @_;
    my $strLine;
    my @strTokenList;
    my ($strKey, $strValue);
    my %impOutSettings;
    
    open IMP_OUT_FILE, "<$strFullPath";
    foreach $strLine (<IMP_OUT_FILE>)
    {
        @strTokenList = ();
        chomp $strLine;
        if ($strLine =~ m/=/)
        {
            @strTokenList = split ("=", $strLine);
        }
        else
        {
            @strTokenList = split (" ", $strLine);
        }
        if (scalar @strTokenList == 2)
        {
            $strKey = $strTokenList[0];
            $strKey =~ s/\s+//;
            $strValue = $strTokenList[1];
            $strValue =~ s/\s+//;
            $impOutSettings{$strKey} = $strValue;
        }
    }
    return \%impOutSettings;
}

# Function: make_defined
# Desc:     this function returns 'undef' for the undefined variable.
sub make_defined ()
{
    my $var = shift @_;
    
    if (defined ($var))
    {
        return ($var);
    }
    else
    {
        return ("undef");
    }        
}

# Function: asr_mode_get_string
# Desc:     this function return the string form of the ASR mode. 
sub asr_mode_get_string ()
{
    my $asrMode = shift @_;
    
    if ($asrMode eq "0")
    {
        return ("DISABLE");
    }
    elsif ($asrMode eq "1")
    {
        return ("IGNORE");
    }
    elsif ($asrMode eq "2")
    {
        return ("ENABLE");
    }
    else
    {
        return ($asrMode);
    }
}

# Function: asr_sub_mode_get_string
# Desc:     this function return the string form of the ASR sub-mode. 
sub asr_sub_mode_get_string ()
{
    my $asrSubMode = shift @_;
    
    if ($asrSubMode eq "0")
    {
        return ("WATERMARK");
    }
    elsif ($asrSubMode eq "1")
    {
        return ("VBLANK");
    }
    elsif ($asrSubMode eq "2")
    {
        return ("WATERMARK_AND_VBLANK");
    }
    else
    {
        return ($asrSubMode);
    }
}
#-----------------------------------------------------------------------------------------------------------
# The end.
#-----------------------------------------------------------------------------------------------------------
