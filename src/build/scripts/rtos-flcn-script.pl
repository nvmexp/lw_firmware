#!/usr/bin/perl -w

#
# Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# rtos-flcn-script.pl
# version 1.00
#
# Example usage :
#   > perl -Ic:/code/sw/dev/gpu_drv/chips_a/pmu_sw/build rtos-flcn-script.pl
#        --profile pmu-gm20b --gen-ostask-header
#
# Or use the wrapper script at build dir of Flcn module:
#   > cd .../chips_a/pmu_sw/build
#   > perl pmu-script.pl --help
#   > perl pmu-script.pl --profile pmu-gm20b --gen-ostask-header --verbose 2
#

use strict;
use warnings 'all';

use Getopt::Long;
use Carp;                              # for 'croak', 'cluck', etc.
use File::Basename;
use File::Spec;

BEGIN {
    # Save and show the original command line when the exelwtion fails.  The content
    # of @INC is changed after 'BEGIN' block.  So we need to cache the value here.
    # use 'our' to make this variable available outside 'BEGIN' block.
    our $g_orgCmdLine = '';

    #   e.g. "-Ic:/code/sw/dev/gpu_drv/chips_a/pmu_sw/build"
    my $cmdIncPath = join " ", map { '-I'.$_ } grep { m/gpu_drv/ } @INC;
    $g_orgCmdLine  = join " ", $cmdIncPath, $0, @ARGV;

    # HACK : add chip-config dir @INC path
    # for using chip-config modules without '-Ic:/long/path/to/chip-config' in command line
    my $buildDir = dirname __FILE__;
    unshift @INC, File::Spec->catfile($buildDir, '..', '..', '..','drivers', 'common', 'chip-config');
    unshift @INC, File::Spec->catfile($buildDir);
}

use Utils;                             # import chip-config utility functions
use ProfileUtil;                       # import chip-config profile utility functions
use chipGroups;                        # imports CHIPS from Chips.pm and chip functions
my $Chips;                             # ref to obj chipGroups

use impl::TasksImpl;                   # Implementations for Tasks
use impl::ProfilesImpl;                # Implementations for Profiles
use impl::SectionsImpl;                # Implementations for Sections
use ldgen::GenLdscript;                # Generate link-script
use analyze::UcodeAnalyze;             # Statically analyze objdump
use util::CfgReaderUproc;              # utility to import chip-config CFG files

#
# Global variables
#
my $opt_profile    = '';
my $opt_opmode     = '';        # 'pmu/dpu/sec2', parsed from $opt_profile
my $opt_build      = '';        # build name, parsed from $opt_profile
my $opt_arch       = 'falcon';  # 'riscv/falcon'
my $opt_lwroot     = '';        # LW_ROOT

my $opt_verbose                      = 0;
my $opt_gen_profile_header           = 0;
my $opt_gen_ostask_header            = 0;
my $opt_gen_analyze_header           = 0;
my $opt_gen_ldscript                 = 0;
my $opt_gen_dmem_end_carveout        = 0;
my $opt_gen_dmesg_buffer             = undef;
my $opt_gen_identity_mapped_sections = 0;
my $opt_gen_makefile                 = 0;
my $opt_gen_sections_header          = 0;
my $opt_gen_sections_source          = 0;
my $opt_check_overlay_imem           = 0;
my $opt_check_nm_file                = '';     # path to *.nm file
my $opt_analyze_objdump              = 0;
my $opt_validate_no_objdump_overlays = 0;

my $opt_outfile   = '';
my $opt_template  = '';

my $opt_overlaysimem_file    = 'OverlaysImem.pm';
my $opt_overlaysdmem_file    = 'OverlaysDmem.pm';
my $opt_dmem_ovl_vars_long   = 0;
my $opt_elf_in_place         = 0;
my $opt_elf_in_place_odp_cow = 0;

my $chipCfgCmd;         # chip-config.pl command with opmode (pmu/dpu/sec2-config)
my $PERL;               # perl program name

# global var to collect shared data
our $UPROC;

END {
    our $g_orgCmdLine;      # original command line cached in BEGIN block

    # if exit code is not zero, dump the original command line to facilitate debugging
    if ($?) {
        print "\nFailed command:\n  ";
        print ($PERL? $PERL : "perl");
        print ' '.$g_orgCmdLine."\n";
    }
}


#
# functions
#
sub usage {

    print <<__USAGE__;

Usage:

   perl rtos-flcn-script.pl [ options ]

Options:

    -? --help                      This message

    --profile <name>               The target profile.  Ex. 'pmu-gm10x'

    --arch <name>                  The target architecture. May be 'falcon' (default) or 'riscv'

    --gen-sections-header          Generate header file for RISCV sections defines

    --gen-sections-source          Generate source file for RISCV sections data

    --gen-profile-header           Generate header file for general Profile defines

    --gen-ostask-header            Generate header file for OSTASK defines

    --gen-analyze-header           Generate header file for defines from Ucode Analyze

    --gen-ldscript                 Generate linker-script

    --gen-dmem-end-carveout        Generate carveout of given size at the end of DMEM (RISCV only)

    --gen-dmesg-buffer             Override the size of the dmesg buffer at the end of the above carveout (RISCV only)
                                   Intended for use by offline-extracted builds that need to reserve space for the bootstub
                                   Defaults to using the full DMEM carveout if not specified

    --gen-identity-mapped-sections Generate identity mapped sections (RISCV only)

    --gen-makefile                 Generate makefile for Profile configuration

    --check-overlays-imem          Check Tasks overlays size in IMEM

    --check-nm-file <file>         Check resident code boundary

    --analyze-objdump <file>       Extract callgraph to generate overlay rules and compute stack usage

    --validate-no-objdump-overlays Skip validating overlays from objdump

    --outfile                      Path for output file

    --template                     Path for template file

__USAGE__

    exit 0;
}

sub getEnabledChips {

    (@_ == 1) or croak 'usage: getEnabledChip( OP-MODE, PROFILE-NAME )';

    my ($profile) = @_;

    my $cmd = "$chipCfgCmd --profile $profile --print enabled-chips";

    # so far it has only one enabled chip for each profile,
    my $enabledChips = `$cmd`;
    chomp($enabledChips);

    # give a fatal message if the query fails
    if(!$enabledChips)  {
        print ("Error: cannot get valid enable chip list for profile '$opt_profile'\n");
        exit 1;
    }

    return $enabledChips;
}


#
# Scan NM file to check if any function body is located at resident code
# boundary (__resident_code_end)
#
sub checkResidentCodeBoundary {
   (@_ == 1) or croak 'usage: checkResidentCodeBoundary( NM-FILE-PATH )';

    my ($nm_file) = @_;

    if (! -f $nm_file) {
        print("Error: Cannot find NM file : $nm_file\n");
        exit 1;
    }

    my $line;
    my $hsOvlEnd = 0;
    my $residentEnd = 0;
    my $nsOvlStart = 0;

    #
    # first loop:   find '__resident_code_end'
    #
    open NM_FILE, '<', $nm_file;

    while ($line = <NM_FILE>) {
        # Ex: 0000147C A __resident_code_end
        if ($line =~ /^([0-9A-Fa-f]+) A __resident_code_end$/) {
            $residentEnd = hex($1);
        }
        # Ex: 00002900 A __non_hs_overlay_code_start
        elsif ($line =~ /^([0-9A-Fa-f]+) A __non_hs_overlay_code_start$/) {
            $nsOvlStart = hex($1);
        }
        # Ex: 00000100 A __hs_overlay_code_end
        elsif ($line =~ /^([0-9A-Fa-f]+) A __hs_overlay_code_end$/) {
            $hsOvlEnd = hex($1);
        }
        # stop the loop when we get all required offsets
        last if ($residentEnd && $nsOvlStart && $hsOvlEnd);
    }

    if (! $residentEnd) {
        print("Error: Parsing error in NM file '$nm_file', cannot find valid '__resident_code_end'.\n");
        exit 1;
    }
    if (! $nsOvlStart) {
        print("Error: Parsing error in NM file '$nm_file', cannot find valid '__non_hs_overlay_code_start'.\n");
        exit 1;
    }

    printf "    __resident_code_end  = 0x%x\n",$residentEnd        if $opt_verbose;
    printf "    __non_hs_overlay_code_start = 0x%x\n",$nsOvlStart  if $opt_verbose;
    printf "    __hs_overlay_code_end = 0x%x\n",$hsOvlEnd          if $opt_verbose;

    close NM_FILE;

    #
    # second loop:  check start/end offset of each function
    #
    open NM_FILE, '<', $nm_file;
    my ($start, $end, $function);
    while (<NM_FILE>) {
        # Ex: 00001600 00000007 t _dpuHalIfacesSetup_GM107
        #     0000179e 00000012 T _dpuInit_v02_00
        if (m/^([0-9A-F]+) ([0-9A-F]+) T (\w+)$/i) {
            $start = hex($1);
            $end   = hex($1) + hex($2);
            $function = $3;

            # skips the rest items if the next funtion located in overlay section
            last if ($start >= $nsOvlStart);

            if (($start <= $residentEnd && $end > $residentEnd) ||
                ($start <= $hsOvlEnd  && $end > $hsOvlEnd)){
                print("Error: $function() is not assigned to valid overlay or resident code!\n");
                exit 1;
            }
        }
    }
    close NM_FILE;
}

sub setup {

    my %cfgArgs;

    my $cfgParseResult = GetOptions(\%cfgArgs,

        'help|?',                      # --help / -?
        'profile=s'                    => \$opt_profile,
        'arch=s'                       => \$opt_arch,
        'gen-profile-header'           => \$opt_gen_profile_header,
        'gen-ostask-header'            => \$opt_gen_ostask_header,
        'gen-analyze-header'           => \$opt_gen_analyze_header,
        'gen-ldscript'                 => \$opt_gen_ldscript,
        'gen-dmem-end-carveout=o'      => \$opt_gen_dmem_end_carveout,
        'gen-dmesg-buffer=o',          => \$opt_gen_dmesg_buffer,
        'gen-identity-mapped-sections' => \$opt_gen_identity_mapped_sections,
        'gen-makefile'                 => \$opt_gen_makefile,
        'gen-sections-header'          => \$opt_gen_sections_header,
        'gen-sections-source'          => \$opt_gen_sections_source,
        'check-overlays-imem'          => \$opt_check_overlay_imem,
        'check-nm-file=s'              => \$opt_check_nm_file,
        'verbose=i'                    => \$opt_verbose,
        'elf-in-place'                 => \$opt_elf_in_place,
        'elf-in-place-odp-cow'         => \$opt_elf_in_place_odp_cow,
        'template=s'                   => \$opt_template,
        'outfile=s'                    => \$opt_outfile,

        'analyze-objdump=s'            => \$opt_analyze_objdump,
        'validate-no-objdump-overlays' => \$opt_validate_no_objdump_overlays,

        # those options are added for makefiles
        'perl=s',                      # --perl PERLPATH
        'lwroot=s'                     => \$opt_lwroot,

        'overlaysimem-file=s'          => \$opt_overlaysimem_file,
        'overlaysdmem-file=s'          => \$opt_overlaysdmem_file,
        'dmem-ovl-vars-long'           => \$opt_dmem_ovl_vars_long
    );

    # --help  or parsing error
    #     fatal("Arguments parsing error")    if ! $result;
    usage()                     if ! $cfgParseResult;
    usage()                     if $cfgArgs{help};

    # get and check 'perl' cmd
    if ($cfgArgs{perl}) {
        $PERL = File::Spec->catfile($cfgArgs{perl});
    } else {
        $PERL = 'perl';
    }

    # get opmode from profile name
    $opt_opmode = $opt_profile;
    $opt_opmode =~ s/^(\w+)-[\w-]+$/$1/;   # pmu-gf11x -> pmu
                                           # fbflcn-tu10x-hbm -> fbflcn  (extended profile group to include additional "-"

    if ($opt_opmode !~ m/^pmu|dpu|sec2|soe|fbfalcon|gsp$/ ) {
        print("Error: cannot get valid opmode for profile '$opt_profile'\n");
        exit 1;
    }

    # setup $chipCfgCmd path
    my $chipCfgPath;    # full path to chip-config.pl
    my $cfgFileName;    # xxx-config.cfg file for pmu/dpu/sec2
    my $cfgFilePath;    # path to config file
    my $srcRootPath;    # path to src root

    $cfgFileName = $opt_opmode."-config.cfg";

    # init LW_ROOT
    if (!$opt_lwroot) {

        if (defined $ELW{LW_ROOT}) {
            $opt_lwroot = $ELW{LW_ROOT};
        }
        if (defined $ELW{LW_SOURCE}) {
            $opt_lwroot = $ELW{LW_SOURCE};
        }
        else {
            # get LW_ROOT from the path of current file
            $opt_lwroot = File::Spec->catfile((dirname __FILE__), '..', '..', '..');
        }
    }

    $chipCfgPath = File::Spec->catfile($opt_lwroot,
                        'drivers', 'common', 'chip-config', 'chip-config.pl');

    # for $srcRootPath, it must be UNIX format
    my @tmp = split /\\/, $opt_lwroot;
    if ($opt_opmode eq 'pmu') {
        $cfgFilePath = File::Spec->catfile($opt_lwroot,
                            'pmu_sw', 'config', $cfgFileName);
        $srcRootPath = File::Spec::Unix->catfile( @tmp ,
                            'pmu_sw', 'prod_app');
    } elsif ($opt_opmode eq 'dpu') {
        $cfgFilePath = File::Spec->catfile($opt_lwroot,
                            'uproc', 'disp', 'dpu', 'config', $cfgFileName);
        $srcRootPath = File::Spec::Unix->catfile( @tmp ,
                            'uproc', 'disp', 'dpu', 'src');
    } elsif ($opt_opmode eq 'sec2') {
        if (uc $opt_arch eq 'RISCV') {
            $cfgFilePath = File::Spec->catfile($opt_lwroot,
                                'uproc', 'sec2_riscv', 'config', $cfgFileName);
            $srcRootPath = File::Spec::Unix->catfile( @tmp ,
                                'uproc', 'sec2_riscv', 'src');
        } elsif (uc $opt_arch eq 'FALCON') {
            $cfgFilePath = File::Spec->catfile($opt_lwroot,
                                'uproc', 'sec2', 'config', $cfgFileName);
            $srcRootPath = File::Spec::Unix->catfile( @tmp ,
                                'uproc', 'sec2', 'src');
        }
    } elsif ($opt_opmode eq 'soe') {
        if (uc $opt_arch eq 'RISCV') {
            $cfgFilePath = File::Spec->catfile($opt_lwroot,
                                'uproc', 'soe_riscv', 'config', $cfgFileName);
            $srcRootPath = File::Spec::Unix->catfile( @tmp ,
                                'uproc', 'soe_riscv', 'src');
        } elsif (uc $opt_arch eq 'FALCON') {
            $cfgFilePath = File::Spec->catfile($opt_lwroot,
                                'uproc', 'soe', 'config', $cfgFileName);
            $srcRootPath = File::Spec::Unix->catfile( @tmp ,
                                'uproc', 'soe', 'src');
        }
    } elsif ($opt_opmode eq 'fbfalcon') {
        $cfgFilePath = File::Spec->catfile($opt_lwroot,
                            'uproc', 'fbflcn', 'config', $cfgFileName);
        $srcRootPath = File::Spec::Unix->catfile( @tmp ,
                            'uproc', 'fbflcn', 'src');
    } elsif ($opt_opmode eq 'gsp') {
        $cfgFilePath = File::Spec->catfile($opt_lwroot,
                            'uproc', 'gsp', 'config', $cfgFileName);
        $srcRootPath = File::Spec::Unix->catfile( @tmp ,
                            'uproc', 'gsp', 'src');
    }

    $chipCfgCmd  = "$PERL $chipCfgPath ".
                   "--mode ${opt_opmode}-config ".
                   "--config $cfgFilePath ".
                   "--source-root $srcRootPath ".
                   '';

    # init chip info if option '--profile' is given in the command line
    if ($opt_profile) {
        # get enabled chip list
        my $enabledChips = getEnabledChips($opt_profile);

        # init Chips object to access chipGroups util functions
        $Chips = chipGroups->new('rtos-flcn-script');     utilExitIfErrors();

        if (chipNameIsValid($enabledChips, CHIP_ALIAS_OK) ||
            chipFamilyNameIsValid($enabledChips, CHIP_ALIAS_OK)) {
            chipUpdateEnabledList(1 , $enabledChips);
        }
        else {
            print ("Error: The enabled chip list for profile '$opt_profile' is not valid : $enabledChips\n");
            exit 1;
        }

        my $cfgData = CfgReaderUproc::readCfgFile($cfgFilePath);
        my @PROFILES = @{$cfgData->{PROFILE_LIST}};

        my $ProfileUtil = ProfileUtil->new($opt_profile, $cfgData->{PROFILE_LIST});
        $UPROC->{PROFILE_UTIL} = $ProfileUtil;
    }

    # cache options in global var $UPROC
    $UPROC->{opt}->{verbose} = $opt_verbose;
    $UPROC->{opt}->{profile} = $opt_profile;

}



#
# run
#
setup();

# generate header g_profile.h
if ($opt_gen_profile_header) {

    if (!$opt_outfile) {
        $opt_outfile = 'g_profile.h';
    }

    my $Profiles;
    $Profiles = Profiles->new('PROFILE_HEADER', $opt_arch); utilExitIfErrors();
    $Profiles->printProfileHeader($opt_profile, $opt_outfile);
    exit 0;
}

# generate header g_tasks.h
if ($opt_gen_ostask_header) {

    if (!$opt_outfile) {
        $opt_outfile = 'g_tasks.h';
    }

    my $TasksRef;
    $TasksRef = Tasks->new('TASK_HEADER',
                           $opt_profile,
                           $opt_lwroot,
                           $opt_verbose);           utilExitIfErrors();

    $TasksRef->printOstaskHeader($opt_outfile);
    exit 0;
}

# generate header g_analyze.h
if ($opt_gen_analyze_header) {

    if (!$opt_outfile) {
        $opt_outfile = 'g_analyze.h';
    }

    my $TasksRef = Tasks->new('UCODE_ANALYZE',
                              $opt_profile,
                              $opt_lwroot,
                              $opt_verbose);      utilExitIfErrors();

    my $ProfilesRef = Profiles->new('UCODE_ANALYZE', $opt_arch); utilExitIfErrors();

    my $AnalyzeRef = UcodeAnalyze->new($TasksRef, $ProfilesRef);

    $AnalyzeRef->printAnalyzeHeader($opt_outfile);
    exit 0;
}

# generate ld-script
if ($opt_gen_ldscript) {

    if (! $opt_template) {
        print ("Error: need to specify the template file by option '--template'\n");
        exit 1;
    }
    if (! -e $opt_template) {
        print ("Error: template file doesn't exist\n");
        exit 1;
    }
    if (!$opt_outfile) {
        $opt_outfile = 'g_sections.ld';
    }
    if (! defined $opt_gen_dmesg_buffer) {
        $opt_gen_dmesg_buffer = $opt_gen_dmem_end_carveout;
    }

    my $genLdRef;   # ref to GenLdScript
    $genLdRef = GenLdscript->new('LD-SCRIPT',
                                 $opt_profile,
                                 $opt_lwroot,
                                 $opt_overlaysimem_file,
                                 $opt_overlaysdmem_file,
                                 $opt_verbose,
                                 $opt_elf_in_place,
                                 $opt_elf_in_place_odp_cow,
                                 $opt_arch,
                                 $opt_dmem_ovl_vars_long,
                                 $opt_gen_dmem_end_carveout,
                                 $opt_gen_dmesg_buffer,
                                 $opt_gen_identity_mapped_sections
                                 );            utilExitIfErrors();

    $genLdRef->init();                                  utilExitIfErrors();

    $genLdRef->emitFileFromTemplate($opt_template, $opt_outfile);

    exit 0;
}

if ($opt_gen_makefile) {

    if (!$opt_outfile) {
        $opt_outfile = 'g_profile.mk';
    }

    # Create the dir for output file if it doesn't exist.
    # This is required because makefile could be referenced before
    # creation of the output dir.
    if (! -e dirname($opt_outfile)) {
        mkdir dirname($opt_outfile);
    }

    my $Profiles;
    $Profiles = Profiles->new('PROFILE_MAKEFILE', $opt_arch); utilExitIfErrors();
    $Profiles->printProfileMakefile($opt_profile, $opt_outfile);
    exit 0;
}

if ($opt_gen_sections_header) {

    if (! $opt_template) {
        print ("Error: need to specify the template file by option '--template'\n");
        exit 1;
    }
    if (! -e $opt_template) {
        print ("Error: template file doesn't exist\n");
        exit 1;
    }
    if (!$opt_outfile) {
        $opt_outfile = 'g_sections_riscv.h';
    }

    Sections->printSectionsHeader($opt_profile, $opt_template, $opt_outfile);

    exit 0;
}

if ($opt_gen_sections_source) {

    if (!$opt_outfile) {
        $opt_outfile = 'g_sections_data.c';
    }

    Sections->printSectionsSource($opt_profile, $opt_outfile);

    exit 0;
}

# check resident code segment boundary
if ($opt_check_nm_file) {
    checkResidentCodeBoundary($opt_check_nm_file);

    # no exit : several tests can be done in a single run
}

if ($opt_check_overlay_imem) {

    my $TasksRef;
    $TasksRef = Tasks->new('OVL_CHECK',
                           $opt_profile,
                           $opt_lwroot,
                           $opt_verbose);           utilExitIfErrors();

    $TasksRef->buildCfgFeatureList($chipCfgCmd."--profile $opt_profile ");

    $TasksRef->validateTaskNames();

    $TasksRef->checkOverlayImemSize($opt_overlaysimem_file);

    # no exit : several tests can be done in a single run
}

if ($opt_analyze_objdump) {
    my $output_dir = $opt_outfile or die "Option --outfile required";

    my $TasksRef = Tasks->new('UCODE_ANALYZE', $opt_profile,
                              $opt_lwroot, $opt_verbose);          utilExitIfErrors();

    my $ProfilesRef = Profiles->new('UCODE_ANALYZE', $opt_arch);   utilExitIfErrors();

    my $AnalyzeRef = UcodeAnalyze->new($TasksRef, $ProfilesRef, $opt_arch);
    $AnalyzeRef->init($opt_analyze_objdump);

    # Only perform OVL analysis for Falcon
    if (uc $opt_arch eq 'FALCON') {
        my $profileRef = $ProfilesRef->grpItemRef($AnalyzeRef->{BUILD});
        # Only if DMEM VA is enabled
        if ($ProfilesRef->dmemVaSize($profileRef)) {
            my $OvlRef = Overlays->new('UCODE_ANALYZE', 'dmem', $opt_overlaysdmem_file);
            $OvlRef->parseOverlaysDmemStackInfo();
            $AnalyzeRef->{DMEM_OVL_INFO} = $OvlRef;
        }
    }

    if (uc $opt_arch eq 'RISCV') {
        my $profileRef = $ProfilesRef->grpItemRef($AnalyzeRef->{BUILD});

        # MMINTS-TODO: pass these file names in as cmdline args
        my $RiscvSectionsDataRef = Sections->new('UCODE_ANALYZE', 'data', 'RiscvSectionsData.pm',
                                                 $profileRef->{DATA_SECTION_PREFIX});
        my $RiscvSectionsCodeRef = Sections->new('UCODE_ANALYZE', 'code', 'RiscvSectionsCode.pm',
                                                 $profileRef->{CODE_SECTION_PREFIX});

        $AnalyzeRef->{DATA_SECTION_INFO} = $RiscvSectionsDataRef;
        $AnalyzeRef->{CODE_SECTION_INFO} = $RiscvSectionsCodeRef;
    }

    $AnalyzeRef->analyze();                             utilExitIfErrors();
    $AnalyzeRef->output($output_dir);
    $AnalyzeRef->validate(!$opt_validate_no_objdump_overlays && (uc $opt_arch eq 'FALCON'));

    exit 0;
}

## done!

1;
