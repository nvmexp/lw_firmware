import sys

print("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_header_plaintext[31:0]}    32'h00010301")

fo = open(sys.argv[1] , "r")

line = fo.readline().strip()
line = line.replace(' ', '')

print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_pdi_0[31:0]}             32'h%s" % (line[0:8]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_pdi_1[31:0]}             32'h%s" % (line[8:]))

line = fo.readline().strip()
line = line.replace(' ', '')
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_0[31:0]}    32'h%s" % (line[0:8]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_1[31:0]}    32'h%s" % (line[8:16]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_2[31:0]}    32'h%s" % (line[16:24]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_3[31:0]}    32'h%s" % (line[24:]))

line = fo.readline().strip()
line = line.replace(' ', '')
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_4[31:0]}    32'h%s" % (line[0:8]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_5[31:0]}    32'h%s" % (line[8:16]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_6[31:0]}    32'h%s" % (line[16:24]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_7[31:0]}    32'h%s" % (line[24:]))

line = fo.readline().strip()
line = line.replace(' ', '')
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_8[31:0]}    32'h%s" % (line[0:8]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_9[31:0]}    32'h%s" % (line[8:16]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_10[31:0]}    32'h%s" % (line[16:24]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_11[31:0]}    32'h%s" % (line[24:]))

line = fo.readline().strip()
line = line.replace(' ', '')
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_12[31:0]}    32'h%s" % (line[0:8]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_13[31:0]}    32'h%s" % (line[8:16]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_14[31:0]}    32'h%s" % (line[16:24]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_15[31:0]}    32'h%s" % (line[24:]))

line = fo.readline().strip()
line = line.replace(' ', '')
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_16[31:0]}    32'h%s" % (line[0:8]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_17[31:0]}    32'h%s" % (line[8:16]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_18[31:0]}    32'h%s" % (line[16:24]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_keys_ciphertext_19[31:0]}    32'h%s" % (line[24:]))

line = fo.readline().strip()
line = line.replace(' ', '')
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_protectinfo_plaintext_0[31:0]}    32'h%s" % (line[0:8]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_protectinfo_plaintext_1[31:0]}    32'h%s" % (line[8:16]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_protectinfo_plaintext_2[31:0]}    32'h%s" % (line[16:24]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_payload_protectinfo_plaintext_3[31:0]}    32'h%s" % (line[24:]))
line = fo.readline().strip()
line = line.replace(' ', '')
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_signature_0[31:0]}    32'h%s" % (line[0:8]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_signature_1[31:0]}    32'h%s" % (line[8:16]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_signature_2[31:0]}    32'h%s" % (line[16:24]))
print ("force {$elw(IXCOM_TOP)lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_enc_fuse_key_signature_3[31:0]}    32'h%s" % (line[24:]))

