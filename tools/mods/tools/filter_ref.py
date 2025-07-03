#!/usr/bin/elw python3

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2017,2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import gzip
import re
import sys
import optparse
import os.path

def main (argv=None):
    """Filter a register ref.txt.gz files for whitelisted registers
    
The register whitelist is contained in reg_whitelist.txt
"""

    if argv is None:
        argv = sys.argv

    usage = ("usage: %prog input_file output_file\n")
    parser = optparse.OptionParser(usage=usage)
    (options, args) = parser.parse_args(argv)
    if len(args) < 2:
        parser.print_help()
        return 1
    
    with gzip.open(args[1], "rb") as input_file:
        registers = input_file.readlines()
    registers = [x.decode().strip() for x in registers if x.strip() != ''] 
    
    with open(os.path.dirname(os.path.abspath(__file__)) + "/reg_whitelist.txt", "r") as whitelist_file:
        whitelist = whitelist_file.readlines()
    whitelist = [x.strip() for x in whitelist if x.strip() != ''] 
    whitelist = r'\b(' + "|".join(whitelist) + r')\b'
    whitelist_re = re.compile(whitelist)
    
    registers = filter(whitelist_re.search, registers)
    
    with gzip.open(args[2], "w") as output_file:
        output_file.write(("\n".join(registers) + "\n").encode())

if __name__ == "__main__":
    sys.exit(main())
