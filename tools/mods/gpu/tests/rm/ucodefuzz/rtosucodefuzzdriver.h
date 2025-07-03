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

#ifndef RTOSUCODEFUZZDRIVER_H_INCLUDED
#define RTOSUCODEFUZZDRIVER_H_INCLUDED

//!
//! \file rtosucodefuzzdriver.h
//! \brief ucode fuzz driver base classes for RTOS ucodes
//!

#include "ucodefuzzdriver.h"

// Macro for defining rtos ucode parameters
#define DEFINE_RTOS_UCODE_FUZZ_PROPS(CLASS_NAME) \
CLASS_PROP_READWRITE(CLASS_NAME, UnitId, LwU32, \
                     "Unit (task) to be fuzzed"); \
CLASS_PROP_READWRITE(CLASS_NAME, CmdId, LwU32, \
                     "Cmd to be fuzzed"); \
DEFINE_UCODE_FUZZ_PROPS(CLASS_NAME);

// Macro for defining rtos flcn ucode parameters
#define DEFINE_RTOS_FLCN_UCODE_FUZZ_PROPS(CLASS_NAME) \
CLASS_PROP_READWRITE(CLASS_NAME, CmdSize, LwU32, \
                     "Size of cmd payload of cmd to be fuzzed"); \
DEFINE_RTOS_UCODE_FUZZ_PROPS(CLASS_NAME);

class FilledRtosCmdTemplate : public FilledDataTemplate
{
public:
    std::vector<LwU8> cmd;
    std::vector<LwU8> payload;
};

class RtosUcodeFuzzDriver : public UcodeFuzzDriver
{
public:
    virtual ~RtosUcodeFuzzDriver() = default;

    virtual RC    PreRun(FilledDataTemplate* pData);
    virtual RC    SubmitFuzzedData(const FilledDataTemplate* pData) final;
    virtual LwU32 GetUcodeId() = 0; //! See ctrl2080ucodefuzzer.h for enum
    virtual RC    CoverageEnable();
    virtual RC    CoverageDisable();
    virtual RC    CoverageCollect(TestcaseResult& result);

    SETGET_PROP(UnitId, LwU32);
    SETGET_PROP(CmdId, LwU32);
private:
    LwU32 m_UnitId;
    LwU32 m_CmdId;
};

class RtosFlcnCmdFuzzDriver : public RtosUcodeFuzzDriver
{
public:
    virtual ~RtosFlcnCmdFuzzDriver() = default;
    virtual const DataTemplate* GetDataTemplate() const override { return m_pDataTemplate.get(); }

    virtual RC Setup();
    virtual RC Cleanup();

    SETGET_PROP(CmdSize, LwU32);
private:
    unique_ptr<DataTemplate> m_pDataTemplate = nullptr;
    LwU32 m_CmdSize;
};

#endif
