# This script is called after
# cd /dvs/p4/build/sw/BRANCH_PATH/diag/mods/output

#Note: For release branches - we may have to setelw GCOV_PREFIX_STRIP to a value different than 14.
chmod +x mods
BRANCH_DEPTH=$(echo $BRANCH_PATH | tr -cd '/' | wc -c)
export GCOV_PREFIX="/dvs/p4/build/sw/$BRANCH_PATH/diag/mods/output"
export GCOV_PREFIX_STRIP=$(expr $BRANCH_DEPTH + 12)
perl $MODS_LCOV --no-external --capture --initial --base-directory /dvs/p4/build/sw/$BRANCH_PATH/diag/mods --directory . --output-file $GCOV_PREFIX/temp-mods-base.info
./mods --gtest_filter=-MemDevice*
UT_RESULT="$?"
if [ "$UT_RESULT" != "0" ]; then
    exit $UT_RESULT
fi
perl $MODS_LCOV --base-directory /dvs/p4/build/sw/$BRANCH_PATH/diag/mods --directory . --capture --output-file $GCOV_PREFIX/mods-test.info
perl $MODS_LCOV --add-tracefile $GCOV_PREFIX/temp-mods-base.info --add-tracefile $GCOV_PREFIX/mods-test.info --output-file $GCOV_PREFIX/mods-total.info
perl $MODS_LCOV --remove $GCOV_PREFIX/mods-total.info '/dvs/p4/build/sw/tools/linux/mods/*' '/usr/include/*' '/usr/lib/*' '/dvs/p4/build/sw/tools/mods/rapidjson/*' '/dvs/p4/build/sw/$BRANCH_PATH/diag/mods/unittests/gtest/*' '/dvs/p4/build/sw/tools/mods/googletest/*' -o $GCOV_PREFIX/mods_filtered.info
perl $MODS_GENHTML --legend --title $VERSION --output-directory $PWD/cov_html mods_filtered.info
mkdir ut_build
mv cov_html/ ut_build/


# Final output must be mods_runspace.tgz. This is available for download from results DVS page.
tar czf mods_runspace.tgz ut_build
