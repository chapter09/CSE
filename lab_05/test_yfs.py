#!/usr/bin/python

import sys,os


t = int(sys.argv[1])
os.putenv("RPC_LOSSY", "5")

for i in range(0, t):
	os.system("./start.sh 5")
	os.system("./test-lab-3-a.pl ./yfs1")
	os.system("./test-lab-3-b.pl ./yfs1 ./yfs2")
	os.system("./test-lab-3-c.pl ./yfs1 ./yfs2")
	os.system("./stop.sh")
