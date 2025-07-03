#!/usr/bin/elw python3
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import os
import subprocess
import sys
import json
import re
sys.path.append(os.path.dirname(__file__))
import synctools
import process_uphy_bkv_files
import getpass
import smtplib

######################################################################
def NotifyUphyBkvsChanged(changedBkvFiles):
    """ Send an email notifying that the BKV files have changed in the HW tree.

        Args:
            changedBkvFiles: List of changed BKV versions/files.
    """
    server = "mailgw.lwpu.com"

    fromAddr = getpass.getuser() + "@lwpu.com"
    toAddr = "mods-gpu@exchange.lwpu.com"

    msg = ""
    msg += "From: {}\n".format(getpass.getuser() + "@lwpu.com")
    msg += "To: {}\n".format(toAddr)
    msg += "Subject: UPHY BKV Values Changed\n"
    msg += "\n"
    for changedBkv in changedBkvFiles:
        msg += 'Execute "chips_a/diag/mods/tools/update_uphy_bkv_files.py" to generate a pending CL\n'
        msg += 'if the changelist description for the TOT source BKV file indicates that it is an\n'
        msg += 'official BKV release\n'
        msg += '    NOTE : P4ROOT must be set in your environment\n\n'
        msg += "BKV value files {} for UPHY v{}\n".format(changedBkv[3], changedBkv[0])
        msg += "    BKV source files\n"
        for srcFile in changedBkv[1]:
            msg += "        {}\n".format(srcFile)
        msg += "    BKV destination file\n"
        msg += "        {}\n\n".format(changedBkv[2])

    smtpSession = smtplib.SMTP(server)
    smtpSession.sendmail(fromAddr, toAddr, msg)
    smtpSession.quit()

######################################################################
def main():
    cl = synctools.Changelist("p4hw:2001", "p4proxy-sc:2006")

    changedBkvs = []
    for config in process_uphy_bkv_files.g_BkvConfigs:
        (srcFiles, dstFile, action) = \
            cl.processFiles(config['src_files'], config['dst_file'],
                            process_uphy_bkv_files.GenerateUphyBkvJson, allowDelete = False)
        if action:
            changedBkvs += [(config['uphy_version'], srcFiles, dstFile, action)]

    if len(changedBkvs):
        NotifyUphyBkvsChanged(changedBkvs)
    return 0

######################################################################
# Call the main program with a wrapper that sets up the environment
# variables and does error reporting.
#
synctools.cronWrapper(main)
