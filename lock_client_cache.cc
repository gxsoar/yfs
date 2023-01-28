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
  // 先检查是否有其他线程持有该锁
  if (lock_table_.count(lid)) {
    while(lock_table_[lid]->getClientLockState() == ClientLockState::LOCKED) {
      lock_table_[lid]->cv_.wait(ulock);
    }
    if (lock_table_[lid]->getClientLockState() == ClientLockState::FREE) {
      lock_table_[lid]->setClientLockState(ClientLockState::LOCKED);
      lock_table_[lid]->addThread(std::this_thread::get_id());
      return lock_protocol::OK;
    }
  }
  lock_table_[lid]->setClientLockState(ClientLockState::ACQUIRING);
  int r;
  auto ret = cl->call(lock_protocol::acquire, cl->id(), lid, r);
  if (ret == lock_protocol::OK) {
    lock_table_[lid]->setClientLockState(ClientLockState::LOCKED);
    lock_table_[lid]->addThread(std::this_thread::get_id());
  } else {
    lock_table_[lid]->setClientLockState(ClientLockState::NONE);
  }
  return ret;
}

lock_protocol::status lock_client_cache::release(lock_protocol::lockid_t lid) {
  if (!lock_table_.count(lid)) {
    return lock_protocol::IOERR;
  }
  lock_table_[lid]->setClientLockState(ClientLockState::FREE);
  // 唤醒所有等待的线程
  lock_table_[lid]->cv_.notify_all();
  return lock_protocol::OK;
}

rlock_protocol::status lock_client_cache::revoke_handler(
    lock_protocol::lockid_t lid, int &) {
  int ret = rlock_protocol::OK;
  return ret;
}

rlock_protocol::status lock_client_cache::retry_handler(
    lock_protocol::lockid_t lid, int &) {
  int ret = rlock_protocol::OK;
  return ret;
}
