/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_AZABDL_H
#define INCLUDED_AZABDL_H

#ifndef INCLUDED_GDTABLE_H
    #include "device/include/gdtable.h"
#endif
#ifndef INCLUDED_AZAFMT_H
    #include "azafmt.h"
#endif

#define MIN_BDLE_SEGMENT_SIZE 1
// BDL must be 128-byte aligned
#define BDL_ALIGNMENT 0x80
// Azalia spec requires that all BDLs have at least two entries
#define MIN_BDLES 2
// Azalia spec claims no explicit limit on the number of entries in a BDL,
// but there is an implied limit of 256 because the LastValidIndex register
// has only 8 bits.
#define MAX_BDLES   256
#define BDLE_SIZE   16
#define BDLE_IOC    0:0

// Returns 0xffffffff if no iocs set, or buffer is not allocated.
// Wraps around end of buffer to find next IOC, if necessary
static const UINT32 NO_IOC = 0xffffffff;

typedef struct _bdle
{
    PHYSADDR address;
    UINT32   length;
    UINT32   rsvd_ioc;
} BDLE;

class AzaBDL : public GDTable
{
public:

    AzaBDL(UINT32 AddrBits, Memory::Attrib MemoryType, bool AllowIocs, UINT32 IocSpace, Controller *pCtrl = nullptr, bool IsUseMappedPhysAddr = false);
    virtual ~AzaBDL();

    virtual RC   Print();
    virtual RC   FillTableEntries(MemoryFragment::FRAGLIST *pFragList);
    virtual bool IsEntryFree(void* Entry) {return true;}

    virtual RC PrintEntry(UINT32 Index);
    virtual RC PrintBuffer(UINT32 Index = 0xffffffff);

    RC SetEntry(UINT32 Index, void* Address, UINT32 Length, bool IOC = false);
    RC SetIOC(UINT32 Index);
    UINT32 GetNextIOCPos(UINT32 LwrrPos);

    virtual UINT32 GetBufferLengthInBytes();

    RC WriteSampleList(UINT32  StartSampleIndex,
                       void*   Samples,
                       UINT32  nSamplesInArray,
                       UINT32* nSamplesWritten,
                       UINT32  BytesPerSample);

    // Fills the buffer with the contents of the specified file. The file proper
    // file format is an ascii text file hold hex values of the samples, matching
    // the buffer's format. If the file contents are larger than the buffer,
    // then the file contents will be truncated. If the file contents are
    // smaller than the buffer, the file will be repeated (truncating the
    // last repetition if necessary) or the remainder of the buffer will be
    // filled with zeros, based on the value of the repeat parameter.

    RC WriteBufferWithPattern(void*  Samples,
                              UINT32 nSamplesInArray,
                              bool   IsRepeat,
                              UINT32 BytesPerSample);

    RC WriteBufferWithRandomData(UINT32 BytesPerSample);

    virtual RC ReadSampleList(UINT32  StartSampleIndex,
                              void*   Samples,
                              UINT32  nSamplesInArray,
                              UINT32* nSamplesRead,
                              UINT32  BytesPerSample);

    RC ReadWriteSamples(bool    IsWrite,
                        UINT32  StartSampleIndex,
                        void*   Samples,
                        UINT32  nSamplesInArray,
                        UINT32* nSamplesWritten,
                        UINT32  BytesPerSample);

    RC WriteSample(UINT32  BytesPerSample,
                   UINT08* Address,
                   UINT32  ByteOffset,
                   UINT32  SampleIndex,
                   void*   Samples);

    RC ReadSample (UINT32  BytesPerSample,
                   UINT08* Address,
                   UINT32  ByteOffset,
                   UINT32  SampleIndex,
                   void*   Samples);

    RC ReadBufferToFile(const char* filename,
                        UINT32 BytesPerSample);

    RC WriteBufferWithZeros();

    UINT32 GetLastBdleIndex();

    RC     GetStartAddress(UINT32  StartSampleIndex,
                           UINT32  BytesPerSample,
                           BDLE**  BDLEEntry,
                           UINT32* ByteOffset);

    UINT32 GetLastSampleIndex(UINT32 StartSampleIndex,
                              UINT32 nSamplesInArray,
                              UINT32 BytesPerSample);
    UINT32 GetNumIOCsSet();

    RC ResetTable();

    RC CopyTable(bool IsUseMappedPhysAddr, AzaBDL** pNewBdl);

private:

    UINT16 PackWord(const vector<UINT08>& Data, const UINT32  Offset);

    UINT32 PackDWord(const vector<UINT08>& Data, const UINT32  Offset);

    RC WriteBufferToFile(const char* FileName,
                         vector<UINT08>& Buffer,
                         UINT32 BytesPerSample);

    UINT32 m_BufferLength;
    UINT32 m_IocSpace;
    bool   m_AllowIocs;
    UINT32 m_NumIOCsSet;
    Controller *m_pCtrl;
    bool m_IsUseMappedPhysAddr;
};

#endif
