#include "threadpool.h"

#include <malloc.h>
#include <pthread.h>
#ifdef THREAD_DEBUG
#include <stdio.h>
#endif

int tp_init(struct tp *pool, int workers, int queue_size) {
    int i;
    pool->workers =
        (struct tp_worker *)malloc(sizeof(struct tp_worker) * workers);
    if (pool->workers == NULL) {
#ifdef THREAD_DEBUG
        fprintf(stderr, "tp_init:malloc tp_worker{} failed\n");
#endif
        return -1;
    }

    for (i = 0; i < workers; ++i) {
        pool->workers[i].id = i;
        pool->workers[i].stoping = 0;
        pool->workers[i].pool = pool;
    }

    pool->worker_count = workers;
    pool->queue_size = queue_size;
    pool->queue_len = 0;
    pool->stoping = 0;
    pool->enqueue_waiting = 0;
    pthread_mutex_init(&pool->mu, NULL);
    pthread_cond_init(&pool->cond, NULL);
    pthread_cond_init(&pool->enqueue_cond, NULL);
    pool->task_head = NULL;
    pool->ptask_tail = &pool->task_head;

    return 0;
}

void tp_clean(struct tp *pool) {
    free(pool->workers);
    pthread_cond_destroy(&pool->enqueue_cond);
    pthread_cond_destroy(&pool->cond);
    pthread_mutex_destroy(&pool->mu);
}

static void *tp_worker_loop(void *arg) {
    struct tp_worker *worker = (struct tp_worker *)arg;
    struct tp *pool = worker->pool;
    struct tp_task *task = NULL;

#ifdef THREAD_DEBUG
    fprintf(stdout, "thread %d start\n", worker->id);
#endif

    for (;;) {
        // wait tasks
        pthread_mutex_lock(&pool->mu);
        while (pool->queue_len == 0 && pool->stoping == 0) {
            pthread_cond_wait(&pool->cond, &pool->mu);
        }
        // exit if stoping and queue empty
        if (pool->queue_len == 0 && pool->stoping) {
            worker->stoping = 1;
            pthread_mutex_unlock(&pool->mu);
#ifdef THREAD_DEBUG
            fprintf(stdout, "thread %d stop\n", worker->id);
#endif
            return NULL;
        }

        // get task
        task = pool->task_head;
        pool->task_head = pool->task_head->next;
        if (pool->task_head == NULL) {
            pool->ptask_tail = &pool->task_head;
        }
        pool->queue_len--;

        if (pool->enqueue_waiting) {
            pthread_cond_signal(&pool->enqueue_cond);
        }

        pthread_mutex_unlock(&pool->mu);

        // apply task->cb() till queue empty
        while (task) {
            task->cb(worker, task->arg);
            free(task);

            // get next task
            pthread_mutex_lock(&pool->mu);
            task = pool->task_head;
            if (task) {
                pool->task_head = pool->task_head->next;
                if (pool->task_head == NULL) {
                    pool->ptask_tail = &pool->task_head;
                }
                pool->queue_len--;
                if (pool->enqueue_waiting) {
                    pthread_cond_signal(&pool->enqueue_cond);
                }
                if (pool->stoping) {
                    worker->stoping = 1;
                }
            }
            pthread_mutex_unlock(&pool->mu);
        }
    }

    return NULL;
}

int tp_start(struct tp *pool) {
    int i, j, r;
    for (i = 0; i < pool->worker_count; ++i) {
        struct tp_worker *worker = &pool->workers[i];
        r = pthread_create(&worker->thread, NULL, tp_worker_loop, worker);
        if (r != 0) {
#ifdef THREAD_DEBUG
            fprintf(stderr, "tp_start: error creating thread\n");
#endif
            pthread_mutex_lock(&pool->mu);
            pool->stoping = 1;
            pthread_cond_broadcast(&pool->cond);
            pthread_mutex_unlock(&pool->mu);
            for (j = 0; j < i; ++j) {
                pthread_join(worker->thread, NULL);
            }
            return -1;
        }
    }
    return 0;
}

void tp_join(struct tp *pool) {
    int i;
    for (i = 0; i < pool->worker_count; ++i) {
        pthread_join(pool->workers[i].thread, NULL);
    }
}

void tp_stop(struct tp *pool) {
    pthread_mutex_lock(&pool->mu);
    pool->stoping = 1;
    pthread_cond_broadcast(&pool->cond);
    pthread_cond_broadcast(&pool->enqueue_cond);
    pthread_mutex_unlock(&pool->mu);
}

int tp_post_nowait(struct tp *pool, tp_callback_t cb, void *arg) {
    struct tp_task *task = (struct tp_task *)malloc(sizeof(struct tp_task));
    task->arg = arg;
    task->cb = cb;
    task->next = NULL;

    pthread_mutex_lock(&pool->mu);
    if (pool->stoping) {
        pthread_mutex_unlock(&pool->mu);
#ifdef THREAD_DEBUG
        fprintf(stderr, "drop because pool stop\n");
#endif
        free(task);
        return -1;
    }

    if (pool->queue_len > pool->queue_size) {
        pthread_mutex_unlock(&pool->mu);
#ifdef THREAD_DEBUG
        fprintf(stderr, "drop because pool full\n");
#endif
        free(task);

        return -1;
    }

    *pool->ptask_tail = task;
    pool->ptask_tail = &task->next;
    pool->queue_len++;
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mu);

    return 0;
}

int tp_post_wait(struct tp *pool, tp_callback_t cb, void *arg) {
    struct tp_task *task = (struct tp_task *)malloc(sizeof(struct tp_task));
    task->arg = arg;
    task->cb = cb;
    task->next = NULL;

    pthread_mutex_lock(&pool->mu);
    if (pool->stoping) {
        pthread_mutex_unlock(&pool->mu);
#ifdef THREAD_DEBUG
        fprintf(stderr, "drop because pool stop\n");
#endif
        free(task);
        return -1;
    }

    while (pool->queue_len >= pool->queue_size || pool->stoping) {
        pool->enqueue_waiting++;
        pthread_cond_wait(&pool->enqueue_cond, &pool->mu);
        pool->enqueue_waiting--;
    }

    if (pool->stoping) {
        pthread_mutex_unlock(&pool->mu);
#ifdef THREAD_DEBUG
        fprintf(stderr, "drop because pool stop\n");
#endif
        free(task);
        return -1;
    }

    *pool->ptask_tail = task;
    pool->ptask_tail = &task->next;
    pool->queue_len++;
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mu);

    return 0;
}
