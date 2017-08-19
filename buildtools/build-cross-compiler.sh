#!/bin/sh

binutils_ver=binutils-2.27
gcc_ver=gcc-6.2.0

set -e

######## Prepare
mkdir cross-compiler/
mkdir cross-compiler/src
mkdir cross-compiler/build
mkdir cross-compiler/prefix

cross_compiler=`pwd`/cross-compiler
_ncores=`./determine_cores.sh`

if [[ $IS_TRAVIS == 1 ]]
then exec 3>/dev/null
else exec 3>&1
fi

######## Binutils

echo Fetching binutils sources...
[[ ! -e ${cross_compiler}/src/${binutils_ver}.tar.gz ]] && \
	wget https://ftp.gnu.org/gnu/binutils/${binutils_ver}.tar.gz \
		-O ${cross_compiler}/src/${binutils_ver}.tar.gz
cd ${cross_compiler}/src

tar xzvf ${binutils_ver}.tar.gz 1>/dev/null 2>&1

echo Building binutils
cd ${cross_compiler}/build
mkdir build-binutils
cd build-binutils
../../src/${binutils_ver}/configure \
	--prefix=${cross_compiler}/prefix \
	--target=i686-elf \
	--with-sysroot \
	--disable-nls \
	--disable-werror

echo making binutils
make -j${_ncores} >&3
make install 

######## GCC

echo Fetching GCC sources...

[[ ! -e ${cross_compiler}/src/${gcc_ver}.tar.gz ]] && \
	wget https://ftp.gnu.org/gnu/gcc/${gcc_ver}/${gcc_ver}.tar.gz \
		-O ${cross_compiler}/src/${gcc_ver}.tar.gz
cd ${cross_compiler}/src

tar xzvf ${gcc_ver}.tar.gz 1>/dev/null 2>&1

echo Building GCC
cd ${cross_compiler}/build
mkdir build-gcc
cd build-gcc
../../src/${gcc_ver}/configure \
	--target=i686-elf \
	--prefix=${cross_compiler}/prefix \
	--disable-nls \
	--enable-languages=c,c++ \
	--without-headers

make -j${_ncores} all-gcc >&3
make -j${_ncores} all-target-libgcc >&3
make install-gcc
make install-target-libgcc

echo Success, all done.
