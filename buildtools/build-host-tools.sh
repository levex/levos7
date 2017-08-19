#!/bin/sh

newlib_ver=newlib-2.5.0.20170323
binutils_ver=binutils-2.27
gcc_ver=gcc-6.2.0

set -e

######## Prepare

echo Creating build directory...

[[ -e host-tools ]] && rm -rf host-tools

mkdir host-tools/
mkdir host-tools/src 
mkdir host-tools/build
mkdir host-tools/prefix
mkdir host-tools/am-prefix
mkdir ../sysroot/ || true

host_tools=`pwd`/host-tools
levos_sysroot=`pwd`/../sysroot

######## autoconf-2.64 the big crap
[[ ! -e host-tools/src/autoconf-2.64.tar.gz ]] && \
	wget https://ftp.gnu.org/gnu/autoconf/autoconf-2.64.tar.gz -O host-tools/src/autoconf-2.64.tar.gz
cd host-tools/src/
tar xzvf autoconf-2.64.tar.gz
cd autoconf-2.64
./configure --prefix=${host_tools}/am-prefix
make
make install

PATH=${host_tools}/am-prefix/bin:$PATH
#PATH=${host_tools}/src/autoconf-2.64/bin:$PATH

######## automake-1.12
cd ${host_tools}/..
[[ ! -e host-tools/src/automake-1.12.tar.gz ]] && \
	wget https://ftp.gnu.org/gnu/automake/automake-1.12.tar.gz -O host-tools/src/automake-1.12.tar.gz
cd host-tools/src/
tar xzvf automake-1.12.tar.gz
cd automake-1.12
./configure --prefix=${host_tools}/am-prefix
make
make install

export PATH=${host_tools}/am-prefix/bin:$PATH
ln -s ${host_tools}/am-prefix/share/aclocal-1.12 ${host_tools}/am-prefix/share/aclocal
ln -s ${host_tools}/am-prefix/share/automake-1.12 ${host_tools}/am-prefix/share/automake

# ######## Binutils - bare

cd ${host_tools}/..

echo Fetching binutils sources...
wget https://ftp.gnu.org/gnu/binutils/${binutils_ver}.tar.gz -O host-tools/src/${binutils_ver}.tar.gz
cd host-tools/src/

echo Extracting binutils sources...
tar xzvf ./${binutils_ver}.tar.gz

echo Applying binutils patches...
cd ${binutils_ver}
patch -p1 < ../../../patches/${binutils_ver}.patch

cd ld
aclocal
autoreconf
automake
cd ..

echo Building binutils...
cd ../../build/
mkdir binutils-bare
cd binutils-bare
../../src/${binutils_ver}/configure --prefix=${host_tools}/prefix --target=i686-levos
make all
make install


######## GCC bare

cd ${host_tools}/..

PATH=${host_tools}/prefix/bin:$PATH

echo Fetching gcc sources...
wget https://ftp.gnu.org/gnu/gcc/${gcc_ver}/${gcc_ver}.tar.gz -O host-tools/src/${gcc_ver}.tar.gz
cd host-tools/src/

echo Extracting gcc sources...
tar xzvf ./${gcc_ver}.tar.gz

echo Applying gcc patches...
cd ${gcc_ver}
patch -p1 < ../../../patches/${gcc_ver}.patch

cd libstdc++-v3
autoconf
cd ..

echo Building gcc...
cd ../../build
mkdir gcc-bare
cd gcc-bare
../../src/${gcc_ver}/configure --target=i686-levos --prefix=${host_tools}/prefix --enable-languages=c,c++ --without-nls

make all-gcc
make all-target-libgcc
make install-gcc
make install-target-libgcc

######## Newlib

cd ${host_tools}/..
echo Fetching newlib sources...
[[ ! -e host-tools/src/${newlib_ver}.tar.gz ]] && \
	wget http://download.openpkg.org/components/cache/newlib/${newlib_ver}.tar.gz -O host-tools/src/${newlib_ver}.tar.gz
cd host-tools/src/

echo Extracting newlib sources...
tar xzvf ./${newlib_ver}.tar.gz

echo Applying newlib patches...
cd ${newlib_ver}
#patch -p1 < ../../../patches/${newlib_ver}.patch

cd newlib/libc/sys
# libtoolize --force
# ${host_tools}/am-prefix/bin/aclocal
# cd ..
# ${host_tools}/am-prefix/bin/aclocal
# cd ..
# ${host_tools}/am-prefix/bin/aclocal
# ${host_tools}/am-prefix/bin/automake --force-missing --add-missing
# ${host_tools}/src/autoconf-2.64/bin/autoconf
# cd libc/sys
#${host_tools}/src/autoconf-2.64/bin/autoconf

cd ../../../
patch -p1 < ../../../patches/${newlib_ver}.patch

cd newlib/libc/sys
${host_tools}/src/autoconf-2.64/bin/autoconf

cd levos
${host_tools}/src/autoconf-2.64/bin/autoreconf

echo Building newlib...
cd ${host_tools}/build
mkdir build-newlib
cd build-newlib
../../src/${newlib_ver}/configure --prefix=/usr --target=i686-levos
make all
make DESTDIR=${levos_sysroot} install

echo Tools for Compiling Helwyr/LevOS7 are ready to be used!
