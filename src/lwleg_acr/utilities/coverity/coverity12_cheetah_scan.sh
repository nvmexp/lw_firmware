#!/bin/tcsh

setelw COVERITY_HOME $P4ROOT/sw/mobile/tools/coverity/coverity_2019.06/
setelw SRC $TOP/tegra_acr/src
setelw COVERITY_OUT $SRC

cd $TOP/tegra_acr/src/
$COVERITY_HOME/bin/cov-configure \
   --compiler falcon-elf-gcc \
   --comptype gcc \
   --template
p4 clean _out/...
$COVERITY_HOME/bin/cov-build \
   --emit-complementary-info \
   --dir $COVERITY_OUT \
   --add-arg \
   --c99 \
   --delete-stale-tus \
   --return-emit-failures \
    make | tee /tmp/cov.out

#if ($? != 0) then
#     exit 1
#endif

cd $TOP/tegra_acr/utilities/coverity
mv .cov.new .cov.old
$COVERITY_HOME/bin/cov-analyze \
     --all \
     --disable-default  \
     --misra-config $P4ROOT/sw/mobile/tools/coverity/coverity_2019.06/config/MISRA/MISRA_c2012_lw.config \
     --coding-standard-config ${COVERITY_HOME}/config/coding-standards/cert-c/cert-c-all.config \
     --dir $SRC \
     --strip-path $SRC \
     --jobs max4 \
     --ignore-deviated-findings \
     --max-mem 768 \
     -en AUDIT.SPELWLATIVE_EXELWTION_DATA_LEAK | tee .cov.new

$COVERITY_HOME/bin/cov-format-errors \
     --dir $SRC \
     --strip-path $SRC \
     --emacs-style > covout_acr.txt

vim -d .cov.old .cov.new

