// the lock server implementation

#include "lock_server.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <sstream>

lock_server::lock_server() : nacquire(0) {}

lock_protocol::status lock_server::stat(int clt, lock_protocol::lockid_t lid,
                                        int &r) {
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

int lock_server::accquire(int clt, lock_protocol::lockid_t lid, int &r) {
  lock_protocol::status ret = lock_protocol::OK;
  auto ite = lock_tale_.find(lid);
  std::unique_lock<std::mutex> ulock(mutex_);
  if (ite != lock_tale_.end()) {
    auto the_lock = ite->second;
    while(the_lock->lock_state_ != LockState::FREE) {
      the_lock->cv_.wait(ulock);
    }
    the_lock->lock_state_ = LockState::LOCKED;
  } else {
    lock *new_lock = new lock(lid);
    new_lock->lock_state_ = LockState::LOCKED;
    lock_tale_[lid] = new_lock;
  }
  return ret;  
}

int lock_server::release(int clt, lock_protocol::lockid_t lid, int &r) {
  std::unique_lock<std::mutex> ulock(mutex_);
  lock_protocol::status ret = lock_protocol::OK;
  auto ite = lock_tale_.find(lid);
  if (ite == lock_tale_.end()) {
    ret = lock_protocol::IOERR;
    return ret;
  }
  auto the_lock = lock_tale_[lid];
  the_lock->lock_state_ = LockState::FREE;
  the_lock->cv_.notify_all();
  return ret; 
}