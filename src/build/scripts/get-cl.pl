#!/home/utils/perl5/perlbrew/perls/5.24.2-009/bin/perl -w 
#
# Copyright 2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# NOTE: must use a perl version with Params::ValidateCompiler installed
#

use strict;
use warnings "all";

use lib '/home/lw/utils/lwctdb/LATEST/lib';
use Moo;
use LW::CTDB::INC; 
use LW::CTDB;
use File::Spec::Functions qw(catfile);
use Getopt::Long;

# options
my $tagLabel = "";

# extract command-line arguments
GetOptions("tag=s" => \$tagLabel);

die "CTDB tag required!\n"
    if ( $tagLabel eq "" );

# ctdb api
my $ctdb;
my @events;
my $eventCl = "";

#
# Instantiate a CTDB object in order to pull the most 
# recent event under the 'sw_to_hw_mods_promotion_try' tag.
# The most recent event from this tag will be used to 
# collect the correct changelist to collect the correct
# version of each file needed for the promotion fix. 
#
$ctdb = LW::CTDB->new();

# Get the event using the correct tag
@events = $ctdb->get_event(tag => $tagLabel);

# Sanity Check: Ensure at least one event exists
if ( ( scalar @events ) == 0 )
{
    print("-1");
    exit;
}

# Get the changelist of the most recent event
$eventCl = $events[0][0]{"CL_p4sw"};

# Sanity Check: See if the event is defined
if ( ! defined $eventCl )
{
    print("-1");
    exit;
}

# Sanity Check: Make sure the CL is retrieved
if ( $eventCl eq "" )
{
    print("-1");
    exit;
}

# Display the Event's CL
print("$eventCl");
exit;
