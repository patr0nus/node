#include "node_api_embedding.h"

#include "node.h"
#include "uv.h"
#include <assert.h>

#include <sys/select.h>

using node::ArrayBufferAllocator;
using node::Environment;
using node::IsolateData;
using node::MultiIsolatePlatform;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::MaybeLocal;
using v8::SealHandleScope;
using v8::Value;
using v8::V8;

namespace {
  class UVPoller {
  private:
    uv_loop_t* uv_loop_;
    uv_async_t dummy_uv_handle_;
  public:
    UVPoller(const UVPoller&) = delete;
    void operator=(const UVPoller&) = delete;

    UVPoller(uv_loop_t* loop): uv_loop_(loop) {
      uv_async_init(uv_loop_, &dummy_uv_handle_, nullptr);
    }
    void PlatformInit() {
      uv_loop_->data = this;
      uv_loop_->on_watcher_queue_updated = [](uv_loop_t* loop) {
        UVPoller* self = static_cast<UVPoller*>(loop->data);
        uv_async_send(&(self->dummy_uv_handle_));
      };
    }
    void PlatformDeinit() {
      uv_loop_->data = nullptr;
    }
    void PollEvents() {
      struct timeval tv;
      int timeout = uv_backend_timeout(uv_loop_);
      if (timeout != -1) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
      }

      fd_set readset;
      int fd = uv_backend_fd(uv_loop_);
      FD_ZERO(&readset);
      FD_SET(fd, &readset);

      // Wait for new libuv events.
      int r;
      do {
        r = select(fd + 1, &readset, nullptr, nullptr,
                   timeout == -1 ? nullptr : &tv);
      } while (r == -1 && errno == EINTR);
    }

    ~UVPoller() {
      uv_close(reinterpret_cast<uv_handle_t*>(&dummy_uv_handle_), nullptr);
      PlatformDeinit();
    }
  };
}

struct TickData {
  uv_loop_t* loop;
  MultiIsolatePlatform* platform;
  Isolate* isolate;
  Environment* env;
  bool more;
};

static int RunNodeInstance(MultiIsolatePlatform* platform, const node_init_info* init_info) {
  std::vector<std::string> args(
    init_info->instance_argv,
    init_info->instance_argv + init_info->instance_argc
  );
  std::vector<std::string> exec_args { };
  int exit_code = 0;
  uv_loop_t loop;
  int ret = uv_loop_init(&loop);
  if (ret != 0) {
    fprintf(stderr, "%s: Failed to initialize loop: %s\n",
            args[0].c_str(),
            uv_err_name(ret));
    return 1;
  }

  std::shared_ptr<ArrayBufferAllocator> allocator =
      ArrayBufferAllocator::Create();

  Isolate* isolate = NewIsolate(allocator, &loop, platform);
  if (isolate == nullptr) {
    fprintf(stderr, "%s: Failed to initialize V8 Isolate\n", args[0].c_str());
    return 1;
  }

  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);

    std::unique_ptr<IsolateData, decltype(&node::FreeIsolateData)> isolate_data(
        node::CreateIsolateData(isolate, &loop, platform, allocator.get()),
        node::FreeIsolateData);

    HandleScope handle_scope(isolate);
    Local<Context> context = node::NewContext(isolate);
    if (context.IsEmpty()) {
      fprintf(stderr, "%s: Failed to initialize V8 Context\n", args[0].c_str());
      return 1;
    }

    Context::Scope context_scope(context);
    std::unique_ptr<Environment, decltype(&node::FreeEnvironment)> env(
        node::CreateEnvironment(isolate_data.get(), context, args, exec_args),
        node::FreeEnvironment);

    MaybeLocal<Value> loadenv_ret = node::LoadEnvironment(
        env.get(), init_info->script
    );

    if (loadenv_ret.IsEmpty())  // There has been a JS exception.
      return 1;

    {
      SealHandleScope seal(isolate);
      TickData tick_data {
        &loop,
        platform,
        isolate,
        env.get(),
        true
      };
      if (init_info->loop_func == nullptr) {
        while (tick_data.more) {
          node_tick(&tick_data);
        }
      }
      else {
        uv_thread_t polling_thread;
        struct PollingThreadData {
          std::unique_ptr<UVPoller> poller;
          void(*on_tick_func)(tick_data_t);
          TickData* tick_data;
        } poolThreadData {
          std::make_unique<UVPoller>(&loop),
          init_info->on_tick_func,
          &tick_data
        };

        uv_thread_create(&polling_thread, [](void* data) {
          PollingThreadData* polling_thread_data = static_cast<PollingThreadData*>(data);
          while (polling_thread_data->tick_data->more) {
            polling_thread_data->poller->PollEvents();
            polling_thread_data->on_tick_func(polling_thread_data->tick_data);
          }
        }, &poolThreadData);

        init_info->loop_func();

        uv_thread_join(&polling_thread);
      }
    }

    exit_code = node::EmitExit(env.get());

    node::Stop(env.get());
  }

  bool platform_finished = false;
  platform->AddIsolateFinishedCallback(isolate, [](void* data) {
    *static_cast<bool*>(data) = true;
  }, &platform_finished);
  platform->UnregisterIsolate(isolate);
  isolate->Dispose();

  // Wait until the platform has cleaned up all relevant resources.
  while (!platform_finished)
    uv_run(&loop, UV_RUN_ONCE);
  int err = uv_loop_close(&loop);
  assert(err == 0);

  return exit_code;
}


extern "C" {

void node_tick(tick_data_t data) {
  TickData* tickData = static_cast<TickData*>(data);

  while (uv_run(tickData->loop, UV_RUN_NOWAIT) != 0) ;

  tickData->platform->DrainTasks(tickData->isolate);
  tickData->more = uv_loop_alive(tickData->loop);
  if (tickData->more) return;

  node::EmitBeforeExit(tickData->env);
  tickData->more = uv_loop_alive(tickData->loop);
}

int node_main(const node_init_info* init_info) {
  if (init_info->reg_func != nullptr) {
      static napi_module module =
      {
        NAPI_MODULE_VERSION,
        0,
        __FILE__,
        init_info->reg_func,
        "_embedded_binding",
        nullptr,
        { 0 },
      };
      napi_module_register(&module);
  }

  std::vector<std::string> args { "node" };
  std::vector<std::string> exec_args;
  std::vector<std::string> errors;
  int exit_code = node::InitializeNodeWithArgs(&args, &exec_args, &errors);
  for (const std::string& error : errors)
    fprintf(stderr, "%s: %s\n", args[0].c_str(), error.c_str());
  if (exit_code != 0) {
    return exit_code;
  }

  std::unique_ptr<MultiIsolatePlatform> platform =
      MultiIsolatePlatform::Create(4);
  V8::InitializePlatform(platform.get());
  V8::Initialize();

  int ret = RunNodeInstance(platform.get(), init_info);

  V8::Dispose();
  V8::ShutdownPlatform();
  return ret;
}

}
