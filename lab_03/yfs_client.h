#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client.h"

class yfs_client {
	extent_client *ec;
	lock_client *lc;
	public:

	typedef unsigned long long inum;
	enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
	typedef int status;

	struct fileinfo {
		unsigned long long size;
		unsigned long atime;
		unsigned long mtime;
		unsigned long ctime;
	};
	struct dirinfo {
		unsigned long atime;
		unsigned long mtime;
		unsigned long ctime;
	};
	struct dirent {
		std::string name;
		yfs_client::inum inum;
	};

	private:
	static std::string filename(inum);
	static inum n2i(std::string);
	inum gen_inum(bool);
	void build_root();
	bool yfs_lookup(inum, const char*, inum &);
	int yfs_read_dir(inum, std::list<struct yfs_client::dirent> &);

	public:

	yfs_client(std::string, std::string);

	bool isfile(inum);
	bool isdir(inum);
	bool lookup(inum, const char*, inum &);

	int getfile(inum, fileinfo &);
	int getdir(inum, dirinfo &);
	int create(inum, const char*, inum &, bool);
	int read_dir(inum, std::list<struct yfs_client::dirent> &);
	int write(inum, size_t, off_t, const char *);
	int read(inum, size_t, off_t, std::string &);
	int setattr(inum, long long int);
	int mkdir(inum, const char *, inum &);
	int unlink(inum p_inum, const char *name);
};

#endif 
