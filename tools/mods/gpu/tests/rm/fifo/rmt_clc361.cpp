/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_clc361Usermode.cpp
//! \brief To test for correct allocation and mapping of volta_usermode_a class.
//!
//! An object of volta_usermode_a class is mapped and tested. Test includes
//! negative testing of mapped area(bad offset and size), access to undefined
//! registers and checking correct freeing of allocated object.

#include "gpu/tests/rmtest.h"
#include "core/include/lwrm.h"
#include "lwos.h"
#include "core/include/rc.h"
#include "lwRmApi.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "core/include/massert.h"
#include "gpu/include/gpudev.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "class/cl0080.h" // LW01_DEVICE_0
#include "class/clc361.h" // VOLTA_USERMODE_A class
#include "ctrl/ctrl2080/ctrl2080fifo.h"
#include <stdio.h>
#include "core/include/memcheck.h"

class ClassC361Test : public RmTest
{
public:
    ClassC361Test();
    virtual ~ClassC361Test();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:

    LwU32  m_pClass;

    UINT64 m_correctOffset;
    UINT64 m_correctSize;

    void  *m_pVirtualAddress;

    LwRm::Handle m_hObject;
    LwRm::Handle m_hSubdevice;
    LwRm::Handle m_hDevice;
    LwRm::Handle m_hClient;

    RC negativeOffsetTest();
    RC negativeSizeTest();
    RC overflowTest();
    RC undefinedRegAccessTest();
    RC freeParentTest();
};

//! \brief ClassC361Test constructor
//!
//! \sa Setup
//------------------------------------------------------------------------------
ClassC361Test::ClassC361Test()
{
    SetName("ClassC361Test");
    m_pClass = VOLTA_USERMODE_A;
}

//! \brief ClassC361Test destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
ClassC361Test::~ClassC361Test()
{

}

//! \brief Whether or not the test is supported in current environment.
//!
//! \return RUN_RMTEST_TRUE if VOLTA_USERMODE_A is supported on current chip.
//------------------------------------------------------------------------------
string ClassC361Test::IsTestSupported()
{
    if (IsClassSupported(m_pClass))
    {
        return RUN_RMTEST_TRUE;
    }
    else
    {
        return "VOLTA_USERMODE_A class is not supported on current chip";
    }
}

//! \brief Set up all necessary state before running
//!
//! \sa ClassC361Test constructor
//------------------------------------------------------------------------------
RC ClassC361Test::Setup()
{
    RC rc;
    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    if (LwRmAllocRoot(&m_hClient) != LW_OK)
    {
        return RC::SOFTWARE_ERROR;
    }

    m_hDevice = m_hClient + 1;
    m_hSubdevice = m_hDevice + 1;
    m_hObject = m_hSubdevice + 1;

    LW0080_ALLOC_PARAMETERS devAllocParams;
    memset(&devAllocParams, 0, sizeof(devAllocParams));
    devAllocParams.deviceId = 0;

    if (LwRmAlloc(m_hClient,
                  m_hClient,
                  m_hDevice,
                  LW01_DEVICE_0,
                  &devAllocParams) != LW_OK)
    {
        return RC::SOFTWARE_ERROR;
    }

    LW2080_ALLOC_PARAMETERS params;
    memset(&params, 0, sizeof(params));
    params.subDeviceId = 0;

    if (LwRmAlloc(m_hClient, m_hDevice, m_hSubdevice, LW20_SUBDEVICE_0, &params)
                                                  != LW_OK)
    {
        return RC::SOFTWARE_ERROR;
    }

    if(LwRmAlloc(m_hClient, m_hSubdevice, m_hObject, m_pClass, NULL) != LW_OK)
    {
        return RC::SOFTWARE_ERROR;
    }
    m_correctOffset = 0;
    m_correctSize = DRF_SIZE(LWC361);

    return rc;
}

//! \brief Main body of this test
//!
//! Calls a bunch of sub tests
//!
//! \return OK if test ran successfully, test-specific RC if it failed
//------------------------------------------------------------------------------
RC ClassC361Test::Run()
{
    RC rc;
    CHECK_RC(negativeOffsetTest());
    CHECK_RC(negativeSizeTest());
    CHECK_RC(overflowTest());
    CHECK_RC(undefinedRegAccessTest());
    CHECK_RC(freeParentTest());
    return rc;
}

//! \brief Free any resources that this test selwred.
//!
//------------------------------------------------------------------------------
RC ClassC361Test::Cleanup()
{
    LwRmFree(m_hClient, m_hClient, m_hClient);
    return OK;
}

RC ClassC361Test::negativeOffsetTest()
{
    RC rc;
    UINT64 wrongOffset;

    wrongOffset = m_correctOffset + 1; // can be any number apart from correctOffset

    if (LwRmMapMemory(m_hClient, m_hDevice, m_hObject, wrongOffset, m_correctSize,
                        &m_pVirtualAddress, 0) == LW_OK)
    {
        Printf(Tee::PriError, "RM: %s failed for class C361 ", __FUNCTION__);
        LwRmUnmapMemory(m_hClient, m_hDevice, m_hObject, m_pVirtualAddress, 0);
        rc = RC::SOFTWARE_ERROR;
    }

    else
    {
        rc = OK;
        Printf(Tee::PriDebug, "RM: %s passed for class C361 ", __FUNCTION__);
    }
    return rc;
}

RC ClassC361Test::negativeSizeTest()
{
    RC rc;
    UINT64 outsideLimit;

    outsideLimit = m_correctSize + 1; // case of offset + length > region size

    if (LwRmMapMemory(m_hClient, m_hDevice, m_hObject, m_correctOffset, outsideLimit,
                                                     &m_pVirtualAddress, 0) == LW_OK)
    {
        Printf(Tee::PriError, "RM: %s failed for class C361 ", __FUNCTION__);
        LwRmUnmapMemory(m_hClient, m_hDevice, m_hObject, m_pVirtualAddress, 0);
        rc = RC::SOFTWARE_ERROR;
    }

    else
    {
        rc = OK;
        Printf(Tee::PriDebug, "RM: %s passed for class C361 ", __FUNCTION__);
    }
    return rc;
}

RC ClassC361Test::overflowTest()
{
    RC rc;
    UINT64 overflowSize;

    overflowSize = (UINT64)-1;
    if (LwRmMapMemory(m_hClient, m_hDevice, m_hObject, m_correctOffset, overflowSize,
                                                     &m_pVirtualAddress, 0) == LW_OK)
    {
        Printf(Tee::PriError, "RM: %s failed for class C361 ", __FUNCTION__);
        LwRmUnmapMemory(m_hClient, m_hDevice, m_hObject, m_pVirtualAddress, 0);
        rc = RC::SOFTWARE_ERROR;
    }

    else
    {
        rc = OK;
        Printf(Tee::PriDebug, "RM: %s passed for class C361 ", __FUNCTION__);
    }
    return rc;
}

RC ClassC361Test::undefinedRegAccessTest()
{
    RC rc;
    LwU32 *pUndefRegAddress;
    LwU32 value;

    if (LwRmMapMemory(m_hClient, m_hDevice, m_hObject, m_correctOffset, m_correctSize,
                                                     &m_pVirtualAddress, 0) != LW_OK)
    {
        return RC::SOFTWARE_ERROR;
    }

    // reads to undefined registers should return 0
    pUndefRegAddress = (LwU32*)((char*)m_pVirtualAddress + DRF_SIZE(LWC361)) - 1;

    value = MEM_RD32(pUndefRegAddress);

    if (value != 0)
    {
        Printf(Tee::PriError, "RM: %s failed as undefined register access did not return zero for class C361 ", __FUNCTION__);
        LwRmUnmapMemory(m_hClient, m_hDevice, m_hObject, m_pVirtualAddress, 0);
        return RC::SOFTWARE_ERROR;
    }

    LwRmUnmapMemory(m_hClient, m_hDevice, m_hObject, m_pVirtualAddress, 0);
    return OK;
}

RC ClassC361Test::freeParentTest()
{
    RC rc;
    LwRmFree(m_hClient, m_hDevice, m_hSubdevice);

    if (LwRmMapMemory(m_hClient, m_hDevice, m_hObject, m_correctOffset, m_correctSize,
                                    &m_pVirtualAddress, 0) == LW_ERR_OBJECT_NOT_FOUND)
    {
        rc = OK;
    }
    else
    {
        LwRmUnmapMemory(m_hClient, m_hDevice, m_hObject, m_pVirtualAddress, 0);
        Printf(Tee::PriError, "RM: %s failed as child was not freed on freeing parent for class C361 ", __FUNCTION__);
        rc = RC::SOFTWARE_ERROR;
    }
    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ClassC361Test
//! object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(ClassC361Test, RmTest,
    "Class361Test RM test");
