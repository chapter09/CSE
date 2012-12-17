// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache()
{
	nacquire = 0;
	VERIFY(pthread_mutex_init(&mutex, NULL) == 0);
}

lock_server_cache::~lock_server_cache()
{
	VERIFY(pthread_mutex_destroy(&mutex) == 0);
}

void
lock_server_cache::print_list(lock_protocol::lockid_t lid)
{
	printf("Waiting List of %lld is:\n", lid);
	std::list<std::string>::iterator it;
	for(it = l_map[lid].begin(); it != l_map[lid].end(); it++ ) {
		printf("%s ", (*it).c_str());
	}
	printf("\n");
}

int 
lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, int &r)
{
	//check the status of lock
	pthread_mutex_lock(&mutex);
//	printf("acquire request from clt %s for lock %016llx\n", 
//		id.c_str(), lid);

	if(l_map.find(lid) == l_map.end()) {
		//lock is requested first time
		std::list<std::string> wait_list;
		wait_list.push_back(id);
		l_map[lid] = wait_list;
		pthread_mutex_unlock(&mutex);
		//printf("acquire new lock return OK : clt %s for lock %016llx\n", 
		//	id.c_str(), lid);
		return lock_protocol::OK;
	} else {
		//printf("acq exist lock from clt %s for lock %016llx\n", 
		//	id.c_str(), lid);
		//lock is on a LOCKED client
		l_map[lid].push_back(id);
	//	print_list(lid);
		std::string w_id = l_map[lid].front();
		//send retry to the last one		
		//send revoke to the first client
		if(l_map[lid].size() <= 1) { 
			pthread_mutex_unlock(&mutex);
			return lock_protocol::OK;
		} else if(l_map[lid].size() == 2) {
			pthread_mutex_unlock(&mutex);
			//printf("revoke : clt %s for lock %016llx\n", 
		//		id.c_str(), lid);
			revoke(lid, w_id);
		} else {
			pthread_mutex_unlock(&mutex);
		}
		//printf("acquire return RETRY : clt %s for lock %016llx\n", 
	//		id.c_str(), lid);
		return lock_protocol::RETRY;
	}
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
		int &r)
{
	//printf("[release] : clt %s for lock %016llx\n", id.c_str(), lid);
	lock_protocol::status ret = lock_protocol::OK;
	pthread_mutex_lock(&mutex);
	l_map[lid].pop_front();

	if (l_map[lid].size() > 0) {
		std::string h_id = l_map[lid].front();
		//print_list(lid);
		pthread_mutex_unlock(&mutex);
		retry(lid, h_id);	
		pthread_mutex_lock(&mutex);
		if(l_map[lid].size() > 1) {
			pthread_mutex_unlock(&mutex);
			revoke(lid, h_id);	
		} else {
			pthread_mutex_unlock(&mutex);
		}		
	} else {
		l_map.erase(lid);
		pthread_mutex_unlock(&mutex);
	}
	return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
	//printf("stat request\n");
	r = nacquire;
	return lock_protocol::OK;
}

int lock_server_cache::revoke(lock_protocol::lockid_t lid, std::string id)
{
	int r;
	rlock_protocol::status ret;
	//bind the client
	handle h(id);
	rpcc *cl = h.safebind();
	if(cl) {
		//printf("revoke rpc [client:%s, lock %016llx]\n", 
		//	id.c_str(), lid);
		ret = cl->call(rlock_protocol::revoke, lid, r);
	} else {
		//printf("revoke rpc faild [client:%s, lock %016llx]\n", 
		//	id.c_str(), lid);
		ret = rlock_protocol::RPCERR;
	}
	return ret;
}

int lock_server_cache::retry(lock_protocol::lockid_t lid, std::string id)
{
	int r;
	rlock_protocol::status ret;
	handle h(id);
	rpcc *cl = h.safebind();
	if(cl) {
		//printf("retry rpc [client:%s, lock %016llx]\n", 
		//	id.c_str(), lid);
		ret = cl->call(rlock_protocol::retry, lid, r);
	} else {
		//printf("retry rpc faild [client:%s, lock %016llx]\n", 
		//	id.c_str(), lid);
		ret = rlock_protocol::RPCERR;
	}
	return ret;
}
