#!/home/utils/Python-3.6.1/bin/python3
import argparse
import datetime
import hashlib
import os
import re

from asn1crypto import pem, util, x509, keys, algos, core

DEBUG_PRINT = False

class FWID(x509.Sequence):
    _fields = [
            ('hash_alg', x509.ObjectIdentifier),
            ('fwid', x509.OctetString),
            ]

class CompositeDeviceID(x509.Sequence):
    _fields = [
            ('version', x509.Integer),
            ('issuer_public_key_info', keys.PublicKeyInfo),
            ('fwid', FWID),
            ]

class CertGenerator:
    def __init__(self, 
                is_ca,
                issuer_common_name,    # utf string
                issuer_serial_number,  # utf string
                subject_common_name,   # utf string
                subject_serial_number, # utf string
                authority_key_identifier,
                has_tcg_dice_fwid,               
                real_sign,              
                sign_key_pem_bytes,     # file object
                key_pem_bytes,          # file object
                ):
        self.sign_key_pem_bytes = sign_key_pem_bytes
        self.key_pem_bytes = key_pem_bytes
        # public key part will be stored in cdi.public_key
        # sha1(public key) will be stored in ext.authority_key_identifier
        # use private key to sign this cert
        self.sign_key = self.read_key(sign_key_pem_bytes)
        
        # public key part will be stored in subject_public_key_info.public_key
        # sha1(public key) will be stored in key_identifier
        # private key is not present in cert
        self.subject_key = self.read_key(key_pem_bytes)

        self.is_ca = is_ca
        self.issuer_common_name = issuer_common_name
        self.issuer_serial_number = issuer_serial_number
        self.subject_common_name = subject_common_name
        self.subject_serial_number = subject_serial_number

        # Use Authorization Key Identifier from command line if give.
        # Else use hash of Public Key.
        print("auth_id = " + authority_key_identifier)
        if authority_key_identifier != "0":
          self.authority_key_identifier = bytes.fromhex(authority_key_identifier)
        else:
          self.authority_key_identifier = hashlib.sha1(self.sign_key["public_key"].contents[1:]).digest()
        self.has_tcg_dice_fwid = has_tcg_dice_fwid
        self.real_sign = real_sign
        
        self.build_tbs()
        self.sign_tbs()


    def build_tbs(self):
        tbs = x509.TbsCertificate()
        tbs["version"] = 2
        tbs["serial_number"] = int(self.subject_serial_number, 16)
        #int.from_bytes(hashlib.sha1(ak_keys["public_key"].contents[1:]).digest(), byteorder="big")
        tbs["signature"] = x509.SignedDigestAlgorithm(
            {
                "algorithm" : "sha256_ecdsa"
            })
        
        tbs["issuer"] = x509.Name.build(
            {
                "country_name" : u"US",
                "organization_name" : u"LWPU Corporation",
                "common_name" : self.issuer_common_name,
                #"serial_number" : self.issuer_serial_number,
            })
        
        tbs["validity"] = x509.Validity(
            {
                "not_before" : x509.UTCTime(datetime.datetime(2021, 10, 21, 9, 0, tzinfo=datetime.timezone.utc)),
                "not_after" : x509.GeneralizedTime(datetime.datetime(2022, 10, 21, 7, 0, 0, tzinfo=datetime.timezone.utc))
                })
        
        tbs["subject"] = x509.Name.build(
            {
                "country_name" : u"US",
                "organization_name" : u"LWPU Corporation",
                "common_name" : self.subject_common_name,
                #"serial_number" : self.subject_serial_number,
            })
        tbs["subject_public_key_info"] = keys.PublicKeyInfo(
            {
                "algorithm":  keys.PublicKeyAlgorithm(
                    {
                        "algorithm": "ec",
                        "parameters" : self.subject_key["parameters"]
                        }),
                "public_key" : self.subject_key["public_key"]
            })

        tbs_extensions = []
        if self.is_ca:
            tbs_extensions.extend([
                {
                    "extn_id" : "basic_constraints",
                    "critical" : True,
                    "extn_value" : {"ca" : True }
                },
                {
                    "extn_id" : "key_usage",
                    "critical" : True,
                    "extn_value" : x509.KeyUsage((0, 0, 0, 0, 0, 1, 0, 0, 0))
                }])
        else:
            tbs_extensions.append(
                {
                    "extn_id" : "key_usage",
                    "critical" : True,
                    "extn_value" : x509.KeyUsage((1, 0, 1, 1, 0, 0, 0, 0, 0))
                })

        tbs_extensions.extend([
            {
                "extn_id" : "key_identifier",
                "extn_value" : hashlib.sha1(self.subject_key["public_key"].contents[1:]).digest()
            },
            {
                "extn_id" : "authority_key_identifier",
                "extn_value" : {"key_identifier" : self.authority_key_identifier}
            }])

        #if not self.is_ca:
        #    tbs_extensions.append(
        #        {
        #            "extn_id" : "subject_alt_name",
        #            "extn_value" : [ 
        #                x509.GeneralName("other_name", 
        #                    {
        #                        'type_id': "1.3.6.1.4.1.412.274.1",
        #                        'value' : x509.UTF8String('LWPU:XXXXX:0123456789ABCDEF', explicit=0)
        #                    })]
        #        })
                                
        if self.has_tcg_dice_fwid:
            # TCG-DICE-FWID ::== SEQUENCE 
            # {
            #   TCG-DICE-fwid   OBJECT IDENTIFIER,
            #   SEQUENCE        CompositeDeviceID
            # }
            com_dev_id = CompositeDeviceID(
                {
                    'version' : 1,
                    'issuer_public_key_info' : keys.PublicKeyInfo(
                        {
                            "algorithm":  keys.PublicKeyAlgorithm(
                                {
                                    "algorithm": "ec",
                                    "parameters" : self.sign_key["parameters"]
                                }),
                        "public_key" : self.sign_key["public_key"]
                        }),
                    'fwid' : FWID(
                        {
                            'hash_alg' : x509.ObjectIdentifier('2.16.840.1.101.3.4.2.2'),
                            'fwid'     : x509.OctetString(b"\xBE\xC0\x21\xB4\xF3\x68\xE3\x06\x91\x34\xE0\x12\xC2\xB4\x30\x70\x83\xD3\xA9\xBD\xD2\x06\xE2\x4E\x5F\x0D\x86\xE1\x3D\x66\x36\x65\x59\x33\xEC\x2B\x41\x34\x65\x96\x68\x17\xA9\xC2\x08\xA1\x17\x17")
                        }),
             })
            tbs_extensions.append(
                {
                    "extn_id" : '2.23.133.5.4.1',
                    "extn_value" : com_dev_id.dump()
                })
        
        tbs["extensions"] = x509.Extensions(tbs_extensions)
        self.tbs = tbs

    def sign_tbs(self):
        self.cert = x509.Certificate()
        self.cert["tbs_certificate"] = self.tbs
        self.cert["signature_algorithm"] = algos.SignedDigestAlgorithm(
                {
                    "algorithm" : "sha256_ecdsa"
                })

        if self.real_sign:
            try:
                from cryptography.hazmat.primitives.serialization import load_pem_private_key
                from cryptography.hazmat.primitives import hashes
                from cryptography.hazmat.primitives.asymmetric import ec
                from cryptography.hazmat.backends import default_backend

                sk = load_pem_private_key(self.sign_key_pem_bytes, password = None, backend=default_backend())
                self.cert["signature_value"] = sk.sign(self.tbs.dump(), ec.ECDSA(hashes.SHA256()))
            except:
                print("Error: Fail to sign the certificate")
                self.real_sign = False
        
        if not self.real_sign:
            self.cert["signature_value"] = algos.DSASignature(
                {
                    # note we use the biggest size 49 for r and s
                    "r" : int.from_bytes(b"\x8012345678223456783234567842345678523456786234567", "big"),
                    "s" : int.from_bytes(b"\x8012345678223456783234567842345678523456786234567", "big"),
                }).dump()
            

    def read_key(self, key_bytes):
        key_type, key_header, key_der = pem.unarmor(key_bytes)
        assert(key_type == "EC PRIVATE KEY")
        return keys.ECPrivateKey.load(key_der) 
    
    def dump(self):
        # encodes the value using DER
        return self.cert.dump()
    
    def dump_pem(self, type_name = "CERTIFICATE"):
        # encodes the DER in PEM format
        return pem.armor(type_name, self.dump())


class Asn1Node:
    def __init__(self, off, hl, l, tag, val, cls_name, oid, lvl, has_cs):
        self.children = []
        self.off = off
        self.hl = hl
        self.l = l
        self.tag = tag
        self.val = val
        self.cls_name = cls_name
        self.oid = oid
        self.lvl = lvl
        self.has_cs = has_cs
        if DEBUG_PRINT:
            print(self.__str__)
   
    def add_child(self, node):
        if node:
            self.children.append(node)

    def __str__(self):
        return "%s Off:%d, HL:%d, L:%d, Tag:%d, Cls:%s, Oid:%s, Cs:%s"%("    "*self.lvl,
                                                                    self.off, 
                                                                    self.hl, 
                                                                    self.l,
                                                                    self.tag,
                                                                    self.cls_name,
                                                                    self.oid,
                                                                    self.has_cs)


class Asn1Parser:
    ext_map = {
            '2.5.29.15':  x509.KeyUsage,      ## 'key_usage'
            '2.5.29.17':  x509.GeneralNames,      ## 'subject_alt_name'
            '2.5.29.35':  x509.AuthorityKeyIdentifier,      ## 'authority_key_identifier'
            '2.23.133.5.4.1' : CompositeDeviceID
    }
    FIRST_LEVEL = 0
    def __init__(self, asn1val, name=""):
        self.offset = 0
        self.records = []
        self.ready_to_parse_extension = None
        self.top_node = self.relwrsive_parse(asn1val, name, self.FIRST_LEVEL, 0)

    def relwrsive_parse(self, asn1val, name, lwrr_level, offset):
        # return directly if the default value doesn't exist
        if isinstance(asn1val, core.Void):
            return None
        
        # if it's the instance of Any, then we can use the parsed to replace the current asn1val
        if isinstance(asn1val, x509.Any):
            asn1val = asn1val.parsed

        if asn1val._header is None: 
            asn1val.dump()
        hl = len(asn1val._header) if asn1val._header else 0

        if isinstance(asn1val, core.Primitive):
            # If native doesn't exist, it means the value is not present in the binary
            # e.g. this is useful for default values
            if not asn1val.native :
                return None
            
            try:
                oid = asn1val.map(asn1val.dotted)
                # this is kind of arguly, but the extension is packed into an octect string causing we lost the 
                # class definition of it. So here we will use the OID to find the corresponding extension class
                if asn1val.dotted in self.ext_map:
                    self.ready_to_parse_extension = self.ext_map[asn1val.dotted]
                    del(self.ext_map[asn1val.dotted])
                else:
                    self.ready_to_parse_extension = None
            except:
                oid = name

            node = Asn1Node(off = offset,
                            hl = hl,
                            l  = len(asn1val.contents),
                            tag = asn1val.tag,
                            val = asn1val.contents,
                            cls_name = asn1val.__class__.__name__,
                            oid = oid,
                            lvl = lwrr_level,
                            has_cs = True if asn1val.explicit else False
                        )

            #if one value is Parsable it may contain other types
            if isinstance(asn1val, x509.ParsableOctetString):
                if self.ready_to_parse_extension:
                    try:
                        new_val = self.ready_to_parse_extension.load(asn1val.contents)
                        n = self.relwrsive_parse(new_val, "", lwrr_level + 1, offset + hl)
                        if n:
                            node.add_child(n)
                            offset += len(new_val.dump())
                    except:
                        pass
            return node

        if isinstance(asn1val, x509.Choice):
            return self.relwrsive_parse(asn1val.chosen, name, lwrr_level, offset)

        if isinstance (asn1val, (x509.SetOf, x509.SequenceOf, x509.Sequence, x509.Set)):
            node = Asn1Node(off = offset,
                            hl = hl,
                            l  = len(asn1val.contents),
                            tag = asn1val.tag,
                            val = asn1val.contents,
                            cls_name = asn1val.__class__.__name__,
                            oid = "",
                            lvl = lwrr_level,
                            has_cs = True if asn1val.explicit else False
                            )

            for field in asn1val:
                # sequence or set are dict like interface
                if isinstance (asn1val, (x509.Set, x509.Sequence)):
                    field_name = field
                    field_val =  asn1val[field_name]
                else:
                    # sequence_of or set_of are list like interface
                    field_name = ""
                    field_val = field
                n = self.relwrsive_parse(field_val, field_name, lwrr_level + 1, offset + hl)
                if n:
                    node.add_child(n)
                    offset += len(field_val.dump())
            return node
        
        print("Shouldn't be here")
        exit(1)

    
    def _visit(self, node, path):
        yield (node, path)
        for i, child in enumerate(node.children):
            yield from self._visit(child, "%s_%s"%(path, i))
    
    def __iter__(self):
        yield from self._visit(self.top_node, "0")

class AdaRecordTemplate:
    RECORD_FIELD_DEFINE = "    {type}  {name}; {comment}"
    RECORD_BODY_DEFINE = '''
{comment}
#define {name}_Offset  {offset}
#define {name}_Len_In_Byte  {length}
typedef struct {begin}
{fields_def}
{end}  __attribute__((packed)) {name};


'''
    RECORD_FIELD_RANGE = "//    {name} at 0  range {start} .. {end};"
    RECORD_BODY_RANGE = '''
//for {name} use record
//{fields_range}
//end record;
'''

    def build_plain_type(type_len):
        return "LwU8" 

    def build(self, node, path):
        fields_def = [
            self.RECORD_FIELD_DEFINE.format(
                name = "Tag",
                type = AdaRecordTemplate.build_plain_type(1),
                comment = ""
            ),
            self.RECORD_FIELD_DEFINE.format(
                name = "Len[%d]"%(node.hl - 1) if node.hl - 1 > 1 else "Len",
                type = AdaRecordTemplate.build_plain_type(node.hl - 1),
                comment = ""
            )] + ([ self.RECORD_FIELD_DEFINE.format(
                        #name = "Val%d[%d]"%(idx, node.hl - 1) if node.hl - 1 > 1 else "Val%d"%idx,
                        name = "Val%d"%idx,
                        type = "%s_%s_%d"%(child.cls_name, path, idx),
                        comment = "// %s"%child.oid if child.oid else ""
                    ) for idx, child in enumerate(node.children)
                ] if len(node.children) > 0 else [ 
                    self.RECORD_FIELD_DEFINE.format(
                        name = "Val0[%d]"%(node.l) if node.l > 1 else "Val0",
                        type = AdaRecordTemplate.build_plain_type(node.l),
                        comment = ""
                    )]
            )

        fields_range = [
            self.RECORD_FIELD_RANGE.format(
                name = "Tag",
                start = 0,
                end = 7
            ),
            self.RECORD_FIELD_RANGE.format(
                name = "Len",
                start = 8,
                end = node.hl * 8 - 1
            )] + ([ self.RECORD_FIELD_RANGE.format(
                        name = "Val%d"%idx,
                        start = (child.off - node.off) * 8,
                        end = (child.off - node.off) * 8 + (child.l + child.hl) * 8 - 1
                    ) for idx, child in enumerate(node.children)
                ] if len(node.children) > 0 else [ 
                    self.RECORD_FIELD_RANGE.format(
                        name = "Val0",
                        start = node.hl * 8,
                        end = (node.hl + node.l) * 8 - 1
                    )])

        record_name = "%s_%s"%(node.cls_name, path)
        return self.RECORD_BODY_DEFINE.format(
                    comment = "// %s"%node.oid if node.oid else "",
                    name = record_name,
                    begin = '{',
                    end   = '}',
                    fields_def = "\n".join(fields_def),
                    bit_size = (node.l + node.hl) * 8,
                    offset = node.off,
                    length = node.hl + node.l,
                    ) + self.RECORD_BODY_RANGE.format(
                        name = record_name,
                        fields_range = "\n".join(fields_range))
   
class AdaConstantTemplate:
    CONSTANT_DEFINE = "    {name}_Offset : constant := {offset}; {comment}"
    def build(self, node, path):
        name = "%s_%s"%(node.cls_name, path)
        offset = node.off
        comment = "// %s"%node.oid if node.oid else ""
        return self.CONSTANT_DEFINE.format(name = name, offset = offset, comment = comment)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate x509 certificate as well as corresponding Ada package')
    parser.add_argument("--sign_key", type=str, required=True, help='full path of key used to sign this certificate in PEM format')
    parser.add_argument("--key", type=str, required=True, help='full path of key for this certificate in PEM format')
    parser.add_argument("--issuer_cn", type=str, required=True, help='issuer common name')
    parser.add_argument("--issuer_sn", type=str, required=True, help='issuer serial number')
    parser.add_argument("--subject_cn", type=str, required=True, help='subject common name')
    parser.add_argument("--subject_sn", type=str, required=True, help='subject serial number')
    parser.add_argument("--auth_id", type=str, required=False, default='0', help='authority key identifier')
    parser.add_argument("--CA", action='store_true', default=False, help='CA or NonCA')
    parser.add_argument("--TCG_DICE_FWID", action='store_true', default=False, help='Include LW defined TCG-DICE_FWID extension or not')
    parser.add_argument("--real_sign", action='store_true', default=False, help='Really sign this certificate using the sign_key')
    parser.add_argument("--dump_der", type=str, default=None, help='Dump the certificate to file in der format')
    parser.add_argument("--dump_pem", type=str, default=None, help='Dump the certificate to file in pem format')
    parser.add_argument("-o", '--output', type=str, default=None, help='Output Ada package full filename with .h extension')
    options = parser.parse_args()

    PACKAGE_TEMPLATE = '''
 /*
  * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
  * information contained herein is proprietary and confidential to LWPU
  * Corporation.  Any use, reproduction, or disclosure without the written
  * permission of LWPU Corporation is prohibited.
  */

/*
 * DO NOT EDIT - THIS FILE WAS AUTOMATICALLY GENERATED by x509_cert_to_c.py.
 */

#ifndef _X509_CERT_TO_C_H
#define _X509_CERT_TO_C_H

typedef struct Cert_Structure
{{
     LwU16 length;
     LwU16 reserved;
     LwU8  root_hash[32];
     unsigned char Fmc_X509_Ak_Nonca_Cert_Ro_Data[1024];
}} CERT_STRUCT;

CERT_STRUCT Ak_Cert_Structure GCC_ATTRIB_SECTION("dmem_spdm", "_Ak_Cert_Structure") = 
{{
    0,
    0,
    {{ 0 }},
    {{
{bytes}
    }}
}};

{records}
#endif  // _X509_CERT_TO_C_H

'''


    cg = CertGenerator( 
        is_ca = options.CA, 
        issuer_common_name = options.issuer_cn,
        issuer_serial_number = options.issuer_sn,
        subject_common_name = options.subject_cn,
        subject_serial_number = options.subject_sn,
        authority_key_identifier = options.auth_id,
        has_tcg_dice_fwid = options.TCG_DICE_FWID,
        real_sign = options.real_sign,
        sign_key_pem_bytes = open(options.sign_key, "rb").read(),
        key_pem_bytes = open(options.key, "rb").read()
        )

    if options.output:
        p = Asn1Parser(cg.cert, "certificate")
        record_list = []
        for node, path in p:
            record_list.append(AdaRecordTemplate().build(node, path))
#            print("***************************************\n")
#            print(node)
#            print(path)
#            print("***************************************\n")
        record_list.reverse()

        dirname = os.path.dirname(options.output)
        # get lowercase basename without extension
        basename = re.sub(r"\.h$", "", os.path.basename(options.output)).lower()
        outpath = os.path.join(dirname, basename+".h")
        # Upper case each word
        package_name = "_".join([word[0].upper()+word[1:] for word in basename.split("_")])
        with open(outpath, "w") as f:
            f.write(
                PACKAGE_TEMPLATE.format(
                    bytes = " ".join(["0x%02x,"%v for i, v in enumerate(cg.dump())]),
                    name = package_name,
                    records = "\n".join(record_list),
                    begin = '{',
                    end = '}',
            )
        )

    if options.dump_der:
        with open(options.dump_der, "wb") as f:
            f.write(cg.dump())

    if options.dump_pem:
        with open(options.dump_pem, "wb") as f:
            f.write(cg.dump_pem())

