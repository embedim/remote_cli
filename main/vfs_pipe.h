#pragma once

#define RINGBUFFER_SIZE     128
#define MAXPIPE_SIZE        3

int vfs_pipe_init(void);
int get_free_pipe(void);
