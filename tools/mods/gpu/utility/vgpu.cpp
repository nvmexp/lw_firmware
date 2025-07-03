/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/vgpu.h"
#include "vgpu/dev_vgpu.h"
#include "vgpu/rpc_headers.h"
#include "core/include/platform.h"
#include "lwmisc.h"
#include "core/include/fileholder.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"
#include "core/include/regexp.h"
#include "core/include/registry.h"
#include "lwRmReg.h"
#include <memory>
#include "t23x/t234/address_map_new.h"

namespace
{
    unique_ptr<Vgpu> s_pVgpu(nullptr);
}

Vgpu::Vgpu()
    : m_bInitialized(false)
    , m_Mutex(Tasker::AllocMutex("Vgpu", Tasker::mtxUnchecked))
    , m_pAperture(0)
    , m_BufferSize(0x1000) //4K
    , m_Sequence(0)
    , m_SendPut(0)
{
    memset(m_BufDesc, 0, sizeof(m_BufDesc));
    memset(m_pBuf, 0, sizeof(m_pBuf));

    m_bGpInRing = false;
    UINT32 data = ~0U;
    if (OK == Registry::Read("ResourceManager", LW_REG_STR_VGPU_GP_IN_BAR0, &data)
        && data == LW_REG_STR_VGPU_GP_IN_BAR0_DISABLE)
    {
        m_bGpInRing = true;
    }
}

bool Vgpu::IsFSF()
{
    if (Platform::IsTegra())
    {
        return false;
    }

    string cpuInfo;
    if (OK != Xp::InteractiveFileRead("/proc/cpuinfo", &cpuInfo))
    {
        return false;
    }

    RegExp re;
#if defined(PPC64LE)
    if (OK != re.Set("model[^\n]*emulated by qemu"))
#else
    if (OK != re.Set("model name[^\n]*KVM"))
#endif
    {
        return false;
    }

    return re.Matches(cpuInfo);
}

bool Vgpu::IsVdk()
{
    static bool isVdk = false;
    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;

        string platformType = Vgpu::ParseTegraPlatformFile();
        if (platformType != "vdk")
        {
            Printf(Tee::PriDebug, "VGPU interface could not detect vdk\n");
            return false;
        }
        isVdk = true;
    }
    return isVdk;
}

// Static helper API to find the platform type from sysfs
string Vgpu::ParseTegraPlatformFile()
{
    static const char* const tegraPlatformFile =
            "/sys/module/tegra_fuse/parameters/tegra_platform";
    if (!Xp::DoesFileExist(tegraPlatformFile))
    {
        Printf(Tee::PriDebug, "VGPU interface not available silicon\n");
        return "silicon";
    }

    FileHolder file;
    RC rc = file.Open(tegraPlatformFile, "rb");
    if (rc != OK)
    {
        Printf(Tee::PriDebug, "VGPU interface not available silicon\n");
        return "silicon";
    }

    string buf;
    buf.resize(16, 0);
    const size_t numRead = fread(&buf[0], 1, buf.size(), file.GetFile());
    buf.resize(numRead > 0 ? numRead : 0);
    while (!buf.empty() && buf[buf.size()-1] <= ' ')
    {
        buf.resize(buf.size()-1);
    }

    return buf;
}

bool Vgpu::Initialize()
{
    if (m_bInitialized)
    {
        return true;
    }

    UINT32 vgpuApertureSize = 0;
    PHYSADDR vgpuApertureBase = 0;

    if (Platform::IsTegra() && IsVdk())
    {
        vgpuApertureBase = LW_ADDRESS_MAP_FAKE_RPC_BASE;
        vgpuApertureSize = (LW_ADDRESS_MAP_FAKE_RPC_LIMIT - LW_ADDRESS_MAP_FAKE_RPC_BASE) + 1U;
    }
    else if (IsFSF())
    {
        // Assume GPU BAR0 on FSF -- if this is sim MODS, then chiplib
        // is not initialized yet and we can't query it.
#if defined(PPC64LE)
        vgpuApertureBase = 0x3ff8011000000LLU + DRF_BASE(LW_VGPU);
#else
        vgpuApertureBase = 0xF0000000U + DRF_BASE(LW_VGPU);
#endif
        vgpuApertureSize = DRF_SIZE(LW_VGPU);
    }
    else
    {
        Printf(Tee::PriLow, "VGPU interface not detected\n");
        return false;
    }

    // Map the VGPU aperture
    if (OK != Platform::MapDeviceMemory(
                reinterpret_cast<void**>(&m_pAperture),
                vgpuApertureBase,
                vgpuApertureSize,
                Memory::UC,
                Memory::ReadWrite))
    {
        Printf(Tee::PriLow, "Failed to map VGPU aperture\n");
        return false;
    }

    // Allocate three buffers
    for (UINT32 i=0; i < numBuffers; i++)
    {
        if (OK != Platform::AllocPages(m_BufferSize, &m_BufDesc[i],
                    true, 32, Memory::UC,
                    Platform::UNSPECIFIED_GPUINST))
        {
            MASSERT(0);
            return false;
        }
    }

    // Map the buffers to user space
    for (UINT32 i=0; i < numBuffers; i++)
    {
        if (OK != Platform::MapPages(
                    reinterpret_cast<void**>(&m_pBuf[i]),
                    m_BufDesc[i], 0, m_BufferSize, Memory::ReadWrite))
        {
            MASSERT(0);
            return false;
        }
    }

    if (!InitRpcRings())
        return false;

    const PHYSADDR shrBufPhys  = Platform::GetPhysicalAddress(
            m_BufDesc[shrBuffer],  0);

    WriteReg(LW_VGPU_CONTROL_BUFF,
            DRF_DEF(_VGPU, _SHARED_MEMORY, _STATUS, _ILWALID));

    const UINT64 shrPage = shrBufPhys / m_BufferSize;
    WriteReg(LW_VGPU_CONTROL_BUFF_HI,
            DRF_NUM(_VGPU,_SHARED_MEMORY_HI,_ADDR,
                    shrPage >> DRF_SIZE(LW_VGPU_SHARED_MEMORY_ADDR_LO)));

    WriteReg(LW_VGPU_CONTROL_BUFF,
            (DRF_DEF(_VGPU, _SHARED_MEMORY, _TARGET, _PHYS_PCI_COHERENT) |
             DRF_DEF(_VGPU, _SHARED_MEMORY, _STATUS, _VALID) |
             DRF_DEF(_VGPU, _SHARED_MEMORY, _SIZE, _4KB) |
             DRF_NUM(_VGPU, _SHARED_MEMORY, _ADDR_LO, shrPage)));

    m_bInitialized = true;

    return true;
}

bool Vgpu::InitRpcRings()
{
    // Obtain physical addresses for the buffers
    const PHYSADDR sendBufPhys = Platform::GetPhysicalAddress(
            m_BufDesc[sendBuffer], 0);
    const PHYSADDR recvBufPhys = Platform::GetPhysicalAddress(
            m_BufDesc[recvBuffer], 0);

    // Mark send ring invalid
    WriteReg(LW_VGPU_SEND_RING,
             DRF_DEF(_VGPU, _SEND_RING, _STATUS, _ILWALID));

    // Read get pointer and make it equal to put.
    m_SendPut = ReadReg(LW_VGPU_SEND_GET);
    WriteReg(LW_VGPU_SEND_PUT, m_SendPut);
    const UINT32 lwrSendGet = ReadReg(LW_VGPU_SEND_GET);
    const UINT32 lwrSendPut = ReadReg(LW_VGPU_SEND_PUT);
    if (lwrSendGet != lwrSendPut)
    {
        Printf(Tee::PriWarn,
               "Failed to initialize VGPU send buffer: get=0x%x, put=0x%x, wrote put=0x%x\n",
               lwrSendGet, lwrSendPut, m_SendPut);
    }

    // Write Send ring address and make it valid
    const UINT64 sendPage = sendBufPhys / m_BufferSize;
    WriteReg(LW_VGPU_SEND_RING_HI,
             DRF_NUM(_VGPU, _SEND_RING_HI, _ADDR,
                 sendPage >> DRF_SIZE(LW_VGPU_SEND_RING_ADDR_LO)));
    WriteReg(LW_VGPU_SEND_RING,
             (DRF_DEF(_VGPU, _SEND_RING, _TARGET,  _PHYS_PCI_COHERENT) |
              DRF_DEF(_VGPU, _SEND_RING, _STATUS,  _VALID) |
              DRF_DEF(_VGPU, _SEND_RING, _SIZE,    _4KB) |
              DRF_NUM(_VGPU, _SEND_RING, _GP_IN_RING, m_bGpInRing) |
              DRF_NUM(_VGPU, _SEND_RING, _ADDR_LO, sendPage)));

    ReadBuffer(sendBuffer, LW_VGPU_DMA_SEND_GET, &m_SendPut, sizeof(m_SendPut));
    WriteBuffer(sendBuffer, LW_VGPU_DMA_SEND_PUT, &m_SendPut, sizeof(m_SendPut));

    // Repeat above steps for receive ring
    WriteReg(LW_VGPU_RECV_RING,
             DRF_DEF(_VGPU, _RECV_RING, _STATUS, _ILWALID));

    UINT32 recvGet = ReadReg(LW_VGPU_RECV_PUT);
    WriteReg(LW_VGPU_RECV_GET, recvGet);
    const UINT32 lwrRecvGet = ReadReg(LW_VGPU_RECV_GET);
    const UINT32 lwrRecvPut = ReadReg(LW_VGPU_RECV_PUT);
    if (lwrRecvGet != lwrRecvPut)
    {
        Printf(Tee::PriWarn,
               "Failed to initialize VGPU recv buffer: get=0x%x, put=0x%x, wrote get=0x%x\n",
               lwrRecvGet, lwrRecvPut, recvGet);
    }

    const UINT64 recvPage = recvBufPhys / m_BufferSize;
    WriteReg(LW_VGPU_RECV_RING_HI,
             DRF_NUM(_VGPU, _RECV_RING_HI, _ADDR,
                 recvPage >> DRF_SIZE(LW_VGPU_RECV_RING_ADDR_LO)));
    WriteReg(LW_VGPU_RECV_RING,
             (DRF_DEF(_VGPU, _RECV_RING, _TARGET,  _PHYS_PCI_COHERENT) |
              DRF_DEF(_VGPU, _RECV_RING, _STATUS,  _VALID) |
              DRF_DEF(_VGPU, _RECV_RING, _SIZE,    _4KB) |
              DRF_NUM(_VGPU, _RECV_RING, _GP_IN_RING, m_bGpInRing) |
              DRF_NUM(_VGPU, _RECV_RING, _ADDR_LO, recvPage)));

    ReadBuffer(recvBuffer, LW_VGPU_DMA_RECV_PUT, &recvGet, sizeof(recvGet));
    WriteBuffer(recvBuffer, LW_VGPU_DMA_RECV_GET, &recvGet, sizeof(recvGet));

    return true;
}

Vgpu::~Vgpu()
{
    for (UINT32 i=0; i < numBuffers; i++)
    {
        if (m_pBuf[i])
        {
            Platform::UnMapPages(m_pBuf[i]);
        }
    }
    for (UINT32 i=0; i < numBuffers; i++)
    {
        if (m_BufDesc[i])
        {
            Platform::FreePages(m_BufDesc[i]);
        }
    }
    if (m_pAperture)
    {
        Platform::UnMapDeviceMemory(m_pAperture);
    }
    Tasker::FreeMutex(m_Mutex);
}

RC Vgpu::EscapeRead(const char* path, UINT32 index, UINT32 size, UINT32* pValue)
{
    Tasker::MutexHolder lock(m_Mutex);
    const UINT32 pathLen = static_cast<UINT32>(strlen(path)) + 1;
    const UINT32 dataOffset = ALIGN_UP(pathLen, sizeof(UINT32));
    WriteHeader(LW_VGPU_MSG_FUNCTION_SIM_ESCAPE_READ,
                DRF_SIZE(LW_VGPU_SIM_ESCAPE_READ_HEADER));
    WriteParam(LW_VGPU_SIM_ESCAPE_READ_DATAIDX, index);
    WriteParam(LW_VGPU_SIM_ESCAPE_READ_DATALEN, size);
    WriteParam(LW_VGPU_SIM_ESCAPE_READ_DATAOFF, dataOffset);
    WriteParam(LW_VGPU_SIM_ESCAPE_READ_PATHSTR, path, pathLen);
    if (IssueAndWait())
    {
        ReadParam(LW_VGPU_SIM_ESCAPE_READ_PATHSTR + dataOffset,
                  pValue, size);
        return OK;
    }
    else
    {
        return RC::FILE_READ_ERROR;
    }
}

RC Vgpu::EscapeWrite(const char* path, UINT32 index, UINT32 size, UINT32 value)
{
    Tasker::MutexHolder lock(m_Mutex);
    const UINT32 pathLen = static_cast<UINT32>(strlen(path)) + 1;
    const UINT32 dataOffset = ALIGN_UP(pathLen, sizeof(UINT32));
    WriteHeader(LW_VGPU_MSG_FUNCTION_SIM_ESCAPE_WRITE,
                DRF_SIZE(LW_VGPU_SIM_ESCAPE_WRITE_HEADER));
    WriteParam(LW_VGPU_SIM_ESCAPE_WRITE_DATAIDX, index);
    WriteParam(LW_VGPU_SIM_ESCAPE_WRITE_DATALEN, size);
    WriteParam(LW_VGPU_SIM_ESCAPE_WRITE_DATAOFF, dataOffset);
    WriteParam(LW_VGPU_SIM_ESCAPE_WRITE_PATHSTR, path, pathLen);
    WriteParam(LW_VGPU_SIM_ESCAPE_WRITE_PATHSTR + dataOffset, value);
    return IssueAndWait() ? OK : RC::FILE_WRITE_ERROR;
}

void Vgpu::ReadReg(UINT32 offset, void* buf, UINT32 size)
{
    Platform::VirtualRd(m_pAperture + offset, buf, size);
}

void Vgpu::WriteReg(UINT32 offset, const void* buf, UINT32 size)
{
    Platform::VirtualWr(m_pAperture + offset, buf, size);
}

void Vgpu::WriteHeader(UINT32 offset, UINT32 size)
{
    const UINT32 regLo = ReadReg(LW_VGPU_SEND_RING);
    const UINT32 regHi = ReadReg(LW_VGPU_SEND_RING_HI);
    const PHYSADDR reg = ((DRF_VAL(_VGPU, _SEND_RING_HI, _ADDR, regHi) << DRF_SIZE(LW_VGPU_SEND_RING_ADDR_LO)) |
                           DRF_VAL(_VGPU, _SEND_RING, _ADDR_LO, regLo));
    const PHYSADDR sendBufPhys = Platform::GetPhysicalAddress(m_BufDesc[sendBuffer], 0);
    const UINT64 sendPage = sendBufPhys / m_BufferSize;
    if (reg != sendPage)
    {
        // The RPC rings need to be re-initialized
        InitRpcRings();
    }

    WriteMsg(LW_VGPU_MSG_HEADER_VERSION, DRF_DEF(_VGPU_MSG_HEADER, _VERSION, _MAJOR, _TOT) |
                                         DRF_DEF(_VGPU_MSG_HEADER, _VERSION, _MINOR, _TOT));
    WriteMsg(LW_VGPU_MSG_SIGNATURE, LW_VGPU_MSG_SIGNATURE_VALID);
    WriteMsg(LW_VGPU_MSG_RESULT,    LW_VGPU_MSG_RESULT_RPC_PENDING);
    WriteMsg(LW_VGPU_MSG_SPARE,     LW_VGPU_MSG_UNION_INIT);
    WriteMsg(LW_VGPU_MSG_FUNCTION,  offset);
    WriteMsg(LW_VGPU_MSG_LENGTH,    DRF_SIZE(LW_VGPU_MSG_HEADER) + size);
}

void Vgpu::WriteParam(UINT32 offset, const void* buf, UINT32 size)
{
    Platform::VirtualWr(m_pBuf[msgBuffer] +
            DRF_SIZE(LW_VGPU_MSG_HEADER) + offset,
            buf, size);
}

void Vgpu::ReadParam(UINT32 offset, void* buf, UINT32 size)
{
    Platform::VirtualRd(m_pBuf[msgBuffer] +
            DRF_SIZE(LW_VGPU_MSG_HEADER) + offset,
            buf, size);
}

void Vgpu::WriteMsg(UINT32 offset, UINT32 value)
{
    Platform::VirtualWr(m_pBuf[msgBuffer] + offset, &value,
            sizeof(UINT32));
}

void Vgpu::ReadBuffer(BufferType type, UINT32 offset, void* data, UINT32 size)
{
    Platform::VirtualRd(m_pBuf[type] + offset, data, size);
}

void Vgpu::WriteBuffer(BufferType type, UINT32 offset, const void* data, UINT32 size)
{
    Platform::VirtualWr(m_pBuf[type] + offset, data, size);
}

bool Vgpu::IssueAndWait()
{
    // Push the message_buffer across
    WriteMsg(LW_VGPU_MSG_SEQUENCE, m_Sequence++);

    const PHYSADDR msgBufPhys  = Platform::GetPhysicalAddress(
            m_BufDesc[msgBuffer],  0);

    // write the message buffer address in send ring page
    const UINT64 msgBufPage = msgBufPhys / m_BufferSize;
    const UINT32 bufAddr[2] = {
        static_cast<UINT32>(DRF_DEF(_VGPU, _DMA, _TARGET,  _PHYS_PCI_COHERENT) |
         DRF_DEF(_VGPU, _DMA, _STATUS,  _VALID) |
         DRF_DEF(_VGPU, _DMA, _SIZE,    _4KB) |
         DRF_NUM(_VGPU, _DMA, _ADDR_LO, msgBufPage))
            ,
        static_cast<UINT32>(DRF_NUM(_VGPU, _DMA_HI, _ADDR,
                    (msgBufPage >> DRF_SIZE(LW_VGPU_DMA_ADDR_LO))))
    };

    if (m_bGpInRing)
    {
        WriteBuffer(sendBuffer, LW_VGPU_DMA_HI, &bufAddr[1], sizeof(bufAddr[1]));
        WriteBuffer(sendBuffer, LW_VGPU_DMA,    &bufAddr[0], sizeof(bufAddr[0]));
    }
    else
    {
        WriteBuffer(sendBuffer, m_SendPut + LW_VGPU_DMA_HI, &bufAddr[1], sizeof(bufAddr[1]));
        WriteBuffer(sendBuffer, m_SendPut + LW_VGPU_DMA,    &bufAddr[0], sizeof(bufAddr[0]));
    }

    m_SendPut = (m_SendPut + sizeof(bufAddr)) % m_BufferSize;

    if (m_bGpInRing)
    {
        WriteBuffer(sendBuffer, LW_VGPU_DMA_SEND_PUT, &m_SendPut, sizeof(m_SendPut));
        // Update the send_put pending bit in LW_VGPU_STATUS register,
        // This will trap into the host
        WriteReg(LW_VGPU_RPC_LAUNCH, 0x0);
    }
    else
    {
        // Update the put pointer. This will trap into the host.
        WriteReg(LW_VGPU_SEND_PUT, m_SendPut);
    }

    // Wait for data
    class PollReplies
    {
        public:
            PollReplies(Vgpu* pVgpu, size_t bufferSize, bool gpInRing)
                : m_pVgpu(pVgpu)
                , m_InitialWait(true)
                , m_BufferSize(bufferSize)
                , m_bGpInRing(gpInRing)
            {
                if (m_bGpInRing)
                    m_pVgpu->ReadBuffer(recvBuffer, LW_VGPU_DMA_RECV_GET, &m_RecvGet, sizeof(m_RecvGet));
                else
                    m_RecvGet = m_pVgpu->ReadReg(LW_VGPU_RECV_GET);
            }
            static bool Func(void* arg)
            {
                PollReplies* const p = static_cast<PollReplies*>(arg);
                return p->PollFunc();
            }

        private:
            bool PollFunc()
            {
                if (m_InitialWait)
                {
                    // First wait for the put pointer to move
                    UINT32 recvPut = 0;
                    if (m_bGpInRing)
                        m_pVgpu->ReadBuffer(recvBuffer, LW_VGPU_DMA_RECV_PUT, &recvPut, sizeof(recvPut));
                    else
                        recvPut = m_pVgpu->ReadReg(LW_VGPU_RECV_PUT);
                    if (recvPut == m_RecvGet)
                    {
                        if (m_bGpInRing)
                            m_pVgpu->WriteReg(LW_VGPU_RPC_LAUNCH, 0x0);
                        return false;
                    }
                    m_InitialWait = false;
                }

                // Update GET pointer
                m_RecvGet = (m_RecvGet + 2*sizeof(UINT32)) % m_BufferSize;

                UINT32 recvPut = 0;
                if (m_bGpInRing)
                {
                    m_pVgpu->WriteBuffer(recvBuffer, LW_VGPU_DMA_RECV_GET, &m_RecvGet, sizeof(m_RecvGet));
                    m_pVgpu->ReadBuffer(recvBuffer, LW_VGPU_DMA_RECV_PUT, &recvPut, sizeof(recvPut));
                }
                else
                {
                    m_pVgpu->WriteReg(LW_VGPU_RECV_GET, m_RecvGet);
                    recvPut = m_pVgpu->ReadReg(LW_VGPU_RECV_PUT);
                }

                // Continue until all replies are received
                if (m_bGpInRing && recvPut != m_RecvGet)
                {
                    m_pVgpu->WriteReg(LW_VGPU_RPC_LAUNCH, 0x0);
                }

                return recvPut == m_RecvGet;
            }

            Vgpu* const  m_pVgpu;
            UINT32       m_RecvGet;
            bool         m_InitialWait;
            const size_t m_BufferSize;
            bool         m_bGpInRing;
    };
    PollReplies pollRepliesArgs(this, m_BufferSize, m_bGpInRing);
    if (OK != POLLWRAP_HW(PollReplies::Func,
                          &pollRepliesArgs,
                          Tasker::GetDefaultTimeoutMs()))
    {
        MASSERT(0);
        return false;
    }

    // Now check if RPC really succeeded
    UINT32 result = ~0U;
    Platform::VirtualRd(m_pBuf[msgBuffer] + LW_VGPU_MSG_RESULT,
            &result, sizeof(result));

    MASSERT(result == LW_VGPU_MSG_RESULT_SUCCESS);
    return result == LW_VGPU_MSG_RESULT_SUCCESS;
}

bool Vgpu::s_bSupported = false;

// This function is so that you can still query for Vgpu support
// even at the end of MODS when Vgpu has been shutdown.
bool Vgpu::IsSupported()
{
    return s_bSupported || GetPtr();
}

Vgpu* Vgpu::GetPtr()
{
#if defined(INCLUDE_GPU) && (defined(SIM_BUILD) || defined(LINUX_MFG) || defined(TEGRA_MODS))
    static bool initFailed = false;
    if (s_pVgpu.get() == nullptr && !initFailed)
    {
        if (IsFSF() || (Platform::IsTegra() && IsVdk()))
        {
            s_pVgpu.reset(new Vgpu);
            if (!s_pVgpu->Initialize())
            {
                s_pVgpu.reset(nullptr);
                initFailed = true;
            }
            else
            {
                s_bSupported = true;
            }
        }
        else
        {
            initFailed = true;
        }
    }
#endif

    return s_pVgpu.get();
}

void Vgpu::Shutdown()
{
    s_pVgpu.reset(nullptr);
}
