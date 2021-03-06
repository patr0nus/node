#include "node_api_embedding.h"

#include "node.h"
#include "uv.h"
#include <assert.h>

#include "uv_poller/uv_poller.h"

using node::ArrayBufferAllocator;
using node::Environment;
using node::IsolateData;
using node::MultiIsolatePlatform;
using node::uv_poller::UVPoller;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::MaybeLocal;
using v8::SealHandleScope;
using v8::V8;
using v8::Value;

struct TickData {
  uv_loop_t* loop;
  MultiIsolatePlatform* platform;
  Isolate* isolate;
  Environment* env;
  bool more;
  uv_sem_t* finish_sem;
};

static void NodeTick(TickData* tickData) {
  uv_run(tickData->loop, UV_RUN_NOWAIT);
  tickData->platform->DrainTasks(tickData->isolate);
  tickData->more = uv_loop_alive(tickData->loop);
  if (tickData->more) return;

  node::EmitBeforeExit(tickData->env);
  tickData->more = uv_loop_alive(tickData->loop);
}

static int RunNodeInstance(
  MultiIsolatePlatform* platform,
  const node_init_info* init_info) {
  std::vector<std::string> args(
    init_info->instance_argv,
    init_info->instance_argv + init_info->instance_argc);

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

    bool has_loop_func = init_info->loop_func != nullptr;
    std::unique_ptr<UVPoller> poller;
    if (has_loop_func) {
      // The poller must be setup before the very first tick.
      poller = std::make_unique<UVPoller>(&loop);
    }

    MaybeLocal<Value> loadenv_ret = node::LoadEnvironment(
        env.get(), init_info->script);

    if (loadenv_ret.IsEmpty())  // There has been a JS exception.
      return 1;

    {
      SealHandleScope seal(isolate);
      TickData tick_data {
        &loop,
        platform,
        isolate,
        env.get(),
        true,  // more
        nullptr  // finish_sem
      };
      if (!has_loop_func) {
        while (tick_data.more) {
          NodeTick(&tick_data);
        }
      } else {
        uv_sem_t tick_finish_sem;
        uv_sem_init(&tick_finish_sem, 0);
        tick_data.finish_sem = &tick_finish_sem;

        uv_thread_t polling_thread;
        struct PollingThreadData {
          std::unique_ptr<UVPoller> poller;
          void(*on_tick_func)(tick_data_t);
          TickData* tick_data;
        } polling_thread_data {
          std::move(poller),
          init_info->on_tick_func,
          &tick_data
        };

        uv_thread_create(&polling_thread, [](void* data) {
          auto polling_thread_data = static_cast<PollingThreadData*>(data);
          while (true) {
            polling_thread_data->on_tick_func(polling_thread_data->tick_data);
            uv_sem_wait(polling_thread_data->tick_data->finish_sem);
            if (!polling_thread_data->tick_data->more) {
              break;
            }
            polling_thread_data->poller->PollEvents();
          }
        }, &polling_thread_data);

        init_info->loop_func();

        uv_sem_post(&tick_finish_sem);
        uv_thread_join(&polling_thread);
        uv_sem_destroy(&tick_finish_sem);
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
  TickData* tick_data = static_cast<TickData*>(data);
  NodeTick(tick_data);
  uv_sem_post(tick_data->finish_sem);
}

int node_main(const node_init_info* init_info) {
  if (init_info->reg_func != nullptr) {
      static napi_module module = {
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
