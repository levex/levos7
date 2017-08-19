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

######## Binutils

echo Fetching binutils sources...
[[ ! -e ${cross_compiler}/src/${binutils_ver}.tar.gz ]] && \
	wget https://ftp.gnu.org/gnu/binutils/${binutils_ver}.tar.gz \
		-O ${cross_compiler}/src/${binutils_ver}.tar.gz
cd ${cross_compiler}/src

tar xzvf ${binutils_ver}.tar.gz

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

make -j${_ncores}
make install 

######## GCC

echo Fetching GCC sources...

[[ ! -e ${cross_compiler}/src/${gcc_ver}.tar.gz ]] && \
	wget https://ftp.gnu.org/gnu/gcc/${gcc_ver}/${gcc_ver}.tar.gz \
		-O ${cross_compiler}/src/${gcc_ver}.tar.gz
cd ${cross_compiler}/src

tar xzvf ${gcc_ver}.tar.gz

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

make -j${_ncores} all-gcc
make -j${_ncores} all-target-libgcc
make install-gcc
make install-target-libgcc

echo Success, all done.
