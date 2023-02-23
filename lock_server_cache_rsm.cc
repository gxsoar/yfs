// the caching lock server implementation

#include "lock_server_cache_rsm.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <sstream>

#include "handle.h"
#include "lang/verify.h"
#include "tprintf.h"

static void *revokethread(void *x) {
  auto sc = reinterpret_cast<lock_server_cache_rsm *>(x);
  sc->revoker();
  return 0;
}

static void *retrythread(void *x) {
  auto sc = reinterpret_cast<lock_server_cache_rsm *>(x);
  sc->retryer();
  return 0;
}

lock_server_cache_rsm::lock_server_cache_rsm(class rsm *_rsm) : rsm(_rsm) {
  pthread_t th;
  int r = pthread_create(&th, NULL, &revokethread, (void *)this);
  VERIFY(r == 0);
  r = pthread_create(&th, NULL, &retrythread, (void *)this);
  VERIFY(r == 0);
  // rsm->set_state_transfer(this);
}

void lock_server_cache_rsm::revoker() {
  // This method should be a continuous loop, that sends revoke
  // messages to lock holders whenever another client wants the
  // same lock
  std::shared_ptr<Lock> lock;
  int r;
  while(true) {
    revoke_queue_.deq(&lock);
    if (!rsm->amiprimary()) continue;
    auto &owner = lock->getLockOwner();
    handle h(owner);
    if (auto cl = h.safebind()) {
      cl->call(rlock_protocol::revoke, lock->getLockId(), lock->getLockXid(), r);
    }
  }
}

void lock_server_cache_rsm::retryer() {
  // This method should be a continuous loop, waiting for locks
  // to be released and then sending retry messages to those who
  // are waiting for it.
  std::shared_ptr<Lock> lock;
  int r;
  while(true) {
    retry_queue_.deq(&lock);
    if (!rsm->amiprimary()) continue;
    auto &owner = lock->getLockOwner();
    handle h(owner);
    if (auto cl = h.safebind()) {
      cl->call(rlock_protocol::retry, lock->getLockId(), lock->getLockXid(), r);
    }
  }
}

int lock_server_cache_rsm::acquire(lock_protocol::lockid_t lid, std::string id,
                                   lock_protocol::xid_t xid, int &) {
  lock_protocol::status ret = lock_protocol::OK;
  std::unique_lock<std::mutex> ulock(mutex_);
  std::shared_ptr<Lock> lock;
  if (lock_table_.count(lid) == 0U) {
    // std::cout << "lid " << lid << " xid " << xid << "\n"; 
    lock = std::make_shared<Lock>(lid, ServerLockState::FREE, xid);
    lock_table_[lid] = lock;
  }
  lock = lock_table_[lid];
  if (lock->getLockXid() > xid) return lock_protocol::RPCERR;
  if (lock->getLockXid() < xid) lock->setLockXid(xid);
  bool revoke = false;
  if (lock->getServerLockState() == ServerLockState::FREE) {
    lock->setServerLockState(ServerLockState::LOCKED);
    lock->setLockOwner(id);
    ret = lock_protocol::OK;
  } else if (lock->getServerLockState() == ServerLockState::LOCKED) {
    lock->setServerLockState(ServerLockState::LOCK_AND_WAIT);
    revoke = true;
    lock->addWaitClient(id);
    ret = lock_protocol::RETRY;
  } else if (lock->getServerLockState() == ServerLockState::LOCK_AND_WAIT) {
    lock->addWaitClient(id);
    ret = lock_protocol::RETRY;
  } else {
    if (lock->findWaitClient(id)) {
      // 说明此时正在发送retry给该客户端，将其从集合中移除
      lock->deleteWaitClient(id);
      lock->setLockOwner(id);
      if (!lock->waitClientSetEmpty()) {
        lock->setServerLockState(ServerLockState::LOCK_AND_WAIT);
        revoke = true;
      } else {
        lock->setServerLockState(ServerLockState::LOCKED);
      }
    } else {
      lock->addWaitClient(id);
      ret = lock_protocol::RETRY;
    }
  }
  if (revoke) {
    // auto &owner = lock->getLockOwner();
    // handle h(owner);
    // ulock.unlock();
    // int r;
    // h.safebind()->call(rlock_protocol::revoke, lid, r);
    revoke_queue_.enq(lock);
  }
  return ret;
}

int lock_server_cache_rsm::release(lock_protocol::lockid_t lid, std::string id,
                                   lock_protocol::xid_t xid, int &r) {
  lock_protocol::status ret = lock_protocol::OK;
  std::unique_lock<std::mutex> ulock(mutex_);
  if (lock_table_.count(lid) == 0U) {
    return lock_protocol::RPCERR;
  }
  auto lock = lock_table_[lid];
  if (lock->getLockXid() > xid) return lock_protocol::RPCERR;
  else lock->setLockXid(xid);
  if (lock->getServerLockState() == ServerLockState::FREE ||
      lock->getServerLockState() == ServerLockState::RETRYING) {
    ret = lock_protocol::RPCERR;
  } else if (lock->getServerLockState() == ServerLockState::LOCKED) {
    lock->setServerLockState(ServerLockState::FREE);
    auto &owner = lock->getLockOwner();
    owner.clear();
    ret = lock_protocol::OK;
  } else {
    lock->setServerLockState(ServerLockState::RETRYING);
    auto &owner = lock->getLockOwner();
    owner.clear();
    if (lock->waitClientSetEmpty()) {
      ret = lock_protocol::RPCERR;
    } else {
      // auto wait_client = lock->getWaitClient();
      // handle h(wait_client);
      // int r;
      // ret = lock_protocol::OK;
      // ulock.unlock();
      // h.safebind()->call(rlock_protocol::retry, lid, r);
      retry_queue_.enq(lock);
    }
  }
  return ret;
}

std::string lock_server_cache_rsm::marshal_state() {
  std::ostringstream ost;
  std::string r;
  return r;
}

void lock_server_cache_rsm::unmarshal_state(std::string state) {}

lock_protocol::status lock_server_cache_rsm::stat(lock_protocol::lockid_t lid,
                                                  int &r) {
  printf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}
