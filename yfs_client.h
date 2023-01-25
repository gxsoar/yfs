#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include <random>
#include <chrono>

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
  inum createFileInum();  // 产生file inum
  inum createDirInum();   // 产生dir inum
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  // lab2 part1
  int create(inum parent, std::string name, inum &child);
  int readdir(inum inum, std::vector<dirent> &);
  bool lookup(inum parent, std::string child_name, inum &child_inum);
  // lab2 part2
  int setattr(inum, struct stat *attr);
  int read(inum, const size_t &size, const off_t &off, std::string &buf);
  int write(inum, const size_t &, const off_t &, const std::string &);
};

#endif 
