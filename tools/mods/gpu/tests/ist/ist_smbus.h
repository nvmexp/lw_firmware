/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file ist_subus.h
 * @brief Implementation of SMBus realted function used in IST
 */

#pragma once

#include "core/include/rc.h"

class GpuDevice;
class GpuSubdevice;
class SmbManager;
class SmbPort;

class IstSmbus
{

public:
    IstSmbus();
    RC ReadTemp(INT32 *temp, UINT32 numRetry);
    RC UninitializeIstSmbusDevice();
    RC InitializeIstSmbusDevice(const GpuSubdevice *pSubdev);
    RC WriteCommand(std::vector<UINT08> *command);
    RC ReadStatus(UINT08 *status);
    RC ReadData(UINT32 *data);

private:
    SmbManager  *m_pSmbMgr = nullptr;
    SmbPort *m_SmbPort = nullptr;
    UINT08 m_SmbAddress = 0;

    static constexpr UINT08 STATUS_MASK = 0x1F; // with respect to bits [28:24]
    static constexpr UINT08 STATUS_SUCCESS = 0x1F;
    static constexpr UINT08 NO_OP = 0;

    // Reference: SMBPBI Notebook Software Design Guide DG-06034-002_v03.11.pdf
    static constexpr UINT32 COMMAND_REGISTER = 0x5C;
    static constexpr UINT32 TEMP_REGISTER = 0x00;
    static constexpr UINT08 DATA_REGISTER = 0x5D;
    static constexpr UINT32 OPCODE_GET_TEMPERATURE_SINGLE_PRECISION = 0x02;
};
