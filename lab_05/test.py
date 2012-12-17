#!/usr/bin/python

import sys,os


test_id = int(sys.argv[1])
t = int(sys.argv[2])
os.putenv("RPC_LOSSY", "5")

for i in range(0, t):
	os.system("killall lock_server")
	os.system("./lock_server 3772 > server.log &")
	if test_id: 
		os.system("./lock_tester 3772 %d"%test_id)
	else:
		os.system("./lock_tester 3772")
	print "="*30
