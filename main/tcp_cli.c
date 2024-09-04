#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_console.h"
#include "lwip/sockets.h"
#include "remote_cli.h"

void task_tcp_client(void *pvParameters){
    int socket = *(int*)pvParameters;

    fflush(stdout);
    fsync(fileno(stdout));

    stdout = fdopen(socket, "w");
    stdin = fdopen(socket, "r");

    printf("\r\nConsole run\r\n");
    fflush(stdout);

    const char* prompt = "# ";
    char buffer[128]= {0};

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }
        int len = strlen(buffer);

        if (buffer[len - 1] == '\n'){
            buffer[len - 1] = 0;
        }
        if (buffer[len - 2] == '\r'){
            buffer[len - 2] = 0;
        }

        int ret;
        esp_err_t err = esp_console_run(buffer, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\r\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\r\n", ret, esp_err_to_name(ret));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\r\n", esp_err_to_name(err));
        }
        printf(prompt);
        fflush(stdout);
    }

    fclose(stdout);
    close(socket);
    vTaskDelete(NULL);
    return;
}

