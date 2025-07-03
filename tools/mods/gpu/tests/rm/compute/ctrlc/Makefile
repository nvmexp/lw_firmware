all: ctrlctest10 ctrlctest20 ctrlctest30
	
ctrlctest10: ctrlctest.lw ctrlctest.h
	@echo Lwbin $@
	@python ../../../lwca/make_kernel_bin.py -l ctrlctest -s ctrlc -a 10 -m 32

ctrlctest20: ctrlctest.lw ctrlctest.h
	@echo Lwbin $@
	@python ../../../lwca/make_kernel_bin.py -l ctrlctest -s ctrlc -a 20 -m 64

ctrlctest30: ctrlctest.lw ctrlctest.h
	@echo Lwbin $@
	@python ../../../lwca/make_kernel_bin.py -l ctrlctest -s ctrlc -a 30 -m 64

clean:
	@rm -f ctrlc*.lwbin ctrlctest.sm_*.lwbin ctrlctest.sm_*.ptx
	@echo Cleaned
