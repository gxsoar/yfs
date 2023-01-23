// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include "extent_client.h"

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst) {
  ec = new extent_client(extent_dst);
}

yfs_client::inum yfs_client::n2i(std::string n) {
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string yfs_client::filename(inum inum) {
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool yfs_client::isfile(inum inum) {
  if (inum & 0x80000000) return true;
  return false;
}

bool yfs_client::isdir(inum inum) { return !isfile(inum); }

int yfs_client::getfile(inum inum, fileinfo &fin) {
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

int yfs_client::getdir(inum inum, dirinfo &din) {
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

yfs_client::inum yfs_client::createFileInum() {
  std::mt19937 generator(std::chrono::system_clock::now().time_since_epoch().count());
  auto inum = generator();
  inum |= 0x80000000;
  return inum;
}

yfs_client::inum yfs_client::createDirInum() {
  yfs_client::inum inum;
  // 随机产生inum号，同时确保目录号不能等于0x00000000000000001
  while(true) {
    std::mt19937 generator(std::chrono::system_clock::now().time_since_epoch().count());
    inum = generator();
    inum &= ~(0x80000000);
    if (inum != 1) break;
  }
  return inum;
}

int yfs_client::create(inum parent, std::string child_name, inum &child) {
  std::string parent_content;
  std::vector<dirent> dirs;
  readdir(parent, dirs);
  for (auto dir : dirs) {
    if (isfile(dir.inum) && dir.name == child_name) {
      return yfs_client::EXIST;
    } 
  }
  child = createFileInum();
  std::cout  << "yfs_client::create: " << "parent id : " << parent<< " child_name = " << child_name << " " << child << " child & 0x80 " << ((child >> 31) & 1 ) << std::endl; 
  ec->put(child, child_name);
  std::cout << "filename " << filename(child) << std::endl;
  ec->put(parent, filename(child));
  return yfs_client::OK;
}

int yfs_client::readdir(inum parent, std::vector<dirent> &dir_content) {
  std::string buf;
  auto ret = ec->get(parent, buf);
  std::stringstream ss(buf);
  std::vector<std::string> tmp;
  std::string str;
  while(ss >> str) tmp.push_back(str);
  for (int i = 1; i < tmp.size(); ++ i) {
    auto npos = tmp[i].find_last_of('&');
    auto name = tmp[i].substr(0, npos);
    auto str_inum = tmp[i].substr(npos + 1, tmp[i].size() - npos - 1);
    auto inum = n2i(str_inum);
    dir_content.push_back({name, inum});
  }
  return yfs_client::OK;
}
// dsfadf&123445

bool yfs_client::lookup(inum parent, std::string child_name, inum &child_inum) {
  std::vector<dirent> dirs;
  readdir(parent, dirs);
  for (auto dir : dirs) {
    if (dir.name == child_name) {
      child_inum = dir.inum;
      return true;
    }
  }
  return false;
}
