/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "gpu/include/gpusbdev.h"

class Fsp
{
private:
    static constexpr UINT08 s_FspMsgHeaderSize         = 8;
    static constexpr UINT08 s_FspMsgPayloadSize        = 8;
    static constexpr UINT08 s_FspMsgPayloadHdrSize     = 2;
    static constexpr UINT08 s_FspMsgPayloadAddDataSize = 6;
    static constexpr UINT08 s_FspCmdResponseSizeDwords = 5;

public:
    explicit Fsp(TestDevice *pDev);
    ~Fsp();
    
    // The MODS use case we are using the FSP management partition for
    enum class DataUseCase : UINT08
    {
        Ecc = 0x1,
        Floorsweep,
        HbmFreq,
        ResetCoupling,
        RowRemap
    };

    // Whether we want the FSP use case to be applied persistently or only once
    enum class DataFlag : UINT08
    {
        OneShot     = 0x1 << 0,   // Applies for the next reset
        Persistent  = 0x1 << 1    // Applied to the inforom, persistent across resets
    };

#pragma pack(push)  // Push current alignment to stack
#pragma pack(1)     // Set alignment to 1 byte boundary
    typedef union
    {
        UINT08 data[s_FspMsgHeaderSize];
        struct fields
        {
            UINT08 version;
            UINT08 destEID;
            UINT08 sourceEID;
            UINT08 msgTags;
            UINT08 msgType;
            UINT16 vendorId;
            UINT08 lwMsgType;
        } fields;
    } MsgHeader;

    typedef union
    {
        UINT08 data[s_FspMsgPayloadSize];
        struct fields
        {
            UINT08 modsSubMsgType;
            UINT08 flags;
            UINT08 additionalData[s_FspMsgPayloadAddDataSize];
        } fields;
    } MsgPayload;

    typedef union
    {
        UINT32 data[s_FspCmdResponseSizeDwords];
        struct fields
        {
            MsgHeader responseHeader;
            UINT32 taskId;
            UINT32 failedMsgType;
            UINT32 errorCode;
        } fields;
    } CmdResponse;
#pragma pack(pop)

    RC SendFspMsg
    (
        Fsp::DataUseCase useCase,
        Fsp::DataFlag flag,
        Fsp::CmdResponse *pResponse
    );

    RC SendFspMsg
    (
        Fsp::DataUseCase useCase,
        Fsp::DataFlag flag,
        const vector<UINT08> &additionalData,
        Fsp::CmdResponse *pResponse
    );
    RC PollFspInitialized();

private:
    RC AllocateResources();

    RC ConstructFspMsg
    (
        Fsp::DataUseCase useCase,
        Fsp::DataFlag flag,
        const vector<UINT08> &additionalData,
        vector<UINT08> *pFspMsg,
        UINT32 *pFspMsgSizeBytes
    ) const;

    void FreeResources();
    RC PollFspQueueEmpty();
    RC PollFspResponseComplete(Fsp::CmdResponse *pResponse);

    // Routines needed in emulation for stability.
    RC PollMsgQueueHeadInitialized();


    static constexpr UINT08 s_FspEmemModsChannel     = 0x1;
    static constexpr UINT32 s_FspQueueSize           = 0x400;
    static constexpr UINT08 s_FspHeaderVendorTypePci = 0x7E;
    static constexpr UINT16 s_FspHeaderVendorIdLw    = 0x10DE;
    static constexpr UINT08 s_FspHeaderMsgTypePrc    = 0x13;
    static constexpr UINT08 s_FspDataSubMsgIdMods    = 0x1;
    static constexpr UINT32 s_FspModsQueueStartOffset = s_FspQueueSize * s_FspEmemModsChannel;
    static constexpr UINT32 s_FspTimeoutMs            = 60000;

    //! Global mutex to serialize calls by multiple threads
    static void *s_Mutex;
    static UINT32 s_RefCount;
    TestDevice *m_pDev;

};
