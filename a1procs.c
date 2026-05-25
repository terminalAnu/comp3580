/*
 * COMP 3430 - Operating Systems, Assignment 1, Question 2
 * Process herder - manages worker processes using signals
 */

#include <stdio.h>   /* printf, fprintf, fopen, fscanf */
#include <stdlib.h>  /* exit, EXIT_SUCCESS, EXIT_FAILURE */
#include <unistd.h>  /* fork, getpid, pause, kill */
#include <signal.h>  /* sigaction, SIGINT, SIGHUP */
#include <sys/wait.h> /* waitpid */
#include <string.h>  /* memset */

#define MAX_WORKERS 64
#define CONFIG_FILE "config"

/* pids of all running worker processes */
static pid_t workers[MAX_WORKERS];
static int num_workers = 0;

/* flags that signal handlers set to tell the main loop what to do */
static volatile sig_atomic_t flag_reload = 0;
static volatile sig_atomic_t flag_quit = 0;

/* used by workers to know when to stop looping */
static volatile sig_atomic_t worker_should_quit = 0;

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

/* worker catches SIGINT and sets a flag so it can exit cleanly */
static void worker_signal_handler(int sig)
{
    if (sig == SIGINT) {
        worker_should_quit = 1;
    }
}

/* this runs inside each child process after fork() */
static void run_worker(int worker_id)
{
    struct sigaction sa;

    /* set up the workers own signal handler for SIGINT */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = worker_signal_handler;
    sigaction(SIGINT, &sa, NULL);

    printf("[worker %d] started (pid %d)\n", worker_id, getpid());
    fflush(stdout);

    /* just spin until the supervisor sends SIGINT */
    while (!worker_should_quit) {
        /* doing work */
    }

    printf("[worker %d] got signal, quitting (pid %d)\n", worker_id, getpid());
    fflush(stdout);

    exit(EXIT_SUCCESS);
}

/* fork a new child process and save its pid */
static void start_worker(int worker_id)
{
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Error: fork failed.\n");
        return;
    }

    if (pid == 0) {
        /* child process runs the worker code */
        run_worker(worker_id);
    }

    /* parent saves the childs pid and increments counter */
    workers[num_workers] = pid;
    num_workers++;
    printf("[supervisor] started worker %d (pid %d)\n", worker_id, pid);
    fflush(stdout);
}

/* stop the last worker by sending it SIGINT and waiting for it to exit */
static void stop_last_worker(void)
{
    pid_t pid;

    if (num_workers == 0) {
        return;
    }

    num_workers--;
    pid = workers[num_workers];

    printf("[supervisor] stopping worker (pid %d)\n", pid);
    fflush(stdout);

    kill(pid, SIGINT);
    waitpid(pid, NULL, 0);
}

/* signal all workers then wait for each one to finish */
static void stop_all_workers(void)
{
    int i;

    printf("[supervisor] stopping all %d workers\n", num_workers);
    fflush(stdout);

    /* send SIGINT to all workers first so they stop in parallel */
    for (i = 0; i < num_workers; i++) {
        kill(workers[i], SIGINT);
    }

    /* now wait for each one */
    for (i = 0; i < num_workers; i++) {
        waitpid(workers[i], NULL, 0);
    }

    num_workers = 0;
}

/* start or stop workers until we reach the target number */
static void adjust_workers(int target)
{
    int i;

    if (target < 0 || target > MAX_WORKERS) {
        fprintf(stderr, "Error: invalid worker count %d\n", target);
        return;
    }

    printf("[supervisor] adjusting workers: %d -> %d\n", num_workers, target);
    fflush(stdout);

    while (num_workers < target) {
        start_worker(num_workers);
    }

    for (i = num_workers; i > target; i--) {
        stop_last_worker();
    }
}

/* supervisor signal handlers just set flags, main loop does the real work */
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

    /* tell the OS which functions to call when we get these signals */
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

    /* pause() sleeps until a signal arrives, then we check the flags */
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