/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCODEFUZZ_MQDATAINTERFACE_H
#define UCODEFUZZ_MQDATAINTERFACE_H

//!
//! \file mqdatainterface.h
//! \brief A message queue (MQ) interface to a data source (such as libFuzzer)
//!

#include "rtosucodefuzzdriver.h"

class MqDataInterface : public FuzzDataInterface
{
public:
    MqDataInterface() = default;
    virtual ~MqDataInterface();
    virtual RC Initialize(LwU32 maxSize) override;
    virtual RC NextBytes(std::vector<LwU8>& bytes) override;
    virtual RC SubmitResult(const TestcaseResult& result) override;
private:
    std::string m_Path;
    int         m_QdSend = -1;
    int         m_QdRecv = -1;
    LwBool      m_LogVerbose = LW_FALSE;
};

#endif // UCODEFUZZ_MQDATAINTERFACE_H
