/**
 * @file threadpool.h
 * @brief thread pool implementation.
 */

#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tp;
struct tp_worker;

typedef void (*tp_callback_t)(const struct tp_worker *worker, void *arg);

struct tp_task {
    tp_callback_t cb;
    void *arg;
    struct tp_task *next;
};

struct tp_worker {
    int id;
    int stoping;
    pthread_t thread;
    struct tp *pool;
};

struct tp {
    int worker_count;
    int queue_size;
    int queue_len;
    int stoping;
    struct tp_worker *workers;
    pthread_mutex_t mu;
    pthread_cond_t cond;
    pthread_cond_t enqueue_cond;

    int enqueue_waiting;
    struct tp_task *task_head;
    struct tp_task **ptask_tail;
};

int tp_init(struct tp *pool, int workers, int queue_size);
void tp_clean(struct tp *pool);
int tp_start(struct tp *pool);
void tp_join(struct tp *pool);
void tp_stop(struct tp *pool);
int tp_post_nowait(struct tp *pool, tp_callback_t cb, void *arg);
int tp_post_wait(struct tp *pool, tp_callback_t cb, void *arg);

#ifdef __cplusplus
}
#endif

#endif  //  THREAD_POOL_H_
