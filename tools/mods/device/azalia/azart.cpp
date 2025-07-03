/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2012,2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "azart.h"
#include "azacdc.h"
#include "azafmt.h"
#include "cheetah/include/tegradrf.h"
#include "core/include/massert.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "AzaRoute"

#define FGROUP_POS 0
#define COLW_POS 1
#define PIN_POS (m_NodeIDs.size() - 1)

RC AzaliaRoute::AddNode(UINT16 NodeID)
{
    m_NodeIDs.push_back(NodeID);
    return OK;
}

UINT16 AzaliaRoute::LastNode()
{
    if (m_NodeIDs.size())
    {
        return m_NodeIDs.back();
    }
    return 0;
}

bool AzaliaRoute::IsValidRoute()
{
    const AzaliaCodec::Node* nodePtr;
    if (m_pCodec == NULL) return false;
    if (m_NodeIDs.size() < 3) return false;
    // First node must be audio function group
    nodePtr = m_pCodec->GetNode(m_NodeIDs[FGROUP_POS]);
    MASSERT(nodePtr);
    if (nodePtr->type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDFGRP) return false;
    // Second node must be colwerter
    nodePtr = m_pCodec->GetNode(m_NodeIDs[COLW_POS]);
    MASSERT(nodePtr);
    if ((nodePtr->type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT)
        && (nodePtr->type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN)) return false;
    // Final node must be pin
    nodePtr = m_pCodec->GetNode(m_NodeIDs[PIN_POS]);
    MASSERT(nodePtr);
    if (nodePtr->type != AZA_CDCP_AUDIOW_CAP_TYPE_PIN) return false;

    return true;
}

bool AzaliaRoute::IsInput() const
{
    const AzaliaCodec::Node* colwNode = m_pCodec->GetNode(m_NodeIDs[COLW_POS]);

    if (colwNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT)
        return false;
    else if (colwNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN)
        return true;
    else
    {
        Printf(Tee::PriHigh, "Route's 2nd element is not an audio i/o colwerter!\n");
        return false;
    }
}

bool AzaliaRoute::operator<(const AzaliaRoute& Other) const
{
    return (this->m_NodeIDs.size() < Other.m_NodeIDs.size());
}

RC AzaliaRoute::PrintInfo(Tee::Priority Pri)
{
    RC rc;
    Printf(Pri, "Route using pin node 0x%x:\n",
           m_NodeIDs[PIN_POS]);
    vector<UINT16>::iterator iter = m_NodeIDs.begin();
    for ( ; iter != m_NodeIDs.end(); iter++)
    {
        CHECK_RC(m_pCodec->PrintInfoNode(Pri, *iter));
    }
    return OK;
}

RC AzaliaRoute::Reverse()
{
    INT32 first = 0;
    INT32 last = m_NodeIDs.size()-1;
    UINT16 tempID;
    while (first < last)
    {
        tempID = m_NodeIDs[last];
        m_NodeIDs[last] = m_NodeIDs[first];
        m_NodeIDs[first] = tempID;
        first++;
        last--;
    }
    return OK;
}

AzaliaRoute::AzaliaRoute(AzaliaCodec* pCodec)
{
    MASSERT(pCodec);
    m_pCodec = pCodec;
    m_NodeIDs.clear();
    m_IsReserved = false;
    m_StreamIndex = 0;
    m_ChannelIndex = 0;
    m_pFormat = NULL;
    m_UnsolEnable = 0;
    m_UnsolTag = 0;
}

AzaliaRoute::~AzaliaRoute()
{
    m_NodeIDs.clear();
}

RC AzaliaRoute::WireRoute()
{
    RC rc;
    if (!IsValidRoute())
    {
        Printf(Tee::PriError, "WireRoute: Invalid route!\n");
        PrintInfo(Tee::PriHigh);
        return RC::SOFTWARE_ERROR;
    }
    const AzaliaCodec::Node* nodePtr;
    nodePtr = m_pCodec->GetNode(m_NodeIDs[COLW_POS]);
    MASSERT(nodePtr);
    if (nodePtr->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN)
    {
        // Input route. Start at the colwerter and move out to the pin.
        // We skip the function group and pin nodes, since they have
        // no input from other nodes.
        UINT32 i = 1;
        do
        {
            UINT32 ccnt = 0;
            for (ccnt=0; ccnt<nodePtr->clist.size(); ccnt++)
            {
                if (nodePtr->clist[ccnt] == m_NodeIDs[i+1]) break;
            }
            if (ccnt == nodePtr->clist.size())
            {
                Printf(Tee::PriError, "Node 0x%x not connected to node 0x%x!\n",
                       m_NodeIDs[i], m_NodeIDs[i+1]);

                return RC::SOFTWARE_ERROR;
            }
            Printf(Tee::PriDebug, "Connecting input path node 0x%x to node 0x%x\n",
                   m_NodeIDs[i], m_NodeIDs[i+1]);

            CHECK_RC(m_pCodec->SendCommand(m_NodeIDs[i],
                                           VERB_SET_CONNECTION_SELECT, ccnt));
            i++;
            nodePtr = m_pCodec->GetNode(m_NodeIDs[i]);
            MASSERT(nodePtr);
        }
        while (i < m_NodeIDs.size()-1);
    }
    else
    {
        // Output route. Start at the pin and move into the colwerter.
        // We skip the function group and colwerter nodes, since they have
        // no input from other nodes.
        for (UINT32 i=PIN_POS; i > 1; i--)
        {
            nodePtr = m_pCodec->GetNode(m_NodeIDs[i]);
            MASSERT(nodePtr);
            UINT32 ccnt = 0;
            for (ccnt=0; ccnt<nodePtr->clist.size(); ccnt++)
            {
                if (nodePtr->clist[ccnt] == m_NodeIDs[i-1]) break;
            }
            if (ccnt == nodePtr->clist.size())
            {
                Printf(Tee::PriError, "Node 0x%x not connected to node 0x%x!\n",
                       m_NodeIDs[i], m_NodeIDs[i-1]);

                return RC::SOFTWARE_ERROR;
            }
            Printf(Tee::PriDebug, "Connecting output path node 0x%x to node 0x%x\n",
                   m_NodeIDs[i], m_NodeIDs[i-1]);

            CHECK_RC(m_pCodec->SendCommand(m_NodeIDs[i],
                                           VERB_SET_CONNECTION_SELECT, ccnt));
        }
    }
    return OK;
}

RC AzaliaRoute::VerifyConfiguration(AzaliaDmaEngine::DIR Dir, UINT32 StreamIndex,
                                    UINT32 ChannelIndex, AzaliaFormat* pFormat)
{
    RC rc, myRC;
    UINT32 index;
    UINT32 response;
    UINT32 temp;

    if (!StreamIndex)
    {
        Printf(Tee::PriError, "Stream index of 0 is not allowed by convention!\n");
        myRC = RC::SOFTWARE_ERROR;
    }

    UINT16 vid, did;
    CHECK_RC(m_pCodec->GetVendorDeviceID(&vid, &did));

    for (UINT32 i=0; i<m_NodeIDs.size(); i++)
    {
        const AzaliaCodec::Node* pNode = m_pCodec->GetNode(m_NodeIDs[i]);
        if (!pNode)
        {
            Printf(Tee::PriError, "VerifyRoute: Node %d not found!\n", m_NodeIDs[i]);
            return RC::SOFTWARE_ERROR;
        }
        Printf(Tee::PriLow, "VerifyRoute: checking node %d\n", pNode->id);
        index = 0;

        // Check power status
        if (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDFGRP ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_POWER ||
            RF_VAL(AZA_CDCP_AUDIOW_CAP_POWER, pNode->caps))
        {
            CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_POWER_STATE, 0, &response));
            if (RF_VAL(7:4, response))
            {
                Printf(Tee::PriError, "Node is not in D0\n");
                myRC = RC::HW_STATUS_ERROR;
            }
        }

        // Check connection to next widget
        if (pNode->type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT &&
            pNode->type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDFGRP &&
            pNode->type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOMIX &&
            !(pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_PIN &&
              Dir == AzaliaDmaEngine::DIR_INPUT))
        {
            index = 0;
            if (RF_VAL(AZA_CDCP_AUDIOW_CAP_CONLIST, pNode->caps) &&
                pNode->clist.size() > 1)
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_CONNECTION_SELECT,
                                               0, &index));
            }
            response = m_NodeIDs[i + ((Dir == AzaliaDmaEngine::DIR_INPUT)?1:-1)];
            if (pNode->clist[index] != response)
            {
                Printf(Tee::PriError, "Widget %d connected to %d, expected %d\n",
                       pNode->id, pNode->clist[index], response);

                Printf(Tee::PriHigh, "    List (index %d)\n", index);

                for (UINT32 cnt = 0; cnt < pNode->clist.size(); cnt++)
                {
                    Printf(Tee::PriHigh, " %d\n", pNode->clist[cnt]);
                }

                Printf(Tee::PriNormal, "\n");
                myRC = RC::SOFTWARE_ERROR;
            }
        }

        // Check output amp gain and mute setting
        if (RF_VAL(AZA_CDCP_AUDIOW_CAP_OUTAMP, pNode->caps))
        {
            UINT32 ampCaps;
            if (RF_VAL(AZA_CDCP_AUDIOW_CAP_AMPOVR, pNode->caps))
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_PARAMETER,
                                               AZA_CDCP_OUTAMP_CAP, &ampCaps));
            }
            else
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->fgroup_id, VERB_GET_PARAMETER,
                                               AZA_CDCP_OUTAMP_CAP, &ampCaps));
            }
            if ((vid == AZA_CDC_VENDOR_SIGMATEL) &&
                ((did == AZA_CDC_SIGMATEL_STAC9772 &&
                  pNode->id == 0xb) ||
                 (did == AZA_CDC_SIGMATEL_STAC9770 &&
                  (pNode->id == 0xb || pNode->id == 0x14))))
            {
                // TODO: why are we guessing here?
                ampCaps = 0x3f1f; // a guess to get things working
            }
            UINT32 offset = RF_VAL(AZA_CDCP_AMP_CAP_OFFSET, ampCaps);
            if ((Dir != AzaliaDmaEngine::DIR_INPUT) &&
                pNode->id == 0xc &&
                vid == AZA_CDC_VENDOR_REALTEK &&
                (did == AZA_CDC_REALTEK_ALC880 ||
                 did == AZA_CDC_REALTEK_ALC882 ||
                 did == AZA_CDC_REALTEK_ALC883 ||
                 did == AZA_CDC_REALTEK_ALC885 ||
                 did == AZA_CDC_REALTEK_ALC888))
            {
                // Many realtek codecs (but not all) have different full-scale
                // voltages for input and output. Ramping down the output gain
                // on the mixer prevents the loopback from getting clipped
                Printf(Tee::PriHigh, "Decreasing gain on Realtek output amp\n");
                if (offset < 3)
                {
                    Printf(Tee::PriWarn, "Realtek codec with insufficient gain on output amp!\n");
                    offset = 0;
                } else {
                    offset -= 3;
                }
            }

            // Mute if assigned to stream 0 and mute capable
            temp = (StreamIndex ? 0 : RF_VAL(AZA_CDCP_AMP_CAP_MUTE, ampCaps)) << 7;
            temp |= offset;
            CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_AMPLIFIER_GAIN_MUTE,
                                           0xa000, &response));
            if (response != temp)
            {
                Printf(Tee::PriError, "L out gain/mute settings (0x%08x) not correct. "
                                      "Expected 0x%08x\n",
                       response, temp);

                myRC = RC::SOFTWARE_ERROR;
            }
            CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_AMPLIFIER_GAIN_MUTE,
                                           0x8000, &response));
            if (response != temp)
            {
                Printf(Tee::PriError, "R out gain/mute settings (0x%08x) not correct. "
                                      "Expected 0x%08x\n",
                       response, temp);

                myRC = RC::SOFTWARE_ERROR;
            }
        }

        // Check input amps for gain and mute
        if (RF_VAL(AZA_CDCP_AUDIOW_CAP_INAMP, pNode->caps))
        {
            UINT32 ampCaps;
            if (RF_VAL(AZA_CDCP_AUDIOW_CAP_AMPOVR, pNode->caps))
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_PARAMETER,
                                               AZA_CDCP_INAMP_CAP, &ampCaps));
            }
            else
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->fgroup_id, VERB_GET_PARAMETER,
                                               AZA_CDCP_INAMP_CAP, &ampCaps));
            }
            UINT32 offset = RF_VAL(AZA_CDCP_AMP_CAP_OFFSET, ampCaps);

            index = 0;
            if (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOSEL)
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->id,
                                               VERB_GET_CONNECTION_SELECT,
                                               0, &index));
                index &= 0xff;
            }
            else if (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOMIX)
            {
                // Connection select is not supported on mixer
                // widgets, so we find it manually
                UINT16 lookingFor = 0;
                if (Dir == AzaliaDmaEngine::DIR_INPUT)
                {
                    // For input routes, input to mixer is from pin side
                    lookingFor = m_NodeIDs[i+1];
                }
                else
                {
                    // For output routes, input to mixer is from colwerter side
                    lookingFor = m_NodeIDs[i-1];
                }
                for (index = 0; index < pNode->clist.size(); index++)
                {
                    if (pNode->clist[index] == lookingFor) break;
                }
                MASSERT(index < pNode->clist.size());
            }

            for (UINT32 cnt=0; cnt<pNode->clist.size(); cnt++)
            {
                // All mute-capable input amps other than the current
                // index should be muted. According to the spec, only mixer
                // and selector widgets should have more than one input amp.
                // I believe we read this wrong for the Gen1 code. I'm fixing
                // it here to only program a single input amp for non-mixer,
                // non-selector widgets, but we may have to revert to the old
                // way if we start seeing errors. (If we have to revert, we'll
                // have avoid setting index=0 above and stick in a special
                // case for the conexant codec, which follows the spec as I
                // read it.)
                temp = offset | ((cnt == index ?
                                  0 : RF_VAL(AZA_CDCP_AMP_CAP_MUTE, ampCaps))<<7);
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_AMPLIFIER_GAIN_MUTE,
                                               0x4000 | cnt, &response));
                if (response != temp)
                {
                    Printf(Tee::PriError, "L in gain/mute settings (0x%08x) on cindex %d "
                                          "different from expected (0x%08x)\n",
                           response, cnt, temp);

                    myRC = RC::SOFTWARE_ERROR;
                }
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_AMPLIFIER_GAIN_MUTE,
                                               0x0000 | cnt, &response));
                if (response != temp)
                {
                    Printf(Tee::PriError, "R in gain/mute settings (0x%08x) on cindex %d "
                                          "different from expected (0x%08x)\n",
                           response, cnt, temp);

                    myRC = RC::SOFTWARE_ERROR;
                }
                if (pNode->type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOMIX &&
                    pNode->type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOSEL)
                {
                    break;
                }
            }
        }

        if (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT)
        {
            // Check direction
            if ((pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN &&
                 Dir != AzaliaDmaEngine::DIR_INPUT) ||
                (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT &&
                 Dir != AzaliaDmaEngine::DIR_OUTPUT))
            {
                Printf(Tee::PriError, "Stream and colwerter direction do not match\n");
                myRC = RC::SOFTWARE_ERROR;
            }

            // Check digital status
            if (RF_VAL(AZA_CDCP_AUDIOW_CAP_DIGITAL, pNode->caps))
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_DIGITAL_COLW,
                                               0, &response));
                if ((response & 0x0001) == 0)
                {
                    Printf(Tee::PriError, "Digital colwerter not enabled\n");
                    myRC = RC::SOFTWARE_ERROR;
                }
            }

            // Check stream and channel numbers
            CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_STREAM_CHANNEL_ID,
                                           0, &response));
            temp = ((StreamIndex << 4) | ChannelIndex);
            if (response != temp)
            {
                Printf(Tee::PriError, "Stream/Channel (0x%08x) does not match, "
                                      "expected 0x%08x\n",
                       response, temp);

                myRC = RC::SOFTWARE_ERROR;
            }

            // Check format
            CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_STREAM_FORMAT,
                                           0, &response));
            temp =
                ((pFormat->GetType()     & 0x0001) << 15) |
                ((pFormat->GetRateBase() & 0x0001) << 14) |
                ((pFormat->GetRateMult() & 0x0007) << 11) |
                ((pFormat->GetRateDiv()  & 0x0007) <<  8) |
                ((pFormat->GetSize()     & 0x0007) <<  4) |
                ((pFormat->GetChannels() & 0x000f) <<  0);
            if (response != temp)
            {
                Printf(Tee::PriError, "Format (0x%08x) different from expected (0x%08x)\n",
                       response, temp);

                myRC = RC::SOFTWARE_ERROR;
            }
        }

        if (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_PIN)
        {
            // Check direction
            if (!RF_VAL(AZA_CDCP_AUDIOW_CAP_DIGITAL, pNode->caps))
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_PIN_WIDGET_CONTROL,
                                               0, &response));
                if (Dir == AzaliaDmaEngine::DIR_INPUT)
                    temp = 0x00000020;
                else
                    temp = 0x00000040;
                if ((response & temp) != temp)
                {
                    Printf(Tee::PriError, "Pin directionality (0x%08x) does not match\n", response);
                    myRC = RC::SOFTWARE_ERROR;
                }
            }
            else
            {
                // At least on the ALC883, pin directionality appears to be
                // broken for digital pins. The codec's spec does not mention
                // that directionality is supported for digital pins, so I'm
                // just going to skip the check and print out a warning message.
                Printf(Tee::PriDebug, "Skipping pin direction check for digital stream.\n");
            }

            // Check unsolicited response disable
            if (pNode->caps & 0x00000080)
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->id,
                                               VERB_GET_UNSOLICITED_ENABLE,
                                               0, &response));
                if (response & 0x7f)
                {
                    Printf(Tee::PriError, "Unsolicted responses not disabled on pin\n");
                    myRC = RC::SOFTWARE_ERROR;
                }
            }
        }

        // Check stereo/mono
        if (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOMIX ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOSEL ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_PIN)
        {
            // I believe all digital widgets are mono, but they can
            // of course carry a stereo stream in their one channel of
            // data. Ignore a widget if it's digital, otherwise check that
            // single-channel streams go to mono widgets and that multi-channel
            // streams go to stereo widgets.
            if (!RF_VAL(AZA_CDCP_AUDIOW_CAP_DIGITAL, pNode->caps) &&
                ((UINT32) (pFormat->GetChannelsAsInt() == 1 ? 0 : 1) !=
                 RF_VAL(AZA_CDCP_AUDIOW_CAP_STEREO, pNode->caps)))
            {
                Printf(Tee::PriError, "Stream channels (%d) do not match stereo/mono caps\n",
                       pFormat->GetChannelsAsInt());

                myRC = RC::SOFTWARE_ERROR;
            }
        }
    }

    return myRC;
}

RC AzaliaRoute::Reserve()
{
    if (m_IsReserved)
    {
        Printf(Tee::PriError, "Route already reserved!\n");
        return RC::SOFTWARE_ERROR;
    }
    m_IsReserved = true;
    return OK;
}

void AzaliaRoute::Release()
{
    m_IsReserved = false;
}

bool AzaliaRoute::IsReserved()
{
    return m_IsReserved;
}

bool AzaliaRoute::Overlaps(AzaliaRoute* pOther)
{
    MASSERT(pOther);
    // Don't check function group node
    UINT32 i = PIN_POS;
    while (i)
    {
        if (pOther->Contains(m_NodeIDs[i]))
        {
            return true;
        }
        i--;
    }
    return false;
}

bool AzaliaRoute::Contains(UINT16 NodeId)
{
    UINT32 i = m_NodeIDs.size();
    while (i)
    {
        if (NodeId == m_NodeIDs[i-1])
        {
            return true;
        }
        i--;
    }
    return false;
}

bool AzaliaRoute::IsSupported(AzaliaFormat* pFormat)
{
    RC rc;
    UINT32 pcmCaps, fmtCaps;
    const AzaliaCodec::Node* pColwNode = m_pCodec->GetNode(m_NodeIDs[COLW_POS]);

    // Determine whether the colwerter or the function group
    // node controls the format, and get capabilities from that node.
    if (m_NodeIDs.size() < 3)
    {
        Printf(Tee::PriError, "Route too short--must have fgroup, colw, and pin!\n");
        return false;
    }
    CHECK_RC(m_pCodec->SendCommand(m_NodeIDs[COLW_POS], VERB_GET_PARAMETER,
                                   AZA_CDCP_AUDIOW_CAP, &pcmCaps));
    if (RF_VAL(AZA_CDCP_AUDIOW_CAP_FMTOVR, pcmCaps) == 0x1)
    {
        // Colwerter node controls format
        CHECK_RC(m_pCodec->SendCommand(m_NodeIDs[COLW_POS], VERB_GET_PARAMETER,
                                       AZA_CDCP_PCM_SUP, &pcmCaps));
        CHECK_RC(m_pCodec->SendCommand(m_NodeIDs[COLW_POS], VERB_GET_PARAMETER,
                                       AZA_CDCP_STREAM_FMT_SUP, &fmtCaps));
    }
    else
    {
        // Function group node controls format
        CHECK_RC(m_pCodec->SendCommand(m_NodeIDs[FGROUP_POS], VERB_GET_PARAMETER,
                                       AZA_CDCP_PCM_SUP, &pcmCaps));
        CHECK_RC(m_pCodec->SendCommand(m_NodeIDs[FGROUP_POS], VERB_GET_PARAMETER,
                                       AZA_CDCP_STREAM_FMT_SUP, &fmtCaps));
    }

    // Special cases for some codecs. Boo for non-conforming codecs!
    UINT16 vid, did;
    CHECK_RC(m_pCodec->GetVendorDeviceID(&vid, &did));
    if (vid == AZA_CDC_VENDOR_CMEDIA
        && did == AZA_CDC_CMEDIA_CMI9880)
    {
        // TODO: Gen1 code had a condition here for (ColwID > 2 || ColwID < 11).
        // Since this obviously is always true, I just removed it. Need to
        // find a CMEDIA codec to determine what this condition was *actually*
        // supposed to guard against.
        pcmCaps |=
            RF_SHIFTMASK(AZA_CDCP_PCM_SUP_R6) |
            RF_SHIFTMASK(AZA_CDCP_PCM_SUP_R7) |
            RF_SHIFTMASK(AZA_CDCP_PCM_SUP_R9);

        CHECK_RC(m_pCodec->SendCommand(m_NodeIDs[FGROUP_POS], VERB_GET_PARAMETER,
                                       AZA_CDCP_STREAM_FMT_SUP, &fmtCaps));

    }

    // Check for supported rates
    bool rSupport = false;
    switch (pFormat->GetRate())
    {
        case AzaliaFormat::RATE_8000:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R1, pcmCaps);
            break;
        case AzaliaFormat::RATE_11025:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R2, pcmCaps);
            break;
        case AzaliaFormat::RATE_16000:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R3, pcmCaps);
            break;
        case AzaliaFormat::RATE_22050:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R4, pcmCaps);
            break;
        case AzaliaFormat::RATE_32000:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R5, pcmCaps);
            break;
        case AzaliaFormat::RATE_44100:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R6, pcmCaps);
            break;
        case AzaliaFormat::RATE_48000:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R7, pcmCaps);
            break;
        case AzaliaFormat::RATE_88200:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R8, pcmCaps);
            break;
        case AzaliaFormat::RATE_96000:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R9, pcmCaps);
            break;
        case AzaliaFormat::RATE_176400:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R10, pcmCaps);
            break;
        case AzaliaFormat::RATE_192000:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R11, pcmCaps);
            break;
        case AzaliaFormat::RATE_384000:
            rSupport = RF_VAL(AZA_CDCP_PCM_SUP_R12, pcmCaps);
            break;
        default:
            Printf(Tee::PriError, "Unknown rate!\n");
            rSupport = false;
    }
    if (!rSupport)
    {
        Printf(Tee::PriDebug, "Rate %d not supported on colwerter 0x%x\n",
               pFormat->GetRate(), m_NodeIDs[COLW_POS]);
    }

    // Check for supported sizes
    bool sSupport = false;
    switch (pFormat->GetSize())
    {
        case AzaliaFormat::SIZE_8:
            sSupport = RF_VAL(AZA_CDCP_PCM_SUP_B8, pcmCaps);
            break;
        case AzaliaFormat::SIZE_16:
            sSupport = RF_VAL(AZA_CDCP_PCM_SUP_B16, pcmCaps);
            break;
        case AzaliaFormat::SIZE_20:
            sSupport = RF_VAL(AZA_CDCP_PCM_SUP_B20, pcmCaps);
            break;
        case AzaliaFormat::SIZE_24:
            sSupport = RF_VAL(AZA_CDCP_PCM_SUP_B24, pcmCaps);
            break;
        case AzaliaFormat::SIZE_32:
            sSupport = RF_VAL(AZA_CDCP_PCM_SUP_B32, pcmCaps);
            break;
        default:
            Printf(Tee::PriError, "Unknown size!\n");
            sSupport = false;
    }
    if (!sSupport)
    {
        Printf(Tee::PriDebug, "Size %d not supported on colwerter 0x%x\n",
               pFormat->GetSize(), m_NodeIDs[COLW_POS]);
    }

    // Check for supported types
    bool tSupport = false;
    switch (pFormat->GetType())
    {
        case AzaliaFormat::TYPE_PCM:
            tSupport = RF_VAL(AZA_CDCP_STREAM_FMT_SUP_PCM, fmtCaps);
            break;
        case AzaliaFormat::TYPE_NONPCM:
            tSupport = RF_VAL(AZA_CDCP_STREAM_FMT_SUP_AC3, fmtCaps) ||
                       RF_VAL(AZA_CDCP_STREAM_FMT_SUP_F32, fmtCaps);
            break;
        default:
            Printf(Tee::PriError, "Unknown type!\n");
            tSupport = false;
    }
    if (!tSupport)
    {
        Printf(Tee::PriDebug, "Type %d not supported on colwerter 0x%x\n",
               pFormat->GetType(), m_NodeIDs[COLW_POS]);
    }

    // Check for supported channels
    bool cSupport = false;

    // If the pin supports HDMI then it supports upto 8 channels
    if (IsHdmiCapable() && pFormat->GetChannelsAsInt() < 9)
    {
        Printf(Tee::PriDebug, "HDMI Route Detected!!\n");
        // We also need to check that the AOCW supports the same number of channels
        UINT32 chanCnt = (RF_VAL(AZA_CDCP_AUDIOW_CAP_CHAN_CNT, pColwNode->caps) << 1) |
                         (RF_VAL(AZA_CDCP_AUDIOW_CAP_STEREO, pColwNode->caps));

        if (pFormat->GetChannelsAsInt() <= chanCnt + 1)
            cSupport = true;
    }
    else if ((pFormat->GetChannelsAsInt() < 3)
        || !(vid == AZA_CDC_VENDOR_CONEXANT && did == AZA_CDC_CONEXANT_2054912Z))
    {
        cSupport = true;
    }
    if (!cSupport)
    {
        Printf(Tee::PriDebug, "Channels %d not supported on this codec\n",
               pFormat->GetChannelsAsInt());
    }

    // TODO: WARNING!!!!!!
    // This is a dangerous hack for siumulation that is pureley on a temporary basis
    // This ABSOLUTELY HAS to be removed later on
#ifndef SIM_BUILD
    return (rSupport && sSupport && tSupport && cSupport);
#else
    return true;
#endif
}

RC AzaliaRoute::Configure(UINT32 StreamIndex, UINT32 ChannelIndex,
                          AzaliaFormat* pFormat, AzaliaDmaEngine::STRIPE_LINES StripeLines,
                          UINT32 CodingType)
{
    RC rc;
    UINT32 response;
    MASSERT(pFormat);
    if (!StreamIndex)
    {
        Printf(Tee::PriError, "Stream index of 0 is not allowed by convention!\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(WireRoute());

    // Get codec info for special cases
    UINT16 vid, did;
    CHECK_RC(m_pCodec->GetVendorDeviceID(&vid, &did));

    // Get information on direction and digital support
    const AzaliaCodec::Node* pPinNode = m_pCodec->GetNode(m_NodeIDs[PIN_POS]);
    if (!pPinNode)
    {
        Printf(Tee::PriError, "Node %d not found!\n", m_NodeIDs[PIN_POS]);
        return RC::SOFTWARE_ERROR;
    }
    const AzaliaCodec::Node* pColwNode = m_pCodec->GetNode(m_NodeIDs[COLW_POS]);
    if (!pColwNode)
    {
        Printf(Tee::PriError, "Node %d not found!\n", m_NodeIDs[COLW_POS]);
        return RC::SOFTWARE_ERROR;
    }
    bool pinIsDigital = RF_VAL(AZA_CDCP_AUDIOW_CAP_DIGITAL, pPinNode->caps);
    bool pinIsOutput = RF_VAL(AZA_CDCP_PIN_CAP_OUTPUT, pPinNode->specialCaps);
    bool pinIsInput = RF_VAL(AZA_CDCP_PIN_CAP_INPUT, pPinNode->specialCaps);
    bool colwIsDigital = RF_VAL(AZA_CDCP_AUDIOW_CAP_DIGITAL, pColwNode->caps);
    bool colwIsInput = (pColwNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN);

    if (pinIsDigital != colwIsDigital)
    {
        if (pinIsDigital)
        {
            Printf(Tee::PriError, "Digital pin connected to analog colwerter!\n");
        } else {
            Printf(Tee::PriError, "Analog pin connected to digital colwerter!\n");
        }
        return RC::SOFTWARE_ERROR;
    }
    if ((colwIsInput && !pinIsInput) || (!colwIsInput && !pinIsOutput))
    {
        Printf(Tee::PriError, "Pin and colwerter directionality do not match!\n");
        return RC::SOFTWARE_ERROR;
    }

    // Set pin's direction
    CHECK_RC(m_pCodec->SendCommand(pPinNode->id, VERB_GET_PIN_WIDGET_CONTROL,
                                   0, &response));
    if (colwIsInput)
    {
        response = FLD_SET_RF_NUM(AZA_CDC_PINW_CTRL_IN_ENABLE, 1, response);
        response = FLD_SET_RF_NUM(AZA_CDC_PINW_CTRL_OUT_ENABLE, 0, response);
    }
    else
    {
        response = FLD_SET_RF_NUM(AZA_CDC_PINW_CTRL_OUT_ENABLE, 1, response);
        response = FLD_SET_RF_NUM(AZA_CDC_PINW_CTRL_IN_ENABLE, 0, response);
    }

    CHECK_RC(m_pCodec->SendCommand(pPinNode->id, VERB_SET_PIN_WIDGET_CONTROL,
                                   response));

    // Disable unsolicited responses on pin
    if (FLD_TEST_RF_NUM(AZA_CDCP_FGROUP_TYPE_UNSOL, 1, pPinNode->caps))// & 0x00000080)
    {
        CHECK_RC(m_pCodec->SendCommand(pPinNode->id,
                                       VERB_SET_UNSOLICITED_ENABLE, 0x00|m_UnsolTag));
    }

    // On some codecs, the nodes don't come up in D0 by default.
    // Configuring the widgets to D0 results in the settings being lost.
    // I don't see this causing any problems.
    CHECK_RC(ConfigureWidgets());

    // Program stream and channel numbers and format
    MASSERT((StreamIndex <= 15) && (ChannelIndex <= 15));
    CHECK_RC(m_pCodec->SendCommand(m_NodeIDs[COLW_POS], VERB_SET_STREAM_CHANNEL_ID,
                                   (StreamIndex << 4) | ChannelIndex));
    response =
        ((pFormat->GetType()     & 0x0001) << 15) |
        ((pFormat->GetRateBase() & 0x0001) << 14) |
        ((pFormat->GetRateMult() & 0x0007) << 11) |
        ((pFormat->GetRateDiv()  & 0x0007) <<  8) |
        ((pFormat->GetSize()     & 0x0007) <<  4) |
        ((pFormat->GetChannels() & 0x000f) <<  0);
    CHECK_RC(m_pCodec->SendCommand(m_NodeIDs[COLW_POS],
                                   VERB_SET_STREAM_FORMAT, response));

    // Enable digital colwerters
    // For some reason, this has to happen after setting the format. I don't
    // see anything in the spec about it, but perhaps I'm just not looking
    // hard enough. All of the codecs seem to behave this way.
    if (colwIsDigital)
    {
        CHECK_RC(m_pCodec->SendCommand(pColwNode->id, VERB_GET_DIGITAL_COLW,
                                       0, &response));
        response = (response & 0x00ff) | 0x1;
        CHECK_RC(m_pCodec->SendCommand(pColwNode->id,
                                       VERB_SET_DIGITAL_COLW_BYTE1, response));
    }

    // Set the number of SDO Lines.
    // Lwrrently this is only being used for GT21x for dual SDO
    CHECK_RC(m_pCodec->SendCommand(pColwNode->id, VERB_SET_STRIPE_CONTROL,
                                   StripeLines, NULL));

    // This is lwrrently being used only for GT21x
    // The value defaults to 0, in which case no action is taken
    if (CodingType)
    {
        CHECK_RC(m_pCodec->SendCommand(pPinNode->id, VERB_GET_PIN_WIDGET_CONTROL,
                                       0, &response));
        response = FLD_SET_RF_NUM(AZA_CDC_PINW_CTRL_IN_HDMI_CODING_TYPE, CodingType, response);
        CHECK_RC(m_pCodec->SendCommand(pPinNode->id, VERB_SET_PIN_WIDGET_CONTROL,
                                       response));
    }

    // If this is a HDMI route we need to set the number of channels in the
    // AOCW (Lwrrently this is used only in GT21x)
    if (IsHdmiCapable())
    {
        CHECK_RC(m_pCodec->SendCommand(m_NodeIDs[COLW_POS], VERB_SET_COLW_CHAN_CNT,
                                       pFormat->GetChannelsAsInt() - 1));
    }

    return OK;
}

RC AzaliaRoute::ConfigureWidgets()
{
    RC rc = OK;

    // Get codec info for special cases
    UINT16 vid, did;
    CHECK_RC(m_pCodec->GetVendorDeviceID(&vid, &did));

    UINT32 response;
    const AzaliaCodec::Node* pColwNode = m_pCodec->GetNode(m_NodeIDs[COLW_POS]);
    bool colwIsInput = (pColwNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN);

    // Configure each widget
    for (UINT32 i=0; i<m_NodeIDs.size(); i++)
    {
        const AzaliaCodec::Node* pNode = m_pCodec->GetNode(m_NodeIDs[i]);
        if (!pNode)
        {
            Printf(Tee::PriError, "Cannot find node %d\n", m_NodeIDs[i]);
            return RC::SOFTWARE_ERROR;
        }

        // Make sure widget is in D0 state
        if (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDFGRP ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_POWER ||
            RF_VAL(AZA_CDCP_AUDIOW_CAP_POWER, pNode->caps))
        {
            CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_POWER_STATE,
                                           0, &response));

            if (RF_VAL(AZA_CDC_NODE_POWER_STATE_ACT, response))
            {
                Printf(Tee::PriDebug, "Node %d is not in D0. Changing to D0\n", pNode->id);
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_SET_POWER_STATE, 0));
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_POWER_STATE,
                                               0, &response));
                if (response)
                {
                    Printf(Tee::PriWarn, "Could not move node to D0!\n");
                }
            }
        }

        // If there's an output amp, set gain to 0db and don't mute
        if (RF_VAL(AZA_CDCP_AUDIOW_CAP_OUTAMP, pNode->caps))
        {
            UINT32 ampCaps;
            if (RF_VAL(AZA_CDCP_AUDIOW_CAP_AMPOVR, pNode->caps))
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_PARAMETER,
                                               AZA_CDCP_OUTAMP_CAP, &ampCaps));
            }
            else
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->fgroup_id, VERB_GET_PARAMETER,
                                               AZA_CDCP_OUTAMP_CAP, &ampCaps));
            }
            if ((vid == AZA_CDC_VENDOR_SIGMATEL) &&
                ((did == AZA_CDC_SIGMATEL_STAC9772 &&
                  pNode->id == 0xb) ||
                 (did == AZA_CDC_SIGMATEL_STAC9770 &&
                  (pNode->id == 0xb || pNode->id == 0x14))))
            {
                // TODO: why are we guessing here?
                ampCaps = 0x3f1f; // a guess to get things working
            }

            // For Realtek ALC269 we need to send an EAPD to enable the external
            // amp for the RCA speaker output.
            if (!colwIsInput &&
                vid == AZA_CDC_VENDOR_REALTEK &&
                did == AZA_CDC_REALTEK_ALC269 &&
                pNode->id == 0x14)
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_SET_EAPD_BTL_ENABLE, 0x2));
            }

            UINT32 offset = RF_VAL(AZA_CDCP_AMP_CAP_OFFSET, ampCaps);

            if (!colwIsInput &&
                pNode->id == 0xc &&
                vid == AZA_CDC_VENDOR_REALTEK &&
                (did == AZA_CDC_REALTEK_ALC880 ||
                 did == AZA_CDC_REALTEK_ALC882 ||
                 did == AZA_CDC_REALTEK_ALC883 ||
                 did == AZA_CDC_REALTEK_ALC885 ||
                 did == AZA_CDC_REALTEK_ALC888))
            {
                // Many realtek codecs (but not all) have different full-scale
                // voltages for input and output. Ramping down the output gain
                // on the mixer prevents the loopback from getting clipped
                Printf(Tee::PriHigh, "Decreasing gain on Realtek output amp\n");
                if (offset < 3)
                {
                    Printf(Tee::PriWarn, "Realtek codec with insufficient gain on output amp!\n");
                    offset = 0;
                } else {
                    offset -= 3;
                }
            }
            UINT32 mute = 0;
            if ((vid == AZA_CDC_VENDOR_SIGMATEL) &&
                ((did == AZA_CDC_SIGMATEL_STAC9772 && pNode->id == 0xa) ||
                 (did == AZA_CDC_SIGMATEL_STAC9770 && pNode->id == 0xa)))
            {
                CHECK_RC(m_pCodec->SendCommand(pColwNode->id,
                                               VERB_GET_STREAM_CHANNEL_ID,
                                               0, &response));
                // Sigmatel DAC cannot be taken out of mute if the stream
                // number is 0.
                // TODO: will this ever be called with stream number 0?
                if (((response >> 4) & 0xf) == 0)
                {
                    Printf(Tee::PriWarn, "Muting Sigmatel DAC with stream number 0\n");
                    mute = 1;
                }
            }

            UINT16 gainMute = RF_NUM(AZA_CDCV_SET_GAINMUTE_OUT_AMP,1) |
                              RF_NUM(AZA_CDCV_SET_GAINMUTE_LEFT_AMP,1) |
                              RF_NUM(AZA_CDCV_SET_GAINMUTE_RIGHT_AMP,1) |
                              offset |
                              RF_NUM(AZA_CDCV_SET_GAINMUTE_MUTE, mute);
            CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_SET_AMPLIFIER_GAIN_MUTE,
                     gainMute, &response));
        }

        // If there's an input amp, set gain to 0db and don't mute
        // If there's an input amp, set gain to 0db and don't mute
        if (RF_VAL(AZA_CDCP_AUDIOW_CAP_INAMP, pNode->caps))
        {
            UINT32 ampCaps;
            if (RF_VAL(AZA_CDCP_AUDIOW_CAP_AMPOVR, pNode->caps))
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->id, VERB_GET_PARAMETER,
                                               AZA_CDCP_INAMP_CAP, &ampCaps));
            }
            else
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->fgroup_id, VERB_GET_PARAMETER,
                                               AZA_CDCP_INAMP_CAP, &ampCaps));
            }
            UINT32 offset = RF_VAL(AZA_CDCP_AMP_CAP_OFFSET, ampCaps);

            UINT32 index;
            if (pNode->type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOMIX)
            {
                CHECK_RC(m_pCodec->SendCommand(pNode->id,
                                               VERB_GET_CONNECTION_SELECT,
                                               0, &index));
                index &= 0xff;
            }
            else
            {
                // Connection select is not supported on mixer
                // widgets, so we find it manually
                UINT16 lookingFor = 0;
                if (colwIsInput)
                {
                    // For input routes, input to mixer is from pin side
                    lookingFor = m_NodeIDs[i+1];
                }
                else
                {
                    // For output routes, input to mixer is from colwerter side
                    lookingFor = m_NodeIDs[i-1];
                }
                for (index = 0; index < pNode->clist.size(); index++)
                {
                    if (pNode->clist[index] == lookingFor) break;
                }
                MASSERT(index < pNode->clist.size());
            }
            if ((vid == AZA_CDC_VENDOR_CONEXANT) &&
                ((did == AZA_CDC_CONEXANT_2054912Z)))
            {
                // Conexant codec is out of spec here. For setting the gain
                // and mute options on the input amp, only 0 is valid.
                index = 0x0;
            }

            // If there is no connection list present then we need to unmute
            // the widget here. Else we can walk through the list and mute
            // everything except the current index
            // I have seen this to be the case when performing SPDIF loopback
            if (pNode->clist.size() == 0)
            {
                UINT16 gainMute = RF_NUM(AZA_CDCV_SET_GAINMUTE_IN_AMP,1) |
                                  RF_NUM(AZA_CDCV_SET_GAINMUTE_LEFT_AMP,1) |
                                  RF_NUM(AZA_CDCV_SET_GAINMUTE_RIGHT_AMP,1) |
                                  RF_NUM(AZA_CDCV_SET_GAINMUTE_INDEX, index) |
                                  offset |
                                  RF_NUM(AZA_CDCV_SET_GAINMUTE_MUTE, 0);

                CHECK_RC(m_pCodec->SendCommand(pNode->id,
                                               VERB_SET_AMPLIFIER_GAIN_MUTE,
                                               gainMute, &response));
            }
            else
            {
                for (UINT32 cnt=0; cnt < pNode->clist.size(); cnt++)
                {
                    // All mute-capable input amps other than the current
                    // index should be muted. According to the spec, only mixer
                    // and selector widgets should have more than one input amp.
                    // I believe we read this wrong for the Gen1 code. I'm fixing
                    // it here to only program a single input amp for non-mixer,
                    // non-selector widgets, but we may have to revert to the old
                    // way if we start seeing errors. (If we have to revert, we'll
                    // have avoid setting index=0 above and stick in a special
                    // case for the conexant codec, which follows the spec as I
                    // read it.)
                    UINT16 gainMute = RF_NUM(AZA_CDCV_SET_GAINMUTE_IN_AMP,1) |
                                      RF_NUM(AZA_CDCV_SET_GAINMUTE_LEFT_AMP,1) |
                                      RF_NUM(AZA_CDCV_SET_GAINMUTE_RIGHT_AMP,1) |
                                      RF_NUM(AZA_CDCV_SET_GAINMUTE_INDEX, cnt) |
                                      offset;
                    if (cnt != index)
                    {
                        UINT16 muteCap = RF_VAL(AZA_CDCP_AMP_CAP_MUTE, ampCaps);
                        gainMute |= RF_NUM(AZA_CDCV_SET_GAINMUTE_MUTE, muteCap);
                    }

                    CHECK_RC(m_pCodec->SendCommand(pNode->id,
                                                   VERB_SET_AMPLIFIER_GAIN_MUTE, gainMute));
                    if (pNode->type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOMIX &&
                        pNode->type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOSEL)
                    {
                        break;
                    }
                }
            }
        }
    }

    UINT16 gainMute = RF_NUM(AZA_CDCV_SET_GAINMUTE_IN_AMP,1) |
                      RF_NUM(AZA_CDCV_SET_GAINMUTE_LEFT_AMP,1) |
                      RF_NUM(AZA_CDCV_SET_GAINMUTE_RIGHT_AMP,1) |
                      RF_NUM(AZA_CDCV_SET_GAINMUTE_INDEX, 1) |
                      0 |
                      RF_NUM(AZA_CDCV_SET_GAINMUTE_MUTE, 1);;
    CHECK_RC(m_pCodec->SendCommand(0xc, VERB_SET_AMPLIFIER_GAIN_MUTE, gainMute));

    return rc;
}

RC AzaliaRoute::SetUnsolRespTag(UINT32 Tag)
{
    if (Tag&0xFFFFFFC0)
    {
        Printf(Tee::PriError, "Tag value out of range, 0x%x\n", Tag);
        return RC::SOFTWARE_ERROR;
    }
    m_UnsolTag = Tag&0x3F;
    return OK;
}

UINT32 AzaliaRoute::GetUnsolRespTag() const
{
    return m_UnsolTag;
}

bool AzaliaRoute::CouldBeUnsolRespSoure(UINT32 RespTag) const
{
    return ((RespTag == m_UnsolTag) && m_UnsolEnable);
}

RC AzaliaRoute::EnableJackSenseUnsolResp(bool IsEnable)
{
    RC rc;
    m_UnsolEnable = IsEnable;

    CHECK_RC( m_pCodec->SendCommand(LastNode(), VERB_SET_UNSOLICITED_ENABLE,
                                    ((IsEnable?0x80:0x00)|m_UnsolTag)) );

    return OK;
}

bool AzaliaRoute::IsJackSenseCapable() const
{
    const AzaliaCodec::Node* pinNode = m_pCodec->GetNode(m_NodeIDs[PIN_POS]);

    return (((pinNode->specialCaps>>2)&0x1) != 0);
}

bool AzaliaRoute::IsHdmiCapable()
{
    const AzaliaCodec::Node* pPinNode = m_pCodec->GetNode(m_NodeIDs[PIN_POS]);

    return (RF_VAL(AZA_CDCP_PIN_CAP_HDMI, pPinNode->specialCaps));
}

bool AzaliaRoute::IsDigitalPin()
{
    const AzaliaCodec::Node* pPinNode = m_pCodec->GetNode(m_NodeIDs[PIN_POS]);

    return (RF_VAL(AZA_CDCP_AUDIOW_CAP_DIGITAL, pPinNode->caps));
}
