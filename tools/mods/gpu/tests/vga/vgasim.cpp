//
//      VGASIM.CPP - Direct hardware linkage for VGACORE tests
//      Copyright (c) 1994-2004 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, Inc.
//      All rights reserved.
//
//      Written by:     Larry Coffey
//      Date:           10 December 1994
//      Last modified:  24 November 2020
//
//      Routines in this file:
//      SimStart                Initialize simulation model
//      SimEnd                  Terminate simulation model
//      IOByteWrite             Write a byte to a specific I/O port
//      IOByteRead              Read a byte from a specific I/O port
//      IOByteTest              Test a byte from an I/O location
//      IOWordWrite             Write a word to a specific I/O port
//      IOWordRead              Read a word from a specific I/O port
//      MemByteWrite            Write a byte to a memory location
//      MemByteRead             Read a byte from a memory location
//      MemByteTest             Test a byte at a memory location
//      MemWordWrite            Write a word to a memory location
//      MemWordRead             Read a word from a memory location
//      MemDwordWrite           Write a dword to a memory location
//      MemDwordRead            Read a dword from a memory location
//      SimModeDirty            Flag that a mode set has oclwred
//      CaptureFrame            Capture a video frame
//      SimSetCaptureMode       Set frame grab mode
//      SimGetCaptureMode       Get frame grab mode
//      WaitLwrsorBlink         Wait for the cursor blink state to change
//      WaitAttrBlink           Wait for the attribute blink state to change
//      SimSetType              Set simulation flags
//      SimGetType              Get the simulation flags
//      SimGetKey               Get a key from the simulated environment
//      SimSetState             Set the simulation into "dump" mode
//      SimGetState             Get the simulation's "dump" mode
//      SimDumpMemory           Dump video memory
//      SimLoadFont             Do a fast load of the font image
//      SimLoadDac              Do a fast load of the RAMDAC
//      SimClearMemory          Do a fast clear of video memory
//      SimGetParms             Override the standard VGA mode table on a mode set
//      SimSetFrameSize         Allow override of the standard VGA mode table
//      SimGetFrameSize         Return the current setting for the override of the standard VGA mode table
//      SimSetAccessMode        Allow selective skipping of non-essential I/O registers
//      SimGetAccessMode        Return the current setting for selective skipping of non-essential I/O registers
//      SimSetSimulationMode    Allow special case acceleration for simulation elwironments
//      SimGetSimulationMode    Return the current setting for special case acceleration for simulation elwironments
//      SimStartCapture         Begin a stream of capture frames
//      SimEndCapture           End a stream of capture frames
//      SimComment              Add a comment to a test vector stream
//
//      SegOffToPhysical        Colwert SEGOFF to physical address
//      SimGetRegAddress        Return the address of the given named register
//      SimGetRegHeadAddress    Return the address of the given named register on the given head
//      SimGetRegField          Return field information of the given named field of the named register
//      SimGetRegValue          Return the field value of the given named field of the named register
//
#include    <stdio.h>
#include    <string.h>
#ifdef __MSVC16__
#include    <bios.h>
#include    <conio.h>
#endif
#ifdef LW_MODS
#include    "core/include/platform.h"
#include    "crccheck.h"
#include    "core/include/mgrmgr.h"
#include    "core/include/gpu.h"
#include    "gpu/include/gpusbdev.h"
#include    "gpu/include/gpumgr.h"
#include    "core/include/tee.h"
#include    "core/include/disp_test.h"

#include    "mdiag/IRegisterMap.h"
#endif
#include    "vgacore.h"
#include    "vgasim.h"
#include    "smallf.h"

#ifdef LW_MODS
namespace DispTest
{
    RC PollRegValue(UINT32 , UINT32 , UINT32 , UINT32 );
    RC PollHWValue(const char * , UINT32, UINT32, UINT32, UINT32, UINT32);
}

unique_ptr<IRegisterMap> CreateRegisterMap(const RefManual *Manual);

namespace LegacyVGA {
#endif

#define OPEN_BUS                    0xAA

static  WORD    _wSimType = SIM_BACKDOOR | SIM_ADAPTER | SIM_SIMULATION | SIM_NOVECTORS | SIM_SMALLFRAME | SIM_TURBOACCESS;     // Simulation
//static    WORD    _wSimType = SIM_NOBACKDOOR | SIM_ADAPTER | SIM_PHYSICAL | SIM_VECTORS | SIM_STDFRAME | SIM_FULLACCESS;              // Physical
static  BOOL    _bSimDumpIO = TRUE;
static  BOOL    _bSimDumpMem = TRUE;
static  BOOL    _bSimDumpDAC = TRUE;
static  BYTE    _byGlobalCaptureMode = CAP_SCREEN;          // Capture mode = SCREEN
static  int     _wLwrsorBlinkOn = BLINK_ON;
static  int     _wAttrBlinkOn = BLINK_ON;
static  BOOL    _bBlinkCare = FALSE;
BOOL    _bSyncBeforeCapture = TRUE;

#ifdef LW_MODS
PHYSICAL    SegOffToPhysical (SEGOFF);

// Fake the DOS memory model
BYTE        byDOSLowMemory[0x1000] = {0};

// Needed to get register definitions from the manuals
static  unique_ptr<IRegisterMap>           _regMap;
static  RefManual*              _Manual;

#endif

//
//      SimStart - Initialize simulation model
//
//      Entry:  None
//      Exit:   <int>       Error code (0 = Success)
//
int SimStart (void)
{
    if (_wSimType & SIM_VECTORS)
        return (VectorStart ());

    // Initialize fake BIOS data area
#ifdef LW_MODS
    ::memset (byDOSLowMemory, 0, sizeof (byDOSLowMemory));
    byDOSLowMemory[0x0410] = 0x20;
    byDOSLowMemory[0x0463] = 0xD4;
    byDOSLowMemory[0x0464] = 0x03;
    byDOSLowMemory[0x0487] = 0x60;
    byDOSLowMemory[0x0488] = 0x09;
    byDOSLowMemory[0x0489] = 0x11;
    byDOSLowMemory[0x048A] = 0x0C;
#endif

#ifdef LW_MODS
    _Manual = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu()->GetRefManual();
    if (_Manual == NULL)
    {
        Printf (Tee::PriNormal, "SimStart: ERROR - Manuals not available (GetRefManual failed).\n");
        return (ERROR_UNEXPECTED);
    }
    _regMap = CreateRegisterMap (_Manual);
    if (!_regMap)
    {
        Printf (Tee::PriNormal, "SimStart: ERROR - Manuals not available (CreateRegisterMap failed).\n");
        return (ERROR_UNEXPECTED);
    }

#endif

    return (ERROR_NONE);
}

//
//      SimEnd - Terminate simulation model
//
//      Entry:  None
//      Exit:   None
//
void SimEnd (void)
{
    if (_wSimType & SIM_VECTORS)
        VectorEnd ();
}

//
//      IOByteWrite - Write a byte to a specific I/O port
//
//      Entry:  wPort   I/O address
//              byData  Data to write
//      Exit:   None
//
void IOByteWrite (WORD wPort, BYTE byData)
{
    // Do an I/O write only if full access is on OR, in turbo mode, only
    // if I/O dumping is enabled. If in turbo mode and I/O enabled and
    // the I/O port is a DAC register, then further qualify that with
    // whether the DAC is enabled.
    if (_wSimType & SIM_TURBOACCESS)
    {
        if ((_bSimDumpIO) &&
            (!(((wPort >= 0x3C6) && (wPort <= 0x3C9)) && (!_bSimDumpDAC))))
        {
            // Perform the I/O Byte Write
#ifdef LW_MODS
            Platform::PioWrite08 (wPort, byData);
#else
            _outp (wPort, byData);
#endif
            if (_wSimType & SIM_VECTORS) VectorIOByteWrite (wPort, byData);
        }
    }
    else
    {
        // Perform the I/O Byte Write
#ifdef LW_MODS
        Platform::PioWrite08 (wPort, byData);
#else
        _outp (wPort, byData);
#endif
        if (_wSimType & SIM_VECTORS) VectorIOByteWrite (wPort, byData);
    }
}

//
//      IOByteRead - Read a byte from a specific I/O port
//
//      Entry:  wPort   I/O address
//      Exit:   <BYTE>  Data read
//
BYTE IOByteRead (WORD wPort)
{
    BYTE    byData;

    // Do an I/O read only if full access is on OR, in turbo mode, only
    // if I/O dumping is enabled. If in turbo mode and I/O enabled and
    // the I/O port is a DAC register, then further qualify that with
    // whether the DAC is enabled.
    byData = OPEN_BUS;
    if (_wSimType & SIM_TURBOACCESS)
    {
        if ((_bSimDumpIO) &&
            (!(((wPort >= 0x3C6) && (wPort <= 0x3C9)) && (!_bSimDumpDAC))))
        {
            // Perform the I/O Byte Read
#ifdef LW_MODS
            Platform::PioRead08 (wPort, &byData);
#else
            byData = _inp (wPort);
#endif

            if (_wSimType & SIM_VECTORS) VectorIOByteRead (wPort);
        }
    }
    else
    {
        // Perform the I/O Byte Read
#ifdef LW_MODS
        Platform::PioRead08 (wPort, &byData);
#else
        byData = _inp (wPort);
#endif
        if (_wSimType & SIM_VECTORS) VectorIOByteRead (wPort);
    }

    return (byData);
}

//
//      IOByteTest - Test a byte at a specific I/O port
//
//      Entry:  wPort       I/O address
//              byExp       Expected data
//              lpbyAct     Pointer to return data read
//      Exit:   <BOOL>      Success flag
//
BOOL IOByteTest (WORD wPort, BYTE byExp, LPBYTE lpbyAct)
{
    BYTE    byData;

    // Do an I/O read only if full access is on OR, in turbo mode, only
    // if I/O dumping is enabled. If in turbo mode and I/O enabled and
    // the I/O port is a DAC register, then further qualify that with
    // whether the DAC is enabled.
    byData = OPEN_BUS;
    if (_wSimType & SIM_TURBOACCESS)
    {
        if ((_bSimDumpIO) &&
            (!(((wPort >= 0x3C6) && (wPort <= 0x3C9)) && (!_bSimDumpDAC))))
        {
            // Perform the I/O Byte Read
#ifdef LW_MODS
            Platform::PioRead08 (wPort, &byData);
#else
            byData = _inp (wPort);
#endif
            if (_wSimType & SIM_VECTORS) VectorIOByteTest (wPort, byData);
        }
    }
    else
    {
        // Perform the I/O Byte Read
#ifdef LW_MODS
        Platform::PioRead08 (wPort, &byData);
#else
        byData = _inp (wPort);
#endif
        if (_wSimType & SIM_VECTORS) VectorIOByteTest (wPort, byData);
    }
    *lpbyAct = byData;

    return (byExp == byData);
}

//
//      IOWordWrite - Write a word to a specific I/O port
//
//      Entry:  wPort       I/O address
//              wData       Data to write
//      Exit:   None
//
void IOWordWrite (WORD wPort, WORD wData)
{
    // Do an I/O write only if full access is on OR, in turbo mode, only
    // if I/O dumping is enabled. If in turbo mode and I/O enabled and
    // the I/O port is a DAC register, then further qualify that with
    // whether the DAC is enabled.
    if (_wSimType & SIM_TURBOACCESS)
    {
        if ((_bSimDumpIO) &&
            (!(((wPort >= 0x3C6) && (wPort <= 0x3C9)) && (!_bSimDumpDAC))))
        {
            // Perform the I/O Word Write
#ifdef LW_MODS
            Platform::PioWrite16 (wPort, wData);
#else
            _outpw (wPort, wData);
#endif
            if (_wSimType & SIM_VECTORS) VectorIOWordWrite (wPort, wData);
        }
    }
    else
    {
        // Perform the I/O Word Write
#ifdef LW_MODS
        Platform::PioWrite16 (wPort, wData);
#else
        _outpw (wPort, wData);
#endif
        if (_wSimType & SIM_VECTORS) VectorIOWordWrite (wPort, wData);
    }
}

//
//      IOWordRead - Read a word from a specific I/O port
//
//      Entry:  wPort   I/O address
//      Exit:   <WORD>  Data read
//
WORD IOWordRead (WORD wPort)
{
    WORD    wData;

    // Do an I/O read only if full access is on OR, in turbo mode, only
    // if I/O dumping is enabled. If in turbo mode and I/O enabled and
    // the I/O port is a DAC register, then further qualify that with
    // whether the DAC is enabled.
    wData = (OPEN_BUS << 8) | OPEN_BUS;
    if (_wSimType & SIM_TURBOACCESS)
    {
        if ((_bSimDumpIO) &&
            (!(((wPort >= 0x3C6) && (wPort <= 0x3C9)) && (!_bSimDumpDAC))))
        {
            // Perform the I/O Word Read
#ifdef LW_MODS
            Platform::PioRead16 (wPort, &wData);
#else
            wData = _inpw (wPort);
#endif
            if (_wSimType & SIM_VECTORS) VectorIOWordRead (wPort);
        }
    }
    else
    {
        // Perform the I/O Word Read
#ifdef LW_MODS
        Platform::PioRead16 (wPort, &wData);
#else
        wData = _inpw (wPort);
#endif
        if (_wSimType & SIM_VECTORS) VectorIOWordRead (wPort);
    }

    return (wData);
}

//
//      MemByteWrite - Write a byte to a memory location
//
//      Entry:  lpAddr  Memory address
//              byData  Data to write
//      Exit:   None
//
void MemByteWrite (SEGOFF lpAddr, BYTE byData)
{
#ifdef LW_MODS
    // Colwert segment:offset to physical memory. If it's low memory,
    // then handle it locally.
    lpAddr = SegOffToPhysical (lpAddr);
    if (lpAddr < 0x1000)
    {
        byDOSLowMemory[lpAddr] = byData;
        return;
    }
    else if ((lpAddr >= 0xC0000) && (lpAddr < 0xC8000))
    {
        // Write to the BIOS area?
        return;
    }
#endif

    // Do a memory write only if full access is on OR, if in turbo mode,
    // only if memory dumping is enabled.
    if (_wSimType & SIM_TURBOACCESS)
    {
        if (_bSimDumpMem)
        {
            // Perform the Memory Byte Write
#ifdef LW_MODS
            Platform::PhysWr08 (lpAddr, byData);
#else
            *((LPBYTE) (lpAddr)) = byData;
#endif

            // If generating vectors, signal a memory byte write
            if (_wSimType & SIM_VECTORS) VectorMemByteWrite (lpAddr, byData);
        }
    }
    else
    {
        // Perform the Memory Byte Write
#ifdef LW_MODS
        Platform::PhysWr08 (lpAddr, byData);
#else
        *((LPBYTE) (lpAddr)) = byData;
#endif

        // If generating vectors, signal a memory byte write
        if (_wSimType & SIM_VECTORS) VectorMemByteWrite (lpAddr, byData);
    }
}

//
//      MemByteRead - Read a byte from a memory location
//
//      Entry:  lpAddr  Memory address
//      Exit:   <BYTE>  Data read
//
BYTE MemByteRead (SEGOFF lpAddr)
{
    BYTE    byData;

#ifdef LW_MODS
    // Colwert segment:offset to physical memory. If it's low memory,
    // then handle it locally.
    lpAddr = SegOffToPhysical (lpAddr);
    if (lpAddr < 0x1000)
    {
        byData = byDOSLowMemory[lpAddr];
        return (byData);
    }
    else if ((lpAddr >= 0xC0000) && (lpAddr < 0xC8000))
    {
        // Read from the BIOS area?
        return (OPEN_BUS);
    }
#endif

    // Do a memory read only if full access is on OR, if in turbo mode,
    // only if memory dumping is enabled.
    byData = OPEN_BUS;
    if (_wSimType & SIM_TURBOACCESS)
    {
        if (_bSimDumpMem)
        {
            // Perform the Memory Byte Read
#ifdef LW_MODS
            byData = Platform::PhysRd08 (lpAddr);
#else
            byData = *((LPBYTE) (lpAddr));
#endif

            // If generating vectors, signal a memory byte read
            if (_wSimType & SIM_VECTORS) VectorMemByteRead (lpAddr);
        }
    }
    else
    {
        // Perform the Memory Byte Read
#ifdef LW_MODS
        byData = Platform::PhysRd08 (lpAddr);
#else
        byData = *((LPBYTE) (lpAddr));
#endif

        // If generating vectors, signal a memory byte read
        if (_wSimType & SIM_VECTORS) VectorMemByteRead (lpAddr);
    }

    return (byData);
}

//
//      MemByteTest - Test a byte at a memory location
//
//      Entry:  lpAddr  Memory address
//              byExp   Expected data
//              lpbyAct Pointer to return data read
//      Exit:   <BOOL>  Success flag (TRUE = Successful match, FALSE = Not)
//
BOOL MemByteTest (SEGOFF lpAddr, BYTE byExp, LPBYTE lpbyAct)
{
    BYTE    byData;

#ifdef LW_MODS
    // Colwert segment:offset to physical memory. If it's low memory,
    // then handle it locally.
    lpAddr = SegOffToPhysical (lpAddr);
    if (lpAddr < 0x1000)
    {
        byData = byDOSLowMemory[lpAddr];
        return (byExp == byData);
    }
    else if ((lpAddr >= 0xC0000) && (lpAddr < 0xC8000))
    {
        // Read from / write to the BIOS area?
        return (FALSE);
    }
#endif

    // Do a memory read only if full access is on OR, if in turbo mode,
    // only if memory dumping is enabled.
    byData = OPEN_BUS;
    if (_wSimType & SIM_TURBOACCESS)
    {
        if (_bSimDumpMem)
        {
            // Perform the Memory Byte Read
#ifdef LW_MODS
            byData = Platform::PhysRd08 (lpAddr);
#else
            byData = *((LPBYTE) (lpAddr));
#endif

            // If generating vectors, signal a memory byte test
            if (_wSimType & SIM_VECTORS) VectorMemByteTest (lpAddr, byExp);
        }
    }
    else
    {
        // Perform the Memory Byte Read
#ifdef LW_MODS
        byData = Platform::PhysRd08 (lpAddr);
#else
        byData = *((LPBYTE) (lpAddr));
#endif

        // If generating vectors, signal a memory byte test
        if (_wSimType & SIM_VECTORS) VectorMemByteTest (lpAddr, byExp);
    }

    // Return data read to the caller
    *lpbyAct = byData;

    // Flag a match vs. mismatch condition
    return (byExp == byData);
}

//
//      MemWordWrite - Write a word to a memory location
//
//      Entry:  lpAddr  Memory address
//              wData   Data to write
//      Exit:   None
//
void MemWordWrite (SEGOFF lpAddr, WORD wData)
{
#ifdef LW_MODS
    // Colwert segment:offset to physical memory. If it's low memory,
    // then handle it locally.
    lpAddr = SegOffToPhysical (lpAddr);
    if (lpAddr < 0x1000)
    {
        *((WORD *) &byDOSLowMemory[lpAddr]) = wData;
        return;
    }
    else if ((lpAddr >= 0xC0000) && (lpAddr < 0xC8000))
    {
        // Write to the BIOS area?
        return;
    }
#endif

    // Do a memory write only if full access is on OR, if in turbo mode,
    // only if memory dumping is enabled.
    if (_wSimType & SIM_TURBOACCESS)
    {
        if (_bSimDumpMem)
        {
            // Perform the Memory Word Write
#ifdef LW_MODS
            Platform::PhysWr16 (lpAddr, wData);
#else
            *(LPWORD) lpAddr = wData;
#endif

            // If generating vectors, signal a memory word write
            if (_wSimType & SIM_VECTORS) VectorMemWordWrite (lpAddr, wData);
        }
    }
    else
    {
        // Perform the Memory Word Write
#ifdef LW_MODS
        Platform::PhysWr16 (lpAddr, wData);
#else
        *(LPWORD) lpAddr = wData;
#endif

        // If generating vectors, signal a memory word write
        if (_wSimType & SIM_VECTORS) VectorMemWordWrite (lpAddr, wData);
    }
}

//
//      MemWordRead - Read a word from a memory location
//
//      Entry:  lpAddr  Memory address
//      Exit:   <WORD>  Data read
//
WORD MemWordRead (SEGOFF lpAddr)
{
    WORD    wData;

#ifdef LW_MODS
    // Colwert segment:offset to physical memory. If it's low memory,
    // then handle it locally.
    lpAddr = SegOffToPhysical (lpAddr);
    if (lpAddr < 0x1000)
    {
        wData = *((WORD *) &byDOSLowMemory[lpAddr]);
        return (wData);
    }
    else if ((lpAddr >= 0xC0000) && (lpAddr < 0xC8000))
    {
        // Read from the BIOS area?
        return ((OPEN_BUS << 8) | OPEN_BUS);
    }
#endif

    wData = (OPEN_BUS << 8) | OPEN_BUS;

    // Do a memory read only if full access is on OR, if in turbo mode,
    // only if memory dumping is enabled.
    if (_wSimType & SIM_TURBOACCESS)
    {
        if (_bSimDumpMem)
        {
            // Perform the Memory Word Read
#ifdef LW_MODS
            wData = Platform::PhysRd16 (lpAddr);
#else
            wData = *(LPWORD) lpAddr;
#endif

            // If generating vectors, signal a memory word read
            if (_wSimType & SIM_VECTORS) VectorMemWordRead (lpAddr);
        }
    }
    else
    {
        // Perform the Memory Word Read
#ifdef LW_MODS
        wData = Platform::PhysRd16 (lpAddr);
#else
        wData = *(LPWORD) lpAddr;
#endif

        // If generating vectors, signal a memory word read
        if (_wSimType & SIM_VECTORS) VectorMemWordRead (lpAddr);
    }

    return (wData);
}

//
//      MemDwordWrite - Write a dword to a memory location
//
//      Entry:  lpAddr  Memory address
//              dwData  Data to write
//      Exit:   None
//
void MemDwordWrite (SEGOFF lpAddr, DWORD dwData)
{
#ifdef LW_MODS
    // Colwert segment:offset to physical memory. If it's low memory,
    // then handle it locally.
    lpAddr = SegOffToPhysical (lpAddr);
    if (lpAddr < 0x1000)
    {
        *((DWORD *) &byDOSLowMemory[lpAddr]) = dwData;
        return;
    }
    else if ((lpAddr >= 0xC0000) && (lpAddr < 0xC8000))
    {
        // Write to the BIOS area?
        return;
    }
#endif

    // Do a memory write only if full access is on OR, if in turbo mode,
    // only if memory dumping is enabled.
    if (_wSimType & SIM_TURBOACCESS)
    {
        if (_bSimDumpMem)
        {
            // Perform the Memory Dword Write
#ifdef LW_MODS
            Platform::PhysWr32 (lpAddr, dwData);
#else
            *(LPDWORD) lpAddr = dwData;
#endif

            // If generating vectors, signal a memory dword write
            if (_wSimType & SIM_VECTORS) VectorMemDwordWrite (lpAddr, dwData);
        }
    }
    else
    {
        // Perform the Memory Dword Write
#ifdef LW_MODS
        Platform::PhysWr32 (lpAddr, dwData);
#else
        *(LPDWORD) lpAddr = dwData;
#endif

        // If generating vectors, signal a memory dword write
        if (_wSimType & SIM_VECTORS) VectorMemDwordWrite (lpAddr, dwData);
    }
}

//
//      MemDwordRead - Read a dword from a memory location
//
//      Entry:  lpAddr  Memory address
//      Exit:   <DWORD> Data read
//
DWORD MemDwordRead (SEGOFF lpAddr)
{
    DWORD   dwData;

#ifdef LW_MODS
    // Colwert segment:offset to physical memory. If it's low memory,
    // then handle it locally.
    lpAddr = SegOffToPhysical (lpAddr);
    if (lpAddr < 0x1000)
    {
        dwData = *((DWORD *) &byDOSLowMemory[lpAddr]);
        return (dwData);
    }
    else if ((lpAddr >= 0xC0000) && (lpAddr < 0xC8000))
    {
        // Read from the BIOS area?
        return (((OPEN_BUS << 24) | (OPEN_BUS << 16) | (OPEN_BUS << 8) | OPEN_BUS));
    }
#endif

    // Do a memory read only if full access is on OR, if in turbo mode,
    // only if memory dumping is enabled.
    dwData = (DWORD) ((OPEN_BUS << 24) | (OPEN_BUS << 16) | (OPEN_BUS << 8) | OPEN_BUS);
    if (_wSimType & SIM_TURBOACCESS)
    {
        if (_bSimDumpMem)
        {
            // Perform the Dword Memory Read
#ifdef LW_MODS
            dwData = Platform::PhysRd32 (lpAddr);
#else
            dwData = *(LPDWORD) lpAddr;
#endif

            // If generating vectors, signal a memory dword read
            if (_wSimType & SIM_VECTORS) VectorMemDwordRead (lpAddr);
        }
    }
    else
    {
        // Perform the Dword Memory Read
#ifdef LW_MODS
        dwData = Platform::PhysRd32(lpAddr);
#else
        dwData = *(LPDWORD) lpAddr;
#endif

        // If generating vectors, signal a memory dword read
        if (_wSimType & SIM_VECTORS) VectorMemDwordRead (lpAddr);
    }

    return (dwData);
}

//
//      SimModeDirty - Flag that a mode set has oclwred
//
//      Entry:  None
//      Exit:   None
//
void SimModeDirty (void)
{
#ifdef LW_MODS
    WORD    wCRTC;
    BYTE    byMisc, byLockStatus;

    // Determine CRTC I/O address
    Platform::PioRead08 (MISC_INPUT, &byMisc);
    wCRTC = (byMisc & 0x01) ? CRTC_CINDEX : CRTC_MINDEX;

    // Determine lock status
    Platform::PioWrite08 (wCRTC, 0x3F);
    Platform::PioRead08 (wCRTC + 1, &byLockStatus);

    // Unlock the registers and set the fastblink mode
    Platform::PioWrite16 (CRTC_CINDEX, 0x573F);
    Platform::PioWrite16 (CRTC_CINDEX, (0x80 << 8) | 0x20);     // LW_CIO_CRE_BLINK_PHASE__INDEX
    Platform::PioWrite16 (CRTC_MINDEX, (0x80 << 8) | 0x20);

    // Re-lock the registers if necessary
    //
    // Notes - The following read values correspond to the given write value:
    //              rd  wr
    //              0   99
    //              1   75
    //              3   57
    //
    if (byLockStatus == 0x00)
        Platform::PioWrite16 (CRTC_CINDEX, 0x993F);     // Complete lock
    else if (byLockStatus == 0x01)
        Platform::PioWrite16 (CRTC_CINDEX, 0x753F);     // R/O state (can read regs)

    LegacyVGA::CRCManager()->SetClockChanged();

    // Tell CaptureFrame to wait for vertical display end
    _bSyncBeforeCapture = TRUE;
#endif
}

//
//      CaptureFrame - Capture a video frame
//
//      Entry:  lpFilename      Filename to capture to
//      Exit:   <int>           Error code (0 = Success, Non-zero = Error)
//
int CaptureFrame (LPSTR lpFilename)
{
#ifdef LW_MODS
    static int  nFrame = 0;

//  // Frame generation (collect CRC)
//  DispTest::VgaCrlwpdate();
    // capturing and storing CRC on every frame
    Printf (Tee::PriNormal, "CaptureFrame : Frame %d\n", nFrame++);
    if (_bBlinkCare)
        LegacyVGA::CRCManager()->SetFrameState (_wLwrsorBlinkOn == 1, _wAttrBlinkOn == 1);
  // arguments are VGA_WAIT, VGA_UPDATE, Timeout

#if 0
    LegacyVGA::CRCManager()->CaptureFrame(1, 1, LegacyVGA::vga_timeout_ms());
#else
    LegacyVGA::CRCManager()->CaptureFrame(1, 1, _bSyncBeforeCapture, LegacyVGA::vga_timeout_ms());
    _bSyncBeforeCapture = FALSE;
#endif
#endif

    if (_wSimType & SIM_VECTORS)
    {
        VectorCaptureFrame (lpFilename);
        return (0);
    }

    return (0);
}
//
//      SimSetCaptureMode - Set frame grab mode
//
//      Entry:  byMode  Capture mode:
//                          CAP_SCREEN
//                          CAP_OVERSCAN
//                          CAP_COMPOSITE
//      Exit:   None
//
void SimSetCaptureMode (BYTE byMode)
{
    _byGlobalCaptureMode = byMode;
    //*** FRAME control goes here - LGC
}

//
//      SimGetCaptureMode - Get frame grab mode
//
//      Entry:  None
//      Exit:   <BYTE>  Capture mode
//
BYTE SimGetCaptureMode (void)
{
    //*** Return FRAME control goes here - LGC
    return (_byGlobalCaptureMode);
}

//
//      WaitLwrsorBlink - Wait for the cursor blink state to change
//
//      Entry:  wState  Cursor state to wait for:
//                          BLINK_ON
//                          BLINK_OFF
//      Exit:   None
//
void WaitLwrsorBlink (WORD wState)
{
    _wLwrsorBlinkOn = wState;
    _bBlinkCare = TRUE;

    if (_wSimType & SIM_VECTORS)
        VectorWaitLwrsorBlink (wState);
}

//
//      WaitAttrBlink - Wait for the attribute blink state to change
//
//      Entry:  wState  Attribute state to wait for:
//                          BLINK_OFF
//                          BLINK_ON
//      Exit:   None
//
void WaitAttrBlink (WORD wState)
{
    _wAttrBlinkOn = wState;
    _bBlinkCare = TRUE;

    if (_wSimType & SIM_VECTORS)
        VectorWaitAttrBlink (wState);
}

//
//      SimSetType - Set simulation flags
//
//      Entry:  byType  VGA type:
//                          SIM_ADAPTER or SIM_MOTHERBOARD
//                          SIM_SIMULATION or SIM_PHYSICAL
//                          SIM_VECTORS or SIM_NOVECTORS
//      Exit:   <WORD>  Previous simulation flags
//
WORD SimSetType (WORD wType)
{
    WORD    wTemp;

    wTemp = _wSimType;
    _wSimType = wType;
    if ((_wSimType & SIM_SIMULATION) == SIM_SIMULATION)
        SimComment ("Test is running in simulation (backdoor initialization enabled).");
    else
        SimComment ("Test is running on physical hardware (frontdoor initialization enabled).");
    return (wTemp);
}

//
//      SimGetType - Get the simulation flags
//
//      Entry:  None
//      Exit:   <BYTE>  VGA type
//
WORD SimGetType (void)
{
    return (_wSimType);
}

//
//      SimGetKey - Get a key from the simulated environment
//
//      Entry:  None
//      Exit:   <WORD>  Scan code read
//
//      Since this is a physical hardware test, a real "GetKey"
//      needs to be implemented. A simulation, though, would
//      just return a "NULL" with no waiting.
//
WORD SimGetKey (void)
{
    return (0);
}

//
//      SimSetState - Set the simulation into "dump" mode
//
//      Entry:  bDumpIO     State of I/O simulation
//              bDumpMem    State of Memory simulation
//              bDumpDAC    State of DAC simulation
//      Exit:   None
//
void SimSetState (BOOL bDumpIO, BOOL bDumpMem, BOOL bDumpDAC)
{
    _bSimDumpIO = bDumpIO;
    _bSimDumpMem = bDumpMem;
    _bSimDumpDAC = bDumpDAC;
}

//
//      SimGetState - Get the simulation's "dump" mode
//
//      Entry:  lpbDumpIO       State of I/O simulation
//              lpbDumpMem      State of Memory simulation
//              lpbDumpDAC      State of DAC simulation
//      Exit:   None
//
void SimGetState (LPBOOL lpbDumpIO, LPBOOL lpbDumpMem, LPBOOL lpbDumpDAC)
{
    *lpbDumpIO = _bSimDumpIO;
    *lpbDumpMem = _bSimDumpMem;
    *lpbDumpDAC = _bSimDumpDAC;
}

//
//      SimDumpMemory - Dump video memory
//
//      Entry:  szFilename      Memory image filename
//      Exit:   None
//
void SimDumpMemory (LPCSTR szFilename)
{
    //*** MEM dump goes here? - LGC
    if (_wSimType & SIM_VECTORS)
        VectorDumpMemory (szFilename);
}

//
//      SimLoadFont - Do a fast load of the font image
//
//      Entry:  lpFont      Pointer to 8K font image map
//      Exit:   <BOOL>      Success flag (TRUE = Load oclwrred, FALSE = Not)
//
BOOL SimLoadFont (LPBYTE lpFont)
{
#ifdef LW_MODS
    LPBYTE      pMem, pMemOrg;
    int         i;

    // Only do the short cut if backdoor is enabled
    if ((_wSimType & SIM_BACKDOOR) != SIM_BACKDOOR)
        return (FALSE);

    // Get a "handle" to the framebuffer
    pMemOrg = pMem = (LPBYTE) DispTest::GetFbVgaPhysAddr ();
    Printf (Tee::PriNormal, "SimLoadFont %p\n", pMem);

    // Load the entire font image
    pMem += 2;          // Point to plane 2 (the font plane)
    for (i = 0; i < (8 * 1024); i++)
    {
        Platform::MemCopy (pMem, lpFont++, 1);
        pMem += 4;
    }

    // Release the memory handle
    DispTest::ReleaseFbVgaPhysAddr (pMemOrg);
    Printf (Tee::PriNormal, "SimLoadFont - Exit\n");
    return (TRUE);
#else
    //*** IMAGELOAD goes here - LGC
    return (FALSE);
#endif
}

//
//      SimLoadDac - Do a fast load of the RAMDAC
//
//      Entry:  lpDAC       Pointer to R,G,B table
//              wEntries    Number of entries
//      Exit:   <BOOL>      Success flag (TRUE = Load oclwrred, FALSE = Not)
//
BOOL SimLoadDac (LPBYTE lpDAC, WORD wLength)
{
#ifdef LW_MODS
    DWORD   dwValue, dwDACAddress;
    BYTE    byRed, byGreen, byBlue;
    int     i;

    // Only do the short cut if backdoor is enabled
    if ((_wSimType & SIM_BACKDOOR) != SIM_BACKDOOR)
        return (FALSE);

    if ((dwDACAddress = SimGetRegAddress ("LW_PDISP_VGA_LUT")) == 0)
        return (FALSE);

    // The short cut really is to use extended memory mapped registers instead of I/O
    Printf (Tee::PriNormal, "SimLoadDac - Enter\n");
    for (i = 0; i < wLength; i++)
    {
        byRed = *lpDAC++;
        byGreen = *lpDAC++;
        byBlue = *lpDAC++;
        dwValue = byBlue | (((int) byGreen) << 8) | (((int) byRed) << 16);

        // LW_PDISP_VGA_LUT(i)
        DispTest::GetBoundGpuSubdevice()->RegWr32 (dwDACAddress + i*4, dwValue);
    }

    // Clear out the rest of the DAC (Std BIOS doesn't do this)
    dwValue = 0;
    for (i = wLength; i < 256; i++)
    {
        // LW_PDISP_VGA_LUT(i)
        DispTest::GetBoundGpuSubdevice()->RegWr32 (dwDACAddress + i*4, dwValue);
    }

    Printf (Tee::PriNormal, "SimLoadDac - Exit\n");
    return (TRUE);
#else
    //*** DACLOAD goes here - LGC
    return (FALSE);
#endif
}

//
//      SimClearMemory - Do a fast clear of video memory
//
//      Entry:  dwFill      Fill value
//      Exit:   <BOOL>      Success flag (TRUE = Clear oclwrred, FALSE = Not)
//
BOOL SimClearMemory (DWORD dwFill)
{
#ifdef LW_MODS
    LPBYTE          pMem, pMemOrg;
    BYTE            by[2], byFill[128];
    unsigned int    i;

    // Only do the short cut if backdoor is enabled
    if ((_wSimType & SIM_BACKDOOR) != SIM_BACKDOOR)
        return (FALSE);

    // Get a "handle" to the framebuffer
    pMemOrg = pMem = (LPBYTE) DispTest::GetFbVgaPhysAddr ();
    Printf (Tee::PriNormal, "SimClearMemory %p\n", pMem);

    by[0] = LOBYTE (LOWORD (dwFill));
    by[1] = HIBYTE (LOWORD (dwFill));
    if (by[0] != by[1])
    {
        // Assume only character / attributes need to be filled for 32K
        for (i = 0; i < (32 * 1024); i++)
        {
            Platform::MemCopy (pMem, by, 2);
            pMem += 4;
        }
    }
    else
    {
        // Assume all of memory can be trashed
        ::memset (byFill, by[0], sizeof (byFill));
        for (i = 0; i < ((256 * 1024) / sizeof (byFill)); i++)
        {
            Platform::MemCopy (pMem + (i * sizeof (byFill)), byFill, sizeof (byFill));
        }
    }

    // Release the memory handle
    DispTest::ReleaseFbVgaPhysAddr (pMemOrg);
    Printf (Tee::PriNormal, "SimClearMemory - Exit\n");
    return (TRUE);
#else
    //*** MEM clear goes here - LGC
    return (FALSE);
#endif
}

//
//      SimGetParms - Override the standard VGA mode table on a mode set
//
//      Entry:  wMode       Mode number
//      Exit:   <LPBYTE>    Pointer to 64-byte data structure (LPPARMENTRY)
//
LPBYTE SimGetParms (WORD wMode)
{
    static BYTE tblText200[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17, 18, 26, 27, 28
    };
    static BYTE tblText350[] = {
        19, 20, 21, 22, 4, 5, 6, 7
    };
    static BYTE tblText400[] = {
        23, 23, 24, 24, 4, 5, 6, 25
    };
    LPPARMENTRY     lpParms;
    static SEGOFF   lpVGAInfo = (SEGOFF) (0x00000489);

#ifdef LW_MODS

    //  Set "fast blink" for simulation
    if ((_wSimType & SIM_SIMULATION) != 0)
    {
        BYTE    byMiscInput;
        WORD    wCRTC;

        Platform::PioRead08 (0x3CC, &byMiscInput);
        wCRTC = (byMiscInput & 0x01) ? 0x3D4 : 0x3B4;
        Platform::PioWrite16 (wCRTC, 0x8020);                   // CRTC[20h].7 = 1 => Enable "fast blink"
    }

#endif

    // If standard frame is being used, use the standard VGA table
    if (!(_wSimType & SIM_SMALLFRAME)) return (NULL);

    // Override the parameter table with the "small" frame version
    wMode &= 0x7F;
    lpParms = (LPPARMENTRY) &tblSmallParameterTable;

    // 200 scan line text or graphics modes
    if ((wMode > 7) || ((MemByteRead (lpVGAInfo) & 0x80) == 0x80))
        return ((LPBYTE) (lpParms + tblText200[wMode]));

    // 400 scan line text
    if ((MemByteRead (lpVGAInfo) & 0x10) == 0x10)
        return ((LPBYTE) (lpParms + tblText400[wMode]));

    // 350 scan line text
    return ((LPBYTE) (lpParms + tblText350[wMode]));
}

//
//      SimSetFrameSize - Allow override of the standard VGA mode table
//
//      Entry:  bSize   Size flag (TRUE = Large, FALSE = Small frame)
//      Exit:   <BOOL>  Previous size flag
//
BOOL SimSetFrameSize (BOOL bSize)
{
    BOOL    bTemp;

//  printf ("SimSetFrameSize: %s\n", bSize ? "TRUE" : "FALSE");
    bTemp = !(_wSimType & SIM_SMALLFRAME);
    _wSimType = _wSimType & ~SIM_SMALLFRAME;
    _wSimType |= bSize ? SIM_STDFRAME : SIM_SMALLFRAME;

    return (bTemp);
}

//
//      SimGetFrameSize - Return the current setting for the override of the standard VGA mode table
//
//      Entry:  None
//      Exit:   <BOOL>  Size flag (TRUE = Large, FALSE = Small frame)
//
BOOL SimGetFrameSize (void)
{
//  printf ("SimGetFrameSize: %s\n", (!(_wSimType & SIM_SMALLFRAME)) ? "TRUE" : "FALSE");
    return (!(_wSimType & SIM_SMALLFRAME));
}

//
//      SimSetAccessMode - Allow selective skipping of non-essential I/O registers
//
//      Entry:  bAccess     Access flag (TRUE = Full, FALSE = "Turbo" access)
//      Exit:   <BOOL>      Previous access flag
//
BOOL SimSetAccessMode (BOOL bAccess)
{
    BOOL    bTemp;

    bTemp = !(_wSimType & SIM_TURBOACCESS);
    _wSimType = _wSimType & ~SIM_TURBOACCESS;
    _wSimType |= bAccess ? SIM_FULLACCESS : SIM_TURBOACCESS;

    return (bTemp);
}

//
//      SimGetAccessMode - Return the current setting for selective skipping of non-essential I/O registers
//
//      Entry:  None
//      Exit:   <BOOL>  Size flag (TRUE = Full, FALSE = "Turbo" access)
//
BOOL SimGetAccessMode (void)
{
    return (!(_wSimType & SIM_TURBOACCESS));
}

//
//      SimSetSimulationMode - Allow special case acceleration for simulation elwironments
//
//      Entry:  bAccess     Simulation flag (TRUE = Simulation, FALSE = Physical access)
//      Exit:   <BOOL>      Previous simulation mode flag
//
BOOL SimSetSimulationMode (BOOL bSimulation)
{
    BOOL    bTemp;

    bTemp = (_wSimType & SIM_SIMULATION) ?  TRUE : FALSE;
    _wSimType = _wSimType & ~SIM_SIMULATION;
    _wSimType |= bSimulation ? SIM_SIMULATION : SIM_PHYSICAL;

    return (bTemp);
}

//
//      SimGetSimulationMode - Return the current setting for special case acceleration for simulation elwironments
//
//      Entry:  None
//      Exit:   <BOOL>  Size flag (TRUE = Full, FALSE = "Turbo" access)
//
BOOL SimGetSimulationMode (void)
{
    return ((_wSimType & SIM_SIMULATION) ? TRUE : FALSE);
}

//
//      SimStartCapture - Begin a stream of capture frames
//
//      Entry:  byFrameDelay    Frames to delay for continous capture
//      Exit:   None
//
//      Note:   This function is mainly used by time-based models to assist
//              with time-dependent tests like panning. For physical hardware
//              and for state-based simulation, this function is unused.
//
//      A "SimEndCapture" must be called to stop streaming.
//
void SimStartCapture (BYTE byFrameDelay)
{
}

//
//      SimEndCapture - End a stream of capture frames
//
//      Entry:  None
//      Exit:   None
//
//      Assume "SimStartCapture" has been called.
//
void SimEndCapture (void)
{
}

//
//      SimComment - Add a comment to a test vector stream
//
//      Entry:  pStr    Pointer to a comment string
//      Exit:   None
//
void SimComment (LPCSTR pStr)
{
    if ((_wSimType & SIM_SIMULATION) != 0)
    {
#ifdef LW_MODS
        Printf (Tee::PriNormal, "%s\n", pStr);
#else
        printf (pStr);
        printf ("\n");
#endif
    }

    if (_wSimType & SIM_VECTORS)
        VectorComment (pStr);
}

#ifdef LW_MODS
//
//      SegOffToPhysical - Colwert SEGOFF to physical address
//
//      Entry:  pAddr       Segment:offset address
//      Exit:   <PHYSICAL>  Physical address
//
PHYSICAL SegOffToPhysical (SEGOFF pSegOff)
{
    return (LOWORD (pSegOff) + (HIWORD (pSegOff) << 4));
}

//
//      SimGetRegAddress - Return the address of the given named register
//
//      Entry:  pStr        Name of the register
//      Exit:   <DWORD>     Address of the register
//
DWORD SimGetRegAddress (LPCSTR pStr)
{
    unique_ptr<IRegisterClass>     reg;

    if (!_regMap)
        return (0);

    reg = _regMap->FindRegister (pStr);
    if (!reg)
    {
        Printf (Tee::PriNormal, "SimGetRegisterAddress: ERROR - Register not found (%s)\n", pStr);
        return (0);
    }
#if 0
    Printf (Tee::PriNormal, "*** SimGetRegisterAddress:"
                            "\n     GetAddress = %Xh"
                            "\n     GetClassId = %Xh"
                            "\n     GetReadMask = %Xh"
                            "\n     GetWriteMask = %Xh"
                            "\n     GetTaskMask = %Xh"
                            "\n     GetUnwriteableMask = %Xh"
                            "\n     GetConstMask = %Xh"
                            "\n     GetW1ClrMask = %Xh"
                            "\n     GetUndefMask = %Xh"
                            "\n     GetArrayDimensions = %Xh\n",
        reg->GetAddress(), reg->GetClassId(), reg->GetReadMask(), reg->GetWriteMask(), reg->GetTaskMask(),
        reg->GetUnwriteableMask(), reg->GetConstMask(), reg->GetW1ClrMask(), reg->GetUndefMask(), reg->GetArrayDimensions());
#endif
    return (reg->GetAddress ());
}

//
//      SimGetRegHeadAddress - Return the address of the given named register on the given head
//
//      Entry:  pStr        Name of the register
//              nHead       Head ID
//      Exit:   <DWORD>     Address of the register
//
DWORD SimGetRegHeadAddress (LPCSTR pStr, int nHead)
{
    unique_ptr<IRegisterClass>     reg;

    if (!_regMap)
        return (0);

    reg = _regMap->FindRegister (pStr);
    if (!reg)
    {
        Printf (Tee::PriNormal, "SimGetRegHeadAddress: ERROR - Register not found (%s)\n", pStr);
        return (0);
    }
    return (reg->GetAddress (nHead));
}

//
//      SimGetRegField - Return field information of the given named field of the named register
//
//      Entry:  pStrReg     Name of the register
//              pStrField   Name of the field
//              pregField   Pointer to the data structure recieving the field information
//      Exit:   <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL SimGetRegField (LPCSTR pStrReg, LPCSTR pStrField, LPREGFIELD pregField)
{
    unique_ptr<IRegisterClass>     reg;
    unique_ptr<IRegisterField>     field;

    if (!_regMap || (pregField == NULL))
        return (FALSE);

    reg = _regMap->FindRegister (pStrReg);
    if (!reg)
    {
        Printf (Tee::PriNormal, "SimGetRegField: ERROR - Register not found (%s)\n", pStrReg);
        return (FALSE);
    }

    field = reg->FindField (pStrField);
    if (!field)
    {
        Printf (Tee::PriNormal, "SimGetRegField: ERROR - Field not found (%s)\n", pStrField);
        return (FALSE);
    }

    memset (pregField, 0, sizeof (REGFIELD));
    pregField->startbit = field->GetStartBit();
    pregField->endbit = field->GetEndBit();
    pregField->readmask = field->GetReadMask();
    pregField->writemask = field->GetWriteMask();
    pregField->unwriteablemask = field->GetUnwriteableMask();
    pregField->constmask = field->GetConstMask();
#if 0
    Printf (Tee::PriNormal, "*** SimGetRegField (%s):"
                            "\n     GetStartBit = %Xh"
                            "\n     GetEndBit = %Xh"
                            "\n     GetReadMask = %Xh"
                            "\n     GetWriteMask = %Xh"
                            "\n     GetUnwriteableMask = %Xh"
                            "\n     GetConstMask = %Xh\n",
        pStrField, pregField->startbit, pregField->endbit, pregField->readmask,
        pregField->writemask, pregField->unwriteablemask, pregField->constmask);
#endif
    return (TRUE);
}

//
//      SimGetRegValue - Return the field value of the given named field of the named register
//
//      Entry:  pStrReg     Name of the register
//              pStrField   Name of the field
//              pStrValue   Name of the value
//      Exit:   <DWORD>     Field value
//
DWORD SimGetRegValue (LPCSTR pStrReg, LPCSTR pStrField, LPCSTR pStrValue)
{
    unique_ptr<IRegisterClass>     reg;
    unique_ptr<IRegisterField>     field;
    unique_ptr<IRegisterValue>     value;

    if (!_regMap)
        return (0);

    reg = _regMap->FindRegister (pStrReg);
    if (!reg)
    {
        Printf (Tee::PriNormal, "SimGetRegValue: ERROR - Register not found (%s)\n", pStrReg);
        return (0);
    }

    field = reg->FindField (pStrField);
    if (!field)
    {
        Printf (Tee::PriNormal, "SimGetRegValue: ERROR - Field not found (%s)\n", pStrField);
        return (0);
    }

    value = field->FindValue (pStrValue);
    if (!value)
    {
        Printf (Tee::PriNormal, "SimGetRegValue: ERROR - Value not found (%s)\n", pStrValue);
        return (0);
    }
//  Printf (Tee::PriNormal, "*** SimGetRegValue (%s): Value = %Xh\n", pStrValue, value->GetValue ());

    return (value->GetValue ());
}
#endif

#ifdef LW_MODS
}
#endif

//
//      Copyright (c) 1994-2004 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, Inc.
//      All rights reserved.
//
