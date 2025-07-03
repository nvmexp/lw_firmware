OBJS += utils.o

ifneq (,$(filter 1,$(LIBCCC_FSP_SE_ENABLED) $(LIBCCC_SECHUB_SE_ENABLED)))
    OBJS += peregrine_sha.o # Needed for rsa3kpss
    OBJS += rsa3kpss.o
    OBJS += rawrsa1k.o
    OBJS += ecdsa.o
    OBJS += ecdsa_atomic_primitive.o
    OBJS += lwrng.o
    OBJS += ecc_key_gen.o
    OBJS += ecdsa_verif.o
endif

ifneq (,$(filter 1,$(LIBCCC_FSP_SE_ENABLED) $(LIBCCC_SELITE_SE_ENABLED)))
    OBJS += aesrng.o
	OBJS += aesgcm.o
endif

ifneq (,$(filter 1,$(LIBCCC_FSP_SE_ENABLED)))
    OBJS += sha.o
endif
