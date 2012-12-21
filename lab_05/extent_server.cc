// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() 
{
	VERIFY(pthread_mutex_init(&mutex, NULL) == 0);
}

extent_server::~extent_server()
{
	VERIFY(pthread_mutex_destroy(&mutex) == 0);
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
	// You fill this in for Lab 2.
	// Set mtime and ctime
//	printf("[PUT] buf is %s \n", buf.c_str());
//	return extent_protocol::IOERR;
	pthread_mutex_lock(&mutex);
	if(info_map.find(id) == info_map.end()) {
		printf("[PUT] create %016llx\n", id);
		extent_protocol::attr a;
		a.mtime = time(NULL);
		a.ctime = time(NULL);
		a.atime = time(NULL);
		a.size = buf.size();
		info_map[id] = a;	
		ctnt_map[id] = buf; 
		pthread_mutex_unlock(&mutex);
		return extent_protocol::OK;
	} else {
		printf("[PUT] update %016llx\n", id);
		info_map[id].mtime = time(NULL);
		info_map[id].ctime = time(NULL);
		info_map[id].size = buf.size();
		ctnt_map[id] = buf;
		pthread_mutex_unlock(&mutex);
		return extent_protocol::OK;
	}	
	return extent_protocol::IOERR;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
	// You fill this in for Lab 2.
	// Set atime
	//return extent_protocol::IOERR;
	pthread_mutex_lock(&mutex);		
	if(info_map.count(id) == 0) {
		printf("[GET] not found %s \n", buf.c_str());
		pthread_mutex_unlock(&mutex);		
		return extent_protocol::NOENT;
	}
	info_map[id].atime = time(NULL);
	buf = ctnt_map[id];
	printf("[GET] buf is %s \n", buf.c_str());
	pthread_mutex_unlock(&mutex);		

	return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
	// You fill this in for Lab 2.
	// You replace this with a real implementation. We send a phony response
	// for now because it's difficult to get FUSE to do anything (including
	// unmount) if getattr fails.
	pthread_mutex_lock(&mutex);		
	if(info_map.count(id) == 0) {
		printf("[E] extent_server %016llx no attr found\n", id);
		pthread_mutex_unlock(&mutex);		
		return extent_protocol::NOENT;
	} else {
		printf("[E] extent_server %016llx getattr success\n", id);
		a = info_map[id];
		pthread_mutex_unlock(&mutex);		
		return extent_protocol::OK;
	}
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
	// You fill this in for Lab 2.
	pthread_mutex_lock(&mutex);
	if(info_map.find(id) == info_map.end()) {
		pthread_mutex_unlock(&mutex);		
		return extent_protocol::IOERR;
	} else {
		info_map.erase(id);
		ctnt_map.erase(id);
	//How to deal with the file name under the directory
	//Here we haven't remove the file name in the value to the key of dir	
		pthread_mutex_unlock(&mutex);		
		return extent_protocol::OK;
	}
	return extent_protocol::IOERR;
}
