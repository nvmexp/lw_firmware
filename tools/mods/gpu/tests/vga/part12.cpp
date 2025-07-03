//
//      PART12.CPP - VGA Core Test Suite (Part 12)
//      Copyright (c) 1994-2004 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, Inc.
//      All rights reserved.
//
//      Written by:     Larry Coffey, John Weidman
//      Date:           2 August 2005
//      Last modified:  14 December 2005
//
//      Routines in this file:
//      FBPerfMode03TestLW50                Performance test for mode 3
//      FBPerfMode12TestLW50                Performance test for mode 12
//      FBPerfMode13TestLW50                Performance test for mode 13
//      FBPerfMode101TestLW50               Performance test for mode 101
//
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <errno.h>
#ifdef LW_MODS
#include    "crccheck.h"
#include    "core/include/disp_test.h"
#include    "core/include/gpu.h"
#include    "gpu/include/gpusbdev.h"
#include    "core/include/platform.h"
#endif
#include    "vgacore.h"
#include    "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

// Function prototypes of the global tests
int FBPerfMode03TestLW50 (void);
int FBPerfMode12TestLW50 (void);
int FBPerfMode13TestLW50 (void);
int FBPerfMode101TestLW50 (void);

// Global variables used by tests in this module
#ifdef LW_MODS
static DWORD            head_index = 0;                 // Used by FBPerfMode03TestLW50, FBPerfMode12TestLW50, FBPerfMode13TestLW50, FBPerfMode101TestLW50
static DWORD            dac_index = 0;                  // Used by FBPerfMode03TestLW50, FBPerfMode12TestLW50, FBPerfMode13TestLW50, FBPerfMode101TestLW50
#endif

#define REF_PART        12
#define REF_TEST        1
//
//      T1201
//      FBPerfMode03TestLW50 - Performance test for mode 3
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int FBPerfMode03TestLW50 (void)
{
    int         nLength, offset_end, nTrigger, i;
    SEGOFF      lpVideo;
    DWORD       dwOverheadWrite, dwOverheadRead, dwOverheadMove, dwTriggers[10];
    double      fMbSec;
    BOOL        bSim;
    static const char *tblPerfMsgs[] =
    {
        "Byte writes",
        "Word writes",
        "Dword writes",
        "Byte reads",
        "Word reads",
        "Dword reads",
        "Byte moves",
        "Word moves",
        "Dword moves"
    };

    // Initialize locals
    lpVideo = (SEGOFF) 0xB8000000;
    SimSetFrameSize (TRUE);                             // *** Force full frame values ***
    nTrigger = 0;
    bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;
    if (bSim)
        offset_end = nLength = 512;
    else
        offset_end = nLength = 1024*16;

#ifdef LW_MODS
    {
        char        szMsg[512];

        // Tell the user where the test is running
        LegacyVGA::CRCManager ()->GetHeadDac (&head_index, &dac_index);     // These variables are global to this module
        sprintf (szMsg, "Starting with head index %d and dac index %d.", head_index, dac_index);
        SimComment (szMsg);
        DispTest::IsoInitVGA (head_index, dac_index, 720, 400, 1, 1, false, 0, 28322, 0, 0, DispTest::GetGlobalTimeout ());
    }
#endif

    // Set the mode
    VBESetMode (0x03);                                  // Do a true BIOS modeset on physical systems
    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    SetMode (0x03);
    IOByteRead (INPUT_CSTATUS_1);                       // Flush MODS

    // Callwlate overhead
    PerfStart (NULL);
    PerfWrite (PERF_BYTE, lpVideo, PERF_BYTE, 0x00);
    PerfStop (NULL, &dwOverheadWrite);
    PerfStart (NULL);
    PerfRead (PERF_BYTE, lpVideo, PERF_BYTE);
    PerfStop (NULL, &dwOverheadRead);
    PerfStart (NULL);
    PerfMove (PERF_BYTE, lpVideo, lpVideo + offset_end, PERF_BYTE);
    PerfStop (NULL, &dwOverheadMove);

    //
    // Measure BYTE writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_BYTE, lpVideo, nLength, 0x41);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    //
    // Measure WORD writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_WORD, lpVideo, nLength, 0x0741);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    //
    // Measure DWORD writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_DWORD, lpVideo, nLength, 0x07420741);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    //
    // Measure BYTE reads
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfRead (PERF_BYTE, lpVideo, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadRead;

    //
    // Measure WORD reads
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfRead (PERF_WORD, lpVideo, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadRead;

    //
    // Measure DWORD reads
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfRead (PERF_DWORD, lpVideo, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadRead;

    //
    // Measure BYTE reads/writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfMove (PERF_BYTE, lpVideo, lpVideo + offset_end, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadMove;

    //
    // Measure WORD reads/writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfMove (PERF_WORD, lpVideo, lpVideo + offset_end, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadMove;

    //
    // Measure DWORD reads/writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfMove (PERF_DWORD, lpVideo, lpVideo + offset_end, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadMove;

    //
    // Clean up for exit
    //
    SystemCleanUp ();
#ifdef LW_MODS
    DispTest::IsoShutDowlwGA (head_index, dac_index, DispTest::GetGlobalTimeout ());
#endif

    //
    // Display the results
    //
    printf ("\nResults of mode 3 performance testing (memory length = %d bytes):\n", nLength);
    for (i = 0; i < nTrigger; i++)
    {
        fMbSec = ((double) nLength) / ((double) dwTriggers[i]);
        printf ("  %s: %u microseconds (%f MB/sec)\n", tblPerfMsgs[i], dwTriggers[i], fMbSec);
    }

    SimComment ("Mode 3 Performance Test completed.");
    return ERROR_NONE;
}

#undef  REF_TEST
#define REF_TEST        2
//
//      T1202
//      FBPerfMode12TestLW50 - Performance test for mode 12
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int FBPerfMode12TestLW50 (void)
{
    int         nLength, offset_end, nTrigger, i;
    SEGOFF      lpVideo;
    DWORD       dwOverheadWrite, dwOverheadRead, dwOverheadMove, dwTriggers[20];
    double      fMbSec;
    BOOL        bSim;
    static BYTE tblMapMasks[] = {0x0F, 0x01, 0x02, 0x04, 0x08, 0x05, 0x0A};
    static const char *tblPerfMsgs[] =
    {
        "Byte writes with mask 0Fh",
        "Byte writes with mask 01h",
        "Byte writes with mask 02h",
        "Byte writes with mask 04h",
        "Byte writes with mask 08h",
        "Byte writes with mask 05h",
        "Byte writes with mask 0Ah",
        "Byte writes in write mode 2",
        "Word writes",
        "Dword writes",
        "Byte reads from plane 0",
        "Byte reads from plane 1",
        "Byte reads from plane 2",
        "Byte reads from plane 3",
        "Word reads",
        "Dword reads",
        "Byte moves (latch copy)",
        "Byte moves (write mode 1)"
    };

    // Initialize locals
    lpVideo = (SEGOFF) 0xA0000000;
    SimSetFrameSize (TRUE);                             // *** Force full frame values ***
    nTrigger = 0;
    bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;
    if (bSim)
        offset_end = nLength = 512;
    else
        offset_end = nLength = 1024*16;

#ifdef LW_MODS
    {
        char        szMsg[512];

        // Tell the user where the test is running
        LegacyVGA::CRCManager ()->GetHeadDac (&head_index, &dac_index);     // These variables are global to this module
        sprintf (szMsg, "Starting with head index %d and dac index %d.", head_index, dac_index);
        SimComment (szMsg);
        DispTest::IsoInitVGA (head_index, dac_index, 640, 480, 1, 1, false, 0, 25175, 0, 0, DispTest::GetGlobalTimeout ());
    }
#endif

    // Set the mode
    VBESetMode (0x12);                                  // Do a true BIOS modeset on physical systems
    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    SetMode (0x12);
    IOByteRead (INPUT_CSTATUS_1);                       // Flush MODS

    // Callwlate overhead
    PerfStart (NULL);
    PerfWrite (PERF_BYTE, lpVideo, PERF_BYTE, 0x00);
    PerfStop (NULL, &dwOverheadWrite);
    PerfStart (NULL);
    PerfRead (PERF_BYTE, lpVideo, PERF_BYTE);
    PerfStop (NULL, &dwOverheadRead);
    PerfStart (NULL);
    PerfMove (PERF_BYTE, lpVideo, lpVideo + offset_end, PERF_BYTE);
    PerfStop (NULL, &dwOverheadMove);

    ////////////////////////////////////////////////////////////////////////
    // Measure WRITE performance
    //
    // Measure BYTE writes for each of the maps
    for (i = 0; i < (int) sizeof (tblMapMasks); i++)
    {
        // Set the map mask
        IOByteWrite (SEQ_INDEX, 0x02);
        IOByteWrite (SEQ_DATA, tblMapMasks[i]);

        //
        // Measure BYTE writes with various map masks
        //
        PerfStart (tblPerfMsgs[nTrigger]);
        PerfWrite (PERF_BYTE, lpVideo, nLength, 0x55);
        PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
        dwTriggers[nTrigger++] -= dwOverheadWrite;
    }

    // Set the map mask back to all planes
    IOWordWrite (SEQ_INDEX, 0x0F02);

    // Set write mode 2
    IOWordWrite (GDC_INDEX, 0x0205);

    //
    // Measure BYTE writes in write mode 2
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_BYTE, lpVideo, nLength, 0x55);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    // Set back to write mode 0
    IOWordWrite (GDC_INDEX, 0x0005);

    //
    // Measure WORD writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_WORD, lpVideo, nLength, 0x0741);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    //
    // Measure DWORD writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_DWORD, lpVideo, nLength, 0x07420741);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    ////////////////////////////////////////////////////////////////////////
    // Measure READ performance
    //
    for (i = 0; i < 4; i++)
    {
        IOByteWrite (GDC_INDEX, 0x04);
        IOByteWrite (GDC_DATA, i);          // Set read map select

        //
        // Measure BYTE reads from each of the planes
        //
        PerfStart (tblPerfMsgs[nTrigger]);
        PerfRead (PERF_BYTE, lpVideo, nLength);
        PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
        dwTriggers[nTrigger++] -= dwOverheadRead;
    }

    // Set back to read map 0
    IOWordWrite (GDC_INDEX, 0x0004);

    //
    // Measure WORD reads
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfRead (PERF_WORD, lpVideo, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadRead;

    //
    // Measure DWORD reads
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfRead (PERF_DWORD, lpVideo, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadRead;

    ////////////////////////////////////////////////////////////////////////
    // Measure READ/WRITE performance
    //
    // Set the map mask to no planes (causes latches to be copied)
    IOWordWrite (SEQ_INDEX, 0x0002);

    //
    // Measure BYTE reads/writes (latch copy)
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfMove (PERF_BYTE, lpVideo, lpVideo + offset_end, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadMove;

    // Set the map mask back to all planes and set write mode 1 (latch copy)
    // Note: the previous read/write and this read/write sequence should have identical times
    IOWordWrite (SEQ_INDEX, 0x0F02);
    IOWordWrite (GDC_INDEX, 0x0105);

    //
    // Measure BYTE reads/writes (write mode 1)
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfMove (PERF_BYTE, lpVideo, lpVideo + offset_end, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadMove;

    //
    // Clean up for exit
    //
    SystemCleanUp ();
#ifdef LW_MODS
    DispTest::IsoShutDowlwGA (head_index, dac_index, DispTest::GetGlobalTimeout ());
#endif

    //
    // Display the results
    //
    printf ("\nResults of mode 12 performance testing (memory length = %d bytes):\n", nLength);
    for (i = 0; i < nTrigger; i++)
    {
        fMbSec = ((double) nLength) / ((double) dwTriggers[i]);
        printf ("  %s: %u microseconds (%f MB/sec)\n", tblPerfMsgs[i], dwTriggers[i], fMbSec);
    }

    SimComment ("Mode 12 Performance Test completed.");
    return ERROR_NONE;
}

#undef  REF_TEST
#define REF_TEST        3
//
//      T1203
//      FBPerfMode13TestLW50 - Performance test for mode 13
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int FBPerfMode13TestLW50 (void)
{
    int         nLength, offset_end, nTrigger, i;
    SEGOFF      lpVideo;
    DWORD       dwOverheadWrite, dwOverheadRead, dwOverheadMove, dwTriggers[10];
    double      fMbSec;
    BOOL        bSim;
    static const char *tblPerfMsgs[] =
    {
        "Byte writes",
        "Word writes",
        "Dword writes",
        "Byte reads",
        "Word reads",
        "Dword reads",
        "Byte moves",
        "Word moves",
        "Dword moves"
    };

    // Initialize locals
    lpVideo = (SEGOFF) 0xA0000000;
    SimSetFrameSize (TRUE);                             // *** Force full frame values ***
    nTrigger = 0;
    bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;
    if (bSim)
        offset_end = nLength = 512;
    else
        offset_end = nLength = 1024*16;

#ifdef LW_MODS
    {
        char        szMsg[512];

        // Tell the user where the test is running
        LegacyVGA::CRCManager ()->GetHeadDac (&head_index, &dac_index);     // These variables are global to this module
        sprintf (szMsg, "Starting with head index %d and dac index %d.", head_index, dac_index);
        SimComment (szMsg);
        DispTest::IsoInitVGA (head_index, dac_index, 640, 400, 1, 1, false, 0, 25175, 0, 0, DispTest::GetGlobalTimeout ());
    }
#endif

    // Set the mode
    VBESetMode (0x13);                                  // Do a true BIOS modeset on physical systems
    SimSetState (TRUE, TRUE, FALSE);                    // Ignore DAC writes
    SetMode (0x13);
    IOByteRead (INPUT_CSTATUS_1);                       // Flush MODS

    // Callwlate overhead
    PerfStart (NULL);
    PerfWrite (PERF_BYTE, lpVideo, PERF_BYTE, 0x00);
    PerfStop (NULL, &dwOverheadWrite);
    PerfStart (NULL);
    PerfRead (PERF_BYTE, lpVideo, PERF_BYTE);
    PerfStop (NULL, &dwOverheadRead);
    PerfStart (NULL);
    PerfMove (PERF_BYTE, lpVideo, lpVideo + offset_end, PERF_BYTE);
    PerfStop (NULL, &dwOverheadMove);

    //
    // Measure BYTE writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_BYTE, lpVideo, nLength, 0x41);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    //
    // Measure WORD writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_WORD, lpVideo, nLength, 0x0741);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    //
    // Measure DWORD writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_DWORD, lpVideo, nLength, 0x07420741);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    //
    // Measure BYTE reads
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfRead (PERF_BYTE, lpVideo, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadRead;

    //
    // Measure WORD reads
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfRead (PERF_WORD, lpVideo, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadRead;

    //
    // Measure DWORD reads
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfRead (PERF_DWORD, lpVideo, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadRead;

    //
    // Measure BYTE reads/writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfMove (PERF_BYTE, lpVideo, lpVideo + offset_end, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadMove;

    //
    // Measure WORD reads/writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfMove (PERF_WORD, lpVideo, lpVideo + offset_end, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadMove;

    //
    // Measure DWORD reads/writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfMove (PERF_DWORD, lpVideo, lpVideo + offset_end, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadMove;

    //
    // Clean up for exit
    //
    SystemCleanUp ();
#ifdef LW_MODS
    DispTest::IsoShutDowlwGA (head_index, dac_index, DispTest::GetGlobalTimeout ());
#endif

    //
    // Display the results
    //
    printf ("\nResults of mode 13 performance testing (memory length = %d bytes):\n", nLength);
    for (i = 0; i < nTrigger; i++)
    {
        fMbSec = ((double) nLength) / ((double) dwTriggers[i]);
        printf ("  %s: %u microseconds (%f MB/sec)\n", tblPerfMsgs[i], dwTriggers[i], fMbSec);
    }

    SimComment ("Mode 13 Performance Test completed.");
    return ERROR_NONE;
}

#undef  REF_TEST
#define REF_TEST        4
//
//      T1204
//      FBPerfMode101TestLW50 - Performance test for mode 101
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int FBPerfMode101TestLW50 (void)
{
    static PARMENTRY    parm101full =                   // 640x480x8
    {
        0x50, 0x1D, 0x10,
        0xFF, 0xFF,
        {0x01, 0x0F, 0x00, 0x0E},                   // SEQ 1..4
        0xE3,                                               // Misc
        {0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80,    // CRTC 0..18h
        0x0B, 0x3E, 0x00, 0x40, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xEA, 0x0C,
        0xDF, 0x50, 0x00, 0xE7, 0x04, 0xE3,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,    // ATC 0..13h
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00,
        0x0F, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x40,    // GDC 0..8
        0x05, 0x0F, 0xFF}
    };
    static PARMENTRY    parm101small =
    {
        0x50, 0x1D, 0x10,
        0xFF, 0xFF,
        {0x01, 0x0F, 0x00, 0x0E},
        0xE3,
        {0x2D, 0x27, 0x28, 0x90, 0x2B, 0x80,
        0x17, 0x10, 0x00, 0x40, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x15, 0x86,
        0x13, 0x28, 0x00, 0x14, 0x17, 0xE3,
        0xFF},
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x01, 0x00,
        0x0F, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
        0x05, 0x0F, 0xFF},
    };

    int         nLength, offset_end, nTrigger, i, selFramebuffer;
    BYTE        *pBar1;
    BYTE        byData;
    WORD        wData;
    BOOL        bFullVGA, bSim, bVBEAvail;
    DWORD       dwOverheadWrite, dwOverheadRead, dwOverheadMove, dwTriggers[20];
    size_t      FramebufferOffset;
    double      fMbSec;
    SEGOFF      lpVideo;
    char        szMsg[512];
    static const char *tblPerfMsgs[] =
    {
        "Banked Byte writes",
        "Banked Word writes",
        "Banked Dword writes",
        "Banked Byte reads",
        "Banked Word reads",
        "Banked Dword reads",
        "Banked Byte reads/writes",
        "Banked Word reads/writes",
        "Banked Dword reads/writes",
        "Linear Byte writes",
        "Linear Word writes",
        "Linear Dword writes",
        "Linear Byte reads",
        "Linear Word reads",
        "Linear Dword reads",
        "Linear Byte reads/writes",
        "Linear Word reads/writes",
        "Linear Dword reads/writes",
    };
    VBEVESAINFO     vbevi;
#ifdef LW_MODS
    LwRmPtr         pLwRm;
    PHYSADDR        PhysBar1Addr;
    DWORD           dwData;
#endif

    // Initialize locals
    SimComment ("*** Starting FBPerfMode101TestLW50...");
    lpVideo = (SEGOFF) 0xA0000000;
    pBar1 = NULL;
    constexpr BOOL bBackdoor = FALSE; // Need to retrieve commandline for this - LGC
    nTrigger = 0;
//  SimSetFrameSize (TRUE);                             // *** Force full frame values ***
    bFullVGA = SimGetFrameSize ();
    bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;
    if (bSim)
        offset_end = nLength = 512;
    else
        offset_end = nLength = 1024*16;
    sprintf (szMsg, "*** Using memory length of %d bytes.", nLength);
    SimComment (szMsg);

#ifdef LW_MODS
    // Tell the user where the test is running
    LegacyVGA::CRCManager ()->GetHeadDac (&head_index, &dac_index);     // These variables are global to this module
    sprintf (szMsg, "Starting with head index %d and dac index %d.", head_index, dac_index);
    SimComment (szMsg);
    DispTest::IsoInitVGA (head_index, dac_index, 640, 480, 1, 1, false, 0, 25175, 0, 0, DispTest::GetGlobalTimeout ());
#endif

    //////////////////////
    //  Begin set 640x480x8 extended video mode
    //
    SimComment ("*** Begin set 640x480x8 extended video mode.");
    bVBEAvail = VBEGetInfo (&vbevi);
    SimSetState (TRUE, TRUE, FALSE);                // Ignore DAC writes
    if (bVBEAvail)
    {
        SimComment ("*** Set physical mode 101h.");
        VBESetMode (0x4101);                        // Use real VBIOS
    }
    else
    {
        SimSetState (TRUE, TRUE, FALSE);            // Ignore DAC writes
        SimComment ("*** Set physical mode 13h (ignored in simulation).");
        VBESetMode (0x13);                          // Use real VBIOS
        SimComment ("*** Set mode 13h.");
        SetMode (0x93);                             // Make the simulation environment match
        SimComment ("*** End set mode 13h.");
    }
    IOByteRead (INPUT_CSTATUS_1);                   // Flush MODS

    // Load the registers
    if (bSim)
    {
        SimComment ("*** Load the parameter table for the mode 101h perf test.");
        if (bFullVGA)
        {
            SimComment ("*** Use full frame mode 101h.");
            SetRegs (&parm101full);
        }
        else
        {
            SimComment ("*** Use small frame mode 101h.");
            SetRegs (&parm101small);
        }

#ifdef LW_MODS
        IOWordWrite (CRTC_CINDEX, 0x573F);                                  // Unlock extended registers
        IOByteWrite (CRTC_CINDEX, LW_CIO_CRE_RPC1__INDEX);
        IOByteWrite (CRTC_CDATA, 0x01);                                     // DRF_DEF(_CIO,_CRE_RPC1,_RSVD1,_INIT) (Disable 256K address wrap)

        IOByteWrite (CRTC_CINDEX, LW_CIO_CRE_ENH__INDEX);
        IOByteWrite (CRTC_CDATA, 0x16);                                     // 8-bit / Sequential chain 4
#endif
    }
    SimComment ("*** End set 640x480x8 extended video mode.");
    //
    //  End set 640x480x8 extended video mode
    //////////////////////

    //////////////////////
    //  Begin get memory pointer
    //
    // Get the physical memory address
    bVBEAvail = TRUE;           // Fake VBE to get linear address testing by Mods
    selFramebuffer = 0;
    FramebufferOffset = 0;

#ifdef LW_MODS
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();
    dwData = pSubdev->RegRd32 (SimGetRegAddress ("LW_PDISP_VGA_BASE"));
    if (bBackdoor)
    {
        pLwRm->MapFbMemory (dwData & 0xffffff00, 0x1000000, (void**)&pBar1, 0, pSubdev);
        Printf (Tee::PriNormal, "*** LW_PDISP_VGA_BASE = %Xh, pBar1 = %ph\n", dwData, pBar1);
    }
    else
    {
        PhysBar1Addr = DispTest::GetBoundGpuSubdevice()->GetPhysFbBase();
        MASSERT(PhysBar1Addr != Gpu::s_IlwalidPhysAddr);

        Platform::MapDeviceMemory ((void **) &pBar1, PhysBar1Addr + (dwData & 0xffffff00), 0x1000000, Memory::UC, Memory::ReadWrite);
        Printf (Tee::PriNormal, "*** PhysBar1Addr = %Xh, LW_PDISP_VGA_BASE = %Xh, pBar1 = %ph\n", (DWORD) PhysBar1Addr, dwData, pBar1);
    }
    FramebufferOffset = (size_t)pBar1;
#else
    pBar1 = (LPBYTE) FramebufferOffset;
#endif
    //
    //  End get memory pointer
    //////////////////////

#if 1
    // Flush MODS
    SimComment ("*** Flush MODS...");
    IOByteRead (CRTC_CINDEX + 6);

    // #define MEM_WR08(a, d) Platform::VirtualWr08((volatile void *)(a), d)
    sprintf (szMsg, "*** Writing %Xh to %ph", 0x5A, pBar1);
    SimComment (szMsg);
    MEM_WR08 (pBar1, 0x5A);

    // #define MEM_RD08(a) Platform::VirtualRd08((const volatile void *)(a))
    sprintf (szMsg, "*** Reading back from %ph...", pBar1);
    SimComment (szMsg);
    byData = MEM_RD08 (pBar1);
    sprintf (szMsg, "*** ...and it is: %Xh", byData);
    SimComment (szMsg);

    // Try it again as a WORD access
    MEM_WR16 (pBar1, 0xAA55);
    wData = MEM_RD16 (pBar1);
    sprintf (szMsg, "*** Word R/W test reads: %Xh (expected = 0xAA55)", wData);
    SimComment (szMsg);
#endif

    // Callwlate overhead
    PerfStart (NULL);
    PerfWriteLinear (PERF_BYTE, selFramebuffer, FramebufferOffset, PERF_BYTE, 0x00);
    PerfStop (NULL, &dwOverheadWrite);
    PerfStart (NULL);
    PerfReadLinear (PERF_BYTE, selFramebuffer, FramebufferOffset, PERF_BYTE);
    PerfStop (NULL, &dwOverheadRead);
    PerfStart (NULL);
    PerfMoveLinear (PERF_BYTE, selFramebuffer, FramebufferOffset, FramebufferOffset + offset_end, PERF_BYTE);
    PerfStop (NULL, &dwOverheadMove);

    ////////////////////////////////////////////////////////
    //
    // Banked testing
    //
    // Measure BYTE writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_BYTE, lpVideo, nLength, 0x5A);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    //
    // Measure WORD writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_WORD, lpVideo, nLength, 0x5AA5);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    //
    // Measure DWORD writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfWrite (PERF_DWORD, lpVideo, nLength, 0x5AA56996l);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadWrite;

    //
    // Measure BYTE reads
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfRead (PERF_BYTE, lpVideo, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadRead;

    //
    // Measure WORD reads
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfRead (PERF_WORD, lpVideo, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadRead;

    //
    // Measure DWORD reads
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfRead (PERF_DWORD, lpVideo, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadRead;

    //
    // Measure BYTE reads/writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfMove (PERF_BYTE, lpVideo, lpVideo + offset_end, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadMove;

    //
    // Measure WORD reads/writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfMove (PERF_WORD, lpVideo, lpVideo + offset_end, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadMove;

    //
    // Measure DWORD reads/writes
    //
    PerfStart (tblPerfMsgs[nTrigger]);
    PerfMove (PERF_DWORD, lpVideo, lpVideo + offset_end, nLength);
    PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
    dwTriggers[nTrigger++] -= dwOverheadMove;

    ////////////////////////////////////////////////////////
    //
    // Linear testing
    //
    // Measure BYTE writes
    //
    if (bVBEAvail)
    {
        PerfStart (tblPerfMsgs[nTrigger]);
        PerfWriteLinear (PERF_BYTE, selFramebuffer, FramebufferOffset, nLength, 0x55);
        PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
        dwTriggers[nTrigger++] -= dwOverheadWrite;

        //
        // Measure WORD writes
        //
        PerfStart (tblPerfMsgs[nTrigger]);
        PerfWriteLinear (PERF_WORD, selFramebuffer, FramebufferOffset, nLength, 0x0741);
        PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
        dwTriggers[nTrigger++] -= dwOverheadWrite;

        //
        // Measure DWORD writes
        //
        PerfStart (tblPerfMsgs[nTrigger]);
        PerfWriteLinear (PERF_DWORD, selFramebuffer, FramebufferOffset, nLength, 0x07420741);
        PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
        dwTriggers[nTrigger++] -= dwOverheadWrite;

        //
        // Measure BYTE reads
        //
        PerfStart (tblPerfMsgs[nTrigger]);
        PerfReadLinear (PERF_BYTE, selFramebuffer, FramebufferOffset, nLength);
        PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
        dwTriggers[nTrigger++] -= dwOverheadRead;

        //
        // Measure WORD reads
        //
        PerfStart (tblPerfMsgs[nTrigger]);
        PerfReadLinear (PERF_WORD, selFramebuffer, FramebufferOffset, nLength);
        PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
        dwTriggers[nTrigger++] -= dwOverheadRead;

        //
        // Measure DWORD reads
        //
        PerfStart (tblPerfMsgs[nTrigger]);
        PerfReadLinear (PERF_DWORD, selFramebuffer, FramebufferOffset, nLength);
        PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
        dwTriggers[nTrigger++] -= dwOverheadRead;

        //
        // Measure BYTE reads/writes
        //
        PerfStart (tblPerfMsgs[nTrigger]);
        PerfMoveLinear (PERF_BYTE, selFramebuffer, FramebufferOffset, FramebufferOffset + offset_end, nLength);
        PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
        dwTriggers[nTrigger++] -= dwOverheadMove;

        //
        // Measure WORD reads/writes
        //
        PerfStart (tblPerfMsgs[nTrigger]);
        PerfMoveLinear (PERF_WORD, selFramebuffer, FramebufferOffset, FramebufferOffset + offset_end, nLength);
        PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
        dwTriggers[nTrigger++] -= dwOverheadMove;

        //
        // Measure DWORD reads/writes
        //
        PerfStart (tblPerfMsgs[nTrigger]);
        PerfMoveLinear (PERF_DWORD, selFramebuffer, FramebufferOffset, FramebufferOffset + offset_end, nLength);
        PerfStop (tblPerfMsgs[nTrigger], &dwTriggers[nTrigger]);
        dwTriggers[nTrigger++] -= dwOverheadMove;
    }

    //
    // Clean up allocated memory space(s)
    //
#ifdef LW_MODS
    sprintf (szMsg, "*** Unmapping %ph...\n", pBar1);
    SimComment (szMsg);
    if (bBackdoor)
        pLwRm->UnmapFbMemory ((void*)pBar1, 0, pSubdev);
    else
        Platform::UnMapDeviceMemory (pBar1);
#endif

#ifdef LW_MODS
    DispTest::IsoShutDowlwGA (head_index, dac_index, DispTest::GetGlobalTimeout ());
#endif
    if (bVBEAvail)
        VBESetMode (0x03);      // May need physical BIOS to clean up extended mode stuff
    SystemCleanUp ();

    //
    // Display the results
    //
    printf ("\nResults of mode 101h performance testing (memory length = %d bytes):\n", nLength);
    for (i = 0; i < nTrigger; i++)
    {
        fMbSec = ((double) nLength) / ((double) dwTriggers[i]);
        printf ("  %s: %u microseconds (%f MB/sec)\n", tblPerfMsgs[i], dwTriggers[i], fMbSec);
    }

    SimComment ("Mode 101 Performance Test completed.");
    return ERROR_NONE;
}

#ifdef LW_MODS
}           // namespace LegacyVGA
#endif      // LW_MODS

//
//      Copyright (c) 1994-2004 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, Inc.
//      All rights reserved.
//
