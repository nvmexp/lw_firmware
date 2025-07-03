#!/usr/bin/python3
from __future__ import print_function
import sys
import json
import jsonschema
from jsonschema import validate
from jsonschema import Draft4Validator
import optparse
import sys

parser = optparse.OptionParser();
parser.add_option('-s','--schema', action="store",dest="schema_file_name",default="lwdisp_rmtest_schema.json") 
parser.add_option('-d','--data', action="store",dest="data_file_name",default="data.json") 

schema_file_name=""
data_file_name=""
my_dbg_message=""

if __name__ == "__main__":
    options, args = parser.parse_args();
    schema_file_name = options.schema_file_name
    data_file_name   = options.data_file_name
    errorsFound = False

    try:
        with open(schema_file_name) as schema_file:    
            try:
                schema = json.load(schema_file)
            except:
                my_dbg_message = "ERROR: Failed to parse schema file (check with jsonLint) - "  + (schema_file_name)
                print (my_dbg_message)
                exit(-1);
    except:
        my_dbg_message = "ERROR: Failed to open schema file - "  + schema_file_name
        print (my_dbg_message)
        exit(-1);
    try:
        with open(data_file_name) as data_file:
            try:
                data = json.load(data_file)
            except:
                my_dbg_message = "ERROR: Failed to parse data file (check with jsonLint) - "  + (data_file_name)
                print (my_dbg_message)
                exit(-1);
    except:
        my_dbg_message = "ERROR: Failed to open data file - "  + (data_file_name)
        print (my_dbg_message)
        exit(-1);

    try:
        Draft4Validator.check_schema(schema);
        v = Draft4Validator(schema);
        for error in (v.iter_errors(data)):
            print (error.message)
            errorsFound = True;
        if (errorsFound):
            my_dbg_msg = "Errors found parsing data in data file - " + data_file_name
            print (my_dbg_message)
            exit(-1)
    except jsonschema.ValidationError as e:
        my_dbg_msg = "Errors found validating data in data file - " + data_file_name + "\n"
        print (e.message);
        exit(-1)
    my_dbg_message = "SUCCESS validating file - " + data_file_name + " with schema " + schema_file_name
    print (my_dbg_message)
