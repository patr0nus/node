#include "node_api_embedding.h"

#include "node.h"
#include "uv.h"
#include <assert.h>

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
      bool more;
      do {
        uv_run(&loop, UV_RUN_DEFAULT);

        platform->DrainTasks(isolate);
        more = uv_loop_alive(&loop);
        if (more) continue;

        node::EmitBeforeExit(env.get());
        more = uv_loop_alive(&loop);
      } while (more == true);
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
