#!/bin/tcsh

########################Whats happening in Script################################
# 1. Clean Previous Build Completely
# 2. Build MODS normally
# 3. Clean RM part from whole mods pacakage
# 4. Build RM Only with code coverage tool (Bullseye) enable
# 5. Execute the selected test set
# 6. create Function and directory level report in .html format
# 7. store all these stuffs in one folder marked with fmodel and date
# 8. Clean the MODS completely
# 
# NOTE : Script expects user to set $MODS_PATH, $MODS_RUNSPACE, $TGENBASE variables
# e.g
# MODS_PATH = <branch_name>/diag/mods
# MODS_RUNSPACE = Where u gets mods.exe
# TGENBASE = hw/fermi_gf100/diag/testgen
#
#################################################################################

################################### Argument parsing ############################
if ($#argv < 1 || $#argv > 2) then
   echo "Wrong number of arguments"
   echo "Use -h or --help for more info"
   exit 0
endif

foreach i ($argv)
    if ($i == "-h" || $i == "--help") then
        echo " NOTE : MODS_PATH , MODS_RUNSPACE and TGENBASE are expected to be set by user"
        echo " Specify the Chip ( fmodel )for whom to take coverage"
        echo " available fmodel options"
        echo " gf100/gt216/gt218"
        echo " Specify which test suits to Run"
        echo " available test suits options "
        echo " 1> dvs (default)"
        echo " 2> fermi_set"
        echo " e.g ./codecoverage.sh gf100 dvs"
        exit 0
    endif
end

if ($1 != "gt218" && $1 != "gt216" && $1 != "gf100") then
    echo " Bad Fmodel Name "
    echo " available options : gt216/gt218/gf100"
    exit 0
endif

set dvs = 0
set fermiset = 0

if ($#argv == 2) then
    if ($2 == "dvs") then
        set dvs = 1
    else if ($2 == "fermi_set" && $1 != "gf100") then
        echo " fermi_set option should only come with gf100 fmodel"
        exit 0
    else if ($2 == "fermi_set") then
        set fermiset = 1
    else
        echo " Illegal second argument "
        echo " available options : dvs/fermi_set"
        exit 0
    endif
else
    set dvs = 1
endif 

####################################################################################

# Go To MODS Directory
cd $MODS_PATH

# Clean your earlier build
make clean_all
if ( $? != "0" ) then
    echo " make clean_all : command fail"
    exit 0
endif

make clobber
if ( $? != "0" ) then
    echo " make clobber : command fail"
    exit 0
endif

# Build Fresh MODS
make build_all
if ( $? != "0" ) then
    echo " make build_all : command fail"
    exit 0
endif


# Clean Only resman
make submake.resman.clean
if ( $? != "0" ) then
    echo " make submake.resman.clean : command fail"
    exit 0
endif


# Set the covearge file path
setelw COVFILE $MODS_RUNSPACE/test.cov

# set BULLSEYE BIN Path
setelw LW_BULLSEYE_BIN /home/tools/ccover/ccover-7.3.11/bin

# build the librm.so with newer compiler and Code coverage enable
make submake.resman.build_install LW_BULLSEYE=1
if ( $? != "0" ) then
    echo " make submake.resman.build_install : command fail"
    exit 0
endif


# Move to Mods Runspace to execute RMTEST
cd $MODS_RUNSPACE

# Create a Directory where to have the coverage report in HTML format
set DirName = $1_coverage_`date +%d%m%y`

mkdir $DirName
if ( $? != "0" ) then
    echo " Not able to create directory "
    echo $Dirname
    exit 0
endif

if ($fermiset == 1) then

    # HW/arch/rmtest tgen tests
    cd $TGENBASE

    ./tgen.pl -level rmtest_coverage -outdir ./$DirName/rmtest_coverage -debug
    if ( $? != "0" ) then
        echo " exelwtion of -level rmtest_coverage fail "
        exit 0
    endif
else if ($dvs == 1) then
    ./sim.pl -chip $1 -fmod rmtest.js -dvs -run_on_error | tee ./$DirName/dvs.log
    if ( $? != "0" ) then
        echo " exelwtion of RMTEST for -dvs fail "
        exit 0
    endif
endif

# Create report as per function coverage
/home/tools/ccover/ccover-7.3.11/bin/covfn --html > ./$DirName/fncoverage.html

if ( $? != "0" ) then
    echo " /home/tools/ccover/ccover-7.3.11/bin/covf fail "
    exit 0
endif

# Create report as per Directory coverage
/home/tools/ccover/ccover-7.3.11/bin/covdir --html > ./$DirName/dircoverage.html
if ( $? != "0" ) then
    echo " /home/tools/ccover/ccover-7.3.11/bin/covdir fail "
    exit 0
endif

# copy coverage file to relevant location
cp ./test.cov ./$DirName/fermi_coverage.cov
if ( $? != "0" ) then
    echo " cp ./test.cov ./$DirName/fermi_coverage.cov fail "
    exit 0
endif

# Clean the build now
make clean_all
if ( $? != "0" ) then
    echo " make clean_all : command fail"
    exit 0
endif

make clobber
if ( $? != "0" ) then
    echo " make clobber : command fail"
    exit 0
endif
