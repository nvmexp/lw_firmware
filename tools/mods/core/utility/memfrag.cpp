/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  utility/memory.cpp
 * @brief Access to memory.
 *
 *
 */

#include "core/include/memfrag.h"
#include "core/include/memoryif.h"
#include "core/include/tee.h"
#include "random.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "lwtypes.h"
#include <algorithm>
#include <vector>

namespace MemoryFragment
{
}

RC MemoryFragment::GenerateFixedFragment
      (
         void * Address,
         UINT32 AddrPhys,
         UINT32 ByteSize,
         FRAGLIST * pFragList,
         UINT32 DataWidth,
         vector<UINT32> FragList
      )
{
   UINT32 NumFrags;

   NumFrags = (UINT32)FragList.size();

   Printf(Tee::PriDebug, "MemoryFragment::GenerateFixedFragment(Addr %16p, Bytes 0x%x, Align 0x%x, NumFrags[0x%x])\n",
          Address, ByteSize, DataWidth, NumFrags);

   if (NumFrags <=0)
   {
      Printf(Tee::PriHigh, "*** ERROR, MemoryFragment::GenerateFixedFragment(), Invalid vector size %d ***\n",
                      NumFrags);
      return RC::ILWALID_INPUT;
   }
   if( (DataWidth!=BYTE_SIZE)
       &&(DataWidth!=WORD_SIZE)
       &&(DataWidth!=DWORD_SIZE)
       &&(DataWidth!=QWORD_SIZE) )
   {
      Printf(Tee::PriHigh, "*** ERROR, MemoryFragment::GenerateFixedFragment(), DataWidth = 0x%x ***\n",
                      DataWidth);
      return RC::ILWALID_INPUT;
   }

   if( ((LwUPtr)Address%DataWidth)
       ||(ByteSize%DataWidth))
   {
      Printf(Tee::PriHigh, "*** ERROR, MemoryFragment::GenerateFixedFragment(), Input not aligned, Addr %16p, Bytes 0x%x DataWidth %i***\n",
             Address, ByteSize, DataWidth);
      return RC::ILWALID_INPUT;
   }

   UINT32 totalBytes=0;
   for(UINT32 iIndex=0;iIndex<NumFrags;iIndex++)
   {
        totalBytes += FragList[iIndex];
   }

   if (totalBytes != ByteSize)
   {
      Printf(Tee::PriHigh, "*** ERROR, MemoryFragment::GenerateFixedFragment(), Input Total Bytes 0x%i ByteSize %i***\n",
                      totalBytes, ByteSize);
      return RC::ILWALID_INPUT;
   }

   void * LwrrentAddr = Address;
   UINT32 LwrrentPhys = AddrPhys;
   FRAGMENT Frag = { };

   for(UINT32 i=0; i < NumFrags; i++)
   {
         // Fixed size for each buffer
      if(FragList[i])
      {
            if(FragList[i]%DataWidth)
            {
                Printf(Tee::PriHigh, "*** ERROR, MemoryFragment::GenerateFixedFragment(), Input block %d=0x%i DataWidth %i***\n",
                        i, FragList[i], DataWidth);
                return RC::ILWALID_INPUT;
            }
            Frag.ByteSize = FragList[i];
      }
      Frag.Address = LwrrentAddr;
      Frag.PhysAddr = LwrrentPhys;

      pFragList->push_back(Frag);

      // Update size counter and buffer pointer
      LwrrentAddr = (void *)((char *)LwrrentAddr+Frag.ByteSize);
      if(LwrrentPhys)
          LwrrentPhys += Frag.ByteSize;
   }

   return OK;
}

RC MemoryFragment::FillPattern
      (
         FRAGLIST * pFragList,
         vector<UINT08> * pPattern08,
         bool IsContinous
      )
{
   MASSERT(pFragList);

   size_t Offset = 0;

   for(size_t i = 0; i<pFragList->size(); i++)
   {
      if( (i>0)&&IsContinous )
      {
         Offset += (*pFragList)[i-1].ByteSize;
         Offset = (Offset%pPattern08->size());
      }
      Memory::FillPattern((*pFragList)[i].Address, pPattern08,
                          (*pFragList)[i].ByteSize, (UINT32)Offset);
   }

   return OK;
}

RC MemoryFragment::FillPattern
      (
         FRAGLIST * pFragList,
         vector<UINT16> * pPattern16,
         bool IsContinous
      )
{
   MASSERT(pFragList);

   size_t Offset = 0;
   for(size_t i = 0; i<pFragList->size(); i++)
   {
      if( (i>0)&&IsContinous )
      {
         Offset += (*pFragList)[i-1].ByteSize/WORD_SIZE;
         Offset = (Offset%pPattern16->size());
      }
      Memory::FillPattern((*pFragList)[i].Address, pPattern16,
                          (*pFragList)[i].ByteSize, (UINT32)Offset);
   }
   return OK;
}

RC MemoryFragment::FillPattern
      (
         FRAGLIST * pFragList,
         vector<UINT32> * pPattern32,
         bool IsContinous
      )
{

   MASSERT(pFragList);

   size_t Offset = 0;
   for(size_t i = 0; i<pFragList->size(); i++)
   {
      if( (i>0)&&IsContinous )
      {
         Offset += (*pFragList)[i-1].ByteSize/DWORD_SIZE;
         Offset = (Offset%pPattern32->size());
      }
      Memory::FillPattern((*pFragList)[i].Address, pPattern32,
                          (*pFragList)[i].ByteSize, (UINT32)Offset);
   }
   return OK;
}

// @TODO This will re-seed the default random number generator object
// for each fragment, and therefore will not be true random.  The
// appropriate fix in later branches should be to pass a random object
// rather than a seed.
RC MemoryFragment::FillRandom
      (
          FRAGLIST * pFragList,
          UINT32 Seed
      )
{
   RC rc = OK;

   MASSERT(pFragList);
   for(size_t i = 0; i<pFragList->size(); i++)
   {
      CHECK_RC(Memory::FillRandom((*pFragList)[i].Address, Seed,
                                  (*pFragList)[i].ByteSize));
   }
   return rc;
}

RC MemoryFragment::DumpToVector(FRAGLIST * pFragList,
                                vector<UINT08> * pvData08)
{
    MASSERT(pFragList);

    RC rc;
    for(size_t i = 0; i<pFragList->size(); i++)
    {
       CHECK_RC( Memory::Dump((*pFragList)[i].Address, pvData08,
                              (*pFragList)[i].ByteSize) );
    }
    return OK;
}

RC MemoryFragment::DumpToVector(FRAGLIST * pFragList,
                                vector<UINT16> * pvData16)
{
   MASSERT(pFragList);

   RC rc;
   for(size_t i = 0; i<pFragList->size(); i++)
   {
      CHECK_RC( Memory::Dump((*pFragList)[i].Address, pvData16,
                             (*pFragList)[i].ByteSize) );
   }
   return OK;
}

RC MemoryFragment::DumpToVector(FRAGLIST * pFragList,
                                vector<UINT32> * pvData32)
{
   MASSERT(pFragList);

   RC rc;
   for(size_t i = 0; i<pFragList->size(); i++)
   {
      CHECK_RC( Memory::Dump((*pFragList)[i].Address, pvData32,
                             (*pFragList)[i].ByteSize) );
   }
   return OK;
}

RC MemoryFragment::ComparePattern
      (
         FRAGLIST * pFragList,
         vector<UINT08> * pPattern08,
         bool IsContinous
      )
{
   MASSERT(pFragList);

   RC rc;
   size_t Offset = 0;

   for(size_t i = 0; i<pFragList->size(); i++)
   {
      if( (i>0)&&IsContinous )
      {
         Offset += (*pFragList)[i-1].ByteSize;
         Offset = (Offset%pPattern08->size());
      }
      CHECK_RC( Memory::ComparePattern((*pFragList)[i].Address, pPattern08,
                                       (*pFragList)[i].ByteSize, (UINT32)Offset) );
   }
   return OK;
}

RC MemoryFragment::ComparePattern
      (
         FRAGLIST * pFragList,
         vector<UINT16> * pPattern16,
         bool IsContinous
      )
{
   MASSERT(pFragList);

   RC rc;
   size_t Offset = 0;

   for(size_t i = 0; i<pFragList->size(); i++)
   {
      if( (i>0)&&IsContinous )
      {
         Offset += (*pFragList)[i-1].ByteSize/WORD_SIZE;
         Offset = (Offset%pPattern16->size());
      }
      CHECK_RC( Memory::ComparePattern((*pFragList)[i].Address, pPattern16,
                                       (*pFragList)[i].ByteSize, (UINT32)Offset) );
   }
   return OK;
}

RC MemoryFragment::ComparePattern
      (
         FRAGLIST * pFragList,
         vector<UINT32> * pPattern32,
         bool IsContinous
      )
{
   MASSERT(pFragList);

   RC rc;
   size_t Offset = 0;

   for(size_t i = 0; i<pFragList->size(); i++)
   {
      if( (i>0)&&IsContinous )
      {
         Offset += (*pFragList)[i-1].ByteSize/DWORD_SIZE;
         Offset = (Offset%pPattern32->size());
      }
      CHECK_RC( Memory::ComparePattern((*pFragList)[i].Address, pPattern32,
                                       (*pFragList)[i].ByteSize, (uint32)Offset) );
   }
   return OK;
}

RC MemoryFragment::CompareVector
      (
         FRAGLIST * pFragList,
         vector<UINT08> * pvData08
      )
{
   MASSERT( pFragList );
   MASSERT( pvData08 );
   MASSERT( pvData08->size() );
   MASSERT( pFragList->size() );

   RC rc;
   size_t i;
   size_t Offset = 0;

   for(i=0; i<pFragList->size(); i++)
   {
      CHECK_RC( Memory::CompareVector((*pFragList)[i].Address, pvData08,
                                      (*pFragList)[i].ByteSize, false, Offset) );
      Offset += (*pFragList)[i].ByteSize;
   }

   if(Offset!=pvData08->size())
   {
      Printf(Tee::PriHigh, "*** ERROR: MemoryFragment::CompareVector Total 0x%x Bytes, Expect 0x%0x Bytes ***\n",
             (UINT32)Offset, (UINT32)(pvData08->size()));
      return RC::MEMFRAG_SIZE_MISMATCH;
   }
   return OK;
}

RC MemoryFragment::CompareVector
      (
         FRAGLIST * pFragList,
         vector<UINT16> * pvData16
      )
{
   MASSERT( pFragList );
   MASSERT( pvData16 );
   MASSERT( pvData16->size() );
   MASSERT( pFragList->size() );

   RC rc;
   size_t i;
   size_t Offset = 0;
   for(i=0; i<pFragList->size(); i++)
   {
      CHECK_RC( Memory::CompareVector((*pFragList)[i].Address, pvData16,
                                      (*pFragList)[i].ByteSize, false, Offset) );
      Offset += (*pFragList)[i].ByteSize/2;
   }

   if(Offset!=pvData16->size())
   {
      Printf(Tee::PriHigh, "*** ERROR: MemoryFragment::CompareVector Total 0x%x Words, Expect 0x%0x Words ***\n",
             (UINT32)Offset, (UINT32)(pvData16->size()));
      return RC::MEMFRAG_SIZE_MISMATCH;
   }

   return OK;
}

RC MemoryFragment::CompareVector
      (
         FRAGLIST * pFragList,
         vector<UINT32> * pvData32
      )
{
   MASSERT( pFragList );
   MASSERT( pvData32 );
   MASSERT( pvData32->size() );
   MASSERT( pFragList->size() );

   RC rc;
   size_t i;
   size_t Offset = 0;

   for(i=0; i<pFragList->size(); i++)
   {
      CHECK_RC( Memory::CompareVector((*pFragList)[i].Address, pvData32,
                                      (*pFragList)[i].ByteSize, false, Offset) );
      Offset += (*pFragList)[i].ByteSize/4;
   }

   if(Offset!=pvData32->size())
   {
      Printf(Tee::PriHigh, "*** ERROR: MemoryFragment::CompareVector Total 0x%x Words, Expect 0x%0x Words ***\n",
             (UINT32)Offset, (UINT32)(pvData32->size()));
      return RC::MEMFRAG_SIZE_MISMATCH;
   }
   return OK;
}

RC MemoryFragment::CompareFragList08
      (
         FRAGLIST * pFragList,
         FRAGLIST * pFragListExp
      )
{
   if((!pFragList)||(!pFragListExp))
   {
      Printf(Tee::PriHigh, "*** ERROR: MemoryFragment::CompareFragList08() Invalid List ***\n");
       return RC::ILWALID_INPUT;
   }

   //Error checking for give data
   size_t ChkListLen = pFragList->size();
   size_t ExpListLen = pFragListExp->size();
   size_t BytesCompared = 0;
   size_t ExpTotal = 0;
   size_t ChkTotal = 0;
   size_t iChk = 0;
   size_t jChk = 0;
   size_t iExp, jExp;

   if((!ExpListLen)||(!ChkListLen))
   {
      Printf(Tee::PriHigh, "*** ERROR: Invalid input ExpListLen 0x%x, ChkListLen = %d ***\n",
             (UINT32)ExpListLen, (UINT32)ChkListLen);
       return RC::ILWALID_INPUT;
   }

   volatile UINT08 * ExpAdd;
   UINT32  ExpByte;
   volatile UINT08 * Add;
   UINT08 Data;
   UINT08 ExpData;

   Add = (volatile UINT08*)(*pFragList)[0].Address;
   ChkTotal =  (*pFragList)[0].ByteSize;

   for(iExp = 0; iExp<ExpListLen; iExp++)
   {
      ExpByte = (*pFragListExp)[iExp].ByteSize;
      ExpAdd = (volatile UINT08*)(*pFragListExp)[iExp].Address;
      ExpTotal += ExpByte;

      for(jExp=0; jExp<ExpByte; jExp++, BytesCompared++, jChk++)
      {
         // Advance the buffer list
         if(jChk>=(*pFragList)[iChk].ByteSize)
         {
            iChk++;
            jChk=0;
            if(iChk>ChkListLen)
            {
               Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList08(), iChk=0x%x, total = 0x%x ***\n",
                      (UINT32)iChk, (UINT32)(pFragList->size())-1 );
               return RC::MEMFRAG_SIZE_MISMATCH;
            }
            Add = (volatile UINT08*)(*pFragList)[iChk].Address;
            ChkTotal +=  (*pFragList)[iChk].ByteSize;
         }
         ExpData = MEM_RD08(&ExpAdd[jExp]);
         Data = MEM_RD08(&Add[jChk]);
         if(Data!=ExpData)
         {
            Printf(Tee::PriDebug, "\tChecked Buffer\n");
            Print08(pFragList, Tee::PriDebug);
            Printf(Tee::PriDebug, "\tExpected Buffer\n");
            Print08(pFragListExp, Tee::PriDebug);

            Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList08() ***\n");
            Printf(Tee::PriHigh, "\tMismatch @ %d, Data [0x%x, 0x%x] = 0x%x, Expt [%x, %x] = 0x%x\n",
                   (UINT32)BytesCompared, (UINT32)iChk, (UINT32)jChk, Data,
                   (UINT32)iExp, (UINT32)jExp, ExpData);
            return RC::DATA_MISMATCH;
         }
      }
   }

   if(iChk!=pFragList->size()-1)
   {
      Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList08(), iChk=0x%x, total = 0x%x ***\n",
             (UINT32)iChk, (UINT32)(pFragList->size())-1 );
      // Not reached the last Fragment yet
      return RC::MEMFRAG_SIZE_MISMATCH;
   }
   if(ChkTotal!=ExpTotal)
   {
      Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList08(), ChkTotal=0x%x, Exptotal = 0x%x ***\n",
             (UINT32)ChkTotal, (UINT32)ExpTotal );
      // Total Number of Bytes not match
      return RC::MEMFRAG_SIZE_MISMATCH;
   }

   return OK;
}

RC MemoryFragment::CompareFragList16
      (
         FRAGLIST * pFragList,
         FRAGLIST * pFragListExp
      )
{
   if((!pFragList)||(!pFragListExp))
   {
      Printf(Tee::PriHigh, "*** ERROR: MemoryFragment::CompareFragList16() Invalid List ***\n");
       return RC::ILWALID_INPUT;
   }

   //Error checking for give data
   size_t ChkListLen = pFragList->size();
   size_t ExpListLen = pFragListExp->size();
   size_t WordCompared = 0;
   size_t ExpTotal = 0;
   size_t ChkTotal = 0;
   size_t iChk = 0;
   size_t jChk = 0;
   size_t iExp, jExp;

   volatile UINT16 * ExpAdd;
   UINT32  ExpWord;
   volatile UINT16 * Add;
   UINT16 Data;
   UINT16 ExpData;

   if((!ExpListLen)||(!ChkListLen))
   {
      Printf(Tee::PriHigh, "*** ERROR: Invalid input ExpListLen 0x%x, ChkListLen = %d ***\n",
             (UINT32)ExpListLen, (UINT32)ChkListLen);
       return RC::ILWALID_INPUT;
   }
   // Initilaize the Checked data
   Add = (volatile UINT16*)(*pFragList)[0].Address;
   ChkTotal =  (*pFragList)[0].ByteSize/2;

   for(iExp = 0; iExp<ExpListLen; iExp++)
   {
      if((*pFragListExp)[iExp].ByteSize%2)
      {
         Printf(Tee::PriHigh, "*** ERROR: Invalid input ByteSize 0x%x, iExp = %d***\n",
                (*pFragListExp)[iExp].ByteSize, (UINT32)iExp);
      }
      if(((LwUPtr)(*pFragListExp)[iExp].Address)%2)
      {
         Printf(Tee::PriHigh, "*** ERROR: Invalid input Address 0x%p, iExp = %d***\n",
                (*pFragListExp)[iExp].Address, (UINT32)iExp);
      }
      ExpWord = (*pFragListExp)[iExp].ByteSize/2;
      ExpAdd = (volatile UINT16*)(*pFragListExp)[iExp].Address;
      ExpTotal += ExpWord;

      for(jExp=0; jExp<ExpWord; jExp++, WordCompared++, jChk++)
      {
         // Advance the buffer list
         if(jChk>=((*pFragList)[iChk].ByteSize)/2)
         {
            iChk++;
            jChk=0;
            if(iChk>=ChkListLen)
            {
               Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList16(), iChk=0x%x, total = 0x%x ***\n",
                      (UINT32)iChk, (UINT32)(pFragList->size())-1 );
               return RC::MEMFRAG_SIZE_MISMATCH;
            }
            if((*pFragList)[iChk].ByteSize%2)
            {
               Printf(Tee::PriHigh, "*** ERROR: Invalid input ByteSize 0x%x, iExp = %d***\n",
                      (*pFragList)[iChk].ByteSize, (UINT32)iExp);
            }
            if(((LwUPtr)(*pFragList)[iChk].Address)%2)
            {
               Printf(Tee::PriHigh, "*** ERROR: Invalid input Address %p, iExp = %d***\n",
                      (*pFragList)[iChk].Address, (UINT32)iExp);
            }
            Add = (volatile UINT16*)(*pFragList)[iChk].Address;
            ChkTotal +=  (*pFragList)[iChk].ByteSize/2;
         }
         ExpData = MEM_RD16(&ExpAdd[jExp]);
         Data = MEM_RD16(&Add[jChk]);
         if(Data!=ExpData)
         {
            Printf(Tee::PriDebug, "\tChecked Buffer\n");
            Print08(pFragList, Tee::PriDebug);
            Printf(Tee::PriDebug, "\tExpected Buffer\n");
            Print08(pFragListExp, Tee::PriDebug);

            Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList16() ***\n");
            Printf(Tee::PriHigh, "\tMismatch @ %d, Data [0x%x, 0x%x] = 0x%x, Expt [%x, %x] = 0x%x\n",
                   (UINT32)WordCompared, (UINT32)iChk, (UINT32)jChk, Data,
                   (UINT32)iExp, (UINT32)jExp, ExpData);
            return RC::DATA_MISMATCH;
         }
      }
   }

   if(iChk!=pFragList->size()-1)
   {
      Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList16(), iChk=0x%x, total = 0x%x ***\n",
             (UINT32)iChk, (UINT32)(pFragList->size())-1 );
      // Not reached the last Fragment yet
      return RC::MEMFRAG_SIZE_MISMATCH;
   }
   if(ChkTotal!=ExpTotal)
   {
      Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList16(), ChkTotal=0x%x, Exptotal = 0x%x ***\n",
             (UINT32)ChkTotal, (UINT32)ExpTotal );
      // Total Number of Bytes not match
      return RC::MEMFRAG_SIZE_MISMATCH;
   }

   return OK;
}

RC MemoryFragment::CompareFragList32
      (
         FRAGLIST * pFragList,
         FRAGLIST * pFragListExp
      )
{
    if((!pFragList)||(!pFragListExp))
    {
       Printf(Tee::PriHigh, "*** ERROR: MemoryFragment::CompareFragList32() Invalid List ***\n");
        return RC::ILWALID_INPUT;
    }

    //Error checking for give data
    size_t ChkListLen = pFragList->size();
    size_t ExpListLen = pFragListExp->size();
    size_t WordCompared = 0;
    size_t ExpTotal = 0;
    size_t ChkTotal = 0;
    size_t iChk = 0;
    size_t jChk = 0;
    size_t iExp, jExp;

    volatile UINT32 * ExpAdd;
    UINT32  ExpDword;
    volatile UINT32 * Add;
    UINT32 Data;
    UINT32 ExpData;

    if((!ExpListLen)||(!ChkListLen))
    {
       Printf(Tee::PriHigh, "*** ERROR: Invalid input ExpListLen 0x%x, ChkListLen = %d ***\n",
              (UINT32)ExpListLen, (UINT32)ChkListLen);
        return RC::ILWALID_INPUT;
    }
    // Initilaize the Checked data
    Add = (volatile UINT32*)(*pFragList)[0].Address;
    ChkTotal =  (*pFragList)[0].ByteSize/4;

    for(iExp = 0; iExp<ExpListLen; iExp++)
    {
       if((*pFragListExp)[iExp].ByteSize%4)
       {
          Printf(Tee::PriHigh, "*** ERROR: Invalid input ByteSize 0x%x, iExp = %d***\n",
                 (*pFragListExp)[iExp].ByteSize, (UINT32)iExp);
       }
       if(((LwUPtr)(*pFragListExp)[iExp].Address)%4)
       {
          Printf(Tee::PriHigh, "*** ERROR: Invalid input Address 0x%p, iExp = %d***\n",
                 (*pFragListExp)[iExp].Address, (UINT32)iExp);
       }
       ExpDword = (*pFragListExp)[iExp].ByteSize/4;
       ExpAdd = (volatile UINT32*)(*pFragListExp)[iExp].Address;
       ExpTotal += ExpDword;

       for(jExp=0; jExp<ExpDword; jExp++, WordCompared++, jChk++)
       {
          // Advance the buffer list
          if(jChk>=((*pFragList)[iChk].ByteSize)/4)
          {
             iChk++;
             jChk=0;
             if(iChk>=ChkListLen)
             {
                Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList32(), iChk=0x%x, total = 0x%x ***\n",
                       (UINT32)iChk, (UINT32)(pFragList->size())-1 );
                return RC::MEMFRAG_SIZE_MISMATCH;
             }
             if((*pFragList)[iChk].ByteSize%4)
             {
                Printf(Tee::PriHigh, "*** ERROR: Invalid input ByteSize 0x%x, iExp = %d***\n",
                       (*pFragList)[iChk].ByteSize, (UINT32)iExp);
             }
             if(((LwUPtr)(*pFragList)[iChk].Address)%4)
             {
                Printf(Tee::PriHigh, "*** ERROR: Invalid input Address %p, iExp = %d***\n",
                       (*pFragList)[iChk].Address, (UINT32)iExp);
             }
             Add = (volatile UINT32*)(*pFragList)[iChk].Address;
             ChkTotal +=  (*pFragList)[iChk].ByteSize/4;
          }
          ExpData = MEM_RD32(&ExpAdd[jExp]);
          Data = MEM_RD32(&Add[jChk]);
          if(Data!=ExpData)
          {
             Printf(Tee::PriDebug, "\tChecked Buffer\n");
             Print08(pFragList, Tee::PriDebug);
             Printf(Tee::PriDebug, "\tExpected Buffer\n");
             Print08(pFragListExp, Tee::PriDebug);

             Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList32() ***\n");
             Printf(Tee::PriHigh, "\tMismatch @ %d, Data [0x%x, 0x%x] = 0x%x, Expt [%x, %x] = 0x%x\n",
                    (UINT32)WordCompared, (UINT32)iChk, (UINT32)jChk, Data,
                    (UINT32)iExp, (UINT32)jExp, ExpData);
             return RC::DATA_MISMATCH;
          }
       }
    }

    if(iChk!=pFragList->size()-1)
    {
       Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList32(), iChk=0x%x, total = 0x%x ***\n",
              (UINT32)iChk, (UINT32)(pFragList->size())-1 );
       // Not reached the last Fragment yet
       return RC::MEMFRAG_SIZE_MISMATCH;
    }
    if(ChkTotal!=ExpTotal)
    {
       Printf(Tee::PriHigh, "*** ERROR: MemFrag::CompareFragList32(), ChkTotal=0x%x, Exptotal = 0x%x ***\n",
              (UINT32)ChkTotal, (UINT32)ExpTotal );
       // Total Number of Bytes not match
       return RC::MEMFRAG_SIZE_MISMATCH;
    }

    return OK;
}

void MemoryFragment::PrintFragList(FRAGLIST * pFragList, Tee::Priority Pri)
{
   MASSERT(pFragList);

   Printf(Pri, "\tIndex\tAddr\t\t(Physical)\tBytes\n");
   for(size_t i = 0; i<pFragList->size(); i++)
   {
      Printf(Pri, "\t%d\t%16p\t0x%llx\t0x%08x\n", (UINT32)i,
             (*pFragList)[i].Address,
             ((*pFragList)[i].PhysAddr) ? (*pFragList)[i].PhysAddr
                                          : Platform::VirtualToPhysical((*pFragList)[i].Address),
             (*pFragList)[i].ByteSize);
   }
   return;
}

void MemoryFragment::Print08(FRAGLIST * pFragList, Tee::Priority Pri)
{
   MASSERT(pFragList);

   for(size_t i = 0; i<pFragList->size(); i++)
   {
      Printf(Pri, "\tIndex %d\t%16p\t(Phys)0x%llx\t0x%08x\n", (UINT32)i,
             (*pFragList)[i].Address,
             ((*pFragList)[i].PhysAddr) ? (*pFragList)[i].PhysAddr
                                          : Platform::VirtualToPhysical((*pFragList)[i].Address),
             (*pFragList)[i].ByteSize);
      Memory::Print08((*pFragList)[i].Address, (*pFragList)[i].ByteSize, Pri);
   }
   return;
}

void MemoryFragment::Print16(FRAGLIST * pFragList, Tee::Priority Pri)
{
   MASSERT(pFragList);

   for(size_t i = 0; i<pFragList->size(); i++)
   {
      Printf(Pri, "\tIndex %d\t%16p\t(Phys)0x%llx\t0x%08x\n", (UINT32)i,
             (*pFragList)[i].Address,
             ((*pFragList)[i].PhysAddr) ? (*pFragList)[i].PhysAddr
                                          : Platform::VirtualToPhysical((*pFragList)[i].Address),
             (*pFragList)[i].ByteSize);
      Memory::Print16((*pFragList)[i].Address, (*pFragList)[i].ByteSize, Pri);
   }
   return;
}

void MemoryFragment::Print32(FRAGLIST * pFragList, Tee::Priority Pri)
{
   MASSERT(pFragList);

   for(size_t i = 0; i<pFragList->size(); i++)
   {
      Printf(Pri, "\tIndex%d\t%16p\t(Phys)0x%llx\t0x%08x\n", (UINT32)i,
             (*pFragList)[i].Address,
             ((*pFragList)[i].PhysAddr) ? (*pFragList)[i].PhysAddr
                                          : Platform::VirtualToPhysical((*pFragList)[i].Address),
             (*pFragList)[i].ByteSize);
      Memory::Print32((*pFragList)[i].Address, (*pFragList)[i].ByteSize, Pri);
   }
   return;
}

