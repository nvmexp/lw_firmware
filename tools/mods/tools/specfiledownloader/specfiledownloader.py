#!/usr/bin/python3
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019-2020 by LWPU Corporation. All rights reserved. All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import argparse
import pylwrl
import sys
import subprocess
import requests
from zipfile import ZipFile
import re
import os
import json
import urllib.parse

# The below line will be replaced by VERSION="BRANCH.VERSION"
# when MODS packages the file into a release
VERSION="branch"

def CallServerAPI(URL, getterMethod = True, request = {}):
    print ("Connecting to server")
    try:
        if getterMethod:
            requestsObj = requests.get(URL, stream=True, verify='HQLWCA121-CA.crt')
        else:
            requestsObj = requests.post(URL, json=request, verify='HQLWCA121-CA.crt')
    except requests.exceptions.RequestException as e:
        print ("Error = {}".format(e))
        sys.exit(1)
    return requestsObj

def GetAllSpecNames(baseURL, release):
    if release and "." not in release:
        URL = baseURL + 'GetSpecNamesbybranch/R' + release
    elif release:
        URL = baseURL + 'GetAllSpecNamesByRelease/' + release
    else:
        URL = baseURL + 'GetAllSpecNamesByRelease/' + VERSION
    requestsObj = CallServerAPI(URL)
    try:
        responseJsonObj = requestsObj.json()
        if not requestsObj.json():
            print ("URL = {}".format(URL), file=sys.stderr)
            print ("No specs are found!")
            return
        print ("Names of Specs are:")
        for specName in requestsObj.json():
            print (specName)
    except json.decoder.JSONDecodeError as e:
        print ("Error! URL is set incorrectly = {}".format(URL))
        print ("Exception = {}".format(e))
        sys.exit(1)

def ValidateArgs(args):
    if not args.getAllSpecNames:
        assert args.specId != None or args.specName != None, "SpecId or SpecName should be given"
    elif args.filter:
        assert args.specName != None, "SpecName should be given when filter option is given"
    else:
        assert args.specId == None and args.specName == None, "SpecId or SpecName should not be given"

def ExtractAndDelZip(filename):
    with ZipFile(filename, 'r') as zipObj:
        # Extract all the contents of zip file in current directory
        for specFile in zipObj.namelist():
            print ("Extracting {}".format(specFile))
            zipObj.extract(specFile)
    print ("Deleting {}".format(filename))
    subprocess.getstatusoutput('rm -f ' + filename)

def GetFileNameWithExt(contentDis, output):
    fname = re.findall("filename=(.+)", contentDis)
    filename = fname[0]
    if output:
        path_ext = os.path.splitext(fname[0])
        filename = output
        if path_ext[1] not in filename:
            filename += path_ext[1]
    return filename

def GetSpecBasedOnFilter(filterlst, specName, output, baseURL, release = ""):
    URL = baseURL + 'DownloadSpecs'
    getterMethod = False
    data = {}
    data['Name'] = specName
    specFilterQueries = {}
    specFilterQueries["SpecFilterQueries"] = [dict(Query=filter) for filter in filterlst]
    Version = {}
    Version["Version"] = VERSION
    if release:
        Version["Version"] = release
    data["Release"] = Version
    data['LwrrentRevision'] = specFilterQueries
    requestsObj = CallServerAPI(URL, getterMethod, data)
    if requestsObj.status_code != 200 or "Spec doesn't exist" in requestsObj.text or 'content-disposition' not in requestsObj.headers:
        print ("URL = {}".format(URL))
        print ("Error! Spec doesn't exist", file=sys.stderr)
        return
    contentDis = requestsObj.headers['content-disposition']
    filename = GetFileNameWithExt(contentDis, output)
    with open (filename, 'wb') as f:
        f.write(requestsObj.content)
    if ".zip" in filename:
        ExtractAndDelZip(filename)
    print ("Success!")

def GetSpec(specId, specName, release, revisionNumber, includeAllTests, output, baseURL, filterlst = []):
    RESPONSE_OK = 200
    c = pylwrl.Lwrl()
    URL = ""
    if specName:
        if release and "." not in release:
            URL = 'DownloadSpecByBranch/' + specName + '/R' + release
        elif release and revisionNumber:
            URL = 'DownloadSpecByRelease/' + specName + '/' + release + '/' + revisionNumber
        elif release:
            URL = 'DownloadSpecByRelease/' + specName + '/' + release
        elif revisionNumber:
            URL = 'DownloadSpecByRelease/' + specName + '/' + VERSION + '/' + revisionNumber
        else:
            URL = 'DownloadSpecByRelease/' + specName + '/' + VERSION
    elif specId:
        if revisionNumber and includeAllTests:
            URL = 'DownloadSpec/' + specId + '/' + revisionNumber + '/true'
        elif revisionNumber:
            URL = 'DownloadSpec/' + specId + '/' + revisionNumber
        else:
            URL = 'DownloadSpec/' + specId
    # URL encoding
    URL = baseURL + urllib.parse.quote(URL)
    requestsObj = CallServerAPI(URL)
    # content-disposition contains information about the attachment that gets downloaded
    if "Spec doesn't exist" in requestsObj.text or 'content-disposition' not in requestsObj.headers:
        print ("URL = {}".format(URL))
        print ("Error! Spec doesn't exist", file=sys.stderr)
        return
    contentDis = requestsObj.headers['content-disposition']
    filename = GetFileNameWithExt(contentDis, output)
    print ("Downloading {}".format(filename))
    with open(filename, 'wb') as f:
        f.write(requestsObj.content)

    if ".zip" in filename:
        ExtractAndDelZip(filename)

    print ("Success!")
    c.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Spec File Downloader Args')
    parser.add_argument('-o', '--output', help='Output file name, Can only be used when the file downloaded' +
                        ' from server is a spec file and not a zip file')
    parser.add_argument('-specId', '--specId', help='Spec Id')
    parser.add_argument('-specName', '--specName', help='Spec Name')
    parser.add_argument('-release', '--release', help='--release MajorVersion.MinorVersion for a specific release '
                                        + 'or --release MajorVersion for the entire branch')
    parser.add_argument('-revisionNumber', '--revisionNumber', help='revision number of the spec')
    parser.add_argument('-includeAllTests', '--includeAllTests', action='store_true', help = 'include all tests' +
                                            'that are disabled in a commented manner')
    parser.add_argument('--getAllSpecNames', action='store_true', help='Provide a --release with this argument')
    parser.add_argument('--filter', action='append', dest='filter', default=[],
                        help='Add repeated filters to a list, --specName should be specified with this argument.' + 
                        ' If release is given, specify the minor version too. ex: 445.2')
    args = parser.parse_args()
    if not len(sys.argv) > 1:
        parser.print_help()
        exit(1)

    ValidateArgs(args)
    baseURL = 'https://cftt.lwpu.com/ModsWebApi/'
    if args.getAllSpecNames:
        GetAllSpecNames(baseURL, args.release)
    elif args.filter:
        GetSpecBasedOnFilter(args.filter, args.specName, args.output, baseURL, args.release)
    else:
        # TODO: Cleanup this function declaration to pass args
        GetSpec(args.specId, args.specName, args.release, args.revisionNumber, args.includeAllTests, args.output, baseURL, args.filter)
