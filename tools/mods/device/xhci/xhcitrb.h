/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
On Peatrans MEM_RD and MEM_WR will not work for local variables in stack
(but work for memory allocated by memtrack)
So flags IsTrbInStack is added to some of the functions to indicate where the
target TRB locate. see also: TrbCopy(), and search IsTrbInStack

the generic guideline for adding the flag to functions are as follows
1.all ParseXxx() for Event TRBs, since they will only deal with local Event queue,
should access stack only. note: not including ParseNormalTrb()
2.all MakeXxx() and SetupXxx() should access variables in allocated memory only
*/

#ifndef INCLUDED_XHCITRB_H
#define INCLUDED_XHCITRB_H

#ifndef INCLUDED_STL_VECTOR
    #include <vector>
    #define INCLUDED_STL_VECTOR
#endif

struct _XhciTrb
{
    UINT32 DW0;
    UINT32 DW1;
    UINT32 DW2;
    UINT32 DW3;

    _XhciTrb()
    {
        DW0=0;DW1=0;DW2=0;DW3=0;
    }
};
typedef struct _XhciTrb XhciTrb;

// enum for TRB Types
typedef enum _XhciTrbType
{
// General
    XhciTrbTypeReserved             = 0,
    XhciTrbTypeNormal               = 1,
    XhciTrbTypeSetup                = 2,
    XhciTrbTypeData                 = 3,
    XhciTrbTypeStatus               = 4,
    XhciTrbTypeIsoch                = 5,
    XhciTrbTypeLink                 = 6,
    XhciTrbTypeEventData            = 7,
    XhciTrbTypeNoOp                 = 8,
// Cmd TRB
    XhciTrbTypeCmdEnableSlot        = 9,
    XhciTrbTypeCmdDisableSlot       = 10,
    XhciTrbTypeCmdAddrDev           = 11,
    XhciTrbTypeCmdCfgEndp           = 12,
    XhciTrbTypeCmdEvalContext       = 13,
    XhciTrbTypeCmdResetEndp         = 14,
    XhciTrbTypeCmdStopEndp          = 15,
    XhciTrbTypeCmdSetTrDeqPtr       = 16,
    XhciTrbTypeCmdResetDev          = 17,
    XhciTrbTypeCmdForceEvent        = 18,
    XhciTrbTypeCmdNegoBW            = 19,
    XhciTrbTypeCmdSetlatancyTol     = 20,
    XhciTrbTypeCmdGetPortBw         = 21,
    XhciTrbTypeCmdForceHeader       = 22,
    XhciTrbTypeCmdNoOp              = 23,
    XhciTrbTypeCmdGetExtendedProperty = 24,
// Event
    XhciTrbTypeEvTransfer           = 32,
    XhciTrbTypeEvCmdComplete        = 33,
    XhciTrbTypeEvPortStatusChg      = 34,
    XhciTrbTypeEvBwReq              = 35,
    XhciTrbTypeEvDoorbell           = 36,
    XhciTrbTypeEvHostCtrl           = 37,
    XhciTrbTypeEvDevNotif           = 38,
    XhciTrbTypeEvMfIndexWrap        = 39,
    // following for device mode
    XhciTrbTypeEvSetupEvent         = 63,
}XhciTrbType;

// enum for completion code in Event TRBs
/*
typedef enum _XhciTrbCompletionCode
{
    XhciTrbCmpCodeIlwalid           = 0,
    XhciTrbCmpCodeSuccess           = 1,
    XhciTrbCmpCodeDataBufErr        = 2,
    XhciTrbCmpCodeBabbleDetectErr   = 3,
    XhciTrbCmpCodeUsbTransactionErr = 4,
    XhciTrbCmpCodeTrbErr            = 5,
    XhciTrbCmpCodeStallErr          = 6,
    XhciTrbCmpCodeResourceErr       = 7,
    XhciTrbCmpCodeBandwidthErr      = 8,
    XhciTrbCmpCodeNoSlotAvailableErr= 9,
    XhciTrbCmpCodeIlwaStreamTypeErr = 10,
    XhciTrbCmpCodeSlotNotEnabledErr = 11,
    XhciTrbCmpCodeEpNotEnabledErr   = 12,
    XhciTrbCmpCodeShortPacket       = 13,
    XhciTrbCmpCodeRingUnderrun      = 14,
    XhciTrbCmpCodeRingOverrun       = 15,
    XhciTrbCmpCodeVFEventRingFull   = 16,
    XhciTrbCmpCodeParameterErr      = 17,
    XhciTrbCmpCodeBwOverrunErr      = 18,
    XhciTrbCmpCodeContextStateErr   = 19,
    XhciTrbCmpCodeNoPingResponse    = 20,
    XhciTrbCmpCodeEventRingFullErr  = 21,
    XhciTrbCmpCodeIncompatibleDevErr= 22,
    XhciTrbCmpCodeMissedServiceErr  = 23,
    XhciTrbCmpCodeCmdRingStopped    = 24,
    XhciTrbCmpCodeCmdAborted        = 25,
    XhciTrbCmpCodeStopped           = 26,
    XhciTrbCmpCodeIlwalidLength     = 27,
    // 28 reserved
    XhciTrbCmpCodeMaxExitLatTooLarge= 29,
    // 30 reserved
    XhciTrbCmpCodeIsoBufOverrun     = 31,
    XhciTrbCmpCodeEventLostErr      = 32,
    XhciTrbCmpCodeUndefinedErr      = 33,
    XhciTrbCmpCodeIlwalidStreamIdErr= 34,
    XhciTrbCmpCodeSecBandwidthErr   = 35,
    XhciTrbCmpCodeSplitTransErr     = 36,
    XhciTrbCmpCodeIlwalidFrameIdErr = 37,
    // following for device mode
    XhciTrbCmpCodeRejStreamSelErr   = 221,
    XhciTrbCmpCodeCtrlDirErr        = 222,
    XhciTrbCmpCodeCtrlSeqNumErr     = 223
}XhciTrbCompletionCode;
*/
#define XHCI_COMPLETION_CODE    \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeIlwalid,0)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeSuccess,1)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeDataBufErr,2)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeBabbleDetectErr,3)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeUsbTransactionErr,4)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeTrbErr,5)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeStallErr,6)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeResourceErr,7)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeBandwidthErr,8)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeNoSlotAvailableErr,9)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeIlwaStreamTypeErr,10)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeSlotNotEnabledErr,11)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeEpNotEnabledErr,12)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeShortPacket,13)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeRingUnderrun,14)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeRingOverrun,15)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeVFEventRingFull,16)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeParameterErr,17)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeBwOverrunErr,18)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeContextStateErr,19)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeNoPingResponse,20)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeEventRingFullErr,21)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeIncompatibleDevErr,22)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeMissedServiceErr,23)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeCmdRingStopped,24)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeCmdAborted,25)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeStopped,26)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeIlwalidLength,27)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeMaxExitLatTooLarge,29)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeIsoBufOverrun,31)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeEventLostErr,32)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeUndefinedErr,33)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeIlwalidStreamIdErr,34)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeSecBandwidthErr,35)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeSplitTransErr,36)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeIlwalidFrameIdErr,37)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeRejStreamSelErr,221)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeCtrlDirErr,222)  \
    DEFINE_XHCI_COMPLETION_CODE(XhciTrbCmpCodeCtrlSeqNumErr,223)  \

#define DEFINE_XHCI_COMPLETION_CODE(tag,id) tag=id,
    typedef enum _XhciTrbCompletionCode
    {
        XHCI_COMPLETION_CODE
    }XhciTrbCompletionCode;
#undef DEFINE_XHCI_COMPLETION_CODE

#define XHCI_TRB_LEN_BYTE                          16
#define XHCI_ALIGNMENT_TRB                         16
#define XHCI_ALIGNMENT_GET_EXTENED_PROPERTY        64

// Common definition
#define XHCI_TRB_DW2_INTR_TARGET                31:22

#define XHCI_TRB_DW3_CYCLE                        0:0
#define XHCI_TRB_DW3_EVAL_NEXT_TRB                1:1
#define XHCI_TRB_DW3_ISP                          2:2
#define XHCI_TRB_DW3_NS                           3:3
#define XHCI_TRB_DW3_CH                           4:4
#define XHCI_TRB_DW3_IOC                          5:5
#define XHCI_TRB_DW3_IDT                          6:6
#define XHCI_TRB_DW3_TYPE                       15:10
#define XHCI_TRB_DW3_EP_ID                      20:16
#define XHCI_TRB_DW3_SLOT_ID                    31:24
#define XHCI_TRB_DW3_BEI                          9:9   // for Normal, Isoch and Event Data TRBs

// command TRB
#define XHCI_TRB_CMD_SET_DEQ_DW0_DCS              0:0
#define XHCI_TRB_CMD_SET_DEQ_DW0_SCT              3:1
#define XHCI_TRB_CMD_SET_DEQ_DW2_STREAM_ID      31:16
#define XHCI_TRB_CMD_CFG_EP_DW3_DC                9:9
#define XHCI_TRB_CMD_RESET_EP_DW3_TSP             9:9
#define XHCI_TRB_CMD_ADDR_DEV_DW3_BSR             9:9
#define XHCI_TRB_CMD_STOP_EP_DW3_SUSPEND        23:23
#define XHCI_TRB_CMD_ENABLE_SLOT_DW3_SLOT_TYPE  20:16

// Control TRB
#define XHCI_TRB_SETUP_DW2_XFER_LENGTH           16:0
#define XHCI_TRB_DATA_DW3_DIR                   16:16
#define XHCI_TRB_DATA_STAGE_DW0_BUF_LO           31:0
#define XHCI_TRB_DATA_STAGE_DW1_BUF_HI           31:0

// Get Port Bandwidth
#define XHCI_TRB_CMD_GET_PORTBW_DW0_ADDR_LO      31:4
#define XHCI_TRB_CMD_GET_PORTBW_DW1_ADDR_HI      31:0
#define XHCI_TRB_CMD_GET_PORTBW_DW3_DEV_SPD     19:16
#define XHCI_TRB_CMD_GET_PORTBW_DW3_HUB_SLOTID  31:24

// Force Header Command TRB
#define XHCI_TRB_CMD_FORCE_HEADER_DW0_TYPE        4:0
#define XHCI_TRB_CMD_FORCE_HEADER_DW0_LO         31:5
#define XHCI_TRB_CMD_FORCE_HEADER_DW1_MID        31:0
#define XHCI_TRB_CMD_FORCE_HEADER_DW2_HI         31:0
#define XHCI_TRB_CMD_FORCE_HEADER_DW3_PORT_NUM  31:24

// Get Extended Property Command TRB
#define XHCI_TRB_CMD_GET_EXTPRO_DW0_ADDR_LO      31:4
#define XHCI_TRB_CMD_GET_EXTPRO_DW1_ADDR_HI      31:0
#define XHCI_TRB_CMD_GET_EXTPRO_DW2_ECI          15:0
#define XHCI_TRB_CMD_GET_EXTPRO_DW3_SLOTID      31:24
#define XHCI_TRB_CMD_GET_EXTPRO_DW3_EPID        23:19
#define XHCI_TRB_CMD_GET_EXTPRO_DW3_SUBTYPE     18:16

// Transfer TRB
#define XHCI_TRB_XFER_DW0_BUF_LO                 31:0
#define XHCI_TRB_XFER_DW1_BUF_HI                 31:0
#define XHCI_TRB_XFER_DW2_LEN                    16:0
#define XHCI_TRB_XFER_DW3_ISP                     2:2
#define XHCI_TRB_XFER_DW2_TD_SIZE               21:17

// Isoch TRB
#define XHCI_TRB_ISOCH_DW3_FRAME_ID             30:20
#define XHCI_TRB_ISOCH_DW3_SIA                  31:31
#define XHCI_TRB_ISOCH_DW3_TLBPC                19:16
#define XHCI_TRB_ISOCH_DW3_TBC                    8:7

// Link TRB
#define XHCI_TRB_LINK_DW0_BUF_LO                 31:4
#define XHCI_TRB_LINK_DW1_BUF_HI                 31:0
#define XHCI_TRB_LINK_DW3_TC                      1:1
#define XHCI_TRB_LINK_DW3_CH                      4:4
#define XHCI_TRB_LINK_DW3_IOC                     5:5

// Event TRB
#define XHCI_TRB_EVENT_DW2_LENGTH                23:0
#define XHCI_TRB_EVENT_DW2_COMPLETION_CODE      31:24
#define XHCI_TRB_EVENT_DW2_CMD_COMPLETION_PARAM  23:0

#define XHCI_TRB_EVENT_XFER_DW3_ED                2:2

#define XHCI_TRB_EVENT_PORT_STATUS_DW0_PORT_ID  31:24

// Event Data TRB
#define XHCI_TRB_EVENT_DATA_DW0_DATA_LO          31:0
#define XHCI_TRB_EVENT_DATA_DW1_DATA_HI          31:0

// Setup Event TRB, from USB3_SW_PG_DeviceMode.doc
#define XHCI_TRB_SETUP_EVENT_DW0_PACKET_DATA_LO  31:0
#define XHCI_TRB_SETUP_EVENT_DW1_PACKET_DATA_HI  31:0
#define XHCI_TRB_SETUP_EVENT_DW2_CTRL_SEQ_NUM    15:0
#define XHCI_TRB_SETUP_EVENT_DW2_COMP_CODE      31:24
#define XHCI_TRB_SETUP_EVENT_DW3_EP_ID          20:16

#define XHCI_TRB_COPY_SRC_IN_STACK                0x1
#define XHCI_TRB_COPY_DEST_IN_STACK               0x2

// The class to deal with TRBs, all functions in this class are static.
// Since they are helper functions without maintaining any internal state
class TrbHelper
{
public:
    //! \brief Return true if the TRB is empty (all 0), used only with TRB in memory but not stack
    //! \param [i] pTrb : the pointer pointing to a TRB
    static bool IsTrbEmpty(XhciTrb *pTrb);

    //! \brief Update specified fileds in any TRB when the TRB is constructed
    //! all the input settings will be saved to m_vTrbUpdateData in format of
    //! [TRB Type, Mask0,1,2,3, Data0,1,2,3, Times]
    //! \param [i] Type : Type of TRB to apply the settings
    //! if Type = 0, all the data in m_vTrbUpdateData will be deleted
    //! \param [i] pMaskData : mask and data in foramt of [Mask0,1,2,4, Data0,1,2,3]
    //! if all elements in the vector = 0, the corresponding entry of Type will be removed from m_vTrbUpdateData
    //! \param [i] Times : how many time should the setting be applied
    static RC UpdateTrb(UINT32 Type, vector<UINT32> *pMaskData, UINT32 Times);

    //! \brief Companion function of UpdateTrb
    //! Read the usert setting from m_vTrbUpdateData to pMaskData
    //! when read Times times or IsDelOnFound =true, delete the entry
    //! \param [i] Type : Type of TRB to retreive data
    //! \param [o] pMaskData : mask and data in foramt of [Mask0,1,2,4, Data0,1,2,3], check its size before use it
    //! \param [i] IsDelOnFound : if = true, the enrty will be deleted when found
    static RC GetTrbOverride(UINT32 Type, vector<UINT32> *pMaskData, bool IsDelOnFound = false);

    //! \brief Get the type of the specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [o] pTrbType : the type of the specified TRB
    //! \param [i] Pri : Printing priority for TRB type's name dumping
    //! \param [i] IsTrbInStack : if the TRB pointed by pTrb is in stack instead of allocated buffer
    static RC GetTrbType(XhciTrb *pTrb,
                         UINT08 * pTrbType,
                         Tee::Priority Pri = Tee::PriSecret,
                         bool IsTrbInStack = false);

    static RC GetTrbType(XhciTrb *pTrb,
                         UINT08 * pTrbType,
                         Tee::PriDebugStub,
                         bool isTrbInStack)
    {
        return GetTrbType(pTrb, pTrbType, Tee::PriSecret, isTrbInStack);
    }

    //! \brief Get the Cycle Bit of the specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [o] pCBit : the Cycle Bit state, 1=true, 0=false
    static RC GetCycleBit(XhciTrb * pTrb, bool * pCBit);

    //! \brief Set the Cycle Bit of the specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [i] CycleBit : the Cycle Bit state, true=1, false=0
    static RC SetCycleBit(XhciTrb *pTrb, bool CycleBit);

    //! \brief Set the Chain Bit of the specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [i] ChainBit : the Chain Bit state, true=1, false=0
    static RC SetChainBit(XhciTrb *pTrb, bool ChainBit);

    //! \brief Set the IOC bit of the specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [i] IocBit : the IOC bit state, true=1, false=0
    static RC SetIocBit(XhciTrb *pTrb, bool IocBit);

    //! \brief Set the Isp bit of the specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [i] IspBit : the Isp bit state, true=1, false=0
    static RC SetIspBit(XhciTrb *pTrb, bool IspBit);

    //! \brief Toggle the CycleBit of the specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    static RC ToggleCycleBit(XhciTrb *pTrb);

    // Spec wrapper, ass also 6.4.1.1 Normal TRB, 6.4.1.2.2 Data Stage TRB, 6.4.1.3 Isoch TRB
    static UINT32 GetMaxXferSizeNormalTrb();
    static UINT32 GetMaxXferSizeDataStageTrb();
    static UINT32 GetMaxXferSizeIsoTrb();

    //! \brief Setup a normal TRB rwt the input parameters
    //! see also Spec 6.4.1.1, Xfer TRBs
    static RC MakeNormalTrb(XhciTrb *pTrb,
                            UINT32 PtrLo,
                            UINT32 PtrHi,
                            UINT32 Length,
                            UINT08 TdSize,
                            UINT08 IntrTarget = 0,
                            bool IsIoc = true,
                            bool IsImmData = false,
                            bool IsChainBit = false,
                            bool IsISP = true,
                            bool IsEvalNextTrb = false,
                            bool IsNs = false,
                            bool IsSetupCycleBit = true);
    //! \brief Parse a normal TRB
    //! retreive its associated buffer address and buffer length
    static RC ParseNormalTrb(XhciTrb * pTrb,
                             PHYSADDR * ppTrbPtr = NULL,
                             UINT32 * pLength = NULL);

    //! \brief Setup TRBs for a control transaction, this ilwolves a setup TRB
    //! a Normal TRB(Data TRB) and a Status TRB. see also Spec 6.4.1.2 Control TRBs
    static RC MakeSetupTrb(XhciTrb *pTrb,
                           UINT08 ReqType,
                           UINT08 Req,
                           UINT16 Value,
                           UINT16 Index,
                           UINT16 Length,
                           UINT08 IntrTarget = 0,
                           bool IsEvalNextTrb = false);
    static RC MakeDataStageTrb(XhciTrb *pTrb,
                               UINT32 PtrLo,
                               UINT32 PtrHi,
                               UINT32 Length,
                               bool IsDataOut,
                               UINT08 IntrTarget = 0,
                               bool IsIoc = false,
                               bool IsChainBit = false,
                               bool IsEvalNextTrb = false,
                               bool IsIdt = false,
                               bool IsNs = false);
    static RC MakeStatusTrb(XhciTrb *pTrb,
                            bool IsDataOut,
                            UINT08 IntrTarget = 0,
                            bool IsIoc = true,
                            bool IsEvalNextTrb = false);

    //! \brief setup Isoch TRB for a data transfer
    static RC MakeIsoTrb(XhciTrb * pTrb,
                         UINT32 PtrLo,
                         UINT32 PtrHi,
                         UINT32 Length,
                         UINT08 TdSize,
                         bool IsSIA = true,
                         UINT16 FrameID = 0,
                         UINT08 IntrTarget = 0,
                         bool IsIoc = true,
                         bool IsImmData = false,
                         bool IsChainBit = false,
                         bool IsISP = true,
                         bool IsEvalNextTrb = false,
                         bool IsNs = false,
                         bool IsSetupCycleBit = true);

    //! \brief setup No Op TRB for Xfer Ring test purpose
    static RC MakeNoOpTrb(XhciTrb *pTrb,
                          UINT08 IntrTarget = 0,
                          bool IsIoc = true,
                          bool IsChainBit = false,
                          bool IsEvalNextTrb = false);

    // ....... For Event TRBs
    //! \brief Parse the Setup TRB recieved by Device Mode
    static RC ParseSetupTrb(XhciTrb* pEventTrb,
                            UINT08* pReqType,
                            UINT08* pReq,
                            UINT16* pValue,
                            UINT16* pIndex,
                            UINT16* pLength,
                            UINT16* pCtrlSeqNum);

    //! \brief Parse the Transfer Event TRB
    static RC ParseXferEvent(XhciTrb * pTrb,
                             UINT08 * pCompletionCode,
                             PHYSADDR * ppTrbPtr = NULL,
                             UINT32 * pLength = NULL,
                             bool * pIsEveData = NULL,
                             UINT08 * pSlotId = NULL,
                             UINT08 * pEndPont = NULL,
                             bool * pCycleBit = NULL);

    //! \brief Parse the Command Completion Event TRB
    static RC ParseCmdCompleteEvent(XhciTrb * pTrb,
                                    UINT08 * pCompletionCode,
                                    PHYSADDR * ppTrbPtr = NULL,
                                    UINT08 * pSlotId = NULL,
                                    bool * pCycleBit = NULL);

    //! \brief Parse the Port Status Change Event TRB
    static RC ParsePortStatusChgEvent(XhciTrb * pTrb,
                                      UINT08 * pCompletionCode = NULL,
                                      UINT08 * pPortId = NULL,
                                      bool * pCycleBit = NULL);

    //! Not implemented yet, will implement on request basis
    static RC ParseBwRequestEvent(){return OK;}
    static RC ParseDoorbellEvent(){return OK;}
    static RC ParseHostCtrlerEvent(){return OK;}
    static RC ParseDevNotifEvent(){return OK;}
    static RC ParseMFIndexWrapEvent(){return OK;}

    //! \brief Retreive the Completion Code from TRB, checks the valid type
    static RC GetCompletionCode(XhciTrb * pTrb,
                                UINT08 * pCompCode,
                                bool IsSilent = false,
                                bool IsTrbInStack = true);

    //! \brief Retreive the Physical Address from XFER and CMD completion event TRBs
    static RC ParseTrbPointer(XhciTrb * pTrb, PHYSADDR *pTRB);

    // ....... For Command TRBs
    //! \brief Setup No Op Command TRB for host test purpose
    static RC SetupCmdNoOp(XhciTrb * pTrb);

    static RC SetupCmdEnableSlot(XhciTrb * pTrb, UINT08 SlotType = 0);
    static RC SetupCmdDisableSlot(XhciTrb * pTrb,
                                  UINT08 SlotId);
    static RC SetupCmdAddrDev(XhciTrb * pTrb,
                              PHYSADDR pInpContext,
                              UINT08 SlotId,
                              bool IsBsr = false);
    static RC SetupCmdCfgEndp(XhciTrb * pTrb,
                              PHYSADDR pInpContext,
                              UINT08 SlotId,
                              bool IsDeCfg = false);
    static RC SetupCmdEvalContext(XhciTrb * pTrb,
                                  PHYSADDR pInpContext,
                                  UINT08 SlotId);
    static RC SetupCmdResetEndp(XhciTrb * pTrb,
                                UINT08 SlotId,
                                UINT08 EndpId,
                                bool IsTSP = true);
    static RC SetupCmdStopEndp(XhciTrb * pTrb,
                               UINT08 SlotId,
                               UINT08 Dci,
                               bool IsSuspend = false);
    static RC SetupCmdSetDeq(XhciTrb * pTrb,
                             PHYSADDR pDeq,
                             bool Ccs,
                             UINT08 SlotId,
                             UINT08 EndpId,
                             UINT16 StreamId,
                             UINT08 Sct);
    static RC SetupCmdResetDev(XhciTrb * pTrb,
                               UINT08 SlotId);
    static RC SetupCmdGetPortBw(XhciTrb * pTrb,
                                PHYSADDR pPortBwContext,
                                UINT08 DevSpeed,
                                UINT08 HubSlotId = 0);
    static RC SetupCmdForceHeader(XhciTrb * pTrb,
                                  UINT32 HeadInfoLo,
                                  UINT32 HeadInfoMid,
                                  UINT32 HeadInfoHi,
                                  UINT08 Type,
                                  UINT08 RootHubPort);
    static RC SetupCmdGetExtendedProperty(XhciTrb * pTrb,
                                          UINT08 SlotId,
                                          UINT08 EndpId,
                                          UINT08 SubType = 0,
                                          UINT16 Eci = 0,
                                          PHYSADDR pExtProContext = 0);

    // ....... Misc TRBs
    //! \brief Setup Link TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [i] pLinkTarget : physical address of the target TRB to link to
    //! \param [i] IsTc : if to set the Toggle Cycle bit
    static RC SetupLinkTrb(XhciTrb * pTrb,
                           PHYSADDR pLinkTarget,
                           bool IsTc);

    //! \brief Get TC bit of the specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [o] pTc : state of TC bit, true = 1; false = 0
    static RC GetTc(XhciTrb * pTrb, bool *pTc);

    //! \brief Set TC bit of the specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [i] IsTc : if to set the Toggle Cycle bit
    static RC SetTc(XhciTrb * pTrb, bool IsTc);

    //! \brief Get the phisical address of the linked Target of a Link TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [o] ppLink : physical address of linked TRB
    static RC GetLinkTarget(XhciTrb * pTrb, PHYSADDR * ppLink);

    //! \brief Setup Event Data TRB
    static RC SetupEventDataTrb(XhciTrb * pTrb,
                                UINT32 EventDataLo,
                                UINT32 EventDataHi,
                                UINT08 IntrTarget = 0,
                                bool IsChainBit = false,
                                //bool IsIoc = true,    // this must be true
                                bool IsEvalNextTrb = false);

    // ....... Helpers
    //! \brief Clear the TRB to all 0s
    //! \param [i] pTrb : the pointer pointing to a TRB
    static RC ClearTrb(XhciTrb * pTrb);     // clear TRB to 0s w/o touch the cyccle bit

    //! \brief Copy source TRB to destiny
    //! \param [i] pDestTrb : the pointer pointing to destiny TRB
    //! \param [i] pSrcTrb : the pointer pointing to source TRB
    //! \param [i] MemInStackMask : to specify the location of TRBs
    //! by default, both source and destiny are not in the stack
    static RC TrbCopy(XhciTrb * pDestTrb, XhciTrb * pSrcTrb,  UINT08 MemInStackMask = 0);

    //! \brief Dump a specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [i] IsDetail : if to print detailed info
    //! \param [i] Pri : print priority
    //! \param [i] IsCr : if to add CR in the printed info
    //! \param [i] IsTrbInStack : if to TRB in stack
    static RC DumpTrbInfo(XhciTrb * pTrb,
                          bool IsDetail = false,
                          Tee::Priority Pri = Tee::PriSecret,
                          bool IsCr = true,
                          bool IsTrbInStack = true);

    static RC DumpTrbInfo(XhciTrb * pTrb,
                          bool isDetail,
                          Tee::PriDebugStub,
                          bool isCr = true,
                          bool isTrbInStack = true)
    {
        return DumpTrbInfo(pTrb, isDetail, Tee::PriSecret, isCr, isTrbInStack);
    }

    //! \brief Dump the raw data of specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [i] Pri : print priority
    //! \param [i] IsCr : if to add CR in the printed info
    //! \param [i] IsTrbInStack : if to TRB in stack
    static RC DumpTrb(XhciTrb * pTrb,
                      Tee::Priority Pri = Tee::PriSecret,
                      bool IsCr = true,
                      bool IsTrbInStack = false);

    static RC DumpTrb(XhciTrb * pTrb,
                      Tee::PriDebugStub,
                      bool isCr = true,
                      bool isTrbInStack = false)
    {
        return DumpTrb(pTrb, Tee::PriSecret, isCr, isTrbInStack);
    }

    static RC PrintCompletionCode(UINT08 CompCode);

private:

    //! \brief Setup the Type field of specified TRB
    //! \param [i] pTrb : the pointer pointing to a TRB
    //! \param [i] Type : TRB type
    //! \param [i] IsSetupCycleBit : if to setup Cycle bit as well
    static RC SetTrbType(XhciTrb * pTrb, XhciTrbType Type, bool IsSetupCycleBit = true);

    //! \brief Check if the given TRB address is valid (lwrrently just check for non-NULL)
    //! \param [i] pTrb : the pointer pointing to a TRB
    static RC Validate(XhciTrb *pTrb);

    //! \brief vector to preserve user-specified hack data, see also UpdateTrb()
    //! one entry consis of 10 UINT32: TrbType, Mask0-3, Data0-3, ApplyTimes
    static  vector<UINT32> m_vTrbUpdateData;
};
#endif

