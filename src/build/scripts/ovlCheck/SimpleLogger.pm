##!/usr/bin/perl -w

#
# Copyright 2014 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

package SimpleLogger;

use strict;
use Exporter;

our @EXPORT = qw(logError logInfo logDebug);


use constant               OFF   => 0;
use constant               ERROR => 1;
use constant               INFO  => 2;
use constant               DEBUG => 3;


our $log_level = OFF;
our $log_fh    = *STDERR;

sub logError {
    if ( $log_level >= ERROR ) {
        print $log_fh @_;
    }
}

sub logInfo {
    if ( $log_level >= INFO ) {
        print $log_fh @_;
    }
}

sub logDebug {
    if ( $log_level >= DEBUG ) {
        print $log_fh @_;
    }
}