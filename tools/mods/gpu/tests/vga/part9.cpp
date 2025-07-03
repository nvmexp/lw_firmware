//
//      PART9.CPP - VGA Core Test Suite (Part 9)
//      Copyright (c) 1994-2004 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, Inc.
//      All rights reserved.
//
//      Written by:     Larry Coffey
//      Date:           14 December 2004
//      Last modified:  14 December 2005
//
//      Routines in this file:
//      IORWTest            Write a pattern to each I/O register and verify that there are no errors.
//      ByteModeTest        Load a pattern and set each of BYTE mode, WORD mode, and DWORD mode.
//      Chain2Chain4Test    Fill memory in each of chain/2 and chain/4 modes and read back specific locations
//      CGAHerlwlesTest     Test compatibility address line bits in CRTC[17]
//      LatchTest           Write all possible data value and copy each to another video memory location using the latches
//      Mode0Test           Verify memory is read/writable and test each mode 0, 0+, and 0*.
//      Mode3Test           Verify memory is read/writable and test each mode 3, 3+, and 3*.
//      Mode5Test           Verify memory is read/writable and test mode 5.
//      Mode6Test           Verify memory is read/writable and test mode 6.
//      Mode7Test           Verify memory is read/writable and test each mode 7+, and 7*.
//      ModeDTest           Verify memory is read/writable and test mode D.
//      Mode12Test          Verify memory is read/writable and test mode E/10/11/12.
//      ModeFTest           Verify memory is read/writable and test mode F.
//      Mode13Test          Verify memory is read/writable and test mode 13.
//      HiResMono1Test      Verify C&T Test #2 compatible operation
//      HiResMono2Test      Verify C&T Test #1 compatible operation
//      HiResColor1Test     Verify C&T Test #4 compatible operation
//      ModeXTest           Verify mode 'X' style test
//      MemoryAccessTest    Verify memory can be accessed as BYTE, WORD, and DWORD
//      MemoryAccessSubTest Test various BYTE, WORD, and DWORD accesses
//      RandomAccessTest    Verify that the VGA space can be accessed in any sequence
//      IOTest              Test a specific read/write I/O port
//      BurstMemoryWrites   Write a block of DWORD data to memory
//      ClearModeX          Clear Mode X memory
//      WritePatternStrip   Write a strip of DWORD patterns
//      VerifyPatternStrip  Verify a strip of DWORD patterns
//      VLoad2Test          Verify interaction between VLoad/2, Chain/2 and WORD mode
//
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <errno.h>
#include    "vgacore.h"
#include    "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

// Function prototypes of the global tests
int IORWTest (void);
int ByteModeTest (void);
int Chain2Chain4Test (void);
int CGAHerlwlesTest (void);
int LatchTest (void);
int Mode0Test (void);
int Mode3Test (void);
int Mode5Test (void);
int Mode6Test (void);
int Mode7Test (void);
int ModeDTest (void);
int Mode12Test (void);
int ModeFTest (void);
int Mode13Test (void);
int HiResMono1Test (void);
int HiResMono2Test (void);
int HiResColor1Test (void);
int ModeXTest (void);
int MemoryAccessTest (void);
int RandomAccessTest (void);
int VLoad2Test (void);

// Structures needed by some tests
typedef struct tagIOTABLE
{
    WORD    wport;
    WORD    rport;
    BOOL    fIndexed;
    BYTE    idx;
    BYTE    mask;
    BYTE    rsvdmask;
    BYTE    rsvddata;
    BOOL    bError;
    BYTE    byExp;
    BYTE    byAct;
} IOTABLE;

typedef struct tagIOEXCEPTIONS
{
    WORD    wport;
    BYTE    idx;
    BYTE    mask;
} IOEXCEPTIONS;

#define REF_PART        9
#define REF_TEST        1
//
//      T0901
//      IORWTest - Write a pattern to each I/O register and verify that there are no errors.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
//      Notes regarding the exception file:
//
//      The exception file, which defaults to "t0901.dat" in the current directory, contains a list
//      of "override" values for registers should a particular register fail. The exception file is
//      read only if failures occur, and values placed in there for working registers will simply
//      be ignored.
//
//      Each line of the file is an ASCII representation (of hexadecimal digits) of an entry in the
//      IOEXCEPTIONS data structure, where the fields are defined as follows:
//
//          wport   The I/O [write] address of the offending register: If the register is indexed,
//                  then use the data address of the register pair for the I/O address. For example,
//                  if sequencer, index 2, is the register in question, then use "3C5".
//
//          idx     The index value for indexed registers: If it's not an indexed register, then
//                  this field is set to a "0".
//
//          mask    The non-writable bits: If a bit can't be written and read back, then the mask
//                  is set to a "1". Colwersely, if a bit is read/write, then the corresponding
//                  mask bit is set to "0". For example, assume bits 6 & 7 of a register can't be
//                  written, then the corresponding mask would be "C0". Note that this field is
//                  NOT a "compatibility" mask, it is simply a read/write capability mask used
//                  only if there is a compatibility failure earlier in the test.
//
//      Example file:
//          t0901.dat                                       (not in the file)
//              Column  012345678901234567890123456789      (not in the file)
//                      3D4 0 00
//                      3DA 0 C0
//
//      Summary of above example file:
//
//          Port 3D4h, the CRTC index, is completely read/write - no reserved bits.
//              (The IBM VGA would have a mask of 40h)
//          Port 3DAh, the Feature Control register, only reserves bits 6 & 7.
//              (The IBM VGA would have a mask of C4h)
//
//
int IORWTest (void)
{
    static IOTABLE iot[] = {
        {DAC_MASK, DAC_MASK, FALSE, 0x00, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {DAC_WINDEX, DAC_WINDEX, FALSE, 0x00, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {SEQ_INDEX, SEQ_INDEX, FALSE, 0x00, 0x07, 0xF8, 0x00, FALSE, 0x00, 0x00},
        {SEQ_DATA, SEQ_DATA, TRUE, 0x00, 0x03, 0xFC, 0x00, FALSE, 0x00, 0x00},
        {SEQ_DATA, SEQ_DATA, TRUE, 0x01, 0x3D, 0xC2, 0x00, FALSE, 0x00, 0x00},
        {SEQ_DATA, SEQ_DATA, TRUE, 0x02, 0x0F, 0xF0, 0x00, FALSE, 0x00, 0x00},
        {SEQ_DATA, SEQ_DATA, TRUE, 0x03, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {SEQ_DATA, SEQ_DATA, TRUE, 0x04, 0x0E, 0xF1, 0x00, FALSE, 0x00, 0x00},
//      {CRTC_CINDEX, CRTC_CINDEX, FALSE, 0x00, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CINDEX, CRTC_CINDEX, FALSE, 0x00, 0xBF, 0x40, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x00, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x01, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x02, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x03, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x04, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x05, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x06, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x07, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x08, 0x7F, 0x80, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x09, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x0A, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x0B, 0x7F, 0x80, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x0C, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x0D, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x0E, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x0F, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x10, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x11, 0xCF, 0x00, 0x00, FALSE, 0x00, 0x00},      // Don't test interrupt bits here
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x12, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x13, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x14, 0x7F, 0x80, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x15, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x16, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x17, 0xEF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {CRTC_CDATA, CRTC_CDATA, TRUE, 0x18, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {GDC_INDEX, GDC_INDEX, FALSE, 0x00, 0x0F, 0xF0, 0x00, FALSE, 0x00, 0x00},
        {GDC_DATA, GDC_DATA, TRUE, 0x00, 0x0F, 0xF0, 0x00, FALSE, 0x00, 0x00},
        {GDC_DATA, GDC_DATA, TRUE, 0x01, 0x0F, 0xF0, 0x00, FALSE, 0x00, 0x00},
        {GDC_DATA, GDC_DATA, TRUE, 0x02, 0x0F, 0xF0, 0x00, FALSE, 0x00, 0x00},
        {GDC_DATA, GDC_DATA, TRUE, 0x03, 0x1F, 0xE0, 0x00, FALSE, 0x00, 0x00},
//      {GDC_DATA, GDC_DATA, TRUE, 0x04, 0x03, 0xFC, 0x00, FALSE, 0x00, 0x00},
        {GDC_DATA, GDC_DATA, TRUE, 0x04, 0xF3, 0x0C, 0x00, FALSE, 0x00, 0x00},
//      {GDC_DATA, GDC_DATA, TRUE, 0x05, 0x7B, 0x84, 0x00, FALSE, 0x00, 0x00},
        {GDC_DATA, GDC_DATA, TRUE, 0x05, 0xFB, 0x04, 0x00, FALSE, 0x00, 0x00},
        {GDC_DATA, GDC_DATA, TRUE, 0x06, 0x0F, 0xF0, 0x00, FALSE, 0x00, 0x00},
        {GDC_DATA, GDC_DATA, TRUE, 0x07, 0x0F, 0xF0, 0x00, FALSE, 0x00, 0x00},
        {GDC_DATA, GDC_DATA, TRUE, 0x08, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_INDEX, FALSE, 0x00, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x00, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x01, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x02, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x03, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x04, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x05, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x06, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x07, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x08, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x09, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x0A, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x0B, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x0C, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x0D, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x0E, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x0F, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x10, 0xEF, 0x10, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x11, 0xFF, 0x00, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x12, 0x3F, 0xC0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x13, 0x0F, 0xF0, 0x00, FALSE, 0x00, 0x00},
        {ATC_INDEX, ATC_RDATA, TRUE, 0x34, 0x0F, 0xF0, 0x00, FALSE, 0x00, 0x00},
        {MISC_OUTPUT, MISC_INPUT, FALSE, 0x00, 0xEF, 0x10, 0x00, FALSE, 0x00, 0x00},
        {FEAT_CWCONTROL, FEAT_RCONTROL, FALSE, 0x00, 0x3B, 0xC4, 0x00, FALSE, 0x00, 0x00}
    };
    static char szErrorMsg[64];
    int             nErr, i, j, idx, nTableSize;
    WORD            wIOWrt, wIORd;
    BOOL            bFirst;
    BYTE            temp;
    IOEXCEPTIONS    ioex[sizeof (iot) / sizeof (IOTABLE)];
    int             nExceptions, n1, n2, n3;
    FILE            *hFile;
    char            szBuffer[512];
    char            szFilename[300] = "t0901.dat";

    nTableSize = sizeof (iot) / sizeof (IOTABLE);
    nErr = ERROR_NONE;

    SimSetState (TRUE, TRUE, FALSE);            // Ignore DAC writes
    SetMode (0x92);                             // Memory doesn't matter
    SimSetState (TRUE, TRUE, TRUE);

    // See if the file exists
    GetDataFilename (szFilename, sizeof (szFilename));
    hFile = fopen (szFilename, "r");
    if (hFile == NULL)
    {
        sprintf (szBuffer, "Exception file \"%s\" not found.\n", szFilename);
    }
    else
    {
        sprintf (szBuffer, "Exception file \"%s\" found.\n", szFilename);
        fclose (hFile);
    }
    SimComment (szBuffer);
    printf ("%s\n", szBuffer);

    // Unlock the CRTC and blank the screen
    IOByteWrite (CRTC_CINDEX, 0x11);
    IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) & 0x7F));
    IOByteWrite (SEQ_INDEX, 0x01);
    IOByteWrite (SEQ_DATA, (BYTE) (IOByteRead (SEQ_DATA) | 0x20));

    SimDumpMemory ("T0901.VGA");

    sprintf (szBuffer, "IORWTest - Testing %d I/O locations.", nTableSize);
    SimComment (szBuffer);
    for (i = 0; i < nTableSize; i++)
    {
        wIOWrt = iot[i].wport;
        wIORd = iot[i].rport;

        if (wIOWrt == ATC_INDEX)
        {
            ClearIObitDataBus ();               // Set index state
            if (iot[i].fIndexed)
            {
                IOByteWrite (wIOWrt, iot[i].idx);
                ClearIObitDataBus ();           // Set back to index state
            }
        }
        else if (iot[i].fIndexed)
        {
            IOByteWrite (wIOWrt - 1, iot[i].idx);
        }

        if ((temp = IsIObitFunctional (wIORd, wIOWrt, (BYTE) ~iot[i].mask)) != 0x00)
        {
            iot[i].bError = TRUE;
            iot[i].byExp = 0x00;
            iot[i].byAct = temp;
            nErr = ERROR_IOFAILURE;
        }

        if (iot[i].rsvdmask)
        {
            if (wIOWrt == ATC_INDEX) ClearIObitDataBus ();
            if ((temp = IsIObitFunctional (wIORd, wIOWrt, (BYTE) ~(iot[i].rsvdmask | iot[i].mask))) != iot[i].rsvdmask)
            {
                iot[i].bError = TRUE;
                iot[i].byExp = iot[i].rsvdmask;
                iot[i].byAct = temp;
                nErr = ERROR_IOFAILURE;
            }
            else
            {
                temp = IOByteRead (wIORd) & iot[i].rsvdmask;
                if (temp != iot[i].rsvddata)
                {
                    iot[i].bError = TRUE;
                    iot[i].byExp = iot[i].rsvdmask;
                    iot[i].byAct = temp;
                    nErr = ERROR_IOFAILURE;
                }
            }
        }
    }

    SystemCleanUp ();

    // If there were any errors display them now.
    if (nErr != ERROR_NONE)
    {
        // Get exception list
        nErr = ERROR_NONE;              // Assume no problems
        nExceptions = 0;
        memset (ioex, 0, sizeof (ioex));
        hFile = fopen (szFilename, "r");
        if (hFile == NULL)
        {
            sprintf (szBuffer, "Exception file \"%s\" not found - assuming all I/O R/W errors are real errors\n", szFilename);
            SimComment (szBuffer);
        }
        else
        {
            sprintf (szBuffer, "Opened %s - reading exceptions:\n", szFilename);
            SimComment (szBuffer);
            // Note: Can't use "sprintf (szBuffer...)" technique in this loop because the "fgets" uses the same temporary buffer ("szBuffer").
            while (fgets (szBuffer, sizeof (szBuffer), hFile) != NULL)
            {
                sscanf (szBuffer, "%x %x %x", &n1, &n2, &n3);
                ioex[nExceptions].wport = n1;
                ioex[nExceptions].idx = n2;
                ioex[nExceptions].mask = n3;
//              printf ("Exception: %Xh %Xh %Xh\n", ioex[nExceptions].wport, ioex[nExceptions].idx, ioex[nExceptions].mask);
                if (++nExceptions >= (int) (sizeof (iot) / sizeof (IOTABLE)))
                {
                    break;
                }
            }
            fclose (hFile);
        }
        sprintf (szBuffer, "Number of exceptions: %d\n", nExceptions);
        SimComment (szBuffer);

        // Display the errors
        for (i = 0; i < nTableSize; i++)
        {
            if (iot[i].bError)
            {
                // Determine if an exception exists
                for (j = 0; j < nExceptions; j++)
                {
                    if ((iot[i].wport == ioex[j].wport) && (iot[i].idx == ioex[j].idx))
                    {
                        // If the exception mask and the actual mask match, then flag this as no error
                        if (iot[i].byAct == ioex[j].mask)
                            iot[i].bError = FALSE;
                        break;
                    }
                }

                // Skip if there no longer is any error
                if (!iot[i].bError)
                    continue;

                // Display raw error
                nErr = ERROR_IOFAILURE;             // Assumption of no error above was, er, in error
                if (iot[i].fIndexed)
                {
                    printf ("%04X[%02X] failed (exp=%02Xh, act=%02Xh)\n", iot[i].wport, iot[i].idx, iot[i].byExp, iot[i].byAct);
                }
                else
                {
                    printf ("Write Address = %04X; Read Address = %04X failed (exp=%02Xh, act=%02Xh)\n", iot[i].wport, iot[i].rport, iot[i].byExp, iot[i].byAct);
                }

                // Display a nice "user friendly" message that lists the
                // non-standard bits for the user
                temp = iot[i].byExp ^ iot[i].byAct;
                szErrorMsg[0] = '\0';
                bFirst = TRUE;
                idx = 0;
                for (j = 0; j < 8; j++)
                {
                    if ((temp & 0x01) == 0x01)
                    {
                        if (!bFirst)
                        {
                            strcat (szErrorMsg, ", ");
                            idx += 2;
                        }
                        szErrorMsg[idx++] = j | 0x30;
                        szErrorMsg[idx] = '\0';
                        bFirst = FALSE;
                    }
                    temp = temp >> 1;
                }
                printf ("   (Non-standard bit(s): %s)\n", szErrorMsg);
            }
        }
    }

    if (nErr == ERROR_NONE)
        SimComment ("I/O R/W Test passed.");
    else
        SimComment ("I/O R/W Test failed.");
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        2
//
//      T0902
//      ByteModeTest - Load a pattern and set each of BYTE mode, WORD mode, and DWORD mode.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int ByteModeTest (void)
{
    int         nErr;
    SEGOFF      lpVideo;
    DWORD       offset, dwLength;
    BOOL        bFullVGA;

    nErr = ERROR_NONE;
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto ByteModeTest_exit;

    if (bFullVGA)
    {
        dwLength = 0x10000;
    }
    else
    {
        dwLength = 0x2000;
    }

    SimSetState (TRUE, TRUE, FALSE);                // Ignore DAC writes
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);

    lpVideo = (SEGOFF) 0xA0000000;
    SimDumpMemory ("T0902.VGA");

    // Fill memory with an RGB pattern
    SimComment ("Fill memory with an RGB pattern.");
    for (offset = 0; offset < dwLength; offset += 4)
    {
        IOWordWrite (SEQ_INDEX, 0x0102);
        MemByteWrite (lpVideo++, 0xFF);
        IOWordWrite (SEQ_INDEX, 0x0202);
        MemByteWrite (lpVideo++, 0xFF);
        IOWordWrite (SEQ_INDEX, 0x0402);
        MemByteWrite (lpVideo++, 0xFF);
        IOWordWrite (SEQ_INDEX, 0x0F02);
        MemByteWrite (lpVideo++, 0xFF);
    }

    // Verify BYTE mode
    SimComment ("Verify BYTE mode.");
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto ByteModeTest_exit;
        }
    }

    // Set WORD mode and observe every other byte is skipped
    SimComment ("Set WORD mode and observe every other byte is skipped.");
    IOWordWrite (CRTC_CINDEX, 0xA317);
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto ByteModeTest_exit;
        }
    }

    // Set DWORD mode and observe every fourth byte is skipped (note that
    // WORD mode is still set)
    SimComment ("Set DWORD mode and observe every fourth byte is skipped (note that WORD mode is still set).");
    IOWordWrite (CRTC_CINDEX, 0x4014);
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto ByteModeTest_exit;
        }
    }

    // Verify that BYTE/WORD mode bit has no effect on DWORD mode bit by
    // setting back to BYTE mode without changing from DWORD mode.
    SimComment ("Verify that BYTE/WORD mode bit has no effect on DWORD mode bit by setting back to BYTE mode without changing from DWORD mode.");
    IOWordWrite (CRTC_CINDEX, 0xE317);
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

ByteModeTest_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Byte Mode Test passed.");
    else
        SimComment ("Byte Mode Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        3
//
//      T0903
//      Chain2Chain4Test - Fill memory in each of chain/2 and chain/4 modes and read back specific locations
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int Chain2Chain4Test (void)
{
    int         nErr, i;
    SEGOFF      lpVideo;
    WORD        count, wOffset;
    WORD        wSimType;
    BYTE        temp;
    BOOL        bFullVGA;
    DWORD       dwMemSize;

    nErr = ERROR_NONE;
    wSimType = SimGetType ();
    bFullVGA = SimGetFrameSize () && (!(wSimType & SIM_SIMULATION));
    lpVideo = (SEGOFF) 0xA0000000;

    SimSetState (TRUE, TRUE, FALSE);                // Ignore DAC writes
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);

    if (bFullVGA)
    {
        dwMemSize = 0x800;
    }
    else
    {
        dwMemSize = 0x80;
    }

    SimDumpMemory ("T0903.VGA");

    // Set chain/2, odd/even mode and fill some of memory
    SimComment ("Set chain/2, odd/even mode and fill some of memory.");
    IOWordWrite (GDC_INDEX, 0x0706);                // Chain/2
    IOWordWrite (SEQ_INDEX, 0x0204);                // Odd/even mode
    IOWordWrite (SEQ_INDEX, 0x0302);                // Enable plane 0 / plane 1
    for (count = 0; count < dwMemSize; count++)
    {
        MemByteWrite (lpVideo++, '0');
        MemByteWrite (lpVideo++, '1');
    }

    // Set back to planar mode and verify memory
    SimComment ("Set back to planar mode and verify memory.");
    lpVideo = (SEGOFF) 0xA0000000;
    IOWordWrite (GDC_INDEX, 0x0506);                // Not chain/2
    IOWordWrite (SEQ_INDEX, 0x0604);                // Not odd/even
    IOWordWrite (GDC_INDEX, 0x0004);                // Read map 0
    for (count = 0; count < dwMemSize; count++)
    {
        temp = MemByteRead (lpVideo);
        if (temp != '0')
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (lpVideo), HIWORD (lpVideo), '0', temp);
            goto Chain2Chain4Test_exit;
        }
        lpVideo += 2;
    }
    lpVideo = (SEGOFF) 0xA0000000;
    IOWordWrite (GDC_INDEX, 0x0104);                // Read map 1
    for (count = 0; count < dwMemSize; count++)
    {
        temp = MemByteRead (lpVideo);
        if (temp != '1')
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (lpVideo), HIWORD (lpVideo), '1', temp);
            goto Chain2Chain4Test_exit;
        }
        lpVideo += 2;
    }

    // Clear memory
    SimComment ("Clear memory.");
    SimSetState (TRUE, TRUE, FALSE);
    SetMode (0x12);

    // Set chain/4 mode and fill memory
    SimComment ("Set chain/4 mode and fill memory.");
    lpVideo = (SEGOFF) 0xA0000000;
    IOWordWrite (SEQ_INDEX, 0x0E04);                // Chain/4 mode
    IOWordWrite (SEQ_INDEX, 0x0F02);                // Enable planes 0..3
    for (count = 0; count < dwMemSize / 2; count++)
    {
        MemByteWrite (lpVideo++, '0');
        MemByteWrite (lpVideo++, '1');
        MemByteWrite (lpVideo++, '2');
        MemByteWrite (lpVideo++, '3');
    }
    // Set back to planar mode and verify memory
    SimComment ("Set back to planar mode and verify memory.");
    lpVideo = (SEGOFF) 0xA0000000;
    wOffset = 0;
    IOWordWrite (SEQ_INDEX, 0x0604);                // Not odd/even
    for (i = 0; i < 4; i++)
    {
        IOByteWrite (GDC_INDEX, 0x04);          // Read map
        IOByteWrite (GDC_DATA, (BYTE) i);
        for (count = 0; count < dwMemSize / 2; count++)
        {
            wOffset = count*4 + ((count & 0x3000) >> 12);
            temp = MemByteRead (lpVideo + wOffset);
            if (temp != (BYTE) ('0' + i))
            {
                nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, wOffset, HIWORD (lpVideo), '0' + i, temp);
                goto Chain2Chain4Test_exit;
            }
        }
    }

Chain2Chain4Test_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Chain/2 Chain/4 Test passed.");
    else
        SimComment ("Chain/2 Chain/4 Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        4
//
//      T0904
//      CGAHerlwlesTest - Test compatibility address line bits in CRTC[17]
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int CGAHerlwlesTest (void)
{
    int         nErr, i;
    SEGOFF      lpVideo;
    DWORD       offset, dwLength1, dwLength2;
    BOOL        bFullVGA;

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xA0000000;
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto CGAHerlwlesTest_exit;

    if (bFullVGA)
    {
        dwLength1 = 0x10000;
        dwLength2 = 0x2000;
    }
    else
    {
        dwLength1 = 0x2000;
        dwLength2 = 500;
    }

    SimSetState (TRUE, TRUE, FALSE);            // Ignore DAC writes
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);

    SimDumpMemory ("T0904.VGA");

    // Fill memory with a known pattern
    SimComment ("Fill memory with a known pattern.");
    for (offset = 0; offset < dwLength1; offset += 4)
    {
        IOWordWrite (SEQ_INDEX, 0x0102);
        MemByteWrite (lpVideo++, 0xFF);
        IOWordWrite (SEQ_INDEX, 0x0202);
        MemByteWrite (lpVideo++, 0xFF);
        IOWordWrite (SEQ_INDEX, 0x0402);
        MemByteWrite (lpVideo++, 0xFF);
        IOWordWrite (SEQ_INDEX, 0x0F02);
        MemByteWrite (lpVideo++, 0xFF);
    }

    // Verify bit 5 has no effect in BYTE mode comparing this screen to the
    // next one. They should be the same.
    SimComment ("Verify bit 5 has no effect in BYTE mode comparing this screen to the next one. They should be the same.");
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto CGAHerlwlesTest_exit;
        }
    }

    IOWordWrite (CRTC_CINDEX, 0xC317);          // Clear CRTC[17].5
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto CGAHerlwlesTest_exit;
        }
    }

    // Verify bit 5 has no effect in DWORD mode comparing this screen to the
    // next one. They should be the same.
    SimComment ("Verify bit 5 has no effect in DWORD mode comparing this screen to the next one. They should be the same.");
    IOWordWrite (CRTC_CINDEX, 0x4014);
    IOWordWrite (CRTC_CINDEX, 0xE317);
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto CGAHerlwlesTest_exit;
        }
    }

    SimComment ("Clear CRTC[17].5.");
    IOWordWrite (CRTC_CINDEX, 0xC317);          // Clear CRTC[17].5
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto CGAHerlwlesTest_exit;
        }
    }

    // Set to WORD mode, "normal" addressing
    SimComment ("Set to WORD mode, \"normal\" addressing");
    IOWordWrite (CRTC_CINDEX, 0x0014);
    IOWordWrite (CRTC_CINDEX, 0xA317);
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto CGAHerlwlesTest_exit;
        }
    }

    // Now set bit 5 again in WORD mode. MA13 replaces LA0 instead of MA15.
    SimComment ("Now set bit 5 again in WORD mode. MA13 replaces LA0 instead of MA15.");
    IOWordWrite (CRTC_CINDEX, 0x8317);
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto CGAHerlwlesTest_exit;
        }
    }

    // Verify CGA compatible addressing. Fill memory with a block of data
    // for the first 2000h bytes.
    SimComment ("Verify CGA compatible addressing. Fill memory with a block of data for the first 2000h bytes.");
    lpVideo = (SEGOFF) 0xA0000000;
    SimSetState (TRUE, TRUE, FALSE);
    SetMode (0x12);
    for (i = 0; i < (int) dwLength2; i++)
        MemByteWrite (lpVideo++, 0xFF);

    if (!bFullVGA)
    {
        IOWordWrite (CRTC_CINDEX, 0x200C);
        WaitVerticalRetrace ();                     // Let frame settle
    }

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto CGAHerlwlesTest_exit;
        }
    }

    // Clear CRTC[0] and verify that the first 2000h bytes are "duplicated"
    // on the display.
    SimComment ("Clear CRTC[0] and verify that the first 2000h bytes are \"duplicated\" on the display.");
    IOWordWrite (CRTC_CINDEX, 0xE217);
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto CGAHerlwlesTest_exit;
        }
    }

    // Set DWORD mode, normal addressing
    SimComment ("Set DWORD mode, normal addressing.");

    if (!bFullVGA)
    {
        IOWordWrite (CRTC_CINDEX, 0x080C);
        WaitVerticalRetrace ();                     // Let frame settle
    }

    IOWordWrite (CRTC_CINDEX, 0x4014);
    IOWordWrite (CRTC_CINDEX, 0xE317);
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto CGAHerlwlesTest_exit;
        }
    }

    // Set DWORD mode, CGA addressing
    SimComment ("Set DWORD mode, CGA addressing.");
    IOWordWrite (CRTC_CINDEX, 0xE217);
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

CGAHerlwlesTest_exit:
    if (nErr == ERROR_NONE)
        SimComment ("CGA / Herlwles Test passed.");
    else
        SimComment ("CGA / Herlwles Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        5
//
//      T0905
//      LatchTest - Write all possible data value and copy each to another video memory location using the latches
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int LatchTest (void)
{
    static BYTE     byPattern[4][4] = {
                        {0x5A, 0x23, 0xA1, 0xFF},
                        {0x15, 0x51, 0x8A, 0xA8},
                        {0x69, 0xF0, 0xCA, 0x11},
                        {0x01, 0xEF, 0x90, 0x77}
                };
    int     nErr;
    SEGOFF  lpVideo;
    BYTE    temp;
    WORD    i, j, wXRes, wYRes;

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xA0000000;

    SimSetState (TRUE, TRUE, FALSE);                // Ignore DAC writes
    SetMode (0x12);
    GetResolution (&wXRes, &wYRes);

    SimDumpMemory ("T0905.VGA");

    // Fill memory with a pattern
    SimComment ("Fill memory with a pattern.");
    for (i = 0; i < 4; i++)
    {
        IOByteWrite (SEQ_INDEX, 0x02);
        IOByteWrite (SEQ_DATA, (BYTE) (1 << i));
        for (j = 0; j < (WORD) (wXRes/8)*(wYRes); j++)
            MemByteWrite (lpVideo + j, byPattern[j % 4][i]);
    }

    SimComment ("Test the latches at each memory location.");
    IOWordWrite (SEQ_INDEX, 0x0F02);                    // Enable all planes
    IOWordWrite (GDC_INDEX, 0x0105);                    // Write mode one
    for (j = 0; j < (WORD) (wXRes/8)*(wYRes); j++)
    {
        MemByteRead (lpVideo + j);                      // Load latches
        MemByteWrite (lpVideo + 0xFFFF, 0);             // Write latches
        for (i = 0; i < 4; i++)
        {
            IOByteWrite (GDC_INDEX, 0x04);
            IOByteWrite (GDC_DATA, (BYTE) i);           // Read map select
            temp = MemByteRead (lpVideo + 0xFFFF);
            if (temp != byPattern[j % 4][i])
            {
                nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, j, HIWORD (lpVideo), byPattern[j % 4][i], temp);
                goto LatchTest_exit;
            }
        }
    }

LatchTest_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Latch Test passed.");
    else
        SimComment ("Latch Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        6
//
//      T0906
//      Mode0Test - Verify memory is read/writable and test each mode 0, 0+, and 0*.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int Mode0Test (void)
{
    int             nErr, i, j, k, nLength;
    SEGOFF          lpVideo, lpTemp;
    BYTE            tblTest[] = {0x00, 0x55, 0xAA, 0xFF};
    WORD            tblScan[] = {200, 350, 400};
    BYTE            chr, attr;
    WORD            wSimType;
    int             nRow, nRowCount, nCol, nColCount;
    BOOL            bFullVGA;
    char            szBuffer[256];

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xB8000000;
    nLength = 0x8000;
    wSimType = SimGetType ();
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto Mode0Test_exit;

    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    if (bFullVGA)
    {
        nRow = 6;
        nRowCount = 8;
        nCol = 4;
        nColCount = 32;
    }
    else
    {
        nRow = 0;
        nRowCount = 1;
        nCol = 0;
        nColCount = 40;
    }

    // Do the memory test only for physical devices
    if (!((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION)))
    {
        SetMode (0x00);
        SimSetState (TRUE, TRUE, TRUE);

        for (i = 0; i < (int) sizeof tblTest; i++)
        {
            for (j = 0; (WORD) j < (WORD) nLength; j++)
                MemByteWrite (lpVideo + j, tblTest[i]);

            lpTemp = VerifyBytes (lpVideo, tblTest[i], nLength);
            if (lpTemp != (SEGOFF) NULL)
            {
                nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                                    LOWORD (lpTemp), HIWORD (lpTemp), tblTest[i],
                                    MemByteRead (lpTemp));
                goto Mode0Test_exit;
            }
        }
    }

    for (k = 0; k < (int) (sizeof (tblScan) / sizeof (WORD)); k++)
    {
        sprintf (szBuffer, "Set Mode 0 at %d scan lines.", tblScan[k]);
        SimComment (szBuffer);
        SetScans (tblScan[k]);
        SimSetState (TRUE, TRUE, FALSE);
        SetMode (0x00);
        if (k == 0) SimDumpMemory ("T0906.VGA");
        SimSetState (TRUE, TRUE, TRUE);

        SimComment ("Load a pattern into memory.");
        chr = attr = 0;
        for (i = nRow; i < (nRow + nRowCount); i++)
        {
            for (j = nCol; j < (nCol + nColCount); j++)
            {
                TextCharOut (chr, attr, (BYTE) j, (BYTE) i, 0);
                chr++; attr++;
            }
        }

        // Bright white overscan
        SimComment ("Turn on overscan.");
        IOByteRead (INPUT_CSTATUS_1);
        IOByteWrite (ATC_INDEX, 0x31);
        IOByteWrite (ATC_INDEX, 0x3F);

        WaitAttrBlink (BLINK_OFF);
        WaitLwrsorBlink (BLINK_ON);
        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto Mode0Test_exit;
            }
        }
    }

Mode0Test_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Mode 0 Test passed.");
    else
        SimComment ("Mode 0 Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        7
//
//      T0907
//      Mode3Test - Verify memory is read/writable and test each mode 3, 3+, and 3*.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int Mode3Test (void)
{
    int         nErr, i, j, k, nLength;
    SEGOFF      lpVideo, lpTemp;
    BYTE        tblTest[] = {0x00, 0x55, 0xAA, 0xFF};
    WORD        tblScan[] = {200, 350, 400};
    BYTE        chr, attr;
    WORD        wSimType;
    int         nRow, nRowCount, nCol, nColCount;
    BOOL        bFullVGA;
    char        szBuffer[256];

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xB8000000;
    nLength = 0x8000;
    wSimType = SimGetType ();
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto Mode3Test_exit;

    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    if (bFullVGA)
    {
        nRow = 10;
        nRowCount = 4;
        nCol = 8;
        nColCount = 64;
    }
    else
    {
        nRow = 0;
        nRowCount = 1;
        nCol = 0;
        nColCount = 40;
    }

    // Do the memory test only for physical devices
    if (!((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION)))
    {
        SetMode (0x03);
        SimSetState (TRUE, TRUE, TRUE);

        for (i = 0; i < (int) sizeof tblTest; i++)
        {
            for (j = 0; (WORD) j < (WORD) nLength; j++)
                MemByteWrite (lpVideo + j, tblTest[i]);

            lpTemp = VerifyBytes (lpVideo, tblTest[i], nLength);
            if (lpTemp != (SEGOFF) NULL)
            {
                nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                                    LOWORD (lpTemp), HIWORD (lpTemp), tblTest[i],
                                    MemByteRead (lpTemp));
                goto Mode3Test_exit;
            }
        }
    }

    for (k = 0; k < (int) (sizeof (tblScan) / sizeof (WORD)); k++)
    {
        sprintf (szBuffer, "Set Mode 3 at %d scan lines.", tblScan[k]);
        SimComment (szBuffer);
        SetScans (tblScan[k]);
        SimSetState (TRUE, TRUE, FALSE);
        SetMode (0x03);
        if (k == 0) SimDumpMemory ("T0907.VGA");
        SimSetState (TRUE, TRUE, TRUE);

        SimComment ("Load a pattern into memory.");
        chr = attr = 0;
        for (i = nRow; i < (nRow + nRowCount); i++)
        {
            for (j = nCol; j < (nCol + nColCount); j++)
            {
                TextCharOut (chr, attr, (BYTE) j, (BYTE) i, 0);
                chr++; attr++;
            }
        }

        // Bright white overscan
        SimComment ("Turn on overscan.");
        IOByteRead (INPUT_CSTATUS_1);
        IOByteWrite (ATC_INDEX, 0x31);
        IOByteWrite (ATC_INDEX, 0x3F);

        WaitAttrBlink (BLINK_OFF);
        WaitLwrsorBlink (BLINK_ON);
        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto Mode3Test_exit;
            }
        }
    }

Mode3Test_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Mode 3 Test passed.");
    else
        SimComment ("Mode 3 Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        8
//
//      T0908
//      Mode5Test - Verify memory is read/writable and test mode 5.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int Mode5Test (void)
{
    int         nErr, i, j, nLength;
    SEGOFF      lpVideo, lpTemp;
    BYTE        tblTest[] = {0x00, 0x55, 0xAA, 0xFF};
    BYTE        chr, attr;
    WORD        wSimType;
    int         nRow, nRowCount, nCol, nColCount;
    BOOL        bFullVGA;

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xB8000000;
    nLength = 0x8000;
    wSimType = SimGetType ();
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto Mode5Test_exit;

    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    if (bFullVGA)
    {
        nRow = 6;
        nRowCount = 8;
        nCol = 4;
        nColCount = 32;
    }
    else
    {
        nRow = 0;
        nRowCount = 1;
        nCol = 0;
        nColCount = 40;
    }

    // Do the memory test only for physical devices
    if (!((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION)))
    {
        SetMode (0x05);
        SimSetState (TRUE, TRUE, TRUE);

        for (i = 0; i < (int) sizeof tblTest; i++)
        {
            for (j = 0; (WORD) j < (WORD) nLength; j++)
                MemByteWrite (lpVideo + j, tblTest[i]);

            lpTemp = VerifyBytes (lpVideo, tblTest[i], nLength);
            if (lpTemp != (SEGOFF) NULL)
            {
                nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                                    LOWORD (lpTemp), HIWORD (lpTemp), tblTest[i],
                                    MemByteRead (lpTemp));
                goto Mode5Test_exit;
            }
        }
    }

    SimComment ("Set mode 5");
    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    SetMode (0x05);
    SimSetState (TRUE, TRUE, TRUE);
    SimDumpMemory ("T0908.VGA");

    SimComment ("Load a pattern into memory.");
    chr = attr = 0;
    for (i = nRow; i < (nRow + nRowCount); i++)
    {
        for (j = nCol; j < (nCol + nColCount); j++)
        {
            CGA4CharOut (chr, attr, (BYTE) j, (BYTE) i, 0);
            chr++; attr++;
        }
    }

    // Bright white overscan
    SimComment ("Turn on overscan.");
    IOByteRead (INPUT_CSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x31);
    IOByteWrite (ATC_INDEX, 0x17);

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

Mode5Test_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Mode 5 Test passed.");
    else
        SimComment ("Mode 5 Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        9
//
//      T0909
//      Mode6Test - Verify memory is read/writable and test mode 6.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int Mode6Test (void)
{
    int         nErr, i, j, nLength;
    SEGOFF      lpVideo, lpTemp;
    BYTE        tblTest[] = {0x00, 0x55, 0xAA, 0xFF};
    BYTE        chr, attr;
    WORD        wSimType;
    int         nRow, nRowCount, nCol, nColCount;
    BOOL        bFullVGA;

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xB8000000;
    nLength = 0x8000;
    wSimType = SimGetType ();
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto Mode6Test_exit;

    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    if (bFullVGA)
    {
        nRow = 10;
        nRowCount = 4;
        nCol = 8;
        nColCount = 64;
    }
    else
    {
        nRow = 0;
        nRowCount = 1;
        nCol = 0;
        nColCount = 40;
    }

    // Do the memory test only for physical devices
    if (!((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION)))
    {
        SetMode (0x06);
        SimSetState (TRUE, TRUE, TRUE);

        for (i = 0; i < (int) sizeof tblTest; i++)
        {
            for (j = 0; (WORD) j < (WORD) nLength; j++)
                MemByteWrite (lpVideo + j, tblTest[i]);

            lpTemp = VerifyBytes (lpVideo, tblTest[i], nLength);
            if (lpTemp != (SEGOFF) NULL)
            {
                nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                                    LOWORD (lpTemp), HIWORD (lpTemp), tblTest[i],
                                    MemByteRead (lpTemp));
                goto Mode6Test_exit;
            }
        }
    }

    SimComment ("Set mode 6.");
    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    SetMode (0x06);
    SimDumpMemory ("T0909.VGA");
    SimSetState (TRUE, TRUE, TRUE);

    SimComment ("Load a pattern into memory.");
    chr = attr = 0;
    for (i = nRow; i < (nRow + nRowCount); i++)
    {
        for (j = nCol; j < (nCol + nColCount); j++)
        {
            CGA2CharOut (chr, attr, (BYTE) j, (BYTE) i, 0);
            chr++; attr++;
        }
    }

    // Bright white overscan
    SimComment ("Turn on overscan.");
    IOByteRead (INPUT_CSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x31);
    IOByteWrite (ATC_INDEX, 0x17);

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

Mode6Test_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Mode 6 Test passed.");
    else
        SimComment ("Mode 6 Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        10
//
//      T0710
//      Mode7Test - Verify memory is read/writable and test each mode 7+, and 7*.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int Mode7Test (void)
{
    int         nErr, i, j, k, nLength;
    SEGOFF      lpVideo, lpTemp;
    BYTE        tblTest[] = {0x00, 0x55, 0xAA, 0xFF};
    WORD        tblScan[] = {350, 400};
    BYTE        chr, attr;
    WORD        wSimType;
    int         nRow, nRowCount, nCol, nColCount;
    BOOL        bFullVGA;
    char        szBuffer[256];

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xB0000000;
    nLength = 0x8000;
    wSimType = SimGetType ();
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto Mode7Test_exit;

    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    if (bFullVGA)
    {
        nRow = 10;
        nRowCount = 4;
        nCol = 8;
        nColCount = 64;
    }
    else
    {
        nRow = 0;
        nRowCount = 1;
        nCol = 0;
        nColCount = 40;
    }

    // Do the memory test only for physical devices
    if (!((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION)))
    {
        SetMode (0x07);
        SimSetState (TRUE, TRUE, TRUE);

        for (i = 0; i < (int) sizeof tblTest; i++)
        {
            for (j = 0; (WORD) j < (WORD) nLength; j++)
                MemByteWrite (lpVideo + j, tblTest[i]);

            lpTemp = VerifyBytes (lpVideo, tblTest[i], nLength);
            if (lpTemp != (SEGOFF) NULL)
            {
                nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                                    LOWORD (lpTemp), HIWORD (lpTemp), tblTest[i],
                                    MemByteRead (lpTemp));
                goto Mode7Test_exit;
            }
        }
    }

    for (k = 0; k < (int) (sizeof (tblScan) / sizeof (WORD)); k++)
    {
        sprintf (szBuffer, "Set mode 7 at %d scan lines.", tblScan[k]);
        SimComment (szBuffer);
        SetScans (tblScan[k]);
        SimSetState (TRUE, TRUE, FALSE);            // Ignore DAC writes
        SetMode (0x07);
        if (k == 0) SimDumpMemory ("T0910.VGA");
        SimSetState (TRUE, TRUE, TRUE);

        SimComment ("Load a pattern into memory.");
        chr = attr = 0;
        for (i = nRow; i < (nRow + nRowCount); i++)
        {
            for (j = nCol; j < (nCol + nColCount); j++)
            {
                TextCharOut (chr, attr, (BYTE) j, (BYTE) i, 0);
                chr++; attr++;
            }
        }

        // Bright white overscan
        SimComment ("Turn on overscan.");
        IOByteRead (INPUT_MSTATUS_1);
        IOByteWrite (ATC_INDEX, 0x31);
        IOByteWrite (ATC_INDEX, 0x3F);

        WaitAttrBlink (BLINK_OFF);
        WaitLwrsorBlink (BLINK_ON);
        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto Mode7Test_exit;
            }
        }
    }

Mode7Test_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Mode 7 Test passed.");
    else
        SimComment ("Mode 7 Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        11
//
//      T0911
//      ModeDTest - Verify memory is read/writable and test mode D.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int ModeDTest (void)
{
    int         nErr, i, j, nLength;
    SEGOFF      lpVideo, lpTemp;
    BYTE        tblTest[] = {0x00, 0x55, 0xAA, 0xFF};
    BYTE        chr, attr;
    WORD        wSimType;
    int         nRow, nRowCount, nCol, nColCount;
    BOOL        bFullVGA;

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xA0000000;
    nLength = 0xFFFF;
    wSimType = SimGetType ();
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto ModeDTest_exit;

    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    if (bFullVGA)
    {
        nRow = 6;
        nRowCount = 8;
        nCol = 4;
        nColCount = 32;
    }
    else
    {
        nRow = 0;
        nRowCount = 1;
        nCol = 0;
        nColCount = 40;
    }

    // Do the memory test only for physical devices
    if (!((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION)))
    {
        SetMode (0x0D);
        SimSetState (TRUE, TRUE, TRUE);

        for (i = 0; i < (int) sizeof tblTest; i++)
        {
            for (j = 0; (WORD) j < (WORD) nLength; j++)
                MemByteWrite (lpVideo + j, tblTest[i]);

            for (j = 0; j < 4; j++)
            {
                IOByteWrite (GDC_INDEX, 0x04);
                IOByteWrite (GDC_DATA, (BYTE) j);
                lpTemp = VerifyBytes (lpVideo, tblTest[i], nLength);
                if (lpTemp != (SEGOFF) NULL)
                {
                    nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                                        LOWORD (lpTemp), HIWORD (lpTemp), tblTest[i],
                                        MemByteRead (lpTemp));
                    goto ModeDTest_exit;
                }
            }
        }
    }

    SimComment ("Set mode D");
    SimSetState (TRUE, TRUE, FALSE);                // Ignore DAC writes
    SetMode (0x0D);
    SimDumpMemory ("T0911.VGA");
    SimSetState (TRUE, TRUE, TRUE);

    SimComment ("Load a pattern into memory.");
    chr = attr = 0;
    for (i = nRow; i < (nRow + nRowCount); i++)
    {
        for (j = nCol; j < (nCol + nColCount); j++)
        {
            PlanarCharOut (chr, attr, (BYTE) j, (BYTE) i, 0);
            chr++; attr++;
        }
    }

    // Bright white overscan
    SimComment ("Turn on overscan.");
    IOByteRead (INPUT_CSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x31);
    IOByteWrite (ATC_INDEX, 0x3F);

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

ModeDTest_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Mode D Test passed.");
    else
        SimComment ("Mode D Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        12
//
//      T0912
//      Mode12Test - Verify memory is read/writable and test mode E/10/11/12.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int Mode12Test (void)
{
    int         nErr, i, j, k, nLength;
    SEGOFF      lpVideo, lpTemp;
    BYTE        tblTest[] = {0x00, 0x55, 0xAA, 0xFF};
    BYTE        tblModes[] = {0x0E, 0x10, 0x11, 0x12};
    BYTE        chr, attr;
    WORD        wSimType;
    int         nRow, nRowCount, nCol, nColCount;
    BOOL        bFullVGA;
    char        szBuffer[256];

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xA0000000;
    nLength = 0xFFFF;
    wSimType = SimGetType ();
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto Mode12Test_exit;

    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    if (bFullVGA)
    {
        nRow = 10;
        nRowCount = 4;
        nCol = 8;
        nColCount = 64;
    }
    else
    {
        nRow = 0;
        nRowCount = 1;
        nCol = 0;
        nColCount = 40;
    }

    // Do the memory test only for physical devices
    if (!((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION)))
    {
        SetMode (0x12);
        SimSetState (TRUE, TRUE, TRUE);

        for (i = 0; i < (int) sizeof tblTest; i++)
        {
            for (j = 0; (WORD) j < (WORD) nLength; j++)
                MemByteWrite (lpVideo + j, tblTest[i]);

            for (j = 0; j < 4; j++)
            {
                IOByteWrite (GDC_INDEX, 0x04);
                IOByteWrite (GDC_DATA, (BYTE) j);
                lpTemp = VerifyBytes (lpVideo, tblTest[i], nLength);
                if (lpTemp != (SEGOFF) NULL)
                {
                    nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                                        LOWORD (lpTemp), HIWORD (lpTemp), tblTest[i],
                                        MemByteRead (lpTemp));
                    goto Mode12Test_exit;
                }
            }
        }
    }

    for (k = 0; k < (int) sizeof (tblModes); k++)
    {
        sprintf (szBuffer, "Set mode %X", tblModes[k]);
        SimComment (szBuffer);

        SimSetState (TRUE, TRUE, FALSE);                // Ignore DAC writes
        SetMode (tblModes[k]);
        if (k == 0) SimDumpMemory ("T0912.VGA");
        SimSetState (TRUE, TRUE, TRUE);

        SimComment ("Load a pattern into memory.");
        chr = attr = 0;
        for (i = nRow; i < (nRow + nRowCount); i++)
        {
            for (j = nCol; j < (nCol + nColCount); j++)
            {
                PlanarCharOut (chr, attr, (BYTE) j, (BYTE) i, 0);
                chr++; attr++;
            }
        }

        // Bright white overscan
        SimComment ("Turn on overscan");
        IOByteRead (INPUT_CSTATUS_1);
        IOByteWrite (ATC_INDEX, 0x31);
        IOByteWrite (ATC_INDEX, 0x3F);

        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto Mode12Test_exit;
            }
        }
    }

Mode12Test_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Mode 12 Test passed.");
    else
        SimComment ("Mode 12 Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        13
//
//      T090F
//      ModeFTest - Verify memory is read/writable and test mode F.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int ModeFTest (void)
{
    int         nErr, i, j, nLength;
    SEGOFF      lpVideo, lpTemp;
    BYTE        tblTest[] = {0x00, 0x55, 0xAA, 0xFF};
    BYTE        chr, attr;
    WORD        wSimType;
    int         nRow, nRowCount, nCol, nColCount;
    BOOL        bFullVGA;

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xA0000000;
    nLength = 0xFFFF;
    wSimType = SimGetType ();
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto ModeFTest_exit;

    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    if (bFullVGA)
    {
        nRow = 10;
        nRowCount = 4;
        nCol = 8;
        nColCount = 64;
    }
    else
    {
        nRow = 0;
        nRowCount = 1;
        nCol = 0;
        nColCount = 40;
    }

    // Do the memory test only for physical devices
    if (!((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION)))
    {
        SetMode (0x0F);
        SimSetState (TRUE, TRUE, TRUE);

        for (i = 0; i < (int) sizeof tblTest; i++)
        {
            for (j = 0; (WORD) j < (WORD) nLength; j++)
                MemByteWrite (lpVideo + j, tblTest[i]);

            for (j = 0; j < 4; j++)
            {
                IOByteWrite (GDC_INDEX, 0x04);
                IOByteWrite (GDC_DATA, (BYTE) j);
                lpTemp = VerifyBytes (lpVideo, tblTest[i], nLength);
                if (lpTemp != (SEGOFF) NULL)
                {
                    nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                                        LOWORD (lpTemp), HIWORD (lpTemp), tblTest[i],
                                        MemByteRead (lpTemp));
                    goto ModeFTest_exit;
                }
            }
        }
    }

    SimComment ("Set mode F");
    SimSetState (TRUE, TRUE, FALSE);
    SetMode (0x0F);
    SimDumpMemory ("T0913.VGA");
    SimSetState (TRUE, TRUE, TRUE);

    SimComment ("Load a pattern into memory.");
    chr = attr = 0;
    for (i = nRow; i < (nRow + nRowCount); i++)
    {
        for (j = nCol; j < (nCol + nColCount); j++)
        {
            PlanarCharOut (chr, attr, (BYTE) j, (BYTE) i, 0);
            chr++; attr++;
        }
    }

    // Bright white overscan
    SimComment ("Turn on overscan.");
    IOByteRead (INPUT_MSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x31);
    IOByteWrite (ATC_INDEX, 0x3F);

    SimComment ("First frame, with blinking.");
    WaitAttrBlink (BLINK_ON);
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

    SimComment ("Second frame, with no blinking.");
    WaitAttrBlink (BLINK_OFF);
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

ModeFTest_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Mode F Test passed.");
    else
        SimComment ("Mode F Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        14
//
//      T0914
//      Mode13Test - Verify memory is read/writable and test mode 13.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int Mode13Test (void)
{
    int         nErr, i, j, nLength;
    SEGOFF      lpVideo, lpTemp;
    BYTE        tblTest[] = {0x00, 0x55, 0xAA, 0xFF};
    BYTE        chr, attr;
    WORD        wSimType;
    int         nRow, nRowCount, nCol, nColCount;
    BOOL        bFullVGA;

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xA0000000;
    nLength = 0xFFFF;
    wSimType = SimGetType ();
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto Mode13Test_exit;

    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    if (bFullVGA)
    {
        nRow = 6;
        nRowCount = 8;
        nCol = 4;
        nColCount = 32;
    }
    else
    {
        nRow = 0;
        nRowCount = 1;
        nCol = 0;
        nColCount = 20;
    }

    // Do the memory test only for physical devices
    if (!((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION)))
    {
        SetMode (0x13);
        SimSetState (TRUE, TRUE, TRUE);

        for (i = 0; i < (int) sizeof tblTest; i++)
        {
            for (j = 0; (WORD) j < (WORD) nLength; j++)
                MemByteWrite (lpVideo + j, tblTest[i]);

            lpTemp = VerifyBytes (lpVideo, tblTest[i], nLength);
            if (lpTemp != (SEGOFF) NULL)
            {
                nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                                    LOWORD (lpTemp), HIWORD (lpTemp), tblTest[i],
                                    MemByteRead (lpTemp));
                goto Mode13Test_exit;
            }
        }
    }

    SimComment ("Set mode 13");
    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    SetMode (0x13);
    SimDumpMemory ("T0914.VGA");
    SimSetState (TRUE, TRUE, TRUE);

    SimComment ("Load a pattern into memory.");
    chr = attr = 0;
    for (i = nRow; i < (nRow + nRowCount); i++)
    {
        for (j = nCol; j < (nCol + nColCount); j++)
        {
            VGACharOut (chr, attr, (BYTE) j, (BYTE) i, 0);
            chr++; attr++;
        }
    }

    // Bright white overscan
    SimComment ("Turn on overscan.");
    IOByteRead (INPUT_CSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x31);
    IOByteWrite (ATC_INDEX, 0x0F);

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

Mode13Test_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Mode 13 Test passed.");
    else
        SimComment ("Mode 13 Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        15
//
//      T0915
//      HiResMono1Test - Verify C&T Test #2 compatible operation
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int HiResMono1Test (void)
{
    static PARMENTRY    parmHiResMono1 = {
        0xA0, 0x31, 0x08,
        0xFF, 0xFF,
        {0x05, 0x0F, 0x00, 0x0E},                   // SEQ 1..4
        0x63,                                               // Misc
        {0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80,    // CRTC 0..18h
        0xBF, 0x1F, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x9C, 0x0E,
        0x8F, 0x14, 0x60, 0x96, 0xB9, 0xEB,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,    // ATC 0..13h
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
        0x03, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x20,    // GDC 0..8
        0x05, 0x0F, 0xFF}
    };
    static PARMENTRY    parmSmallHiResMono1 = {
        0x50, 0x31, 0x08,
        0xFF, 0xFF,
        {0x05, 0x0F, 0x00, 0x0E},                   // SEQ 1..4
        0x63,                                               // Misc
        {0x2D, 0x27, 0x28, 0x90, 0x2B, 0x80,    // CRTC 0..18h
        0x17, 0x10, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x15, 0x06,
        0x13, 0x0A, 0x60, 0x14, 0x17, 0xEB,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,    // ATC 0..13h
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
        0x03, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x20,    // GDC 0..8
        0x05, 0x0F, 0xFF}
    };
    int         nErr, i, j;
    BYTE        chr;
    BOOL        bFullVGA;
    int         nRowCount,  nColCount;

    nErr = ERROR_NONE;
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto HiResMono1Test_exit;

    SimSetState (TRUE, TRUE, FALSE);            // Ignore DAC writes
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);

    // Load the registers
    SimComment ("Load the parameter table for the hi-res mono 1 test.");
    if (bFullVGA)
    {
        SetRegs (&parmHiResMono1);
        nRowCount = 50;
        nColCount = 160;
    }
    else
    {
        SetRegs (&parmSmallHiResMono1);
        nRowCount = 3;
        nColCount = 80;
    }
    IOByteWrite (DAC_MASK, 0x03);
    SetDac (0x01, 0x20, 0x20, 0x20);
    SetDac (0x02, 0x20, 0x20, 0x20);
    SetDac (0x03, 0x2A, 0x2A, 0x2A);

    SimComment ("Draw the characters into memory.");
    chr = 1;
    for (i = 0; i < nRowCount; i++)
        for (j = 0; j < nColCount; j++)
            DrawMonoChar (j, i, chr++, tblFont8x8, 8, nColCount);

    SimDumpMemory ("T0915.VGA");

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto HiResMono1Test_exit;
        }
    }

HiResMono1Test_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Hi-Res Mono 1 Test passed.");
    else
        SimComment ("Hi-Res Mono 1 Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        16
//
//      T0916
//      HiResMono2Test - Verify C&T Test #1 compatible operation
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int HiResMono2Test (void)
{
    static PARMENTRY    parmHiResMono1 = {
        0xA0, 0x31, 0x08,
        0xFF, 0xFF,
        {0x11, 0x0F, 0x00, 0x0E},               // SEQ 1..4
        0x63,                                   // Misc
        {0x5F, 0x50, 0x52, 0xE2, 0x54, 0xE0,    // CRTC 0..18h
        0xBF, 0x1F, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x9C, 0x0E,
        0x8F, 0x14, 0x60, 0x96, 0xB9, 0xE3,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,    // ATC 0..13h
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
        0x01, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    // GDC 0..8
        0x05, 0x0F, 0xFF}
    };
    static PARMENTRY    parmSmallHiResMono1 = {
        0x50, 0x31, 0x08,
        0xFF, 0xFF,
        {0x11, 0x0F, 0x00, 0x0E},               // SEQ 1..4
        0x63,                                   // Misc
        {0x2D, 0x27, 0x28, 0xF0, 0x2B, 0xE0,    // CRTC 0..18h
        0x17, 0x10, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x15, 0x06,
        0x13, 0x0A, 0x60, 0x14, 0x17, 0xE3,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,    // ATC 0..13h
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
        0x01, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    // GDC 0..8
        0x05, 0x0F, 0xFF}
    };
    int         nErr, i, j, nPans, nRowCount, nColCount;
    BYTE        chr;
    BOOL        bFullVGA, bCapture;

    nErr = ERROR_NONE;
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto HiResMono2Test_exit;

    SimSetState (TRUE, TRUE, FALSE);                // Ignore DAC writes
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);

    // Load the registers
    SimComment ("Load the parameter table for the hi-res mono 2 test.");
    if (bFullVGA)
    {
        SetRegs (&parmHiResMono1);
        nRowCount = 50;
        nColCount = 160;
    }
    else
    {
        SetRegs (&parmSmallHiResMono1);
        nRowCount = 3;
        nColCount = 80;
    }

    IOByteWrite (0x3C6, 0x03);
    SetDac (0x01, 0x20, 0x20, 0x20);
    SetDac (0x02, 0x20, 0x20, 0x20);
    SetDac (0x03, 0x2A, 0x2A, 0x2A);

    SimComment ("Draw the characters into memory.");
    chr = 1;
    for (i = 0; i < nRowCount; i++)
        for (j = 0; j < nColCount; j++)
            DrawMonoChar (j, i, chr++, tblFont8x8, 8, nColCount);

    SimDumpMemory ("T0916.VGA");

    bCapture = FrameCapture (REF_PART, REF_TEST);
    if (!bCapture)
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto HiResMono2Test_exit;
        }
    }

    // Now pan the beast
    SimComment ("Pan the beast.");
    nPans = 640;
    if (bCapture) nPans = 34;
    for (i = 0; i < nPans; i++)
    {
        WaitNotVerticalRetrace ();
        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (_kbhit ())
            {
                if (GetKey () == KEY_ESCAPE)
                {
                    nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                    goto HiResMono2Test_exit;
                }
            }
        }
        IOByteRead (INPUT_CSTATUS_1);
        IOByteWrite (ATC_INDEX, 0x33);
        IOByteWrite (ATC_INDEX, (BYTE) (i & 0x7));              // Pixel panning
        IOByteWrite (CRTC_CINDEX, 0x08);
        IOByteWrite (CRTC_CDATA, (BYTE) ((i & 0x18) << 2));     // Byte panning
        IOByteWrite (CRTC_CINDEX, 0x0C);
        IOByteWrite (CRTC_CDATA, (BYTE) (i >> 13));             // Display start high
        IOByteWrite (CRTC_CINDEX, 0x0D);
        IOByteWrite (CRTC_CDATA, (BYTE) ((i >> 5) & 0xFF));     // Display start low
    }

    WaitNotVerticalRetrace ();
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

HiResMono2Test_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Hi-Res Mono 2 Test passed.");
    else
        SimComment ("Hi-Res Mono 2 Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        17
//
//      T0917
//      HiResColor1Test - Verify C&T Test #4 compatible operation
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int HiResColor1Test (void)
{
    static PARMENTRY    parmHiResColor1 = {     // 1280x400x2
        0xA0, 0x31, 0x08,
        0xFF, 0xFF,
        {0x01, 0x0F, 0x00, 0x0E},                   // SEQ 1..4
        0x63,                                               // Misc
        {0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80,    // CRTC 0..18h
        0xBF, 0x1F, 0x00, 0x41, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00, 0x9C, 0x0E,
        0x8F, 0x28, 0x40, 0x96, 0xB9, 0xE3,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,    // ATC 0..13h
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
        0x0F, 0x07},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x40,    // GDC 0..8
        0x05, 0x0F, 0xFF}
    };
    static PARMENTRY    parmSmallHiResColor1 = {// 1280x400x2
        0x50, 0x31, 0x08,
        0xFF, 0xFF,
        {0x01, 0x0F, 0x00, 0x0E},                   // SEQ 1..4
        0x63,                                               // Misc
        {0x2D, 0x27, 0x28, 0x90, 0x2B, 0x80,    // CRTC 0..18h
        0x17, 0x10, 0x00, 0x41, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00, 0x15, 0x06,
        0x13, 0x14, 0x40, 0x14, 0x17, 0xE3,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,    // ATC 0..13h
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
        0x0F, 0x07},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x40,    // GDC 0..8
        0x05, 0x0F, 0xFF}
    };
    BYTE    dacTable[] = {
        0x00, 0x00, 0x00,
        0x20, 0x00, 0x00,
        0x00, 0x20, 0x00,
        0x2A, 0x2A, 0x2A,
        0x20, 0x00, 0x00,
        0x2A, 0x00, 0x00,
        0x2A, 0x2A, 0x00,
        0x3F, 0x20, 0x20,
        0x00, 0x20, 0x00,
        0x2A, 0x2A, 0x00,
        0x00, 0x2A, 0x00,
        0x20, 0x3F, 0x20,
        0x2A, 0x2A, 0x2A,
        0x3F, 0x20, 0x20,
        0x20, 0x3F, 0x20,
        0x3F, 0x3F, 0x3F
    };
    int         nErr;
    SEGOFF      lpVideo, lpVideoLeft, lpVideoRight;
    WORD        i, j, k, n;
    BYTE        chr;
    BYTE        table[] = {0xFF, 0xAA, 0x55};
    BOOL        bFullVGA;
    int         nRowOffset, nLineCount, nLastLine, nLineSpace;

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xA0000000;
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto HiResColor1Test_exit;

    SimSetState (TRUE, TRUE, FALSE);                // Ignore DAC writes
    SetMode (0x13);
    SimSetState (TRUE, TRUE, TRUE);

    // Load the registers
    SimComment ("Load the parameter table for the hi-res color test.");
    if (bFullVGA)
    {
        SetRegs (&parmHiResColor1);
        nRowOffset = 320;
        nLastLine = 199;
        nLineCount = 21;
        nLineSpace = 3;
    }
    else
    {
        SetRegs (&parmSmallHiResColor1);
        nRowOffset = 160;
        nLastLine = 9;
        nLineCount = 3;
        nLineSpace = 2;
    }

    IOByteWrite (DAC_MASK, 0x0F);
    SetDacBlock (dacTable, 0, 16);

    SimComment ("Draw a pattern into memory.");
    for (i = 0; i < (WORD) nLineCount; i++)
    {
        chr = table[i%3];
        lpVideoLeft = lpVideo + (nRowOffset + 1)*i*nLineSpace;
        lpVideoRight = lpVideoLeft + (nRowOffset - i*nLineSpace*2) - 1;
        k = nRowOffset - i*nLineSpace*2;
        n = i*nLineSpace + nRowOffset*(nLastLine - i*nLineSpace);
        for (j = 0; j < k; j++)
        {
            MemByteWrite (lpVideoLeft + j, chr);
            MemByteWrite (lpVideo + n + j, chr);
        }
        for (j = 0; j < (nLastLine - i*nLineSpace*2); j++)
        {
            MemByteWrite (lpVideoLeft, (BYTE) (MemByteRead (lpVideoLeft) | (chr & 0xF0)));
            MemByteWrite (lpVideoRight, (BYTE) (MemByteRead (lpVideoRight) | (chr & 0x0F)));
            lpVideoLeft += nRowOffset;
            lpVideoRight += nRowOffset;
        }
    }

    SimDumpMemory ("T0917.VGA");

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto HiResColor1Test_exit;
        }
    }

HiResColor1Test_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Hi-Res Color 1 Test passed.");
    else
        SimComment ("Hi-Res Color 1 Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        18
//
//      T0918
//      ModeXTest - Verify mode 'X' style test
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int ModeXTest (void)
{
    static BYTE dac[256][3] = {
        // Vary blue by +2
        {0x0, 0x0, 0x0},        // Index 0
        {0x0, 0x0, 0x2},        // Index 1
        {0x0, 0x0, 0x4},        // Index 2
        {0x0, 0x0, 0x6},        // Index 3
        {0x0, 0x0, 0x8},        // Index 4
        {0x0, 0x0, 0x0A},       // Index 5
        {0x0, 0x0, 0x0C},       // Index 6
        {0x0, 0x0, 0x0E},       // Index 7
        {0x0, 0x0, 0x10},       // Index 8
        {0x0, 0x0, 0x12},       // Index 9
        {0x0, 0x0, 0x14},       // Index 10
        {0x0, 0x0, 0x16},       // Index 11
        {0x0, 0x0, 0x18},       // Index 12
        {0x0, 0x0, 0x1A},       // Index 13
        {0x0, 0x0, 0x1C},       // Index 14
        {0x0, 0x0, 0x1E},       // Index 15
        {0x0, 0x0, 0x20},       // Index 16
        {0x0, 0x0, 0x22},       // Index 17
        {0x0, 0x0, 0x24},       // Index 18
        {0x0, 0x0, 0x26},       // Index 19
        {0x0, 0x0, 0x28},       // Index 20
        {0x0, 0x0, 0x2A},       // Index 21
        {0x0, 0x0, 0x2C},       // Index 22
        {0x0, 0x0, 0x2E},       // Index 23
        {0x0, 0x0, 0x30},       // Index 24
        {0x0, 0x0, 0x32},       // Index 25
        {0x0, 0x0, 0x34},       // Index 26
        {0x0, 0x0, 0x36},       // Index 27
        {0x0, 0x0, 0x38},       // Index 28
        {0x0, 0x0, 0x3A},       // Index 29
        {0x0, 0x0, 0x3C},       // Index 30
        {0x0, 0x0, 0x3F},       // Index 31
        // Vary green by +2
        {0x0, 0x02, 0x03F},     // Index 32
        {0x0, 0x04, 0x03F},     // Index 33
        {0x0, 0x06, 0x03F},     // Index 34
        {0x0, 0x08, 0x03F},     // Index 35
        {0x0, 0x0A, 0x03F},     // Index 36
        {0x0, 0x0C, 0x03F},     // Index 37
        {0x0, 0x0E, 0x03F},     // Index 38
        {0x0, 0x10, 0x03F},     // Index 39
        {0x0, 0x12, 0x03F},     // Index 40
        {0x0, 0x14, 0x03F},     // Index 41
        {0x0, 0x16, 0x03F},     // Index 42
        {0x0, 0x18, 0x03F},     // Index 43
        {0x0, 0x1A, 0x03F},     // Index 44
        {0x0, 0x1C, 0x03F},     // Index 45
        {0x0, 0x1E, 0x03F},     // Index 46
        {0x0, 0x20, 0x03F},     // Index 47
        {0x0, 0x22, 0x03F},     // Index 48
        {0x0, 0x24, 0x03F},     // Index 49
        {0x0, 0x26, 0x03F},     // Index 50
        {0x0, 0x28, 0x03F},     // Index 51
        {0x0, 0x2A, 0x03F},     // Index 52
        {0x0, 0x2C, 0x03F},     // Index 53
        {0x0, 0x2E, 0x03F},     // Index 54
        {0x0, 0x30, 0x03F},     // Index 55
        {0x0, 0x32, 0x03F},     // Index 56
        {0x0, 0x34, 0x03F},     // Index 57
        {0x0, 0x36, 0x03F},     // Index 58
        {0x0, 0x38, 0x03F},     // Index 59
        {0x0, 0x3A, 0x03F},     // Index 60
        {0x0, 0x3C, 0x03F},     // Index 61
        {0x0, 0x3F, 0x03F},     // Index 62
        // Vary blue by -2
        {0x0, 0x3F, 0x3C},      // Index 63
        {0x0, 0x3F, 0x3A},      // Index 64
        {0x0, 0x3F, 0x38},      // Index 65
        {0x0, 0x3F, 0x36},      // Index 66
        {0x0, 0x3F, 0x34},      // Index 67
        {0x0, 0x3F, 0x32},      // Index 68
        {0x0, 0x3F, 0x30},      // Index 69
        {0x0, 0x3F, 0x2E},      // Index 70
        {0x0, 0x3F, 0x2C},      // Index 71
        {0x0, 0x3F, 0x2A},      // Index 72
        {0x0, 0x3F, 0x28},      // Index 73
        {0x0, 0x3F, 0x26},      // Index 74
        {0x0, 0x3F, 0x24},      // Index 75
        {0x0, 0x3F, 0x22},      // Index 76
        {0x0, 0x3F, 0x20},      // Index 77
        {0x0, 0x3F, 0x1E},      // Index 78
        {0x0, 0x3F, 0x1C},      // Index 79
        {0x0, 0x3F, 0x1A},      // Index 80
        {0x0, 0x3F, 0x18},      // Index 81
        {0x0, 0x3F, 0x16},      // Index 82
        {0x0, 0x3F, 0x14},      // Index 83
        {0x0, 0x3F, 0x12},      // Index 84
        {0x0, 0x3F, 0x10},      // Index 85
        {0x0, 0x3F, 0x0E},      // Index 86
        {0x0, 0x3F, 0x0C},      // Index 87
        {0x0, 0x3F, 0x0A},      // Index 88
        {0x0, 0x3F, 0x08},      // Index 89
        {0x0, 0x3F, 0x06},      // Index 90
        {0x0, 0x3F, 0x04},      // Index 91
        {0x0, 0x3F, 0x02},      // Index 92
        {0x0, 0x3F, 0x00},      // Index 93
        // Vary red by +2
        {0x02, 0x03F, 0x00},    // Index 94
        {0x04, 0x03F, 0x00},    // Index 95
        {0x06, 0x03F, 0x00},    // Index 96
        {0x08, 0x03F, 0x00},    // Index 97
        {0x0A, 0x03F, 0x00},    // Index 98
        {0x0C, 0x03F, 0x00},    // Index 99
        {0x0E, 0x03F, 0x00},    // Index 100
        {0x10, 0x03F, 0x00},    // Index 101
        {0x12, 0x03F, 0x00},    // Index 102
        {0x14, 0x03F, 0x00},    // Index 103
        {0x16, 0x03F, 0x00},    // Index 104
        {0x18, 0x03F, 0x00},    // Index 105
        {0x1A, 0x03F, 0x00},    // Index 106
        {0x1C, 0x03F, 0x00},    // Index 107
        {0x1E, 0x03F, 0x00},    // Index 108
        {0x20, 0x03F, 0x00},    // Index 109
        {0x22, 0x03F, 0x00},    // Index 110
        {0x24, 0x03F, 0x00},    // Index 111
        {0x26, 0x03F, 0x00},    // Index 112
        {0x28, 0x03F, 0x00},    // Index 113
        {0x2A, 0x03F, 0x00},    // Index 114
        {0x2C, 0x03F, 0x00},    // Index 115
        {0x2E, 0x03F, 0x00},    // Index 116
        {0x30, 0x03F, 0x00},    // Index 117
        {0x32, 0x03F, 0x00},    // Index 118
        {0x34, 0x03F, 0x00},    // Index 119
        {0x36, 0x03F, 0x00},    // Index 120
        {0x38, 0x03F, 0x00},    // Index 121
        {0x3A, 0x03F, 0x00},    // Index 122
        {0x3C, 0x03F, 0x00},    // Index 123
        {0x3F, 0x03F, 0x00},    // Index 124
        // Vary green by -2
        {0x3F, 0x03C, 0x00},    // Index 125
        {0x3F, 0x03A, 0x00},    // Index 126
        {0x3F, 0x038, 0x00},    // Index 127
        {0x3F, 0x036, 0x00},    // Index 128
        {0x3F, 0x034, 0x00},    // Index 129
        {0x3F, 0x032, 0x00},    // Index 130
        {0x3F, 0x030, 0x00},    // Index 131
        {0x3F, 0x02E, 0x00},    // Index 132
        {0x3F, 0x02C, 0x00},    // Index 133
        {0x3F, 0x02A, 0x00},    // Index 134
        {0x3F, 0x028, 0x00},    // Index 135
        {0x3F, 0x026, 0x00},    // Index 136
        {0x3F, 0x024, 0x00},    // Index 137
        {0x3F, 0x022, 0x00},    // Index 138
        {0x3F, 0x020, 0x00},    // Index 139
        {0x3F, 0x01E, 0x00},    // Index 140
        {0x3F, 0x01C, 0x00},    // Index 141
        {0x3F, 0x01A, 0x00},    // Index 142
        {0x3F, 0x018, 0x00},    // Index 143
        {0x3F, 0x016, 0x00},    // Index 144
        {0x3F, 0x014, 0x00},    // Index 145
        {0x3F, 0x012, 0x00},    // Index 146
        {0x3F, 0x010, 0x00},    // Index 147
        {0x3F, 0x00E, 0x00},    // Index 148
        {0x3F, 0x00C, 0x00},    // Index 149
        {0x3F, 0x00A, 0x00},    // Index 150
        {0x3F, 0x008, 0x00},    // Index 151
        {0x3F, 0x006, 0x00},    // Index 152
        {0x3F, 0x004, 0x00},    // Index 153
        {0x3F, 0x002, 0x00},    // Index 154
        {0x3F, 0x000, 0x00},    // Index 155
        // Vary blue by +2
        {0x3F, 0x000, 0x02},    // Index 156
        {0x3F, 0x000, 0x04},    // Index 157
        {0x3F, 0x000, 0x06},    // Index 158
        {0x3F, 0x000, 0x08},    // Index 159
        {0x3F, 0x000, 0x0A},    // Index 160
        {0x3F, 0x000, 0x0C},    // Index 161
        {0x3F, 0x000, 0x0E},    // Index 162
        {0x3F, 0x000, 0x10},    // Index 163
        {0x3F, 0x000, 0x12},    // Index 164
        {0x3F, 0x000, 0x14},    // Index 165
        {0x3F, 0x000, 0x16},    // Index 166
        {0x3F, 0x000, 0x18},    // Index 167
        {0x3F, 0x000, 0x1A},    // Index 168
        {0x3F, 0x000, 0x1C},    // Index 169
        {0x3F, 0x000, 0x1E},    // Index 170
        {0x3F, 0x000, 0x20},    // Index 171
        {0x3F, 0x000, 0x22},    // Index 172
        {0x3F, 0x000, 0x24},    // Index 173
        {0x3F, 0x000, 0x26},    // Index 174
        {0x3F, 0x000, 0x28},    // Index 175
        {0x3F, 0x000, 0x2A},    // Index 176
        {0x3F, 0x000, 0x2C},    // Index 177
        {0x3F, 0x000, 0x2E},    // Index 178
        {0x3F, 0x000, 0x30},    // Index 179
        {0x3F, 0x000, 0x32},    // Index 180
        {0x3F, 0x000, 0x34},    // Index 181
        {0x3F, 0x000, 0x36},    // Index 182
        {0x3F, 0x000, 0x38},    // Index 183
        {0x3F, 0x000, 0x3A},    // Index 184
        {0x3F, 0x000, 0x3C},    // Index 185
        {0x3F, 0x000, 0x3F},    // Index 186
        // Vary green by +2
        {0x3F, 0x002, 0x3F},    // Index 187
        {0x3F, 0x004, 0x3F},    // Index 188
        {0x3F, 0x006, 0x3F},    // Index 189
        {0x3F, 0x008, 0x3F},    // Index 190
        {0x3F, 0x00A, 0x3F},    // Index 191
        {0x3F, 0x00C, 0x3F},    // Index 192
        {0x3F, 0x00E, 0x3F},    // Index 193
        {0x3F, 0x010, 0x3F},    // Index 194
        {0x3F, 0x012, 0x3F},    // Index 195
        {0x3F, 0x014, 0x3F},    // Index 196
        {0x3F, 0x016, 0x3F},    // Index 197
        {0x3F, 0x018, 0x3F},    // Index 198
        {0x3F, 0x01A, 0x3F},    // Index 199
        {0x3F, 0x01C, 0x3F},    // Index 200
        {0x3F, 0x01E, 0x3F},    // Index 201
        {0x3F, 0x020, 0x3F},    // Index 202
        {0x3F, 0x022, 0x3F},    // Index 203
        {0x3F, 0x024, 0x3F},    // Index 204
        {0x3F, 0x026, 0x3F},    // Index 205
        {0x3F, 0x028, 0x3F},    // Index 206
        {0x3F, 0x02A, 0x3F},    // Index 207
        {0x3F, 0x02C, 0x3F},    // Index 208
        {0x3F, 0x02E, 0x3F},    // Index 209
        {0x3F, 0x030, 0x3F},    // Index 210
        {0x3F, 0x032, 0x3F},    // Index 211
        {0x3F, 0x034, 0x3F},    // Index 212
        {0x3F, 0x036, 0x3F},    // Index 213
        {0x3F, 0x038, 0x3F},    // Index 214
        {0x3F, 0x03A, 0x3F},    // Index 215
        {0x3F, 0x03C, 0x3F},    // Index 216
        {0x3F, 0x03E, 0x3F},    // Index 217
        // Vary (approximately) red by -2, green by -2, blue by -2
        {0x3F, 0x03F, 0x3F},    // Index 218
        {0x3E, 0x03E, 0x3E},    // Index 219
        {0x3D, 0x03D, 0x3D},    // Index 220
        {0x3C, 0x03C, 0x3C},    // Index 221
        {0x3B, 0x03B, 0x3B},    // Index 222
        {0x3A, 0x03A, 0x3A},    // Index 223
        {0x39, 0x039, 0x39},    // Index 224
        {0x38, 0x038, 0x38},    // Index 225
        {0x37, 0x037, 0x37},    // Index 226
        {0x36, 0x036, 0x36},    // Index 227
        {0x34, 0x034, 0x34},    // Index 228
        {0x32, 0x032, 0x32},    // Index 229
        {0x30, 0x030, 0x30},    // Index 230
        {0x2E, 0x02E, 0x2E},    // Index 231
        {0x2C, 0x02C, 0x2C},    // Index 232
        {0x2A, 0x02A, 0x2A},    // Index 233
        {0x28, 0x028, 0x28},    // Index 234
        {0x26, 0x026, 0x26},    // Index 235
        {0x24, 0x024, 0x24},    // Index 236
        {0x22, 0x022, 0x22},    // Index 237
        {0x20, 0x020, 0x20},    // Index 238
        {0x1E, 0x01E, 0x1E},    // Index 239
        {0x1C, 0x01C, 0x1C},    // Index 240
        {0x1A, 0x01A, 0x1A},    // Index 241
        {0x18, 0x018, 0x18},    // Index 241
        {0x16, 0x016, 0x16},    // Index 243
        {0x14, 0x014, 0x14},    // Index 244
        {0x12, 0x012, 0x12},    // Index 245
        {0x10, 0x010, 0x10},    // Index 246
        {0x0E, 0x00E, 0x0E},    // Index 247
        {0x0C, 0x00C, 0x0C},    // Index 248
        {0x0A, 0x00A, 0x0A},    // Index 249
        {0x08, 0x008, 0x08},    // Index 250
        {0x06, 0x006, 0x06},    // Index 251
        {0x04, 0x004, 0x04},    // Index 252
        {0x02, 0x002, 0x02},    // Index 253
        {0x01, 0x001, 0x01},    // Index 254
        {0x00, 0x000, 0x00} // Index 255
    };
    static PARMENTRY    parmX = {                   // 320x400x8
        0x50, 0x1D, 0x10,
        0xFF, 0xFF,
        {0x01, 0x0F, 0x00, 0x06},                   // SEQ 1..4
        0x63,                                               // Misc
        {0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80,    // CRTC 0..18h
        0xBF, 0x1F, 0x20, 0x40, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00, 0x9C, 0x0E,
        0x8F, 0x28, 0x00, 0x96, 0xB9, 0xE3,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,    // ATC 0..13h
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
        0x0F, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x40,    // GDC 0..8
        0x05, 0x0F, 0xFF}
    };
    static PARMENTRY    parmSmallX = {
        0x28, 0x1D, 0x10,
        0xFF, 0xFF,
        {0x01, 0x0F, 0x00, 0x06},                   // SEQ 1..4
        0x63,                                               // Misc
        {0x2D, 0x27, 0x28, 0x90, 0x2B, 0x80,    // CRTC 0..18h
        0x17, 0x10, 0x20, 0x40, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00, 0x15, 0x06,
        0x13, 0x14, 0x00, 0x14, 0x17, 0xE3,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,    // ATC 0..13h
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
        0x0F, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x40,    // GDC 0..8
        0x05, 0x0F, 0xFF}
    };
    int         nErr, i, j;
    SEGOFF      lpVideo;
    BYTE        clr, clrOrg;
    WORD        wXRes, wYRes;
    BOOL        bFullVGA;

    nErr = ERROR_NONE;
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto ModeXTest_exit;

    SimSetState (TRUE, TRUE, FALSE);                // Ignore DAC writes
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);

    SimComment ("Load a visually suitable DAC pattern.");
    SetDacBlock ((LPBYTE) dac, 0, 256);

    // Load the registers
    SimComment ("Load the parameter table for the mode X test.");
    if (bFullVGA)
    {
        SetRegs (&parmX);
        wXRes = 320;
        wYRes = 400;
    }
    else
    {
        SetRegs (&parmSmallX);
        wXRes = 160;
        wYRes = 20;
    }

    // Draw a "cool" pattern into memory
    SimComment ("Draw a \"cool\" pattern into memory.");
    clr = clrOrg = 0;
    for (i = 0; i < (int) wXRes; i++)
    {
        lpVideo = (SEGOFF) (0xA0000000) + (i / 4);
        j = 1 << (i & 3);
        IOByteWrite (SEQ_INDEX, 0x02);
        IOByteWrite (SEQ_DATA, (BYTE) j);
        clr = clrOrg++;
        for (j = 0; j < (int) (wYRes - 1); j++)
        {
            MemByteWrite (lpVideo, clr++);
            lpVideo += wXRes/4;
        }
    }

    SimDumpMemory ("T0918.VGA");

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto ModeXTest_exit;
        }
    }

ModeXTest_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Mode X Test passed.");
    else
        SimComment ("Mode X Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        19
int MemoryAccessSubTest (void);
//
//      T0919
//      MemoryAccessTest - Verify memory can be accessed as BYTE, WORD, and DWORD
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int MemoryAccessTest (void)
{
    int     nErr;

    nErr = ERROR_NONE;

    SimSetState (TRUE, FALSE, FALSE);
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);

    SimDumpMemory ("T0919.VGA");

    // Do test first in planar mode
    SimComment ("Do test first in planar mode.");
    nErr = MemoryAccessSubTest ();
    if (nErr != ERROR_NONE) goto MemoryAccessTest_exit;

    // Set chain/4 and repeat test
    SimComment ("Set chain/4 and repeat test.");
    IOWordWrite (SEQ_INDEX, 0x0E04);
    nErr = MemoryAccessSubTest ();
    if (nErr != ERROR_NONE) goto MemoryAccessTest_exit;

    // Set chain/2 and odd/even modes and repeat test
    SimComment ("Set chain/2 and odd/even modes and repeat test.");
    IOWordWrite (SEQ_INDEX, 0x0204);            // Clear Chain/4, Set odd/even
    IOWordWrite (GDC_INDEX, 0x1005);            // Set odd/even
    IOWordWrite (GDC_INDEX, 0x0706);            // Set chain/2
    nErr = MemoryAccessSubTest ();

MemoryAccessTest_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Memory Access Test passed.");
    else
        SimComment ("Memory Access Test failed.");
    SystemCleanUp ();
    return (nErr);
}

//
//      MemoryAccessSubTest - Test various BYTE, WORD, and DWORD accesses
//
//      Entry:  None
//      Exit:   <int>
//
int MemoryAccessSubTest (void)
{
    int     nErr, i, j;
    SEGOFF  lpVideo;
    BYTE    byData0, byData1, byData2, byData3;
    WORD    wData;
    DWORD   dwData;
    BYTE    tblBytePattern[16] = {
                0x00, 0x55, 0xAA, 0xFF, 0x66, 0x99, 0x69, 0x96,
                0xC3, 0x3C, 0x5A, 0xA5, 0xDB, 0xBD, 0x81, 0x18
            };
    WORD    tblWordPattern[8] = {
                0x0FF0, 0x5AA5, 0xA55A, 0xF00F, 0x9669, 0x6996, 0xC33C, 0xDBBD
            };
    DWORD   tblDWordPattern[4] = {
                0xC00F0CF0, 0xE77E5AA5, 0x6996C33C, 0xBDDB1881
            };

    nErr = ERROR_NONE;
    lpVideo = (SEGOFF) 0xA0000000;

    // Write BYTEs to 16 locations and verify as BYTE, WORD, and DWORD
    for (i = 0; i < 16; i++)
    {
        MemByteWrite (lpVideo + i, tblBytePattern[i]);
    }
    for (i = 0; i < 16; i++)
    {
        byData0 = MemByteRead (lpVideo + i);
        if (byData0 != tblBytePattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, i, HIWORD (lpVideo), tblBytePattern[i], byData0);
            goto MemoryAccessSubTest_exit;
        }
    }
    for (i = 0; i < 8; i+=2)
    {
        wData = MemWordRead (lpVideo + i);
        byData0 = LOBYTE (wData);
        byData1 = HIBYTE (wData);
        if (byData0 != tblBytePattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, i, HIWORD (lpVideo), tblBytePattern[i], byData0);
            goto MemoryAccessSubTest_exit;
        }
        if (byData1 != tblBytePattern[i + 1])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, i, HIWORD (lpVideo), tblBytePattern[i+1], byData1);
            goto MemoryAccessSubTest_exit;
        }
    }
    for (i = 0; i < 4; i+=4)
    {
        dwData = MemDwordRead (lpVideo + i);
        byData0 = LOBYTE (LOWORD (dwData));
        byData1 = HIBYTE (LOWORD (dwData));
        byData2 = LOBYTE (HIWORD (dwData));
        byData3 = HIBYTE (HIWORD (dwData));
        if (byData0 != tblBytePattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, i, HIWORD (lpVideo), tblBytePattern[i], byData0);
            goto MemoryAccessSubTest_exit;
        }
        if (byData1 != tblBytePattern[i + 1])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, i, HIWORD (lpVideo), tblBytePattern[i+1], byData1);
            goto MemoryAccessSubTest_exit;
        }
        if (byData2 != tblBytePattern[i + 2])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, i, HIWORD (lpVideo), tblBytePattern[i+2], byData2);
            goto MemoryAccessSubTest_exit;
        }
        if (byData3 != tblBytePattern[i + 3])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, i, HIWORD (lpVideo), tblBytePattern[i+3], byData3);
            goto MemoryAccessSubTest_exit;
        }
    }

    // Write WORDs to 16 locations as WORD aligned and verify (as BYTES)
    for (i = 0, j = 0; i < 8; i++, j+=2)
    {
        MemWordWrite (lpVideo + j, tblWordPattern[i]);
    }
    for (i = 0, j = 0; i < 8; i++, j+=2)
    {
        byData0 = MemByteRead (lpVideo + j);
        byData1 = MemByteRead (lpVideo + j + 1);
        wData = ((WORD) byData1 << 8) | (WORD) byData0;
        if (wData != tblWordPattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, j, HIWORD (lpVideo), tblWordPattern[i], wData);
            goto MemoryAccessSubTest_exit;
        }
    }

#ifdef ALLOW_UNALIGNED
    // Write WORDs to 16 locations as non-aligned
    for (i = 0, j = 1; i < 8; i++, j+=2)
    {
        MemWordWrite (lpVideo + j, tblWordPattern[i]);
    }
    for (i = 0, j = 1; i < 8; i++, j+=2)
    {
        // First verify that an unaligned WORD read can take place and matches
        // the data written.
        wData = MemWordRead (lpVideo + j);
        if (wData != tblWordPattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, j, HIWORD (lpVideo), tblWordPattern[i], wData);
            goto MemoryAccessSubTest_exit;
        }
        // Next verify that the WORD write worked correctly and that the data
        // written is as expected.
        byData0 = MemByteRead (lpVideo + j);
        byData1 = MemByteRead (lpVideo + j + 1);
        wData = ((WORD) byData1 << 8) | (WORD) byData0;
        if (wData != tblWordPattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, j, HIWORD (lpVideo), tblWordPattern[i], wData);
            goto MemoryAccessSubTest_exit;
        }
    }
#endif

    // Write DWORDs to 16 locations as DWORD aligned (starting on BYTE 0)
    for (i = 0, j = 0; i < 4; i++, j+=4)
    {
        MemDwordWrite (lpVideo + j, tblDWordPattern[i]);
    }
    for (i = 0, j = 0; i < 4; i++, j+=4)
    {
        byData0 = MemByteRead (lpVideo + j);
        byData1 = MemByteRead (lpVideo + j + 1);
        byData2 = MemByteRead (lpVideo + j + 2);
        byData3 = MemByteRead (lpVideo + j + 3);
        dwData = ((DWORD) byData3 << 24) | ((DWORD) byData2 << 16) | ((DWORD) byData1 << 8) | (DWORD) byData0;
        if (dwData != tblDWordPattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, j, HIWORD (lpVideo), tblDWordPattern[i], dwData);
            goto MemoryAccessSubTest_exit;
        }
    }

#ifdef ALLOW_UNALIGNED
    // Write DWORDs to 16 locations non-aligned starting on BYTE 1
    for (i = 0, j = 1; i < 4; i++, j+=4)
    {
        MemDwordWrite (lpVideo + j, tblDWordPattern[i]);
    }
    for (i = 0, j = 1; i < 4; i++, j+=4)
    {
        // First verify that an unaligned DWORD read can take place and
        // matches the data written.
        dwData = MemDwordRead (lpVideo + j);
        if (dwData != tblDWordPattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, j, HIWORD (lpVideo), tblDWordPattern[i], dwData);
            goto MemoryAccessSubTest_exit;
        }
        // Next verify that the DWORD write worked correctly and that the data
        // written is as expected.
        byData0 = MemByteRead (lpVideo + j);
        byData1 = MemByteRead (lpVideo + j + 1);
        byData2 = MemByteRead (lpVideo + j + 2);
        byData3 = MemByteRead (lpVideo + j + 3);
        dwData = ((DWORD) byData3 << 24) | ((DWORD) byData2 << 16) | ((DWORD) byData1 << 8) | (DWORD) byData0;
        if (dwData != tblDWordPattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, j, HIWORD (lpVideo), tblDWordPattern[i], dwData);
            goto MemoryAccessSubTest_exit;
        }
    }

    // Write DWORDs to 16 locations non-aligned starting on BYTE 2
    for (i = 0, j = 2; i < 4; i++, j+=4)
    {
        MemDwordWrite (lpVideo + j, tblDWordPattern[i]);
    }
    for (i = 0, j = 2; i < 4; i++, j+=4)
    {
        // First verify that an unaligned DWORD read can take place and
        // matches the data written.
        dwData = MemDwordRead (lpVideo + j);
        if (dwData != tblDWordPattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, j, HIWORD (lpVideo), tblDWordPattern[i], dwData);
            goto MemoryAccessSubTest_exit;
        }
        // Next verify that the DWORD write worked correctly and that the data
        // written is as expected.
        byData0 = MemByteRead (lpVideo + j);
        byData1 = MemByteRead (lpVideo + j + 1);
        byData2 = MemByteRead (lpVideo + j + 2);
        byData3 = MemByteRead (lpVideo + j + 3);
        dwData = ((DWORD) byData3 << 24) | ((DWORD) byData2 << 16) | ((DWORD) byData1 << 8) | (DWORD) byData0;
        if (dwData != tblDWordPattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, j, HIWORD (lpVideo), tblDWordPattern[i], dwData);
            goto MemoryAccessSubTest_exit;
        }
    }

    // Write DWORDs to 16 locations non-aligned starting on BYTE 3
    for (i = 0, j = 3; i < 4; i++, j+=4)
    {
        MemDwordWrite (lpVideo + j, tblDWordPattern[i]);
    }
    for (i = 0, j = 3; i < 4; i++, j+=4)
    {
        // First verify that an unaligned DWORD read can take place and
        // matches the data written.
        dwData = MemDwordRead (lpVideo + j);
        if (dwData != tblDWordPattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, j, HIWORD (lpVideo), tblDWordPattern[i], dwData);
            goto MemoryAccessSubTest_exit;
        }
        // Next verify that the DWORD write worked correctly and that the data
        // written is as expected.
        byData0 = MemByteRead (lpVideo + j);
        byData1 = MemByteRead (lpVideo + j + 1);
        byData2 = MemByteRead (lpVideo + j + 2);
        byData3 = MemByteRead (lpVideo + j + 3);
        dwData = ((DWORD) byData3 << 24) | ((DWORD) byData2 << 16) | ((DWORD) byData1 << 8) | (DWORD) byData0;
        if (dwData != tblDWordPattern[i])
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, j, HIWORD (lpVideo), tblDWordPattern[i], dwData);
            goto MemoryAccessSubTest_exit;
        }
    }
#endif

MemoryAccessSubTest_exit:
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        20
int IOTest (BYTE, WORD, WORD, BOOL, BYTE, BYTE);
void BurstMemoryWrites (SEGOFF, LPDWORD, int);
void ClearModeX (BOOL);
void WritePatternStrip (LPBYTE, int, int, DWORD *, BOOL);
BOOL VerifyPatternStrip (LPBYTE, int, int, DWORD *, BOOL);

int     g_T0920_nErrPlane = 0;
DWORD   g_T0920_dwErrAct = 0;
DWORD   g_T0920_dwErrExp = 0;
LPBYTE  g_T0920_lpErrAddr = NULL;

//
//      T0920
//      RandomAccessTest - Verify that the VGA space can be accessed in any sequence
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int RandomAccessTest (void)
{
    static DWORD    dwBurstData[] = {
        0x33221100, 0x77665544, 0xBBAA9988, 0xFFEEDDCC,
        0x76543210, 0xFEDCBA98, 0x89ABCDEF, 0x01234567
    };
    static BYTE     byCRTC[] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80,
        0x0B, 0x3E, 0x00, 0x40, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xEA, 0x8C,
        0xDF, 0x28, 0x00, 0xE7, 0x04, 0xE3,
        0xFF
    };
    static PARMENTRY    parmX = {                   // 320x400x8
        0x50, 0x1D, 0x10,
        0xFF, 0xFF,
        {0x01, 0x0F, 0x00, 0x06},                   // SEQ 1..4
        0x63,                                               // Misc
        {0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80,    // CRTC 0..18h
        0xBF, 0x1F, 0x20, 0x40, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00, 0x9C, 0x0E,
        0x8F, 0x28, 0x00, 0x96, 0xB9, 0xE3,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,    // ATC 0..13h
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
        0x0F, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x40,    // GDC 0..8
        0x05, 0x0F, 0xFF}
    };
    static PARMENTRY    parmSmallX = {
        0x28, 0x1D, 0x10,
        0xFF, 0xFF,
        {0x01, 0x0F, 0x00, 0x06},                   // SEQ 1..4
        0x63,                                               // Misc
        {0x2D, 0x27, 0x28, 0x90, 0x2B, 0x80,    // CRTC 0..18h
        0x17, 0x10, 0x20, 0x40, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00, 0x15, 0x06,
        0x13, 0x14, 0x00, 0x14, 0x17, 0xE3,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,    // ATC 0..13h
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
        0x0F, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x40,    // GDC 0..8
        0x05, 0x0F, 0xFF}
    };
    static DWORD    tblPattern1[] = {0x0C080400, 0x0D090501, 0x0E0A0602, 0x0F0B0703};
    static DWORD    tblPattern2[] = {0x1C181410, 0x1D191511, 0x1E1A1612, 0x1F1B1713};
    int         nErr, nState, nScans, nRowOffset, nFrames;
    BOOL        bIndexed, bEnd, bFullVGA;
    SEGOFF      lpVideoA0, lpVideoB8, lpROM;
    WORD        i, wROMSize, wWPort, wRPort, wCRTC;
    BYTE        bySum, byIdx, byMask, byAct, byExp;
    DWORD       dw, dwAct, dwExp;
    FILE        *hIO;
    LPBYTE      lpBurstData;
    int         arg1, arg2, arg3, arg4, arg5, arg6;
    char        szFilename[300] = "rand.txt";
    char        szBuffer[500];

    nErr = ERROR_NONE;
    bFullVGA = SimGetFrameSize ();

    lpVideoA0 = (SEGOFF) 0xA0000000;
    lpVideoB8 = (SEGOFF) 0xB8000000;
    lpROM = (SEGOFF) 0xC0000000;
    lpBurstData = (LPBYTE) &dwBurstData;        // For BYTE accesses

    // See if the file exists
    GetDataFilename (szFilename, sizeof (szFilename));
    hIO = fopen (szFilename, "r");
    if (hIO == NULL)
    {
        sprintf (szBuffer, "Random access I/O file \"%s\" not found.\n", szFilename);
    }
    else
    {
        sprintf (szBuffer, "Random access I/O file \"%s\" found.\n", szFilename);
        fclose (hIO);
    }
    SimComment (szBuffer);

    SimSetState (TRUE, FALSE, FALSE);
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);

    // If a ROM exists (first two bytes are 055h and 0AAh) then, do a
    // POST-like sequence that includes a checksum verification and a
    // ROM copy into system memory. On a PCI-bus this should involve
    // burst memory reads.
    if (MemWordRead (lpROM) == 0xAA55)
    {
        SimComment ("Testing ROM accesses.");
        wROMSize = (MemByteRead (lpROM + 2)) * 512;
        bySum = 0;
        for (i = 0; i < wROMSize; i++)
            bySum += MemByteRead (lpROM + i);

        if (bySum != 0)
        {
            nErr = FlagError (ERROR_CHECKSUM, REF_PART, REF_TEST, 0, 0, 0x00, bySum);
            goto RandomAccessTest_exit;
        }

        // Now do a PCI-like ROM image transfer
        bySum = 0;
        for (i = 0; i < wROMSize; i += 4)
        {
            dw = MemDwordRead (lpROM + i);
            bySum += LOBYTE (LOWORD (dw));
            bySum += HIBYTE (LOWORD (dw));
            bySum += LOBYTE (HIWORD (dw));
            bySum += HIBYTE (HIWORD (dw));
        }

        if (bySum != 0)
        {
            nErr = FlagError (ERROR_CHECKSUM, REF_PART, REF_TEST, 0, 0, 0x00, bySum);
            goto RandomAccessTest_exit;
        }
    }
    else
        SimComment ("No ROM image found - No ROM accesses tested.");

    // Unlock registers
    IOByteWrite (CRTC_CINDEX, 0x11);
    IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) & 0x7F));
#ifdef LW_MODS
    IOWordWrite (CRTC_CINDEX, 0x573F);                              // Unlock extended registers
    IOByteWrite (CRTC_CINDEX, LW_CIO_CRE_RPC1__INDEX);
    IOByteWrite (CRTC_CDATA, (IOByteRead (CRTC_CDATA) & 0xF7));     // Enable legacy registers for R/W test to work
#endif

    // Perform a random I/O read/write sequence based on a script file
    // that includes whether to just read, just write or do a full test.
    // Read the pseudo-random I/O sequence from a file called "RAND.TXT"
    hIO = fopen (szFilename, "r");
    if (hIO == NULL)
    {
        SimComment ("Random access script file not found.");
        nErr = FlagError (ERROR_FILE, REF_PART, REF_TEST, 0, 0, 0, errno);
        goto RandomAccessTest_exit;
    }

    SimComment ("Perform random I/O test.");
    while (!feof (hIO))
    {
        // For compatibility with most C compilers and 16-bit vs. 32-bit
        // integers, the arguments passed to "fscanf" must all be "int".
        if (6 != fscanf (hIO, "%02X %04X %04X %02X %02X %02X", &arg1, &arg2, &arg3, &arg4, &arg5, &arg6))
        {
            fclose(hIO);
            goto RandomAccessTest_exit;
        }
        nState = arg1;
        wWPort = arg2;
        wRPort = arg3;
        bIndexed = arg4;
        byIdx = arg5;
        byMask = arg6;

        if (!IOTest (nState, wWPort, wRPort, bIndexed, byIdx, byMask))
        {
            fclose (hIO);
            goto RandomAccessTest_exit;
        }
    }
    fclose (hIO);

    // At this point the VGA is in an unknown state due to the random
    // I/O writes that have oclwrred. Set it back to a known state.
    SimSetState (TRUE, FALSE, FALSE);
    SetMode (0x92);                         // Don't write to memory
    SimSetState (TRUE, TRUE, TRUE);

    // Perform a variety of operations ilwolving different mixes of memory
    // and I/O accesses.
    // First, do a burst memory write with a couple of "random" memory reads.
    SimComment ("Do a burst memory write with a couple of \"random\" memory reads.");
    BurstMemoryWrites (lpVideoA0, dwBurstData, 8);
    byAct = MemByteRead (lpVideoA0 + 3);
    byExp = *(lpBurstData + 3);
    if (byAct != byExp)
    {
        nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, HIWORD (lpVideoA0), 3, byExp, byAct);
        goto RandomAccessTest_exit;
    }
    byAct = MemByteRead (lpVideoA0 + 21);
    byExp = *(lpBurstData + 21);
    if (byAct != byExp)
    {
        nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, HIWORD (lpVideoA0), 21, byExp, byAct);
        goto RandomAccessTest_exit;
    }

    // Now do a burst of memory writes with an I/O read operation in the middle
    // followed by a complete verification of memory
    SimComment ("Now do a burst of memory writes with an I/O read operation in the middle followed by a complete verification of memory");
    BurstMemoryWrites (lpVideoA0, dwBurstData, 4);
    IOByteRead (0x3DA);
    BurstMemoryWrites (lpVideoA0 + 16, &dwBurstData[4], 4);
    for (i = 0; i < 32; i++)
    {
        byAct = MemByteRead (lpVideoA0 + i);
        byExp = *(lpBurstData + i);
        if (byAct != byExp)
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, HIWORD (lpVideoA0), i, byExp, byAct);
            goto RandomAccessTest_exit;
        }
    }

    // Now do a burst of memory writes with an I/O write operation in the
    // middle that switches the memory address from A0000h to B8000h. Switch
    // back to A0000h before reading memory back as DWORD's.
    SimComment ("Now do a burst of memory writes with an I/O write operation in the middle that switches the memory address from A0000h to B8000h. Switch back to A0000h before reading memory back as DWORD's.");
    BurstMemoryWrites (lpVideoA0, dwBurstData, 4);
    IOWordWrite (0x3CE, 0x0D06);
    BurstMemoryWrites (lpVideoB8 + 16, &dwBurstData[4], 4);
    IOWordWrite (0x3CE, 0x0506);
    for (i = 0; i < 8; i++)
    {
        dwAct = MemDwordRead (lpVideoA0 + (i*4));
        dwExp = *(lpBurstData + i);
        if (dwAct != dwExp)
        {
            // Note: error handler can only handle BYTE's, not DWORD's
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, HIWORD (lpVideoA0), i*4, (BYTE) dwExp, (BYTE) dwAct);
            goto RandomAccessTest_exit;
        }
    }

    // Now write 00h to all the CRTC registers, switching the I/O address
    // between 3D4h and 3B4h. When complete, verify that it can be read
    // from both locations and then restore it to "normal" values.
    SimComment ("Now write 00h to all the CRTC registers, switching the I/O address between 3D4h and 3B4h. When complete, verify that it can be read from both locations and then restore it to \"normal\" values.");
    wCRTC = 0x3D4;
    IOWordWrite (wCRTC, 0x0011);            // Unwrite-protect the CRTC
    for (i = 0; i < sizeof (byCRTC); i++)
    {
        // Switch the CRTC address
        if (wCRTC == 0x3D4)
        {
            IOByteWrite (0x3C2, 0x66);
            wCRTC = 0x3B4;
        }
        else
        {
            IOByteWrite (0x3C2, 0x67);
            wCRTC = 0x3D4;
        }

        if (i % 3)                              // On every 3rd access, do a WORD write
            IOWordWrite (wCRTC, i);
        else
        {
            IOByteWrite (wCRTC, (BYTE) i);
            IOByteWrite (wCRTC + 1, 0x00);
        }
    }
    // Verify all is 00h
    IOByteWrite (0x3C2, 0x67);              // Set back to 3D4h address
    for (i = 0; i < sizeof (byCRTC); i++)
    {
        IOByteWrite (wCRTC, (BYTE) i);
        byAct = IOByteRead (wCRTC + 1);
        if (byAct != 0x00)
        {
            nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, wCRTC, i, 0, byAct);
            goto RandomAccessTest_exit;
        }
    }
    // Restore the CRTC
    for (i = 0; i < sizeof (byCRTC); i++)
        IOWordWrite (0x3D4, (((WORD) byCRTC[i]) << 8) | i);

    // Some Mode X type games will perform DWORD accesses to video memory
    // and switch planes four times to write 16-pixel "strips". Write a
    // strip of data two-DWORDs wide and verify that the video memory
    // is correct. Perform this for a given number of frames.
    // First, set mode X (assume the VGA is already in Mode 12h)
    SimComment ("Set mode X ");
    if (bFullVGA)
    {
        SetRegs (&parmX);
        nScans = 400;
        nRowOffset = 320/4;
    }
    else
    {
        SetRegs (&parmSmallX);
        nScans = 20;
        nRowOffset = 160/4;
    }

    // Loop thru a number of frames, while writing strips and reading them
    // back for accuracy.
    SimComment ("Loop thru a number of frames, while writing strips and reading them back for accuracy.");
    nFrames = 5;
    bEnd = FALSE;
    while (!bEnd)
    {
        // Stick this condition inside the loop so that - for debugging
        // purposes - a wait for "kbhit" can be inserted.
        if (--nFrames <= 0) bEnd = TRUE;

        WaitVerticalRetrace ();
        ClearModeX (bFullVGA);
        WritePatternStrip ((LPBYTE)(size_t)0xA0000028, nScans, nRowOffset, tblPattern1, bFullVGA);
        WritePatternStrip ((LPBYTE)(size_t)0xA000002C, nScans, nRowOffset, tblPattern2, bFullVGA);
        // Assume g_T0920_lpErrAddr contains 32 bit value:
        DWORD dg_T0920_lpErrAddr = (DWORD)((size_t)g_T0920_lpErrAddr);
        if (!VerifyPatternStrip ((LPBYTE)(size_t)0xA0000028, nScans, nRowOffset, tblPattern1, bFullVGA))
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (dg_T0920_lpErrAddr), HIWORD (dg_T0920_lpErrAddr), g_T0920_dwErrExp, g_T0920_dwErrAct);
            break;
        }
        if (!VerifyPatternStrip ((LPBYTE)(size_t)0xA000002C, nScans, nRowOffset, tblPattern2, bFullVGA))
        {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (dg_T0920_lpErrAddr), HIWORD (dg_T0920_lpErrAddr), g_T0920_dwErrExp, g_T0920_dwErrAct);
            break;
        }
    }

RandomAccessTest_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Random Access Test passed.");
    else
        SimComment ("Random Access Test failed.");
    SystemCleanUp ();
    return (nErr);
}

//
//      IOTest - Test a specific read/write I/O port
//
//      Entry:  state       I/O action (0: Read, 1: Write, 2: Test)
//              wport       WRITE address of port
//              rport       READ address of port
//              bIndexed    Index flag (TRUE = Indexed register, FALSE = Not)
//              idx         Index register
//              mask        Mask of unused bits
//      Exit:   <int>       DOS ERRORLEVEL value
//
int IOTest (BYTE state, WORD wport, WORD rport, BOOL bIndexed, BYTE idx, BYTE mask)
{
    int         nErr;
    BYTE        temp;
    static BYTE data = 0x55;

    nErr = ERROR_NONE;

    switch (state)
    {
        case 0:                                         // Just read (ignore index)

            IOByteRead (rport);
            break;

        case 1:                                         // Just write

            if (wport == ATC_INDEX)
            {
                ClearIObitDataBus ();
                if (bIndexed)
                {
                    IOByteWrite (wport, idx);
                    ClearIObitDataBus ();
                }
            }
            else if (bIndexed)
                IOByteWrite (wport - 1, idx);

            IOByteWrite (wport, data & mask);
            break;

        case 2:                                         // Test the port

            if (wport == ATC_INDEX)
            {
                ClearIObitDataBus ();               // Set index state
                if (bIndexed)
                {
                    IOByteWrite (wport, idx);
                    ClearIObitDataBus ();           // Set back to index state
                }
            }
            else if (bIndexed)
                IOByteWrite (wport - 1, idx);

            if ((temp = IsIObitFunctional (rport, wport, (BYTE) ~mask)) != 0x00)
                nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, rport, idx, 0, temp);
            break;
    }

    return (nErr);
}

//
//      BurstMemoryWrites - Write a block of DWORD data to memory
//
//      Entry:  lpVideo     Pointer to memory
//              lpdwData        Pointer to data table
//              nCount      Number of DWORD's in the table
//      Exit:   None
//
void BurstMemoryWrites (SEGOFF lpVideo, LPDWORD lpdwData, int nCount)
{
    int i;

    for (i = 0; i < nCount; i++)
    {
        MemDwordWrite (lpVideo, *lpdwData);
        lpdwData++;
        lpVideo += 4;
    }
}

//
//      ClearModeX - Clear Mode X memory
//
//      Entry:  bPhysical   Physical usage flag (TRUE = Use physical, FALSE = Use library)
//      Exit:   None
//
void ClearModeX (BOOL bPhysical)
{
    LPDWORD     lpVideo;
    long int    i;

    lpVideo = (LPDWORD)(size_t)0xA0000000;

    IOWordWrite (0x3C4, 0x0F02);
    for (i = 0; i < 0x4000; i++)
    {
        if (bPhysical)
        {
            *lpVideo++ = 0;
        }
        else
        {
            MemDwordWrite ((SEGOFF)((size_t)lpVideo), 0);
            lpVideo++;
        }
    }
}

//
//      WritePatternStrip - Write a strip of DWORD patterns
//
//      Entry:  lpStart     Start address on first scan
//              nScans      Number of scan lines
//              nRowOffset  Row offset between scans
//              lpPattern   Array of DWORD patterns for plane 0,1,2,3
//              bPhysical   Physical usage flag (TRUE = Use physical, FALSE = Use library)
//      Exit:   None
//
void WritePatternStrip (LPBYTE lpStart, int nScans, int nRowOffset, DWORD *lpPattern, BOOL bPhysical)
{
    int     i;
    LPDWORD lpVideo;

    lpVideo = (LPDWORD) lpStart;
    IOByteWrite (0x3C4, 0x02);
    for (i = 0; i < nScans; i++)
    {
#ifdef __X86_16__
        if (bPhysical)
        {
            _outp (0x3C5, 0x01);
            *lpVideo = *lpPattern;
            _outp (0x3C5, 0x02);
            *lpVideo = *(lpPattern + 1);
            _outp (0x3C5, 0x04);
            *lpVideo = *(lpPattern + 2);
            _outp (0x3C5, 0x08);
            *lpVideo = *(lpPattern + 3);
        }
        else
        {
            IOByteWrite (0x3C5, 0x01);
            MemDwordWrite ((SEGOFF) lpVideo, *lpPattern);
            IOByteWrite (0x3C5, 0x02);
            MemDwordWrite ((SEGOFF) lpVideo, *(lpPattern + 1));
            IOByteWrite (0x3C5, 0x04);
            MemDwordWrite ((SEGOFF) lpVideo, *(lpPattern + 2));
            IOByteWrite (0x3C5, 0x08);
            MemDwordWrite ((SEGOFF) lpVideo, *(lpPattern + 3));
        }
#else
        IOByteWrite (0x3C5, 0x01);
        MemDwordWrite ((SEGOFF)(size_t)lpVideo, *lpPattern);
        IOByteWrite (0x3C5, 0x02);
        MemDwordWrite ((SEGOFF)(size_t)lpVideo, *(lpPattern + 1));
        IOByteWrite (0x3C5, 0x04);
        MemDwordWrite ((SEGOFF)(size_t)lpVideo, *(lpPattern + 2));
        IOByteWrite (0x3C5, 0x08);
        MemDwordWrite ((SEGOFF)(size_t)lpVideo, *(lpPattern + 3));
#endif
        lpVideo += nRowOffset / 4;
    }
}

//
//      VerifyPatternStrip - Verify a strip of DWORD patterns
//
//      Entry:  lpStart     Start address on first scan
//              nScans      Number of scan lines
//              nRowOffset  Row offset between scans
//              lpPattern   Array of DWORD patterns for plane 0,1,2,3
//              bPhysical   Physical usage flag (TRUE = Use physical, FALSE = Use library)
//      Exit:   <BOOL>      Error flag (TRUE = Passed, FALSE = Error)
//
BOOL VerifyPatternStrip (LPBYTE lpStart, int nScans, int nRowOffset, DWORD *lpPattern, BOOL bPhysical)
{
    int         i, nPlane;
    LPDWORD     lpVideo;
    DWORD       dwActual;
    BOOL        bErr;

    bErr = FALSE;
    lpVideo = (LPDWORD) lpStart;
    IOByteWrite (0x3CE, 0x04);
    for (i = 0; i < nScans; i++)
    {
#ifdef __X86_16__
        if (bPhysical)
        {
            for (nPlane = 0; nPlane < 4; nPlane++)
            {
                _outp (0x3CF, nPlane);
                dwActual = *lpVideo;
                if (*lpVideo != *(lpPattern + nPlane)) goto VerifyPatternStrip_error;
            }
        }
        else
        {
            for (nPlane = 0; nPlane < 4; nPlane++)
            {
                IOByteWrite (0x3CF, nPlane);
                dwActual = MemDwordRead ((SEGOFF) lpVideo);
                if (dwActual != *(lpPattern + nPlane)) goto VerifyPatternStrip_error;
            }
        }
#else
        for (nPlane = 0; nPlane < 4; nPlane++)
        {
            IOByteWrite (0x3CF, nPlane);
            dwActual = MemDwordRead ((SEGOFF)(size_t)lpVideo);
            if (dwActual != *(lpPattern + nPlane)) goto VerifyPatternStrip_error;
        }
#endif
        lpVideo += nRowOffset / 4;
    }

VerifyPatternStrip_exit:
    return (!bErr);

VerifyPatternStrip_error:
    g_T0920_nErrPlane = nPlane;
    g_T0920_lpErrAddr = (LPBYTE) lpVideo;
    bErr = TRUE;
    g_T0920_dwErrAct = dwActual;
    g_T0920_dwErrExp = *(lpPattern + nPlane);
    goto VerifyPatternStrip_exit;
}

#undef  REF_TEST
#define REF_TEST        21
//
//      T0921
//      VLoad2Test - Verify interaction between VLoad/2, Chain/2 and WORD mode
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int VLoad2Test (void)
{
    static PARMENTRY    parm12X = {                 // Strange VLoad/2 Mode
        0x50, 0x1D, 0x10,
        0x00, 0xA0,
        {0x05, 0x0F, 0x00, 0x04},                   // SEQ 1..4
        0xE3,                                               // Misc
        {0x5F, 0x4F, 0x51, 0xA0, 0x56, 0x00,    // CRTC 0..18h
        0x0B, 0x3E, 0x00, 0x40, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x00, 0xEA, 0x8C,
        0xDF, 0x14, 0x00, 0xE7, 0x04, 0xAB,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x3F,    // ATC 0..13h
        0x14, 0x07, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x01, 0x3F,
        0x05, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    // GDC 0..8
        0x07, 0x0F, 0xFF}
    };
    int     nErr;

    nErr = ERROR_NONE;
    SetMode (0x12);                                 // Put the VGA into a known state

    // Load the registers
    SetRegs (&parm12X);

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto VLoad2Test_exit;
        }
    }

VLoad2Test_exit:
    SystemCleanUp ();
    return (nErr);
}

#ifdef LW_MODS
}
#endif

//
//      Copyright (c) 1994-2004 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, Inc.
//      All rights reserved.
//
