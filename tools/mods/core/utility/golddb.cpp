/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013,2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "core/include/golden.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/tee.h"
#include "core/include/version.h"
#include "core/include/fileholder.h"
#include <vector>
#include <string>
#include <set>
#include <cstdio>
#include <cstring>

//------------------------------------------------------------------------------
// GoldenDb -- database used by Goldelwalues class.
//
//------------------------------------------------------------------------------

using namespace GoldenDb;

const UINT32 LwrrentVersion = 5;

#pragma pack(1)
//------------------------------------------------------------------------------
// Binary disk format:
//
struct DiskFileHdr
{
   UINT32   Magic;               // Always 0x19951105 (Jocelyn's birthday)
   UINT32   Version;             // Version 4

   UINT32   RunHdrCount;         // num records in run header section
   UINT32   RunHdrOffset;        // file byte-offset of beginning of run header section

   UINT32   DataCount;           // num 32-bit values in the data section
   UINT32   DataOffset;          // file byte-offset of beginning of data section

   char     Gelwersion[64];      // version of mods that generated this file (null-padded)
};
struct DiskRunHdr
{
   char     TestName[StrLen+1];  // test name (null padded)
   char     RecName[StrLen+1];   // record name (null padded)
   UINT32   Code;                // what CRC code was used to generate these records
   UINT32   ValsPerRec;          // number of code values per record
   UINT32   StartLoop;           // Loop number of first record in block
   UINT32   IncrLoop;            // Loop number increment between records
   UINT32   NumRecs;             // Number of records
   UINT32   DataOffset;          // offset into data section of first code value
};

//------------------------------------------------------------------------------
// In-memory data structures for the database:
//
struct MemRecHdr
{
   char     TestName[StrLen+1];  // test name (null padded)
   char     RecName[StrLen+1];   // record name (null padded)
   UINT32   Code;                // what CRC/checksum method generated this data
   UINT32   ValsPerRec;          // number of code values per record

   //
   // Constructors, destructor, operator=
   //
   // We need these to support the STL set<> container.
   //
   MemRecHdr();
   MemRecHdr(const char *, const char *, UINT32, UINT32);
   MemRecHdr(const MemRecHdr &);
   ~MemRecHdr();
   const MemRecHdr & operator=(const MemRecHdr &);
};
bool operator< (const MemRecHdr & A, const MemRecHdr & B);
typedef set< MemRecHdr >  HdrSet;

struct MemRec
{
   const MemRecHdr * Hdr;        // reference to header in s_Hdrs.
   UINT32 * Data;                // Points to s_RecHdrs[HdrIdx].ValsPerRec items.
   UINT32   Loop;                // loop number of first record in this run
   UINT32   Mark;                // to "mark" some records for writing to disk

    // 24 bytes to here on 64-bit build.
    // Fill with inline CRCs up to a power of 2 to avoid wasting space when
    // using the Pool heap manager.
    static const size_t NUM_INLINE_CRCS = (32-24) / sizeof(UINT32);
    UINT32   InlineCrcs[NUM_INLINE_CRCS];

   //
   // Constructors, destructor, operator=
   //
   // We need these to support the STL set<> container.
   //
   MemRec(const MemRecHdr *, UINT32, const UINT32*);
   MemRec(const MemRecHdr *, UINT32);
   MemRec(const MemRec &);
   ~MemRec();
   const MemRec & operator=(const MemRec &);
   void AllocData()
   {
      if (Hdr->ValsPerRec > NUM_INLINE_CRCS)
         Data = new UINT32 [Hdr->ValsPerRec];
      else
         Data = InlineCrcs;
   }
};
bool operator< (const MemRec & A, const MemRec & B);
typedef set< MemRec >  RecSet;

#pragma pack()

//------------------------------------------------------------------------------
// Private: module global data
//
static HdrSet s_Hdrs;
static RecSet s_Recs;

//------------------------------------------------------------------------------
// Public: get a handle for reading or writing records.
//
RC GoldenDb::GetHandle
(
   const char *   TestName,
   const char *   RecName,
   UINT32         Code,
   UINT32         NumVals,
   RecHandle *    pHandle
)
{
   // Check for valid arguments.

   if( (strlen(TestName) > StrLen) ||
       (strlen(RecName) > StrLen) )
      return RC::GOLDEN_NAME_TOO_LONG;

   MASSERT( pHandle );

   // Construct a dummy record header.

   MemRecHdr TmpHdr(TestName, RecName, Code, NumVals);

   // Get an iterator for this record header (creates one if necessary).

   pair< HdrSet::iterator, bool> hdrPair;

   hdrPair = s_Hdrs.insert( TmpHdr );

   *pHandle = (RecHandle)&*hdrPair.first;

   return OK;
}

//------------------------------------------------------------------------------
// Public: get the stored data for a particular loop.
//
RC GoldenDb::GetRec
(
   RecHandle         Handle,
   UINT32            Loop,
   const UINT32 **   ppData
)
{
   MASSERT( Handle );
   MASSERT( ppData );

   const MemRecHdr * Hdr = (const MemRecHdr *)Handle;

   // Construct a dummy record.

   MemRec TmpRec(Hdr, Loop);

   // Find a matching record, if present.

   RecSet::const_iterator RecIt;

   RecIt = s_Recs.find( TmpRec );

   if( RecIt == s_Recs.end() )
   {
      Printf(Tee::PriHigh, "%s[%s, ",
            Hdr->TestName,
            Hdr->RecName
            );
      Goldelwalues::PrintCodesValue(Hdr->Code, Tee::PriHigh);
      Printf(Tee::PriHigh, ", %d][%d] golden value not found.\n",
            Hdr->ValsPerRec,
            Loop
            );
      return RC::GOLDEN_VALUE_NOT_FOUND;
   }

   *ppData = (*RecIt).Data;
   return OK;
}

//------------------------------------------------------------------------------
// Public: store a record for a particular loop.
RC GoldenDb::PutRec
(
   RecHandle      Handle,
   UINT32         Loop,
   const UINT32 * pData,
   bool           compare
)
{
   MASSERT( Handle );
   MASSERT( pData );

   const MemRecHdr * Hdr = (const MemRecHdr *)Handle;
   RC rc = OK;

   // Construct a new record.

   MemRec TmpRec(Hdr, Loop, pData);

   RecSet::iterator RecIt;

   RecIt = s_Recs.find( TmpRec );

   if( RecIt != s_Recs.end() )
   {
      // Update the existing record with new data.

      UINT32 * pOldData  = (*RecIt).Data;
      UINT32 i;

      for(i = 0; i < Hdr->ValsPerRec; i++)
      {
         if (compare && (pOldData[i] != pData[i]))
            rc = RC::GOLDEN_VALUE_MISCOMPARE;
         else
            pOldData[i] = pData[i];
      }
      if (rc)
      {
         Printf(Tee::PriHigh, "Miscompare: %s[%s, ",
               Hdr->TestName,
               Hdr->RecName
               );
         Goldelwalues::PrintCodesValue(Hdr->Code, Tee::PriHigh);
         Printf(Tee::PriHigh, ", %d][%d]\n",
               Hdr->ValsPerRec,
               Loop
               );
         Printf(Tee::PriDebug, "\told={");
         for(i = 0; i < Hdr->ValsPerRec; i++)
         {
            Printf(Tee::PriDebug, "%c0x%08x",
                  i ? ',' : ' ',
                  pOldData[i]
                  );
         }
         Printf(Tee::PriDebug, "}\n\tnew={");
         for(i = 0; i < Hdr->ValsPerRec; i++)
         {
            Printf(Tee::PriDebug, "%c0x%08x",
                  i ? ',' : ' ',
                  pData[i]
                  );
         }
         Printf(Tee::PriDebug, "}\n");
      }
   }
   else
   {
      // Add the new record.

      s_Recs.insert( TmpRec );
   }
   return rc;
}

//------------------------------------------------------------------------------
// Public: Compares CRC values provided in pData to values stored for Loop
RC GoldenDb::CompareToLoop
(
    RecHandle      Handle,
    UINT32         Loop,
    const UINT32 * pData,
    bool         * pEqual
)
{
    MASSERT( Handle );
    MASSERT( pData );
    MASSERT( pEqual );

    *pEqual = true;
    const MemRecHdr * Hdr = (const MemRecHdr *)Handle;

    MemRec TmpRec(Hdr, Loop, pData);
    RecSet::iterator RecIt = s_Recs.find( TmpRec );

    if (RecIt == s_Recs.end() )
        return RC::GOLDEN_VALUE_NOT_FOUND;

    UINT32 * pOldData  = (*RecIt).Data;

    for (UINT32 i = 0; i < Hdr->ValsPerRec; i++)
    {
        if (pOldData[i] != pData[i])
        {
            *pEqual = false;
            return OK;
        }
    }
    return OK;
}

#define SWAP(x32) x32 = (((x32) >> 24) & 0xFF)      + \
                        (((x32) & 0x00FF0000) >> 8) + \
                        (((x32) & 0x0000FF00) << 8) + \
                        ((x32) << 24)

//------------------------------------------------------------------------------
// Public: load a binary file from disk into the database.
//
RC GoldenDb::ReadFile
(
   const char * filename,
   UINT32       pri,
   bool         compareValues
)
{
   MASSERT( filename );

   RC rc          = OK;
   RC deferred_rc = OK;
   bool ByteSwap  = false;
   DiskFileHdr FileHeader;

   FileHolder file;
   vector<DiskRunHdr> RunHeaders;
   vector<UINT32> DataBuf;

   {
      CHECK_RC(file.Open(filename, "rb"));

      // Read in the file header.
      if( 1 != fread(&FileHeader, sizeof(FileHeader), 1, file.GetFile()) )
      {
          Printf(Tee::PriError, "Failure reading \"%s\".\n", filename);
          return RC::FILE_READ_ERROR;
      }
      switch (FileHeader.Magic)
      {
         case 0x19951105:
            break;

         case 0x05119519:
            Printf(pri, "File is byte-reversed...\n");
            ByteSwap = true;
            break;

         default:
             Printf(Tee::PriError, "Failure reading \"%s\".\n", filename);
             return RC::ILWALID_FILE_FORMAT;
      }

      if( ByteSwap )
      {
         SWAP(FileHeader.Version);
      }
      if (FileHeader.Version != LwrrentVersion)
      {
         Printf(Tee::PriError,
               "%s is file format version %d, expected %d.\n",
               filename,
               FileHeader.Version,
               LwrrentVersion
               );
         return  RC::ILWALID_FILE_FORMAT;
      }

      if( ByteSwap )
      {
         SWAP(FileHeader.RunHdrCount);
         SWAP(FileHeader.RunHdrOffset);
         SWAP(FileHeader.DataCount);
         SWAP(FileHeader.DataOffset);
      }
      if (0 != strcmp(FileHeader.Gelwersion, g_Version))
      {
         Printf(Tee::PriError,
               "%s was generated by MODS version %s, expected %s.\n",
               filename,
               FileHeader.Gelwersion,
               g_Version
               );
         deferred_rc = RC::GOLDEN_FILE_MISMATCH;

         // Go ahead and load the file, but return error later.
         // Let the script/user decide if they want to continue.
      }

      // Read in all the Run headers.

      RunHeaders.resize(FileHeader.RunHdrCount);
      if( fseek(file.GetFile(), FileHeader.RunHdrOffset, SEEK_SET) )
      {
             Printf(Tee::PriError, "Failure reading \"%s\".\n", filename);
             return RC::FILE_READ_ERROR;
      }
      if( (FileHeader.RunHdrCount > 0) &&
          (FileHeader.RunHdrCount !=
            fread(&RunHeaders[0], sizeof(DiskRunHdr), FileHeader.RunHdrCount, file.GetFile()) ))
      {
          Printf(Tee::PriError, "Failure reading \"%s\".\n", filename);
          return RC::FILE_READ_ERROR;
      }
      UINT32 TotalBytes = ftell(file.GetFile());
      UINT32 TotalRecords = 0;

      for (UINT32 RunHdrIdx = 0; RunHdrIdx < FileHeader.RunHdrCount; ++RunHdrIdx)
      {
         DiskRunHdr * pRunHdr = &RunHeaders[RunHdrIdx];
         if( ByteSwap )
         {
            SWAP(pRunHdr->Code);
            SWAP(pRunHdr->ValsPerRec);
            SWAP(pRunHdr->StartLoop);
            SWAP(pRunHdr->IncrLoop);
            SWAP(pRunHdr->NumRecs);
            SWAP(pRunHdr->DataOffset);
         }

         Printf(pri, "\t%s[%s, ",
               pRunHdr->TestName,
               pRunHdr->RecName
               );
         Goldelwalues::PrintCodesValue(pRunHdr->Code, (Tee::Priority)pri);
         Printf(pri, ", %d][%d..%d] (%d records, skip=%d)\n",
               pRunHdr->ValsPerRec,
               pRunHdr->StartLoop,
               pRunHdr->StartLoop + (pRunHdr->NumRecs - 1)*pRunHdr->IncrLoop,
               pRunHdr->NumRecs,
               pRunHdr->IncrLoop
               );

         if (pRunHdr->ValsPerRec == 0)
         {
             Printf(Tee::PriError, "Failure reading \"%s\" - ValsPerRec=0.\n", filename);
             return RC::FILE_READ_ERROR;
         }

         // Get a handle for this run.

         RecHandle handle;

         CHECK_RC(GetHandle(
               pRunHdr->TestName,
               pRunHdr->RecName,
               (Goldelwalues::Code)pRunHdr->Code,
               pRunHdr->ValsPerRec,
               &handle ));

         // Get a buffer to read the record data into.
         if( DataBuf.size() < pRunHdr->ValsPerRec )
         {
            DataBuf.resize(pRunHdr->ValsPerRec);
         }

         // Seek to the beginning of the data for this run.

         if( fseek(file.GetFile(), pRunHdr->DataOffset, SEEK_SET) )
         {
             Printf(Tee::PriError, "Failure reading \"%s\".\n", filename);
             return RC::FILE_READ_ERROR;
         }

         // For each record in the run:

         UINT32 rec;
         for( rec = 0; rec < pRunHdr->NumRecs; rec++ )
         {
            // Read the data for this record:

            if( pRunHdr->ValsPerRec !=
                     fread(&DataBuf[0], sizeof(UINT32), pRunHdr->ValsPerRec, file.GetFile()) )
            {
                Printf(Tee::PriError, "Failure reading \"%s\".\n", filename);
                return RC::FILE_READ_ERROR;
            }
            if( ByteSwap )
            {
               UINT32 i;
               for(i = 0; i < pRunHdr->ValsPerRec; i++ )
                  SWAP(DataBuf[i]);
            }

            // Add the record to the in-memory database.

            rc = PutRec(
                  handle,
                  pRunHdr->StartLoop + rec * pRunHdr->IncrLoop,
                  &DataBuf[0],
                  compareValues
                  );
            if (rc == RC::GOLDEN_VALUE_MISCOMPARE)
            {
               if (OK == deferred_rc)
                  deferred_rc = rc;
               else
                   return rc;  // 2nd error, bail!
            }
            else if( OK != rc )
            {
                return rc;
            }
         }
         TotalRecords += pRunHdr->NumRecs;
      }

      Printf(Tee::PriLow, "Done reading \"%s\" runs=%d records=%d values=%d bytes=%d\n",
            filename,
            FileHeader.RunHdrCount,
            TotalRecords,
            FileHeader.DataCount,
            TotalBytes );
   }

   if (rc != OK)
       return rc;

   return deferred_rc;
}

//------------------------------------------------------------------------------
// Public: write all golden values to a binary file.
//
RC GoldenDb::WriteFile(const char * filename, bool markedOnly, UINT32 statusPri)
{
   RC rc = OK;
   FileHolder file;

   MASSERT( filename );

   {
      // Open the file (raw byte mode, overwrite/create)

      CHECK_RC(file.Open(filename, "wb"));

      // Write a placeholder file header.

      DiskFileHdr FileHeader;

      FileHeader.Magic        = 0x19951105;
      FileHeader.Version      = LwrrentVersion;
      FileHeader.RunHdrCount  = 0;     // update this later!
      FileHeader.RunHdrOffset = 0;     // update this later!
      FileHeader.DataCount    = 0;     // update this later!
      FileHeader.DataOffset   = sizeof(FileHeader);

      memset(FileHeader.Gelwersion, '\0', sizeof(FileHeader.Gelwersion));
      strncpy(FileHeader.Gelwersion, g_Version, sizeof(FileHeader.Gelwersion)-1);

      if( 1 != fwrite(&FileHeader, sizeof(FileHeader), 1, file.GetFile()) )
      {
          Printf(Tee::PriHigh, "Failure writing \"%s\".\n", filename );
          return RC::FILE_WRITE_ERROR;
      }

      DiskRunHdr LwrRunHeader;
      memset( &LwrRunHeader, '\0', sizeof(LwrRunHeader) );

      const MemRecHdr * LwrMemRecHdr = 0;

      vector< DiskRunHdr > RunHeaders;

      RecSet::const_iterator RecIt    = s_Recs.begin();
      RecSet::const_iterator RecItEnd = s_Recs.end();

      UINT32 TotalRecords = 0;

      // For each record:

      for( /**/; RecIt != RecItEnd; ++RecIt )
      {
         if (markedOnly && !(*RecIt).Mark)
            continue;

         // If this is the second rec in a run, set the loop increment.

         if( (LwrRunHeader.NumRecs == 1) &&
             (LwrMemRecHdr == (*RecIt).Hdr) )
         {
            MASSERT( (*RecIt).Loop > LwrRunHeader.StartLoop );

            LwrRunHeader.IncrLoop = (*RecIt).Loop - LwrRunHeader.StartLoop;
         }
         UINT32 nextLoop = LwrRunHeader.StartLoop +
                           LwrRunHeader.IncrLoop * LwrRunHeader.NumRecs;

         // If this record doesn't fit into the current run, start a new one.

         if( (LwrMemRecHdr != (*RecIt).Hdr) ||
             (nextLoop     != (*RecIt).Loop) )
         {
            // Save old run header.
            if( LwrRunHeader.NumRecs )
            {
               RunHeaders.push_back(LwrRunHeader);
               TotalRecords += LwrRunHeader.NumRecs;

               Printf(Tee::PriLow, "\t%s[%s, ",
                     LwrRunHeader.TestName,
                     LwrRunHeader.RecName
                     );
               Goldelwalues::PrintCodesValue(LwrRunHeader.Code, Tee::PriLow);
               Printf(Tee::PriLow, ", %d][%d..%d] (%d records, skip=%d)\n",
                     LwrRunHeader.ValsPerRec,
                     LwrRunHeader.StartLoop,
                     LwrRunHeader.StartLoop + (LwrRunHeader.NumRecs - 1)*LwrRunHeader.IncrLoop,
                     LwrRunHeader.NumRecs,
                     LwrRunHeader.IncrLoop
                     );
            }

            LwrMemRecHdr = (*RecIt).Hdr;

            memcpy(LwrRunHeader.TestName, LwrMemRecHdr->TestName, sizeof(LwrRunHeader.TestName));
            memcpy(LwrRunHeader.RecName,  LwrMemRecHdr->RecName,  sizeof(LwrRunHeader.RecName));
            LwrRunHeader.Code          = LwrMemRecHdr->Code;
            LwrRunHeader.ValsPerRec    = LwrMemRecHdr->ValsPerRec;
            LwrRunHeader.StartLoop     = (*RecIt).Loop;
            LwrRunHeader.IncrLoop      = 1;
            LwrRunHeader.NumRecs       = 0;
            LwrRunHeader.DataOffset    = ftell(file.GetFile());
         }

         // Store the data for this record.

         if( LwrRunHeader.ValsPerRec !=
               fwrite((*RecIt).Data, sizeof(UINT32), LwrRunHeader.ValsPerRec, file.GetFile()) )
         {
             Printf(Tee::PriHigh, "Failure writing \"%s\".\n", filename );
             return RC::FILE_WRITE_ERROR;
         }
         LwrRunHeader.NumRecs++;
         FileHeader.DataCount += LwrRunHeader.ValsPerRec;
      }

      // Save last run header.
      if( LwrRunHeader.NumRecs )
      {
         RunHeaders.push_back(LwrRunHeader);
         TotalRecords += LwrRunHeader.NumRecs;

         Printf(Tee::PriLow, "\t%s[%s, ",
               LwrRunHeader.TestName,
               LwrRunHeader.RecName
               );
         Goldelwalues::PrintCodesValue(LwrRunHeader.Code, Tee::PriLow);
         Printf(Tee::PriLow, ", %d][%d..%d] (%d records, skip=%d)\n",
               LwrRunHeader.ValsPerRec,
               LwrRunHeader.StartLoop,
               LwrRunHeader.StartLoop + (LwrRunHeader.NumRecs - 1)*LwrRunHeader.IncrLoop,
               LwrRunHeader.NumRecs,
               LwrRunHeader.IncrLoop
               );
      }

      // Append the run headers to the end of the file.

      FileHeader.RunHdrCount  = (UINT32) RunHeaders.size();
      FileHeader.RunHdrOffset = ftell(file.GetFile());

      // NOTE: We are assuming vector<> is implemented as an array.
      if( (RunHeaders.size() > 0) &&
          (RunHeaders.size() !=
               fwrite( &RunHeaders[0], sizeof(DiskRunHdr), RunHeaders.size(), file.GetFile()) ))
      {
          Printf(Tee::PriHigh, "Failure writing \"%s\".\n", filename );
          return RC::FILE_WRITE_ERROR;
      }
      UINT32 TotalBytes = ftell(file.GetFile());

      // Seek back to the beginning of the file, rewrite the file header.

      if( fseek(file.GetFile(), 0, SEEK_SET) )
      {
          Printf(Tee::PriHigh, "Failure writing \"%s\".\n", filename );
          return RC::FILE_WRITE_ERROR;
      }
      if( 1 != fwrite(&FileHeader, sizeof(FileHeader), 1, file.GetFile()) )
      {
          Printf(Tee::PriHigh, "Failure writing \"%s\".\n", filename );
          return RC::FILE_WRITE_ERROR;
      }

      Printf(statusPri, "Success writing \"%s\" runs=%ld records=%d values=%d bytes=%d\n",
             filename,
             (long)RunHeaders.size(),
             TotalRecords,
             FileHeader.DataCount,
             TotalBytes );
   }

   return rc;
}

//------------------------------------------------------------------------------
// Public: print values to output stream.
//
RC GoldenDb::PrintValues
(
   const char *   TestName,
   int            Columns
)
{
   MASSERT( TestName );

   Printf(Tee::PriHigh,
      "%s.Golden.Values = \n"
      "{\n", TestName);

   int Column = 0;

   RecSet::const_iterator RecIt     = s_Recs.begin();
   RecSet::const_iterator RecItEnd  = s_Recs.end();

   for( /**/; RecIt != RecItEnd; ++RecIt )
   {
      int cmp = strcmp((*RecIt).Hdr->TestName, TestName);
      if( cmp < 0 )
         continue;
      if( cmp > 0 )
         break;

      // This one matches.  Print it.

      const MemRecHdr * Hdr = (*RecIt).Hdr;
      const UINT32 *    Data = (*RecIt).Data;
      MASSERT(Hdr);
      MASSERT(Data);

      Printf(Tee::PriHigh,
         "\t{\"%s\", %d, %d, %d, [",
         Hdr->RecName,
         Hdr->Code,
         Hdr->ValsPerRec,
         (*RecIt).Loop );

      for( UINT32 i = 0; i < Hdr->ValsPerRec; i++ )
         Printf(Tee::PriHigh, "0x%x,", Data[i] );

      Printf(Tee::PriHigh, "]}," );

      Column++;
      if( Column >= Columns )
      {
         Printf(Tee::PriHigh, "\n");
         Column = 0;
      }
   }
   if( Column )
         Printf(Tee::PriHigh, "\n");

   Printf(Tee::PriHigh, "};\n");

   return OK;
}

//------------------------------------------------------------------------------
// Public: clear the Mark field of all in-memory records.
//
void GoldenDb::UnmarkAllRecs()
{
   RecSet::iterator RecIt    = s_Recs.begin();
   RecSet::iterator RecItEnd = s_Recs.end();

   for( /**/; RecIt != RecItEnd; ++RecIt )
   {
      // s_Recs is an STL "set", which always stores its data as static.
      // This is because the set is sorted on its data values, not on a
      // separate key (otherwise, it would be a "map" not a "set").
      // You are supposed to delete and re-add a record when you
      // change it, to avoid unsorting the btree.
      //
      // I'm cheating here because I happen to know that the Mark member
      // is not compared for sorting, so changing it won't change the order.

      MemRec * p = const_cast<MemRec *>(&(*RecIt));  // cast away const
      p->Mark = false;
   }
}

//------------------------------------------------------------------------------
// Public: Mark any record for the given test name at the given loop number.
//
RC GoldenDb::MarkRecs
(
   const char *   TestName,
   UINT32         Loop
)
{
   MASSERT(TestName);

   // Simple linear search.  This doesn't have to be fast.

   RecSet::iterator RecIt    = s_Recs.begin();
   RecSet::iterator RecItEnd = s_Recs.end();

   for( /**/; RecIt != RecItEnd; ++RecIt )
   {
      int cmp = strcmp((*RecIt).Hdr->TestName, TestName);
      if ((0 == cmp) &&
          ((*RecIt).Loop == Loop))
      {
         // s_Recs is an STL "set", which always stores its data as static.
         // This is because the set is sorted on its data values, not on a
         // separate key (otherwise, it would be a "map" not a "set").
         // You are supposed to delete and re-add a record when you
         // change it, to avoid unsorting the btree.
         //
         // I'm cheating here because I happen to know that the Mark member
         // is not compared for sorting, so changing it won't change the order.

         MemRec * p = const_cast<MemRec *>(&(*RecIt));  // cast away const
         p->Mark = true;
      }
   }
   return OK;
}

//------------------------------------------------------------------------------
// Public: Mark any record that contains the given name in the RecName
//
RC GoldenDb::MarkRecsWithName
(
   const char *   Name
)
{
   MASSERT(Name);

   // Simple linear search.  This doesn't have to be fast.

   RecSet::iterator RecIt    = s_Recs.begin();
   RecSet::iterator RecItEnd = s_Recs.end();

   for( /**/; RecIt != RecItEnd; ++RecIt )
   {
      const char * cmp = strstr((*RecIt).Hdr->RecName, Name);
      if (NULL != cmp)
      {
         // s_Recs is an STL "set", which always stores its data as static.
         // This is because the set is sorted on its data values, not on a
         // separate key (otherwise, it would be a "map" not a "set").
         // You are supposed to delete and re-add a record when you
         // change it, to avoid unsorting the btree.
         //
         // I'm cheating here because I happen to know that the Mark member
         // is not compared for sorting, so changing it won't change the order.

         MemRec * p = const_cast<MemRec *>(&(*RecIt));  // cast away const
         p->Mark = true;
      }
   }
   return OK;
}

//------------------------------------------------------------------------------
//
// Private: housekeeping functions for MemRecHdr.
//
MemRecHdr::MemRecHdr()
{
   memset(TestName, '\0', sizeof(TestName));
   memset(RecName, '\0', sizeof(RecName));
   Code = 0;
   ValsPerRec = 0;
}
MemRecHdr::MemRecHdr(const char *testStr, const char *recStr, UINT32 code, UINT32 numVals )
{
   memset(TestName, '\0', sizeof(TestName));
   memset(RecName, '\0', sizeof(RecName));
   strncpy(TestName, testStr, sizeof(TestName)-1);
   strncpy(RecName, recStr, sizeof(RecName)-1);
   Code = code;
   ValsPerRec = numVals;
}
MemRecHdr::MemRecHdr(const MemRecHdr & rhs)
{
   memcpy(TestName, rhs.TestName, sizeof(TestName));
   memcpy(RecName, rhs.RecName, sizeof(RecName));
   Code = rhs.Code;
   ValsPerRec = rhs.ValsPerRec;
}
MemRecHdr::~MemRecHdr()
{
}
const MemRecHdr & MemRecHdr::operator=(const MemRecHdr & rhs)
{
   memcpy(TestName, rhs.TestName, sizeof(TestName));
   memcpy(RecName, rhs.RecName, sizeof(RecName));
   Code = rhs.Code;
   ValsPerRec = rhs.ValsPerRec;

   return *this;
}

//------------------------------------------------------------------------------
//
// Private: housekeeping functions for MemRec.
//
MemRec::MemRec(const MemRecHdr * hdr, UINT32 loop, const UINT32* data)
{
   MASSERT( data );
   MASSERT( hdr );

   Hdr = hdr;
   Loop = loop;
   AllocData();

   memcpy(Data, data, sizeof(*data) * Hdr->ValsPerRec);
   Mark = false;
}
MemRec::MemRec(const MemRecHdr * hdr, UINT32 loop)
{
   MASSERT( hdr );

   Hdr  = hdr;
   Loop = loop;
   Data = 0;
   Mark = false;
}
MemRec::MemRec(const MemRec & rhs)
{
   Hdr  = rhs.Hdr;
   Loop = rhs.Loop;
   AllocData();
   if( rhs.Data )
   {
      memcpy(Data, rhs.Data, sizeof(*Data) * Hdr->ValsPerRec);
   }
   else
   {
      MASSERT( rhs.Data );
      memset(Data, 0, sizeof(*Data) * Hdr->ValsPerRec);
   }

   Mark = rhs.Mark;
}
MemRec::~MemRec()
{
   if( Data && (Data != InlineCrcs) )
      delete [] Data;
}
const MemRec & MemRec::operator=(const MemRec & rhs)
{
   Hdr  = rhs.Hdr;
   Loop = rhs.Loop;
   AllocData();

   if( rhs.Data )
   {
      memcpy(Data, rhs.Data, sizeof(*Data) * Hdr->ValsPerRec);
   }
   else
   {
      MASSERT( 1 ); // shouldn't get here
      memset(Data, 0, sizeof(*Data) * Hdr->ValsPerRec);
   }
   Mark = rhs.Mark;

   return *this;
}

//------------------------------------------------------------------------------
// Private: sort ordering functions.
//
bool operator< (const MemRecHdr & A, const MemRecHdr & B)
{
   int cmp = strcmp(A.TestName, B.TestName);
   if( cmp < 0 )
      return true;
   if( cmp > 0 )
      return false;

   cmp = strcmp(A.RecName, B.RecName);
   if( cmp < 0 )
      return true;
   if( cmp > 0 )
      return false;

   if( A.Code < B.Code )
      return true;
   if( A.Code > B.Code )
      return false;

   return( A.ValsPerRec < B.ValsPerRec );
}

// including this prototype fixes a compile warning on the Mac.
bool operator< (const MemRec & A, const MemRec & B);

bool operator< (const MemRec & A, const MemRec & B)
{
   if( A.Hdr != B.Hdr )
      return (*A.Hdr < *B.Hdr);

   return (A.Loop < B.Loop);
}

