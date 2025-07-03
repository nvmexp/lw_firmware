/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_CHANNELALLOCINFO_H
#define INCLUDED_CHANNELALLOCINFO_H

#include "mdiag/utils/types.h"

#include <tuple>
#include <list>
#include <map>
#include <vector>
#include <string>

class GpuSubdevice;
class TraceChannel;
class PmChannel;
class Channel;
class LWGpuTsg;
class VaSpace;
class PmVaSpace;
class SubCtx;
class LwRm;
class GpuDevice;
class LWGpuChannel;
class SmcEngine;

/*
 * ChannelAllocInfo is a helper class which likes BuffInfo and used to print registered
 * channel info including Instance memory properties, va space,
 * subctx info and etc.
 * All the info will be printed in a table in which first line is a header describing each column.
 * */
class ChannelAllocInfo
{
public:
    // Column definitions
    enum COL_ID
    {
        COL_NAME = 0,
        COL_HCHANNEL,
        COL_CHID,
        COL_INSTAPER,
        COL_INSTPA,
        COL_INSTSIZE,
        COL_TSG,
        COL_VASPACE,
        COL_SUBCTX,
        COL_VEID,
        COL_ENGINE,
        COL_NUM, // number of columns in the table.
    };

    // Each row is a Entry
    struct Entry
    {
        Entry()
        : m_Columns(COL_NUM, "n/a")
        {
        };

        Entry(const Entry&) = default;
        Entry(Entry&&) = default;
        ~Entry() = default;

        std::vector<std::string> m_Columns;
        bool m_Printed = false;
        bool m_Header = false;
    };

    using Entries = std::list<Entry>;

    ChannelAllocInfo();
    ~ChannelAllocInfo() = default;

    // Core function.
    // Channels which need to be printed should be registered via AddChannel.
    void AddChannel(PmChannel* pPmChannel, GpuSubdevice *pSubGpuDevice, UINT32 engineId);
    void AddChannel(LWGpuChannel* pLWGpuChannel);

    // Print function will output a table including all the registered channels
    // and if m_PrintOnce is set to true, it will only output not printed channels
    void Print(const string& subtitle = "");
    void SetPrintOnce(bool printOnce) { m_PrintOnce = printOnce; }

private:
    // Initialize first line which describes each column
    void InitHeader();

    // below functions are used to assemble columns
    void SetName(const std::string& name);
    void SetChannel(Channel* pChannel, GpuSubdevice *pSubGpuDevice, LwRm* pLwRm);
    void SetTsg(const LWGpuTsg* pTsg);
    void SetVaSpace(VaSpace* pVaSpace);
    void SetSubCtx(SubCtx* pSubCtx);
    void SetEngine(UINT32 engineId, GpuDevice* pGpuDevice, SmcEngine* pSmcEngine);

    // Keep tracking size of each column and make sure the final printing looks
    // pretty
    void UpdateColumnMaxLength(COL_ID colID, size_t length);
    bool IsPrintNeeded() const;

    string InstApertureToString(UINT32 instAperture);

    static const int m_EntryLength = 128; // max entry length
    static const std::map<COL_ID, std::string> m_ColumnName;
    std::vector<UINT32> m_MaxEntryLength;
    Entries m_Entries;

    // if m_PrintOnce is true, those printed channels will not be printed again
    bool m_PrintOnce = false;
};

#endif //INCLUDED_CHANNELALLOCINFO_H

