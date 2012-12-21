// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include "extent_client.h"
#include <pthread.h>


// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
	public:
		virtual void dorelease(lock_protocol::lockid_t) = 0;
		virtual ~lock_release_user() {};
};

class _lock_release_user : public lock_release_user {
	private:
		class extent_client *ec;

	public:
		_lock_release_user(class extent_client *);
		~_lock_release_user() {};
		void dorelease(lock_protocol::lockid_t); 
};

class lock_client_cache : public lock_client {
	private:
		class lock_release_user *lu;
		int rlock_port;
		std::string hostname;
		std::string id;
		enum xxstatus {NONE, FREE, LOCKED, ACQUIRING, RELEASING};
		typedef int l_status;

		// struct lock_info {
		// 	bool is_used;
		// 	bool is_retried;
		// };

		std::map<lock_protocol::lockid_t, bool> info_map;
		std::map<lock_protocol::lockid_t, l_status> cache_map;
		std::map<lock_protocol::lockid_t, pthread_cond_t> cv_map;
		pthread_mutex_t mutex;	//lock_client global lock
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
