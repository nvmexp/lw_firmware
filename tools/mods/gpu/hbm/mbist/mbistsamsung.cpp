/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mbistsamsung.h"
#include "gpu/hbm/hbm_spec_defines.h"

//-----------------------------------------------------------------------------
/* virtual */ RC SamsungMBist::StartMbistHostToJtagPath(const UINT32 siteID,
    const UINT32 mbistType) const
{
    RC rc;
    if (mbistType == HBM_MBIST_TYPE_HIGH_FREQUENCY)
    {
        MASSERT(!"Un-Implemented MBIST type.\n");
        CHECK_RC(RC::ILWALID_ARGUMENT);
    }

    vector<UINT32> writeData;
    switch (mbistType)
    {
        case HBM_MBIST_TYPE_SCAN_PATTERN:
            writeData.resize(1, MBIST_SCAN_PAT_WRDATA_1);
            CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, MBIST_SCAN_PAT_INST_1, writeData,
                MBIST_SCAN_PAT_WRDATA_LEN_1));

            if (GetStackHeight() == 8)
                writeData[0] = MBIST_SCAN_PAT_WRDATA_2_HEIGHT_8;
            else
                writeData[0] = MBIST_SCAN_PAT_WRDATA_2_HEIGHT_4;

            CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, MBIST_SCAN_PAT_INST_2, writeData,
                MBIST_SCAN_PAT_WRDATA_LEN_2));
            break;

        case HBM_MBIST_TYPE_MARCH_PATTERN:
            writeData.resize(1, MBIST_MARCH_PAT_WRDATA_1);
            CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, MBIST_MARCH_PAT_INST_1, writeData,
                MBIST_MARCH_PAT_WRDATA_LEN_1));

            if (GetStackHeight() == 8)
                writeData[0] = MBIST_MARCH_PAT_WRDATA_2_HEIGHT_8;
            else
                writeData[0] = MBIST_MARCH_PAT_WRDATA_2_HEIGHT_4;

            CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, MBIST_MARCH_PAT_INST_2, writeData,
                MBIST_MARCH_PAT_WRDATA_LEN_2));
            break;

        case HBM_MBIST_TYPE_HIGH_FREQUENCY:
        default:
            MASSERT(!"Un-Implemented MBIST type.\n");
            rc = RC::ILWALID_ARGUMENT;
            break;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC SamsungMBist::CheckMbistCompletionHostToJtagPath
(
    const UINT32 siteID
    ,const  UINT32 mbistType
    ,bool* pRepairable
    ,UINT32* pNumBadRows
) const
{
    RC rc;
    vector<UINT32> readData;
    MASSERT(pRepairable && pNumBadRows);

    switch (mbistType)
    {
        case HBM_MBIST_TYPE_SCAN_PATTERN:
            CHECK_RC(GetHBMImpl()->ReadHostToJtagHBMReg(siteID, MBIST_SCAN_PAT_INST_3, &readData,
                MBIST_SCAN_PAT_READ_LEN_3));
            break;

        case HBM_MBIST_TYPE_MARCH_PATTERN:
            CHECK_RC(GetHBMImpl()->ReadHostToJtagHBMReg(siteID, MBIST_MARCH_PAT_INST_3, &readData,
                MBIST_MARCH_PAT_READ_LEN_3));
            break;

        case HBM_MBIST_TYPE_HIGH_FREQUENCY:
        default:
            MASSERT(!"Un-Implemented MBIST type.\n");
            rc = RC::ILWALID_ARGUMENT;
            break;
    }

    if (readData[0] & (1 << INFO_BIT_MBIST_COMPLETE))
    {
        Printf(Tee::PriError, "MBIST incomplete.\n");
        CHECK_RC(RC::STATUS_INCOMPLETE);
    }

    if (readData[0] & (1 << INFO_BIT_RDQS_STATUS))
    {
        *pRepairable = false;
    }

    for (UINT32 i = INFO_BIT_FAILING_ROW_START;
        i <= INFO_BIT_FAILING_ROW_END; i++)
    {
        if (readData[0] & ( 1 << i ))
            (*pNumBadRows) += (1 << i);
    }

    Printf(Tee::PriNormal, "Status: MBIST Complete. Number of Failing rows = %u\n",
           *pNumBadRows);
    return rc;
}

//-----------------------------------------------------------------------------
RC SamsungMBist::CheckMbistCompletionFbToPriv
(
    const UINT32 siteID
    ,const  UINT32 mbistType
    ,bool* pRepairable
    ,UINT32* pNumBadRows
)
{
    MASSERT(pRepairable && pNumBadRows);
    RC rc;

    GpuSubdevice *gpuSubdev = GetGpuSubdevice();
    const RegHal &regs = gpuSubdev->Regs();
    UINT32 masterFbpaNum;
    CHECK_RC(gpuSubdev->GetHBMSiteMasterFbpaNumber(siteID, &masterFbpaNum));
    vector<UINT32> readData;

    // TODO: MODS does not have sequence yet (using the FB to priv path )
    // on how to check MBIST status of a specific type of MBIST on a specific site
    // the source below this will be modified once we have that information.

    UINT32 regValue = regs.Read32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA, masterFbpaNum);

    if (regValue & 1)
    {
        CHECK_RC(RC::STATUS_INCOMPLETE);
    }

    std::bitset<MAX_DATA_BITS> dataRegister;

    // Read 11 more 32 bits serially
    dataRegister.reset();
    for (UINT32 i = 0; i < NUM_MBIST_DATA_READS; i++)
    {
        if (i != 0) // already read the first 32 bit
        {
            regValue = regs.Read32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA, masterFbpaNum);
        }

        for (UINT32 j = 0; j < 32; j++)
        {
            dataRegister[32*i + j] = (regValue>>j) & 1;
        }
    }

    for (UINT32 i = INFO_BIT_FAILING_ROW_START;i <= INFO_BIT_FAILING_ROW_END; i++)
    {
        (*pNumBadRows) += (dataRegister[i] * (1 << (i - INFO_BIT_FAILING_ROW_START)));
    }

    Printf(Tee::PriNormal, "Number of rows failed : %d, RDQS failed : %d.\n",
           *pNumBadRows, static_cast<UINT32>(dataRegister[INFO_BIT_RDQS_STATUS]));

    if (readData[0] & (1 << INFO_BIT_RDQS_STATUS))
    {
        *pRepairable = false;
    }
    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC SamsungMBist::StartMBist(const UINT32 siteID, const UINT32 mbistType)
{
    RC rc;

    RegHal &regs = GetGpuSubdevice()->Regs();

    if (IsHostToJtagPath())
    {
        return StartMbistHostToJtagPath(siteID, mbistType);
    }

    // TODO: MODS does not have sequence yet (using the FB to priv path )
    // on how to run a specific type of MBIST on a specific site
    // the source below this will be modified once we have that information.

    // enable MBIST mode on all channels
    regs.Write32(MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_I1500_INSTR, ENABLE_MBIST_INST);

    // Poll for completion
    CHECK_RC(GetHBMImpl()->WaitForTestI1500(siteID, false, Tasker::GetDefaultTimeoutMs()));

    // Start MBIST
    regs.Write32(MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_I1500_INSTR, ENABLE_MBIST_INST);

    // Poll for completion
    CHECK_RC(GetHBMImpl()->WaitForTestI1500(siteID, false, Tasker::GetDefaultTimeoutMs()));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC SamsungMBist::CheckCompletionStatus(const UINT32 siteID,
    const UINT32 mbistType)
{
    RC rc;
    bool repairable = true;
    UINT32 numBadRows = 0;

    if (IsHostToJtagPath())
    {
        CHECK_RC(CheckMbistCompletionHostToJtagPath(siteID, mbistType,
            &repairable, &numBadRows));
    }
    else
    {
        CHECK_RC(CheckMbistCompletionFbToPriv(siteID, mbistType,
            &repairable, &numBadRows));
    }

    if (!repairable)
    {
        return RC::HBM_REPAIR_IMPOSSIBLE;
    }

    if (numBadRows > 0)
    {
        CHECK_RC(RC::HBM_BAD_ROWS_FOUND);
    }
    return rc;
}

//! Does a soft repair on rows. The soft repair info stays until a FB reset is done
//! Used to verify if HW fusing spare rows will work
//-----------------------------------------------------------------------------
/* virtual */ RC SamsungMBist::SoftRowRepair(const UINT32 siteID,
    const UINT32 stackID, const UINT32 channelID, const UINT32 row,
    const UINT32 bank, const bool firstRow, const bool lastRow)
{
    RC rc;

    if (!IsHostToJtagPath())
    {
        // Repair sequence is not available on FB priv path for GP100
        CHECK_RC(RC::UNSUPPORTED_FUNCTION);
    }

    UINT32 instructionID = channelID<<8 | TMRS_WRITE_WIR_BYTE_VAL;

    vector<UINT32> writeData(2);

    // TMRS write and MRDS write are only needed the first time
    // these writes are vendor specific WARs
    if (firstRow)
    {
        // WAR step 1, add a TMRS write
        writeData[0] = TMRS_WRITE_DATA_1;
        writeData[1] = TMRS_WRITE_DATA_2;
        CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, instructionID, writeData,
                    TMRS_WR_DATA_LEN));
        Tasker::Sleep(SOFT_REPAIR_REGWR_SLEEP_TIME_MS);

        writeData[0] = TMRS_WRITE_DATA_3;
        writeData[1] = TMRS_WRITE_DATA_4;
        CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, instructionID, writeData,
                    TMRS_WR_DATA_LEN));
        Tasker::Sleep(SOFT_REPAIR_REGWR_SLEEP_TIME_MS);

        writeData[0] = TMRS_WRITE_DATA_5;
        writeData[1] = TMRS_WRITE_DATA_6;
        CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, instructionID, writeData,
                    TMRS_WR_DATA_LEN));
        Tasker::Sleep(SOFT_REPAIR_REGWR_SLEEP_TIME_MS);

        // WAR step 2 - Do a mode register dump write
        instructionID = channelID<<8 | MRDS_WRITE_WIR_BYTE_VAL;

        writeData.resize(4, 0);
        writeData.assign(writeData.size(), 0);
        CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, instructionID, writeData,
                    MRDS_WR_DATA_LEN));
        Tasker::Sleep(SOFT_REPAIR_REGWR_SLEEP_TIME_MS);

        m_FirstSoftRepairInfo.siteID = siteID;
        m_FirstSoftRepairInfo.channelID = channelID;
    }

    // Begin soft repair
    vector<UINT32> softRepairWriteData(1, 0);
    softRepairWriteData[0] |= 1 << 20;
    softRepairWriteData[0] |= stackID << 19;
    softRepairWriteData[0] |= ((bank & 0xF)<<14);
    softRepairWriteData[0] |= (row & 0x3FFF);

    // need to write twice because of a timing issue
    instructionID = SOFT_REPAIR_INST_ID | channelID << 8;
    for (UINT32 j = 0; j < 2; j++)
    {
        CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, instructionID, softRepairWriteData,
            REPAIR_WR_DATA_LEN));
        Tasker::Sleep(SOFT_REPAIR_SLEEP_TIME_MS);
    }

    // a last dummy write is required with the 'soft repair enable' bit set to 0
    if (lastRow)
    {
        softRepairWriteData[0] &=  ~(1<<20);
        for (UINT32 j = 0; j < 2; j++)
        {
            CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, instructionID,
                softRepairWriteData, REPAIR_WR_DATA_LEN));
            Tasker::Sleep(SOFT_REPAIR_SLEEP_TIME_MS);
            m_SoftRepairComplete = true;
        }
    }

    return rc;
}

//! If a JTAG/FB reset is done, Soft repair info is lost. this function verifies if a reset was done
//-----------------------------------------------------------------------------
RC SamsungMBist::DidJtagResetOclwrAfterSWRepair(bool *pResetOclwrred) const
{
    MASSERT(pResetOclwrred);
    *pResetOclwrred = true;

    RC rc;

    if (!IsHostToJtagPath())
    {
        // Repair sequence is not available on FB priv path for GP100
        CHECK_RC(RC::UNSUPPORTED_FUNCTION);
    }

    // if the TMRS and MRDS registers dont hold their value, it means a RESET was done
    if (m_SoftRepairComplete)
    {
        // Read TMRS
        vector<UINT32> readData(2, 0);
        UINT32 instructionID = m_FirstSoftRepairInfo.channelID<<8 | TMRS_WRITE_WIR_BYTE_VAL;
        CHECK_RC(GetHBMImpl()->ReadHostToJtagHBMReg(m_FirstSoftRepairInfo.siteID,
            instructionID, &readData, TMRS_WR_DATA_LEN));
        Tasker::Sleep(SOFT_REPAIR_REGWR_SLEEP_TIME_MS);

        if (readData[0] != TMRS_WRITE_DATA_5 || readData[1] != TMRS_WRITE_DATA_6)
        {
            *pResetOclwrred = true;
            return rc;
        }

        // Reads MRDS
        /*
        instructionID = m_FirstSoftRepairInfo.channelID<<8 | MRDS_WRITE_WIR_BYTE_VAL;
        readData.resize(4);
        readData.assign(readData.size(), 0);

        CHECK_RC(GetHBMImpl()->ReadHostToJtagHBMReg(m_FirstSoftRepairInfo.siteID,
            instructionID, &readData, MRDS_WR_DATA_LEN));

        Tasker::Sleep(SOFT_REPAIR_REGWR_SLEEP_TIME_MS);

        if (readData[0] != 0 || readData[1] != 0 ||
            readData[2] != 0 || readData[3] != 0)
        {
            *pResetOclwrred = true;
            return rc;
        }
        */
        *pResetOclwrred = false;
    }

    return rc;
}

RC SamsungMBist::HardRowRepairHtoJ
(
    const bool isPseudoRepair
    ,const UINT32 siteID
    ,const UINT32 stackID
    ,const UINT32 channel
    ,const UINT32 row
    ,const UINT32 bank
) const
{
    RC rc;

    // Reset the HBM site before starting HW repair in case we did a SW repair earlier
    GetHBMImpl()->ResetSite(siteID);

    UINT32 instructionID = channel<<8 | HARD_REPAIR_INST_ID;
    const UINT32 numClockToggles = 800; //suggested by vendor

    // There are 2 spare rows available for every bank
    // Each row is made up of 2 dwords
    // Since AMAP does not tell which dword in the row is bad, we repair both dwords
    // fuse both dwords one after the other
    for (UINT32 dword = 0; dword < 2; dword++)
    {
        vector<UINT32> hardRepairWriteData(1, 0);
        hardRepairWriteData[0] |= 1 << 20;
        hardRepairWriteData[0] |= stackID << 19;
        hardRepairWriteData[0] |= ((bank & 0xF)<<14);
        hardRepairWriteData[0] |= (row & 0x3FFF);

        // bit RA[18] = 1 for dword 1 and 0 for dword 0
        if (dword == 1)
        {
            hardRepairWriteData[0] |= 1 << 18;
        }

        if (!isPseudoRepair)
        {
            CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, instructionID,
                hardRepairWriteData, REPAIR_WR_DATA_LEN));

            // Clock toggle is required for repair vector to be applied
            CHECK_RC(GetHBMImpl()->ToggleClock(siteID, numClockToggles));

            // select bypass
            hardRepairWriteData[0] = HARD_REPAIR_BYPASS_DATA;

            CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, HARD_REPAIR_BYPASS_INST_ID,
                hardRepairWriteData, HARD_REPAIR_BYPASS_WR_LEN));

            //CHECK_RC(GetHBMImpl()->ToggleClock(siteID, numClockToggles));
        }

        // Reset is required for fuse burn to happen
        CHECK_RC(GetHBMImpl()->ResetSite(siteID));
    }

    return rc;
}

RC SamsungMBist::HardRowRepairFbPriv
(
    const bool isPseudoRepair
    ,const UINT32 siteID
    ,const UINT32 stackID
    ,const UINT32 channel
    ,const UINT32 row
    ,const UINT32 bank
) const
{
    RC rc;
    GpuSubdevice *pGpuSubdev = GetGpuSubdevice();
    RegHal &regs = pGpuSubdev->Regs();
    UINT32 masterFbpaNum;
    CHECK_RC(pGpuSubdev->GetHBMSiteMasterFbpaNumber(siteID, &masterFbpaNum));

    // Reset the HBM site before starting HW repair in case we did a SW repair earlier
    GetHBMImpl()->ResetSite(siteID);

    const UINT32 numPseudoChannels = 2;

    for (UINT32 pseudoChannel = 0; pseudoChannel < numPseudoChannels; pseudoChannel++)
    {
        UINT32 instructionID = 0x1000 | (channel << 8 ) | HARD_REPAIR_INST_ID;

        UINT32 hardRepairWriteData = 0;
        hardRepairWriteData |= 1 << 20;
        hardRepairWriteData |= stackID << 19;
        hardRepairWriteData |= ((pseudoChannel & 0x1) << 18);
        hardRepairWriteData |= ((bank & 0xF)<<14);
        hardRepairWriteData |= (row & 0x3FFF);

        vector<UINT32> repairData = {hardRepairWriteData};

        if (!isPseudoRepair)
        {
            regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR,
                         masterFbpaNum, instructionID);

            regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_MODE,
                         masterFbpaNum, REPAIR_WR_DATA_LEN);

            CHECK_RC(GetHBMImpl()->WriteHBMDataReg(siteID, instructionID, repairData,
                     REPAIR_WR_DATA_LEN, false));
            Tasker::Sleep(500);

            // Write a Bypass instruction needed to reset the interface and allow for running
            // additional instructions
            regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR, masterFbpaNum, 0x00001F00);
        }
    }

    // Reset is required for fuse burn to happen
    return GetHBMImpl()->ResetSite(siteID);
}

//! Hard fuse the spare rows in a bank
//-----------------------------------------------------------------------------
RC SamsungMBist::HardRowRepair
(
    const bool isPseudoRepair
    ,const UINT32 siteID
    ,const UINT32 stackID
    ,const UINT32 channel
    ,const UINT32 row
    ,const UINT32 bank
) const
{
    RC rc;

    Printf(Tee::PriNormal, "Fusing siteID %d, stackID %d, channel %d, bank %d, row %d\n",
        siteID, stackID, channel, bank, row);

    if (IsHostToJtagPath())
    {
        rc = HardRowRepairHtoJ(isPseudoRepair, siteID, stackID, channel, row, bank);
    }
    else
    {
        rc = HardRowRepairFbPriv(isPseudoRepair, siteID, stackID, channel, row, bank);
    }

    return rc;
}

namespace
{
    const UINT32 numCols = 23;

    // Lookup table from Vendor
    // Gives the spare fuse address from a bank of a channel
    const bool enableFuseScanRegMask[64][numCols] = {
        {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}
    };

    UINT32 GetFuseScanRegValue(const UINT32 row)
    {
        UINT32 returlwalue = 0;
        for (UINT32 i = 0; i < numCols; i++)
        {
            returlwalue += ((1<<i) * static_cast<UINT32>(enableFuseScanRegMask[row][numCols - 1 - i]));
        }
        return returlwalue;
    }
}

//-----------------------------------------------------------------------------
/* virtual */ RC SamsungMBist::GetNumSpareRowsAvailableFbPriv(const UINT32 siteID,
    const UINT32 stackID, const UINT32 channel, const UINT32 bank, UINT32 *pNumRowsAvailable,
    bool doPrintResults) const
{
    RC rc;

    if (doPrintResults)
    {
        Printf(Tee::PriNormal,
               "Checking for spare rows in site %d, stackID %d, channel %d, bank %d.\n",
                siteID,
                stackID,
                channel,
                bank);
    }

    GpuSubdevice *pGpuSubdev = GetGpuSubdevice();
    RegHal &regs = pGpuSubdev->Regs();
    UINT32 masterFbpaNum;
    CHECK_RC(pGpuSubdev->GetHBMSiteMasterFbpaNumber(siteID, &masterFbpaNum));

    const UINT32 bankOffset = bank < 8 ? 0 : 16;
    const UINT32 numSpareRows = 2;
    const UINT32 numPseudoChannels = 2;
    const UINT32 responseShamt = (32 - ENABLE_FUSE_SCAN_WR_LENGTH);

    for (UINT32 row = 0; row < numSpareRows; row++)
    {
        for (UINT32 pseudoChannel = 0; pseudoChannel < numPseudoChannels; pseudoChannel++)
        {
            const UINT32 pseudoChannelOffset = pseudoChannel ? 16 : 0;

            UINT32 regWrVal = stackID ? (1 << 24) : (1 << 23);
            regWrVal |= GetFuseScanRegValue(pseudoChannelOffset + bankOffset + bank * 2 + row);

            UINT32 instructionID = 0x1000 | (channel << 8) | ENABLE_FUSE_SCAN_INST_ID;
            regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR, masterFbpaNum, instructionID);

            regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_MODE, masterFbpaNum, ENABLE_FUSE_SCAN_WR_LENGTH);
            vector<UINT32> writeData = {regWrVal};
            CHECK_RC(GetHBMImpl()->WriteHBMDataReg(siteID, instructionID,
                                   writeData, ENABLE_FUSE_SCAN_WR_LENGTH, false));
            Tasker::Sleep(1); // Need to wait for 2000ns

            // The 26 byte response comes back in the MSB portion of the register, so we shift down
            UINT32 regRdVal = regs.Read32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA, masterFbpaNum)
                >> responseShamt;
            if ((regRdVal & 0x1) == 0x1)
            {
                if (doPrintResults)
                {
                    Printf(Tee::PriNormal, "Fused row found: write: 0x%x, read: 0x%x\n",
                                           regWrVal, regRdVal);
                }
                break; // Don't bother checking another pseudo-channel if one of them is fused
            }
            else
            {
                if (doPrintResults)
                {
                    Printf(Tee::PriNormal, "Unfused row found: write: 0x%x, read: 0x%x\n",
                                           regWrVal, regRdVal);
                }
                if (pseudoChannel > 0)
                {
                    (*pNumRowsAvailable)++;
                }
            }

            // Write a Bypass instruction needed to reset the interface and allow for running
            // additional instructions
            regs.Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR, masterFbpaNum, 0x00001F00);
            Tasker::Sleep(100);
        }
    }

    return rc;
}

//!
//! This routines scans for available spare rows in a bank, if spare rows are not available
//! hard repair is not possible
//-----------------------------------------------------------------------------
/* virtual */ RC SamsungMBist::GetNumSpareRowsAvailableHtoJ(const UINT32 siteID, const UINT32 stackID,
    const UINT32 channel, const UINT32 bank, UINT32 *pNumRowsAvailable, bool doPrintResults) const
{
    RC rc;

    if (doPrintResults)
    {
        Printf(Tee::PriNormal,
               "Checking for spare rows in site %d, stackID %d, channel %d, bank %d.\n",
               siteID, stackID, channel, bank);
    }

    // set channel select
    // WSO mux selection is not 1:1 for all channels, (WSO channel mapping bug in HBM test macro, see bug 1671530)
    UINT32 setChannelSelect = channel;    //only needed for selecting channel, not for repair
    switch (channel)
    {
        case 0: setChannelSelect = 0; break;
        case 1: setChannelSelect = 1; break;
        case 2: setChannelSelect = 4; break;
        case 3: setChannelSelect = 5; break;
        case 4: setChannelSelect = 2; break;
        case 5: setChannelSelect = 3; break;
        case 6: setChannelSelect = 6; break;
        case 7: setChannelSelect = 7; break;
        default:
            MASSERT(!"Invalid Channel.\n");
            CHECK_RC(RC::ILWALID_ARGUMENT);
    }

    // The mask is set so that the FB priv path transaction should not reach the DRAM (only needed for MUX select)
    CHECK_RC(GetHBMImpl()->AssertMask(siteID));

    // Easier to use FB priv path sequence to do channel select (also avoid security issues with htoj)
    CHECK_RC(GetHBMImpl()->SelectChannel(siteID, setChannelSelect));

    UINT32 instructionID = channel<<8 | ENABLE_FUSE_SCAN_INST_ID;
    const UINT32 bankOffset = bank < 8 ? 0 : 16;
    const UINT32 dword0Offset = 0;
    const UINT32 dword1Offset = 16;

    const UINT32 numClockToggles = 600; // need WRCLK cycles, (as per vendor)
    vector<UINT32> bypassdata(1, 0);

    // Check if any of the spare rows (row 0 and row 1) are available
    // Since a row is made up of 2 dwords, check both dword

    // The way this works is, we write a reg value (queried using the look up table above) to read
    // This regvalue is different for every bank/dword
    // If the read value of the register has LSB set to 1, it means the spare row was used

    vector<UINT32> row0_Dword0_Reg(1, 0);
    row0_Dword0_Reg[0] = stackID ? 1<<24 : 1 <<23;
    row0_Dword0_Reg[0] |= GetFuseScanRegValue(dword0Offset + bankOffset + bank * 2);
    vector<UINT32> row0_Dword0_RegValue(1, 0);

    CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, instructionID,
        row0_Dword0_Reg, ENABLE_FUSE_SCAN_WR_LENGTH));
    CHECK_RC(GetHBMImpl()->ToggleClock(siteID, numClockToggles));
    CHECK_RC(GetHBMImpl()->ReadHostToJtagHBMReg(siteID, instructionID,
        &row0_Dword0_RegValue, ENABLE_FUSE_SCAN_WR_LENGTH));
    CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID,
        HARD_REPAIR_BYPASS_INST_ID, bypassdata, HARD_REPAIR_BYPASS_WR_LEN));

    // Row 0 Dword 1
    vector<UINT32> row0_Dword1_Reg(1, 0);
    row0_Dword1_Reg[0] = stackID ? 1<<24 : 1 <<23;
    row0_Dword1_Reg[0] |= GetFuseScanRegValue(dword1Offset + bankOffset + bank * 2);
    vector<UINT32> row0_Dword1_RegValue(1, 0);
    CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, instructionID,
        row0_Dword1_Reg, ENABLE_FUSE_SCAN_WR_LENGTH));
    CHECK_RC(GetHBMImpl()->ToggleClock(siteID, numClockToggles));
    CHECK_RC(GetHBMImpl()->ReadHostToJtagHBMReg(siteID, instructionID,
        &row0_Dword1_RegValue, ENABLE_FUSE_SCAN_WR_LENGTH));
    CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID,
        HARD_REPAIR_BYPASS_INST_ID, bypassdata, HARD_REPAIR_BYPASS_WR_LEN));

    // Row 1 Dword 0
    vector<UINT32> row1_Dword0_Reg(1, 0);
    row1_Dword0_Reg[0] = stackID ? 1<<24 : 1 <<23;
    row1_Dword0_Reg[0] |= GetFuseScanRegValue(dword0Offset + bankOffset + bank * 2 + 1);
    vector<UINT32> row1_Dword0_RegValue(1, 0);
    CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, instructionID,
        row1_Dword0_Reg, ENABLE_FUSE_SCAN_WR_LENGTH));
    CHECK_RC(GetHBMImpl()->ToggleClock(siteID, numClockToggles));
    CHECK_RC(GetHBMImpl()->ReadHostToJtagHBMReg(siteID, instructionID,
        &row1_Dword0_RegValue, ENABLE_FUSE_SCAN_WR_LENGTH));
    CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID,
        HARD_REPAIR_BYPASS_INST_ID, bypassdata, HARD_REPAIR_BYPASS_WR_LEN));

    // Row 1 Dword 1
    vector<UINT32> row1_Dword1_Reg(1, 0);
    row1_Dword1_Reg[0] = stackID ? 1<<24 : 1 <<23;
    row1_Dword1_Reg[0] |= GetFuseScanRegValue(dword1Offset + bankOffset + bank * 2 + 1);
    vector<UINT32> row1_Dword1_RegValue(1, 0);

    CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, instructionID,
        row1_Dword1_Reg, ENABLE_FUSE_SCAN_WR_LENGTH));
    CHECK_RC(GetHBMImpl()->ToggleClock(siteID, numClockToggles));
    CHECK_RC(GetHBMImpl()->ReadHostToJtagHBMReg(siteID, instructionID,
        &row1_Dword1_RegValue, ENABLE_FUSE_SCAN_WR_LENGTH));
    CHECK_RC(GetHBMImpl()->WriteHostToJtagHBMReg(siteID, HARD_REPAIR_BYPASS_INST_ID,
        bypassdata, HARD_REPAIR_BYPASS_WR_LEN));

    // Read the registers to decipher if rows are available, reading the same value as that was
    // written with LSB = 1 confirms that the row is *not* available. i.e it was HW fused.
    if ((row0_Dword0_RegValue[0] == (row0_Dword0_Reg[0] | 1)) ||
        (row0_Dword1_RegValue[0] == (row0_Dword1_Reg[0] | 1)))
    {
        if (doPrintResults)
        {
            Printf(Tee::PriNormal, "Row 0 was fused. ");
            Printf(Tee::PriNormal, "read for row 0: write: 0x%x 0x%x, read: 0x%x 0x%x\n",
                row0_Dword0_Reg[0], row0_Dword1_Reg[0],
                row0_Dword0_RegValue[0], row0_Dword1_RegValue[0]);
        }
    }
    else
    {
        (*pNumRowsAvailable)++;
        if (doPrintResults)
        {
            Printf(Tee::PriNormal, "Unfused Row 0 found: read for row 0: "
                                   "write: 0x%x 0x%x, read: 0x%x 0x%x\n",
                row0_Dword0_Reg[0], row0_Dword1_Reg[0],
                row0_Dword0_RegValue[0], row0_Dword1_RegValue[0]);
        }
    }

    if ((row1_Dword0_RegValue[0] == (row1_Dword0_Reg[0] | 1)) ||
        (row1_Dword1_RegValue[0] == (row1_Dword1_Reg[0] | 1)))
    {
        if (doPrintResults)
        {
            Printf(Tee::PriNormal, "Row 1 was fused. ");
            Printf(Tee::PriNormal, "read for row 1: write: 0x%x 0x%x, read: 0x%x 0x%x\n",
                row1_Dword0_Reg[0], row1_Dword1_Reg[0],
                row1_Dword0_RegValue[0], row1_Dword1_RegValue[0]);
        }
    }
    else
    {
        (*pNumRowsAvailable)++;
        if (doPrintResults)
        {
            Printf(Tee::PriNormal, "Unfused Row 1 found: read for row 1: "
                                   "write: 0x%x 0x%x, read: 0x%x 0x%x\n",
                row1_Dword0_Reg[0], row1_Dword1_Reg[0],
                row1_Dword0_RegValue[0], row1_Dword1_RegValue[0]);
        }
    }

    return rc;
}

//! This routines scans for available spare rows in a bank, if spare rows are not available
//! hard repair is not possible
//-----------------------------------------------------------------------------
/* virtual */ RC SamsungMBist::GetNumSpareRowsAvailable(const UINT32 siteID, const UINT32 stackID,
    const UINT32 channel, const UINT32 bank, UINT32 *pNumRowsAvailable, bool doPrintResults) const
{
    RC rc;

    MASSERT(pNumRowsAvailable);
    *pNumRowsAvailable = 0;

    if (IsHostToJtagPath())
    {
        rc = GetNumSpareRowsAvailableHtoJ(siteID, stackID, channel, bank, pNumRowsAvailable, doPrintResults);
    }
    else
    {
        rc = GetNumSpareRowsAvailableFbPriv(siteID, stackID, channel, bank, pNumRowsAvailable, doPrintResults);
    }

#ifdef USE_FAKE_HBM_FUSE_DATA
    *pNumRowsAvailable = 2;
#endif
    return rc;
}
