/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "core/include/cpu.h"
#include <map>

namespace JsIrq
{
#ifdef LWCPU_FAMILY_ARM
   const UINT32 MAX_IRQS = 256;
#else
   const UINT32 MAX_IRQS = 16;
#endif

   const UINT32 MAX_MSILINES = 0x100;

#ifdef LWCPU_FAMILY_ARM
   static std::map<long, JSFunction*> s_IrqIsrMap;
#else
   static JSFunction * f_irq[MAX_IRQS] = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
#endif

   static JSFunction * f_msi[MAX_MSILINES] = {0};

   static UINT32 d_IdtBase = 0;
   static UINT32 d_IdtLimit = 0;

   static long DispatchIrqs(long Irq);

   static long Irq00Isr(void*) { return DispatchIrqs(0x00); }
   static long Irq01Isr(void*) { return DispatchIrqs(0x01); }
   static long Irq02Isr(void*) { return DispatchIrqs(0x02); }
   static long Irq03Isr(void*) { return DispatchIrqs(0x03); }
   static long Irq04Isr(void*) { return DispatchIrqs(0x04); }
   static long Irq05Isr(void*) { return DispatchIrqs(0x05); }
   static long Irq06Isr(void*) { return DispatchIrqs(0x06); }
   static long Irq07Isr(void*) { return DispatchIrqs(0x07); }
   static long Irq08Isr(void*) { return DispatchIrqs(0x08); }
   static long Irq09Isr(void*) { return DispatchIrqs(0x09); }
   static long Irq0aIsr(void*) { return DispatchIrqs(0x0a); }
   static long Irq0bIsr(void*) { return DispatchIrqs(0x0b); }
   static long Irq0cIsr(void*) { return DispatchIrqs(0x0c); }
   static long Irq0dIsr(void*) { return DispatchIrqs(0x0d); }
   static long Irq0eIsr(void*) { return DispatchIrqs(0x0e); }
   static long Irq0fIsr(void*) { return DispatchIrqs(0x0f); }

#ifdef LWCPU_FAMILY_ARM
   static long Irq10Isr(void*) { return DispatchIrqs(0x10); }
   static long Irq11Isr(void*) { return DispatchIrqs(0x11); }
   static long Irq12Isr(void*) { return DispatchIrqs(0x12); }
   static long Irq13Isr(void*) { return DispatchIrqs(0x13); }
   static long Irq14Isr(void*) { return DispatchIrqs(0x14); }
   static long Irq15Isr(void*) { return DispatchIrqs(0x15); }
   static long Irq16Isr(void*) { return DispatchIrqs(0x16); }
   static long Irq17Isr(void*) { return DispatchIrqs(0x17); }
   static long Irq18Isr(void*) { return DispatchIrqs(0x18); }
   static long Irq19Isr(void*) { return DispatchIrqs(0x19); }
   static long Irq1aIsr(void*) { return DispatchIrqs(0x1a); }
   static long Irq1bIsr(void*) { return DispatchIrqs(0x1b); }
   static long Irq1cIsr(void*) { return DispatchIrqs(0x1c); }
   static long Irq1dIsr(void*) { return DispatchIrqs(0x1d); }
   static long Irq1eIsr(void*) { return DispatchIrqs(0x1e); }
   static long Irq1fIsr(void*) { return DispatchIrqs(0x1f); }

   static long Irq20Isr(void*) { return DispatchIrqs(0x20); }
   static long Irq21Isr(void*) { return DispatchIrqs(0x21); }
   static long Irq22Isr(void*) { return DispatchIrqs(0x22); }
   static long Irq23Isr(void*) { return DispatchIrqs(0x23); }
   static long Irq24Isr(void*) { return DispatchIrqs(0x24); }
   static long Irq25Isr(void*) { return DispatchIrqs(0x25); }
   static long Irq26Isr(void*) { return DispatchIrqs(0x26); }
   static long Irq27Isr(void*) { return DispatchIrqs(0x27); }
   static long Irq28Isr(void*) { return DispatchIrqs(0x28); }
   static long Irq29Isr(void*) { return DispatchIrqs(0x29); }
   static long Irq2aIsr(void*) { return DispatchIrqs(0x2a); }
   static long Irq2bIsr(void*) { return DispatchIrqs(0x2b); }
   static long Irq2cIsr(void*) { return DispatchIrqs(0x2c); }
   static long Irq2dIsr(void*) { return DispatchIrqs(0x2d); }
   static long Irq2eIsr(void*) { return DispatchIrqs(0x2e); }
   static long Irq2fIsr(void*) { return DispatchIrqs(0x2f); }

   static long Irq30Isr(void*) { return DispatchIrqs(0x30); }
   static long Irq31Isr(void*) { return DispatchIrqs(0x31); }
   static long Irq32Isr(void*) { return DispatchIrqs(0x32); }
   static long Irq33Isr(void*) { return DispatchIrqs(0x33); }
   static long Irq34Isr(void*) { return DispatchIrqs(0x34); }
   static long Irq35Isr(void*) { return DispatchIrqs(0x35); }
   static long Irq36Isr(void*) { return DispatchIrqs(0x36); }
   static long Irq37Isr(void*) { return DispatchIrqs(0x37); }
   static long Irq38Isr(void*) { return DispatchIrqs(0x38); }
   static long Irq39Isr(void*) { return DispatchIrqs(0x39); }
   static long Irq3aIsr(void*) { return DispatchIrqs(0x3a); }
   static long Irq3bIsr(void*) { return DispatchIrqs(0x3b); }
   static long Irq3cIsr(void*) { return DispatchIrqs(0x3c); }
   static long Irq3dIsr(void*) { return DispatchIrqs(0x3d); }
   static long Irq3eIsr(void*) { return DispatchIrqs(0x3e); }
   static long Irq3fIsr(void*) { return DispatchIrqs(0x3f); }

   static long Irq40Isr(void*) { return DispatchIrqs(0x40); }
   static long Irq41Isr(void*) { return DispatchIrqs(0x41); }
   static long Irq42Isr(void*) { return DispatchIrqs(0x42); }
   static long Irq43Isr(void*) { return DispatchIrqs(0x43); }
   static long Irq44Isr(void*) { return DispatchIrqs(0x44); }
   static long Irq45Isr(void*) { return DispatchIrqs(0x45); }
   static long Irq46Isr(void*) { return DispatchIrqs(0x46); }
   static long Irq47Isr(void*) { return DispatchIrqs(0x47); }
   static long Irq48Isr(void*) { return DispatchIrqs(0x48); }
   static long Irq49Isr(void*) { return DispatchIrqs(0x49); }
   static long Irq4aIsr(void*) { return DispatchIrqs(0x4a); }
   static long Irq4bIsr(void*) { return DispatchIrqs(0x4b); }
   static long Irq4cIsr(void*) { return DispatchIrqs(0x4c); }
   static long Irq4dIsr(void*) { return DispatchIrqs(0x4d); }
   static long Irq4eIsr(void*) { return DispatchIrqs(0x4e); }
   static long Irq4fIsr(void*) { return DispatchIrqs(0x4f); }

   static long Irq50Isr(void*) { return DispatchIrqs(0x50); }
   static long Irq51Isr(void*) { return DispatchIrqs(0x51); }
   static long Irq52Isr(void*) { return DispatchIrqs(0x52); }
   static long Irq53Isr(void*) { return DispatchIrqs(0x53); }
   static long Irq54Isr(void*) { return DispatchIrqs(0x54); }
   static long Irq55Isr(void*) { return DispatchIrqs(0x55); }
   static long Irq56Isr(void*) { return DispatchIrqs(0x56); }
   static long Irq57Isr(void*) { return DispatchIrqs(0x57); }
   static long Irq58Isr(void*) { return DispatchIrqs(0x58); }
   static long Irq59Isr(void*) { return DispatchIrqs(0x59); }
   static long Irq5aIsr(void*) { return DispatchIrqs(0x5a); }
   static long Irq5bIsr(void*) { return DispatchIrqs(0x5b); }
   static long Irq5cIsr(void*) { return DispatchIrqs(0x5c); }
   static long Irq5dIsr(void*) { return DispatchIrqs(0x5d); }
   static long Irq5eIsr(void*) { return DispatchIrqs(0x5e); }
   static long Irq5fIsr(void*) { return DispatchIrqs(0x5f); }

   static long Irq60Isr(void*) { return DispatchIrqs(0x60); }
   static long Irq61Isr(void*) { return DispatchIrqs(0x61); }
   static long Irq62Isr(void*) { return DispatchIrqs(0x62); }
   static long Irq63Isr(void*) { return DispatchIrqs(0x63); }
   static long Irq64Isr(void*) { return DispatchIrqs(0x64); }
   static long Irq65Isr(void*) { return DispatchIrqs(0x65); }
   static long Irq66Isr(void*) { return DispatchIrqs(0x66); }
   static long Irq67Isr(void*) { return DispatchIrqs(0x67); }
   static long Irq68Isr(void*) { return DispatchIrqs(0x68); }
   static long Irq69Isr(void*) { return DispatchIrqs(0x69); }
   static long Irq6aIsr(void*) { return DispatchIrqs(0x6a); }
   static long Irq6bIsr(void*) { return DispatchIrqs(0x6b); }
   static long Irq6cIsr(void*) { return DispatchIrqs(0x6c); }
   static long Irq6dIsr(void*) { return DispatchIrqs(0x6d); }
   static long Irq6eIsr(void*) { return DispatchIrqs(0x6e); }
   static long Irq6fIsr(void*) { return DispatchIrqs(0x6f); }

   static long Irq70Isr(void*) { return DispatchIrqs(0x70); }
   static long Irq71Isr(void*) { return DispatchIrqs(0x71); }
   static long Irq72Isr(void*) { return DispatchIrqs(0x72); }
   static long Irq73Isr(void*) { return DispatchIrqs(0x73); }
   static long Irq74Isr(void*) { return DispatchIrqs(0x74); }
   static long Irq75Isr(void*) { return DispatchIrqs(0x75); }
   static long Irq76Isr(void*) { return DispatchIrqs(0x76); }
   static long Irq77Isr(void*) { return DispatchIrqs(0x77); }
   static long Irq78Isr(void*) { return DispatchIrqs(0x78); }
   static long Irq79Isr(void*) { return DispatchIrqs(0x79); }
   static long Irq7aIsr(void*) { return DispatchIrqs(0x7a); }
   static long Irq7bIsr(void*) { return DispatchIrqs(0x7b); }
   static long Irq7cIsr(void*) { return DispatchIrqs(0x7c); }
   static long Irq7dIsr(void*) { return DispatchIrqs(0x7d); }
   static long Irq7eIsr(void*) { return DispatchIrqs(0x7e); }
   static long Irq7fIsr(void*) { return DispatchIrqs(0x7f); }

   static long Irq80Isr(void*) { return DispatchIrqs(0x80); }
   static long Irq81Isr(void*) { return DispatchIrqs(0x81); }
   static long Irq82Isr(void*) { return DispatchIrqs(0x82); }
   static long Irq83Isr(void*) { return DispatchIrqs(0x83); }
   static long Irq84Isr(void*) { return DispatchIrqs(0x84); }
   static long Irq85Isr(void*) { return DispatchIrqs(0x85); }
   static long Irq86Isr(void*) { return DispatchIrqs(0x86); }
   static long Irq87Isr(void*) { return DispatchIrqs(0x87); }
   static long Irq88Isr(void*) { return DispatchIrqs(0x88); }
   static long Irq89Isr(void*) { return DispatchIrqs(0x89); }
   static long Irq8aIsr(void*) { return DispatchIrqs(0x8a); }
   static long Irq8bIsr(void*) { return DispatchIrqs(0x8b); }
   static long Irq8cIsr(void*) { return DispatchIrqs(0x8c); }
   static long Irq8dIsr(void*) { return DispatchIrqs(0x8d); }
   static long Irq8eIsr(void*) { return DispatchIrqs(0x8e); }
   static long Irq8fIsr(void*) { return DispatchIrqs(0x8f); }

   static long Irq90Isr(void*) { return DispatchIrqs(0x90); }
   static long Irq91Isr(void*) { return DispatchIrqs(0x91); }
   static long Irq92Isr(void*) { return DispatchIrqs(0x92); }
   static long Irq93Isr(void*) { return DispatchIrqs(0x93); }
   static long Irq94Isr(void*) { return DispatchIrqs(0x94); }
   static long Irq95Isr(void*) { return DispatchIrqs(0x95); }
   static long Irq96Isr(void*) { return DispatchIrqs(0x96); }
   static long Irq97Isr(void*) { return DispatchIrqs(0x97); }
   static long Irq98Isr(void*) { return DispatchIrqs(0x98); }
   static long Irq99Isr(void*) { return DispatchIrqs(0x99); }
   static long Irq9aIsr(void*) { return DispatchIrqs(0x9a); }
   static long Irq9bIsr(void*) { return DispatchIrqs(0x9b); }
   static long Irq9cIsr(void*) { return DispatchIrqs(0x9c); }
   static long Irq9dIsr(void*) { return DispatchIrqs(0x9d); }
   static long Irq9eIsr(void*) { return DispatchIrqs(0x9e); }
   static long Irq9fIsr(void*) { return DispatchIrqs(0x9f); }

   static long Irqa0Isr(void*) { return DispatchIrqs(0xa0); }
   static long Irqa1Isr(void*) { return DispatchIrqs(0xa1); }
   static long Irqa2Isr(void*) { return DispatchIrqs(0xa2); }
   static long Irqa3Isr(void*) { return DispatchIrqs(0xa3); }
   static long Irqa4Isr(void*) { return DispatchIrqs(0xa4); }
   static long Irqa5Isr(void*) { return DispatchIrqs(0xa5); }
   static long Irqa6Isr(void*) { return DispatchIrqs(0xa6); }
   static long Irqa7Isr(void*) { return DispatchIrqs(0xa7); }
   static long Irqa8Isr(void*) { return DispatchIrqs(0xa8); }
   static long Irqa9Isr(void*) { return DispatchIrqs(0xa9); }
   static long IrqaaIsr(void*) { return DispatchIrqs(0xaa); }
   static long IrqabIsr(void*) { return DispatchIrqs(0xab); }
   static long IrqacIsr(void*) { return DispatchIrqs(0xac); }
   static long IrqadIsr(void*) { return DispatchIrqs(0xad); }
   static long IrqaeIsr(void*) { return DispatchIrqs(0xae); }
   static long IrqafIsr(void*) { return DispatchIrqs(0xaf); }

   static long Irqb0Isr(void*) { return DispatchIrqs(0xb0); }
   static long Irqb1Isr(void*) { return DispatchIrqs(0xb1); }
   static long Irqb2Isr(void*) { return DispatchIrqs(0xb2); }
   static long Irqb3Isr(void*) { return DispatchIrqs(0xb3); }
   static long Irqb4Isr(void*) { return DispatchIrqs(0xb4); }
   static long Irqb5Isr(void*) { return DispatchIrqs(0xb5); }
   static long Irqb6Isr(void*) { return DispatchIrqs(0xb6); }
   static long Irqb7Isr(void*) { return DispatchIrqs(0xb7); }
   static long Irqb8Isr(void*) { return DispatchIrqs(0xb8); }
   static long Irqb9Isr(void*) { return DispatchIrqs(0xb9); }
   static long IrqbaIsr(void*) { return DispatchIrqs(0xba); }
   static long IrqbbIsr(void*) { return DispatchIrqs(0xbb); }
   static long IrqbcIsr(void*) { return DispatchIrqs(0xbc); }
   static long IrqbdIsr(void*) { return DispatchIrqs(0xbd); }
   static long IrqbeIsr(void*) { return DispatchIrqs(0xbe); }
   static long IrqbfIsr(void*) { return DispatchIrqs(0xbf); }

   static long Irqc0Isr(void*) { return DispatchIrqs(0xc0); }
   static long Irqc1Isr(void*) { return DispatchIrqs(0xc1); }
   static long Irqc2Isr(void*) { return DispatchIrqs(0xc2); }
   static long Irqc3Isr(void*) { return DispatchIrqs(0xc3); }
   static long Irqc4Isr(void*) { return DispatchIrqs(0xc4); }
   static long Irqc5Isr(void*) { return DispatchIrqs(0xc5); }
   static long Irqc6Isr(void*) { return DispatchIrqs(0xc6); }
   static long Irqc7Isr(void*) { return DispatchIrqs(0xc7); }
   static long Irqc8Isr(void*) { return DispatchIrqs(0xc8); }
   static long Irqc9Isr(void*) { return DispatchIrqs(0xc9); }
   static long IrqcaIsr(void*) { return DispatchIrqs(0xca); }
   static long IrqcbIsr(void*) { return DispatchIrqs(0xcb); }
   static long IrqccIsr(void*) { return DispatchIrqs(0xcc); }
   static long IrqcdIsr(void*) { return DispatchIrqs(0xcd); }
   static long IrqceIsr(void*) { return DispatchIrqs(0xce); }
   static long IrqcfIsr(void*) { return DispatchIrqs(0xcf); }

   static long Irqd0Isr(void*) { return DispatchIrqs(0xd0); }
   static long Irqd1Isr(void*) { return DispatchIrqs(0xd1); }
   static long Irqd2Isr(void*) { return DispatchIrqs(0xd2); }
   static long Irqd3Isr(void*) { return DispatchIrqs(0xd3); }
   static long Irqd4Isr(void*) { return DispatchIrqs(0xd4); }
   static long Irqd5Isr(void*) { return DispatchIrqs(0xd5); }
   static long Irqd6Isr(void*) { return DispatchIrqs(0xd6); }
   static long Irqd7Isr(void*) { return DispatchIrqs(0xd7); }
   static long Irqd8Isr(void*) { return DispatchIrqs(0xd8); }
   static long Irqd9Isr(void*) { return DispatchIrqs(0xd9); }
   static long IrqdaIsr(void*) { return DispatchIrqs(0xda); }
   static long IrqdbIsr(void*) { return DispatchIrqs(0xdb); }
   static long IrqdcIsr(void*) { return DispatchIrqs(0xdc); }
   static long IrqddIsr(void*) { return DispatchIrqs(0xdd); }
   static long IrqdeIsr(void*) { return DispatchIrqs(0xde); }
   static long IrqdfIsr(void*) { return DispatchIrqs(0xdf); }

   static long Irqe0Isr(void*) { return DispatchIrqs(0xe0); }
   static long Irqe1Isr(void*) { return DispatchIrqs(0xe1); }
   static long Irqe2Isr(void*) { return DispatchIrqs(0xe2); }
   static long Irqe3Isr(void*) { return DispatchIrqs(0xe3); }
   static long Irqe4Isr(void*) { return DispatchIrqs(0xe4); }
   static long Irqe5Isr(void*) { return DispatchIrqs(0xe5); }
   static long Irqe6Isr(void*) { return DispatchIrqs(0xe6); }
   static long Irqe7Isr(void*) { return DispatchIrqs(0xe7); }
   static long Irqe8Isr(void*) { return DispatchIrqs(0xe8); }
   static long Irqe9Isr(void*) { return DispatchIrqs(0xe9); }
   static long IrqeaIsr(void*) { return DispatchIrqs(0xea); }
   static long IrqebIsr(void*) { return DispatchIrqs(0xeb); }
   static long IrqecIsr(void*) { return DispatchIrqs(0xec); }
   static long IrqedIsr(void*) { return DispatchIrqs(0xed); }
   static long IrqeeIsr(void*) { return DispatchIrqs(0xee); }
   static long IrqefIsr(void*) { return DispatchIrqs(0xef); }

   static long Irqf0Isr(void*) { return DispatchIrqs(0xf0); }
   static long Irqf1Isr(void*) { return DispatchIrqs(0xf1); }
   static long Irqf2Isr(void*) { return DispatchIrqs(0xf2); }
   static long Irqf3Isr(void*) { return DispatchIrqs(0xf3); }
   static long Irqf4Isr(void*) { return DispatchIrqs(0xf4); }
   static long Irqf5Isr(void*) { return DispatchIrqs(0xf5); }
   static long Irqf6Isr(void*) { return DispatchIrqs(0xf6); }
   static long Irqf7Isr(void*) { return DispatchIrqs(0xf7); }
   static long Irqf8Isr(void*) { return DispatchIrqs(0xf8); }
   static long Irqf9Isr(void*) { return DispatchIrqs(0xf9); }
   static long IrqfaIsr(void*) { return DispatchIrqs(0xfa); }
   static long IrqfbIsr(void*) { return DispatchIrqs(0xfb); }
   static long IrqfcIsr(void*) { return DispatchIrqs(0xfc); }
   static long IrqfdIsr(void*) { return DispatchIrqs(0xfd); }
   static long IrqfeIsr(void*) { return DispatchIrqs(0xfe); }
   static long IrqffIsr(void*) { return DispatchIrqs(0xff); }
#endif

   typedef long (*DispatchTable)(void*);

   static DispatchTable IsrFunctions[MAX_IRQS] =
   {
      Irq00Isr, Irq01Isr, Irq02Isr, Irq03Isr,
      Irq04Isr, Irq05Isr, Irq06Isr, Irq07Isr,
      Irq08Isr, Irq09Isr, Irq0aIsr, Irq0bIsr,
      Irq0cIsr, Irq0dIsr, Irq0eIsr, Irq0fIsr,

   #ifdef LWCPU_FAMILY_ARM
      Irq10Isr, Irq11Isr, Irq12Isr, Irq13Isr,
      Irq14Isr, Irq15Isr, Irq16Isr, Irq17Isr,
      Irq18Isr, Irq19Isr, Irq1aIsr, Irq1bIsr,
      Irq1cIsr, Irq1dIsr, Irq1eIsr, Irq1fIsr,

      Irq20Isr, Irq21Isr, Irq22Isr, Irq23Isr,
      Irq24Isr, Irq25Isr, Irq26Isr, Irq27Isr,
      Irq28Isr, Irq29Isr, Irq2aIsr, Irq2bIsr,
      Irq2cIsr, Irq2dIsr, Irq2eIsr, Irq2fIsr,

      Irq30Isr, Irq31Isr, Irq32Isr, Irq33Isr,
      Irq34Isr, Irq35Isr, Irq36Isr, Irq37Isr,
      Irq38Isr, Irq39Isr, Irq3aIsr, Irq3bIsr,
      Irq3cIsr, Irq3dIsr, Irq3eIsr, Irq3fIsr,

      Irq40Isr, Irq41Isr, Irq42Isr, Irq43Isr,
      Irq44Isr, Irq45Isr, Irq46Isr, Irq47Isr,
      Irq48Isr, Irq49Isr, Irq4aIsr, Irq4bIsr,
      Irq4cIsr, Irq4dIsr, Irq4eIsr, Irq4fIsr,

      Irq50Isr, Irq51Isr, Irq52Isr, Irq53Isr,
      Irq54Isr, Irq55Isr, Irq56Isr, Irq57Isr,
      Irq58Isr, Irq59Isr, Irq5aIsr, Irq5bIsr,
      Irq5cIsr, Irq5dIsr, Irq5eIsr, Irq5fIsr,

      Irq60Isr, Irq61Isr, Irq62Isr, Irq63Isr,
      Irq64Isr, Irq65Isr, Irq66Isr, Irq67Isr,
      Irq68Isr, Irq69Isr, Irq6aIsr, Irq6bIsr,
      Irq6cIsr, Irq6dIsr, Irq6eIsr, Irq6fIsr,

      Irq70Isr, Irq71Isr, Irq72Isr, Irq73Isr,
      Irq74Isr, Irq75Isr, Irq76Isr, Irq77Isr,
      Irq78Isr, Irq79Isr, Irq7aIsr, Irq7bIsr,
      Irq7cIsr, Irq7dIsr, Irq7eIsr, Irq7fIsr,

      Irq80Isr, Irq81Isr, Irq82Isr, Irq83Isr,
      Irq84Isr, Irq85Isr, Irq86Isr, Irq87Isr,
      Irq88Isr, Irq89Isr, Irq8aIsr, Irq8bIsr,
      Irq8cIsr, Irq8dIsr, Irq8eIsr, Irq8fIsr,

      Irq90Isr, Irq91Isr, Irq92Isr, Irq93Isr,
      Irq94Isr, Irq95Isr, Irq96Isr, Irq97Isr,
      Irq98Isr, Irq99Isr, Irq9aIsr, Irq9bIsr,
      Irq9cIsr, Irq9dIsr, Irq9eIsr, Irq9fIsr,

      Irqa0Isr, Irqa1Isr, Irqa2Isr, Irqa3Isr,
      Irqa4Isr, Irqa5Isr, Irqa6Isr, Irqa7Isr,
      Irqa8Isr, Irqa9Isr, IrqaaIsr, IrqabIsr,
      IrqacIsr, IrqadIsr, IrqaeIsr, IrqafIsr,

      Irqb0Isr, Irqb1Isr, Irqb2Isr, Irqb3Isr,
      Irqb4Isr, Irqb5Isr, Irqb6Isr, Irqb7Isr,
      Irqb8Isr, Irqb9Isr, IrqbaIsr, IrqbbIsr,
      IrqbcIsr, IrqbdIsr, IrqbeIsr, IrqbfIsr,

      Irqc0Isr, Irqc1Isr, Irqc2Isr, Irqc3Isr,
      Irqc4Isr, Irqc5Isr, Irqc6Isr, Irqc7Isr,
      Irqc8Isr, Irqc9Isr, IrqcaIsr, IrqcbIsr,
      IrqccIsr, IrqcdIsr, IrqceIsr, IrqcfIsr,

      Irqd0Isr, Irqd1Isr, Irqd2Isr, Irqd3Isr,
      Irqd4Isr, Irqd5Isr, Irqd6Isr, Irqd7Isr,
      Irqd8Isr, Irqd9Isr, IrqdaIsr, IrqdbIsr,
      IrqdcIsr, IrqddIsr, IrqdeIsr, IrqdfIsr,

      Irqe0Isr, Irqe1Isr, Irqe2Isr, Irqe3Isr,
      Irqe4Isr, Irqe5Isr, Irqe6Isr, Irqe7Isr,
      Irqe8Isr, Irqe9Isr, IrqeaIsr, IrqebIsr,
      IrqecIsr, IrqedIsr, IrqeeIsr, IrqefIsr,

      Irqf0Isr, Irqf1Isr, Irqf2Isr, Irqf3Isr,
      Irqf4Isr, Irqf5Isr, Irqf6Isr, Irqf7Isr,
      Irqf8Isr, Irqf9Isr, IrqfaIsr, IrqfbIsr,
      IrqfcIsr, IrqfdIsr, IrqfeIsr, IrqffIsr
   #endif
   };

   typedef long (*DispatchTable)(void*);

   void DisplayIdt(UINT32 StartingIndex, UINT32 NumberEntries = 1);
}

using namespace JsIrq;

static long JsIrq::DispatchIrqs(long Irq)
{
#ifdef LWCPU_FAMILY_ARM
   MASSERT(s_IrqIsrMap[Irq]);
   if(s_IrqIsrMap[Irq])
      JavaScriptPtr()->CallMethod(s_IrqIsrMap[Irq]);
#else
   MASSERT(f_irq[Irq]);
   if(f_irq[Irq])
      JavaScriptPtr()->CallMethod(f_irq[Irq]);
#endif
   // Make sure we always call the real-mode interrupt if there is one.
   // We may way to reconsider this in the future.
   return Xp::ISR_RESULT_NOT_SERVICED;
}

void JsIrq::DisplayIdt(UINT32 StartingIndex, UINT32 NumberEntries)
{
   // Initialize d_IdtBase if not initialized yet
   if(!d_IdtBase)
   {
      Cpu::RdIdt(&d_IdtBase, &d_IdtLimit);
   }
   Printf(Tee::PriNormal, "IDT Base 0x%08x, Limit 0x%x, Descriptor:\n",
          d_IdtBase, d_IdtLimit);
   if(!d_IdtLimit)
      return;

   Printf(Tee::PriNormal, "\tIndex [PhysAddr]   Word0      Word1\t\tIVT\n");
   for(UINT32 i = StartingIndex; i<(StartingIndex+NumberEntries); i++)
   {
      Printf(Tee::PriNormal, "\t0x%02x  [0x%08x] 0x%08x 0x%08x\t0x%08x\n",
             i,
             d_IdtBase+8*i,
             Platform::PhysRd32(d_IdtBase+8*i),
             Platform::PhysRd32(d_IdtBase+8*i+4),
             Platform::PhysRd32(4*i));
   }
}

//
// Script methods and tests.
//
C_(DumpIdt);
SMethod DumpIdt
   (
   "DumpIdt",
   C_DumpIdt,
   2,
   "Display IDT contecnt."
   );

C_(HookIrq);
STest HookIrq
   (
   "HookIrq",
   C_HookIrq,
   2,
   "Hook an IRQ."
   );

C_(UnhookIrq);
STest UnhookIrq
   (
   "UnhookIrq",
   C_UnhookIrq,
   1,
   "Unhook an IRQ"
   );

C_(HookMsi);
STest HookMsi
   (
   "HookMsi",
   C_HookMsi,
   2,
   "Hook an ISR at IVT entry."
   );

C_(UnhookMsi);
STest UnhookMsi
   (
   "UnhookMsi",
   C_UnhookMsi,
   1,
   "Unhook an ISR from IVT entry"
   );

C_(PrintHookedIrqs);
STest PrintHookedIrqs
   (
   "PrintHookedIrqs",
   C_PrintHookedIrqs,
   0,
   "Print the hooked IRQs"
   );

C_(PrintHookedMsis);
STest PrintHookedMsis
   (
   "PrintHookedMsis",
   C_PrintHookedMsis,
   0,
   "Print the hooked MSIs"
   );

//
// Implimentation of methods
//
/* SMethod */
C_(DumpIdt)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   JavaScriptPtr pJavaScript;
   StickyRC rc;
   INT32 ivt = 0;
   INT32 num = 1;

   switch (NumArguments)
   {
      case 2: // optional parameter, fall-through
         rc = pJavaScript->FromJsval(pArguments[1], &num);

      case 1: // mandatory parameter
         rc = pJavaScript->FromJsval(pArguments[0], &ivt);
         break;

      default: // error condition
         JS_ReportError(pContext, "Usage: DumpIdt(ivt, *num)");
         rc = RC::BAD_PARAMETER;
         break;
   }

   if (rc == OK)
   {
      JsIrq::DisplayIdt(ivt, num);
      return true;
   }
   return JS_FALSE;

}

/* STest */
C_(HookIrq)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   JavaScriptPtr pJavaScript;

   INT32 Irq;
   JSFunction * JsFunc;

   if ( (NumArguments != 2)
        || (OK != pJavaScript->FromJsval(pArguments[0], &Irq))
        || (OK != pJavaScript->FromJsval(pArguments[1], &JsFunc)) )
   {
      JS_ReportError(pContext, "Usage: HookIrq(irq, isr)");
      return JS_FALSE;
   }

#ifndef LWCPU_FAMILY_ARM
   if ( (Irq > 15)|| (Irq <  0)|| (Irq == 2) )
#else
   if ( (Irq <  0) || (Irq >= static_cast<INT32>(MAX_IRQS)) )
#endif
   {
      Printf(Tee::PriDebug, "Invalid IRQ.\n");
      RETURN_RC(RC::BAD_PARAMETER);
   }

#ifdef LWCPU_FAMILY_ARM
   s_IrqIsrMap[Irq] = JsFunc;
#else
   f_irq[Irq] = JsFunc;
#endif

   RETURN_RC(Platform::HookIRQ(Irq, IsrFunctions[Irq], 0));
}

/* STest */
C_(UnhookIrq)
{
   INT32         Irq;
   JavaScriptPtr pJavaScript;

   if ( (NumArguments !=1)
        || (OK != pJavaScript->FromJsval(pArguments[0], &Irq)) )
   {
      JS_ReportError(pContext, "Usage: UnhookIrq(irq)");
      return JS_FALSE;
   }
#ifndef LWCPU_FAMILY_ARM
   if ( (Irq > 15)|| (Irq <  0)|| (Irq == 2) )
#else
   if ( (Irq <  0) || (Irq >= static_cast<INT32>(MAX_IRQS)) )
#endif
   {
      Printf(Tee::PriDebug, "Invalid IRQ.\n");
      RETURN_RC(RC::BAD_PARAMETER);
   }

#ifdef LWCPU_FAMILY_ARM
   s_IrqIsrMap[Irq] = 0;
#else
   f_irq[Irq] = 0;
#endif

   RETURN_RC(Platform::UnhookIRQ(Irq, IsrFunctions[Irq], 0));
}

/* STest */
C_(HookMsi)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   JS_ReportError(pContext, "Hooking MSI by message index is supported only on DOS\n");
   return JS_FALSE;
}

/* STest */
C_(UnhookMsi)
{
   JS_ReportError(pContext, "Hooking MSI by message index is supported only on DOS\n");
   return JS_FALSE;
}

/* STest */
C_(PrintHookedIrqs)
{
   UINT32 i;
   Printf(Tee::PriHigh, "Hooked IRQs\n");
   for (i = 0; i < MAX_IRQS; i++)
   {
   #ifdef LWCPU_FAMILY_ARM
       if(s_IrqIsrMap[i])
           Printf(Tee::PriHigh, "IRQ%d ", i);
   #else
       if(f_irq[i])
           Printf(Tee::PriHigh, "IRQ%d ", i);
   #endif
   }
   Printf(Tee::PriHigh, "\n");

   RETURN_RC(OK);
}

C_(PrintHookedMsis)
{
   UINT32 i;
   Printf(Tee::PriHigh, "Hooked MSI\n");
   for (i = 0; i < MAX_MSILINES; i++)
   {
      if(f_msi[i])
         Printf(Tee::PriHigh, "Msi(IVT 0x%0x) ", (i) );
   }
   Printf(Tee::PriHigh, "\n");

   RETURN_RC(OK);
}
