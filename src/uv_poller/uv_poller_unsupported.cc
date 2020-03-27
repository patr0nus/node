#include "uv_poller.h"

#include "util.h"

namespace node {
namespace uv_poller {

struct UVPoller::Impl {

};

UVPoller::UVPoller(uv_loop_t* loop) {
  ABORT();
}

UVPoller::~UVPoller() {

}

void UVPoller::PollEvents() {

}

}  // namespace uv_poller
}  // namespace node
