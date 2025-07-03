/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_privsecregaccesstest.cpp
//! \brief Test to verify that RM priv reg accesses work
//!
//! This test verifies that that the registers that RM accesses actually work
//! (i.e. do not cause a fault). Note that RM can only access registers whose
//! L0 read/write bits are set in the PRIV MASK. The test has three stages:
//! 1) progam the silicon using fuses from fuses.lwpu.com
//! 2) query mcheck to see how RM accesses each reg (read, write, read + write)
//! 3) access each register the same way, looking for faults
//!
//! NOTE: this test must be run with the -hot_reset flag for MODS since it may
//!       put the GPU into a bad state, affecting tests that follow it.
//! NOTE: specify the mcheck --reglist log file to this test using the following
//!       command line argument:
//!       -reg_access_file <path of reg access file>

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include <string>
#include "core/include/memcheck.h"
#include <map>
#include <vector>
#include "class/cl0005.h"
#include "gpu/include/gpusbdev.h"
#include <iostream>
#include <fstream>
#include <regex>
#include "core/include/utility.h"
#include "core/utility/errloggr.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"

#define BASE_16      (16)
#define SECOND_TOKEN (1)
#define TIMEOUT (1000000)

static void PrivRegFaultCallback(void *parg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status);

class AccessedPrivReg
{
private:
    bool read; // true if the priv reg is read by RM
    bool write; // true if the priv reg is written by RM
    UINT32 baseAddr;
    UINT32 writeVal;
    string regName;

public:
    AccessedPrivReg(string regName, UINT32 baseAddr);

    void setWrite(UINT32 writeVal);
    void setRead();
    bool isRead();
    bool isWritten();
    UINT32 getWriteValue();
    UINT32 getBaseAddr();
    string getName();
};

AccessedPrivReg::AccessedPrivReg(string regName, UINT32 baseAddr)
{
    read = false;
    write = false;
    this->baseAddr = baseAddr;
    writeVal = 0;
    this->regName = regName;
}

void AccessedPrivReg::setWrite(UINT32 writeVal)
{
    write = true;
    this->writeVal = writeVal;
}

void AccessedPrivReg::setRead()
{
    read = true;
}

bool AccessedPrivReg::isRead()
{
    return read;
}

bool AccessedPrivReg::isWritten()
{
    return write;
}

UINT32 AccessedPrivReg::getWriteValue()
{
    return writeVal;
}

UINT32 AccessedPrivReg::getBaseAddr()
{
    return baseAddr;
}

string AccessedPrivReg::getName()
{
    return regName;
}

typedef map<UINT32, AccessedPrivReg *> AccessMapType;

class PrivRegAccessTest : public RmTest
{
public:
    PrivRegAccessTest();
    virtual ~PrivRegAccessTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(reg_access_file, string);

private:
    LwRm * pLwRm = nullptr;
    AccessMapType accessMap;
    map<string, LwU32> writeValueMap;
    map<string, LwBool> ignoreMap;
    vector<Lw2080PrivRegAccessFaultNotification> regFaultList;
    LwRm::Handle hClient        = 0;
    LwRm::Handle hSubDev        = 0;
    LwRm::Handle hRegFaultEvent = 0;
    LWOS10_EVENT_KERNEL_CALLBACK_EX callbackParams = {};
    GpuSubdevice * pSubDev = nullptr;
    string m_reg_access_file;

    RC parseRegAccessFile();
    LwBool shouldIgnoreRegister(string regName, UINT32 baseAddr, LwBool isRead,
        LwBool isWritten);
    void setupWriteValueMap();
    void setupIgnoreMap();
};

//! \brief Does nothing
//!
//! \sa Setup
//------------------------------------------------------------------------------
PrivRegAccessTest::PrivRegAccessTest()
{
    SetName("PrivRegAccessTest");
}

//! \brief Does nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
PrivRegAccessTest::~PrivRegAccessTest()
{

}

//! \brief Test only supported on silicon
//!
//! PrivRegAccessTest is only supported on silicon, RTL simulation, and emulation.
//! Simulations do not have priv masks, making this test invalid.
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         skip reason otherwise
//------------------------------------------------------------------------------
string PrivRegAccessTest::IsTestSupported()
{
    if (Platform::GetSimulationMode() != Platform::Hardware &&
        Platform::GetSimulationMode() != Platform::RTL)
    {
        return "PrivRegAccessTest only works on silicon, emulation, or RTL simulation";
    }
    else if (!IsMAXWELLorBetter(GetBoundGpuSubdevice()->DeviceId()))
    {
        return "PrivRegAccessTest only works for chips that have priv security enabled "
            "which starts with Maxwell";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Register priv reg fault callbacks, populate list of RM accesses
//!
//! - registers a callback for priv register access faults
//! - populates a list of priv register accesses that RM makes by parsing
//!   mcheck dump
//!
//! \return Fails if callbacks cannot be registered or if the specified
//!         mcheck file cannot be opened
//------------------------------------------------------------------------------
RC PrivRegAccessTest::Setup()
{
    RC rc;
    LW0005_ALLOC_PARAMETERS regCallbackParams = {0};
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS setNotificationParams = {0};

    // initialize test parameters
    CHECK_RC(InitFromJs());

    // save off local copy of common params, for brevity
    pLwRm = GetBoundRmClient();
    pSubDev = GetBoundGpuSubdevice();
    hSubDev = pLwRm->GetSubdeviceHandle(pSubDev);

    // register callback for bad priv register accesses
    callbackParams.func = PrivRegFaultCallback;
    callbackParams.arg = &regFaultList;

    regCallbackParams.hParentClient = pLwRm->GetClientHandle();
    regCallbackParams.hClass = LW01_EVENT_KERNEL_CALLBACK_EX;
    regCallbackParams.notifyIndex = LW2080_NOTIFIERS_PRIV_REG_ACCESS_FAULT;
    regCallbackParams.data = LW_PTR_TO_LwP64(&callbackParams);

    CHECK_RC(pLwRm->Alloc(hSubDev,
                          &hRegFaultEvent,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &regCallbackParams));

    //
    // enable repeat interrupts (i.e. not single-shot notification) for
    // priv register access faults
    //
    setNotificationParams.event = LW2080_NOTIFIERS_PRIV_REG_ACCESS_FAULT;
    setNotificationParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(hSubDev,
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &setNotificationParams, sizeof(setNotificationParams)));

    // load all the registers that we want to special case
    setupWriteValueMap();
    setupIgnoreMap();

    // parse in mcheck files and populate the access map
    CHECK_RC(parseRegAccessFile());

    return rc;
}

//! \brief Access each priv register the same way RM does
//!
//! Iterate through all priv registers that RM accesses, and make the same
//! kind of access. Example: if it written by RM, we write to the register
//! also. We sleep briefly to let interrupts, if any, fire and complete
//!
//! \return Returns OK if there were no priv reg faults, otherwise
//! dumps all faults and then returns an error
//------------------------------------------------------------------------------
RC PrivRegAccessTest::Run()
{
    LwU32 writebackVal = 0;

    // tell MODS that this test expects interrupts
    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    //
    // iterate over all accessed priv registers, access them the same way RM
    // accesses them
    //
    for (AccessMapType::iterator it = accessMap.begin(); it != accessMap.end();
        it++) {

        AccessedPrivReg * privReg = it->second;

        if (privReg->isRead())
        {
            writebackVal = pSubDev->RegRd32(privReg->getBaseAddr());
            Utility::SleepUS(TIMEOUT);

            if (privReg->isWritten())
            {
                pSubDev->RegWr32(privReg->getBaseAddr(), writebackVal);
                Utility::SleepUS(TIMEOUT);
            }
        }
        else if (privReg->isWritten()) // write only register
        {
            pSubDev->RegWr32(privReg->getBaseAddr(), privReg->getWriteValue());
            Utility::SleepUS(TIMEOUT);
        }
    }

    // tell MODS that the test has ended -- no more interrupts are expected
    ErrorLogger::TestCompleted();

    // print out any priv register access faults, if any
    if (regFaultList.empty())
    {
        return OK;
    }
    else
    {
        Printf(Tee::PriNormal, "Bad priv reg accesses made by RM detected!\n");

        for (vector<Lw2080PrivRegAccessFaultNotification>::iterator it = regFaultList.begin();
            it != regFaultList.end(); it++)
        {
            if (accessMap.find(it->errAddr) != accessMap.end())
            {
                Printf(Tee::PriNormal,
                       "Priv Reg Fault: name = %s address = 0x%x\n",
                       accessMap[it->errAddr]->getName().c_str(), it->errAddr);
            }
            else
            {
                Printf(Tee::PriNormal,
                       "Priv Reg Fault: address = 0x%x\n",
                       it->errAddr);
            }
        }

        return RC::REGISTER_READ_WRITE_FAILED;
    }
}

//! \brief Clean data structures and free all handles
//!
//! - clean the access map
//! - clean all exception maps (i.e. ignoreMap, rwMap, writeValueMap)
//! - clean the list that stores all priv register faults
//! - free all handles
//------------------------------------------------------------------------------
RC PrivRegAccessTest::Cleanup()
{
    for (AccessMapType::iterator it = accessMap.begin(); it != accessMap.end();
        it++)
    {
        delete it->second;
    }
    accessMap.clear();
    regFaultList.clear();
    ignoreMap.clear();
    writeValueMap.clear();
    pLwRm->Free(hRegFaultEvent);

    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

RC PrivRegAccessTest::parseRegAccessFile()
{
    ifstream regAccessFile(m_reg_access_file);
    string lwrLine;
    string regName = "dummy name";
    regex regNameRegEx("^Register ([A-Z_]+)$");
    regex endRegEx("^Found [0-9]+ register usages.$");
    regex baseAddrRegEx("^ *(0x[0-9a-fA-F]+) *: *[a-z0-9]+$");
    regex accessRegEx("^ *[a-zA-Z0-9_]+:.*$");
    cmatch cm;
    UINT32 baseAddr;
    AccessedPrivReg * privReg;
    RC rc = OK;
    LwBool isRead = false;
    LwBool isWritten = false;

    if (!regAccessFile.is_open())
    {
        return RC::CANNOT_OPEN_FILE;
    }

    // parsing logic
    while (!regAccessFile.eof())
    {
        //
        // get a line, if it has the form:
        // "Found X register usages."
        // then exist, if it has the form:
        // "Register X"
        // then it is the start of a register usage information block,
        // otherwise it is irrelevant information
        //
        getline(regAccessFile, lwrLine);

        // check for the final line of the mcheck file
        if (regex_match(lwrLine.c_str(), endRegEx))
        {
            break;
        }

        //
        // check to make sure that the line is a line specifying a register
        // name
        //
        else if (!regex_match(lwrLine.c_str(), cm, regNameRegEx))
        {
            continue;
        }

        regName = cm[SECOND_TOKEN];

        //
        // parse the base address. which should have the form
        // "   X : chip"
        //
        getline(regAccessFile, lwrLine);

        if (!regex_match(lwrLine.c_str(), cm, baseAddrRegEx))
        {
            continue;
        }

        baseAddr = stoul(cm[SECOND_TOKEN], nullptr, BASE_16);

        // parse access types
        getline(regAccessFile, lwrLine);
        isRead = false;
        isWritten = false;
        while (regex_match(lwrLine.c_str(), accessRegEx))
        {
            if (lwrLine.find("read") != string::npos)
            {
                isRead = true;
            }

            if (lwrLine.find("write") != string::npos)
            {
                isWritten = true;
            }

            getline(regAccessFile, lwrLine);
        }

        //
        // if the register is accessed and isn't one of the registers
        // that we want to ignore, then create the AccessPrivReg object
        // and populate it
        //
        if (!shouldIgnoreRegister(regName, baseAddr, isRead, isWritten))
        {

            privReg = new AccessedPrivReg(regName, baseAddr);
            accessMap.emplace(baseAddr, privReg);

            if (isRead)
            {
                privReg->setRead();
            }

            if (isWritten)
            {
                //
                // check if we've explicitly defined a value that is
                // safe to write to the register
                //
                if (writeValueMap.find(regName) != writeValueMap.end())
                {
                    privReg->setWrite(writeValueMap[regName]);
                }

                //
                // check if the register is read and written. if so,
                // mark the register for read and write. the test will
                // read the value of the register and write it back
                //
                else if (isRead)
                {
                    privReg->setWrite(0);
                }

                //
                // possible write-only register with no specified safe
                // write value
                //
                else
                {
                    Printf(Tee::PriNormal,
                           "ERROR: no safe write value specified for possible "
                           "write-only register: %s\n", regName.c_str());
                    accessMap.erase(baseAddr);
                    delete privReg;
                }
            }
        }
    }

    regAccessFile.close();

    return rc;
}

//
// set of rules for choosing wheter to ignore testing of a register that RM
// accesses. some of these rules are heuristics to filter out bit fields and
// values that look like registers in the mcheck spew
//
LwBool PrivRegAccessTest::shouldIgnoreRegister(string regName, UINT32 baseAddr,
    LwBool isRead, LwBool isWritten)
{
    //
    // check if the register name has certain keywords. if so, it is likely
    // not a register.
    //
    if (regName.find("SIZE") != string::npos
        || regName.find("TRUE") != string::npos
        || regName.find("FALSE") != string::npos
        || regName.find("STRIDE") != string::npos
        || regName.find("__PROD") != string::npos)
    {
        return LW_TRUE;
    }

    //
    // check that the base address is less than 4. this is to filter out
    // flags.
    //
    if (baseAddr < 4)
    {
        return LW_TRUE;
    }

    //
    // filter out registers that are only written to by RM. we do this
    // because it is a lot of work to figure out safe value to write to
    // these registers
    //
    if (isWritten && !isRead)
    {
        return LW_TRUE;
    }

    // check that the register is accessed at all
    if (!isWritten && !isRead)
    {
        return LW_TRUE;
    }

    //
    // check that the register isn't in the list of special registers
    // that we special case to ignore
    //
    if (ignoreMap.find(regName) != ignoreMap.end())
    {
        return LW_TRUE;
    }

    //
    // check that this register isn't already in the list of registers
    // that we're going to access. if it is, then there is an address
    // collision
    //
    MASSERT(accessMap.find(baseAddr) == accessMap.end());

    return LW_FALSE;
}

// populate this function with register names that should be ignored
void PrivRegAccessTest::setupIgnoreMap()
{
    // these are not registers
    ignoreMap["LW_PFALCON_FALCON_DMACTL"] = true;
    ignoreMap["LW_PFALCON_FALCON_IRQSCLR"] = true;
    ignoreMap["LW_PFALCON_FALCON_NXTCTX"] = true;
    ignoreMap["LW_PFB_FBPA_OFFSET_PRE"] = true;
    ignoreMap["LW_PFB_FBPA_OFFSET_REF"] = true;
    ignoreMap["LW_PFB_FBPA_OFFSET_TESTCMD"] = true;
    ignoreMap["LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_RESTORE_GOLDEN"] = true;
    ignoreMap["LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_SET_WATCHDOG_TIMEOUT"] = true;
    ignoreMap["LW_PFIFO_FB_TIMEOUT_PERIOD_MAX"] = true;
    ignoreMap["LW_PFB_FBPA_OFFSET_FBIO_CFG_BRICK_VAUXGEN"] = true;
    ignoreMap["LW_PFB_FBPA_OFFSET_CSTATUS"] = true;

    //
    // these registers are only valid and used for
    // RTL simulation and emulation. they are disabled in
    // silicon
    //
    ignoreMap["LW_PMGR_NON_SILICON_VERIF_DP_BFM_ADDR"] = true;
    ignoreMap["LW_PMGR_NON_SILICON_VERIF_DP_BFM_DATA"] = true;
    ignoreMap["LW_PMGR_NON_SILICON_VERIF_DP_BFM_REG_ALC_VLD"] = true;

    //
    // this register is known to be write-protected. the code that
    // writes to this register is intended to be run with the correct HULK
    // license to lower the priv level. the code that accesses this
    // register will eventually be deprecated for GP100+
    //
    ignoreMap["LW_PMGR_ROM_SERIAL_BYPASS"] = true;

    //
    // this register is either written to by a privileged client
    // (i.e. BSI, PMU), accessed during device initialization when
    // it is set to L0, or accessed after a check for if SPI bit bang
    // is enabled
    //
    ignoreMap["LW_PMGR_ROM_ADDR_OFFSET"] = true;

    //
    // writing to the indirect frame buffer could overwrite some arbitrary
    // piece of FB, therefore skip it.
    //
    ignoreMap["LW_PBUS_IFB_RDWR_DATA"] = true;
}

//
// populate this function with assignments of write-only registers
// and values that can be safely written to them
//
void PrivRegAccessTest::setupWriteValueMap()
{
}

static void PrivRegFaultCallback(void *parg, void *pData, LwU32 hEvent,
    LwU32 data, LwU32 status)
{
    vector<Lw2080PrivRegAccessFaultNotification> * regFaultList
        = (vector<Lw2080PrivRegAccessFaultNotification> *) parg;
    Lw2080PrivRegAccessFaultNotification * notification
        = (Lw2080PrivRegAccessFaultNotification *) pData;

    if (regFaultList == NULL || notification == NULL)
    {
        Printf(Tee::PriNormal,
            "ERROR: NULL regFaultList or notification parameter!\n");
    }
    else
    {
        regFaultList->push_back(*notification);
    }

    return;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(PrivRegAccessTest, RmTest,
    "Test to verify that all priv register accesses made by RM are good");

CLASS_PROP_READWRITE(PrivRegAccessTest, reg_access_file, string,
    "File describing priv register accesses made by RM. Generated by running mcheck --reglist.");

