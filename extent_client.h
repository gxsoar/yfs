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
protected:
  rpcc *cl;

 public:
  extent_client(std::string dst);

  virtual extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  virtual extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  virtual extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  virtual extent_protocol::status remove(extent_protocol::extentid_t eid);
  // virtual extent_protocol::status flush(extent_protocol::extentid_t eid);
};

#endif 

