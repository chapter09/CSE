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

//void
//lock_client_cache::to_status(int stat)
//{
//    pthread_mutex_lock(&mutex);
//    cl_status = stat;
//    pthread_mutex_unlock(&mutex);
//}

bool 
lock_client_cache::is_lock_cached(lock_protocol::lockid_t lid)
{
	if(cache_map[lid] == NONE) {
		return false;
	} else { return true; }
}


bool
lock_client_cache::is_locked(lock_protocol::lockid_t lid)
{
	if(cache_map[lid] != FREE) {
		return true;
	} else { return false; }
}


lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
	//check the revoke/retry
	int r;

	pthread_mutex_lock(&mutex);
	if(is_lock_cached(lid)) {
		//the lock is cached in client
		printf("lock %016llx has been cached in the client %s\n", lid, this->id.c_str());
		to_lock(lid);
		pthread_mutex_unlock(&mutex);
		return lock_protocol::OK;
	} else {
		//the lock is not cached in client
		//need to request from the server
		printf("lock %016llx is new in the client %s\n", lid, this->id.c_str());
		if(cache_map[lid] == NONE) {
			cache_map[lid] = ACQUIRING;
			pthread_mutex_unlock(&mutex);

			lock_protocol::status ret;
			ret = cl->call(lock_protocol::acquire, lid, id, r);

			if(ret == lock_protocol::OK) { 
				//ret == OK
				pthread_mutex_lock(&mutex);
				pthread_cond_t lock_cv;
				VERIFY(pthread_cond_init(&lock_cv, NULL) == 0);	
				cv_map[lid] = lock_cv;				

				cache_map[lid] = LOCKED;
				pthread_mutex_unlock(&mutex);
				printf("acquire return OK in client %s\n", this->id.c_str());
				return lock_protocol::OK;
			} else if(ret == lock_protocol::RETRY) {
				//or ret == RETRY
				pthread_mutex_lock(&mutex);
				if(info_map[lid].is_retried) {
					printf("lock %016llx [RETRY is_retried] client %s\n", lid, this->id.c_str());
					//if the retry() is called before the RETRY arrives
					cache_map[lid] = LOCKED;
					//need to set is_retried back to false?	
					pthread_mutex_unlock(&mutex);
					return lock_protocol::OK;
				} else {
					printf("lock %016llx [RETRY ] client %s\n", lid, this->id.c_str());
					//normal sequence
					to_lock(lid);
					//printf("%d get the signal\n", pthread_self());
					pthread_mutex_unlock(&mutex);
					return lock_protocol::OK;
				}
			}
		} else if(cache_map[lid] == ACQUIRING) {
			to_lock(lid);
			pthread_mutex_unlock(&mutex);
			return lock_protocol::OK;
		} else {
			printf("else is %d\n", cache_map[lid]);
			pthread_mutex_unlock(&mutex);
			return lock_protocol::OK;
		}
	}
	return lock_protocol::OK;
}


void
lock_client_cache::to_lock(lock_protocol::lockid_t lid)
{
	if(is_locked(lid)){
		printf("\tclient %s wait for the signal\n", this->id.c_str());
		pthread_cond_wait(&cv_map[lid], &mutex);
		//printf("%d get the signal\n", pthread_self());
		printf("\tclient %s get lock\n", this->id.c_str());
		cache_map[lid] = LOCKED;
	}else{
		printf("\tclient %s lock is FREE\n", this->id.c_str());
		cache_map[lid] = LOCKED;
	}
}


lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
	int r;
	int ret = lock_protocol::OK;
	pthread_mutex_lock(&mutex);
	if(info_map[lid].is_revoked) {	
		printf("[release is_revoked=true] lock_status is %d\n", cache_map[lid]);
		//call server to release
		//delete the cache record under the client
		if(cache_map[lid] == LOCKED) {
			cache_map[lid] = RELEASING;
			pthread_mutex_unlock(&mutex);

			pthread_mutex_lock(&mutex);
			info_map.erase(lid);
			cache_map[lid] = NONE;
			pthread_mutex_unlock(&mutex);
			
			ret = cl->call(lock_protocol::release, lid, id, r);
			VERIFY (ret == lock_protocol::OK);
			return lock_protocol::OK;
		} else {
			//unknown condition before test
			printf("[release] lock_status is %d\n", cache_map[lid]);
			pthread_mutex_unlock(&mutex);
			return lock_protocol::OK;
		}		

	} else {
		printf("release request from thread %s\n", this->id.c_str());
		cache_map[lid] = FREE;
		
		printf("[send signal] thread %s\n", this->id.c_str());

		pthread_cond_signal(&cv_map[lid]);
		pthread_mutex_unlock(&mutex);
		return lock_protocol::OK;
	}
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
		int &)
{
	printf("\t%s revoke lock %lld status %d\n", this->id.c_str(), lid, cache_map[lid]);
	pthread_mutex_lock(&mutex);
	printf("\t%s revoke lock %lld status %d\n", this->id.c_str(), lid, cache_map[lid]);
	if(cache_map[lid] != NONE){
		//the lock has been cached on the client
		if(cache_map[lid] != FREE){
			printf("\t%s revoke lock %lld exist LOCKED\n", this->id.c_str(), lid);
			/*
			 *if the lock is held by one of the thread
			 *set is_revoked true
			 *release func will check is_revoked, if true return lock
			 *to server
			 */
			info_map[lid].is_revoked = true;		
			pthread_mutex_unlock(&mutex);
			return rlock_protocol::OK;
		} else {
			printf("\t%s revoke lock %lld exist OFF\n", this->id.c_str(), lid);
			//if the lock is free at the client
			//the lock will be brought back to server
			lock_protocol::status ret;
			int r;
			
			info_map.erase(lid);
			cache_map[lid] = NONE;
			pthread_mutex_unlock(&mutex);
			printf("\t%s call server release lock %lld exist OFF\n", this->id.c_str(), lid);
			ret = cl->call(lock_protocol::release, lid, id, r);
			return ret;
		}
	} else {
		printf("\t%s revoke lock %lld no exist\n", this->id.c_str(), lid);
		//if the revoke rpc comes before the lock arrives
		//add lock
		info_map[lid].is_revoked = true;		
		pthread_mutex_unlock(&mutex);
		return rlock_protocol::OK;
	}
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
		int &)
{
	//retry will cause a signal to wake up the acquire lock
	int ret = rlock_protocol::OK;
	
	printf("retry lock %lld client %s\n", lid, this->id.c_str());
	pthread_mutex_lock(&mutex);
	if(cache_map[lid] != NONE){
		
		info_map[lid].is_retried = true;
		cache_map[lid] = LOCKED;
		pthread_cond_signal(&cv_map[lid]);
		pthread_mutex_unlock(&mutex);

	} else {
		//if the retry rpc comes before the lock arrives
		info_map[lid].is_retried = true;		
		pthread_mutex_unlock(&mutex);

	}
	return ret;
}
