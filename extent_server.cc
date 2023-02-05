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
  content_map[id].att_.size = buf.size();
  content_map[id].att_.atime = time(nullptr);
  content_map[id].att_.ctime = time(nullptr);
  content_map[id].att_.mtime = time(nullptr);
  content_map[id].content_ = buf;
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf) {
  // You fill this in for Lab 2.
  std::scoped_lock<std::mutex> lock(mutex_);
  if (content_map.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  content_map[id].att_.atime = time(nullptr);
  buf = content_map[id].content_;
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id,
                           extent_protocol::attr &a) {
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  std::scoped_lock<std::mutex> lock(mutex_);
  if (content_map.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  a.size = content_map[id].att_.size;
  a.atime = content_map[id].att_.atime;
  a.mtime = content_map[id].att_.mtime;
  a.ctime = content_map[id].att_.ctime;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &) {
  // You fill this in for Lab 2.
  std::scoped_lock<std::mutex> lock(mutex_);
  if (content_map.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  content_map.erase(id);
  return extent_protocol::OK;
}
