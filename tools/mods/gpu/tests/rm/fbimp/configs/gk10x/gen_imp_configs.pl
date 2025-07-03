#!/home/utils/perl-5.8.8/bin/perl
##
## File:   gen_imp_configs.pl
## Desc:   This script generates IMP input config files for the verification IMP
##         implementation of gk10x chips.
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
use Getopt::Long;
use vars qw/ %opt /;  #command options

#----------------------------------------------------------------------------------------------
# Constant definitions.
#----------------------------------------------------------------------------------------------
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
    GK10X_MAX_HEADS => 4
};

use constant
{
    GK10X_MAX_DACS  => 4,
    GK10X_MAX_SORS  => 8,
    GK10X_MAX_PIORS => 4
};

#----------------------------------------------------------------------------------------------
# Global variables.
#----------------------------------------------------------------------------------------------
# The log file.
my $g_fLog;
my $g_fLogFull;

my $opt_help;
my $opt_rmverif;
my $opt_archverif;
my $opt_genArchImpTestHdr;
my $opt_project = "all";

# Default location of legacy IMP input config files.
my $g_defLegacyImpConfigDir = "../gf10x";

# Table of all chips supported by this script.
my %g_supChipInfo =
(
    gk104 =>
    {
        family          => "gk104|gk10x|kepler",
        genSanityConfig => TRUE
    },
    
    gk110 =>
    {
        family          => "gk110|gk11x|kepler",
        genSanityConfig => FALSE
    }
);

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
        ['10x7@60N',    '10x7@85DT',  '12x10@60N',  '12x10@85DT',
         '16x12@60N',   '16x12@85DT', '19x12@60N',  '19x12@60DT',
         '19x12@120DT', '20x15@75DT', '25x14@60DT', '25x16@60DT'],
);

my %g_threeHeadDispModes =
(
    # For head 0.
    head0 =>
        ['10x7@60N',   '10x7@85DT',  '12x10@85DT', '16x12@60N',
         '19x12@60DT', '20x15@75DT', '25x14@60DT', '25x16@60DT'],
     
    # For head 1.
    head1 =>
        ['10x7@85DT',   '12x10@60N',  '16x12@85DT', '19x12@60N',
         '19x12@120DT', '20x15@75DT', '25x14@60DT', '25x16@60DT'],
     
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
        ['10x7@85DT',   '12x10@60N', '19x12@60N',
         '19x12@120DT', '20x15@75DT', '25x16@60DT'],
     
    # For head 2.
    head2 =>
        ['10x7@85DT',  '16x12@85DT', '19x12@60N',
         '25x14@60DT', '25x16@60DT'],
         
    # For head 3.
    head3 =>
        ['10x7@85DT',  '12x10@60N', '19x12@60N',
         '25x16@60DT']
);

# Table of all display configs.
my @g_rasterStructures = (LW_RASTER_STRUCTURE_PROGRESSIVE, LW_RASTER_STRUCTURE_INTERLACED);
my @g_bScalerOn444Modes = (TRUE, FALSE);
my @g_basePixelDepth = (1, 2, 4); #Removing FP16 verif
my @g_ovlyPixelDepth = (0, 1, 2, 4); #POR for overlay is 16/32 bits per pixels (from Philip Brown)
                                     #but we keep 8bits as it did exist in previous chip (from Rupesh)
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
        timingRP                  => 3,
        timingRFC                 => 15,
        tRCDRD                    => 15,
        tWCK2MRS                  => 4,
        tWCK2TR                   => 4,
        tMRD                      => 4,
        tMRSTWCK                  => 4,
        tMRS2RDWCK                => 25,
        tQPOPWCK                  => 11,
        tREFI                     => 1900,

        EXT_BIG_TIMER             => 1,
        STEP_LN                   => 32,
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
        tXSR                      => 5000,
        tXSNR                     => 120,
        dllOn                     => 0
    }
);

# FBP configs.
my %g_fbpConfigs =
(
    gk104 =>
    {
        # 2 or 4, depending on floorsweeping
        dramChipCountPerBwUnit    => 4,
        dramChipCountPerBwUnit_fs => 2,
        
        L2Slices => 4,
        ltcBwUnitPipes => 4,
        
        # 1 or 2, depending on floorsweeping
        enabledLtcs    => 2, 
        enabledLtcs_fs => 1, 

        enabledDramBwUnits    => 2,
        enabledDramBwUnits_fs => 1,
    },
    
    gk110 =>
    {
        # 2 or 4, depending on floorsweeping
        dramChipCountPerBwUnit    => 4,
        dramChipCountPerBwUnit_fs => 2,
        
        L2Slices => 4,
        ltcBwUnitPipes => 4,

        # 1 or 2, depending on floorsweeping
        enabledLtcs    => 2, 
        enabledLtcs_fs => 1, 

        enabledDramBwUnits    => 2,
        enabledDramBwUnits_fs => 1,
    }
);

# XBAR configs.
my %g_xbarConfigs =
(
    gk104 =>
    {
        maxFbpsPerXbarSlice => 1
    },
    
    gk110 =>
    {
        maxFbpsPerXbarSlice => 1
    }
);

my %g_isohubConfigs =
(
    gk104 =>
    {
        maxLwrsorEntries => 32,
        pitchFetchQuanta => 256,
    },
    
    gk110 =>
    {
        maxLwrsorEntries => 32,
        pitchFetchQuanta => 256,
    }
);

# Iso (display & isohub) configs.
my %g_isoConfigs =
(
    gk104 =>
    {
        num_heads           => 4,
        mempool             => 10240
    },
    
    gk110 =>
    {
        num_heads           => 4,
        mempool             => 10240
    }
);

# Clock configs.
my %g_clkConfigs =
(
    P0 =>
    {
        gddr5 =>
        {
            display   => 540,
            ram       => 3500,
            l2        => 1000,        
            xbar      => 1000,
            hub       => 667,
            sysclk    => 900,
            
            ram_gk104 => 2500,
            display_gk110 => 648,
        },
        
        sddr3 =>
        {
            display => 540,
            ram     => 800,        
            l2      => 1000,        
            xbar    => 1000,
            hub     => 667,
            sysclk  => 900,
            
            display_gk110 => 648,
        }
    },
    
    P8 =>
    {
        gddr5 =>
        {
            display => 540,
            ram     => 324,        
            l2      => 81,        
            xbar    => 162,
            hub     => 324,        
            sysclk  => 324,
        },
        
        sddr3 =>
        {
            display => 540,
            ram     => 324,        
            l2      => 81,        
            xbar    => 162,
            hub     => 324,        
            sysclk  => 324,
        }
    },
    
    P12 =>
    {
        gddr5 =>
        {
            display => 540,
            ram     => 324,
            l2      => 81,        
            xbar    => 162,
            hub     => 324,        
            sysclk  => 324,
        },
        
        sddr3 =>
        {
            display => 540,
            ram     => 324,        
            l2      => 81,        
            xbar    => 162,
            hub     => 324,        
            sysclk  => 324        
        }
    }
);

#----------------------------------------------------------------------------------------------
# Quick way to define custom IMP input configs.
#----------------------------------------------------------------------------------------------
my @g_archImpConfigDescs_GK104 =
(
    # A custom IMP config sample.
    # Use specific chip name "gf117" or "gf119" for chip-specific IMP configs.
    # Use the general chip name "gf11x" for common IMP configs.
    # Moved interlace to head 0 instead of head 1, as head 1 is going to use DP and DP do not support interlace (head0 will be dual_tmds)
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '16x12@85', structure => INTERLACED, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            
            {
                mode => '19x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    
    #
    # Group 1: One head internal panels with ASR enabled.
    #
    # 19x12@60N
    {
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
    # 10x7@85DT
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 8, viewPortRelation =>EVR_IN_INSIDE_MIN_MAX ,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    #
    # Group 2: one head desktop panels with ASR enabled.
    #
    # 10x7@60DT
    {
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
    
    # 19x12@120DT
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P8",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '19x12@120', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
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
        chip   => "gk10x",
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

    # two-head similar
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # FP16 on 2 heads
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 8, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # FP16 on 2 heads + 1 ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 8, viewPortRelation => EVR_IN_LT_MIN,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # three-head similar
    {
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip   => "gk10x",
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
        chip       => "gk10x",
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
        chip       => "gk10x",
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
    },

    #
    # Group 7: IMP testplan configs
    #          
    #
    # 

    # two-head similar, IMP TP, case 1
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_LT_MIN,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # three-head similar, IMP TP, case 2
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_LT_MIN,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_LT_MIN,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    }, 
    # four-head similar, IMP TP, case 3
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_GT_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_GT_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_LT_MIN,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # four-head similar, IMP TP, case 4
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_IN_GT_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # four-head similar, IMP TP, case 5,13
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # two-head similar, IMP TP, case 6,10,14,18
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x12@120', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    }, 
    # three-head similar, IMP TP, case 7,11,15,19
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x12@120', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # four-head similar, IMP TP, case 8,12,16,20
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x12@120', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # four-head skew case IMP TP, case for FP 16
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 8, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x12@120', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 8, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # one-head similar, IMP TP, case 9,17,21
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # two-head similar, IMP TP, case 22
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # one-head similar, IMP TP, case new
    {
        chip   => "gk10x",
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
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # one-head similar, IMP TP, case new
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '19x12@120', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # one-head similar, IMP TP, mclk_switch case 1
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => FALSE,
        mclkSwitchNeeded => TRUE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # one-head similar, IMP TP, mclk_switch case 2
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => FALSE,
        mclkSwitchNeeded => TRUE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '19x12@120', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # one-head similar, IMP TP, mclk_switch case 3
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => FALSE,
        mclkSwitchNeeded => TRUE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # one-head similar, IMP TP, mclk_switch case 4
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => FALSE,
        mclkSwitchNeeded => TRUE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # one-head similar, IMP TP, mclk_switch case 5
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => FALSE,
        mclkSwitchNeeded => TRUE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x12@120', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    }   
);

#Custom configurations list for the PERF verif
my @g_perfConfigDescs_GK104 =
(
    # one-head similar, case , 38x6@1827,  ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # one-head similar, case , 25x6@1353, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # one-head similar, case , 25x6@1353,  ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # one-head similar, case , 19x6@2091, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # one-head similar, case , 19x6@2091,  ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # one-head similar, case , 19x6@1881, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '19x6@1881', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # one-head similar, case , 19x6@1881,  ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '19x6@1881', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # two-head similar, case , 38x6@1827, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # two-head similar, case , 38x6@1827,  ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # three-head similar, case , 38x6@1827, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # three-head similar, case , 38x6@1827,  ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # four-head similar, case , 38x6@1827, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # four-head similar, case , 38x6@1827,  ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # two-head similar, case , 38x6@1827,25x6@1353, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # two-head similar, case , 38x6@1827,25x6@1353, ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # three-head similar, case , 38x6@1827,25x6@1353,19x6@2091, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # three-head similar, case , 38x6@1827,25x6@1353,19x6@2091, ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # four-head similar, case , 38x6@1827,25x6@1353,19x6@2091,19x6@1881, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '19x6@1881', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # four-head similar, case , 38x6@1827,25x6@1353,19x6@2091,19x6@1881, ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x6@1881', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # three-head similar, case , 25x6@1353,19x6@2091,19x6@1881, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '19x6@1881', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # three-head similar, case , 25x6@1353,19x6@2091,19x6@1881, ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x6@1881', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # two-head similar, case , 25x6@1353,19x6@2091, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # two-head similar, case , 25x6@1353,19x6@2091, ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # three-head similar, case , 38x6@1827,19x6@2091,19x6@1881, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '19x6@1881', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # three-head similar, case , 38x6@1827,19x6@2091,19x6@1881, ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x6@2091', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x6@1881', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    # three-head similar, case , 38x6@1827,25x6@1353,19x6@1881, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '19x6@1881', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },
    # three-head similar, case , 38x6@1827,25x6@1353,19x6@1881, ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        pstate => "P0",
        asr    => TRUE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '25x6@1353', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x6@1881', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    }
);

#Custom configurations list for the RM Verif
my @g_rmVerifConfigDescs_GK104 =
(
    # one-head similar, case , 38x6@1827, no ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        autoSync => 0,
        fastExit => 0,
        x16      => 0,
        pstate => "P0",
        asr    => FALSE,
        mscg   => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        CAPS => {
            MEMPOOL_COMPRESSION_DIS => 1,
        },
        disp_mode =>
        [
            {
                mode => '38x6@1827', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },

    # four-head similar, case , 38x21@60,  ovly
    {
        chip   => "gk10x",
        dram   => "gddr5",
        autoSync => 0,
        fastExit => 0,
        x16      => 1,
        pstate => "P0",
        asr    => FALSE,
        mscg   => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '38x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '38x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
            {
                mode => '38x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            }
        ]
    },

    # three-head similar, IMP TP, case 2, upscaling
    {
        chip   => "gk10x",
        dram   => "gddr5",
        autoSync => 0,
        fastExit => 1,
        x16      => 0,
        pstate => "P0",
        asr    => FALSE,
        mscg   => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        CAPS => {
            MIN_MEMPOOL => 1,
        },
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_LT_MIN,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_LT_MIN,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },

    # four-head similar, IMP TP, case 3, larger downscaling
    {
        chip   => "gk10x",
        dram   => "gddr5",
        autoSync => 0,
        fastExit => 1,
        x16      => 1,
        pstate => "P0",
        asr    => FALSE,
        mscg   => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>

        [
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_GT_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_GT_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '10x7@85', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_LT_MIN,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },

    # Single head 19x10@60DT, P12, GDDR5, Floorsweep (1FBP), with MSCG enabled
    {
        chip       => "gk10x",
        floorsweep => TRUE,
        dram       => "gddr5",
        autoSync   => 1,
        fastExit   => 0,
        x16        => 1,
        pstate     => "P12",
        asr        => FALSE,
        mscg       => TRUE,
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
    
    # Single head 19x10@60DT, P12, SDDR3, Floorsweep (1FBP), with MSCG enabled
    {
        chip       => "gk10x",
        floorsweep => TRUE,
        dram       => "sddr3",
        pstate     => "P12",
        asr        => FALSE,
        mscg       => TRUE,
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

    # Single head downscaling mode that is supposed to fail dispIMP.
    {
        chip   => "gk10x",
        dram   => "gddr5",
        autoSync   => 1,
        fastExit   => 0,
        x16        => 1,
        pstate => "P0",
        asr    => FALSE,
        mscg   => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '25x16@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_IN_INSIDE_MIN_MAX,
                baseLineBuf => TRUE, ovlyLineBuf => FALSE
            },
        ]
    },

    # Dual head with MSCG enabled & mclk_switch enabled.
    {
        chip   => "gk10x",
        dram   => "gddr5",
        autoSync   => 0,
        fastExit   => 0,
        x16        => 0,
        pstate => "P8",
        asr    => FALSE,
        mscg   => TRUE,
        mclkSwitchNeeded => TRUE,
        igpu => FALSE,
        disp_mode =>
        [
            {
                mode => '19x12@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '19x12@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf =>FALSE 
            },
        ]
    },

    #
    # Single-head ASR at P8 and P12.
    # gk104_DT_{P8, P12}_asr_h0_19x12_60_32bpp_ovly_{gddr5, sddr3}.txt
    #
    {
        chip   => "gk104",
        dram   => ["gddr5", "sddr3"],
        autoSync   => 0,
        fastExit   => 0,
        x16        => 1,
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mscg   => FALSE,
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
    
    #
    # Dual-head ASR at P8 and P12
    # gk104_DT_{P8, P12}_asr_h0_10x7_60_32bpp_h1_12x10_60_32bpp_{gddr5, sddr3}.txt
    #
    {
        chip   => "gk104",
        dram   => ["gddr5", "sddr3"],
        autoSync   => 0,
        fastExit   => 0,
        x16        => 0,
        pstate => ["P8", "P12"],
        asr    => TRUE,
        mscg   => FALSE,
        mclkSwitchNeeded => TRUE,
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
);

my @g_asrImpConfigDescs_GK104 =
(
    #
    # Single-head ASR at P8 and P12.
    #
    
    # gk104_DT_P{8, 12}_asr_h0_12x10_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_19x12_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_10x7_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_8x6_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_12x10_60_32bpp_ovly_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_19x12_60_32bpp_ovly_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_10x7_60_32bpp_ovly_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_8x6_60_32bpp_ovly_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_12x10_60_32bpp_h1_12x10_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_12x10_60_32bpp_h1_10x7_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_12x10_60_32bpp_h1_8x6_60_32bpp_{gddr5,sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_10x7_60_32bpp_h1_12x10_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_10x7_60_32bpp_h1_10x7_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_10x7_60_32bpp_h1_8x6_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_8x6_60_32bpp_h1_12x10_60_32bpp_{sddr3, gddr5}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_8x6_60_32bpp_h1_10x7_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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
    
    # gk104_DT_{P8, P12}_asr_h0_8x6_60_32bpp_h1_8x6_60_32bpp_{gddr5, sddr3}.txt
    {
        chip   => "gk104",
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

my @g_porOneHeadDescs_GK110 =
(
    # {3840x2400@60Hz, 8bpc, ovly}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },

    # {4096x2560@50Hz, 8bpc, ovly}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # {4096x2160@60Hz, 8bpc, ovly}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
);

my @g_porTwoHeadDescs_GK110 =
(
    # {3840x2400@60Hz, 8bpc, ovly : 3840x2400@60Hz, 8bpc}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # {4096x2560@50Hz, 8bpc, ovly : 4096x2560@50Hz, 8bpc}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # {4096x2160@60Hz, 8bpc, ovly : 4096x2160@60Hz, 8bpc, ovly}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
);

my @g_porThreeHeadDescs_GK110 =
(
    # {3840x2400@60Hz, 8bpc, ovly : 3840x2400@60Hz, 8bpc : 3840x2400@60Hz, 8bpc}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
   
    # {4096x2560@50Hz, 8bpc, ovly : 4096x2560@50Hz, 8bpc : 4096x2560@50Hz, 8bpc}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # {4096x2160@60Hz, 8bpc, ovly : 4096x2160@60Hz, 8bpc, ovly : 4096x2160@60Hz, 8bpc}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # {3840x2400@60Hz, 8bpc, ovly : 4096x2560@50Hz, 8bpc : 4096x2160@60Hz, 8bpc}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
);

my @g_porFourHeadDescs_GK110 =
(
    # {3840x2400@60Hz, 8bpc, ovly : 3840x2400@60Hz, 8bpc : 3840x2400@60Hz, 8bpc : 3840x2400@60Hz, 8bpc}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # {4096x2560@50Hz, 8bpc, ovly : 4096x2560@50Hz, 8bpc : 4096x2560@50Hz, 8bpc : 4096x2560@50Hz, 8bpc}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # {4096x2160@60Hz, 8bpc, ovly : 4096x2160@60Hz, 8bpc, ovly : 4096x2160@60Hz, 8bpc : 4096x2160@60Hz, 8bpc}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
    
    # {3840x2400@60Hz, 8bpc, ovly : 4096x2560@50Hz, 8bpc : 4096x2160@60Hz, 8bpc : 3840x2160@60Hz, 8bpc}
    {
        chip             => "gk110",
        dram             => ["gddr5"],
        pstate           => ["P0"],
        asr              => FALSE,
        mscg             => FALSE,
        mclkSwitchNeeded => FALSE,
        igpu             => FALSE,
        disp_mode        =>
        [
            {
                mode => '38x24@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 4, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x25@50', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '40x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            },
            {
                mode => '38x21@60', structure => PROGRESSIVE, bScalerOn444Mode => FALSE,
                basePixelDepth => 4, ovlyPixelDepth => 0, viewPortRelation => EVR_NO_SCALING,
                baseLineBuf => TRUE, ovlyLineBuf => TRUE
            }
        ]
    },
);

#----------------------------------------------------------------------------------------------
#   Public Functions
#----------------------------------------------------------------------------------------------

sub parseArgs {

    my $project;
    my $bProjFound;

    # check for -h option
    if ($opt_help)
    {
        usage();
    }

    if (!$opt_rmverif && !$opt_archverif)
    {
        die "Error: You must use one of rmverif of archverif command line options";
    }

    if ($opt_rmverif)
    {
        print "Creating configs for RM Verif\n";
    }

    if ($opt_archverif)
    {
        print "Creating configs for Arch Verif\n";
    }
    
    if ($opt_project ne "all")
    {
        $bProjFound = FALSE;
        foreach my $poject (keys %g_supChipInfo)
        {
            if ($opt_project eq $poject)
            {
                $bProjFound = TRUE;
                last;
            }
        }
        if (!$bProjFound)
        {
            die "Error: invalid project name $opt_project\n";
        }
    }
}

sub usage {
    print STDERR << "EOF";

    usage: $0 [options]

    --help             : Help message
    --rmverif          : creates config files for RM verif i.e. RM ImpTest
    --archverif        : creates config files for arch IMP verif
    --gen_arch_test_hdr: generate header files used by arch IMP test
    --project projName : generate IMP input config files only for the specified project, e.g, all/gk104/gk110
EOF
    exit 1;
}

# Function: main
# Desc: the main function of this script.
sub main ()
{
    # We want to produce the same sequence from rand(), so we use the same seed for all runs.
    srand (1024);
    
    # Generate the display mode table.
    &GenDispModeTable ();

    # Create the log file.
    open ($g_fLog, ">config.txt");
    open ($g_fLogFull, ">config_fullpath.txt");

    if ($opt_rmverif)
    {    
        &GenLwstomImpConfigs (\@g_rmVerifConfigDescs_GK104, "rm_verif");
    }

    if ($opt_archverif)
    {
        # Generate the display config database, used for sanity IMP config files
        &GenDispConfigDB ();
    
        # Generate IMP input config files for GK104.
        if (($opt_project eq "gk104") || $opt_project eq "all")
        {
            # Generate sanity IMP input config files.
            &GenSanityImpConfigs ();
    
            # Generate custom IMP input config files.
            &GenLwstomImpConfigs (\@g_archImpConfigDescs_GK104, "arch");
            &GenLwstomImpConfigs (\@g_asrImpConfigDescs_GK104, "asr");
            &GenLwstomImpConfigs (\@g_perfConfigDescs_GK104, "perf");
        }
        
        # Generate custom GK110 IMP input config files.
        if (($opt_project eq "gk110") || $opt_project eq "all")
        {
            &GenLwstomImpConfigs (\@g_porOneHeadDescs_GK110, "por_one_head");
            &GenLwstomImpConfigs (\@g_porTwoHeadDescs_GK110, "por_two_head");
            &GenLwstomImpConfigs (\@g_porThreeHeadDescs_GK110, "por_three_head");
            &GenLwstomImpConfigs (\@g_porFourHeadDescs_GK110, "por_four_head");
        }
    }

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
    printf "Info: total %d display modes.\n", scalar keys %g_dispModeTable;
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
    my ($impConfigFile, $dramType, $dramCfg) = @_;
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

    if ($dramType ne "gddr5")
    {
        print $impConfigFile "\n";
        return;
    }
    
    # GDDR5 ASR default values for Kepler are taken from GK107 silicon E1304.
    # Look at Bug 722918 for details.
    print $impConfigFile "DRAM.timingRP                                  $dramConfig->{timingRP}\n";
    print $impConfigFile "DRAM.timingRFC                                 $dramConfig->{timingRFC}\n";
    print $impConfigFile "DRAM.tMRS2RDWCK                                $dramConfig->{tMRS2RDWCK}\n";
    print $impConfigFile "DRAM.tQPOPWCK                                  $dramConfig->{tQPOPWCK}\n";
    print $impConfigFile "DRAM.tMRSTWCK                                  $dramConfig->{tMRSTWCK}\n";
    print $impConfigFile "DRAM.tREFI                                     $dramConfig->{tREFI}\n";

    if ($dramCfg && $dramCfg->{autoSync})
    {
        print $impConfigFile "DRAM.bAutoSync                                 $dramCfg->{autoSync}\n";
    }
    else
    {
        print $impConfigFile "DRAM.bAutoSync                                 0\n";
    }
    if ($dramCfg && $dramCfg->{fastExit})
    {
        print $impConfigFile "DRAM.bFastExit                                 $dramCfg->{fastExit}\n";
    }
    else
    {
        print $impConfigFile "DRAM.bFastExit                                 0\n";
    }
    if ($dramCfg && $dramCfg->{x16})
    {
        print $impConfigFile "DRAM.bX16                                      $dramCfg->{x16}\n";
    }
    else
    {
        print $impConfigFile "DRAM.bX16                                      0\n";
    }

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
        print $impConfigFile "FBP.dramChipCountPerBwUnit                     $fpbConfig->{dramChipCountPerBwUnit_fs}\n";
        print $impConfigFile "FBP.enabledDramBwUnits                         $fpbConfig->{enabledDramBwUnits_fs}\n";
        print $impConfigFile "FBP.enabledLtcs                                $fpbConfig->{enabledLtcs_fs}\n";
    }
    else
    {
        print $impConfigFile "FBP.dramChipCountPerBwUnit                     $fpbConfig->{dramChipCountPerBwUnit}\n";
        print $impConfigFile "FBP.enabledDramBwUnits                         $fpbConfig->{enabledDramBwUnits}\n";
        print $impConfigFile "FBP.enabledLtcs                                $fpbConfig->{enabledLtcs}\n";
    }
    print $impConfigFile "FBP.ltcBwUnitPipes                             $fpbConfig->{ltcBwUnitPipes}\n";
    print $impConfigFile "FBP.L2Slices                                   $fpbConfig->{L2Slices}\n";
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
    print $impConfigFile "XBAR.maxFbpsPerXbarSlice                       $xbarConfig->{maxFbpsPerXbarSlice}\n";
    print $impConfigFile "\n";
}

# Function: PrintIsoConfig
# Desc:     this function prints the ISO parameters.
sub PrintIsoConfig ()
{
    my ($isArch,$impConfigFile, $memPool, $impIsoConfigList) = @_;
    my $head;
    my ($lineBufFlag, $maxFlipPerf);
    my $impIsoConfig;
    my $equal_sign_string = "";

    if ($isArch) {
        $equal_sign_string = "=";
    }
    print $impConfigFile "ISO.linebufferAdditionalLines                 $equal_sign_string 1\n";
    print $impConfigFile "ISO.linesFetchedForBlockLinear                $equal_sign_string 2\n";
    print $impConfigFile "ISO.linebufferTotalBlocks                     $equal_sign_string $memPool\n";
    print $impConfigFile "ISO.linebufferMinBlocksPerHead                $equal_sign_string 16\n";
    for ($head = 0; $head < scalar @$impIsoConfigList; $head ++)
    {
        $impIsoConfig = $$impIsoConfigList[$head];
        $lineBufFlag = sprintf ("%d_%d", $impIsoConfig->{baseLineBuf}, $impIsoConfig->{ovlyLineBuf});
        
        if ($isArch) {
          print $impConfigFile "ISO.lineBufferingIsAllowed[$head]                 $equal_sign_string \"$lineBufFlag\"\n";
        } else {
          print $impConfigFile "ISO.lineBufferingIsAllowed[$head]                 $equal_sign_string $lineBufFlag\n";
        }
    }
    print $impConfigFile "\n";
}

# Function: GetChipClockConfig
# Desc:     this function returns the clock value of the specified unit for the specified chip.
sub GetChipClockConfig ()
{
    my ($clkConfig, $unit, $chip) = @_;
    my $clock;
    
    $clock = $clkConfig->{"${unit}_${chip}"};
    if (!defined ($clock))
    {
        $clock = $clkConfig->{"${unit}"};
    }
    return ($clock);
}

# Function: PrintClockConfig
# Desc:     this function prints the clock parameters.
sub PrintClockConfig ()
{
    my ($impConfigFile, $pstate, $dramType, $chip) = @_;
    my $clkConfig;
    my ($dispClk, $ramClk);
    
    $clkConfig = $g_clkConfigs{$pstate}->{$dramType};
    if (!defined ($clkConfig))
    {
        die "Error: invalid power state: $pstate, DRAM type: $dramType\n";
    }
    $dispClk = &GetChipClockConfig ($clkConfig, "display", $chip);    
    $ramClk = &GetChipClockConfig ($clkConfig, "ram", $chip);
    
    print  $impConfigFile "# CLOCKS Parameters\n";
    printf $impConfigFile "CLOCKS.display                                 %d\n", $dispClk * 1000000;
    printf $impConfigFile "CLOCKS.ram                                     %u\n", $ramClk * 1000000;
    printf $impConfigFile "CLOCKS.l2                                      %d\n", $clkConfig->{l2} * 1000000;
    printf $impConfigFile "CLOCKS.xbar                                    %d\n", $clkConfig->{xbar} * 1000000;
    printf $impConfigFile "CLOCKS.hub                                     %d\n", $clkConfig->{hub} * 1000000;
    printf $impConfigFile "CLOCKS.sys                                     %d\n", $clkConfig->{sysclk} * 1000000;
    printf $impConfigFile "\n";
}

# Function: PrintCapsConfig
# Desc:     this function prints capability parameters.
sub PrintCapsConfig ()
{
    my ($impConfigFile, $capsConfig) = @_;

    print $impConfigFile "# CAPS Parameters\n";
    print $impConfigFile "CAPS.bEccIsEnabled                             0\n";
    # Default setting is min_mempool disabled unless specified
    if ($capsConfig && $capsConfig->{MIN_MEMPOOL})
    {
        print $impConfigFile "CAPS.bForceMinMempool                          1\n";
    }
    else
    {
        print $impConfigFile "CAPS.bForceMinMempool                          0\n";
    }
    # Default setting is mempool compression enabled unless specified
    if ($capsConfig && $capsConfig->{MEMPOOL_COMPRESSION_DIS})
    {
        print $impConfigFile "CAPS.bMempoolCompression                       0\n";
    }
    else
    {
        print $impConfigFile "CAPS.bMempoolCompression                       1\n";
    }
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
    print $impConfigFile "ASR.efficiencyThreshold                        10\n";
    print $impConfigFile "ASR.dllOn                                      $dramConfig->{dllOn}\n";
    print $impConfigFile "ASR.tXSR                                       $dramConfig->{tXSR}\n";
    print $impConfigFile "ASR.tXSNR                                      $dramConfig->{tXSNR}\n";
    print $impConfigFile "ASR.powerdown                                  1\n";
    print $impConfigFile "\n";
}

# Function: PrintMscgConfig
# Desc:     this function prints the MSCG parameters.
sub PrintMscgConfig ()
{
    my ($impConfigFile, $enableMscg, $dramType) = @_;
    my $dramConfig;
    
    $dramConfig = $g_dramConfigs{$dramType};
    if (!defined ($dramConfig))
    {
        die "Error: invalid DRAM type: $dramType\n";
    }

    print $impConfigFile "# MSCG Parameters\n";
    print $impConfigFile "MSCG.bIsAllowed                                $enableMscg\n";
    print $impConfigFile "\n";
}

# Function: PrintIsohubConfig
# Desc:     this function prints the isohubIn parameters.
sub PrintIsohubConfig {
    my ($impConfigFile, $chipName) = @_;
    my $isohub = $g_isohubConfigs{$chipName};

    print $impConfigFile "# Isohub Parameters\n";
    print $impConfigFile "impIsohubIn.maxLwrsorEntries                   $isohub->{maxLwrsorEntries}\n";
    print $impConfigFile "impIsohubIn.pitchFetchQuanta                   $isohub->{pitchFetchQuanta}\n";
}

# Function: GenSanityImpConfigs
# Desc:     this function adds sanity IMP input configs, whose purpose is to test
#           the display IMP throughly.
sub GenSanityImpConfigs ()
{
    my $chipName;
    
    foreach $chipName (keys %g_supChipInfo)
    {
        if ($g_supChipInfo{$chipName}->{"genSanityConfig"})
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
                        $chipName, "NONE", FALSE, FALSE, "P0", "gddr5", undef, FALSE, FALSE, FALSE,
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
            $chipName, "NONE", FALSE, FALSE, "P0", "gddr5", undef, FALSE, FALSE, FALSE,
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
            $chipName, "NONE", FALSE, FALSE, "P0", "gddr5", undef, FALSE, FALSE, FALSE,
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
                    $chipName, "NONE", FALSE, FALSE, "P8", "gddr5", undef, $asr, FALSE, $mclkSwitchNeeded,
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
                $chipName, "NONE", FALSE, FALSE, "P0", "gddr5", undef, FALSE, FALSE, FALSE,
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
                $chipName, "NONE", FALSE, FALSE, $pState, "gddr5", undef, $asr, FALSE, $mclkSwitchNeeded,
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
                    $chipName, "NONE", FALSE, FALSE, "P0", "gddr5", undef, FALSE, FALSE, FALSE,
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
                    $chipName, "NONE", FALSE, FALSE, $pState, "gddr5", undef, $asr, FALSE, $mclkSwitchNeeded,
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
                        $chipName, "NONE", FALSE, FALSE, $pState, "gddr5", undef, $asr, FALSE, $mclkSwitchNeeded,
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
    my ($refImpConfis, $chipName, $CAPS, $bFloorsweep, $igpu, $pState, $dramType, $dramCfg, $asr, $mscg, $mclkSwitchNeeded, $refHeadsInfo, $comment) = @_;
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
    $newImpConfig{dramCfg} = $dramCfg;
    $newImpConfig{fos} = 0;
    $newImpConfig{cdos} = 0;
    $newImpConfig{igpu} = $igpu;
    $newImpConfig{asr} = $asr;
    $newImpConfig{mscg} = $mscg;
    $newImpConfig{mclkSwitchNeeded} = $mclkSwitchNeeded;
    if ($CAPS ne 'NONE')
    {
         $newImpConfig{CAPS} = $CAPS;
    }

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

    $impDispHeadIn{"outputResourcePixelDepthBPP"} = "LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_16_422";

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
        # RasterSize.Height needs to be ODD, so we add +1
        # for interlaced mode you will need to keep the raster balanced. So if you have to increase it,
        #  add 1 blank line to top and bottom and 2 blank lines in the middle
        #  (which ends up being 1 at the bottom of field 1 and 1 at the top of field 2).
        # So, add 1 to RasterBlankEnd_Y, RasterBlankStart_Y.
        # Add 3 to RasterVertBlank2_YEnd, RasterVertBlank2_YStart  ( +1 for already added blank start, + 2 to increase the middle gap by 2)
        # Add 4 to RasterSize_Height.                              ( +3 to account for adding through the middle gap, +1 to add one more blank line on the bottom)

        $impDispHeadIn{"Control.Structure"} = "LW_RASTER_STRUCTURE_INTERLACED";
        $heightField1 = int ($dispMode->{resY} / 2);
        $heightField2 = $dispMode->{resY} - $heightField1;
        $impDispHeadIn{"RasterBlankEnd.Y"} = 1 + int ($dispMode->{vblank_lines} / 2);
        $impDispHeadIn{"RasterBlankStart.Y"} = $impDispHeadIn{"RasterBlankEnd.Y"} + $heightField1;
        $impDispHeadIn{"RasterVertBlank2.YEnd"} = 2 + $impDispHeadIn{"RasterBlankStart.Y"} + $dispMode->{vblank_lines};
        $impDispHeadIn{"RasterVertBlank2.YStart"} = $impDispHeadIn{"RasterVertBlank2.YEnd"} + $heightField2;
        $impDispHeadIn{"RasterSize.Height"} = 4 + 1 + $dispMode->{resY} + $dispMode->{vblank_lines} * 2;
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

        ##lwbug 737173 From Henry:
        ##When scaling is used, the IMP config file must make sure the ViewportSizeOut is different
        ##from ViewportSizeIn and the IMP does not care the real value of ViewportSizeOut. So 0 is
        ##used since ViewportSizeIn is non-zero. But this config file cannot be used for real test run.
        $impDispHeadIn{"ViewportSizeOut.Width"} = int ($dispMode->{resX}); 
        $impDispHeadIn{"ViewportSizeOut.Height"} = int ($dispMode->{resY}); 
        $impDispHeadIn{"ViewportSizeOutMin.Width"} = int ($dispMode->{resX} );   # Keeping min = out so IMP callwlates the scaling properly 
        $impDispHeadIn{"ViewportSizeOutMin.Height"} = int ($dispMode->{resY}); 
        $impDispHeadIn{"ViewportSizeOutMax.Width"} = int ($dispMode->{resX} * 2 ); 
        $impDispHeadIn{"ViewportSizeOutMax.Height"} = int ($dispMode->{resY} * 2 );
        if ($viewPortRelation == EVR_IN_LT_MIN) # Upscaling
        {
            $impDispHeadIn{"ViewportSizeIn.Width"} = int ($dispMode->{resX} / 3);
            $impDispHeadIn{"ViewportSizeIn.Height"} = int ($dispMode->{resY} / 3);
        }
        elsif ($viewPortRelation == EVR_IN_INSIDE_MIN_MAX) # I don't know what this is for but assigning little downscaling
        {
            $impDispHeadIn{"ViewportSizeIn.Width"} = int ($dispMode->{resX}  * 3 / 2);
            $impDispHeadIn{"ViewportSizeIn.Height"} = int ($dispMode->{resY} * 3 / 2);
        }
        elsif ($viewPortRelation == EVR_IN_GT_MAX)
        {
            $impDispHeadIn{"ViewportSizeIn.Width"} = $dispMode->{resX} * 5 / 2;
            $impDispHeadIn{"ViewportSizeIn.Height"} = $dispMode->{resY} * 5 / 2;
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
    
    $strKeyDesc .= ($structure == LW_RASTER_STRUCTURE_PROGRESSIVE) ? "" : "_inter";
    if ($viewPortRelation != EVR_NO_SCALING)
    {
        $strKeyDesc .= $bScalerOn444Mode ? "_444" : "_422";
        #$strKeyDesc .= "_vpr${viewPortRelation}";
    }
    $strKeyDesc .= sprintf ("_%d", $basePixelDepth * 8);
    if ($ovlyPixelDepth != 0)
    {
        $strKeyDesc .= sprintf ("_ovly%d", $ovlyPixelDepth * 8);
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
    my $impArchConfigFileName;
    my $impArchConfigFile;
    my $impArchConfigName;
    my $archHeadParameter;
    my $head_is_DP_and_interlace;
    my $head_is_DP_and_interlace_count;
    
    # Info log.
    # print $g_fLog "Generate IMP input config files under <$outDir>:\n";
    $head_is_DP_and_interlace_count = 0;
    
    # Get current directory.
    $strPWD = `pwd`;
    chomp $strPWD;
    
    # Generate IMP input config files under the sub-directory.
    foreach $impConfig (@$refimpConfigs)
    {
        # Only generate IMP config files for chips supported.
        if (!defined ($g_supChipInfo{$impConfig->{chip}}))
        {
            next;
        }

        ## check if head in in both DP and interlace, which is unsupported
        $impDispHeadInList = $impConfig->{impDispHeadIn};
        $head_is_DP_and_interlace = 0;
        $sorIdx = 0;
        for ($head = 0; $head < scalar @$impDispHeadInList; $head ++)
        {
            $impDispHeadIn = $$impDispHeadInList[$head];
            if (($impDispHeadIn->{"PixelClock.Frequency"} > 165000) &&
                (($sorIdx == 1) | ($sorIdx == 2)) &&
                ($impDispHeadIn->{"Control.Structure"} eq "LW_RASTER_STRUCTURE_INTERLACED")){
                    $head_is_DP_and_interlace ++;
                    
                }
            $sorIdx ++;
        }


        if ($head_is_DP_and_interlace) {
            $head_is_DP_and_interlace_count ++;
            next;
        }


        # Create the sub-directory if not already.
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
        $impConfigFileName .= "_$impConfig->{pstate}";
        $impConfigFileName .= "_asr" if $impConfig->{asr};
        $impConfigFileName .= "_mscg" if $impConfig->{mscg};
        $impConfigFileName .= "_mclk" if $impConfig->{mclkSwitchNeeded};
       
        $impDispHeadInList = $impConfig->{impDispHeadIn};
        $impIsoConfigList = $impConfig->{impIsoConfig};
        for ($head = 0; $head < scalar @$impDispHeadInList; $head ++)
        {
            $impConfigFileName .= "_h${head}";
            
            $impDispHeadIn = $$impDispHeadInList[$head];
            $impConfigFileName .= "_$impDispHeadIn->{strKeyDesc}";
            
            $impIsoConfig = $$impIsoConfigList[$head];
            #$impConfigFileName .= "_$impIsoConfig->{maxFlipPerf}_$impIsoConfig->{baseLineBuf}_$impIsoConfig->{ovlyLineBuf}";
        }
        $impConfigFileName .= "_$impConfig->{dram}";
        $impConfigFileName .= ".txt";

        $impArchConfigFileName = $impConfig->{chip};
        $impArchConfigFileName .= $impConfig->{igpu} ? "_N" : "_DT";
        $impArchConfigFileName .= "_$impConfig->{pstate}";
        $impArchConfigFileName .= "_asr" if $impConfig->{asr};
        $impArchConfigFileName .= "_mscg" if $impConfig->{mscg};
        $impArchConfigFileName .= "_mclk" if $impConfig->{mclkSwitchNeeded};

        $impDispHeadInList = $impConfig->{impDispHeadIn};
        $impIsoConfigList = $impConfig->{impIsoConfig};

        for ($head = 0; $head < scalar @$impDispHeadInList; $head ++)
        {

            $impArchConfigFileName .= "_h${head}";

            $impDispHeadIn = $$impDispHeadInList[$head];
            $impArchConfigFileName .= "_$impDispHeadIn->{strKeyDesc}";

            $impIsoConfig = $$impIsoConfigList[$head];
            $impArchConfigFileName .= "_$impIsoConfig->{baseLineBuf}_$impIsoConfig->{ovlyLineBuf}";

        }

        $impArchConfigFileName .= "_$impConfig->{dram}";
        $impArchConfigName      = "$impArchConfigFileName";
        $impArchConfigFileName .= "_arch.txt";
        
        # Create this IMP input config file if not already.
        if (!defined ($impConfigFiles{$impConfigFileName}))
        {
            # Open the IMP input config file for writing.
            if (!open ($impConfigFile, ">", "$chipOutDir/$impConfigFileName"))
            {
                print "Warn: cannot open file <$chipOutDir/$impConfigFileName> for writing: $!\n";
                next;
            }

            # Open the arch IMP test header file for writing.
            if (!open ($impArchConfigFile, ">$impArchConfigFileName"))
            {
                print "Warn: cannot open file <$impArchConfigFileName> for writing: $!\n";
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
            &PrintDramConfig ($impConfigFile, $impConfig->{dram}, $impConfig->{dramCfg});
            
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
            &PrintIsoConfig (0,$impConfigFile, $memPool, $impIsoConfigList);
            
            # Print CLOCK parameters.
            &PrintClockConfig ($impConfigFile, $pstate, $impConfig->{dram}, $chip);
            
            # Print CAPs parameters.
            &PrintCapsConfig ($impConfigFile, $impConfig->{CAPS});
            
            # Print ASR parameters.
            &PrintAsrConfig ($impConfigFile, $impConfig->{asr}, $impConfig->{dram});
            
            # Print MSCG parameters.
            &PrintMscgConfig ($impConfigFile, $impConfig->{mscg}, $impConfig->{dram});

            # Print isohub params
            &PrintIsohubConfig ($impConfigFile, $chip);

            # Print head parameters.
            print $impArchConfigFile "#include \"../../../../include/imp_defines.h\"\n\n";
            printf $impArchConfigFile "%-50s\"%s\"\n", "SubTestName = ", $impArchConfigName;
            for ($head = 0; $head < scalar @$impDispHeadInList; $head ++)
            {
                $impDispHeadIn = $$impDispHeadInList[$head];
                print $impConfigFile "# Head $head Parameters\n";

                if($impDispHeadIn->{strKeyDesc} =~ /_422_/) {
                   printf $impArchConfigFile "%-50s%s\n", "Force_422[$head] = ", "1";
                }
                else {
                   printf $impArchConfigFile "%-50s%s\n", "Force_422[$head] = ", "0";
                }

                foreach $headParameter (sort (keys %$impDispHeadIn))
                {
                    next if $headParameter eq "strKeyDesc";
                    printf $impConfigFile "%-50s%s\n", "impDispHeadIn[$head].$headParameter", $impDispHeadIn->{$headParameter};
                    $archHeadParameter = "$headParameter";
                    $archHeadParameter =~ s/\./\_/g;
                    $archHeadParameter .= "[$head]";
                    printf $impArchConfigFile "%-50s%s\n", "$archHeadParameter = ", $impDispHeadIn->{$headParameter};
                }
                print $impConfigFile "\n";
                print $impArchConfigFile "\n";
            }
            printf $impArchConfigFile "%-50s%s\n", "num_heads_used = ", "$head\n";
            
            &PrintIsoConfig (1, $impArchConfigFile, $memPool, $impIsoConfigList);

            # The OR (combined with the protocol) has the orMaxPclkLimit and we should choose
            # the OR which will support the required pclk otherwise this mode will fail at
            # the very beginning of the dispIMP.
            #    Dac.orMaxPclkLimit  {Tv = 0, Crt = 400000}
            #    Sor.orMaxPclkLimit  {Dp = 270000, Single Link Tmds = 165000, Dual Link TMDS = 330000 Lvds = 230000}
            #    Pior.orMaxPclkLimit {???}
            # Setting 165MHz as limit and all the resolution below that would use SINGLE_TMDS and above DUAL_TMDS
            # Will not use DAC/PIOR or DP/LVDS to simplify the tests
            print $impConfigFile "# OR parameters\n";
            %OrTaken = ();
            $dacIdx = 1;
            $sorIdx = 0;
            for ($head = 0; $head < scalar @$impDispHeadInList; $head ++)
            {
                $impDispHeadIn = $$impDispHeadInList[$head];
                if ($impDispHeadIn->{"PixelClock.Frequency"} < 165000)
                {
                    printf $impConfigFile "%-50s%s\n", "impSorIn[$sorIdx].owner", "LW_OR_OWNER_HEAD${head}";
                    printf $impConfigFile "%-50s%s\n", "impSorIn[$sorIdx].protocol", "LW_SOR_PROTOCOL_SINGLE_TMDS_A";
                    printf $impArchConfigFile "%-50s%s\n", "or[$head] = ", "\"sor${sorIdx}\"";
                    printf $impArchConfigFile "%-50s%s\n", "orOwner[$head] = ", "LW_OR_OWNER_HEAD${head}";
                    printf $impArchConfigFile "%-50s%s\n", "orProtocol[$head] = ", "\"LW_SOR_PROTOCOL_SINGLE_TMDS_A\"";
                    $OrTaken{"Sor${sorIdx}"} = TRUE;
                    $sorIdx ++;
                }
                else
                {
                    printf $impConfigFile "%-50s%s\n", "impSorIn[$sorIdx].owner", "LW_OR_OWNER_HEAD${head}";
                    printf $impArchConfigFile "%-50s%s\n", "or[$head] = ", "\"sor${sorIdx}\"";
                    printf $impArchConfigFile "%-50s%s\n", "orOwner[$head] = ", "LW_OR_OWNER_HEAD${head}";
                    if(($sorIdx == 0) | ($sorIdx == 3)){
                      printf $impArchConfigFile "%-50s%s\n", "orProtocol[$head] = ", "\"LW_SOR_PROTOCOL_DUAL_TMDS\"";
                      printf $impConfigFile "%-50s%s\n", "impSorIn[$sorIdx].protocol", "LW_SOR_PROTOCOL_DUAL_TMDS";
                    }
                    else {
                      printf $impConfigFile "%-50s%s\n", "impSorIn[$sorIdx].protocol", "LW_SOR_PROTOCOL_DP_A";
                      printf $impArchConfigFile "%-50s%s\n", "orProtocol[$head] = ", "\"LW_SOR_PROTOCOL_DP_A\"";
                    }
                    $OrTaken{"Sor${sorIdx}"} = TRUE;
                    $sorIdx ++;
                }
                print $impConfigFile "\n";
                print $impArchConfigFile "\n";
            }
            
            # Print DAC parameters.
            print $impConfigFile "# DAC parameters\n";
            for ($dacIdx = 0; $dacIdx < GK10X_MAX_DACS; $dacIdx ++)
            {
                next if defined ($OrTaken{"Dac${dacIdx}"});
                printf $impConfigFile "%-50s%s\n", "impDacIn[$dacIdx].owner", "orOwner_None";
                printf $impConfigFile "%-50s%s\n", "impDacIn[$dacIdx].protocol", "LW_DAC_PROTOCOL_RGB_CRT";
                print $impConfigFile "\n";
            }
            
            # Print SOR parameters.
            print $impConfigFile "# SOR parameters\n";
            for ($sorIdx = 0; $sorIdx < GK10X_MAX_SORS; $sorIdx ++)
            {
                next if defined ($OrTaken{"Sor${sorIdx}"});
                printf $impConfigFile "%-50s%s\n", "impSorIn[$sorIdx].owner", "orOwner_None";
                printf $impConfigFile "%-50s%s\n", "impSorIn[$sorIdx].protocol", "LW_SOR_PROTOCOL_SINGLE_TMDS_A";
                print $impConfigFile "\n";
            }
            
            # Print PIOR parameters.
            print $impConfigFile "# PIOR parameters\n";
            for ($piorIdx = 0; $piorIdx < GK10X_MAX_PIORS; $piorIdx ++)
            {
                next if defined ($OrTaken{"Pior${piorIdx}"});
                printf $impConfigFile "%-50s%s\n", "impPiorIn[$piorIdx].owner", "orOwner_None";
                printf $impConfigFile "%-50s%s\n", "impPiorIn[$piorIdx].protocol", "LW_PIOR_PROTOCOL_EXT_TMDS_ENC";
                print $impConfigFile "\n";
            }

            # Done.
            close ($impConfigFile);
            close ($impArchConfigFile);
            if (!$opt_genArchImpTestHdr)
            {
                unlink $impArchConfigFileName;
            }
            
            # Info log.
            print $g_fLog "gk10x/$chipOutDir/$impConfigFileName\n";
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
    if ($head_is_DP_and_interlace_count) {
        print "Warn: skip writing <$head_is_DP_and_interlace_count> configs because was trying to use DP with interlace.\n";
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
    my ($mode, $igpu, $asr, $mscg, $mclkSwitchNeeded);
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
        $mscg = defined ($impConfigDesc->{mscg}) ? $impConfigDesc->{mscg} : FALSE;
        $mclkSwitchNeeded = defined ($impConfigDesc->{mclkSwitchNeeded}) ?
                            $impConfigDesc->{mclkSwitchNeeded} : FALSE;
        $impConfigDesc->{CAPS} = 'NONE' if !defined($impConfigDesc->{CAPS});

        # Create list of target chips.
        if (ref ($impConfigDesc->{chip}) eq "ARRAY")
        {
            @chipList = @{$impConfigDesc->{chip}};
        }
        else
        {
            foreach $chipName (keys %g_supChipInfo)
            {
                if ($g_supChipInfo{$chipName}->{family} =~ m/$impConfigDesc->{chip}/)
                {
                    push @chipList, $chipName;
                }
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
        my $dramCfg;
        foreach $dramType (@dramList)
        {
            if ($dramType =~ m/gddr5/i)
            {
               $dramCfg->{autoSync} = defined ($impConfigDesc->{autoSync}) ? $impConfigDesc->{autoSync} : 0;
               $dramCfg->{fastExit} = defined ($impConfigDesc->{fastExit}) ? $impConfigDesc->{fastExit} : 0;
               $dramCfg->{x16} = defined ($impConfigDesc->{x16}) ? $impConfigDesc->{x16} : 0;
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
                        $chipName, $impConfigDesc->{CAPS}, $bFloorsweep, $igpu, $pstate, $dramType,
                        $dramCfg, $asr, $mscg, $mclkSwitchNeeded,
                        \@headsInfo);
                }
            }
        }
    }
    
    # Generate these IMP input config files.
    &GenimpConfigFiles (\@impConfigs, $outDir);
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

#----------------------------------------------------------------------------------------------
# Entry point of this script.
#----------------------------------------------------------------------------------------------

# load options
# allow bundling of single char forms and case matters
Getopt::Long::Configure("bundling", "no_ignore_case");
GetOptions(\%opt,
           "help"               => \$opt_help,
           "rmverif"            => \$opt_rmverif,
           "archverif"          => \$opt_archverif,
           "gen_arch_test_hdr"  => \$opt_genArchImpTestHdr,
           "project=s"          => \$opt_project,
           )
    or usage();

parseArgs();
main();


#-----------------------------------------------------------------------------------------------
# The end.
#-----------------------------------------------------------------------------------------------
