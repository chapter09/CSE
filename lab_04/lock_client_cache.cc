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

acquire:
	if(cache_map[lid] != NONE) {
		//the lock is cached in client
		printf("lock %016llx has been cached in the client %s\n", lid, this->id.c_str());
		if(to_lock(lid)) {
			goto acquire;
		}
		pthread_mutex_unlock(&mutex);
		return lock_protocol::OK;
	} else {
		//the lock is not cached in client
		//need to request from the server
		printf("lock %016llx client %s acquire NONE\n", lid, this->id.c_str());
		cache_map[lid] = ACQUIRING;
		if(cv_map.find(lid) == cv_map.end()) {
			pthread_cond_t lock_cv;
			cv_map[lid] = lock_cv;				
		} 
		VERIFY(pthread_cond_init(&cv_map[lid], NULL) == 0);	
		pthread_mutex_unlock(&mutex);

		lock_protocol::status ret;
		ret = cl->call(lock_protocol::acquire, lid, id, r);
		pthread_mutex_lock(&mutex);
		
		if(ret == lock_protocol::OK) { 
			//ret == OK
			printf("acquire OK status %d\n", cache_map[lid]);
			printf("acquire OK+ACQ [c] %s [t] %u\n", this->id.c_str(), (unsigned int) pthread_self());
			cache_map[lid] = LOCKED;
			pthread_mutex_unlock(&mutex);
			return lock_protocol::OK;
		} else if(ret == lock_protocol::RETRY) {
			cache_map[lid] = RETRYING;
			if(!info_map[lid].is_retried) {
				printf("lock %016llx [RETRYING] client %s\n", lid, this->id.c_str());
				//normal sequence
				if(to_lock(lid)) {
					goto acquire;
				}
				//printf("%d get the signal\n", pthread_self());
				pthread_mutex_unlock(&mutex);
				return lock_protocol::OK;
			} else {
				cache_map[lid] = LOCKED;
				pthread_mutex_unlock(&mutex);
				return lock_protocol::OK;
			}
		}
	}
	return lock_protocol::OK;
}

bool
lock_client_cache::to_lock(lock_protocol::lockid_t lid)
{
	if(cache_map[lid] != FREE){
		printf("\tclient %s thread %u wait for the signal\n", this->id.c_str(), (unsigned int) pthread_self());
		while(1) {
			pthread_cond_wait(&cv_map[lid], &mutex);
			printf("wait again\n");
			if(cache_map[lid] == FREE) {
				break;
			} else if(cache_map[lid] == NONE) {
				return true;	
			}
		}
		printf("\tclient %s get lock\n", this->id.c_str());
		cache_map[lid] = LOCKED;
	}else{
		printf("\tclient %s lock is FREE and got lock\n", this->id.c_str());
		cache_map[lid] = LOCKED;
	}
	info_map[lid].is_retried = false;
	return false;
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
		cache_map[lid] = RELEASING;
		pthread_mutex_unlock(&mutex);
		
		ret = cl->call(lock_protocol::release, lid, id, r);
	
		pthread_mutex_lock(&mutex);
		if(ret == lock_protocol::OK) {
			info_map.erase(lid);
			cache_map[lid] = NONE;
		}
		pthread_mutex_unlock(&mutex);
		return lock_protocol::OK;
	} else {
		printf("release request from thread %s\n", this->id.c_str());
		cache_map[lid] = FREE;
		info_map.erase(lid);
		pthread_cond_signal(&cv_map[lid]);
		printf("[send signal out] thread %s\n", this->id.c_str());
		pthread_mutex_unlock(&mutex);
		return lock_protocol::OK;
	}
}


rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
		int &)
{
	pthread_mutex_lock(&mutex);
	if(cache_map[lid] == FREE && !info_map[lid].is_retried) {
		int r;

		lock_protocol::status ret;
		info_map.erase(lid);
		cache_map[lid] = NONE;
		pthread_mutex_unlock(&mutex);
		printf("\t%s call server release lock %lld exist OFF\n", this->id.c_str(), lid);
		ret = cl->call(lock_protocol::release, lid, id, r);
		return ret;
	} else {
		printf("\t%s revoke lock %lld exist LOCKED [s] %d\n", this->id.c_str(), lid, cache_map[lid]);

		info_map[lid].is_revoked = true;		
		pthread_mutex_unlock(&mutex);
		return rlock_protocol::OK;
	}
}
	//   if(cache_map[lid] != FREE || info_map[lid].is_retried) {
	//        printf("\t%s revoke lock %lld exist LOCKED\n", this->id.c_str(), lid);
			/*
			 *if the lock is held by one of the thread
			 *set is_revoked true
			 *release func will check is_revoked, if true return lock
			 *to server
			 */
	//        info_map[lid].is_revoked = true;		
	//        pthread_mutex_unlock(&mutex);
	//        return rlock_protocol::OK;
	//    } else {
	//        printf("\t%s revoke lock %lld exist OFF\n", this->id.c_str(), lid);
	//        //if the lock is free at the client
	//        //the lock will be brought back to server
	//        lock_protocol::status ret;
	//        int r;
			
	//        info_map.erase(lid);
	//        cache_map[lid] = NONE;
	//        pthread_mutex_unlock(&mutex);
	//        printf("\t%s call server release lock %lld exist OFF\n", this->id.c_str(), lid);
	//        ret = cl->call(lock_protocol::release, lid, id, r);
	//        return ret;
	//    }
	//} else {
	//    printf("\t%s revoke lock %lld no exist\n", this->id.c_str(), lid);
	//    //if the revoke rpc comes before the lock arrives
	//    //add lock
	//    info_map[lid].is_revoked = true;		
	//    pthread_mutex_unlock(&mutex);
	//    return rlock_protocol::OK;
	//}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
		int &)
{
	//retry will cause a signal to wake up the acquire lock
	rlock_protocol::status ret;
	pthread_mutex_lock(&mutex);

	if(cache_map[lid] == RETRYING){
		printf("[retry after RETRY]lock %lld client %s\n", lid, this->id.c_str());
		cache_map[lid] = FREE;
		info_map[lid].is_retried = true;		
		pthread_cond_signal(&cv_map[lid]);
		pthread_mutex_unlock(&mutex);
		ret = rlock_protocol::OK;
	} else {
		printf("[retry before RETRY]lock %lld client %s\n", lid, this->id.c_str());
		//if the retry rpc comes before the lock arrives
		info_map[lid].is_retried = true;		
		pthread_mutex_unlock(&mutex);
		ret = rlock_protocol::OK;
		switch(cache_map[lid]) {
			case ACQUIRING:
				printf("retry ACQ [c] %s [t] %u\n", this->id.c_str(), (unsigned int) pthread_self());
				cache_map[lid] = LOCKED;
				break;
			case FREE:
				printf("retry FREE [c] %s [t] %u\n", this->id.c_str(), (unsigned int) pthread_self());
				break;
			case LOCKED:
				printf("retry LOC [c] %s [t] %u\n", this->id.c_str(), (unsigned int) pthread_self());
				break;
		}
	}
	return ret;
}
