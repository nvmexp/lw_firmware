/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   bgmonitor.h
 *
 * @brief  Background monitor base class.
 *
 */

#pragma once

#include "gpu/utility/bglogger.h"
#include "inc/bytestream.h"
#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "core/include/massert.h"
#include "core/include/tasker.h"
#include "core/include/xp.h"
#include <limits>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <functional>

//-----------------------------------------------------------------------------
class BgMonitor
{
public:
    enum ValueType
    {
        ANY,
        FLOAT,
        INT,
        STR,
        INT_ARRAY
    };

    struct Sample
    {
        struct SampleItem
        {
            union Value
            {
                INT64 intValue[1];
                float floatValue[1];
                char  stringValue[1];
            };

            ValueType type;
            UINT32    size;
            UINT32    numElems;
            Value     value;
        };

        Mle::Context  mleContext;             // Timestamp, GPU, test, etc.
        vector<char>  data;
        UINT32        numDataItems = 0;
        UINT64        timeDeltaNs  = 0;       // Time delta from the previous sample
        BgMonitor*    pMonitor     = nullptr; // Bg monitor which generated this sample
        UINT32        devIdx       = 0;       // Index of device (GPU, LWSwitch)

        Sample()                         = default;
        Sample(Sample&&)                 = default;
        Sample& operator=(Sample&&)      = default;

        void Push(float value);
        void Push(UINT32 value);
        void Push(INT64 value);
        void Push(const INT64* pValues, UINT32 numElems);
        void Push(const char* str, UINT32 len);
        void Push(UINT16 value)
        {
            Push(static_cast<UINT32>(value));
        }
        void Push(INT32 value)
        {
            Push(static_cast<INT64>(value));
        }
        void Push(const char* str)
        {
            Push(str, static_cast<UINT32>(strlen(str)));
        }
        void Push(const string& str)
        {
            Push(str.data(), static_cast<UINT32>(str.size()));
        }

        void ClearData()
        {
            data.clear();
            numDataItems = 0;
        }

        class ConstIterator
        {
            public:
                ConstIterator(const vector<char>& data, size_t index);
                ConstIterator(ConstIterator&&) = default;
                ConstIterator& operator=(ConstIterator&&) = default;
                ConstIterator& operator++();
                bool operator==(const ConstIterator& other) const
                {
                    return (m_pData == other.m_pData) && (m_Index == other.m_Index);
                }
                bool operator!=(const ConstIterator& other) const
                {
                    return ! (*this == other);
                }
                const SampleItem& operator*() const
                {
                    return *reinterpret_cast<const SampleItem*>(&(*m_pData)[m_Index]);
                }
                const SampleItem* operator->() const
                {
                    return reinterpret_cast<const SampleItem*>(&(*m_pData)[m_Index]);
                }

            private:
                const vector<char>* m_pData;
                size_t              m_Index;
        };

        ConstIterator begin() const { return ConstIterator(data, 0); }
        ConstIterator end()   const { return ConstIterator(data, data.size()); }
        ConstIterator cbegin() const { return begin(); }
        ConstIterator cend()   const { return end(); }

        //! Extracts all data items in this sample and colwerts them to strings.
        //!
        //! @param pStrings Vector of strings, to be filled out by this function.
        //!
        //! @param type     Expected type of the data.  This may serve a purpose
        //!                 of additional check whether the data was output correctly
        //!                 by GatherData(), or it can be set to ANY.  If
        //!                 the background monitor produces data of multiple
        //!                 types, then it needs to be set to ANY.
        //!
        //! @param precision Precision to be used when formatting FLOAT data
        //!                  items.  For other types this parameter is ignored.
        //!
        RC FormatAsStrings(vector<string>* pStrings, ValueType type, int precision) const;

        private:
            SampleItem* AllocItem(ValueType type, UINT32 numElems);
    };

    struct SampleDesc
    {
        string    columnName;
        string    unit;
        bool      summarize;
        ValueType dataFormat;
    };

    BgMonitor(BgMonitorType type, string name)
        : m_Type(type), m_Name(move(name))
    {
    }

    virtual ~BgMonitor() = default;

    BgMonitorType GetType() const { return m_Type; }

    //! Returns name of this background monitor
    const string& GetName() const { return m_Name; }

    //! Returns description of each data entry (column) in the produced samples
    virtual vector<SampleDesc> GetSampleDescs(UINT32 devIdx) = 0;

    //! Collects information
    //!
    //! The implementation shall perform a minimum amount of work to collect
    //! sample data and call pSample->Push() for each data item.  It may collect
    //! just a single piece of data, or multiple pieces of data (e.g. voltage
    //! and current).  Each piece of collected data may have an individual type
    //! (e.g. FLOAT32, UINT32, etc.).
    //!
    //! The implementation shall not perform any additional operations, such
    //! as printing to MLE or log, etc.
    //!
    //! @param devIdx  Device index.  If IsSubdevBased() returns true, this will
    //!                be a GPU instance number.  If IsLwSwitchBased() returns true,
    //!                this will be an LwSwitch device number.  For other monitors
    //!                this is just a conselwtive integer from 0 to
    //!                GetNumDevices()-1.
    //!
    //! @param pSample Pointer to a sample to be filled up with data.  The sample
    //!                is already primed with all the necessary information and
    //!                the monitor only has to call pSample->Push() to add the
    //!                data values.  The monitor may read other members from pSample,
    //!                but it should avoid modifying them.  Some monitors may find
    //!                it useful to use pSample->timeDeltaNs, which is the amount
    //!                of time that has passed since the last data collection, in ns.
    //!
    //! @return RC to indicate success or failure.  If this function fails, the
    //!         thread for this background monitor will exit.
    //!
    virtual RC GatherData(UINT32 devIdx, Sample* pSample) = 0;

    //! Colwerts sample data to a printable list of strings.
    //!
    //! The input sample already contains data items produced by GatherData().
    //! GetPrintable() extracts each data item from the sample by calling
    //! pSample->FormatAsStrings() to colwert each extracted
    //! data item to an individual string.  The background logger infrastructure
    //! takes care of formatting these data items and printing them where
    //! relevant.
    //!
    //! The default implementation calls FormatAsStrings() with type ANY
    //! and precision 4.  Individual background monitors may choose to leave
    //! the default implementation or to override this function and produce
    //! the strings in a different way.
    //!
    //! @param sample   Data sample to colwert to strings.  The data sample was
    //!                 collected by GatherData() and was not modified since.
    //!
    //! @param pStrings Vector of strings which will be filled by this function.
    //!                 Each data item inside the sample will become a separate
    //!                 string.
    //!
    //! @return RC to indicate success or failure.  If this function fails, the
    //!         thread which does the printing for all the monitors will exit.
    //!
    virtual RC GetPrintable(const Sample& sample, vector<string>* pStrings);

    //! This is mainly for formating the print in MonitorThread
    virtual bool IsSubdevBased() = 0;

    //! This is mainly for formating the print in MonitorThread
    virtual bool IsLwSwitchBased() = 0;

    //! Tells the running thread whether it needs to obey pausing.
    //! Most bg monitors touch GPUs, so they need to obey pausing to avoid
    //! touching the GPUs e.g. during GCx tests when the GPUs are shut down.
    //! A few loggers don't touch GPUs at all, like the CPU usage logger.
    virtual bool IsPauseable() { return true; }

    //! Go through the collected data and see if there's something wrong.
    // not sure if this really is needed now that we have ErrorCounter
    virtual RC StatsChecker() { return OK; };

    //! This reports the number of registered devices (eg. num syscon, num GPU
    // that has successfully initialized)
    virtual UINT32 GetNumDevices() = 0;

    //! This reports whether the monitor is actually monitoring the specified
    //! index (i.e. for GPU monitors, one GPU may be monitored while another
    //! may not)
    virtual bool IsMonitoring(UINT32 Index) = 0;

    //! Returns true if this monitor is printing samples
    bool IsPrinting() const
    {
        return m_Priority != Tee::PriNone;
    }

    INT32 GetStatsPri() const
    {
        return m_StatsPriority;
    }

    //! Initializes the bg monitor.
    //!
    //! This can also set the Min/Max for Stats checker if required.
    RC Initialize(const vector<UINT32> &Param, const set<UINT32> &MonitorDevices)
    {
        if (Param.size() != 0 && Param[0])
        {
            m_Priority = Param[0] & BgLogger::FLAG_NO_PRINTS ? Tee::PriNone : Tee::PriNormal;
            m_StatsPriority = Param[0] & BgLogger::FLAG_DUMPSTATS ? Tee::PriNormal : Tee::PriNone;
        }
        if (BgLogger::d_MleOnly)
        {
            m_Priority = Tee::PriNone;
        }
        return InitializeImpl(Param, MonitorDevices);
    }

    //! Initializes the bg monitor from params passed from JS
    //!
    //! This can also set the Min/Max for Stats checker if required.
    RC InitFromJs(const JsArray &Param, const set<UINT32> &MonitorDevices)
    {
        if (Param.size() != 0)
        {
            RC rc;
            JavaScriptPtr js;

            UINT32 flags; CHECK_RC(js->FromJsval(Param[0], &flags));

            m_Priority = flags & BgLogger::FLAG_NO_PRINTS ? Tee::PriNone : Tee::PriNormal;
            m_StatsPriority = flags & BgLogger::FLAG_DUMPSTATS ? Tee::PriNormal : Tee::PriNone;
        }
        if (BgLogger::d_MleOnly)
        {
            m_Priority = Tee::PriNone;
        }
        return InitFromJsImpl(Param, MonitorDevices);
    }

    //! Ilwoked right before the bg monitor thread exits.
    //!
    //! Bg monitor implementations can choose to implement this optional function.
    virtual RC Shutdown()
    {
        return RC::OK;
    }

    template <typename numType>
    struct Range
    {
        numType min;
        numType max;
        numType last;

        Range()
        {
            last = 0;
            min = (std::numeric_limits<numType>::max)();
            max = (std::numeric_limits<numType>::min)();
        }

        void Update(numType num)
        {
            if (num > max) { max = num; }
            if (num < min) { min = num; }
            last = num;
        }
    };

private:
    //! brief: basic setups for the type of background monitor
    // this also sets the Min/Max for Stats checker if required
    virtual RC InitializeImpl(const vector<UINT32> &Param,
                              const set<UINT32>    &MonitorDevices)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    //! brief: basic setups for the type of background monitor
    // this also sets the Min/Max for Stats checker if required
    virtual RC InitFromJsImpl(const JsArray     &Param,
                              const set<UINT32> &MonitorDevices)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    BgMonitorType m_Type;
    string        m_Name;
    Tee::Priority m_Priority      = Tee::PriNormal;
    Tee::Priority m_StatsPriority = Tee::PriNone;
};
