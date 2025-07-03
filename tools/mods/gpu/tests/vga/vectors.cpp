//
//    VECTORS.C - Test vector generation library interface
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       20 February 1996
//    Last modified: 3 February 2005
//
//    Routines in this file:
//    VectorStart             Initialize the global test vector generation logic
//    VectorEnd               Terminate test vector generation
//    VectorIOByteWrite       Write a byte to a specific I/O port
//    VectorIOByteRead        Read a byte from a specific I/O port
//    VectorIOByteTest        Test a byte at a specific I/O port
//    VectorIOWordWrite       Write a word to a specific I/O port
//    VectorIOWordRead        Read a word from a specific I/O port
//    VectorMemByteWrite         Write a byte to a memory location
//    VectorMemByteRead       Read a byte from a memory location
//    VectorMemByteTest       Test a byte at a memory location
//    VectorMemWordWrite         Write a word to a memory location
//    VectorMemWordRead       Read a word from a memory location
//    VectorMemDwordWrite        Write a dword to a memory location
//    VectorMemDwordRead         Read a dword from a memory location
//    VectorCaptureFrame         Capture a video frame
//    VectorWaitLwrsorBlink      Wait for the cursor blink state to change
//    VectorWaitAttrBlink        Wait for the attribute blink state to change
//    VectorDumpMemory        Dump physical memory to OEM's file format
//    VectorComment           Add a comment to a test vector stream
//
#include <stdio.h>
#include <string.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

#define  GEN_VECTORS       1

#ifdef GEN_VECTORS
static   FILE        *fileVectors = NULL;
static   BOOL        _bInMemDump = FALSE;

static DWORD SO2Lin (SEGOFF);
#endif

//
//    VectorStart - Initialize the global test vector generation logic
//
//    Entry:   None
//    Exit:    <int>    Error code (0 = Success)
//
int VectorStart (void)
{
#ifdef GEN_VECTORS
   // Truncate and test vector file
   fileVectors = fopen ("TEMP.VEC", "w");
   if (fileVectors == NULL)
      return (ERROR_SIM_INIT);

   return (ERROR_NONE);
#else
   return (ERROR_NONE);
#endif
}

//
//    VectorEnd - Terminate test vector generation
//
//    Entry:   None
//    Exit:    None
//
void VectorEnd (void)
{
#ifdef GEN_VECTORS
   if (fileVectors != NULL)
      fclose (fileVectors);
#endif
}

//
//    VectorIOByteWrite - Write a byte to a specific I/O port
//
//    Entry:   wPort    I/O address
//             byData   Data to write
//    Exit:    None
//
void VectorIOByteWrite (WORD wPort, BYTE byData)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\niowr_b(32'h%08X, 8'h%02X);", wPort, byData);
#else
   // Prevent compiler warnings
   wPort = wPort;
   byData = byData;
#endif
}

//
//    VectorIOByteRead - Read a byte from a specific I/O port
//
//    Entry:   wPort    I/O address
//    Exit:    None
//
void VectorIOByteRead (WORD wPort)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\niocmp_b(32'h%08X, 8'h00, 8'h00);", wPort);
#else
   // Prevent compiler warnings
   wPort = wPort;
#endif
}

//
//    VectorIOByteTest - Test a byte at a specific I/O port
//
//    Entry:   wPort       I/O address
//             byExp       Expected data
//    Exit:    None
//
void VectorIOByteTest (WORD wPort, BYTE byExp)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\niocmp_b(32'h%08X, 8'hFF, 32'h%08X);", wPort, byExp);
#else
   // Prevent compiler warnings
   wPort = wPort;
   byExp = byExp;
#endif
}

//
//    VectorIOWordWrite - Write a word to a specific I/O port
//
//    Entry:   wPort    I/O address
//             wData    Data to write
//    Exit:    None
//
void VectorIOWordWrite (WORD wPort, WORD wData)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\niowr_w(32'h%08X, 16'h%04X);", wPort, wData);
#else
   // Prevent compiler warnings
   wPort = wPort;
   wData = wData;
#endif
}

//
//    VectorIOWordRead - Read a word from a specific I/O port
//
//    Entry:   wPort    I/O address
//    Exit:    None
//
void VectorIOWordRead (WORD wPort)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\niocmp_w(32'h%08X, 16'h0000, 16'h0000);", wPort);
#else
   // Prevent compiler warnings
   wPort = wPort;
#endif
}

//
//    VectorMemByteWrite - Write a byte to a memory location
//
//    Entry:   lpMem    Memory address
//             byData   Data to write
//    Exit:    None
//
void VectorMemByteWrite (SEGOFF lpMem, BYTE byData)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\nmemwr_b(32'h%08X, 8'h%02X);", SO2Lin(lpMem), byData);
#else
   // Prevent compiler warnings
   lpMem = lpMem;
   byData = byData;
#endif
}

//
//    VectorMemByteRead - Read a byte from a memory location
//
//    Entry:   lpMem    Memory address
//    Exit:    None
//
void VectorMemByteRead (SEGOFF lpMem)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\nmemcmp_b(32'h%08X, 8'h00, 8'h00);", SO2Lin(lpMem));
#else
   // Prevent compiler warnings
   lpMem = lpMem;
#endif
}

//
//    VectorMemByteTest - Test a byte at a memory location
//
//    Entry:   lpMem    Memory address
//             byExp    Expected data
//    Exit:    None
//
void VectorMemByteTest (SEGOFF lpMem, BYTE byExp)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\nmemcmp_b(32'h%08X, 8'hFF, 8'h%02X);", SO2Lin(lpMem), byExp);
#else
   // Prevent compiler warnings
   lpMem = lpMem;
   byExp = byExp;
#endif
}

//
//    VectorMemWordWrite - Write a word to a memory location
//
//    Entry:   lpMem    Memory address
//             wData    Data to write
//    Exit:    None
//
void VectorMemWordWrite (SEGOFF lpMem, WORD wData)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\nmemwr_w(32'h%08X, 16'h%04X);", SO2Lin(lpMem), wData);
#else
   // Prevent compiler warnings
   lpMem = lpMem;
   wData = wData;
#endif
}

//
//    VectorMemWordRead - Read a word from a memory location
//
//    Entry:   lpMem    Memory address
//    Exit:    None
//
void VectorMemWordRead (SEGOFF lpMem)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\nmemcmp_w(32'h%08X, 16'h0000, 16'h0000);", SO2Lin(lpMem));
#else
   // Prevent compiler warnings
   lpMem = lpMem;
#endif
}

//
//    VectorMemDwordWrite - Write a dword to a memory location
//
//    Entry:   lpMem    Memory address
//             dwData   Data to write
//    Exit:    None
//
void VectorMemDwordWrite (SEGOFF lpMem, DWORD dwData)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\nmemwr_dw(32'h%08X, 32'h%08X);", SO2Lin(lpMem), dwData);
#else
   // Prevent compiler warnings
   lpMem = lpMem;
   dwData = dwData;
#endif
}

//
//    VectorMemDwordRead - Read a dword from a memory location
//
//    Entry:   lpMem    Memory address
//    Exit:    None
//
void VectorMemDwordRead (SEGOFF lpMem)
{
#ifdef GEN_VECTORS
   if (fileVectors == NULL) return;
   if (_bInMemDump) return;
   fprintf (fileVectors, "\nmemcmp_dw(32'h%08X, 32'h00000000, 32'h00000000);", SO2Lin(lpMem));
#else
   // Prevent compiler warnings
   lpMem = lpMem;
#endif
}

//
//    VectorCaptureFrame - Capture a video frame
//
//    Entry:   lpFilename     Filename to capture to
//    Exit:    None
//
void VectorCaptureFrame (LPSTR lpFilename)
{
#ifdef GEN_VECTORS
   static BOOL bFirst = TRUE;
   static int  nFrame = 1;
   WORD        wIStatus1;

   if (fileVectors == NULL) return;

   _bInMemDump = TRUE;        // Prevent the next I/O from generating a vector
   wIStatus1 = (IOByteRead (MISC_INPUT) & 0x01) ? INPUT_CSTATUS_1 : INPUT_MSTATUS_1;
   _bInMemDump = FALSE;       // Restore it

   // If this is the first frame captured, then throw away a "garbage" frame.
   if (bFirst)
   {
      bFirst = FALSE;         // Not a virgin simulation anymore
      fprintf (fileVectors, "\n// Skip blanked (garbage) frame");
      fprintf (fileVectors, "\niopoll_b(32'h%08X, 8'h08, 8'h08, 32'h100);", wIStatus1);
      fprintf (fileVectors, "\niopoll_b(32'h%08X, 8'h08, 8'h00, 32'h100);", wIStatus1);
   }

   // Generate a frame
   fprintf (fileVectors, "\n// Generate frame %d", nFrame++);
   fprintf (fileVectors, "\niopoll_b(32'h%08X, 8'h08, 8'h08, 32'h100);", wIStatus1);
   fprintf (fileVectors, "\niopoll_b(32'h%08X, 8'h08, 8'h00, 32'h100);", wIStatus1);
#else
   // Prevent compiler warnings
   lpFilename = lpFilename;
#endif
}

//
//    VectorWaitLwrsorBlink - Wait for the cursor blink state to change
//
//    Entry:   wState   Cursor state to wait for:
//                         BLINK_ON
//                         BLINK_OFF
//    Exit:    None
//
void VectorWaitLwrsorBlink (WORD wState)
{
}

//
//    VectorWaitAttrBlink - Wait for the attribute blink state to change
//
//    Entry:   wState   Attribute state to wait for:
//                         BLINK_ON
//                         BLINK_OFF
//    Exit:    None
//
void VectorWaitAttrBlink (WORD wState)
{
}

//
//    VectorDumpMemory - Dump physical memory to OEM's file format
//
//    Entry:   szFilename  Memory image file
//    Exit: None
//
void VectorDumpMemory (LPCSTR szFilename)
{
#if GEN_VECTORS
   BYTE        bySEQIdx, byGDCIdx;
   BYTE        bySEQ04, byGDC04, byGDC05, byGDC06;
// BYTE        byP0, byP1, byP2, byP3;
   BYTE        byPixels[8];
   DWORD       i;
   SEGOFF         soVideo;
   FILE        *fsimMem;
#ifdef __X86_16__
   static char szFile[128];
#endif

   // Prevent vectors from being dumped
   _bInMemDump = TRUE;

   // Truncate and open memory image file
#ifdef __X86_16__
   if (_fstrlen (szFilename) > (sizeof (szFile) - 1)) return;
   _fstrcpy (szFile, szFilename);
   fsimMem = fopen (szFile, "w+b");
#else
   fsimMem = fopen (szFilename, "w+b");
#endif

   // Write file header information
// fprintf (fsimMem, "// Memory\n10000\n@00000000\n");

   bySEQIdx = IOByteRead (SEQ_INDEX);
   byGDCIdx = IOByteRead (GDC_INDEX);

   // Set planar mode for read
   IOByteWrite (SEQ_INDEX, 0x04);
   bySEQ04 = IOByteRead (SEQ_DATA);
   IOByteWrite (SEQ_DATA, 0x06);

   IOByteWrite (GDC_INDEX, 0x04);
   byGDC04 = IOByteRead (GDC_DATA);

   IOByteWrite (GDC_INDEX, 0x05);
   byGDC05 = IOByteRead (GDC_DATA);
   IOByteWrite (GDC_DATA, 0x00);

   IOByteWrite (GDC_INDEX, 0x06);
   byGDC06 = IOByteRead (GDC_DATA);
   IOByteWrite (GDC_DATA, 0x05);

   // Get memory data
   soVideo = (SEGOFF) (0xA0000000);
   memset (byPixels, 0, 8);
   for (i = 0; i < 0x10000; i++)
   {
      IOWordWrite (GDC_INDEX, 0x0004);
//    byP0 = MemByteRead (soVideo);
      byPixels[0] = MemByteRead (soVideo);
      IOWordWrite (GDC_INDEX, 0x0104);
//    byP1 = MemByteRead (soVideo);
      byPixels[1] = MemByteRead (soVideo);
      IOWordWrite (GDC_INDEX, 0x0204);
//    byP2 = MemByteRead (soVideo);
      byPixels[2] = MemByteRead (soVideo);
      IOWordWrite (GDC_INDEX, 0x0304);
//    byP3 = MemByteRead (soVideo);
      byPixels[3] = MemByteRead (soVideo);
//    fprintf (fsimMem, "%02X%02X%02X%02X\n", byP3, byP2, byP1, byP0);
////     fwrite (byPixels, 1, 8, fsimMem);
      if (0 == fwrite (byPixels, 1, 4, fsimMem))
          break;
      soVideo++;
   }

   // Reset VGA to original state
   IOByteWrite (SEQ_INDEX, 0x04);
   IOByteWrite (SEQ_DATA, bySEQ04);
   IOByteWrite (GDC_INDEX, 0x04);
   IOByteWrite (GDC_DATA, byGDC04);
   IOByteWrite (GDC_INDEX, 0x05);
   IOByteWrite (GDC_DATA, byGDC05);
   IOByteWrite (GDC_INDEX, 0x06);
   IOByteWrite (GDC_DATA, byGDC06);

   IOByteWrite (SEQ_INDEX, bySEQIdx);
   IOByteWrite (GDC_INDEX, byGDCIdx);

   _bInMemDump = FALSE;
   fclose (fsimMem);
#else
   // Prevent compiler warnings
   szFilename = szFilename;
#endif
}

//
//    VectorComment - Add a comment to a test vector stream
//
//    Entry:   lpStr Pointer to a comment string
//    Exit:    None
//
void VectorComment (const char * lpStr)
{
#if GEN_VECTORS
   if (fileVectors == NULL) return;
   fprintf (fileVectors, "\n// %s", lpStr);
#else
   // Prevent compiler warnings
   lpStr = lpStr;
#endif
}

#ifdef GEN_VECTORS
//
//    SO2Lin - Colwerts segment:offset address to 20 bit linear address
//
//    Entry:   lpbyAddr    Segment:Offset address
//    Exit:    <DWORD>     20 bit linear address
//
DWORD SO2Lin (SEGOFF lpbyAddr)
{
   DWORD dwRetAddr;

   dwRetAddr = (((DWORD)lpbyAddr) & 0xFFFF);
   dwRetAddr += (((DWORD)lpbyAddr) & 0xFFFF0000) >> 12;

   return (dwRetAddr);
}
#endif

#ifdef LW_MODS
}
#endif

//
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
