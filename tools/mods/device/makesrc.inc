#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#


#-----------------------------------------------------------------------
#common file for cheetah mods and gpu mods of device directory
#-----------------------------------------------------------------------

include_device_common=false


ifeq "$(INCLUDE_COMMON_TEGRA)" "true"
    include_device_common=true
endif

ifneq "$(INCLUDE_COMMON_TEGRA)" "true"
    ifeq "$(base_os)" "linux"
        include_device_common=true
    endif
    ifeq "$(BUILD_OS)" "macosxmfg"
        include_device_common=true
    endif
endif


ifeq "$(include_device_common)" "true"
    ifneq "$(INCLUDE_PEATRANS)" "true"
        cpp_files += device/utility/memtrk.cpp
    endif
    cpp_files += device/utility/memtrkif.cpp
endif

ifeq "$(include_azalia_support)" "true"
    defines += -DINCLUDE_AZALIA

    cpp_files += device/azalia/azabdl.cpp
    cpp_files += device/azalia/azacdc.cpp
    cpp_files += device/azalia/azactrl.cpp
    cpp_files += device/azalia/azadmae.cpp
    cpp_files += device/azalia/azafmt.cpp
    cpp_files += device/azalia/azaif.cpp
    cpp_files += device/azalia/azactrlmgr.cpp
    cpp_files += device/azalia/azart.cpp
    cpp_files += device/azalia/azautil.cpp
    cpp_files += device/azalia/tests/azarbtm.cpp
    cpp_files += device/azalia/azastrm.cpp
    cpp_files += device/azalia/tests/azasttm.cpp
    cpp_files += device/azalia/tests/azatest.cpp

    cpp_files += device/xhci/xusbif.cpp
    cpp_files += device/xhci/xusbctrl.cpp
    cpp_files += device/xhci/xusbctrlmgr.cpp
    cpp_files += device/xhci/xhcicomn.cpp
    cpp_files += device/xhci/xhcictrl.cpp
    cpp_files += device/xhci/usbdext.cpp
    cpp_files += device/xhci/xhcicntx.cpp
    cpp_files += device/xhci/xhciring.cpp
    cpp_files += device/xhci/xhcitrb.cpp
    cpp_files += device/xhci/xhciif.cpp
    cpp_files += device/xhci/xhcictrlmgr.cpp
    cpp_files += device/xhci/tests/xhcitst.cpp

    cpp_files += device/utility/bufmgr.cpp
    cpp_files += device/common/bufbuild.cpp
    cpp_files += device/common/gdtable.cpp
endif

include_azalia_and_smb_common=false
ifeq "$(include_smb_support)" "true"
    include_azalia_and_smb_common=true
endif
ifeq "$(include_azalia_support)" "true"
    include_azalia_and_smb_common=true
endif

ifeq "$(include_azalia_and_smb_common)" "true"
    cpp_files += device/common/mcpdev.cpp
    cpp_files += device/common/mcpmgr.cpp
endif

#-----------------------------------------------------------------------

ifneq "$(INCLUDE_COMMON_TEGRA)" "true"
    linux_or_macosxmfg = false
    ifeq "$(base_os)" "linux"
        linux_or_macosxmfg = true
    endif
    ifeq "$(BUILD_OS)" "macosxmfg"
        linux_or_macosxmfg = true
    endif

    ifeq "$(linux_or_macosxmfg)" "true"
        cpp_files += device/azalia/alsabdl.cpp
        cpp_files += device/azalia/alsastrm.cpp
        cpp_files += device/azalia/tests/alsasttm.cpp
    endif

    ifeq "$(include_smb_support)" "true"
        cpp_files += device/smb/smbdev.cpp
        cpp_files += device/smb/smbmask.cpp
        cpp_files += device/smb/smbmgr.cpp
        cpp_files += device/smb/smbif.cpp
        cpp_files += device/smb/lwsmbpt.cpp
        cpp_files += device/smb/intsmbpt.cpp
        cpp_files += device/smb/amdsmbpt.cpp
    endif

    internal_release_files += device/azalia/tests/aza.js

    ifneq (,$(filter $(BUILD_OS), sim winsim))
        internal_release_files += device/azalia/tests/azasimsi.js
    endif
else
    #------------------------------------------------------------------------
    #files of device directory which only cheetah mods use
    #------------------------------------------------------------------------

    cpp_files += device/azalia/azapreinit.cpp
    cpp_files += device/xhci/t210xhcipreinit.cpp
    cpp_files += device/xhci/t186xhcipreinit.cpp
    cpp_files += device/xhci/xhcipreinit.cpp
    cpp_files += device/xhci/xusbpadctrl.cpp
    cpp_files += device/xhci/t194xusbpadctrl.cpp
    cpp_files += device/xhci/t186xusbpadctrl.cpp
    cpp_files += device/xhci/t210xusbpadctrl.cpp
    basic_release_files += device/xhci/xusb_prod_fw
    basic_release_files += device/xhci/xusb_sil_prod_fw
    basic_release_files += device/xhci/xusb_sil_rel_fw
    basic_release_files += device/xhci/xusb_sil_rel_fw_x
    basic_release_files += device/xhci/xusb_sil_prod_fw_2.ifr.secure
    tegra_release_files += device/xhci/tests/js/xhcicomn.js
    tegra_release_files += device/xhci/tests/js/xhci.js

    internal_release_files += device/azalia/tests/aza.js
    tegra_release_files += device/xhci/tests/js/xusb.js

    cpp_files              += device/azalia/tests/alsasttmstub.cpp
endif

include_dirs$(ONLY_DGPU) += ../sensors/
cpp_files$(ONLY_DGPU) += device/irthermometer/extech42570.cpp
cpp_files$(ONLY_DGPU) += device/meter/colorimetryresearch.cpp

cpp_files$(ONLY_PC)   += device/sensors/nbsensors.cpp
cpp_files$(ONLY_PC)   += device/sensors/js_nbsensors.cpp

cpp_files += device/testfixture/gp100testfixture.cpp

cpp_files += device/interface/uphyif.cpp
ifeq "$(INCLUDE_LWLINK)" "true"
    cpp_files += device/interface/lwlink.cpp
    cpp_files += device/interface/lwlink/lwlpowerstate.cpp
    cpp_files += device/interface/lwlink/lwlcci.cpp
    cpp_files += device/interface/lwlink/lwlthroughputcounters.cpp
    cpp_files += device/interface/lwlink/lwliobist.cpp
    cpp_files += device/interface/lwlink/lwltrex.cpp
endif

cpp_files += device/interface/pcie.cpp
cpp_files += device/interface/c2c.cpp

# JS interfaces for GPUs and switches rely on test device infrastructure
# which is only present with INCLUDE_GPU=true
ifeq "$(INCLUDE_GPU)" "true"
    cpp_files += device/interface/js/js_c2c.cpp
    cpp_files += device/interface/js/js_pcie.cpp
    cpp_files += device/interface/js/js_gpio.cpp
    cpp_files += device/interface/js/js_i2c.cpp
    ifeq "$(INCLUDE_XUSB)" "true"
        cpp_files += device/interface/js/js_xusbhostctrl.cpp
        cpp_files += device/interface/js/js_portpolicyctrl.cpp
    endif
    ifeq "$(INCLUDE_LWLINK)" "true"
        cpp_files += device/interface/js/js_lwlink.cpp
    endif
endif


ifeq "$(INCLUDE_GPU)" "true"
    cpp_files += device/c2c/gh100c2cdiagdriver.cpp
endif
cpp_files += device/c2c/th500c2cdiagdriver.cpp
cpp_files += device/c2c/c2cdiagdriver.cpp
cpp_files += device/c2c/gh100c2cdiagregs.cpp


