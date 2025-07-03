//
//      PART11.CPP - VGA Core Test Suite (Part 11)
//      Copyright (c) 1994-2004 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, Inc.
//      All rights reserved.
//
//      Written by:     Larry Coffey, Rich Goodin
//      Date:           15 March 2005
//      Last modified:  17 February 2006
//
//      Routines in this file:
//      IORWTestLW50                Write a pattern to each I/O register and verify that there are no errors.
//      SyncPulseTimingTestLW50     Set a given video timing and then alter the sync position.
//      TextModeSkewTestLW50        Set a known graphics mode video timing and compare the image after setting the text mode bit.
//      VerticalTimesTwoTestLW50    Set a known mode using different sets of CRTC values (one normal, the other vert times 2)
//      SkewTestLW50                In mode 12h, set a known pattern and visually inspect the sync skew and display skew.
//      LwrsorBlinkTestLW50         In text mode, test the actual cursor blink rate
//      GraphicsBlinkTestLW50       In graphics mode, test the actual blink rate
//      MethodGenTestLW50           Test methods generated from I/O operations
//      BlankStressTestLW50         Test blanking within the display period
//      LineCompareTestLW50         Smooth scroll a split screen window to the top of the display
//      StartAddressTestLW50        Simulate the DMU smooth scroll test which depends on start address being latched properly
//      InputStatus1TestLW50        Verify Input Status 1 in a system timing independent manner
//      DoubleBufferTestLW50        Verify double buffering with continuous capture
//      TestTest                    Dummy routine used to write and test the tests before being added to the tests
//
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <errno.h>
#define USE_EXTREGS
#include    "vgacore.h"
#include    "vgasim.h"

#ifdef LW_MODS
#include    "core/include/platform.h"
#include    "crccheck.h"
#include    "core/include/disp_test.h"
#include    "core/include/tee.h"
#include    "core/include/gpu.h"
#include    "gpu/include/gpusbdev.h"
#include    "lwmisc.h"
#endif

#ifdef LW_MODS
#define T1111_CONT_CAP              // Use continuous capture in test T1111
#define T1113_CONT_CAP              // Use continuous capture in test T1113
#else
#define LW_CIO_INP1__COLOR              INPUT_CSTATUS_1
#define LW_CIO_ARX                      ATC_INDEX
#define LW_CIO_AR_PLANE__READ           ATC_RDATA
#endif

#ifndef LW_MODS
#include    <string>
using std::string;
#endif

#define USE_TESTTEST            0
#if USE_TESTTEST
#include    "testtest.h"
#endif

#ifdef LW_MODS
namespace LegacyVGA {
#endif

// Function prototypes of the global tests
int IORWTestLW50 (void);
int IORWTestGF100 (void);
int IORWTestGF11x (void);
int IORWTestMCP89 (void);
int SyncPulseTimingTestLW50 (void);
int TextModeSkewTestLW50 (void);
int VerticalTimesTwoTestLW50 (void);
int SkewTestLW50 (void);
int LwrsorBlinkTestLW50 (void);
int GraphicsBlinkTestLW50 (void);
int MethodGenTestLW50 (void);
int MethodGenTestGF11x (void);
int BlankStressTestLW50 (void);
int LineCompareTestLW50 (void);
int StartAddressTestLW50 (void);
int InputStatus1TestLW50 (void);
int DoubleBufferTestLW50 (void);
int TestTest (void);

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
    BOOL    bFullChip;
} IOTABLE;

typedef struct tagIOUNUSED
{
    WORD    wport;
    WORD    rport;
    BOOL    fIndexed;
    BYTE    idx;
    BOOL    bError;
    BOOL    bFullChip;
} IOUNUSED;

typedef struct tagVGAMETHOD
{
    string  regname;
    int     regindex;
    int     Dac_SetControl;
    int     Dac_SetPolarity;
    int     Sor_SetControl;
    int     Pior_SetControl;
    int     Head_SetPixelClock;
    int     Head_SetControl;
    int     Head_SetRasterSize;
    int     Head_SetRasterSyncEnd;
    int     Head_SetRasterBlankEnd;
    int     Head_SetRasterBlankStart;
    int     Head_SetRasterVertBlank2;
    int     Head_SetDitherControl;
    int     Head_SetControlOutputScaler;
    int     Head_SetProcamp;
    int     Head_SetViewportSizeIn;
    int     Head_SetViewportPointOutAdjust;
    int     Head_SetViewportSizeOut;
    int     VGAControl0;
    int     VGAControl1;
    int     VGAViewportEnd;
    int     VGAActiveStart;
    int     VGAActiveEnd;
    int     VGADisplaySize;
    int     VGAFbContractParms0;
    int     VGAFbContractParms1;
    int     Head_SetLegacyCrcControl;
    int     Core_Update;
    DWORD   val;
    BOOL    fHeadSpecific;
} VGAMETHOD;

typedef struct tagVGAMETHOD_GF11x
{
    string  regname;
    int     regindex;
    int     Dac_SetControl;
    int     Sor_SetControl;
    int     Pior_SetControl;
    int     Head_SetControlOutputResource;
    int     Head_SetPixelClockConfiguration;
    int     Head_SetPixelClockFrequency;
    int     Head_SetControl;
    int     Head_SetRasterSize;
    int     Head_SetRasterSyncEnd;
    int     Head_SetRasterBlankEnd;
    int     Head_SetRasterBlankStart;
    int     Head_SetRasterVertBlank2;
    int     Head_SetDitherControl;
    int     Head_SetControlOutputScaler;
    int     Head_SetProcamp;
    int     Head_SetViewportSizeIn;
    int     Head_SetViewportPointOutAdjust;
    int     Head_SetViewportSizeOut;
    int     VGAControl0;
    int     VGAControl1;
    int     VGAViewportEnd;
    int     VGAActiveStart;
    int     VGAActiveEnd;
    int     VGADisplaySize;
    int     VGAFbContractParms0;
    int     VGAFbContractParms1;
    int     Head_SetLegacyCrcControl;
    int     Core_Update;
    DWORD   val;
    BOOL    fHeadSpecific;
} VGAMETHOD_GF11x;

typedef struct tagMETHODENTRY
{
    LPSTR   pMethodName;
    BOOL    bFound;
} METHODENTRY, *PMETHODENTRY;

typedef struct tagMUXDATA
{
    BYTE    bit0;
    BYTE    shl0;
    BYTE    shr0;
    BYTE    bit1;
    BYTE    shl1;
    BYTE    shr1;
} MUXDATA;

//  Function prototypes used by MethodGenTestLW50
void _MethodGenOutputAndPrint_rVGA(const VGAMETHOD& vmeth, BOOL bUpdate);
void _MethodGenOutputAndPrint_lwGA(const VGAMETHOD& vmeth, BOOL bUpdate);
void _MethodGenOutputRegister_rVGA(const VGAMETHOD& vmeth, BOOL bUpdate);
void _MethodGenPrintRegister_rVGA(const VGAMETHOD& vmeth);
void _MethodGenOutputRegister_lwGA(const VGAMETHOD& vmeth, BOOL bUpdate);
void _MethodGenPrintRegister_lwGA(const VGAMETHOD& vmeth);
void _MethodGenOutputAndPrint_gf11x_rVGA(const VGAMETHOD_GF11x& vmeth, BOOL bUpdate);
void _MethodGenOutputAndPrint_gf11x_lwGA(const VGAMETHOD_GF11x& vmeth, BOOL bUpdate);
void _MethodGenOutputRegister_gf11x_rVGA(const VGAMETHOD_GF11x& vmeth, BOOL bUpdate);
void _MethodGenPrintRegister_gf11x_rVGA(const VGAMETHOD_GF11x& vmeth);
void _MethodGenOutputRegister_gf11x_lwGA(const VGAMETHOD_GF11x& vmeth, BOOL bUpdate);
void _MethodGenPrintRegister_gf11x_lwGA(const VGAMETHOD_GF11x& vmeth);
BOOL _MethodGenSetLegacyCrc (BOOL en, int head);
BOOL _MethodGenWaitNoMethods (void);
BOOL _MethodGenProcessActual (char *pszFileIn, char *pszFileOut);
void _MethodGenSignalMethodStart (BOOL bPriv, DWORD dwAddr, int nIndex);
void _MethodGenSignalMethodStart_gf11x (BOOL bPriv, DWORD dwAddr, int nIndex);
void _MethodGenSignalMethodStop (void);
BOOL _MethodGelwerifyFiles (char *pszFileExpected, char *pszFileActual);
BOOL _MethodGenGatherStrings (FILE *hFile, int *pnLines, PMETHODENTRY *pMethodList, int *pnListEntries);
BOOL _MethodGenFreeList (PMETHODENTRY pMethodList, int nListEntries);
BOOL _MethodGenCompareLists (PMETHODENTRY pMethodListExpected, int nListEntriesExpected, PMETHODENTRY pMethodListActual, int nListEntriesActual);
BOOL _MethodGenExtractName (LPSTR pMethodRaw, LPSTR pMethodName);
BOOL _MethodGelwalidData (char *pszBuffer);

int IORWTestChip(tagIOTABLE iot[], tagIOUNUSED tblUnusedRegs[], int size_iot, int size_tblUnusedRegs);

// Global variables used by tests in this module
static DWORD            head_index = 0;                             // Used by MethodGenTestLW50, BlankStressTestLW50, InputStatus1TestLW50
static DWORD            dac_index = 0;                              // Used by MethodGenTestLW50, BlankStressTestLW50, InputStatus1TestLW50

#ifdef LW_MODS
static FILE             *hFileExpected = NULL;                      // Used by MethodGenTestLW50
static char             szMethodStart[] = "VgaControl1";            // Used by MethodGenTestLW50
static char             szMethodStop[] = "VgaFbContractParms1";     // Used by MethodGenTestLW50
#endif

#define REF_PART        11
#define REF_TEST        1
//
//      T1101
//      IORWTestLW50 - Write a pattern to each extended I/O register and verify that there are no errors.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int IORWTestLW50 (void)
{
    static IOTABLE iot[] = {
        // wport        rport         fIndexed  idx                             mask  rsvdmask  rsvddata bError byExp   byAct   bFullChip
        {CRTC_CINDEX,   CRTC_CINDEX,    FALSE,  0x00,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE0_LO__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Ch
#if 1
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE0_HI__INDEX,     0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE1_LO__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE1_HI__INDEX,     0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HDT__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 28h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HDE__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 29h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HRS__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HRE__INDEX,          0x01,   0xFE,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VT__INDEX,           0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_TI_LO__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 30h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_TI_HI__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 31h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SA_HI__INDEX,        0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 34h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SA_LO__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 35h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VRS__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 38h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VDE__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 3Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_OFFSET__INDEX,       0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 3Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 40h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHT_HIGH__INDEX,    0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 41h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 42h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHDE_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 43h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHBS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 44h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHBE_LOW__INDEX,    0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 46h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 48h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRS_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 49h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRE_LOW__INDEX,    0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 4Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 50h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVT_HIGH__INDEX,    0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 51h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 52h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVDE_HIGH__INDEX,   0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 53h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 54h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBS_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 55h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 56h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 58h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRS_HIGH__INDEX,   0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 59h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RPC1__INDEX,         0xCD,   0x32,   0x32,   FALSE,  0x00,   0x00,   FALSE},         // 1Ah (this can't be done before 28h-59h)
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRE_LOW__INDEX,    0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 5Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 60h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHT_HIGH__INDEX,    0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 61h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 62h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHDE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 63h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 64h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 65h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 66h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 67h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 68h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 69h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 6Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 6Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 70h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVT_HIGH__INDEX,    0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 71h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 72h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVDE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 73h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 74h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 75h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 76h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 77h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 78h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 79h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 7Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 7Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(0),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 80h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(1),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 81h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(2),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 82h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(3),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 83h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(4),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 84h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(5),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 85h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(6),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 86h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(7),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 87h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(8),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 88h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(9),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 89h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(10),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(11),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(12),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(13),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(14),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(15),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(16),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 90h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(17),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 91h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(18),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 92h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(19),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 93h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(20),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 94h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(21),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 95h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(22),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 96h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(23),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 97h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(24),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 98h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(25),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 99h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(26),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(27),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(28),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(29),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(30),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Eh
#endif
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(31),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PORT__INDEX,         0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CAh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_OVERRIDE__INDEX,     0x04,   0xFB,   0x03,   FALSE,  0x00,   0x00,   TRUE},          // CBh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_CNTL__INDEX,         0xFF,   0x00,   0x20,   FALSE,  0x00,   0x00,   TRUE},          // CCh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_DAB__INDEX,          0xEF,   0x80,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CDh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_UPR_DAB__INDEX,      0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CEh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RAB__INDEX,          0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CFh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_DCC_I2C_DATA__INDEX, 0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D0h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_CNTL2__INDEX,        0x03,   0xFC,   0x02,   FALSE,  0x00,   0x00,   TRUE},          // D1h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_LWR_BURST__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D2h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_UPR_BURST__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D3h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RMA__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F0h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF1,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE}           // F1h
    };
    static IOUNUSED tblUnusedRegs[] =
    {
        // wport        rport         fIndexed  idx     bError  bFullChip
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x21,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x23,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x25,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x27,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2A,   FALSE,  FALSE},     // Labeled "reserved"
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2B,   FALSE,  FALSE},     // Labeled "reserved"
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x32,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x33,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x36,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x37,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x39,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA7,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAB,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAC,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAD,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAE,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAF,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB0,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB1,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB2,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB3,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB7,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB8,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB9,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBA,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBB,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBC,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBD,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBE,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBF,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC0,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC1,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC2,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC3,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC4,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC5,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC6,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC7,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC8,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC9,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD3,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD4,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD5,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD6,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD7,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD8,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD9,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDA,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDB,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDC,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDD,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDE,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDF,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE0,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE1,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE2,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE3,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE7,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xEA,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xEE,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xEF,   FALSE,  FALSE},
//      {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF1,   FALSE,  FALSE},     // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF2,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF3,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF4,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF5,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF6,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF7,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF8,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF9,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFA,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFB,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFC,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFD,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFE,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFF,   FALSE,  TRUE}       // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
    };

    return IORWTestChip(iot, tblUnusedRegs, sizeof (iot), sizeof (tblUnusedRegs));
}

//
//      T1101
//      IORWTestGF100 - Write a pattern to each extended I/O register and verify that there are no errors.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int IORWTestGF100 (void)
{
    static IOTABLE iot[] = {
        // wport        rport         fIndexed  idx                             mask  rsvdmask  rsvddata bError byExp   byAct   bFullChip
        {CRTC_CINDEX,   CRTC_CINDEX,    FALSE,  0x00,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE0_LO__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Ch
#if 1
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE0_HI__INDEX,     0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE1_LO__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE1_HI__INDEX,     0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HDT__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 28h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HDE__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 29h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HRS__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HRE__INDEX,          0x01,   0xFE,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VT__INDEX,           0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_TI_LO__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 30h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_TI_HI__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 31h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SA_HI__INDEX,        0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 34h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SA_LO__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 35h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VRS__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 38h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VDE__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 3Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_OFFSET__INDEX,       0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 3Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 40h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHT_HIGH__INDEX,    0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 41h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 42h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHDE_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 43h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHBS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 44h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHBE_LOW__INDEX,    0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 46h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 48h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRS_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 49h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRE_LOW__INDEX,    0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 4Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 50h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVT_HIGH__INDEX,    0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 51h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 52h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVDE_HIGH__INDEX,   0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 53h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 54h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBS_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 55h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 56h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 58h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRS_HIGH__INDEX,   0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 59h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RPC1__INDEX,         0xCD,   0x32,   0x32,   FALSE,  0x00,   0x00,   FALSE},         // 1Ah (this can't be done before 28h-59h)
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRE_LOW__INDEX,    0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 5Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 60h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHT_HIGH__INDEX,    0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 61h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 62h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHDE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 63h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 64h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 65h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 66h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 67h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 68h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 69h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 6Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 6Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 70h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVT_HIGH__INDEX,    0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 71h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 72h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVDE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 73h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 74h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 75h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 76h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 77h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 78h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 79h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 7Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 7Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(0),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 80h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(1),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 81h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(2),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 82h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(3),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 83h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(4),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 84h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(5),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 85h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(6),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 86h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(7),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 87h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(8),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 88h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(9),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 89h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(10),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(11),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(12),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(13),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(14),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(15),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(16),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 90h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(17),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 91h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(18),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 92h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(19),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 93h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(20),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 94h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(21),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 95h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(22),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 96h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(23),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 97h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(24),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 98h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(25),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 99h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(26),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(27),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(28),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(29),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(30),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Eh
#endif
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(31),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PORT__INDEX,         0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CAh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_OVERRIDE__INDEX,     0x0C,   0xF0,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CBh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_CNTL__INDEX,         0xFF,   0x00,   0x20,   FALSE,  0x00,   0x00,   TRUE},          // CCh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_DAB__INDEX,          0xEF,   0x80,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CDh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_UPR_DAB__INDEX,      0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CEh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RAB__INDEX,          0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CFh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_DCC_I2C_DATA__INDEX, 0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D0h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_CNTL2__INDEX,        0x03,   0xFC,   0x02,   FALSE,  0x00,   0x00,   TRUE},          // D1h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_LWR_BURST__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D2h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_UPR_BURST__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D3h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RMA__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F0h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF1,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},           // F1h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF2,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F2h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF3,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F3h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF4,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F4h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF5,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F5h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF6,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F6h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF7,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F7h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF8,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F8h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF9,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F9h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFA,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // FAh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFB,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // FBh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFC,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // FCh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFD,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // FDh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFE,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // FEh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFF,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE}          // FFh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
    };
    static IOUNUSED tblUnusedRegs[] =
    {
        // wport        rport         fIndexed  idx     bError  bFullChip
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x21,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x23,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x25,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x27,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2A,   FALSE,  FALSE},     // Labeled "reserved"
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2B,   FALSE,  FALSE},     // Labeled "reserved"
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x32,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x33,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x36,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x37,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x39,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA7,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAB,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAC,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAD,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAE,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAF,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB0,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB1,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB2,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB3,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB7,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB8,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB9,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBA,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBB,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBC,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBD,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBE,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBF,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC0,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC1,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC2,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC3,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC4,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC5,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC6,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC7,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC8,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC9,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD3,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD4,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD5,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD6,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD7,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD8,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD9,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDA,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDB,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDC,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDD,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDE,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDF,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE0,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE1,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE2,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE3,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE7,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xEA,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xEE,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xEF,   FALSE,  FALSE}
//      {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF1,   FALSE,  FALSE},     // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
    };

    return IORWTestChip(iot, tblUnusedRegs, sizeof (iot), sizeof (tblUnusedRegs));
}

//
//      T1101
//      IORWTestGF11x - Write a pattern to each extended I/O register and verify that there are no errors.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int IORWTestGF11x (void)
{
    static IOTABLE iot[] = {
        // wport        rport         fIndexed  idx                             mask  rsvdmask  rsvddata bError byExp   byAct   bFullChip
        {CRTC_CINDEX,   CRTC_CINDEX,    FALSE,  0x00,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE0_LO__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Ch
#if 1
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE0_HI__INDEX,     0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE1_LO__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE1_HI__INDEX,     0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HDT__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 28h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HDE__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 29h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HRS__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HRE__INDEX,          0x01,   0xFE,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VT__INDEX,           0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_TI_LO__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 30h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_TI_HI__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 31h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SA_HI__INDEX,        0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 34h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SA_LO__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 35h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VRS__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 38h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VDE__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 3Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_OFFSET__INDEX,       0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 3Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 40h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHT_HIGH__INDEX,    0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 41h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 42h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHDE_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 43h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHBS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 44h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHBE_LOW__INDEX,    0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 46h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 48h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRS_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 49h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRE_LOW__INDEX,    0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 4Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 50h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVT_HIGH__INDEX,    0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 51h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 52h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVDE_HIGH__INDEX,   0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 53h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 54h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBS_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 55h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 56h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 58h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRS_HIGH__INDEX,   0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 59h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RPC1__INDEX,         0xCD,   0x32,   0x32,   FALSE,  0x00,   0x00,   FALSE},         // 1Ah (this can't be done before 28h-59h)
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRE_LOW__INDEX,    0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 5Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 60h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHT_HIGH__INDEX,    0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 61h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 62h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHDE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 63h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 64h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 65h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 66h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 67h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 68h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 69h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 6Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 6Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 70h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVT_HIGH__INDEX,    0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 71h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 72h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVDE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 73h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 74h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 75h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 76h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 77h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 78h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 79h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 7Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 7Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(0),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 80h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(1),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 81h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(2),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 82h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(3),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 83h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(4),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 84h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(5),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 85h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(6),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 86h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(7),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 87h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(8),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 88h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(9),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 89h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(10),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(11),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(12),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(13),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(14),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(15),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(16),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 90h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(17),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 91h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(18),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 92h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(19),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 93h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(20),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 94h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(21),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 95h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(22),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 96h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(23),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 97h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(24),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 98h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(25),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 99h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(26),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(27),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(28),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(29),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(30),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Eh
#endif
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(31),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PORT__INDEX,         0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CAh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_OVERRIDE__INDEX,     0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CBh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_CNTL__INDEX,         0xFF,   0x00,   0x20,   FALSE,  0x00,   0x00,   TRUE},          // CCh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_DAB__INDEX,          0xEF,   0x80,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CDh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_UPR_DAB__INDEX,      0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CEh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RAB__INDEX,          0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CFh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_DCC_I2C_DATA__INDEX, 0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D0h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_CNTL2__INDEX,        0x03,   0xFC,   0x02,   FALSE,  0x00,   0x00,   TRUE},          // D1h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_LWR_BURST__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D2h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_UPR_BURST__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D3h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RMA__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F0h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF1,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},           // F1h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF2,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F2h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF3,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F3h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF4,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F4h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF5,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F5h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF6,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F6h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF7,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F7h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF8,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F8h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF9,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F9h Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFA,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // FAh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFB,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // FBh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFC,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // FCh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFD,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // FDh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFE,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // FEh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFF,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE}          // FFh Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
    };
    static IOUNUSED tblUnusedRegs[] =
    {
        // wport        rport         fIndexed  idx     bError  bFullChip
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x21,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x23,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x25,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x27,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2A,   FALSE,  FALSE},     // Labeled "reserved"
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2B,   FALSE,  FALSE},     // Labeled "reserved"
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x32,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x33,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x36,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x37,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x39,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA7,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAB,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAC,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAD,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAE,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAF,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB0,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB1,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB2,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB3,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB7,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB8,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB9,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBA,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBB,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBC,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBD,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBE,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBF,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC0,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC1,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC2,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC3,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC4,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC5,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC6,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC7,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC8,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC9,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD3,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD4,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD5,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD6,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD7,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD8,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD9,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDA,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDB,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDC,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDD,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDE,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDF,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE0,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE1,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE2,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE3,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE7,   FALSE,  FALSE}
//      {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF1,   FALSE,  FALSE},     // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
    };

    return IORWTestChip(iot, tblUnusedRegs, sizeof (iot), sizeof (tblUnusedRegs));
}

//
//      T1101
//      IORWTestMCP89 - Write a pattern to each extended I/O register and verify that there are no errors.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int IORWTestMCP89 (void)
{
    static IOTABLE iot[] = {
        // wport        rport         fIndexed  idx                             mask  rsvdmask  rsvddata bError byExp   byAct   bFullChip
        {CRTC_CINDEX,   CRTC_CINDEX,    FALSE,  0x00,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE0_LO__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Ch
#if 1
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE0_HI__INDEX,     0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE1_LO__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PAGE1_HI__INDEX,     0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 1Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HDT__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 28h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HDE__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 29h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HRS__INDEX,          0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_HRE__INDEX,          0x01,   0xFE,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VT__INDEX,           0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 2Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_TI_LO__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 30h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_TI_HI__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 31h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SA_HI__INDEX,        0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 34h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SA_LO__INDEX,        0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 35h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VRS__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 38h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_VDE__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 3Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_OFFSET__INDEX,       0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 3Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 40h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHT_HIGH__INDEX,    0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 41h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 42h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHDE_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 43h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHBS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 44h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHBE_LOW__INDEX,    0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 46h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 48h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRS_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 49h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEHRE_LOW__INDEX,    0x3F,   0xC0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 4Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 50h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVT_HIGH__INDEX,    0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 51h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 52h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVDE_HIGH__INDEX,   0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 53h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 54h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBS_HIGH__INDEX,   0x03,   0xFC,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 55h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVBE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 56h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 58h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRS_HIGH__INDEX,   0x1F,   0xE0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 59h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RPC1__INDEX,         0xCD,   0x32,   0x32,   FALSE,  0x00,   0x00,   FALSE},         // 1Ah (this can't be done before 28h-59h)
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_FEVRE_LOW__INDEX,    0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 5Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 60h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHT_HIGH__INDEX,    0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 61h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 62h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHDE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 63h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 64h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 65h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 66h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHVE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 67h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 68h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 69h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 6Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEHSE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 6Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVT_LOW__INDEX,     0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 70h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVT_HIGH__INDEX,    0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 71h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVDE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 72h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVDE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 73h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 74h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 75h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 76h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVVE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 77h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSS_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 78h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSS_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 79h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSE_LOW__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 7Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_BEVSE_HIGH__INDEX,   0x7F,   0x80,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 7Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(0),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 80h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(1),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 81h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(2),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 82h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(3),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 83h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(4),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 84h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(5),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 85h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(6),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 86h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(7),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 87h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(8),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 88h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(9),   0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 89h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(10),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(11),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(12),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(13),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(14),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Eh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(15),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 8Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(16),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 90h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(17),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 91h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(18),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 92h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(19),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 93h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(20),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 94h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(21),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 95h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(22),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 96h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(23),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 97h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(24),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 98h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(25),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 99h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(26),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Ah
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(27),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Bh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(28),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Ch
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(29),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Dh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(30),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Eh
#endif
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_SCRATCH__INDEX(31),  0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   FALSE},         // 9Fh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_PORT__INDEX,         0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CAh
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_OVERRIDE__INDEX,     0x0C,   0xF0,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CBh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_CNTL__INDEX,         0xFF,   0x00,   0x20,   FALSE,  0x00,   0x00,   TRUE},          // CCh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_DAB__INDEX,          0xEF,   0x80,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CDh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_UPR_DAB__INDEX,      0x0F,   0xF0,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CEh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RAB__INDEX,          0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // CFh
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_DCC_I2C_DATA__INDEX, 0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D0h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_CNTL2__INDEX,        0x03,   0xFC,   0x02,   FALSE,  0x00,   0x00,   TRUE},          // D1h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_LWR_BURST__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D2h
//        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_UPR_BURST__INDEX,    0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // D3h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   LW_CIO_CRE_RMA__INDEX,          0x07,   0xF8,   0x00,   FALSE,  0x00,   0x00,   TRUE},          // F0h
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF1,                           0xFF,   0x00,   0x00,   FALSE,  0x00,   0x00,   TRUE}           // F1h
    };
    static IOUNUSED tblUnusedRegs[] =
    {
        // wport        rport         fIndexed  idx     bError  bFullChip
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x21,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x23,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x25,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x27,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2A,   FALSE,  FALSE},     // Labeled "reserved"
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2B,   FALSE,  FALSE},     // Labeled "reserved"
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x2F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x32,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x33,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x36,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x37,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x39,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x3E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x4F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x5F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x6F,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7C,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7D,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0x7E,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xA7,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAB,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAC,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAD,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAE,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xAF,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB0,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB1,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB2,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB3,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB7,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB8,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xB9,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBA,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBB,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBC,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBD,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBE,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xBF,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC0,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC1,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC2,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC3,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC4,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC5,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC6,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC7,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC8,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xC9,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD3,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD4,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD5,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD6,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD7,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD8,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xD9,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDA,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDB,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDC,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDD,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDE,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xDF,   FALSE,  TRUE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE0,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE1,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE2,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE3,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE4,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE5,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE6,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xE7,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xEA,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xEE,   FALSE,  FALSE},
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xEF,   FALSE,  FALSE},
//      {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF1,   FALSE,  FALSE},     // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF2,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF3,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF4,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF5,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF6,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF7,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF8,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xF9,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFA,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFB,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFC,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFD,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFE,   FALSE,  TRUE},      // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
        {CRTC_CDATA,    CRTC_CDATA,     TRUE,   0xFF,   FALSE,  TRUE}       // Labeled "Reserved for Host use only.  No Display related functionality is allowed here."
    };

    return IORWTestChip(iot, tblUnusedRegs, sizeof (iot), sizeof (tblUnusedRegs));
}

//
//      T1101
//      IORWTest - Write a pattern to each extended I/O register and verify that there are no errors.
//
//      Entry:  iot           List of all writeable registers and masks for reserved bits
//              tblUnusedRegs List of all unused addresses that should not be writable
//      Exit:   <int>         DOS ERRORLEVEL value
//
int IORWTestChip(tagIOTABLE iot[], tagIOUNUSED tblUnusedRegs[], int size_iot, int size_tblUnusedRegs)
{
    static char     szErrorMsg[64];
    int             nErr, i, j, idx, nTableSize, nUnusedSize;
    WORD            wIOWrt, wIORd;
    BOOL            bFirst, bFullChip;
    BYTE            temp;
    char            szBuffer[512];

    nTableSize = size_iot / sizeof (IOTABLE);
    nUnusedSize = size_tblUnusedRegs / sizeof (IOUNUSED);
    nErr = ERROR_NONE;

    // Determine if the full chip is available or not
    bFullChip = TRUE;
#ifdef LW_MODS
    if ((stricmp (LegacyVGA::SubTarget (), "DISP_GV") == 0) || (stricmp (LegacyVGA::SubTarget (), "DISP_ALL") == 0) || (strlen (LegacyVGA::SubTarget ()) == 0))
        bFullChip = FALSE;
    if (bFullChip)
        sprintf (szBuffer, "Running test against the full chip (SubTarget = %s).", LegacyVGA::SubTarget ());
    else
        sprintf (szBuffer, "Running test against the display core (not the full chip - SubTarget = %s).", LegacyVGA::SubTarget ());
#else
    sprintf (szBuffer, "Running test against the the full chip.");
#endif
    SimComment (szBuffer);

    // Set the mode
    SimSetState (TRUE, TRUE, FALSE);                // Ignore DAC writes
    SetMode (0x92);                                 // Don't bother clearing memory
    SimSetState (TRUE, TRUE, TRUE);

    // Unlock the CRTC and blank the screen
    IOByteWrite (CRTC_CINDEX, 0x11);
    IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) & 0x7F));
    IOByteWrite (SEQ_INDEX, 0x01);
    IOByteWrite (SEQ_DATA, (BYTE) (IOByteRead (SEQ_DATA) | 0x20));

    // Unlock extended registers
    IOWordWrite (CRTC_CINDEX, 0x573F);

    // Enable legacy registers for R/W test to work
    IOByteWrite (CRTC_CINDEX, LW_CIO_CRE_RPC1__INDEX);
    IOByteWrite (CRTC_CDATA, (IOByteRead (CRTC_CDATA) & 0xF7));

    // Test all the locations
    sprintf (szBuffer, "IORWTest - Testing %d I/O locations.", nTableSize);
    SimComment (szBuffer);
    for (i = 0; i < nTableSize; i++)
    {
        // Skip registers that need the full chip
        if (!bFullChip && iot[i].bFullChip) continue;

        // Short cut for readability
        wIOWrt = iot[i].wport;
        wIORd = iot[i].rport;

        // Tell the test runner what's happening
        sprintf (szBuffer, "IORWTest - Write: %Xh, Read: %Xh, Index: %Xh", wIOWrt, wIORd, iot[i].idx);
        SimComment (szBuffer);

        // Set the index if necessary
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

        /*
        // Special case register (full chip only)!
        // Notes:   Writing a 0 to bit 2 and then reading it back will always read back the
        //          pull up values into bits 0..1, which would be a "1" in each register (03h).
        //          Writing a 1 to bit 2 and then reading it back will always read back the
        //          last value written. Therefore, pre-writing a 00000111b (07h) should cause
        //          the read back value to always be 03h in the low two bits.
        if ((iot[i].idx == LW_CIO_CRE_OVERRIDE__INDEX) && (wIOWrt == CRTC_CDATA))
        {
            IOByteWrite (wIOWrt, 0x07);
        }
        */

        // Do the test
        if ((temp = IsIObitFunctional (wIORd, wIOWrt, (BYTE) ~iot[i].mask)) != 0x00)
        {
            iot[i].bError = TRUE;
            iot[i].byExp = 0x00;
            iot[i].byAct = temp;
            nErr = ERROR_IOFAILURE;

            sprintf (szBuffer, "IORWTest - Failure1: Write: %Xh, Read: %Xh, Index: %Xh, Expected: %Xh, Actual: %Xh", wIOWrt, wIORd, iot[i].idx, iot[i].byExp, iot[i].byAct);
            SimComment (szBuffer);
            continue;
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

                sprintf (szBuffer, "IORWTest - Failure2: Write: %Xh, Read: %Xh, Index: %Xh, Expected: %Xh, Actual: %Xh", wIOWrt, wIORd, iot[i].idx, iot[i].byExp, iot[i].byAct);
                SimComment (szBuffer);
            }
            else
            {
                temp = IOByteRead (wIORd) & iot[i].rsvdmask;
                if (temp != iot[i].rsvddata)
                {
                    iot[i].bError = TRUE;
                    iot[i].byExp = iot[i].rsvddata;
                    iot[i].byAct = temp;
                    nErr = ERROR_IOFAILURE;

                    sprintf (szBuffer, "IORWTest - Failure3: Write: %Xh, Read: %Xh, Index: %Xh, Expected: %Xh, Actual: %Xh", wIOWrt, wIORd, iot[i].idx, iot[i].byExp, iot[i].byAct);
                    SimComment (szBuffer);
                }
            }
        }
    }
#if 0
    // Special case testing
    //
    // LW_CIO_CRE_COUNTDOWN__INDEX - Can only put an increasing value into this register
    // Assume a 0 is already in this register to start.
    IOByteWrite (CRTC_CINDEX, LW_CIO_CRE_RPC1__INDEX);
    j = IOByteRead (CRTC_CDATA);
    printf ("j=%Xh\n", j);
    for (i = i; i < 256; i++)
    {
        // Make sure value written can be read
        IOByteWrite (CRTC_CDATA, (BYTE) i);
        if ((temp = IOByteRead (CRTC_CDATA)) != (BYTE) i)
        {
            iot[i].bError = TRUE;
            iot[i].byExp = (BYTE) i;
            iot[i].byAct = temp;
            nErr = ERROR_IOFAILURE;

            sprintf (szBuffer, "Failure CRTC[A8h] - Readback test: Expected: %Xh, Actual: %Xh", i, temp);
            SimComment (szBuffer);
            break;
        }

        // Make sure writing a lower value doesn't change this value
        IOByteWrite (CRTC_CDATA, (BYTE) (i - 1));
        if ((temp = IOByteRead (CRTC_CDATA)) != (BYTE) i)
        {
            iot[i].bError = TRUE;
            iot[i].byExp = (BYTE) i;
            iot[i].byAct = temp;
            nErr = ERROR_IOFAILURE;

            sprintf (szBuffer, "Failure CRTC[A8h] - Lower value test: Expected: %Xh, Actual: %Xh", i, temp);
            SimComment (szBuffer);
            break;
        }
    }
#endif

    // Now test for unused registers
    sprintf (szBuffer, "IORWTest - Testing %d Unused I/O locations.", nUnusedSize);
    SimComment (szBuffer);
    for (i = 0; i < nUnusedSize; i++)
    {
        // Test for full chip
        if (!bFullChip && tblUnusedRegs[i].bFullChip) continue;

        // Short cut for readability
        wIOWrt = tblUnusedRegs[i].wport;
        wIORd = tblUnusedRegs[i].rport;

        // Tell the test runner what's happening
        sprintf (szBuffer, "IORWTest - Write: %Xh, Read: %Xh, Index: %Xh", wIOWrt, wIORd, tblUnusedRegs[i].idx);
        SimComment (szBuffer);

        // Set the index if necessary
        if (wIOWrt == ATC_INDEX)
        {
            ClearIObitDataBus ();               // Set index state
            if (tblUnusedRegs[i].fIndexed)
            {
                IOByteWrite (wIOWrt, iot[i].idx);
                ClearIObitDataBus ();           // Set back to index state
            }
        }
        else if (tblUnusedRegs[i].fIndexed)
        {
            IOByteWrite (wIOWrt - 1, tblUnusedRegs[i].idx);
        }

        // Do the test
        IOByteWrite (wIOWrt, 0xFF);             // Attempt to write 0xFF into the register
        ClearIObitDataBus ();                   // Clear the I/O data bus
        temp = IOByteRead (wIORd);              // Read the register back
        if (temp != 0)                          // If the register is non-zero, then there's an error
        {
            tblUnusedRegs[i].bError = TRUE;
            nErr = ERROR_IOFAILURE;
            sprintf (szBuffer, "IORWTest - Failure - Unused register returned non-zero data: Write: %Xh, Read: %Xh, Index: %02Xh, Data: %02Xh", wIOWrt, wIORd, tblUnusedRegs[i].idx, temp);
            SimComment (szBuffer);
        }
    }

    SystemCleanUp ();

    // If there were any errors display them now.
    if (nErr != ERROR_NONE)
    {
        nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, 0, 0, 0, 0);     // There could be multiple errors, so don't ID any particular one

        // Display the errors
        for (i = 0; i < nTableSize; i++)
        {
            if (iot[i].bError)
            {
                // Display raw error
                if (iot[i].fIndexed)
                {
                    printf ("IORWTest - %04X[%02X] failed (exp=%02Xh, act=%02Xh)\n", iot[i].wport, iot[i].idx, iot[i].byExp, iot[i].byAct);
                }
                else
                {
                    printf ("IORWTest - Write Address = %04X; Read Address = %04X failed (exp=%02Xh, act=%02Xh)\n", iot[i].wport, iot[i].rport, iot[i].byExp, iot[i].byAct);
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
                printf ("IORWTest -   (Non-standard bit(s): %s)\n", szErrorMsg);
            }
        }

        // Display the unused registers' errors
        for (i = 0; i < nUnusedSize; i++)
        {
            if (tblUnusedRegs[i].bError)
            {
                // Display raw error
                if (tblUnusedRegs[i].fIndexed)
                {
                    printf ("IORWTest - Unused register %04X[%02X] responded.\n", tblUnusedRegs[i].wport, tblUnusedRegs[i].idx);
                }
                else
                {
                    printf ("IORWTest - Unused register responded: Write Address = %04X; Read Address = %04X.\n", tblUnusedRegs[i].wport, tblUnusedRegs[i].rport);
                }
            }
        }
    }

    if (nErr == ERROR_NONE)
        SimComment ("I/O R/W Extended Register Test passed.");
    else
        SimComment ("I/O R/W Extended Register Test failed.");
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        2
//
//      T1102
//      SyncPulseTimingTestLW50 - Set a given video timing and then alter the sync position.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int SyncPulseTimingTestLW50 (void)
{
    int     nErr, i, nCount;
    WORD    wXRes, wYRes;
    BYTE    bySyncS, bySyncE;
    BYTE    byBlankS;
    BYTE    byDisplayE;
//  WORD    wSimType;
    BOOL    bFullVGA;

    nErr = ERROR_NONE;
//  wSimType = SimGetType ();
//  bFullVGA = !((wSimType & SIM_TURBOACCESS) && (wSimType & SIM_SIMULATION));
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto SyncPulseTimingTestLW50_exit;

    if (bFullVGA)
    {
        bySyncS = 0x50;
        bySyncE = 0x9C;
        // increase blank so it won't interfere with sync
        byBlankS = 0x4d;
        // decrease display enable so it won't interfere
        byDisplayE = 0x4d;
        nCount = 7;
    }
    else
    {
//      SimSetFrameSize (FALSE);                        // Use small frame
        bySyncS = 0x28;
        bySyncE = 0x8D;
        // increase blank so it won't interfere with sync
        byBlankS = 0x25;
        // decrease display enable so it won't interfere
        byDisplayE = 0x25;
        nCount = 4;
    }

    SimSetState (TRUE, TRUE, FALSE);                    // No RAMDAC writes
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);
//  SimSetCaptureMode (CAP_COMPOSITE);
    GetResolution (&wXRes, &wYRes);

    SetLine4Columns ((WORD) (wXRes / 8));
    Line4 (0, 0, (WORD) (wXRes - 1), 0, 0x0F);
    Line4 ((WORD) (wXRes - 1), 0, (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
    Line4 (0, (WORD) (wYRes - 1), (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
    Line4 (0, 0, 0, (WORD) (wYRes - 1), 0x0F);
    Line4 (0, 0, (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
    Line4 (0, (WORD) (wYRes - 1), (WORD) (wXRes - 1), 0, 0x0F);
    SimDumpMemory ("T0323.VGA");

    IOByteWrite (CRTC_CINDEX, 0x11);
    IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) & 0x7F));  // Disable write protect
    for (i = 0; i < nCount; i++)
    {
        IOByteWrite (CRTC_CINDEX, 0x01);
        IOByteWrite (CRTC_CDATA, byDisplayE);

        IOByteWrite (CRTC_CINDEX, 0x02);
        IOByteWrite (CRTC_CDATA, byBlankS);

        IOByteWrite (CRTC_CINDEX, 0x04);
        IOByteWrite (CRTC_CDATA, bySyncS);
        IOByteWrite (CRTC_CINDEX, 0x05);
        IOByteWrite (CRTC_CDATA, bySyncE);
        bySyncS++;
        bySyncE = (bySyncE & 0xE0) | ((bySyncE + 1) & 0x1F);
        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                break;
            }
        }
    }

SyncPulseTimingTestLW50_exit:
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        3
//
//      T1103
//      TextModeSkewTestLW50 - Set a known graphics mode video timing and compare the image after setting the text mode bit.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int TextModeSkewTestLW50 (void)
{
    int     nErr;
//  WORD    wSimType;
    BOOL    bFullVGA;

    nErr = ERROR_NONE;
//  wSimType = SimGetType ();
//  bFullVGA = !((wSimType & SIM_TURBOACCESS) && (wSimType & SIM_SIMULATION));
    bFullVGA = SimGetFrameSize ();

//  if (!bFullVGA) SimSetFrameSize (FALSE);         // Use small frame
    SimSetState (TRUE, TRUE, FALSE);                // No RAMDAC writes
    SetMode (0x12);

    // unlock registers
    IOByteWrite (CRTC_CINDEX, 0x11);
    IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) & 0x7F);

    // Move blank over to accomidate sync movement
    if (bFullVGA)
    {
      IOByteWrite (CRTC_CINDEX, 0x02);
      IOByteWrite (CRTC_CDATA, 0x4f);
    } else {
      IOByteWrite (CRTC_CINDEX, 0x02);
      IOByteWrite (CRTC_CDATA, 0x27);
    }

//  SimSetCaptureMode (CAP_COMPOSITE);

    IOByteRead (INPUT_CSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x31);
    IOByteWrite (ATC_INDEX, 0x3F);              // Turn on overscan

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto TextModeSkewTestLW50_exit;
        }
    }

    IOByteRead (INPUT_CSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x30);
    IOByteWrite (ATC_INDEX, 0x00);          // Treat pixel pipeline as "text"
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto TextModeSkewTestLW50_exit;
        }
    }

TextModeSkewTestLW50_exit:
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        4
//
//      T1104
//      VerticalTimesTwoTestLW50 - Set a known mode using different sets of CRTC values (one normal, the other vert times 2)
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int VerticalTimesTwoTestLW50 (void)
{
    static WORD tblCRTCFull[] = {
        0x0011, 0x0706, 0x1107, 0xF610, 0x0711, 0xEF12, 0xF315, 0x0416, 0xE717
    };
    static WORD tblCRTCSmall[] = {
//      0x0011, 0x0B06, 0x1007, 0x0A10, 0x0B11, 0x0912, 0x0A15, 0x0B16, 0xE717
        0x0011, 0x0D06, 0x1007, 0x0B10, 0x0C11, 0x0912, 0x0A15, 0x0D16, 0xE717
    };
    int     nErr, i, ntbl;
    WORD    wXRes, wYRes, wYStart, wLinear;
//  WORD    wSimType;
    BOOL    bFullVGA;

    ntbl = sizeof (tblCRTCFull) / sizeof (WORD);
    nErr = ERROR_NONE;
//  wSimType = SimGetType ();
//  bFullVGA = !((wSimType & SIM_TURBOACCESS) && (wSimType & SIM_SIMULATION));
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto VerticalTimesTwoTestLW50_exit;

//  if (!bFullVGA) SimSetFrameSize (FALSE);     // Use small frame
    SimSetState (TRUE, TRUE, FALSE);                    // No RAMDAC writes
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);
//  SimSetCaptureMode (CAP_COMPOSITE);
    GetResolution (&wXRes, &wYRes);

    // Draw a pattern
    SetLine4Columns ((WORD) (wXRes / 8));
    Line4 (0, 0, (WORD) (wXRes - 1), 0, 0x0F);
    Line4 ((WORD) (wXRes - 1), 0, (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
    Line4 (0, (WORD) (wYRes - 1), (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
    Line4 (0, 0, 0, (WORD) (wYRes - 1), 0x0F);
    Line4 (0, 0, (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
    Line4 (0, (WORD) (wYRes - 1), (WORD) (wXRes - 1), 0, 0x0F);

    // Start the second image one scan line after the end of the first image
    wYStart = wYRes + 1;
    Line4 (0, wYStart, (WORD) (wXRes - 1), wYStart, 0x0F);
    Line4 ((WORD) (wXRes - 1), wYStart, (WORD) (wXRes - 1), (WORD) (wYStart + wYRes/2 - 1), 0x0F);
    Line4 (0, (WORD) (wYStart + wYRes/2 - 1), (WORD) (wXRes - 1), (WORD) (wYStart + wYRes/2 - 1), 0x0F);
    Line4 (0, wYStart, 0, (WORD) (wYStart + wYRes/2 - 1), 0x0F);
    Line4 (0, wYStart, (WORD) (wXRes - 1), (WORD) (wYStart + wYRes/2 - 1), 0x0F);
    Line4 (0, (WORD) (wYStart + wYRes/2 - 1), (WORD) (wXRes - 1), wYStart, 0x0F);

    SimDumpMemory ("T0325.VGA");

    SimComment ("Normal Image");
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto VerticalTimesTwoTestLW50_exit;
        }
    }

    // Change the CRTC values
    SimComment ("Set Vertical X2 Values");
    for (i = 0; i < ntbl; i++)
    {
        if (bFullVGA)
            IOWordWrite (CRTC_CINDEX, tblCRTCFull[i]);
        else
            IOWordWrite (CRTC_CINDEX, tblCRTCSmall[i]);
    }

    SimComment ("Vertical X2 Image");
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto VerticalTimesTwoTestLW50_exit;
        }
    }

    // Show interaction of vertical times two and double scan
    // Change the display start address to start at the second image
    SimComment ("Change the display start address to start at the second image.");
    wLinear = (wXRes / 8) * (wYStart);
    IOByteWrite (CRTC_CINDEX, 0x0C);
    IOByteWrite (CRTC_CDATA, HIBYTE (wLinear));
    IOByteWrite (CRTC_CINDEX, 0x0D);
    IOByteWrite (CRTC_CDATA, LOBYTE (wLinear));

    // Set double scan
    IOWordWrite (CRTC_CINDEX, 0xC009);
    WaitNotVerticalRetrace ();

    SimComment ("Vertical X2 Image with Double Scan");
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

VerticalTimesTwoTestLW50_exit:
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        5
//
//      T1105
//      SkewTestLW50 - In mode 12h, set a known pattern and visually inspect the sync skew and display skew.
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int SkewTestLW50 (void)
{
    int     nErr, i;
    WORD    wXRes, wYRes;
//  WORD    wSimType;
    BOOL    bFullVGA;

    nErr = ERROR_NONE;
//  wSimType = SimGetType ();
//  bFullVGA = !((wSimType & SIM_TURBOACCESS) && (wSimType & SIM_SIMULATION));
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto SkewTestLW50_exit;

    // Set 640x480 graphics, draw a rectangle around the screen, and
    // draw two diagonal lines across the center.
//  if (!bFullVGA) SimSetFrameSize (FALSE);         // Use small frame
    SimSetState (TRUE, TRUE, FALSE);
    SetMode (0x12);

//  SimSetCaptureMode (CAP_COMPOSITE);
    GetResolution (&wXRes, &wYRes);
    SimSetState (TRUE, TRUE, TRUE);

    SetLine4Columns (wXRes / 8);
    Line4 (0, 0, wXRes - 1, 0, 0x0F);
    Line4 (wXRes -1, 0, wXRes - 1, wYRes - 1, 0x0F);
    Line4 (0, wYRes -1, wXRes - 1, wYRes -1, 0x0F);
    Line4 (0, 0, 0, wYRes, 0x0F);
    Line4 (0, 0, wXRes - 1, wYRes - 1, 0x0F);
    Line4 (0, wYRes - 1, wXRes - 1, 0, 0x0F);

    // Unlock CRTC
    IOByteWrite (CRTC_CINDEX, 0x11);
    IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) & 0x7F));

    // Shorten display enable to accomidate blank
    if (bFullVGA)
    {
      IOByteWrite (CRTC_CINDEX, 0x01);
      IOByteWrite (CRTC_CDATA, 0x4d);
    } else {
      IOByteWrite (CRTC_CINDEX, 0x01);
      IOByteWrite (CRTC_CDATA, 0x25);
    }

    SimDumpMemory ("T0326.VGA");

    // Verify screen is as expected
    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
        {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto SkewTestLW50_exit;
        }
    }

    for (i = 0; i < 4; i++)
    {
        IOByteWrite (CRTC_CINDEX, 0x03);
        IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) & 0x9F);
        IOByteWrite (CRTC_CINDEX, 0x05);
        IOByteWrite (CRTC_CDATA, (BYTE)((IOByteRead (CRTC_CDATA) & 0x9F) | (i << 5)));

        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto SkewTestLW50_exit;
            }
        }
    }
    for (i = 0; i < 4; i++)
    {
        IOByteWrite (CRTC_CINDEX, 0x03);
        IOByteWrite (CRTC_CDATA, (BYTE)((IOByteRead (CRTC_CDATA) & 0x9F) | (i << 5)));
        IOByteWrite (CRTC_CINDEX, 0x05);
        IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) & 0x9F);

        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto SkewTestLW50_exit;
            }
        }
    }

    for (i = 0; i < 4; i++)
    {
        IOByteWrite (CRTC_CINDEX, 0x03);
        IOByteWrite (CRTC_CDATA, (BYTE)((IOByteRead (CRTC_CDATA) & 0x9F) | (i << 5)));
        IOByteWrite (CRTC_CINDEX, 0x05);
        IOByteWrite (CRTC_CDATA, (BYTE)((IOByteRead (CRTC_CDATA) & 0x9F) | (i << 5)));

        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto SkewTestLW50_exit;
            }
        }
    }

SkewTestLW50_exit:
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        6
//
//      T1106
//      LwrsorBlinkTestLW50 - In text mode, test the actual cursor blink rate
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
//      Notes:  This test tests the normal blink rate of the cursor, not the fast blink setting that
//              the other text tests use. The logic is as follows:
//                  1)  Set text mode and put useful data on the screen
//                  2)  Disable the fast blink logic (bypass it)
//                  3)  Wait for the cursor blink to start
//                  4)  Using continuous capture, capture 32 frames where the first 8 should show the
//                      cursor, the second 8 should not, the third 8 should show the cursor again, and
//                      the last 8 should not.
//
int LwrsorBlinkTestLW50 (void)
{
    int         xPos[2], yPos[2], nCols, i, nFrames;
    BYTE        attr;
    BOOL        bFullVGA;
    char        szMsg[80];
    static char szMsgLwrsor[] = "Blinking cursor: ";
    static char szMsgAttribute[] = "Blinking attributes!";

    bFullVGA = SimGetFrameSize ();
    nFrames = 64;

    if (bFullVGA)
    {
        nCols = 80;
        xPos[0] = (nCols - (int)strlen (szMsgAttribute)) / 2;
        yPos[0] = 12;
        xPos[1] = (nCols - (int)strlen (szMsgLwrsor)) / 2;
        yPos[1] = 13;
    }
    else
    {
        nCols = 40;
        xPos[0] = 0;
        xPos[1] = (int)strlen (szMsgAttribute) + 1;
        yPos[0] = yPos[1] = 0;
    }

    // Set text mode
    SimSetState (TRUE, TRUE, FALSE);                    // No RAMDAC writes
    SetMode (0x03);
    SimSetState (TRUE, TRUE, TRUE);

    // Put a pretty picture on the screen
    for (i = 0; i < (int) strlen (szMsgAttribute); i++)
    {
        attr = ((i & 0x0F) << 4) | 0x07;
        TextCharOut (szMsgAttribute[i], attr, xPos[0] + i, yPos[0], 0);
    }
    TextStringOut (szMsgLwrsor, 0x07, xPos[1], yPos[1], 0);

    SimDumpMemory ("T1107.VGA");

#ifdef LW_MODS
    // Disable fastblink (set on modeset)
    SimComment ("Disable fastblink.");
    IOWordWrite (CRTC_CINDEX, 0x573F);
    IOByteWrite (CRTC_CINDEX, 0x20);                                                // LW_CIO_CRE_BLINK_PHASE__INDEX
    IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) & 0x7F);

    // Start the frame capture
    SimComment ("Let the frame settle.");
    LegacyVGA::CRCManager()->ClearFrameState ();
    LegacyVGA::CRCManager()->ClearClockChanged ();
    DispTest::UpdateNow (LegacyVGA::vga_timeout_ms());                               // Send an update and wait a few frames
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse

    // Set the frame state to wait for blink
    SimComment ("Set the frame state to wait for blink.");
    LegacyVGA::CRCManager ()->WaitBlinkState (0, LegacyVGA::vga_timeout_ms());

    // Trigger the continuous capture
    SimComment ("Trigger the continuous capture.");
    LegacyVGA::CRCManager ()->CaptureFrame (0, 1, 0, LegacyVGA::vga_timeout_ms());    // This needs the WaitVerticalRetrace that follows
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse

    // Loop on this
    SimComment ("Start the capture.");
    for (i = 0; i < nFrames - 1; i++)
    {
        sprintf (szMsg, "Frame #%d.", i);
        SimComment (szMsg);
        LegacyVGA::CRCManager()->CaptureFrame(1, 1, 0, LegacyVGA::vga_timeout_ms());
    }

    // Last frame is special
    SimComment ("Last frame.");
    LegacyVGA::CRCManager ()->WaitCapture(1, 0, LegacyVGA::vga_timeout_ms());
#else
    szMsg[0] = ' ';         // Prevent compiler warning
    SimGetKey ();
#endif

    SimComment ("Cursor Blink Test completed.");
    SystemCleanUp ();
    return ERROR_NONE;
}

#undef  REF_TEST
#define REF_TEST        7
//
//      T1107
//      GraphicsBlinkTestLW50 - In graphics mode, test the actual blink rate
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
//      Notes:  This test tests the normal blink rate of graphics mode, not the fast blink setting that
//              the other graphics tests use. The logic is as follows:
//                  1)  Set graphics mode and put useful data on the screen
//                  2)  Load an appropriate palette
//                  3)  Disable the fast blink logic (bypass it)
//                  4)  Wait for graphics blink to start
//                  5)  Using continuous capture, capture 64 frames where the first 16 should show normal,
//                      the second 16 should show the blink state, the third 16 should show normal again,
//                      and the last 16 should show the blink state again.
//
int GraphicsBlinkTestLW50 (void)
{
    int         i, j, k, l, yStart[2], nFrames;
    BOOL        bFullVGA;
    DWORD       offset;
    int         nHeight, nStride, nBlockWidth;
    SEGOFF      lpVideo;
    BYTE        attr;
    char        szMsg[80];

    bFullVGA = SimGetFrameSize ();
    nFrames = 64;

    lpVideo = (SEGOFF) 0xA0000000;
    if (bFullVGA)
    {
        nStride = 80;                                       // Stride = width in bytes
        nHeight = 16;
        yStart[0] = 224;
        yStart[1] = yStart[0] + nHeight;
    }
    else
    {
        nStride = 40;
        nHeight = 8;
        yStart[0] = 0;
        yStart[1] = yStart[0] + nHeight;
    }

    // Set graphics mode
    SimSetState (TRUE, TRUE, FALSE);                        // No RAMDAC writes
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);

    // Put a pretty picture on the screen
    attr = 0;
    nBlockWidth = nStride / 8;
    for (i = 0; i < 2; i++)                                 // Two rows of blocks
    {
        offset = yStart[i] * nStride;
        for (j = 0; j < 8; j++)                             // Eight blocks per row
        {
            IOByteWrite (SEQ_INDEX, 0x02);                  // Color plane enable
            IOByteWrite (SEQ_DATA, attr++);
            for (k = 0; k < nHeight; k++)                   // Draw the block
            {
                for (l = 0; l < nBlockWidth; l++)
                {
                    MemByteWrite (lpVideo + offset + l + (k * nStride), 0xFF);
                }
            }
            offset += nBlockWidth;
        }
    }

    SimDumpMemory ("T1108.VGA");

    // Bright white overscan
    SimComment ("Turn on overscan.");
    IOByteRead (INPUT_CSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x31);
    IOByteWrite (ATC_INDEX, 0x3F);

    // Enable blink
    SimComment ("Enable graphics blink.");
    IOByteWrite (ATC_INDEX, 0x30);
    IOByteWrite (ATC_INDEX, 0x09);

#ifdef LW_MODS
    // Disable fastblink (set on modeset)
    SimComment ("Disable fastblink.");
    IOWordWrite (CRTC_CINDEX, 0x573F);
    IOByteWrite (CRTC_CINDEX, 0x20);                                                // LW_CIO_CRE_BLINK_PHASE__INDEX
    IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) & 0x7F);

    // Start the frame capture
    SimComment ("Let the frame settle.");
    LegacyVGA::CRCManager()->ClearFrameState ();
    LegacyVGA::CRCManager()->ClearClockChanged ();
    DispTest::UpdateNow (LegacyVGA::vga_timeout_ms());                                 // Send an update and wait a few frames
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse

    // Set the frame state to wait for blink
    SimComment ("Set the frame state to wait for blink.");
    LegacyVGA::CRCManager ()->WaitBlinkState (0, LegacyVGA::vga_timeout_ms());

    // Trigger the continuous capture
    SimComment ("Trigger the continuous capture.");
    LegacyVGA::CRCManager ()->CaptureFrame (0, 1, 0, LegacyVGA::vga_timeout_ms());    // This needs the WaitVerticalRetrace that follows
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse

    // Loop on this
    SimComment ("Start the capture.");
    for (i = 0; i < nFrames - 1; i++)
    {
        sprintf (szMsg, "Frame #%d.", i);
        SimComment (szMsg);
        LegacyVGA::CRCManager ()->CaptureFrame(1, 1, 0, LegacyVGA::vga_timeout_ms());
    }

    // Last frame is special
    SimComment ("Last frame.");
    LegacyVGA::CRCManager ()->WaitCapture(1, 0, LegacyVGA::vga_timeout_ms());
#else
    szMsg[0] = ' ';         // Prevent compiler warning
    SimGetKey ();
#endif

    SimComment ("Graphics Blink Test completed.");
    SystemCleanUp ();
    return ERROR_NONE;
}

#undef  REF_TEST
#define REF_TEST        8
//
//      T1108
//      MethodGenTestLW50 - Test methods generated from I/O operations
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
//      Notes:  From Aron's test description:
//
//              Checks that the VBIOS mechanism of indirectly generating methods via I/O or PRIV registers
//              (by way of RMA) works for all methods that would be required during VBIOS operation.  There
//              is 1 subtest. Methods should be generated with all possible values in each field.
//
//         Procedure to add a new method:
//
//             1) Modify the data structure "VGAMETHOD" to include the new method
//             2) Add the appropriate data to the two tables, "lwGAMethods" and "rVGAMethods",
//                which are both arrays for the above data structure. A "1" entry means the
//                method is generated by the given register on an I/O write.
//             3) Add the expected method output to the functions "_MethodGenPrintRegister_rVGA"
//                and "_MethodGenPrintRegister_lwGA".
//
//         Procedure to add a new register:
//
//             1) Add the register into both tables, "lwGAMethods" and "rVGAMethods", and
//                flag which methods are generated by that register write.
//             2) Add the I/O write to the functions "_MethodGenOutputRegister_rVGA" and
//                "_MethodGenOutputRegister_lwGA".
//

int MethodGenTestLW50 (void)
{
#ifndef LW_MODS
    printf ("MethodGenTestLW50 can only be exelwted under RTL simulation.\n");
    return (ERROR_INTERNAL);
#else
    static VGAMETHOD    lwGAMethods[] =
/*                                            N N N N N N N N N N N N N N N N N N N N N N N N N N N
                                              V V V V V V V V V V V V V V V V V V V V V V V V V V V
                                              - - - - - - - - - - - - - - - - - - - - - - - - - - -
                                              5 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5 D D D D D D D D 5 5
                                              0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 I I I I I I I I 0 0
                                              7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 S S S S S S S S 7 7
                                              D D D D D D D D D D D D D D D D D P P P P P P P P D D
                                              - - - - - - - - - - - - - - - - - P P P P P P P P - -
                                              C C C C C C C C C C C C C C C C C V V V V V V V V C C
                                              O O O O O O O O O O O O O O O O O T T T T T T T T O O
                                              R R R R R R R R R R R R R R R R R - - - - - - - - R R
                                              E E E E E E E E E E E E E E E E E C C C C C C C C E E
                                              - - - - - - - - - - - - - - - - - O O O O O O O O - -
                                              D D S P H H H H H H H H H H H H H R R R R R R R R H U
                                              A A 0 I E E E E E E E E E E E E E E E E E E E E E E P
                                              C C R O A A A A A A A A A A A A A - - - - - - - - A D
                                              - - - R D D D D D D D D D D D D D V V V V V V V V D A
                                              S S S - - - - - - - - - - - - - - G G G G G G G G 0 T
                                              E E E S S S S S S S S S S S S S S A A A A A A A A - E
                                              T T T E E E E E E E E E E E E E E C C V A A D F F S |
                                              C P C T T T T T T T T T T T T T T O O I C C I B B E |
                                              O O O C P C R R R R R D C P V V V N N E T T S C C T |
                                              N L N O I O A A A A A I O R I I I T T W I I P O O L |
                                              T A T N X N S S S S S T N O E E E R R P V V L N N E |
                                              R R R T E T T T T T T H T C W W W 0 O O E E A T T G |
                                              O I O R L R E E E E E E R A P P P L L R S E Y R R A |
                                              L T L O C O R R R R R R O M O O O 0 1 T T N S A A C |
                                              | Y | L L L S S B B V C L P R R R | | E A D I C C Y |
                                              | | | | O | I Y L L E O O | T T T | | N R | Z T T C |
                                              | | | | C | Z N A A R N U | S P S | | D T | E P P R |
                                              | | | | K | E C N N T T T | I O I | | | | | | A A C |
                                              | | | | | | | E K K B R P | Z I Z | | | | | | R R C |
                                              | | | | | | | N E S L O U | E N E | | | | | | M M O |
                                              | | | | | | | D N T A L T | I T O | | | | | | S S N |
                                              | | | | | | | | D A N | S | N O U | | | | | | 0 1 T |
                                              | | | | | | | | | R K | C | | U T | | | | | | | | R |
                                              | | | | | | | | | T 2 | A | | T | | | | | | | | | O |
                                              | | | | | | | | | | | | L | | A | | | | | | | | | L |
                                              | | | | | | | | | | | | E | | D | | | | | | | | | | |
                                              | | | | | | | | | | | | R | | J | | | | | | | | | | |
                                              | | | | | | | | | | | | | | | U | | | | | | | | | | |
                                              | | | | | | | | | | | | | | | S | | | | | | | | | | |
                                              | | | | | | | | | | | | | | | T | | | | | | | | | | |
*/
    {
        {"MISC",0x0,                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xe3, FALSE},
        {"SR",0x1,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x01, FALSE},
        {"SR",0x2,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"SR",0x3,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"SR",0x4,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x06, FALSE},
        {"AR",0x0,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x1,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"AR",0x2,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x02, FALSE},
        {"AR",0x3,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x03, FALSE},
        {"AR",0x4,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x04, FALSE},
        {"AR",0x5,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x05, FALSE},
        {"AR",0x6,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x14, FALSE},
        {"AR",0x7,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x07, FALSE},
        {"AR",0x8,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x38, FALSE},
        {"AR",0x9,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x39, FALSE},
        {"AR",0xa,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3a, FALSE},
        {"AR",0xb,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3b, FALSE},
        {"AR",0xc,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3c, FALSE},
        {"AR",0xd,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3d, FALSE},
        {"AR",0xe,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3e, FALSE},
        {"AR",0xf,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"AR",0x10,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x01, FALSE},
        {"AR",0x11,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x12,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"AR",0x13,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x14,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x0,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x1,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x2,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x3,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x4,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x5,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x6,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x05, FALSE},
        {"GR",0x7,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"GR",0x8,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xff, FALSE},
        {"CR",0x0,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x2d, FALSE},
        {"CR",0x1,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x27, FALSE},
        {"CR",0x2,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x28, FALSE},
        {"CR",0x3,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x90, FALSE},
        {"CR",0x4,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x2b, FALSE},
        {"CR",0x5,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x80, FALSE},
        {"CR",0x6,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x7,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x10, FALSE},
        {"CR",0x8,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x9,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x40, FALSE},
        {"CR",0xa,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xb,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xc,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xd,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xe,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xf,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x10,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x15, FALSE},
        {"CR",0x11,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x06, FALSE},
        {"CR",0x12,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x13, FALSE},
        {"CR",0x13,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x14, FALSE},
        {"CR",0x14,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x15,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x14, FALSE},
        {"CR",0x16,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x17,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0xe3, FALSE},
        {"CR",0x18,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xff, FALSE},
        {"CR",0x19,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, TRUE},
        {"CR",0x1a,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x02, FALSE},
        {"CR",0x1b,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x1c,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1d,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1e,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1f,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x20,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x28,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x29,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2a,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2b,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x2c,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2d,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2e,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x30,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x31,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x34,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x35,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x38,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x39,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x3a,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x3b,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x40,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x2d, FALSE},
        {"CR",0x41,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x42,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x27, FALSE},
        {"CR",0x43,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x44,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x28, FALSE},
        {"CR",0x45,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x46,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x20, FALSE},
        {"CR",0x47,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x48,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x2b, FALSE},
        {"CR",0x49,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x4a,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x4b,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x50,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x51,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x52,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x13, FALSE},
        {"CR",0x53,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x54,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x14, FALSE},
        {"CR",0x55,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x56,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x57,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x58,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x15, FALSE},
        {"CR",0x59,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x5a,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x06, FALSE},
        {"CR",0x5b,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x60,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x8f, FALSE},
        {"CR",0x61,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x62,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"CR",0x63,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x64,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x65,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x66,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"CR",0x67,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x68,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x47, FALSE},
        {"CR",0x69,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x6a,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x67, FALSE},
        {"CR",0x6b,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x70,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x1b, FALSE},
        {"CR",0x71,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x72,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x18, FALSE},
        {"CR",0x73,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x74,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x75,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x76,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x18, FALSE},
        {"CR",0x77,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x78,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x19, FALSE},
        {"CR",0x79,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x7a,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x1a, FALSE},
        {"CR",0x7b,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
//      {"CR",0xa9,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1, 0x00, FALSE},
        {"CR",0xaa,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"DAC_SET_CONTROL",0x0,               1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"DAC_SET_POLARITY",0x0,              0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"SOR_SET_CONTROL",0x0,               0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"PIOR_SET_CONTROL",0x0,              0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_PIXEL_CLOCK",0x0,          0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_CONTROL",0x0,              0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_DITHER_CONTROL",0x0,       0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_CONTROL_OUTPUT_SCALER",0x0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_PROCAMP",0x0,              0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_HTOTAL",0x0,                     0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x0000018f, FALSE},
        {"BE_HDISPLAY_END",0x0,               0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x0000013f, FALSE},
        {"BE_HVALID_START",0x0,               0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_HVALID_END",0x0,                 0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x0000013f, FALSE},
        {"BE_HSYNC_START",0x0,                0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000147, FALSE},
        {"BE_HSYNC_END",0x0,                  0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000167, FALSE},
        {"BE_VTOTAL",0x0,                     0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x0000001b, FALSE},
        {"BE_VDISPLAY_END",0x0,               0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000018, FALSE},
        {"BE_VVALID_START",0x0,               0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_VVALID_END",0x0,                 0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000018, FALSE},
        {"BE_VSYNC_START",0x0,                0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000019, FALSE},
        {"BE_VSYNC_END",0x0,                  0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x0000001a, FALSE}
    };
    static VGAMETHOD    rVGAMethods[] =
    {
        {"MISC",0x0,                          0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xe3, FALSE},
        {"SR",0x1,                            0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x01, FALSE},
        {"SR",0x2,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"SR",0x3,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"SR",0x4,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x06, FALSE},
        {"AR",0x0,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x1,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"AR",0x2,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x02, FALSE},
        {"AR",0x3,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x03, FALSE},
        {"AR",0x4,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x04, FALSE},
        {"AR",0x5,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x05, FALSE},
        {"AR",0x6,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x14, FALSE},
        {"AR",0x7,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x07, FALSE},
        {"AR",0x8,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x38, FALSE},
        {"AR",0x9,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x39, FALSE},
        {"AR",0xa,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3a, FALSE},
        {"AR",0xb,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3b, FALSE},
        {"AR",0xc,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3c, FALSE},
        {"AR",0xd,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3d, FALSE},
        {"AR",0xe,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3e, FALSE},
        {"AR",0xf,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"AR",0x10,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x01, FALSE},
        {"AR",0x11,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x12,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"AR",0x13,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x14,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x0,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x1,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x2,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x3,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x4,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x5,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x6,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x05, FALSE},
        {"GR",0x7,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"GR",0x8,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xff, FALSE},
        {"CR",0x0,                            0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x2d, FALSE},
        {"CR",0x1,                            0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x27, FALSE},
        {"CR",0x2,                            0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x28, FALSE},
        {"CR",0x3,                            0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x90, FALSE},
        {"CR",0x4,                            0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x2b, FALSE},
        {"CR",0x5,                            0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x80, FALSE},
        {"CR",0x6,                            0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x7,                            0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x10, FALSE},
        {"CR",0x8,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x9,                            0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x40, FALSE},
        {"CR",0xa,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xb,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xc,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xd,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xe,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xf,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x10,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x15, FALSE},
        {"CR",0x11,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x06, FALSE},
        {"CR",0x12,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x13, FALSE},
        {"CR",0x13,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x14, FALSE},
        {"CR",0x14,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x15,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x14, FALSE},
        {"CR",0x16,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x17,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0xe3, FALSE},
        {"CR",0x18,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xff, FALSE},
        {"CR",0x19,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x10, TRUE},
        {"CR",0x1a,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x02, FALSE},
        {"CR",0x1b,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x1c,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1d,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1e,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1f,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x20,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x28,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x29,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2a,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2b,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x2c,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2d,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2e,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x30,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x31,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x34,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x35,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x38,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x39,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x3a,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x3b,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x40,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x2d, FALSE},
        {"CR",0x41,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x42,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x27, FALSE},
        {"CR",0x43,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x44,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x28, FALSE},
        {"CR",0x45,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x46,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x20, FALSE},
        {"CR",0x47,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x48,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x2b, FALSE},
        {"CR",0x49,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x4a,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x4b,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x50,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x51,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x52,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x13, FALSE},
        {"CR",0x53,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x54,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x14, FALSE},
        {"CR",0x55,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x56,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x57,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x58,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x15, FALSE},
        {"CR",0x59,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x5a,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x06, FALSE},
        {"CR",0x5b,                           0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x60,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x8f, FALSE},
        {"CR",0x61,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x62,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"CR",0x63,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x64,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x65,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x66,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"CR",0x67,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x68,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x47, FALSE},
        {"CR",0x69,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x6a,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x67, FALSE},
        {"CR",0x6b,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x70,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x1b, FALSE},
        {"CR",0x71,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x72,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x18, FALSE},
        {"CR",0x73,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x74,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x75,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x76,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x18, FALSE},
        {"CR",0x77,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x78,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x19, FALSE},
        {"CR",0x79,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x7a,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x1a, FALSE},
        {"CR",0x7b,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
//      {"CR",0xa9,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1, 0x00, FALSE},
        {"CR",0xaa,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"DAC_SET_CONTROL",0x0,               1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"DAC_SET_POLARITY",0x0,              0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"SOR_SET_CONTROL",0x0,               0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"PIOR_SET_CONTROL",0x0,              0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_PIXEL_CLOCK",0x0,          0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_CONTROL",0x0,              0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_DITHER_CONTROL",0x0,       0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_CONTROL_OUTPUT_SCALER",0x0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_PROCAMP",0x0,              0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_HTOTAL",0x0,                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0000018f, FALSE},
        {"BE_HDISPLAY_END",0x0,               0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0000013f, FALSE},
        {"BE_HVALID_START",0x0,               0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_HVALID_END",0x0,                 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0000013f, FALSE},
        {"BE_HSYNC_START",0x0,                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000147, FALSE},
        {"BE_HSYNC_END",0x0,                  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000167, FALSE},
        {"BE_VTOTAL",0x0,                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0000001b, FALSE},
        {"BE_VDISPLAY_END",0x0,               0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000018, FALSE},
        {"BE_VVALID_START",0x0,               0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_VVALID_END",0x0,                 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000018, FALSE},
        {"BE_VSYNC_START",0x0,                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000019, FALSE},
        {"BE_VSYNC_END",0x0,                  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0000001a, FALSE}
    };
    int             nErr, nRegNum, rRegNum, i;
    char            szMsg[512];
    static char     szFileExpected[] = "expected.log";
    static char     szFileMethodOut[] = "core_method.out";
    static char     szFileActual[] = "actual.log";

    // Initialize local variables
    SimComment ("MethodGenTestLW50 started.");
    nErr = ERROR_NONE;
    nRegNum = sizeof (lwGAMethods) / sizeof (VGAMETHOD);
    rRegNum = sizeof (rVGAMethods) / sizeof (VGAMETHOD);
    LegacyVGA::CRCManager ()->GetHeadDac (&head_index, &dac_index);     // These variables are global to this module

    // Tell the user where the test is running
    sprintf (szMsg, "Starting with head index %d and dac index %d.", head_index, dac_index);
    SimComment (szMsg);

    // Initialize the VGA
#ifdef LW_MODS
    DispTest::IsoInitVGA (head_index, dac_index, 720, 400, 1, 1, false, 1, 300000,     // pclk_period = 3333
            0,  0, DispTest::GetGlobalTimeout ());
#endif

    // Open the log file "expected.strip"
    hFileExpected = fopen (szFileExpected, "w");
    if (hFileExpected == NULL)
    {
        Printf (Tee::PriNormal, "Could not create log file: %s\n", szFileExpected);
        return (ERROR_FILE);
    }

    // Point to color space
    IOByteWrite (MISC_OUTPUT, 0x01);
    // Unlock extended registers
    IOWordWrite (CRTC_CINDEX, 0x573F);
    // Set rVGA mode
    IOByteWrite (CRTC_CINDEX, 0x19);
    IOByteWrite (CRTC_CDATA, 0x10|(head_index<<7));
    // Unlock CRTC
    IOByteWrite (CRTC_CINDEX, 0x11);
    IOByteWrite (CRTC_CDATA, 0x00);
    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more

    // Reset registers with sane values
    for (i = 0; i < rRegNum; i++)
    {
        _MethodGenOutputAndPrint_rVGA (rVGAMethods[i], false);
    }

    // Clear memory - use a mode 12h clear, because the above register load for "sane" values
    // is close to mode 12h-type numbers.
    ModeClearMemory (0x12);                     // This allows gilding of the image files
    IOByteRead (INPUT_CSTATUS_1);               // Flush MODS

    // Signal the end of setup and the beginning of the method generation testing
    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more
    _MethodGenSignalMethodStart (0, 0, 0);
    _MethodGenSignalMethodStop ();
    fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");      // Signal end of method

    // Set registers followed by updates in rVGA mode
    for (i = 0; i < rRegNum; i++)
    {
        _MethodGenOutputAndPrint_rVGA (rVGAMethods[i], true);
    }

    // Set lwGA mode
    _MethodGenSignalMethodStart (0, 0, 0);
    IOByteWrite (CRTC_CINDEX, 0x19);
    IOByteWrite (CRTC_CDATA, head_index << 7);
    _MethodGenSignalMethodStop ();

    // Match Output for set lwGA mode
    fprintf (hFileExpected, "rVGA %s %02x\n", "CR", 0x19);
    fprintf (hFileExpected, "  vga                              Dac%d_SetPolarity 00000000\n", dac_index);
    fprintf (hFileExpected, "  vga                           Head%d_SetPixelClock 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                           Head%d_SetRasterSize 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                        Head%d_SetRasterSyncEnd 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                       Head%d_SetRasterBlankEnd 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                     Head%d_SetRasterBlankStart 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                     Head%d_SetRasterVertBlank2 00000000\n", head_index);
    fprintf (hFileExpected, "  vga               Head%d_SetViewportPointOutAdjust 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                      Head%d_SetViewportSizeOut 00000000\n", head_index);
    fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");      // Signal end of method

    // Set registers followed by updates in lwGA mode
    for (i = 0; i < nRegNum; i++)
    {
        _MethodGenOutputAndPrint_lwGA (lwGAMethods[i], true);
    }

    // Close the log file
    fclose (hFileExpected);

    if (Platform::GetSimulationMode () != Platform::Hardware)
    {
        // Process the "core_method.out" file, stripping out what's not needed
        if (_MethodGenProcessActual (szFileMethodOut, szFileActual))
        {
            // Diff the files
            if (!_MethodGelwerifyFiles (szFileExpected, szFileActual))
                nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
        }
        else
            nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

    // Report success - manually gilded
    if (nErr == ERROR_NONE)
        SimComment ("Method Generation Test passed.");
    else
        SimComment ("Method Generation Test failed.");
    SystemCleanUp ();
#ifdef LW_MODS
    DispTest::IsoShutDowlwGA (head_index, dac_index, DispTest::GetGlobalTimeout ());
#endif
    return (nErr);
#endif
}

//
//      T1108
//      MethodGenTestGF11x - Test methods generated from I/O operations
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
//      Notes:  From Aron's test description:
//
//              Checks that the VBIOS mechanism of indirectly generating methods via I/O or PRIV registers
//              (by way of RMA) works for all methods that would be required during VBIOS operation.  There
//              is 1 subtest. Methods should be generated with all possible values in each field.
//
//         Procedure to add a new method:
//
//             1) Modify the data structure "VGAMETHOD" to include the new method
//             2) Add the appropriate data to the two tables, "lwGAMethods" and "rVGAMethods",
//                which are both arrays for the above data structure. A "1" entry means the
//                method is generated by the given register on an I/O write.
//             3) Add the expected method output to the functions "_MethodGenPrintRegister_rVGA"
//                and "_MethodGenPrintRegister_lwGA".
//
//         Procedure to add a new register:
//
//             1) Add the register into both tables, "lwGAMethods" and "rVGAMethods", and
//                flag which methods are generated by that register write.
//             2) Add the I/O write to the functions "_MethodGenOutputRegister_rVGA" and
//                "_MethodGenOutputRegister_lwGA".
//

int MethodGenTestGF11x (void)
{
#ifndef LW_MODS
    printf ("MethodGenTestGF11x can only be exelwted under RTL simulation.\n");
    return (ERROR_INTERNAL);
#else
    static VGAMETHOD_GF11x    lwGAMethods[] =
/*                                                      N N N N N N N N N N N N N N N N N N N N N N N N N N N N
                                                        V V V V V V V V V V V V V V V V V V V V V V V V V V V V
                                                        - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                                                        9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 9 D D D D D D D D 9 9
                                                        0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 I I I I I I I I 0 0
                                                        7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 S S S S S S S S 7 7
                                                        D D D D D D D D D D D D D D D D D D P P P P P P P P D D
                                                        - - - - - - - - - - - - - - - - - - P P P P P P P P - -
                                                        C C C C C C C C C C C C C C C C C C V V V V V V V V C C
                                                        O O O O O O O O O O O O O O O O O O T T T T T T T T O O
                                                        R R R R R R R R R R R R R R R R R R - - - - - - - - R R
                                                        E E E E E E E E E E E E E E E E E E C C C C C C C C E E
                                                        - - - - - - - - - - - - - - - - - - O O O O O O O O - -
                                                        D S P H H H H H H H H H H H H H H H R R R R R R R R H U
                                                        A 0 I E E E E E E E E E E E E E E E E E E E E E E E E P
                                                        C R O A A A A A A A A A A A A A A A - - - - - - - - A D
                                                        - - R D D D D D D D D D D D D D D D V V V V V V V V D A
                                                        S S - _ - - - - - - - - - - - - - - G G G G G G G G 0 T
                                                        E E S S S S S S S S S S S S S S S S A A A A A A A A - E
                                                        T T E E E E E E E E E E E E E E E E C C V A A D F F S |
                                                        C C T T T T T T T T T T T T T T T T O O I C C I B B E |
                                                        O O C C P P C R R R R R D C P V V V N N E T T S C C T |
                                                        N N O O I I O A A A A A I O R I I I T T W I I P O O L |
                                                        T T N N X X N S S S S S T N O E E E R R P V V L N N E |
                                                        R R T T E E T T T T T T H T C W W W 0 O O E E A T T G |
                                                        O O R R L L R E E E E E E R A P P P L L R S E Y R R A |
                                                        L L O O C C O R R R R R R O M O O O 0 1 T T N S A A C |
                                                        | | L L L L L S S B B V C L P R R R | | E A D I C C Y |
                                                        | | | O O O | I Y L L E O O | T T T | | N R | Z T T C |
                                                        | | | U C C | Z N A A R N U | S P S | | D T | E P P R |
                                                        | | | T K K | E C N N T T T | I O I | | | | | | A A C |
                                                        | | | P C F | | E K K B R P | Z I Z | | | | | | R R C |
                                                        | | | U O R | | N E S L O U | E N E | | | | | | M M O |
                                                        | | | T N E | | D N T A L T | I T O | | | | | | S S N |
                                                        | | | R F Q | | | D A N | S | N O U | | | | | | 0 1 T |
                                                        | | | E I U | | | | R K | C | | U T | | | | | | | | R |
                                                        | | | S G E | | | | T 2 | A | | T | | | | | | | | | O |
                                                        | | | O U N | | | | | | | L | | A | | | | | | | | | L |
                                                        | | | U R C | | | | | | | E | | D | | | | | | | | | | |
                                                        | | | R A Y | | | | | | | R | | J | | | | | | | | | | |
                                                        | | | C T | | | | | | | | | | | U | | | | | | | | | | |
                                                        | | | E I | | | | | | | | | | | S | | | | | | | | | | |
                                                        | | | | O | | | | | | | | | | | T | | | | | | | | | | |
                                                        | | | | N | | | | | | | | | | | | | | | | | | | | | | |
*/
    {
        {"MISC",0x0,                                    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xe3, FALSE},
        {"SR",0x1,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x01, FALSE},
        {"SR",0x2,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"SR",0x3,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"SR",0x4,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x06, FALSE},
        {"AR",0x0,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x1,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"AR",0x2,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x02, FALSE},
        {"AR",0x3,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x03, FALSE},
        {"AR",0x4,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x04, FALSE},
        {"AR",0x5,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x05, FALSE},
        {"AR",0x6,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x14, FALSE},
        {"AR",0x7,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x07, FALSE},
        {"AR",0x8,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x38, FALSE},
        {"AR",0x9,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x39, FALSE},
        {"AR",0xa,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3a, FALSE},
        {"AR",0xb,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3b, FALSE},
        {"AR",0xc,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3c, FALSE},
        {"AR",0xd,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3d, FALSE},
        {"AR",0xe,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3e, FALSE},
        {"AR",0xf,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"AR",0x10,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x01, FALSE},
        {"AR",0x11,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x12,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"AR",0x13,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x14,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x0,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x1,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x2,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x3,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x4,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x5,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x6,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x05, FALSE},
        {"GR",0x7,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"GR",0x8,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xff, FALSE},
        {"CR",0x0,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x2d, FALSE},
        {"CR",0x1,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x27, FALSE},
        {"CR",0x2,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x28, FALSE},
        {"CR",0x3,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x90, FALSE},
        {"CR",0x4,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x2b, FALSE},
        {"CR",0x5,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x80, FALSE},
        {"CR",0x6,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x7,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x10, FALSE},
        {"CR",0x8,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x9,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x40, FALSE},
        {"CR",0xa,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xb,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xc,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xd,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xe,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xf,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x10,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x15, FALSE},
        {"CR",0x11,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x06, FALSE},
        {"CR",0x12,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x13, FALSE},
        {"CR",0x13,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x14, FALSE},
        {"CR",0x14,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x15,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x14, FALSE},
        {"CR",0x16,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x17,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0xe3, FALSE},
        {"CR",0x18,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xff, FALSE},
        {"CR",0x19,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, TRUE},
        {"CR",0x1a,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x02, FALSE},
        {"CR",0x1b,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x1c,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1d,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1e,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1f,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x20,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x28,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x29,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2a,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2b,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x2c,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2d,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2e,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x30,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x31,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x34,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x35,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x38,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x39,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x3a,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x3b,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x40,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x2d, FALSE},
        {"CR",0x41,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x42,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x27, FALSE},
        {"CR",0x43,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x44,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x28, FALSE},
        {"CR",0x45,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x46,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x20, FALSE},
        {"CR",0x47,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x48,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x2b, FALSE},
        {"CR",0x49,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x4a,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x4b,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x50,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x51,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x52,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x13, FALSE},
        {"CR",0x53,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x54,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x14, FALSE},
        {"CR",0x55,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x56,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x57,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x58,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x15, FALSE},
        {"CR",0x59,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x5a,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x06, FALSE},
        {"CR",0x5b,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x60,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x8f, FALSE},
        {"CR",0x61,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x62,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"CR",0x63,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x64,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x65,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x66,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"CR",0x67,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x68,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x47, FALSE},
        {"CR",0x69,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x6a,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x67, FALSE},
        {"CR",0x6b,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x70,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x1b, FALSE},
        {"CR",0x71,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x72,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x18, FALSE},
        {"CR",0x73,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x74,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x75,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x76,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x18, FALSE},
        {"CR",0x77,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x78,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x19, FALSE},
        {"CR",0x79,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x7a,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x1a, FALSE},
        {"CR",0x7b,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
//      {"CR",0xa9,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1, 0x00, FALSE},
        {"CR",0xaa,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"DAC_SET_CONTROL",0x0,                         1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"SOR_SET_CONTROL",0x0,                         0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"PIOR_SET_CONTROL",0x0,                        0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_CONTROL_OUTPUT_RESOURCE",0x0,        0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_PIXEL_CLOCK_FREQUENCY",0x0,          0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_PIXEL_CLOCK_CONFIGURATION",0x0,      0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_CONTROL",0x0,                        0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_DITHER_CONTROL",0x0,                 0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_CONTROL_OUTPUT_SCALER",0x0,          0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_PROCAMP",0x0,                        0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_HTOTAL",0x0,                               0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x0000018f, FALSE},
        {"BE_HDISPLAY_END",0x0,                         0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x0000013f, FALSE},
        {"BE_HVALID_START",0x0,                         0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_HVALID_END",0x0,                           0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x0000013f, FALSE},
        {"BE_HSYNC_START",0x0,                          0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000147, FALSE},
        {"BE_HSYNC_END",0x0,                            0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000167, FALSE},
        {"BE_VTOTAL",0x0,                               0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x0000001b, FALSE},
        {"BE_VDISPLAY_END",0x0,                         0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000018, FALSE},
        {"BE_VVALID_START",0x0,                         0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_VVALID_END",0x0,                           0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000018, FALSE},
        {"BE_VSYNC_START",0x0,                          0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x00000019, FALSE},
        {"BE_VSYNC_END",0x0,                            0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1, 0x0000001a, FALSE}
    };
    static VGAMETHOD_GF11x    rVGAMethods[] =
    {
        {"MISC",0x0,                                    0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xe3, FALSE},
        {"SR",0x1,                                      0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x01, FALSE},
        {"SR",0x2,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"SR",0x3,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"SR",0x4,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x06, FALSE},
        {"AR",0x0,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x1,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"AR",0x2,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x02, FALSE},
        {"AR",0x3,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x03, FALSE},
        {"AR",0x4,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x04, FALSE},
        {"AR",0x5,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x05, FALSE},
        {"AR",0x6,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x14, FALSE},
        {"AR",0x7,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x07, FALSE},
        {"AR",0x8,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x38, FALSE},
        {"AR",0x9,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x39, FALSE},
        {"AR",0xa,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3a, FALSE},
        {"AR",0xb,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3b, FALSE},
        {"AR",0xc,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3c, FALSE},
        {"AR",0xd,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3d, FALSE},
        {"AR",0xe,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3e, FALSE},
        {"AR",0xf,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"AR",0x10,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x01, FALSE},
        {"AR",0x11,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x12,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"AR",0x13,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"AR",0x14,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x0,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x1,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x2,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x3,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x4,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x5,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"GR",0x6,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x05, FALSE},
        {"GR",0x7,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0f, FALSE},
        {"GR",0x8,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xff, FALSE},
        {"CR",0x0,                                      0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x2d, FALSE},
        {"CR",0x1,                                      0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x27, FALSE},
        {"CR",0x2,                                      0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x28, FALSE},
        {"CR",0x3,                                      0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x90, FALSE},
        {"CR",0x4,                                      0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x2b, FALSE},
        {"CR",0x5,                                      0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x80, FALSE},
        {"CR",0x6,                                      0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x7,                                      0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x10, FALSE},
        {"CR",0x8,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x9,                                      0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x40, FALSE},
        {"CR",0xa,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xb,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xc,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xd,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xe,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0xf,                                      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x10,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x15, FALSE},
        {"CR",0x11,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x06, FALSE},
        {"CR",0x12,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x13, FALSE},
        {"CR",0x13,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x14, FALSE},
        {"CR",0x14,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x15,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x14, FALSE},
        {"CR",0x16,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x17,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0xe3, FALSE},
        {"CR",0x18,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0xff, FALSE},
        {"CR",0x19,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x10, TRUE},
        {"CR",0x1a,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x02, FALSE},
        {"CR",0x1b,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x1c,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1d,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1e,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x1f,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x20,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x28,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x29,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2a,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2b,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x2c,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2d,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x2e,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x30,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x31,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x34,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x35,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x38,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x39,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x3a,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x3b,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x40,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x2d, FALSE},
        {"CR",0x41,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x42,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x27, FALSE},
        {"CR",0x43,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x44,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x28, FALSE},
        {"CR",0x45,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x46,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x20, FALSE},
        {"CR",0x47,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x48,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x2b, FALSE},
        {"CR",0x49,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x4a,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x4b,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x50,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x51,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x52,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x13, FALSE},
        {"CR",0x53,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x54,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x14, FALSE},
        {"CR",0x55,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x56,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x17, FALSE},
        {"CR",0x57,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x58,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x15, FALSE},
        {"CR",0x59,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x5a,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x06, FALSE},
        {"CR",0x5b,                                     0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,1,1,0,0,1,1,1,1,0,0,0,1, 0x00, FALSE},
        {"CR",0x60,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x8f, FALSE},
        {"CR",0x61,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x62,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"CR",0x63,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x64,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x65,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x66,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x3f, FALSE},
        {"CR",0x67,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x68,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x47, FALSE},
        {"CR",0x69,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x6a,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x67, FALSE},
        {"CR",0x6b,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x01, FALSE},
        {"CR",0x70,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x1b, FALSE},
        {"CR",0x71,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x72,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x18, FALSE},
        {"CR",0x73,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x74,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x75,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x76,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x18, FALSE},
        {"CR",0x77,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x78,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x19, FALSE},
        {"CR",0x79,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"CR",0x7a,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x1a, FALSE},
        {"CR",0x7b,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
//      {"CR",0xa9,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1, 0x00, FALSE},
        {"CR",0xaa,                                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00, FALSE},
        {"DAC_SET_CONTROL",0x0,                         1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"SOR_SET_CONTROL",0x0,                         0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"PIOR_SET_CONTROL",0x0,                        0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_CONTROL_OUTPUT_RESOURCE",0x0,        0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_PIXEL_CLOCK_FREQUENCY",0x0,          0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_PIXEL_CLOCK_CONFIGURATION",0x0,      0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_CONTROL",0x0,                        0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_DITHER_CONTROL",0x0,                 0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_CONTROL_OUTPUT_SCALER",0x0,          0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"HEAD_SET_PROCAMP",0x0,                        0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_HTOTAL",0x0,                               0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0000018f, FALSE},
        {"BE_HDISPLAY_END",0x0,                         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0000013f, FALSE},
        {"BE_HVALID_START",0x0,                         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_HVALID_END",0x0,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0000013f, FALSE},
        {"BE_HSYNC_START",0x0,                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000147, FALSE},
        {"BE_HSYNC_END",0x0,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000167, FALSE},
        {"BE_VTOTAL",0x0,                               0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0000001b, FALSE},
        {"BE_VDISPLAY_END",0x0,                         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000018, FALSE},
        {"BE_VVALID_START",0x0,                         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000000, FALSE},
        {"BE_VVALID_END",0x0,                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000018, FALSE},
        {"BE_VSYNC_START",0x0,                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x00000019, FALSE},
        {"BE_VSYNC_END",0x0,                            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, 0x0000001a, FALSE}
    };
    int             nErr, nRegNum, rRegNum, i;
    char            szMsg[512];
    static char     szFileExpected[] = "expected.log";
    static char     szFileMethodOut[] = "core_method.out";
    static char     szFileActual[] = "actual.log";

    // Initialize local variables
    SimComment ("MethodGenTestLW50 started.");
    nErr = ERROR_NONE;
    nRegNum = sizeof (lwGAMethods) / sizeof (VGAMETHOD_GF11x);
    rRegNum = sizeof (rVGAMethods) / sizeof (VGAMETHOD_GF11x);
    LegacyVGA::CRCManager ()->GetHeadDac (&head_index, &dac_index);     // These variables are global to this module

    // Tell the user where the test is running
    sprintf (szMsg, "Starting with head index %d and dac index %d.", head_index, dac_index);
    SimComment (szMsg);

    // Initialize the VGA
#ifdef LW_MODS
    DispTest::IsoInitVGA (head_index, dac_index, 720, 400, 1, 1, false, 1, 300000000,     // pclk_period = 3333
            0,  0, DispTest::GetGlobalTimeout ());
#endif

    // Open the log file "expected.strip"
    hFileExpected = fopen (szFileExpected, "w");
    if (hFileExpected == NULL)
    {
        Printf (Tee::PriNormal, "Could not create log file: %s\n", szFileExpected);
        return (ERROR_FILE);
    }

    // Point to color space
    IOByteWrite (MISC_OUTPUT, 0x01);
    // Unlock extended registers
    IOWordWrite (CRTC_CINDEX, 0x573F);
    // Set rVGA mode
    IOByteWrite (CRTC_CINDEX, 0x19);
    IOByteWrite (CRTC_CDATA, 0x10|(head_index<<5));
    // Unlock CRTC
    IOByteWrite (CRTC_CINDEX, 0x11);
    IOByteWrite (CRTC_CDATA, 0x00);
    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more

    // Reset registers with sane values
    for (i = 0; i < rRegNum; i++)
    {
        _MethodGenOutputAndPrint_gf11x_rVGA (rVGAMethods[i], false);
    }

    // Clear memory - use a mode 12h clear, because the above register load for "sane" values
    // is close to mode 12h-type numbers.
    ModeClearMemory (0x12);                     // This allows gilding of the image files
    IOByteRead (INPUT_CSTATUS_1);               // Flush MODS

    // Signal the end of setup and the beginning of the method generation testing
    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more
    _MethodGenSignalMethodStart_gf11x (0, 0, 0);
    _MethodGenSignalMethodStop ();
    fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");      // Signal end of method

    // Set registers followed by updates in rVGA mode
    for (i = 0; i < rRegNum; i++)
    {
        _MethodGenOutputAndPrint_gf11x_rVGA (rVGAMethods[i], true);
    }

    // Set lwGA mode
    _MethodGenSignalMethodStart_gf11x (0, 0, 0);
    IOByteWrite (CRTC_CINDEX, 0x19);
    IOByteWrite (CRTC_CDATA, head_index << 5);
    _MethodGenSignalMethodStop ();

    // Match Output for set lwGA mode
    fprintf (hFileExpected, "rVGA %s %02x\n", "CR", 0x19);
    fprintf (hFileExpected, "  vga                Head%d_SetControlOutputResource 00000000\n", head_index);
    fprintf (hFileExpected, "  vga              Head%d_SetPixelClockConfiguration 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                  Head%d_SetPixelClockFrequency 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                           Head%d_SetRasterSize 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                        Head%d_SetRasterSyncEnd 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                       Head%d_SetRasterBlankEnd 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                     Head%d_SetRasterBlankStart 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                     Head%d_SetRasterVertBlank2 00000000\n", head_index);
    fprintf (hFileExpected, "  vga               Head%d_SetViewportPointOutAdjust 00000000\n", head_index);
    fprintf (hFileExpected, "  vga                      Head%d_SetViewportSizeOut 00000000\n", head_index);
    fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");      // Signal end of method

    // Set registers followed by updates in lwGA mode
    for (i = 0; i < nRegNum; i++)
    {
        _MethodGenOutputAndPrint_gf11x_lwGA (lwGAMethods[i], true);
    }

    // Close the log file
    fclose (hFileExpected);

    if (Platform::GetSimulationMode () != Platform::Hardware)
    {
        // Process the "core_method.out" file, stripping out what's not needed
        if (_MethodGenProcessActual (szFileMethodOut, szFileActual))
        {
            // Diff the files
            if (!_MethodGelwerifyFiles (szFileExpected, szFileActual))
                nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
        }
        else
            nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

    // Report success - manually gilded
    if (nErr == ERROR_NONE)
        SimComment ("Method Generation Test passed.");
    else
        SimComment ("Method Generation Test failed.");
    SystemCleanUp ();
#ifdef LW_MODS
    DispTest::IsoShutDowlwGA (head_index, dac_index, DispTest::GetGlobalTimeout ());
#endif
    return (nErr);
#endif
}

#ifdef LW_MODS
//
//      _MethodGenSignalMethodStart - Signal the beginning of a method to snoop
//
//      Entry:  bPriv       Priv vs. I/O flag (TRUE = Priv reg)
//              dwAddr      Register address
//              nIndex      Index (if indexed)
//      Exit:   None
//
void _MethodGenSignalMethodStart (BOOL bPriv, DWORD dwAddr, int nIndex)
{
    DWORD         dwData, dwRegVal, dwOffset, dwDbgCtl, dw1, dw2, dw3, dw4;
    REGFIELD      rfMethodOfs, rfMode, rfNewMethod, rfMethodType;
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

    dwOffset = 0x19;                // LW_DispPvt_Core_VgaControl1__ADDR
    if (bPriv)
        dwData = dwAddr;
    else
        dwData = (dwAddr << 16) | (nIndex & 0xFFFF) | 0x80000000;

    // Gather the needed field information
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_METHOD_OFS", &rfMethodOfs);
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", &rfMode);
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD", &rfNewMethod);
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE", &rfMethodType);

    fprintf (hFileExpected, "  vga  pvt                              VgaControl1 %08x\n", dwData);

    // Enter debug mode
    dwRegVal =  pSubdev->RegRd32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"));
//    dwRegVal &= ~(DRF_SHIFTMASK (LW_PDISP_VGA_DEBUG_CTL_MODE));
//    dwRegVal |= DRF_DEF (_PDISP,_VGA_DEBUG_CTL,_MODE,_ENABLE);
    dwRegVal &= ~rfMode.readmask;
    dwRegVal |= SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", "LW_PDISP_VGA_DEBUG_CTL_MODE_ENABLE") << (rfMode.startbit % 32);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"), dwRegVal);

    // Callwlate the terms
    dw1 = (dwOffset << (rfMethodOfs.startbit % 32)) & rfMethodOfs.readmask;
    dw2 = SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", "LW_PDISP_VGA_DEBUG_CTL_MODE_ENABLE") << (rfMode.startbit % 32);
    dw3 = SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD_TRIGGER") << (rfNewMethod.startbit % 32);
    dw4 = (SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE", "LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE_PRIVATE") << (rfMethodType.startbit % 32)) & rfMethodType.readmask;
    dwDbgCtl = dw1 | dw2 | dw3 | dw4;

//    dwDbgCtl =      DRF_NUM(_PDISP,_VGA_DEBUG_CTL,_METHOD_OFS, dwOffset) |
//                    DRF_DEF(_PDISP,_VGA_DEBUG_CTL,_MODE,_ENABLE) |
//                    DRF_DEF(_PDISP,_VGA_DEBUG_CTL,_NEW_METHOD,_TRIGGER) |
//                    DRF_NUM(_PDISP,_VGA_DEBUG_CTL,_METHOD_TYPE, LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE_PRIVATE);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_DAT"), dwData);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"), dwDbgCtl);

    // Poll for HW to accept the method
//    DispTest::PollRegValue (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"),
//                            DRF_DEF(_PDISP,_VGA_DEBUG_CTL,_NEW_METHOD,_DONE),
//                            DRF_SHIFTMASK(LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD),
//                            LegacyVGA::vga_timeout_ms());
    DispTest::PollRegValue (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"),
                            SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD_DONE") << (rfNewMethod.startbit % 32),
                            rfNewMethod.readmask,
                            LegacyVGA::vga_timeout_ms());

    // Exit "debug" mode
    dwRegVal =  pSubdev->RegRd32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"));
//    dwRegVal &= ~(DRF_SHIFTMASK (LW_PDISP_VGA_DEBUG_CTL_MODE));
//    dwRegVal |= DRF_DEF (_PDISP,_VGA_DEBUG_CTL,_MODE,_DISABLE);
    dwRegVal &= ~rfMode.readmask;
    dwRegVal |= SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", "LW_PDISP_VGA_DEBUG_CTL_MODE_DISABLE") << (rfMode.startbit % 32);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"), dwRegVal);

    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear
}

//
//      _MethodGenSignalMethodStop - Signal the end of a method to snoop
//
//      Entry:  None
//      Exit:   None
//
void _MethodGenSignalMethodStop (void)
{
    DWORD         dwData, dwRegVal, dwOffset, dwDbgCtl, dw1, dw2, dw3, dw4;
    REGFIELD      rfMethodOfs, rfMode, rfNewMethod, rfMethodType;
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

    dwOffset = 0x1F;                // LW_DispPvt_Core_VgaFbContractParms1__ADDR
    dwData = 0x00000000;

    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear

    // Gather the needed field information
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_METHOD_OFS", &rfMethodOfs);
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", &rfMode);
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD", &rfNewMethod);
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE", &rfMethodType);

    // Enter debug mode
    dwRegVal =  pSubdev->RegRd32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"));
//    dwRegVal &= ~(DRF_SHIFTMASK (LW_PDISP_VGA_DEBUG_CTL_MODE));
//    dwRegVal |= DRF_DEF (_PDISP,_VGA_DEBUG_CTL,_MODE,_ENABLE);
    dwRegVal &= ~rfMode.readmask;
    dwRegVal |= SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", "LW_PDISP_VGA_DEBUG_CTL_MODE_ENABLE") << (rfMode.startbit % 32);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"), dwRegVal);

    // Callwlate the terms
    dw1 = (dwOffset << (rfMethodOfs.startbit % 32)) & rfMethodOfs.readmask;
    dw2 = SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", "LW_PDISP_VGA_DEBUG_CTL_MODE_ENABLE") << (rfMode.startbit % 32);
    dw3 = SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD_TRIGGER") << (rfNewMethod.startbit % 32);
    dw4 = (SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE", "LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE_PRIVATE") << (rfMethodType.startbit % 32)) & rfMethodType.readmask;
    dwDbgCtl = dw1 | dw2 | dw3 | dw4;

//    dwDbgCtl =      DRF_NUM(_PDISP,_VGA_DEBUG_CTL,_METHOD_OFS, dwOffset) |
//                    DRF_DEF(_PDISP,_VGA_DEBUG_CTL,_MODE,_ENABLE) |
//                    DRF_DEF(_PDISP,_VGA_DEBUG_CTL,_NEW_METHOD,_TRIGGER) |
//                    DRF_NUM(_PDISP,_VGA_DEBUG_CTL,_METHOD_TYPE, LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE_PRIVATE);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_DAT"), dwData);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"), dwDbgCtl);

    // Poll for HW to accept the method
//    DispTest::PollRegValue (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"),
//                            DRF_DEF(_PDISP,_VGA_DEBUG_CTL,_NEW_METHOD,_DONE),
//                            DRF_SHIFTMASK(LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD),
//                            LegacyVGA::vga_timeout_ms());
    DispTest::PollRegValue (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"),
                            SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD_DONE") << (rfNewMethod.startbit % 32),
                            rfNewMethod.readmask,
                            LegacyVGA::vga_timeout_ms());

    // Exit "debug" mode
    dwRegVal =  pSubdev->RegRd32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"));
//    dwRegVal &= ~(DRF_SHIFTMASK (LW_PDISP_VGA_DEBUG_CTL_MODE));
//    dwRegVal |= DRF_DEF (_PDISP,_VGA_DEBUG_CTL,_MODE,_DISABLE);
    dwRegVal &= ~rfMode.readmask;
    dwRegVal |= SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", "LW_PDISP_VGA_DEBUG_CTL_MODE_DISABLE") << (rfMode.startbit % 32);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"), dwRegVal);

    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear
}

//
//      _MethodGenOutputAndPrint_rVGA - Output the rVGA register
//
//      Entry:  vmeth       VGAMETHOD data structure
//              bUpdate     Update flag
//      Exit:   None
//
void _MethodGenOutputAndPrint_rVGA (const VGAMETHOD& vmeth, BOOL bUpdate)
{
    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more
    _MethodGenOutputRegister_rVGA (vmeth, bUpdate);
    if (bUpdate)
        _MethodGenPrintRegister_rVGA (vmeth);
}

//
//      _MethodGenOutputAndPrint_lwGA - Output the lwGA register
//
//      Entry:  vmeth       VGAMETHOD data structure
//              bUpdate     Update flag
//      Exit:   None
//
void _MethodGenOutputAndPrint_lwGA(const VGAMETHOD& vmeth, BOOL bUpdate)
{
    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more
    _MethodGenOutputRegister_lwGA (vmeth, bUpdate);
    if (bUpdate)
        _MethodGenPrintRegister_lwGA (vmeth);
}

//
//      _MethodGenOutputRegister_rVGA - Output the rVGA register
//
//      Entry:  vmeth       VGAMETHOD data structure
//              bUpdate     Update flag
//      Exit:   None
//
void _MethodGenOutputRegister_rVGA(const VGAMETHOD& vmeth, BOOL bUpdate)
{
    static  WORD  CRTC = CRTC_CINDEX;         // Assume color
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

    if (vmeth.regname == "MISC")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (FALSE, MISC_OUTPUT, 0);
        IOByteWrite (MISC_OUTPUT, vmeth.val);
        CRTC = CRTC_MINDEX;                     // Record the CRTC address if it changes
        if (vmeth.val & 1) CRTC = CRTC_CINDEX;
    }
    if (vmeth.regname == "SR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (FALSE, SEQ_INDEX, vmeth.regindex);
        IOByteWrite (SEQ_INDEX, vmeth.regindex);
        IOByteWrite (SEQ_DATA, vmeth.val);
    }
    if (vmeth.regname == "AR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (FALSE, ATC_INDEX, vmeth.regindex);
        IOByteRead (CRTC + 6);
        IOByteWrite (ATC_INDEX, vmeth.regindex);
        IOByteWrite (ATC_INDEX, vmeth.val);
    }
    if (vmeth.regname == "GR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (FALSE, GDC_INDEX, vmeth.regindex);
        IOByteWrite (GDC_INDEX, vmeth.regindex);
        IOByteWrite (GDC_DATA, vmeth.val);
    }
    if (vmeth.regname == "CR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (FALSE, CRTC, vmeth.regindex);
        IOByteWrite (CRTC, vmeth.regindex);
        if (vmeth.regindex == 0x19)
        {
            IOByteWrite (CRTC + 1, (vmeth.val & 0x7f) | (head_index << 7));
        }
        else
        {
            IOByteWrite (CRTC + 1, vmeth.val);
        }
    }
    if (vmeth.regname == "DAC_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_CONTROL", dac_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_CONTROL", dac_index), (vmeth.val & 0xf0) | (0x1 << head_index));
    }
    if (vmeth.regname == "DAC_SET_POLARITY")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_POLARITY", dac_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_POLARITY", dac_index), vmeth.val);
    }
    if (vmeth.regname == "SOR_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_SOR_SET_CONTROL", head_index^0x1), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_SOR_SET_CONTROL", head_index^0x1), vmeth.val);
    }
    if (vmeth.regname == "PIOR_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_PIOR_SET_CONTROL",  (dac_index^0x3)-1), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_PIOR_SET_CONTROL", (dac_index^0x3)-1), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_PIXEL_CLOCK")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_DITHER_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_DITHER_CONTROL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_DITHER_CONTROL", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_CONTROL_OUTPUT_SCALER")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_SCALER", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_SCALER", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_PROCAMP")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PROCAMP", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PROCAMP", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HTOTAL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HTOTAL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HTOTAL", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HDISPLAY_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HDISPLAY_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HDISPLAY_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HVALID_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HVALID_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HSYNC_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HSYNC_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VTOTAL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VTOTAL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VTOTAL", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VDISPLAY_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VDISPLAY_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VDISPLAY_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VVALID_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VVALID_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VSYNC_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VSYNC_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_END", head_index), vmeth.val);
    }

    if (bUpdate)
    {
        _MethodGenSignalMethodStop ();
    }
}

//
//      _MethodGenPrintRegister_rVGA - Display the rVGA register data
//
//      Entry:  vmeth       VGAMETHOD data structure
//      Exit:   None
//
void _MethodGenPrintRegister_rVGA(const VGAMETHOD& vmeth)
{
    fprintf (hFileExpected, "rVGA %s %02x\n", vmeth.regname.c_str (), vmeth.regindex);

//  fprintf (hFileExpected, "  vga  pvt                              VgaControl1 %08x\n", dwData);

    if (vmeth.Dac_SetControl)
        fprintf (hFileExpected, "  vga                               Dac%d_SetControl 00000000\n", dac_index);
    if (vmeth.Dac_SetPolarity)
        fprintf (hFileExpected, "  vga                              Dac%d_SetPolarity 00000000\n", dac_index);
    if (vmeth.Sor_SetControl)
        fprintf (hFileExpected, "  vga                               Sor%d_SetControl 00000000\n", head_index^0x1);
    if (vmeth.Pior_SetControl)
        fprintf (hFileExpected, "  vga                              Pior%d_SetControl 00000000\n", (dac_index^0x3)-1);
    if (vmeth.Head_SetPixelClock)
        fprintf (hFileExpected, "  vga                           Head%d_SetPixelClock 00000000\n", head_index);
    if (vmeth.Head_SetControl)
        fprintf (hFileExpected, "  vga                              Head%d_SetControl 00000000\n", head_index);
    if (vmeth.Head_SetRasterSize)
        fprintf (hFileExpected, "  vga                           Head%d_SetRasterSize 00000000\n", head_index);
    if (vmeth.Head_SetRasterSyncEnd)
        fprintf (hFileExpected, "  vga                        Head%d_SetRasterSyncEnd 00000000\n", head_index);
    if (vmeth.Head_SetRasterBlankEnd)
        fprintf (hFileExpected, "  vga                       Head%d_SetRasterBlankEnd 00000000\n", head_index);
    if (vmeth.Head_SetRasterBlankStart)
        fprintf (hFileExpected, "  vga                     Head%d_SetRasterBlankStart 00000000\n", head_index);
    if (vmeth.Head_SetRasterVertBlank2)
        fprintf (hFileExpected, "  vga                     Head%d_SetRasterVertBlank2 00000000\n", head_index);
    if (vmeth.Head_SetDitherControl)
        fprintf (hFileExpected, "  vga                        Head%d_SetDitherControl 00000000\n", head_index);
    if (vmeth.Head_SetControlOutputScaler)
        fprintf (hFileExpected, "  vga                  Head%d_SetControlOutputScaler 00000000\n", head_index);
    if (vmeth.Head_SetProcamp)
        fprintf (hFileExpected, "  vga                              Head%d_SetProcamp 00000000\n", head_index);
    if (vmeth.Head_SetViewportSizeIn) {
        fprintf (hFileExpected, "  vga                       Head0_SetViewportSizeIn 00000000\n");
        fprintf (hFileExpected, "  vga                       Head1_SetViewportSizeIn 00000000\n");
    }
    if (vmeth.Head_SetViewportPointOutAdjust)
        fprintf (hFileExpected, "  vga               Head%d_SetViewportPointOutAdjust 00000000\n", head_index);
    if (vmeth.Head_SetViewportSizeOut)
        fprintf (hFileExpected, "  vga                      Head%d_SetViewportSizeOut 00000000\n", head_index);
    if (vmeth.VGAControl0)
        fprintf (hFileExpected, "  vga  pvt                              VgaControl0 00000000\n");
    if (vmeth.VGAControl1)
        fprintf (hFileExpected, "  vga  pvt                              VgaControl1 00000000\n");
    if (vmeth.VGAViewportEnd)
        fprintf (hFileExpected, "  vga  pvt                           VgaViewportEnd 00000000\n");
    if (vmeth.VGAActiveStart)
        fprintf (hFileExpected, "  vga  pvt                           VgaActiveStart 00000000\n");
    if (vmeth.VGAActiveEnd)
        fprintf (hFileExpected, "  vga  pvt                             VgaActiveEnd 00000000\n");
    if (vmeth.VGADisplaySize)
        fprintf (hFileExpected, "  vga  pvt                           VgaDisplaySize 00000000\n");
    if (vmeth.VGAFbContractParms0)
        fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms0 00000000\n");
    if (vmeth.VGAFbContractParms1)
        fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");
    if (vmeth.Head_SetLegacyCrcControl)
        fprintf (hFileExpected, "  vga                     Head%d_SetLegacyCrcControl 00000000\n", head_index);
//  if (vmeth.Core_Update)
//      fprintf (hFileExpected, "  vga                                        Update 00000000\n");

    // Signal end of method
    fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");
}

//
//      _MethodGenOutputRegister_lwGA - Output the lwGA register
//
//      Entry:  vmeth       VGAMETHOD data structure
//              bUpdate     Update flag
//      Exit:   None
//
void _MethodGenOutputRegister_lwGA(const VGAMETHOD& vmeth, BOOL bUpdate)
{
    static  WORD  CRTC = CRTC_CINDEX;
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

//  _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more
//  _MethodGenSetLegacyCrc (1, head_index);
//  _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more

    if (vmeth.regname == "MISC")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (FALSE, MISC_OUTPUT, 0);
        IOByteWrite (MISC_OUTPUT, vmeth.val);
        CRTC = CRTC_MINDEX;                     // Record the CRTC address if it changes
        if (vmeth.val & 1) CRTC = CRTC_CINDEX;
    }
    if (vmeth.regname == "SR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (FALSE, SEQ_INDEX, vmeth.regindex);
        IOByteWrite (SEQ_INDEX, vmeth.regindex);
        IOByteWrite (SEQ_DATA, vmeth.val);
    }
    if (vmeth.regname == "AR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (FALSE, ATC_INDEX, vmeth.regindex);
        IOByteRead (CRTC + 6);
        IOByteWrite (ATC_INDEX, vmeth.regindex);
        IOByteWrite (ATC_INDEX, vmeth.val);
    }
    if (vmeth.regname == "GR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (FALSE, GDC_INDEX, vmeth.regindex);
        IOByteWrite (GDC_INDEX, vmeth.regindex);
        IOByteWrite (GDC_DATA, vmeth.val);
    }
    if (vmeth.regname == "CR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (FALSE, CRTC, vmeth.regindex);
        IOByteWrite (CRTC, vmeth.regindex);
        if (vmeth.regindex == 0x19)
        {
            IOByteWrite (CRTC + 1, (vmeth.val&0x7f)|(head_index<<7));
        }
        else
        {
            IOByteWrite (CRTC + 1, vmeth.val);
        }
    }
    if (vmeth.regname == "DAC_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_CONTROL", dac_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_CONTROL", dac_index), (vmeth.val & 0xf0) | (0x1 << head_index));
    }
    if (vmeth.regname == "DAC_SET_POLARITY")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_POLARITY", dac_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_POLARITY", dac_index), vmeth.val);
    }
    if (vmeth.regname == "SOR_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_SOR_SET_CONTROL", head_index^0x1), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_SOR_SET_CONTROL", head_index^0x1), vmeth.val);
    }
    if (vmeth.regname == "PIOR_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_PIOR_SET_CONTROL", (dac_index^0x3)-1), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_PIOR_SET_CONTROL", (dac_index^0x3)-1), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_PIXEL_CLOCK")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_DITHER_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_DITHER_CONTROL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_DITHER_CONTROL", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_CONTROL_OUTPUT_SCALER")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_SCALER", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_SCALER", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_PROCAMP")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PROCAMP", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PROCAMP", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HTOTAL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HTOTAL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HTOTAL", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HDISPLAY_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HDISPLAY_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HDISPLAY_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HVALID_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HVALID_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HSYNC_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HSYNC_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VTOTAL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VTOTAL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VTOTAL", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VDISPLAY_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VDISPLAY_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VDISPLAY_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VVALID_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VVALID_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VSYNC_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VSYNC_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_END", head_index), vmeth.val);
    }

    if (bUpdate)
    {
        _MethodGenSignalMethodStop ();
    }
}

//
//      _MethodGenPrintRegister_lwGA - Display the lwGA register data
//
//      Entry:  vmeth       VGAMETHOD data structure
//      Exit:   None
//
void _MethodGenPrintRegister_lwGA(const VGAMETHOD& vmeth)
{
    fprintf (hFileExpected,     "lwGA %s %02x\n", vmeth.regname.c_str (), vmeth.regindex);
//  fprintf (hFileExpected,     "  vga                     Head%d_SetLegacyCrcControl 00000000\n", head_index);
//  fprintf (hFileExpected,     "  vga                     Head%d_SetLegacyCrcControl 00000001\n", head_index);

    if (vmeth.Dac_SetControl)
        fprintf (hFileExpected, "  vga                               Dac%d_SetControl 00000000\n", dac_index);
    if (vmeth.Dac_SetPolarity)
        fprintf (hFileExpected, "  vga                              Dac%d_SetPolarity 00000000\n", dac_index);
    if (vmeth.Sor_SetControl)
        fprintf (hFileExpected, "  vga                               Sor%d_SetControl 00000000\n", head_index^0x1);
    if (vmeth.Pior_SetControl)
        fprintf (hFileExpected, "  vga                              Pior%d_SetControl 00000000\n", (dac_index^0x3)-1);
    if (vmeth.Head_SetPixelClock)
        fprintf (hFileExpected, "  vga                           Head%d_SetPixelClock 00000000\n", head_index);
    if (vmeth.Head_SetControl)
        fprintf (hFileExpected, "  vga                              Head%d_SetControl 00000000\n", head_index);
    if (vmeth.Head_SetRasterSize)
        fprintf (hFileExpected, "  vga                           Head%d_SetRasterSize 00000000\n", head_index);
    if (vmeth.Head_SetRasterSyncEnd)
        fprintf (hFileExpected, "  vga                        Head%d_SetRasterSyncEnd 00000000\n", head_index);
    if (vmeth.Head_SetRasterBlankEnd)
        fprintf (hFileExpected, "  vga                       Head%d_SetRasterBlankEnd 00000000\n", head_index);
    if (vmeth.Head_SetRasterBlankStart)
        fprintf (hFileExpected, "  vga                     Head%d_SetRasterBlankStart 00000000\n", head_index);
    if (vmeth.Head_SetRasterVertBlank2)
        fprintf (hFileExpected, "  vga                     Head%d_SetRasterVertBlank2 00000000\n", head_index);
    if (vmeth.Head_SetDitherControl)
        fprintf (hFileExpected, "  vga                        Head%d_SetDitherControl 00000000\n", head_index);
    if (vmeth.Head_SetControlOutputScaler)
        fprintf (hFileExpected, "  vga                  Head%d_SetControlOutputScaler 00000000\n", head_index);
    if (vmeth.Head_SetProcamp)
        fprintf (hFileExpected, "  vga                              Head%d_SetProcamp 00000000\n", head_index);
    if (vmeth.Head_SetViewportSizeIn) {
        fprintf (hFileExpected, "  vga                       Head0_SetViewportSizeIn 00000000\n");
        fprintf (hFileExpected, "  vga                       Head1_SetViewportSizeIn 00000000\n");
    }
    if (vmeth.Head_SetViewportPointOutAdjust)
        fprintf (hFileExpected, "  vga               Head%d_SetViewportPointOutAdjust 00000000\n", head_index);
    if (vmeth.Head_SetViewportSizeOut)
        fprintf (hFileExpected, "  vga                      Head%d_SetViewportSizeOut 00000000\n", head_index);
    if (vmeth.VGAControl0)
        fprintf (hFileExpected, "  vga  pvt                              VgaControl0 00000000\n");
    if (vmeth.VGAControl1)
        fprintf (hFileExpected, "  vga  pvt                              VgaControl1 00000000\n");
    if (vmeth.VGAViewportEnd)
        fprintf (hFileExpected, "  vga  pvt                           VgaViewportEnd 00000000\n");
    if (vmeth.VGAActiveStart)
        fprintf (hFileExpected, "  vga  pvt                           VgaActiveStart 00000000\n");
    if (vmeth.VGAActiveEnd)
        fprintf (hFileExpected, "  vga  pvt                             VgaActiveEnd 00000000\n");
    if (vmeth.VGADisplaySize)
        fprintf (hFileExpected, "  vga  pvt                           VgaDisplaySize 00000000\n");
    if (vmeth.VGAFbContractParms0)
        fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms0 00000000\n");
    if (vmeth.VGAFbContractParms1)
        fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");
    if (vmeth.Head_SetLegacyCrcControl)
        fprintf (hFileExpected, "  vga                     Head%d_SetLegacyCrcControl 00000000\n", head_index);
//  if (vmeth.Core_Update)
//      fprintf (hFileExpected, "  vga                                        Update 00000000\n");

    // Signal end of method
    fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");
}

//
//      _MethodGenSignalMethodStart_gf11x - Signal the beginning of a method to snoop
//
//      Entry:  bPriv       Priv vs. I/O flag (TRUE = Priv reg)
//              dwAddr      Register address
//              nIndex      Index (if indexed)
//      Exit:   None
//
void _MethodGenSignalMethodStart_gf11x (BOOL bPriv, DWORD dwAddr, int nIndex)
{
    DWORD         dwData, dwRegVal, dwOffset, dwDbgCtl, dw1, dw2, dw3, dw4;
    REGFIELD      rfMethodOfs, rfMode, rfNewMethod, rfMethodType;
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

    dwOffset = 0x17;                // LW_DispPvt_Core_VgaControl1__ADDR
    if (bPriv)
        dwData = dwAddr;
    else
        dwData = (dwAddr << 16) | (nIndex & 0xFFFF) | 0x80000000;

    // Gather the needed field information
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_METHOD_OFS", &rfMethodOfs);
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", &rfMode);
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD", &rfNewMethod);
    SimGetRegField ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE", &rfMethodType);

    fprintf (hFileExpected, "  vga  pvt                              VgaControl1 %08x\n", dwData);

    // Enter debug mode
    dwRegVal =  pSubdev->RegRd32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"));
//    dwRegVal &= ~(DRF_SHIFTMASK (LW_PDISP_VGA_DEBUG_CTL_MODE));
//    dwRegVal |= DRF_DEF (_PDISP,_VGA_DEBUG_CTL,_MODE,_ENABLE);
    dwRegVal &= ~rfMode.readmask;
    dwRegVal |= SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", "LW_PDISP_VGA_DEBUG_CTL_MODE_ENABLE") << (rfMode.startbit % 32);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"), dwRegVal);

    // Callwlate the terms
    dw1 = (dwOffset << (rfMethodOfs.startbit % 32)) & rfMethodOfs.readmask;
    dw2 = SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", "LW_PDISP_VGA_DEBUG_CTL_MODE_ENABLE") << (rfMode.startbit % 32);
    dw3 = SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD_TRIGGER") << (rfNewMethod.startbit % 32);
    dw4 = (SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE", "LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE_PRIVATE") << (rfMethodType.startbit % 32)) & rfMethodType.readmask;
    dwDbgCtl = dw1 | dw2 | dw3 | dw4;

//    dwDbgCtl =      DRF_NUM(_PDISP,_VGA_DEBUG_CTL,_METHOD_OFS, dwOffset) |
//                    DRF_DEF(_PDISP,_VGA_DEBUG_CTL,_MODE,_ENABLE) |
//                    DRF_DEF(_PDISP,_VGA_DEBUG_CTL,_NEW_METHOD,_TRIGGER) |
//                    DRF_NUM(_PDISP,_VGA_DEBUG_CTL,_METHOD_TYPE, LW_PDISP_VGA_DEBUG_CTL_METHOD_TYPE_PRIVATE);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_DAT"), dwData);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"), dwDbgCtl);

    // Poll for HW to accept the method
//    DispTest::PollRegValue (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"),
//                            DRF_DEF(_PDISP,_VGA_DEBUG_CTL,_NEW_METHOD,_DONE),
//                            DRF_SHIFTMASK(LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD),
//                            LegacyVGA::vga_timeout ());
    DispTest::PollRegValue (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"),
                            SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD", "LW_PDISP_VGA_DEBUG_CTL_NEW_METHOD_DONE") << (rfNewMethod.startbit % 32),
                            rfNewMethod.readmask,
                            LegacyVGA::vga_timeout_ms ());

    // Exit "debug" mode
    dwRegVal =  pSubdev->RegRd32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"));
//    dwRegVal &= ~(DRF_SHIFTMASK (LW_PDISP_VGA_DEBUG_CTL_MODE));
//    dwRegVal |= DRF_DEF (_PDISP,_VGA_DEBUG_CTL,_MODE,_DISABLE);
    dwRegVal &= ~rfMode.readmask;
    dwRegVal |= SimGetRegValue ("LW_PDISP_VGA_DEBUG_CTL", "LW_PDISP_VGA_DEBUG_CTL_MODE", "LW_PDISP_VGA_DEBUG_CTL_MODE_DISABLE") << (rfMode.startbit % 32);
    pSubdev->RegWr32 (SimGetRegAddress ("LW_PDISP_VGA_DEBUG_CTL"), dwRegVal);

    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear
}

//
//      _MethodGenOutputAndPrint_gf11x_rVGA - Output the rVGA register
//
//      Entry:  vmeth       VGAMETHOD_GF11x data structure
//              bUpdate     Update flag
//      Exit:   None
//
void _MethodGenOutputAndPrint_gf11x_rVGA(const VGAMETHOD_GF11x& vmeth, BOOL bUpdate)
{
     _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more
     _MethodGenOutputRegister_gf11x_rVGA (vmeth, bUpdate);
     if (bUpdate)
         _MethodGenPrintRegister_gf11x_rVGA (vmeth);
}

//
//      _MethodGenOutputAndPrint_gf11x_lwGA - Output the lwGA register
//
//      Entry:  vmeth       VGAMETHOD_GF11x data structure
//              bUpdate     Update flag
//      Exit:   None
//
void _MethodGenOutputAndPrint_gf11x_lwGA(const VGAMETHOD_GF11x& vmeth, BOOL bUpdate)
{
    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more
    _MethodGenOutputRegister_gf11x_lwGA (vmeth, bUpdate);
    if (bUpdate)
        _MethodGenPrintRegister_gf11x_lwGA (vmeth);
}

//
//      _MethodGenOutputRegister_gf11x_rVGA - Output the rVGA register
//
//      Entry:  vmeth       VGAMETHOD_GF11x data structure
//              bUpdate     Update flag
//      Exit:   None
//
void _MethodGenOutputRegister_gf11x_rVGA(const VGAMETHOD_GF11x& vmeth, BOOL bUpdate)
{
    static  WORD  CRTC = CRTC_CINDEX;         // Assume color
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

    if (vmeth.regname == "MISC")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (FALSE, MISC_OUTPUT, 0);
        IOByteWrite (MISC_OUTPUT, vmeth.val);
        CRTC = CRTC_MINDEX;                     // Record the CRTC address if it changes
        if (vmeth.val & 1) CRTC = CRTC_CINDEX;
    }
    if (vmeth.regname == "SR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (FALSE, SEQ_INDEX, vmeth.regindex);
        IOByteWrite (SEQ_INDEX, vmeth.regindex);
        IOByteWrite (SEQ_DATA, vmeth.val);
    }
    if (vmeth.regname == "AR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (FALSE, ATC_INDEX, vmeth.regindex);
        IOByteRead (CRTC + 6);
        IOByteWrite (ATC_INDEX, vmeth.regindex);
        IOByteWrite (ATC_INDEX, vmeth.val);
    }
    if (vmeth.regname == "GR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (FALSE, GDC_INDEX, vmeth.regindex);
        IOByteWrite (GDC_INDEX, vmeth.regindex);
        IOByteWrite (GDC_DATA, vmeth.val);
    }
    if (vmeth.regname == "CR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (FALSE, CRTC, vmeth.regindex);
        IOByteWrite (CRTC, vmeth.regindex);
        if (vmeth.regindex == 0x19)
        {
            IOByteWrite (CRTC + 1, (vmeth.val & 0x1f) | (head_index << 5));
        }
        else
        {
            IOByteWrite (CRTC + 1, vmeth.val);
        }
    }
    if (vmeth.regname == "DAC_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_CONTROL", dac_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_CONTROL", dac_index), (vmeth.val & 0xf0) | (0x1 << head_index));
    }
    if (vmeth.regname == "SOR_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_SOR_SET_CONTROL", dac_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_SOR_SET_CONTROL", dac_index), vmeth.val);
    }
    if (vmeth.regname == "PIOR_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_PIOR_SET_CONTROL",  dac_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_PIOR_SET_CONTROL", dac_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_CONTROL_OUTPUT_RESOURCE")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_RESOURCE",  head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_RESOURCE", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_PIXEL_CLOCK_FREQUENCY")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK_FREQUENCY", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK_FREQUENCY", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_PIXEL_CLOCK_CONFIGURATION")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK_CONFIGURATION", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK_CONFIGURATION", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_DITHER_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_DITHER_CONTROL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_DITHER_CONTROL", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_CONTROL_OUTPUT_SCALER")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_SCALER", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_SCALER", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_PROCAMP")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PROCAMP", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PROCAMP", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HTOTAL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HTOTAL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HTOTAL", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HDISPLAY_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HDISPLAY_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HDISPLAY_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HVALID_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HVALID_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HSYNC_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HSYNC_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VTOTAL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VTOTAL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VTOTAL", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VDISPLAY_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VDISPLAY_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VDISPLAY_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VVALID_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VVALID_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VSYNC_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VSYNC_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_END", head_index), vmeth.val);
    }

    if (bUpdate)
    {
        _MethodGenSignalMethodStop ();
    }
}

//
//      _MethodGenPrintRegister_gf11x_rVGA - Display the rVGA register data
//
//      Entry:  vmeth       VGAMETHOD_GF11x data structure
//      Exit:   None
//
void _MethodGenPrintRegister_gf11x_rVGA(const VGAMETHOD_GF11x& vmeth)
{
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

    DWORD data = pSubdev->RegRd32 (SimGetRegAddress ("LW_PDISP_DSI_SYS_CAP"));
    UINT32 head2_exists = ((data & 0x4) == 0x4);
    UINT32 head3_exists = ((data & 0x8) == 0x8);

    fprintf (hFileExpected, "rVGA %s %02x\n", vmeth.regname.c_str (), vmeth.regindex);

//  fprintf (hFileExpected, "  vga  pvt                              VgaControl1 %08x\n", dwData);

    if (vmeth.Dac_SetControl)
        fprintf (hFileExpected, "  vga                               Dac%d_SetControl 00000000\n", dac_index);
    if (vmeth.Sor_SetControl)
        fprintf (hFileExpected, "  vga                               Sor%d_SetControl 00000000\n", dac_index);
    if (vmeth.Pior_SetControl)
        fprintf (hFileExpected, "  vga                              Pior%d_SetControl 00000000\n", dac_index);
    if (vmeth.Head_SetControlOutputResource)
        fprintf (hFileExpected, "  vga                Head%d_SetControlOutputResource 00000000\n", head_index);
    if (vmeth.Head_SetPixelClockConfiguration)
        fprintf (hFileExpected, "  vga              Head%d_SetPixelClockConfiguration 00000000\n", head_index);
    if (vmeth.Head_SetPixelClockFrequency)
        fprintf (hFileExpected, "  vga                  Head%d_SetPixelClockFrequency 00000000\n", head_index);
    if (vmeth.Head_SetControl)
        fprintf (hFileExpected, "  vga                              Head%d_SetControl 00000000\n", head_index);
    if (vmeth.Head_SetRasterSize)
        fprintf (hFileExpected, "  vga                           Head%d_SetRasterSize 00000000\n", head_index);
    if (vmeth.Head_SetRasterSyncEnd)
        fprintf (hFileExpected, "  vga                        Head%d_SetRasterSyncEnd 00000000\n", head_index);
    if (vmeth.Head_SetRasterBlankEnd)
        fprintf (hFileExpected, "  vga                       Head%d_SetRasterBlankEnd 00000000\n", head_index);
    if (vmeth.Head_SetRasterBlankStart)
        fprintf (hFileExpected, "  vga                     Head%d_SetRasterBlankStart 00000000\n", head_index);
    if (vmeth.Head_SetRasterVertBlank2)
        fprintf (hFileExpected, "  vga                     Head%d_SetRasterVertBlank2 00000000\n", head_index);
    if (vmeth.Head_SetDitherControl)
        fprintf (hFileExpected, "  vga                        Head%d_SetDitherControl 00000000\n", head_index);
    if (vmeth.Head_SetControlOutputScaler)
        fprintf (hFileExpected, "  vga                  Head%d_SetControlOutputScaler 00000000\n", head_index);
    if (vmeth.Head_SetProcamp)
        fprintf (hFileExpected, "  vga                              Head%d_SetProcamp 00000000\n", head_index);
    if (vmeth.Head_SetViewportSizeIn) {
        fprintf (hFileExpected, "  vga                       Head0_SetViewportSizeIn 00000000\n");
        fprintf (hFileExpected, "  vga                       Head1_SetViewportSizeIn 00000000\n");
        if(head2_exists == 1) {
          fprintf (hFileExpected, "  vga                       Head2_SetViewportSizeIn 00000000\n");
        }
        if(head3_exists == 1) {
          fprintf (hFileExpected, "  vga                       Head3_SetViewportSizeIn 00000000\n");
        }
    }
    if (vmeth.Head_SetViewportPointOutAdjust)
        fprintf (hFileExpected, "  vga               Head%d_SetViewportPointOutAdjust 00000000\n", head_index);
    if (vmeth.Head_SetViewportSizeOut)
        fprintf (hFileExpected, "  vga                      Head%d_SetViewportSizeOut 00000000\n", head_index);
    if (vmeth.VGAControl0)
        fprintf (hFileExpected, "  vga  pvt                              VgaControl0 00000000\n");
    if (vmeth.VGAControl1)
        fprintf (hFileExpected, "  vga  pvt                              VgaControl1 00000000\n");
    if (vmeth.VGAViewportEnd)
        fprintf (hFileExpected, "  vga  pvt                           VgaViewportEnd 00000000\n");
    if (vmeth.VGAActiveStart)
        fprintf (hFileExpected, "  vga  pvt                           VgaActiveStart 00000000\n");
    if (vmeth.VGAActiveEnd)
        fprintf (hFileExpected, "  vga  pvt                             VgaActiveEnd 00000000\n");
    if (vmeth.VGADisplaySize)
        fprintf (hFileExpected, "  vga  pvt                           VgaDisplaySize 00000000\n");
    if (vmeth.VGAFbContractParms0)
        fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms0 00000000\n");
    if (vmeth.VGAFbContractParms1)
        fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");
    if (vmeth.Head_SetLegacyCrcControl)
        fprintf (hFileExpected, "  vga                     Head%d_SetLegacyCrcControl 00000000\n", head_index);
//  if (vmeth.Core_Update)
//      fprintf (hFileExpected, "  vga                                        Update 00000000\n");

    // Signal end of method
    fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");
}

//
//      _MethodGenOutputRegister_gf11x_lwGA - Output the lwGA register
//
//      Entry:  vmeth       VGAMETHOD_GF11x data structure
//              bUpdate     Update flag
//      Exit:   None
//
void _MethodGenOutputRegister_gf11x_lwGA(const VGAMETHOD_GF11x& vmeth, BOOL bUpdate)
{
    static  WORD  CRTC = CRTC_CINDEX;
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

//  _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more
//  _MethodGenSetLegacyCrc (1, head_index);
//  _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more

    if (vmeth.regname == "MISC")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (FALSE, MISC_OUTPUT, 0);
        IOByteWrite (MISC_OUTPUT, vmeth.val);
        CRTC = CRTC_MINDEX;                     // Record the CRTC address if it changes
        if (vmeth.val & 1) CRTC = CRTC_CINDEX;
    }
    if (vmeth.regname == "SR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (FALSE, SEQ_INDEX, vmeth.regindex);
        IOByteWrite (SEQ_INDEX, vmeth.regindex);
        IOByteWrite (SEQ_DATA, vmeth.val);
    }
    if (vmeth.regname == "AR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (FALSE, ATC_INDEX, vmeth.regindex);
        IOByteRead (CRTC + 6);
        IOByteWrite (ATC_INDEX, vmeth.regindex);
        IOByteWrite (ATC_INDEX, vmeth.val);
    }
    if (vmeth.regname == "GR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (FALSE, GDC_INDEX, vmeth.regindex);
        IOByteWrite (GDC_INDEX, vmeth.regindex);
        IOByteWrite (GDC_DATA, vmeth.val);
    }
    if (vmeth.regname == "CR")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (FALSE, CRTC, vmeth.regindex);
        IOByteWrite (CRTC, vmeth.regindex);
        if (vmeth.regindex == 0x19)
        {
            IOByteWrite (CRTC + 1, (vmeth.val&0x1f)|(head_index<<5));
        }
        else
        {
            IOByteWrite (CRTC + 1, vmeth.val);
        }
    }
    if (vmeth.regname == "DAC_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_CONTROL", dac_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_DAC_SET_CONTROL", dac_index), (vmeth.val & 0xf0) | (0x1 << head_index));
    }
    if (vmeth.regname == "SOR_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_SOR_SET_CONTROL", dac_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_SOR_SET_CONTROL", dac_index), vmeth.val);
    }
    if (vmeth.regname == "PIOR_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_PIOR_SET_CONTROL", dac_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_PIOR_SET_CONTROL", dac_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_CONTROL_OUTPUT_RESOURCE")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_RESOURCE",  head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_RESOURCE", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_PIXEL_CLOCK_FREQUENCY")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK_FREQUENCY", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK_FREQUENCY", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_PIXEL_CLOCK_CONFIGURATION")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK_CONFIGURATION", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK_CONFIGURATION", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_DITHER_CONTROL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_DITHER_CONTROL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_DITHER_CONTROL", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_CONTROL_OUTPUT_SCALER")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_SCALER", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_CONTROL_OUTPUT_SCALER", head_index), vmeth.val);
    }
    if (vmeth.regname == "HEAD_SET_PROCAMP")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PROCAMP", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_HEAD_SET_PROCAMP", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HTOTAL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HTOTAL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HTOTAL", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HDISPLAY_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HDISPLAY_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HDISPLAY_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HVALID_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HVALID_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HVALID_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HSYNC_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_HSYNC_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_HSYNC_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VTOTAL")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VTOTAL", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VTOTAL", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VDISPLAY_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VDISPLAY_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VDISPLAY_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VVALID_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VVALID_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VVALID_END", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VSYNC_START")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_START", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_START", head_index), vmeth.val);
    }
    if (vmeth.regname == "BE_VSYNC_END")
    {
        if (bUpdate)
            _MethodGenSignalMethodStart_gf11x (TRUE, SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_END", head_index), 0);
        pSubdev->RegWr32 (SimGetRegHeadAddress ("LW_PDISP_VGA_BE_VSYNC_END", head_index), vmeth.val);
    }

    if (bUpdate)
    {
        _MethodGenSignalMethodStop ();
    }
}

//
//      _MethodGenPrintRegister_gf11x_lwGA - Display the lwGA register data
//
//      Entry:  vmeth       VGAMETHOD_GF11x data structure
//      Exit:   None
//
void _MethodGenPrintRegister_gf11x_lwGA(const VGAMETHOD_GF11x& vmeth)
{
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

    DWORD data = pSubdev->RegRd32 (SimGetRegAddress ("LW_PDISP_DSI_SYS_CAP"));
    UINT32 head2_exists = ((data & 0x4) == 0x4);
    UINT32 head3_exists = ((data & 0x8) == 0x8);

    fprintf (hFileExpected,     "lwGA %s %02x\n", vmeth.regname.c_str (), vmeth.regindex);
//  fprintf (hFileExpected,     "  vga                     Head%d_SetLegacyCrcControl 00000000\n", head_index);
//  fprintf (hFileExpected,     "  vga                     Head%d_SetLegacyCrcControl 00000001\n", head_index);

    if (vmeth.Dac_SetControl)
        fprintf (hFileExpected, "  vga                               Dac%d_SetControl 00000000\n", dac_index);
    if (vmeth.Sor_SetControl)
        fprintf (hFileExpected, "  vga                               Sor%d_SetControl 00000000\n", dac_index);
    if (vmeth.Pior_SetControl)
        fprintf (hFileExpected, "  vga                              Pior%d_SetControl 00000000\n", dac_index);
    if (vmeth.Head_SetControlOutputResource)
        fprintf (hFileExpected, "  vga                Head%d_SetControlOutputResource 00000000\n", head_index);
    if (vmeth.Head_SetPixelClockConfiguration)
        fprintf (hFileExpected, "  vga              Head%d_SetPixelClockConfiguration 00000000\n", head_index);
    if (vmeth.Head_SetPixelClockFrequency)
        fprintf (hFileExpected, "  vga                  Head%d_SetPixelClockFrequency 00000000\n", head_index);
    if (vmeth.Head_SetControl)
        fprintf (hFileExpected, "  vga                              Head%d_SetControl 00000000\n", head_index);
    if (vmeth.Head_SetRasterSize)
        fprintf (hFileExpected, "  vga                           Head%d_SetRasterSize 00000000\n", head_index);
    if (vmeth.Head_SetRasterSyncEnd)
        fprintf (hFileExpected, "  vga                        Head%d_SetRasterSyncEnd 00000000\n", head_index);
    if (vmeth.Head_SetRasterBlankEnd)
        fprintf (hFileExpected, "  vga                       Head%d_SetRasterBlankEnd 00000000\n", head_index);
    if (vmeth.Head_SetRasterBlankStart)
        fprintf (hFileExpected, "  vga                     Head%d_SetRasterBlankStart 00000000\n", head_index);
    if (vmeth.Head_SetRasterVertBlank2)
        fprintf (hFileExpected, "  vga                     Head%d_SetRasterVertBlank2 00000000\n", head_index);
    if (vmeth.Head_SetDitherControl)
        fprintf (hFileExpected, "  vga                        Head%d_SetDitherControl 00000000\n", head_index);
    if (vmeth.Head_SetControlOutputScaler)
        fprintf (hFileExpected, "  vga                  Head%d_SetControlOutputScaler 00000000\n", head_index);
    if (vmeth.Head_SetProcamp)
        fprintf (hFileExpected, "  vga                              Head%d_SetProcamp 00000000\n", head_index);
    if (vmeth.Head_SetViewportSizeIn) {
        fprintf (hFileExpected, "  vga                       Head0_SetViewportSizeIn 00000000\n");
        fprintf (hFileExpected, "  vga                       Head1_SetViewportSizeIn 00000000\n");
        if(head2_exists == 1) {
          fprintf (hFileExpected, "  vga                       Head2_SetViewportSizeIn 00000000\n");
        }
        if(head3_exists == 1) {
          fprintf (hFileExpected, "  vga                       Head3_SetViewportSizeIn 00000000\n");
        }
    }
    if (vmeth.Head_SetViewportPointOutAdjust)
        fprintf (hFileExpected, "  vga               Head%d_SetViewportPointOutAdjust 00000000\n", head_index);
    if (vmeth.Head_SetViewportSizeOut)
        fprintf (hFileExpected, "  vga                      Head%d_SetViewportSizeOut 00000000\n", head_index);
    if (vmeth.VGAControl0)
        fprintf (hFileExpected, "  vga  pvt                              VgaControl0 00000000\n");
    if (vmeth.VGAControl1)
        fprintf (hFileExpected, "  vga  pvt                              VgaControl1 00000000\n");
    if (vmeth.VGAViewportEnd)
        fprintf (hFileExpected, "  vga  pvt                           VgaViewportEnd 00000000\n");
    if (vmeth.VGAActiveStart)
        fprintf (hFileExpected, "  vga  pvt                           VgaActiveStart 00000000\n");
    if (vmeth.VGAActiveEnd)
        fprintf (hFileExpected, "  vga  pvt                             VgaActiveEnd 00000000\n");
    if (vmeth.VGADisplaySize)
        fprintf (hFileExpected, "  vga  pvt                           VgaDisplaySize 00000000\n");
    if (vmeth.VGAFbContractParms0)
        fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms0 00000000\n");
    if (vmeth.VGAFbContractParms1)
        fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");
    if (vmeth.Head_SetLegacyCrcControl)
        fprintf (hFileExpected, "  vga                     Head%d_SetLegacyCrcControl 00000000\n", head_index);
//  if (vmeth.Core_Update)
//      fprintf (hFileExpected, "  vga                                        Update 00000000\n");

    // Signal end of method
    fprintf (hFileExpected, "  vga  pvt                      VgaFbContractParms1 00000000\n");
}

//
//      _MethodGenSetLegacyCrc - Writes to CRA9 to cause a SetLegacyCrc method to be sent to the DSI
//
//      Entry:  en          Enable flag (TRUE = Enable, FALSE = Disable)
//              head        Head index
//      Exit:   <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
//      Notes:  1) Assume CRTC is in 3Dx addressing mode (color)
//              2) Assume extended registers are unlocked
//
#define LW_CIO_CRE_LGY_CRC_H0_COMPUTE                           0:0 /* RWIVF */
#define LW_CIO_CRE_LGY_CRC_H0_COMPUTE_ENABLE             0x00000001 /* RW--V */
#define LW_CIO_CRE_LGY_CRC_H0_COMPUTE_DISABLE            0x00000000 /* RW--V */

#define LW_CIO_CRE_LGY_CRC_H1_COMPUTE                           1:1 /* RWIVF */
#define LW_CIO_CRE_LGY_CRC_H1_COMPUTE_ENABLE             0x00000001 /* RW--V */
#define LW_CIO_CRE_LGY_CRC_H1_COMPUTE_DISABLE            0x00000000 /* RW--V */
//
BOOL _MethodGenSetLegacyCrc (BOOL en, int head)
{
    BYTE    crA9;
    int     tmp, head0_compute, head1_compute;

    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more

    IOByteWrite (CRTC_CINDEX, 0xA9);                // LW_CIO_CRE_LGY_CRC__INDEX
    crA9 = IOByteRead (CRTC_CDATA);

    tmp = DRF_NUM(_CIO, _CRE_LGY_CRC, _H0_COMPUTE, LW_CIO_CRE_LGY_CRC_H0_COMPUTE_DISABLE ) |
            DRF_NUM(_CIO, _CRE_LGY_CRC, _H1_COMPUTE, LW_CIO_CRE_LGY_CRC_H1_COMPUTE_DISABLE );
    IOByteWrite (CRTC_CDATA, tmp);

    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more

    head0_compute = DRF_VAL(_CIO, _CRE_LGY_CRC, _H0_COMPUTE, crA9);
    head1_compute = DRF_VAL(_CIO, _CRE_LGY_CRC, _H1_COMPUTE, crA9);
    if (head == 0 )
        head0_compute = en ? LW_CIO_CRE_LGY_CRC_H0_COMPUTE_ENABLE : LW_CIO_CRE_LGY_CRC_H0_COMPUTE_DISABLE;
    else if (head == 1 )
        head1_compute = en ? LW_CIO_CRE_LGY_CRC_H1_COMPUTE_ENABLE : LW_CIO_CRE_LGY_CRC_H1_COMPUTE_DISABLE;
    else
    {
        SimComment ("[_MethodGenSetLegacyCrc] - Unrecognized head.");
        return (FALSE);
    }

    crA9 = DRF_NUM(_CIO, _CRE_LGY_CRC, _H0_COMPUTE, head0_compute) |
            DRF_NUM(_CIO, _CRE_LGY_CRC, _H1_COMPUTE, head1_compute);
    IOByteWrite (CRTC_CDATA, crA9);

    _MethodGenWaitNoMethods ();                 // Wait for the method bus to clear before sending any more

    return (TRUE);
}

//
//      _MethodGenWaitNoMethods - Wait until there are no methods pending
//
//      Entry:  None
//      Exit:   <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
//      Assume color CRTC
//
BOOL _MethodGenWaitNoMethods (void)
{
    BYTE    crIdx;

    fprintf (hFileExpected, "DEBUG - Waiting for method bus to clear.\n");
    crIdx = IOByteRead (CRTC_CINDEX);                   // Save original index
    IOByteWrite (CRTC_CINDEX, 0xE8);
    while ((IOByteRead (CRTC_CDATA) & 0x40) != 0);
    IOByteWrite (CRTC_CINDEX, crIdx);                   // Restore index
    return (TRUE);
}

//
//      _MethodGenProcessActual - Process the list of actual methods produced by this test
//
//      Entry:  pszFileIn       Filename of the input file
//              pszFileOut      Filename of the output file
//      Exit:   <BOOL>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL _MethodGenProcessActual (char *pszFileIn, char *pszFileOut)
{
    FILE    *hFileIn;
    FILE    *hFileOut;
    char    szBuffer[256];
    BOOL    bFirst, bSecond, bPrintIt;
    int     nLines;

    // Initialize local variables
    Printf (Tee::PriNormal, "Creating ACTUAL file: Processing %s to produce %s\n", pszFileIn, pszFileOut);
    hFileIn = hFileOut = NULL;
    bFirst = bSecond = bPrintIt = false;
    nLines = 0;

    // Pseudo-flush the input file which is still open on the system
    hFileIn = fopen (pszFileIn, "r");
    fseek (hFileIn, 0, SEEK_END);
    fclose (hFileIn);

    // Open the input file for reading
    hFileIn = fopen (pszFileIn, "r");
    if (hFileIn == NULL)
    {
        Printf (Tee::PriNormal, "Could not open generated method file: %s\n", pszFileIn);
        return (FALSE);
    }

    // Open the output file for writing
    hFileOut = fopen (pszFileOut, "w");
    if (hFileOut == NULL)
    {
        fclose (hFileIn);
        Printf (Tee::PriNormal, "Could not create actual method file: %s\n", pszFileOut);
        return (FALSE);
    }

    // Process each line of the method file and create the actual file
    bFirst = TRUE;
    bSecond = TRUE;
    while (fgets (szBuffer, sizeof (szBuffer), hFileIn))
    {
        // Strip out everything up to and including the first start and stop method flags
        // Need to skip the first start method flag since it gets generated on the VBIOS grab
        // Therefore, skip everything up to and including the second VgaControl1 method
        if (bFirst)
        {
            if (strstr (szBuffer, szMethodStart) != NULL)
            {
                bFirst = FALSE;
            }
        }
        else if (bSecond)
        {
            if (strstr (szBuffer, szMethodStart) != NULL)
            {
                bSecond = FALSE;
                bPrintIt = TRUE;
            }
        }

        if (bPrintIt)
        {
            // If in the "print zone" of the file, determine if the line is actually
            // useable or if it's just diagnostic data (hint: preceded by "vga" is good).
            if (_MethodGelwalidData (szBuffer))
            {
                nLines++;
                fprintf (hFileOut, "%s", szBuffer);
            }
        }
    }
    Printf (Tee::PriNormal, "ACTUAL file created with %d lines.\n", nLines);

    fclose (hFileOut);
    fclose (hFileIn);
    return (nLines > 0);
}

//
//      _MethodGelwalidData - Determine if the line contains useable data
//
//      Entry:  pszBuffer   Pointer to the line in the "core_method.out" file
//      Exit:   <BOOL>      Useable flag (TRUE = Useable, FALSE = Not)
//
BOOL _MethodGelwalidData (char *pszBuffer)
{
    char            *pszStart;
    size_t          n, nLength;
    static char     szTarget[] = "vga ";

    // Validate data
    if (pszBuffer == NULL)
        return (FALSE);
    nLength = strlen (pszBuffer);
    if (nLength < strlen (szTarget))
        return (FALSE);

    // Separate out the first word on the line
    pszStart = pszBuffer;
    n = 0;
    while (isspace (*pszStart))
    {
        if (n++ > nLength)
            return (FALSE);
        pszStart++;
    }

    // Determine if the line is valid by comparing the first string with the method comment "vga"
    if (strncmp (pszStart, szTarget, strlen (szTarget)) == 0)
        return (TRUE);

    return (FALSE);
}

//
//      _MethodGelwerifyFiles - Compare the expected file with the actual generated methods
//
//      Entry:  pszFileLog      Filename of the expected methods file
//              pszFileActual   Filename of the actual methods file
//      Exit:   <BOOL>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL _MethodGelwerifyFiles (char *pszFileExpected, char *pszFileActual)
{
    FILE            *hFileExpected;
    FILE            *hFileActual;
    int             nLinesExpected, nLinesActual, nListEntriesExpected, nListEntriesActual;
    PMETHODENTRY    pMethodListExpected;
    PMETHODENTRY    pMethodListActual;
    BOOL            bSuccess;

    // Initialize local variables
    printf ("Verifying expected (%s) matches actual (%s)\n", pszFileExpected, pszFileActual);
    bSuccess = TRUE;
    nLinesExpected = nLinesActual = 0;
    nListEntriesExpected = nListEntriesActual = 0;
    hFileExpected = hFileActual = NULL;
    pMethodListExpected = pMethodListActual = NULL;

    // Open the expected file for reading
    hFileExpected = fopen (pszFileExpected, "r");
    if (hFileExpected == NULL)
    {
        printf ("ERROR: Could not open generated log file: %s\n", pszFileExpected);
        return (FALSE);
    }

    // Open the actual file for reading
    hFileActual = fopen (pszFileActual, "r");
    if (hFileActual == NULL)
    {
        fclose (hFileExpected);
        printf ("ERROR: Could not open actual methods file: %s\n", pszFileActual);
        return (FALSE);
    }

    do
    {
        // Gather a list of EXPECTED methods
        if (_MethodGenGatherStrings (hFileExpected, &nLinesExpected, &pMethodListExpected, &nListEntriesExpected))
        {
            // Gather a list of ACTUAL methods
            if (_MethodGenGatherStrings (hFileActual, &nLinesActual, &pMethodListActual, &nListEntriesActual))
            {
                // Compare the two
                if (!_MethodGenCompareLists (pMethodListExpected, nListEntriesExpected, pMethodListActual, nListEntriesActual))
                {
                    printf ("ERROR: Method entries do not match!\n   File %s, Above line %d\n   File %s, Above line %d\n", pszFileExpected, nLinesExpected, pszFileActual, nLinesActual);
                    bSuccess = FALSE;
                }

                // Free the ACTUAL list
                _MethodGenFreeList (pMethodListActual, nListEntriesActual);
            }
            else
                bSuccess = FALSE;

            // Free the EXPECTED list
            _MethodGenFreeList (pMethodListExpected, nListEntriesExpected);
        }
        else
        {
            if (!feof (hFileExpected))
            {
                printf ("ERROR: Reading the file: %s.\n", pszFileExpected);
                bSuccess = FALSE;
            }
        }
    } while ((!feof (hFileExpected)) && bSuccess);

    fclose (hFileActual);
    fclose (hFileExpected);
    return (bSuccess);
}

//
//      _MethodGenGatherStrings - Gather all the method strings between the start and stop method flags
//
//      Entry:  hFile           Handle to the file
//              pnLines         Number of lines processed (updated)
//              ppMethodList    Allocated pointer to the method entry list
//              pnListEntries   Number of entries in the list (returned)
//      Exit:   <BOOL>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL _MethodGenGatherStrings (FILE *hFile, int *pnLines, PMETHODENTRY *ppMethodList, int *pnListEntries)
{
    int             nLength, nListEntries, nLines;
    char            szBufferInput[256];
    static char     szRVGA[] = "rVGA";
    static char     szLWGA[] = "lwGA";
    static char     szDebug[] = "DEBUG";
    PMETHODENTRY    pMethodList;

    // Validate parameters
    if ((hFile == NULL) || (pnLines == NULL) || (ppMethodList == NULL) || (pnListEntries == NULL))
        return (FALSE);

    // Initialize local variables
    *ppMethodList = pMethodList = NULL;
    *pnListEntries = nListEntries = 0;
    nLines = *pnLines;

    // Gather methods from the expected file
    nLines++;
    if (fgets (szBufferInput, sizeof (szBufferInput), hFile) == NULL)
        return (FALSE);

    // Find the beginning of the method
    while (strstr (szBufferInput, szMethodStart) == NULL)
    {
        nLines++;
        if (fgets (szBufferInput, sizeof (szBufferInput), hFile) == NULL)
            return (FALSE);
    }

    // Store strings until the end of the method
    while (strstr (szBufferInput, szMethodStop) == NULL)
    {
        // Determine whether to use this line or not
        if (!((strncmp (szBufferInput, szRVGA, sizeof (szRVGA) - 1) == 0) ||
            (strncmp (szBufferInput, szLWGA, sizeof (szLWGA) - 1) == 0) ||
            (strncmp (szBufferInput, szDebug, sizeof (szDebug) - 1) == 0)))
        {
            // Okay... It's a "live" line, so process it
            // Make space for the new method entry
            if (pMethodList == NULL)
                pMethodList = (PMETHODENTRY) malloc (sizeof (METHODENTRY));
            else
                pMethodList = (PMETHODENTRY) realloc (pMethodList, sizeof (METHODENTRY) * (nListEntries + 1));

            // Make space for the string within the method entry and copy it over
//          printf ("_MethodGenGatherStrings: Copy over entry %d: %s", nListEntries, szBufferInput);
            _MethodGenExtractName (szBufferInput, szBufferInput);
            nLength = (int) strlen (szBufferInput) + 1;
            (pMethodList + nListEntries)->pMethodName = (LPSTR) malloc (nLength);
            strcpy ((pMethodList + nListEntries)->pMethodName, szBufferInput);
            (pMethodList + nListEntries)->bFound = FALSE;
            nListEntries++;                                 // Count the entries
        }

        nLines++;
        if (fgets (szBufferInput, sizeof (szBufferInput), hFile) == NULL)
            return (FALSE);
    }

    // Set the return values
    *ppMethodList = pMethodList;
    *pnListEntries = nListEntries;
    *pnLines = nLines;
    return (TRUE);
}

//
//      _MethodGenFreeList - Free the method list allocated by _MethodGatherStrings
//
//      Entry:  pMethodList     Pointer to the list of strings
//              nListEntries    Number of entries
//      Exit:   <BOOL>          Success flag (TRUE = Successful, FALSE = Not)
//
BOOL _MethodGenFreeList (PMETHODENTRY pMethodList, int nListEntries)
{
    int     i;

    if (pMethodList == NULL)
        return (FALSE);

    for (i = 0; i < nListEntries; i++)
    {
        free ((pMethodList + i)->pMethodName);
    }
    free (pMethodList);

    return (TRUE);
}

//
//      _MethodGenCompareLists - Verify that the two lists are similar
//
//      Entry:  pMethodListExpected     Pointer to the EXPECTED method list
//              nListEntriesExpected    Number of items on the EXPECTED list
//              pMethodListActual       Pointer to the ACTUAL method list
//              nListEntriesActual      Number of items on the ACTUAL list
//      Exit:   <BOOL>                  Success flag (TRUE = Lists are similar, FALSE = Mismatch)
//
//      Notes:  1) Methods may be out of order
//              2) More than 1 ACTUAL entry may exist
//              3) Only method names are checked, not method data
//
BOOL _MethodGenCompareLists (PMETHODENTRY pMethodListExpected, int nListEntriesExpected, PMETHODENTRY pMethodListActual, int nListEntriesActual)
{
    int     i, j;

    // Cycle thru the EXPECTED list
    for (i = 0; i < nListEntriesExpected; i++)
    {
        // Check each of the ACTUAL entries
        for (j = 0; j < nListEntriesActual; j++)
        {
            if (strcmp ((pMethodListExpected + i)->pMethodName, (pMethodListActual + j)->pMethodName) == 0)
            {
                (pMethodListExpected + i)->bFound = TRUE;
                (pMethodListActual + j)->bFound = TRUE;
                // Note: do NOT "break" here, so that multiple ACTUALs may be found
            }
        }
    }

    // Cycle thru the EXPECTED list, making sure everything was found
    for (i = 0; i < nListEntriesExpected; i++)
    {
        if (!(pMethodListExpected + i)->bFound)
        {
            printf ("ERROR: EXPECTED method %s missing.\n", (pMethodListExpected + i)->pMethodName);
            return (FALSE);
        }
    }

    // Cycle thru the ACTUAL list, making sure everything was found
    for (i = 0; i < nListEntriesActual; i++)
    {
        if (!(pMethodListActual + i)->bFound)
        {
            printf ("ERROR: ACTUAL method %s not expected.\n", (pMethodListActual + i)->pMethodName);
            return (FALSE);
        }
    }

    return (TRUE);
}

//
//      _MethodGenExtractName - Extract the method name from the raw data
//
//      Entry:  pMethodRaw  Raw method buffer
//              pMethodName Isolated method name
//      Exit:   <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
//      Note:   The pointers, "pMethodRaw" and "pMethodName", can be the same buffer,
//              in which case the raw data will just be overwritten with the method name.
//
BOOL _MethodGenExtractName (LPSTR pMethodRaw, LPSTR pMethodName)
{
    char        *pstr;
    char        *pstrRaw;
    vector<char> strRaw;

    // Allocate space and work with a copy of the string
    strRaw.resize(strlen(pMethodRaw) + 1); // include terminating '\0'
    pstrRaw = strRaw.data();
    strcpy(pstrRaw, pMethodRaw);

    // Strip out the digits at the end of each line
    // The format of the end of line is <useful data> <space> <hex-digits> <space?>

    // Get to the back of the digits
    pstr = pstrRaw + (strlen (pstrRaw) - 1);
    while (pstr >= pstrRaw)
    {
        if (!isspace (*pstr))
        {
            *(pstr+1) = '\0';
            break;
        }
        pstr--;
    }

    // Get to the front of the digits
    pstr = strrchr (pstrRaw, ' ');
    if (pstr != NULL)
        *pstr = '\0';
    else
        return (FALSE);

    // Now search backwards for the first white space
    pstr--;
    while (pstr >= pstrRaw)
    {
        if (isspace (*pstr))
        {
            pstr++;
            break;
        }
        pstr--;
    }

    // Copy the found name into the return variable
    strcpy (pMethodName, pstr);

    // Release the allocated space
    free (pstrRaw);

    return (TRUE);
}
#endif

#undef  REF_TEST
#define REF_TEST        9
//
//      T1109
//      BlankStressTestLW50 - Test blanking within the display period
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int BlankStressTestLW50 (void)
{
    int         nErr, i, j;
    int         nCols, nRows, nColsSm, nRowsSm, nColStartSm, nRowStartSm;
    int         wXResSm, wYResSm, xStartSm, yStartSm, cFixup, cLWGA;
    WORD        wXRes, wYRes;
    BOOL        bFullVGA;
    LPWORD      ptblFixup, ptblLWGA;
    BYTE        chr;
    static WORD tblFixupFull[] = {0x4D02, 0x8103, 0x0005, 0x3E07, 0xBF15, 0x1F16};
    static WORD tblFixupSmall[] = {0x2502, 0x8103, 0x0005, 0x1007, 0x0F15, 0x0316};
    static WORD tblLWGAFull[] = {
        0x8f60, 0x0161, 0x3f62, 0x0163, 0x0064, 0x0065, 0x3f66, 0x0167, 0x4768, 0x0169, 0x676a, 0x016b,
        0x1b70, 0x0071, 0x1872, 0x0073, 0x0074, 0x0075, 0x1876, 0x0077, 0x1978, 0x0079, 0x1a7a, 0x007b
    };
    static WORD tblLWGASmall[] = {
        0x8f60, 0x0161, 0x3f62, 0x0163, 0x0064, 0x0065, 0x3f66, 0x0167, 0x4768, 0x0169, 0x676a, 0x016b,
        0x1b70, 0x0071, 0x1872, 0x0073, 0x0074, 0x0075, 0x1876, 0x0077, 0x1978, 0x0079, 0x1a7a, 0x007b
    };

    SimComment ("Starting BlankStressTestLW50.");
#ifdef LW_MODS
    LegacyVGA::CRCManager ()->GetHeadDac (&head_index, &dac_index);     // These variables are global to this module
#else
    head_index = 0;
    dac_index = 0;
#endif
    nErr = ERROR_NONE;
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto BlankStressTest_exit;

    // Set the mode
    SimSetState (TRUE, TRUE, FALSE);
    SetMode (0x12);
    SimSetState (TRUE, TRUE, TRUE);

    // Unlock extended registers
    SimComment ("Unlock extended registers.");
    IOWordWrite (CRTC_CINDEX, 0x573F);

    // Set rVGA mode
    SimComment ("Set rVGA mode.");
    IOByteWrite (CRTC_CINDEX, 0x19);
    IOByteWrite (CRTC_CDATA, 0x10 | (head_index<<7));

    // Determine screen dimensions
    SimComment ("Determine screen dimensions.");
    GetResolution (&wXRes, &wYRes);
    nCols = wXRes / 8;                      // Assume 8x16 font
    nRows = wYRes / 16;
    SetLine4Columns ((WORD) nCols);

    // Set resolution based locals
    if (bFullVGA)
    {
        // 640x480 -> 608x416
        ptblFixup = tblFixupFull;
        cFixup = sizeof (tblFixupFull) / sizeof (WORD);
        ptblLWGA = tblLWGAFull;
        cLWGA = sizeof (tblLWGAFull) / sizeof (WORD);

        nColsSm = 76;
        nRowsSm = 26;
        nColStartSm = 2;
        nRowStartSm = 2;
        wXResSm = nColsSm * 8;
        wYResSm = nRowsSm * 16;
        xStartSm = nColStartSm * 8;
        yStartSm = nRowStartSm * 16;
    }
    else
    {
        // Small frame can't use column based calc
        // 320x20 -> 304x12
        ptblFixup = tblFixupSmall;
        cFixup = sizeof (tblFixupSmall) / sizeof (WORD);
        ptblLWGA = tblLWGASmall;
        cLWGA = sizeof (tblLWGASmall) / sizeof (WORD);

        nRowStartSm = nColStartSm = nRowsSm = nColsSm = 0;      // Prevent compiler warning
        wXResSm = 288;
        wYResSm = 12;
        xStartSm = 16;
        yStartSm = 4;
    }

    // Load backend registers
    SimComment ("Load backend registers.");
    for (i = 0; i < cLWGA; i++)
    {
        IOWordWrite (CRTC_CINDEX, ptblLWGA[i]);
    }

    // Paint a pretty picture on the screen
    SimComment ("Paint a pretty picture on the screen.");
    SimComment (" -- Draw characters.");
    if (bFullVGA)
    {
        // Strategically place characters for full frame image
        // Upper left corner
        PlanarCharOut ('0', 0x0F, 0, 0, 0);
        PlanarCharOut ('0', 0x0F, nColStartSm, 0, 0);
        PlanarCharOut ('0', 0x0F, 0, nRowStartSm, 0);
        PlanarCharOut ('A', 0x0F, nColStartSm, nRowStartSm, 0);

        // Upper right corner
        PlanarCharOut ('1', 0x0F, nCols - 1, 0, 0);
        PlanarCharOut ('1', 0x0F, nColStartSm + (nColsSm - 1), 0, 0);
        PlanarCharOut ('1', 0x0F, nCols - 1, nRowStartSm, 0);
        PlanarCharOut ('B', 0x0F, nColStartSm + (nColsSm - 1), nRowStartSm, 0);

        // Lower right corner
        PlanarCharOut ('2', 0x0F, nCols - 1, nRows - 1, 0);
        PlanarCharOut ('2', 0x0F, nColStartSm + (nColsSm - 1), nRows - 1, 0);
        PlanarCharOut ('2', 0x0F, nCols - 1, nRowStartSm + (nRowsSm - 1), 0);
        PlanarCharOut ('C', 0x0F, nColStartSm + (nColsSm - 1), nRowStartSm + (nRowsSm - 1), 0);

        // Lower left corner
        PlanarCharOut ('3', 0x0F, 0, nRows - 1, 0);
        PlanarCharOut ('3', 0x0F, nColStartSm, nRows - 1, 0);
        PlanarCharOut ('3', 0x0F, 0, nRowStartSm + (nRowsSm - 1), 0);
        PlanarCharOut ('D', 0x0F, nColStartSm, nRowStartSm + (nRowsSm - 1), 0);
    }
    else
    {
        chr = '0';
        for (i = 0; i < nRows; i++)
        {
             for (j = 0; j < nCols; j++)
             {
                PlanarCharOut (chr++, 0x0F, j, i, 0);
             }
        }
    }

    // Draw the outer box
    SimComment (" -- Draw the outer box.");
    wXRes--; wYRes--;
    Line4 (0, 0, wXRes, 0, 0x0F);
    Line4 (wXRes, 0, wXRes, wYRes, 0x0F);
    Line4 (0, wYRes, wXRes, wYRes, 0x0F);
    Line4 (0, 0, 0, wYRes, 0x0F);

    // Draw the inner boxes
    SimComment (" -- Draw the inner boxes.");
    wXResSm--; wYResSm--;
    Line4 (0, yStartSm, wXRes, yStartSm, 0x0F);
    Line4 (xStartSm, 0, xStartSm, wYRes, 0x0F);
    Line4 (0, yStartSm + wYResSm, wXRes, yStartSm + wYResSm, 0x0F);
    Line4 (xStartSm + wXResSm, 0, xStartSm + wXResSm, wYRes, 0x0F);

    // Disable CRTC write protect
    SimComment ("Disable CRTC write protect.");
    IOByteWrite (CRTC_CINDEX, 0x11);
    IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) & 0x7F);

    for (i = 0; i < 2; i++)
    {
        // Second time through, set LWGA mode
        if (i == 1)
        {
            // Turn off overscan
            SimComment ("Turn off overscan.");
            IOByteRead (INPUT_CSTATUS_1);
            IOByteWrite (ATC_INDEX, 0x31);
            IOByteWrite (ATC_INDEX, 0x00);

            SimComment ("Turn on LWGA mode.");
            IOByteWrite (CRTC_CINDEX, 0x19);
            IOByteWrite (CRTC_CDATA, head_index << 7);
        }

        // Capture original frame - no changes to blanking
        SimComment ("Capture original frame - no changes to blanking.");
        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto BlankStressTest_exit;
            }
        }

        // Set the inner blanking area
        SimComment ("Set the inner blanking area.");
        for (j = 0; j < cFixup; j++)
        {
            IOWordWrite (CRTC_CINDEX, ptblFixup[j]);
        }

        // Capture frame with inner blanking
        SimComment ("Capture frame with inner blanking.");
        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto BlankStressTest_exit;
            }
        }

        // Turn on overscan (there should be none)
        SimComment ("Turn on overscan (there should be none).");
        IOByteRead (INPUT_CSTATUS_1);
        IOByteWrite (ATC_INDEX, 0x31);
        IOByteWrite (ATC_INDEX, 0x0F);

        // Capture frame with inner blanking and overscan
        SimComment ("Capture frame with inner blanking and overscan.");
        if (!FrameCapture (REF_PART, REF_TEST))
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto BlankStressTest_exit;
            }
        }
    }

BlankStressTest_exit:
    if (nErr == ERROR_NONE)
        SimComment ("Blank Stress Test completed.");
    else
        SimComment ("Blank Stress Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        10
//
//      T1110
//      LineCompareTestLW50 - Smooth scroll a split screen window to the top of the display
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int LineCompareTestLW50 (void)
{
    static BYTE pattern0[] = {
        '0', 0x0F, '1', 0x0F, '2', 0x0F, '3', 0x0F, '4', 0x0F,
        '5', 0x0F, '6', 0x0F, '7', 0x0F, '8', 0x0F, '9', 0x0F
    };
    static BYTE pattern1[] = {
        'A', 0x70, 'B', 0x70, 'C', 0x70, 'D', 0x70, 'E', 0x70,
        'F', 0x70, 'G', 0x70, 'H', 0x70, 'I', 0x70, 'J', 0x70,
        'K', 0x70, 'L', 0x70, 'M', 0x70, 'N', 0x70, 'O', 0x70,
        'P', 0x70, 'Q', 0x70, 'R', 0x70, 'S', 0x70, 'T', 0x70,
        'U', 0x70, 'V', 0x70, 'W', 0x70, 'X', 0x70, 'Y', 0x70,
        'Z', 0x70
    };
    int         nErr, i, n, nMemSize, nRows, nColumns, nRowCount, nLineStart, yChar;
    SEGOFF      lpVideo;
    BYTE        temp;
    WORD        wXRes, wYRes;
    BOOL        bFullVGA;

    nErr = ERROR_NONE;
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto LineCompareTestLW50_exit;

    SimSetState (TRUE, TRUE, FALSE);            // No RAMDAC writes
    SetScans (200);                             // 200 scan line mode
    SetMode (0x03);

    yChar = 8;
    GetResolution (&wXRes, &wYRes);
    nRows = (wYRes + 13) / yChar;
    nColumns = wXRes / 8;
    SimSetState (TRUE, TRUE, TRUE);

    // Fill page 0 memory with a pattern
    lpVideo = (SEGOFF) 0xB8000000;
    nMemSize = nColumns*nRows*2;
    i = n = 0;
    while (n < nMemSize)
    {
        MemByteWrite (lpVideo++, pattern0[i]);
        if (++i >= (int) sizeof (pattern0)) i = 0;
        n++;
    }

    // Fill page 1 memory with a pattern
    lpVideo = (SEGOFF) 0xB8001000;
    nMemSize = nColumns*nRows*2;
    if (!bFullVGA)
        nMemSize += nColumns * 2;               // Compensate for the last row not being hidden
    i = n = 0;
    while (n < nMemSize)
    {
        MemByteWrite (lpVideo++, pattern1[i]);
        if (++i >= (int) sizeof (pattern1)) i = 0;
        n++;
    }

    SimDumpMemory ("T1110.VGA");

    // Disable pixel panning for bottom half of screen
    IOByteRead (INPUT_CSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x30);
    IOByteWrite (ATC_INDEX, (BYTE) (IOByteRead (ATC_RDATA) | 0x20));

    // Set display start to 1000h
    IOWordWrite (CRTC_CINDEX, 0x080C);
    IOWordWrite (CRTC_CINDEX, 0x000D);

    // Set preset row scan to 7 to show that the internal row scan counter
    // is NOT set to the preset row scan count at line compare. It appears
    // that all relevant internal counters are reset to "0" at line compare.
    IOWordWrite (CRTC_CINDEX, 0x0708);

    // Set the line compare value
    if (bFullVGA)
        nLineStart = 199;
    else
        nLineStart = 9;
    // CRTC[9].6 is line compare bit 9
    IOByteWrite (CRTC_CINDEX, 0x09);
    temp = (BYTE) (IOByteRead (CRTC_CDATA) & 0xBF);
    IOByteWrite (CRTC_CDATA, (BYTE) (temp | ((nLineStart & 0x200) >> 3)));

    // CRTC[7].4 is line compare bit 8
    IOByteWrite (CRTC_CINDEX, 0x07);
    temp = (BYTE) (IOByteRead (CRTC_CDATA) & 0xEF);
    IOByteWrite (CRTC_CDATA, (BYTE) (temp | ((nLineStart & 0x100) >> 4)));

    // CRTC[18].0..7 is line compare bits 0-7
    IOByteWrite (CRTC_CINDEX, 0x18);
    IOByteWrite (CRTC_CDATA, (BYTE) (nLineStart & 0xFF));

    // Capture the initial frame and set the number of rows based on
    // whether this is a visual test or whether it is an automated test.
    nRowCount = __min (wYRes - 1, 34);

    StartCapture (1);
    FrameCapture (REF_PART, REF_TEST);
    WaitNotVerticalRetrace ();

    for (i = nRowCount, n = 0; i >= 0; i--, n++)
    {
        // Do comparison one scan line before the end.
        //  Standard VGA is off by one.
        if (!FrameCapture (REF_PART, REF_TEST))
        {

            if (i == 0)
            {
                if (GetKey () == KEY_ESCAPE)
                {
                    nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                    goto LineCompareTestLW50_exit;
                }
            }
            else
            {
                if (_kbhit ())
                {
                    if (GetKey () == KEY_ESCAPE)
                    {
                        nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                        goto LineCompareTestLW50_exit;
                    }
                }
            }
        }

        WaitNotVerticalRetrace ();

        IOByteWrite (CRTC_CINDEX, 0x0D);
        IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) + 1 ));

    }

    FrameCapture (REF_PART, REF_TEST);
    WaitNotVerticalRetrace ();

    for (i = nRowCount, n = 0; i >= 0; i--, n++)
    {
        // Do comparison one scan line before the end.
        //  Standard VGA is off by one.
        if (!FrameCapture (REF_PART, REF_TEST))
        {

            if (i == 0)
            {
                if (GetKey () == KEY_ESCAPE)
                {
                    nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                    goto LineCompareTestLW50_exit;
                }
            }
            else
            {
                if (_kbhit ())
                {
                    if (GetKey () == KEY_ESCAPE)
                    {
                        nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                        goto LineCompareTestLW50_exit;
                    }
                }
            }
        }

        WaitNotVerticalRetrace ();
        IOByteWrite (CRTC_CINDEX, 0x0D);
        IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) - 1 ));
    }

    FrameCapture (REF_PART, REF_TEST);
    WaitVerticalRetrace ();

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }

LineCompareTestLW50_exit:
    EndCapture ();
    if (nErr == ERROR_NONE)
        SimComment ("Line Compare Test completed.");
    else
        SimComment ("Line Compare Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        11
//
//      T1111
//      StartAddressTestLW50 - Simulate the DMU smooth scroll test which depends on start address being latched properly
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int StartAddressTestLW50 (void)
{
    static BYTE pattern0[] =
    {
        '0', 0x0F, '1', 0x0F, '2', 0x0F, '3', 0x0F, '4', 0x0F,
        '5', 0x0F, '6', 0x0F, '7', 0x0F, '8', 0x0F, '9', 0x0F
    };
    static BYTE tblPixelPalwalues[] = {8, 0, 1, 2, 3, 4, 5, 6, 7};
    int         nErr, i, n, nMemSize, nRows, nColumns, nRowCount, xChar, yChar, nFrame;
    SEGOFF      lpVideo;
    BYTE        byBytePan, byPixelPan, byPresetRow;
    WORD        wSimType, wXRes, wYRes, wStartAddr, wLineCompare;
    char        szMsg[128];

    nErr = ERROR_NONE;
    wSimType = SimGetType ();
    if (!DisplayInspectionMessage ())
        goto StartAddressTestLW50_exit;

    // Set up the mode and callwlate the screen size
    SimSetState (TRUE, TRUE, FALSE);            // No RAMDAC writes
    SetMode (0x03);
    yChar = 16;
    xChar = 9;
    GetResolution (&wXRes, &wYRes);
    nRows = (wYRes + (yChar - 1)) / yChar;
    nColumns = wXRes / xChar;
    SimSetState (TRUE, TRUE, TRUE);

    // Fill page 0 memory with a pattern
    lpVideo = (SEGOFF) 0xB8000000;
    nMemSize = nColumns*nRows*2;
    i = n = 0;
    while (n < nMemSize)
    {
        MemByteWrite (lpVideo++, pattern0[i]);
        if (++i >= (int) sizeof (pattern0)) i = 0;
        n++;
    }

    SimDumpMemory ("T1111.VGA");

    // Disable fastblink (set on modeset)
    SimComment ("Disable fastblink.");
    IOWordWrite (CRTC_CINDEX, 0x573F);
    IOByteWrite (CRTC_CINDEX, 0x20);                                                // LW_CIO_CRE_BLINK_PHASE__INDEX
    IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) & 0x7F);

    // Disable pixel panning for bottom half of screen
    IOByteRead (INPUT_CSTATUS_1);
    IOByteWrite (ATC_INDEX, 0x30);
    IOByteWrite (ATC_INDEX, (BYTE) (IOByteRead (ATC_RDATA) | 0x20));

    // Capture the initial frame and set the number of rows based on
    // whether this is a visual test or whether it is an automated test.
    if ((wSimType & SIM_TURBOACCESS) || (wSimType & SIM_SIMULATION))
        nRowCount = __min (wYRes - 1, 34);
    else
        nRowCount = 200;

    nFrame = 0;
#ifdef T1111_CONT_CAP
    // Start the frame capture
    SimComment ("Let the frame settle.");
    LegacyVGA::CRCManager()->ClearFrameState ();
    LegacyVGA::CRCManager()->ClearClockChanged ();
    DispTest::UpdateNow (LegacyVGA::vga_timeout_ms());                               // Send an update and wait a few frames
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse

    // Set the frame state to wait for blink
    SimComment ("Set the frame state to wait for blink.");
    LegacyVGA::CRCManager ()->WaitBlinkState (1, LegacyVGA::vga_timeout_ms());

    // Trigger the continuous capture
    SimComment ("Trigger the continuous capture.");
    SimComment ("Sending first SetLegacyCrcControl for continuous capture.");
    LegacyVGA::CRCManager ()->CaptureFrame (0, 1, 0, LegacyVGA::vga_timeout_ms());    // This needs the WaitVerticalRetrace that follows
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse
    SimComment ("Sending second SetLegacyCrcControl for continuous capture.");
    LegacyVGA::CRCManager ()->CaptureFrame (0, 1, 0, LegacyVGA::vga_timeout_ms());    // This needs the WaitVerticalRetrace that follows
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse
#else
    BOOL bBlink = BLINK_ON;
//  StartCapture (1);
    WaitLwrsorBlink (BLINK_ON);
    SimComment ("Capture frame 0.");
    FrameCapture (REF_PART, REF_TEST);
    WaitNotVerticalRetrace ();
    nFrame++;
    SimComment ("Capture frame 1.");
    FrameCapture (REF_PART, REF_TEST);
    WaitNotVerticalRetrace ();
    nFrame++;
#endif
    wStartAddr = 0;
    byPixelPan = 0;
    byBytePan = 0;
    wLineCompare = 0;
    byPresetRow = 0;
    for (i = 0; i < nRowCount; i++)
    {
        // Capture the frame
        if (_kbhit ())
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto StartAddressTestLW50_exit;
            }
        }
        sprintf (szMsg, "Capture frame %d.", nFrame++);
        SimComment (szMsg);
#ifdef T1111_CONT_CAP
        LegacyVGA::CRCManager()->CaptureFrame(1, 1, 0, LegacyVGA::vga_timeout_ms());
#endif

        // Set pixel pan and line compare
        WaitNotVerticalRetrace ();                      // Resets ATC index
        SetLineCompare (CRTC_CINDEX, wLineCompare);

#ifndef T1111_CONT_CAP
        if ((nFrame % 8) == 1)
        {
            bBlink = !bBlink;
            WaitLwrsorBlink (bBlink ?  BLINK_ON : BLINK_OFF);
        }
        FrameCapture (REF_PART, REF_TEST);              // Needed here to simulate caputuring value latching logic
#endif

        IOByteWrite (ATC_INDEX, 0x33);
        IOByteWrite (ATC_INDEX, tblPixelPalwalues[byPixelPan]);

        // Set byte pan and start address
        WaitNotBlank ();

        IOByteWrite (CRTC_CINDEX, 0x08);
        IOByteWrite (CRTC_CDATA, (BYTE) ((byBytePan << 5) | byPresetRow));

        IOByteWrite (CRTC_CINDEX, 0x0D);
        IOByteWrite (CRTC_CDATA, (BYTE) (wStartAddr & 0xFF));
        IOByteWrite (CRTC_CINDEX, 0x0C);
        IOByteWrite (CRTC_CDATA, (BYTE) ((wStartAddr >> 8) & 0xFF));

        // Set up next screen
        wLineCompare++;
        byPixelPan++;
        if (byPixelPan > 8)
        {
            byPixelPan = 0;
            byBytePan++;
            if (byBytePan > 3)
            {
                byBytePan = 0;
                wStartAddr += 4;
            }
        }
        if (++byPresetRow >= yChar)
            byPresetRow = 0;
    }

#ifndef T1111_CONT_CAP
    WaitVerticalRetrace ();
#endif
    for (i = nRowCount; i > 0; i--)
    {
        // Capture the frame
        if (_kbhit ())
        {
            if (GetKey () == KEY_ESCAPE)
            {
                nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                goto StartAddressTestLW50_exit;
            }
        }
        sprintf (szMsg, "Capture frame %d.", nFrame++);
        SimComment (szMsg);
#ifdef T1111_CONT_CAP
        LegacyVGA::CRCManager()->CaptureFrame(1, 1, 0, LegacyVGA::vga_timeout_ms());
#endif

        // Set pixel pan and line compare
        WaitNotVerticalRetrace ();                      // Resets ATC index
        SetLineCompare (CRTC_CINDEX, wLineCompare);

#ifndef T1111_CONT_CAP
        if ((nFrame % 8) == 1)
        {
            bBlink = !bBlink;
            WaitLwrsorBlink (bBlink ?  BLINK_ON : BLINK_OFF);
        }
        FrameCapture (REF_PART, REF_TEST);              // Needed here to simulate caputuring value latching logic
#endif

        IOByteWrite (ATC_INDEX, 0x33);
        IOByteWrite (ATC_INDEX, tblPixelPalwalues[byPixelPan]);

        // Set byte pan and start address
        WaitNotBlank ();

        IOByteWrite (CRTC_CINDEX, 0x08);
        IOByteWrite (CRTC_CDATA, (BYTE) ((byBytePan << 5) | byPresetRow));

        IOByteWrite (CRTC_CINDEX, 0x0D);
        IOByteWrite (CRTC_CDATA, (BYTE) (wStartAddr & 0xFF));
        IOByteWrite (CRTC_CINDEX, 0x0C);
        IOByteWrite (CRTC_CDATA, (BYTE) ((wStartAddr >> 8) & 0xFF));

        // Set up next screen
        wLineCompare--;
        byPixelPan--;
        if (byPixelPan == 0xFF)     // Byte down wraps from 0x00 to 0xFF
        {
            byPixelPan = 8;
            byBytePan--;
            if (byBytePan == 0xFF)  // Byte down wraps from 0x00 to 0xFF
            {
                byBytePan = 3;
                wStartAddr -= 4;
            }
        }
        if (--byPresetRow == 0xFF)  // Byte wraps down from 0x00 to 0xFF
            byPresetRow = yChar - 1;
    }

    sprintf (szMsg, "Capture frame %d.", nFrame++);
    SimComment (szMsg);
#ifdef T1111_CONT_CAP
    // Last frame is special
    LegacyVGA::CRCManager()->CaptureFrame(1, 1, 0, LegacyVGA::vga_timeout_ms());
    sprintf (szMsg, "Capture frame %d.", nFrame++);
    SimComment (szMsg);
    LegacyVGA::CRCManager ()->WaitCapture(1, 0, LegacyVGA::vga_timeout_ms());
    SimComment ("Last frame.");
    LegacyVGA::CRCManager ()->WaitCapture(1, 0, LegacyVGA::vga_timeout_ms());
//  sprintf (szMsg, "Capture frame %d.", nFrame++);
//  SimComment (szMsg);
//  LegacyVGA::CRCManager ()->WaitCapture(1, 0, LegacyVGA::vga_timeout_ms());
#else
    if ((nFrame % 8) == 1)
    {
        bBlink = !bBlink;
        WaitLwrsorBlink (bBlink ?  BLINK_ON : BLINK_OFF);
    }
//  FrameCapture (REF_PART, REF_TEST);
    WaitVerticalRetrace ();
//  sprintf (szMsg, "Capture frame %d.", nFrame++);
//  SimComment (szMsg);
//  FrameCapture (REF_PART, REF_TEST);
//  WaitVerticalRetrace ();

    if (!FrameCapture (REF_PART, REF_TEST))
    {
        if (GetKey () == KEY_ESCAPE)
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
    }
#endif

StartAddressTestLW50_exit:
#ifndef T1111_CONT_CAP
    EndCapture ();
#endif
    if (nErr == ERROR_NONE)
        SimComment ("Start Address Test completed.");
    else
        SimComment ("Start Address Test failed.");
    SystemCleanUp ();
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        12
//
//      T1112
//      InputStatus1TestLW50 - Verify Input Status 1 in a system timing independent manner
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
//      Notes:  1) This test is really a combined, timing independent version of SyncStatusTest (T0707)
//                  and VideoStatusTest (T0603).
//              2) The inline assembly in this function is needed when run at DOS on the slow Model 70.
//
int InputStatus1TestLW50 (void)
{
    int             nErr, nLines, i, j, nExpectedLines;
    BOOL            bFullVGA;
    BYTE            temp, byActual, byExpected, byMaskDisplayDisable, byMaskVSync;
    WORD            wXRes, wYRes;
    static MUXDATA mx[] =
    {
        {0x01, 4, 0, 0x04, 3, 0},
        {0x10, 0, 0, 0x20, 0, 0},
        {0x02, 3, 0, 0x08, 2, 0},
        {0x40, 0, 2, 0x80, 0, 2}
    };
    static BYTE     tblData[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00, 0x55, 0xAA, 0xFF, 0xCC, 0x33, 0x66, 0x99};
#ifdef LW_MODS
    WORD        LW_CIO_INP1__COLOR, LW_CIO_ARX, LW_CIO_AR_PLANE__READ;
//    REGFIELD    rfDisplayDisable, rfVSync;

    LW_CIO_INP1__COLOR = SimGetRegAddress ("LW_CIO_INP1__COLOR");
    LW_CIO_ARX = SimGetRegAddress ("LW_CIO_ARX");
    LW_CIO_AR_PLANE__READ = SimGetRegAddress ("LW_CIO_AR_PLANE__READ");

//    SimGetRegField ("LW_CIO_INP1__COLOR", "LW_CIO_INP1_DISPLAY_DISABLE", &rfDisplayDisable);
//    SimGetRegField ("LW_CIO_INP1__COLOR", "LW_CIO_INP1_VSYNC", &rfVSync);

//    byMaskDisplayDisable = rfDisplayDisable.readmask;
//    byMaskVSync = rfVSync.readmask;
#endif
    byMaskDisplayDisable = 0x01;
    byMaskVSync = 0x08;
    constexpr BYTE byHackLW50 = 0xFF;

    nErr = ERROR_NONE;
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto InputStatus1Test_exit;

#ifdef LW_MODS
    // Tell the user where the test is running
    {
        char            szMsg[512];
        LegacyVGA::CRCManager ()->GetHeadDac (&head_index, &dac_index);     // These variables are global to this module
        sprintf (szMsg, "Starting with head index %d and dac index %d.", head_index, dac_index);
        SimComment (szMsg);
    }
    if (bFullVGA)
    {
        nExpectedLines = 480;
        DispTest::IsoInitVGA (head_index, dac_index, 640, 480, 1, 1, false, 1, 25000, 0, 0, DispTest::GetGlobalTimeout ());
    }
    else
    {
        nExpectedLines = 20;
        DispTest::IsoInitVGA (head_index, dac_index, 320, 20, 1, 1, false, 1, 25000, 0, 0, DispTest::GetGlobalTimeout ());
    }
#else
    if (bFullVGA)
        nExpectedLines = 480;
    else
        nExpectedLines = 20;
#endif

    // Set up the mode and callwlate the screen size
    SimSetState (TRUE, TRUE, FALSE);        // No RAMDAC write
    SetMode (0x12);
    GetResolution (&wXRes, &wYRes);
    SimSetState (TRUE, TRUE, TRUE);         // Enable RAMDAC again

    // Wait until the frame settles after the mode set and then wait until the beginning of the frame.
#ifdef LW_MODS
    DispTest::UpdateNow (LegacyVGA::vga_timeout_ms());                   // Send an update to get correct a frame size
    WaitVerticalRetrace ();
    WaitVerticalRetrace ();
    WaitVerticalRetrace ();
    WaitVerticalRetrace ();
    temp = IOByteRead (LW_CIO_INP1__COLOR);             // INPUT_CSTATUS_1
//  while ((temp & 0x01) == 0x01)
    while ((temp & byMaskDisplayDisable) == byMaskDisplayDisable)
    {
        temp = IOByteRead (LW_CIO_INP1__COLOR);         // INPUT_CSTATUS_1
    }
#else
    WaitVerticalRetrace ();
    WaitVerticalRetrace ();
    WaitVerticalRetrace ();
    WaitVerticalRetrace ();
    temp = IOByteRead (LW_CIO_INP1__COLOR);
    while ((temp & 0x01) == 0x01)
    {
        temp = IOByteRead (LW_CIO_INP1__COLOR);
    }
#endif

    //  Count the number of display enables until vertical retrace.
    // This number should be about 480.
    _disable ();
    nLines = 0;
#if LW_MODS
    while ((temp & byMaskVSync) == 0)
    {
        // Wait while the display is active
        while ((temp & byMaskDisplayDisable) == 0)
        {
            temp = IOByteRead (LW_CIO_INP1__COLOR);
        }
        // Wait while the display is inactive
        while (((temp & byMaskDisplayDisable) == byMaskDisplayDisable) &&
                ((temp & byMaskVSync) == 0x00))
        {
            temp = IOByteRead (LW_CIO_INP1__COLOR);
        }
        nLines++;
    }
#else
    while ((temp & 0x08) == 0x00)
    {
        // Wait while the display is active
        while ((temp & 0x01) == 0x00)
        {
            temp = IOByteRead (LW_CIO_INP1__COLOR);
        }
        while (((temp & 0x01) == 0x01) &&
                ((temp & 0x08) == 0x00))
        {
            temp = IOByteRead (LW_CIO_INP1__COLOR);
        }
        temp = IOByteRead (LW_CIO_INP1__COLOR);
        nLines++;
    }
#endif
    _enable ();

    // Verify number of lines
    if (nLines != nExpectedLines)
    {
        printf ("ERROR: Expected lines = %d, Actual lines = %d\n", nExpectedLines, nLines);
        nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
        // Let the test continue to run
    }

    // Blank the entire screen on LW50
    if (byHackLW50 != 0xFF)
    {
        IOByteRead (LW_CIO_INP1__COLOR);                    // For LW50
        IOByteWrite (LW_CIO_ARX, 0x00);                     // For LW50 (ATC_INDEX)
        WaitVerticalRetrace ();                             // For LW50
    }

    // Now test the video muxes
    for (i = 0; i < 4; i++)
    {
        // Set video status mux
        IOByteRead (LW_CIO_INP1__COLOR);
        IOByteWrite (LW_CIO_ARX, 0x32 & byHackLW50);
//      IOByteWrite (LW_CIO_ARX, 0x12);                     // For LW50
        temp = IOByteRead (LW_CIO_AR_PLANE__READ);          // ATC_RDATA
        IOByteWrite (LW_CIO_ARX, (BYTE) ((temp & 0x0F) | (i << 4)));
        for (j = 0; j < (int) sizeof (tblData); j++)
        {
            IOByteRead (LW_CIO_INP1__COLOR);
            IOByteWrite (LW_CIO_ARX, 0x00);                 // Set palette bits 0-5
            IOByteWrite (LW_CIO_ARX, (BYTE) (tblData[j] & 0x3F));
            IOByteWrite (LW_CIO_ARX, 0x34 & byHackLW50);    // Set palette bits 6-7
//          IOByteWrite (LW_CIO_ARX, 0x14);                 // Set palette bits 6-7 (for LW50)
            IOByteWrite (LW_CIO_ARX, (BYTE) ((tblData[j] & 0xC0) >> 4));
//
// HACK: Due to the double buffering of the internal palette, to make LW50 work,
//      the whole screen must display overscan, which is not double buffered.
//
            IOByteWrite (LW_CIO_ARX, 0x31 & byHackLW50);    // Must include overscan
//          IOByteWrite (LW_CIO_ARX, 0x11);                 // Must include overscan (for LW50)
            IOByteWrite (LW_CIO_ARX, tblData[j]);

            // Wait for display
#ifdef LW_MODS
            while (((byActual = IOByteRead (LW_CIO_INP1__COLOR)) & byMaskDisplayDisable) == byMaskDisplayDisable);
#else
            while (((byActual = IOByteRead (LW_CIO_INP1__COLOR)) & 0x01) == 0x01);
#endif
            byActual &= 0x30;
            byExpected = (((tblData[j] & mx[i].bit0) << mx[i].shl0) >> mx[i].shr0) |
                                (((tblData[j] & mx[i].bit1) << mx[i].shl1) >> mx[i].shr1);
            if (byActual != byExpected)
            {
                printf ("ERROR: Actual video data does not match expected video data.\n");
                printf ("byActual = %Xh, byExpected = %Xh, tblData[%d] = %Xh, i = %d, j = %d\n", byActual, byExpected, j, tblData[j], i, j);
                nErr = FlagError (ERROR_VIDEO, REF_PART, REF_TEST, 0, 0, byExpected, byActual);
                goto InputStatus1Test_exit;
            }
        }
    }

InputStatus1Test_exit:
    SystemCleanUp ();
#ifdef LW_MODS
    DispTest::IsoShutDowlwGA (head_index, dac_index, DispTest::GetGlobalTimeout ());
#endif
    if (nErr == ERROR_NONE)
        SimComment ("Input Status 1 Test passed.");
    else
        SimComment ("Input Status 1 Test failed.");
    return (nErr);
}

#undef  REF_TEST
#define REF_TEST        13
//
//      T1113
//      DoubleBufferTestLW50 - Verify double buffering with continuous capture
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int DoubleBufferTestLW50 (void)
{
    int     nFrames, i, j, wySprite, nRowOffset;
    BOOL    bFullVGA, bFrameA;
    WORD    wXRes, wYRes, wFrameA, wFrameB, wOffScreen, wOffset, wFrameOffset;
    SEGOFF  lpVideo;
    char    szMsg[128];

    // Initialize local variables
    SimComment ("DoubleBufferTestLW50 is being run.");
    bFullVGA = SimGetFrameSize ();
    if (!DisplayInspectionMessage ())
        goto DoubleBufferTestLW50_exit;

    lpVideo = (SEGOFF) 0xA0000000;
    wFrameA = 0x0000;
    wFrameB = 0x2000;
    wOffScreen = 0x4000;
    wySprite = 16;
    if (bFullVGA)
    {
        nFrames = 24;
    }
    else
    {
        nFrames = 8;
    }

    // Set up the mode and callwlate the screen size
    SimSetState (TRUE, TRUE, FALSE);        // No RAMDAC write
    SetMode (0x0D);
    GetResolution (&wXRes, &wYRes);
    SimSetState (TRUE, TRUE, TRUE);         // Enable RAMDAC again
    nRowOffset = wXRes / 8;

    // Draw a pretty picture off screen
    SetLine4Columns ((WORD) nRowOffset);
    SetLine4StartAddress (wOffScreen);
    for (i = 0; i < wySprite; i++)
        Line4 (0, i, i, i, i & 0x0F);

    // Copy the picture to frame A
    IOWordWrite (GDC_INDEX, 0x0008);            // Do a latch copy
    for (i = 0; i < wySprite; i++)
    {
        MemByteRead (lpVideo + wOffScreen + (nRowOffset * i));     // Load latches
        MemByteWrite (lpVideo + wFrameA + (nRowOffset * i), 0x00);          // Write any data
        MemByteRead (lpVideo + wOffScreen + (nRowOffset * i) + 1); // Load latches
        MemByteWrite (lpVideo + wFrameA + (nRowOffset * i) + 1, 0x00);      // Write any data
    }
    IOWordWrite (GDC_INDEX, 0xFF08);

#ifdef T1113_CONT_CAP
    // Start the frame capture
    SimComment ("Let the frame settle.");
    LegacyVGA::CRCManager()->ClearFrameState ();
    LegacyVGA::CRCManager()->ClearClockChanged ();
    DispTest::UpdateNow (LegacyVGA::vga_timeout_ms());                               // Send an update and wait a few frames
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse

    // Trigger the continuous capture
    SimComment ("Trigger the continuous capture.");
    SimComment ("Capture frame 0.");
    LegacyVGA::CRCManager ()->CaptureFrame (0, 1, 0, LegacyVGA::vga_timeout_ms());  // This needs the WaitVerticalRetrace that follows
    WaitVerticalRetrace ();                                                         // Sync the beginning to the vsync pulse
#else
    FrameCapture (REF_PART, REF_TEST);
#endif

    // Cycle through each frame
    wOffset = 1;
    bFrameA = TRUE;
    for (i = 0; i < nFrames; i++)
    {
        // Clear picture in the opposite frame (after the first frame)
        if (i == 0)
            wFrameOffset = wFrameB + 1;     // 1st time, Frame B starts here
        else
        {
            wFrameOffset = (bFrameA ? wFrameB : wFrameA) + (wOffset - 2);
            for (j = 0; j < wySprite; j++)
            {
                MemByteWrite (lpVideo + wFrameOffset + (nRowOffset * j), 0x00);
                MemByteWrite (lpVideo + wFrameOffset + (nRowOffset * j) + 1, 0x00);
            }
            wFrameOffset += 2;              // Point to new position
        }

        // Draw picture in opposite frame
        IOWordWrite (GDC_INDEX, 0x0008);            // Do a latch copy
//      for (j = 0; j < wySprite; j++)
        for (j = wySprite - 1; j >= 0 ; j--)        // Reverse copy
        {
            MemByteRead (lpVideo + wOffScreen + (nRowOffset * j));         // Load latches
            MemByteWrite (lpVideo + wFrameOffset + (nRowOffset * j), 0x00);         // Write any data
            MemByteRead (lpVideo + wOffScreen + (nRowOffset * j) + 1);     // Load latches
            MemByteWrite (lpVideo + wFrameOffset + (nRowOffset * j) + 1, 0x00);     // Write any data
        }
        IOWordWrite (GDC_INDEX, 0xFF08);

        // Switch buffers
        wFrameOffset = bFrameA ? wFrameB : wFrameA;
        IOByteWrite (CRTC_CINDEX, 0x0C);
        IOByteWrite (CRTC_CDATA, HIBYTE (wFrameOffset));
        IOByteWrite (CRTC_CINDEX, 0x0D);
        IOByteWrite (CRTC_CINDEX, LOBYTE (wFrameOffset));

        // Wait for sync
        sprintf (szMsg, "Capture frame %d.", i+1);
#ifdef T1113_CONT_CAP
        SimComment (szMsg);
        // for continueous capture, need to send a second CRC update before waiting for CRCs
        if (i == 0)
            LegacyVGA::CRCManager()->CaptureFrame(0, 1, 0, LegacyVGA::vga_timeout_ms());
        else
            LegacyVGA::CRCManager()->CaptureFrame(1, 1, 0, LegacyVGA::vga_timeout_ms());
        WaitVerticalRetrace ();                     // Start address latched at the beginning of vsync
#else
        WaitVerticalRetrace ();                     // Start address latched at the beginning of vsync
        SimComment (szMsg);
        FrameCapture (REF_PART, REF_TEST);
#endif

        // Prepare next frame
        bFrameA = !bFrameA;
        wOffset++;
    }

#ifdef T1113_CONT_CAP
    // Last two frames are special
    LegacyVGA::CRCManager ()->WaitCapture(1, 0, LegacyVGA::vga_timeout_ms());
    LegacyVGA::CRCManager ()->WaitCapture(1, 0, LegacyVGA::vga_timeout_ms());
#endif

DoubleBufferTestLW50_exit:
    SimComment ("Double Buffer Test completed.");
    SystemCleanUp ();
    SimComment ("DoubleBufferTestLW50 is exiting.");
    return ERROR_NONE;
}

#undef  REF_TEST
#define REF_TEST        0
//
//      T1100
//      TestTest - Dummy routine used to write and test the tests before being added to the tests
//
//      Entry:  None
//      Exit:   <int>       DOS ERRORLEVEL value
//
int TestTest (void)
{
#if USE_TESTTEST
    VGA_CRC crc;
    WORD    wType;

    // Get command line variables
    wType = SimGetType ();
    VGA_SIM_BACKDOOR = (wType & SIM_BACKDOOR) ? 1 : 0;
    VGA_SIM_FRAME = (wType & SIM_SMALLFRAME) ? 0 : 1;

    disp_lwga_raster_scale_01 (crc);
    return (ERROR_NONE);

#else
    SimComment ("TestTest is being run.");
    SimComment ("TestTest is exiting.");
    return ERROR_NONE;
#endif
}

#if USE_TESTTEST
#include    "testtest.cpp"
#endif

#ifdef LW_MODS
}           // namespace LegacyVGA
#endif      // LW_MODS

//
//      Copyright (c) 1994-2004 Elpin Systems, Inc.
//      Copyright (c) 2004-2005 SillyTutu.com, Inc.
//      All rights reserved.
//
