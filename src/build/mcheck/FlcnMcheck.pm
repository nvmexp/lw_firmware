#!/usr/bin/perl
#
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
# File Name:   FlcnMcheck.pm
#
# See the wiki for full documentation
#    https://wiki.lwpu.com/engwiki/index.php/Mcheck
#

#===========================================================
package FlcnMcheck;

use File::Basename;
use Carp qw(cluck croak);

use Mcheck;
use comntools;
use FlcnMethodTagDb;
use FlcnLwolwtility;

our @ISA;
@ISA = qw( Mcheck );

sub parse_args
{
    my $self = shift;

    $self->SUPER::parse_args();

    # some command args are not supported in RTOS Flcn mcheck
    my @unsupported_args = qw (

            # instead of support '--chips', it nicer to have '--profile'
            chips

            # rmconfig_profile doesn't apply for RTOS FLCN.
            rmconfig_profile

            # filemap only runs single scan().  It requires some small code
            # change before enabling the feature.
            filemap
        );

    my $any_bad_arg = 0;
    foreach my $bad_arg (@unsupported_args)
    {
        if ($self->{opt}->{$bad_arg})
        {
            print "Option '--$bad_arg' is not supported\n";
            $self->{opt}->{$bad_arg} = undef;
            $any_bad_arg++;
        }
    }
    exit 1      if $any_bad_arg;
}

sub generate_headers
{
    my $self = shift;

    $self->generate_headers_default();

    # build flcndrf.h and appand it to $self->{CLIENT}->{SCAN_CPP_IMACROS}
    $self->build_drfheader_flcn();
}


sub build_drfheader_flcn
{
    my $self = shift;

    my $sdk_inc = $self->{LW_ROOT}."/sdk/lwpu/inc";
    my $drfheader_cpp_args;
    my $tmpHeaderDir = $self->get_tempfile_dir();
    my $kernel_inc = $self->{LW_ROOT}."/drivers/resman/kernel/inc";
    my $hwref_inc  = $self->{LW_ROOT}."/drivers/common/inc/hwref";
    my $swref_inc  = $self->{LW_ROOT}."/drivers/common/inc/swref";

    open DRF_FILE, ">$tmpHeaderDir/flcndrf.h";
    #
    # RM register access macros takes pGpu parameters while Rtos Flcn modules do not.
    #        RM usage : REG_RD32(pGpu, CSB, LW_PMGR_REGISTER)
    # RTOS Flcn usage : REG_RD32(CSB, LW_PMGR_REGISTER);
    #
    # undef those macros to avoid parsing error
    print DRF_FILE << "__EOL__";
#ifdef REG_RD32
#undef REG_RD32
#endif

#ifdef REG_WR32
#undef REG_WR32
#endif

__EOL__
    close DRF_FILE;

    $self->{CLIENT}->{SCAN_CPP_IMACROS} .= "-imacros $tmpHeaderDir/flcndrf.h ";
}



sub build_profile_db
{
    (@_ == 1) or croak 'usage: obj->build_profile_db()';

    util_print_elapsed_time; print "Building Profile DB...\n";
    __build_profile_db(@_);
    util_print_elapsed_time; print "done\n";
}

#    $self->{PROFILE_DB}
#       ->{PROFILES}                array of profiles
#       ->{ALL_CHIPS_H}             hash of all ucode chips from bindata query
#       ->{ALL_PROFILE_CHIPS_H}     hash of all profile chips defined in pmu-config.cfg
#       ->{DB}                      DB of each profiles hash for each profile DB
#           ->{pmu-gm20x}
#               ->{CHIPS_H}         ucode chips
#               ->{PROFILE_CHIPS_H} profile chips
# TODO: maybe move this to profiles.pm ?
# TODO: the client special implementation ($flcn_target eq 'dpu') should be moved out from Falcon shared code
sub __build_profile_db
{
    my $self = shift;

    my $rmcfg_cmd = $self->{LW_ROOT}.'/drivers/resman/config/rmconfig.pl';

    my $flcn_target   = $self->{CLIENT}->{FLCN_TARGET};         # 'pmu/dpu/sec2/acr'
    my $binFuncPrefix = $self->{CLIENT}->{UCODE_ACCESS_FUNC};   # 'pmuGetBinArchiveUcodeFalcon'

    croak 'FLCN_TARGET is not specified in %client_info'        unless $flcn_target;
    croak 'UCODE_ACCESS_FUNC is not specified in %client_info'  unless $binFuncPrefix;
    croak 'UCODE_ACCESS_FUNC has regex pattern with groups'     if ($binFuncPrefix =~ /[\(\)]/);

    # get valid profiles from (falcon) chip-config query
    my $validProfileH;
    my $validProfiles = $self->{RMCONFIG}->run_rmconfig("--print profiles");
    $validProfileH->{$_} = 1    foreach @$validProfiles;

    # get ucode chips from rmconfig halfunctions query
    my $query_cmd = "$rmcfg_cmd --profile fullrm --print halfunctions:ip2chips";
    my @results_rmconfig = grep { chomp; } `$query_cmd`;

    # check if any error
    fatal("Query to Rmconfig failed : '$queryCmd'")         if ($?);

    # get ucode chips from lwoc halfunctions query
    my @results_lwoc = $self->{LWOC_UTIL}->query_halinfo_lwoc($flcn_target);

    # @results contains the halinfo qeury result from both rmconfig and lwoc
    my @results = (@results_rmconfig, @results_lwoc);

    my %chipsByProfile;
    my %allChipsH;
    my @profiles;

    #  Output format:
    #  <Function Name>:    < CHIP List >
    # pmuGetBinArchiveUcode_GF10X: GF100 GF100B GF104 GF104B GF106 GF106B GF108
    # pmuGetBinArchiveUcode_GF11X: GF117 GF119
    # pmuGetBinArchiveUcode_GK10X: GK104 GK106 GK107
    # pmuGetBinArchiveUcode_GK11X: GK110 GK110B GK110C
    foreach my $line (@results) {

        my @items         = split ' ', $line;
        my $binAccessFunc = shift @items;           # pmuGetBinArchiveUcode_GF11X:
        my @chips         = @items;                 # chip list : GM10X GK20A GM20B GM20D DISPv0401

        next unless ($binAccessFunc =~ m/^($binFuncPrefix)_(.+):/);

        # Filter out ipversion. FlcnMcheck doesn't understand ipversion
        @chips = grep { $_ !~ m/^(DISP|DPU)v\d+/ } @chips;

        # Give error msg when no chips in chips list.
        # This could happen when bindata access function only uses ipversion to describe its usage, and we filtered out ipversion.
        die "No enable chips for function $binAccessFunc"       unless scalar @chips;

        $binAccessFunc  =~ s/://;   # drop the tailing ':' from 'pmuGetBinArchiveUcode_GF11X:'

        # get profile name from bindata access function
        my $profile = $self->getProfileNameFromBinAccessFunc($binAccessFunc);
        next unless defined $profile;

        if ($flcn_target eq 'pmu') {
            # MMINTS-TODO: this will have to be removed when we get rid of the _riscv suffixes!
            # This is quite messy, but it's designed to be easily removable. The _riscv suffixes
            # will hopefully be gone in a couple of months from now (once we get rid of the
            # backup falcon profiles for ga10b, gh100 and gh20x PMU).

            if ($validProfileH->{$profile.'_riscv'}) {
                if ($binAccessFunc =~ /riscv/i) {
                    # Add the RISCV profile suffix for riscv bindata funcs
                    $profile .= '_riscv';
                } else {
                    # Hack: if this is a falcon bindata func and a matching riscv profile exists, skip
                    next;
                }
            }
        }

        # Duplicate entry in hal-function result.  This is unlikely happen and probably an error
        # in mcheck implementation
        if ($self->{PROFILE_DB}->{DB}->{$profile}) {
            croak "Error: Profile '$profile' has multiple entries in halfunction query result. \n";
        }

        if (! $validProfileH->{$profile}) {
            print "Warning: Profile '$profile' doesn't exist.  The profile name is from\n".
                  "         Bindata access function '$binAccessFunc'.\n";
            next;
        }

        #      {PROFILE_DB}->{DB}->{pmu-gk20a}->{CHIPS_H}->{gk20a} = 1 foreach @chips;
        $self->{PROFILE_DB}->{DB}->{$profile}->{CHIPS_H}->{lc $_} = 1 foreach @chips;

        # $allChipsH{GM204} = 1;
        $allChipsH{lc $_} = 1  foreach @chips;

        push @profiles, $profile;
    }

    $self->{PROFILE_DB}->{PROFILES} = \@profiles;
    $self->{PROFILE_DB}->{ALL_CHIPS_H} = \%allChipsH;

    # Sanity check. FlcnMcheck should fail if no profiles found
    die "Cannot get profile names from Bindata Access function parsing"        unless scalar @profiles;

    my %allProfileChipsH;
    foreach my $profile (@profiles)
    {
        # TODO : effeciency improvement
        #   chip-config query is expensive, to shorten the running time, the the default chip
        #   can be got from parsing pmu-config.cfg directly
        my $output = $self->{RMCONFIG}->run_rmconfig("--profile $profile --print enabled-chips");

        if (scalar @$output > 1)
        {
            print "Warning: Profile '$profile' has multiple chips enabled: ".join(' ',@$output)." \n".
                  "         RTOS Flcn mcheck assumes it has single chip enabled per profile.\n".
                  "         It requires code change to avoid unexpected errors\n";
        }

        my $profile_chip = lc $output->[0];

        #      {PROFILE_DB}->{DB}->{pmu-gk20a}->{PROFILE_CHIPS_H}->{gk20a} = 1
        $self->{PROFILE_DB}->{DB}->{$profile}->{PROFILE_CHIPS_H}->{$profile_chip} = 1;

        if ($allProfileChipsH{$profile_chip})
        {
            # It should be safe to assume a profile chip is not used for more than one profiles.
            # Give a message if this assumption is no longer valid.
            print "Warning: Chip '$profile_chip' is the profile chip for at least two profiles:\n".
                  "         ".$allProfileChipsH{$profile_chip}." and $profile.\n".
                  "         It requires code change to avoid unexpected errors\n";
        }
        $allProfileChipsH{$profile_chip} = $profile;
    }
    $self->{PROFILE_DB}->{ALL_PROFILE_CHIPS_H} = \%allProfileChipsH;
}

sub getProfileNameFromBinAccessFunc_default
{
    (@_ == 2) or croak 'usage: obj->getProfileNameFromBinAccessFunc_default( FUNC-NAME )';

    my ($self, $binAccessFunc) = @_;

    my $flcn_target   = $self->{CLIENT}->{FLCN_TARGET};
    my $binFuncPrefix = $self->{CLIENT}->{UCODE_ACCESS_FUNC};

    $binAccessFunc =~ m/^($binFuncPrefix)_(.+)/;
    my $funcPostfix = $2;       # The hal provider name at function postfix, e.g. 'GF11X'

    my $profile = $flcn_target.'-'.(lc $funcPostfix);           # 'sec2-gp10x'

    return $profile;
}

# Client code should implement this function
# see getProfileNameFromBinAccessFunc_default() for reference
sub getProfileNameFromBinAccessFunc
{
    croak "client must implement getProfileNameFromBinAccessFunc()\n".
          "please refer to getProfileNameFromBinAccessFunc_default()\n";
}

sub set_target_profile
{
    (@_ == 2) or croak "usage: obj->set_target_profile( PROFILE-NAME/'all')";

    my ($self, $profile) = @_;

    $self->set_profile_target_chips($profile);

    # set the valid tag hash to new target chips
    $self->{METHOD_DB}->init_valid_tags();
}

# set_profile_target_chips()
#
# set {TARGET_CHIPS} and {TARGET_CHIPS_H} for specific profile
sub set_profile_target_chips
{
    (@_ == 2) or croak "usage: obj->set_target_chips( PROFILE-NAME/'all')";

    my ($self, $profile) = @_;

    my $profile_chip_H;
    if($profile eq 'all')
    {
        $profile_chip_H = $self->{PROFILE_DB}->{ALL_CHIPS_H};
    }
    else
    {
        $profile_chip_H = $self->{PROFILE_DB}->{DB}->{$profile}->{CHIPS_H};
    }
    croak "Cannot get chip list for profile '$profile'"       unless $profile_chip_H;

    my @target_chips;
    foreach my $chip (keys %{$profile_chip_H})
    {
        # skips the chips disabled in mcheck.config
        if (exists $self->{cfg}->{verify_chips}->{$chip} and !$self->{cfg}->{verify_chips}->{$chip})
        {
           print "Disabling $chip for mcheck.config setting\n";
           next;
        }

        push @target_chips, $chip;
    }

    my %target_chip_hash = ();
    $target_chip_hash{$_} = 1       foreach (@target_chips);

    $self->{TARGET_CHIPS} = \@target_chips;
    $self->{TARGET_CHIPS_H} = \%target_chip_hash;
}

#
# Rtos Flcn clients override run() function as they have very difference
# sequence
#
sub run
{
    my $self = shift;

    $self->build_profile_db();

    $self->generate_headers();  # skips for now

    # my $ip_map      = $self->{IP_MAP};
    my $profile_db  = $self->{PROFILE_DB};
    my $file_db     = $self->{FILE_DB};
    my $method_db   = $self->{METHOD_DB};
    my $chip_db     = $self->{CHIP_DB};
    my $exceptions  = $self->{EXCEPTIONS};
    my $manual_db   = $self->{MANUAL_DB};
    my $callgraph   = $self->{CALLGRAPH};
    my $scanner     = $self->{SCANNER};

    # enable all chips of all profiles while building DB for
    # method tags, call graph and manuals...
    $self->set_target_profile('all');

    $method_db->build_method_tag_db()           unless $self->{dbg}->{'no_method_tag'};

    $exceptions->set_exception_level();

    # $ip_map->build_ip_map();      # RTOS-Flcn ucodes don't really use IP yet
    $chip_db->build_netlist_data()  unless $self->{opt}->{no_snapshots};

    $file_db->build_file_list();

    $callgraph->build_callgraph()               unless $self->{dbg}->{'no_callgraph'};

    $chip_db->show_manual_targets();

    # --funclist?  Skips the rest steps after dumping functions list
    if ($self->{opt}->{funclist})
    {
        $method_db->dumpFunctionList();
        goto END_OF_RUN;
    }

    $manual_db->build_manual_db()               unless ($self->{opt}->{filemap} || $self->{dbg}->{'no_manual'});

    foreach $profile (@{$self->{PROFILE_DB}->{PROFILES}})
    {
        print "\n-----------------------------------------------------------------\n";
        print "Scan source code for profile : $profile...\n";
        print "     Ucode used by chips : ".(join ', ', sort keys %{$self->{PROFILE_DB}->{DB}->{$profile}->{CHIPS_H}})."\n";
        print "\n";
        $self->set_target_profile($profile);
        $scanner->scan();

        if (!$self->{opt}->{reglist}) {
            print "\nERROR and WARNING counts of profile $profile:\n";
            $exceptions->print_counts();
            $exceptions->save_and_reset_counts();
        }
    }

    ##
    ## output the results
    ##
    if ($self->{opt}->{reglist})
    {
        $manual_db->dumpRegisterList();
    }
    else
    {
        # output the results
        print "\n-----------------------------------------------------------------\n";
        print "Total ERROR and WARNING counts:\n";
        $exceptions->print_counts('all');
    }


  END_OF_RUN:
    printf "\nTotal Runtime : %4.4f sec\n", util_get_elapsed_time();

    # if there was any error code signal'd set it
    Mcheck::proper_exit($exceptions->get_error_counts('all'));

}

sub create_main_modules
{
    my ($self) = @_;

    $self->{METHOD_DB} = FlcnMethodTagDb->new();
    $self->{LWOC_UTIL} = FlcnLwolwtility->new();

    # call MethodTagDb::create_main_modules() to init the other modules
    $self->SUPER::create_main_modules();
}

