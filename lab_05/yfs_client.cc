// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client_cache.h"
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
	lu = new _lock_release_user(ec);
	lc = new lock_client_cache(lock_dst, lu);
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
	inum num = 0x0000000000000001;
	lc->acquire(num);
	VERIFY(ec->put(num, "") == extent_protocol::OK);
	lc->release(num);
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
	lc->acquire(inum);
	printf("getfile %016llx\n", inum);
	extent_protocol::attr a;
	if (ec->getattr(inum, a) != extent_protocol::OK) {
		r = NOENT;
		goto release;
	}

	fin.atime = a.atime;
	fin.mtime = a.mtime;
	fin.ctime = a.ctime;
	fin.size = a.size;
	printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
	lc->release(inum);
	return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
	int r = OK;
	// You modify this function for Lab 3
	// - hold and release the directory lock

	lc->acquire(inum);
	printf("getdir %016llx\n", inum);
	extent_protocol::attr a;
	if (ec->getattr(inum, a) != extent_protocol::OK) {
		r = NOENT;
		goto release;
	}
	din.atime = a.atime;
	din.mtime = a.mtime;
	din.ctime = a.ctime;

release:
	lc->release(inum);
	return r;
}

int
yfs_client::create(inum dir_inum, const char *name, inum &f, bool is_f)
{
	int r = OK;
	extent_protocol::attr a;
	std::string value;
	lc->acquire(dir_inum);
	if (ec->getattr(dir_inum, a) != extent_protocol::OK) {
		r = NOENT;
		printf("directory %016llx does not exist\n", dir_inum);
		goto release;
	}
	
	//extent_protocol::status ret;
	printf("[i] create_yfs_client\n");	
	//ret = ec->get(dir_inum, value);
	VERIFY(ec->get(dir_inum, value) == extent_protocol::OK);
	inum f_inum;

//	lc->acquire(f_inum);
	if(yfs_lookup(dir_inum, name, f_inum)) {
		f = f_inum;
		r = EXIST;
		//lc->release(f_inum);
		goto release;
	} else {
		f = gen_inum(is_f);	//generate dir inum or not
		value.append(filename(f));
		value.append(":");
		value.append(name);
		value.append("\n");
		ec->put(dir_inum, value);	
		lc->acquire(f);
		ec->put(f, "");	
		lc->release(f);
		r = OK;	
//		lc->release(f_inum);
		goto release;
	}
release:
	lc->release(dir_inum);
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
	lc->acquire(inum);
	VERIFY(ec->get(inum, value) == extent_protocol::OK);
	v_size = value.size();
	printf("[i] write_yfs_client value: %s size: %d\n", 
		value.c_str(), v_size);	
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
	printf("[i] write_yfs_client new value: %s size: %d\n", 
		value.c_str(), value.size());	
	if (ec->put(inum, value) != extent_protocol::OK) {
		r = IOERR;
		printf("[yfs_client] %016llx write_put failed\n", inum);
		goto release;
	}
release:
	lc->release(inum);
	return r;
}

int
yfs_client::read(inum inum, size_t size, off_t off, std::string &buf)
{
	printf("[i] read_yfs_client\n");	
	int r = OK;
	std::string value;
	lc->acquire(inum);
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
	lc->release(inum);
	return r;
}

int
yfs_client::setattr(inum inum, long long int size)
{
	int r = OK;
	printf("[i] setattr_yfs_client\n");
	std::string value;	
	lc->acquire(inum);
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
	lc->release(inum);
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
	int r = OK;
	printf("[U] unlink %s\n", name);	
	lc->acquire(p_inum);
	if(!yfs_lookup(p_inum, name, f)) {
		r = NOENT;
		goto release;
	}

	if(!isfile(f)) {
		r = IOERR;
		goto release; 
	} else {
		VERIFY(ec->remove(f) == extent_protocol::OK);
		
		std::string value;
		size_t found;
		VERIFY(ec->get(p_inum, value) == extent_protocol::OK);
		printf("[U] c_value:\n %s size: %d f: %016llx\n", value.c_str(), value.size(), f);	
		found = value.find(name);
		value.erase(found-11,12+strlen(name));
		VERIFY(ec->put(p_inum, value) == extent_protocol::OK);
		printf("[U] d_value\n: %s size: %d\n", value.c_str(), value.size());	
	}
	printf("[U] done\n");	

release:
	lc->release(p_inum);
	return OK;
}

int 
yfs_client::read_dir(inum dir_inum, std::list<struct yfs_client::dirent> &list_dir)
{
	lc->acquire(dir_inum);
	int r;
	r = yfs_read_dir(dir_inum, list_dir);
	lc->release(dir_inum);
	return r;
}

int
yfs_client::yfs_read_dir(inum dir_inum, std::list<struct yfs_client::dirent> &list_dir)
{
	std::string value = "";
	printf("[i] readdir_yfs_client\n");	
	printf("[i] %016llx\n", dir_inum);
//	VERIFY(ec->get(dir_inum, value) == extent_protocol::OK);
	if(ec->get(dir_inum, value) != extent_protocol::OK) {
		return extent_protocol::OK;
	}

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
yfs_client::yfs_lookup(inum dir_inum, const char *name, inum &f_inum)
{
	bool r;
	std::string value;
	printf("[LOOK UP] under dir %016llx\n", dir_inum);
	printf("[i] look up %s\n", name);
	std::list<struct yfs_client::dirent> list_dir;
	std::list<struct yfs_client::dirent>::iterator dir_it;
	std::string s = name;
	//VERIFY(yfs_read_dir(dir_inum, list_dir) == extent_protocol::OK);
	yfs_read_dir(dir_inum, list_dir);
	
	for(dir_it = list_dir.begin(); dir_it != list_dir.end(); dir_it++) {
		if((*dir_it).name == s) {
			f_inum = (*dir_it).inum;
			printf("have found %s %016llx\n", (*dir_it).name.c_str(), f_inum);
			r = true;
			goto release;
		}
	}
	printf("have not found %s\n", name);
	r = false;
	goto release;

release:
	return r;
}


bool
yfs_client::lookup(inum dir_inum, const char *name, inum &f_inum)
{
	bool r;
	lc->acquire(dir_inum);
	r = yfs_lookup(dir_inum, name, f_inum);
	lc->release(dir_inum);
	return r;

}

yfs_client::inum
yfs_client::gen_inum(bool is_dir)
{
	inum inum;
	std::string buf;
	while(1) {
		inum = rand() % 2147483648U;
		if(!is_dir) {
			inum = inum | 0x0000000080000000;
		} else {
			inum = inum & 0x000000007FFFFFFF;
		}
		if(ec->get(inum, buf) == extent_protocol::NOENT) {
			break;
		}
	}
	return inum;
}
