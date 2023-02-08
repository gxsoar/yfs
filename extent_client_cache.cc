#include "extent_client_cache.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extent_client_cache::extent_client_cache(std::string dst) : extent_client(dst) {
  // extent_client_cache::put(1, "");
}

extent_protocol::status extent_client_cache::put(
    extent_protocol::extentid_t eid, std::string buf) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  extent_cache_[eid].buf_ = buf;
  extent_cache_[eid].state_ = ExtentState::DIRTY;
  extent_protocol::attr att;
  att.atime = att.ctime = att.mtime = time(nullptr);
  att.size = buf.size();
  extent_cache_[eid].attr_ = att;
  return ret;
}

extent_protocol::status extent_client_cache::get(
    extent_protocol::extentid_t eid, std::string &buf) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  if (extent_cache_.count(eid) == 0 ||
      extent_cache_[eid].state_ == ExtentState::NONE) {
    ulock.unlock();
    ret = cl->call(extent_protocol::get, eid, buf);
    if (ret != extent_protocol::OK) return ret;
    extent_protocol::attr att;
    getattr(eid, att);
    ulock.lock();
    extent_cache_[eid].state_ = ExtentState::CONSISTENT;
    extent_cache_[eid].buf_ = buf;
    extent_cache_[eid].attr_ = att;
  } else {
    buf = extent_cache_[eid].buf_;
    extent_cache_[eid].attr_.atime = time(nullptr);
  }
  return ret;
}

extent_protocol::status extent_client_cache::getattr(
    extent_protocol::extentid_t eid, extent_protocol::attr &a) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  if (extent_cache_.count(eid) == 0U) {
    ulock.unlock();
    ret = cl->call(extent_protocol::getattr, eid, a);
    if (ret != extent_protocol::OK) {
      return ret;
    }
    ulock.lock();
    extent_cache_[eid].attr_ = a;
    extent_cache_[eid].state_ = ExtentState::NONE;
  } else {
    if (extent_cache_[eid].state_ == ExtentState::REMOVE) {
      return extent_protocol::NOENT;
    }
    a = extent_cache_[eid].attr_;
  }
  return ret;
}

extent_protocol::status extent_client_cache::remove(
    extent_protocol::extentid_t eid) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  if (extent_cache_[eid].state_ == ExtentState::REMOVE) {
    return extent_protocol::NOENT;
  }
  extent_cache_[eid].state_ = ExtentState::REMOVE;
  return ret;
}

extent_protocol::status extent_client_cache::flush(
    extent_protocol::extentid_t eid) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  int r;
  if (extent_cache_.count(eid) == 0 ||
      extent_cache_[eid].state_ == ExtentState::NONE) {
    ret = extent_protocol::NOENT;
  } else {
    if (extent_cache_[eid].state_ == ExtentState::DIRTY) {
      ulock.unlock();
      ret = cl->call(extent_protocol::put, eid, extent_cache_[eid].buf_, r);
      ulock.lock();
      extent_cache_.erase(eid);
    } else if (extent_cache_[eid].state_ == ExtentState::REMOVE) {
      ulock.unlock();
      ret = cl->call(extent_protocol::remove, eid, r);
      ulock.lock();
      extent_cache_.erase(eid);
    } else {
      extent_cache_.erase(eid);
    }
  }
  return ret;
}