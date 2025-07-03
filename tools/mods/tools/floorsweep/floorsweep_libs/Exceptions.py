# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

class FloorsweepException(Exception):
    TEST = 0
    EC = 0
    def GetEC(self):
        return int("{:03d}{:03d}".format(self.TEST, self.EC),10)
    def __str__(self):
        basestr = super(FloorsweepException,self).__str__()
        return str(self.__class__.__name__) + (": " + basestr if basestr else "")

class FusingException(FloorsweepException):
    def __init__(self, ec, msg):
        super(FusingException,self).__init__(msg)
        self.EC = ec

class SkuMatchEarlyExit(FloorsweepException):
    EC = 0

class SoftwareError(FloorsweepException):
    EC = 2

class UnknownModsError(FloorsweepException):
    EC = 2

class UnsupportedFunction(FloorsweepException):
    EC = 3

class CommandError(FloorsweepException):
    EC = 5

class FileNotFound(FloorsweepException):
    EC = 11

class UserAbortedScript(FloorsweepException):
    EC = 16

class ModsTimeout(FloorsweepException):
    EC = 77

class TimeoutExpired(FloorsweepException):
    EC = 77

class PciDeviceNotFound(FloorsweepException):
    EC = 220

class IncorrectL2Slices(FloorsweepException):
    EC = 657

class IncorrectRop(FloorsweepException):
    EC = 659

class IncorrectLtc(FloorsweepException):
    EC = 659

class IncorrectFB(FloorsweepException):
    EC = 660

class IncorrectTpc(FloorsweepException):
    EC = 662

class IncorrectPes(FloorsweepException):
    EC = 663

class IncorrectCpc(FloorsweepException):
    EC = 663

class IncorrectGpc(FloorsweepException):
    EC = 664

class CannotMeetFsRequirements(FloorsweepException):
    EC = 668

class FsSanityFailed(FloorsweepException):
    EC = 669

class UnsupportedHardware(FloorsweepException):
    EC = 709

class AssertionError(FloorsweepException):
    EC = 818

def ASSERT(test, message=""):
    if not test:
        raise AssertionError(message)

def CheckFatalErrorCode(code):
    def _raise(exp):
        raise exp
    # switch (code)
    {
        220 : lambda : _raise(PciDeviceNotFound())
    }.get(code % 1000,
          # default
          lambda : None)()
