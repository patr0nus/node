#include "uv_poller.h"

#include <sys/select.h>

namespace node {
namespace uv_poller {

struct UVPoller::Impl {
  uv_loop_t* uv_loop_;
  uv_async_t dummy_uv_handle_;
};

UVPoller::UVPoller(uv_loop_t* loop) {
  impl_ = std::make_unique<Impl>();
  impl_->uv_loop_ = loop;

  uv_async_init(loop, &impl_->dummy_uv_handle_, nullptr);
  loop->data = this;
  loop->on_watcher_queue_updated = [](uv_loop_t* loop) {
    UVPoller* self = static_cast<UVPoller*>(loop->data);
    uv_async_send(&(self->impl_->dummy_uv_handle_));
  };
}

UVPoller::~UVPoller() {
  uv_close(reinterpret_cast<uv_handle_t*>(&impl_->dummy_uv_handle_), nullptr);
  impl_->uv_loop_->data = nullptr;
}

void UVPoller::PollEvents() {
  struct timeval tv;
  int timeout = uv_backend_timeout(impl_->uv_loop_);
  if (timeout != -1) {
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout % 1000) * 1000;
  }

  fd_set readset;
  int fd = uv_backend_fd(impl_->uv_loop_);
  FD_ZERO(&readset);
  FD_SET(fd, &readset);

  int r;
  do {
  r = select(fd + 1, &readset, nullptr, nullptr,
             timeout == -1 ? nullptr : &tv);
  } while (r == -1 && errno == EINTR);
}

}  // namespace uv_poller
}  // namespace node
