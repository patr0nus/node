#include <node_api_embedding.h>
#include <uv.h>


tick_data_t current_tick_data;
uv_sem_t tick_sem;

int main(int argc, const char** argv)
{
	uv_sem_init(&tick_sem, 0);

    node_init_info init_info = {
        "require('fs').readdir('/e32e2', err => process.exit(0))",
        nullptr,
        argc, argv,
        []() {
        	while (true) {
	        	uv_sem_wait(&tick_sem);
	            node_tick(current_tick_data);
        	}
        },
        [](tick_data_t tick_data) {
        	current_tick_data = tick_data;
        	uv_sem_post(&tick_sem);
        }
    };
    node_main(&init_info);
}
