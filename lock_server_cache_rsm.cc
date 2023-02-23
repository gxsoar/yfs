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
  Lock *lock = nullptr;
  int r;
  while(true) {
    revoke_queue_.deq(&lock);
    // if (!rsm->amiprimary()) continue;
    auto &owner = lock->getLockOwner();
    handle h(owner);
    if (auto cl = h.safebind()) {
      cl->call(rlock_protocol::revoke, lock->getLockId(), lock->getOwnerXid(), r);
    }
  }
}

void lock_server_cache_rsm::retryer() {
  // This method should be a continuous loop, waiting for locks
  // to be released and then sending retry messages to those who
  // are waiting for it->
  Lock *lock = nullptr;
  int r;
  while(true) {
    retry_queue_.deq(&lock);
    // if (!rsm->amiprimary()) continue;
    auto &owner = lock->getLockOwner();
    handle h(owner);
    if (auto cl = h.safebind()) {
      cl->call(rlock_protocol::retry, lock->getLockId(), lock->getOwnerXid(), r);
    }
  }
}

int lock_server_cache_rsm::acquire(lock_protocol::lockid_t lid, std::string id,
                                   lock_protocol::xid_t xid, int &) {
  lock_protocol::status ret = lock_protocol::OK;
  std::unique_lock<std::mutex> ulock(mutex_);
  std::cout << "id " << id << " xid " << xid << "\n";
  Lock *lock = nullptr;
  if (lock_table_.count(lid) == 0U) {
    lock = new Lock(lid, ServerLockState::FREE);
    lock_table_[lid] = lock;
  } else lock = lock_table_[lid];
  
  bool revoke = false;
  if (lock->getOwnerXid() > xid) {
    return lock_protocol::RPCERR;
  } else if (lock->getOwnerXid() < xid) {
    if (lock->getServerLockState() == ServerLockState::FREE) {
      lock->setServerLockState(ServerLockState::LOCKED);
      lock->setLockOwner(id);
      lock->setOwnerXid(xid);
      ret = lock_protocol::OK;
    } else if (lock->getServerLockState() == ServerLockState::LOCKED) {
      lock->setServerLockState(ServerLockState::LOCK_AND_WAIT);
      revoke = true;
      lock->addWaitClient(id);
      ret = lock_protocol::RETRY;
    } else if (lock->getServerLockState() == ServerLockState::LOCK_AND_WAIT) {
      lock->addWaitClient(id);
      revoke_queue_.enq(lock);
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
      revoke_queue_.enq(lock);
    }
  } else {
    ret = lock->acquire_reply_[lock->getLockOwner()];
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
  if (lock->getOwnerXid() > xid) return lock_protocol::RPCERR;
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
