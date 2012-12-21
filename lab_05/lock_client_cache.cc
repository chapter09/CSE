// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

_lock_release_user::_lock_release_user(class extent_client *_ec)
:ec(_ec)
{
}

void
_lock_release_user::dorelease(lock_protocol::lockid_t lid)
{
	VERIFY(ec->flush(lid) == extent_protocol::OK);
}

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
	lock_protocol::status ret;
	pthread_mutex_lock(&mutex);
	
	if(cache_map.count(lid) == 0) {
		//the lock is not cached in client
		//need to request from the server
		printf("[c] %s acquire [l] %016llx not exist\n", 
			this->id.c_str(), lid);
		cache_map[lid] = ACQUIRING;
		info_map[lid] = false;	//lock is not used yet

		if(cv_map.count(lid) == 0) {
			pthread_cond_t lock_cv;
			cv_map[lid] = lock_cv;				
		} 
		VERIFY(pthread_cond_init(&cv_map[lid], NULL) == 0);	
		pthread_mutex_unlock(&mutex);
		ret = cl->call(lock_protocol::acquire, lid, this->id, r);
		pthread_mutex_lock(&mutex);

		if(ret == lock_protocol::OK) {
			//ret == OK
			printf("acquire OK status %d\n", cache_map[lid]);
			if(cache_map[lid] != RELEASING) {
				cache_map[lid] = LOCKED;
			}
			info_map[lid] = true;
			pthread_mutex_unlock(&mutex);
			return lock_protocol::OK;
		} else if(ret == lock_protocol::RPCERR) {
			pthread_mutex_unlock(&mutex);
			return ret;
		}
	}

	printf("[l] %016llx [RETRYING] [c] %s wait for lock\n", 
				lid, this->id.c_str());

	while(cache_map[lid] != FREE) {
		if(cache_map[lid] == NONE) {
			printf("acquire remote %s\n", this->id.c_str());
			cache_map[lid] = ACQUIRING;
			pthread_mutex_unlock(&mutex);
			ret = cl->call(lock_protocol::acquire, lid, this->id, r);
			pthread_mutex_lock(&mutex);
			if(ret == lock_protocol::OK) {
				if(cache_map[lid] != RELEASING) {
					cache_map[lid] = LOCKED;
				}
				info_map[lid] = true;
				pthread_mutex_unlock(&mutex);
				return ret;
			} else if(ret == lock_protocol::RPCERR) {
				pthread_mutex_unlock(&mutex);
				return ret;
			}
		}

		if(cache_map[lid] == RELEASING 
			&& info_map[lid] == false) {
			info_map[lid] = true;
			pthread_mutex_unlock(&mutex);
			return lock_protocol::OK;
		}

		if(cache_map[lid] == FREE) {
			break;
		} else if(cache_map[lid] == NONE) {
			continue;
		} else {
			printf("acquire sleep %s\n", this->id.c_str());
			pthread_cond_wait(&cv_map[lid], &mutex);
		}
		printf("wake up %s\n", this->id.c_str());
	}

	cache_map[lid] = LOCKED;
	info_map[lid] = true;
	pthread_mutex_unlock(&mutex);
	return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
	printf("[release]\n");
	int r;
	pthread_mutex_lock(&mutex);
	if(info_map.count(lid) != 0) {
		printf("lock_status %d\n", cache_map[lid]);

		if(cache_map[lid] == RELEASING) {
			printf("release to remote %s\n", this->id.c_str());
			lu->dorelease(lid);
			pthread_mutex_unlock(&mutex);
			cl->call(lock_protocol::release, lid, id, r);
			pthread_mutex_lock(&mutex);
			cache_map[lid] = NONE;
			info_map[lid] = false;
		} else {
			cache_map[lid] = FREE;
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
			pthread_mutex_unlock(&mutex);
			return rlock_protocol::OK;
		}

		if(cache_map[lid] == FREE && info_map[lid] == true) {
			cache_map[lid] = NONE;
			info_map[lid] = false;
			int r;
			lu->dorelease(lid);
			pthread_mutex_unlock(&mutex);
			cl->call(lock_protocol::release, lid, id, r);
			return rlock_protocol::OK;
		} else {
			cache_map[lid] = RELEASING;
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
		if(cache_map[lid] != RELEASING) {
			cache_map[lid] = FREE;
		}
		pthread_cond_signal(&cv_map[lid]);

	}
	pthread_mutex_unlock(&mutex);

	return rlock_protocol::OK;
}
