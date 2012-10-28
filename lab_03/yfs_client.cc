// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
	ec = new extent_client(extent_dst);
	srandom(time(NULL));
	build_root();
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
	std::istringstream ist(n);
	unsigned long long finum;
	ist >> finum;
	return finum;
}

void
yfs_client::build_root()
{
	inum num =  0x0000000000000001;
	VERIFY(ec->put(num, "") == extent_protocol::OK);
}

std::string
yfs_client::filename(inum inum)
{
	std::ostringstream ost;
	ost << inum;
	return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
	if(inum & 0x80000000)
		return true;
	return false;
}

bool
yfs_client::isdir(inum inum)
{
	return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
	int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock

	printf("getfile %016llx\n", inum);
	extent_protocol::attr a;
	if (ec->getattr(inum, a) != extent_protocol::OK) {
		r = IOERR;
		goto release;
	}

	fin.atime = a.atime;
	fin.mtime = a.mtime;
	fin.ctime = a.ctime;
	fin.size = a.size;
	printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:

	return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
	int r = OK;
	// You modify this function for Lab 3
	// - hold and release the directory lock

	printf("getdir %016llx\n", inum);
	extent_protocol::attr a;
	if (ec->getattr(inum, a) != extent_protocol::OK) {
		r = IOERR;
		goto release;
	}
	din.atime = a.atime;
	din.mtime = a.mtime;
	din.ctime = a.ctime;

release:
	return r;
}

int
yfs_client::create(inum dir_inum, const char *name, inum &f, bool is_f)
{
	int r = OK;
	extent_protocol::attr a;
	std::string value;
	if (ec->getattr(dir_inum, a) != extent_protocol::OK) {
		r = IOERR;
		printf("directory %016llx does not exist\n", dir_inum);
		goto release;
	}
	
	extent_protocol::status ret;
	printf("[i] create_yfs_client\n");	
	ret = ec->get(dir_inum, value);
	inum f_inum;

	if(lookup(dir_inum, name, f_inum)) {
		r = EXIST;
		goto release;
	} else {
		f = gen_inum(is_f);	//generate dir inum or not
		value.append(filename(f));
		value.append(":");
		value.append(name);
		value.append("\n");
		ec->put(dir_inum, value);	
		ec->put(f, "");	
		r = OK;	
		goto release;
	}
release:
	return r;
}

int
yfs_client::write(inum inum, size_t size, off_t off, const char *buf)
{
	printf("[i] write_yfs_client\n");	
	int r = OK;
	size_t v_size;
	int t;
	std::string value;
	VERIFY(ec->get(inum, value) == extent_protocol::OK);
	v_size = value.size();
	printf("[i] write_yfs_client value: %s size: %d\n", value.c_str(), v_size);	
	if(off > v_size) {
		for(t = v_size; t < off; t++) {
			value.push_back('\0');
		}
		for(t = 0; t < int(size); t++) {
			value.push_back(*(buf++));
		}	
	} else if((off+size) > v_size) {
		value.erase(off, v_size-off);
		for(t = 0; t < int(size); t++) {
			value.push_back(*(buf++));
		}	
	} else {
		value.replace(off, size, buf, size);
			
	}
	printf("[i] write_yfs_client new value: %s size: %d\n", value.c_str(), value.size());	
	if (ec->put(inum, value) != extent_protocol::OK) {
		r = IOERR;
		printf("[yfs_client] %016llx write_put failed\n", inum);
		goto release;
	}
	
release:
	return r;
}

int
yfs_client::read(inum inum, size_t size, off_t off, std::string &buf)
{
	printf("[i] read_yfs_client\n");	
	int r = OK;
	std::string value;
	VERIFY(ec->get(inum, value) == extent_protocol::OK);
	size_t v_size = value.size();
	if(off >= v_size) {
		buf = "";
	} else if((off+size) > v_size) {
		buf = value.substr(off, v_size-off);
	} else {
		buf = value.substr(off, size);
	}
//	printf("[yfs_client] %016llx read\n", inum);
//	printf("[i] read_yfs_client value: %s size: %d\n", value.c_str(), value.size());	
	return r;
}

int
yfs_client::setattr(inum inum, long long int size)
{
	int r = OK;
	printf("[i] setattr_yfs_client\n");
	std::string value;	
	VERIFY(ec->get(inum, value) == extent_protocol::OK);
	size_t v_size = value.size();
	int t;
	if(size > v_size) {
		for(t = v_size; t < size; t++) {
			value.push_back('\0');
		}
	} else {
		value.erase(size, v_size-size);	
	}	
			
	if (ec->put(inum, value) != extent_protocol::OK) {
		r = IOERR;
		printf("[yfs_client] %016llx write_put failed\n", inum);
		goto release;
	}
	
release:
	return r;
}

int
yfs_client::mkdir(inum p_inum, const char *name, inum &inum)
{
	int r = OK;
	r = create(p_inum, name, inum, true);
	return r;
}

int
yfs_client::unlink(inum p_inum, const char *name)
{
	inum f;

	if(!lookup(p_inum, name, f)) return NOENT;	

	if(!isfile(f)) {
		return IOERR; 
	} else {
		VERIFY(ec->remove(f) == OK);
		
		std::string value;
		size_t found;
		VERIFY(ec->get(p_inum, value) == extent_protocol::OK);
		printf("[i] unlink_yfs_client value: %s size: %d f: %016llx\n", value.c_str(), value.size(), f);	
		found = value.find(name);
		value.erase(found-11,12+strlen(name));
		VERIFY(ec->put(p_inum, value) == extent_protocol::OK);
		printf("[i] unlink_yfs_client new value: %s size: %d\n", value.c_str(), value.size());	
	}
		printf("[i] unlink_yfs_client done\n");	
	return OK;
}

int 
yfs_client::read_dir(inum dir_inum, std::list<struct yfs_client::dirent> &list_dir)
{
	std::string value = "";
	printf("[i] readdir_yfs_client\n");	
	printf("[i] %016llx\n", dir_inum);
	ec->get(dir_inum, value);

	std::string line;
	std::stringstream stream;
	stream << value;
	while(getline(stream, line)) {
		size_t pos;
		struct dirent dir_ent;
		pos = line.find(':');
		dir_ent.name = line.substr(pos+1);
		dir_ent.inum = n2i(line.substr(0, pos));
		list_dir.push_back(dir_ent);		
	}
	return extent_protocol::OK;
}

bool
yfs_client::lookup(inum dir_inum, const char *name, inum &f_inum)
{
	bool r;
	std::string value;
	size_t found;
	printf("[i] lookup_yfs_client B\n");	
	printf("[LOOK UP] %016llx\n", dir_inum);
	printf("[i] %s\n", name);
	VERIFY(ec->get(dir_inum, value) == extent_protocol::OK);
	
//	printf("[value]\n");
//	printf("%s\n", value.c_str());
//	printf("[value]\n");
	found = value.find(name);
	if(found != std::string::npos) {
		printf("have found %s\n", name);
		r = true;
		printf("have found %s off %d\n", value.c_str(), found);
		f_inum = n2i(value.substr(found-11, 10));
		goto release;
	} else {
		printf("have not found %s\n", name);
		r = false;
		goto release;
	}

release:
	return r;
}

yfs_client::inum
yfs_client::gen_inum(bool is_dir)
{
	inum inum;
	inum = rand() % 2147483648U;
	if(!is_dir) {
		inum = inum | 0x0000000080000000;
	} else {
		inum = inum & 0x000000007FFFFFFF;
	}
	return inum;
}
