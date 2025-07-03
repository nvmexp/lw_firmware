#!/usr/bin/elw python3
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import sys
import os
sys.path.append(os.path.dirname(__file__))
import synctools
import process_uphy_bkv_files
import getpass
import argparse

######################################################################
# Generates a pending CL containing all UPHY BKV table updates
def main():
    os.elwiron["P4USER"] = getpass.getuser()
    os.elwiron["PATH"] = "/bin:/usr/bin:/home/utils/bin:/home/lw/bin"

    if not os.elwiron.get('P4ROOT'):
        print('P4ROOT must be set in the environment!')
        return 1

    cl = synctools.Changelist("p4hw:2001", "p4proxy-sc:2006")

    changedBkvs = []
    for config in process_uphy_bkv_files.g_BkvConfigs:
        (srcFiles, dstFile, action) = \
            cl.processFiles(config['src_files'], config['dst_file'],
                            process_uphy_bkv_files.GenerateUphyBkvJson, allowDelete = False)

    clNum = cl.createCl(os.elwiron.get('P4ROOT'))
    if clNum:
        print("Created UPHY BKV update CL {}".format(clNum))
    return 0

######################################################################
if __name__ == "__main__":
    sys.exit(main())
