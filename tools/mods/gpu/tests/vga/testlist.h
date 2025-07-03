//
//    TESTLIST.H - Function prototypes for the tests in the Elpin Test Suite
//
//    Written by:    Kwun Han, Larry Coffey
//    Date:       7 January 2005
//    Last modified: 23 November 2005
//
#ifdef LW_MODS
namespace LegacyVGA {
#endif

//
// Part 2
//
int MotherboardSetupTest (void);          // 0201
int AdapterSetupTest (void);              // 0202
int LimitedSetupTest (void);              // 0203

//
// Part 3
//
int LwrsorTypeTest (void);                // 0301
int LwrsorLocationTest (void);               // 0302
int LwrsorDisableTest (void);             // 0303
int SyncPulseTimingTest (void);              // 0304
int TextModeSkewTest (void);              // 0305
int VerticalTimesTwoTest (void);          // 0306
int DotClock2Test (void);                 // 0307
int Vload2Vload4Test (void);              // 0308
int CharWidthTest (void);                 // 0309
int CRTCWriteProtectTest (void);          // 0310
int DoubleScanTest (void);                // 0311
int VerticalInterruptTest (void);            // 0312
int PanningTest (void);                   // 0313
int UnderLineLocationTest (void);            // 0314
int SyncDisableTest (void);                  // 0315
int LineCompareTest (void);                  // 0316
int Panning256Test (void);                // 0317
int CountBy4Test (void);                  // 0318
int SkewTest (void);                   // 0319
int PresetRowScanTest (void);             // 0320
int LwrsorSkewTest (void);                // 0321
int NonBIOSLineCompareTest (void);           // 0322

//
// Part 4
//
int SyncResetTest (void);                 // 0401
int CPUMaxBandwidthTest (void);              // 0402
int WriteMapReadPlaneTest (void);            // 0403
int Std256CharSetTest (void);             // 0404
int Ext512CharSetTest (void);             // 0405
int EightLoadedFontsTest (void);          // 0406
int Text64KTest (void);                   // 0407
int LineCharTest (void);                  // 0408
int LargeCharTest (void);                 // 0409
int FontFetchStressTest (void);              // 0410

//
// Part 5
//
int SetResetTest (void);                  // 0501
int ColorCompareTest (void);              // 0502
int ROPTest (void);                       // 0503
int WriteMode1Test (void);                // 0504
int WriteMode2Test (void);                // 0505
int WriteMode3Test (void);                // 0506
int VideoSegmentTest (void);              // 0507
int BitMaskTest (void);                   // 0508
int NonPlanarReadModeTest (void);            // 0509
int NonPlanarWriteMode1Test (void);          // 0510
int NonPlanarWriteMode2Test (void);          // 0511
int NonPlanarWriteMode3Test (void);          // 0512
int ShiftRegisterModeTest (void);            // 0513

//
// Part 6
//
int PaletteAddressSourceTest (void);         // 0601
int BlinkVsIntensityTest (void);          // 0602
int VideoStatusTest (void);                  // 0603
int InternalPaletteTest (void);              // 0604
int OverscanTest (void);                  // 0605
int V54Test (void);                       // 0606
int V67Test (void);                       // 0607
int PixelWidthTest (void);                // 0608
int ColorPlaneEnableTest (void);          // 0609
int GraphicsModeBlinkTest (void);            // 0610

//
// Part 7
//
int SyncPolarityTest (void);              // 0701
int PageSelectTest (void);                // 0702
int ClockSelectsTest (void);              // 0703
int RAMEnableTest (void);                 // 0704
int CRTCAddressTest (void);                  // 0705
int SwitchReadbackTest (void);               // 0706
int SyncStatusTest (void);                // 0707
int VSyncOrVDispTest (void);              // 0708

//
// Part 8
//
int DACMaskTest (void);                   // 0801
int DACReadWriteStatusTest (void);           // 0802
int DACAutoIncrementTest (void);          // 0803
int DACSparkleTest (void);                // 0804
int DACStressTest (void);                 // 0805

//
// Part 9
//
int IORWTest (void);                   // 0901
int ByteModeTest (void);                  // 0902
int Chain2Chain4Test (void);              // 0903
int CGAHerlwlesTest (void);                  // 0904
int LatchTest (void);                     // 0905
int Mode0Test (void);                     // 0906
int Mode3Test (void);                     // 0907
int Mode5Test (void);                     // 0908
int Mode6Test (void);                     // 0909
int Mode7Test (void);                     // 0910
int ModeDTest (void);                     // 0911
int Mode12Test (void);                    // 0912
int ModeFTest (void);                     // 0913
int Mode13Test (void);                    // 0914
int HiResMono1Test (void);                // 0915
int HiResMono2Test (void);                // 0916
int HiResColor1Test (void);                  // 0917
int ModeXTest (void);                     // 0918
int MemoryAccessTest (void);              // 0919
int RandomAccessTest (void);              // 0920

//
// Part 10
//
int UndocLightpenTest (void);             // 1001
int UndocLatchTest (void);                // 1002
int UndocATCToggleTest (void);               // 1003
int UndocATCIndexTest (void);             // 1004

//
// Part 11
//
int TestTest (void);                   // 1100
int IORWTestLW50 (void);                  // 1101
int IORWTestGF100 (void);                  // 1101
int IORWTestGF11x (void);                  // 1101
int IORWTestMCP89 (void);                  // 1101
int SyncPulseTimingTestLW50 (void);          // 1102
int TextModeSkewTestLW50 (void);          // 1103
int VerticalTimesTwoTestLW50 (void);         // 1104
int SkewTestLW50 (void);                  // 1105
int LwrsorBlinkTestLW50 (void);              // 1106
int GraphicsBlinkTestLW50 (void);            // 1107
int MethodGenTestLW50 (void);             // 1108
int MethodGenTestGF11x (void);             // 1108
int BlankStressTestLW50 (void);              // 1109
int LineCompareTestLW50 (void);              // 1110
int StartAddressTestLW50 (void);          // 1111
int InputStatus1TestLW50 (void);          // 1112
int DoubleBufferTestLW50 (void);          // 1113

int   FBPerfMode03TestLW50 (void);           // 1201
int   FBPerfMode12TestLW50 (void);           // 1202
int   FBPerfMode13TestLW50 (void);           // 1203
int   FBPerfMode101TestLW50 (void);          // 1204

#ifdef LW_MODS
};
#endif
