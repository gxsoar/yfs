// the extent server implementation

#include "extent_server.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sstream>

extent_server::extent_server() {}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &) {
  // You fill this in for Lab 2.
  std::scoped_lock<std::mutex> lock(mutex_);
  if (id_to_name_.count(id) || id_to_attr_.count(id)) {
    return extent_protocol::IOERR;
  }
  extent_protocol::extentid_t child_id;
  bool flag = true;
  // 先检查Buf是不是子目录的id
  for (auto ch : buf) {
    if (!isdigit(ch - '0')) {
      flag = false;
      break;
    }
    child_id = child_id * 10 + ch - '0';
  }
  if (flag && child_id & 0x80000000) {
    dir_content_[id].insert(child_id);
  } else {
    id_to_name_[id] = buf;
    extent_protocol::attr att;
    att.size = buf.size();
    att.ctime = time(nullptr);
    att.mtime = time(nullptr);
    att.atime = time(nullptr);
    id_to_attr_[id] = att;
  }
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf) {
  // You fill this in for Lab 2.
  std::scoped_lock<std::mutex> lock(mutex_);
  if (id_to_name_.count(id) == 0U || id_to_attr_.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  buf = id_to_name_[id]; // buf的头部表示的是文件夹的名称
  buf += " ";
  for (auto &id : dir_content_[id]) {
    std::string content = id_to_name_[id];
    content.push_back('&');
    buf += (content +  std::to_string(id));
    buf += " ";
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
  if (id_to_name_.count(id) == 0 || id_to_attr_.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  id_to_name_.erase(id);
  id_to_attr_.erase(id);
  return extent_protocol::OK;
}
