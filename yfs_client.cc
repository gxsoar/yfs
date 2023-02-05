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
  lc_ = new lock_client(lock_dst);
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

int yfs_client::create(const inum parent, const std::string &child_name, inum &child) {
  lock_guard lock(lc_, parent);
  std::string parent_content;
  std::vector<dirent> dirs;
  readdir(parent, dirs);
  for (auto dir : dirs) {
    if (isfile(dir.inum) && dir.name == child_name) {
      return yfs_client::EXIST;
    } 
  }
  child = createFileInum();
  ec->put(child, "");
  std::string buf;
  if (ec->get(parent, buf) != extent_protocol::OK) {
    return yfs_client::IOERR;
  } 
  buf += (" " + child_name + "&" + filename(child));
  ec->put(parent, buf);
  return yfs_client::OK;
}

int yfs_client::readdir(inum parent, std::vector<dirent> &dir_content) {
  std::string buf;
  auto ret = ec->get(parent, buf);
  if (ret != extent_protocol::OK) {
    return yfs_client::IOERR;
  }
  std::stringstream ss(buf);
  std::vector<std::string> tmp;
  std::string str;
  while(ss >> str) tmp.push_back(str);
  // 保证str的头部存储的是目录名
  int n = tmp.size();
  for (int i = 0; i < n; ++ i) {
    auto npos = tmp[i].find_last_of('&');
    auto name = tmp[i].substr(0, npos);
    auto str_inum = tmp[i].substr(npos + 1, tmp[i].size() - npos - 1);
    auto inum = n2i(str_inum);
    if (!name.empty())dir_content.push_back({name, inum});
  }
  return yfs_client::OK;
}

int yfs_client::lookup(inum parent, const std::string &child_name, inum &child_inum) {
  std::vector<dirent> dirs;
  readdir(parent, dirs);
  for (auto dir : dirs) {
    if (dir.name == child_name) {
      child_inum = dir.inum;
      return yfs_client::OK;
    }
  }
  return yfs_client::NOENT;
}

int yfs_client::setattr(inum inum, struct stat *attr) {
  lock_guard lock(lc_, inum);
  std::string buf;
  auto ret = ec->get(inum, buf);
  if (ret != extent_protocol::OK) {
    return yfs_client::IOERR;
  }
  size_t size = attr->st_size;
  buf.resize(size, '\0');
  ret = ec->put(inum, buf);
  if (ret != extent_protocol::OK) {
    return yfs_client::IOERR;
  }
  return yfs_client::OK;
}

int yfs_client::read(inum inum, const size_t &size, const off_t &off, std::string &buf) {
  std::string str;
  auto ret = ec->get(inum, str);
  if (ret != extent_protocol::OK) {
    return yfs_client::IOERR;
  }
  auto len = str.size();
  if (off >= len) {
    buf = "";
    return yfs_client::OK;
  }
  if (size + off > len) {
    buf = str.substr(off, len - off);
  } else {
    buf = str.substr(off, size);
  }
  return yfs_client::OK;
}

int yfs_client::write(const inum inum, const size_t &size, const off_t &off, const std::string &buf) {
  lock_guard lock(lc_, inum);
  std::string str;
  auto ret = ec->get(inum, str);
  if (ret != extent_protocol::OK) {
    return yfs_client::IOERR;
  }
  if (off + size > str.size()) {
    str.resize(off + size, '\0');
  }
  for (size_t i = 0; i < size; ++ i) {
    str[off + i] = buf[i];
  }
  ret = ec->put(inum, str);
  if (ret != extent_protocol::OK) {
    return yfs_client::IOERR;
  }
  return yfs_client::OK;
}

int yfs_client::mkdir(const inum parent_inum, inum &child_inum, const std::string &child_name) {
  lock_guard lock(lc_, parent_inum);
  if (lookup(parent_inum, child_name, child_inum)) {
    return yfs_client::EXIST;
  }
  child_inum = createDirInum();
  if (ec->put(child_inum, "") != extent_protocol::OK) {
    return yfs_client::IOERR;
  }
  std::string buf;
  if (ec->get(parent_inum, buf) != extent_protocol::OK) {
    return yfs_client::IOERR;
  }
  buf +=(" " + child_name + "&" + filename(child_inum));
  if (ec->put(parent_inum, buf) != extent_protocol::OK) {
    return yfs_client::IOERR;
  }
  return yfs_client::OK;
}

int yfs_client::unlink(const inum parent_inum, const std::string &file_name) {
  lock_guard lock(lc_, parent_inum);
  yfs_client::inum file_inum;
  if (!lookup(parent_inum, file_name, file_inum)) {
    return yfs_client::NOENT;
  }
  std::vector<dirent> dirs;
  if (readdir(parent_inum, dirs) != extent_protocol::OK) {
    return yfs_client::IOERR;
  }
  std::string buf;
  bool flag = false;
  for (auto &content : dirs) {
    if (content.name == file_name) {
      flag = true;
      file_inum = content.inum;
      continue;
    }
    buf +=(" " + content.name + "&" + filename(content.inum));
  }
  if (!flag) {
    return yfs_client::NOENT;
  }
  ec->put(parent_inum, buf);
  if (ec->remove(file_inum) != extent_protocol::OK) {
    return yfs_client::IOERR;
  }
  return yfs_client::OK;
}