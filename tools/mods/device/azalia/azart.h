/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007,2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_AZART_H
#define INCLUDED_AZART_H

#include "azadmae.h"
#include <vector>

class AzaliaCodec;
class AzaliaFormat;
class AzaliaDmaEngine;

class AzaliaRoute
{
public:
    AzaliaRoute(AzaliaCodec* pCodec);
    ~AzaliaRoute();

    RC AddNode(UINT16 NodeID);
    RC Reverse();
    UINT16 LastNode();
    AzaliaCodec* GetCodec() { return m_pCodec; }

    RC WireRoute();
    RC PrintInfo(Tee::Priority Pri);
    RC PrintInfo(Tee::PriDebugStub)
    {
        return PrintInfo(Tee::PriSecret);
    }
    bool IsValidRoute();
    bool operator<(const AzaliaRoute& Other) const;

    RC Reserve();
    void Release();
    bool IsReserved();

    bool Overlaps(AzaliaRoute* pOther);
    bool Contains(UINT16 NodeId);
    bool IsInput() const;
    bool IsJackSenseCapable() const;
    bool IsHdmiCapable();
    bool IsDigitalPin();

    RC Configure(UINT32 StreamIndex, UINT32 ChannelIndex, AzaliaFormat* pFormat,
                 AzaliaDmaEngine::STRIPE_LINES StripeLines = AzaliaDmaEngine::SL_ONE,
                 UINT32 CodingType = 0);

    RC ConfigureWidgets();

    bool IsSupported(AzaliaFormat* pFormat);
    RC VerifyConfiguration(AzaliaDmaEngine::DIR Dir, UINT32 StreamIndex,
                           UINT32 ChannelIndex, AzaliaFormat* pFormat);

    // Unsolicited Response Functions
    RC EnableJackSenseUnsolResp(bool IsEnable);
    RC SetUnsolRespTag(UINT32 RespTag);
    UINT32 GetUnsolRespTag() const;
    bool CouldBeUnsolRespSoure(UINT32 RespTag) const;

private:

    // A route is defined as a series of connected nodes on
    // a single codec. The route must start with a function
    // group node followed by a colwerter node, and must end
    // with a pin node.
    AzaliaCodec* m_pCodec;
    vector<UINT16> m_NodeIDs;
    bool m_IsReserved;
    UINT32 m_StreamIndex;
    UINT32 m_ChannelIndex;
    AzaliaFormat* m_pFormat;

    UINT32 m_UnsolTag;
    bool m_UnsolEnable;
};

#endif // INCLUDED_AZART_H

