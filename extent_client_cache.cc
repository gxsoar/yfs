#include "extent_client_cache.h"

extent_protocol::status extent_client_cache::get(extent_protocol::extentid_t eid, std::string &buf) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  if (extent_cache_.count(eid) == 0U) {
    ulock.unlock();
    ret = cl->call(extent_protocol::get, eid, buf);
    extent_cache_[eid].buf_ = buf;
    extent_cache_[eid].state_ = ExtentState::CONSISTENT;
  } else if (extent_cache_[eid].state_ == ExtentState::REMOVE) {
    ret = extent_protocol::NOENT;
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

extent_protocol::status extent_client_cache::put(extent_protocol::extentid_t eid, std::string buf) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  extent_cache_[eid].buf_ = buf;
  extent_cache_[eid].state_ = ExtentState::DIRTY;
  return ret;
}

extent_protocol::status extent_client_cache::remove(extent_protocol::extentid_t eid) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  if (extent_cache_.count(eid) == 0U) {
    return extent_protocol::NOENT;
  }
  extent_cache_[eid].state_ = ExtentState::REMOVE;
  return ret;
}

extent_protocol::status extent_client_cache::flush(extent_protocol::extentid_t eid) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  int r;
  ulock.unlock();
  if (extent_cache_[eid].state_ == ExtentState::DIRTY) {
    ret = cl->call(extent_protocol::put, eid, extent_cache_[eid].buf_, r);
  } else if (extent_cache_[eid].state_ == ExtentState::REMOVE) {
    ret = cl->call(extent_protocol::remove, eid, r);
  }
  return ret;
}