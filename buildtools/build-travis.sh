#!/bin/sh

export PATH=/helwyr/buildtools/cross-compiler/prefix/bin:$PATH

_nproc=`/helwyr/buildtools/determine_cores.sh`

make depend
make -j${_nproc}
