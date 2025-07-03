/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2013,2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_AZACTRL_H
#define INCLUDED_AZACTRL_H

#include "cheetah/include/controller.h"
#include "azadmae.h"
#include "azastrm.h"
#include "core/include/tasker.h"

#include <list>
#include <vector>

#define MAX_NUM_SDINS 16
// The azalia spec requires support for 15 input and 15 output streams, but
// our implementation has only 8 dma engines. In most cases, the relevant
// metric is the number of engines. The number of streams is only used in
// rare instances.
#define MAX_NUM_STREAMS_RARELY_USED 30
#define MAX_NUM_ENGINES 10

#define AZA_DEFAULT_MSILINES 1

class AzaliaCodec;

class AzaliaController : public Controller
{
public:

    enum AzaIntType
    {
        INT_CORB_MEM_ERR,
        INT_RIRB_OVERRUN,
        INT_RIRB,
        INT_WAKE,
        INT_STREAM_DESC_ERR,
        INT_STREAM_FIFO_ERR,
        INT_STREAM_IOC,
        INT_CONTROLLER,
        INT_STREAM,
        INT_GLOBAL,
        INT_PCI,

        //INT_ALL
    };

    class IntControllerStatus
    {
    public:
        bool corbMemErr;
        bool rirbOverrun;
        bool rirbIntFlag;
        UINT32 statests;
        void Clear();
        void Merge(const IntControllerStatus ToMerge);
    };

    class IntStreamStatus
    {
    public:
        bool dese;
        bool fifoe;
        bool bcis;
        void Clear();
        void Merge(const IntStreamStatus ToMerge);
    };

    enum PowerState
    {
        D0 = 0,
        D3HOT = 3
    };

    RC SetPowerState(PowerState Target);

    AzaliaController();
    virtual ~AzaliaController();
    const char* GetName();

    // ---------------------------------------------
    // Functions inherited from the Controller Class
    RC PrintReg(Tee::Priority Pri = Tee::PriNormal) const;
    RC PrintState(Tee::Priority Pri = Tee::PriNormal) const;
    RC PrintExtDevInfo(Tee::Priority Pri = Tee::PriNormal) const;

    RC ToggleControllerInterrupt(bool IsEnable);
    RC GetIsr(ISR* Func) const;

    RC InitBar();
    RC InitExtDev();
    RC UninitExtDev() { return OK; }

    RC Reset() {return OK;}

    RC RegRd(const UINT32 Offset,
             UINT32 *pData,
             const bool IsDebug=false) const;

    RC RegWr(const UINT32 Offset,
             const UINT32 Data,
             const bool IsDebug=false) const;

    // ---------------------------------------------

    long Isr();

    RC GetInterruptMode(IntrModes *pMode) const;
    RC Reset(bool PutIntoReset);
    RC IsInReset(bool* IsInReset);
    RC ToggleRandomization(bool IsEnable);
    RC SendICommand(UINT32 CodecNum, UINT32 NodeId, UINT32 Verb, UINT32 Payload,
                    UINT32 *pResp);

    // ---------------------------------------------
    // CORB control
    RC CorbInitialize(UINT16 Size, bool IsMemErrIntEnable = false);
    RC CorbIsRunning(bool* pIsRunning);
    RC CorbToggleRunning(bool IsRunning);
    RC CorbPush(UINT32 Verbs[], UINT16 lwerbs, UINT16* nPushed);
    RC CorbGetFreeSpace(UINT16 *pSpace);
    RC CorbGetTotalSize(UINT16 *pSize);
    RC CorbReset();
    RC CorbToggleCoherence(bool IsEnable);
    RC PrintCorbState(Tee::Priority Pri) const;
    RC DumpCorb(Tee::Priority, UINT16 FirstEntry = 0, UINT16 LastEntry = 255);
    // ---------------------------------------------

    // ---------------------------------------------
    // RIRB Control
    RC RirbInitialize(UINT16 Size, UINT16 RespIntCnt = 0,
                      bool RespIntEnable = false,
                      bool RespOverrunIntEnable = false);
    RC RirbIsRunning(bool* pIsRunning);
    RC RirbToggleRunning(bool Running);
    RC RirbPop(UINT64 Responses[], UINT16 nToPop, UINT16 *pNumPopped);
    RC RirbGetNResponses(UINT16 *pNumResponses);
    RC RirbGetTotalSize(UINT16 *pSize);
    RC RirbGetIntCnt(UINT16 *pIntCnt);
    RC RirbReset();
    RC RirbToggleCoherence(bool IsEnable);
    RC PrintRirbState(Tee::Priority Pri) const;
    RC DumpRirb(Tee::Priority, UINT16 FirstEntry = 0, UINT16 LastEntry = 255);
    RC RirbProcess();
    RC RirbGetNSolicitedResponses(UINT08 CodecAddr, UINT16 *nResponses);
    RC RirbGetSolicitedResponse(UINT08 CodecAddr, UINT32 *response);
    RC RirbGetNUnsolicitedResponses(UINT08 CodecAddr, UINT16 *nResponses);
    RC RirbGetUnsolictedResponse(UINT08 CodecAddr, UINT32 *pResponse);
    RC ToggleUnsolResponses(bool IsEnable);
    // ---------------------------------------------

    // ---------------------------------------------
    // Codec control
    RC GetCodec(UINT08 sdin, AzaliaCodec **ppCodec);
    RC GetNumCodecs(UINT08 *pNumCodecs) const;

    RC GetMaxNumInputStreams(UINT32 *pNumInputStreams) const;
    RC GetMaxNumOutputStreams(UINT32 *pNumOutputStreams) const;
    RC GetMaxNumStreams(UINT32 *pNumStreams) const;
    bool IsFormatSupported(UINT32 Dir, UINT32 Rate, UINT32 Size, UINT32 Sdin);

    RC CodecLoad(string &FileName, bool IsOnlyPrint);
    RC SendICommand(UINT32 Command, UINT32 *pResponse);
    RC ReadIResponse(UINT32 *pResponse);
    // ---------------------------------------------

    // ---------------------------------------------
    // Stream control
    RC GenerateStreamNumber(AzaliaDmaEngine::DIR Dir, UINT08* pStreamNumber,
                            UINT08 DesiredStreamNumber = RANDOM_STRM_NUMBER);

    RC GetStream(UINT16 stream, AzaStream **pStream);
    RC GetNextAvailableStream(AzaliaDmaEngine::DIR Dir, UINT32 *pStreamIndex) const;
    RC DisableStream(UINT32 Index);
    RC EnableStream(UINT32 Index);
    RC ReleaseAllStreams();

    RC StreamToggleInputCoherence(bool IsEnable);
    RC StreamToggleOutputCoherence(bool IsEnable);
    // ---------------------------------------------

    // ---------------------------------------------
    // Interrupt control
    RC IntToggle(AzaIntType Type, bool IsEnable, UINT32 Mask = 0xffffffff);
    RC IntQuery(AzaIntType Type, UINT32 Index, bool* pResult);
    RC IntClear(AzaIntType Type, UINT32 Index);
    RC IntControllerUpdateStatus();
    RC IntControllerGetStatus(IntControllerStatus *pStatus);
    RC IntControllerClearStatus();
    RC IntStreamUpdateStatus(UINT32 StreamNumber);
    RC IntStreamGetStatus(UINT32 StreamNumber, IntStreamStatus *pStatus);
    RC IntStreamClearStatus(UINT32 StreamNumber);
    // ---------------------------------------------

    // ---------------------------------------------
    // DMA Control
    RC DmaPosInitialize();
    RC DmaPosToggle(bool IsEnable);
    RC DmaPosGet(UINT08 StreamNumber, UINT32 *pPos);
    RC DmaPosClear(UINT08 StreamNumber);
    RC DmaPosToggleCoherence(bool IsEnable);
    // ---------------------------------------------

    // Additional coherence control
    RC ToggleBdlCoherence(bool IsEnable);

    // Wall Clock Access
    RC GetWallClockCount(UINT32 *pCount);
    RC GetWallClockCountAlias(UINT32 *pCount);

    // PCI access
    virtual RC GetAspmState(UINT32 *aspm);
    virtual RC SetAspmState(UINT32 aspm);

    // Intel and ECR Mode Support
    bool IsIch6();
    RC SetEcrMode(bool IsEcrMode);

    // GPU Azalia Support
    bool IsGpuAza();

    RC MapPins() const;
    virtual UINT08 GetDefaultMsiLines() {return AZA_DEFAULT_MSILINES;}

    UINT16 GetAerOffset() const;
    bool AerEnabled() const;

    Memory::Attrib GetMemoryType();
    UINT32 GetAddrBits() { return m_AddrBits; }

    RC SetIsAllocFromPcieNode(bool IsPcie);
    bool GetIsAllocFromPcieNode() { return m_IsAllocFromPcieNode; }
    RC SetIsUseMappedPhysAddr(bool IsMapped);
    bool GetIsUseMappedPhysAddr() { return m_IsUseMappedPhysAddr; }

    UINT32 m_IsrCnt[2];
    UINT32 m_rirbWp, m_rirbSts;

protected:

    // ---------------------------------------------
    // Functions inherited from the Controller Class
    RC InitRegisters();
    RC UninitRegisters();
    RC InitDataStructures();
    RC UninitDataStructures();
    // ---------------------------------------------

    RC IsSupport64bit(bool *pIsSupport64);
    virtual RC DeviceRegRd(UINT32 Offset, UINT32 *pData) const;
    virtual RC DeviceRegWr(UINT32 Offset, UINT32 Data) const;

    // PCI access
    virtual RC GetCapPtr(UINT32 *CapBaseOffset);

private:
    UINT08 AzaMemRd08(UINT08* Address) const;
    UINT16 AzaMemRd16(UINT08* Address) const;
    void AzaMemWr08(UINT08* Address, UINT08 Data) const;
    void AzaMemWr16(UINT08* Address, UINT16 Data) const;
    bool IsStreamNumberUnique(AzaliaDmaEngine::DIR Dir, UINT08 StreamIndex);

    bool m_IsRandomEnabled;
    bool m_IsIch6Mode;
    bool m_IsEcrMode;
    UINT08 m_NInputStreamsSupported;
    UINT08 m_NOutputStreamsSupported;
    UINT08 m_NBidirStreamsSupported;
    UINT08 m_NCodecs;
    vector<AzaliaDmaEngine*> m_vpStreamEngines;
    AzaliaCodec* m_pCodecTable[MAX_NUM_SDINS];
    list<UINT32> m_SolResponses[MAX_NUM_SDINS];
    list<UINT32> m_UnsolResponses[MAX_NUM_SDINS];
    UINT32* m_pCorbBaseAddr;
    UINT64* m_pRirbBaseAddr;
    void* m_pDmaPosBaseAddr;
    UINT32 m_CorbSize;
    UINT32 m_RirbSize;

    UINT16 m_RirbReadPtr;
    IntControllerStatus m_IntStatus;
    IntStreamStatus m_IntStreamStatus[MAX_NUM_ENGINES];
    vector<AzaStream*> m_vpStreams;

    vector<UINT32> m_vPowStateConfigRegisters;
    // Interrupt enables are cached here because we want to have quick
    // access to them that does not require a register read.
    struct {
        bool cmei;
        bool rirboic;
        bool rirbctl;
        UINT32 wakeen;
        UINT32 dei;
        UINT32 fei;
        UINT32 ioc;
        bool ci;
        UINT32 si;
        bool gi;
        bool pcii;
    } m_IntEnables;

    UINT16 m_VendorId;
    UINT16 m_DeviceId;
    UINT32 m_NumMsiLines;
    UINT32 m_AddrBits;
    UINT08* m_pBar0;
    bool m_IsAllocFromPcieNode;
    bool m_IsUseMappedPhysAddr;
};

#endif // INCLUDED_AZACTRL_H
