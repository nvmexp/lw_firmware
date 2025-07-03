#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

# Note: This file is included in makeroms.inc

# Add TU102 FPGA ROMs
internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll_dp-mst_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpgasddr3nopll.zbx

internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll.rom
internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll_dldvi.rom
internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll_dldvi_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll_dp-mst.rom
internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll_dualsst.rom
internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll_dualsst_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll_head0_tmds.rom
internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll_head0_tmds_no_ifr.rom