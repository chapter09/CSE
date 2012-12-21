#!/usr/bin/python
import os, sys

s = int(sys.argv[1])
t = int(sys.argv[2])

for i in range(0, t):

	os.system("./start.sh")
	if s == 0:	
		os.system("./test-lab-3-a.pl ./yfs1")
		os.system("./test-lab-3-b ./yfs1 ./yfs2")
		os.system("./test-lab-3-c ./yfs1 ./yfs2")
		os.system("./stop.sh")
		sys.exit()
	if s == 1: 
		os.system("./test-lab-3-a.pl ./yfs1")
		os.system("./stop.sh")
		sys.exit()
	if s == 2: 
		os.system("./test-lab-3-b ./yfs1 ./yfs2")
		os.system("./stop.sh")
		sys.exit()
	if s == 3: 
		os.system("./test-lab-3-c ./yfs1 ./yfs2")
		os.system("./stop.sh")
		sys.exit()
