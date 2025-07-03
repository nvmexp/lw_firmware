#!/usr/bin/elw python3

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import os
import subprocess
import sys
sys.path.append(os.path.dirname(__file__))
import synctools

######################################################################
def main():
    cl = Changelist("p4hw:2001", "p4sw:2006")
    cl.copyFile("//dev/mixsig/uphy/6.1/rel/5.1/defs/yaml-spec/dln-reg-spec.json",
                "//sw/mods/shared_files/uphy/dln-reg-spec-v6.1.json", allowDelete = False)
    cl.copyFile("//dev/mixsig/uphy/6.1/rel/5.1/defs/yaml-spec/cln-reg-spec.json",
                "//sw/mods/shared_files/uphy/cln-reg-spec-v6.1.json", allowDelete = False)
    cl.copyFile("//dev/mixsig/uphy/5.0/rel/ga100-fnl/defs/yaml-spec/dln-reg-spec.json",
                "//sw/mods/shared_files/uphy/dln-reg-spec-v5.0.json", allowDelete = False)
    cl.copyFile("//dev/mixsig/uphy/5.0/rel/ga100-fnl/defs/yaml-spec/cln-reg-spec.json",
                "//sw/mods/shared_files/uphy/cln-reg-spec-v5.0.json", allowDelete = False)
    cl.copyFile("//dev/mixsig/uphy/3.2/rel/5/defs/yaml-spec/dln-reg-spec.json",
                "//sw/mods/shared_files/uphy/dln-reg-spec-v3.2.json", allowDelete = False)
    cl.copyFile("//dev/mixsig/uphy/3.2/rel/5/defs/yaml-spec/cln-reg-spec.json",
                "//sw/mods/shared_files/uphy/cln-reg-spec-v3.2.json", allowDelete = False)
    cl.submit()

    return 0

######################################################################
# Subclass of synctools.Changelist with a custom doSubmit() method
#
class Changelist(synctools.Changelist):
    def doSubmit(self, portName, clientName, clNum):
        submitCmd = ["p4", "-p", portName, "-c", clientName,
                     "submit", "-c", clNum]
        subprocess.run(submitCmd, check = True,
                       stdout = subprocess.PIPE,
                       stderr = subprocess.PIPE)

######################################################################
# Call the main program with a wrapper that sets up the environment
# variables and does error reporting.
#
synctools.cronWrapper(main)
