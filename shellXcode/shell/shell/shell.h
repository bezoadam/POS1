/**
 *  Program: Shell
 *  Author: Adam Bezak xbezak01
 */

#define _XOPEN_SOURCE 500
#define _XOPEN_SOURCE_EXTENDED 1
#define _POSIX_C_SOURCE 199506L
#ifndef _REENTRANT
#define _REENTRANT
#endif

/*#define CUSTOMDEBUG 1*/
#define PROMPT "$ "
#define BUFFER_MAX_LENGTH 512

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <wordexp.h>

enum {
    OK = 0,
    SIG_INT_ERR,
    SIG_CHLD_ERR,
    THREAD_ERR,
    COND_ERR,
    MUTEX_ERR,
    BUFFER_MAX_LENGTH_ERR,
    AMPERSAND_ERR,
    FORK_ERR,
    COMMAND_BG_ERR,
    COMMAND_ERR,
    INPUT_FILE_ERR,
    OUTPUT_FILE_ERR
};

const char *errors[] = {
    "OK",
    "SIG INT ERR!\n",
    "SIG CHLD ERR!\n",
    "Chyba pri vytvarani vlakna.\n",
    "Chyba pri inicializacii cond.\n",
    "Chyba pri inicializacii mutexu.\n",
    "Prilis dlhy vstup.\n",
    "Chyba syntaxe programu.\n",
    "Chyba pri forku.\n",
    "Chyba pri vykonavani prikazu na pozadi.\n",
    "Chyba pri vykonavani prikazu.",
    "Chyba pri praci so vstupnym suborom.\n",
    "Chyba pri praci s vystupnym suborom.\n",
};

const char *HELP =
"Spustenie programu: ./shell \n"
"- spustenie lubovolneho programu v lubovolnymi parametramy\n"
"- pri spusteni sa bere v uvahu premenna PATH (nastavena pred spustenim shellu)\n"
"- spustenie na pozadi pomocou &\n"
"- presmerovanie vstupu a vystupu pomocou < a >\n"
"Priklad pouzitia programu: \n"
"$ ls -l -a\n"
"$ sleep 10m &\n"
"$ echo karel >/tmp/test\n"
"\n"
;

struct Monitor {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

struct Buffer {
    char buffer[BUFFER_MAX_LENGTH + 1];
    int  length;
    bool isReading;
};

struct Command {
    int argc;
    char **argv;
    char *inputFile;
    char *outputFile;
    bool isBackground;
    int error;
};

/**
 Inicializacia handlerov

 @param *sigInt
 @param *sigChld
 @return int
 */
int initSigHandlers(struct sigaction *sigInt, struct sigaction *sigChld);

/**
 SigInt Handler

 @param sig
 @return void
*/
void sigIntHandler(int sig);

/**
 SigChld handler

 @param sig
 @param *pid
 @param *contxt
 @return void
*/
void sigChldHandler(int sig, siginfo_t *pid, void *contxt);

/**
 Inicializacia monitoru

 @param *monitor
 @param *thread
 @return int
 */
int initMonitor(struct Monitor *monitor, pthread_t *thread);

/**
 Ziskanie novych argumentov

 @param *args
 @param **argv
 @return int
*/
static int setargs(char *args, char **argv);

/**
 Parsovanie zadanych argumentov

 @param *args
 @param *argc
 @param *isBackground
 @return char**
*/
char **parsedargs(char *args, int *argc, bool *isBackground);

/**
 Ziskanie prikazu z bufferu

 @return struct Command
*/
struct Command getCommand();

/**
 Presmerovanie vystupu

 @param *outputFile
 @return int
*/
int redirectOutput(char *outputFile);

/**
 Presmerovanie vstupu

 @param *inputFile
 @return int
*/
int redirectInput(char *inputFile);

/**
 Spustenie prikazu na pozadi

 @param struct Command
 @return int
*/
int startBackgroundCommand(struct Command *command);

/**
 Spustenie prikazu

 @param struct Command
 @return int
*/
int startNormalCommand(struct Command *command);

/**
 Vlakno pre spustenie prikazu.

 @param *arg
 @return void
*/
void *runCommand(void *arg);

/**
 Citanie vstupu

 @param void
 @return int
*/
int readMyInput(void);

/**
 Citanie vstupu pomocu read()

 @param void
 @return int
*/
int readStdin(void);

/**
 Ukamknutie a vstup do kritickej sekcie

 @param isReading
 @return void
*/
void lockKS(bool isReading);

/**
 Uvolnenie kritickej sekcie

 @param withSignal
 @return vooid
*/
void unlockKS();

/**
 Deinit monitora, threads.

 @param struct Monitor
 @param *thread
 @return void
*/
void freeMonitor(struct Monitor *monitor, pthread_t *thread);

/**
 Vypis chybovej hlasky

 @param int err
 @return void
*/
void printError(int err);
