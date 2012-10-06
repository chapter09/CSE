// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <map>

#define FREE 1
#define LOCKED 0

class lock_server {

	protected:
		int nacquire;
		std::map<lock_protocol::lockid_t, int> table_lk;

	public:
		lock_server();
		~lock_server() {};
		//clt : the thread_id of lock_client
		//lid : the id of lock requested
		//int &: the return value type
		lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
		lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
		lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};
#endif 







