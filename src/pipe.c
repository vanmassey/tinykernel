#include "pipe.h"

#include "string.h"

#define PIPE_MAX  4
#define PIPE_SIZE 512

struct pipe {
    int used;
    char buf[PIPE_SIZE];
    unsigned head;
    unsigned tail;
    unsigned count;
};

static struct pipe pipes[PIPE_MAX];

void pipe_init(void)
{
    memset(pipes, 0, sizeof(pipes));
    pipes[PIPE_SYS].used = 1;
}

int pipe_open(void)
{
    for (int i = 1; i < PIPE_MAX; i++) {
        if (!pipes[i].used) {
            pipes[i].used = 1;
            pipes[i].head = pipes[i].tail = pipes[i].count = 0;
            return i;
        }
    }
    return -1;
}

static struct pipe *get(int id)
{
    if (id < 0 || id >= PIPE_MAX || !pipes[id].used) {
        return 0;
    }
    return &pipes[id];
}

int pipe_write(int id, const char *buf, size_t n)
{
    struct pipe *p = get(id);
    if (!p || !buf) {
        return -1;
    }
    size_t wrote = 0;
    while (wrote < n && p->count < PIPE_SIZE) {
        p->buf[p->head] = buf[wrote++];
        p->head = (p->head + 1) % PIPE_SIZE;
        p->count++;
    }
    return (int)wrote;
}

int pipe_read(int id, char *buf, size_t n)
{
    struct pipe *p = get(id);
    if (!p || !buf) {
        return -1;
    }
    size_t got = 0;
    while (got < n && p->count > 0) {
        buf[got++] = p->buf[p->tail];
        p->tail = (p->tail + 1) % PIPE_SIZE;
        p->count--;
    }
    return (int)got;
}

unsigned pipe_used(int id)
{
    struct pipe *p = get(id);
    return p ? p->count : 0;
}
