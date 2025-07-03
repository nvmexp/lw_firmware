 /*
  * LWIDIA_COPYRIGHT_BEGIN
  *
  * Copyright 1999-2021 by LWPU Corporation. All rights
  * reserved. All information contained herein is proprietary and confidential
  * to LWPU Corporation. Any use, reproduction, or disclosure without the
  * written permission of LWPU Corporation is prohibited.
  *
  * LWIDIA_COPYRIGHT_END
  */

// Serial JavaScript object for linux.

// define INSIDE_SERIAL_CPP to get some privileged variables in serial.h
#define INSIDE_SERIAL_CPP

#include <ctype.h>
#include <cstdio>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>

#include <string>

using namespace std;

#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tasker.h"
#include "core/include/platform.h"
#include "core/include/tee.h"

#include "core/include/serial.h"

#define ILWALID_HANDLE_VALUE 0

namespace LinuxSerial
{
   const UINT32 fixedJsPorts = 6;
   const char *fixedJsPortsNames[] = { "/dev/null", "/dev/ttyS0", "/dev/ttyS1",
                                       "/dev/ttyS2", "/dev/ttyS3", "/dev/ttyUSB0",
                                       "/dev/ttyACM0"};

   RC ReadSerialPort(UINT32 comPortNumber, string* pStr, size_t maxBytes);
   bool WaitForCharacters(UINT32 comPortNumber, int timeoutMs);
}

using namespace LinuxSerial;

namespace GetSerialObj
{
    // The implementation of the Serial class uses non virtual static interface
    // to implement communication port class for different platforms. This is
    // why the class definition in the header cannot have any platform specific
    // data members. Instead the author (cgreen) placed all platform specific
    // data in a couple of arrays, keeping a map between unique "port number"
    // and the correspondent OS specifics. His implementation allowed to exist
    // only a handful of fixed Serial class instances. The following classes and
    // functions extend this idea on arbitrary amount of Serial class instances.
    // It's a good alternative to redesigning the full class.

    // This class mixes in some OS specific additions to the Serial class to
    // avoid keeping maps between Serial instances and various OS handles.
    class SerialImpl : public Serial
    {
    public:
        SerialImpl(UINT32 port)
          : Serial(port)
        {}

        SerialImpl(const char *portName)
          : Serial(portName)
        {}

        void SetComPortId(UINT32 portId)
        {
            Serial::SetComPortId(portId);
        }

        int GetPortHandle() const { return m_hPort; }
        void SetPortHandle(int val) { m_hPort = val; }
        termios& GetDCB() { return m_SavedDCB;  }

    private:
        termios m_SavedDCB = {};
        int     m_hPort    = ILWALID_HANDLE_VALUE;
    };

    class PortHandleAssign
    {
    public:
        PortHandleAssign(SerialImpl *ser)
          : m_Serial(ser)
        {}

        void operator =(int h)
        {
            if (nullptr != m_Serial)
            {
                m_Serial->SetPortHandle(h);
            }
        }

        operator int()
        {
            if (nullptr != m_Serial)
            {
                return m_Serial->GetPortHandle();
            }
            else
            {
                return ILWALID_HANDLE_VALUE;
            }
        }

    private:
        SerialImpl *m_Serial;
    };

    class SavedDCBAccess
    {
    public:
        SavedDCBAccess(SerialImpl *ser)
          : m_Serial(ser)
        {}

        termios * operator & ()
        {
            if (nullptr != m_Serial)
            {
                return &m_Serial->GetDCB();
            }
            else
            {
                return nullptr;
            }
        }

    private:
        SerialImpl *m_Serial;
    };

    typedef vector<unique_ptr<SerialImpl>> PortsCollection;

    PortsCollection &GetPortsCollection()
    {
        static PortsCollection hwComPorts;

        return hwComPorts;
    }

    SerialImpl* FindComPort(UINT32 port)
    {
        PortsCollection &hwComPorts = GetPortsCollection();

        PortsCollection::iterator it;
        it = find_if(
            hwComPorts.begin(),
            hwComPorts.end(),
            [&](const auto& elem) { return elem->GetComPortId() == port; }
        );
        if (hwComPorts.end() == it)
        {
            return nullptr;
        }
        else
        {
            return it->get();
        }
    }

    SerialImpl* FindComPort(const char *portName)
    {
        PortsCollection &hwComPorts = GetPortsCollection();

        PortsCollection::iterator it;
        it = find_if(
            hwComPorts.begin(),
            hwComPorts.end(),
            [&](const auto& elem) { return elem->GetPortName() == portName; }
        );
        if (hwComPorts.end() == it)
        {
            return nullptr;
        }
        else
        {
            return it->get();
        }
    }

    Serial *GetCom(UINT32 port)
    {
        PortsCollection &hwComPorts = GetPortsCollection();

        Serial *p = FindComPort(port);
        if (nullptr == p)
        {
            if (1 > port || fixedJsPorts < port)
            {
                Printf(Tee::PriLow,
                       "Caller to GetCom() requested an invalid serial port\n");
                MASSERT(0);
                // return a valid pointer anyway so the caller doesn't
                // try to dereference a null;
                return GetCom(1);
            }
            hwComPorts.push_back(make_unique<SerialImpl>(port));
            p = hwComPorts.back().get();
        }
        return p;
    }

    Serial *GetCom(const char *portName)
    {
        PortsCollection &hwComPorts = GetPortsCollection();
        SerialImpl *p = FindComPort(portName);
        if (nullptr == p)
        {
            PortsCollection::iterator maxIdIt;
            maxIdIt = max_element(
                hwComPorts.begin(),
                hwComPorts.end(),
                [](const auto& lhs, const auto& rhs)
                {
                    return lhs->GetComPortId() < rhs->GetComPortId();
                }
            );
            UINT32 maxId = (*maxIdIt)->GetComPortId();
            hwComPorts.push_back(make_unique<SerialImpl>(portName));
            p = hwComPorts.back().get();
            p->SetComPortId(maxId + 1);
        }
        return p;
    }

    class PortHandles
    {
    public:
        PortHandleAssign operator[](UINT32 port)
        {
            return FindComPort(port);
        }
    };

    class SavedDCBs
    {
    public:
        SavedDCBAccess operator[](UINT32 port)
        {
            return FindComPort(port);
        }
    };
}

// these two variable and their correspondent types allow to keep the
// implementation below as is, without redesigning
GetSerialObj::PortHandles hPort;
GetSerialObj::SavedDCBs savedDCB;

//-----------------------------------------------------------------------------
// Instantiate the JavaScript interface for four serial ports
//-----------------------------------------------------------------------------
DECLARE_SERIAL(1, 1)
DECLARE_SERIAL(2, 2)
DECLARE_SERIAL(3, 3)
DECLARE_SERIAL(4, 4)
DECLARE_SERIAL(Usb0, 5)
DECLARE_SERIAL(Acm0, 6)

//-----------------------------------------------------------------------------
// Actual working routines--independent of Javascript
//-----------------------------------------------------------------------------

const int BAUDS[][2] = {
  { 0, B0 },
  { 50, B50 },
  { 75, B75 },
  { 110, B110 },
  { 134, B134 },
  { 150, B150 },
  { 200, B200 },
  { 300, B300 },
  { 600, B600 },
  { 1200, B1200 },
  { 1800, B1800 },
  { 2400, B2400 },
  { 4800, B4800 },
  { 9600, B9600 },
  { 19200, B19200 },
  { 38400, B38400 },
  { 57600, B57600 },
  { 115200, B115200 },
  { -1, -1 } };

const int DATABITS[][2] =
  { { 5, CS5 }, { 6, CS6 }, { 7, CS7 }, { 8, CS8 }, { -1, -1 } };

// helper functions
int ColwertToCode(const int base[][2], int val)
{
    int i = 0;
    while (base[i][0] != -1)
    {
        if (base[i][0] == val) return base[i][1];
        i++;
    }
    return -1;
}

int ColwertFromCode(const int base[][2], int val)
{
    int i = 0;
    while (base[i][0] != -1)
    {
        if (base[i][1] == val) return base[i][0];
        i++;
    }
    return -1;
}

void Serial::ProgramUart()
{
   struct termios dcb;

   memset(&dcb, 0, sizeof(dcb));
   if (hPort[m_ComPortId] == ILWALID_HANDLE_VALUE ||
       tcgetattr(hPort[m_ComPortId] , &dcb)) // get current DCB
   {
      Printf(Tee::PriNormal, "Error getting com attributes\n");
      return;
   }

   int status;
   if (ioctl(hPort[m_ComPortId], TIOCMGET, &status))
   {
       Printf(Tee::PriNormal, "Error: Can't get line status\n");
       return;
   }

   status |= TIOCM_DTR;
   status |= TIOCM_RTS;
   if (ioctl(hPort[m_ComPortId], TIOCMSET, &status))
   {
       Printf(Tee::PriNormal, "Error: Can't set line status\n");
       return;
   }

   // Update DCB rate.
   // dcb.c_cflag &= ~CRTSCTS;
   dcb.c_cflag |= CLOCAL | CREAD;
   // dcb.c_cflag &= ~CBAUD;
   dcb.c_cflag |= ColwertToCode(BAUDS, m_Baud);
   dcb.c_cflag &= ~CSIZE;
   dcb.c_cflag |= ColwertToCode(DATABITS, m_DataBits);
   dcb.c_cflag &= ~CSTOPB;
   dcb.c_cflag |= (1 == m_StopBits) ? 0 : CSTOPB;

   // Enable colwersion of LF to CR-LF if requested
   if (m_AddCr)
   {
      dcb.c_oflag &= ~OCRNL;
      dcb.c_oflag |= OPOST | ONLCR;
   }

   dcb.c_lflag = 0;

   dcb.c_cflag &= ~(PARENB|PARODD);
   switch (m_Parity)
   {
      case 'n': break;
      case 'o': dcb.c_cflag |= PARENB | PARODD;   break;
      case 'e': dcb.c_cflag |= PARENB;  break;
   }

   dcb.c_cc[VMIN] = 0;
   dcb.c_cc[VTIME] = 10; // 1 second

   // clean the modem line
   tcflush(hPort[m_ComPortId], TCIFLUSH);

   // Set the state.  If we got an error, just bail.  It is possibly a problem
   // with the communications port handle or a problem with the termios struct.
   if (tcsetattr(hPort[m_ComPortId] , TCSANOW, &dcb))
   {
      Printf(Tee::PriNormal, "Error setting com state\n");
      return;
   }

   // Reset buffers
   m_WriteHead = m_WriteTail = m_WriteBuffer;
   m_ReadHead  = m_ReadTail  = m_ReadBuffer;

   m_ReadOverflow  = false;
   m_WriteOverflow = false;

   struct termios dcb_comp;
   if (tcgetattr(hPort[m_ComPortId] , &dcb_comp)) // get current DCB
   {
      Printf(Tee::PriNormal, "Error getting com state.\n");
      return;
   }
   if (dcb.c_cflag != dcb_comp.c_cflag) // compare to check proper set
   {
      Printf(Tee::PriNormal, "Error setting all com state flags\n");
   }

   Printf(Tee::PriDebug, "Serial port status\n");
   Printf(Tee::PriDebug, "   BaudRate          = %08x\n",
          ColwertFromCode(BAUDS, true/*dcb_comp.c_cflag & CBAUD */));
   Printf(Tee::PriDebug, "   ByteSize          = %08x\n",
          ColwertFromCode(DATABITS, dcb_comp.c_cflag & CSIZE));
   Printf(Tee::PriDebug, "   Parity            = %c\n",
          (dcb_comp.c_cflag & PARENB) ?
          ((dcb_comp.c_cflag & PARODD) ? 'o' : 'e' ) : 'n');
   Printf(Tee::PriDebug, "   StopBits          = %08x\n",
          (dcb_comp.c_cflag & CSTOPB) ? 2 : 1);
}

//-----------------------------------------------------------------------------
// Implementation of Serial:: API
//-----------------------------------------------------------------------------
UINT32 Serial::GetHighestComPort()
{
    return fixedJsPorts;
}

Serial::Serial(UINT32 port)
{
   m_ComPortClient   = SerialConst::CLIENT_NOT_IN_USE;
   m_Baud            = 19200;
   m_DataBits        = 8;
   m_Parity          = 'n';
   m_StopBits        = 1;
   m_AddCr           = false;
   m_ComPortId       = port;

   // this constructor accepts only fixed port numbers meant to use in
   // JavaScript programs
   MASSERT(1 <= port && fixedJsPorts >= port);

   m_ComPortName = fixedJsPortsNames[port];
}

Serial::Serial(const char *portName)
  : m_ComPortName(portName)
{}

Serial::~Serial()
{
   // this routine will make sure the interrupt gets unhooked.
   Uninitialize(m_ComPortClient, true);
}

RC Serial::Initialize(UINT32 clientId, bool isSynchronous)
{
    RC rc = OK;

    if (m_ComPortClient != SerialConst::CLIENT_NOT_IN_USE)
       CHECK_RC(ValidateClient(clientId));

    struct stat statbuf;
    if ((0 == m_ComPortId) || (0 != stat(GetPortName().c_str(), &statbuf)))
    {
       Printf(Tee::PriLow,
              "Com %d not present in this system\n", m_ComPortId);
       return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    // Check permissions
    if (0 != access(GetPortName().c_str(), R_OK | W_OK))
    {
        Printf(Tee::PriNormal, "Error: Insufficient access rights for %s.\n",
               GetPortName().c_str());
        return RC::FILE_PERM;
    }

    // Open the IO port.
    hPort[m_ComPortId] = open(GetPortName().c_str(),
                              O_RDWR | O_NONBLOCK | O_NOCTTY);

    if (hPort[m_ComPortId] == ILWALID_HANDLE_VALUE)
    {
       Printf(Tee::PriNormal, "Cannot open %s (%s)\n",
              GetPortName().c_str(),
              strerror(errno));

       return RC::SERIAL_COMMUNICATION_ERROR;
    }

    // Save DCB
    if (tcgetattr(hPort[m_ComPortId], &savedDCB[m_ComPortId]))
    {
       Printf(Tee::PriError, "Unable to obtain configuration of %s (%s)\n",
              GetPortName().c_str(),
              strerror(errno));

       return RC::SERIAL_COMMUNICATION_ERROR;
    }

    // Make sure the serial port is in a known state
    ProgramUart();

    m_ComPortClient = clientId;

    Printf(Tee::PriLow, "Setting client for COM%d to %s.\n",
           m_ComPortId, SerialConst::CLIENT_STRING[m_ComPortClient]);

    return OK;
}

RC Serial::Uninitialize(UINT32 clientId, bool isDestructor)
{
   if (m_ComPortClient == SerialConst::CLIENT_NOT_IN_USE )
   {
      if (!isDestructor)
         Printf(Tee::PriLow, "Serial port already uninitialized\n");

      return OK;
   }

   Printf(Tee::PriLow, "Attempting to uninitialize serial port\n");

   // make sure that the guy who deinits the serial port is the same guy
   // that init'd it.
   RC rc = OK;
   CHECK_RC(ValidateClient(clientId));

   Printf(Tee::PriLow, "Validated client\n");

   m_ComPortClient= SerialConst::CLIENT_NOT_IN_USE;

   Printf(Tee::PriLow, "Set client for port %x to %s. (Dec=%d)\n",
          m_ComPortId, SerialConst::CLIENT_STRING[m_ComPortClient],
          m_ComPortClient);

   // Restore DCB
   tcsetattr(hPort[m_ComPortId], TCSANOW, &savedDCB[m_ComPortId]);

   if (close(hPort[m_ComPortId]))
   {

      Printf(Tee::PriNormal, "Cannot close port handle %s (%s)\n",
             GetPortName().c_str(),
             strerror(errno));
      return RC::SERIAL_COMMUNICATION_ERROR;
   }
   hPort[m_ComPortId] = ILWALID_HANDLE_VALUE;

   return OK;
}

RC Serial::SetBaud(UINT32 clientId, UINT32 value)
{
   // make sure this is the owner
   RC rc = OK;
   CHECK_RC(ValidateClient(clientId));

   int baudCode = ColwertToCode(BAUDS, value);
   if (baudCode == -1)
   {
      Printf(Tee::PriNormal, "Invalid baud: %d\n", value);
      return RC::SOFTWARE_ERROR;
   }

   m_Baud = value;
   ProgramUart();
   return OK;
}

RC Serial::SetParity(UINT32 clientId, char par)
{
   switch (par)
   {
   case 'n':
   case 'o':
   case 'e':
      break;
   default:
      Printf(Tee::PriNormal, "Valid parity is 'n' none, 'e' even, 'o' odd\n");
      return RC::SOFTWARE_ERROR;
   }
   // make sure this is the owner
   RC rc = OK;
   CHECK_RC(ValidateClient(clientId));

   m_Parity = par;
   ProgramUart();
   return OK;
}

RC Serial::SetDataBits(UINT32 clientId, UINT32 numBits)
{
   // make sure this is the owner
   RC rc = OK;
   CHECK_RC(ValidateClient(clientId));

   if (numBits < 5 || numBits > 8)
   {
      Printf(Tee::PriNormal, "Data bits must be between 5 and 8\n");
      return RC::SOFTWARE_ERROR;
   }
   m_DataBits = numBits;
   ProgramUart();
   return OK;
}

RC Serial::SetStopBits(UINT32 clientId, UINT32 value)
{
   // make sure this is the owner
   RC rc = OK;
   CHECK_RC(ValidateClient(clientId));

   if (value < 1 || value > 2)
   {
      Printf(Tee::PriNormal, "StopBits must be 1 or 2\n");
      return RC::SOFTWARE_ERROR;
   }
   m_StopBits = value;
   ProgramUart();
   return OK;
}

RC Serial::SetAddCarriageReturn(bool value)
{
   m_AddCr = value;
   return OK;
}

bool Serial::GetReadOverflow() const
{
   return m_ReadOverflow;
}

bool Serial::GetWriteOverflow() const
{
   return m_WriteOverflow;
}

bool Serial::SetReadOverflow(bool x)
{
   return true;
}

bool Serial::SetWriteOverflow(bool x)
{
   return true;
}

UINT32 Serial::GetBaud() const
{
   return m_Baud;
}

char Serial::GetParity() const
{
   return m_Parity;
}

UINT32 Serial::GetDataBits() const
{
   return m_DataBits;
}

UINT32 Serial::GetStopBits() const
{
   return m_StopBits;
}

bool Serial::GetAddCarriageReturn() const
{
   return m_AddCr;
}

UINT32 Serial::ReadBufCount()
{
   return WaitForCharacters(m_ComPortId, 0) ? 1 : 0;
}

UINT32 Serial::WriteBufCount()
{
   return 0;
}

RC Serial::ClearBuffers(UINT32 clientId)
{
   // make sure this is the owner
   RC rc = OK;
   CHECK_RC(ValidateClient(clientId));

   // set the buffer pointers to zero them out
   m_WriteHead = m_WriteTail = m_WriteBuffer;
   m_ReadHead = m_ReadTail = m_ReadBuffer;

   m_ReadOverflow  = false;
   m_WriteOverflow = false;

   return rc;
}

RC Serial::Get(UINT32 clientId, UINT32 *value)
{
    MASSERT(value);

    RC rc;
    CHECK_RC(ValidateClient(clientId));

    string s;
    CHECK_RC(ReadSerialPort(m_ComPortId, &s, 1));
    *value = s[0];
    return OK;
}

RC Serial::GetString(UINT32 clientId, string *pStr)
{
    RC rc;
    CHECK_RC(ValidateClient(clientId));

    return ReadSerialPort(m_ComPortId, pStr, 1024);
}

RC LinuxSerial::ReadSerialPort(UINT32 comPortNumber, string* pStr, size_t maxBytes)
{
    MASSERT(pStr);
    *pStr = "";

    // Wait for characters to become available for reading
    int timeLeft = 1000;
    const int stepMs = 10;
    while ((timeLeft > 0) && !WaitForCharacters(comPortNumber, stepMs))
    {
        timeLeft -= stepMs;

        // Yield to other threads
        Tasker::Yield();

        // Handle Ctrl+C
        RC rc;
        if (rc != OK)
        {
            return rc;
        }
    }
    if (timeLeft <= 0)
    {
        Printf(Tee::PriNormal, "Timeout waiting for the COM port response.\n");
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    // Receive characters
    string s;
    s.resize(maxBytes);
    const ssize_t numRead = read(hPort[comPortNumber], &s[0], maxBytes);
    if (numRead <= 0)
    {
        Printf(Tee::PriNormal,
                "Serial Port Error\n");
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    *pStr = s.substr(0, numRead);
    return OK;
}

bool LinuxSerial::WaitForCharacters(UINT32 comPortNumber, int timeoutMs)
{
    pollfd fds = { hPort[comPortNumber], POLLIN, 0 };
    return poll(&fds, 1, timeoutMs) == 1;
}

RC Serial::Put(UINT32 clientId, UINT32 outChar)
{
   // make sure this is the owner
   RC rc = OK;

   CHECK_RC(ValidateClient(clientId));

   ssize_t numOut; // Counts bytes written.

   char msg[2];
   msg[0] = (char)outChar;
   msg[1] = '\0';

   //--- Write message to port. ---
   numOut = write(hPort[m_ComPortId], msg, 1);

   if (numOut < 0)
   {
      Printf(Tee::PriNormal, "Cannot write message (%s)\n", strerror(errno));
      return RC::SERIAL_COMMUNICATION_ERROR;
   }

   return OK;
}

RC Serial::PutString(UINT32 clientId, string *pStr)
{
   return PutString( clientId, pStr->c_str() );
}

RC Serial::PutString(UINT32 clientId, const char *pStr, UINT32 length)
{
   MASSERT(pStr != 0);

   RC rc = OK;

   CHECK_RC(ValidateClient(clientId));

   if(0xDEADCAFE == length)
      length = strlen(pStr);

   ssize_t numOut; // Counts bytes written.

   //--- Write message to port. ---
   numOut = write(hPort[m_ComPortId], pStr, length);

   if (numOut < 0)
   {
      Printf(Tee::PriNormal, "Cannot write message (%s)\n",
             strerror(errno));
      return RC::SERIAL_COMMUNICATION_ERROR;
   }

   return OK;
}

RC Serial::ValidateClient(UINT32 clientId)
{
   if (clientId > SerialConst::CLIENT_HIGHESTNUM)
   {
      Printf(Tee::PriNormal,
             "Client %d is illegal\n", clientId);
      return RC::UNSUPPORTED_HARDWARE_FEATURE;
   }

    // make sure nobody is already using that port
   if (m_ComPortClient != clientId)
   {
       if (clientId != SerialConst::CLIENT_SINK)
       {
           Printf(
               Tee::PriError,
               "Client %s tried to use port %x already assigned to client %s.\n",
               SerialConst::CLIENT_STRING[clientId],
               m_ComPortId,
               SerialConst::CLIENT_STRING[m_ComPortClient]);
       }

       return RC::SOFTWARE_ERROR;
   }
   return OK;
}

RC Serial::SetReadCallbackFunction(void (*m_readCallbackFunc)(char))
{
   return RC::UNSUPPORTED_HARDWARE_FEATURE;
}
