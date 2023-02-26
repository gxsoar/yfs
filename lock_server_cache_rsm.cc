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
  rsm->set_state_transfer(this);
}

void lock_server_cache_rsm::revoker() {
  // This method should be a continuous loop, that sends revoke
  // messages to lock holders whenever another client wants the
  // same lock
  // Lock *lock = nullptr;
  
  int r;
  while(true) {
    lock_entry tmp;
    revoke_queue_.deq(&tmp);
    if (!rsm->amiprimary()) continue;
    auto &owner = tmp.id_;
    handle h(owner);
    if (auto cl = h.safebind()) {
      cl->call(rlock_protocol::revoke, tmp.lid_, tmp.xid_, r);
    }
  }
}

void lock_server_cache_rsm::retryer() {
  // This method should be a continuous loop, waiting for locks
  // to be released and then sending retry messages to those who
  // are waiting for it->
  // Lock *lock = nullptr;
  int r;
  while(true) {
    lock_entry tmp;
    retry_queue_.deq(&tmp);
    if (!rsm->amiprimary()) continue;
    auto &owner = tmp.id_;
    handle h(owner);
    if (auto cl = h.safebind()) {
      cl->call(rlock_protocol::retry, tmp.lid_, tmp.xid_, r);
    }
  }
}

// int lock_server_cache_rsm::acquire(lock_protocol::lockid_t lid, std::string id,
//                                    lock_protocol::xid_t xid, int &) {
//   lock_protocol::status ret = lock_protocol::OK;
//   std::lock_guard<std::mutex> lg(mutex_);
//   // std::shared_ptr<Lock> lock;
//   // Lock lock;
//   if (lock_table_.count(lid) == 0U) {
//     // lock = Lock (lid, ServerLockState::FREE);
//     // lock_table_[lid] = lock;
//     lock_table_[lid] = ServerLock();
//   } 
//   // else lock = lock_table_[lid];
//   auto &lock = lock_table_[lid];
//   auto xid_ite = lock.client_max_xid_.find(id);
//   // Lock *lock = &lock_table[lid];
//   bool revoke = false;
//   if (xid_ite == lock.client_max_xid_.end() || xid_ite->second < xid) {
//     // lock.setClientXid(id, xid);
//     lock.client_max_xid_[id] = xid;
//     lock.client_relsease_reply_.erase(id);
//     if (lock.release_reply_.count(id)) lock.release_reply_.erase(id);
//     if (lock.getServerLockState() == ServerLockState::FREE) {
//       lock.setServerLockState(ServerLockState::LOCKED);
//       lock.setLockOwner(id);
//       ret = lock_protocol::OK;
//     } else if (lock.getServerLockState() == ServerLockState::LOCKED) {
//       lock.setServerLockState(ServerLockState::LOCK_AND_WAIT);
//       revoke = true;
//       lock.addWaitClient(id);
//       ret = lock_protocol::RETRY;
//     } else if (lock.getServerLockState() == ServerLockState::LOCK_AND_WAIT) {
//       lock.addWaitClient(id);
//       revoke_queue_.enq(&lock);
//       ret = lock_protocol::RETRY;
//     } else {
//       if (lock.findWaitClient(id)) {
//         // 说明此时正在发送retry给该客户端，将其从集合中移除
//         lock.deleteWaitClient(id);
//         lock.setLockOwner(id);
//         if (!lock.waitClientSetEmpty()) {
//           lock.setServerLockState(ServerLockState::LOCK_AND_WAIT);
//           revoke = true;
//         } else {
//           lock.setServerLockState(ServerLockState::LOCKED);
//         }
//         ret = lock_protocol::OK;
//       } else {
//         lock.addWaitClient(id);
//         ret = lock_protocol::RETRY;
//       }
//     }
//     lock.acquire_reply_[id] = ret;
//     if (revoke) {
//       revoke_queue_.enq(&lock);
//     }
//   } else if (lock.getClientXid(id) > xid) {
//     return lock_protocol::RPCERR;
//   } else {
//     ret = lock.acquire_reply_[lock.getLockOwner()];
//   }
//   return ret;
// }

int lock_server_cache_rsm::acquire(lock_protocol::lockid_t lid, std::string id,
                                   lock_protocol::xid_t xid, int &) {
  lock_protocol::status ret = lock_protocol::OK;
  std::lock_guard<std::mutex> lg(mutex_);
  if (lock_table_.count(lid) == 0U) {
    lock_table_[lid] = ServerLock();
  } 
  auto &lock = lock_table_[lid];
  auto xid_ite = lock.client_max_xid_.find(id);
  if (xid_ite == lock.client_max_xid_.end() || xid_ite->second < xid) {
    lock.client_max_xid_[id] = xid;
    lock.client_release_reply_.erase(id);
    if (lock.state_ == ServerLockState::FREE) {
      lock.state_ = ServerLockState::LOCKED;
      lock.owner_ = id;
      ret = lock_protocol::OK;
    } else if (lock.state_ == ServerLockState::LOCKED) {
      lock.state_ = ServerLockState::LOCK_AND_WAIT;
      revoke_queue_.enq(lock_entry(lock.owner_, lid, lock.client_max_xid_[lock.owner_]));
      lock.wait_client_set_.insert(id);
      ret = lock_protocol::RETRY;
    } else if (lock.state_ == ServerLockState::LOCK_AND_WAIT) {
      lock.wait_client_set_.insert(id);
      revoke_queue_.enq(lock_entry(lock.owner_, lid, lock.client_max_xid_[lock.owner_]));
      ret = lock_protocol::RETRY;
    } else {
      if (lock.wait_client_set_.count(id)) {
        // 说明此时正在发送retry给该客户端，将其从集合中移除
        lock.wait_client_set_.erase(id);
        lock.owner_ = id;
        if (!lock.wait_client_set_.empty()) {
          lock.state_ = ServerLockState::LOCK_AND_WAIT;
          revoke_queue_.enq(lock_entry(lock.owner_, lid, lock.client_max_xid_[lock.owner_]));
        } else {
          lock.state_ = ServerLockState::LOCKED;
        }
        ret = lock_protocol::OK;
      } else {
        lock.wait_client_set_.insert(id);
        ret = lock_protocol::RETRY;
      }
    }
    lock.client_acquire_reply_[id] = ret;
  } else if (xid_ite->second > xid) {
    ret = lock_protocol::RPCERR;
  } else if (xid_ite->second == xid){
    ret = lock.client_acquire_reply_[id];
  }
  return ret;
}

// int lock_server_cache_rsm::release(lock_protocol::lockid_t lid, std::string id,
//                                    lock_protocol::xid_t xid, int &r) {
//   lock_protocol::status ret = lock_protocol::OK;
//   // std::unique_lock<std::mutex> ulock(mutex_);
//   std::lock_guard<std::mutex> lg(mutex_);
//   if (lock_table_.count(lid) == 0U) {
//     return lock_protocol::RPCERR;
//   }
//   // Lock *lock = &lock_table_[lid];
//   auto lock = lock_table_[lid];
//   // auto &client_max_xid = lock->client_max_xid_;
//   // auto ite = client_max_xid.find(id);
//   if (lock.findClientId(id) && lock.getClientXid(id) == xid) {
//     if (lock.release_reply_.find(id) == lock.release_reply_.end()) {
//       if (lock.getServerLockState() == ServerLockState::FREE ||
//           lock.getServerLockState() == ServerLockState::RETRYING) {
//         ret = lock_protocol::RPCERR;
//       } else if (lock.getServerLockState() == ServerLockState::LOCKED) {
//         lock.setServerLockState(ServerLockState::FREE);
//         auto &owner = lock.getLockOwner();
//         owner.clear();
//         ret = lock_protocol::OK;
//       } else {
//         lock.setServerLockState(ServerLockState::RETRYING);
//         auto &owner = lock.getLockOwner();
//         owner.clear();
//         if (lock.waitClientSetEmpty()) {
//           ret = lock_protocol::RPCERR;
//         } else {
//           ret = lock_protocol::OK;
//           retry_queue_.enq(&lock);
//         }
//       }
//       lock.release_reply_[id] = ret;
//     } else {
//       ret = lock.release_reply_[id];
//     }
//   } else {
//     ret = lock_protocol::RPCERR;
//   }
//   return ret;
// }

int lock_server_cache_rsm::release(lock_protocol::lockid_t lid, std::string id,
                                   lock_protocol::xid_t xid, int &r) {
  lock_protocol::status ret = lock_protocol::OK;
  std::lock_guard<std::mutex> lg(mutex_);
  if (lock_table_.count(lid) == 0U) {
    return lock_protocol::RPCERR;
  }
  auto &lock = lock_table_[lid];
  auto xid_ite = lock.client_max_xid_.find(id);
  if (xid_ite != lock.client_max_xid_.end() && xid_ite->second == xid) {

    if (lock.client_release_reply_.find(id) == lock.client_release_reply_.end()) {
      if (lock.owner_ != id) {
        ret = lock_protocol::IOERR;
        lock.client_release_reply_[id] = ret;
      }
      if (lock.state_ == ServerLockState::FREE ||
          lock.state_ == ServerLockState::RETRYING) {
        ret = lock_protocol::RPCERR;
      } else if (lock.state_ == ServerLockState::LOCKED) {
        lock.state_ = ServerLockState::FREE;
        auto &owner = lock.owner_;
        owner.clear();
        ret = lock_protocol::OK;
      } else {
        lock.state_ = ServerLockState::RETRYING;
        auto &owner = lock.owner_;
        owner.clear();
        ret = lock_protocol::OK;
        auto retry_id = *lock.wait_client_set_.begin();
        retry_queue_.enq(lock_entry(retry_id, lid, lock.client_max_xid_[retry_id]));
      }
      lock.client_release_reply_[id] = ret;
    } else {
      ret = lock.client_release_reply_[id];
    }
  } else {
    ret = lock_protocol::RPCERR;
  }
  return ret;
}

std::string lock_server_cache_rsm::marshal_state() {
  std::lock_guard<std::mutex> lck(mutex_);
  using ull = unsigned long long;
  std::ostringstream ost;
  std::string r;
  marshall rep;
  rep << static_cast<ull>(lock_table_.size());
  // 注意对齐marshal lock 的 lock 顺序和 unmarshal的逆序
  for (auto &[lid, lock] : lock_table_) {
    rep << lid;
    rep << lock.owner_;
    int state = lock.state_;
    rep << state;
    int revoked = lock.revoked_;
    rep << revoked;
    int wait_client_set_size = lock.wait_client_set_.size();
    rep << wait_client_set_size;
    for (auto &wait_client : lock.wait_client_set_) {
      rep << wait_client;
    }
    int client_max_xid_size = lock.client_max_xid_.size();
    rep << client_max_xid_size;
    for (auto &[id, xid] : lock.client_max_xid_) {
      rep << id << xid;
    }
    int acquire_reply_size = lock.client_acquire_reply_.size();
    for (auto &[id, reply] : lock.client_acquire_reply_) {
      rep << id << reply;
    }
    int release_reply_size = lock.client_release_reply_.size();
    for (auto &[id, reply] : lock.client_release_reply_) {
      rep << id << reply;
    }
  }
  r = rep.str();
  return r;
}

void lock_server_cache_rsm::unmarshal_state(std::string state) {
  std::lock_guard<std::mutex> lck(mutex_);
  using ull = unsigned long long;
  unmarshall rep(state);
  ull locks_size;
  rep >> locks_size;
  for (unsigned int i = 0; i < locks_size; ++ i) {
    lock_protocol::lockid_t lid;
    rep >> lid;
    ServerLock lock;
    int state;
    rep >> lock.owner_ >> state;
    lock.state_ = static_cast<ServerLockState>(state);
    int revoked;
    rep >> revoked;
    lock.revoked_ = revoked;
    int wait_client_size;
    rep >> wait_client_size;
    for (int i = 0; i < wait_client_size; ++ i) {
      std::string str;
      rep >> str;
      lock.wait_client_set_.insert(str);
    }
    int client_max_xid_size;
    rep >> client_max_xid_size;
    for (int i = 0; i < client_max_xid_size; ++ i) {
      std::string id;
      ull xid;
      rep >> id >> xid;
      lock.client_max_xid_[id] = xid;
    }
    int acquire_reply_size;
    rep >> acquire_reply_size;
    for (int i = 0; i < acquire_reply_size; ++ i) {
      std::string id;
      int reply;
      rep >> id >> reply;
      lock.client_acquire_reply_[id] = reply;
    }
    int release_reply_size;
    rep >> release_reply_size;
    for (int i = 0; i < release_reply_size; ++ i) {
      std::string id;
      int reply;
      rep >> id >> reply;
      lock.client_release_reply_[id] = reply;
    }
    lock_table_[lid] = lock;
  }
}

lock_protocol::status lock_server_cache_rsm::stat(lock_protocol::lockid_t lid,
                                                  int &r) {
  printf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}
