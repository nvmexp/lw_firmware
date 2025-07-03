#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#
#
#
# dpu-config file that specifies all known dpu-config features
#

package Features;
use warnings 'all';
no warnings qw(bareword);       # barewords makes this file easier to read
                                # and not so important for error checks here

use Carp;                       # for 'croak', 'cluck', etc.

use Groups;                     # Features is derived from 'Groups'

@ISA = qw(Groups);


# This list contains all fbfalcon-config features.
#
my $featuresRef = [

    # platform the software will run on.
    # For now, just have unknown and FBFALCON
    PLATFORM_UNKNOWN =>
    {
       DESCRIPTION         => "Running on an unknown platform",
       DEFAULT_STATE       => DISABLED,
    },

    PLATFORM_FBFALCON =>
    {
       DESCRIPTION         => "Running on the FB Falcon",
       DEFAULT_STATE       => DISABLED,
    },

    # target architecture
    ARCH_UNKNOWN =>
    {
       DESCRIPTION         => "unknown arch",
       DEFAULT_STATE       => DISABLED,
    },
    ARCH_FALCON =>
    {
       DESCRIPTION         => "FALCON architecture",
       DEFAULT_STATE       => DISABLED,
    },


    # This feature is special.  It and the FBFALCONCORE_<<gpuFamily>> features will
    # be automatically enabled by rmconfig based on the enabled gpus.
    FBFALCONCORE_BASE =>
    {
       DESCRIPTION         => "FBFLCN Base",
       SRCFILES            => [
                               "start.S", 
                                "priv.c",
                              ],
    },

    FBFALCONCORE_FBFLCN_BASE =>
    {
       DESCRIPTION         => "FBFLCN Base",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [

                                "fbflcn_interrupts.c", 
                                "fbflcn_helpers.c",
                                "fbflcn_timer.c",  
                                "fbflcn_table.c",
                                "fbflcn_access.c",
                                "fbflcn_mutex.c",
                                "revocation.c",
                                "interface.c",
                                "fbflcn_dummy_mclk_switch.c",
                              ],
    },

    FBFALCONCORE_GV10X =>
    {
       DESCRIPTION         => "FBFLCN GV10X Base",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [],
    },


    FBFALCONCORE_TU10X =>
    {
       DESCRIPTION         => "FBFLCN TU10X Base",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [],
    },

    FBFALCONCORE_GA10X =>
    {
       DESCRIPTION         => "FBFLCN GA10X Base",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [],
    },

    FBFALCONCORE_AD10X =>
    {
       DESCRIPTION         => "FBFLCN GA10X Base",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [],
    },
   
    FBFALCONCORE_ADDITIONS_GA10X =>
     {
       DESCRIPTION         => "FBFLCN Features added to the GA10X Base that carry forward",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "data.c",
                              ],
    },   

    FBFALCONCORE_ADDITIONS_AD10X =>
     {
       DESCRIPTION         => "FBFLCN Features added to the AD10X Base that carry forward",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                              ],
    },   
   
    FBFALCONCORE_GH10X =>
    {
       DESCRIPTION         => "FBFLCN GH10X Base",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [],
    },
   
    # wrap around features that were added in GH10X and are intented to carry forward
    # to the next generation, but use to avoid polution of older binaries
    FBFALCONCORE_ADDITIONS_GH10X =>
    {
       DESCRIPTION         => "FBFLCN Features added to the GH10X Base",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [],
    },
   
    FBFALCONCORE_GH20X =>
    {
       DESCRIPTION         => "FBFLCN GH20X Base",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [],
    },
   
    FBFALCONCORE_ADDITIONS_GH20X =>
    {
       DESCRIPTION         => "FBFLCN Features added to the GH20X Base that carry forward",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [],
    },

    FBFALCON_CORE_AUTOMOTIVE_BOOT =>
    {
       DESCRIPTION         => "FBFLCN ucode target selector for automotive boot core",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "boot/automotive/turing/main.c",
                              ],
    },
    
    FBFALCON_CORE_BOOT =>
    {
       DESCRIPTION         => "FBFLCN ucode target selector for ROM (boot functionality) contents",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "boot/main.c",
                              ],
    },
    
    FBFALCON_CORE_RUNTIME =>
    {
       DESCRIPTION         => "FBFLCN ucode target selector for runtime binary (mclk switch etc)",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "main.c",
                              ],
    },

    PAFALCON_SEQUENCER  =>
    {
       DESCRIPTION         => "support for PAFALCON sequencer",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "segment.c",
                                "seq_decoder.c",
                              ],
    },

    PAFALCON_CORE =>
    {
       DESCRIPTION         => "PA FBFLCN ucode target selector for runtime binary (mclk switch etc)",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "seq_decoder.c",
                                "paflcn/main.c",
                              ],
    },
    PAFALCON_INTERRUPT =>
    {
       DESCRIPTION         => "PA FBFLCN error interrupt enable",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "fbflcn_interrupts.c",
                              ],
    },
   
    PAFALCON_STORAGE =>
	{
       DESCRIPTION         => "Feature to enable PA Falcon storage for debug and training values",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                              ],
    },

    PAFALCON_VERIF =>
    {
       DESCRIPTION         => "Feature to enable verification binary for pafalcon",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "verif/paflcn/main.c",
                              ],
    },
   
    FBFALCON_HW_BAR0_MASTER =>
    {
       DESCRIPTION         => "FBFLCN ucode flag for bar0 master hw support. This creates a separation between csb and priv space",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                              ],
    },
    
    FBFALCON_SW_BAR0_MASTER_ENFORCED =>
    {
       DESCRIPTION         => "FBFLCN ucode flag for bar0 master hw support. This creates a separation between csb and priv space",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                              ],
    },

    FBFALCON_JOB_BOOT_TRAINING =>
    {
       DESCRIPTION         => "Boot Time Training support for FBFalcon ucode",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "fbflcn_gddr_boot_time_training_tu10x.c",
                                "fbflcn_gddr_boot_time_training_ga10x.c",
                                "fbflcn_hbm_boot_time_training_gh100.c",
                              ], 
    },

    # HBM2 Feature inllwdes multiple files that are mutually exclusive,
    # the selction of which file to load based of targets is done through
    # the mechanism in scrddefs/Sources.def
    FBFALCON_JOB_MCLK_HBM2 =>
    {
       DESCRIPTION         => "FBFLCN Job - MCLK Switch for HBM2",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "fbflcn_hbm_mclk_switch.c",
                              ],
    },

    # new HBM support starting with Hopper
    FBFALCON_JOB_MCLK_HBM_GH100 =>
    {
       DESCRIPTION         => "FBFLCN Job - MCLK Switch for HBM starting with Hopper",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "fbflcn_hbm_mclk_switch_gh100.c",
                                "fbflcn_hbm_shared_gh100.c",
                              ],
    },
   
    FBFALCON_JOB_MCLK_GDDR =>
    {
       DESCRIPTION         => "FBFLCN Job - MCLK Switch GDDR5/5x/6",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "fbflcn_gddr_mclk_switch_tu10x.c",
                                "fbflcn_gddr_mclk_switch_ga10x.c",
                              ],
    },

    FBFALCON_JOB_TREFI_HBM =>
    {
       DESCRIPTION         => "FBFLCN Job - Temperature readout on HBM IEEE1500 and update refresh rate",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "fbflcn_ieee1500_gv100.c"
                              ],
    },

    FBFALCON_JOB_TEMP_TRACKING =>
    {
       DESCRIPTION         => "FBFLCN Job - Temperature readout on GDDR6 GA10x and update refresh rate",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "temp_tracking.c"
                              ],
    },

    FBFALCON_JOB_HBM_PERIODIC_TR =>
    {
       DESCRIPTION         => "FBFLCN Job - Perform periodic training via IEEE1500 read of WOSC",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "fbflcn_hbm_periodic_training.c"
                              ],
    },

    FBFALCON_JOB_CMDQUEUE_SUPPORT =>
    {
       DESCRIPTION         => "FBFLCN Feature - Enable communication through cmdqueus & messages queues",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "fbflcn_queues.c",
                              ],
    },
    
    FBFALCON_JOB_PRIV_PROFILING =>
    {
       DESCRIPTION         => "FBFLCN Feature - Enable priv profiling",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                              ],
    },

    SUPPORT_GDDR =>
    {
       DESCRIPTION         => "GDDR support enabled",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                                "memory/turing/memory_gddr_tu10x.c",
                                "memory/ampere/memory_gddr_ga10x.c",
                              ],
    },

    SUPPORT_HBM =>
    {
       DESCRIPTION         => "HBM support enabled",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [
                              ],
    },
    
    SUPPORT_NITROC =>
    {
       DESCRIPTION         => "NitroC support enabled",
       DEFAULT_STATE       => DISABLED,
       SRCFILES            => [],
    },

    FBFALCON_JOB_TRAINING_DATA_SUPPORT =>
    {
        DESCRIPTION        => "Training data handler for boottime training",
        DEFAULT_STATE      => DISABLED,
        SRCFILES           => [
                                "data.c",
                              ],
    }, 
   
    TRAINING_DATA_IN_SYSMEM =>
    {
    	DESCRIPTION => "Training data is saved in and restored from a sha256 protected region in sysmem",
    	DEFAULT_STATE => DISABLED,
    	SRCFILES => [
    				],
    },

    DEBUG_OUTPUT =>
    {
        DESCRIPTION        => "Enable debug output through mailbox registers",
        DEFAULT_STATE      => DISABLED,
        SRCFILES           => [
                                "data.c",
                              ],
    },
    
    VBIOS_TABLE_SUPPORT_PERFORMANCE_TABLE =>
    {
        DESCRIPTION        => "Enable loading and processing of vbios performance table",
        DEFAULT_STATE      => DISABLED,
    },
    
    # gpio table parsing was added in the gh100 timeframe, before that the gpio pins were hard coded or used a
    # substitute field in the pmct. See bug: https://lwbugswb.lwpu.com/LwBugs5/SWBug.aspx?bugid=2060971
    # implementation is only working properly with VBIOS_TABLE_PREPARSING_VDPA enabled and GH100 Hal layers
    VBIOS_TABLE_SUPPORT_GPIO_TABLE =>
    {
    	DESCRIPTION        => "Enable loading and parsing of GPIO table for voltage pin assignement",
    	DEFAULT_STATE      => DISABLED,
    },
   
    VBIOS_TABLE_SUPPORT_TRAINING_TABLE =>
    {
    	DESCRIPTION        => "Enable loading and parsing of training tables",
    	DEFAULT_STATE      => DISABLED,
    },

    NO_RELEASE_REVOCATION_DISABLED =>
    {
        # Control flag to disable revocation.  
        # This flag disables revobation and should never be released on silicion. Only to be used in sim
        # at the beginning of a new project.  After moving to boot42 this whole mechanism should be removed
	    # again.
        DESCRIPTION   => "Disable revocaion for simulation. This should not be enabled on silicon",
        DEFAULT_STATE => DISABLED,
    },
   
    REGISTER_ACCESS_RETRY =>
    {
        # This feature will retry failing priv requests (only non-posted request) should there be a priv error
        # This was implemented as a workaround for bug 200470194 
        # Use of this feature should be reviewed an probably disabled once we have a better solution and understanding
        # of the priv issues we're seeing on Turing
        DESCRIPTION   => "Enable retry feature on priv accesses",
        DEFAULT_STATE => DISABLED,
    },
          
    VBIOS_TABLE_PREPARSING_VDPA =>
    {
        # The vdpa table for content lookup in the vbios was initially loadded in its entity into the fbfalcon dmem,
        # as the table keeps growing in takes up more and more dmem space, the change to Cert 3.0 increses the footprint
        # furthermore by adding sha keys to each entry.
        # The preparsing addresses this issue by doing the vdpa loading and looup first and retain only the pointers
        # to the table entries before overwriting the with the reqquired vbios tables.
        DESCRIPTION   => "preparses vdba table before loading vbios tables",
        DEFAULT_STATE => DISABLED,
    },
   
    FBFALCON_BUG_3088142_DRAM_BW_55PCT =>
    {
    	# bugfix for Bug 3088142 in Ampere, this will update to 55% dram bandwith
    	DESCRIPTION => "Increase display bandwith to55% for bug 3088142",
    	DEFAULT_STATE => DISABLED,
    },
   
    VBIOS_INCLUDED_IN_BINARY =>
    {
    	# This feature will package relevant VBIOS tables directly into the dmem space of the falcon binary
    	# Intented to be used in cases where the falcon binary needs to run stand_alone w/o external memory
    	# and RM support and was a request by ATE
    	DESCRIPTION => "Prepackage relevant vbios tables directly into the dmem space of the falcon binary",
    	DEFAULT_STATE => DISABLED,
    },
    
    MINING_LOCK_SUPPORT =>
    {
        # This feature will enable functionality to limit the use of mining and the stress/temperature increase
        # caused to memory subsystem. 
        DESCRIPTION => "Support functions to limit/throttle features when mining is performed",
        DEFAULT_STATE => DISABLED,
    },
   
    DECODE_HBM_MEMORY_DEVICE_ID =>
    {
        # This feature will read the device id from an HBM memory through IEEE1500 and decode 
        # strap with a match to the memory information table
        # Intention is to use this for a stand alone binary as used in ATE where the strap
        # lookup has not been programmed.
        DESCRIPTION => "Support ieee1500 readout of memory device id to use for strap lookup",
        DEFAULT_STATE => DISABLED,
    },
];


# Create the Features group
sub new {

    @_ == 1 or croak 'usage: obj->new()';

    my $type = shift;

    my $self = Groups->new("feature", $featuresRef);

    $self->{GROUP_PROPERTY_INHERITS} = 1;   # FEATUREs inherit based on name

    return bless $self, $type;
}

# end of the module
1;
