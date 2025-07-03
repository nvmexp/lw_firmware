/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2016-2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "tasker.h"
#include "tee.h"

class ByteStream;

/**
 * @class( DataSink )
 *
 * Abstract base class that consumes Tee's stream
 * and prints it to the destination.
 */

class DataSink
{
    public:
        DataSink();
        virtual ~DataSink() = default;
        void Print(const char* pStr, size_t strLen)
        {
            Print(pStr, strLen, nullptr, nullptr, Tee::SPS_NORMAL);
        }
        void Print(const char* pStr, size_t strLen,  const char* pLinePrefix)
        {
            Print(pStr, strLen, pLinePrefix, nullptr, Tee::SPS_NORMAL);
        }
        void Print(const char* pStr, size_t strLen, const char* pLinePrefix, const ByteStream* pBinaryPrefix)
        {
            Print(pStr, strLen, pLinePrefix, pBinaryPrefix, Tee::SPS_NORMAL);
        }
        void Print(const char* pStr, size_t strLen, const ByteStream* pBinaryPrefix)
        {
            Print(pStr, strLen, nullptr, pBinaryPrefix, Tee::SPS_NORMAL);
        }
        void Print(const char*           pStr,
                   size_t                strLen,
                   const char*           pLinePrefix,
                   const ByteStream*     pBinaryPrefix,
                   Tee::ScreenPrintState sps);

        virtual bool PrintMleHeader() { return DoPrintMleHeader(); }
        void PrintRaw(const ByteStream& data);
        void PrintMle(ByteStream* pData, Tee::Priority pri, const Mle::Context& ctx);

        void PostTaskerInitialize();
        void PreTaskerShutdown();

        virtual bool IsSinkOpen() { return true; }

    protected:
        bool m_bLineStart = true;
        Tasker::Mutex m_Mutex = Tasker::NoMutex();
        virtual void DoPrint(const char *pStr, size_t strLen, Tee::ScreenPrintState sps) = 0;
        virtual bool DoPrintBinary(const UINT08* data, size_t size) { return false; }
        bool DoPrintMleHeader();

    private:
        virtual void CacheOutput(const char* pStr, size_t size) { }

        Mle::Context  m_PrevContext;
        Tee::Priority m_PrevPri = Tee::PriNone;
};
