//
//  main.c
//  shell
//
//  Created by Adam Bezák on 7.4.18.
//  Copyright © 2018 Adam Bezák. All rights reserved.
//
//TODO Pomocí Ctrl-C se ukončí aktuálně běžící proces na popředí (pokud nějaký je).
//TODO Kontrolu na ukončení synů (běžících na pozadí) provádějte pomocí signálu SIGCHLD. Informaci o ukončení synů na pozadí vypište!

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
#ifdef CUSTOMDEBUG
        printf("------------------\nvse v poradku\n");
#endif
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

void sigChldHandler(int sig, siginfo_t *pid, void *contxt) {

    waitpid(pid->si_pid, NULL, 0);
    printf("Pid: %d finished.\n", pid->si_pid);
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

static int setargs(char *args, char **argv) {
    int count = 0;

    while (isspace(*args)) ++args;
    while (*args) {
        if (argv) {
            argv[count] = args;
        }
        while (*args && !isspace(*args)) ++args;
        if (argv && *args) *args++ = '\0';
        while (isspace(*args)) ++args;
        count++;
    }
    return count;
}

void removeSubstring(char *s,const char *toremove) {
    while ((s=strstr(s,toremove))) memmove(s,s+strlen(toremove),1+strlen(s+strlen(toremove)));
}

char **parsedargs(char *args, int *argc, bool *isBackground) {
    char **argv = NULL;
    int    argn = 0;

    /* je potrebne zistit a odstranit vyskyt ampersandu */
    if ((strstr(args, " & ") != NULL) || strstr(args, " &") != NULL) {
        *isBackground = true;
        removeSubstring(args, " &");
    } else {
        *isBackground = false;
    }

    if (args && *args
        && (args = strdup(args))
        && (argn = setargs(args,NULL))
        && (argv = malloc((argn+1) * sizeof(char *)))) {
        *argv++ = args;
        argn = setargs(args,argv);
    }

    if (args && !argv) free(args);

    argv[argn] = (char*) 0;

    *argc = argn;
    return argv;
}

struct Command getCommand() {
    struct Command command;
    int i = 0, commandLineArgc = 0;
    char **commandLineArgv = parsedargs(bufferGlobal.buffer, &commandLineArgc, &command.isBackground);

    /* buffer -> argv */
    command.argv = NULL;
    command.argc = 0;
    command.inputFile = NULL;
    command.outputFile = NULL;
    command.error = OK;

    /* alokovani pole */
    command.argv = malloc((commandLineArgc * sizeof(char*)) + 2*sizeof(char*));

    for(i = 0; i < commandLineArgc; i++) {
        char *currentArgv = commandLineArgv[i];
        if (currentArgv == NULL) continue;
        unsigned long currentArgvLength = strlen(currentArgv);

        switch (currentArgv[0]) {
            case '<':
                if (currentArgvLength <= 1) {
                    command.error = INPUT_FILE_ERR;
                }
                command.inputFile = malloc((currentArgvLength - 1) * sizeof(char));
                strcpy(command.inputFile, currentArgv + 1);
                command.inputFile[currentArgvLength - 1] = '\0';
                break;
            case '>':
                if (currentArgvLength <= 1) {
                    command.error = OUTPUT_FILE_ERR;
                }
                command.outputFile = malloc((currentArgvLength - 1) * sizeof(char));
                strcpy(command.outputFile, currentArgv + 1);
                command.outputFile[currentArgvLength - 1] = '\0';
                break;
            case '&':
                if (currentArgvLength != 1) {
                    command.error = AMPERSAND_ERR;
                }
                break;
            default:
                command.argv[command.argc] = NULL;
                command.argv[command.argc] = malloc((currentArgvLength * sizeof(char)) + sizeof(char));
                strcpy(command.argv[command.argc], currentArgv);
                command.argv[command.argc][currentArgvLength] = '\0';
                command.argc++;
                break;
        }
    }

    command.argv[command.argc] = (char*) 0;

#ifdef CUSTOMDEBUG
    int count;
    printf("--------------\n");
    printf("Pocet slov = %d\n", command.argc);
    for(count = 0; count < command.argc; count++){
        printf("argv[%d] %d = '%s'\n", count, (int)strlen(command.argv[count]), command.argv[count]);
    }
    printf("input = %s\noutput = %s\nna pozadi = %d\n", command.inputFile, command.outputFile, (int)command.isBackground);
    printf("--------------\n");
#endif

    return command;
}

int redirectOutput(char *outputFile) {
    int out = open(outputFile, O_RDWR|O_CREAT|O_APPEND, 0600);
    if (out == -1) {
        return OUTPUT_FILE_ERR;
    }

    if (dup2(out, fileno(stdout)) == -1) {
        return OUTPUT_FILE_ERR;
    }

    return OK;
}

int redirectInput(char *inputFile) {
    int in = open(inputFile, O_RDONLY);
    if (in == -1) {
        return INPUT_FILE_ERR;
    }

    if (dup2(in, fileno(stdin)) == -1) {
        return INPUT_FILE_ERR;
    }

    return OK;
}

int startBackgroundCommand(struct Command *command) {
    int err = 0;
    pid_t pid  = fork();

    /* child */
    if(pid == 0) {
        /* presmerovanie vystupu */
        if (command->outputFile != NULL) {
            if ((err = redirectOutput(command->outputFile) != OK)) {
                printError(err);
            }
        }
        /* presmerovanie vstupu */
        if (command->inputFile != NULL) {
            if ((err = redirectInput(command->inputFile) != OK)) {
                printError(err);
            }
        }

        /* vykonani prikazu */
        execvp(command->argv[0], command->argv);

        /* chyba prikazu */
        perror(errors[COMMAND_BG_ERR]);
        exit(EXIT_FAILURE);
    } /* parent */
    else if(pid > 0) {
        printf("\n na pozadi spusteno \n");
    } else{
        return FORK_ERR;
    }

    return OK;

}

int startNormalCommand(struct Command *command) {
    int err = 0;
    pid_t pid  = fork();

    /* child */
    if(pid == 0) {
        /* presmerovanie vystupu */
        if (command->outputFile != NULL) {
            if ((err = redirectOutput(command->outputFile) != OK)) {
                printError(err);
            }
        }
        /* presmerovanie vstupu */
        if (command->inputFile != NULL) {
            if ((err = redirectInput(command->inputFile) != OK)) {
                printError(err);
            }
        }

        /* vykonani prikazu */
        execvp(command->argv[0], command->argv);

        /* chyba vykonania prikazu */
        perror(errors[COMMAND_ERR]);
        exit(EXIT_FAILURE);
    } /* parent */
    else if(pid > 0){
        /* cekani na proces ... uchovani id, pro pripadne kill-nuti*/
        waitpid(pid, NULL, 0);
        return OK;
    } else {
        return FORK_ERR;
    }

    return OK;
}

void *runCommand(void *arg) {
    struct Command command;
    int err;

    while (!end) {
        /* vstup do KS */
        lockKS(false);

#ifdef CUSTOMDEBUG
        printf("shell: delka vstupu: %d-%d\n'%s'\n", (int)strlen(bufferGlobal.buffer), bufferGlobal.length, bufferGlobal.buffer);
#endif
        /* Ukoncenie programu */
        if (strcmp("exit", bufferGlobal.buffer) == 0) {
            end = true;
        } else if(bufferGlobal.length > 0) {
            command = getCommand();
            if (command.error != OK) {
                printError(command.error);
            } else {
                if ((err = (command.isBackground ? startBackgroundCommand(&command) : startNormalCommand(&command)) != OK)) {
                    printError(err);
                }

            }
            if (command.inputFile != NULL) free(command.inputFile);
            if (command.outputFile != NULL) free(command.outputFile);
        }

        /* free */

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
