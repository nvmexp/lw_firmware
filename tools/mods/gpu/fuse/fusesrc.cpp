/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2011,2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   fusesrc.cpp
 * @brief  Implementation of Fuse-blowing functions
 */

#include "core/include/tee.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "fusesrc.h"
#include "core/include/jscript.h"

//-----------------------------------------------------------------------------
FuseSource::FuseSource()
:
 m_pSubdev(NULL)
 ,m_pJsFunc(NULL)
 ,m_Method(UNKNOWN_METHOD)
 ,m_GpioNum(0)
{
}

FuseSource::~FuseSource()
{
}

RC FuseSource::Initialize(GpuSubdevice* pSubdev,
                          string        MethodStr)
{
    // accepted format:
    // gpio_xxxx  where xxx is gpio number in decimal
    // ioexpand_yyy_zzz  where yyy = port, zzz = addr

    size_t Pos = MethodStr.find("_");
    if (Pos == string::npos)
    {
        Printf(Tee::PriNormal, "Bad method string format.\n)");
        return RC::BAD_PARAMETER;
    }

    string Method = MethodStr.substr(0, Pos);
    if (Method == "gpio")
    {
        string GpioNumStr = MethodStr.substr(Pos + 1);
        UINT32 GpioNum = strtoul(GpioNumStr.c_str(), NULL, 10);
        return InitializeGpio(pSubdev, GpioNum);
    }
    else if (Method == "ioexpand")
    {
        // ExpanderStr == yyy_zzz
        string ExpanderStr = MethodStr.substr(Pos + 1);
        Pos = ExpanderStr.find("_");
        if (Pos == string::npos)
        {
            Printf(Tee::PriNormal, "Bad method string format.\n)");
            return RC::BAD_PARAMETER;
        }
        // PortStr == yyy
        string PortStr = ExpanderStr.substr(0, Pos);
        UINT32 Port    = strtoul(ExpanderStr.c_str(), NULL, 16);
        // ExpanderStr == zzz
        ExpanderStr    = ExpanderStr.substr(Pos + 1);
        UINT32 Addr    = strtoul(ExpanderStr.c_str(), NULL, 16);
        return InitializeIOExpander(pSubdev, Port, Addr);
    }
    else if (Method == "jsfunc")
    {
        return InitializeJsFunc(pSubdev, MethodStr.substr(Pos + 1));
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
}

RC FuseSource::InitializeGpio(GpuSubdevice* pSubdev, UINT32 GpioNum)
{
    RC rc;
    m_pSubdev = pSubdev;
    m_Method  = GPIO;
    m_GpioNum = GpioNum;

    if (!pSubdev->GpioIsOutput(GpioNum))
    {
        Printf(Tee::PriNormal, "GpioNum %d was set to input; changing to output\n",
               GpioNum);
        CHECK_RC(pSubdev->SetGpioDirection(GpioNum, GpuSubdevice::OUTPUT));
    }
    return rc;
}

RC FuseSource::InitializeIOExpander(GpuSubdevice* pSubdev,
                                    UINT32        I2CPort,
                                    UINT32        I2CAddr)
{
    m_pSubdev = pSubdev;
    m_Method  = IO_EXPANDER;
    if (!pSubdev->SupportsInterface<I2c>())
    {
        Printf(Tee::PriError, "%s does not support I2c\n", pSubdev->GetName().c_str());
        return RC::UNSUPPORTED_DEVICE;
    }
    m_pI2cDev.reset(new I2c::Dev(pSubdev->GetInterface<I2c>()->I2cDev()));
    m_pI2cDev->SetPortBaseOne(I2CPort);
    m_pI2cDev->SetAddr(I2CAddr);
    return OK;
}

RC FuseSource::InitializeJsFunc(GpuSubdevice* pSubdev, string FuncName)
{
    RC rc;
    m_pSubdev = pSubdev;
    m_Method  = JS_FUNC;
    JavaScriptPtr pJs;
    CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(),
                              FuncName.c_str(),
                              &m_pJsFunc));
    return rc;
}

RC FuseSource::Toggle(Level Value)
{
    MASSERT(m_pSubdev);

    if (Value > TO_HIGH)
    {
        return RC::BAD_PARAMETER;
    }

    switch (m_Method)
    {
        case GPIO:
            return ByGpio(Value);
        case IO_EXPANDER:
            return ByIoExpander(Value);
        case JS_FUNC:
            return ByJsFunc(Value);
        default:
            return RC::SOFTWARE_ERROR;
    }
}

RC FuseSource::ByGpio(Level Value)
{
    bool ToHigh = true;
    if (Value == TO_LOW)
        ToHigh = false;

    m_pSubdev->GpioSetOutput(m_GpioNum, ToHigh);
    return OK;
}

RC FuseSource::ByIoExpander(Level Value)
{
    RC rc;
    Printf(Tee::PriNormal, "IO expander - need to do a bunch of i2c\n");

    unsigned long WriteData = (unsigned long)Value;
    const UINT32 IO_EXPANDER_I2C_CONFIG_OFFSET = 0x3;
    const UINT32 IO_EXPANDER_I2C_OUTPUT_OFFSET = 0x1;
    const UINT32 IO_EXPANDER_PORT              = 0;

    // open IO expander port
    UINT32 ReadData;
    CHECK_RC(m_pI2cDev->Read(IO_EXPANDER_I2C_CONFIG_OFFSET, &ReadData));

    UINT32 ToWrite = 0xff & ReadData & ~(1<<IO_EXPANDER_PORT);
    CHECK_RC(m_pI2cDev->Write(IO_EXPANDER_I2C_CONFIG_OFFSET, ToWrite));

    CHECK_RC(m_pI2cDev->Read(IO_EXPANDER_I2C_CONFIG_OFFSET, &ReadData));

    // double check (pulled from HW team's algorithm)
    if (ReadData & (1<<IO_EXPANDER_PORT))
    {
        Printf(Tee::PriHigh, "unable to configure IO Expander for output\n");
        return RC::I2C_COULD_NOT_ACQUIRE_PORT;
    }

    CHECK_RC(m_pI2cDev->Read(IO_EXPANDER_I2C_OUTPUT_OFFSET, &ReadData));

    ToWrite = (ReadData & ~(1<<IO_EXPANDER_PORT)) |
               (WriteData|IO_EXPANDER_PORT);

    CHECK_RC(m_pI2cDev->Write(IO_EXPANDER_I2C_OUTPUT_OFFSET, ToWrite));
    return rc;
}

RC FuseSource::ByJsFunc(Level value)
{
    RC rc;
    JavaScriptPtr pJs;

    JsArray Args(1);
    CHECK_RC(pJs->ToJsval((value == TO_HIGH), &Args[0]));

    jsval retValInJs = JSVAL_NULL;
    CHECK_RC(pJs->CallMethod(m_pJsFunc, Args, &retValInJs));

    UINT32 rcNum;
    CHECK_RC(pJs->FromJsval(retValInJs, &rcNum));
    return RC(rcNum);
}

//-----------------------------------------------------------------------------
JS_CLASS(FuseSrcConst);
//-----------------------------------------------------------------------------
//! \brief ParamConst javascript interface object
//!
//! ParamConst javascript interface
//!
static SObject FuseSrcConst_Object
(
    "FuseSrcConst",
    FuseSrcConstClass,
    0,
    0,
    "FuseSrcConst JS Object"
);

PROP_CONST(FuseSrcConst, TO_HIGH, FuseSource::TO_HIGH);
PROP_CONST(FuseSrcConst, TO_LOW, FuseSource::TO_LOW);
