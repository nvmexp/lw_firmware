#!/home/utils/perl-5.8.8/bin/perl
##
## File:   gen_imp_configs.pl
## Desc:   This script generates IMP input config files for the verification IMP
##         implementation of gf11x chips.
##         It is based on Fung's gf10x version of gen_imp_configs.pl and updated
##         for the following extensions:
##             1) More IMP input configs for better coverage
##             2) Support both display IMP and fb IMP
## Usage:  .gen_imp_configs.pl
## Author: Fung Xie, Henry Xu
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
    GF10X_MAX_HEADS => 2
};

use constant
{
    GF11X_MAX_DACS  => 4,
    GF11X_MAX_SORS  => 8,
    GF11X_MAX_PIORS => 4
};

#-----------------------------------------------------------------------------------------------------------
# Global variables.
#-----------------------------------------------------------------------------------------------------------
# The log file.
my $g_fLog;
my $g_fLogFull;

# Default location of legacy IMP input config files.
my $g_defLegacyImpConfigDir = "../gf10x";

# Table of all gf11x chips.
my @g_chips = ("gf119");

# Table of all POR display modes.
my %g_dispModeTable;
my @g_dispModeNameTable;

# Table of interesting display modes for multi-head cases.
my %g_twoHeadDispModes = 
(
    # For head 0.
    head0 =>
        \@g_dispModeNameTable,
    
    # For head 1.
    head1 =>
        ['10x7@60N',   '10x7@85DT',  '12x10@60N', '12x10@85DT',
         '16x12@60N',  '16x12@85DT', '19x12@60N', '19x12@60DT',
         '20x15@75DT', '25x14@60DT', '25x16@60DT'],
);

my %g_threeHeadDispModes =
(
    # For head 0.
    head0 =>
        ['10x7@60N',  '12x10@85DT', '16x12@60N',  '19x12@60DT',
         '20x15@75DT', '25x14@60DT', '25x16@60DT'],
     
    # For head 1.
    head1 =>
        ['10x7@85DT',  '12x10@60N', '16x12@85DT', '19x12@60N',
         '20x15@75DT', '25x14@60DT', '25x16@60DT'],
     
    # For head 2.
    head2 =>
        ['10x7@85DT',  '19x12@60DT', '25x16@60DT']
);

my %g_fourHeadDispModes =
(
    # For head 0.
    head0 =>
        ['10x7@60N',  '12x10@85DT', '19x12@60DT',
         '20x15@75DT', '25x16@60DT'],
     
    # For head 1.
    head1 =>
        ['10x7@85DT',  '12x10@60N', '19x12@60N',
         '20x15@75DT', '25x16@60DT'],
     
    # For head 2.
    head2 =>
        ['10x7@85DT',  '16x12@85DT', '19x12@60N',
         '25x14@60DT'],
         
    # For head 3.
    head3 =>
        ['10x7@85DT',  '12x10@60N', '19x12@60N',
         '25x16@60DT']
);

# Table of all display configs.
my @g_rasterStructures = (LW_RASTER_STRUCTURE_PROGRESSIVE, LW_RASTER_STRUCTURE_INTERLACED);
my @g_bScalerOn444Modes = (TRUE, FALSE);
my @g_basePixelDepth = (4, 8);
my @g_ovlyPixelDepth = (0, 4, 8);
my @g_viewPortRelation = (EVR_NO_SCALING, EVR_IN_LT_MIN, EVR_IN_INSIDE_MIN_MAX, EVR_IN_GT_MAX);
my @g_dispConfigDB;

my $g_defDispConfig =
{
    structure        => LW_RASTER_STRUCTURE_PROGRESSIVE,
    bScalerOn444Mode => FALSE,
    basePixelDepth   => 4,
    ovlyPixelDepth   => 4,
    viewPortRelation => EVR_NO_SCALING
};
            
# DRAM types & configs.
my %g_dramConfigs =
(
    gddr5 =>
    {
        tRC                       => 42,
        tRAS                      => 30,
        tRP                       => 15,
        tRCDRD                    => 15,
        tWCK2MRS                  => 4,
        tWCK2TR                   => 4,
        tMRD                      => 4,
        EXT_BIG_TIMER             => 1,
        STEP_LN                   => 32,
        asrClkConstD4L1           => 353,
        asrClkConstC1L2           => 13,
        asrClkConstC1L1           => 414,
        asrClkConstD1             => 200,
        numBanks                  => 3,
        bytesPerClock             => 4,
        bytesPerClockFromBusWidth => 8,
        bytesPerActivate          => 64,
        tXSR                      => 5000,
        tXSNR                     => 100,
        dllOn                     => 1

    },
    
    sddr3 =>
    {
        tRC                       => 46,
        tRAS                      => 33,
        tRP                       => 13,
        tRCDRD                    => 13,
        tWCK2MRS                  => 4,
        tWCK2TR                   => 4,
        tMRD                      => 4,
        EXT_BIG_TIMER             => 1,
        STEP_LN                   => 32,
        asrClkConstD4L1           => 353,
        asrClkConstC1L2           => 13,
        asrClkConstC1L1           => 414,
        asrClkConstD1             => 200,
        numBanks                  => 3,
        bytesPerClock             => 4,
        bytesPerClockFromBusWidth => 8,
        bytesPerActivate          => 64,
        tXSR                      => 5000,
        tXSNR                     => 120,
        dllOn                     => 0
    }
);

# FBP configs.
my %g_fbpConfigs =
(
    gf119 =>
    {
        # 2 for th non-floorswept chip, and 1 for floorswept chip.
        ramCountPerFbp    => 2,
        ramCountPerFbp_fs => 1,
        
        L2Slices       => 1,
        enabledFbps    => 1,
        
        # Leave this as enabled Fbps, no uneven BW distribution for 11x XBAR
        totalFbps => 1, 
        
         # Remains the same (FOS and CDOS are not POR)
        bytesPerClock            => 16,
        ltcBytesPerClockWithDcmp => 16,
        ltcBytesPerClockWithFos  => 4,
        
        # See bug 625238
        awpPoolEntries    => 136,
        bytesPerAwpBlock  => 64,
        rrrbPoolEntries   => 152,
        bytesPerRrrbBlock => 64
     },
     
    gf117 =>
    {
        # 2 or 4, depending on floorssweeping
        ramCountPerFbp    => 4,
        ramCountPerFbp_fs => 2,
        
        L2Slices => 2,
        
        # 1 or 2, depending on floorssweeping
        enabledFbps => 2, 
        
        # Leave this as enabled Fbps, no uneven BW distribution for 11x XBAR
        totalFbps => 2,
        
        # Remains the same (FOS and CDOS are not POR)
        bytesPerClock            => 16, 
        ltcBytesPerClockWithDcmp => 16,
        ltcBytesPerClockWithFos  => 4,
        
        # See bug 625238
        awpPoolEntries    => 34,
        bytesPerAwpBlock  => 64,
        rrrbPoolEntries   => 512,
        bytesPerRrrbBlock => 64
    }
);

# XBAR configs.
my %g_xbarConfigs =
(
    gf119 =>
    {
        bytesPerClock       => 24,
        totalSlices         => 1,
        maxFbpsPerXbarSlice => 1
    },
    
    gf117 =>
    {
        bytesPerClock       => 24,
        totalSlices         => 2,
        maxFbpsPerXbarSlice => 1
    }
);

# Iso (display & isohub) configs.
my %g_isoConfigs =
(
    gf119 =>
    {
        num_heads           => 2,
        mempool             => 5312
    },
    
    gf117 =>
    {
        num_heads           => 4,
        mempool             => 5312
    }
);

# latency configs. Data come bug 625238.
my %g_latencyConfigs =
(
    P0 =>
    {
        gddr5 =>
        {
            constant => 451,
            ramClks  => 509,
            l2Clks   => 111,
            xbarClks => 49,
            hubClks  => 60
        },
        
        sddr3 =>
        {
            constant => 510,
            ramClks  => 494,
            l2Clks   => 111,
            xbarClks => 49,
            hubClks  => 60
        }
    },
    
    P8 =>
    {
        gddr5 =>
        {
            constant => 451,
            ramClks  => 413,
            l2Clks   => 111,
            xbarClks => 49,
            hubClks  => 60
        },
        
        sddr3 =>
        {
            constant => 510,
            ramClks  => 398,
            l2Clks   => 111,
            xbarClks => 49,
            hubClks  => 60
        }
    }
);
$g_latencyConfigs{P12} = $g_latencyConfigs{P8}; # P8 == P12 in terms of latencies.

# Clock configs.
my %g_clkConfigs =
(
    P0 =>
    {
        gddr5 =>
        {
            display   => 405,
            ram       => 2000,
            l2        => 750,        
            xbar      => 750,
            hub       => 750,
            
            ram_gf119 => 1600,
            ram_gf117 => 2500
        },
        
        sddr3 =>
        {
            display => 405,
            ram     => 900,        
            l2      => 750,        
            xbar    => 750,
            hub     => 750
        }
    },
    
    P8 =>
    {
        gddr5 =>
        {
            display => 389,
            ram     => 324,        
            l2      => 324,        
            xbar    => 324,
            hub     => 389
        },
        
        sddr3 =>
        {
            display => 389,
            ram     => 324,        
            l2      => 324,        
            xbar    => 324,
            hub     => 389
        }
    },
    
    P12 =>
    {
        gddr5 =>
        {
            display => 270,
            ram     => 135,        
            l2      => 135,        
            xbar    => 135,
            hub     => 270
        },
        
        sddr3 =>
        {
           display => 270,
           ram     => 135,        
           l2      => 135,        
           xbar    => 135,
           hub     => 270        
        }
    }
);

#-----------------------------------------------------------------------------------------------------------
# Quick way to define custom IMP input configs.
#-----------------------------------------------------------------------------------------------------------
my @g_archImpConfigDescs =
(
    # A custom IMP config sample.
    # Use specific chip name "gf117" or "gf119" for chip-specific IMP configs.
    # Use the general chip name "gf11x" for common IMP configs.
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '19x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            
            {
                mode => '16x12@85', structure => INTERLACED, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    #
    # Group 1: One head internal panels with ASR enabled.
    #
    # 19x12@60N
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => TRUE,
        disp_mode =>
        [
            {
                mode => '19x12@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # 16x12@60N
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => TRUE,
        disp_mode =>
        [
            {
                mode => '16x12@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 8, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # 12x10@60N
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => TRUE,
        disp_mode =>
        [
            {
                mode => '12x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # 10x7@60N
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => TRUE,
        disp_mode =>
        [
            {
                mode => '10x7@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 8, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    #
    # Group 2: one head desktop panels with ASR enabled.
    #
    # 10x7@60DT
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '10x7@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # 12x10@60DT
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '12x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 8, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # 16x12@60DT
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '16x12@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # 19x12@60DT
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '19x12@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 8, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # 19x21@60DT
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '19x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # 20x15@75DT
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '20x15@75', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 8, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # 25x14@60DT
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x14@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # 25x16@60DT
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 8, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    #
    # Group 3: multi-head systems with hi-res displays of similar and slightly different resolutions
    #          attached to all heads
    #
    # two-head similar
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    
    # three-head similar
    {
        chip   => "gf117",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    
    # four-head similar
    {
        chip   => "gf117",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    
    # two-head slightly different
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x14@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
        ]
    },
    
    # three-head slightly different
    {
        chip   => "gf117",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x14@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    
    # four-head slightly different
    {
        chip   => "gf117",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x14@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x14@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    
    #
    # Group 4: multi-head systems with one or two hi-res displays, and one or two lower-res
    #          displays with substantial downscaling (e.g. 1080i output).
    #
    # two-head case.
    {
        chip   => "gf11x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '16x12@60', structure => INTERLACED, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
        ]
    },
    
    # three-head case.
    {
        chip   => "gf117",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x14@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '16x12@60', structure => INTERLACED, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
        ]
    },
    
    {
        chip   => "gf117",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '16x12@60', structure => INTERLACED, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '16x12@60', structure => INTERLACED, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
        ]
    },
    
    # four-head case.
    {
        chip   => "gf117",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '16x12@60', structure => INTERLACED, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '16x12@60', structure => INTERLACED, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
        ]
    },
    
    #
    # Group 5: some multi-head with ASR. The sanity_* folders already contain enough such cases.
    #
    
    #
    # Group 6: some configs added on demanding.
    #
    # 19x10@60DT, P12, GDDR5, Floorsweep
    {
        chip       => "gf119",
        floorsweep => TRUE,
        dram       => "gddr5",
        pstate     => "P12",
        asr        => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu       => FALSE,
        disp_mode  =>
        [
            {
                mode => '19x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # 19x10@60DT, P12, SDDR3, Floorsweep
    {
        chip       => "gf119",
        floorsweep => TRUE,
        dram       => "sddr3",
        pstate     => "P12",
        asr        => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu       => FALSE,
        disp_mode  =>
        [
            {
                mode => '19x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    }
);

my @g_asrImpConfigDescs_GF119 =
(
    #
    # Single-head ASR at P8 and P12.
    #
    
    # gf119_DT_P{8, 12}_asr_h0_12x10_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '12x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_19x12_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '19x12@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_10x7_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '10x7@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_8x6_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '8x6@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_12x10_60_32bpp_ovly_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '12x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_19x12_60_32bpp_ovly_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '19x12@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_10x7_60_32bpp_ovly_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '10x7@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_8x6_60_32bpp_ovly_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '8x6@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    #
    # Dual-head ASR at P8 and P12
    #
    
    # gf119_DT_{P8, P12}_asr_h0_12x10_60_32bpp_h1_12x10_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '12x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            
            {
                mode => '12x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_12x10_60_32bpp_h1_10x7_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '12x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            
            {
                mode => '10x7@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_12x10_60_32bpp_h1_8x6_60_32bpp_{gddr5,sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '12x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            
            {
                mode => '8x6@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_10x7_60_32bpp_h1_12x10_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '10x7@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            
            {
                mode => '12x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_10x7_60_32bpp_h1_10x7_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '10x7@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            
            {
                mode => '10x7@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_10x7_60_32bpp_h1_8x6_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '10x7@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            
            {
                mode => '8x6@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_8x6_60_32bpp_h1_12x10_60_32bpp_{sddr3, gddr5}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '8x6@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            
            {
                mode => '12x10@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_8x6_60_32bpp_h1_10x7_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '8x6@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            
            {
                mode => '10x7@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # gf119_DT_{P8, P12}_asr_h0_8x6_60_32bpp_h1_8x6_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gf119",
        dram   => ["gddr5", "sddr3"],
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '8x6@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            
            {
                mode => '8x6@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    }
);

#-----------------------------------------------------------------------------------------------------------
# Public functions.
#-----------------------------------------------------------------------------------------------------------
# Entry point of this script.
&main ();

# Function: main
# Desc:     the main function of this script.
sub main ()
{
    # We want to produce the same sequence from rand(), so we use the same seed for all runs.
    srand (1024);
    
    # Generate the display mode table.
    &GenDispModeTable ();
    
    # Generate the display config database.
    &GenDispConfigDB ();
    
    # Create the log file.
    open ($g_fLog, ">config.txt");
    open ($g_fLogFull, ">config_fullpath.txt");
    
    # Generate sanity IMP input config files.
    &GenSanityImpConfigs ();
    
    # Generate custom IMP input config files.
    &GenLwstomImpConfigs (\@g_archImpConfigDescs, "arch");
    &GenLwstomImpConfigs (\@g_asrImpConfigDescs_GF119, "asr");
    
    # Generate ported legacy (gf10x) input config files.
    &GenLegacyImpConfigs ();
    
    # Close the log file.
    close ($g_fLog);
    close ($g_fLogFull);
}

# Function: GenDispModeTable
# Desc:     this function populates the display mode table from the file <diplay_modes.txt>
sub GenDispModeTable ()
{
    my $NUM = "[-+]?[0-9]*\.?[0-9]+";
    
    foreach (`cat display_modes.txt`)
    {
        if (/^\#.*$/)
        {
            next;
        }
        elsif (/(\d+x\d+\@\d+\w+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(y|n)\s+(\d+)\s+(\d+)\s+($NUM)\s+($NUM)\s+($NUM)\s+($NUM)\s+/)
        {
            my $dispMode =
            {
                name          => $1,
                resX          => $2,
                resY          => $3,
                freq_hz       => $4,
                pclk_freq_khz => $10 * 1000,
 
                reduced_blank => $5,
                hblank_pixels => $6,
                vblank_lines  => $7,
                pclk          => $10,
                dispclk       => $11
            };
            $g_dispModeTable{$1} = $dispMode;
            push @g_dispModeNameTable, $1;
        }
        else
        {
            die "Unknown config: \n$_";
        }
    } 
    printf "Info: totally %d display modes.\n", scalar keys %g_dispModeTable;
}

# Function: GenDispConfigDB
# Desc:     this function generates the display config table which will
#           be used to populate the LW9070_CTRL_IMP_DISP_HEAD_INPUT structure.
# Note:     we cannot generate all combinations of display config parameters
#           or we will get combination explosion. But we try best to as many
#           interesting display configs as possible.
sub GenDispConfigDB ()
{
    my ($rasterIdx, $scalerIdx, $baseIdx, $ovlyIdx, $vprIdx);
    my ($bScalerOn444Mode, $basePixelDepth, $ovlyPixelDepth, $viewPortRelation);

    for ($rasterIdx = 0; $rasterIdx < scalar @g_rasterStructures; $rasterIdx ++)
    {
        # Only vary the bScalerOn444Mode.
        for ($scalerIdx = 0; $scalerIdx < scalar @g_bScalerOn444Modes; $scalerIdx ++)
        {
            $basePixelDepth = &GetRandomInSet (\@g_basePixelDepth);
            $ovlyPixelDepth = &GetRandomInSet (\@g_ovlyPixelDepth);
            $viewPortRelation = &GetRandomInSet (\@g_viewPortRelation);
            
            my $dispConfig =
            {
                structure        => $g_rasterStructures[$rasterIdx],
                bScalerOn444Mode => $g_bScalerOn444Modes[$scalerIdx],
                basePixelDepth   => $basePixelDepth,
                ovlyPixelDepth   => $ovlyPixelDepth,
                viewPortRelation => $viewPortRelation
            };
            push @g_dispConfigDB, $dispConfig;
        }
        
        # Only vary the pixel depths.
        for ($baseIdx = 0; $baseIdx < scalar @g_basePixelDepth; $baseIdx ++)
        {
            for ($ovlyIdx = 0; $ovlyIdx < scalar @g_ovlyPixelDepth; $ovlyIdx ++)
            {
                $bScalerOn444Mode = &GetRandomInSet (\@g_bScalerOn444Modes);            
                $viewPortRelation = &GetRandomInSet (\@g_viewPortRelation);

                my $dispConfig =
                {
                    structure        => $g_rasterStructures[$rasterIdx],
                    bScalerOn444Mode => $viewPortRelation,
                    basePixelDepth   => $g_basePixelDepth[$baseIdx],
                    ovlyPixelDepth   => $g_ovlyPixelDepth[$ovlyIdx],
                    viewPortRelation => $viewPortRelation
                };
                push @g_dispConfigDB, $dispConfig;
            }
        }
        
        # Only vary the viewport relation.
        for ($vprIdx = 0; $vprIdx < scalar @g_viewPortRelation; $vprIdx ++)
        {
            $bScalerOn444Mode = &GetRandomInSet (\@g_bScalerOn444Modes);
            $basePixelDepth = &GetRandomInSet (\@g_basePixelDepth);
            $ovlyPixelDepth = &GetRandomInSet (\@g_ovlyPixelDepth);
                
            my $dispConfig =
            {
                structure        => $g_rasterStructures[$rasterIdx],
                bScalerOn444Mode => $bScalerOn444Mode,
                basePixelDepth   => $basePixelDepth,
                ovlyPixelDepth   => $ovlyPixelDepth,
                viewPortRelation => $g_viewPortRelation[$vprIdx]
            };
            push @g_dispConfigDB, $dispConfig;
        }
    }
    printf "Info: totally %d display configs.\n", scalar @g_dispConfigDB;
}

# Function: PrintDramConfig
# Desc:     this function prints the DRAM config parameters from the DRAM type.
sub PrintDramConfig ()
{
    my ($impConfigFile, $dramType) = @_;
    my $dramConfig;

    $dramConfig = $g_dramConfigs{$dramType};
    if (!defined ($dramConfig))
    {
        die "Error: invalid DRAM type: $dramType\n";
    }
    if ($dramType eq "sddr3")
    {
        # The RM ImpTest expect such a name for sddr3.
        $dramType = "sys_ddr3";
    }
    
    print $impConfigFile "# DRAM Parameters\n";
    print $impConfigFile "DRAM.ramType                                   $dramType\n";
    print $impConfigFile "DRAM.tRC                                       $dramConfig->{tRC}\n";
    print $impConfigFile "DRAM.tRAS                                      $dramConfig->{tRAS}\n";
    print $impConfigFile "DRAM.tRP                                       $dramConfig->{tRP}\n";
    print $impConfigFile "DRAM.tRCDRD                                    $dramConfig->{tRCDRD}\n";
    
    # GDDR5 ASR default values come from bug 451834.  Some of them are also
    # reflected in https://p4viewer/get/p4hw:2001/hw/doc/gpu/fermi/fermi/design/
    # IAS/FB_IAS_docs/imp/asr_callwlator.xls.
    print $impConfigFile "DRAM.tWCK2MRS                                  $dramConfig->{tWCK2MRS}\n";
    print $impConfigFile "DRAM.tWCK2TR                                   $dramConfig->{tWCK2TR}\n";
    print $impConfigFile "DRAM.tMRD                                      $dramConfig->{tMRD}\n";
    print $impConfigFile "DRAM.EXT_BIG_TIMER                             $dramConfig->{EXT_BIG_TIMER}\n";
    print $impConfigFile "DRAM.STEP_LN                                   $dramConfig->{STEP_LN}\n";
    
    # See the comments in _fbIMPASRGetExitDelay_GF100 for the callwlation of the
    # default values of the following four constants.
    print $impConfigFile "DRAM.asrClkConstD4L1                           $dramConfig->{asrClkConstD4L1}\n";
    print $impConfigFile "DRAM.asrClkConstC1L2                           $dramConfig->{asrClkConstC1L2}\n";
    print $impConfigFile "DRAM.asrClkConstC1L1                           $dramConfig->{asrClkConstC1L1}\n";
    print $impConfigFile "DRAM.asrClkConstD1                             $dramConfig->{asrClkConstD1}\n";

    print $impConfigFile "DRAM.numBanks                                  $dramConfig->{numBanks}\n";
    print $impConfigFile "DRAM.bytesPerClock                             $dramConfig->{bytesPerClock}\n";
    print $impConfigFile "DRAM.bytesPerClockFromBusWidth                 $dramConfig->{bytesPerClockFromBusWidth}\n";
    print $impConfigFile "DRAM.bytesPerActivate                          $dramConfig->{bytesPerActivate}\n";
    print $impConfigFile "\n";
}

# Function: PrintFbpConfig
# Desc:     this function prints the FBP parameters from the chip name.
sub PrintFbpConfig ()
{
    my ($impConfigFile, $chipName, $bFloorsweep) = @_;
    my $fpbConfig;
    
    $fpbConfig = $g_fbpConfigs{$chipName};
    if (!defined ($fpbConfig))
    {
        die "Error: un-supported chip: $chipName\n";
    }
    
    print $impConfigFile "# FBP Parameters\n";
    if ($bFloorsweep)
    {
        print $impConfigFile "FBP.ramCountPerFbp                             $fpbConfig->{ramCountPerFbp_fs}\n";
    }
    else
    {
        print $impConfigFile "FBP.ramCountPerFbp                             $fpbConfig->{ramCountPerFbp}\n";
    }
    print $impConfigFile "FBP.L2Slices                                   $fpbConfig->{L2Slices}\n";
    print $impConfigFile "FBP.enabledFbps                                $fpbConfig->{enabledFbps}\n";
    print $impConfigFile "FBP.totalFbps                                  $fpbConfig->{totalFbps}\n";
    print $impConfigFile "FBP.bytesPerClock                              $fpbConfig->{bytesPerClock}\n";
    print $impConfigFile "FBP.ltcBytesPerClockWithDcmp                   $fpbConfig->{ltcBytesPerClockWithDcmp}\n";
    print $impConfigFile "FBP.ltcBytesPerClockWithFos                    $fpbConfig->{ltcBytesPerClockWithFos}\n";
    print $impConfigFile "FBP.awpPoolEntries                             $fpbConfig->{awpPoolEntries}\n";
    print $impConfigFile "FBP.bytesPerAwpBlock                           $fpbConfig->{bytesPerAwpBlock}\n";
    print $impConfigFile "FBP.rrrbPoolEntries                            $fpbConfig->{rrrbPoolEntries}\n";
    print $impConfigFile "FBP.bytesPerRrrbBlock                          $fpbConfig->{bytesPerRrrbBlock}\n";
    print $impConfigFile "\n";
}

# Function: PrintXbarConfig
# Desc:     this function prints the XBAR parameters from the chip name.
sub PrintXbarConfig ()
{
    my ($impConfigFile, $chipName) = @_;
    my $xbarConfig;
    
    $xbarConfig = $g_xbarConfigs{$chipName};
    if (!defined ($xbarConfig))
    {
        die "Error: un-supported chip: $chipName\n";
    }
    
    print $impConfigFile "# XBAR Parameters\n";
    print $impConfigFile "XBAR.bytesPerClock                             $xbarConfig->{bytesPerClock}\n";
    print $impConfigFile "XBAR.totalSlices                               $xbarConfig->{totalSlices}\n";
    print $impConfigFile "XBAR.maxFbpsPerXbarSlice                       $xbarConfig->{maxFbpsPerXbarSlice}\n";
    print $impConfigFile "\n";
}

# Function: PrintIsoConfig
# Desc:     this function prints the ISO parameters.
sub PrintIsoConfig ()
{
    my ($impConfigFile, $memPool, $impIsoConfigList) = @_;
    my $head;
    my ($lineBufFlag, $maxFlipPerf);
    my $impIsoConfig;
    
    print $impConfigFile "# ISO Parameters\n";
    print $impConfigFile "ISO.linebufferAdditionalLines                  1\n";
    print $impConfigFile "ISO.linesFetchedForBlockLinear                 2\n";
    print $impConfigFile "ISO.linebufferTotalBlocks                      $memPool\n";
    print $impConfigFile "ISO.linebufferMinBlocksPerHead                 16\n";
    print $impConfigFile "ISO.bytesPerClock                              24\n";
    for ($head = 0; $head < scalar @$impIsoConfigList; $head ++)
    {
        $impIsoConfig = $$impIsoConfigList[$head];
        $lineBufFlag = sprintf ("%d_%d", $impIsoConfig->{baseLineBuf}, $impIsoConfig->{ovlyLineBuf});
        
        print $impConfigFile "ISO.lineBufferingIsAllowed[$head]                  $lineBufFlag\n";
    }
    print $impConfigFile "\n";
}

# Function: PrintClockConfig
# Desc:     this function prints the clock parameters.
sub PrintClockConfig ()
{
    my ($impConfigFile, $pstate, $dramType, $chip) = @_;
    my $clkConfig;
    my $ram;
    
    $clkConfig = $g_clkConfigs{$pstate}->{$dramType};
    if (!defined ($clkConfig))
    {
        die "Error: invalid power state: $pstate, DRAM type: $dramType\n";
    }
    
    $ram = $clkConfig->{"ram_${chip}"};
    if (!defined ($ram))
    {
        $ram = $clkConfig->{ram};
    }
    
    print  $impConfigFile "# CLOCKS Parameters\n";
    printf $impConfigFile "CLOCKS.display                                 %d\n", $clkConfig->{display} * 1000000;
    printf $impConfigFile "CLOCKS.ram                                     %d\n", $ram * 1000000;
    printf $impConfigFile "CLOCKS.l2                                      %d\n", $clkConfig->{l2} * 1000000;
    printf $impConfigFile "CLOCKS.xbar                                    %d\n", $clkConfig->{xbar} * 1000000;
    printf $impConfigFile "CLOCKS.hub                                     %d\n", $clkConfig->{hub} * 1000000;
    printf $impConfigFile "\n";
}

# Function: PrintLatencyConfig
# Desc:     this function prints the round-trip and return latency parameters.
sub PrintLatencyConfig ()
{
    my ($impConfigFile, $pstate, $dramType) = @_;
    my $latencyConfig;
    
    $latencyConfig = $g_latencyConfigs{$pstate}->{$dramType};
    if (!defined ($latencyConfig))
    {
        die "Error: un-supported pstate $pstate & dramType $dramType\n";
    }
    
    print $impConfigFile "# roundtripLatency Parameters\n";
    print $impConfigFile "roundtripLatency.constant                      $latencyConfig->{constant}\n";
    print $impConfigFile "roundtripLatency.ramClks                       $latencyConfig->{ramClks}\n";
    print $impConfigFile "roundtripLatency.l2Clks                        $latencyConfig->{l2Clks}\n";
    print $impConfigFile "roundtripLatency.xbarClks                      $latencyConfig->{xbarClks}\n";
    print $impConfigFile "roundtripLatency.hubClks                       $latencyConfig->{hubClks}\n";
    print $impConfigFile "\n";

    print $impConfigFile "# returnLatency Parameters\n";
    print $impConfigFile "returnLatency.constant                         $latencyConfig->{constant}\n";
    print $impConfigFile "returnLatency.ramClks                          $latencyConfig->{ramClks}\n";
    print $impConfigFile "returnLatency.l2Clks                           $latencyConfig->{l2Clks}\n";
    print $impConfigFile "returnLatency.xbarClks                         $latencyConfig->{xbarClks}\n";
    print $impConfigFile "returnLatency.hubClks                          $latencyConfig->{hubClks}\n";
    print $impConfigFile "\n";
}

# Function: PrintCapsConfig
# Desc:     this function prints capability parameters.
sub PrintCapsConfig ()
{
    my ($impConfigFile) = @_;
    
    print $impConfigFile "# CAPS Parameters\n";
    print $impConfigFile "CAPS.bEccIsEnabled                             0\n";
    print $impConfigFile "\n";
}

# Function: PrintAsrConfig
# Desc:     this function prints the ASR parameters.
sub PrintAsrConfig ()
{
    my ($impConfigFile, $enableASR, $dramType) = @_;
    my $dramConfig;
    
    $dramConfig = $g_dramConfigs{$dramType};
    if (!defined ($dramConfig))
    {
        die "Error: invalid DRAM type: $dramType\n";
    }

    print $impConfigFile "# ASR Parameters\n";
    print $impConfigFile "ASR.isAllowed                                  $enableASR\n";
    print $impConfigFile "ASR.efficiencyThreshold                        1\n";
    print $impConfigFile "ASR.dllOn                                      $dramConfig->{dllOn}\n";
    print $impConfigFile "ASR.tXSR                                       $dramConfig->{tXSR}\n";
    print $impConfigFile "ASR.tXSNR                                      $dramConfig->{tXSNR}\n";
    print $impConfigFile "ASR.powerdown                                  1\n";
    print $impConfigFile "\n";
}

# Function: GenSanityImpConfigs
# Desc:     this function adds sanity IMP input configs, whose purpose is to test
#           the display IMP throughly.
sub GenSanityImpConfigs ()
{
    my $chipName;
    
    foreach $chipName (@g_chips)
    {
        &GenSanityImpConfigs_OneHead ($chipName);
        &GenSanityImpConfigs_TwoHeads ($chipName);
        if ($g_isoConfigs{$chipName}->{num_heads} > 2)
        {
            &GenSanityImpConfigs_ThreeHeads ($chipName);
        }
        if ($g_isoConfigs{$chipName}->{num_heads} > 3)
        {
            &GenSanityImpConfigs_FourHeads ($chipName);
        }
    }
}

# Function: GenSanityImpConfigs_OneHead
# Desc:     this function generates sanity IMP input configs for the single-head case.
sub GenSanityImpConfigs_OneHead ()
{
    my ($chipName) = @_;
    my ($pState, $dramType);
    my ($dispModeName, $dispConfig);
    my ($maxFlipPerf, $baseLineBuf, $ovlyLineBuf);
    my ($asr, $mclkSwitchNeeded);
    my @dispModeNames;
    my @impConfigs;
    
    # Add IMP input configs at the P0 state with the default display config.
    foreach $dispModeName (keys %g_dispModeTable)
    {
        foreach $maxFlipPerf (0, 1)
        {
            foreach $baseLineBuf (0, 1)
            {
                foreach $ovlyLineBuf (0, 1)
                {
                    &AddImpInputConfig (
                        \@impConfigs,
                        $chipName, FALSE, FALSE, "P0", "gddr5", FALSE, FALSE,
                        [
                            [$dispModeName, $g_defDispConfig, $maxFlipPerf, $baseLineBuf, $ovlyLineBuf]
                        ]);
                }
            }
        }
    }
    
    # Add IMP input configs at the P0 state with random display configs.
    foreach $dispModeName (keys %g_dispModeTable)
    {
        $dispConfig = &GetRandomInSet (\@g_dispConfigDB);
        &AddImpInputConfig (
            \@impConfigs,
            $chipName, FALSE, FALSE, "P0", "gddr5", FALSE, FALSE,
            [
                [$dispModeName, $dispConfig, FALSE, TRUE, TRUE]
            ]);
    }
    
    # Add IMP input configs at the P0 state for a representative mode with all display configs.
    @dispModeNames = keys %g_dispModeTable;
    $dispModeName = $dispModeNames[int ((scalar @dispModeNames) / 2)];
    foreach $dispConfig (@g_dispConfigDB)
    {
        &AddImpInputConfig (
            \@impConfigs,
            $chipName, FALSE, FALSE, "P0", "gddr5", FALSE, FALSE,
            [
                [$dispModeName, $dispConfig, FALSE, TRUE, TRUE]
            ]);
    }
    
    # Add IMP input configs at the P8 state with the default config.
    foreach $dispModeName (keys %g_dispModeTable)
    {
        foreach $asr (0, 1)
        {
            foreach $mclkSwitchNeeded (0, 1)
            {
                &AddImpInputConfig (
                    \@impConfigs,
                    $chipName, FALSE, FALSE, "P8", "gddr5", $asr, $mclkSwitchNeeded,
                    [
                        [$dispModeName, $g_defDispConfig, FALSE, TRUE, TRUE]
                    ]);
            }
        }
    }
    
    # Generate these IMP input config files.
    &GenimpConfigFiles (\@impConfigs, "sanity_one_head");
}

# Function: GenSanityImpConfigs_TwoHeads
# Desc:     this function adds sanity IMP input configs for the two-head case.
sub GenSanityImpConfigs_TwoHeads()
{
    my ($chipName) = @_;
    my ($pState, $asr, $mclkSwitchNeeded);
    my ($dispModeHead0, $maxFlipPerfHead0, $baseLineBufHead0, $ovlyLineBufHead0, $dispConfigHead0);
    my ($dispModeHead1, $maxFlipPerfHead1, $baseLineBufHead1, $ovlyLineBufHead1, $dispConfigHead1);
    my @impConfigs;
    
    # Add IMP input configs at the P0 state with the default display config.
    foreach $dispModeHead0 (@{$g_twoHeadDispModes{head0}})
    {
        foreach $dispModeHead1 (@{$g_twoHeadDispModes{head1}})
        {
            &AddImpInputConfig (
                \@impConfigs,
                $chipName, FALSE, FALSE, "P0", "gddr5", FALSE, FALSE,
                [
                    [$dispModeHead0, $g_defDispConfig, FALSE, TRUE, TRUE],
                    [$dispModeHead1, $g_defDispConfig, FALSE, TRUE, TRUE]
                ]);
        }
    }
    
    # Add IMP input configs with various varying parameters.
    foreach $dispModeHead0 (@{$g_twoHeadDispModes{head0}})
    {
        foreach $dispModeHead1 (@{$g_twoHeadDispModes{head1}})
        {
            $pState = &GetRandomInSet (["P0", "P8", "P12"]);
            $asr = &GetRandomInSet ([0, 1]);
            $mclkSwitchNeeded = &GetRandomInSet ([0, 1]);
            
            $maxFlipPerfHead0 = &GetRandomInSet ([0, 1]);
            $baseLineBufHead0 = &GetRandomInSet ([0, 1]);
            $ovlyLineBufHead0 = &GetRandomInSet ([0, 1]);
            $dispConfigHead0 = &GetRandomInSet (\@g_dispConfigDB);
            
            $maxFlipPerfHead1 = &GetRandomInSet ([0, 1]);
            $baseLineBufHead1 = &GetRandomInSet ([0, 1]);
            $ovlyLineBufHead1 = &GetRandomInSet ([0, 1]);
            $dispConfigHead1 = &GetRandomInSet (\@g_dispConfigDB);
            
            &AddImpInputConfig (
                \@impConfigs,
                $chipName, FALSE, FALSE, $pState, "gddr5", $asr, $mclkSwitchNeeded,
                [
                    [$dispModeHead0, $dispConfigHead0, $maxFlipPerfHead0, $baseLineBufHead0, $ovlyLineBufHead0],
                    [$dispModeHead1, $dispConfigHead1, $maxFlipPerfHead1, $baseLineBufHead1, $ovlyLineBufHead1]
                ]);
        }
    }
    
    # Generate these IMP input config files.
    &GenimpConfigFiles (\@impConfigs, "sanity_two_heads");
}

# Function: GenSanityImpConfigs_ThreeHeads
# Desc:     this function adds sanity IMP input configs for the three-head case.
sub GenSanityImpConfigs_ThreeHeads()
{
    my ($chipName) = @_;
    my ($pState, $asr, $mclkSwitchNeeded);
    my ($dispModeHead0, $maxFlipPerfHead0, $baseLineBufHead0, $ovlyLineBufHead0, $dispConfigHead0);
    my ($dispModeHead1, $maxFlipPerfHead1, $baseLineBufHead1, $ovlyLineBufHead1, $dispConfigHead1);
    my ($dispModeHead2, $maxFlipPerfHead2, $baseLineBufHead2, $ovlyLineBufHead2, $dispConfigHead2);
    my @impConfigs;
    
    # Add IMP input configs at the P0 state with the default display config.
    foreach $dispModeHead0 (@{$g_threeHeadDispModes{head0}})
    {
        foreach $dispModeHead1 (@{$g_threeHeadDispModes{head1}})
        {
            foreach $dispModeHead2 (@{$g_threeHeadDispModes{head2}})
            {
                &AddImpInputConfig (
                    \@impConfigs,
                    $chipName, FALSE, FALSE, "P0", "gddr5", FALSE, FALSE,
                    [
                        [$dispModeHead0, $g_defDispConfig, FALSE, TRUE, TRUE],
                        [$dispModeHead1, $g_defDispConfig, FALSE, TRUE, TRUE],
                        [$dispModeHead2, $g_defDispConfig, FALSE, TRUE, TRUE]
                    ]);
            }
        }
    }
    
    # Add IMP input configs with various varying parameters.
    foreach $dispModeHead0 (@{$g_threeHeadDispModes{head0}})
    {
        foreach $dispModeHead1 (@{$g_threeHeadDispModes{head1}})
        {
            foreach $dispModeHead2 (@{$g_threeHeadDispModes{head2}})
            {
                $pState = &GetRandomInSet (["P0", "P8", "P12"]);
                $asr = &GetRandomInSet ([0, 1]);
                $mclkSwitchNeeded = &GetRandomInSet ([0, 1]);
                
                $maxFlipPerfHead0 = &GetRandomInSet ([0, 1]);
                $baseLineBufHead0 = &GetRandomInSet ([0, 1]);
                $ovlyLineBufHead0 = &GetRandomInSet ([0, 1]);
                $dispConfigHead0 = &GetRandomInSet (\@g_dispConfigDB);
                
                $maxFlipPerfHead1 = &GetRandomInSet ([0, 1]);
                $baseLineBufHead1 = &GetRandomInSet ([0, 1]);
                $ovlyLineBufHead1 = &GetRandomInSet ([0, 1]);
                $dispConfigHead1 = &GetRandomInSet (\@g_dispConfigDB);
                
                $maxFlipPerfHead2 = &GetRandomInSet ([0, 1]);
                $baseLineBufHead2 = &GetRandomInSet ([0, 1]);
                $ovlyLineBufHead2 = &GetRandomInSet ([0, 1]);
                $dispConfigHead2 = &GetRandomInSet (\@g_dispConfigDB);
                
                &AddImpInputConfig (
                    \@impConfigs,
                    $chipName, FALSE, FALSE, $pState, "gddr5", $asr, $mclkSwitchNeeded,
                    [
                        [$dispModeHead0, $dispConfigHead0, $maxFlipPerfHead0, $baseLineBufHead0, $ovlyLineBufHead0],
                        [$dispModeHead1, $dispConfigHead1, $maxFlipPerfHead1, $baseLineBufHead1, $ovlyLineBufHead1],
                        [$dispModeHead2, $dispConfigHead2, $maxFlipPerfHead2, $baseLineBufHead2, $ovlyLineBufHead2],
                    ]);
            }
        }
    }
    
    # Generate these IMP input config files.
    &GenimpConfigFiles (\@impConfigs, "sanity_three_heads");
}

# Function: GenSanityImpConfigs_FourHeads
# Desc:     this function adds sanity IMP input configs for the four-head case.
sub GenSanityImpConfigs_FourHeads()
{
    my ($chipName) = @_;
    my ($pState, $asr, $mclkSwitchNeeded);
    my ($dispModeHead0, $maxFlipPerfHead0, $baseLineBufHead0, $ovlyLineBufHead0, $dispConfigHead0);
    my ($dispModeHead1, $maxFlipPerfHead1, $baseLineBufHead1, $ovlyLineBufHead1, $dispConfigHead1);
    my ($dispModeHead2, $maxFlipPerfHead2, $baseLineBufHead2, $ovlyLineBufHead2, $dispConfigHead2);
    my ($dispModeHead3, $maxFlipPerfHead3, $baseLineBufHead3, $ovlyLineBufHead3, $dispConfigHead3);
    my @impConfigs;
    
   # Add IMP input configs with various varying parameters.
    foreach $dispModeHead0 (@{$g_fourHeadDispModes{head0}})
    {
        foreach $dispModeHead1 (@{$g_fourHeadDispModes{head1}})
        {
            foreach $dispModeHead2 (@{$g_fourHeadDispModes{head2}})
            {
                foreach $dispModeHead3 (@{$g_fourHeadDispModes{head3}})
                {
                    $pState = &GetRandomInSet (["P0", "P8", "P12"]);
                    $asr = &GetRandomInSet ([0, 1]);
                    $mclkSwitchNeeded = &GetRandomInSet ([0, 1]);
                    
                    $maxFlipPerfHead0 = &GetRandomInSet ([0, 1]);
                    $baseLineBufHead0 = &GetRandomInSet ([0, 1]);
                    $ovlyLineBufHead0 = &GetRandomInSet ([0, 1]);
                    $dispConfigHead0 = &GetRandomInSet (\@g_dispConfigDB);
                    
                    $maxFlipPerfHead1 = &GetRandomInSet ([0, 1]);
                    $baseLineBufHead1 = &GetRandomInSet ([0, 1]);
                    $ovlyLineBufHead1 = &GetRandomInSet ([0, 1]);
                    $dispConfigHead1 = &GetRandomInSet (\@g_dispConfigDB);
                    
                    $maxFlipPerfHead2 = &GetRandomInSet ([0, 1]);
                    $baseLineBufHead2 = &GetRandomInSet ([0, 1]);
                    $ovlyLineBufHead2 = &GetRandomInSet ([0, 1]);
                    $dispConfigHead2 = &GetRandomInSet (\@g_dispConfigDB);
                    
                    $maxFlipPerfHead3 = &GetRandomInSet ([0, 1]);
                    $baseLineBufHead3 = &GetRandomInSet ([0, 1]);
                    $ovlyLineBufHead3 = &GetRandomInSet ([0, 1]);
                    $dispConfigHead3 = &GetRandomInSet (\@g_dispConfigDB);
                    
                    &AddImpInputConfig (
                        \@impConfigs,
                        $chipName, FALSE, FALSE, $pState, "gddr5", $asr, $mclkSwitchNeeded,
                        [
                            [$dispModeHead0, $dispConfigHead0, $maxFlipPerfHead0, $baseLineBufHead0, $ovlyLineBufHead0],
                            [$dispModeHead1, $dispConfigHead1, $maxFlipPerfHead1, $baseLineBufHead1, $ovlyLineBufHead1],
                            [$dispModeHead2, $dispConfigHead2, $maxFlipPerfHead2, $baseLineBufHead2, $ovlyLineBufHead2],
                            [$dispModeHead3, $dispConfigHead3, $maxFlipPerfHead3, $baseLineBufHead3, $ovlyLineBufHead3],
                        ]);
                }
            }
        }
    }
    
    # Generate these IMP input config files.
    &GenimpConfigFiles (\@impConfigs, "sanity_four_heads");
}

# Function: AddImpInputConfig
# Desc:     this function generates an IMP input config and add it to a list.
sub AddImpInputConfig ()
{
    my ($refImpConfis, $chipName, $bFloorsweep, $igpu, $pState, $dramType, $asr, $mclkSwitchNeeded, $refHeadsInfo, $comment) = @_;
    my $refHeadInfo;
    my $head;
    my ($dispModeName, $dispConfig, $maxFlipPerf, $baseLineBuf, $ovlyLineBuf);
    my @impDispHeadIn;
    my @impIsoConfig;
    my %newImpConfig;

    $newImpConfig{chip} = $chipName;
    $newImpConfig{floorsweep} = $bFloorsweep;
    $newImpConfig{pstate} = $pState;
    $newImpConfig{dram} = $dramType;
    $newImpConfig{fos} = 0;
    $newImpConfig{cdos} = 0;
    $newImpConfig{igpu} = $igpu;
    $newImpConfig{asr} = $asr;
    $newImpConfig{mclkSwitchNeeded} = $mclkSwitchNeeded;
    
    for ($head = 0; $head < scalar @$refHeadsInfo; $head ++)
    {
        $refHeadInfo = $$refHeadsInfo[$head];

        $dispModeName = $$refHeadInfo[0];
        $dispConfig = $$refHeadInfo[1];
        $maxFlipPerf = $$refHeadInfo[2];
        $baseLineBuf = $$refHeadInfo[3];
        $ovlyLineBuf = $$refHeadInfo[4];
            
        $impDispHeadIn[$head] = &InitImpDispHeadIn ($dispModeName,
            $dispConfig->{structure}, $dispConfig->{bScalerOn444Mode}, 
            $dispConfig->{basePixelDepth}, $dispConfig->{ovlyPixelDepth},
            $dispConfig->{viewPortRelation});
            
        $impIsoConfig[$head] =
        {
            maxFlipPerf => $maxFlipPerf,
            baseLineBuf => $baseLineBuf,
            ovlyLineBuf => $ovlyLineBuf
        }
    }
    $newImpConfig{impDispHeadIn} = \@impDispHeadIn;
    $newImpConfig{impIsoConfig} = \@impIsoConfig;
    $newImpConfig{comment} = $comment;
    
    # Done.
    push @$refImpConfis, \%newImpConfig;
}

# Function: InitImpDispHeadIn
# Desc:     this function initialize the ImpDispHeadIn structure from display mode
#           and display config info.
sub InitImpDispHeadIn ()
{
    my ($dispModeName, $structure, $bScalerOn444Mode, $basePixelDepth,
        $ovlyPixelDepth, $viewPortRelation) = @_;
    my $dispMode;
    my ($heightField1, $heightField2);
    my %impDispHeadIn;
    my $strKeyDesc;

    # Get the display mode.
    $dispMode = $g_dispModeTable{$dispModeName};
    if (!defined ($dispMode))
    {
        die "Error: invalid display mode: $dispModeName\n";
    }
    
    # Setup head inputs.
    $impDispHeadIn{"HeadActive"} = TRUE;
    $impDispHeadIn{"PixelClock.Frequency"} = $dispMode->{pclk} * 1000; # In KHz.
    $impDispHeadIn{"PixelClock.UseAdj1000Div1001"} = FALSE;
    
    # Setup raster inputs.
    $impDispHeadIn{"RasterBlankEnd.X"} = int ($dispMode->{hblank_pixels} / 2);
    $impDispHeadIn{"RasterBlankStart.X"} = $impDispHeadIn{"RasterBlankEnd.X"} + $dispMode->{resX};
    $impDispHeadIn{"RasterSize.Width"} = $dispMode->{resX} + $dispMode->{hblank_pixels};    
    if ($structure == LW_RASTER_STRUCTURE_PROGRESSIVE)
    {
        # The progresive raster.
        $impDispHeadIn{"Control.Structure"} = "LW_RASTER_STRUCTURE_PROGRESSIVE";
        $impDispHeadIn{"RasterBlankEnd.Y"} = int ($dispMode->{vblank_lines} / 2);
        $impDispHeadIn{"RasterBlankStart.Y"} = $impDispHeadIn{"RasterBlankEnd.Y"} + $dispMode->{resY};
        $impDispHeadIn{"RasterVertBlank2.YEnd"} = 0;
        $impDispHeadIn{"RasterVertBlank2.YStart"} = 0;
        $impDispHeadIn{"RasterSize.Height"} = $dispMode->{resY} + $dispMode->{vblank_lines};        
    }
    else
    {
        # The interlaced raster.
        $impDispHeadIn{"Control.Structure"} = "LW_RASTER_STRUCTURE_INTERLACED";
        $heightField1 = int ($dispMode->{resY} / 2);
        $heightField2 = $dispMode->{resY} - $heightField1;
        $impDispHeadIn{"RasterBlankEnd.Y"} = int ($dispMode->{vblank_lines} / 2);
        $impDispHeadIn{"RasterBlankStart.Y"} = $impDispHeadIn{"RasterBlankEnd.Y"} + $heightField1;
        $impDispHeadIn{"RasterVertBlank2.YEnd"} = $impDispHeadIn{"RasterBlankStart.Y"} + $dispMode->{vblank_lines};
        $impDispHeadIn{"RasterVertBlank2.YStart"} = $impDispHeadIn{"RasterVertBlank2.YEnd"} + $heightField2;
        $impDispHeadIn{"RasterSize.Height"} = $dispMode->{resY} + $dispMode->{vblank_lines} * 2;
    }
    
    # Setup base layer inputs.
    $impDispHeadIn{"Params.CoreBytesPerPixel"} = $basePixelDepth;
    $impDispHeadIn{"Params.SuperSample"} = "LW_SUPER_SAMPLE_X1AA";
    $impDispHeadIn{"BaseUsageBounds.Usable"} = TRUE;
    $impDispHeadIn{"BaseUsageBounds.BytesPerPixel"} = $basePixelDepth;
    $impDispHeadIn{"BaseUsageBounds.SuperSample"} = "LW_SUPER_SAMPLE_X1AA";
    
    # Setup overlay layer inputs.
    if ($ovlyPixelDepth != 0)
    {
        $impDispHeadIn{"OverlayUsageBounds.Usable"} = TRUE;
        $impDispHeadIn{"OverlayUsageBounds.BytesPerPixel"} = $ovlyPixelDepth;
    }
    else
    {
        $impDispHeadIn{"OverlayUsageBounds.Usable"} = FALSE;
    }
    
    # Setup the scaling inputs.
    if ($viewPortRelation == EVR_NO_SCALING)
    {
        $impDispHeadIn{"ViewportSizeIn.Width"} = $dispMode->{resX};
        $impDispHeadIn{"ViewportSizeIn.Height"} = $dispMode->{resY};
        $impDispHeadIn{"ViewportSizeOut.Width"} = $dispMode->{resX};
        $impDispHeadIn{"ViewportSizeOut.Height"} = $dispMode->{resY};
        $impDispHeadIn{"ViewportSizeOutMin.Width"} = $dispMode->{resX};
        $impDispHeadIn{"ViewportSizeOutMin.Height"} = $dispMode->{resY};
        $impDispHeadIn{"ViewportSizeOutMax.Width"} = $dispMode->{resX};
        $impDispHeadIn{"ViewportSizeOutMax.Height"} = $dispMode->{resY};
    }
    else
    {
        if ($bScalerOn444Mode)
        {
            $impDispHeadIn{"OutputScaler.VerticalTaps"} = "LW_VERTICAL_TAPS_1";
        }
        else
        {
            $impDispHeadIn{"OutputScaler.VerticalTaps"} = "LW_VERTICAL_TAPS_3";
        }

        $impDispHeadIn{"ViewportSizeOut.Width"} = int ($dispMode->{resX} * 3 / 4);
        $impDispHeadIn{"ViewportSizeOut.Height"} = int ($dispMode->{resY} * 3 / 4);
        $impDispHeadIn{"ViewportSizeOutMin.Width"} = int ($dispMode->{resX} / 4);
        $impDispHeadIn{"ViewportSizeOutMin.Height"} = int ($dispMode->{resY} / 4);
        $impDispHeadIn{"ViewportSizeOutMax.Width"} = int ($dispMode->{resX} * 7 / 8);
        $impDispHeadIn{"ViewportSizeOutMax.Height"} = int ($dispMode->{resY} * 7 / 8);
        if ($viewPortRelation == EVR_IN_LT_MIN)
        {
            $impDispHeadIn{"ViewportSizeIn.Width"} = int ($dispMode->{resX} / 5);
            $impDispHeadIn{"ViewportSizeIn.Height"} = int ($dispMode->{resY} / 5);
        }
        elsif ($viewPortRelation == EVR_IN_INSIDE_MIN_MAX)
        {
            $impDispHeadIn{"ViewportSizeIn.Width"} = int ($dispMode->{resX} / 2);
            $impDispHeadIn{"ViewportSizeIn.Height"} = int ($dispMode->{resY} / 2);
        }
        elsif ($viewPortRelation == EVR_IN_GT_MAX)
        {
            $impDispHeadIn{"ViewportSizeIn.Width"} = $dispMode->{resX};
            $impDispHeadIn{"ViewportSizeIn.Height"} = $dispMode->{resY};
        }
        else
        {
            die "Error: invalid viewport relation: $viewPortRelation";
        }
    }
    
    # Setup the description of this head.
    $strKeyDesc = $dispModeName;
    $strKeyDesc =~ s/@/_/g;
    # $strKeyDesc =~ s/N|DT//g;
    
    $strKeyDesc .= ($structure == LW_RASTER_STRUCTURE_PROGRESSIVE) ? "_prog" : "_inter";
    if ($viewPortRelation != EVR_NO_SCALING)
    {
        $strKeyDesc .= $bScalerOn444Mode ? "_444" : "_422";
        $strKeyDesc .= "_vpr${viewPortRelation}";
    }
    $strKeyDesc .= sprintf ("_%dbpp", $basePixelDepth * 8);
    $strKeyDesc .= "_1x";
    if ($ovlyPixelDepth != 0)
    {
        $strKeyDesc .= sprintf ("_ovly_%dbpp", $ovlyPixelDepth * 8);
    }
    $impDispHeadIn{strKeyDesc} = $strKeyDesc;
    
    # Done.
    return (\%impDispHeadIn);
}

# Function: GetRandomInRange
# Desc:     this function returns an item from a range randomly.
sub GetRandomInRange ()
{
    my ($low, $high) = @_;
    my $ret;
    
    $ret = int (rand ($high - $low + 1)) + $low;
    return ($ret);
}

# Function: GetRandomInSet
# Desc:     this function returns an item from a set randomly.
sub GetRandomInSet ()
{
    my ($refSet) = @_;
    
    return (@$refSet[&GetRandomInRange (0, scalar @$refSet - 1)]);
}

# Function: GenimpConfigFiles
# Desc:     this function generates IMP input config files.
sub GenimpConfigFiles ()
{
    my ($refimpConfigs, $outDir) = @_;
    my $chipOutDir;
    my ($impConfig, $impConfigFileName);
    my ($impDispHeadInList, $impDispHeadIn);
    my ($impIsoConfigList, $impIsoConfig);
    my $head;
    my $impConfigFileContent;
    my %impConfigFiles;
    my $impConfigFile;
    my ($chip, $pstate, $priority, $expect, $memPool);
    my $headParameter;
    my %numImpConfigFiles;
    my ($dacIdx, $sorIdx, $piorIdx);
    my %OrTaken;
    my $strPWD;
    my $bFloorsweep;
    
    # Info log.
    # print $g_fLog "Generate IMP input config files under <$outDir>:\n";
    
    # Get current directory.
    $strPWD = `pwd`;
    chomp $strPWD;
    
    # Generate IMP input config files under the sub-directory.
    foreach $impConfig (@$refimpConfigs)
    {
        # Only generate IMP config files for thips listed in @g_chips.
        if (!&bIsNameInList (\@g_chips, $impConfig->{chip}))
        {
            next;
        }

        # Createh the sub-directory if not already.
        $chipOutDir = $impConfig->{chip} . "/$outDir";
        `mkdir -p $chipOutDir`;
        
        # Determine whether the chip is floopswept.
        if (defined ($impConfig->{floorsweep}) && ($impConfig->{floorsweep} == TRUE))
        {
            $bFloorsweep = TRUE;
        }
        else
        {
            $bFloorsweep = FALSE;
        }
        
        # Create the IMP input file name.
        $impConfigFileName = $impConfig->{chip};
        if ($bFloorsweep)
        {
            $impConfigFileName .= "fs";
        }
        $impConfigFileName .= $impConfig->{igpu} ? "_N" : "_DT";
        $impConfigFileName .= "_$impConfig->{pstate}";
        $impConfigFileName .= "_asr" if $impConfig->{asr};
        $impConfigFileName .= "_mclk" if $impConfig->{mclkSwitchNeeded};
        
        $impDispHeadInList = $impConfig->{impDispHeadIn};
        $impIsoConfigList = $impConfig->{impIsoConfig};
        for ($head = 0; $head < scalar @$impDispHeadInList; $head ++)
        {
            $impConfigFileName .= "_h${head}";
            
            $impDispHeadIn = $$impDispHeadInList[$head];
            $impConfigFileName .= "_$impDispHeadIn->{strKeyDesc}";
            
            $impIsoConfig = $$impIsoConfigList[$head];
            $impConfigFileName .= "_$impIsoConfig->{maxFlipPerf}_$impIsoConfig->{baseLineBuf}_$impIsoConfig->{ovlyLineBuf}";
        }
        $impConfigFileName .= "_$impConfig->{dram}";
        $impConfigFileName .= ".txt";
        
        # Create this IMP input config file if not already.
        if (!defined ($impConfigFiles{$impConfigFileName}))
        {
            # Open the file for writing.
            if (!open ($impConfigFile, ">$chipOutDir/$impConfigFileName"))
            {
                print "Warn: cannot open file <$chipOutDir/$impConfigFileName> for writing.\n";
                next;
            }
            
            # Print the file header.
            $chip = $impConfig->{chip};
            $pstate = $impConfig->{pstate};
            $priority = 0;
            $expect = "";
            
            print $impConfigFile "# NOTE: Automatically generated by gen_imp_configs.pl. DO NOT edit!\n";
            if (defined ($impConfig->{comment}))
            {
                print $impConfigFile "# $impConfig->{comment}\n";
            }
            print $impConfigFile "# \n";
            print $impConfigFile "# For chip $chip, display mode: \n";
            print $impConfigFile "# \n\n";
            print $impConfigFile "# PRIORITY:        $priority\n\n";
            print $impConfigFile "# PSTATE:          $pstate\n";
            print $impConfigFile "# EXPECTED RESULT: $expect\n\n";
            
            # Print the GPU family.
            print $impConfigFile "gpu.gpu_family                                 $chip\n\n";
            
            # Print DRAM parameters.
            &PrintDramConfig ($impConfigFile, $impConfig->{dram});
            
            # Print FBP parameters.
            &PrintFbpConfig ($impConfigFile, $chip, $bFloorsweep);
            
            # Print XBAR parameters.
            &PrintXbarConfig ($impConfigFile, $chip);
            
            # Print ISO parameters.
            if (defined ($impConfig->{mempool}))
            {
                $memPool = $impConfig->{mempool};
            }
            else
            {
                $memPool = $g_isoConfigs{$chip}->{mempool};
            }
            &PrintIsoConfig ($impConfigFile, $memPool, $impIsoConfigList);
            
            # Print CLOCK parameters.
            &PrintClockConfig ($impConfigFile, $pstate, $impConfig->{dram}, $chip);
            
            # Print LATENCY parameters.
            &PrintLatencyConfig ($impConfigFile, $pstate, $impConfig->{dram});
            
            # Print CAPs parameters.
            &PrintCapsConfig ($impConfigFile);
            
            # Print ASR parameters.
            &PrintAsrConfig ($impConfigFile, $impConfig->{asr}, $impConfig->{dram});
            
            # Print head parameters.
            for ($head = 0; $head < scalar @$impDispHeadInList; $head ++)
            {
                $impDispHeadIn = $$impDispHeadInList[$head];
                print $impConfigFile "# Head $head Parameters\n";
                foreach $headParameter (sort (keys %$impDispHeadIn))
                {
                    if ($headParameter eq "strKeyDesc")
                    {
                        next;
                    }
                    printf $impConfigFile "%-50s%s\n", "impDispHeadIn[$head].$headParameter",
                        $impDispHeadIn->{$headParameter};
                }
                print $impConfigFile "\n";
            }
            
            # The OR (combined with the protocol) has the orMaxPclkLimit and we should choose
            # the OR which will support the required pclk otherwise this mode will fail at
            # the very beginning of the dispIMP.
            #    Dac.orMaxPclkLimit  {Tv = 0, Crt = 400000}
            #    Sor.orMaxPclkLimit  {Dp = 270000, Tmds = 230000, Lvds = 230000}
            #    Pior.orMaxPclkLimit {???}
            print $impConfigFile "# OR parameters\n";
            %OrTaken = ();
            $dacIdx = 1;
            $sorIdx = 0;
            for ($head = 0; $head < scalar @$impDispHeadInList; $head ++)
            {
                $impDispHeadIn = $$impDispHeadInList[$head];
                if ($impDispHeadIn->{"PixelClock.Frequency"} < 230000)
                {
                    printf $impConfigFile "%-50s%s\n", "impSorIn[$sorIdx].owner", "LW_OR_OWNER_HEAD${head}";
                    printf $impConfigFile "%-50s%s\n", "impSorIn[$sorIdx].protocol", "LW_SOR_PROTOCOL_SINGLE_TMDS_A";
                    $OrTaken{"Sor${sorIdx}"} = TRUE;
                    $sorIdx ++;
                }
                else
                {
                    printf $impConfigFile "%-50s%s\n", "impDacIn[$dacIdx].owner", "LW_OR_OWNER_HEAD${head}";
                    printf $impConfigFile "%-50s%s\n", "impDacIn[$dacIdx].protocol", "LW_DAC_PROTOCOL_RGB_CRT";
                    $OrTaken{"Dac${dacIdx}"} = TRUE;
                    $dacIdx ++;
                }
                print $impConfigFile "\n";
            }
            
            # Print DAC parameters.
            print $impConfigFile "# DAC parameters\n";
            for ($dacIdx = 0; $dacIdx < GF11X_MAX_DACS; $dacIdx ++)
            {
                if (!defined ($OrTaken{"Dac${dacIdx}"}))
                {
                    printf $impConfigFile "%-50s%s\n", "impDacIn[$dacIdx].owner", "orOwner_None";
                    printf $impConfigFile "%-50s%s\n", "impDacIn[$dacIdx].protocol", "LW_DAC_PROTOCOL_RGB_CRT";
                    print $impConfigFile "\n";
                }
            }
            
            # Print SOR parameters.
            print $impConfigFile "# SOR parameters\n";
            for ($sorIdx = 0; $sorIdx < GF11X_MAX_SORS; $sorIdx ++)
            {
                if (!defined ($OrTaken{"Sor${sorIdx}"}))
                {
                    printf $impConfigFile "%-50s%s\n", "impSorIn[$sorIdx].owner", "orOwner_None";
                    printf $impConfigFile "%-50s%s\n", "impSorIn[$sorIdx].protocol", "LW_SOR_PROTOCOL_SINGLE_TMDS_A";
                    print $impConfigFile "\n";
                }
            }
            
            # Print PIOR parameters.
            print $impConfigFile "# PIOR parameters\n";
            for ($piorIdx = 0; $piorIdx < GF11X_MAX_PIORS; $piorIdx ++)
            {
                if (!defined ($OrTaken{"Pior${piorIdx}"}))
                {
                    printf $impConfigFile "%-50s%s\n", "impPiorIn[$piorIdx].owner", "orOwner_None";
                    printf $impConfigFile "%-50s%s\n", "impPiorIn[$piorIdx].protocol", "LW_PIOR_PROTOCOL_EXT_TMDS_ENC";
                    print $impConfigFile "\n";
                }
            }

            # Done.
            close ($impConfigFile);
            
            # Info log.
            print $g_fLog "gf11x/$chipOutDir/$impConfigFileName\n";
            print $g_fLogFull "$strPWD/$chipOutDir/$impConfigFileName\n";
            
            # A IMP input config file is generated.
            $impConfigFiles{$impConfigFileName} = TRUE;
            if (exists $numImpConfigFiles{$chip})
            {
                $numImpConfigFiles{$chip} ++;
            }
            else
            {
                $numImpConfigFiles{$chip} = 1;
            }
        }
        else
        {
            # print "Warn: try to generate a duplicate IMP input config file: $impConfigFileName\n";
        }
    }
    
    # Info log.
    foreach $chip (keys %numImpConfigFiles)
    {
        $chipOutDir = "$chip/$outDir";
        # printf $g_fLog "Generated %d IMP input config files under <$chipOutDir>\n\n", $numImpConfigFiles{$chip};
        printf "Info: generated %d IMP input config files under <$chipOutDir>\n", $numImpConfigFiles{$chip};
    }
}

# Funtion: GenLwstomImpConfigs
# Desc:    this function generates custom IMP input config files.
sub GenLwstomImpConfigs ()
{
    my ($refLwstomImpConfigs, $outDir) = @_;
    my @impConfigs;
    my $impConfigDesc;
    my $impConfig;
    my $dispConfig;
    my ($headDesc, $headDescList);
    my ($headInfo, @headsInfo);
    my ($mode, $igpu, $asr, $mclkSwitchNeeded);
    my ($maxFlipPerf, $baseLineBuf, $ovlyLineBuf);
    my ($structure, $bScalerOn444Mode, $basePixelDepth, $ovlyPixelDepth, $viewPortRelation);
    my (@chipList, $chipName);
    my (@dramList, $dramType);
    my (@pstateList, $pstate);
    my $bFloorsweep;
    
    foreach $impConfigDesc (@$refLwstomImpConfigs)
    {
        @headsInfo = ();
        $headDescList = $impConfigDesc->{disp_mode};
        $bFloorsweep = defined ($impConfigDesc->{floorsweep}) ? $impConfigDesc->{floorsweep} : FALSE;
        $igpu = defined ($impConfigDesc->{igpu}) ? $impConfigDesc->{igpu} : FALSE;
        foreach $headDesc (@$headDescList)
        {
            $structure =
                defined ($headDesc->{structure}) ? $headDesc->{structure} : PROGRESSIVE;
            $bScalerOn444Mode =
                defined ($headDesc->{bScalerOn444Mode}) ? $headDesc->{bScalerOn444Mode} : FALSE;
            $basePixelDepth =
                defined ($headDesc->{basePixelDepth}) ? $headDesc->{basePixelDepth} : 4;
            $ovlyPixelDepth =
                defined ($headDesc->{ovlyPixelDepth}) ? $headDesc->{ovlyPixelDepth} : 4;
            $viewPortRelation =
                defined ($headDesc->{viewPortRelation}) ? $headDesc->{viewPortRelation} : EVR_NO_SCALING;
            $dispConfig = {structure        => $structure,
                           bScalerOn444Mode => $bScalerOn444Mode,
                           basePixelDepth   => $basePixelDepth,
                           ovlyPixelDepth   => $ovlyPixelDepth,
                           viewPortRelation => $viewPortRelation};
                           
            $maxFlipPerf = defined ($headDesc->{maxFlipPerf}) ? $headDesc->{maxFlipPerf} : FALSE;
            $baseLineBuf = defined ($headDesc->{baseLineBuf}) ? $headDesc->{baseLineBuf} : FALSE;
            $ovlyLineBuf = defined ($headDesc->{ovlyLineBuf}) ? $headDesc->{ovlyLineBuf} : FALSE;
            $mode = $headDesc->{mode} . ($igpu ? "N" : "DT");
            $headInfo = [$mode, $dispConfig, $maxFlipPerf, $baseLineBuf, $ovlyLineBuf];
            
            push @headsInfo, $headInfo;
        }
        
        $asr = defined ($impConfigDesc->{asr}) ? $impConfigDesc->{asr} : FALSE;
        $mclkSwitchNeeded = defined ($impConfigDesc->{mclkSwitchNeeded}) ?
                            $impConfigDesc->{mclkSwitchNeeded} : FALSE;
            
        # Create list of target chips.
        if (ref ($impConfigDesc->{chip}) eq "ARRAY")
        {
            @chipList = @{$impConfigDesc->{chip}};
        }
        else
        {
            if ($impConfigDesc->{chip} eq "gf11x")
            {
                @chipList = @g_chips;
            }
            else
            {
                @chipList = ($impConfigDesc->{chip});
            }
        }
        
        # Create list of target DRAM types.
        if (ref ($impConfigDesc->{dram}) eq "ARRAY")
        {
            @dramList = @{$impConfigDesc->{dram}};
        }
        else
        {
            if ($impConfigDesc->{dram} eq "all")
            {
                @dramList = keys %g_dramConfigs;
            }
            else
            {
                @dramList = ($impConfigDesc->{dram});
            }
        }
        
        # Create list of target p-state.
        if (ref ($impConfigDesc->{pstate}) eq "ARRAY")
        {
            @pstateList = @{$impConfigDesc->{pstate}};
        }
        else
        {
            if ($impConfigDesc->{pstate} eq "all")
            {
                @pstateList = keys %g_dramConfigs;
            }
            else
            {
                @pstateList = ($impConfigDesc->{pstate});
            }
        }
        
        # Add the IMP input configs.
        foreach $chipName (@chipList)
        {
            foreach $dramType (@dramList)
            {
                foreach $pstate (@pstateList)
                {
                    &AddImpInputConfig (
                        \@impConfigs,
                        $chipName, $bFloorsweep, $igpu, $pstate, $dramType,
                        $asr, $mclkSwitchNeeded,
                        \@headsInfo);
                }
            }
        }
    }
    
    # Generate these IMP input config files.
    &GenimpConfigFiles (\@impConfigs, $outDir);
}

# Funtion: GenLegacyImpConfigs
# Desc:    this function generates legacy IMP input config files.
sub GenLegacyImpConfigs ()
{
    my @impConfigs;
    my $legacyImpConfigDir;
    my $legacyImpConfigFileName;
    
    # Determine the location of legacy IMP input config files.
    if (defined ($ARGV[0]))
    {
        $legacyImpConfigDir = $ARGV[0];
    }
    else
    {
        $legacyImpConfigDir = $g_defLegacyImpConfigDir;
    }
    
    # Get the list of legacy IMP input config files.
    foreach $legacyImpConfigFileName (<$legacyImpConfigDir/*>)
    {
        $legacyImpConfigFileName =~ s|$legacyImpConfigDir/||;
        if ($legacyImpConfigFileName =~ m/gf100|gf106/)
        {
            if ($legacyImpConfigFileName =~ m/PX/)
            {
                &ColwertLegacyImpConfigFile_PX ($legacyImpConfigDir, $legacyImpConfigFileName, "gf10x_legacy");
            }
            else
            {
                &ColwertLegacyImpConfigFile (\@impConfigs, $legacyImpConfigDir, $legacyImpConfigFileName);
            }
        }
    }

    # Generate these IMP input config files.
    &GenimpConfigFiles (\@impConfigs, "gf10x_legacy");
}

# Function: ColwertLegacyImpConfigFile
# Desc:     this function colwerts a legacy gf10x IMP input config file with the pstate to gf11x version.
sub ColwertLegacyImpConfigFile ()
{
    my ($refImpConfigs, $legacyImpConfigDir, $legacyImpConfigFileName) = @_;
    my ($token, @fileNameTokenList);
    my ($chip, $igpu, $pstate, $lb, $asr, $dram, $raster, $frameRate);
    my ($modeName, $baseBPP, $baseAA, $overlayBPP);
    my $headIdx;
    my $dispConfig;
    my ($headInfo, @headsInfo);
    my $comment;
    my $chipName;
    
    # Split the legacy gf10x IMP input config file name into tokens.
    $legacyImpConfigFileName =~ s/\.txt//;
    @fileNameTokenList = split ("_", $legacyImpConfigFileName);
    
    # Get chip, igpu and pstate.
    $chip = shift @fileNameTokenList;
    $igpu = (shift @fileNameTokenList eq "N") ? TRUE : FALSE;
    $pstate = shift @fileNameTokenList;
    
    # Get asr, line-buf and other parameters.
    $lb = TRUE;
    $asr = FALSE;
    while ($fileNameTokenList[0] ne "h0")
    {
        $token = shift @fileNameTokenList;
        if ($token eq "nolb")
        {
            $lb = FALSE;
        }
        elsif ($token eq "asr")
        {
            $asr = TRUE;
        }
        elsif ($token eq "lb")
        {
            $lb = TRUE;
        }
        else
        {
            print "Warn: un-recognized legacy gf10x IMP input config file name token: $token\n";
        }
    }
    
    # Get head parameters.
    for ($headIdx = 0; $headIdx < GF10X_MAX_HEADS; $headIdx ++)
    {
        if ($fileNameTokenList[0] eq "h${headIdx}")
        {
            # Get head.
            shift @fileNameTokenList;
            
            # Get mode name.
            $raster = shift @fileNameTokenList;
            $frameRate = shift @fileNameTokenList;
            $modeName = "$raster\@$frameRate" . ($igpu ? "N" : "DT");
            
            # Get base BPP.
            $baseBPP = shift @fileNameTokenList;
            $baseBPP =~ s/bpp//;
            $baseBPP /= 8;
            
            # Get base AA.
            $baseAA = shift @fileNameTokenList;
            
            # Get overlay BPP.
            if ($fileNameTokenList[0] eq "ovly")
            {
                shift @fileNameTokenList;
                if ($fileNameTokenList[0] =~ m/bpp/)
                {
                    $overlayBPP = shift @fileNameTokenList;
                    $overlayBPP =~ s/bpp//;
                    $overlayBPP /= 8;
                }
                else
                {
                    $overlayBPP = $baseBPP;
                }
            }
            else
            {
                $overlayBPP = 0;
            }
            
            # Create the display config.
            $dispConfig = {structure        => PROGRESSIVE,
                           bScalerOn444Mode => FALSE,
                           basePixelDepth   => $baseBPP,
                           ovlyPixelDepth   => $overlayBPP,
                           viewPortRelation => EVR_NO_SCALING};
            
            # Create the head info.
            $headInfo = [$modeName, $dispConfig, FALSE, $lb, $lb];
            push @headsInfo, $headInfo;
        }
    }
    
    # Get the DRAM type.
    $dram = shift @fileNameTokenList;
    if ($dram eq "gddr3")
    {
        # We no longer support GDDR3 on gf11x.
        $dram = "gddr5"
    }

    # Add this cofig to the list.
    $comment = "Colwerted from legacy gf10x IMP input config file: $legacyImpConfigFileName.txt";
    for $chipName (@g_chips)
    {
        &AddImpInputConfig ($refImpConfigs, $chipName, FALSE, $igpu, $pstate, $dram, $asr, FALSE, \@headsInfo, $comment);
    }
}

# Function: ColwertLegacyImpConfigFile
# Desc:     this function colwerts a legacy gf10x IMP input config file without the pstate to gf11x version.
sub ColwertLegacyImpConfigFile_PX ()
{
}

# Function: bIsNameInList
# Desc:     this function determines whether a name is in the list.
sub bIsNameInList ()
{
    my ($refNameList, $name) = @_;
    my $idx;
    
    for ($idx = 0; $idx < scalar @$refNameList; $idx++)
    {
        if ($$refNameList[$idx] eq $name)
        {
            return (TRUE);
        }
    }
    return (FALSE);
}
#-----------------------------------------------------------------------------------------------------------
# The end.
#-----------------------------------------------------------------------------------------------------------
