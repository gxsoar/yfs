#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <map>
#include <string>

#include "lock_protocol.h"
#include "lock_server.h"
#include "rpc.h"

class lock_server_cache {
 private:
  int nacquire;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
