#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
	private:
		int nacquire;
		struct lock_rec {
			unsigned long long lid;
			int stat;
			std::string h_cid;
			std::list<std::string> wait_list;
		};
		
		std::list<struct lock_rec> l_list;
		int revoke(std::string id);
		int retry(std::string id);

	public:
		lock_server_cache();
		lock_protocol::status stat(lock_protocol::lockid_t, int &);
		int acquire(lock_protocol::lockid_t, std::string id, int &);
		int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
