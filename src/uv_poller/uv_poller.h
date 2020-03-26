#ifndef SRC_UV_POLLER_UV_POLLER_H_
#define SRC_UV_POLLER_UV_POLLER_H_

#include "uv.h"
#include "memory"

namespace node {
namespace uv_poller {

class UVPoller {
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
 public:
  UVPoller(const UVPoller&) = delete;
  void operator=(const UVPoller&) = delete;

  explicit UVPoller(uv_loop_t* loop);
  void PollEvents();
  ~UVPoller();
};

}  // namespace uv_poller
}  // namespace node

#endif  // SRC_UV_POLLER_UV_POLLER_H_
