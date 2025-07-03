
cert_tool = ./x509_cert_to_c.py

gen: 
	${cert_tool} --sign_key EK_TEMPLATE_PVT.pem --key AK_NONCA_TEMPLATE_PVT.pem --issuer_cn 'GA100 APM Dev Root CA' --issuer_sn 4A76C4536730A753DA4AEE9A50A020036AD185BA --subject_cn 'GA100 APM Dev AK Cert' --subject_sn 6153BC261259800F4B54F9BCE7561E4F755DC19C --dump_pem AK_NONCA_TEMPLATE_CERT.pem --real_sign -o x509_ak_nonca_cert.h --auth_id AF0D3589B92CFF5F8FA3AA8B07EF2C5B26864E31
	/home/utils/openssl-1.0.2o/bin/openssl x509 -in AK_NONCA_TEMPLATE_CERT.pem -outform der -out AK_NONCA_TEMPLATE_CERT.der
	/home/utils/openssl-1.0.2o/bin/openssl x509 -text -in AK_NONCA_TEMPLATE_CERT.pem > AK_NONCA_TEMPLATE_CERT.text
	/home/utils/openssl-1.0.2o/bin/openssl asn1parse -in AK_NONCA_TEMPLATE_CERT.pem > AK_NONCA_TEMPLATE_CERT.asn1parse

verify: 
	/home/utils/openssl-1.0.2o/bin/openssl verify -CAfile ek.cert.pem AK_NONCA_TEMPLATE_CERT.pem
all : gen verify 

clean : 
	-rm   AK_NONCA_TEMPLATE_CERT.pem *.asn1parse *.text *.h *.der
