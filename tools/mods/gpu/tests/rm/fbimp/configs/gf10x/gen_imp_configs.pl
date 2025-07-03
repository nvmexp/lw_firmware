#!/home/utils/perl-5.6.1/bin/perl
#===============================================================================
#
#         FILE:  gen_imp_configs.pl
#
#        USAGE:  ./gen_imp_configs.pl
#
#  DESCRIPTION:  This script is used for generate IMP configs for fermi gf10x
#                by crossing mode/gpu/chip_config/fos/cdos.
#
#      OPTIONS:  ---
# REQUIREMENTS:  ---
#         BUGS:  ---
#        NOTES:  ---
#       AUTHOR:  Fung Xie (), <ftse@lwpu.com>
#      COMPANY:  lWpu
#      VERSION:  1.0
#      CREATED:  11/11/08 02:05:28 PST
#     REVISION:  ---
#===============================================================================

use strict;
use warnings;

use Data::Dumper;

use Regexp::Common;
# my $NUM = $RE{num}{real}{-keep};
my $NUM = "[-+]?[0-9]*\.?[0-9]+";

my %disp_modes_table;
foreach (`cat display_modes.txt`) {
    if (/^\#.*$/) {
        next;
    } elsif (/(\d+x\d+\@\d+\w+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(y|n)\s+(\d+)\s+(\d+)\s+($NUM)\s+($NUM)\s+($NUM)\s+($NUM)\s+/) {
        my $mode = {};
        $mode = {
            resX            => $2,
            resY            => $3,
            freq_hz         => $4,
            pclk_freq_khz   => $10 * 1000,

            reduced_blank   => $5,
            hblank_pixels   => $6,
            vblank_lines    => $7,
            pclk            => $10,
            dispclk         => $11,
        };
        $disp_modes_table{$1} = $mode;
    } else {
        die "Unknown config: \n$_";
    }
}

sub get_disp_mode($$$) {
    my ($mode, $igpu, $fos) = @_;

    my $imp_head_in = {
        impDispHeadIn  => { HeadActive     => 0 },
        impFbHeadIn    => { fbFetchRateKHz => 0 },
    };
    return $imp_head_in if (!defined($mode));

    my $mode_name = $mode->{base};
    if ($igpu) {
        $mode_name .= "N";
    } else {
        $mode_name .= "DT";
    }

    unless (exists($mode->{base_aa})) {
        $mode->{base_aa} = $fos;
    }

    unless (exists($mode->{overlay_aa})) {
        $mode->{overlay_aa} = "1x";
    }
    unless (exists($mode->{overlay_bpp})) {
        $mode->{overlay_bpp} = $mode->{base_bpp};
    }

    if (exists($disp_modes_table{$mode_name})) {
        my $disp_mode = $disp_modes_table{$mode_name};

#
# IMP needs the average fetch rate, so multiply pclk by (hActive / hTotal).
# Also multiply by 1.05 to account for cursor, LUT, stalls, etc.
#
        $imp_head_in->{"impFbHeadIn"}->{"fbFetchRateKHz"}          =
            int( $disp_mode->{'pclk_freq_khz'} * 1.05 * $disp_mode->{'resX'} /
                 ($disp_mode->{'resX'} + $disp_mode->{'hblank_pixels'}) );
        $imp_head_in->{"impDispHeadIn"}->{"HeadActive"}            = 1;
        $imp_head_in->{"impDispHeadIn"}->{"ViewportSizeIn.Width"}  = $disp_mode->{'resX'};
        $imp_head_in->{"impDispHeadIn"}->{"ViewportSizeIn.Height"} = $disp_mode->{'resY'};

        $imp_head_in->{"impDispHeadIn"}->{"Params.CoreBytePerPixel"} =
            $mode->{'base_bpp'} / 8;

        if ($mode->{'base_aa'} eq '1x') {
            $imp_head_in->{"impDispHeadIn"}->{"Params.SuperSample"} = "LW_SUPER_SAMPLE_X1AA";
        } elsif ($mode->{'base_aa'} eq '4x') {
            $imp_head_in->{"impDispHeadIn"}->{"Params.SuperSample"} = "LW_SUPER_SAMPLE_X4AA";
        }

        $imp_head_in->{"impDispHeadIn"}->{"BaseUsageBounds.Usable"}        =  1;
        $imp_head_in->{"impDispHeadIn"}->{"BaseUsageBounds.BytesPerPixel"} = $mode->{'base_bpp'} / 8;
        if ($mode->{'base_aa'} eq '1x') {
            $imp_head_in->{"impDispHeadIn"}->{"BaseUsageBounds.SuperSample"}   = "LW_SUPER_SAMPLE_X1AA";
        } elsif ($mode->{'base_aa'} eq '4x') {
            $imp_head_in->{"impDispHeadIn"}->{"BaseUsageBounds.SuperSample"}   = "LW_SUPER_SAMPLE_X4AA";
        }

        # print Dumper($mode);
        if ($mode->{'overlay'} eq 1) {
            $imp_head_in->{"impDispHeadIn"}->{"OverlayUsageBounds.Usable"}     = 1;
            $imp_head_in->{"impDispHeadIn"}->{"OverlayUsageBounds.BytesPerPixel"} = $mode->{'overlay_bpp'} / 8;
        } else {
            $imp_head_in->{"impDispHeadIn"}->{"OverlayUsageBounds.Usable"}     = 0;
            $imp_head_in->{"impDispHeadIn"}->{"OverlayUsageBounds.BytesPerPixel"} = 4;
        }
    } else {
        die "Unable to find display mode $mode_name\n";
    }

    $mode_name = $mode->{base};
    $mode_name =~ s/\@/_/;
    $imp_head_in->{string} = sprintf "%s_%sbpp_%s", $mode_name, $mode->{base_bpp}, $mode->{base_aa};

    if ($mode->{overlay}) {
        $imp_head_in->{string} .= "_ovly" ;
        if ($mode->{"overlay_bpp"} != $mode->{"base_bpp"}) {
            $mode->{string} .= sprintf "_%dbpp", $mode->{"overlay_bpp"};
        }
        # if ($disp_mode->{"overlay_mtype"} ne $disp_mode->{"base_mtype"}) {
        #     $disp_mode->{string} .= sprintf "_%s", $disp_mode->{"overlay_mtype"};
        # }
    }

    return $imp_head_in;
}

#   Modeled by IMP in Tesla Fullchip "Perf" Test    Standalone Apps (e.g. Fermi_DRAMs.xls)  Unit Tests
#   DRAM types  Y   P0  X
#   Display modes   Y   P0  X
#   Overlay alignment vs. base  N       X
#   FOS (AA amount) Y   P1  X
#   CDOS    N   X?  ?
#   compaction  ?   ?   ?
#   Post-XBAR request merging   N/A X (incidental coverage)
#   Hub reorder buffer size N/A X (incidental coverage)     X
#   AWP size    N/A X (incidental coverage)     X
#   Linebuffer regimes  Y   P0      ?
#   Training    Y (watermark calc)  P0      ?
#

my @imp_configs = ();

# P    Chip  FBPs  RamType      P-State  LB   Display Res/Config                                                       ASR RRRB   Other Traffic
# *0   gf100  4    gddr5/gddr3    P0     ?    dual 2048x1536 at 85hz with 4xAA and 2x 32bpp blocklinear overlay        no
# *0   gf100  4    gddr5/gddr3    P0     ?    dual 2560x1600 at 60hz with 4xAA and 2x 32bpp blocklinear overlay        no
# *1   gf100  4    gddr5/gddr3    P0     0/1  dual 2048x1536 at 85hz with 1xAA 64bpp and 2x 32bpp blocklinear overlay  no
# *1   gf100  4    gddr5/gddr3    P0     0/1  dual 2560x1600 at 60hz with 1xAA 64bpp and 2x 32bpp blocklinear overlay  no
# *1   gf100  4    gddr5/gddr3    P0     ?    dual 1920x1200 at 60hz with 1xAA and 2x 32bpp blocklinear overlay        no
# *1   gf100  4    gddr5/gddr3    P0     0/1  dual 1600x1200 at 85hz with 1xAA and 2x 32bpp blocklinear overlay        no
# *0   gf100  4    gddr5/gddr3    P12    ?    dual 1920x1200 at 60hz with 1xAA and 2x 32bpp blocklinear overlay        no         no
# *0   gf100  4    gddr5/gddr3    P12    0/1  dual 1600x1200 at 85hz with 1xAA and 2x 32bpp blocklinear overlay        no         no
my @imp_configs_without_dram =
  (
   ###########################################################
   # New POR modes
   ###########################################################
   {
    chip        => "gf100",
    chip_config => "gf100",
    pstates     => ['0'],
    dram        => "gddr5",
    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "25x16\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "25x16\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['0'],       # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "20x15\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "20x15\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   {
    chip        => "gf100",
    pstates     => ['0'],       # default pstate
    chip_config => "gf100",

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "25x16\@60", base_aa => "1x", base_bpp => 64, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "25x16\@60", base_aa => "1x", base_bpp => 64, overlay => 1, overlay_bpp => 32, },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   # gf100 4       gddr5   P12     dual 1920x1200 at 60hz with 1xAA and 2x 32bpp blocklinear overlay       no
   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['12'],      # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   # gf100 4       gddr5   P12     dual 1600x1200 at 85hz with 1xAA and 2x 32bpp blocklinear overlay       no
   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['12'],      # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "16x12\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "16x12\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   # Four more tests added by Mike
   # gf100-446  dual 25x16 60Hz DT-Rb 1xAA, bl 32bpp  bl 32bpp  P0  2000 750 750 750 92  0 0 92  0 0  PASS
   # gf100-446  dual 20x15 85Hz DT    1xAA, bl 32bpp  bl 32bpp  P0  2000 750 750 750 102 0 0 102 0 0  PASS
   # gf100-446  dual 25x16 60Hz DT-Rb 1xAA, bl 32bpp  none      P12 162  162 162 270 160 0 0 160 0 0  PASS
   # gf100-446  dual 19x12 60Hz DT-Rb 1xAA, bl 32bpp  bl 32bpp  P12 162  162 162 270 178 0 0 178 0 0  PASS
   {
    chip        => "gf100",
    chip_config => "gf100",
    pstates     => ['0'],
    dram        => "gddr5",
    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "25x16\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "25x16\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => 0,
    mempool     => 184,

    priority    => 0,
   },
   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['0'],

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "20x15\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "20x15\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => 0,
    mempool     => 204,

    priority    => 0,
   },
   {
    chip        => "gf100",
    chip_config => "gf100",
    pstates     => ['12'],
    dram        => "gddr5",
    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "25x16\@60", base_aa => "1x", base_bpp => 32, overlay => 0 },
                    head1 => { base => "25x16\@60", base_aa => "1x", base_bpp => 32, overlay => 0 },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => 0,
    mempool     => 320,

    priority    => 0,
   },
   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['12'], 

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => 0,
    mempool     => 356,

    priority    => 0,
   },



   #################################################################################
   # Old POR tests
   # gf100 4       gddr5   P0      dual 2048x1536 at 85hz with 4xAA and 2x 32bpp blocklinear overlay       no
   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['0'],       # default pstate

    fos         => undef,
    linebuffer  => undef,

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "20x15\@85", base_aa => "4x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "20x15\@85", base_aa => "4x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },

    priority    => 1,
   },

   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['0'],       # default pstate

    fos         => undef,
    linebuffer  => undef,

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "20x15\@85", base_aa => "1x", base_bpp => 64, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "20x15\@85", base_aa => "1x", base_bpp => 64, overlay => 1, overlay_bpp => 32, },
                   },

    priority    => 1,
   },

   # gf100 4       gddr5   P0      dual 2560x1600 at 60hz with 4xAA and 2x 32bpp blocklinear overlay       no
   {
    chip        => "gf100",
    pstates     => ['0'],       # default pstate
    chip_config => "gf100",
    dram        => "gddr5",
    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "25x16\@60", base_aa => "4x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "25x16\@60", base_aa => "4x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 1,
   },

   # 25x16@60DT 2xB 2xO 1xAA       hybrid  P0      15674
   # 20x15@85DT 2xB 2xO 1xAA       hybrid  P0      15674
   {
    chip        => "gf100",
    pstates     => ['0'],       # default pstate
    chip_config => "gf100",

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "20x15\@85", base_aa => "1x", base_bpp => 64, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "20x15\@85", base_aa => "1x", base_bpp => 64, overlay => 1, overlay_bpp => 32, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 1,
   },

   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['0'],       # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 1,
   },

   # gf100 4       gddr5   P12     dual 1600x1200 at 85hz with 1xAA and 2x 32bpp blocklinear overlay       no
   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['12'],      # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "16x12\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "16x12\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['12'],      # default pstate

    disp_mode   => {
                    igpu  => 0,

                    head0 => { base => "16x12\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "16x12\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 1,
   },

   # P    Chip    FBPs  Ram Type     P-State LB   Display Res/Config                                                  ASR RRRB   Other Traffic   
   # *0   gf106   1     gddr5/gddr3  P0      ?    dual 2048x1536 at 85hz with 4xAA and 2x 32bpp blocklinear overlay   no              
   # *0   gf106   1     gddr5/gddr3  P0      ?    dual 2560x1600 at 60hz with 4xAA and 2x 32bpp blocklinear overlay   no              
   # *1   gf106   1     gddr5/gddr3  P0      ?    dual 1920x1200 at 60hz with 1xAA and 2x 32bpp blocklinear overlay   no              
   # *1   gf106   1     gddr5/gddr3  P0      ?    dual 1600x1200 at 85hz with 1xAA and 2x 32bpp blocklinear overlay   no              
   # *0   gf106   1     gddr5/gddr3  P12     ?    dual 1920x1200 at 60hz with 1xAA and 1x 32bpp blocklinear overlay   no
   # *0   gf106   1     gddr5/gddr3  P12     ?    dual 1920x1200 at 60hz with 1xAA and 2x 32bpp blocklinear overlay   no
   # *0   gf106   1     gddr5/gddr3  P12     ?    dual 1600x1200 at 85hz with 1xAA and 2x 32bpp blocklinear overlay   no

   # gf106 1       gddr5   P0      dual 2048x1536 at 85hz with 4xAA and 2x 32bpp blocklinear overlay       no
   {
    chip        => "gf106",
    chip_config => "gf106",
    dram        => "gddr5",
    pstates     => ['0'],       # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "20x15\@85", base_aa => "4x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "20x15\@85", base_aa => "4x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   # gf106 1       gddr5   P0      dual 2560x1600 at 60hz with 4xAA and 2x 32bpp blocklinear overlay       no
   {
    chip        => "gf106",
    chip_config => "gf106",
    dram        => "gddr5",
    pstates     => ['0'],       # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "25x16\@60", base_aa => "4x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "25x16\@60", base_aa => "4x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   # gf106 1       gddr5   P12     dual 1920x1200 at 60hz with 1xAA and 2x 32bpp blocklinear overlay       no
   {
    chip        => "gf106",
    chip_config => "gf106",
    dram        => "gddr5",
    pstates     => ['0'],       # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 1,
   },

   # gf106 1       gddr5   P12     dual 1600x1200 at 85hz with 1xAA and 2x 32bpp blocklinear overlay       no               
   {
    chip        => "gf106",
    chip_config => "gf106",
    dram        => "gddr5",
    pstates     => ['0'],       # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "16x12\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "16x12\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 1,
   },

   # gf106 1       gddr5   P12     dual 1920x1200 at 60hz with 1xAA and 1x 32bpp blocklinear overlay       no
   {
    chip        => "gf106",
    chip_config => "gf106",
    dram        => "gddr5",
    pstates     => ['12'],      # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 0, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   # gf106 1       gddr5   P12     dual 1920x1200 at 60hz with 1xAA and 1x 32bpp blocklinear overlay       no
   {
    chip        => "gf106",
    chip_config => "gf106",
    dram        => "gddr5",
    pstates     => ['12'],      # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   # gf106 1       gddr5   P12     dual 1600x1200 at 85hz with 1xAA and 2x 32bpp blocklinear overlay       no
   {
    chip        => "gf106",
    chip_config => "gf106",
    dram        => "gddr5",
    pstates     => ['12'],      # default pstate

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "16x12\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "16x12\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   ########################
   # Optional modes
   ########################
   ########################
   # FOS and cdos isn't POR of gf100
   ########################
   # 12x10@60N 2xB 4xAA    pure    P12     2553
   # {
   #  chip      => "gf100",
   #  disp_mode => {
   #                igpu  => 1,
   #                head0 => { base => "12x10\@60", base_aa => "4x", base_bpp => 32, overlay => 0, },
   #                head1 => { base => "12x10\@60", base_aa => "4x", base_bpp => 32, overlay => 0, },
   #               },
   #  pstates     => ['12'],      # default pstate
   #  chip_config => "gf100_100mhz",
   #  fos         => undef,
   #  linebuffer  => undef,

   #  priority    => 2,
   # },

   # # 12x10@85DT 4xAA pure
   # {
   #  chip      => "gf100",
   #  disp_mode => {
   #                igpu  => 0,
   #                head0 => { base => "12x10\@85", base_aa => "4x", base_bpp => 32, overlay => 0, },
   #                head1 => { base => "12x10\@85", base_aa => "4x", base_bpp => 32, overlay => 0, },
   #               },
   #  pstates     => ['12'],      # default pstate
   #  chip_config => "gf100_100mhz",
   #  fos         => undef,
   #  linebuffer  => undef,

   #  priority    => 2,
   # },

   # # 20x15@85DT B+O 4xAA   hybrid  P8      7741
   # {
   #  chip      => "gf100",
   #  chip_config => "gf100",
   #  pstates     => ['8'],       # default pstate

   #  disp_mode => {
   #                igpu  => 0,
   #                head0 => { base => "20x15\@85", base_aa => "4x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
   #                head1 => undef,
   #               },
   #  fos         => undef,
   #  linebuffer  => undef,

   #  priority    => 2,
   # },

   # OBSOLETE: Keep it here in case we want to change linebuffer allocation again.
   #   Was: Test Linebuffer allocation(latency > hlimit && latency > 1-hlimit(head1))
   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['X'],
    clk         => { expect => "pass", mclk => 500, ltcclk => 500, xbarclk => 500, hubclk => 550 },
    attach_str  => "_lb",

    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "20x15\@85", base_aa => "1x", base_bpp => 64, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "20x15\@85", base_aa => "1x", base_bpp => 64, overlay => 1, overlay_bpp => 32, },
                   },

    priority    => 0,

    fos         => undef,
    linebuffer  => undef,
   },

   # cross product of @disp_modes_with_chips x @chip_config x @fos will be added below
  );

###########################################################
# Margin tests
###########################################################
my @imp_configs_margin =
  (
   {
    chip        => "gf100",
    chip_config => "gf100",
    pstates     => ['X'],
    margin      => [
                    { expect => "pass", mclk => 2000, ltcclk => 134, xbarclk => 134, hubclk => int(356 * 1.05) },
                    { expect => "fail", mclk => 2000, ltcclk => 107, xbarclk => 134, hubclk => int(356 * 1.05) },
                    { expect => "fail", mclk => 2000, ltcclk => 134, xbarclk => 107, hubclk => int(356 * 1.05) },
                    { expect => "fail", mclk => 2000, ltcclk => 134, xbarclk => 134, hubclk => 285 },
                   ],
    dram        => "gddr5",
    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "25x16\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "25x16\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => undef,
    priority    => 0,
   },

   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['X'],       # default pstate
    margin      => [
                    { expect => "pass", mclk => 2000, ltcclk => 149, xbarclk => 149, hubclk => int(397 * 1.05) },
                    { expect => "fail", mclk => 2000, ltcclk => 119, xbarclk => 149, hubclk => int(397 * 1.05) },
                    { expect => "fail", mclk => 2000, ltcclk => 149, xbarclk => 119, hubclk => int(397 * 1.05) },
                    { expect => "fail", mclk => 2000, ltcclk => 149, xbarclk => 149, hubclk => 318 },
                   ],
    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "20x15\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "20x15\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => "gddr5",
    pstates     => ['X'],
    margin      => [
                    { expect => "pass", mclk => 162, ltcclk => 144, xbarclk => 144, hubclk => int(201 * 1.05) },
                    { expect => "fail", mclk => 162, ltcclk => 115, xbarclk => 144, hubclk => int(201 * 1.05) },
                    { expect => "fail", mclk => 162, ltcclk => 144, xbarclk => 115, hubclk => int(201 * 1.05) },
                    { expect => "fail", mclk => 162, ltcclk => 144, xbarclk => 144, hubclk => 161 },
                   ],
    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                    head1 => { base => "19x12\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32, },
                   },
    cdos        => 0,
    fos         => undef,
    linebuffer  => undef,
    priority    => 0,
   },
  );

my @imp_configs_asr =
  (
   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => 'gddr5',
    pstates     => ['12'],      # default pstate
    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "12x10\@85", base_aa => "1x", base_bpp => 32, overlay => 0 },
                    head1 => undef,
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => 'gddr5',
    pstates     => ['12'],      # default pstate
    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "12x10\@85", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32 },
                    head1 => undef,
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => 'gddr5',
    pstates     => ['12'],      # default pstate
    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "12x10\@60", base_aa => "1x", base_bpp => 32, overlay => 1, overlay_bpp => 32 },
                    head1 => undef,
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },

   {
    chip        => "gf100",
    chip_config => "gf100",
    dram        => 'gddr5',
    pstates     => ['X'],      # default pstate
    clk         => { mclk => 162, ltcclk => 162, xbarclk => 162, hubclk => int(389 * 1.05) },
    attach_str  => "_2xhubclk",
    disp_mode   => {
                    igpu  => 0,
                    head0 => { base => "12x10\@85", base_aa => "1x", base_bpp => 32, overlay => 0 },
                    head1 => undef,
                   },
    fos         => undef,
    linebuffer  => undef,

    priority    => 0,
   },


  );

my @dram_types = ("gddr5", "gddr3");
my %dram_configs =
    (
     gddr5 => {
         tRC               =>  42,
         numBanks          =>  3,
         bytesPerClock     =>  4,
     },
     gddr3 => {
         tRC               =>  40,
         numBanks          =>  3,
         bytesPerClock     =>  4,
     },
     );

my @disp_modes_with_chips =
  (
   {
    chip    => "gf100",
    igpu    => 0,
    head0   => { base => "20x15\@85", base_bpp => 64, overlay => 1, },
    head1   => { base => "20x15\@85", base_bpp => 64, overlay => 1, },
    pstates => ['0'],           # default pstate
   },

   {
    chip    => "gf100",
    igpu    => 0,
    head0   => { base => "16x12\@85", base_bpp => 32, overlay => 1, },
    head1   => { base => "16x12\@85", base_bpp => 32, overlay => 1, },
    pstates => ['0'],           # default pstate
   },
  );

my $chip_configs_with_pstate =
  {
   gf100 => {
             num_fbps_en => 4,
             maxFbpsPerXbarSlice => 2,
             gpc2clk     => 750,
             mclk        => 2000,
             m2clk       => 4000,
            },
   gf106 => {
             num_fbps_en => 1,
             maxFbpsPerXbarSlice => 1,
             gpc2clk     => 750,
             mclk        => 2000,
             m2clk       => 4000,
            },
   gf100_100mhz => {
                    num_fbps_en => 4,
                    maxFbpsPerXbarSlice => 2,
                    gpc2clk     => 100,
                    mclk        => 100,
                    m2clk       => 200,
                   },
  };

my $PStates =
  {
   P0  => {
           dispclk => 405,
           hubclk  => 750,
           xbarclk => 750,
           ltcclk  => 750,
           mclk    => 2000,
           m2clk   => 4000,
          },
   P8  => {
           dispclk => 389,
           hubclk  => 389,
           xbarclk => 324,
           ltcclk  => 324,
           mclk    => 324,
           m2clk   => 648,
          },
   P12 => {
           dispclk => 270,
           hubclk  => 270,
           ltcclk  => 162,
           xbarclk => 162,
           mclk    => 162,
           m2clk   => 324,
          },
  };

my @fos_types = ("1x", "4x");

foreach my $cfg (@imp_configs_without_dram) {
    foreach my $dram (@dram_types) {
        my $new_cfg = {};
        foreach (keys(%{ $cfg })) {
            $new_cfg->{$_} = $cfg->{$_};
        }
        $new_cfg->{'dram'} = $dram;
        push @imp_configs, $new_cfg;
    }
}

foreach my $cfg (@imp_configs_asr) {
    # $cfg->{'dram'} = 'gddr3';
    $cfg->{'asr'}  = 1;
    push @imp_configs, $cfg;
}

# Expand margin tests
foreach my $cfg (@imp_configs_margin) {
    my $c = 0;
    foreach my $clk (@{ $cfg->{margin} }) {
        if ($clk->{expect} eq "fail") {
            $clk->{expect} .= "_$c";
            $c++;
        }
        my $config = {
                      chip        => $cfg->{'chip'},
                      chip_config => $cfg->{'chip_config'},
                      clk         => $clk,
                      disp_mode   => $cfg->{'disp_mode'},
                      pstates     => $cfg->{'pstates'},
                      dram        => $cfg->{'dram'},
                      fos         => $cfg->{'fos'},
                      linebuffer  => $cfg->{'linebuffer'},

                      priority    => $cfg->{'priority'},
                     };
        push @imp_configs, $config;
    }
}

foreach my $mode (@disp_modes_with_chips) {
    foreach my $dram (@dram_types) {
        foreach my $fos (@fos_types) {
            my $config = {
                          chip        => $mode->{'chip'},
                          disp_mode   => $mode,
                          pstates     => $mode->{'pstates'},
                          chip_config => $mode->{'chip'},
                          dram        => $dram,
                          fos         => $fos,
                          linebuffer  => undef,

                          priority    => 2,
                         };
            push @imp_configs, $config;
        }
    }
}

my $generated_configs = {};
open IMP_CFG_LIST, "> configs.txt";
foreach my $cfg (@imp_configs) {
    my $chip       = $cfg->{chip};
    my $chip_cfg   = $cfg->{chip_config};
    my $pstate     = $cfg->{pstates}->[0];
    my $priority   = $cfg->{priority};
    my $cdos       = exists($cfg->{cdos})? $cfg->{cdos}: 0;
    my $asr        = exists($cfg->{asr})? $cfg->{asr}: 0;
    my $expect     = "";

    my $num_fbps_en = $chip_configs_with_pstate->{$chip_cfg}->{num_fbps_en};
    my $maxFbpsPerXbarSlice = $chip_configs_with_pstate->{$chip_cfg}->{maxFbpsPerXbarSlice};

    my ($dispclk, $mclk, $m2clk, $hubclk, $xbarclk, $ltcclk) = (0, 0, 0, 0, 0, 0);
    my $margin = "NA";
    if ($pstate eq "X") { # Margin test
        $dispclk  = 405000000;
        $hubclk   = $cfg->{clk}->{hubclk} * 1000000;
        $xbarclk  = $cfg->{clk}->{xbarclk} * 1000000;
        $ltcclk   = $cfg->{clk}->{ltcclk} * 1000000;
        $mclk     = $cfg->{clk}->{mclk} * 1000000;
        $m2clk    = int($mclk * 2);
        $margin   = $cfg->{clk}->{expect} if defined $cfg->{clk}->{expect};
        $expect   = ($margin =~ /pass/ ? "pass" : "fail") if defined $cfg->{clk}->{expect};
    }
    else {
        $dispclk  = int($PStates->{"P$pstate"}->{dispclk} * 1000000);
        $hubclk   = int($PStates->{"P$pstate"}->{hubclk} * 1000000);
        $xbarclk  = int($PStates->{"P$pstate"}->{xbarclk} * 1000000);
        $ltcclk   = int($PStates->{"P$pstate"}->{ltcclk} * 1000000);
        $mclk     = int($PStates->{"P$pstate"}->{mclk} * 1000000);
        $m2clk    = int($PStates->{"P$pstate"}->{m2clk} * 1000000);
    }

    my $mempool = defined($cfg->{"mempool"}) ? $cfg->{"mempool"} : 1156;
    my $linebuffer = defined($cfg->{"linebuffer"}) ? $cfg->{"linebuffer"} : 1;

    # print "CFG" . Dumper($cfg->{disp_mode}->{head0});
    # print "CFG" . Dumper($cfg->{disp_mode}->{head1});
    my $mode_head0 = get_disp_mode($cfg->{disp_mode}->{head0}, $cfg->{disp_mode}->{igpu}, $cfg->{fos});
    my $mode_head1 = get_disp_mode($cfg->{disp_mode}->{head1}, $cfg->{disp_mode}->{igpu}, $cfg->{fos});

    my $imp_cfg_file = "$chip" . "_" . ($cfg->{disp_mode}->{igpu}? "N": "DT") . "_P$pstate";
    $imp_cfg_file .= "_nolb" if ($linebuffer == 0);
    $imp_cfg_file .= "_asr" if $asr;
    $imp_cfg_file .= "_margin_$margin" if $margin ne "NA";
    $imp_cfg_file .= "$cfg->{attach_str}" if (exists($cfg->{attach_str}));

    $imp_cfg_file .= sprintf "_h0_%s", $mode_head0->{string} if ($mode_head0->{"impDispHeadIn"}->{"HeadActive"} eq 1);
    $imp_cfg_file .= sprintf "_h1_%s", $mode_head1->{string} if ($mode_head1->{"impDispHeadIn"}->{"HeadActive"} eq 1);

    my $dram = $cfg->{dram};
    if (!defined($dram)) {
        $dram = "gddr3";
    }

#
# latencyRamClocks come from
# https://p4viewer.lwpu.com/get/p4hw:2001/hw/doc/gpu/fermi/fermi/design/IAS/FB_IAS_docs/imp/IMP_worksheet.xls.
# Assume that various FBPA perf features affecting latency (mentioned in bug
# 619826) are enabled if mclk >= 1GHz.
#
    my $latencyRamClocks;
    if ($mclk >= 1000000000) {
        $latencyRamClocks = 509;
# If the memory is GDDR5 and mclk >= 1 GHz, assume that crc is enabled, and
# increase latency for a possible crc event.
        if ($dram eq "gddr5") {
            $latencyRamClocks += 530;
        }
    } 
    else {
        $latencyRamClocks = 413;
    }
    $imp_cfg_file .= "_${dram}";

    $imp_cfg_file .= ".txt";

    if (exists($generated_configs->{$imp_cfg_file})) {
        warn "Dup imp config file $imp_cfg_file";
        next;
    }
    $generated_configs->{$imp_cfg_file} = 1;

    print IMP_CFG_LIST "$imp_cfg_file\n";
    open IMP_CONFIG, "> $imp_cfg_file";

    print IMP_CONFIG "# NOTE: Automatically generated by gen_imp_configs.pl. DO NOT edit!\n";
    print IMP_CONFIG "# \n";
    print IMP_CONFIG "# For chip $chip, display mode: \n";
    print IMP_CONFIG "# \n\n";
    print IMP_CONFIG "# PRIORITY:        $priority\n\n";
    print IMP_CONFIG "# PSTATE:          $pstate\n";
    print IMP_CONFIG "# EXPECTED RESULT: $expect\n\n";

    print IMP_CONFIG "gpu.gpu_family                                $chip\n\n";

    print IMP_CONFIG "# DRAM Parameters \n";
    print IMP_CONFIG "DRAM.ramType                                  $dram\n";
    print IMP_CONFIG "DRAM.tRC                                      $dram_configs{$dram}->{tRC}\n";
    print IMP_CONFIG "DRAM.tRAS                                     30\n";
    print IMP_CONFIG "DRAM.tRP                                      15\n";
    print IMP_CONFIG "DRAM.tRCDRD                                   15\n";
#
# GDDR5 ASR default values come from bug 451834.  Some of them are also
# reflected in https://p4viewer/get/p4hw:2001/hw/doc/gpu/fermi/fermi/design/
# IAS/FB_IAS_docs/imp/asr_callwlator.xls.
#
    print IMP_CONFIG "DRAM.tWCK2MRS                                 4\n";
    print IMP_CONFIG "DRAM.tWCK2TR                                  4\n";
    print IMP_CONFIG "DRAM.tMRD                                     4\n";
    print IMP_CONFIG "DRAM.EXT_BIG_TIMER                            1\n";
    print IMP_CONFIG "DRAM.STEP_LN                                  32\n";
#
# See the comments in _fbIMPASRGetExitDelay_GF100 for the callwlation of the
# default values of the following four constants.
#
    print IMP_CONFIG "DRAM.asrClkConstD4L1                          353\n";
    print IMP_CONFIG "DRAM.asrClkConstC1L2                          13\n";
    print IMP_CONFIG "DRAM.asrClkConstC1L1                          414\n";
    print IMP_CONFIG "DRAM.asrClkConstD1                            200\n";

    print IMP_CONFIG "DRAM.numBanks                                 $dram_configs{$dram}->{numBanks}\n";
    print IMP_CONFIG "DRAM.bytesPerClock                            $dram_configs{$dram}->{bytesPerClock}\n";
    print IMP_CONFIG "DRAM.bytesPerClockFromBusWidth                8\n";
    print IMP_CONFIG "DRAM.bytesPerActivate                         64\n";
    print IMP_CONFIG "\n";

    print IMP_CONFIG "# FBP Parameters\n";
    print IMP_CONFIG "FBP.ramCountPerLtc                            2\n";
    print IMP_CONFIG "FBP.L2Slices                                  2\n";
    print IMP_CONFIG "FBP.enabledLtcs                               $num_fbps_en\n";
    print IMP_CONFIG "FBP.bytesPerClock                             16\n";
    print IMP_CONFIG "FBP.ltcBytesPerClockWithDcmp                  16\n";
    print IMP_CONFIG "FBP.ltcBytesPerClockWithFos                   4\n";
    print IMP_CONFIG "FBP.awpPoolEntries                            34\n";
    print IMP_CONFIG "FBP.bytesPerAwpBlock                          64\n";
    print IMP_CONFIG "FBP.rrrbPoolEntries                           512\n";
    print IMP_CONFIG "FBP.bytesPerRrrbBlock                         64\n";
    print IMP_CONFIG "\n";

    print IMP_CONFIG "# XBAR Parameters\n";
    print IMP_CONFIG "XBAR.bytesPerClock                            24\n";
    print IMP_CONFIG "XBAR.totalSlices                              4\n";
    print IMP_CONFIG "XBAR.maxFbpsPerXbarSlice                      $maxFbpsPerXbarSlice\n";
    print IMP_CONFIG "\n";

    print IMP_CONFIG "# ISO Parameters\n";
    print IMP_CONFIG "ISO.linebufferAdditionalLines                 $linebuffer\n";
    print IMP_CONFIG "ISO.linesFetchedForBlockLinear                2\n";
    print IMP_CONFIG "ISO.linebufferTotalBlocks                     $mempool\n";
    print IMP_CONFIG "ISO.linebufferMinBlocksPerHead                16\n";
    print IMP_CONFIG "ISO.bytesPerClock                             24\n";
    print IMP_CONFIG "\n";

    print IMP_CONFIG "# CLOCKS Parameters\n";
    print IMP_CONFIG "CLOCKS.display                                $dispclk\n";
    print IMP_CONFIG "CLOCKS.ram                                    $mclk\n";
    print IMP_CONFIG "CLOCKS.l2                                     $ltcclk\n";
    print IMP_CONFIG "CLOCKS.xbar                                   $xbarclk\n";
    print IMP_CONFIG "CLOCKS.hub                                    $hubclk\n";
    print IMP_CONFIG "\n";

    print IMP_CONFIG "# roundtripLatency Parameters\n";
    print IMP_CONFIG "roundtripLatency.constant                     451\n";
    print IMP_CONFIG "roundtripLatency.ramClks                      $latencyRamClocks\n";
    print IMP_CONFIG "roundtripLatency.l2Clks                       111\n";
    print IMP_CONFIG "roundtripLatency.xbarClks                     49\n";
    print IMP_CONFIG "roundtripLatency.hubClks                      97\n";
    print IMP_CONFIG "\n";

    print IMP_CONFIG "# returnLatency Parameters\n";
    print IMP_CONFIG "returnLatency.constant                        451\n";
    print IMP_CONFIG "returnLatency.ramClks                         $latencyRamClocks\n";
    print IMP_CONFIG "returnLatency.l2Clks                          91\n";
    print IMP_CONFIG "returnLatency.xbarClks                        26\n";
    print IMP_CONFIG "returnLatency.hubClks                         41\n";
    print IMP_CONFIG "\n";

    print IMP_CONFIG "# CAPS Parameters\n";
    # CDOS is no longer affect IMP
    # print IMP_CONFIG "CAPS.compressedSurfacesAllowed                $cdos\n";
    print IMP_CONFIG "CAPS.bEccIsEnabled                            0\n";
    print IMP_CONFIG "\n";

    print IMP_CONFIG "# ASR Parameters\n";
    print IMP_CONFIG "ASR.isAllowed                                 $asr\n";
    print IMP_CONFIG "ASR.efficiencyThreshold                       1\n";
    print IMP_CONFIG "ASR.dllOn                                     1\n";
    print IMP_CONFIG "ASR.tXSR                                      5000\n";
    print IMP_CONFIG "ASR.tXSNR                                     100\n";
    print IMP_CONFIG "ASR.powerdown                                 1\n";
    print IMP_CONFIG "\n";

    print IMP_CONFIG "# Isohub Parameters\n";
    print IMP_CONFIG "impIsohubIn.maxLwrsorEntries                  8\n";
    print IMP_CONFIG "\n";

    print IMP_CONFIG "# DispHeadIn  Parameters\n";
    foreach (sort(keys %{ $mode_head0->{"impDispHeadIn"} })) {
        printf IMP_CONFIG "%-50s%s\n", "impDispHeadIn[0].$_", "$mode_head0->{impDispHeadIn}->{$_}";
    }
    printf IMP_CONFIG "%-50s%s\n", "impFbHeadIn[0].fbFetchRateKHz", "$mode_head0->{impFbHeadIn}->{fbFetchRateKHz}" if ($mode_head0->{"impDispHeadIn"}->{"HeadActive"} eq 1);
    print IMP_CONFIG "\n";

    foreach (sort(keys %{ $mode_head1->{"impDispHeadIn"} })) {
        printf IMP_CONFIG "%-50s%s\n", "impDispHeadIn[1].$_", "$mode_head1->{impDispHeadIn}->{$_}";
    }
    printf IMP_CONFIG "%-50s%s\n", "impFbHeadIn[1].fbFetchRateKHz", "$mode_head1->{impFbHeadIn}->{fbFetchRateKHz}\n" if ($mode_head1->{"impDispHeadIn"}->{"HeadActive"} eq 1);
    print IMP_CONFIG "\n";

    # if (defined($lb)) {
    #     print IMP_CONFIG "# line buffer config\n";
    #     print IMP_CONFIG "iregs.base_lb_nblocks0 $lb->{base}\n";
    #     print IMP_CONFIG "iregs.ovly_lb_nblocks0 $lb->{overlay}\n";
    #     print IMP_CONFIG "iregs.base_lb_nblocks1 $lb->{base}\n";
    #     print IMP_CONFIG "iregs.ovly_lb_nblocks1 $lb->{overlay}\n";
    # }

    close IMP_CONFIG;
}

close IMP_CFG_LIST;


my $prioritized_imp_configs = (
                               );

exit 0;
