#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_console.h"
#include "lwip/sockets.h"
#include "remote_cli.h"

static const char *TAG = "remote_cli";
static int client_socket = 0;

void task_tcp_listen(void *pvParameters){
    struct sockaddr_in addr;
    int sock, err, opt = 1;
    char task_name[16];

    console_remote_cfg *cfg = (console_remote_cfg*)pvParameters;
    ESP_LOGI(TAG, "Server run use port: %d", cfg->port);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        ESP_LOGE(TAG, "Cannot create socket");
        goto clean;
    }
    else {
        ESP_LOGI(TAG, "Create socket [%4d]", sock);
    }
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family         = AF_INET;
    addr.sin_addr.s_addr    = htonl(INADDR_ANY);
    addr.sin_port           = htons(cfg->port);

    err = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto clean;
    }

    err = listen(sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto clean;
    }

    while(1){
        ESP_LOGI(TAG, "Socket listening");
        struct sockaddr_in  source_addr;
        socklen_t addr_len = sizeof(source_addr);
        client_socket = accept(sock, (struct sockaddr *)&source_addr, &addr_len);
        if (client_socket < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }
        else {
            ESP_LOGI(TAG, "New sock [%d]", client_socket);
            snprintf(task_name, 16, "client_%d",  client_socket);
            xTaskCreate(cfg->task, task_name, cfg->c_stack_size, &client_socket, cfg->c_task_priority, NULL);
        }
    }

clean:
    ESP_LOGE(TAG, "Socket close [%4d]", sock);
    close(sock);
    vTaskDelete(NULL);
}

esp_err_t init_remote_cli(console_remote_cfg *cfg){
  if (cfg == NULL) return ESP_ERR_INVALID_ARG;

  console_remote_cfg *tcpcfg = calloc(1, sizeof(console_remote_cfg));
  memcpy(tcpcfg, cfg, sizeof(console_remote_cfg));

  BaseType_t status = xTaskCreate(task_tcp_listen, "tcp_listen",
                                    tcpcfg->s_stack_size,
                                    tcpcfg,
                                    tcpcfg->s_task_priority,
                                    0);
  if (status == pdPASS) {
    ESP_LOGV(TAG, "Telnet task created successfully.");
    return ESP_OK;
  }

  free(tcpcfg);
  ESP_LOGE(TAG, "Failed to create telnet task.");
  return ESP_FAIL;
}