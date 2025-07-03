/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/chiplibtracecapture.h"

#include "core/include/fileholder.h"
#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/rc.h"
#include "core/include/refparse.h"
#include "core/include/tar.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "kepler/gk104/dev_master.h"  // LW_PMC_INTR_0
#include "kepler/gk110/dev_bus.h"
#include "kepler/gk110/dev_fifo.h"
#include "kepler/gk110/dev_flush.h"
#include "kepler/gk110/dev_graphics_nobundle.h" //LW_PGRAPH_STATUS
#include "kepler/gk110/dev_host.h"
#include "kepler/gk110/dev_perf.h"
#include "kepler/gk110/dev_pri_ringstation_fbp.h"
#include "kepler/gk110/dev_pri_ringstation_gpc.h"
#include "kepler/gk110/dev_pri_ringstation_sys.h"
#include "kepler/gk110/dev_pwr_pri.h"
#include "kepler/gk110/dev_ram.h"
#include "kepler/gk110/dev_timer.h"   // LW_PTIMER_TIME_0, LW_PTIMER_TIME_1
#include "kepler/gk110/dev_top.h"
#include "kepler/gk110/dev_trim.h"
#include "maxwell/gm20d/dev_chiplet_pwr.h"
#include "maxwell/gm20d/dev_fb.h"
#include "maxwell/gm20d/dev_ltc.h"
#include "maxwell/gm20d/dev_xbar.h"
#include "mdiag/sysspec.h"
#include "stdio.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <errno.h>
extern bool s_MultiHeapSupported;

struct RegisterBusMemOpInfo
{
    PHYSADDR RegAddr;
    ChiplibTraceDumper::RegOpInfo RegOpInfo;
    UINT32 RegSize;
    UINT32 RegAddrPitch;
};

#define MAKE_REGBUSMEMOPINF(addr, mask, flag, size, pitch) \
                         {addr, {mask, flag}, size, pitch}

static const RegisterBusMemOpInfo s_SpecialBusMemOpRegs[] =
{
    //                  startAddr                           mask,   flags,         size, pitch
    // registers whose returned value can be ignored
    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_PRIV_ERROR_INFO,         0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_PRIV_ERROR_CODE,         0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_PRIV_ERROR_ADR,          0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_PRIV_ERROR_WRDAT,        0, BusMemOp::RD_IGNORE, 1, 0),

    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_FBP0_PRIV_ERROR_INFO,    0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_FBP0_PRIV_ERROR_CODE,    0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_FBP0_PRIV_ERROR_ADR,     0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_FBP0_PRIV_ERROR_WRDAT,   0, BusMemOp::RD_IGNORE, 1, 0),

    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_FBPS_PRIV_ERROR_INFO,    0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_FBPS_PRIV_ERROR_CODE,    0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_FBPS_PRIV_ERROR_ADR,     0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_FBP_FBPS_PRIV_ERROR_WRDAT,   0, BusMemOp::RD_IGNORE, 1, 0),

    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_PRIV_ERROR_INFO,         0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_PRIV_ERROR_CODE,         0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_PRIV_ERROR_ADR,          0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_PRIV_ERROR_WRDAT,        0, BusMemOp::RD_IGNORE, 1, 0),

    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_GPC0_PRIV_ERROR_INFO,    0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_GPC0_PRIV_ERROR_CODE,    0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_GPC0_PRIV_ERROR_ADR,     0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_GPC0_PRIV_ERROR_WRDAT,   0, BusMemOp::RD_IGNORE, 1, 0),

    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_GPCS_PRIV_ERROR_INFO,    0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_GPCS_PRIV_ERROR_CODE,    0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_GPCS_PRIV_ERROR_ADR,     0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_GPC_GPCS_PRIV_ERROR_WRDAT,   0, BusMemOp::RD_IGNORE, 1, 0),

    MAKE_REGBUSMEMOPINF(LW_PPRIV_SYS_PRIV_ERROR_INFO,         0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_SYS_PRIV_ERROR_CODE,         0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_SYS_PRIV_ERROR_ADR,          0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPRIV_SYS_PRIV_ERROR_WRDAT,        0, BusMemOp::RD_IGNORE, 1, 0),

    MAKE_REGBUSMEMOPINF(LW_PTIMER_TIME_0,                     0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PTIMER_TIME_1,                     0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PFIFO_FB_TIMEOUT,                  0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PTOP_DEBUG_1,                      0, BusMemOp::RD_IGNORE, 1, 0),

    MAKE_REGBUSMEMOPINF(LW_PJTAG_ACCESS_CTRL,                 0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PJTAG_ACCESS_DATA,                 0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PJTAG_ACCESS_CONFIG,               0, BusMemOp::RD_IGNORE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PTRIM_SYS_JTAGINTFC,               0, BusMemOp::RD_IGNORE, 1, 0),

    // Rm reds LW_PFIFO_PBDMA_STATUS to get chid/tsgid; no state polling
    // _ID/_NEXT_ID mismatch in LW_PFIFO_PBDMA_STATUS is because sometimes they are not _VALID
    MAKE_REGBUSMEMOPINF(LW_PFIFO_PBDMA_STATUS(0),             0, BusMemOp::RD_IGNORE,
                        LW_PFIFO_PBDMA_STATUS__SIZE_1, LW_PFIFO_PBDMA_STATUS(1) - LW_PFIFO_PBDMA_STATUS(0)),

    // PM registers
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_SAMPLECNT(0),          0, BusMemOp::PM_COUNTER, 1, 0),
    // MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_SAMPLECNT(1),          0, BusMemOp::PM_COUNTER, 1, 0),
    // MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_SAMPLECNT(2),          0, BusMemOp::PM_COUNTER, 1, 0),
    // MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_SAMPLECNT(3),          0, BusMemOp::PM_COUNTER, 1, 0),
    // MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_SAMPLECNT(4),          0, BusMemOp::PM_COUNTER, 1, 0),
    // MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_SAMPLECNT(5),          0, BusMemOp::PM_COUNTER, 1, 0),
    // MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_SAMPLECNT(6),          0, BusMemOp::PM_COUNTER, 1, 0),
    // MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_SAMPLECNT(7),          0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMSYSROUTER_GLOBAL_PMMTRIGGERCNT, 0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_ELAPSED(0),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_ELAPSED(1),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_ELAPSED(2),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_ELAPSED(3),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_ELAPSED(4),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_ELAPSED(5),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_ELAPSED(6),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_ELAPSED(7),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CYCLECNT(0),          0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CYCLECNT(1),          0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CYCLECNT(2),          0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CYCLECNT(3),          0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CYCLECNT(4),          0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CYCLECNT(5),          0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CYCLECNT(6),          0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CYCLECNT(7),          0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CONTROL(0),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CONTROL(1),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CONTROL(2),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CONTROL(3),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CONTROL(4),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CONTROL(5),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CONTROL(6),           0, BusMemOp::PM_COUNTER, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PERF_PMMGPC_CONTROL(7),           0, BusMemOp::PM_COUNTER, 1, 0),

    // registers that only masked bits are interested; 1 in interested bits must meet
    MAKE_REGBUSMEMOPINF(LW_PMC_INTR_0,          ~(DRF_SHIFTMASK(LW_PMC_INTR_0_PTIMER) | DRF_SHIFTMASK(LW_PMC_INTR_0_PMU)),
                        BusMemOp::RD_POLL_ONE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPWR_FALCON_IRQSTAT, ~(DRF_SHIFTMASK(LW_PPWR_FALCON_IRQSTAT_GPTMR) |
                        DRF_SHIFTMASK(LW_PPWR_FALCON_IRQSTAT_WDTMR) /*| DRF_SHIFTMASK(LW_PPWR_FALCON_IRQSTAT_SWGEN0)*/),
                        BusMemOp::RD_POLL_ONE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PGRAPH_INTR, ~0U, BusMemOp::RD_POLL_ONE, 1, 0),

    // registers that only masked bits are interested; 0 in interested bits must meet
    MAKE_REGBUSMEMOPINF(LW_PGRAPH_STATUS,   DRF_SHIFTMASK(LW_PGRAPH_STATUS_STATE),  BusMemOp::RD_POLL_ZERO, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PGRAPH_ENGINE_STATUS, DRF_SHIFTMASK(LW_PGRAPH_ENGINE_STATUS_VALUE), BusMemOp::RD_POLL_ZERO, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_UFLUSH_FB_FLUSH, DRF_SHIFTMASK(LW_UFLUSH_FB_FLUSH_PENDING)|DRF_SHIFTMASK(LW_UFLUSH_FB_FLUSH_OUTSTANDING),
                        BusMemOp::RD_POLL_ZERO, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PFIFO_ENGINE_STATUS(0), DRF_SHIFTMASK(LW_PFIFO_ENGINE_STATUS_ENGINE), BusMemOp::RD_POLL_ZERO,
                        LW_PFIFO_ENGINE_STATUS__SIZE_1, LW_PFIFO_ENGINE_STATUS(1) - LW_PFIFO_ENGINE_STATUS(0)),
    MAKE_REGBUSMEMOPINF(LW_PFIFO_PREEMPT, DRF_SHIFTMASK(LW_PFIFO_PREEMPT_PENDING), BusMemOp::RD_POLL_ZERO, 1, 0),

    // Poll GE registers
    MAKE_REGBUSMEMOPINF(LW_PPWR_PMU_MSGQ_HEAD, ~0U, BusMemOp::RD_POLL_GE, 1, 0),
    MAKE_REGBUSMEMOPINF(LW_PPWR_PMU_QUEUE_TAIL(0), ~0U, BusMemOp::RD_POLL_GE,
                        LW_PPWR_PMU_QUEUE_TAIL__SIZE_1, LW_PPWR_PMU_QUEUE_TAIL(1) - LW_PPWR_PMU_QUEUE_TAIL(0)),
};

namespace
{
    Tasker::ThreadID LwrrentThreadId()
    {
        return Tasker::IsInitialized() ? Tasker::LwrrentThread() : 0;
    }

    string LwrrentThreadName()
    {
        return Tasker::IsInitialized() ? Tasker::ThreadName(LwrrentThreadId()) : "main";
    }
}

ChiplibOp::ChiplibOp(OpType type)
 : m_Type(type)
{
    if (Platform::IsInitialized())
        m_TimeNs = Platform::GetTimeNS();
    else
        m_TimeNs = ~0;
    Tasker::ThreadID threadId = LwrrentThreadId();
    string threadName = LwrrentThreadName();
    m_Thread = make_tuple(threadId, threadName);
    m_ChiplibTraceDumper = ChiplibTraceDumper::GetPtr();
    MASSERT(m_ChiplibTraceDumper);
}

const char *ChiplibOp::GetOpTypeName(OpType type)
{
    switch(type)
    {
    case BUSMEM_READ:     return "rd";
    case BUSMEM_WRITE:    return "wr";
    case BUSMEM_READMASK: return "rd_mask";
    case ANNOTATION:      return "@annot";
    case CHIPLIBOP_BLOCK: return "@tag";
    case ESCAPE_READ:     return "esc_rd";
    case ESCAPE_WRITE:    return "esc_wr";
    case CFG_WRITE:       return "wr";
    case CFG_READ:        return "rd";
    case DUMP_RAW_MEM:    return "dump_raw_mem";
    case LOAD_RAW_MEM:    return "load_raw_mem";
    case DUMP_PHYS_MEM:   return "dump_phys_mem";
    case LOAD_PHYS_MEM:   return "load_phys_mem";
    default:              break;
    }

    return "unknown_op";
}

BusMemOp *ChiplibOp::GetBusMemOp(ChiplibOp *pOp)
{
    return dynamic_cast<BusMemOp*>(pOp);
}

ChiplibOpBlock *ChiplibOp::GetChiplibOpBlock(ChiplibOp *pOp)
{
    return dynamic_cast<ChiplibOpBlock*>(pOp);
}

void ChiplibOp::SetOpType(OpType type)
{
    // USMEM_READ will be overridden by BUSMEM_READMASK
    // BUSMEM_READMASK might override BUSMEM_READMASK
    // if changing register is detected.
    switch(m_Type)
    {
    case BUSMEM_READ:
        if (type == BUSMEM_READMASK)
            break;
        else
            MASSERT(!"Please check");
    case BUSMEM_READMASK:
        if (type == BUSMEM_READMASK)
            break;
        else
            MASSERT(!"Please check");
    default:
        MASSERT(!"Please check");

    }
    m_Type = type;
}

UINT32 ChiplibOp::GetSysTime() const
{
    UINT64 timeFromStart = Tee::GetTimeFromStart();
    return UINT32(timeFromStart/1000000000);
}

string ChiplibOp::GetComments() const
{
    string comments;
    if (!Platform::IsTegra())
    {
        UINT64 startTimeNs = m_ChiplibTraceDumper->GetStartTimeNs();
        MASSERT(m_TimeNs >= startTimeNs);
        comments = Utility::StrPrintf("[clk %lld] [wallclk %u] [Thread #%u %s]",
                                      m_TimeNs - startTimeNs,
                                      GetSysTime(),
                                      get<THREAD_ID>(m_Thread), get<THREAD_NAME>(m_Thread).c_str());
    }

    return comments;
}

RC ChiplibOp::WriteTraceFile(const string & traceTrans, FileIO *pTraceFile) const
{
    size_t size = traceTrans.size();
    if (size && pTraceFile->FileWrite(traceTrans.c_str(), 1,
        UNSIGNED_CAST(UINT32, size)) != size)
    {
        Printf(Tee::PriHigh, "Error: Failed to write transactions to file\n");
        MASSERT(!"fatal error!");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//--------------------------------------------------------------------
BusMemOp::BusMemOp
(
    OpType       type,
    PHYSADDR     addr,
    const void*  data,
    UINT32       count
) :
    ChiplibOp(type),
    m_Address(addr),
    m_BusMemOpFlags(RD_MATCH),
    m_ReadMask(~0)
{
    if (type == BUSMEM_WRITE)
    {
        m_BusMemOpFlags = WRITE_OP;
    }

    m_Data.resize(count);
    memcpy(&m_Data[0], data, count);
}

string BusMemOp::DumpToFile(FileIO *pTraceFile) const
{
    // Syntax:
    // <rd/wr>            <aperture_type> <offset> <data ...> [#comment]
    // <rd_mask> <mask32> <aperture_type> <offset> <data ...> [#comment]
    string output;

    if (GetOpType() == ChiplibOp::BUSMEM_READ && m_Data.size() == 4)
    {
        if (m_ChiplibTraceDumper->IgnoreAddr(m_Address))
            return output;
    }

    if (m_ChiplibTraceDumper->RawDumpChiplibTrace())
    {
        const bool dumping = m_ChiplibTraceDumper->GetRawDumpChiplibTraceLevel() >
            ChiplibTraceDumper::RawChiplibTraceDumpLevel::Light;

        if (!dumping)
        {
            return output;
        }
    }

    // operation type
    output = Utility::StrPrintf("%s ", GetOpTypeName(GetOpType()));

    // mask for rd_mask operation
    if (GetOpType() == BUSMEM_READMASK)
    {
        output += GetRdMaskModeStr() + " ";

        if (m_BusMemOpFlags == RD_IGNORE   ||
            m_BusMemOpFlags == RD_OVERRIDE ||
            m_BusMemOpFlags == RD_CHANGING ||
            m_BusMemOpFlags == RD_IGNORE_FIRST_REGRD ||
            m_BusMemOpFlags == RD_FOLLOWED_WR ||
            m_BusMemOpFlags == PM_COUNTER)
        {
            // all bits are not interested
            output += Utility::StrPrintf("0x%08x ", 0);
        }
        else if (m_BusMemOpFlags == RD_POLL_ONE)
        {
            // bits with value 1b in mask are interested
            UINT32 mask = GetFirstUINT32();
            mask &= m_ReadMask;
            output += Utility::StrPrintf("0x%08x ", mask);
        }
        else if (m_BusMemOpFlags == RD_POLL_ZERO)
        {
            // bits with value 0b in mask are interested
            size_t i;
            const UINT08 *pMask = reinterpret_cast<const UINT08*>(&m_ReadMask);
            for (i = 0; i < m_Data.size(); ++ i)
            {
                if ((m_Data[i] & pMask[i%4]) != 0)
                {
                    break;
                }
            }

            if (i == m_Data.size())
            {
                // interested bits are zero => mush match
                output += Utility::StrPrintf("0x%08x ", m_ReadMask);
            }
            else
            {
                // no need to match
                output += Utility::StrPrintf("0x%08x ", 0);
            }
        }
        else if (m_BusMemOpFlags == RD_DROP)
        {
            return ""; // dropped operation; no output
        }
        else
        {
            // all bits in mask are interested
            output += Utility::StrPrintf("0x%08x ", m_ReadMask);
        }
    }

    // bar info
    PHYSADDR offset;
    ChiplibTraceDumper::AddrType type =
        m_ChiplibTraceDumper->GetAddrTypeOffset(m_Address, &offset);
    output += Utility::StrPrintf("%s ",
        m_ChiplibTraceDumper->GetAddrTypeName(type, m_Address).c_str());

    // BusMemOp address
    if (type == ChiplibTraceDumper::ADDR_UNKNOWN)
    {
        output += Utility::StrPrintf("0x%08llx ", m_Address);
    }
    else
    {
        output += Utility::StrPrintf("0x%08llx ", offset);
    }

    // byte data
    size_t sizeInBytes = m_Data.size();
    const size_t estimateCommentsLength = 100;

    // we just want to reserve enough capacity for output
    // the capacity could be a bit larger than needed
    if (!m_ChiplibTraceDumper->RawDumpChiplibTrace())
    {
        output.reserve(output.size() + sizeInBytes * (sizeof("0xXX ") - 1) + estimateCommentsLength);

        for (size_t i = 0; i < sizeInBytes; ++ i)
        {
            output += Utility::StrPrintf("0x%02x ", m_Data[i]);
        }
    }
    else
    {
        output.reserve(output.size() + (sizeInBytes / 4 + 1) * strlen("0xABCDABCD ") + estimateCommentsLength);
        size_t i = 0;
        for (; i + 4 <= sizeInBytes; i += 4)
        {
            output += Utility::StrPrintf("0x%02x%02x%02x%02x ", m_Data[i+3], m_Data[i+2], m_Data[i+1], m_Data[i]);
        }
        if (i < sizeInBytes)
        {
            output += Utility::StrPrintf("0x");
            while (i < sizeInBytes)
            {
                output += Utility::StrPrintf("%02x", m_Data[i++]);
            }
            output += Utility::StrPrintf(" ");
        }
    }

    // comments
    output += Utility::StrPrintf(" # %s\n", GetComments().c_str());

    WriteTraceFile(output, pTraceFile);

    return output;
}

string BusMemOp::GetComments() const
{
    string output = ChiplibOp::GetComments();
    output += Utility::StrPrintf("Addr=0x%llx ", m_Address);

    switch(m_BusMemOpFlags)
    {
    case RD_IGNORE:
        output += "ignore returned data";
        break;
    case RD_MATCH:
        output += "rd data must match captured data";
        break;
    case RD_OVERRIDE:
        output += "rd operation is overriden by later rd on same address";
        break;
    case RD_CHANGING:
        output += "rd data is constantly changing";
        break;
    case RD_POLL_ONE:
        output += "poll wait the interested bits to be 1";
        break;
    case RD_POLL_ZERO:
        output += "poll wait the interested bits to be 0";
        break;
    case RD_POLL_GE:
        output += "poll wait until replay_bits >= caputre_bits";
        break;
    case RD_NONPOLL_EQ:
        output += "read once and try equal match";
        break;
    case RD_DROP:
        output += "dropped rd which is not in chiplib trace";
        break;
    case RD_IGNORE_FIRST_REGRD:
        output += "ignore first read to the register in global range";
        break;
    case RD_FOLLOWED_WR:
        output += "ignore the read because wr follows";
        break;
    case WRITE_OP:
        output += "write operation";
        break;
    case PM_COUNTER:
        output += "pm_read";
        break;
    default: MASSERT(!"Unknow BusMemOp flags!");
    };

    return output;
}

// Notes: GetxxxAddrOffset() may not work because GetGpuSubdevInfo() is not called
//
RC BusMemOp::PreProcessBeforeCapture(ChiplibOpBlock *pOpBlock)
{
    RC rc;

    // LW_PPWR_FALCON_IRQSTAT:
    // Ignore already masked interrupts and any interrupts not destined for
    // host/RM.
    //
    //  mask = GPU_REG_RD32(pGpu, LW_PPWR_FALCON_IRQMASK) &
    //      GPU_REG_RD32(pGpu, LW_PPWR_FALCON_IRQDEST);
    //  intrStatus = GPU_REG_RD32(pGpu, LW_PPWR_FALCON_IRQSTAT) & mask;
    //
    PHYSADDR offset;
    if(m_ChiplibTraceDumper->GetBar0AddrOffset(m_Address, &offset) &&
       (offset == LW_PPWR_FALCON_IRQSTAT))
    {
        m_ChiplibTraceDumper->DisableChiplibTraceDump();

        UINT32 irqMask, irqDest;
        PHYSADDR bar0Base = m_Address & ~0xFFFFFF;
        CHECK_RC(Platform::PhysRd(bar0Base + LW_PPWR_FALCON_IRQMASK,
                                  &irqMask, sizeof(irqMask)));
        CHECK_RC(Platform::PhysRd(bar0Base + LW_PPWR_FALCON_IRQDEST,
                                  &irqDest, sizeof(irqDest)));
        m_ReadMask = irqMask & irqDest;

        m_ChiplibTraceDumper->EnableChiplibTraceDump();
    }

    return rc;
}

//
// Notes GetBar0AddrOffset() should work
RC BusMemOp::PostProcessBeforeDump()
{
    RC rc;

    // specially handle those registers in s_SpecialBusMemOpRegs table
    //
    if (GetOpType() == BUSMEM_READ)
    {
        PHYSADDR offset;
        if(m_ChiplibTraceDumper->GetBar0AddrOffset(m_Address, &offset))
        {
            const ChiplibTraceDumper::RegOpInfo
                *pRegOpInfo = m_ChiplibTraceDumper->GetSpecialRegOpInfo(offset);

            if (pRegOpInfo)
            {
                SetOpType(BUSMEM_READMASK);
                m_ReadMask &= pRegOpInfo->InterestedMask;
                m_BusMemOpFlags = static_cast<BusMemOpFlags>(pRegOpInfo->Flags);
            }
            else if ((GetFirstUINT32() & GPU_READ_PRI_ERROR_MASK)
                      == GPU_READ_PRI_ERROR_CODE)
            {
                Printf(Tee::PriNormal, "Bad register read is detected. Returned value "
                    "will be ignored. addr = 0x%llx data = 0x%x\n",
                    m_Address, GetFirstUINT32());

                SetOpType(BUSMEM_READMASK);
                m_ReadMask = 0;
                m_BusMemOpFlags = RD_IGNORE;
            }
        }
        else
        {
            m_ReadMask = ~0; // mask for non-bar0 read
        }
    }

    return rc;
}

//! \brief Retrun mode for rd_mask operation
//
string BusMemOp::GetRdMaskModeStr() const
{
    string modeStr;

    if (GetOpType() != BUSMEM_READMASK)
    {
        return modeStr; // only rd_mask has mode key
    }

    // rd_mask <mode P_LE|P_EQ|P_GE|P_G|P_L| NP_LE|NP_EQ|NP_GE|NP_G|NP_L >
    //
    modeStr = "mode";
    switch (m_BusMemOpFlags)
    {
    case RD_MATCH:
    case RD_POLL_ONE:
    case RD_POLL_ZERO:
        modeStr += " P_EQ"; // poll match
        break;
    case RD_IGNORE:
    case RD_OVERRIDE:
    case RD_CHANGING:
    case RD_IGNORE_FIRST_REGRD:
    case RD_FOLLOWED_WR:
    case PM_COUNTER:
        modeStr += " NP_EQ"; // non-poll ignore; mask = 0
        break;
    case RD_POLL_GE:
        modeStr += " P_GE"; // poll until Great_Equal
        break;
    case RD_NONPOLL_EQ:
        modeStr += " NP_EQ"; // non-poll read once; mask != 0
        break;
    case RD_DROP:
        modeStr = "";
        break;
    case WRITE_OP:
        MASSERT(!"It's invalid that RD operation has write flag");
        break;
    default:
        MASSERT(!"Unkown flag");
        break;
    }

    return modeStr;
}

//! \brief Private Function
//!        Get the 1st UINT32 data
UINT32 BusMemOp::GetFirstUINT32() const
{
    UINT32 data = 0;
    size_t size = m_Data.size() > 4? 4 : m_Data.size();
    for (size_t i = 0; i < size; ++ i)
    {
        data |= (m_Data[i] << i*8);
    }

    return data;
}

//--------------------------------------------------------------------
CfgPioOp::CfgPioOp
(
    OpType       type,
    UINT32       cfgBase,
    UINT32       addr,
    const void*  data,
    UINT32       count
) :
    ChiplibOp(type),
    m_CfgBase(cfgBase),
    m_Address(addr)
{
    // need more consideration about how to composite
    // the address if it's more than 0xFF
    MASSERT((m_Address & 0xFFFFFF00) == 0);

    m_Data.resize(count);
    memcpy(&m_Data[0], data, count);
}

string CfgPioOp::DumpToFile(FileIO *pTraceFile) const
{
    //
    // Syntax:
    // <rd/wr> <cfgspace> <offset> <data ...> [#comment]
    string output;

    // only output those cfg rd/wr in recorded cfgspace
    UINT32 cfgIdx;
    RC rc = m_ChiplibTraceDumper->GetPciCfgSpaceIdx(m_CfgBase, &cfgIdx);
    if (rc != OK)
    {
        return output;
    }

    if (m_ChiplibTraceDumper->RawDumpChiplibTrace())
    {
        const bool dumping = m_ChiplibTraceDumper->GetRawDumpChiplibTraceLevel() >
            ChiplibTraceDumper::RawChiplibTraceDumpLevel::Light;

        if (!dumping)
        {
            return output;
        }
    }

    // op type
    output = Utility::StrPrintf("%s ", GetOpTypeName(GetOpType()));

    // cfgspace
    output += Utility::StrPrintf("cfg%d ", cfgIdx);

    // CfgPioOp address
    output += Utility::StrPrintf(" 0x%08x", m_Address);

    // byte aligned data
    for (size_t i = 0; i < m_Data.size(); ++ i)
    {
        output += Utility::StrPrintf(" 0x%02x", m_Data[i]);
    }

    // comments
    output += Utility::StrPrintf(" # %s\n", GetComments().c_str());

    WriteTraceFile(output, pTraceFile);

    return output;
}

string CfgPioOp::GetComments() const
{
    return ChiplibOp::GetComments() +
        Utility::StrPrintf("cfg base addr 0x%x", m_CfgBase);
}

//--------------------------------------------------------------------
AnnotationOp::AnnotationOp(const string &annotation):
    ChiplibOp(ANNOTATION),
    m_Annotation(annotation)
{
}

void AnnotationOp::AppendAnnotaion(const string& str)
{
    m_Annotation += str;
}

string AnnotationOp::DumpToFile(FileIO *pTraceFile) const
{
    //
    // Syntax:
    // <@annot> <messages from mods print>
    string output;

    output = GetOpTypeName(GetOpType());
    output += Utility::StrPrintf(" %s # %s\n", Utility::StringReplace(m_Annotation, "\n", "").c_str(),
                                 GetComments().c_str());

    WriteTraceFile(output, pTraceFile);

    return output;
}

string AnnotationOp::GetComments() const
{
    return ChiplibOp::GetComments();
}

//--------------------------------------------------------------------
EscapeOp::EscapeOp
(
    OpType type,
    UINT08 gpuId,
    const char *path,
    UINT32 index,
    UINT32 size,
    const void *buf
):
    ChiplibOp(type),
    m_GpuId(gpuId),
    m_EscapeKey(path),
    m_Index(index),
    m_Size(size)
{
    m_Values.resize(m_Size);
    memcpy(&m_Values[0], buf, m_Size);
}

string EscapeOp::DumpToFile(FileIO *pTraceFile) const
{
    //
    // Syntax:
    // <esc_rd/esc_wr> <gpuId> <key> <index> <data ...> [#comment]
    string output;

    if (m_ChiplibTraceDumper->RawDumpChiplibTrace())
    {
        const bool dumping = m_ChiplibTraceDumper->GetRawDumpChiplibTraceLevel() >
            ChiplibTraceDumper::RawChiplibTraceDumpLevel::Light;

        if (!dumping)
        {
            return output;
        }
    }

    // <esc_rd/esc_wr> <gpuId> <key> <index>
    output = Utility::StrPrintf("%s 0x%02x \"%s\" 0x%x 0x%x",
                                GetOpTypeName(GetOpType()),
                                m_GpuId, m_EscapeKey.c_str(),
                                m_Index, m_Size);

    // <data ...>
    for (size_t i = 0; i < m_Values.size(); ++i)
    {
        output += Utility::StrPrintf(" 0x%02x", m_Values[i]);
    }

    // [#comment]
    output += Utility::StrPrintf(" # %s\n", GetComments().c_str());

    WriteTraceFile(output, pTraceFile);

    return output;
}

string EscapeOp::GetComments() const
{
    string dataStr = "...";
    if (m_Values.size() == 4)
    {
        UINT32 data = m_Values[0] | (m_Values[1] << 8) |
               (m_Values[2] << 16) | (m_Values[3] << 24);
        dataStr = Utility::StrPrintf("0x%08x", data);
    }

    dataStr = Utility::StrPrintf("%s Ecscape%s(/*Key*/\"%s\", /*Index*/0x%x, "
            "/*Size*/0x%x, Data=%s)", ChiplibOp::GetComments().c_str(),
            GetOpType() == ESCAPE_READ?"Read" : "Wrtie",
            m_EscapeKey.c_str(), m_Index, m_Size,
            dataStr.c_str());

    return dataStr;
}

//--------------------------------------------------------------------
ChiplibOpBlock::ChiplibOpBlock
(
    OpType type,
    ChiplibOpScope::ScopeType scopeType,
    const string &blockDes,
    UINT32 stackDepth,
    bool optional
) :
    ChiplibOp(type),
    m_BlockDescription(blockDes),
    m_StackDepth(stackDepth),
    m_ChiplibOpScopeType(scopeType),
    m_optional(optional),
    m_pLastAnnotOp(nullptr),
    m_bNewAnnotationLineStart(true)
{
    m_Name = Utility::StringReplace(m_BlockDescription,"::", "_");
    m_Name = Utility::StringReplace(m_Name,":", "_");
}

ChiplibOpBlock::~ChiplibOpBlock()
{
    for (vector<ChiplibOp*>::iterator it = m_Operations.begin();
         it != m_Operations.end(); ++ it)
    {
        delete (*it);
    }
    m_Operations.clear();
    if (m_PraminDumpFile)
    {
        fclose(m_PraminDumpFile);
        m_PraminDumpFile = nullptr;
    }
}

string ChiplibOpBlock::DumpHeadToFile(FileIO *pTraceFile) const
{
    //
    // Syntax:
    // @tag_begin/tag_end <block_description> [# comment]

    // @tag_begin <block_description> [# comment]
    string output;
    output = Utility::StrPrintf("%s_begin %s %s # call depth %d # %s\n",
        GetOpTypeName(GetOpType()),
        m_optional?"optional" : "",
        GetName().c_str(),
        m_StackDepth,
        GetComments().c_str());
    WriteTraceFile(output, pTraceFile);
    return output;
}

string ChiplibOpBlock::DumpOpsToFile(FileIO *pTraceFile) const
{
    WriteTraceFile("\n", pTraceFile);
    for (size_t opIdx = 0; opIdx < m_Operations.size(); ++ opIdx)
    {
        ChiplibOp *pChiplibOp = m_Operations[opIdx];
        if (!pChiplibOp->DumpToFile(pTraceFile).empty())
        {
            WriteTraceFile("\n", pTraceFile);
        }
    }
    return "";
}

string ChiplibOpBlock::DumpTailToFile(FileIO *pTraceFile) const
{
    // @tag_end
    if (Platform::IsInitialized())
    {
        m_TimeNs = Platform::GetTimeNS();
    }

    string output;
    output = Utility::StrPrintf("%s_end %s %s # call depth %d # %s\n",
        GetOpTypeName(GetOpType()),
        m_optional?"optional" : "",
        GetName().c_str(),
        m_StackDepth,
        GetComments().c_str());

    WriteTraceFile(output, pTraceFile);
    return output;
}

string ChiplibOpBlock::DumpToFile(FileIO *pTraceFile) const
{
    string output;
    output += DumpHeadToFile(pTraceFile);
    output += DumpOpsToFile(pTraceFile);
    output += DumpTailToFile(pTraceFile);

    return output;
}

string ChiplibOpBlock::GetComments() const
{
    return ChiplibOp::GetComments() + GetBlockDescription();
}

RC ChiplibOpBlock::PostProcessBeforeDump()
{
    StickyRC stickyRc;

    //
    // Step1: merge read in read block
    bool rdBlockBegin = false;
    bool processBlock = false;
    vector<ChiplibOp*>::reverse_iterator ritBegin;
    vector<ChiplibOp*>::reverse_iterator ritEnd;

    vector<ChiplibOp*>::reverse_iterator ritOp;
    for (ritOp = m_Operations.rbegin(); ritOp != m_Operations.rend(); ++ritOp)
    {
        // identify a read block
        if ((*ritOp)->GetOpType() == BUSMEM_WRITE    ||
            (*ritOp)->GetOpType() == CHIPLIBOP_BLOCK ||
            (ritOp+1) == m_Operations.rend())
        {
            if (rdBlockBegin)
            {
                // block end
                rdBlockBegin = false;
                processBlock = true;
                ritEnd = ritOp + 1; // include this one
            }
        }
        else if ((*ritOp)->GetOpType() == BUSMEM_READ ||
                 (*ritOp)->GetOpType() == BUSMEM_READMASK)
        {
            if (!rdBlockBegin)
            {
                // block begin
                rdBlockBegin = true;
                ritBegin = ritOp;
            }
        }

        // process a read block
        if (processBlock)
        {
            // composite address and size to a UINT64 key
            // size: bit63 - bit44  addr: bit43 - bit0
            //
            const UINT32 sizeBits = 20;
            const UINT64 sizeMask = (1 << sizeBits) - 1;
            const UINT32 shiftedBits = (sizeof(UINT64)*8 - sizeBits);
            const UINT64 shiftedSizemask = sizeMask << shiftedBits;

            UINT32 rpos = 0;
            map<UINT64, vector<UINT32> > addrMap;
            vector<ChiplibOp*>::reverse_iterator rit;
            for (rit = ritBegin; rit != ritEnd; ++rit, ++rpos)
            {
                if ((*rit)->GetOpType() == BUSMEM_READ ||
                    (*rit)->GetOpType() == BUSMEM_READMASK)
                {
                    BusMemOp* pBusMemOp = GetBusMemOp(*rit);
                    UINT64 addr = pBusMemOp->GetStartAddr();
                    UINT64 size = pBusMemOp->GetData()->size();
                    if (((addr & shiftedSizemask) == 0) &&
                        ((size & ~sizeMask) == 0))
                    {
                        UINT64 key = addr | (size<<shiftedBits);
                        addrMap[key].push_back(rpos);
                    }
                    else
                    {
                        Printf(Tee::PriNormal, "Warning: %s: address(0x%llx) or size(0x%llx)"
                            " is bigger than it's expected. post process is skipped!\n",
                            __FUNCTION__, addr, size);
                    }
                }
            }

            // mask stale read; only keep the latest one
            //
            BusMemOp::BusMemOpFlags flag = BusMemOp::RD_OVERRIDE;
            if (m_ChiplibOpScopeType == ChiplibOpScope::SCOPE_POLL)
            {
                flag = BusMemOp::RD_DROP;
            }

            for (map<UINT64, vector<UINT32> >::iterator
                 addrMapIt = addrMap.begin();
                 addrMapIt != addrMap.end();
                 ++ addrMapIt)
            {
                for (vector<UINT32>::const_iterator
                     posIt = addrMapIt->second.begin() + 1;
                     posIt != addrMapIt->second.end();
                     ++ posIt)
                {
                    BusMemOp* pBusMemOp = GetBusMemOp(*(ritBegin + *posIt));
                    MASSERT(pBusMemOp);

                    pBusMemOp->SetBusMemOpFlags(flag);
                    pBusMemOp->SetOpType(BUSMEM_READMASK);
                }
            }

            processBlock = false;
        }
    }

    //
    // Step2: PostProcess for individual operations
    for (size_t opIdx = 0; opIdx < m_Operations.size(); ++ opIdx)
    {
        ChiplibOp *pChiplibOp = m_Operations[opIdx];
        stickyRc = pChiplibOp->PostProcessBeforeDump();

        if ((pChiplibOp->GetOpType() == BUSMEM_READ) ||
            (pChiplibOp->GetOpType() == BUSMEM_WRITE)||
            (pChiplibOp->GetOpType() == BUSMEM_READMASK))
        {
            PHYSADDR offset;
            BusMemOp* pLwrrentOp = GetBusMemOp(pChiplibOp);
            PHYSADDR address = pLwrrentOp->GetStartAddr();
            ChiplibTraceDumper::AddrType type =
                m_ChiplibTraceDumper->GetAddrTypeOffset(address, &offset);

            // 1. Special handle for a bar0 read which has a write
            // on same address followed: read-write sequence
            //
            set<PHYSADDR> *pBar0AddrSet =
                m_ChiplibTraceDumper->GetBar0AccessAddrSet();

            bool wrFollowed = ((opIdx+1) < m_Operations.size()) &&
                    (m_Operations[opIdx+1]->GetOpType() == BUSMEM_WRITE);

            if (wrFollowed &&
                (type == ChiplibTraceDumper::ADDR_BAR0) &&
                (pChiplibOp->GetOpType() == BUSMEM_READ))
            {
                BusMemOp* pWriteOp = GetBusMemOp(m_Operations[opIdx+1]);
                if ((pLwrrentOp->GetStartAddr() == pWriteOp->GetStartAddr()) &&
                    (pLwrrentOp->GetData()->size() == pWriteOp->GetData()->size()))
                {
                    bool isPramInReg = (offset >= LW_PRAMIN_DATA008(0)) &&
                        (offset < LW_PRAMIN_DATA008(LW_PRAMIN_DATA008__SIZE_1));

                    if (isPramInReg || offset == LW_PBUS_BAR0_WINDOW)
                    {
                        // PRAMIN related read-write sequence
                        pLwrrentOp->SetOpType(BUSMEM_READMASK);
                        pLwrrentOp->SetBusMemOpFlags(BusMemOp::RD_FOLLOWED_WR);
                    }
                    else if (pBar0AddrSet->count(address) == 0)
                    {
                        // First access in global range is rd
                        pLwrrentOp->SetOpType(BUSMEM_READMASK);
                        pLwrrentOp->SetBusMemOpFlags(BusMemOp::RD_IGNORE_FIRST_REGRD);
                    }
                }
            }

            if (type == ChiplibTraceDumper::ADDR_BAR0)
            {
                pBar0AddrSet->insert(address);
            }

            // 2. tag bar1 read in ChiplibOpScope::SCOPE_CRC_SURF_READ call
            //
            if ((m_ChiplibOpScopeType == ChiplibOpScope::SCOPE_CRC_SURF_READ) &&
                (pChiplibOp->GetOpType() == BUSMEM_READ) &&
                (type == ChiplibTraceDumper::ADDR_BAR1))
            {
                pLwrrentOp->SetOpType(BUSMEM_READMASK);
                pLwrrentOp->SetBusMemOpFlags(BusMemOp::RD_NONPOLL_EQ);
            }

            // 3. tag bar1 read in WriteGpEntryData_pollson_&PollGpFifoFull
            //    Poll function return true (exit polling loop) when all subdevices
            //    have read past the gpfifo entry we are about to overwrite.
            if ((pChiplibOp->GetOpType() == BUSMEM_READ) &&
                (m_ChiplibOpScopeType == ChiplibOpScope::SCOPE_POLL) &&
                (type == ChiplibTraceDumper::ADDR_BAR1))
            {
                if ((m_BlockDescription == "&PollGpFifoFull") &&
                    (pLwrrentOp->GetData()->size() == 4)) //pbGet read
                {
                    pLwrrentOp->SetOpType(BUSMEM_READMASK);
                    pLwrrentOp->SetBusMemOpFlags(BusMemOp::RD_POLL_GE);
                }
            }
        }
    }

    return stickyRc;
}

//--------------------------------------------------------------------
RawMemOp::RawMemOp
(
    OpType type,
    const string &memAperture,
    UINT64 physAddress,
    UINT32 size,
    const string &fileName
)
    : ChiplibOp(type),
      m_MemAperture(memAperture),
      m_PhysAddress(physAddress),
      m_Size(size),
      m_FileName(fileName)
{
}

string RawMemOp::DumpToFile(FileIO *pTraceFile) const
{
    //
    // Syntax:
    // @dump_raw_mem <memory aperture> <physical address> <file name>
    string output;

    if (m_ChiplibTraceDumper->RawDumpChiplibTrace())
    {
        const bool dumping = m_ChiplibTraceDumper->GetRawDumpChiplibTraceLevel() >
            ChiplibTraceDumper::RawChiplibTraceDumpLevel::Light;

        if (!dumping)
        {
            return output;
        }
    }

    output = GetOpTypeName(GetOpType());
    output += Utility::StrPrintf(" %s 0x%llx 0x%x %s # %s\n",
        m_MemAperture.c_str(), m_PhysAddress,
        m_Size, m_FileName.c_str(), GetComments().c_str());

    WriteTraceFile(output, pTraceFile);

    return output;
}

string RawMemOp::GetComments() const
{
    return ChiplibOp::GetComments();
}

UINT32 ChiplibOpBlock::m_PraminNextIndex = 0;
FILE* ChiplibOpBlock::m_PraminDumpFile = nullptr;
std::vector<BusMemOp*> ChiplibOpBlock::m_CachedOps;
UINT32 ChiplibOpBlock::m_CachedDataSize = 0;
PHYSADDR ChiplibOpBlock::m_LastPhysAddr = 0;
BusMemOp *ChiplibOpBlock::AddBusMemOp
(
    const char *typeStr,
    PHYSADDR    addr,
    const void *data,
    UINT32      count
)
{
    if (!m_ChiplibTraceDumper->IsChiplibTraceDumpEnabled())
    {
        return nullptr;
    }

    if (m_ChiplibTraceDumper->IsGpuInfoInitialized())
    {
        PHYSADDR offset;
        if ((!m_ChiplibTraceDumper->IsBar1DumpEnabled() && 
            m_ChiplibTraceDumper->GetBar1AddrOffset(addr, &offset)) ||
            (!m_ChiplibTraceDumper->IsSysmemDumpEnabled() &&
            IsSysmemAccess(addr)))
        {
            return nullptr;
        }
    }

    ChiplibOp::OpType type = ChiplibOp::BUSMEM_WRITE;
    if (!strcmp(typeStr, "prd") ||
        !strcmp(typeStr, "vrd"))
    {
        type = ChiplibOp::BUSMEM_READ;
    }

    if (type == ChiplibOp::BUSMEM_READ && count == 4)
    {
        if (m_ChiplibTraceDumper->IgnoreAddr(addr))
        {
            return nullptr;
        }
    }

    // add the new opertion
    //
    unique_ptr<BusMemOp> pBusMemOp(new BusMemOp(type, addr, data, count));
    if (pBusMemOp->PreProcessBeforeCapture(this) != OK)
    {
        return nullptr;
    }
    if (m_ChiplibTraceDumper->IsPraminEnabled() )
    {
        // backdoor_pramin
        GpuSubdevice* pSubdev = nullptr;
        GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        if (pGpuDevMgr)
            pSubdev = pGpuDevMgr->GetFirstGpu();
        if(pSubdev && type == ChiplibOp::BUSMEM_WRITE)
        {
            UINT64 bar0Base = pSubdev->GetPhysLwBase();
            if (addr >= bar0Base + DEVICE_BASE(LW_PRAMIN) &&
                addr <= bar0Base + DEVICE_EXTENT(LW_PRAMIN))
            {
                if(DumpPramin(pSubdev, pBusMemOp.get(), addr, data, count))
                    return nullptr;
            }
        }
    }
    if (IsSysmemAccess(addr))
    {
        m_ChiplibTraceDumper->SaveSysmemSegment(
            GetSysmemBaseForPAddr(addr), GetSysmemSizeForPAddr(addr));
    }

    m_Operations.push_back(pBusMemOp.get());
    m_ChiplibTraceDumper->RawDumpToFile(pBusMemOp.get());
    return pBusMemOp.release();;

}

bool ChiplibOpBlock::DumpPramin
(
    GpuSubdevice* pSubdev,
    BusMemOp*   pBusMemOp,
    PHYSADDR    addr,
    const void *data,
    UINT32      count
)
{
    if (!pSubdev || !pBusMemOp)
        return false;

    const UINT32 MIN_PRAMIN_DUMP_SIZE = 8;
    UINT64 bar0Base = pSubdev->GetPhysLwBase();
    RawMemOp* lastOp = nullptr;
    if (m_Operations.size())
       lastOp = dynamic_cast<RawMemOp*>(m_Operations.back());

    m_ChiplibTraceDumper->DisableChiplibTraceDump();
    PHYSADDR physAddr = (addr - bar0Base - DEVICE_BASE(LW_PRAMIN))
        + (pSubdev->RegRd32(LW_PBUS_BAR0_WINDOW) << 16);
    m_ChiplibTraceDumper->EnableChiplibTraceDump();

    // There're some other ops between current op and cached ops.
    if (m_LastPhysAddr + m_CachedDataSize != physAddr ||
            m_Operations.back() != m_CachedOps.back())
    {
        m_LastPhysAddr = 0;
        m_CachedOps.clear();
        m_CachedDataSize = 0;
    }
    if (m_CachedDataSize+ count <= MIN_PRAMIN_DUMP_SIZE &&
                (!lastOp || (lastOp && lastOp->GetPhysAddress() + lastOp->GetSize() != physAddr)))
    {
       if (m_CachedOps.size() == 0 )
           m_LastPhysAddr = physAddr;
        m_CachedOps.push_back(pBusMemOp);
        m_CachedDataSize += count;
        return false;
    }
    else
    {
        if (lastOp && lastOp->GetPhysAddress() + lastOp->GetSize() == physAddr)
        {
            const string& filename = lastOp->GetFileName();
            if(!m_PraminDumpFile)
            {
                Utility::OpenFile(filename.c_str(), &m_PraminDumpFile, "a");
            }
            const UINT08 *buf = (const UINT08*)data;
            for (UINT32 i = 0; i < count; ++i)
            {
                fprintf(m_PraminDumpFile, "%02x\n", buf[i] );
            }
            m_Operations.pop_back();
            AddRawMemLoadOp("FB", lastOp->GetPhysAddress(), lastOp->GetSize() + count, filename);
            delete lastOp;
            return true;
        }
        else
        {
            const string filename = Utility::StrPrintf("pramin_%03d.raw", m_PraminNextIndex);
            if (m_PraminDumpFile)
            {
                fclose(m_PraminDumpFile);
                m_PraminDumpFile = nullptr;
            }
            Utility::OpenFile(filename.c_str(), &m_PraminDumpFile, "w");
            for (size_t i = 0; i < m_CachedOps.size(); ++i)
            {
                BusMemOp* op = m_CachedOps[i];
                std::vector<UINT08>* data = op->GetData();
                for(size_t j = 0; j < data->size(); ++j)
                {
                    fprintf(m_PraminDumpFile, "%02x\n", (*data)[j] );
                }
                m_Operations.pop_back();
                delete op;
            }
            const UINT08 *buf = (const UINT08*)data;
            for (UINT32 i = 0; i < count; ++i)
            {
                fprintf(m_PraminDumpFile, "%02x\n", buf[i] );
            }
            AddRawMemLoadOp("FB", m_LastPhysAddr, m_CachedDataSize+ count, filename);
            m_LastPhysAddr = 0;
            m_CachedOps.clear();
            m_CachedDataSize = 0;
            m_PraminNextIndex++;
            return true;
        }
    }
}

CfgPioOp *ChiplibOpBlock::AddCfgPioOp
(
    const char *typeStr,
    INT32       domainNumber,
    INT32       busNumber,
    INT32       deviceNumber,
    INT32       functionNumber,
    INT32       address,
    const void *data,
    UINT32      count
)
{
    if (!m_ChiplibTraceDumper->IsChiplibTraceDumpEnabled())
    {
        return nullptr;
    }

    ChiplibOp::OpType type = ChiplibOp::CFG_WRITE;
    if (!strcmp(typeStr, "cfgrd"))
    {
        type = ChiplibOp::CFG_READ;
    }

    // Only save the cfgspace which has cfgwr;
    // The cfgspace only has cfgrd will not be dumped
    UINT32 cfgBaseAddr = Pci::GetConfigAddress(domainNumber, busNumber, deviceNumber, functionNumber, 0);
    if (type == ChiplibOp::CFG_WRITE)
    {
        if (OK != m_ChiplibTraceDumper->SetPciCfgSpace(
                        domainNumber, busNumber, deviceNumber, functionNumber, cfgBaseAddr))
        {
            MASSERT(!"SetPciCfgSpace() failed! Please check!");
        }
    }

    CfgPioOp* pCfgPioOp = new CfgPioOp(type, cfgBaseAddr, address, data, count);
    m_Operations.push_back(pCfgPioOp);
    m_ChiplibTraceDumper->RawDumpToFile(pCfgPioOp);
    return pCfgPioOp;
}

AnnotationOp *ChiplibOpBlock::AddAnnotOp(const char* pLinePrefix, const char* pStr)
{
    if (!m_ChiplibTraceDumper->IsChiplibTraceDumpEnabled())
    {
        return nullptr;
    }

    if (m_bNewAnnotationLineStart)
    {
        AnnotationOp *pAnnotation = new AnnotationOp(pLinePrefix);
        m_pLastAnnotOp = pAnnotation;
        m_Operations.push_back(pAnnotation);
    }

    m_pLastAnnotOp->AppendAnnotaion(pStr);

    const size_t len = strlen(pStr);
    m_bNewAnnotationLineStart = (len > 0) && (pStr[len - 1] == '\n');

    if (m_bNewAnnotationLineStart)
    {
        m_ChiplibTraceDumper->RawDumpToFile(m_pLastAnnotOp);
    }

    return m_pLastAnnotOp;
}

ChiplibOpBlock *ChiplibOpBlock::AddBlockOp
(
    OpType type,
    ChiplibOpScope::ScopeType scopeType,
    const string &blockDes,
    UINT32 stackDepth,
    bool optional
)
{
    if (!m_ChiplibTraceDumper->IsChiplibTraceDumpEnabled())
    {
        return nullptr;
    }

    ChiplibOpBlock* pOpBlock =
        new ChiplibOpBlock(type, scopeType, blockDes,
                           stackDepth, optional);

    m_Operations.push_back(pOpBlock);
    return pOpBlock;
}

EscapeOp *ChiplibOpBlock::AddEscapeOp
(
    const char *typeStr,
    UINT08 gpuId,
    const char *path,
    UINT32 index,
    UINT32 size,
    const void *buf
)
{
    if (!m_ChiplibTraceDumper->IsChiplibTraceDumpEnabled())
    {
        return nullptr;
    }

    ChiplibOp::OpType type = ChiplibOp::ESCAPE_WRITE;
    if (!strcmp(typeStr, "escrd"))
    {
        type = ChiplibOp::ESCAPE_READ;
    }

    EscapeOp *pEscapeOp = new EscapeOp(type, gpuId, path, index, size, buf);
    m_Operations.push_back(pEscapeOp);
    m_ChiplibTraceDumper->RawDumpToFile(pEscapeOp);
    return pEscapeOp;
}

RawMemOp *ChiplibOpBlock::AddRawMemDumpOp
(
    const string &memAperture,
    UINT64 physAddress,
    UINT32 size,
    const string &fileName
)
{
    if (!m_ChiplibTraceDumper->IsChiplibTraceDumpEnabled())
    {
        return nullptr;
    }

    RawMemOp *op = new RawMemOp(ChiplibOp::DUMP_RAW_MEM, memAperture, physAddress, size, fileName);
    m_Operations.push_back(op);
    m_ChiplibTraceDumper->RawDumpToFile(op);

    return op;
}

RawMemOp *ChiplibOpBlock::AddRawMemLoadOp
(
    const string &memAperture,
    UINT64 physAddress,
    UINT32 size,
    const string &fileName
)
{
    if (!m_ChiplibTraceDumper->IsChiplibTraceDumpEnabled())
    {
        return nullptr;
    }

    RawMemOp *op = new RawMemOp(ChiplibOp::LOAD_RAW_MEM, memAperture, physAddress, size, fileName);
    m_Operations.push_back(op);
    m_ChiplibTraceDumper->RawDumpToFile(op);

    return op;
}

RawMemOp *ChiplibOpBlock::AddPhysMemDumpOp
(
    const string &memAperture,
    UINT64 physAddress,
    UINT32 size,
    const string &fileName
)
{
    if (!m_ChiplibTraceDumper->IsChiplibTraceDumpEnabled())
    {
        return nullptr;
    }

    RawMemOp *op = new RawMemOp(ChiplibOp::DUMP_PHYS_MEM, memAperture, physAddress, size, fileName);
    m_Operations.push_back(op);
    m_ChiplibTraceDumper->RawDumpToFile(op);

    return op;
}

RawMemOp *ChiplibOpBlock::AddPhysMemLoadOp
(
    const string &memAperture,
    UINT64 physAddress,
    UINT32 size,
    const string &fileName
)
{
    if (!m_ChiplibTraceDumper->IsChiplibTraceDumpEnabled())
    {
        return nullptr;
    }

    RawMemOp *op = new RawMemOp(ChiplibOp::LOAD_PHYS_MEM, memAperture, physAddress, size, fileName);
    m_Operations.push_back(op);
    m_ChiplibTraceDumper->RawDumpToFile(op);

    return op;
}

RC ChiplibOpBlock::DropLastOp()
{
    RC rc;

    if (!m_Operations.empty())
    {
        delete m_Operations.back();
        m_Operations.pop_back();
    }

    return rc;
}

//--------------------------------------------------------------------
ChiplibTraceDumper ChiplibTraceDumper::s_ChiplibTraceDumper;

//--------------------------------------------------------------------
bool ChiplibTraceDumper::IsChiplibTraceDumpEnabled()
{
    if (m_DumpChiplibTrace)
    {
        Tasker::ThreadID id = LwrrentThreadId();
        if (id !=  m_ThreadDumpStateCache.LatestThreadId)
        {
            // cache miss:
            // search disabled thread set and update cache
            m_ThreadDumpStateCache.LatestThreadId = id;
            m_ThreadDumpStateCache.DumpEnabled =
                m_DisabledThreads.find(id) == m_DisabledThreads.end();
        }

        return m_ThreadDumpStateCache.DumpEnabled;
    }

    return false;
}

void ChiplibTraceDumper::EnableChiplibTraceDump()
{
    Tasker::ThreadID id = LwrrentThreadId();
    m_DisabledThreads.erase(id);

    m_ThreadDumpStateCache.LatestThreadId = id;
    m_ThreadDumpStateCache.DumpEnabled = true;
}

void ChiplibTraceDumper::DisableChiplibTraceDump()
{
    Tasker::ThreadID id = LwrrentThreadId();
    m_DisabledThreads.insert(id);

    m_ThreadDumpStateCache.LatestThreadId = id;
    m_ThreadDumpStateCache.DumpEnabled = false;
}

void ChiplibTraceDumper::SetRawDumpChiplibTrace()
{
#ifdef INCLUDE_MDIAG
    RC rc;

    if ( m_RawDumpChiplibTrace == true )
    {
        return;
    }

    rc = RawChiplibTraceDumpInit();
    if ( rc != OK )
    {
        m_RawDumpChiplibTrace = false;
    }
    else
    {
        m_RawDumpChiplibTrace = true;
    }
#else
    m_RawDumpChiplibTrace = false;
#endif
}

void ChiplibTraceDumper::SetRawDumpChiplibTraceLevel(UINT32 level)
{
    if (static_cast<UINT32>(RawChiplibTraceDumpLevel::Unsupported) <= level)
    {
        Printf(Tee::PriHigh, "Warning: %s: Level = %u is not supported. Reset to lightweight mode\n",
              __FUNCTION__, level);
        m_RawDumpChiplibTraceLevel = RawChiplibTraceDumpLevel::Light;
    }
    else
    {
        m_RawDumpChiplibTraceLevel = static_cast<RawChiplibTraceDumpLevel>(level);
    }
}

//--------------------------------------------------------------------
// \brief Performs a raw dump of an operation to the vector_raw file
void ChiplibTraceDumper::RawDumpToFile(ChiplibOp *op)
{

    if ( m_RawDumpChiplibTrace )
    {
        op->DumpToFile(m_RawTraceFile);
    }
}

//--------------------------------------------------------------------
// \brief Private Functions; Init work before chiplib trace transactions dumped
RC ChiplibTraceDumper::RawChiplibTraceDumpInit()
{
#ifdef INCLUDE_MDIAG
    string traceName = "vector_raw.log";

    if (m_IsDumpCompressed)
    {
        traceName += ".gz";
    }

    m_RawTraceFile = Sys::GetFileIO(traceName.c_str(), "w");
    if (m_RawTraceFile == nullptr)
    {
        Printf(Tee::PriHigh, "Error: %s: can't create file %s , fh= %p\n",
                  __FUNCTION__, traceName.c_str(), m_RawTraceFile);
        return RC::CANNOT_OPEN_FILE;
    }

    PrintCopyright(m_RawTraceFile);
    PrintBuildInfo(m_RawTraceFile);
#endif
    return OK;
}

void ChiplibTraceDumper::Cleanup()
{
    // If raw chiplib trace is dumped, it is not necessary
    // to dump normal chiplib trace
    if (m_DumpChiplibTrace && !m_RawDumpChiplibTrace)
    {
        DumpToFile();
    }

    // Top means Global_ChiplibOpBlock; delete it will
    // delete all dumped operations below it.
    if (!m_RawDumpChiplibTrace)
    {
        while (m_ChiplibOpBlocks.size() > 1)
        {
            m_ChiplibOpBlocks.pop();
        }
        if (!m_ChiplibOpBlocks.empty())
        {
            delete m_ChiplibOpBlocks.top();
            m_ChiplibOpBlocks.pop();
        }
    }
    else
    {
        for (auto& p : m_RawChiplibOpBlocks)
        {
            auto chiplibOpBlock = p.second;
            // MASSERT(chiplibOpBlock.size() == 1);
            chiplibOpBlock.top()->DumpTailToFile(m_RawTraceFile);
            delete chiplibOpBlock.top();
            chiplibOpBlock.pop();
        }
        PrintTestInfo(m_RawTraceFile);
    }

#ifdef INCLUDE_MDIAG
    if ( m_RawDumpChiplibTrace )
    {
        m_RawTraceFile->FileClose();
        Sys::ReleaseFileIO(m_RawTraceFile);
    }
#endif

    m_DumpChiplibTrace = false;
    m_RawDumpChiplibTrace = false;
}

static UINT64 s_IgnoreAddrs[] =
{
    // monitor regs
    0x106900,
    0x0400c0,
    0x0400c4,

    LW_PBUS_PERFMON,
    LW_PCHIPLET_PWR_GPC0, // how to get LW_PCHIPLET_PWR range??
    LW_PFB_NISO_PM_CLIENT,
    LW_PFB_PRI_MMU_PM,
    LW_PGRAPH_PRI_BES_CROP_PPM,
    LW_PGRAPH_PRI_BES_RDM_PPM,
    LW_PGRAPH_PRI_BES_ZROP_PPM,
    LW_PGRAPH_PRI_CWD_PM,
    LW_PGRAPH_PRI_DS_PM,
    LW_PGRAPH_PRI_FECS_PERFMON,
    LW_PGRAPH_PRI_FE_PERFMON,
    LW_PGRAPH_PRI_GPCS_CRSTR_PM_CTRL,
    LW_PGRAPH_PRI_GPCS_FRSTR_PM_CTRL,
    LW_PGRAPH_PRI_GPCS_GCC_PERFMON,
    LW_PGRAPH_PRI_GPCS_GCC_TSL2_DBG1,
    LW_PGRAPH_PRI_GPCS_GPCCS_PERFMON,
    LW_PGRAPH_PRI_GPCS_GPM_PD_PM,
    LW_PGRAPH_PRI_GPCS_GPM_RPT_PM,
    LW_PGRAPH_PRI_GPCS_GPM_SD_PM,
    LW_PGRAPH_PRI_GPCS_MMU_PM,
    LW_PGRAPH_PRI_GPCS_PPCS_CBM_PM,
    LW_PGRAPH_PRI_GPCS_PPCS_PES_PM,
    LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PM,
    LW_PGRAPH_PRI_GPCS_PROP_PM_CTRL,
    LW_PGRAPH_PRI_GPCS_RASTERARB_PM_CTRL,
    LW_PGRAPH_PRI_GPCS_SETUP_PM_CTRL,
    LW_PGRAPH_PRI_GPCS_SWDX_PM,
    LW_PGRAPH_PRI_GPCS_TC_PM_CTRL,
    LW_PGRAPH_PRI_GPCS_TPCS_MPC_PM_CTRL,
    LW_PGRAPH_PRI_GPCS_TPCS_PE_PM,
    LW_PGRAPH_PRI_GPCS_TPCS_SM_PM_CTRL,
    LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PM,
    LW_PGRAPH_PRI_GPCS_TPCS_TEX_M_PM,
    LW_PGRAPH_PRI_GPCS_TPCS_TEX_SAMP_PM,
    LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PM,
    LW_PGRAPH_PRI_GPCS_WDXPS_PM,
    LW_PGRAPH_PRI_GPCS_WIDCLIP_PM_CTRL,
    LW_PGRAPH_PRI_GPCS_ZLWLL_PM_CTRL,
    LW_PGRAPH_PRI_PDB_PM,
    LW_PGRAPH_PRI_PD_PM,
    LW_PGRAPH_PRI_RSTR2D_PM_CTRL,
    LW_PGRAPH_PRI_SCC_PERFMON,
    LW_PGRAPH_PRI_SKED_PM,
    LW_PLTCG_LTCS_LTSS_MISC_LTC_LTS_PM,
    LW_PLTCG_LTCS_MISC_LTC_PM,
    LW_PPWR_PMU_PM,
    LW_XBAR_MXBAR_PRI_GPC0_GNIC_PREG_PM_CTRL,
    LW_PGRAPH_PRI_BES_CROP_DEBUG2,

    LW_PERF_PMASYS_GPC_TRIGGER_START_MASK,
    LW_PERF_PMASYS_FBP_TRIGGER_START_MASK,
    LW_PERF_PMASYS_SYS_TRIGGER_START_MASK,
    LW_PERF_PMASYS_GPC_TRIGGER_STOP_MASK,
    LW_PERF_PMASYS_FBP_TRIGGER_STOP_MASK,
    LW_PERF_PMASYS_SYS_TRIGGER_STOP_MASK,
    LW_PERF_PMASYS_GPC_TRIGGER_CONFIG_MIXED_MODE,
    LW_PERF_PMASYS_FBP_TRIGGER_CONFIG_MIXED_MODE,
    LW_PERF_PMASYS_SYS_TRIGGER_CONFIG_MIXED_MODE,
    LW_PERF_PMASYS_TRIGGER_GLOBAL,
    LW_PERF_PMASYS_CONTROL,

    // TODO: PRAMIN?

};

bool ChiplibTraceDumper::IgnoreAddr(PHYSADDR addr)
{
    PHYSADDR offset;
    AddrType type = GetPtr()->GetAddrTypeOffset(addr, &offset);
    if (type == ADDR_BAR0)
    {
        map<PHYSADDR, PHYSADDR>::const_iterator it = m_IgnoreAddrMap.upper_bound(offset);
        if (it == m_IgnoreAddrMap.begin())
            return false;

        --it;

        return offset >= it->first && offset <= it->second;
    }
    return false;
}

void ChiplibTraceDumper::DumpToFile()
{
#ifdef INCLUDE_MDIAG
    DisableChiplibTraceDump();

    string traceName = "vector.log";
    if (m_IsDumpCompressed)
    {
        traceName += ".gz";
    }

    FileIO *pTraceFile = Sys::GetFileIO(traceName.c_str(), "w");
    if (pTraceFile == nullptr)
    {
        Printf(Tee::PriHigh, "Error: %s: can't create file %s , fh= %p\n",
                  __FUNCTION__, traceName.c_str(), pTraceFile);
        return;
    }

    Printf(Tee::PriNormal, "Starting chiplib trace post process. It takes some time ...\n");

    // Data definiations; start from top level
    //
    while (m_ChiplibOpBlocks.size() > 1)
    {
        m_ChiplibOpBlocks.pop();
    }

    m_ChiplibOpBlocks.top()->PostProcessBeforeDump();

    Printf(Tee::PriNormal, "Dump chiplibtrace to file %s ...\n", traceName.c_str());

    // copyright, declearations and macro definitions
    // test run infromation like bar0/bar1 base
    PrintFileHeader(pTraceFile);

    // chiplib transactions
    //
    m_ChiplibOpBlocks.top()->DumpToFile(pTraceFile);

    pTraceFile->FileClose();
    Sys::ReleaseFileIO(pTraceFile);

    Printf(Tee::PriNormal, "Chiplibtrace is dumped successfully\n");
#else
    MASSERT(!"FileIO is not supported outside mdiag!\n");
#endif
}

//--------------------------------------------------------------------
//! \brief Constructor
//!        Add a global ChiplibOpBlock to hold all lot others
ChiplibTraceDumper::ChiplibTraceDumper():
    m_RawTraceFile(nullptr),
    m_DumpChiplibTrace(false),
    m_EnablePramin(false),
    m_RawDumpChiplibTrace(false),
    m_RawDumpChiplibTraceLevel(RawChiplibTraceDumpLevel::Light),
    m_IsGpuInfoInitialized(false),
    m_IsChiplibDumpInitialized(false),
    m_IsDumpCompressed(true),
    m_IsBar1DumpEnabled(true),
    m_IsSysmemDumpEnabled(true),
    m_Bar0Base(0),
    m_Bar0Size(0),
    m_Bar1Base(0),
    m_Bar1Size(0),
    m_InstBase(0),
    m_InstSize(0),
    m_PciDeviceId(0),
    m_StartTimeNs(0)
{
    ChiplibOpBlock *pBlock =
        new ChiplibOpBlock(ChiplibOp::CHIPLIBOP_BLOCK,
                           ChiplibOpScope::SCOPE_COMMON,
                           "Global_ChiplibOpBlock",
                           UNSIGNED_CAST(UINT32, GetStackSize()));
    Push(pBlock);

    for (UINT32 index = 0; index < sizeof(s_IgnoreAddrs) / sizeof(s_IgnoreAddrs[0]); ++index)
    {
        m_IgnoreAddrMap[s_IgnoreAddrs[index]] = s_IgnoreAddrs[index];
    }

    ChiplibTraceDumpInit();
}

ChiplibTraceDumper::~ChiplibTraceDumper()
{
}

//--------------------------------------------------------------------
//! \brief Get current block on stack top
ChiplibOpBlock* ChiplibTraceDumper::GetLwrrentChiplibOpBlock()
{
    if (m_RawDumpChiplibTrace)
    {
        if (m_RawChiplibOpBlocks.count(LwrrentThreadId()) == 0)
        {
            //Add to m_RawChiplibOpBlocks for each thread
            ChiplibOpBlock *pBlock =
                new ChiplibOpBlock(ChiplibOp::CHIPLIBOP_BLOCK,
                                   ChiplibOpScope::SCOPE_COMMON,
                                   Utility::StrPrintf("Global_ChiplibOpBlock#%d", LwrrentThreadId()),
                                   0);
            Push(pBlock);
            pBlock->DumpHeadToFile(m_RawTraceFile);
        }
        return m_RawChiplibOpBlocks.at(LwrrentThreadId()).top();
    }
    else
    {
        return m_ChiplibOpBlocks.top();
    }
}

void ChiplibTraceDumper::Push(ChiplibOpBlock *pOpBlock)
{
    if (m_RawDumpChiplibTrace)
    {
        m_RawChiplibOpBlocks[LwrrentThreadId()].push(pOpBlock);
    }
    else
    {
        m_ChiplibOpBlocks.push(pOpBlock);
    }
}

void ChiplibTraceDumper::Pop()
{
    if (m_RawDumpChiplibTrace)
    {
        m_RawChiplibOpBlocks.at(LwrrentThreadId()).pop();
    }
    else
    {
        m_ChiplibOpBlocks.pop();
    }
}

size_t ChiplibTraceDumper::GetStackSize() const
{
    if (!m_RawDumpChiplibTrace)
    {
        return m_ChiplibOpBlocks.size();
    }
    else
    {
        return m_RawChiplibOpBlocks.at(LwrrentThreadId()).size();
    }
}

RC ChiplibTraceDumper::GetGpuSubdevInfo()
{
    if (m_IsGpuInfoInitialized) return OK;

    if (!GpuPtr()->IsInitialized())
    {
        return RC::WAS_NOT_INITIALIZED;
    }
    GpuSubdevice* pSubdev = nullptr;
    GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    if (pGpuDevMgr)
        pSubdev = pGpuDevMgr->GetFirstGpu();
    if (!pSubdev || !pSubdev->IsInitialized())
    {
        return RC::SOFTWARE_ERROR;
    }

    m_Bar0Base = pSubdev->GetPhysLwBase();
    m_Bar0Size = pSubdev->LwApertureSize();
    m_Bar1Base = pSubdev->GetPhysFbBase();
    m_Bar1Size = pSubdev->FbApertureSize();
    m_InstBase = pSubdev->GetPhysInstBase();
    m_InstSize = pSubdev->InstApertureSize();

    m_GpuName = Gpu::DeviceIdToString(pSubdev->DeviceId());

    if (!Platform::IsTegra())
    {
        m_PciDeviceId = pSubdev->GetInterface<Pcie>()->DeviceId();
    }
    else
    {
        m_PciDeviceId = 0xffff;
    }

    ParseIgnoredRegisterFile();

    m_IsGpuInfoInitialized = true;

    if (m_RawDumpChiplibTrace)
    {
        PrintGpuInfo(m_RawTraceFile);
    }

    return OK;
}

RC ChiplibTraceDumper::SetPciCfgSpace
(
    INT32 pciDomainNumber,
    INT32 pciBusNumber,
    INT32 pciDevNumber,
    INT32 pciFunNumber,
    UINT32 cfgSpaceBaseAddr
)
{
    if (m_PciCfgSpaceMap.count(cfgSpaceBaseAddr) == 0)
    {
        PciCfgSpace cfgSpace = { pciDomainNumber, pciBusNumber, pciDevNumber, pciFunNumber};
        m_PciCfgSpaceMap[cfgSpaceBaseAddr] = cfgSpace;
    }
    else
    {
        if (m_PciCfgSpaceMap[cfgSpaceBaseAddr].PciDomainNumber != pciDomainNumber ||
            m_PciCfgSpaceMap[cfgSpaceBaseAddr].PciBusNumber    != pciBusNumber    ||
            m_PciCfgSpaceMap[cfgSpaceBaseAddr].PciDevNumber    != pciDevNumber    ||
            m_PciCfgSpaceMap[cfgSpaceBaseAddr].PciFunNumber    != pciFunNumber)
        {
            Printf(Tee::PriHigh, "Error: Same cfgspace Base Address for different "
                "DomainNum/BusNum/DevNum/FunNum. Need double check");
            return RC::SOFTWARE_ERROR;
        }
    }

    return OK;
}

RC ChiplibTraceDumper::GetPciCfgSpaceIdx(UINT32 compAddrBase, UINT32 *pRtnIdx)
{
    MASSERT(pRtnIdx);

    UINT32 cfgId = 0;
    map<UINT32, PciCfgSpace>::const_iterator cit = m_PciCfgSpaceMap.begin();
    for (; cit != m_PciCfgSpaceMap.end(); ++ cit, ++ cfgId)
    {
        if (compAddrBase == cit->first)
        {
            *pRtnIdx = cfgId;
            return OK;
        }
    }

    return RC::ILWALID_ARGUMENT; // not found
}

string ChiplibTraceDumper::GetAddrTypeName(AddrType type, PHYSADDR addr) const
{
    switch(type)
    {
    case ADDR_BAR0:    return "bar0";
    case ADDR_BAR1:    return "bar1";
    case ADDR_INST:    return "bar2";
    case ADDR_SYSMEM:
            return Utility::StrPrintf("sysmem%d",
                   GetSysmemSegmentId(addr));
    case ADDR_UNKNOWN: return "unknown";
    default: break;
    }

    MASSERT(!"Should not hit here!");
    return "unknown";
}

ChiplibTraceDumper::AddrType ChiplibTraceDumper::GetAddrTypeOffset
(
    PHYSADDR addr,
    PHYSADDR *pOffset
)
{
    AddrType type = ADDR_UNKNOWN;

    if (GetBar0AddrOffset(addr, pOffset))
    {
        type = ADDR_BAR0;
    }
    else if (GetBar1AddrOffset(addr, pOffset))
    {
        type = ADDR_BAR1;
    }
    else if (GetInstAddrOffset(addr, pOffset))
    {
        type = ADDR_INST;
    }
    else if(IsSysmemAccess(addr))
    {
        type = ADDR_SYSMEM;
        *pOffset = addr - GetSysmemBaseForPAddr(addr);
    }
    else
    {
        type = ADDR_UNKNOWN;
        *pOffset = 0;
    }

    return type;
}

bool ChiplibTraceDumper::GetBar0AddrOffset(PHYSADDR addr, PHYSADDR *pOffset)
{
    if (OK != GetGpuSubdevInfo())
    {
        return false;
    }

    if ((m_Bar0Base <= addr) && (m_Bar0Base + m_Bar0Size > addr))
    {
        *pOffset = addr - m_Bar0Base;
        return true;
    }
    return false;
}

bool ChiplibTraceDumper::GetBar1AddrOffset(PHYSADDR addr, PHYSADDR *pOffset)
{
    if (OK != GetGpuSubdevInfo())
    {
        return false;
    }

    if ((m_Bar1Base <= addr) && (m_Bar1Base + m_Bar1Size > addr))
    {
        *pOffset = addr - m_Bar1Base;
        return true;
    }
    return false;
}

bool ChiplibTraceDumper::GetInstAddrOffset(PHYSADDR addr, PHYSADDR *pOffset)
{
    if (OK != GetGpuSubdevInfo())
    {
        return false;
    }

    if ((m_InstBase <= addr) && (m_InstBase + m_InstSize > addr))
    {
        *pOffset = addr - m_InstBase;
        return true;
    }
    return false;
}

void ChiplibTraceDumper::SaveSysmemSegment(PHYSADDR baseAddr, UINT64 size)
{
    //  Assume:
    //      1. no overlap segment
    //      2. sysmem will not be freed/realloc before Platform::Cleanup()
    if (m_SysMemSegments.count(baseAddr) == 0)
    {
        m_SysMemSegments[baseAddr] =
            make_tuple(size, static_cast<UINT32>(m_SysMemSegments.size()));
        if (RawDumpChiplibTrace())
        {
            string str = Utility::StrPrintf(
                "@sysmem%d base 0x%08llx size 0x%08llx\n",
                get<SYSMEM_ID>(m_SysMemSegments[baseAddr]),
                baseAddr, get<SYSMEM_SIZE>(m_SysMemSegments[baseAddr]));
            m_RawTraceFile->FileWrite(str.c_str(), sizeof(str[0]),
                                      UNSIGNED_CAST(UINT32, str.size()));
        }
    }
    else
    {
        MASSERT(get<SYSMEM_SIZE>(m_SysMemSegments[baseAddr]) == size);
    }
}

UINT32 ChiplibTraceDumper::GetSysmemSegmentId(PHYSADDR addr) const
{
    static constexpr UINT32 unknownId = -1;
    for (const auto& segment : m_SysMemSegments)
    {
        const PHYSADDR base = segment.first;
        const UINT64 size = get<SYSMEM_SIZE>(segment.second);
        const UINT32 id = get<SYSMEM_ID>(segment.second);
        if ((base <= addr) && (addr < base + size))
        {
            return id;
        }
    }

    MASSERT(!"This should not happen!");
    return unknownId;
}

const ChiplibTraceDumper::RegOpInfo *ChiplibTraceDumper::GetSpecialRegOpInfo
(
    PHYSADDR addr
)
{
    if (m_SpecialRegs.count(addr))
    {
        return &m_SpecialRegs[addr];
    }

    return nullptr;
}

void ChiplibTraceDumper::SetTestNamePath(const string& testName, const string& tracePath)
{
    TestInfo testInfo = {testName, tracePath};
    m_TestsInfo.push_back(testInfo);
}

//--------------------------------------------------------------------
// \brief Private Functions; Init work before chiplib trace transactions dumped
RC ChiplibTraceDumper::ChiplibTraceDumpInit()
{
    for (UINT32 i = 0; i < sizeof(s_SpecialBusMemOpRegs)/sizeof(s_SpecialBusMemOpRegs[0]); ++ i)
    {
        PHYSADDR regBaseAddr = s_SpecialBusMemOpRegs[i].RegAddr;
        UINT32 arraySize = s_SpecialBusMemOpRegs[i].RegSize;
        UINT32 arrayPitch = s_SpecialBusMemOpRegs[i].RegAddrPitch;

        for (UINT32 regArrayIdx = 0; regArrayIdx < arraySize; ++ regArrayIdx)
        {
            m_SpecialRegs[regBaseAddr + regArrayIdx * arrayPitch] = s_SpecialBusMemOpRegs[i].RegOpInfo;
        }
    }

    // The place calling ChiplibTraceDumpInit is changed into constructor.
    // It's too early to get the time from Platform::GetTimeNS.
    // Todo: fix-me
    m_StartTimeNs = 0; //Platform::GetTimeNS();

    m_IsChiplibDumpInitialized = true;
    return OK;
}

//! \brief Private Functions; Print copyright and struct declearation
void ChiplibTraceDumper::PrintFileHeader(FileIO *pTraceFile)
{
    PrintCopyright(pTraceFile);

    string traceStr;
    //#chiplib trace info begin --------------------
    traceStr = Utility::StrPrintf("#chiplib trace info begin -------------------- \n");
    pTraceFile->FileWrite(traceStr.c_str(), sizeof(traceStr[0]),
            UNSIGNED_CAST(UINT32, traceStr.size()));

    PrintBuildInfo(pTraceFile);

    PrintGpuInfo(pTraceFile);

    PrintTestInfo(pTraceFile);

    traceStr = Utility::StrPrintf("#chiplib trace info end ---------------------- \n\n");
    pTraceFile->FileWrite(traceStr.c_str(), sizeof(traceStr[0]),
        UNSIGNED_CAST(UINT32, traceStr.size()));
}

void ChiplibTraceDumper::PrintCopyright(FileIO *pTraceFile)
{
    const char copyRight[] =
    {
        "#\n"
        "# LWIDIA_COPYRIGHT_BEGIN\n"
        "#\n"
        "# Copyright 2013 by LWPU Corporation.  All rights reserved.  All\n"
        "# information contained herein is proprietary and confidential to LWPU\n"
        "# Corporation.  Any use, reproduction, or disclosure without the written\n"
        "# permission of LWPU Corporation is prohibited.\n"
        "#\n"
        "# LWIDIA_COPYRIGHT_END\n"
        "#\n"
    };

    pTraceFile->FileWrite(copyRight, sizeof(copyRight[0]),
            UNSIGNED_CAST(UINT32, strlen(copyRight)));
}

void ChiplibTraceDumper::PrintBuildInfo(FileIO *pTraceFile)
{
    string traceStr;

    traceStr += Utility::StrPrintf("#chiplib trace info begin -------------------- \n");

    // @ swcl  12345
    // @ hwcl  67890
    traceStr += Utility::StrPrintf(
        "@version %s\n"
        "@build   %s\n",
        g_Version,
        Utility::GetBuildType().c_str());

    if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
    {
        string clNumber = "unspecified";
        if (g_Changelist != 0)
        {
            clNumber = Utility::StrPrintf("%d", g_Changelist);
        }
        traceStr += Utility::StrPrintf(
            "@changelist %s\n"
            "@build_date %s\n"
            "@modules %s\n",
            clNumber.c_str(),
            g_BuildDate,
            Utility::GetBuiltinModules().c_str());
    }
    pTraceFile->FileWrite(traceStr.c_str(), sizeof(traceStr[0]),
            UNSIGNED_CAST(UINT32, traceStr.size()));
}

void ChiplibTraceDumper::PrintGpuInfo(FileIO *pTraceFile)
{
    string traceStr;

    if (m_IsGpuInfoInitialized)
    {
        if (m_Bar0Size)
        {
            traceStr += Utility::StrPrintf(
                "@bar0 base 0x%08llx size 0x%08x\n",
                 m_Bar0Base, m_Bar0Size);
        }

        if (m_Bar1Size)
        {
            traceStr += Utility::StrPrintf(
                "@bar1 base 0x%08llx size 0x%08llx\n",
                m_Bar1Base, m_Bar1Size);
        }

        if (m_InstSize)
        {
            traceStr += Utility::StrPrintf(
                "@bar2 base 0x%08llx size 0x%08llx\n",
                m_InstBase, m_InstSize);
        }

        vector<string> sysMemSegmentsDesc(m_SysMemSegments.size());
        for (const auto& segment : m_SysMemSegments)
        {
            const PHYSADDR base = segment.first;
            const UINT64 size = get<SYSMEM_SIZE>(segment.second);
            const UINT32 id = get<SYSMEM_ID>(segment.second);
            sysMemSegmentsDesc[id] = Utility::StrPrintf(
                "@sysmem%d base 0x%08llx size 0x%08llx\n", id, base, size);
        }
        for (const auto& str : sysMemSegmentsDesc)
        {
            traceStr += str;
        }

        if (!Platform::IsTegra())
        {
            traceStr += Utility::StrPrintf(
                "@devid 0x%x\n", m_PciDeviceId);

            UINT32 cfgId = 0;
            map<UINT32, PciCfgSpace>::const_iterator cit = m_PciCfgSpaceMap.begin();
            for (; cit != m_PciCfgSpaceMap.end(); ++ cit)
            {
                traceStr += Utility::StrPrintf(
                    "@cfg%d domain 0x%x bus 0x%x dev 0x%x fun 0x%x compaddr 0x%x\n",
                    cfgId, cit->second.PciDomainNumber, cit->second.PciBusNumber,
                    cit->second.PciDevNumber, cit->second.PciFunNumber, cit->first);

                ++ cfgId;
            }
        }

        traceStr += Utility::StrPrintf("@gpu_name %s\n", m_GpuName.c_str());
    }

    pTraceFile->FileWrite(traceStr.c_str(), sizeof(traceStr[0]),
        UNSIGNED_CAST(UINT32, traceStr.size()));
}

void ChiplibTraceDumper::PrintTestInfo(FileIO *pTraceFile)
{
    string traceStr;

    for (size_t ii = 0; ii < m_TestsInfo.size(); ++ii)
    {
        if (m_TestsInfo[ii].TestName.length() > 0)
        {
            traceStr += Utility::StrPrintf(
                "@test_name %s\n", m_TestsInfo[ii].TestName.c_str());
        }

        if (m_TestsInfo[ii].TracePath.length() > 0)
        {
            traceStr += Utility::StrPrintf("@trace_path %s\n",
                m_TestsInfo[ii].TracePath.c_str());
        }
    }

    if (m_BackdoorArchiveId.length() > 0)
    {
        traceStr += Utility::StrPrintf(
            "@backdoor_archive_id %s\n", m_BackdoorArchiveId.c_str());
    }

    pTraceFile->FileWrite(traceStr.c_str(), sizeof(traceStr[0]),
            UNSIGNED_CAST(UINT32, traceStr.size()));
}

void ChiplibTraceDumper::ParseIgnoredRegisterFile()
{
    if (m_ignoredRegFile.empty()) return;

    std::ifstream fs(m_ignoredRegFile.c_str());
    if (!fs.is_open())
    {
        Printf(Tee::PriNormal, "Unable to open file \"%s\"\n", m_ignoredRegFile.c_str());
        return;
    }
    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    RefManual* pRefMan( pSubdev->GetRefManual() );
    std::string line;
    while (fs.good())
    {
        getline(fs, line);

        std::vector<std::string> tokens;
        Utility::Tokenizer(line, " ", &tokens);
        if(tokens.size() == 0)
            continue;

        UINT64 regAddr1 = 0;
        char* p;
        errno = 0;
        regAddr1 = Utility::Strtoull(tokens[0].c_str(), &p, 0);
        bool validEntry = true;
        if ((p == tokens[0].c_str()) ||
            (p && *p != 0) ||
            (ERANGE == errno)) // Catching out of range
        {
            const RefManualRegister* reg = pRefMan->FindRegister(tokens[0].c_str());
            if (reg)
                regAddr1 = reg->GetOffset();
            else
                validEntry = false;
        }

        UINT64 regAddr2 = regAddr1;
        if (tokens.size() > 1)
        {
            errno = 0;
            regAddr2 = Utility::Strtoull(tokens[1].c_str(), &p, 0);
            if ((p == tokens[1].c_str()) ||
                (p && *p != 0) ||
                (ERANGE == errno)) // Catching out of range
            {
                const RefManualRegister* reg = pRefMan->FindRegister(tokens[1].c_str());
                if (reg)
                    regAddr2 = reg->GetOffset();
                else
                    validEntry = false;
            }
        }

        if(validEntry)
            m_IgnoreAddrMap[regAddr1] = regAddr2;
        else
            Printf(Tee::PriNormal, "WARNING: invalid entry in ignored reg file: \"%s\".\n", line.    c_str());
    }

}
void ChiplibTraceDumper::AddArchiveFile(const std::string& dumpFileName, const std::string& archiveFileName)
{
    m_BackdoorArchiveFiles.insert(std::make_pair(dumpFileName, archiveFileName));
}
RC ChiplibTraceDumper::DumpArchiveFiles()
{
    RC rc;
    unique_ptr<TarArchive> archive(new TarArchive());
    for (map<string,string>::const_iterator it = m_BackdoorArchiveFiles.begin();
            it != m_BackdoorArchiveFiles.end(); ++it)
    {
        FileHolder file;
        rc = file.Open(it->first, "r");
        if (OK != rc)
        {
            MASSERT(!"Failed to open archive file.");
            return rc;
        }
        long fileSize = 0;
        CHECK_RC(Utility::FileSize(file.GetFile(), &fileSize));
        vector<char> buffer;
        buffer.resize(fileSize);

        if ((size_t)fileSize != fread(&buffer[0], 1, fileSize, file.GetFile()))
        {
            Printf(Tee::PriHigh, "Error reading file %s\n", it->first.c_str());
            return RC::FILE_READ_ERROR;
        }
        unique_ptr<TarFile> tarFile(new TarFile_SCImp());
        tarFile->SetFileName(it->second);
        tarFile->Write(&buffer[0], fileSize);
        archive->AddToArchive(tarFile.get());
    }
    if (m_BackdoorArchiveFiles.size() && m_TestsInfo.size() > 0)
    {
        string archiveName = m_TestsInfo[0].TracePath + "/backdoor.tgz";
        if (!archive->WriteToFile(archiveName))
        {
            Printf(Tee::PriHigh, "Error: couldn't write archive file %s\n", archiveName.c_str());
            return RC::SOFTWARE_ERROR;
        }
    }
    return OK;
}

/*static*/ string ChiplibOpScope::GetScopeDescription
(
    const string& scopeInfo,
    UINT08 irq,
    ScopeType type,
    void* extraInfo)
{
    if (extraInfo && (type == SCOPE_CRC_SURF_READ))
    {
        Crc_Surf_Read_Info *pSurfInfo =
            reinterpret_cast<Crc_Surf_Read_Info*>(extraInfo);
        static int unknownSurfIndex = 0;
        string scopeDes = Utility::StrPrintf("%s surfName %s surfFmt %s "
            "surfPitch %d surfWidth %d surfHeight %d surfBytesPerPix %d "
            "surfLayers %d surfArraySize %d rectSrcX %d rectSrcY %d "
            "rectSrcWidth %d rectSrcHeight %d rectDestX %d rectDestY %d",
            scopeInfo.c_str(),
            pSurfInfo->SurfName.empty() ?
                Utility::StrPrintf("unknown_%d", unknownSurfIndex++).c_str() :
                pSurfInfo->SurfName.c_str(),
            pSurfInfo->SurfFormat.c_str(),
            pSurfInfo->SurfPitch,
            pSurfInfo->SurfWidth,
            pSurfInfo->SurfHeight,
            pSurfInfo->SurfBytesPerPixel,
            pSurfInfo->SurfNumLayers,
            pSurfInfo->SurfArraySize,
            pSurfInfo->RectSrcX,
            pSurfInfo->RectSrcY,
            pSurfInfo->RectSrcWidth,
            pSurfInfo->RectSrcHeight,
            pSurfInfo->RectDestX,
            pSurfInfo->RectDestY);

        return scopeDes;
    }

    if (type == SCOPE_IRQ)
    {
        return Utility::StrPrintf("%s IRQ %u", scopeInfo.c_str(), irq);
    }

    return scopeInfo;
}

ChiplibOpScope::ChiplibOpScope
(
    const string& scopeInfo,
    UINT08 irq,
    ScopeType type,
    void* extraInfo,
    bool optional
):
    m_CancelOpBlock(false)
{
    if (ChiplibTraceDumper::GetPtr()->IsChiplibTraceDumpEnabled())
    {
        // Flush the cached transactions before entering a new block
        Platform::FlushCpuWriteCombineBuffer();

        ChiplibTraceDumper *pDumper = ChiplibTraceDumper::GetPtr();
        ChiplibOpBlock *lwrBlock = pDumper->GetLwrrentChiplibOpBlock();

        if (pDumper->GetStackSize() > (std::numeric_limits<UINT32>::max)())
        {
            Printf(Tee::PriHigh, "Error: ChiplibOpScope::ChiplibOpScope pDumper->GetStackSize() larger than UINT32\n");
            MASSERT(!"pDumper->GetStackSize() larger than UINT32!");
        }

        ChiplibOpBlock *pBlock = lwrBlock->AddBlockOp(
            ChiplibOp::CHIPLIBOP_BLOCK, type,
            GetScopeDescription(scopeInfo, irq, type, extraInfo),
            UNSIGNED_CAST(UINT32, pDumper->GetStackSize()), optional);

        if (pBlock)
        {
            pDumper->Push(pBlock);
            if (pDumper->RawDumpChiplibTrace())
            {
                const bool notDumping = ((type == SCOPE_POLL) &&
                    pDumper->GetRawDumpChiplibTraceLevel() < ChiplibTraceDumper::RawChiplibTraceDumpLevel::Heavy);
                if (!notDumping)
                {
                    pBlock->DumpHeadToFile(pDumper->GetRawTraceFile());
                }
            }
        }
    }
}

ChiplibOpScope::~ChiplibOpScope()
{
    if (ChiplibTraceDumper::GetPtr()->IsChiplibTraceDumpEnabled())
    {
        // flush the cached transactions before exiting a new block
        Platform::FlushCpuWriteCombineBuffer();

        ChiplibTraceDumper *pDumper = ChiplibTraceDumper::GetPtr();
        if (pDumper->RawDumpChiplibTrace())
        {
            ChiplibOpBlock *pLwrBlock = pDumper->GetLwrrentChiplibOpBlock();
            const bool notDumping = ((pLwrBlock->GetChiplibOpScopeType() == SCOPE_POLL) &&
                pDumper->GetRawDumpChiplibTraceLevel() < ChiplibTraceDumper::RawChiplibTraceDumpLevel::Heavy);
            if (!notDumping)
            {
                pLwrBlock->DumpTailToFile(pDumper->GetRawTraceFile());
            }
        }

        ChiplibTraceDumper::GetPtr()->Pop();

        if (m_CancelOpBlock)
        {
            // The operations in scope is canceled; drop last OpBlock inserted
            //
            ChiplibOpBlock *lwrBlock = ChiplibTraceDumper::GetPtr()->GetLwrrentChiplibOpBlock();
            RC rc = lwrBlock->DropLastOp();
            if (rc != OK)
            {
                MASSERT(!"Please check why DropLastOp failed!");
            }
        }
    }
}
