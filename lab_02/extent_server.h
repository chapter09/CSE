// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include <pthread.h>

class extent_server {

	private:
		std::map<extent_protocol::extentid_t, std::string> file_map;
		std::map<extent_protocol::extentid_t, extent_protocol::attr> info_map;
		pthread_mutex_t mutex;
	
	public:
		extent_server();

		int put(extent_protocol::extentid_t id, std::string, int &);
		int get(extent_protocol::extentid_t id, std::string &);
		int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
		int remove(extent_protocol::extentid_t id, int &);
};

#endif 







