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
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <algorithm>
#include <iomanip>
#include <memory>
#include "chiplibtrace.h"
#include "core/include/fileholder.h"
#include "core/include/rc.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "lwmisc.h"
#include "core/include/xp.h"
#include "core/include/regexp.h"
#include "core/include/tar.h"
#include "kepler/gk110/dev_ram.h"
#include "kepler/gk110/dev_bus.h"

#include "kepler/gk104/dev_master.h"  // LW_PMC_INTR_0

DECLARE_MSG_GROUP(Core, Utility, ChiplibTrace)
DEFINE_MSG_GROUP(Core, Utility, ChiplibTrace, false)

#define MSGID() __MSGID__(Core, Utility, ChiplibTrace)

void WriteDataToBmp(const UINT08* data, UINT32 width, UINT32 height,
        ColorUtils::Format format, const std::string& surfName);
static bool IsSupportedFormat(ColorUtils::Format format)
{
    switch (format)
    {
        case ColorUtils::Y8:
        case ColorUtils::I8:
        case ColorUtils::CPST8:
        case ColorUtils::S8:
        case ColorUtils::Z16:
        case ColorUtils::R5G6B5:
        case ColorUtils::VOID16:
        case ColorUtils::X8R8G8B8:
        case ColorUtils::A8R8G8B8:
        case ColorUtils::R8G8B8A8:
        case ColorUtils::Z24S8:
        case ColorUtils::Z32:
        case ColorUtils::A2R10G10B10:
        case ColorUtils::R10G10B10A2:
        case ColorUtils::VOID32:
        case ColorUtils::A2B10G10R10:
        case ColorUtils::A2U10Y10V10:
        case ColorUtils::X2BL10GL10RL10_XRBIAS:
        case ColorUtils::X2BL10GL10RL10_XVYCC:
        case ColorUtils::A8B8G8R8:
        case ColorUtils::Z1R5G5B5:
        case ColorUtils::A1R5G5B5:
        case ColorUtils::Z8R8G8B8:
        case ColorUtils::A2V10Y10U10:
        case ColorUtils::VE8YO8UE8YE8:
        case ColorUtils::YO8VE8YE8UE8:
            return true;

        default:
            return false;
    }
}
namespace Replayer
{
const std::string ChiplibTrace::m_Bar0Prefix   = "  BAR0M";
const std::string ChiplibTrace::m_PmClockCheck = "gpcElapsedClocks";
const float ChiplibTrace::m_PmTolerance = .05f;

bool ChiplibTrace::IsInsideCRCReading()
{
    return m_StartCRCReading;
}
void ChiplibTrace::CollectReplayData(size_t size, size_t diff)
{
    if (m_StartCRCReading)
    {
        m_TotalSize += size;
        m_TotalDiff += diff;
    }
}

void ChiplibTrace::StartCRCChecking()
{
    m_TotalSize = 0;
    m_TotalDiff = 0;
    m_StartCRCReading = true;
}
void ChiplibTrace::EndCRCChecking(const std::string& surfName)
{
    std::map<std::string, std::pair<UINT64, UINT64> >::iterator it =
        m_ReplayStat.find(surfName);
    if (it != m_ReplayStat.end() && it->second.first == 0)
        m_ReplayStat.erase(it);

    m_ReplayStat.insert(std::make_pair(surfName,
                std::make_pair(m_TotalSize, m_TotalDiff)));
    m_StartCRCReading = false;
}
bool inline LessThan(UINT32 src, UINT32 dst)
{
    return src < dst;
}
bool inline LessOrEqual(UINT32 src, UINT32 dst)
{
    return src <= dst;
}
bool inline EqualTo(UINT32 src, UINT32 dst)
{
    return src == dst;
}
bool inline GreaterOrEqual(UINT32 src, UINT32 dst)
{
    return src >= dst;
}
bool inline GreaterThan(UINT32 src, UINT32 dst)
{
    return src > dst;
}

static size_t CompareData(const void* src, const void* dst,
        size_t size, std::string& msg_type, std::string& msg,
        bool (*f)(UINT32, UINT32) = NULL)
{
    size_t diffCnt = 0;
    const UINT08* p1 = (const UINT08*)src;
    const UINT08* p2 = (const UINT08*)dst;
    if (f)
    {
        const UINT32* pSrc = (const UINT32*)src;
        const UINT32* pDst = (const UINT32*)dst;
        size_t dwordCnt = size/4;
        for ( unsigned int i = 0; i < dwordCnt; ++i)
        {
            if (!f(pSrc[i], pDst[i]))
                diffCnt++;
        }
        unsigned int srcVal = 0;
        unsigned int dstVal = 0;
        for (size_t i = dwordCnt *4; i < size; ++i)
        {
            srcVal = srcVal << 8;
            srcVal += p1[i];
            dstVal = dstVal << 8;
            dstVal += p1[i];
        }
        if (dwordCnt *4 < size)
        {
            if (!f(srcVal, dstVal))
                diffCnt++;
        }
    }
    else
    {
        for ( size_t i = 0; i < size; ++i)
        {
            if (p1[i] != p2[i])
                diffCnt++;
        }
    }
    if ( diffCnt && msg.empty() )
    {
        msg.reserve(size*6 + 100);
        msg = Utility::StrPrintf("[MISMATCH]: %s %lld byte(s) mismatched, expected",
                                msg_type.c_str(), static_cast<UINT64>(diffCnt));

        p1 = (const UINT08*)src;
        p2 = (const UINT08*)dst;
        for (size_t i = size; i > 0; )
        {
            if (i % 4 == 0 || i == size)
            {
                msg += Utility::StrPrintf(" 0x");
            }

            msg += Utility::StrPrintf("%02x", (unsigned int)p2[--i]);
        }
        msg += Utility::StrPrintf(" got");
        for (size_t i = size; i > 0; )
        {
            if (i % 4 == 0 || i == size)
            {
                msg += Utility::StrPrintf(" 0x");
            }
            msg += Utility::StrPrintf("%02x", (unsigned int)p1[--i]);
        }
    }
    if ( diffCnt == 0 && msg.empty() )
    {
        msg = "[MATCH]: " + msg_type;
    }
    return diffCnt;
}
static int GetUINT08FromStr(const char* str, UINT08* val)
{
    char* p;
    errno = 0;
    UINT08 v = static_cast<UINT08>(strtoul(str, &p, 0));
    if ( (p == str) || (p && *p!=0) || (ERANGE == errno) )
        return -1;
    *val = v;
    return 0;

}
static int GetUINT32FromStr(const char* str, UINT32* val)
{
    char* p;
    errno = 0;
    UINT32 v = strtoul(str, &p, 0);
    if ( (p == str) || (p && *p!=0) || (ERANGE == errno) )
        return -1;
    *val = v;
    return 0;

}
static int GetUINT64FromStr(const char*str, UINT64* val)
{
    char* p;
    errno = 0;
    UINT64 v = Utility::Strtoull(str, &p, 0);
    if ( (p == str) || (p && *p!=0) || (ERANGE == errno) )
        return -1;
    *val = v;
    return 0;
}

static bool IsEndingWith(const string& str, const string& ending)
{
    if (str.length() >= ending.length())
    {
        string cmpstr(str, str.length() - ending.length(), ending.length());
        return cmpstr.compare(ending) == 0;
    }
    else
    {
        return false;
    }
}

BusMemOp::APERTURE GetApertureFromStr(const char* str)
{
    if (!str)
        return BusMemOp::ILWALID_APERTURE;
    if      (strcmp(str, "bar0"     ) == 0) return BusMemOp::BAR0;
    else if (strcmp(str, "bar1"     ) == 0) return BusMemOp::BAR1;
    else if (strcmp(str, "bar2"     ) == 0) return BusMemOp::BAR2;
    else if (strcmp(str, "inst"     ) == 0) return BusMemOp::BARINST;
    else if (strcmp(str, "sysmem"   ) == 0) return BusMemOp::SYSMEM;
    else if (strcmp(str, "cfg0"     ) == 0) return BusMemOp::CFGSPACE0;
    else if (strcmp(str, "cfg1"     ) == 0) return BusMemOp::CFGSPACE1;
    else if (strcmp(str, "cfg2"     ) == 0) return BusMemOp::CFGSPACE2;
    else                                    return BusMemOp::ILWALID_APERTURE;

    return BusMemOp::ILWALID_APERTURE;
}

TraceOp::TraceOp(ChiplibTrace* trace, const std::vector<const char*>& tokens,
        const std::string& comment, int lineNum)
    :m_Comment(comment), m_Trace(trace), m_LineNum(lineNum)
{
    std::vector<const char*>::const_iterator it = tokens.begin();
    for (; it != tokens.end(); ++it)
    {
        m_OpStr += *it;
        m_OpStr += " ";
    }
}

bool TraceOp::LoadDumpFile(const string& fileName, vector<UINT08>* pData,
        UINT32 size, bool allowArchive)
{
    pData->reserve(size);
    string archiveName = m_Trace->GetTracePath() + "/" + "backdoor.tgz";
    unique_ptr<TarArchive> archive(new TarArchive());
    string archiveDumpFileName;

    if (allowArchive && archive->ReadFromFile(archiveName, true))
    {
        archiveDumpFileName = Utility::StrPrintf("%s/%s/%s",
            m_Trace->GetGpuName().c_str(),
            m_Trace->GetBackdoorArchiveId().c_str(),
            fileName.c_str());
        TarFile *tarFile = archive->FindPath(archiveDumpFileName);

        if (tarFile != 0)
        {
            char buffer[3];
            UINT32 value;

            for (UINT32 i = 0; i < size; ++i)
            {
                tarFile->Read(buffer, sizeof(buffer));
                sscanf(buffer, "%02x", &value);
                (*pData)[i] = value;
            }

            return true;
        }
    }

    FileHolder file;
    if (OK != file.Open(fileName, "r"))
    {
        if (allowArchive)
        {
            Printf(Tee::PriHigh, "Error: dump file %s does not exist in archive %s\n",
                archiveDumpFileName.c_str(), archiveName.c_str());
        }
        else
        {
            Printf(Tee::PriHigh, "Error: file %s does not exist\n",
                fileName.c_str());
        }

        return false;
    }

    int errors = 0;
    for (UINT32 i = 0; i < size; ++i)
    {
        UINT32 value;
        errors += 1 - fscanf(file.GetFile(), "%02x", &value);
        (*pData)[i] = value;
    }

    if (errors)
    {
        Printf(Tee::PriNormal, "Error reading data from file %s\n", fileName.c_str());
        return false;
    }

    return true;
}
bool ChiplibTrace::NeedRetry(unsigned int retryCnt, const ReplayOptions& option,
        const string& aperture)
{
    if (IsInsideCRCReading() && aperture == "bar1")
    {
        Printf(Tee::PriNormal, "  skipping for CRC checking read\n");
        return false;
    }
    if (retryCnt > option.maxPollCnt
           && Platform::GetSimulationMode() == Platform::Hardware )
    {
        Printf(Tee::PriNormal, " Retried %d times on hw, skipped. ",
                option.maxPollCnt + 2);
        return false;
    }
    return true;
}
void ChiplibTrace::WaitForRetry(const ReplayOptions& option)
{
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        Platform::ClockSimulator(1);
    }
    else
    {
        Printf(Tee::PriNormal, " Sleeping %d ms on hw...", option.maxSleepTimeInMs);
        Platform::Delay(option.maxSleepTimeInMs * 1000);
    }
}
void ChiplibTrace::PrintRetryMsg(const string& msg, unsigned int retryCnt, int maxPollCnt)
{
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        Printf(Tee::PriNormal, "%s. Try %d.\n", msg.c_str(), retryCnt);
    }
    else
    {
        Printf(Tee::PriNormal, "%s. Try %d of %d.\n", msg.c_str(), retryCnt, maxPollCnt);
    }
}

RC BusMemOp::Replay(const ReplayOptions& option, int* replayCount)
{
    RC rc = OK;
    switch (m_Type)
    {
        case READ:
            {
            UINT64 addr = m_Trace->GetBaseAddr(m_Aperture).first + m_Offset;
            vector<UINT08> data(m_Data.size());
            unsigned int retryCnt = 0;
            size_t mismatchCnt = m_Data.size();
            std::string msg("");
            while(mismatchCnt)
            {
                int ret = ReplayRead(addr, &data[0], UNSIGNED_CAST(UINT32, m_Data.size()));
                if (ret == 0)
                {
                    std::string msg_type_str =
                        Utility::StrPrintf("%s read. Addr: 0x%08llx",
                                              m_Aperture.c_str(), addr);
                    mismatchCnt = CompareData(&data[0], &m_Data[0],
                                              m_Data.size(), msg_type_str, msg);
                }
                else
                {
                    Printf(Tee::PriNormal, "  Replay read failed!(%s)\n", m_OpStr.c_str());
                }

                if (!m_Trace->NeedRetry(retryCnt, option, m_Aperture)
                        || mismatchCnt == 0)
                {
                    break;
                }

                m_Trace->PrintRetryMsg(msg, retryCnt+1, option.maxPollCnt + 2);
                if (mismatchCnt && retryCnt == 0 && m_Aperture == "bar0")
                {
                    m_Trace->PrintRegister(m_Offset, *(UINT32*)(&data[0]), *(UINT32*)(&m_Data[0]));
                }

                m_Trace->WaitForRetry(option);

                retryCnt++;
            }

            m_Trace->CollectReplayStatistics(m_Data.size(), mismatchCnt,
                    m_Aperture,  ChiplibTrace::OP_READ);
            if (m_Aperture == "bar1" || m_Aperture == "sysmem0")
                m_Trace->CollectReplayData(m_Data.size(), mismatchCnt);

            if (!mismatchCnt)
            {
                Printf(Tee::PriDebug, MSGID(), "%s\n", msg.c_str());
            }
            else
            {
                if (m_Aperture == "bar0")
                {
                    m_Trace->PrintRegister(m_Offset,
                            *(UINT32*)(&data[0]), *(UINT32*)(&m_Data[0]));
                }
                if (IsMustMatchBusMemOp(option))
                {
                    Printf(Tee::PriHigh, "Error: Mismatch happened on a must-match read.\n");
                    rc = RC::VECTORDATA_VALUE_MISMATCH;
                }
            }
            break;
            }
        case WRITE:
            {
                int ret = ReplayWrite(m_Trace->GetBaseAddr(m_Aperture).first + m_Offset,
                        &m_Data[0], UNSIGNED_CAST(UINT32, m_Data.size()));
                if (ret != 0)
                {
                    Printf(Tee::PriNormal, "  Replay write failed! (%s)\n",
                            m_OpStr.c_str());
                }
                m_Trace->CollectReplayStatistics(m_Data.size(), 0, m_Aperture,
                        ChiplibTrace::OP_WRITE);
                break;
            }
        default:
            assert(false);
    }
    *replayCount = 1;
    return rc;
}

static void PciAddrColw(UINT32 addr, INT32* domainNumber, INT32* busNumber, INT32*  deviceNumber,
        INT32*  functionNumber)
{
    *domainNumber = (addr & 0xf0000000) >> 28;
    *busNumber = (addr & 0x00ff0000) >> 16;
    *deviceNumber = (addr & 0x0000f800) >> 11;
    *functionNumber = (addr & 0x00000700) >> 8;
}
int BusMemOp::ReplayRead(UINT64 addr, void* data, UINT32 count)
{
    std::string aperture = m_Aperture;
    if (aperture.find("bar") == string::npos
            && aperture.find("cfg") == string::npos
            && aperture.find("sysmem") == string::npos)
    {
        aperture = m_Trace->GetRealAperture(m_Aperture);
    }
    if (aperture.find("bar") == 0 || aperture.find("sysmem") == 0)
    {
        std::pair<UINT64, UINT64> addrCfg = m_Trace->GetBaseAddr(m_Aperture);
        m_Trace->MapDeviceMemory(aperture, addrCfg.first, addrCfg.second);
    }
    if (aperture.find("bar") == 0)
    {
        return Platform::PhysRd(addr, data, UNSIGNED_CAST(UINT32, m_Data.size()));
    }
    else if (aperture.find("cfg") == 0)
    {
        UINT32 realAddr = (UINT32)(addr - m_Trace->GetBaseAddr(m_Aperture).first);
        INT32 domain, bus, dev, func;
        PciAddrColw((UINT32)(m_Trace->GetBaseAddr(m_Aperture).first), &domain, &bus, &dev, &func);
        if (m_Data.size() == 1)
            return Platform::PciRead08(domain, bus, dev, func, realAddr, (UINT08*)data);
        if (m_Data.size() == 2)
            return Platform::PciRead16(domain, bus, dev, func, realAddr, (UINT16*)data);
        if (m_Data.size() == 4)
            return Platform::PciRead32(domain, bus, dev, func, realAddr, (UINT32*)data);
        assert(false);
        return -1;
    }
    else if (aperture.find("sysmem") == 0)
    {
        return Platform::PhysRd(addr, data, UNSIGNED_CAST(UINT32, m_Data.size()));
    }
    else
    {
        if (Platform::IsTegra())
        {
            const UINT64 unknownApertureBase = addr & ~0xFFFFFF;
            const UINT64 unknownApertureSize = 0x1000000;
            string mappedApertureName = Utility::StrPrintf("UNKOW_0x%llx",
                    unknownApertureBase);
            m_Trace->MapDeviceMemory(mappedApertureName, unknownApertureBase,
                    unknownApertureSize);

            return Platform::PhysRd(addr, data, UNSIGNED_CAST(UINT32, m_Data.size()));
        }
        else
        {
            Printf(Tee::PriNormal, "WARNING: Invalid aperture (%s)\n",
                    m_OpStr.c_str());
        }
        return -1;
    }
}

bool BusMemOp::IsMustMatchBusMemOp(const ReplayOptions& option) const
{
    if (option.bar1MustMatch && (m_Aperture == "bar1"))
    {
        return true;
    }

    return false;
}

int BusMemOp::ReplayWrite(UINT64 addr, const void* data, UINT32 count )
{
    std::string aperture = m_Aperture;
    if (aperture.find("bar") == string::npos
            && aperture.find("cfg") == string::npos
            && aperture.find("sysmem") == string::npos)
    {
        aperture = m_Trace->GetRealAperture(m_Aperture);
    }
    if (aperture.find("bar") == 0 || aperture.find("sysmem") == 0)
    {
        std::pair<UINT64, UINT64> addrCfg = m_Trace->GetBaseAddr(m_Aperture);
        m_Trace->MapDeviceMemory(aperture, addrCfg.first, addrCfg.second);
    }
    if(aperture.find("bar") == 0)
    {
        return Platform::PhysWr(addr, data, count);
    }
    else if (aperture.find("cfg") == 0)
    {
        UINT32 realAddr = (UINT32)(addr - m_Trace->GetBaseAddr(m_Aperture).first);
        INT32 domain, bus, dev, func;
        PciAddrColw((UINT32)(m_Trace->GetBaseAddr(m_Aperture).first),
                &domain, &bus, &dev, &func);
        if (m_Data.size() == 1)
            return Platform::PciWrite08(domain, bus, dev, func, realAddr,
                    *((const UINT08*)data));
        if (m_Data.size() == 2)
            return Platform::PciWrite16(domain, bus, dev, func, realAddr,
                    *((const UINT16*)data));
        if (m_Data.size() == 4)
            return Platform::PciWrite32(domain, bus, dev, func, realAddr,
                    *((const UINT32*)data));
        assert(false);
        return -1;
    }
    else if (aperture.find("sysmem") == 0)
    {
        return Platform::PhysWr(addr, data, static_cast<UINT32>(m_Data.size()));
    }
    else
    {
        if (Platform::IsTegra())
        {
            const UINT64 unknownApertureBase = addr & ~0xFFFFFF;
            const UINT64 unknownApertureSize = 0x1000000;
            string mappedApertureName = Utility::StrPrintf("UNKOW_0x%llx",
                    unknownApertureBase);
            m_Trace->MapDeviceMemory(mappedApertureName, unknownApertureBase,
                    unknownApertureSize);

            return Platform::PhysWr(addr, data, static_cast<UINT32>(m_Data.size()));
        }
        else
        {
            Printf(Tee::PriNormal, "WARNING: Invalid aperture (%s)\n",
                    m_OpStr.c_str());
        }
        return 0;
    }
}
BusMemOp::BusMemOp(ChiplibTrace* trace, const std::vector<const char*>& tokens,
        MEM_OPTYPE type, const std::string& comment, int lineNum)
:TraceOp(trace, tokens, comment, lineNum), m_Type(type)
{
    assert(tokens.size() >= 4);
    //m_Aperture = getApertureFromStr(tokens[1]);
    m_Aperture = tokens[1];
    GetUINT64FromStr(tokens[2], &m_Offset);
    for (size_t i = 3; i < tokens.size(); i++)
    {
        UINT08 val;
        GetUINT08FromStr(tokens[i], &val);
        m_Data.push_back(val);
    }
}
EscapeOp::EscapeOp(ChiplibTrace* trace, const std::vector<const char*>& tokens,
        ESCAPE_OPTYPE type, const std::string& comment, int lineNum)
:TraceOp(trace, tokens, comment, lineNum),
    m_Type(type), m_GpuId(0xff), m_Index(-1), m_Size(0)
{
    assert(tokens.size() >= 5);
    GetUINT08FromStr(tokens[1], &m_GpuId);
    if (m_GpuId != 0xff)
    {
        if (type == EscapeOp::READ) m_Type = EscapeOp::GPU_READ;
        if (type == EscapeOp::WRITE) m_Type = EscapeOp::GPU_WRITE;
    }

    m_Key = tokens[2];
    if (m_Key[0] == '"' && m_Key[m_Key.size() - 1] == '"')
        m_Key = m_Key.substr(1, m_Key.size() - 2);
    GetUINT32FromStr(tokens[3], &m_Index);
    GetUINT32FromStr(tokens[4], &m_Size);

    for (size_t i = 5; i < tokens.size(); i++)
    {
        UINT08 val;
        GetUINT08FromStr(tokens[i], &val);
        m_Data.push_back(val);
    }
}
RC EscapeOp::Replay(const ReplayOptions& option, int* replayCount) {
    if (!option.enableEscape)
    {
        Printf(Tee::PriNormal, "Ignore escape op(%s).\n", m_OpStr.c_str());
        *replayCount = 0;
        return OK;
    }
    switch(m_Type) {
        case READ:
        case GPU_READ:
            {
                UINT08* pData = new UINT08[m_Data.size()];
                int ret = ReplayEscapeRead(m_Size, pData);
                if (ret == 0)
                {
                    std::string msg("");
                    std::string msg_type("EscapeRead");
                    size_t mismatchCnt = CompareData(pData, &m_Data[0],
                            m_Data.size(), msg_type, msg);
                    if (!mismatchCnt)
                    {
                        Printf(Tee::PriDebug, MSGID(), "\n%s", msg.c_str());
                    }
                    else
                    {
                        Printf(Tee::PriNormal, "%s  skipping", msg.c_str());
                    }
                }
                else
                {
                    Printf(Tee::PriNormal, "  EscapeRead failed\n");
                }
                delete []pData;
                break;
            }
        case WRITE:
        case GPU_WRITE:
            {
                int ret = ReplayEscapeWrite(
                    UNSIGNED_CAST(UINT32, m_Data.size()), &m_Data[0]);
                if (ret != 0)
                {
                    Printf(Tee::PriNormal, "  EscapeWrite failed\n");
                }
                break;
            }
        default:
            assert(false);
    }
    *replayCount = 1;
    return OK;
}
int EscapeOp::ReplayEscapeRead(UINT32 count, void* data)
{
    if (m_Type == READ)
    {
        return Platform::EscapeRead(m_Key.c_str(), m_Index, count, (UINT32*)data);
    }
    else
    {
        return Platform::GpuEscapeReadBuffer(m_GpuId, m_Key.c_str(),
                m_Index, count, data);
    }
}
int EscapeOp::ReplayEscapeWrite(UINT32 count, const void* data)
{
    if (m_Type == WRITE)
    {
        UINT32 val = 0;
        if (data)
        {
            val = *(const UINT32*)data;
        }
        return Platform::EscapeWrite(m_Key.c_str(), m_Index, count, val);
    }
    else
    {
        return Platform::GpuEscapeWriteBuffer(m_GpuId, m_Key.c_str(),
                m_Index, count, data);
    }
}

DumpRawMemOp::DumpRawMemOp
(
    ChiplibTrace* trace,
    const std::vector<const char*>& tokens,
    const std::string& comment,
    int lineNum
)
:TraceOp(trace, tokens, comment, lineNum),
    m_PhysAddress(0), m_Size(0)
{
    if (tokens.size() >= 2)
        m_MemAperture = tokens[1];
    if (tokens.size() >= 3)
        GetUINT64FromStr(tokens[2], &m_PhysAddress);
    if (tokens.size() >= 4)
        GetUINT32FromStr(tokens[3], &m_Size);
    if (tokens.size() >= 5)
        m_FileName = tokens[4];
}

RC DumpRawMemOp::Replay(const ReplayOptions& option, int* replayCount)
{
    RC rc;
    bool dumpBmp = false;

    if (m_FileName.find(".pitch.chiplib_replay.") != std::string::npos)
    {
        dumpBmp = true;
    }

    if (Platform::IsTegra()) // modern CheetAh only has sysmem
    {
        Printf(Tee::PriHigh, "Calling DumpViaDirectPhysRd\n");
        rc = DumpViaDirectPhysRd(option, dumpBmp);
        Printf(Tee::PriHigh, "Returned from DumpViaDirectPhysRd\n");
    }
    else if (m_PhysAddress + m_Size >= 0x8000000ULL)
    {
        Printf(Tee::PriHigh, "Calling DumpViaPramin\n");
        rc = DumpViaPramin(option, dumpBmp);
        Printf(Tee::PriHigh, "Returned from DumpViaPramin\n");
    }
    else
    {
        Printf(Tee::PriHigh, "Calling DumpViaBar1\n");
        rc = DumpViaBar1(option, dumpBmp);
        Printf(Tee::PriHigh, "Returned from DumpViaBar1\n");
    }

    if (rc == OK)
    {
        CompareDumpFiles();
    }

    *replayCount = 1;
    return rc;
}

RC DumpRawMemOp::DumpViaBar1( const ReplayOptions& option, bool saveImg)
{
    if (m_MemAperture != "FB")
    {
        Printf(Tee::PriError, "BAR1 dumping only supports FB\n");
        return RC::BAD_PARAMETER;
    }

    FileHolder dumpF;
    if (OK != dumpF.Open(m_FileName, "w"))
    {
        return RC::BAD_PARAMETER;
    }

    UINT64 bar0Base = m_Trace->GetBaseAddr("bar0").first;
    UINT32 oldBar1Window = 0;

    Platform::PhysRd(bar0Base + LW_PBUS_BAR1_BLOCK, &oldBar1Window, 4);

    UINT32 newBar1Setting = 0;
    Platform::PhysWr(bar0Base + LW_PBUS_BAR1_BLOCK, &newBar1Setting, 4);

    std::vector<UINT08> data;
    data.resize(m_Size);

    UINT64 bar1Base = m_Trace->GetBaseAddr("bar1").first;
    UINT64 address = m_PhysAddress + bar1Base;
    Printf(Tee::PriHigh, "  Bar1 PhysRd start!\n");
    Platform::PhysRd(address, &data[0], m_Size);
    Printf(Tee::PriHigh, "  Bar1 PhysRd done!\n");

    if (saveImg)
    {
        const SurfInfo& info = m_Trace->GetLwrSurfInfo();
        WriteDataToBmp(&data[0], info.width, info.height,
                info.format, info.name + ".pitch.chiplib_replay");
        Printf(Tee::PriHigh, "  BMP write done!\n");
    }

    for (UINT32 i = 0; i < m_Size; ++i)
    {
        fprintf(dumpF.GetFile(), "%02x\n", data[i]);
    }

    Platform::PhysWr(bar0Base + LW_PBUS_BAR1_BLOCK, &oldBar1Window, 4);

    return OK;
}

RC DumpRawMemOp::DumpViaPramin(const ReplayOptions& option, bool saveImg)
{
    if (m_MemAperture != "FB")
        return RC::BAD_PARAMETER;

    FileHolder dumpF;
    if (OK != dumpF.Open(m_FileName, "w"))
    {
        return RC::BAD_PARAMETER;
    }

    UINT64 bar0Base = m_Trace->GetBaseAddr("bar0").first;
    UINT32 oldBar0Window;

    // Read the BAR0 window so the current value can be restored later.
    Platform::PhysRd(bar0Base + LW_PBUS_BAR0_WINDOW, &oldBar0Window, 4);

    UINT32 remaining = m_Size;
    UINT64 readAddr = m_PhysAddress;
    const UINT32 BAR0_WINDOW_SIZE = 0x100000;

    std::vector<UINT08> imgData;
    imgData.resize(m_Size);

    std::vector<UINT08> data;
    data.resize(min(BAR0_WINDOW_SIZE, remaining));

    UINT32 readBackSize = 0;
    while (remaining)
    {
        UINT32 newBar0Window = static_cast<UINT32>(readAddr>> 16);

        Platform::PhysWr(bar0Base + LW_PBUS_BAR0_WINDOW, &newBar0Window, 4);

        UINT32 offset = static_cast<UINT32>(readAddr & 0x000000000000ffffLL);
        UINT32 sizeToRead = min(BAR0_WINDOW_SIZE, remaining);
        Platform::PhysRd(bar0Base + offset + DEVICE_BASE(LW_PRAMIN),
                &data[0], sizeToRead);

        std::copy(data.begin(), data.begin() + sizeToRead,
                imgData.begin() + readBackSize);

        for (UINT32 i = 0; i < sizeToRead; ++i)
        {
            fprintf(dumpF.GetFile(), "%02x\n", data[i]);
        }
        readBackSize += sizeToRead;

        if (remaining < BAR0_WINDOW_SIZE)
            break;
        remaining -= BAR0_WINDOW_SIZE;
        readAddr += BAR0_WINDOW_SIZE;
    }
    MASSERT(readBackSize == m_Size);

    if (saveImg)
    {
        const SurfInfo& info = m_Trace->GetLwrSurfInfo();
        WriteDataToBmp(&imgData[0], info.width, info.height,
                info.format, m_FileName);
    }

    Platform::PhysWr(bar0Base + LW_PBUS_BAR0_WINDOW, &oldBar0Window, 4);

    return OK;
}

RC DumpRawMemOp::DumpViaDirectPhysRd(const ReplayOptions& option, bool saveImg)
{
    if (m_MemAperture != "sysmem")
    {
        Printf(Tee::PriError, "Direct Phys Rd dumping only supports sysmem\n");
        return RC::BAD_PARAMETER;
    }

    FileHolder dumpF;
    if (OK != dumpF.Open(m_FileName, "w"))
    {
        return RC::BAD_PARAMETER;
    }

    std::vector<UINT08> data;
    data.resize(m_Size);

    void *pMem = nullptr;
    Platform::MapDeviceMemory(&pMem, m_PhysAddress, m_Size, Memory::UC, Memory::ReadWrite);
    Platform::PhysRd(m_PhysAddress, &data[0], m_Size);
    Platform::UnMapDeviceMemory(pMem);

    if (saveImg)
    {
        const SurfInfo& info = m_Trace->GetLwrSurfInfo();
        WriteDataToBmp(&data[0], info.width, info.height,
            info.format, info.name + ".pitch.chiplib_replay");
        Printf(Tee::PriDebug, "BMP write done!\n");
    }

    for (UINT32 i = 0; i < m_Size; ++i)
    {
        fprintf(dumpF.GetFile(), "%02x\n", data[i]);
    }

    return OK;
}

int DumpRawMemOp::CompareDumpFiles()
{
    const char* origSuffix = "chiplib_dump";
    const char* lwrrentSuffix = "chiplib_replay";
    std::string fileName1 = m_FileName;
    std::string& fileName2 = m_FileName;
    std::string surfaceName = m_FileName;

    size_t pos;

    pos = fileName1.find(lwrrentSuffix);

    if (pos == std::string::npos)
    {
        Printf(Tee::PriHigh,
                "Error:  dump file name %s doesn't have suffix \'%s\'.\n",
                fileName1.c_str(), lwrrentSuffix);
        return -1;
    }

    fileName1.replace(pos, strlen(lwrrentSuffix), origSuffix);
    surfaceName.erase(pos - 1);
    pos = surfaceName.find("pitch");
    if (pos != std::string::npos)
        surfaceName.erase(pos - 1);

    std::vector<UINT08> data1;

    // Load dump from capture (check archive if possible)
    if (!LoadDumpFile(fileName1, &data1, m_Size, true))
    {
        return -1;
    }

    std::vector<UINT08> data2;

    // Load recent dump file (but don't check archive)
    if (!LoadDumpFile(fileName2, &data2, m_Size, false))
    {
        return -1;
    }

    size_t mismatches = 0;

    for (UINT32 i = 0; i < m_Size; ++i)
    {
        if (data1[i] != data2[i])
        {
            ++mismatches;
        }
    }
    m_Trace->CollectReplayData(m_Size, mismatches);

    return 0;
}

LoadRawMemOp::LoadRawMemOp
(
    ChiplibTrace* trace,
    const std::vector<const char*>& tokens,
    const std::string& comment,
    int lineNum
)
:TraceOp(trace, tokens, comment, lineNum),
    m_PhysAddress(0), m_Size(0)
{
    if (tokens.size() >= 2)
        m_MemAperture = tokens[1];
    if (tokens.size() >= 3)
        GetUINT64FromStr(tokens[2], &m_PhysAddress);
    if (tokens.size() >= 4)
        GetUINT32FromStr(tokens[3], &m_Size);
    if (tokens.size() >= 5)
        m_FileName = tokens[4];
}

RC LoadRawMemOp::Replay(const ReplayOptions& option, int* replayCount)
{
    std::vector<UINT08> data;

    if (!LoadDumpFile(m_FileName, &data, m_Size, true))
    {
        return RC::SOFTWARE_ERROR;
    }

    *replayCount = 1;
    if (Platform::IsTegra())
        return LoadViaDirectPhysWrite(data, option);
    if (m_PhysAddress + m_Size >= 0x8000000ULL)
        return LoadViaPramin(data, option);
    else
        return LoadViaBar1(data, option);
}

RC LoadRawMemOp::LoadViaBar1(const vector<UINT08> &data,
        const ReplayOptions& option)
{
    if (m_MemAperture != "FB")
    {
        Printf(Tee::PriError, "BAR1 loading only supports FB\n");
        return RC::BAD_PARAMETER;
    }

    UINT64 bar0Base = m_Trace->GetBaseAddr("bar0").first;
    UINT32 oldBar1Window = 0;

    Platform::PhysRd(bar0Base + LW_PBUS_BAR1_BLOCK, &oldBar1Window, 4);

    UINT32 newBar1Setting = 0;
    Platform::PhysWr(bar0Base + LW_PBUS_BAR1_BLOCK, &newBar1Setting, 4);

    UINT64 bar1Base = m_Trace->GetBaseAddr("bar1").first;
    UINT64 address = m_PhysAddress + bar1Base;
    Platform::PhysWr(address, &data[0], m_Size);

    Platform::PhysWr(bar0Base + LW_PBUS_BAR1_BLOCK, &oldBar1Window, 4);
    return OK;
}

RC LoadRawMemOp::LoadViaPramin(const vector<UINT08> &data,
        const ReplayOptions& option)
{
    if (m_MemAperture != "FB")
        return RC::BAD_PARAMETER;

    UINT64 bar0Base = m_Trace->GetBaseAddr("bar0").first;
    UINT32 oldBar0Window;

    // Read the BAR0 window so the current value can be restored later.
    Platform::PhysRd(bar0Base + LW_PBUS_BAR0_WINDOW, &oldBar0Window, 4);

    UINT32 remaining = m_Size;
    UINT64 writeAddr = m_PhysAddress;
    const UINT32 BAR0_WINDOW_SIZE = 0x100000;

    UINT32 writeBackSize = 0;
    UINT32 dataOffset = 0;
    while (remaining)
    {
        UINT32 newBar0Window = static_cast<UINT32>(writeAddr>> 16);
        UINT32 offset = static_cast<UINT32>(writeAddr & 0xffffLL);
        Platform::PhysWr(bar0Base + LW_PBUS_BAR0_WINDOW, &newBar0Window, 4);

        UINT32 sizeToWrite = min(BAR0_WINDOW_SIZE - offset, remaining);

        Platform::PhysWr(bar0Base + DEVICE_BASE(LW_PRAMIN) + offset,
                &data[dataOffset], sizeToWrite);

        writeBackSize += sizeToWrite;

        if (remaining <= sizeToWrite)
            break;
        remaining -= sizeToWrite;
        writeAddr += sizeToWrite;
        dataOffset += sizeToWrite;
    }
    MASSERT(writeBackSize == m_Size);

    Platform::PhysWr(bar0Base + LW_PBUS_BAR0_WINDOW, &oldBar0Window, 4);

    return OK;
}

RC LoadRawMemOp::LoadViaDirectPhysWrite(const vector<UINT08> &data,
        const ReplayOptions& option)
{
    if (m_MemAperture != "sysmem")
    {
        Printf(Tee::PriError, "Direct Phys wr loading only supports sysmem\n");
        return RC::BAD_PARAMETER;
    }

    void *pMem = nullptr;
    Platform::MapDeviceMemory(&pMem, m_PhysAddress, m_Size, Memory::UC, Memory::ReadWrite);
    Platform::PhysWr(m_PhysAddress, &data[0], m_Size);
    Platform::UnMapDeviceMemory(pMem);

    return OK;
}

AnnotationOp::AnnotationOp(ChiplibTrace* trace,
        const std::vector<const char*>& tokens,
        const std::string& comment, int lineNum)
:TraceOp(trace, tokens, comment, lineNum), m_Annotation(""), m_NeedReplay(false)
{
    if (tokens.size() == 2)
        m_Annotation = tokens[1];
    if (m_OpStr.find("Checking image CRCs") != std::string::npos
            || m_OpStr.find("Assembling") != std::string::npos
            || m_OpStr.find("Callwlating CRC") != std::string::npos)
    {
        m_NeedReplay = true;
    }
}
RC AnnotationOp::Replay(const ReplayOptions& option, int* replayCount)
{
    if (m_OpStr.find("PerformanceMonitor::RegRead") != std::string::npos)
    {
        RC rc;
        RegExp re;
        char* endptr;
        // corresponds to printf in PerformanceMonitor::RegRead()
        // in mdiag/perfmon/perf_mon.cpp
        // example:
        // Info: PerformanceMonitor::RegRead gpc0_chiplet0.gpc0_chiplet0CycleCount.event_count(00180090) 0000fad0:ffffffff
        CHECK_RC(re.Set(".*PerformanceMonitor::RegRead (.*)\\((.*)\\) (.*):ffffffff",
                    RegExp::SUB));
        MASSERT(re.Matches(m_OpStr));

        UINT32 reg = (UINT32)strtoul(re.GetSubstring(2).c_str(), &endptr, 16);
        MASSERT(*endptr == '\0');
        UINT32 val = (UINT32)strtoul(re.GetSubstring(3).c_str(), &endptr, 16);
        MASSERT(*endptr == '\0');

        m_Trace->AddPmCapture(reg, val, re.GetSubstring(1));
    }

    *replayCount = 1;
    return OK;
}
Mask32ReadOp::Mask32ReadOp(ChiplibTrace* trace,
        const std::vector<const char*>& tokens,
        const std::string& comment, int lineNum)
:BusMemOp(trace, tokens, comment, lineNum),
    m_Mask(0), m_Polling(true), m_CompFunc(NULL)
{
    int startIdx = 0;
    if (string(tokens[1]) == "mode")
    {
        string mode(tokens[2]);
        startIdx = 2;

        if(mode.find("P") == 0)
            m_Polling = true;
        else if (mode.find("NP") == 0)
            m_Polling = false;
        else
            MASSERT(!"Unknow mode.");

        if (mode.find("_EQ") != string::npos)
            m_CompFunc = EqualTo;
        else if (mode.find("_GE") != string::npos)
            m_CompFunc = GreaterOrEqual;
        else if (mode.find("_LE") != string::npos)
            m_CompFunc = LessOrEqual;
        else if (mode.find("_G") != string::npos)
            m_CompFunc = GreaterThan;
        else if (mode.find("_L") != string::npos)
            m_CompFunc = LessThan;
        else
            MASSERT(!"Unknow mode.");
    }
    GetUINT32FromStr(tokens[startIdx + 1], &m_Mask);
    m_Aperture = tokens[startIdx + 2];
    GetUINT64FromStr(tokens[startIdx + 3], &m_Offset);
    for (size_t i = startIdx + 4; i < tokens.size(); i++)
    {
        UINT08 val;
        GetUINT08FromStr(tokens[i], &val);
        m_Data.push_back(val);
    }
    const static string pm_Read = "pm_Read";
    m_Pm = IsEndingWith(comment, pm_Read);
}
static void MaskData(const UINT08* src, UINT08* dst, UINT32 mask, size_t size)
{
    size_t dword_cnt = size/4;
    const UINT32* pSrc = (const UINT32*)src;
    UINT32* pDst = (UINT32*)dst;
    for(size_t i = 0; i < dword_cnt; ++i)
    {
        pDst[i] = mask & pSrc[i];
    }
    size_t remaining = size - dword_cnt * 4;
    UINT08* pMask = (UINT08*)(&mask);
    for (size_t i = 0; i < remaining; ++i, pMask++)
    {
        dst[dword_cnt * 4 + i] = pMask[i] & src[dword_cnt*4 + i];
    }
}
RC Mask32ReadOp::Replay(const ReplayOptions& option, int* replayCount)
{
    RC rc = OK;
    UINT64 addr = m_Trace->GetBaseAddr(m_Aperture).first + m_Offset;

    vector<UINT08> data(m_Data.size());
    vector<UINT08> maskTarget(m_Data.size());
    vector<UINT08> maskReadback(m_Data.size());

    UINT32* pData = (UINT32*)&m_Data[0];
    UINT32* pRtnData = (UINT32*)&data[0];
    MaskData((UINT08*)pData, &maskTarget[0], m_Mask, m_Data.size());
    unsigned int retryCnt = 0;
    size_t mismatchCnt = m_Data.size();
    std::string msg("");
    while(mismatchCnt)
    {
        int ret = ReplayRead(addr, &data[0], UNSIGNED_CAST(UINT32, m_Data.size()));

        // Read failure

        if (ret != 0)
        {
            Printf(Tee::PriNormal, "  Replay read failed!(%s)\n", m_OpStr.c_str());
        }
        else
        {
            if (m_Pm)
            {
                m_Trace->AddPmReplay(UNSIGNED_CAST(UINT32, m_Offset), *(UINT32*)(&data[0]));
            }

            MaskData((UINT08*)pRtnData, &maskReadback[0], m_Mask, m_Data.size());
            if (m_Mask)
            {
                string msg_type_str = Utility::StrPrintf("%s mask read. Addr: 0x%08llx",
                                                        m_Aperture.c_str(), addr);
                mismatchCnt = CompareData(&maskReadback[0], &maskTarget[0],
                        m_Data.size(), msg_type_str, msg, m_CompFunc);
            }
            else
            {
                mismatchCnt = 0;
            }
        }

        if (!m_Polling || !m_Trace->NeedRetry(retryCnt, option, m_Aperture)
                || mismatchCnt == 0 )
            break;

        m_Trace->WaitForRetry(option);

        m_Trace->PrintRetryMsg(msg, retryCnt+1, option.maxPollCnt + 2);
        if (mismatchCnt && retryCnt == 0 && m_Aperture == "bar0")
        {
            m_Trace->PrintRegister(m_Offset, *(UINT32*)(&data[0]),
                    *(UINT32*)(&m_Data[0]));
        }
        retryCnt++;
    }

    if(!mismatchCnt)
    {
        Printf(Tee::PriDebug, MSGID(), "\n%s", msg.c_str());
    }
    else
    {
        if (m_Aperture == "bar0")
            m_Trace->PrintRegister(m_Offset, *(UINT32*)(&data[0]),
                    *(UINT32*)(&m_Data[0]));
        if (IsMustMatchBusMemOp(option))
        {
            Printf(Tee::PriHigh, "Error: Mismatch happened on a must-match read.\n");
            rc = RC::VECTORDATA_VALUE_MISMATCH;
        }
    }

    if (m_Aperture == "bar1")
        m_Trace->CollectReplayData(m_Data.size(), mismatchCnt);

    m_Trace->CollectReplayStatistics(m_Data.size(), mismatchCnt,
            m_Aperture, ChiplibTrace::OP_READ);

    *replayCount = 1;
    return rc;
}
static UINT64 GetCfgBase(std::vector<const char*>::const_iterator begin,
        std::vector<const char*>::const_iterator end)
{
    UINT64 domain = 0;
    UINT64 bus, dev, func, addr;

    if (strcmp(*begin, "base") == 0)
    {
        UINT64 base = 0;
        ++begin;
        GetUINT64FromStr(*begin, &base);
        return base;
    }
    if (strcmp(*begin, "domain") == 0)
    {
        ++begin;
        GetUINT64FromStr((*begin++), &domain);
    }

    assert(strcmp(*begin++, "bus") == 0);
    GetUINT64FromStr((*begin++), &bus);

    assert(strcmp(*begin++, "dev") == 0);
    GetUINT64FromStr((*begin++), &dev);

    assert(strcmp(*begin++, "fun") == 0);
    GetUINT64FromStr((*begin++), &func);

    assert(strcmp(*begin++, "compaddr") == 0);
    GetUINT64FromStr((*begin++), &addr);
    assert(begin == end);

    addr = 0;

    return  (((addr & 0xf00) << 16) | (domain << 28) | (bus << 16) | (dev << 11) |
            (func << 8) | (addr & 0xff));
}
TagInfoOp::TagInfoOp(ChiplibTrace* trace, std::vector<const char*> tokens,
        const std::string& comment, int lineNum)
:TraceOp(trace, tokens, comment, lineNum)
{
    unsigned int startIdx = 0;
    if (strcmp("@", tokens[0]) == 0)
    {
        startIdx = 1;
        m_Tag = tokens[1];
    }
    else
    {
        const char* p = tokens[0];
        m_Tag = ++p;
    }

    if ( m_Tag.find("bar") == 0 || m_Tag.find("sysmem") == 0 )
    {
        UINT64 addr = 0;
        UINT64 size = 0;
        GetUINT64FromStr(tokens[startIdx + 2], &addr);
        if ( startIdx + 3 < tokens.size())
        {
            if ( strcmp(tokens[startIdx + 3], "size") == 0
                    && startIdx + 4 < tokens.size() )
                GetUINT64FromStr(tokens[startIdx + 4], &size);
            else
                GetUINT64FromStr(tokens[startIdx + 3], &size);
        }
        else
        {
            Printf(Tee::PriNormal, "Parse op error(%s)\n", m_OpStr.c_str());
        }
        trace->SetBaseAddr(m_Tag, addr, size);
    }
    else if ( m_Tag.find("cfg") == 0)
    {
        UINT64 addr = GetCfgBase(tokens.begin() + startIdx + 1, tokens.end());
        trace->SetBaseAddr(m_Tag, addr, 0);
    }
    else if (m_Tag.find("gpu_name") == 0)
    {
        trace->SetGpuName(tokens[1]);
    }
    else if (m_Tag.find("test_name") == 0)
    {
        trace->SetTestName(tokens[1]);
    }
    else if (m_Tag.find("trace_path") == 0)
    {
        trace->SetTracePath(tokens[1]);
    }
    else if (m_Tag.find("backdoor_archive_id") == 0)
    {
        trace->SetBackdoorArchiveId(tokens[1]);
    }

    if (strcmp(tokens[0], "@tag_begin") == 0)
    {
        SurfInfo info;
        bool valid = false;
        for(size_t i = 1; i < tokens.size(); ++i)
        {
            if (strcmp(tokens[i], "surfName") == 0)
            {
                info.name = tokens[++i];
                valid = true;
            }
            else if (strcmp(tokens[i],"surfFmt") == 0)
            {
                info.format = ColorUtils::StringToFormat(tokens[++i]);
            }
            else if (strcmp(tokens[i], "surfHeight") == 0)
            {
                GetUINT32FromStr(tokens[++i], &info.height);
            }
            else if (strcmp(tokens[i], "surfWidth") == 0)
            {
                GetUINT32FromStr(tokens[++i], &info.width);
            }
        }
        if (valid)
            m_Trace->SetLwrSurfInfo(info);
    }
    if (m_OpStr.find("tag_begin crc_surf_read") != std::string::npos
            || m_OpStr.find("tag_begin gpucacl_crc_read") != std::string::npos)
    {
        m_Trace->StartCRCChecking();
    }
    if (m_OpStr.find("tag_end crc_surf_read") != std::string::npos
            || m_OpStr.find("tag_begin gpucacl_crc_read") != std::string::npos)
    {
        const SurfInfo& info = m_Trace->GetLwrSurfInfo();
        m_Trace->EndCRCChecking(info.name);
    }
}

RC TagInfoOp::Replay(const ReplayOptions& option, int* replayCount)
{
    return OK;
}

void ChiplibTrace::PrintTraceOp(TraceOp* op, Tee::Priority level)
{
    Printf(level, MSGID(), "\nReplaying[%d]: ", op->GetLineNum());
    for(size_t  i = 0; i < op->GetOpStr().size(); i += Tee::MaxPrintSize)
    {
        Printf(level, MSGID(), "%.*s", Tee::MaxPrintSize,
                &((op->GetOpStr().c_str())[i]) );
    }
    Printf(level, MSGID(), " %s",  op->GetComment().c_str());
}

void ChiplibTrace::CollectReplayStatistics(size_t bytes, size_t mismatched,
        const std::string &aperture, OPTYPE opType)
{
    if(opType == ChiplibTrace::OP_READ)
    {
        if(aperture == "bar0")
        {
            m_Bar0Read += bytes;
            m_Bar0Mismatch += mismatched;
        }
        else if(aperture == "bar1")
        {
            m_Bar1Read += bytes;
            m_Bar1Mismatch += mismatched;
        }
        else if(aperture == "bar2")
        {
            m_Bar2Read += bytes;
            m_Bar2Mismatch += mismatched;
        }
        else if((aperture == "sysmem") || (aperture == "sysmem0"))
        {
            m_SysMemRead += bytes;
            m_SysMemMismatch += mismatched;
        }
    }
    else
    {
        if(aperture == "bar0")
        {
            m_Bar0Write += bytes;
        }
        else if(aperture == "bar1")
        {
            m_Bar1Write += bytes;
        }
        else if(aperture == "bar2")
        {
            m_Bar2Write += bytes;
        }
        else if(aperture == "sysmem")
        {
            m_SysMemWrite += bytes;
        }
    }
    m_Transactions++;
}

void ChiplibTrace::SetGpuName(const string& gpuName)
{
    m_GpuName = gpuName;
    if (!m_RefManual)
    {
        m_RefManual = new RefManual();
        std::string filename(gpuName + "_ref.txt");
        transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        //Try parsing, if failure, don't try again
        RC rc = m_RefManual->Parse(filename.c_str());
        if (rc != OK) // need a way to exit?
        {
            delete m_RefManual;
            m_RefManual = NULL;
            Printf(Tee::PriHigh, "Could not parse %s\n", filename.c_str());
        }
    }
}

RC ChiplibTrace::Report(std::string& message)
{
    if (IsInsideCRCReading())
    {
        const SurfInfo& info = GetLwrSurfInfo();
        EndCRCChecking(info.name);
    }

    UINT64 bytes_read = 0;
    UINT64 bytes_mismatch = 0;
    for (std::map<std::string, void*>::const_iterator it = m_MappedApertures.begin();
            it != m_MappedApertures.end(); ++it)
    {
        Platform::UnMapDeviceMemory(it->second);
    }

    std::map<UINT32, std::pair<UINT32, UINT32> >::const_iterator pm_End = m_PmsAll.end();
    for (std::map<UINT32, std::pair<UINT32, UINT32> >::const_iterator it = m_PmsAll.begin();
            it != pm_End; ++it)
    {
        Printf(Tee::PriNormal, "PM @ 0x%08x: capture: 0x%08x replay: 0x%08x",
                it->first, it->second.first, it->second.second);
        if (m_PmsCheck.find(it->first) != m_PmsCheck.end())
        {
            Printf(Tee::PriNormal, " [must match within %g%%]\n",
                    (100 * m_PmTolerance));
        }
        else
        {
            Printf(Tee::PriNormal, "\n");
        }
    }

    RC test_status;
    std::set<UINT32>::const_iterator check_end = m_PmsCheck.end();
    for (std::set<UINT32>::const_iterator it = m_PmsCheck.begin();
            it != check_end; ++it)
    {
        MASSERT(m_PmsAll.find(*it) != m_PmsAll.end());
        if (std::abs(static_cast<int>(m_PmsAll[*it].first - m_PmsAll[*it].second)) >
                int(m_PmTolerance * m_PmsAll[*it].first)
             || std::abs(static_cast<int>(m_PmsAll[*it].first - m_PmsAll[*it].second)) >
                int(m_PmTolerance * m_PmsAll[*it].second))
        {
            Printf(Tee::PriNormal,
                    "Capture and replay PM vaues of %#08x are too different: "
                    "capture=0x%08x replay=0x%08x\n",
                    *it, m_PmsAll[*it].first, m_PmsAll[*it].second);

            test_status = RC::GOLDEN_VALUE_MISCOMPARE;
        }
    }

    Printf(Tee::PriNormal, "\n\nReplayer end : ");
    if (m_ReplayCnt == 0)
    {
        Printf(Tee::PriNormal,
                "No trace op was replayed! Please check the trace file!\n");
    }
    else
    {
        Printf(Tee::PriNormal, "%d trace ops were replayed!\n",
                static_cast<UINT32>(m_ReplayCnt));
    }
    for (std::map<std::string, std::pair<UINT64, UINT64> >::const_iterator it = m_ReplayStat.begin();
            it != m_ReplayStat.end(); ++it)
    {
        Printf(Tee::PriNormal, "Replayed CRC check: %s, %llu bytes read, %llu bytes mismatch\n",
                it->first.c_str(), it->second.first, it->second.second);
        bytes_read += it->second.first;
        bytes_mismatch += it->second.second;
    }

    Printf(Tee::PriNormal, "Bar0: %u bytes read, %u bytes written, %u bytes mismatched\n",
                           static_cast<UINT32>(m_Bar0Read),
                           static_cast<UINT32>(m_Bar0Write),
                           static_cast<UINT32>(m_Bar0Mismatch));
    Printf(Tee::PriNormal, "Bar1: %u bytes read, %u bytes written, %u bytes mismatched\n",
                           static_cast<UINT32>(m_Bar1Read),
                           static_cast<UINT32>(m_Bar1Write),
                           static_cast<UINT32>(m_Bar1Mismatch));
    Printf(Tee::PriNormal, "Bar2: %u bytes read, %u bytes written, %u bytes mismatched\n",
                           static_cast<UINT32>(m_Bar2Read),
                           static_cast<UINT32>(m_Bar2Write),
                           static_cast<UINT32>(m_Bar2Mismatch));
    Printf(Tee::PriNormal, "SysMem: %u bytes read, %u bytes written, %u bytes mismatched\n",
                           static_cast<UINT32>(m_SysMemRead),
                           static_cast<UINT32>(m_SysMemWrite),
                           static_cast<UINT32>(m_SysMemMismatch));
    Printf(Tee::PriNormal, "Total transations replayed: %u\n",
            static_cast<UINT32>(m_Transactions));
    Printf(Tee::PriNormal, "CRC: %llu bytes read, %llu bytes mismatched\n",
            bytes_read, bytes_mismatch);

    if (bytes_mismatch == 0 && bytes_read > 100)
    {
        Printf(Tee::PriNormal, "Passed Replay!\n");
    }
    else
    {
        Printf(Tee::PriNormal, "FAILED Replay!\n");
        test_status = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    return test_status;
}

ChiplibTrace::ChiplibTrace(const ReplayOptions& option, const string& filename)
    : m_RefManual(filename != "" ? new RefManual() : 0),
    m_Option(option), m_MismatchCnt(0), m_ReplayCnt(0),
    m_Bar0Read(0), m_Bar1Read(0), m_Bar2Read(0), m_SysMemRead(0),
    m_Bar0Write(0), m_Bar1Write(0), m_Bar2Write(0), m_SysMemWrite(0),
    m_Bar0Mismatch(0), m_Bar1Mismatch(0), m_Bar2Mismatch(0), m_SysMemMismatch(0), m_Transactions(0),
    m_MappedApertureCacheBase(0), m_MappedApertureCacheSize(0),
    m_StartCRCReading(false), m_TotalSize(0), m_TotalDiff(0)
{
    if (m_RefManual)
    {
        RC rc = m_RefManual->Parse(filename.c_str());
        if (rc != OK) // need a way to exit?
        {
            delete m_RefManual;
            m_RefManual = NULL;
            Printf(Tee::PriHigh, "Could not parse %s\n", filename.c_str());
        }
    }
}

ChiplibTrace::~ChiplibTrace()
{
    if (m_RefManual)
    {
        delete m_RefManual;
    }

    std::vector<Replayer::TraceOp*>::iterator end = m_TraceOps.end();
    for (std::vector<Replayer::TraceOp*>::iterator it = m_TraceOps.begin();
            it != end; ++it)
    {
        delete *it;
    }
}

void ChiplibTrace::SetBaseAddr(const std::string& aperture, UINT64 addr, UINT64 size)
{
    m_BaseAddrMap.insert(std::make_pair(aperture, std::make_pair(addr, size)));
    m_BaseAddrArray.push_back(std::make_pair(aperture, std::make_pair(addr, size)));
}
std::pair<UINT64,UINT64> ChiplibTrace::GetBaseAddr(const std::string& aperture) const
{
    std::map<std::string, std::pair<UINT64, UINT64> >::const_iterator it =
        m_BaseAddrMap.find(aperture);
    if(it != m_BaseAddrMap.end())
        return it->second;
    else
    {
        UINT32 idx = ~0;
        GetUINT32FromStr(aperture.c_str(), &idx);
        if (m_BaseAddrArray.size() > idx)
            return m_BaseAddrArray[idx].second;
        else
        {
            Printf(Tee::PriNormal, "WARNING: unknown aperture \"%s\".\n", aperture.c_str());
            return std::make_pair(0,0);
        }
    }
}
const std::string& ChiplibTrace::GetRealAperture(const std::string& aperture)
{
    UINT32 idx = ~0;
    GetUINT32FromStr(aperture.c_str(), &idx);
    if (m_BaseAddrArray.size() > idx)
        return m_BaseAddrArray[idx].first;
    else
    {
        return aperture;
    }
}
RC ChiplibTrace::AddTraceOp(TraceOp* op, bool isOptional)
{
    if (!op)
        return RC::BAD_PARAMETER;

    if (m_Option.inplaceReplay)
    {
        if (!isOptional || (isOptional && m_Option.replayOptional))
        {
            if (op->GetOpStr().find("tag_") == string::npos)
            {
                PrintTraceOp(op, Tee::PriDebug);
            }
            else
            {
                PrintTraceOp(op, Tee::PriNormal);
            }
            int replayCount = 0;
            RC rc = op->Replay(m_Option, &replayCount);
            delete op;

            if (rc == OK)
            {
                m_ReplayCnt += replayCount;
            }
            else
            {
                return rc;
            }
        }
        else
        {
            Printf(Tee::PriNormal, "\nIgnoring optional: %s", op->GetOpStr().c_str());
            delete op;
        }
    }
    else
        m_TraceOps.push_back(op);

    return OK;
}
RC ChiplibTrace::Replay()
{
    if (!m_Option.inplaceReplay)
    {
        std::vector<Replayer::TraceOp*>::iterator it = m_TraceOps.begin();
        for (; it != m_TraceOps.end(); ++it)
        {
            PrintTraceOp(*it, Tee::PriDebug);
            int replayCount = 0;
            RC rc  = (*it)->Replay(m_Option, &replayCount);

            if (rc != OK)
            {
                return rc;
            }
        }
    }
    return OK;
}
unsigned int ChiplibTrace::GetApertureID(const std::string& aperture)
{
    std::map<std::string, unsigned int>::const_iterator it = m_ApertureMap.find(aperture);
    if (it != m_ApertureMap.end())
        return it->second;
    else
    {
        Printf(Tee::PriNormal, "WARNING: invalid aperture \"%s\".\n", aperture.c_str());
        return ~0;
    }
}
void ChiplibTrace::MapDeviceMemory(const std::string& aperture, UINT64 addr, UINT64 size)
{
    if (size == 0)
    {
        Printf(Tee::PriNormal,
                "Specified size is 0, will not map device memory for \"%s\".\n",
                aperture.c_str());
        return;
    }

    if ((addr < m_MappedApertureCacheBase)
            || (addr >= m_MappedApertureCacheBase + m_MappedApertureCacheSize)
            || (addr + size > m_MappedApertureCacheBase + m_MappedApertureCacheSize))
    {
        // not find it in most recently used cache
        // search all mapped aperture
        if (m_MappedApertures.find(aperture) != m_MappedApertures.end())
        {
            m_MappedApertureCacheBase = addr;
            m_MappedApertureCacheSize = size;
            return;
        }
    }
    else
    {
        // hit most recently used cache
        return;
    }

    Printf(Tee::PriNormal, "%s: %s, 0x%llx, 0x%llx\n", __FUNCTION__, aperture.c_str(), addr, size);

    void * pMem = 0;
    Platform::MapDeviceMemory(&pMem, static_cast<PHYSADDR>(addr), size, Memory::UC, Memory::ReadWrite);
    m_MappedApertures.insert(std::make_pair(aperture, pMem));
    m_MappedApertureCacheBase = addr;
    m_MappedApertureCacheSize = size;
}
void* ChiplibTrace::GetMappedVirtualAddr(const std::string& aperture) const
{
    std::map<std::string, void*>::const_iterator it = m_MappedApertures.find(aperture);
    if (it == m_MappedApertures.end())
        return NULL;
    return it->second;
}
void ChiplibTrace::PrintRegister(UINT64 offset,
        UINT32 data, UINT32 expected_data) const
{
    if (!m_RefManual)
        return;

    if (offset > UINT64(~UINT32(0)))
    {
        Printf(Tee::PriNormal, "%s offset %#08X is too large\n", m_Bar0Prefix.c_str(), UINT32(offset));
        return;
    }

    const RefManualRegister* reg = m_RefManual->FindRegister(UINT32(offset));
    if (reg)
    {
        Printf(Tee::PriNormal, "%sR %s (%#08X)\n", m_Bar0Prefix.c_str(),
                reg->GetName().c_str(), UINT32(offset));
        size_t max_field = 0;
        for (int i = 0; i < reg->GetNumFields(); ++i)
        {
            max_field = max(max_field, reg->GetField(i)->GetName().size());
        }

        size_t reg_name_size = reg->GetName().size() + 1;
        max_field += 2;

        for (int i = 0; i < reg->GetNumFields(); ++i)
        {
            const RefManualRegisterField* fiield = reg->GetField(i);
            Printf(Tee::PriNormal, "%sF  %s%s%#08X %s\n", m_Bar0Prefix.c_str(),
                    fiield->GetName().substr(reg_name_size).c_str(),
                    string(max_field - fiield->GetName().size(), ' ').c_str(),
                    ((data & fiield->GetMask()) >> fiield->GetLowBit()),
                    (((data & fiield->GetMask()) == (expected_data & fiield->GetMask()))
                     ? "  OK" : "  FAIL") );
        }
    }
    else
    {
        Printf(Tee::PriNormal, "%s cannot find register with offset %#08X",
                m_Bar0Prefix.c_str(), UINT32(offset));
    }
}

void ChiplibTrace::AddPmCapture(UINT32 address, UINT32 value,
        const string& clock_name)
{
    if (m_PmsAll.find(address) == m_PmsAll.end())
    {
        m_PmsAll[address] = std::make_pair(value, 0);
    }
    else
    {
        m_PmsAll[address].first = value;
    }

    if (clock_name == m_PmClockCheck)
    {
        m_PmsCheck.insert(address);
    }
}

void ChiplibTrace::AddPmReplay(UINT32 address, UINT32 value)
{
    if (m_PmsAll.find(address) == m_PmsAll.end())
    {
        m_PmsAll[address] = std::make_pair(0, value);
    }
    else
    {
        m_PmsAll[address].second = value;
    }
}
}

void WriteDataToBmp(const UINT08* data, UINT32 width, UINT32 height,
        ColorUtils::Format format, const std::string& surfName)
{
    if (!IsSupportedFormat(format))
    {
        Printf(Tee::PriDebug, MSGID(), "Unsupported format in WriteDataToBmp");
        return;
    }
    FILE* dumpFp = NULL;
    Utility::OpenFile(surfName + ".bmp", &dumpFp, "w");
    UINT32 padding = (4-((width * 3) % 4)) % 4;
    UINT32 imagesize = (width * 3 + padding) * height;
    UINT32 filesize = 64 + imagesize;

    unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0,0,0, 64,0,0,0};
    unsigned char bmpinfoheader[50] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,24,0};
    unsigned char bmppad[3] = {0,0,0};

    bmpfileheader[ 2] = (unsigned char)(filesize    );
    bmpfileheader[ 3] = (unsigned char)(filesize>> 8);
    bmpfileheader[ 4] = (unsigned char)(filesize>>16);
    bmpfileheader[ 5] = (unsigned char)(filesize>>24);

    bmpinfoheader[ 4] = (unsigned char)(width    );
    bmpinfoheader[ 5] = (unsigned char)(width>> 8);
    bmpinfoheader[ 6] = (unsigned char)(width>>16);
    bmpinfoheader[ 7] = (unsigned char)(width>>24);
    bmpinfoheader[ 8] = (unsigned char)(height    );
    bmpinfoheader[ 9] = (unsigned char)(height>> 8);
    bmpinfoheader[10] = (unsigned char)(height>>16);
    bmpinfoheader[11] = (unsigned char)(height>>24);

    size_t errors = 0;
    errors += 14 - fwrite(bmpfileheader,1,14,dumpFp);
    errors += 50 - fwrite(bmpinfoheader,1,50,dumpFp);

    UINT32 rgba( 0 );
    unsigned char r, g, b;
    UINT32 bpp = ColorUtils::PixelBytes(format);
    for ( INT32 y = height-1; y > -1; --y )
    {
        for ( UINT32 x = 0; x < width; ++x )
        {
            UINT32 rawPixel = 0;
            int base = y * width * bpp + x * bpp;
            for (UINT32 ii = 0; ii < bpp; ++ ii)
            {
                rawPixel += data[base + ii] << (8 * ii);
            }

            rgba = ColorUtils::Colwert(rawPixel, format, ColorUtils::R8G8B8A8);

            if (format == ColorUtils::Z24S8)
            {
                // only reserv high 8bits
                char zero = 0;
                char z = (unsigned char)(rgba >> 24);
                char s = (unsigned char)(rgba & 0xFF);

                errors += 1 - fwrite(&s,1,1,dumpFp);
                errors += 1 - fwrite(&zero,1,1,dumpFp);
                errors += 1 - fwrite(&z,1,1,dumpFp);
            }
            else
            {
                r = (unsigned char) (rgba >> 24);
                g = (unsigned char) (rgba >> 16);
                b = (unsigned char) (rgba >> 8);

                errors += 1 - fwrite(&b,1,1,dumpFp);
                errors += 1 - fwrite(&g,1,1,dumpFp);
                errors += 1 - fwrite(&r,1,1,dumpFp);
            }
        }
        errors += padding - fwrite(bmppad,1,padding,dumpFp);
    }
    fclose(dumpFp);

    if (errors)
    {
        Printf(Tee::PriHigh, "Error writing data to file %s.bmp\n", surfName.c_str());
    }
}
