#ifndef SRC_NODE_API_EMBEDDING_H_
#define SRC_NODE_API_EMBEDDING_H_

#include "node_api.h"

EXTERN_C_START

typedef struct {
  const char* script;
  napi_addon_register_func reg_func;
  int instance_argc;
  const char** instance_argv;
} node_init_info;

int node_main(const node_init_info* init_info);

EXTERN_C_END

#endif  // SRC_NODE_API_EMBEDDING_H_
