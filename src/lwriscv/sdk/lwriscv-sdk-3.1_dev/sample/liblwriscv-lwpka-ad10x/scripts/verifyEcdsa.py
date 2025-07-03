#!/usr/bin/python3

import ecdsa
import codecs
import hashlib
import sys, getopt

message = ''
pubx    = ''
puby    = ''
sigs    = ''
sigr    = ''
lwrve   = ''

try:
   opts, args = getopt.getopt(sys.argv[1:], "hm:x:y:r:s:c:", ["help=","message=","pubx=","puby=","sigr=","sigs=","lwrve="])
except getopt.GetoptError:
   print("verifyEcdsa.py -m <message> -x <public_key_x> -y <public_key_y> -r <signature_r> -s <signature_s> -c <lwrve>")
   exit(2)

for opt, arg in opts:
   if opt in ("-h", "--help"):
      print("verifyEcdsa.py -m <message> -x <public_key_x> -y <public_key_y> -r <signature_r> -s <signature_s> -c <lwrve>")
      sys.exit()
   elif opt in ("-m", "--message"):
      message = arg
   elif opt in ("-x", "--pubx"):
      pubx = arg
   elif opt in ("-y", "--puby"):
      puby = arg
   elif opt in ("-r", "--sigr"):
      sigr = arg
   elif opt in ("-s", "--sigs"):
      sigs = arg
   elif opt in ("-c", "--lwrve"):
      lwrve = arg

message     = message.encode()
pubkey      = pubx+puby
signature   = sigr+sigs

try:
    if (lwrve == "NIST256p"):
        vk = ecdsa.VerifyingKey.from_string(bytes.fromhex(pubkey), lwrve=ecdsa.NIST256p, hashfunc=hashlib.sha256)
    elif (lwrve == "NIST384p"):
        vk = ecdsa.VerifyingKey.from_string(bytes.fromhex(pubkey), lwrve=ecdsa.NIST384p, hashfunc=hashlib.sha384)
    vk.verify(bytes.fromhex(signature), message)
except:
    print("ECDSA signature validation failed!")
    exit(1)

print("ECDSA signature validation success!")
exit(0)
