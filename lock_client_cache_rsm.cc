// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache_rsm.h"

#include <stdio.h>

#include <iostream>
#include <sstream>
#include <chrono>

#include "rpc.h"
#include "rsm_client.h"
#include "tprintf.h"

static void *releasethread(void *x) {
  auto cc = reinterpret_cast<lock_client_cache_rsm *>(x);
  cc->releaser();
  return 0;
}

int lock_client_cache_rsm::last_port = 0;

lock_client_cache_rsm::lock_client_cache_rsm(std::string xdst,
                                             class lock_release_user *_lu)
    : lock_client(xdst), lu(_lu) {
  srand(time(NULL) ^ last_port);
  rlock_port = ((rand() % 32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this,
              &lock_client_cache_rsm::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this,
              &lock_client_cache_rsm::retry_handler);
  xid = 0;
  // You fill this in Step Two, Lab 7
  // - Create rsmc, and use the object to do RPC
  //   calls instead of the rpcc object of lock_client
  // rsmc = new rsm_client(xdst);
  pthread_t th;
  int r = pthread_create(&th, NULL, &releasethread, (void *)this);
  VERIFY(r == 0);
}

void lock_client_cache_rsm::releaser() {
  // This method should be a continuous loop, waiting to be notified of
  // freed locks that have been revoked by the server, so that it can
  // send a release RPC.
  Lock *lock = nullptr;
  while(true) {
      release_fifo_.deq(lock);
      if (lu != nullptr) lu->dorelease(lock->getLockId());
      int r;
      cl->call(lock_protocol::release, lock->getLockId(),id, lock->getLockXid(), r);
      std::unique_lock<std::mutex> ulck(mutex_);
      lock->setClientLockState(ClientLockState::NONE);
      release_cv_.notify_all();
      wait_cv_.notify_all();
      ulck.unlock();
  }
}

lock_protocol::status lock_client_cache_rsm::acquire(
    lock_protocol::lockid_t lid) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = lock_protocol::OK;
  Lock* lock = nullptr;
  if (lock_table_.count(lid) == 0U) {
    lock = new Lock(lid, ClientLockState::NONE);
    lock_table_[lid] = lock;
  } else {
    lock = lock_table_[lid];
  }
  while (true) {
    switch (lock->getClientLockState()) {
      case ClientLockState::NONE: {
        lock->setClientLockState(ClientLockState::ACQUIRING);
        lock->retry_ = false;
        int r;
        lock_protocol::xid_t acquire_xid = ++xid;
        ulock.unlock();
        auto server_ret = cl->call(lock_protocol::acquire, lid, id, acquire_xid, r);
        ulock.lock();
        if (server_ret == lock_protocol::RETRY) {
          if (!lock->retry_) {
            auto start = std::chrono::system_clock::now();
            if (retry_cv_.wait_until(ulock, start + std::chrono::seconds(3)) == std::cv_status::timeout) {
              lock->retry_ = true;
            }
          }
        } else if (server_ret == lock_protocol::OK) {
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
          wait_cv_.wait(ulock);
        } else {
          // 对应第二个问题，当我们发送acquire rpc 但是 retry rpc的结果先到达,
          // 已经收到了retry就向 server请求锁
          lock->retry_ = false;
          int r;
          lock_protocol::xid_t acqueir_xid = ++xid;
          ulock.unlock();
          ret = cl->call(lock_protocol::acquire, lid, id, acqueir_xid, r);
          ulock.lock();
          if (ret == lock_protocol::OK) {
            lock->setClientLockState(ClientLockState::LOCKED);
            return ret;
          } else if (ret == lock_protocol::RETRY) {
            if (!lock->retry_) {
              auto now = std::chrono::system_clock::now();
              if (retry_cv_.wait_until(ulock, now + std::chrono::seconds(3)) == std::cv_status::timeout) {
                lock->retry_ = true;
              }
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

lock_protocol::status lock_client_cache_rsm::release(
    lock_protocol::lockid_t lid) {
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
    if (lu != nullptr) lu->dorelease(lid);
    ret = cl->call(lock_protocol::release, lid, id, lock->getLockXid(), r);
    ulock.lock();
    lock->setClientLockState(ClientLockState::NONE);
    release_cv_.notify_all();
    wait_cv_.notify_all();
    return ret;
  }
  lock->setClientLockState(ClientLockState::FREE);
  wait_cv_.notify_one();
  return lock_protocol::OK;
}

rlock_protocol::status lock_client_cache_rsm::revoke_handler(
    lock_protocol::lockid_t lid, lock_protocol::xid_t xid, int &) {
  std::unique_lock<std::mutex> ulock(mutex_);
  if (lock_table_.count(lid) == 0U ||
      lock_table_[lid]->getClientLockState() == ClientLockState::NONE) {
    return rlock_protocol::RPCERR;
  }
  auto lock = lock_table_[lid];
  if (lock->getLockXid() != xid) {
    return rlock_protocol::RPCERR;
  } 
  int ret = rlock_protocol::OK;
  if (lock->getClientLockState() == ClientLockState::FREE) {
    lock->setClientLockState(ClientLockState::RELEASING);
    lock->revoked_ = false;
    release_fifo_.enq(*lock);
    // ulock.unlock();
    // int r;
    // if (lu != nullptr) lu->dorelease(lid);
    // std::cout << "revoke lu->dorelease\n";
    // ret = cl->call(lock_protocol::release, lid, id, r);
    // ulock.lock();
    // if (ret != lock_protocol::OK) {
    //   return lock_protocol::RPCERR;
    // }
    // lock->setClientLockState(ClientLockState::NONE);
    // release_cv_.notify_all();
  } else {
    lock->revoked_ = true;
  }
  return ret;
}

rlock_protocol::status lock_client_cache_rsm::retry_handler(
    lock_protocol::lockid_t lid, lock_protocol::xid_t xid, int &) {
  std::unique_lock<std::mutex> ulock(mutex_);
  int ret = rlock_protocol::OK;
  if (lock_table_.count(lid) == 0U) {
    return rlock_protocol::RPCERR;
  }
  auto lock = lock_table_[lid];
  lock->retry_ = true;
  retry_cv_.notify_one();
  return ret;
}
