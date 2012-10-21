// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
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
yfs_client::create(inum dir_inum, const char *name, inum *f)
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
	printf("[i] %d\n", ret);

	if(lookup(dir_inum, name)) {
		r = EXIST;
		goto release;
	} else {
		*f = gen_inum(false);	//not generate dir inum
		value.append(filename(*f));
		value.append(":");
		value.append(name);
		value.append("\n");
		printf("[value] %s\n", value.c_str());
		r = ec->put(*f, value);	
		goto release;	
	}

release:
	return r;
}

int 
yfs_client::read_dir(inum dir_inum, std::list<struct dirent> *list_dir)
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
		(*list_dir).push_back(dir_ent);		
	}
}

bool
yfs_client::lookup(inum dir_inum, const char *name)
{
	bool r;
	std::string value;
	printf("[i] lookup_yfs_client B\n");	
	printf("[i] %016llx\n", dir_inum);
	printf("[i] %s\n", name);
	VERIFY(ec->get(dir_inum, value) == extent_protocol::OK);
	printf("[value] %s\n", value.c_str());
	
	if((value).find(name) != std::string::npos) {
		r = true;
		goto release;
	} else {
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
		inum = inum & 0x000000000FFFFFFF;
	}
	return inum;
}
