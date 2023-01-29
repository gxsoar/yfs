// the caching lock server implementation

#include "lock_server_cache.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <sstream>

#include "handle.h"
#include "lang/verify.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache() {}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id,
                               int &) {
  std::unique_lock<std::mutex> ulock(mutex_);
  if (lock_table_.count(lid) == 0U) {
    lock_table_[lid] = Lock(lid, ServerLockState::FREE);
  }
  auto &lock = lock_table_[lid];
  lock_protocol::status ret = lock_protocol::OK;
  bool revoke = false;
  if (lock.getServerLockState() == ServerLockState::FREE) {
    lock.setServerLockState(ServerLockState::LOCKED);
    lock.setLockOwner(id);
    ret = lock_protocol::OK;
  } else if (lock.getServerLockState() == ServerLockState::LOCKED) {
    lock.setServerLockState(ServerLockState::LOCK_AND_WAIT);
    revoke = true;
    lock.addWaitClient(id);
    ret = lock_protocol::RETRY;
  } else if (lock.getServerLockState() == ServerLockState::LOCK_AND_WAIT) {
    lock.addWaitClient(id);
    ret = lock_protocol::RETRY;
  } else {
    if (lock.findWaitClient(id)) {
      // 说明此时正在发送retry给该客户端，将其从集合中移除
      lock.deleteWaitClient(id);
      lock.setLockOwner(id);
    } else {
      if (lock.waitClientSetEmpty()) {
        lock.setServerLockState(ServerLockState::LOCKED);
        lock.setLockOwner(id);
        ret = lock_protocol::OK;
      } else {
        lock.setServerLockState(ServerLockState::LOCK_AND_WAIT);
        lock.addWaitClient(id);
        revoke = true;
        ret = lock_protocol::RETRY;
      }
    }
  }
  if (revoke) {
    auto &owner = lock.getLockOwner();
    handle h(owner);
    ulock.unlock();
    int r;
    h.safebind()->call(rlock_protocol::revoke, lid, r);
  }
  return ret;
}

int lock_server_cache::release(lock_protocol::lockid_t lid, std::string id,
                               int &r) {
  lock_protocol::status ret = lock_protocol::OK;
  std::unique_lock<std::mutex> ulock(mutex_);
  if (lock_table_.count(lid) == 0U) {
    return lock_protocol::RPCERR;
  }
  auto &lock = lock_table_[lid];
  if (lock.getServerLockState() == ServerLockState::FREE || lock.getServerLockState() == ServerLockState::RETRYING) {
    ret = lock_protocol::RPCERR;
  } else if (lock.getServerLockState() == ServerLockState::LOCKED) {
    lock.setServerLockState(ServerLockState::FREE);
    auto &owner = lock.getLockOwner();
    owner.clear();
    ret = lock_protocol::OK;
  } else {
    lock.setServerLockState(ServerLockState::RETRYING);
    auto &owner = lock.getLockOwner();
    owner.clear();
    if (lock.waitClientSetEmpty()) {
      ret = lock_protocol::RPCERR;
    } else {
      auto wait_client = lock.getWaitClient();
      handle h(wait_client);
      int r;
      ret = lock_protocol::OK;
      ulock.unlock();
      h.safebind()->call(rlock_protocol::retry, lid, r);
    }
  }
  return ret;
}

lock_protocol::status lock_server_cache::stat(lock_protocol::lockid_t lid,
                                              int &r) {
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}
