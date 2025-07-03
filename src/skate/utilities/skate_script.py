from ctypes import Structure
import sys
import base64
import hashlib,hmac, binascii
from typing import ByteString
from Crypto.Cipher import AES
import argparse

 
def strtok(str, ch):
    return str.split(ch)
 
def swap_bytes(str):
    return str[6:8] + str[4:6] + str[2:4] + str[0:2]
 
def get_key(line):
    data = strtok(line, ' ')
    out = ""
    for d in data:
        out += (swap_bytes(d))
    return out
 
def get_hex(str):
    chunks = [str[i:i+8] for i in range(0,len(str), 8)]
    out = b""
    shift = 0
    for c in chunks:
        ## print ("c  = ",c)
        x = int(c,16)
        out += x.to_bytes(4, byteorder = 'big')
        shift += 8
 
    return out

def get_hex1(str):
    chunks = [str[i:i+8] for i in range(0,len(str), 8)]
    out = b""
    shift = 0
    for c in chunks:
        ## print ("c  = ",c)
        x  =  int.from_bytes(c, "big")
        out += x.to_bytes(4, byteorder = 'big')
        shift += 8
 
    return out
 
def callwlate_kcv(key):
    # print ("key  - ",key.decode())
    key = bytes.fromhex(key.decode())
     
    iv = "00000000000000000000000000000000"
    iv = bytes.fromhex(iv)
    cipher = AES.new(key, AES.MODE_ECB)
    abc = "00000000000000000000000000000000"
    abc = bytes.fromhex(abc)
    # print ("msg is ",msg)
    enc = cipher.encrypt(abc)
    return enc.hex()
def byteSwap(str):
    return str[6:8]+str[4:6]+str[2:4]+str[0:2]
def colwertKey4(str):
    str = str.replace(" ","")
    return byteSwap(str[0:8])+byteSwap(str[8:16])+byteSwap(str[16:24])+byteSwap(str[24:32])
 
def hexStrToBytes(str):
    str=str.replace(' ','')
    str.strip()
    pltH=colwertKey4(str)
    pltH=pltH.strip();
    plt=bytes.fromhex(pltH)
    return plt
 
def encrypt_payload(key):
    # print ("key  - ",key.decode())
    key = bytes.fromhex(key.decode())
     
    iv = hexStrToBytes("bcf3419e f188793d 80f1b6fd a2a3253d")
    cipher = AES.new(key, AES.MODE_CBC, iv)
    abc = hexStrToBytes("008a3cec 617248ba df9e796f 39a0bf62")
    # print ("msg is ",msg)
    enc = cipher.encrypt(abc)
    return enc.hex()
 
 
def brom_KDF(key):
    #key = b"31c69ed7aebb544d90b9190aab3653e4"
    hkey  = get_hex(key)
    msg = b"000000010000000000000000000000000000000000000000ffffffff00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000080"
    hmsg = get_hex(msg)
 #hash = binascii.hexlify(hmac.new(binascii.hexlify(key), binascii.hexlify (msg), hashlib.sha256).digest())
    hash = binascii.hexlify(hmac.new((hkey), (hmsg), hashlib.sha256).digest())
    return hash[0:len(hash) // 2]
 
def bitstring_to_bytes(s):
    v = int(s, 2)
    b = bytearray()
    while v:
        b.append(v & 0xff)
        v >>= 8
    return bytes(b[::-1])


def brom_KDF_new(key,engineId, ucodeId, ucodeVersion,fmcDigest,manifestConstant):
    hkey  = get_hex(key)
    firstConst32b = 1
    secondConst64b = 0x1f
    thirdConst32b = 0
    forthConst128b = 0
    lastConst = 0x80

    msg = f'{firstConst32b:032b}'+f'{secondConst64b:064b}' + f'{thirdConst32b:032b}' + f'{engineId:032b}'+f'{ucodeId:032b}'+f'{ucodeVersion:032b}' + f'{fmcDigest:0384b}'+f'{manifestConstant:0256b}' + f'{forthConst128b:0128b}'+f'{lastConst:032b}'
    p = [int(msg[i:i+8],2) for i in range(0,len(msg),8)] 
    s = bytes([x for x in p]) 

    hmsg = s
    hash = binascii.hexlify(hmac.new((hkey), (hmsg), hashlib.sha256).digest())
    return hash[0:len(hash) // 2]

def getDigest():
    arry = [ "1ef3c2cf","50fa0c73","d23b11ce","57815a48","ac879046","22900ba9","a2478703","188d8709","97e67cb5","196c9804","990fb7c2","4315a05a" ]

    out = ""
    for a in arry:
        out += (swap_bytes(a))
    # print(out)
    return out

def Compute(keyFile,hash):
    fo = open(keyFile , "r")
    lines = fo.readlines()
    for line in lines:
        key = get_key(line)
        dkey = brom_KDF(key)
        print (callwlate_kcv(dkey))
        print(encrypt_payload(dkey))
        engineId = 0x4
        ucodeId = 0xd
        ucodeVersion = 0
        fmcDigest = int(hash, 16)
        manifestConstant = 0
        dkey = brom_KDF_new(key, engineId, ucodeId, ucodeVersion,fmcDigest,manifestConstant)
        print (callwlate_kcv(dkey))
        print(encrypt_payload(dkey))

def execute():
    parser = argparse.ArgumentParser(description='SKATE verification')
    parser.add_argument('--key', type=str,
                help='Plaintext key file', required=True)
    parser.add_argument('--text', type=str,
                help='Encrypted text file', required=True)
    parser.add_argument('--data', type=str,
                help='Encrypted data file', required=True)
    parser.add_argument('--manifest', type=str,
                help='Manifest file')
    parser.add_argument('--pdi', type=str,
                help='PDI file')

    args = parser.parse_args()

    pdi = "0000000000000000"
    pdi_pad_len=120
    if args.pdi is not None:
        with open(args.pdi,"r") as f:
            pdi = f.readline().rstrip()
    for i in range(0,pdi_pad_len):
        pdi += "00"

    input = get_hex(pdi)

    with open(args.text, "rb") as f:
        input = input +  f.read()


    with open(args.data, "rb") as f:
        input = input +  f.read()


    digest = hashlib.sha384((input))

    print(type(input))
    hash = (digest.hexdigest())
    print(type(hash))
    Compute(args.key, hash)

execute()

