// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <unordered_map>
#include <string>
#include <memory>
#include "extent_protocol.h"
#include "rpc.h"
// #include "extent_client_cache.h"

class extent_client {
 private:
  rpcc *cl;

 public:
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
private:
  // std::unique_ptr<ExtentClientCache> cache_;
  // std::unordered_map<int, std::unique_ptr<ExtentClientCache>> dirty_cache;  // 用来记录脏cache
};

#endif 

