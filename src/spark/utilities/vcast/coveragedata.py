import os;
import re;
import binascii
from optparse import OptionParser;

def generateCoverageData(coverageDataFile, tempOutputFile, outputFile):
    objHandle = open(coverageDataFile,'r');
    outfileHandle = open(outputFile, 'w');
    
    print("generateCoverageData ilwoked");

    coverageValuePattern = re.compile(r'(........)([ \t\n\r\f\v])(........)([ \t\n\r\f\v])(........)([ \t\n\r\f\v])(........)');

    hexValuePattern = re.compile(r'(..)(..)(..)(..)');

    for line in objHandle:
        line = line.strip();
        for match in re.finditer(coverageValuePattern, line):
            coverageValue1Data = match.group(1);
            for match1 in re.finditer(hexValuePattern, coverageValue1Data):
               reverseEndian1 = match1.group(1) + match1.group(2) + match1.group(3) + match1.group(4);
 #              reverseEndian1 = match1.group(4) + match1.group(3) + match1.group(2) + match1.group(1);

            coverageValue2Data = match.group(3);
            for match2 in re.finditer(hexValuePattern, coverageValue2Data):
                reverseEndian2 = match2.group(1) + match2.group(2) + match2.group(3) + match2.group(4);
              #  reverseEndian2 = match2.group(4) + match2.group(3) + match2.group(2) + match2.group(1);
 
              
            coverageValue3Data = match.group(5);
            for match3 in re.finditer(hexValuePattern, coverageValue3Data):
               reverseEndian3 = match3.group(1) + match3.group(2) + match3.group(3) + match3.group(4);
             #  reverseEndian3 = match3.group(4) + match3.group(3) + match3.group(2) + match3.group(1);

               
            coverageValue4Data = match.group(7);
            for match4 in re.finditer(hexValuePattern, coverageValue4Data):
               reverseEndian4 = match4.group(1) + match4.group(2) + match4.group(3) + match4.group(4);
            #   reverseEndian4 = match4.group(4) + match4.group(3) + match4.group(2) + match4.group(1);

#            asciiValue = (str( binascii.unhexlify(reverseEndian1), 'utf-8' )).rstrip('\0') + (str( binascii.unhexlify(reverseEndian2), 'utf-8' )).rstrip('\0') + (str( binascii.unhexlify(reverseEndian3), 'utf-8' )).rstrip('\0') + (str( binascii.unhexlify(reverseEndian4), 'utf-8' )).rstrip('\0');

            asciiValue = str( binascii.unhexlify(reverseEndian1), 'utf-8') + str( binascii.unhexlify(reverseEndian2), 'utf-8') + str( binascii.unhexlify(reverseEndian3), 'utf-8') + str( binascii.unhexlify(reverseEndian4), 'utf-8');

            outfileHandle.write(asciiValue);

    objHandle.close();
    outfileHandle.close();

parser = OptionParser();
parser.add_option("-d", "--txt", dest="coverageDataFile", help=" Coverage data dump from PMU DMEM");
parser.add_option("-p", "--path", dest="dirPath", help="Enter the directory to parse for coverage");
parser.add_option("-o", "--outFile", dest="outFile", help="Coverage data outputted.");
(options,args) = parser.parse_args();

for root, dirs, files in os.walk(options.dirPath):
    for file in files:
        if file.endswith(".txt"):
            moduleName      = file.split('.')[0];
            coverageDataFile = os.path.join(root,file);
            outputFile      = moduleName+'.dat';
            outputFile      = os.path.join(root,outputFile);
            tempOutputFile      = moduleName+'_temp.dat';
            tempOutputFile      = os.path.join(root,tempOutputFile);
            generateCoverageData(coverageDataFile, tempOutputFile, outputFile );
