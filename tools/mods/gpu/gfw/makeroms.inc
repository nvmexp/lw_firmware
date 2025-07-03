#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

# Note: This file is included in makesrc.inc

internal_release_files           += sim/resources/gm20b_sim_sddr3.rom
internal_release_files           += sim/resources/gp10b_sim_sddr3.rom
internal_release_files           += sim/resources/t210_sim_sddr3.rom
internal_release_files           += sim/resources/t210_sim_sddr3_nogpcpll.rom
internal_release_files           += sim/resources/t210_sim_sddr3_coproc_nodisp_pstates.rom
internal_release_files           += sim/resources/tegra_simsddr3-coproc-nodisp.zbx
internal_release_files           += sim/resources/t186_sim_sddr3.rom
internal_release_files           += sim/resources/t186_sim_sddr3_nogpcpll.rom
internal_release_files           += sim/resources/t186_sim_sddr3_coproc_nodisp_pstates.rom

internal_release_files$(ONLY_PC) += sim/resources/gv100_emu_hbm.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_emu_hbm_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_emu_hbm_dcb-ultimate.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_emu_hbm_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_emu_hbm_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_emu_hbm_ecc.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_emu_hbm_ecc_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_emu_hbm_p-states.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_emu_hbm_dualrank.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_emu_hbm_dualrank_ecc.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_emu_hbm_dualrank_ecc_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_emuhbm.zbx

internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_dcb-ultimate.rom
basic_release_files$(ONLY_PC)    += sim/resources/gv100_sim_hbm_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_ecc.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_ecc_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_nodisplay_0fb.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_p-states.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_hbm1.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_hbm1_ecc.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_dualrank.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_dualrank_ecc.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100_simhbm.zbx

# Please Note - Unlike GP000 we need a ZBX file for GV000
internal_release_files$(ONLY_PC) += sim/resources/gv000_sim_hbm.rom
internal_release_files$(ONLY_PC) += sim/resources/gv000_sim_hbm_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/gv000_sim_hbm_ecc.rom
internal_release_files$(ONLY_PC) += sim/resources/gv000_sim_hbm_ecc_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/gv000_simhbm.zbx

internal_release_files$(ONLY_PC) += sim/resources/gv100f_fpga_sddr3_nopll.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100f_fpga_sddr3_nopll_dldvi.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100f_fpga_sddr3_nopll_dualsst.rom
internal_release_files$(ONLY_PC) += sim/resources/gv100f_fpgasddr3nopll.zbx

# Add GV11B
internal_release_files$(ONLY_PC) += sim/resources/gv11b_sim_0fb.rom

internal_release_files$(ONLY_PC) += sim/resources/gv000_sim_gddr5.rom

# Add G000 ROMs
include $(MODS_DIR)/gpu/gfw/g000.inc

# Add TU117 ROMs
include $(MODS_DIR)/gpu/gfw/tu117.inc

# Add TU102 FPGA ROMs
include $(MODS_DIR)/gpu/gfw/tu102f.inc

# Add TU104 ROMs
include $(MODS_DIR)/gpu/gfw/tu104.inc

# Add TU116 ROMs
include $(MODS_DIR)/gpu/gfw/tu116.inc

# Add TU106 ROMs
include $(MODS_DIR)/gpu/gfw/tu106.inc

# Add TU102 ROMs
include $(MODS_DIR)/gpu/gfw/tu102.inc

# Add GA100 ROMs
include $(MODS_DIR)/gpu/gfw/ga100.inc

# Add GA102 ROMs
include $(MODS_DIR)/gpu/gfw/ga102.inc

# Add GA103 ROMs
include $(MODS_DIR)/gpu/gfw/ga103.inc

# Add GA104 ROMs
include $(MODS_DIR)/gpu/gfw/ga104.inc

# Add GA106 ROMs
include $(MODS_DIR)/gpu/gfw/ga106.inc

# Add GA107 ROMs
include $(MODS_DIR)/gpu/gfw/ga107.inc

# Add GH100 ROMs
include $(MODS_DIR)/gpu/gfw/gh100.inc

# Add GA100f ROMs
include $(MODS_DIR)/gpu/gfw/ga102f.inc

# Add GA10B ROMs
include $(MODS_DIR)/gpu/gfw/ga10b.inc

# Add GH202 ROMs
include $(MODS_DIR)/gpu/gfw/gh202.inc

# Add GA10F ROMs
include $(MODS_DIR)/gpu/gfw/ga10f.inc

# Add AD102 ROMs
include $(MODS_DIR)/gpu/gfw/ad102.inc
