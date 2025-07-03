#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

#-------------------------------------------------------------------------------
#
# Target platform: Windows Mfg
#
#-------------------------------------------------------------------------------

# Set up lwapi for winmfg and win32
ifneq (,$(filter $(BUILD_OS), winmfg win32))
    target_lib_dirs += $(DRIVERS_DIR)/lwapi/sdk/_out/Windows/lwsdk/$(BUILD_ARCH)
    include_dirs += $(DRIVERS_DIR)/lwapi/sdk/_out/Windows/lwsdk/$(BUILD_ARCH)
    ifeq "$(BUILD_ARCH)" "x86"
        extra_libs += lwapi.lib
        cpp_files += win32/vsnwprintf.cpp
        CLIENTPATHS += //sw/tools/win32/msvc71/bin/...
        CLIENTPATHS += //sw/tools/win32/msvc71/include/...
        CLIENTPATHS += //sw/tools/win32/msvc71/PlatformSDK/Include/...
    endif
    ifeq "$(BUILD_ARCH)" "amd64"
        extra_libs += lwapi64.lib
        cpp_files += win32/vsnwprintf.cpp
        CLIENTPATHS += //sw/tools/ddk/nt7/7600.1/bin/x86/amd64/...
        CLIENTPATHS += //sw/tools/ddk/nt7/7600.1/inc/api/...
        CLIENTPATHS += //sw/tools/ddk/nt7/7600.1/inc/crt/...
    endif
    CLIENTPATHS += //sw/tools/win32/ActivePerl/5.10.0.1004/...
    CLIENTPATHS += //sw/tools/win32/gnumake/...
    CLIENTPATHS += //sw/tools/win32/MiscBuildTools/lwmake.exe
    CLIENTPATHS += //sw/$(BRANCH)/drivers/lwapi/...
endif

ifeq "$(BUILD_OS)" "winmfg"
    cpp_files += winmfg/xp.cpp
    $(o_dir)/xp$(obj_suffix): submake.lwapi.build_all

    ifeq "$(dll_build)" "false"
        LDFLAGS += /INCREMENTAL:NO
    endif
endif # BUILD_OS == winmfg

#-------------------------------------------------------------------------------
#
# Target platform: MacOSX
#
#-------------------------------------------------------------------------------
ifeq "$(base_os)" "macosx"
    defines += -DMACOS -DMACOSX

    cpp_files += macosx/restarter_mac.cpp

    ifeq "$(use_mods_rm)" "true"
        # use the libmods if using the mods built-in RM.
        cpp_files += macosx/xp_hw_mac.cpp
        c_files += macosx/libmods.c
        cpp_files += macosx/linxrawc.cpp
    else
        ifeq "$(INCLUDE_GPU)" "true"
            # build the user mode RM API if not using the mods built-in RM
            cpp_files += macosx/ndrv.cpp
            cpp_files += macosx/macrawc.cpp
            cpp_files += $(DRIVERS_DIR)/resman/arch/lwalloc/mods/src/rmmacosx.cpp
            c_files   += $(DRIVERS_DIR)/resman/arch/lwalloc/macos/macosX/rmapi.c

            resman_name = LWDAresmanAPI

            defines += -DLW_USERMODE_API

            include_dirs += $(DRIVERS_DIR)/display/macosx/LWDAndrv/shared
            include_dirs += $(DRIVERS_DIR)/common/mac/inc
            include_dirs += $(DRIVERS_DIR)/resman/arch/lwalloc/macos/include/macosX

            #remove sdk/lib/libLWDAresmanAPI.a, a we build our own rmapi.c

            ifneq "$(REMOTE_DEBUG_PORT)" "0"
                defines           += -DREMOTE_DEBUG_PORT=$(REMOTE_DEBUG_PORT)
                libraries         += gdb
            endif # REMOTE_DEBUG_PORT
        else
            cpp_files += macosx/linxrawc.cpp
        endif
    endif

    cpp_files += core/ui/remotesink.cpp
    cpp_files += core/ui/serlstub.cpp

    ifeq "$(USE_NEW_TASKER)" "true"
        cpp_files += macosx/posixthread.cpp
    else
        cpp_files += macosx/ostasker_mac.cpp
    endif

    cpp_files += macosx/xp_mac.cpp
    cpp_files += linux/xp_posix.cpp

    ifeq "$(INCLUDE_GPU)" "true"
        cpp_files += linux/linxmodc.cpp
        internal_release_files += tools/simgen.sh
    endif

    LDFLAGS += -framework Carbon -framework IOKit
endif

ifeq "$(base_os)" "linux"
    ifeq "$(ENABLE_CCOVER)" "true"
        LD = /home/tools/ccover/ccover-7.3.11/bin/covc $(LD)
    endif

    ifneq "$(use_pthreads)" "true"
        s_files += linux/threadsw.S
    endif
    cpp_files += core/ui/remotesink.cpp

    ifeq "$(USE_NEW_TASKER)" "true"
        cpp_files += linux/posixthread.cpp
    else
        cpp_files += linux/ostasker.cpp
    endif
    cpp_files += linux/xp.cpp
    cpp_files += linux/xp_posix.cpp
    ifeq "$(BUILD_OS_SUBTYPE)" "qnx"
        cpp_files += qnx/qnxserl.cpp
    else
        cpp_files += linux/linxserl.cpp
    endif
    cpp_files += linux/linxrawc.cpp

    ifeq "$(INCLUDE_GPU)" "true"
        cpp_files += linux/linxmodc.cpp
    endif

    ifneq "$(DEBUG_TRACE_LEVEL)" "0"
        cpp_files += linux/stackdbg.cpp
        LDFLAGS += -rdynamic
    endif

    ifeq "$(BUILD_OS)" "linuxmfg"
        LEAK_DETECTOR_SUPPORTED := true
        cpp_files += linux/xp_hw.cpp
        cpp_files += linux/xp_mods_kd.cpp

        ifeq "$(INCLUDE_XUSB)" "true"
            cpp_files += linux/xp_hw_usb.cpp
        endif

        # For USB temperature sensor
        cpp_files += core/usbtherm.cpp
        cpp_files += ../pcsensor/pcsensor.cpp
        include_dirs += ../pcsensor/

        ifeq "$(INCLUDE_DIRECTFB)" "true"
            ifeq "$(INCLUDE_GPU)" "true"
                cpp_files += linux/directfbmgr.cpp
                cpp_files += linux/foreign_display.cpp
            endif
        endif

        ifneq "$(INCLUDE_PEATRANS)" "true"
            cpp_files += linux/restarter.cpp
        else
             cpp_files += linux/restarter_sim.cpp
        endif
    endif

    ifeq "$(BUILD_OS)" "linux"
        cpp_files += linux/xp_kernel_resman.cpp
        cpp_files += linux/restarter.cpp
    endif

    ifeq "$(BUILD_OS)" "cheetah"
        ifeq "$(BUILD_OS_SUBTYPE)" "qnx"
            cpp_files += qnx/xp_qnx.cpp
            cpp_files += qnx/xp_mods_kd_stub.cpp
        else
            cpp_files += android/xp_android.cpp
            cpp_files += linux/xp_mods_kd.cpp
            cpp_files += android/restarter_and.cpp
        endif
    endif

    ifeq "$(BUILD_OS)" "sim"
        cpp_files += linux/xp_sim.cpp
        cpp_files += linux/restarter_sim.cpp
        ifeq (,$(filter $(BUILD_ARCH),amd64 ppc64le))
            internal_release_files += sim/$(BUILD_ARCH)/hwchip.so
            internal_release_files += sim/$(BUILD_ARCH)/shim.so
        endif
        internal_release_files += sim/$(BUILD_ARCH)/hwchip2.so
        internal_release_files += sim/$(BUILD_ARCH)/sockchip.so
        ifneq (,$(filter $(BUILD_ARCH), amd64))
            internal_release_files += sim/$(BUILD_ARCH)/hwchip2_emusys.so
            internal_release_files += sim/$(BUILD_ARCH)/peatrans.so
            internal_release_files += sim/$(BUILD_ARCH)/libvpcie.so
            internal_release_files += sim/$(BUILD_ARCH)/vpciechip.so
        endif
        internal_release_files += sim/amd64/sockserv64
        exelwtable_files       += sim/amd64/sockserv64
        internal_release_files += sim/x86/sockserv32
        exelwtable_files       += sim/x86/sockserv32
        ifeq "$(BUILD_ARCH)" "amd64"
            ifeq "$(toolchain_rev)" "7.3.0"
                basic_release_files += sim/amd64/amd64_gcc930/libstdc++.so.6
                exelwtable_files    += sim/amd64/amd64_gcc930/libstdc++.so.6
                basic_release_files += sim/amd64/amd64_gcc930/libstdc++.so.6.0.28
                exelwtable_files    += sim/amd64/amd64_gcc930/libstdc++.so.6.0.28
            endif
        endif
    endif
endif

# $(INCLUDE_LINUX_RMAPI_LIB) - Build linux rmapi library into MODS
INCLUDE_LINUX_RMAPI_LIB = false

ifeq "$(BUILD_OS)" "linux"
    ifeq "$(INCLUDE_GPU)" "true"
        ifneq "$(use_mods_rm)" "true"
            cpp_files += $(DRIVERS_DIR)/resman/arch/lwalloc/mods/src/rmlinux.cpp
            ifeq "$(INCLUDE_MINIRM)" "true"
                ifeq "$(BUILD_CFG)" "debug"
                    libraries += rmapi_debug
                else
                    libraries += rmapi
                endif
            else
                INCLUDE_LINUX_RMAPI_LIB = true
            endif
        endif
    endif
endif

ifeq "$(BUILD_OS)" "cheetah"
    ifeq "$(INCLUDE_GPU)" "true"
        cpp_files += $(DRIVERS_DIR)/resman/arch/lwalloc/mods/src/rmlinux.cpp
        INCLUDE_LINUX_RMAPI_LIB = true
    endif
endif

ifeq "$(INCLUDE_LINUX_RMAPI_LIB)" "true"
    # linux rmapi support
    RMAPI_ROOT              = $(shell cd $(DRIVERS_DIR)/..; /bin/pwd)
    RMAPI_OUTPUTDIR         = $(o_dir)
    RMAPI_PLATFORM          = $(base_os)

    # rmapi.mk may trigger an rmconfig run, so setup for that
    RMCFG_BUILD_OS          = $(base_os)
    RMCFG_TARGET_PLATFORM  ?= mods_$(RMCFG_BUILD_OS)
    RMCFG_TARGET_ARCH      := $(BUILD_ARCH)
    RMCFG_PROFILE          ?= devel-gpus-mods
    RMCFG_OUTPUTDIR         = $(o_dir)

    RMCFG_VERBOSE           = quiet

    # disable most ONLY_IF processing of Sources.def for mods builds since
    # mods is not size sensitive and some of the windows compilers don't
    # fully elide code from within 'if (0) { }' code blocks.
    RMCFG_INTERNAL_OPTIONS += --no-trim-srcfiles

    -include $(DRIVERS_DIR)/resman/arch/lwalloc/unix/lib/rmapi.mk

    # update MODS build vars to include RMAPI (and possibly XAPI) support
    # NOTE: add RMAPI_INCLUDES to 'include_dirs' later to maintain correct ordering
    c_files        += $(RMAPI_SOURCES)
    defines        += $(addprefix -D,$(RMAPI_DEFINES))
    ifeq "$(MODS_KERNEL_RM_VER_CHK)" "true"
        defines += -DMODS_KERNEL_RM_VER_CHK
    endif

    # add $(RMAPI_INCLUDES) to include_dirs *only* for rmapi compiles
    # otherwise there are too many conflicts with mods includes and headers
    # with the same name
    rmapiLeaves     = $(notdir $(RMAPI_SOURCES))
    rmapiObjects    = $(rmapiLeaves:%.c=$(o_dir)/%$(obj_suffix))
$(rmapiObjects): include_dirs += $(RMAPI_INCLUDES)

    CLEAN_ME       += $(RMAPI_CLEAN)
endif

ifeq "$(use_lwrses)" "true"
    ifeq "$(base_os)" "linux"
        cpp_files += linux/linxncrc.cpp
    endif
    ifeq "$(base_os)" "macosx"
        cpp_files += macosx/macnlwrc.cpp
    endif
    cpp_files += core/ui/nlwrcnsl.cpp
endif

ifeq "$(use_pthreads)" "true"
    # Android supports POSIX threads, but not libpthread.so
    ifeq (,$(filter $(BUILD_OS_SUBTYPE), bionic qnx))
        libraries += pthread
    endif
endif

ifeq "$(use_softfloat)" "true"
    SOFTFLOAT_DIR = $(DRIVERS_DIR)/common/softfloat
    -include $(SOFTFLOAT_DIR)/lwpu/softfloatsrc.inc
    c_files += $(SOFTFLOAT_SOURCES)
    include_dirs += $(SOFTFLOAT_DIR)/lwpu
    include_dirs += $(SOFTFLOAT_DIR)/source/include
    include_dirs += $(SOFTFLOAT_DIR)/source/8086-SSE
    defines += -DSOFTFLOAT_ROUND_ODD=1
    defines += -DSOFTFLOAT_FAST_DIV32TO16=1
    defines += -DSOFTFLOAT_FAST_DIV64TO32=1
endif

ifeq "$(base_os)" "win32"
    defines += -DREGEX_MALLOC -DSTDC_HEADERS -DHAVE_CONFIG_H
    LDFLAGS += /NODEFAULTLIB:libc.lib
    ifneq "$(BUILD_CFG)" "debug"
        LDFLAGS += /NODEFAULTLIB:libcmtd.lib
    else
        LDFLAGS += /NODEFAULTLIB:libcmt.lib
    endif

    extra_libs += ws2_32.lib crypt32.lib selwr32.lib gdi32.lib user32.lib advapi32.lib shell32.lib psapi.lib

    defines += -D_WIN32_WINNT=0x0600

    cpp_files += win32/restarter_win.cpp
    cpp_files += core/ui/remotesink.cpp
    cpp_files += core/ui/winserl.cpp
    ifeq "$(USE_NEW_TASKER)" "true"
        cpp_files += win32/winthread.cpp
    else
        cpp_files += win32/ostasker.cpp
    endif
    cpp_files += win32/sockpipe.cpp

    ifneq "$(DEBUG_TRACE_LEVEL)" "0"
        cpp_files += win32/stackdbg.cpp
        extra_libs += dbghelp.lib
    endif

    cpp_files += win32/winrawc.cpp

    ifeq "$(INCLUDE_GPU)" "true"
        cpp_files += win32/winmodsc.cpp
    endif

    ifneq "$(BUILD_OS)" "winmfg"
        cpp_files += win32/winfmtc.cpp
        cpp_files += win32/xp_winnt.cpp
    endif

    cpp_files += win32/xp_common.cpp

    ifeq "$(BUILD_OS)" "winsim"
        ifeq "$(BUILD_ARCH)" "amd64"
            internal_release_files += winsim/$(BUILD_ARCH)_msvc90/sockchip.dll
        else
            internal_release_files += winsim/$(BUILD_ARCH)/sockchip.dll
        endif
        ifeq "$(dll_build)" "false"
            LDFLAGS += /INCREMENTAL:NO
        endif
        internal_release_files += winsim/amd64_msvc90/sockserv64.exe
        internal_release_files += winsim/x86/sockserv32.exe
    endif
endif

ifeq "$(BUILD_OS)" "win32"
    target_lib_dirs += $(SDK_DIR)/lwpu/lib/$(BUILD_ARCH)
    CLIENTPATHS += //sw/tools/sdk/DirectX_Mar2009/...
    ifeq "$(BUILD_ARCH)" "x86"
        target_lib_dirs += $(BUILD_TOOLS_DIR)/sdk/DirectX_Mar2009/Lib/x86
    endif
    ifeq "$(BUILD_ARCH)" "amd64"
        target_lib_dirs += $(BUILD_TOOLS_DIR)/sdk/DirectX_Mar2009/Lib/x64
    endif

    extra_libs += kernel32.lib
    extra_libs += d3d9.lib
    extra_libs += D3dx9.lib

    ifeq "$(INCLUDE_GPU)" "true"
        cpp_files += $(DRIVERS_DIR)/resman/arch/lwalloc/mods/src/rmwin.cpp
    endif

    RMAPI_USE_ESCAPE_AND_IOCTL := true
    RMAPI_LW_SOURCE := $(DRIVERS_DIR)/..
    -include $(DRIVERS_DIR)/resman/interface/windows/rmapi/rmapi-common.mk
    c_files += $(RMAPI_SOURCES)

endif

ifeq "$(sim_build)" "true"
    cpp_files += sim/simpg32.cpp
    cpp_files += sim/simpdpolicy.cpp
    cpp_files += sim/substitute_registers.cpp
    cpp_files += sim/simpltfm.cpp
    cpp_files += sim/mheapsim.cpp
    cpp_files += sim/simcallmutex.cpp
    cpp_files += sim/shim.cpp
    cpp_files += sim/ifspec3_shim_mods.cpp

    cpp_files += sim/chiplibtrace/capture/chiplibtracecapture.cpp
    cpp_files += sim/chiplibtrace/replayer/chiplibtrace.cpp
    cpp_files += sim/chiplibtrace/replayer/replayer.cpp

    ifeq "$(BUILD_OS_SUBTYPE)" "WH"
        LDFLAGS += -u __ctype_b /home/tools/vcs/vcs7.2R12/linux/lib/ctype-stubs_32.a
    endif
else
    ifeq "$(da_build)" "true"
        cpp_files += directamodel/dafakerm.cpp
        cpp_files += directamodel/dapltfm.cpp
        cpp_files += directamodel/xp_da.cpp
    else
        cpp_files += core/platform/hwpltfm.cpp
    endif
endif

#-------------------------------------------------------------------------------
# Core Files
#-------------------------------------------------------------------------------

# core files which cheetah and mods use
ifeq "$(include_azalia_support)" "true"
    cpp_files += core/utility/patterns.cpp
endif

# Unconditionally include core files
cpp_files += fieldiag.cpp
ifeq "$(BUILD_ARCH)" "amd64"
    ifeq "$(toolchain)" "msvc"
        cpp_files += core/cpu/amd64cpu.cpp
    else
        cpp_files += core/cpu/amd64_gcc_cpu.cpp
    endif
else
    cpp_files += core/cpu/$(BUILD_ARCH)cpu.cpp
endif
cpp_files += core/cpu/cpu.cpp
cpp_files += core/device/device.cpp
cpp_files += core/device/devmgr.cpp
cpp_files += core/device/mgrmgr.cpp
cpp_files += core/error/errormap.cpp
cpp_files += core/error/rx.cpp
cpp_files += core/filetype/png.cpp
$(o_dir)/png$(obj_suffix): include_dirs += $(BUILD_TOOLS_DIR)/lpng1251 core/include
cpp_files += core/filetype/readimg.cpp
cpp_files += core/filetype/tga.cpp
cpp_files += core/filetype/yuv.cpp
cpp_files += core/filetype/rgbe.cpp
cpp_files += core/filetype/hdr.cpp
cpp_files += core/main/cmdline.cpp
cpp_files += core/main/argread.cpp
cpp_files += core/main/argdb.cpp
cpp_files += core/main/argpproc.cpp
cpp_files += core/main/js_param.cpp
cpp_files += core/main/globals.cpp
cpp_files += core/main/help.cpp
cpp_files += core/main/jscript.cpp
ifneq "$(BOUND_JS)" "false"
$(o_dir)/jscript$(obj_suffix): include_dirs += $(gen_cpp_dir)
$(o_dir)/jscript$(obj_suffix): $(gen_cpp_dir)/boundjs.h
endif
cpp_files += core/main/log.cpp
cpp_files += core/main/massert.cpp
cpp_files += core/main/modsdrv.cpp  # Relies on stubs if INCLUDE_GPU=false
ifneq "$(INCLUDE_MODS_GTEST)" "true"
    cpp_files += core/main/modsmain.cpp
endif
cpp_files += core/main/onetime.cpp
cpp_files += core/main/out.cpp
cpp_files += core/main/rc.cpp
cpp_files += core/main/script.cpp
ifeq "$(USE_NEW_TASKER)" "true"
    cpp_files += core/main/taskerm.cpp
    cpp_files += core/main/thread_mill.cpp
else
    cpp_files += core/main/tasker.cpp   # Relies on stubs if INCLUDE_GPU=false
endif
cpp_files += core/main/jstasker.cpp
cpp_files += core/main/abort.cpp
cpp_files += core/platform/pci.cpp
cpp_files += core/platform/pcicfg_saver.cpp
cpp_files += core/platform/platform.cpp
cpp_files += core/platform/xpjs.cpp
cpp_files += core/signature/signature.cpp
cpp_files += core/tests/modstest.cpp
cpp_files += core/tests/testconf.cpp
cpp_files += core/tests/thread.cpp
cpp_files += core/tests/selftest.cpp
cpp_files += core/ui/circbuf.cpp
cpp_files += core/ui/assertinfosink.cpp
cpp_files += core/ui/debuglogsink.cpp
cpp_files += core/ui/bscrsink.cpp
cpp_files += core/ui/circsink.cpp
cpp_files += core/ui/stdoutsink.cpp
cpp_files += core/ui/cmdhstry.cpp
cpp_files += core/ui/datasink.cpp
cpp_files += core/ui/dbgsink.cpp
cpp_files += core/ui/filesink.cpp
cpp_files += core/ui/ethrsink.cpp
cpp_files += core/ui/remotui.cpp
cpp_files += core/ui/cnslmgr.cpp
cpp_files += core/ui/cnslbuf.cpp
cpp_files += core/ui/rawcnsl.cpp
cpp_files += core/ui/nullcnsl.cpp
cpp_files += core/ui/outcwrap.cpp
cpp_files += core/ui/runmacro.cpp
cpp_files += core/ui/sersink.cpp
cpp_files += core/ui/simpleui.cpp
cpp_files += core/ui/tabcompl.cpp
cpp_files += core/ui/sockansi.cpp
cpp_files += core/ui/sockmods.cpp
cpp_files += core/main/system.cpp
cpp_files += core/ui/tee.cpp
ifeq "$(USE_NEW_TASKER)" "true"
    cpp_files += core/ui/teeq.cpp
else
    cpp_files += core/ui/teeqstub.cpp
endif
cpp_files += core/ui/telnutil.cpp
cpp_files += core/unittests/unittest.cpp
ifeq "$(INCLUDE_UNITTEST)" "true"
    cpp_files += core/unittests/unit_cirbuffer.cpp
    cpp_files += core/unittests/unit_color.cpp
    cpp_files += core/unittests/unit_crc.cpp
    cpp_files += core/unittests/unit_memcpy.cpp
    cpp_files += core/unittests/unit_rc.cpp
    cpp_files += core/unittests/unit_bytestream.cpp
    cpp_files += core/unittests/unit_rmhandles.cpp
    cpp_files += core/unittests/unit_random.cpp
    ifeq "$(USE_NEW_TASKER)" "true"
        cpp_files += core/unittests/unit_tasker.cpp
    endif
    cpp_files += core/unittests/unit_utility.cpp
    cpp_files += core/unittests/unit_interactive_file.cpp
endif
cpp_files += core/ui/js_mle.cpp
cpp_files += core/utility/callback.cpp
cpp_files += core/utility/color.cpp
cpp_files += core/utility/crc.cpp
cpp_files += core/utility/data.cpp
cpp_files += core/utility/deprecat.cpp
cpp_files += core/utility/dllhelper.cpp
cpp_files += core/utility/errcount.cpp
cpp_files += core/utility/crypto.cpp
cpp_files += core/utility/errloggr.cpp
cpp_files += core/utility/evntthrd.cpp
cpp_files += core/utility/jsonlog.cpp
cpp_files += core/utility/jsonparser.cpp
cpp_files += core/utility/interactive_file.cpp
cpp_files += core/utility/js_interactive_file.cpp
cpp_files += core/utility/lwrm_handles.cpp
cpp_files += core/utility/bitrot.cpp
cpp_files += core/utility/fft.cpp
cpp_files += core/utility/fpicker.cpp
cpp_files += core/utility/fsaacomn.cpp
cpp_files += core/utility/ptrnclss.cpp
cpp_files += core/utility/fuse.cpp
cpp_files += core/utility/fuseutil.cpp
cpp_files += core/utility/fusecache.cpp
cpp_files += core/utility/fuseparser.cpp
cpp_files += core/utility/fusexml_parser.cpp
cpp_files += core/utility/fusejsonparser.cpp

cpp_files += core/utility/hbm_model.cpp
cpp_files += core/utility/heartbeatmonitor.cpp
cpp_files += core/tests/xmlparsetest.cpp
cpp_files += core/utility/oven.cpp

cpp_files += core/utility/js_ismspeedo.cpp
cpp_files += core/utility/ism.cpp
cpp_files += core/utility/heap.cpp
cpp_files += core/utility/mheap.cpp
cpp_files += core/utility/hp3632.cpp
cpp_files += core/utility/interrpt.cpp
cpp_files += core/utility/js_rnd.cpp
cpp_files += core/utility/preprocess.cpp
cpp_files += core/utility/memerror.cpp # Relies on stubs
cpp_files += core/utility/memfrag.cpp
cpp_files += core/utility/memlane.cpp
cpp_files += core/utility/memory.cpp   # Relies on stubs if INCLUDE_GPU=false
cpp_files += core/utility/memtypes.cpp
cpp_files += core/utility/mods_profile.cpp
ifeq "$(INCLUDE_LWSWITCH)" "true"
    cpp_files += core/utility/mods_gdm_client.cpp
    cpp_files += core/utility/mods_gdm_handlers.cpp
    cpp_files += $(gen_cpp_dir)/gdm_message_handler.cpp
else
    cpp_files += core/utility/mods_gdm_client_stub.cpp
endif
cpp_files += core/utility/obfuscate_string.cpp
cpp_files += core/utility/pio.cpp
cpp_files += core/utility/pool.cpp
cpp_files += core/utility/refparse.cpp
cpp_files += core/utility/regexp.cpp
cpp_files += core/utility/registry.cpp
cpp_files += core/utility/sharedsysmem.cpp
cpp_files += core/utility/simplejs.cpp
cpp_files += core/utility/sockjs.cpp
cpp_files += core/utility/trep.cpp
cpp_files += core/utility/utility.cpp # This needs to be reorganized, relies on stubs
cpp_files += core/utility/tar.cpp
cpp_files += core/utility/tario2.cpp
cpp_files += core/utility/simclk.cpp
cpp_files += core/utility/security.cpp

cpp_files += core/utility/coverage.cpp

cpp_files += core/utility/gcmonitor.cpp

cpp_files += core/utility/js_uint64.cpp

cpp_files += core/utility/lz4.cpp
$(o_dir)/lz4$(obj_suffix): include_dirs += $(LZ4_DIR)
CLIENTPATHS += $(LZ4_DIR_P4)/...

c_files += core/utility/ioapi.c
c_files += core/utility/unzip.c
cpp_files += $(VERSION_CPP)

cpp_files += core/device/featstr.cpp

# sha-256
c_files += $(DRIVERS_DIR)/common/src/lwSha256.c

#--------------------------------------------------------------------------------------
# Stub section
#--------------------------------------------------------------------------------------

ifeq "$(INCLUDE_COMMON_TEGRA)" "false"
    ifneq "$(BUILD_OS)" "win32"
        cpp_files += stub/sysgpio_stub.cpp
        cpp_files += stub/cpuutil_stub.cpp
    else
        ifeq (,$(filter $(BUILD_ARCH), armv7 aarch64))
            cpp_files += stub/sysgpio_stub.cpp
        endif
        cpp_files += stub/cpuutil_stub.cpp
    endif
endif

ifeq "$(sim_build)" "false"
    cpp_files += stub/chiplibtracecapture_stub.cpp
endif

ifeq "$(INCLUDE_GPU)" "false"
    cpp_files += stub/bascstub.cpp
    cpp_files += stub/gpu_stub.cpp
    cpp_files += stub/rm_stub.cpp
endif # INCLUDE_GPU (false)

ifeq "$(SERVER_MODE)" "true"
    cpp_files += win32/server.cpp
    internal_release_files += gpu/js/dx.js
endif

ifeq "$(use_static_lwwatch)" "true"
    cpp_files += gpu/utils/lwwatch.cpp
    internal_release_files += tools/lwwatch.gdb
endif

ifneq (,$(filter $(BUILD_ARCH), armv7 aarch64))
    cpp_files += cheetah/comn/tegrasystem.cpp
endif
ifeq "$(BUILD_ARCH)" "amd64"
    ifeq "$(INCLUDE_PEATRANS)" "true"
        cpp_files += cheetah/comn/tegrasystem.cpp
    else
        cpp_files += device/common/x86system.cpp
    endif
endif
ifeq "$(BUILD_ARCH)" "ppc64le"
    cpp_files += device/common/x86system.cpp
endif

ifneq "$(include_smb_support)" "true"
    cpp_files += stub/smb_stub.cpp
    ifeq "$(INCLUDE_GPU)" "true"
        cpp_files += stub/smbec_stub.cpp
    endif
endif

ifeq "$(INCLUDE_GPU)" "false"
    ifneq "($INCLUDE_PEATRANS)" "true"
        cpp_files += stub/dispstub.cpp
    endif
endif # INCLUDE_GPU (false)



#-------------------------------------------------------------------------------
#
# Lists of runtime files for inclusion in the .zip for release.
#
#-------------------------------------------------------------------------------
basic_release_files += $(o_dir)/$(exe_name)
basic_release_files += docs/licenses.txt
basic_release_files += core/js/arghndlr.js
basic_release_files += core/js/comnargs.js
basic_release_files += core/js/comnmods.js
basic_release_files += core/js/comnprnt.js
basic_release_files += core/js/comntest.js
basic_release_files += core/js/comnutil.js
basic_release_files += core/js/jsthread.js
basic_release_files += gpu/js/mods.js
basic_release_files += core/js/testlist.js
basic_release_files += core/js/fileid.js
basic_release_files += core/js/intrutil.js
basic_release_files += core/js/mods.h
basic_release_files += core/js/prntutil.js
basic_release_files += core/js/tofile.js
basic_release_files += core/js/json2.js

ifeq "$(dll_build)" "true"
    ifeq "$(use_mods_rm)" "true"
        dll_release_files += $(GEN_LIB_DIR)/lib$(resman_name)/lib$(resman_name)$(dll_suffix)
    endif
    ifeq "$(INCLUDE_DRVEXPORT)" "true"
        dll_release_files += $(GEN_LIB_DIR)/libdrvexport/libdrvexport$(dll_suffix)
    endif
    ifeq "$(INCLUDE_LWOGTEST)" "true"
        dll_release_files += $(GEN_LIB_DIR)/liblwogtest/liblwogtest$(dll_suffix)
    endif
    ifeq "$(INCLUDE_OGL)" "true"
        dll_release_files += $(GEN_LIB_DIR)/libmodsgl/libmodsgl$(dll_suffix)
    endif
    ifeq "$(INCLUDE_GPU)" "true"
        dll_release_files += $(GEN_LIB_DIR)/libamap/libamap$(dll_suffix)
        dll_release_files += $(GEN_LIB_DIR)/libdramhal/libdramhal$(dll_suffix)
    endif
endif

ifeq "$(use_mods_rm)" "true"
    ifeq "$(BUILD_CFG)" "release"
        internal_release_files += $(o_dir)/../librm_gen/fileids.txt
    endif
endif

ifeq "$(INCLUDE_VULKAN)" "true"
    dll_release_files += $(LWMAKE_DRV_OUT_DIR)/drivers/compiler/spirv/build/mods/libspirv$(dll_suffix)
    dll_release_files += $(LWMAKE_DRV_OUT_DIR)/drivers/rtcore/librtcore$(dll_suffix)
endif

internal_release_files += core/js/goldguts.js
internal_release_files += core/js/memleak.js
basic_release_files    += core/js/shmclass.js
internal_release_files += core/js/telnetport.js
internal_release_files += core/js/fxmltest.js

ifeq "$(INCLUDE_UNITTEST)" "true"
    internal_release_files += core/js/unittest.js
endif

ifeq "$(sim_build)" "true"
    internal_release_files += sim/gdbwrap.pl
    exelwtable_files       += sim/gdbwrap.pl
    internal_release_files += sim/sim.pl
    internal_release_files += sim/python_suppressions.supp
    internal_release_files += sim/javascript_suppressions.supp
    internal_release_files += sim/chiplibtrace/postprocess/process_trace.pl
    internal_release_files += sim/chiplibtrace/postprocess/LwMan.pm
    internal_release_files += sim/logger/modslogger.pl
    internal_release_files += sim/logger/ModsLogger.pm
    internal_release_files += sim/logger/MLParser.pm
    internal_release_files += sim/logger/SimpleTree.pm
    exelwtable_files       += sim/sim.pl
    exelwtable_files       += sim/chiplibtrace/postprocess/process_trace.pl
    exelwtable_files       += sim/chiplibtrace/postprocess/LwMan.pm
    exelwtable_files       += sim/logger/modslogger.pl
    internal_release_files += sim/sim_amodel_args.pl
    exelwtable_files       += sim/sim_amodel_args.pl
    internal_release_files += sim/generate_skiprange_substitutereg_file.pl
    exelwtable_files       += sim/generate_skiprange_substitutereg_file.pl
endif

ifeq "$(LEAK_DETECTOR_SUPPORTED)" "true"
    ifeq "$(INCLUDE_LEAK_DETECTOR)" "true"
        ifeq "$(BUILD_CFG)" "debug"
            defines += -DPRINT_LEAK_REPORT
            internal_release_files += tools/leak-detector.pl
            internal_release_files += tools/Utils.pm
        endif
    endif
endif

ifneq (,$(filter $(BUILD_OS), sim winsim))
    internal_release_files += sim/hw_tree_test.config
endif

ifeq "$(BUILD_OS_SUBTYPE)" "bionic"
    pvs_release_files += tools/install_android.sh
    exelwtable_files  += tools/install_android.sh
endif

ifeq "$(BUILD_OS)" "sim"
    pvs_release_files += tools/dvsfmodel.py
    exelwtable_files  += tools/dvsfmodel.py
endif

ifneq "$(sim_build)" "true"
    basic_release_files += $(SHARED_FILES_DIR)/HQLWCA121-CA.crt
    CLIENTPATHS += //sw/mods/shared_files/...
endif

ifeq "$(INCLUDE_MODS_GTEST)" "true"
    ifeq "$(base_os)" "linux"
        defines += -DGTEST_OS_LINUX -DMODS_GTEST
    else
        $(error GTEST build is not yet supported for base_os $(base_os))
    endif

    include_dirs +=  $(GTEST_DIR)/googlemock/include
    include_dirs +=  $(GTEST_DIR)/googletest/include

    libraries += gcov
    PREBUILT_LIB_DIR = $(shell cd -P $(base_mods_lib_dir) && pwd)
    extra_libs += $(PREBUILT_LIB_DIR)/libgmock.a
    extra_libs += $(PREBUILT_LIB_DIR)/libgtest.a
    extra_libs += $(PREBUILT_LIB_DIR)/libgmock_main.a
    extra_libs += $(PREBUILT_LIB_DIR)/libgtest_main.a

    include_dirs += gpu/display/dpc
    cpp_files += unittests/gtest/dpc/dpc_file_parser_ut.cpp
    cpp_files += unittests/gtest/dpc/dpc_configurator_ut.cpp
    cpp_files += unittests/gtest/dpc/dpc_config_gen_ut.cpp
    cpp_files += unittests/gtest/idt/idt_ut.cpp
    cpp_files += unittests/gtest/iot/iot_parser_ut.cpp
    cpp_files += unittests/gtest/lwdr/lwdrresourceallocator_ut.cpp
    cpp_files += unittests/gtest/mods/core/include/bitfield_ut.cpp
    cpp_files += unittests/gtest/mods/core/include/hbmlane_ut.cpp
    cpp_files += unittests/gtest/mods/core/include/hbmtypes_ut.cpp
    cpp_files += unittests/gtest/mods/core/include/memlane_ut.cpp
    cpp_files += unittests/gtest/mods/core/include/memtypes_ut.cpp
    cpp_files += unittests/gtest/mods/core/include/mucc_ut.cpp
    cpp_files += unittests/gtest/mods/core/include/trie_ut.cpp
    cpp_files += unittests/gtest/mods/core/utility/char_utility_ut.cpp
    cpp_files += unittests/gtest/mods/core/utility/strong_typedef_helper_ut.cpp
    cpp_files += unittests/gtest/mods/core/utility/utility_ut.cpp
    cpp_files += unittests/gtest/mods/gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_xdie_interface_ut.cpp
    cpp_files += unittests/gtest/mods/gpu/repair/hbm/hbm_interface/vendor/skhynix/skhynix_hbm_interface_ut.cpp
    cpp_files += unittests/gtest/mods/gpu/repair/repair_util_ut.cpp
    cpp_files += unittests/gtest/mods/gpu/fuse/downbin/downbinelementpickers_ut.cpp
    cpp_files += unittests/gtest/mods/gpu/fuse/downbin/downbingrouppickers_ut.cpp
    cpp_files += unittests/gtest/mods/gpu/fuse/downbin/downbinrulesimpl_ut.cpp
    cpp_files += unittests/gtest/mods/gpu/fuse/downbin/fselement_ut.cpp
    cpp_files += unittests/gtest/mods/gpu/fuse/downbin/fsmask_ut.cpp
    cpp_files += unittests/gtest/mods/gpu/fuse/downbin/fsset_ut.cpp
    cpp_files += unittests/gtest/mods/gpu/fuse/downbin/ga10xdownbinimpl_ut.cpp
    cpp_files += unittests/gtest/mods/gpu/fuse/encoding/hopperiffencoder_ut.cpp
    install_files += $(wildcard $(MODS_DIR)/../mucc/unittests/*.mucc $(MODS_DIR)/../mucc/unittests/*.json)

    # Unittested header file only classes
    unit_tested_cpp_files += gpu/display/dpc/dpc_file_parser.cpp
    unit_tested_cpp_files += gpu/display/dpc/dpc_configurator.cpp
    unit_tested_cpp_files += gpu/display/dpc/dpc_config_gen.cpp
    unit_tested_cpp_files += gpu/tests/idt/idt.cpp
    unit_tested_cpp_files += gpu/tests/lwdisplayrandomv2/lwdisplayrandom_v2.cpp
    unit_tested_cpp_files += unittests/gtest/mods/core/include/bitfield_ut.cpp
    unit_tested_cpp_files += unittests/gtest/mods/core/include/hbmlane_ut.cpp
    unit_tested_cpp_files += unittests/gtest/mods/core/include/memtypes_ut.cpp
    unit_tested_cpp_files += unittests/gtest/mods/core/include/trie_ut.cpp
    unit_tested_cpp_files += unittests/gtest/mods/core/utility/char_utility_ut.cpp
    unit_tested_cpp_files += unittests/gtest/mods/core/utility/strong_typedef_helper_ut.cpp
    unit_tested_cpp_files += unittests/gtest/mods/gpu/fuse/downbin/downbinelementpickers_ut.cpp
    unit_tested_cpp_files += unittests/gtest/mods/gpu/fuse/downbin/downbingrouppickers_ut.cpp
    unit_tested_cpp_files += unittests/gtest/mods/gpu/fuse/downbin/downbinrulesimpl_ut.cpp
    unit_tested_cpp_files += unittests/gtest/mods/gpu/fuse/downbin/fsmask_ut.cpp
    unit_tested_cpp_files += unittests/gtest/mods/gpu/fuse/downbin/fselement_ut.cpp

    $(addprefix $(o_dir)/,$(notdir $(unit_tested_cpp_files:.cpp=$(obj_suffix)))): CFLAGS += --coverage
    coverage_gcno_files = $(addprefix $(o_dir)/, $(notdir $(unit_tested_cpp_files:.cpp=.gcno)))

    internal_release_files += unittests/gtest/ut_on_dvs.sh
endif

ifeq "$(INCLUDE_TRACY)" "true"
    cpp_files += $(TRACY_DIR)/TracyClient.cpp
    $(o_dir)/TracyClient$(obj_suffix): CFLAGS += -Wno-error -D__STDC_FORMAT_MACROS
endif
