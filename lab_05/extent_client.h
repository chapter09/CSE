// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "rpc.h"
#include <pthread.h>

class extent_client {
	private:
		rpcc *cl;
		std::map<extent_protocol::extentid_t, std::string> ctnt_cmap;
		std::map<extent_protocol::extentid_t, extent_protocol::attr> info_cmap;
		pthread_mutex_t mutex;		
		
	public:
		extent_client(std::string dst);

		extent_protocol::status get(extent_protocol::extentid_t eid, 
				std::string &buf);
		extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				extent_protocol::attr &a);
		extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
		extent_protocol::status remove(extent_protocol::extentid_t eid);
		extent_protocol::status flush(extent_protocol::extentid_t);
};

#endif 

