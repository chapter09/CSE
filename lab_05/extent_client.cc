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
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
	if(ctnt_cmap.count(eid) == 0) {
		extent_protocol::attr attr;
		cl->call(extent_protocol::get, eid, buf);
		//cache it
		cl->call(extent_protocol::getattr, eid, attr);
		ctnt_cmap[eid] = buf;
		info_cmap[eid] = attr;
	} else {
		buf = ctnt_cmap[eid];
	}
	info_cmap[eid].atime = time(NULL);
	return extent_protocol::OK;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		extent_protocol::attr &attr)
{
	extent_protocol::status ret;
	if(info_cmap.count(eid) == 0) {
		ret = cl->call(extent_protocol::getattr, eid, attr);
		if(ret == extent_protocol::OK) {
			info_cmap[eid] = attr;
		}
		return ret;
	} else {
		printf("[E] extent_server %016llx getattr success\n", eid);
		attr = info_cmap[eid];
		return extent_protocol::OK;
	}
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
	extent_protocol::status ret = extent_protocol::OK;
	if(info_cmap.count(eid) == 0) {
		printf("[PUT] create \n");
		extent_protocol::attr a;
		a.mtime = time(NULL);
		a.ctime = time(NULL);
		a.atime = time(NULL);
		a.size = buf.size();
		a.dirty = true;
		info_cmap[eid] = a;
	} else {
		printf("[PUT] update \n");
		info_cmap[eid].mtime = time(NULL);
		info_cmap[eid].ctime = time(NULL);
		info_cmap[eid].size = buf.size();
		info_cmap[eid].dirty = true;
	}
	ctnt_cmap[eid] = buf;
	//int r;
	//ret = cl->call(extent_protocol::put, eid, buf, r);
	return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
	extent_protocol::status ret = extent_protocol::OK;
	ctnt_cmap.erase(eid);
	info_cmap.erase(eid);
	//int r;
	//ret = cl->call(extent_protocol::remove, eid, r);
	return ret;
}
