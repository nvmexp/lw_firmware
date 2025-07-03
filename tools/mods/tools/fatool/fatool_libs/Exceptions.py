# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

class FAException(Exception):
    def __init__(self, ec):
        self.exit_code = ec

class SoftwareError(FAException):
    def __init__(self):
        FAException.__init__(self, 2)

class CommandError(FAException):
    def __init__(self):
        FAException.__init__(self, 2)

class FileNotFoundError(FAException):
    def __init__(self):
        FAException.__init__(self, 11)

class UserAbortedScript(FAException):
    def __init__(self):
        FAException.__init__(self, 16)
