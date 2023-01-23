// the extent server implementation

#include "extent_server.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sstream>

extent_server::extent_server() {
  int ret;
  put(1, "", ret);
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &) {
  // You fill this in for Lab 2.
  std::scoped_lock<std::mutex> lock(mutex_);
  extent_protocol::extentid_t child_id = 0;
  bool flag = true;
  id_to_name_[id] = buf;
  dir_content_[id].insert(buf);
  // std::cout << "extent_server::put : id = " << id << " buf = " << buf << "\n"; 
  for (auto content_ : dir_content_[1]) {
    std::cout << "extent_server::put : id = 1 content " << content_ << "\n";
  }
  extent_protocol::attr att;
  att.size = buf.size();
  att.ctime = time(nullptr);
  att.mtime = time(nullptr);
  att.atime = time(nullptr);
  id_to_attr_[id] = att;
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf) {
  // You fill this in for Lab 2.
  std::scoped_lock<std::mutex> lock(mutex_);
  if (id_to_name_.count(id) == 0U || id_to_attr_.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  auto str_to_id = [](const std::string &str) {
    std::istringstream ist(str);
    unsigned long long finum;
    ist >> finum;
    return finum;
  };
  buf = id_to_name_[id]; // buf的头部表示的是文件夹的名称
  auto ite = dir_content_[id].begin();
  if (*ite == buf) {
    ++ite;
    buf.push_back(' ');
  }
  for (; ite != dir_content_[id].end(); ++ ite) {
    auto inum = str_to_id(*ite);
    std::string str = id_to_name_[inum];
    buf += (str + "&" + *ite);
    buf.push_back(' ');
  }
  id_to_attr_[id].atime = time(nullptr);
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id,
                           extent_protocol::attr &a) {
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  std::scoped_lock<std::mutex> lock(mutex_);
  a.size = 0;
  a.atime = 0;
  a.mtime = 0;
  a.ctime = 0;
  if (id_to_attr_.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  a.size = id_to_attr_[id].size;
  a.atime = id_to_attr_[id].atime;
  a.mtime = id_to_attr_[id].mtime;
  a.ctime = id_to_attr_[id].ctime;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &) {
  // You fill this in for Lab 2.
  std::scoped_lock<std::mutex> lock(mutex_);
  if (id_to_name_.count(id) == 0 || id_to_attr_.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  id_to_name_.erase(id);
  id_to_attr_.erase(id);
  dir_content_.erase(id);
  // dir_content_[id].clear();
  return extent_protocol::OK;
}
