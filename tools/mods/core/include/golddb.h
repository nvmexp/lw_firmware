/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#ifndef INCLUDED_GOLDDB_H
#define INCLUDED_GOLDDB_H

#ifndef INCLUDED_GOLDEN_H
#include "golden.h"
#endif

//------------------------------------------------------------------------------
// GoldenDb
//
// Used only by the Goldelwalues class.
//
//------------------------------------------------------------------------------
namespace GoldenDb
{
   enum { StrLen = 80-1 };     // Max string length for test or rec names.

   typedef const void* RecHandle;

   RC GetHandle
   (
      const char *   TestName,
      const char *   RecName,
      UINT32         Code,
      UINT32         NumVals,
      RecHandle *    pHandle
   );
      // Get a handle for reading or writing records.

   RC GetRec
   (
      RecHandle         Handle,
      UINT32            Loop,
      const UINT32 **   ppData
   );
      // Get the stored data for a particular loop.

   RC PutRec
   (
      RecHandle      Handle,
      UINT32         Loop,
      const UINT32 * pData,
      bool           compare
   );
      // Store a record for a particular loop.
      // If compare, and we have a matching record, report error if old
      // data doesn't match new data.

   RC CompareToLoop
   (
      RecHandle      Handle,
      UINT32         Loop,
      const UINT32 * pData,
      bool         * pEqual
   );
      // Compares CRC values provided in pData to values stored for Loop

   void UnmarkAllRecs();
      // Clear the "mark" on all in-memory golden records.

   RC MarkRecs
   (
      const char *   TestName,
      UINT32         Loop
   );
      // Mark any record for the given test name at the given loop number.

   RC MarkRecsWithName
   (
      const char *   Name
   );
      // mark any record that contains Name in the RecName

   RC ReadFile
   (
      const char *  filename,
      UINT32        pri,
      bool          compareValues
   );
      // Read a compacted binary file into memory.
      // Print header info with verbosity.
      // If compareValues, and matching records are found in memory,
      //   print any miscompares between memory and file records, return
      //   golden miscompare error.

   RC WriteFile
   (
      const char * filename,
      bool         markedOnly,
      UINT32       statusPri
   );
      // Write a compacted binary file.  If markedOnly is true, only marked
      //  records will be written, the rest skipped.

   RC PrintValues
   (
      const char *   TestName,
      int            Columns
   );
      // Print values to output stream.

}; // namespace GoldenDb

#endif // INCLUDED_GOLDDB_H

