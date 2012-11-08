// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst, 
		class lock_release_user *_lu)
: lock_client(xdst), lu(_lu)
{
	rpcs *rlsrpc = new rpcs(0);
	rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
	rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

	VERIFY(pthread_mutex_init(&mutex, NULL) == 0);
	VERIFY(pthread_mutex_init(&lock_mutex, NULL) == 0);
	VERIFY(pthread_cond_init(&lock_cv, NULL) == 0);	
	VERIFY(pthread_cond_init(&retry_cv, NULL) == 0);	
	
	const char *hname;
	hname = "127.0.0.1";
	std::ostringstream host;
	host << hname << ":" << rlsrpc->port();
	id = host.str();
}

bool 
lock_client_cache::is_lock_cached(lock_protocol::lockid_t lid)
{
	pthread_mutex_lock(&mutex);
	if(cache_map.find(lid) == cache_map.end()) {
		pthread_mutex_unlock(&mutex);
		return false;
	} else {
		pthread_mutex_unlock(&mutex);
		return true;
	}
}


lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
	//check the revoke/retry
	
	int r;
	int ret = lock_protocol::OK;

	if(is_lock_cached(lid)) {
		//the lock is cached in client
		printf("lock %016llx had been cached in the client\n");
		tolock(lid);



	} else {

		 //the lock is not cached in client
		 //need to request from the server

		ret = cl->call(lock_protocol::acquire, lid, id, r);
		pthread_mutex_unlock(&mutex);

		if(ret == lock_protocol::OK) { 
			
			//ret == OK
	
			cache_map[lid] = LOCKED;	
		} else if(ret == lock_protocol::RETRY) {
			//or ret == RETRY
			if(info_map[lid].is_retried) {
				//if the retry() is called before the RETRY arrives
		
				pthread_mutex_lock(&lock_mutex);
				printf("%d wait for the signal\n", pthread_self());
				pthread_cond_wait(&retry_cv, &lock_mutex);
				printf("%d get the signal\n", pthread_self());
			} else {
				//normal sequence
			
	
			}
		}

	}


//	pthread_mutex_unlock(&cache_mutex);
	return ret;
}

void
lock_client_cache::tolock(lock_protocol::lockid_t lid)
{
	if(cache_map[lid] == LOCKED){
		pthread_mutex_lock(&lock_mutex);
		printf("%d wait for the signal\n", pthread_self());
		pthread_cond_wait(&lock_cv, &lock_mutex);
		printf("%d get the signal\n", pthread_self());
		cache_map[lid] = LOCKED;
		pthread_mutex_unlock(&mutex);
	}else{
		pthread_mutex_lock(&mutex);
		cache_map[lid] = LOCKED;
		pthread_mutex_unlock(&mutex);
	}
}


lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
	int r;
	int ret = lock_protocol::OK;
	if(info_map[lid].is_revoked) {	

	//call server to release
	//delete the cache record under the client
			
		pthread_mutex_lock(&mutex);
		


		pthread_mutex_unlock(&mutex);
		ret = cl->call(lock_protocol::release, lid, id, r);

	} else {

		printf("release request from thread %d\n", pthread_self());
		pthread_mutex_lock(&lock_mutex);

		cache_map[lid] = FREE;
		
		printf("[send signal] thread %d\n", pthread_self());

		pthread_cond_signal(&lock_cv);
		pthread_mutex_unlock(&lock_mutex);

	}

	return lock_protocol::OK;

}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
		int &)
{
	printf("revoke lock %lld\n", lid);
	if(cache_map.find(lid) != cache_map.end()){
		//the lock has been cached on the client
		if(cache_map[lid] == LOCKED){
			/*
			 *if the lock is held by one of the thread
			 *set is_revoked true
			 *release func will check is_revoked, if true return lock
			 *to server
			 */

			info_map[lid].is_revoked = true;		


		}else{
			//if the lock is free at the client
			pthread_mutex_lock(&mutex);
			cache_map[lid] = LOCKED;
			pthread_mutex_unlock(&mutex);
		}
	} else {
		//if the revoke rpc comes before the lock arrives
		//add lock
		info_map[lid].is_revoked = true;		
		
	}
	//remove the lid in the cache_list
	int ret = rlock_protocol::OK;
	return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
		int &)
{
	//retry will cause a signal to wake up the acquire lock
	int ret = rlock_protocol::OK;
	
	if(cache_map.find(lid) != cache_map.end()){
		printf("release request from thread %d\n", pthread_self());
		pthread_mutex_lock(&lock_mutex);

		table_lk[lid] = FREE;
		
		printf("[send signal] thread %d\n", pthread_self());

		pthread_cond_signal(&lock_cv);
		pthread_mutex_unlock(&lock_mutex);
	} else {
		info_map[lid].is_retried = true;		

	}
	return ret;
}
