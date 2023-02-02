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
    // id_ = cache_count_++;
  }

  auto count(extent_protocol::extentid_t id) { return extent_id_cache_.count(id); }
  
  void erase(extent_protocol::extentid_t id) {
    if (extent_id_cache_.count(id)) extent_id_cache_.erase(id);
    if (content_.count(id)) content_.erase(id);
    if (attr_.count(id)) attr_.erase(id);
  }

  extent_protocol::status insert(extent_protocol::extentid_t id, const std::string &str) {
    extent_id_cache_.insert(id);
    content_[id] = str;
    return extent_protocol::OK;
  }

  extent_protocol::status insert(extent_protocol::extentid_t id, const extent_protocol::attr &attr) {
    extent_id_cache_.insert(id);
    attr_[id] = attr;
    return extent_protocol::OK;
  }

  extent_protocol::status get(extent_protocol::extentid_t id, std::string &buf) {
    if (content_.count(id) == 0U) {
      return extent_protocol::NOENT;
    }
    buf = content_[id];
    return extent_protocol::OK;
  }

  extent_protocol::status get(extent_protocol::extentid_t id, extent_protocol::attr &attr) {
    if (attr_.count(id) == 0U) {
      return extent_protocol::NOENT;
    }
    attr = attr_[id];
    return extent_protocol::OK;
  }

private:
  std::unordered_set<extent_protocol::extentid_t> extent_id_cache_;
  std::unordered_map<extent_protocol::extentid_t, std::string> content_;
  std::unordered_map<extent_protocol::extentid_t, extent_protocol::attr> attr_;
  // int id_;
  static int cache_count_;
};

// int ExtentClientCache::cache_count_ = 0;

#endif