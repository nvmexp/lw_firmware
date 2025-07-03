/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \float rmt_spi.cpp
//! \brief Test for validate SPI interface and working of SPI devices.
//!
//! This test is targeted to check SPI HW controller and SPI devices
//! functionality. Right now as we have INFOROM device only, so this
//! test checks devices here only.
//!

//

#include "gpu/tests/rmtest.h"
#include <stdio.h>

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/platform.h"
#include "lwmisc.h" // for REG_RD_DRF()
#include "core/include/tasker.h"
#include "gpu/include/gpusbdev.h"
#include "lwRmApi.h"

#include <string.h>
#include "rmt_spi.h"
#include "core/include/memcheck.h"

#include "ctrl/ctrl2080.h"

class SpiTest : public RmTest
{
public:
    SpiTest();
    virtual ~SpiTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
private:
    GpuSubdevice *m_pSubdev;
    LW2080_CTRL_SPI_DEVICES_GET_INFO_PARAMS  m_spiDeviceInfo;
    RC    GetSpiDevicesInfo();
    RC    AllSpiDevicesWriteBufferTest();
    RC    AllSpiDevicesReadBufferTest();
    RC    SpiDeviceWriteBuffer(LW2080_CTRL_SPI_DEVICE_WRITE_BUFFER_PARAMS params);
    RC    SpiDeviceReadBuffer(LW2080_CTRL_SPI_DEVICE_READ_BUFFER_PARAMS params);
    RC    SpiDeviceEraseBuffer(LW2080_CTRL_SPI_DEVICE_ERASE_BUFFER_PARAMS params);
    RC    SpiDeviceRomReadBuffer(LwU8 devIdx, LwU8 *pBuf, LwU32 start, LwU32 size);
    RC    SpiDeviceRomWriteBuffer(LwU8 devIdx, LwU8 *pBuf, LwU32 start, LwU32 size);
    RC    SpiDeviceRomWriteTest(LwU8 devIdx, LwU8 *pBuf, LwU32 start, LwU32 size);
};

//! \brief SpiTest constructor
//!
//! \sa Setup
//-----------------------------------------------------------------------------
SpiTest::SpiTest()
{
    SetName("SpiTest");
    m_pSubdev = GetBoundGpuSubdevice();
    memset(&m_spiDeviceInfo, 0, sizeof(m_spiDeviceInfo));
}

//! \brief SpiTest destructor
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
SpiTest::~SpiTest()
{

}

//! \brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//-----------------------------------------------------------------------------
string SpiTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup(): Generally used for any test level allocation
//!
//! Here link to Reg. Params structure is done depending on the GpuId,
//! Temp offset register is stored for restoring the register in CleanUp and
//! device, client allocation are done.
//!
//! \return OK, If all allocation are success.
//! \sa Run()
//-----------------------------------------------------------------------------
RC SpiTest::Setup()
{
    RC        rc;
    m_pSubdev = GetBoundGpuSubdevice();

    return rc;
}

//-----------------------------------------------------------------------------
//
//! \brief Run the test!
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC SpiTest::Run()
{
    RC rc;

    Printf(Tee::PriHigh, "%s: SPI test start: \n", __FUNCTION__);

    // 1. Get SPI devices first
    rc = GetSpiDevicesInfo();
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s: SPI test: GetSpiDevicesInfo fail\n", __FUNCTION__);
        goto Run_done;
    }
    // 2. Read test for all devices
    rc = AllSpiDevicesReadBufferTest();
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s: SPI test: Read SPI devices test fail\n", __FUNCTION__);
        goto Run_done;
    }
    // 3. Write test for all devices
    rc = AllSpiDevicesWriteBufferTest();
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s: SPI test: Write SPI devices test fail\n", __FUNCTION__);
    }

Run_done:
    return rc;
}

//! \brief Cleanup()
//!
//! \sa Run()
//-----------------------------------------------------------------------------
RC SpiTest::Cleanup()
{
    return OK;
}

RC SpiTest::GetSpiDevicesInfo()
{
    RC      rc = OK;
    LwU32   i  = 0;
    LwRmPtr pLwRm;

    rc = pLwRm->ControlBySubdevice(m_pSubdev,
                    LW2080_CTRL_CMD_SPI_DEVICES_GET_INFO,
                    (void*)&m_spiDeviceInfo,
                    sizeof(m_spiDeviceInfo));

    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s: failed to get SPI devices info\n", __FUNCTION__);
        goto GetSpiDevicesInfo_done;
    }

    // Print SPI devices info
    Printf(Tee::PriHigh, "%s: \t List of SPI devices configured: \n", __FUNCTION__);

    FOR_EACH_INDEX_IN_MASK(32, i, m_spiDeviceInfo.devMask)
    {
        switch (m_spiDeviceInfo.devices[i].type)
        {
            case LW2080_CTRL_SPI_DEVICE_TYPE_ROM:
            {
                Printf(Tee::PriHigh, "%s: \t\t SPI ROM is configured \t\t\t bInitialized = %s: \n", __FUNCTION__,
                    (m_spiDeviceInfo.devices[i].data.rom.bInitialized ? "True" : "False"));
                break;
            }
            default:
            {
                Printf(Tee::PriHigh, "%s: \t\t Undefined SPI device at index %d: \n", __FUNCTION__, i);
                rc = RC::LWRM_NOT_SUPPORTED;
                break;
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    Printf(Tee::PriHigh, "%s: \t Get SPI devices info test PASSED!: \n", __FUNCTION__);
GetSpiDevicesInfo_done:
    return rc;
}

RC SpiTest::AllSpiDevicesWriteBufferTest()
{
    RC      rc = OK;
    LwU8    i  = 0;
    LwU8    backup[LW_SPI_ROM_SECTOR_SIZE_IN_BYTES] = { 0 };
    LwU8    writeBuffer[] = LW_SPI_WRITE_BUFFER;
    LwU8    writeByte = LW_SPI_WRITE_BYTE;

    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s: failed to WRITE SPI devices\n", __FUNCTION__);
        goto AllSpiDevicesWriteBufferTest_done;
    }

    // Print SPI devices info
    Printf(Tee::PriHigh, "%s: \t Writing to all SPI devices: \n", __FUNCTION__);

    FOR_EACH_INDEX_IN_MASK(32, i, m_spiDeviceInfo.devMask)
    {
        switch (m_spiDeviceInfo.devices[i].type)
        {
            case LW2080_CTRL_SPI_DEVICE_TYPE_ROM:
            {
                if (m_spiDeviceInfo.devices[i].data.rom.bInitialized)
                {
                    // First take the backup read of first sector which is going to be modified.
                    rc = SpiDeviceRomReadBuffer(i, backup, 0x0, LW_SPI_ROM_SECTOR_SIZE_IN_BYTES);
                    if (OK != rc)
                    {
                        goto AllSpiDevicesWriteBufferTest_done;
                    }

                    //
                    // Write-in-place Test
                    // Pascal onwards every board has an inforom image which starts with
                    // signature of file system type i.e. 0x4a464653 ("JFFS" in ASCII).
                    // Write-in-place test writes a known byte at the start address which
                    // toggles one bit from 1 to 0 which doesn't require an erase.
                    //
                    rc = SpiDeviceRomWriteTest(i, &writeByte, 0x0, sizeof(LwU8));
                    if (OK != rc)
                    {
                        goto AllSpiDevicesWriteBufferTest_done;
                    }

                    //
                    // Write-with-erase Test
                    // Write a known buffer which will require an erase and read back to
                    // verify whether write was successfull or not.
                    //
                    rc = SpiDeviceRomWriteTest(i, writeBuffer, 0x0, LW_SPI_WRITE_BUFFER_SIZE_IN_BYTES);
                    if (OK != rc)
                    {
                        goto AllSpiDevicesWriteBufferTest_done;
                    }

                    // Restore the original image.
                    rc = SpiDeviceRomWriteBuffer(i, backup, 0x0, LW_SPI_ROM_SECTOR_SIZE_IN_BYTES);
                    if (OK != rc)
                    {
                        goto AllSpiDevicesWriteBufferTest_done;
                    }
                }
                break;
            }
            default:
            {
                Printf(Tee::PriHigh, "%s: \t\t Undefined SPI device at index %d: \n", __FUNCTION__, i);
                rc = RC::LWRM_NOT_SUPPORTED;
                break;
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    Printf(Tee::PriHigh, "%s: \t Writing to all SPI devices PASSED!: \n", __FUNCTION__);

AllSpiDevicesWriteBufferTest_done:
    return rc;
}

RC SpiTest::AllSpiDevicesReadBufferTest()
{
    RC      rc = OK;
    LwU8    i  = 0;
    LwU8 buffer[LW_SPI_READ_BUFFER_SIZE_IN_BYTES] = { 0 };

    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s: failed to READ SPI devices\n", __FUNCTION__);
        goto AllSpiDevicesReadBufferTest_done;
    }

    // Print SPI devices info
    Printf(Tee::PriHigh, "%s: \t Reading from all SPI devices: \n", __FUNCTION__);

    FOR_EACH_INDEX_IN_MASK(32, i, m_spiDeviceInfo.devMask)
    {
        switch (m_spiDeviceInfo.devices[i].type)
        {
            case LW2080_CTRL_SPI_DEVICE_TYPE_ROM:
            {
                if (m_spiDeviceInfo.devices[i].data.rom.bInitialized)
                {
                    rc = SpiDeviceRomReadBuffer(i, buffer, 0x0, LW_SPI_READ_BUFFER_SIZE_IN_BYTES);
                    if (OK != rc)
                    {
                        goto AllSpiDevicesReadBufferTest_done;
                    }
                }

                break;
            }
            default:
            {
                Printf(Tee::PriHigh, "%s: \t\t Undefined SPI device at index %d: \n", __FUNCTION__, i);
                rc = RC::LWRM_NOT_SUPPORTED;
                break;
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    Printf(Tee::PriHigh, "%s: \t Reading from all SPI devices PASSED!: \n", __FUNCTION__);

AllSpiDevicesReadBufferTest_done:
    return rc;
}

RC SpiTest::SpiDeviceEraseBuffer(LW2080_CTRL_SPI_DEVICE_ERASE_BUFFER_PARAMS params)
{
    RC rc = OK;
    LwRmPtr pLwRm;

    rc = pLwRm->ControlBySubdevice(m_pSubdev,
                LW2080_CTRL_CMD_SPI_DEVICE_ERASE_BUFFER,
                (void*)&params,
                sizeof(params));

    if (OK != rc || params.out.rom.sizeInBytes == 0)
    {
        Printf(Tee::PriHigh, "%s: \t\t failed to Erase SPI ROM device\n", __FUNCTION__);
        rc = RC::LWRM_ERROR;
    }
    return rc;
}

RC SpiTest::SpiDeviceWriteBuffer(LW2080_CTRL_SPI_DEVICE_WRITE_BUFFER_PARAMS params)
{
    RC rc = OK;
    LwRmPtr pLwRm;

    rc = pLwRm->ControlBySubdevice(m_pSubdev,
                LW2080_CTRL_CMD_SPI_DEVICE_WRITE_BUFFER,
                (void*)&params,
                sizeof(params));

    if (OK != rc || params.out.rom.sizeInBytes == 0)
    {
        Printf(Tee::PriHigh, "%s: \t\t failed to WRITE SPI ROM device\n", __FUNCTION__);
        rc = RC::LWRM_ERROR;
    }
    return rc;
}

RC SpiTest::SpiDeviceReadBuffer(LW2080_CTRL_SPI_DEVICE_READ_BUFFER_PARAMS params)
{
    RC rc = OK;
    LwRmPtr pLwRm;

    rc = pLwRm->ControlBySubdevice(m_pSubdev,
                LW2080_CTRL_CMD_SPI_DEVICE_READ_BUFFER,
                (void*)&params,
                sizeof(params));

    if (OK != rc || params.out.rom.sizeInBytes == 0)
    {
        Printf(Tee::PriHigh, "%s: \t\t failed to READ SPI device\n", __FUNCTION__);
        rc = RC::LWRM_ERROR;
    }
    return rc;
}

RC SpiTest::SpiDeviceRomReadBuffer(LwU8 devIdx, LwU8 *pBuf, LwU32 start, LwU32 size)
{
    RC rc = OK;
    LW2080_CTRL_SPI_DEVICE_READ_BUFFER_PARAMS spiDevRead;

    // Fill in Read params
    spiDevRead.flags = 0;
    spiDevRead.version = 0;
    spiDevRead.type = LW2080_CTRL_SPI_DEVICE_TYPE_ROM;
    spiDevRead.devIndex = devIdx;
    spiDevRead.in.rom.startAddress = start;
    spiDevRead.in.rom.sizeInBytes = size;
    spiDevRead.out.rom.pBuffer = pBuf;
    spiDevRead.out.rom.sizeInBytes = 0;

    rc = SpiDeviceReadBuffer(spiDevRead);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s: \t\t Failed to READ SPI ROM device\n", __FUNCTION__);
    }

    return rc;
}

RC SpiTest::SpiDeviceRomWriteBuffer(LwU8 devIdx, LwU8 *pBuf, LwU32 start, LwU32 size)
{
    RC    rc = OK;
    LwU32 bytesToWrite = 0;
    LwU32 sectorOffset = 0;
    LwU8 *pSectors = NULL;
    LwU8 *pLwrSector = NULL;
    LwU32 limit;
    LwU32 i;
    LwU32 j;
    LwU32 k;
    LwU8  newByte;
    LwU8  oldByte;
    LW2080_CTRL_SPI_DEVICE_WRITE_BUFFER_PARAMS spiDevWrite;
    LW2080_CTRL_SPI_DEVICE_ERASE_BUFFER_PARAMS spiDevErase;

    //
    // Determine the number of sectors we need to cover, and read in all of
    // of those sectors initially instead of one by one later.
    //
    sectorOffset = start & ~(LW_SPI_ROM_SECTOR_SIZE_IN_BYTES - 1);
    limit = start + size;
    if ((limit & (LW_SPI_ROM_SECTOR_SIZE_IN_BYTES - 1)) != 0)
    {
        limit = (limit & ~(LW_SPI_ROM_SECTOR_SIZE_IN_BYTES - 1)) + LW_SPI_ROM_SECTOR_SIZE_IN_BYTES;
    }

    pSectors = new LwU8[limit - sectorOffset];
    if (pSectors == NULL)
    {
        Printf(Tee::PriHigh, "%s: \t\t Failed to allocate memory for the sectors\n", __FUNCTION__);
        rc = RC::LWRM_ERROR;
        goto SpiDevicesRomWriteBuffer_done;
    }

    rc = SpiDeviceRomReadBuffer(devIdx, pSectors, sectorOffset, limit - sectorOffset);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s: \t\t Failed to read buffer\n", __FUNCTION__);
        goto SpiDevicesRomWriteBuffer_done;
    }

    //
    // Look at each sector, and if the changes to the data in the sector
    // require an erase, do so, and write data back to the ROM as necessary.
    //
    pLwrSector = pSectors;
    while (sectorOffset < (start + size))
    {
        //
        // Compare the new data to the old data to determine whether an erase
        // is necessary. An erase will only be necessary if any bit in the
        // sector changes from a 0 to a 1 (1->0 changes should not require an
        // erase).
        //
        i = (sectorOffset < start) ? (start - sectorOffset) : 0;
        j = (sectorOffset > start) ? (sectorOffset - start) : 0;
        bytesToWrite = ((sectorOffset + LW_SPI_ROM_SECTOR_SIZE_IN_BYTES) <
                            (start + size)) ?
                        (LW_SPI_ROM_SECTOR_SIZE_IN_BYTES - i) : (size - j);

        for (k = 0; k < bytesToWrite; k++)
        {
            newByte = *((LwU8 *)pBuf + j + k);
            oldByte = pLwrSector[i + k];
            if (((newByte ^ oldByte) & newByte) != 0)
            {
                break;
            }

            pLwrSector[i + k] = newByte;
        }

        if (k < bytesToWrite)
        {
            //
            // A sector erase will be required - copy the remainder of the
            // bytes over to the sector buffer
            //
            memcpy(&(pLwrSector[i + k]), (pBuf + j + k), (bytesToWrite - k) * sizeof(LwU8));

            spiDevErase.flags = 0;
            spiDevErase.version = 0;
            spiDevErase.type = LW2080_CTRL_SPI_DEVICE_TYPE_ROM;
            spiDevErase.devIndex = devIdx;
            spiDevErase.in.rom.startAddress = sectorOffset;
            spiDevErase.in.rom.sizeInBytes = LW_SPI_ROM_SECTOR_SIZE_IN_BYTES;
            spiDevErase.out.rom.sizeInBytes = 0;

            rc = SpiDeviceEraseBuffer(spiDevErase);
            if (OK != rc)
            {
                Printf(Tee::PriHigh, "%s: \t Failed to erase sector.\n", __FUNCTION__);
                goto SpiDevicesRomWriteBuffer_done;
            }

            // Write the entire sector back to the ROM
            bytesToWrite = LW_SPI_ROM_SECTOR_SIZE_IN_BYTES;
            k = 0;
        }
        else
        {
            //
            // A sector erase is not required - simply program the pages that
            // were touched in this sector
            //
            k = i;
        }

        // Fill in Write params and program the pages that were touched.
        spiDevWrite.flags = 0;
        spiDevWrite.version = 0;
        spiDevWrite.type = LW2080_CTRL_SPI_DEVICE_TYPE_ROM;
        spiDevWrite.devIndex = devIdx;
        spiDevWrite.in.rom.startAddress = sectorOffset + k;
        spiDevWrite.in.rom.sizeInBytes = bytesToWrite;
        spiDevWrite.in.rom.pBuffer = &pLwrSector[k];
        spiDevWrite.out.rom.sizeInBytes = 0;

        rc = SpiDeviceWriteBuffer(spiDevWrite);
        if (OK != rc)
        {
            goto SpiDevicesRomWriteBuffer_done;
        }

        sectorOffset += LW_SPI_ROM_SECTOR_SIZE_IN_BYTES;
        pLwrSector   += LW_SPI_ROM_SECTOR_SIZE_IN_BYTES;
    }

SpiDevicesRomWriteBuffer_done:
    return rc;
}

RC SpiTest::SpiDeviceRomWriteTest(LwU8 devIdx, LwU8 *pBuf, LwU32 start, LwU32 size)
{
    RC     rc = OK;
    LwU8  *pVerifBuffer = NULL;

    rc = SpiDeviceRomWriteBuffer(devIdx, pBuf, start, size);
    if (OK != rc)
    {
        goto SpiDeviceRomWriteTest_done;
    }

    pVerifBuffer = new LwU8[size];
    if (pVerifBuffer == NULL)
    {
        Printf(Tee::PriHigh, "%s: \t\t Failed to allocate memory buffer\n", __FUNCTION__);
        rc = RC::LWRM_ERROR;
        goto SpiDeviceRomWriteTest_done;
    }

    // Read back to verify.
    rc = SpiDeviceRomReadBuffer(devIdx, pVerifBuffer, start, size);
    if (OK != rc)
    {
        goto SpiDeviceRomWriteTest_done;
    }

    // Verify that write was successful.
    if (memcmp(pVerifBuffer, pBuf, size) != 0)
    {
        rc = RC::LWRM_ERROR;
        goto SpiDeviceRomWriteTest_done;
    }

SpiDeviceRomWriteTest_done:
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s: \t SPI ROM Write Test failed on device:%d. \n", __FUNCTION__, devIdx);
    }
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ SpiTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(SpiTest, RmTest,"SpiTest RM test.");

