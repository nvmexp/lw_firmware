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

# Add GA103 ROMs
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_cmos.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_cmos_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_cmos_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_cmos_nodisplay_fmodel_no_meminit_delays.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_cmos_nodisplay_fmodel_no_meminit_delays_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_cmos_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_cmos_p-states.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_cmos_p-states_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_cmos_p-states_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_cmos_p-states_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_dcb-ultimate.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_dcb-ultimate_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_ecc-fmodel_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_ecc-fmodel_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_ecc.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_ecc_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_ecc_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_ecc_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_p-states.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_p-states_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_p-states_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_p-states_nodisplay_no_ifr.rom

internal_release_files$(ONLY_PC) += sim/resources/ga103_simgddr6.zbx


internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_dcb-ultimate.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_dcb-ultimate_g6_emu_16gb.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_dcb-ultimate_g6_emu_16gb_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_dcb-ultimate_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_ecc.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_ecc_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_ecc_nodisplay_g6_emu_16gb.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_ecc_nodisplay_g6_emu_16gb_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_ecc_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_ecc_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_g6_emu_16gb.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_g6_emu_16gb_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_nodisplay_g6_emu_16gb.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_nodisplay_g6_emu_16gb_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_dcb-ultimate.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_dcb-ultimate_g6_emu_16gb.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_dcb-ultimate_g6_emu_16gb_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_dcb-ultimate_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_g6_emu_16gb.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_g6_emu_16gb_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emugddr6.zbx
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6_cmos_p-states.frts

internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_cmos_nodisplay_fmodel_no_meminit_delays.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_cmos_nodisplay_fmodel_no_meminit_delays_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_cmos_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_dcb-ultimate.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_dcb-ultimate_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_ecc-fmodel_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_ecc-fmodel_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_ecc.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_ecc_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_cmos.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_ecc_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_cmos_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_ecc_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_cmos_nodisplay.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_sim_gddr6x_nodisplay_no_ifr.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_simgddr6x.zbx

internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_dcb-ultimate-qt.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_dcb-ultimate-qt_mem-16gb.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_ecc_nodisplay_mem-16gb.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_mem-16gb.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_nodisplay_mem-16gb.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_dcb-ultimate-qt.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_dcb-ultimate-qt_mem-16gb.rom
internal_release_files$(ONLY_PC) += sim/resources/ga103_emu_gddr6_p-states_mem-16gb.rom
