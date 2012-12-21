// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
	sockaddr_in dstsock;
	make_sockaddr(dst.c_str(), &dstsock);
	cl = new rpcc(dstsock);
	if (cl->bind() != 0) {
		printf("extent_client: bind failed\n");
	}
	VERIFY(pthread_mutex_init(&mutex, NULL) == 0);
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
	extent_protocol::status ret;
	pthread_mutex_lock(&mutex);
	if(ctnt_cmap.count(eid) == 0) {
		printf("[EC] remote get\n");
		ret = cl->call(extent_protocol::get, eid, buf);
		ctnt_cmap[eid] = buf;
		goto release;
	} else {
		printf("[EC] cache get\n");
		printf("[EC] get %016llx %s\n", eid, ctnt_cmap[eid].c_str());
		buf = ctnt_cmap[eid];
		info_cmap[eid].atime = time(NULL);
		ret = extent_protocol::OK;
		goto release;
	}
release:
	pthread_mutex_unlock(&mutex);
	return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		extent_protocol::attr &attr)
{
	extent_protocol::status ret;
	pthread_mutex_lock(&mutex);
	if(info_cmap.count(eid) == 0) {
		printf("[EC] attr to remote %016llx\n", eid);
		ret = cl->call(extent_protocol::getattr, eid, attr);
		if(ret == extent_protocol::OK) {
			printf("[EC] success attr remote %016llx\n", eid);
			attr.dirty = false;
			info_cmap[eid] = attr;
		}
		goto release;
	} else {
		printf("[EC] attr from cached %016llx\n", eid);
		attr = info_cmap[eid];
		ret = extent_protocol::OK;
		goto release;
	}
release:
	pthread_mutex_unlock(&mutex);
	return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
	extent_protocol::status ret = extent_protocol::OK;
	pthread_mutex_lock(&mutex);
	if(info_cmap.count(eid) == 0) {
		printf("[PUT] create %016llx\n", eid);
		extent_protocol::attr a;
		a.mtime = time(NULL);
		a.ctime = time(NULL);
		a.atime = time(NULL);
		a.size = buf.size();
		a.dirty = true;
		info_cmap[eid] = a;
	} else {
		printf("[PUT] update %016llx\n", eid );
		info_cmap[eid].mtime = time(NULL);
		info_cmap[eid].ctime = time(NULL);
		info_cmap[eid].size = buf.size();
		info_cmap[eid].dirty = true;
	}
	ctnt_cmap[eid] = buf;
	pthread_mutex_unlock(&mutex);
	return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
	extent_protocol::status ret;
	pthread_mutex_lock(&mutex);
	if(ctnt_cmap.count(eid) == 0) {
		info_cmap[eid].dirty = true;
		ret = extent_protocol::OK;
	} else {
		ctnt_cmap.erase(eid);
		info_cmap[eid].dirty = true;
		ret = extent_protocol::OK;
	}
	pthread_mutex_unlock(&mutex);
	return ret;
}


extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid)
{
	//is dirty?
	//is removed?
	//invalidate the attr
	extent_protocol::status ret = extent_protocol::OK;
	int r;
	pthread_mutex_lock(&mutex);
	if(info_cmap.count(eid) != 0) {
		if(info_cmap[eid].dirty) {
			printf("[FLU] dirty \n");
			if(ctnt_cmap.count(eid) == 0) {
				printf("[FLU] removed \n");
				ret = cl->call(extent_protocol::remove, eid, r);
			} else {
				printf("[FLU] update \n");
				ret = cl->call(extent_protocol::put, eid, ctnt_cmap[eid], r);
			}
		}
	}
	ctnt_cmap.erase(eid);
	info_cmap.erase(eid);			
	pthread_mutex_unlock(&mutex);
	return ret;
}
