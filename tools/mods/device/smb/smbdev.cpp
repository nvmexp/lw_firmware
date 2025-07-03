/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file smbdev.cpp
//! \brief Implementation of class for Smbus devices
//!
//! Contains class implementation for Smbus devices
//!

#include "smbdev.h"
#include "smbreg.h"
#include "smbport.h"
#include "lwsmbpt.h"
#include "intsmbpt.h"
#include "amdsmbpt.h"
#include "amdsmbrg.h"
#include "cheetah/include/logmacros.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "lwmisc.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME  "SmbDev"

SmbDevice::SmbDevice(UINT32 Domain, UINT32 Bus, UINT32 Dev, UINT32 Func, UINT32 ChipVersion)  //constructor
    : McpDev(Domain,Bus,Dev,Func,ChipVersion)
     ,m_InitCount(0)
{
    LOG_ENT();
    m_InitState = INIT_CREATED;
    LOG_EXT();
}

SmbDevice::~SmbDevice() //destructor
{
    LOG_ENT();
    m_InitCount = 1;
    Uninitialize(INIT_CREATED);
    LOG_EXT();
}

RC SmbDevice::Initialize(InitStates target)
{
    LOG_ENT();
    UINT16 command;
    RC rc;

    // Only track full initializations
    if (target == INIT_DONE)
    {
        m_InitCount++;
    }

    //check if already initialized
    if(target<=m_InitState)
    {
        Printf(Tee::PriDebug,"\nSmb Device already initialized\n");
        return OK;
    }

    if( (target>m_InitState) && m_InitState<INIT_PCI )
    {
        // Get more Pci Cfg Spec
        CHECK_RC(ReadBasicInfo());
        PciRd16(LW_PCI_COMMAND,&command);

        // Enable IO space
        if (FLD_TEST_DRF_NUM(_PCI, _COMMAND, _IO_SPACE_EN, 0, command))
            CHECK_RC(PciWr16(LW_PCI_COMMAND, FLD_SET_DRF_NUM(_PCI, _COMMAND, _IO_SPACE_EN, 1, command)));

        CHECK_RC(PciRd16(LW_PCI_COMMAND, &command));
        if (FLD_TEST_DRF_NUM(_PCI, _COMMAND, _IO_SPACE_EN, 0, command))
        {
            return RC::CANNOT_ENABLE_IO_MEM_SPACE;
        }
        m_InitState=INIT_PCI;
    }

    if( (target>m_InitState) && m_InitState<INIT_BAR)
    {
        // find the I/O range base addresses
        UINT32 ctrlBase0 = 0;
        UINT32 ctrlBase1 = 0;
        UINT08 decodeEn0 = 0;
        UINT08 decodeEn1 = 0;

        switch(m_ChipVersion)
        {
            case Chipset::AMD:
                // Read 0th byte of the DecodeEn register (aka PMx00 in AMD docs)
                Platform::PioWrite08(AMD_SMBUS_PORT_PM_IDX, AMD_SMBUS_PM_00_ENABLE_OFFSET);
                Platform::PioRead08(AMD_SMBUS_PORT_PM_DATA, &decodeEn0);
                if (!(decodeEn0 & AMD_SMBUS_PM_00_ENABLED))
                {
                    Printf(Tee::PriError, "SmbDevice::Initialize - SMBus Host Controller not enabled\n");
                    return RC::DEVICE_NOT_FOUND;
                }

                // Byte 1 of DecodeEn is SmbusAsfIoBase
                Platform::PioWrite08(AMD_SMBUS_PORT_PM_IDX, AMD_SMBUS_PM_00_ADDR_OFFSET);
                Platform::PioRead08(AMD_SMBUS_PORT_PM_DATA, &decodeEn1);

                // ASF IO base address
                ctrlBase0 = (decodeEn1 << AMD_SMBUS_BASE_ADDR_SHIFT) | AMD_SMBUS_BASE_ADDR_LOWER;
                break;

            case Chipset::MCP89:
                /* Follow definition in dev_mcp_fpci_sm.ref under MCP89 tree, use the same offsets as MCP79*/
            default:
                CHECK_RC(PciRd32(SMBUS_PCI_CNTRL0_BASE_OFFSET, &ctrlBase0) );
                if (m_ChipVersion != Chipset::INTEL)
                    CHECK_RC(PciRd32(SMBUS_PCI_CNTRL1_BASE_OFFSET, &ctrlBase1) );
                break;
        }

        //check if bios has set up bars
        ctrlBase0 &= 0xfffffffe;
        if (!ctrlBase0)
        {
            Printf(Tee::PriError, "SmbDevice::Initialize - Bios didn't set up Base for Port 0\n");
            return RC::ILWALID_DEVICE_BAR;
        }

        //create port objects
        SmbPort *subDev;
        switch (m_ChipVersion)
        {
            case Chipset::INTEL:
                subDev = new IntelSmbPort(this, 0, ctrlBase0);
                break;
            case Chipset::AMD:
                subDev = new AmdSmbPort(this, 0, ctrlBase0);
                break;
            default:
                subDev = new LwSmbPort(this, 0, ctrlBase0);
                break;
        }
        if (NULL == subDev)
        {
            Printf(Tee::PriError, "SmbDevice::Initialize - Not enough memory to create port object\n");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
        m_vpSubDev.push_back(subDev);
        Printf(Tee::PriDebug, "\t\tmCtrolBase0: \t0x%08x\n", ctrlBase0);

        if (m_ChipVersion != Chipset::INTEL && m_ChipVersion != Chipset::AMD)
        {
            // Second base control
            ctrlBase1 &= 0xfffffffe;
            if (!ctrlBase1)
            {
                Printf(Tee::PriError, "SmbDevice::Initialize - Bios didn't set up Base for Port 1\n");
                return RC::ILWALID_DEVICE_BAR;
            }

            subDev = new LwSmbPort(this,1,ctrlBase1);
            if (NULL == subDev)
            {
                Printf(Tee::PriError, "SmbDevice::Initialize - Not enough memory to create port object\n");
                delete m_vpSubDev[0];
                return RC::CANNOT_ALLOCATE_MEMORY;
            }
            m_vpSubDev.push_back(subDev);
            Printf(Tee::PriDebug, "\t\tmCtrolBase1: \t0x%08x\n", ctrlBase1);
        }
        m_InitState=INIT_DEV;
    }

    if( (target>m_InitState) && m_InitState<INIT_DONE)
    {
        CHECK_RC(InitializeAllSubDev());

        // default the current port to port_0
        m_LwrSubDevIndex = 0;
        m_InitState = INIT_DONE;
    }
    LOG_EXT();
    return OK;
}

RC SmbDevice::Search()
{
    LOG_ENT();
    RC rc;
    SmbPort *pPort = NULL;
    UINT08 index=0xff;
    while(OK == GetNextSubDev(&index, &pPort))
    {
        CHECK_RC(pPort->Search());
    }

    LOG_EXT();
    return OK;
}

RC SmbDevice::Uninitialize(InitStates target)
{
    LOG_ENT();

    RC rc;

    // Only track full de-initializations
    if ((target == INIT_CREATED) && --m_InitCount)
    {
        Printf(Tee::PriDebug,
               "Smb Device skipping uninitialization, InitCount = %d\n",
               m_InitCount);
        LOG_EXT();
        return OK;
    }

    if( (target<m_InitState) && m_InitState==INIT_DONE )
    {
        CHECK_RC(UninitializeAllSubDev());
        m_InitState = INIT_DEV;
    }

    if( (target<m_InitState) && m_InitState==INIT_DEV)
    {
        //free ports
        SmbPort *pPort = NULL;
        //set start index to -1
        UINT08 index = 0xff;
        while(OK==GetNextSubDev(&index, &pPort))
        {
            delete pPort;
        }
        m_vpSubDev.clear();

        //reverse steps as initialize
        m_InitState = INIT_CREATED;
    }
    LOG_EXT();
    return OK;
}

RC SmbDevice::DeviceRegRd(UINT32 offset, UINT32* pData)
{
    RC rc;
    SmbPort *pPort = NULL;
    CHECK_RC(GetLwrrentSubDev(&pPort));
    return pPort->RegRd(offset,pData);
}

RC SmbDevice::DeviceRegWr(UINT32 offset, UINT32 Data)
{
    RC rc;
    SmbPort *pPort = NULL;
    CHECK_RC(GetLwrrentSubDev(&pPort));
    return pPort->RegWr(offset,Data);
}

RC SmbDevice::PrintInfoController(Tee::Priority priority)
{
    Printf(priority, "Port info:\n");
    SmbPort * pPort = NULL;
    UINT08 index = 0xff;
    while(OK == GetNextSubDev(&index,&pPort))
    {
        pPort->PrintInfo(priority);
    }
    return OK;
}

RC SmbDevice::PrintReg(Tee::Priority Pri)
{
    RC rc;
    UINT32 lwrrentPort;
    UINT08 pIndex = 0xff;
    CHECK_RC(GetLwrrentSubDev(&lwrrentPort));
    Printf(Pri, "\nRegisters for Current Smbus Port: %d", lwrrentPort);
    SmbPort *pPort = NULL;
    CHECK_RC(GetLwrrentSubDev(&pPort));
    pPort->PrintReg(Pri);

    CHECK_RC(GetNextSubDev(&pIndex, &pPort));
    CHECK_RC(GetLwrrentSubDev(&lwrrentPort));
    Printf(Tee::PriNormal, "\nRegisters for Smbus Port: %d", lwrrentPort);
    pPort->PrintReg();
    return rc;
}
