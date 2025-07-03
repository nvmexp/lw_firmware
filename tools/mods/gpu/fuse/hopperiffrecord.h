/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <vector>

#include "gpu/fuse/iffrecord.h"

class HopperIffRecord : public IffRecord
{
public:

    // Record format details from -
    // https://p4hw-swarm.lwpu.com/files/hw/doc/gpu/hopper_info/gen_manuals/gh100/dev_pmc.ref#1729
    enum class HopperIffType : UINT32
    {
        NULL_RECORD                         = 0x0,
        CONTROL_SET_IFF_EXELWTION_FILTER    = 0x1,
        CONTROL_POLL_UNTIL_NO_PRI_ERROR     = 0x2,
        POLL_RETRY_UNTIL_EQUAL              = 0x4,
        POLL_RETRY_UNTIL_LESS_THAN          = 0x5,
        POLL_RETRY_UNTIL_GREATER_THAN       = 0x6,
        POLL_RETRY_UNTIL_NOT_EQUAL          = 0x7,
        POLL_ABORT_UNTIL_EQUAL              = 0x8,
        POLL_ABORT_UNTIL_LESS_THAN          = 0x9,
        POLL_ABORT_UNTIL_GREATER_THAN       = 0xA,
        POLL_ABORT_UNTIL_NOT_EQUAL          = 0xB,
        WRITE_MODIFY_DW                     = 0xC,
        WRITE_WRITE_DW                      = 0xD,
        WRITE_SET_FIELD                     = 0xE,
        ILWALIDATED                         = 0xF,
        UNKNOWN
    };

    HopperIffRecord() {}
    explicit HopperIffRecord(UINT32 firstRecord);
    virtual ~HopperIffRecord() {}

    virtual bool operator==(const HopperIffRecord& other) const
    {
        return (m_IffType == other.m_IffType) &&
        (IffRecord::GetDataList() == other.GetDataList());
    }

    virtual bool operator!=(const HopperIffRecord& other) const
        { return !(*this == other); }

    HopperIffType GetHopperIffType() const { return m_IffType; }

    UINT32 GetOpCode(const UINT32 lwrRecord) const override { return lwrRecord & s_OpCodeMask; }
    UINT32 GetIffNumRows() const override;
    UINT32 GetIffNumRows(const UINT32 opCode) const override;

    bool IsIlwalidatedRecord() const override { return  m_IffType == HopperIffType::ILWALIDATED; }
    bool IsNullRecord() const override { return m_IffType == HopperIffType::NULL_RECORD; }
    bool IsUnknownRecord() const override { return m_IffType == HopperIffType::UNKNOWN; }

private:
    static constexpr UINT08 s_OpCodeMask = 0xF;
    static constexpr UINT08 s_TypeMask = 0x20;

    HopperIffType m_IffType = HopperIffType::UNKNOWN;
};