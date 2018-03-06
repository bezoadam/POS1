/*
    fork.c
    3 procesy + exit status

    Created by Adam Bezák on 3.3.18.
    xbezak01@stud.fit.vutbr.cz
*/


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1 /* XPG 4.2 - needed for WCOREDUMP() */

#define ERROR -1
#define SUCCESS 0

/* Deklarace funkcii */
void printProcessInfoBefore(char *label);
void waitAndPrintProcessInfo(char *label, pid_t pid);
void runProgram(char *argv[]);

/* ARGSUSED */
int main(int argc, char *argv[])
{
    pid_t parentId, childId;
    
    if (argc <= 1) {
        fprintf(stderr, "Chyba parametrov prikazoveho riadku.\n");
        return ERROR;
    }


    printProcessInfoBefore("grandparent");
    parentId = fork();
    if (parentId == 0) {
        printProcessInfoBefore("parent");

        childId = fork();
        if (childId == 0) {
            /* Child process */
            printProcessInfoBefore("child");
            runProgram(argv);
        } else if (childId > 0) {
            /* Parent process */
            waitAndPrintProcessInfo("child", childId);
        } else {
            fprintf(stderr, "Fork error.\n");
            return ERROR;
        }
    } else if (parentId > 0) {
        /* Grandparent process */
        waitAndPrintProcessInfo("parent", parentId);
    } else {
        fprintf(stderr, "Fork error.\n");
        return ERROR;
    }

    return 0;
}

void printProcessInfoBefore(char *label) {
    printf("%s identification: \n", label); /*grandparent/parent/child */
    printf("    pid = %d,    ppid = %d,    pgrp = %d\n", getpid(), getppid(), getpgrp());
    printf("    uid = %d,    gid = %d\n", getuid(), getgid());
    printf("    euid = %d,    egid = %d\n", geteuid(), getegid());
}

void waitAndPrintProcessInfo(char *label, pid_t pid) {
    int status;
    waitpid(pid, &status, 0);

    printf("%s exit (pid = %d):", label, pid); /* and one line from */

    if (WIFEXITED(status)) {
        printf("    normal termination (exit code = %d)\n", WEXITSTATUS(status)); /* or */
    } else if (WIFSIGNALED(status)) {
        #ifdef WCOREDUMP
        if (WCOREDUMP(status)) {
            printf("    signal termination with core dump (signal = %d)\n", WTERMSIG(status)); /* or */
        } else
        #endif
        printf("    signal termination (signal = %d)\n", WTERMSIG(status)); /* or */
    } else {
        printf("    unknown type of termination\n");
    }
}

void runProgram(char *argv[]) {
    execv(argv[1], argv + 1);
}
