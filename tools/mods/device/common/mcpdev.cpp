/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/apicreg.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "device/include/mcp.h"
#include "device/include/mcpdev.h"
#include "device/include/mcpmacro.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "device/smb/smbport.h"
#include <ctype.h>

#ifndef LWCPU_FAMILY_ARM
    #include "device/include/chipset.h"
#endif

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define  CLASS_NAME  "McpDev"

UINT32 McpDev::m_PrintMask = 0;
UINT32 McpDev::m_TimeoutScaler = 1;

McpDev::McpDev(UINT32 Domain, UINT32 Bus, UINT32 Device, UINT32 Func, UINT32 ChipVersion)
{
    m_Domain = Domain;
    m_Bus = Bus;
    m_Dev = Device;
    m_Func = Func;

    m_VendorId = 0;
    m_DeviceId = 0;
    m_ClassCode = 0;
    m_ChipVersion = ChipVersion;
    m_CapLength = 0;
    m_LwrSubDevIndex = 0xff;
    m_AddrBits = MEM_ADDRESS_32BIT;
    m_pCap = nullptr;
    m_InitState = INIT_CREATED;
    m_NAssociatedTests = 0;
}

McpDev::~McpDev()
{
    MASSERT(m_InitState == INIT_CREATED);
}

RC McpDev::Validate()
{
    return OK;
}

RC McpDev::ForcePageRange(UINT32 ForceAddrBits)
{
    LOG_ENT();
    if (m_InitState >= INIT_PCI)
    {
        LOG_ERR("ForcePageRange should be called prior to device initialization");
        return RC::WAS_NOT_INITIALIZED;
    }
    else
        m_AddrBits = ForceAddrBits;

    LOG_EXT();
    return OK;
}

RC McpDev::GetAddrBits(UINT32 *pDmaBits)
{
    *pDmaBits = m_AddrBits;
    return OK;
}

RC McpDev::Search()
{
    Printf(Tee::PriNormal, " Search function is not yet implemented for current device \n");
    return OK;
}

RC McpDev::SetPrintMask(UINT32 Mask)
{
    m_PrintMask = Mask;
    return OK;
}

RC McpDev::GetPrintMask(UINT32* pMask)
{
    MASSERT(pMask);
    *pMask = m_PrintMask;
    return OK;
}

RC McpDev::ReadBasicInfo()
{
    RC rc;
    CHECK_RC(GetVendorId());
    CHECK_RC(GetDeviceId());
    CHECK_RC(GetClassCode());
    return OK;
}

RC McpDev::GetNextSubDev(UINT08 *pIndex, SmbPort** ppSubDev)
{
    //ATTN: Using UINT08 *pIndex to reduce the processing time in this function is crutial to MSI routine

    if(!pIndex)
    {
        LOG_ERR("Invalid int pointer pIndex");
        return RC::BAD_PARAMETER;
    }

    if(!ppSubDev)
    {
        LOG_ERR("Invalid McpSubDev pointer ppSubDev");
        return RC::BAD_PARAMETER;
    }

    if(m_InitState < INIT_DEV)
    {
        LOG_ERR("Device has not been fully initialized: sub-devices not discovered");
        return RC::WAS_NOT_INITIALIZED;
    }

    bool isFound = false;
    (*ppSubDev) = NULL;
    UINT32 size = m_vpSubDev.size();   //code optimization to reduce the process time

    if(!size)
    {
        return RC::SUB_DEVICE_EMPTY;
    }
    else
    {
        //starting from the next index after (*pIndex)
        while((++(*pIndex))<size)
        {
            if(m_vpSubDev[(*pIndex)])
            {
                // set the return McpSubDev pointer
                (*ppSubDev) = m_vpSubDev[(*pIndex)];
                isFound = true;
                break;
            }
        }
    }

    return isFound ? OK : RC::SUB_DEVICE_ILWALID_INDEX;
}

RC McpDev::ValidateSubDev(UINT32 SubDevIndex)
{
    if (m_InitState < INIT_DONE)
    {
        LOG_ERR("Device not initialized");
        return RC::WAS_NOT_INITIALIZED;
    }
    if (!m_vpSubDev.size())
    {
        LOG_ERR("Not Initialized or NO device found");
        return RC::SUB_DEVICE_EMPTY;
    }
    if (SubDevIndex >= m_vpSubDev.size())
    {
        LOG_ERR("Sub device index is too high");
        return RC::SUB_DEVICE_ILWALID_INDEX;
    }
    if (!m_vpSubDev[SubDevIndex])
    {
        LOG_ERR("Invalid Sub Device Index number, Not implemented");
        return RC::SUB_DEVICE_ILWALID_INDEX;
    }

    return OK;
}

RC McpDev::InitializeAllSubDev()
{
    RC rc;

    if( !m_vpSubDev.size() )
    {
        return OK;
    }

    UINT08 index = 0xff;
    SmbPort *pSubDev;
    while(OK==GetNextSubDev(&index, &pSubDev))
    {
       CHECK_RC(pSubDev->Initialize());
    }

    return OK;
}

RC McpDev::UninitializeAllSubDev()
{
    if( !m_vpSubDev.size() )
    {
        return OK;
    }

    UINT08 index = 0xff;
    SmbPort *pSubDev;
    while(OK==GetNextSubDev(&index, &pSubDev))
    {
        if(OK != pSubDev->Uninitialize())
        {
            LOG_ERR("Failed in Uninitializing McpSubDevice %d", index);
        }
    }

    return OK;
}

RC McpDev::GetNumSubDev(UINT32 *pNumSubDev)
{
    MASSERT(pNumSubDev);
    *pNumSubDev = m_vpSubDev.size();
    return OK;
}

RC McpDev::GetLwrrentSubDev(SmbPort** ppSubDev)
{
    RC rc;
    MASSERT(ppSubDev);
    CHECK_RC(ValidateSubDev(m_LwrSubDevIndex));

    *ppSubDev = m_vpSubDev[m_LwrSubDevIndex];
    return OK;
}

RC McpDev::GetLwrrentSubDev(UINT32* pSubDevIndex)
{
    RC rc;
    MASSERT(pSubDevIndex);
    CHECK_RC(ValidateSubDev(m_LwrSubDevIndex));
    *pSubDevIndex = m_LwrSubDevIndex;
    return OK;
}

RC McpDev::SetLwrrentSubDev(UINT32 SubDevIndex)
{
    RC rc;
    CHECK_RC(ValidateSubDev(SubDevIndex));
    m_LwrSubDevIndex = SubDevIndex;
    return OK;
}

RC McpDev::GetSubDev(UINT32 SubDevIndex, SmbPort** ppSubDev)
{
    RC rc;
    MASSERT(ppSubDev);

    CHECK_RC(ValidateSubDev(SubDevIndex));
    *ppSubDev = m_vpSubDev[SubDevIndex];
    return OK;
}

RC McpDev::LinkExtDev()
{
    return OK;
}

RC McpDev::DumpPci(Tee::Priority Pri)
{
    RC rc;
    UINT16 val16;
    UINT32 val32;
    bool isCapEnable = false;
    // for MSI-x
    bool isEnabled;
    UINT32 tableSize;
    UINT32 tableBase;
    UINT08 tableBir;
    void * pTableBase;

    for (UINT32 i=0; i < (256/4); i++ )
    { // raw data dump
        if ( 0 == (i%4) )
            Printf(Pri, "%02xh", i*4);

        CHECK_RC(PciRd32(i*4, &val32));
        Printf(Pri, " 0x%08x", val32);

        if ( 0 == ((i+1)%4))
            Printf(Pri, "\n");
    }

    Printf(Pri, "VendorId = 0x%04x, DeviceId = 0x%04x, ClassCode = 0x%06x\n",
           m_VendorId, m_DeviceId, m_ClassCode);

    CHECK_RC(PciRd16(PCI_COMMAND_OFFSET, &val16));
    Printf(Pri, "Command = 0x%04x,", val16);
    CHECK_RC(PciRd16(PCI_CONTROL_STATUS_OFFSET, &val16));
    Printf(Pri, "  Status = 0x%04x\n", val16);
    isCapEnable = (val16 & 0x10) != 0;

    for (UINT32 i = 0; i< 6; i++)
    {   //dump BARs
        CHECK_RC(PciRd32(0x10+4*i, &val32));
        if ( val32 | (i==0) )
            Printf(Pri, "Bar%d = 0x%08x\n", i, val32);
    }

    CHECK_RC(PciRd16(PCI_INTR_LINE_OFFSET, &val16));
    Printf(Pri, "Int Pin = 0x%02x,    Int Line = 0x%02x\n", val16 >> 8, val16 & 0xff);

    if (isCapEnable)
    {   //dump cap list
        Printf(Pri, "\tCapabilities List :\n");

        UINT08 capPtr;
        UINT08 myPtr;
        bool   isMSI64bit = true;
        CHECK_RC(PciRd08(PCI_CAP_PTR_OFFSET, &capPtr));

        capPtr &= 0xfc;
        while(capPtr != 0)
        {
            CHECK_RC( PciRd32(capPtr, &val32) );
            Printf(Pri, "Cap found at 0x%02x, ID = 0x%02x : ", capPtr, val32 & 0xff);
            myPtr = capPtr;
            capPtr = ( (val32 & 0xffff) >> 8 );

            switch(val32 & 0xff)
            {
                case PCI_CAP_ID_MSI:
                    Printf(Pri, "MSI\n");
                    val32 = val32 >> 16;
                    isMSI64bit = (val32 & 0x80) != 0;
                    Printf(Pri, "MSI %s, MSI cap = %d, MSI Enabled = %d, 64bit address %s\n",
                           (val32 & 0x1)?"Enabled":"Disabled",
                           1 << ((val32 >> 1) & 0x07),
                           1 << ((val32 >> 4) & 0x07),
                           isMSI64bit?"Yes":"No" );
                    myPtr += 4;
                    CHECK_RC( PciRd32(myPtr, &val32) );
                    myPtr += 4;
                    Printf(Pri, "\tMSI message addr. 0x%08x\n", val32);
                    if ( isMSI64bit )
                    {
                        CHECK_RC( PciRd32(myPtr, &val32) );
                        myPtr += 4;
                        Printf(Pri, "\tMSI message upper addr. 0x%08x\n", val32);
                    }
                    CHECK_RC( PciRd16(myPtr, &val16) );
                    Printf(Pri, "\tMSI message data 0x%04x\n", val16);

                    break;
                case PCI_CAP_ID_MSIX:
                    Printf(Pri, "MSI-X\n");
                    val32 = val32 >> 16;
                    isEnabled = ((val32 & 0x8000) != 0);
                    tableSize = (val32 & 0x7ff) + 1;
                    Printf(Pri, "MSI-X %s, Func Mask %s, Table Size = %d\n",
                           isEnabled?"Enabled":"Disabled",
                           (val32 & 0x4000)?"1":"0",
                           tableSize);
                    myPtr += 4;
                    CHECK_RC( PciRd32(myPtr, &val32) );
                    tableBase = val32 & 0xfffffff8;
                    tableBir = val32 & 0x7;
                    Printf(Pri, "\tTable Offset 0x%x, Table BIR %u\n",
                           tableBase,
                           tableBir);
                    myPtr += 4;
                    CHECK_RC( PciRd32(myPtr, &val32) );
                    Printf(Pri, "\tPBA Offset 0x%x, PBA BIR %u\n",
                           (val32 & 0xfffffff8),
                           (val32 & 0x7));
                    if (isEnabled)
                    { // dump the MSI-X table is enabled
                        CHECK_RC(PciRd32(PCI_BAR_OFFSET(tableBir), &val32));
                        val32 = val32 & 0xfffffff0;
                        val32 = val32 + tableBase;
                        Platform::MapDeviceMemory(&pTableBase,
                                                  val32,
                                                  tableSize * MSIX_TABLE_ENTRY_SIZE,
                                                  Memory::UC, Memory::ReadWrite);
                        UINT32 * pTemp = (UINT32 *) pTableBase;
                        for (UINT32 i = 0; i < tableSize; i++)
                        {
                            Printf(Pri, "  %u - Msg Addr 0x%x%08x, Msg Data 0x%x, Control 0x%x\n",
                                   i, MEM_RD32(pTemp+1), MEM_RD32(pTemp), MEM_RD32(pTemp+2), MEM_RD32(pTemp+3));

                            pTemp += 4;
                        }
                        Platform::UnMapDeviceMemory(pTableBase);
                    }
                    // ignore the PBA for now
                    break;
                case PCI_CAP_ID_PCIE:
                    Printf(Pri, "PCI-E\n");
                    break;
                case 0x01:
                    Printf(Pri, "PCI Power Management \n");
                    break;
                default:
                    Printf(Pri, "Unknown Cap %u \n", val32 & 0xff);
            }
        }
    }
    return OK;
}

RC McpDev::EnableBusMastering()
{
    RC rc;
    UINT16 cmd = 0;
    CHECK_RC(PciRd16(PCI_COMMAND_OFFSET, &cmd));
    if (FLD_TEST_RF_NUM(LW_PCI_COMMAND_BUS_MASTER_EN, 0, cmd))
    {
        CHECK_RC(PciWr16(PCI_COMMAND_OFFSET, cmd | RF_SET(LW_PCI_COMMAND_BUS_MASTER_EN)));
        CHECK_RC(PciRd16(PCI_COMMAND_OFFSET, &cmd));
        if (FLD_TEST_RF_NUM(LW_PCI_COMMAND_BUS_MASTER_EN, 1, cmd))
        {
            LOG_WRN("MODS enabled BUS_MASTER!");
        }
        else
        {
            LOG_ERR("Cannot enable BUS_MASTER! cmd = 0x%04x", cmd);
            return RC::CANNOT_ENABLE_BUS_MASTER;
        }
    }
    return OK;
}

RC McpDev::RegRd(UINT32 Offset, UINT32* pData, bool IsDebug)
{
    RC rc;
    if (m_InitState < INIT_BAR)
    {
        LOG_ERR("BAR values not initialized");
        return RC::WAS_NOT_INITIALIZED;
    }
    CHECK_RC(DeviceRegRd(Offset, pData));
    if (m_PrintMask & PRINTMASK_REGRD)
    {
        Printf(Tee::PriHigh, "RegRd(0x%x) -> 0x%08x\n", Offset, *pData);
    }
    else if (IsDebug)
    {
        Printf(Tee::PriNormal, "RegRd(0x%x) -> 0x%08x\n", Offset, *pData);
    }
    return OK;
}

RC McpDev::RegWr(UINT32 Offset, UINT32 Data, bool IsDebug)
{
    RC rc;
    if (m_InitState < INIT_BAR)
    {
        LOG_ERR("BAR values not initialized");
        return RC::WAS_NOT_INITIALIZED;
    }
    CHECK_RC(DeviceRegWr(Offset, Data));
    if (m_PrintMask & PRINTMASK_REGWR)
    {
        Printf(Tee::PriHigh, "RegWr(0x%x) <- 0x%08x\n", Offset, Data);
    }
    else if (IsDebug)
    {
        Printf(Tee::PriNormal, "RegWr(0x%x) <- 0x%08x\n", Offset, Data);
    }

    return OK;
}

RC McpDev::PciRd32(UINT32 Offset, UINT32* pData)
{
    RC rc;
    CHECK_RC(Platform::PciRead32(m_Domain, m_Bus, m_Dev, m_Func, Offset, pData));
    if (m_PrintMask & PRINTMASK_PCIRD)
    {
        Printf(Tee::PriHigh, "PciRd32(0x%x, 0x%x, 0x%x, 0x%x, 0x%x) <- 0x%08x\n",
               m_Domain, m_Bus, m_Dev, m_Func, Offset, *pData);
    }
    return rc;
}

RC McpDev::PciWr32(UINT32 Offset, UINT32 Data)
{
    RC rc;
    CHECK_RC(Platform::PciWrite32(m_Domain, m_Bus, m_Dev, m_Func, Offset, Data));
    if (m_PrintMask & PRINTMASK_PCIWR)
    {
        Printf(Tee::PriHigh, "PciWr32(0x%x, 0x%x, 0x%x, 0x%x, 0x%x) -> 0x%08x\n",
               m_Domain, m_Bus, m_Dev, m_Func, Offset, Data);
    }
    return rc;
}

RC McpDev::PciRd16(UINT32 Offset, UINT16* pData)
{
    RC rc;
    CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Dev, m_Func, Offset, pData));
    if (m_PrintMask & PRINTMASK_PCIRD)
    {
        Printf(Tee::PriHigh, "PciRd16(0x%x, 0x%x, 0x%x, 0x%x, 0x%x) <- 0x%04x\n",
               m_Domain, m_Bus, m_Dev, m_Func, Offset, *pData);
    }
    return rc;
}

RC McpDev::PciWr16(UINT32 Offset, UINT16 Data)
{
    RC rc;
    CHECK_RC(Platform::PciWrite16(m_Domain, m_Bus, m_Dev, m_Func, Offset, Data));
    if (m_PrintMask & PRINTMASK_PCIWR)
    {
        Printf(Tee::PriHigh, "PciWr16(0x%x, 0x%x, 0x%x, 0x%x, 0x%x) -> 0x%04x\n",
               m_Domain, m_Bus, m_Dev, m_Func, Offset, Data);
    }
    return rc;
}

RC McpDev::PciRd08(UINT32 Offset, UINT08* pData)
{
    RC rc;
    CHECK_RC(Platform::PciRead08(m_Domain, m_Bus, m_Dev, m_Func, Offset, pData));
    if (m_PrintMask & PRINTMASK_PCIRD)
    {
        Printf(Tee::PriHigh, "PciRd08(0x%x, 0x%x, 0x%x, 0x%x, 0x%x) <- 0x%02x\n",
               m_Domain, m_Bus, m_Dev, m_Func, Offset, *pData);
    }
    return rc;
}

RC McpDev::PciWr08(UINT32 Offset, UINT08 Data)
{
    RC rc;
    CHECK_RC(Platform::PciWrite08(m_Domain, m_Bus, m_Dev, m_Func, Offset, Data));
    if (m_PrintMask & PRINTMASK_PCIWR)
    {
        Printf(Tee::PriHigh, "PciWr08(0x%x, 0x%x, 0x%x, 0x%x, 0x%x) -> 0x%02x\n",
               m_Domain, m_Bus, m_Dev, m_Func, Offset, Data);
    }
    return rc;
}

RC McpDev::PrintInfo(Tee::Priority Pri, bool IsBasic,
                     bool IsController, bool IsExternal)
{
    RC rc;

    if( IsBasic )
    {
        Printf(Pri, ">Basic info:\n");
        Printf(Pri, "Device at (0x%04x, 0x%02x, 0x%02x, 0x%02x)", m_Domain, m_Bus, m_Dev, m_Func);
        Printf(Pri, "\t(Init_state = %i)\n", m_InitState);
        Printf(Pri, "\tClassCode = 0x%06x\n", m_ClassCode);
        Printf(Pri, "\tVendorId  = 0x%04x, DeviceId = 0x%04x\n", m_VendorId, m_DeviceId);

        if ( m_InitState >= INIT_PCI )
        {
            CHECK_RC(PrintInfoBasic(Pri));
        }
    }
    if( (IsController)&&(m_InitState > INIT_BAR) )
    {
        Printf(Pri, "\n>Controller capability:\n");
        CHECK_RC(PrintCap());
        Printf(Pri, "\n>Controller info:\n");
        CHECK_RC(PrintInfoController(Pri));
    }
    if( (IsExternal)&&(m_InitState >= INIT_DONE) )
    {
        Printf(Pri, "\n>External info:\n");
        CHECK_RC(PrintInfoExternal(Pri));
    }
    return OK;
}

RC McpDev::GetVendorId(UINT16* pId)
{
    RC rc;

    if (!m_VendorId)
        CHECK_RC( PciRd16(PCI_VENDOR_ID_OFFSET, &m_VendorId) );

    if(pId != NULL)
        *pId = m_VendorId;
    return OK;
}

RC McpDev::GetDeviceId(UINT16* pId)
{
    RC rc;

    if (!m_DeviceId)
        CHECK_RC( PciRd16(PCI_DEVICE_ID_OFFSET, &m_DeviceId) );

    if(pId != NULL)
        *pId = m_DeviceId;

    return OK;
}

RC McpDev::GetClassCode(UINT32* pId)
{
    RC rc;

    if (!m_ClassCode)
    {
        CHECK_RC( PciRd32(PCI_CLASS_CODE_OFFSET, &m_ClassCode) );
        m_ClassCode >>= 8;
    }

    if(pId != NULL)
        *pId = m_ClassCode;
    return OK;
}

RC McpDev::GetDomainBusDevFunc(UINT32* pDomain, UINT32* pBus, UINT32* pDev, UINT32* pFunc)
{
    if (pDomain != NULL)
        *pDomain = m_Domain;

    if (pBus != NULL)
        *pBus = m_Bus;

    if (pDev != NULL)
        *pDev = m_Dev;

    if (pFunc != NULL)
        *pFunc = m_Func;

    return OK;
}

RC McpDev::OverrideCap(UINT32 ChipVersion)
{
    RC rc = OK;
    m_ChipVersion = ChipVersion;
    CHECK_RC(ReadInCap());
    return rc;
}

UINT32 McpDev::GetChipVersion()
{
    return m_ChipVersion;
}

Chipset::ChipSku McpDev::GetChipSku()
{
    return static_cast<Chipset::ChipSku>(0);
}

bool McpDev::IsLwDevice()
{
    if(OK != GetVendorId())
    {
        LOG_ERR("Could not read register!");
        return false;
    }
    if (m_VendorId != VENDOR_ID_LW)
    {
        Printf(Tee::PriDebug, "    Not an LW device! Vendor = 0x%04x\n", m_VendorId);
        return false;
    }

    return true;  // Lwpu Device
}

// For right now, just keep track of the number of assocated tests. Hopefully
// this will always be 1. In the future, we may want to keep track of the
// actual pointer.
RC McpDev::AssociateTest()
{
    m_NAssociatedTests++;
    return OK;
}

RC McpDev::DeAssociateTest()
{
    m_NAssociatedTests--;
    return OK;
}

bool McpDev::HasAssociatedTest()
{
    return(m_NAssociatedTests != 0);
}

//Common utility functions
bool McpDev::PollMem(void * pPollArgs)
{
    PollMemArgs * pArgs = (PollMemArgs*)pPollArgs;

    UINT32 value = MEM_RD32(pArgs->pMemAddr);

    if (!pArgs->exp)
    {
        if (!(value & pArgs->unexp))
            return true;
        else
            return false;
    }
    else
    {
        if ( ((value & pArgs->exp)==pArgs->exp)
             && ((value & pArgs->unexp)==0) )
            return true;
        else
            return false;
    }
}

bool McpDev::PollMemNotEqual(void *pArgs)
{
    UINT32 volatile *const pSts = (UINT32 *)(uintptr_t)(*((UINT32 *) pArgs));
    UINT32 Val = *((UINT32 *) ((size_t) pArgs + 4));

    UINT32 Status = *pSts;
    if(Status!=Val)
        return true;
    else
        return false;
}

bool McpDev::PollMemClearAny(void * pPollArgs)
{
    PollMemClearAnyArgs * pArgs = (PollMemClearAnyArgs *)pPollArgs;

    UINT32 value = MEM_RD32(pArgs->pMemAddr);

    if ((value & pArgs->unexp) != pArgs->unexp)
        return true;
    else
        return false;
}

bool McpDev::PollMemSetAny(void * pPollArgs)
{
    PollMemSetAnyArgs * pArgs = (PollMemSetAnyArgs *)pPollArgs;
    UINT32 value = MEM_RD32(pArgs->pMemAddr);

    if (value & pArgs->exp)
        return true;
    else
        return false;
}

bool McpDev::PollReg(void * pPollArgs)
{
    PollRegArgs * pArgs = (PollRegArgs *)pPollArgs;

    UINT32 value = 0;
    MASSERT(pArgs->pDev);
    if (OK != pArgs->pDev->RegRd(pArgs->regCode, &value))
    {
        LOG_ERR("Could not read register!");
        return false;
    }
    if ( ((value & pArgs->exp) == pArgs->exp)
         && ((value & pArgs->unexp) == 0) )
         return true;
    return false;
}

bool McpDev::PollIo(void *pPollArgs)
{
    PollIoArgs *pArgs = (PollIoArgs*)pPollArgs;
    UINT08 val;
    Platform::PioRead08(pArgs->ioAddr,&val);
    if(((val & pArgs->exp) == pArgs->exp)
       && ((val & pArgs->unexp) == 0) )
        return true;
    return false;
}

bool McpDev::NoCaseStrEq(const string S1, const string S2)
{
    string::const_iterator it1 = S1.begin();
    string::const_iterator it2 = S2.begin();
    UINT32 size1=S1.size();
    UINT32 size2=S2.size();

    if (size1 != size2)
    {
        return false;
    }

    while ((it1 != S1.end()) && (it2 != S2.end()))
    {
        if (toupper((char)*it1) != toupper((char)*it2))
        {
            return false;
        }
        ++it1;
        ++it2;
    }
    return true;
}

RC McpDev::GetCap(UINT32 CapIndex, UINT32 * CapValue)
{
    if (CapValue)
    {
        if (CapIndex >= m_CapLength)
        {
            LOG_DBG("Cap Length = %d\n",m_CapLength);
            LOG_ERR("Capability not defined");
            return RC::BAD_PARAMETER;
        }
        else
        {
            *CapValue = m_pCap[CapIndex];
            LOG_DBG("CapIndex = %d, Capability = %d\n",
                   CapIndex,*CapValue);
        }

    }
    else
    {
        LOG_ERR("Null Pointer");
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC McpDev::GetCapList(const UINT32** CapList, UINT32* CapLength)
{
    if (CapList && CapLength)
    {
        *CapLength = m_CapLength;
        *CapList = m_pCap;
    }
    else
    {
        LOG_ERR("Null Pointer");
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC McpDev::SetTimeoutScaler(UINT32 TimeoutScaler)
{
    LOG_ENT();
    m_TimeoutScaler = TimeoutScaler;

    return OK;
}

RC McpDev::GetTimeoutScaler(UINT32* pTimeoutScaler)
{
    MASSERT(pTimeoutScaler);
    *pTimeoutScaler = m_TimeoutScaler;
    return OK;
}

RC McpDev::WaitRegMem(UINT32 VirtMemBase, UINT32 RegCode, UINT32 Exp, UINT32 UnExp, UINT32 TimeoutMs)
{
    LOG_ENT();
    PollMemArgs args;
    args.memAddr = VirtMemBase + RegCode;
    args.exp = Exp;
    args.unexp = UnExp;

    return Tasker::Poll(PollMem, (void*)(&args), TimeoutMs * m_TimeoutScaler);
}

RC McpDev::WaitRegMem(void *pVirtMemBase, UINT32 RegCode, UINT32 Exp, UINT32 UnExp, UINT32 TimeoutMs)
{
    LOG_ENT();
    PollMemArgs args;
    args.pMemAddr = (char *)pVirtMemBase + RegCode;
    args.exp = Exp;
    args.unexp = UnExp;

    return Tasker::Poll(PollMem, (void*)(&args), TimeoutMs * m_TimeoutScaler);
}

RC McpDev::WaitRegDev(McpDev* pDev, UINT32 RegCode, UINT32 Exp, UINT32 UnExp, UINT32 TimeoutMs)
{
    LOG_ENT();
    PollRegArgs args;
    args.pDev = pDev;
    args.regCode = RegCode;
    args.exp = Exp;
    args.unexp = UnExp;

    return Tasker::Poll(PollReg, (void*)(&args), TimeoutMs * m_TimeoutScaler);
}

RC McpDev::WaitRegBits(McpDev* pDev, UINT32 RegCode, UINT32 MsBit, UINT32 LsBit, UINT32 Exp, UINT32 TimeoutMs)
{
    MASSERT(MsBit >= LsBit);
    UINT32 mask =((1 << (MsBit - LsBit + 1)) - 1) << LsBit;
    UINT32 exp = Exp << LsBit;
    UINT32 unExp = (~exp) & mask;

    return McpDev::WaitRegDev(pDev, RegCode, exp, unExp, TimeoutMs * m_TimeoutScaler);
}

RC McpDev::WaitRegBitClearAnyMem(UINT32 VirtMemBase, UINT32 RegCode, UINT32 UnExp, UINT32 TimeoutMs)
{
    LOG_ENT();
    PollMemClearAnyArgs args;
    args.memAddr = VirtMemBase + RegCode;
    args.unexp = UnExp;

    return Tasker::Poll(PollMemClearAny, (void*)(&args), TimeoutMs * m_TimeoutScaler);
}

RC McpDev::WaitRegBitClearAnyMem(void *pVirtMemBase, UINT32 RegCode, UINT32 UnExp, UINT32 TimeoutMs)
{
    LOG_ENT();
    PollMemClearAnyArgs args;
    args.pMemAddr = (char *)pVirtMemBase + RegCode;
    args.unexp = UnExp;

    return Tasker::Poll(PollMemClearAny, (void*)(&args), TimeoutMs * m_TimeoutScaler);
}

RC McpDev::WaitRegBitSetAnyMem(void *pVirtMemBase, UINT32 RegCode, UINT32 Exp, UINT32 TimeoutMs)
{
    LOG_ENT();
    PollMemSetAnyArgs args;
    args.pMemAddr = (char *)pVirtMemBase + RegCode;
    args.exp = Exp;

    return Tasker::Poll(PollMemSetAny, (void*)(&args), TimeoutMs * m_TimeoutScaler);
}

JS_CLASS(McpDev);
SObject McpDev_Object
    (
    "McpDev",
    McpDevClass,
    0,
    0,
    "McpDev constants"
    );

PROP_CONST(McpDev, PMASK_PCIRD, PRINTMASK_PCIRD);
PROP_CONST(McpDev, PMASK_PCIWR, PRINTMASK_PCIWR);
PROP_CONST(McpDev, PMASK_REGRD, PRINTMASK_REGRD);
PROP_CONST(McpDev, PMASK_REGWR, PRINTMASK_REGWR);
JS_STEST_BIND_ONE_ARG_NAMESPACE(McpDev, SetPrintMask, UINT32, Mask,
                                "Set the mask that controls special printing abilities");
JS_STEST_BIND_ONE_ARG_NAMESPACE(McpDev, SetTimeoutScaler, UINT32, TimeoutScaler,
                                "Sets the scaler to use for all timeouts in the code");
C_(McpDev_GetPrintMask)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: McpDev.GetPrintMask()");
        return JS_FALSE;
    }
    UINT32 value = 0;
    if (OK != McpDev::GetPrintMask(&value))
    {
        JS_ReportError(pContext, "Error oclwred in McpDev.GetPrintMask()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    if (OK != JavaScriptPtr()->ToJsval(value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwred in McpDev.GetPrintMask()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}
static SMethod McpDev_GetPrintMask
(
    McpDev_Object,
    "GetPrintMask",
    C_McpDev_GetPrintMask,
    0,
    "Gets the mask controlling special printing abilities"
);

C_(McpDev_GetTimeoutScaler)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: McpDev.GetTimeoutScaler()");
        return JS_FALSE;
    }
    UINT32 value = 0;
    if (OK != McpDev::GetTimeoutScaler(&value))
    {
        JS_ReportError(pContext, "Error oclwred in McpDev.GetTimeoutScaler()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    if (OK != JavaScriptPtr()->ToJsval(value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwred in McpDev.GetTimeoutScaler()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}
static SMethod McpDev_GetTimeoutScaler
(
    McpDev_Object,
    "GetTimeoutScaler",
    C_McpDev_GetTimeoutScaler,
    0,
    "Gets the TimeoutScaler value"
);
