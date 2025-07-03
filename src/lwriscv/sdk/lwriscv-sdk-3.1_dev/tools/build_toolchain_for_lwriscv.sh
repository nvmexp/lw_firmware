#!/usr/bin/elw bash

# Script to build toolchain for lwriscv
# gcc, binutils, and gdb

gcc_version=8.3.0

multilib_list=" \
    rv32im-ilp32-- \
    rv32imc-ilp32-- \
    rv32imfc-ilp32f-- \
    rv64im-lp64-- \
    rv64imc-lp64-- \
    rv64imfc-lp64f--"

gmp_version=6.1.2
mpfr_version=3.1.5
mpc_version=1.0.3
isl_version=0.18

binutils_version=2.32
gdb_version=8.3

packageversion="LWRISCV GCC "$gcc_version"-"$(date +"%Y.%m.%d")

case "$(uname -s)" in
    Linux*)
        build=x86_64-pc-linux-gnu
        tmp_dir=/var/tmp/$(whoami)
        install_dir=linux/release/$(date +"%Y%m%d")
        ;;
    MINGW*)
        build=i686-pc-mingw32
        tmp_dir=$HOME/tmp
        install_dir=MinGW/release/$(date +"%Y%m%d")
        ;;
    *)
        __die "OS Platform not supported"
        ;;
esac

if [ -z "$P4ROOT" ]; then
    install_dir=$tmp_dir/$install_dir
else
    install_dir=$P4ROOT/sw/tools/riscv/toolchain/$install_dir
fi

#======================================================================
# Utility functions
#======================================================================
tarfile_dir=$tmp_dir/gcc-${gcc_version}_taballs
source_dir=$tmp_dir/gcc-${gcc_version}_source
build_dep_dir=$tmp_dir/gcc-${gcc_version}_dep

__pause()
{
    read -p "Hit any key to continue..."
}

__die()
{
    echo $*
    exit 1
}

function __banner()
{
    echo "============================================================"
    echo $*
    echo "============================================================"
}

__untar()
{
    echo $2
    dir="$1";
    file="$2"
    case $file in
        *xz)
            tar xJ -C "$dir" -f "$file"
            ;;
        *bz2)
            tar xj -C "$dir" -f "$file"
            ;;
        *gz)
            tar xz -C "$dir" -f "$file"
            ;;
        *)
            __die "don't know how to unzip $file"
            ;;
    esac
}

__abort()
{
        cat <<EOF
***************
*** ABORTED ***
***************
An error oclwrred. Exiting...
EOF
        exit 1
}

function __wget()
{
    urlroot=$1; shift
    tarfile=$1; shift

    if [ ! -e "$tarfile_dir/$tarfile" ]; then
        wget --verbose $urlroot/$tarfile --directory-prefix="$tarfile_dir"
    else
        echo "already downloaded: $tarfile  '$tarfile_dir/$tarfile'"
    fi
}

# Set script to abort on any command that results an error status
trap '__abort' 0
set -e

#======================================================================
# Directory creation
#======================================================================
__banner Creating directories

# ensure workspace directories don't already exist
for d in "$source_dir" "$build_dep_dir" ; do
    if [ -d  "$d" ]; then
        __die "directory already exists - please remove and try again: $d"
    fi
done

for d in "$tarfile_dir" "$source_dir" "$build_dep_dir" "$install_dir" ;
do
    test  -d "$d" || mkdir --verbose -p $d
done

#======================================================================
# Download source code
#======================================================================
__banner Downloading source code

gcc_tarfile=gcc-${gcc_version}.tar.xz
gmp_tarfile=gmp-${gmp_version}.tar.xz
mpfr_tarfile=mpfr-${mpfr_version}.tar.xz
mpc_tarfile=mpc-${mpc_version}.tar.gz
isl_tarfile=isl-${isl_version}.tar.xz

__wget http://ftp.gnu.org/gnu/gcc/gcc-$gcc_version $gcc_tarfile
__wget http://ftp.gnu.org/gnu/gmp $gmp_tarfile
__wget http://ftp.gnu.org/gnu/mpfr $mpfr_tarfile
__wget http://ftp.gnu.org/gnu/mpc $mpc_tarfile
__wget http://isl.gforge.inria.fr $isl_tarfile

for f in $gcc_tarfile $gmp_tarfile $mpfr_tarfile $mpc_tarfile $isl_tarfile
do
    if [ ! -f "$tarfile_dir/$f" ]; then
        __die tarfile not found: $tarfile_dir/$f
    fi
done

binutils_tarfile=binutils-${binutils_version}.tar.xz
gdb_tarfile=gdb-${gdb_version}.tar.xz

__wget http://ftp.gnu.org/gnu/binutils $binutils_tarfile
__wget http://ftp.gnu.org/gnu/gdb $gdb_tarfile

for f in $binutils_tarfile $gdb_tarfile
do
    if [ ! -f "$tarfile_dir/$f" ]; then
        __die tarfile not found: $tarfile_dir/$f
    fi
done

#======================================================================
# Unpack source tarfiles
#======================================================================
__banner Unpacking source code

__untar "$source_dir" "$tarfile_dir/$gcc_tarfile"
__untar "$source_dir" "$tarfile_dir/$binutils_tarfile"
__untar "$source_dir" "$tarfile_dir/$gdb_tarfile"
__untar "$source_dir" "$tarfile_dir/$gmp_tarfile"
__untar "$source_dir" "$tarfile_dir/$mpfr_tarfile"
__untar "$source_dir" "$tarfile_dir/$mpc_tarfile"
__untar "$source_dir" "$tarfile_dir/$isl_tarfile"

echo Please apply local patches
__pause

#======================================================================
# Clean environment
#======================================================================
__banner Cleaning environment

# U=$USER
# H=$HOME

# for i in $(elw | awk -F"=" '{print $1}') ;
# do
#     unset $i || true   # ignore unset fails
# done

# export USER=$U
# export HOME=$H
# export PATH=${install_dir}/bin:/usr/local/bin:/usr/bin:/bin:/sbin:/usr/sbin
export PATH=$PATH:$install_dir/bin:$build_dep_dir/bin:/usr/bin:/bin:/usr/sbin:/sbin:/mingw/bin
export PATH=$PATH:$(dirname $(which python))

echo shell environment follows:
elw

#======================================================================
# Prepare prerequisites
#======================================================================
__banner Preparing prerequisites

__banner Preparing gmp
cd $source_dir/gmp-$gmp_version
mkdir build
cd build
../configure \
    --prefix=$build_dep_dir \
    --build=$build \
    --disable-shared \
    --enable-static \
    --enable-fat

make -j8
make install

__banner Preparing mpfr
cd $source_dir/mpfr-$mpfr_version
mkdir build
cd build
../configure \
    --prefix=$build_dep_dir \
    --build=$build \
    --disable-shared \
    --enable-static \
    --with-gmp=$build_dep_dir

make -j8
make install

__banner Preparing mpc
cd $source_dir/mpc-$mpc_version
mkdir build
cd build
../configure \
    --prefix=$build_dep_dir \
    --build=$build \
    --disable-shared \
    --enable-static \
    --with-gmp=$build_dep_dir \
    --with-mpfr=$build_dep_dir

make -j8
make install

__banner Preparing isl
cd $source_dir/isl-$isl_version
mkdir build
cd build
../configure \
    --prefix=$build_dep_dir \
    --build=$build \
    --disable-shared \
    --enable-static \
    --enable-portable-binary \
    --with-gmp_prefix=$build_dep_dir

make -j8
make install

#======================================================================
# Configure binutils
#======================================================================
__banner Configuring binutils source code

cd $source_dir/binutils-$binutils_version
mkdir build
cd build

CC=gcc
CXX=g++
OPT_FLAGS="-O2 -Wall"
CC="$CC" CXX="$CXX" CFLAGS="$OPT_FLAGS" \
    CXXFLAGS="`echo " $OPT_FLAGS " | sed 's/ -Wall / /g'`" \
    ../configure \
    --prefix=$install_dir \
    --build=$build \
    --host=$build \
    --target=riscv64-elf \
    --with-gmp=$build_dep_dir \
    --with-mpfr=$build_dep_dir \
    --with-mpc=$build_dep_dir \
    --enable-lto \
    --enable-multilib \
    --disable-shared \
    --enable-static

#======================================================================
# Compiling binutils
#======================================================================
__banner Compiling binutils
make -j8

#======================================================================
# Installing binutils
#======================================================================
__banner Installing binutils
make install

#======================================================================
# Configure gcc
#======================================================================
__banner Configuring gcc source code

cd $source_dir/gcc-$gcc_version
mkdir build
cd build

$source_dir/gcc-$gcc_version/gcc/config/riscv/multilib-generator \
    $multilib_list > $source_dir/gcc-$gcc_version/gcc/config/riscv/t-elf-multilib

CC=gcc
CXX=g++
OPT_FLAGS="-O2 -Wall"
CC="$CC" CXX="$CXX" CFLAGS="$OPT_FLAGS" \
    CXXFLAGS="`echo " $OPT_FLAGS " | sed 's/ -Wall / /g'`" \
    ../configure \
    --prefix=$install_dir \
    --build=$build \
    --host=$build \
    --target=riscv64-elf \
    --with-gmp=$build_dep_dir \
    --with-mpfr=$build_dep_dir \
    --with-mpc=$build_dep_dir \
    --with-isl=$build_dep_dir \
    --enable-checking=release \
    --enable-multilib \
    --with-pkgversion="$packageversion" \
    --enable-languages=c,c++,lto \
    --disable-shared \
    --disable-gcov \
    --disable-nls \
    --enable-static \
    --without-libicolw-prefix \
    --disable-libstdcxx \
    --disable-libstdcxx-pch \
    --disable-libada \
    --disable-libssp \
    --disable-host-shared \
    --with-specs='%{!mcmodel=*:-mcmodel=medany}'

#======================================================================
# Compiling gcc
#======================================================================
__banner Compiling gcc
make -j8

#======================================================================
# Installing gcc
#======================================================================
__banner Installing gcc
make install

#======================================================================
# Configure gdb
#======================================================================
__banner Configuring gdb

cd $source_dir/gdb-$gdb_version
mkdir build
cd build

CC=gcc
CXX=g++
OPT_FLAGS="-O2 -Wall"
CC="$CC" CXX="$CXX" CFLAGS="$OPT_FLAGS" \
    CXXFLAGS="`echo " $OPT_FLAGS " | sed 's/ -Wall / /g'`" \
    ../configure \
    --prefix=$install_dir \
    --build=$build \
    --host=$build \
    --target=riscv64-linux \
    --with-gmp=$build_dep_dir \
    --with-mpfr=$build_dep_dir \
    --with-mpc=$build_dep_dir \
    --enable-lto \
    --enable-multilib \
    --disable-shared \
    --enable-static \
    --with-python=no

#======================================================================
# Compiling gdb
#======================================================================
__banner Compiling gdb
make -j8

#======================================================================
# Installing gdb
#======================================================================
__banner Installing gdb
make install

#======================================================================
# Post build (disabled)
#======================================================================
# Create a shell script that users can source to bring GCC into shell
# environment
# cat << EOF > $install_dir/elw.sh
# # source this script to bring gcc $gcc_version into your environment
# export PATH=$install_dir/bin:\$PATH
# export LD_LIBRARY_PATH=$install_dir/lib:$install_dir/lib64:\$LD_LIBRARY_PATH
# export MANPATH=$install_dir/share/man:\$MANPATH
# export INFOPATH=$install_dir/share/info:\$INFOPATH
# EOF

__banner Complete

trap : 0

#end
