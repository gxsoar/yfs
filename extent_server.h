// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <list>

#include "extent_protocol.h"

class extent_server {
 public:
  extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);

 private:
  std::mutex mutex_;
  struct container {
    std::string content_;
    extent_protocol::attr att_;
  };
  std::unordered_map<extent_protocol::extentid_t, container> content_map; 
  // std::unordered_map<extent_protocol::extentid_t, std::string> id_to_name_; // id 到名字的映射
  // std::unordered_map<extent_protocol::extentid_t, extent_protocol::attr> id_to_attr_;   // id 到 属性的映射
  // std::unordered_map<extent_protocol::extentid_t, std::list<std::string>> dir_content_; //  存储目录的内容
  // std::unordered_map<extent_protocol::extentid_t, std::string> file_content_; //  存储文件的内容
  // std::unordered_map<std::string, extent_protocol::extentid_t> file_to_dir_;  // 建立文件和目录之间的映射关系
};

#endif
