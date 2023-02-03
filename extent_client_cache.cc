#include "extent_client_cache.h"

extent_protocol::status extent_client_cache::get(extent_protocol::extentid_t eid, std::string &buf) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  if (extent_cache_.count(eid) == 0U) {
    ulock.unlock();
    ret = cl->call(extent_protocol::get, eid, buf);
    extent_cache_[eid].buf_ = buf;
    extent_cache_[eid].state_ = ExtentState::CONSISTENT;
    return ret;
  }
  buf = extent_cache_[eid].buf_;
  return ret;
}


extent_protocol::status extent_client_cache::getattr(extent_protocol::extentid_t eid, extent_protocol::attr &a) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  if (extent_cache_.count(eid) == 0U) {
    ulock.unlock();
    ret = cl->call(extent_protocol::getattr, eid, a);
    extent_cache_[eid].attr_ = a;
    extent_cache_[eid].state_ = ExtentState::CONSISTENT;
    return ret;
  }
  a = extent_cache_[eid].attr_;
  return ret;
}

extent_protocol::status extent_client::put(extent_protocol::extentid_t eid, std::string buf) {
  
}
