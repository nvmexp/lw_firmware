//
//      COREC5.CPP - Performance measurement functions
//      Copyright (c) 1994-2000 Elpin Systems, Inc.
//      Copyright (c) 2005 SillyTutu.com, Inc.
//      All rights reserved.
//
//      Written by:     Larry Coffey
//      Date:           14 October 2005
//      Last modified:  20 December 2005
//
//      Routines in this file:
//      PerfStart               Trigger the start of performance testing
//      PerfStop                Trigger the end of performance testing and return the time
//      PerfWrite               Write data to sequential addresses
//      PerfWriteLinear         Write data to sequential linear addresses
//      PerfRead                Read data from sequential addresses
//      PerfReadLinear          Read data from sequential linear addresses
//      PerfMove                Move data to / from sequential addresses
//      PerfMoveLinear          Move data to / from sequential linear addresses
//      ZenTimerOn              Start the Zen timer
//      ZenTimerOff             Stop the Zen timer
//      ZenTimerGetValue        Get the last value of the timer
//      _PZTimerOn              Start the short Zen timer
//      _PZTimerOff             Stop the short Zen timer
//      _PZTimerGetValue        Get the last value of the short Zen timer
//      _ReferenceTimerOn       Duplicate logic of xxTimerOn for timing purposes
//      _ReferenceTimerOff      Duplicate logic of xxTimerOff for timing purposes
//      _LZTimerOn              Start the long Zen timer
//      _LZTimerOff             Stop the long Zen timer
//      _LZTimerGetValue        Get the last value of the long Zen timer
//
#include    <stdio.h>
#include    <stdlib.h>
#include    "vgacore.h"
#include    "vgasim.h"

#ifdef LW_MODS
#include    "core/include/platform.h"
#include    "core/include/disp_test.h"
#include    "gpu/include/gpusbdev.h"
#endif

#ifdef LW_MODS
namespace LegacyVGA {
#endif

#define BASE_8253       0x40            // Base address of the 8253 timer chip.
#define TIMER_0_8253    BASE_8253 + 0   // The address of the timer 0 count registers in the 8253.
#define MODE_8253       BASE_8253 + 3   // The address of the mode register in the 8253.

// The address of Operation Command Word 3 in the 8259 Programmable
// Interrupt Controller (PIC) (write only, and writable only when
// bit 4 of the byte written to this address is 0 and bit 3 is 1).
#define OCW3            0x20

// The address of the Interrupt Request register in the 8259 PIC
// (read only, and readable only when bit 1 of OCW3 = 1 and bit 0
// of OCW3 = 0).
#define IRR             0x20

#define DELAY   {asm ("jmp .+2\n\tjmp .+2\n\tjmp .+2\n\t");}

// The address of the BIOS timer count variable in the BIOS data segment.
#define TIMER_COUNT     0x46c

void _ReferenceTimerOn (void);
void _ReferenceTimerOff (void);
BOOL _PZTimerOn (void);
BOOL _PZTimerOff (void);
BOOL _PZTimerGetValue (LPDWORD lpdwTimerValue);
BOOL _LZTimerOn (void);
BOOL _LZTimerOff (void);
BOOL _LZTimerGetValue (LPDWORD lpdwTimerValue);

//
//      PerfStart - Trigger the start of performance testing
//
//      Entry:  pMsg        Performance test name
//      Exit:   <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL PerfStart (LPCSTR pMsg)
{
    // Display message
    if (pMsg != NULL)
    {
        SimComment ("START: ");
        SimComment (pMsg);
        SimComment ("\n");
    }

    // Flush any I/O or memory queues (MODS)
    IOByteRead (INPUT_CSTATUS_1);

    // Signal the start to simulation control
    IOByteWrite (FEAT_CWCONTROL, 0x11);             // Should have no effect in hardware
//#ifdef LW_MODS
//    DispTest::GetBoundGpuSubdevice()->RegWr32 (LW_PGRAPH_DEBUG_1, DRF_DEF(_PGRAPH,_DEBUG_1,_PM,_TRIGGER));
//#endif
    return (ZenTimerOn (TRUE));
}

//
//      PerfStop - Trigger the end of performance testing and return the time
//
//      Entry:  pMsg        Performance test name
//              pdwTrigger  Returned value of trigger time (in microseconds)
//      Exit:   <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL PerfStop (LPCSTR pMsg, LPDWORD pdwTrigger)
{
    // Stop the timer (must be first)
    if (!ZenTimerOff (TRUE))
        return (FALSE);

    // Validate the paramaters
    if (pdwTrigger == NULL)
        return (FALSE);

    // Signal the end to simulation control
    IOByteWrite (FEAT_CWCONTROL, 0x22);
//#ifdef LW_MODS
//    DispTest::GetBoundGpuSubdevice()->RegWr32 (LW_PGRAPH_DEBUG_1, DRF_DEF(_PGRAPH,_DEBUG_1,_PM,_TRIGGER));
//#endif

    // Flush any I/O or memory queues (MODS)
    IOByteRead (INPUT_CSTATUS_1);

    // Display message
    if (pMsg != NULL)
    {
        SimComment ("STOP: ");
        SimComment (pMsg);
        SimComment ("\n");
    }

    // Get the value of the timer and return it
    return (ZenTimerGetValue (TRUE, pdwTrigger));
}

//
//      PerfWrite - Write data to sequential addresses
//
//      Entry:  nPerfSize   Size of writes to use (PERF_BYTE, PERF_WORD, PERF_DWORD)
//              soVideo     Segment:Offset of starting video memory
//              nLength     Length of memory area
//              dwData      Data
//      Exit:   <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL PerfWrite (int nPerfSize, SEGOFF soVideo, int nLength, DWORD dwData)
{
    int     i;

    switch (nPerfSize)
    {
        case PERF_BYTE:
            for (i = 0; i < nLength; i++)
            {
                MemByteWrite (soVideo + i, (BYTE) dwData);
            }
            break;

        case PERF_WORD:
            for (i = 0; i < nLength; i += 2)
            {
                MemWordWrite (soVideo + i, (WORD) dwData);
            }
            break;

        case PERF_DWORD:
            for (i = 0; i < nLength; i += 4)
            {
                MemDwordWrite (soVideo + i, dwData);
            }
            break;

        default:

            return (FALSE);
            break;
    }

    return (TRUE);
}

//
//      PerfWriteLinear - Write data to sequential linear addresses
//
//      Entry:  nPerfSize   Size of writes to use (PERF_BYTE, PERF_WORD, PERF_DWORD)
//              selVideo    Selector to the framebuffer
//              Offset      Offset to the framebuffer
//              nLength     Length of memory area
//              dwData      Data
//      Exit:   <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL PerfWriteLinear (int nPerfSize, int selVideo, size_t Offset, int nLength, DWORD dwData)
{
    int     i;

    switch (nPerfSize)
    {
        case PERF_BYTE:

            for (i = 0; i < nLength; i++)
            {
                MEM_WR08 ((LPBYTE) (Offset + i), (BYTE) dwData);
            }
            break;

        case PERF_WORD:

            for (i = 0; i < nLength; i += 2)
            {
                MEM_WR16 ((LPBYTE) (Offset + i), (WORD) dwData);
            }
            break;

        case PERF_DWORD:

            for (i = 0; i < nLength; i += 4)
            {
                MEM_WR32 ((LPBYTE) (Offset + i), dwData);
            }
            break;

        default:

            return (FALSE);
            break;
    }

    return (TRUE);
}

//
//      PerfRead - Read data from sequential addresses
//
//      Entry:  nPerfSize   Size of reads to use (PERF_BYTE, PERF_WORD, PERF_DWORD)
//                  soVideo     Segment:Offset of starting video memory
//                  nLength     Length of memory area
//      Exit:       <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL PerfRead (int nPerfSize, SEGOFF soVideo, int nLength)
{
    int     i;

    switch (nPerfSize)
    {
        case PERF_BYTE:
            for (i = 0; i < nLength; i++)
            {
                MemByteRead (soVideo + i);
            }
            break;

        case PERF_WORD:
            for (i = 0; i < nLength; i += 2)
            {
                MemWordRead (soVideo + i);
            }
            break;

        case PERF_DWORD:
            for (i = 0; i < nLength; i += 4)
            {
                MemDwordRead (soVideo + i);
            }
            break;

        default:

            return (FALSE);
            break;
    }

    return (TRUE);
}

//
//      PerfReadLinear - Read data from sequential linear addresses
//
//      Entry:  nPerfSize   Size of reads to use (PERF_BYTE, PERF_WORD, PERF_DWORD)
//              selVideo    Selector to the framebuffer
//              Offset      Offset to the framebuffer
//              nLength     Length of memory area
//      Exit:   <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL PerfReadLinear (int nPerfSize, int selVideo, size_t Offset, int nLength)
{
    int     i;

    switch (nPerfSize)
    {
        case PERF_BYTE:

            for (i = 0; i < nLength; i++)
            {
                MEM_RD08 ((LPBYTE) (Offset + i));
            }
            break;

        case PERF_WORD:

            for (i = 0; i < nLength; i += 2)
            {
                MEM_RD16 ((LPBYTE) (Offset + i));
            }
            break;

        case PERF_DWORD:

            for (i = 0; i < nLength; i += 4)
            {
                MEM_RD32 ((LPBYTE) (Offset + i));
            }
            break;

        default:

            return (FALSE);
            break;
    }

    return (TRUE);
}

//
//      PerfMove - Move data to / from sequential addresses
//
//      Entry:  nPerfSize   Size of reads to use (PERF_BYTE, PERF_WORD, PERF_DWORD)
//                  soVideoDst  Segment:Offset of destination starting video memory
//                  soVideoSrc  Segment:Offset of source starting video memory
//                  nLength     Length of memory area
//      Exit:       <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL PerfMove (int nPerfSize, SEGOFF soVideoDst, SEGOFF soVideoSrc, int nLength)
{
    BYTE    byData;
    WORD    wData;
    DWORD   dwData;
    int     i;

    switch (nPerfSize)
    {
        case PERF_BYTE:
            for (i = 0; i < nLength; i++)
            {
                byData = MemByteRead (soVideoSrc + i);
                MemByteWrite (soVideoDst + i, byData);
            }
            break;

        case PERF_WORD:
            for (i = 0; i < nLength; i += 2)
            {
                wData = MemWordRead (soVideoSrc + i);
                MemWordWrite (soVideoDst + i, wData);
            }
            break;

        case PERF_DWORD:
            for (i = 0; i < nLength; i += 4)
            {
                dwData = MemDwordRead (soVideoSrc + i);
                MemDwordWrite (soVideoDst + i, dwData);
            }
            break;

        default:

            return (FALSE);
            break;
    }

    return (TRUE);
}

//
//      PerfMoveLinear - Move data to / from sequential linear addresses
//
//      Entry:  nPerfSize   Size of reads to use (PERF_BYTE, PERF_WORD, PERF_DWORD)
//              selVideo    Selector to the framebuffer
//              OffsetDst   Offset to the framebuffer destination
//              OffsetSrc   Offset to the framebuffer source
//              nLength     Length of memory area
//      Exit:   <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL PerfMoveLinear (int nPerfSize, int selVideo, size_t OffsetDst, size_t OffsetSrc, int nLength)
{
    int     i;
    BYTE    byData;
    WORD    wData;
    DWORD   dwData;

    switch (nPerfSize)
    {
        case PERF_BYTE:

            for (i = 0; i < nLength; i++)
            {
                byData = MEM_RD08 ((LPBYTE) (OffsetSrc + i));
                MEM_WR08 ((LPBYTE) (OffsetDst + i), byData);
            }
            break;

        case PERF_WORD:

            for (i = 0; i < nLength; i += 2)
            {
                wData = MEM_RD16 ((LPBYTE) (OffsetSrc + i));
                MEM_WR16 ((LPBYTE) (OffsetDst + i), wData);
            }
            break;

        case PERF_DWORD:

            for (i = 0; i < nLength; i += 4)
            {
                dwData = MEM_RD32 ((LPBYTE) (OffsetSrc + i));
                MEM_WR32 ((LPBYTE) (OffsetDst + i), dwData);
            }
            break;

        default:

            return (FALSE);
            break;
    }

    return (TRUE);
}

//
//      ZenTimerOn - Start the Zen timer
//
//      Entry:  bUseLongTimer   TRUE = Use the long timer, FALSE = Use the short timer
//      Exit:   <bool>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL ZenTimerOn (BOOL bUseLongTimer)
{
    if (bUseLongTimer)
        return (_LZTimerOn ());
    else
        return (_PZTimerOn ());
}

//
//      ZenTimerOff - Stop the Zen timer
//
//      Entry:  bUseLongTimer   TRUE = Use the long timer, FALSE = Use the short timer
//      Exit:   <bool>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL ZenTimerOff (BOOL bUseLongTimer)
{
    if (bUseLongTimer)
        return (_LZTimerOff ());
    else
        return (_PZTimerOff ());
}

//
//      ZenTimerGetValue - Get the last value of the timer
//
//      Entry:  bUseLongTimer   TRUE = Use the long timer, FALSE = Use the short timer
//              lpdwTimerValue  Value of the timer (returned)
//      Exit:   <bool>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL ZenTimerGetValue (BOOL bUseLongTimer, LPDWORD lpdwTimerValue)
{
    if (bUseLongTimer)
        return (_LZTimerGetValue (lpdwTimerValue));
    else
        return (_PZTimerGetValue (lpdwTimerValue));
}

//
//      _ReferenceTimerOn - Duplicate logic of _PZTimerOn for timing purposes
//
//      Entry:  None
//      Exit:   None
//
void _ReferenceTimerOn (void)
{
}

//
//      _ReferenceTimerOff - Duplicate logic of _PZTimerOff for timing purposes
//
//      Entry:  None
//      Exit:   None
//
void _ReferenceTimerOff (void)
{
}

//
//      _PZTimerOn - Start the short Zen timer
//
//      Entry:  None
//      Exit:   <bool>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL _PZTimerOn (void)
{
    return (TRUE);
}

//
//      _PZTimerOff - Stop the short Zen timer
//
//      Entry:  None
//      Exit:   <bool>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL _PZTimerOff (void)
{
    return (TRUE);
}

//
//      _PZTimerGetValue - Get the last value of the short Zen timer
//
//      Entry:  lpdwTimerValue  Value of the timer (returned)
//      Exit:   <bool>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL _PZTimerGetValue (LPDWORD lpdwTimerValue)
{
    if (lpdwTimerValue == NULL)
        return (FALSE);
    *lpdwTimerValue = 0;

    return (TRUE);
}

//
//      _LZTimerOn - Start the long Zen timer
//
//      Entry:  None
//      Exit:   <bool>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL _LZTimerOn (void)
{
    return (TRUE);
}

//
//      _LZTimerOff - Stop the long Zen timer
//
//      Entry:  None
//      Exit:   <bool>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL _LZTimerOff (void)
{
    return (TRUE);
}

//
//      _LZTimerGetValue - Get the last value of the long Zen timer
//
//      Entry:  lpdwTimerValue  Value of the timer (returned)
//      Exit:   <bool>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL _LZTimerGetValue (LPDWORD lpdwTimerValue)
{
    if (lpdwTimerValue == NULL)
        return (FALSE);
    *lpdwTimerValue = 0;

    return (TRUE);
}

#ifdef LW_MODS
}
#endif

//
//      Copyright (c) 1994-2000 Elpin Systems, Inc.
//      Copyright (c) 2005 SillyTutu.com, Inc.
//      All rights reserved.
//
