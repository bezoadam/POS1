//
//  main.c
//  shell
//
//  Created by Adam Bezák on 7.4.18.
//  Copyright © 2018 Adam Bezák. All rights reserved.
//

#include "shell.h"

/* Globalne premenne */
struct Monitor monitorGlobal;
struct Buffer bufferGlobal;
bool end = false;

int main(int argc, const char * argv[]) {

    int err = 0;
    /* Odchytenie signalov */
    struct sigaction sigInt, sigChld;
    pthread_t thread;
    bufferGlobal.isReading = true;

    /* kontrola parametru */
    if(argc != 1){
        printf("%s", HELP);
        return EXIT_FAILURE;
    }

    /* inicializace sig */
    if ((err = initSigHandlers(&sigInt, &sigChld)) != OK) {
        printError(err);
        return EXIT_FAILURE;
    }    

    /* inicializace pthreads */
    if ((err = initMonitor(&monitorGlobal, &thread)) != OK) {
        printError(err);
        return EXIT_FAILURE;
    }

    /* cteni vstupu */
    if ((err = readMyInput()) != OK) {
        printError(err);
        freeMonitor(&monitorGlobal, &thread);
        return EXIT_FAILURE;
    }

    freeMonitor(&monitorGlobal, &thread);

    /* end */
    if(DEBUG){
        printf("------------------\nvse v poradku\n");
    }
    return 0;
}



int initSigHandlers(struct sigaction *sigInt, struct sigaction *sigChld) {
    sigInt->sa_handler = sigIntHandler;
    sigInt->sa_flags = SA_RESTART;
    sigemptyset(&sigInt->sa_mask);

    if (sigaction(SIGINT, sigInt, NULL) == -1) {
        return SIG_INT_ERR;
    }

    sigChld->sa_sigaction  = sigChldHandler;
    sigChld->sa_flags    = SA_RESTART | SA_SIGINFO | SA_NOCLDSTOP;
    sigemptyset(&sigChld->sa_mask);

    if (sigaction(SIGCHLD, sigChld, NULL) == -1) {
        return SIG_CHLD_ERR;
    }

    return OK;
}


void sigIntHandler(int sig) {

}

/*
 * ==== handler pro sig chld
 * */
void sigChldHandler(int sig, siginfo_t *pid, void *contxt) {

    waitpid(pid->si_pid, NULL, 0);
    if (DEBUG){
        printf("dokonceno\n");
    }
}

int initMonitor(struct Monitor *monitor, pthread_t *thread) {

    /* init mutexu */
    if (pthread_mutex_init(&(monitor->mutex), NULL) != 0) {
        return MUTEX_ERR;
    }

    /* init cond */
    if (pthread_cond_init(&(monitor->cond), NULL) != 0) {
        pthread_mutex_destroy(&(monitor->mutex));
        return COND_ERR;
    }

    /* Vytvorenie druheho vlakna na provedeni prikazu */
    while (pthread_create(thread, NULL, runCommand, NULL)) {
        pthread_cond_destroy(&(monitor->cond));
        pthread_mutex_destroy(&(monitor->mutex));
        return THREAD_ERR;
    }

    return OK;
}

void *runCommand(void *arg) {
    while (!end) {
        /* vstup do KS */
        lockKS(false);

        if(DEBUG) printf("shell: delka vstupu: %d-%d\n'%s'\n", (int)strlen(bufferGlobal.buffer), bufferGlobal.length, bufferGlobal.buffer);

        /* Ukoncenie programu */
        if (strcmp("exit", bufferGlobal.buffer) == 0) {
            end = true;
        } else if(bufferGlobal.length > 0) {
            printf("%s", bufferGlobal.buffer);
        }

        /* vystup z KS */
        bufferGlobal.isReading = true;
        unlockKS(true);
    }
    pthread_exit(arg);
}

int readMyInput() {
    int err;

    while (!end) {
        /* vstup do KS */
        lockKS(true);

        /* muzume zacit cist dokoncene procesy a zapsat prompt */
        if(!end){

            /* PROMPT */
            write(STDOUT_FILENO, (void*)PROMPT, strlen(PROMPT));

            if ((err = readStdin()) != OK) {
                bufferGlobal.isReading = true;
                printError(err);
            } else {
                bufferGlobal.isReading = false;
            }
        }
        /* nastal end - neni co odblokovat */
        else {
            bufferGlobal.isReading = false;
        }

        /* odblokovani cekajici udalosti + vystup z KS */
        unlockKS();
    }

    return OK;
}

int readStdin() {
    bufferGlobal.length = 0;
    bufferGlobal.buffer[0] = '\0';

    /* nacitam vstup */
    bufferGlobal.length = read(STDIN_FILENO, (void*)bufferGlobal.buffer, BUFFER_MAX_LENGTH + 1);

    /* Skontrolujem dlzku */
    if (bufferGlobal.length > BUFFER_MAX_LENGTH && bufferGlobal.buffer[BUFFER_MAX_LENGTH] != '\n') {
        /* docitam vstup do konca */
        while('\n' != getchar());
        return BUFFER_MAX_LENGTH_ERR;
    }
    else if(bufferGlobal.length > 0){
        /* nastaveni konce stringu */
        bufferGlobal.buffer[bufferGlobal.length - 1] = '\0';
        bufferGlobal.length = (int)strlen(bufferGlobal.buffer);
    }
    return OK;
}

void lockKS(bool isReading) {
    pthread_mutex_lock(&(monitorGlobal.mutex));
    bool cond = isReading ? !bufferGlobal.isReading : bufferGlobal.isReading;
    while(cond) {
        pthread_cond_wait(&(monitorGlobal.cond), &(monitorGlobal.mutex));
    }
}

void unlockKS() {
    /* odblokovani cekajici udalosti + vystup z KS */
    pthread_cond_signal(&(monitorGlobal.cond));
    pthread_mutex_unlock(&(monitorGlobal.mutex));
}

void freeMonitor(struct Monitor *monitor, pthread_t *thread) {
    pthread_join(*thread, NULL);
    pthread_cond_destroy(&(monitor->cond));
    pthread_mutex_destroy(&(monitor->mutex));
}

void printError(int err) {
    fprintf(stderr, "%s", errors[err]);
}

