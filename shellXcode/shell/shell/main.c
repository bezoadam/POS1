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
    bufferGlobal.reading = true;

    /* kontrola parametru */
    if(argc != 1){
        printf("%s", HELP);
        return EXIT_FAILURE;
    }

    /* inicializace sig */
    if((err = initSigHandlers(&sigInt, &sigChld)) != OK){
        printError(err);
        return EXIT_FAILURE;
    }    

    /* inicializace pthreads */
    if((err = initMonitor(&monitorGlobal, &thread)) != OK){
        printError(err);
        return EXIT_FAILURE;
    }

    /* cteni vstupu */
    if((err = readMyInput()) != OK){
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

    if(sigaction(SIGINT, sigInt, NULL) == -1){
        return SIG_INT_ERR;
    }

    sigChld->sa_sigaction  = sigChldHandler;
    sigChld->sa_flags    = SA_RESTART | SA_SIGINFO | SA_NOCLDSTOP;
    sigemptyset(&sigChld->sa_mask);

    if(sigaction(SIGCHLD, sigChld, NULL) == -1){
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
    if(DEBUG){
        printf("dokonceno\n");
    }
}

int initMonitor(struct Monitor *monitor, pthread_t *thread) {

    /* init mutexu */
    if(pthread_mutex_init(&(monitor->mutex), NULL) != 0){
        return MUTEX_ERR;
    }

    /* init cond */
    if(pthread_cond_init(&(monitor->cond), NULL) != 0){
        pthread_mutex_destroy(&(monitor->mutex));
        return COND_ERR;
    }

    /* Vytvorenie druheho vlakna na provedeni prikazu */
    while (pthread_create(thread, NULL, runCommand, NULL)){
        pthread_cond_destroy(&(monitor->cond));
        pthread_mutex_destroy(&(monitor->mutex));
        return THREAD_ERR;
    }

    return OK;
}

void *runCommand(void *arg) {
    while(!end){
        /* vstup do KS */
        lockKS(false);

        /* obsluha prikazove radky */
        if(DEBUG) printf("shell: delka vstupu: %d-%d\n'%s'\n", (int)strlen(bufferGlobal.read_buffer), bufferGlobal.length, bufferGlobal.read_buffer);

        /* ukonceni shellu */
        if(strcmp("exit", bufferGlobal.read_buffer) == 0){
            end = true;
        }

        /* zpracovani prikazu */
        else if(bufferGlobal.length > 0){
            printf("%s", bufferGlobal.read_buffer);
        }

        /* vystup z KS */
        bufferGlobal.reading = true;
        unlockKS(true);
    }
    pthread_exit(arg);
}

int readMyInput() {
    int err;
    bool with_signal = true;

    while (!end){
        /* vstup do KS */
        lockKS(true);

        /* muzume zacit cist dokoncene procesy a zapsat prompt */
        if(!end){

            /* PROMPT */
            write(STDOUT_FILENO, (void*)PROMPT, strlen(PROMPT));

            /* cteni ze stdin */
            if((err = readStdin()) != OK){
                bufferGlobal.reading = true;
                printError(err);
            }
            else{
                bufferGlobal.reading = false;
            }

            with_signal = true;
        }
        /* nastal end - neni co odblokovat */
        else{
            bufferGlobal.reading = false;
            with_signal = false;
        }

        /* odblokovani cekajici udalosti + vystup z KS */
        unlockKS(with_signal);
        if(!with_signal) return OK;
    }

    return OK;
}

int readStdin() {
    bufferGlobal.length = 0;
    bufferGlobal.read_buffer[0] = '\0';

    bufferGlobal.length = read(STDIN_FILENO, (void*)bufferGlobal.read_buffer, BUFFER_MAX_LENGTH + 1);

    /* kontrola delky - BUFFER_MAX_LENGTH */
    if(bufferGlobal.length > BUFFER_MAX_LENGTH && bufferGlobal.read_buffer[BUFFER_MAX_LENGTH] != '\n'){
        while('\n' != getchar());
        return BUFFER_MAX_LENGTH_ERR;
    }
    else if(bufferGlobal.length > 0){
        /* nastaveni konce stringu */
        bufferGlobal.read_buffer[bufferGlobal.length-1] = '\0';
        bufferGlobal.length = (int)strlen(bufferGlobal.read_buffer);
    }
    return OK;
}

void lockKS(bool reading) {
    pthread_mutex_lock(&(monitorGlobal.mutex));
    while(reading ? !bufferGlobal.reading : bufferGlobal.reading){
        pthread_cond_wait(&(monitorGlobal.cond), &(monitorGlobal.mutex));
    }
}

void unlockKS(bool with_signal) {
    /* odblokovani cekajici udalosti + vystup z KS */
    if(with_signal) pthread_cond_signal(&(monitorGlobal.cond));
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

