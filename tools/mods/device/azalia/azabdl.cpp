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

#include "azabdl.h"
#include "core/include/platform.h"
#include "device/include/memtrk.h"
#include "azautil.h"
#include "cheetah/include/tegradrf.h"
#include "azactrl.h"
#include <cstdio>

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "AzaBDL"

AzaBDL::AzaBDL(UINT32 AddrBits,
               Memory::Attrib MemoryType,
               bool AllowIocs,
               UINT32 IocSpace,
               Controller *pCtrl,
               bool IsUseMappedPhysAddr)
{
    m_EntrySize = BDLE_SIZE;
    m_PhysAlign = BDL_ALIGNMENT;
    m_NumEntries = MAX_BDLES;
    m_MemAttr = MemoryType;
    m_AddrBits = AddrBits;
    m_BufferLength = 0;
    m_NextEntry = 0;
    m_AllowIocs = AllowIocs;
    m_IocSpace = IocSpace;
    m_NumIOCsSet = 0;
    m_pCtrl = pCtrl;
    m_IsUseMappedPhysAddr = IsUseMappedPhysAddr;
}

AzaBDL::~AzaBDL()
{
    // Nothing to do here : All cleaned up in base class.
}

RC AzaBDL::ResetTable()
{
    RC rc = OK;
    m_BufferLength = 0;
    m_NumIOCsSet = 0;
    CHECK_RC(GDTable::ResetTable());
    return rc;
}

RC AzaBDL::CopyTable(bool IsUseMappedPhysAddr, AzaBDL** pNewBdl)
{
    RC rc = OK;
    unique_ptr<AzaBDL> pBdl = make_unique<AzaBDL>(m_AddrBits, m_MemAttr, m_AllowIocs, m_IocSpace, m_pCtrl, IsUseMappedPhysAddr);
    CHECK_RC(pBdl->Init(MAX_BDLES, 0, m_pCtrl));
    if (pNewBdl)
    {
        *pNewBdl = pBdl.release();
    }
    return rc;
}

RC AzaBDL::SetEntry(UINT32 Index, void* pAddress, UINT32 Length, bool IOC)
{
    RC rc = OK;

    if (Index >= m_NumEntries)
    {
        Printf(Tee::PriError, "DTable Invalid Index\n");
        return RC::ILWALID_INPUT;
    }

    BDLE* pEntry = ((BDLE*)m_BaseVirt) + Index;

    // Set the BDLE fields
    if (m_IsUseMappedPhysAddr)
    {
        PHYSADDR physAddr = 0;
        CHECK_RC(MemoryTracker::GetMappedPhysicalAddress(pAddress, m_pCtrl, &physAddr));
        MEM_WR64(&(pEntry->address), physAddr);
    }
    else
    {
        MEM_WR64(&(pEntry->address), Platform::VirtualToPhysical(pAddress));
    }
    MEM_WR32(&(pEntry->length), Length);

    if (IOC)
        CHECK_RC(SetIOC(Index));

    Printf(Tee::PriLow, " SetEntry(Index(0x%02x), Address=" PRINT_PHYS ", Length=0x%08x, IOC=%0d\n",
           Index,
           MEM_RD64(&(pEntry->address)),
           MEM_RD32(&(pEntry->length)),
           RF_VAL(BDLE_IOC, MEM_RD32(&(pEntry->rsvd_ioc))));

    m_NextEntry++;
    m_BufferLength += Length;
    return rc;
}

UINT32 AzaBDL::GetNextIOCPos(UINT32 LwrrPos)
{
    UINT32 iocPos = 0;
    UINT32 index = 0;
    BDLE* pEntry = 0;

    for (index = 0; index<m_NumEntries; ++index)
    {
        GetEntryAddrVirt(index, (void**)(&pEntry));
        iocPos += MEM_RD32(&(pEntry->length));
        if (iocPos >= LwrrPos)
            break;
    }

    for (; index<m_NumEntries; ++index)
    {
        GetEntryAddrVirt(index, (void**)(&pEntry));
        if( RF_VAL(BDLE_IOC, MEM_RD32(&(pEntry->rsvd_ioc))) == 1)
            return iocPos;

        iocPos += MEM_RD32(&(pEntry->length));
    }

    return NO_IOC;
}

RC AzaBDL::SetIOC(UINT32 Index)
{
    if (Index >= m_NumEntries)
    {
        Printf(Tee::PriError, "DTable Invalid Index\n");
        return RC::ILWALID_INPUT;
    }

    BDLE* pEntry = ((BDLE*)m_BaseVirt) + Index;

    // Set the IOC Bit
    MEM_WR32(&(pEntry->rsvd_ioc), RF_SET(BDLE_IOC));
    m_NumIOCsSet++;

    return OK;
}

RC AzaBDL::FillTableEntries(MemoryFragment::FRAGLIST *pFragList)
{
    RC rc = OK;

    MASSERT(pFragList);

    for (UINT32 i = 0; i < pFragList->size(); ++i)
    {
        MemoryFragment::FRAGMENT &frag = (*pFragList)[i];

        if (m_AllowIocs)
        {
            // IOCs are set 1 in ever IocSpace BDLEs
            if(i%m_IocSpace)
            {
                CHECK_RC(SetEntry(i, frag.Address, frag.ByteSize, false));
            }
            else
            {
                CHECK_RC(SetEntry(i, frag.Address, frag.ByteSize, true));
            }
        }
        else
        {
            CHECK_RC(SetEntry(i, frag.Address, frag.ByteSize, false));
        }
    }

    return rc;
}

RC AzaBDL::Print()
{
    RC rc = OK;

    Printf (Tee::PriNormal, "  Print Table Entries:\n");
    Printf (Tee::PriNormal,
            "  m_TableBase  = %16p (Phys =" PRINT_PHYS "), m_NumEntries = 0x%08x\n",
            m_BaseVirt, Platform::VirtualToPhysical(m_BaseVirt), m_NumEntries);

    Printf (Tee::PriNormal,
            "     Index    tBase   pBuffer(Vir) pBuffer(Phys)   Length \n");

    for (UINT32 i = 0; i < m_NumEntries; i++)
        CHECK_RC(PrintEntry(i));

    Printf(Tee::PriNormal, "\n");

    return rc;
}

RC AzaBDL::PrintEntry(UINT32 Index)
{
    RC rc = OK;

    if (Index >= m_NumEntries)
    {
        Printf(Tee::PriError, "DTable Invalid Index\n");
        return RC::ILWALID_INPUT;
    }

    BDLE* tableEntry;
    CHECK_RC(GetEntryAddrVirt(Index, (void**)(&tableEntry)));

    PHYSADDR Address;
    UINT32   Length;
    bool     IOC;

    Address = MEM_RD64(&(tableEntry->address));
    Length = MEM_RD32(&(tableEntry->length));
    IOC     = RF_VAL(BDLE_IOC, MEM_RD32(&(tableEntry->rsvd_ioc)));

    Printf (Tee::PriNormal,
            "    %04d  0x%08x %16p   " PRINT_PHYS "    0x%08x   %s\n",
            Index,
            Platform::VirtualToPhysical32(tableEntry),
            Platform::PhysicalToVirtual(Address),
            Address, Length, IOC?"IOC":"");

    return rc;
}

RC AzaBDL::PrintBuffer(UINT32 Index)
{
    RC rc = OK;

    BDLE* tableEntry;
    UINT32 Address;
    UINT32 Length;
    if (Index == 0xffffffff)
    {
        for (UINT32 i = 0; i < m_NumEntries; ++i)
        {
            CHECK_RC(GetEntryAddrVirt(i, (void**)(&tableEntry)));
            Address = MEM_RD32(&(tableEntry->address));
            Length = MEM_RD32(&(tableEntry->length));

            Printf(Tee::PriNormal, "Table Entry 0x%04x Buffer\n", i);
            if (Address)
            {
                Memory::Print08(Platform::PhysicalToVirtual(Address), Length);
            }
        }
    }
    else
    {
        if (Index >= m_NumEntries)
        {
            Printf(Tee::PriError, "DTable Invalid Index\n");
            return RC::ILWALID_INPUT;
        }

        CHECK_RC(GetEntryAddrVirt(Index, (void**)(&tableEntry)));
        Address = MEM_RD32(&(tableEntry->address));
        Length  = MEM_RD32(&(tableEntry->length));

        Printf(Tee::PriNormal, "Table Entry 0x%04x Buffer\n", Index);
        if (Address)
        {
            Memory::Print32(Platform::PhysicalToVirtual(Address), Length);
        }
    }

    return rc;
}

RC AzaBDL::WriteBufferWithZeros()
{
    RC rc = OK;

    vector<UINT08> pattern;
    pattern.push_back(0);
    CHECK_RC(WriteToBuffer(&pattern));

    return OK;
}

UINT32 AzaBDL::GetLastSampleIndex(UINT32 StartSampleIndex,
                                  UINT32 nSamplesInArray,
                                  UINT32 BytesPerSample)
{

    UINT32 lastIndexToDo = StartSampleIndex + nSamplesInArray - 1;
    UINT32 lastIndexOverall = GetBufferLengthInBytes()/BytesPerSample - 1;

    if (lastIndexOverall < lastIndexToDo)
    {
        lastIndexToDo = lastIndexOverall;
    }

    return lastIndexToDo;
}

RC AzaBDL::WriteSample
(
    UINT32  BytesPerSample,
    UINT08* Address,
    UINT32  ByteOffset,
    UINT32  SampleIndex,
    void*   Samples
)
{
    // Write one sample
    switch (BytesPerSample)
    {
        case 1:
               MEM_WR08(Address + ByteOffset,
                        ((UINT08*)Samples)[SampleIndex]);
               break;
        case 2:
               MEM_WR16(Address + ByteOffset,
                        ((UINT16*) Samples)[SampleIndex]);
               break;
        case 4:
               MEM_WR32(Address + ByteOffset,
                        ((UINT32*) Samples)[SampleIndex]);
               break;
        default:
            // not reached
            Printf(Tee::PriError, "Invalid stream size in bytes!\n");
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC AzaBDL::ReadSample
(
    UINT32  BytesPerSample,
    UINT08* Address,
    UINT32  ByteOffset,
    UINT32  SampleIndex,
    void*   Samples
)
{
    // Write one sample
    switch (BytesPerSample)
    {
        case 1:
             static_cast<UINT08*>(Samples)[SampleIndex] = MEM_RD08(Address + ByteOffset);
             break;
        case 2:
             static_cast<UINT16*>(Samples)[SampleIndex] = MEM_RD16(Address + ByteOffset);
             break;
        case 4:
             static_cast<UINT32*>(Samples)[SampleIndex] = MEM_RD32(Address + ByteOffset);
             break;
        default:
            // not reached
            Printf(Tee::PriError, "Invalid stream size in bytes!\n");
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC AzaBDL::GetStartAddress
(
    UINT32  StartSampleIndex,
    UINT32  BytesPerSample,
    BDLE**  BDLEEntry,
    UINT32* ByteOffset
)
{
    RC rc = OK;
    // Locate Start Buffer Pointer
    UINT32 iteratedSamples = 0;
    UINT32 index = 0;
    BDLE* pEntry = 0;

    MASSERT(BDLEEntry);
    MASSERT(ByteOffset);
    MASSERT(m_BaseVirt);

    CHECK_RC(GetEntryAddrVirt(0, (void**)(&pEntry)));

    while((iteratedSamples < StartSampleIndex) && (index < m_NumEntries))
    {
        CHECK_RC(GetEntryAddrVirt(index, (void**)(&pEntry)));
        iteratedSamples += (MEM_RD32(&(pEntry->length)) / BytesPerSample);
        ++index;
    }

    *BDLEEntry = pEntry;

    if (iteratedSamples == 0)
    {
        *ByteOffset = 0;
    }
    else
    {
        UINT32 startSample = (MEM_RD32(&(pEntry->length))/BytesPerSample);
        *ByteOffset = (StartSampleIndex -
                      (iteratedSamples - startSample)) *
                       BytesPerSample;
    }

    return rc;
}

RC AzaBDL::ReadWriteSamples
(
    bool   IsWrite,
    UINT32 StartSampleIndex,
    void*  Samples,
    UINT32 nSamplesInArray,
    UINT32* nSamplesWritten,
    UINT32 BytesPerSample
)
{
    BDLE* pEntry = NULL;
    UINT32 byteOffset = 0;
    UINT32 tableIndex = 0;

    if (m_NumEntries == 0)
    {
        Printf(Tee::PriError, "Buffer not allocated\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    if (StartSampleIndex*BytesPerSample > GetBufferLengthInBytes())
    {
        Printf(Tee::PriError, "Invalid Sample Index\n");
        return RC::SOFTWARE_ERROR;
    }

    GetStartAddress(StartSampleIndex,
                    BytesPerSample,
                    &pEntry,
                    &byteOffset);

    UINT32 LastSampleIndex = GetLastSampleIndex(StartSampleIndex,
                                                nSamplesInArray,
                                                BytesPerSample);

    UINT08* addr = static_cast<UINT08*>(Platform::PhysicalToVirtual(MEM_RD64(&(pEntry->address))));

    for (UINT32 index = StartSampleIndex;
         index <= LastSampleIndex;
         ++index)
    {
        if (byteOffset >= MEM_RD32(&(pEntry->length)))
        {
            pEntry++;
            GetIndex(static_cast<void*>(pEntry),&tableIndex);
            addr = static_cast<UINT08*>(Platform::PhysicalToVirtual(MEM_RD64(&(pEntry->address))));
            byteOffset = 0;
        }

        RC rc = OK;

        if (IsWrite)
        {

            CHECK_RC(WriteSample(BytesPerSample,
                                 addr,
                                 byteOffset,
                                 index - StartSampleIndex,
                                 Samples));
        }
        else
        {
            CHECK_RC(ReadSample(BytesPerSample,
                                addr,
                                byteOffset,
                                index - StartSampleIndex,
                                Samples));
        }

        byteOffset += BytesPerSample;
    }
    *nSamplesWritten = LastSampleIndex - StartSampleIndex + 1;

    return OK;
}

RC AzaBDL::WriteSampleList(UINT32 StartSampleIndex,
                           void *Samples,
                           UINT32 nSamplesInArray,
                           UINT32* nSamplesWritten,
                           UINT32 BytesPerSample)
{
    RC rc = OK;
    *nSamplesWritten = 0;

    CHECK_RC(ReadWriteSamples(true,
                              StartSampleIndex,
                              Samples,
                              nSamplesInArray,
                              nSamplesWritten,
                              BytesPerSample));
    return OK;
}

RC AzaBDL::ReadSampleList(UINT32 StartSampleIndex,
                          void *Samples,
                          UINT32 nSamplesInArray,
                          UINT32* nSamplesRead,
                          UINT32 BytesPerSample)
{
    RC rc = OK;

    *nSamplesRead = 0;
    CHECK_RC(ReadWriteSamples(false,
                              StartSampleIndex,
                              Samples,
                              nSamplesInArray,
                              nSamplesRead,
                              BytesPerSample));
    return rc;
}

RC AzaBDL::WriteBufferWithPattern
(
    void* Samples,
    UINT32 nSamplesInArray,
    bool IsRepeat,
    UINT32 BytesPerSample
)
{
    RC rc = OK;
    UINT32 bufferLength = GetBufferLengthInBytes() / BytesPerSample;
    UINT32 nSamplesWrittenTotal = 0;
    UINT32 nSamplesWritten = 0;

    while ((IsRepeat || !nSamplesWrittenTotal) &&
           (nSamplesWrittenTotal != bufferLength))
    {
        CHECK_RC(WriteSampleList(nSamplesWrittenTotal,
                                 Samples,
                                 nSamplesInArray,
                                 &nSamplesWritten,
                                 BytesPerSample));
        nSamplesWrittenTotal += nSamplesWritten;
    }

    return rc;
}

RC AzaBDL::WriteBufferWithRandomData(UINT32 BytesPerSample)
{
    RC rc = OK;
    const UINT32 chunksize = 64; // Actual value is not important--it just
                                 // determines the tradeoff between calling
                                 // WriteBuffer more often vs. using extra
                                 // memory.

    UINT32 randomBuffer[chunksize];
    UINT32 bufferLength = GetBufferLengthInBytes()/BytesPerSample;
    UINT32 samplesInRandomBuffer = chunksize / BytesPerSample;
    UINT32 nSamplesWrittenTotal = 0;
    UINT32 nSamplesWritten = 0;

    while (nSamplesWrittenTotal != bufferLength)
    {
        for (UINT32 i=0; i<chunksize; ++i)
        {
            randomBuffer[i] = AzaliaUtilities::GetRandom(0, 0xffffffff);
        }
        CHECK_RC(WriteSampleList(nSamplesWrittenTotal,
                                 &randomBuffer,
                                 samplesInRandomBuffer,
                                 &nSamplesWritten,
                                 BytesPerSample));
        nSamplesWrittenTotal += nSamplesWritten;
    }

    return OK;
}

RC AzaBDL::ReadBufferToFile(const char* FileName, UINT32 BytesPerSample)
{
    RC rc = OK;

    UINT32 nSamples = GetBufferLengthInBytes()/BytesPerSample;
    Printf(Tee::PriNormal, "Writing %d sample buffer to PCM file %s\n",
                            nSamples,
                            FileName);
    UINT08* tempBuffer08 = NULL;

    if (nSamples != 0)
    {

        UINT32 tempBufferSizeInSamples = 0x100;
        if (OK != MemoryTracker::AllocBuffer(tempBufferSizeInSamples * BytesPerSample,
                                             (void **)&tempBuffer08,
                                             true,
                                             32,
                                             Memory::WB,
                                             m_pCtrl))
        {
            Printf(Tee::PriError, "Could not allocate memory for reading file!\n");
            rc = RC::CANNOT_ALLOCATE_MEMORY;
            goto Cleanup;
        }

        vector<UINT08> Buffer;

        for (UINT32 nSamplesWritten = 0; nSamplesWritten < nSamples; )
        {
            UINT32 nSamplesRead = 0;
            CHECK_RC_CLEANUP(ReadSampleList(nSamplesWritten,
                                            tempBuffer08,
                                            tempBufferSizeInSamples,
                                            &nSamplesRead,
                                            BytesPerSample));

            for (UINT32 i=0; i<(nSamplesRead*BytesPerSample); ++i)
            {
                Buffer.push_back(tempBuffer08[i]);
            }

            nSamplesWritten += nSamplesRead;
        }

        CHECK_RC(WriteBufferToFile(FileName, Buffer, BytesPerSample));
    }

 Cleanup:
    if (tempBuffer08)
    {
        MemoryTracker::FreeBuffer(tempBuffer08);
    }

    return rc;
}

UINT32 AzaBDL::GetBufferLengthInBytes()
{
    return m_BufferLength;
}

UINT32 AzaBDL::GetLastBdleIndex()
{
    return m_NextEntry-1;
}

UINT32 AzaBDL::GetNumIOCsSet()
{
    return m_NumIOCsSet;
}

UINT16 AzaBDL::PackWord(const vector<UINT08>& Data,
                        const UINT32 Offset)
{
    return (Data[Offset] | Data[Offset + 1] << 8);
}

UINT32 AzaBDL::PackDWord(const vector<UINT08>& Data,
                         const UINT32 Offset)
{
    return (Data[Offset] |
            Data[Offset + 1] << 8  |
            Data[Offset + 2] << 16 |
            Data[Offset + 3] << 24) ;
}

RC AzaBDL::WriteBufferToFile(const char* FileName,
                             vector<UINT08>& Buffer,
                             UINT32 BytesPerSample)
{
    RC rc = OK;
    UINT32 byte = 0;

    MASSERT(FileName);

    FILE* fp = fopen(FileName, "w");
    if (!fp)
    {
        Printf(Tee::PriError, "Could not open file: %s\n", FileName);
        return RC::CANNOT_OPEN_FILE;
    }

    while (byte <Buffer.size())
    {
        switch (BytesPerSample)
        {
            case 1:
                fprintf(fp, "%02x\n", Buffer[byte]);
                break;
            case 2:
                fprintf(fp, "%04x\n", PackWord(Buffer, byte));
                break;
            case 4:
                fprintf(fp, "%08x\n", PackDWord(Buffer, byte));
                break;
            default:
                Printf(Tee::PriError, "Invalid size\n");
                rc = RC::SOFTWARE_ERROR;
                goto Cleanup;
        }
        byte += BytesPerSample;
    }

    Cleanup:
        fclose(fp);
        return rc;
}
