// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"

#include <stdio.h>

#include <iostream>
#include <sstream>

#include "rpc.h"
#include "tprintf.h"

lock_client_cache::lock_client_cache(std::string xdst,
                                     class lock_release_user *_lu)
    : lock_client(xdst), lu(_lu) {
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
}

lock_protocol::status lock_client_cache::acquire(lock_protocol::lockid_t lid) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = lock_protocol::OK;
  std::shared_ptr<Lock> lock;
  if (lock_table_.count(lid) == 0U) {
    lock = std::make_shared<Lock>(lid, ClientLockState::NONE);
    lock_table_[lid] = lock;
  }
  lock = lock_table_[lid];
  while (true) {
    switch (lock->getClientLockState()) {
      case ClientLockState::NONE: {
        lock->setClientLockState(ClientLockState::ACQUIRING);
        lock->retry_ = false;
        int r;
        ulock.unlock();
        auto server_ret = cl->call(lock_protocol::acquire, lid, id, r);
        ulock.lock();
        if (server_ret == lock_protocol::RETRY) {
          if (!lock->retry_) {
            retry_cv_.wait(ulock);
          }
        } else if (server_ret == lock_protocol::OK){
          lock->setClientLockState(ClientLockState::LOCKED);
          return lock_protocol::OK;
        }
        break;
      }
      case ClientLockState::FREE: {
        lock->setClientLockState(ClientLockState::LOCKED);
        return lock_protocol::OK;
        break;
      }
      case ClientLockState::LOCKED: {
        wait_cv_.wait(ulock);
        break;
      }
      case ClientLockState::ACQUIRING: {
        if (!lock->retry_) {
          // 如果没有收到retry就将其挂起
          // retry_cv_.wait(ulock);
          wait_cv_.wait(ulock);
        } else {
          // 对应第二个问题，当我们发送acquire rpc 但是 retry rpc的结果先到达, 已经收到了retry就向 server请求锁
          ulock.unlock();
          lock->retry_ = false;
          int r;
          ret = cl->call(lock_protocol::acquire, lid, id, r);
          ulock.lock();
          if (ret == lock_protocol::OK) {
            lock->setClientLockState(ClientLockState::LOCKED);
            return ret;
          } else if (ret == lock_protocol::RETRY) {
            if (!lock->retry_) {
              // release_cv_.wait(ulock);
              retry_cv_.wait(ulock);
            }
          }
        }
        break;
      }
      case ClientLockState::RELEASING: {
        release_cv_.wait(ulock);
        break;
      }
    }
  }
  return ret;
}

lock_protocol::status lock_client_cache::release(lock_protocol::lockid_t lid) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = lock_protocol::OK;
  if (lock_table_.count(lid) == 0U) {
    return lock_protocol::NOENT;
  }
  auto ite = lock_table_.find(lid);
  auto lock = ite->second;
  if (lock->revoked_) {
    lock->revoked_ = false;
    lock->setClientLockState(ClientLockState::RELEASING);
    ulock.unlock();
    int r;
    ret = cl->call(lock_protocol::release, lid, id, r);
    ulock.lock();
    lock->setClientLockState(ClientLockState::NONE);
    release_cv_.notify_all();
    return ret;
  }
  lock->setClientLockState(ClientLockState::FREE);
  wait_cv_.notify_one();
  return ret;
}

rlock_protocol::status lock_client_cache::revoke_handler(
    lock_protocol::lockid_t lid, int &) {
  std::unique_lock<std::mutex> ulock(mutex_);
  if (lock_table_.count(lid) == 0U || lock_table_[lid]->getClientLockState() == ClientLockState::NONE) {
    return rlock_protocol::RPCERR;
  }
  auto lock = lock_table_[lid];
  int ret = rlock_protocol::OK;
  if (lock->getClientLockState() == ClientLockState::FREE) {
    lock->setClientLockState(ClientLockState::RELEASING);
    ulock.unlock();
    int r;
    ret = cl->call(lock_protocol::release, lid, id, r);
    ulock.lock();
    if (ret != lock_protocol::OK) {
      return lock_protocol::RPCERR;
    }
    lock->setClientLockState(ClientLockState::NONE);
    release_cv_.notify_all();
  }
  else {
    lock->revoked_ = true; 
  }
  return ret;
}

rlock_protocol::status lock_client_cache::retry_handler(
    lock_protocol::lockid_t lid, int &) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = rlock_protocol::OK;
  if (lock_table_.count(lid) == 0U) {
    return rlock_protocol::RPCERR;
  }
  auto lock = lock_table_[lid];
  // lock->setClientLockState(ClientLockState::FREE);
  lock->retry_ = true;
  // lock->retry_cv_.notify_all();
  retry_cv_.notify_one();
  return ret;
}
