#include "extent_client_cache.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>

extent_client_cache::extent_client_cache(std::string dst) : extent_client(dst) {
  // extent_client_cache::put(1, "");
}

extent_protocol::status extent_client_cache::put(extent_protocol::extentid_t eid, std::string buf) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  extent_cache_[eid].buf_ = buf;
  extent_cache_[eid].state_ = ExtentState::DIRTY;
  extent_cache_[eid].attr_.size = buf.size();
  extent_protocol::attr att;
  att.atime = att.ctime = att.mtime = time(nullptr);
  extent_cache_[eid].attr_ = att;
  std::cout << "extent_client_cache::put " << eid << " buf " << buf << std::endl;
  return ret;
}

extent_protocol::status extent_client_cache::get(extent_protocol::extentid_t eid, std::string &buf) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
 
  if (extent_cache_.count(eid) == 0U || extent_cache_[eid].state_ == ExtentState::DIRTY) {
    std::cout << "extent_cache.count(eid)\n";
    ulock.unlock();
    ret = cl->call(extent_protocol::get, eid, buf);
    extent_cache_[eid].buf_ = buf;
    extent_cache_[eid].state_ = ExtentState::CONSISTENT;
  } else if (extent_cache_[eid].state_ == ExtentState::REMOVE) {
    ret = extent_protocol::NOENT;
  } else {
    buf = extent_cache_[eid].buf_;
  }
  std::cout << "extent_client_cache::get " << eid << " buf " << extent_cache_[eid].buf_ << std::endl;
  std::cout << "extent_client_cache " << eid << " eid state " << extent_cache_[eid].state_ << "\n";
  extent_cache_[eid].attr_.atime = time(nullptr);
  
  return ret;
}


extent_protocol::status extent_client_cache::getattr(extent_protocol::extentid_t eid, extent_protocol::attr &a) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = extent_protocol::OK;
  if (extent_cache_.count(eid) == 0U) {
    ulock.unlock();
    ret = cl->call(extent_protocol::getattr, eid, a);
    extent_cache_[eid].attr_ = a;
    // extent_cache_[eid].state_ = ExtentState::CONSISTENT;
    return ret;
  }
  a = extent_cache_[eid].attr_;
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
    extent_cache_[eid].state_ = ExtentState::CONSISTENT;
  } else if (extent_cache_[eid].state_ == ExtentState::REMOVE) {
    ret = cl->call(extent_protocol::remove, eid, r);
    extent_cache_[eid].state_ = ExtentState::REMOVE;
  }
  std::cout << "extent_client_cache::flush\n";
  extent_cache_.erase(eid);
  return ret;
}