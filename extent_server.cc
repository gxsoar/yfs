// the extent server implementation

#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "extent_server.h"

extent_server::extent_server() {}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  std::scoped_lock<std::mutex> lock(mutex_);
  if (extent_content_.count(id) || extent_attr_.count(id)) {
    return extent_protocol::IOERR;
  }
  extent_content_[id] = buf;
  extent_protocol::attr att;
  att.size = buf.size();
  att.ctime = time(nullptr);
  att.mtime = time(nullptr);
  att.atime = time(nullptr);
  extent_attr_[id] = att;
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  std::scoped_lock<std::mutex> lock(mutex_);
  if (extent_content_.count(id) == 0U || extent_attr_.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  buf = extent_content_[id];
  extent_attr_[id].atime = time(nullptr);
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  a.size = 0;
  a.atime = 0;
  a.mtime = 0;
  a.ctime = 0;
  if (extent_attr_.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  a.size = extent_attr_[id].size;
  a.atime = extent_attr_[id].atime;
  a.mtime = extent_attr_[id].mtime;
  a.ctime = extent_attr_[id].ctime;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  if (extent_content_.count(id) == 0 || extent_attr_.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  extent_content_.erase(id);
  extent_attr_.erase(id);
  return extent_protocol::OK;
}

