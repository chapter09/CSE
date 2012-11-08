// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include <pthread.h>

#define FREE 1
#define LOCKED 0

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
	public:
		virtual void dorelease(lock_protocol::lockid_t) = 0;
		virtual ~lock_release_user() {};
};

class lock_client_cache : public lock_client {
	private:
		class lock_release_user *lu;
		int rlock_port;
		std::string hostname;
		std::string id;

		struct lock_info {
			bool is_revoked = false;
			bool is_retried = false;
		}

		std::map<lock_protocol::lockid_t, lock_info> info_map;
		std::map<lock_protocol::lockid_t, int> cache_map;
		pthread_mutex_t mutex;	//lock_client global lock
		pthread_mutex_t lock_mutex;
		pthread_cond_t lock_cv;
		pthread_cond_t retry_cv;
		bool is_lock_cached(lock_protocol::lockid_t);
		bool is_locked(lock_protocol::lockid_t);
	
	public:
		lock_client_cache(std::string xdst, class lock_release_user *l = 0);
		virtual ~lock_client_cache() {};
		lock_protocol::status acquire(lock_protocol::lockid_t);
		lock_protocol::status release(lock_protocol::lockid_t);
		rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
				int &);
		rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
				int &);
};


#endif
