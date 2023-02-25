// lock client interface.

#ifndef lock_client_cache_rsm_h

#define lock_client_cache_rsm_h

#include <string>
#include <atomic>
#include <unordered_map>

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

struct ClientLock {
  bool revoked_{false};
  bool retry_{false};
  ClientLockState state_;
  lock_protocol::lockid_t lid_;
  lock_protocol::xid_t xid_{0};
  ClientLock() : revoked_{false}, retry_{false} {}
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
  // std::atomic<lock_protocol::xid_t> xid{0};
  lock_protocol::xid_t xid;
  struct lock_entry {
    lock_protocol::lockid_t lid_;
    lock_protocol::xid_t xid_;
    lock_entry(lock_protocol::lockid_t lid = 0, lock_protocol::xid_t xid = 0) : lid_(lid), xid_(xid) {}
  };
  // fifo<Lock> release_fifo_;
  fifo<lock_entry> release_queue_;

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
  // std::unordered_map<lock_protocol::lockid_t, Lock*>
  //     lock_table_;
  std::unordered_map<lock_protocol::lockid_t, ClientLock> lock_table_;
  std::mutex mutex_;
  std::condition_variable retry_cv_;
  std::condition_variable wait_cv_;
  std::condition_variable release_cv_;
};

#endif
