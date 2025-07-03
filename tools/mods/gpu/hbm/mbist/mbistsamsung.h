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

#ifndef INCLUDE_SMSG_MBIST_H
#define INCLUDE_SMSG_MBIST_H

#include <core/include/memerror.h>
#include "mbist.h"

#include <bitset>
#include <vector>

class SamsungMBist : public MBistImpl
{
public:
    SamsungMBist(HBMDevice* pHbmDev, bool useHostToJtag, UINT32 stackHeight) :
        MBistImpl(pHbmDev, useHostToJtag, stackHeight)
    {
        m_SoftRepairComplete = false;
        memset(&m_FirstSoftRepairInfo, 0, sizeof(FirstSoftRepairInfo));
    }

    RC StartMBist(const UINT32 siteID, const UINT32 mbistType) override;
    RC CheckCompletionStatus
    (
        const UINT32 siteID
        ,const UINT32 mbistType
    ) override;

    RC CheckMbistCompletionFbToPriv
    (
        const UINT32 siteID
        ,const UINT32 mbistType
        ,bool* repairable
        ,UINT32* numBadRows
    );

    // Using host to jtag path
    RC StartMbistHostToJtagPath
    (
        const UINT32 siteID
        ,const UINT32 mbistType
    ) const;
    RC CheckMbistCompletionHostToJtagPath
    (
        const UINT32 siteID
        ,const UINT32 mbistType
        ,bool* repairable
        ,UINT32* numBadRows
    ) const;

    RC SoftRowRepair
    (
        const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 row
        ,const UINT32 bank
        ,const bool firstRow
        ,const bool lastRow
    ) override;

    RC HardRowRepair
    (
        const bool isPseudoRepair
        ,const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 row
        ,const UINT32 bank
    ) const override;

    RC DidJtagResetOclwrAfterSWRepair(bool* pReset) const override;

    RC GetNumSpareRowsAvailable
    (
        const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 bank
        ,UINT32* pNumAvailable
        ,bool doPrintResults = true
    ) const override;

private:
    enum
    {
        NUM_MBIST_DATA_READS               = 12,
        ENABLE_MBIST_INST                  = 0x1F06,
        BEGIN_MBIST_INST                   = 0x81100000,
        MAX_DATA_BITS                      = 372,

        INFO_BIT_MBIST_COMPLETE            = 0,
        INFO_BIT_RDQS_STATUS               = 6,
        INFO_BIT_FAILING_ROW_START         = 1,
        INFO_BIT_FAILING_ROW_END           = 5,

        // scan pattern MBIST
        MBIST_SCAN_PAT_INST_1              = 0xFA7,
        MBIST_SCAN_PAT_WRDATA_1            = 0x1,
        MBIST_SCAN_PAT_WRDATA_LEN_1        = 10,

        MBIST_SCAN_PAT_INST_2              = 0xF06,
        MBIST_SCAN_PAT_WRDATA_2_HEIGHT_4   = 0x800,
        MBIST_SCAN_PAT_WRDATA_2_HEIGHT_8   = 0x804,
        MBIST_SCAN_PAT_WRDATA_LEN_2        = 12,

        MBIST_SCAN_PAT_INST_3              = 0xF06,
        MBIST_SCAN_PAT_READ_LEN_3          = 7,

        // March pattern MBIST
        MBIST_MARCH_PAT_INST_1             = MBIST_SCAN_PAT_INST_1,
        MBIST_MARCH_PAT_WRDATA_1           = MBIST_SCAN_PAT_WRDATA_1,
        MBIST_MARCH_PAT_WRDATA_LEN_1       = MBIST_SCAN_PAT_WRDATA_LEN_1,

        MBIST_MARCH_PAT_INST_2             = MBIST_SCAN_PAT_INST_2,
        MBIST_MARCH_PAT_WRDATA_2_HEIGHT_4  = MBIST_SCAN_PAT_WRDATA_2_HEIGHT_4,
        MBIST_MARCH_PAT_WRDATA_2_HEIGHT_8  = 0xA04,
        MBIST_MARCH_PAT_WRDATA_LEN_2       = MBIST_SCAN_PAT_WRDATA_LEN_2,

        MBIST_MARCH_PAT_INST_3             = MBIST_SCAN_PAT_INST_3,
        MBIST_MARCH_PAT_READ_LEN_3         = MBIST_SCAN_PAT_READ_LEN_3,

        //Soft repair
        TMRS_WRITE_WIR_BYTE_VAL            = 0xB0,
        MRDS_WRITE_WIR_BYTE_VAL            = 0x10,
        TMRS_WRITE_DATA_1                  = 0x2002001,
        TMRS_WRITE_DATA_2                  = 0x12,
        TMRS_WRITE_DATA_3                  = 0x8080010,
        TMRS_WRITE_DATA_4                  = 0x10,
        TMRS_WRITE_DATA_5                  = 0x8020001,
        TMRS_WRITE_DATA_6                  = 0x10,

        SOFT_REPAIR_REGWR_SLEEP_TIME_MS    = 1,
        SOFT_REPAIR_SLEEP_TIME_MS          = 500,
        TMRS_WR_DATA_LEN                   = 37,
        MRDS_WR_DATA_LEN                   = 128,
        SOFT_REPAIR_INST_ID                = 7,
        REPAIR_WR_DATA_LEN                 = 21,
        HARD_REPAIR_INST_ID                = 8,

        HARD_REPAIR_BYPASS_INST_ID         = 0,
        HARD_REPAIR_BYPASS_DATA            = 0,
        HARD_REPAIR_BYPASS_WR_LEN          = 1,

        ENABLE_FUSE_SCAN_INST_ID           = 0xC0,
        ENABLE_FUSE_SCAN_WR_LENGTH         = 26,
        ENABLE_FUSE_SCAN_SLEEP_TIME_MS     = 100,

    };

    RC GetNumSpareRowsAvailableHtoJ
    (
        const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 bank
        ,UINT32 *pNumAvailable
        ,bool doPrintResults = true
    ) const;
    RC GetNumSpareRowsAvailableFbPriv
    (
        const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 bank
        ,UINT32 *pNumAvailable
        ,bool doPrintResults = true
    ) const;

    RC HardRowRepairHtoJ
    (
        const bool isPseudoRepair
        ,const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 row
        ,const UINT32 bank
    ) const;
    RC HardRowRepairFbPriv
    (
        const bool isPseudoRepair
        ,const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 row
        ,const UINT32 bank
    ) const;

    bool m_SoftRepairComplete;              // denotes if SW repair is complete

    struct FirstSoftRepairInfo              // used to check for JTAG reset
    {
        UINT32 siteID;
        UINT32 channelID;
    } m_FirstSoftRepairInfo;

};

#endif
