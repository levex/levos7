#!/usr/bin/python

import string
import struct
#import bytearray

mapFile = open("kernel.map.small")
outputFile = open("kernel.map.o", "w+")

# newFileByteArray = bytearray(newFileBytes)
# newFile.write(newFileByteArray)

for line in mapFile:
    line = line.split(" ")
    func = line[2];
    #print("function: " + str(len(func)) +":" + func);
    offset = string.atoi(line[1], 16)
    #print("offset: " + str(offset))
    padding = (32 - len(func))
    if (padding < 0):
        print "FATAL ERROR"
        exit(1)

    outputFile.write(struct.pack('I', offset))
    outputFile.write(bytearray(func, "ascii"))
    outputFile.write(bytearray(padding))

