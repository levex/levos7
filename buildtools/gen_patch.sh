#!/bin/sh

# generate a patch file from $0 to $1.patch

dir=$0
name=$1

diff -uNr --exclude 'autom4te.cache' --exclude 'Makefile.in' --exclude 'aclocal.m4' --exclude 'configure' . ${dir} > ${name}.patch
