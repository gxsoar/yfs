// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <algorithm>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "extent_client/extent_client_cache.h"
#include "lang/verify.h"
#include "lock_client/lock_client.h"
#include "lock_server/lock_protocol.h"
#include "rpc/rpc.h"

// Classes that inherit lock_release_user can override dorelease so that
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user(){};
};

class lock_release : public lock_release_user {
 public:
  lock_release(extent_client_cache *ec) : ec_(ec) {}
  void dorelease(lock_protocol::lockid_t lid) {
    ec_->flush(lid);
  }
  virtual ~lock_release(){};

 private:
  // std::shared_ptr<extent_client_cache> ec_;
  extent_client_cache *ec_;
};

enum class ClientLockState { NONE, FREE, LOCKED, ACQUIRING, RELEASING };

class Lock {
 public:
  Lock(lock_protocol::lockid_t lid, ClientLockState state)
      : lid_(lid), state_(state) {}

  lock_protocol::lockid_t getLockId() { return lid_; }

  ClientLockState getClientLockState() { return state_; }

  void setClientLockState(ClientLockState state) { state_ = state; }

  bool operator==(const Lock &rhs) { return rhs.lid_ == lid_; }

 public:
  // std::condition_variable retry_cv_;
  // std::condition_variable wait_cv_;
  // std::condition_variable release_cv_;
  bool revoked_{false};
  bool retry_{false};

 private:
  lock_protocol::lockid_t lid_;
  ClientLockState state_;
};

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;

 public:
  lock_client_cache(std::string xdst, class lock_release_user *l = nullptr);
  virtual ~lock_client_cache(){};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, int &);

 private:
  std::unordered_map<lock_protocol::lockid_t, std::shared_ptr<Lock>>
      lock_table_;
  std::mutex mutex_;
  std::condition_variable retry_cv_;
  std::condition_variable wait_cv_;
  std::condition_variable release_cv_;
};

#endif
