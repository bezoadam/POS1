/* makra */
#define _XOPEN_SOURCE 500
#define _XOPEN_SOURCE_EXTENDED 1
#define _POSIX_C_SOURCE 199506L
#ifndef _REENTRANT
#define _REENTRANT
#endif

//#define CUSTOMDEBUG 1
#define PROMPT "$ "
#define BUFFER_MAX_LENGTH 512
#define PARSE_CHARS " \n\t"
#define MAZAT_DOKONCENE 1

/* hlavicky */
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
    ERR_MALLOC_ADD_LIST, //unused
    ERR_PARSE_AMPERSAND, //unused
    ERR_PARSE_SPECIAL_CHARS, //unused
    ERR_PARSE_FILE, //unused
    ERR_FORK, //unused
    ERR_CMD_BG, //unused
    ERR_INPUT_FILE, //unused
    ERR_OUTPUT_FILE, //unused
    ERR_MALLOC, //unused
    ERR_CMD //unused
};

const char *errors[] = {
    "OK",
    "SIG INT ERR\n",
    "SIG CHLD ERR\n",
    "Chyba pri vytvarani vlakna.\n",
    "Chyba pri inicializacii cond.\n",
    "Chyba pri inicializacii mutexu.\n",
    "Prilis dlhy vstup.\n",
    "Chyba alokovani dat pri pridavani prvku do seznamu\n",
    "Chyba syntaxe: za ampersandem nesmi nasledovat dalsi text bez mezery\n",
    "Chyba syntaxe: specialni znaky &,<,> musi byt v prikazu at na poslednich mistech\n",
    "Chyba syntaxe: vstupni/vystupni soubor byl spatne zadan\n",
    "Chyba forku\n",
    "Chyba prikazu na pozadi\n",
    "Chyba otevreni souboru input\n",
    "Chyba otevreni souboru output\n",
    "Chyba alokovani prostoru\n",
    "Chyba prikazu"
};

/* napoveda programu */
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

/* struktury */
struct Monitor {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

struct Buffer {
    char buffer[BUFFER_MAX_LENGTH + 1];    /* +1 pro konec retezce */
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

/* deklarace funkcii */
int initSigHandlers(struct sigaction *sigInt, struct sigaction *sigChld);

/*
 * Sig int handler
 *
 */
void sigIntHandler(int sig);

/*
 * Sig chld handler
 *
 */
void sigChldHandler(int sig, siginfo_t *pid, void *contxt);

/*
 * ==== inicializace monitoru
 * */
int initMonitor(struct Monitor *monitor, pthread_t *thread);

/*
 * ==== vlakno pro spousteni prikazu = shell
 * */
void *runCommand(void *arg);

/*
 * ==== cteni vstupu
 * */
int readMyInput(void);

/*
 * ==== cteni ze stdin pomoci fce read
 * */
int readStdin(void);

/*
 * ==== vstup do kriticke sekce
 * */
void lockKS(bool isReading);

/*
 * ==== uvolneni kriticke sekce
 * */
void unlockKS();

/*
 * ==== uvolneni pthreads
 * */
void freeMonitor(struct Monitor *monitor, pthread_t *thread);
void printError(int err);
