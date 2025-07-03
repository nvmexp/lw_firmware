/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2000-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#ifndef INCLUDED_OVEN_H
#define INCLUDED_OVEN_H

#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

static const UINT32 MAX_PACKET_SIZE = 25;

// this is (mostly) an abstract base class
class Oven
{
   public:
      virtual ~Oven() { /* empty */ };

      virtual RC GetTemperature(FLOAT64 *Temperature)      = 0;
      virtual RC SetSetPoint(FLOAT64 Temperature)          = 0;
      virtual RC GetSetPoint(FLOAT64 *Temperature)         = 0;
      virtual RC Initialize(UINT32 Port)                   = 0;
      virtual RC Uninitialize()                            = 0;
      virtual RC SetFan(bool FanControl)                   = 0;
      virtual RC GetFan(bool *Value)                       = 0;
      virtual RC WriteReg(UINT32 Register, UINT32 Value)   = 0;
      virtual RC ReadReg(UINT32 Register, UINT32 *Value)   = 0;

      RC Soak(FLOAT64 Temperature, UINT32 Minutes);

      Oven();

   protected:
      bool   IsOvenInitialized;
      Serial *pCom;
};

// these are the "big" ovens in the oven rooms
class TestEquity : public Oven
{
   public:
      virtual RC GetTemperature(FLOAT64 *Temperature);
      virtual RC SetSetPoint(FLOAT64 Temperature);
      virtual RC GetSetPoint(FLOAT64 *Temperature);
      virtual RC Initialize(UINT32 Port);
      virtual RC Uninitialize();
      virtual RC SetFan(bool FanControl);
      virtual RC GetFan(bool *Value);
      virtual RC WriteReg(UINT32 Register, UINT32 Value);
      virtual RC ReadReg(UINT32 Register, UINT32 *Value);

   private:

      RC     CheckCrc(unsigned char *Packet, UINT32 PacketLength);
      void   AppendCrc(unsigned char *Packet, UINT32 *PacketLength);
      void   PrintPacket(unsigned char *Packet, UINT32 PacketLength);
      UINT32 CallwlateCrc(unsigned char *Packet, UINT32 PacketLength);
};

// these are the "small" microwave-size ovens
class Lw5C7: public Oven
{
   public:
      virtual RC GetTemperature(FLOAT64 *Temperature);
      virtual RC SetSetPoint(FLOAT64 Temperature);
      virtual RC GetSetPoint(FLOAT64 *Temperature);
      virtual RC Initialize(UINT32 Port);
      virtual RC Uninitialize();
      virtual RC SetFan(bool FanControl);
      virtual RC GetFan(bool *Value);
      virtual RC WriteReg(UINT32 Register, UINT32 Value);
      virtual RC ReadReg(UINT32 Register, UINT32 *Value);

   private:
      RC     WriteReg(const UINT08 *Register, UINT32 Value);
      RC     CheckChecksum(bool ComparePackets);
      void   AppendChecksum();
      void   PrintPacket(UINT32 PacketType);
      RC     ReadReg(const UINT08 *Register, UINT32 *Value);
      UINT32 CallwlateChecksum(UINT32 PacketType);
      UINT32 ColwertCharToInt(UINT32 in);
      UINT32 ColwertIntToChar(UINT32 in);

      UINT32 SendPacket[MAX_PACKET_SIZE];
      UINT32 ReceivePacket[MAX_PACKET_SIZE];
};

// these are the "Giant Force" ovens in Shenzhen lab
class GF9700: public Oven
{
   public:
      virtual RC GetTemperature(FLOAT64 *Temperature);
      virtual RC SetSetPoint(FLOAT64 Temperature);
      virtual RC GetSetPoint(FLOAT64 *Temperature);
      virtual RC Initialize(UINT32 Port);
      virtual RC Uninitialize();
      virtual RC SetFan(bool FanControl);    //dummy
      virtual RC GetFan(bool *Value);   //dummy
      virtual RC WriteReg(UINT32 Register, UINT32 Value);   //dummy
      virtual RC ReadReg(UINT32 Register, UINT32 *Value);   //dummy

   private:
      RC     GetOvenClock(char * Date);   //used in Initialize()
      UINT08 CalcFCS(UINT32 PacketLength);   //callwlate the XOR sum of data in SendBuffer.
      RC     CheckFCS(UINT32 PacketLength);   //check the XOR sum of data in ReceiveBuffer.
      UINT08 Hex2ASCII(UINT08 HexValue);
      UINT08 ASCII2Hex(UINT08 AsciiValue);
      UINT32 Rcv2Buffer();   //receive data from Com to ReceiveBuffer
      RC     SendPackage(UINT32 DataLength);   //append FCS and tail to package before send it
      RC     GetOvenTempeStatus(FLOAT64 *Data, UINT08 DataType);
      void   PrintBuffer(UINT32 Type);

      UINT08 SendBuffer[MAX_PACKET_SIZE * 4];
      UINT08 ReceiveBuffer[MAX_PACKET_SIZE * 4];
};

#endif // ! INCLUDED_OVEN_H
