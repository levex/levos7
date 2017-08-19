#!/bin/sh

dir=$0
name=$1

diff -uNr --exclude 'autom4te.cache' --exclude 'Makefile.in' --exclude 'aclocal.m4' --exclude 'configure' --exclude 'gmp*' --exclude 'isl*' --exclude 'mpfr*' --exclude 'mpc*' . ${dir} > ${name}.patch
