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
	printf("extent_server \n");
//	return extent_protocol::IOERR;
	if(info_map.find(id) == info_map.end()) {
		extent_protocol::attr a;
		a.mtime = time(NULL);
		a.ctime = time(NULL);
		a.atime = time(NULL);
		a.size = buf.size();
		pthread_mutex_lock(&mutex);
		info_map[id] = a;	
		ctnt_map[id] = buf; 
		pthread_mutex_unlock(&mutex);
		return extent_protocol::OK;
	} else {
		pthread_mutex_lock(&mutex);
		info_map[id].mtime = time(NULL);
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
	if(info_map.find(id) == info_map.end()) {
		return extent_protocol::NOENT;
	}

	pthread_mutex_lock(&mutex);		
	info_map[id].atime = time(NULL);
	buf = ctnt_map[id];
	pthread_mutex_unlock(&mutex);		

	return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
	// You fill this in for Lab 2.
	// You replace this with a real implementation. We send a phony response
	// for now because it's difficult to get FUSE to do anything (including
	// unmount) if getattr fails.
	if(info_map.find(id) == info_map.end()) {
		return extent_protocol::IOERR;
	} else {
		a = info_map[id];
		return extent_protocol::OK;
	}
//	a.size = 0;
//	a.atime = 0;
//	a.mtime = 0;
//	a.ctime = 0;
//	return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
	// You fill this in for Lab 2.
	if(info_map.find(id) == info_map.end()) {
		return extent_protocol::IOERR;
	} else {
		pthread_mutex_lock(&mutex);
		info_map.erase(id);
		ctnt_map.erase(id);
	//How to deal with the file name under the directory
	//Here we haven't remove the file name in the value to the key of dir	
		pthread_mutex_unlock(&mutex);		
		return extent_protocol::OK;
	}
	return extent_protocol::IOERR;
}

