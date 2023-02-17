// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <list>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

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
};

#endif