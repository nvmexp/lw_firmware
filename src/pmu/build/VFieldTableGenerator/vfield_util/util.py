if __name__ == '__main__':
    print('ERROR! This file is not meant to be used as a standalone script!')
    exit(-1)

def groupChipsByVfieldTbl(chips):
    groups = {}
    for chip, chipVfieldTbl in chips.items():
        tup = tuple(sorted(chipVfieldTbl.items()))
        if tup in groups:
            groups[tup].append(chip)
        else:
            groups[tup] = [chip]
    
    return groups

def generateChipGroup(chip_list):

    leadingChip = chip_list[0].upper()

    return '_{lead}   => [ {list}, ],'.format(
            lead=leadingChip,
            list=', '.join([chip.upper() for chip in chip_list])
    )

def generateVfieldCTbl(vfield_tbl):
    num_vfields = len(vfield_tbl)

    generatedTbl = (
        'static LwU32 s_vfieldRegAddrLookupTable[{n}]\n'.format(n=num_vfields)  +
        '    GCC_ATTRIB_SECTION("dmem_perfVfe", "vfieldLookupTable") =\n' +
        '{\n'
    )
    for i in range(0, len(vfield_tbl), 5):
        if i != 0:
            generatedTbl = generatedTbl + ',\n'
        generatedTbl = generatedTbl + '    {}'.format(', '.join(vfield_tbl[i:i+5]))
    generatedTbl = generatedTbl + '\n};\n'

    return generatedTbl
