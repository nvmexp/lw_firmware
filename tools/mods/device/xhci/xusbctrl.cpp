/* LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xusbctrl.h"
#include "xusbctrlmgr.h"
#include "usbtype.h"
#include "xusbreg.h"
#include "core/include/tee.h"
#include "core/include/rc.h"
#include "core/include/platform.h"
#include "cheetah/include/tegradrf.h"
#include "device/include/memtrk.h"
#include "usbdext.h"
#include "core/include/cnslmgr.h"
#include "core/include/utility.h"
#include "core/include/platform.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "XusbController"

XusbController::XusbController()
{
    LOG_ENT();

    m_ErstMax       = 0;
    m_EventRingSize = XUSB_DEFAULT_EVENT_RING_SIZE;
    m_XferRingSize  = XUSB_DEFAULT_TRANSFER_RING_SIZE;

    m_pEventRing            = nullptr;
    m_pDevmodeEpContexts    = nullptr;
    m_DevDescriptor.Address = nullptr;

    m_DeviceState   = XUSB_DEVICE_STATE_DEFAULT;
    m_UsbSpeed      = 0;
    m_UsbVer        = 0x300;        // USB 3.0 spec

    m_DevClass      = 0;
    m_ConfigIndex   = 0;

    m_vpRings.clear();
    m_vConfigDescriptor.clear();
    m_vStringDescriptor.clear();

    m_vIsochBuffers.clear();
    m_IsoTdSize     = 1024 * 16;

    m_DevId         = 0;
    m_RetryTimes    = 0;
    m_RetryTimesBuf = 0;
    m_TimeToFinish  = 0;

    m_pJsCbDisconn  = nullptr;
    m_pJsCbConn     = nullptr;
    m_pJsCbU3U0     = nullptr;

    m_pBufBuild     = new BufBuild();
    m_IsEnBufBuild  = false;
    m_bufIndex      = 0;

    m_BulkDelay     = 0;
    m_IsRstXferRing = false;

    m_vDataPattern32.clear();

    LOG_EXT();
}

XusbController::~XusbController()
{
    LOG_ENT();
    delete m_pBufBuild;
    LOG_EXT();
}

RC XusbController::InitBar()
{
    LOG_ENT();
    RC rc = OK;

    CHECK_RC(Controller::InitBar());
    // read Device ID for later use
    CHECK_RC(CfgRd32(0, &m_DevId));
    m_DevId >>= 16;
    Printf(Tee::PriNormal, "  XUSB DevID = 0x%04x \n", m_DevId);

    LOG_EXT();
    return rc;
}

RC XusbController::InitRegisters()
{
    LOG_ENT();
    RC rc = OK;
    UINT32 val = 0;

    // see also bug http://lwbugs/200039349
    CHECK_RC(RegRd(XUSB_CFG_SSPX_CORE_CNT0, &val));
    val = FLD_SET_RF_NUM(XUSB_CFG_SSPX_CORE_CNT0_PING_TBRST, 0xd, val);
    CHECK_RC(RegWr(XUSB_CFG_SSPX_CORE_CNT0, val));

    // Clear the Enable Bit to prevent HW access the registers
    CHECK_RC(RegRd(XUSB_REG_PARAMS1, &val));
    val = FLD_SET_RF_NUM(XUSB_REG_PARAMS1_EN, 0, val);
    CHECK_RC(RegWr(XUSB_REG_PARAMS1, val));

    // read ErstMax
    CHECK_RC(RegRd(XUSB_REG_PARAM0, &val));
    m_ErstMax = RF_VAL(XUSB_REG_PARAM0_ERSTMAX, val);
    if ( 1 != m_ErstMax )
    {   // seems the only valid value is 1 since we have registers for two segments
        Printf(Tee::PriWarn, "ErstMax = %d \n", m_ErstMax);
    }

    // enable the Port Status Event
    CHECK_RC(RegRd(XUSB_REG_PARAMS1, &val));
    val = FLD_SET_RF_NUM(XUSB_REG_PARAMS1_PSEE, 1, val);
    CHECK_RC(RegWr(XUSB_REG_PARAMS1, val));

    // clear the system err bit just in case
    CHECK_RC(RegRd(XUSB_REG_PARAMS2, &val));
    val = FLD_SET_RF_NUM(XUSB_REG_PARAMS2_DEV_SYS_ERR, 1, val);
    CHECK_RC(RegWr(XUSB_REG_PARAMS2, val));

    // read in the Port Speed
    CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
    m_UsbSpeed = RF_VAL(XUSB_REG_PORTSC_PS, val);

    // setup default bufbuild parameters
    CHECK_RC(m_pBufBuild->SetAttr(16, 1, 65536, MEM_ADDRESS_32BIT, Memory::UC));
    CHECK_RC(SetBufParams(1, 65536, 1, 10, 0, 128, 0));

    LOG_EXT();
    return rc;
}

RC XusbController::UninitRegisters()
{
    LOG_ENT();
    // release the user-specified descriptors
    // this could be treated as HW properties, so put it in UninitRegisters()
    if (m_DevDescriptor.Address)
    {
        MemoryTracker::FreeBuffer(m_DevDescriptor.Address);
        m_DevDescriptor.Address = NULL;
    }
    if (m_BosDescriptor.Address)
    {
        MemoryTracker::FreeBuffer(m_BosDescriptor.Address);
        m_BosDescriptor.Address = NULL;
    }
    for ( UINT32 i = 0; i < m_vConfigDescriptor.size(); i++ )
    {
        if ( m_vConfigDescriptor[i].Address )
        {
            MemoryTracker::FreeBuffer((void *)m_vConfigDescriptor[i].Address);
        }
    }
    m_vConfigDescriptor.clear();
    for ( UINT32 i = 0; i < m_vStringDescriptor.size(); i++ )
    {
        if ( m_vStringDescriptor[i].Address )
        {
            MemoryTracker::FreeBuffer((void *)m_vStringDescriptor[i].Address);
        }
    }
    m_vStringDescriptor.clear();
    for ( UINT32 i = 0; i < m_vScsiCmdResponse.size(); i++ )
    {
        if ( m_vScsiCmdResponse[i].Address )
        {
            MemoryTracker::FreeBuffer((void *)m_vScsiCmdResponse[i].Address);
        }
    }
    m_vScsiCmdResponse.clear();
    LOG_EXT();
    return OK;
}

RC XusbController::InitDataStructures()
{
    LOG_ENT();
    RC rc ;

    UINT32 val;
    m_pDevmodeEpContexts = new DevModeEpContexts();
    if ( !m_pDevmodeEpContexts )
    {
        Printf(Tee::PriError, "Fail to allocate memory \n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    CHECK_RC(m_pDevmodeEpContexts->Init());

    PHYSADDR pEndPContext;
    CHECK_RC(m_pDevmodeEpContexts->GetBaseAddr(&pEndPContext));

    //Initialize the Endpoint context base address MMIO registers
    UINT32 tmp = pEndPContext;
    val = FLD_SET_RF_NUM(XUSB_REG_EPPR_LO_OFFSET, tmp>>(0?XUSB_REG_EPPR_LO_OFFSET), 0);
    CHECK_RC(RegWr(XUSB_REG_EPPR_LO, val));

    pEndPContext >>= 32;
    tmp = pEndPContext;
    val = FLD_SET_RF_NUM(XUSB_REG_EPPR_HI_OFFSET, tmp>>(0?XUSB_REG_EPPR_HI_OFFSET), 0);
    CHECK_RC(RegWr(XUSB_REG_EPPR_HI, val));

    //Init transfer ring for the default endpoint
    PHYSADDR pXferRing;
    CHECK_RC(InitXferRing(0, true, true, &pXferRing));
    CHECK_RC(m_pDevmodeEpContexts->BindXferRing(0, true, pXferRing));

    //Initialize event ring segment 0 and 1 (create one segment then append another)
    m_pEventRing = new EventRing(this, 0, 2);
    CHECK_RC(m_pEventRing->Init(m_EventRingSize));
    CHECK_RC(m_pEventRing->AppendTrbSeg(m_EventRingSize));

    // Get the base address of segments
    PHYSADDR pEventRingSeg0;
    PHYSADDR pEventRingSeg1;
    CHECK_RC(m_pEventRing->GetSegmentBase(0, &pEventRingSeg0));
    CHECK_RC(m_pEventRing->GetSegmentBase(1, &pEventRingSeg1));

    //Initialize the MMIO registers with the Event Ring parameters
    CHECK_RC(RegRd(XUSB_REG_ERSS, & val));
    val = FLD_SET_RF_NUM(XUSB_REG_ERSS_0, m_EventRingSize, val);
    val = FLD_SET_RF_NUM(XUSB_REG_ERSS_1, m_EventRingSize, val);
    CHECK_RC(RegWr(XUSB_REG_ERSS, val));

    // segment 0 and DeqPtr Lo/Hi and EnqPtr Lo/Hi
    CHECK_RC(RegRd(XUSB_REG_ERS0_BA_LO, & val));
    tmp = pEventRingSeg0;
    val = FLD_SET_RF_NUM(XUSB_REG_ERS0_BA_LO_OFFSET, tmp>>(0?XUSB_REG_ERS0_BA_LO_OFFSET), val);
    CHECK_RC(RegWr(XUSB_REG_ERS0_BA_LO, val));

    CHECK_RC(RegRd(XUSB_REG_DP_LO, & val));
    val = FLD_SET_RF_NUM(XUSB_REG_DP_LO_OFFSET, tmp>>(0?XUSB_REG_DP_LO_OFFSET), val);
    CHECK_RC(RegWr(XUSB_REG_DP_LO, val) );

    CHECK_RC(RegRd(XUSB_REG_EP_LO, & val));
    val = FLD_SET_RF_NUM(XUSB_REG_EP_LO_OFFSET, tmp>>(0?XUSB_REG_EP_LO_OFFSET), val);
    val = FLD_SET_RF_NUM(XUSB_REG_EP_LO_CYCLE_STATE, 1, val);
    val = FLD_SET_RF_NUM(XUSB_REG_EP_LO_SEGI, 0, val);
    CHECK_RC(RegWr(XUSB_REG_EP_LO, val));

    CHECK_RC(RegRd(XUSB_REG_ERS0_BA_HI, & val));
    pEventRingSeg0 >>= 32;
    tmp = pEventRingSeg0;
    val = FLD_SET_RF_NUM(XUSB_REG_ERS0_BA_HI_OFFSET, tmp>>(0?XUSB_REG_ERS0_BA_HI_OFFSET), val);
    CHECK_RC(RegWr(XUSB_REG_ERS0_BA_HI, val));

    CHECK_RC(RegRd(XUSB_REG_DP_HI, & val));
    val = FLD_SET_RF_NUM(XUSB_REG_DP_HI_OFFSET, tmp>>(0?XUSB_REG_DP_HI_OFFSET), val);
    CHECK_RC(RegWr(XUSB_REG_DP_HI, val));

    CHECK_RC(RegRd(XUSB_REG_EP_HI, & val));
    val = FLD_SET_RF_NUM(XUSB_REG_EP_HI_OFFSET, tmp>>(0?XUSB_REG_EP_HI_OFFSET), val);
    CHECK_RC(RegWr(XUSB_REG_EP_HI, val));

    // segment 1
    CHECK_RC(RegRd(XUSB_REG_ERS1_BA_LO, & val));
    tmp = pEventRingSeg1;
    val = FLD_SET_RF_NUM(XUSB_REG_ERS1_BA_LO_OFFSET, tmp>>(0?XUSB_REG_ERS1_BA_LO_OFFSET), val);
    CHECK_RC(RegWr(XUSB_REG_ERS1_BA_LO, val));

    CHECK_RC(RegRd(XUSB_REG_ERS1_BA_HI, & val));
    pEventRingSeg1 >>= 32;
    tmp = pEventRingSeg1;
    val = FLD_SET_RF_NUM(XUSB_REG_ERS1_BA_HI_OFFSET, tmp>>(0?XUSB_REG_ERS1_BA_HI_OFFSET), val);
    CHECK_RC(RegWr(XUSB_REG_ERS1_BA_HI, val));

    // setup the Endpoint Context for Control Endpoint
    CHECK_RC(m_pDevmodeEpContexts->InitEpContext(0, true,
                                                EP_TYPE_CONTROL,
                                                EP_STATE_RUNNING,
                                                0x200,
                                                true,
                                                0,0,0,0,3,0,
                                                false));

    //Set the Enable Bit
    CHECK_RC(RegRd(XUSB_REG_PARAMS1, &val));
    val = FLD_SET_RF_NUM(XUSB_REG_PARAMS1_EN, 1, val);
    CHECK_RC(RegWr(XUSB_REG_PARAMS1, val));

    vector<UINT64> vIndex;
    CHECK_RC(m_pBufBuild->CreateBufMgrs(&vIndex, 1, 0));    // don't alloc any MEM at this point
    m_bufIndex = vIndex[0];

    m_vDataPattern32.push_back(0xdeadbeef);
    LOG_EXT();
    return OK;
}

RC XusbController::UninitDataStructures()
{
    LOG_ENT();
    UINT32 val;

    m_pBufBuild->Clear(m_bufIndex);

    // Clear the Enable Bit to prevent HW access the registers
    RegRd(XUSB_REG_PARAMS1, &val);
    val = FLD_SET_RF_NUM(XUSB_REG_PARAMS1_EN, 0, val);
    RegWr(XUSB_REG_PARAMS1, val);

    // release data structures defined in InitiDataStructures()
    if ( m_pDevmodeEpContexts )
    {
        delete m_pDevmodeEpContexts;
        m_pDevmodeEpContexts=NULL;
    }

    if ( m_pEventRing )
    {
        delete m_pEventRing;
        m_pEventRing=NULL;
    }

    for ( UINT32 i = 0; i < m_vpRings.size(); i++ )
    {   // delete all the Rings
        if ( m_vpRings[i] )
        {
            delete m_vpRings[i];
            m_vpRings[i] = NULL;
        }
    }
    m_vpRings.clear();

    m_DeviceState = XUSB_DEVICE_STATE_DEFAULT;
    m_ConfigIndex = 0;

    LOG_EXT();
    return OK;
}

RC XusbController::InitXferRing(UINT08 EpNum, 
                                bool IsDataOut, 
                                bool IsEp0HasDir,
                                PHYSADDR * ppPhyBase)
{
    MASSERT(ppPhyBase);
    LOG_ENT();
    RC rc = OK;
    Printf(Tee::PriLow, 
           "Creating Xfer Ring for Ep %u, %s, IsEp0HasDir %s \n", 
           EpNum, IsDataOut?"Out":"In", IsEp0HasDir?"Y":"N");

    // create new Xfer Ring
    TransferRing * pXferRing = new TransferRing(0, EpNum, IsDataOut, 0, IsEp0HasDir);
    if ( !pXferRing )
    {
        Printf(Tee::PriError, "InitXferRing() Fail to allocate memory");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    rc = pXferRing->Init(m_XferRingSize);

    if ( OK != rc )
    {
        delete pXferRing;
    }
    else
    {
        CHECK_RC(pXferRing->GetBaseAddr(ppPhyBase));
        m_vpRings.push_back(pXferRing);
    }

    LOG_EXT();
    return rc;
}

RC XusbController::SetDeviceDescriptor(vector<UINT08> *pvData)
{
    RC rc;
    if ( !pvData )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    if ( 0 == pvData->size() )
    {   // user just want to delete the existing Descriptor
        if (m_DevDescriptor.Address)
        {
            MemoryTracker::FreeBuffer(m_DevDescriptor.Address);
            m_DevDescriptor.Address = NULL;
        }
        Printf(Tee::PriNormal, "Lwstomized Device Descriptor deleted \n");
        return OK;
    }
    if ((*pvData)[1] != UsbDescriptorDevice)
    {
        Printf(Tee::PriError, "Unexpected Descriptor Type %d, expected %d", (*pvData)[1], UsbDescriptorDevice);
        return RC::BAD_PARAMETER;
    }

    CHECK_RC( MemoryTracker::AllocBuffer(pvData->size(), &(m_DevDescriptor.Address), true, 32, Memory::UC) );
    for ( UINT32 i = 0; i < pvData->size(); i++ )
    {
        MEM_WR08((UINT08*)m_DevDescriptor.Address + i, (*pvData)[i]);
    }
    m_DevDescriptor.ByteSize = (UINT32)pvData->size();
    Printf(Tee::PriNormal, "Lwstomized Device Descriptor %u BYTE saved \n", (UINT32)pvData->size());
    return rc;
}

RC XusbController::SetBosDescriptor(vector<UINT08> *pvData)
{
    RC rc;
    if ( !pvData )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    if ( 0 == pvData->size() )
    {   // user just want to delete the existing Descriptor
        if (m_BosDescriptor.Address)
        {
            MemoryTracker::FreeBuffer(m_BosDescriptor.Address);
            m_BosDescriptor.Address = NULL;
        }
        Printf(Tee::PriNormal, "Lwstomized BOS Descriptor deleted \n");
        return OK;
    }
    // sanity check
    if ((*pvData)[1] != UsbDescriptorBOS)
    {
        Printf(Tee::PriError, "Unexpected Descriptor Type %d, expected %d", (*pvData)[1], UsbDescriptorBOS);
        return RC::BAD_PARAMETER;
    }
    UINT32 totalLen = ( (*pvData)[3] << 8 ) | (*pvData)[2];
    if (totalLen != pvData->size())
    {
        Printf(Tee::PriError, "Unexpected TotalLendth %d, expected %d", totalLen, (UINT32)pvData->size());
        return RC::BAD_PARAMETER;
    }

    CHECK_RC( MemoryTracker::AllocBuffer(pvData->size(), &(m_BosDescriptor.Address), true, 32, Memory::UC) );
    for ( UINT32 i = 0; i < pvData->size(); i++ )
    {
        MEM_WR08((UINT08*)m_BosDescriptor.Address + i, (*pvData)[i]);
    }
    m_BosDescriptor.ByteSize = (UINT32)pvData->size();
    Printf(Tee::PriNormal, "Lwstomized BOS Descriptor %u BYTE saved \n", (UINT32)pvData->size());
    return rc;
}

RC XusbController::SetConfigurationDescriptor(vector<UINT08> *pvData)
{
    RC rc ;
    UsbConfigDescriptor descCfg;

    if ( !pvData )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    if ( 0 == pvData->size() )
    {   // erase all
        for ( UINT32 i = 0; i < m_vConfigDescriptor.size(); i++ )
        {
            if (m_vConfigDescriptor[i].Address)
            {
                MemoryTracker::FreeBuffer(m_vConfigDescriptor[i].Address);
            }
        }
        m_vConfigDescriptor.clear();
        Printf(Tee::PriNormal, "All Lwstomized Config Descriptor deleted \n");
        return OK;
    }

    UINT08 * pDataIn;
    CHECK_RC( MemoryTracker::AllocBuffer(pvData->size(), (void**)&pDataIn, true, 32, Memory::UC) );
    for ( UINT32 i = 0; i < pvData->size(); i++ )
    {
        MEM_WR08(pDataIn + i, (*pvData)[i]);
    }
    // sanity check
    UINT08 dummyType, dummyLength;
    rc = UsbDevExt::GetDescriptorTypeLength(pDataIn, &dummyType, &dummyLength, UsbDescriptorConfig);
    if ( OK != rc )
    {
        Printf(Tee::PriError, "Invalid Type / Length for Config Descriptor");
        MemoryTracker::FreeBuffer(pDataIn);
        return RC::BAD_PARAMETER;
    }
    // more check
    CHECK_RC(UsbDevExt::MemToStruct(&descCfg, (UINT08*)pDataIn));
    if ( pvData->size() != descCfg.TotalLength )
    {
        Printf(Tee::PriError, "Total Length %d, != TotalLength field in Descriptor %d", (UINT32)pvData->size(), descCfg.TotalLength);
        MemoryTracker::FreeBuffer(pDataIn);
        return RC::BAD_PARAMETER;
    }
    UINT08 configValue = descCfg.ConfigValue;
    // find and delete the existing Config Descriptor with the same ConfigIndex
    for ( UINT32 i = 0; i < m_vConfigDescriptor.size(); i++ )
    {
        UsbConfigDescriptor descCfgTmp;
        CHECK_RC(UsbDevExt::MemToStruct(&descCfgTmp, (UINT08*)m_vConfigDescriptor[i].Address));
        if ( descCfgTmp.ConfigValue == configValue )
        {   // found and delete
            Printf(Tee::PriLow, " Delete the existing Descriptor with index %d\n", configValue);
            MemoryTracker::FreeBuffer(m_vConfigDescriptor[i].Address);
            m_vConfigDescriptor.erase(m_vConfigDescriptor.begin() + i);
            break;
        }
    }

    MemoryFragment::FRAGMENT myFrag;
    myFrag.Address = pDataIn;
    myFrag.ByteSize = pvData->size();
    m_vConfigDescriptor.push_back(myFrag);

    Printf(Tee::PriNormal, "Configuration Descriptor with ConfigIndex %d saved (%u BYTEs) \n",
           configValue, (UINT32)pvData->size());
    return rc;
}

RC XusbController::SetStringDescriptor(vector<UINT08> *pvData)
{
    RC rc ;

    if ( !pvData )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( 0 == pvData->size() )
    {   // erase all
        for ( UINT32 i = 0; i < m_vStringDescriptor.size(); i++ )
        {
            if (m_vStringDescriptor[i].Address)
            {
                MemoryTracker::FreeBuffer(m_vStringDescriptor[i].Address);
            }
        }
        m_vStringDescriptor.clear();
        Printf(Tee::PriNormal, "All Lwstomized STRING Descriptor deleted \n");
        return OK;
    }
    // sanity check
    if ((*pvData)[1] != UsbDescriptorString)
    {
        Printf(Tee::PriError, "Unexpected Descriptor Type %d, expected %d", (*pvData)[1], UsbDescriptorString);
        return RC::BAD_PARAMETER;
    }

    UINT08 * pDataIn;
    CHECK_RC( MemoryTracker::AllocBuffer(pvData->size(), (void**)&pDataIn, true, 32, Memory::UC) );
    for ( UINT32 i = 0; i < pvData->size(); i++ )
    {
        MEM_WR08(pDataIn + i, (*pvData)[i]);
    }

    MemoryFragment::FRAGMENT myFrag;
    myFrag.Address = pDataIn;
    myFrag.ByteSize = pvData->size();
    m_vStringDescriptor.push_back(myFrag);

    Printf(Tee::PriNormal, "STRING Descriptor saved (%u BYTEs) \n", (UINT32)pvData->size());
    return rc;
}

RC XusbController::EventHandler(XhciTrb* pEventTrb)
{
    LOG_ENT();
    RC rc;
    if ( !pEventTrb )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    UINT08 eventType;
    CHECK_RC(TrbHelper::GetTrbType(pEventTrb, &eventType, Tee::PriDebug, true));

    bool isHandled = false;
    switch ( eventType )
    {
        case XhciTrbTypeEvSetupEvent:
            Printf(Tee::PriLow, "SetupEvent Trb Received \n");
            CHECK_RC(HandlerSetupEvent(pEventTrb, &isHandled));
            break;

        case XhciTrbTypeEvPortStatusChg:
            Printf(Tee::PriNormal, "Port Status Change Event Trb Received \n");
            CHECK_RC(HandlerPortStatusChange());
            isHandled = true;
            break;

        default:
            Printf(Tee::PriWarn, "Event type %d doesn't has any handler yet \n", eventType);
            //TrbHelper::DumpTrbInfo(pEventTrb, true, Tee::PriNormal);
            break;
    }
    LOG_EXT();
    return OK;
}

RC XusbController::HandlerSetupEvent(XhciTrb* pEventTrb, bool* pIsHandled)
{
    LOG_ENT();
    RC rc;
    if ( !pEventTrb )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pIsHandled = true;

    UINT08 reqType;
    UINT08 req;
    UINT16 value;
    UINT16 index;
    UINT16 length;
    UINT16 ctrlSeqNum;
    CHECK_RC(TrbHelper::ParseSetupTrb(pEventTrb, &reqType, &req, &value, &index, &length, &ctrlSeqNum));
    if ( req == UsbSetAddress )    //SET_ADDRESS
    {
        if ( !value )
        {
            Printf(Tee::PriError, "Invalid SetAddress Request received with value equal to 0 ");
            return RC::BAD_PARAMETER;
        }
        Printf(Tee::PriNormal, "SetAddress received (address %d) \n", value);
        CHECK_RC(HandleSetAddress(value, ctrlSeqNum));
    }
    else if ( req == UsbSetFeature )    //SET_FEATURE
    {
        Printf(Tee::PriNormal, "SetFeature received (feature %d) \n", value);
        CHECK_RC(HandleSetFeature(value, reqType, index, ctrlSeqNum));
    }
    else if ( req == UsbSetConfiguration )     //SET_CONFIGURATION
    {
        if ( !value && (m_DeviceState != XUSB_DEVICE_STATE_CONFIGURED) )
        {
            Printf(Tee::PriError, "Invalid SetConfig Request received with value 0 ");
            return RC::BAD_PARAMETER;
        }
        Printf(Tee::PriNormal, "SetConfig received (%d) \n", value);
        CHECK_RC(HandleSetConfiguration(value, ctrlSeqNum));
    }
    else if ( req == UsbGetDescriptor )        //GET_DESCRIPTOR
    {
        if ( !value || !length )
        {
            Printf(Tee::PriError, "Invalid GetDescriptor Request Received with value %d and length %d ", value, length);
            return RC::BAD_PARAMETER;
        }
        Printf(Tee::PriNormal, "GetDescriptor received (0x%x) \n", value);
        CHECK_RC(HandleGetDescriptor(value,index,length, ctrlSeqNum));
    }
    else if ( req == UsbGetMaxLun )
    {   // Get Max Lun comes
        Printf(Tee::PriNormal, "GetMaxLun received \n");
        CHECK_RC(HandleGetMaxLun(length, ctrlSeqNum));
    }
    else if ( req == UsbClearFeature )
    {
        Printf(Tee::PriNormal, "ClearFeature received (feature %d) \n", value);
        CHECK_RC(HandleClearFeature(value, reqType, index, ctrlSeqNum));
    }
    else
    {
        *pIsHandled = false;
        Printf(Tee::PriWarn, "Not implemented request %d \n", req);
    }

    LOG_EXT();
    return OK;
}

RC XusbController::HandleSetAddress(UINT16 DeviceAddress, UINT16 CtrlSeqNum)
{
    LOG_ENT();
    Printf(Tee::PriLow,"(DeviceAddress %d, CtrlSeqNum 0x%x) \n", DeviceAddress, CtrlSeqNum);
    //Set device address in MMIO register field
    //set device state as addressed
    RC rc;
    // set Address to register
    UINT32 val;
    CHECK_RC(RegRd(XUSB_REG_PARAMS1, &val));
    val= FLD_SET_RF_NUM(XUSB_REG_PARAMS1_DEVADDR, DeviceAddress, val);
    CHECK_RC(RegWr(XUSB_REG_PARAMS1, val));

    CHECK_RC(SentStatus(CtrlSeqNum, false));

    m_DeviceState = XUSB_DEVICE_STATE_ADDRESSED;
    m_ConfigIndex = 0;
    LOG_EXT();
    return OK;
}

RC XusbController::HandleSetFeature(UINT16 FeatureSelector,
                                    UINT08 ReqType,
                                    UINT16 Index,
                                    UINT16 CtrlSeqNum)
{
    LOG_ENT();
    Printf(Tee::PriLow,"(SetFeature %d, ReqType 0x%x, Index 0x%x, CtrlSeqNum 0x%x) \n",
           FeatureSelector, ReqType, Index, CtrlSeqNum);
    RC rc;
    UINT32 val32;
    bool isEnTestMode = false;
    UINT16 testMode = 0;

    switch ( FeatureSelector )
    {
        case UsbEndpointHalt:
             Printf(Tee::PriNormal, "Endpoint Halt received \n");
             // Device mods write 1 to ep_halt.
             if ( 0x2 == ReqType )   // see also 9.4.9 of USB3.0 spec
             {      // retreive the EP number
                 UINT32 epNum = Index & 0xf;
                 bool isDataOut = ((Index & 0x80) == 0);
                 Printf(Tee::PriNormal, "  on Endpoint %d, Data %s \n", epNum, isDataOut?"Out":"In");
                 // see also 4.4 Halt an endpoint of USB3_SW_PG_DeviceMode.pdf

                 RegRd(XUSB_REG_PARAMS1, &val32);
                 if ( FLD_TEST_RF_NUM(XUSB_REG_PARAMS1_RUN, 0, val32) )
                 {
                     Printf(Tee::PriNormal, "Run bit not set, skip endpoint halt \n");
                     break;
                 }
                 RegRd(XUSB_REG_EPHALTR, &val32);
                 val32 |= 1<<(2*epNum + (isDataOut?0:1));
                 RegWr(XUSB_REG_EPHALTR, val32);
                 // polling for state change
                 rc = WaitRegMem(XUSB_REG_EPSCR, 1<<(2*epNum + (isDataOut?0:1)), 0, 2000);
                 RegRd(XUSB_REG_EPSCR, &val32);
                 if ( OK != rc )
                 {
                     Printf(Tee::PriError,
                            "Wait for HALT state change time out, StateChange = 0x%x \n",
                            val32);
                     return rc;
                 }
                 // clear the state change by writing 1
                 val32 &= ~(1<<(2*epNum + (isDataOut?0:1)));
                 RegWr(XUSB_REG_EPSCR, val32);
             }
             //todo: figure out how to turn off the halt
             break;
        //case UsbFunctionSuspend:
        //     Printf(Tee::PriWarn,"  FunctionSuspend is not handled yet \n");
        //     break;
        case UsbU1Enable:
             Printf(Tee::PriNormal,"U1Enable received \n");
             RegRd(XUSB_REG_PARAMS4, &val32);
             val32 = FLD_SET_RF_NUM(XUSB_REG_PARAMS4_SSU1E, 1, val32);
             RegWr(XUSB_REG_PARAMS4, val32);
             break;
        case UsbU2Enable:
             Printf(Tee::PriNormal,"U2Enable received \n");
             RegRd(XUSB_REG_PARAMS4, &val32);
             val32 = FLD_SET_RF_NUM(XUSB_REG_PARAMS4_SSU2E, 1, val32);
             RegWr(XUSB_REG_PARAMS4, val32);
             break;
        case UsbTestMode:
            if ( 0 == ReqType )
            {
                testMode = Index >> 8;
                Printf(Tee::PriNormal,"  Entering TestMode %d \n", testMode);
                isEnTestMode = true;
            }
            break;
        default:
             Printf(Tee::PriError,"Unkwnown feature 0x%d \n", FeatureSelector);
             return RC::BAD_PARAMETER;
    }

    CHECK_RC(SentStatus(CtrlSeqNum, false));
    if ( isEnTestMode )
    {
        RegWr(XUSB_DEV_XHCI_PORT_TM_0, testMode);
    }
    LOG_EXT();
    return OK;
}

RC XusbController::HandleClearFeature(UINT16 FeatureSelector,
                                      UINT08 ReqType,
                                      UINT16 Index,
                                      UINT16 CtrlSeqNum)
{
    LOG_ENT();
    Printf(Tee::PriLow,"(ClearFeature %d, ReqType 0x%x, Index 0x%x, CtrlSeqNum 0x%x) \n",
           FeatureSelector, ReqType, Index, CtrlSeqNum);
    RC rc;
    UINT32 val32;
    UINT32 epNum = 0;
    bool isDataOut = false;
    bool isToRingDoorbell = false;

    switch ( FeatureSelector )
    {
        case UsbEndpointHalt:
             Printf(Tee::PriNormal, "Endpoint Halt Clear received \n");
        /* per Karthik's email, we need to clear the sequence number for the halted EPs
        When clear_halt request received
        1. Set EP state to disabled
        2. Reload EP context
        3. Wait for reload bit to be cleared
        4. Clear halt register
        5. Make sure STCHG bit are set
        6. Clear STCHG bit for the endpoint
        7. Set EP state to running
        8. Set SeqNum = 0
        9. reload EP context
        10. wait for reload bit to be cleared
        11. send the status stage of clear halt request
        12. ring door bell for the endpoint.
        */
             if ( ReqType == 0x2 )   // see also 9.4.9 of USB3.0 spec
             {      // retreive the EP number
                 epNum = Index & 0xf;
                 isDataOut = ((Index & 0x80) == 0);
                 Printf(Tee::PriNormal, "  on Endpoint %d, Data %s \n", epNum, isDataOut?"Out":"In");
                 // see also 4.4 Halt an endpoint of USB3_SW_PG_DeviceMode.pdf

                 // setup Endpoint Contexts
                 CHECK_RC(m_pDevmodeEpContexts->SetEpState(epNum, isDataOut, EP_STATE_DISABLED));   // put EP to disable state
                 CHECK_RC(ReloadEpContext(epNum, isDataOut));

                 // clear halt register
                 RegRd(XUSB_REG_PARAMS1, &val32);
                 if ( FLD_TEST_RF_NUM(XUSB_REG_PARAMS1_RUN, 0, val32) )
                 {
                     Printf(Tee::PriNormal, "Run bit not set, skip endpoint halt \n");
                     break;
                 }
                 RegRd(XUSB_REG_EPHALTR, &val32);
                 val32 &= ~(1<<(2*epNum + (isDataOut?0:1)));
                 RegWr(XUSB_REG_EPHALTR, val32);
                 // polling for state change
                 rc = WaitRegMem(XUSB_REG_EPSCR, 1<<(2*epNum + (isDataOut?0:1)), 0, 2000);
                 RegRd(XUSB_REG_EPSCR, &val32);
                 if ( OK != rc )
                 {
                     Printf(Tee::PriError,
                            "Wait for HALT state change timeout, StateChange = 0x%x \n",
                            val32);
                     return rc;
                 }
                 // clear the state change by writing 1
                 val32 &= ~(1<<(2*epNum + (isDataOut?0:1)));
                 RegWr(XUSB_REG_EPSCR, val32);
                 // set EP state to running
                 CHECK_RC(m_pDevmodeEpContexts->SetEpState(epNum, isDataOut, EP_STATE_RUNNING));
                 CHECK_RC(m_pDevmodeEpContexts->SetSeqNum(epNum, isDataOut, 0));
                 CHECK_RC(ReloadEpContext(epNum, isDataOut));
                 //m_pDevmodeEpContexts->PrintInfo(Tee::PriNormal,  Tee::PriNormal);  // for debug
                 // ring doorbell later
                 isToRingDoorbell = true;
             }
             break;
        case UsbU1Enable:
             Printf(Tee::PriNormal,"U1Disable received \n");
             RegRd(XUSB_REG_PARAMS4, &val32);
             val32 = FLD_SET_RF_NUM(XUSB_REG_PARAMS4_SSU1E, 0, val32);
             RegWr(XUSB_REG_PARAMS4, val32);
             break;
        case UsbU2Enable:
             Printf(Tee::PriNormal,"U2Disable received \n");
             RegRd(XUSB_REG_PARAMS4, &val32);
             val32 = FLD_SET_RF_NUM(XUSB_REG_PARAMS4_SSU2E, 0, val32);
             RegWr(XUSB_REG_PARAMS4, val32);
             break;
        default:
             Printf(Tee::PriError,"Unkwnown feature 0x%d \n", FeatureSelector);
             return RC::BAD_PARAMETER;
    }

    CHECK_RC(SentStatus(CtrlSeqNum, false));
    if ( isToRingDoorbell )
    {
        CHECK_RC(RingDoorBell(epNum, isDataOut));
    }

    LOG_EXT();
    return OK;
}

RC XusbController::ReloadEpContext(UINT32 EpNum, bool IsDataOut)
{
    UINT32 endpointMask = 0;
    UINT08 dci;
    RC rc;

    CHECK_RC(DeviceContext::EpToDci(EpNum, IsDataOut, &dci, true));    // true is a dummy here
    endpointMask |= 1<<dci;
    CHECK_RC(ReloadEpContext(endpointMask));

    return OK;
}

RC XusbController::ReloadEpContext(UINT32 EpBitMask, UINT32 PauseBitMask)
{
    LOG_ENT();
    RC rc;
    UINT32 val32;

    if ( PauseBitMask & (~EpBitMask) )
    {
        Printf(Tee::PriError,
               "PauseBitMask 0x%x contains bit not in the EpBitMask 0x%x \n",
               PauseBitMask, EpBitMask);
        return RC::BAD_PARAMETER;
    }
    if ( !EpBitMask )
    {
        Printf(Tee::PriError,
               "ReloadEpContext(): Endpoint Mask = 0 \n");
        return RC::BAD_PARAMETER;
    }
    Printf(Tee::PriLow, "Set Load Context with 0x%08x\n", EpBitMask);
    CHECK_RC(RegWr(XUSB_REG_EPLCR, EpBitMask));

    // wait for HW to clear the bits
    rc = WaitRegMem(XUSB_REG_EPLCR, 0, EpBitMask, GetTimeout());
    if ( OK != rc )
    {
        CHECK_RC(RegRd(XUSB_REG_EPLCR, &val32));
        Printf(Tee::PriError,
               "Wait for HW to clear Load Context Endpoint Register timeout, Reg0x%x = 0x%x",
               XUSB_REG_EPLCR, val32);
        return RC::HW_STATUS_ERROR;
    }

    // Clear the corresponding pause bit
    RegRd(XUSB_REG_EPPAUSER, &val32);
    val32 &= ~EpBitMask;
    val32 |= PauseBitMask;
    Printf(Tee::PriLow, "Set EP Pause with 0x%08x \n", val32);
    CHECK_RC(RegWr(XUSB_REG_EPPAUSER, val32));

    LOG_EXT();
    return OK;
}

RC XusbController::HandleSetConfiguration(UINT08 Configuratiolwalue, UINT16 CtrlSeqNum)
{
    LOG_ENT();
    Printf(Tee::PriLow,"(CfgIndex %d) \n", Configuratiolwalue);
    if(m_InitState < INIT_DONE)
    {
        Printf(Tee::PriError, "Controller has not been fully initialized, call Init() first \n");
        return RC::WAS_NOT_INITIALIZED;
    }
    RC rc;
    UINT32 val;
    UINT32 pauseMaskBit = 0;

    if ( m_ConfigIndex )
    {   // this is not the first time receiving SetConfig
        // we need only to pause and un-pause EPs
        Printf(Tee::PriNormal, "SetConfiguration to %d (from %d)",
               Configuratiolwalue, m_ConfigIndex);

        if ( Configuratiolwalue == m_ConfigIndex )
        {
            Printf(Tee::PriNormal, " do nothing \n");
        }
        else
        {
            Printf(Tee::PriNormal, "\n");
            // scan the EP Info to find out which to Pause and which to un-pause
            for ( UINT32 i = 0; i < m_vEpInfo.size(); i++ )
            {
                if ( m_vEpInfo[i].ConfigIndex != Configuratiolwalue )
                {
                    UINT08 dci;
                    CHECK_RC(DeviceContext::EpToDci(m_vEpInfo[i].EpNum, m_vEpInfo[i].IsDataOut, &dci, true));
                    pauseMaskBit |= 1<<dci;
                }
                else
                {
                    // we need to reset all existing transfer rings when switching EP config
                    m_IsRstXferRing = true;
                    CHECK_RC( EpParserHooker( m_DevClass, &(m_vEpInfo[i]) ) );
                }
            }
            // pause and un-pause respectrive bits
            RegRd(XUSB_REG_EPPAUSER, &val);
            Printf(Tee::PriNormal,
                   "HandleSetConfiguration() Ep Pause = 0x%x, write 0x%x to it\n",
                   val, pauseMaskBit);
            CHECK_RC(RegWr(XUSB_REG_EPPAUSER, pauseMaskBit));
        }

        CHECK_RC(SentStatus(CtrlSeqNum, false));
        return OK;
    }

    Printf(Tee::PriLow, "Entering HandleSetConfiguration() \n");
    //set the Run bit in the MMIO register
    //set the Ep state change bits for all the valid endpoints
    //set device state to configured

    if ( 0 == Configuratiolwalue )
    {   // de-cfg
        CHECK_RC(RegRd(XUSB_REG_PARAMS1, &val));
        val = FLD_SET_RF_NUM(XUSB_REG_PARAMS1_RUN, 0, val);
        CHECK_RC(RegWr(XUSB_REG_PARAMS1, val));

        // make sure the EP status changes.
        Printf(Tee::PriLow, "Wait for EPSCR to set...\n");
        rc = WaitRegBitClearAnyMem(XUSB_REG_EPSCR, 0xffffffff, GetTimeout());
        if ( OK != rc )
        {
            Printf(Tee::PriWarn, "No Endpoint State Change bit set \n");
        }
        else
        {   // clear the bits to ack
            CHECK_RC(RegRd(XUSB_REG_EPSCR, &val));
            CHECK_RC(RegWr(XUSB_REG_EPSCR, val));
        }

        m_pDevmodeEpContexts->ClearOut();
        m_DeviceState = XUSB_DEVICE_STATE_ADDRESSED;
        m_ConfigIndex = 0;
        // after finish, send status
        CHECK_RC(SentStatus(CtrlSeqNum, false));
        return OK;
    }

    // now retrieve the Config Descriptor and setup the Endpoint contexts
    m_vEpInfo.clear();
    // clear all Transfer Rings expect for Xfer Ring of EP0
    if (  m_vpRings.size() )
    {
        for ( UINT32 i = 1; i < m_vpRings.size(); i++ ) // INDEX 0 is Xfer Ring for EP0, so start with 1
        {
            if ( m_vpRings[i] )
            {
                delete m_vpRings[i];
            }
        }
        TransferRing * pTmpXferRing = m_vpRings[0];
        m_vpRings.clear();
        m_vpRings.push_back(pTmpXferRing);
    }

    vector<EndpointInfo> vtmpEpInfo;
    UINT32 offset = 0;
    UINT08* pConfig = NULL;
    UINT32 totalLength = 0;
    UINT32 cfgIndex;
    bool indexFound = false;
    for ( cfgIndex = 0; cfgIndex < m_vConfigDescriptor.size(); cfgIndex++ )
    {
        UsbConfigDescriptor descCfgTmp;
        CHECK_RC(UsbDevExt::MemToStruct(&descCfgTmp, (UINT08*)m_vConfigDescriptor[cfgIndex].Address));
        pConfig = (UINT08*)m_vConfigDescriptor[cfgIndex].Address;
        totalLength = m_vConfigDescriptor[cfgIndex].ByteSize;
        if ( descCfgTmp.ConfigValue == Configuratiolwalue )
        {
            m_ConfigIndex = Configuratiolwalue;             // record it.
            indexFound = true;
        }
        // figure out the class of the device and save it
        // Find the first Interface Descriptor
        CHECK_RC( UsbDevExt::FindNextDescriptor(pConfig, totalLength,
                                                UsbDescriptorInterface,
                                                &offset) );
        UsbInterfaceDescriptor descIntf;
        CHECK_RC(UsbDevExt::MemToStruct(&descIntf, (UINT08*)m_vConfigDescriptor[cfgIndex].Address + offset));
        if ( cfgIndex == 0 )
        {   // lwrrently we don't support configs with different class definitions
            Printf(Tee::PriNormal,
                   "USB Class: 0x%x, SubClass 0x%x, Protocol 0x%x ",
                    descIntf.Class, descIntf.SubClass, descIntf.Protocol);
            m_DevClass = descIntf.Class;        // record it.
        }
        UINT32 numEndpoints = descIntf.NumEP;
        Printf(Tee::PriNormal, "Cfg %d, NumEPs %d \n", descCfgTmp.ConfigValue, numEndpoints);

        // parse the endpoint descriptors
        vtmpEpInfo.clear();
        CHECK_RC( UsbDevExt::FindNextDescriptor(pConfig, totalLength,
                                                UsbDescriptorEndpoint,
                                                &offset) );
        CHECK_RC(UsbDevExt::ParseEpDescriptors(pConfig + offset, numEndpoints,
                                               m_UsbVer, m_UsbSpeed,
                                               &vtmpEpInfo));
        // append to EpInfo and save ConfigIndex
        for( UINT32 i = 0; i < vtmpEpInfo.size(); i++ )
        {   // save the Config Index the EP belongs to
            vtmpEpInfo[i].ConfigIndex = descCfgTmp.ConfigValue;
            m_vEpInfo.push_back(vtmpEpInfo[i]);

            if ( descCfgTmp.ConfigValue != Configuratiolwalue )
            {   // record which endpoint should be paused
                UINT08 dci;
                CHECK_RC(DeviceContext::EpToDci(vtmpEpInfo[i].EpNum, vtmpEpInfo[i].IsDataOut, &dci, true));
                pauseMaskBit |= 1<<dci;
            }
            else
            {   // class specified hookers for target config / EPs
                CHECK_RC( EpParserHooker( m_DevClass, &(vtmpEpInfo[i]) ) );
            }
        }
    }   // goes to the next Config Descriptor
    if ( !indexFound )
    {
        Printf(Tee::PriError, "Configuration %d not found", Configuratiolwalue);
        return RC::BAD_PARAMETER;
    }

    UINT32 endpointMask = 0;
    for ( UINT32 i=0; i< m_vEpInfo.size(); i++ )
    {
        UINT08 dci;
        CHECK_RC(DeviceContext::EpToDci(m_vEpInfo[i].EpNum, m_vEpInfo[i].IsDataOut, &dci, true));    // true is a dummy here

        // setup the Transfer Rings and the Endpoint Contexts
        endpointMask |= 1<<dci;

        PHYSADDR pXferRing;
        CHECK_RC(InitXferRing(m_vEpInfo[i].EpNum, m_vEpInfo[i].IsDataOut, false, &pXferRing));
        CHECK_RC(m_pDevmodeEpContexts->BindXferRing(m_vEpInfo[i].EpNum, m_vEpInfo[i].IsDataOut, pXferRing));
        // setup Endpoint Contexts
        CHECK_RC(m_pDevmodeEpContexts->InitEpContext(m_vEpInfo[i].EpNum,
                                                     m_vEpInfo[i].IsDataOut,
                                                     m_vEpInfo[i].Type,
                                                     EP_STATE_RUNNING,
                                                     m_vEpInfo[i].Mps,
                                                     true,
                                                     m_vEpInfo[i].Interval,
                                                     m_vEpInfo[i].MaxBurst,
                                                     m_vEpInfo[i].MaxStreams,
                                                     m_vEpInfo[i].Mult));
    }

    CHECK_RC(ReloadEpContext(endpointMask, pauseMaskBit));

    // set the RUN bit
    CHECK_RC(RegRd(XUSB_REG_PARAMS1, &val));
    val = FLD_SET_RF_NUM(XUSB_REG_PARAMS1_RUN, 1, val);
    CHECK_RC(RegWr(XUSB_REG_PARAMS1, val));

    // after finish, send status
    CHECK_RC(SentStatus(CtrlSeqNum, false));

    m_DeviceState= XUSB_DEVICE_STATE_CONFIGURED;
    LOG_EXT();
//Cleanup:
    // todo: add cleanup when fail
    return OK;
}

// setup member variables based on EP info passed in
RC XusbController::EpParserHooker(UINT08 DevClass, EndpointInfo * pEpInfo)
{
    LOG_ENT();
    switch( DevClass )
    {
        case UsbClassMassStorage:
            if ( pEpInfo->Type == XhciEndpointTypeBulkOut )
            {
                m_BulkOutEp = pEpInfo->EpNum;
                Printf(Tee::PriLow, "  BulkEpOut set to %d \n", m_BulkOutEp);
            }
            if ( pEpInfo->Type == XhciEndpointTypeBulkIn )
            {
                m_BulkInEp = pEpInfo->EpNum;
                Printf(Tee::PriLow, "  BulkEpIn set to %d \n", m_BulkInEp);
            }
            break;
        case UsbConfigurableDev:
            Printf(Tee::PriNormal, "  Cfg Dev Ep%d, Data%s \n",
                   pEpInfo->EpNum, pEpInfo->IsDataOut?"Out":"In");
            Printf(Tee::PriLow, " Nothing to record for Configurable(ISO) device\n");
            break;
        default:
            Printf(Tee::PriWarn, "Device Class 0x%x is not supported yet \n", DevClass);
    }
    LOG_EXT();
    return OK;
}

//------------------------------------------------------------------------
RC XusbController::HandleGetDescriptor(UINT16 Value, UINT16 Index, UINT16 Length, UINT16 CtrlSeqNum)
{
    LOG_ENT();
    RC rc;
    void * pDataIn = NULL;
    UINT08 *stringDescBuf = NULL;
    UINT08 descIndex = Value & 0xff;
    UINT08 descType = Value >> 8;

    //set pDataIn to store the Descriptor and send that to the xferring trb packup
    if ( (UsbDescriptorType)descType == UsbDescriptorDevice )
    {
        if ( !m_DevDescriptor.Address )
        {
            Printf(Tee::PriError, "Device Descriptor was not set, call SetDeviceDescriptor() first");
            return RC::WAS_NOT_INITIALIZED;
        }
        pDataIn = m_DevDescriptor.Address;

        UINT08 dummyType, length;
        CHECK_RC(UsbDevExt::GetDescriptorTypeLength((UINT08*)pDataIn, &dummyType, &length));

        if ( Length > length )
        {   // use the smaller one
            Length = length;
        }
    }
    else if ( (UsbDescriptorType)descType == UsbDescriptorConfig )
    {
        if ( !m_vConfigDescriptor.size() )
        {
            Printf(Tee::PriError, "Configure Descriptor was not set, call SetConfigDescriptor() first");
            return RC::WAS_NOT_INITIALIZED;
        }
        UINT32 numCfgDesc = (UINT32)m_vConfigDescriptor.size();
        if ( descIndex >= numCfgDesc )
        {
            Printf(Tee::PriError, "Index overflow, valid [0-%d]", (UINT32)m_vConfigDescriptor.size() - 1);
            return RC::BAD_PARAMETER;
        }
        pDataIn = m_vConfigDescriptor[descIndex].Address;

        UsbConfigDescriptor descCfgTmp;
        CHECK_RC(UsbDevExt::MemToStruct(&descCfgTmp, (UINT08*)pDataIn));
        if ( Length > descCfgTmp.TotalLength )
        {   // use the smaller one to prevent memeory violation
            Length = descCfgTmp.TotalLength;
        }
        /*
        UINT32 index;
        bool isFound = false;
        for ( index = 0; index < m_vConfigDescriptor.size(); index++ )
        {
            if ( (((UsbConfigDescriptor*)m_vConfigDescriptor[index].Address)->ConfigValue) == (Value % 0xff) )
            {
                isFound = true;
                pDataIn = m_vConfigDescriptor[index].Address;
                break;
            }
        }
        if ( !isFound )
        {
            Printf(Tee::PriError, "Configure Descriptor %d not found", Index);
            return RC::BAD_PARAMETER;
        }
        */
    }
    else if ( (UsbDescriptorType)descType == UsbDescriptorString )
    {
        UINT32 numStringDesc = (UINT32)m_vStringDescriptor.size();
        if ( ( 0 == descIndex )
             || ( descIndex > numStringDesc ) )
        {
            Printf(Tee::PriLow, "Host is requesting String Index %d which doesn't exist. Return dummy string\n", descIndex);
            CHECK_RC( MemoryTracker::AllocBuffer(0xa , (void**)&stringDescBuf, true, 32, Memory::UC) );
            MEM_WR08(stringDescBuf+0, 0xa);
            MEM_WR08(stringDescBuf+1, 0x3);
            MEM_WR08(stringDescBuf+2, 0x55);
            MEM_WR08(stringDescBuf+3, 0x0);
            MEM_WR08(stringDescBuf+4, 0x53);
            MEM_WR08(stringDescBuf+5, 0x0);
            MEM_WR08(stringDescBuf+6, 0x42);
            MEM_WR08(stringDescBuf+7, 0x0);
            MEM_WR08(stringDescBuf+8, 0x30 + descIndex);
            MEM_WR08(stringDescBuf+9, 0x0);
            pDataIn = (void*)stringDescBuf;
            if ( Length > 0xa )
            {   // use the smaller one to prevent memeory violation
                Length = 0xa;
            }
        }
        else
        {
            pDataIn = m_vStringDescriptor[descIndex - 1].Address;
            if ( Length > MEM_RD08(pDataIn) )
            {   // use the smaller one to prevent memeory violation
                Length = MEM_RD08(pDataIn);
            }
        }
    }
    else if ( ( UsbDescriptorType)descType == UsbDescriptorBOS  )
    {
        if ( !m_BosDescriptor.Address )
        {
            Printf(Tee::PriError, "BOS Descriptor was not set, call SetBosDescriptor() first");
            return RC::WAS_NOT_INITIALIZED;
        }
        pDataIn = m_BosDescriptor.Address;

        // read the total length
        if ( Length > MEM_RD16((UINT08*)pDataIn + 2) )
        {   // use the smaller one to prevent memeory violation
            Length = MEM_RD16((UINT08*)pDataIn + 2);
        }
    }
    else
    {
        Printf(Tee::PriError, "Un-supported Descriptor Type %d", descType);
        return RC::BAD_PARAMETER;
    }
    rc = SentDataStatus(pDataIn, Length, CtrlSeqNum);

    if ( stringDescBuf )
    {   // todo: remove this and build default string in InitDataSture()
        MemoryTracker::FreeBuffer(stringDescBuf);
        stringDescBuf = NULL;
    }
    LOG_EXT();
    return rc;
}

RC XusbController::HandleGetMaxLun(UINT16 Length, UINT16 CtrlSeqNum)
{
    LOG_ENT();
    RC rc;
    void * pDataIn = NULL;

    CHECK_RC( MemoryTracker::AllocBuffer(Length , &pDataIn, true, 32, Memory::UC) );
    // always return 0 to this request
    Memory::Fill08(pDataIn, 0, Length);

    rc = SentDataStatus(pDataIn, Length, CtrlSeqNum);
    MemoryTracker::FreeBuffer(pDataIn);

    LOG_EXT();
    return rc;
}

RC XusbController::SentDataStatus(void* pData, UINT32 Length, UINT16 CtrlSeqNum)
{
    LOG_ENT();
    RC rc;

    TransferRing * pXferRing;
    CHECK_RC(FindXferRing(0, true, &pXferRing));
    CHECK_RC(pXferRing->InsertDataStateTrb(pData, Length, false, true));
    CHECK_RC(RingDoorBell(0, true, CtrlSeqNum));
    UINT32 completionCode;
    CHECK_RC(WaitXfer(0, true, GetTimeout(), &completionCode));
    if ( XhciTrbCmpCodeSuccess != completionCode )
    {
        Printf(Tee::PriError, "Data TRB failed with Completion Code %d", completionCode);
        return RC::HW_STATUS_ERROR;
    }

    CHECK_RC(SentStatus(CtrlSeqNum));

    LOG_EXT();
    return OK;
}

RC XusbController::SentStatus(UINT16 CtrlSeqNum, bool IsHostToDev)
{
    LOG_ENT();

    RC rc;
    TransferRing * pXferRing;
    CHECK_RC(FindXferRing(0, true, &pXferRing));
    CHECK_RC(pXferRing->InsertStastusTrb(IsHostToDev));

    CHECK_RC(RingDoorBell(0, true, CtrlSeqNum));
    UINT32 completionCode;
    CHECK_RC(WaitXfer(0, true, GetTimeout(), &completionCode));

    if ( XhciTrbCmpCodeSuccess != completionCode )
    {
        Printf(Tee::PriError, "Status TRB failed with Completion Code %d", completionCode);
        return RC::HW_STATUS_ERROR;
    }

    LOG_EXT();
    return OK;
}

RC XusbController::PortReset()
{   // Handle Port Reset and Warm Port Reset
    LOG_ENT();
    // Run bit has been cleared on PR 0->1
    //  make sure Port Enable bit is 1

    RC rc;
    UINT32 val;

    // clear the PRC
    CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
    CHECK_RC(RegWrPortSc(val, RF_NUM(XUSB_REG_PORTSC_PRC, 1)));

    rc = WaitRegMem(XUSB_REG_PORTSC, RF_SET(XUSB_REG_PORTSC_PED), 0, GetTimeout());
    if ( OK != rc )
    {
        RegRd(XUSB_REG_PORTSC, &val);
        Printf(Tee::PriError, "PortReset(): Wait for PortEnable timeout, Reg(0x%x) = 0x%08x \n",
               XUSB_REG_PORTSC, val);
        return RC::HW_STATUS_ERROR;
    }

    // init internal device state
    m_DeviceState = XUSB_DEVICE_STATE_DEFAULT;
    m_ConfigIndex = 0;

    // Clear Device Address
    CHECK_RC(RegRd(XUSB_REG_PARAMS1, &val));
    val = FLD_SET_RF_NUM(XUSB_REG_PARAMS1_DEVADDR, 0, val);
    CHECK_RC(RegWr(XUSB_REG_PARAMS1, val));

    // clear all Transfer Rings
    for ( UINT32 i = 0; i < m_vpRings.size(); i++ )
    {
        if ( m_vpRings[i] )
        {
            delete m_vpRings[i];
        }
    }
    m_vpRings.clear();

    // init the Endpoint Context for Control Endpoint to default state
    CHECK_RC(m_pDevmodeEpContexts->ClearOut());
    CHECK_RC(m_pDevmodeEpContexts->InitEpContext(0, true,
                                                EP_TYPE_CONTROL,
                                                EP_STATE_RUNNING,
                                                0x200,
                                                true,
                                                0,0,0,0,3,0,
                                                false));
    //Init transfer ring for the default control endpoint
    PHYSADDR pXferRing;
    CHECK_RC(InitXferRing(0, true, true, &pXferRing));
    CHECK_RC(m_pDevmodeEpContexts->BindXferRing(0, true, pXferRing));

    LOG_EXT();
    return OK;
}

RC XusbController::HandlerPortStatusChange()
{
    LOG_ENT();
    RC rc;

    UINT32 val;
    CHECK_RC(RegRd(XUSB_REG_PORTSC, & val));
    Printf(Tee::PriNormal,"Port Status Change Event received, PortSc = 0x%08x \n", val);

    if ( FLD_TEST_RF_NUM(XUSB_REG_PORTSC_WRC, 0x1, val) )
    {   // Port Reset Change is set
        Printf(Tee::PriNormal, "Warm Port Reset Change\n");

        CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
        CHECK_RC(RegWrPortSc(val, RF_NUM(XUSB_REG_PORTSC_WRC, 1)));

        CHECK_RC(PortReset());
    }
    if ( FLD_TEST_RF_NUM(XUSB_REG_PORTSC_PRC, 0x1, val) )
    {   // Port Reset Change is set
        Printf(Tee::PriNormal, "Port Reset Change\n");

        // read in the Port Speed
        CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
        m_UsbSpeed = RF_VAL(XUSB_REG_PORTSC_PS, val);
        Printf(Tee::PriNormal, " Port Speed %d \n", m_UsbSpeed);

        CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
        // val =  FLD_SET_RF_NUM(XUSB_REG_PORTSC_PRC, 1, val);
        CHECK_RC(RegWrPortSc(val, RF_NUM(XUSB_REG_PORTSC_PRC, 1)));
        CHECK_RC(PortReset());
    }
    if ( FLD_TEST_RF_NUM(XUSB_REG_PORTSC_CSC, 0x1, val) )
    {
        Printf(Tee::PriNormal, "Connect Status Change. \n");
        CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));

        if ( FLD_TEST_RF_NUM(XUSB_REG_PORTSC_CCS, 0x0, val) )
        {
            /*
            Printf(Tee::PriNormal, "Device disconnect, issuing Soft Reset... \n");
            CHECK_RC(RegRd(XUSB_REG_SOFTRST, &val));
            val = FLD_SET_RF_NUM(XUSB_REG_SOFTRST_RESET, 1, val);
            CHECK_RC(RegWr(XUSB_REG_SOFTRST, val));

            rc = WaitRegMem(XUSB_REG_SOFTRST, 0, RF_SET(XUSB_REG_SOFTRST_RESET), GetTimeout());
            if ( OK != rc )
            {
                RegRd(XUSB_REG_SOFTRST, &val);
                Printf(Tee::PriError, "Wait for HW to clear RESET bit timeout, Reg (0x%x) = 0x%08x \n", XUSB_REG_SOFTRST, val);
                return RC::HW_STATUS_ERROR;
            }
            */
            CHECK_RC(UninitDataStructures());
            CHECK_RC(InitDataStructures());
            // enable LSE and IE in CTRL register after reset
            CHECK_RC(RegRd(XUSB_REG_PARAMS1, &val));
            val =  FLD_SET_RF_NUM(XUSB_REG_PARAMS1_PSEE, 1, val);
            val =  FLD_SET_RF_NUM(XUSB_REG_PARAMS1_IE, 1, val);
            CHECK_RC(RegWr(XUSB_REG_PARAMS1, val));
            // call JS call back
            if ( m_pJsCbDisconn )
            {
                Printf(Tee::PriNormal, "  Calling the JS callback - JsCbDiscon ...\n");
                rc = JavaScriptPtr()->CallMethod(m_pJsCbDisconn);
            }
        }
        else
        {
            Printf(Tee::PriNormal, "Device Connected, do nothing... \n");
            if ( m_pJsCbConn )
            {
                Printf(Tee::PriNormal, "  Calling the JS callback - JsCbConn ...\n");
                rc = JavaScriptPtr()->CallMethod(m_pJsCbConn);
            }
        }
        // clear Halt LTSSM
        if (m_DevId == 0x0e17)
        {   // if T114
            Printf(Tee::PriLow, "Clear LTSSM @ PARAMS3... \n");
            CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
            val =  FLD_SET_RF_NUM(XUSB_REG_PORTSC_HALT_LTSSM, 0, val);
            CHECK_RC(RegWrPortSc(val, RF_NUM(XUSB_REG_PORTSC_CSC, 1)));
        }
        else
        {
            Printf(Tee::PriLow, "Clear LTSSM @ PORTHALT... \n");
            CHECK_RC(RegRd(XUSB_REG_PORTHALT, &val));
            val =  FLD_SET_RF_NUM(XUSB_REG_PORTHALT_HALT_LTSSM, 0, val);
            CHECK_RC(RegWrPortHalt(val));
            // clear the CSC
            CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
            CHECK_RC(RegWrPortSc(val, RF_NUM(XUSB_REG_PORTSC_CSC, 1)));
        }
    }
    if ( FLD_TEST_RF_NUM(XUSB_REG_PORTSC_PLSC, 0x1, val) )
    {
        Printf(Tee::PriNormal, "Port Link Status Change \n");
        // Shriganesh Giri : Read the portsc register and if we are in resume ,
        // write directed_u0 to take link to U0.
        /*
        if ( FLD_TEST_RF_NUM(XUSB_REG_PORTSC_PLS, 0xf, val) )
        {
            UINT32 retryTimes;
            SetRetry(0, &retryTimes);
            if (retryTimes)
            {
                Printf(Tee::PriNormal, "  SetRetry() called, skip U0 transition \n");
            }
            else
            {
                Printf(Tee::PriNormal, "  Link is in the Resume State, write to U0 \n");
                CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
                val = FLD_SET_RF_NUM(XUSB_REG_PORTSC_LWS, 0x1, val);
                val = FLD_SET_RF_NUM(XUSB_REG_PORTSC_PLS, 0x0, val);
                CHECK_RC(RegWrPortSc(val));
            }

            if ( m_pJsCbU3U0 )
            {
                Printf(Tee::PriNormal, "  Calling the JS callback - JsCbU3U0 ...\n");
                rc = JavaScriptPtr()->CallMethod(m_pJsCbU3U0);
            }
        }*/
        if ( FLD_TEST_RF_NUM(XUSB_REG_PORTSC_PLS, 0x0, val) )   // U0 State
        {
            if ( m_pJsCbU3U0 )
            {
                Printf(Tee::PriNormal, "  Calling the JS callback - JsCbU3U0 ...\n");
                rc = JavaScriptPtr()->CallMethod(m_pJsCbU3U0);
            }
        }
        CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
        // val =  FLD_SET_RF_NUM(XUSB_REG_PORTSC_PLSC, 1, val);
        CHECK_RC(RegWrPortSc(val, RF_NUM(XUSB_REG_PORTSC_PLSC, 1)));
    }

    if (m_DevId == 0x0e17)
    {
        CHECK_RC(RegRd(XUSB_REG_PORTSC, & val));
        if ( FLD_TEST_RF_NUM(XUSB_REG_PORTSC_STCHG_REQ, 0x1, val) )
        {   // Details can be found in section 8 of Device Mode IAS.
            Printf(Tee::PriNormal, "STCHG_REQ generated, clear STCHG_REQ and LTSSM \n");
            // Clear STCHG_REQ ( this bit is write 1 to clear)
            CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
            // val =  FLD_SET_RF_NUM(XUSB_REG_PORTSC_STCHG_REQ, 1, val);
            CHECK_RC(RegWrPortSc(val, RF_NUM(XUSB_REG_PORTSC_STCHG_REQ, 1)));

            // clear Halt LTSSM (this bit is write 0 to clear)
            CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
            val =  FLD_SET_RF_NUM(XUSB_REG_PORTSC_HALT_LTSSM, 0, val);
            CHECK_RC(RegWrPortSc(val));
        }
    }
    else
    {
        CHECK_RC(RegRd(XUSB_REG_PORTHALT, & val));
        if ( FLD_TEST_RF_NUM(XUSB_REG_PORTHALT_STCHG_REQ, 0x1, val) )
        {
            Printf(Tee::PriNormal, "STCHG_REQ generated, clear STCHG_REQ and LTSSM \n");
            CHECK_RC(RegRd(XUSB_REG_PORTHALT, &val));
            CHECK_RC(RegWrPortHalt(val, RF_NUM(XUSB_REG_PORTHALT_STCHG_REQ, 1)));

            CHECK_RC(RegRd(XUSB_REG_PORTHALT, &val));
            val =  FLD_SET_RF_NUM(XUSB_REG_PORTHALT_HALT_LTSSM, 0, val);
            CHECK_RC(RegWrPortHalt(val));
        }
    }

    if ( FLD_TEST_RF_NUM(XUSB_REG_PORTSC_PCEC, 0x1, val) )
    {   // do nothing for now
        Printf(Tee::PriNormal, "Port Config Error Change\n");

        CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
        // val =  FLD_SET_RF_NUM(XUSB_REG_PORTSC_PCEC, 1, val);
        CHECK_RC(RegWrPortSc(val, RF_NUM(XUSB_REG_PORTSC_PCEC, 1)));
    }

    LOG_EXT();
    return OK;
}

RC XusbController::RingDoorbells()
{
    RC rc;
    UINT08 slotIdDummy;
    UINT08 epNum;
    bool isDataOut;
    bool isEmpty;
    for ( UINT32 i = 0; i < m_vpRings.size(); i++ )
    {
        CHECK_RC(m_vpRings[i]->IsEmpty(&isEmpty));
        if ( !isEmpty )
        {
            CHECK_RC(m_vpRings[i]->GetHost(&slotIdDummy, &epNum, &isDataOut));
            CHECK_RC(RingDoorBell(epNum,  isDataOut));
            Printf(Tee::PriLow, "RingDoorbells() - Ring doorbell for EP %d, Data %s\n", epNum, isDataOut?"Out":"In");
        }
    }
    return OK;
}

RC XusbController::RegWrPortSc(UINT32 Val, UINT32 WrClearBits /*= 0*/)
{
    UINT32 bitMask;
    if (m_DevId == 0x0e17)
    {   // if T114
        bitMask = ( RF_SHIFTMASK(XUSB_REG_PORTSC_HALT_REJECT)
                    |RF_SHIFTMASK(XUSB_REG_PORTSC_CSC)
                    |RF_SHIFTMASK(XUSB_REG_PORTSC_WRC)
                    |RF_SHIFTMASK(XUSB_REG_PORTSC_STCHG_REQ)
                    |RF_SHIFTMASK(XUSB_REG_PORTSC_PRC)
                    |RF_SHIFTMASK(XUSB_REG_PORTSC_PLSC)
                    |RF_SHIFTMASK(XUSB_REG_PORTSC_PCEC)
                   );
    }
    else
    {   // T124 and above
        bitMask = ( RF_SHIFTMASK(XUSB_REG_PORTSC_CSC)
                    |RF_SHIFTMASK(XUSB_REG_PORTSC_WRC)
                    |RF_SHIFTMASK(XUSB_REG_PORTSC_PRC)
                    |RF_SHIFTMASK(XUSB_REG_PORTSC_PLSC)
                    |RF_SHIFTMASK(XUSB_REG_PORTSC_PCEC)
                   );
    }
    Val &= ~bitMask;
    WrClearBits &= bitMask;
    Val |= WrClearBits;
    // handle the PED writing
    // MODS should retrieve the state of port and determine the value to write based on that
    if ( FLD_TEST_RF_NUM(XUSB_REG_PORTSC_PLS, XUSB_REG_PORTSC_PLS_VAL_DIS, Val)
         && FLD_TEST_RF_NUM(XUSB_REG_PORTSC_PED, 0, Val) )
    {   // write 0 to PED only when port is in disabled state and PED is already 0
        Val = FLD_SET_RF_NUM(XUSB_REG_PORTSC_PED, 0, Val);
    }
    else
    {   // set PED to 1
        Val = FLD_SET_RF_NUM(XUSB_REG_PORTSC_PED, 1, Val);
    }
    Printf(Tee::PriDebug, "RegWrPortSc() 0x%08x --> PortSc,", Val);
    RegWr(XUSB_REG_PORTSC, Val);
    UINT32 val;
    RegRd(XUSB_REG_PORTSC, &val);
    Printf(Tee::PriDebug, " on Read 0x%08x \n", val);
    return OK;
}

RC XusbController::RegWrPortHalt(UINT32 Val, UINT32 WrClearBits /*= 0*/)
{
    RC rc;
    UINT32 bitMask;
    if (m_DevId == 0x0e17)
    {   // if T114
        return OK;
    }
    else
    {   // T124 and above
        bitMask = ( RF_SHIFTMASK(XUSB_REG_PORTHALT_HALT_REJECT)
                    |RF_SHIFTMASK(XUSB_REG_PORTHALT_STCHG_REQ)
                  );
    }
    Val &= ~bitMask;
    WrClearBits &= bitMask;
    Val |= WrClearBits;

    Printf(Tee::PriLow, "  0x%08x --> PortHalt", Val);
    CHECK_RC(RegWr(XUSB_REG_PORTHALT, Val));
    return OK;
}

RC XusbController::DevCfg(UINT32 SecToRun /* = 0*/)
{
    LOG_ENT();
    if(m_InitState < INIT_DONE)
    {
        Printf(Tee::PriError, "Controller has not been fully initialized, call Init() first \n");
        return RC::WAS_NOT_INITIALIZED;
    }

    // clear all change bits in port SC
    UINT32 val;
    RegRd(XUSB_REG_PORTSC, &val);
    RegWrPortSc(val, ~0);

    XhciTrb eventTrb;
    RC rc;

    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole = consoleCtx.AcquireRealConsole(true);
    
    if (SecToRun) 
    {   // set the finish time
        m_TimeToFinish = Platform::GetTimeMS() + SecToRun * 1000;
    }

    Printf(Tee::PriNormal, "DevCfg entered, press any key to abort ... \n");
    while ( 1 ) //m_DeviceState!= XUSB_DEVICE_STATE_CONFIGURED
    {
        bool isNewEvent = m_pEventRing->GetNewEvent(&eventTrb, 500);

        if ( isNewEvent )
        {
            Printf(Tee::PriNormal, "  Event received .. ");
            TrbHelper::DumpTrbInfo(&eventTrb, true, Tee::PriNormal);
            CHECK_RC(EventHandler(&eventTrb));
        }
        if ( pConsole->KeyboardHit() )
        {
            if (Console::VKC_LOWER_P == pConsole->GetKey()) 
            {   // print debug information
                PrintAll();
            }
            else
            {
                Printf(Tee::PriNormal, "User abort.\n");
                break;
            }
        }

        if ( m_DeviceState == XUSB_DEVICE_STATE_CONFIGURED )
        {
            Printf(Tee::PriLow, "Entering class engine... \n");
            switch(m_DevClass)
            {
                case  UsbClassMassStorage:
                    Printf(Tee::PriNormal, "Launching Mass Storage Engine \n");
                    CHECK_RC(EngineBulk());
                    break;
                case  UsbConfigurableDev:
                    Printf(Tee::PriNormal, "Launching ISOCH Engine \n");
                    CHECK_RC(EngineIsoch());
                    break;
                default:
                    Printf(Tee::PriWarn, "un-supported class 0x%x\n", m_DevClass);
                    // do nothing
                    break;
            }
        }

        if (SecToRun 
            && (Platform::GetTimeMS() > m_TimeToFinish))
        {
            Printf(Tee::PriNormal, "Running time %d Second is up, exiting... \n", SecToRun);
            m_TimeToFinish = 0;
            break;
        }
    }   //while ( m_DeviceState!= XUSB_DEVICE_STATE_CONFIGURED )
    Printf(Tee::PriLow, "Exiting event engine... \n");
    return OK;
}

RC XusbController::EngineIsoch()
{   // see also bug 836724
    // This engine deals with two major transfer catagories :
    // 1. vendor defined control transfer
    // 2. data buffer transfer with information embedded
    // the whole loop is like (press any key to abort...)
    // 1. Wait for Setup Event ( command from host )
    // 2. Parse the control message (Start_transfer/Stop_transfer/Query_statistics/Reset_statistics)
    // 3. Do the operation asked by Host
    // In each TD transfered on none-ep0, we need to embedded following data
    // CRC / TD sequence number / Target frame ID / SIA / data_length
    //
    // for Out:
    // Wait for host command, keep scheduling TD in OUT EP.
    // until # of TD has finished or Stop command on EP0
    //
    // for In:
    // Wait for host command, keep scheduling TD in IN EP.
    // until # of TD has finished or Stop command on EP0
   LOG_ENT();
   RC rc;
   CHECK_RC(m_pEventRing->FlushRing());

   // data for statistics
   // # of TDs scheduled/received with zero length
   UINT32 numTdNoData   = 0;
   //# of TDs transferred with correct CRC and uframe.
   UINT32 numCorrectTd  = 0;
   // # of TDs transferred with correct CRC and but wrong uframe.
   UINT32 numBaduFrameTd= 0;
   // # of TDs transferred with bad CRC.
   UINT32 numBadCrcTd   = 0;

   UINT32 seqNumTd  = 0;
   UINT32 expNumTd  = ~0;
   UINT08 epDci     = 0;
   bool isSia       = true;
   UINT32 frameId   = 0;

   // target EP
   UINT08 endpoint  = 0;
   bool isDataOut   = true;

   MemoryFragment::FRAGMENT myFrag;
   myFrag.ByteSize  = m_IsoTdSize;
   MemoryFragment::FRAGLIST myFrags;

   bool isAddTd     = false;
   bool hasKey      = false;
   ConsoleManager::ConsoleContext consoleCtx;
   Console *pConsole = consoleCtx.AcquireRealConsole(true);

   UINT32 xferedTdCount = 0;
   do
   {
       XhciTrb eventTrb;
       bool isNewEvent = m_pEventRing->GetNewEvent(&eventTrb, 500);
       if ( isNewEvent )
       {
           UINT08 eventType;
           CHECK_RC(TrbHelper::GetTrbType(&eventTrb, &eventType, Tee::PriDebug, true));
           if ( eventType == XhciTrbTypeEvSetupEvent )
           {
               Printf(Tee::PriNormal, "Setup Event received ... ");
               TrbHelper::DumpTrbInfo(&eventTrb, true, Tee::PriNormal);

               UINT08 reqType, req;
               UINT16 value, index, length, ctrlSeqNum;
               CHECK_RC(TrbHelper::ParseSetupTrb(&eventTrb, &reqType, &req, &value, &index, &length, &ctrlSeqNum));
               switch( req )
               {
                   case UsbIsochStart:
                       //Start_transfer (EP#, target frameID, SIA, # of TDs)
                       epDci = value & 0xff;
                       isSia = value >> 8;
                       frameId = index;
                       expNumTd = length;
                       Printf(Tee::PriNormal,
                              "  StartTransfer(EpDci %d, SIA %d, FrameId 0x%x, NumTd %d) \n",
                              epDci, isSia?1:0, frameId, expNumTd);
                       CHECK_RC(SentStatus(ctrlSeqNum, false));

                       // allocate 5 TD in advance
                       Printf(Tee::PriLow, "  Allocate 5 TDs in advance\n");
                       for ( UINT32 i = 0; i < 5; i ++ )
                       {
                           CHECK_RC( MemoryTracker::AllocBufferAligned(myFrag.ByteSize, &(myFrag.Address), 0x10000, 32, Memory::UC) );
                           // init the data pattern
                           Memory::Fill32(myFrag.Address, seqNumTd++, myFrag.ByteSize / 4);
                           // push to global fraglist
                           m_vIsochBuffers.push_back(myFrag);
                           myFrags.clear();
                           myFrags.push_back(myFrag);
                           // get EpNum and DIR
                           CHECK_RC(DeviceContext::DciToEp(epDci, &endpoint, &isDataOut));
                           // check if the EP is valid
                           EndPContext epCntxDummy;
                           m_pDevmodeEpContexts->GetEpRawData(endpoint, isDataOut, &epCntxDummy);
                           UINT32 content = epCntxDummy.DW0 | epCntxDummy.DW1
                                            | epCntxDummy.DW2 | epCntxDummy.DW3;
                           if ( 0 == content )
                           {
                               Printf(Tee::PriError, " Endpoint %d, Data%s not setup(empty entry), call PrintContext() to for detail", endpoint, isDataOut?"Out":"In");
                               return RC::BAD_PARAMETER;
                           }
                           CHECK_RC( SetupTd(endpoint, isDataOut, &myFrags, true, false, 0, isSia, frameId) );
                       }
                       Printf(Tee::PriLow, "  Doorbell ring for Ep %d, Data%s\n", endpoint, isDataOut?"Out":"In");
                       CHECK_RC(RingDoorBell(endpoint, isDataOut));
                       isAddTd = true;
                       break;
                   case UsbIsochStop:
                       //Start_transfer (EP#, target frameID, SIA, # of TDs)
                       Printf(Tee::PriNormal,
                              "  StopTransfer(EpDci %d, SIA %d, FrameId 0x%x) \n",
                              value & 0xff, value >> 8, index);
                       isAddTd = false;
                       CHECK_RC(SentStatus(ctrlSeqNum, false));
                       break;
                   case UsbIsochQuery:
                       //Start_transfer (EP#, target frameID, SIA, # of TDs)
                       Printf(Tee::PriNormal,
                              "  QueryStatistic(EpDci %d) \n",
                              value & 0xff);

                       void * pTmpBuf;
                       CHECK_RC( MemoryTracker::AllocBuffer(16, &pTmpBuf, true, 32, Memory::UC) );
                       MEM_WR32((UINT32*)pTmpBuf + 0, numTdNoData);
                       MEM_WR32((UINT32*)pTmpBuf + 1, numCorrectTd);
                       MEM_WR32((UINT32*)pTmpBuf + 2, numBaduFrameTd);
                       MEM_WR32((UINT32*)pTmpBuf + 3, numBadCrcTd);

                       SentDataStatus(pTmpBuf, 16, ctrlSeqNum);
                       MemoryTracker::FreeBuffer(pTmpBuf);
                       break;
                   case UsbIsochReset:
                       //Start_transfer (EP#, target frameID, SIA, # of TDs)
                       Printf(Tee::PriNormal,
                              "  ResetStatistic(EpDci %d) \n",
                              value & 0xff);
                       CHECK_RC(SentStatus(ctrlSeqNum, false));
                       numTdNoData = 0;
                       numCorrectTd = 0;
                       numBaduFrameTd = 0;
                       numBadCrcTd = 0;
                       break;
                   default:
                       Printf(Tee::PriWarn, "Un-supported Command for Isoch \n");
                       return OK;
               }
           }
           else if ( eventType == XhciTrbTypeEvTransfer )
           {
               Printf(Tee::PriLow, " Transfer Event Rvced\n");
               // first recollect finished buffers and generate statistics
               UINT08 compCode;
               PHYSADDR phyCompTrb;
               CHECK_RC(TrbHelper::ParseXferEvent(&eventTrb, &compCode, &phyCompTrb));
               if ( XhciTrbCmpCodeSuccess != compCode )
               {
                   Printf(Tee::PriWarn, "Completion Code %d \n", compCode);
               }
               xferedTdCount++;

               // parse the finished TRB to get the buffer virtual address
               if ( phyCompTrb )
               {
                   XhciTrb * pXferTrb = (XhciTrb *)MemoryTracker::PhysicalToVirtual(phyCompTrb);
                   PHYSADDR phyBuffer;
                   CHECK_RC(TrbHelper::ParseNormalTrb(pXferTrb, &phyBuffer));
                   void * pDataBuf = MemoryTracker::PhysicalToVirtual(phyBuffer);
                   // now search the allocated list for the buffer.
                   // staticstic all older buffers and release all obseleted boffers
                   UINT32 bufIndex = 0;
                   bool isFound = false;
                   for ( ; bufIndex < m_vIsochBuffers.size(); bufIndex++ )
                   {
                       if (m_vIsochBuffers[bufIndex].Address == pDataBuf)
                       {
                           isFound = true;
                           break;
                       }
                   }
                   if ( !isFound )
                   {
                       Printf(Tee::PriError, "Buffer not found in allocate list");
                       return RC::SOFTWARE_ERROR;
                   }
                   for ( UINT32 i = 0; i <= bufIndex; i ++ )
                   { // todo: chech CRC
                     // release buffers
                       MemoryTracker::FreeBuffer(m_vIsochBuffers[0].Address);
                     // update the fraglist
                       m_vIsochBuffers.erase(m_vIsochBuffers.begin());
                   }
                   Printf(Tee::PriLow, "  %d TD retired\n", bufIndex + 1);
               }

               if ( isAddTd )
               {   // add TDs to specified Xfer Ring
                   // we only add new TD when get xfer Event for every Service Interval,
                   // we add a TD with Max payload (48KB)
                   // recollect buffer with retired TDs
                   // allocate buffers at 64KB alignement with size of 64K for each TD
                   Printf(Tee::PriLow, "  Add new TD\n");
                   CHECK_RC( MemoryTracker::AllocBufferAligned(myFrag.ByteSize, &(myFrag.Address), 0x10000, 32, Memory::UC) );
                   // init the data pattern
                   Memory::Fill32(myFrag.Address, seqNumTd++, myFrag.ByteSize / 4);
                   // push to global fraglist
                   m_vIsochBuffers.push_back(myFrag);
                   myFrags.clear();
                   myFrags.push_back(myFrag);
                   CHECK_RC( SetupTd(endpoint, isDataOut, &myFrags, true, true, 0, isSia, frameId) );
               }
           }
           else
           {
               Printf(Tee::PriError, "Unexpected Event Type %d", eventType);
               return RC::BAD_PARAMETER;
           }
       }

       if ( pConsole->KeyboardHit() )
       {
           hasKey = true;
           pConsole->GetKey();
       }
   } while ( !hasKey && (seqNumTd <= expNumTd) );

   if ( hasKey )
   {
       Printf(Tee::PriNormal, "TD xfered %d\n", xferedTdCount);
   }
   LOG_EXT();
   return OK;
}

RC XusbController::EngineBulk()
{
    LOG_ENT();

    RC rc = OK;
    MemoryFragment::FRAGLIST cbwFragList;
    MemoryFragment::FRAGLIST cswFragList;
    MemoryFragment::FRAGMENT myFrag;

    // setup a Data TRB with 31 Bytes for the coming CBW
    UINT08 myEpId;
    CHECK_RC(DeviceContext::EpToDci(m_BulkOutEp, true, &myEpId, true));

    void *pBufCbw;
    CHECK_RC( MemoryTracker::AllocBuffer(31 , &pBufCbw, true, 32, Memory::UC) );
    void *pBufCsw;
    CHECK_RC( MemoryTracker::AllocBuffer(13 , &pBufCsw, true, 32, Memory::UC) );

    bool hasKey = false;
    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole = consoleCtx.AcquireRealConsole(true);
    
    UINT08 epNumToAddCbw = 0;
    bool isToAddCbwTd = true;
    CHECK_RC_CLEANUP(m_pEventRing->FlushRing());
    Printf(Tee::PriNormal, "CBW standby mode entered ... Press any key to abort\n");
    do
    {
        if ( isToAddCbwTd )
        {
            // init, setup a TD for CBW
            Memory::Fill08(pBufCbw, 0, 31);
            cbwFragList.clear();
            cswFragList.clear();

            // setup fragment for CBW and CSW
            myFrag.Address = pBufCbw;
            myFrag.ByteSize = 31;
            cbwFragList.push_back(myFrag);

            myFrag.Address = pBufCsw;
            myFrag.ByteSize = 13;
            cswFragList.push_back(myFrag);

            // add TRB for CBW to out endpoint
            Printf(Tee::PriNormal, "  Setup TRB on Endpoint %d, Data out for CBW\n", m_BulkOutEp );
            CHECK_RC_CLEANUP(SetupTd(m_BulkOutEp, true, &cbwFragList, false, true));
            epNumToAddCbw = m_BulkOutEp;
            isToAddCbwTd = false;
        }

        XhciTrb eventTrb;
        bool isNewEvent = m_pEventRing->GetNewEvent(&eventTrb, 500);
        if ( isNewEvent )
        {
            Printf(Tee::PriNormal, "Event received ... ");
            // for debug!!!
            TrbHelper::DumpTrbInfo(&eventTrb, true, Tee::PriNormal);
            UINT08 eventType;
            UINT08 compCode;
            UINT08 epId;
            CHECK_RC_CLEANUP(TrbHelper::GetTrbType(&eventTrb, &eventType, Tee::PriDebug, true));
            if ( eventType == XhciTrbTypeEvTransfer )
            {
                CHECK_RC_CLEANUP(TrbHelper::ParseXferEvent(&eventTrb, &compCode, NULL, NULL, NULL, NULL, &epId));
                if ( epId == myEpId )
                {   // we get a transfer event and target to this endpoint, call handler
                    if ( compCode == XhciTrbCmpCodeSuccess  )
                    {
                        Printf(Tee::PriNormal, " Transfer Event for DCI %d received  \n", epId);
                        isToAddCbwTd = true;
                        rc = BulkCmdHandler((UINT08*)pBufCbw);
                        if ( m_DeviceState!= XUSB_DEVICE_STATE_CONFIGURED )
                        {   // when wait for transfer, device is reset
                            goto Cleanup;
                        }

                        // setup CSW and send it to host
                        MEM_WR32(pBufCsw, 0x53425355);      // write the signature
                        MEM_WR32((UINT08*)pBufCsw + 4, MEM_RD32((UINT08*)pBufCbw + 4)); // copy the dCBWTag
                        MEM_WR32((UINT08*)pBufCsw + 8, 0);      // write the dCSWDataResidue
                        MEM_WR08((UINT08*)pBufCsw + 12, 0);     // write the status
                        if(OK != rc)
                        {
                            Printf(Tee::PriNormal, "Setting CSW to 0x1 due to error in handling SCSI command\n");
                            MEM_WR08((UINT08*)pBufCsw + 12, 1);    //Set status to 0x1 to indicate error
                            //TODO: Set Sense data
                        }
                        // send the CSW
                        CHECK_RC_CLEANUP(SetupTd(m_BulkInEp, false, &cswFragList, false, true));
                        CHECK_RC_CLEANUP(WaitXfer(m_BulkInEp, false, GetTimeout(), NULL, true));
                        if ( m_DeviceState!= XUSB_DEVICE_STATE_CONFIGURED )
                        {   // when wait for transfer, device is reset, exit
                             goto Cleanup;
                        }
                    }
                    else
                    {
                        Printf(Tee::PriError, "Transfer Error happend while waiting for CBW\n");
                        CHECK_RC_CLEANUP(RC::HW_STATUS_ERROR);
                    }
                }
                else
                {
                    Printf(Tee::PriWarn,
                           "Un-expected XferCompletion Event on EPid %d when waiting EPid %d\n",
                           epId, myEpId);
                }
            }
            else
            {
                CHECK_RC_CLEANUP(EventHandler(&eventTrb));
                if ( m_DeviceState!= XUSB_DEVICE_STATE_CONFIGURED )
                {   // Device state has been changed in above EventHandler(), escape to upper loop
                    Printf(Tee::PriNormal, "Device State changed, existing Bulk Engine...\n");
                    hasKey = true;
                }
                if ( m_IsRstXferRing )
                {
                    Printf(Tee::PriLow, "XferRing reset, switch from EP %d ", epNumToAddCbw);
                    m_IsRstXferRing = false;
                    TransferRing * pXferRing;
                    XhciTrb * pDummyEnqTrb;
                    UINT32 segIndex, trbIndex;
                    CHECK_RC_CLEANUP(FindXferRing(epNumToAddCbw, true, &pXferRing));
                    CHECK_RC_CLEANUP(pXferRing->GetBackwardEntry(1, &pDummyEnqTrb, &segIndex, &trbIndex));
                    CHECK_RC_CLEANUP(pXferRing->SetEnqPtr(segIndex,  trbIndex));
                    // above code revert the TRB added for CBW at the beginning of the loop
                    // set isToAddCbwTd to true to require a new TRB be added for CBW in another transfer ring
                    isToAddCbwTd = true;
                    // re-callwlate Endpoint to expect Transfer Event
                    CHECK_RC_CLEANUP(DeviceContext::EpToDci(m_BulkOutEp, true, &myEpId, true));
                    Printf(Tee::PriLow, "to EP %d \n", m_BulkOutEp);
                }
            }
        }
        if ( pConsole->KeyboardHit() )
        {
            hasKey = true;
            if (Console::VKC_LOWER_P == pConsole->GetKey()) 
            {   // print debug information
                PrintAll();
                hasKey = false; // swallow the key
            }
        }
        
        if (m_TimeToFinish 
            && (Platform::GetTimeMS() > m_TimeToFinish))
        {
            hasKey = true;      // exit the loop
        }
    } while ( !hasKey );

Cleanup:

    MemoryTracker::FreeBuffer(pBufCsw);
    MemoryTracker::FreeBuffer(pBufCbw);
    Printf(Tee::PriNormal, "...Existing BulkEngine()\n");

    LOG_EXT();
    return rc;
}

RC XusbController::BulkCmdHandler(UINT08* pBufCbw)
{   // parse the CBW, construct the data TRB and send it to host side
    // for cmd definition ass also usbdext.cpp. e.g. CmdRead10()
    // or http://www.usb.org/developers/devclass_docs Mass Storage UFI Command Specification 1.0
    LOG_ENT();
    RC rc = OK;
    MemoryFragment::FRAGLIST dataFragList;
    MemoryFragment::FRAGLIST * pDataFragList = NULL;
    MemoryFragment::FRAGMENT myFrag;
    void * pDataBuf = NULL;
    char filename[255];
    FILE *pFile;
    UINT32 writenSize = 0;
    UINT32 xferSizePerTrb;
    UINT32 numTrb;
    UINT32 residueByte;
    UINT32 xferLengthBlock;
    UINT32 lba;
    UINT32 capacity;
    UINT32 i;
    UINT32 byteNum = 0;
    UINT32 byteIntoFrag = 0;
    // bool isEnUr = GetDebugMode()&XUSB_DEBUG_MODE_EN_UNDERRUN;

    UINT08 allocLen;
    // grabbed from a USB flash drive
    UINT08 respInquiry[] = {0x00,0x00,0x00,0x02,0x1f,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00};
    UINT08 respModeSense[] = {0x00,0x00,0x00,0x43,0x03,0x00,0x0a,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x80,
    0x88,0x13,0x1e,0x05,0x00,0x3f,0x10,0x00,0x00,0xb0,0x07,0x00,0x00,0x00,0x00,0x00,
    0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x1e,0x00,0x00,0x00,0x00,0x00,0x00,0x68,0x01,
    0x01,0x00,0x0a,0x1b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x00,0x06,0x1c,0x1c};
    UINT08 respCmdA1[] = {
    0x7A,0x42,0xFF,0x3F,0x37,0xC8,0x10,0x00,0x00,0x00,0x00,0x00,0x3F,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x57,0x20,0x2D,0x44,0x4D,0x57,0x55,0x41,
    0x30,0x52,0x39,0x30,0x31,0x39,0x36,0x32,0x00,0x00,0x00,0x00,0x32,0x00,0x31,0x30,
    0x30,0x2E,0x31,0x30,0x31,0x30,0x44,0x57,0x20,0x43,0x44,0x57,0x30,0x32,0x31,0x30,
    0x41,0x46,0x53,0x53,0x30,0x2D,0x55,0x30,0x42,0x30,0x20,0x30,0x20,0x20,0x20,0x20,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x10,0x80,
    0x00,0x00,0x00,0x2F,0x01,0x40,0x00,0x00,0x00,0x00,0x07,0x00,0xFF,0x3F,0x10,0x00,
    0x3F,0x00,0x10,0xFC,0xFB,0x00,0x00,0x01,0xFF,0xFF,0xFF,0x0F,0x00,0x00,0x07,0x04,
    0x03,0x00,0x78,0x00,0x78,0x00,0x78,0x00,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x00,0x06,0x1F,0x00,0x00,0x44,0x00,0x40,0x00,
    0xFE,0x01,0x00,0x00,0x6B,0x74,0x69,0x7F,0x63,0x61,0x69,0x74,0x49,0xBC,0x63,0x61,
    0x7F,0x00,0xA0,0x00,0xA0,0x00,0x80,0x00,0xFE,0xFF,0x00,0x00,0xFE,0x80,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xB0,0x88,0xE0,0xE8,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x50,0xE0,0x4E,0x78,0xAC,0xCC,0x05,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x40,
    0x18,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xC3,0x16,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x37,0x30,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x20,0x1C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1E,0x10,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xA5,0x48
    };

    UINT08 cmd = MEM_RD08(pBufCbw+15);

    if (OK == GetScsiResp(cmd, &(myFrag.Address), &(myFrag.ByteSize)) )
    {   // if user spacify an empty string for the SCSI CMD, Data will be ignored, send CSW only
        if ( myFrag.ByteSize )
        {   // if user has specified lwstomized response, send it and return;
            Printf(Tee::PriNormal, "Replying to SCSI Cmd 0x%02x with packet:", cmd);
            dataFragList.clear();
            dataFragList.push_back(myFrag);
            Memory::Print08(myFrag.Address, myFrag.ByteSize);   // dump it
            CHECK_RC(SetupTd(m_BulkInEp, false, &dataFragList, false, true));
            CHECK_RC(WaitXfer(m_BulkInEp, false, GetTimeout(), NULL, true));
        }
        return OK;
    }

    switch ( cmd )
    {
        case 0x0:       // CmdTestUnitReady, actaully do nothing
            Printf(Tee::PriNormal, "[SCSI]Test Unit Ready received\n");
            break;
        case 0x23:
            Printf(Tee::PriNormal, "[SCSI]Read Format Capacity received\n");
            CHECK_RC( MemoryTracker::AllocBuffer(12 , &pDataBuf, true, 32, Memory::UC) );
            myFrag.Address = pDataBuf;
            myFrag.ByteSize = 12;
            //Format Capacity List Length
            MEM_WR08((UINT08*)myFrag.Address + 0, 0x00);
            MEM_WR08((UINT08*)myFrag.Address + 1, 0x00);
            MEM_WR08((UINT08*)myFrag.Address + 2, 0x00);
            MEM_WR08((UINT08*)myFrag.Address + 3, 0x08);
             // num of sectors
            capacity = 0x32000;
            MEM_WR08((UINT08*)myFrag.Address + 4, capacity >> 24);
            MEM_WR08((UINT08*)myFrag.Address + 5, capacity >> 16);
            MEM_WR08((UINT08*)myFrag.Address + 6, capacity >> 8);
            MEM_WR08((UINT08*)myFrag.Address + 7, capacity >> 0);
            // sector size
            capacity = 512;
            MEM_WR08((UINT08*)myFrag.Address + 8, 0x02); // Descriptor Code
            MEM_WR08((UINT08*)myFrag.Address + 9, capacity >> 16);
            MEM_WR08((UINT08*)myFrag.Address + 10, capacity >> 8);
            MEM_WR08((UINT08*)myFrag.Address + 11, capacity >> 0);
            dataFragList.push_back(myFrag);
            CHECK_RC_CLEANUP(SetupTd(m_BulkInEp, false, &dataFragList, false, true));
            CHECK_RC_CLEANUP(WaitXfer(m_BulkInEp, false, GetTimeout(), NULL, true));
            break;
        case 0x25:      // CmdReadCapacity, always return a size of 48MB (512*0x18000)
            Printf(Tee::PriNormal, "[SCSI]Read Capacity received\n");
            CHECK_RC( MemoryTracker::AllocBuffer(8 , &pDataBuf, true, 32, Memory::UC) );
            myFrag.Address = pDataBuf;
            myFrag.ByteSize = 8;
            // num of sectors
            capacity = 0x32000;
            MEM_WR08((UINT08*)myFrag.Address + 0, capacity >> 24);
            MEM_WR08((UINT08*)myFrag.Address + 1, capacity >> 16);
            MEM_WR08((UINT08*)myFrag.Address + 2, capacity >> 8);
            MEM_WR08((UINT08*)myFrag.Address + 3, capacity >> 0);
            // sector size
            capacity = 512;
            MEM_WR08((UINT08*)myFrag.Address + 4, capacity >> 24);
            MEM_WR08((UINT08*)myFrag.Address + 5, capacity >> 16);
            MEM_WR08((UINT08*)myFrag.Address + 6, capacity >> 8);
            MEM_WR08((UINT08*)myFrag.Address + 7, capacity >> 0);
            dataFragList.push_back(myFrag);
            CHECK_RC_CLEANUP(SetupTd(m_BulkInEp, false, &dataFragList, false, true));
            CHECK_RC_CLEANUP(WaitXfer(m_BulkInEp, false, GetTimeout(), NULL, true));
            break;
        case 0x1a:  // mode sense
            allocLen = MEM_RD08(pBufCbw + 19);
            Printf(Tee::PriNormal, "[SCSI]Mode Sense(6) received, alloc %d Bytes (sending %d Bytes)\n",
                    allocLen, (UINT32)sizeof(respModeSense));
            CHECK_RC( MemoryTracker::AllocBuffer(allocLen , &pDataBuf, true, 32, Memory::UC) );
            myFrag.Address = pDataBuf;
            myFrag.ByteSize = allocLen;
            //fill the data
            for ( i = 0; i < allocLen; i++ )
            {
                if (  i < sizeof(respModeSense) )
                {
                    MEM_WR08((UINT08*)myFrag.Address + i, respModeSense[i]);
                }
                else
                {
                    MEM_WR08((UINT08*)myFrag.Address + i, 0);
                }
            }
            dataFragList.push_back(myFrag);
            CHECK_RC_CLEANUP(SetupTd(m_BulkInEp, false, &dataFragList, false, true));
            CHECK_RC_CLEANUP(WaitXfer(m_BulkInEp, false, GetTimeout(), NULL, true));
            break;
        case 0xa1:  // sent by Ubuntu host, don't know what it is
            Printf(Tee::PriNormal, "[SCSI]Cmd 0xa1 received, sending %d Bytes\n", (UINT32)sizeof(respCmdA1));
            CHECK_RC( MemoryTracker::AllocBuffer(sizeof(respCmdA1) , &pDataBuf, true, 32, Memory::UC) );
            myFrag.Address = pDataBuf;
            myFrag.ByteSize = sizeof(respCmdA1);
            //fill the data
            for ( i = 0; i < sizeof(respCmdA1); i++ )
            {
                MEM_WR08((UINT08*)myFrag.Address + i, respCmdA1[i]);
            }
            dataFragList.push_back(myFrag);
            CHECK_RC_CLEANUP(SetupTd(m_BulkInEp, false, &dataFragList, false, true));
            CHECK_RC_CLEANUP(WaitXfer(m_BulkInEp, false, GetTimeout(), NULL, true));
            break;
        case 0x12:      // CmdInq
            allocLen = MEM_RD08(pBufCbw + 19);
            Printf(Tee::PriNormal, "[SCSI]Inquiry received, alloc %d Bytes (effective %d Bytes)\n",
                                    allocLen, (UINT32)sizeof(respInquiry));
            CHECK_RC( MemoryTracker::AllocBuffer(allocLen , &pDataBuf, true, 32, Memory::UC) );
            myFrag.Address = pDataBuf;
            myFrag.ByteSize = allocLen;
            //fill the data
            for ( i = 0; i < allocLen; i++ )
            {
                if (  i < sizeof(respInquiry) )
                {
                    MEM_WR08((UINT08*)myFrag.Address + i, respInquiry[i]);
                }
                else
                {
                    MEM_WR08((UINT08*)myFrag.Address + i, 0);
                }
            }
            dataFragList.push_back(myFrag);

            CHECK_RC_CLEANUP(SetupTd(m_BulkInEp, false, &dataFragList, false, true));
            CHECK_RC_CLEANUP(WaitXfer(m_BulkInEp, false, GetTimeout(), NULL, true));
            break;
        case 0x28:      // CmdRead10
        case 0x2A:      // CmdWrite10
            // Get the transfer length
            lba  = MEM_RD08(pBufCbw + 17) << 24;
            lba |= MEM_RD08(pBufCbw + 18) << 16;
            lba |= MEM_RD08(pBufCbw + 19) << 8;
            lba |= MEM_RD08(pBufCbw + 20) << 0;
            xferLengthBlock  = MEM_RD08(pBufCbw + 22) << 8;
            xferLengthBlock |= MEM_RD08(pBufCbw + 23) << 0;
            Printf(Tee::PriNormal,
                   "[SCSI]Host request %s %d blocks at LBA 0x%x \n",
                   (cmd == 0x28)?"Read":"Write", xferLengthBlock, lba);

            // allocate data buffers
            // CHECK_RC(     MemoryTracker::AllocBuffer(xferLengthBlock * 512, &pDataBuf, true, 32, Memory::UC) );
            CHECK_RC( MemoryTracker::AllocBufferAligned(xferLengthBlock * 512, &pDataBuf, 1024, 32, Memory::UC) );

            // build up the fraglist
            xferSizePerTrb = TrbHelper::GetMaxXferSizeNormalTrb();
            numTrb = (xferLengthBlock * 512 + xferSizePerTrb - 1)/ xferSizePerTrb;
            residueByte =  xferLengthBlock * 512;
            for ( i = 0; i < numTrb; i++ )
            {
                myFrag.Address = (UINT08*)pDataBuf + i * xferSizePerTrb;
                if ( i == (numTrb-1) )
                {
                    myFrag.ByteSize = residueByte;
                }
                else
                {
                    myFrag.ByteSize = xferSizePerTrb;
                }
                dataFragList.push_back(myFrag);
                residueByte -= xferSizePerTrb;
            }

            if ( 0x28 == cmd )
            {   // read
                // read local file to memory buffer
                for ( i = 0; i < xferLengthBlock; i++ )
                {
                    sprintf(filename, "lba%x.bin", lba+i);
                    //Open a file for reading
                    rc = Utility::OpenFile(filename, &pFile, "rb");
                    if ( OK != rc )
                    {   // file not exist
                        // Memory::Fill32((UINT08*)pDataBuf + writenSize, 0xdeadbeef, 512/4);
                        Memory::FillPattern((UINT08*)pDataBuf + writenSize, &m_vDataPattern32, 512);
                    }
                    else
                    {
                        fseek (pFile, 0, SEEK_SET);
    #if defined(SIM_BUILD) || defined(INCLUDE_PEATRANS)
                        // see also comment for Write
                        Printf(Tee::PriLow, "ReadSector(): Use temp buffer for Peatrans|SIM \n");
                        vector<UINT08> tmpBuff (512, 0);
                        UINT32 tmp = fread(&(tmpBuff[0]), 1, 512, pFile);
                        Memory::Fill((UINT08*)pDataBuf + writenSize, &tmpBuff);
    #else
                        UINT32 tmp = fread((UINT08*)pDataBuf + writenSize, 512, 1, pFile);
    #endif
                        fclose(pFile);
                        Printf(Tee::PriLow, "  Read %u bytes successfully\n", tmp);
                    }
                    writenSize += 512;
                }

                if ( m_IsEnBufBuild )
                {   // use BufBuild to allocate memory
                    // m_pBufBuild->PrintProperties();
                    CHECK_RC_CLEANUP(m_pBufBuild->GenerateFragmentList(m_bufIndex,
                                                               xferLengthBlock * 512,
                                                               &pDataFragList,
                                                               0xbabeface,
                                                               (GetDebugMode()&XUSB_DEBUG_MODE_PRINT_PRD_INFO)?0x2:0));
                    // copy data from pDataBuf to pDataFragList
                    Printf(Tee::PriLow, "  Read() data copy start ...");
                    byteNum = 0;
                    for ( i = 0; i < pDataFragList->size(); i++ )
                    {
                        byteIntoFrag = 0;
                        while ( byteIntoFrag < (*pDataFragList)[i].ByteSize )
                        {
                            MEM_WR08(static_cast<UINT08*>((*pDataFragList)[i].Address) + byteIntoFrag, MEM_RD08(static_cast<UINT08*>(pDataBuf) + byteNum));
                            byteIntoFrag++;
                            byteNum++;
                        }
                    }
                    Printf(Tee::PriLow, " %d Byte copied \n", byteNum);
                    CHECK_RC_CLEANUP(SetupTd(m_BulkInEp, false, pDataFragList, false, true, 0, 0, 0, m_BulkDelay?1:0, m_BulkDelay));
                }
                else
                {
                    CHECK_RC_CLEANUP(SetupTd(m_BulkInEp, false, &dataFragList, false, true, 0, 0, 0, m_BulkDelay?1:0, m_BulkDelay));
                }
                CHECK_RC_CLEANUP(WaitXfer(m_BulkInEp, false, GetTimeout(), NULL, true));
            }
            else
            {   // write
                if ( m_IsEnBufBuild )
                {   // use BufBuild to allocate memory
                    CHECK_RC_CLEANUP(m_pBufBuild->GenerateFragmentList(m_bufIndex,
                                                               xferLengthBlock * 512,
                                                               &pDataFragList,
                                                               0xbabeface,
                                                               (GetDebugMode()&XUSB_DEBUG_MODE_PRINT_PRD_INFO)?0x2:0));
                    CHECK_RC_CLEANUP(SetupTd(m_BulkOutEp, true, pDataFragList, false, true, 0, 0, 0, m_BulkDelay?1:0, m_BulkDelay));
                }
                else
                {
                    CHECK_RC_CLEANUP(SetupTd(m_BulkOutEp, true, &dataFragList, false, true, 0, 0, 0, m_BulkDelay?1:0, m_BulkDelay));
                }

                CHECK_RC_CLEANUP(WaitXfer(m_BulkOutEp, true, GetTimeout(), NULL, true));
                if ( m_IsEnBufBuild )
                {   // copy data from pDataFragList to pDataBuf
                    Printf(Tee::PriLow, "  Write() data copy start ...");
                    byteNum = 0;
                    for ( i = 0; i < pDataFragList->size(); i++ )
                    {
                        byteIntoFrag = 0;
                        while ( byteIntoFrag < (*pDataFragList)[i].ByteSize )
                        {
                            MEM_WR08(static_cast<UINT08*>(pDataBuf) + byteNum, MEM_RD08(static_cast<UINT08*>((*pDataFragList)[i].Address) + byteIntoFrag));
                            byteIntoFrag++;
                            byteNum++;
                        }
                    }
                    Printf(Tee::PriLow, " %d Byte copied \n", byteNum);
                }

                // save the received data to local file
                for ( i = 0; i < xferLengthBlock; i++ )
                {
                    sprintf(filename,"lba%x.bin", lba + i);
                    //Open a file for writing
                    CHECK_RC_CLEANUP(Utility::OpenFile(filename, &pFile, "wb"));
    #if defined(SIM_BUILD) || defined(INCLUDE_PEATRANS)
            // Memory allocated using Platform::Allocxxx cannot be used by
            // regular C functions in the simulatione environment
            // Allocate a temporary buffer as a intermidiate
                    Printf(Tee::PriLow, "WriteSector(): Use temp buffer for Peatrans|SIM \n");
                    vector<UINT08> tmpBuff (512, 0);
                    for ( UINT32 j = 0; j < 512; j++ )
                    {
                        tmpBuff[j] = MEM_RD08((UINT08 *)pDataBuf + writenSize + j);
                    }
                    UINT32 tmp = fwrite(&(tmpBuff[0]), 512, 1, pFile);
    #else
                    UINT32 tmp = fwrite((UINT08 *)pDataBuf + writenSize, 512, 1, pFile);
    #endif
                    fclose(pFile);
                    Printf(Tee::PriLow, "  Write %u bytes successfully\n", tmp*512);
                    writenSize += 512;
                }
                Printf(Tee::PriNormal, "%d block save to files.\n", xferLengthBlock);
            }
            break;
        default:
            Printf(Tee::PriError, "un-support SCSI command 0x%x, raw data: ", cmd);
            Memory::Print08(pBufCbw, 31);
            return RC::BAD_PARAMETER;
    }

Cleanup:
    if ( pDataBuf )
    {
        MemoryTracker::FreeBuffer(pDataBuf);
    }

    LOG_EXT();
    return rc;
}

RC XusbController::FindXferRing(UINT08 EndPoint, bool IsDataOut, TransferRing ** ppRing, UINT16 StreamId)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(Endpoint %d, IsDataOut %s) \n",EndPoint, IsDataOut?"T":"F");
    for ( UINT32 i = 0; i<m_vpRings.size(); i++ )
    {
        if ( m_vpRings[i]->IsThatMe(0, EndPoint, IsDataOut, StreamId) )
        {
            *ppRing = m_vpRings[i];
            return OK;
        }
    }

    * ppRing = NULL;
    Printf(Tee::PriError, "No Transfer Ring defined for this Endpoint");
    return RC::BAD_PARAMETER;
}

RC XusbController::RingDoorBell(UINT08 Endpoint, bool IsOut, UINT16 StreamIdCtrlSeqNum)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(EpNum %d, IsOut %s, StreamId/CtrlSeqNum 0x%x) \n", Endpoint, IsOut?"T":"F", StreamIdCtrlSeqNum);
    RC rc;
    if ( Endpoint > 15 )
    {
        Printf(Tee::PriError, "Invalid Endpoint %d, valid[0-15]", Endpoint);
        return RC::BAD_PARAMETER;
    }
    UINT32 data32 = 0;
    // ack the Run Change bit to unblock the Doorbell just in case
    CHECK_RC(RegRd(XUSB_REG_PARAMS2, &data32));
    data32 = FLD_SET_RF_NUM(XUSB_REG_PARAMS2_RC, 1, data32);
    CHECK_RC(RegWr(XUSB_REG_PARAMS2, data32));

    UINT08 target;
    CHECK_RC(DeviceContext::EpToDci(Endpoint, IsOut, &target, true));

    data32 = FLD_SET_RF_NUM(XUSB_REG_DB_TRGT, target, data32);
    data32 = FLD_SET_RF_NUM(XUSB_REG_DB_SEQN, StreamIdCtrlSeqNum, data32);

    CHECK_RC(RegWr(XUSB_REG_DB, data32));

    Printf(Tee::PriLow,"Written to DoorBell 0x%x \n", data32);
    LOG_EXT();
    return OK;
}

// to emulate buffer underrun, MODS splits the data transfer into two parts
// 1> first n TRB and 2> the rest TRBs
// after setup first TRB MODS ring doorbell
// then sleep for a while to trigger HW flow control
// at last fill the rest TRBs and ring the doorbell
// this is triggered by setting TrbNum to a non-zero
RC XusbController::SetupTd(UINT08 Endpoint,
                           bool IsDataOut,
                           MemoryFragment::FRAGLIST* pFrags,
                           bool IsIsoch,
                           bool IsDbRing,
                           UINT16 StreamId,
                           // below are for Isoch
                           bool IsSia,
                           UINT16 FrameId,
                           // below is for IsDbRing == true to emulate buffer underrun
                           UINT32 TrbNum,
                           UINT32 SleepUs)
{
    LOG_ENT();
    RC rc;
    if ( TrbNum && (TrbNum >= pFrags->size()) )
    {
        TrbNum = 0;
        Printf(Tee::PriWarn, "Ignore Bulk delay setting since there's no enough TRBs \n");
    }
    // Get the associated Ring
    TransferRing * pXferRing;
    CHECK_RC(FindXferRing(Endpoint, IsDataOut, &pXferRing, StreamId));

    UINT32 mps;
    CHECK_RC(GetEpMps(Endpoint, IsDataOut, &mps));
    CHECK_RC(pXferRing->InsertTd(pFrags, mps, IsIsoch, NULL, 0, IsSia, FrameId));

    if ( TrbNum )
    {
        XhciTrb *pTrbToStoppedAt;
        CHECK_RC(pXferRing->TruncateTD(TrbNum, &pTrbToStoppedAt));
        CHECK_RC(RingDoorBell(Endpoint, IsDataOut));
        Printf(Tee::PriLow,
               "Wait for %d TRB to finish @ Ep %d, Data %s \n",
               TrbNum, Endpoint, IsDataOut?"out":"in");
        CHECK_RC(m_pEventRing->WaitForEvent(pTrbToStoppedAt, 2000));

        Printf(Tee::PriLow, "  Sleep %d us for underrun/overrun \n",  SleepUs);
        Platform::SleepUS(SleepUs);
        // release the rest TRBs
        CHECK_RC(pXferRing->TruncateTD(0));
    }

    // ring the doorbell
    if ( IsDbRing )
    {
        CHECK_RC(RingDoorBell(Endpoint, IsDataOut));
    }

    LOG_EXT();
    return OK;
}

RC XusbController::GetEpMps(UINT08 Endpoint, bool IsDataOut, UINT32 * pMps)
{
    LOG_ENT();
    if(m_InitState < INIT_DONE)
    {
        Printf(Tee::PriError, "Controller has not been fully initialized, call Xusb.Init() first \n");
        return RC::WAS_NOT_INITIALIZED;
    }

    RC rc;
    CHECK_RC(m_pDevmodeEpContexts->GetMps(Endpoint, IsDataOut, pMps));

    LOG_EXT();
    return OK;
}

// Wait for Trasfer Completion from given endpoint.
// in the middle, we need to handle two conditions
// 1. BAD completion code for any of the tranfer TRBs
// 2. serve Setup Requests from EP0
RC XusbController::WaitXfer(UINT08 Endpoint,
                            bool IsDataOut,
                            UINT32 TimeoutMs,
                            UINT32 * pCompCode,
                            bool isWaitAll)
{
    LOG_ENT();
    if(m_InitState < INIT_DONE)
    {
        Printf(Tee::PriError, "Controller has not been fully initialized, call Xusb.Init() first \n");
        return RC::WAS_NOT_INITIALIZED;
    }

    RC rc;
    XhciTrb eventTrb;
    UINT08 completionCode = 0;  // invalid

    if ( isWaitAll )
    {   // wait for all the TRBs in specified Xfer Ring to be finished
        TransferRing * pXferRing;
        CHECK_RC(FindXferRing(Endpoint, IsDataOut, &pXferRing));

        XhciTrb * pTrb;
        CHECK_RC(pXferRing->GetBackwardEntry(1, &pTrb));

        UINT64 timeStartUs = Platform::GetTimeUS();
        UINT64 timeElapseUs = 0;
        do
        {
            rc = m_pEventRing->WaitForEvent(pTrb, 0, &eventTrb, false, true);
            if ( OK == rc )
            {
                break;
            }
            else if ( RC::TIMEOUT_ERROR != rc )
            {
                Printf(Tee::PriError,
                       "Error happens when waiting for TRB(@0x%llx) from Enpoint %d Data %s \n",
                       Platform::VirtualToPhysical((void*)pTrb), Endpoint, IsDataOut?"Out":"In");
                return rc;
            }
            else if ( 0 != Endpoint )
            {   // we handle Events for Control EP when WaitXfer for non-control EPs
                // and to avoid re-enter
                UINT32 oldDeviceState = m_DeviceState;
                if ( (OK == m_pEventRing->WaitForEvent(XhciTrbTypeEvSetupEvent, 0, &eventTrb, false) )
                    ||  (OK == m_pEventRing->WaitForEvent(XhciTrbTypeEvPortStatusChg, 0, &eventTrb, false) ) )
                {
                    Printf(Tee::PriNormal,
                           "  EP0 Event comes when waiting for Xfer completion... \n");
                    CHECK_RC(EventHandler(&eventTrb));
                }
                if ( (oldDeviceState == XUSB_DEVICE_STATE_CONFIGURED)
                     && (m_DeviceState != XUSB_DEVICE_STATE_CONFIGURED) )
                {   // Device state has been changed in above EventHandler(), escape to upper loop
                    Printf(Tee::PriNormal,
                           "Device State changed, aborting wait for transfer completion...\n");
                    if ( pCompCode )
                    {   // give alert to callers who don't check DeviceState
                        *pCompCode = XhciTrbCmpCodeIlwalid;
                    }
                    return OK;
                }
            }

            timeElapseUs = (Platform::GetTimeUS() - timeStartUs);
        } while ( (timeElapseUs/1000) < TimeoutMs );

        if ( (timeElapseUs/1000) >= TimeoutMs )
        {
            Printf(Tee::PriError,
                   "Timeout on waiting for the last TRB(@0x%llx) from Enpoint %d Data %s to finish \n",
                   Platform::VirtualToPhysical((void*)pTrb), Endpoint, IsDataOut?"Out":"In");
            return RC::TIMEOUT_ERROR;
        }
    }
    else
    {
        Printf(Tee::PriError, "Not support yes, use GetNewEvent() instead");
        return RC::SOFTWARE_ERROR;
    }

    // parse the completion code
    CHECK_RC(TrbHelper::ParseXferEvent(&eventTrb, &completionCode));

    if ( pCompCode )
    {
        *pCompCode = completionCode;
    }
    else
    {
        if ( (completionCode != XhciTrbCmpCodeSuccess)
             && (completionCode != XhciTrbCmpCodeShortPacket) )
        {
            Printf(Tee::PriError, "Bad Completion Code %d", completionCode);
            return RC::HW_STATUS_ERROR;
        }
    }

    Printf(Tee::PriLow, "Xfer Completion Code = %d \n", completionCode);
    LOG_EXT();
    return OK;
}

UINT32 XusbController::GetOptimisedFragLength()
{
    return TrbHelper::GetMaxXferSizeNormalTrb();
}

RC XusbController::PrintAll()
{
    PrintState();
    PrintReg();
    PrintContext();
    PrintRingList(Tee::PriNormal, true);
    Printf(Tee::PriNormal, "\n");
    return OK;
}

RC XusbController::PrintState(Tee::Priority Pri) const
{
    if(m_InitState < INIT_DONE)
    {
        Printf(Tee::PriError, "Controller has not been fully initialized, call Xusb.Init() first \n");
        return RC::WAS_NOT_INITIALIZED;
    }

    const char * strState[] = {"Default", "Addressed", "Configed", "ERROR"};
    Printf(Pri, "XUSB Controller Info:\n");
    Printf(Pri, "  BAR = (virt: %p) \n",  m_pRegisterBase);
    Printf(Pri, "  State = %s \n",
                strState[m_DeviceState & 0x3]);
    Printf(Pri, "  Lwstomized Device Descriptor:");
    if ( m_DevDescriptor.Address )
    {
        Printf(Pri, "\n");
        Memory::Print08(m_DevDescriptor.Address, m_DevDescriptor.ByteSize);
    }
    else
    {
        Printf(Pri, " NULL \n");
    }
    Printf(Pri, "  Lwstomized Config Descriptor");
    if ( m_vConfigDescriptor.size() )
    {
        Printf(Pri, "(%d Entries): \n", (UINT32)m_vConfigDescriptor.size());
        for ( UINT32 i = 0; i < m_vConfigDescriptor.size(); i++ )
        {
            Printf(Pri, "    Entry %d: \n", i);
            Memory::Print08(m_vConfigDescriptor[i].Address, m_vConfigDescriptor[i].ByteSize);
        }
    }
    else
    {
        Printf(Pri, ": NULL \n");
    }

    return OK;
}

RC XusbController::DeviceEnableInterrupts()
{
    LOG_ENT();
    RC rc;
    UINT32 val;
    CHECK_RC(RegRd(XUSB_REG_PARAMS1, &val));
    val = FLD_SET_RF_NUM(XUSB_REG_PARAMS1_IE, 0x1, val);
    CHECK_RC(RegWr(XUSB_REG_PARAMS1, val));

    LOG_EXT();
    return OK;
}

RC XusbController::DeviceDisableInterrupts()
{
    LOG_ENT();
    RC rc;
    UINT32 val;
    CHECK_RC(RegRd(XUSB_REG_PARAMS1, &val));
    val = FLD_SET_RF_NUM(XUSB_REG_PARAMS1_IE, 0x0, val);
    CHECK_RC(RegWr(XUSB_REG_PARAMS1, val));

    LOG_EXT();
    return OK;
}

RC XusbController::SetXferRingSize(UINT32 XferRingSize)
{
    if ( XferRingSize>0 )
    {
        m_XferRingSize=XferRingSize;
    }
    else
    {
        Printf(Tee::PriError, "Invalid XferRingSize");
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC XusbController::SetEventRingSize(UINT32 EventRingSize)
{
    if ( EventRingSize>0 ) m_EventRingSize=EventRingSize;
    else
    {
        Printf(Tee::PriError, "Invalid EventRingSize");
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC  XusbController::GetDeviceState(UINT32* DeviceState)
{
    if ( !DeviceState )
    {
        Printf(Tee::PriError, "NULL Pointer \n");
        return RC::BAD_PARAMETER;
    }
    else
    {
        *DeviceState=m_DeviceState;
    }
    return OK;
}

RC XusbController::PrintReg(Tee::Priority Pri) const
{
    UINT32 val, val1;
    RC rc;

    CHECK_RC(RegRd(XUSB_REG_PARAM0, &val));
    Printf(Pri, " (0x%02x) PARAM0 = 0x%08x \t", XUSB_REG_PARAM0, val);
    Printf(Pri, "\n  ERST Max %d \n",
           RF_VAL(XUSB_REG_PARAM0_ERSTMAX, val));

    CHECK_RC(RegRd(XUSB_REG_DB, &val));
    Printf(Pri, " (0x%02x) Doorbell = 0x%08x \t", XUSB_REG_DB, val);
    Printf(Pri, "\n  Doorbell Target %d, Doorbell Stream ID/Ctrl SeqNum %d \n",
           RF_VAL(XUSB_REG_DB_TRGT, val),
           RF_VAL(XUSB_REG_DB_SEQN, val));

    CHECK_RC(RegRd(XUSB_REG_ERSS, &val));
    Printf(Pri, " (0x%02x) Event Ring Segment Table Size = 0x%08x \t", XUSB_REG_ERSS, val);
    Printf(Pri, "\n  Segment 0 size %d, Segment 1 size %d \n",
           RF_VAL(XUSB_REG_ERSS_0, val),
           RF_VAL(XUSB_REG_ERSS_1, val));

    CHECK_RC(RegRd(XUSB_REG_ERS0_BA_LO, &val));
    CHECK_RC(RegRd(XUSB_REG_ERS0_BA_HI, &val1));
    PHYSADDR phyBar = val1;
    phyBar = (phyBar << 32) | val;
    Printf(Pri, " (0x%02x) Event Ring Segment 0 BAR = 0x%llx \n", XUSB_REG_ERS0_BA_LO, phyBar);

    CHECK_RC(RegRd(XUSB_REG_ERS1_BA_LO, &val));
    CHECK_RC(RegRd(XUSB_REG_ERS1_BA_HI, &val1));
    phyBar = val1;
    phyBar = (phyBar << 32) | val;
    Printf(Pri, " (0x%02x) Event Ring Segment 1 BAR = 0x%llx \n", XUSB_REG_ERS1_BA_LO, phyBar);

    CHECK_RC(RegRd(XUSB_REG_DP_LO, &val));
    CHECK_RC(RegRd(XUSB_REG_DP_HI, &val1));
    phyBar = val1;
    phyBar = (phyBar << 32) | (RF_VAL(XUSB_REG_DP_LO_OFFSET, val)<<(0?XUSB_REG_DP_LO_OFFSET));
    Printf(Pri, " (0x%02x) Event Ring Dequeue Pointer  = 0x%llx \n", XUSB_REG_DP_LO, phyBar);
    Printf(Pri, "   Event Handler Busy = %d \n", RF_VAL(XUSB_REG_DP_EHB, val));

    CHECK_RC(RegRd(XUSB_REG_EP_LO, &val));
    CHECK_RC(RegRd(XUSB_REG_EP_HI, &val1));
    phyBar = val1;
    phyBar = (phyBar << 32) | (RF_VAL(XUSB_REG_EP_LO_OFFSET, val)<<(0?XUSB_REG_EP_LO_OFFSET));
    Printf(Pri, " (0x%02x) Event Ring Enqueue Pointer  = 0x%llx \n", XUSB_REG_EP_LO, phyBar);
    Printf(Pri, "   Enqueue Cycle State = %d \n", RF_VAL(XUSB_REG_EP_LO_CYCLE_STATE, val));

    CHECK_RC(RegRd(XUSB_REG_PARAMS1, &val));
    Printf(Pri, " (0x%02x) Control = 0x%08x \t", XUSB_REG_PARAMS1, val);
    Printf(Pri, "\n  Run %d, Port Status Event Enable %d, Interrupt Enable %d "
                "\n  SMI on Event Interrupt Enable %d, SMI on Device System Error Enable %d "
                "\n  Device Address %d, Enable %d \n",
                RF_VAL(XUSB_REG_PARAMS1_RUN, val),
                RF_VAL(XUSB_REG_PARAMS1_PSEE, val),
                RF_VAL(XUSB_REG_PARAMS1_IE, val),
                RF_VAL(XUSB_REG_PARAMS1_SMI_EIE, val),
                RF_VAL(XUSB_REG_PARAMS1_SMI_DESS, val),
                RF_VAL(XUSB_REG_PARAMS1_DEVADDR, val),
                RF_VAL(XUSB_REG_PARAMS1_EN, val));

    CHECK_RC(RegRd(XUSB_REG_PARAMS2, &val));
    Printf(Pri, " (0x%02x) Status = 0x%08x \t", XUSB_REG_PARAMS2, val);
    Printf(Pri, "\n  Run Change %d, Interrupt Pending %d, Device System Error %d \n",
                RF_VAL(XUSB_REG_PARAMS2_RC, val),
                RF_VAL(XUSB_REG_PARAMS2_IP, val),
                RF_VAL(XUSB_REG_PARAMS2_DEV_SYS_ERR, val));

    CHECK_RC(RegRd(XUSB_REG_INTR, &val));
    Printf(Pri, " (0x%02x) Interrupt Modulation = 0x%08x \t", XUSB_REG_INTR, val);
    Printf(Pri, "\n  Interval %d, Counter %d \n",
                RF_VAL(XUSB_REG_INTR_MI, val),
                RF_VAL(XUSB_REG_INTR_MC, val));

    CHECK_RC(RegRd(XUSB_REG_PORTSC, &val));
    Printf(Pri, " (0x%02x) Port Status and Control = 0x%08x \t", XUSB_REG_PORTSC, val);
    Printf(Pri, "\n  Current Connect Status %d, Port Enabled/Disabled %d "
                "\n  Port Reset %d, Port Link State %d, Port Speed %d "
                "\n  Connect Status Change %d, Port Reset Change %d "
                "\n  Port Link Status Change %d, Port Config Error Change %d \n",
                RF_VAL(XUSB_REG_PORTSC_CCS, val),
                RF_VAL(XUSB_REG_PORTSC_PED, val),
                RF_VAL(XUSB_REG_PORTSC_PR, val),
                RF_VAL(XUSB_REG_PORTSC_PLS, val),
                RF_VAL(XUSB_REG_PORTSC_PS, val),
                RF_VAL(XUSB_REG_PORTSC_CSC, val),
                RF_VAL(XUSB_REG_PORTSC_PRC, val),
                RF_VAL(XUSB_REG_PORTSC_PLSC, val),
                RF_VAL(XUSB_REG_PORTSC_PCEC, val));

    CHECK_RC(RegRd(XUSB_REG_EPPR_LO, &val));
    CHECK_RC(RegRd(XUSB_REG_EPPR_HI, &val1));
    phyBar = val1;
    phyBar = (phyBar << 32) | (RF_VAL(XUSB_REG_EPPR_LO_OFFSET, val)<<(0?XUSB_REG_EPPR_LO_OFFSET));
    Printf(Pri, " (0x%02x) Endpoint Context Pointer = 0x%llx \n", XUSB_REG_EPPR_LO, phyBar);

    CHECK_RC(RegRd(XUSB_REG_FRAMEIDR, &val));
    Printf(Pri, " (0x%02x) Frame ID = 0x%08x \t", XUSB_REG_FRAMEIDR, val);
    Printf(Pri, "\n  uFrame ID %d, Frame ID %d \n",
                RF_VAL(XUSB_REG_FRAMEIDR_UFRAMEID, val),
                RF_VAL(XUSB_REG_FRAMEIDR_FRAMEID, val));

    CHECK_RC(RegRd(XUSB_REG_PARAMS4, &val));
    Printf(Pri, " (0x%02x) Port PM Status and Control = 0x%08x \t", XUSB_REG_PARAMS4, val);
    Printf(Pri, "\n  HighSpeed L1 State %d, HighSpeed L1 Remote Wake Enable %d "
                "\n  HighSpeed Host Initiated Resume Duration %d, SuperSpeed U2 Timeout %d "
                "\n  SS U1 Timeout %d, SS Force Link PM Accept %d, Vbus Attach Status %d "
                "\n  Wake on Connection %d, Wake on Disconnection %d, SS U1 Enable %d, SS U2 Enable %d, "
                "\n  SS Func Remote Wake Enable %d, SS Deferred PING Response %d \n",
                RF_VAL(XUSB_REG_PARAMS4_HSL1S, val),
                RF_VAL(XUSB_REG_PARAMS4_HSL1RWE, val),
                RF_VAL(XUSB_REG_PARAMS4_HSHIRD, val),
                RF_VAL(XUSB_REG_PARAMS4_SSU2T, val),
                RF_VAL(XUSB_REG_PARAMS4_SSU1T, val),
                RF_VAL(XUSB_REG_PARAMS4_SSFLPA, val),
                RF_VAL(XUSB_REG_PARAMS4_VAS, val),
                RF_VAL(XUSB_REG_PARAMS4_WC, val),
                RF_VAL(XUSB_REG_PARAMS4_WDC, val),
                RF_VAL(XUSB_REG_PARAMS4_SSU1E, val),
                RF_VAL(XUSB_REG_PARAMS4_SSU2E, val),
                RF_VAL(XUSB_REG_PARAMS4_SSFRWE, val),
                RF_VAL(XUSB_REG_PARAMS4_SSDPR, val));

    CHECK_RC(RegRd(XUSB_REG_EPHALTR, &val));
    Printf(Pri, " (0x%02x) Endpoint Halt = 0x%08x \n", XUSB_REG_EPHALTR, val);

    CHECK_RC(RegRd(XUSB_REG_EPPAUSER, &val));
    Printf(Pri, " (0x%02x) Endpoint Pause = 0x%08x \n", XUSB_REG_EPPAUSER, val);

    CHECK_RC(RegRd(XUSB_REG_EPLCR, &val));
    Printf(Pri, " (0x%02x) Endpoint Load Context = 0x%08x \n", XUSB_REG_EPLCR, val);

    CHECK_RC(RegRd(XUSB_REG_EPSCR, &val));
    Printf(Pri, " (0x%02x) Endpoint State Change = 0x%08x \n", XUSB_REG_EPSCR, val);

    CHECK_RC(RegRd(XUSB_REG_FCTR, &val));
    Printf(Pri, " (0x%02x) Flow Control Threshold = 0x%08x \n", XUSB_REG_FCTR, val);

    CHECK_RC(RegRd(XUSB_REG_DNR, &val));
    CHECK_RC(RegRd(XUSB_REG_DNR_DNTSHI, &val1));
    UINT64 data64 = val1;
    data64 = (data64 << 32) | val;
    Printf(Pri, " (0x%02x) Device Notification = 0x%llx \n", XUSB_REG_DNR, data64);
    Printf(Pri, "   Device Notification Packet Fire = %d \n", RF_VAL(XUSB_REG_DNR_DNPF, val));
    Printf(Pri, "   Notifiation Type = %d \n", RF_VAL(XUSB_REG_DNR_NT, val));
    data64 = val1;
    data64 = (data64 << (32-(0?XUSB_REG_DNR_DNTSLO_OFFSET))) | (val >> (0?XUSB_REG_DNR_DNTSLO_OFFSET));
    Printf(Pri, "   Notification Type = 0x%llx \n", data64);

    return OK;
}

RC XusbController::PrintRingList(Tee::Priority Pri, bool IsPrintDetail /*=false*/)
{
    RC rc;

    Printf(Pri, "Ring List: \n");

    UINT32 maxRingUuid;
    CHECK_RC(FindRing(0, NULL, &maxRingUuid));

    XhciTrbRing * pRing;
    for ( UINT32 i = 0; i <= maxRingUuid; i++ )
    {
        FindRing(i, &pRing);    //<--no CHECK_RC
        if ( pRing )
        {
            Printf(Pri, "  %u - \t", i);
            pRing->PrintTitle(Pri);
            if (IsPrintDetail)
            {
                CHECK_RC(pRing->PrintInfo(Pri));
            }
        }
    }

    return OK;
}

RC XusbController::PrintRing(UINT32 Uuid, Tee::Priority Pri)
{
    RC rc;

    XhciTrbRing * pRing;
    CHECK_RC(FindRing(Uuid, &pRing));
    if ( pRing )
    {
        CHECK_RC(pRing->PrintInfo(Pri));
    }
    else
    {
        Printf(Pri, "  Ring %u not exists \n", Uuid);
    }

    return OK;
}

RC XusbController::PrintContext(Tee::Priority Pri)
{
    if(m_InitState < INIT_DONE)
    {
        Printf(Tee::PriError, "Controller has not been fully initialized, call Xusb.Init() first \n");
        return RC::WAS_NOT_INITIALIZED;
    }

    return m_pDevmodeEpContexts->PrintEpContext(0, Tee::PriNormal, Pri);
}

RC XusbController::FindRing(UINT32 Uuid, XhciTrbRing ** ppRing, UINT32 * pMaxUuid)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(Uuid %u) \n", Uuid);
    if(m_InitState < INIT_DONE)
    {
        Printf(Tee::PriError, "Controller has not been fully initialized, call Xusb.Init() first \n");
        return RC::WAS_NOT_INITIALIZED;
    }

    UINT32 maxUuid = m_vpRings.size();      // Elwent Ring * 1 + XferRing * m_vpRings.size()
    if ( pMaxUuid )
    {
        *pMaxUuid = maxUuid;
        return OK;
    }
    if ( !ppRing )
    {
        Printf(Tee::PriError, "pRing null pointer");
        return RC::BAD_PARAMETER;
    }

    if ( Uuid ==0 )
    {
        *ppRing = m_pEventRing;
    }
    else
    {
        if ( Uuid > maxUuid )
        {
            Printf(Tee::PriError, "RingId %u overeflow, valid [0-%d], use PrintRing() to see the list", Uuid, maxUuid);
            *ppRing = NULL;
        }
        else
        {
            *ppRing = m_vpRings[Uuid - 1];
        }
    }

    LOG_EXT();
    if ( !(*ppRing) )
        return RC::SOFTWARE_ERROR;   // we don't return NULL without any error

    return OK;
}

RC XusbController::NewEventTrbHandler(XhciTrb* pTrb)
{
    LOG_ENT();
    RC rc;
    UINT08 type;
    CHECK_RC(TrbHelper::GetTrbType(pTrb, &type, Tee::PriDebug, true));

    if ( type == XhciTrbTypeEvTransfer )
    {
        PHYSADDR pXferTrb;
        UINT08 completionCode, dci, endpoint;
        bool isDataOut;
        CHECK_RC(TrbHelper::ParseXferEvent(pTrb, &completionCode, &pXferTrb, NULL, NULL, NULL, &dci));
        CHECK_RC(DeviceContext::DciToEp(dci, &endpoint, &isDataOut));

        // Update Xfer Ring's Deq Pointer
        CHECK_RC(UpdateXferRingDeq(&m_vpRings, 0, endpoint, isDataOut, pXferTrb));
    }
    LOG_EXT();
    return OK;
}

RC XusbController::UpdateEventRingDeq(UINT32 EventIndex, XhciTrb * pDeqPtr, UINT32 SegmengIndex /*dummy for device mode*/)
{
    RC rc;
    if ( 0 != EventIndex || SegmengIndex > 1 )
    {   // we have two segments for Event Ring
        Printf(Tee::PriError, "Invalid Event Ring Index %u, Segment Index %u", EventIndex, SegmengIndex);
        return RC::SOFTWARE_ERROR;
    }

    PHYSADDR pTrb = Platform::VirtualToPhysical(pDeqPtr);
    Printf(Tee::PriDebug, "Updating Event Ring %d DeqPtr to %p (0x%llx) \n", EventIndex, pDeqPtr, pTrb);

    UINT32 data32 = pTrb;
    // if SW does not touch Event Ring since last time a interrup asserted,
    // no interrupt generated even when IMODC count down to 0 and Event Ring not empty
    data32 = FLD_SET_RF_NUM(XUSB_REG_DP_EHB, 0x1, data32);
    CHECK_RC(RegWr(XUSB_REG_DP_LO, data32));
    pTrb = pTrb >> 32;
    data32 = pTrb;
    CHECK_RC(RegWr(XUSB_REG_DP_HI, data32));
    return OK;
}

RC XusbController::SetIsoTdSize(UINT32 TdSize)
{
    Printf(Tee::PriNormal,
           "  TD buffer size (old value %d) =  %d(0x%x)\n",
           m_IsoTdSize, TdSize, TdSize);
    m_IsoTdSize = TdSize;
    return OK;
}

RC XusbController::SetScsiResp(UINT08 Cmd, vector<UINT08> *pvData)
{
    LOG_ENT();
    RC rc;
    // search existed command response and do a setup / replace
    for ( UINT32 index = 0; index < m_vScsiCmdResponse.size(); index++ )
    {
        if ( m_vScsiCmdResponse[index].PhysAddr == Cmd )
        {
            // remove the old one
            if ( m_vScsiCmdResponse[index].Address )
            {
                MemoryTracker::FreeBuffer(m_vScsiCmdResponse[index].Address);
            }
            m_vScsiCmdResponse.erase(m_vScsiCmdResponse.begin() + index);
            break;
        }
    }

    // allocate memory and fill it with data from vector
    // for empty string, we store NULL, when engine find the this, it will send only the CSW
    UINT08 * pDataIn = NULL;
    if ( pvData->size() )
    {
        CHECK_RC( MemoryTracker::AllocBuffer(pvData->size(), (void**)&pDataIn, true, 32, Memory::UC) );
        for ( UINT32 i = 0; i < pvData->size(); i++ )
        {
            MEM_WR08(pDataIn + i, (*pvData)[i]);
        }
    }
    MemoryFragment::FRAGMENT myFrag;
    myFrag.Address = pDataIn;
    myFrag.ByteSize = pvData->size();
    myFrag.PhysAddr = Cmd;              // we use PhysAddr as Index
    m_vScsiCmdResponse.push_back(myFrag);

    Printf(Tee::PriNormal, "SCSI response %u BYTE saved for CMD 0x%02x \n", myFrag.ByteSize, (UINT08) myFrag.PhysAddr);
    LOG_EXT();
    return OK;
}

RC XusbController::GetScsiResp(UINT08 Cmd, void **ppData, UINT32 *pLength)
{   // search existed command response and do a setup / replace
    LOG_ENT();
    if ( !ppData || !pLength )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *ppData = NULL;
    *pLength = 0;
    bool isFound = false;
    for ( UINT32 index = 0; index < m_vScsiCmdResponse.size(); index++ )
    {
        if ( m_vScsiCmdResponse[index].PhysAddr == Cmd )
        {
            *ppData = m_vScsiCmdResponse[index].Address;
            *pLength = m_vScsiCmdResponse[index].ByteSize;
            isFound = true;
            break;
        }
    }
    if ( !isFound )
    {
        // no error message here
        return RC::BAD_PARAMETER;
    }
    LOG_EXT();
    return OK;
}

// SetRetry is called by C++ as GetRetry with auto-decr
RC XusbController::SetRetry(UINT32 Times, UINT32 * pTimesLeft /*=NULL*/)
{
    if ( !pTimesLeft )
    {
        m_RetryTimes    = Times;
        m_RetryTimesBuf = Times;
        if ( !Times )
        {
            Printf(Tee::PriNormal, " Retry disabled \n");
        }
        else
        {
            Printf(Tee::PriNormal, " Retry = %d \n", Times);
        }
    }
    else
    {
        *pTimesLeft = m_RetryTimesBuf;
        if (0 == m_RetryTimesBuf)
        {
            if ( m_RetryTimes )
            {
                m_RetryTimesBuf = m_RetryTimes;
            }
        }
        else
        {
            m_RetryTimesBuf--;
        }
    }
    return OK;
}

const char* XusbController::GetName()
{
    return CLASS_NAME;
}

RC XusbController::ToggleControllerInterrupt(bool IsEn)
{
    if (IsEn)
    {
        return DeviceEnableInterrupts();
    }
    return DeviceDisableInterrupts();
}

RC XusbController::GetIsr(ISR* Func) const
{
    Printf(Tee::PriError, "Not Implemented \n");
    return RC::BAD_PARAMETER;
}

RC XusbController::EnBufParams(bool IsEnable)
{
    m_IsEnBufBuild = IsEnable;
    Printf(Tee::PriNormal, "Buffer management %s\n", (IsEnable)?"On":"Off");
    return OK;
}

RC XusbController::SetupJsCallback(UINT32 CbIndex, JSFunction * pCallbackFunc)
{
    if ( XUSB_JS_CALLBACK_CONN == CbIndex )
    {
        m_pJsCbConn = pCallbackFunc;
        Printf(Tee::PriNormal, "Call back for Conn %s\n", (pCallbackFunc==NULL)?"Unset":"Set");
    }
    else if ( XUSB_JS_CALLBACK_DISCONN == CbIndex )
    {
        m_pJsCbDisconn = pCallbackFunc;
        Printf(Tee::PriNormal, "Call back for Disconn %s\n", (pCallbackFunc==NULL)?"Unset":"Set");
    }
    else if ( XUSB_JS_CALLBACK_U3U0 == CbIndex )
    {
        m_pJsCbU3U0 = pCallbackFunc;
        Printf(Tee::PriNormal, "Call back for U3U0 %s\n", (pCallbackFunc==NULL)?"Unset":"Set");
    }
    else
    {
        Printf(Tee::PriError, "Un-supported hook index %d \n", CbIndex );
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC XusbController::SetBufParams(UINT32 SizeMin, UINT32 SizeMax,
                                UINT32 NumMin, UINT32 NumMax,
                                UINT32 OffMin, UINT32 OffMax, UINT32 OffStep,
                                UINT32 BDataWidth, UINT32 Align, bool IsShuffle)
{
    if ( !m_pBufBuild )
    {
        Printf(Tee::PriError, "Null point for BufBuild\n");
        return RC::SOFTWARE_ERROR;
    }

    m_pBufBuild->p_SizeMin  = SizeMin;
    m_pBufBuild->p_SizeMax  = SizeMax;
    m_pBufBuild->p_NumMin   = NumMin;
    m_pBufBuild->p_NumMax   = NumMax;
    m_pBufBuild->p_OffMin   = OffMin;
    m_pBufBuild->p_OffMax   = OffMax;
    m_pBufBuild->p_OffStep  = OffStep;
    m_pBufBuild->p_DataWidth= BDataWidth;
    m_pBufBuild->p_Align    = Align;
    m_pBufBuild->p_IsShuffle= IsShuffle;
    return OK;
}

RC XusbController::SetBulkDelay(UINT32 DelayUs)
{
    m_BulkDelay = DelayUs;
    return OK;
}

RC XusbController::SetPattern32(vector<UINT32> * pvData32)
{
    m_vDataPattern32.swap(*pvData32);
    return OK;
}
