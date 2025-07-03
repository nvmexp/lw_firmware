#!/usr/bin/python3

import ecdsa
import codecs
import hashlib
import sys, getopt

message = ''
pubx    = ''
puby    = ''
lwrve   = ''
privKey = ''

try:
   opts, args = getopt.getopt(sys.argv[1:], "hm:x:y:c:p:", ["help=","message=","pubx=","puby=","lwrve=","priv="])
except getopt.GetoptError:
   print("verifyEcdsa.py -m <message> -x <public_key_x> -y <public_key_y> -c <lwrve> -p <private_key>")
   exit(2)

for opt, arg in opts:
   if opt in ("-h", "--help"):
      print("verifyEcdsa.py -m <message> -x <public_key_x> -y <public_key_y> -c <lwrve> -p <private_key>")
      sys.exit()
   elif opt in ("-m", "--message"):
      message = arg
   elif opt in ("-x", "--pubx"):
      pubx = arg
   elif opt in ("-y", "--puby"):
      puby = arg
   elif opt in ("-c", "--lwrve"):
      lwrve = arg
   elif opt in ("-p", "--priv"):
      privKey = arg

message     = message.encode()
pubkey      = pubx+puby

try:
    if (lwrve == "NIST256p"):
        sk = ecdsa.SigningKey.from_string(bytes.fromhex(privKey), lwrve=ecdsa.NIST256p, hashfunc=hashlib.sha256)
        vk = ecdsa.VerifyingKey.from_string(bytes.fromhex(pubkey), lwrve=ecdsa.NIST256p, hashfunc=hashlib.sha256)
    elif (lwrve == "NIST384p"):
        sk = ecdsa.SigningKey.from_string(bytes.fromhex(privKey), lwrve=ecdsa.NIST384p, hashfunc=hashlib.sha384)
        vk = ecdsa.VerifyingKey.from_string(bytes.fromhex(pubkey), lwrve=ecdsa.NIST384p, hashfunc=hashlib.sha384)
    signature = sk.sign(message)
    vk.verify(signature, message)
except:
    print("ECDSA signature validation failed!")
    exit(1)

print("ECDSA signature validation success!")
exit(0)
