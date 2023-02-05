#ifndef yfs_client_h
#define yfs_client_h

#include <string>
// #include "yfs_protocol.h"
#include <chrono>
#include <random>
#include <vector>

#include "extent_client.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include "extent_client_cache.h"

class yfs_client {
  // std::shared_ptr<lock_client> lc_;
  // std::shared_ptr<extent_client_cache> ec_;
  // std::shared_ptr<lock_release_user> lu_;
  lock_client *lc_;
  extent_client *ec_;
  lock_release_user *lu_;
  // lock_client_cache *lcc_;
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
  int create(const inum parent, const std::string &name, inum &child);
  int readdir(inum inum, std::vector<dirent> &);
  int lookup(const inum parent, const std::string &child_name,
              inum &child_inum);
  // lab2 part2
  int setattr(const inum, struct stat *attr);
  int read(const inum, const size_t &size, const off_t &off, std::string &buf);
  int write(const inum, const size_t &, const off_t &, const std::string &);
  // lab3 part1
  int mkdir(const inum parent, inum &child, const std::string &child_name);
  int unlink(const inum parent, const std::string &file_name);
};

class lock_guard {
public:
  lock_guard(lock_client *lc, lock_protocol::lockid_t lid) : lc_(lc), lid_(lid) {
    lc_->acquire(lid_);
  }
  ~lock_guard() {
    lc_->release(lid_);
  }
private:
  // std::shared_ptr<lock_client> lc_;
  lock_client *lc_;
  lock_protocol::lockid_t lid_;
};

#endif