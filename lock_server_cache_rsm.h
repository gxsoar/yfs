#ifndef lock_server_cache_rsm_h
#define lock_server_cache_rsm_h

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "lock_protocol.h"
#include "rpc.h"
#include "rsm.h"
#include "rsm_state_transfer.h"
#include "rpc/fifo.h"

enum class ServerLockState { FREE, LOCKED, LOCK_AND_WAIT, RETRYING };

class Lock {
 public:
  Lock(lock_protocol::lockid_t lid, ServerLockState state, lock_protocol::lockid_t xid)
      // : lid_(lid), state_(state), xid_(xid) {
      {
        std::cout << "lid_ " << lid << " xid_ " << xid << "\n";
        lid_ = lid;
        xid_ = xid;
        state_ = state;
      }

  void setServerLockState(ServerLockState state) { state_ = state; }

  ServerLockState getServerLockState() { return state_; }

  void addWaitClient(const std::string &wait_id) {
    wait_client_set_.insert(wait_id);
  }

  void deleteWaitClient(const std::string &wait_id) {
    wait_client_set_.erase(wait_id);
  }

  bool findWaitClient(const std::string &wait_id) {
    return (wait_client_set_.count(wait_id) != 0U);
  }

  std::string getWaitClient() { return *wait_client_set_.begin(); }

  bool waitClientSetEmpty() { return wait_client_set_.empty(); }

  std::string &getLockOwner() { return owner_; }

  void setLockOwner(const std::string &owner) { owner_ = owner; }

  lock_protocol::lockid_t getLockXid() { return xid_; }

  void setLockXid(lock_protocol::lockid_t xid) { xid_ = xid; }

  lock_protocol::lockid_t getLockId() { return lid_; }

 private:
  // 保存锁的持有者的id
  std::string owner_;
  // 锁的id
  lock_protocol::lockid_t lid_;
  // 保存锁的状态
  ServerLockState state_;
  // 等待该锁的集合
  std::unordered_set<std::string> wait_client_set_;
  // 请求该锁的requese id, 只记录该锁的最高的xid
  lock_protocol::lockid_t xid_{0};
};

class lock_server_cache_rsm : public rsm_state_transfer {
 private:
  int nacquire;
  class rsm *rsm;
  std::unordered_map<lock_protocol::lockid_t, std::shared_ptr<Lock>>
      lock_table_;
  std::mutex mutex_;
  fifo<std::shared_ptr<Lock>> revoke_queue_;
  fifo<std::shared_ptr<Lock>> retry_queue_;
  

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
