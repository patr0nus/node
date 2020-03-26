#ifndef SRC_NODE_API_EMBEDDING_H_
#define SRC_NODE_API_EMBEDDING_H_

#include "node_api.h"

EXTERN_C_START

typedef void* tick_data_t;

typedef struct {
  const char* script;
  napi_addon_register_func reg_func;
  int instance_argc;
  const char** instance_argv;
  void(*loop_func)();
  void(*on_tick_func)(tick_data_t tick_data);
} node_init_info;

int node_main(const node_init_info* init_info);
void node_tick(tick_data_t tick_data);


EXTERN_C_END

#endif  // SRC_NODE_API_EMBEDDING_H_
