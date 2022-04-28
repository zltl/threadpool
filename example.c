#include <stdio.h>
#include <stdlib.h>

#include "threadpool.h"

struct tp_arg {
    int cnt;
};

pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
int total_task = 0;

static void tp_cb(const struct tp_worker *worker, void *arg) {
    (void)worker;
    (void)arg;
    struct tp_arg *a = (struct tp_arg *)arg;
    printf("task: %d\n", a->cnt);
    pthread_mutex_lock(&mu);
    total_task++;
    pthread_mutex_unlock(&mu);
}

int main() {
#define task_num 1000
    struct tp_arg tp_args[task_num];
    struct tp tp;

    int r;

    r = tp_init(&tp, 10, 30);
    if (r != 0) {
        fprintf(stderr, "error tp_init()\n");
        return -1;
    }
    r = tp_start(&tp);
    if (r != 0) {
        fprintf(stderr, "error tp_start()\n");
        return -1;
    }

    int i;
    for (i = 0; i < task_num; i++) {
        tp_args[i].cnt = i;
        tp_post_wait(&tp, tp_cb, &tp_args[i]);
    }

    tp_stop(&tp);
    tp_join(&tp);
    tp_clean(&tp);

    if (total_task != task_num) {
        fprintf(stderr, "error task not finished\n");
        return -1;
    }

    return 0;
}
