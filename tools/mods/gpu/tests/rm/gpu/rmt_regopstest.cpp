/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017,2019,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_regopstest.cpp
//! \brief RegOps tests
//!
//! The purpose of this file to test the RegOp control call to make
//! sure that it works as expected.
//!

#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "class/cl5097.h" // LW50_TESLA
#include "class/cl9097.h" // FERMI_A
#include "class/cla097.h" // KEPLER_A
#include "class/cla197.h" // KEPLER_B
#include "class/cla297.h" // KEPLER_C
#include "lwos.h"
#include "lwRmApi.h"
#include "hwref/kepler/gk107/dev_bus.h"
#include "hwref/kepler/gk107/dev_ram.h"
#include "hwref/kepler/gk107/dev_graphics_nobundle.h"
#include "core/include/channel.h"
#include "gpu/tests/rm/utility/changrp.h"
#include "gpu/include/gralloc.h"
#include "class/cl9067.h"
#include "core/include/lwrm.h"
#include "core/include/utility.h"
#include "gpu/utility/fakeproc.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/floorsweepimpl.h"
#include "core/include/memcheck.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpumgr.h"
#include "core/include/refparse.h"

//at DEBUG_TRACE_LEVEL 1

#define RMT_INIT_REGOP(pRegOp)                          \
    memset((pRegOp), 0, sizeof(LW2080_CTRL_GPU_REG_OP))

#define REGOP_EXPECT_VAL(val, expectedVal)                              \
    Printf(Tee::PriNormal, "[%s] Expecting: 0x%x Got: 0x%x\n",          \
           __FUNCTION__, (expectedVal), (val));                         \
    if ((val) != (expectedVal))                                         \
    {                                                                   \
        Printf(Tee::PriNormal, "[%s] Value Mismatch!\n", __FUNCTION__); \
        tmpRc = RC::SOFTWARE_ERROR;                                     \
    }

#define REGOP_RUN_TEST(name)                                            \
    rc = name();                                                        \
    if (rc != OK)                                                       \
    {                                                                   \
        Printf(Tee::PriNormal, "[%s] -- Test failed! --\n", __FUNCTION__); \
        retRc = rc;                                                     \
    }

// Values that will be used by various test cases for writing into registers.
#define WRITE_VAL1              0xdeadd00d
#define WRITE_VAL2              0x1337d00d

// The andMask used by the test.
#define AND_MASK                0x10101010

#define ILWALID_ADDRESS 0xFFFFFFFF

class RegOpsTest: public RmTest
{
public:
    RegOpsTest();
    virtual ~RegOpsTest();

    virtual string      IsTestSupported();

    virtual RC          Setup();
    virtual RC          Run();
    virtual RC          Cleanup();

private:
    //the parameters that will be passed to the control call.
    LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS regOpParam = {};

    Channel *pCh1       = nullptr;
    Channel *pCh2       = nullptr;
    LwRm::Handle m_hCh1 = 0;
    LwRm::Handle m_hCh2 = 0;

    // graphics contexts for the channels.
    ThreeDAlloc gr;
    ComputeAlloc gr1;

    RC exelwteRegOpCtrlCmd(LW2080_CTRL_GPU_REG_OP *pRegOps, UINT32 count);
    RC doRead(LW2080_CTRL_GPU_REG_OP *regOp, UINT32 opType, LwU32 regOffset);
    RC doWrite(LW2080_CTRL_GPU_REG_OP *regOp, UINT32 opType, LwU32 regOffset, UINT32 ValueLo);
    RC testReadAfterWrite();
    RC testAndMasks();
    RC testBadOps();
    RC testBadRegType();
    RC testBadRegOp();
    RC testBadOffset();
    RC testBadParams();
    RC testBadChannelClient();
    RC testChannelOps();
    RC doChannelRead(LwRm::Handle hCh, UINT32 *regVal);
    RC doChannelWrite(UINT32 val, LwRm::Handle hCh);
    UINT32 getScratchReg();
    UINT32 getCtxRegOffset();
    UINT32 getCtxRegAndMask(UINT32 regOffset);
    RC testChannelGrp();
    RC testContextShare();
    RC testPermissions();
    RC testPermissionsRandom();
    RC doSharedCtxChannelTest(LwRm::Handle hCh1, LwRm::Handle hCh2, LwRm::Handle hChUnshared);
    void dumpParams(LW2080_CTRL_GPU_REG_OP *pRegOps, UINT32 count);
    RC testPermissionsRange(LwU32 start, LwU32 size, LwU32 minAddress, LwU32 maxSize, bool bAllow);
};

//! \brief RegOpsTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RegOpsTest::RegOpsTest()
{
    SetName("RegOpsTest");
    gr.SetOldestSupportedClass(KEPLER_A);
}

//! \brief RegOpsTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RegOpsTest::~RegOpsTest()
{

}

//! \brief Is the test supported in the current environment.
//!
//! \return True
//-----------------------------------------------------------------------------
string RegOpsTest::IsTestSupported()
{
    if (!gr.IsSupported(GetBoundGpuDevice()))
    {
        return "Test is supported only for KEPLER and later chips.";
    }
    return RUN_RMTEST_TRUE;
}

//! \brief Setup before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC RegOpsTest::Setup()
{
    RC rc;
    memset(&regOpParam, 0, sizeof(LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS));

    // This will allow us to create multiple channels.
    m_TestConfig.SetAllowMultipleChannels(true);

    return rc;
}

//! \brief Run the RegOpsTest
//!
//! \return OK if the test passed.
//-----------------------------------------------------------------------------
RC RegOpsTest::Run()
{
    LwRmPtr pLwRm;
    RC rc;
    RC retRc;

    Printf(Tee::PriNormal, "[%s] -- Starting RegOps test --\n", __FUNCTION__);

    REGOP_RUN_TEST(testReadAfterWrite);
    REGOP_RUN_TEST(testAndMasks);
    REGOP_RUN_TEST(testBadOps);
    REGOP_RUN_TEST(testChannelOps);
    REGOP_RUN_TEST(testPermissions);
    REGOP_RUN_TEST(testPermissionsRandom);

    // Do channel group test only if it is supported
    if (pLwRm->IsClassSupported(KEPLER_CHANNEL_GROUP_A, GetBoundGpuDevice()))
    {
        REGOP_RUN_TEST(testChannelGrp);
    }

    REGOP_RUN_TEST(testContextShare);

    if (retRc == OK)
    {
        Printf(Tee::PriNormal, "[%s] -- RegOps test complete! --\n", __FUNCTION__);
    }
    else
    {
        Printf(Tee::PriNormal, "[%s] -- RegOps test failed --\n",
               __FUNCTION__);
    }

    return retRc;
}

//! \brief Cleanup resources acquired by this test.
//!
//! \sa Cleanp
//-----------------------------------------------------------------------------
RC RegOpsTest::Cleanup()
{
    RC rc;
    RC firstRc;

    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ RegOpsTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(RegOpsTest, RmTest,
                 "RegOps RM test.");

//! \brief Returns the offset of a scratch register.
//!
//! Lwrrently just returns one value. More offsets can be added when
//! for newer chips, if necessary.
//!
//! \return Address of a register that can be used as scratch.
//!
UINT32 RegOpsTest::getScratchReg()
{
    GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
    RefManual* pRefMan( pSubdev->GetRefManual() );
    const RefManualRegister* pReg;
    UINT32 offset = ILWALID_ADDRESS;

    // Writting into sw scratch doesn't have any side-effect
    pReg = pRefMan->FindRegister("LW_PBUS_SW_SCRATCH");
    if(pReg)
    {
        // Use LW_PBUS_SW_SCRATCH(0) register
        offset = pReg->GetOffset(0);
    }
    else
    {
        // Kelper doesn't have scratch register, so use LW_PBUS_DEBUG_10
        pReg = pRefMan->FindRegister("LW_PBUS_DEBUG_10");
        if(pReg)
        {
            offset = pReg->GetOffset();
        }
    }

    return offset;
}

//! \brief Gives the offset of a context switched register that can be
//! used as scratch.
//!
//!
//! \return Offset of the register.
//!
UINT32 RegOpsTest::getCtxRegOffset()
{
    return (UINT32)LW_PGRAPH_PRI_MEMFMT_PACK_CTRL1;
}

//! \brief Gives the max valid andMask for a register.
//!
//! \param regOffset The offset of the register whose mask is needed.
//!
//! \return The max valid andMask for the register.
//!
UINT32 RegOpsTest::getCtxRegAndMask(UINT32 regOffset)
{
    UINT32 mask;

    switch (regOffset)
    {
        case LW_PGRAPH_PRI_MEMFMT_PACK_CTRL1:
            //
            // The scratch register that we lwrrently use has only 30 usable bits. This
            // mask masks the bits [32:31]. This mask is used when writing to the
            // register.
            //
            mask = 0x3fffffff;
            break;
        default:
            mask = 0x0;
            break;
    }
    return mask;
}

//! \brief Dumps the parameters for the RegOps control call to stdio.
//!
//! \param pRegOps Pointer to the parameters.
//! \param count Number of paramaters.
//!
//! Lwrrently un-used. Can be of help while debugging in the future.
//!
void RegOpsTest::dumpParams
(
    LW2080_CTRL_GPU_REG_OP *pRegOps,
    UINT32 count
)
{
    UINT32 i;
    LW2080_CTRL_GPU_REG_OP regOp;

    if (pRegOps)
    {
        for (i = 0; i < count; i++)
        {
            regOp = pRegOps[i];
            Printf(Tee::PriNormal, "---- pRegOp[%d] ---- \n", i);
            Printf(Tee::PriNormal, "regOp: %d\n", (int)regOp.regOp);
            Printf(Tee::PriNormal, "regType: %d\n", (int)regOp.regType);
            Printf(Tee::PriNormal, "regStatus: %d\n", (int)regOp.regStatus);
            Printf(Tee::PriNormal, "regQuad: %d\n", (int)regOp.regQuad);
            Printf(Tee::PriNormal, "regGroupMask: %d\n", (int)regOp.regGroupMask);
            Printf(Tee::PriNormal, "regSubGroupMask: %d\n", (int)regOp.regSubGroupMask);
            Printf(Tee::PriNormal, "regOffset: %d\n", (int)regOp.regOffset);
            Printf(Tee::PriNormal, "regValueHi: %d\n", (int)regOp.regValueHi);
            Printf(Tee::PriNormal, "regValueLo: %d\n", (int)regOp.regValueLo);
            Printf(Tee::PriNormal, "regAndNMaskHi: %d\n", (int)regOp.regAndNMaskHi);
            Printf(Tee::PriNormal, "regAndNMaskLo: %d\n", (int)regOp.regAndNMaskLo);

        }
    }
    else
    {
        Printf(Tee::PriNormal, "pRegOps is NULL\n");
    }
}

//! \brief Wrapper function to ilwoke RegOp control call.
//!
//! \param pRegOps Pointer to the array of operations to be performed.
//! \param count   The number of valid operations in the array.
//!
//!
RC RegOpsTest::exelwteRegOpCtrlCmd
(
    LW2080_CTRL_GPU_REG_OP *pRegOps,
    UINT32 count
)
{
    RC rc;
    LwRmPtr pLwRm;

    //set up the parameters for the control call.
    regOpParam.regOpCount = count;
    regOpParam.regOps = LW_PTR_TO_LwP64(pRegOps);

    rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                   LW2080_CTRL_CMD_GPU_EXEC_REG_OPS,
                                   &regOpParam, sizeof(regOpParam));
    return rc;
}

//! \brief Performs a simple 32 bit read operation.
//!
//! Constructs a regOp structure to write 32 bits to getScratchReg() and
//! exelwtes a control call using that structure.
//!
//! \param regOp Pointer to the LW2080_CTRL_GPU_REG_OP struct to be
//! used for the control command.
//! \param opType The type of write operation - either of
//! LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL or  LW2080_CTRL_GPU_REG_OP_TYPE_GR_CTX
//! \param regOffset The address of the register to be read.
//!
//! \return OK if the passed, specific RC if failed
//!
RC RegOpsTest::doRead
(
    LW2080_CTRL_GPU_REG_OP *regOp,
    UINT32 opType,
    LwU32 regOffset
)
{
    RC rc;

    regOp->regOp = LW2080_CTRL_GPU_REG_OP_READ_32;
    regOp->regType = opType;
    regOp->regOffset = regOffset;

    rc = exelwteRegOpCtrlCmd(regOp, 1);

    return rc;
}

//! \brief Performs a single 32 bit read operation.
//!
//! Constructs a regOp structure to read 32 bits from getScratchReg() and
//! exelwtes a control call using that structure.
//!
//! \param regOp Pointer to the LW2080_CTRL_GPU_REG_OP struct to be
//! used for the control command.
//! \param opType The type of read operation - either of
//! LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL or LW2080_CTRL_GPU_REG_OP_TYPE_GR_CTX
//! \param regOffset The address of the register to be written.
//! \param ValueLo The value to be written to the register.
//!
//! \return OK if the passed, specific RC if failed
//!
RC RegOpsTest::doWrite
(
    LW2080_CTRL_GPU_REG_OP *regOp,
    UINT32 opType,
    LwU32 regOffset,
    UINT32 valueLo
)
{
    RC rc;

    regOp->regOp = LW2080_CTRL_GPU_REG_OP_WRITE_32;
    regOp->regType = opType;
    regOp->regOffset = regOffset;
    regOp->regValueLo = valueLo;

    rc = exelwteRegOpCtrlCmd(regOp, 1);

    return rc;
}

//! \brief Performs a single 32 bit write operation on a specific channel.
//!
//! Constructs a regOp structure to write 32 bits to a scratch register and
//! exelwtes a control call using that structure. The regType field is
//! set to LW2080_CTRL_GPU_REG_OP_TYPE_GR_CTX.
//!
//! \param val The value to be written.
//! \param hCh The handle of the channel whose context is to written to.
//!
//! \return OK if the passed, specific RC if failed
//!
RC RegOpsTest::doChannelWrite
(
    UINT32 val,
    LwRm::Handle hCh
)
{
    RC rc;
    LW2080_CTRL_GPU_REG_OP regOp;

    RMT_INIT_REGOP(&regOp);

    regOpParam.hChannelTarget = hCh;
    regOp.regAndNMaskLo = getCtxRegAndMask(getCtxRegOffset());

    rc = doWrite(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GR_CTX, getCtxRegOffset() , val);
    return rc;
}

//! \brief Performs a single 32 bit read operation on a specific channel.
//!
//! Constructs a regOp structure to read 32 bits from a scratch register and
//! exelwtes a control call using that structure. The regType field is
//! set to LW2080_CTRL_GPU_REG_OP_TYPE_GR_CTX.
//!
//! \param hCh The handle of the channel whose context the value is
//! read from.
//! \param[out] regVal The value that is read from the register.
//!
//! \return OK if the passed, specific RC if failed
//!
RC RegOpsTest::doChannelRead
(
    LwRm::Handle hCh,
    UINT32 *regVal
)
{
    RC rc;
    LW2080_CTRL_GPU_REG_OP regOp;

    RMT_INIT_REGOP(&regOp);

    regOpParam.hChannelTarget = hCh;
    regOp.regAndNMaskLo = getCtxRegAndMask(getCtxRegOffset());

    rc = doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GR_CTX, getCtxRegOffset());
    if (rc == OK)
    {
        *regVal = regOp.regValueLo;
    }
    return rc;
}

//! \brief Tests the RegOp control call by performing context
//! (channel) specific reads and writes.
//!
//! Two channels and two contexts are created for the purpose of the
//! test. Values are read and written to the the two channels to
//! verify that channel specific reads and writes work correctly.
//!
//! \return OK if the passed, specific RC if failed
//!
RC RegOpsTest::testChannelOps()
{
    RC rc;
    RC tmpRc;
    UINT32 expectedVal1;
    UINT32 expectedVal2;
    UINT32 regVal;

    Printf(Tee::PriNormal, "[%s] -- Testing channel ops -- \n",
           __FUNCTION__);

    CHECK_RC(m_TestConfig.AllocateChannel(&pCh1, &m_hCh1, LW2080_ENGINE_TYPE_GR(0)));
    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&pCh2, &m_hCh2, LW2080_ENGINE_TYPE_GR(0)));

    CHECK_RC_CLEANUP(gr.Alloc(m_hCh1, GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(gr1.Alloc(m_hCh2, GetBoundGpuDevice()));

    regOpParam.hClientTarget = LwRmPtr()->GetClientHandle();

    //
    // read a value from context 2. This should not be affected by
    // write to a context 1.
    //
    rc = doChannelRead(m_hCh2, &expectedVal2);
    CHECK_RC_CLEANUP(rc);

    // channel specific write to channel 1
    rc = doChannelWrite(WRITE_VAL1, m_hCh1);
    CHECK_RC_CLEANUP(rc);

    //
    // now read the values from both the channels and verify that
    // they are what we expect them to be.
    //
    rc = doChannelRead(m_hCh1, &regVal);
    CHECK_RC_CLEANUP(rc);

    expectedVal1 = WRITE_VAL1 & getCtxRegAndMask(getCtxRegOffset());

    //We should and the mask read value too.
    regVal &= getCtxRegAndMask(getCtxRegOffset());

    REGOP_EXPECT_VAL(regVal, expectedVal1);

    rc = doChannelRead(m_hCh2, &regVal);
    CHECK_RC_CLEANUP(rc);

    //We should and the mask read value too.
    regVal &= getCtxRegAndMask(getCtxRegOffset());

    REGOP_EXPECT_VAL(regVal, expectedVal2);

    // same dance as above, but now value is written to channel 2
    rc = doChannelWrite(WRITE_VAL2, m_hCh2);
    CHECK_RC_CLEANUP(rc);

    rc = doChannelRead(m_hCh1, &regVal);
    CHECK_RC_CLEANUP(rc);

    //We should and the mask read value too.
    regVal &= getCtxRegAndMask(getCtxRegOffset());

    REGOP_EXPECT_VAL(regVal, expectedVal1);

    rc = doChannelRead(m_hCh2, &regVal);
    expectedVal2 = WRITE_VAL2 & getCtxRegAndMask(getCtxRegOffset());

    CHECK_RC_CLEANUP(rc);

    //We should and the mask read value too.
    regVal &= getCtxRegAndMask(getCtxRegOffset());

    REGOP_EXPECT_VAL(regVal, expectedVal2);

    if (tmpRc == OK)
    {
        Printf(Tee::PriNormal, "[%s] -- Test successful --\n",
               __FUNCTION__);
    }

Cleanup:
    gr.Free();
    gr1.Free();
    m_TestConfig.FreeChannel(pCh1);
    m_TestConfig.FreeChannel(pCh2);

    if (tmpRc != OK)
    {
        rc = tmpRc;
    }

    return rc;
}

//! \brief Tests the RegOp control call's context (channel) specific
//! read/write operations with bad parameters.
//!
//! Tests the control call with a bad arguments such as - Bad
//! ClientTarget, bad ChannelClient.
//! This is a negative test and is successful when the control call
//! fails.
//!
//! \return OK if the passed, RC::SOFTWARE_ERROR if failed
//!
RC RegOpsTest::testBadChannelClient()
{
    RC rc;
    RC tmpRc;
    LwU32 oldClient;
    LW2080_CTRL_GPU_REG_OP regOp;

    Printf(Tee::PriNormal, "[%s] -- Testing with a bad client/channel -- \n",
           __FUNCTION__);

    // Test with a bad client
    Printf(Tee::PriNormal, "[%s] Testing with a bad client target\n",
           __FUNCTION__);

    oldClient = regOpParam.hClientTarget;

    regOpParam.hClientTarget = WRITE_VAL1;

    RMT_INIT_REGOP(&regOp);

    tmpRc = doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, getScratchReg());
    if (tmpRc == OK)
    {
        Printf(Tee::PriNormal, "[%s] Unexpected return from Read test\n",
               __FUNCTION__);
        rc = RC::SOFTWARE_ERROR;
    }

    regOpParam.hClientTarget = oldClient;

    // Test with a bad channel.
    Printf(Tee::PriNormal, "[%s] Testing with a bad channel\n",
           __FUNCTION__);
    regOpParam.hChannelTarget = WRITE_VAL1;

    RMT_INIT_REGOP(&regOp);
    tmpRc = doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GR_CTX, getScratchReg());
    if (tmpRc == OK)
    {
        Printf(Tee::PriNormal, "[%s] Unexpected return from Write test\n",
               __FUNCTION__);
        rc = RC::SOFTWARE_ERROR;
    }

    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] -- Test complete --\n",
               __FUNCTION__);
    }
    return rc;
}

//! \brief Tests the RegOp control call with a bad operation (invalid
//! regOp value).
//!
//! This is a negative test and is successful iff the control call fails.
//!
//! \return OK on success, RC::SOFTWARE_ERROR on failure
//!
RC RegOpsTest::testBadRegOp()
{
    LW2080_CTRL_GPU_REG_OP regOp;
    RC rc;

    Printf(Tee::PriNormal, "[%s] Testing BadRegOP\n",
           __FUNCTION__);

    LwRmPtr pLwRm;
    rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                   LW2080_CTRL_CMD_GPU_EXEC_REG_OPS,
                                   NULL, 0);
    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] !! Unexpected return value from CtrlCmd\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    RMT_INIT_REGOP(&regOp);

    regOp.regOp = 0xf; //0xf is an invalid regOp;

    rc = exelwteRegOpCtrlCmd(&regOp, 1);

    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] BadRegOp test failed.",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    rc.Clear();

    if (regOp.regStatus != LW2080_CTRL_GPU_REG_OP_STATUS_ILWALID_OP)
    {
        Printf(Tee::PriNormal, "[%s] Unexpected regStatus: 0x%x Expecting: 0x%x\n",
               __FUNCTION__,
               regOp.regStatus,
               LW2080_CTRL_GPU_REG_OP_STATUS_ILWALID_OP);

        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriNormal, "[%s] Bad regOp test succeeded\n",
           __FUNCTION__);

    return OK;
}

//! \brief Tests the RegOp control call with a bad type of operation
//! (invalid regType).
//!
//! This is a negative test and is successful iff the control call fails.
//!
//! \return OK on success, RC::SOFTWARE_ERROR on failure
//!
RC RegOpsTest::testBadRegType()
{
    LW2080_CTRL_GPU_REG_OP regOp;
    RC retRc;
    RC rc;

    Printf(Tee::PriNormal, "[%s] Testing Bad regType\n",
           __FUNCTION__);
    RMT_INIT_REGOP(&regOp);

    regOp.regOp = LW2080_CTRL_GPU_REG_OP_READ_32;
    regOp.regType = 0xf; //0xf is an invalid regType

    rc = exelwteRegOpCtrlCmd(&regOp, 1);

    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] BadRegType test failed.",
               __FUNCTION__);
        retRc = RC::SOFTWARE_ERROR;
    }

    if (regOp.regStatus != LW2080_CTRL_GPU_REG_OP_STATUS_ILWALID_TYPE)
    {
        Printf(Tee::PriNormal, "[%s]Unexpected regStatus: 0x%x Expecting: 0x%x\n",
               __FUNCTION__,
               regOp.regStatus,
               LW2080_CTRL_GPU_REG_OP_STATUS_ILWALID_TYPE);

        retRc = RC::SOFTWARE_ERROR;
    }

    //try exelwting a context specific RegOp on a global reg
    RMT_INIT_REGOP(&regOp);
    regOp.regAndNMaskLo = 0xFFFFFFFF;
    rc = doWrite(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GR_CTX, getScratchReg(), WRITE_VAL1);

    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] BadRegType test failed.",
               __FUNCTION__);
        retRc =  RC::SOFTWARE_ERROR;
    }
    if (regOp.regStatus != LW2080_CTRL_GPU_REG_OP_STATUS_ILWALID_OFFSET)
    {
        Printf(Tee::PriNormal, "[%s]Unexpected regStatus: 0x%x Expecting: 0x%x\n",
               __FUNCTION__,
               regOp.regStatus,
               LW2080_CTRL_GPU_REG_OP_STATUS_ILWALID_TYPE);

        retRc = RC::SOFTWARE_ERROR;
    }

    if (retRc == OK)
    {
        Printf(Tee::PriNormal, "[%s] Bad regType test succeeded\n",
               __FUNCTION__);
    }

    return retRc;
}

//! \brief Tests the RegOp control call with bad parameters.
//!
//! The control call is exercised with a NULL regOps pointers and then
//! with an opCount of 0.
//!
//! This is a negative test and is successful iff the control call fails.
//!
//! \return OK on success, RC::SOFTWARE_ERROR on failure
//!
RC RegOpsTest::testBadParams()
{
    LW2080_CTRL_GPU_REG_OP regOp;
    RC rc;
    RC tmpRC;

    Printf(Tee::PriNormal, "[%s] -- Testing bad parameters --\n",
           __FUNCTION__);

    // NULL regOps
    rc = exelwteRegOpCtrlCmd(NULL, 1);
    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] NULL RegOps test failed.\n",
               __FUNCTION__);
        tmpRC = RC::SOFTWARE_ERROR;
    }

    // Zero opCount
    RMT_INIT_REGOP(&regOp);
    rc = exelwteRegOpCtrlCmd(&regOp, 0);
    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] Zero OpCount test failed.\n",
               __FUNCTION__);
        tmpRC = RC::SOFTWARE_ERROR;
    }

    return tmpRC;
}

//! \brief Tests the RegOp control call with a bad register address
//! (invalid regOffset) and other things which can cause failure in
//! the gpuValidateRegOpOffset() function.
//!
//! Tests with invalid register address, invalid group mask and a
//! misaligned register offset.
//!
//! This is a negative test and is successful iff the control call fails.
//!
//! \return OK on success, RC::SOFTWARE_ERROR on failure
//!
RC RegOpsTest::testBadOffset()
{
    LW2080_CTRL_GPU_REG_OP regOp;
    RC tmpRc;
    RC rc;

    Printf(Tee::PriNormal, "[%s] -- Testing Bad Offsets -- \n",
           __FUNCTION__);

    // misaligned offset
    RMT_INIT_REGOP(&regOp);

    regOp.regOp = LW2080_CTRL_GPU_REG_OP_READ_08;
    regOp.regOffset = 0x11111111; //0x11111111 is an invalid offset
    regOp.regType = LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL;

    tmpRc = exelwteRegOpCtrlCmd(&regOp, 1);

    if (tmpRc == OK)
    {
        Printf(Tee::PriNormal, "[%s] Misaligned offset test failed.\n",
               __FUNCTION__);
        rc = RC::SOFTWARE_ERROR;
    }
    rc.Clear();

    if (regOp.regStatus != LW2080_CTRL_GPU_REG_OP_STATUS_ILWALID_OFFSET)
    {
        Printf(Tee::PriNormal, "[%s] Unexpected regStatus: 0x%x Expecting: 0x%x\n",
               __FUNCTION__,
               regOp.regStatus,
               LW2080_CTRL_GPU_REG_OP_STATUS_ILWALID_OFFSET);

        rc = RC::SOFTWARE_ERROR;
    }

    // bad offset (bad register address)
    RMT_INIT_REGOP(&regOp);

    // 0xfffffff4 is a bad register address
    tmpRc = doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, 0xfffffff4);

    if (tmpRc == OK)
    {
        Printf(Tee::PriNormal, "[%s] Bad Offset test failed.",
               __FUNCTION__);
        rc = RC::SOFTWARE_ERROR;
    }

    if (regOp.regStatus != LW2080_CTRL_GPU_REG_OP_STATUS_ILWALID_OFFSET)
    {
        Printf(Tee::PriNormal, "[%s]Unexpected regStatus: 0x%x Expecting: 0x%x\n",
               __FUNCTION__,
               regOp.regStatus,
               LW2080_CTRL_GPU_REG_OP_STATUS_ILWALID_OFFSET);

        rc = RC::SOFTWARE_ERROR;
    }

    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] -- Bad Offsets test succeeded -- \n",
               __FUNCTION__);
    }

    return rc;
}

//! \brief Tests the behavior of the RegOp control call when bad
//! parameters are used.
//!
//! Internally calls various functions that tests for bad regOp, bad
//! regType, bad regOffset, bad group mask etc. among other things.
//!
//! This is a negative test and is successful iff the control call fails.
//!
//! \return OK on success, RC::SOFTWARE_ERROR on failure
//!
RC RegOpsTest::testBadOps()
{
    RC rc;
    RC retRc;

    Printf(Tee::PriNormal, "[%s] -- Testing bad operations -- \n",
           __FUNCTION__);

    REGOP_RUN_TEST(testBadRegOp);
    REGOP_RUN_TEST(testBadRegType);
    REGOP_RUN_TEST(testBadOffset);
    REGOP_RUN_TEST(testBadParams);
    REGOP_RUN_TEST(testBadChannelClient);

    if (retRc == OK)
    {
        Printf(Tee::PriNormal, "[%s] -- Testing bad operations complete -- \n",
               __FUNCTION__);
    }

    return retRc;
}

//! \brief Tests the basic functionality of writing and reading from a
//! global register.
//!
//! \return OK if the passed, specific RC if failed
//!
RC RegOpsTest::testReadAfterWrite()
{
    LW2080_CTRL_GPU_REG_OP regOp;
    RC rc;

    Printf(Tee::PriNormal, "[%s] -- Testing basic read after write --\n",
           __FUNCTION__);

    // Write a value to a global register.
    RMT_INIT_REGOP(&regOp);
    regOp.regAndNMaskLo = 0xFFFFFFFF;
    rc = doWrite(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, getScratchReg(), WRITE_VAL1);
    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] Value written at regOffset 0x%x : 0x%x\n",
               __FUNCTION__, (UINT32)regOp.regOffset, (UINT32)regOp.regValueLo);
        //
        // now read the value back and check if we got back the same
        // value that we wrote.
        //
        RMT_INIT_REGOP(&regOp);
        rc = doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, getScratchReg());
        if (rc == OK)
        {
            Printf(Tee::PriNormal, "[%s]Value read from regOffset 0x%x : 0x%x\n",
                   __FUNCTION__, (UINT32)regOp.regOffset, (UINT32)regOp.regValueLo);

            if (WRITE_VAL1 != regOp.regValueLo)
            {
                Printf(Tee::PriNormal, "[%s] Value mismatch!\n", __FUNCTION__);
                rc = RC::SOFTWARE_ERROR;
            }
        }
        else
        {
            Printf(Tee::PriNormal, "[%s] Reading failed!\n", __FUNCTION__);
        }
    }
    else
    {
        Printf(Tee::PriNormal, "[%s] Write failed!\n", __FUNCTION__);
    }

    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] -- Basic read after write test succeeded --\n",
               __FUNCTION__);
    }

    return rc;
}

//! \brief Tests the andMasks that used to write values to registers.
//!
//! Writes an initial value to a global register. After this value is
//! written, another value is written to the same register using an
//! andMask. After this write, the value from the register is read and
//! then checked again the expected value to verify that the control
//! call works correctly with andMasks.
//!
//! \return OK if the passed, specific RC if failed
//!
RC RegOpsTest::testAndMasks()
{
    LW2080_CTRL_GPU_REG_OP regOp[2];
    UINT32 expectedValue;
    RC rc;

    Printf(Tee::PriNormal, "[%s] -- Testing andMask --\n",
           __FUNCTION__);

    //
    // Initially, write a value in the scratch register. Then, exercise the masks.
    // write a value into the scratch register.
    //
    RMT_INIT_REGOP(&regOp[0]);
    regOp[0].regAndNMaskLo = 0xFFFFFFFF;
    rc = doWrite(&regOp[0], LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, getScratchReg(), WRITE_VAL1);
    if (rc == OK)
    {
        // now write and read using masks
        RMT_INIT_REGOP(&regOp[0]);

        regOp[0].regType = LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL;
        regOp[0].regOp = LW2080_CTRL_GPU_REG_OP_WRITE_32;
        regOp[0].regOffset = getScratchReg();
        regOp[0].regValueLo = WRITE_VAL2;
        regOp[0].regAndNMaskLo = AND_MASK;

        RMT_INIT_REGOP(&regOp[1]);

        regOp[1].regType = LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL;
        regOp[1].regOp = LW2080_CTRL_GPU_REG_OP_READ_32;
        regOp[1].regOffset = getScratchReg();

        rc = exelwteRegOpCtrlCmd(regOp, 2);
        if (rc == OK)
        {
            expectedValue = (WRITE_VAL1 & ~AND_MASK) | WRITE_VAL2;
            Printf(Tee::PriNormal, "[%s]Expecting: 0x%x Got: 0x%x\n",
                   __FUNCTION__, expectedValue, (UINT32)regOp[1].regValueLo);

            if (expectedValue != regOp[1].regValueLo)
            {
                Printf(Tee::PriNormal, "[%s] Value Mismatch!\n", __FUNCTION__);
                rc = RC::SOFTWARE_ERROR;
            }
        }
        else
        {
            Printf(Tee::PriNormal, "[%s] Failed to read from register!\n", __FUNCTION__);
        }
    }
    else
    {
        Printf(Tee::PriNormal, "[%s] Failed to write initial value\n", __FUNCTION__);
    }

    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] -- Testing andMask succeeded --\n", __FUNCTION__);
    }

    return rc;
}

//! \brief Tests if the RegOp control command works properly for TSG.
//!
//! Allocates two channels on a TSG and runs read/write tests on them
//! if the context share works for RegOps or not.
//!
//! \return OK on success, specific RC on error.
//!
RC RegOpsTest::testChannelGrp()
{
    RC rc;
    LwRm::Handle hCh1;
    LwRm::Handle hCh2;
    ChannelGroup chGrp(&m_TestConfig);
    Channel *pChUnshared;
    LwRm::Handle hChUnshared;
    ThreeDAlloc grCtx;

    Printf(Tee::PriNormal, "[%s] -- Testing Channel Group --\n",
           __FUNCTION__);

    CHECK_RC(m_TestConfig.AllocateChannel(&pChUnshared, &hChUnshared, LW2080_ENGINE_TYPE_GR(0)));
    CHECK_RC_CLEANUP(grCtx.Alloc(hChUnshared, GetBoundGpuDevice()));

    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());
    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh1));
    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh2));

    hCh1 = pCh1->GetHandle();
    hCh2 = pCh2->GetHandle();

    CHECK_RC_CLEANUP(gr.Alloc(hCh1, GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(gr1.Alloc(hCh2, GetBoundGpuDevice()));

    CHECK_RC_CLEANUP(chGrp.Schedule());

    CHECK_RC_CLEANUP(doSharedCtxChannelTest(hCh1, hCh2, hChUnshared));

Cleanup:
    grCtx.Free();
    gr.Free();
    gr1.Free();

    m_TestConfig.FreeChannel(pChUnshared);
    chGrp.FreeChannel(pCh1);
    chGrp.FreeChannel(pCh2);

    chGrp.Free();

    if (rc == OK)
    {
        Printf(Tee::PriNormal, "[%s] -- Testing Channel Group succeeded --\n", __FUNCTION__);
    }
    return rc;
}

//! \brief Shares a context between 2 channels and verifies that writes
//! from one channel show up in the other. Also verifies that the
//! values in a 3rd unshared channel remain unaffected.
//!
//! \param hCh1 Handle of the first channel
//! \param hCh2 Handle of the second channel
//! \param hChUnshared Handle of an independent channel.
//!
//! hCh1 and hCh2 share a context. hChUnshared has an independent
//! context not shared with either hCh1 or hCh2.
//!
//! \return OK on success, specific error no failure.
//!
RC RegOpsTest::doSharedCtxChannelTest
(
    LwRm::Handle hCh1,
    LwRm::Handle hCh2,
    LwRm::Handle hChUnshared
)
{
    UINT32 expectedVal;
    UINT32 regVal;
    RC rc;
    RC tmpRc;

    regOpParam.hClientTarget = LwRmPtr()->GetClientHandle();

    // write a val to unshared channel
    rc = doChannelWrite(WRITE_VAL2, hChUnshared);
    CHECK_RC(rc);

    //write to shared channel and read from channels
    rc = doChannelWrite(WRITE_VAL1, hCh1);
    CHECK_RC(rc);

    expectedVal = WRITE_VAL1 & getCtxRegAndMask(getCtxRegOffset());

    rc = doChannelRead(hCh1, &regVal);
    CHECK_RC(rc);
    //We should and the mask read value too.
    regVal &= getCtxRegAndMask(getCtxRegOffset());
    REGOP_EXPECT_VAL(regVal, expectedVal);

    rc = doChannelRead(hCh2, &regVal);
    CHECK_RC(rc);
    //We should and the mask read value too.
    regVal &= getCtxRegAndMask(getCtxRegOffset());
    REGOP_EXPECT_VAL(regVal, expectedVal);

    // read from the unshared channel
    expectedVal = WRITE_VAL2 & getCtxRegAndMask(getCtxRegOffset());
    rc = doChannelRead(hChUnshared, &regVal);
    CHECK_RC(rc);
    regVal &= getCtxRegAndMask(getCtxRegOffset());
    REGOP_EXPECT_VAL(regVal, expectedVal);

    //
    //write from one of the channels and read from channel group and
    //other channel.
    //
    rc = doChannelWrite(WRITE_VAL2, hCh2);
    CHECK_RC(rc);

    // write to the unshared channel
    rc = doChannelWrite(WRITE_VAL1, hChUnshared);
    CHECK_RC(rc);

    expectedVal = WRITE_VAL2 & getCtxRegAndMask(getCtxRegOffset());

    rc = doChannelRead(hCh1, &regVal);
    CHECK_RC(rc);
    //We should and the mask read value too.
    regVal &= getCtxRegAndMask(getCtxRegOffset());
    REGOP_EXPECT_VAL(regVal, expectedVal);

    rc = doChannelRead(hCh2, &regVal);
    CHECK_RC(rc);
    //We should and the mask read value too.
    regVal &= getCtxRegAndMask(getCtxRegOffset());
    REGOP_EXPECT_VAL(regVal, expectedVal);

    // read from the unshared channel
    expectedVal = WRITE_VAL1 & getCtxRegAndMask(getCtxRegOffset());
    rc = doChannelRead(hChUnshared, &regVal);
    CHECK_RC(rc);
    //We should and the mask read value too.
    regVal &= getCtxRegAndMask(getCtxRegOffset());
    REGOP_EXPECT_VAL(regVal, expectedVal);

    if (tmpRc != OK)
    {
        rc = tmpRc;
    }
    return rc;
}

//! \brief Tests the various scenarios where a context is shared
//! between 2 channels.
//! Exercises TSG and shared contexts.
//!
//! \return OK on success, specific RC on error.
//!
RC RegOpsTest::testContextShare()
{
    LwRmPtr pLwRm;
    RC rc;
    RC retRc;

    // Do channel group tests only if it is supported
    if (pLwRm->IsClassSupported(KEPLER_CHANNEL_GROUP_A, GetBoundGpuDevice()))
    {
        //
        // Add a subcontext test here, when ready
        //
        retRc = OK;
    }
    else
    {
        Printf(Tee::PriNormal, "Skipping shared ctx tests\n");
    }

    return retRc;
}

#ifdef LW_VERIF_FEATURES
// Helper class to deal with overriding the access map and making sure it is
// restored.
class AccessMap
{
    public:

    AccessMap(GpuSubdevice* sd)
        : subdev(sd), accessmap(DRF_SIZE(LW_RSPACE)/sizeof(LwU32)/8, 0xFF)
    {
        LwRmPtr pLwRm;
        LW208F_CTRL_GPU_SET_USER_REGISTER_ACCESS_PERMISSIONS params = {0};

        params.pNewAccessMap = LW_PTR_TO_LwP64(&accessmap[0]);
        params.newAccessMapSize = static_cast<LwU32>(accessmap.size());

        RC rc  = pLwRm->Control(subdev->GetSubdeviceDiag(),
                LW208F_CTRL_CMD_GPU_SET_USER_REGISTER_ACCESS_PERMISSIONS,
                &params, sizeof(params));
        MASSERT(rc == OK);

        oldMap = params.pOldAccessMap;
        oldSize = params.oldAccessMapSize;
    }

    ~AccessMap()
    {
        LwRmPtr pLwRm;
        LW208F_CTRL_GPU_SET_USER_REGISTER_ACCESS_PERMISSIONS params = {0};
        params.pNewAccessMap = oldMap;
        params.newAccessMapSize = oldSize;
        RC rc  = pLwRm->Control(subdev->GetSubdeviceDiag(),
                LW208F_CTRL_CMD_GPU_SET_USER_REGISTER_ACCESS_PERMISSIONS,
                &params, sizeof(params));
        MASSERT(rc == OK);
    }

    void ChangeRange(LwU32 start, LwU32 size, bool bAllow)
    {
        RC rc;
        LwRmPtr pLwRm;
        LW208F_CTRL_GPU_SET_USER_REGISTER_ACCESS_PERMISSIONS params = {0};

        params.offset = start;
        params.size = size;
        params.bAllow = bAllow;

        rc = pLwRm->Control(subdev->GetSubdeviceDiag(),
                    LW208F_CTRL_CMD_GPU_SET_USER_REGISTER_ACCESS_PERMISSIONS,
                    &params, sizeof(params));
        MASSERT(rc == OK);

        for (LwU32 i = start; i < start+size; i+=sizeof(LwU32))
        {
            MASSERT(CanAccess(i) == bAllow);
        }
    }

    bool CanAccess(LwU32 address)
    {
        return accessmap[address/sizeof(LwU32)/8] & (1<<(address/sizeof(LwU32)%8)) ? true : false;
    }

    private:
    GpuSubdevice* subdev;
    LwP64 oldMap;
    LwU32 oldSize;
    vector<LwU8> accessmap;
};
#endif

RC RegOpsTest::testPermissions()
{
#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GPU_REG_OP regOp = {0};
    RC rc;
    LwU32 addr = getScratchReg();
    AccessMap accessmap(GetBoundGpuSubdevice());

    // Set the PID to one that is not considered the administrator.
    RMFakeProcess::AsFakeProcess runas(42);
    MASSERT(RMFakeProcess::GetFakeProcessID() == 42);

    Printf(Tee::PriNormal, "[%s] -- Testing RegOps permissions --\n",
           __FUNCTION__);

    memset(&regOpParam, 0, sizeof(regOpParam));
    RMT_INIT_REGOP(&regOp);
    CHECK_RC(doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, addr));

    accessmap.ChangeRange(addr, 4, LW_FALSE);

    RMT_INIT_REGOP(&regOp);

    rc = doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, addr);

    if (rc == OK)
        CHECK_RC(RC::SOFTWARE_ERROR);
    else
        rc.Clear();

    accessmap.ChangeRange(addr, 4, LW_TRUE);
    CHECK_RC(doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, addr));

    return rc;
#else
    return OK;
#endif
}

RC RegOpsTest::testPermissionsRange(LwU32 start, LwU32 size, LwU32 minAddress, LwU32 maxSize, bool bAllow)
{
#ifdef LW_VERIF_FEATURES
    RC rc;
    LW2080_CTRL_GPU_REG_OP regOp = {0};
    memset(&regOpParam, 0, sizeof(regOpParam));
    RMT_INIT_REGOP(&regOp);

    rc = doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, start);

    if (bAllow)
        CHECK_RC(rc);
    else if (rc == OK)
    {
        Printf(Tee::PriHigh, "%s: Read of base succeeded when it shouldn't have\n", __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    else
        rc.Clear();

    rc = doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, start+size-4);

    if (bAllow)
        CHECK_RC(rc);
    else if (rc == OK)
    {
        Printf(Tee::PriHigh, "%s: Read of limit succeeded when it shouldn't have\n", __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    else
        rc.Clear();

    if (start-4 >= minAddress)
        CHECK_RC(doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, start-4));

    if (start + size < maxSize)
        CHECK_RC(doRead(&regOp, LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL, start+size));

    return rc;
#else
    return OK;
#endif
}

RC RegOpsTest::testPermissionsRandom()
{
#ifdef LW_VERIF_FEATURES
    RC rc;
    LwU32 iterations = 100;
    AccessMap accessmap(GetBoundGpuSubdevice());

    //
    // Since we don't want constant pri timeouts, use PRAMIN instead of all of
    // BAR0.
    //
    LwU32 maxSize = DRF_SIZE(LW_PRAMIN)/sizeof(LwU32);

    // Set the PID to one that is not considered the administrator.
    RMFakeProcess::AsFakeProcess runas(42);
    MASSERT(RMFakeProcess::GetFakeProcessID() == 42);

    Printf(Tee::PriNormal, "[%s] -- Testing RegOps permissions randomly --\n",
           __FUNCTION__);

    for (LwU32 i = 0; i < iterations; i++)
    {
        LW2080_CTRL_GPU_REG_OP regOp = {0};
        memset(&regOpParam, 0, sizeof(regOpParam));
        RMT_INIT_REGOP(&regOp);

        DefaultRandom::SeedRandom(0x4321);

        LwU32 start = DefaultRandom::GetRandom(DRF_BASE(LW_PRAMIN)/sizeof(LwU32), DRF_BASE(LW_PRAMIN)/sizeof(LwU32) + maxSize - 1);
        LwU32 size = DefaultRandom::GetRandom(1, (DRF_BASE(LW_PRAMIN)/sizeof(LwU32) + maxSize) - start + 1);

        start *= sizeof(LwU32);
        size *= sizeof(LwU32);

        LW208F_CTRL_GPU_SET_USER_REGISTER_ACCESS_PERMISSIONS params = {0};

        accessmap.ChangeRange(start, size, false);

        CHECK_RC(testPermissionsRange(start, size, DRF_BASE(LW_PRAMIN), maxSize, params.bAllow != LW_FALSE));

        accessmap.ChangeRange(start, size, true);
    }
    return rc;
#else
    return OK;
#endif
}
