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
  if (lock_table_.find(lid) == lock_table_.end()) {
    lock_table_.emplace(lid, Lock(lid, ClientLockState::NONE));
  }
  auto &lock = lock_table_[lid];
  while (true) {
    switch (lock.getClientLockState()) {
      case ClientLockState::NONE: {
        lock.setClientLockState(ClientLockState::ACQUIRING);
        lock.retry_ = false;
        int r;
        ulock.unlock();
        auto server_ret = cl->call(lock_protocol::acquire, cl->id(), lid, r);
        ulock.lock();
        if (server_ret == lock_protocol::RETRY) {
          if (!lock.retry_) {
            lock.retry_cv_.wait(ulock);
          }
          break;
        } else {
          lock.setClientLockState(ClientLockState::LOCKED);
          return lock_protocol::OK;
        }
      }
      case ClientLockState::FREE: {
        lock.setClientLockState(ClientLockState::LOCKED);
        return lock_protocol::OK;
      }
      case ClientLockState::LOCKED: {
        lock.wait_cv_.wait(ulock);
        break;
      }
      case ClientLockState::ACQUIRING: {
        if (!lock.retry_) {
          // 如果没有收到retry就将其挂起
          lock.retry_cv_.wait(ulock);
        } else {
          // 对应第二个问题，当我们发送acquire rpc 但是 retry rpc的结果先到达, 已经收到了retry就向 server请求锁
          ulock.unlock();
          int r;
          ret = cl->call(lock_protocol::acquire, cl->id(), lid, r);
          ulock.lock();
          if (ret == lock_protocol::OK) {
            lock.setClientLockState(ClientLockState::LOCKED);
            return ret;
          } else if (ret == lock_protocol::RETRY) {
            if (!lock.retry_) {
              lock.release_cv_.wait(ulock);
            }
          }
        }
        break;
      }
      case ClientLockState::RELEASING: {
        lock.release_cv_.wait(ulock);
        break;
      }
    }
  }
  return ret;
}

lock_protocol::status lock_client_cache::release(lock_protocol::lockid_t lid) {
  std::unique_lock<std::mutex> ulock(mutex_);
  if (!lock_table_.count(lid)) {
    return lock_protocol::IOERR;
  }
  lock_table_[lid]->setClientLockState(ClientLockState::FREE);
  lock_table_[lid]->eraseThread(std::this_thread::get_id());
  lock_table_[lid]->cv_.notify_all();
  return lock_protocol::OK;
}

rlock_protocol::status lock_client_cache::revoke_handler(
    lock_protocol::lockid_t lid, int &) {
  std::unique_lock<std::mutex> ulock(mutex_);
  if (!lock_table_.count(lid)) {
    return rlock_protocol::RPCERR;
  }
  while (lock_table_[lid]->getClientLockState() != ClientLockState::FREE) {
    lock_table_[lid]->cv_.wait(ulock);
  }
  lock_table_[lid]->setClientLockState(ClientLockState::RELEASING);
  int r;
  ulock.unlock();
  auto ret = cl->call(lock_protocol::release, cl->id(), lid, r);
  ulock.lock();
  if (ret != lock_protocol::OK) {
    return rlock_protocol::RPCERR;
  }
  lock_table_[lid]->setClientLockState(ClientLockState::NONE);
  lock_table_[lid]->cv_.notify_all();
  lock_table_.erase(lid);
  return rlock_protocol::OK;
}

rlock_protocol::status lock_client_cache::retry_handler(
    lock_protocol::lockid_t lid, int &) {
  int ret = rlock_protocol::OK;

  return ret;
}
