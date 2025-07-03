#!/usr/bin/elw python3
import os
import subprocess
import sys
sys.path.append(os.path.dirname(__file__))
import synctools

######################################################################
def main():
    cl = Changelist("p4hw:2001", "p4sw:2006")
    cl.copy("//hw/lwgpu/clib/lwshared/fs_lib/...",
            "//sw/dev/gpu_drv/chips_hw/diag/mods/gpu/floorsweep/fs_lib/...",
            ignore = r"^contrib_sw/")
    cl.submit()

    return 0

######################################################################
# Subclass of synctools.Changelist with a custom doSubmit() method
#
class Changelist(synctools.Changelist):
    def doSubmit(self, portName, clientName, clNum):
        submitCmd = ["as2", "-p", portName, "-c", clientName,
                     "submit", "-c", clNum, "-project", "sw_inf_sw_mods",
                     "-notify", "aperiwal:failed"]
        subprocess.run(submitCmd, check = True,
                       stdout = subprocess.PIPE,
                       stderr = subprocess.PIPE)

######################################################################
# Call the main program with a wrapper that sets up the environment
# variables and does error reporting.
#
synctools.cronWrapper(main)
