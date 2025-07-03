#!/usr/bin/perl -w

#
# Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#

#
# This module is responsible for sourcing the appropriate GenLdscript script
# based on the build's architecture.
#

package GenLdscript;

use strict;
no strict qw(vars);
use warnings 'all';

use ldgen::GenLdscriptFalcon;
use ldgen::GenLdscriptRiscv;

use Carp;

sub new {
    (@_ >= 11 and @_ <= 14) or croak 'usage: GenLdscript->new( NAME, PROFILE-NAME, LW-ROOT, OVERLAYSIMEM-FILE, OVERLAYSDMEM-FILE, VERBOSE, ELF-IN-PLACE, ELF-IN-PLACE-ODP-COW, UPROC-ARCH, DMEM_OVL_VARS_LONG[, DMEM_END_CARVEOUT_SIZE, DMESG_BUFFER_SIZE, IDENTITY_MAPPING] )';

    my ($class, $name, $profile, $lwroot, $opt_overlaysimem_file, $opt_overlaysdmem_file,
        $verbose, $elf_in_place, $elf_in_place_odp_cow, $arch, $dmem_ovl_vars_long,
        $dmem_end_carveout_size, $dmesg_buffer_size, $identity_mapping) = @_;

    $arch = uc $arch;

    if ($arch eq "FALCON") {
        return GenLdscriptFalcon->new($name, $profile, $lwroot, $opt_overlaysimem_file, $opt_overlaysdmem_file, $verbose, $dmem_ovl_vars_long);
    }
    elsif ($arch eq "RISCV") {
        if (! defined $dmem_end_carveout_size) {
            $dmem_end_carveout_size = 0;
            $class->warning("No DMEM-end carveout size specified, setting carveout size to 0!");
        }
        if (! defined $dmesg_buffer_size) {
            $dmesg_buffer_size = $dmem_end_carveout_size;
            $class->warning("No DMESG-buffer size specified, matching to DMEM-end carveout size!");
        }
        if (! defined $identity_mapping) {
            # Can be silent.
            $identity_mapping = 0;
        }

        return GenLdscriptRiscv->new($name, $profile, $lwroot, $dmem_end_carveout_size, $dmesg_buffer_size,
                                     $verbose, $elf_in_place, $elf_in_place_odp_cow, $identity_mapping);
    }
    else {
        croak 'UPROC-ARCH must be FALCON or RISCV';
    }
}

1;
