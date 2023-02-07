#ifndef extent_client_cache_h
#define extent_client_cache_h

#include <unordered_set>
#include <string>
#include <atomic>
#include <mutex>

#include "extent_protocol.h"
#include "extent_client.h"


class extent_client_cache : public extent_client {
public:
  // DIRTY put操作，当cache的内容和server的内容不一致
  // remove 在cache中已经被移除
  // none cache中只有其属性没有其任何内容
  // CONSISTENT，cache的内容和server中的内容一致
  enum  ExtentState { DIRTY, REMOVE, NONE, CONSISTENT };

  extent_client_cache(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, std::string &buf);

  extent_protocol::status getattr(extent_protocol::extentid_t eid, extent_protocol::attr &a);

  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);

  extent_protocol::status remove(extent_protocol::extentid_t eid);

  extent_protocol::status flush(extent_protocol::extentid_t eid);


private:
  struct eid_content {
    extent_protocol::extentid_t id_;
    std::string buf_;
    extent_protocol::attr attr_;
    ExtentState state_;
    eid_content(extent_protocol::extentid_t id, ExtentState state = ExtentState::NONE) : id_(id), state_(state) {}
    eid_content(extent_protocol::extentid_t id) : id_(id) {};
    eid_content() = default;
  };
  std::unordered_map<extent_protocol::extentid_t, eid_content> extent_cache_;
  std::mutex mutex_;
};

#endif