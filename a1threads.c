/*
 * COMP 3430 - Operating Systems, Assignment 1, Question 2
 * Thread herder - manages worker threads using shared stop flags
 */

#include <stdio.h>    /* printf, fprintf, fopen, fscanf */
#include <stdlib.h>   /* malloc, free, exit */
#include <unistd.h>   /* getpid, pause */
#include <signal.h>   /* sigaction, SIGINT, SIGHUP */
#include <string.h>   /* memset */
#include <pthread.h>  /* pthread_create, pthread_join, pthread_self */

#define MAX_WORKERS 64
#define CONFIG_FILE "config"

/* each worker checks its own stop flag to know when to exit */
static volatile int stop_flags[MAX_WORKERS]; // used volatile to not make assumptions about the variable and to read it

/* thread handles so we can pthread_join each worker */
static pthread_t workers[MAX_WORKERS];
static int num_workers = 0;

/* flags set by signal handlers to tell main loop what happened */
static volatile sig_atomic_t flag_reload = 0;
static volatile sig_atomic_t flag_quit = 0;

/* read the number from the config file */
static int read_config(void)
{
    FILE *fp;
    int count;

    fp = fopen(CONFIG_FILE, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open config file.\n");
        return -1;
    }

    if (fscanf(fp, "%d", &count) != 1) {
        fprintf(stderr, "Error: config file has no valid number.\n");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return count;
}

/* each worker thread runs this function */
static void *worker_func(void *arg)
{
    int worker_id = *(int *)arg;
    free(arg); /* malloc'd in start_worker, free it here */

    printf("[worker %d] started (tid %lu)\n",
           worker_id, (unsigned long)pthread_self());
    fflush(stdout);

    /* spin until supervisor sets our stop flag */
    while (!stop_flags[worker_id]) {
        /* doing work */
    }

    printf("[worker %d] stop flag set, exiting (tid %lu)\n",
           worker_id, (unsigned long)pthread_self());
    fflush(stdout);

    return NULL;
}

/* create a new worker thread and record its handle */
static void start_worker(int worker_id)
{
    int *id_ptr;

    stop_flags[worker_id] = 0;

    /* malloc so the thread gets its own copy of the id
     * passing a stack variable would be unsafe since it could change */
    id_ptr = malloc(sizeof(int));
    if (id_ptr == NULL) {
        fprintf(stderr, "Error: malloc failed.\n");
        return;
    }
    *id_ptr = worker_id;

    if (pthread_create(&workers[worker_id], NULL, worker_func, id_ptr) != 0) {
        fprintf(stderr, "Error: pthread_create failed.\n");
        free(id_ptr);
        return;
    }

    num_workers++;
    printf("[supervisor] started worker %d\n", worker_id);
    fflush(stdout);
}

/* stop the last worker by setting its flag and joining it */
static void stop_last_worker(void)
{
    int worker_id;

    if (num_workers == 0) {
        return;
    }

    num_workers--;
    worker_id = num_workers;

    printf("[supervisor] stopping worker %d\n", worker_id);
    fflush(stdout);

    stop_flags[worker_id] = 1;
    pthread_join(workers[worker_id], NULL);
}

/* set all stop flags first so they all stop in parallel, then join each one */
static void stop_all_workers(void)
{
    int i;

    printf("[supervisor] stopping all %d workers\n", num_workers);
    fflush(stdout);

    for (i = 0; i < num_workers; i++) {
        stop_flags[i] = 1;
    }

    for (i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL);
    }

    num_workers = 0;
}

/* start or stop workers until we reach the target number */
static void adjust_workers(int target)
{
    if (target < 0 || target > MAX_WORKERS) {
        fprintf(stderr, "Error: invalid worker count %d\n", target);
        return;
    }

    printf("[supervisor] adjusting workers: %d -> %d\n", num_workers, target);
    fflush(stdout);

    while (num_workers < target) {
        start_worker(num_workers);
    }

    while (num_workers > target) {
        stop_last_worker();
    }
}

/* signal handlers just set flags, main loop does the actual work */
static void supervisor_sighup_handler(int sig)
{
    (void)sig;
    flag_reload = 1;
}

static void supervisor_sigint_handler(int sig)
{
    (void)sig;
    flag_quit = 1;
}

int main(void)
{
    struct sigaction sa;
    int target;

    /* register signal handlers with the OS */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = supervisor_sighup_handler;
    sigaction(SIGHUP, &sa, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = supervisor_sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    printf("[supervisor] started (pid %d)\n", getpid());
    fflush(stdout);

    target = read_config();
    if (target < 0) {
        return EXIT_FAILURE;
    }
    adjust_workers(target);

    /* pause() sleeps until a signal comes in, then we check the flags */
    while (!flag_quit) {
        pause();

        if (flag_reload) {
            flag_reload = 0;
            printf("[supervisor] SIGHUP received, reloading config\n");
            fflush(stdout);
            target = read_config();
            if (target >= 0) {
                adjust_workers(target);
            }
        }
    }

    printf("[supervisor] SIGINT received, shutting down\n");
    fflush(stdout);
    stop_all_workers();

    printf("[supervisor] all workers done, exiting\n");
    fflush(stdout);

    return EXIT_SUCCESS;
}