/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include <iostream>
#include <map>
#include <set>

#include "lwgpu_ch_helper.h"
#include "lwgpu_subchannel.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/thread.h"
#include "core/include/massert.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "mdiag/utils/buf.h"
#include "mdiag/utils/types.h"
#include "mdiag/utils/crc.h"

#include "lwgpu_single.h"
#include "mdiag/tests/test.h"
#include "mdiag/tests/testdir.h"
#include "class/cl9097.h" // FERMI_A
#include "class/cla097.h"       // KEPLER_A
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO

typedef map<string, LWGpuChannel*> ChannelNameMap;
// The key SmcEngine* can be nullptr in case of non-SMC mode
static map<SmcEngine*, ChannelNameMap> channels;
static map<Thread*, GpuChannelHelper*> thread_channel_helper_map;

LWGpuSubChannel* get_subchannel_object(LWGpuChannel* ch, int subch);

class ChannelHelper : public GpuChannelHelper {

public:
    ChannelHelper(const char *name, LWGpuResource *pLwGpu,
                  const Test* test, 
                  LwRm *pLwRm,
                  SmcEngine *pSmcEngine);
    ~ChannelHelper();

    int ParseChannelArgs(const ArgReader* params);
    int ForceCtxswitch(bool mode);
    bool IsCtxswitch();
    int AllocChannel(LWGpuChannel* ch, UINT32 engineId, UINT32 hw_class);
    int SetupChannelNoPush(LWGpuChannel* ch);

private:
    bool              m_ctxswitch;
    vector<int>       m_ctxswPoints;
    vector<UINT32>    m_ctxswPercents;
    int               m_ctxswGran;
    int               m_ctxswNum;
    int               m_ctxswNumReplay;
    int               m_ctxswSeed0, m_ctxswSeed1, m_ctxswSeed2;
    int               m_ctxswSelf;
    int               m_ctxswSingleStep;
    int               m_ctxswDebug;
    bool              m_ctxswBash;
    int               m_ctxswReset;
    int               m_ctxswLog;
    int               m_ctxswSerial;
    int               m_ctxswResetOnly;
    bool              m_ctxswWFIOnly;
    int               m_ctxswResetMask[10];  // more than enough hopefully
    bool              m_ctxswStopPull;
    int               m_ctxswTimeSlice;
    vector<string>*   m_ctxswMethodsList;

    int               m_cannotCtxsw;            // HACK to not allow subchannel switch when DMA is still going on

    UINT32              m_ctxswType;
    const char *      m_pLimiterFileName;
    unsigned int      m_semAcquireTimeout;

    bool VerifyParameters(const ArgReader* params) const;
public:

    struct ChHelperTestEventListener : TestDirectory::Listener
    {
        TestDirectory *m_testdir;
        ChannelHelper *m_chHelper;

        ChHelperTestEventListener(TestDirectory *testdir, ChannelHelper* chHelper) :
            m_testdir(testdir),
        m_chHelper(chHelper)
        {
            m_testdir->add_listener(this);
            thread_channel_helper_map[Thread::LwrrentThread()] = m_chHelper;
        }

        ~ChHelperTestEventListener()
        {
            m_testdir->remove_listener(this);
            thread_channel_helper_map[Thread::LwrrentThread()] = 0;
        }
        void activate_test()
        {
            thread_channel_helper_map[Thread::LwrrentThread()] = m_chHelper;
        }

        void deactivate_test()
        {
            thread_channel_helper_map[Thread::LwrrentThread()] = 0;
        }
    };

    void set_class_ids(unsigned num_classes, const UINT32 class_ids[]);
    bool acquire_gpu_resource();
    void release_gpu_resource();

    bool acquire_channel();
    bool is_channel_setup() const;
    void channel_is_setup(bool is_setup);
    bool alloc_channel(UINT32 engineId, UINT32 hwClass);
    void release_channel();

    LWGpuSubChannel *create_subchannel(UINT32 objHandle);
    LWGpuSubChannel *create_object(UINT32 id, void *params);
    void reassign_subchannel_number(LWGpuSubChannel *subch, int old_num);

    LWGpuResource* gpu_resource() const { return m_LwGpu; }
    LWGpuChannel* channel() const { return m_Channel; }
    const char* testname() const { return m_testname; }

    void SetMethodCount(int num_mth);
    void SetMethodWriteCount(int num_writes);
    void BeginChannelSwitch();
    void EndChannelSwitch();

    bool subchannel_switching_enabled() const;
    void notify_method_write(LWGpuChannel* ch, int subch, UINT32 method, MethodType mode);
    bool has_subchannel_switch_event() const;
    bool next_thread_is_on_same_channel() const;
    void setCannotCtxsw(int);
    int getCannotCtxsw();
    unsigned method_count() const { return m_method_count; }
    void SetSmcEngine(SmcEngine* pSmcEngine) { m_pSmcEngine = pSmcEngine; }
    void SetLwRmPtr(LwRm* pLwRm) { m_pLwRm = pLwRm; }

private:
    string method_name(UINT32 method) const;

    const char *m_testname;
    const Test *m_Test;
    string m_channelName;
    unsigned m_num_classes;
    const UINT32 *m_class_ids;
    bool m_subchannel_switching_enabled;
    set<unsigned> m_subchsw_points;
    set<string> m_subchsw_methods;
    unsigned m_subchsw_num;
    unsigned m_subchsw_gran;

    bool m_time_for_subchannel_switch;
    unsigned m_method_count;
    RandomStream *m_subch_random;

    LWGpuResource* m_LwGpu;
    LWGpuChannel* m_Channel;
    LwRm* m_pLwRm;
    SmcEngine* m_pSmcEngine;

    unique_ptr<ChHelperTestEventListener> m_pTestdirListener;
};

class ChHelperChannelListener : public LWGpuChannel::PushBufferListener
{
    set<GpuChannelHelper*> m_tests_since_flush; // test<->helper is 1:1 relationship
    bool m_subchsw_seen;

public:
    ChHelperChannelListener() : m_tests_since_flush(), m_subchsw_seen(false)
    {}

    ~ChHelperChannelListener()
    {
        DebugPrintf("DAVEW: ~ChHelperChannelListener\n");
        if (m_tests_since_flush.size() > 1)
        {
            m_subchsw_seen = true;
        }
        if (!m_subchsw_seen)
        {
            Test::FailAllTests("Subchannel switching enabled, but no subchannel switches generated");
        }
    }

    void notify_MethodWrite(LWGpuChannel* ch, int subch, UINT32 method, MethodType mode)
    {
        GpuChannelHelper *helper = GpuChannelHelper::LwrrentChannelHelper();

        if (helper && helper->subchannel_switching_enabled())
        {
            helper->notify_method_write(ch, subch, method, mode);
            if (helper->has_subchannel_switch_event())
            {
                if (!helper->next_thread_is_on_same_channel())
                {
                    WarnPrintf("Subchannel switch requested, but next thread is not on same channel!\n");
                    ch->WaitForIdle();
                }
                if (helper->getCannotCtxsw() == 0) {
                    DebugPrintf("DAVEW: notify_MethodWrite #%d (subch=%d) -- doing subch switch\n",
                            helper->method_count(), subch);
                    m_tests_since_flush.insert(helper);

                    //LWGpuSubChannel* subch_obj = get_subchannel_object(ch, subch);
                    //if (subch_obj) subch_obj->flush_volatile_state();

                    RC rc = helper->gpu_resource()->GetContextScheduler()->YieldToNextSubchThread();
                    if (OK != rc)
                    {
                        MASSERT(!"%s Yield to next subchannel thread failed\n");
                    }
                }
                else
                {
                    DebugPrintf("DAVEW: notify_MethodWrite #%d (subch=%d) -- Can't subch switch (in critical region\n",
                            helper->method_count(), subch);
                }
            }
            else
            {
                DebugPrintf("DAVEW: notify_MethodWrite #%d (subch=%d) -- no subchswitch event\n",
                        helper->method_count(), subch);
            }
        }
    }

    void notify_Flush(LWGpuChannel*)
    {
        DebugPrintf("DAVEW: notify_Flush\n");
        if (m_tests_since_flush.size() > 1)
        {
            m_subchsw_seen = true;
        }
        m_tests_since_flush.clear();
    }
};

const ParamDecl GpuChannelHelper::Params[] = {
    SIMPLE_PARAM("-noop", "Dummy argument to make testgen level creation easier"),

    SIMPLE_PARAM("-no_ctxswitch", "override -ctxswitch"),
    STRING_PARAM("-use_channel", "specify a name to associate with the channel used by the test. Use to force multiple tests to use the same change (see -subchswitch)"),

    SIMPLE_PARAM("-subchswitch", "enable subchannel switching between tests that share a channel (see -use_channel)"),
    {"-subchsw_point", "uuuuuuuuuuuuuuuuuuuuuuuuuuuuuu", ParamDecl::PARAM_FEWER_OK, 0, 0,
        "Specify the methods at which to execute a subchannel switch, format -subchsw_point pt1 pt2 ... ptn (n<=30)"},
    {"-subchsw_method", "t",  ParamDecl::PARAM_MULTI_OK, 0, 0, "do subchannel switch after specifed method"},
    {"-subchsw_gran","u",ParamDecl::PARAM_ENFORCE_RANGE,0x0,0xFFFF,"subchsw_gran: average number of MethodWrites between subchannel switches (0-65535)"},
    {"-subchsw_num","u",ParamDecl::PARAM_ENFORCE_RANGE,0x0,0xFFFF,"subchsw_num: total number of subchannel switches to execute during this test (0-65535)"},
    {"-subchannel_base","u",ParamDecl::PARAM_ENFORCE_RANGE,0x0,0x7,"subchannel_base: base id of where subchannel ids get allocated from (0-7)"},
    UNSIGNED_PARAM("-subchsw_seed0", "Random stream seed for subchannel switching"),
    UNSIGNED_PARAM("-subchsw_seed1", "Random stream seed for subchannel switching"),
    UNSIGNED_PARAM("-subchsw_seed2", "Random stream seed for subchannel switching"),

    SIMPLE_PARAM("-ctxswitch", "run multiple graphics diags, context switching between them"),
    SIMPLE_PARAM("-ctxswTimeSlice", "Enable PTimer, let HOST context switch between channels"),
    {"-ctxswPoint", "ssssssssssssssssssssssssssssss", ParamDecl::PARAM_FEWER_OK, 0, 0,
        "Specify the signed offset at which to execute a context switch, format -ctxswPoint pt1 pt2 ... ptn (n<=30)"},
    {"-ctxsw_percent", "uuuuuuuuuuuuuuuuuuuuuuuuuuuuuu", ParamDecl::PARAM_FEWER_OK, 0, 0,
        "Specify the percentage of total methods at which to execute a context switch, format -ctxswPoint pt1 pt2 ... ptn (ptn<=100;n<=30)"},
    UNSIGNED_PARAM("-ctxsw_scan_method", "Specify the method which ctxsw testpoints are relative to, format -ctxsw_scan_method mthaddr. supported on Fermi and later chips."),
    {"-ctxswMethod", "t",  ParamDecl::PARAM_MULTI_OK, 0, 0, "do context switch after specifed method"},
    {"-ctxswGran","u",ParamDecl::PARAM_ENFORCE_RANGE,0x0,0xFFFF,"ctxswGran: average number of MethodWrites between context switches (0-65535)"},
    {"-ctxswNum","u",ParamDecl::PARAM_ENFORCE_RANGE,0x0,0xFFFF,"ctxswNum: total number of context switches to execute during this test (0-65535)"},
    {"-ctxswNumReplay","u",ParamDecl::PARAM_ENFORCE_RANGE,0x0,0xFFFF,"ctxswNumReplay: total number of Spill Replay Only to execute during this test (0-65535)"},
    {"-limiterFile", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Specify Limiter File Path"},
    UNSIGNED_PARAM("-ctxswSeed0", "Random stream seed for ctx switching"),
    UNSIGNED_PARAM("-ctxswSeed1", "Random stream seed for ctx switching"),
    UNSIGNED_PARAM("-ctxswSeed2", "Random stream seed for ctx switching"),
    SIMPLE_PARAM("-ctxswSelf", "force a context switches against itself by forcing a save then restore of this channel"),
    SIMPLE_PARAM("-ctxswBash", "bash state before doing restore phase of context switch"),
    SIMPLE_PARAM("-ctxswLog", "Context Switch Log File"),
    SIMPLE_PARAM("-ctxswSerial", "have the ctxsw ucode serialize the context switching of the regs and the rams"),
    SIMPLE_PARAM("-ctxswSingleStep", "have the ctxsw ucode single step its exelwtion. Each instruction sends an interrupt to the cpu"),
    UNSIGNED_PARAM("-ctxswDebug", "select a debug mode (0 or 1) for the ucode to run in"),
    {"-ctxswReset","u",0,0,0, "do a [partial] gfx reset before doing restore phase of context switch by momentarily writing this value into ctxsw_reset register"},
    {"-ctxswResetOnly","u",0,0,0, "do a [partial] gfx reset at ctxsw points [but dont do the save/restore] by momentarily writing this value into ctxsw_reset register"},
    SIMPLE_PARAM("-ctxswWFIOnly", "force a WaitForIdle(WFI) at ctxsw points, but don't actually switch"),
    SIMPLE_PARAM("-ctxswStopPull", "stop the puller at ctxsw event points"),

    UNSIGNED64_PARAM("-pushbufsize", "size of push buffer in bytes (default is 1048576)"),
    UNSIGNED_PARAM("-gpfifo_entries", "size of GPFIFO in entries (Must be the power of 2, default is 512)"),
    UNSIGNED_PARAM("-kickoff_thresh", "autoflush threshold in bytes of pushbuf (default is 256)"),
    UNSIGNED_PARAM("-auto_gp_entry_thresh", "automatically write GPFIFO entry when reach threshold in bytes of pushbuf"),
    UNSIGNED_PARAM("-semAcquireTimeout", "set semaphore acquire timeout (in 1024 ns"),
    SIMPLE_PARAM("-use_bar1_doorbell", "Use BAR1 doorbell ring for work submission."),

    {"-random_af_threshold",           "uuu", 0, 0, 0, "Random value from min <x> to max <y> with seed <z> for \"-kickoff_thresh\""},
    {"-random_af_threshold_gp_ent",    "uuu", 0, 0, 0, "Random value from min <x> to max <y> with seed <z> for how many GPFIFO entries should be written before autoflush"},
    {"-random_auto_gpentry_threshold", "uuu", 0, 0, 0, "Random value from min <x> to max <y> with seed <z> for \"-auto_gp_entry_thresh\""},
    {"-pause_before_gpwrite",          "uuu", 0, 0, 0, "Randomly pause for min <x> to max <y> cycles with seed <z>"},
    {"-pause_after_gpwrite",           "uuu", 0, 0, 0, "Randomly pause for min <x> to max <y> cycles with seed <z>"},

    SIMPLE_PARAM("-single_kick", "Disable autoflush, write PUT only once. Fail if pushbuf or gpfifo wraps."),

    { "-channel_type", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                             ParamDecl::GROUP_START), 0, 2, "Type of channel" },
    { "-pio",          "0", ParamDecl::GROUP_MEMBER, 0, 0, "PIO channel" },
    { "-dma",          "1", ParamDecl::GROUP_MEMBER, 0, 0, "DMA channel" },
    { "-gpfifo",       "2", ParamDecl::GROUP_MEMBER, 0, 0, "GPFIFO channel" },

    // We don't use MEMORY_ATTR_PARAMS here because not all standard buffer
    // parameters can be applied to all buffers.
    UNSIGNED_PARAM("-page_size", "override the page size for all buffers"),

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

    MEMORY_SPACE_PARAMS_NOP2P("_pb", "pushbuffer"),
    MEMORY_SPACE_PARAMS_NOP2P("_gpfifo", "GPFIFO"),
    MEMORY_SPACE_PARAMS_NOP2P("_err", "Error notifier"),

    MEMORY_LOC_PARAMS_NOP2P("_inst", "instance memory"),
    MEMORY_LOC_PARAMS_NOP2P("_ramfc", "RAMFC memory"),

    UNSIGNED_PARAM("-hash_table_size", "size of hash table in kilobytes"),

    UNSIGNED_PARAM("-timeslice", "length of channel's timeslice"),
    UNSIGNED_PARAM("-eng_timeslice", "for individual channel, length of channel's engine timeslice(us). "
                                     "value 0 can disable timeslice. For fermi and later."),
    UNSIGNED_PARAM("-tsg_timeslice", "for tsg, length of tsg's timeslice(us) shared by channels in tsg. "
                                     "Unlike -eng_timeslice, value 0 is invalid because tsg timeslice can't be disabled."),
    SIMPLE_PARAM("-ignore_tsgs", "Channels will not be associated with any TSG specified by the test. RM will allocate a default TSG per channel."),
    SIMPLE_PARAM("-ignore_subcontexts", "Channels will not be associated with any Subcontext specified by the test. RM will internally allocate VEID0."),
    UNSIGNED_PARAM("-pb_timeslice", "length of channel's pb timeslice, set value 0 can disable timeslice, only for Fermi"),
    UNSIGNED_PARAM("-timescale", "scale factor for channel's timeslice"),
    SIMPLE_PARAM("-no_timeout", "disable timeslicing timeout for channel"),

    { "-ctxswType", "u", (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 11, "ctxsw type" },
    { "-ctxswTypeWFI", "1", ParamDecl::GROUP_MEMBER, 0, 0, "old stype wait for idle ctxsw" },
    { "-ctxswTypeHalt",  "2", ParamDecl::GROUP_MEMBER, 0, 0, "Halt style ctxsw" },
    { "-ctxswTypeHaltOnMethod",   "3", ParamDecl::GROUP_MEMBER, 0, 0, "halt on method" },
    { "-ctxswTypeSpillReplayOnly",   "4", ParamDecl::GROUP_MEMBER, 0, 0, "Spill Replay Only" },
    { "-ctxswTypeHaltOnWFITimeout",   "5", ParamDecl::GROUP_MEMBER, 0, 0, "halt on wait for idle timeout" },
    { "-ctxswTypeWFIScanChain", "6", ParamDecl::GROUP_MEMBER, 0, 0, "wait for idle, but use scan chain to ctxsw state" },
    { "-ctxswTypeWFIAllPLI", "7", ParamDecl::GROUP_MEMBER, 0, 0, "wait for idle, but use PLI to ctxsw sram and flop state" },
    { "-ctxswTypeWFIFlopPLI", "8", ParamDecl::GROUP_MEMBER, 0, 0, "wait for idle, but use PLI to ctxsw flop state (ramchain for srams)" },
    { "-ctxswTypeHaltAllPLI", "9", ParamDecl::GROUP_MEMBER, 0, 0, "halt, but use PLI to ctxsw sram and flop state" },
    { "-ctxswTypeHaltFlopPLI", "10", ParamDecl::GROUP_MEMBER, 0, 0, "halt, but use PLI to ctxsw flop state (ramchain for srams)" },

    UNSIGNED_PARAM("-idle_method_count", "wait for idle after the channel processes every N methods from a test"),
    UNSIGNED_PARAM("-sleep_between_methods", "put a test to sleep for a given number of milliseconds"),

    UNSIGNED_PARAM("-idle_channels_retries", "retry count for LwRmIdleChannels when MORE_PROCESSING_REQUIRED is returned"),
    SIMPLE_PARAM("-use_legacy_wait_idle_with_risky_intermittent_deadlocking",
                 "Use RmIdleChannels to idle channels.  By default, fermi+ waits for a WFI semaphore, and RM is deprecating RmIdleChannels."),
    SIMPLE_PARAM("-skip_recovery", "Don't let RM attempt recovery with robust channels"),

    { "-timeout_ms", "f", (ParamDecl::ALIAS_START | ParamDecl::ALIAS_OVERRIDE_OK), 0, 0, "Amount of time in milliseconds to wait before declaring the test hung" },
    { "-hwWaitFail", "f", ParamDecl::ALIAS_MEMBER, 0, 0, "Amount of time in milliseconds to wait before declaring the test hung" },

    SIMPLE_PARAM("-force_cont", "Force contiguous system memory allocation (could fail)"),
    SIMPLE_PARAM("-pm_ncoh_no_snoop", "Temporary hack to disable snooping for Perf test on EMU/Silicon"),

    { "-zbc_mode", "u",
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1,
        "ZBC compression mode for all buffers" },
    { "-zbc_off", "0", ParamDecl::GROUP_MEMBER, 0, 0,
        "don't use ZBC compression for all buffers" },
    { "-zbc_on", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "use ZBC compression for all buffers" },
    { "-zbc", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "use ZBC compression for all buffers" },

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
        "use or don't use GPU cache for p2p buffer" },
    { "-gpu_p2p_cache_off", "0", ParamDecl::GROUP_MEMBER, 0, 0,
        "don't use GPU cache for p2p buffer" },
    { "-gpu_p2p_cache_on", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "use GPU cache for p2p buffer" },

    { "-split_gpu_cache_mode", "u",
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1,
        "use or don't use GPU cache for split memory half" },
    { "-split_gpu_cache_off", "0", ParamDecl::GROUP_MEMBER, 0, 0,
        "don't use GPU cache for split memory half" },
    { "-split_gpu_cache_on", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "use GPU cache for split memory half" },

    SIMPLE_PARAM("-hwcrccheck_gp", "Use OPCODE_GP_CRC to check GP entries on every waitforidle."),
    SIMPLE_PARAM("-hwcrccheck_pb", "Use OPCODE_PB_CRC to check PB buffer on every new GP entry."),
    SIMPLE_PARAM("-hwcrccheck_method", "Use CRC_Method to check methods sent to crossbar on every waitforidle."),
    SIMPLE_PARAM("-compress", "turn on compression for all memory regions"),

    { "-map_mode", "u",
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1,
        "Memory mapping mode for all buffers" },
    { "-map_direct", "0", ParamDecl::GROUP_MEMBER, 0, 0,
        "Use direct memory mapping for all buffers" },
    { "-map_reflected", "1", ParamDecl::GROUP_MEMBER, 0, 0,
        "Use reflected memory mapping for all buffers" },

    { "-alloc_chid_mode", "u",
        (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, LWGpuChannel::ALLOC_ID_MODS_LAST,
        "Channel ID selection mode in RM" },
    { "-alloc_chid_default", (const char *)LWGpuChannel::ALLOC_ID_MODE_DEFAULT,
        ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0,
        "After/include Kepler, -alloc_chid_rand is the default; before Keper, -alloc_chid_rm is the defalut" },
    { "-alloc_chid_incr", (const char *)LWGpuChannel::ALLOC_ID_MODE_GROWUP,
        ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0,
        "Allocate the lowest available ID" },
    { "-alloc_chid_decr", (const char *)LWGpuChannel::ALLOC_ID_MODE_GROWDOWN,
        ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0,
        "Allocate the highest available ID" },
    { "-alloc_chid_rand", (const char *)LWGpuChannel::ALLOC_ID_MODE_RANDOM,
        ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0,
        "Allocate a random available ID" },
    { "-alloc_chid_rm", (const char *)LWGpuChannel::ALLOC_ID_MODE_RM,
        ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0,
        "Let RM select an available ID" },

    SIMPLE_PARAM("-disable_location_override_for_mdiag", "Disable the surface location override (e.g. -force_fb) for all mdiag surfaces."),

    LAST_PARAM
};

const ParamConstraints lwgpu_ch_helper_constraints[] = {
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswSeed0"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswSeed1"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswSeed2"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswPoint"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswGran"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswNum"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswSelf"),
    MUTUAL_EXCLUSIVE_PARAM("-ctxswTimeSlice", "-ctxswReset"),
    LAST_CONSTRAINT
};

//------------------------------------------------------------------------------
//! \brief Verify channel parameters to avoid invalid parameter combinations
//!        Called in ParseChannelArgs()
//! return value: false - parameter combination is invalid
//!               true
bool ChannelHelper::VerifyParameters(const ArgReader* params) const
{
    //
    // bug 662296: disallow ctxswitch with pio.
    //
    if(params->ParamPresent("-ctxswitch") && params->ParamPresent("-channel_type"))
    {
        if (params->ParamUnsigned("-channel_type") == 0)
        {
            ErrPrintf("Option -pio cannot be used with -ctxswitch\n");
            return false;
        }
    }

    //
    // it would be better if we do mutual exclusive checking between
    // -ctxswTimeSlice and -serial. Unfortunately '-serial' is everywhere...
    //
    if(!params->ParamPresent("-ctxswitch") && params->ParamPresent("-ctxswTimeSlice"))
    {
        ErrPrintf("Option -ctxswTimeSlice need come with -ctxswitch\n");
        return false;
    }

    //
    // timeslice parameter verification
    //
    UINT32 engTimeslice = 0;
    UINT32 tsgTimeslice = 0;
    bool hasEngTimeslice = params->ParamPresent("-eng_timeslice") > 0;
    bool hasTsgTimeslice = params->ParamPresent("-tsg_timeslice") > 0;
    if (hasEngTimeslice)
    {
        engTimeslice = params->ParamUnsigned("-eng_timeslice");
    }
    if (hasTsgTimeslice)
    {
        tsgTimeslice = params->ParamUnsigned("-tsg_timeslice");
    }

    // bug 765010:
    // -tsg_timeslice 0 should always be an error, it's never valid.
    // No matter whether there is a tsg. Remind user to think about it.
    if (hasTsgTimeslice && tsgTimeslice == 0)
    {
        ErrPrintf("%s: -tsg_timeslice %d is never valid. \n",
            __FUNCTION__, tsgTimeslice);
        return false;
    }

    // bug 824084:
    // Invalid combinations which I'd like to see error out:
    // 1. non-tsg run: "-ignore_tsgs -eng_timeslice X"
    // 2. non-tsg run: "-ignore_tsgs -eng_timeslice X -tsg_timeslice Y" (X!=Y)
    // 3. non-tsg run: "-ignore_tsgs -tsg_timeslice X"
    if (params->ParamPresent("-ignore_tsgs") > 0)
    {
         if (hasEngTimeslice && hasTsgTimeslice)
         {
            if (engTimeslice != tsgTimeslice)
            {
                ErrPrintf("%s: Invalid parameter combination: "
                    "\"-ignore_tsgs -eng_timeslice %d -tsg_timeslice %d\". "
                    "It's invalid to specify different timeslice value. \n",
                    __FUNCTION__, engTimeslice, tsgTimeslice);
                return false;
            }
         }
         else if (hasTsgTimeslice)
         {
             ErrPrintf("%s: Invalid parameter combination: "
                 "\"-ignore_tsgs -tsg_timeslice %d\". \n",
                 __FUNCTION__, tsgTimeslice);
             return false;
         }
         else if (hasEngTimeslice)
         {
             ErrPrintf("%s: Invalid parameter combination: "
                 "\"-ignore_tsgs -eng_timeslice %d\". \n",
                 __FUNCTION__, engTimeslice);
             return false;
         }
    }

    return true;
}

#define LIMITER_FILENAME "Limiters"
int ChannelHelper::ParseChannelArgs(const ArgReader* params)
{
    if (!params) {
        return 0;
    }

    if (! params->ValidateConstraints(lwgpu_ch_helper_constraints) )
    {
        ErrPrintf("Test parameters are not valid.\n");
        return 0;
    }

    if (!VerifyParameters(params))
    {
        ErrPrintf("%s:Verify channel parameters failed!\n", __FUNCTION__);
        return 0;
    }

    // do stuff with args...
    m_semAcquireTimeout = params->ParamUnsigned("-semAcquireTimeout", 0);

    m_ctxswTimeSlice = params->ParamPresent("-ctxswTimeSlice");
    m_ctxswitch = GpuChannelHelper::IsCtxswEnabledByCmdline(params);
    m_ctxswGran = params->ParamUnsigned("-ctxswGran", (unsigned)-1); // HACK: the param is "unsigned" but the variable is signed: -1 implies not set.
    m_ctxswNum = params->ParamUnsigned("-ctxswNum", 0);
    m_ctxswNumReplay = params->ParamUnsigned("-ctxswNumReplay", 0);
    m_ctxswSeed0 = params->ParamUnsigned("-ctxswSeed0", 0x1111);
    m_ctxswSeed1 = params->ParamUnsigned("-ctxswSeed1", 0x2222);
    m_ctxswSeed2 = params->ParamUnsigned("-ctxswSeed2", 0x3333);
    m_ctxswSelf = params->ParamPresent("-ctxswSelf") > 0;
    m_ctxswSingleStep = params->ParamPresent("-ctxswSingleStep") > 0;
    m_ctxswDebug = params->ParamUnsigned("-ctxswDebug",999);
    m_ctxswBash = params->ParamPresent("-ctxswBash") > 0;
    m_ctxswStopPull = params->ParamPresent("-ctxswStopPull") > 0;

    m_ctxswPoints.clear();
    UINT32 ctxswPointsNum = params->ParamNArgs( "-ctxswPoint" );
    if (ctxswPointsNum > 0) {
        for (UINT32 i = 0; i < ctxswPointsNum; i++ )
        {
            m_ctxswPoints.push_back(params->ParamNSigned("-ctxswPoint", i));
        }
    }

    m_ctxswPercents.clear();
    UINT32 ctxswPercentsNum = params->ParamNArgs("-ctxsw_percent");
    if (ctxswPercentsNum > 0)
    {
        const UINT32 PERCENTAGELIMIT = 100;
        for (UINT32 i = 0; i < ctxswPercentsNum; i++ )
        {
            UINT32 val = params->ParamNUnsigned("-ctxsw_percent", i);
            if (val > PERCENTAGELIMIT)
            {
                ErrPrintf("-ctxsw_percent: Invalid value %d.\n", val);
                return 0;
            }

            m_ctxswPercents.push_back(val);
        }
    }

    m_ctxswMethodsList = new vector<string>;
    const vector<string>* list = params->ParamStrList("-ctxswMethod", NULL);
    if ( list && !list->empty() ) {
        auto lwrMethod = list->begin();
        while ( lwrMethod != list->end() ) {
            m_ctxswMethodsList->push_back(lwrMethod->c_str());
            lwrMethod++;
        }
    }

    m_ctxswReset = 0;
    if (params->ParamPresent("-ctxswReset"))
    {
        m_ctxswReset = 1;
        m_ctxswResetMask[0] = params->ParamUnsigned("-ctxswReset", 0);
    }

    m_ctxswResetOnly = 0;
    if (params->ParamPresent("-ctxswResetOnly"))
    {
        m_ctxswResetOnly = 1;
        m_ctxswResetMask[0] = params->ParamUnsigned("-ctxswResetOnly", 0);
    }

    m_ctxswWFIOnly = params->ParamPresent("-ctxswWFIOnly") > 0;

    if( params->ParamPresent("-use_channel"))
    {
        m_channelName = params->ParamStr("-use_channel");
    }

    m_subchannel_switching_enabled = params->ParamPresent("-subchswitch") != 0;

    for (int
            i=params->ParamNArgs("-subchsw_point");
            i>0;
            --i)
    {
        unsigned point = params->ParamNUnsigned("-subchsw_point", i-1);
        m_subchsw_points.insert(point);
    }

    for (int
            i=params->ParamNArgs("-subchsw_method");
            i>0;
            --i)
    {
        string method = params->ParamNStr("-subchsw_method", i-1);
        m_subchsw_methods.insert(method);
    }

    if (params->ParamPresent("-subchsw_num"))
    {
        m_subchsw_num = params->ParamUnsigned("-subchsw_num");
    }

    if (params->ParamPresent("-subchsw_gran"))
    {
        m_subchsw_gran = params->ParamUnsigned("-subchsw_gran");
    }

    int seed0 = params->ParamUnsigned("-subchsw_seed0", 0x1111);
    int seed1 = params->ParamUnsigned("-subchsw_seed1", 0x2222);
    int seed2 = params->ParamUnsigned("-subchsw_seed2", 0x3333);

    m_subch_random = new RandomStream(seed0, seed1, seed2);

    m_ctxswLog = 0;
    if (params->ParamPresent("-ctxswLog"))
    {
        m_ctxswLog = 1;
    }

    m_ctxswSerial = 0;
    if (params->ParamPresent("-ctxswSerial"))
    {
        m_ctxswSerial = 1;
    }

    m_ctxswType = params->ParamUnsigned("-ctxswType", 0);

    m_pLimiterFileName = params->ParamStr("-limiterFile", LIMITER_FILENAME);

    return 1;
}

int ChannelHelper::ForceCtxswitch(bool mode)
{
    m_ctxswitch = mode;
    return 1;
}

bool ChannelHelper::IsCtxswitch()
{
    return m_ctxswitch;
}

int ChannelHelper::AllocChannel(LWGpuChannel* ch, UINT32 engineId, UINT32 hw_class)
{
    SetupChannelNoPush(ch);

    if (OK != ch->AllocChannel(engineId, hw_class))
    {
        ErrPrintf("Channel alloc failed!\n");
        return 0;
    }

    return 1;
}

int ChannelHelper::SetupChannelNoPush(LWGpuChannel* ch)
{
    ch->SetSemaphoreAcquireTimeout(m_semAcquireTimeout);

    if ( m_ctxswitch ) {
        ch->SetCtxSwTimeSlice(m_ctxswTimeSlice);
        ch->SetCtxSwGran(m_ctxswGran);
        ch->SetCtxSwNum(m_ctxswNum);
        ch->SetCtxSwPoints(&m_ctxswPoints);
        ch->SetCtxSwPercents(&m_ctxswPercents);
        ch->SetCtxSwSeeds(m_ctxswSeed0, m_ctxswSeed1, m_ctxswSeed2);
        if (m_ctxswSelf) ch->SetCtxSwSelf();
        ch->SetCtxSwReset(m_ctxswReset);
        ch->SetCtxSwResetOnly(m_ctxswResetOnly);
        ch->SetCtxSwWFIOnly(m_ctxswWFIOnly);
        ch->SetCtxSwResetMask(m_ctxswResetMask[0]);
        ch->SetCtxSwBash(m_ctxswBash);
        if (m_ctxswSerial) ch->SetCtxSwSerial();
        if (m_ctxswSingleStep) ch->SetCtxSwSingleStep();
        if (m_ctxswDebug !=999) ch->SetCtxSwDebug(m_ctxswDebug);
        ch->SetCtxSwStopPull(m_ctxswStopPull);
    } else {
        ch->SetCtxSwGran(-1);
    }

    ch->SetCtxSwNumReplay(m_ctxswNumReplay);
    ch->SetCtxSwMethods(m_ctxswMethodsList);

    if (m_ctxswLog)
        ch->SetCtxSwLog(1);

    if (m_ctxswType)
        ch->SetCtxSwType(m_ctxswType);

    ch->SetLimiterFileName(m_pLimiterFileName);

    return 1;
}

struct ChannelInfo
{
    unsigned ref_count;
    bool initialized;
    vector<LWGpuSubChannel*> subchannels;
    ChHelperChannelListener *listener;

    ChannelInfo() : ref_count(0), initialized(false), subchannels(Channel::NumSubchannels) {}
    // use default copy-ctor, assignment, dtor
};

static map<LWGpuChannel*, ChannelInfo> channel_info;

LWGpuSubChannel* get_subchannel_object(LWGpuChannel* ch, int subch)
{
    return channel_info[ch].subchannels[subch];
}

ChannelHelper::ChannelHelper(const char *test_name, LWGpuResource *pLwGpu,
                             const Test *test, 
                             LwRm *pLwRm,
                             SmcEngine *pSmcEngine) :
    m_ctxswitch(false),
    m_ctxswGran(-1),
    m_ctxswNum(0),
    m_ctxswNumReplay(0),
    m_ctxswSeed0(0x1111),
    m_ctxswSeed1(0x2222),
    m_ctxswSeed2(0x3333),
    m_ctxswSelf(0),
    m_ctxswSingleStep(0),
    m_ctxswDebug(999),
    m_ctxswBash(false),
    m_ctxswReset(0),
    m_ctxswLog(0),
    m_ctxswSerial(0),
    m_ctxswResetOnly(0),
    m_ctxswWFIOnly(0),
    m_ctxswStopPull(false),
    m_ctxswTimeSlice(0),
    m_ctxswMethodsList(0),
    m_cannotCtxsw(0),
    m_ctxswType(0),
    m_pLimiterFileName(0),
    m_semAcquireTimeout(0),
    m_testname(test_name),
    m_Test(test),
    m_channelName("NOT SET"),
    m_num_classes(0),
    m_class_ids(0),
    m_subchannel_switching_enabled(false),
    m_subchsw_num(0),
    m_subchsw_gran(0),
    m_time_for_subchannel_switch(false),
    m_method_count(0),
    m_subch_random(0),
    m_LwGpu(pLwGpu),
    m_Channel(0),
    m_pLwRm(pLwRm),
    m_pSmcEngine(pSmcEngine)
{
    setCannotCtxsw(0);

    if (pLwGpu)
    {
        m_pTestdirListener.reset(new ChHelperTestEventListener(
            LWGpuResource::GetTestDirectory(), this));
    }
}

ChannelHelper::~ChannelHelper()
{
    for (vector<LWGpuSubChannel*>::const_iterator
            iter = channel_info[m_Channel].subchannels.begin();
            iter != channel_info[m_Channel].subchannels.end();
            ++iter)
    {
        delete *iter;
    }
    delete m_subch_random;
    delete m_ctxswMethodsList;
}

void ChannelHelper::setCannotCtxsw(int val)
{
    m_cannotCtxsw = val;
}

int ChannelHelper::getCannotCtxsw()
{
    return m_cannotCtxsw;
}

bool ChannelHelper::subchannel_switching_enabled() const
{
    return m_subchannel_switching_enabled;
}

bool ChannelHelper::next_thread_is_on_same_channel() const
{
    GpuChannelHelper *next_helper = thread_channel_helper_map[NULL]; // XXX used to call Thread::NextThread, which was unimplemented

    return !next_helper || (m_Channel == next_helper->channel());
}

string ChannelHelper::method_name(UINT32 method) const
{
    unique_ptr<IRegisterClass> reg_class = gpu_resource()->GetRegister(method);
    return reg_class ? reg_class->GetName() : "";
}

void ChannelHelper::notify_method_write(LWGpuChannel* ch, int subch, UINT32 method, MethodType mode)
{
    if (m_subchsw_points.empty() && m_subchsw_methods.empty())
    {
        m_time_for_subchannel_switch = (m_subchsw_gran <= 1)
            || (m_subch_random->RandomUnsigned(1, m_subchsw_gran) == 1);
    }
    else if ((m_subchsw_points.find(m_method_count) != m_subchsw_points.end())
            || (!m_subchsw_methods.empty()
                && m_subchsw_methods.find(method_name(method)) != m_subchsw_methods.end()))
    {
        m_time_for_subchannel_switch = true;
    }
    else
    {
        m_time_for_subchannel_switch = false;
    }

    // HACK: subchannel ctx_switching is not allowed between some methods
    // turn off the switching flag if such case is detected.
    if (m_time_for_subchannel_switch)
    {
        LWGpuSubChannel* psubch = get_subchannel_object(ch, subch);
        if (psubch &&
            EngineClasses::IsGpuFamilyClassOrLater(
                psubch->get_classid(), LWGpuClasses::GPU_CLASS_KEPLER) &&
            (method == LW9097_NOTIFY))
        {
            InfoPrintf("Subch switching at NOTIFY method (method count %d) -- skipping.\n",
                m_method_count);
            m_time_for_subchannel_switch = false;
        }
        if (psubch && 
            EngineClasses::IsGpuFamilyClassOrLater(
                psubch->get_classid(), LWGpuClasses::GPU_CLASS_KEPLER))
        {
            // Assume the same offset applied to all classes for kepler and later
            // family. If the method offset can only apply to specific class,
            // the checking should be restricted to the related class.

            // !!! This also need to be reviewed in for Maxwell and later chips.
            switch (method)
            {
                case LWC36F_SEM_ADDR_LO:
                case LWC36F_SEM_ADDR_HI:
                case LWC36F_SEM_PAYLOAD_LO:
                case LWC36F_SEM_PAYLOAD_HI:
                    // These semaphore methods are only valid for Volta+.
                    if (!EngineClasses::IsGpuFamilyClassOrLater(
                        psubch->get_classid(), LWGpuClasses::GPU_CLASS_VOLTA))
                    {
                        break;
                    }
                case LWA097_LAUNCH_DMA:
                case LWA06F_SEMAPHOREA:
                case LWA06F_SEMAPHOREB:
                case LWA06F_SEMAPHOREC:
                case LWA097_LOAD_INLINE_DATA:
                    // In incrementing or incrementing_once mode, most if not all
                    // associated data should already be injected with current method,
                    // therefore it's safe to do subch ctxsw. Lwrrently this does
                    // not cover all scenario.
                    switch (mode)
                    {
                        case NON_INCREMENTING:
                            DebugPrintf("Subch switching at atomic method (0x%x) (method count %d) -- skipping.\n",
                                method, m_method_count);
                            m_time_for_subchannel_switch = false;
                            break;
                        default:
                            break;
                    }
                    break;
            }
        }
    }

    m_method_count++;
}

bool ChannelHelper::has_subchannel_switch_event() const
{
    return m_time_for_subchannel_switch;
}

void ChannelHelper::set_class_ids(unsigned num_classes, const UINT32 class_ids[])
{
    m_num_classes = num_classes;
    m_class_ids = class_ids;
}

bool ChannelHelper::acquire_gpu_resource()
{
    assert(!m_LwGpu);

    m_LwGpu = LWGpuResource::GetGpuByClassSupported(
                 m_num_classes, //1,
                 m_class_ids);

    if (!m_LwGpu)
    {
        return false;
    }

    // Create m_pTestdirListener object if it has not been created.
    //
    // Those mdiag tests except trace3d are calling this function to acquire gpu
    // resource; m_pTestdirListener is created here. For trace3d tests, gpu resource
    // has been decoupled with channel class. So it doesn't make sense to call
    // acquire_gpu_resource function.
    if (m_pTestdirListener.get() == NULL)
    {
        m_pTestdirListener.reset(new ChHelperTestEventListener(
            LWGpuResource::GetTestDirectory(), this));
    }

    return true;
}

void ChannelHelper::release_gpu_resource()
{
    if (!m_LwGpu) return;
    release_channel();

    m_LwGpu = 0;

    if (m_pTestdirListener.get())
    {
        m_pTestdirListener.reset();
    }
}

bool ChannelHelper::acquire_channel()
{
    DebugPrintf("%s: acquire_channel\n", testname());

    assert(!m_Channel);

    string ctxName;
    if (m_channelName == "NOT SET")
    {
        m_Channel = m_LwGpu->CreateChannel(m_pLwRm, m_pSmcEngine);
    }
    else
    {
        if (!channels[m_pSmcEngine][m_channelName])
        {
            channels[m_pSmcEngine][m_channelName] = m_LwGpu->CreateChannel(m_pLwRm, m_pSmcEngine);
        }
        m_Channel = channels[m_pSmcEngine][m_channelName];
        ctxName = m_channelName;
    }

    if (m_Channel)
    {
        if (OK != m_LwGpu->GetContextScheduler()->RegisterTestToContext(ctxName, m_Test))
        {
            ErrPrintf("%s: RegisterTestToContext while creating channels failed\n");
            return false;
        }

        if (++channel_info[m_Channel].ref_count == 1)
        {
            if (subchannel_switching_enabled())
            {
                channel_info[m_Channel].listener = new ChHelperChannelListener;
                m_Channel->add_listener( channel_info[m_Channel].listener );
            }
        }
        return true;
    }

    return false;
}

bool ChannelHelper::is_channel_setup() const
{
    MASSERT(m_Channel);
    return channel_info[m_Channel].initialized;
}

void ChannelHelper::channel_is_setup(bool is_setup)
{
    channel_info[m_Channel].initialized = is_setup;
}

bool ChannelHelper::alloc_channel(UINT32 engineId, UINT32 hwClass)
{
    MASSERT(m_Channel);

    if (is_channel_setup()) return true;

    channel_is_setup(AllocChannel(m_Channel, engineId, hwClass) != 0);
    return is_channel_setup();
}

void ChannelHelper::release_channel()
{
    if (!m_Channel) return;
    MASSERT(m_LwGpu);
    MASSERT(channel_info[m_Channel].ref_count != 0);

    if (--channel_info[m_Channel].ref_count)
    {
        // do nothing: channel is still in use
    }
    else
    {
        m_Channel->DestroyPushBuffer();

        for (vector<LWGpuSubChannel*>::iterator
               iter = channel_info[m_Channel].subchannels.begin();
             iter != channel_info[m_Channel].subchannels.end();
             ++iter)
          {
            (*iter) = 0;
          }

        channel_info[m_Channel].initialized = false;
        channels[m_pSmcEngine][m_channelName] = 0;
        delete m_Channel;
    }

    m_Channel = 0;
}

void ChannelHelper::reassign_subchannel_number(LWGpuSubChannel *subch, int old_num)
{
    MASSERT(channel_info[m_Channel].subchannels[old_num]);
    if (subch)
    {
        MASSERT(channel_info[m_Channel].subchannels[old_num] == subch);
        MASSERT(channel_info[m_Channel].subchannels[subch->subchannel_number()]);
        channel_info[m_Channel].subchannels[subch->subchannel_number()] = subch;
    }
    channel_info[m_Channel].subchannels[old_num] = 0;
}

LWGpuSubChannel *ChannelHelper::create_subchannel(UINT32 objHandle)
{
    UINT32 subchNum, subchClass;
    if (m_Channel->GetSubChannelNumClass(objHandle, &subchNum, &subchClass) != OK)
    {
        ErrPrintf("%s: ChannelHelper::create_subchannel: fail to get subchannel number/class.", testname());
        return 0;
    }

    if (subchNum >= channel_info[m_Channel].subchannels.size())
    {
        ErrPrintf("%s: ChannelHelper::create_subchannel: invalid subchNum", testname());
        return 0;
    }

    if (channel_info[m_Channel].subchannels[subchNum] != 0)
    {
        ErrPrintf("%s: ChannelHelper::create_subchannel: specified subchNum is unavailable", testname());
        return 0;
    }

    // create subchannel on given num
    LWGpuSubChannel *subch = mk_SubChannel(this, subchNum, subchClass, objHandle);
    if (!subch)
    {
        ErrPrintf("%s: ChannelHelper::create_subchannel: failed to create subchannel", testname());
        return 0;
    }
    channel_info[m_Channel].subchannels[subchNum] = subch;
    return subch;
}

LWGpuSubChannel *ChannelHelper::create_object(UINT32 id, void *params)
{
    // 1. create object
    UINT32 objHandle = m_Channel->CreateObject(id, params);
    if (objHandle == 0)
    {
        ErrPrintf("%s: %s: failed to create objhandle", testname(), __FUNCTION__);
        return 0;
    }

    // 2. create LWGpuSubChannel
    LWGpuSubChannel *subch = create_subchannel(objHandle);
    if (!subch)
    {
        ErrPrintf("%s: %s: failed to create subchannel", testname(), __FUNCTION__);
        return 0;
    }

    InfoPrintf("%s: ChannelHelper::CreateObject(ClassID=0x%08x) on subchannel %d of channel #%d\n",
        testname(), id, subch->subchannel_number(), m_Channel->ChannelNum());

    return subch;
}

void ChannelHelper::SetMethodCount(int count)
{
    assert(m_Channel);

    m_Channel->SetMethodCount(count);
}

void ChannelHelper::SetMethodWriteCount(int num_writes)
{
    assert(m_Channel);

    m_Channel->SetMethodWriteCount(num_writes);

    if (m_subchsw_num == 0) return;

    if (m_subchsw_num >= (unsigned)num_writes)
    {
        m_subchsw_gran = 1; // switch every method
        return;
    }

    while (m_subchsw_points.size() < (unsigned)m_subchsw_num)
    {
        m_subchsw_points.insert( m_subch_random->RandomUnsigned(0, num_writes-1) );
    }
}

void ChannelHelper::BeginChannelSwitch()
{
    if(m_Channel)
        m_Channel->BeginChannelSwitch();
    else
        DebugPrintf("%s: This test does not have a channel, and therefore can't be used for testing channel switching!\n", testname());
}

void ChannelHelper::EndChannelSwitch()
{
    // make sure the test has a channel before going off to cleanup channel switching
    // a number of tests use this base class for colwenience, but don't actually need a channel, and don't
    // expect to be used in channel switching.  This allows them to keep functioning!
    if(m_Channel)
        m_Channel->EndChannelSwitch();
}

GpuChannelHelper *GpuChannelHelper::LwrrentChannelHelper()
{
    return thread_channel_helper_map[Thread::LwrrentThread()];
}

bool GpuChannelHelper::IsCtxswEnabledByCmdline(const ArgReader* params)
{
    return (params->ParamPresent("-no_ctxswitch") == 0) &&
            (params->ParamPresent("-ctxswitch") > 0
             || params->ParamPresent("-ctxswTimeSlice") > 0);
}

GpuChannelHelper *mk_GpuChannelHelper
(
    const char *test_name,
    LWGpuResource *pLwGpu,
    Test *pTest,
    LwRm* pLwRm,
    SmcEngine *pSmcEngine
)
{
    return new ChannelHelper(test_name, pLwGpu, pTest, pLwRm, pSmcEngine);
}
