#!/usr/bin/python

import sys

finalstr = ""

while True:
    line = sys.stdin.readline()
    if not line:
        break
    ll = line.split("=")
    ll[0] = ll[0].strip()
    ll[1] = ll[1].strip()
    if not ll[0].startswith("#") and not ll[1] == "n":
        finalstr += " -D" + ll[0] + "=" + ll[1]

print finalstr.strip()
