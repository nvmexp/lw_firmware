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

// Secure Dma Copy Test: Tests multple secure copy channels used by the confidential compute
// copy engines.
#include "core/include/rc.h"
#include "core/include/golden.h"
#include "gputestc.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/include/nfysema.h"
#include "gpu/include/rngpickr.h"
#include "core/include/jscript.h"
#include "gpu/include/gralloc.h"
#include "gpu/tests/pcie/pextest.h"
#include "gpu/tests/rm/utility/changrp.h"
#include <vector>
#include <map>
#include <set>

// Helper class to handle surface2D resources
class CeMemDesc
{
public:
    Surface2D   m_Surf2d;
    LwU32       m_Size          = 0;
    bool        m_AllocDone     = false;
    UINT64      m_GpuAddrSysMem = 0;
    UINT32 *    m_pCpuAddrSysMem = nullptr;

                CeMemDesc() {}
    virtual     ~CeMemDesc() {};
    RC          AllocateMemory(GpuDevice *gpudev, UINT32 size, Memory::Location location);
    RC          Compare(CeMemDesc *pDstMemDesc);
    void        Dump(const char *pStr);
    RC          Fill(UINT32 pat);
    void        InitializeSrc();
};

//! Copy and check, report HW pass/fail.
class SelwreCopy : public GpuTest
{
public:
    SelwreCopy() { };
    virtual ~SelwreCopy() { };

    virtual RC      Cleanup() override;
    virtual bool    IsSupported() override;
    virtual void    PrintJsProperties(Tee::Priority pri) override;
    virtual RC      Run() override;
    virtual RC      Setup() override;
private:
    enum
    {
        CHANNEL_GROUPS_PER_ENGINE = 2,
        CHANNELS_PER_GROUP = 2
    };
    typedef struct
    {
        ChannelGroup *pChGrp;
        Channel *     pCh[CHANNELS_PER_GROUP];
        LwRm::Handle  hObj[CHANNELS_PER_GROUP];
        CC_KMB        kmb[CHANNELS_PER_GROUP];
    } CeChanGroup;

    struct CeBuffers
    {
        LwU32     sizeInBytes;
        CeMemDesc dmaSrc;   // cypher data source
        CeMemDesc dmaDest;  // cypher data dest
        CeMemDesc authTag;  // auth tag to be generated
        CeMemDesc iv;       //12 byte initialization vector
        CeMemDesc sema;     // sync object for CE HW
        CeBuffers() { sizeInBytes = 0; }
    };

    typedef struct
    {
        UINT32 value;
        UINT32 * pAddr;
    } PollSemaPayloadArgs;

    CeChanGroup     m_CeChanGroup[CHANNEL_GROUPS_PER_ENGINE];
    LwRm::Handle    m_hObj = 0;
    ChannelGroup    *pChGrp = nullptr;

    RC AllocCeBuffers
    (
        CeBuffers * pCeBuf,
        LwU32 srcSizeBytes,
        Memory::Location srcLocation
    );

    void DumpSrc
    (
        CeMemDesc *pMemDesc,
        char *pStr
    );

    RC FreeChannels();

    void InitializeSrc
    (
        CeMemDesc *pMemDesc
    );

    RC PollSemaMethod
    (
        CeMemDesc *pMemDesc
    );

    static bool PollSemaPayload
    (
        void * Args
    );

    RC PushCeMethods
    (
        Channel *pCh, 
        LwRm::Handle hObj, 
        CeBuffers * pCeBufs, 
        LwU32 semPayload,
        bool bIsModeEncrypt, 
        bool bFlush_Wait
    );

    RC SetupChannels
    (
        UINT32 engineType
    );

    RC SwDecrypt
    (
        CC_KMB *pKmb,
        CeMemDesc *pInMemDesc,
        CeMemDesc *pAuthTag,
        CeMemDesc *pOutMemDesc
    );

    RC SwEncrypt
    (
        CC_KMB *pKmb,
        CeMemDesc *pInMemDesc,
        CeMemDesc *pAuthTag,
        CeMemDesc *pOutMemDesc
    );

    RC VerifyOutput
    (
        CeBuffers *pCeBuf,
        CC_KMB kmb,
        bool bCeEncrypt,
        CeMemDesc *pGenMemDesc
    );
};
