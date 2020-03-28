#include <node_api_embedding.h>
#include <cstdlib>

int main(int argc, const char** argv)
{
    node_init_info init_info = {
        "setInterval(() => {}, 1000);"
        "process._linkedBinding('_embedded_binding').exit()",
        [](napi_env env, napi_value exports) -> napi_value {
            napi_value exitFunction;
            napi_create_function(
                env, "exit", NAPI_AUTO_LENGTH,
                [](napi_env env, napi_callback_info info) -> napi_value {
                    std::exit(0);
                    return nullptr;
                }, nullptr,
                &exitFunction
            );
            napi_set_named_property(env, exports, "exit", exitFunction);
            return exports;
        },
        argc, argv,
        nullptr, nullptr
    };
    node_main(&init_info);
}
