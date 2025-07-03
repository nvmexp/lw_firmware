#! /usr/bin/elw python

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

# This script callwlates size of the specified file and produces output
# file (should be named size.data) which DVS can recognize and pull the
# size from.  This is used in DVS to monitor the size of the MODS exelwtable.
#
# Usage: ./dvssize.py size.data mods
#
# The output file size.data is formatted using Google Protobuf format,
# which is described here:
# https://developers.google.com/protocol-buffers/docs/encoding
#
# This script is based on the PERL script used by GL and LWCA, which is
# in drivers/common/build/unix/size.pl

import os
import struct
import subprocess
import sys

# Creates protobuf string or raw data message
# Input: fieldnum is the field index, data is the string or data
# Output message layout:
# - byte 0 is packed field index and message type (2 for string)
# - byte 1 is string length (0..127, we don't support longer strings)
# - bytes 2... are the string contents
def PBString(fieldnum, data):
    if sys.version_info[0] >= 3 and isinstance(data, str):
        data = data.encode("utf8")
    size = len(data)
    # 3 least significant bits are message type (2 for string)
    type = (fieldnum << 3) | 2
    return struct.pack('BB' + str(size) + 's', type, size, data)

# Creates protobuf uint32 message
# Input: fieldnum is the field index, value is the integer number
# Output message layout:
# - byte 0 is packed field index and message type (0 for varint)
# - bytes 1... are the packed integer number (refer to protocol desc)
def PBUint32(fieldnum, value):
    data = bytes()
    while value > 127:
        data += struct.pack('B', (value & 0x7F) + 128)
        value >>= 7
    data += struct.pack('B', value)
    return struct.pack('B', fieldnum << 3) + data

# Creates full protobuf message for a file and its size with the layout
# understood by DVS.  Refer to drivers/common/build/unix/size.pl for
# layout description.
def GenDataSizeOneItem(filename, size):
    return PBString(1,
               PBString(1,
                   PBString(1, "Total") +
                   PBUint32(2, size)) +
               PBString(2,
                   PBString(1, filename) +
                   PBUint32(2, size) +
                   PBString(6, "File")))

# Returns sum of sizes of all sections inside the exelwtable pointed by path
def GetELFSectionsSize(path):
    data = subprocess.check_output(["readelf", "-S", path])
    if sys.version_info[0] >= 3:
        data = data.decode("utf8")

    # Note: sections with no storage in the exe, like .bss, are marked as NOBITS
    data = filter(lambda x: not "NOBITS" in x,
               filter(lambda x: x.startswith("."),
                   map(lambda x: x.strip(),
                       data.split("]"))))

    total = 0
    for value in data:
        value = value.split(None, 5)
        print(value[0] + " - " + str(int(value[4], 16)))
        total += int(value[4], 16)
    return total

# Creates full protobuf message for a file pointed to by full path
def GenDataSizeOneFile(path):
    if sys.platform.startswith("linux"):
        size = GetELFSectionsSize(path)
    else:
        size = os.path.getsize(path)
    print(path + " total - " + str(size))
    return GenDataSizeOneItem(os.path.basename(path), size)

open(sys.argv[1], "wb").write(GenDataSizeOneFile(sys.argv[2]))
