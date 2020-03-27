#include "uv_poller.h"

#include <sys/epoll.h>

namespace node {
namespace uv_poller {

struct UVPoller::Impl {
    uv_loop_t* uv_loop_;
    uv_async_t dummy_uv_handle_;
    int epoll_;
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

  impl_->epoll_ = epoll_create(1);

  int backend_fd = uv_backend_fd(loop);
  struct epoll_event ev = { 0, { } };
  ev.events = EPOLLIN;
  ev.data.fd = backend_fd;
  epoll_ctl(impl_->epoll_, EPOLL_CTL_ADD, backend_fd, &ev);
}

UVPoller::~UVPoller() {
  uv_close(reinterpret_cast<uv_handle_t*>(&impl_->dummy_uv_handle_), nullptr);
  impl_->uv_loop_->data = nullptr;
}

void UVPoller::PollEvents() {
  int timeout = uv_backend_timeout(impl_->uv_loop_);

  int r;
  do {
    struct epoll_event ev;
    r = epoll_wait(impl_->epoll_, &ev, 1, timeout);
  } while (r == -1 && errno == EINTR);
}

}  // namespace uv_poller
}  // namespace node
