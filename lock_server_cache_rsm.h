#ifndef lock_server_cache_rsm_h
#define lock_server_cache_rsm_h

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>

#include "lock_protocol.h"
#include "rpc.h"
#include "rsm.h"
#include "rsm_state_transfer.h"
#include "rpc/fifo.h"

enum ServerLockState { FREE, LOCKED, LOCK_AND_WAIT, RETRYING };

class Lock {
 public:
  friend class lock_server_cache_rsm;
  Lock()=default;
  Lock(lock_protocol::lockid_t lid, ServerLockState state)
      : lid_(lid), state_(state){}
  Lock(const Lock &rhs) {
    owner_ = rhs.owner_;
    lid_ = rhs.lid_;
    state_ = rhs.state_;
    wait_client_set_ = rhs.wait_client_set_;
    client_max_xid_ = rhs.client_max_xid_;
    acquire_reply_ = rhs.acquire_reply_;
    release_reply_ = rhs.release_reply_;
  }
  Lock& operator=(const Lock &rhs) {
    owner_ = rhs.owner_;
    lid_ = rhs.lid_;
    state_ = rhs.state_;
    wait_client_set_ = rhs.wait_client_set_;
    client_max_xid_ = rhs.client_max_xid_;
    acquire_reply_ = rhs.acquire_reply_;
    release_reply_ = rhs.release_reply_;
    return *this;
  }

  void setServerLockState(ServerLockState state) { 
    std::lock_guard<std::mutex> lg(lck_mutex_);
    state_ = state; 
  }

  ServerLockState getServerLockState() {
    std::lock_guard<std::mutex> lg(lck_mutex_);
    return state_; 
  }

  void addWaitClient(const std::string &wait_id) { 
    std::lock_guard<std::mutex> lg(lck_mutex_);
    wait_client_set_.insert(wait_id);
  }

  void deleteWaitClient(const std::string &wait_id) {
    std::lock_guard<std::mutex> lg(lck_mutex_);
    wait_client_set_.erase(wait_id);
  }

  bool findWaitClient(const std::string &wait_id) {
    std::lock_guard<std::mutex> lg(lck_mutex_);
    return (wait_client_set_.count(wait_id) != 0U);
  }

  std::string getWaitClient() { 
    std::lock_guard<std::mutex> lg(lck_mutex_);
    return *wait_client_set_.begin(); 
  }

  bool waitClientSetEmpty() {
    std::lock_guard<std::mutex> lg(lck_mutex_);
    return wait_client_set_.empty(); 
  }

  std::string &getLockOwner() { 
    std::lock_guard<std::mutex> lg(lck_mutex_);
    return owner_; 
  }

  void setLockOwner(const std::string &owner) { 
    std::lock_guard<std::mutex> lg(lck_mutex_);
    owner_ = owner; 
  }

  lock_protocol::lockid_t getLockId() { 
    std::lock_guard<std::mutex> lg(lck_mutex_);
    return lid_; 
  }

  lock_protocol::xid_t getOwnerXid() { 
    std::lock_guard<std::mutex> lg(lck_mutex_);
    if (client_max_xid_.count(owner_) == 0) return -1;
    return client_max_xid_[owner_];
  }

  void setClientXid(std::string id, lock_protocol::xid_t xid) {
    std::lock_guard<std::mutex> lg(lck_mutex_);
    client_max_xid_[id] = xid;
  }

  bool findClientId(std::string id) {
    std::lock_guard<std::mutex> lg(lck_mutex_);
    return client_max_xid_.count(id) != 0;
  }

  lock_protocol::xid_t getClientXid(std::string id) {
    std::lock_guard<std::mutex> lg(lck_mutex_);
    return client_max_xid_[id];
  }
 private:
  // 保存锁的持有者的id
  std::string owner_;
  // 锁的id
  lock_protocol::lockid_t lid_;
  // 保存锁的状态
  ServerLockState state_;
  // 等待该锁的集合
  std::unordered_set<std::string> wait_client_set_;

  std::map<std::string, lock_protocol::xid_t> client_max_xid_;
  // 不同id 对应的响应也不同
  std::map<std::string, int> acquire_reply_;
  std::map<std::string, int> release_reply_;
  std::mutex lck_mutex_;
};

struct ServerLock {
  ServerLockState state_;
  std::string owner_;
  bool revoked_ = false;
  std::set<std::string> wait_client_set_;
  std::map<std::string, lock_protocol::xid_t> client_max_xid_;
  std::map<std::string, int> client_acquire_reply_;
  std::map<std::string, int> client_release_reply_;
  ServerLock() : state_(ServerLockState::FREE), revoked_(false) {}
};

class lock_server_cache_rsm : public rsm_state_transfer {
 private:
  int nacquire;
  class rsm *rsm;
  // std::unordered_map<lock_protocol::lockid_t, std::shared_ptr<Lock>>
  //     lock_table_;
  // std::unordered_map<lock_protocol::lockid_t, Lock> lock_table_;
  struct lock_entry {
    std::string id_;
    lock_protocol::lockid_t lid_;
    lock_protocol::xid_t xid_;
    lock_entry(const std::string &id = "", lock_protocol::lockid_t lid = 0, lock_protocol::xid_t xid = 0) : id_(id), lid_(lid), xid_(xid) {}
  };
  std::unordered_map<lock_protocol::lockid_t, ServerLock> lock_table_;
  std::mutex mutex_;
  // fifo<Lock*> revoke_queue_;
  // fifo<Lock*> retry_queue_;

  fifo<lock_entry> revoke_queue_;
  fifo<lock_entry> retry_queue_;
  // fifo<std::shared_ptr<Lock>> revoke_queue_;
  // fifo<std::shared_ptr<Lock>> retry_queue_;

  // fifo<Lock> revoke_queue_;
  // fifo<Lock> retry_queue_;
  
 public:
  lock_server_cache_rsm(class rsm *rsm = 0);
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  void revoker();
  void retryer();
  std::string marshal_state();
  void unmarshal_state(std::string state);
  int acquire(lock_protocol::lockid_t, std::string id, lock_protocol::xid_t,
              int &);
  int release(lock_protocol::lockid_t, std::string id, lock_protocol::xid_t,
              int &);
};

#endif
