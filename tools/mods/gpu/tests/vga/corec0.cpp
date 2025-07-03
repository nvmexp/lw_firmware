//
//      COREC0.CPP - VGA Core "C" Library File #1 (Miscellaneous)
//      Copyright (c) 1994-2004 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, Inc.
//      All rights reserved.
//
//      Written by:     Larry Coffey
//      Date:           17 February 1994
//      Last modified:  2 December 2005
//
//      Routines in this file:
//      SetPixelAt                  Set a pixel at a given video offset (M12 only)
//      RWMemoryTest                Test a byte of video memory (M12 only)
//      IsIObitFunctional           Test bits in a register and return a mask of failed bits
//      ClearIObitDataBus           Read input status 1 registers to change the I/O bus
//      FlagFailure                 Return a bit mask reflecting a failure of two values which are supposed to compare equal
//      IsVGAEnabled                Determine if the VGA is enabled
//      SetLwrsorPosition           Set the cursor position
//      GetKey                      Get a key from the user
//      PeekKey                     Return the ASCII portion of the key code without removing key from buffer
//      DisableLwrsor               Turn off the text mode cursor
//      EnableLwrsor                Turn on the text mode cursor
//      GetFrameRate                Get current frame rate
//      GetSystemTicks              Get the current timer tick count
//      WaitVerticalRetrace         Wait for the beginning of vertical retrace
//      WaitNotVerticalRetrace      Wait for the end of vertical retrace
//      GetVideoData                Return the video status register value
//      GetNolwideoData             Return the video status register value during non-display time (overscan data is returned)
//      RotateByteLeft              Rotate a data byte a given number of times
//      RotateByteRight             Rotate a data byte a given number of times
//      RotateWordRight             Rotate a data word a given number of times
//      MoveBytes                   Move a block of memory of a given size
//      CompareBytes                Compares two blocks of memory and returns the failure
//      VerifyBytes                 Scan a block of memory for a non-conforming value
//      SetScans                    Set the number of scan lines for text modes.
//      DisplayInspectionMessage    Display a message about visual inspection
//      FrameCapture                Capture a frame
//      StartCapture                Begin a stream of frame captures
//      EndCapture                  Terminate a stream of frame captures
//      SwapWords                   Swap two WORD variables
//      MemoryFill                  Fill memory with a byte for a given length
//      GetResolution               Return the current resolution (in pixels)
//      StringLength                Return a model independent string length
//      SystemCleanUp               Clean up and prepare for exit from the test
//      GetDataFilename             Retrieve the full path name of the data file needed by the test
//      WaitNotBlank                Wait for blanking to end
//      SetLineCompare              Set the line compare value
//
//
#include    <stdio.h>
#include    <string.h>
#include    <assert.h>
#include    "vgacore.h"
#include    "vgasim.h"

#ifdef LW_MODS
#include    "crccheck.h"
using namespace LegacyVGA;

namespace LegacyVGA {
#endif

//
//      SetPixelAt - Set a pixel at a given video offset
//
//      Entry:  lpVMem      Pointer to video memory
//              byMask      Pixel mask
//              byColor     Color of pixel
//      Exit:   None
//
//      Assume: 16-color planar mode
//
void SetPixelAt (SEGOFF lpVMem, BYTE byMask, BYTE byColor)
{
    IOByteWrite (GDC_INDEX, 0x08);
    IOByteWrite (GDC_DATA, byMask);
    IOByteWrite (SEQ_INDEX, 0x02);
    IOByteWrite (SEQ_DATA, 0x0F);

    MemByteRead (lpVMem);             // Latch data
    MemByteWrite (lpVMem, 0x00);
    IOByteWrite (SEQ_DATA, byColor);
    MemByteWrite (lpVMem, 0xFF);

    IOByteWrite (SEQ_DATA, 0x0F);
    IOByteWrite (GDC_DATA, 0xFF);
}

//
//      RWMemoryTest - Test a byte of video memory
//
//      Entry:  wSegment    Video memory segment
//              wOffset     Video memory offset
//      Exit:   <BOOL>      Success flag (TRUE = Memory OK, FALSE = Not)
//
//      Assume planar mode
//
BOOL RWMemoryTest (WORD wSegment, WORD wOffset)
{
    SEGOFF      lpVideo;
    unsigned int            i, j;
    BYTE            byOrgMask, byOrgPlane, byOrgData, byActual;
    static BYTE bytblTestValues[] = {0x00, 0xFF, 0xAA, 0x55};
    BOOL            bPassed;

    // Colwert segment:offset to a far pointer
    lpVideo = (SEGOFF) ((((DWORD) wSegment) << 16) + wOffset);

    IOByteWrite (SEQ_INDEX, 0x02);
    byOrgMask = (BYTE) IOByteRead (SEQ_DATA);
    IOByteWrite (GDC_INDEX, 0x04);
    byOrgPlane = (BYTE) IOByteRead (GDC_DATA);
    bPassed = TRUE;
    for (i = 0; i < 4; i++)
    {
        IOByteWrite (SEQ_DATA, (BYTE) (1 << i));        // Set map mask (index was preset before loop)
        IOByteWrite (GDC_DATA, (BYTE) i);               // Set read plane select (ditto on index)
        byOrgData = MemByteRead (lpVideo);              // Save original data

        for (j = 0; j < sizeof (bytblTestValues); j++)
        {
            MemByteWrite (lpVideo, bytblTestValues[j]);
            if (!MemByteTest (lpVideo, bytblTestValues[j], &byActual))
            {
                i = 5;                                          // Set outer loop counter past end
                bPassed = FALSE;                                // Flag error
                break;
            }
        }

        MemByteWrite (lpVideo, byOrgData);              // Restore original data
    }
    IOByteWrite (GDC_DATA, byOrgPlane);
    IOByteWrite (SEQ_DATA, byOrgMask);

    return (bPassed);
}

//
//      IsIObitFunctional - Test bits in a register and return a mask of failed bits
//
//      Entry:  wIOR        I/O Read address
//              wIOW        I/O Write address
//              byMask      Mask of bits to ignore
//      Exit:   <BYTE>      Bit mask of failed bits
//
BYTE IsIObitFunctional (WORD wIOR, WORD wIOW, BYTE byMask)
{
    BYTE            byBadBits, byUntouched, byNotMask, byOrgReg;
    BYTE            byExpected, byActual;
    static BYTE bytblTestValues[] = {0x00, 0xFF, 0xAA, 0x55};
    unsigned int            i;

    byBadBits = 0;
    byOrgReg = (BYTE) IOByteRead (wIOR);
    byUntouched = (BYTE) (byOrgReg & byMask);
    byNotMask = (BYTE) (~byMask);

    // Test with 0's, 1's, AAh, and 55h
    for (i = 0; i < sizeof (bytblTestValues); i++)
    {
        // Compensate for attribute controller toggle (assume "INDEX" state on entry)
        if ((wIOW == ATC_INDEX) && (wIOR == ATC_RDATA))
            IOByteWrite (ATC_INDEX, IOByteRead (ATC_INDEX));
        byExpected = (BYTE) (byUntouched | (byNotMask & bytblTestValues[i]));
        IOByteWrite (wIOW, byExpected);
        ClearIObitDataBus ();
        if (!IOByteTest (wIOR, byExpected, &byActual))
            byBadBits |= FlagFailure (byExpected, byActual, byMask);
    }

    // Restore original value (compensate for attribute controller toggle
    //  (assume "INDEX" state on entry)
    if ((wIOW == ATC_INDEX) && (wIOR == ATC_RDATA))
        IOByteWrite (ATC_INDEX, IOByteRead (ATC_INDEX));
    IOByteWrite (wIOW, byOrgReg);
    return (byBadBits);
}

//
//      ClearIObitDataBus - Read input status 1 registers to change the I/O
//                                  data bus.
//
//      Entry:  None
//      Exit:   None
//
void ClearIObitDataBus (void)
{
    IOByteRead (INPUT_MSTATUS_1);
    IOByteRead (INPUT_CSTATUS_1);
}

//
//      FlagFailure - Return a bit mask reflecting a failure of two values
//                          which are supposed to compare equal
//
//      Entry:  byExpected      Expected value
//              byActual        Actual value
//              byMask          Mask of don't care bits
//      Exit:   <BYTE>          Bit mask of failed bits
//
BYTE FlagFailure (BYTE byExpected, BYTE byActual, BYTE byMask)
{
    return ((BYTE) ((byExpected & ~byMask) ^ (byActual & ~byMask)));
}

//
//      IsVGAEnabled - Determine if the VGA is enabled
//
//      Entry:  None
//      Exit:       <BOOL>  Enabled flag (TRUE = VGA is enabled, FALSE = not)
//
BOOL IsVGAEnabled (void)
{
    BYTE        temp;

    temp = IsIObitFunctional (MISC_INPUT, MISC_OUTPUT, 0x1C);
    return ((temp == 0) ? TRUE : FALSE);
}

//
//      SetLwrsorPosition - Set the cursor position
//
//      Entry:  col     Column
//              row     Row
//              page    Page
//      Exit:   None
//
void SetLwrsorPosition (BYTE col, BYTE row, BYTE page)
{
    static SEGOFF   lpModeNumber = (SEGOFF) (0x00000449);
    static SEGOFF   lpCRTCAddr = (SEGOFF) (0x00000463);
    static SEGOFF   lpColumns = (SEGOFF) (0x0000044A);
    static SEGOFF   lpRegenLength = (SEGOFF) (0x0000044C);
    static SEGOFF   lpLwrsorPos = (SEGOFF) (0x00000450);
    WORD                wOffset, wCRTC;

    // In text modes, write the CRTC register for cursor position
    if ((MemByteRead (lpModeNumber) <= 3) ||
        (MemByteRead (lpModeNumber) == 7))
    {
        wOffset = ((MemWordRead (lpColumns) * row) + col) +
                        (page * MemWordRead (lpRegenLength));
        wCRTC = MemWordRead (lpCRTCAddr);
        IOByteWrite (wCRTC, 0x0E);
        IOByteWrite ((WORD) (wCRTC + 1), HIBYTE (wOffset));
        IOByteWrite (wCRTC, 0x0F);
        IOByteWrite ((WORD) (wCRTC + 1), LOBYTE (wOffset));
    }

    MemByteWrite (lpLwrsorPos + page*2, col);
    MemByteWrite (lpLwrsorPos + page*2 + 1, row);
}

//
//      GetKey - Get a key from the user
//
//      Entry:  None
//      Exit:   <WORD>  Key code
//
WORD GetKey (void)
{
    return (SimGetKey ());
}

//
//      PeekKey - See if a key is hit and return the ASCII portion of the key
//                      code without removing key from buffer
//
//      Entry:  None
//      Exit:   <WORD>  Key code
//
WORD PeekKey (void)
{
    WORD    wKey;

    wKey = 0;
    if (_kbhit ())
    {
        wKey = _getch ();
        _ungetch (wKey);
    }

    return (wKey);
}

//
//      DisableLwrsor - Turn off the text mode cursor
//
//      Entry:  None
//      Exit:   None
//
//      Note:   This is an "absolute" function. In other words, it can't be
//              nested, because it will always disable the cursor.
//
void DisableLwrsor (void)
{
    WORD    wCRTC;

    wCRTC = CRTC_MINDEX;
    if (IOByteRead (MISC_INPUT) & 1) wCRTC = CRTC_CINDEX;

    IOByteWrite (wCRTC, 0x0A);
    IOByteWrite ((WORD) (wCRTC + 1), (BYTE) (IOByteRead ((WORD) (wCRTC + 1)) | 0x20));
}

//
//      EnableLwrsor - Turn on the text mode cursor
//
//      Entry:  None
//      Exit:   None
//
//      Note:   This is an "absolute" function. In other words, it can't be
//              nested, because it will always enable the cursor.
//
void EnableLwrsor (void)
{
    WORD    wCRTC;

    wCRTC = CRTC_MINDEX;
    if (IOByteRead (MISC_INPUT) & 1) wCRTC = CRTC_CINDEX;

    IOByteWrite (wCRTC, 0x0A);
    IOByteWrite ((WORD) (wCRTC + 1), (BYTE) (IOByteRead ((WORD) (wCRTC + 1)) & 0xDF));
}

//
//      GetFrameRate - Get current frame rate
//
//      Entry:  None
//      Exit:   <DWORD> Frame rate in hertz*1000
//
DWORD GetFrameRate (void)
{
    DWORD   time0, time1, rate;
    long    counter;

    if (!WaitVerticalRetrace ())
        return (0);

    // Wait a given number of retraces and time it
    time0 = GetSystemTicks ();
    for (counter = 0; counter < 300; counter++)
        WaitVerticalRetrace ();
    time1 = GetSystemTicks ();

    // This can happen when DOS memory is emulated
    if ((time1 - time0) == 0)
        return (0);

    rate = (DWORD) ((counter * 100 * 182) / (time1 - time0));

    return (rate);
}

//
//      GetSystemTicks - Get the current timer tick count
//
//      Entry:  None
//      Exit:   <DWORD> Current timer tick
//
DWORD GetSystemTicks (void)
{
    static SEGOFF   lpTicks = (SEGOFF) 0x0000046C;  // Timer tick BIOS variable
    static DWORD    dwTicks = 0;

    if (SimGetType () & SIM_SIMULATION)
    {
        // If a stream of frames is being captured, never time out.
        if (_bCaptureStream)
            return (dwTicks);
        else
            return (dwTicks++);
    }
    else
    {
        // If DOS memory is emulated
        if (MemDwordRead ((SEGOFF) lpTicks) == 0)
            return (dwTicks++);
    }

    return (MemDwordRead ((SEGOFF) lpTicks));
}

//
//      WaitVerticalRetrace - Wait for the beginning of vertical retrace
//
//      Entry:  None
//      Exit:   <BOOL>  Retrace status (TRUE = No error, FALSE = Timed out)
//
//      Note:   Since this is an event that happens several (60 to 70) times
//              per second, one second should be sufficient to verify that
//              it is oclwrring regularly. Therefore, if no retrace oclwrs
//              within one second, flag is as an error and return.
//
BOOL WaitVerticalRetrace (void)
{
    DWORD   time0, time1;
    WORD    wStatus, wSimType;
    BOOL    bTimeout, bDumpIO, bDumpMem, bDumpDAC;

    wStatus = INPUT_MSTATUS_1;
    if (IOByteRead (MISC_INPUT) & 0x01) wStatus = INPUT_CSTATUS_1;

    // If vectors are being generated, then only generate a single
    // I/O read (from the Input Status register) into the test vector.
    // Furthermore, if in simulation mode while generating vectors,
    // generate one read and exit. Disabling I/O capture will cause the
    // simulator to return OPEN_BUS when in SIM_TURBOACCESS configuraiton.
    wSimType = SimGetType ();
    if (wSimType & SIM_VECTORS)
    {
        if (wSimType & SIM_SIMULATION)
        {
            IOByteRead (wStatus);                           // Single I/O read
            return (TRUE);                                      //  ...and exit
        }
        SimGetState (&bDumpIO, &bDumpMem, &bDumpDAC);
        SimSetState (FALSE, bDumpMem, bDumpDAC);        // Stop capturing I/O
    }

    if (wSimType & SIM_SIMULATION)
    {
        bTimeout = FALSE;
        while ((IOByteRead (wStatus) & 0x08) == 0x08);
        while ((IOByteRead (wStatus) & 0x08) == 0x00);
    }
    else
    {
        bTimeout = TRUE;
        time0 = GetSystemTicks ();
        time1 = time0 + ONE_SECOND;     // If not status change within 1 sec, exit
        // Wait while in retrace
        while (time0 < time1)
        {
            if ((IOByteRead (wStatus) & 0x08) == 0)
            {
                bTimeout = FALSE;
                break;
            }
            time0 = GetSystemTicks ();
        }
        if (bTimeout) goto WaitVerticalRetrace_exit;

        // Wait while not retrace
        bTimeout = TRUE;
        while (time0 < time1)
        {
            if ((IOByteRead (wStatus) & 0x08) == 0x08)
            {
                bTimeout = FALSE;
                break;
            }
            time0 = GetSystemTicks ();
        }
    }

WaitVerticalRetrace_exit:
    if (wSimType & SIM_VECTORS)
    {
        SimSetState (bDumpIO, bDumpMem, bDumpDAC);  // Restore I/O state
        IOByteRead (wStatus);                               // Single I/O read
    }
    return (!bTimeout);
}

//
//      WaitNotVerticalRetrace - Wait for the end of vertical retrace
//
//      Entry:  None
//      Exit:   <BOOL>  Retrace status (TRUE = No error, FALSE = Timed out)
//
//      Note:   Since this is an event that happens several (60 to 70) times
//              per second, one second should be sufficient to verify that
//              it is oclwrring regularly. Therefore, if no retrace oclwrs
//              within one second, flag is as an error and return.
//
BOOL WaitNotVerticalRetrace (void)
{
    DWORD   time0, time1;
    WORD    wStatus, wSimType;
    BOOL    bTimeout, bDumpIO, bDumpMem, bDumpDAC;

    wStatus = INPUT_MSTATUS_1;
    if (IOByteRead (MISC_INPUT) & 0x01) wStatus = INPUT_CSTATUS_1;

    // If vectors are being generated, then only generate a single
    // I/O read (from the Input Status register) into the test vector.
    // Furthermore, if in simulation mode while generating vectors,
    // generate one read and exit. Disabling I/O capture will cause the
    // simulator to return OPEN_BUS when in SIM_TURBOACCESS configuraiton.
    wSimType = SimGetType ();
    if (wSimType & SIM_VECTORS)
    {
        if (wSimType & SIM_SIMULATION)
        {
            IOByteRead (wStatus);                           // Single I/O read
            return (TRUE);                                      //  ...and exit
        }
        SimGetState (&bDumpIO, &bDumpMem, &bDumpDAC);
        SimSetState (FALSE, bDumpMem, bDumpDAC);        // Stop capturing I/O
    }

    if (wSimType & SIM_SIMULATION)
    {
        bTimeout = FALSE;
        while ((IOByteRead (wStatus) & 0x08) == 0x00);
        while ((IOByteRead (wStatus) & 0x08) == 0x08);
    }
    else
    {
        bTimeout = TRUE;
        time0 = GetSystemTicks ();
        time1 = time0 + ONE_SECOND;     // If not status change within 1 sec, exit
        // Wait while not in retrace
        while (time0 < time1)
        {
            if ((IOByteRead (wStatus) & 0x08) == 0x08)
            {
                bTimeout = FALSE;
                break;
            }
            time0 = GetSystemTicks ();
        }
        if (bTimeout) goto WaitNotVerticalRetrace_exit;

        // Wait while in retrace
        bTimeout = TRUE;
        while (time0 < time1)
        {
            if ((IOByteRead (wStatus) & 0x08) == 0)
            {
                bTimeout = FALSE;
                break;
            }
            time0 = GetSystemTicks ();
        }
    }

WaitNotVerticalRetrace_exit:
    if (wSimType & SIM_VECTORS)
    {
        SimSetState (bDumpIO, bDumpMem, bDumpDAC);  // Restore I/O state
        IOByteRead (wStatus);                               // Single I/O read
    }
    return (!bTimeout);
}

//
//      GetVideoData - Return the video status register value
//
//      Entry:  None
//      Exit:   <BYTE>  Current video data
//
//      Note:   Assume that the entire screen is one uniform color,
//              including overscan.
//
BYTE GetVideoData (void)
{
    BYTE    index, cp_enab, p0, p1, p2, p3;
    BYTE    video;

    index = (BYTE) IOByteRead (ATC_INDEX);
    IOByteRead (INPUT_CSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x32);
    cp_enab = (BYTE) IOByteRead (ATC_RDATA);
    IOByteWrite (ATC_INDEX, 0x0F);
    while ((p0 = (BYTE) IOByteRead (INPUT_CSTATUS_1)) & 1);     // Wait for display time

    IOByteWrite (ATC_INDEX, 0x32);
    IOByteWrite (ATC_INDEX, 0x1F);
    while ((p1 = (BYTE) IOByteRead (INPUT_CSTATUS_1)) & 1);     // Wait for display time

    IOByteWrite (ATC_INDEX, 0x32);
    IOByteWrite (ATC_INDEX, 0x2F);
    while ((p2 = (BYTE) IOByteRead (INPUT_CSTATUS_1)) & 1);     // Wait for display time

    IOByteWrite (ATC_INDEX, 0x32);
    IOByteWrite (ATC_INDEX, 0x3F);
    while ((p3 = (BYTE) IOByteRead (INPUT_CSTATUS_1)) & 1);     // Wait for display time

    // Build the video byte
    video = (BYTE) (((p0 & 0x10) >> 4) |    // Bit 0
                ((p2 & 0x10) >> 3) |                // Bit 1
                ((p0 & 0x20) >> 3) |                // Bit 2
                ((p2 & 0x20) >> 2) |                // Bit 3
                (p1 & 0x30) |                       // Bits 4 & 5
                ((p3 & 0x30) << 2));                // Bits 6 & 7

    IOByteWrite (ATC_INDEX, 0x32);                  // Restore color plane enable
    IOByteWrite (ATC_INDEX, cp_enab);
    IOByteWrite (ATC_INDEX, index);                 // Restore index

    return (video);
}

//
//      GetNolwideoData - Return the video status register value during
//                              non-display time (overscan data is returned)
//
//      Entry:  None
//      Exit:   <BYTE>  Current video data
//
BYTE GetNolwideoData (void)
{
    BYTE    index, cp_enab, p0, p1, p2, p3;
    BYTE    video;
    BOOL    bSim;

    bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;

    WaitVerticalRetrace ();

    if (!bSim) _disable ();
    index = (BYTE) IOByteRead (ATC_INDEX);
    IOByteRead (INPUT_CSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x32);
    cp_enab = (BYTE) IOByteRead (ATC_RDATA);
    IOByteWrite (ATC_INDEX, 0x0F);
    p0 = (BYTE) IOByteRead (INPUT_CSTATUS_1);

    IOByteWrite (ATC_INDEX, 0x32);
    IOByteWrite (ATC_INDEX, 0x1F);
    p1 = (BYTE) IOByteRead (INPUT_CSTATUS_1);

    IOByteWrite (ATC_INDEX, 0x32);
    IOByteWrite (ATC_INDEX, 0x2F);
    p2 = (BYTE) IOByteRead (INPUT_CSTATUS_1);

    IOByteWrite (ATC_INDEX, 0x32);
    IOByteWrite (ATC_INDEX, 0x3F);
    p3 = (BYTE) IOByteRead (INPUT_CSTATUS_1);
    if (!bSim) _enable ();

    // Build the video byte
    video = (BYTE) (((p0 & 0x10) >> 4) |    // Bit 0
                ((p2 & 0x10) >> 3) |                // Bit 1
                ((p0 & 0x20) >> 3) |                // Bit 2
                ((p2 & 0x20) >> 2) |                // Bit 3
                (p1 & 0x30) |                       // Bits 4 & 5
                ((p3 & 0x30) << 2));                // Bits 6 & 7

    IOByteWrite (ATC_INDEX, 0x32);                  // Restore color plane enable
    IOByteWrite (ATC_INDEX, cp_enab);
    IOByteWrite (ATC_INDEX, index);                 // Restore index

    return (video);
}

//
//      RotateByteLeft - Rotate a data byte a given number of times
//
//      Entry:  byData      Data byte
//              byCount     Number of times to rotate data
//      Exit:   <BYTE>      Resultant data
//
BYTE RotateByteLeft (BYTE byData, BYTE byCount)
{
    BYTE    i;

    for (i = 0; i < byCount; i++)
        byData = (BYTE) (((byData & 0x80) >> 7) | ((BYTE) (byData << 1)));

    return (byData);
}

//
//      RotateByteRight - Rotate a data byte a given number of times
//
//      Entry:  byData      Data byte
//              byCount     Number of times to rotate data
//      Exit:   <BYTE>      Resultant data
//
BYTE RotateByteRight (BYTE byData, BYTE byCount)
{
    BYTE    i;

    for (i = 0; i < byCount; i++)
        byData = (BYTE) (((byData & 0x01) << 7) | ((BYTE) (byData >> 1)));

    return (byData);
}

//
//      RotateWordRight - Rotate a data word a given number of times
//
//      Entry:  wData       Data word
//              byCount     Number of times to rotate data
//      Exit:   <WORD>      Resultant data
//
WORD RotateWordRight (WORD wData, BYTE byCount)
{
    BYTE    i;

    for (i = 0; i < byCount; i++)
        wData = (WORD) (((wData & 0x01) << 15) | ((WORD) (wData >> 1)));

    return (wData);
}

//
//      MoveBytes - Move a block of memory of a given size
//
//      Entry:  lpDest      Pointer to destination
//              lpSrc       Pointer to source
//              nLength     Number of bytes to move
//      Exit:   None
//
//      Note:   Due to the way VGA latches work, only one BYTE at at time
//              can be moved around video memory. Therefore, the "C" routine,
//              "_fmemmove" is useless since it is optimized for speed, and
//              thus does WORD or DWORD moves.
//
void MoveBytes (SEGOFF lpDest, SEGOFF lpSrc, int nLength)
{
    while (nLength--)
        MemByteWrite (lpDest++, MemByteRead (lpSrc++));
}

//
//      CompareBytes - Compares two blocks of memory and returns the failure
//
//      Entry:  lpExpected  Pointer to expected data
//              lpActual    Pointer to actual data
//              nLength     Number of bytes to move
//      Exit:   <LPBYTE>    Pointer to failing byte
//                              (Return NULL if successful)
//
SEGOFF CompareBytes (SEGOFF lpExpected, SEGOFF lpActual, int nLength)
{
    SEGOFF  lpFailure;

    //lpFailure = NULL;
    lpFailure = 0;
    while (nLength--)
    {
        if (MemByteRead (lpActual) != MemByteRead (lpExpected))
        {
            lpFailure = lpActual;
            break;
        }
        lpActual++;
        lpExpected++;
    }

    return (lpFailure);
}

//
//      VerifyBytes - Scan a block of memory for a non-conforming value
//
//      Entry:  lpBuffer    Pointer to memory
//              chr         Expected data
//              nLength     Number of bytes to scan
//      Exit:   <LPBYTE>    Pointer to failing byte
//                              (Return NULL if successful)
//
SEGOFF VerifyBytes (SEGOFF lpBuffer, BYTE chr, int nLength)
{
    WORD    hiword, loword;
    int i;

    hiword = loword = 0;
    for (i = 0; i < nLength; i++)
    {
        if (MemByteRead (lpBuffer) != chr)
        {
            hiword = HIWORD (lpBuffer);
            loword = LOWORD (lpBuffer);
            break;
        }
        lpBuffer++;
    }

    return ((SEGOFF) ((((DWORD) (hiword)) << 16) | loword));
}

//
//      SetScans - Set the number of scan lines for text modes. This setting
//                      will take place on the next mode set.
//
//      Entry:  wScan       Number of scans (200, 350, 400)
//      Exit:   None
//
void SetScans (WORD wScan)
{
    static SEGOFF   lpVGAInfo = (SEGOFF) 0x00000489;
    BYTE                mask;

    switch (wScan)
    {
        case 200:
            mask = 0x80;
            break;

        case 350:
            mask = 0x00;
            break;

        case 400:
        default:
            mask = 0x10;
            break;
    }

    MemByteWrite (lpVGAInfo, (BYTE) ((MemByteRead (lpVGAInfo) & 0x6F) | mask));
}

//
//      DisplayInspectionMessage - Display a message about visual inspection
//
//      Entry:  None
//      Exit:   <BOOL>  Ok to proceed flag (TRUE = Continue, FALSE = Abort)
//
BOOL DisplayInspectionMessage (void)
{
    WORD    wSimType;

    // If vectors are being generated or if this is not a physical device,
    // then don't display this message.
    wSimType = SimGetType ();
    if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION)) return (TRUE);

    SetMode (0x03);
    TextStringOut ("This test requires visual inspection and should be compared against", ATTR_NORMAL, 1, 1, 0);
    TextStringOut ("the IBM Model 70 for accuracy.", ATTR_NORMAL, 1, 2, 0);
    TextStringOut ("Press any key to continue, <ESC> to abort test...", ATTR_NORMAL, 1, 4, 0);
    if (SimGetKey () == KEY_ESCAPE)
        return (FALSE);
    return (TRUE);
}

//
//      FrameCapture - Get a frame or return "no-support" flag
//
//      Entry:  nPart       Test section number
//              nTest       Test number within section
//      Exit:   <BOOL>      Support flag (TRUE = Supported, FALSE = Capturing unsupported)
//
BOOL FrameCapture (int nPart, int nTest)
{
    static int  nCount = 0;
    char            szFilename[32];

    // Check to see if a key is hit. If a key is hit and that key is
    // an "<ESC>" key, then abort the frame capture.
    if (_kbhit ())
    {
        // Look at the ASCII part of the key (extended ASCII cannot be
        // "peeked" without removing them from the buffer)
        if (PeekKey () == KEY_ESCAPE)
            return (FALSE);
    }

    if (nCount < 256)
    {
        sprintf (szFilename, "FR%02d%02d%02X", nPart, nTest, nCount++);
        return (CaptureFrame (szFilename) == 0);
    }
    else
        return (FALSE);
}

//
//      StartCapture - Begin a stream of frame captures
//
//      Entry:  byFrameDelay    Frames to delay for continous capture
//      Exit:   None
//
void StartCapture (BYTE byFrameDelay)
{
    WORD    wCRTC;

    _bCaptureStream = TRUE;
    if ((SimGetType () & SIM_SIMULATION))
    {
        wCRTC = CRTC_MINDEX;
        if (IOByteRead (MISC_INPUT) & 0x01) wCRTC = CRTC_CINDEX;
        IOByteWrite (wCRTC, 0x0A);
        IOByteWrite (wCRTC + 1, IOByteRead (wCRTC + 1) | 0x20);
    }

    SimStartCapture (byFrameDelay);
}

//
//      EndCapture - Terminate a stream of frame captures
//
//      Entry:  None
//      Exit:   None
//
void EndCapture (void)
{
    WORD    wCRTC;

    if (_bCaptureStream)
    {
        if ((SimGetType () & SIM_SIMULATION))
        {
            wCRTC = CRTC_MINDEX;
            if (IOByteRead (MISC_INPUT) & 0x01) wCRTC = CRTC_CINDEX;
            IOByteWrite (wCRTC, 0x0A);
            IOByteWrite (wCRTC + 1, IOByteRead (wCRTC + 1) & 0xDF);
        }
        SimEndCapture ();
    }
    _bCaptureStream = FALSE;
}

//
//      SwapWords - Swap two WORD variables
//
//      Entry:  pw1 Pointer to first WORD variable
//              pw2 Pointer to second WORD variable
//      Exit:   None
//
void SwapWords (LPWORD pw1, LPWORD pw2)
{
    WORD    wTemp;

    wTemp = *pw1;
    *pw1 = *pw2;
    *pw2 = wTemp;
}

//
//      MemoryFill - Fill memory with a byte for a given length
//
//      Entry:  lpAddr  Address of memory
//              byData  Data to fill with
//              wLength Length of data
//      Exit:   None
//
void MemoryFill (SEGOFF lpAddr, BYTE byData, WORD wLength)
{
    while (wLength--)
        MemByteWrite (lpAddr++, byData);
}

//
//      GetResolution - Return the current resolution (in pixels)
//
//      Entry:  lpwXRes Pointer to X resolution
//              lpwYRes Pointer to Y resolution
//      Exit:   None
//
void GetResolution (LPWORD lpwXRes, LPWORD lpwYRes)
{
    WORD    wCRTC, wXRes, wYRes, wCharWidth;
    BYTE    byATCMode, byCRTC9;

    wCRTC = CRTC_MINDEX;
    if (IOByteRead (MISC_INPUT) & 1) wCRTC = CRTC_CINDEX;

    IOByteWrite (SEQ_INDEX, 0x01);
    wCharWidth = 9 - (IOByteRead (SEQ_DATA) & 0x01);
    IOByteWrite (wCRTC, 0x01);
    wXRes = (IOByteRead ((WORD) (wCRTC + 1)) + 1) * wCharWidth;

    IOByteWrite (wCRTC, 0x07);
    wYRes = IOByteRead ((WORD) (wCRTC + 1));
    wYRes = ((wYRes & 0x02) << 7) | ((wYRes & 0x40) << 3);
    IOByteWrite (wCRTC, 0x12);
    wYRes |= IOByteRead ((WORD) (wCRTC + 1));
    wYRes++;

    // Determine if pixels are double pumped
    IOByteRead (wCRTC + 6);
    IOByteWrite (ATC_INDEX, 0x30);
    byATCMode = IOByteRead (ATC_RDATA);
    if ((byATCMode & 0x40) == 0x40)
        wXRes = wXRes / 2;

    // Determine if rows are double pumped
    IOByteWrite (wCRTC, 9);
    byCRTC9 = IOByteRead (wCRTC + 1);
    if ((byATCMode & 0x01) == 0x01)
        wYRes = wYRes >> (byCRTC9 & 0x1F);

    // Determine if rows are double scanned
    if ((byCRTC9 & 0x80) == 0x80)
        wYRes = wYRes / 2;

    *lpwXRes = wXRes;
    *lpwYRes = wYRes;
}

//
//      StringLength - Return a model independent string length
//
//      Entry:  lpstr   Pointer to string
//      Exit:   <WORD>  Size of string
//
WORD StringLength (LPSTR lpstr)
{
    WORD    wCount;

    wCount = 0;
    while (*lpstr++)
        wCount++;

    return (wCount);
}

//
//      SystemCleanUp - Clean up and prepare for exit from the test
//
//      Entry:  None
//      Exit:       None
//
void SystemCleanUp (void)
{
#ifdef LW_MODS
    return;         // Don't clean up for now - LGC
#else

    //
    // On physical systems, restore the state of the machine such that
    // the user can return to the DOS prompt. Note that since vectors
    // may or may not be generated on a physical system, only the
    // "SIM_SIMULATION" flag is used to determine whether to clean up
    // or not. Situations in which vectors are being generated on a
    // physical device need to do a final "SetMode" for the physical
    // without doing one for the test vectors.
    //
    if (!(SimGetType () & SIM_SIMULATION))
    {
        SimSetFrameSize (TRUE);
        SimSetState (FALSE, FALSE, FALSE);
        SetScans (400);
        SetMode (0x03);
    }
#endif
}

//
//      GetDataFilename - Retrieve the full path name of the data file needed by the test
//
//      Entry:  pszDatafile     Pointer to the buffer to copy the data file to
//              nMax            Size of the buffer
//      Exit:   <BOOL>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL GetDataFilename (LPSTR pszDatafile, int nMax)
{
#ifdef LW_MODS
    if (strlen (LegacyVGA::ExceptionFile ()) > 0)
        strncpy (pszDatafile, LegacyVGA::ExceptionFile (), nMax);
#endif
    return (TRUE);
}

//
//      WaitNotBlank - Wait for blanking to end
//
//      Entry:  None
//      Exit:   <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL WaitNotBlank (void)
{
    WORD    wStatus;

    wStatus = INPUT_MSTATUS_1;
    if (IOByteRead (MISC_INPUT) & 0x01) wStatus = INPUT_CSTATUS_1;

    // Wait while in display
    while ((IOByteRead (wStatus) & 0x01) == 0);

    // Wait while in blanking
    while ((IOByteRead (wStatus) & 0x01) == 1);

    return (TRUE);
}

//
//      SetLineCompare - Set the line compare value
//
//      Entry:  wCRTC           CRTC index address (0x3D4 or 0x3B4)
//              wLineCompare    Value to set line compare to
//      Exit:   None
//
void SetLineCompare (WORD wCRTC, WORD wLineCompare)
{
    BYTE    temp;

    // CRTC[18].0..7 is line compare bits 0-7
    IOByteWrite (wCRTC, 0x18);
    IOByteWrite (wCRTC + 1, (BYTE) (wLineCompare & 0xFF));

    // CRTC[7].4 is line compare bit 8
    IOByteWrite (wCRTC, 0x07);
    temp = (BYTE) (IOByteRead (wCRTC + 1) & 0xEF);
    IOByteWrite (wCRTC + 1, (BYTE) (temp | ((wLineCompare & 0x100) >> 4)));

    // CRTC[9].6 is line compare bit 9
    IOByteWrite (wCRTC, 0x09);
    temp = (BYTE) (IOByteRead (wCRTC + 1) & 0xBF);
    IOByteWrite (wCRTC + 1, (BYTE) (temp | ((wLineCompare & 0x200) >> 3)));
}

#ifdef LW_MODS
}
#endif

//
//      Copyright (c) 1994-2004 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, Inc.
//      All rights reserved.
//
