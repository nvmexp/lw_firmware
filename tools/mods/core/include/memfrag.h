/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Memory interface.

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_MEMFRAG_H
#define INCLUDED_MEMFRAG_H

#ifndef INCLUDED_MEMTYPES_H
   #include "memtypes.h"
#endif
#ifndef INCLUDED_TEE_H
   #include "tee.h"
#endif
#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

#ifndef INCLUDED_MEMORYIF_H
#include "memoryif.h"
#endif

#define BYTE_SIZE  1
#define WORD_SIZE  2
#define DWORD_SIZE 4
#define QWORD_SIZE 8

//! MemoryFragment name space, manage a memroy fragment and a list of memory fragment
namespace MemoryFragment
{

   //! Memory Fragment data structure
   typedef struct _FRAGMENT
   {
      void * Address; // Virtual Address
      PHYSADDR PhysAddr; // Physical Address corresponding to Address.
                       // Should ignore if it is 0.
      UINT32 ByteSize;
   }FRAGMENT;

   //!A list of Memory Fragment
   typedef vector<FRAGMENT>  FRAGLIST;

   //! Generate a Fixed List of memory fragments for the given memory chunk
   /**
     * @param Address    Starting address of the whole memory chunk
     * @param AddrPhys   Physical Address corresponding to Address.
     *                   If provided, the AddrPhys field in each
     *                   Fragment will be updated, otherwise remain
     *                   0.
     * @param ByteSize   Total size of the given memory
     * @param DataWidth  Alignment, or width of data,valid input: 1, 2, 4
     * @param FragList  Input vector containing each fragment numbers
     */
   RC GenerateFixedFragment
      (
         void * Address,
         UINT32 AddrPhys,
         UINT32 ByteSize,
         FRAGLIST * pFragList,
         UINT32 DataWidth,
         vector <UINT32> FragList
      );

   //! Fill the memory fragment list with given pattern with byte data
   /**
     * @param pFragList   Pointer of given fragment list
     * @param pPattern08  Pointer of data pattern with 8bit(byte) data in
     * @param IsContinous Fill each fragment as a continuous buffer or each fragment start from start of pattern
     */
   RC FillPattern
      (
         FRAGLIST * pFragList,
         vector<UINT08> * pPattern08,
         bool IsContinuous = true
      );

   //! Fill the memory fragment list with given pattern with word data
   /**
     * @param pFragList   Pointer of given fragment list
     * @param pPattern16  Pointer of data pattern with 16bit(word) data in
     * @param IsContinous Fill each fragment as a continuous buffer or each fragment start from start of pattern
     */
   RC FillPattern
      (
         FRAGLIST * pFragList,
         vector<UINT16> * pPattern16,
         bool IsContinuous = true
      );

   //! Fill the memory fragment list with given pattern with dword data
   /**
     * @param pFragList   Pointer of given fragment list
     * @param pPattern32  Pointer of data pattern with 32bit(dword) data in
     * @param IsContinous Fill each fragment as a continuous buffer or each fragment start from start of pattern
     */
   RC FillPattern
      (
         FRAGLIST * pFragList,
         vector<UINT32> * pPattern32,
         bool IsContinuous = true
      );

   //! Fill the memory fragment list with random data in bytes
   /**
     * @param pFragList   Pointer of given fragment list
     * @param Seed        Seed for all of the fragments (not random!)
     */
   RC FillRandom
   (
       FRAGLIST * pFragList,
       UINT32 Seed
   );

   //! Copy content of the memory fragment list to given vector
   /**
     * @param pFragList  Pointer of given fragment list
     * @param pvData08   Pointer of data with 8bit(byte) data in
     */
   RC DumpToVector
      (
         FRAGLIST * pFragList,
         vector<UINT08> * pvData08
      );

   //! Copy content of the memory fragment list to given vector
   /**
     * @param pFragList  Pointer of given fragment list
     * @param pvData16   Pointer of data with 16bit(word) data in
     */
   RC DumpToVector
      (
         FRAGLIST * pFragList,
         vector<UINT16> * pvData16
      );

   //! Copy content of the memory fragment list to given vector
   /**
     * @param pFragList  Pointer of given fragment list
     * @param pvData32   Pointer of data with 32bit(dword) data in
     */
   RC DumpToVector
      (
         FRAGLIST * pFragList,
         vector<UINT32> * pvData32
      );

   //! Compare the memory fragment list with given pattern
   /**
     * @param pFragList   Pointer of given fragment list
     * @param pPattern08  Pointer of data pattern with 8bit(byte) data in
     * @param IsContinous compare each fragment as a continuous buffer or each fragment start from start of pattern
     */
   RC ComparePattern
      (
         FRAGLIST * pFragList,
         vector<UINT08> * pPattern08,
         bool IsContinous = true
      );

   //! Compare the memory fragment list with given pattern
   /**
     * @param pFragList   Pointer of given fragment list
     * @param pPattern16  Pointer of data pattern with 16bit(word) data in
     * @param IsContinous compare each fragment as a continuous buffer or each fragment start from start of pattern
     */
   RC ComparePattern
      (
         FRAGLIST * pFragList,
         vector<UINT16> * pPattern16,
         bool IsContinous = true
      );

   //! Compare the memory fragment list with given pattern
   /**
     * @param pFragList   Pointer of given fragment list
     * @param pPattern32  Pointer of data pattern with 32bit(eword) data in
     * @param IsContinous compare each fragment as a continuous buffer or each fragment start from start of pattern
     */
   RC ComparePattern
      (
         FRAGLIST * pFragList,
         vector<UINT32> * pPattern32,
         bool IsContinous = true
      );

   //! Compare the memory fragment list with given data list
   /**
     * @param pFragList  Pointer of given fragment list
     * @param pvData08   Pointer of data with 8bit(byte) data in
     */
   RC CompareVector
      (
         FRAGLIST * pFragList,
         vector<UINT08> * pvData08
      );

   //! Compare the memory fragment list with given data list
   /**
     * @param pFragList  Pointer of given fragment list
     * @param pvData16   Pointer of data with 16bit(word) data in
     */
   RC CompareVector
      (
         FRAGLIST * pFragList,
         vector<UINT16> * pvData16
      );

   //! Compare the memory fragment list with given data list
   /**
     * @param pFragList  Pointer of given fragment list
     * @param pvData32   Pointer of data with 32bit(dword) data in
     */
   RC CompareVector
      (
         FRAGLIST * pFragList,
         vector<UINT32> * pvData32
      );

   //! Compare one memory fragment list with another in bytes
   /**
     * @param pFragList     Pointer of given fragment list
     * @param pFragListExp  Pointer of fragment list with expected data in
     */
   RC CompareFragList08
      (
         FRAGLIST * pFragList,
         FRAGLIST * pFragListExp
      );

   //! Compare one memory fragment list with another in words
   /**
     * @param pFragList     Pointer of given fragment list
     * @param pFragListExp  Pointer of fragment list with expected data in
     */
   RC CompareFragList16
      (
         FRAGLIST * pFragList,
         FRAGLIST * pFragListExp
      );

   //! Compare one memory fragment list with another in dwords
   /**
     * @param pFragList     Pointer of given fragment list
     * @param pFragListExp  Pointer of fragment list with expected data in
     */
   RC CompareFragList32
      (
         FRAGLIST * pFragList,
         FRAGLIST * pFragListExp
      );

   //! Print info of fragment in Fragment List
   void PrintFragList(FRAGLIST * pFragList, Tee::Priority Pri = Tee::PriNormal);

   //! Print buffer in each fragment contained in the fragment list in byte format
   void Print08(FRAGLIST * pFragList, Tee::Priority Pri = Tee::PriNormal);
   //! Print buffer in each fragment contained in the fragment list in word format
   void Print16(FRAGLIST * pFragList, Tee::Priority Pri = Tee::PriNormal);
   //! Print buffer in each fragment contained in the fragment list in dword format
   void Print32(FRAGLIST * pFragList, Tee::Priority Pri = Tee::PriNormal);

   inline void PrintFragList(FRAGLIST* pFragList, Tee::PriDebugStub)
   {
       PrintFragList(pFragList, Tee::PriSecret);
   }

   inline void Print08(FRAGLIST* pFragList, Tee::PriDebugStub)
   {
       Print08(pFragList, Tee::PriSecret);
   }

   inline void Print16(FRAGLIST* pFragList, Tee::PriDebugStub)
   {
       Print08(pFragList, Tee::PriSecret);
   }

   inline void Print32(FRAGLIST* pFragList, Tee::PriDebugStub)
   {
       Print08(pFragList, Tee::PriSecret);
   }
}

#endif // !INCLUDED_MEMORYFRAG_H
