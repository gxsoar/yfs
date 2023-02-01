#ifndef extent_client_cache
#define extent_client_cache

#include <unordered_set>
#include <string>
#include <atomic>
#include <mutex>

#include "extent_protocol.h"

class ExtentClientCache {
public:

  ExtentClientCache() {
    id_ = cache_count_++;
  }

  auto count(extent_protocol::extentid_t id) { return extent_id_cache_.count(id); }

  void insert(extent_protocol::extentid_t id, const std::string &str) {
    extent_id_cache_.insert(id);
    content_[id] = str;
  }

  void insert(extent_protocol::extentid_t id, const extent_protocol::attr &attr) {
    extent_id_cache_.insert(id);
    attr_[id] = attr;
  }

  void get(extent_protocol::extentid_t id, std::string &buf) {
    buf = content_[id];
  }

  void get(extent_protocol::extentid_t id, extent_protocol::attr &attr) {
    attr = attr_[id];
  }

private:
  std::unordered_set<extent_protocol::extentid_t> extent_id_cache_;
  std::unordered_map<extent_protocol::extentid_t, std::string> content_;
  std::unordered_map<extent_protocol::extentid_t, extent_protocol::attr> attr_;
  int id_;
  static int cache_count_;
};

int ExtentClientCache::cache_count_ = 0;

#endif extent_client_cache