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

// Memory interface.

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_MEMORYIF_H
#define INCLUDED_MEMORYIF_H

#ifndef INCLUDED_MEMTYPES_H
   #include "memtypes.h"
#endif
#ifndef INCLUDED_SCRIPT_H
   #include "script.h"
#endif
#ifndef INCLUDED_FPICKER_H
   #include "fpicker.h"
#endif
#ifndef INCLUDED_RANDOM_H
   #include "random.h"
#endif
#ifndef INCLUDED_TEE_H
   #include "tee.h"
#endif
#ifndef INCLUDED_COLOR_H
   #include "color.h"
#endif
#ifndef INCLUDED_STL_CSTDLIB
   #include <cstdlib>
   #define INCLUDED_STL_CSTDLIB
#endif

#define MEM_ADDRESS_32BIT  32
#define MEM_ADDRESS_64BIT  64

namespace Memory
{
   extern UINT32 Allocated_Memory;

   // For tests: get a JavaScript object property of type Memory::Location.
   RC GetLocationProperty
      (
      const SObject   & object,
      const SProperty & property,
      Location        * pLoc
      );
   RC GetLocationProperty
      (
      const JSObject * pObject,
      const char *     propName,
      Location       * pLoc
      );

/*RC Memory::GetSystemMemModelProperty
      (
         const SObject &      object,
         const SProperty &    property,
         Memory::SystemModel *   pSys
      );
*/

   UINT08 Read08
      (
      void * Address
      );

   UINT16 Read16
      (
      void * Address
      );

   UINT32 Read32
      (
      void * Address
      );

   void Write08
      (
      void * Address,
      UINT08 Data
      );

   void Write16
      (
      void * Address,
      UINT16 Data
      );

   void Write32
      (
      void * Address,
      UINT32 Data
      );

   void Fill08
      (
      volatile void * Address,
      UINT08 Data,
      UINT32 Words
      );

   void Fill16
      (
      volatile void * Address,
      UINT16 Data,
      UINT32 Words
      );

   void Fill32
      (
      volatile void * Address,
      UINT32 Data,
      UINT32 Words
      );

   void Fill64
      (
      volatile void * Address,
      UINT64 Data,
      UINT32 Words
      );

   //! Fill Memory with random data
   /**
     * @param Address:     The starting address of Memory
     * @param Seed:        The seed for the default random object
     * @param Bytes:       Total Number of bytes in memory need to be filled
     */
   RC FillRandom
      (
      void * Address,
      UINT32 Seed,
      size_t Bytes
      );

   RC FillRandom
      (
      void * Address,
      size_t Bytes,
      Random& random
      );

   RC FillRandom32
      (
      void * Address,
      UINT32 Seed,
      size_t Bytes
      );

   RC FillConstant
      (
      void * Address,
      UINT32 Constant,
      UINT32 Width,
      UINT32 Height,
      UINT32 Depth,
      UINT32 Pitch
      );

   RC FillAddress
      (
      void * Address,
      UINT32 Width,
      UINT32 Height,
      UINT32 Depth,
      UINT32 Pitch
      );

   //! Fill Memory with UINT08 vector, Size is controlled by size of vector
   /**
     * @param Address:     The starting address of Memory
     * @param paDta08:     Pointer of vector of UINT08
     */
   RC Fill
      (
      volatile void * Address,
      const vector<UINT08> * pData08
      );

   //! Fill Memory with UINT16 vector, Size is controlled by size of vector
   /**
     * @param Address:     The starting address of Memory
     * @param pData16:     Pointer of vector of UINT16
     */
   RC Fill
      (
      volatile void * Address,
      const vector<UINT16> * pData16
      );

   //! Fill Memory with UINT32 vector, Size is controlled by size of vector
   /**
     * @param Address:     The starting address of Memory
     * @param pData32:     Pointer of vector of UINT32
     */
   RC Fill
      (
      volatile void * Address,
      const vector<UINT32> * pData32
      );

   //! Fill Memory with 8bit data pattern
   /**
     * @param Address:     Starting address of Memory
     * @param pPattern:    Pointer of 8bit pattern in vector
     * @param ByteSize:    Total number of bytes in memroy need to be filled
     * @param StartIndex:  Index/offset of pattern where started to fill, default 0
     */
   RC FillPattern
      (
      void * Address,
      const vector<UINT08> * pPattern,
      UINT32 ByteSize,
      UINT32 StartIndex = 0
      );

   //! Fill Memory with 16bit data pattern
   /**
     * @param Address:     Starting address of Memory, have to be word aligned
     * @param pPattern:    Pointer of 16bit pattern in vector
     * @param ByteSize:    Total number of bytes in memroy need to be filled
     * @param StartIndex:  Index/offset of pattern where started to fill, default 0
     */
   RC FillPattern
      (
      void * Address,
      const vector<UINT16> * pPattern,
      UINT32 ByteSize,
      UINT32 StartIndex = 0
      );

   //! Fill Memory with 32bit data pattern
   /**
     * @param Address:     Starting address of Memory, have to be dword aligned
     * @param pPattern:    Pointer of 32bit pattern in vector
     * @param ByteSize:    Total number of bytes in memroy need to be filled
     * @param StartIndex:  Index/offset of pattern where started to fill, default 0
     */
   RC FillPattern
      (
      void * Address,
      const vector<UINT32> * pPattern,
      UINT32 ByteSize,
      UINT32 StartIndex = 0
      );

   RC FillSurfaceWithPattern
      (
      void * Address,
      const vector<UINT32> *pPattern,
      UINT32 WidthInBytes,
      UINT32 Lines,
      UINT32 PitchInBytes
      );

   RC FillRandom
      (
      void * Address,
      UINT32 Seed,
      UINT32 Low,
      UINT32 High,
      UINT32 Width,
      UINT32 Height,
      UINT32 Depth,
      UINT32 Pitch
      );

   RC FillWeightedRandom
      (
      void *              Address,
      UINT32              Seed,
      Random::PickItem *  pItems,   // Remember to run PreparePickItems first!
      UINT32              Width,
      UINT32              Height,
      UINT32              Depth,
      UINT32              Pitch
      );
   RC FillWeightedRandom
      (
      void *              Address,
      FancyPicker &       Picker,
      UINT32              Width,
      UINT32              Height,
      UINT32              Depth,
      UINT32              Pitch
      );

   RC FillStripey
      (
      void * Address,
      UINT32 Width,
      UINT32 Height,
      UINT32 Depth,
      UINT32 Pitch
      );

   RC FillRgb
      (
      void * Address,
      UINT32 Width,
      UINT32 Height,
      UINT32 Depth,
      UINT32 Pitch
      );

   RC FillRgbScaled
      (
      void *  Address,
      UINT32  Width,
      UINT32  Height,
      UINT32  Depth,
      UINT32  Pitch,
      FLOAT64 HScale,
      FLOAT64 VScale,
      INT32   X0
      );

   RC FillRgbBars
      (
      void * Address,
      UINT32 Width,
      UINT32 Height,
      ColorUtils::Format Format,
      UINT32 Pitch
      );

   RC FillStripes
      (
      void * Address,
      UINT32 Width,
      UINT32 Height,
      UINT32 Depth,
      UINT32 Pitch
      );

   RC FillTvPattern
      (
      void * Address,
      UINT32 Width,
      UINT32 Height,
      UINT32 Depth,
      UINT32 Pitch
      );

   RC Tile
      (
      void *   Address,
      UINT32   SrcWidth,
      UINT32   SrcHeight,
      UINT32   Depth,
      UINT32   Pitch,
      UINT32   DstWidth,
      UINT32   DstHeight
      );
   // Copies a smaller tile from upper left across rest of bigger buffer.

   RC FillSine
      (
      void * Address,
      FLOAT64 Period,
      UINT32 Width,
      UINT32 Height,
      UINT32 Depth,
      UINT32 Pitch
      );
   // fills memory with sine wave with a period of Period pixels

    //! \brief Replaces bits in RGB part with its Complement
    //! does not touch Alpha component.
    //!
    //! \param pAddress: virtual address of surface which is modified in this function.
    //! \param width: width of surface in pixels
    //! \param height: height of surface
    //! \param format: Colour format of the surface. Only A8R8G8B8 is needed now.
    //!                More support can be added
    //! \param pitch: pitch of Pitch kind surfaces.
    //!
    //! \return RC
    RC FlipRGB
    (
        void * pAddress,
        UINT32 width,
        UINT32 height,
        ColorUtils::Format format,
        UINT32 pitch
    );

    //! \brief copy the complemented RGB bits from one surface to another
    //! does not touch Alpha component.
    //!
    //! \param pSrcAdd: virtual address of filled surface to be used as reference.
    //! \param pDestAdd: virtual address of surface which is modified in this function.
    //! \param width: width of surface in pixels
    //! \param height: height of surface
    //! \param format: Colour format of the surface. Only A8R8G8B8 is needed now.
    //!                More support can be added
    //! \param pitch: pitch of Pitch kind surfaces.
    //!
    //! \return RC
    RC FlipRGB
    (
        void * pSrcAdd,
        void * pDestAdd,
        UINT32 width,
        UINT32 height,
        ColorUtils::Format format,
        UINT32 pitch
    );


   //! Dump memory content to a 8bit vector
   /**
     * @param Address:     Starting address of Memory
     * @param pData:       Pointer of vector will contain the dumpped data
     * @param ByteSize:    Total number of bytes in memroy to be dumpped
     */
   RC Dump
      (
      void * Address,
      vector<UINT08> * pData,
      UINT32 ByteSize
      );

   //! Dump memory content to a 16bit vector
   /**
     * @param Address:     Starting address of Memory, has to be WORD aligned
     * @param pData:       Pointer of vector will contain the dumpped data
     * @param ByteSize:    Total number of bytes in memroy to be dumpped
     */
   RC Dump
      (
      void * Address,
      vector<UINT16> * pData,
      UINT32 ByteSize
      );

   //! Dump memory content to a 32bit vector
   /**
     * @param Address:     Starting address of Memory, has to be DWORD aligned
     * @param pData:       Pointer of vector will contain the dumpped data
     * @param ByteSize:    Total number of bytes in memroy to be dumpped
     */
   RC Dump
      (
      void * Address,
      vector<UINT32> * pData,
      UINT32 ByteSize
      );

   //! Print memroy content in byte(8 bit) format
   /**
     * @param Address:     Starting address of Memory
     * @param ByteSize:    Total number of bytes in memroy to be printed
     * @param Pri:         Print priority
     * @param BytePerLine: Number of bytes printed each line
     */
   void Print08
      (
      volatile void * Address,
      UINT32 ByteSize,
      Tee::Priority Pri = Tee::PriNormal,
      UINT08 BytePerLine = 0x10
      );

   //! Print memroy content in WORD(16 bit) format
   /**
     * @param Address:     Starting address of Memory, has to be WORD aligned
     * @param ByteSize:    Total number of bytes in memroy to be printed
     * @param Pri:         Print priority
     * @param BytePerLine: Number of words printed each line
     */
   void Print16
      (
      volatile void * Address,
      UINT32 ByteSize,
      Tee::Priority Pri = Tee::PriNormal,
      UINT08 WordPerLine = 8
      );

   //! Print memroy content in DWORD(32 bit) format
   /**
     * @param Address:     Starting address of Memory, has to be WORD aligned
     * @param ByteSize:    Total number of bytes in memroy to be printed
     * @param Pri:         Print priority
     * @param BytePerLine: Number of dwords printed each line
     */
   void Print32
      (
      volatile void * Address,
      UINT32 ByteSize,
      Tee::Priority Pri = Tee::PriNormal,
      UINT08 DWordPerLine = 4
      );

   inline void Print08(volatile void * address, UINT32 byteSize, Tee::PriDebugStub, UINT08 bytePerLine = 0x10)
   {
       Print08(address, byteSize, Tee::PriSecret, bytePerLine);
   }

   inline void Print16(volatile void * address, UINT32 byteSize, Tee::PriDebugStub, UINT08 wordPerLine = 8)
   {
       Print08(address, byteSize, Tee::PriSecret, wordPerLine);
   }

   inline void Print32(volatile void * address, UINT32 byteSize, Tee::PriDebugStub, UINT08 dWordPerLine = 4)
   {
       Print08(address, byteSize, Tee::PriSecret, dWordPerLine);
   }

   //! Print memroy content in floating point format
   /**
     * @param Address:     Starting address of Memory, has to be WORD aligned
     * @param ByteSize:    Total number of bytes in memroy to be printed
     * @param Pri:         Print priority
     * @param BytePerLine: Number of floating point printed each line
     */
   void PrintFloat
      (
      volatile void * Address,
      UINT32 ByteSize,
      Tee::Priority Pri = Tee::PriNormal,
      UINT08 FloatPerLine = 4
      );

   //! Compare the content in 2 memory locations, and report the first failure location
   /**
     * @param Address1:    Starting address1 of Memory to be compared
     * @param Address2:    Starting address2 of Memory to be compared
     * @param ByteSize:    Total number of bytes to be compared
     */
   RC Compare
      (
      void * Address1,
      void * Address2,
      UINT32 ByteSize
      );

   //! Compare the content in memory locations with 8bit pattern
   /**
     * @param Address:     Starting address1 of Memory to be compared
     * @param pPattern:    Pointer of 8bit(byte) expected pattern vector
     * @param ByteSize:    Total number of bytes to be compared
     * @param PatternOffset:  The offset of pattern will started, default 0
     */
   RC ComparePattern
      (
      void * Addres,
      vector<UINT08> * pPattern,
      UINT32 ByteSize,
      UINT32 PatternOffset = 0
      );

   //! Compare the content in memory locations with 16bit pattern
   /**
     * @param Address:     Starting address1 of Memory to be compared, WORD aligned
     * @param pPattern:    Pointer of 16bit(WORD) expected pattern vector
     * @param ByteSize:    Total number of bytes to be compared
     * @param PatternOffset:  The offset of pattern will started, default 0
     */
   RC ComparePattern
      (
      void * Address,
      vector<UINT16> * pPattern,
      UINT32 ByteSize,
      UINT32 PatternOffset = 0
      );

   //! Compare the content in memory locations with 32bit pattern
   /**
     * @param Address:     Starting address1 of Memory to be compared, DWORD aligned
     * @param pPattern:    Pointer of 32bit(WORD) expected pattern vector
     * @param ByteSize:    Total number of bytes to be compared
     * @param PatternOffset:  The offset of pattern will started, default 0
     */
   RC ComparePattern
      (
      void * Address,
      vector<UINT32> * pPattern,
      UINT32 ByteSize,
      UINT32 PatternOffset = 0
      );

   //! Compare the content in memory locations with 8bit datas in vector
   /**
     * @param Address:     Starting address1 of Memory to be compared
     * @param pvData08:    Pointer of 8bit(byte) expected data in vector
     * @param ByteSize:    Total number of bytes to be compared
     *  @param IsFullCmp:  Whether to compare the memory to the whole vector
     * @param DataOffset:  The start point(offset) of data, default 0
     */
   RC CompareVector
       (
       void * Addres,
       vector<UINT08> * pvData08,
       UINT32 ByteSize,
       bool IsFullCmp = true,
       size_t DataOffset = 0
       );

   //! Compare the content in memory locations with 16bit datas in vector
   /**
     * @param Address:     Starting address1 of Memory to be compared, WORD aligned
     * @param pvData08:    Pointer of 16bit(word) expected data in vector
     * @param ByteSize:    Total number of bytes to be compared
     * @param IsFullCmp:  Whether to compare the memory to the whole vector
     * @param DataOffset:  The start point(offset) of data, default 0
     */
    RC CompareVector
       (
       void * Address,
       vector<UINT16> * pvData16,
       UINT32 ByteSize,
       bool IsFullCmp = true,
       size_t DataOffset = 0
       );

    //! Translate Attrib to string
    /**
      * @param Attrib:     enum to translate
      */
    const char *GetMemAttribName(Memory::Attrib ma);

    //! Compare the content in memory locations with 32bit datas in vector
    /**
      * @param Address:     Starting address1 of Memory to be compared, DWORD aligned
      * @param pvData08:    Pointer of 32bit(dword) expected data in vector
      * @param ByteSize:    Total number of bytes to be compared
      *  @param IsFullCmp:  Whether to compare the memory to the whole vector
      * @param DataOffset:  The start point(offset) of data, default 0
      */
    RC CompareVector
       (
       void * Address,
       vector<UINT32> * pData32,
       UINT32 ByteSize,
       bool IsFullCmp = true,
       size_t DataOffset = 0
       );

    //! @{
    //! @brief A data pattern, used by WfMats, GLStress, and other tests.
    extern const vector<UINT32> PatShortMats;
    extern const vector<UINT32> PatLongMats;
    extern const vector<UINT32> PatWalkingOnes;
    extern const vector<UINT32> PatWalkingZeros;
    extern const vector<UINT32> PatSuperZeroOne;
    extern const vector<UINT32> PatByteSuper;
    extern const vector<UINT32> PatCheckerBoard;
    extern const vector<UINT32> PatIlwCheckerBd;
    extern const vector<UINT32> PatSolidZero;
    extern const vector<UINT32> PatSolidOne;
    extern const vector<UINT32> PatWalkSwizzle;
    extern const vector<UINT32> PatWalkNormal;
    extern const vector<UINT32> PatSolidAlpha;
    extern const vector<UINT32> PatSolidRed;
    extern const vector<UINT32> PatSolidGreen;
    extern const vector<UINT32> PatSolidBlue;
    extern const vector<UINT32> PatSolidWhite;
    extern const vector<UINT32> PatSolidCyan;
    extern const vector<UINT32> PatSolidMagenta;
    extern const vector<UINT32> PatSolidYellow;
    extern const vector<UINT32> PatDoubleZeroOne;
    extern const vector<UINT32> PatTripleSuper;
    extern const vector<UINT32> PatQuadZeroOne;
    extern const vector<UINT32> PatTripleWhammy;
    extern const vector<UINT32> PatIsolatedOnes;
    extern const vector<UINT32> PatIsolatedZeros;
    extern const vector<UINT32> PatSlowFalling;
    extern const vector<UINT32> PatSlowRising;
    //@ @}
}

#endif // !INCLUDED_MEMORYIF_H
