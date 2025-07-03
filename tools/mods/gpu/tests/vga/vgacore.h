//
//      VGACORE.H - Defines, structures, and prototypes for VGACORE.LIB
//      Copyright (c) 1994-2000 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, inc.
//      All rights reserved.
//
//      Written by:     Larry Coffey
//      Date:           17 February 1994
//      Last modified:  7 December 2005
//
#ifndef _VGACORE_H
#define _VGACORE_H

#ifdef _MSC_VER
  #define __MSC__
  #include    <conio.h>
  #include    <dos.h>
#endif

#ifdef LW_MODS
namespace LegacyVGA {
#endif

#ifdef  __cplusplus
extern "C" {            /* Assume C declarations for C++ */
#endif                  /* __cplusplus */

#ifdef FALSE
#undef FALSE
#define FALSE                       0
#endif

#ifdef TRUE
#undef TRUE
#define TRUE                        1
#endif

#define VERSION_MAJOR               5
#define VERSION_MINOR               0
#define VERSION_SUBMINOR            0
#define VERSION_STRING              "5.00"

// Implementation specific equates
#ifndef __MSC__
#define __max(a,b)              (((a)>(b))?(a):(b))
#define __min(a,b)              (((a)<(b))?(a):(b))
#endif

#define _enable()               {}
#define _disable()              {}
#define _kbhit()                0
#define _getch()                0
#define _ungetch(a)             {a = 0;}
#define _stricmp(s1,s2)         (strncasecmp ((s1), (s2), __min (strlen((s1)), strlen((s2)))))
#define PACKED_DATA

//  Register definitions
#define PS2_SETUP               0x94
#define VGA_SETUP               0x102
#define ADAPTER_ENABLE          0x46E8
#define CRTC_MINDEX             0x3B4
#define CRTC_MDATA              0x3B5
#define INPUT_MSTATUS_1         0x3BA
#define FEAT_MWCONTROL          0x3BA
#define ATC_INDEX               0x3C0
#define ATC_RDATA               0x3C1
#define INPUT_STATUS_0          0x3C2
#define MISC_OUTPUT             0x3C2
#define MB_ENABLE               0x3C3
#define SEQ_INDEX               0x3C4
#define SEQ_DATA                0x3C5
#define DAC_MASK                0x3C6
#define DAC_RINDEX              0x3C7
#define DAC_WINDEX              0x3C8
#define DAC_DATA                0x3C9
#define FEAT_RCONTROL           0x3CA
#define MISC_INPUT              0x3CC
#define GDC_INDEX               0x3CE
#define GDC_DATA                0x3CF
#define CRTC_CINDEX             0x3D4
#define CRTC_CDATA              0x3D5
#define INPUT_CSTATUS_1         0x3DA
#define FEAT_CWCONTROL          0x3DA

// Extended register definitions
#if defined (LW_MODS) || defined (USE_EXTREGS)
#define LW_CIO_CRE_DISPCTL__INDEX                        0x00000019 /*       */     // Don't touch
#define LW_CIO_CRE_RPC1__INDEX                           0x0000001A /*       */
#define LW_CIO_CRE_ENH__INDEX                            0x0000001B /*       */
#define LW_CIO_CRE_PAGE0_LO__INDEX                       0x0000001C /*       */
#define LW_CIO_CRE_PAGE0_HI__INDEX                       0x0000001D /*       */
#define LW_CIO_CRE_PAGE1_LO__INDEX                       0x0000001E /*       */
#define LW_CIO_CRE_PAGE1_HI__INDEX                       0x0000001F /*       */
#define LW_CIO_CRE_HDT__INDEX                            0x00000028 /*       */
#define LW_CIO_CRE_HDE__INDEX                            0x00000029 /*       */
#define LW_CIO_CRE_HRS__INDEX                            0x0000002C /*       */
#define LW_CIO_CRE_HRE__INDEX                            0x0000002D /*       */
#define LW_CIO_CRE_VT__INDEX                             0x0000002E /*       */
#define LW_CIO_CRE_TI_LO__INDEX                          0x00000030 /*       */
#define LW_CIO_CRE_TI_HI__INDEX                          0x00000031 /*       */
#define LW_CIO_CRE_SA_HI__INDEX                          0x00000034 /*       */
#define LW_CIO_CRE_SA_LO__INDEX                          0x00000035 /*       */
#define LW_CIO_CRE_VRS__INDEX                            0x00000038 /*       */
#define LW_CIO_CRE_VDE__INDEX                            0x0000003A /*       */
#define LW_CIO_CRE_OFFSET__INDEX                         0x0000003B /*       */
#define LW_CIO_CRE_LOCK__INDEX                           0x0000003F /*       */
#define LW_CIO_CRE_FEHT_LOW__INDEX                       0x00000040 /*       */
#define LW_CIO_CRE_FEHT_HIGH__INDEX                      0x00000041 /*       */
#define LW_CIO_CRE_FEHDE_LOW__INDEX                      0x00000042 /*       */
#define LW_CIO_CRE_FEHDE_HIGH__INDEX                     0x00000043 /*       */
#define LW_CIO_CRE_FEHBS_LOW__INDEX                      0x00000044 /*       */
#define LW_CIO_CRE_FEHBS_HIGH__INDEX                     0x00000045 /*       */     // No bits
#define LW_CIO_CRE_FEHBE_LOW__INDEX                      0x00000046 /*       */
#define LW_CIO_CRE_FEHBE_HIGH__INDEX                     0x00000047 /*       */     // No bits
#define LW_CIO_CRE_FEHRS_LOW__INDEX                      0x00000048 /*       */
#define LW_CIO_CRE_FEHRS_HIGH__INDEX                     0x00000049 /*       */
#define LW_CIO_CRE_FEHRE_LOW__INDEX                      0x0000004A /*       */
#define LW_CIO_CRE_FEHRE_HIGH__INDEX                     0x0000004B /*       */     // No bits
#define LW_CIO_CRE_FEVT_LOW__INDEX                       0x00000050 /*       */
#define LW_CIO_CRE_FEVT_HIGH__INDEX                      0x00000051 /*       */
#define LW_CIO_CRE_FEVDE_LOW__INDEX                      0x00000052 /*       */
#define LW_CIO_CRE_FEVDE_HIGH__INDEX                     0x00000053 /*       */
#define LW_CIO_CRE_FEVBS_LOW__INDEX                      0x00000054 /*       */
#define LW_CIO_CRE_FEVBS_HIGH__INDEX                     0x00000055 /*       */
#define LW_CIO_CRE_FEVBE_LOW__INDEX                      0x00000056 /*       */
#define LW_CIO_CRE_FEVBE_HIGH__INDEX                     0x00000057 /*       */     // No bits
#define LW_CIO_CRE_FEVRS_LOW__INDEX                      0x00000058 /*       */
#define LW_CIO_CRE_FEVRS_HIGH__INDEX                     0x00000059 /*       */
#define LW_CIO_CRE_FEVRE_LOW__INDEX                      0x0000005A /*       */
#define LW_CIO_CRE_FEVRE_HIGH__INDEX                     0x0000005B /*       */     // No bits
#define LW_CIO_CRE_BEHT_LOW__INDEX                       0x00000060 /*       */
#define LW_CIO_CRE_BEHT_HIGH__INDEX                      0x00000061 /*       */
#define LW_CIO_CRE_BEHDE_LOW__INDEX                      0x00000062 /*       */
#define LW_CIO_CRE_BEHDE_HIGH__INDEX                     0x00000063 /*       */
#define LW_CIO_CRE_BEHVS_LOW__INDEX                      0x00000064 /*       */
#define LW_CIO_CRE_BEHVS_HIGH__INDEX                     0x00000065 /*       */
#define LW_CIO_CRE_BEHVE_LOW__INDEX                      0x00000066 /*       */
#define LW_CIO_CRE_BEHVE_HIGH__INDEX                     0x00000067 /*       */
#define LW_CIO_CRE_BEHSS_LOW__INDEX                      0x00000068 /*       */
#define LW_CIO_CRE_BEHSS_HIGH__INDEX                     0x00000069 /*       */
#define LW_CIO_CRE_BEHSE_LOW__INDEX                      0x0000006A /*       */
#define LW_CIO_CRE_BEHSE_HIGH__INDEX                     0x0000006B /*       */
#define LW_CIO_CRE_BEVT_LOW__INDEX                       0x00000070 /*       */
#define LW_CIO_CRE_BEVT_HIGH__INDEX                      0x00000071 /*       */
#define LW_CIO_CRE_BEVDE_LOW__INDEX                      0x00000072 /*       */
#define LW_CIO_CRE_BEVDE_HIGH__INDEX                     0x00000073 /*       */
#define LW_CIO_CRE_BEVVS_LOW__INDEX                      0x00000074 /*       */
#define LW_CIO_CRE_BEVVS_HIGH__INDEX                     0x00000075 /*       */
#define LW_CIO_CRE_BEVVE_LOW__INDEX                      0x00000076 /*       */
#define LW_CIO_CRE_BEVVE_HIGH__INDEX                     0x00000077 /*       */
#define LW_CIO_CRE_BEVSS_LOW__INDEX                      0x00000078 /*       */
#define LW_CIO_CRE_BEVSS_HIGH__INDEX                     0x00000079 /*       */
#define LW_CIO_CRE_BEVSE_LOW__INDEX                      0x0000007A /*       */
#define LW_CIO_CRE_BEVSE_HIGH__INDEX                     0x0000007B /*       */
#define LW_CIO_CRE_BE_HEAD__INDEX                        0x0000007F /*       */     // Don't touch
#define LW_CIO_CRE_SCRATCH__INDEX(i)               (0x00000080+(i)) /*       */
#define LW_CIO_CRE_ISI__INDEX                            0x000000A0 /*       */     // Don't touch
#define LW_CIO_CRE_ISD__INDEX                            0x000000A1 /*       */     // Don't touch
#define LW_CIO_CRE_STACK_DATA__INDEX                     0x000000A2 /*       */     // Don't touch
#define LW_CIO_CRE_STACK_CTRL__INDEX                     0x000000A3 /*       */     // Don't touch
#define LW_CIO_CRE_COUNTDOWN__INDEX                      0x000000A8 /*       */
#define LW_CIO_CRE_LGY_CRC__INDEX                        0x000000A9 /*       */     // Don't touch
#define LW_CIO_CRE_END_NEAR__INDEX                       0x000000AA /*       */     // Don't touch
#define LW_CIO_CRE_PORT__INDEX                           0x000000CA /*       */     // Don't touch
#define LW_CIO_CRE_OVERRIDE__INDEX                       0x000000CB /*       */
#define LW_CIO_CRE_CNTL__INDEX                           0x000000CC /*       */
#define LW_CIO_CRE_DAB__INDEX                            0x000000CD /*       */
#define LW_CIO_CRE_UPR_DAB__INDEX                        0x000000CE /*       */
#define LW_CIO_CRE_RAB__INDEX                            0x000000CF /*       */
#define LW_CIO_CRE_DCC_I2C_DATA__INDEX                   0x000000D0 /*       */
#define LW_CIO_CRE_DSI_CTRL__INDEX                       0x000000E8 /*       */     // Don't touch
#define LW_CIO_CRE_DSI_PSI__INDEX                        0x000000E9 /*       */     // Don't touch
#define LW_CIO_CRE_DSI_SUPA__INDEX                       0x000000EB /*       */     // Don't touch
#define LW_CIO_CRE_DSI_SUPB0__INDEX                      0x000000EC /*       */     // Don't touch
#define LW_CIO_CRE_DSI_SUPB1__INDEX                      0x000000ED /*       */     // Don't touch
#define LW_CIO_CRE_RMA__INDEX                            0x000000F0 /*       */
#endif

//  Useful attribute definitions
#define ATTR_NORMAL             0x07
#define ATTR_BLINK              0x87

// Useful timer tick values (18.2065 timer ticks per second)
#define FIVE_SECONDS            91
#define TWO_SECONDS             36
#define ONE_SECOND              18

//  Bus types
#define BUS_ISA                 1
#define BUS_VESA                2
#define BUS_PCI                 3

//  Error definitions
#define ERROR_NONE              0           // No error oclwrred
#define ERROR_USERABORT         1           // User aborted test
#define ERROR_UNEXPECTED        2           // Unexpected behavior
#define ERROR_IOFAILURE         3           // I/O Failure
#define ERROR_CAPTURE           4           // Capture buffer didn't match
#define ERROR_ILWALIDFRAMES     5           // Wrong or no frame rate
#define ERROR_INTERNAL          6           // Test couldn't complete (malloc failure, etc.)
#define ERROR_MEMORY            7           // Video memory failure
#define ERROR_VIDEO             8           // Video data failure
#define ERROR_ILWALIDCLOCK      9           // Wrong dot clock frequency
#define ERROR_SIM_INIT          10          // Initialization error in simulation
#define ERROR_CHECKSUM          11          // Checksum error
#define ERROR_FILE              12          // File open/read/write/close error
#define ERROR_CRCMISSING        20          // no lead crc
#define ERROR_CRCLEAD           21          // test passed lead crc

//  Key codes
#define KEY_BACKSPACE               0x0008
#define KEY_TAB                     0x0009
#define KEY_ENTER                   0x000D
#define KEY_CTRL_N                  0x000E
#define KEY_CTRL_O                  0x000F
#define KEY_CTRL_R                  0x0012
#define KEY_CTRL_S                  0x0013
#define KEY_ESCAPE                  0x001B
#define KEY_SPACE                   0x0020
#define KEY_N                       0x004E
#define KEY_Y                       0x0059
#define KEY_n                       0x006E
#define KEY_y                       0x0079
#define KEY_SHIFT_TAB               0x0F00
#define KEY_HOME                    0x4700
#define KEY_ARROW_UP                0x4800
#define KEY_PGUP                    0x4900
#define KEY_ARROW_LEFT              0x4B00
#define KEY_ARROW_RIGHT             0x4D00
#define KEY_END                     0x4F00
#define KEY_ARROW_DOWN              0x5000
#define KEY_PGDN                    0x5100
#define KEY_DELETE                  0x5300
#define KEY_CTRL_ARROW_LEFT         0x7300
#define KEY_CTRL_ARROW_RIGHT        0x7400
#define KEY_CTRL_PGDN               0x7600
#define KEY_CTRL_PGUP               0x8400
#define KEY_F1                      0x3B00
#define KEY_F2                      0x3C00
#define KEY_F3                      0x3D00
#define KEY_F4                      0x3E00
#define KEY_F5                      0x3F00
#define KEY_F6                      0x4000
#define KEY_F7                      0x4100
#define KEY_F8                      0x4200
#define KEY_F9                      0x4300
#define KEY_F10                     0x4400

// Performance definitions
#define PERF_BYTE                   1
#define PERF_WORD                   2
#define PERF_DWORD                  4

// Industry standard macros
#define LOBYTE(w)                   ((BYTE)(w))
#define HIBYTE(w)                   ((BYTE)(((WORD)(w) >> 8) & 0xFF))
#define LOWORD(l)                   ((WORD)((DWORD)(l) & 0xFFFF))
#define HIWORD(l)                   ((WORD)((((DWORD)(l)) >> 16) & 0xFFFF))
#define MAKELONG(l,h)               ((LONG)(((WORD)(l))|(((DWORD)((WORD)(h)))<<16)))

//  Industry standard type definitions
typedef unsigned char           BYTE;
typedef unsigned short          WORD;
typedef unsigned int            DWORD;
typedef WORD                    BOOL;
typedef BYTE                    *LPBYTE;
typedef WORD                    *LPWORD;
typedef DWORD                   *LPDWORD;
typedef char                    *LPSTR;
typedef const char              *LPCSTR;
typedef BOOL                    *LPBOOL;
typedef DWORD                   SEGOFF;
typedef DWORD                   PHYSICAL;

// Direct access macros for platform specific access
#ifdef LW_MODS
  // Defined in platform.h
#else
#define MEM_WR08(a, d)  do { MemByteWrite ((SEGOFF) (a), (BYTE) (d)); } while (0)
#define MEM_RD08(a) (MemByteRead ((DWORD) (a)))
#define MEM_WR16(a, d)  do { MemWordWrite ((SEGOFF) (a), (WORD) (d)); } while (0)
#define MEM_RD16(a) (MemWordRead ((DWORD) (a)))
#define MEM_WR32(a, d)  do { MemDwordWrite ((SEGOFF) (a), (DWORD) (d)); } while (0)
#define MEM_RD32(a) (MemDwordRead ((DWORD) (a)))
#endif

// Structures
#ifndef PARMENTRY_DEFINED
  #ifdef __MSC__
#pragma pack( 1 )
  #endif
#define PARMENTRY_DEFINED       1
typedef struct tagPARMENTRY {
    BYTE    columns             PACKED_DATA;
    BYTE    rows                PACKED_DATA;
    BYTE    char_height         PACKED_DATA;
    BYTE    regen_length_low    PACKED_DATA;
    BYTE    regen_length_high   PACKED_DATA;
    BYTE    seq_data[4]         PACKED_DATA;
    BYTE    misc                PACKED_DATA;
    BYTE    crtc_data[25]       PACKED_DATA;
    BYTE    atc_data[20]        PACKED_DATA;
    BYTE    gdc_data[9]         PACKED_DATA;
} PARMENTRY;
typedef PARMENTRY               *LPPARMENTRY;
  #ifdef __MSC__
#pragma pack()
  #endif
#endif

typedef struct tagVERTDATA {
    WORD    vtot;
    WORD    vde;
    WORD    vss;
    WORD    vbs;
    BYTE    vse;
    BYTE    vbe;
} VERTDATA;
typedef VERTDATA                *LPVERTDATA;

#ifdef __MSC__
#pragma pack( 1 )
#endif
typedef struct tagVBEVESAINFO
{
    BYTE    VESASignature[4]    PACKED_DATA;
    WORD    VESAVersion         PACKED_DATA;
    DWORD   OEMStringPtr        PACKED_DATA;
    BYTE    Capabilities[4]     PACKED_DATA;
    DWORD   VideoModePtr        PACKED_DATA;
    WORD    TotalMemory         PACKED_DATA;
    WORD    OemSoftwareRev      PACKED_DATA;
    DWORD   OemVendorNamePtr    PACKED_DATA;
    DWORD   OemProductNamePtr   PACKED_DATA;
    DWORD   OemProductRevPtr    PACKED_DATA;
    BYTE    Reserved[222]       PACKED_DATA;
    BYTE    OemData[256]        PACKED_DATA;
} VBEVESAINFO, *LPVBEVESAINFO, *PVBEVESAINFO;
#ifdef __MSC__
#pragma pack()
#endif

#ifdef __MSC__
#pragma pack( 1 )
#endif
typedef struct tagVBEMODEINFO
{
    WORD    ModeAttributes      PACKED_DATA;
    BYTE    WinAAttributes      PACKED_DATA;
    BYTE    WinBAttributes      PACKED_DATA;
    WORD    WinGranularity      PACKED_DATA;
    WORD    WinSize             PACKED_DATA;
    WORD    WinASegment         PACKED_DATA;
    WORD    WinBSegment         PACKED_DATA;
    DWORD   WinFuncPtr          PACKED_DATA;
    WORD    BytesPerScanLine    PACKED_DATA;
    WORD    XResolution         PACKED_DATA;
    WORD    YResolution         PACKED_DATA;
    BYTE    XCharSize           PACKED_DATA;
    BYTE    YCharSize           PACKED_DATA;
    BYTE    NumberOfPlanes      PACKED_DATA;
    BYTE    BitsPerPixel        PACKED_DATA;
    BYTE    NumberOfBanks       PACKED_DATA;
    BYTE    MemoryModel         PACKED_DATA;
    BYTE    BankSize            PACKED_DATA;
    BYTE    NumberOfImagePages  PACKED_DATA;
    BYTE    Reserved_page       PACKED_DATA;
    BYTE    RedMaskSize         PACKED_DATA;
    BYTE    RedMaskPos          PACKED_DATA;
    BYTE    GreenMaskSize       PACKED_DATA;
    BYTE    GreenMaskPos        PACKED_DATA;
    BYTE    BlueMaskSize        PACKED_DATA;
    BYTE    BlueMaskPos         PACKED_DATA;
    BYTE    ReservedMaskSize    PACKED_DATA;
    BYTE    ReservedMaskPos     PACKED_DATA;
    BYTE    DirectColorModeInfo PACKED_DATA;
    DWORD   PhysBasePtr         PACKED_DATA;
    DWORD   OffScreenMemOffset  PACKED_DATA;
    WORD    OffScreenMemSize    PACKED_DATA;
    BYTE    Reserved[206]       PACKED_DATA;
} VBEMODEINFO, *LPVBEMODEINFO, *PVBEMODEINFO;
#ifdef __MSC__
#pragma pack()
#endif

//
//  Function prototypes
//
//  COREC0.CPP
void SetPixelAt (SEGOFF, BYTE, BYTE);
BOOL RWMemoryTest (WORD, WORD);
BYTE IsIObitFunctional (WORD, WORD, BYTE);
void ClearIObitDataBus (void);
BYTE FlagFailure (BYTE, BYTE, BYTE);
BOOL IsVGAEnabled (void);
void SetLwrsorPosition (BYTE, BYTE, BYTE);
WORD GetKey (void);
WORD PeekKey (void);
void DisableLwrsor (void);
void EnableLwrsor (void);
DWORD GetFrameRate (void);
DWORD GetSystemTicks (void);
BOOL WaitVerticalRetrace (void);
BOOL WaitNotVerticalRetrace (void);
BYTE GetVideoData (void);
BYTE GetNolwideoData (void);
BYTE RotateByteLeft (BYTE, BYTE);
BYTE RotateByteRight (BYTE, BYTE);
WORD RotateWordRight (WORD, BYTE);
void MoveBytes (SEGOFF, SEGOFF, int);
SEGOFF CompareBytes (SEGOFF, SEGOFF, int);
SEGOFF VerifyBytes (SEGOFF, BYTE, int);
void SetScans (WORD wScan);
BOOL DisplayInspectionMessage (void);
BOOL FrameCapture (int, int);
void StartCapture(BYTE);
void EndCapture(void);
void SwapWords (LPWORD, LPWORD);
void MemoryFill (SEGOFF, BYTE, WORD);
void GetResolution (LPWORD, LPWORD);
WORD StringLength (LPSTR);
void SystemCleanUp (void);
BOOL GetDataFilename (LPSTR, int);
BOOL WaitNotBlank (void);
void SetLineCompare (WORD, WORD);

//  COREC1.CPP
void PreFontLoad (void);
void PostFontLoad (void);
void LoadFontGlyph (BYTE, BYTE, BYTE, LPBYTE);
void Load8x8 (BYTE);
void Load8x14 (BYTE);
void Load8x16 (BYTE);
void LoadFont (LPBYTE, BYTE, BYTE, WORD, BYTE);
void DrawTransparentChar (WORD, WORD, BYTE, BYTE, BYTE, LPBYTE);
void DrawMonoChar (WORD, WORD, BYTE, LPBYTE, WORD, WORD);
void LoadFixup (LPBYTE, BYTE, BYTE);

//  COREC2.CPP
int FlagError (int, int, int, DWORD, WORD, DWORD, DWORD);
void DisplayError (void);

//  COREC3.CPP
void SetMode (WORD);
void SetDac (BYTE, BYTE, BYTE, BYTE);
void GetDac (BYTE, LPBYTE, LPBYTE, LPBYTE);
void FillDAC (BYTE, BYTE, BYTE);
void FillDACLength (BYTE, WORD, BYTE, BYTE, BYTE);
void SetDacBlock (LPBYTE, BYTE, WORD);
void SetRegs (LPPARMENTRY);
LPPARMENTRY GetParmTable (WORD);
void SetBIOSVars (WORD, LPPARMENTRY);
void ModeLoadFont (WORD, LPPARMENTRY);
void ModeLoadDAC (WORD, LPPARMENTRY);
void ModeClearMemory (WORD);
void SetAll2CPU (BOOL);

// COREC4.CPP
void TextCharOut (BYTE, BYTE, BYTE, BYTE, BYTE);
void TextStringOut (const char *, BYTE, BYTE, BYTE, BYTE);
void PlanarCharOut (BYTE, BYTE, BYTE, BYTE, BYTE);
void CGA4CharOut (BYTE, BYTE, BYTE, BYTE, BYTE);
void CGA2CharOut (BYTE, BYTE, BYTE, BYTE, BYTE);
void MonoGrCharOut (BYTE, BYTE, BYTE, BYTE, BYTE);
void VGACharOut (BYTE, BYTE, BYTE, BYTE, BYTE);
void Line4 (WORD, WORD, WORD, WORD, BYTE);
void HLine4Internal (WORD, WORD, WORD);
void VLine4Internal (WORD, WORD, WORD);
DWORD CalcStart4 (WORD, WORD, LPBYTE);
void SetLine4Columns (WORD);
void SetLine4StartAddress (WORD);

// COREC5.CPP
BOOL PerfStart (LPCSTR pMsg);
BOOL PerfStop (LPCSTR pMsg, LPDWORD pdwTrigger);
BOOL PerfWrite (int nPerfSize, SEGOFF soVideo, int nLength, DWORD dwData);
BOOL PerfWriteLinear (int nPerfSize, int selVideo, size_t Offset, int nLength, DWORD dwData);
BOOL PerfRead (int nPerfSize, SEGOFF soVideo, int nLength);
BOOL PerfReadLinear (int nPerfSize, int selVideo, size_t dwOffset, int nLength);
BOOL PerfMove (int nPerfSize, SEGOFF soVideoDst, SEGOFF soVideoSrc, int nLength);
BOOL PerfMoveLinear (int nPerfSize, int selVideo, size_t dwOffsetDst, size_t dwOffsetSrc, int nLength);
BOOL ZenTimerOn (BOOL bUseLongTimer);
BOOL ZenTimerOff (BOOL bUseLongTimer);
BOOL ZenTimerGetValue (BOOL bUseLongTimer, LPDWORD lpdwTimerValue);

// COREC6.CPP
BOOL VBESetMode (WORD wMode);
BOOL VBEGetInfo (PVBEVESAINFO pvbevi);
BOOL VBEGetModeInfo (WORD wMode, PVBEMODEINFO pvbemi);

//
//  Data definitions
//
#ifndef _DATAFILE_
extern  int _vcerr_code;
extern  int _vcerr_part;
extern  int _vcerr_test;
extern  DWORD   _vcerr_address;
extern  WORD    _vcerr_index;
extern  DWORD   _vcerr_expected;
extern  DWORD   _vcerr_actual;

extern  BYTE    _line_color;
extern  BYTE    _line_rop;
extern  WORD    _line_columns;
extern  WORD    _line_startaddr;

extern  BOOL    _bLoadDAC;

extern  WORD    tblFontBlock[];

extern  BYTE    tblFont8x8[];
extern  BYTE    tblFont8x14[];
extern  BYTE    tblFont8x16[];
extern  BYTE    tblFont9x14[];
extern  BYTE    tblFont9x16[];
extern  BYTE    mapFont8x8[];
extern  BYTE    mapFont8x14[];
extern  BYTE    mapFont9x16[];
extern  BYTE    tblParameterTable[];
extern  BYTE    tblSmallParameterTable[];
extern  BYTE    tbl16ColorDAC[];
extern  BYTE    tblCGADAC[];
extern  BYTE    tblMonoDAC[];
extern  BYTE    tbl256ColorDAC[];
extern  int     nDAC16color;
extern  int     nDACcga;
extern  int     nDACmono;
extern  int     nDAC256color;
extern  BOOL    _bCaptureStream;
#endif

#ifdef __cplusplus
}                       /* End of extern "C" { */
#endif                  /* __cplusplus */

#ifdef LW_MODS
}
#endif

#endif                  /* End of #ifndef _VGACORE_H */
//
//      Copyright (c) 1994-2000 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, inc.
//      All rights reserved.
//

