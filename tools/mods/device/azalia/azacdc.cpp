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

#include "azacdc.h"
#include "azactrl.h"
#include "azafmt.h"
#include "azautil.h"
#include "cheetah/include/tegradrf.h"
#include "cheetah/include/logmacros.h"
#include "core/include/tasker.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "AzaCodec"

namespace
{
    typedef struct
    {
        AzaliaController* pAzalia;
        UINT32* verbs;
        UINT16 lwerbs;
    } AzaCorbPushPollArgs;

    bool AzaCorbPushWaitPoll(void* pArgs)
    {
        MASSERT(pArgs);
        RC rc;
        UINT16 nPushed = 0;
        AzaCorbPushPollArgs* pPollArgs = (AzaCorbPushPollArgs*) pArgs;
        MASSERT(pPollArgs->pAzalia);
        CHECK_RC(pPollArgs->pAzalia->CorbPush(pPollArgs->verbs,
                                              pPollArgs->lwerbs,
                                              &nPushed));

        return (nPushed == pPollArgs->lwerbs);
    }
};

struct StringMapping
{
    UINT08 type;
    const char* str;
};

const StringMapping FGroupStrings[] =
{
    {AZA_CDCP_FGROUP_TYPE_TYPE_AUDIO, "AUDIO"},
    {AZA_CDCP_FGROUP_TYPE_TYPE_MODEM, "MODEM"},
    {AZA_CDCP_FGROUP_TYPE_TYPE_VEND,  "OTHER"},
    {AZA_CDCP_FGROUP_TYPE_TYPE_OTHER, "OTHER"},
};

const StringMapping NodeStrings[] =
{
    {AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT,   "AUDIO_OUTPUT"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN,    "AUDIO_INPUT"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOMIX,   "AUDIO_MIXER"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOSEL,   "AUDIO_SELECTOR"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_PIN,        "AUDIO_PIN_COMPLEX"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_POWER,      "AUDIO_POWER"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_VOLUME,     "AUDIO_VOLUME_KNOB"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_BEEP,       "AUDIO_BEEP_GENERATOR"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_VENDORDEF,  "AUDIO_VENDOR_DEFINED"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_ROOT,       "NODE_ROOT"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_AUDFGRP,    "NODE_AUDIO_FGROUP"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_MODFGRP,    "NODE_MODEM_FGROUP"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_OTHFGRP,    "NODE_OTHER_FGROUP"},
    {AZA_CDCP_AUDIOW_CAP_TYPE_ILWALID,    "NODE_ILWALID"},
};

AzaliaCodec::AzaliaCodec(AzaliaController* pAzalia, UINT08 Addr)
{
    MASSERT(pAzalia);
    m_pAza = pAzalia;
    m_Addr = Addr;
    m_Vid = 0;
    m_Did = 0;
    m_DefaultInfoIndex = 0xffff;

    m_Nodes.clear();
    m_AudioGroupBaseNodes.clear();
    m_ModemGroupBaseNodes.clear();
    m_InputRoutes.clear();
    m_OutputRoutes.clear();
}

AzaliaCodec::~AzaliaCodec()
{
    // Do nothing
}

RC AzaliaCodec::ConstructVerb(UINT32* pCommand, UINT08 CodecAddr, UINT32 NodeID,
                              UINT32 VerbID, UINT32 Payload)
{
    // Make sure that verb and payload don't overlap (i.e., that
    // we don't have a long verb with a long payload)
    MASSERT(!(RF_NUM(AZA_CDC_CMD_VERB_LONG, VerbID) &Payload));

    // We define our short verbs to be shifted 8 bits (i.e., 0xb00 instead
    // of just 0xb), so we use VERB_LONG with VERB_SHORT_PAYLOAD.
    *pCommand = RF_NUM(AZA_CDC_CMD_CAD, CodecAddr) |
        RF_NUM(AZA_CDC_CMD_NID, NodeID) |
        RF_NUM(AZA_CDC_CMD_VERB_LONG, VerbID) |
        RF_NUM(AZA_CDC_CMD_VERB_SHORT_PAYLOAD, Payload);

    return OK;
}

RC AzaliaCodec::EnumerateFunctionGroups()
{
    RC rc;
    m_AudioGroupBaseNodes.clear();
    m_ModemGroupBaseNodes.clear();
    UINT32 value = 0;
    CHECK_RC(SendCommand(0, VERB_GET_PARAMETER, AZA_CDCP_SUB_NODE_CNT, &value));
    UINT32 startNode = RF_VAL(AZA_CDCP_SUB_NODE_CNT_START, value);
    UINT32 nGroups = RF_VAL(AZA_CDCP_SUB_NODE_CNT_TOTAL, value);
    Printf(Tee::PriLow, "The codec contains %d function group(s).\n", nGroups);

    for (UINT32 node = startNode; node < startNode+nGroups; node++)
    {
        CHECK_RC(SendCommand(node, VERB_GET_PARAMETER,
                             AZA_CDCP_FGROUP_TYPE, &value));
        UINT08 type = RF_VAL(AZA_CDCP_FGROUP_TYPE_TYPE, value);
        switch (type)
        {
            case AZA_CDCP_FGROUP_TYPE_TYPE_AUDIO:
                m_AudioGroupBaseNodes.push_back(node);
                break;
            case AZA_CDCP_FGROUP_TYPE_TYPE_MODEM:
                m_ModemGroupBaseNodes.push_back(node);
                break;
            case AZA_CDCP_FGROUP_TYPE_TYPE_OTHER:
                Printf(Tee::PriWarn, "\"Other\" function group type at node 0x%x\n", node);
                break;
            default:
                Printf(Tee::PriError, "Unknown function group type (0x%x) at node 0x%x\n",
                       RF_VAL(AZA_CDCP_FGROUP_TYPE_TYPE, value), node);
                return RC::SOFTWARE_ERROR;
        }
        Printf(Tee::PriDebug, "    Function group of type \"%s\" at node 0x%x\n",
               FunctionGroupTypeToString(type), node);

        if (m_Vid == AZA_CDC_VENDOR_ADI
            && m_Did == AZA_CDC_ADI_1986)
        {
            // ADI1986 does not send a response to a function reset verb.
            // It still needs two frames of delay, though.
            CHECK_RC(SubmitVerb(node, VERB_RESET, 0));
            AzaliaUtilities::Pause(1);
        }
        else
        {
            CHECK_RC(SendCommand(node, VERB_RESET, 0));
        }
        // Without
        //   --CMI9880 needs about 10ms delay here or else it will send some
        //     unexpected unsolicited responses
        //   --ADI1986 needs the 80ms delay here or else some node queries will
        //     show non D0 power states.
        //   --ADI1986A needs the 200ms delay here or else some node queries will
        //     show non D0 power states.
        if (m_Vid == AZA_CDC_VENDOR_ADI || m_Vid == AZA_CDC_VENDOR_CMEDIA)
        {
            Tasker::Sleep(200);
        }

        // Disable Unsolicited Responses
        CHECK_RC(SendCommand(node, VERB_SET_UNSOLICITED_ENABLE, node));
    }
    return OK;
}

RC AzaliaCodec::EnumerateNodes()
{
    RC rc;
    UINT32 value;
    Node node;
    m_Nodes.clear();

    // Start at the root node
    node.id = 0;
    node.fgroup_id = 0; // dummy value
    node.type = AZA_CDCP_AUDIOW_CAP_TYPE_ROOT;
    node.caps = 0;
    node.specialCaps = 0;
    node.clist.clear();
    m_Nodes.push_back(node);

    vector<UINT16>::iterator groupIterator = m_AudioGroupBaseNodes.begin();

    for ( ; groupIterator != m_AudioGroupBaseNodes.end(); groupIterator++)
    {
        // Process the function group base node
        UINT16 baseNode = *groupIterator;
        CHECK_RC(SendCommand(baseNode, VERB_GET_PARAMETER,
                             AZA_CDCP_SUB_NODE_CNT, &value));
        UINT32 nNodes = RF_VAL(AZA_CDCP_SUB_NODE_CNT_TOTAL, value);
        UINT32 startNode = RF_VAL(AZA_CDCP_SUB_NODE_CNT_START, value);
        node.id = baseNode;
        node.fgroup_id = baseNode;
        node.type = AZA_CDCP_AUDIOW_CAP_TYPE_AUDFGRP;
        node.caps = 0;
        node.specialCaps = 0;
        node.clist.clear();
        m_Nodes.push_back(node);

        // Create a temporary node list for every function group node
        vector<Node> tempNodeList;
        tempNodeList.clear();

        // We can use these as static arrays because the maximum number
        // of commands we send is equal to the number of nodes
        // We can avoid having to create a vector and colwert it to an array
        vector<UINT32> verbList(nNodes);
        vector<UINT64> responses(nNodes);
        UINT32 verb = 0;
        UINT32 lwerbs = 0;

        // Get all the node caps first
        // Create the verbs for all the nodes and send them out
        for (UINT32 i = startNode; i < startNode + nNodes; i++)
        {
            node.id = i;
            CHECK_RC(ConstructVerb(&verb, m_Addr, node.id, VERB_GET_PARAMETER,
                                   AZA_CDCP_AUDIOW_CAP));
            verbList[lwerbs] = verb;
            tempNodeList.push_back(node);
            lwerbs++;
        }
        CHECK_RC(SendCommands(&verbList[0], lwerbs, &responses[0]));

        // Store all the node types from the responses received and create
        // the commands to determin pin caps
        lwerbs = 0;
        for (UINT32 i = startNode, j = 0; i < startNode + nNodes; i++, j++)
        {
            // Store the caps and the type
            tempNodeList[j].caps = responses[j];
            tempNodeList[j].type = RF_VAL(AZA_CDCP_AUDIOW_CAP_TYPE,
                                          tempNodeList[j].caps);

            // If it is a pin node then generate the commands to be sent to get
            // the pin caps
            if (tempNodeList[j].type == AZA_CDCP_AUDIOW_CAP_TYPE_PIN)
            {
                CHECK_RC(ConstructVerb(&verb, m_Addr, i, VERB_GET_PARAMETER,
                                       AZA_CDCP_PIN_CAP));
                verbList[lwerbs] = verb;
                lwerbs++;
            }
        }
        CHECK_RC(SendCommands(&verbList[0], lwerbs, &responses[0]));

        // Store all the pin caps and create the commands to get the connection
        // list length
        lwerbs = 0;
        for (UINT32 i = startNode, j = 0, k = 0; i < startNode + nNodes; i++, j++)
        {
            if (tempNodeList[j].type == AZA_CDCP_AUDIOW_CAP_TYPE_PIN)
                tempNodeList[j].specialCaps = responses[k++];

            if (RF_VAL(AZA_CDCP_AUDIOW_CAP_CONLIST, tempNodeList[j].caps))
            {
                CHECK_RC(ConstructVerb(&verb, m_Addr, i, VERB_GET_PARAMETER,
                                       AZA_CDCP_CON_LIST_LEN));
                verbList[lwerbs] = verb;
                lwerbs++;
            }
        }
        CHECK_RC(SendCommands(&verbList[0], lwerbs, &responses[0]));

        // Store all the connection list lengths and create the commands to
        // get the lists
        vector<UINT32> vConnListVerbs;
        for (UINT32 i = startNode, j = 0, k = 0; i < startNode + nNodes; i++, j++)
        {
            if (RF_VAL(AZA_CDCP_AUDIOW_CAP_CONLIST, tempNodeList[j].caps))
            {
                UINT32 nConnections = RF_VAL(AZA_CDCP_CON_LIST_LEN_NUM, responses[k++]);
                UINT32 cIncr = (RF_VAL(AZA_CDCP_CON_LIST_LEN_LONG, value) == 1) ? 2 : 4;

                for (UINT32 ccnt = 0; ccnt < nConnections; ccnt+=cIncr)
                {
                    CHECK_RC(ConstructVerb(&verb, m_Addr, i, VERB_GET_CON_LIST_ENTRY,
                                           ccnt));
                    vConnListVerbs.push_back(verb);
                }
            }
        }

        // Arrays to store the connection list verbs and responses
        // since we do not know the maximum upfront in this case
        vector<UINT32> connListVerbs(vConnListVerbs.size());
        vector<UINT64> connListResponses(vConnListVerbs.size());

        // Colwer the connection list verb vector to an array
        for (UINT32 i = 0; i < vConnListVerbs.size(); ++i)
            connListVerbs[i] = vConnListVerbs[i];

        CHECK_RC(SendCommands(&connListVerbs[0], vConnListVerbs.size(),
                              &connListResponses[0]));

        // Store all the connection list info in the respective nodes
        UINT32 connListResponseIndex = 0;
        for (UINT32 i = startNode, j = 0, k = 0; i < startNode + nNodes; i++, j++)
        {
            if (RF_VAL(AZA_CDCP_AUDIOW_CAP_CONLIST, tempNodeList[j].caps))
            {
                UINT32 nConnections = RF_VAL(AZA_CDCP_CON_LIST_LEN_NUM, responses[k]);
                UINT32 cMask = 0xff;
                UINT32 cShift = 8;
                UINT32 cIncr = 4;
                if (RF_VAL(AZA_CDCP_CON_LIST_LEN_LONG, responses[k]) == 1)
                {
                    cMask = 0xffff;
                    cShift = 16;
                    cIncr = 2;
                }

                UINT16 nodeID;
                tempNodeList[j].clist.clear();
                for (UINT32 ccnt=0; ccnt<nConnections; ccnt+=cIncr)
                {
                    value = connListResponses[connListResponseIndex++];
                    for (UINT16 entryCnt = 0; entryCnt<cIncr; entryCnt++)
                    {
                        // ADI1986a inserts a zero element in the middle
                        // of some connection lists. Normally a zero
                        // element would indicate the end of the list, but
                        // this is only really by convention. In this case,
                        // the zero element is a placeholder, and it's
                        // important to keep it in the list for relative
                        // ordering purposes. At the same time, trailing
                        // zeros in a connection list (beyond nConnections)
                        // should not be appended.
                        if (ccnt + entryCnt < nConnections)
                        {
                            nodeID = value & cMask;
                            if (nodeID & ~(cMask>>1))
                            {
                                // Range of inputs
                                // See spec 7.1.2 for information on
                                // connection list ranges
                                nodeID &= (cMask>>1);
                                for (UINT16 oldcon = tempNodeList[j].clist.back() + 1;
                                      oldcon < nodeID; oldcon++)
                                {
                                    tempNodeList[j].clist.push_back(oldcon);
                                }
                            }
                            // Single input
                            tempNodeList[j].clist.push_back(nodeID);
                            value = value >> cShift;
                        }
                    }
                }

                ++k;

                // Can't check for equality because of node ranges mean that
                // one clist entry could account result in more than one
                // connection.
                if (tempNodeList[j].clist.size() < nConnections)
                {
                    Printf(Tee::PriError, "Number of connections (%d) smaller than expected (%d)!\n",
                           (UINT32)tempNodeList[j].clist.size(),
                           nConnections);

                    return RC::SOFTWARE_ERROR;
                }
            }
        }

        // Copy the nodes to the permanent list
        for (UINT32 i = 0; i < tempNodeList.size(); ++i)
            m_Nodes.push_back(tempNodeList[i]);

    }

    return OK;
}

const char* AzaliaCodec::FunctionGroupTypeToString(UINT32 Type)
{
    for (UINT32 i=0; i<sizeof(FGroupStrings)/sizeof(StringMapping); i++)
    {
        if (FGroupStrings[i].type == Type)
        {
            return FGroupStrings[i].str;
        }
    }
    MASSERT(!"UNKNOWN FUNCTION GROUP TYPE");
    return "UNKNOWN";
}

const AzaliaCodec::Node* AzaliaCodec::GetNode(UINT16 NodeID)
{
    // In most cases, node ID will be the index into the node list
    if ((m_Nodes.size() > NodeID) && (m_Nodes[NodeID].id == NodeID))
    {
        return &m_Nodes[NodeID];
    }
    else
    {
        vector<Node>::iterator iter = m_Nodes.begin();
        for ( ; iter != m_Nodes.end(); iter++)
        {
            if (iter->id == NodeID)
            {
                return &(*iter);
                break;
            }
        }
    }
    return NULL;
}

const vector<AzaliaCodec::Node>* AzaliaCodec::GetNodeList()
{
    return &m_Nodes;
}

RC AzaliaCodec::GetVendorDeviceID(UINT16* pVid, UINT16* pDid)
{
    RC rc;
    if (!m_Vid || !m_Did)
    {
        UINT32 value;
        CHECK_RC(SendCommand(0, VERB_GET_PARAMETER, AZA_CDCP_VENDOR_ID, &value));
        m_Vid = RF_VAL(AZA_CDCP_VENDOR_ID_VID, value);
        m_Did = RF_VAL(AZA_CDCP_VENDOR_ID_DID, value);
    }

    if (pVid)
        *pVid = m_Vid;

    if (pDid)
        *pDid = m_Did;

    return OK;
}

RC AzaliaCodec::Initialize()
{
    RC rc;
    CHECK_RC(EnumerateFunctionGroups());
    CHECK_RC(EnumerateNodes());
    CHECK_RC(PrintInfoNodeList());
    CHECK_RC(GenerateAllInputRoutes());
    CHECK_RC(GenerateAllOutputRoutes());
    CHECK_RC(GetVendorDeviceID(NULL, NULL));
    return OK;
}
const char* AzaliaCodec::NodeTypeToString(UINT32 Type)
{
    for (UINT32 i=0; i<sizeof(NodeStrings)/sizeof(StringMapping); i++)
    {
        if (NodeStrings[i].type == Type)
        {
            return NodeStrings[i].str;
        }
    }
    MASSERT(!"UNKNOWN NODE TYPE");
    return "UNKNOWN";
}

RC AzaliaCodec::PrintInputRoutes()
{
    RC rc;
    Printf(Tee::PriNormal, "Input Routes: %lu\n", (unsigned long)m_InputRoutes.size());
    Printf(Tee::PriNormal, "----------------\n");

    list<AzaliaRoute>::iterator routeIter = m_InputRoutes.begin();
    while (routeIter != m_InputRoutes.end())
    {
        CHECK_RC(routeIter->PrintInfo(Tee::PriNormal));
        routeIter++;
    }

    return OK;
}

RC AzaliaCodec::PrintOutputRoutes()
{
    RC rc;
    Printf(Tee::PriNormal, "Output Routes: %lu\n", (unsigned long)m_OutputRoutes.size());
    Printf(Tee::PriNormal, "-----------------\n");

    list<AzaliaRoute>::iterator routeIter = m_OutputRoutes.begin();
    while (routeIter != m_OutputRoutes.end())
    {
        CHECK_RC(routeIter->PrintInfo(Tee::PriNormal));
        routeIter++;
    }
    return OK;
}

RC AzaliaCodec::PrintInfoNode(Tee::Priority Pri, UINT16 NodeID)
{
    RC rc;
    Node node;
    UINT32 value;
    const Node* nodePtr = GetNode(NodeID);
    if (!nodePtr)
    {
        Printf(Tee::PriError, "Invalid node id (0x%04x)\n", NodeID);
        return RC::SOFTWARE_ERROR;
    }
    node = *nodePtr;

    // Basic ID info
    Printf(Pri, "Node 0x%x of type %s.\n", NodeID, NodeTypeToString(node.type));

    // Capabilities
    Printf(Pri, "    Capabilities: 0x%08x: %s %sInAmp %sOutAmp %sAmpOvr %sFmtOvr\n", node.caps,
           RF_VAL(AZA_CDCP_AUDIOW_CAP_STEREO, node.caps) ? "stereo":"mono",
           RF_VAL(AZA_CDCP_AUDIOW_CAP_INAMP, node.caps) ? "":"No",
           RF_VAL(AZA_CDCP_AUDIOW_CAP_OUTAMP, node.caps) ? "":"No",
           RF_VAL(AZA_CDCP_AUDIOW_CAP_AMPOVR, node.caps) ? "":"No",
           RF_VAL(AZA_CDCP_AUDIOW_CAP_FMTOVR, node.caps) ? "":"No");
    Printf(Pri, "        %sStripe %sProcWidg %sUnsol %sCList %s %sPwr %slrswap ChanCnt: %d\n",
           RF_VAL(AZA_CDCP_AUDIOW_CAP_STRIPE, node.caps) ? "":"No",
           RF_VAL(AZA_CDCP_AUDIOW_CAP_PROC, node.caps) ? "":"No",
           RF_VAL(AZA_CDCP_AUDIOW_CAP_UNSOL, node.caps) ? "":"No",
           RF_VAL(AZA_CDCP_AUDIOW_CAP_CONLIST, node.caps) ? "":"No",
           RF_VAL(AZA_CDCP_AUDIOW_CAP_DIGITAL, node.caps) ? "dig":"alog",
           RF_VAL(AZA_CDCP_AUDIOW_CAP_POWER, node.caps) ? "":"No",
           RF_VAL(AZA_CDCP_AUDIOW_CAP_SWAP, node.caps) ? "":"No",
           (RF_VAL(AZA_CDCP_AUDIOW_CAP_CHAN_CNT, node.caps) << 1 |
            RF_VAL(AZA_CDCP_AUDIOW_CAP_STEREO, node.caps)));

    // Unsolicted response enable
    if (RF_VAL(AZA_CDCP_AUDIOW_CAP_UNSOL, node.caps))
    {
        CHECK_RC(SendCommand(node.id, VERB_GET_UNSOLICITED_ENABLE, 0, &value));
        Printf(Pri, "    UnsolEnable: 0x%08x\n", value);
    }

    // Power state
    if (RF_VAL(AZA_CDCP_AUDIOW_CAP_POWER, node.caps)
        && node.type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDFGRP)
    {
        CHECK_RC(SendCommand(node.id, VERB_GET_POWER_STATE, 0, &value));
        Printf(Pri, "    PowerState: 0x%08x (%sin DO)\n", value,
               (value ? "WARNING! NOT ": ""));
    }

    // Processing state
    if (RF_VAL(AZA_CDCP_AUDIOW_CAP_PROC, node.caps))
    {
        // TODO: check other processing-related registers
        CHECK_RC(SendCommand(node.id, VERB_GET_PROCESSING_STATE, 0, &value));
        Printf(Pri, "    ProcessingState: 0x%08x %s\n", value,
               (value ? "WARNING!":""));
    }

    // Colwerter-specific state
    if (node.type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN
        || node.type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT)
    {
        CHECK_RC(SendCommand(node.id, VERB_GET_STREAM_CHANNEL_ID, 0, &value));
        Printf(Pri, "    StrmChannel: 0x%08x (Strm=0x%x, Chan=0x%x)\n",
               value, (value >> 4) & 0xf, value & 0xf);
        CHECK_RC(SendCommand(node.id, VERB_GET_STREAM_FORMAT, 0, &value));
        Printf(Pri, "    ColwerterFormat: 0x%08x\n", value);
        CHECK_RC(SendCommand(node.id, VERB_GET_DIGITAL_COLW, 0, &value));
        Printf(Pri, "    DigColwCtrl: 0x%08x (Digital colwerter %s)\n", value,
               ((RF_VAL(AZA_CDCP_AUDIOW_CAP_DIGITAL, value) && ((value & 0x1) == 0))
                ? "DISABLED" : "enabled"));
    }

    // Pin-specific state
    if (node.type == AZA_CDCP_AUDIOW_CAP_TYPE_PIN)
    {
        UINT32 pinCaps;
        CHECK_RC(SendCommand(node.id, VERB_GET_PARAMETER, AZA_CDCP_PIN_CAP, &pinCaps));
        Printf(Pri, "    PinCaps: 0x%08x %sImpSense %sPresDt %sHPhone %sOut %sIn %sHDMI\n",
               pinCaps,
               RF_VAL(AZA_CDCP_PIN_CAP_IMPEDENCE, pinCaps) ? "" : "No",
               RF_VAL(AZA_CDCP_PIN_CAP_PRESENCE, pinCaps) ? "" : "No",
               RF_VAL(AZA_CDCP_PIN_CAP_HEADPHONE, pinCaps) ? "" : "No",
               RF_VAL(AZA_CDCP_PIN_CAP_OUTPUT, pinCaps) ? "" : "No",
               RF_VAL(AZA_CDCP_PIN_CAP_INPUT, pinCaps) ? "" : "No",
               RF_VAL(AZA_CDCP_PIN_CAP_HDMI, pinCaps) ? "" : "No");

        if (RF_VAL(AZA_CDCP_PIN_CAP_PRESENCE, pinCaps)
            && RF_VAL(AZA_CDCP_PIN_CAP_IMPEDENCE, pinCaps))
        {
            CHECK_RC(SendCommand(node.id, VERB_GET_PIN_SENSE, 0, &value));
            Printf(Pri, "    PinSense: 0x%08x (%spresent)\n", value,
                   (value & 0x80000000) ? "" : "NOT ");
        }
        CHECK_RC(SendCommand(node.id, VERB_GET_PIN_WIDGET_CONTROL, 0, &value));
        Printf(Pri, "    PinWidCtrl: 0x%08x, OutEn=%d InEn=%d PhoneEn=%d\n",
               value, (value >> 6) & 0x1, (value >> 5) & 0x1, (value >> 7) & 0x1);
    }

    UINT32 firstIndex = 0;
    UINT32 lastIndex = node.clist.size();
    // Mixers do not support connection select. Show all inputs.
    if (node.type != AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOMIX
        && RF_VAL(AZA_CDCP_AUDIOW_CAP_CONLIST, node.caps)
        && node.clist.size() > 1)
    {
        // Get the connection select
        UINT32 lwrrIndex = 0;
        CHECK_RC(SendCommand(node.id, VERB_GET_CONNECTION_SELECT, 0, &lwrrIndex));
        Printf(Pri, "    ConnSelect: 0x%08x (NodeID=0x%x)\n", lwrrIndex,
               node.clist[lwrrIndex]);
        if (Pri > Tee::PriSecret)
        {
            // Print info only for active connection
            firstIndex = lwrrIndex;
            lastIndex = lwrrIndex+1;
        }
    }

    // Input amplifiers
    if (RF_VAL(AZA_CDCP_AUDIOW_CAP_INAMP, node.caps))
    {
        UINT32 ampCaps;
        // Capabilities
        if (RF_VAL(AZA_CDCP_AUDIOW_CAP_AMPOVR, node.caps))
        {
            CHECK_RC(SendCommand(node.id, VERB_GET_PARAMETER,
                                 AZA_CDCP_INAMP_CAP, &ampCaps));
        }
        else
        {
            CHECK_RC(SendCommand(node.fgroup_id, VERB_GET_PARAMETER,
                                 AZA_CDCP_INAMP_CAP, &ampCaps));
        }
        UINT32 offset = RF_VAL(AZA_CDCP_AMP_CAP_OFFSET, ampCaps);
        UINT32 numSteps = RF_VAL(AZA_CDCP_AMP_CAP_NUMSTEPS, ampCaps);
        UINT32 stepSize = RF_VAL(AZA_CDCP_AMP_CAP_STEPSIZE, ampCaps);
        FLOAT32 stepSizeDB = (stepSize+1) * 0.25f;
        bool muteCap = RF_VAL(AZA_CDCP_AMP_CAP_MUTE, ampCaps);
        Printf(Pri, "    InAmpCaps: 0x%08x. Offset=%d, numSteps=%d, stepSize=%d(%5.2fdB) %sMuteCap\n",
               ampCaps, offset, numSteps, stepSize, stepSizeDB,
               (muteCap ? "":"Not"));
        for (UINT32 index=firstIndex; index<lastIndex; index++)
        {
            UINT32 gain;
            FLOAT32 dB;
            // Left amp
            CHECK_RC(SendCommand(node.id, VERB_GET_AMPLIFIER_GAIN_MUTE,
                                 0x2000|index, &value));
            gain = value & 0x7f;
            if (gain > numSteps)
            {
                Printf(Pri, "ERROR: Index %d L input amp gain (%d) > numSteps (%d)\n",
                       index, gain, numSteps);
            }
            dB = (gain - offset) * stepSizeDB;
            Printf(Pri, "    L_InAmpGain(%d): 0x%08x (%5.2fdB) %sMuted\n", index,
                   value, dB, ((value >> 7) & 0x1 ? "":"Not"));
            // Right amp
            CHECK_RC(SendCommand(node.id, VERB_GET_AMPLIFIER_GAIN_MUTE,
                                 0x0000|index, &value));
            gain = value & 0x7f;
            if (gain > numSteps)
            {
                Printf(Pri, "ERROR: Index %d R input amp gain (%d) > numSteps (%d)\n",
                       index, gain, numSteps);
            }
            dB = (gain - offset) * stepSizeDB;
            Printf(Pri, "    R_InAmpGain(%d): 0x%08x (%5.2fdB) %sMuted\n", index,
                   value, dB, ((value >> 7) & 0x1 ? "":"Not"));
        }
    }

    // Output amplifiers
    if (RF_VAL(AZA_CDCP_AUDIOW_CAP_OUTAMP, node.caps))
    {
        UINT32 ampCaps;
        // Capabilities
        if (RF_VAL(AZA_CDCP_AUDIOW_CAP_AMPOVR, node.caps))
        {
            CHECK_RC(SendCommand(node.id, VERB_GET_PARAMETER,
                                 AZA_CDCP_OUTAMP_CAP, &ampCaps));
        }
        else
        {
            CHECK_RC(SendCommand(node.fgroup_id, VERB_GET_PARAMETER,
                                 AZA_CDCP_OUTAMP_CAP, &ampCaps));
        }
        if (m_Vid == AZA_CDC_VENDOR_SIGMATEL
            && ((m_Did == AZA_CDC_SIGMATEL_STAC9772 && node.id == 0xb)
                || (m_Did == AZA_CDC_SIGMATEL_STAC9770 && (node.id == 0xb
                                                          || node.id == 0x14))))
        {
            // Verify nodes are still broken
            // Some nodes on these codecs appear to be broken
            // Take a guess at caps to get things working properly
            ampCaps = 0x3f1f;
        }
        INT32 offset = RF_VAL(AZA_CDCP_AMP_CAP_OFFSET, ampCaps);
        UINT32 numSteps = RF_VAL(AZA_CDCP_AMP_CAP_NUMSTEPS, ampCaps);
        UINT32 stepSize = RF_VAL(AZA_CDCP_AMP_CAP_STEPSIZE, ampCaps);
        FLOAT32 stepSizeDB = (stepSize+1) * 0.25f;
        bool muteCap = RF_VAL(AZA_CDCP_AMP_CAP_MUTE, ampCaps);
        Printf(Pri, "    OutAmpCaps: 0x%08x. Offset=%d, numSteps=%d, stepSize=%d(%5.2fdB) %sMuteCap\n",
               ampCaps, offset, numSteps, stepSize, stepSizeDB,
               (muteCap ? "":"Not"));
        INT32 gain;
        FLOAT32 dB;
        // Left amp
        CHECK_RC(SendCommand(node.id, VERB_GET_AMPLIFIER_GAIN_MUTE,
                             0xa000, &value));
        gain = value & 0x7f;
        if ((UINT32) gain > numSteps)
        {
            Printf(Pri, "ERROR: L output amp gain (%d) > numSteps (%d)\n",
                   gain, numSteps);
        }
        dB = (gain - offset) * stepSizeDB;
        Printf(Pri, "    L_OutAmpGain: 0x%08x (%5.2fdB) %sMuted\n",
               value, dB, ((value >> 7) & 0x1 ? "":"Not"));
        // Right amp
        CHECK_RC(SendCommand(node.id, VERB_GET_AMPLIFIER_GAIN_MUTE,
                             0x8000, &value));
        gain = value & 0x7f;
        if ((UINT32) gain > numSteps)
        {
            Printf(Pri, "ERROR: R output amp gain (%d) > numSteps (%d)\n",
                   gain, numSteps);
        }
        dB = (gain - offset) * stepSizeDB;
        Printf(Pri, "    R_OutAmpGain: 0x%08x (%5.2fdB) %sMuted\n",
               value, dB, ((value >> 7) & 0x1 ? "":"Not"));
    }

    return OK;
}

RC AzaliaCodec::PrintInfoNodeList()
{
    constexpr auto Pri = Tee::PriDebug;
    Printf(Pri, "There are %ld nodes on codec on sdin %d\n",
           static_cast<long>(m_Nodes.size()), m_Addr);
    for (UINT32 i=0; i<m_Nodes.size(); i++)
    {
        Node node = m_Nodes[i];
        Printf(Pri, "Node 0x%x (node ID 0x%x of group %d) is of type %s\n",
               i, node.id, node.fgroup_id, NodeTypeToString(node.type));
        if (node.clist.size() > 0)
        {
            Printf(Pri, "    Has %ld inputs from nodes ",
                   static_cast<long>(node.clist.size()));
            for (UINT32 j=0; j<node.clist.size(); j++)
            {
                Printf(Pri, "0x%x, ", node.clist[j]);
            }
            Printf(Pri, "\n");
        }
    }
    return OK;
}

RC AzaliaCodec::SendCommands(UINT32* pCommands, UINT32 nCommands,
                             UINT64* pResponses)
{
    RC rc;
    UINT16 value1 = 0;
    UINT16 value2 = 0;
    // Verify that CORB is empty
    CHECK_RC(m_pAza->CorbGetFreeSpace(&value1));
    CHECK_RC(m_pAza->CorbGetTotalSize(&value2));
    if (value1 != value2)
    {
        Printf(Tee::PriError, "AzaliaCodec::SendCommands called when there are pending commands!\n");
        return RC::SOFTWARE_ERROR;
    }
    // Verify that RIRB and response queues are emtpy
    CHECK_RC(m_pAza->RirbGetNSolicitedResponses(m_Addr, &value1));
    CHECK_RC(m_pAza->RirbGetNResponses(&value2));
    if (value1 > 0 || value2 > 0)
    {
        Printf(Tee::PriError, "AzaliaCodec::SendCommands called when there are pending responses!\n");
        return RC::SOFTWARE_ERROR;
    }

    AzaCorbPushPollArgs pollArgs;
    pollArgs.pAzalia = m_pAza;
    pollArgs.lwerbs = nCommands;
    pollArgs.verbs = pCommands;
    rc = Tasker::Poll(AzaCorbPushWaitPoll, &pollArgs, 5000*nCommands);
    if (rc != OK)
    {
        Printf(Tee::PriError, "Timeout error pushing CORB commands!\n");
        return RC::TIMEOUT_ERROR;
    }

    Printf(Tee::PriDebug, "AzaliaCodec::SendCommands Pushed %d commands\n",
           nCommands);

    UINT32 time = nCommands;
    UINT16 nResponses = 0;
#ifdef SIM_BUILD
    time = nCommands * AzaliaUtilities::GetTimeScaler();
    while (time > 0)
    {
        AzaliaUtilities::Pause(1);
        CHECK_RC(m_pAza->RirbGetNResponses(&nResponses));

        if (nResponses >= nCommands)
            break;
        Printf(Tee::PriDebug, "RirbWaitReponse::Continuing to wait %d time units\n",
               time);
        time -= 20;
    }
    AzaliaUtilities::Pause(1);
#else
    AzaliaUtilities::Pause(time);
    CHECK_RC(m_pAza->RirbGetNResponses(&nResponses));
#endif

    UINT16 nPopped = 0;
    CHECK_RC(m_pAza->RirbPop(pResponses, nCommands, &nPopped));
    Printf(Tee::PriDebug, "AzaliaCodec::SendCommands Popped %d responses\n",
           nPopped);

    if (nCommands != nPopped)
    {
        Printf(Tee::PriError, "AzaliaCodec::SendCommands did not receive the required number of"
                              "responses from the codec\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC AzaliaCodec::SendCommand(UINT32 NodeID, UINT32 VerbID,
                            UINT32 Payload, UINT32* pResponse)
{
    RC rc;
    UINT16 value1 = 0;
    UINT16 value2 = 0;
    // Verify that CORB is empty
    CHECK_RC(m_pAza->CorbGetFreeSpace(&value1));
    CHECK_RC(m_pAza->CorbGetTotalSize(&value2));
    if (value1 != value2)
    {
        Printf(Tee::PriError, "AzaliaCodec::SendCommand called when there are pending commands!\n");
        return RC::SOFTWARE_ERROR;
    }
    // Verify that RIRB and response queues are emtpy
    CHECK_RC(m_pAza->RirbGetNSolicitedResponses(m_Addr, &value1));
    CHECK_RC(m_pAza->RirbGetNResponses(&value2));
    if (value1 > 0 || value2 > 0)
    {
        Printf(Tee::PriError, "AzaliaCodec::SendCommand called when there are pending responses!\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(SubmitVerb(NodeID, VerbID, Payload));

    // Codec should send a response back on the next frame. Wait until next
    // frame to process RIRB
    //
    // NOTE: This was changed to polling periodically for simulation purposes
    // since Pause leads to long wait times. We still need the threshold of 1ms
    // since this is the maximum time to wait. Changing it to Poll helps us stops
    // waiting at times when the response comes back faster
    //
#ifdef SIM_BUILD
    UINT32 time = 1 * AzaliaUtilities::GetTimeScaler();
    //UINT32 maxSingleWaitTime = AzaliaUtilities::GetMaxSingleWaitTime();
    UINT16 nResponses = 0;
    while (time > 0)
    {
        AzaliaUtilities::Pause(1);
        CHECK_RC(m_pAza->RirbGetNResponses(&nResponses));

        if (nResponses > 0)
            break;
        Printf(Tee::PriDebug, "RirbWaitReponse::Continuing to wait %d time units\n", time);
        time -= 20;
    }
    AzaliaUtilities::Pause(1);
#else
    AzaliaUtilities::Pause(1);
#endif

    CHECK_RC(m_pAza->RirbProcess());

    UINT32 tResponse = 0;
    CHECK_RC(m_pAza->RirbGetSolicitedResponse(m_Addr, &tResponse));
    if (pResponse)
    {
        *pResponse = tResponse;
    }
    return OK;
}

RC AzaliaCodec::SubmitVerb(UINT32 NodeID, UINT32 VerbID, UINT32 Payload)
{
    RC rc;
    UINT32 verb;
    CHECK_RC(ConstructVerb(&verb, m_Addr, NodeID, VerbID, Payload));

    AzaCorbPushPollArgs pollArgs;
    pollArgs.pAzalia = m_pAza;
    pollArgs.lwerbs = 1;
    pollArgs.verbs = &verb;
    rc = Tasker::Poll(AzaCorbPushWaitPoll, &pollArgs, 5000);

    if (rc != OK)
    {
        Printf(Tee::PriError, "Timeout error pushing CORB command!\n");
        return RC::TIMEOUT_ERROR;
    }
    return rc;
}

RC AzaliaCodec::GenerateAllInputRoutes()
{
    RC rc;
    m_InputRoutes.clear();
    // All input routes begin with the fgroup node, followed by
    // an input colwerter.
    vector<AzaliaCodec::Node>::const_iterator iter = m_Nodes.begin();
    for ( ; iter != m_Nodes.end(); iter++)
    {
        if (iter->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN)
        {
            AzaliaRoute newRoute(this);
            newRoute.AddNode(iter->fgroup_id);
            newRoute.AddNode(iter->id);
            m_InputRoutes.push_back(newRoute);
            // Generate routes relwrsively until we reach a pin
            // or hit MAX_ROUTE_LENGTH
            CHECK_RC(GenerateRoutesRelwrsively(&m_InputRoutes,
                                               AZA_CDCP_AUDIOW_CAP_TYPE_PIN, 0));
        }
    }
    // Prune out any routes that did not hit a pin
    list<AzaliaRoute>::iterator routeIter = m_InputRoutes.begin();
    while (routeIter != m_InputRoutes.end())
    {
        list<AzaliaRoute>::iterator routeToKill = routeIter;
        routeIter++;
        if (!routeToKill->IsValidRoute())
        {
            m_InputRoutes.erase(routeToKill);
        }
    }
    m_InputRoutes.sort();

    // Move the digital pin routes to the end since we dont
    // want to use these by default when the use doesn't specify a required pin
    list<AzaliaRoute> temp;
    routeIter = m_InputRoutes.begin();
    while (routeIter != m_InputRoutes.end())
    {
        list<AzaliaRoute>::iterator routeToMove = routeIter;
        routeIter++;
        if (routeToMove->IsDigitalPin())
        {
            temp.push_back(*routeToMove);
            m_InputRoutes.erase(routeToMove);
        }
    }

    routeIter = temp.begin();
    while (routeIter != temp.end())
    {
        m_InputRoutes.push_back(*routeIter);
        routeIter++;
    }

    return OK;
}

RC AzaliaCodec::GenerateAllOutputRoutes()
{
    RC rc;
    m_OutputRoutes.clear();
    // We build output routes in reverse: we start from an output-capable
    // pin and follow the input connections back to a colwerter.
    // Once we're done, we reverse the route.
    vector< AzaliaCodec::Node>::const_iterator iter = m_Nodes.begin();
    for ( ; iter != m_Nodes.end(); iter++)
    {
        if (iter->type == AZA_CDCP_AUDIOW_CAP_TYPE_PIN
            && RF_VAL(AZA_CDCP_PIN_CAP_OUTPUT, iter->specialCaps))
        {
            AzaliaRoute newRoute(this);
            CHECK_RC(newRoute.AddNode(iter->id));
            m_OutputRoutes.push_back(newRoute);
            // Generate routes relwrsively until we reach an output
            // colwerter or hit MAX_ROUTE_LENGTH
            CHECK_RC(GenerateRoutesRelwrsively(&m_OutputRoutes,
                                               AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT, 0));
        }
    }
    // Prune out any routes that did not hit a colwerter
    list<AzaliaRoute>::iterator routeIter = m_OutputRoutes.begin();
    while (routeIter != m_OutputRoutes.end())
    {
        list<AzaliaRoute>::iterator routeToKill = routeIter;
        routeIter++;
        if (GetNode(routeToKill->LastNode())->type
            != AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT)
        {
            routeToKill->PrintInfo(Tee::PriDebug);
            m_OutputRoutes.erase(routeToKill);
        }
        else
        {
            routeToKill->PrintInfo(Tee::PriDebug);
            // This is a real route, so add the function group node and
            // reverse the route
            CHECK_RC(routeToKill->AddNode(GetNode(routeToKill->LastNode())->fgroup_id));
            CHECK_RC(routeToKill->Reverse());
        }
    }
    m_OutputRoutes.sort();

    // Move the digital pin routes to the end since we dont
    // want to use these by default when the use doesn't specify a required pin
    list<AzaliaRoute> temp;
    routeIter = m_OutputRoutes.begin();
    while (routeIter != m_OutputRoutes.end())
    {
        list<AzaliaRoute>::iterator routeToMove = routeIter;
        routeIter++;
        if (routeToMove->IsDigitalPin())
        {
            temp.push_back(*routeToMove);
            m_OutputRoutes.erase(routeToMove);
        }
    }

    routeIter = temp.begin();
    while (routeIter != temp.end())
    {
        m_OutputRoutes.push_back(*routeIter);
        routeIter++;
    }

    return OK;
}

#define MAX_ROUTE_LENGTH 5
RC AzaliaCodec::GenerateRoutesRelwrsively(list<AzaliaRoute>* pRList,
                                          UINT32 DestType,
                                          UINT32 LwrrDepth)
{
    // We want to expand the last route on the list using a depth-first
    // algorithm. Basically, at each level, we pop the last route from
    // the list and replace it with a set of longer routes containing
    // the original route.
    RC rc;
    AzaliaRoute lastRoute = pRList->back();
    pRList->pop_back();
    LwrrDepth++;
    if (LwrrDepth < MAX_ROUTE_LENGTH)
    {
        const AzaliaCodec::Node* pLastNode = GetNode(lastRoute.LastNode());
        MASSERT(pLastNode);
        for (UINT32 i=0; i<pLastNode->clist.size(); i++)
        {
            const AzaliaCodec::Node* pNextNode = GetNode(pLastNode->clist[i]);
            AzaliaRoute newRoute = lastRoute;
            CHECK_RC(newRoute.AddNode(pNextNode->id));
            pRList->push_back(newRoute);
            if (pNextNode->type != DestType)
            {
                // Not an endpoint--keep relwrsing
                CHECK_RC(GenerateRoutesRelwrsively(pRList, DestType, LwrrDepth));
            }
        }
    }
    return OK;
}

RC AzaliaCodec::FindRoute(AzaliaRoute** ppRoute, AzaliaDmaEngine::DIR Dir,
                          AzaliaFormat* pFormat, bool IsMutable, UINT32 PinNum)
{
    MASSERT(ppRoute);
    *ppRoute = NULL;
    list<AzaliaRoute>* pRouteList = NULL;
    switch (Dir)
    {
        case AzaliaDmaEngine::DIR_INPUT:
            pRouteList = &m_InputRoutes;
            break;
        case AzaliaDmaEngine::DIR_OUTPUT:
            pRouteList = &m_OutputRoutes;
            break;
        default:
            Printf(Tee::PriError, "No such thing as a bidirectional stream!\n");
            return RC::BAD_PARAMETER;
            break;
    }

    list<AzaliaRoute>::iterator routeIter;
    UINT32 routeNum = 0;
    for (routeIter = pRouteList->begin(); routeIter != pRouteList->end(); routeIter++, ++routeNum)
    {
        if (!routeIter->IsReserved()
            && (!PinNum || routeIter->Contains(PinNum)) // either don't care, or check for reserved pin
            && routeIter->IsSupported(pFormat))
        {
            // Found a match
            *ppRoute = &(*routeIter);
            (*ppRoute)->Reserve();
            // Take all routes that overlap this one out of cirlwlation
            list<AzaliaRoute>::iterator otherIter;
            for (otherIter = m_InputRoutes.begin();
                 otherIter != m_InputRoutes.end();
                 otherIter++)
            {
                if (!otherIter->IsReserved()
                    && otherIter->Overlaps(*ppRoute))
                {
                    otherIter->Reserve();
                }
            }
            for (otherIter = m_OutputRoutes.begin();
                 otherIter != m_OutputRoutes.end();
                 otherIter++)
            {
                if (!otherIter->IsReserved()
                    && otherIter->Overlaps(*ppRoute))
                {
                    otherIter->Reserve();
                }
            }
            break;
        }
    }

    return OK;
}

RC AzaliaCodec::MapPins()
{
    RC rc;
    UINT32 value;

    Printf(Tee::PriNormal, "Pin mapping information for codec on sdin %d\n", m_Addr);
    // This will print codec name and default loopback info
    const CodecDefaultInformation* pCDInfo;
    CHECK_RC(GetCodecDefaults(&pCDInfo));
    Printf(Tee::PriLow, "    Warning: location/color information is a guide only\n");
    Printf(Tee::PriLow, "    and may not be accurate, especially on paddle cards!\n");

    for (UINT32 ncnt=0; ncnt<m_Nodes.size(); ncnt++)
    {
        if (m_Nodes[ncnt].type == AZA_CDCP_AUDIOW_CAP_TYPE_PIN)
        {
            UINT32 caps = m_Nodes[ncnt].caps;
            UINT32 pinCaps = m_Nodes[ncnt].specialCaps;
            const char* jackStatus = NULL;
            if (RF_VAL(AZA_CDCP_PIN_CAP_PRESENCE, pinCaps))
            {
                CHECK_RC(SendCommand(m_Nodes[ncnt].id, VERB_GET_PIN_SENSE, 0, &value));
                if (value & 0x80000000)
                    jackStatus = "1";
                else
                    jackStatus = "0";
            }
            else
                jackStatus = "X";

            Printf(Tee::PriNormal, "Pin %3d (0x%02x). Connected:%s Digital:%d Out:%d In:%d Stereo:%d\n",
                   m_Nodes[ncnt].id, m_Nodes[ncnt].id, jackStatus,
                   RF_VAL(AZA_CDCP_AUDIOW_CAP_DIGITAL, caps),
                   RF_VAL(AZA_CDCP_PIN_CAP_OUTPUT, pinCaps),
                   RF_VAL(AZA_CDCP_PIN_CAP_INPUT, pinCaps),
                   RF_VAL(AZA_CDCP_AUDIOW_CAP_STEREO, caps));
            Printf(Tee::PriLow, "    ACAP=0x%08x, PCAP=0x%08x\n", caps, pinCaps);

            UINT32 cfgDefault = 0;
            CHECK_RC(SendCommand(m_Nodes[ncnt].id, VERB_GET_CONFIG_DEFAULT,
                                 0, &cfgDefault));
            const char *port, *lochi, *loclo, *color;
            switch (RF_VAL(AZA_CDCV_GCONFIG_DEFAULT_PORT, cfgDefault))
            {
                case AZA_CDCV_GCONFIG_DEFAULT_PORT_JACK: port = "Jack"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_PORT_NONE:  port = "Unconnected"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_PORT_FIXED: port = "Fixed Device"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_PORT_BOTH:  port = "Jack and Device"; break;
                default:
                    port = "ERROR PORT CONNECTION";
                    Printf(Tee::PriWarn, "Invalid Port Connection\n");
            }
            switch (RF_VAL(AZA_CDCV_GCONFIG_DEFAULT_LOCHI, cfgDefault))
            {
                case AZA_CDCV_GCONFIG_DEFAULT_LOCHI_EXT: lochi = "External"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCHI_INT: lochi = "Internal"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCHI_SEP: lochi = "Separate"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCHI_OTH: lochi = "Other"; break;
                default:
                    lochi = "ERROR LOCATION HI";
                    Printf(Tee::PriWarn, "Invalid Location HI\n");
            }
            switch (RF_VAL(AZA_CDCV_GCONFIG_DEFAULT_LOCLO, cfgDefault))
            {
                case AZA_CDCV_GCONFIG_DEFAULT_LOCLO_NA:     loclo = "NA"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCLO_REAR:   loclo = "Rear"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCLO_FRONT:  loclo = "Front"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCLO_LEFT:   loclo = "Left"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCLO_RIGHT:  loclo = "Right"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCLO_TOP:    loclo = "Top"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCLO_BOTTOM: loclo = "Bottom"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCLO_SPEC7:  loclo = "Rear/riser/MobileInside"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCLO_SPEC8:  loclo = "Drive/HDMI/MobileOutside"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_LOCLO_SPEC9:  loclo = "ATAPI"; break;
                default:
                    loclo = "ERROR LOCATION LO";
                    Printf(Tee::PriWarn, "Invalid Location LO\n");
            }
            switch (RF_VAL(AZA_CDCV_GCONFIG_DEFAULT_COLOR, cfgDefault))
            {
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_UNKNOWN: color = "UNKNOWN"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_BLACK:   color = "Black"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_GREY:    color = "Grey"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_BLUE:    color = "Blue"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_GREEN:   color = "Green"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_RED:     color = "Red"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_ORANGE:  color = "Orange"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_YELLOW:  color = "Yellow"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_PURPLE:  color = "Purple"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_PINK:    color = "Pink"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_WHITE:   color = "White"; break;
                case AZA_CDCV_GCONFIG_DEFAULT_COLOR_OTHER:   color = "Other color"; break;
                default:
                    color = "ERROR COLOR VALUE";
                    Printf(Tee::PriWarn, "Invalid Color Value\n");
            }
            Printf(Tee::PriLow, "    Location: %s %s %s %s\n",
                   lochi, port, loclo, color);
        }
    }
    return OK;
}

RC AzaliaCodec::GetCodecDefaults(const CodecDefaultInformation** pCDInfo)
{
    MASSERT(pCDInfo);
    *pCDInfo = NULL;
    // Search for codec defaults first time through
    if (m_DefaultInfoIndex == 0xffff)
    {
        // Search codecs from back to front, on the theory that we're more
        // likely to encounter a newer codec. This may be a bogus theory,
        // but we had to start at either the beginning or the end. :)
        for (m_DefaultInfoIndex = sizeof(pCodecDefaults)/sizeof(CodecDefaultInformation);
              m_DefaultInfoIndex > 0; m_DefaultInfoIndex--)
        {
            if (m_Vid == pCodecDefaults[m_DefaultInfoIndex-1].vendorID &&
                ((m_Vid == AZA_CDC_VENDOR_LWIDIA) ||
                 (m_Did == pCodecDefaults[m_DefaultInfoIndex-1].deviceID)))
            {
                break;
            }
        }
        if (!m_DefaultInfoIndex)
        {
            Printf(Tee::PriWarn, "Unknown codec (0x%08x)! "
                                 "Using default values for codec settings!\n",
                   (UINT32) (m_Vid<<16) | m_Did);
        }
        else
        {
            m_DefaultInfoIndex--; // remove off-by-one
            Printf(Tee::PriLow, "Codec on %d is %s, [default loopback from %d to %d]\n",
                   m_Addr, pCodecDefaults[m_DefaultInfoIndex].name,
                   pCodecDefaults[m_DefaultInfoIndex].output,
                   pCodecDefaults[m_DefaultInfoIndex].input);
        }
    }
    *pCDInfo = &pCodecDefaults[m_DefaultInfoIndex];
    return OK;
}

RC AzaliaCodec::PowerUpAllWidgets()
{
    LOG_ENT();
    RC rc = OK;
    bool powerChangeRequired = false;

    vector<UINT32> verbs(m_Nodes.size());
    vector<UINT64> responses(m_Nodes.size());
    UINT32 lwerbs = 0;
    UINT32 verb;

    // Construct the verbs for all the nodes
    for (UINT32 ncnt=0; ncnt<m_Nodes.size(); ncnt++)
    {
        AzaliaCodec::Node* pNode = &m_Nodes[ncnt];
        if (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDFGRP ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_MODFGRP ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_OTHFGRP ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_POWER ||
            (RF_VAL(AZA_CDCP_AUDIOW_CAP_POWER, pNode->caps) &&
              (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN ||
               pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT ||
               pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOSEL ||
               pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOMIX ||
               pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_PIN)))
        {
            CHECK_RC(ConstructVerb(&verb, m_Addr, pNode->id, VERB_GET_POWER_STATE, 0));
            verbs[lwerbs++] = verb;
        }
    }

    CHECK_RC(SendCommands(&verbs[0], lwerbs, &responses[0]));
    lwerbs = 0;

    // Construct commands for nodes that need to be powered
    for (UINT32 ncnt=0, j = 0; ncnt<m_Nodes.size(); ncnt++)
    {
        AzaliaCodec::Node* pNode = &m_Nodes[ncnt];
        if (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDFGRP ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_MODFGRP ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_OTHFGRP ||
            pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_POWER ||
            (RF_VAL(AZA_CDCP_AUDIOW_CAP_POWER, pNode->caps) &&
              (pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN ||
               pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT ||
               pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOSEL ||
               pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOMIX ||
               pNode->type == AZA_CDCP_AUDIOW_CAP_TYPE_PIN)))
        {
            if (RF_VAL(7:4, responses[j]))
            {
                powerChangeRequired = true;
                Printf(Tee::PriDebug, "Moving node 0x%x to D0. (Power state = 0x%08llx)\n",
                       pNode->id,
                       responses[j]);
                j++;
                CHECK_RC(ConstructVerb(&verb, m_Addr, pNode->id, VERB_SET_POWER_STATE, 0));
                verbs[lwerbs++] = verb;
            }
        }
    }

    if (lwerbs > 0)
        CHECK_RC(SendCommands(&verbs[0], lwerbs, &responses[0]));

    if (powerChangeRequired)
    {
        // Pause to let state change take effect. I'm just guessing at
        // the 100ms pause time here--the max length of a D3->D0 power
        // transition is not spec'd as far as I can tell.
        Tasker::Sleep(100);
    }

    LOG_EXT();
    return OK;
}

RC AzaliaCodec::SetDIPTransmitControl(UINT32 NodeID, bool Enable)
{
    RC rc;
    vector<Node>::iterator iterNodes;

    for (iterNodes = m_Nodes.begin();
         iterNodes != m_Nodes.end();
         iterNodes ++)
    {
        if (iterNodes->id == NodeID)
        {
            break;
        }
    }

    if (iterNodes == m_Nodes.end())
    {
        Printf(Tee::PriHigh, "Bad Pin: %d\n", NodeID);
        return RC::SOFTWARE_ERROR;
    }

    UINT32 payload = Enable? 0xC0: 0x00;
    CHECK_RC(SendCommand(NodeID, VERB_SET_DIP_TRANSMIT_CONTROL, payload));

    return OK;
}

bool AzaliaCodec::IsFormatSupported(UINT32 Dir, UINT32 Rate, UINT32 Size)
{
    list<AzaliaRoute>::iterator otherIter;

    AzaliaFormat format = AzaliaFormat(AzaliaFormat::ToType(0),
                                       AzaliaFormat::ToSize(Size),
                                       AzaliaFormat::ToRate(Rate),
                                       AzaliaFormat::ToChannels(2));

    if (Dir == AzaliaDmaEngine::DIR_INPUT)
    {
        // Codecs that don't support a routable input path
        // just have a direct memory data path to the input buffers
        // So they support all formats
        if (!IsSupportRoutableInputPath())
        {
            Printf(Tee::PriDebug, "All formats supported by non-routable input paths\n");
            return true;
        }

        for (otherIter = m_InputRoutes.begin();
             otherIter != m_InputRoutes.end();
             otherIter++)
            if (otherIter->IsSupported(&format))
                return true;
    }

    if (Dir == AzaliaDmaEngine::DIR_OUTPUT)
    {
        for (otherIter = m_OutputRoutes.begin();
             otherIter != m_OutputRoutes.end();
             otherIter++)
            if (otherIter->IsSupported(&format))
                return true;
    }

    return false;
}

RC AzaliaCodec::EnableUnsolicitedResponsesOnAllPins(bool IsEnable)
{
    RC rc;

    vector<UINT32> verbList(m_Nodes.size());
    vector<UINT64> responses(m_Nodes.size());
    UINT32 verb = 0;
    UINT32 lwerbs = 0;

    for (UINT32 node = 0; node < m_Nodes.size(); ++node)
    {
        if (m_Nodes[node].caps & 0x00000080)
        {
            UINT32 unsolTag = m_Nodes[node].id;
            if (m_Vid == AZA_CDC_CMEDIA_CMI9880 && m_Nodes[node].id==0x13)
                Printf(Tee::PriLow, "Skipping unsolicited responses enable of CMI9880 node 0x13!\n");
            else
            {
                CHECK_RC(ConstructVerb(&verb, m_Addr, m_Nodes[node].id,
                                       VERB_SET_UNSOLICITED_ENABLE,
                                       ((IsEnable?0x80:0x00)|unsolTag)));
                verbList[lwerbs++] = verb;
                Printf(Tee::PriLow, "Enabling unsolicited responses on node 0x%08x,"
                                    "with tag 0x%08x\n",
                       m_Nodes[node].id, unsolTag);
            }
        }
    }

    CHECK_RC(SendCommands(&verbList[0], lwerbs, &responses[0]));

    return OK;
}

RC AzaliaCodec::GetUnsolRespRoutes(bool CheckInput, bool CheckOutput,
                                   vector<AzaliaRoute*>* pRoutes, UINT32 RespTag)
{
    list<AzaliaRoute>::iterator routePtr;
    if (CheckInput)
    {
        for (routePtr = m_InputRoutes.begin(); routePtr != m_InputRoutes.end();
              routePtr++)
        {
            if (routePtr->CouldBeUnsolRespSoure(RespTag))
                pRoutes->push_back(&(*routePtr));
        }
    }
    if (CheckOutput)
    {
        for (routePtr = m_OutputRoutes.begin(); routePtr != m_OutputRoutes.end();
              routePtr++)
        {
            if (routePtr->CouldBeUnsolRespSoure(RespTag))
                pRoutes->push_back(&(*routePtr));
        }
    }
    return OK;
}

RC AzaliaCodec::EnableUnsolicitedResponses(bool IsEnable)
{
    RC rc;

    list<AzaliaRoute>::iterator routePtr;
    UINT32 tag;
    for (tag = 0, routePtr = m_OutputRoutes.begin();
          routePtr != m_OutputRoutes.end();
          ++routePtr, ++tag)
    {
        const Node* pinNode = GetNode((*routePtr).LastNode());
        bool isUnsolCapable = RF_VAL(AZA_CDCP_AUDIOW_CAP_UNSOL, pinNode->caps);
        if ((*routePtr).IsJackSenseCapable() && isUnsolCapable)
        {
            // Set the reposne tag as the codec address
            CHECK_RC( (*routePtr).SetUnsolRespTag(tag) );

            // Enable jack sense and unsol responses at the pin
            CHECK_RC((*routePtr).EnableJackSenseUnsolResp(true));
        }
    }

    return OK;
}

bool AzaliaCodec::IsSupportRoutableInputPath()
{
    if (m_Vid == AZA_CDC_VENDOR_LWIDIA)
        return false;

    return true;
}
