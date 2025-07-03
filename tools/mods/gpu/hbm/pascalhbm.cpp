/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pascalhbm.h"
#include "hbm_spec_defines.h"
#include "gpu/include/gpusbdev.h"
#include "pascal/gp100/dev_pwr_pri.h"
#include "pascal/gp100/dev_fifo.h"
#include "pascal/gp100/dev_bus_addendum.h"
#include "pascal/gp100/dev_bus.h"

//------------------------------------------------------------------------------
//! \brief Constructor
//!
PascalHBM::PascalHBM(GpuSubdevice *pSubdev)
 : HBMImpl(pSubdev)
{
    m_HBMResetDelayMs = 100;
}

//-----------------------------------------------------------------------------
/* static */ bool PascalHBM::PollForFBStopAck(void *pVoidPollArgs)
{
    UINT32 regValue = ((GpuSubdevice *)pVoidPollArgs)->RegRd32(LW_PPWR_PMU_GPIO_INPUT);
    return FLD_TEST_DRF(_PPWR, _PMU_GPIO_INPUT, _FB_STOP_ACK, _TRUE, regValue);
}

//-----------------------------------------------------------------------------
/* static */ bool PascalHBM::PollForFBStartAck(void *pVoidPollArgs)
{
    UINT32 regValue = ((GpuSubdevice *)pVoidPollArgs)->RegRd32(LW_PPWR_PMU_GPIO_INPUT);
    return FLD_TEST_DRF(_PPWR, _PMU_GPIO_INPUT, _FB_STOP_ACK, _FALSE, regValue);
}

//-----------------------------------------------------------------------------
RC PascalHBM::AssertFBStop()
{
    RC rc;
    // disable all non-posted pipeline requestors
    UINT32 regValue = m_pGpuSub->RegRd32(LW_PBUS_ACCESS);
    m_pGpuSub->RegWr32(LW_PBUS_ACCESS,
                    (regValue & ~LW_PBUS_ACCESS_NONPOSTED_REQ_MASK) |
                    LW_PBUS_ACCESS_NONPOSTED_REQ_MASK_DISABLED);

    // disable all posted pipeline requestors
    regValue = m_pGpuSub->RegRd32(LW_PBUS_ACCESS);
    m_pGpuSub->RegWr32(LW_PBUS_ACCESS,
                    (regValue & ~LW_PBUS_ACCESS_POSTED_REQ_MASK) |
                    LW_PBUS_ACCESS_POSTED_REQ_MASK_DISABLED);

    // Stop host from sending additional requests to FB
    regValue = m_pGpuSub->RegRd32(LW_PFIFO_FB_IFACE);
    m_pGpuSub->RegWr32(LW_PFIFO_FB_IFACE,
                    FLD_SET_DRF(_PFIFO, _FB_IFACE, _CONTROL, _DISABLE, regValue));

    // Add delay, bug Bug 541083
    regValue = m_pGpuSub->RegRd32(LW_PFIFO_FB_IFACE);

    //  assert FB_STOP_REQ
    m_pGpuSub->RegWr32(LW_PPWR_PMU_GPIO_OUTPUT_SET,
                    DRF_SHIFTMASK(LW_PPWR_PMU_GPIO_OUTPUT_SET_FB_STOP_REQ));

    // Poll for ACK
    CHECK_RC(POLLWRAP_HW(PollForFBStopAck,
        static_cast<void*>(m_pGpuSub), Tasker::GetDefaultTimeoutMs()));

    return rc;
}

//-----------------------------------------------------------------------------
RC PascalHBM::DeAssertFBStop()
{
    RC rc;
    // De assert FB_STOP_REQ
    m_pGpuSub->RegWr32(LW_PPWR_PMU_GPIO_OUTPUT_CLEAR,
                    DRF_SHIFTMASK(LW_PPWR_PMU_GPIO_OUTPUT_CLEAR_FB_STOP_REQ));

    // Poll for ACK
    CHECK_RC(POLLWRAP_HW(PollForFBStartAck,
        static_cast<void*>(m_pGpuSub), Tasker::GetDefaultTimeoutMs()));

    // Enable host's interface with the FB.
    UINT32 regValue = m_pGpuSub->RegRd32(LW_PFIFO_FB_IFACE);
    m_pGpuSub->RegWr32(LW_PFIFO_FB_IFACE,
                    FLD_SET_DRF(_PFIFO, _FB_IFACE, _CONTROL, _ENABLE, regValue));

    // enable all posted pipeline requestors
    regValue = m_pGpuSub->RegRd32(LW_PBUS_ACCESS);
    m_pGpuSub->RegWr32(LW_PBUS_ACCESS,
                    (regValue & ~LW_PBUS_ACCESS_POSTED_REQ_MASK) |
                    LW_PBUS_ACCESS_POSTED_REQ_MASK_ENABLED);

    // enable all non-posted pipeline requestors
    regValue = m_pGpuSub->RegRd32(LW_PBUS_ACCESS);
    m_pGpuSub->RegWr32(LW_PBUS_ACCESS,
                    (regValue & ~LW_PBUS_ACCESS_NONPOSTED_REQ_MASK) |
                    LW_PBUS_ACCESS_NONPOSTED_REQ_MASK_ENABLED);
    return rc;
}

//! required for repair vector to be applied
// -----------------------------------------------------------------------------
/* virtual */ RC PascalHBM::ToggleClock(const UINT32 siteID, const UINT32 numTimes) const
{
    RC rc;

    vector<GpuSubdevice::JtagCluster> jc, jc1;
    jc.push_back(GpuSubdevice::JtagCluster(JTAG_CLUSTER_CHIPLET_ID2, JTAG_CLUSTER_PADCAL_VALS_INSTR));

    UINT32 padcalCLen = 0;
    switch (siteID)
    {
        case 0:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_A;
            break;
        case 1:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_B;
            break;
        case 2:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_C;
            break;
        case 3:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_D;
            break;
        default:
            MASSERT(!"Invalid site number.\n");
            break;
    }

    for (UINT32 i= 0; i< numTimes; i++)
    {
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc, 0, TRANSMIT_DATA_VALUE_WCLK));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc, 0, TRANSMIT_DATA_VALUE_INIT));
    }

    return rc;
}

// -----------------------------------------------------------------------------
//!
//! \brief Set the mask so that the FB priv path transaction does not reach the DRAM.
//!
/* virtual */ RC PascalHBM::AssertMask(const UINT32 siteID) const
{
    RC rc;

    vector<GpuSubdevice::JtagCluster> jc;
    jc.push_back(GpuSubdevice::JtagCluster(JTAG_CLUSTER_CHIPLET_ID2, JTAG_CLUSTER_PADCAL_MASK_INSTR));
    UINT32 padCalMask = 0, padCalIndex = 0;

    switch (siteID)
    {
        case 0:
            padCalIndex = 2;
            padCalMask = PADCAL_MASK_SITE_A;
            break;
        case 1:
            padCalIndex = 1;
            padCalMask = PADCAL_MASK_SITE_B;
            break;
        case 2:
            padCalIndex = 3;
            padCalMask = PADCAL_MASK_SITE_C;
            break;
        case 3:
            padCalIndex = 4;
            padCalMask = PADCAL_MASK_SITE_D;
            break;
        default:
            MASSERT(!"Invalid site number.\n");
            break;
    }

    // set mask to 1
    CHECK_RC(WriteHost2JtagHelper(PADCAL_MASK_CHAIN_LENGTH, jc, padCalIndex, padCalMask));

    return rc;
}

//! A reset actually burns the fuses
// -----------------------------------------------------------------------------
/* virtual */ RC PascalHBM::ResetSite(const UINT32 siteID) const
{
    RC rc;

    vector<GpuSubdevice::JtagCluster> jc, jc1;
    jc.push_back(GpuSubdevice::JtagCluster(JTAG_CLUSTER_CHIPLET_ID2, JTAG_CLUSTER_PADCAL_VALS_INSTR));

    UINT32 padcalCLen = 0;
    switch (siteID)
    {
        case 0:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_A;
            break;
        case 1:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_B;
           break;
        case 2:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_C;
            break;
        case 3:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_D;
            break;
        default:
            MASSERT(!"Invalid site number.\n");
            break;
    }

    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc, 0, 0));
    Tasker::Sleep(m_HBMResetDelayMs);
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc, 0, 1));
    Tasker::Sleep(m_HBMResetDelayMs);
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc, 0, TRANSMIT_DATA_VALUE_INIT));
    Tasker::Sleep(m_HBMResetDelayMs);
    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC PascalHBM::WriteHost2JtagHelper(
    const UINT32 chainLength,
    vector<GpuSubdevice::JtagCluster> jc,
    const UINT32 index,
    const UINT32 value) const
{
    RC rc;
    const UINT32 dataLength = (chainLength + 31)/32;
    vector<UINT32> data(dataLength, 0);
    data[index] = value;
    CHECK_RC(m_pGpuSub->WriteHost2Jtag(jc, chainLength, data));
    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC PascalHBM::RWHostToJtagHBMReg(
    const bool read, // Read or Write
    const UINT32 siteID,
    const UINT32 instructionID,
    vector<UINT32> *rwData,
    const UINT32 rwDataLen) const
{
    RC rc;

    vector<GpuSubdevice::JtagCluster> jc0, jc1, jc2, jc3;
    jc0.push_back(GpuSubdevice::JtagCluster(JTAG_CLUSTER_CHIPLET_ID2, JTAG_CLUSTER_INSTRUCTION_ID));
    jc1.push_back(GpuSubdevice::JtagCluster(JTAG_CLUSTER_CHIPLET_ID2, JTAG_CLUSTER_PADCAL_VALS_INSTR));
    jc2.push_back(GpuSubdevice::JtagCluster(JTAG_CLUSTER_CHIPLET_ID2, JTAG_CLUSTER_PADCAL_MASK_INSTR));
    jc3.push_back(GpuSubdevice::JtagCluster(JTAG_CLUSTER_CHIPLET_ID3, JTAG_CLUSTER_DEVID_INSTR_1));
    jc3.push_back(GpuSubdevice::JtagCluster(JTAG_CLUSTER_CHIPLET_ID4, JTAG_CLUSTER_DEVID_INSTR_2));

    UINT32 chainLength = IOBIST_CONFIG0_CHAIN_LENGTH;
    UINT32 dataLength = (chainLength + 31)/32;
    vector<UINT32> data(dataLength, 0);
    data[SITE_A_INDEX] = SITE_A_BYPASS;
    data[SITE_B_INDEX] = SITE_B_BYPASS;
    data[SITE_C_INDEX] = SITE_C_BYPASS;
    data[SITE_D_INDEX] = SITE_D_BYPASS;

    UINT32 dataIndex = 0, padCalMask = 0, padCalIndex = 0;
    UINT32 padcalCLen = 0;  //pad cal chain length
    switch (siteID)
    {
        case 0:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_A;
            dataIndex = SITE_A_INDEX;
            padCalIndex = 2;
            padCalMask = PADCAL_MASK_SITE_A;
            break;
        case 1:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_B;
            dataIndex = SITE_B_INDEX;
            padCalIndex = 1;
            padCalMask = PADCAL_MASK_SITE_B;
            break;
        case 2:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_C;
            dataIndex = SITE_C_INDEX;
            padCalIndex = 3;
            padCalMask = PADCAL_MASK_SITE_C;
            break;
        case 3:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_D;
            dataIndex = SITE_D_INDEX;
            padCalIndex = 4;
            padCalMask = PADCAL_MASK_SITE_D;
            break;
        default:
            MASSERT(!"Invalid site number.\n");
            break;
    }

    data[dataIndex] = 0;
    CHECK_RC(m_pGpuSub->WriteHost2Jtag(jc0, chainLength, data));

    // set mask to 1
    CHECK_RC(WriteHost2JtagHelper(PADCAL_MASK_CHAIN_LENGTH, jc2, padCalIndex, padCalMask));

    // Init
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));

    // Need 5 cycles of clock toggling
    for (UINT32 j = 0; j < 5; ++j)
    {
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_WCLK));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));
    }

    // program SELECT to 1
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT | TRANSMIT_DATA_VALUE_SELECT));

    // add a clock cycle
    {
        // program WRCK to 1
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_WCLK | TRANSMIT_DATA_VALUE_SELECT));
        // Init
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT | TRANSMIT_DATA_VALUE_SELECT));
    }

    UINT32 value = TRANSMIT_DATA_VALUE_SELECT;
    for (UINT32 i = 0; i < INSTRUCTION_REGISTER_LENGTH; i++)
    {
        value = value | TRANSMIT_DATA_VALUE_WCLK;
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, value));
        UINT32 ithBit = instructionID & (1 << i);
        value = ithBit ? TRANSMIT_DATA_VALUE_INSTR_WSI_1 : TRANSMIT_DATA_VALUE_INSTR_WSI_0;
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, value));

    }

    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, value | TRANSMIT_DATA_VALUE_WCLK));
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_SELECT));
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_SELECT | TRANSMIT_DATA_VALUE_WCLK));
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_SELECT));
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INSTR_UPDATE | TRANSMIT_DATA_VALUE_WCLK));
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INSTR_UPDATE));
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_SELECT | TRANSMIT_DATA_VALUE_WCLK));
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT | TRANSMIT_DATA_VALUE_WCLK));

    if (read)
    {
        rwData->assign((rwDataLen + 31)/32, 0);
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_DATA_CAPTURE));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_DATA_CAPTURE | TRANSMIT_DATA_VALUE_WCLK));

        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT | TRANSMIT_DATA_VALUE_WCLK));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));

        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT | TRANSMIT_DATA_VALUE_WCLK));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));

        int dwordToRead = CHAIN_LENGTH / 32;
        int bitToRead = CHAIN_LENGTH % 32  - 1;
        dataLength = (CHAIN_LENGTH + 31)/32;
        vector<UINT32> data(dataLength, 0);
        CHECK_RC(m_pGpuSub->ReadHost2Jtag(jc3, CHAIN_LENGTH, &data));
        if (data[dwordToRead] >> bitToRead)
        {
            (*rwData)[0] |= 1;
        }
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT | TRANSMIT_DATA_VALUE_WCLK));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT)); // Workaround (bug 1975846)
    }
    else
    {
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_WCLK));
    }

    if (read)
    {
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_DATA_WSI_0));
        for (UINT32 i = 1; i < rwDataLen; i++)
        {
            CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_DATA_WSI_0 | TRANSMIT_DATA_VALUE_WCLK));
            CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_DATA_WSI_0));
            UINT32 dataLength = (CHAIN_LENGTH + 31)/32;
            vector<UINT32> data(dataLength, 0);
            CHECK_RC(m_pGpuSub->ReadHost2Jtag(jc3, CHAIN_LENGTH, &data));
            const UINT32 dWordTOAccess = i/(sizeof((*rwData)[0]) * 8);
            int dwordToRead = CHAIN_LENGTH / 32;
            int bitToRead = CHAIN_LENGTH % 32  - 1;
            if (data[dwordToRead] >> bitToRead)
            {
                (*rwData)[dWordTOAccess] |= 1<<i;
            }
        }
    }
    else
    {
        rwData->resize((rwDataLen + 31)/32);
        for (UINT32 i = 0; i < rwDataLen; i++)
        {
            UINT32 writeValue = 0;
            const UINT32 dWordTOAccess = i/(sizeof((*rwData)[0]) * 8);
            if ((*rwData)[dWordTOAccess] & (1 << i))
                writeValue = TRANSMIT_DATA_VALUE_DATA_WSI_1;
            else
                writeValue = TRANSMIT_DATA_VALUE_DATA_WSI_0;
            CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, writeValue));
            CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, writeValue | TRANSMIT_DATA_VALUE_WCLK));
        }
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT | TRANSMIT_DATA_VALUE_WCLK));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));

        //add a few clock cycles
        for (UINT32 j = 0; j < 5; ++j)
        {
            // program WRCK to 1
            CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_WCLK));
            // Init
            CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));
        }

        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_DATA_UPDATE | TRANSMIT_DATA_VALUE_WCLK));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_DATA_UPDATE));

        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_DATA_UPDATE | TRANSMIT_DATA_VALUE_WCLK));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_DATA_UPDATE));

        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT | TRANSMIT_DATA_VALUE_WCLK));
        CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));
    }

    // set mask to 0
    // Must set back to 0 or the site will be hosed on the next transaction
    CHECK_RC(WriteHost2JtagHelper(PADCAL_MASK_CHAIN_LENGTH, jc2, padCalIndex, 0));

    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC PascalHBM::ReadHostToJtagHBMReg(
    const UINT32 siteID,
    const UINT32 instructionID,
    vector<UINT32> *dataRead,
    const UINT32 dataReadLength) const
{
    return RWHostToJtagHBMReg(true, siteID, instructionID, dataRead, dataReadLength);
}

// -----------------------------------------------------------------------------
/* virtual */ RC PascalHBM::WriteHBMDataReg(
    const UINT32 siteID,
    const UINT32 instruction,
    const vector<UINT32>& writeData,
    const UINT32 writeDataSize,
    const bool useHostToJtag) const
{
    if (!useHostToJtag)
    {
        Printf(Tee::PriError, "HBM writes via IEEE1500 are not supported on Pascal\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return WriteHostToJtagHBMReg(siteID, instruction, writeData, writeDataSize);
}

// -----------------------------------------------------------------------------
/* virtual */ RC PascalHBM::WriteBypassInstruction(
    const UINT32 siteID,
    const UINT32 channel) const
{
    RC rc;

    vector<UINT32> bypassData(HARD_REPAIR_BYPASS_WR_LEN, HARD_REPAIR_BYPASS_DATA);
    CHECK_RC(WriteHostToJtagHBMReg(siteID, HARD_REPAIR_BYPASS_INST_ID,
                 bypassData, HARD_REPAIR_BYPASS_WR_LEN));
    return rc;
}

// -----------------------------------------------------------------------------
//!
//! \brief Applies channel selection fix for hardware bug 1671530 on GP100
//! HostToJtag read path.
//!
//! The WSO mux channel selection is not 1:1 for all channels on the HtoJ read
//! path (ie. not a->a, b->b, ...). This must be corrected before using the HtoJ
//! read path.
//!
RC PascalHBM::ChannelSelectFixOnHtoJReadPath
(
    UINT32 hbmChannel
    ,UINT32* pOutSetChannelSelect
) const
{
    RC rc;
    UINT32 setChannelSelect = hbmChannel;

    switch (hbmChannel)
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
            MASSERT(!"Invalid Channel\n");
            CHECK_RC(RC::ILWALID_ARGUMENT);
    }

    *pOutSetChannelSelect = setChannelSelect;

    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC PascalHBM::ReadHBMLaneRepairReg
(
    const UINT32 siteID
    ,const UINT32 instruction
    ,const UINT32 hbmChannel
    ,vector<UINT32>* readData
    ,const UINT32 readSize
    ,const bool useHostToJtag
) const
{
    RC rc;

    if (!useHostToJtag)
    {
        Printf(Tee::PriError, "HBM reads via IEEE1500 are not supported on Pascal\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Need to set the DATA_VALUE to all 0s (TRANSMIT_DATA_VALUE_INIT) before doing the assert mask
    vector<GpuSubdevice::JtagCluster> jc1;
    jc1.push_back(GpuSubdevice::JtagCluster(JTAG_CLUSTER_CHIPLET_ID2, JTAG_CLUSTER_PADCAL_VALS_INSTR));

    UINT32 padcalCLen = 0;  // Pad cal chain length
    switch (siteID)
    {
        case 0:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_A;
            break;
        case 1:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_B;
            break;
        case 2:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_C;
            break;
        case 3:
            padcalCLen = PADCAL_VAL_CHAIN_LEN_SITE_D;
            break;
        default:
            MASSERT(!"Invalid site number.\n");
            break;
    }

    // Init
    CHECK_RC(WriteHost2JtagHelper(padcalCLen, jc1, 0, TRANSMIT_DATA_VALUE_INIT));

    CHECK_RC(AssertMask(siteID));
    CHECK_RC(SelectChannel(siteID, hbmChannel));

    CHECK_RC(ReadHostToJtagHBMReg(siteID, instruction, readData, readSize));

#ifdef USE_FAKE_HBM_FUSE_DATA
    readData->clear();
    UINT32 numDwords = (readSize + 31) / 32;
    readData->resize(numDwords, ~0U);
#endif
    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC PascalHBM::GetDwordAndByteOverFbpaHbmBoundary
(
    const UINT32 siteID
    ,const UINT32 laneBit
    ,UINT32 *pDword
    ,UINT32 *pByte
) const
{
    RC rc;
    MASSERT(pDword || pByte); // Need at least one out parameter

    CHECK_RC(HBMImpl::GetDwordAndByteOverFbpaHbmBoundary(siteID, laneBit,
                                                         pDword, pByte));
    if (pDword && (siteID == 1 || siteID == 3))
    {
        // Dwords on Sites B and D are swizzled on GP100
        *pDword = 3 - *pDword;
    }

    return rc;
}

// -----------------------------------------------------------------------------
/* virtual */ RC PascalHBM::WriteHostToJtagHBMReg(
    const UINT32 siteID,
    const UINT32 instructionID,
    const vector<UINT32>& writeData,
    const UINT32 dataWriteLength) const
{
    return RWHostToJtagHBMReg(false, siteID, instructionID,
                              const_cast<vector<UINT32> *>(&writeData), dataWriteLength);
}

//------------------------------------------------------------------------------
//!
//! This is IEEE1550 to set the WSO MUX select.
//!
RC PascalHBM::SelectChannel(const UINT32 siteID, const UINT32 hbmChannel) const
{
    RC rc;

    UINT32 channel = 0;
    CHECK_RC(ChannelSelectFixOnHtoJReadPath(hbmChannel, &channel));

    UINT32 masterFbpaNum[4];
    CHECK_RC(m_pGpuSub->GetHBMSiteMasterFbpaNumber(0, &masterFbpaNum[0]));
    CHECK_RC(m_pGpuSub->GetHBMSiteMasterFbpaNumber(1, &masterFbpaNum[1]));
    CHECK_RC(m_pGpuSub->GetHBMSiteMasterFbpaNumber(2, &masterFbpaNum[2]));
    CHECK_RC(m_pGpuSub->GetHBMSiteMasterFbpaNumber(3, &masterFbpaNum[3]));

    //TODO: Use Reghal (below) after LW_PFB_FBPA_FBIO_HBM_TEST_MACRO_DRAM_RESET_ENABLED's value is fixed in the header
    {

        UINT32 val;

        val = m_pGpuSub->Regs().Read32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum[0]);
        m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum[0],
                                  (val|0x10));
        val = m_pGpuSub->Regs().Read32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum[1]);
        m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum[1],
                                  (val|0x10));
        val = m_pGpuSub->Regs().Read32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum[2]);
        m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum[2],
                                  (val|0x10));
        val = m_pGpuSub->Regs().Read32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum[3]);
        m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum[3],
                                  (val|0x10));
    }
    //{
    //    m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_2_FBIO_HBM_TEST_MACRO_DRAM_RESET_ENABLED);
    //    m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_6_FBIO_HBM_TEST_MACRO_DRAM_RESET_ENABLED);
    //    m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_A_FBIO_HBM_TEST_MACRO_DRAM_RESET_ENABLED);
    //    m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_E_FBIO_HBM_TEST_MACRO_DRAM_RESET_ENABLED);
    //}

    const UINT32 instruction = (channel << 8) | HBM_WIR_INSTR_DEVICE_ID;
    m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR_HBM_WIR,
                              masterFbpaNum[0], instruction);
    m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR_HBM_WIR,
                              masterFbpaNum[1], instruction);
    m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR_HBM_WIR,
                              masterFbpaNum[2], instruction);
    m_pGpuSub->Regs().Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR_HBM_WIR,
                              masterFbpaNum[3], instruction);

    return rc;
}

//------------------------------------------------------------------------------
RC PascalHBM::GetHBMDevIdDataHtoJ(
    const UINT32 siteID,
    vector<UINT32> *pRegValues) const
{
    MASSERT(pRegValues);

    RC rc;

    CHECK_RC(ResetSite(siteID));

    UINT32 instructionID = HBM_WIR_CHAN_ALL << 8 | HBM_WIR_INSTR_DEVICE_ID;
    CHECK_RC(ReadHostToJtagHBMReg(siteID, instructionID, pRegValues, HBM_WDR_LENGTH_DEVICE_ID));

    return rc;
}

//------------------------------------------------------------------------------
RC PascalHBM::GetHBMDevIdDataFbPriv(
    const UINT32 siteID,
    vector<UINT32> *pRegValues) const
{
    Printf(Tee::PriError, "GetHBMDevIdDataFbPriv is not supported on Pascal!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//! \brief Create a null HBM implementer
//!
//! \return nullptr
HBMImpl *CreateNullHBM(GpuSubdevice *pGpu)
{
    return nullptr;
}

CREATE_HBM_FUNCTION(PascalHBM);
