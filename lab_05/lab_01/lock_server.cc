// the lock server implementation
//Todo: 1. ~lcok_server(); 2. the loop for the signal waiting thread.
#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <map>

#define TIMEOUT true

lock_server::lock_server():
  nacquire (0)
{
	VERIFY(pthread_mutex_init(&mutex, NULL) == 0);
	VERIFY(pthread_cond_init(&cv, NULL) == 0);	
}

lock_server::~lock_server()
{
	VERIFY(pthread_mutex_destroy(&mutex) == 0);
	VERIFY(pthread_cond_destroy(&cv) == 0);	
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
  	//printf("acquire request from clt %d\n", clt);
	locker(clt, lid);
	nacquire += 1;
	r = nacquire;
	return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  	lock_protocol::status ret = lock_protocol::OK;
  	printf("release request from clt %d\n", clt);
	pthread_mutex_lock(&mutex);
	
	table_lk[lid] = FREE;
	nacquire -= 1;
	r = nacquire;
  	printf("[send signal] thread %d\n", clt);
	
	pthread_cond_signal(&cv);
	pthread_mutex_unlock(&mutex);
	return ret;
}

bool
lock_server::is_lock_existed(lock_protocol::lockid_t lid)
{	
	if(table_lk.find(lid) == table_lk.end()) {
		return false;
	} else {
		return true;
	}
}

bool
lock_server::is_locked(lock_protocol::lockid_t lid)
{
	if(table_lk[lid] == FREE) {
		return false;
	} else {
		return true;
	}
}

void
lock_server::create_lock(int clt, lock_protocol::lockid_t lid)
{	
	printf("[create_lock %lld] thread %d\n",lid, clt);
	table_lk[lid] = LOCKED;
	printf("[create_lock %lld] thread %d unlock\n",lid, clt);
}

void
lock_server::locker(int clt, lock_protocol::lockid_t lid)
{
	pthread_mutex_lock(&mutex);
	if(is_lock_existed(lid)){
		if(is_locked(lid)){
			printf("%d wait for the signal\n", clt);
			pthread_cond_wait(&cv, &mutex);
			printf("%d get the signal\n", clt);
			create_lock(clt, lid);
			pthread_mutex_unlock(&mutex);
		}else{
			create_lock(clt, lid);
			pthread_mutex_unlock(&mutex);
		}
	}
	else{
		create_lock(clt, lid);
		pthread_mutex_unlock(&mutex);
	}
}
