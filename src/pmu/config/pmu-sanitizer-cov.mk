
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

LW_INCLUDES += $(SANITIZERCOV_SRC)/inc

# inform support library of desired number of elements in DMEM buffer
ifndef SANITIZER_COV_BUF_NUM_ELEMENTS
    SANITIZER_COV_BUF_NUM_ELEMENTS := 4094
endif

# SanitizerCoverage compiler flags

ifndef SANITIZER_COV_INSTRUMENT_CFLAGS
    SANITIZER_COV_INSTRUMENT_CFLAGS :=
    SANITIZER_COV_INSTRUMENT_CFLAGS += -fsanitize-coverage=trace-pc
    SANITIZER_COV_INSTRUMENT_CFLAGS += -fsanitize-coverage=trace-cmp
endif

ifndef SANITIZER_COV_EXCLUDE_CFLAGS
    SANITIZER_COV_EXCLUDE_CFLAGS :=
    SANITIZER_COV_EXCLUDE_CFLAGS += -fno-sanitize-coverage=trace-pc
    SANITIZER_COV_EXCLUDE_CFLAGS += -fno-sanitize-coverage=trace-cmp
endif

CFLAGS += $(SANITIZER_COV_INSTRUMENT_CFLAGS)
CFLAGS += -DSANITIZER_COV_INSTRUMENT=1

# File patterns to exclude from SanitizerCoverage

ifndef SANITIZER_COV_EXCLUDE_FILES
    SANITIZER_COV_EXCLUDE_FILES :=

    # exclude SanitizerCoverage-related RPCs
    SANITIZER_COV_EXCLUDE_FILES += cmdmgmt/lw/cmdmgmt_sanitizer_cov.c

    # exclude shared RPC machinery
    SANITIZER_COV_EXCLUDE_FILES += _out/%.c
    SANITIZER_COV_EXCLUDE_FILES += cmdmgmt/lw/cmdmgmt_dispatch.c
    SANITIZER_COV_EXCLUDE_FILES += cmdmgmt/lw/cmdmgmt_queue.c
    SANITIZER_COV_EXCLUDE_FILES += cmdmgmt/lw/cmdmgmt_queue_dmem.c
    SANITIZER_COV_EXCLUDE_FILES += cmdmgmt/lw/cmdmgmt_queue_fb.c
    SANITIZER_COV_EXCLUDE_FILES += cmdmgmt/lw/cmdmgmt_queue_fb_heap.c
    SANITIZER_COV_EXCLUDE_FILES += cmdmgmt/lw/cmdmgmt_queue_fb_ptcb.c
    SANITIZER_COV_EXCLUDE_FILES += cmdmgmt/lw/cmdmgmt_queue_fbflcn.c
    SANITIZER_COV_EXCLUDE_FILES += cmdmgmt/lw/cmdmgmt_queue_fsp.c
    SANITIZER_COV_EXCLUDE_FILES += cmdmgmt/lw/cmdmgmt_rpc.c
    SANITIZER_COV_EXCLUDE_FILES += os/lw/pmu_%.c
    SANITIZER_COV_EXCLUDE_FILES += pmu/lw/pmu_riscv.c
    SANITIZER_COV_EXCLUDE_FILES += task_cmdmgmt.c

    # exclude OS callback implementations
    SANITIZER_COV_EXCLUDE_FILES += lpwr/lw/ei_callback.c
    SANITIZER_COV_EXCLUDE_FILES += lpwr/lw/lpwr_callback.c
    SANITIZER_COV_EXCLUDE_FILES += os/lw/pmu_ostmrcallback.c
endif

define SANITIZER_COV_ADD_FILE_CFLAGS
$$(OUTPUTDIR)/$(basename $(1)).o: CFLAGS += $(2)
endef

$(foreach pat, $(SANITIZER_COV_EXCLUDE_FILES), \
    $(eval $(call SANITIZER_COV_ADD_FILE_CFLAGS, \
                  $(strip $(pat)), $(SANITIZER_COV_EXCLUDE_CFLAGS))))

# File patterns to exlucde from trace-pc
# (leaving others like trace-cmp enabled)

SANITIZER_COV_EXCLUDE_TRACE_PC_FILES :=

# WAR: prevent STACK_LPWR from blowing up
# max stack usage call path:
#   task_lpwr
#   pgLogicAll
#   pgLogicStateMachine
#   s_grRgExit
#   grGrRgExit_GA10X
#   perfDaemonChangeSeqLpwrScriptExelwte
#   perfDaemonChangeSeqScriptExelwteStep_CLK_RESTORE
#   clkGrRgRestore
#   s_clkLpwrClkInitDeinit
#   clkAdcPowerOnAndEnable
#   clkAdcDeviceHwRegisterAccessSync
#   clkAdcsVoltageReadWrapper
#   pgCtrlDisallowExt
#   pgCtrlDisallow
#   pgCtrlEventSend
#   appTaskCriticalEnter_FUNC-COMPACTED

SANITIZER_COV_EXCLUDE_TRACE_PC_FILES += pg/lw/pmu_pglogicsm.c
SANITIZER_COV_EXCLUDE_TRACE_PC_FILES += gr/lw/pmu_objgr.c
SANITIZER_COV_EXCLUDE_TRACE_PC_FILES += gr/ampere/pmu_grga10x.c
SANITIZER_COV_EXCLUDE_TRACE_PC_FILES += perf/lw/changeseq_lpwr_daemon.c
SANITIZER_COV_EXCLUDE_TRACE_PC_FILES += clk/lw/pmu_clklpwr.c
SANITIZER_COV_EXCLUDE_TRACE_PC_FILES += clk/lw/pmu_clkadc.c
SANITIZER_COV_EXCLUDE_TRACE_PC_FILES += clk/lw/pmu_clkadc_extended.c
SANITIZER_COV_EXCLUDE_TRACE_PC_FILES += pg/lw/pmu_objpgctrl.c

$(foreach pat, $(SANITIZER_COV_EXCLUDE_TRACE_PC_FILES), \
    $(eval $(call SANITIZER_COV_ADD_FILE_CFLAGS, \
                  $(strip $(pat)), -fno-sanitize-coverage=trace-pc)))

export SANITIZER_COV_EXCLUDE_FILES
export SANITIZER_COV_BUF_NUM_ELEMENTS