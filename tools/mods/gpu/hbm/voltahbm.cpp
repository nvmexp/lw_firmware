/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "voltahbm.h"
#include "device/interface/i2c.h"
#include "hbm_spec_defines.h"

// The mask is the prefix for all PRIV related errors which can be found at
// https://wiki.lwpu.com/gpuhwvolta/index.php/Volta_PRIV#New_Table_of_PRIV_error_types
#define HBM_PRIV_LEVEL_VIOLATION_MASK 0xBADF0000

using namespace std;

//! A reset actually burns the fuses
// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::ResetSite(const UINT32 siteID) const
{
    RC rc;

    RegHal &regs = m_pGpuSub->Regs();
    UINT32 masterFbpaNum;
    CHECK_RC(m_pGpuSub->GetHBMSiteMasterFbpaNumber(siteID, &masterFbpaNum));

    // Put HBM into reset mode
    UINT32 regVal = regs.Read32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum);
    regs.SetField(&regVal, MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_MACRO_DRAM_RESET_ENABLED);
    regs.SetField(&regVal, MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_MACRO_DRAM_WRST_RESET_ENABLED);
    regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum, regVal);

    Tasker::Sleep(100); // Need to sleep for 100ms

    // Pull HBM out of reset mode
    regVal = regs.Read32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum);
    regs.SetField(&regVal, MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_MACRO_DRAM_RESET_DISABLED);
    regs.SetField(&regVal, MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_MACRO_DRAM_WRST_RESET_DISABLED);
    regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum, regVal);

    Tasker::Sleep(1); // Need to sleep for 200us

    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::WriteHBMDataReg(const UINT32 siteID, const UINT32 instruction,
    const vector<UINT32>& writeData, const UINT32 writeDataSize, const bool useHostToJtag) const
{
    RC rc;

    if (useHostToJtag)
    {
        Printf(Tee::PriError, "HBM writes are not supported via Host2Jtag on GV100\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    RegHal &regs = m_pGpuSub->Regs();
    UINT32 masterFbpaNum;
    CHECK_RC(m_pGpuSub->GetHBMSiteMasterFbpaNumber(siteID, &masterFbpaNum));

    // Clear the Error register in order to read the error code after writing
    // to the IEEE15000 Data register.
    regs.Write32(MODS_PPRIV_SYS_PRIV_ERROR_CODE, 0x0);

    // Write to the IEEE1500 Data Register
    for (const auto& data : writeData)
    {
        regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA, masterFbpaNum,
                     data);
    }

    // Check the error code in the Error register
    UINT32 errorCode = regs.Read32(MODS_PPRIV_SYS_PRIV_ERROR_CODE);
    if ((errorCode & 0xFFFF0000) == HBM_PRIV_LEVEL_VIOLATION_MASK)
    {
        Printf(Tee::PriError, "Write to IEEE HBM data register is priv protected." \
                              " Please acquire hulk license.\n");
        Printf(Tee::PriError, "For more information, please refer to " \
                              "https://confluence.lwpu.com/display/GM/Memory+Repair \n");
        Printf(Tee::PriLow,
               "Writing to IEEE1500 Data register failed with error: 0x%x\n", errorCode);
        return RC::PRIV_LEVEL_VIOLATION;
    }

    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::ReadHBMLaneRepairReg(const UINT32 siteID, const UINT32 instruction,
    const UINT32 channel, vector<UINT32>* readData, const UINT32 readSize,
    const bool useHostToJtag) const
{
    RC rc;

    if (useHostToJtag)
    {
        Printf(Tee::PriError, "HostToJtag reads are not supported on Volta\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    RegHal &regs = m_pGpuSub->Regs();
    UINT32 masterFbpaNum;
    CHECK_RC(m_pGpuSub->GetHBMSiteMasterFbpaNumber(siteID, &masterFbpaNum));

    regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR, masterFbpaNum,
                 instruction | 0x1000);

    regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_MODE, masterFbpaNum,
                 readSize);

    const UINT32 numDwords = (readSize + 31) / 32;
    readData->clear();
    readData->resize(numDwords, 0);
    for (UINT32 i = 0; i < numDwords; i++)
    {
        (*readData)[i] = regs.Read32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA,
                                     masterFbpaNum);
    }

    // The upper byte response comes back in the MSB portion of the register, so we shift down
    const UINT32 shamt = (32 - (readSize % 32));
    readData->back() >>= shamt;

#ifdef USE_FAKE_HBM_FUSE_DATA
    readData->clear();
    readData->resize(numDwords, 0);
#endif
    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::WriteBypassInstruction(const UINT32 siteID, const UINT32 channel) const
{
    RC rc;

    RegHal &regs = m_pGpuSub->Regs();
    UINT32 masterFbpaNum;
    CHECK_RC(m_pGpuSub->GetHBMSiteMasterFbpaNumber(siteID, &masterFbpaNum));

    regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR, masterFbpaNum,
                 0x1000 | (channel << 8));

    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::GetHBMDevIdDataHtoJ(
    const UINT32 siteID,
    vector<UINT32> *pRegValues) const
{
    Printf(Tee::PriError, "GetHBMDevIdDataHtoJ is not supported on Volta!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::GetHBMDevIdDataFbPriv(
    const UINT32 siteID,
    vector<UINT32> *pRegValues) const
{
    RC rc;

    RegHal &regs = m_pGpuSub->Regs();
    UINT32 masterFbpaNum;
    CHECK_RC(m_pGpuSub->GetHBMSiteMasterFbpaNumber(siteID, &masterFbpaNum));

    // Test that IEEE1500 interface reset is disabled (has a value of 1)
    if (!regs.Test32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO_DRAM_WRST_RESET,
                     masterFbpaNum, 1))
    {
        Printf(Tee::PriWarn, "IEEE1500 is not enabled for this site!\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    // Write reg to query stack height and ECC info
    UINT32 instruction = HBM_WIR_CHAN_ALL << 8 | HBM_WIR_INSTR_DEVICE_ID;
    regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR, masterFbpaNum,
                 instruction);
    Utility::Delay(10000);
    regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_MODE, masterFbpaNum,
                 HBM_WDR_LENGTH_DEVICE_ID);
    Utility::Delay(10000);

    // Poll for completion
    CHECK_RC(WaitForTestI1500(siteID, true));

    for (UINT32 i = 0; i < pRegValues->size(); ++i)
    {
        (*pRegValues)[i] = regs.Read32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA,
                                       masterFbpaNum);
    }

    // The upper byte response comes back in the MSB portion of the register, so we shift down
    const UINT32 responseShamt = (32 - (HBM_WDR_LENGTH_DEVICE_ID % 32));
    pRegValues->back() = pRegValues->back() >> responseShamt;

    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::AssertFBStop()
{
    Printf(Tee::PriError, "AssertFBStop is not supported on Volta!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::DeAssertFBStop()
{
    Printf(Tee::PriError, "DeAssertFBStop is not supported on Volta!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::ReadHostToJtagHBMReg(const UINT32 siteID, const UINT32 instructionID,
    vector<UINT32> *dataRead, const UINT32 dataReadLength) const
{
    Printf(Tee::PriError, "ReadHostToJtagHBMReg is not supported on Volta!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::WriteHostToJtagHBMReg(const UINT32 siteID, const UINT32 instructionID,
    const vector<UINT32>& writeData, const UINT32 dataWriteLength) const
{
    Printf(Tee::PriError, "WriteHostToJtagHBMReg is not supported on Volta!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::SelectChannel(const UINT32 siteID, const UINT32 channel) const
{
    Printf(Tee::PriError, "SelectChannel is not supported on Volta!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::AssertMask(const UINT32 siteID) const
{
    Printf(Tee::PriError, "AssertMask is not supported on Volta!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::ToggleClock(const UINT32 siteID, const UINT32 numTimes) const
{
    Printf(Tee::PriError, "ToggleClock is not supported on Volta!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

namespace
{
    const UINT32 MP2888_DCB_TYPE          = 0x45;
    const UINT32 GV100_MP2888_ADDR        = 0x38;
    const UINT32 GV100_MP2888_PORT        = 2;
    const UINT32 MP2888_LOCK_STATUS_REG   = 0x7E;
    const UINT32 MP2888_UNLOCKED_BIT      = 0x8;
    const UINT32 MP2888_LOCK_REG          = 0xF2;
    const UINT32 MP2888_UNLOCK_VALUE      = 0x347B;
    const UINT32 MP2888_LOCK_VALUE        = 0x1234;
    const UINT32 MP2888_VOLTAGE_REG       = 0x21;
    const UINT32 MP2888_VOLTAGE_DATASHIFT = 8;
    const UINT32 MP2888_VOLTAGE_STEP_UV   = 6250;

    const UINT32 MIN_VOLTAGE_UV           = 1000000;
    const UINT32 MAX_VOLTAGE_UV           = 1500000;
}

// -----------------------------------------------------------------------------
RC VoltaHBM::InitVoltageController()
{
    if (m_bVoltageControllerInit)
        return OK;

    RC rc;
    I2c::I2cDcbDevInfo i2cDcbDevInfo;
    CHECK_RC(m_pGpuSub->GetInterface<I2c>()->GetI2cDcbDevInfo(&i2cDcbDevInfo));

    for (UINT32 devIdx = 0;
          (devIdx < i2cDcbDevInfo.size()) && !m_bVoltageControllerPresent;
          ++devIdx)
    {
        auto const &dcbDev = i2cDcbDevInfo[devIdx];
        if ((dcbDev.Type == MP2888_DCB_TYPE) &&
            (dcbDev.I2CAddress == GV100_MP2888_ADDR) &&
            (dcbDev.I2CLogicalPort == GV100_MP2888_PORT))
        {
            m_bVoltageControllerPresent = true;
        }
    }
    if (!m_bVoltageControllerPresent)
    {
        Printf(Tee::PriWarn,
               "HBM MP2888 voltage control device not found on port %u, address 0x%2x\n",
               GV100_MP2888_PORT, GV100_MP2888_ADDR);
    }
    else
    {
        UINT32 lockStatus;
        I2c::Dev i2cDev = m_pGpuSub->GetInterface<I2c>()->I2cDev(GV100_MP2888_PORT,
                                                                 GV100_MP2888_ADDR);
        i2cDev.SetOffsetLength(1);
        i2cDev.SetMessageLength(1);
        CHECK_RC(i2cDev.Read(MP2888_LOCK_STATUS_REG, &lockStatus));
        if ((lockStatus & MP2888_UNLOCKED_BIT) != MP2888_UNLOCKED_BIT)
        {
            i2cDev.SetMessageLength(2);
            CHECK_RC(i2cDev.Write(MP2888_LOCK_REG, MP2888_UNLOCK_VALUE));
            i2cDev.SetMessageLength(1);
            CHECK_RC(i2cDev.Read(MP2888_LOCK_STATUS_REG, &lockStatus));
            if ((lockStatus & MP2888_UNLOCKED_BIT) != MP2888_UNLOCKED_BIT)
            {
                Printf(Tee::PriError, "Failed to unlock MP2888\n");
                m_bVoltageControllerPresent = false;
                m_bVoltageControllerInit    = true;
                return RC::CANNOT_SET_STATE;
            }
            m_bVoltageControllerLockOnExit = true;
        }
        i2cDev.SetMessageLength(2);
        CHECK_RC(i2cDev.Read(MP2888_VOLTAGE_REG, &m_OriginalVoltageReg));
    }
    m_bVoltageControllerInit = true;
    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::GetVoltageUv(UINT32 *pVoltUv)
{
    RC rc;
    CHECK_RC(InitVoltageController());

    if (!m_bVoltageControllerPresent)
        return RC::UNSUPPORTED_FUNCTION;

    I2c::Dev i2cDev = m_pGpuSub->GetInterface<I2c>()->I2cDev(GV100_MP2888_PORT, GV100_MP2888_ADDR);
    i2cDev.SetOffsetLength(1);
    i2cDev.SetMessageLength(2);
    CHECK_RC(i2cDev.Read(MP2888_VOLTAGE_REG, pVoltUv));

    *pVoltUv >>= MP2888_VOLTAGE_DATASHIFT;
    *pVoltUv *= MP2888_VOLTAGE_STEP_UV;

    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::SetVoltageUv(UINT32 voltUv)
{
    RC rc;
    CHECK_RC(InitVoltageController());

    if (!m_bVoltageControllerPresent)
        return RC::UNSUPPORTED_FUNCTION;

    if (voltUv % MP2888_VOLTAGE_STEP_UV)
    {
        UINT32 newVoltUv = ALIGN_DOWN(voltUv, MP2888_VOLTAGE_STEP_UV);
        Printf(Tee::PriWarn,
               "HBM voltage %uuV not a multiple of voltage step %uuV, rounding down to %uuV\n",
               voltUv, MP2888_VOLTAGE_STEP_UV, newVoltUv);
        voltUv = newVoltUv;
    }
    if (voltUv < MIN_VOLTAGE_UV)
    {
        Printf(Tee::PriWarn, "HBM voltage %uuV below minimum, clamping to %uuV\n",
               voltUv, MIN_VOLTAGE_UV);
        voltUv = MIN_VOLTAGE_UV;
    }
    else if (voltUv > MAX_VOLTAGE_UV)
    {
        Printf(Tee::PriWarn, "HBM voltage %uuV above maximum, clamping to %uuV\n",
               voltUv, MAX_VOLTAGE_UV);
        voltUv = MAX_VOLTAGE_UV;
    }

    I2c::Dev i2cDev = m_pGpuSub->GetInterface<I2c>()->I2cDev(GV100_MP2888_PORT, GV100_MP2888_ADDR);
    i2cDev.SetOffsetLength(1);
    i2cDev.SetMessageLength(2);
    const UINT32 mp2888VoltVal = (voltUv / MP2888_VOLTAGE_STEP_UV) << MP2888_VOLTAGE_DATASHIFT;
    CHECK_RC(i2cDev.Write(MP2888_VOLTAGE_REG, mp2888VoltVal));
    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC VoltaHBM::ShutDown()
{
    RC rc;

    if (m_bVoltageControllerPresent && (m_OriginalVoltageReg != 0))
    {
        I2c::Dev i2cDev = m_pGpuSub->GetInterface<I2c>()->I2cDev(GV100_MP2888_PORT, GV100_MP2888_ADDR);
        i2cDev.SetOffsetLength(1);
        i2cDev.SetMessageLength(2);
        CHECK_RC(i2cDev.Write(MP2888_VOLTAGE_REG, m_OriginalVoltageReg));
        if (m_bVoltageControllerLockOnExit)
        {
            CHECK_RC(i2cDev.Write(MP2888_LOCK_REG, MP2888_LOCK_VALUE));
        }
    }
    return rc;
}

CREATE_HBM_FUNCTION(VoltaHBM);
