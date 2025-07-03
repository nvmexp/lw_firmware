#!/home/utils/Python/builds/3.6.6-20180809/bin/python3.6

## Python script to generate all valid floorsweepable configs
## for a given chip for PRODUCT/FUNC_VALID modes using the fs_lib
## Output of this script is a excel spreadsheet
## Copyright LWPU Corp. 2019
import re
import os
import sys
import pandas as pd
import numpy as np
import argparse

def color_negative_red(val):
    color = 'red' if (val == '0' or val == 0) else 'black'
    return 'color: %s' % color

def write_excel(writer,csvfile,sheet):

    df_new = pd.read_csv(csvfile)
#    if (iscolor):
#        df_new = df_new.style.applymap(color_negative_red, subset=['A:F0','B:F1','A:F2','C:F3','B:F4','C:F5','E:F6','D:F7','F:F8','D:F9','F:F10','E:F11','G0','G1','G2','G3','G4','G5','G6','G7'])
    
    df_new.to_excel(writer, sheet_name=sheet, index=False)

    workbook  = writer.book
    worksheet = writer.sheets[sheet]
    worksheet.set_zoom(70)

    # Account info columns
#    worksheet.set_column('A:A', 10)
#    worksheet.set_column('B:M', 5)
#    worksheet.set_column('N:Q', 10)
#    worksheet.set_column('R:Y', 5)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate fslib valid config list, dumpboth implies dump both product and func_valid, not used with -mode')
    parser.add_argument('--chip', action="store", dest="chip", required=True)
    parser.add_argument('--mode', action="store", dest="mode", required=False, default="PRODUCT")
    parser.add_argument('--outdir', action="store", dest="output_dir", required=False, default=".")
    parser.add_argument('--filename', action="store", dest="filename", required=False, default="default_dump")
    parser.add_argument('--skip-fsdump', action="store", dest="skip", required=False, default="0")
    parser.add_argument('--dumpboth', action="store", dest="dumpboth", required=False, default="0")

    args = parser.parse_args()
#    shutil.rmtree(args.output_dir, ignore_errors=True)
#    os.mkdir(args.output_dir)
    xlsx = os.path.join(args.output_dir, args.chip+"_"+args.filename+".xlsx")
    if (args.skip != "1"):
        if (args.dumpboth != "0"):
            for mode in ['PRODUCT','FUNC_VALID']:
                os.system("lib/fsdump " + " -chip " + args.chip + " -mode " + mode + " -o " + args.filename + "_" + mode + ".csv")
        else:
            os.system("lib/fsdump " + " -chip " + args.chip + " -mode " + args.mode + " -o " + args.filename + "_" + args.mode + ".csv")

    print ("Generating excel spreadsheet ...")
    writer = pd.ExcelWriter(xlsx, engine='xlsxwriter')

    if (args.dumpboth != "0"):
        for mode in ['PRODUCT','FUNC_VALID']:
            write_excel(writer, args.filename + "_" + mode + ".csv", mode)
    else:
        write_excel(writer, args.filename + "_" + args.mode + ".csv", args.mode) # default print without color

    writer.save()


