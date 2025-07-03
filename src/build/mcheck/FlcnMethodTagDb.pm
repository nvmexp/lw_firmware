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
# File Name:   FlcnMethodTagDb.pm
#
# See the wiki for full documentation
#    https://wiki.lwpu.com/engwiki/index.php/Mcheck
#

package FlcnMethodTagDb;
use Carp qw(cluck croak);

our @ISA;
@ISA = qw( MethodTagDb );

# HACK to get access to $MCHECK from mcheck.pl
our $MCHECK;
*MCHECK  = \$main::MCHECK;


sub __build_method_tag_db
{
    (@_ == 1) or croak 'usage: obj->__build_method_tag_db()';

    my $self = shift @_;

    ##
    ## Query hal functions for chip list of profile chips only,
    ##
    my @valid_tags = keys %{$MCHECK->{PROFILE_DB}->{ALL_PROFILE_CHIPS_H}};

    # init the hash for valid tags
    $self->init_valid_tags(\@valid_tags);

    # query chip-config
    my $enabled_chip_str = join '', map {"--enable $_ "} @valid_tags;
    $MCHECK->{RMCONFIG}->set_extra_args($enabled_chip_str);
    $self->SUPER::__build_method_tag_db();
    $MCHECK->{RMCONFIG}->set_extra_args('');

    # reset valid tags back to the current target chips
    $self->init_valid_tags();

    ##
    ## Expand profile chips to ucode chips
    ##
    $self->expand_method_tag_db();
}

#
# profile chips => ucode chips
#
# Expand list of profile chips to list of ucode chips
# Save the chip-config query to {DB_ORG}, the new list of ucode chips of in {DB}
sub expand_method_tag_db
{
    (@_ == 1) or croak 'usage: obj->expand_method_tag_db()';

    my $self = shift @_;

    # save the original chip-config query result in {DB_ORG}
    $self->{DB_ORG} = delete $self->{DB};
    $self->{DB} = {};

    foreach my $hal_function (keys %{$self->{DB_ORG}})
    {
        my $org_chip_h = $self->{DB_ORG}->{$hal_function};

        # colwert each profile chip to the list of ucode chips of the target profile
        foreach my $org_chip (keys %$org_chip_h)
        {
            my $profile = $MCHECK->{PROFILE_DB}->{ALL_PROFILE_CHIPS_H}->{$org_chip};
            croak 'Invalid data in $MCHECK->{PROFILE_DB}'        unless $profile;

            my $new_chip_h = $MCHECK->{PROFILE_DB}->{DB}->{$profile}->{CHIPS_H};
            $self->{DB}->{$hal_function}->{$_} = 1      foreach keys %$new_chip_h;
        }

        if($MCHECK->{opt}->{verbose})
        {
            print "$hal_function\n";
            print "    profile chips : ".(join ' ', keys %$org_chip_h)."\n";
            print " => ucode chips   : ".(join ' ', keys %{$self->{DB}->{$hal_function}})."\n";
        }
    }
}

