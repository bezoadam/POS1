/* makra */
#define _XOPEN_SOURCE 500
#define _XOPEN_SOURCE_EXTENDED 1
#define _POSIX_C_SOURCE 199506L
#ifndef _REENTRANT
#define _REENTRANT
#endif

#define DEBUG 1
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

enum {
    OK = 0,
    ERR_SET_SIG_INT,
    ERR_SET_SIG_CHLD,
    ERR_MUTEX,
    ERR_COND,
    ERR_THREAD,
    ERR_MALLOC_ADD_LIST,
    ERR_BUFFER_MAX_LENGTH,
    ERR_PARSE_AMPERSAND,
    ERR_PARSE_SPECIAL_CHARS,
    ERR_PARSE_FILE,
    ERR_FORK,
    ERR_CMD_BG,
    ERR_INPUT_FILE,
    ERR_OUTPUT_FILE,
    ERR_MALLOC,
    ERR_CMD
};

const char *errors[] = {
    "",
    "Chyba sigaction: sig int\n",
    "Chyba sigaction: sig chld\n",
    "Chyba inicializace mutexu\n",
    "Chyba inicializace cond\n",
    "Chyba pri vytvareni vlakna\n",
    "Chyba alokovani dat pri pridavani prvku do seznamu\n",
    "Chyba: maximalni delka vstupu byla prekrocena\n",
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
const char *HELP_MSG    = "Program: Shell\n"
"Autor: Adam Bezak | xbezak01\n\n"
"Pouziti: ./shell \n"
"    - spusteni libovolneho program s libovolnymi parametry\n"
"    - pri spusteni se bere v uvahu promenna PATH (nastavena pred spustenim shellu)\n"
"    - spusteni na pozadi pomoci &\n"
"    - presmerovani vstupu na vystup pomoci <a>\n"
"    - po spusteni shell vypise prompt a bude cekat na vstup pomoci read()\n"
"    - shell pouziva dve vlakna, prvni pro ziskani vstupu a druhe pro spousteni prikazu"
"Priklad pouziti: \n"
"    $ ls -l -a\n"
"    $ sleep 10m &\n"
"    $ echo karel >/tmp/test\n"
"\n"
;

/* struktury */
struct Monitor {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

struct Buffer {
    char read_buffer[BUFFER_MAX_LENGTH + 1];    /* +1 pro konec retezce */
    int  length;
    bool reading;

};
