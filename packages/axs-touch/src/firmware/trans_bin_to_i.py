#!/usr/bin/env python3
#encoding: utf-8
import sys, os 
i = 0
outFile = open('firmware_flash.i','w') 
fileName = "./firmware_flash.bin"
inFile = open(fileName,'rb') 
outstr = ""
ch = inFile.read(1)
count = 0
while ch:
	data = ord(ch)
	if count<255:
		outstr+="0x%02X,"%data
	else:
		outstr+="0x%02X,"%data
	ch = inFile.read(1)
	count += 1
	if count == 256:
		outstr+="\n"
		outFile.write(outstr)
		count = 0
		outstr = ""

outFile.write(outstr)
inFile.close() 
outFile.close() 
