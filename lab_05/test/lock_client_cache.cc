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
	//VERIFY(pthread_cond_init(&retry_cv, NULL) == 0);

	const char *hname;
	hname = "127.0.0.1";
	std::ostringstream host;
	host << hname << ":" << rlsrpc->port();
	id = host.str();
}


lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
	//check the revoke/retry
	int r;
	pthread_mutex_lock(&mutex);

	if(cache_map.count(lid) == 0) {
		printf("[l] %016llx all new [c] %s\n", lid, this->id.c_str());
		lock_protocol::status ret;
		cache_map[lid] = ACQUIRING;
		info_map[lid].is_retried = false;
		info_map[lid].is_revoked = false;
		if(cv_map.count(lid) == 0) {
			pthread_cond_t lock_cv;
			cv_map[lid] = lock_cv;				
		} 
		pthread_mutex_unlock(&mutex);
		ret = cl->call(lock_protocol::acquire, lid, this->id, r);
		pthread_mutex_lock(&mutex);
		if(ret == lock_protocol::OK) { 
			//ret == OK
			printf("acquire OK status %d\n", cache_map[lid]);
			printf("acquire OK+ACQ [c] %s [t] %u\n", 
					this->id.c_str(), (unsigned int) pthread_self());
			cache_map[lid] = LOCKED;
			pthread_mutex_unlock(&mutex);
			return lock_protocol::OK;
		}
	}

	while(cache_map[lid] != FREE) {
		//the lock is cached in client
		printf("lock %016llx has been cached in the client %s\n", 
				lid, this->id.c_str());
		if(cache_map[lid] == NONE) {
			//the lock is not cached in client
			//need to request from the server
			printf("[l] %016llx [c] %s acquire NONE\n", lid, this->id.c_str());
			cache_map[lid] = ACQUIRING;
			VERIFY(pthread_cond_init(&cv_map[lid], NULL) == 0);	
			pthread_mutex_unlock(&mutex);
			lock_protocol::status ret;
			ret = cl->call(lock_protocol::acquire, lid, id, r);
			pthread_mutex_lock(&mutex);

			if(ret == lock_protocol::OK) {
				cache_map[lid] = LOCKED;
				pthread_mutex_unlock(&mutex);
				return ret;
			} else if(ret == lock_protocol::RPCERR) {
				pthread_mutex_unlock(&mutex);
				return ret;
			}	
		}
	
		if(cache_map[lid] == FREE){
			break;
		} else if( cache_map[lid] == NONE) {
			continue;		
		} else {
			pthread_cond_wait(&cv_map[lid], &mutex);
		}
	}
	cache_map[lid] = LOCKED;
	pthread_mutex_unlock(&mutex);
	return lock_protocol::OK;
}


lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
	int r;
	pthread_mutex_lock(&mutex);
	if(info_map.count(lid) != 0) {
		if(info_map[lid].is_revoked) {	
			printf("release remote lock_status is %d\n", cache_map[lid]);
			//call server to release
			//delete the cache record under the client
			//cache_map[lid] = RELEASING;
			info_map.erase(lid);
			cache_map[lid] = NONE;
			pthread_mutex_unlock(&mutex);
			cl->call(lock_protocol::release, lid, this->id, r);
			pthread_mutex_lock(&mutex);
		} else {
			printf("release locally [t] %u [c]%s\n", 
					(unsigned int) pthread_self(), this->id.c_str());
			cache_map[lid] = FREE;
			info_map.erase(lid);
		}
		pthread_cond_signal(&cv_map[lid]);
		pthread_mutex_unlock(&mutex);
	}
	return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
		int &)
{
	printf("[c] %s revoke\n", this->id.c_str());
	pthread_mutex_lock(&mutex);
	if(cache_map.count(lid) != 0) {
		if(cache_map[lid] == NONE) {
			printf("\t%s revoke lock %lld exist NONE [s] %d\n", 
					this->id.c_str(), lid, cache_map[lid]);
			pthread_mutex_unlock(&mutex);
			return rlock_protocol::OK;
		}
		if(cache_map[lid] == FREE && info_map.count(lid) != 0) {
			int r;

			lock_protocol::status ret;
			info_map.erase(lid);
			cache_map[lid] = NONE;
			pthread_cond_signal(&cv_map[lid]);
			pthread_mutex_unlock(&mutex);
			printf("\t%s call server release lock %lld exist OFF\n", 
					this->id.c_str(), lid);
			ret = cl->call(lock_protocol::release, lid, id, r);
			return ret;
		} else {
			info_map[lid].is_revoked = true;		
		}
	}
	pthread_mutex_unlock(&mutex);
	return rlock_protocol::OK;
}


rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
		int &)
{
	//retry will cause a signal to wake up the acquire lock
	pthread_mutex_lock(&mutex);
	if(cache_map.count(lid) != 0) {
		if(!info_map[lid].is_revoked) {
			cache_map[lid] = FREE;
		}
		pthread_cond_signal(&cv_map[lid]);
	}
	pthread_mutex_unlock(&mutex);
	return rlock_protocol::OK;
}
