#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include "pthread.h"


class lock_server_cache {
	private:
		int nacquire;
		std::map<lock_protocol::lockid_t, std::list<std::string> > lock_map;
		int revoke(std::string id, lock_protocol::lockid_t lid);
		int retry(std::string id, lock_protocol::lockid_t lid);

		pthread_mutex_t mutex;

	public:
		lock_server_cache();
		~lock_server_cache();
		lock_protocol::status stat(lock_protocol::lockid_t, int &);
		int acquire(lock_protocol::lockid_t, std::string id, int &);
		int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
