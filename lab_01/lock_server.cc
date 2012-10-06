// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
	table_lk[12345] = 54321;
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  	lock_protocol::status ret = lock_protocol::OK;
  	printf("acquire request from clt %d\n", clt);
	r = 88;
	return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  	lock_protocol::status ret = lock_protocol::OK;
  	printf("release request from clt %d\n", clt);
	table_lk[lid] = FREE;
    printf("%ll", lid);
	r = table_lk[lid];
	return ret;
}


