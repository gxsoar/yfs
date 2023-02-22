// lock client interface.

#ifndef lock_client_cache_rsm_h

#define lock_client_cache_rsm_h

#include <string>
#include <atomic>

#include "extent_client_cache.h"
#include "lang/verify.h"
#include "lock_client.h"
#include "lock_protocol.h"
#include "rpc.h"
#include "rsm_client.h"
#include "rpc/fifo.h"

// Classes that inherit lock_release_user can override dorelease so that
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user(){};
};

class lock_client_cache_rsm;

class lock_release : public lock_release_user {
 public:
  lock_release(extent_client_cache *ec) : ec_(ec) {}
  void dorelease(lock_protocol::lockid_t lid) {
    ec_->flush(lid);
  };
  virtual ~lock_release(){};

 private:
  extent_client_cache *ec_;
};

enum class ClientLockState { NONE, FREE, LOCKED, ACQUIRING, RELEASING };

class Lock {
 public:
  Lock(lock_protocol::lockid_t lid, ClientLockState state)
      : lid_(lid), state_(state) {}

  lock_protocol::lockid_t getLockId() { return lid_; }

  lock_protocol::lockid_t getLockXid() { return xid_; }

  void setLockXid(lock_protocol::lockid_t xid) { xid_ = xid; }

  ClientLockState getClientLockState() { return state_; }

  void setClientLockState(ClientLockState state) { state_ = state; }

  bool operator==(const Lock &rhs) { return rhs.lid_ == lid_; }

 public:
  bool revoked_{false};
  bool retry_{false};

 private:
  lock_protocol::lockid_t lid_;
  lock_protocol::lockid_t xid_{0}; // 每个锁都有一个对应的请求的序列号
  ClientLockState state_;
};

// Clients that caches locks.  The server can revoke locks using
// lock_revoke_server.
class lock_client_cache_rsm : public lock_client {
 private:
  rsm_client *rsmc;
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  std::atomic<lock_protocol::xid_t> xid{0};
  fifo<std::shared_ptr<Lock>> release_fifo_;

 public:
  static int last_port;
  lock_client_cache_rsm(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache_rsm(){};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  virtual lock_protocol::status release(lock_protocol::lockid_t);
  void releaser();
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t,
                                        lock_protocol::xid_t, int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t,
                                       lock_protocol::xid_t, int &);

 private:
  std::unordered_map<lock_protocol::lockid_t, std::shared_ptr<Lock>>
      lock_table_;
  std::mutex mutex_;
  std::condition_variable retry_cv_;
  std::condition_variable wait_cv_;
  std::condition_variable release_cv_;
};

#endif
