/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwgpu.h"


#include "class/cl9067.h" // FERMI_CONTEXT_SHARE_A
#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "class/cl9097.h" // GF100_FERMI
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h" // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h" // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h" // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h" // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h" // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc637.h"
#include "class/clc638.h"
#include "class/clc86f.h" // HOPPER_CHANNEL_GPFIFO_A

#include "core/include/cmdline.h"
#include "core/include/display.h"
#include "core/include/framebuf.h"
#include "core/include/gpu.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/pool.h"
#include "core/include/rc.h"
#include "core/include/refparse.h"
#include "core/include/utility.h"
#include "core/utility/errloggr.h"
#include "ctrl/ctrl0080.h"
#include "device/interface/pcie.h"

#include "fermi/gf100/dev_bus.h"
#include "fermi/gf100/dev_master.h"
#include "fermi/gf100/dev_timer.h"

#include "gpu/display/dpc/dpc_configurator.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/perf/pmusub.h"
#include "gpu/reghal/smcreghal.h"
#include "gpu/utility/blocklin.h"
#include "gpu/utility/surf2d.h"

#include "kepler/gk110/dev_ltc.h"

#include "mdiag/advschd/policymn.h"
#include "mdiag/gpu/surfmgr.h"
#include "mdiag/smc/gpupartition.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/smc/smcresourcecontroller.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/tests/gpu/lwgpu_tsg.h"
#include "mdiag/tests/gpu/trace_3d/tracesubchan.h"
#include "mdiag/tests/test.h"
#include "mdiag/utils/buffinfo.h"
#include "mdiag/utils/cregctrl.h"
#include "mdiag/utils/mdiag_xml.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "mdiag/utils/perf_mon.h"
#include "mdiag/utils/sharedsurfacecontroller.h"
#include "mdiag/utils/utils.h"
#include "mdiag/utl/utl.h"

#include "Lwcm.h"
#include "lwos.h"
#include "pascal/gp100/dev_fifo.h" // LW_PFIFO_CFG0
#include <regex>

DEFINE_MSG_GROUP(Mdiag, Gpu, TestScheduler, false)
#define MSGID() __MSGID__(Mdiag, Gpu, TestScheduler)

namespace
{
    // Tracks the GPU to which the FLA export surface belongs
    map<string, LWGpuResource*> s_FlaExportGpuMap;
}

const string LWGpuResource::FLA_VA_SPACE = "FlaVaSpace";

TestDirectory* LWGpuResource::s_pTestDirectory = nullptr;
vector<LWGpuResource*> LWGpuResource::s_LWGpuResources;

const char *AAstrings[] = {
    "-1X1",
    "obsolete",
    "obsolete",
    "-2X1",
    "obsolete",
    "-2X2",
    "-4X2",
    "-2X2_VC_4",
    "-2X2_VC_12",
    "-4X2_VC_8",
    "obsolete",
    "obsolete",
    "obsolete",
    "obsolete",
    "obsolete",
    "obsolete",
    "-4X4",
    NULL
};

const ParamDecl LWGpuResource::ParamList[] =
{
    PARAM_SUBDECL(lwgpu_single_params),
    SIMPLE_PARAM("-hwctxsw", "use hardware context switching"),
    SIMPLE_PARAM("-swapctxsw", "swap between hybrid hw/sw and hardware context switching"),
    SIMPLE_PARAM("-ctxswLog", "output context switching info into the log file"),
    SIMPLE_PARAM("-ctxswSerial", "serialize the context switching of the reg and ram state"),
    SIMPLE_PARAM("-ctxswSingleStep", "single step the ctxsw ucode"),
    SIMPLE_PARAM("-ext_tag_en", "Enables extended tags"),

    { "-ctxswType", "u", (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 10, "ctxsw type" },
    { "-ctxswTypeHalt", "0", ParamDecl::GROUP_MEMBER, 0, 0, "Halt style ctxsw" },
    { "-ctxswTypeWFI",  "1", ParamDecl::GROUP_MEMBER, 0, 0, "Wait For Idle type ctxsw" },
    { "-ctxswTypeSM",   "2", ParamDecl::GROUP_MEMBER, 0, 0, "Old state machine ctxsw" },
    { "-ctxswTypeSPILLREPLAYONLY",   "3", ParamDecl::GROUP_MEMBER, 0, 0, "Old state machine ctxsw" },

    UNSIGNED_PARAM("-ctxswDebug", "set ctxsw debug type number"),
    SIMPLE_PARAM("-ctxswSingleStep", "single step the context switch ucode"),

    {"-priv", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Send a block of private options(BAR0 based). Format: -priv +begin <reg_space> -reg <action>:<reg>:<data>[:optional_mask] -reg ... +end"},
    {"-conf", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Send a block of config options(Config space access via BAR0 prior to Hopper, performs config cycle from Hopper onwards. Format: -conf +begin gpu -reg <action>:<reg>:<data>[:optional_mask] -reg ... +end"},

    {"-reg", "t",  ParamDecl::PARAM_MULTI_OK, 0, 0,"Optional access for registers. Format:  -reg <action>:<reg>:<data>[:optional_mask]"},
    {"-reg_eos", "t",  ParamDecl::PARAM_MULTI_OK, 0, 0,"Optional access for pri registers at the end of simulation."},
    SIMPLE_PARAM("-assert_upon_bad_pri_config", "assert in case of processing -reg failed"),
    SIMPLE_PARAM("-disable_assert_upon_bad_pri_config", "disable the assert in case of processing -reg failed"),

    SIMPLE_PARAM("-no_gpu", "init primary device (assume no POST)"),
    SIMPLE_PARAM("-multi_gpu", "init for multiple GPUs"),
    SIMPLE_PARAM("-ctxswitch", "do channel switches (overrides -nochsw)"),
    SIMPLE_PARAM("-ctxswTimeSlice", "let HOST context switch between channels"),
    SIMPLE_PARAM("-sync_trace_method_send", "sync multiple trace3d threads before and after sending trace methods, polling GET==PUT to promise all methods are fetched."),
    SIMPLE_PARAM("-disable_timer", "Disable PTIMER to ensure deterministic results for some tests"),

    { "-scramblingMode",    "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                                  ParamDecl::GROUP_START),   0,  1,  "0=off, 1=on" },
    { "-scramblingDisable", "0", ParamDecl::GROUP_MEMBER,   0,  0,  "disable framebuffer scrambling" },
    { "-scramblingEnable",  "1", ParamDecl::GROUP_MEMBER,   0,  0,  "enable framebuffer scrambling" },

    // XXX This is still used -- see http://lwbugs/200094421
    SIMPLE_PARAM("-handleIntr", "This is the LEGACY way to handle HW interrupts.  You probably want to use -skip_recovery"),
    SIMPLE_PARAM("-skip_intr_check", "Do NOT check the HW interrupts that oclwrred against an 'expected' file.  Only use this option if you know what you're doing..."),

    SIMPLE_PARAM("-channel_logging", "Turn on logging of method data on channel flushes"),

    STRING_PARAM("-vpm_command", "takes a string specifying what vpm command to run"),
    UNSIGNED_PARAM("-vpm_stop", "stop emulator at specified state number"),
    STRING_PARAM("-vpm_control", "path to file specifying list of states to dump"),
    SIMPLE_PARAM("-vpm_dump_all","dump all states"),
    UNSIGNED_PARAM("-vpm_pm_min_capture", "capture pm signals for state buckets above minimum size: default 40000"),
    SIMPLE_PARAM("-start_vpm_capture", "Start VPM capture with START_OF_TEST_SET for LW_PPM_GLOBAL_CNTRL (only when HWPM enabled)"),
    SIMPLE_PARAM("-pm_zpass_pixel_cnt", "pm does NOT read zpass pixel count at test end"),
    UNSIGNED_PARAM("-pm_modec_memsize", "Allocate memory block with given <size> for PM mode c"),
    UNSIGNED_PARAM("-pm_modec_memsize_hostclk", "Allocate memory block with given <size> for domain host"),
    UNSIGNED_PARAM("-pm_modec_memsize_lw1clk", "Allocate memory block with given <size> for domain lw1"),
    UNSIGNED_PARAM("-pm_modec_memsize_lw2clk", "Allocate memory block with given <size> for domain lw2"),
    UNSIGNED_PARAM("-pm_modec_memsize_tclk", "Allocate memory block with given <size> for domain t"),
    UNSIGNED_PARAM("-pm_modec_memsize_mclk", "Allocate memory block with given <size> for domain m"),
    UNSIGNED_PARAM("-pm_modec_memsize_lw3clk", "Allocate memory block with given <size> for domain lw3"),
    UNSIGNED_PARAM("-pm_modec_memsize_vpclk", "Allocate memory block with given <size> for domain vpc"),
    UNSIGNED_PARAM("-pm_modec_memsize_lw4clk", "Allocate memory block with given <size> for domain lw4"),
    UNSIGNED_PARAM("-pm_modec_memsize_msdclk", "Allocate memory block with given <size> for domain msd"),

    UNSIGNED_PARAM("-fb_random_bank_swizzle", "random bank swizzle (0=default, 1=enable, 2=disable)"),

    STRING_PARAM("-disp_cfg_json", "Get display configuration for multiple heads from JSON file"),
    SIMPLE_PARAM("-disp_cfg_json_debug", "Enable debugging prints for the DPC/JSON parser"),
    SIMPLE_PARAM("-disp_cfg_json_ext_trigger_mode", "Allow external control of CRC capture start and end"),
    SIMPLE_PARAM("-disp_cfg_json_exelwte_at_end", "Exelwtes DPC at the end of Trace3D"),
    STRING_PARAM("-disp_cfg_json_dump_surfaces", "Dumps the surfaces in either png/tga/text"),
    SIMPLE_PARAM("-disp_cfg_json_dump_luts", "Dumps the used LUTs (ILUT, OLUT and TmoLUT)"),
    SIMPLE_PARAM("-disp_cfg_json_enable_prefetch", "Enable Prefetch"),
    STRING_PARAM("-disp_cfg", "Get display configuration from a file"),
    STRING_PARAM("-disp_cfg0", "Get display configuration for Head0 from a file"),
    STRING_PARAM("-disp_cfg1", "Get display configuration for Head1 from a file"),
    STRING_PARAM("-disp_cfg2", "Get display configuration for Head2 from a file"),
    STRING_PARAM("-disp_cfg3", "Get display configuration for Head3 from a file"),
    STRING_PARAM("-disp_cfg_imp", "Get IsModePossible configuration from a file"),
    SIMPLE_PARAM("-disp_single_head_mode", "Configure 1 head (defaults to 2) with disp_cfg arg"),
    SIMPLE_PARAM("-disp_skip_crc", "Don't Capture/Dump CRCs and perform no CRC comparison"
            "(only allowed on RTL where there is an assert to guard against underflow)"),
    SIMPLE_PARAM("-disp_skip_crc_test", "Capture/Dump CRCs but don't perform comparison"),
    SIMPLE_PARAM("-disp_perform_crc_count_check", "Compare the number of CRCs captured"),
    SIMPLE_PARAM("-disp_skip_head_detach", "Don't detach head after CRC generation"),
    SIMPLE_PARAM("-disp_poll_awake", "Poll to make sure at least one frame has been scanned out (ie display is in steady state)"),
    SIMPLE_PARAM("-disp_primary_core", "Use the core channel to display the primary surface"),
    {"-disp_reg_ovrd_pre",  "t",  ParamDecl::PARAM_MULTI_OK, 0, 0,
        "Register overrides after CrtcManager sends display methods, but before \"-disp_reg_ovrd\", the same format as '-reg' argument"},
    {"-disp_reg_ovrd",      "t",  ParamDecl::PARAM_MULTI_OK, 0, 0,
        "Register overrides after CrtcManager sends display methods, the same format as '-reg' argument"},
    {"-disp_reg_ovrd_post", "t",  ParamDecl::PARAM_MULTI_OK, 0, 0,
        "Register overrides after CrtcManager sends display methods, but after \"-disp_reg_ovrd\", the same format as '-reg' argument"},
    SIMPLE_PARAM("-disp_ignore_imp", "Ignore IsModePossible result from RM"),
    {"-disp_clk_switch", "uuu", 0, 0, 0, "Execute clock switch during scanout on head <x> at line <y> to clock value <z>"},
    SIMPLE_PARAM("-disp_raster_lock", "Enable raster lock between multiple heads"),
    SIMPLE_PARAM("-sync_scanout", "synchronize exelwtion of a trace with begining of display scanout"),
    UNSIGNED_PARAM("-pitch_disp_primary", "Use a pitched primary surface with the given pitch (pass 0 to use default pitch)"),
    UNSIGNED_PARAM("-block_width_disp_primary", "gobs in X dimension per block for primary display buffer (default is 1)"),
    UNSIGNED_PARAM("-block_height_disp_primary", "gobs in Y dimension per block for primary display buffer (default is 16)"),
    UNSIGNED_PARAM("-block_width_disp_overlay", "gobs in X dimension per block for overlay display buffer (default is 1)"),
    UNSIGNED_PARAM("-block_height_disp_overlay", "gobs in Y dimension per block for overlay display buffer (default is 16)"),

    UNSIGNED_PARAM("-compbit_backing_size", "Set size of compbit backing store: default is 0 (compbit cache not used), MAX is 16 MB; DEBUG ONLY: Set BIT(31) for RM to zero out compbit backing store by writing to FB"),
    UNSIGNED_PARAM("-fermi_big_page_size", "Set page size of Fermi big pages, 64kB (0x10000) or 128kB (0x20000); default is 64kB"),
    UNSIGNED_PARAM("-fermi_cbsize",    "Set Cirlwlar Buffer (CB) size (# of 32 byte chunks)"),

    SIMPLE_PARAM("-weak_skey",         "Tells RM to skip the Diffie-Hellman  key exchange.  Same as default"),
    SIMPLE_PARAM("-disable_display",   "Tells RM to never use display engine"),
    { "-timeout_ms", "f", (ParamDecl::ALIAS_START | ParamDecl::ALIAS_OVERRIDE_OK), 0, 0, "Amount of time in milliseconds to wait before declaring the test hung" },
    { "-hwWaitFail", "f", ParamDecl::ALIAS_MEMBER, 0, 0, "Amount of time in milliseconds to wait before declaring the test hung (Alias of -timeout_ms)" },

    STRING_PARAM("-dump_pri_group","specifies register group name to be dumped"),
    { "-dump_pri_range", "uu", ParamDecl::PARAM_MULTI_OK, 0, 0, "specifies range of registers to be dumped"},
    STRING_PARAM("-dump_pri_file","specifies file with groups and/or ranges to be dumped"),
    { "-dump_pri_range_no_manuals_check", "uu", ParamDecl::PARAM_MULTI_OK, 0, 0, "specifies range of registers to be dumped without checking against manuals (use this on your own risk)"},

    STRING_PARAM("-skip_pri_group","specifies register group name to be excluded from dumping"),
    { "-skip_pri_range", "uu", ParamDecl::PARAM_MULTI_OK, 0, 0, "specifies range of registers to be excluded from dumping"},
    STRING_PARAM("-skip_pri_file","specifies file with groups and/or ranges to be excluded from dumping"),

    STRING_PARAM("-dump_pri_file_to","specifies file name registers to be dumped into; regs.txt is default"),
    SIMPLE_PARAM("-pm_disable_info_log", "Disable PM info printout during simulation"),
    SIMPLE_PARAM("-pm_ncoh_no_snoop", "Temporary hack to disable snooping for Perf test on EMU/Silicon"),

    // why are these called alloc_vid, and not vid_alloc????????
    UNSIGNED_PARAM("-alloc_vid", "Allocates X bytes of video mem, calls EscapeWrite(\"Vid_Alloc_Addr\",0,8,<addr>)"),
    UNSIGNED_PARAM("-alloc_coh", "Allocates X bytes of coherent sysmem, calls EscapeWrite(\"CohMem_Alloc_Addr\",0,8,<addr>)"),
    UNSIGNED_PARAM("-alloc_ncoh", "Allocates X bytes of non-coherent sysmem, calls EscapeWrite(\"NCohMem_Alloc_Addr\",0,8,<addr>)"),
    MEMORY_SPACE_PARAMS("_alloc", "buffer"),
    UNSIGNED_PARAM("-page_size", "override the page size for all buffers (in KB)"),
    { "-ctx_dma_type", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                             ParamDecl::GROUP_START), PAGE_PHYSICAL, PAGE_VIRTUAL, "context DMA type for all buffers" },
    { "-phys_ctx_dma",    (const char *)PAGE_PHYSICAL, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
      0, 0, "use physical context DMA for all buffers" },
    { "-virtual_ctx_dma", (const char *)PAGE_VIRTUAL, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
      0, 0, "use virtual context DMA for all buffers" },
    SIMPLE_PARAM("-ctxdma_attrs", "put attributes in context DMA for all buffers"),
    SIMPLE_PARAM("-privileged", "allow only privileged access to all buffers"),
    { "-mem_model", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                           ParamDecl::GROUP_START), Memory::Paging, Memory::Paging, "memory model for all buffers" },
    { "-paging",       (const char *)Memory::Paging, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
      0, 0, "use paging memory model for all buffers" },
    SIMPLE_PARAM("-va_reverse", "allocate virtual address space from the top down for all buffers"),
    SIMPLE_PARAM("-pa_reverse", "allocate physical address space from the top down for all buffers"),
    UNSIGNED_PARAM("-max_coalesce", "maximum number of coalesced PTEs for all buffers"),
    SIMPLE_PARAM("-force_cont", "Force contiguous system memory allocation (could fail)"),

    SIMPLE_PARAM("-null_display", "don't send any display commands to HW."),

    SIMPLE_PARAM("-rm_break_on_rc_disable", "Disable RM breakpoints on RC"),
    SIMPLE_PARAM("-rm_rc_enable", "Enable robust channels, even for simulation"),

    { "-zbc_mode", "u",
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1,
        "ZBC compression mode for all buffers" },
    { "-zbc_off", "0", ParamDecl::GROUP_MEMBER, 0, 0,
        "don't use ZBC compression for all buffers" },
    { "-zbc_on", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "use ZBC compression for all buffers" },
    { "-zbc", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "use ZBC compression for all buffers" },
    SIMPLE_PARAM("-zbc_skip_ltc_setup", "Prevents MODS from writing to the L2 clear table registers when ZBC is active"),

    { "-phys_addr_range", "LL", 0, 0, 0,
      "desired GPU physical address range for all buffers" },

    { "-gpu_cache_mode", "u",
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1,
        "use or don't use GPU cache" },
    { "-gpu_cache_off", "0", ParamDecl::GROUP_MEMBER, 0, 0,
        "don't use GPU cache" },
    { "-gpu_cache_on", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "use GPU cache" },
    { "-sysmem_nolwolatile", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "use non-volatile memory" },

    { "-gpu_p2p_cache_mode", "u",
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1,
        "use or don't use GPU cache for peer memory" },
    { "-gpu_p2p_cache_off", "0", ParamDecl::GROUP_MEMBER, 0, 0,
        "don't use GPU cache for peer memory" },
    { "-gpu_p2p_cache_on", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "use GPU cache for peer memory" },

    { "-split_gpu_cache_mode", "u",
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1,
        "use or don't use GPU cache for split memory half" },
    { "-split_gpu_cache_off", "0", ParamDecl::GROUP_MEMBER, 0, 0,
        "don't use GPU cache for split memory half" },
    { "-split_gpu_cache_on", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "use GPU cache for split memory half" },

    STRING_PARAM("-pm_option", "Specifies the PM triggers to use" ),
    SIMPLE_PARAM("-pm_ilwalidate_cache", "Ilwalidate the L1 and L2 cache before a pm trigger"),
    SIMPLE_PARAM("-check_display_underflow", "Check for display underflow at the end of a PM experiment"),

    SIMPLE_PARAM("-disable_fspg_tpc", "disable FSPG feature for TPC"),
    SIMPLE_PARAM("-disable_fspg_MSDEC", "disable FSPG feature for MSDEC"),
    SIMPLE_PARAM("-disable_ce_pbchecker", "disable push buffer checker app for CE engine"),
    SIMPLE_PARAM("-pri_restore", "Restore PRI register values updated by '-reg W or M' command. Default is to not restore."
        " Will not work on silicon/emulator"),
    SIMPLE_PARAM("-no_pri_restore", "Do not restore the PRI register values updated by '-reg W or M' command. Default is to not restore."
        " Will not work on silicon/emulator"),
    SIMPLE_PARAM("-compress", "turn on compression for all memory regions"),

    { "-map_mode", "u",
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1,
        "Memory mapping mode for all buffers" },
    { "-map_direct", "0", ParamDecl::GROUP_MEMBER, 0, 0,
        "Use direct memory mapping for all buffers" },
    { "-map_reflected", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "Use reflected memory mapping for all buffers" },

    UNSIGNED_PARAM("-sysmem_size", "check that sysmem is at least n MB"),
    UNSIGNED_PARAM("-fb_minimum", "check that fb size is at least n MB"),

    SIMPLE_PARAM("-require_primary", "force the test to fail if the GPU is not in the primary slot"),
    SIMPLE_PARAM("-require_not_primary", "force the test to fail if the GPU is in the primary slot"),
    SIMPLE_PARAM("-require_quadro", "force the test to fail if not in lwdqro mode"),
    SIMPLE_PARAM("-require_msdec", "force the test to fail if msdec is diabled"),
    SIMPLE_PARAM("-require_ecc_fuse", "force the test to fail if the ECC fuse is disabled"),
    SIMPLE_PARAM("-require_no_ecc_fuse", "force the test to fail if the ECC fuse is enabled"),
    SIMPLE_PARAM("-require_fb_ecc", "force the test to fail if FB ECC is disabled"),
    SIMPLE_PARAM("-require_l2_ecc", "force the test to fail if L2 ECC is disabled"),
    SIMPLE_PARAM("-require_l1_ecc", "force the test to fail if L1 ECC is disabled"),
    SIMPLE_PARAM("-require_sm_ecc", "force the test to fail if SM ECC is disabled"),
    SIMPLE_PARAM("-require_no_fb_ecc", "force the test to fail if FB ECC is enabled"),
    SIMPLE_PARAM("-require_no_l2_ecc", "force the test to fail if L2 ECC is enabled"),
    SIMPLE_PARAM("-require_no_l1_ecc", "force the test to fail if L1 ECC is enabled"),
    SIMPLE_PARAM("-require_no_sm_ecc", "force the test to fail if SM ECC is enabled"),
    { "-valid_chipset", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "specify a valid chipset for the test; fail if the actual chipset doesn't match any valid chipsets" },
    UNSIGNED_PARAM("-require_revision", "require a specific revision of silicon"),
    { "-require_chip", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "specify a valid chip; fail if the actual chip doesn't match any of the valid chips" },
    STRING_PARAM("-require_ecid", "forces the test to fail if not run with the specified ECID"),
    STRING_PARAM("-require_ram_type", "forces the test to fail if not run on a board with specified RAM type"),
    SIMPLE_PARAM("-require_tex_parity_check", "force the test to fail if the TEX parity check is disabled"),
    SIMPLE_PARAM("-require_no_tex_parity_check", "force the test to fail if the TEX parity check is enabled"),
    SIMPLE_PARAM("-require_l2_parity", "force the test to fail if L2 parity is disabled"),
    SIMPLE_PARAM("-require_no_l2_parity", "force the test to fail if L2 parity is enabled"),
    SIMPLE_PARAM("-require_2xtex_fuse", "force the test to fail if the 2xtex fuse is not enabled"),

    UNSIGNED_PARAM("-alloc_chid_randseed0", "Random stream seed for random channel ID allocation"),
    UNSIGNED_PARAM("-alloc_chid_randseed1", "Random stream seed for random channel ID allocation"),
    UNSIGNED_PARAM("-alloc_chid_randseed2", "Random stream seed for random channel ID allocation"),

    {"-loc_inst_random", "uuu", 0, 0, 0, "Specify seed to generate random location. Syntax: -loc_inst_random <seed0> <seed1> <seed2>"},

    UNSIGNED_PARAM("-elpg_mask", "Mask for enabling ELPG"),

    { "-ats_address_space", "t", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "enables ATS for the specified address space" },

    SIMPLE_PARAM("-create_separate_address_space", "Alloc a non-zero vaspace for each trace_3d test. Format: -create_separate_address_space"),

    { "-set_pasid", "tu", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "sets the PASID of the specified address space to the specified value" },

    { "-zbc_color_table_entry", "tuuuu", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "add an entry to the ZBC color table using the specified color format and value" },

    { "-zbc_depth_table_entry", "tu", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "add an entry to the ZBC depth table using the specified Z buffer format and depth value" },

    { "-zbc_stencil_table_entry", "tu", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "add an entry to the ZBC stencil table using the specified Z buffer format and stencil value" },

    SIMPLE_PARAM("-share_subctx_cfg", "Share the subctx partition table or watermarks between traces. Formate: -share_subctx_cfg"),

    { "-message_enable", "t", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "Enable debug messages from mods modules.  Separate with colons, e.g. -message_enable ModsCore:ModsLwGpu"},

    { "-message_disable", "t", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "Disable debug messages from mods modules.  Separate with colons, e.g. -message_disable ModsCore:ModsLwGpu"},

    SIMPLE_PARAM("-flush_l2_probe_filter_cache", "flush the GPU probe filter cache"),

    STRING_PARAM("-smc_partitioning", "configure the gpu partition and smc information. Example:"
            "-smc_partitioning [GPU Partition 1]..[GPU Partition N], or "
            "-smc_partitioning {GPU Partition 1}..{GPU Partition N},"
            "GPU Partition, <GPC number in SMC engine M1>sysN1-<GPC number in SMC engine M2>sysN2-"
            "...-<GPC number in SMC engine M1>sysN2\n"
            "[ ] or { } square brackets or braces for partition boundary, [ ] or { } means a partition\n"
            "( ) parentheses for SMC Engine boundary, ( ) means a SMC Engine, "
            "[()-()-()-()] or {()-()-()-()} means a partition with 4 SMC Engines. In addtion, \"-\" is inserted in between SMC engine.\n"
            "More Details you can check https://confluence.lwpu.com/display/ArchMods/SMC+static+partitioning"),

    SIMPLE_PARAM("-smc_partitioning_sys0_only", "specified all non-floorsweeping GPC will be connected to the sys0.\n"),

    STRING_PARAM("-smc_mem", "SMC Partition specification Hopper onwards. Example:"
                " -smc_mem {GpuPartition0}....{GpuPartitionN},"
                "GpuPartition -> PART_SIZE:PART_NAME\n"
                "Each {} is one partition\n"
                "Each partition needs to specify a required PART_SIZE (FULL/HALF/QUARTER/EIGHTH). The str comparison done by MODS for this would be case-insensitive.\n"
                "PART_NAME is the name of the partition but is optional. If specified, it has to be unique.\n"
                "More details at: https://confluence.lwpu.com/pages/viewpage.action?pageId=249627380"),
    STRING_PARAM("-smc_eng", "SMC Engine specification Hopper onwards. Example:"
                " -smc_eng {SmcEngine0}....{SmcEngineN},"
                "SmcEngine -> ENG_SIZE:ENG_NAME\n"
                "Each {} is one partition\n"
                "There is one-one mapping between -smc_mem and -smc_eng. First {} partition in -smc_eng is the first {} partition in -smc_mem and so on.\n"
                "Within a {} partition, each SmcEngine is split by comma\n"
                "ENG_SIZE is the number of GPCs for the engine and ENG_NAME is the name of the SmcEngine\n"
                "More details at: https://confluence.lwpu.com/pages/viewpage.action?pageId=249627380"),

    { "-fla_alloc", "LL", 0, 0, 0,
      "Allocate an FLA VaSpace. Usage: -fla_alloc <size> <base_address>" },
    { "-fla_export", "Lt", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "Export a surface into FLA VaSpace and assign it a name. Usage: -fla_export <size> <name>" },
    { "-fla_export_page_size", "tt", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "Specify the page size for the FLA export surface with the indicated name. Usage: -fla_export_page_size <name> <page_size>" },
    { "-fla_export_pte_kind", "tt", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "Specify the PTE kind for the FLA export surface with the indicated name. Usage: -fla_export_page_size <name> <pte_kind>" },
    { "-fla_export_acr1", "tt", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "Specify the ACR1 (WPR) property for the FLA export surface with the indicated name. Usage: -fla_export_acr1 <name> <ON|OFF>" },
    { "-fla_export_acr2", "tt", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "Specify the ACR2 (WPR) property for the FLA export surface with the indicated name. Usage: -fla_export_acr2 <name> <ON|OFF>" },
    { "-fla_export_aperture", "tt", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "Specify the aperture for the FLA export surface with the indicated name. Usage: -fla_export_aperture <name> <VIDEO|COHERENT_SYSMEM|NONCOHERENT_SYSMEM|P2P>" },
    { "-fla_export_peer_id", "tu", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "Specify the peer ID for the FLA export surface with the indicated name. Usage: -fla_export_peer_id <name> <peer_id>" },
    { "-fla_egm_alloc", "t", ParamDecl::PARAM_MULTI_OK, 0, 0,
      "Specify the EGM flag for the FLA export surface with the indicated name. Usage: -fla_egm_alloc <name>" },
    
    LAST_PARAM
};

static const ParamConstraints argus_constraints[] = {
    MUTUAL_EXCLUSIVE_PARAM("-smc_partitioning", "-smc_partitioning_sys0_only"),
    MUTUAL_EXCLUSIVE_PARAM("-smc_partitioning", "-smc_mem"),
    MUTUAL_EXCLUSIVE_PARAM("-smc_partitioning", "-smc_eng"),
    MUTUAL_EXCLUSIVE_PARAM("-smc_partitioning_sys0_only", "-smc_mem"),
    MUTUAL_EXCLUSIVE_PARAM("-smc_partitioning_sys0_only", "-smc_eng"),
    MUTUAL_EXCLUSIVE_PARAM("-smc_partitioning", "-alloc_vid"),
    MUTUAL_EXCLUSIVE_PARAM("-smc_partitioning_sys0_only", "-alloc_vid"),
    MUTUAL_EXCLUSIVE_PARAM("-smc_mem", "-alloc_vid"),
    LAST_CONSTRAINT
};

LWGpuResource::LWGpuResource(ArgDatabase *args, GpuDevice *pGpuDevice)
    :m_pGpuDevice(pGpuDevice)
    ,m_CrcChannelMutex(Tasker::AllocMutex("LWGpuResource::m_CrcChannelMutex", Tasker::mtxUnchecked))
    ,m_grCopyEngineType(LW2080_ENGINE_TYPE_LAST)
    ,m_FECSMailboxMutex(Tasker::AllocMutex("LWGpuResource::m_FECSMailboxMutex", Tasker::mtxUnchecked))
    ,m_cfg_base(0)
    ,m_pSmcResourceController(nullptr)
    ,m_SmcRegHalMutex(Tasker::AllocMutex("LWGpuResource::m_SmcRegHalMutex", Tasker::mtxUnchecked))
{
    s_LWGpuResources.push_back(this); 
    MASSERT(pGpuDevice);
    // bug 345041, sanity check display config before run
    UINT32 parseRet;
    params.reset(new ArgReader(ParamList));

    string devStr = Utility::StrPrintf("dev%d", pGpuDevice->GetDeviceInst());
    parseRet = params->ParseArgs(args, devStr.c_str());

    if (!parseRet)
    {
        MASSERT(!"LWGpuResource::LWGpuResource: ParseArgs failed\n");
    }

    if (!params->ValidateConstraints(argus_constraints))
    {
        ErrPrintf("LWGpuResource::LWGpuResource: ParseArgs failed. Some arguments are exclusived.\n");
        MASSERT(0);
    }

    // Cache the floorsweeping info before SMC
    // The floorsweeping info can't be accessed by VF
    if (!Platform::IsVirtFunMode())
    {
        for (UINT32 subdev = 0; subdev < GetGpuDevice()->GetNumSubdevices(); subdev++)
        {
            GpuSubdevice *pSubdev = GetGpuSubdevice(subdev);
            FloorsweepImpl *pFsImpl = pSubdev->GetFsImpl();

            m_FsInfo[subdev] = make_tuple(pSubdev->GetGpcCount(),
                                        pFsImpl->GpcMask(),
                                        pSubdev->GetTpcCount(),
                                        pFsImpl->TpcMask(),
                                        pSubdev->GetMaxSmPerTpcCount());
        }
    }

    // This still uses the current subdevice.  For true homogeneous SLI
    // support, this needs to be resolved
    GpuSubdevice * pSubdev = GetGpuSubdevice();

    if (!Platform::IsTegra())
    {
        auto pGpuPcie = pSubdev->GetInterface<Pcie>();
        MASSERT((pGpuPcie->BusNumber() != Gpu::IlwalidPciBus) &&
                (pGpuPcie->DeviceNumber() != Gpu::IlwalidPciDevice) &&
                (pGpuPcie->FunctionNumber() != Gpu::IlwalidPciFunction));
        m_cfg_base = Pci::GetConfigAddress(pGpuPcie->DomainNumber(),
                                           pGpuPcie->BusNumber(),
                                           pGpuPcie->DeviceNumber(),
                                           pGpuPcie->FunctionNumber(),
                                           0);
    }

    m_ChannelLogging = (params->ParamPresent("-channel_logging") != 0);

    if (PolicyManager::HasInstance())
    {
        PolicyManager::Instance()->SetChannelLogging(m_ChannelLogging);
    }

    if (params->ParamPresent("-disable_timer"))
    {
        // Keep the host interface timeslice timer from ticking
        pSubdev->RegWr32(LW_PTIMER_NUMERATOR, 0);
        pSubdev->RegWr32(LW_PTIMER_DENOMINATOR, 0);
        pSubdev->RegWr32(LW_PTIMER_TIME_0, 0);
        pSubdev->RegWr32(LW_PTIMER_TIME_1, 0);
        pSubdev->RegWr32(LW_PTIMER_TIME_0, 0);
    }

    m_HandleIntr = (params->ParamPresent("-handleIntr") > 0);
    if (m_HandleIntr)
        GpuPtr()->RegisterGrIntrHook(MdiagGrIntrHook, this);

    InitSmOorAddrCheckMode();

    // Defer handling the register until SMC partition is done if it's inteneded to run in SMC mode.
    // If run in legacy mode, process register options ASAP.
    if (!params->ParamPresent("-smc_partitioning") && 
        !params->ParamPresent("-smc_partitioning_sys0_only") &&
        !params->ParamPresent("-smc_mem"))
    {
        ParseHandleOptionalRegisters(params.get(), pSubdev);
    }

    if (params->ParamPresent("-dump_pri_group") > 0)
    {
        RegName reg;
        reg.RegNameStr = params->ParamStr("-dump_pri_group");
        m_DumpGroups.insert(reg);
    }

    if (params->ParamPresent("-skip_pri_group") > 0)
    {
        RegName reg;
        reg.RegNameStr = params->ParamStr("-skip_pri_group");
        m_SkipGroups.insert(reg);
    }

    if (params->ParamNArgs("-dump_pri_range") > 0)
    {
        UINT32 rangeNumber = params->ParamPresent("-dump_pri_range");
        for(UINT32 rangeIndex = 0; rangeIndex < rangeNumber; rangeIndex++)
        {
            RegRange regRange;
            regRange.RegAddrRange = make_pair(params->ParamNUnsigned("-dump_pri_range", rangeIndex,0),
                                              params->ParamNUnsigned("-dump_pri_range", rangeIndex,1));
            m_DumpRanges.insert(regRange);
        }
    }

    if (params->ParamNArgs("-dump_pri_range_no_manuals_check") > 0)
    {
        UINT32 rangeNumber = params->ParamPresent("-dump_pri_range_no_manuals_check");
        for(UINT32 rangeIndex = 0; rangeIndex < rangeNumber; rangeIndex++)
        {
            RegRange regRange;
            regRange.RegAddrRange = make_pair(params->ParamNUnsigned("-dump_pri_range_no_manuals_check", rangeIndex, 0),
                                              params->ParamNUnsigned("-dump_pri_range_no_manuals_check", rangeIndex, 1));
            m_DumpRangesNoCheck.insert(regRange);
        }
    }

    if (params->ParamNArgs("-skip_pri_range") > 0)
    {
        UINT32 rangeNumber = params->ParamPresent("-skip_pri_range");
        for(UINT32 rangeIndex = 0; rangeIndex < rangeNumber; rangeIndex++)
        {
            RegRange regRange;
            regRange.RegAddrRange = make_pair(params->ParamNUnsigned("-skip_pri_range", rangeIndex, 0),
                                              params->ParamNUnsigned("-skip_pri_range", rangeIndex, 1));
            m_SkipRanges.insert(regRange);
        }
    }

    if (params->ParamPresent("-dump_pri_file") > 0)
    {
        ProcessPriFile(params->ParamStr("-dump_pri_file"), true);
    }

    if (params->ParamPresent("-skip_pri_file") > 0)
    {
        ProcessPriFile(params->ParamStr("-skip_pri_file"), false);
    }

    // -------------------------------------------------------------
    // Use this to suppress a warning about rc defined but not used.
    // Applies to MASSERTs below
    #ifdef DEBUG
    #define RC_USED(x) RC rc = x
    #else
    #define RC_USED(x) x
    #endif
    // -------------------------------------------------------------

    if (params->ParamPresent("-alloc_vid") > 0)
    {
        // This still uses the current subdevice.  For true homogeneous SLI
        // support, this needs to be resolved
        RC_USED( AllocBuffer(params->ParamUnsigned("-alloc_vid"), _DMA_TARGET_VIDEO, params.get(), LwRmPtr().Get()));
        MASSERT(OK == rc);
    }

    if (params->ParamPresent("-alloc_coh") > 0)
    {
        // This still uses the current subdevice.  For true homogeneous SLI
        // support, this needs to be resolved
        RC_USED( AllocBuffer(params->ParamUnsigned("-alloc_vid"), _DMA_TARGET_COHERENT, params.get(), LwRmPtr().Get()));
        MASSERT(OK == rc);
    }

    if (params->ParamPresent("-alloc_coh") > 0)
    {
        // This still uses the current subdevice.  For true homogeneous SLI
        // support, this needs to be resolved
        RC_USED( AllocBuffer(params->ParamUnsigned("-alloc_vid"), _DMA_TARGET_NONCOHERENT, params.get(), LwRmPtr().Get()));
        MASSERT(OK == rc);
    }

    // bug 359112, extended tags for gt216
    if (params->ParamPresent("-ext_tag_en") > 0)
        WarnPrintf("-ext_tag_en option only valid on gt21x chips!\n");

    if (params->ParamPresent("-disp_cfg_json"))
    {
        if (GetGpuDevice()->GetDisplay()->GetDisplayClassFamily() ==
            Display::NULLDISP)
        {
            ErrPrintf("Using a -disp_cfg_json arg with null display. Make sure your netlist has a display engine, "
                      "and you aren't using -null_display");
            MASSERT(!"Cannot use any -disp_cfg_json args with null display.");
        }
        RC rc = HandleJsonDisplayConfig(params.get());
        if (OK != rc)
            ErrPrintf("Can not create LWGpuResource: %s\n", rc.Message());
        MASSERT(OK == rc);
    }
    else if (params->ParamPresent("-disp_cfg") ||
         params->ParamPresent("-disp_cfg0") ||
         params->ParamPresent("-disp_cfg1") ||
         params->ParamPresent("-disp_cfg2") ||
         params->ParamPresent("-disp_cfg3") ||
         params->ParamPresent("-disp_cfg_imp") ||
         params->ParamPresent("-pitch_disp_primary") ||
         params->ParamPresent("-block_width_disp_primary") ||
         params->ParamPresent("-block_height_disp_primary") ||
         params->ParamPresent("-block_width_disp_overlay") ||
         params->ParamPresent("-block_height_disp_overlay") ||
         params->ParamPresent("-disp_single_head_mode") ||
         params->ParamPresent("-disp_raster_lock") ||
         params->ParamPresent("-disp_clk_switch"))
    {
        ErrPrintf("CRT manager is no longer supported. Use DPC instead.\n"
                  "See here: https://confluence.lwpu.com/display/Display/DPC+Files \n");
        MASSERT(!"CRT manager is no longer supported. Use DPC instead.");
    }

    // Get max physical channel number supported from RM
    LwRmPtr pLwRm;
    LW2080_CTRL_FIFO_GET_PHYSICAL_CHANNEL_COUNT_PARAMS getChCntParams = {0};
    RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetGpuSubdevice(0)),
                LW2080_CTRL_CMD_FIFO_GET_PHYSICAL_CHANNEL_COUNT,
                &getChCntParams, sizeof(getChCntParams));
    if (OK != rc)
    {
        m_MaxNumChId = 128; // hardcode 128 as default
        WarnPrintf("Can not get max physical channel count. Use %d as default.\n", m_MaxNumChId);
        rc.Clear();
    }
    else
    {
        m_MaxNumChId = getChCntParams.physChannelCount;
    }

    // Init random chid seed
    UINT32 chIdRandAllocSeed0, chIdRandAllocSeed1, chIdRandAllocSeed2;
    chIdRandAllocSeed0 = params->ParamUnsigned("-alloc_chid_randseed0", 0x1111);
    chIdRandAllocSeed1 = params->ParamUnsigned("-alloc_chid_randseed1", 0x2222);
    chIdRandAllocSeed2 = params->ParamUnsigned("-alloc_chid_randseed2", 0x3333);
    m_ChIdRandomStream.reset(new RandomStream(chIdRandAllocSeed0,
                                              chIdRandAllocSeed1,
                                              chIdRandAllocSeed2));
    DebugPrintf("Set channel ID selection RandomStream seeds to 0x%x, 0x%x, 0x%x\n",
               chIdRandAllocSeed0, chIdRandAllocSeed1, chIdRandAllocSeed2);

    if (params->ParamPresent("-loc_inst_random") > 0)
    {
        UINT16 seed0 = params->ParamNUnsigned("-loc_inst_random", 0);
        UINT16 seed1 = params->ParamNUnsigned("-loc_inst_random", 1);
        UINT16 seed2 = params->ParamNUnsigned("-loc_inst_random", 2);

        m_LocationRandomizer.reset(new RandomStream(seed0, seed1, seed2));
    }

    m_ContexScheduler = make_shared<LWGpuContexScheduler>();
    m_IsMultiVasSupported = false;

    if (!GpuPtr()->IsInitSkipped())
    {

        LW0080_CTRL_DMA_GET_CAPS_PARAMS paramsCaps = {0};
        paramsCaps.capsTblSize = LW0080_CTRL_DMA_CAPS_TBL_SIZE;
        rc = pLwRm->Control(pLwRm->GetDeviceHandle(GetGpuDevice()),
                 LW0080_CTRL_CMD_DMA_GET_CAPS,
                 &paramsCaps,
                 sizeof(paramsCaps));
        MASSERT(rc == OK);
        m_IsMultiVasSupported = !!LW0080_CTRL_GR_GET_CAP(paramsCaps.capsTbl,
            LW0080_CTRL_DMA_CAPS_MULTIPLE_VA_SPACES_SUPPORTED);
    }

    m_SyncTracesMethods = params->ParamPresent("-sync_trace_method_send") > 0;

    // bug 359112, extended tags for gt216
    if (params->ParamPresent("-flush_l2_probe_filter_cache") > 0)
    {
        rc = pSubdev->FlushL2ProbeFilterCache();
        MASSERT(rc == OK);
    }

    PolicyManagerMessageDebugger::Instance()->Init(params.get());

    GpuSubdevice * pSubDev = GetGpuSubdevice();

    if (params->ParamPresent("-smc_partitioning") ||
            params->ParamPresent("-smc_partitioning_sys0_only") ||
            pSubDev->IsSMCEnabled() ||
            params->ParamPresent("-smc_mem"))
    {
        m_pSmcResourceController = new SmcResourceController(this, params.get());
    }

    m_pSharedSurfaceController = new SharedSurfaceController();

    rc = HandleFlaConfig(pLwRm.Get());
    MASSERT(rc == OK && "Failed to configure FLA\n");
}

RC LWGpuResource::CheckMemoryMinima()
{
    if ( params->ParamPresent("-sysmem_size"))
    {
        size_t size = params->ParamUnsigned("-sysmem_size");

        // Different platforms require different tests for sysmem size.
        switch (Platform::GetSimulationMode())
        {
        case Platform::Hardware:
            if (Platform::ExtMemAllocSupported())
            {
                if (OK != Platform::TestAllocPages(size*1024*1024))
                {
                    ErrPrintf("INSUFFICIENT_RESOURCES: sysmem size is smaller than required by -sysmem_size.\n");
                    return RC::LWRM_INSUFFICIENT_RESOURCES;
                }
            }
            break;
        case Platform::Fmodel:
        case Platform::RTL:
            {
                // for fmodel and RTL, both fb and sysmem come from
                // the pool of system memory
                if (params->ParamPresent("-fb_minimum"))
                {
                    size += params->ParamUnsigned("-fb_minimum");
                }
                // Here we should call TestAllocPages, but it is not
                // implemented on Fmodel/RTL. We should correctly
                // implement TestAllocPages on Fmodel/RTL and try to allocate
                // memory from chiplib.  See bug 874306.
#if 0
                if (OK != Platform::TestAllocPages(size*1024*1024))
                {
                    ErrPrintf("INSUFFICIENT_RESOURCES: sysmem size is smaller than required by -sysmem_size.\n");
#if !defined(LWCPU_ARM)
                    return RC::LWRM_INSUFFICIENT_RESOURCES;
#endif
                }
#endif
            }
            break;
        default:
            break;
        }
    }

    if (params->ParamPresent("-fb_minimum"))
    {
        UINT64 size = params->ParamUnsigned("-fb_minimum");

        // Different platforms require different tests for fb size.
        switch (Platform::GetSimulationMode())
        {
        case Platform::Hardware:
            {
                UINT64 fbMb = GetGpuSubdevice()->GetFB()->GetGraphicsRamAmount() >> 20;
                if (fbMb < size)
                {
                    ErrPrintf("INSUFFICIENT_RESOURCES: FB size (%llu MB) is smaller than required by -fb_minimum.\n", fbMb);
                    return RC::LWRM_INSUFFICIENT_RESOURCES;
                }
            }
            break;
        case Platform::Fmodel:
        case Platform::RTL:
            if (!params->ParamPresent("-sysmem_size")) // already checked.
            {
                // for fmodel and RTL, fb comes from
                // the pool of system memory
                if ((sizeof(size_t) == 4) && (size > 0xFFFFFFFF))
                {
                    ErrPrintf("\"size\" is too big on this platform.\n");
                    return RC::LWRM_INSUFFICIENT_RESOURCES;
                }
                // Here we should call TestAllocPages, but it is not
                // implemented on Fmodel/RTL. We should correctly
                // implement TestAllocPages on Fmodel/RTL and try to allocate
                // memory from chiplib.  See bug 874306.
#if 0
                else if (OK != Platform::TestAllocPages(size*1024*1024))
                {
                    ErrPrintf("INSUFFICIENT_RESOURCES: FB size is smaller than required by -fb_minimum.\n");
                    return RC::LWRM_INSUFFICIENT_RESOURCES;
                }
#endif
            }
            break;
        default:
            break;
        }
    }

    return OK;
}

RC LWGpuResource::AllocBuffer
(
    UINT32 size, _DMA_TARGET Target, const ArgReader *params,
    LwRm* pLwRm, UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */
)
{
    RC rc;
    m_Buffers.push_back(MdiagSurf());
    MdiagSurf* const buffer = &m_Buffers.back();
    string pteKindName;
    CHECK_RC(ParseDmaBufferArgs(*buffer, params, "alloc", &pteKindName, 0));
    CHECK_RC(SetPteKind(*buffer, pteKindName, GetGpuDevice()));
    buffer->SetArrayPitch(size);
    buffer->SetColorFormat(ColorUtils::Y8);
    buffer->SetForceSizeAlloc(true);
    buffer->SetLocation(TargetToLocation(Target));

    if (Platform::IsPhysFunMode() && (Target == _DMA_TARGET_VIDEO))
    {
        // This buffer is allocated before launching VF processes.
        // Force this internal buffer to be allocated from TOP to save the
        // reserved VMMU segment. Comments above vfSetting.fbReserveSize in
        // VmiopMdiagElwManager::ParseVfConfigFile() have more details.
        //
        buffer->SetPaReverse(true);
    }

    CHECK_RC(buffer->Alloc(GetGpuDevice(), pLwRm));
    PrintDmaBufferParams(*buffer);
    CHECK_RC(buffer->Map(subdev));

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        PHYSADDR physAddr = Platform::VirtualToPhysical(buffer->GetAddress());
        Platform::EscapeWrite("Vid_Alloc_Addr_Low", 0, 4, (UINT32) physAddr);
        Platform::EscapeWrite("Vid_Alloc_Addr_High", 0, 4, (UINT32) (physAddr >> 32));
        Platform::EscapeWrite("Vid_Alloc_Size", 0, 4, size);
    }

    return OK;
}

void LWGpuResource::FreeDmaChannels(LwRm* pLwRm)
{
    // Clean DMA loading channels for this client
    for (auto& dmaChannel : m_DmaChannels)
    {
        // dmaChannel.first <-> pair<LwRm*, LwRm::Handle>
        // Check if LwRm matches and
        // for the channel existence
        if (dmaChannel.first.first == pLwRm &&
            dmaChannel.second)
        {
            dmaChannel.second->WaitForIdle();
            if (m_DmaObjHandles[dmaChannel.first])
            {
                dmaChannel.second->DestroyObject(m_DmaObjHandles[dmaChannel.first]);
                m_DmaObjHandles.erase(dmaChannel.first);
            }
            dmaChannel.second->DestroyPushBuffer();
            delete dmaChannel.second;
            dmaChannel.second = nullptr;
        }
    }
}

LWGpuResource::~LWGpuResource()
{
    for (auto& arg : m_privArgMap)
    {
        ArgDatabase *argDB;
        ArgReader *privParams;
        tie(argDB, privParams) = arg.second;

        delete argDB;
        delete privParams;
    }

    FreeDmaChannels(LwRmPtr().Get());

    delete m_pSharedSurfaceController;

    if (IsSMCEnabled())
    {
        // Free peer mappings here, before freeing RM clients.
        auto pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        pGpuDevMgr->RemoveAllPeerMappings(GetGpuSubdevice());
    }

    FreeSmcRegHals();

    if (m_pSmcResourceController)
    {
        delete m_pSmcResourceController;
        m_pSmcResourceController = nullptr;
    }

    UINT32 elpgMask = params->ParamUnsigned("-elpg_mask", 0x0);
    if (elpgMask > 0)
    {
        // example:
        // pgParams[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        // pgParams[0].parameterExtended = PMU::ELPG_GRAPHICS_ENGINE;
        // pgParams[0].parameterValue = 1; (0 for disable)

        vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams;
        LW2080_CTRL_POWERGATING_PARAMETER oneParam = {0};
        oneParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        oneParam.parameterValue = 0;
        UINT32 cursor = 0x1;
        for (int index = 0; index < 32; index++)
        {
            if (elpgMask & cursor)
            {
                oneParam.parameterExtended = index;
                pgParams.push_back(oneParam);
            }
            cursor <<= 1;
        }

        GpuSubdevice *pSubDev = GetGpuSubdevice(0);
        PMU* pPmu;
        RC rc = pSubDev->GetPmu(&pPmu);
        if (OK == rc)
        {
            UINT32 Flags = 0;
            Flags = FLD_SET_DRF(2080, _CTRL_MC_SET_POWERGATING_THRESHOLD,
                                _FLAGS_BLOCKING,
                                _TRUE, Flags);
            rc = pPmu->SetPowerGatingParameters(&pgParams, Flags);
            if (OK != rc)
            {
                ErrPrintf("Error turning off elpg: %s\n", rc.Message());
            }
        }
        else
        {
            ErrPrintf("Error retrieving PMU: %s\n", rc.Message());
        }
    }

    for (int i=0; i < LW2080_MAX_SUBDEVICES; i++)
    {
        m_PerfMonList[i].reset(0);
    }

    m_Buffers.clear();

    for (map<uintptr_t, Surface2D*>::iterator iter = m_AllocSurfaces.begin(); iter != m_AllocSurfaces.end(); ++iter)
    {
        delete iter->second;
    }
    m_AllocSurfaces.clear();

    Tasker::FreeMutex(m_FECSMailboxMutex);
    Tasker::FreeMutex(m_CrcChannelMutex);
    Tasker::FreeMutex(m_SmcRegHalMutex);

    // VA space normally gets freed by the test. FLA VA space lwrrently only
    // uses the the global RM client (LwRmPtr), which is different from the one
    // that the test uses when running in SMC mode, so free the VA space owned
    // by the global RM client here.
    GetVaSpaceManager(LwRmPtr().Get())->FreeObjectInTest(TEST_ID_GLOBAL);

    for(VaSpaceManagers::iterator iter = m_VaSpaceManagers.begin(); iter != m_VaSpaceManagers.end(); ++iter)
    {
        MASSERT(iter->second->IsEmpty());
        delete iter->second;
    }
    m_VaSpaceManagers.clear();

    for(SubCtxManagers::iterator iter = m_SubCtxManagers.begin(); iter != m_SubCtxManagers.end(); ++iter)
    {
        MASSERT(iter->second->IsEmpty());
        delete iter->second;
    }
    m_SubCtxManagers.clear();

    PolicyManagerMessageDebugger::Instance()->Release();

    // Iterate over each FLA export surface (mapped name->surface) on this GPU
    for(auto &kv : m_FlaExportSurfaces)
    {
        s_FlaExportGpuMap.erase(kv.first);
    }
    m_FlaExportSurfaces.clear();

    CRegCtrlFactory::s_Registers.clear();
    
    m_DmaChannels.clear();
    m_DmaObjHandles.clear();

    SmcEngine::FreeFakeSmcEngine();

    m_pGpuDevice = nullptr;
}

void LWGpuResource::CheckRegCtrlResult(const StickyRC& rc, const char *pOption)
{
    MASSERT(pOption != 0);

    if (OK != rc)
    {
        /*
        // 1. When bad PRI, just error prints but won't assert until
        //    -assert_upon_bad_pri_config provided.
        //    Will actually assert if this arg provided.
        // 2. If -disable_assert_upon_bad_pri_config argument provided,
        //    only error prints but won't assert even if
        //    -assert_upon_bad_pri_config argument provided.
        // 3. Won't assert if Amodel even if -assert_upon_bad_pri_config
        //    argument provided.
        */
        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            WarnPrintf("Ignore Error(s) while processing register ctrl (%s) params because AMODEL is register agnostic.\n",
                       pOption);
        }
        else if (params->ParamPresent("-disable_assert_upon_bad_pri_config") > 0)
        {
            WarnPrintf("Ignore Error(s) while processing register ctrl option %s because of -disable_assert_upon_bad_pri_config\n",
                       pOption);
        }
        else
        {
            if (params->ParamPresent("-assert_upon_bad_pri_config") > 0)
            {
                ErrPrintf("Error(s) found while processing register ctrl option %s.\n", pOption);
                MASSERT(!"For fermi and later chips, assert by default if there's error.");
            }
            else
            {
                WarnPrintf("Error(s) found while processing register ctrl option %s. Skip assertion because -assert_upon_bad_pri_config not specified.\n",
                           pOption);
            }
        }
    }
}

// Register related operations:
// 1. Dump registers
// 2. reg_eos check
RC LWGpuResource::RegisterOperations()
{
    // This still uses the current subdevice when dumping registers.  When true
    // homogeneous SLI support is required, it may be necessary to add m_DumpSubdev
    // list as well (or make the Dump lists per subdevice)
    if (m_DumpGroups.size() || m_DumpRanges.size() || m_DumpRangesNoCheck.size())
        DumpRegisters();

    // process the list of -reg_eos options.
    //
    StickyRC stickyRc;
    vector<CRegisterControl> regControls;
    stickyRc = ProcessRegBlock(params.get(),       "-reg_eos", regControls);

    // process the list of "-priv +begin gpu -reg"
    for (auto arg : m_privArgMap)
    {
        ArgDatabase *argDB;
        ArgReader *privParams;

        string regSpace = arg.first;
        tie(argDB, privParams) = arg.second;

        vector<CRegisterControl> regCtrls;

        stickyRc = ProcessRegBlock(privParams, "-reg_eos", regCtrls);

        for (auto& reg : regCtrls)
        {
            string regName;
            
            if (reg.m_regPtr)
            {
                regName = reg.m_regPtr->GetName();
            }
            // Handle domain register which is zero based register address and LW_PGRAPH_PRI_GPC[%u]/_GPC[%u]_TPC[%u] registers
            if ((reg.m_domain != RegHalDomain::RAW) || (std::regex_search(regName, std::regex(MDiagUtils::s_LWPgraphGpcPrefix + "[0-9]+"))))
            {
                // need to adjust the domain for multicast lwlink
                reg.m_domain = MDiagUtils::AdjustDomain(reg.m_domain, regSpace);
                reg.m_regSpace = regSpace;
            }

            regControls.push_back(reg);
        }
    }

    stickyRc = HandleOptionalRegisters(&regControls);

    regControls.clear();
    stickyRc = ProcessRegBlock(m_confParams.get(), "-reg_eos", regControls);
    vector<CRegisterControl>::iterator iter = regControls.begin();
    if (!UsePcieConfigCycles())
    {
        for (; iter != regControls.end(); ++ iter)
        {
            // add config space base for -cfg registers
            // which is to be accessed through bar0
            iter->m_address += DEVICE_BASE(LW_PCFG);
        }
    }
    stickyRc = HandleOptionalRegisters(&regControls);

    // Print error message but don't fail the test
    // These messages can be picked up in the post log process phase
    // EDIT Dec 2015:
    // A "-reg_eos" option is processed at the end of a testing and is not able to affect
    // overall testing result PASS or FAIL already by reading its returned value at this
    // point. So we'd change error handling to meet requirements like those in bug#1686707,
    // which needs registers comparing result as a testing criteria of PASS or FAIL.
    // As users could accept, we assert if compare failed, not just print an error msg.
    // Furthermore, this updated "-reg_eos" behaviros align with "-reg"'s in
    // LWGpuResource::ParseHandleOptionalRegisters().
    CheckRegCtrlResult(stickyRc, "-reg_eos");

    return stickyRc;
}

void LWGpuResource::PrintDumpRegisters
(
    FILE *pFile,
    const RefManualRegister* pReg,
    UINT32 addr,
    int i,
    int j,
    UINT32 value,
    SmcEngine *pSmcEngine
) const
{
    // Prepend SMC Syspipe Id
    if (pSmcEngine)
    {
        fprintf(pFile, "SMC:%d ", pSmcEngine->GetSysPipe());
    }

    if (pReg->GetDimensionality() == 2)
    {
        fprintf(pFile, "%s(%d,%d)(0x%08x) = 0x%08x\n",
                pReg->GetName().c_str(), i, j, addr, value);
    }
    else if (pReg->GetDimensionality() == 1)
    {
        fprintf(pFile, "%s(%d)(0x%08x) = 0x%08x\n",
                pReg->GetName().c_str(), i, addr, value);
    }
    else
    {
        fprintf(pFile, "%s(0x%08x) = 0x%08x\n",
                pReg->GetName().c_str(), addr, value);
    }
}

void LWGpuResource::ProcessPrintDumpRegisters
(
    FILE* pFile,
    const RefManualRegister* pReg,
    UINT32 addr,
    UINT32 subdev,
    bool isConfig,
    int i,
    int j
) const
{
    GpuSubdevice *pSubdev = GetGpuSubdevice(subdev);
    UINT32 readAddress = addr;
    UINT32 value = 0;
    if (IsPgraphReg(addr) && IsSMCEnabled())
    {
        for (const auto& gpuPartition : m_pSmcResourceController->GetActiveGpuPartition())
        {
            for (const auto& smcEngine : gpuPartition->GetActiveSmcEngines())
            {
                LwRm *pLwRm = GetLwRmPtr(smcEngine);
                readAddress += MDiagUtils::GetDomainBase(pReg->GetName().c_str(), nullptr , pSubdev, pLwRm);
                
                if (isConfig && UsePcieConfigCycles())
                {
                    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
                    Platform::PciRead32(pGpuPcie->DomainNumber(), pGpuPcie->BusNumber(), 
                        pGpuPcie->DeviceNumber(), pGpuPcie->FunctionNumber(), readAddress, &value);
                }
                else if (isConfig)
                {
                    readAddress += DEVICE_BASE(LW_PCFG);
                    value = RegRd32(readAddress, subdev, smcEngine);
                }
                else
                {
                    value = RegRd32(readAddress, subdev, smcEngine);
                }

                PrintDumpRegisters(pFile, pReg, readAddress, i, j,
                    value, smcEngine);
            }
        }
    }
    else
    {
        LwRm *pLwRm = LwRmPtr().Get();
        readAddress += MDiagUtils::GetDomainBase(pReg->GetName().c_str(), nullptr , pSubdev, pLwRm);
        if (isConfig && UsePcieConfigCycles())
        {
            auto pGpuPcie = GetGpuSubdevice(subdev)->GetInterface<Pcie>();
            Platform::PciRead32(pGpuPcie->DomainNumber(), pGpuPcie->BusNumber(), 
            pGpuPcie->DeviceNumber(), pGpuPcie->FunctionNumber(), readAddress, &value);
        }
        else if (isConfig)
        {
            readAddress += DEVICE_BASE(LW_PCFG);
            value = RegRd32(readAddress, subdev);
        }
        else
        {   
            value = RegRd32(readAddress, subdev);
        }

        PrintDumpRegisters(pFile, pReg, readAddress, i, j, value);
    }
}

void LWGpuResource::DumpRegisters
(
    UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */
) const
{
    RefManual *pManual = GetGpuSubdevice(subdev)->GetRefManual();

    FILE * pFile;
    RC rc = Utility::OpenFile(params->ParamStr("-dump_pri_file_to", "regs.txt"), &pFile, "w");

    if (rc != OK)
    {
        ErrPrintf("Error while opening file: %s\n", rc.Message());
    }
    MASSERT(rc == OK);

    for (int regIdx = 0; regIdx < pManual->GetNumRegisters(); ++regIdx)
    {
        const RefManualRegister* pReg = pManual->GetRegister(regIdx);
        MASSERT(pReg);
        RegName regName;
        RegRange regRange;

        if (MatchGroupName(m_DumpGroups, pReg, &regName) ||
            MatchRange(m_DumpRanges, pReg, &regRange))
        {

            bool isConfig = regName.RegAddrSpace == RegName::Conf ||
                regRange.RegAddrSpace == RegRange::Conf;
            
            if (pReg->GetDimensionality() == 2) // 2D array
            {
                for (int i = 0; i < pReg->GetArraySizeI(); i++)
                {
                    for(int j = 0; j < pReg->GetArraySizeJ(); j++)
                    {
                        ProcessPrintDumpRegisters(pFile, pReg, pReg->GetOffset(i, j), 
                                                  subdev, isConfig, i, j);
                    }
                }
            }
            else if (pReg->GetDimensionality() == 1) // 1D array
            {
                for(int i = 0; i < pReg->GetArraySizeI(); i++)
                {
                    ProcessPrintDumpRegisters(pFile, pReg, pReg->GetOffset(i), 
                                              subdev, isConfig, i);
                }
            }
            else
            {
                ProcessPrintDumpRegisters(pFile, pReg, pReg->GetOffset(),
                                          subdev, isConfig);
            }
        }
    }

    RegRanges::const_iterator end = m_DumpRangesNoCheck.end();
    for (RegRanges::const_iterator iter = m_DumpRangesNoCheck.begin(); iter != end; ++iter)
    {
        for (UINT32 addr = iter->RegAddrRange.first;
             addr <= iter->RegAddrRange.second;
             addr += 4)
        {
            // Fix bug 362431 "mcp78 perf: dumping all iXVE registers, most return values of 0".
            // When dumping register, check whether the address is belong to skip ranges or not.
            // If yes, don't dump it.
            if (!MatchRange(m_SkipRanges, addr))
            {
                fprintf(pFile, "0x%08x = 0x%08x\n", addr, RegRd32(addr, subdev));
            }
        }
    }

    fclose(pFile);
}

bool LWGpuResource::MatchGroupName(const RegGroups& groups, const RefManualRegister* reg,
                                   RegName* pRegName) const
{
    RegGroups::const_iterator end = groups.end();
    for(RegGroups::const_iterator iter = groups.begin(); iter != end; ++iter)
    {
        if (iter->RegNameStr == reg->GetName().substr(0, iter->RegNameStr.length()))
        {
            if (pRegName != 0)
                *pRegName = *iter;

            return !MatchGroupName(m_SkipGroups, reg, 0);
        }
    }
    return false;
}

bool LWGpuResource::MatchRange(const RegRanges& ranges, const RefManualRegister* reg,
                               RegRange* pRegRange) const
{
    RegRanges::const_iterator end = ranges.end();
    for(RegRanges::const_iterator iter = ranges.begin(); iter != end; ++iter)
    {
        if (iter->RegAddrRange.first <= reg->GetOffset() &&
            iter->RegAddrRange.second >= reg->GetOffset())
        {
            if (pRegRange != 0)
                *pRegRange = *iter;

            return !MatchRange(m_SkipRanges, reg, 0);
        }
    }
    return false;
}

void LWGpuResource::ProcessPriFile(const char* filename, bool dump)
{
    MASSERT(filename);

    FILE * pFile;
    RC rc = Utility::OpenFile(filename,  &pFile, "r");

    if (rc != OK)
    {
        ErrPrintf("Error while opening file: %s\n", rc.Message());
    }

    MASSERT(rc == OK);

    char buf[256], group[256], *pStr;
    UINT32 rlo, rhi;

    while (!feof(pFile))
    {
        for (pStr = fgets(buf, sizeof(buf), pFile); pStr && *pStr == ' '; ++pStr) /* */ ;

        if (!pStr || *pStr == '#' || *pStr == '\n')
            continue;

        if (sscanf(pStr, "0x%x 0x%x", &rlo, &rhi) == 2)
        {
            RegRange regRange;
            regRange.RegAddrRange = make_pair(rlo, rhi);
            if (0 != strstr(pStr, " -conf"))
            {
                regRange.RegAddrSpace = RegRange::Conf;
            }
            if (dump)
                m_DumpRanges.insert(regRange);
            else
                m_SkipRanges.insert(regRange);
        }
        else if (sscanf(pStr, "%s", group) == 1)
        {
            RegName reg;
            reg.RegNameStr = group;
            if (0 != strstr(pStr, " -conf"))
            {
                reg.RegAddrSpace = RegName::Conf;
            }
            if (dump)
                m_DumpGroups.insert(reg);
            else
                m_SkipGroups.insert(reg);
        }
        else
            MASSERT(!"error in pri file");
    }

    fclose(pFile);
}

int LWGpuResource::ScanSystem(ArgDatabase *args)
{
    ArgReader reader(LWGpuResource::ParamList);

    // parse our parameters first
    if (!reader.ParseArgs(args)) return(0);

    // create an instance of ourself... unless we're told not to
    if (reader.ParamPresent("-no_gpu"))
    {
        InfoPrintf("LWGpuResource::ScanSystem: -no_gpu specified, bailing\n");
        return(0);
    }
    if ( (GpuPtr()->IsInitialized() == false) ||
         (0 == DevMgrMgr::d_GraphDevMgr->NumDevices()))
    {
        InfoPrintf("LWGpuResource::ScanSystem: there are no gpu's to scan...\n");
        return(0);
    }

    if (reader.ParamPresent("-multi_gpu"))
    {
        Device *pGpuDevice = NULL;
        for (UINT32 i = 0; i < DevMgrMgr::d_GraphDevMgr->NumDevices(); i++)
        {
            if (DevMgrMgr::d_GraphDevMgr->GetDevice(i, &pGpuDevice) != OK)
            {
                InfoPrintf("LWGpuResource::ScanSystem: Unable to get GpuDevice %d\n", i);
                return(0);
            }

            LWGpuResource* current = new LWGpuResource(args, (GpuDevice *)pGpuDevice);
            if (OK != current->CheckMemoryMinima())
            {
                // current will get deleted when the resource controller is deleted.
                return -1;
            }

            if (OK != current->CheckDeviceRequirements())
            {
                return -1;
            }
        }
    }
    else
    {
        GpuDevice* pDev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpuDevice();

        LWGpuResource* current = new LWGpuResource(args, pDev);
        if (OK != current->CheckMemoryMinima())
        {
            // current will get deleted when the resource controller is deleted.
            return -1;
        }

        if (OK != current->CheckDeviceRequirements())
        {
            return -1;
        }
    }
    return 1;
}

LWGpuResource *LWGpuResource::GetGpuByClassSupported
(
    UINT32 numClasses,
    const UINT32 classIds[]
)
{
    for (auto pLWGpuResource : GetAllResources())
    {
        if (numClasses == 0)
        {
            // grab any GPU resource without class limitation
            return pLWGpuResource;
        }

        bool bSupported = false;
        for (UINT32 i = 0; (i < numClasses) && !bSupported; i++)
        {
            // go through the class list, if current GPU supports one of
            // the class, return this GPU
            bSupported = LwRmPtr().Get()->IsClassSupported(classIds[i], pLWGpuResource->GetGpuDevice());
        }

        if (bSupported)
            return pLWGpuResource;
    }

    return nullptr;
}

LWGpuResource *LWGpuResource::GetGpuByDeviceInstance(UINT32 devInst)
{
    for (auto pLWGpuResource : LWGpuResource::GetAllResources())
    {
        if (pLWGpuResource->GetGpuDevice()->GetDeviceInst() == devInst)
        {
            return pLWGpuResource;
        }
    }

    return nullptr;
}

LWGpuChannel *LWGpuResource::CreateChannel(LwRm* pLwRm, SmcEngine* pSmcEngine)
{
    pLwRm = pLwRm ? pLwRm : LwRmPtr().Get();
    LWGpuChannel *ch = new LWGpuChannel(this, pLwRm, pSmcEngine);
    return ch;
}

LWGpuChannel *LWGpuResource::GetDmaChannel(const shared_ptr<VaSpace> &pVaSpace, const ArgReader *pParams,
                                            LwRm *pLwRm, SmcEngine* pSmcEngine, UINT32 engineId)
{
    LwRm::Handle hVASpace = pVaSpace->GetHandle();
    pair<LwRm*, LwRm::Handle> dmaChannelKey(pLwRm, hVASpace);
    if (m_DmaChannels[dmaChannelKey] == 0)
    {
        m_DmaChannels[dmaChannelKey] = CreateChannel(pLwRm, pSmcEngine);
        m_DmaChannels[dmaChannelKey]->ParseChannelArgs(pParams);
        m_DmaChannels[dmaChannelKey]->SetVASpace(pVaSpace);
        RC rc = m_DmaChannels[dmaChannelKey]->AllocChannel(engineId);
        if (OK != rc)
        {
            ErrPrintf("Fail to alloc dma channel.\n");
            MASSERT(0);
        }
    }
    return m_DmaChannels[dmaChannelKey];
}

UINT32 LWGpuResource::GetDmaObjHandle(const shared_ptr<VaSpace> &pVaSpace,
                                      UINT32 Class, LwRm* pLwRm, UINT32 *retClass)
{
    LwRm::Handle hVASpace = pVaSpace->GetHandle();
    pair<LwRm*, LwRm::Handle> dmaChannelKey(pLwRm, hVASpace);
    // create the (unique) DMA loading object if it doesn't exist yet.
    if (!m_DmaObjHandles[dmaChannelKey])
        // From Ampere-10, RM would be deducing the engineID from the channel handle
        // Hence it is fine if we pass null as params for the object creation
        m_DmaObjHandles[dmaChannelKey] = m_DmaChannels[dmaChannelKey]->CreateObject(Class, 0, retClass);
    return m_DmaObjHandles[dmaChannelKey];
}

bool LWGpuResource::CreatePerformanceMonitor()
{
    for (size_t i = 0; i < LW2080_MAX_SUBDEVICES; i++)
    {
        m_PerfMonList[i].reset();
    }

    PerformanceMonitor::ClearZLwllIds();
    PerformanceMonitor::ClearSurfaces();

    for (UINT32 subdev = 0;
        subdev < GetGpuDevice()->GetNumSubdevices();
        subdev++)
    {
        m_PerfMonList[subdev].reset(new PerformanceMonitor(
            this, subdev, params.get()));
    }

    return true;
}

bool LWGpuResource::PerfMon() const
{
    return m_PerfMonList[0].get() != nullptr;
}

int LWGpuResource::PerfMonInitialize(const char *option_type, const char *name)
{
    for (size_t i = 0; (i<LW2080_MAX_SUBDEVICES) && (m_PerfMonList[i].get() != nullptr); ++i)
    {
        if (!m_PerfMonList[i]->Initialize(option_type, name))
        {
            return 0;
        }
    }

    return 1;
}

void LWGpuResource::PerfMonStart(LWGpuChannel* ch, UINT32 subch, UINT32 class3d)
{
    for (size_t i=0; (i<LW2080_MAX_SUBDEVICES) && (m_PerfMonList[i].get() != nullptr); ++i)
    {
        GpuSubdevice* pSubdev = m_PerfMonList[i]->GetSubDev();

        if (gXML && (m_PerfMonList[1].get() != nullptr) && pSubdev)
        {
            XD->XMLStartElement("<PmSubDev");
            XD->outputAttribute("devnum", (unsigned int)i);
            XD->endAttributes();
        }

        m_PerfMonList[i]->Start( ch, subch, class3d );

        if (gXML && (m_PerfMonList[1].get() != nullptr) && pSubdev)
        {
            XD->XMLEndLwrrent();
        }
    }
}

void LWGpuResource::PerfMonEnd(LWGpuChannel* ch, UINT32 subch, UINT32 class3d)
{
    for (size_t i=0; (i<LW2080_MAX_SUBDEVICES) && (m_PerfMonList[i].get() != nullptr); ++i)
    {
        GpuSubdevice* pSubdev = m_PerfMonList[i]->GetSubDev();

        if (gXML && (m_PerfMonList[1].get() != nullptr) && pSubdev)
        {
            XD->XMLStartElement("<PmSubDev");
            XD->outputAttribute("devnum", (unsigned int)i);
            XD->endAttributes();
        }

        m_PerfMonList[i]->End( ch, subch, class3d );

        if (gXML && (m_PerfMonList[1].get() != nullptr) && pSubdev)
        {
            XD->XMLEndLwrrent();
        }
    }
}

RC LWGpuResource::AllocSurfaceRC
(
    Memory::Protect Protect, UINT64 Bytes,
    _DMA_TARGET Target, _PAGE_LAYOUT Layout,
    LwRm::Handle hVASpace,
    UINT32 *Handle, uintptr_t *Buf, LwRm* pLwRm,
    UINT32 Attr, UINT64 *GpuOffset, UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */
)
{
    RC rc;

    Surface2D* surf = new Surface2D();
    switch (Layout)
    {
        case PAGE_PHYSICAL:
        case PAGE_VIRTUAL:
        case PAGE_VIRTUAL_4KB:
        case PAGE_VIRTUAL_64KB:
        case PAGE_VIRTUAL_2MB:
        case PAGE_VIRTUAL_512MB:
            break;
        default:
            MASSERT(!"Invalid Layout");
            return RC::BAD_PARAMETER;
    }
    surf->SetColorFormat(ColorUtils::Y8);
    surf->SetArrayPitch(Bytes);
    surf->SetLayout(Surface2D::Pitch);
    switch (Target)
    {
        case _DMA_TARGET_VIDEO:
            surf->SetLocation(Memory::Fb);
            break;
        case _DMA_TARGET_COHERENT:
            surf->SetLocation(Memory::Coherent);
            break;
        case _DMA_TARGET_NONCOHERENT:
            surf->SetLocation(Memory::NonCoherent);
            break;
        default:
            MASSERT(!"Invalid Target");
            return RC::BAD_PARAMETER;
    }
    surf->SetProtect(Protect);
    surf->SetGpuVASpace(hVASpace);
    CHECK_RC(surf->Alloc(GetGpuDevice(), pLwRm));
    CHECK_RC(surf->Map(subdev));

    // callers will have to do their own bind if needed.
    //surf->BindGpuChannel(m_GpuChannel->ChannelHandle());

    if (GpuOffset) *GpuOffset = surf->GetCtxDmaOffsetGpu();
    if (Handle) *Handle = surf->GetCtxDmaHandle();
    if (Buf) *Buf = (uintptr_t)surf->GetAddress();
    m_AllocSurfaces[*Buf] = surf;

    return OK;
}

void LWGpuResource::DeleteSurface(uintptr_t address)
{
    // Allow deleting a nonexistent buffer
    if (address == 0)
        return;

    map<uintptr_t, Surface2D*>::iterator iter = m_AllocSurfaces.find(address);
    if (iter != m_AllocSurfaces.end())
    {
        delete iter->second;
        m_AllocSurfaces.erase(iter);
    }
}

Surface2D* LWGpuResource::GetSurface(uintptr_t address)
{
    map<uintptr_t, Surface2D*>::iterator iter = m_AllocSurfaces.find(address);
    if (iter != m_AllocSurfaces.end())
    {
        return iter->second;
    }
    return 0;
}

RC LWGpuResource::CreateFlaVaSpace(UINT64 size, UINT64 base, LwRm *pLwRm)
{
    // Check if FLA VA space has been set up in GPU device.
    if (GetGpuDevice()->GetVASpace(FERMI_VASPACE_A,
            LW_VASPACE_ALLOCATION_INDEX_GPU_FLA,
            pLwRm) != 0)
    {
        ErrPrintf("%s: FLA VaSpace already allocated\n", __FUNCTION__);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    auto vaSpace = GetVaSpaceManager(pLwRm)->
        CreateResourceObject(TEST_ID_GLOBAL, FLA_VA_SPACE);
    vaSpace->SetIndex(VaSpace::Index::FLA);
    vaSpace->SetRange(base, size);
    DebugPrintf("%s: size=0x%llx base=0x%llx\n", __FUNCTION__, size, base);

    return OK;
}


RC LWGpuResource::ExportFlaRange(UINT64 size, const string &name, LwRm *pLwRm)
{
    RC rc;

    if (s_FlaExportGpuMap.find(name) != s_FlaExportGpuMap.end())
    {
        ErrPrintf("%s: name %s already in use\n", __FUNCTION__, name.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    LwRm::Handle hVaSpace = 0;
    auto vaSpace = GetVaSpaceManager(pLwRm)->GetResourceObject(
        TEST_ID_GLOBAL, FLA_VA_SPACE);
    if (vaSpace != nullptr)
    {
        hVaSpace = vaSpace->GetHandle();
    }
    else
    {
        // Check if FLA VA space has been set up in GPU device.
        hVaSpace = GetGpuDevice()->GetVASpace(FERMI_VASPACE_A,
            LW_VASPACE_ALLOCATION_INDEX_GPU_FLA,
            pLwRm);
    }

    if (hVaSpace == 0)
    {
        ErrPrintf("%s: No FLA VaSpace allocated\n", __FUNCTION__);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    MASSERT(m_FlaExportSurfaces[name] == nullptr);
    m_FlaExportSurfaces[name] = make_unique<MdiagSurf>();

    auto surface = m_FlaExportSurfaces[name].get(); 
    surface->SetGpuVASpace(hVaSpace);
    surface->SetLayout(Surface2D::Pitch);
    surface->SetColorFormat(ColorUtils::Y8);
    surface->SetForceSizeAlloc(true);
    surface->SetArrayPitch(size);
    surface->SetName(name);

    s_FlaExportGpuMap[name] = this;

    DebugPrintf("%s: name=%s size=0x%llx\n", __FUNCTION__, name.c_str(), surface->GetSize());

    return rc;
}

RC LWGpuResource::SetFlaExportPageSize(const string &name, const string &pageSizeStr)
{
    UINT32 pageSize = 0;

    if (pageSizeStr == "4KB")
    {
        pageSize = 4;
    }
    else if (pageSizeStr == "64KB")
    {
        pageSize = 64;
    }
    else if (pageSizeStr == "2MB")
    {
        pageSize = 2*1024;
    }
    else if (pageSizeStr == "512MB")
    {
        pageSize = 512*1024;
    }
    else
    {
        ErrPrintf("%s: Invalid page size %s specified for FLA export\n",
                  __FUNCTION__, pageSizeStr.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    auto surface = m_FlaExportSurfaces[name].get();
    if (surface == nullptr)
    {
        ErrPrintf("%s: name %s is not associated with an exported FLA range\n",
                  __FUNCTION__, name.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    surface->SetPageSize(pageSize);

    DebugPrintf("%s: name=%s pageSize=%u\n", __FUNCTION__, name.c_str(), pageSize);

    return OK;
}

RC LWGpuResource::SetFlaExportPteKind(const string &name, const char *pteKindStr)
{
    UINT32 pteKind = 0;
    const auto subdevice = GetGpuSubdevice();

    if (!subdevice->GetPteKindFromName(pteKindStr, &pteKind))
    {
        ErrPrintf("%s: Invalid PTE kind %s specified for FLA export\n", __FUNCTION__, pteKindStr);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    auto surface = m_FlaExportSurfaces[name].get();
    if (surface == nullptr)
    {
        ErrPrintf("%s: name %s is not associated with an exported FLA range\n", __FUNCTION__, name.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    surface->SetPteKind(pteKind);

    DebugPrintf("%s: name=%s pteKind=%u\n", __FUNCTION__, name.c_str(), pteKind);

    return OK;
}

RC LWGpuResource::SetFlaExportAcr(const string &name, const string &value, UINT32 index)
{
    bool setting = false;

    if (value == "ON")
    {
        setting = true;
    }
    else if (value == "OFF")
    {
        setting = false;
    }
    else
    {
        ErrPrintf("Unrecognized ACR value %s. Valid options are ON or OFF.\n", value.c_str());

        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    auto surface = m_FlaExportSurfaces[name].get();
    if (surface == nullptr)
    {
        ErrPrintf("%s: name %s is not associated with an exported FLA range\n", __FUNCTION__, name.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    switch (index)
    {
        case 1:
            surface->SetAcrRegion1(setting);
            break;
        case 2:
            surface->SetAcrRegion2(setting);
            break;
        default:
            ErrPrintf("Illegal ACR index %u\n", index);
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC LWGpuResource::SetFlaExportAperture(const string &name, const string &aperture)
{
    auto surface = m_FlaExportSurfaces[name].get();
    if (surface == nullptr)
    {
        ErrPrintf("%s: name %s is not associated with an exported FLA range\n", __FUNCTION__, name.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (aperture == "VIDEO")
    {
        surface->SetLocation(Memory::Fb);
    }
    else if (aperture == "COHERENT_SYSMEM")
    {
        surface->SetLocation(Memory::Coherent);
    }
    else if (aperture == "NONCOHERENT_SYSMEM")
    {
        surface->SetLocation(Memory::NonCoherent);
    }
    else if (aperture == "P2P")
    {
        if (params->ParamPresent("-sli_p2ploopback"))
        {
            surface->SetLoopBack(true);
        }
        else
        {
            ErrPrintf("%s: Only loopback is supported for P2P aperture for FLA export. Use -sli_p2ploopback to enable loopback.\n",
                      __FUNCTION__);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        surface->SetLocation(Memory::Fb);
    }
    else
    {
        ErrPrintf("%s: Unrecognized aperture %s specified for FLA export\n", __FUNCTION__, aperture.c_str());

        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return OK;
}

RC LWGpuResource::SetFlaExportPeerId(const string &name, UINT32 peerId)
{
    auto surface = m_FlaExportSurfaces[name].get();
    if (surface == nullptr)
    {
        ErrPrintf("%s is not associated with an exported FLA range\n", name.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (surface->GetLoopBack())
    {
        surface->SetLoopBackPeerId(peerId);
    }
    else
    {
        ErrPrintf("%s is not mapped to peer. Use '-fla_export_aperture %s P2P' to enable peer mapping.\n",
            name.c_str(), name.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return OK;
}

RC LWGpuResource::SetFlaExportEgm(const string &name)
{
    auto surface = m_FlaExportSurfaces[name].get();
    if (surface == nullptr)
    {
        WarnPrintf("%s is not associated with an exported FLA range\n", name.c_str());
        return OK;
    }

    if (Platform::IsSelfHosted())
    {
        if (!surface->IsOnSysmem())
        {
            ErrPrintf("%s is not under the sysmem for SHH. "
                    "Please add -fla_export_aperture to specify it\n", name.c_str());
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        surface->SetLocation(Memory::Fb);
    }
    surface->SetEgmAttr();
    return OK;
}

RC LWGpuResource::AllocFlaExportSurfaces(LwRm *pLwRm)
{
    RC rc;

    // Iterate over each FLA export surface (mapped name->surface) on this GPU
    for (const auto &kv : m_FlaExportSurfaces)
    {
        auto *pSurface = kv.second.get();
        CHECK_RC(pSurface->Alloc(GetGpuDevice(), pLwRm));
        PHYSADDR physAddr = 0;
        pSurface->GetPhysAddress(0, &physAddr);
        InfoPrintf("%s: Name=%s, FlaAddr=0x%llx, PhysAddr=0x%llx\n",
                   __FUNCTION__, pSurface->GetName().c_str(), pSurface->GetCtxDmaOffsetGpu(), physAddr);
    }

    return rc;
}

set<LWGpuResource*> LWGpuResource::GetFlaExportGpus()
{
    set<LWGpuResource*> gpus;

    // Iterate over all FLA export surfaces (mapped surface name->GPU)
    for (const auto &kv : s_FlaExportGpuMap)
    {
        gpus.insert(kv.second);
    }

    return gpus;
}

set<MdiagSurf*> LWGpuResource::GetFlaExportSurfacesLocal()
{
    set<MdiagSurf*> surfaces;

    // Iterate over each FLA export surface (mapped name->surface) on this GPU
    for (const auto &kv : m_FlaExportSurfaces)
    {
        surfaces.insert(kv.second.get());
    }

    return surfaces;
}

set<MdiagSurf*> LWGpuResource::GetFlaExportSurfaces()
{
    const set<LWGpuResource*> gpus = GetFlaExportGpus();
    set<MdiagSurf*> surfaces;

    for (const auto &gpu : gpus)
    {
        const set<MdiagSurf*> gpuSurfaces = gpu->GetFlaExportSurfacesLocal();
        surfaces.insert(gpuSurfaces.begin(), gpuSurfaces.end());
    }

    return surfaces;
}

MdiagSurf* LWGpuResource::GetFlaExportSurfaceLocal(const string &name)
{
    return m_FlaExportSurfaces[name].get();
}

MdiagSurf* LWGpuResource::GetFlaExportSurface(const string &name)
{
    if (s_FlaExportGpuMap.find(name) == s_FlaExportGpuMap.end())
    {
        return nullptr;
    }
    return s_FlaExportGpuMap[name]->GetFlaExportSurfaceLocal(name);
}

//! All Interrupt hooks are deprecated.  Please do not add any additional
//! code that uses them.  Please do not add any more functions that install
//! callbacks with the Resource Manager, that approach does not work on
//! real operating systems where the RM lives in the kernelx
//------------------------------------------------------------------------------
bool LWGpuResource::MdiagGrIntrHook(UINT32 GrIdx, UINT32 GrIntr, UINT32 ClassError,
                                    UINT32 Addr, UINT32 DataLo,
                                    void *CallbackData)
{
    char buf[1024];

    // Log the error in the legacy error format
    sprintf(buf, "[GrIdx:0x%08x]gr_intr=0x%08x class_error=0x%08x trapped addr=0x%08x data_lo=0x%08x\n",
            GrIdx, GrIntr, ClassError, Addr, DataLo);
    ErrorLogger::LogError(buf, ErrorLogger::LT_ERROR);

    // Tell the RM to clear the interrupt and continue
    return true;
}

bool LWGpuResource::SkipIntrCheck()
{
    return (params->ParamPresent("-skip_intr_check") != 0);
}

UINT32 LWGpuResource::GetArchitecture
(
    UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */
) const
{
    // Happy coincidence of enumerants
    return GetGpuSubdevice(subdev)->DeviceId();
}

IGpuSurfaceMgr *LWGpuResource::CreateGpuSurfaceMgr(Trace3DTest* pTest, const ArgReader *params, LWGpuSubChannel* in_sch)
{
    return new LWGpuSurfaceMgr(pTest, this, in_sch, params);
}

UINT64 LWGpuResource::GetFBMemSize
(
    UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */
)
{
    MASSERT(GetGpuSubdevice(subdev)->FbApertureSize() != (UINT64)(Gpu::IlwalidFbApertureSize));
    return GetGpuSubdevice(subdev)->FbApertureSize();
}

unique_ptr<IRegisterClass> LWGpuResource::GetRegister
(
    const char* name,
    UINT32 subdev, /* = Gpu::UNSPECIFIED_SUBDEV */
    UINT32 *pRegIdxI, /* = NULL */
    UINT32 *pRegIdxJ  /* = NULL */
)
{
    IRegisterMap *RegMap = GetRegisterMap(subdev);
    return RegMap->FindRegister(name, pRegIdxI, pRegIdxJ);
}

unique_ptr<IRegisterClass> LWGpuResource::GetRegister
(
    UINT32 address,
    UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */
)
{
    IRegisterMap *RegMap = GetRegisterMap(subdev);
    return RegMap->FindRegister(address);
}

IRegisterMap* LWGpuResource::GetRegisterMap
(
    UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */
)
{
    if (!m_RegMap)
    {
        RefManual *Manual = GetGpuSubdevice(subdev)->GetRefManual();
        m_RegMap = CreateRegisterMap(Manual);
    }

    return m_RegMap.get();
}

RC LWGpuResource::ReinitializeRegSettings()
{
    StickyRC src;

    src = HandleOptionalRegisters(&m_mdiagPrivRegs);
    src = HandleOptionalRegisters(&m_confRegControls);
    //TODO: do we need to reinit display Regs (see HandleDisplayConfig)?

    return src;
}

void LWGpuResource::AddCtxOverride(UINT32 addr, UINT32 andMask, UINT32 orMask)
{
    CtxOverrides::iterator iter = m_CtxOverrides.begin();
    for (; iter != m_CtxOverrides.end(); iter++)
    {
        if (iter->first == addr)
        {
            break;
        }
    }

    if (iter == m_CtxOverrides.end())
    {
        m_CtxOverrides.push_back(make_pair(addr, make_pair(andMask, orMask)));
    }
    else
    {
        if ((andMask & iter->second.first & orMask) != 
            (andMask & iter->second.first & iter->second.second))
        {
            WarnPrintf(
                "Conflicting masks (andMasks=0x%x 0x%x, orMasks = 0x%x 0x%x) have been specified with multiple -reg args for addr 0x%x. "
                "MODS will write them in sequence and override the context register by doing an OR of the two masks.\n",
                andMask, iter->second.first, orMask, iter->second.second, addr);
        }
        iter->second = make_pair(andMask | iter->second.first, orMask | iter->second.second);
    }
}

RC LWGpuResource::HandleRegisterAction
(
    CRegisterControl* regc,
    UINT32 subdev, /* = Gpu::UNSPECIFIED_SUBDEV */
    SmcEngine *pSmcEngine /* = nullptr */
)
{
    RC rc;
    UINT32 rd_data = 0;
    GpuSubdevice *pSubdev = GetGpuSubdevice(subdev);
    LwRm *pLwRm = pSmcEngine ? GetLwRmPtr(pSmcEngine) : LwRmPtr().Get();
    string smcEngineName(pSmcEngine ? pSmcEngine->GetName() : "n/a");

    InfoPrintf("HandleRegisterAction:\n");
    regc->PrintRegInfo();
    
    string regName;
    if (regc->m_regPtr)
    {
        regName = regc->m_regPtr->GetName();
    }

    if (regc->m_domain != RegHalDomain::RAW)
    {
        // RegHalDomain::RAW means the regsiter does not need to add address base.
        // So after add the address base, change the domain to RAW in case it will be handled again.
        UINT32 domainIdx = MDiagUtils::GetDomainIndex(regc->m_regSpace);
        regc->m_address += pSubdev->GetDomainBase(domainIdx, regc->m_domain, pLwRm);
        regc->m_domain = RegHalDomain::RAW;
    }
    
    // LW_PGRAPH_PRI_GPC[%u] registers are zero base, but the domain is RegHalDomain::RAW as a seperate domain is
    // not available for these registers. MDiagUtils::GetDomainBase in this case returns the register offset 
    // callwlated using (gpcIndex * gpcStride + tpcIndex * tpcStride).
    if (std::regex_search(regName, std::regex(MDiagUtils::s_LWPgraphGpcPrefix + "[0-9]+")))
    {
        regc->m_address += MDiagUtils::GetDomainBase(regName.c_str(), regc->m_regSpace.c_str(), pSubdev, pLwRm);
    }
    
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();

    switch (regc->m_action)
    {
        case CRegisterControl::EWRITE:
        case CRegisterControl::EARRAY1WRITE:
        case CRegisterControl::EARRAY2WRITE:
            if (m_RestorePrivReg)
            {
                if(regc->m_isConfigSpace)
                {
                    Platform::PciRead32(pGpuPcie->DomainNumber(), pGpuPcie->BusNumber(), pGpuPcie->DeviceNumber(), pGpuPcie->FunctionNumber(), regc->m_address, &rd_data);
                    pSubdev->SetConfRegDuringShutdown(regc->m_address, rd_data);
                }
                // not guaranteed to work for multiple-GPU!
                if (!IsSMCEnabled() || !IsPgraphReg(regc->m_address))
                {
                    //No need to retore for PGRAPH register in SMC mode
                    rd_data = RegRd32(regc->m_address, subdev, pSmcEngine);
                    pSubdev->SetRegDuringShutdown(regc->m_address, rd_data);
                }
            }

            if(regc->m_isConfigSpace)
            {
                Platform::PciWrite32(pGpuPcie->DomainNumber(), pGpuPcie->BusNumber(), pGpuPcie->DeviceNumber(), pGpuPcie->FunctionNumber(), regc->m_address, regc->m_data);
                InfoPrintf("results: Written via config cycles: wr_addr=0x%08x, wr_data=0x%08x\n",
                       regc->m_address, regc->m_data);
            }                
            
            else
            {
                // Ask RM to modify the register's init value in gr ctx buffer if it is ctx switched.
                if (OK != SetContextOverride(regc->m_address, 0xFFFFFFFF, regc->m_data, pSmcEngine))
                {
                    DebugPrintf("SetContextOverride failed : regAddr=0x%08x, andMask=0xFFFFFFFF, orMask=0x%08x, SmcEngine=%s",
                        regc->m_address, regc->m_data, smcEngineName.c_str());
                }
                RegWr32(regc->m_address, regc->m_data, subdev, pSmcEngine);
                InfoPrintf("results: wr_addr=0x%08x, wr_data=0x%08x\n",
                           regc->m_address, regc->m_data);
            }
            break;
        case CRegisterControl::EREAD:
        case CRegisterControl::EARRAY1READ:
        case CRegisterControl::EARRAY2READ:
            if(regc->m_isConfigSpace)
            {
                Platform::PciRead32(pGpuPcie->DomainNumber(), pGpuPcie->BusNumber(), pGpuPcie->DeviceNumber(), pGpuPcie->FunctionNumber(), regc->m_address, &(regc->m_data));
                regc->m_data = regc->m_data & regc->m_mask;
                InfoPrintf("results: Read via config cycles: rd_data=0x%08x\n", regc->m_data);
            }
            else
            {    
                regc->m_data = RegRd32(regc->m_address, subdev, pSmcEngine) & regc->m_mask;
                InfoPrintf("results: rd_data=0x%08x\n", regc->m_data);
            }
            break;
        case CRegisterControl::EMODIFY:
        case CRegisterControl::EARRAY1MODIFY:
        case CRegisterControl::EARRAY2MODIFY:
            if(regc->m_isConfigSpace)
            {
                Platform::PciRead32(pGpuPcie->DomainNumber(), 
                    pGpuPcie->BusNumber(), pGpuPcie->DeviceNumber(), 
                    pGpuPcie->FunctionNumber(), regc->m_address, &rd_data);

                if (m_RestorePrivReg)
                {
                    pSubdev->SetConfRegDuringShutdown(regc->m_address, rd_data);  
                }
                
                UINT32 wr_data = ((regc->m_data) & (regc->m_mask)) | (rd_data & ~(regc->m_mask));
                InfoPrintf("results: Modify via config cycles rd_data=0x%08x  wr_data=0x%08x\n", 
                    rd_data, wr_data);
                
                Platform::PciWrite32(pGpuPcie->DomainNumber(), 
                    pGpuPcie->BusNumber(), pGpuPcie->DeviceNumber(), 
                    pGpuPcie->FunctionNumber(), regc->m_address, wr_data);
            }
            
            else
            {
                if (regc->m_address == LW_PBUS_FS)
                {
                    // Can't allow overrides of PBUS_FS register here because the RM has
                    // already initialized.  Hangs will result.
                    ErrPrintf("Overriding PBUS_FS register not supported!\n");
                    rc = RC::BAD_PARAMETER;
                }
                
                // Ask RM to modify the register's init value in gr ctx buffer if it is ctx switched.
                AddCtxOverride(regc->m_address, regc->m_mask, regc->m_data);
                
                if (m_RestorePrivReg)
                {
                    // not guaranteed to work for multiple-GPU!
                    if (!IsSMCEnabled() || !IsPgraphReg(regc->m_address))
                    {
                        //No need to retore for PGRAPH register in SMC mode
                        pSubdev->SetRegDuringShutdown(regc->m_address,
                                RegRd32(regc->m_address, subdev, pSmcEngine));
                    }
                }
            }
            break;
        case CRegisterControl::ECOMPARE:
            if(regc->m_isConfigSpace)
            {
                Platform::PciRead32(pGpuPcie->DomainNumber(), pGpuPcie->BusNumber(), pGpuPcie->DeviceNumber(), pGpuPcie->FunctionNumber(), regc->m_address, &rd_data);
                rd_data = rd_data & regc->m_mask;
                InfoPrintf("results: 0x%08x=0x%08x\n", regc->m_data, rd_data);
            }
            
            else
            {                
                rd_data = RegRd32(regc->m_address, subdev, pSmcEngine) & regc->m_mask;
                InfoPrintf("results: 0x%08x=0x%08x\n", regc->m_data, rd_data);
            }
            if (rd_data != regc->m_data)
            {
                ErrPrintf("Register read/compare failed: addr=0x%08x expected=0x%08x actual=0x%08x\n",
                          regc->m_address, regc->m_data, rd_data);
                rc = RC::DATA_MISMATCH;
            }
            break;
        default:
            DebugPrintf("Ignoring unknown register action\n");
            break;
    }
    if (rc != OK)
    {
        ErrPrintf("Handling the following register and action failed:\n");
        regc->PrintRegInfo();
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LWGpuResource::HandleJsonDisplayConfig(ArgReader *params)
{
    RC rc;

    if (params->ParamPresent("-disable_display"))
    {
        ErrPrintf("Display is disabled, but a JSON display configuration was supplied\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    m_pDispPipeConfigurator = make_unique<DispPipeConfig::DispPipeConfigurator>();
    m_pDispPipeConfigurator->SetLwRmClient(LwRmPtr().Get());

    Display *pDisplay = GetGpuDevice()->GetDisplay();
    if (params->ParamPresent("-disp_poll_awake"))
    {
        pDisplay->PollForDisplayAwake(true);
    }

    if (params->ParamPresent("-timeout_ms"))
    {
        CHECK_RC(pDisplay->SetDefaultTimeoutMs(params->ParamFloat("-timeout_ms")));
    }

    if (params->ParamPresent("-disp_ignore_imp"))
    {
        pDisplay->SetIgnoreIMP(true);
    }

    if (params->ParamPresent("-disp_perform_crc_count_check"))
    {
        CHECK_RC(m_pDispPipeConfigurator->SetPerformCrcCountCheck(true));
    }
    else
    {
        // The value is explicitly set to true in MODS, so we need to set it
        // to false if arg is not specified
        CHECK_RC(m_pDispPipeConfigurator->SetPerformCrcCountCheck(false));
    }

    if (params->ParamPresent("-disp_cfg_json_enable_prefetch"))
    {
        CHECK_RC(m_pDispPipeConfigurator->SetEnablePrefetch(true));
    }

    if (params->ParamPresent("-disp_cfg_json"))
    {
        const char * dpcFile = params->ParamStr("-disp_cfg_json");
        MASSERT(dpcFile);
        if (params->ParamPresent("-disp_cfg_json_debug"))
        {
            CHECK_RC(m_pDispPipeConfigurator->SetPrintPriority(Tee::PriNormal));
        }
        if (params->ParamPresent("-disp_cfg_json_dump_luts"))
        {
            CHECK_RC(m_pDispPipeConfigurator->SetEnableLUTDumps(true));
        }
        m_pDispPipeConfigurator->SetBoundGpuDevice(GetGpuDevice());
        CHECK_RC(m_pDispPipeConfigurator->Setup(true, dpcFile));
    }

    if (params->ParamPresent("-disp_skip_crc"))
    {
        CHECK_RC(m_pDispPipeConfigurator->SetSkipCRCCapture(true));
    }

    if (params->ParamPresent("-disp_skip_crc_test"))
    {
        CHECK_RC(m_pDispPipeConfigurator->SetSkipCRCTest(true));
    }

    if (params->ParamPresent("-disp_skip_head_detach"))
    {
        CHECK_RC(m_pDispPipeConfigurator->SetSkipHeadDetach(true));
    }

    if (params->ParamPresent("-disp_cfg_json_dump_surfaces"))
    {
        const char * dumpType = params->ParamStr("-disp_cfg_json_dump_surfaces");
        if (!strcmp(dumpType, "png"))
        {
            CHECK_RC(m_pDispPipeConfigurator->SetDumpSurfaceType(
                DispPipeConfig::DispPipeConfigurator::DumpSurfaceType::PNG));
        }
        else if (!strcmp(dumpType, "tga"))
        {
            CHECK_RC(m_pDispPipeConfigurator->SetDumpSurfaceType(
                DispPipeConfig::DispPipeConfigurator::DumpSurfaceType::TGA));
        }
        else if (!strcmp(dumpType, "text"))
        {
            CHECK_RC(m_pDispPipeConfigurator->SetDumpSurfaceType(
                DispPipeConfig::DispPipeConfigurator::DumpSurfaceType::TEXT));
        }
        else
        {
            ErrPrintf("-disp_cfg_json_dump_surfaces only supports png/tga/text\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    bool syncScanoutPresent = (params->ParamPresent("-sync_scanout") != 0);
    bool regOvrdPresent = (params->ParamPresent("-disp_reg_ovrd_pre") != 0) ||
                          (params->ParamPresent("-disp_reg_ovrd") != 0) ||
                          (params->ParamPresent("-disp_reg_ovrd_post") != 0);
    bool extTriggerModePresent = (params->ParamPresent("-disp_cfg_json_ext_trigger_mode") != 0);
    bool exelwteAtEndPresent = (params->ParamPresent("-disp_cfg_json_exelwte_at_end") != 0);

    // When "-sync_scanout" is present there is no need to wait for notifier
    // as later a trace will be synchronized to beginning of scanout anyway.
    // But if somebody requests register overrides we need to wait for SetModes
    // to complete here.

    if (exelwteAtEndPresent &&
       (extTriggerModePresent || syncScanoutPresent))
    {
        ErrPrintf("DPC:: disp_cfg_json_exelwte_at_end can't be used in conjunction with sync_scanout/disp_cfg_json_ext_trigger_mode.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (params->ParamPresent("-disp_skip_crc")
        && params->ParamPresent("-disp_perform_crc_count_check"))
    {
        ErrPrintf("DPC:: disp_skip_crc cannot be used with disp_perform_crc_count_check.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    bool waitForNotifier = regOvrdPresent || !syncScanoutPresent;
    m_pDispPipeConfigurator->SetWaitForNotifier(waitForNotifier);

    if (extTriggerModePresent || syncScanoutPresent)
    {
        CHECK_RC(m_pDispPipeConfigurator->SetExtTriggerMode(true));
        CHECK_RC(m_pDispPipeConfigurator->ExtTriggerModeSetup());
        CHECK_RC(m_pDispPipeConfigurator->ExtTriggerModeStart());
    }
    else if (exelwteAtEndPresent)
    {
        CHECK_RC(m_pDispPipeConfigurator->SetExtTriggerMode(true));
        CHECK_RC(m_pDispPipeConfigurator->SetExelwteAtEnd(true));
        CHECK_RC(m_pDispPipeConfigurator->ExtTriggerModeSetup());
    }
    else
    {
        CHECK_RC(m_pDispPipeConfigurator->Run());
    }

    return rc;
}
//------------------------------------------------------------------------------
RC LWGpuResource::HandleFlaConfig(LwRm *pLwRm)
{
    RC rc;

    // No RM support for FLA on Amodel
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        return rc;
    }

    if (params->ParamPresent("-fla_alloc"))
    {
        const auto size = params->ParamNUnsigned64("-fla_alloc", 0);
        const auto base = params->ParamNUnsigned64("-fla_alloc", 1);
        CHECK_RC(CreateFlaVaSpace(size, base, pLwRm));
    }

    for (UINT32 i = 0; i < params->ParamPresent("-fla_export"); ++i)
    {
        const auto size = params->ParamNUnsigned64("-fla_export", i, 0);
        const auto name = params->ParamNStr("-fla_export", i, 1);
        CHECK_RC(ExportFlaRange(size, name, pLwRm));
    }

    for (UINT32 i = 0; i < params->ParamPresent("-fla_export_page_size"); ++i)
    {
        const auto name = params->ParamNStr("-fla_export_page_size", i, 0);
        const auto pageSize =
            string(params->ParamNStr("-fla_export_page_size", i, 1));
        CHECK_RC(SetFlaExportPageSize(name, pageSize));

    }

    for (UINT32 i = 0; i < params->ParamPresent("-fla_export_pte_kind"); ++i)
    {
        const auto name = params->ParamNStr("-fla_export_pte_kind", i, 0);
        const auto pteKind = params->ParamNStr("-fla_export_pte_kind", i, 1);
        CHECK_RC(SetFlaExportPteKind(name, pteKind));

    }

    for (UINT32 i = 0; i < params->ParamPresent("-fla_export_acr1"); ++i)
    {
        const auto name = params->ParamNStr("-fla_export_acr1", i, 0);
        const auto acr = params->ParamNStr("-fla_export_acr1", i, 1);
        CHECK_RC(SetFlaExportAcr(name, acr, 1));
    }

    for (UINT32 i = 0; i < params->ParamPresent("-fla_export_acr2"); ++i)
    {
        const auto name = params->ParamNStr("-fla_export_acr2", i, 0);
        const auto acr = params->ParamNStr("-fla_export_acr2", i, 1);
        CHECK_RC(SetFlaExportAcr(name, acr, 2));
    }

    for (UINT32 i = 0; i < params->ParamPresent("-fla_export_aperture"); ++i)
    {
        const auto name = params->ParamNStr("-fla_export_aperture", i, 0);
        const auto aperture = params->ParamNStr("-fla_export_aperture", i, 1);
        CHECK_RC(SetFlaExportAperture(name, aperture));
    }

    for (UINT32 i = 0; i < params->ParamPresent("-fla_export_peer_id"); ++i)
    {
        const auto name = params->ParamNStr("-fla_export_peer_id", i, 0);
        const auto peerId = params->ParamNUnsigned("-fla_export_peer_id", i, 1);
        CHECK_RC(SetFlaExportPeerId(name, peerId));
    }

    // egm support
    for (UINT32 i = 0; i < params->ParamPresent("-fla_egm_alloc"); ++i)
    {
        const auto name = params->ParamNStr("-fla_egm_alloc", i, 0);
        CHECK_RC(SetFlaExportEgm(name));
    }

    CHECK_RC(AllocFlaExportSurfaces(pLwRm));

    if (Utl::HasInstance())
    {
        const auto flaExportSurfaces = GetFlaExportSurfacesLocal();
        for (const auto pSurface : flaExportSurfaces)
        {
            Utl::Instance()->AddGpuSurface(GetGpuSubdevice(), pSurface);
        }
    }

    return rc;
}

RC LWGpuResource::SetContextOverride
(
    UINT32 regAddr,
    UINT32 andMask,
    UINT32 orMask,
    SmcEngine *pSmcEngine
)
{
    RC rc;
    GpuDevice *pGpuDev = GetGpuDevice();

    if (Platform::IsVirtFunMode())
    {
        WarnPrintf(
            "Skipping SetContextOverride on VF: regAddr=0x%08x, andMask=0x%08x, orMask=0x%08x\n",
            regAddr, andMask, orMask);
        return rc;
    }

    if (!IsSMCEnabled())
    {
        // legacy mode. No route info is needed.
        return pGpuDev->SetContextOverride(regAddr, andMask, orMask);
    }
    else if (pSmcEngine)
    {
        LwRm *pLwRm = GetLwRmPtr(pSmcEngine);
        MASSERT(pLwRm);
        rc = pGpuDev->SetContextOverride(regAddr, andMask, orMask, pLwRm);
    }
    else
    {
        // need to broadcast to all SMC engines;
        const vector<GpuPartition*>& gpuPartitions = m_pSmcResourceController->GetActiveGpuPartition();
        for (const auto& gpuPartition : gpuPartitions)
        {
            const vector<SmcEngine*>& smcEngines = gpuPartition->GetActiveSmcEngines();
            for (const auto& smcEngine : smcEngines)
            {
                LwRm *pLwRm = GetLwRmPtr(smcEngine);
                MASSERT(pLwRm);
                CHECK_RC(pGpuDev->SetContextOverride(regAddr, andMask, orMask, pLwRm));
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// Handle a list of regCtrl.
// 1. If SMC is not enabled or it's non-PGRAPH, pSmcEngine and pLwRm are not needed
// 2. Otherwise, pSmcEngine and pLwRm should be provided.
// return false: if no failure
//        true:  if failure
RC LWGpuResource::HandleOptionalRegisters
(
    vector<CRegisterControl>* regvec,
    UINT32 subdevInst, /* = Gpu::UNSPECIFIED_SUBDEV */
    SmcEngine* pSmcEngine /* = nullptr */
)
{
    StickyRC src;
    string smcEngineName(pSmcEngine ? pSmcEngine->GetName() : "n/a");

    if (regvec->size() == 0)
        return OK;

    for (auto& reg : *regvec)
    {
        src = HandleRegisterAction(&reg, subdevInst, pSmcEngine);
    }

    for (auto& ctxOverride : m_CtxOverrides)
    {
        UINT32 addr = ctxOverride.first;
        UINT32 andMask = ctxOverride.second.first;
        UINT32 orMask =  ctxOverride.second.second;

        RC rc = SetContextOverride(addr, andMask, orMask, pSmcEngine);

        if (rc != OK)
        {
            DebugPrintf("SetContextOverride failed. %s: regAddr=0x%08x, andMask=0x%08x, orMask=0x%08x, SmcEngine=%s",
                rc.Message(), addr, andMask, orMask, smcEngineName.c_str());

            UINT32 rd_data = RegRd32(addr, subdevInst, pSmcEngine);
            UINT32 wr_data = (orMask & andMask) | (rd_data& ~andMask);
            InfoPrintf("results: rd_data=0x%08x  wr_data=0x%08x\n", rd_data, wr_data);

            for (UINT32 subdev = 0; subdev < GetGpuDevice()->GetNumSubdevices(); ++subdev)
            {
                RegWr32(addr, wr_data, subdev, pSmcEngine);
            }
        }
    }

    m_CtxOverrides.clear();
    return src;
}

RC LWGpuResource::ParseRegBlock(const ArgReader *params, const char *usage, const char *block, multimap<string, tuple<ArgDatabase*, ArgReader*>> *argMap, const ParamDecl *paramList)
{
    if (!params->ParamPresent(block))
        return OK;

    // process the list of -conf/-priv options, as there may be more than one
    const vector<string>* cmdLines = params->ParamStrList(block, NULL);
    if (!cmdLines || cmdLines->empty())
        return OK;

    for (auto& cmdLwr : *cmdLines)
    {
        ArgDatabase *argDB = new ArgDatabase();
        ArgReader *argReader = new ArgReader(paramList);

        string regSpace = ParseSingleRegBlock(cmdLwr, usage, block, argDB, argReader);
        if (!argReader->ParseArgs(argDB))
        {
            ErrPrintf("LWGpuResource::LWGpuResource: ParseArgs failed for %s\n", cmdLwr.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        argMap->insert(make_pair(regSpace, make_tuple(argDB, argReader)));
    }

    return OK;
}

RC LWGpuResource::ParseRegBlock(const ArgReader *params, const char *usage, const char *block, unique_ptr<ArgDatabase> *argDB, unique_ptr<ArgReader> *argReader, const ParamDecl *paramList)
{
    argDB->reset(new ArgDatabase());
    argReader->reset(new ArgReader(paramList));

    if (!params->ParamPresent(block))
        return OK;

    // process the list of -conf/-priv options, as there may be more than one
    const vector<string>* cmdLines = params->ParamStrList(block, NULL);
    if (!cmdLines || cmdLines->empty())
        return OK;


    for (auto& cmdLwr : *cmdLines)
    {
        string regSpace = ParseSingleRegBlock(cmdLwr, usage, block, argDB->get(), argReader->get());

        if (!(*argReader)->ParseArgs(argDB->get()))
        {
            ErrPrintf("LWGpuResource::LWGpuResource: ParseArgs failed for %s\n", cmdLwr.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    return OK;
}

string  LWGpuResource::ParseSingleRegBlock(const string &cmd, const char *usage, const char *block, ArgDatabase *argDB, ArgReader *argReader)
{
    bool confReg = strncmp(block, "-conf", 5)==0;
    size_t start = 0;
    string regSpace = "";
    // These registers should not be used with -conf
    // Todo: Remove those register
    static const set<string> s_IgnoreRegSpaces = 
    {
        "lw41 ",
        "lw43 ",
        "lw44 ",
        "lw46 ",
        "lw47 ",
        "lw4x "
    };
    static const set<string> s_RegSpaces = 
    {
        "LW_ENGINE",
        "LW_LWLINK", 
        "LW_CONTEXT",
        "LW_XTL_PRI",
        "LW_XPL",
        MDiagUtils::s_LWPgraphGpcPrefix
    };

    while (start < cmd.size() && isspace(cmd[start]))
        start++; // skip leading white space

    // Search in IgnoreRegSpaces and return ""
    for (const auto& ignoreReg : s_IgnoreRegSpaces)
    {
        size_t regSpaceLen = ignoreReg.size();
        if(!confReg && cmd.compare(start, regSpaceLen, ignoreReg) == 0)
        {
            if (OK != argDB->AddArgLine(&cmd[start + regSpaceLen],
                        ArgDatabase::ARG_MUST_USE))
            {
                MASSERT(!"Adding line to m_privArgDB failed\n");
            }
            return regSpace;
        }
    }
    // Search in regSpaces and return full regSpace
    for (const auto& reg : s_RegSpaces)
    {
        size_t regSpaceLen = reg.size();
        if(!confReg && cmd.compare(start, regSpaceLen, reg) == 0)
        {
            // extract regSpace
            size_t regSpaceStart = start;
            size_t regSpaceEnd = start + regSpaceLen;

            while (regSpaceEnd < cmd.size()  && !isspace(cmd[regSpaceEnd]))
                regSpaceEnd++;

            regSpace = cmd.substr(regSpaceStart, regSpaceEnd);

            if (OK != argDB->AddArgLine(&cmd[regSpaceEnd], ArgDatabase::ARG_MUST_USE))
            {
                MASSERT(!"Adding line to m_privArgDB failed\n");
            }
            // return full regSpace
            return regSpace;
        }
    }
    if (cmd.compare(start, 4, "gpu ") == 0)
    {
        if (OK != argDB->AddArgLine(&cmd[start + 4],
                    ArgDatabase::ARG_MUST_USE))
        {
            MASSERT(!"Adding line to m_privArgDB failed\n");
        }
    }
    else
    {
        // Else didn't find the block
        ErrPrintf("Unrecognized %s block: %s\n", block, start);
        ErrPrintf("%s", usage);
        MASSERT(0);
    }

    return regSpace;
}

RC LWGpuResource::ProcessRegBlock
(
    const ArgReader *params,
    const char* paramName,
    vector<CRegisterControl>& regVec
)
{
    StickyRC src;
    if (params->ParamPresent(paramName))
    {
        const vector<string>* regCmdLines = params->ParamStrList(paramName, NULL);
        if (regCmdLines && !regCmdLines->empty())
        {
            auto regCmdLwr = regCmdLines->begin();

            while (regCmdLwr != regCmdLines->end())
            {
                src = ProcessOptionalRegisters(&regVec, regCmdLwr->c_str());
                regCmdLwr++;
            }
        }
    }

    return src;
}

void LWGpuResource::ParseHandleOptionalRegisters(const ArgReader* params, GpuSubdevice* pSubdev)
{
    // -no_pri_restore needs to be handled before all register writing
    // options because it sets a data member (m_RestorePrivReg)
    // that is referenced in LWGpuResource::HandleRegisterAction() (which
    // is called by the -reg handler
    //
  
    // Default is to not restore PriReg
    // Restore PriReg for Silicon/Emulation always or
    // when -pri_restore is specified
    m_RestorePrivReg = (Platform::GetSimulationMode() == Platform::Hardware) ||
                        (params->ParamPresent("-pri_restore"));

    if ((Platform::GetSimulationMode() == Platform::Hardware) && 
        (params->ParamPresent("-no_pri_restore")))
    {
        WarnPrintf("Option -no_pri_restore ignored on silicon/emulator\n");
    }

    if (m_RestorePrivReg)
    {
        DebugPrintf("%s: Restore PRI reg values updated by -reg W or M\n", __FUNCTION__);
    }

    StickyRC src; 
    // process the list of -reg options, as there may be more than one
    src = ProcessRegBlock(params, "-reg", m_privRegControls);
    for (auto reg : m_privRegControls)
    {
        string regName;
        
        if (reg.m_regPtr)
        {
            regName = reg.m_regPtr->GetName();
        }
        if (reg.m_domain != RegHalDomain::RAW)
        {
            ErrPrintf("-priv should be used for domain register. Correct usage is '-priv +begin <RegSpace> -reg ... +end'\n");
            reg.PrintRegInfo();
            MASSERT(0);
        }

        if (std::regex_search(regName, std::regex(MDiagUtils::s_LWPgraphGpcPrefix + "[0-9]+")))
        {
            if (!GetRegister(regName.c_str()))
            {
                ErrPrintf("%s does not exist in the manuals, please use -priv with regSpace as LW_PGRAPH_PRI_GPC[%u] and regName as 0th GPC index\n", regName.c_str());
                ErrPrintf("Format: '-priv +begin <RegSpace> -reg .... +end'\n");
                ErrPrintf("More details are here: https://confluence.lwpu.com/display/ArchMods/LW_PGRAPH_PRI_GPC+RegOps\n");
                reg.PrintRegInfo();
                MASSERT(0);
            }
        }
    }

    // process the list of lw40 -priv options, as there may be more than one
    if (OK != ParseRegBlock(params,
            "Correct usage is '-priv +begin gpu -reg ... +end'\n"
            "Or, just '-reg ...'\n", "-priv", &m_privArgMap, ParamList))
    {
        MASSERT(!"Error in parsing -priv cmdline arg");
    }

    for (auto arg : m_privArgMap)
    {
        ArgDatabase *argDB;
        ArgReader *privParams;

        string regSpace = arg.first;
        tie(argDB, privParams) = arg.second;

        vector<CRegisterControl> regCtrls;

        src = ProcessRegBlock(privParams, "-reg", regCtrls);
        for (auto& reg : regCtrls)
        {
            string regName;
            
            if (reg.m_regPtr)
            {
                regName = reg.m_regPtr->GetName();    
            }
            // Handle domain register which is zero based register address and LW_PGRAPH_PRI_GPC[%u]/_GPC[%u]_TPC[%u] registers
            if ((reg.m_domain != RegHalDomain::RAW) || (std::regex_search(regName, std::regex(MDiagUtils::s_LWPgraphGpcPrefix + "[0-9]+"))))
            {
                // need to adjust the domain for multicast lwlink
                reg.m_domain = MDiagUtils::AdjustDomain(reg.m_domain, regSpace);
                reg.m_regSpace = regSpace;
            }

            m_privRegControls.push_back(reg);
        }
    }

    // process the list of -conf options, as there may be more than one
    if (OK != ParseRegBlock(params,
            "Correct usage is '-conf +begin gpu -reg ... +end'\n"
            "Or, just '-reg ...'\n", "-conf", &m_confArgDB, &m_confParams, ParamList))
    {
        MASSERT(!"Error in parsing -conf cmdline arg");
    }

    src = ProcessRegBlock(m_confParams.get(), "-reg", m_confRegControls);

    if (!UsePcieConfigCycles())
    {
        for (auto& reg : m_confRegControls)
        {
            reg.m_address += DEVICE_BASE(LW_PCFG);
        }
    }
    
    else
    {
        for (auto& reg : m_confRegControls)
        {
            reg.m_isConfigSpace = true;
        }
    }

    for (auto& reg : m_privRegControls)
    {
        if (IsSMCEnabled() && IsCtxReg(reg.m_address)) //Ctx regsiter should be handled in Trace3d
        {
            DebugPrintf("In SMC mode, Defer to handle in trace3d if the PGRAPH/RUNLIST/CHRAM register is from mdiag:%d", reg.m_address);
            continue;
        }

        m_mdiagPrivRegs.push_back(reg);
    }

    src = HandleOptionalRegisters(&m_mdiagPrivRegs);
    src = HandleOptionalRegisters(&m_confRegControls);

    // EDIT Dec 2015, now "-reg_eos" and "-reg" have the same assertion scheme.
    CheckRegCtrlResult(src, "-reg");
}

RC LWGpuResource::ProcessOptionalRegisters(vector<CRegisterControl>* regvec, const char *regs)
{
    StickyRC rc;
    vector<string> reg_list;

    vector<char> regs_copy(strlen(regs)+1);
    memcpy(&regs_copy[0], regs, regs_copy.size());

    char* token = strtok(&regs_copy[0], ";");
    while (token)
    {
        DebugPrintf("New register control: %s\n", token);
        reg_list.push_back(token);
        token = strtok(NULL, ";");
    }

    CRegCtrlFactory factory(this);
    for (UINT32 i = 0; i < reg_list.size(); i++)
    {
        if (factory.ParseLine(*regvec, reg_list[i].c_str()) == false)
        {
            ErrPrintf("Failed to parse register and action: %s.\n", reg_list[i].c_str());
            rc = RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    return rc;
}

UINT32 LWGpuResource::GetMemSpaceCtxDma(_DMA_TARGET  Target,
                                        LwRm* pLwRm,
                                        GpuDevice   *pGpuDevice)
{
    MASSERT(pGpuDevice);

    switch (Target)
    {
        case _DMA_TARGET_VIDEO:
            return pGpuDevice->GetMemSpaceCtxDma(Memory::Fb, false, pLwRm);
        case _DMA_TARGET_COHERENT:
            return pGpuDevice->GetMemSpaceCtxDma(Memory::Coherent, false, pLwRm);
        case _DMA_TARGET_NONCOHERENT:
            return pGpuDevice->GetMemSpaceCtxDma(Memory::NonCoherent, false, pLwRm);
        default:
            MASSERT(!"Invalid Target");
            return 0;
    }
}

// Return register number, return 1 if register not found
int LWGpuResource::GetRegNum
(
    const char *regName,
    UINT32 *Value,
    const char *regSpace,
    UINT32 subdev, /* = Gpu::UNSPECIFIED_SUBDEV */
    LwRm *pLwRm, /* = nullptr */
    bool ignoreDomainBase /* = false */
)
{
    DebugPrintf("Get register address: Reg %s, ignoreDomainBase %s, RegSpace %s\n", 
        regName, ignoreDomainBase ? "true": "false", regSpace);

    unique_ptr<IRegisterClass> ireg;
    UINT32 idxI, idxJ;
    ireg = GetRegister(regName, subdev, &idxI, &idxJ);
    
    if (!ireg)
    {
        DebugPrintf("Get register address: Could not find register %s\n", regName);
        return 1;
    }
    UINT32 regAddr = ireg->GetAddress(idxI, idxJ);

    if (!ignoreDomainBase)
    {
        GpuSubdevice *pSubDev = GetGpuSubdevice(subdev);
        regAddr += MDiagUtils::GetDomainBase(regName, regSpace, pSubDev, pLwRm);
    }

    DebugPrintf("Register address returned: 0x%x\n", regAddr);

    *Value = regAddr;
    return 0;
}

// set Register field by name, ignore if field == 0
int LWGpuResource::SetRegFldDef
(
    const char *regName,
    const char *regFieldName,
    const char *regFieldDef,
    UINT32 *myvalue,
    const char *regSpace,
    UINT32 subdev, /* = Gpu::UNSPECIFIED_SUBDEV */
    SmcEngine *pSmcEngine, /* = nullptr */
    bool overrideCtx /* = false */
)
{
    DebugPrintf("Set Register Field Def: Reg %s, Field %s, Def %s, RegSpace %s\n", 
        regName, regFieldName, regFieldDef, regSpace);

    unique_ptr<IRegisterClass> ireg;
    UINT32 idxI, idxJ;
    // look up our register, return if register not found
    ireg = GetRegister(regName, subdev, &idxI, &idxJ);
    if (!ireg)
    {
        DebugPrintf("Set Register Field Def: Could not find register %s\n", regName);
        return 1;
    }

    LwRm *pLwRm = pSmcEngine ? GetLwRmPtr(pSmcEngine) : LwRmPtr().Get();
    string smcEngineName(pSmcEngine ? pSmcEngine->GetName() : "n/a");

    const string absRegFieldName = string(ireg->GetName()) + regFieldName;
    const string absRegFieldDef = absRegFieldName + regFieldDef;

    unique_ptr<IRegisterField> ifield;
    ifield = ireg->FindField(absRegFieldName.c_str());
    if (!ifield) 
    {
        DebugPrintf("Set Register Field Def: Could not find register field %s\n", 
            absRegFieldName.c_str());
        return 1;
    }

    unique_ptr<IRegisterValue> ivalue;
    ivalue = ifield->GetValueHead();

    while (ivalue)
    {
        if (ivalue->GetName() == absRegFieldDef) break;
        ivalue = ifield->GetValueNext();
    }

    if (ivalue) 
    {
        // if found the match definition, now we can set the value
        UINT32 regAddr = ireg->GetAddress(idxI, idxJ);

        GpuSubdevice *pSubDev = GetGpuSubdevice(subdev);
        regAddr += MDiagUtils::GetDomainBase(regName, regSpace, pSubDev, pLwRm);

        UINT32 setValue = ivalue->GetValue();
        UINT32 mask = ifield->GetWriteMask();

        UINT32 value = myvalue ? (*myvalue) : (RegRd32(regAddr, subdev, pSmcEngine));
        
        // set the field
        value =
            (value & (~mask) ) |
            ((setValue<< ifield->GetStartBit()) & mask);

        DebugPrintf("Set Register Field Def: RegAddr 0x%x, WriteMask 0x%08x, Data 0x%x\n", 
            regAddr, mask, value);

        if ( myvalue != 0 ) 
        {
            *myvalue = value;
        } 
        else 
        {
            if (overrideCtx)
            {
                RC rc = SetContextOverride(regAddr, 0xffffffff, value, pSmcEngine);
                if (OK != rc)
                {
                    ErrPrintf("SetContextOverride failed. %s: regAddr=0x%08x, andMask=0xffffffff, orMask=0x%08x, SmcEngine=%s",
                        rc.Message(), regAddr, value, smcEngineName.c_str());
                    return 1;
                }
            }
            RegWr32(regAddr, value, subdev, pSmcEngine);
        }
        return 0;
    } 
    else 
    {
        DebugPrintf("Set Register Field Def: Could not find register field def %s\n", 
            absRegFieldDef.c_str());
        return 1;
    }
}

// set Register field by name, ignore if field == 0
int LWGpuResource::SetRegFldNum
(
    const char *regName,
    const char *regFieldName,
    UINT32 setValue,
    UINT32 *myvalue,
    const char *regSpace,
    UINT32 subdev,
    SmcEngine *pSmcEngine,
    bool overrideCtx 
)
{
    DebugPrintf("Set Register Field Num: Reg %s, Field %s, Num 0x%x, RegSpace %s\n", 
        regName, regFieldName, setValue, regSpace);

    // look up our register, return if register not found
    unique_ptr<IRegisterClass> ireg;
    UINT32 idxI, idxJ;
    ireg = GetRegister(regName, subdev, &idxI, &idxJ);
    if (!ireg)
    {
        DebugPrintf("Set Register Field Num: Could not find register %s\n", regName);
        return 1;
    }

    GpuSubdevice *pSubDev = GetGpuSubdevice(subdev);
    LwRm *pLwRm = pSmcEngine ? GetLwRmPtr(pSmcEngine) : LwRmPtr().Get();
    string smcEngineName(pSmcEngine ? pSmcEngine->GetName() : "n/a");
    UINT32 regBase = MDiagUtils::GetDomainBase(regName, regSpace, pSubDev, pLwRm);

    if (!regFieldName || strlen(regFieldName) == 0) 
    {
        // if pass 0 or "" for field , then write the whole register
        UINT32 regAddr = ireg->GetAddress(idxI, idxJ);

        DebugPrintf("Set Register Field Num: Field name missing, write Data 0x%x to RegAddr 0x%x\n", 
            setValue, regAddr+regBase);
        RegWr32(regAddr+regBase, setValue, subdev, pSmcEngine);

        return 0;
    }

    const string absRegFieldName = string(ireg->GetName()) + regFieldName;

    unique_ptr<IRegisterField> ifield;
    ifield = ireg->FindField(absRegFieldName.c_str());
    if (!ifield) 
    {
        DebugPrintf("Set Register Field Num: Could not find register field %s\n", 
            absRegFieldName.c_str());
        return 1;
    }

    // if found the match definition, now we can set the value
    UINT32 regAddr = ireg->GetAddress(idxI, idxJ);
    regAddr += regBase;
    UINT32 mask = ifield->GetWriteMask();

    UINT32 value = myvalue ? (*myvalue) : (RegRd32(regAddr, subdev, pSmcEngine));

    // set the field
    value = ((value & (~mask) ) |
             ((setValue<< ifield->GetStartBit()) & mask));

    DebugPrintf("Set Register Field Num: RegAddr 0x%x, WriteMask 0x%08x, Data 0x%x\n", 
        regAddr, mask, value);

    if (myvalue) 
    {
        *myvalue = value;
    } 
    else 
    {
        if (overrideCtx)
        {
            RC rc = SetContextOverride(regAddr, 0xffffffff, value, pSmcEngine);
            if (OK != rc)
            {
                ErrPrintf("SetContextOverride failed. %s: regAddr=0x%08x, andMask=0xffffffff, orMask=0x%08x, SmcEngine=%s",
                    rc.Message(), regAddr, value, smcEngineName.c_str());
                return 1;
            }
        }
        RegWr32(regAddr, value, subdev, pSmcEngine);
    }

    return 0;
}

// Get Register field definition value by name, return 0 if success
int LWGpuResource::GetRegFldDef
(
    const char *regName,
    const char *regFieldName,
    const char *regFieldDef,
    UINT32 * myvalue,
    UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */
)
{
    DebugPrintf("Get Register Field Def: Reg %s, Field %s, Def %s\n", 
        regName, regFieldName, regFieldDef);

    unique_ptr<IRegisterClass> ireg;
    // look up our register, return if register not found
    ireg = GetRegister(regName, subdev);
    if (!ireg)
    {
        DebugPrintf("Get Register Field Def: Could not find register %s\n", regName);
        return 1;
    }

    const string absRegFieldName = string(ireg->GetName()) + regFieldName;

    const string absRegFieldDef = absRegFieldName + regFieldDef;

    unique_ptr<IRegisterField> ifield;
    ifield = ireg->FindField(absRegFieldName.c_str());
    if (!ifield) 
    {
        DebugPrintf("Get Register Field Def: Could not find register field %s\n", 
            absRegFieldName.c_str());
        return 1;
    }

    unique_ptr<IRegisterValue> ivalue;

    ivalue = ifield->FindValue(absRegFieldDef.c_str());
    if (ivalue) 
    {
        // if found the match definition, now we can set the value
        *myvalue = ivalue->GetValue();
        DebugPrintf("Get Register Field Def: Returned Value 0x%x\n", *myvalue);
        return 0;
    }
    
    DebugPrintf("Get Register Field Def: Could not find register field def %s\n", 
            absRegFieldDef.c_str());
    return 1;
}

// get Register field by name, ignore if field == 0
// If myvar != 0, get field from variable *myvar, instead of the register
int LWGpuResource::GetRegFldNum
(
    const char *regName,
    const char *regFieldName,
    UINT32 *theValue,
    UINT32 *myvar,
    const char *regSpace,
    UINT32 subdev, /* = Gpu::UNSPECIFIED_SUBDEV */
    SmcEngine *pSmcEngine /* = nullptr */
)
{
    DebugPrintf("Get Register Field Num: Reg %s, Field %s, RegSpace %s\n", 
        regName, regFieldName, regSpace);

    IRegisterMap* regMap = GetRegisterMap(subdev);

    unique_ptr<IRegisterClass> ireg;
    UINT32 idxI, idxJ;
    // look up our register, return if register not found
    ireg = regMap->FindRegister(regName, &idxI, &idxJ);
    if (!ireg)
    {
        DebugPrintf("Get Register Field Num: Could not find register %s\n", regName);
        return 1;
    }

    const string absRegFieldName = string(ireg->GetName()) + regFieldName;
    unique_ptr<IRegisterField> ifield;
    ifield = ireg->FindField(absRegFieldName.c_str());

    // if found the match definition, now we can set the value
    UINT32 regAddr = ireg->GetAddress(idxI, idxJ);

    GpuSubdevice *pSubDev = GetGpuSubdevice(subdev);
    LwRm *pLwRm = pSmcEngine ? GetLwRmPtr(pSmcEngine) : LwRmPtr().Get();
    regAddr += MDiagUtils::GetDomainBase(regName, regSpace, pSubDev, pLwRm);

    UINT32 value = (myvar != 0) ? *myvar : RegRd32(regAddr, subdev, pSmcEngine);

    // if field null or invalid, return the whole register
    // but also return 1, allowing to be used as a way to test valid fields
    if ((!ifield) || (strlen(regFieldName) == 0)) 
    {
        *theValue = value;
        DebugPrintf("Get Register Field Num: Field name missing, return Data 0x%x for RegAddr 0x%x\n", 
            value, regAddr);
        if (strlen(regFieldName) == 0) 
        { 
            return 0;
        } 
        else 
        {
            return 1;
        }
    } 
    else 
    {
        UINT32 mask = ifield->GetReadMask();
        // set the field
        *theValue = (value & mask) >> ifield->GetStartBit();
        DebugPrintf("Get Register Field Num: RegAddr 0x%x, ReadMask 0x%08x, Data 0x%x\n", 
            regAddr, mask, *theValue);
    }
    return 0;
}

// Test Register field by name
bool LWGpuResource::TestRegFldDef
(
    const char *regName,
    const char *regFieldName,
    const char *regFieldDef,
    UINT32 *myValue,
    const char *regSpace,
    UINT32 subdev, /* = Gpu::UNSPECIFIED_SUBDEV */
    SmcEngine *pSmcEngine /* = nullptr */
)
{
    DebugPrintf("Test Register Field Def: Reg %s, Field %s, Def %s, RegSpace %s\n", 
        regName, regFieldName, regFieldDef, regSpace);

    unique_ptr<IRegisterClass> ireg;
    UINT32 idxI, idxJ;

    // look up our register, return if register not found
    ireg = GetRegister(regName, subdev, &idxI, &idxJ);
    if (!ireg)
    {
        ErrPrintf("Test Register Field Def: Could not find register %s\n", regName);
        MASSERT(0);
        return false;
    }

    LwRm *pLwRm = pSmcEngine ? GetLwRmPtr(pSmcEngine) : LwRmPtr().Get();

    const string absRegFieldName = string(ireg->GetName()) + regFieldName;

    const string absRegFieldDef = absRegFieldName + regFieldDef;

    unique_ptr<IRegisterField> ifield;
    ifield = ireg->FindField(absRegFieldName.c_str());
    if (!ifield) 
    {
        ErrPrintf("Test Register Field Def: Could not find register field %s\n", 
            absRegFieldName.c_str());
        MASSERT(0);
        return false;
    }

    unique_ptr<IRegisterValue> ivalue;
    ivalue = ifield->GetValueHead();

    while (ivalue)
    {
        if (ivalue->GetName() == absRegFieldDef) break;
        ivalue = ifield->GetValueNext();
    }

    if (ivalue) 
    {
        // if found the match definition, now we can set the value
        UINT32 regAddr = ireg->GetAddress(idxI, idxJ);

        GpuSubdevice *pSubDev = GetGpuSubdevice(subdev);
        regAddr += MDiagUtils::GetDomainBase(regName, regSpace, pSubDev, pLwRm);

        UINT32 fieldValue = ivalue->GetValue();
        UINT32 mask = ifield->GetReadMask();
        UINT32 value;

        // if pass a non-zero point, that means I want to simulate
        // so don't write out the register
        if (myValue) 
        {
            value = *myValue;
        } 
        else 
        {
            value = RegRd32(regAddr, subdev, pSmcEngine);
        }

        DebugPrintf(
            "Test Register Field Def: RegAddr 0x%x, ReadMask 0x%x, ReadData 0x%x FieldValue 0x%x\n", 
            regAddr, mask, value, fieldValue);

        return ((value & mask) ==
                ((fieldValue << ifield->GetStartBit()) & mask));
    } 
    else 
    {
        ErrPrintf("Test Register Field Def: Could not find register field def %s\n", 
            absRegFieldDef.c_str());
        MASSERT(0);
        return false;
    }
}

// Test Register field by value 
bool LWGpuResource::TestRegFldNum
(
    const char *regName,
    const char *regFieldName,
    UINT32 regFieldValue,
    UINT32 *myValue,
    const char *regSpace,
    UINT32 subdev,
    SmcEngine *pSmcEngine
)
{
    DebugPrintf("Test Register Field Num: Reg %s, Field %s, Num 0x%x, RegSpace %s\n", 
        regName, regFieldName, regFieldValue, regSpace);

    unique_ptr<IRegisterClass> ireg;
    UINT32 idxI, idxJ;

    // look up our register, return if register not found
    ireg = GetRegister(regName, subdev, &idxI, &idxJ);
    if (!ireg)
    {
        ErrPrintf("Test Register Field Num: Could not find register %s\n", regName);
        MASSERT(0);
        return false;
    }

    GpuSubdevice *pSubDev = GetGpuSubdevice(subdev);
    LwRm *pLwRm = pSmcEngine ? GetLwRmPtr(pSmcEngine) : LwRmPtr().Get();
    UINT32 regBase = MDiagUtils::GetDomainBase(regName, regSpace, pSubDev, pLwRm);

    const string absRegFieldName = string(ireg->GetName()) + regFieldName;

    unique_ptr<IRegisterField> ifield;
    ifield = ireg->FindField(absRegFieldName.c_str());
    if (!ifield) 
    {
        ErrPrintf("Test Register Field Num: Could not find register field %s\n", 
            absRegFieldName.c_str());
        MASSERT(0);
        return false;
    }

    // if found the match definition, now we can set the value
    UINT32 regAddr = ireg->GetAddress(idxI, idxJ);
    regAddr += regBase;
    UINT32 mask = ifield->GetReadMask();

    UINT32 value;

    if (myValue) 
    {
        value = *myValue;
    } 
    else 
    {
        value = RegRd32(regAddr, subdev, pSmcEngine);
    }

    DebugPrintf(
        "Test Register Field Num: RegAddr 0x%x, ReadMask 0x%x, ReadData 0x%x FieldValue 0x%x\n", 
        regAddr, mask, value, regFieldValue);

    return ((value & mask) ==
                ((regFieldValue << ifield->GetStartBit()) & mask));
}

RC LWGpuResource::GetRegFieldBitPositions
(
    string regName,
    string regFieldName,
    UINT32* startBit,
    UINT32* endBit,
    UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */
)
{
    RC rc;
    unique_ptr<IRegisterClass> iReg;
    iReg = GetRegister(regName.c_str(), subdev);
    if (!iReg) 
    {
        ErrPrintf("Could not find register %s\n", regName.c_str());
        return RC::SOFTWARE_ERROR;
    }

    const string absRegFieldName = string(iReg->GetName()) + regFieldName;
    unique_ptr<IRegisterField> iField;
    iField = iReg->FindField(absRegFieldName.c_str());
    if (!iField) 
    {
        ErrPrintf("Could not find register field %s\n", absRegFieldName.c_str());
        return RC::SOFTWARE_ERROR;
    }

    if (startBit)
    {
        *startBit = iField->GetStartBit();
    }
    if (endBit)
    {
        *endBit = iField->GetEndBit();
    }

    return rc;
}

void LWGpuResource::GetBuffInfo(BuffInfo* inf)
{
    Buffers::const_iterator end = m_Buffers.end();
    for (Buffers::const_iterator iter = m_Buffers.begin(); iter != end; ++iter)
    {
        inf->SetDmaBuff("Buffer", *iter);
    }
}

struct PollRegArgs
{
    const LWGpuResource* pGpuRes;
    UINT32               address;
    UINT32               value;
    UINT32               mask;
    UINT32               subdev;
    SmcEngine*           pSmcEngine;
};

bool LWGpuResource::PollRegValueFunc(void *pArgs)
{
    PollRegArgs *pollArgs = (PollRegArgs*)(pArgs);
    UINT32 address = pollArgs->address;
    UINT32 value   = pollArgs->value;
    UINT32 mask    = pollArgs->mask;
    UINT32 subdev  = pollArgs->subdev;
    SmcEngine *pSmcEngine  = pollArgs->pSmcEngine;

    UINT32 readValue = pollArgs->pGpuRes->RegRd32(address, subdev, pSmcEngine);

    return ((value & mask) == (readValue & mask));
}

RC LWGpuResource::PollRegValue
(
    UINT32 address,
    UINT32 value,
    UINT32 mask,
    FLOAT64 timeoutMs,
    UINT32 subdev,
    SmcEngine *pSmcEngine
)
const
{
    PollRegArgs pollArgs = {this, address, value, mask, subdev, pSmcEngine};
    return POLLWRAP_HW(LWGpuResource::PollRegValueFunc, &pollArgs, timeoutMs);
}

UINT32 LWGpuResource::PgraphRegRd(UINT32 offset, UINT32 subdev, SmcEngine *pSmcEngine) const
{
    MASSERT(pSmcEngine);
    string smcEngineName("n/a");
    UINT32 swizzId = ~0U;
    LwRm *pLwRm = LwRmPtr().Get();
    bool isLegacy = (pSmcEngine == SmcEngine::GetFakeSmcEngine());

    GpuSubdevice * pGpusubdevice = GetGpuSubdevice(subdev);
    RC rc;
    UINT32 regVal = ~0U;

    // If use legacy PGPRAH space, SMC info is not needed.
    if (!isLegacy)
    {
        smcEngineName = pSmcEngine->GetName();
        swizzId = pSmcEngine->GetGpuPartition()->GetSwizzId();
        pLwRm = GetLwRmPtr(pSmcEngine);
        MASSERT(pLwRm);
        
        LW2080_CTRL_GPU_REG_OP regReadOp = { 0 };
        LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS params;

        memset(&params, 0, sizeof(LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS));
        regReadOp.regOp = LW2080_CTRL_GPU_REG_OP_READ_32;
        regReadOp.regType = LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL;
        regReadOp.regOffset = offset;
        regReadOp.regValueHi = 0;
        regReadOp.regValueLo = 0;
        regReadOp.regAndNMaskHi = 0;
        regReadOp.regAndNMaskLo = ~0;

        params.regOpCount = 1;
        params.regOps = LW_PTR_TO_LwP64(&regReadOp);
        rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpusubdevice),
                LW2080_CTRL_CMD_GPU_EXEC_REG_OPS,
                &params,
                sizeof(params));
        regVal = regReadOp.regValueLo;
    }
    else
    {
        LW2080_CTRL_GR_REG_ACCESS_PARAMS params = { };
        params.regOffset = offset;
        params.regVal = 0;
        params.accessFlag = LW2080_CTRL_GR_REG_ACCESS_FLAG_READ | LW2080_CTRL_GR_REG_ACCESS_FLAG_LEGACY;
        // Use LW2080_CTRL_CMD_GR_REG_ACCESS only to access legacy space 
        // in SMC mode
        rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpusubdevice),
                LW2080_CTRL_CMD_GR_REG_ACCESS,
                &params,
                sizeof(params));
        regVal = params.regVal;
    }

    if (rc != OK)
    {
        ErrPrintf("Could not access register: %s. offset=%d, smcEngineName=%s, swizzId=%d, isLegacy =%s",
                  rc.Message(), offset, smcEngineName.c_str(), swizzId, isLegacy ? "true" : "false");
        MASSERT(0);
    }

    return regVal;
}

void LWGpuResource::PgraphRegWr(UINT32 offset, UINT32 value, UINT32 subdev, SmcEngine *pSmcEngine)
{
    MASSERT(pSmcEngine);
    string smcEngineName("n/a");
    UINT32 swizzId = ~0U;
    LwRm *pLwRm = LwRmPtr().Get();
    bool isLegacy = (pSmcEngine == SmcEngine::GetFakeSmcEngine());

    GpuSubdevice * pGpusubdevice = GetGpuSubdevice(subdev);
    RC rc;

    // If use legacy PGPRAH space, SMC info is not needed.
    if (!isLegacy)
    {
        smcEngineName = pSmcEngine->GetName();
        swizzId = pSmcEngine->GetGpuPartition()->GetSwizzId();
        pLwRm = GetLwRmPtr(pSmcEngine);
        MASSERT(pLwRm);

        LW2080_CTRL_GPU_REG_OP regWriteOp = { 0 };
        LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS params;

        memset(&params, 0, sizeof(LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS));
        regWriteOp.regOp = LW2080_CTRL_GPU_REG_OP_WRITE_32;
        regWriteOp.regType = LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL;
        regWriteOp.regOffset = offset;
        regWriteOp.regValueHi = 0;
        regWriteOp.regValueLo = value;
        regWriteOp.regAndNMaskHi = 0;
        regWriteOp.regAndNMaskLo = ~0;

        params.regOpCount = 1;
        params.regOps = LW_PTR_TO_LwP64(&regWriteOp);
        rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpusubdevice),
                LW2080_CTRL_CMD_GPU_EXEC_REG_OPS,
                &params,
                sizeof(params));
    }
    else
    {
        LW2080_CTRL_GR_REG_ACCESS_PARAMS params = { };
        params.regOffset = offset;
        params.regVal = value;
        params.accessFlag = LW2080_CTRL_GR_REG_ACCESS_FLAG_WRITE | LW2080_CTRL_GR_REG_ACCESS_FLAG_LEGACY;
        // Use LW2080_CTRL_CMD_GR_REG_ACCESS only to access legacy space 
        // in SMC mode
        rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpusubdevice),
                LW2080_CTRL_CMD_GR_REG_ACCESS,
                &params,
                sizeof(params));
    }

    if (rc != OK)
    {
        ErrPrintf("Could not access register: %s. offset=%d, smcEngineName=%s, swizzId=%d, isLegacy =%s",
                  rc.Message(), offset, smcEngineName.c_str(), swizzId, isLegacy ? "true" : "false");
        MASSERT(0);
    }
}

bool LWGpuResource::IsSMCEnabled() const
{
    return GetGpuSubdevice()->IsSMCEnabled();
}

bool LWGpuResource::IsPgraphReg(UINT32 offset) const
{
    return GetGpuSubdevice()->IsPgraphRegister(offset);
}

bool LWGpuResource::IsPerRunlistReg(UINT32 offset) const
{
    return GetGpuSubdevice()->IsRunlistRegister(offset);
}

bool LWGpuResource::IsCtxReg(UINT32 offset) const
{
    // In SMC mode, CtxReg should be handled in Trace3d with SMC info.
    GpuSubdevice * pSubDev = GetGpuSubdevice();
    return pSubDev->IsRunlistRegister(offset) || pSubDev->IsPgraphRegister(offset);
}

UINT08 LWGpuResource::RegRd08(UINT32 offset, UINT32 subdev, SmcEngine* pSmcEngine) const
{
    if (IsSMCEnabled() && IsPgraphReg(offset))
    {
        return (UINT08) LWGpuResource::PgraphRegRd(offset, subdev, pSmcEngine);
    }

    return GetGpuSubdevice(subdev)->RegRd08(offset);
}

UINT16 LWGpuResource::RegRd16(UINT32 offset, UINT32 subdev, SmcEngine* pSmcEngine) const
{
    if (IsSMCEnabled() && IsPgraphReg(offset))
    {
        return (UINT16) LWGpuResource::PgraphRegRd(offset, subdev, pSmcEngine);
    }

    return GetGpuSubdevice(subdev)->RegRd16(offset);
}

UINT32 LWGpuResource::RegRd32(UINT32 offset, UINT32 subdev, SmcEngine* pSmcEngine) const
{
    if (IsSMCEnabled() && IsPgraphReg(offset))
    {
        return LWGpuResource::PgraphRegRd(offset, subdev, pSmcEngine);
    }

    if (IsSMCEnabled() && pSmcEngine && Tasker::CanThreadYield())
    {
        LwRm *pLwRm = GetLwRmPtr(pSmcEngine);
        return GetGpuSubdevice(subdev)->RegOpRd32(offset, pLwRm);
    }

    return GetGpuSubdevice(subdev)->RegRd32(offset);
}

void LWGpuResource::RegWr08(UINT32 offset, UINT08 data, UINT32 subdev, SmcEngine* pSmcEngine)
{
    if (IsSMCEnabled() && IsPgraphReg(offset))
    {
        return PgraphRegWr(offset, data, subdev, pSmcEngine);
    }

    return GetGpuSubdevice(subdev)->RegWr08(offset, data);
}

void LWGpuResource::RegWr16(UINT32 offset, UINT16 data, UINT32 subdev, SmcEngine* pSmcEngine)
{
    if (IsSMCEnabled() && IsPgraphReg(offset))
    {
        return PgraphRegWr(offset, data, subdev, pSmcEngine);
    }

    return GetGpuSubdevice(subdev)->RegWr16(offset, data);
}

void LWGpuResource::RegWr32(UINT32 offset, UINT32 data, UINT32 subdev, SmcEngine* pSmcEngine)
{
    if (IsSMCEnabled() && IsPgraphReg(offset))
    {
        return PgraphRegWr(offset, data, subdev, pSmcEngine);
    }
    
    if (IsSMCEnabled() && pSmcEngine && Tasker::CanThreadYield())
    {
        LwRm *pLwRm = GetLwRmPtr(pSmcEngine);
        return GetGpuSubdevice(subdev)->RegOpWr32(offset, data, pLwRm);
    }

    return GetGpuSubdevice(subdev)->RegWr32(offset, data);
}

void LWGpuResource::Write32(ModsGpuRegValue value, UINT32 index, UINT32 subdev, SmcEngine* pSmcEngine)
{
    GpuSubdevice *pSubdev = GetGpuSubdevice(subdev);
    const RegHal& regHal = pSubdev->Regs();

    ModsGpuRegField field = regHal.ColwertToField(value);
    UINT32 regAddr = regHal.LookupAddress(regHal.ColwertToAddress(field), RegHal::ArrayIndexes_t{index});
    UINT32 regValue = RegRd32(regAddr, subdev, pSmcEngine);
    regHal.SetField(&regValue, field, regHal.LookupValue(value));
    RegWr32(regAddr, regValue, subdev, pSmcEngine);
}

PHYSADDR LWGpuResource::PhysFbBase() const
{
    return GetGpuSubdevice()->GetPhysFbBase();
}

//! Get the GpuDevice for channel/object/FB alloc or Ctrl calls.
GpuDevice * LWGpuResource::GetGpuDevice() const
{
    MASSERT(m_pGpuDevice);
    return m_pGpuDevice;
}

//! Get the only GpuSubdevice for register/clock/FB stuff.
//! For use by SLI-unaware code, asserts if NumSubdevices > 1.
GpuSubdevice* LWGpuResource::GetGpuSubdevice() const
{
    GpuSubdevice* pSubdev = GetGpuDevice()->GetSubdevice(0);
    MASSERT(pSubdev);
    return pSubdev;
}

//! Get one of the GpuSubdevices for register/clock/FB stuff.
//! Asserts if subInst >= NumSubdevices.
GpuSubdevice* LWGpuResource::GetGpuSubdevice(UINT32 subInst) const
{
    if (subInst == Gpu::UNSPECIFIED_SUBDEV)
        return GetGpuSubdevice();

    GpuDevice *pGpuDev = GetGpuDevice();

    MASSERT(subInst < pGpuDev->GetNumSubdevices());

    return pGpuDev->GetSubdevice(subInst);
}

bool LWGpuResource::MatchRange(const RegRanges& ranges, UINT32 offset) const
{
    for (RegRanges::const_iterator iter = ranges.begin(); iter != ranges.end(); ++iter)
    {
        if ((offset >= iter->RegAddrRange.first) &&
            (offset <= iter->RegAddrRange.second))
        {
            return true;
        }
    }

    return false;
}

RC LWGpuResource::CheckDeviceRequirements()
{
    RC rc;

    GpuSubdevice *gpuSubDev = GetGpuSubdevice();

    if (params->ParamPresent("-require_primary"))
    {
        if (!gpuSubDev->IsPrimary())
        {
            ErrPrintf("-require_primary specified but GPU is not primary.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    if (params->ParamPresent("-require_not_primary"))
    {
        if (gpuSubDev->IsPrimary())
        {
            ErrPrintf("-require_not_primary specified but GPU is primary.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    if (params->ParamPresent("-require_quadro"))
    {
        if (!gpuSubDev->IsQuadroEnabled())
        {
            ErrPrintf("-require_quadro specified but GPU is not in lwdqro mode.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    if (params->ParamPresent("-require_msdec"))
    {
        if (!gpuSubDev->IsMsdecEnabled())
        {
            ErrPrintf("-require_msdec specified but MSDEC is disabled via fuse.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    if (params->ParamPresent("-require_ecc_fuse"))
    {
        if (!gpuSubDev->IsEccFuseEnabled())
        {
            ErrPrintf("-require_ecc_fuse specified but the ECC fuse is disabled.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    if (params->ParamPresent("-require_no_ecc_fuse"))
    {
        if (gpuSubDev->IsEccFuseEnabled())
        {
            ErrPrintf("-require_no_ecc_fuse specified but the ECC fuse is enabled.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    bool checkFbEcc = params->ParamPresent("-require_fb_ecc") > 0;
    bool checkL2Ecc = params->ParamPresent("-require_l2_ecc") > 0;
    bool checkL1Ecc = params->ParamPresent("-require_l1_ecc") > 0;
    bool checkSmEcc = params->ParamPresent("-require_sm_ecc") > 0;
    bool checkNoFbEcc = params->ParamPresent("-require_no_fb_ecc") > 0;
    bool checkNoL2Ecc = params->ParamPresent("-require_no_l2_ecc") > 0;
    bool checkNoL1Ecc = params->ParamPresent("-require_no_l1_ecc") > 0;
    bool checkNoSmEcc = params->ParamPresent("-require_no_sm_ecc") > 0;

    if (checkFbEcc || checkL2Ecc || checkL1Ecc || checkSmEcc ||
        checkNoFbEcc || checkNoL2Ecc || checkNoL1Ecc || checkNoSmEcc)
    {
        bool fbEccEnabled = false;
        bool l2EccEnabled = false;
        bool l1EccEnabled = false;
        bool smEccEnabled = false;
        bool supported;
        UINT32 mask;

        CHECK_RC(gpuSubDev->GetEccEnabled(&supported, &mask));

        if (supported)
        {
            fbEccEnabled = Ecc::IsUnitEnabled(Ecc::Unit::DRAM, mask);
            l2EccEnabled = Ecc::IsUnitEnabled(Ecc::Unit::L2, mask);
            l1EccEnabled = Ecc::IsUnitEnabled(Ecc::Unit::L1, mask);
            smEccEnabled = Ecc::IsUnitEnabled(Ecc::Unit::LRF, mask);
        }

        if (checkFbEcc && !fbEccEnabled)
        {
            ErrPrintf("-require_fb_ecc specified but FB ECC is not enabled.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if (checkL2Ecc && !l2EccEnabled)
        {
            ErrPrintf("-require_l2_ecc specified but L2 ECC is not enabled.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if (checkL1Ecc && !l1EccEnabled)
        {
            ErrPrintf("-require_l1_ecc specified but L1 ECC is not enabled.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if (checkSmEcc && !smEccEnabled)
        {
            ErrPrintf("-require_sm_ecc specified but SM ECC is not enabled.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        else if (checkNoFbEcc && fbEccEnabled)
        {
            ErrPrintf("-require_no_fb_ecc specified but FB ECC is enabled.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if (checkNoL2Ecc && l2EccEnabled)
        {
            ErrPrintf("-require_no_l2_ecc specified but L2 ECC is enabled.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if (checkNoL1Ecc && l1EccEnabled)
        {
            ErrPrintf("-require_no_l1_ecc specified but L1 ECC is enabled.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if (checkNoSmEcc && smEccEnabled)
        {
            ErrPrintf("-require_no_sm_ecc specified but SM ECC is enabled.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    UINT32 argCount = params->ParamPresent("-valid_chipset");

    if (argCount > 0)
    {
        LwRmPtr pLwRm;
        LW0000_CTRL_SYSTEM_GET_CHIPSET_INFO_PARAMS Params = {0};
        CHECK_RC(pLwRm->Control(
                        pLwRm->GetClientHandle(),
                        LW0000_CTRL_CMD_SYSTEM_GET_CHIPSET_INFO,
                        &Params,
                        sizeof(Params)));
        UINT32 i;
        bool found = false;

        for (i = 0; i < argCount; ++i)
        {
            if (strcmp((const char*)Params.chipsetNameString,
                params->ParamNStr("-valid_chipset", i, 0)) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            ErrPrintf("Chipset %s does not match any -valid_chipset argument.\n", Params.chipsetNameString);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    if (params->ParamPresent("-require_revision"))
    {
        UINT32 requiredRevision = params->ParamUnsigned("-require_revision");
        UINT32 regValue = RegRd32(LW_PMC_BOOT_0);
        UINT32 major = DRF_VAL(_PMC, _BOOT_0, _MAJOR_REVISION, regValue);
        UINT32 minor = DRF_VAL(_PMC, _BOOT_0, _MINOR_REVISION, regValue);
        UINT32 revision = (major << 4) | minor;

        if (revision != requiredRevision)
        {
            ErrPrintf("Required revision %X doesn't match silicon revision %x.\n", requiredRevision, revision);

            return RC::BAD_COMMAND_LINE_ARGUMENT;

        }
    }

    argCount = params->ParamPresent("-require_chip");

    if (argCount > 0)
    {
        string deviceName = Gpu::DeviceIdToInternalString(gpuSubDev->DeviceId());

        UINT32 i;
        bool found = false;

        for (i = 0; i < argCount; ++i)
        {
            if (stricmp(deviceName.c_str(),
                params->ParamNStr("-require_chip", i, 0)) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            ErrPrintf("A -require_chip argument has been specified but does not match actual chip %s.\n", deviceName.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    if (params->ParamPresent("-require_ecid") > 0)
    {
        vector<UINT32> ecidVec;

        CHECK_RC(gpuSubDev->ChipId(&ecidVec));

        string ecidStr = "0x";

        for (UINT32 i = 0; i < ecidVec.size(); ++i)
        {
            char ecidWord[10];
            sprintf(ecidWord, "%08x", ecidVec[i]);
            ecidStr += ecidWord;
        }

        if (strcmp(ecidStr.c_str(), params->ParamStr("-require_ecid")) != 0)
        {
            ErrPrintf("A -require_ecid argument has been specified but does not match actual ECID %s.\n", ecidStr.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    if (params->ParamPresent("-require_ram_type") > 0)
    {
        string protocolString = gpuSubDev->GetFB()->GetRamProtocolString();
        bool match = false;

        if (strcmp(protocolString.c_str(), params->ParamStr("-require_ram_type")) == 0)
        {
            match = true;
        }

        // Allow SDDR to be an alias of DDR.
        if (strncmp(protocolString.c_str(), "DDR", 3) == 0)
        {
            protocolString.insert(0, "S");

            if (strcmp(protocolString.c_str(),
                    params->ParamStr("-require_ram_type")) == 0)
            {
                match = true;
            }
        }

        if (!match)
        {
            ErrPrintf("A -require_ram_type argument has been specified but does not match the actual Ram type of %s.\n", protocolString.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    bool requireTexParity =
        (params->ParamPresent("-require_tex_parity_check") > 0);
    bool requireNoTexParity =
        (params->ParamPresent("-require_no_tex_parity_check") > 0);

    if (requireTexParity && requireNoTexParity)
    {
        ErrPrintf("both -require_tex_parity_check and -require_no_tex_parity_check are specified.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    else if (requireTexParity || requireNoTexParity)
    {
        if (gpuSubDev->Regs().IsSupported(MODS_PGRAPH_PRI_GPCS_TPCS_TEX_M_PARITY))
        {
            if (gpuSubDev->Regs().Test32(MODS_PGRAPH_PRI_GPCS_TPCS_TEX_M_PARITY_CHECK_ENABLE))
            {
                if (requireNoTexParity)
                {
                    ErrPrintf("-require_no_tex_parity_check specified but TEX parity check is enabled.\n");
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
                }
            }
            else
            {
                if (requireTexParity)
                {
                    ErrPrintf("-require_tex_parity_check specified but TEX parity check is disabled.\n");
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
                }
            }
        }
        else
        {
            WarnPrintf("-require_tex_parity_check/-require_no_tex_parity_check not supported from > Kepler\n");
        }
    }

    bool requireL2Parity = (params->ParamPresent("-require_l2_parity") > 0);
    bool requireNoL2Parity =
        (params->ParamPresent("-require_no_l2_parity") > 0);

    if (requireL2Parity && requireNoL2Parity)
    {
        ErrPrintf("Both -require_l2_parity and -require_no_l2_parity are specified.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    else if (requireL2Parity || requireNoL2Parity)
    {
        UINT32 regValue = RegRd32(LW_PLTCG_LTCS_LTSS_DSTG_CFG1);

        if (FLD_TEST_DRF(_PLTCG, _LTCS_LTSS_DSTG_CFG1, _PARITY_GEN, _ENABLED,
            regValue))
        {
            if (requireNoL2Parity)
            {
                ErrPrintf("-require_no_l2_parity specified but L2 parity is enabled.\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
        else
        {
            if (requireL2Parity)
            {
                ErrPrintf("-require_l2_parity specified but L2 parity is disabled.\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }

    if (params->ParamPresent("-require_2xtex_fuse"))
    {
        if (!gpuSubDev->Is2XTexFuseEnabled())
        {
            ErrPrintf("-require_2xtex_fuse specified but the OPT_PGOB fuse is disabled.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    return rc;
}

LWGpuTsg* LWGpuResource::GetSharedGpuTsg(string tsgname, SmcEngine* pSmcEngine)
{
    if (m_SharedLWGpuTsgMap.find(pSmcEngine) != m_SharedLWGpuTsgMap.end())
    {
        SharedLWGpuTsgList sharedLWGpuTsgList = *(m_SharedLWGpuTsgMap[pSmcEngine]);
        for (auto& pSharedLwGpuTsg : sharedLWGpuTsgList)
        {
            if (pSharedLwGpuTsg->GetName() == tsgname)
                return pSharedLwGpuTsg;
        }
    }

    return nullptr;
}

void LWGpuResource::AddSharedGpuTsg(LWGpuTsg* pGpuTsg, SmcEngine *pSmcEngine)
{
    if (m_SharedLWGpuTsgMap.find(pSmcEngine) == m_SharedLWGpuTsgMap.end())
    {
        m_SharedLWGpuTsgMap[pSmcEngine] = make_unique<SharedLWGpuTsgList>();
    }
    SharedLWGpuTsgList sharedLWGpuTsgList = *(m_SharedLWGpuTsgMap[pSmcEngine]);
    for (auto& pSharedLwGpuTsg : sharedLWGpuTsgList)
    {
        if (pSharedLwGpuTsg->GetName() == pGpuTsg->GetName())
        {
            // tsg name is unique in shared tsglist
            MASSERT(pSharedLwGpuTsg == pGpuTsg);
            return;
        }
    }

    m_SharedLWGpuTsgMap[pSmcEngine]->push_back(pGpuTsg);
    return;
}

void LWGpuResource::RemoveSharedGpuTsg(LWGpuTsg* pGpuTsg, SmcEngine *pSmcEngine)
{
    if (m_SharedLWGpuTsgMap.find(pSmcEngine) == m_SharedLWGpuTsgMap.end())
    {
        MASSERT(!"RemoveSharedGpuTsg: no entry for the SmcEngine\n");
        return;
    }

    SharedLWGpuTsgList* sharedLWGpuTsgList = m_SharedLWGpuTsgMap[pSmcEngine].get();

    list<LWGpuTsg*>::iterator it = sharedLWGpuTsgList->begin();
    for (; it != sharedLWGpuTsgList->end(); it++)
    {
        if (*it == pGpuTsg)
        {
            sharedLWGpuTsgList->erase(it);
            break;
        }
    }

    if (sharedLWGpuTsgList->size() == 0)
    {
        m_SharedLWGpuTsgMap.erase(pSmcEngine);
    }

    return;
}

UINT32 LWGpuResource::GetRandChIdFromPool(pair<LwRm*, UINT32> lwRmEngineIdPair)
{
    if (m_AvailableChIdPoolPerEngine.end() == m_AvailableChIdPoolPerEngine.find(lwRmEngineIdPair))
    {
        m_AvailableChIdPoolPerEngine.insert(make_pair(lwRmEngineIdPair, vector<UINT08>(m_MaxNumChId, (UINT08)0)));

        if (GetGpuDevice()->GetFamily() >= GpuDevice::Ampere)
        {
            // From Ampere onwards, RM uses ChID0 for scrubber
            // This is a WAR until RM adds a ctrl call to query the scrubber status
            // TODO Bug: 2723744
            m_AvailableChIdPoolPerEngine[lwRmEngineIdPair][0] = 1;
        }
    }

    UINT32 randIdx = m_ChIdRandomStream->RandomUnsigned(0, m_MaxNumChId - 1);
    UINT32 emptyIdx = randIdx;

    while (m_AvailableChIdPoolPerEngine[lwRmEngineIdPair][emptyIdx] != 0)
    {
        ++ emptyIdx;
        emptyIdx %= m_MaxNumChId;

        if (emptyIdx == randIdx)
        {
            break;
        }
    }

    if (m_AvailableChIdPoolPerEngine[lwRmEngineIdPair][emptyIdx] != 0)
    {
        MASSERT(!"No available channel ID\n");
    }

    // Starting from Ampere, channel will bind with engine.
    if (m_pGpuDevice->GetSubdevice(0)->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        DebugPrintf("Get random channel ID %d for engine ID %d from pool.\n", emptyIdx, lwRmEngineIdPair.second);
    }
    else
    {
        DebugPrintf("Get random channel ID %d from pool.\n", emptyIdx);
    }

    return emptyIdx;
}

void LWGpuResource::RemoveChIdFromPool(pair<LwRm*, UINT32> lwRmEngineIdPair, UINT32 chId)
{
    if (m_AvailableChIdPoolPerEngine.end() != m_AvailableChIdPoolPerEngine.find(lwRmEngineIdPair))
    {
        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            chId = chId & 0xFFFF;
        }
        MASSERT(chId < m_MaxNumChId);
        m_AvailableChIdPoolPerEngine[lwRmEngineIdPair][chId] = 1; // mark as allocated id
    }
}

void LWGpuResource::AddChIdToPool(pair<LwRm*, UINT32> lwRmEngineIdPair, UINT32 chId)
{
    if (m_AvailableChIdPoolPerEngine.end() != m_AvailableChIdPoolPerEngine.find(lwRmEngineIdPair))
    {
        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            chId = chId & 0xFFFF;
        }
        MASSERT(chId < m_MaxNumChId);
        m_AvailableChIdPoolPerEngine[lwRmEngineIdPair][chId] = 0; // mark as freed id
    }
}

RandomStream* LWGpuResource::GetLocationRandomizer()
{
    return m_LocationRandomizer.get();
}

RC LWGpuResource::AddSharedSurface(const char* name, MdiagSurf* surf)
{
    if (name == NULL)
    {
        ErrPrintf("The shared surface name is NULL.\n");
        return RC::BAD_PARAMETER;
    }

    if (surf == NULL)
    {
        ErrPrintf("The shared surface '%s' is setting to NULL.\n", name);
        return RC::BAD_PARAMETER;
    }

    if (m_SharedSurfaces.find(name) != m_SharedSurfaces.end())
    {
        ErrPrintf("The shared surface '%s' has been created.\n", name);
        return RC::SOFTWARE_ERROR;
    }

    m_SharedSurfaces[name] = surf;
    return OK;
}

RC LWGpuResource::RemoveSharedSurface(const char* name)
{
    RC rc;
    if (name == NULL)
    {
        ErrPrintf("%s: Ilwalide parameter - name is NULL.\n", __FUNCTION__);
        return RC::BAD_PARAMETER;
    }

    map<string, MdiagSurf*>::iterator it =  m_SharedSurfaces.find(name);
    if (it != m_SharedSurfaces.end())
    {
        m_SharedSurfaces.erase(it);
    }

    return rc;
}

MdiagSurf* LWGpuResource::GetSharedSurface(const char* name)
{
    if (name == NULL)
    {
        ErrPrintf("The shared surface name is NULL.\n");
        return NULL;
    }

    if (m_SharedSurfaces.find(name) == m_SharedSurfaces.end())
    {
        ErrPrintf("The shared surface '%s' hasn't been created.\n", name);
        return NULL;
    } else {
        return m_SharedSurfaces[name];
    }
}

RC LWGpuResource::AddActiveT3DTest(Trace3DTest *pTest)
{
    // Nothing to do if the test has been added
    //
    if (m_ActiveT3DTests.count(pTest) > 0)
    {
        return OK;
    }

    m_ActiveT3DTests.insert(pTest);
    return OK;
}

RC LWGpuResource::RemoveActiveT3DTest(Trace3DTest *pTest)
{
    // Nothing to do if the test has been removed
    //
    if (m_ActiveT3DTests.count(pTest) == 0)
    {
        return OK;
    }

    m_ActiveT3DTests.erase(pTest);
    return OK;
}

//
// Return the copy engine which is partner of graphic engine
//
RC LWGpuResource::GetGrCopyEngineType(vector<UINT32>& engineType, LwRm* pLwRm)
{
    RC rc;
    UINT32 runqueueCount = GetGpuSubdevice()->GetNumGraphicsRunqueues();
    UINT32 grCopyType = LW2080_ENGINE_TYPE_NULL;
    for (UINT32 runqueue = 0; runqueue < runqueueCount; ++runqueue)
    {
        CHECK_RC(GetGrCopyEngineType(&grCopyType, runqueue, pLwRm));
        if (grCopyType != LW2080_ENGINE_TYPE_NULL)
            engineType.push_back(grCopyType);
    }

    return rc;
}

RC LWGpuResource::GetGrCopyEngineType(UINT32 * pRtnEngineType, UINT32 runqueue, LwRm* pLwRm)
{
    // RM does not support LW2080_CTRL_CMD_GPU_GET_ENGINE_PARTNERLIST
    // call for amodel. Hard coded LW2080_ENGINE_TYPE_COPY2 for it
    //
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        if (EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_A))
        {
            m_grCopyEngineType = LW2080_ENGINE_TYPE_COPY0;
            //TODO: Need to distinguish GRCOPY1 from GRCOPY0
        }
        else
        {
            m_grCopyEngineType = LW2080_ENGINE_TYPE_COPY2;
        }
        *pRtnEngineType = m_grCopyEngineType;
        return OK;
    }

    RC rc;
    LW2080_CTRL_GPU_GET_ENGINE_PARTNERLIST_PARAMS params;
    UINT32 engineType = LW2080_ENGINE_TYPE_NULL;

    // Get channel class
    //
    UINT32 ChClass = 0;
    static const UINT32 GpFifoClasses[] =
    {
        HOPPER_CHANNEL_GPFIFO_A,
        AMPERE_CHANNEL_GPFIFO_A,        
        TURING_CHANNEL_GPFIFO_A,
        VOLTA_CHANNEL_GPFIFO_A,
        PASCAL_CHANNEL_GPFIFO_A,
        MAXWELL_CHANNEL_GPFIFO_A,
        KEPLER_CHANNEL_GPFIFO_C,
        KEPLER_CHANNEL_GPFIFO_B,
        KEPLER_CHANNEL_GPFIFO_A,
        GF100_CHANNEL_GPFIFO,
    };
    CHECK_RC(pLwRm->GetFirstSupportedClass(static_cast<UINT32>(NUMELEMS(GpFifoClasses)),
                                            GpFifoClasses, &ChClass, m_pGpuDevice));

    // Get engine partnerlist
    //
    memset(&params, 0, sizeof(params));
    params.engineType = LW2080_ENGINE_TYPE_GRAPHICS;
    params.partnershipClassId = ChClass;
    params.runqueue = runqueue;
    CHECK_RC(pLwRm->ControlBySubdevice(GetGpuSubdevice(0),
        LW2080_CTRL_CMD_GPU_GET_ENGINE_PARTNERLIST,
        &params,
        sizeof(params)));

    // Loop over find the partner ce of graphic engine
    //
    UINT32 partnerIdx, i = 0;
    for (partnerIdx = 0; partnerIdx < params.numPartners; partnerIdx++)
    {
        for (i = 0; i < LW2080_ENGINE_TYPE_COPY_SIZE; i++)
        {
            if (params.partnerList[partnerIdx] == LW2080_ENGINE_TYPE_COPY(i))
            {
                engineType = params.partnerList[partnerIdx];
                break;
            }
        }
    }

    m_grCopyEngineType = engineType;
    *pRtnEngineType = engineType;
    return OK;
}

LWGpuResource::VaSpaceManager * LWGpuResource::GetVaSpaceManager
(
    LwRm* pLwRm
)
{
    if (m_VaSpaceManagers.find(pLwRm) == m_VaSpaceManagers.end())
    {
        // register the new VaSpaceManager
        VaSpaceManager * pVaSpacemn = new VaSpaceManager(this, pLwRm);
        m_VaSpaceManagers.insert(make_pair(pLwRm, pVaSpacemn));
    }

    return m_VaSpaceManagers[pLwRm];
}

LWGpuResource::SubCtxManager * LWGpuResource::GetSubCtxManager
(
    LwRm* pLwRm
)
{
    if (m_SubCtxManagers.find(pLwRm) == m_SubCtxManagers.end())
    {
        // register the new SubCtxManager
        SubCtxManager * pSubCtxmn = new SubCtxManager(this, pLwRm);
        m_SubCtxManagers.insert(make_pair(pLwRm, pSubCtxmn));
    }

    return m_SubCtxManagers[pLwRm];
}

string LWGpuResource::GetAddressSpaceName(LwRm::Handle vaSpaceHandle, LwRm* pLwRm)
{
    VaSpaceManager *vaSpaceManager = GetVaSpaceManager(pLwRm);
    VaSpaceManager::iterator iter;

    for (iter = vaSpaceManager->begin(); iter != vaSpaceManager->end(); ++iter)
    {
        const shared_ptr<VaSpace> &vaSpace = iter->second;

        if (vaSpace->GetHandle() == vaSpaceHandle)
        {
            return vaSpace->GetName();
        }
    }

    return "";
}

UINT32 LWGpuResource::GetAddressSpacePasid(LwRm::Handle vaSpaceHandle, LwRm* pLwRm)
{
    VaSpaceManager *vaSpaceManager = GetVaSpaceManager(pLwRm);
    VaSpaceManager::iterator iter;

    for (iter = vaSpaceManager->begin(); iter != vaSpaceManager->end(); ++iter)
    {
        const shared_ptr<VaSpace> &vaSpace = iter->second;

        if (vaSpace->GetHandle() == vaSpaceHandle)
        {
            return vaSpace->GetPasid();
        }
    }

    return 0;
}

ZbcTable *LWGpuResource::GetZbcTable(LwRm* pLwRm)
{
    if (m_ZbcTable.get() == 0)
    {
        m_ZbcTable.reset(new ZbcTable(this, pLwRm));
        RC rc = m_ZbcTable->AddArgumentEntries(GetLwgpuArgs(), pLwRm);
        if (OK != rc)
        {
            ErrPrintf("Fail to add argument entries to zbc table: %s\n", rc.Message());
            MASSERT(0);
        }
    }

    return m_ZbcTable.get();
};

void LWGpuResource::ClearZbcTables(LwRm* pLwRm)
{
    if (m_ZbcTable.get() != 0)
    {
        GpuDevice *gpuDevice = GetGpuDevice();
        for (UINT32 i = 0; i < gpuDevice->GetNumSubdevices(); ++i)
        {
            m_ZbcTable->ClearTables(gpuDevice->GetSubdevice(i), pLwRm);
        }
        m_ZbcTable.reset(0);
    }
}

bool LWGpuResource::HasGlobalZbcArguments()
{
    if ((params->ParamPresent("-zbc_color_table_entry") > 0) ||
        (params->ParamPresent("-zbc_depth_table_entry") > 0) ||
        (params->ParamPresent("-zbc_stencil_table_entry") > 0))
    {
        return true;
    }
    return false;
}

void LWGpuResource::InitSmOorAddrCheckMode()
{
    GpuDevice *pGpuDev = GetGpuDevice();

    if (Platform::IsVirtFunMode())
        return;

    if (pGpuDev != nullptr)
    {
        for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); ++subdev)
        {
            GpuSubdevice* pGpuSubdev = pGpuDev->GetSubdevice(subdev);
            if (pGpuSubdev->Regs().Test32(
                MODS_PGRAPH_PRI_GPCS_TPCS_SM_TEXIO_CONTROL_OOR_ADDR_CHECK_MODE_POWER_63_49_ZERO))
            {
                pGpuSubdev->Regs().Write32(
                    MODS_PGRAPH_PRI_GPCS_TPCS_SM_TEXIO_CONTROL_OOR_ADDR_CHECK_MODE_POWER_63_49_ZERO);
            }
        }
    }
}

LWGpuContexScheduler::LWGpuContexScheduler():
    m_GlobalActiveContext (nullptr)
{
}

RC LWGpuContexScheduler::RegisterTestToContext(const string& name, const Test *test)
{
    RC rc;

    // let testId same as trace3d test id
    UINT32 testId = static_cast<UINT32>(test->GetSeqId() - 1);

    if (IsTestInSharedContext(name))
    {
        // Share with other threads - multiple threadIds in the context
        for (size_t i = 0; i < m_TraceContexts.size(); ++i)
        {
            if (m_TraceContexts[i].m_ContextName != name)
                continue; // skip contexts with mismatched name

            vector<UINT32>* pTestIds = &m_TraceContexts[i].m_TestIds;
            if (std::find(pTestIds->begin(), pTestIds->end(), testId) == pTestIds->end())
            {
                DebugPrintf(MSGID(), "%s: Add TestId(%d) to the context(%s)\n",
                            __FUNCTION__, testId, name.c_str());

                pTestIds->push_back(testId);
                m_TestStates[testId].m_TestState = Registered;
                m_TestStates[testId].m_ThreadId = Tasker::LwrrentThread();
            }

            return rc;
        }
    }
    else
    {
        // Not share with another thread - each testId is in a seperate context
        for (size_t i = 0; i < m_TraceContexts.size(); ++i)
        {
            if (!m_TraceContexts[i].m_ContextName.empty())
                continue; // skip named context

            vector<UINT32>* pTestIds = &m_TraceContexts[i].m_TestIds;
            if (std::find(pTestIds->begin(), pTestIds->end(), testId) != pTestIds->end())
            {
                return rc; // has been registered
            }
        }
    }

    // not found, create a new context
    DebugPrintf(MSGID(), "%s: Created context (%s) which has test(testId=%d)\n",
                __FUNCTION__, name.empty()? "Unnamed" : name.c_str(), testId);
    m_TraceContexts.push_back(TraceContext(name, testId));
    m_TestStates[testId].m_TestState = Registered;
    m_TestStates[testId].m_ThreadId = Tasker::LwrrentThread();

    return rc;
}

RC LWGpuContexScheduler::UnRegisterTestFromContext()
{
    RC rc;

    // Remove test id in the context
    UINT32 id = GetLwrrentTestId();
    if (!IsRegisteredTest(id))
    {
        DebugPrintf(MSGID(), "%s: Test(id=%d) is not a registered test. Skip the operation\n",
                    __FUNCTION__, id);

        return rc; // Not a registered test
    }

    CHECK_RC(WaitWakenUp()); // Only waking thread can Unregister

    size_t ctxIndex = 0;
    CHECK_RC(GetLwrrentCtx(&ctxIndex));
    TraceContext* pTraceCtx = &m_TraceContexts[ctxIndex];

    DebugPrintf(MSGID(), "%s: Unregister test(id=%d) from context(name = %s)\n",
                __FUNCTION__, id, pTraceCtx->m_ContextName.empty()? "unnamed" : pTraceCtx->m_ContextName.c_str());

    pTraceCtx->m_TestIds.erase(
        std::remove(pTraceCtx->m_TestIds.begin(), pTraceCtx->m_TestIds.end(), id),
        pTraceCtx->m_TestIds.end());

    if (m_TestsReachedSyncPoint.count(id))
    {
        // non-trace3d tests like v2d may skip SyncAllRegisteredCtx() call
        // before reaching UnRegisterTestFromContext()
        m_TestsReachedSyncPoint.erase(id);
    }

    m_TestStates.erase(id);

    // Destroy the context if it's empty
    if (pTraceCtx->m_TestIds.size() == 0)
    {
        // remove the context
        m_TraceContexts.erase(m_TraceContexts.begin() + ctxIndex);

        DebugPrintf(MSGID(), "%s: Context(name = %s) is empty. Remove it from scheduler\n",
                    __FUNCTION__, pTraceCtx->m_ContextName.empty()? "unnamed" : pTraceCtx->m_ContextName.c_str());
    }
    else
    {
        // Set the active id to the 1st test.
        pTraceCtx->m_CtxActiveTestId = pTraceCtx->m_TestIds[0];
    }

    m_GlobalActiveContext = nullptr; // release
    return rc;
}

RC LWGpuContexScheduler::BindThreadToTest(const Test *test)
{
    RC rc;

    // ThreadId got while creating context maybe main thread
    // Here is the real thread created to execute each trace3d test
    UINT32 testId = static_cast<UINT32>(test->GetSeqId() - 1);
    if (!IsRegisteredTest(testId))
    {
        DebugPrintf(MSGID(), "%s: Test(id=%d) is not a registered test. Skip the operation\n",
                    __FUNCTION__, testId);

        return rc; // Not a registered test
    }

    m_TestStates[testId].m_ThreadId = Tasker::LwrrentThread();

    DebugPrintf(MSGID(), "%s: Test(id = %d) is exelwting in thread(id = %d)\n",
                __FUNCTION__, testId, m_TestStates[testId].m_ThreadId);

    return rc;
}

RC LWGpuContexScheduler::AcquireExelwtion()
{
    RC rc;

    UINT32 id = GetLwrrentTestId();
    if (!IsRegisteredTest(id))
    {
        DebugPrintf(MSGID(), "%s: Test(id=%d) is not a registered test. Skip the operation\n",
                    __FUNCTION__, id);

        return rc; // Not a registered test
    }

    DebugPrintf(MSGID(), "%s: Test(id = %d) is acquiring the exelwtion\n",
                __FUNCTION__, id);

    CHECK_RC(WaitWakenUp()); // Only waking thread can yield to next

    size_t ctxIndex = 0;
    CHECK_RC(GetLwrrentCtx(&ctxIndex));
    m_GlobalActiveContext = &m_TraceContexts[ctxIndex];

    DebugPrintf(MSGID(), "%s: Test(id = %d) got the exelwtion\n", __FUNCTION__, id);

    return rc;
}

RC LWGpuContexScheduler::ReleaseExelwtion()
{
    RC rc;
    UINT32 id = GetLwrrentTestId();
    if (!IsRegisteredTest(id))
    {
        DebugPrintf(MSGID(), "%s: Test(id=%d) is not a registered test. Skip the operation\n",
                    __FUNCTION__, id);

        return rc; // Not a registered test
    }

    CHECK_RC(WaitWakenUp()); // Only waking thread can yield to next

    // comment out below debug messages because it will print tons of message
    // in a slow simulator.
    // Keep it here if someone wants to enable it and debug something

    m_GlobalActiveContext = nullptr; // give up ownership

    m_TestStates[id].m_TestState = Registered; // change state

    return rc;
}

RC LWGpuContexScheduler::YieldToNextCtxThread()
{
    RC rc;

    if (m_TraceContexts.size() < 2)
    {
        return rc; // last contexts, nothing to do
    }

    UINT32 id = GetLwrrentTestId();
    if (!IsRegisteredTest(id))
    {
        DebugPrintf(MSGID(), "%s: Test(id=%d) is not a registered test. Skip the operation\n",
                    __FUNCTION__, id);

        return rc; // Not a registered test
    }

    CHECK_RC(WaitWakenUp()); // Only waking thread can yield to next

    size_t ctxIndex = 0;
    CHECK_RC(GetLwrrentCtx(&ctxIndex));
    ctxIndex = (ctxIndex + 1) % m_TraceContexts.size(); // next one

    // wake up the active test in next context
    m_GlobalActiveContext = &m_TraceContexts[ctxIndex];

    DebugPrintf(MSGID(), "%s: thread %d swithing to test %d\n",
                __FUNCTION__, id, m_GlobalActiveContext->m_CtxActiveTestId);

    CHECK_RC(WaitWakenUp()); // Waiting wakenup by other thread

    DebugPrintf(MSGID(), "%s: test %d is waking to thread exelwtion\n", __FUNCTION__, id);

    return rc;
}

RC LWGpuContexScheduler::YieldToNextSubchThread()
{
    RC rc;

    UINT32 id = GetLwrrentTestId();
    if (!IsRegisteredTest(id))
    {
        DebugPrintf(MSGID(), "%s: Test(id=%d) is not a registered test. Skip the operation\n",
                    __FUNCTION__, id);

        return rc; // Not a registered test
    }

    CHECK_RC(WaitWakenUp()); // Only waking thread can yield to next

    size_t ctxIndex = 0;
    CHECK_RC(GetLwrrentCtx(&ctxIndex));
    TraceContext* pTraceCtx = &m_TraceContexts[ctxIndex];

    if (pTraceCtx->m_TestIds.size() < 2)
    {
        return rc; // last test in the context, nothing to do
    }

    size_t index = 0;
    for (; index < pTraceCtx->m_TestIds.size(); ++ index)
    {
        if (pTraceCtx->m_TestIds[index] == id)
        {
            break;
        }
    }

    // wake up the next thread in same context
    index = (index + 1) % pTraceCtx->m_TestIds.size();
    pTraceCtx->m_CtxActiveTestId = pTraceCtx->m_TestIds[index];
    m_GlobalActiveContext = pTraceCtx;

    DebugPrintf(MSGID(), "%s: test %d swithing to test %d\n",
                __FUNCTION__, id, m_GlobalActiveContext->m_CtxActiveTestId);

    CHECK_RC(WaitWakenUp()); // Waiting wakenup by other threads

    DebugPrintf(MSGID(), "%s: test %d is waking to thread exelwtion\n", __FUNCTION__, id);

    return rc;
}

RC LWGpuContexScheduler::SyncAllRegisteredCtx(const LWGpuContexScheduler::SyncPoint& syncPoint)
{
    RC rc;

    if (m_TestStates.size() < 2)
    {
        return rc; // less than 2 tests, nothing to do
    }

    UINT32 id = GetLwrrentTestId();
    if (!IsRegisteredTest(id))
    {
        DebugPrintf(MSGID(), "%s: Test(id=%d) is not a registered test. Skip the operation\n",
                    __FUNCTION__, id);

        return rc; // Not a registered test
    }

    DebugPrintf(MSGID(), "%s: Test(id=%d) is entering the sync-block point (%s)\n",
                __FUNCTION__, id, syncPoint.ToString().c_str());

    //
    // The first test hit here is responsible for checking
    // to make sure all registered tests hit here;
    // Other tests just wait the first test sending the sync
    // done event (m_TestsReachedSyncPoint.empty() == true)
    if (m_TestsReachedSyncPoint.empty())
    {
        m_SyncPoint = syncPoint;
        m_TestsReachedSyncPoint.insert(id); // not empty any more

        // nobody owner the exelwtion
        bool bNoExplicitOwner = m_GlobalActiveContext == nullptr;

        while (!IsAllRegisteredTestsSynced())
        {
            // Not care who is the next
            CHECK_RC(ReleaseExelwtion()); // give up the ownership
            Tasker::Yield();              // let others to go
        }

        DebugPrintf(MSGID(), "%s: All tests reached sync point (%s)\n",
                    __FUNCTION__, syncPoint.ToString().c_str());

        // All registered tests hit the sync point;
        // Take the exelwtion ownership before notifying others.
        // After notification, other tests will be blocked by WaitWakenUp()
        // at the end of the function and only the 1st entering test can exit
        CHECK_RC(AcquireExelwtion());

        // Notify others to stop looping
        m_TestsReachedSyncPoint.clear();
        Tasker::Yield();

        if (bNoExplicitOwner)
        {
            // Give up the ownership got by AcquireExelwtion above
            // and then other threads/test will not be blocked on Yield()
            CHECK_RC(ReleaseExelwtion());
        }
    }
    else
    {
        if (m_SyncPoint != syncPoint)
        {
            ErrPrintf("Can't sync on different sync points (%s) VS. (%s)!\n",
                      m_SyncPoint.ToString().c_str(), syncPoint.ToString().c_str());
            MASSERT("Exit abnormally!");
        }
        m_TestsReachedSyncPoint.insert(id);

        // Not care who is the next
        // Yield until tests to be synced is empty
        while (!m_TestsReachedSyncPoint.empty())
        {
            CHECK_RC(ReleaseExelwtion()); // give up the ownership
            Tasker::Yield();     // let others to go
        }

        DebugPrintf(MSGID(), "%s: Test(id=%d) detected the sync done @(%s)\n",
                    __FUNCTION__, id, syncPoint.ToString().c_str());
    }

    CHECK_RC(WaitWakenUp());

    DebugPrintf(MSGID(), "%s: Test(id=%d) is exiting the sync point @(%s)\n",
                __FUNCTION__, id, syncPoint.ToString().c_str());

    return rc;
}

RC LWGpuContexScheduler::GetLwrrentCtx(size_t* pIndex) const
{
    RC rc;
    if (pIndex == nullptr)
        return RC::SOFTWARE_ERROR;

    UINT32 id = GetLwrrentTestId();

    for (size_t i = 0; i <  m_TraceContexts.size(); ++i)
    {
        const vector<UINT32>* pTestIds = &m_TraceContexts[i].m_TestIds;
        if (std::find(pTestIds->begin(), pTestIds->end(), id) != pTestIds->end())
        {
            *pIndex = i;
            return rc;
        }
    }

    return RC::SOFTWARE_ERROR;
}

UINT32 LWGpuContexScheduler::GetLwrrentTestId() const
{
    // ThreadId => TestId
    //
    Tasker::ThreadID threadId = Tasker::LwrrentThread();

    map<UINT32, TestRunningInfo>::const_iterator cit;
    for (cit = m_TestStates.begin(); cit != m_TestStates.end(); ++cit)
    {
        if (cit->second.m_ThreadId == threadId)
        {
            return cit->first;
        }
    }

    return ~0;
}

RC LWGpuContexScheduler::WaitWakenUp()
{
    RC rc;

    UINT32 id = GetLwrrentTestId();
    m_TestStates[id].m_TestState = Waiting;

    // Wait to be waken up by other threads
    while ((m_GlobalActiveContext != nullptr) &&
           (m_GlobalActiveContext->m_CtxActiveTestId != id))
    {
        Tasker::Yield();
    }

    m_TestStates[id].m_TestState = Running;

    return rc;
}

bool LWGpuContexScheduler::IsAllRegisteredTestsSynced() const
{
    for (size_t ctxIndex = 0; ctxIndex < m_TraceContexts.size(); ++ ctxIndex)
    {
        const TraceContext* pCtx = &m_TraceContexts[ctxIndex];
        for (size_t testIndex = 0; testIndex < pCtx->m_TestIds.size(); ++ testIndex)
        {
            if (m_TestsReachedSyncPoint.count(pCtx->m_TestIds[testIndex]) == 0)
            {
                return false;
            }
        }
    }

    return true;
}

bool LWGpuContexScheduler::IsRegisteredTest(UINT32 testId) const
{
    return m_TestStates.count(testId) > 0;
}

LWGpuContexScheduler::TraceContext::TraceContext
(
    const string& name,
    UINT32 testId
):
    m_ContextName(name),
    m_CtxActiveTestId(testId)
{
    m_TestIds.push_back(testId);
}

LWGpuContexScheduler::SyncPoint::SyncPoint()
{}

LWGpuContexScheduler::SyncPoint::SyncPoint(const string& point)
: m_PointName(point)
{}

bool LWGpuContexScheduler::SyncPoint::operator!=
(
    const LWGpuContexScheduler::SyncPoint& otherPoint
) const
{
    return m_PointName != otherPoint.m_PointName;
}

string LWGpuContexScheduler::SyncPoint::ToString() const
{
    return m_PointName;
}

// Get the SmcEngine* for the smc_engine_label
SmcEngine* LWGpuResource::GetSmcEngine(const string& smcEngineName)
{
    SmcEngine* pSmcEngine = nullptr;

    // smc_partitioning is enabled but smc_engine_label is not specified
    if (m_pSmcResourceController && (smcEngineName == UNSPECIFIED_SMC_ENGINE))
        MASSERT(!"SMC is enabled but smc_engine_label/-smc_eng_name is not specified for test");

    // smc_partitioning is not enabled but smc_engine_label is specified
    if (!m_pSmcResourceController && (smcEngineName != UNSPECIFIED_SMC_ENGINE))
        MASSERT(!"SMC is not enabled but smc_engine_label/-smc_eng_name is specified for test");

    if (m_pSmcResourceController)
    {
        pSmcEngine = m_pSmcResourceController->GetSmcEngine(smcEngineName);
        MASSERT(pSmcEngine);
    }

    return pSmcEngine;
}

void LWGpuResource::SmcSanityCheck() const
{
    MASSERT(m_pSmcResourceController);
    const vector<GpuPartition*>& gpuPartitions = m_pSmcResourceController->GetActiveGpuPartition();
    for (const auto& gpuPartition : gpuPartitions)
    {
        LwRm* pLwRm = GetLwRmPtr(gpuPartition);
        MASSERT(pLwRm);
        gpuPartition->SanityCheck(pLwRm, GetGpuSubdevice(0));
    }
}

RC LWGpuResource::AllocRmClients()
{
    RC rc;
    MASSERT(m_pSmcResourceController);
    const vector<GpuPartition*>& gpuPartitions = m_pSmcResourceController->GetActiveGpuPartition();
    for (const auto& gpuPartition : gpuPartitions)
    {
        CHECK_RC(AllocRmClient(gpuPartition));
    }

    return rc;
}

RC LWGpuResource::AllocRmClient(GpuPartition* pGpuPartition)
{
    RC rc;
    MASSERT(pGpuPartition);
    MASSERT(pGpuPartition->IsValid());
    MASSERT(GetLwRmPtr(pGpuPartition) == nullptr);

    LwRm* pLwRm = LwRmPtr::GetFreeClient();
    MASSERT(pLwRm);

    CHECK_RC(GetGpuDevice()->Alloc(pLwRm));

    LwRm::Handle Handle;
    LWC637_ALLOCATION_PARAMETERS params = { };
    params.swizzId = pGpuPartition->GetSwizzId();

    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetGpuSubdevice(0)), 
        &Handle, AMPERE_SMC_PARTITION_REF, &params));
 
    CHECK_RC(GetGpuDevice()->AllocSingleClientStuff(pLwRm, GetGpuSubdevice(0)));

    m_MapPartitionLwRmPtr[pGpuPartition] = pLwRm;
    m_MapPartitionResourceHandle[pGpuPartition] = Handle;
   
    return rc;
}

RC LWGpuResource::AllocRmClient(SmcEngine* pSmcEngine)
{
    RC rc;
    MASSERT(pSmcEngine);
    MASSERT(pSmcEngine->IsAlloc());
    MASSERT(GetLwRmPtr(pSmcEngine) == nullptr);

    LwRm* pLwRm = LwRmPtr::GetFreeClient();
    MASSERT(pLwRm);

    CHECK_RC(GetGpuDevice()->Alloc(pLwRm));

    LwRm::Handle memoryPartitionHandle;
    LWC637_ALLOCATION_PARAMETERS memoryPartitionParams = { };
    memoryPartitionParams.swizzId = pSmcEngine->GetGpuPartition()->GetSwizzId();

    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetGpuSubdevice(0)), 
        &memoryPartitionHandle, 
        AMPERE_SMC_PARTITION_REF, 
        &memoryPartitionParams));

    LwRm::Handle exelwtionPartitionHandle;
    LWC638_ALLOCATION_PARAMETERS exelwtionPartitionParams = { };
    exelwtionPartitionParams.execPartitionId = pSmcEngine->GetExelwtionPartitionId();

    CHECK_RC(pLwRm->Alloc(memoryPartitionHandle, 
        &exelwtionPartitionHandle, 
        AMPERE_SMC_EXEC_PARTITION_REF, 
        &exelwtionPartitionParams));

    CHECK_RC(GetGpuDevice()->AllocSingleClientStuff(pLwRm, GetGpuSubdevice(0)));

    m_MapSmcEngineLwRmPtr[pSmcEngine] = pLwRm;
    m_MapSmcEngineMemoryPartitionHandle[pSmcEngine] = memoryPartitionHandle;
    m_MapSmcEngineExelwtionPartitionHandle[pSmcEngine] = exelwtionPartitionHandle;

    return rc;
}

RC LWGpuResource::FreeRmClient(GpuPartition * pGpuPartition)
{
    RC rc;

    LwRm * pLwRm = GetLwRmPtr(pGpuPartition);
    MASSERT(pLwRm);

    LwRm::Handle Handle = m_MapPartitionResourceHandle[pGpuPartition];
    pLwRm->Free(Handle);
    m_MapPartitionResourceHandle.erase(pGpuPartition);

    CHECK_RC(GetGpuDevice()->Free(pLwRm));

    m_MapPartitionLwRmPtr.erase(pGpuPartition);
    
    return rc;
}

RC LWGpuResource::FreeRmClient(SmcEngine* pSmcEngine)
{
    RC rc;
    LwRm* pLwRm = GetLwRmPtr(pSmcEngine);
    MASSERT(pLwRm);

    FreeDmaChannels(pLwRm);

    pLwRm->Free(m_MapSmcEngineExelwtionPartitionHandle[pSmcEngine]);
    m_MapSmcEngineExelwtionPartitionHandle.erase(pSmcEngine);

    pLwRm->Free(m_MapSmcEngineMemoryPartitionHandle[pSmcEngine]);
    m_MapSmcEngineMemoryPartitionHandle.erase(pSmcEngine);

    CHECK_RC(GetGpuDevice()->Free(pLwRm));
    m_MapSmcEngineLwRmPtr.erase(pSmcEngine);

    return rc;
}

// Free RM Client / Device / Subdevice
RC LWGpuResource::FreeRmClients()
{
    RC rc;

    for (auto& partitionLwRmPtr : m_MapPartitionLwRmPtr)
    {
        LwRm* pLwRm = partitionLwRmPtr.second;
        LwRm::Handle Handle = m_MapPartitionResourceHandle[partitionLwRmPtr.first];

        pLwRm->Free(Handle);
        m_MapPartitionResourceHandle.erase(partitionLwRmPtr.first);

        CHECK_RC(GetGpuDevice()->Free(pLwRm));
    }
    m_MapPartitionLwRmPtr.clear();

    return rc;
}

LwRm* LWGpuResource::GetLwRmPtr(GpuPartition* pGpuPartition) const
{
    if (m_MapPartitionLwRmPtr.find(pGpuPartition) != m_MapPartitionLwRmPtr.end())
        return m_MapPartitionLwRmPtr.at(pGpuPartition);
    return nullptr;
} 

LwRm* LWGpuResource::GetLwRmPtr(UINT32 swizzid)
{
    for (auto i : m_MapPartitionLwRmPtr)
    {
        if (i.first->GetSwizzId() == swizzid)
        {
            return i.second;
        }
    }
    return nullptr;
}

LwRm* LWGpuResource::GetLwRmPtr(SmcEngine* pSmcEngine) const
{
    if (m_MapSmcEngineLwRmPtr.find(pSmcEngine) != m_MapSmcEngineLwRmPtr.end())
        return m_MapSmcEngineLwRmPtr.at(pSmcEngine);
    return nullptr;
}

LwRm::Handle LWGpuResource::GetGpuPartitionHandle(GpuPartition* pGpuPartition) const
{
    if (m_MapPartitionResourceHandle.find(pGpuPartition) == 
        m_MapPartitionResourceHandle.end())
    {
        MASSERT(!"GpuPartition has not been allocated. Could not find handle.");
        return 0;
    }
    else
    {
        return m_MapPartitionResourceHandle.at(pGpuPartition);
    }
}

void LWGpuResource::FreeSmcRegHal(SmcEngine* pSmcEngine)
{
    // Mointor thread uses the SmcRegHal to read PGRAPH registers
    // It could get the SmcRegHals and then yield and allow this function to clear one SmcRegHal
    // leading to obsolete data in monitor thread
    // This mutex will ensure that all pending monitor infos are queried before freeing up SmcRegHals
    // or SmcRegHals are freed before starting to print monitor info
    Tasker::MutexHolder mh(m_SmcRegHalMutex);

    SmcRegHal* pSmcRegHal = nullptr;

    for (auto const& regHal : m_SmcRegHals)
    {
        if (GetLwRmPtr(pSmcEngine) == regHal->GetLwRmPtr())
        {
            pSmcRegHal = regHal;
            break;
        }
    }

    if (pSmcRegHal)
    {
        m_SmcRegHals.erase(
            remove(m_SmcRegHals.begin(), m_SmcRegHals.end(), pSmcRegHal), 
            m_SmcRegHals.end());

        delete pSmcRegHal;
    }
}

void LWGpuResource::FreeSmcRegHals()
{
    // Mointor thread uses the SmcRegHal to read PGRAPH registers
    // It could get the SmcRegHals and then yield and allow this function to clear SmcRegHals
    // leading to obsolete data in monitor thread
    // This mutex will ensure that all pending monitor infos are queried before freeing up SmcRegHals
    // or SmcRegHals are freed before starting to print monitor info
    Tasker::MutexHolder mh(m_SmcRegHalMutex);

    for (auto& regHal: m_SmcRegHals)
    {
        delete regHal;
    }

    m_SmcRegHals.clear();
}

void LWGpuResource::AllocSmcRegHal(SmcEngine* pSmcEngine)
{
    GpuSubdevice* pSubdev = GetGpuSubdevice();
    GpuPartition* pGpuPartition = pSmcEngine->GetGpuPartition();

    for (auto const& regHal : m_SmcRegHals)
    {
        if (GetLwRmPtr(pSmcEngine) == regHal->GetLwRmPtr())
        {
            // SmcEngine RegHal already allocated
            return;
        }
    }

    SmcRegHal *pSmcRegHal = new SmcRegHal(pSubdev, GetLwRmPtr(pSmcEngine), pGpuPartition->GetSwizzId(), 0);
    pSmcRegHal->Initialize();
    m_SmcRegHals.push_back(pSmcRegHal);
}

void LWGpuResource::AllocSmcRegHals()
{
    // SmcRegHal for SMC mode only
    MASSERT(m_pSmcResourceController);
    GpuSubdevice* pSubdev = GetGpuSubdevice();
    for (auto& gpuPartition : m_pSmcResourceController->GetActiveGpuPartition())
    {
        // Add SmcRegHal for each SMC engine. which is used to access PGRAPH register;
        for (auto& smcEngine : gpuPartition->GetActiveSmcEngines())
        {
            AllocSmcRegHal(smcEngine);
        }
    }

    pSubdev->RegisterGetSmcRegHalsCB(std::bind(&LWGpuResource::GetSmcRegHals, this, std::placeholders::_1));
}

RegHal& LWGpuResource::GetRegHal(GpuSubdevice* pSubdev, LwRm* pLwRm, SmcEngine* pSmcEngine)
{
    if (!pSubdev->IsSMCEnabled())
    {
        return pSubdev->Regs();
    }

    // SMC enabled
    RegHal *pRegHal = nullptr;
    MASSERT(pSmcEngine);
    MASSERT(pLwRm);

    if (pSmcEngine)
    {
        for (auto& regHal: m_SmcRegHals)
        {
            if (pLwRm == regHal->GetLwRmPtr())
            {
                pRegHal = regHal;
            }
        }
    }

    if (!pRegHal)
    {
        MASSERT(!"Can't find SmcReghal");
    }

    return *pRegHal;
}

RegHal& LWGpuResource::GetRegHal(UINT32 gpuSubdevIdx, LwRm* pLwRm, SmcEngine* pSmcEngine)
{
    GpuSubdevice* pGpuSubdevice = GetGpuSubdevice(gpuSubdevIdx);
    return GetRegHal(pGpuSubdevice, pLwRm, pSmcEngine);
}

UINT32 LWGpuResource::GetMaxSubCtxForSmcEngine(LwRm* pLwRm)
{
    for (const auto& smcLwRmPtr : m_MapSmcEngineLwRmPtr)
    {
        if (smcLwRmPtr.second == pLwRm)
        {
            return smcLwRmPtr.first->GetVeidCount();
        }
    }
    MASSERT(!"GetMaxSubCtxForSmcEngine(): Could not find matching SmcEngine for LwRm client\n");
    return ~0U;
}

// Checks if there are multiple partitions existing in SMC mode
bool LWGpuResource::MultipleGpuPartitionsExist()
{
    if (m_pSmcResourceController && 
        m_pSmcResourceController->GetActiveGpuPartition().size() > 1)
        return true;
    else
        return false;
}

SmcEngine* LWGpuResource::GetSmcEngine(LwRm* pLwRm)
{
    for (const auto& smcLwRmPtr : m_MapSmcEngineLwRmPtr)
    {
        if (smcLwRmPtr.second == pLwRm)
        {
            return smcLwRmPtr.first;
        }
    }
    return nullptr;
}

UINT32 LWGpuResource::GetSwizzId(LwRm* pLwRm)
{
    SmcEngine* pSmcEngine = GetSmcEngine(pLwRm);
    MASSERT(pSmcEngine);
    return pSmcEngine->GetGpuPartition()->GetSwizzId();
}

string LWGpuResource::GetSmcEngineName(LwRm* pLwRm)
{
    SmcEngine* pSmcEngine = GetSmcEngine(pLwRm);
    MASSERT(pSmcEngine);
    return pSmcEngine->GetName();
}

bool LWGpuResource::IsSmcMemApi()
{
    MASSERT(m_pSmcResourceController);
    return m_pSmcResourceController->IsSmcMemApi();
}

void* LWGpuResource::GetSmcRegHals(vector<SmcRegHal*> *pSmcRegHals) 
{
    *pSmcRegHals = m_SmcRegHals;
    return m_SmcRegHalMutex;
}

void LWGpuResource::SetTestDirectory(TestDirectory *pTestDirectory)
{
    if (pTestDirectory && s_pTestDirectory)
    {
        ErrPrintf("Trying to link TestDirectory to LWGpuResource, when a TestDirectory is already linked!\n");
    }
    else
    {
        s_pTestDirectory = pTestDirectory;
    }
}

void LWGpuResource::DescribeResourceParams(const char *name)
{
    ArgReader::DescribeParams(LWGpuResource::ParamList);
    RawPrintf("\n");
}

RC LWGpuResource::RegisterOperationsResources()
{
    RC rc;

    for (auto pLWGpuResource : s_LWGpuResources)
    {
        CHECK_RC(pLWGpuResource->RegisterOperations());
    }

    return rc;
}

RC LWGpuResource::InitSmcResource()
{
    RC rc;

    for (auto pLWGpuResource : s_LWGpuResources)
    {
        SmcResourceController * pSmcMgr = pLWGpuResource->GetSmcResourceController();
        if (pSmcMgr != nullptr)
        {
            rc = pSmcMgr->Init();
            if (rc != OK)
            {
                ErrPrintf("Can't init SmcResourceController successfully.\n");
                return rc;
            }

            // SmcRegHals should be allocated after all RM clients are allocated.
            pLWGpuResource->AllocSmcRegHals();

            pLWGpuResource->SmcSanityCheck();

            //Handle the register after SMC initialization is done in SMC mode
            pLWGpuResource->ParseHandleOptionalRegisters(
                pLWGpuResource->GetLwgpuArgs(), pLWGpuResource->GetGpuSubdevice());
        }
    }

    return rc;
}

void LWGpuResource::PrintSmcInfo()
{
    RC rc;

    for (auto pLWGpuResource : s_LWGpuResources)
    {
        SmcResourceController * pSmcMgr = pLWGpuResource->GetSmcResourceController();
        if (pSmcMgr != nullptr)
        {
            pSmcMgr->PrintSmcInfo();
        }
    }
}

LWGpuResource* LWGpuResource::FindFirstResource()
{
    if (s_LWGpuResources.size() == 0)
    {
        return nullptr;
    }

    return *(s_LWGpuResources.begin());
}

void LWGpuResource::FreeResources()
{
    // Delete resources in reverse order they were created
    for (auto rItr = s_LWGpuResources.rbegin() ; rItr != s_LWGpuResources.rend(); ++rItr)
    {
        delete *rItr;
        *rItr = nullptr;
    }
    s_LWGpuResources.clear();
}

void LWGpuResource::DispPipeConfiguratorSetScanout(bool* pAborted)
{
    if (m_pDispPipeConfigurator && !m_DpcScanoutSet)
    {
        m_pDispPipeConfigurator->SetShowScanLine(true);
        m_pDispPipeConfigurator->WaitUntilScanout(pAborted);
        m_DpcScanoutSet = true;
    }
}

RC LWGpuResource::DispPipeConfiguratorRunTest
(
    bool* pAborted, 
    MdiagSurf* pLumaSurface, 
    MdiagSurf* pChromaSurface, 
    bool bSyncScanoutEnd
)
{
    RC rc;
    if (m_pDispPipeConfigurator && !m_DpcTestRun)
    {
        if (m_pDispPipeConfigurator->GetExelwteAtEnd())
        {
            // External surface mode is only supported with option "-disp_cfg_json_exelwte_at_end"
            if (pLumaSurface && pChromaSurface)
            {
                m_pDispPipeConfigurator->SetLumaSurface(pLumaSurface->GetSurf2D());
                m_pDispPipeConfigurator->SetChromaSurface(pChromaSurface->GetSurf2D());
                m_pDispPipeConfigurator->SetExtSurfaceMode(true);
            }

            rc = m_pDispPipeConfigurator->ExtTriggerModeStart();
            if (rc != OK)
            {
                return rc;
            }
        }

        if (!m_pDispPipeConfigurator->GetSkipCRCCapture() && bSyncScanoutEnd)
        {
            // Per bug 519833 let the display scanout the current frame
            // that the traces were run at to the end, so it is possible
            // to verify the pixel values were correct in the complete frame
            m_pDispPipeConfigurator->WaitUntilScanout(pAborted);
        }

        if (m_pDispPipeConfigurator->GetExtTriggerMode())
        {
            rc = m_pDispPipeConfigurator->ExtTriggerModeEndTestAndCheckCrcs();
            if (rc != OK)
            {
                return rc;
            }
        }
        m_DpcTestRun = true;
    }
    return rc;
}

RC LWGpuResource::DispPipeConfiguratorCleanup()
{
    if (m_pDispPipeConfigurator && !m_DpcTestCleanup)
    {
        m_DpcTestCleanup = true;
        return m_pDispPipeConfigurator->Cleanup();
    }
    return OK;
}
