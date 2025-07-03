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

#include "ist_smbus.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "device/smb/smbdev.h"
#include "device/smb/smbmgr.h"
#include "device/smb/smbport.h"
#include "gpu/include/gpudev.h"

//----------------------------------------------------------------------------
IstSmbus::IstSmbus()
{
}

//! \brief Find and initialize MLW device
//! This function will initialize the Smbus Post Box Interface(SmbPBI) and ping
//! Micro Controller Unit(MLW)
//!
//! \return OK if successful
//!
//----------------------------------------------------------------------------
RC IstSmbus::InitializeIstSmbusDevice(const GpuSubdevice *pSubdev)
{
    RC rc;
    m_SmbAddress = pSubdev->GetSmbusAddress();

    m_pSmbMgr = SmbMgr::Mgr();
    CHECK_RC(m_pSmbMgr->Find());
    CHECK_RC(m_pSmbMgr->InitializeAll());
    UINT32 numDev = 0;
    CHECK_RC(m_pSmbMgr->GetNumDevices(&numDev));

    for (UINT32 i = 0; i < numDev; ++i)
    {
        SmbDevice *smbDevice = nullptr;
        CHECK_RC(m_pSmbMgr->GetByIndex(i, reinterpret_cast<McpDev**>(&smbDevice)));
        UINT32 numSubDev = 0;
        CHECK_RC(smbDevice->GetNumSubDev(&numSubDev));
        for (UINT32 j = 0; j < numSubDev; ++j)
        {
            CHECK_RC(smbDevice->GetSubDev(j, reinterpret_cast<SmbPort**>(&m_SmbPort)));
            UINT08 status = 0;
            rc = m_SmbPort->RdByteCmd(m_SmbAddress, COMMAND_REGISTER, &status);
            if (rc == RC::OK)
            {
                return rc;
            }
        }
    }
    return rc;
}

//----------------------------------------------------------------------------
RC IstSmbus::UninitializeIstSmbusDevice()
{
    StickyRC rc;

    if (m_pSmbMgr)
    {
        rc = m_pSmbMgr->Uninitialize();
        rc = m_pSmbMgr->Forget();
        m_pSmbMgr = nullptr;
    }
    return rc;
}

//----------------------------------------------------------------------------
RC IstSmbus::ReadTemp(INT32 *temp, UINT32 numRetry)
{
    RC rc;
    UINT08 status = 0;
    UINT32 retry = 0;
    UINT32 data = 0;
    while (status != STATUS_SUCCESS && retry < numRetry)
    {
        // cmd = {opcode, arg1, arg2, 0x80}
        // opcode: 02h(get temp, single precision), Arg1: 00h(sensor on Primary GPU)
        // Arg2: unused, 0x80: talking to MLW
        std::vector<UINT08> cmd = {OPCODE_GET_TEMPERATURE_SINGLE_PRECISION, 0x0, NO_OP, 0x80};
        CHECK_RC(WriteCommand(&cmd));
        CHECK_RC(ReadStatus(&status));
        // Data out: Fixed-point signed integer with 8 fractional bits (7:0 all zero)
        // representing the source temperature in Celsius. The tempeature is stored in
        // 31:8 and must be right shifted by 8 bits
        CHECK_RC(ReadData(&data));
        *temp = data >> 8;
        Printf(Tee::PriDebug, "%d iteration, status = 0x%x, data = 0x%x, temp = 0x%x\n", retry, status, data, *temp);
        Tasker::Sleep(1);
        retry ++;
    }

    if (status != STATUS_SUCCESS)
    {
        Printf(Tee::PriWarn, "Read Temp from SMBus failed after %d retries\n", numRetry);
        return RC::HW_STATUS_ERROR;
    }
    return rc;
}

//----------------------------------------------------------------------------
RC IstSmbus::WriteCommand(std::vector<UINT08> *command)
{
    RC rc;
    UINT32 retry = 0;

    do
    {
        rc.Clear();
        rc = m_SmbPort->WrBlkCmd(m_SmbAddress, COMMAND_REGISTER, command);
        if (rc != RC::OK)
        {
            Printf(Tee::PriDebug, "Failed write command, retry = %u\n", retry);
            retry++;
            Tasker::Sleep(1);
        }
    } while (rc != RC::OK && retry < 10);

    return rc;
}

//----------------------------------------------------------------------------
RC IstSmbus::ReadStatus(UINT08 *status)
{
    RC rc;
    UINT32 retry = 0;
    *status = 0;
    std::vector<UINT08> data(4, 0);

    do
    {
        rc.Clear();
        rc = m_SmbPort->RdBlkCmd(m_SmbAddress, COMMAND_REGISTER, &data);
        if (rc != RC::OK)
        {
            Printf(Tee::PriDebug, "Failed read command, retry = %u\n", retry);
            retry++;
            Tasker::Sleep(1);
        }
    } while (rc != RC::OK && retry < 10);

    if (rc == RC::OK)
    {
        // As per SmbPBI reference bit 28:24 refers to status in
        // COMMAND_REGISTER which fall in data[3] of our vector
        *status = data[3] & STATUS_MASK;
    }

    return rc;
}

//----------------------------------------------------------------------------
RC IstSmbus::ReadData(UINT32 *data)
{
    RC rc;
    UINT32 retry = 0;
    std::vector<UINT08> cmd(4, 0);

    do
    {
        rc.Clear();
        rc = m_SmbPort->RdBlkCmd(m_SmbAddress, DATA_REGISTER, &cmd);
        if (rc != RC::OK)
        {
            Printf(Tee::PriDebug, "Failed read data command, retry = %u\n", retry);
            retry++;
            Tasker::Sleep(1);
        }
    } while (rc != RC::OK && retry < 10);

    if (rc == RC::OK)
    {
        *data |= cmd[0];
        *data |= (cmd[1] << 8);
        *data |= (cmd[2] << 16);
        *data |= (cmd[3] << 24);
    }

    return rc;
}


