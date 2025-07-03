
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_PASCALHBM_H
#define INCLUDED_PASCALHBM_H

#include "gpu/include/hbmimpl.h"

//--------------------------------------------------------------------
//! \brief HBM related functionality for Pascal
//!
class PascalHBM : public HBMImpl
{
public:
    PascalHBM(GpuSubdevice *pSubdev);
    virtual ~PascalHBM() {};

    // Asserting/DeAsserting FB stop
    static bool PollForFBStopAck(void *pVoidPollArgs);

    static bool PollForFBStartAck(void *pVoidPollArgs);

    virtual RC AssertFBStop();

    virtual RC DeAssertFBStop();

    //Read/Write registers via host to jtag
    virtual RC RWHostToJtagHBMReg(const bool read,  //Read or Write
        const UINT32 siteID,
        const UINT32 instructionID,
        vector<UINT32> *data,
        const UINT32 dataLength) const;

    virtual RC ReadHostToJtagHBMReg(const UINT32 siteID,
        const UINT32 instructionID,
        vector<UINT32> *dataRead,
        const UINT32 dataReadLength) const;

    virtual RC WriteHost2JtagHelper( const UINT32 chainLength,
        vector<GpuSubdevice::JtagCluster> jc,
        const UINT32 index,
        const UINT32 value) const;

    virtual RC WriteHostToJtagHBMReg(const UINT32 siteID,
        const UINT32 instructionID,
        const vector<UINT32>& writeData,
        const UINT32 dataWriteLength) const;

    virtual RC WriteHBMDataReg(const UINT32 siteID, const UINT32 instruction,
        const vector<UINT32>& writeData, const UINT32 writeDataSize,
        const bool useHostToJtag) const;
    virtual RC ReadHBMLaneRepairReg
    (
        const UINT32 siteID
        ,const UINT32 instruction
        ,const UINT32 hbmChannel
        ,vector<UINT32>* readData
        ,const UINT32 readSize
        ,const bool useHostToJtag
    ) const;

    virtual RC GetDwordAndByteOverFbpaHbmBoundary
    (
        const UINT32 siteID
        ,const UINT32 laneBit
        ,UINT32 *pDword
        ,UINT32 *pByte
    ) const;
    
    virtual RC WriteBypassInstruction(const UINT32 siteID, const UINT32 channel) const;

    virtual RC SelectChannel(const UINT32 siteID, const UINT32 hbmChannel) const;

    virtual RC AssertMask(const UINT32 siteID) const;

    virtual RC ResetSite(const UINT32 siteID) const;

    virtual RC ToggleClock(const UINT32 siteID, const UINT32 numTimes) const;

private:
    enum
    {
        //host to jtag constants
        PADCAL_VAL_CHAIN_LEN_SITE_A      = 581,
        PADCAL_VAL_CHAIN_LEN_SITE_B      = 795,
        PADCAL_VAL_CHAIN_LEN_SITE_C      = 350,
        PADCAL_VAL_CHAIN_LEN_SITE_D      = 136,
        IOBIST_CONFIG0_CHAIN_LENGTH      = 0xF08,
        PADCAL_MASK_CHAIN_LENGTH         = 0xA0,
        TRANSMIT_DATA_VALUE              = 0x0,
        TRANSMIT_DATA_VALUE_INIT         = 0x3,
        TRANSMIT_DATA_VALUE_SELECT       = 0x803,
        TRANSMIT_DATA_VALUE_DATA_CAPTURE = 0x103,
        TRANSMIT_DATA_VALUE_INSTR_UPDATE = 0xA03,
        TRANSMIT_DATA_VALUE_DATA_UPDATE  = 0x203,
        TRANSMIT_DATA_VALUE_INSTR_WSI_1  = 0x2813,
        TRANSMIT_DATA_VALUE_INSTR_WSI_0  = 0x813,
        TRANSMIT_DATA_VALUE_DATA_WSI_1   = 0x2013,
        TRANSMIT_DATA_VALUE_DATA_WSI_0   = 0x13,
        TRANSMIT_DATA_VALUE_WCLK         = 0x7,
        INSTRUCTION_REGISTER_LENGTH      = 12,
        CHAIN_LENGTH                     = 45,
        JTAG_CLUSTER_CHIPLET_ID2         = 0x2,
        JTAG_CLUSTER_CHIPLET_ID3         = 0x3,
        JTAG_CLUSTER_CHIPLET_ID4         = 0x4,
        JTAG_CLUSTER_PADCAL_VALS_INSTR   = 0x47,
        JTAG_CLUSTER_PADCAL_MASK_INSTR   = 0x48,
        JTAG_CLUSTER_INSTRUCTION_ID      = 0x59,
        JTAG_CLUSTER_DEVID_INSTR_1       = 0xEB,
        JTAG_CLUSTER_DEVID_INSTR_2       = 0xF0,
        SITE_A_BYPASS                    = 1<<15,
        SITE_B_BYPASS                    = 1<<26,
        SITE_C_BYPASS                    = 1<<21,
        SITE_D_BYPASS                    = 1<<10,
        PADCAL_MASK_SITE_A               = 1<<1,
        PADCAL_MASK_SITE_B               = 1<<6,
        PADCAL_MASK_SITE_C               = 1<<13,
        PADCAL_MASK_SITE_D               = 1<<8,
        SITE_A_INDEX                     = 53,
        SITE_B_INDEX                     = 23,
        SITE_C_INDEX                     = 83,
        SITE_D_INDEX                     = 113,
        HARD_REPAIR_BYPASS_INST_ID       = 0,
        HARD_REPAIR_BYPASS_DATA          = 0,
        HARD_REPAIR_BYPASS_WR_LEN        = 1,
    };

    // Get HBM site info
    virtual RC GetHBMDevIdDataHtoJ(const UINT32 siteID, vector<UINT32> *pRegValues) const;
    virtual RC GetHBMDevIdDataFbPriv(const UINT32 siteID, vector<UINT32> *pRegValues) const;

    RC ChannelSelectFixOnHtoJReadPath(UINT32 hbmChannel, UINT32* pOutHbmChannel) const;
};

#endif
