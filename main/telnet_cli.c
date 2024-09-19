#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_console.h"
#include "lwip/sockets.h"
#include "remote_cli.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "libtelnet.h"
#include "linenoise/linenoise.h"
#include "vfs_pipe.h"
#include "esp_check.h"

static const char *TAG = "telnet";

#define TELNET_BUFFER_SIZE         128

typedef struct {
    int socket;
    FILE* write_stream;
    FILE* read_stream;
    FILE* back_stream;
    telnet_t *telnet;
    TaskHandle_t parent;
} telnet_cli_t ;

static const telnet_telopt_t telopts[] = {
    { TELNET_TELOPT_ECHO,           TELNET_WILL, TELNET_DO },
    { TELNET_TELOPT_LINEMODE,       TELNET_WONT, TELNET_DO },
    { TELNET_TELOPT_SGA,            TELNET_WILL, TELNET_DO },
    { -1, 0, 0 }
};

void telnet_event_handler(telnet_t *telnet, telnet_event_t *event, void *user_data) {
    telnet_cli_t ctx = *(telnet_cli_t*)user_data;
    switch (event->type) {
        case TELNET_EV_DATA:
            fwrite(event->data.buffer, 1, event->data.size, ctx.write_stream);
            fflush(ctx.write_stream);
            break;
        case TELNET_EV_SEND:
            send(ctx.socket, event->data.buffer, event->data.size, 0);
            break;
        case TELNET_EV_ERROR:
            fprintf( stderr, "Telnet error: %s", event->error.msg);
            break;
        default:
            fprintf( stderr, "Any type: %d\n", event->type);
            break;
    }
}

static ssize_t send_over_telnet(struct _reent* ptr, void* cookie, const char* data, size_t size)
{
    telnet_t *tlnt = cookie;
    return telnet_printf(tlnt, "%.*s", size, data);
}


void task_cli(void *pvParameters){
    telnet_cli_t ctx = *(telnet_cli_t*)pvParameters;

    FILE* old_stdin  = stdin;
    FILE* old_stdout = stdout;
    void* old_cookie = ctx.back_stream->_cookie;
    int * old_write  = ctx.back_stream->_write;

    stdin  = ctx.read_stream;
    stdout = ctx.back_stream;
    stdout->_write = &send_over_telnet;
    stdout->_cookie = ctx.telnet;

    printf("\r\nConsole run\r\n");

    while(true) {
        if (ulTaskNotifyTake(pdTRUE, 0)) {
            break;
        }
        char *line = linenoise("prompt> ");
        if (line == NULL) {
            break;
        }
        /* Add the command to the history */
        linenoiseHistoryAdd(line);

        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\r\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\r\n", ret, esp_err_to_name(ret));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\r\n", esp_err_to_name(err));
        }
        linenoiseFree(line);
    }

    stdout->_write  = old_write;
    stdout->_cookie = old_cookie;
    stdin  = old_stdin;
    stdout = old_stdout;

    xTaskNotifyGive(ctx.parent);
    vTaskDelete(NULL);
}


void task_telnet_client(void *pvParameters){
    char telnet_buffer[TELNET_BUFFER_SIZE];
    char path[ESP_VFS_PATH_MAX] = {0};
    char task_name[CONFIG_FREERTOS_MAX_TASK_NAME_LEN];
    TaskHandle_t cli_handle;
    ssize_t received; 
    int ret = -1;

    int socket = *(int*)pvParameters;
    TaskHandle_t parent = xTaskGetCurrentTaskHandle();

    snprintf(path, ESP_VFS_PATH_MAX, "/dev/pipe/%d", get_free_pipe());

    FILE* write_stream = fopen(path, "w");
    ESP_GOTO_ON_FALSE(write_stream, -1, close_write, TAG, "Open write steam");
    ESP_LOGI(TAG, "write_stream: [%d]", fileno(write_stream));

    FILE* read_stream  = fopen(path, "r");
    ESP_GOTO_ON_FALSE(read_stream, -1, close_read, TAG, "Open read steam");
    ESP_LOGI(TAG, "read_stream : [%d]", fileno(read_stream));

    FILE* back_stream  = fdopen(socket, "w");
    ESP_GOTO_ON_FALSE(back_stream, -1, close_back, TAG, "Open back steam");

    telnet_cli_t ctx = {
        .socket = socket,
        .read_stream  = read_stream,
        .write_stream = write_stream,
        .back_stream  = back_stream,
        .parent = parent,
    };

    telnet_t* telnet = telnet_init(telopts, telnet_event_handler , 0, &ctx);

    ctx.telnet = telnet;

    snprintf(task_name, CONFIG_FREERTOS_MAX_TASK_NAME_LEN, "cli_%d", socket);

    BaseType_t status = xTaskCreate(task_cli, task_name,
                                        4000,
                                        &ctx,
                                        5,
                                        &cli_handle);

    while (1) {
        received = recv(socket, telnet_buffer, TELNET_BUFFER_SIZE, 0);
        if (received < 0) {
            break;
        }
        telnet_recv(telnet, telnet_buffer, received);
    }

    ESP_LOGD(TAG, "Try close [%s]", path);

    // Notify and unblock
    xTaskNotifyGive(cli_handle);
    write(fileno(ctx.write_stream), "q\r\n", 3);

    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));

    ESP_LOGD(TAG,  "telnet_free");
    telnet_free(telnet);

close_back:
    ESP_LOGD(TAG, "Close back_stream");
    fclose(back_stream);

close_read:
    ESP_LOGD(TAG, "Close read_stream");
    fclose(read_stream);

close_write:
    ESP_LOGD(TAG, "Close write_stream");
    fclose(write_stream);

    ESP_LOGI(TAG, "Close socket [%d]", socket);
    close(socket);
    vTaskDelete(NULL);
}
