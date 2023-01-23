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
  std::unordered_map<extent_protocol::extentid_t, std::string> id_to_name_;
  std::unordered_map<extent_protocol::extentid_t, extent_protocol::attr> id_to_attr_;
  std::unordered_map<extent_protocol::extentid_t, std::unordered_set<std::string>> dir_content_;
};

#endif
