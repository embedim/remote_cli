#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include <errno.h>
#include "esp_console.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_system.h"
#include "remote_cli.h"
#include "vfs_pipe.h"

static const char *TAG = "vfs_pipe";

typedef struct vfs_pipe
{
    bool used;
    RingbufHandle_t rbuf;
} pipe_ctx_t;

static pipe_ctx_t pipe_ctx[MAXPIPE_SIZE] = {0};

static int term_open(const char * path, int flags, int mode)
{
    int fd = -1;
    if (strcmp(path, "/0") == 0) {
        fd = 0;
    } else if (strcmp(path, "/1") == 0) {
        fd = 1;
    } else if (strcmp(path, "/2") == 0) {
        fd = 2;
    } else {
        errno = ENOENT;
        return fd;
    }

    if (pipe_ctx[fd].rbuf == NULL) {
        pipe_ctx[fd].used = true;
        pipe_ctx[fd].rbuf = xRingbufferCreate(RINGBUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    }
    return fd;
}

static int term_fstat(int fd, struct stat *st)
{
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFIFO ;
    return 0;
}

static ssize_t term_write(int fd, const void *data, size_t size)
{
    BaseType_t result = xRingbufferSend(pipe_ctx[fd].rbuf, (void *)data, size, portMAX_DELAY);
    if (result != pdTRUE) {
        ESP_LOGE(TAG, "term_write error");
        return 0;
    }
    return size;
}

static ssize_t term_read(int fd, void *data, size_t size){
    size_t item_size = 0;
    char *item = (char *)xRingbufferReceiveUpTo(pipe_ctx[fd].rbuf, &item_size, pdMS_TO_TICKS(portMAX_DELAY), size);
    if (item != NULL) {
        memcpy(data, item, item_size);
        vRingbufferReturnItem(pipe_ctx[fd].rbuf, (void *)item);
    }
    return item_size;
}

static int term_close(int fd)
{
    if (pipe_ctx[fd].used) {
        vRingbufferDelete(pipe_ctx[fd].rbuf);
        pipe_ctx[fd].rbuf = NULL;
        pipe_ctx[fd].used = false;
    }
    return 0;
}

int vfs_pipe_init(void)
{
    static const esp_vfs_t vfs = {
        .flags   = ESP_VFS_FLAG_DEFAULT,
        .open    = term_open,
        .fstat   = term_fstat,
        .write   = term_write,
        .read    = term_read,
        .close   = term_close,
    };

    const char *base_path = "/dev/pipe";
    esp_err_t err;
    err = esp_vfs_register(base_path, &vfs, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_register : '%s'", strerror(errno));
        return err;
    }

    return 0;
}

int get_free_pipe()
{
    for(int i = 0; i < MAXPIPE_SIZE; i++){
        if (pipe_ctx[i].used == false) {
            return i;
        }
    }
    return -1;
}