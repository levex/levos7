#!/bin/sh

echo Welcome to Helwyr/LevOS7 disk composer!

mkdir compose       || true
mkdir compose/src   || true
mkdir compose/build || true
mkdir compose/root  || true

_buildtools=`pwd`
_ncores=`./determine_cores.sh`
_compose=${_buildtools}/compose
_compose_root=${_compose}/root
_compose_image_name=compose

set -e

dash_ver=dash-0.5.9.1
build_dash() {
	# fetch dash
	cd ${_compose}/src
	[[ ! -e ${dash_ver}.tar.gz ]] && \
		wget http://gondor.apana.org.au/~herbert/dash/files/${dash_ver}.tar.gz \
			-O ${_compose}/src/${dash_ver}.tar.gz
	tar xzvf ${dash_ver}.tar.gz

	# patch dash
	cd ${dash_ver}
	patch -p1 < ${_buildtools}/patches/${dash_ver}.patch

	# build dash
	cd ${_compose}/build
	[[ -d build-dash ]] && \
		rm -rf build-dash
	mkdir build-dash
	cd build-dash

	../../src/${dash_ver}/configure	\
		--host=i686-levos \
		--enable-static \
		--prefix=/usr

	make -j${_ncores}
	make DESTDIR=${_compose_root} install

	echo Dash installed to compose root
}

binutils_ver=binutils-2.27
build_binutils() {
	# fetch dash

	cd ${_compose}/build
	[[ -d build-binutils ]] && \
		rm -rf build-binutils
	mkdir build-binutils
	cd build-binutils

	${_buildtools}/host-tools/src/${binutils_ver}/configure \
		--target=i686-levos \
		--host=i686-levos \
		--disable-nls \
		--with-sysroot=${_buildtools}/../sysroot \
		--disable-werror \
		--prefix=/usr

	make -j${_ncores}
	make DESTDIR=${_compose_root} install

	echo Binutils installed to compose root
}

build_l_coreutils() {
	for prog in cat date echo getty ls mkdir uname wc udp tcp internet irc irc_bot
	do
		i686-levos-gcc \
			${_buildtools}/../userspace/simple/$prog.c \
			-o ${_compose_root}/usr/bin/$prog
	done
}

read -p "Do you want dash, a POSIX-compatible shell? " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]
then
    build_dash
fi

read -p "Do you want binutils (assembler, readelf, etc. required for gcc)? " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]
then
    build_binutils
fi

read -p "Do you want some coreutils clones (ls, mkdir, cat, and so on)? " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]
then
    build_l_coreutils
fi

read -p "What should be copied to /init? [/usr/bin/dash is an ideal choice, leave empty if none] " -r
[[ ! -z "$REPLY" ]] && \
	cp ${_compose_root}$REPLY ${_compose_root}/init

composed_block_usage=`du ${_compose_root} | tail -n 1 | awk '{ print $1 }' | xargs echo 1.10 \* | bc`

read -p "Should I generate a disk image ${composed_block_usage} ? " -n 1 -r
[[ ! -z "$REPLY" ]] && \
	genext2fs \
		--root ${_compose_root} \
		-b `echo ${composed_block_usage} / 1 | bc` \
		${_compose}/${_compose_image_name}.img

echo disk root in ${_compose_root}
