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
# File Name:   FlcnLwolwtility.pm
#

package FlcnLwolwtility;
use Carp qw(cluck croak);

# HACK to get access to $MCHECK from mcheck.pl
our $MCHECK;
*MCHECK  = \$main::MCHECK;


sub new
{
    # 'usage: FlcnLwolwtility->new( )';

    my $class = shift;

    my $self = {
    };

    return bless $self, $class;
}


# Create a dummy header "lwtypes.h" in temp directory, so that we don't have to include the real one with
# complicated paths and included headers during the query.
sub _create_dummy_lwtypes_header
{
    (@_ == 1) or croak 'usage: obj->_create_dummy_lwtypes_header( )';

    my $tmp_dir      = $MCHECK->get_tempfile_dir();
    my $dummy_header = $tmp_dir."/lwtypes.h";

    open HEADERFILE, ">$dummy_header"   or die $!;
    print HEADERFILE << "__EOF__";
/*!
 *  This is a fake lwtypes.h only for FlcnMcheck to query lwh-trans.
 */
typedef unsigned __INT32_TYPE__ LwU32; /* 0 to 4294967295 */
__EOF__
    close HEADERFILE;
 
    printf("Dummy header file %s created.\n", $dummy_header);
}


sub _delete_dummy_lwtypes_header
{
    (@_ == 1) or croak 'usage: obj->_delete_dummy_lwtypes_header( )';

    my $tmp_dir      = $MCHECK->get_tempfile_dir();
    my $dummy_header = $tmp_dir."/lwtypes.h";

    unlink $dummy_header    or die "Could not delete file $dummy_header!\n";
}


sub query_halinfo_lwoc
{
    (@_ == 2) or croak 'usage: obj->query_halinfo_lwoc( FLCN_TARGET )';

    my ($self, $flcn_target) = @_;

    my $tmp_dir = $MCHECK->get_tempfile_dir();

    #
    # rmconfig - generate g_chips2halspec.h
    #
    my $rmcfg_cmd = $MCHECK->{LW_ROOT}.'/drivers/resman/config/rmconfig.pl';
    my $query_cmd_rmconfig = "$rmcfg_cmd --gen-header rmconfig.h --enable all-chips --skip-srcfiles-chk --output-directory ${tmp_dir}";
    `$query_cmd_rmconfig`;

    # check if any error
    fatal("Run rmconfig failed : '$query_cmd_rmconfig'")         if ($?);

    # create dummy "lwtypes.h" before the query
    $self->_create_dummy_lwtypes_header();

    # 
    # query halinfo from lwh-trans
    #
    # objdummy.h is a fake lwoc header which includes ucode access functions. 
    # The header is only used for generating halinfo.
    my $target_file = $MCHECK->{LW_ROOT}."/uproc/build/mcheck/objdummy.h";

    my $query_cmd = $MCHECK->{LW_ROOT}."/drivers/resman/src/libraries/lwoc/tools/bin/linux/lwh-trans -query=halinfo ${target_file} -- ";
    $query_cmd .= "-x lwoc ";
    $query_cmd .= "-I${tmp_dir} ";
    $query_cmd .= "-I$MCHECK->{LW_ROOT}/drivers/resman/kernel/inc ";
    $query_cmd .= "-I$MCHECK->{LW_ROOT}/drivers/resman/inc/physical ";
    $query_cmd .= "-I$MCHECK->{LW_ROOT}/drivers/common/inc/hwref ";
    $query_cmd .= "-DFLCN_TARGET_" . uc $flcn_target . " ";
    $query_cmd .= "-DMCHECK -DLWOC -Wno-unknown-pragmas";

    printf("Query halinfo from lwh-trans using command:\n %s\n", $query_cmd);

    my @results_lwoc = grep { chomp; } `$query_cmd`;

    # check if any error
    fatal("Query to lwh-trans failed : '$$query_cmd'")         if ($?);

    # delete dummy "lwtypes.h" after finished the query
    $self->_delete_dummy_lwtypes_header();

    return @results_lwoc; 
}

1;

