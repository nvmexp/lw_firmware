/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2012,2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \rmt_privsectest.cpp
//! \brief Priv Security Verification test for Kepler and above chips
//!
//!

#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/tests/rmtest.h"
#include "kepler/gk104/dev_pmgr.h"
#include "kepler/gk104/dev_therm.h"
#include "kepler/gk104/dev_pwr_pri.h"

#include "class/cl208f.h" // LW20_SUBDEVICE_DIAG
#include "rm.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string>         // Only use <> for built-in C++ stuff
#include "core/include/memcheck.h"

//
// PRIV SECURITY Configuration settings
//
typedef struct
{
    LwU32                   regAddr;   // Register Address to program
    LwU32                   bitField;  // Bit Field to Program
    LwU32                   bitMask;   // Bit Bit Mask to Compare
} PRIV_SEC_REG_INFO, *PPRIV_SEC_REG_INFO;

//

//
// List Of PMGR and THERM Registers for Verification
//
PRIV_SEC_REG_INFO g_privSecRegVerifList[] =
{
    {
        LW_THERM_I2C_CFG,
        DRF_BASE(LW_THERM_I2C_CFG_TIMEOUT),
        DRF_SHIFTMASK(LW_THERM_I2C_CFG_TIMEOUT)
    },
    {
        LW_THERM_ARP_CFG,
        DRF_BASE(LW_THERM_ARP_CFG_ENABLE),
        DRF_SHIFTMASK(LW_THERM_ARP_CFG_ENABLE)
    },
    {
        LW_THERM_CONFIG1,
        DRF_BASE(LW_THERM_CONFIG1_BA_ENABLE),
        DRF_SHIFTMASK(LW_THERM_CONFIG1_BA_ENABLE)
    },
    {
        LW_THERM_I2C_ADDR,
        DRF_BASE(LW_THERM_I2C_ADDR_0),
        DRF_SHIFTMASK(LW_THERM_I2C_ADDR_0)
    },
    {
        LW_THERM_SLEEP_CTRL_0,
        DRF_BASE(LW_THERM_SLEEP_CTRL_0_WAKE_MASK_PCIE),
        DRF_SHIFTMASK(LW_THERM_SLEEP_CTRL_0_WAKE_MASK_PCIE)
    },
    {
        LW_PMGR_I2C0_ADDR,
        DRF_BASE(LW_PMGR_I2C0_ADDR_TEN_BIT),
        DRF_SHIFTMASK(LW_PMGR_I2C0_ADDR_TEN_BIT)
    },
    {
        LW_PMGR_I2C1_ADDR,
        DRF_BASE(LW_PMGR_I2C1_ADDR_TEN_BIT),
        DRF_SHIFTMASK(LW_PMGR_I2C1_ADDR_TEN_BIT)
    },
    {
        LW_PMGR_I2C2_ADDR,
        DRF_BASE(LW_PMGR_I2C2_ADDR_TEN_BIT),
        DRF_SHIFTMASK(LW_PMGR_I2C2_ADDR_TEN_BIT)
    },
    {
        LW_PMGR_I2C3_ADDR,
        DRF_BASE(LW_PMGR_I2C3_ADDR_TEN_BIT),
        DRF_SHIFTMASK(LW_PMGR_I2C3_ADDR_TEN_BIT)
    },
    {
        LW_PMGR_I2C4_ADDR,
        DRF_BASE(LW_PMGR_I2C4_ADDR_TEN_BIT),
        DRF_SHIFTMASK(LW_PMGR_I2C4_ADDR_TEN_BIT)
    },
    {
        LW_PMGR_I2C5_ADDR,
        DRF_BASE(LW_PMGR_I2C5_ADDR_TEN_BIT),
        DRF_SHIFTMASK(LW_PMGR_I2C5_ADDR_TEN_BIT)
    },
    {
        LW_PMGR_I2C6_ADDR,
        DRF_BASE(LW_PMGR_I2C6_ADDR_TEN_BIT),
        DRF_SHIFTMASK(LW_PMGR_I2C6_ADDR_TEN_BIT)
    },
    {
        LW_PMGR_I2C7_ADDR,
        DRF_BASE(LW_PMGR_I2C7_ADDR_TEN_BIT),
        DRF_SHIFTMASK(LW_PMGR_I2C7_ADDR_TEN_BIT)
    },
    {
        LW_PMGR_I2C8_ADDR,
        DRF_BASE(LW_PMGR_I2C8_ADDR_TEN_BIT),
        DRF_SHIFTMASK(LW_PMGR_I2C8_ADDR_TEN_BIT)
    },
    {
        LW_PMGR_I2C9_ADDR,
        DRF_BASE(LW_PMGR_I2C9_ADDR_TEN_BIT),
        DRF_SHIFTMASK(LW_PMGR_I2C9_ADDR_TEN_BIT)
    },
    {
        LW_PMGR_GPIO_PADCTL,
        DRF_BASE(LW_PMGR_GPIO_PADCTL_REG_LEVEL),
        DRF_SHIFTMASK(LW_PMGR_GPIO_PADCTL_REG_LEVEL)
    },
    {
        LW_PMGR_ROM_SERIAL_MODE,
        DRF_BASE(LW_PMGR_ROM_SERIAL_MODE_FASTREAD),
        DRF_SHIFTMASK(LW_PMGR_ROM_SERIAL_MODE_FASTREAD)
    },
    {
        LW_PMGR_TACH_CSR,
        DRF_BASE(LW_PMGR_TACH_CSR_RST),
        DRF_SHIFTMASK(LW_PMGR_TACH_CSR_RST)
    },
    {
        LW_PPWR_PMU_RESET_COUNTER,
        DRF_BASE(LW_PPWR_PMU_RESET_COUNTER_VAL),
        DRF_SHIFTMASK(LW_PPWR_PMU_RESET_COUNTER_VAL)
    },
};

class PrivSecTest : public RmTest
{
public:
    PrivSecTest();
    virtual ~PrivSecTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    GpuSubdevice *m_pSubdev;
    RC PrivSecVerify();
    RC WriteRegRM(LwU32 addr, LwU32 regValue);
    RC PrivSecVerifyRmAccess(LwU32 addr, LwU32 regValue,
                             LwU32 bitMask, LwU32 restoreValue);
    RC PrivSecVerifyCpuAccess(LwU32 addr, LwU32 regValue,
                              LwU32 bitMask, LwU32 restoreValue);
};

//! \brief PrivSecTest constructor
//!
//! \return NA
//------------------------------------------------------------------------------
PrivSecTest::PrivSecTest()
{
    m_pSubdev = GetBoundGpuSubdevice();
    SetName("PrivSecTest");
}

//! \brief PrivSecTest destructor
//!
//! \return NA
//------------------------------------------------------------------------------
PrivSecTest::~PrivSecTest()
{

}

//! \brief Check for Test Support
//!
//! \return string
//------------------------------------------------------------------------------
string PrivSecTest::IsTestSupported()
{
    // On fmodel we can't run this test due to lack of fmodel support.
    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        return "Not Supported on F-Model";
    }

    if( m_pSubdev->HasFeature(Device::GPUSUB_HAS_PRIV_SELWRITY))
    {
        return RUN_RMTEST_TRUE;
    }
    return "PrivSecTest not supported on pre-Kepler chips";
}

//! \brief Test Setup Function
//!
//! \return RC
//------------------------------------------------------------------------------
RC PrivSecTest::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    return OK;
}

//! \brief Test Exelwtion Control Function.
//!
//! \return RC
//------------------------------------------------------------------------------
RC PrivSecTest::Run()
{
    RC rc = OK;
    CHECK_RC(PrivSecVerify());
    return rc;
}

//! \brief Cleanup
//!
//! \return RC
//------------------------------------------------------------------------------
RC PrivSecTest::Cleanup()
{
    //Nothing required for Cleanup
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------
//! \brief Priv Security test control function
//!
//! \return RC
//------------------------------------------------------------------------------
RC PrivSecTest::PrivSecVerify()
{
    LwU32         regVal;
    LwU32         value;
    RC            rc;

    for(LwU32 index = 0;
              index < (sizeof(g_privSecRegVerifList)/sizeof(PRIV_SEC_REG_INFO));
              index++)
    {
        regVal = m_pSubdev->RegRd32( g_privSecRegVerifList[index].regAddr );

        //Toggle specified Bit on value read from register
        value = regVal ^ BIT(g_privSecRegVerifList[index].bitField);

        //Verify Access via RM
        CHECK_RC(PrivSecVerifyRmAccess(g_privSecRegVerifList[index].regAddr,
                                       value,
                                       g_privSecRegVerifList[index].bitMask,
                                       regVal));
        //Verify Access via CPU
        CHECK_RC(PrivSecVerifyCpuAccess(g_privSecRegVerifList[index].regAddr,
                                        value,
                                        g_privSecRegVerifList[index].bitMask,
                                        regVal));
    }
    return rc;
}

//! \brief Priv Security test Using RM
//!
//! \return RC
//------------------------------------------------------------------------------
RC PrivSecTest::PrivSecVerifyRmAccess
(
    LwU32 addr,
    LwU32 regValue,
    LwU32 bitMask,
    LwU32 restoreValue
)
{
    LwU32       actVal;
    RC          rc = OK;

    //Write Register Using RM control Call
    CHECK_RC(WriteRegRM(addr, regValue));
    //Read Register back for verification
    actVal = m_pSubdev->RegRd32(addr);

    if((regValue & bitMask) == (actVal & bitMask))
    {
        Printf(Tee::PriHigh,"%s : addr = 0x%08X PASS\n",
               __FUNCTION__, addr);
        //Restore original value
        CHECK_RC(WriteRegRM(addr, restoreValue));
    }
    else
    {
        Printf(Tee::PriHigh,
               "%s: Priv Security RM validation failed:"
               " addr = 0x%08X exp = 0x%08X act = 0x%08X mask = 0x%08X\n",
               __FUNCTION__, addr, regValue, actVal, bitMask);
        return RC::DATA_MISMATCH;
    }

    return rc;
}

//! \brief Priv Security test Using CPU
//!
//! \return RC
//------------------------------------------------------------------------------
RC PrivSecTest::PrivSecVerifyCpuAccess
(
    LwU32 addr,
    LwU32 regValue,
    LwU32 bitMask,
    LwU32 restoreValue
)
{
    LwU32         actVal;

    // Get Bar0
    void *pLinLwBase = m_pSubdev->GetLwBase();
    if(pLinLwBase == NULL)
    {
        Printf(Tee::PriHigh,
              "%s: Priv Security validation can not be performed using CPU.\n",
              __FUNCTION__);
        return OK;
    }

    MEM_WR32((((char *)pLinLwBase) + addr), regValue);

    actVal = m_pSubdev->RegRd32(addr);

    if((regValue & bitMask) == (actVal & bitMask))
    {
        Printf(Tee::PriHigh,
               "%s: Priv Security CPU validation failed:"
               " addr = 0x%08X exp = 0x%08X act = 0x%08X mask = 0x%08X\n",
               __FUNCTION__, addr, regValue, actVal, bitMask);

        //Restore original value
        WriteRegRM(addr, restoreValue);
        return RC::DATA_MISMATCH;
    }
    else
    {
        Printf(Tee::PriHigh,"%s: addr = 0x%08X PASS\n",
               __FUNCTION__, addr);
    }

    return OK;
}

//! \brief Function to Write a Register using RM
//!
//! \return RC
//------------------------------------------------------------------------------
RC PrivSecTest::WriteRegRM
(
    LwU32 addr,
    LwU32 regValue
)
{
    RC                                  rc;
    LW2080_CTRL_GPU_REG_OP              regWriteOp = { 0 };
    LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS params;
    LwRmPtr                             pLwRm;

    memset(&params, 0, sizeof(LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS));

    regWriteOp.regOp            = LW2080_CTRL_GPU_REG_OP_WRITE_32;
    regWriteOp.regType          = LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL;
    regWriteOp.regOffset        = addr;
    regWriteOp.regValueHi       = 0;
    regWriteOp.regValueLo       = regValue;
    regWriteOp.regAndNMaskHi    = 0;
    regWriteOp.regAndNMaskLo    = ~0;

    params.regOpCount = 1;
    params.regOps = LW_PTR_TO_LwP64(&regWriteOp);

    rc = pLwRm->ControlBySubdevice(
                        m_pSubdev,
                        LW2080_CTRL_CMD_GPU_EXEC_REG_OPS,
                        &params,
                        sizeof(params));

    if ((rc != OK) ||
        (regWriteOp.regStatus !=
         LW2080_CTRL_GPU_REG_OP_STATUS_SUCCESS))
    {
        Printf(Tee::PriHigh, "%s : Failed to write register 0x%08X using RM\n",
                __FUNCTION__, addr);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "%s : RC Message = %s\n",
                   __FUNCTION__, rc.Message());
        }
        else
        {
            Printf(Tee::PriHigh,"%s : Reg Op Status = %d\n",
                   __FUNCTION__, regWriteOp.regStatus);
        }
    }

    return rc;
}
//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(PrivSecTest, RmTest,
    "PrivSecTest RM test - Priv Security Sanity Test");

