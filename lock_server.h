// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <map>
#include <unordered_map>
#include <mutex>

#include "lock_client.h"
#include "lock_protocol.h"
#include "rpc.h"

class lock_server {
 protected:
  int nacquire;
  std::unordered_map<lock_protocol::lockid_t, lock*> lock_tale_;
  std::mutex mutex_;
 public:
  lock_server();
  ~lock_server(){};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  int accquire(int clt, lock_protocol::lockid_t lid, int &);
  int release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif
