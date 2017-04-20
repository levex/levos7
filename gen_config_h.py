#!/usr/bin/python

import sys

print "#ifndef __LEVOS_CONFIG_H"
print "#define __LEVOS_CONFIG_H"
print "\n"

while True:
    line = sys.stdin.readline()
    if not line:
        break
    ll = line.split("=")
    ll[0] = ll[0].strip()
    ll[1] = ll[1].strip()
    if not ll[0].startswith("#") and not ll[1] == "n":
        print("#define "+str(ll[0])+" "+str(ll[1]))

print "#endif /* __LEVOS_CONFIG_H */"
