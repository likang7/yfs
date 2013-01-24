#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>


class yfs_client {
  extent_client *ec;
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
  inum generate_inum(bool isdir);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int lookupinode(inum, const std::string&,
           inum &);
  int create(inum, const std::string,
            mode_t mode, int isdir, inum &);
  int remove(inum inum, const std::string name,
            int actondir, int &ans);
  int readdir(inum inum,
             std::vector<dirent> &ans);
  int setattr(inum inum, fileinfo& attr);
  int readfile(inum inum, size_t size, off_t off,
          std::string &buf);
  int writefile(inum inum, off_t off,
         const char* buf, size_t size);
};

#endif 
