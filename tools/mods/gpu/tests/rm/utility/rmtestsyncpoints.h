/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  rmtestsyncpoints.h
 * @brief Define sync points to be used by rmtests.
 */

// FermiClockPathTest sync points
#define FERMICLOCKPATHTEST_SP_SETUP_START "FermiClockPathTest_SP_Setup_Start"
#define FERMICLOCKPATHTEST_SP_SETUP_STOP  "FermiClockPathTest_SP_Setup_Stop"
#define FERMICLOCKPATHTEST_SP_RUN_START   "FermiClockPathTest_SP_Run_Start"
#define FERMICLOCKPATHTEST_SP_RUN_STOP    "FermiClockPathTest_SP_Run_Stop"

// FermiClockSanityTest sync points
#define FERMICLOCKSANITYTEST_SP_SETUP_START "FermiClockSanityTest_SP_Setup_Start"
#define FERMICLOCKSANITYTEST_SP_SETUP_STOP  "FermiClockSanityTest_SP_Setup_Stop"
#define FERMICLOCKSANITYTEST_SP_RUN_START   "FermiClockSanityTest_SP_Run_Start"
#define FERMICLOCKSANITYTEST_SP_RUN_STOP    "FermiClockSanityTest_SP_Run_Stop"

// ClockUtil sync points
#define CLKUTIL_SP_PROGRAMCLK_START "ClkUtil_SP_ProgramClk_Start"
#define CLKUTIL_SP_PROGRAMCLK_STOP  "ClkUtil_SP_ProgramClk_Stop"

// DmaCopyDecompressTest sync points
#define DMACOPYDECOMPRESSTEST_SP_SETUP_START "DmaCopyDecompressTest_SP_Setup_Start"
#define DMACOPYDECOMPRESSTEST_SP_SETUP_STOP "DmaCopyDecompressTest_SP_Setup_Stop"
#define DMACOPYDECOMPRESSTEST_SP_RUN_START "DmaCopyDecompressTest_SP_Run_Start"
#define DMACOPYDECOMPRESSTEST_SP_RUN_STOP "DmaCopyDecompressTest_SP_Run_Stop"
