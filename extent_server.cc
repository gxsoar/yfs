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
  // 如果是文件相关的操作
  // if ((id >> 31) & 1) {
    // if (id_to_name_.count(id) == 0U) {
      // id_to_name_[id] = buf;
    // }
    // file_content_[id] = buf;
  // } else {
    // if (id_to_name_.count(id) == 0U) {
      // id_to_name_[id] = buf;
    // }
    // dir_content_[id].push_back(buf);
    // file_to_dir_[buf] = id;
  // }
  extent_protocol::attr att;
  att.size = buf.size();
  att.ctime = time(nullptr);
  att.mtime = time(nullptr);
  att.atime = time(nullptr);
  content_map[id].att_ = att;
  content_map[id].content_ = buf;
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf) {
  // You fill this in for Lab 2.
  std::scoped_lock<std::mutex> lock(mutex_);
  if (content_map.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  // if (id_to_name_.count(id) == 0U || id_to_attr_.count(id) == 0U) {
    // return extent_protocol::IOERR;
  // }
  // auto str_to_id = [](const std::string &str) {
    // std::istringstream ist(str);
    // unsigned long long finum;
    // ist >> finum;
    // return finum;
  // };
  // 如果是提取文件的内从
  // if ((id >> 31) & 1) {
  //   buf = file_content_[id];
  // } else {
  //   for (auto content : dir_content_[id]) {
  //     if (content.empty()) {
  //       continue;
  //     }
  //     auto inum = str_to_id(content);
  //     std::string str{id_to_name_[inum]};
  //     buf.append(str + "&" + content);
  //     buf.push_back(' ');
  //   }
  // }
  content_map[id].att_.atime = time(nullptr);
  buf = content_map[id].content_;
  // id_to_attr_[id].atime = time(nullptr);
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
  // if (id_to_attr_.count(id) == 0U) {
    // return extent_protocol::IOERR;
  // }
  a.size = content_map[id].att_.size;
  a.atime = content_map[id].att_.atime;
  a.atime = content_map[id].att_.mtime;
  a.ctime = content_map[id].att_.ctime;
  // a.size = id_to_attr_[id].size;
  // a.atime = id_to_attr_[id].atime;
  // a.mtime = id_to_attr_[id].mtime;
  // a.ctime = id_to_attr_[id].ctime;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &) {
  // You fill this in for Lab 2.
  std::scoped_lock<std::mutex> lock(mutex_);
  if (content_map.count(id) == 0U) {
    return extent_protocol::IOERR;
  }
  // if (id_to_name_.count(id) == 0 || id_to_attr_.count(id) == 0U) {
    // return extent_protocol::IOERR;
  // }
  // auto id_to_str = [&]() {
    //  std::ostringstream ost;
      // ost << id;
      // return ost.str();
  // };
  // auto str = id_to_str();
  // auto parent_id = file_to_dir_[str];
  // for (auto ite = dir_content_[parent_id].begin(); ite != dir_content_[parent_id].end(); ++ ite) {
  //   if (*ite == str) {
  //     dir_content_[parent_id].erase(ite);
  //     break;
  //   }
  // }
  // id_to_attr_[parent_id].ctime = time(nullptr);
  // id_to_attr_[parent_id].mtime = time(nullptr);
  // id_to_name_.erase(id);
  // id_to_attr_.erase(id);
  // if (dir_content_.count(id)) {
  //   dir_content_[id].clear();
  // }
  // if (file_content_.count(id)) {
  //   file_content_[id].clear();
  // }
  content_map.erase(id);
  return extent_protocol::OK;
}
