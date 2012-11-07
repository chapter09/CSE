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


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
		int &)
{
	lock_protocol::status ret;
	printf("acquire request from clt %s for lock %016llx\n", id.c_str(), lid);
	
	//check the status of lock
	pthread_mutex_lock(&mutex);

	if(l_map.find(lid) == l_map.end()) {
	//lock is requested first time
		std::list<std::string> wait_list;
		wait_list.push_back(id);
		l_map[lid] = wait_list;
		
		ret = lock_protocol::OK;
		pthread_mutex_unlock(&mutex);
		return ret;

	} else {
	//lock is on a LOCKED client
		l_map[lid].push_back(id);
		//send retry to the last one		
		ret = lock_protocol::RETRY;
		
		pthread_mutex_unlock(&mutex);
		
		//send revoke to the first client
		revoke(id, lid);
		return ret;
	}
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
		int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
	tprintf("stat request\n");
	r = nacquire;
	return lock_protocol::OK;
}

int lock_server_cache::revoke(std::string id, lock_protocol::lockid_t lid)
{
	//bind the client
	handle h(id);
	rpcc *cl = h.safebind();
	if(cl) {
		ret = cl->call(rlock_protocol::revoke);
	} else {
		printf("revoke rpc faild [client:%s, lock %016llx]\n", id.c_str(), lid);
	}

	return 0;
}

int lock_server_cache::retry(std::string id, lock_protocol::lockid_t lid)
{
	handle h(id);
	rpcc *cl = h.safebind();
	if(cl) {
		ret = cl->call(rlock_protocol::retry);
	} else {
		printf("retry rpc faild [client:%s, lock %016llx]\n", id.c_str(), lid);
	}

	return 0;
}


