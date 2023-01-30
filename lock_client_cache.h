// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include <mutex>
#include <thread>
#include <list>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <unordered_set>

#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

enum class ClientLockState { NONE, FREE, LOCKED, ACQUIRING, RELEASING };

class Lock {
public:
  Lock(lock_protocol::lockid_t lid, ClientLockState state) : lid_(lid), state_(state) {}

  lock_protocol::lockid_t getLockId() { return lid_; }

  ClientLockState getClientLockState() { return state_; }

  void setClientLockState(ClientLockState state) { state_ = state; }

  bool operator==(const Lock &rhs) { return rhs.lid_ == lid_; }

public:
  std::condition_variable retry_cv_;
  std::condition_variable wait_cv_;
  std::condition_variable release_cv_;
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
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
private:
  std::unordered_map<lock_protocol::lockid_t, Lock> lock_table_;
  std::mutex mutex_;
};

#endif
