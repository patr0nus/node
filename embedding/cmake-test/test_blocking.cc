#include <node_api_embedding.h>

int main(int argc, const char** argv)
{
    node_init_info init_info = {
        "console.log('first tick'); setImmediate(() => console.log('setImmediate'))",
        nullptr,
        argc, argv,
        nullptr, nullptr
    };
    node_main(&init_info);
}
