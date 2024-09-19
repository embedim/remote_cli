#pragma once

#include "esp_system.h"

typedef struct {
    int s_stack_size;
    int s_task_priority;
    int c_stack_size;
    int c_task_priority;
    int port;
    void (*task)(void *);
} console_remote_cfg;

void task_tcp_client(void *pvParameters);
void task_telnet_client(void *pvParameters);


#define REMOTE_CLI_DEFAULT() \
{                                           \
        .s_stack_size = 4096,               \
        .s_task_priority = 2,               \
        .c_stack_size = 4096,               \
        .c_task_priority = 5,               \
        .task = task_tcp_client,            \
        .port = 7000,                       \
}

#define REMOTE_TELNET_DEFAULT() \
{                                           \
        .s_stack_size = 4096,               \
        .s_task_priority = 2,               \
        .c_stack_size = 4096,               \
        .c_task_priority = 5,               \
        .task = task_telnet_client,         \
        .port = 23,                         \
}

esp_err_t init_remote_cli(console_remote_cfg *cfg);

