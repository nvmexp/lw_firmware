#! /usr/bin/elw python

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

def makeStringTables(typename, symbols):
    result = []
    result.append("")
    result.append("namespace RegHalTableInfo")
    result.append("{")
    result.append("#ifdef DEBUG")
    result.append("    extern const Raw%sMap g_%sMap[];" % (typename, typename));
    result.append("    const Raw%sMap g_%sMap[] =" % (typename, typename));
    result.append("    {")
    for symbol in symbols:
        result.append("        { %s, \"%s\" }," % (symbol, symbol))
    result.append("        { static_cast<%s>(0), nullptr }" % (typename))
    result.append("    };")
    result.append("#endif // DEBUG")
    result.append("}")
    result.append("")
    return "\n".join(result)
