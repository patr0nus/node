#include "uv_poller.h"

#include <windows.h>

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

  // on single-core the io comp port NumberOfConcurrentThreads needs to be 2
  // to avoid cpu pegging likely caused by a busy loop in PollEvents
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  if (info.dwNumberOfProcessors == 1) {
    // the expectation is the loop has just been initialized
    // which makes iocp replacement safe
    CHECK_EQ(0u, loop->active_handles);
    CHECK_EQ(0u, loop->active_reqs.count);

    if (loop->iocp && loop->iocp != INVALID_HANDLE_VALUE)
      CloseHandle(loop->iocp);
    loop->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 2);
  }
}

UVPoller::~UVPoller() {
  uv_close(reinterpret_cast<uv_handle_t*>(&impl_->dummy_uv_handle_), nullptr);
}

void UVPoller::PollEvents() {
  // If there are other kinds of events pending, uv_backend_timeout will
  // instruct us not to wait.
  DWORD bytes, timeout;
  ULONG_PTR key;
  OVERLAPPED* overlapped;

  timeout = uv_backend_timeout(impl_->uv_loop_);

  GetQueuedCompletionStatus(
    impl_->uv_loop_->iocp, &bytes, &key, &overlapped, timeout);

  // Give the event back so libuv can deal with it.
  if (overlapped != nullptr)
    PostQueuedCompletionStatus(impl_->uv_loop_->iocp, bytes, key, overlapped);
}

}  // namespace uv_poller
}  // namespace node
