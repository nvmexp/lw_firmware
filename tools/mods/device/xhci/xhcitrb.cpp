/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tee.h"
#include "core/include/rc.h"
#include "cheetah/include/logmacros.h"
#include "cheetah/include/tegradrf.h"
#include "xhcitrb.h"
#include "core/include/platform.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "XhciTrb"
vector<UINT32> TrbHelper::m_vTrbUpdateData;

RC TrbHelper::MakeNormalTrb(XhciTrb *pTrb,
                            UINT32 PtrLo,
                            UINT32 PtrHi,
                            UINT32 Length,
                            UINT08 TdSize,
                            UINT08 IntrTarget,
                            bool IsIoc,
                            bool IsImmData,
                            bool IsChainBit,
                            bool IsISP,
                            bool IsEvalNextTrb,
                            bool IsNs,
                            bool IsSetupCycleBit)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW0, FLD_SET_RF_NUM(XHCI_TRB_XFER_DW0_BUF_LO, PtrLo, MEM_RD32(&pTrb->DW0)));
    MEM_WR32(&pTrb->DW1, FLD_SET_RF_NUM(XHCI_TRB_XFER_DW1_BUF_HI, PtrHi, MEM_RD32(&pTrb->DW1)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_SETUP_DW2_XFER_LENGTH, Length, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_XFER_DW2_TD_SIZE, TdSize, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_DW2_INTR_TARGET, IntrTarget, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IOC, IsIoc?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IDT, IsImmData?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_CH, IsChainBit?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_XFER_DW3_ISP, IsISP?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_EVAL_NEXT_TRB, IsEvalNextTrb?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_NS, IsNs?1:0, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeNormal, IsSetupCycleBit));

    LOG_EXT();
    return OK;
}

RC TrbHelper::ParseNormalTrb(XhciTrb * pTrb,
                             PHYSADDR * ppTrbPtr,
                             UINT32 * pLength)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    if ( !FLD_TEST_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeNormal, MEM_RD32(&pTrb->DW3))
         && !FLD_TEST_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeIsoch, MEM_RD32(&pTrb->DW3)) )
    {
        Printf(Tee::PriError, "Not Normal/Isoch TRB");
        return RC::BAD_PARAMETER;
    }

    if ( ppTrbPtr )
    {
        *ppTrbPtr = MEM_RD32(&pTrb->DW1);
        *ppTrbPtr = (*ppTrbPtr) << 32;
        *ppTrbPtr |= MEM_RD32(&pTrb->DW0);
    }
    if ( pLength )
        *pLength = RF_VAL(XHCI_TRB_XFER_DW2_LEN, MEM_RD32(&pTrb->DW2));

    LOG_EXT();
    return OK;
}

RC TrbHelper::MakeSetupTrb(XhciTrb *pTrb,
                           UINT08 ReqType,
                           UINT08 Req,
                           UINT16 Value,
                           UINT16 Index,
                           UINT16 Length,
                           UINT08 IntrTarget,
                           bool IsEvalNextTrb)
{
    LOG_ENT(); Printf(Tee::PriDebug,"(ReqType 0x%x, Req 0x%x, Value 0x%x, Index 0x%x, Length 0x%x)", ReqType, Req, Value, Index, Length);
    RC rc;
    CHECK_RC(Validate(pTrb));

    UINT32 data0, data1;
    data0 = ReqType | (Req << 8) | (Value << 16);
    data1 = Index | (Length << 16);

    MEM_WR32(&pTrb->DW0, data0);
    MEM_WR32(&pTrb->DW1, data1);

    UINT08 * pData08 = (UINT08*) &(pTrb->DW0);
    Printf(Tee::PriDebug, "  Setup data: ");
    for ( UINT32 i=0; i < 8; i++ )
    {
        Printf(Tee::PriDebug, "%02x ", MEM_RD08(pData08 + i));
    }
    Printf(Tee::PriDebug, "\n");

    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_SETUP_DW2_XFER_LENGTH, 8, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_DW2_INTR_TARGET, IntrTarget, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_EVAL_NEXT_TRB, IsEvalNextTrb?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IOC, 0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IDT, 1, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeSetup, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeSetup));

    LOG_EXT();
    return OK;
}

RC TrbHelper::MakeDataStageTrb(XhciTrb *pTrb,
                               UINT32 PtrLo,
                               UINT32 PtrHi,
                               UINT32 Length,
                               bool IsDataOut,
                               UINT08 IntrTarget,
                               bool IsIoc,
                               bool IsChainBit,
                               bool IsEvalNextTrb,
                               bool IsIdt,
                               bool IsNs)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW0, FLD_SET_RF_NUM(XHCI_TRB_DATA_STAGE_DW0_BUF_LO, PtrLo, MEM_RD32(&pTrb->DW0)));
    MEM_WR32(&pTrb->DW1, FLD_SET_RF_NUM(XHCI_TRB_DATA_STAGE_DW1_BUF_HI, PtrHi, MEM_RD32(&pTrb->DW1)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_SETUP_DW2_XFER_LENGTH, Length, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DATA_DW3_DIR, IsDataOut?0:1, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_DW2_INTR_TARGET, IntrTarget, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IOC, IsIoc?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_CH, IsChainBit?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_EVAL_NEXT_TRB, IsEvalNextTrb?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IDT, IsIdt?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_NS, IsNs?1:0, MEM_RD32(&pTrb->DW3)));

    LOG_EXT();
    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeData));
    return OK;
}

RC TrbHelper::MakeStatusTrb(XhciTrb *pTrb,
                            bool IsDataOut,
                            UINT08 IntrTarget,
                            bool IsIoc,
                            bool IsEvalNextTrb)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DATA_DW3_DIR, IsDataOut?0:1, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_DW2_INTR_TARGET, IntrTarget, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IOC, IsIoc?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_EVAL_NEXT_TRB, IsEvalNextTrb?1:0, MEM_RD32(&pTrb->DW3)));

    LOG_EXT();
    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeStatus));
    return OK;
}

RC TrbHelper::MakeIsoTrb(XhciTrb * pTrb,
                         UINT32 PtrLo,
                         UINT32 PtrHi,
                         UINT32 Length,
                         UINT08 TdSize,
                         bool IsSIA,
                         UINT16 FrameID,
                         UINT08 IntrTarget,
                         bool IsIoc,
                         bool IsImmData,
                         bool IsChainBit,
                         bool IsISP,
                         bool IsEvalNextTrb,
                         bool IsNs,
                         bool IsSetupCycleBit)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW0, FLD_SET_RF_NUM(XHCI_TRB_XFER_DW0_BUF_LO, PtrLo, MEM_RD32(&pTrb->DW0)));
    MEM_WR32(&pTrb->DW1, FLD_SET_RF_NUM(XHCI_TRB_XFER_DW1_BUF_HI, PtrHi, MEM_RD32(&pTrb->DW1)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_XFER_DW2_LEN, Length, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_XFER_DW2_TD_SIZE, TdSize, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_DW2_INTR_TARGET, IntrTarget, MEM_RD32(&pTrb->DW2)));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_ISOCH_DW3_FRAME_ID, FrameID, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_ISOCH_DW3_SIA, IsSIA?1:0, MEM_RD32(&pTrb->DW3)));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IOC, IsIoc?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IDT, IsImmData?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_CH, IsChainBit?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_XFER_DW3_ISP, IsISP?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_EVAL_NEXT_TRB, IsEvalNextTrb?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_NS, IsNs?1:0, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeIsoch, IsSetupCycleBit));

    LOG_EXT();
    return OK;
}

RC TrbHelper::MakeNoOpTrb(XhciTrb *pTrb,
                          UINT08 IntrTarget,
                          bool IsIoc,
                          bool IsChainBit,
                          bool IsEvalNextTrb)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_DW2_INTR_TARGET, IntrTarget, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IOC, IsIoc?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_CH, IsChainBit?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_EVAL_NEXT_TRB, IsEvalNextTrb?1:0, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeNoOp));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupLinkTrb(XhciTrb * pTrb,
                           PHYSADDR pLinkTarget,
                           bool IsTc)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    if ( pLinkTarget & (XHCI_ALIGNMENT_TRB-1) )
    {
        Printf(Tee::PriError, "Link Target not %d Byte alignment", XHCI_ALIGNMENT_TRB);
        return RC::SOFTWARE_ERROR;
    }

    UINT32 data32 = pLinkTarget;
    MEM_WR32(&pTrb->DW0, FLD_SET_RF_NUM(XHCI_TRB_LINK_DW0_BUF_LO, data32>>(0?XHCI_TRB_LINK_DW0_BUF_LO), MEM_RD32(&pTrb->DW0)));
    pLinkTarget = pLinkTarget >> 32;
    data32 = pLinkTarget;
    MEM_WR32(&pTrb->DW1, FLD_SET_RF_NUM(XHCI_TRB_LINK_DW1_BUF_HI, data32, MEM_RD32(&pTrb->DW1)));

    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_DW2_INTR_TARGET, 0x0, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_LINK_DW3_TC, IsTc?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_LINK_DW3_CH, 0x1, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_LINK_DW3_IOC, 0x0, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeLink, false));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdNoOp(XhciTrb * pTrb)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdNoOp));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdEnableSlot(XhciTrb * pTrb, UINT08 SlotType)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_CMD_ENABLE_SLOT_DW3_SLOT_TYPE, SlotType, MEM_RD32(&pTrb->DW3)));
    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdEnableSlot));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdDisableSlot(XhciTrb * pTrb, UINT08 SlotId)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_SLOT_ID, SlotId, MEM_RD32(&pTrb->DW3)));
    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdDisableSlot));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdAddrDev(XhciTrb * pTrb,
                              PHYSADDR pInpContext,
                              UINT08 SlotId,
                              bool IsBsr)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    MEM_WR32(&pTrb->DW0, (UINT32)pInpContext);
    pInpContext = pInpContext >> 32;
    MEM_WR32(&pTrb->DW1, (UINT32)pInpContext);

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_SLOT_ID, SlotId, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_CMD_ADDR_DEV_DW3_BSR, IsBsr?1:0, MEM_RD32(&pTrb->DW3)));    // block the request
    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdAddrDev));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdCfgEndp(XhciTrb * pTrb,
                              PHYSADDR pInpContext,
                              UINT08 SlotId,
                              bool IsDeCfg)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW0, (UINT32)pInpContext);
    pInpContext = pInpContext >> 32;
    MEM_WR32(&pTrb->DW1, (UINT32)pInpContext);

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_SLOT_ID, SlotId, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_CMD_CFG_EP_DW3_DC, IsDeCfg?1:0, MEM_RD32(&pTrb->DW3)));
    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdCfgEndp));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdEvalContext(XhciTrb * pTrb,
                                  PHYSADDR pInpContext,
                                  UINT08 SlotId)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW0, (UINT32)pInpContext);
    pInpContext = pInpContext >> 32;
    MEM_WR32(&pTrb->DW1, (UINT32)pInpContext);

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_SLOT_ID, SlotId, MEM_RD32(&pTrb->DW3)));
    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdEvalContext));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdResetEndp(XhciTrb * pTrb,
                                UINT08 SlotId,
                                UINT08 EndpId,
                                bool IsTSP)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_SLOT_ID, SlotId, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_EP_ID, EndpId, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_CMD_RESET_EP_DW3_TSP, IsTSP?1:0, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdResetEndp));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdStopEndp(XhciTrb * pTrb,
                               UINT08 SlotId,
                               UINT08 Dci,
                               bool IsSuspend)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_SLOT_ID, SlotId, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_EP_ID, Dci, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_CMD_STOP_EP_DW3_SUSPEND, IsSuspend?1:0, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdStopEndp));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdSetDeq(XhciTrb * pTrb,
                             PHYSADDR pDeq,
                             bool IsDcs,
                             UINT08 SlotId,
                             UINT08 EndpId,
                             UINT16 StreamId,
                             UINT08 Sct)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    if ( pDeq & (XHCI_ALIGNMENT_TRB-1) )
    {
        Printf(Tee::PriError, "Deq pointer not %d Byte alignment", XHCI_ALIGNMENT_TRB);
        return RC::SOFTWARE_ERROR;
    }
    MEM_WR32(&pTrb->DW0, (UINT32)pDeq);
    pDeq = pDeq >> 32;
    MEM_WR32(&pTrb->DW1, (UINT32)pDeq);

    MEM_WR32(&pTrb->DW0, FLD_SET_RF_NUM(XHCI_TRB_CMD_SET_DEQ_DW0_DCS, IsDcs?1:0, MEM_RD32(&pTrb->DW0)));
    MEM_WR32(&pTrb->DW0, FLD_SET_RF_NUM(XHCI_TRB_CMD_SET_DEQ_DW0_SCT, Sct, MEM_RD32(&pTrb->DW0)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_CMD_SET_DEQ_DW2_STREAM_ID, StreamId, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_SLOT_ID, SlotId, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_EP_ID, EndpId, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdSetTrDeqPtr));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdResetDev(XhciTrb * pTrb, UINT08 SlotId)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_SLOT_ID, SlotId, MEM_RD32(&pTrb->DW3)));
    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdResetDev));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdGetPortBw(XhciTrb * pTrb,
                                PHYSADDR pPortBwContext,
                                UINT08 DevSpeed,
                                UINT08 HubSlotId)
{
    LOG_ENT();
    RC rc;
    if ( pPortBwContext & (XHCI_ALIGNMENT_TRB-1) )
    {
        Printf(Tee::PriError, "Port Bandwidth Context not %d Byte alignment", XHCI_ALIGNMENT_TRB);
        return RC::SOFTWARE_ERROR;
    }

    UINT32 data32 = pPortBwContext;
    MEM_WR32(&pTrb->DW0, FLD_SET_RF_NUM(XHCI_TRB_CMD_GET_PORTBW_DW0_ADDR_LO, data32>>(0?XHCI_TRB_CMD_GET_PORTBW_DW0_ADDR_LO), MEM_RD32(&pTrb->DW0)));
    pPortBwContext = pPortBwContext >> 32;
    data32 = pPortBwContext;
    MEM_WR32(&pTrb->DW1, FLD_SET_RF_NUM(XHCI_TRB_CMD_GET_PORTBW_DW1_ADDR_HI, data32, MEM_RD32(&pTrb->DW1)));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_CMD_GET_PORTBW_DW3_DEV_SPD, DevSpeed, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_CMD_GET_PORTBW_DW3_HUB_SLOTID, HubSlotId, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdGetPortBw));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdGetExtendedProperty(XhciTrb * pTrb,
                                          UINT08 SlotId,
                                          UINT08 EndpId,
                                          UINT08 SubType /* = 0*/,
                                          UINT16 Eci /* = 0*/,
                                          PHYSADDR pExtProContext /* = NULL*/)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    if ( pExtProContext & (XHCI_ALIGNMENT_GET_EXTENED_PROPERTY-1) )
    {
        Printf( Tee::PriError,
                "Extended Property Context not %d Byte alignment",
                XHCI_ALIGNMENT_GET_EXTENED_PROPERTY);
        return RC::SOFTWARE_ERROR;
    }

    // Extended Property Context Pointer
    UINT32 data32 = pExtProContext;
    MEM_WR32(&pTrb->DW0, FLD_SET_RF_NUM(XHCI_TRB_CMD_GET_EXTPRO_DW0_ADDR_LO, data32>>(0?XHCI_TRB_CMD_GET_EXTPRO_DW0_ADDR_LO), MEM_RD32(&pTrb->DW0)));
    pExtProContext = pExtProContext >> 32;
    data32 = pExtProContext;
    MEM_WR32(&pTrb->DW1, FLD_SET_RF_NUM(XHCI_TRB_CMD_GET_EXTPRO_DW1_ADDR_HI, data32, MEM_RD32(&pTrb->DW1)));

    // Extended Capability Identifier / Slot ID / Endpoint ID / Sub Type
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_CMD_GET_EXTPRO_DW2_ECI,     Eci,     MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_CMD_GET_EXTPRO_DW3_SLOTID,  SlotId,  MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_CMD_GET_EXTPRO_DW3_EPID,    EndpId,  MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_CMD_GET_EXTPRO_DW3_SUBTYPE, SubType, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdGetExtendedProperty));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupCmdForceHeader(XhciTrb * pTrb,
                                  UINT32 HeadInfoLo,
                                  UINT32 HeadInfoMid,
                                  UINT32 HeadInfoHi,
                                  UINT08 Type,
                                  UINT08 RootHubPort)
{
    LOG_ENT();
    RC rc;

    MEM_WR32(&pTrb->DW0, FLD_SET_RF_NUM(XHCI_TRB_CMD_FORCE_HEADER_DW0_LO, HeadInfoLo>>(0?XHCI_TRB_CMD_FORCE_HEADER_DW0_LO), MEM_RD32(&pTrb->DW0)));
    MEM_WR32(&pTrb->DW1, FLD_SET_RF_NUM(XHCI_TRB_CMD_FORCE_HEADER_DW1_MID, HeadInfoMid>>(0?XHCI_TRB_CMD_FORCE_HEADER_DW1_MID), MEM_RD32(&pTrb->DW1)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_CMD_FORCE_HEADER_DW2_HI, HeadInfoHi>>(0?XHCI_TRB_CMD_FORCE_HEADER_DW2_HI), MEM_RD32(&pTrb->DW2)));

    MEM_WR32(&pTrb->DW0, FLD_SET_RF_NUM(XHCI_TRB_CMD_FORCE_HEADER_DW0_TYPE, Type, MEM_RD32(&pTrb->DW0)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_CMD_FORCE_HEADER_DW3_PORT_NUM, RootHubPort, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeCmdForceHeader));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetupEventDataTrb(XhciTrb * pTrb,
                                UINT32 EventDataLo,
                                UINT32 EventDataHi,
                                UINT08 IntrTarget,
                                bool IsChainBit,
                                bool IsEvalNextTrb)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW0, FLD_SET_RF_NUM(XHCI_TRB_EVENT_DATA_DW0_DATA_LO, EventDataLo, MEM_RD32(&pTrb->DW0)));
    MEM_WR32(&pTrb->DW1, FLD_SET_RF_NUM(XHCI_TRB_EVENT_DATA_DW1_DATA_HI, EventDataHi, MEM_RD32(&pTrb->DW1)));
    MEM_WR32(&pTrb->DW2, FLD_SET_RF_NUM(XHCI_TRB_DW2_INTR_TARGET, IntrTarget, MEM_RD32(&pTrb->DW2)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_CH, IsChainBit?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_EVAL_NEXT_TRB, IsEvalNextTrb?1:0, MEM_RD32(&pTrb->DW3)));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IOC, 1, MEM_RD32(&pTrb->DW3)));

    CHECK_RC(SetTrbType(pTrb, XhciTrbTypeEventData));

    LOG_EXT();
    return OK;
}

RC TrbHelper::GetTc(XhciTrb * pTrb, bool *pTc)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    if ( !pTc )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( !FLD_TEST_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeLink, MEM_RD32(&pTrb->DW3)) )
    {
        Printf(Tee::PriError, "GetTc from a non-Link TRB");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 tc = RF_VAL(XHCI_TRB_LINK_DW3_TC, MEM_RD32(&pTrb->DW3));
    *pTc = (tc != 0);

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetTc(XhciTrb * pTrb, bool Tc)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    if ( !FLD_TEST_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeLink, MEM_RD32(&pTrb->DW3)) )
    {
        Printf(Tee::PriError, "SetTc to a non-Link TRB");
        return RC::SOFTWARE_ERROR;
    }

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_LINK_DW3_TC, Tc?1:0, MEM_RD32(&pTrb->DW3)));

    LOG_EXT();
    return OK;
}

RC TrbHelper::GetLinkTarget(XhciTrb * pTrb, PHYSADDR * ppLink)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    if ( !ppLink )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( !FLD_TEST_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeLink, MEM_RD32(&pTrb->DW3)) )
    {
        Printf(Tee::PriError, "GetLinkTarget from a non-Link TRB");
        return RC::SOFTWARE_ERROR;
    }

    PHYSADDR pPa = MEM_RD32(&pTrb->DW1);
    pPa = pPa << 32;
    pPa |= MEM_RD32(&pTrb->DW0);
    *ppLink = pPa;

    LOG_EXT();
    return OK;
}

RC TrbHelper::ClearTrb(XhciTrb * pTrb)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW0, 0);
    MEM_WR32(&pTrb->DW1, 0);
    MEM_WR32(&pTrb->DW2, 0);
    UINT32 tmp = RF_VAL(XHCI_TRB_DW3_CYCLE, MEM_RD32(&pTrb->DW3));
    MEM_WR32(&pTrb->DW3, RF_NUM(XHCI_TRB_DW3_CYCLE, tmp));

    LOG_EXT();
    return OK;
}

RC TrbHelper::TrbCopy(XhciTrb * pDestTrb, XhciTrb * pSrcTrb, UINT08 MemInStackMask)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pDestTrb));
    CHECK_RC(Validate(pSrcTrb));
    UINT32 dw0, dw1, dw2, dw3;

    if ( MemInStackMask & XHCI_TRB_COPY_SRC_IN_STACK )
    {   // read from stack
        dw0 = pSrcTrb->DW0;
        dw1 = pSrcTrb->DW1;
        dw2 = pSrcTrb->DW2;
        dw3 = pSrcTrb->DW3;
    }
    else
    {   // read from heap
        dw0 = MEM_RD32(&pSrcTrb->DW0);
        dw1 = MEM_RD32(&pSrcTrb->DW1);
        dw2 = MEM_RD32(&pSrcTrb->DW2);
        dw3 = MEM_RD32(&pSrcTrb->DW3);
    }

    if ( MemInStackMask & XHCI_TRB_COPY_DEST_IN_STACK )
    {
        pDestTrb->DW0 = dw0;
        pDestTrb->DW1 = dw1;
        pDestTrb->DW2 = dw2;
        pDestTrb->DW3 = dw3;
    }
    else
    {
        MEM_WR32(&pDestTrb->DW0, dw0);
        MEM_WR32(&pDestTrb->DW1, dw1);
        MEM_WR32(&pDestTrb->DW2, dw2);
        MEM_WR32(&pDestTrb->DW3, dw3);
    }

    LOG_EXT();
    return OK;
}

RC TrbHelper::Validate(XhciTrb *pTrb)
{
    if ( !pTrb )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

bool TrbHelper::IsTrbEmpty(XhciTrb *pTrb)
{
    // clear the Cycle bit
    UINT32 temp = FLD_SET_RF_NUM(XHCI_TRB_DW3_CYCLE, 0, MEM_RD32(&pTrb->DW3));
    temp |= MEM_RD32(&pTrb->DW0)|MEM_RD32(&pTrb->DW1)|MEM_RD32(&pTrb->DW2);
    return (temp == 0);
}

RC TrbHelper::GetTrbType(XhciTrb *pTrb, UINT08 * pTrbType, Tee::Priority Pri, bool IsTrbInStack)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    UINT08 trbType = 0;

    if ( !IsTrbInStack )
    {
        trbType = RF_VAL(XHCI_TRB_DW3_TYPE, MEM_RD32(&pTrb->DW3));
    }
    else
    {
        trbType = RF_VAL(XHCI_TRB_DW3_TYPE, pTrb->DW3);
    }

    switch ( trbType )
    {
    case XhciTrbTypeReserved:
        Printf(Pri, "Empty TRB");
        break;
    case XhciTrbTypeNormal:
        Printf(Pri, "Normal TRB");
        break;
    case XhciTrbTypeSetup:
        Printf(Pri, "Setup Stage TRB");
        break;
    case XhciTrbTypeData:
        Printf(Pri, "Data Stage TRB");
        break;
    case XhciTrbTypeStatus:
        Printf(Pri, "Status Stage TRB");
        break;
    case XhciTrbTypeIsoch:
        Printf(Pri, "Isoch TRB");
        break;
    case XhciTrbTypeLink:
        Printf(Pri, "Link TRB");
        break;
    case XhciTrbTypeEventData:
        Printf(Pri, "Event Data TRB");
        break;
    case XhciTrbTypeNoOp:
        Printf(Pri, "No op TRB");
        break;
    case XhciTrbTypeCmdEnableSlot:
        Printf(Pri, "Enable Slot CMD TRB");
        break;
    case XhciTrbTypeCmdDisableSlot:
        Printf(Pri, "Disable Slot CMD TRB");
        break;
    case XhciTrbTypeCmdAddrDev:
        Printf(Pri, "Address Device CMD TRB");
        break;
    case XhciTrbTypeCmdCfgEndp:
        Printf(Pri, "Cfg Endpoint CMD TRB");
        break;
    case XhciTrbTypeCmdEvalContext:
        Printf(Pri, "Eval Context CMD TRB");
        break;
    case XhciTrbTypeCmdResetEndp:
        Printf(Pri, "Reset Endpoint TRB");
        break;
    case XhciTrbTypeCmdStopEndp:
        Printf(Pri, "Stop Endpoint CMD TRB");
        break;
    case XhciTrbTypeCmdSetTrDeqPtr:
        Printf(Pri, "Set TR Deq Pointer CMD TRB");
        break;
    case XhciTrbTypeCmdResetDev:
        Printf(Pri, "Reset Device CMD TRB");
        break;
    case XhciTrbTypeCmdForceEvent:
        Printf(Pri, "Force Event CMD TRB");
        break;
    case XhciTrbTypeCmdNegoBW:
        Printf(Pri, "Nego Bandwidth CMD TRB");
        break;
    case XhciTrbTypeCmdSetlatancyTol:
        Printf(Pri, "Set LTV CMD TRB");
        break;
    case XhciTrbTypeCmdGetPortBw:
        Printf(Pri, "Get Port Bandwidth CMD TRB");
        break;
    case XhciTrbTypeCmdForceHeader:
        Printf(Pri, "Force Header CMD TRB");
        break;
    case XhciTrbTypeCmdNoOp:
        Printf(Pri, "No op CMD TRB");
        break;
    case XhciTrbTypeCmdGetExtendedProperty:
        Printf(Pri, "Get extended property CMD TRB");
        break;
    case XhciTrbTypeEvTransfer:
        Printf(Pri, "Transfer Event TRB");
        break;
    case XhciTrbTypeEvCmdComplete:
        Printf(Pri, "Command Completion Event TRB");
        break;
    case XhciTrbTypeEvPortStatusChg:
        Printf(Pri, "Port Status Change Event TRB");
        break;
    case XhciTrbTypeEvBwReq:
        Printf(Pri, "Bandwidth Request Event TRB");
        break;
    case XhciTrbTypeEvDoorbell:
        Printf(Pri, "Doorbell Event TRB");
        break;
    case XhciTrbTypeEvHostCtrl:
        Printf(Pri, "Host Controller Event TRB");
        break;
    case XhciTrbTypeEvDevNotif:
        Printf(Pri, "Device Notification Event TRB");
        break;
    case XhciTrbTypeEvMfIndexWrap:
        Printf(Pri, "MFINDEX Wrap Event TRB");
        break;
    case XhciTrbTypeEvSetupEvent:
        Printf(Pri, "Setup Event TRB");
        break;
    default:
        Printf(Tee::PriError, "Bad TRB type %d", trbType);
        return RC::HW_STATUS_ERROR;
    }

    if ( pTrbType )
    {
        *pTrbType = trbType;
    }

    LOG_EXT();
    return OK;
}

RC TrbHelper::GetCycleBit(XhciTrb * pTrb, bool * pCBit)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    if ( !pCBit )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    * pCBit = RF_VAL(XHCI_TRB_DW3_CYCLE, MEM_RD32(&pTrb->DW3)) == 1;

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetCycleBit(XhciTrb *pTrb, bool IsCycleBit)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_CYCLE, IsCycleBit?1:0, MEM_RD32(&pTrb->DW3)));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetChainBit(XhciTrb *pTrb, bool IsChainBit)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_CH, IsChainBit?1:0, MEM_RD32(&pTrb->DW3)));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetIocBit(XhciTrb *pTrb, bool IsIocBit)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_IOC, IsIocBit?1:0, MEM_RD32(&pTrb->DW3)));

    LOG_EXT();
    return OK;
}

RC TrbHelper::SetIspBit(XhciTrb *pTrb, bool IsIspBit)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    UINT08 type;
    CHECK_RC(GetTrbType(pTrb, &type));
    if ( (type != XhciTrbTypeNormal)
         && (type != XhciTrbTypeData)
         && (type != XhciTrbTypeIsoch) )
    {
        Printf(Tee::PriError, "Bad TRB type for ISP bit toggle");
        return RC::BAD_PARAMETER;
    }

    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_ISP, IsIspBit?1:0, MEM_RD32(&pTrb->DW3)));

    LOG_EXT();
    return OK;
}

RC TrbHelper::ToggleCycleBit(XhciTrb *pTrb)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));

    UINT32 cBit = RF_VAL(XHCI_TRB_DW3_CYCLE, MEM_RD32(&pTrb->DW3));
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_CYCLE, (cBit==0)?1:0, MEM_RD32(&pTrb->DW3)));

    LOG_EXT();
    return OK;
}

UINT32 TrbHelper::GetMaxXferSizeNormalTrb()
{
    return 1<<16;
}

UINT32 TrbHelper::GetMaxXferSizeDataStageTrb()
{
    return 1<<16;
}

UINT32 TrbHelper::GetMaxXferSizeIsoTrb()
{
    return 1<<16;
}

// this function is always the last one to be called when constructing a TRB
RC TrbHelper::SetTrbType(XhciTrb * pTrb, XhciTrbType Type, bool IsSetupCycleBit/* = true*/)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(Type = %d)", Type);

    RC rc;
    MEM_WR32(&pTrb->DW3, FLD_SET_RF_NUM(XHCI_TRB_DW3_TYPE, Type, MEM_RD32(&pTrb->DW3)));
    if ( IsSetupCycleBit )
    {
        ToggleCycleBit(pTrb);
    }
    // read back the data to avoid race between TRB setup and Doorbell ringing
    UINT32 dummy = MEM_RD32(&pTrb->DW0)| MEM_RD32(&pTrb->DW1) | MEM_RD32(&pTrb->DW2) | MEM_RD32(&pTrb->DW3);
    if ( !dummy )
    {
        Printf(Tee::PriError, "This should never happen %x \n", dummy);
    }

    // retrieve user's hack if possible
    vector<UINT32> vMaskData;
    CHECK_RC(GetTrbOverride(Type, &vMaskData));
    if (vMaskData.size() == 8)
    {   // apply the hack DWORD by DWORD
        UINT32 data32;
        data32 = MEM_RD32(&pTrb->DW0);
        data32 &= ~vMaskData[0];
        data32 |= vMaskData[4] & vMaskData[0];
        MEM_WR32(&pTrb->DW0, data32);

        data32 = MEM_RD32(&pTrb->DW1);
        data32 &= ~vMaskData[1];
        data32 |= vMaskData[5] & vMaskData[1];
        MEM_WR32(&pTrb->DW1, data32);

        data32 = MEM_RD32(&pTrb->DW2);
        data32 &= ~vMaskData[2];
        data32 |= vMaskData[6] & vMaskData[2];
        MEM_WR32(&pTrb->DW2, data32);

        data32 = MEM_RD32(&pTrb->DW3);
        data32 &= ~vMaskData[3];
        data32 |= vMaskData[7] & vMaskData[3];
        MEM_WR32(&pTrb->DW3, data32);

        Printf(Tee::PriNormal, "TRB type %d hack applied: \n", Type);
        for (UINT32 i = 0; i < 4; i++)
        {
            if (vMaskData[i])
            {
                Printf(Tee::PriNormal, "  DWord %d, Mask 0x%x, Data %0x \n", i, vMaskData[i], vMaskData[i+4] & vMaskData[i]);
            }
        }
    }

    CHECK_RC(DumpTrbInfo(pTrb, true, Tee::PriLow, true, false));

    LOG_EXT();
    return OK;
}

RC TrbHelper::DumpTrbInfo(XhciTrb * pTrb, bool IsDetail, Tee::Priority Pri, bool IsCr, bool IsTrbInStack)
{
    RC rc;
    UINT08 trbType;
    CHECK_RC(GetTrbType(pTrb, &trbType, Pri, IsTrbInStack));
    if ( !IsTrbInStack )
    {   // print address info if TRB is in one of the Rings
        Printf(Pri, " @ (virt) %p  ", pTrb);
    }

    if ( IsCr )
    {
        Printf(Pri, "\n  ");
    }
    if ( IsDetail )
    {
        DumpTrb(pTrb, Pri, IsCr, IsTrbInStack);
    }

    return OK;
}

RC TrbHelper::DumpTrb(XhciTrb * pTrb, Tee::Priority Pri, bool IsCr, bool IsTrbInStack)
{
    if ( !IsTrbInStack )
    {
        Printf(Pri, "0x%08x  0x%08x  0x%08x  0x%08x",
               MEM_RD32(&pTrb->DW0), MEM_RD32(&pTrb->DW1),
               MEM_RD32(&pTrb->DW2), MEM_RD32(&pTrb->DW3));
    }
    else
    {
        Printf(Pri, "0x%08x  0x%08x  0x%08x  0x%08x",
               pTrb->DW0, pTrb->DW1,
               pTrb->DW2, pTrb->DW3);
    }
    if ( IsCr )
    {
        Printf(Pri, "\n");
    }
    return OK;
}

// Event TRBs
//==============================================================================
//todo: define RF macros for Setup TRB
RC TrbHelper::ParseSetupTrb(XhciTrb* pEventTrb,
                            UINT08* pReqType,
                            UINT08* pReq,
                            UINT16* pValue,
                            UINT16* pIndex,
                            UINT16* pLength,
                            UINT16* pCtrlSeqNum)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pEventTrb));
    if ( !FLD_TEST_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeEvSetupEvent, pEventTrb->DW3))
    {
        Printf(Tee::PriError, "Not Setup Event TRB");
        return RC::BAD_PARAMETER;
    }
    //    data0 = ReqType | (Req << 8) | (Value << 16);
    //    data1 = Index | (Length << 16);

    if ( pReqType )
    {
        *pReqType = pEventTrb->DW0;
    }
    if ( pReq )
    {
        *pReq = pEventTrb->DW0>>8;
    }
    if ( pValue)
    {
        *pValue = pEventTrb->DW0>>16;
    }
    if ( pIndex)
    {
        *pIndex = pEventTrb->DW1;
    }
    if ( pLength)
    {
        *pLength = pEventTrb->DW1>>16;
    }
    if ( pCtrlSeqNum)
    {
        *pCtrlSeqNum = pEventTrb->DW2;
    }

    LOG_EXT();
    return OK;
}

RC TrbHelper::ParseXferEvent(XhciTrb * pTrb,
                             UINT08 * pCompletionCode,
                             PHYSADDR * ppTrbPtr,
                             UINT32 * pLength,
                             bool * pIsEveData,
                             UINT08 * pSlotId,
                             UINT08 * pEndPont,
                             bool * pCycleBit)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    if ( !FLD_TEST_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeEvTransfer, pTrb->DW3) )
    {
        Printf(Tee::PriError, "Not Xfer Event \n");
        DumpTrbInfo(pTrb, true, Tee::PriNormal);
        return RC::SOFTWARE_ERROR;
    }

    if ( !pCompletionCode )
    {
        Printf(Tee::PriError, "Null Pointer for Completion Code");
        return RC::SOFTWARE_ERROR;
    }
    *pCompletionCode = RF_VAL(XHCI_TRB_EVENT_DW2_COMPLETION_CODE, pTrb->DW2);
    if ( ppTrbPtr )
    {
        *ppTrbPtr = pTrb->DW1;
        *ppTrbPtr = (*ppTrbPtr) << 32;
        *ppTrbPtr |= pTrb->DW0;
    }
    if ( pLength )
        *pLength = RF_VAL(XHCI_TRB_EVENT_DW2_LENGTH, pTrb->DW2);
    if ( pIsEveData )
        *pIsEveData = RF_VAL(XHCI_TRB_EVENT_XFER_DW3_ED, pTrb->DW3);
    if ( pSlotId )
        *pSlotId = RF_VAL(XHCI_TRB_DW3_SLOT_ID, pTrb->DW3);
    if ( pEndPont )
        *pEndPont = RF_VAL(XHCI_TRB_DW3_EP_ID, pTrb->DW3);
    if ( pCycleBit )
        *pCycleBit = (RF_VAL(XHCI_TRB_DW3_CYCLE, pTrb->DW3) != 0);

    LOG_EXT();
    return OK;
}

RC TrbHelper::ParseCmdCompleteEvent(XhciTrb * pTrb,
                                    UINT08 * pCompletionCode,
                                    PHYSADDR * ppTrbPtr,
                                    UINT08 * pSlotId,
                                    bool * pCycleBit)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    if ( !FLD_TEST_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeEvCmdComplete, pTrb->DW3) )
    {
        Printf(Tee::PriError, "Not CMD Completion Event");
        return RC::SOFTWARE_ERROR;
    }

    if ( !pCompletionCode )
    {
        Printf(Tee::PriError, "Null Pointer for Completion Code");
        return RC::SOFTWARE_ERROR;
    }
    *pCompletionCode = RF_VAL(XHCI_TRB_EVENT_DW2_COMPLETION_CODE, pTrb->DW2);
    if ( ppTrbPtr )
    {
        *ppTrbPtr = pTrb->DW1;
        *ppTrbPtr = (*ppTrbPtr) << 32;
        *ppTrbPtr |= pTrb->DW0;
    }
    if ( pSlotId )
        *pSlotId = RF_VAL(XHCI_TRB_DW3_SLOT_ID, pTrb->DW3);
    if ( pCycleBit )
        *pCycleBit = (RF_VAL(XHCI_TRB_DW3_CYCLE, pTrb->DW3) != 0);

    LOG_EXT();
    return OK;
}

RC TrbHelper::ParsePortStatusChgEvent(XhciTrb * pTrb,
                                      UINT08 * pCompletionCode,
                                      UINT08 * pPortId,
                                      bool * pCycleBit)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    if ( !FLD_TEST_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeEvPortStatusChg, pTrb->DW3) )
    {
        Printf(Tee::PriError, "Not Port Status Change Event");
        return RC::SOFTWARE_ERROR;
    }
    if ( pCompletionCode )
        *pCompletionCode = RF_VAL(XHCI_TRB_EVENT_DW2_COMPLETION_CODE, pTrb->DW2);
    if ( pPortId )
        *pPortId = RF_VAL(XHCI_TRB_EVENT_PORT_STATUS_DW0_PORT_ID, pTrb->DW0);
    if ( pCycleBit )
        *pCycleBit = (RF_VAL(XHCI_TRB_DW3_CYCLE, pTrb->DW3)!=0);

    LOG_EXT();
    return OK;
}

RC TrbHelper::GetCompletionCode(XhciTrb * pTrb,
                                UINT08 * pCompletionCode,
                                bool IsSilent,
                                bool IsTrbInStack)
{
    LOG_ENT();
    if (!pCompletionCode)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pCompletionCode = XhciTrbCmpCodeIlwalid;

    RC rc;
    CHECK_RC(Validate(pTrb));

    UINT32 dWord2;
    if ( !IsTrbInStack )
    {
        dWord2 = MEM_RD32(&pTrb->DW2);
    }
    else
    {
        dWord2 = pTrb->DW2;
    }
    // validate TYB type
    UINT08 trbType;
    CHECK_RC(GetTrbType(pTrb, &trbType, Tee::PriDebug, IsTrbInStack));
    if ( XhciTrbTypeEvTransfer != trbType
         && XhciTrbTypeEvCmdComplete != trbType
         && XhciTrbTypeEvPortStatusChg != trbType
         && XhciTrbTypeEvBwReq != trbType
         && XhciTrbTypeEvDoorbell != trbType
         && XhciTrbTypeEvHostCtrl != trbType
         && XhciTrbTypeEvDevNotif != trbType
         && XhciTrbTypeEvMfIndexWrap != trbType )
    {
        if ( !IsSilent )
        {
            Printf(Tee::PriError, "TRB type %d doesn't have Completion Code field", trbType);
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    *pCompletionCode = RF_VAL(XHCI_TRB_EVENT_DW2_COMPLETION_CODE, dWord2);
    LOG_EXT();
    return OK;
}

RC TrbHelper::ParseTrbPointer(XhciTrb * pTrb, PHYSADDR *pPhysTrb)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(Validate(pTrb));
    if ( !FLD_TEST_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeEvTransfer, pTrb->DW3)
         &&!FLD_TEST_RF_NUM(XHCI_TRB_DW3_TYPE, XhciTrbTypeEvCmdComplete, pTrb->DW3)
       )
    {
        Printf(Tee::PriError, "%d not TRB type with Trb Pointer", RF_VAL(XHCI_TRB_DW3_TYPE, pTrb->DW3));
        return RC::UNSUPPORTED_FUNCTION;
    }

    *pPhysTrb = pTrb->DW1;
    *pPhysTrb = (*pPhysTrb)<<32;
    *pPhysTrb |= pTrb->DW0;

    LOG_EXT();
    return OK;
}

RC TrbHelper::UpdateTrb(UINT32 Type, vector<UINT32> *pMaskData, UINT32 Times)
{   // save the Mask and Data that user input
    LOG_ENT();
    RC rc;
    UINT32 setLen = 10;
    if (0 == Type)
    {
        m_vTrbUpdateData.clear();
        Printf(Tee::PriNormal, "All user TRB settings deleted");
        return OK;
    }

    if ( (!pMaskData)
        || (pMaskData->size()!= 8) )
    {
        Printf(Tee::PriError, "Null Pointer or Not 8 Bytes in size");
        return RC::BAD_PARAMETER;
    }
    // delete the existing deplicated item if there's any
    vector<UINT32> vDummy;
    CHECK_RC(GetTrbOverride(Type, &vDummy, true));

    m_vTrbUpdateData.push_back(Type);
    UINT32 maskAll = 0;
    for (UINT32 i = 0; i < 8; i++)
    {
        m_vTrbUpdateData.push_back((*pMaskData)[i]);
        maskAll |= (*pMaskData)[i];
    }
    m_vTrbUpdateData.push_back(Times);

    if (0 == maskAll)
    { // if user put all 0, remove it
        CHECK_RC(GetTrbOverride(Type, &vDummy, true));
    }

    // Dump it out
    for (UINT32 i = 0; i < m_vTrbUpdateData.size(); i = i + setLen)
    {
        Printf(Tee::PriNormal, "%d. Type: %u, Mask: 0x%x,0x%x,0x%x,0x%x, Data: 0x%x,0x%x,0x%x,0x%x, Times: %u \n", (i/setLen),
               m_vTrbUpdateData[i+0], m_vTrbUpdateData[i+1], m_vTrbUpdateData[i+2], m_vTrbUpdateData[i+3], m_vTrbUpdateData[i+4],
               m_vTrbUpdateData[i+5], m_vTrbUpdateData[i+6], m_vTrbUpdateData[i+7], m_vTrbUpdateData[i+8], m_vTrbUpdateData[i+9]);
    }

    LOG_EXT();
    return OK;
}

// this the look up function for UpdateTrb(), it handle the Times decrement
RC TrbHelper::GetTrbOverride(UINT32 Type, vector<UINT32> *pMaskData, bool IsDelOnFound)
{
    LOG_ENT();
    UINT32 setLen = 10;
    if (!pMaskData)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    pMaskData->clear();
    for (UINT32 i = 0; i < m_vTrbUpdateData.size(); i = i + setLen)
    {
        if (m_vTrbUpdateData[i] == Type)
        {   // found the specified type
            for (UINT32 j = 0; j < 8; j++)
            {   // copy it to return value
                pMaskData->push_back(m_vTrbUpdateData[1 + j]);
            }
            if (m_vTrbUpdateData[i + setLen - 1] || IsDelOnFound)
            {   // not infinite, do the decrement
                m_vTrbUpdateData[i + setLen - 1]--;        // dec the Times
                if ((m_vTrbUpdateData[i + setLen - 1] == 0) || IsDelOnFound)
                {   // remove this item (all 10 Bytes)
                    UINT32 byteCount = setLen;
                    while(byteCount--)
                    {
                        m_vTrbUpdateData.erase(m_vTrbUpdateData.begin() + i);
                    }
                }
            }
        }
    }

    LOG_EXT();
    return OK;
}

RC TrbHelper::PrintCompletionCode(UINT08 CompCode)
{
    switch ( CompCode )
    {
    #define DEFINE_XHCI_COMPLETION_CODE(tag,id) case id:Printf(Tee::PriNormal, "%s",#tag);break;
       XHCI_COMPLETION_CODE
       default:
           Printf(Tee::PriNormal, "undefined Completion Code");
    #undef DEFINE_XHCI_COMPLETION_CODE
    }
    return OK;
}

