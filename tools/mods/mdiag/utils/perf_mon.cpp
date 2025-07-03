/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file perf_mon.cpp
//! \brief Performance monitor
//!
//! Body for the Performance Monitor class
//!

#ifndef LW_WINDOWS
#include <unistd.h>
#endif

#include <utility>
#include <unordered_map>

#include "core/include/utility.h"
#include "mdiag/lwgpu.h"
#include "lwgpu_channel.h"

#include "mdiag_xml.h"
#include "class/cl9097.h"
#include "perf_mon.h"
#include "class/cl844c.h"       // G84_PERFBUFFER
#include "fermi/gf100/dev_disp.h"
#include "xml_wrapper.h"
#include "xml_node.h"
#include "mdiag/tests/testdir.h"
#include "mdiag/utils/buffinfo.h"
#include "gpu/include/floorsweepimpl.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/smc/gpupartition.h"

// This test needs to move from a gt200 based PM to a modern design....
//     Defines cloned from hwref/gt200/dev_pm.h which has been removed
#define LW_PPM_CONTROL(i)                        (0x0000A7C0+(i)*4) /* RW-4A */
#define LW_PPM_CONTROL_SHADOW_STATE                           25:24 /* RRIUF */
#define LW_PPM_CONTROL_SHADOW_STATE_ILWALID              0x00000000 /* RRI-V */
#define LW_PPM_CONTROL_SHADOW_STATE_VALID                0x00000001 /* RR--V */
#define LW_PPM_CONTROL_SHADOW_STATE_OVERFLOW             0x00000003 /* RR--V */
#define LW_PPM_CONTROL_STATE                                  29:28 /* RRIUF */
#define LW_PPM_CONTROL_STATE_IDLE                        0x00000000 /* RRI-V */
#define LW_PPM_CONTROL_STATE_WAIT_TRIG0                  0x00000001 /* RR--V */
#define LW_PPM_CONTROL_STATE_WAIT_TRIG1                  0x00000002 /* RR--V */
#define LW_PPM_CONTROL_STATE_CAPTURE                     0x00000003 /* RR--V */
#define LW_PPM_CYCLECNT_0(i)                     (0x0000A640+(i)*4) /* RR-4A */
#define LW_PPM_EVENTCNT_0(i)                     (0x0000A680+(i)*4) /* RR-4A */

class HwpmReg
{
public:
    HwpmReg(LWGpuResource* pGpuRes)
    : m_pGpuRes(pGpuRes)
    {
    }

    virtual ~HwpmReg() = 0;

    virtual void ConfigurePMA(UINT32 bufferSize, UINT32 channelIndex) = 0;

protected:
    LWGpuResource* m_pGpuRes;
};

HwpmReg::~HwpmReg() {}

// Before GA100
class LegacyHwpmReg : public HwpmReg
{
public:
    LegacyHwpmReg(LWGpuResource* pGpuRes)
    : HwpmReg(pGpuRes) {}

    ~LegacyHwpmReg() override {}

    void ConfigurePMA(UINT32 bufferSize, UINT32 channelIndex) override
    {
        GpuSubdevice *pSubDev = m_pGpuRes->GetGpuSubdevice();

        UINT32 outbaseAddr = pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_OUTBASE);
        UINT32 outbase = m_pGpuRes->RegRd32(outbaseAddr);
        PMInfo("PM Perfmon buffer setup: RegRead LW_PERF_PMASYS_OUTBASE addr 0x%08x value 0x%08x\n", outbaseAddr, outbase);

        UINT32 outsizeAddr = pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_OUTSIZE);
        UINT32 outsize = m_pGpuRes->RegRd32(outsizeAddr);
        PMInfo("PM Perfmon buffer setup: RegRead LW_PERF_PMASYS_OUTSIZE addr 0x%08x value 0x%08x\n", outsizeAddr, outsize);

        if (bufferSize < outsize)
        {
            PMInfo("PM Perfmon buffer setup: LW_PERF_PMASYS_OUTSIZE > requested byte count, limiting to byte_count(%d bytes)\n",
                    bufferSize);
            outsize = bufferSize;
        }
        else if (bufferSize > outsize)
        {
            PMInfo("PM Perfmon buffer setup: LW_PERF_PMASYS_OUTSIZE (%d bytes) < requested byte count (%d bytes), "
                   "PMA cannot access required address space\n", outsize, bufferSize);
            MASSERT(0);
        }
        else
        {
            PMInfo("PM Perfmon buffer setup: LW_PERF_PMASYS_OUTSIZE matches requested byte count\n");
        }

        // Reserve upper 32(single 256 bit record) bytes for memory byte counter
        const UINT32 memBytesAddrSize = 32;
        const UINT32 memBytesAddr = outbase + outsize - memBytesAddrSize;
        outsize = outsize - memBytesAddrSize;

        const UINT32 pmaMemBytesAddr = pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_MEM_BYTES_ADDR);
        PMInfo("PM Perfmon buffer setup: RegWrite LW_PERF_PMASYS_MEM_BYTES_ADDR addr 0x%08x value 0x%08x\n",
               pmaMemBytesAddr, memBytesAddr);
        m_pGpuRes->RegWr32(pmaMemBytesAddr, memBytesAddr);

        const UINT32 pmaOutSizeAddr = pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_OUTSIZE);
        PMInfo("PM Perfmon buffer setup: RegWrite MODS_PERF_PMASYS_OUTSIZE addr 0x%08x value 0x%08x\n",
               pmaOutSizeAddr, outsize);
        m_pGpuRes->RegWr32(pmaOutSizeAddr, outsize);

        // enable MODS_PERF_PMASYS_CONTROL_STREAM_ENABLE
        const UINT32 pmaControlAddr = pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_CONTROL);
        const UINT32 pmaControlStreamMask = pSubDev->Regs().LookupMask(MODS_PERF_PMASYS_CONTROL_STREAM);
        UINT32 pmaControlVal = m_pGpuRes->RegRd32(pmaControlAddr);
        pmaControlVal &= ~pmaControlStreamMask;
        pmaControlVal |= (pSubDev->Regs().LookupValue(MODS_PERF_PMASYS_CONTROL_STREAM_ENABLE) & pmaControlStreamMask);
        PMInfo("PM Perfmon buffer setup: RegWrite MODS_PERF_PMASYS_CONTROL addr 0x%08x value 0x%08x\n",
               pmaControlAddr, pmaControlVal);
        m_pGpuRes->RegWr32(pmaControlAddr, pmaControlVal);
    }
};

class GA100HwpmReg : public HwpmReg
{
public:
    GA100HwpmReg(LWGpuResource* pGpuRes)
    : HwpmReg(pGpuRes) {}

    ~GA100HwpmReg() override {}

    void ConfigurePMA(UINT32 bufferSize, UINT32 channelIndex) override
    {
        //FIXME: Once Ampere-151 is supported, HW will add a fixed MACRO to indicate CHANNEL size
        //       For now we just use one of CHANNEL registers size and assume all of them have same dimension
        GpuSubdevice *pSubDev = m_pGpuRes->GetGpuSubdevice();
        for (UINT32 i = 0; i < pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_CHANNEL_CONTROL__SIZE_1); i++)
        {
            UINT32 outbaseAddr = pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_CHANNEL_OUTBASE, i);
            UINT32 outbase = m_pGpuRes->RegRd32(outbaseAddr);
            PMInfo("PM Perfmon buffer setup: RegRead LW_PERF_PMASYS_CHANNEL_OUTBASE(%u) addr 0x%08x value 0x%08x\n",
                   i, outbaseAddr, outbase);

            UINT32 outsizeAddr = pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_CHANNEL_OUTSIZE, i);
            UINT32 outsize = m_pGpuRes->RegRd32(outsizeAddr);
            PMInfo("PM Perfmon buffer setup: RegRead LW_PERF_PMASYS_CHANNEL_OUTSIZE(%u) addr 0x%08x value 0x%08x\n",
                   i, outsizeAddr, outsize);

            if (bufferSize < outsize)
            {
                PMInfo("PM Perfmon buffer setup: LW_PERF_PMASYS_CHANNEL_OUTSIZE > requested byte count, "
                       "limiting to byte_count(%d bytes)\n", bufferSize);
                outsize = bufferSize;
            }
            else if (bufferSize > outsize)
            {
                PMInfo("PM Perfmon buffer setup: LW_PERF_PMASYS_CHANNEL_OUTSIZE (%d bytes) < "
                       "requested byte count (%d bytes), PMA cannot access required address space\n",
                       outsize, bufferSize);
                MASSERT(0);
            }
            else
            {
                PMInfo("PM Perfmon buffer setup: LW_PERF_PMASYS_CHANNEL_OUTSIZE matches requested byte count\n");
            }

            // Reserve upper 32(single 256 bit record) bytes for memory byte counter
            const UINT32 memBytesAddrSize = 32;
            const UINT32 memBytesAddr = outbase + outsize - memBytesAddrSize;
            outsize = outsize - memBytesAddrSize;

            const UINT32 pmaMemBytesAddr = pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_CHANNEL_MEM_BYTES_ADDR, i);
            PMInfo("PM Perfmon buffer setup: RegWrite LW_PERF_PMASYS_CHANNEL_MEM_BYTES_ADDR(%u) addr 0x%08x value 0x%08x\n",
                   i, pmaMemBytesAddr, memBytesAddr);
            m_pGpuRes->RegWr32(pmaMemBytesAddr, memBytesAddr);

            const UINT32 pmaOutSizeAddr = pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_CHANNEL_OUTSIZE, i);
            PMInfo("PM Perfmon buffer setup: RegWrite LW_PERF_PMASYS_CHANNEL_OUTSIZE(%u) addr 0x%08x value 0x%08x\n",
                   i, pmaOutSizeAddr, outsize);
            m_pGpuRes->RegWr32(pmaOutSizeAddr, outsize);

            // enable MODS_PERF_PMASYS_CONTROL_STREAM_ENABLE
            const UINT32 pmaControlAddr = pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_CHANNEL_CONTROL, i);
            const UINT32 pmaControlStreamMask = pSubDev->Regs().LookupMask(MODS_PERF_PMASYS_CHANNEL_CONTROL_STREAM);
            UINT32 pmaControlVal = m_pGpuRes->RegRd32(pmaControlAddr);
            pmaControlVal &= ~pmaControlStreamMask;
            pmaControlVal |= (pSubDev->Regs().LookupValue(MODS_PERF_PMASYS_CHANNEL_CONTROL_STREAM_ENABLE) & pmaControlStreamMask);
            PMInfo("PM Perfmon buffer setup: RegWrite LW_PERF_PMASYS_CHANNEL_CONTROL(%u) addr 0x%08x value 0x%08x\n",
                   i, pmaControlAddr, pmaControlVal);
            m_pGpuRes->RegWr32(pmaControlAddr, pmaControlVal);
        }
    }
};

class GH100HwpmReg : public HwpmReg
{
public:
    GH100HwpmReg(LWGpuResource* pGpuRes)
    : HwpmReg(pGpuRes) {}

    ~GH100HwpmReg() override {}

    void ConfigurePMA(UINT32 bufferSize, UINT32 channelIndex) override
    {
        GpuSubdevice *pSubDev = m_pGpuRes->GetGpuSubdevice();
        MASSERT(channelIndex < pSubDev->Regs().LookupAddress(MODS_PERF_PMASYS_CHANNEL_CONTROL_SELWRE__SIZE_1));
            
        UINT32 outbase = pSubDev->Regs().Read32(MODS_PERF_PMASYS_CHANNEL_OUTBASE, channelIndex);
        PMInfo("PM Perfmon buffer setup: RegRead LW_PERF_PMASYS_CHANNEL_OUTBASE(%u) value 0x%08x\n",
            channelIndex, outbase);

        UINT32 outsize = pSubDev->Regs().Read32(MODS_PERF_PMASYS_CHANNEL_OUTSIZE, channelIndex);
        PMInfo("PM Perfmon buffer setup: RegRead LW_PERF_PMASYS_CHANNEL_OUTSIZE(%u) value 0x%08x\n",
            channelIndex, outsize);

        if (bufferSize < outsize)
        {
            PMInfo("PM Perfmon buffer setup: LW_PERF_PMASYS_CHANNEL_OUTSIZE > requested byte count.\n");
            PMInfo("Limiting to byte_count(0x%08x bytes)\n.", bufferSize);
            outsize = bufferSize;
        }
        else if (bufferSize > outsize)
        {
            PMInfo("PM Perfmon buffer setup: PMA cannot access required address space.\n");
            PMInfo("LW_PERF_PMASYS_CHANNEL_OUTSIZE (0x%08x bytes) < requested byte count (0x%08x bytes).\n",
                outsize, bufferSize);
            MASSERT(0);
        }
        else
        {
            PMInfo("PM Perfmon buffer setup: LW_PERF_PMASYS_CHANNEL_OUTSIZE matches requested byte count.\n");
        }

        // Reserve upper 32(single 256 bit record) bytes for memory byte counter
        const UINT32 memBytesAddrSize = 32;
        const UINT32 memBytesAddr = outbase + outsize - memBytesAddrSize;
        outsize = outsize - memBytesAddrSize;

        PMInfo("PM Perfmon buffer setup: RegWrite LW_PERF_PMASYS_CHANNEL_MEM_BYTES_ADDR(%u) addr value 0x%08x\n",
            channelIndex, memBytesAddr);
        pSubDev->Regs().Write32(MODS_PERF_PMASYS_CHANNEL_MEM_BYTES_ADDR, channelIndex, memBytesAddr);

        PMInfo("PM Perfmon buffer setup: RegWrite LW_PERF_PMASYS_CHANNEL_OUTSIZE(%u) value 0x%08x\n",
            channelIndex, outsize);
        pSubDev->Regs().Write32(MODS_PERF_PMASYS_CHANNEL_OUTSIZE, channelIndex, outsize);
    }
};

/*
 * PerfmonBuffer is to support streaming capture including ModeC, ModeE and SMPC.
 * */
PerfmonBuffer::PerfmonBuffer(LWGpuResource* pGpuRes, MdiagSurf* pSurf, bool isModeCAlloc):

    m_File(nullptr), m_IsModeCAlloc(isModeCAlloc), m_pGpuRes(pGpuRes), m_pSurface(pSurf)
{
    InitHwpmRegs();
}

PerfmonBuffer::PerfmonBuffer(UINT32 sizeInBytes, Memory::Location aperture, LWGpuResource* pGpuRes, bool isModeCAlloc):
    PerfmonBuffer(pGpuRes, nullptr, isModeCAlloc) 
{
    m_SizeInBytes = sizeInBytes;
    m_OwnedSurface = make_unique<MdiagSurf>();
    m_pSurface = m_OwnedSurface.get();
    m_pSurface->SetName("Perfmon buffer");
    // In the future, we need to allow to use command line argument to override surface size
    m_pSurface->SetArrayPitch(sizeInBytes);
    m_pSurface->SetLocation(aperture);
}

PerfmonBuffer::~PerfmonBuffer()
{
    if (m_File)
    {
        fclose(m_File);
        m_File = nullptr;
    }
    if (m_pSurface->GetSize())
    {
        if (m_pSurface->GetAddress())
        {
            m_pSurface->Unmap();
        }
        m_pSurface->Free();
    }
}

UINT32 PerfmonBuffer::GetSize() const
{
    return m_SizeInBytes;
}

void PerfmonBuffer::Write(UINT32 offset, UINT32 value)
{
    UINT64 addr = (UINT64) m_pSurface->GetAddress();
    MEM_WR32(addr + offset, value);
}

UINT32 PerfmonBuffer::Read(UINT32 offset)
{
    UINT64 addr = (UINT64) m_pSurface->GetAddress();
    return MEM_RD32(addr + offset);
}

bool PerfmonBuffer::OpenFile(const string& path)
{
    RC rc;
    const char* mode = m_IsModeCAlloc ? "w" : "wb";
    rc = Utility::OpenFile(path.c_str(), &m_File, mode);
    if (rc != OK)
    {
        ErrPrintf("%s\n", rc.Message());
    }

    return m_File;
}

bool PerfmonBuffer::IsFileOpened() const
{
    return m_File;
}

void PerfmonBuffer::CloseFile()
{
    fclose(m_File);
    m_File = nullptr;
}

bool PerfmonBuffer::IsModeCAlloc() const
{
    return m_IsModeCAlloc;
}

void PerfmonBuffer::Setup(LwRm* pLwRm)
{
    // allocate and init buffer
    Allocate(pLwRm);

    // Update buffer size
    // specify MEM_BYTES address
    // enable streaming
    ConfigurePMA();
};

void PerfmonBuffer::DumpToFile()
{
    if (m_IsModeCAlloc)
    {
        DumpTextToFile();
    }
    else
    {
        DumpBinaryToFile();
    }
}

void PerfmonBuffer::InitHwpmRegs()
{
    if (m_pGpuRes->GetGpuDevice()->GetFamily() < GpuDevice::Ampere)
    {
        m_HwpmReg = make_unique<LegacyHwpmReg>(m_pGpuRes);
    }
    else if ((m_pGpuRes->GetGpuDevice()->GetFamily() == GpuDevice::Ampere) ||
             (m_pGpuRes->GetGpuDevice()->GetFamily() == GpuDevice::Ada))
    {
        m_HwpmReg = make_unique<GA100HwpmReg>(m_pGpuRes);
    }
    else
    {
        m_HwpmReg = make_unique<GH100HwpmReg>(m_pGpuRes);
    }
}

void PerfmonBuffer::Allocate(LwRm* pLwRm)
{
    m_SizeInBytes = (UINT32)(m_pSurface->GetArrayPitch());
    // https://p4viewer.lwpu.com/get/hw/lwgpu/ip/perf/hwpm/2.0/defs/public/registers/pri_perf_pma.ref
    // OUTSIZE register contains the size, 32 byte granularity, of the buffer
    // allocated in memory.  OUTBASE plus OUTSIZE must be in the same 4G space
    // (output buffer may not cross a 4G boundary).
    MASSERT(m_SizeInBytes && ((m_SizeInBytes % 32) == 0));
    MASSERT(!m_pSurface->IsAllocated());

    PMInfo("PM Perfmon buffer will alloc buffer size = %ld location = %s\n", m_SizeInBytes, Memory::GetMemoryLocationName(m_pSurface->GetLocation()));

    // Intend to disable GPU cache in order to make sure MODS can always read correct packets
    // It's required by GV11B and T194
    m_pSurface->SetGpuCacheMode(Surface2D::GpuCacheOff);
    m_pSurface->SetColorFormat(ColorUtils::Y8);
    m_pSurface->SetProtect(Memory::ReadWrite);
    m_pSurface->SetLayout(Surface2D::Pitch);
    m_pSurface->SetParentClass(G84_PERFBUFFER);

    if (m_pSurface->Alloc(m_pGpuRes->GetGpuDevice(), pLwRm) != OK)
    {
        MASSERT(!"Memory allocation error!");
    }

    if (m_pSurface->Map() != OK)
    {
        MASSERT(!"Memory map error!");
    }

    BuffInfo buffInfo;
    buffInfo.SetDmaBuff(m_pSurface->GetName(), *m_pSurface);
    buffInfo.Print("Perfmon buffer", m_pGpuRes->GetGpuDevice());

    UINT64 addr = (UINT64) m_pSurface->GetAddress();

    for (UINT32 offset = 0; offset < m_SizeInBytes; offset += 4)
    {
        MEM_WR32(addr + offset, 0x00);
    }
}

void PerfmonBuffer::ConfigurePMA()
{
    m_HwpmReg->ConfigurePMA(m_SizeInBytes, 0);
}

void PerfmonBuffer::DumpTextToFile()
{
    MASSERT(m_File);
    UINT64 addr = (UINT64) m_pSurface->GetAddress();
    if (!addr)
    {
        MASSERT(!"Cannot map memory for perfmon buffer");
    }

    for (UINT32 offset = 0; offset < m_SizeInBytes; offset += 4, addr += 4)
    {
        UINT32 data = MEM_RD32(addr);
        string message = Utility::StrPrintf("Dump: addr=%llu, offset=%u, data=0x%08x\n", addr, offset, data);
        fprintf(m_File, "%s", message.c_str());
    }
}

void PerfmonBuffer::DumpBinaryToFile()
{
    MASSERT(m_File);
    UINT64 addr = (UINT64) m_pSurface->GetAddress();
    if (!addr)
    {
        MASSERT(!"Cannot map memory for perfmon buffer");
    }

    RC rc;
    for (UINT32 offset = 0; offset < m_SizeInBytes; offset += 4, addr += 4)
    {
        UINT32 data = MEM_RD32(addr);
        rc = Utility::FWrite(&data, sizeof(data), 1, m_File);
        if (rc != OK)
        {
            ErrPrintf("%s\n", rc.Message());
            MASSERT(!"Failed to write data info file");
        }
    }
}

static const char *s_state_to_string[4][4] = { {
                                    "LW_PPM_CONTROL_STATE_IDLE",
                                    "LW_PPM_CONTROL_STATE_WAIT_TRIG0",
                                    "LW_PPM_CONTROL_STATE_WAIT_TRIG1",
                                    "LW_PPM_CONTROL_STATE_CAPTURE"
                                    },
                                    {
                                    "LW_PPM_CONTROL_SHADOW_STATE_ILWALID",
                                    "LW_PPM_CONTROL_SHADOW_STATE_VALID",
                                    "LW_PPM_CONTROL_SHADOW_STATE_UNKNOWN",
                                    "LW_PPM_CONTROL_SHADOW_STATE_OVERFLOWED"
                                    },
                                    {
                                         // What the value should be for MODE C?
                                         "Not Defined for MODE C",
                                         "Not Defined for MODE C",
                                         "Not Defined for MODE C",
                                         "Not Defined for MODE C",
                                    },
                                    {
                                         // What the value should be for MODE E?
                                         "Not Defined for MODE GENERIC_STREAMING",
                                         "Not Defined for MODE GENERIC_STREAMING",
                                         "Not Defined for MODE GENERIC_STREAMING",
                                         "Not Defined for MODE GENERIC_STREAMING",
                                    }
                                  };

DomainInfo::DomainInfo
(
    const int index, const char *name, PerformanceMonitor *pm
) :
    m_Index(index), m_DomainName(name), m_PerfMon(pm),
    m_InUse(false), m_PmMode(PM_MODE_UNKNOWN)
{
}

// Used to indicate if debug messages need to be printed
bool PerformanceMonitor::m_PMDebugInfo = true;

vector<UINT32> PerformanceMonitor::s_ZLwllIds;
vector<const MdiagSurf*> PerformanceMonitor::s_RenderSurfaces;

PerformanceMonitor::PerformanceMonitor(LWGpuResource *lwgpu_res,
    UINT32 subdev, ArgReader *in_params)
    : m_pGpuRes(lwgpu_res),
      m_subdev(subdev),
      m_TriggerMethod(0), 
      m_Params(nullptr),
      m_TpcMask(0), 
      m_FbMask(0)
{
    DebugPrintf("PerformanceMonitor::PerformanceMonitor()\n");

    m_TriggerMethod = LW9097_PM_TRIGGER;
    m_Params = in_params;
    m_PMDebugInfo = !in_params->ParamPresent("-pm_disable_info_log");
    GpuSubdevice* pSubDev = lwgpu_res->GetGpuSubdevice(subdev);
    m_TpcMask = lwgpu_res->GetTpcMask(subdev);
    m_FbMask = pSubDev->GetFsImpl()->FbMask();
}

PerformanceMonitor::~PerformanceMonitor()
{
    DebugPrintf("PerformanceMonitor::~PerformanceMonitor()\n");
}

int PerformanceMonitor::Initialize(const char *pm_report_option,
    const char *experiment_file_name)
{
    long lwrrent_batch = LWGpuResource::GetTestDirectory()->Get_test_id_being_setup();
    InfoPrintf("PerformanceMonitor::Initialize\n");

    m_PmReportInfo.Init(pm_report_option);

    if (experiment_file_name)
    {
        m_ExperimentFileName = experiment_file_name;

#ifndef LW_WINDOWS
        InfoPrintf("PerformanceMonitor::Initialize searching for pm_file %s\n",m_ExperimentFileName.c_str());
        if (access(m_ExperimentFileName.c_str(), F_OK) == -1)
        {
            InfoPrintf("PerformanceMonitor::Initialize pm_file %s does not exist\n",m_ExperimentFileName.c_str());

            char temp_buf [64];
            sprintf(temp_buf, "%.4ld.xml",lwrrent_batch - 1);
            m_ExperimentFileName += temp_buf;
            InfoPrintf("PerformanceMonitor::Initialize searching for pm_file %s\n",m_ExperimentFileName.c_str());
            if (access(m_ExperimentFileName.c_str(), F_OK) == -1)
            {
                InfoPrintf("PerformanceMonitor::Initialize pm_file %s does not exist\n",m_ExperimentFileName.c_str());

                ErrPrintf("PerformanceMonitor::Initialize unable to find pm_file for single or batch run\n");
                ErrPrintf("PerformanceMonitor::Initialize failed\n");
                return 0;
            }
        }
#endif

        if (!LoadPMFile(m_ExperimentFileName))
        {
            ErrPrintf("Error loading experiment file %s initialization failed\n",
                      m_ExperimentFileName.c_str());
            return 0;
        }
    }
    InfoPrintf("PerformanceMonitor::Initialize() : PM enabled\n");

    if (m_PmReportInfo.WaitForInterrupt())
    {
        InfoPrintf("PerformanceMonitor::Initialize() : PM waiting for interrupt to continue.\n");
    }
    else
    {
        //Called by PerformanceMonitor::Start
        //PMRegInit();
        ;
    }

    return 1;
}

void PerformanceMonitor::Start(LWGpuChannel *channel, UINT32 subchannel,
    UINT32 class_to_use)
{
    long lwrrent_time = 0;
    LwRm* pLwRm = channel->GetLwRmPtr();

    if (gXML)
    {
        lwrrent_time = gXML->GetRuntimeTimeInMilliSeconds();
    }

    PMInfo("PerformanceMonitor::Start(subch=0x%x, class=0x%x, now=%d)\n",
        subchannel, class_to_use, lwrrent_time);

    if (!channel->WaitForIdle())
    {
        ErrPrintf("PerformanceMonitor::Start, WaitForIdle call failed\n");
        return;
    }

    if (!channel->WaitGrIdle())
    {
        ErrPrintf("PerformanceMonitor::Start, WaitGrIdle call failed\n");
        return;
    }

    DomainInfoList::const_iterator dom_iter;

    if (m_PmReportInfo.WaitForInterrupt())
    {
        // Check for domains using mode A.
        for (dom_iter = m_DomainInfoList.begin();
            dom_iter != m_DomainInfoList.end();
            ++dom_iter)
        {
            DomainInfo *domain = *dom_iter;

            if ((domain->GetMode() == PM_MODE_A) && domain->IsInUse())
            {
                WarnPrintf("PM is using PM_REPORT_WAIT_FOR_INTERRUPT but %s uses a mode A experiment.  You may get unexpected results.\n",
                    domain->GetName());
            }
        }

        PMInfo("PerformanceMonitor::Start returning, PM waiting for interrupt\n");
        return;
    }

    // Ilwalidate the L2 cache so that perf results are consistent.

    if (m_Params->ParamPresent("-pm_ilwalidate_cache") > 0)
    {
        InfoPrintf("PerformanceMonitor - ilwalidating L2 cache with L2_ILWALIDATE_CLEAN before PM trigger\n");
        RC rc = m_pSubDev->IlwalidateL2Cache(GpuSubdevice::L2_ILWALIDATE_CLEAN);

        if (rc != OK)
        {
            ErrPrintf("PerformanceMonitor- Error (%s) in ilwalidating L2\n", rc.Message());
            MASSERT(0);
        }
    }

    if (m_Params->ParamPresent("-check_display_underflow") > 0)
    {
        UINT32 i;

        for (i = 0; i < LW_PDISP_RG_UNDERFLOW__SIZE_1; ++i)
        {
            UINT32 value = m_pSubDev->RegRd32(LW_PDISP_RG_UNDERFLOW(i));
            value = FLD_SET_DRF(_PDISP_RG, _UNDERFLOW, _ENABLE, _ENABLE, value);
            m_pGpuRes->RegWr32(LW_PDISP_RG_UNDERFLOW(i), value, m_subdev);
        }
    }

    if (m_pPerfmonBuffer && (!m_pPerfmonBuffer->IsModeCAlloc()))
    {
        PMStreamCapture(HOLD_TEMPORAL_CAPTURE, pLwRm);
    }
    PMRegInit(pLwRm);
    PMRegStart(channel, subchannel);
    if (m_pPerfmonBuffer && (!m_pPerfmonBuffer->IsModeCAlloc()))
    {
        PMStreamCapture(START_TEMPORAL_CAPTURE, pLwRm);
    }
    channel->WaitForDmaPush();

    // Wait on all mode A PMs
    for (dom_iter = m_DomainInfoList.begin();
        dom_iter != m_DomainInfoList.end();
        ++dom_iter)
    {
        if (((*dom_iter)->GetMode() == PM_MODE_A) && (*dom_iter)->IsInUse())
        {
            // If we call in mode B this function will just return and clutter
            // the text output.
            WaitForState(*dom_iter, LW_PPM_CONTROL_STATE_IDLE, NOTEQUAL, 0);
        }
    }

    if (!m_PmReportInfo.UseTraceTrigger())
    {
        channel->MethodWriteRC(subchannel, LW9097_WAIT_FOR_IDLE, 0);
        channel->MethodWriteRC(subchannel, m_TriggerMethod, 1);

        PMInfo("PerformanceMonitor::Start: trigger (0x%.8x) written, will wait for idle.\n",
            m_TriggerMethod);
    }

    if (gXML)
    {
        lwrrent_time = gXML->GetRuntimeTimeInMilliSeconds();

        // Dump surface info (bug 218345)

        for (const auto psurf : s_RenderSurfaces)
        {
            char temp_buf[64];

            if (!psurf || (psurf->GetSize() == 0))
            {
                continue;
            }

            XD->XMLStartStdInline("<MODS_SURFACE");
            XD->outputAttribute("name", psurf->GetName().c_str());
            XD->outputAttribute("hMem", psurf->GetMemHandle());
            XD->outputAttribute("size", psurf->GetSize());
            XD->outputAttribute("width", psurf->GetWidth());
            XD->outputAttribute("height", psurf->GetHeight());

            if (psurf->GetLayout() == Surface2D::BlockLinear)
            {
                sprintf(temp_buf, "%dx%dx%d", 1 << psurf->GetLogBlockWidth(),
                    1 << psurf->GetLogBlockHeight(),
                    1 << psurf->GetLogBlockDepth());
            }
            else
            {
                sprintf(temp_buf, "pitch");
            }

            XD->outputAttribute("block", temp_buf);

#ifdef LW_VERIF_FEATURES
            LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
            params.memOffset = 0;

            RC rc = pLwRm->Control(psurf->GetMemHandle(),
                LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                &params, sizeof(params));

            if (rc != OK)
            {
                ErrPrintf("%s\n", rc.Message());
                MASSERT(0);
            }

            XD->outputAttribute("compress", (params.comprFormat ? "yes" : "no"));
#endif
            XD->XMLEndLwrrent();
        }

        UINT32 region;
        UINT32 size;
        UINT32 start;
        UINT32 status;
        SmcEngine *pSmcEngine = channel->GetSmcEngine();

        for (UINT32 zlwll_id : s_ZLwllIds)
        {
            RegHal& regHal = m_pGpuRes->GetRegHal(m_pSubDev, channel->GetLwRmPtr(), pSmcEngine);
            region = regHal.Read32(MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCREGION, zlwll_id);
            size   = regHal.Read32(MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSIZE, zlwll_id);
            start  = regHal.Read32(MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTART, zlwll_id);
            status = regHal.Read32(MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS, zlwll_id);

            char func_name[16];
            if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_SFUNC_NEVER))
            {
                sprintf(func_name, "NEVER");
            }
            else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_SFUNC_LESS))
            {
                sprintf(func_name, "LESS");
            }
            else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_SFUNC_EQUAL))
            {
                sprintf(func_name, "EQUAL");
            }
            else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_SFUNC_LEQUAL))
            {
                sprintf(func_name, "LEQUAL");
            }
            else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_SFUNC_GREATER))
            {
                sprintf(func_name, "GREATER");
            }
            else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_SFUNC_NOTEQUAL))
            {
                sprintf(func_name, "NOTEQUAL");
            }
            else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_SFUNC_GEQUAL))
            {
                sprintf(func_name, "GEQUAL");
            }
            else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_SFUNC_ALWAYS))
            {
                sprintf(func_name, "ALWAYS");
            }
            else
            {
                MASSERT(!"Unsupported value in LW_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_SFUNC");
            }

            char ramformat[16];
            if (regHal.TestField(region, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCREGION_RAMFORMAT_LOW_RES_Z))
            {
                sprintf(ramformat, "LOW_RES_Z");
            }
            else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCREGION_RAMFORMAT_LOW_RES_Z_2X4))
            {
                sprintf(ramformat, "LOW_RES_Z_2X4");
            }
            else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCREGION_RAMFORMAT_HIGH_RES_Z))
            {
                sprintf(ramformat, "HIGH_RES_Z");
            }
            else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCREGION_RAMFORMAT_LOW_RES_ZS))
            {
                sprintf(ramformat, "LOW_RES_ZS");
            }
            else
            {
                MASSERT(!"Unsupported value in LW_PGRAPH_PRI_GPCS_ZLWLL_ZCREGION_RAMFORMAT");
            }

            char zformat[8];
            if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_ZFORMAT_MSB))
            {
                sprintf(zformat, "MSB");
            }
            else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_ZFORMAT_FP))
            {
                sprintf(zformat, "FP");
            }
            else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_ZFORMAT_ZTRICK))
            {
                sprintf(zformat, "ZTRICK");
            }
            else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_ZFORMAT_ZF32_1))
            {
                sprintf(zformat, "ZF32_1");
            }
            else
            {
                MASSERT(!"Unsupported value in LW_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_ZFORMAT");
            }

            XD->XMLStartStdInline("<MODS_ZLWLL");
            XD->outputAttribute("regionID", zlwll_id);
            XD->outputAttribute("ramFormat", ramformat);

            XD->outputAttribute("start",
                regHal.GetField(start, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTART_ADDR)*
                regHal.LookupValue(MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTART_ADDR__MULTIPLE));

            XD->outputAttribute("width",
                regHal.GetField(size, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSIZE_WIDTH)*
                regHal.LookupValue(MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSIZE_WIDTH__MULTIPLE));

            XD->outputAttribute("height",
                regHal.GetField(size, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSIZE_HEIGHT)*
                regHal.LookupValue(MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSIZE_HEIGHT__MULTIPLE));

            XD->outputAttribute("direction",
                (regHal.TestField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_ZDIR_LESS) ? 
                    "LESS" : "GREATER"));

            XD->outputAttribute("zFormat", zformat);
            XD->outputAttribute("sFunc", func_name);

            XD->outputAttribute("sRef",
                regHal.GetField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_SREF));

            XD->outputAttribute("sMask",
                regHal.GetField(status, MODS_PGRAPH_PRI_GPCS_ZLWLL_ZCSTATUS_SMASK));

            XD->XMLEndLwrrent();
        }
    }

    PMInfo("PerformanceMonitor::Start completed (now=%d)\n", lwrrent_time);
}

void PerformanceMonitor::End(LWGpuChannel *channel, UINT32 subchannel,
    UINT32 class_to_use)
{
    long lwrrent_time  = 0;

    if (!gXML)
    {
        PMInfo("PerformanceMonitor::End : All 'now' values will be 0 because gXML is NULL.\n");
    }

    if (gXML)
    {
        lwrrent_time = gXML->GetRuntimeTimeInMilliSeconds();
    }

    PMInfo("PerformanceMonitor::End called now=%d, channel = %d, subchannel = %d\n",
        lwrrent_time, channel->ChannelNum(), subchannel);

    PMExitPoll(channel, subchannel);

    if (m_pPerfmonBuffer && (!m_pPerfmonBuffer->IsModeCAlloc()))
    {
        PMStreamCapture(HOLD_TEMPORAL_CAPTURE, channel->GetLwRmPtr());
    }

    PMInfo("PerformanceMonitor::End calling 'Stop'\n");

    Stop(channel, subchannel);

    PMInfo("PerformanceMonitor::End finished 'Stop' now=%d\n", lwrrent_time);
}

void PerformanceMonitor::HandleInterrupt()
{
    InfoPrintf("PerformanceMonitor::HandleInterrupt() : Receiving interrupt.\n");

    if (m_PmReportInfo.WaitForInterrupt())
    {
        InfoPrintf("Initializing PM registers.\n");

        // Write PM registers, including TRIG0_OP and enable PM_TRIGGER
        PMRegInit(LwRmPtr().Get());

        // Clear this part of the report - we do not wait for more than one interrupt.
        m_PmReportInfo.ClearWaitForInterrupt();
    }
    else
    {
        WarnPrintf("Not expecting interrupt!  Ignoring...\n");
    }
}

int PerformanceMonitor::WaitForState(DomainInfo *domain, UINT32 state,
    PM_OP op, int max_idle_count)
{
    // retry times to read PM state until it become specified value, default as 5
    if (max_idle_count == 0)
        max_idle_count = 5;

    MASSERT(domain);

    PMInfo("PerformanceMonitor::WaitForState(state = %s(%d), op = %s, max_idle_count = 0x%x) [%s]\n",
        s_state_to_string[domain->GetMode()][state], 
        state,
        (op == EQUAL) ? "EQUAL" : "NOTEQUAL", 
        max_idle_count,
        domain->GetName());

    // wait for performance monitor state machine to be in state <st>
    // (if <wait_type> is EQUAL) or NOT in state <st> if <wait_type>
    // is PM_WAIT_FOR_NOTEQUAL
    //
    // <st> is one of LW_PPM_CONTROL_STATE_* or LW_PPM_CONTROL_SHADOW_STATE_*
    // from dev_pm.ref
    //
    // return 1 if the wait condition was met before the max number of read
    // retries else return 0
    //
    // We will not support this operation at all in mode B even though the
    // code below supports it

    // Mode C will not be supported also
    if (domain->GetMode() != PM_MODE_A)
    {
        InfoPrintf("Skipping since pm is not in mode A\n");
        return 1;
    }

    if (!domain->IsInUse())
    {
        PMInfo("Skipping since pm is not in use.\n");
        return 1;
    }

    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        InfoPrintf("Skipping since pm not supported on fmodel\n");
        return 1;
    }

    int rd_count = 0;

    UINT32 pmstate;

    do
    {
        pmstate = ReadPmState(domain);

        PMInfo("pmstate: %x (eq %d, req 0x%x, rd_count %d)\n",
            pmstate, op == EQUAL, state, rd_count);

        ++rd_count;

    } while (((op == EQUAL) ? (pmstate != state) : (pmstate == state)) && (rd_count  < max_idle_count));

    PMInfo("rd_count : %d\n", rd_count);

    if (rd_count >= max_idle_count)
    {
        // some experiments won't return to idle so this is NOT an error condition
        PMInfo("PerformanceMonitor::WaitForState: max_idle_count (%d) reached (state = %s(%d), op = %s) final state: %s(%d)\n",
            max_idle_count,
            s_state_to_string[domain->GetMode()][state], 
            state,
            (op == EQUAL) ? "EQUAL":"NOTEQUAL",
            s_state_to_string[domain->GetMode()][pmstate], pmstate);

        return 0;
    }
    else
    {
        PMInfo("PerformanceMonitor::WaitForState: success\n");
        return 1;
    }
}

int PerformanceMonitor::Stop(LWGpuChannel *channel, UINT32 subchannel)
{
    long now = 0;
    long lwrrent_batch = LWGpuResource::GetTestDirectory()->Get_test_id_being_setup();
    RegValInfoList regValInfoList;
    SmcEngine *pSmcEngine;
    UINT32 value;

    PMInfo("PerformanceMonitor::Stop Begin RegWriteInfoList\n");

    if (gXML != NULL)
    {
        now = gXML->GetRuntimeTimeInMilliSeconds();

        XD->XMLStartStdInline("<MDIAG_PM");
        XD->outputAttribute("now",now);
        XD->outputAttribute("lwrrent_batch", lwrrent_batch);
    }

    auto& stopRegWriteGroup = m_RegWriteListGroup.GetRegWriteInfo(STOP);

    for (const auto& regWriteSubGroup : stopRegWriteGroup)
    {
        PMInfo("PM: GroupIndex: %d\n", regWriteSubGroup.first);

        for (const auto& regWrite : regWriteSubGroup.second)
        {
            if (regWrite.m_write == PM_FILE_COMMAND_REG_READ)
            {
                regValInfoList = RegRead(regWrite.m_address,
                                 regWrite.m_name,
                                 regWrite.m_instanceType,
                                 regWrite.m_instance,
                                 regWrite.m_sysPipe);

                if (gXML != NULL)
                {
                    for (auto& regValInfo : regValInfoList)
                    {
                        tie(pSmcEngine, value) = regValInfo;
                        string smcInfoStr = pSmcEngine ? pSmcEngine->GetInfoStr() : "";
                        string name = regWrite.m_name + " " + smcInfoStr;
                        XD->outputAttribute(name.c_str(), value);
                    }
                }
            }
            if (regWrite.m_write == PM_FILE_COMMAND_REG_READ_ASSIGN)
            {
                auto assignList = RegRead(regWrite.m_address,
                                          regWrite.m_name,
                                          regWrite.m_instanceType,
                                          regWrite.m_instance,
                                          regWrite.m_assignInfoList,
                                          regWrite.m_sysPipe);

                if (gXML != NULL)
                {
                    for (const auto& assign : assignList)
                    {
                        for (auto regValInfo : assign.second)
                        {
                            tie(pSmcEngine, value) = regValInfo;
                            string smcInfoStr = pSmcEngine ? pSmcEngine->GetInfoStr() : "";
                            string name = assign.first + " " + smcInfoStr;
                            XD->outputAttribute(name.c_str(), value);
                        }
                    }
                }
            }
            else if (regWrite.m_write == PM_FILE_COMMAND_REG_POLL)
            {
                RegComp(regWrite.m_address,
                        regWrite.m_name,
                        regWrite.m_mask,
                        regWrite.m_value,
                        regWrite.m_instanceType,
                        regWrite.m_instance,
                        regWrite.m_sysPipe);
            }
            else if (regWrite.m_write == PM_FILE_COMMAND_REG_WRITE)
            {
                RegWrite(regWrite.m_address,
                        regWrite.m_name,
                        regWrite.m_mask,
                        regWrite.m_value,
                        regWrite.m_instanceType,
                        regWrite.m_instance,
                        regWrite.m_sysPipe);
            }
            else
            {
                PerformanceMonitor::ExtendedCommands(regWrite, channel->GetLwRmPtr());
            }
        }
    }

    PMInfo("PerformanceMonitor::Stop Finish RegWriteInfoList\n");

    if (gXML != NULL)
    {
        XD->XMLEndLwrrent();
    }

    if (!channel->WaitForIdle())
    {
        ErrPrintf("PerformanceMonitor::Stop, WaitForIdle call failed\n");
        return 0;
    }

    if (!channel->WaitGrIdle())
    {
        ErrPrintf("PerformanceMonitor::Stop, WaitGrIdle call failed\n");
        return 0;
    }

    PMInfo("PerformanceMonitor::Stop(now=%d) : stopping PM\n", now);

    int return_code = 1;

    DomainInfoList::const_iterator dom_iter;

    for (dom_iter = m_DomainInfoList.begin();
        dom_iter != m_DomainInfoList.end();
        ++dom_iter)
    {
        if ((*dom_iter)->GetMode() == PM_MODE_A)
        {
            return_code = (return_code && WaitForState(*dom_iter, LW_PPM_CONTROL_STATE_IDLE, EQUAL, 0));
        }
    }

#define DECODE_PM_REG(_v, _reg) {_v = m_pSubDev->RegRd32(_reg(lwr_dom->GetIndex())); PMInfo("\t%s = 0x%x\n", #_reg, _v); }

    if (m_Params->ParamPresent("-check_display_underflow") > 0)
    {
        UINT32 i;

        for (i = 0; i < LW_PDISP_RG_UNDERFLOW__SIZE_1; ++i)
        {
            UINT32 value = m_pSubDev->RegRd32(LW_PDISP_RG_UNDERFLOW(i));

            if (FLD_TEST_DRF(_PDISP_RG, _UNDERFLOW, _UNDERFLOWED, _YES, value))
            {
                ErrPrintf("PerformanceMonitor::Stop - display underflow detected.\n");
                return 0;
            }
        }
    }

    return return_code;
}

void PerformanceMonitor::PMExelwteCommand(LWGpuChannel *channel, UINT32 subchannel,
    const PMCommandInfo &command)
{
    PMInfo("PerformanceMonitor::PMExelwteCommand()\n");

    switch(command.m_address)
    {
    case pm_wait_for_idle:
        PMInfo("PM cmd: wait_for_idle\n");

        if (m_PmReportInfo.WaitForInterrupt())
        {
            PMInfo("PM cmd: wait_for_idle SKIPPED BECAUSE OF PM_REPORT_WAIT_FOR_INTERRUPT\n");
        }
        else
        {
            if (!channel->WaitForDmaPush())
            {
                ErrPrintf("PerformanceMonitor::PMExelwteCommand, WaitForDmaPush call failed\n");
                return;
            }

            // XXX optionally delay useconds depending on -pm_usleep {useconds} switch
            //Sys::PmDelay();
            if (!channel->WaitGrIdle())
            {
                ErrPrintf("PerformanceMonitor::PMExelwteCommand, WaitGrIdle call failed\n");
                return;
            }
        }
        break;

    case pm_trigger:
        if (m_PmReportInfo.UseTraceTrigger())
        {
            PMInfo("PM cmd: trigger METHOD WRITE SKIPPED BECAUSE OF PM_REPORT_USE_TRACE_TRIGGER\n");
        }
        else
        {
            PMInfo("PM cmd: trigger\n");

            PMInfo("PM debug: DmaPushFill\n");

            channel->MethodWriteRC(subchannel, LW9097_WAIT_FOR_IDLE, 0);
            channel->MethodWriteRC(subchannel, m_TriggerMethod, 1);
            channel->WaitForDmaPush();

            PMInfo("PerformanceMonitor::PMExelwteCommand: trigger (0x%x) written\n",
                m_TriggerMethod);
        }

        PMInfo("PM debug: Done!@#\n");
        break;

    default:
        ErrPrintf("PM cmd: Unrecognized init command %i! (internal error)\n", command.m_command);
        break;
    }
}

void PerformanceMonitor::PMExitPoll(LWGpuChannel *channel, UINT32 subchannel)
{
    DomainInfoList::const_iterator dom_iter;
    PMInfo("PerformanceMonitor::PMExitPoll()\n");

    UINT32 i;

    for (i = 0; i < m_PmInitCommands.size(); ++i)
    {
        switch (m_PmInitCommands[i].m_command)
        {
        case pm_finalize:
            PMExelwteCommand(channel, subchannel, m_PmInitCommands[i]);
            break;

        default:
            break;
        }
    }

    if (m_PMDebugInfo)
    {
        for (dom_iter = m_DomainInfoList.begin();
            dom_iter != m_DomainInfoList.end();
            ++dom_iter)
        {
            ReportPmState(*dom_iter);
        }
    }
}

bool PerformanceMonitor::LoadPMFile(const string &filename)
{
    InfoPrintf("PerformanceMonitor::LoadPMFile: %s\n", filename.c_str());
    bool success = false;

    if (ProcessPMFile(filename))
    {
        success = true;
    }

    InfoPrintf("PerformanceMonitor::LoadPMFile finished\n");

    // The PM file must be processed correctly to continue.
    MASSERT(success);

    return success;
}

bool PerformanceMonitor::ProcessPMFile(const string &filename)
{
    InfoPrintf("PerformanceMonitor::ProcessPMFile\n");

    if (!ParsePMFile(filename))
    {
        ErrPrintf("Problem parsing experiment file %s\n", filename.c_str());
        return false;
    }

    AddFinalizeInitCommand(pm_trigger, 0);
    AddFinalizeInitCommand(pm_wait_for_idle, 0);

    return true;
}

bool PerformanceMonitor::ParsePMFile(const string &filename)
{
    XMLWrapper xml_wrapper;
    XMLNode *root_node;

    DebugPrintf("PerformanceMonitor::ParsePMFile begin\n");

    RC rc = xml_wrapper.ParseXMLFileToTree(filename.c_str(), &root_node,
        true);

    if (rc == OK)
    {
        if (!ProcessPMFileRoot(root_node))
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

bool PerformanceMonitor::ProcessPMFileRoot(XMLNode *root_node)
{
    string *value;

    DebugPrintf("PerformanceMonitor::ProcessPMFileRoot begin\n");

    if (*root_node->Name() != "ModsPMInput")
    {
        ErrPrintf("PerformanceMonitor::ParsePMFileRoot root node in PM file no ModsPMInput\n");
        return false;
    }

    value = GetNodeAttribute(root_node, "Version", true);
    if (!value) return false;

    value = GetNodeAttribute(root_node, "ExperimentFile", true);
    if (!value) return false;

    XMLNodeList::iterator child_iter;

    for (child_iter = root_node->Children()->begin();
        child_iter != root_node->Children()->end();
        ++child_iter)
    {
        XMLNode *child_node = *child_iter;

        if (*child_node->Name() == "DomainList")
        {
            if (!ProcessDomainListNode(child_node))
            {
                return false;
            }
        }
        else if (*child_node->Name() == "RegWriteList")
        {
            if (!ProcessRegWriteListNode(child_node))
            {
                return false;
            }
        }
        else if (*child_node->Name() == "ModeCAlloc")
        {
            ProcessModeCAllocNode(child_node);
        }
        else if (*child_node->Name() == "CreatePerfmonBuffer")
        {
            ProcessCreatePerfmonBufferNode(child_node);
        }
        else if (*child_node->Name() == "Info")
        {
            DebugPrintf("PerformanceMonitor::ProcessPMFileRoot ignores node <Info> and its descendant nodes\n");
        }
        else
        {
            ErrPrintf("PerformanceMonitor::ParsePMFileRoot unrecognized element %s\n",
                child_node->Name()->c_str());

            return false;
        }
    }

    return true;
}

string *PerformanceMonitor::GetNodeAttribute(XMLNode *node,
    const char *attribute_name, bool required)
{
    string *value = 0;
    XMLAttributeMap::iterator map_iter;

    map_iter = node->AttributeMap()->find(attribute_name);

    if (map_iter != node->AttributeMap()->end())
    {
        value = &((*map_iter).second);

        DebugPrintf("PerformanceMonitor::GetNodeAttribute %s = %s\n", attribute_name, value->c_str());
    }
    else if (required)
    {
        ErrPrintf("PerformanceMonitor::GetNodeAttribute missing %s attribute in %s element\n",
            attribute_name, node->Name()->c_str());
    }

    return value;
}

bool PerformanceMonitor::ProcessDomainListNode(XMLNode *node)
{
    XMLNodeList::iterator child_iter;

    for (child_iter = node->Children()->begin();
        child_iter != node->Children()->end();
        ++child_iter)
    {
        XMLNode *child_node = *child_iter;

        if (*child_node->Name() == "Domain")
        {
            if (!ProcessDomainNode(child_node))
            {
                return false;
            }
        }
        else
        {
            ErrPrintf("PerformanceMonitor::ProcessDomainListNode unrecognized element %s\n",
                child_node->Name()->c_str());

            return false;
        }
    }

    return true;
}

bool PerformanceMonitor::ProcessDomainNode(XMLNode *node)
{
    string *value;
    int index = static_cast<int>(m_DomainInfoList.size());

    value = GetNodeAttribute(node, "Name", true);
    if (!value) return false;

    DomainInfo *info = new DomainInfo(index, value->c_str(), this);

    DebugPrintf("PerformanceMonitor::ProcessDomainNode adding domain %s\n",
        info->GetName());

    m_DomainInfoList.push_back(info);

    value = GetNodeAttribute(node, "Mode", false);

    if (!value)
    {
        // Domain not in use.
    }
    else if (*value == "A")
    {
        info->SetMode(PM_MODE_A);
        info->SetInUse(true);
    }
    else if (*value == "B")
    {
        info->SetMode(PM_MODE_B);
        info->SetInUse(true);
    }
    else if (*value == "C")
    {
        info->SetMode(PM_MODE_C);
        info->SetInUse(true);
    }
    else if (*value == "E")
    {
        info->SetMode(PM_MODE_E);
        info->SetInUse(true);
    }
    else if (*value == "SMPC")
    {
        info->SetMode(PM_MODE_SMPC);
        info->SetInUse(true);
    }
    else if (*value == "CAU")
    {
        info->SetMode(PM_MODE_CAU);
        info->SetInUse(true);
    }
    else
    {
        ErrPrintf("PerformanceMonitor::ProcessDomainNode unrecognized mode %s\n",
            value->c_str());

        return false;
    }

    XMLNodeList::iterator child_iter;

    for (child_iter = node->Children()->begin();
        child_iter != node->Children()->end();
        ++child_iter)
    {
        XMLNode *child_node = *child_iter;

        if (*child_node->Name() == "SignalList")
        {
            ProcessSignalListNode(child_node);
        }
        else
        {
            ErrPrintf("PerformanceMonitor::ProcessDomainNode unrecognized element %s\n",
                child_node->Name()->c_str());

            return false;
        }
    }

    return true;
}

bool PerformanceMonitor::ProcessSignalListNode(XMLNode *node)
{
    XMLNodeList::iterator child_iter;

    for (child_iter = node->Children()->begin();
        child_iter != node->Children()->end();
        ++child_iter)
    {
        XMLNode *child_node = *child_iter;

        if (*child_node->Name() == "Signal")
        {
            ProcessSignalNode(child_node);
        }
        else
        {
            ErrPrintf("PerformanceMonitor::ProcessSignaListNode unrecognized element %s\n",
                child_node->Name()->c_str());

            return false;
        }
    }

    return true;
}

bool PerformanceMonitor::ProcessSignalNode(XMLNode *node)
{
    string *value;
    XMLNodeList::iterator child_iter;

    value = GetNodeAttribute(node, "Name", true);
    if (!value) return false;

    value = GetNodeAttribute(node, "BusIndex", true);
    if (!value) return false;

    for (child_iter = node->Children()->begin();
        child_iter != node->Children()->end();
        ++child_iter)
    {
        XMLNode *child_node = *child_iter;

        ErrPrintf("PerformanceMonitor::ProcessSignalNode illegal child element %s\n",
            child_node->Name()->c_str());

        return false;
    }

    return true;
}

bool PerformanceMonitor::ProcessModeCAllocNode(XMLNode *node)
{
    // by convention, MODS will multiply by 128
    const UINT32 multiplyFactor = 128;
    return ProcessCreatePerfmonBufferNode(node, multiplyFactor, true);
}

bool PerformanceMonitor::ProcessCreatePerfmonBufferNode(XMLNode *node)
{
    // PmlSpilter will specify the whole size
    return ProcessCreatePerfmonBufferNode(node, 1 , false);
}

bool PerformanceMonitor::ProcessCreatePerfmonBufferNode(XMLNode *node, UINT32 multiplyFactor, bool isModeCAlloc)
{
    string *value;
    XMLNodeList::iterator child_iter;

    const char* nodeName = isModeCAlloc ? "ModeCAlloc" : "CreatePerfmonBuffer";

    UINT32 size = 0;
    value = GetNodeAttribute(node, "Size", false);
    if (value)
    {
        size = static_cast<UINT32>(strtoul(value->c_str(), 0, 0));

        if (size == 0)
        {
            ErrPrintf("%s size must be positive\n", nodeName);
            return false;
        }
    }
    else
    {
        ErrPrintf("%s Size must be specified\n", nodeName);
        return false;
    }

    Memory::Location aperture = Memory::Coherent;
    value = GetNodeAttribute(node, "Aperture", false);
    if (value)
    {
        if (*value == "sysmem")
        {
            aperture = Memory::Coherent;
        }
        else if (*value == "vidmem")
        {
            aperture = Memory::Fb;
        }
        else
        {
            ErrPrintf("PerformanceMonitor::ProcessCreatePerfmonBufferNode Aperture %s is not valid. It should be either sysmem or vidmem\n", value->c_str());
            return false;
        }

    }

    m_pPerfmonBuffer = make_unique<PerfmonBuffer>(size * multiplyFactor, aperture, m_pGpuRes, isModeCAlloc);

    for (child_iter = node->Children()->begin();
        child_iter != node->Children()->end();
        ++child_iter)
    {
        XMLNode *child_node = *child_iter;

        ErrPrintf("PerformanceMonitor::ProcessCreatePerfmonBufferNode illegal child element %s\n",
            child_node->Name()->c_str());

        return false;
    }

    return true;
}

bool PerformanceMonitor::ProcessRegWriteListNode(XMLNode *node)
{
    string *value;
    XMLNodeList::iterator child_iter;
    RegWriteGroup group;

    value = GetNodeAttribute(node, "Description", true);
    if (!value) return false;

    value = GetNodeAttribute(node, "Group", true);
    if (!value) return false;

    if (*value == "Init")
    {
        group = INIT;
    }
    else if (*value == "Stop")
    {
        group = STOP;
    }
    else if (*value == "HoldTemporalCapture")
    {
        group = HOLD_TEMPORAL_CAPTURE;
    }
    else if (*value == "StartTemporalCapture")
    {
        group = START_TEMPORAL_CAPTURE;
    }
    else
    {
        ErrPrintf("PerformanceMonitor::ProcessRegWriteListNode unrecognized group %s\n", value->c_str());
        return false;
    }

    value = GetNodeAttribute(node, "GroupIndex", false);

    INT32 groupIndex = -1;

    if (value != nullptr)
    {
        groupIndex = static_cast<UINT32>(strtoul(value->c_str(), 0, 0));

        const auto& regWriteList = m_RegWriteListGroup.GetRegWriteInfo(group);
        if (regWriteList.count(groupIndex))
        {
            ErrPrintf("PerformanceMonitor::ProcessRegWriteListNode duplicated GroupIndex %d in Group %s\n",
                      groupIndex, GroupEnumToStr(group));
            MASSERT(!"Illegal pm file syntax");
        }
    }
    else
    {
        const auto& regWriteList = m_RegWriteListGroup.GetRegWriteInfo(group);
        for (const auto& regWriteListSubGroup : regWriteList)
        {
            if (regWriteListSubGroup.first != groupIndex)
            {
                ErrPrintf("PerformanceMonitor::ProcessRegWriteListNode not all the Group %s specify GroupIndex\n",
                           groupIndex);
                MASSERT(!"Illegal pm file syntax");
            }
        }
    }

    UINT32 null_address = 0;
    UINT32 null_mask = 0;
    UINT32 null_value = 0;
    UINT32 null_instance = 0;
    PerfmonInstance null_instance_type = NONE;

    for (child_iter = node->Children()->begin();
        child_iter != node->Children()->end();
        ++child_iter)
    {
        XMLNode *child_node = *child_iter;

        if (*child_node->Name() == "RegWrite")
        {
            PMInfo("PerformanceMonitor::ProcessRegWriteListNode call ProcessRegWriteNode\n");
            ProcessRegWriteNode(child_node, group, groupIndex);

        }
        else if (*child_node->Name() == "RegRead")
        {
            PMInfo("PerformanceMonitor::ProcessRegWriteListNode call ProcessRegReadNode\n");
            ProcessRegReadNode(child_node, group, groupIndex);

        }
        else if (*child_node->Name() == "BindBuffer")
        {
            PMInfo("PerformanceMonitor::ProcessRegWriteListNode call BindBuffer\n");
            string bind_name = "";
            UINT32 bind_write = PM_FILE_COMMAND_BIND_MODEC_BUFF;
            m_RegWriteListGroup.AddRegWrite(group, groupIndex, null_address, null_mask, null_value, null_instance_type, null_instance, bind_write, bind_name);
        }
        else if (*child_node->Name() == "DumpBuffer")
        {
            PMInfo("PerformanceMonitor::ProcessRegWriteListNode call DumpBuffer\n");
            string dump_name = "";
            UINT32 dump_write = PM_FILE_COMMAND_DUMP_MODEC_BUFF;
            m_RegWriteListGroup.AddRegWrite(group, groupIndex, null_address, null_mask, null_value, null_instance_type, null_instance, dump_write, dump_name);
        }
        else if ((*child_node->Name() == "SetBuffer") || (*child_node->Name() == "PollBuffer"))
        {
            string *set_value;
            string set_name = "";
            UINT32 set_write;
            UINT32 set_mask;

            if(*child_node->Name() == "SetBuffer")
            {
                PMInfo("PerformanceMonitor::ProcessRegWriteListNode call SetBuffer\n");
                set_write = PM_FILE_COMMAND_SET_MODEC_BUFF;
                set_mask = 0;
            }
            else
            {
                PMInfo("PerformanceMonitor::ProcessRegWriteListNode call PollBuffer\n");
                set_write = PM_FILE_COMMAND_POLL_MODEC_BUFF;

                set_value = GetNodeAttribute(child_node, "Eq", true);
                if (!set_value) return false;
                set_mask = static_cast<UINT32>(strtoul(set_value->c_str(), 0, 0));
            }

            set_value = GetNodeAttribute(child_node, "Address", true);
            if (!set_value) return false;
            UINT32 set_address = static_cast<UINT32>(strtoul(set_value->c_str(), 0, 0));

            set_value = GetNodeAttribute(child_node, "Value", true);
            if (!set_value) return false;
            UINT32 set_mem_val = static_cast<UINT32>(strtoul(set_value->c_str(), 0, 0));

            m_RegWriteListGroup.AddRegWrite(group, groupIndex, set_address, set_mask, set_mem_val, null_instance_type, null_instance, set_write, set_name);
        }
        else if (*child_node->Name() == "RunScript")
        {
            PMInfo("PerformanceMonitor::ProcessRegWriteListNode call RunScript\n");

            string * script_cmd = GetNodeAttribute(child_node, "Name", true);
            if (!script_cmd) return false;
            UINT32 run_script = PM_FILE_COMMAND_RUN_SCRIPT;
            m_RegWriteListGroup.AddRegWrite(group, groupIndex, null_address, null_mask, null_value, null_instance_type, null_instance, run_script, *script_cmd);
        }
        else if (*child_node->Name() == "OpenFile")
        {
            PMInfo("PerformanceMonitor::ProcessRegWriteListNode call OpenFile\n");

            string * filename = GetNodeAttribute(child_node, "Name", true);
            if (!filename) return false;
            UINT32 open_file = PM_FILE_COMMAND_OPEN_FILE;
            m_RegWriteListGroup.AddRegWrite(group, groupIndex, null_address, null_mask, null_value, null_instance_type, null_instance, open_file, *filename);
        }
        else if (*child_node->Name() == "CloseFile")
        {
            PMInfo("PerformanceMonitor::ProcessRegWriteListNode call CloseFile\n");
            string * filename = GetNodeAttribute(child_node, "Name", true);
            UINT32  close_file = PM_FILE_COMMAND_CLOSE_FILE;
            m_RegWriteListGroup.AddRegWrite(group, groupIndex, null_address, null_mask, null_value, null_instance_type, null_instance, close_file, *filename);
        }
        else
        {
            ErrPrintf("PerformanceMonitor::ProcessRegWriteListNode unrecognized element %s\n",
                child_node->Name()->c_str());

            return false;
        }
    }

    return true;
}

bool PerformanceMonitor::ProcessRegWriteNode(XMLNode *node, RegWriteGroup group, INT32 groupIndex)
{
    string *value;
    string name = "noname";
    string sysPipe;
    UINT32 write = 1;
    XMLNodeList::iterator child_iter;
    UINT32 address;
    UINT32 valueToWrite;
    PerfmonInstance instanceType = NONE;
    UINT32 instance = 0;
    UINT32 mask = 0;

    PMInfo("PerformanceMonitor::ProcessRegWriteNode begin\n");

    // The Name attribute is required for debugging, so it's an error for it
    // not to exist, but it isn't needed for processing.
    value = GetNodeAttribute(node, "Name", true);
    if (!value) return false;

    name = *value;

    value = GetNodeAttribute(node, "Address", true);
    if (!value) return false;

    address = static_cast<UINT32>(strtoul(value->c_str(), 0, 0));

    value = GetNodeAttribute(node, "Mask", true);
    if (!value) return false;

    mask = static_cast<UINT32>(strtoul(value->c_str(), 0, 0));

    value = GetNodeAttribute(node, "Value", true);
    if (!value) return false;

    valueToWrite = static_cast<UINT32>(strtoul(value->c_str(), 0, 0));

    PMInfo("PerformanceMonitor::ProcessRegWriteNode Address: %08x %08x\n", address,valueToWrite);

    value = GetNodeAttribute(node, "InstanceType", false);

    if (value != 0)
    {
        if (*value == "TPC")
        {
            instanceType = TPC;
        }
        else if (*value == "ROP")
        {
            instanceType = ROP;
        }
        else if (*value == "FBP")
        {
            instanceType = FBP;
        }
        else if (*value == "NONE")
        {
            instanceType = NONE;
        }
        else
        {
            ErrPrintf("PerformanceMonitor::ProcessRegWriteNode unrecognized InstanceType %s\n",
                value->c_str());

            return false;
        }
    }

    value = GetNodeAttribute(node, "Instance", false);

    if (value != 0)
    {
        instance = static_cast<UINT32>(strtoul(value->c_str(), 0, 0));
    }

    value = GetNodeAttribute(node, "WriteOp", false);
    if (value != 0)
    {
        write = static_cast<UINT32>(strtoul(value->c_str(), 0, 0));
    }

    value = GetNodeAttribute(node, "SysPipe", false);

    if (value != 0)
    {
        sysPipe = *value;
    }
    else
    {
        if (m_pGpuRes->IsSMCEnabled() && m_pGpuRes->IsPgraphReg(address))
        {
            ErrPrintf("PerformanceMonitor::ProcessRegWriteNode 'SysPipe' is not specified for PGRAPH register: %08x \n", address);
            return false;
        }
    }

    m_RegWriteListGroup.AddRegWrite(group, groupIndex, address, mask, valueToWrite, instanceType, instance, write, name, sysPipe);

    for (child_iter = node->Children()->begin();
        child_iter != node->Children()->end();
        ++child_iter)
    {
        XMLNode *child_node = *child_iter;

        ErrPrintf("PerformanceMonitor::ProcessRegWriteNode illegal child element %s\n",
            child_node->Name()->c_str());

        return false;
    }

    return true;
}

bool PerformanceMonitor::ProcessRegReadNode(XMLNode *node, RegWriteGroup group, INT32 groupIndex)
{
    string *value = nullptr;
    string sysPipe;
    XMLNodeList::iterator child_iter;
    UINT32 address;
    PerfmonInstance instanceType = NONE;
    UINT32 instance = 0;
    const UINT32 mask = 0xffffffff;
    const UINT32 valueToWrite = 0;

    PMInfo("PerformanceMonitor::ProcessRegReadNode begin\n");

    value = GetNodeAttribute(node, "Address", true);
    if (!value) return false;

    address = static_cast<UINT32>(strtoul(value->c_str(), 0, 0));

    PMInfo("PerformanceMonitor::ProcessRegReadNode Address: %08x\n", address);

    value = GetNodeAttribute(node, "InstanceType", false);

    if (value != 0)
    {
        if (*value == "TPC")
        {
            instanceType = TPC;
        }
        else if (*value == "ROP")
        {
            instanceType = ROP;
        }
        else if (*value == "FBP")
        {
            instanceType = FBP;
        }
        else if (*value == "NONE")
        {
            instanceType = NONE;
        }
        else
        {
            ErrPrintf("PerformanceMonitor::ProcessRegReadNode unrecognized InstanceType %s\n",
                value->c_str());

            return false;
        }
    }

    value = GetNodeAttribute(node, "Instance", false);

    if (value != 0)
    {
        instance = static_cast<UINT32>(strtoul(value->c_str(), 0, 0));
    }

    value = GetNodeAttribute(node, "SysPipe", false);

    if (value != 0)
    {
        sysPipe = *value;
    }
    else
    {
        if (m_pGpuRes->IsSMCEnabled() && m_pGpuRes->IsPgraphReg(address))
        {
            ErrPrintf("PerformanceMonitor::ProcessRegReadNode 'SysPipe' is not specified for PGRAPH register: %08x \n", address);
            return false;
        }
    }

    auto& regWrite = m_RegWriteListGroup.AddRegWrite(group, groupIndex, address,
                                                     mask, valueToWrite,
                                                     instanceType, instance,
                                                     PM_FILE_COMMAND_REG_READ_ASSIGN, "", sysPipe);
    for (child_iter = node->Children()->begin();
        child_iter != node->Children()->end();
        ++child_iter)
    {

        XMLNode *child_node = *child_iter;

        if (*child_node->Name() == "Assign")
        {
            if (!ProcessAssignNode(child_node, &regWrite))
            {
                return false;
            }
        }
        else
        {
            ErrPrintf("PerformanceMonitor::ProcessRegReadNode unrecognized element %s\n",
                child_node->Name()->c_str());

            return false;
        }
    }

    return true;
}

bool PerformanceMonitor::ProcessAssignNode(XMLNode *node, RegWriteInfo* pRegWriteInfo)
{
    string *value = nullptr;
    string name;
    XMLNodeList::iterator child_iter;
    UINT32 mask = 0xffffffff;
    UINT32 rightShift = 0;

    PMInfo("PerformanceMonitor::ProcessAssignNode begin\n");

    // The Name attribute is required for debugging, so it's an error for it
    // not to exist, but it isn't needed for processing.
    value = GetNodeAttribute(node, "Name", true);
    if (!value)
    {
        return false;
    }
    else
    {
        name = *value;
    }

    value = GetNodeAttribute(node, "Mask", false);
    if (value != 0)
    {
        mask = static_cast<UINT32>(strtoul(value->c_str(), 0, 0));
    }

    value = GetNodeAttribute(node, "RightShift", false);
    if (value != 0)
    {
        rightShift = static_cast<UINT32>(strtoul(value->c_str(), 0, 0));
    }

    PMInfo("PerformanceMonitor::ProcessAssignNode Name: %s Mask: %08x RightShift: %u\n",
            name.c_str(), mask, rightShift);

    pRegWriteInfo->m_assignInfoList.emplace_back(name, mask, rightShift);

    for (child_iter = node->Children()->begin();
        child_iter != node->Children()->end();
        ++child_iter)
    {
        XMLNode *child_node = *child_iter;

        ErrPrintf("PerformanceMonitor::ProcessAssignNode illegal child element %s\n",
                  child_node->Name()->c_str());

        return false;
    }

    return true;
}

void PerformanceMonitor::PMRegStart(LWGpuChannel *channel, UINT32 subchannel)
{
    PMInfo("PerformanceMonitor::PMRegStart() %d init_commands\n",
           m_PmInitCommands.size());

    UINT32 i;

    for (i = 0; i < m_PmInitCommands.size(); ++i)
    {
        switch (m_PmInitCommands[i].m_command)
        {
        case pm_init:
            PMExelwteCommand(channel, subchannel, m_PmInitCommands[i]);
            break;

        default:
            break;
        }
    }
}

void PerformanceMonitor::ExtendedCommands(const RegWriteInfo& regWrite, LwRm *pLwRm)
{
    if(regWrite.m_write == PM_FILE_COMMAND_BIND_MODEC_BUFF)
    {
        MASSERT(m_pPerfmonBuffer);
        m_pPerfmonBuffer->Setup(pLwRm);
    }
    else if (regWrite.m_write == PM_FILE_COMMAND_OPEN_FILE)
    {
        MASSERT(m_pPerfmonBuffer);
        if (!m_pPerfmonBuffer->OpenFile(regWrite.m_name))
        {
            MASSERT(!"PerformanceMonitor::ExtendedCommands PM_FILE_COMMAND_OPEN_FILE failed to open file for write!");
        }
        PMInfo("PerformanceMonitor::ExtendedCommands: open log file %s\n", regWrite.m_name.c_str());

    }
    else if (regWrite.m_write == PM_FILE_COMMAND_CLOSE_FILE)
    {
        MASSERT(m_pPerfmonBuffer);
        if(!m_pPerfmonBuffer->IsFileOpened())
        {
            MASSERT(!"PerformanceMonitor::ExtendedCommands PM_FILE_COMMAND_CLOSE_FILE failed to close file!");
        }
        m_pPerfmonBuffer->CloseFile();
        PMInfo("PerformanceMonitor::ExtendedCommands: close log file\n");

    }
    else if (regWrite.m_write == PM_FILE_COMMAND_SET_MODEC_BUFF)
    {
        MASSERT(m_pPerfmonBuffer);
        m_pPerfmonBuffer->Write(regWrite.m_address, regWrite.m_value);
        PMInfo("PerformanceMonitor::ExtendedCommands: write offset=%ld, data=0x%08x\n", regWrite.m_address, regWrite.m_value);
    }
    else if (regWrite.m_write == PM_FILE_COMMAND_DUMP_MODEC_BUFF)
    {
        MASSERT(m_pPerfmonBuffer);
        m_pPerfmonBuffer->DumpToFile();
    }
    else if (regWrite.m_write == PM_FILE_COMMAND_RUN_SCRIPT)
    {
        PMInfo("PerformanceMonitor::ExtendedCommands: Running: %s\n", regWrite.m_name.c_str());

        if(0 != system(regWrite.m_name.c_str()))
            MASSERT(!"PerformanceMonitor::ExtendedCommands script returned nonzero!");

    }
    else if (regWrite.m_write == PM_FILE_COMMAND_POLL_MODEC_BUFF)
    {
        MASSERT(m_pPerfmonBuffer);
        bool found = false;
        while (!found) {
            RegRead(
                GetSubDev()->Regs().LookupAddress(MODS_PERF_PMASYS_ENGINESTATUS),
                "Polling delay for PM_FILE_COMMAND_POLL_MODEC_BUFF",
                NONE,
                0);
            UINT32 data = m_pPerfmonBuffer->Read(regWrite.m_address);
            PMInfo("PerformanceMonitor::ExtendedCommands: polling offset=%ld, required=0x%08x, read=0x%08x, eqop=%ld\n",
                    regWrite.m_address, regWrite.m_value, data, regWrite.m_mask);

            found = regWrite.m_mask ? (data == regWrite.m_value) :
                                      (data != regWrite.m_value) ;

        }
        PMInfo("PerformanceMonitor::ExtendedCommands: polling complete\n");
    }

    return;
}

void PerformanceMonitor::PMStreamCapture(RegWriteGroup group, LwRm* pLwRm)
{
    PMInfo("PerformanceMonitor::PMStreamCapture for %s\n", GroupEnumToStr(group));

    auto& InitRegWriteGroup = m_RegWriteListGroup.GetRegWriteInfo(group);

    for (const auto& regWriteSubGroup : InitRegWriteGroup)
    {
        PMInfo("PM: GroupIndex: %d\n", regWriteSubGroup.first);

        for (const auto& regWrite : regWriteSubGroup.second)
        {
            if (regWrite.m_write == PM_FILE_COMMAND_REG_READ)
            {
                RegRead(regWrite.m_address,
                        regWrite.m_name,
                        regWrite.m_instanceType,
                        regWrite.m_instance,
                        regWrite.m_sysPipe);
            }
            if (regWrite.m_write == PM_FILE_COMMAND_REG_READ_ASSIGN)
            {
                auto assignList = RegRead(regWrite.m_address,
                                         regWrite.m_name,
                                         regWrite.m_instanceType,
                                         regWrite.m_instance,
                                         regWrite.m_assignInfoList,
                                         regWrite.m_sysPipe);
            }
            else if (regWrite.m_write == PM_FILE_COMMAND_REG_POLL)
            {
                RegComp(regWrite.m_address,
                        regWrite.m_name,
                        regWrite.m_mask,
                        regWrite.m_value,
                        regWrite.m_instanceType,
                        regWrite.m_instance,
                        regWrite.m_sysPipe);
            }
            else if (regWrite.m_write == PM_FILE_COMMAND_REG_WRITE)
            {
                RegWrite(regWrite.m_address,
                         regWrite.m_name,
                         regWrite.m_mask,
                         regWrite.m_value,
                         regWrite.m_instanceType,
                         regWrite.m_instance,
                         regWrite.m_sysPipe);
            }
            else
            {
                PerformanceMonitor::ExtendedCommands(regWrite, pLwRm);
            }
        }
    }
}

void PerformanceMonitor::PMRegInit(LwRm* pLwRm)
{
    PMInfo("PerformanceMonitor::PMRegInit()\n");

    UINT32 i;

    PMInfo("PM: setting up muxed PM signals\n");

    auto& InitRegWriteGroup = m_RegWriteListGroup.GetRegWriteInfo(INIT);

    for (const auto& regWriteSubGroup : InitRegWriteGroup)
    {
        PMInfo("PM: GroupIndex: %d\n", regWriteSubGroup.first);

        for (const auto& regWrite : regWriteSubGroup.second)
        {
            if (regWrite.m_write == PM_FILE_COMMAND_REG_READ)
            {
                RegRead(regWrite.m_address,
                        regWrite.m_name,
                        regWrite.m_instanceType,
                        regWrite.m_instance,
                        regWrite.m_sysPipe);
            }
            if (regWrite.m_write == PM_FILE_COMMAND_REG_READ_ASSIGN)
            {
                auto assignList = RegRead(regWrite.m_address,
                                         regWrite.m_name,
                                         regWrite.m_instanceType,
                                         regWrite.m_instance,
                                         regWrite.m_assignInfoList,
                                         regWrite.m_sysPipe);
            }
            else if (regWrite.m_write == PM_FILE_COMMAND_REG_POLL)
            {
                RegComp(regWrite.m_address,
                        regWrite.m_name,
                        regWrite.m_mask,
                        regWrite.m_value,
                        regWrite.m_instanceType,
                        regWrite.m_instance,
                        regWrite.m_sysPipe);
            }
            else if (regWrite.m_write == PM_FILE_COMMAND_REG_WRITE)
            {
                RegWrite(regWrite.m_address,
                         regWrite.m_name,
                         regWrite.m_mask,
                         regWrite.m_value,
                         regWrite.m_instanceType,
                         regWrite.m_instance,
                         regWrite.m_sysPipe);
            }
            else
            {
                PerformanceMonitor::ExtendedCommands(regWrite, pLwRm);
            }
        }
    }

    PMInfo("PM: Writing to registers, %i commands to process\n",
        m_PmInitCommands.size());

    for (i = 0; i < m_PmInitCommands.size(); ++i)
    {
        const PMCommandInfo &command = m_PmInitCommands[i];

        switch (command.m_command)
        {
        case pm_set:
            PMInfo("PM command %04i: set: %08X    %08x\n", i, command.m_value, command.m_address);

            m_pGpuRes->RegWr32(command.m_address, command.m_value, m_subdev);
            break;

        default:
            break;
        }
    }
}

vector<SmcEngine*> PerformanceMonitor::GetSmcEngines(const string& sysPipe)
{
    SmcResourceController * pSmcResourceController = nullptr;
    vector<SmcEngine*> smcEngines;

    // Make sure the sysPipe is matching SMC mode
    if (m_pGpuRes->IsSMCEnabled())
    {
        pSmcResourceController = m_pGpuRes->GetSmcResourceController();
    }
    else
    {
        MASSERT(sysPipe == "");
    }

    if (sysPipe == "")
    {
        //In legacy mode. SMC info is not needed
        smcEngines.push_back(nullptr);
    }
    else if (sysPipe == "legacy")
    {
        // access legacy register space in SMC mode, Use a fake SmcEngine pointer to identfy this.
        smcEngines.push_back(SmcEngine::GetFakeSmcEngine());
    }
    else if (sysPipe == "all")
    {
        for (const auto& gpuPartition : pSmcResourceController->GetActiveGpuPartition())
        {
            for (const auto& smcEngine : gpuPartition->GetActiveSmcEngines())
            {
                smcEngines.push_back(smcEngine);
            }
        }
    }
    else
    {
        SmcEngine *pSmcEngine = pSmcResourceController->GetSmcEngine(sysPipe);

        if (!pSmcEngine)
        {
            ErrPrintf("Can't find SMC engine by %s", sysPipe.c_str());
            MASSERT(0);
        }

        smcEngines.push_back(pSmcEngine);
    }

    MASSERT(smcEngines.size() >= 1);
    return smcEngines;
}

RegValInfoList PerformanceMonitor::RegRead(UINT32 address, string name,
    UINT32 instanceType, UINT32 instance, const string& sysPipe)
{
    UINT32 ena_bit = (1 << instance);
    RegValInfoList regValList;
    UINT32 regVal = 0;
    bool skip = false;

    switch (instanceType)
    {
    case TPC:
        if (!(ena_bit & m_TpcMask))
        {
            skip = true;
        }
        break;

    case ROP:
        if (m_TpcMask)
        {
            if (!(ena_bit & m_TpcMask))
            {
                skip = true;
            }
        }
        else if (instance != 0) // For chip with FB=0, ROP should be 0x1
        {
            skip = true;
        }
        break;

    case FBP:
        if (!(ena_bit & m_FbMask))
        {
            skip = true;
        }
        break;

    default:
        break;
    }

    if (skip)
    {
        PMInfo("RegRead to address %u instance %u skipped\n", address, instance);
        return regValList;
    }

    //  have to write a masked value
    vector<SmcEngine*> smcEngineList = GetSmcEngines(sysPipe);
    for (auto&  pSmcEngine : smcEngineList)
    {
        string smcEngineInfo = pSmcEngine ? pSmcEngine->GetInfoStr() : "";
        regVal = m_pGpuRes->RegRd32(address, m_subdev, pSmcEngine);

        string info_print_str =
            Utility::StrPrintf("PerformanceMonitor::RegRead %s %s(%08x) %08x:%08x\n",
                     smcEngineInfo.c_str(), name.c_str(), address, regVal, 0xffffffff);
        PMInfo("%s", info_print_str.c_str());
        regValList.push_back(tie(pSmcEngine, regVal));
    }

    return regValList;
}

vector<pair<string, RegValInfoList>> PerformanceMonitor::RegRead(UINT32 address, string name,
    UINT32 instanceType, UINT32 instance, const list<PerformanceMonitor::RegValAssignInfo>& assignList, const string& sysPipe)
{
    vector<pair<string, RegValInfoList>> assignedValues(assignList.size());

    RegValInfoList regValList = RegRead(address, name, instanceType, instance, sysPipe);

    int i = 0;
    for (const auto& assignInfo : assignList)
    {
        for (auto& regValInfo : regValList)
        {
            UINT32 regVal;
            tie(ignore, regVal) = regValInfo;
            regVal = (regVal & assignInfo.m_mask) >> assignInfo.m_rightShift;
        }

        assignedValues[i++] = {assignInfo.m_name, regValList};
    }

    return assignedValues;
}

void PerformanceMonitor::RegWrite(UINT32 address, string name, UINT32 mask,
    UINT32 value, UINT32 instanceType, UINT32 instance, const string& sysPipe)
{
    UINT32 ena_bit = (1 << instance);
    bool skip = false;

    switch (instanceType)
    {
        case TPC:
            if (!(ena_bit & m_TpcMask))
            {
                skip = true;
            }
            break;

        case ROP:
            if (m_TpcMask)
            {
                if (!(ena_bit & m_TpcMask))
                {
                    skip = true;
                }
            }
            else if (instance != 0) // For chip with FB=0, ROP should be 0x1
            {
                skip = true;
            }
            break;

        case FBP:
            if (!(ena_bit & m_FbMask))
            {
                skip = true;
            }
            break;

        default:
            break;
    }

    if (skip)
    {
        PMInfo("RegWrite to address %u instance %u skipped\n", address, instance);
        return;
    }

    vector<SmcEngine*> smcEngineList = GetSmcEngines(sysPipe);
    if (mask == 0xffffffff)
    {
        for (auto& pSmcEngine : smcEngineList)
        {
            string smcEngineInfo = pSmcEngine ? pSmcEngine->GetInfoStr() : "";

            PMInfo("PerformanceMonitor::RegWrite %s %s(%08x) %08x\n", smcEngineInfo.c_str(), name.c_str(), address, value);

            m_pGpuRes->RegWr32(address, value, m_subdev, pSmcEngine);
#ifdef LW_PERF_CONFIRM_PM_FILE_REG_WRITE
            UINT32 regVal = m_pGpuRes->RegRd32(address, m_subdev, pSmcEngine);
            PMInfo("PerformanceMonitor::RegWrite Confirmation %s(%08x) %08x\n", name.c_str(), address, regVal);
#endif
        }
    }
    else
    {
        //  have to write a masked value
        for (auto& pSmcEngine : smcEngineList)
        {
            string smcEngineInfo = pSmcEngine ? pSmcEngine->GetInfoStr() : "";
            UINT32 regVal = m_pGpuRes->RegRd32(address, m_subdev, pSmcEngine);

            regVal &= ~mask;
            regVal |= (value & mask);

            PMInfo("PerformanceMonitor::RegWrite %s %s(%08x) %08x:%08x\n", smcEngineInfo.c_str(), name.c_str(), address, value, mask);
            m_pGpuRes->RegWr32(address, regVal, m_subdev, pSmcEngine);

#ifdef LW_PERF_CONFIRM_PM_FILE_REG_WRITE
            regVal = m_pGpuRes->RegRd32(address, m_subdev, pSmcEngine);
            PMInfo("PerformanceMonitor::RegWrite Confirmation %s %s(%08x) %08x\n", smcEngineInfo.c_str(), name.c_str(), address, regVal);
#endif
        }
    }
}

void PerformanceMonitor::RegComp(UINT32 address, string name, UINT32 mask,
    UINT32 value, UINT32 instanceType, UINT32 instance, const string& sysPipe)
{
    UINT32 ena_bit = (1 << instance);
    bool skip = false;

    switch (instanceType)
    {
    case TPC:
        if (!(ena_bit & m_TpcMask))
        {
            skip = true;
        }
        break;

    case ROP:
        if (m_TpcMask)
        {
            if (!(ena_bit & m_TpcMask))
            {
                skip = true;
            }
        }
        else if (instance != 0) // For chip with FB=0, ROP should be 0x1
        {
            skip = true;
        }
        break;

    case FBP:
        if (!(ena_bit & m_FbMask))
        {
            skip = true;
        }
        break;

    default:
        break;
    }

    if (skip)
    {
        PMInfo("RegWrite to address %u instance %u skipped\n", address, instance);
        return;
    }

    UINT32 regVal;
    UINT32 poll_limit = 0;
    bool allSmcMatch = false;

    vector<SmcEngine*> smcEngineList = GetSmcEngines(sysPipe);

    while (!allSmcMatch)
    {
        allSmcMatch = true;
        for (auto&  pSmcEngine : smcEngineList)
        {
            string smcEngineInfo = pSmcEngine ? pSmcEngine->GetInfoStr() : "";
            regVal = m_pGpuRes->RegRd32(address, m_subdev, pSmcEngine);
            bool smcMatch = (value & mask) == (regVal & mask);

            PMInfo("PerformanceMonitor::RegComp Polling %s %s(%08x) value=%08x, requested=%08x, mask=%08x, match=%d\n", smcEngineInfo.c_str(), name.c_str(), address, regVal, value, mask, smcMatch);
            if (!smcMatch)
            {
                allSmcMatch = false;
                break;
            }
        }
        poll_limit++;
    }
}

void PerformanceMonitor::AddSetInitCommand(UINT32 address, UINT32 value)
{
    m_PmInitCommands.push_back(PMCommandInfo(pm_set, address, value));
}

void PerformanceMonitor::AddFinalizeInitCommand(UINT32 address, UINT32 value)
{
    m_PmInitCommands.push_back(PMCommandInfo(pm_finalize, address, value));
}

UINT32 PerformanceMonitor::ReadPmState(DomainInfo *domain)
{
    UINT32 ctrl = m_pGpuRes->RegRd32(LW_PPM_CONTROL(domain->GetIndex()), m_subdev);

    return ((domain->GetMode() != PM_MODE_B) ?
        DRF_VAL(_PPM,_CONTROL,_SHADOW_STATE, ctrl) :
        DRF_VAL(_PPM,_CONTROL,_STATE, ctrl));
}

void PerformanceMonitor::ReportPmState(DomainInfo *domain)
{
    if (!domain->IsInUse())
        return;

    UINT32 state = m_pGpuRes->RegRd32(LW_PPM_CONTROL(domain->GetIndex()), m_subdev);

    if (domain->GetMode() == PM_MODE_A)
    {
        UINT32 cycle_0 = 0, cycle_1 = 0, event_0 = 0, event_1 = 0;
        double duty;

        switch(DRF_VAL(_PPM, _CONTROL, _STATE, state))
        {
        case LW_PPM_CONTROL_STATE_IDLE:
            InfoPrintf("PM %s Watch IDLE\n", domain->GetName());
            break;

        case LW_PPM_CONTROL_STATE_WAIT_TRIG0:
            InfoPrintf("PM %s Watch WAIT_TRIG0\n", domain->GetName());
            break;

        case LW_PPM_CONTROL_STATE_WAIT_TRIG1:
            InfoPrintf("PM %s Watch WAIT_TRIG1\n", domain->GetName());
            break;

        case LW_PPM_CONTROL_STATE_CAPTURE:
            cycle_0 = m_pGpuRes->RegRd32(LW_PPM_CYCLECNT_0(domain->GetIndex()), m_subdev);
            event_0 = m_pGpuRes->RegRd32(LW_PPM_EVENTCNT_0(domain->GetIndex()), m_subdev);

            InfoPrintf("PM %s Watch %08x%08x events/%08x%08x cycles ",
                domain->GetName(), event_1, event_0, cycle_1, cycle_0);

            while (cycle_1 || event_1)
            {
                cycle_0 >>= 1;

                if (cycle_1 & 1)
                {
                    cycle_0 |= 0x80000000;
                }

                cycle_1 >>= 1;

                event_0 >>= 1;

                if (event_1 & 1)
                {
                    event_0 |= 0x80000000;
                }

                event_1 >>= 1;
            }

            duty = ((double)event_0) / ((double)(cycle_0));

            InfoPrintf("= %lf duty cycle\n", duty);
            break;

        default:
            break;
        }
    }
    else if (domain->GetMode() == PM_MODE_B)
    {
        switch(DRF_VAL(_PPM, _CONTROL, _SHADOW_STATE, state))
        {
        case LW_PPM_CONTROL_SHADOW_STATE_ILWALID:
            InfoPrintf("PM %s Mode B Watch INVALID\n", domain->GetName());
            break;

        case LW_PPM_CONTROL_SHADOW_STATE_VALID:
            InfoPrintf("PM %s Mode B Watch VALID\n", domain->GetName());
            break;

//         case LW_PPM_CONTROL_SHADOW_STATE_OVERFLOW:
//             InfoPrintf("PM %s Mode B Watch OVERFLOWED\n", dm->GetName());
//             break;

        default:
            break;              // no action for now
        }
    }
}

const char* PerformanceMonitor::GroupEnumToStr(RegWriteGroup group)
{
    switch (group)
    {
        case INIT:
            return "Init";
        case STOP:
            return "Stop";
        case HOLD_TEMPORAL_CAPTURE:
            return "HoldTemporalCapture";
        case START_TEMPORAL_CAPTURE:
            return "StartTemporalCapture";
        default:
            return "Unknown";
    }
}

template<typename... Args>
list<PerformanceMonitor::RegWriteInfo>::reference PerformanceMonitor::RegWriteListGroup::AddRegWrite
(
    RegWriteGroup group,
    INT32 groupIndex,
    Args&&... args
)
{
    auto& regWriteBlock = m_RegWriteInfoGroup[group][groupIndex];
    regWriteBlock.emplace_back(std::forward<Args>(args)...);

    return regWriteBlock.back();
}

const map<INT32, list<PerformanceMonitor::RegWriteInfo>>&
PerformanceMonitor::RegWriteListGroup::GetRegWriteInfo
(
    RegWriteGroup group
)
{
    return m_RegWriteInfoGroup[group];
}

void PerformanceMonitor::AddZLwllId(UINT32 id) 
{
    if (find(s_ZLwllIds.begin(), s_ZLwllIds.end(), id) == s_ZLwllIds.end())
    {
        s_ZLwllIds.push_back(id);
    }
}

void PerformanceMonitor::AddSurface(const MdiagSurf* surf) 
{ 
    if (find(s_RenderSurfaces.begin(), s_RenderSurfaces.end(), surf) 
        == s_RenderSurfaces.end())
    {
        s_RenderSurfaces.push_back(surf);
    }
}

