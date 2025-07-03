/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "core/include/setget.h"

class FalconUCode
{
public:
    enum class UCodeDescType : UINT08
    {
        LEGACY_V1,
        LEGACY_V2,
        GFW_Vx
    };

    enum class InterfaceId : UINT32
    {
        NONE = 0,
        DMEM_MAPPER = 4,
        MULTIPLE_TARGET = 5
    };

    struct UCodeHeaderInfo
    {
        UINT08 flags    = 0;
        UINT08 version  = 0;
        UINT16 descSize = 0;
    };
    struct UCodeInfo
    {
        bool   bIsEncrypted;
        UINT32 intrDataOffsetBytes = 0;
        UINT32 imemBase            = 0;
        UINT32 dmemBase            = 0;
        UINT32 secStart            = 0;
        UINT32 secEnd              = 0;
        UINT32 engIdMask           = 0;
        UINT32 ucodeId             = 0;
        UINT32 pkcDataOffsetBytes  = 0;
        UINT32 sigCount            = 0;
        UINT32 sigVersions         = 0;
        UINT32 sigSizeBytes        = 0;
        UINT32 sigImageOffsetBytes = 0;
    };

#pragma pack(push)  // Push current alignment to stack
#pragma pack(1)     // Set alignment to 1 byte boundary
    // From https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/Falcon_Ucode/Application_Interface_Table/Spec_V1
    struct AppTableHeader
    {
        UINT08 version;
        UINT08 headerSize;
        UINT08 entrySize;
        UINT08 entryCount;
    };
    struct AppTableEntry
    {
        InterfaceId interfaceId;
        UINT32 interfacePtr;
    };

    // From https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/Falcon_Ucode/DMEM_Mapper_Interface/Spec_V3
    struct DmemMapper
    {
        UINT32 sign;
        UINT16 version;
        UINT16 structureSize;
        UINT32 cmdInputBufOffset;
        UINT32 cmdInputBufSize;
        UINT32 cmdOutputBufOffset;
        UINT32 cmdOutputBufSize;
        UINT32 imgDataBufOffset;
        UINT32 imgDataBufSize;
        UINT32 printfHeaderOffset;
        UINT32 buildTimeOffset;
        UINT32 ucodeSignOffset;
        UINT32 initCmd;
        UINT32 features;
        UINT32 cmdMask0;
        UINT32 cmdMask1;
        UINT32 multiTgtInterfaceOffset;
    };
    // From https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/Falcon_Ucode/Multiple-Target_Interface/Spec_V1
    struct MultiTgtInterface
    {
        UINT16 version;
        UINT16 size;
        UINT16 appVersion;
        UINT16 appFeatures;
        UINT08 targetId;
        UINT08 loadType;
        UINT16 reserved;
        UINT32 initialStack;
    };
#pragma pack(pop)

    static RC ReadBinary(const string& filename, vector<UINT32>* pBinary);
    static RC ParseBinary
    (
        const vector<UINT32>& uCodeBinary,
        vector<UINT32> *pImemBinary,
        vector<UINT32> *pDmemBinary,
        UCodeInfo *pUcodeInfo,
        UCodeDescType descType
    );

    static RC GetInterfaceOffset
    (
        const UCodeInfo &ucodeInfo,
        vector<UINT32> *pDmemBinary,
        InterfaceId interfaceId,
        UINT32 *pInterfaceOffset
    );
    static RC GetDmemMapper
    (
        const UCodeInfo &ucodeInfo,
        vector<UINT32> *pDmemBinary,
        DmemMapper **ppDmemMapper
    );
    static RC GetMultiTgtInterface
    (
        const UCodeInfo &ucodeInfo,
        vector<UINT32> *pDmemBinary,
        MultiTgtInterface **ppMultiTarget
    );
    static RC AddSignature
    (
        const vector<UINT32>& uCodeBinary,
        const UCodeInfo &ucodeInfo,
        UINT32 ucodeVersion,
        vector<UINT32> *pDmemBinary
    );

private:
    static void ParseBinaryGfwHeader
    (
        const vector<UINT32>& uCodeBinary,
        UCodeHeaderInfo *pHeader
    );
    static RC ParseBinaryLegacyV1
    (
        const vector<UINT32>& uCodeBinary,
        vector<UINT32> *pImemBinary,
        vector<UINT32> *pDmemBinary,
        UCodeInfo *pUcodeInfo
    );
    static RC ParseBinaryLegacyV2
    (
        const vector<UINT32>& uCodeBinary,
        vector<UINT32> *pImemBinary,
        vector<UINT32> *pDmemBinary,
        UCodeInfo *pUcodeInfo
    );
    static RC ParseBinaryGfwVx
    (
        const vector<UINT32>& uCodeBinary,
        vector<UINT32> *pImemBinary,
        vector<UINT32> *pDmemBinary,
        UCodeInfo *pUcodeInfo
    );
    static RC ParseBinaryGfwV2
    (
        const vector<UINT32>& uCodeBinary,
        const UCodeHeaderInfo& header,
        vector<UINT32> *pImemBinary,
        vector<UINT32> *pDmemBinary,
        UCodeInfo *pUcodeInfo
    );
    static RC ParseBinaryGfwV3
    (
        const vector<UINT32>& uCodeBinary,
        const UCodeHeaderInfo& header,
        vector<UINT32> *pImemBinary,
        vector<UINT32> *pDmemBinary,
        UCodeInfo *pUcodeInfo
    );
    static RC GetApplicationTableEntries
    (
        const UCodeInfo &ucodeInfo,
        vector<UINT32> *pDmemBinary,
        vector<AppTableEntry> *pEntries
    );
};
