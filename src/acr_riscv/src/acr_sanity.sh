
lwmake clean
lwmake clobber
lwmake -k
RC=$?
rm -rf acr_build.succeded
if [ $RC -eq 0 ]
then
    touch acr_build.succeded
fi   
cd ../build
make check_acr
