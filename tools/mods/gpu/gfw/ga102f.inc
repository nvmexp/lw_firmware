#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

# Note: This file is included in makeroms.inc

# Add GA102f ROMs
internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll.rom
internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dldvi.rom
internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dualsst.rom
internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dp-mst.rom
internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpgasddr3nopll.zbx

internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dcb41.rom
internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dcb41_no_ifr.rom

internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dcb41_dispmacro.rom
internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dcb41_dispmacro_no_ifr.rom

internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dcb41_2head1or_dualsst.rom
internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dcb41_2head1or_dualsst_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dcb41_2head1or_mst.rom
internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dcb41_2head1or_mst_no_ifr.rom
