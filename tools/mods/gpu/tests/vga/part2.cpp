//
//    PART2.CPP - VGA Core Test Suite (Part 2)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       14 December 2004
//    Last modified: 15 November 2005
//
//    Routines in this file:
//    MotherboardSetupTest Enable VGA access via motherboard setup registers
//    AdapterSetupTest     Enable VGA access via adapter setup registers
//    LimitedSetupTest     Enable VGA access via setup register 3C3h
//

#include <stdio.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

// Function prototypes of the global tests
int MotherboardSetupTest (void);
int AdapterSetupTest (void);
int LimitedSetupTest (void);

#define  REF_PART    2
#define  REF_TEST    1
//
//    T0201
//    MotherboardSetupTest - Enable VGA access via motherboard setup registers
//
//    Entry:   None
//    Exit:    <int>    DOS ERRORLEVEL value
//
//    An IBM VGA (PS/2 motherboard) is disabled upon reset. The
//    wake-up procedure for the motherboard VGA (PS/2) is:
//       1) Put the VGA subsystem into setup mode via bit 94h.5 (= 0)
//       2) Enable the subsystem via bit 102h.0 (= 1)
//       3) Take the VGA out of setup mode (94h.5 = 1). I/O decode for
//          102h MUST not occur when the VGA is not in setup mode.
//       4) Further enable the VGA subsystem via bit 3C3h.0 (= 1)
//       5) All I/O is enabled at this point. Note that after a h/w reset
//          most registers are set to "0", including the Miscellaneous
//          Output Register (3C2h) which controls access to memory
//          and the I/O address for the CRTC.
//
int MotherboardSetupTest (void)
{
   BYTE  byTemp;
   BYTE  byOrgSetup;

   // Put the motherboard into setup mode and test reading and writing
   // the VGA enable bit at port 102h.
   byOrgSetup = (BYTE) IOByteRead (PS2_SETUP);
   IOByteWrite (PS2_SETUP, (BYTE) (byOrgSetup & 0xDF));  // Put PS/2 into setup mode
   byTemp = IsIObitFunctional (VGA_SETUP, VGA_SETUP, 0xFE);
   if (byTemp != 0)
   {
      IOByteWrite (VGA_SETUP, 0x01);                     // Enable VGA
      IOByteWrite (PS2_SETUP, byOrgSetup);               // Get out of setup mode
      return (FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, VGA_SETUP, 0x00, 0x00, byTemp));
   }

   // Take the motherboard out of setup mode and test reading and writing
   // port 102h. Access to 102h should be disabled at this point.
   IOByteWrite (PS2_SETUP, (BYTE) (byOrgSetup | 0x28));
   byTemp = IsIObitFunctional (VGA_SETUP, VGA_SETUP, 0x00);
   if ((byTemp & 0xFF) == 0)
   {
      return (FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, VGA_SETUP, 0x00, 0xFF, byTemp));
   }

   // Test read/writability of 3C3h
   byTemp = IsIObitFunctional (MB_ENABLE, MB_ENABLE, 0xFE);
   if (byTemp != 0)
   {
      IOByteWrite (MB_ENABLE, 0x01);                     // Enable the VGA
      return (FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, MB_ENABLE, 0x00, 0, byTemp));
   }

   IOByteWrite (MB_ENABLE, 0x00);                        // Disable the VGA
   if (IsVGAEnabled ())
      return (FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0));

   IOByteWrite (MB_ENABLE, 0x01);                        // Enable the VGA
   if (!IsVGAEnabled ())
      return (FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0));

   return (ERROR_NONE);
}

#undef   REF_TEST
#define  REF_TEST    2
//
//    T0202
//    AdapterSetupTest - Enable VGA access via adapter setup registers
//
//    Entry:   None
//    Exit:    <int>    DOS ERRORLEVEL value
//
//    The IBM VGA adapter card is disabled upon hardware reset. The
//    wake-up procedure for the adapter card is:
//       1) Place the VGA into setup mode via 46E8h.4 (= 1). Note that
//          46E8h is write only on the standard VGA and on most clones.
//       2) Enable the subsystem via bit 102h.0 (= 1)
//       3) Take the VGA out of setup mode (46E8h.4 = 0) and enable the
//          VGA subsystem (46E8h.3 = 1). Note that when writing to
//          46E8h, the low order three bits are the ROM bank bits and
//          should be set to 110b (06h). Most clones do not implement
//          the ROM bank "feature".
//       4) All I/O is enabled at this point. Note that after a h/w reset
//          most registers are set to "0", including the Miscellaneous
//          Output Register (3C2h) which controls access to memory
//          and the I/O address for the CRTC.
//
int AdapterSetupTest (void)
{
   BYTE  temp;

   // Put the adapter into setup mode and test reading and writing
   // the VGA enable bit at port 102h.
   IOByteWrite (ADAPTER_ENABLE, 0x16);                // Put VGA into setup mode
   IOByteWrite (VGA_SETUP, 0x01);                     // Enable the VGA
   temp = IsIObitFunctional (VGA_SETUP, VGA_SETUP, 0xFE);
   if (temp != 0)
   {
      IOByteWrite (VGA_SETUP, 0x01);                  // Enable VGA
      IOByteWrite (ADAPTER_ENABLE, 0x0E);             // Get out of setup mode
      return (FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, VGA_SETUP, 0x00, 0x00, temp));
   }

   // Take the board out of setup mode and test reading and writing port 102h.
   // Access to 102h should be disabled at this point.
   IOByteWrite (ADAPTER_ENABLE, 0x0E);                // Get out of setup mode
   temp = IsIObitFunctional (VGA_SETUP, VGA_SETUP, 0x00);
   if ((temp & 0xFF) == 0)
   {
      return (FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, VGA_SETUP, 0x00, 0xFF, temp));
   }

   IOByteWrite (ADAPTER_ENABLE, 0x06);                // Disable the VGA
   if (IsVGAEnabled ())
      return (FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0));

   IOByteWrite (ADAPTER_ENABLE, 0x0E);                // Enable the VGA
   if (!IsVGAEnabled ())
      return (FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0));

   return (ERROR_NONE);
}

#undef   REF_TEST
#define  REF_TEST    3
//
//    T0203
//    LimitedSetupTest - Enable VGA access via setup register 3C3h
//
//    Entry:   None
//    Exit:    <int>    DOS ERRORLEVEL value
//
//    PCI & AGP subsystems already have a setup sequence necessary to
//    wake up any adapter (initialize addresses, enable I/O, etc.).
//    Therefore, the extensive "setup mode" and other things that the
//    original VGA had to do is not only outmoded, but causes
//    compatibility with initialization on the newer systems.
//    Furthermore, the AGP subsystem (namely, the "bridge") does NOT
//    pass through the I/O addresses of 46E8h and 102h -- thus making
//    a compatible VGA startup sequence IMPOSSIBLE!
//
//    This test will enable and disable the VGA via 3C3h and verify
//    that I/O has indeed been enabled/disabled in the proper manner.
//
int LimitedSetupTest (void)
{
   BYTE  byTemp;

   if (SimGetType () & SIM_SIMULATION)
   {
      // The VGA simulator still does the original wakeup sequence,
      // so enable the VGA first (a mode set does this in the other
      // tests). Note that since this is simulation, the other setup
      // bits in the PS2_SETUP register are meaningless.
      IOByteWrite (PS2_SETUP, 0);               // VGA into setup mode
      IOByteWrite (VGA_SETUP, 0x01);            // Enable VGA
      IOByteWrite (PS2_SETUP, 0xFF);            // VGA out of setup mode
   }

   // Test read/writability of 3C3h
   byTemp = IsIObitFunctional (MB_ENABLE, MB_ENABLE, 0xFE);
   if (byTemp != 0)
   {
      IOByteWrite (MB_ENABLE, 0x01);                     // Enable the VGA
      return (FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, MB_ENABLE, 0x00, 0, byTemp));
   }

   IOByteWrite (MB_ENABLE, 0x00);                        // Disable the VGA
   if (IsVGAEnabled ())
      return (FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0));

   IOByteWrite (MB_ENABLE, 0x01);                        // Enable the VGA
   if (!IsVGAEnabled ())
      return (FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0));

   return (ERROR_NONE);
}

#ifdef LW_MODS
}
#endif

//
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
